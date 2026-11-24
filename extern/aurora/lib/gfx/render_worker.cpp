#include "render_worker.hpp"

#include "../thread.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>

#include <tracy/Tracy.hpp>

namespace aurora::gfx::render_worker {
namespace {
constexpr size_t QueueCapacity = 256;
constexpr auto IdlePumpInterval = std::chrono::milliseconds{1};

BoundedQueue g_queue{QueueCapacity};
thread::Thread g_thread;
std::atomic_bool g_running = false;
std::atomic_size_t g_pendingItems = 0;
std::thread::id g_workerThreadId;
// OrrePort, frame smoothing (render_worker.hpp, set_idle_hook): the presenter, run between queue items
std::atomic<IdleHook> g_idleHook = nullptr;
// OrrePort (render_worker.hpp, IdleHookPause): while above 0 the worker doesn't call the idle hook
std::atomic<int> g_idleHookPaused = 0;
// OrrePort, frame smoothing's pacing numbers (render_worker.hpp, item_timing): written by the worker, read and reset
// by the port's 10 s log line
std::array<std::atomic<int64_t>, 5> g_itemMaxNs{};
std::array<std::atomic<int64_t>, 5> g_itemTotalNs{};
std::array<std::atomic<uint64_t>, 5> g_itemCount{};
std::atomic<int64_t> g_lastItemStartNs{0}, g_lastItemEndNs{0};
std::atomic<uint8_t> g_lastItemType{0};

int64_t steady_ns(std::chrono::steady_clock::time_point t) noexcept {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
}

void note_item(ItemType type, std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) {
  const auto i = static_cast<size_t>(type) < g_itemMaxNs.size() ? static_cast<size_t>(type) : 0;
  const int64_t ns = steady_ns(end) - steady_ns(start);
  int64_t current = g_itemMaxNs[i].load(std::memory_order_relaxed);
  while (ns > current && !g_itemMaxNs[i].compare_exchange_weak(current, ns, std::memory_order_relaxed)) {
  }
  g_itemTotalNs[i].fetch_add(ns, std::memory_order_relaxed);
  g_itemCount[i].fetch_add(1, std::memory_order_relaxed);
  g_lastItemStartNs.store(steady_ns(start), std::memory_order_relaxed);
  g_lastItemEndNs.store(steady_ns(end), std::memory_order_relaxed);
  g_lastItemType.store(static_cast<uint8_t>(type), std::memory_order_relaxed);
}

void complete_sync(const std::shared_ptr<SyncState>& sync) {
  if (!sync) {
    return;
  }
  ZoneScoped;
  {
    std::lock_guard lock{sync->mutex};
    sync->complete = true;
  }
  sync->cv.notify_all();
}

void worker_main(std::stop_token token) {
  g_workerThreadId = std::this_thread::get_id();

  while (true) {
    bool closed = false;
    // OrrePort, frame smoothing: the presenter runs here, after every item and every idle wake-up, and says how soon
    // it wants to run again (its next picture), so the wait below can be shorter than the usual 1 ms
    std::chrono::nanoseconds wait = IdlePumpInterval;
    bool poll = false;
    if (const auto hook = g_idleHook.load(std::memory_order_acquire);
        hook != nullptr && g_idleHookPaused.load(std::memory_order_acquire) == 0) {
      const int64_t wantNs = hook();
      if (wantNs > 0 && wantNs < wait.count()) {
        wait = std::chrono::nanoseconds{wantNs};
      }
      // The presenter wants to run again within 2 ms: look at the queue without waiting. Measured 2026-11-16 (Windows,
      // MinGW): a timed condition-variable wait of even 1 ns slept about a system timer tick (winpthreads rounds the
      // timeout up to milliseconds and Windows wakes on its coarse timer, likely 15.6 ms), so pictures came 20-28 ms
      // apart. The presenter does its own precise sleeping (aurora.cpp, idle_tick). Linux and macOS wait precisely
      // with pthreads (likely), where this is merely harmless.
      poll = wantNs > 0 && wantNs < 2'000'000;
    }
    auto item = poll ? g_queue.try_pop(closed) : g_queue.pop_for(wait, closed);
    if (!item) {
      if (closed || token.stop_requested()) {
        break;
      }
      continue;
    }

    if (item->work) {
      ZoneScopedN("QueueItem work");
      // OrrePort, frame smoothing (pacing, 2026-11-16): how long each item takes, by kind, so a late picture can be
      // traced to the item that held the worker past its slot (render_worker.hpp, item_timing). Two clock reads per
      // item.
      const auto start = std::chrono::steady_clock::now();
      item->work();
      const auto end = std::chrono::steady_clock::now();
      note_item(item->type, start, end);
    }
    complete_sync(item->sync);
    g_pendingItems.fetch_sub(1, std::memory_order_acq_rel);
    if (item->type == ItemType::Shutdown) {
      break;
    }
  }

  g_workerThreadId = {};
}

void enqueue(QueueItem item) {
  ZoneScoped;
  if (is_worker_thread()) {
    if (item.work) {
      item.work();
    }
    complete_sync(item.sync);
    return;
  }

  if (!g_running.load(std::memory_order_acquire)) {
    if (item.work) {
      item.work();
    }
    complete_sync(item.sync);
    return;
  }

  g_pendingItems.fetch_add(1, std::memory_order_acq_rel);
  if (!g_queue.push(std::move(item))) {
    g_pendingItems.fetch_sub(1, std::memory_order_acq_rel);
  }
}
} // namespace

BoundedQueue::BoundedQueue(size_t capacity) : m_capacity(capacity) {}

bool BoundedQueue::push(QueueItem item) {
  ZoneScoped;
  std::unique_lock lock{m_mutex};
  m_notFull.wait(lock, [&] { return m_closed || m_items.size() < m_capacity; });
  if (m_closed) {
    return false;
  }
  m_items.emplace_back(std::move(item));
  lock.unlock();
  m_notEmpty.notify_one();
  return true;
}

std::optional<QueueItem> BoundedQueue::pop_for(std::chrono::nanoseconds timeout, bool& closed) {
  std::unique_lock lock{m_mutex};
  m_notEmpty.wait_for(lock, timeout, [&] { return m_closed || !m_items.empty(); });
  closed = m_closed && m_items.empty();
  if (m_items.empty()) {
    return std::nullopt;
  }
  auto item = std::move(m_items.front());
  m_items.pop_front();
  lock.unlock();
  m_notFull.notify_one();
  return item;
}

// OrrePort, frame smoothing: pop_for without any wait (the worker loop's poll above)
std::optional<QueueItem> BoundedQueue::try_pop(bool& closed) {
  std::unique_lock lock{m_mutex};
  closed = m_closed && m_items.empty();
  if (m_items.empty()) {
    return std::nullopt;
  }
  auto item = std::move(m_items.front());
  m_items.pop_front();
  lock.unlock();
  m_notFull.notify_one();
  return item;
}

void BoundedQueue::close() {
  {
    std::lock_guard lock{m_mutex};
    m_closed = true;
  }
  m_notEmpty.notify_all();
  m_notFull.notify_all();
}

void BoundedQueue::reset() {
  {
    std::lock_guard lock{m_mutex};
    m_items.clear();
    m_closed = false;
  }
  m_notFull.notify_all();
}

size_t BoundedQueue::size() const {
  std::lock_guard lock{m_mutex};
  return m_items.size();
}

FrameSlotPool::FrameSlotPool(size_t slotCount) : m_freeSlots(slotCount, true) {}

size_t FrameSlotPool::acquire() {
  std::unique_lock lock{m_mutex};
  m_cv.wait(lock, [&] {
    for (const bool free : m_freeSlots) {
      if (free) {
        return true;
      }
    }
    return false;
  });
  for (size_t i = 0; i < m_freeSlots.size(); ++i) {
    if (m_freeSlots[i]) {
      m_freeSlots[i] = false;
      return i;
    }
  }
  return 0;
}

std::optional<size_t> FrameSlotPool::try_acquire() {
  std::lock_guard lock{m_mutex};
  for (size_t i = 0; i < m_freeSlots.size(); ++i) {
    if (m_freeSlots[i]) {
      m_freeSlots[i] = false;
      return i;
    }
  }
  return std::nullopt;
}

void FrameSlotPool::release(size_t slot) {
  {
    std::lock_guard lock{m_mutex};
    if (slot < m_freeSlots.size()) {
      m_freeSlots[slot] = true;
    }
  }
  m_cv.notify_one();
}

void FrameSlotPool::reset() {
  {
    std::lock_guard lock{m_mutex};
    std::fill(m_freeSlots.begin(), m_freeSlots.end(), true);
  }
  m_cv.notify_all();
}

size_t FrameSlotPool::free_count() const {
  std::lock_guard lock{m_mutex};
  return static_cast<size_t>(std::ranges::count(m_freeSlots, true));
}

void initialize() {
  if (g_running.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  g_queue.reset();
  g_pendingItems.store(0, std::memory_order_release);
  g_thread = thread::Thread{{
                                .name = "Aurora render worker",
                                .affinity = thread::Affinity::SharedCache,
                            },
                            worker_main};
}

void shutdown() {
  if (!g_running.load(std::memory_order_acquire)) {
    return;
  }
  enqueue({
      .type = ItemType::Shutdown,
  });
  if (g_thread.joinable()) {
    g_thread.join();
  }
  g_running.store(false, std::memory_order_release);
  g_queue.close();
  g_queue.reset();
  g_pendingItems.store(0, std::memory_order_release);
}

void enqueue_begin_frame(uint64_t frameId, WorkCallback work) {
  enqueue({
      .type = ItemType::BeginFrame,
      .frameId = frameId,
      .work = std::move(work),
  });
}

void enqueue_encode_pass(uint64_t frameId, uint32_t passIndex, WorkCallback work) {
  enqueue({
      .type = ItemType::EncodePass,
      .frameId = frameId,
      .passIndex = passIndex,
      .work = std::move(work),
  });
}

void enqueue_end_frame(uint64_t frameId, WorkCallback work) {
  enqueue({
      .type = ItemType::EndFrame,
      .frameId = frameId,
      .work = std::move(work),
  });
}

void enqueue_work(WorkCallback work) {
  enqueue({
      .type = ItemType::Sync,
      .work = std::move(work),
  });
}

void synchronize() {
  if (is_worker_thread()) {
    return;
  }

  ZoneScoped;
  auto sync = std::make_shared<SyncState>();
  enqueue({
      .type = ItemType::Sync,
      .sync = sync,
  });
  std::unique_lock lock{sync->mutex};
  sync->cv.wait(lock, [&] { return sync->complete; });
}

bool is_worker_thread() noexcept { return g_workerThreadId == std::this_thread::get_id(); }

bool is_idle() noexcept { return g_pendingItems.load(std::memory_order_acquire) == 0; }

void set_idle_hook(IdleHook hook) noexcept { g_idleHook.store(hook, std::memory_order_release); }

// render_worker.hpp: raise the count, then a synchronize() by the caller (or the resize's own) lets a hook call already
// under way finish: the worker takes the sync item only between hook calls, and every later loop sees the count
// ORRE_RESIZE_STRESS_NOPAUSE=1 (a test switch, aurora.cpp's ORRE_RESIZE_STRESS): the pause does nothing, the code as it
// was before the fix, to show the race in a test run
static bool pause_disabled_for_test() noexcept {
  static const bool off = [] {
    const char* v = std::getenv("ORRE_RESIZE_STRESS_NOPAUSE");
    return v != nullptr && v[0] == '1';
  }();
  return off;
}
IdleHookPause::IdleHookPause() noexcept {
  if (!pause_disabled_for_test()) {
    g_idleHookPaused.fetch_add(1, std::memory_order_acq_rel);
  }
}
IdleHookPause::~IdleHookPause() {
  if (!pause_disabled_for_test()) {
    g_idleHookPaused.fetch_sub(1, std::memory_order_acq_rel);
  }
}

ItemTiming item_timing(bool reset) noexcept {
  ItemTiming out;
  for (size_t i = 0; i < g_itemMaxNs.size(); ++i) {
    out.maxNs[i] = reset ? g_itemMaxNs[i].exchange(0, std::memory_order_relaxed)
                         : g_itemMaxNs[i].load(std::memory_order_relaxed);
    out.totalNs[i] = reset ? g_itemTotalNs[i].exchange(0, std::memory_order_relaxed)
                           : g_itemTotalNs[i].load(std::memory_order_relaxed);
    out.count[i] = reset ? g_itemCount[i].exchange(0, std::memory_order_relaxed)
                         : g_itemCount[i].load(std::memory_order_relaxed);
  }
  return out;
}

LastItem last_item() noexcept {
  return {
      .startNs = g_lastItemStartNs.load(std::memory_order_relaxed),
      .endNs = g_lastItemEndNs.load(std::memory_order_relaxed),
      .type = static_cast<ItemType>(g_lastItemType.load(std::memory_order_relaxed)),
  };
}

int64_t steady_now_ns() noexcept { return steady_ns(std::chrono::steady_clock::now()); }

} // namespace aurora::gfx::render_worker
