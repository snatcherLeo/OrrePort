#include "recording.hpp"

#include "encoding.hpp"
#include "frame.hpp"
#include "lru_cache.hpp"
#include "resource_cache.hpp"

#include "clear.hpp"
#include "pipeline_cache.hpp"
#include "readback/readback.hpp"
#include "render_worker.hpp"
#include "tex_copy_conv.hpp"
#include "tex_palette_conv.hpp"
#include "texture.hpp"
#include "../gx/fifo.hpp"
#include "../gx/gx.hpp"
#include "dolphin/gx/GXAurora.h" // OrrePort, round 11: GX_AURORA_ORRE_2D_VERTICES
#include "../gx/pipeline.hpp"
#ifdef AURORA_ENABLE_RMLUI
#include "../rmlui/pipeline.hpp"
#endif
#include "../window.hpp"

#include <algorithm>
#include <array>
#include <cmath> // OrrePort: std::isfinite (the particle blend's corners)
#include <cstdio>  // OrrePort, fix (3)'s draw log
#include <cstdlib> // OrrePort, fix (3)'s draw log: std::getenv
#include <cstring>
#include <new>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <magic_enum.hpp>
#include <tracy/Tracy.hpp>

namespace aurora::gfx {
using namespace detail;
// fix (3)'s draw log: the last step's retrace stamp (aurora_smooth_step, via orre_draw_log_stamp). Kept here, not read
// from aurora.cpp, because the port's test programs link this library without aurora.cpp.
static uint32_t g_drawLogRetrace = 0;

namespace {
constexpr Module Log{"aurora::gfx"};

struct FrameRecorder {
  FramePacket* packet = nullptr;
  size_t frameSlot = 0;
  uint32_t currentRenderPass = UINT32_MAX;
  uint32_t drawCallCount = 0;
  uint32_t mergedDrawCallCount = 0;
  bool inOffscreen = false;
  std::optional<RenderPass> suspendedEfbPass;
  Viewport suspendedEfbViewport;
  ClipRect suspendedEfbScissor;
  webgpu::TextureWithSampler offscreenColor;
  webgpu::TextureWithSampler offscreenDepth;
  Viewport cachedViewport;
  ClipRect cachedScissor;
  bool suppressRenderWorker = false;
  bool normalRequested = false;
#ifdef AURORA_GFX_DEBUG_GROUPS
  std::vector<std::string> debugGroupStack;
#endif

  [[nodiscard]] bool active() const noexcept { return packet != nullptr; }

  FramePacket& frame() const {
    CHECK(packet != nullptr, "No active recording session");
    return *packet;
  }

  RenderPassList& passes() const { return frame().renderPasses; }
};

FrameRecorder g_recorder;

std::string pass_label(std::string_view kind) {
#ifdef AURORA_GFX_DEBUG_GROUPS
  if (!g_recorder.debugGroupStack.empty()) {
    return fmt::format("{} ({})", kind, g_recorder.debugGroupStack.back());
  }
#endif
  return std::string{kind};
}

void set_efb_targets(RenderPass& pass) {
  const auto layout = scene_render_target_layout();
  pass.colorAttachmentCount = layout.colorAttachmentCount;
  auto& sceneColor = pass.colorAttachments[SceneColorAttachmentIndex];
  sceneColor.semantic = ColorAttachmentSemantic::SceneColor;
  sceneColor.format = layout.colorAttachments[SceneColorAttachmentIndex].format;
  sceneColor.size = webgpu::g_frameBuffer.size;
  sceneColor.view = webgpu::g_frameBuffer.view;
  sceneColor.resolveView = layout.sampleCount > 1 ? webgpu::g_frameBufferResolved.view : nullptr;
  for (uint32_t i = SceneColorAttachmentIndex + 1; i < layout.colorAttachmentCount; ++i) {
    auto& color = pass.colorAttachments[i];
    color.semantic = layout.colorAttachments[i].semantic;
    color.format = layout.colorAttachments[i].format;
    if (color.semantic == ColorAttachmentSemantic::Normal) {
      color.size = webgpu::g_normalBuffer.size;
      color.view = webgpu::g_normalBuffer.view;
    } else {
      AURORA_ASSERT(false, "Scene render-target attachment {} has no backing texture", i);
    }
  }
  pass.depthStencilView = webgpu::g_depthBuffer.view;
  pass.depthStencilFormat = layout.depthStencilFormat;
  pass.copySourceTexture =
      webgpu::g_graphicsConfig.msaaSamples > 1 ? webgpu::g_frameBufferResolved.texture : webgpu::g_frameBuffer.texture;
  pass.copySourceView =
      webgpu::g_graphicsConfig.msaaSamples > 1 ? webgpu::g_frameBufferResolved.view : webgpu::g_frameBuffer.view;
  pass.copySourceDepthView = webgpu::g_depthBuffer.view;
  if (webgpu::g_graphicsConfig.normalBuffer) {
    pass.copySourceNormalTexture = webgpu::g_normalBuffer.texture;
  }
  pass.msaaSamples = layout.sampleCount;
  pass.hasDepth = true;
  pass.hasStencil = false;
}

void set_single_color_target(RenderPass& pass, wgpu::TextureFormat format, const wgpu::Extent3D size,
                             wgpu::TextureView view, wgpu::TextureView resolveView = {}) {
  auto& color = pass.colorAttachments[SceneColorAttachmentIndex];
  color.semantic = ColorAttachmentSemantic::SceneColor;
  color.format = format;
  color.size = size;
  color.view = std::move(view);
  color.resolveView = std::move(resolveView);
  pass.colorAttachmentCount = 1;
}

struct OffscreenCacheKey {
  uint32_t width;
  uint32_t height;
  wgpu::TextureFormat colorFormat;
  wgpu::TextureFormat depthFormat;

  bool operator==(const OffscreenCacheKey&) const = default;
  template <typename H>
  friend H AbslHashValue(H h, const OffscreenCacheKey& key) {
    return H::combine(std::move(h), key.width, key.height, key.colorFormat, key.depthFormat);
  }
};
struct OffscreenCacheEntry {
  webgpu::TextureWithSampler color;
  webgpu::TextureWithSampler depth;

