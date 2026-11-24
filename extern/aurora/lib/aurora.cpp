#include <aurora/aurora.h>
#include <aurora/time.hpp>

#ifdef AURORA_ENABLE_GX
#include "gfx/resources.hpp"
#include "gfx/frame.hpp"
#include "gfx/recording.hpp"
#include "gfx/render_worker.hpp"
#include "gfx/smooth_camera.hpp" /* OrrePort, frame smoothing step (c): the camera blend's counters and cuts */
#include "gx/command_processor.hpp"
#include "gx/fifo.hpp"
#include "gx/gx.hpp"
#include "gx/texture.hpp"
#include "imgui.hpp"
#include "webgpu/gpu.hpp"
#include "webgpu/gpu_prof.hpp"
#include <webgpu/webgpu_cpp.h>
#endif

#ifdef AURORA_ENABLE_RMLUI
#include "rmlui.hpp"
#endif

#include "gamepad.hpp"
#include "internal.hpp"
#include "thread.hpp"
#include "window.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_timer.h> /* OrrePort, frame smoothing: the presenter's clock (SDL_GetTicksNS, SDL_DelayPrecise) */
#include <magic_enum.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>  /* OrrePort, 2026-11-18: the frame dump pool's return times */
#include <cstdio>  /* OrrePort: the frame dump's PPM files */
#include <cstdlib> /* OrrePort, frame smoothing: its test switches (getenv) */
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "system_info.hpp"
#include "tracy/Tracy.hpp"

namespace aurora {
AuroraConfig g_config;
uint32_t g_sdlCustomEventsStart;
char g_gameName[4];

namespace {
constexpr Module Log{"aurora"};

#ifdef AURORA_ENABLE_GX
// GPU
using webgpu::g_device;
using webgpu::g_queue;
using webgpu::g_surface;

uint32_t clamp_scissor_coord(double value, uint32_t maximum) noexcept {
  if (!std::isfinite(value)) {
    return 0;
  }
  return static_cast<uint32_t>(std::clamp(value, 0.0, static_cast<double>(maximum)));
}

void set_present_viewport(const wgpu::RenderPassEncoder& pass, const gfx::Viewport& viewport, uint32_t surfaceWidth,
                          uint32_t surfaceHeight) noexcept {
  pass.SetViewport(viewport.left, viewport.top, viewport.width, viewport.height, viewport.znear, viewport.zfar);
  const auto scissorX = clamp_scissor_coord(std::floor(viewport.left), surfaceWidth);
  const auto scissorY = clamp_scissor_coord(std::floor(viewport.top), surfaceHeight);
  const auto scissorRight = clamp_scissor_coord(std::ceil(viewport.left + viewport.width), surfaceWidth);
  const auto scissorBottom = clamp_scissor_coord(std::ceil(viewport.top + viewport.height), surfaceHeight);
  pass.SetScissorRect(scissorX, scissorY, scissorRight - scissorX, scissorBottom - scissorY);
}

// OrrePort (the project's build helper, 2026-11-16; aurora_orre_request_frame_dump, a test switch): the full-resolution game
// picture of one frame, for measuring sharpness (the text-softness question: text vs the box frame at 1080p / 1440p).
// Why here: the frame buffer and the resampled copy are Aurora's own textures; the port's GX frame dumps only see a
// 640x480 EFB copy. The copy is queued on the frame's encoder and the file written when the buffer maps (any thread).
// Platform-neutral (WebGPU and stdio only).
std::mutex g_orreDumpLock;
std::string g_orreDumpPrefix;

std::string orre_take_dump_request() {
  std::lock_guard lock{g_orreDumpLock};
  return std::exchange(g_orreDumpPrefix, std::string{});
}

// OrrePort (2026-11-18, the exe53 start-up crash): the dump buffers come from a pool that keeps a reference to every
// buffer handed back, so the map callback below never drops a buffer's last reference.
// Why: our pinned Dawn (encounter/dawn v20260807.225922, commit 1155e0ed) has a use-after-free in
// Buffer::MapAsyncEvent::Complete. It declares its lock on the buffer's mPendingMapMutex before its own strong
// reference to the buffer, so on return the reference goes first. When that was the last one (our lambda, which held
// the only other reference, is destroyed inside the callback), the buffer is freed and the unlock then writes into
// freed memory. Usually silent; when the freed page has gone back to Windows it is an access violation in
// MSVCP140!_Mtx_unlock under wgpu::Instance::ProcessEvents. Measured with the frozen exe53 and ORRE_SMOOTH_THUMBS=all:
// 2 crashes in 20 short boots, 0 in 30 without the thumbnails (verified). snatcherLeo's F12 screenshots come through
// aurora_orre_request_frame_dump too, so players were exposed (likely).
// Fixed upstream in google/dawn addd9b41de06 (2026-08-27, crbug.com/552762484: the Ref is now declared before the
// lock). Remove this pool's reason (it can stay as a plain reuse pool) once our Dawn includes that commit.
// Buffers of another size (the render size changed) are let go only at a later acquire, at least a second after they
// came back, so a buffer is never released while the Complete that returned it may still be running on another
// thread (likely: Complete returns microseconds after the callback). All OSes (WebGPU only).
struct OrreDumpFree {
  uint64_t bytes;
  wgpu::Buffer buffer;
  std::chrono::steady_clock::time_point returned;
};
std::mutex g_orreDumpPoolLock;
std::vector<OrreDumpFree> g_orreDumpFree;

wgpu::Buffer orre_dump_acquire(uint64_t bytes) {
  std::vector<wgpu::Buffer> letGo; // released after the lock, outside any map callback (this runs while encoding)
  wgpu::Buffer found;
  {
    std::lock_guard lock{g_orreDumpPoolLock};
    const auto now = std::chrono::steady_clock::now();
    for (auto it = g_orreDumpFree.begin(); it != g_orreDumpFree.end();) {
      if (!found && it->bytes == bytes) {
        found = std::move(it->buffer);
        it = g_orreDumpFree.erase(it);
      } else if (it->bytes != bytes && now - it->returned > std::chrono::seconds{1}) {
        letGo.push_back(std::move(it->buffer));
        it = g_orreDumpFree.erase(it);
      } else {
        ++it;
      }
    }
  }
  if (found) {
    return found;
  }
  const wgpu::BufferDescriptor descriptor{
      .label = "OrrePort frame dump",
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = bytes,
  };
  return g_device.CreateBuffer(&descriptor);
}

// From the map callback, on every path except CallbackCancelled: the pool's copy keeps the buffer alive past Dawn's
// Complete. CallbackCancelled comes only from Dawn's shutdown completion, which runs the callback without promoting
// the buffer or taking its lock, so dropping it there is safe (and the pool is already cleared at shutdown).
void orre_dump_return(const wgpu::Buffer& buffer, uint64_t bytes) {
  std::lock_guard lock{g_orreDumpPoolLock};
  g_orreDumpFree.push_back({bytes, buffer, std::chrono::steady_clock::now()});
}

void orre_dump_pool_clear() {
  std::vector<OrreDumpFree> pool;
  {
    std::lock_guard lock{g_orreDumpPoolLock};
    pool.swap(g_orreDumpFree);
  }
}

void orre_dump_texture(const wgpu::CommandEncoder& encoder, const webgpu::TextureWithSampler& tex, std::string path,
                       std::vector<gfx::AfterSubmitCallback>& after) {
  const uint32_t w = tex.size.width;
  const uint32_t h = tex.size.height;
  const bool bgra = tex.format == wgpu::TextureFormat::BGRA8Unorm || tex.format == wgpu::TextureFormat::BGRA8UnormSrgb;
  const bool rgba = tex.format == wgpu::TextureFormat::RGBA8Unorm || tex.format == wgpu::TextureFormat::RGBA8UnormSrgb;
  if (!tex.texture || w == 0 || h == 0 || (!bgra && !rgba)) {
    Log.warn("frame dump {}: skipped (no texture, or format {} isn't 8-bit RGBA / BGRA)", path,
             magic_enum::enum_name(tex.format));
    return;
  }
  const uint32_t rowBytes = AURORA_ALIGN(w * 4, 256);
  // 2026-11-18: from the pool above (the Dawn map-callback use-after-free), not a fresh buffer per picture
  wgpu::Buffer buffer = orre_dump_acquire(static_cast<uint64_t>(rowBytes) * h);
  const wgpu::TexelCopyTextureInfo src{.texture = tex.texture};
  const wgpu::TexelCopyBufferInfo dst{
      .layout = wgpu::TexelCopyBufferLayout{.offset = 0, .bytesPerRow = rowBytes, .rowsPerImage = h},
      .buffer = buffer,
  };
  const wgpu::Extent3D size{w, h, 1};
  encoder.CopyTextureToBuffer(&src, &dst, &size);
  after.emplace_back([buffer, path = std::move(path), w, h, rowBytes, bgra] {
    buffer.MapAsync(
        wgpu::MapMode::Read, 0, static_cast<uint64_t>(rowBytes) * h, wgpu::CallbackMode::AllowSpontaneous,
        [buffer, path, w, h, rowBytes, bgra](wgpu::MapAsyncStatus status, wgpu::StringView message) {
          const uint64_t bufferBytes = static_cast<uint64_t>(rowBytes) * h;
          if (status != wgpu::MapAsyncStatus::Success) {
            Log.warn("frame dump {}: mapping failed ({})", path, magic_enum::enum_name(status));
            // 2026-11-18: Aborted and Error run with Dawn's buffer lock held too (orre_dump_return, the Dawn bug)
            if (status != wgpu::MapAsyncStatus::CallbackCancelled) {
              orre_dump_return(buffer, bufferBytes);
            }
            return;
          }
          const auto* data =
              static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, static_cast<size_t>(rowBytes) * h));
          // OrrePort, frame smoothing round 12 (the census of what still runs at the step rate; a test switch,
          // ORRE_SMOOTH_THUMBS in pc_smooth.c): a "thumb:<frame>_step.ppm" / "_ib<k>.ppm" path writes a small grey
          // picture (160 x 120, each pixel the mean of its block) into one shared file, smooth_thumbs.bin, instead of
          // a full PPM: a whole sitting's pictures fit in a few GB, and one file instead of a million. Every 32nd game
          // frame's step also a 320 x 240 colour picture (for the report's pictures). Records: "THB1", u32 frame, u8 kind (0 the
          // step, k the k-th in-between, 0x80 the colour step), u16 width, u16 height, u8 channels, the pixels. The
          // mapping callbacks may come in any order, so each record carries its frame. All OSes (stdio).
          if (data != nullptr && path.rfind("thumb:", 0) == 0) {
            static std::mutex thumbLock;
            const unsigned long frame = std::strtoul(path.c_str() + 6, nullptr, 10);
            const auto ibAt = path.find("_ib");
            const uint8_t kind = ibAt != std::string::npos
                                     ? static_cast<uint8_t>(std::strtoul(path.c_str() + ibAt + 3, nullptr, 10))
                                     : 0;
            auto put = [&](FILE* f, uint32_t tw, uint32_t th, uint8_t ch, uint8_t k) {
              std::vector<uint8_t> out(static_cast<size_t>(tw) * th * ch);
              const uint32_t bw = std::max(1u, w / tw), bh = std::max(1u, h / th);
              for (uint32_t ty = 0; ty < th; ++ty) {
                for (uint32_t tx = 0; tx < tw; ++tx) {
                  uint32_t sum[3] = {0, 0, 0}, n = 0;
                  for (uint32_t y = ty * bh; y < std::min(h, (ty + 1) * bh); ++y) {
                    const uint8_t* p = data + static_cast<size_t>(y) * rowBytes + static_cast<size_t>(tx * bw) * 4;
                    for (uint32_t x = tx * bw; x < std::min(w, (tx + 1) * bw); ++x, p += 4, ++n) {
                      sum[0] += bgra ? p[2] : p[0];
                      sum[1] += p[1];
                      sum[2] += bgra ? p[0] : p[2];
                    }
                  }
                  n = std::max(n, 1u);
                  uint8_t* o = &out[(static_cast<size_t>(ty) * tw + tx) * ch];
                  if (ch == 1) {
                    o[0] = static_cast<uint8_t>((sum[0] * 299 + sum[1] * 587 + sum[2] * 114) / (1000 * n));
                  } else {
                    o[0] = static_cast<uint8_t>(sum[0] / n);
                    o[1] = static_cast<uint8_t>(sum[1] / n);
                    o[2] = static_cast<uint8_t>(sum[2] / n);
                  }
                }
              }
              const uint32_t fr = static_cast<uint32_t>(frame);
              const uint16_t dims[2] = {static_cast<uint16_t>(tw), static_cast<uint16_t>(th)};
              std::fwrite("THB1", 1, 4, f);
              std::fwrite(&fr, 4, 1, f);
              std::fwrite(&k, 1, 1, f);
              std::fwrite(dims, 2, 2, f);
              std::fwrite(&ch, 1, 1, f);
              std::fwrite(out.data(), 1, out.size(), f);
            };
            std::lock_guard lock{thumbLock};
            if (FILE* f = std::fopen("smooth_thumbs.bin", "ab")) {
              put(f, 160, 120, 1, kind);
              if (kind == 0 && frame % 32 == 0) { // every 32nd frame: a whole the reference playthrough pass stays well under 100 GB
                put(f, 320, 240, 3, 0x80);
              }
              std::fclose(f);
            }
            buffer.Unmap();
            orre_dump_return(buffer, bufferBytes); // 2026-11-18: back to the pool (the Dawn bug, orre_dump_return)
            return;
          }
          FILE* f = data != nullptr ? std::fopen(path.c_str(), "wb") : nullptr;
          if (f != nullptr) {
            std::fprintf(f, "P6\n%u %u\n255\n", w, h);
            std::vector<uint8_t> row(static_cast<size_t>(w) * 3);
            for (uint32_t y = 0; y < h; ++y) {
              const uint8_t* p = data + static_cast<size_t>(y) * rowBytes;
              for (uint32_t x = 0; x < w; ++x, p += 4) {
                row[x * 3 + 0] = bgra ? p[2] : p[0];
                row[x * 3 + 1] = p[1];
                row[x * 3 + 2] = bgra ? p[0] : p[2];
              }
              std::fwrite(row.data(), 1, row.size(), f);
            }
            std::fclose(f);
            Log.info("frame dump: {} ({}x{})", path, w, h);
          } else {
            Log.warn("frame dump {}: cannot write", path);
          }
          buffer.Unmap();
          orre_dump_return(buffer, bufferBytes); // 2026-11-18: back to the pool (the Dawn bug, orre_dump_return)
        });
  });
}
#endif

