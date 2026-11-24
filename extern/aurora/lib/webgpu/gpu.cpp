#include "gpu.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <thread> /* OrrePort: the resize workaround's hold (port-gaps 90) */
#include <utility>
#include <vector>

#include <aurora/aurora.h>
#include <aurora/webgpu.hpp>
#include <aurora/gfx.h>
#include <magic_enum.hpp>
#include <webgpu/webgpu_cpp.h>

#include "../gx/gx.hpp"
#include "../gfx/frame.hpp"
#include "../gfx/recording.hpp"
#include "../gfx/render_worker.hpp"
#include "../internal.hpp"
#include "../window.hpp"
#include "gpu_prof.hpp"

#ifdef WEBGPU_DAWN
#include "../dawn/TracyPlatform.hpp"
#include <dawn/native/DawnNative.h>
#endif
#include <SDL3/SDL_loadso.h> /* OrrePort: save_pipeline_cache finds Dawn's PerformIdleTasks by name on Windows */

namespace aurora::webgpu {
static Module Log("aurora::gpu");

wgpu::Device g_device;
wgpu::Queue g_queue;
wgpu::Surface g_surface;
wgpu::BackendType g_backendType;
GraphicsConfig g_graphicsConfig;
TextureWithSampler g_frameBuffer;
TextureWithSampler g_frameBufferResolved;
TextureWithSampler g_depthBuffer;
TextureWithSampler g_normalBuffer;
uint32_t g_frameBufferGeneration = 0;

// EFB -> XFB copy pipeline
static wgpu::BindGroupLayout g_CopyBindGroupLayout;
wgpu::RenderPipeline g_CopyPipeline;
wgpu::RenderPipeline g_CopyPremultipliedAlphaPipeline;
wgpu::BindGroup g_CopyBindGroup;
static AuroraSampler g_Resampler = SAMPLER_BILINEAR;
static wgpu::BindGroupLayout g_ResampleBindGroupLayout;
static wgpu::RenderPipeline g_ResamplePipeline;
static wgpu::Buffer g_ResampleUniformBuffer;
static TextureWithSampler g_resampledFrameBuffer;

static wgpu::Adapter g_adapter;
wgpu::Instance g_instance;
wgpu::AdapterInfo g_adapterInfo;
static wgpu::SurfaceCapabilities g_surfaceCapabilities;
bool g_hasCoreFeatures = false;
bool g_dualSourceBlendingSupported = false;
bool g_bcTexturesSupported = false;
bool g_astcTexturesSupported = false;
bool g_textureComponentSwizzleSupported = false;
static std::atomic_bool g_initialized = false;
static std::atomic_bool g_vsyncEnabled = true;

namespace {

AuroraLogLevel wgpu_log_level(wgpu::LoggingType type) {
  switch (type) {
  case wgpu::LoggingType::Verbose:
    return LOG_DEBUG;
  case wgpu::LoggingType::Info:
    return LOG_INFO;
  case wgpu::LoggingType::Warning:
    return LOG_WARNING;
  case wgpu::LoggingType::Error:
    return LOG_ERROR;
  default:
    return LOG_FATAL;
  }
}

void wgpu_log(wgpu::LoggingType type, wgpu::StringView message) {
  Log.report(wgpu_log_level(type), "WebGPU message: {}", message);
}

struct ResampleUniformBlock {
  uint32_t samplerMode = 0;
  float frameWidth = 0.f;
  float frameHeight = 0.f;
};

constexpr std::string_view resampleShaderSource = R"(
struct Uniforms {
    sampler_mode: u32,
    frame_width: f32,
    frame_height: f32,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var s: sampler;
@group(0) @binding(2) var t: texture_2d<f32>;

var<private> pos: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -3.0),
    vec2(3.0, 1.0),
);
var<private> uvs: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(0.0, 0.0),
    vec2(0.0, 2.0),
    vec2(2.0, 0.0),
);

@vertex
fn vs_main(@builtin(vertex_index) vtxIdx: u32) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4<f32>(pos[vtxIdx], 0.0, 1.0);
    out.uv = uvs[vtxIdx];
    return out;
}

// BEGIN AREA SAMPLER REGION

fn sample_by_pixel(pixel: vec2<i32>) -> vec4<f32> {
    let source_dims = textureDimensions(t);
    let max_coord = vec2<i32>(source_dims) - vec2<i32>(1, 1);
    let coord = clamp(pixel, vec2<i32>(0, 0), max_coord);
    return textureLoad(t, coord, 0);
}

fn sample_area(frag_position: vec4<f32>) -> vec4<f32> {
    let source_size = vec2<f32>(textureDimensions(t));
    let target_size = max(vec2<f32>(uniforms.frame_width, uniforms.frame_height), vec2<f32>(1.0, 1.0));

    let source_min = clamp((frag_position.xy - vec2<f32>(0.5, 0.5)) / target_size,
                           vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0)) * source_size;
    let source_max = clamp((frag_position.xy + vec2<f32>(0.5, 0.5)) / target_size,
                           vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0)) * source_size;

    let first_pixel = vec2<i32>(floor(source_min));
    let last_pixel = vec2<i32>(ceil(source_max));
    let max_iterations: i32 = 16;

    var avg_color = vec4<f32>(0.0, 0.0, 0.0, 0.0);
    var total_weight = 0.0;

    for (var iy: i32 = 0; iy < max_iterations; iy = iy + 1) {
        let source_y = first_pixel.y + iy;
        if (source_y < last_pixel.y) {
            let y0 = f32(source_y);
            let weight_y = max(min(source_max.y, y0 + 1.0) - max(source_min.y, y0), 0.0);

            for (var ix: i32 = 0; ix < max_iterations; ix = ix + 1) {
                let source_x = first_pixel.x + ix;
                if (source_x < last_pixel.x) {
                    let x0 = f32(source_x);
                    let weight_x = max(min(source_max.x, x0 + 1.0) - max(source_min.x, x0), 0.0);
                    let weight = weight_x * weight_y;
                    avg_color += weight * sample_by_pixel(vec2<i32>(source_x, source_y));
                    total_weight += weight;
                }
            }
        }
    }

    return avg_color / max(total_weight, 0.000001);
}

// END AREA SAMPLER REGION

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var color = textureSample(t, s, in.uv);
    if (uniforms.sampler_mode == 1u) {
        color = sample_area(in.position);
    }
    return vec4(color.rgb, 1.0);
}
)"sv;

wgpu::TextureFormat to_linear(wgpu::TextureFormat format) {
  if (format == wgpu::TextureFormat::RGBA8UnormSrgb) {
    return wgpu::TextureFormat::RGBA8Unorm;
  }
  if (format == wgpu::TextureFormat::BGRA8UnormSrgb) {
    return wgpu::TextureFormat::BGRA8Unorm;
  }
  return format;
}

wgpu::TextureFormat best_surface_format() {
  if (g_surfaceCapabilities.formatCount == 0) {
    return wgpu::TextureFormat::Undefined;
  }
  for (size_t i = 0; i < g_surfaceCapabilities.formatCount; ++i) {
    const auto format = to_linear(g_surfaceCapabilities.formats[i]);
    if (format == wgpu::TextureFormat::RGBA8Unorm || format == wgpu::TextureFormat::BGRA8Unorm) {
      return format;
    }
  }
  return g_surfaceCapabilities.formats[0];
}

uint32_t sampler_mode(AuroraSampler sampler) noexcept {
  switch (sampler) {
  case SAMPLER_AREA:
    return 1;
  case SAMPLER_BILINEAR:
  default:
    return 0;
  }
}

uint32_t viewport_extent(float value) noexcept {
  return std::max(1u, static_cast<uint32_t>(std::lround(std::max(value, 1.f))));
}

} // namespace

bool vsync_enabled() noexcept { return g_vsyncEnabled.load(std::memory_order_acquire); }