  uint64_t size_bytes() const {
    return calc_texture_size(color.format, color.size.width, color.size.height, 1) +
           calc_texture_size(depth.format, depth.size.width, depth.size.height, 1);
  }
};
LruCache<OffscreenCacheKey, OffscreenCacheEntry> g_offscreenCache{RenderTextureCacheLimits};

// Pooled destinations for the public resolve_pass API. Entries are recycled
// per frame slot: a slot is only re-acquired after the render worker has
// submitted its previous frame, and queue serialization orders the new frame's
// copies after the old frame's reads.
struct PassSnapshotEntry {
  webgpu::TextureWithSampler color;
  webgpu::TextureWithSampler depth; // R32Float raw depth
  webgpu::TextureWithSampler normal;
};
struct PassSnapshotPool {
  std::vector<PassSnapshotEntry> entries;
  size_t used = 0;
};
std::array<PassSnapshotPool, FrameSlotCount> g_passSnapshotPools;

PassSnapshotEntry& acquire_pass_snapshot(uint32_t width, uint32_t height, bool wantColor, bool wantDepth,
                                         bool wantNormal) {
  auto& pool = g_passSnapshotPools[g_recorder.frameSlot];
  if (pool.used == pool.entries.size()) {
    pool.entries.emplace_back();
  }
  auto& entry = pool.entries[pool.used++];
  const wgpu::Extent3D size{width, height, 1};
  if (wantColor && (!entry.color.texture || entry.color.size.width != width || entry.color.size.height != height ||
                    entry.color.format != webgpu::g_graphicsConfig.surfaceConfiguration.format)) {
    const auto format = webgpu::g_graphicsConfig.surfaceConfiguration.format;
    const wgpu::TextureDescriptor desc{
        .label = "Pass Snapshot Color",
        .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc,
        .dimension = wgpu::TextureDimension::e2D,
        .size = size,
        .format = format,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    auto texture = webgpu::g_device.CreateTexture(&desc);
    auto view = texture.CreateView();
    entry.color = webgpu::TextureWithSampler{
        .texture = std::move(texture),
        .view = std::move(view),
        .size = size,
        .format = format,
    };
  }
  if (wantDepth && (!entry.depth.texture || entry.depth.size.width != width || entry.depth.size.height != height)) {
    const wgpu::TextureDescriptor desc{
        .label = "Pass Snapshot Depth",
        .usage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc,
        .dimension = wgpu::TextureDimension::e2D,
        .size = size,
        .format = wgpu::TextureFormat::R32Float,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    auto texture = webgpu::g_device.CreateTexture(&desc);
    auto view = texture.CreateView();
    entry.depth = webgpu::TextureWithSampler{
        .texture = std::move(texture),
        .view = std::move(view),
        .size = size,
        .format = wgpu::TextureFormat::R32Float,
    };
  }
  if (wantNormal && (!entry.normal.texture || entry.normal.size.width != width || entry.normal.size.height != height)) {
    const wgpu::TextureDescriptor desc{
        .label = "Pass Snapshot Normal",
        .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc,
        .dimension = wgpu::TextureDimension::e2D,
        .size = size,
        .format = webgpu::NormalBufferFormat,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    auto texture = webgpu::g_device.CreateTexture(&desc);
    auto view = texture.CreateView();
    entry.normal = webgpu::TextureWithSampler{
        .texture = std::move(texture),
        .view = std::move(view),
        .size = size,
        .format = webgpu::NormalBufferFormat,
    };
  }
  return entry;
}

FramePacket& current_frame_packet() { return g_recorder.frame(); }

RenderPassList& current_render_passes() { return g_recorder.passes(); }

StagingHighWater current_high_water(const FramePacket& frame) noexcept {
  return {
      .verts = static_cast<uint32_t>(frame.verts.size()),
      .uniforms = static_cast<uint32_t>(frame.uniforms.size()),
      .indices = static_cast<uint32_t>(frame.indices.size()),
      .storage = static_cast<uint32_t>(frame.storage.size()),
      .textureUpload = static_cast<uint32_t>(frame.textureUpload.size()),
      .textureUploadCount = frame.textureUploads.size(),
  };
}

FrameOp capture_frame_op(FramePacket& frame, FrameOpType type, uint32_t index) {
  FrameOp op{
      .type = type,
      .index = index,
      .renderPass =
          type == FrameOpType::RenderPass && index < frame.renderPasses.size() ? &frame.renderPasses[index] : nullptr,
      .textureCopy = type == FrameOpType::TextureCopy && index < frame.textureCopies.size()
                         ? &frame.textureCopies[index]
                         : nullptr,
      .encoderTask =
          type == FrameOpType::EncoderTask && index < frame.encoderTasks.size() ? &frame.encoderTasks[index] : nullptr,
      .readbackTask =
          type == FrameOpType::Readback && index < frame.readbackTasks.size() ? &frame.readbackTasks[index] : nullptr,
      .highWater = current_high_water(frame),
  };
  op.textureUploads.reserve(op.highWater.textureUploadCount);
  for (size_t i = 0; i < op.highWater.textureUploadCount; ++i) {
    op.textureUploads.push_back(&frame.textureUploads[i]);
  }
  return op;
}

void seal_pass(FramePacket& frame, uint32_t passIndex) {
  if (passIndex >= frame.renderPasses.size()) {
    return;
  }
  auto& pass = frame.renderPasses[passIndex];
  if (pass.sealed) {
    return;
  }
  pass.sealed = true;
}

Range push(ByteBuffer& target, const uint8_t* data, size_t length, size_t alignment) {
  if (alignment != 0) {
    const size_t begin = target.size();
    const size_t alignedBegin = AURORA_ALIGN(begin, alignment);
    if (alignedBegin > begin) {
      target.append_zeroes(alignedBegin - begin);
    }
  }
  const auto begin = target.size();
  if (length > 0) {
    target.append(data, length);
  }
  return {static_cast<uint32_t>(begin), static_cast<uint32_t>(length)};
}

Range map(ByteBuffer& target, size_t length, size_t alignment) {
  if (alignment != 0) {
    const size_t begin = target.size();
    const size_t alignedBegin = AURORA_ALIGN(begin, alignment);
    if (alignedBegin > begin) {
      target.append_zeroes(alignedBegin - begin);
    }
  }
  auto begin = target.size();
  if (length > 0) {
    target.append_zeroes(length);
  }
  return {static_cast<uint32_t>(begin), static_cast<uint32_t>(length)};
}

// For our public API, warn instead of fatal-ing when called outside an active recording frame.
bool check_recording(const char* name) {
  if (!g_recorder.active())
    UNLIKELY {
      Log.warn("{}: called outside an active frame", name);
      return false;
    }
  return true;
}

void push_command(CommandType type, const Command::Data& data) {
  if (g_recorder.currentRenderPass == UINT32_MAX)
    UNLIKELY {
      Log.warn("Dropping command {}", magic_enum::enum_name(type));
      return;
    }
  auto& renderPass = current_render_passes()[g_recorder.currentRenderPass];
  AURORA_ASSERT(!renderPass.sealed, "Attempted to append command {} to sealed render pass {}",
                magic_enum::enum_name(type), g_recorder.currentRenderPass);
  if (type == CommandType::Draw || type == CommandType::CustomDraw) {
    renderPass.hasDraws = true;
  }
  renderPass.commands.push_back({
      .type = type,
#ifdef AURORA_GFX_DEBUG_GROUPS
      .debugGroupStack = g_recorder.debugGroupStack,
#endif
      .data = data,
  });
}

template <class T>
concept InlinePayload =
    std::is_trivially_copyable_v<T> && std::is_aggregate_v<T> && std::is_trivially_destructible_v<T>;

template <InlinePayload T>
T& inline_payload(void* ptr) noexcept {
  // C++20 equivalent of C++23's std::start_lifetime_as
  return *std::launder(static_cast<T*>(std::memmove(ptr, ptr, sizeof(T))));
}

template <auto Renderer, InlinePayload T>
void encode_draw(void* payload, const wgpu::RenderPassEncoder& pass, const RenderPass& passInfo) {
  const T& data = inline_payload<T>(payload);
  if constexpr (std::is_invocable_v<decltype(Renderer), const T&, const wgpu::RenderPassEncoder&,
                                    const wgpu::Extent3D&>) {
    Renderer(data, pass, passInfo.colorAttachments[SceneColorAttachmentIndex].size);
  } else {
    static_assert(std::is_invocable_v<decltype(Renderer), const T&, const wgpu::RenderPassEncoder&>);
    Renderer(data, pass);
  }
}

template <auto Renderer, InlinePayload T>
DrawCommand make_draw_command(const T& data) {
  static_assert(sizeof(T) <= InlineDrawPayloadSize);
  static_assert(alignof(T) <= alignof(std::max_align_t));
  DrawCommand command{.encoder = encode_draw<Renderer, T>};
  std::memcpy(command.payload.data(), &data, sizeof(data));
  return command;
}

void push_draw_command(DrawCommand data) {
  push_command(CommandType::Draw, Command::Data{.draw = data});
  ++g_recorder.drawCallCount;
}

OffscreenCacheEntry get_offscreen_textures(uint32_t width, uint32_t height) {
  const auto colorFormat = webgpu::g_graphicsConfig.surfaceConfiguration.format;
  const auto depthFormat = webgpu::g_graphicsConfig.depthFormat;
  const OffscreenCacheKey key{width, height, colorFormat, depthFormat};
  if (const auto* entry = g_offscreenCache.find(key)) {
    return *entry;
  }
  const wgpu::Extent3D size{width, height, 1};
  const wgpu::TextureDescriptor colorDesc{
      .label = "Offscreen Color",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
               wgpu::TextureUsage::CopyDst,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = colorFormat,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };
  auto colorTexture = webgpu::g_device.CreateTexture(&colorDesc);
  auto colorView = colorTexture.CreateView();
  webgpu::TextureWithSampler color{
      .texture = std::move(colorTexture),
      .view = std::move(colorView),
      .size = size,
      .format = colorFormat,
  };
  const wgpu::TextureDescriptor depthDesc{
      .label = "Offscreen Depth",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = depthFormat,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };
  auto depthTexture = webgpu::g_device.CreateTexture(&depthDesc);
  auto depthView = depthTexture.CreateView();
  webgpu::TextureWithSampler depth{
      .texture = std::move(depthTexture),
      .view = std::move(depthView),
      .size = size,
      .format = depthFormat,
  };
  OffscreenCacheEntry entry{
      .color = std::move(color),
      .depth = std::move(depth),
  };
  g_offscreenCache.insert(key, entry, entry.size_bytes());
  return entry;
}

void enqueue_pass(FramePacket& frame, uint32_t passIndex);

void resume_efb_pass_loading(const RenderPass& prevPass) {
  RenderPass newPass{
      .label = pass_label("EFB"),
      .colorAttachments = prevPass.colorAttachments,
      .colorAttachmentCount = prevPass.colorAttachmentCount,
      .depthStencilView = prevPass.depthStencilView,
      .depthStencilFormat = prevPass.depthStencilFormat,
      .copySourceTexture = prevPass.copySourceTexture,
      .copySourceView = prevPass.copySourceView,
      .copySourceDepthView = prevPass.copySourceDepthView,
      .copySourceNormalTexture = prevPass.copySourceNormalTexture,
      .msaaSamples = prevPass.msaaSamples,
      .clearDepth = false,
      .hasDepth = prevPass.hasDepth,
      .hasStencil = prevPass.hasStencil,
  };
  for (uint32_t i = 0; i < newPass.colorAttachmentCount; ++i) {
    newPass.colorAttachments[i].loadOp = wgpu::LoadOp::Undefined;
    newPass.colorAttachments[i].clear = false;
  }
  newPass.commands.reserve(2048);
  current_render_passes().emplace_back(std::move(newPass));
  ++g_recorder.currentRenderPass;
  push_command(CommandType::SetViewport, Command::Data{.setViewport = g_recorder.cachedViewport});
  push_command(CommandType::SetScissor, Command::Data{.setScissor = g_recorder.cachedScissor});
}

void suspend_efb() {
  AURORA_ASSERT(g_recorder.active() && g_recorder.currentRenderPass != UINT32_MAX,
                "suspend_efb called outside of an active recording frame");
  AURORA_ASSERT(!g_recorder.inOffscreen, "suspend_efb called while offscreen rendering is active");
  AURORA_ASSERT(!g_recorder.suspendedEfbPass, "suspend_efb called with an EFB pass already suspended");

  auto& currentPass = current_render_passes()[g_recorder.currentRenderPass];
  if (!currentPass.has_consumer()) {
    g_recorder.suspendedEfbPass = std::move(currentPass);
    current_render_passes().pop_back();
    --g_recorder.currentRenderPass;
  } else {
    enqueue_pass(current_frame_packet(), g_recorder.currentRenderPass);
  }
  g_recorder.suspendedEfbViewport = g_recorder.cachedViewport;
  g_recorder.suspendedEfbScissor = g_recorder.cachedScissor;
}

void finish_current_offscreen() {
  AURORA_ASSERT(g_recorder.active() && g_recorder.currentRenderPass != UINT32_MAX,
                "finish_current_offscreen called outside of an active recording frame");
  AURORA_ASSERT(g_recorder.inOffscreen, "finish_current_offscreen called without an active offscreen pass");

  auto& offscreenPass = current_render_passes()[g_recorder.currentRenderPass];
  offscreenPass.discardable = !offscreenPass.has_consumer();
  enqueue_pass(current_frame_packet(), g_recorder.currentRenderPass);
  g_recorder.offscreenColor = {};
  g_recorder.offscreenDepth = {};
}

void start_offscreen(uint32_t width, uint32_t height) {
  AURORA_ASSERT(width != 0 && height != 0, "start_offscreen requires nonzero dimensions ({}x{})", width, height);
  AURORA_ASSERT(g_recorder.active(), "start_offscreen called outside of an active recording frame");

  auto offscreenEntry = get_offscreen_textures(width, height);
  g_recorder.offscreenColor = std::move(offscreenEntry.color);
  g_recorder.offscreenDepth = std::move(offscreenEntry.depth);

  RenderPass newPass{
      .label = pass_label("Offscreen"),
      .depthStencilView = g_recorder.offscreenDepth.view,
      .depthStencilFormat = g_recorder.offscreenDepth.format,
      .copySourceTexture = g_recorder.offscreenColor.texture,
      .copySourceView = g_recorder.offscreenColor.view,
      .copySourceDepthView = g_recorder.offscreenDepth.view,
      .msaaSamples = 1,
      .clearDepthValue = gx::UseReversedZ ? 0.f : 1.f,
      .clearDepth = true,
      .hasDepth = true,
      .hasStencil = false,
  };
  set_single_color_target(newPass, g_recorder.offscreenColor.format, {width, height}, g_recorder.offscreenColor.view);
  current_render_passes().emplace_back(std::move(newPass));
  ++g_recorder.currentRenderPass;
  g_recorder.inOffscreen = true;

  g_recorder.cachedViewport = {0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f};
  g_recorder.cachedScissor = {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)};
  push_command(CommandType::SetViewport, Command::Data{.setViewport = g_recorder.cachedViewport});
  push_command(CommandType::SetScissor, Command::Data{.setScissor = g_recorder.cachedScissor});
}

void restore_efb() {
  AURORA_ASSERT(g_recorder.active(), "restore_efb called outside of an active recording frame");
  AURORA_ASSERT(g_recorder.inOffscreen, "restore_efb called without a suspended EFB pass");

  g_recorder.inOffscreen = false;
  if (g_recorder.suspendedEfbPass) {
    current_render_passes().emplace_back(std::move(*g_recorder.suspendedEfbPass));
    g_recorder.suspendedEfbPass.reset();
  } else {
    auto& pass = current_render_passes().emplace_back();
    pass.label = pass_label("EFB");
    set_efb_targets(pass);
    for (uint32_t i = 0; i < pass.colorAttachmentCount; ++i) {
      pass.colorAttachments[i].clear = false;
      pass.colorAttachments[i].loadOp = wgpu::LoadOp::Undefined;
    }
    pass.clearDepth = false;
  }
  ++g_recorder.currentRenderPass;
  set_efb_targets(current_render_passes()[g_recorder.currentRenderPass]);

  g_recorder.cachedViewport = g_recorder.suspendedEfbViewport;
  g_recorder.cachedScissor = g_recorder.suspendedEfbScissor;
  push_command(CommandType::SetViewport, Command::Data{.setViewport = g_recorder.cachedViewport});
  push_command(CommandType::SetScissor, Command::Data{.setScissor = g_recorder.cachedScissor});
}

void enqueue_op(FramePacket& frame, uint32_t opIndex) {
  if (opIndex >= frame.ops.size() || g_recorder.suppressRenderWorker) {
    return;
  }
  auto op = frame.ops[opIndex];
  render_worker::enqueue_encode_pass(frame.frameId, opIndex, [packet = &frame, op = std::move(op)] {
    if (op.renderPass == nullptr && op.textureCopy == nullptr && op.encoderTask == nullptr &&
        op.readbackTask == nullptr) {
      return;
    }
    encode_op(packet->encoder, *packet, op);
  });
}

void enqueue_pass(FramePacket& frame, uint32_t passIndex) {
  seal_pass(frame, passIndex);
  const auto opIndex = static_cast<uint32_t>(frame.ops.size());
  frame.ops.emplace_back(capture_frame_op(frame, FrameOpType::RenderPass, passIndex));
  enqueue_op(frame, opIndex);
}
} // namespace

namespace detail {

// OrrePort: on the GameCube the EFB keeps its picture until a copy with clear empties it (the display copy usually
// clears); Aurora clears it at the start of every frame, which is the same thing only when the frame began at a
// display copy. A frame the port begins without one (a room load ends frames at the retrace) keeps the EFB instead:
// Willie's battle-start swirl captured a cleared EFB there and showed black frames (A, 2026-10-18). Game thread only.
static bool g_keepEfbNextFrame = false;
static uint32_t g_seenFrameBufferGeneration = UINT32_MAX; // the EFB targets the last frame drew into
void keep_efb_next_frame() noexcept { g_keepEfbNextFrame = true; }

void begin_recording(FramePacket& packet, size_t frameSlot) {
  CHECK(!g_recorder.active(), "A recording session is already active");
  if (g_recorder.normalRequested && webgpu::enable_normal_buffer()) {
    g_recorder.normalRequested = false;
  }
  g_recorder.packet = &packet;
  g_recorder.frameSlot = frameSlot;
  g_passSnapshotPools[frameSlot].used = 0;
  g_recorder.drawCallCount = 0;
  g_recorder.mergedDrawCallCount = 0;
  g_recorder.suspendedEfbPass.reset();

  current_render_passes().emplace_back();
  auto& pass = current_render_passes()[0];
  pass.label = pass_label("EFB");
  set_efb_targets(pass);
  pass.colorAttachments[SceneColorAttachmentIndex].clearValue = gx::g_gxState.clearColor;
  pass.clearDepthValue = gx::clear_depth_value();
  // keep only an EFB a previous frame drew into: after a re-creation (a resize, the 4:3 policy at start-up) the new
  // targets hold nothing of the game's, so that frame clears as usual
  const bool sameTargets = g_seenFrameBufferGeneration == webgpu::g_frameBufferGeneration;
  g_seenFrameBufferGeneration = webgpu::g_frameBufferGeneration;
  const bool keep = g_keepEfbNextFrame && sameTargets;
  g_keepEfbNextFrame = false;
  // frame smoothing (frame_packet.hpp): what an in-between picture needs to know about how this step began
  packet.orreKeptEfb = keep;
  packet.orreFrameBufferGeneration = webgpu::g_frameBufferGeneration;
  if (keep) {
    for (uint32_t i = 0; i < pass.colorAttachmentCount; ++i) {
      pass.colorAttachments[i].clear = false; // with loadOp Undefined: LoadOp::Load (encoding.cpp)
    }
    pass.clearDepth = false;
  }
  g_recorder.currentRenderPass = 0;
  g_recorder.cachedViewport = gx::map_logical_viewport(gx::g_gxState.logicalViewport);
  g_recorder.cachedScissor = gx::map_logical_scissor(gx::g_gxState.logicalScissor);
  push_command(CommandType::SetViewport, Command::Data{.setViewport = g_recorder.cachedViewport});
  push_command(CommandType::SetScissor, Command::Data{.setScissor = g_recorder.cachedScissor});
}

RecordedFrame end_recording() {
  CHECK(g_recorder.active(), "No active recording session");
  AURORA_ASSERT(!g_recorder.inOffscreen, "end_frame called while offscreen rendering is active");
  AURORA_ASSERT(g_recorder.currentRenderPass == UINT32_MAX,
                "end_frame called before finish finalized the current render pass");
  auto& frame = g_recorder.frame();
  frame.stats.drawCallCount = g_recorder.drawCallCount;
  frame.stats.mergedDrawCallCount = g_recorder.mergedDrawCallCount;
  frame.stats.lastVertSize = frame.verts.size();
  frame.stats.lastUniformSize = frame.uniforms.size();
  frame.stats.lastIndexSize = frame.indices.size();
  frame.stats.lastStorageSize = frame.storage.size();
  frame.stats.lastTextureUploadSize = frame.textureUpload.size();

  for (auto& array : gx::g_gxState.arrays) {
    array.cachedRange = {};
  }
#if defined(AURORA_GFX_DEBUG_GROUPS)
  if (!g_recorder.debugGroupStack.empty()) {
    for (auto& item : std::ranges::reverse_view(g_recorder.debugGroupStack)) {
      Log.warn("Debug group was not popped at end of frame: {}", item);
    }
    g_recorder.debugGroupStack.clear();
  }
#endif
  const RecordedFrame recorded{.packet = g_recorder.packet, .frameSlot = g_recorder.frameSlot};
  g_recorder.packet = nullptr;
  g_recorder.frameSlot = 0;
  g_offscreenCache.end_frame();
  TracyPlot("aurora: offscreenCacheBytes", static_cast<int64_t>(g_offscreenCache.cached_bytes()));
  TracyPlot("aurora: offscreenCacheEntries", static_cast<int64_t>(g_offscreenCache.cached_count()));
  return recorded;
}

void shutdown_recording() {
  for (auto& pool : g_passSnapshotPools) {
    pool = {};
  }
  g_recorder.currentRenderPass = UINT32_MAX;
  g_offscreenCache.clear();
  g_recorder.offscreenColor = {};
  g_recorder.offscreenDepth = {};
  g_recorder.suspendedEfbPass.reset();
  g_recorder.inOffscreen = false;
  g_recorder.packet = nullptr;
  g_recorder.frameSlot = 0;
  g_recorder.suppressRenderWorker = false;
  g_recorder.normalRequested = false;
}

namespace testing {

void suppress_render_worker(bool suppress) noexcept { g_recorder.suppressRenderWorker = suppress; }

void seed_offscreen_cache(uint32_t width, uint32_t height, wgpu::TextureFormat colorFormat,
                          wgpu::TextureFormat depthFormat) {
  const wgpu::Extent3D size{width, height, 1};
  const OffscreenCacheEntry entry{
      .color = {.size = size, .format = colorFormat},
      .depth = {.size = size, .format = depthFormat},
  };
  g_offscreenCache.insert({width, height, colorFormat, depthFormat}, entry, entry.size_bytes());
}

OffscreenCacheStats offscreen_cache_stats() noexcept {
  return {.bytes = g_offscreenCache.cached_bytes(), .entries = g_offscreenCache.cached_count()};
}

bool is_offscreen_cached(uint32_t width, uint32_t height, wgpu::TextureFormat colorFormat,
                         wgpu::TextureFormat depthFormat) {
  return g_offscreenCache.contains({width, height, colorFormat, depthFormat});
}

} // namespace testing

void increment_merged_draw_count() noexcept {
  if (g_recorder.active()) {
    ++g_recorder.mergedDrawCallCount;
  }
}

} // namespace detail

void queue_texture_upload(TextureUpload upload) {
  if (g_recorder.currentRenderPass != UINT32_MAX) {
    AURORA_ASSERT(!current_render_passes()[g_recorder.currentRenderPass].sealed,
                  "Attempted to append texture upload to sealed render pass {}", g_recorder.currentRenderPass);
  }
  current_frame_packet().textureUploads.emplace_back(std::move(upload));
}

void queue_texture_upload_data(const uint8_t* data, uint32_t bytesPerRow, uint32_t rowsPerImage,
                               wgpu::TexelCopyTextureInfo tex, wgpu::Extent3D size) {
  const auto copyBytesPerRow = AURORA_ALIGN(bytesPerRow, 256);
  auto& frame = current_frame_packet();
  if (frame.textureUpload.size() + copyBytesPerRow * rowsPerImage <= TextureUploadSize) {
    const auto range = push_texture_data(data, bytesPerRow, rowsPerImage);
    const wgpu::TexelCopyBufferLayout layout{
        .offset = range.offset,
        .bytesPerRow = bytesPerRow,
        .rowsPerImage = rowsPerImage,
    };
    queue_texture_upload(TextureUpload{layout, std::move(tex), size});
    return;
  }

  const uint64_t uploadSize = copyBytesPerRow * rowsPerImage;
  const wgpu::BufferDescriptor descriptor{
      .label = "Overflow Texture Upload Buffer",
      .usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc,
      .size = uploadSize,
      .mappedAtCreation = true,
  };
  auto buffer = webgpu::g_device.CreateBuffer(&descriptor);
  auto* dst = static_cast<uint8_t*>(buffer.GetMappedRange(0, uploadSize));
  for (uint32_t row = 0; row < rowsPerImage; ++row) {
    memcpy(dst, data, bytesPerRow);
    data += bytesPerRow;
    dst += copyBytesPerRow;
  }
  buffer.Unmap();

  const wgpu::TexelCopyBufferLayout layout{
      .offset = 0,
      .bytesPerRow = bytesPerRow,
      .rowsPerImage = rowsPerImage,
  };
  queue_texture_upload(TextureUpload{layout, std::move(tex), size, std::move(buffer)});
}

void queue_texture_copy(wgpu::TexelCopyTextureInfo src, wgpu::TexelCopyTextureInfo dst, wgpu::Extent3D size) {
  ZoneScoped;
  auto& frame = current_frame_packet();
  if (g_recorder.currentRenderPass != UINT32_MAX) {
    enqueue_pass(frame, g_recorder.currentRenderPass);
    g_recorder.currentRenderPass = UINT32_MAX;
  }

  const auto copyIndex = static_cast<uint32_t>(frame.textureCopies.size());
  frame.textureCopies.emplace_back(TextureCopy{
      .src = std::move(src),
      .dst = std::move(dst),
      .size = size,
  });
  const auto opIndex = static_cast<uint32_t>(frame.ops.size());
  frame.ops.emplace_back(capture_frame_op(frame, FrameOpType::TextureCopy, copyIndex));
  enqueue_op(frame, opIndex);
}

void begin_color_pass(const ColorPassDescriptor& desc) {
  ZoneScoped;
  auto& frame = current_frame_packet();
  if (g_recorder.currentRenderPass != UINT32_MAX) {
    enqueue_pass(frame, g_recorder.currentRenderPass);
  }

  RenderPass pass{
      .label = desc.label != nullptr ? desc.label : "",
      .depthStencilView = desc.depthStencilView,
      .depthStencilFormat = desc.depthStencilFormat,
      .msaaSamples = desc.sampleCount,
      .clearDepthValue = desc.depthClearValue,
      .depthLoadOp = desc.depthLoadOp,
      .depthStoreOp = desc.depthStoreOp,
      .stencilLoadOp = desc.stencilLoadOp,
      .stencilStoreOp = desc.stencilStoreOp,
      .stencilClearValue = desc.stencilClearValue,
      .clearDepth = desc.depthLoadOp == wgpu::LoadOp::Clear,
      .hasDepth = desc.hasDepth,
      .hasStencil = desc.hasStencil,
  };
  set_single_color_target(pass, desc.colorFormat, desc.targetSize, desc.colorView, desc.resolveView);
  auto& color = pass.colorAttachments[SceneColorAttachmentIndex];
  color.clearValue = {
      static_cast<float>(desc.clearColor.r),
      static_cast<float>(desc.clearColor.g),
      static_cast<float>(desc.clearColor.b),
      static_cast<float>(desc.clearColor.a),
  };
  color.loadOp = desc.colorLoadOp;
  color.storeOp = desc.colorStoreOp;
  color.clear = desc.colorLoadOp == wgpu::LoadOp::Clear;
  pass.commands.reserve(128);
  frame.renderPasses.emplace_back(std::move(pass));
  g_recorder.currentRenderPass = static_cast<uint32_t>(frame.renderPasses.size() - 1);

  g_recorder.cachedViewport = {
      0.f, 0.f, static_cast<float>(desc.targetSize.width), static_cast<float>(desc.targetSize.height), 0.f, 1.f};
  g_recorder.cachedScissor = {0, 0, static_cast<int32_t>(desc.targetSize.width),
                              static_cast<int32_t>(desc.targetSize.height)};
  push_command(CommandType::SetViewport, Command::Data{.setViewport = g_recorder.cachedViewport});
  push_command(CommandType::SetScissor, Command::Data{.setScissor = g_recorder.cachedScissor});
}

void end_color_pass() {
  ZoneScoped;
  if (g_recorder.currentRenderPass == UINT32_MAX) {
    return;
  }
  enqueue_pass(current_frame_packet(), g_recorder.currentRenderPass);
  g_recorder.currentRenderPass = UINT32_MAX;
}

template <>
gx::DrawData* get_last_draw_command() {
  if (g_recorder.currentRenderPass >= current_render_passes().size()) {
    return nullptr;
  }
  auto& last = current_render_passes()[g_recorder.currentRenderPass].commands.back();
  if (last.type != CommandType::Draw || last.data.draw.encoder != encode_draw<gx::render, gx::DrawData>) {
    return nullptr;
  }
  return &inline_payload<gx::DrawData>(last.data.draw.payload.data());
}

Vec2<uint32_t> get_render_target_size() noexcept {
  if (g_recorder.currentRenderPass < current_render_passes().size()) {
    const auto& size =
        current_render_passes()[g_recorder.currentRenderPass].colorAttachments[SceneColorAttachmentIndex].size;
    return {size.width, size.height};
  }
  const auto windowSize = window::get_window_size();
  return {windowSize.fb_width, windowSize.fb_height};
}

void set_viewport(const Viewport& cmd) noexcept {
  if (cmd != g_recorder.cachedViewport) {
    push_command(CommandType::SetViewport, Command::Data{.setViewport = cmd});
    g_recorder.cachedViewport = cmd;
  }
}

void set_scissor(const ClipRect& cmd) noexcept {
  if (cmd != g_recorder.cachedScissor) {
    push_command(CommandType::SetScissor, Command::Data{.setScissor = cmd});
    g_recorder.cachedScissor = cmd;
  }
}

template <>
void push_draw_command(clear::DrawData data) {
  push_draw_command(make_draw_command<clear::render>(data));
}

template <>
PipelineRef pipeline_ref(const clear::PipelineConfig& config) {
  return find_pipeline(config, get_render_target_layout(), gx::g_gxState.drawsBlocking);
}

void resolve_pass_into(TextureHandle texture, ClipRect rect, bool clearColor, bool clearAlpha, bool clearDepth,
                       Vec4<float> clearColorValue, float clearDepthValue, GXTexFmt resolveFormat,
                       GXPixelFmt sourceFormat) {
  // OrrePort, fix (3)'s draw log (test switch, 2026-11-18): an EFB copy among the draws (which pass it ends, its
  // target, size and clears), to tell a capture of this step's own picture from one of an earlier step's
  if (orre_draw_log_on()) {
    char line[200];
    std::snprintf(line, sizeof(line), "EFB copy: ends pass %u into texture %p, %ux%u at (%d %d), clear colour %d depth %d",
                  g_recorder.currentRenderPass, static_cast<const void*>(texture.get()),
                  static_cast<unsigned>(rect.width), static_cast<unsigned>(rect.height), static_cast<int>(rect.x),
                  static_cast<int>(rect.y), clearColor ? 1 : 0, clearDepth ? 1 : 0);
    orre_draw_log(line);
  }
  // Resolve current render pass
  auto& prevPass = current_render_passes()[g_recorder.currentRenderPass];
  prevPass.resolveTarget = std::move(texture);
  prevPass.resolveRect = rect;
  prevPass.resolveFormat = resolveFormat;
  prevPass.resolveSourceFormat = sourceFormat;
  const auto srcW = static_cast<float>(prevPass.colorAttachments[SceneColorAttachmentIndex].size.width);
  const auto srcH = static_cast<float>(prevPass.colorAttachments[SceneColorAttachmentIndex].size.height);
  const tex_copy_conv::Uniforms uniforms{
      .offset = {static_cast<float>(rect.x) / srcW, static_cast<float>(rect.y) / srcH},
      .scale = {static_cast<float>(rect.width) / srcW, static_cast<float>(rect.height) / srcH},
      .opaqueAlpha = !gx::efb_has_alpha(sourceFormat),
  };
  prevPass.resolveUniformRange = push_uniform(uniforms);
  enqueue_pass(current_frame_packet(), g_recorder.currentRenderPass);

  // Populate new render pass from previous
  const auto msaaSamples = prevPass.msaaSamples;
  const auto& targetSize = prevPass.colorAttachments[SceneColorAttachmentIndex].size;
  const auto left = std::clamp<int32_t>(rect.x, 0, static_cast<int32_t>(targetSize.width));
  const auto top = std::clamp<int32_t>(rect.y, 0, static_cast<int32_t>(targetSize.height));
  const auto right = std::clamp<int32_t>(rect.x + rect.width, left, static_cast<int32_t>(targetSize.width));
  const auto bottom = std::clamp<int32_t>(rect.y + rect.height, top, static_cast<int32_t>(targetSize.height));
  const ClipRect clearRect{.x = left, .y = top, .width = right - left, .height = bottom - top};
  const bool fullTarget = left == 0 && top == 0 && right == targetSize.width && bottom == targetSize.height;
  RenderPass newPass{
      .label = pass_label(g_recorder.inOffscreen ? "Offscreen" : "EFB"),
      .colorAttachments = prevPass.colorAttachments,
      .colorAttachmentCount = prevPass.colorAttachmentCount,
      .depthStencilView = prevPass.depthStencilView,
      .depthStencilFormat = prevPass.depthStencilFormat,
      .copySourceTexture = prevPass.copySourceTexture,
      .copySourceView = prevPass.copySourceView,
      .copySourceDepthView = prevPass.copySourceDepthView,
      .copySourceNormalTexture = prevPass.copySourceNormalTexture,
      .msaaSamples = msaaSamples,
      .clearDepthValue = clearDepthValue,
      .clearDepth = clearDepth && fullTarget,
      .hasDepth = prevPass.hasDepth,
      .hasStencil = prevPass.hasStencil,
  };
  const bool fullColorClear = clearColor && clearAlpha && fullTarget;
  for (uint32_t i = 0; i < newPass.colorAttachmentCount; ++i) {
    auto& color = newPass.colorAttachments[i];
    color.loadOp = wgpu::LoadOp::Undefined;
    if (color.semantic == ColorAttachmentSemantic::Normal && clearDepth && fullTarget) {
      color.clear = true;
    } else {
      color.clear = false;
    }
  }
  if (fullColorClear) {
    auto& sceneColor = newPass.colorAttachments[SceneColorAttachmentIndex];
    sceneColor.clear = true;
    sceneColor.clearValue = clearColorValue;
  }
  newPass.commands.reserve(2048);
  current_render_passes().emplace_back(std::move(newPass));
  ++g_recorder.currentRenderPass;

  const bool drawClearColor = clearColor && !fullColorClear;
  const bool drawClearAlpha = clearAlpha && !fullColorClear;
  const bool drawClearDepth = clearDepth && !fullTarget;
  if (clearRect.width > 0 && clearRect.height > 0 && (drawClearColor || drawClearAlpha || drawClearDepth)) {
    // If we're only clearing a portion of the render target (or only one of color/alpha), perform a clear draw
    push_draw_command(clear::DrawData{
        .pipeline = pipeline_ref(clear::PipelineConfig{
            .clearColor = drawClearColor,
            .clearAlpha = drawClearAlpha,
            .clearDepth = drawClearDepth,
        }),
        .color =
            wgpu::Color{
                .r = clearColorValue.x(),
                .g = clearColorValue.y(),
                .b = clearColorValue.z(),
                .a = clearColorValue.w(),
            },
        .depth = clearDepthValue,
        .rect = clearRect,
    });
  }
  push_command(CommandType::SetViewport, Command::Data{.setViewport = g_recorder.cachedViewport});
  push_command(CommandType::SetScissor, Command::Data{.setScissor = g_recorder.cachedScissor});
}

void queue_palette_conv(tex_palette_conv::ConvRequest req) {
  auto& renderPass = current_render_passes()[g_recorder.currentRenderPass];
  AURORA_ASSERT(!renderPass.sealed, "Attempted to append palette conversion to sealed render pass {}",
                g_recorder.currentRenderPass);
  renderPass.paletteConvs.push_back(std::move(req));
}

bool is_offscreen() noexcept { return g_recorder.inOffscreen; }

bool has_normal_attachment() noexcept {
  CHECK(g_recorder.currentRenderPass != UINT32_MAX, "has_normal_attachment called outside of a frame");
  const auto& pass = current_render_passes()[g_recorder.currentRenderPass];
  for (uint32_t i = 0; i < pass.colorAttachmentCount; ++i) {
    if (pass.colorAttachments[i].semantic == ColorAttachmentSemantic::Normal) {
      return true;
    }
  }
  return false;
}

RenderTargetLayout get_render_target_layout() noexcept {
  CHECK(g_recorder.currentRenderPass != UINT32_MAX, "get_render_target_layout called outside of a frame");
  return current_render_passes()[g_recorder.currentRenderPass].target_layout();
}

bool push_custom_draw(DrawTypeId type, const void* payload, size_t payloadSize) {
  if (type == InvalidDrawType) {
    Log.warn("push_custom_draw: invalid draw type");
    return false;
  }
  if (payloadSize > InlineDrawPayloadSize) {
    Log.warn("push_custom_draw: payload size {} exceeds inline payload size {}", payloadSize, InlineDrawPayloadSize);
    return false;
  }
  if (payloadSize > 0 && payload == nullptr) {
    Log.warn("push_custom_draw: non-zero payload size with null payload");
    return false;
  }
  if (!find_runtime_draw_type(type)) {
    Log.warn("push_custom_draw: unregistered draw type {:#x}", type);
    return false;
  }

  gx::fifo::drain();

  if (!g_recorder.active() || g_recorder.currentRenderPass == UINT32_MAX) {
    Log.warn("push_custom_draw: called outside an active render pass");
    return false;
  }

  CustomDrawCommand draw{};
  draw.type = type;
  draw.payloadSize = static_cast<uint32_t>(payloadSize);
  if (payloadSize > 0) {
    std::memcpy(draw.payload.data(), payload, payloadSize);
  }
  push_command(CommandType::CustomDraw, Command::Data{.customDraw = draw});
  ++g_recorder.drawCallCount;
  return true;
}

void begin_offscreen(uint32_t width, uint32_t height) {
  ZoneScoped;
  AURORA_ASSERT(width != 0 && height != 0, "begin_offscreen requires nonzero dimensions ({}x{})", width, height);
  AURORA_ASSERT(g_recorder.active() && g_recorder.currentRenderPass != UINT32_MAX,
                "begin_offscreen called outside of an active recording frame");

  if (g_recorder.inOffscreen) {
    finish_current_offscreen();
  } else {
    suspend_efb();
  }
  start_offscreen(width, height);
}

void end_offscreen() {
  ZoneScoped;
  finish_current_offscreen();
  restore_efb();
}

bool create_pass(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    Log.warn("create_pass: invalid size {}x{}", width, height);
    return false;
  }

  gx::fifo::drain();

  if (!g_recorder.active() || g_recorder.currentRenderPass == UINT32_MAX) {
    Log.warn("create_pass: called outside an active render pass");
    return false;
  }
  if (g_recorder.inOffscreen) {
    Log.warn("create_pass: an offscreen pass is already active (nesting is unsupported)");
    return false;
  }

  begin_offscreen(width, height);
  return true;
}

bool resolve_pass(const ResolveDesc& desc, ResolvedTargets& out) {
  out = {};
  gx::fifo::drain();

  if (!g_recorder.active() || g_recorder.currentRenderPass == UINT32_MAX) {
    Log.warn("resolve_pass: called outside an active render pass");
    return false;
  }

  bool wantDepth = desc.depth;
  if (wantDepth && !tex_copy_conv::snapshot_depth_supported()) {
    Log.warn("resolve_pass: depth snapshots are unsupported on this device");
    wantDepth = false;
  }

  auto& prevPass = current_render_passes()[g_recorder.currentRenderPass];
  const uint32_t width = prevPass.colorAttachments[SceneColorAttachmentIndex].size.width;
  const uint32_t height = prevPass.colorAttachments[SceneColorAttachmentIndex].size.height;
  const bool wantNormal = desc.normal && prevPass.copySourceNormalTexture;
  if (desc.normal && !g_recorder.inOffscreen && !webgpu::g_graphicsConfig.normalBuffer && webgpu::g_hasCoreFeatures &&
      webgpu::g_graphicsConfig.msaaSamples == 1) {
    g_recorder.normalRequested = true;
  }
  // Requesting no snapshots is a plain pass break (or offscreen close, discarding its output).
  if (desc.color || wantDepth || wantNormal) {
    auto& entry = acquire_pass_snapshot(width, height, desc.color, wantDepth, wantNormal);
    if (desc.color) {
      prevPass.snapshotColorDst = entry.color.texture;
      out.colorTexture = entry.color.texture;
      out.color = entry.color.view;
      out.colorFormat = entry.color.format;
    }
    if (wantDepth) {
      prevPass.snapshotDepthDst = entry.depth.view;
      out.depthTexture = entry.depth.texture;
      out.depth = entry.depth.view;
    }
    if (wantNormal) {
      prevPass.snapshotNormalDst = entry.normal.texture;
      out.normalTexture = entry.normal.texture;
      out.normal = entry.normal.view;
    }
  }
  out.width = width;
  out.height = height;

  if (g_recorder.inOffscreen) {
    // Seal the offscreen pass and resume the EFB.
    end_offscreen();
    return true;
  }

  // Seal the current EFB pass and continue on a new one that loads the existing contents.
  // EFB writes persist into the loading continuation, so content keeps the pass alive even
  // without a consumer.
  prevPass.discardable = !prevPass.has_consumer() && !prevPass.has_content();
  enqueue_pass(current_frame_packet(), g_recorder.currentRenderPass);
  resume_efb_pass_loading(prevPass);
  return true;
}

ReadbackCapture readback_capture() noexcept {
  if (!g_recorder.active() || g_recorder.currentRenderPass == UINT32_MAX) {
    return {};
  }
  const auto& pass = current_render_passes()[g_recorder.currentRenderPass];
  const auto& size = pass.colorAttachments[SceneColorAttachmentIndex].size;
  return {
      .active = true,
      .offscreen = g_recorder.inOffscreen,
      .width = size.width,
      .height = size.height,
  };
}

bool push_readback_op(std::shared_ptr<readback::Job> job, bool efbSource) {
  if (!g_recorder.active() || g_recorder.currentRenderPass == UINT32_MAX || !job) {
    return false;
  }
  auto& frame = current_frame_packet();
  if (efbSource) {
    if (g_recorder.inOffscreen) {
      return false;
    }
    auto& pass = current_render_passes()[g_recorder.currentRenderPass];
    job->depthView = pass.copySourceDepthView;
    job->colorView = pass.copySourceView;
    if (pass.has_content()) {
      pass.discardable = !pass.has_consumer() && !pass.has_content();
      enqueue_pass(frame, g_recorder.currentRenderPass);
      resume_efb_pass_loading(pass);
    }
  }
  const auto taskIndex = static_cast<uint32_t>(frame.readbackTasks.size());
  frame.readbackTasks.emplace_back(ReadbackTask{.job = std::move(job)});
  const auto opIndex = static_cast<uint32_t>(frame.ops.size());
  frame.ops.emplace_back(capture_frame_op(frame, FrameOpType::Readback, taskIndex));
  enqueue_op(frame, opIndex);
  return true;
}

bool push_encoder_task(EncoderTaskId type, const void* payload, size_t payloadSize) {
  if (type == InvalidEncoderTask) {
    Log.warn("push_encoder_task: invalid encoder task type");
    return false;
  }
  if (payloadSize > InlineDrawPayloadSize) {
    Log.warn("push_encoder_task: payload size {} exceeds inline payload size {}", payloadSize, InlineDrawPayloadSize);
    return false;
  }
  if (payloadSize > 0 && payload == nullptr) {
    Log.warn("push_encoder_task: non-zero payload size with null payload");
    return false;
  }
  if (!find_runtime_encoder_task_type(type)) {
    Log.warn("push_encoder_task: unregistered encoder task type {:#x}", type);
    return false;
  }

  gx::fifo::drain();

  if (!g_recorder.active() || g_recorder.currentRenderPass == UINT32_MAX) {
    Log.warn("push_encoder_task: called outside an active render pass");
    return false;
  }
  if (g_recorder.inOffscreen) {
    Log.warn("push_encoder_task: unsupported while an offscreen pass is active");
    return false;
  }

  // Seal the current EFB pass, record the task between it and a continuation
  // pass that loads the existing contents. EFB writes persist into the continuation, so
  // content keeps the sealed pass alive even without a consumer.
  auto& frame = current_frame_packet();
  auto& prevPass = current_render_passes()[g_recorder.currentRenderPass];
  prevPass.discardable = !prevPass.has_consumer() && !prevPass.has_content();
  enqueue_pass(frame, g_recorder.currentRenderPass);

  const auto taskIndex = static_cast<uint32_t>(frame.encoderTasks.size());
  auto& task = frame.encoderTasks.emplace_back(EncoderTask{.type = type});
  task.payloadSize = static_cast<uint32_t>(payloadSize);
  if (payloadSize > 0) {
    std::memcpy(task.payload.data(), payload, payloadSize);
  }
  const auto opIndex = static_cast<uint32_t>(frame.ops.size());
  frame.ops.emplace_back(capture_frame_op(frame, FrameOpType::EncoderTask, taskIndex));
  enqueue_op(frame, opIndex);

  resume_efb_pass_loading(prevPass);
  return true;
}

template <>
void push_draw_command(gx::DrawData data) {
  push_draw_command(make_draw_command<gx::render>(data));
}

#ifdef AURORA_ENABLE_RMLUI
template <>
void push_draw_command(rmlui::DrawData data) {
  push_draw_command(make_draw_command<rmlui::render>(data));
}
#endif

template <>
PipelineRef pipeline_ref(const gx::PipelineConfig& config) {
  return find_pipeline(config, get_render_target_layout(), gx::g_gxState.drawsBlocking);
}

#ifdef AURORA_ENABLE_RMLUI
template <>
PipelineRef pipeline_ref(const rmlui::PipelineConfig& config) {
  return find_pipeline(config);
}
#endif

void finish() {
  ZoneScoped;
  if (!g_recorder.active()) {
    return;
  }
  AURORA_ASSERT(!g_recorder.inOffscreen, "finish called while offscreen rendering is active");
  if (g_recorder.currentRenderPass != UINT32_MAX) {
    auto& frame = current_frame_packet();
    frame.uniforms.append_zeroes(gx::MaxUniformSize);
    enqueue_pass(frame, g_recorder.currentRenderPass);
    g_recorder.currentRenderPass = UINT32_MAX;
  }
}

Range push_verts(const uint8_t* data, size_t length, size_t alignment) {
  ZoneScoped;
  if (!check_recording("push_verts")) {
    return {};
  }
  return push(current_frame_packet().verts, data, length, alignment);
}

Range push_indices(const uint8_t* data, size_t length, size_t alignment) {
  ZoneScoped;
  if (!check_recording("push_indices")) {
    return {};
  }
  return push(current_frame_packet().indices, data, length, alignment);
}

Range push_uniform(const uint8_t* data, size_t length) {
  ZoneScoped;
  if (!check_recording("push_uniform")) {
    return {};
  }
  return push(current_frame_packet().uniforms, data, length, resources().limits.minUniformBufferOffsetAlignment);
}

// OrrePort, frame smoothing step (c) (recording.hpp). The camera marker: kept in the step's packet; it becomes the
// current camera for the blocks that follow only when it is one a blend can re-aim: a perspective projection, HSD's
// screen render pass, not inside an offscreen picture (menu models, shadows: those are held, plan section 4).
void orre_smooth_camera(uint64_t id, int32_t renderPass, const float* view, const float* interest) {
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  OrreSmoothCamera camera{
      .id = id,
      .renderPass = renderPass,
      .perspective = gx::g_gxState.projType == GX_PERSPECTIVE,
      .hasInterest = interest != nullptr,
  };
  for (const auto& earlier : frame.orreCameras) {
    camera.occurrence += earlier.id == id ? 1u : 0u;
  }
  std::memcpy(camera.view, view, sizeof(camera.view));
  if (interest != nullptr) {
    std::memcpy(camera.interest, interest, sizeof(camera.interest));
  }
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      camera.proj[r][c] = gx::g_gxState.proj[r][c];
    }
  }
  const bool usable = camera.perspective && renderPass == 0 && !g_recorder.inOffscreen;
  // *Changed 2026-11-17 (step d, the shadows):* a camera of HSD's offscreen render pass (the projected shadows' cameras,
  // pass 3: HSD_ShadowStartRender draws each caster into the EFB and copies it into the shadow's texture) is current for
  // the blocks that follow too, so their model pieces can blend; the camera itself is never re-aimed (smooth_camera.cpp:
  // plan_step leaves it unmatched), because the receivers project the shadow texture with that very camera's matrix of
  // the newest step. Still not inside an Aurora offscreen picture (menu models, GXCreateFrameBuffer): those stay held.
  const bool modelsOnly = !usable && renderPass != 0 && !g_recorder.inOffscreen;
  frame.orreCameras.push_back(camera);
  frame.orreCurrentCamera = usable || modelsOnly ? static_cast<int32_t>(frame.orreCameras.size() - 1) : -1;
}

