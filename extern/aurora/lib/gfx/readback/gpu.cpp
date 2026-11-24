#include "gpu.hpp"

#include "../frame_packet.hpp"
#include "../../webgpu/gpu.hpp"
#include "../../webgpu/gpu_prof.hpp"

#include <algorithm>
#include <chrono> /* OrrePort, 2026-11-18: park_buffer's times */
#include <mutex>
#include <vector>

#include <magic_enum.hpp>
#include <tracy/Tracy.hpp>

namespace aurora::gfx::readback::gpu {
namespace {
constexpr Module Log{"aurora::gfx::readback"};
constexpr uint64_t PoolLimit = 16ull << 20;

std::mutex g_poolMutex;
std::vector<std::pair<uint64_t, wgpu::Buffer>> g_freeBuffers;
uint64_t g_freeBytes = 0;

// OrrePort (2026-11-18, the exe53 start-up crash): buffers the map callback must let go of (the pool is full, or the
// mapping was Aborted / failed) are parked here and released later from the render worker, never inside the callback.
// Why: our pinned Dawn (encounter/dawn v20260807.225922, commit 1155e0ed) has a use-after-free in
// Buffer::MapAsyncEvent::Complete: it locks the buffer's mPendingMapMutex, holds its own strong reference, runs our
// callback, then drops the reference BEFORE unlocking. If our lambda held the buffer's only other reference (it is
// destroyed inside the callback), the buffer is freed and the unlock writes into freed memory: an access violation in
// MSVCP140!_Mtx_unlock under wgpu::Instance::ProcessEvents when the page went back to Windows (seen with Aurora's
// frame dumps, now pooled in aurora.cpp orre_dump_acquire: 2 crashes in 20 short boots, verified; this pool's over-limit
// path has the same shape, likely). Fixed upstream in google/dawn addd9b41de06 (2026-08-27, crbug.com/552762484);
// remove the parking (drop in place again) once our Dawn includes that commit.
// The release waits at least a second after parking, so a Complete still finishing on another thread (a spontaneous
// callback) is long done (likely: it returns microseconds after the callback). All OSes (WebGPU only).
std::vector<std::pair<std::chrono::steady_clock::time_point, wgpu::Buffer>> g_parkedBuffers; // under g_poolMutex

void park_buffer(wgpu::Buffer buffer) noexcept {
  if (!buffer) {
    return;
  }
  std::lock_guard lock{g_poolMutex};
  g_parkedBuffers.emplace_back(std::chrono::steady_clock::now(), std::move(buffer));
}

// Render worker (acquire_buffer, while encoding), outside any map callback
void release_parked_buffers() {
  std::vector<wgpu::Buffer> letGo; // destroyed after the lock is released
  {
    std::lock_guard lock{g_poolMutex};
    const auto now = std::chrono::steady_clock::now();
    for (auto it = g_parkedBuffers.begin(); it != g_parkedBuffers.end();) {
      if (now - it->first > std::chrono::seconds{1}) {
        letGo.push_back(std::move(it->second));
        it = g_parkedBuffers.erase(it);
      } else {
        ++it;
      }
    }
  }
}

// Render worker only
wgpu::Buffer g_scratch;
uint64_t g_scratchBytes = 0;

wgpu::Buffer acquire_buffer(uint64_t bytes) {
  release_parked_buffers(); // OrrePort, 2026-11-18 (the Dawn bug above)
  {
    std::lock_guard lock{g_poolMutex};
    const auto it = std::ranges::find(g_freeBuffers, bytes, &std::pair<uint64_t, wgpu::Buffer>::first);
    if (it != g_freeBuffers.end()) {
      auto buffer = std::move(it->second);
      g_freeBytes -= bytes;
      g_freeBuffers.erase(it);
      return buffer;
    }
  }
  const wgpu::BufferDescriptor descriptor{
      .label = "Readback Staging Buffer",
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = bytes,
  };
  return webgpu::g_device.CreateBuffer(&descriptor);
}

const wgpu::Buffer& ensure_scratch(uint64_t bytes) {
  if (g_scratchBytes < bytes) {
    g_scratchBytes = staging_bucket(bytes);
    const wgpu::BufferDescriptor descriptor{
        .label = "Readback Pack Buffer",
        .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc,
        .size = g_scratchBytes,
    };
    g_scratch = webgpu::g_device.CreateBuffer(&descriptor);
  }
  return g_scratch;
}

void on_mapped(Job& job, wgpu::Buffer buffer, wgpu::MapAsyncStatus status, wgpu::StringView message) {
  if (status != wgpu::MapAsyncStatus::Success) {
    fail_job(job, fmt::format("buffer mapping {}: {}", magic_enum::enum_name(status), message));
    // OrrePort, 2026-11-18: Aborted and Error run with Dawn's buffer lock held (park_buffer, the Dawn bug); only
    // CallbackCancelled (Dawn's shutdown completion, no lock taken) may drop it here
    if (status != wgpu::MapAsyncStatus::CallbackCancelled) {
      park_buffer(std::move(buffer));
    }
    return;
  }
  const auto* data = static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, job.outputBytes));
  if (data != nullptr) {
    complete_job(job, {data, job.outputBytes});
  } else {
    fail_job(job, "mapped range unavailable");
  }
  buffer.Unmap();
  release_buffer(std::move(buffer), job.bufferBytes);
}