wgpu::PresentMode select_present_mode(const wgpu::SurfaceCapabilities& capabilities) noexcept {
  const auto supports = [&capabilities](const wgpu::PresentMode candidate) {
    for (size_t i = 0; i < capabilities.presentModeCount; ++i) {
      if (capabilities.presentModes[i] == candidate) {
        return true;
      }
    }
    return false;
  };
  if (vsync_enabled()) {
    /* OrrePort (2026-11-13, port-gaps 78): V-Sync uses strict Fifo, not FifoRelaxed. FifoRelaxed shows a frame
     * that misses its refresh at once (mid-scan), so a game paced at the GameCube's 59.94 on a 60 Hz screen showed some
     * frames late and some twice: Espeon's idle sway on the summary page jittered (measured: the game renders it
     * evenly, the screen did not). Strict Fifo shows every frame for one refresh, in order. Fifo is supported on every
     * backend and platform (the WebGPU spec requires it), so this is the same on Windows, Linux and macOS. */
    return wgpu::PresentMode::Fifo;
  } else {
    // Dawn only disables CAMetalLayer displaySyncEnabled for Immediate on Metal
    if (g_backendType != wgpu::BackendType::Metal && supports(wgpu::PresentMode::Mailbox)) {
      return wgpu::PresentMode::Mailbox;
    }
    if (supports(wgpu::PresentMode::Immediate)) {
      return wgpu::PresentMode::Immediate;
    }
  }
  return wgpu::PresentMode::Fifo;
}

TextureWithSampler create_render_texture(uint32_t width, uint32_t height, bool multisampled) {
  const wgpu::Extent3D size{
      .width = width,
      .height = height,
      .depthOrArrayLayers = 1,
  };
  const auto format = g_graphicsConfig.surfaceConfiguration.format;
  uint32_t sampleCount = 1;
  if (multisampled) {
    sampleCount = g_graphicsConfig.msaaSamples;
  }
  if (width == 0 || height == 0) {
    Log.fatal("Invalid render texture size! {}x{}, multisampled {}, format {}", width, height,
              static_cast<uint32_t>(format), multisampled);
  }
  const wgpu::TextureDescriptor textureDescriptor{
      .label = "Render texture",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
               wgpu::TextureUsage::CopyDst,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = format,
      .mipLevelCount = 1,
      .sampleCount = sampleCount,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);

  constexpr wgpu::TextureViewDescriptor viewDescriptor{
      .label = "Render texture view",
      .dimension = wgpu::TextureViewDimension::e2D,
  };
  auto view = texture.CreateView(&viewDescriptor);

  constexpr wgpu::SamplerDescriptor samplerDescriptor{
      .label = "Render sampler",
      .addressModeU = wgpu::AddressMode::ClampToEdge,
      .addressModeV = wgpu::AddressMode::ClampToEdge,
      .addressModeW = wgpu::AddressMode::ClampToEdge,
      .magFilter = wgpu::FilterMode::Linear,
      .minFilter = wgpu::FilterMode::Linear,
      .mipmapFilter = wgpu::MipmapFilterMode::Linear,
      .lodMinClamp = 0.f,
      .lodMaxClamp = 1000.f,
      .maxAnisotropy = 1,
  };
  auto sampler = g_device.CreateSampler(&samplerDescriptor);

  return {
      .texture = std::move(texture),
      .view = std::move(view),
      .size = size,
      .format = format,
      .sampler = std::move(sampler),
  };
}

const TextureWithSampler& present_source() noexcept {
  return g_graphicsConfig.msaaSamples > 1 ? g_frameBufferResolved : g_frameBuffer;
}

void set_resampler(AuroraSampler sampler) noexcept {
  switch (sampler) {
  case SAMPLER_AREA:
  case SAMPLER_BILINEAR:
    g_Resampler = sampler;
    return;
  default:
    g_Resampler = SAMPLER_BILINEAR;
    return;
  }
}

AuroraSampler get_resampler() noexcept { return g_Resampler; }

namespace {
std::atomic<uint32_t> g_presentInsetTop{0};
} // namespace

void set_present_inset_top(uint32_t pixels) noexcept { g_presentInsetTop.store(pixels, std::memory_order_relaxed); }

Viewport calculate_game_present_viewport(uint32_t surface_width, uint32_t surface_height, uint32_t content_width,
                                         uint32_t content_height) noexcept {
  const uint32_t inset = g_presentInsetTop.load(std::memory_order_relaxed);
  if (inset == 0 || inset >= surface_height) {
    return calculate_present_viewport(surface_width, surface_height, content_width, content_height);
  }
  Viewport v = calculate_present_viewport(surface_width, surface_height - inset, content_width, content_height);
  v.top += static_cast<float>(inset);
  return v;
}

Viewport calculate_present_viewport(uint32_t surface_width, uint32_t surface_height, uint32_t content_width,
                                    uint32_t content_height) noexcept {
  if (surface_width == 0 || surface_height == 0 || content_width == 0 || content_height == 0) {
    return {};
  }

  uint32_t viewport_width = surface_width;
  uint32_t viewport_height = std::min<uint32_t>(
      surface_height, std::max<uint32_t>(1u, static_cast<uint32_t>(std::lround(static_cast<double>(viewport_width) *
                                                                               static_cast<double>(content_height) /
                                                                               static_cast<double>(content_width)))));
  if (viewport_height == surface_height) {
    viewport_width = std::min<uint32_t>(
        surface_width, std::max<uint32_t>(1u, static_cast<uint32_t>(std::lround(static_cast<double>(viewport_height) *
                                                                                static_cast<double>(content_width) /
                                                                                static_cast<double>(content_height)))));
  }

  return {
      .left = static_cast<float>((surface_width - viewport_width) / 2),
      .top = static_cast<float>((surface_height - viewport_height) / 2),
      .width = static_cast<float>(viewport_width),
      .height = static_cast<float>(viewport_height),
      .znear = 0.f,
      .zfar = 1.f,
  };
}

static TextureWithSampler create_depth_texture(uint32_t width, uint32_t height) {
  const wgpu::Extent3D size{
      .width = width,
      .height = height,
      .depthOrArrayLayers = 1,
  };
  const auto format = g_graphicsConfig.depthFormat;
  const wgpu::TextureDescriptor textureDescriptor{
      .label = "Depth texture",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
      .dimension = wgpu::TextureDimension::e2D,
      .size = size,
      .format = format,
      .mipLevelCount = 1,
      .sampleCount = g_graphicsConfig.msaaSamples,
  };
  auto texture = g_device.CreateTexture(&textureDescriptor);

  const wgpu::TextureViewDescriptor viewDescriptor{
      .label = "Depth texture view",
      .dimension = wgpu::TextureViewDimension::e2D,
  };
  auto view = texture.CreateView(&viewDescriptor);

  const wgpu::SamplerDescriptor samplerDescriptor{
      .label = "Depth sampler",
      .addressModeU = wgpu::AddressMode::ClampToEdge,
      .addressModeV = wgpu::AddressMode::ClampToEdge,
      .addressModeW = wgpu::AddressMode::ClampToEdge,
      .magFilter = wgpu::FilterMode::Linear,
      .minFilter = wgpu::FilterMode::Linear,
      .mipmapFilter = wgpu::MipmapFilterMode::Linear,
      .lodMinClamp = 0.f,
      .lodMaxClamp = 1000.f,
      .maxAnisotropy = 1,
  };
  auto sampler = g_device.CreateSampler(&samplerDescriptor);

  return {
      .texture = std::move(texture),
      .view = std::move(view),
      .size = size,
      .format = format,
      .sampler = std::move(sampler),
  };
}

static TextureWithSampler create_normal_texture(uint32_t width, uint32_t height) {
  const wgpu::Extent3D size{width, height, 1};
  const wgpu::TextureDescriptor desc{
      .label = "Scene normals",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
      .size = size,
      .format = NormalBufferFormat,
  };
  auto texture = g_device.CreateTexture(&desc);
  auto view = texture.CreateView();
  return {.texture = std::move(texture), .view = std::move(view), .size = size, .format = NormalBufferFormat};
}