#ifdef AURORA_ENABLE_GX
constexpr std::array PreferredBackendOrder{
#ifdef ENABLE_BACKEND_WEBGPU
    BACKEND_WEBGPU,
#endif
#ifdef DAWN_ENABLE_BACKEND_D3D12
    BACKEND_D3D12,
#endif
#ifdef DAWN_ENABLE_BACKEND_METAL
    BACKEND_METAL,
#endif
#ifdef DAWN_ENABLE_BACKEND_VULKAN
    BACKEND_VULKAN,
#endif
#ifdef DAWN_ENABLE_BACKEND_D3D11
    BACKEND_D3D11,
#endif
// #ifdef DAWN_ENABLE_BACKEND_DESKTOP_GL
//     BACKEND_OPENGL,
// #endif
#ifdef DAWN_ENABLE_BACKEND_OPENGLES
    BACKEND_OPENGLES,
#endif
#ifdef DAWN_ENABLE_BACKEND_NULL
    BACKEND_NULL,
#endif
};
#else
constexpr std::array<AuroraBackend, 0> PreferredBackendOrder{};
#endif

bool g_initialFrame = false;

AuroraInfo initialize(int argc, char* argv[], const AuroraConfig& config) noexcept {
  g_config = config;
  Log.info("Aurora initializing");
  log_system_information();
  if (g_config.appName == nullptr) {
    g_config.appName = "Aurora";
  } else {
    g_config.appName = strdup(g_config.appName);
  }
  if (g_config.userPath == nullptr) {
    g_config.userPath = SDL_GetPrefPath(nullptr, g_config.appName);
  } else {
    g_config.userPath = strdup(g_config.userPath);
  }
  if (g_config.cachePath == nullptr) {
    g_config.cachePath = SDL_GetPrefPath(nullptr, g_config.appName);
  } else {
    g_config.cachePath = strdup(g_config.cachePath);
  }
  if (g_config.resourcesPath == nullptr) {
    g_config.resourcesPath = SDL_GetBasePath();
  } else {
    g_config.resourcesPath = strdup(g_config.resourcesPath);
  }
  if (g_config.msaa == 0) {
    g_config.msaa = 1;
  }
  if (g_config.maxTextureAnisotropy == 0) {
    g_config.maxTextureAnisotropy = 16;
  }
  AURORA_ASSERT(window::initialize(), "Error initializing window");

  g_sdlCustomEventsStart = SDL_RegisterEvents(2);
  AURORA_ASSERT(g_sdlCustomEventsStart, "Failed to allocate user events: {}", SDL_GetError());
  AURORA_ASSERT(window::initialize_event_watch(), "Error initializing SDL event watch");

#ifdef AURORA_ENABLE_GX
  /* Attempt to create a window using the calling application's desired backend */
  AuroraBackend selectedBackend = config.desiredBackend;
  bool windowCreated = false;
  if (selectedBackend != BACKEND_AUTO && window::create_window(selectedBackend)) {
    if (webgpu::initialize(selectedBackend, config.allowCpuAdapter)) {
      windowCreated = true;
    } else {
      window::destroy_window();
    }
  }

  if (!windowCreated) {
    for (const auto backendType : PreferredBackendOrder) {
      selectedBackend = backendType;
      if (!window::create_window(selectedBackend)) {
        continue;
      }
      if (webgpu::initialize(selectedBackend, config.allowCpuAdapter)) {
        windowCreated = true;
        break;
      } else {
        window::destroy_window();
      }
    }
  }

  AURORA_ASSERT(windowCreated, "Error creating window: {}", SDL_GetError());

  // Initialize SDL_Renderer for ImGui when we can't use a Dawn backend
  if (webgpu::g_backendType == wgpu::BackendType::Null) {
    AURORA_ASSERT(window::create_renderer(), "Failed to initialize SDL renderer: {}", SDL_GetError());
  }
#else
  AuroraBackend selectedBackend = BACKEND_NULL;
  AURORA_ASSERT(window::create_window(BACKEND_NULL), "Error creating window: {}", SDL_GetError());
  AURORA_ASSERT(window::create_renderer(), "Failed to initialize SDL renderer: {}", SDL_GetError());
#endif

  if (g_config.windowCreatedCallback != nullptr) {
    /* OrrePort: the icon, title and title-bar style on the still-hidden window (AuroraConfig.windowCreatedCallback) */
    g_config.windowCreatedCallback(window::get_sdl_window());
  }
  window::show_window();
  thread::set_current({
      .name = "Main thread",
      .affinity = thread::Affinity::SharedCache,
  });

#ifdef AURORA_ENABLE_GX
  gfx::initialize();
  gx::fifo::init();
  imgui::create_context();
#endif
  const auto size = window::get_window_size();
  Log.info("Using framebuffer size {}x{} scale {}", size.fb_width, size.fb_height, size.scale);
#ifdef AURORA_ENABLE_GX
  if (g_config.imGuiInitCallback != nullptr) {
    g_config.imGuiInitCallback(&size);
  }
  imgui::initialize();
#endif

#ifdef AURORA_ENABLE_RMLUI
  rmlui::initialize(size);
#endif

  g_initialFrame = true;
  g_config.desiredBackend = selectedBackend;
  return {
      .backend = selectedBackend,
      .userPath = g_config.userPath,
      .cachePath = g_config.cachePath,
      .window = window::get_sdl_window(),
      .windowSize = size,
  };
}

void shutdown() noexcept {
#ifdef AURORA_ENABLE_GX
  gx::fifo::shutdown();
  gfx::render_worker::synchronize();
  orre_dump_pool_clear(); // OrrePort, 2026-11-18: the frame dump pool's buffers, before the device goes
#ifdef AURORA_ENABLE_RMLUI
  rmlui::shutdown();
#endif
  imgui::shutdown();
  gfx::shutdown();
  webgpu::shutdown();
#endif
  gamepad::shutdown();
  window::shutdown();
}

const AuroraEvent* update() noexcept {
  ZoneScoped;
  if (g_initialFrame) {
    g_initialFrame = false;
    gamepad::initialize();
  }
#ifdef AURORA_ENABLE_GX
  gx::update();
#endif
  /* OrrePort test switch (2026-11-20): ORRE_RESIZE_STRESS=<frames> resizes the window between two sizes every <frames>
   * updates, so each resize re-creates the surface and frame buffers while frame smoothing's presenter runs. Built to
   * prove the start-up crash's fix (render_worker.hpp, IdleHookPause); with ORRE_RESIZE_STRESS_NOPAUSE=1 the pause is
   * off, to show the race without it. Needs a shown window (ORRE_WINDOW=normal): a minimized one never resizes. A test
   * tool only; no player setting reads it. Every OS (SDL's window size call). */
  static const int resizeStress = [] {
    const char* v = std::getenv("ORRE_RESIZE_STRESS");
    return v != nullptr ? std::atoi(v) : 0;
  }();
  if (resizeStress > 0) {
    static uint64_t updates = 0;
    static bool big = false;
    if (++updates % static_cast<uint64_t>(resizeStress) == 0) {
      big = !big;
      window::set_window_size(big ? 1280u : 960u, big ? 960u : 720u);
      static uint64_t resizes = 0;
      if (++resizes % 50 == 0) {
        std::fprintf(stderr, "[resize_stress] %llu resizes\n", static_cast<unsigned long long>(resizes));
      }
    }
  }
  return window::poll_events();
}

bool begin_frame() noexcept {
  ZoneScoped;
#ifdef AURORA_ENABLE_GX
  {
    if (!window::is_presentable()) {
      webgpu::release_surface();
      return false;
    }
    if (window::is_paused()) {
      return false;
    }
    if (!g_surface) {
      webgpu::refresh_surface(true);
      if (!g_surface) {
        return false;
      }
    }
  }

  imgui::new_frame(window::get_window_size());
  if (!gfx::begin_frame()) {
    return false;
  }
  gx::fifo::begin_frame();
#endif
  return true;
}

#ifdef AURORA_ENABLE_GX
// ---- The window picture (OrrePort, 2026-11-16: split out of end_frame for frame smoothing) ----
// A step's own present and frame smoothing's pictures (frame-smoothing-plan.md, steps a and b) put the same things in
// the window: the game picture from the frame buffer, the RmlUi frame and ImGui. WindowLayer is what end_frame freezes
// on the game thread; encode_window_picture and submit_and_present are end_frame's old present code, unchanged in what
// they do, so a step presents exactly as before when smoothing is off. Render worker only. Platform-neutral.
struct WindowLayer {
  wgpu::BindGroup rmlBindGroup;
  bool rmlOverlay = false;
  webgpu::TextureWithSampler rmlTarget; // what rmlBindGroup samples (frame smoothing section 33: the UI dumps)
  gfx::Viewport viewport{};
  imgui::DrawData imguiDrawData;
};

// OrrePort, frame smoothing's pacing numbers (2026-11-16, step c's pacing work; frame-smoothing-plan.md section 13):
// the longest wait for the window's next texture, the longest submit and the longest present call since the last
// [smooth] summary, so a late or slow picture can be traced to where the render worker waited. Three clock reads per
// picture; on every present, smoothing on or off.
namespace smooth_timing {
std::atomic<uint64_t> g_acquireMaxNs{0}, g_submitMaxNs{0}, g_presentMaxNs{0};
void note_max(std::atomic<uint64_t>& value, uint64_t sample) noexcept {
  uint64_t current = value.load(std::memory_order_relaxed);
  while (sample > current && !value.compare_exchange_weak(current, sample, std::memory_order_relaxed)) {
  }
}
uint64_t now_ns() noexcept { return SDL_GetTicksNS(); }
} // namespace smooth_timing

