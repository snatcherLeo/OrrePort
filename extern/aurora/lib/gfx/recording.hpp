#pragma once

#include "types.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace aurora::gfx::detail {

struct FramePacket;

struct RecordedFrame {
  FramePacket* packet;
  size_t frameSlot;
};

void begin_recording(FramePacket& packet, size_t frameSlot);
// OrrePort: the next frame's first EFB pass loads the EFB (colour and depth) instead of clearing it
void keep_efb_next_frame() noexcept;
RecordedFrame end_recording();
void shutdown_recording();
void increment_merged_draw_count() noexcept;

namespace testing {
struct OffscreenCacheStats {
  uint64_t bytes;
  size_t entries;
};
void suppress_render_worker(bool suppress) noexcept;
void seed_offscreen_cache(uint32_t width, uint32_t height, wgpu::TextureFormat colorFormat,
                          wgpu::TextureFormat depthFormat);
OffscreenCacheStats offscreen_cache_stats() noexcept;
bool is_offscreen_cached(uint32_t width, uint32_t height, wgpu::TextureFormat colorFormat,
                         wgpu::TextureFormat depthFormat);
} // namespace testing

} // namespace aurora::gfx::detail

namespace aurora::gfx::readback {
struct Job;
}

namespace aurora::gfx {
struct ColorPassDescriptor {
  const char* label = nullptr;
  wgpu::TextureView colorView;
  wgpu::TextureView resolveView;
  wgpu::TextureFormat colorFormat = wgpu::TextureFormat::Undefined;
  wgpu::TextureView depthStencilView;
  wgpu::TextureFormat depthStencilFormat = wgpu::TextureFormat::Undefined;
  wgpu::Extent3D targetSize;
  uint32_t sampleCount = 1;
  wgpu::LoadOp colorLoadOp = wgpu::LoadOp::Clear;
  wgpu::StoreOp colorStoreOp = wgpu::StoreOp::Store;
  wgpu::Color clearColor{0.f, 0.f, 0.f, 0.f};
  bool hasDepth = false;
  wgpu::LoadOp depthLoadOp = wgpu::LoadOp::Undefined;
  wgpu::StoreOp depthStoreOp = wgpu::StoreOp::Undefined;
  float depthClearValue = 0.f;
  bool hasStencil = false;
  wgpu::LoadOp stencilLoadOp = wgpu::LoadOp::Undefined;
  wgpu::StoreOp stencilStoreOp = wgpu::StoreOp::Undefined;
  uint32_t stencilClearValue = 0;
};

void finish();
void begin_color_pass(const ColorPassDescriptor& desc);
void end_color_pass();
void queue_texture_copy(wgpu::TexelCopyTextureInfo src, wgpu::TexelCopyTextureInfo dst, wgpu::Extent3D size);
void begin_offscreen(uint32_t width, uint32_t height);
void end_offscreen();
bool has_normal_attachment() noexcept;
RenderTargetLayout get_render_target_layout() noexcept;
namespace tex_palette_conv {
struct ConvRequest;
}
void queue_palette_conv(tex_palette_conv::ConvRequest req);

Range push_verts(const uint8_t* data, size_t length, size_t alignment);
template <typename T>
Range push_verts(ArrayRef<T> data, size_t alignment) {
  return push_verts(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T), alignment);
}
Range push_indices(const uint8_t* data, size_t length, size_t alignment);
template <typename T>
Range push_indices(ArrayRef<T> data, size_t alignment) {
  return push_indices(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T), alignment);
}
Range push_uniform(const uint8_t* data, size_t length);
template <typename T>
Range push_uniform(const T& data) {
  return push_uniform(reinterpret_cast<const uint8_t*>(&data), sizeof(T));
}
Range push_storage(const uint8_t* data, size_t length);
template <typename T>
Range push_storage(ArrayRef<T> data) {
  return push_storage(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T));
}
template <typename T>
Range push_storage(const T& data) {
  return push_storage(reinterpret_cast<const uint8_t*>(&data), sizeof(T));
}
Range push_texture_data(const uint8_t* data, uint32_t bytesPerRow, uint32_t rowsPerImage);

template <typename DrawData>
void push_draw_command(DrawData data);
template <typename DrawData>
DrawData* get_last_draw_command();
template <typename PipelineConfig>
PipelineRef pipeline_ref(const PipelineConfig& config);

void resolve_pass_into(TextureHandle texture, ClipRect rect, bool clearColor, bool clearAlpha, bool clearDepth,
                       Vec4<float> clearColorValue, float clearDepthValue, GXTexFmt resolveFormat,
                       GXPixelFmt sourceFormat);
uint32_t align_uniform(uint32_t value);
Vec2<uint32_t> get_render_target_size() noexcept;
void set_viewport(const Viewport& viewport) noexcept;
void set_scissor(const ClipRect& scissor) noexcept;
void push_debug_group(std::string label);
void insert_debug_marker(std::string label);