bool enable_normal_buffer() {
  if (g_graphicsConfig.normalBuffer) {
    return true;
  }
  if (!g_hasCoreFeatures || g_graphicsConfig.msaaSamples != 1 || !g_device || g_frameBuffer.size.width == 0 ||
      g_frameBuffer.size.height == 0) {
    return false;
  }
  g_normalBuffer = create_normal_texture(g_frameBuffer.size.width, g_frameBuffer.size.height);
  g_graphicsConfig.normalBuffer = true;
  return true;
}

void create_copy_pipeline() {
  wgpu::ShaderSourceWGSL sourceDescriptor{};
  sourceDescriptor.code = R"""(
@group(0) @binding(0)
var efb_sampler: sampler;
@group(0) @binding(1)
var efb_texture: texture_2d<f32>;

struct VertexOutput {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

var<private> pos: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -3.0),
    vec2(3.0, 1.0),
);
var<private> uvs: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(0.0, 0.0),
    vec2(0.0, 2.0),
    vec2(2.0, 0.0),
);

@vertex
fn vs_main(@builtin(vertex_index) vtxIdx: u32) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4<f32>(pos[vtxIdx], 0.0, 1.0);
    out.uv = uvs[vtxIdx];
    return out;
}

@fragment
fn fs_opaque(in: VertexOutput) -> @location(0) vec4<f32> {
    let color = textureSample(efb_texture, efb_sampler, in.uv);
    return vec4(color.rgb, 1.0);
}

@fragment
fn fs_premultiplied_alpha(in: VertexOutput) -> @location(0) vec4<f32> {
    return textureSample(efb_texture, efb_sampler, in.uv);
}
)""";
  const wgpu::ShaderModuleDescriptor moduleDescriptor{
      .nextInChain = &sourceDescriptor,
      .label = "XFB Copy Module",
  };
  auto module = g_device.CreateShaderModule(&moduleDescriptor);
  const std::array bindGroupLayoutEntries{
      wgpu::BindGroupLayoutEntry{
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler =
              wgpu::SamplerBindingLayout{
                  .type = wgpu::SamplerBindingType::Filtering,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture =
              wgpu::TextureBindingLayout{
                  .sampleType = wgpu::TextureSampleType::Float,
                  .viewDimension = wgpu::TextureViewDimension::e2D,
              },
      },
  };
  const wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
      .entryCount = bindGroupLayoutEntries.size(),
      .entries = bindGroupLayoutEntries.data(),
  };
  g_CopyBindGroupLayout = g_device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
  const wgpu::PipelineLayoutDescriptor layoutDescriptor{
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &g_CopyBindGroupLayout,
  };
  auto pipelineLayout = g_device.CreatePipelineLayout(&layoutDescriptor);

  const auto make_copy_pipeline = [&](const char* label, const char* fragmentEntryPoint,
                                      const wgpu::BlendState* blend) {
    const std::array colorTargets{wgpu::ColorTargetState{
        .format = g_graphicsConfig.surfaceConfiguration.format,
        .blend = blend,
        .writeMask = wgpu::ColorWriteMask::All,
    }};
    const wgpu::FragmentState fragmentState{
        .module = module,
        .entryPoint = fragmentEntryPoint,
        .targetCount = colorTargets.size(),
        .targets = colorTargets.data(),
    };
    const wgpu::RenderPipelineDescriptor pipelineDescriptor{
        .label = label,
        .layout = pipelineLayout,
        .vertex =
            wgpu::VertexState{
                .module = module,
                .entryPoint = "vs_main",
            },
        .primitive =
            wgpu::PrimitiveState{
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
        .multisample =
            wgpu::MultisampleState{
                .count = 1,
                .mask = UINT32_MAX,
            },
        .fragment = &fragmentState,
    };
    return g_device.CreateRenderPipeline(&pipelineDescriptor);
  };
  g_CopyPipeline = make_copy_pipeline("XFB Copy Pipeline", "fs_opaque", nullptr);

  const wgpu::BlendState premultipliedAlphaBlend{
      .color =
          {
              .operation = wgpu::BlendOperation::Add,
              .srcFactor = wgpu::BlendFactor::One,
              .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
          },
      .alpha =
          {
              .operation = wgpu::BlendOperation::Add,
              .srcFactor = wgpu::BlendFactor::One,
              .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
          },
  };
  g_CopyPremultipliedAlphaPipeline =
      make_copy_pipeline("XFB Premultiplied Alpha Copy Pipeline", "fs_premultiplied_alpha", &premultipliedAlphaBlend);
}

void create_resample_pipeline() {
  wgpu::ShaderSourceWGSL sourceDescriptor{};
  sourceDescriptor.code = resampleShaderSource;
  const wgpu::ShaderModuleDescriptor moduleDescriptor{
      .nextInChain = &sourceDescriptor,
      .label = "Present Resample Module",
  };
  auto module = g_device.CreateShaderModule(&moduleDescriptor);
  const std::array colorTargets{wgpu::ColorTargetState{
      .format = g_graphicsConfig.surfaceConfiguration.format,
      .writeMask = wgpu::ColorWriteMask::All,
  }};
  const wgpu::FragmentState fragmentState{
      .module = module,
      .entryPoint = "fs_main",
      .targetCount = colorTargets.size(),
      .targets = colorTargets.data(),
  };
  const std::array bindGroupLayoutEntries{
      wgpu::BindGroupLayoutEntry{
          .binding = 0,
          .visibility = wgpu::ShaderStage::Fragment,
          .buffer =
              wgpu::BufferBindingLayout{
                  .type = wgpu::BufferBindingType::Uniform,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 1,
          .visibility = wgpu::ShaderStage::Fragment,
          .sampler =
              wgpu::SamplerBindingLayout{
                  .type = wgpu::SamplerBindingType::Filtering,
              },
      },
      wgpu::BindGroupLayoutEntry{
          .binding = 2,
          .visibility = wgpu::ShaderStage::Fragment,
          .texture =
              wgpu::TextureBindingLayout{
                  .sampleType = wgpu::TextureSampleType::Float,
                  .viewDimension = wgpu::TextureViewDimension::e2D,
              },
      },
  };
  const wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
      .entryCount = bindGroupLayoutEntries.size(),
      .entries = bindGroupLayoutEntries.data(),
  };
  g_ResampleBindGroupLayout = g_device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
  const wgpu::PipelineLayoutDescriptor layoutDescriptor{
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &g_ResampleBindGroupLayout,
  };
  auto pipelineLayout = g_device.CreatePipelineLayout(&layoutDescriptor);
  const wgpu::RenderPipelineDescriptor pipelineDescriptor{
      .label = "Present Resample Pipeline",
      .layout = pipelineLayout,
      .vertex =
          wgpu::VertexState{
              .module = module,
              .entryPoint = "vs_main",
          },
      .primitive =
          wgpu::PrimitiveState{
              .topology = wgpu::PrimitiveTopology::TriangleList,
          },
      .multisample =
          wgpu::MultisampleState{
              .count = 1,
              .mask = UINT32_MAX,
          },
      .fragment = &fragmentState,
  };
  g_ResamplePipeline = g_device.CreateRenderPipeline(&pipelineDescriptor);

  const wgpu::BufferDescriptor uniformBufferDescriptor{
      .label = "Present Resample Uniform Buffer",
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
      .size = AURORA_ALIGN(sizeof(ResampleUniformBlock), 16),
  };
  g_ResampleUniformBuffer = g_device.CreateBuffer(&uniformBufferDescriptor);
}

wgpu::BindGroup create_copy_bind_group(const TextureWithSampler& source) {
  const std::array bindGroupEntries{
      wgpu::BindGroupEntry{
          .binding = 0,
          .sampler = source.sampler,
      },
      wgpu::BindGroupEntry{
          .binding = 1,
          .textureView = source.view,
      },
  };
  const wgpu::BindGroupDescriptor bindGroupDescriptor{
      .layout = g_CopyBindGroupLayout,
      .entryCount = bindGroupEntries.size(),
      .entries = bindGroupEntries.data(),
  };
  return g_device.CreateBindGroup(&bindGroupDescriptor);
}

const TextureWithSampler& resample_present_source(const wgpu::CommandEncoder& encoder, const Viewport& viewport) {
  const auto& source = present_source();
  const uint32_t width = viewport_extent(viewport.width);
  const uint32_t height = viewport_extent(viewport.height);
  if (!g_resampledFrameBuffer.view || g_resampledFrameBuffer.size.width != width ||
      g_resampledFrameBuffer.size.height != height || g_resampledFrameBuffer.format != source.format) {
    g_resampledFrameBuffer = create_render_texture(width, height, false);
  }

  const ResampleUniformBlock uniform{
      .samplerMode = sampler_mode(g_Resampler),
      .frameWidth = static_cast<float>(width),
      .frameHeight = static_cast<float>(height),
  };
  AURORA_ASSERT(gfx::render_worker::is_worker_thread(), "Present resample queue write must run on the render worker");
  g_queue.WriteBuffer(g_ResampleUniformBuffer, 0, &uniform, sizeof(uniform));

  const std::array bindGroupEntries{
      wgpu::BindGroupEntry{
          .binding = 0,
          .buffer = g_ResampleUniformBuffer,
          .size = AURORA_ALIGN(sizeof(ResampleUniformBlock), 16),
      },
      wgpu::BindGroupEntry{
          .binding = 1,
          .sampler = source.sampler,
      },
      wgpu::BindGroupEntry{
          .binding = 2,
          .textureView = source.view,
      },
  };
  const wgpu::BindGroupDescriptor bindGroupDescriptor{
      .layout = g_ResampleBindGroupLayout,
      .entryCount = bindGroupEntries.size(),
      .entries = bindGroupEntries.data(),
  };
  const auto bindGroup = g_device.CreateBindGroup(&bindGroupDescriptor);

  const std::array attachments{
      wgpu::RenderPassColorAttachment{
          .view = g_resampledFrameBuffer.view,
          .loadOp = wgpu::LoadOp::Clear,
          .storeOp = wgpu::StoreOp::Store,
      },
  };
  const wgpu::RenderPassDescriptor renderPassDescriptor{
      .label = "Present resample render pass",
      .colorAttachmentCount = attachments.size(),
      .colorAttachments = attachments.data(),
      .timestampWrites = gpu_prof::pass_writes("Present resample"),
  };
  const auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
  pass.SetPipeline(g_ResamplePipeline);
  pass.SetBindGroup(0, bindGroup, 0, nullptr);
  pass.SetViewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
  pass.Draw(3);
  pass.End();

  return g_resampledFrameBuffer;
}

static wgpu::BackendType to_wgpu_backend(AuroraBackend backend) {
  switch (backend) {
  case BACKEND_WEBGPU:
    return wgpu::BackendType::WebGPU;
  case BACKEND_D3D11:
    return wgpu::BackendType::D3D11;
  case BACKEND_D3D12:
    return wgpu::BackendType::D3D12;
  case BACKEND_METAL:
    return wgpu::BackendType::Metal;
  case BACKEND_VULKAN:
    return wgpu::BackendType::Vulkan;
  case BACKEND_OPENGL:
    return wgpu::BackendType::OpenGL;
  case BACKEND_OPENGLES:
    return wgpu::BackendType::OpenGLES;
  default:
    return wgpu::BackendType::Null;
  }
}

static void release_surface_locked() noexcept { g_surface = {}; }

static bool create_surface() {
  SDL_Window* window = window::get_sdl_window();
  if (window == nullptr) {
    Log.error("Failed to create surface: no window");
    return false;
  }
  window::SurfaceLock surfaceLock;
  release_surface_locked();
  g_surface = create_window_surface(g_instance, window, "Surface");
  if (!g_surface) {
    Log.error("Failed to create surface");
    return false;
  }
  return true;
}

bool initialize(AuroraBackend auroraBackend, bool allowCpu) {
  if (!g_instance) {
    Log.info("Creating WebGPU instance");
    const std::array requiredInstanceFeatures{
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instanceDescriptor{
        .requiredFeatureCount = requiredInstanceFeatures.size(),
        .requiredFeatures = requiredInstanceFeatures.data(),
    };
#ifdef WEBGPU_DAWN
    constexpr std::array instanceToggles{
        "allow_unsafe_apis",
    };
    wgpu::DawnTogglesDescriptor instanceTogglesDescriptor{wgpu::DawnTogglesDescriptor::Init{
        .enabledToggleCount = instanceToggles.size(),
        .enabledToggles = instanceToggles.data(),
    }};
#ifndef __MINGW32__
    dawn::native::DawnInstanceDescriptor dawnInstanceDescriptor{};
    dawnInstanceDescriptor.nextInChain = &instanceTogglesDescriptor;
#ifdef AURORA_GFX_DEBUG_GROUPS
    dawnInstanceDescriptor.backendValidationLevel = auroraBackend == BACKEND_VULKAN
                                                        ? dawn::native::BackendValidationLevel::Partial
                                                        : dawn::native::BackendValidationLevel::Disabled;
#else
    dawnInstanceDescriptor.backendValidationLevel = dawn::native::BackendValidationLevel::Disabled;
#endif
    dawnInstanceDescriptor.SetLoggingCallback(wgpu_log);
#ifdef TRACY_ENABLE
    dawnInstanceDescriptor.platform = tracy_dawn_platform();
#endif
    instanceDescriptor.nextInChain = &dawnInstanceDescriptor;
#else
    // OrrePort (MinGW GCC against the MSVC-built prebuilt Dawn): dawn::native::DawnInstanceDescriptor cannot be
    // used: its constructor is an MSVC-mangled C++ export, and GCC lays the struct out differently (72 vs 80 bytes,
    // Itanium reuses ChainedStruct's tail padding). Mirror the MSVC layout as a plain struct and chain it through the
    // C API. Approach from melee-pc (github.com/999sian/melee-pc, extern/aurora/lib/webgpu/gpu.cpp; used under snatcherLeo's
    // private-use decision, docs/licensing.md); here it also keeps the toggles descriptor in the chain.
    struct {
      WGPUChainedStruct chain{nullptr, WGPUSType_DawnInstanceDescriptor};
      uint32_t additionalRuntimeSearchPathsCount = 0;
      const char* const* additionalRuntimeSearchPaths = nullptr;
      dawn::platform::Platform* platform = nullptr;
      dawn::native::BackendValidationLevel backendValidationLevel = dawn::native::BackendValidationLevel::Disabled;
      bool beginCaptureOnStartup = false;
      WGPULoggingCallbackInfo loggingCallbackInfo = WGPU_LOGGING_CALLBACK_INFO_INIT;
    } dawnInstanceDescriptor;
    dawnInstanceDescriptor.chain.next = reinterpret_cast<WGPUChainedStruct*>(&instanceTogglesDescriptor);
    static_assert(sizeof(dawnInstanceDescriptor) == 80);
    static_assert(offsetof(decltype(dawnInstanceDescriptor), backendValidationLevel) == 40);
    static_assert(offsetof(decltype(dawnInstanceDescriptor), loggingCallbackInfo) == 48);
    dawnInstanceDescriptor.loggingCallbackInfo.callback = [](WGPULoggingType type, WGPUStringView message, void*,
                                                             void*) {
      wgpu_log(static_cast<wgpu::LoggingType>(type), message);
    };
    instanceDescriptor.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&dawnInstanceDescriptor);
#endif
#endif
    g_instance = wgpu::CreateInstance(&instanceDescriptor);
    if (!g_instance) {
      Log.error("Failed to create WebGPU instance");
      return false;
    }
  }
  const wgpu::BackendType backend = to_wgpu_backend(auroraBackend);
  Log.info("Attempting to initialize {}", magic_enum::enum_name(backend));
#if 0
  // D3D12's debug layer is very slow
  g_dawnInstance->EnableBackendValidation(backend != WGPUBackendType::D3D12);
#endif

  if (!create_surface()) {
    return false;
  }
  // OrrePort: the GPU choice (AuroraConfig.gpuPreference). "low-power" asks for the integrated GPU (the low-end test
  // path). Otherwise ("high-performance", the default) a high-performance adapter is first requested without tying
  // it to the window's surface (2026-10-17: with the surface, Dawn returned the integrated Intel GPU on a desktop with
  // an RTX 4090, likely because the monitor's GPU wins); a discrete GPU that can present to the surface is taken,
  // anything else falls back to the usual surface-compatible request.
  const bool lowPower = g_config.gpuPreference != nullptr && std::string_view{g_config.gpuPreference} == "low-power";
  // OrrePort (2026-11-11): "software" asks for the backend's software renderer (WebGPU's fallback adapter: Microsoft
  // WARP on Direct3D 12, SwiftShader on Vulkan when Dawn ships it), a second, non-NVIDIA implementation for frame
  // comparisons on a PC that has only one GPU (docs/port/pipeline-hitches.md). Slow; for checks only.
  const bool software = g_config.gpuPreference != nullptr && std::string_view{g_config.gpuPreference} == "software";
  if (software) {
    const wgpu::RequestAdapterOptions swOptions{
        .featureLevel = wgpu::FeatureLevel::Compatibility,
        .forceFallbackAdapter = true,
        .backendType = backend,
        .compatibleSurface = g_surface,
    };
    const auto swFuture = g_instance.RequestAdapter(
        &swOptions, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
          if (status == wgpu::RequestAdapterStatus::Success) {
            g_adapter = std::move(adapter);
          } else {
            Log.warn("No software adapter for this backend: {}", message);
          }
        });
    g_instance.WaitAny(swFuture, 5000000000);
  }
  if (!lowPower && !software) {
    const wgpu::RequestAdapterOptions freeOptions{
        .featureLevel = wgpu::FeatureLevel::Compatibility,
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .backendType = backend,
    };
    wgpu::Adapter candidate;
    const auto freeFuture = g_instance.RequestAdapter(
        &freeOptions, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView) {
          if (status == wgpu::RequestAdapterStatus::Success) {
            candidate = std::move(adapter);
          }
        });
    if (g_instance.WaitAny(freeFuture, 5000000000) == wgpu::WaitStatus::Success && candidate) {
      wgpu::AdapterInfo info;
      candidate.GetInfo(&info);
      wgpu::SurfaceCapabilities caps{};
      const bool canPresent = g_surface.GetCapabilities(candidate, &caps) == wgpu::Status::Success && caps.formatCount > 0;
      Log.info("High-performance adapter (any surface): {} ({}), presents to the window: {}",
               info.device.IsUndefined() ? wgpu::StringView("Unknown") : info.device,
               magic_enum::enum_name(info.adapterType), canPresent);
      if (info.adapterType == wgpu::AdapterType::DiscreteGPU && canPresent) {
        g_adapter = std::move(candidate);
      }
    }
  }
  if (!g_adapter) {
    const wgpu::RequestAdapterOptions options{
        .featureLevel = wgpu::FeatureLevel::Compatibility,
        .powerPreference = lowPower ? wgpu::PowerPreference::LowPower : wgpu::PowerPreference::HighPerformance,
        .backendType = backend,
        .compatibleSurface = g_surface,
    };
    Log.info("Requesting adapter\n  Feature level: {}\n  Power preference: {}\n  Backend: {}\n  Compatible surface: {}",
             magic_enum::enum_name(options.featureLevel), magic_enum::enum_name(options.powerPreference),
             magic_enum::enum_name(options.backendType), static_cast<bool>(options.compatibleSurface));
    bool requestAdapterCallbackCompleted = false;
    wgpu::RequestAdapterStatus requestAdapterStatus = wgpu::RequestAdapterStatus::CallbackCancelled;
    std::string requestAdapterMessage;
    const auto future = g_instance.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
          requestAdapterCallbackCompleted = true;
          requestAdapterStatus = status;
          requestAdapterMessage = std::string{std::string_view{message}};
          if (status == wgpu::RequestAdapterStatus::Success) {
            g_adapter = std::move(adapter);
          } else {
            Log.warn("Adapter request failed: {}: {}", magic_enum::enum_name(status), message);
          }
        });
    const auto status = g_instance.WaitAny(future, 5000000000);
    if (status != wgpu::WaitStatus::Success) {
      if (requestAdapterCallbackCompleted) {
        Log.error("Failed to create adapter: wait status {}, request status {}, message: {}",
                  magic_enum::enum_name(status), magic_enum::enum_name(requestAdapterStatus), requestAdapterMessage);
      } else {
        Log.error("Failed to create adapter: wait status {}, request callback did not complete",
                  magic_enum::enum_name(status));
      }
      return false;
    }
    if (!g_adapter) {
      if (requestAdapterCallbackCompleted) {
        Log.error("Failed to create adapter: request status {}, message: {}",
                  magic_enum::enum_name(requestAdapterStatus), requestAdapterMessage);
      } else {
        Log.error("Failed to create adapter: request callback did not complete");
      }
      return false;
    }
  }
  g_adapter.GetInfo(&g_adapterInfo);
  auto adapterName = g_adapterInfo.device;
  if (adapterName.IsUndefined()) {
    adapterName = wgpu::StringView("Unknown");
  }
  if (!allowCpu && g_adapterInfo.adapterType == wgpu::AdapterType::CPU && backend != wgpu::BackendType::Null) {
    Log.warn("Ignoring CPU adapter: {}", adapterName);
    g_adapterInfo = {};
    g_adapter = {};
    return false;
  }
  auto description = g_adapterInfo.description;
  if (description.IsUndefined()) {
    description = wgpu::StringView("Unknown");
  }
  g_backendType = g_adapterInfo.backendType;
  const auto backendName = magic_enum::enum_name(g_backendType);
  Log.info("Graphics adapter information\n  API: {}\n  Device: {} ({})\n  Driver: {}", backendName, adapterName,
           magic_enum::enum_name(g_adapterInfo.adapterType), description);

  {
    wgpu::Limits supportedLimits{};
    g_adapter.GetLimits(&supportedLimits);
    wgpu::CompatibilityModeLimits compatibilityModeLimits{wgpu::CompatibilityModeLimits::Init{
        .maxStorageBuffersInVertexStage = 2,
        .maxStorageBuffersInFragmentStage = 2,
    }};
    const wgpu::Limits requiredLimits{
        .nextInChain = &compatibilityModeLimits,
        // Use "best" supported limits
        .maxTextureDimension1D = supportedLimits.maxTextureDimension1D == 0 ? WGPU_LIMIT_U32_UNDEFINED
                                                                            : supportedLimits.maxTextureDimension1D,
        .maxTextureDimension2D = supportedLimits.maxTextureDimension2D == 0 ? WGPU_LIMIT_U32_UNDEFINED
                                                                            : supportedLimits.maxTextureDimension2D,
        .maxTextureDimension3D = supportedLimits.maxTextureDimension3D == 0 ? WGPU_LIMIT_U32_UNDEFINED
                                                                            : supportedLimits.maxTextureDimension3D,
        .maxTextureArrayLayers = supportedLimits.maxTextureArrayLayers == 0 ? WGPU_LIMIT_U32_UNDEFINED
                                                                            : supportedLimits.maxTextureArrayLayers,
        .maxStorageBuffersPerShaderStage = 2,
        .minUniformBufferOffsetAlignment =
            supportedLimits.minUniformBufferOffsetAlignment < 64 ? 64 : supportedLimits.minUniformBufferOffsetAlignment,
        .minStorageBufferOffsetAlignment =
            supportedLimits.minStorageBufferOffsetAlignment < 16 ? 16 : supportedLimits.minStorageBufferOffsetAlignment,
        .maxImmediateSize = sizeof(gx::DrawImmediateData),
    };
    Log.info(
        "Using limits:"
        "\n  maxTextureDimension1D: {}"
        "\n  maxTextureDimension2D: {}"
        "\n  maxTextureDimension3D: {}"
        "\n  maxTextureArrayLayers: {}"
        "\n  maxStorageBuffersPerShaderStage: {}"
        "\n  minUniformBufferOffsetAlignment: {}"
        "\n  minStorageBufferOffsetAlignment: {}"
        "\n  maxImmediateSize: {}",
        requiredLimits.maxTextureDimension1D, requiredLimits.maxTextureDimension2D,
        requiredLimits.maxTextureDimension3D, requiredLimits.maxTextureArrayLayers,
        requiredLimits.maxStorageBuffersPerShaderStage, requiredLimits.minUniformBufferOffsetAlignment,
        requiredLimits.minStorageBufferOffsetAlignment, requiredLimits.maxImmediateSize);
    std::vector<wgpu::FeatureName> requiredFeatures;
    g_hasCoreFeatures = false;
    g_bcTexturesSupported = false;
    g_astcTexturesSupported = false;
    g_textureComponentSwizzleSupported = false;
    g_dualSourceBlendingSupported = false;
    wgpu::SupportedFeatures supportedFeatures;
    g_adapter.GetFeatures(&supportedFeatures);
    for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
      const auto feature = supportedFeatures.features[i];
      if (feature == wgpu::FeatureName::CoreFeaturesAndLimits || feature == wgpu::FeatureName::TextureCompressionBC ||
          feature == wgpu::FeatureName::TextureCompressionASTC ||
          feature == wgpu::FeatureName::TextureComponentSwizzle || feature == wgpu::FeatureName::DualSourceBlending) {
        if (feature == wgpu::FeatureName::CoreFeaturesAndLimits) {
          g_hasCoreFeatures = true;
        } else if (feature == wgpu::FeatureName::TextureCompressionBC) {
          g_bcTexturesSupported = true;
        } else if (feature == wgpu::FeatureName::TextureCompressionASTC) {
          g_astcTexturesSupported = true;
        } else if (feature == wgpu::FeatureName::TextureComponentSwizzle) {
          g_textureComponentSwizzleSupported = true;
        } else if (feature == wgpu::FeatureName::DualSourceBlending) {
          g_dualSourceBlendingSupported = true;
        }
        requiredFeatures.push_back(feature);
      }
#ifdef WEBGPU_DAWN
      // OrrePort: Aurora calls the device from several threads at once: the game thread creates textures, samplers and
      // draw bind groups while recording (gx.cpp, resource_cache.cpp), the render worker creates the present and copy
      // bind groups and releases expired ones, the pipeline thread creates pipelines. Dawn native locks the device only
      // with this feature; without it two threads can allocate from the same D3D12 descriptor allocator at once (likely
      // cause of the intermittent CreateBindGroup crash in the worker's end_frame once the frame boundary moved to the
      // display copy and the game thread kept recording while the worker presented).
      if (feature == wgpu::FeatureName::ImplicitDeviceSynchronization) {
        requiredFeatures.push_back(feature);
      }
#endif
#ifdef TRACY_ENABLE
      if (feature == wgpu::FeatureName::TimestampQuery) {
        requiredFeatures.push_back(feature);
      }
#endif
    }
    if (getenv("ORRE_GPU_FEATURES") != nullptr) { // OrrePort: every feature the adapter offers, by enum value
      std::string offered;
      for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
        offered += fmt::format("\n  0x{:X} {}", static_cast<uint32_t>(supportedFeatures.features[i]),
                               magic_enum::enum_name(supportedFeatures.features[i]));
      }
      Log.info("Adapter offers {} features:{}", supportedFeatures.featureCount, offered);
    }
    std::string featureList;
    for (auto featureName : requiredFeatures) {
      featureList += "\n  ";
      // OrrePort: magic_enum has no name for Dawn's native features (0x5xxxx, beyond its range), which printed an
      // empty line; name the one we request, and give any other unnamed one its number
      const auto name = magic_enum::enum_name(featureName);
#ifdef WEBGPU_DAWN
      if (featureName == wgpu::FeatureName::ImplicitDeviceSynchronization) {
        featureList += "ImplicitDeviceSynchronization";
        continue;
      }
#endif
      if (name.empty()) {
        featureList += fmt::format("0x{:X}", static_cast<uint32_t>(featureName));
      } else {
        featureList += name;
      }
    }
    Log.info("Enabling features: {}", featureList);