// Acquires the window's next surface texture and records the window picture into `encoder` (after the work already in
// it, so the game picture is the frame buffer as that work leaves it). Returns the texture to present, or an empty one
// when the window can't take a picture now (`surfaceStatus` says why).
wgpu::Texture encode_window_picture(const wgpu::CommandEncoder& encoder, const WindowLayer& layer,
                                    wgpu::SurfaceGetCurrentTextureStatus& surfaceStatus) {
  const auto& rmlBindGroup = layer.rmlBindGroup;
  const bool rmlOverlay = layer.rmlOverlay;
  const auto& viewport = layer.viewport;
  const auto& imguiDrawData = layer.imguiDrawData;
  {
    wgpu::Texture currentTexture;
    wgpu::TextureView currentView;
    surfaceStatus = wgpu::SurfaceGetCurrentTextureStatus::Error;
    {
      window::SurfaceLock surfaceLock;
      if (window::is_presentable() && g_surface) {
        ZoneScopedN("Acquire texture");
        wgpu::SurfaceTexture surfaceTexture;
        const uint64_t acquireStart = smooth_timing::now_ns();
        g_surface.GetCurrentTexture(&surfaceTexture);
        smooth_timing::note_max(smooth_timing::g_acquireMaxNs, smooth_timing::now_ns() - acquireStart);
        surfaceStatus = surfaceTexture.status;
        if (surfaceStatus == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
          currentTexture = std::move(surfaceTexture.texture);
          currentView = currentTexture.CreateView();
        }
      }
    }

    const bool canPresent = currentTexture && currentView;
    if (canPresent) {
      wgpu::BindGroup presentBindGroup;
      if (rmlBindGroup && !rmlOverlay) {
        presentBindGroup = rmlBindGroup;
      } else {
        const auto& resampledSource = webgpu::resample_present_source(encoder, viewport);
        presentBindGroup = webgpu::create_copy_bind_group(resampledSource);
      }
      {
        const std::array attachments{
            wgpu::RenderPassColorAttachment{
                .view = currentView,
                .loadOp = wgpu::LoadOp::Clear,
                .storeOp = wgpu::StoreOp::Store,
            },
        };
        const wgpu::RenderPassDescriptor renderPassDescriptor{
            .label = "EFB copy render pass",
            .colorAttachmentCount = attachments.size(),
            .colorAttachments = attachments.data(),
            .timestampWrites = webgpu::gpu_prof::pass_writes("Present blit"),
        };
        const auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
        // OrrePort (2026-11-11): the UI frame covers the whole window (rmlui.cpp); the game picture keeps its box
        const gfx::Viewport wholeWindow{
            .left = 0.f,
            .top = 0.f,
            .width = static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.width),
            .height = static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.height),
            .znear = 0.f,
            .zfar = 1.f,
        };
        // Copy EFB -> XFB (swapchain); a UI frame with the game copied in (backdrop filters) is the whole window
        pass.SetPipeline(webgpu::g_CopyPipeline);
        pass.SetBindGroup(0, presentBindGroup, 0, nullptr);
        set_present_viewport(pass, rmlBindGroup && !rmlOverlay ? wholeWindow : viewport,
                             webgpu::g_graphicsConfig.surfaceConfiguration.width,
                             webgpu::g_graphicsConfig.surfaceConfiguration.height);

        pass.Draw(3);
        if (rmlBindGroup && rmlOverlay) {
          set_present_viewport(pass, wholeWindow, webgpu::g_graphicsConfig.surfaceConfiguration.width,
                               webgpu::g_graphicsConfig.surfaceConfiguration.height);
          pass.SetPipeline(webgpu::g_CopyPremultipliedAlphaPipeline);
          pass.SetBindGroup(0, rmlBindGroup, 0, nullptr);
          pass.Draw(3);
        }
        pass.End();
      }
      {
        const std::array attachments{
            wgpu::RenderPassColorAttachment{
                .view = currentView,
                .loadOp = wgpu::LoadOp::Load,
                .storeOp = wgpu::StoreOp::Store,
            },
        };
        const wgpu::RenderPassDescriptor renderPassDescriptor{
            .label = "ImGui render pass",
            .colorAttachmentCount = attachments.size(),
            .colorAttachments = attachments.data(),
            .timestampWrites = webgpu::gpu_prof::pass_writes("ImGui"),
        };
        const auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.SetViewport(0.f, 0.f, static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.width),
                         static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.height), 0.f, 1.f);
        imgui::render(pass, imguiDrawData);
        pass.End();
      }
    } else {
      Log.info("Skipping present; window not presentable");
    }
    return currentTexture;
  }
}

// Finishes and submits `encoder`, then presents `surfaceTexture` (when the window gave one) and deals with a surface
// that failed, then runs the after-submit callbacks. `acquired`: encode_window_picture was called (false: the step's
// work is only submitted, its picture shown later by frame smoothing's presenter). `stepEncoder`: the step's own
// encoder, which carries the GPU profiler's frame. Returns whether a picture was presented.
bool submit_and_present(wgpu::CommandEncoder& encoder, const wgpu::Texture& surfaceTexture,
                        wgpu::SurfaceGetCurrentTextureStatus surfaceStatus, bool acquired, bool stepEncoder,
                        std::vector<gfx::AfterSubmitCallback>& afterSubmitCallbacks) {
  bool presented = false;
  {
    const bool canPresent = static_cast<bool>(surfaceTexture);
    if (stepEncoder) {
      webgpu::gpu_prof::frame_end(encoder);
    }
    const wgpu::CommandBufferDescriptor cmdBufDescriptor{.label = "Redraw command buffer"};
    const auto buffer = encoder.Finish(&cmdBufDescriptor);
    {
      ZoneScopedN("Queue Submit");
      const uint64_t submitStart = smooth_timing::now_ns();
      g_queue.Submit(1, &buffer);
      smooth_timing::note_max(smooth_timing::g_submitMaxNs, smooth_timing::now_ns() - submitStart);
    }
    if (stepEncoder) {
      webgpu::gpu_prof::after_submit();
    }
    if (!acquired) {
      // nothing to present now (frame smoothing shows this step at its picture slot)
    } else if (canPresent && g_surface) {
      ZoneScopedN("Present");
      wgpu::ConvertibleStatus status = wgpu::Status::Error;
      {
        window::SurfaceLock surfaceLock;
        if (window::is_presentable()) {
          const uint64_t presentStart = smooth_timing::now_ns();
          status = g_surface.Present();
          smooth_timing::note_max(smooth_timing::g_presentMaxNs, smooth_timing::now_ns() - presentStart);
        }
      }
      if (status) {
        gfx::after_present();
        presented = true;
      } else {
        Log.warn("Surface present failed");
        webgpu::release_surface();
      }
    } else if (g_surface) {
      switch (surfaceStatus) {
      case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
        Log.warn("Surface texture acquisition timed out");
        break;
      case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
      case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
        // OrrePort (port-gaps 77): not while minimized; the restore's resize event reconfigures it
        if (!window::is_minimized()) {
          Log.info("Surface texture is {}, reconfiguring swapchain", magic_enum::enum_name(surfaceStatus));
          window::push_custom_event(window::CustomEvent::RefreshSurface);
        }
        break;
      case wgpu::SurfaceGetCurrentTextureStatus::Lost:
        Log.warn("Surface texture is {}, releasing surface", magic_enum::enum_name(surfaceStatus));
        webgpu::release_surface();
        break;
      case wgpu::SurfaceGetCurrentTextureStatus::Error:
        Log.warn("Surface texture is {}, dropping surface", magic_enum::enum_name(surfaceStatus));
        g_surface = {};
        break;
      default:
        if (!window::is_presentable()) {
          webgpu::release_surface();
        } else {
          Log.error("Failed to get surface texture: {}", magic_enum::enum_name(surfaceStatus));
        }
        break;
      }
    }
    for (auto& callback : afterSubmitCallbacks) {
      if (callback) {
        callback();
      }
    }
  }
  return presented;
}

