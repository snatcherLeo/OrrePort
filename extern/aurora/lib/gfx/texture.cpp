#include "resources.hpp"
#include "recording.hpp"

#include "../internal.hpp"
#include "../webgpu/gpu.hpp"
#include "aurora/aurora.h"
#include "texture.hpp"
#include "texture_convert.hpp"
#include "../gx/gx_fmt.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include <fmt/format.h>
#include <tracy/Tracy.hpp>
#include <webgpu/webgpu_cpp.h>

namespace aurora::gfx {
using webgpu::g_device;
using webgpu::g_queue;

// OrrePort (observe only, 2026-11-05, port-gaps 67): live textures by creation label, for ORRE_TEX_STATS
namespace {
struct LiveTextureStats {
  int64_t count = 0;
  int64_t bytes = 0;
  int64_t created = 0;
};
std::mutex s_liveTextureMutex;
std::map<std::string, LiveTextureStats>& live_texture_stats() {
  static auto* stats = new std::map<std::string, LiveTextureStats>(); // never destroyed: textures outlive statics
  return *stats;
}
} // namespace

void track_texture(TextureRef& ref, const char* label) noexcept {
  ref.statsLabel = label != nullptr ? label : "(none)";
  ref.statsBytes = calc_texture_size(ref.format, ref.size.width, ref.size.height, ref.mipCount);
  std::lock_guard lock{s_liveTextureMutex};
  auto& stats = live_texture_stats()[ref.statsLabel];
  ++stats.count;
  ++stats.created;
  stats.bytes += static_cast<int64_t>(ref.statsBytes);
}

TextureRef::~TextureRef() {
  if (statsLabel.empty()) {
    return;
  }
  std::lock_guard lock{s_liveTextureMutex};
  auto& stats = live_texture_stats()[statsLabel];
  --stats.count;
  stats.bytes -= static_cast<int64_t>(statsBytes);
}

void log_texture_stats(uint64_t frame, const char* extra) noexcept {
  std::lock_guard lock{s_liveTextureMutex};
  int64_t totalCount = 0;
  int64_t totalBytes = 0;
  for (const auto& [label, stats] : live_texture_stats()) {
    totalCount += stats.count;
    totalBytes += stats.bytes;
  }
  fprintf(stderr, "[texstats] frame %llu: %lld live textures, %.1f MiB; %s\n", static_cast<unsigned long long>(frame),
          static_cast<long long>(totalCount), static_cast<double>(totalBytes) / (1024.0 * 1024.0),
          extra != nullptr ? extra : "");
  for (const auto& [label, stats] : live_texture_stats()) {
    if (stats.count != 0 || stats.bytes != 0) {
      fprintf(stderr, "[texstats]   %-40s live %6lld  %9.1f MiB  (created %lld)\n", label.c_str(),
              static_cast<long long>(stats.count), static_cast<double>(stats.bytes) / (1024.0 * 1024.0),
              static_cast<long long>(stats.created));
    }
  }
}

namespace {
constexpr Module Log{"aurora::gfx"};

wgpu::Extent3D physical_size(wgpu::Extent3D size, TextureFormatInfo info) {
  const uint32_t width = ((size.width + info.blockWidth - 1) / info.blockWidth) * info.blockWidth;
  const uint32_t height = ((size.height + info.blockHeight - 1) / info.blockHeight) * info.blockHeight;
  return {.width = width, .height = height, .depthOrArrayLayers = size.depthOrArrayLayers};
}

bool setup_swizzle(wgpu::TextureComponentSwizzleDescriptor& swizzle, u32 format) noexcept {
  if (!webgpu::g_textureComponentSwizzleSupported) {
    return false;
  }

  switch (format) {
  case GX_TF_R8_PC:
    swizzle.swizzle.r = wgpu::ComponentSwizzle::R;
    swizzle.swizzle.g = wgpu::ComponentSwizzle::R;
    swizzle.swizzle.b = wgpu::ComponentSwizzle::R;
    swizzle.swizzle.a = wgpu::ComponentSwizzle::R;
    return true;
  case GX_TF_RG8_PC:
    swizzle.swizzle.r = wgpu::ComponentSwizzle::R;
    swizzle.swizzle.g = wgpu::ComponentSwizzle::R;
    swizzle.swizzle.b = wgpu::ComponentSwizzle::R;
    swizzle.swizzle.a = wgpu::ComponentSwizzle::G;
    return true;
  default:
    return false;
  }
}

wgpu::AddressMode wgpu_address_mode(GXTexWrapMode mode) {
  switch (mode) {
    DEFAULT_FATAL("invalid wrap mode {}", underlying(mode));
  case GX_CLAMP:
    return wgpu::AddressMode::ClampToEdge;
  case GX_REPEAT:
    return wgpu::AddressMode::Repeat;
  case GX_MIRROR:
    return wgpu::AddressMode::MirrorRepeat;
  }
}

std::pair<wgpu::FilterMode, wgpu::MipmapFilterMode> wgpu_filter_mode(GXTexFilter filter) {
  switch (filter) {
    DEFAULT_FATAL("invalid filter mode {}", static_cast<int>(filter));
  case GX_NEAR:
    return {wgpu::FilterMode::Nearest, wgpu::MipmapFilterMode::Undefined};
  case GX_LINEAR:
    return {wgpu::FilterMode::Linear, wgpu::MipmapFilterMode::Undefined};
  case GX_NEAR_MIP_NEAR:
    return {wgpu::FilterMode::Nearest, wgpu::MipmapFilterMode::Nearest};
  case GX_LIN_MIP_NEAR:
    return {wgpu::FilterMode::Linear, wgpu::MipmapFilterMode::Nearest};
  case GX_NEAR_MIP_LIN:
    return {wgpu::FilterMode::Nearest, wgpu::MipmapFilterMode::Linear};
  case GX_LIN_MIP_LIN:
    return {wgpu::FilterMode::Linear, wgpu::MipmapFilterMode::Linear};
  }
}

u16 wgpu_aniso(GXAnisotropy aniso) {
  switch (aniso) {
    DEFAULT_FATAL("invalid aniso {}", static_cast<int>(aniso));
  case GX_ANISO_1:
  case GX_MAX_ANISOTROPY:
    return 1;
  case GX_ANISO_2:
    return std::max<u16>(webgpu::g_graphicsConfig.textureAnisotropy / 2, 1);
  case GX_ANISO_4:
    return std::max<u16>(webgpu::g_graphicsConfig.textureAnisotropy, 1);
  }
}
} // namespace

TextureHandle new_static_texture_2d(uint32_t width, uint32_t height, uint32_t mips, u32 format, ArrayRef<uint8_t> data,
                                    bool tlut, const char* label) noexcept {
  ZoneScoped;

  auto handle = new_dynamic_texture_2d(width, height, mips, format, label);
  auto& ref = *handle;

  ConvertedTexture converted;
  if (ref.gxFormat != InvalidTextureFormat) {
    if (tlut) {
      CHECK(ref.size.height == 1, "new_static_texture_2d[{}]: expected tlut height 1, got {}", label, ref.size.height);
      CHECK(ref.mipCount == 1, "new_static_texture_2d[{}]: expected tlut mipCount 1, got {}", label, ref.mipCount);
      converted = convert_tlut(ref.gxFormat, ref.size.width, data);
    } else {
      converted = convert_texture(ref.gxFormat, ref.size.width, ref.size.height, ref.mipCount, data);
    }
    if (!converted.data.empty()) {
      data = converted.data;
      ref.hasArbitraryMips = converted.hasArbitraryMips;
    }
  }

  uint32_t offset = 0;
  for (uint32_t mip = 0; mip < mips; ++mip) {
    const wgpu::Extent3D mipSize{
        .width = std::max(ref.size.width >> mip, 1u),
        .height = std::max(ref.size.height >> mip, 1u),
        .depthOrArrayLayers = ref.size.depthOrArrayLayers,
    };
    const auto info = format_info(ref.format);
    const auto physicalSize = physical_size(mipSize, info);
    const uint32_t widthBlocks = physicalSize.width / info.blockWidth;
    const uint32_t heightBlocks = physicalSize.height / info.blockHeight;
    const uint32_t bytesPerRow = widthBlocks * info.blockSize;
    const uint32_t dataSize = bytesPerRow * heightBlocks * mipSize.depthOrArrayLayers;
    CHECK(offset + dataSize <= data.size(), "new_static_texture_2d[{}]: expected at least {} bytes, got {}", label,
          offset + dataSize, data.size());
    const wgpu::TexelCopyTextureInfo dstView{
        .texture = ref.texture,
        .mipLevel = mip,
    };
    if constexpr (UseTextureBuffer) {
      queue_texture_upload_data(data.data() + offset, bytesPerRow, heightBlocks, std::move(dstView), physicalSize);
    } else {
      const wgpu::TexelCopyBufferLayout dataLayout{
          .bytesPerRow = bytesPerRow,
          .rowsPerImage = heightBlocks,
      };
      g_queue.WriteTexture(&dstView, data.data() + offset, dataSize, &dataLayout, &physicalSize);
    }
    offset += dataSize;
  }
  if (data.size() != UINT32_MAX && offset < data.size()) {
    Log.warn("new_static_texture_2d[{}]: texture used {} bytes, but given {} bytes", label, offset, data.size());
  }
  return handle;
}

TextureHandle new_dynamic_texture_2d(uint32_t width, uint32_t height, uint32_t mips, u32 gxFormat,
                                     const char* label) noexcept {
  ZoneScopedS(3);
  const auto wgpuFormat = to_wgpu(gxFormat);
  const wgpu::Extent3D size{
      .width = width,
      .height = height,
      .depthOrArrayLayers = 1,
  };
  const wgpu::TextureDescriptor textureDescriptor{
      .label = label,
      .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = wgpuFormat,
      .mipLevelCount = mips,
      .sampleCount = 1,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);
  const auto viewLabel = fmt::format("{} view", label);
  wgpu::TextureViewDescriptor textureViewDescriptor{
      .label = viewLabel.c_str(),
      .format = wgpuFormat,
      .dimension = wgpu::TextureViewDimension::e2D,
      .mipLevelCount = mips,
  };
  wgpu::TextureComponentSwizzleDescriptor swizzle;
  if (setup_swizzle(swizzle, gxFormat)) {
    textureViewDescriptor.nextInChain = &swizzle;
  }
  auto textureView = texture.CreateView(&textureViewDescriptor);
  auto handle = std::make_shared<TextureRef>(std::move(texture), std::move(textureView), wgpu::TextureView{}, size,
                                             wgpuFormat, mips, gxFormat);
  track_texture(*handle, label);
  return handle;
}

TextureHandle new_render_texture(uint32_t width, uint32_t height, u32 gxFormat, const char* label) noexcept {
  ZoneScoped;

  const auto wgpuFormat = webgpu::g_graphicsConfig.surfaceConfiguration.format;
  const wgpu::Extent3D size{
      .width = width,
      .height = height,
      .depthOrArrayLayers = 1,
  };
  const wgpu::TextureDescriptor textureDescriptor{
      .label = label,
      // OrrePort, frame smoothing round 4 (2026-11-17): + CopySrc, so a kept step's capture targets (EFB copies land in
      // these) can be saved before the step and put back before each replay (gfx/frame.cpp, save_captures)
      .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment |
               wgpu::TextureUsage::CopySrc,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = wgpuFormat,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);

  // Create texture view for color attachments
  const auto viewLabel = fmt::format("{} view", label);
  wgpu::TextureViewDescriptor textureViewDescriptor{
      .label = viewLabel.c_str(),
      .format = wgpuFormat,
      .dimension = wgpu::TextureViewDimension::e2D,
  };
  auto attachmentTextureView = texture.CreateView(&textureViewDescriptor);
  wgpu::TextureView sampleTextureView = attachmentTextureView;
  auto handle = std::make_shared<TextureRef>(std::move(texture), std::move(sampleTextureView),
                                             std::move(attachmentTextureView), size, wgpuFormat, 1, gxFormat);
  track_texture(*handle, label);
  return handle;
}

TextureHandle new_conv_texture(uint32_t width, uint32_t height, u32 gxFormat, const char* label) noexcept {
  ZoneScoped;

  const auto wgpuFormat = to_wgpu(gxFormat);
  const wgpu::Extent3D size{
      .width = width,
      .height = height,
      .depthOrArrayLayers = 1,
  };
  const wgpu::TextureDescriptor textureDescriptor{
      .label = label,
      // OrrePort, frame smoothing round 4 (2026-11-17): + CopySrc and CopyDst, for the same save and put-back of capture
      // targets as new_render_texture above
      .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc |
               wgpu::TextureUsage::CopyDst,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = wgpuFormat,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);

  // Create texture view for color attachments
  const auto viewLabel = fmt::format("{} view", label);
  wgpu::TextureViewDescriptor textureViewDescriptor{
      .label = viewLabel.c_str(),
      .format = wgpuFormat,
      .dimension = wgpu::TextureViewDimension::e2D,
  };
  auto attachmentTextureView = texture.CreateView(&textureViewDescriptor);
  wgpu::TextureView sampleTextureView = attachmentTextureView;
  auto handle = std::make_shared<TextureRef>(std::move(texture), std::move(sampleTextureView),
                                             std::move(attachmentTextureView), size, wgpuFormat, 1, gxFormat);
  track_texture(*handle, label);
  return handle;
}

void write_texture(TextureRef& ref, ArrayRef<uint8_t> data) noexcept {
  ZoneScoped;

  ConvertedTexture converted;
  if (ref.gxFormat != InvalidTextureFormat) {
    converted = convert_texture(ref.gxFormat, ref.size.width, ref.size.height, ref.mipCount, data);
    ref.hasArbitraryMips = converted.hasArbitraryMips;
    if (!converted.data.empty()) {
      data = converted.data;
    }
  }

  uint32_t offset = 0;
  for (uint32_t mip = 0; mip < ref.mipCount; ++mip) {
    const wgpu::Extent3D mipSize{
        .width = std::max(ref.size.width >> mip, 1u),
        .height = std::max(ref.size.height >> mip, 1u),
        .depthOrArrayLayers = ref.size.depthOrArrayLayers,
    };
    const auto info = format_info(ref.format);
    const auto physicalSize = physical_size(mipSize, info);
    const uint32_t widthBlocks = physicalSize.width / info.blockWidth;
    const uint32_t heightBlocks = physicalSize.height / info.blockHeight;
    const uint32_t bytesPerRow = widthBlocks * info.blockSize;
    const uint32_t dataSize = bytesPerRow * heightBlocks * mipSize.depthOrArrayLayers;
    CHECK(offset + dataSize <= data.size(), "write_texture: expected at least {} bytes, got {}", offset + dataSize,
          data.size());
    const wgpu::TexelCopyTextureInfo dstView{
        .texture = ref.texture,
        .mipLevel = mip,
    };
    if constexpr (UseTextureBuffer) {
      queue_texture_upload_data(data.data() + offset, bytesPerRow, heightBlocks, std::move(dstView), physicalSize);
    } else {
      const wgpu::TexelCopyBufferLayout dataLayout{
          .bytesPerRow = bytesPerRow,
          .rowsPerImage = heightBlocks,
      };
      g_queue.WriteTexture(&dstView, data.data() + offset, dataSize, &dataLayout, &physicalSize);
    }
    offset += dataSize;
  }
  if (data.size() != UINT32_MAX && offset < data.size()) {
    Log.warn("write_texture: texture used {} bytes, but given {} bytes", offset, data.size());
  }
}

// OrrePort (font renderer part B, 2026-11-13): one rectangle of mip 0 from linear rows, for the text atlas's
// changed rectangle. Why not write_texture: it re-sends the whole texture (about 6.5 MB at 4K) every time the game
// writes a letter. WebGPU's WriteTexture takes an origin and a size and has no row-alignment rule. All platforms.
void write_texture_region(TextureRef& ref, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                          const uint8_t* rows, uint32_t stride) noexcept {
  ZoneScoped;
  if (width == 0 || height == 0 || x + width > ref.size.width || y + height > ref.size.height) {
    return;
  }
  const wgpu::TexelCopyTextureInfo dst{
      .texture = ref.texture,
      .mipLevel = 0,
      .origin = {x, y, 0},
  };
  const wgpu::TexelCopyBufferLayout layout{
      .bytesPerRow = stride,
      .rowsPerImage = height,
  };
  const wgpu::Extent3D size{width, height, 1};
  // `rows` is the rectangle's own first texel; `stride` its row length in bytes (the texture's own texel size)
  g_queue.WriteTexture(&dst, rows, static_cast<size_t>(stride) * height, &layout, &size);
}

wgpu::SamplerDescriptor TextureBind::get_descriptor() const noexcept {
  auto [minFilter, mipFilter] = wgpu_filter_mode(texObj.min_filter());
  auto [magFilter, _] = wgpu_filter_mode(texObj.mag_filter());
  const bool mipsEnabled = mipFilter != wgpu::MipmapFilterMode::Undefined;
  float minLod = texObj.min_lod();
  float maxLod = texObj.max_lod();
  u16 maxAnisotropy = wgpu_aniso(texObj.max_aniso());
  if (ref && ref->isReplacement) {
    minLod = 0.f;
    maxLod = 1000.f;
    if (!mipsEnabled) {
      mipFilter = wgpu::MipmapFilterMode::Nearest;
    }
  } else if (mipFilter == wgpu::MipmapFilterMode::Undefined) {
    minLod = 0.f;
    maxLod = 0.f;
  }
  if ((ref && ref->hasArbitraryMips) || !mipsEnabled) {
    maxAnisotropy = 1;
  } else if (maxAnisotropy > 1) {
    magFilter = wgpu::FilterMode::Linear;
    minFilter = wgpu::FilterMode::Linear;
    mipFilter = wgpu::MipmapFilterMode::Linear;
  }
  return {
      .label = "Generated Filtering Sampler",
      .addressModeU = wgpu_address_mode(texObj.wrap_s()),
      .addressModeV = wgpu_address_mode(texObj.wrap_t()),
      .addressModeW = wgpu::AddressMode::Repeat,
      .magFilter = magFilter,
      .minFilter = minFilter,
      .mipmapFilter = mipFilter,
      .lodMinClamp = minLod,
      .lodMaxClamp = maxLod,
      .maxAnisotropy = maxAnisotropy,
  };
}
} // namespace aurora::gfx