#ifdef WEBGPU_DAWN
    wgpu::DawnCacheDeviceDescriptor cacheDescriptor({.nextInChain = nullptr});
    cacheDescriptor.SetDawnLoadCacheDataCallback(
        [](std::span<const std::byte> key, std::span<std::byte> value) noexcept -> size_t {
          return load_from_cache(key.data(), key.size(), value.data(), value.size(), nullptr);
        });
    cacheDescriptor.SetDawnStoreCacheDataCallback(
        [](std::span<const std::byte> key, std::span<const std::byte> value) noexcept {
          store_to_cache(key.data(), key.size(), value.data(), value.size(), nullptr);
        });

    // OrrePort: ORRE_GPU_VALIDATE=1 keeps Dawn's validation and robustness on in a release build, and logs WebGPU
    // errors instead of stopping (the uncaptured-error callback below), to find what draws the 4K wedges (A14)
    const bool validate = getenv("ORRE_GPU_VALIDATE") != nullptr;
    // OrrePort (2026-11-11, port-gaps 76): ORRE_D3D12_FXC=1 compiles D3D12 shaders with FXC instead of DXC, to test
    // whether Intel's D3D12 text / box-screen artifacts come from the DXC-compiled shaders
    const bool fxc = getenv("ORRE_D3D12_FXC") != nullptr;
    if (fxc) {
      Log.info("ORRE_D3D12_FXC: Direct3D 12 shaders compiled with FXC (use_dxc off)");
    }
    std::vector<const char*> enableToggles{
#if _WIN32
#ifndef NDEBUG
        "emit_hlsl_debug_symbols",
#endif
#endif
#ifndef ANDROID
        "use_user_defined_labels_in_backend",
#endif
        "disable_symbol_renaming",
        "enable_immediate_error_handling",
        "gl_allow_context_on_multi_threads",
        "vulkan_monolithic_pipeline_cache", /* OrrePort: one pipeline cache per device (save_pipeline_cache says why) */
    };