// ---- Frame smoothing, steps (a) and (b) (OrrePort, 2026-11-16; docs/port/frame-smoothing-plan.md) ----
// The presenter. It runs on Aurora's render worker only: in each step's end_frame job (after the step's own GPU work
// is recorded) and between the worker's queue items (render_worker's idle hook). It never calls game code: what it
// draws is the step Aurora kept (gfx::encode_kept_step) and the window layer end_frame froze. Two modes:
//   - live: pictures on a fixed grid of `period` (the screen's rate or a cap). At each slot it shows the newest step
//     if one came since the last picture, else an in-between picture: the newest step drawn again (identical, steps a
//     and b; blended from step c on), or the last picture repeated when the step can't be drawn again. When steps come
//     faster than pictures (the game sped up), only the newest step is shown at each slot and nothing is in between;
//   - forced (a test switch for recorded runs, whose replay clock is not the screen's): every step is presented at once
//     and followed by `inBetweens` in-between pictures, while the game thread waits (aurora_smooth_wait).
// Why on the render worker (the plan, section 2): presents already happen there, the game's threads are fibres on one
// OS thread that can't stop mid-step to present, and the worker never runs game code, which the tripwire checks.
// Platform-neutral: WebGPU and SDL's clock. The wait for the next picture slot is SDL_DelayPrecise slices, not the
// worker's condition-variable timeout, which is coarse on Windows (idle_tick says what was measured).
namespace smooth {
std::atomic_int g_mode{AURORA_SMOOTH_OFF};
std::atomic<int64_t> g_periodNs{0};
std::atomic<uint32_t> g_forcedCount{0};
std::atomic_bool g_held{false};
std::atomic_bool g_outpaced{false};
std::atomic_bool g_resetGrid{true};
std::atomic_int g_replayOpen{0};
std::atomic_int g_testWipe{0}; // aurora_smooth_test_wipe (a test switch): 1 wipe then replay, 2 wipe only
// the port's stamp for the step being ended (aurora_smooth_step, then end_frame: both on the game thread)
uint32_t g_stampRetrace = 0;
uint32_t g_stampFlags = 0;
uint64_t g_stampAnchorNs = 0; // step (c): when the step is due on screen (aurora_smooth_step)
uint64_t g_stampArrivalNs = 0; // when the step ended on the game thread (aurora_smooth_step)
// Round 5 (2026-11-17): the length of one retrace at the game's current speed (aurora_smooth_field_ns, game thread),
// and the value the step being ended ran at (stamped with it). See "The step's length from the known speed" below.
std::atomic<uint64_t> g_fieldNs{0};
uint64_t g_stampFieldNs = 0;

// ---- The anchor at the step's arrival (2026-11-17; snatcherLeo asked whether smoothing could have no delay: interpolation's
// one step stays, the extra goes). Step (c) anchored each step at the retrace after it ended (the moment the console
// would show it), and a picture at time t shows (t - anchor) / step of the way from the step before to the newest.
// Measured (P01, live, 120 Hz): on the PC a step's work ends about half a millisecond after its retrace, so that anchor
// lay most of a field after the step was there. Two costs: a step was shown whole about 32 ms after it was drawn in
// 60-a-second scenes (one step more than the blend needs), and, worse, between a step's arrival and its anchor the
// fraction stayed clamped at 0, so most pictures showed a whole step: in 60-a-second scenes nearly all of them (the
// blend ran only in the half millisecond between the next retrace and the next arrival), in 30-a-second ones half of
// each step, then a half-step jump. The pictures were step pictures shown twice, not in-betweens.
// Now the anchor is the step's arrival, kept evenly spaced: the expected anchor is the previous one plus the retrace
// gap; a step arriving early keeps it (drawn toward its arrival plus 1.5 ms of margin for the arrivals' jitter, by at
// most 0.25 ms a step, so a lighter scene slowly
// lowers the delay without changing the motion's speed noticeably: 1.5 % of a 60-a-second step), a step arriving late
// moves it to its arrival (a small jump forward at its first picture, counted), and after a reset, pause or gap it
// starts again at the arrival. A picture never needs a step that isn't there yet: before the render worker has the new
// step the older pair shows its end, which is the new pair's start. The step's length for the fraction stays the
// retrace gap (even). Re-phasing the picture grid to each arrival was not done: pictures would then follow the
// arrivals' jitter instead of the screen's even rate; the grid only samples the clock, it adds no delay of its own.
// Render worker only; live mode only (the test mode's in-betweens are placed by count, not time). Platform-neutral.
int64_t g_leadNs = 0;           // how much earlier the anchor is than step (c)'s retrace anchor (the log)
int64_t g_prevArrivalNs = 0;
int64_t g_prevRetraceAnchorNs = 0;
constexpr int64_t LeadStepNs = 250'000;
constexpr int64_t AnchorMarginNs = 1'500'000;
constexpr int64_t ReanchorNs = 4'000'000;
std::mutex g_dumpLock;
std::string g_dumpPrefix; // aurora_smooth_request_dump (a test switch)

struct Counters {
  std::atomic<uint64_t> steps{0}, stepPictures{0}, stepsNotShown{0}, inBetweens{0}, notPresented{0}, slips{0},
      replayNs{0}, replays{0}, replayDraws{0}, presentNs{0}, presents{0}, worstGapNs{0};
  std::array<std::atomic<uint64_t>, AURORA_SMOOTH_HELD_COUNT> held{};
  // step (c)
  std::atomic<uint64_t> blended{0}, repeats{0}, lateStarts{0}, lateByItem{0}, worstLateNs{0}, worstPictureNs{0};
  // step (d), the coverage measure (2026-11-17): in-between pictures (not a step's own picture) drawn with at least
  // one block blended (camera re-aimed or model moved), and those of them with a model piece blended
  std::atomic<uint64_t> ibBlended{0}, ibModels{0};
  // the step-to-screen delay (2026-11-17, live mode): from a step's arrival to the moment it is shown whole (its
  // successor's anchor), summed, its largest, and what the retrace anchor of step (c) would have given
  std::atomic<uint64_t> delaySumNs{0}, delayMaxNs{0}, delayRetSumNs{0}, delayCount{0}, lateArrivals{0};
  std::atomic<uint64_t> fracZero{0}, fracMid{0}, fracOne{0}; // live pictures by fraction (idle_tick)
  std::atomic<uint64_t> reanchors{0}; // anchors started again at the arrival (drifted more than 4 ms past it)
  std::atomic<uint64_t> retimes{0};   // round 5: pairs re-timed for a game-speed change (retime_for_speed)
};
Counters g_count;

// render worker only
WindowLayer g_layer;
bool g_haveLayer = false;
bool g_stepShown = true;  // the newest step has been in at least one picture (live mode)
uint32_t g_keptFlags = 0; // the port's flags of the kept step
int64_t g_nextSlotNs = 0;
int64_t g_lastPresentNs = 0;
// step (c), the clock (aurora_smooth_step): the newest step's anchor, the time between the last two anchors, and
// whether its cameras may blend from the step before's (history). Live mode only for the times; both modes for history.
int64_t g_anchorNs = 0;
int64_t g_prevAnchorNs = 0;
int64_t g_stepDurNs = 0;
uint64_t g_pairFieldNs = 0; // round 5: the retrace length the current pair's g_stepDurNs was worked out at
// round 5: how long after its last retrace's due time a step arrives (its game work), averaged over recent steps; with
// the port's prediction of the next step's last retrace (aurora_smooth_field_ns) it says when that step will arrive
int64_t g_workNs = 2'000'000;
std::atomic<uint64_t> g_nextStepDueNs{0};
uint32_t g_prevRetrace = 0;
uint32_t g_prevStepFlags = AURORA_SMOOTH_STEP_RESET;
bool g_history = false;
// a gap longer than this between two steps resets history (Dusklight's presentation epoch: 250 ms)
constexpr int64_t HistoryGapNs = 250'000'000;
std::string g_ibDumpPrefix;
uint32_t g_ibDumpLeft = 0;
uint32_t g_ibDumpIndex = 0;

int64_t now_ns() noexcept { return static_cast<int64_t>(SDL_GetTicksNS()); }

void store_max(std::atomic<uint64_t>& value, uint64_t sample) noexcept {
  uint64_t current = value.load(std::memory_order_relaxed);
  while (sample > current && !value.compare_exchange_weak(current, sample, std::memory_order_relaxed)) {
  }
}

// Shows the window picture recorded after `encoder`'s work, and counts it
void present_picture(wgpu::CommandEncoder& encoder, bool stepEncoder,
                     std::vector<gfx::AfterSubmitCallback>& afterSubmitCallbacks) {
  const int64_t start = now_ns();
  auto surfaceStatus = wgpu::SurfaceGetCurrentTextureStatus::Error;
  const auto surfaceTexture = encode_window_picture(encoder, g_layer, surfaceStatus);
  const bool presented =
      submit_and_present(encoder, surfaceTexture, surfaceStatus, true, stepEncoder, afterSubmitCallbacks);
  const int64_t end = now_ns();
  g_count.presentNs.fetch_add(static_cast<uint64_t>(end - start), std::memory_order_relaxed);
  g_count.presents.fetch_add(1, std::memory_order_relaxed);
  if (!presented) {
    g_count.notPresented.fetch_add(1, std::memory_order_relaxed);
  }
  if (g_lastPresentNs != 0) {
    store_max(g_count.worstGapNs, static_cast<uint64_t>(end - g_lastPresentNs));
  }
  g_lastPresentNs = end;
}

// Why an in-between picture can't draw the kept step again (an AURORA_SMOOTH_HELD_* value), or -1 when it can
int held_reason() noexcept {
  if ((g_keptFlags & AURORA_SMOOTH_STEP_PAUSE) != 0) {
    return AURORA_SMOOTH_HELD_PAUSE;
  }
  if ((g_keptFlags & AURORA_SMOOTH_STEP_MOVIE) != 0) {
    return AURORA_SMOOTH_HELD_MOVIE;
  }
  if ((g_keptFlags & AURORA_SMOOTH_STEP_NO_COPY) != 0) {
    return AURORA_SMOOTH_HELD_NO_COPY;
  }
  // ---- UI frames (RmlUi): no longer held (2026-11-19, frame-smoothing-plan.md section 33). ----
  // History: at first every RmlUi frame held ("held at least until smoothing knows what 2D to hold"). *Changed
  // 2026-11-17 (round 9, section 22):* only a UI frame with the game picture copied into it (a backdrop filter: the
  // Settings overlay's blur, the hub) held; an overlay frame (the fast-forward badge, toasts, the menu hint) was put
  // over each in-between like over the step's own picture (snatcherLeo's short fast-forward taps: 75-88 pictures held per tap
  // before that, verified in his log of 2026-11-17 18:19).
  // *Changed 2026-11-19 (section 33):* backdrop frames don't hold either. snatcherLeo: "if I open the settings ... the
  // smoothness goes away" (his log of 2026-11-19 13:48: with the Settings overlay open and the game running behind it,
  // 887 of 902 in-betweens in 10 s held for "UI", so the game behind the menu moved at its 30 steps a second whatever
  // the Frame rate setting; he wants to compare frame rates live with the menu open). Why holding was never needed:
  // the UI frame is recorded into the step's own command list (aurora::end_frame calls rmlui::record_frame before
  // gfx::end_frame closes the recording), after the game's draws, and its first pass seeds the backdrop from the
  // frame buffer (WebGPURenderInterface::EnsureFrameRenderingStarted: the "base layer seed pass" reads present_source).
  // An in-between replays the whole step (encode_replay: every render pass and texture copy, RmlUi's included), so it
  // draws the blended game picture into the frame buffer first, then the UI frame again on top of it: the seed, the
  // blur and the panel are re-made from the in-between's own picture, and encode_window_picture shows the re-made UI
  // texture as for the step. The overlay frames of round 9 already went through this same replay. Cost: the UI's
  // passes once more per in-between (the blur over the window). Display only: the replay runs on the render worker
  // and touches no game state (the tripwire checks every in-between). The hub: smoothing only runs from the first game
  // step on, so the hub before the game is never smoothed; a hub frame replayed would come out identical anyway.
  // ORRE_SMOOTH_HOLD_UI=1 (a test switch, read once): the old rule (backdrop UI frames hold), for before / after
  // measurements with one exe. Platform-neutral (WebGPU only).
  static const bool holdUiTest = [] {
    const char* v = std::getenv("ORRE_SMOOTH_HOLD_UI");
    return v != nullptr && v[0] == '1';
  }();
  if (holdUiTest && g_layer.rmlBindGroup && !g_layer.rmlOverlay) {
    return AURORA_SMOOTH_HELD_UI;
  }
  switch (gfx::kept_step_state()) {
  case gfx::KeptStep::Replayable:
    return -1;
  case gfx::KeptStep::KeptEfb:
    return AURORA_SMOOTH_HELD_KEPT_EFB;
  case gfx::KeptStep::Resized:
    return AURORA_SMOOTH_HELD_RESIZED;
  case gfx::KeptStep::Stale:
    return AURORA_SMOOTH_HELD_STALE;
  case gfx::KeptStep::None:
  default:
    return AURORA_SMOOTH_HELD_NONE;
  }
}

// ORRE_SMOOTH_DUMP's UI pictures (2026-11-19, section 33; a test switch, look only): when the layer is a UI frame with
// the game picture inside it (the Settings overlay's backdrop blur), the UI texture the window shows is dumped next to
// the frame buffer's picture ("<name>ui.ppm"), so a step's and its in-betweens' window pictures can be compared: each
// in-between's UI must carry its own blended game picture under the panel. Full dumps only (the census's small
// "thumb:" pictures stay the frame buffer's). Render worker only; platform-neutral.
void dump_ui(const wgpu::CommandEncoder& encoder, const WindowLayer& layer, const std::string& path,
             std::vector<gfx::AfterSubmitCallback>& afterSubmitCallbacks) {
  if (!layer.rmlBindGroup || layer.rmlOverlay || !layer.rmlTarget.texture || path.rfind("thumb:", 0) == 0) {
    return;
  }
  const auto dot = path.rfind(".ppm");
  orre_dump_texture(encoder, layer.rmlTarget, (dot == std::string::npos ? path : path.substr(0, dot)) + "ui.ppm",
                    afterSubmitCallbacks);
}

// Step (c), the clock: how far a picture shown at `now` is from the previous step's camera (0) to the newest's (1).
// One step behind (Dusklight): the newest step's camera is reached one step period after its anchor, by which time the
// next step should have come. Clamped, so a late step holds the newest camera rather than running past it.
float live_fraction(int64_t now) noexcept {
  if (!g_history || g_stepDurNs <= 0 || g_outpaced.load(std::memory_order_relaxed)) {
    return 1.f;
  }
  const double f = static_cast<double>(now - g_anchorNs) / static_cast<double>(g_stepDurNs);
  return static_cast<float>(std::clamp(f, 0.0, 1.0));
}

// ---- Round 5 (2026-11-17; docs/port/frame-smoothing-plan.md section 17): letting go of fast-forward with no pause. ----
// snatcherLeo, after round 4: "when I let go, it is in 30FPS, and then goes to the 120FPS ... if I let go, it goes right back
// to the frame rate we're at without any pause". Measured (round 4, P01, 3x released): the last fast step's pair
// (step N-1 to N) was timed for the fast speed (11 ms at 3x in a 30-a-second scene), so it reached N whole 11 ms after
// N's anchor, but the first slow step N+1 only arrives one slow step (about 33 ms) after N: one to three pictures showed
// N whole, a held picture at the release. Why it can't be avoided by waiting: the screen runs one step behind, and one
// step is three times longer now, so the delay has to grow by 22 ms somewhere. Where it grows decides how it looks;
// a hold looks like a hitch. Fixed here: the moment the game's speed changes (the port tells the new retrace length,
// aurora_smooth_field_ns), the pair on screen is re-timed so it ends when the next step will be there, continuing from
// the fraction shown now: the motion slows at once, with no picture held. When that is: the port predicts the due time
// of the next step's last retrace at the new speed (it knows the step's length in retraces and the retrace clock), and
// a step arrives a steady few ms after that retrace (its game work: g_workNs, measured on every step), plus the usual
// anchor margin. Why predicted and not simply stretched by the speed ratio (the first version, measured): the release
// usually comes part-way through a step that already ran partly fast, so that step arrives sooner than a whole slow
// step; the stretched pair was cut short there (a jump of half a step: 15304 at fraction 0.51, then 15306 at 0.06), and
// holding the next anchor back to the stretched end showed the older step whole instead (pictures at fraction 0): only
// the newest pair can be drawn once its step is in. Without a prediction (0) the stretch is the fallback. Speeding up
// shortens the pair the same way (a pair already past its new end ends now: a small step forward, as before).
// Render worker only; live mode only. Platform-neutral.
void retime_for_speed(int64_t now) noexcept {
  const uint64_t field = g_fieldNs.load(std::memory_order_acquire);
  if (field == 0 || g_pairFieldNs == 0 || field == g_pairFieldNs) {
    return;
  }
  const uint64_t was = g_pairFieldNs;
  g_pairFieldNs = field;
  if (!g_history || g_stepDurNs <= 0) {
    return;
  }
  const double ratio = static_cast<double>(field) / static_cast<double>(was);
  const auto due = static_cast<int64_t>(g_nextStepDueNs.load(std::memory_order_relaxed));
  const int64_t newEnd = due > 0 ? due + g_workNs + AnchorMarginNs
                                 : g_anchorNs + static_cast<int64_t>(static_cast<double>(g_stepDurNs) * ratio);
  const double f0 = std::clamp(static_cast<double>(now - g_anchorNs) / static_cast<double>(g_stepDurNs), 0.0, 1.0);
  int64_t duration;
  if (f0 >= 1.0 || newEnd <= now) {
    // already showing the newest step whole, or the pair's new end has passed (speeding up): the pair ends now
    duration = std::max<int64_t>(1, static_cast<int64_t>(static_cast<double>(g_stepDurNs) * ratio));
    g_anchorNs = now - duration;
  } else {
    // continue from f0 and reach 1 at the new end: the rest of the pair over (newEnd - now)
    duration = static_cast<int64_t>(static_cast<double>(newEnd - now) / (1.0 - f0));
    g_anchorNs = now - static_cast<int64_t>(f0 * static_cast<double>(duration));
  }
  g_stepDurNs = duration;
  g_prevAnchorNs = g_anchorNs;
  g_count.retimes.fetch_add(1, std::memory_order_relaxed);
}

// One in-between picture: the kept step drawn again into the frame buffer, its cameras `fraction` of the way from the
// previous step's (step c; 1 = the step's own camera, the identical picture), or, when it can't be drawn again, the
// frame buffer as the last picture left it; then shown. g_replayOpen marks it for the tripwire. The test mode's path:
// it always draws the step again, so the replay is exercised even where nothing moves.
void in_between_picture(float fraction) {
  const int reason = held_reason();
  g_replayOpen.fetch_add(1, std::memory_order_acq_rel);
  const int64_t start = now_ns();
  constexpr wgpu::CommandEncoderDescriptor EncoderDescriptor{.label = "Smoothing in-between encoder"};
  auto encoder = g_device.CreateCommandEncoder(&EncoderDescriptor);
  std::vector<gfx::AfterSubmitCallback> afterSubmitCallbacks;
  uint32_t draws = 0;
  uint32_t reaimed = 0;
  if (reason < 0) {
    const int wipe = g_testWipe.load(std::memory_order_relaxed);
    if (wipe != 0) {
      // test switch (aurora_smooth_test_wipe): the frame buffer is painted magenta first, so an in-between picture
      // that matches its step's byte for byte proves the replay redrew the whole picture (without it, a replay that
      // drew nothing would also "match", the frame buffer still holding the step's own picture)
      const bool msaa = webgpu::g_graphicsConfig.msaaSamples > 1;
      const std::array attachments{wgpu::RenderPassColorAttachment{
          .view = webgpu::g_frameBuffer.view,
          .resolveTarget = msaa ? webgpu::g_frameBufferResolved.view : nullptr,
          .loadOp = wgpu::LoadOp::Clear,
          .storeOp = wgpu::StoreOp::Store,
          .clearValue = {1.0, 0.0, 1.0, 1.0},
      }};
      const wgpu::RenderPassDescriptor wipeDescriptor{
          .label = "Smoothing test wipe",
          .colorAttachmentCount = attachments.size(),
          .colorAttachments = attachments.data(),
      };
      encoder.BeginRenderPass(&wipeDescriptor).End();
    }
    // wipe mode 2 is the control's own control: wipe and draw nothing, so the dumps must come out magenta (proves the
    // wipe works and the dump comparison can fail)
    if (wipe != 2) {
      bool drew = false;
      draws = gfx::encode_kept_step_blended(encoder, fraction, reaimed, true, drew);
    }
  }
  if (g_ibDumpLeft > 0) {
    --g_ibDumpLeft;
    const std::string path = g_ibDumpPrefix + "_ib" + std::to_string(++g_ibDumpIndex) + ".ppm";
    orre_dump_texture(encoder, webgpu::present_source(), path, afterSubmitCallbacks);
    dump_ui(encoder, g_layer, path, afterSubmitCallbacks); // section 33: the UI as this in-between re-made it
  }
  const int64_t recorded = now_ns();
  present_picture(encoder, false, afterSubmitCallbacks);
  g_replayOpen.fetch_sub(1, std::memory_order_acq_rel);
  if (reason < 0) {
    g_count.inBetweens.fetch_add(1, std::memory_order_relaxed);
    g_count.blended.fetch_add(reaimed > 0 ? 1u : 0u, std::memory_order_relaxed);
    g_count.ibBlended.fetch_add(reaimed > 0 ? 1u : 0u, std::memory_order_relaxed);
    g_count.ibModels.fetch_add(reaimed > 0 && gfx::smooth_camera::last_model_writes() > 0 ? 1u : 0u,
                               std::memory_order_relaxed);
    g_count.replayNs.fetch_add(static_cast<uint64_t>(recorded - start), std::memory_order_relaxed);
    g_count.replays.fetch_add(1, std::memory_order_relaxed);
    g_count.replayDraws.fetch_add(draws, std::memory_order_relaxed);
  } else {
    g_count.held[reason].fetch_add(1, std::memory_order_relaxed);
  }
  store_max(g_count.worstPictureNs, static_cast<uint64_t>(now_ns() - start));
}

// Step (c), live mode: the picture at a slot. The newest step with its cameras at `fraction`: drawn again blended when
// a camera moved, drawn again unblended when the frame buffer holds a blended picture but nothing is to move now, or
// shown as the frame buffer holds it (the step's own picture: no redraw at all). Steps that can't be drawn again repeat
// the last picture, as in steps (a) and (b).
void slot_picture(float fraction) {
  const int reason = held_reason();
  const int64_t start = now_ns();
  constexpr wgpu::CommandEncoderDescriptor EncoderDescriptor{.label = "Smoothing picture encoder"};
  auto encoder = g_device.CreateCommandEncoder(&EncoderDescriptor);
  std::vector<gfx::AfterSubmitCallback> afterSubmitCallbacks;
  uint32_t draws = 0;
  uint32_t reaimed = 0;
  bool redrawn = false;
  g_replayOpen.fetch_add(1, std::memory_order_acq_rel);
  // item (c) (2026-11-17): at fraction 1 too when a menu model is in the first half of its two-step blend (its picture
  // at the step's slot shows it half way, not at the step's own pose)
  if (reason < 0 && (fraction < 1.f || !gfx::kept_step_exact() || gfx::smooth_camera::two_step_at_one())) {
    draws = gfx::encode_kept_step_blended(encoder, fraction, reaimed, false, redrawn);
  }
  if (redrawn && g_ibDumpLeft > 0) {
    --g_ibDumpLeft;
    const std::string path = g_ibDumpPrefix + "_ib" + std::to_string(++g_ibDumpIndex) + ".ppm";
    orre_dump_texture(encoder, webgpu::present_source(), path, afterSubmitCallbacks);
    dump_ui(encoder, g_layer, path, afterSubmitCallbacks); // section 33: the UI as this in-between re-made it
  }
  const int64_t recorded = now_ns();
  present_picture(encoder, false, afterSubmitCallbacks);
  g_replayOpen.fetch_sub(1, std::memory_order_acq_rel);
  if (!g_stepShown) {
    g_count.stepPictures.fetch_add(1, std::memory_order_relaxed);
  } else if (reason >= 0) {
    g_count.held[reason].fetch_add(1, std::memory_order_relaxed);
  } else if (!redrawn) {
    g_count.repeats.fetch_add(1, std::memory_order_relaxed);
  } else {
    g_count.inBetweens.fetch_add(1, std::memory_order_relaxed);
    // step (d), the coverage measure: an in-between that moved something (an unblended redraw only puts the frame
    // buffer back to the step's own picture)
    g_count.ibBlended.fetch_add(reaimed > 0 ? 1u : 0u, std::memory_order_relaxed);
    g_count.ibModels.fetch_add(reaimed > 0 && gfx::smooth_camera::last_model_writes() > 0 ? 1u : 0u,
                               std::memory_order_relaxed);
  }
  if (redrawn) {
    g_count.blended.fetch_add(reaimed > 0 ? 1u : 0u, std::memory_order_relaxed);
    g_count.replayNs.fetch_add(static_cast<uint64_t>(recorded - start), std::memory_order_relaxed);
    g_count.replays.fetch_add(1, std::memory_order_relaxed);
    g_count.replayDraws.fetch_add(draws, std::memory_order_relaxed);
  }
  g_stepShown = true;
  store_max(g_count.worstPictureNs, static_cast<uint64_t>(now_ns() - start));
}

// The picture grid moves on one period after a picture shown at `shownAt`; a picture more than half a period late is a
// slip, and a grid left behind (a long step, a hold) restarts from now rather than catching up
void advance_slot(int64_t shownAt, int64_t period) noexcept {
  if (shownAt - g_nextSlotNs > period / 2) {
    g_count.slips.fetch_add(1, std::memory_order_relaxed);
  }
  g_nextSlotNs += period;
  if (g_nextSlotNs <= shownAt) {
    g_nextSlotNs = shownAt + period;
  }
}

bool live_and_running() noexcept {
  return g_mode.load(std::memory_order_acquire) == AURORA_SMOOTH_LIVE && !g_held.load(std::memory_order_acquire) &&
         g_periodNs.load(std::memory_order_relaxed) > 0;
}

// render_worker's idle hook (live mode): shows the next picture when its slot has come
int64_t idle_tick() noexcept {
  if (!live_and_running() || !g_haveLayer || window::is_minimized()) {
    return 0;
  }
  const int64_t period = g_periodNs.load(std::memory_order_relaxed);
  int64_t now = now_ns();
  if (g_resetGrid.exchange(false, std::memory_order_acq_rel)) {
    g_nextSlotNs = now + period;
  }
  if (now < g_nextSlotNs) {
    // Not due yet. Measured 2026-11-16 (Windows, a 120 Hz screen): leaving the wait to the worker's own
    // condition-variable timeout gave about 110 pictures a second at both 120 and 240, 40-77 late pictures a second
    // and gaps up to 27 ms, because that wait sleeps in the system's coarse timer ticks (likely about 15.6 ms on
    // Windows unless the timer resolution is raised; Linux and macOS wait more precisely, unmeasured). So while the
    // queue is empty the presenter sleeps here itself, with SDL's precise sleep (a high-resolution waitable timer on
    // Windows), at most 1 ms at a time, and hands the worker back a 1 ns wait (a poll) so queued work never waits more
    // than that slice.
    const int64_t left = g_nextSlotNs - now;
    if (!gfx::render_worker::is_idle()) {
      return 1; // work is queued: the worker takes it first (a poll: try_pop), then comes back here
    }
    // *Changed 2026-11-16 (step c's pacing work):* a slice that ends well before the slot sleeps on the OS timer
    // (SDL_DelayNS: a high-resolution waitable timer on Windows, nanosleep elsewhere) instead of SDL_DelayPrecise,
    // which spins for any wait of 1 ms or less, so the worker no longer holds a core busy the whole time; only the last
    // half millisecond before the slot spins. Slices stay at most 1 ms, so queued work never waits longer than that.
    if (left > 1'500'000) {
      SDL_DelayNS(static_cast<Uint64>(std::min<int64_t>(left - 500'000, 1'000'000)));
    } else {
      SDL_DelayPrecise(static_cast<Uint64>(left));
    }
    now = now_ns();
    if (now < g_nextSlotNs) {
      return 1;
    }
  }
  if (g_stepShown && g_outpaced.load(std::memory_order_relaxed)) {
    return 500'000; // steps outpace pictures but none came yet: show the next one as soon as it comes
  }
  // pacing (step c): how late this slot is handled, and whether a queue item held the worker across it
  const int64_t late = now - g_nextSlotNs;
  if (late > 0) {
    store_max(g_count.worstLateNs, static_cast<uint64_t>(late));
  }
  if (late > 2'000'000) {
    g_count.lateStarts.fetch_add(1, std::memory_order_relaxed);
    // an item that started before the slot and ended after it held the worker across the slot (one that merely woke
    // the worker after the slot did not)
    const int64_t slotSteady = gfx::render_worker::steady_now_ns() - late;
    const auto item = gfx::render_worker::last_item();
    if (item.startNs < slotSteady && item.endNs > slotSteady) {
      g_count.lateByItem.fetch_add(1, std::memory_order_relaxed);
    }
  }
  {
    // 2026-11-17: where each picture falls between the two steps (0: the older step whole, 1: the newest whole, in
    // between: a real in-between), counted only while the steps have history (otherwise every picture is 1)
    retime_for_speed(now); // round 5: a speed change re-times the pair on screen at once
    const float fraction = live_fraction(now);
    if (g_history && g_stepDurNs > 0 && !g_outpaced.load(std::memory_order_relaxed)) {
      auto& bucket = fraction <= 0.f ? g_count.fracZero : fraction >= 1.f ? g_count.fracOne : g_count.fracMid;
      bucket.fetch_add(1, std::memory_order_relaxed);
    }
    // ORRE_SMOOTH_PICLOG=<from>-<to> (a test switch, read once; round 4, 2026-11-17; retrace counts of the kept step):
    // one line per live picture: its time after the kept step's arrival and anchor, the fraction, the history and the
    // step length, how late the slot was, and the gap since the last picture. Built to measure the first pictures
    // after fast-forward is let go (snatcherLeo: "a brief dizzy moment"). Observation only.
    static const std::pair<uint32_t, uint32_t> picLog = [] {
      unsigned from = 0, to = 0;
      const char* v = std::getenv("ORRE_SMOOTH_PICLOG");
      if (v == nullptr || std::sscanf(v, "%u-%u", &from, &to) != 2) {
        return std::pair<uint32_t, uint32_t>{1, 0};
      }
      return std::pair<uint32_t, uint32_t>{from, to};
    }();
    if (g_prevRetrace >= picLog.first && g_prevRetrace <= picLog.second) {
      std::fprintf(stderr,
                   "[smooth] piclog retrace %u: %.2f ms after its arrival, %+.2f ms from its anchor; fraction %.3f, "
                   "history %d, step %.2f ms, outpaced %d; slot late %.2f ms, %.2f ms since the last picture\n",
                   g_prevRetrace, (now - g_prevArrivalNs) / 1e6, (now - g_anchorNs) / 1e6, fraction, g_history ? 1 : 0,
                   g_stepDurNs / 1e6, g_outpaced.load(std::memory_order_relaxed) ? 1 : 0, (now - g_nextSlotNs) / 1e6,
                   g_lastPresentNs != 0 ? (now - g_lastPresentNs) / 1e6 : 0.0);
    }
    slot_picture(fraction);
  }
  advance_slot(now, period);
  // *Changed 2026-11-16 (step c's pacing work):* always a poll, never the time to the next slot. Returning that time
  // (8.3 ms at 120 Hz) let the worker wait for its next item in the queue's condition variable for up to 1 ms, and
  // that timed wait sleeps a whole Windows timer tick (MinGW's winpthreads; up to 15.6 ms): measured with the new
  // pacing counters, P01's first 3,000 frames at Match (120) in a normal window, 145-222 slots in every 10 s handled
  // over 2 ms late (up to 15.8 ms) while no queue item took over 1 ms (except one 17 ms pipeline build). The
  // presenter does its own sleeping above. Linux and macOS: the timed wait is likely precise there, so this only
  // costs a few extra wake-ups.
  return 1;
}
} // namespace smooth
#endif

void end_frame() noexcept {
  ZoneScoped;
#ifdef AURORA_ENABLE_GX
  gx::fifo::drain();
  gx::fifo::end_frame();
  gx::texture::end_frame();
  gfx::finish();
  auto imguiDrawData = imgui::freeze();

  const auto& presentSource = webgpu::present_source();
  /* OrrePort: the game picture's place (below a top strip kept free for an always-shown menu bar) */
  const auto viewport = webgpu::calculate_game_present_viewport(webgpu::g_graphicsConfig.surfaceConfiguration.width,
                                                                webgpu::g_graphicsConfig.surfaceConfiguration.height,
                                                                presentSource.size.width, presentSource.size.height);

  WindowLayer layer{.viewport = viewport, .imguiDrawData = std::move(imguiDrawData)};
#if AURORA_ENABLE_RMLUI
  if (rmlui::is_initialized()) {
    auto rmlFrame = rmlui::record_frame(viewport);
    layer.rmlBindGroup = std::move(rmlFrame.bindGroup);
    layer.rmlOverlay = rmlFrame.overlay;
    layer.rmlTarget = std::move(rmlFrame.target);
  }
#endif

  std::string dumpPrefix = orre_take_dump_request(); // OrrePort: aurora_orre_request_frame_dump (a test switch)
  // OrrePort, frame smoothing: the step's stamp and the in-between dump request (test switch), taken on this thread
  const uint32_t stepFlags = std::exchange(smooth::g_stampFlags, 0u);
  const uint32_t stepRetrace = smooth::g_stampRetrace;          // step (c): the clock and the history
  const uint64_t stepAnchorNs = std::exchange(smooth::g_stampAnchorNs, 0u);
  const uint64_t stepArrivalNs = std::exchange(smooth::g_stampArrivalNs, 0u);
  const uint64_t stepFieldNs = std::exchange(smooth::g_stampFieldNs, 0u); // round 5: the retrace length it ran at
  std::string smoothDump;
  {
    std::lock_guard lock{smooth::g_dumpLock};
    smoothDump = std::exchange(smooth::g_dumpPrefix, std::string{});
  }
  gfx::end_frame([layer = std::move(layer), dumpPrefix = std::move(dumpPrefix), stepFlags, stepRetrace, stepAnchorNs,
                  stepArrivalNs, stepFieldNs, smoothDump = std::move(smoothDump)](
                     wgpu::CommandEncoder& encoder, std::vector<gfx::AfterSubmitCallback> afterSubmitCallbacks) mutable {
    const auto& viewport = layer.viewport;
    if (!dumpPrefix.empty()) {
      // the frame buffer as rendered, then the same through the present resampler at its place in the window
      orre_dump_texture(encoder, webgpu::present_source(), dumpPrefix + "_render.ppm", afterSubmitCallbacks);
      const auto& shown = webgpu::resample_present_source(encoder, viewport);
      orre_dump_texture(encoder, shown, dumpPrefix + "_window.ppm", afterSubmitCallbacks);
      Log.info("frame dump {}: render {}x{}, window picture {}x{} at ({}, {}), surface {}x{}, resampler {}", dumpPrefix,
               webgpu::present_source().size.width, webgpu::present_source().size.height, viewport.width,
               viewport.height, viewport.left, viewport.top, webgpu::g_graphicsConfig.surfaceConfiguration.width,
               webgpu::g_graphicsConfig.surfaceConfiguration.height,
               webgpu::get_resampler() == SAMPLER_AREA ? "area" : "bilinear");
    }
    if (!smoothDump.empty()) {
      // OrrePort, frame smoothing (test switch): the step's own picture, to compare its in-betweens' pictures with
      orre_dump_texture(encoder, webgpu::present_source(), smoothDump + "_step.ppm", afterSubmitCallbacks);
      smooth::dump_ui(encoder, layer, smoothDump + "_step.ppm", afterSubmitCallbacks); // section 33: the step's UI
    }

    const int mode = smooth::g_mode.load(std::memory_order_acquire);
    if (mode == AURORA_SMOOTH_OFF) {
      // smoothing off: the step presents at once, as before
      auto surfaceStatus = wgpu::SurfaceGetCurrentTextureStatus::Error;
      const auto surfaceTexture = encode_window_picture(encoder, layer, surfaceStatus);
      submit_and_present(encoder, surfaceTexture, surfaceStatus, true, true, afterSubmitCallbacks);
      smooth::g_haveLayer = false;
      smooth::g_stepShown = true;
      smooth::g_prevAnchorNs = 0;
      smooth::g_prevStepFlags = AURORA_SMOOTH_STEP_RESET;
    } else {
      // frame smoothing: this step becomes the kept one (gfx::end_frame kept its recording just before this call)
      smooth::g_count.steps.fetch_add(1, std::memory_order_relaxed);
      smooth::g_layer = std::move(layer);
      smooth::g_haveLayer = true;
      smooth::g_keptFlags = stepFlags;
      const bool live = smooth::live_and_running();
      const bool held = smooth::g_held.load(std::memory_order_acquire);
      // Step (c), the clock and the history (smooth_camera.hpp): the newest step's cameras blend from the step
      // before's only when nothing came between them that hides a cut: a reset the port asked for (room change, speed
      // change), a step that couldn't be drawn again on either side, a pause, or a gap (over 250 ms on the screen's
      // clock in live mode; over 8 retraces of game time in both modes, so the test mode's choices depend only on
      // the game, never on the host's timing).
      constexpr uint32_t NotReplayable =
          AURORA_SMOOTH_STEP_NO_COPY | AURORA_SMOOTH_STEP_MOVIE | AURORA_SMOOTH_STEP_PAUSE | AURORA_SMOOTH_STEP_RESET;
      const uint32_t retraceGap = stepRetrace - smooth::g_prevRetrace;
      // the anchor (above, "The anchor lead"): live only, and only for a stamped step
      const int64_t retraceAnchor = static_cast<int64_t>(stepAnchorNs);
      const int64_t arrival = static_cast<int64_t>(stepArrivalNs);
      // the steps' even spacing on screen: the gap between their retrace anchors (game time at the current speed)
      const int64_t retraceGapNs =
          smooth::g_prevRetraceAnchorNs != 0 ? retraceAnchor - smooth::g_prevRetraceAnchorNs : 0;
      int64_t anchor = retraceAnchor;
      // ORRE_SMOOTH_RETRACE_ANCHOR=1 (a test switch, read once): step (c)'s retrace anchor, for before / after
      // measurements with the same exe
      static const bool retraceAnchorTest = [] {
        const char* v = std::getenv("ORRE_SMOOTH_RETRACE_ANCHOR");
        return v != nullptr && v[0] == '1';
      }();
      // *Changed 2026-11-17 (round 4):* a reset concerns the step that carries it (nothing before it may blend into
      // it), not the step after: that one blends from the reset step as usual. Before, the step after a reset was
      // refused history too (and its anchor restarted), so every reset showed two steps whole: after a room change,
      // and after letting go of fast-forward (measured: two 30-a-second steps unblended, about 67 ms). A step that
      // couldn't be drawn again (no copy, a movie, a pause) still breaks history on both sides.
      constexpr uint32_t PrevBreaks = AURORA_SMOOTH_STEP_NO_COPY | AURORA_SMOOTH_STEP_MOVIE | AURORA_SMOOTH_STEP_PAUSE;
      // ---- Round 5 (2026-11-17): the step's length from the known speed. ----
      // A pair's length is the game time between its two steps: their retrace gap times the length of a retrace at the
      // speed the step ran at (the port stamps it). Round 4 measured it from the two steps' retrace due times instead,
      // which across a speed change mixes the two speeds (the last fast step's due time was 5.6 ms after it at 3x, the
      // first slow step's 16.7 ms), so the first slow pair ran too long or too short. With equal speeds both give the
      // same length. And the next pair starts where the pair before ends (its anchor plus its own length, re-timed by
      // a speed change: retime_for_speed), not one new length after it: equal at a steady speed, continuous at a change.
      if (live) {
        smooth::retime_for_speed(smooth::now_ns()); // a change seen before any picture since: re-time the pair first
      }
      const int64_t nominalNs = stepFieldNs > 0 && retraceGap >= 1 && retraceGap <= 8
                                    ? static_cast<int64_t>(retraceGap) * static_cast<int64_t>(stepFieldNs)
                                    : retraceGapNs;
      if (live && retraceAnchor != 0 && arrival != 0) {
        constexpr uint32_t Fresh = AURORA_SMOOTH_STEP_NO_COPY | AURORA_SMOOTH_STEP_MOVIE | AURORA_SMOOTH_STEP_PAUSE |
                                   AURORA_SMOOTH_STEP_RESET;
        const bool fresh = smooth::g_prevAnchorNs == 0 || retraceGapNs <= 0 || retraceGapNs > smooth::HistoryGapNs ||
                           (stepFlags & Fresh) != 0 || (smooth::g_prevStepFlags & PrevBreaks) != 0 || held;
        // where the pair before ends (round 5); before a pair has a length (after a reset), one new length
        const int64_t expected =
            smooth::g_prevAnchorNs + (smooth::g_history && smooth::g_stepDurNs > 0 ? smooth::g_stepDurNs : nominalNs);
        if (retraceAnchorTest) {
          anchor = retraceAnchor;
        } else if (fresh) {
          // after a reset, a pause or a gap the anchor starts again at the arrival (the camera snaps there anyway)
          anchor = arrival + smooth::AnchorMarginNs;
        } else if (arrival >= expected) {
          anchor = arrival; // late: the step can't be shown before it exists (a small jump forward at its first picture)
          smooth::g_count.lateArrivals.fetch_add(arrival > expected ? 1u : 0u, std::memory_order_relaxed);
        } else if (expected - (arrival + smooth::AnchorMarginNs) > smooth::ReanchorNs) {
          // far too early: the even spacing has drifted well past the arrivals (measured 2026-11-17: after a
          // replay's switch from full speed to real time the anchors sat 40 ms after the arrivals in a 30-a-second
          // scene, and drawing them in by 0.25 ms a step would take seconds): start again at the arrival, one small
          // jump forward
          anchor = arrival + smooth::AnchorMarginNs;
          smooth::g_count.reanchors.fetch_add(1, std::memory_order_relaxed);
        } else {
          // early: keep the even spacing, drawn toward the arrival plus a margin by at most LeadStepNs a step.
          // *Tuned 2026-11-17 (measured):* drawn toward the arrival itself, 15-30 % of the steps then came after
          // their anchor (each a small jump forward), the anchor hugging the arrivals' jitter; the margin keeps the
          // anchor just above it for 1 ms more of delay
          anchor = std::max(arrival + smooth::AnchorMarginNs, expected - smooth::LeadStepNs);
          anchor = std::min(anchor, expected);
        }
        // round 5: the step's game work, from its last retrace's due time (the due time stamped is the next retrace's)
        // to its arrival, averaged: retime_for_speed predicts the next step's arrival with it
        if (stepFieldNs > 0) {
          const int64_t work = arrival - (retraceAnchor - static_cast<int64_t>(stepFieldNs));
          if (work > 0 && work < 50'000'000) {
            smooth::g_workNs += (work - smooth::g_workNs) / 4;
          }
        }
        smooth::g_leadNs = retraceAnchor - anchor; // how much earlier than step (c)'s anchor (the log)
        // ORRE_SMOOTH_ANCHORLOG=<from>-<to> (a test switch, read once; retrace counts): one line per step, the clock's
        // inputs and choice, relative to the step's arrival
        static const std::pair<uint32_t, uint32_t> anchorLog = [] {
          unsigned from = 0, to = 0;
          const char* v = std::getenv("ORRE_SMOOTH_ANCHORLOG");
          if (v == nullptr || std::sscanf(v, "%u-%u", &from, &to) != 2) {
            return std::pair<uint32_t, uint32_t>{1, 0};
          }
          return std::pair<uint32_t, uint32_t>{from, to};
        }();
        if (stepRetrace >= anchorLog.first && stepRetrace <= anchorLog.second) {
          std::fprintf(stderr,
                       "[smooth] anchorlog retrace %u flags %u: since the last arrival %.2f ms, retrace anchor +%.2f "
                       "ms, expected %+.2f ms, anchor %+.2f ms, retrace gap %.2f ms, fresh %d\n",
                       stepRetrace, stepFlags, (arrival - smooth::g_prevArrivalNs) / 1e6,
                       (retraceAnchor - arrival) / 1e6, (expected - arrival) / 1e6, (anchor - arrival) / 1e6,
                       retraceGapNs / 1e6, fresh ? 1 : 0);
        }
        // the delay measure: the step before is shown whole at this step's anchor (where its pair's fraction reaches
        // 1), counted only while the two belong together (the history test below uses the same gap rules)
        if (smooth::g_prevArrivalNs != 0 && anchor > smooth::g_prevArrivalNs &&
            anchor - smooth::g_prevArrivalNs < smooth::HistoryGapNs && !smooth::g_outpaced.load()) {
          const auto delay = static_cast<uint64_t>(anchor - smooth::g_prevArrivalNs);
          smooth::g_count.delaySumNs.fetch_add(delay, std::memory_order_relaxed);
          smooth::store_max(smooth::g_count.delayMaxNs, delay);
          smooth::g_count.delayRetSumNs.fetch_add(
              static_cast<uint64_t>(retraceAnchor - smooth::g_prevArrivalNs), std::memory_order_relaxed);
          smooth::g_count.delayCount.fetch_add(1, std::memory_order_relaxed);
        }
        smooth::g_prevArrivalNs = arrival;
      }
      smooth::g_prevRetraceAnchorNs = retraceAnchor;
      const int64_t anchorGap = anchor - smooth::g_prevAnchorNs;
      bool history = (stepFlags & NotReplayable) == 0 && (smooth::g_prevStepFlags & PrevBreaks) == 0 && !held &&
                     retraceGap >= 1 && retraceGap <= 8;
      if (live) {
        // *Changed 2026-11-17:* the gap and the step's length are the retrace anchors' (even), not the moved
        // anchors' (a late step's anchor is its arrival, which would make the next step's blend run fast)
        history = history && smooth::g_prevAnchorNs != 0 && anchorGap > 0 && retraceGapNs > 0 &&
                  retraceGapNs <= smooth::HistoryGapNs;
      }
      smooth::g_history = history;
      // round 5: the pair's length from the known speed (nominalNs, above); the speed it was worked out at
      smooth::g_stepDurNs = history ? (live ? nominalNs : anchorGap) : 0;
      smooth::g_pairFieldNs = stepFieldNs;
      smooth::g_anchorNs = anchor;
      smooth::g_prevAnchorNs = anchor;
      smooth::g_prevRetrace = stepRetrace;
      smooth::g_prevStepFlags = stepFlags;
      gfx::plan_kept_step(history, stepRetrace);
      if (live) {
        if (smooth::g_resetGrid.exchange(false, std::memory_order_acq_rel)) {
          smooth::g_nextSlotNs = smooth::now_ns();
        }
        if (!smooth::g_stepShown) {
          smooth::g_count.stepsNotShown.fetch_add(1, std::memory_order_relaxed); // replaced before its slot
        }
        // step (c): the step's own work is submitted now and its pictures come at the slots (slot_picture), one step
        // behind, so they can blend from the previous step's camera
        submit_and_present(encoder, {}, wgpu::SurfaceGetCurrentTextureStatus::Error, false, true,
                           afterSubmitCallbacks);
        smooth::g_stepShown = false;
      } else {
        // the test mode (and a pause): the step's own picture at once, its in-betweens after it
        smooth::present_picture(encoder, true, afterSubmitCallbacks);
        smooth::g_count.stepPictures.fetch_add(1, std::memory_order_relaxed);
        smooth::g_stepShown = true;
      }
      if (!smoothDump.empty()) {
        smooth::g_ibDumpPrefix = smoothDump;
        smooth::g_ibDumpIndex = 0;
        smooth::g_ibDumpLeft =
            mode == AURORA_SMOOTH_FORCED ? smooth::g_forcedCount.load(std::memory_order_relaxed) : 1;
      }
      if (mode == AURORA_SMOOTH_FORCED && !held) {
        // the in-betweens of the test mode: evenly between the previous step's camera and this step's (n = 1: half
        // way; n = 3: a quarter, a half, three quarters), as a screen n + 1 times the step rate would show them
        const uint32_t count = smooth::g_forcedCount.load(std::memory_order_relaxed);
        // ORRE_SMOOTH_TEST_FRACTION=<0-1> (a test switch, read once; round 5, 2026-11-17): every in-between of the test
        // mode at that fraction instead (1: the step replayed unblended), to tell a replay problem from a blend problem
        static const float testFraction = [] {
          const char* v = std::getenv("ORRE_SMOOTH_TEST_FRACTION");
          return v != nullptr ? std::clamp(static_cast<float>(std::atof(v)), 0.f, 1.f) : -1.f;
        }();
        for (uint32_t i = 0; i < count; ++i) {
          const float f = testFraction >= 0.f ? testFraction : static_cast<float>(i + 1) / static_cast<float>(count + 1);
          smooth::in_between_picture(history ? f : 1.f);
        }
        // ORRE_SMOOTH_TEST_ATONE=1 (a test switch, item c, 2026-11-17): one more picture at fraction 1 after them, as
        // the live presenter draws at the step's own slot (a replay only while a menu model is in the first half of
        // its two-step blend; otherwise the step's own picture again), dumped as the next ib file, so the dumps hold
        // the whole live sequence
        static const bool atOne = [] {
          const char* v = std::getenv("ORRE_SMOOTH_TEST_ATONE");
          return v != nullptr && v[0] == '1';
        }();
        if (atOne && history && testFraction < 0.f) {
          if (!smoothDump.empty()) {
            ++smooth::g_ibDumpLeft;
          }
          smooth::in_between_picture(1.f);
        }
      }
    }

    TracyPlotConfig("aurora: lastVertSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastUniformSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastIndexSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastStorageSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastTextureUploadSize", tracy::PlotFormatType::Memory, false, true, 0);

    const auto& stats = gfx::detail::resources().stats;
    TracyPlot("aurora: queuedPipelines", static_cast<int64_t>(stats.queuedPipelines));
    TracyPlot("aurora: createdPipelines", static_cast<int64_t>(stats.createdPipelines));
    TracyPlot("aurora: drawCallCount", static_cast<int64_t>(stats.drawCallCount));
    TracyPlot("aurora: mergedDrawCallCount", static_cast<int64_t>(stats.mergedDrawCallCount));
    TracyPlot("aurora: lastVertSize", static_cast<int64_t>(stats.lastVertSize));
    TracyPlot("aurora: lastUniformSize", static_cast<int64_t>(stats.lastUniformSize));
    TracyPlot("aurora: lastIndexSize", static_cast<int64_t>(stats.lastIndexSize));
    TracyPlot("aurora: lastStorageSize", static_cast<int64_t>(stats.lastStorageSize));
    TracyPlot("aurora: lastTextureUploadSize", static_cast<int64_t>(stats.lastTextureUploadSize));
  });

#endif
}

// OrrePort: re-present the last finished frame (with ImGui over it) without touching the game's open frame. The
// game's drawing is recorded during a frame and only executed in end_frame, so while the game is held mid-frame (the
// debug toolkit's pause) the frame buffer still holds the last presented picture, at full resolution with any
// replaced textures. overlay_frame_begin starts an ImGui frame; overlay_frame_present waits for the render worker to
// go idle, then blits the frame buffer and draws ImGui exactly as end_frame does.
void keep_efb_next_frame() noexcept {
#ifdef AURORA_ENABLE_GX
  gfx::detail::keep_efb_next_frame();
#endif
}

void overlay_frame_begin() noexcept {
#ifdef AURORA_ENABLE_GX
  imgui::new_frame(window::get_window_size());
#endif
}

bool overlay_frame_present() noexcept {
#ifdef AURORA_ENABLE_GX
  auto imguiDrawData = imgui::freeze();
  gfx::gpu_synchronize();
  if (!window::is_presentable() || !g_surface) {
    return false;
  }
  const auto& presentSource = webgpu::present_source();
  const auto viewport = webgpu::calculate_game_present_viewport(webgpu::g_graphicsConfig.surfaceConfiguration.width,
                                                                webgpu::g_graphicsConfig.surfaceConfiguration.height,
                                                                presentSource.size.width, presentSource.size.height);
  const wgpu::CommandEncoderDescriptor encoderDescriptor{.label = "Overlay present encoder"};
  auto encoder = webgpu::g_device.CreateCommandEncoder(&encoderDescriptor);
  wgpu::Texture currentTexture;
  wgpu::TextureView currentView;
  {
    window::SurfaceLock surfaceLock;
    wgpu::SurfaceTexture surfaceTexture;
    g_surface.GetCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
      return false;
    }
    currentTexture = std::move(surfaceTexture.texture);
    currentView = currentTexture.CreateView();
  }
  {
    const auto& resampledSource = webgpu::resample_present_source(encoder, viewport);
    const auto presentBindGroup = webgpu::create_copy_bind_group(resampledSource);
    const std::array attachments{
        wgpu::RenderPassColorAttachment{
            .view = currentView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
        },
    };
    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .label = "Overlay present blit",
        .colorAttachmentCount = attachments.size(),
        .colorAttachments = attachments.data(),
    };
    const auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(webgpu::g_CopyPipeline);
    pass.SetBindGroup(0, presentBindGroup, 0, nullptr);
    set_present_viewport(pass, viewport, webgpu::g_graphicsConfig.surfaceConfiguration.width,
                         webgpu::g_graphicsConfig.surfaceConfiguration.height);
    pass.Draw(3);
    pass.End();
  }
  {
    const std::array attachments{
        wgpu::RenderPassColorAttachment{
            .view = currentView,
            .loadOp = wgpu::LoadOp::Load,
            .storeOp = wgpu::StoreOp::Store,
        },
    };
    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .label = "Overlay ImGui",
        .colorAttachmentCount = attachments.size(),
        .colorAttachments = attachments.data(),
    };
    const auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetViewport(0.f, 0.f, static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.width),
                     static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.height), 0.f, 1.f);
    imgui::render(pass, imguiDrawData);
    pass.End();
  }
  const wgpu::CommandBufferDescriptor cmdBufDescriptor{.label = "Overlay command buffer"};
  const auto buffer = encoder.Finish(&cmdBufDescriptor);
  g_queue.Submit(1, &buffer);
  {
    window::SurfaceLock surfaceLock;
    if (window::is_presentable()) {
      return static_cast<bool>(wgpu::ConvertibleStatus(g_surface.Present()));
    }
  }
