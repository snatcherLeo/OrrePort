#pragma once

#include "pipeline_cache.hpp"
#include "types.hpp"
#include "tex_palette_conv.hpp"
#include "texture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace aurora::gfx::readback {
struct Job;
}

namespace aurora::gfx::detail {

// OrrePort, frame smoothing step (c), the camera blend (docs/port/frame-smoothing-plan.md section 13; 2026-11-16).
// A camera the game set while this step was recorded: written into the GX stream by the port's OrreHook_CameraSet
// (HSD_CObjSetCurrent) as a GX_AURORA_ORRE_CAMERA marker, so it lands in order with the draws on the FIFO thread.
// `proj` is the GX projection as it stood at the marker (GXSetProjection comes just before it in HSD_CObjSetCurrent).
// Platform-neutral.
struct OrreSmoothCamera {
  uint64_t id = 0;          // the game's camera (its HSD_CObj address): matched with the previous step's by id
  uint32_t occurrence = 0;  // the how-many-th time this camera was set in this step (a camera set twice)
  int32_t renderPass = 0;   // HSD's render pass (0 = the screen; offscreen cameras are never blended)
  bool perspective = false; // the projection at the marker was a perspective one
  bool hasInterest = false; // `interest` holds the point the camera looks at (orbit blends need its distance)
  float view[3][4]{};       // the view matrix (world to camera), as HSD built it
  float interest[3]{};
  float proj[4][4]{};       // g_gxState.proj (rows), before Aurora's depth-range change
};
// A uniform block built while a perspective screen camera was current (shader_info.cpp, build_uniform): the draws
// using it are the ones a camera blend re-aims. `bytes` is a CPU copy of the whole block (the staging memory it went to
// is write-only), at `bytesOffset` in FramePacket::orreBlockBytes.
struct OrreSmoothBlock {
  uint32_t uniformOffset = 0; // where the step's draws find it in the shared uniform buffer
  uint32_t size = 0;          // the shader's uniform size (aligned), what a copy must provide
  uint32_t bytes = 0;         // bytes actually written (the rest of a copy is zero)
  uint32_t bytesOffset = 0;
  uint32_t projOffset = 0;    // where the projection sits in the block (after the viewport sizes and line values)
  uint32_t camera = 0;        // index into the step's cameras
  bool projIsCamera = false;  // its projection is exactly the camera's (a field-of-view blend applies too)
  // step (d), the model blend (2026-11-17): the HSD model piece being drawn when the block was built (the current
  // JObj and PObj, from OrreHook_ModelDraw's marker; 0 when none: a draw outside HSD's model path), and how many blocks
  // with the same pair came before it in this step. A block of step N is matched with the step before's block of the
  // same pair and occurrence, and its position and normal matrices blend between the two.
  uint64_t modelJObj = 0;
  uint64_t modelPObj = 0;
  uint32_t modelOccurrence = 0;
  // round 4 (2026-11-17): the piece's model (its top JObj; all pieces of one model share it) and the mask of the
  // position-matrix slots its display list uses (bit n: GX_PNMTX0 + 3n). Only those slots are blended or tested: the
  // others hold matrices an earlier piece (often another model) left in the GX state, and a jump there means nothing.
  uint64_t modelRoot = 0;
  uint32_t modelSlots = 0;
  // round 5 (2026-11-17): the texture matrices (bit k: GX_TEXMTX0 + 3k) that the draw's texture coordinate
  // generators apply to the vertex position / the normal: those carry the piece's world matrix (a projected shadow on
  // its receiver, a projection or reflection), so they must follow the piece when its matrices blend
  uint16_t texPosMask = 0;
  uint16_t texNrmMask = 0;
  // item (b), scrolling textures (2026-11-17): the post-transform texture matrices fed by the vertex's texture
  // coordinates (bit k: GX_PTTEXMTX0 + 3k; HSD's TObj matrices), where they start in the block (0: none), and per
  // matrix whether every texture read through it repeats along s / t (a whole-repeat shift shows the same picture), or
  // at least repeats or mirrors (a shift by whole pairs of repeats does)
  uint32_t texUvMask = 0;
  uint32_t postMtxOffset = 0;
  uint32_t texRepeatS = 0;
  uint32_t texRepeatT = 0;
  uint32_t texWholePairsS = 0;
  uint32_t texWholePairsT = 0;
  // the colour-data blend (2026-11-18): where the block's colour floats run ([colorAt, colorEnd); 0 / 0: unknown), where
  // the lights start inside it (0: none; eight 80-byte lights, then four u32 light masks, never blended), and which
  // registers, channels and konstant colours the shader wrote (blocks of another layout never blend with each other)
  uint32_t colorAt = 0;
  uint32_t colorEnd = 0;
  uint32_t lightAt = 0;
  uint32_t colorLayout = 0;
  // the warp blend (the capture prototype, 2026-11-18): where the block's indirect matrices start (0: none), how many
  // indirect stages the draw uses, and the texture matrices (bit k: GX_TEXMTX0 + 3k) its texture coordinate
  // generators apply to the vertex's own texture coordinates (Psychic's warp moves its picture through them)
  uint32_t indAt = 0;
  uint8_t indStages = 0;
  uint16_t texUvMtxMask = 0;
  // item (a), the particle blend (2026-11-17): the particle being drawn when the block was built (its address; 0: not a
  // particle), its guard (bank << 16 | id number) and remaining life, how many times the same particle was drawn
  // before in this step, and the middle of its drawn corners in world space (`particleAtValid` false: not known, e.g.
  // a draw in another vertex layout). A particle's block is matched with the step before's block of the same particle.
  uint64_t particle = 0;
  uint32_t particleGuard = 0;
  uint32_t particleLife = 0;
  uint32_t particleOccurrence = 0;
  bool particleAtValid = false;
  float particleAt[3]{};
  // round 11, 2D at 120 (2026-11-17): the named 2D piece being drawn when the block was built (kind 0: none), its
  // variant (picture), id (window), value (a bar's drawn width), how many times the same piece came before in this step,
  // the left end of its vertices' x (filled in when the piece is done; `piece2dXValid` false: not known), and the
  // position-matrix slot it was drawn with (GX_PNMTX0 + 3 x slot)
  uint32_t piece2dKind = 0;
  uint32_t piece2dVariant = 0;
  uint32_t piece2dId = 0;
  float piece2dValue = 0.f;
  uint32_t piece2dOccurrence = 0;
  bool piece2dXValid = false;
  float piece2dMinX = 0.f;
  uint32_t piece2dSlot = 0;
  uint32_t piece2dBlockIndex = 0; // round 11: the block's number within its piece (a piece of several draws)
};

// Round 11, 2D at 120 (2026-11-17): how one draw's vertices are laid out, for the vertex blend of a named 2D piece in
// vertex mode (GXAurora.h, GX_AURORA_ORRE_2D_VERTICES: the pause menu's buttons and darkening). Only the position (the
// vertex's first attribute: no matrix indexes before it) and the first colour (when it directly follows the position
// and is 8 bits a channel) are blended; anything else must be the same in both steps.
struct OrreSmooth2DLayout {
  uint32_t vtxSize = 0;
  uint8_t posType = 0;     // GXCompType of the position (GX_U8 ... GX_F32), big-endian in the stream
  uint8_t posCount = 0;    // 2 (x, y) or 3 (x, y, z); 0: the position isn't first (nothing blended)
  uint8_t posFrac = 0;     // fixed-point fraction bits (integer types)
  uint8_t clrBytes = 0;    // 3 or 4 colour bytes right after the position (RGB8, RGBX8, RGBA8); 0: none blended
  uint16_t posBytes = 0;   // the position's size in bytes
  bool operator==(const OrreSmooth2DLayout&) const = default;
};

// (3b), the particle corners (2026-11-18): a particle's draws are kept in the same chunk list as the 2D pieces, under
// this kind (no GX_AURORA_ORRE_2D_VERTICES bit, so it never collides with a 2D kind); id = the particle (its address),
// variant = its guard, occurrence = its draw count in the step (smooth_camera.cpp, the corner pass)
constexpr uint32_t OrreParticleChunkKind = 0x7FFF0000u;

// one draw of a named 2D piece in vertex mode: where its vertices went in this step's vertex buffer, and a CPU copy
// (the packet's own vertex bytes are a mapped staging range, gone once the step is submitted)
struct OrreSmooth2DChunk {
  uint32_t kind = 0;
  uint32_t variant = 0;
  uint32_t id = 0;
  uint32_t occurrence = 0;
  uint32_t ordinal = 0;      // the draw's number within the piece
  uint32_t vtxOffset = 0;    // in the step's vertex buffer (a draw's DrawImmediateData::vtxStart)
  uint32_t length = 0;       // bytes
  uint32_t bytesOffset = 0;  // in FramePacket::orre2dChunkBytes
  OrreSmooth2DLayout layout;
};

struct StagingHighWater {
  uint32_t verts = 0;
  uint32_t uniforms = 0;
  uint32_t indices = 0;
  uint32_t storage = 0;
  uint32_t textureUpload = 0;
  size_t textureUploadCount = 0;
};

struct CustomDrawCommand {
  DrawTypeId type = 0;
  uint32_t payloadSize = 0;
  alignas(std::max_align_t) std::array<std::byte, InlineDrawPayloadSize> payload{};
};

struct RenderPass;
using DrawEncoder = void (*)(void* payload, const wgpu::RenderPassEncoder& pass, const RenderPass& passInfo);

struct DrawCommand {
  DrawEncoder encoder = nullptr;
  alignas(std::max_align_t) std::array<std::byte, InlineDrawPayloadSize> payload{};
};

enum class CommandType {
  SetViewport,
  SetScissor,
  Draw,
  CustomDraw,
  DebugMarker,
};

struct Command {
  CommandType type;
#ifdef AURORA_GFX_DEBUG_GROUPS
  std::vector<std::string> debugGroupStack;
#endif
  union Data {
    Viewport setViewport;
    ClipRect setScissor;
    DrawCommand draw;
    CustomDrawCommand customDraw;
    size_t debugMarkerIndex;
  } data;
};

using CommandList = std::vector<Command>;

struct RenderPass {
  struct ColorAttachment {
    ColorAttachmentSemantic semantic = ColorAttachmentSemantic::Auxiliary;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    wgpu::Extent3D size;
    wgpu::TextureView view;
    wgpu::TextureView resolveView;
    Vec4<float> clearValue{0.f, 0.f, 0.f, 0.f};
    wgpu::LoadOp loadOp = wgpu::LoadOp::Undefined;
    wgpu::StoreOp storeOp = wgpu::StoreOp::Store;
    bool clear = true;
  };