void map_job(const std::shared_ptr<Job>& job) {
  auto buffer = std::move(job->buffer);
  buffer.MapAsync(wgpu::MapMode::Read, 0, job->outputBytes, wgpu::CallbackMode::AllowSpontaneous,
                  [job, buffer](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                    on_mapped(*job, buffer, status, message);
                  });
}
} // namespace

void initialize() {
  if (!webgpu::g_hasCoreFeatures) {
    Log.info("Asynchronous readback is unavailable without core WebGPU features");
    return;
  }
  if (webgpu::g_graphicsConfig.msaaSamples > 1) {
    Log.info("Asynchronous readback is unavailable with MSAA");
    return;
  }
  initialize_efb_gather();
  initialize_copy_pack();
  set_supported(true);
}

void shutdown() {
  if (supported()) {
    set_supported(false);
    shutdown_efb_gather();
    shutdown_copy_pack();
  }
  g_scratch = {};
  g_scratchBytes = 0;
  std::lock_guard lock{g_poolMutex};
  g_freeBuffers.clear();
  g_freeBytes = 0;
  g_parkedBuffers.clear(); // OrrePort, 2026-11-18 (park_buffer): the render worker is synchronized by now
}

void release_buffer(wgpu::Buffer buffer, uint64_t bytes) noexcept {
  if (!buffer || !supported()) {
    return;
  }
  std::lock_guard lock{g_poolMutex};
  if (g_freeBytes + bytes > PoolLimit) {
    // OrrePort, 2026-11-18: parked, not dropped: this runs inside the map callback (on_mapped), where dropping the
    // last reference trips the Dawn use-after-free (park_buffer). g_poolMutex is already held here.
    g_parkedBuffers.emplace_back(std::chrono::steady_clock::now(), std::move(buffer));
    return;
  }
  g_freeBytes += bytes;
  g_freeBuffers.emplace_back(bytes, std::move(buffer));
}

void encode_job(const wgpu::CommandEncoder& cmd, detail::FramePacket& frame, const std::shared_ptr<Job>& job) {
  ZoneScoped;
  if (!supported()) {
    fail_job(*job, "readback GPU resources are not initialized");
    return;
  }
  const auto& scratch = ensure_scratch(job->outputBytes);
  job->bufferBytes = staging_bucket(job->outputBytes);
  job->buffer = acquire_buffer(job->bufferBytes);
  switch (job->kind) {
  case JobKind::Efb:
    encode_efb_gather(cmd, *job, scratch);
    break;
  case JobKind::Copy:
    encode_copy_pack(cmd, *job, scratch);
    break;
  }
  {
    const webgpu::gpu_prof::Zone zone{cmd, "Readback transfer"};
    cmd.CopyBufferToBuffer(scratch, 0, job->buffer, 0, job->outputBytes);
  }
  frame.afterSubmitCallbacks.emplace_back([job] { map_job(job); });
}

} // namespace aurora::gfx::readback::gpu