#endif
  return false;
}
} // namespace
} // namespace aurora

// C API bindings
AuroraInfo aurora_initialize(int argc, char* argv[], const AuroraConfig* config) {
  return aurora::initialize(argc, argv, *config);
}
void aurora_shutdown() { aurora::shutdown(); }
const AuroraEvent* aurora_update() { return aurora::update(); }
bool aurora_begin_frame() { return aurora::begin_frame(); }
void aurora_end_frame() { aurora::end_frame(); }
void aurora_keep_efb_next_frame() { aurora::keep_efb_next_frame(); }
void aurora_overlay_frame_begin() { aurora::overlay_frame_begin(); }
bool aurora_overlay_frame_present() { return aurora::overlay_frame_present(); }
SDL_Window* aurora_get_window() { return aurora::window::get_sdl_window(); }
AuroraWindowSize aurora_get_window_size() {
  return aurora::window::get_sdl_window() != nullptr ? aurora::window::get_window_size() : AuroraWindowSize{};
}
AuroraBackend aurora_get_backend() { return aurora::g_config.desiredBackend; }
const AuroraBackend* aurora_get_available_backends(size_t* count) {
  if (count != nullptr) {
    *count = aurora::PreferredBackendOrder.size();
  }
  return aurora::PreferredBackendOrder.data();
}
void aurora_set_log_level(AuroraLogLevel level) { aurora::g_config.logLevel = level; }
void aurora_set_pause_on_focus_lost(bool value) { aurora::g_config.pauseOnFocusLost = value; }
void aurora_set_background_input(bool value) {
  aurora::g_config.allowJoystickBackgroundEvents = value;
  aurora::window::set_background_input(value);
}
void aurora_set_resampler(AuroraSampler sampler) {
#ifdef AURORA_ENABLE_GX
  aurora::webgpu::set_resampler(sampler);
#else
  (void)sampler;
#endif
}
void aurora_set_timescale(float scale) { aurora::time::set_scale(scale); }
void aurora_set_present_inset_top(uint32_t pixels) { aurora::webgpu::set_present_inset_top(pixels); }
void aurora_orre_request_frame_dump(const char* pathPrefix) {
#ifdef AURORA_ENABLE_GX
  std::lock_guard lock{aurora::g_orreDumpLock};
  aurora::g_orreDumpPrefix = pathPrefix != nullptr ? pathPrefix : "";
#else
  (void)pathPrefix;
#endif
}
float aurora_get_timescale() { return aurora::time::scale(); }