// A uniform block just pushed (shader_info.cpp, build_uniform): copied when a usable camera is current and the block's
// projection is a perspective one (2D, text and fades use flat projections: held, plan section 4)
void orre_smooth_uniform(Range range, const uint8_t* data, size_t length, uint32_t uniformSize, uint32_t projOffset,
                         uint32_t postMtxOffset, const OrreSmoothColorSpan& color) {
  if (!g_recorder.active() || range.size == 0) {
    return;
  }
  auto& frame = current_frame_packet();
  // round 11 (2D at 120): a block of a named 2D piece is kept whatever the camera and projection (the game's 2D is
  // drawn through a projection of its own, with or without a camera marker before it); it is never re-aimed, only the
  // piece's own value blends (smooth_camera.cpp)
  const bool piece2d = frame.orre2dKind != 0;
  if ((frame.orreCurrentCamera < 0 && !piece2d) || g_recorder.inOffscreen || length < projOffset + 64) {
    if (frame.orreCurrentPObj != 0 && orre_draw_log_on()) { // fix (3)'s draw log: a model piece's block not kept
      orre_draw_log(frame.orreCurrentCamera < 0 ? "uniform not kept: no camera current"
                    : g_recorder.inOffscreen    ? "uniform not kept: offscreen"
                                                : "uniform not kept: too short");
    }
    return;
  }
  static const OrreSmoothCamera noCamera{};
  const auto& camera = frame.orreCurrentCamera >= 0 ? frame.orreCameras[static_cast<size_t>(frame.orreCurrentCamera)]
                                                    : noCamera;
  // a screen camera's blocks only under a perspective projection (2D, text and fades use flat ones: held). *Changed
  // 2026-11-17 (round 5):* a shadow camera's blocks (HSD's offscreen pass) whatever the projection: the bike ride's
  // shadow cameras are orthographic (a light far away), so their casters were never recorded and never blended; the
  // shadow then stayed at the newest step's pose while the bike blended, and with a moving camera it showed as a large
  // wrong shape under an older view (verified, snatcherLeo's 2026-11-17 sitting, ride 1 frame 16,442: blending only the
  // camera gave the blob, blending only the models the right shadow in the wrong place)
  if (gx::g_gxState.projType != GX_PERSPECTIVE && camera.renderPass == 0 && !piece2d) {
    if (frame.orreCurrentPObj != 0 && orre_draw_log_on()) { // fix (3)'s draw log: a model piece's block not kept
      orre_draw_log("uniform not kept: a flat projection under a screen camera");
    }
    return;
  }
  if (frame.orreCurrentPObj != 0 && orre_draw_log_on()) { // fix (3)'s draw log: which camera a model block is under
    orre_draw_log(camera.perspective ? (camera.renderPass == 0 ? "uniform: camera perspective, screen pass"
                                                               : "uniform: camera perspective, another pass")
                                     : (camera.renderPass == 0 ? "uniform: camera flat, screen pass"
                                                               : "uniform: camera flat, another pass"));
  }
  bool projIsCamera = frame.orreCurrentCamera >= 0;
  for (int r = 0; r < 4 && projIsCamera; ++r) {
    for (int c = 0; c < 4; ++c) {
      if (camera.proj[r][c] != gx::g_gxState.proj[r][c]) {
        projIsCamera = false;
        if (frame.orreCurrentPObj != 0 && orre_draw_log_on()) { // fix (3)'s draw log
          orre_draw_log(camera.perspective ? "uniform kept: own projection, camera perspective"
                                           : "uniform kept: own projection, camera flat");
        }
        break;
      }
    }
  }
  // step (d): the model piece this block belongs to, and which draw of that piece in this render pass it was built for
  // (orre_smooth_model counts them). *Changed 2026-11-17 (round 4):* the occurrence was a count of *blocks* per piece
  // across all passes. It shifted whenever a piece's shadow caster started or stopped being drawn in HSD's shadow pass
  // (pass 3), which comes before the screen draw, so the screen block was matched with the caster's block of the step
  // before or with none (verified, the model logs of P13 and P15: Nuzleaf's mouth, PObj 814d9ec0, went 1, 2, 1 blocks
  // every few steps; Nascour's Xatu's wing panel the same; a send-out's first shadow did it to every piece). Now every
  // block of one draw shares the draw's number, counted per pass, and blocks of one draw blend alike (a draw that
  // needs a second block in some steps can't shift it either).
  const uint32_t modelOccurrence = frame.orreCurrentPObj != 0 ? frame.orreCurrentDraw : 0;
  // round 5: which texture matrices this draw applies to positions or normals (OrreSmoothBlock::texPosMask; the
  // smooth_camera.cpp prepare step says why)
  uint16_t texPosMask = 0, texNrmMask = 0, texUvMtxMask = 0;
  for (uint32_t i = 0; i < gx::g_gxState.numTexGens && i < gx::g_gxState.tcgs.size(); ++i) {
    const auto& tcg = gx::g_gxState.tcgs[i];
    if (tcg.mtx < GX_TEXMTX0 || tcg.mtx > GX_TEXMTX9 || (tcg.mtx - GX_TEXMTX0) % 3 != 0) {
      continue;
    }
    const uint16_t bit = static_cast<uint16_t>(1u << ((tcg.mtx - GX_TEXMTX0) / 3));
    if (tcg.src == GX_TG_POS) {
      texPosMask |= bit;
    } else if (tcg.src == GX_TG_NRM) {
      texNrmMask |= bit;
    } else if (tcg.src >= GX_TG_TEX0 && tcg.src <= GX_TG_TEX7) {
      texUvMtxMask |= bit; // the warp blend (2026-11-18): a texture matrix applied to the vertex's own coordinates
    }
  }
  // item (b), scrolling textures (2026-11-17): the post-transform texture matrices fed by the vertex's own texture
  // coordinates, and how the textures read through them wrap. HSD's TObj matrix (a scroll, a turn, a scale, often
  // animated) is such a post matrix: setupTextureCoordGen (src/hsd/tobj.c) generates a UV coordinate through the
  // identity and then GX_PTTEXMTX<n> = the TObj's matrix (verified: the first census, looking at GX_TEXMTX<n> instead,
  // found none in 24,000 steps of P16). A scroll that wraps (the animation runs from 1 back to 0) may be blended across
  // the wrap only by shifting the step before's offset by whole texture repeats, which shows the same picture only if
  // every texture read through that matrix repeats (GX_REPEAT; whole pairs of repeats for GX_MIRROR) along that axis.
  uint32_t texUvMask = 0, repeatS = 0, repeatT = 0, mirrorS = 0, mirrorT = 0;
  for (uint32_t i = 0; i < gx::g_gxState.numTexGens && i < gx::g_gxState.tcgs.size() && postMtxOffset != 0; ++i) {
    const auto& tcg = gx::g_gxState.tcgs[i];
    if (tcg.postMtx < GX_PTTEXMTX0 || tcg.postMtx > GX_PTTEXMTX19 || (tcg.postMtx - GX_PTTEXMTX0) % 3 != 0 ||
        tcg.mtx != GX_IDENTITY || tcg.src < GX_TG_TEX0 || tcg.src > GX_TG_TEX7) {
      continue;
    }
    const uint32_t bit = 1u << ((tcg.postMtx - GX_PTTEXMTX0) / 3);
    if ((texUvMask & bit) == 0) {
      texUvMask |= bit;
      repeatS |= bit, repeatT |= bit, mirrorS |= bit, mirrorT |= bit; // cleared below by any texture that doesn't
    }
    for (uint32_t s = 0; s < gx::g_gxState.numTevStages && s < gx::g_gxState.tevStages.size(); ++s) {
      const auto& stage = gx::g_gxState.tevStages[s];
      const uint32_t map = static_cast<uint32_t>(stage.texMapId) & 0xFFu;
      if (static_cast<uint32_t>(stage.texCoordId) != i || map >= gx::g_gxState.textures.size()) {
        continue;
      }
      const auto& obj = gx::g_gxState.textures[map].texObj;
      const auto ws = obj.wrap_s(), wt = obj.wrap_t();
      if (ws != GX_REPEAT) {
        repeatS &= ~bit;
      }
      if (wt != GX_REPEAT) {
        repeatT &= ~bit;
      }
      if (ws != GX_REPEAT && ws != GX_MIRROR) {
        mirrorS &= ~bit;
      }
      if (wt != GX_REPEAT && wt != GX_MIRROR) {
        mirrorT &= ~bit;
      }
    }
  }
  const auto bytesOffset = static_cast<uint32_t>(frame.orreBlockBytes.size());
  frame.orreBlockBytes.insert(frame.orreBlockBytes.end(), data, data + length);
  frame.orreBlocks.push_back({
      .uniformOffset = range.offset,
      .size = std::max<uint32_t>(uniformSize, static_cast<uint32_t>(length)),
      .bytes = static_cast<uint32_t>(length),
      .bytesOffset = bytesOffset,
      .projOffset = projOffset,
      .camera = static_cast<uint32_t>(frame.orreCurrentCamera),
      .projIsCamera = projIsCamera,
      .modelJObj = frame.orreCurrentJObj,
      .modelPObj = frame.orreCurrentPObj,
      .modelOccurrence = modelOccurrence,
      .modelRoot = frame.orreCurrentRoot,
      .modelSlots = frame.orreCurrentSlots,
      .texPosMask = texPosMask,
      .texNrmMask = texNrmMask,
      .texUvMask = texUvMask, // item (b)
      .postMtxOffset = texUvMask != 0 ? postMtxOffset : 0u,
      .texRepeatS = repeatS,
      .texRepeatT = repeatT,
      .texWholePairsS = mirrorS,
      .texWholePairsT = mirrorT,
      // the colour-data blend (2026-11-18): the colour run, if it lies inside what was written
      .colorAt = color.end <= length && color.at < color.end ? color.at : 0u,
      .colorEnd = color.end <= length && color.at < color.end ? color.end : 0u,
      .lightAt = color.lightAt,
      .colorLayout = color.layout,
      // the warp blend (2026-11-18): its indirect matrices and the texture matrices fed by texture coordinates
      .indAt = color.indAt != 0 && color.indAt + 96 <= length ? color.indAt : 0u,
      .indStages = static_cast<uint8_t>(gx::g_gxState.numIndStages),
      .texUvMtxMask = texUvMtxMask,
      // item (a): the particle (its middle is filled in when the particle is done: orre_smooth_particle)
      .particle = frame.orreCurrentParticle,
      .particleGuard = frame.orreParticleGuard,
      .particleLife = frame.orreParticleLife,
      .particleOccurrence = frame.orreParticleOccurrence,
      // round 11: the named 2D piece (its x span is filled in when the piece is done: orre_smooth_2d)
      .piece2dKind = frame.orre2dKind,
      .piece2dVariant = frame.orre2dVariant,
      .piece2dId = frame.orre2dId,
      .piece2dValue = frame.orre2dValue,
      .piece2dOccurrence = frame.orre2dOccurrence,
      // the slot index already (regs.cpp stores GXSetCurrentMtx's id / 3). *Corrected 2026-11-17:* divided by 3 again,
      // so the HP bar (slot 9) scaled slot 3, a camera matrix, and nothing on screen moved (verified, P15's drain)
      .piece2dSlot = gx::g_gxState.currentPnMtx,
      .piece2dBlockIndex = frame.orre2dKind != 0 ? frame.orre2dBlockOrdinal++ : 0u,
  });
}