#if _WIN32
    if (!fxc) {
      enableToggles.push_back("use_dxc");
    }
#endif
#ifdef NDEBUG
    if (!validate) {
      enableToggles.push_back("skip_validation");
      enableToggles.push_back("disable_robustness");
    }
#endif
    if (validate) {
      Log.info("ORRE_GPU_VALIDATE: Dawn validation on; WebGPU errors are logged, not fatal");
    }
    std::vector<const char*> disableToggles{
        "timestamp_quantization",
        // Adreno strikes again!
        // https://github.com/TwilitRealm/dusklight/issues/2563
        "use_spirv_reconvergence_mode",
    };
    if (fxc) {
      disableToggles.push_back("use_dxc");
    }
    wgpu::DawnTogglesDescriptor togglesDescriptor(wgpu::DawnTogglesDescriptor::Init{
        .nextInChain = &cacheDescriptor,
        .enabledToggleCount = enableToggles.size(),
        .enabledToggles = enableToggles.data(),
        .disabledToggleCount = disableToggles.size(),
        .disabledToggles = disableToggles.data(),
    });
#endif
    wgpu::DeviceDescriptor deviceDescriptor({
#ifdef WEBGPU_DAWN
        .nextInChain = &togglesDescriptor,
#endif
        .requiredFeatureCount = requiredFeatures.size(),
        .requiredFeatures = requiredFeatures.data(),
        .requiredLimits = &requiredLimits,
    });
    deviceDescriptor.SetUncapturedErrorCallback(
        [](const wgpu::Device& device, wgpu::ErrorType type, wgpu::StringView message) {
          static const bool s_logOnly = getenv("ORRE_GPU_VALIDATE") != nullptr;
          static int s_logged = 0;
          if (g_initialized && !s_logOnly) {
            FATAL("WebGPU error {}: {}", underlying(type), message);
          } else if (s_logged < 40) { // ORRE_GPU_VALIDATE: the first 40 errors of a run
            ++s_logged;
            Log.warn("WebGPU error {}: {}", underlying(type), message);
          }
        });
    deviceDescriptor.SetDeviceLostCallback(
        wgpu::CallbackMode::AllowSpontaneous,
        [](const wgpu::Device& device, wgpu::DeviceLostReason reason, wgpu::StringView message) {
          if (g_initialized) {
            FATAL("Device lost: {}", message);
          } else {
            Log.warn("Device lost: {}", message);
          }
        });
    const auto future =
        g_adapter.RequestDevice(&deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
                                  if (status == wgpu::RequestDeviceStatus::Success) {
                                    g_device = std::move(device);
                                  } else {
                                    Log.warn("Device request failed: {}", message);
                                  }
                                });
    const auto status = g_instance.WaitAny(future, 5000000000);
    if (status != wgpu::WaitStatus::Success) {
      Log.error("Failed to create device: {}", magic_enum::enum_name(status));
      return false;
    }
    if (!g_device) {
      return false;
    }
    g_device.SetLoggingCallback(wgpu_log);
  }
  g_queue = g_device.GetQueue();

  const wgpu::Status status = g_surface.GetCapabilities(g_adapter, &g_surfaceCapabilities);
  if (status != wgpu::Status::Success) {
    Log.error("Failed to get surface capabilities: {}", magic_enum::enum_name(status));
    return false;
  }
  if (g_surfaceCapabilities.formatCount == 0) {
    Log.error("Surface has no formats");
    return false;
  }
  if (g_surfaceCapabilities.presentModeCount == 0) {
    Log.error("Surface has no present modes");
    return false;
  }
  auto surfaceFormat = best_surface_format();
  g_vsyncEnabled.store(g_config.vsync, std::memory_order_release);
  auto presentMode = select_present_mode(g_surfaceCapabilities);
  Log.info("Using surface format {}, present mode {}", magic_enum::enum_name(surfaceFormat),
           magic_enum::enum_name(presentMode));
  const auto size = window::get_window_size();
  g_graphicsConfig = GraphicsConfig{
      .surfaceConfiguration =
          wgpu::SurfaceConfiguration{
              .format = surfaceFormat,
              .usage = wgpu::TextureUsage::RenderAttachment,
              .width = size.native_fb_width,
              .height = size.native_fb_height,
              .presentMode = presentMode,
          },
      .depthFormat = wgpu::TextureFormat::Depth32Float,
      .msaaSamples = g_config.msaa,
      .textureAnisotropy = g_config.maxTextureAnisotropy,
  };
  create_copy_pipeline();
  create_resample_pipeline();
  gpu_prof::initialize();
  resize_swapchain(size.fb_width, size.fb_height, size.native_fb_width, size.native_fb_height, true);
  g_initialized = true;
  return true;
}