  std::string label;
  std::array<ColorAttachment, MaxColorAttachments> colorAttachments;
  uint32_t colorAttachmentCount = 0;
  wgpu::TextureView depthStencilView;
  wgpu::TextureFormat depthStencilFormat = wgpu::TextureFormat::Undefined;
  wgpu::Texture copySourceTexture;
  wgpu::TextureView copySourceView;
  wgpu::TextureView copySourceDepthView;
  wgpu::Texture copySourceNormalTexture;
  uint32_t msaaSamples = 1;

  TextureHandle resolveTarget;
  GXTexFmt resolveFormat = GX_TF_RGBA8;
  GXPixelFmt resolveSourceFormat = GX_PF_RGBA6_Z24;
  ClipRect resolveRect;
  Range resolveUniformRange;
  wgpu::Texture snapshotColorDst;
  wgpu::TextureView snapshotDepthDst;
  wgpu::Texture snapshotNormalDst;
  float clearDepthValue = 1.f;
  wgpu::LoadOp depthLoadOp = wgpu::LoadOp::Undefined;
  wgpu::StoreOp depthStoreOp = wgpu::StoreOp::Store;
  wgpu::LoadOp stencilLoadOp = wgpu::LoadOp::Undefined;
  wgpu::StoreOp stencilStoreOp = wgpu::StoreOp::Undefined;
  uint32_t stencilClearValue = 0;
  CommandList commands;
  bool clearDepth = true;
  bool hasDepth = true;
  bool hasStencil = false;
  bool hasDraws = false;
  bool discardable = false;
  bool sealed = false;
  std::vector<tex_palette_conv::ConvRequest> paletteConvs;