// Round 11 (recording.hpp; 2026-11-17): a 2D piece starts or ends; the one being drawn is finished first: its blocks
// get the left end of its draws' x (or stay "not known")
void orre_smooth_2d(uint32_t kind, uint32_t variant, uint32_t id, float value) {
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  if (frame.orre2dKind != 0) {
    const bool valid = frame.orre2dSeen && !frame.orre2dBad;
    for (size_t i = frame.orre2dFirstBlock; i < frame.orreBlocks.size(); ++i) {
      auto& b = frame.orreBlocks[i];
      if (b.piece2dKind == frame.orre2dKind && b.piece2dId == frame.orre2dId &&
          b.piece2dOccurrence == frame.orre2dOccurrence) {
        b.piece2dXValid = valid;
        b.piece2dMinX = frame.orre2dMinX;
      }
    }
  }
  frame.orre2dKind = kind;
  frame.orre2dVariant = variant;
  frame.orre2dId = id;
  frame.orre2dValue = value;
  frame.orre2dFirstBlock = frame.orreBlocks.size();
  frame.orre2dMinX = 0.f;
  frame.orre2dSeen = false;
  frame.orre2dBad = false;
  frame.orre2dOccurrence = kind != 0 ? frame.orre2dCounts[{kind, id}]++ : 0;
  frame.orre2dChunkOrdinal = 0;
  frame.orre2dBlockOrdinal = 0;
}