/* OrrePort (2026-11-13; snatcherLeo: "is it supposed to take a second or 2 for our hub to open up?"): Vulkan with ONE
 * pipeline cache per device ("vulkan_monolithic_pipeline_cache", enabled with the toggles above). Why: by default
 * Dawn gives every pipeline its own VkPipelineCache, loaded from and saved to the blob cache (dawn_cache.db). On
 * NVIDIA each of those blobs held the driver's whole cache, about 1.6 MB, so every pipeline read 1.6 MB from SQLite,
 * unpacked it and handed it to the driver again (verified: a probe logged a 1.6 MB hit for each pipeline at start; the
 * start's warm-up then took about 19 ms a pipeline and Aurora's start about 0.5 s). With one cache per device, Dawn
 * loads it once when the device is made and saves it only when asked, which is this call
 * (dawn::native::PerformIdleTasks).
 * The prebuilt Dawn is built with MSVC and OrrePort with MinGW GCC, whose C++ names differ, so on Windows the function
 * is looked up by its MSVC name; it takes a const wgpu::Device&, a reference to one handle, which both compilers pass
 * the same way (a pointer in the first register; verified: starting with an empty cache, the call wrote the device's
 * cache, 2.4 MB, once the start's warm-up was done, and the next starts loaded it once and built the warm-up's
 * pipelines in a median 11 ms instead of 19; the hub's window at 0.71 s instead of 0.92).
 * Open (likely, from the probes): Dawn saves the cache only when it started empty; one loaded from the blob cache was
 * not saved again although new pipelines were built, so pipelines first built in later runs stay out of it until the
 * cache is cleared. The driver's own disk cache likely still covers them (guess: not measured).
 * Platforms: the toggle matters only for the Vulkan backend (Windows and Linux; Direct3D 12 and Metal ignore it).
 * Linux and macOS builds of Dawn use the same compiler as OrrePort there, so they call it directly (likely). */
