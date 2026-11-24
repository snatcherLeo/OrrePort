#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace aurora::gfx::render_worker {

enum class ItemType : uint8_t {
  BeginFrame,
  EncodePass,
  EndFrame,
  Sync,
  Shutdown,
};

using WorkCallback = std::function<void()>;

struct SyncState {
  std::mutex mutex;
  std::condition_variable cv;
  bool complete = false;
};

struct QueueItem {
  ItemType type = ItemType::Sync;
  uint64_t frameId = 0;
  uint32_t passIndex = 0;
  WorkCallback work;
  std::shared_ptr<SyncState> sync;
};

class BoundedQueue {
public:
  explicit BoundedQueue(size_t capacity);

  bool push(QueueItem item);
  std::optional<QueueItem> pop_for(std::chrono::nanoseconds timeout, bool& closed);
  std::optional<QueueItem> try_pop(bool& closed); // OrrePort, frame smoothing: no wait at all
  void close();
  void reset();
  [[nodiscard]] size_t size() const;

private:
  size_t m_capacity = 0;
  mutable std::mutex m_mutex;
  std::condition_variable m_notEmpty;
  std::condition_variable m_notFull;
  std::deque<QueueItem> m_items;
  bool m_closed = false;
};

class FrameSlotPool {
public:
  explicit FrameSlotPool(size_t slotCount);

  size_t acquire();
  std::optional<size_t> try_acquire();
  void release(size_t slot);
  void reset();
  [[nodiscard]] size_t free_count() const;

private:
  mutable std::mutex m_mutex;
  std::condition_variable m_cv;
  std::vector<bool> m_freeSlots;
};

void initialize();
void shutdown();

void enqueue_begin_frame(uint64_t frameId, WorkCallback work);
void enqueue_encode_pass(uint64_t frameId, uint32_t passIndex, WorkCallback work);
void enqueue_end_frame(uint64_t frameId, WorkCallback work);
void enqueue_work(WorkCallback work);
void synchronize();

bool is_worker_thread() noexcept;
bool is_idle() noexcept;

// OrrePort, frame smoothing (docs/port/frame-smoothing-plan.md, step a; 2026-11-16): a function the worker calls between
// queue items and on idle wake-ups, on the worker thread. It returns how many nanoseconds until it wants to run again
// (<= 0: no wish; the worker still wakes every 1 ms). The presenter (aurora.cpp) uses it to show in-between pictures
// at the screen's pace without a thread of its own: pictures stay on the thread that already presents, so they never
// race the steps' own presents and never call game code. Platform-neutral.
using IdleHook = int64_t (*)() noexcept;
void set_idle_hook(IdleHook hook) noexcept;

// OrrePort (2026-11-20, a crash on snatcherLeo's start-up): synchronize() waits for the queue only. Right after its sync item
// the worker runs the idle hook again, so a caller that then replaces the surface or the frame buffers (resize_swapchain,
// refresh_surface, release_surface) raced the presenter's in-between picture: NVIDIA's Vulkan driver read freed memory
// inside Queue::Submit (present_picture <- slot_picture <- idle_tick) right after "Frame buffers ... -> ...". An
// IdleHookPause held around such a change keeps the hook from running; synchronize() inside it guarantees a hook call
// already running has finished (the worker takes the sync item only between hook calls). Nests; any thread but the
// worker. Platform-neutral (the race is the same on every OS; it showed on Windows with NVIDIA).
struct IdleHookPause {
  IdleHookPause() noexcept;
  ~IdleHookPause();
  IdleHookPause(const IdleHookPause&) = delete;
  IdleHookPause& operator=(const IdleHookPause&) = delete;
};

// OrrePort, frame smoothing's pacing (frame-smoothing-plan.md section 13; 2026-11-16): per item kind (ItemType's
// order), the longest item, the total time and the count since the last reset, and the last item's start and end
// (steady clock), so the presenter can tell whether an item held the worker past a picture's slot. Platform-neutral.
struct ItemTiming {
  int64_t maxNs[5]{};
  int64_t totalNs[5]{};
  uint64_t count[5]{};
};
ItemTiming item_timing(bool reset) noexcept;
struct LastItem {
  int64_t startNs = 0;
  int64_t endNs = 0;
  ItemType type = ItemType::BeginFrame;
};
LastItem last_item() noexcept;
int64_t steady_now_ns() noexcept;

} // namespace aurora::gfx::render_worker