struct ReadbackCapture {
  bool active = false;
  bool offscreen = false;
  uint32_t width = 0;
  uint32_t height = 0;
};
ReadbackCapture readback_capture() noexcept;
bool push_readback_op(std::shared_ptr<readback::Job> job, bool efbSource);

// OrrePort, frame smoothing step (c), the camera blend (frame_packet.hpp, OrreSmoothCamera; 2026-11-16). FIFO thread,
// only while steps are kept. A camera the game set (the GX_AURORA_ORRE_CAMERA marker), and each uniform block built
// after it: blocks built under a perspective screen camera are copied so an in-between picture can re-aim their draws.
// `view` is 3x4, `interest` 3 floats or null. Platform-neutral.
void orre_smooth_camera(uint64_t id, int32_t renderPass, const float* view, const float* interest);
// `postMtxOffset` (item b, 2026-11-17): where the block's post-transform texture matrices start (0: it has none)
// `color` (the colour-data blend, 2026-11-18): where the block's colour floats run (shader_info.cpp, fill_uniform)
struct OrreSmoothColorSpan {
  uint32_t at = 0, end = 0; // [at, end): TEV colour registers, lights and masks, channel colours, konstant colours
  uint32_t lightAt = 0;     // the eight lights (80 bytes each) and then four u32 light masks; 0: none
  uint32_t layout = 0;      // which registers, channels and konstant colours were written (fill_uniform)
  uint32_t indAt = 0;       // the warp blend (2026-11-18): where the indirect matrices start (3 x 32 bytes); 0: none
};
void orre_smooth_uniform(Range range, const uint8_t* data, size_t length, uint32_t uniformSize, uint32_t projOffset,
                         uint32_t postMtxOffset, const OrreSmoothColorSpan& color);
// Step (d), the model blend (2026-11-17): the HSD model piece the following draws belong to (the GX_AURORA_ORRE_MODEL
// marker: the current JObj and PObj from HSD_PObjDisp; 0 / 0 when the piece is done). Blocks built in between carry
// the pair, so the next step's block of the same piece can be found and its matrices blended. *Extended 2026-11-17
// (round 4):* the model's root JObj and the mask of matrix slots the piece uses (GXAurora.h).
void orre_smooth_model(uint64_t jobj, uint64_t pobj, uint64_t root, uint32_t slots);
// Item (a), the particle blend (2026-11-17): the particle the following draws belong to (the GX_AURORA_ORRE_PARTICLE
// marker; 0 when it is done), and each of its draws' vertices (`plain`: the position is the vertex's first attribute,
// three big-endian floats, under GX_PNMTX0). The blocks built for it carry the particle and, once it is done, the middle
// of its drawn corners.
void orre_smooth_particle(uint64_t particle, uint32_t guard, uint32_t life);
void orre_smooth_particle_vertices(const uint8_t* data, uint32_t vtxCount, uint32_t vtxSize, bool plain);
// Round 11, 2D at 120 (2026-11-17): a named 2D piece the following draws belong to (the GX_AURORA_ORRE_2D marker; kind
// 0 when it is done), whether one is being drawn now (a cheap check for draw_prim), and each of its draws' left and
// right x (the piece's own space; `ok` false: not readable). Its blocks carry the piece, and once it is done, its x span.
void orre_smooth_2d(uint32_t kind, uint32_t variant, uint32_t id, float value);
bool orre_smooth_2d_active() noexcept;
void orre_smooth_2d_vertices(float minX, float maxX, bool ok);
// vertex mode (the kind carries GX_AURORA_ORRE_2D_VERTICES: the pause menu's buttons and darkening): whether the piece
// being drawn now is one, and each of its draws' vertices as pushed (`layout`: frame_packet.hpp), kept for the blend
namespace detail {
struct OrreSmooth2DLayout;
}
bool orre_smooth_2d_vertex_mode() noexcept;
void orre_smooth_2d_chunk(const uint8_t* data, uint32_t vtxOffset, uint32_t length,
                          const detail::OrreSmooth2DLayout& layout);
// (3b), the particle corners (2026-11-18): whether a particle is being drawn, and each of its draws' vertices as pushed
bool orre_smooth_particle_active() noexcept;
void orre_smooth_particle_chunk(const uint8_t* data, uint32_t vtxOffset, uint32_t length,
                                const detail::OrreSmooth2DLayout& layout);
// Fix (3)'s draw log (2026-11-18; a test switch, ORRE_SMOOTH_DRAWLOG=<from>-<to>[,...], retrace counts): whether draws
// are logged now, and one line per draw (draw_prim) with what the smoother knows about it (its tags) and what the GX
// state says (primitive, vertices, matrix, texture), so a draw that changes between steps without blending can be
// traced to the game code that makes it. Observation only; never changes a picture. Platform-neutral.
bool orre_draw_log_on() noexcept;
void orre_draw_log(const char* what);
void orre_draw_log_stamp(uint32_t retrace) noexcept; // aurora_smooth_step: the step's retrace stamp
} // namespace aurora::gfx
