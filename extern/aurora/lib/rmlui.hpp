#pragma once

#include "aurora/aurora.h"
#include "webgpu/gpu.hpp"

#include <SDL3/SDL_events.h>
#include <aurora/rmlui.hpp>
#include <dawn/webgpu_cpp.h>

namespace aurora::rmlui {

struct RecordedFrame {
  wgpu::BindGroup bindGroup;
  bool overlay = false;
  // OrrePort (2026-11-19, frame smoothing section 33): the texture the frame was drawn into (what bindGroup samples),
  // so the smoother's dump test switch can save the UI picture of each in-between (an in-between redraws it from its
  // own game picture). Look only; platform-neutral.
  webgpu::TextureWithSampler target;
};

void initialize(const AuroraWindowSize& size) noexcept;
void handle_window_event(const SDL_Event& event) noexcept;
RecordedFrame record_frame(const webgpu::Viewport& presentViewport) noexcept;
void shutdown() noexcept;

} // namespace aurora::rmlui