  RenderTargetLayout target_layout() const noexcept;
  bool has_consumer() const { return resolveTarget || snapshotColorDst || snapshotDepthDst || snapshotNormalDst; }
  bool has_content() const {
    if (hasDraws || clearDepth) {
      return true;
    }
    for (uint32_t i = 0; i < colorAttachmentCount; ++i) {
      if (colorAttachments[i].clear) {
        return true;
      }
    }
    return false;
  }
};

inline void finalize_render_target_layout(RenderTargetLayout& layout) noexcept {
  Hasher hasher{};
  hasher.update(layout.colorAttachmentCount);
  for (uint32_t i = 0; i < layout.colorAttachmentCount; ++i) {
    hasher.update(layout.colorAttachments[i].semantic);
    hasher.update(layout.colorAttachments[i].format);
  }
  hasher.update(static_cast<uint32_t>(layout.depthStencilFormat));
  hasher.update(layout.sampleCount);
  layout.key = hasher.digest();
}

inline RenderTargetLayout RenderPass::target_layout() const noexcept {
  RenderTargetLayout layout{
      .colorAttachmentCount = colorAttachmentCount,
      .depthStencilFormat = hasDepth || hasStencil ? depthStencilFormat : wgpu::TextureFormat::Undefined,
      .sampleCount = msaaSamples,
  };
  for (uint32_t i = 0; i < colorAttachmentCount; ++i) {
    layout.colorAttachments[i] = {
        .semantic = colorAttachments[i].semantic,
        .format = colorAttachments[i].format,
        .width = colorAttachments[i].size.width,
        .height = colorAttachments[i].size.height,
    };
  }
  finalize_render_target_layout(layout);
  return layout;
}

struct TextureCopy {
  wgpu::TexelCopyTextureInfo src;
  wgpu::TexelCopyTextureInfo dst;
  wgpu::Extent3D size;
};

struct EncoderTask {
  EncoderTaskId type = InvalidEncoderTask;
  std::array<uint8_t, InlineDrawPayloadSize> payload{};
  uint32_t payloadSize = 0;
};

struct ReadbackTask {
  std::shared_ptr<readback::Job> job;
};

enum class FrameOpType : uint8_t {
  RenderPass,
  TextureCopy,
  EncoderTask,
  Readback,
};

struct FrameOp {
  FrameOpType type = FrameOpType::RenderPass;
  uint32_t index = 0;
  RenderPass* renderPass = nullptr;
  TextureCopy* textureCopy = nullptr;
  EncoderTask* encoderTask = nullptr;
  ReadbackTask* readbackTask = nullptr;
  StagingHighWater highWater;
  std::vector<const TextureUpload*> textureUploads;
};

using RenderPassList = std::deque<RenderPass>;

struct FramePacket {
  RenderPassList renderPasses;
  std::deque<TextureCopy> textureCopies;
  std::deque<EncoderTask> encoderTasks;
  std::deque<ReadbackTask> readbackTasks;
  std::deque<FrameOp> ops;
  std::deque<TextureUpload> textureUploads;
#ifdef AURORA_GFX_DEBUG_GROUPS
  std::vector<std::string> debugMarkers;
#endif
  ByteBuffer verts;
  ByteBuffer uniforms;
  ByteBuffer indices;
  ByteBuffer storage;
  ByteBuffer textureUpload;
  wgpu::CommandEncoder encoder;
  std::vector<AfterSubmitCallback> afterSubmitCallbacks;
  uint64_t frameId = 0;
  uint32_t frameIndex = 0;
  size_t stagingBuffer = 0;
  StagingHighWater copied;
  AuroraStats stats{};
  // OrrePort, frame smoothing (docs/port/frame-smoothing-plan.md, step b; 2026-11-16): an in-between picture replays
  // the newest step's passes. It can't replay a step that began from the previous step's EFB (keep_efb_next_frame:
  // that starting picture is gone once the step has run), nor one drawn into frame buffers that were re-created since
  // (a resize). Set in begin_recording. Platform-neutral.
  bool orreKeptEfb = false;
  uint32_t orreFrameBufferGeneration = 0;
  // OrrePort, frame smoothing step (c) (above, OrreSmoothCamera): the cameras set in this step and the uniform blocks
  // built under a perspective screen camera, with their CPU copies. Filled on the FIFO thread only while steps are
  // kept (frame.hpp, keep_steps); empty otherwise.
  std::vector<OrreSmoothCamera> orreCameras;
  std::vector<OrreSmoothBlock> orreBlocks;
  std::vector<uint8_t> orreBlockBytes;
  int32_t orreCurrentCamera = -1; // the camera the following draws are built with (-1: none, or not one to blend)
  // step (d): the HSD model piece being drawn now (GX_AURORA_ORRE_MODEL markers; 0 / 0 between pieces)
  uint64_t orreCurrentJObj = 0;
  uint64_t orreCurrentPObj = 0;
  uint64_t orreCurrentRoot = 0;   // round 4: the piece's model and its used matrix slots (OrreSmoothBlock)
  uint32_t orreCurrentSlots = 0;
  // draws so far this step by (JObj, PObj, HSD render pass): OrreSmoothBlock::modelOccurrence (*changed 2026-11-17*:
  // was blocks by (JObj, PObj), recording.cpp says why), and the current draw's number
  std::map<std::tuple<uint64_t, uint64_t, int32_t>, uint32_t> orreModelCounts;
  uint32_t orreCurrentDraw = 0;
  // item (a), the particle blend (2026-11-17): the particle being drawn now (GX_AURORA_ORRE_PARTICLE markers), the
  // first of its blocks, the sum and count of its drawn corners so far (`orreParticleBad`: a draw in another layout),
  // and the draws of each particle so far this step (OrreSmoothBlock::particleOccurrence)
  uint64_t orreCurrentParticle = 0;
  uint32_t orreParticleGuard = 0;
  uint32_t orreParticleLife = 0;
  uint32_t orreParticleOccurrence = 0;
  size_t orreParticleFirstBlock = 0;
  double orreParticleSum[3]{};
  uint32_t orreParticleCount = 0;
  bool orreParticleBad = false;
  std::map<uint64_t, uint32_t> orreParticleCounts;
  uint32_t orreParticleChunkOrdinal = 0; // (3b): the current particle's draws so far (their chunks' numbers)
  // round 11, 2D at 120: the named 2D piece being drawn now (GX_AURORA_ORRE_2D markers), its first block, the left end
  // of its draws' x so far (`orre2dBad`: a draw not readable), and the draws of each piece so far this step
  uint32_t orre2dKind = 0;
  uint32_t orre2dVariant = 0;
  uint32_t orre2dId = 0;
  float orre2dValue = 0.f;
  uint32_t orre2dOccurrence = 0;
  size_t orre2dFirstBlock = 0;
  float orre2dMinX = 0.f;
  bool orre2dSeen = false;
  bool orre2dBad = false;
  std::map<std::pair<uint32_t, uint32_t>, uint32_t> orre2dCounts;
  // vertex mode (the pause menu): the piece's draws and blocks numbered so far, and its draws' vertices this step
  uint32_t orre2dChunkOrdinal = 0;
  uint32_t orre2dBlockOrdinal = 0;
  std::vector<OrreSmooth2DChunk> orre2dChunks;
  std::vector<uint8_t> orre2dChunkBytes;
};

} // namespace aurora::gfx::detail