void orre_draw_log_stamp(uint32_t retrace) noexcept { g_drawLogRetrace = retrace; }

// Fix (3)'s draw log (recording.hpp; 2026-11-18): on for the steps after the retraces listed in ORRE_SMOOTH_DRAWLOG
// (the stamp of the step before: aurora_smooth_step comes after a step's draws), at most 4,000 lines a step
bool orre_draw_log_on() noexcept {
  static const std::vector<std::pair<uint32_t, uint32_t>> ranges = [] {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    const char* v = std::getenv("ORRE_SMOOTH_DRAWLOG");
    while (v != nullptr && *v != '\0') {
      unsigned from = 0, to = 0;
      int used = 0;
      if (std::sscanf(v, "%u-%u%n", &from, &to, &used) != 2) {
        break;
      }
      out.emplace_back(from, to);
      v += used;
      v += *v == ',' ? 1 : 0;
    }
    return out;
  }();
  if (ranges.empty() || !g_recorder.active()) {
    return false;
  }
  const uint32_t at = g_drawLogRetrace;
  for (const auto& [from, to] : ranges) {
    if (at >= from && at <= to) {
      return true;
    }
  }
  return false;
}

void orre_draw_log(const char* what) {
  static uint64_t lastFrame = UINT64_MAX;
  static uint32_t lines = 0;
  static uint32_t draw = 0;
  auto& frame = current_frame_packet();
  if (frame.frameIndex != lastFrame) {
    lastFrame = frame.frameIndex;
    lines = 0;
    draw = 0;
  }
  ++draw;
  if (lines++ >= 4000) {
    return;
  }
  std::fprintf(stderr,
               "[drawlog] after %u draw %u pass %u%s cam %d 2d %x/%x pobj %llx particle %llx: %s\n",
               g_drawLogRetrace, draw, g_recorder.currentRenderPass, g_recorder.inOffscreen ? " offscreen" : "",
               frame.orreCurrentCamera, frame.orre2dKind, frame.orre2dId,
               static_cast<unsigned long long>(frame.orreCurrentPObj),
               static_cast<unsigned long long>(frame.orreCurrentParticle), what);
}

