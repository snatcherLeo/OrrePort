#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "internal.hpp"

#include <dolphin/os.h>

#define ROUND64(n, a) (((u64)(n) + (a)-1) & ~(u64)((a)-1))
#define TRUNC64(n, a) (((u64)(n)) & ~(u64)((a)-1))

static void* ArenaLow;
static void* ArenaHigh;

// OrrePort: ORRE_HEAP_LOG=1 logs every arena allocation and arena move with its caller, to compare the arena's use
// with the console's (who takes how much before the game's heaps are created).
static int ArenaLog = -1;
static bool arena_log() {
  if (ArenaLog < 0) {
    ArenaLog = getenv("ORRE_HEAP_LOG") != nullptr;
  }
  return ArenaLog != 0;
}
static uint32_t gc_addr(const void* p) {
  return MEM1Start == nullptr ? 0
                              : static_cast<uint32_t>(0x80000000u + (static_cast<const u8*>(p) - static_cast<u8*>(MEM1Start)));
}

void* OSGetArenaHi() {
  return ArenaHigh;
}

void* OSGetArenaLo() {
  return ArenaLow;
}

void OSSetArenaHi(void* newHi) {
  assert(newHi <= MEM1End && newHi >= MEM1Start);
  if (arena_log()) {
    fprintf(stderr, "[arena] hi 0x%08X -> 0x%08X (called from %p)\n", gc_addr(ArenaHigh), gc_addr(newHi),
            __builtin_return_address(0));
  }
  ArenaHigh = newHi;
}

void OSSetArenaLo(void* newLo) {
  assert(newLo <= MEM1End && newLo >= MEM1Start);
  if (arena_log()) {
    fprintf(stderr, "[arena] lo 0x%08X -> 0x%08X (called from %p)\n", gc_addr(ArenaLow), gc_addr(newLo),
            __builtin_return_address(0));
  }
  ArenaLow = newLo;
}

void* OSAllocFromArenaLo(const u32 size, const u32 align) {
  if (arena_log()) {
    fprintf(stderr, "[arena] alloc %u bytes (align %u) from lo 0x%08X (called from %p)\n", size, align,
            gc_addr(ArenaLow), __builtin_return_address(0));
  }
  void* ptr = OSGetArenaLo();
  auto arenaLo = static_cast<u8*>(ptr = reinterpret_cast<void*>(ROUND64(ptr, align)));
  arenaLo += size;
  arenaLo = reinterpret_cast<u8*>(ROUND64(arenaLo, align));
  OSSetArenaLo(arenaLo);
  return ptr;
}

void* OSAllocFromArenaHi(const u32 size, const u32 align) {
  if (arena_log()) {
    fprintf(stderr, "[arena] alloc %u bytes (align %u) from hi 0x%08X (called from %p)\n", size, align,
            gc_addr(ArenaHigh), __builtin_return_address(0));
  }
  void* ptr;

  auto arenaHi = static_cast<u8*>(OSGetArenaHi());
  arenaHi = reinterpret_cast<u8*>(TRUNC64(arenaHi, align));
  arenaHi -= size;
  arenaHi = static_cast<u8*>(ptr = reinterpret_cast<void*>(TRUNC64(arenaHi, align)));
  OSSetArenaHi(arenaHi);
  return ptr;
}

void AuroraInitArena() {
  if (MEM1Start == nullptr) {
    return;
  }

  // OrrePort: the game's own arena bounds when configured (AuroraConfig.arenaLoOffset / arenaHiOffset)
  const uintptr_t lo = aurora::g_config.arenaLoOffset != 0 ? aurora::g_config.arenaLoOffset : ARENA_START_OFFSET;
  u8* hi = aurora::g_config.arenaHiOffset != 0 ? static_cast<u8*>(MEM1Start) + aurora::g_config.arenaHiOffset
                                               : static_cast<u8*>(MEM1End);
  if (static_cast<u8*>(MEM1Start) + lo >= hi || hi > static_cast<u8*>(MEM1End)) {
    Log.fatal("arena bounds 0x{:X}-0x{:X} do not fit MEM1", lo, static_cast<uintptr_t>(hi - static_cast<u8*>(MEM1Start)));
  }
  OSSetArenaLo(static_cast<u8*>(MEM1Start) + lo);
  OSSetArenaHi(hi);
}