void save_pipeline_cache() noexcept {
#ifdef WEBGPU_DAWN
  if (!g_device) {
    return;
  }
#if defined(_WIN32) && defined(__MINGW32__)
  using IdleFn = void (*)(const wgpu::Device*);
  static const IdleFn idle = [] {
    SDL_SharedObject* dawn = SDL_LoadObject("webgpu_dawn.dll"); /* already loaded: this only finds it */
    return dawn != nullptr ? reinterpret_cast<IdleFn>(
                                 SDL_LoadFunction(dawn, "?PerformIdleTasks@native@dawn@@YAXAEBVDevice@wgpu@@@Z"))
                           : nullptr;
  }();
  if (idle != nullptr) {
    idle(&g_device);
  }
#else
  dawn::native::PerformIdleTasks(g_device);
#endif
#endif
}

void shutdown() {
  g_initialized = false;
  gfx::gpu_synchronize();
  save_pipeline_cache(); /* OrrePort: what was compiled this run, for the next start */
  gpu_prof::shutdown();
  g_CopyBindGroupLayout = {};
  g_CopyPipeline = {};
  g_CopyPremultipliedAlphaPipeline = {};
  g_CopyBindGroup = {};
  g_ResampleBindGroupLayout = {};
  g_ResamplePipeline = {};
  g_ResampleUniformBuffer = {};
  g_resampledFrameBuffer = {};
  g_frameBuffer = {};
  g_frameBufferResolved = {};
  g_depthBuffer = {};
  g_normalBuffer = {};
  g_graphicsConfig.normalBuffer = false;
  g_queue = {};
  g_surface = {};
  g_device = {};
  g_adapter = {};
  g_instance = {};
  cache_shutdown();
}

void release_surface() noexcept {
  /* OrrePort (2026-11-20): the presenter paused first, so no in-between picture uses the surface while it goes
   * (render_worker.hpp, IdleHookPause) */
  const gfx::render_worker::IdleHookPause presenterPaused;
  gfx::gpu_synchronize();
  {
    window::SurfaceLock surfaceLock;
    release_surface_locked();
  }
}

// OrrePort: new frame buffers start black. Vulkan does not zero new images (D3D12 usually does), so a frame that shows
// the frame buffer before anything cleared it, or one that keeps the EFB, showed leftover GPU memory: coloured noise,
// yellow dashes and a blue band right after launch on snatcherLeo's first Vulkan run (2026-10-18). Called with the render
// worker idle (resize_swapchain / refresh_surface synchronize first).
static void clear_new_frame_buffers() {
  const auto clear_one = [](const wgpu::TextureView& view, const wgpu::TextureView& resolve) {
    if (!view) {
      return;
    }
    const std::array attachments{
        wgpu::RenderPassColorAttachment{
            .view = view,
            .resolveTarget = resolve,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {0.0, 0.0, 0.0, 1.0},
        },
    };
    const wgpu::RenderPassDescriptor desc{
        .label = "Frame buffer initial clear",
        .colorAttachmentCount = attachments.size(),
        .colorAttachments = attachments.data(),
    };
    const wgpu::CommandEncoderDescriptor encoderDesc{.label = "Frame buffer initial clear"};
    auto encoder = g_device.CreateCommandEncoder(&encoderDesc);
    encoder.BeginRenderPass(&desc).End();
    const auto buffer = encoder.Finish();
    g_queue.Submit(1, &buffer);
  };
  const bool msaa = g_graphicsConfig.msaaSamples > 1;
  clear_one(g_frameBuffer.view, msaa ? g_frameBufferResolved.view : wgpu::TextureView{});
  if (!msaa) {
    clear_one(g_frameBufferResolved.view, {});
  }
}

