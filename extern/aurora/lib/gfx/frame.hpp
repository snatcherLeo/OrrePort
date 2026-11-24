#pragma once

#include "resources.hpp"
#include "frame_packet.hpp"

#include <optional>

namespace aurora::gfx::detail {

inline constexpr size_t FrameSlotCount = 2;
inline constexpr size_t StagingBufferCount = FrameSlotCount + 3;
inline constexpr uint64_t StagingBufferSize = UniformBufferSize + VertexBufferSize + IndexBufferSize +
                                              StorageBufferSize + (UseTextureBuffer ? TextureUploadSize : 0);

const wgpu::Buffer& staging_buffer(size_t slot);

struct RegisteredDrawType {
  DrawCallback draw = nullptr;
  void* userdata = nullptr;
};

struct RegisteredEncoderTaskType {
  EncoderTaskCallback callback = nullptr;
  void* userdata = nullptr;
  EncoderTaskCompletionCallback afterSubmit = nullptr;
};

std::optional<RegisteredDrawType> find_runtime_draw_type(DrawTypeId id);
std::optional<RegisteredEncoderTaskType> find_runtime_encoder_task_type(EncoderTaskId id);

} // namespace aurora::gfx::detail

namespace aurora::gfx {
void initialize();
void shutdown();
bool begin_frame();
void end_frame(EndFrameCallback callback);
uint32_t current_frame() noexcept;
void gpu_synchronize();
void after_present() noexcept;
float calculate_fps() noexcept;

// OrrePort, frame smoothing steps (a) and (b) (docs/port/frame-smoothing-plan.md; 2026-11-16). While on, each finished
// step's recording (its passes and their draws) is kept on the render worker after it is submitted, instead of being
// thrown away, so in-between pictures can draw it again (encoding.cpp, encode_replay). Off (the default): nothing is
// kept, as before. Platform-neutral.
void set_keep_steps(bool keep) noexcept; // any thread
bool keep_steps() noexcept;              // any thread
enum class KeptStep : uint8_t {
  None,       // nothing kept (smoothing just turned on)
  Replayable, // the newest step can be drawn again
  KeptEfb,    // it began from the previous step's EFB, which its own run has overwritten: repeat the picture
  Resized,    // the frame buffers were re-created since it was drawn: repeat the picture
  Stale,      // too many frames ago: its bind groups may have left Aurora's cache (resource_cache.cpp keeps 32)
};
KeptStep kept_step_state() noexcept;          // render worker only
uint32_t encode_kept_step(wgpu::CommandEncoder& encoder); // render worker only; returns the draws recorded again
// Step (c), the camera blend (smooth_camera.hpp; 2026-11-16). Render worker only.
// After a step is kept: match its cameras with the step before's and decide which blend (history false: none)
void plan_kept_step(bool history, uint32_t retrace);
// The kept step drawn again with its cameras `fraction` of the way from the step before's (0) to its own (1);
// `reaimed`: uniform blocks re-aimed (0: nothing moved, the replay is the plain one). With nothing to re-aim it draws
// only when `always` (the test mode) or the frame buffer holds a blended picture; `drew` says whether it drew.
// Returns the draws.
uint32_t encode_kept_step_blended(wgpu::CommandEncoder& encoder, float fraction, uint32_t& reaimed, bool always,
                                  bool& drew);
// whether the frame buffer holds the kept step's own picture (false after a blended replay, until the restore that
// runs before the next step's GPU work)
bool kept_step_exact() noexcept;
uint64_t kept_step_restores() noexcept; // restores so far (any thread)
// round 4 (2026-11-17): capture-target copies so far (saved before a kept step, put back before each replay; any
// thread). frame.cpp, "Captures that cross steps", says why
uint64_t capture_copies() noexcept;
} // namespace aurora::gfx