bool orre_smooth_2d_active() noexcept {
  return g_recorder.active() && current_frame_packet().orre2dKind != 0;
}

bool orre_smooth_2d_vertex_mode() noexcept {
  return g_recorder.active() && (current_frame_packet().orre2dKind & GX_AURORA_ORRE_2D_VERTICES) != 0;
}

// Round 11 (recording.hpp): one draw of a vertex-mode piece, its vertices copied as pushed (big-endian GX stream)
void orre_smooth_2d_chunk(const uint8_t* data, uint32_t vtxOffset, uint32_t length, const OrreSmooth2DLayout& layout) {
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  if ((frame.orre2dKind & GX_AURORA_ORRE_2D_VERTICES) == 0 || length == 0) {
    return;
  }
  const auto bytesOffset = static_cast<uint32_t>(frame.orre2dChunkBytes.size());
  frame.orre2dChunkBytes.insert(frame.orre2dChunkBytes.end(), data, data + length);
  frame.orre2dChunks.push_back({
      .kind = frame.orre2dKind,
      .variant = frame.orre2dVariant,
      .id = frame.orre2dId,
      .occurrence = frame.orre2dOccurrence,
      .ordinal = frame.orre2dChunkOrdinal++,
      .vtxOffset = vtxOffset,
      .length = length,
      .bytesOffset = bytesOffset,
      .layout = layout,
  });
}