// OrrePort, frame smoothing steps (a) and (b) (aurora.h; the presenter above). Without GX they do nothing.
void aurora_smooth_configure(int mode, uint32_t pictureHz, uint32_t inBetweens) {
#ifdef AURORA_ENABLE_GX
  namespace s = aurora::smooth;
  s::g_periodNs.store(pictureHz > 0 ? static_cast<int64_t>(1'000'000'000ll / pictureHz) : 0, std::memory_order_relaxed);
  s::g_forcedCount.store(inBetweens, std::memory_order_relaxed);
  s::g_resetGrid.store(true, std::memory_order_release);
  s::g_mode.store(mode, std::memory_order_release);
  aurora::gfx::set_keep_steps(mode != AURORA_SMOOTH_OFF);
  aurora::gfx::render_worker::set_idle_hook(mode == AURORA_SMOOTH_LIVE ? s::idle_tick : nullptr);
#else
  (void)mode, (void)pictureHz, (void)inBetweens;
#endif
}
void aurora_smooth_set_outpaced(bool outpaced) {
#ifdef AURORA_ENABLE_GX
  aurora::smooth::g_outpaced.store(outpaced, std::memory_order_release);
#else
  (void)outpaced;
#endif
}
void aurora_smooth_hold(bool held) {
#ifdef AURORA_ENABLE_GX
  if (!aurora::smooth::g_held.exchange(held, std::memory_order_acq_rel) == !held) {
    return; // unchanged
  }
  if (!held) {
    aurora::smooth::g_resetGrid.store(true, std::memory_order_release); // the grid restarts after a pause
  }
#else
  (void)held;
#endif
}
void aurora_smooth_step(uint32_t retrace, uint32_t flags, uint64_t anchorNs) {
#ifdef AURORA_ENABLE_GX
  aurora::smooth::g_stampRetrace = retrace; // step (c): the history's gap check (game time)
  aurora::gfx::orre_draw_log_stamp(retrace); // fix (3)'s draw log (a test switch): which steps to log
  aurora::smooth::g_stampFlags = flags;
  aurora::smooth::g_stampAnchorNs = anchorNs; // step (c): the clock (screen time)
  // 2026-11-17: when the step's picture work is done (the delay measure and the anchor lead, end_frame's job)
  aurora::smooth::g_stampArrivalNs = SDL_GetTicksNS();
  // round 5: the retrace length at the speed this step ran at (aurora_smooth_field_ns)
  aurora::smooth::g_stampFieldNs = aurora::smooth::g_fieldNs.load(std::memory_order_relaxed);
#else
  (void)retrace, (void)flags, (void)anchorNs;
#endif
}
void aurora_smooth_field_ns(uint64_t fieldNs, uint64_t nextStepDueNs) {
#ifdef AURORA_ENABLE_GX
  // the prediction first, so the render worker never sees the new length with the old prediction
  aurora::smooth::g_nextStepDueNs.store(nextStepDueNs, std::memory_order_relaxed);
  aurora::smooth::g_fieldNs.store(fieldNs, std::memory_order_release);
#else
  (void)fieldNs, (void)nextStepDueNs;
#endif
}
void aurora_smooth_last_cut(AuroraSmoothCut* out) {
  if (out == nullptr) {
    return;
  }
  *out = {};
#ifdef AURORA_ENABLE_GX
  const auto cut = aurora::gfx::smooth_camera::last_cut();
  out->sequence = cut.sequence;
  out->reason = static_cast<int>(cut.reason);
  out->retrace = cut.retrace;
  out->degrees = cut.degrees;
  out->eyeJump = cut.eyeJump;
  out->targetJump = cut.targetJump;
  out->distance = cut.distance;
  out->fovRatio = cut.fovRatio;
#endif
}
void aurora_smooth_wait(void) {
#ifdef AURORA_ENABLE_GX
  if (aurora::smooth::g_mode.load(std::memory_order_acquire) == AURORA_SMOOTH_FORCED) {
    aurora::gfx::gpu_synchronize(); // the render worker has finished the step and its in-betweens
  }
#endif
}
bool aurora_smooth_in_between_now(void) {
#ifdef AURORA_ENABLE_GX
  const int mode = aurora::smooth::g_mode.load(std::memory_order_acquire);
  if (mode == AURORA_SMOOTH_OFF) {
    return false;
  }
  // live: any game code on the render worker is a mistake; forced: also any game code anywhere while an in-between
  // picture is open (the game thread itself waits in aurora_smooth_wait then)
  return aurora::gfx::render_worker::is_worker_thread() ||
         (mode == AURORA_SMOOTH_FORCED && aurora::smooth::g_replayOpen.load(std::memory_order_acquire) > 0);
#else
  return false;
#endif
}
void aurora_smooth_test_wipe(int mode) {
#ifdef AURORA_ENABLE_GX
  aurora::smooth::g_testWipe.store(mode, std::memory_order_relaxed);
#else
  (void)mode;
#endif
}
void aurora_smooth_request_dump(const char* prefix) {
#ifdef AURORA_ENABLE_GX
  std::lock_guard lock{aurora::smooth::g_dumpLock};
  aurora::smooth::g_dumpPrefix = prefix != nullptr ? prefix : "";
#else
  (void)prefix;
#endif
}
void aurora_smooth_get_stats(AuroraSmoothStats* out) {
  if (out == nullptr) {
    return;
  }
  *out = {};
#ifdef AURORA_ENABLE_GX
  const auto& c = aurora::smooth::g_count;
  constexpr auto r = std::memory_order_relaxed;
  out->steps = c.steps.load(r);
  out->stepPictures = c.stepPictures.load(r);
  out->stepsNotShown = c.stepsNotShown.load(r);
  out->inBetweens = c.inBetweens.load(r);
  for (int i = 0; i < AURORA_SMOOTH_HELD_COUNT; ++i) {
    out->held[i] = c.held[i].load(r);
  }
  out->notPresented = c.notPresented.load(r);
  out->slips = c.slips.load(r);
  out->replayNs = c.replayNs.load(r);
  out->replays = c.replays.load(r);
  out->replayDraws = c.replayDraws.load(r);
  out->presentNs = c.presentNs.load(r);
  out->presents = c.presents.load(r);
  out->worstGapNs = aurora::smooth::g_count.worstGapNs.exchange(0, r);
  // step (c)
  auto& m = aurora::smooth::g_count;
  out->blended = c.blended.load(r);
  out->repeats = c.repeats.load(r);
  out->restores = aurora::gfx::kept_step_restores();
  const auto camera = aurora::gfx::smooth_camera::stats();
  out->cameras = camera.cameras;
  for (int i = 0; i < AURORA_SMOOTH_CUT_COUNT; ++i) {
    out->cuts[i] = camera.cuts[i];
  }
  out->reaimedBlocks = camera.blocks;
  out->looseBlocks = camera.looseBlocks;
  out->overflowBlocks = camera.overflow;
  out->blendBytes = camera.bytes;
  out->modelTagged = camera.modelTagged;
  out->modelUntagged = camera.modelUntagged;
  out->modelMatched = camera.modelMatched;
  out->modelMoved = camera.modelMoved;
  out->modelSnapped = camera.modelSnapped;
  out->modelHeld = camera.modelHeld;
  out->modelsSnapped = camera.modelsSnapped; // round 4 (2026-11-17)
  out->modelBorrowed = camera.modelBorrowed;
  out->captureCopies = aurora::gfx::capture_copies();
  out->texFollows = camera.texFollows; // round 5
  out->modelBlockWrites = camera.modelBlockWrites;
  out->looseSameFrustum = camera.looseSameFrustum;
  out->looseTagged = camera.looseTagged;
  out->particleTagged = camera.particleTagged; // item (a), 2026-11-17
  out->particleBlended = camera.particleBlended;
  out->particleNew = camera.particleNew;
  out->particleReused = camera.particleReused;
  out->particleJumped = camera.particleJumped;
  out->particleHeld = camera.particleHeld;
  out->scrollBlended = camera.scrollBlended; // item (b), 2026-11-17
  out->scrollWrapped = camera.scrollWrapped;
  out->scrollSnapped = camera.scrollSnapped;
  out->scrollUnsteady = camera.scrollUnsteady; // round 9
  out->lockedBlocks = camera.lockedBlocks;     // round 10
  out->p2Tagged = camera.p2Tagged;             // round 11
  out->p2Blended = camera.p2Blended;
  out->p2New = camera.p2New;
  out->p2Swapped = camera.p2Swapped;
  out->p2Written = camera.p2Written;
  out->p2NoSpan = camera.p2NoSpan;
  out->p2VtxDraws = camera.p2VtxDraws; // round 11, vertex mode
  out->p2VtxSnapped = camera.p2VtxSnapped;
  out->p2MtxBlocks = camera.p2MtxBlocks;
  out->p2VtxWritten = camera.p2VtxWritten;
  out->twoStepBlocks = camera.twoStepBlocks; // item (c), 2026-11-17
  out->twoStepModels = camera.twoStepModels;
  out->colorBlended = camera.colorBlended; // the colour-data blend, 2026-11-18
  out->colorHeld = camera.colorHeld;
  out->cornerParticles = camera.cornerParticles; // (3b), 2026-11-18
  out->ibBlended = c.ibBlended.load(r);
  out->ibModels = c.ibModels.load(r);
  out->lateStarts = m.lateStarts.exchange(0, r);
  out->lateByItem = m.lateByItem.exchange(0, r);
  out->worstLateNs = m.worstLateNs.exchange(0, r);
  out->worstPictureNs = m.worstPictureNs.exchange(0, r);
  out->worstAcquireNs = aurora::smooth_timing::g_acquireMaxNs.exchange(0, r);
  out->worstPresentNs = aurora::smooth_timing::g_presentMaxNs.exchange(0, r);
  out->worstSubmitNs = aurora::smooth_timing::g_submitMaxNs.exchange(0, r);
  out->delaySumNs = m.delaySumNs.exchange(0, r);
  out->delayMaxNs = m.delayMaxNs.exchange(0, r);
  out->delayRetSumNs = m.delayRetSumNs.exchange(0, r);
  out->delayCount = m.delayCount.exchange(0, r);
  out->lateArrivals = m.lateArrivals.exchange(0, r);
  out->fracZero = m.fracZero.exchange(0, r);
  out->fracMid = m.fracMid.exchange(0, r);
  out->fracOne = m.fracOne.exchange(0, r);
  out->reanchors = m.reanchors.exchange(0, r);
  out->retimes = m.retimes.exchange(0, r); // round 5
  out->leadNs = aurora::smooth::g_leadNs; // a plain read of the render worker's value (log only)
  const auto items = aurora::gfx::render_worker::item_timing(true);
  for (int i = 0; i < 5; ++i) {
    out->itemMaxNs[i] = items.maxNs[i];
    out->itemTotalNs[i] = items.totalNs[i];
    out->itemCount[i] = items.count[i];
  }
#endif
}
const char* aurora_smooth_present_mode(void) {
#ifdef AURORA_ENABLE_GX
  switch (aurora::webgpu::g_graphicsConfig.surfaceConfiguration.presentMode) {
  case wgpu::PresentMode::Fifo:
    return "Fifo (V-Sync)";
  case wgpu::PresentMode::FifoRelaxed:
    return "FifoRelaxed";
  case wgpu::PresentMode::Immediate:
    return "Immediate";
  case wgpu::PresentMode::Mailbox:
    return "Mailbox";
  default:
    return "unknown";
  }
#else
  return "none";
#endif
}