/* OrrePort (2026-11-24, port-gaps 90): the resize workarounds under test (aurora_set_resize_workaround, aurora.h).
 * snatcherLeo's laptop (Intel Iris Xe, Vulkan) deadlocked inside Intel's driver after a restore from maximized: the render
 * worker in Instance::ProcessEvents waits on a driver thread that waits on a lock. Each bit changes the resize in one
 * way, so one test build can try them all on the laptop; the one that works becomes the fix (for Intel's Vulkan
 * driver only, or for everyone if it costs nothing), the others go. Windows' Intel driver only, as far as is known
 * (likely); Linux's Mesa and macOS's Metal take other paths. */
static std::atomic_uint32_t g_resizeWorkaround = 0;

void set_resize_workaround(uint32_t bits) noexcept {
  g_resizeWorkaround.store(bits, std::memory_order_relaxed);
  if (bits != 0) {
    Log.info("Resize workaround (port-gaps 90 test): {}{}{}", (bits & 1) != 0 ? "wait for the GPU; " : "",
             (bits & 2) != 0 ? "(2, a new surface: dropped, ignored) " : "", (bits & 4) != 0 ? "hold 250 ms" : "");
  }
}

/* bit 1: waits (up to 2 s) until the GPU has finished everything submitted so far. Called on the main thread with the
 * render worker idle (resize_swapchain synchronizes first), so nothing else is in Dawn while it waits. */
static void wait_gpu_idle(const char* when) {
  bool done = false;
  const auto future = g_queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,
                                                  [&](wgpu::QueueWorkDoneStatus, wgpu::StringView) { done = true; });
  const auto status = g_instance.WaitAny(future, 2000000000);
  Log.info("Resize workaround: GPU idle {} ({})", when,
           status == wgpu::WaitStatus::Success && done ? "done" : magic_enum::enum_name(status));
}

static void resize_swapchain_internal(uint32_t width, uint32_t height, uint32_t nativeWidth, uint32_t nativeHeight,
                                      bool force) {
  if (!g_surface || !g_device || width == 0 || height == 0 || nativeHeight == 0 || nativeWidth == 0) {
    return;
  }
  if (window::is_minimized()) {
    return; // OrrePort (port-gaps 77): no swap chain changes while minimized; the restore resizes again
  }
  const bool sizeChanged = g_graphicsConfig.surfaceConfiguration.width != nativeWidth ||
                           g_graphicsConfig.surfaceConfiguration.height != nativeHeight ||
                           g_frameBuffer.size.width != width || g_frameBuffer.size.height != height;
  if (!force && !sizeChanged) {
    return;
  }
  // OrrePort: each re-creation of the frame buffers, for A14 (the D3D12 wedges in auto render size, likely after a
  // resize: minimize / restore / fullscreen)
  Log.info("Frame buffers {}x{} -> {}x{} (surface {}x{}){}", g_frameBuffer.size.width, g_frameBuffer.size.height, width,
           height, nativeWidth, nativeHeight, force ? " (forced)" : "");
  g_graphicsConfig.surfaceConfiguration.width = nativeWidth;
  g_graphicsConfig.surfaceConfiguration.height = nativeHeight;
  auto surfaceConfiguration = g_graphicsConfig.surfaceConfiguration;
  surfaceConfiguration.device = g_device;
  const uint32_t workaround = g_resizeWorkaround.load(std::memory_order_relaxed); /* port-gaps 90 (above) */
  if ((workaround & 1) != 0) {
    wait_gpu_idle("before the change");
  }
  /* bit 2 (a new surface instead of reconfiguring) was tried and dropped the same night: on the RTX every try lost the
   * device with VK_ERROR_NATIVE_WINDOW_IN_USE_KHR (the old swap chain still held the window), even after Unconfigure,
   * releasing the old surface, a GPU-idle wait and a device tick. Something else still holds the old surface (likely a
   * surface texture kept for a later present); not chased further while bits 1 and 4 are untested on the laptop. */
  {
    window::SurfaceLock surfaceLock;
    g_surface.Configure(&surfaceConfiguration);
  }
  g_frameBuffer = create_render_texture(width, height, true);
  g_frameBufferResolved = create_render_texture(width, height, false);
  g_depthBuffer = create_depth_texture(width, height);
  if (g_graphicsConfig.normalBuffer) {
    g_normalBuffer = create_normal_texture(width, height);
  }
  ++g_frameBufferGeneration;
  clear_new_frame_buffers();
  g_CopyBindGroup = create_copy_bind_group(present_source());
  if ((workaround & 1) != 0) {
    wait_gpu_idle("after the change"); /* the clears above included */
  }
  if ((workaround & 4) != 0) {
    /* bit 4: nothing new reaches the worker or the driver for 250 ms (the main thread is here, the presenter paused),
     * time for the driver's own threads to finish with the old swap chain (a guess at the race) */
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
  }
}

bool refresh_surface(bool recreate) {
  /* OrrePort (2026-11-20): the presenter paused for the whole surface and frame-buffer change (IdleHookPause) */
  const gfx::render_worker::IdleHookPause presenterPaused;
  gfx::gpu_synchronize();
  if (!g_instance || !g_device) {
    return false;
  }
  if (!window::is_presentable()) {
    {
      window::SurfaceLock surfaceLock;
      release_surface_locked();
    }
    return false;
  }
  if ((!g_surface || recreate) && !create_surface()) {
    return false;
  }
  // OrrePort (port-gaps 77): a refresh while minimized (a vsync change, a suboptimal surface) keeps the swap chain as
  // it is: Intel's Vulkan driver refuses one for a window with no visible size, and Dawn then loses the device
  if (window::is_minimized()) {
    return true;
  }
  uint32_t width = g_graphicsConfig.surfaceConfiguration.width;
  uint32_t height = g_graphicsConfig.surfaceConfiguration.height;
  uint32_t nativeWidth = width;
  uint32_t nativeHeight = height;
  if (window::get_sdl_window() != nullptr) {
    const auto size = window::get_window_size();
    width = size.fb_width;
    height = size.fb_height;
    nativeWidth = size.native_fb_width;
    nativeHeight = size.native_fb_height;
  }
  if (width != 0 && height != 0) {
    resize_swapchain_internal(width, height, nativeWidth, nativeHeight, true);
  }
  return true;
}

void resize_swapchain(uint32_t width, uint32_t height, uint32_t nativeWidth, uint32_t nativeHeight, bool force) {
  /* OrrePort (2026-11-20, a start-up crash on snatcherLeo's PC): the presenter (frame smoothing's in-between pictures, run by the
   * worker between queue items) paused for the whole change. Before, synchronize() let the worker present again at once,
   * and an in-between picture was submitted while the surface was configured and the frame buffers replaced: NVIDIA's
   * Vulkan driver crashed inside Queue::Submit (render_worker.hpp, IdleHookPause). Every OS. */
  const gfx::render_worker::IdleHookPause presenterPaused;
  gfx::gpu_synchronize();
  resize_swapchain_internal(width, height, nativeWidth, nativeHeight, force);
}
} // namespace aurora::webgpu

void aurora_enable_vsync(const bool enabled) {
  aurora::webgpu::g_vsyncEnabled.store(enabled, std::memory_order_release);
  aurora::webgpu::g_graphicsConfig.surfaceConfiguration.presentMode =
      aurora::webgpu::select_present_mode(aurora::webgpu::g_surfaceCapabilities);
  /* OrrePort (2026-11-13): say which mode a live V-Sync change picks; the start-up line was the only record, so a
   * test couldn't tell from the log whether the Settings toggle took effect (port-gaps 78) */
  aurora::webgpu::Log.info("V-Sync {}: present mode {}", enabled ? "on" : "off",
                           magic_enum::enum_name(aurora::webgpu::g_graphicsConfig.surfaceConfiguration.presentMode));
  aurora::window::push_custom_event(aurora::window::CustomEvent::RefreshSurface);
}