void orre_smooth_2d_vertices(float minX, float maxX, bool ok) {
  (void)maxX;
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  if (frame.orre2dKind == 0) {
    return;
  }
  if (!ok) {
    frame.orre2dBad = true;
    return;
  }
  frame.orre2dMinX = frame.orre2dSeen ? std::min(frame.orre2dMinX, minX) : minX;
  frame.orre2dSeen = true;
}

// Item (a) (recording.hpp; 2026-11-17): a particle starts or ends. The one that was being drawn is finished first: its
// blocks get the middle of all its drawn corners (world space: HSD's particle code draws under GX_PNMTX0 = the camera's
// view, with world positions), or stay "not known" when a draw had another layout or none was seen.
void orre_smooth_particle(uint64_t particle, uint32_t guard, uint32_t life) {
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  if (frame.orreCurrentParticle != 0) {
    const bool valid = !frame.orreParticleBad && frame.orreParticleCount != 0;
    float at[3]{};
    for (int k = 0; k < 3 && valid; ++k) {
      at[k] = static_cast<float>(frame.orreParticleSum[k] / frame.orreParticleCount);
    }
    for (size_t i = frame.orreParticleFirstBlock; i < frame.orreBlocks.size(); ++i) {
      auto& b = frame.orreBlocks[i];
      if (b.particle == frame.orreCurrentParticle && b.particleOccurrence == frame.orreParticleOccurrence) {
        b.particleAtValid = valid;
        std::memcpy(b.particleAt, at, sizeof(at));
      }
    }
  }
  frame.orreCurrentParticle = particle;
  frame.orreParticleGuard = guard;
  frame.orreParticleLife = life;
  frame.orreParticleFirstBlock = frame.orreBlocks.size();
  frame.orreParticleSum[0] = frame.orreParticleSum[1] = frame.orreParticleSum[2] = 0;
  frame.orreParticleCount = 0;
  frame.orreParticleBad = false;
  frame.orreParticleOccurrence = particle != 0 ? frame.orreParticleCounts[particle]++ : 0;
  frame.orreParticleChunkOrdinal = 0; // (3b)
}

// (3b), the particle corners (recording.hpp; 2026-11-18): one draw of the current particle, its vertices as pushed
// (big-endian GX stream), kept in the 2D chunk list under OrreParticleChunkKind for the corner blend
// (smooth_camera.cpp). The particle marker forces a new draw (DirtyUniform), so a particle's span starts its own draw.
bool orre_smooth_particle_active() noexcept {
  return g_recorder.active() && current_frame_packet().orreCurrentParticle != 0;
}

void orre_smooth_particle_chunk(const uint8_t* data, uint32_t vtxOffset, uint32_t length,
                                const OrreSmooth2DLayout& layout) {
  if (!g_recorder.active() || length == 0) {
    return;
  }
  auto& frame = current_frame_packet();
  if (frame.orreCurrentParticle == 0) {
    return;
  }
  const auto bytesOffset = static_cast<uint32_t>(frame.orre2dChunkBytes.size());
  frame.orre2dChunkBytes.insert(frame.orre2dChunkBytes.end(), data, data + length);
  frame.orre2dChunks.push_back({
      .kind = OrreParticleChunkKind,
      .variant = frame.orreParticleGuard,
      .id = static_cast<uint32_t>(frame.orreCurrentParticle),
      .occurrence = frame.orreParticleOccurrence,
      .ordinal = frame.orreParticleChunkOrdinal++,
      .vtxOffset = vtxOffset,
      .length = length,
      .bytesOffset = bytesOffset,
      .layout = layout,
  });
}

void orre_smooth_particle_vertices(const uint8_t* data, uint32_t vtxCount, uint32_t vtxSize, bool plain) {
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  if (frame.orreCurrentParticle == 0) {
    return;
  }
  if (!plain) {
    frame.orreParticleBad = true;
    return;
  }
  // GX stream data is big-endian (fifo.hpp, write_f32 swaps): read each position's three floats back
  const auto readBe = [](const uint8_t* p) {
    const uint32_t v = (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | uint32_t{p[3]};
    float out;
    std::memcpy(&out, &v, sizeof(out));
    return out;
  };
  for (uint32_t v = 0; v < vtxCount; ++v) {
    const uint8_t* p = data + static_cast<size_t>(v) * vtxSize;
    const float x = readBe(p), y = readBe(p + 4), z = readBe(p + 8);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
      frame.orreParticleBad = true;
      return;
    }
    frame.orreParticleSum[0] += x;
    frame.orreParticleSum[1] += y;
    frame.orreParticleSum[2] += z;
    ++frame.orreParticleCount;
  }
}

// Step (d) (recording.hpp): the model piece the next blocks belong to. Observation only, like the camera marker.
void orre_smooth_model(uint64_t jobj, uint64_t pobj, uint64_t root, uint32_t slots) {
  if (!g_recorder.active()) {
    return;
  }
  auto& frame = current_frame_packet();
  frame.orreCurrentJObj = jobj;
  frame.orreCurrentPObj = pobj;
  frame.orreCurrentRoot = root;   // 2026-11-17 (round 4): the model, so a model can snap whole
  frame.orreCurrentSlots = slots; // the matrix slots the piece's display list uses
  if (pobj != 0) {
    // round 4: this draw's number among the piece's draws in the current render pass (orre_smooth_uniform)
    const int32_t pass = frame.orreCurrentCamera >= 0 &&
                                 static_cast<size_t>(frame.orreCurrentCamera) < frame.orreCameras.size()
                             ? frame.orreCameras[static_cast<size_t>(frame.orreCurrentCamera)].renderPass
                             : -1;
    frame.orreCurrentDraw = frame.orreModelCounts[{jobj, pobj, pass}]++;
  }
}

Range push_storage(const uint8_t* data, size_t length) {
  ZoneScoped;
  if (!check_recording("push_storage")) {
    return {};
  }
  return push(current_frame_packet().storage, data, length, resources().limits.minStorageBufferOffsetAlignment);
}

Range push_texture_data(const uint8_t* data, u32 bytesPerRow, u32 rowsPerImage) {
  // For CopyBufferToTexture, we need an alignment of 256 per row (see Dawn kTextureBytesPerRowAlignment)
  const auto copyBytesPerRow = AURORA_ALIGN(bytesPerRow, 256);
  const auto range = map(current_frame_packet().textureUpload, copyBytesPerRow * rowsPerImage, 0);
  u8* dst = current_frame_packet().textureUpload.data() + range.offset;
  for (u32 i = 0; i < rowsPerImage; ++i) {
    memcpy(dst, data, bytesPerRow);
    data += bytesPerRow;
    dst += copyBytesPerRow;
  }
  return range;
}

uint32_t align_uniform(uint32_t value) {
  return AURORA_ALIGN(value, detail::resources().limits.minUniformBufferOffsetAlignment);
}

void insert_debug_marker(std::string label) {
#if defined(AURORA_GFX_DEBUG_GROUPS)
  auto& markers = current_frame_packet().debugMarkers;
  const auto idx = markers.size();
  markers.emplace_back(std::move(label));
  push_command(CommandType::DebugMarker, {.debugMarkerIndex = idx});
#endif
}

void push_debug_group(std::string label) {
#if defined(AURORA_GFX_DEBUG_GROUPS)
  g_recorder.debugGroupStack.push_back(std::move(label));
#endif
}
} // namespace aurora::gfx

void push_debug_group(const char* label) {
#ifdef AURORA_GFX_DEBUG_GROUPS
  aurora::gfx::g_recorder.debugGroupStack.emplace_back(label);
#endif
}
void pop_debug_group() {
#ifdef AURORA_GFX_DEBUG_GROUPS
  if (aurora::gfx::g_recorder.debugGroupStack.empty()) {
    aurora::gfx::Log.error("Debug group stack underflowed!");
    return;
  }

  aurora::gfx::g_recorder.debugGroupStack.pop_back();
#endif
}
