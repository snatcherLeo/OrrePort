/*
 * pc_mem: GameCube main memory (MEM1) at its real address, 0x80000000, in the PC process.
 *
 * Why (platform-layer plan, decision 5): disc data keeps 32-bit pointers, and the engine keeps pointers in u32
 * globals and struct slots (the GSthread scheduler, for one). If every game allocation lives inside
 * [0x80000000, 0x80000000 + size), a host pointer truncated to 32 bits is still the same pointer, so those slots keep
 * working on a 64-bit PC.
 *
 * The range must be reserved before SDL, Dawn and the GPU driver load DLLs and make allocations that could land in
 * the low 4 GB, so pc_mem_reserve() is the first call in main(). It reserves address space only (no physical
 * memory) for PC_MEM_RESERVE_SIZE, and commits the first `commit_size` bytes (24 MB = retail MEM1). The reserved
 * part beyond that is room for mods to grow the heaps later (snatcherLeo's check 5(b)); pc_mem_commit() grows the
 * committed part in place.
 *
 * Platform files: pc_mem_win32.c (VirtualAlloc), pc_mem_posix.c (mmap).
 */
#ifndef PC_MEM_H
#define PC_MEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_MEM1_BASE         0x80000000u
#define PC_MEM1_RETAIL_SIZE  (24u << 20)  /* GameCube MEM1: 24 MB */
#define PC_MEM_RESERVE_SIZE  (1024u << 20) /* address space kept free for MEM1 and its growth: 1 GB */
/* The game's arena, exactly as on the GameCube (port-gaps 22, option 1, snatcherLeo 2026-10-17). Below the arena, main.dol's
 * data sits at its GameCube addresses (the project's copy), so battle scripts' embedded addresses work.
 * *Corrected 2026-10-17:* the first values were the link values __ArenaLo / __ArenaHi (0x80496740 / 0x81700000), but
 * the console's OSInit takes the bounds from the boot info: the bottom is _stack_addr rounded up (0x8048E738 ->
 * 0x8048E740; the 32 KB debugger stack above it goes to the arena when no debugger runs), the top is where the
 * apploader put the disc's file table (FST), 0x81800000 minus its size rounded to 32 bytes. Both verified against
 * Dolphin (the read: heap array at 0x8130E740 = bottom + GSmem's 0xE80000; SDK heap 3,628,544 bytes, which ends exactly
 * at 0x817F0560 = 0x81800000 - 64,160, the USA disc's FST size 64,156 rounded). Region note: the top depends on the
 * disc's FST size; GC6E01 only for now. */
#define PC_ARENA_LO          0x8048E740u
#define PC_ARENA_HI          0x817F0560u
#define PC_DOL_DATA_START    0x80003100u /* main.dol's lowest section; the arena must not reach below PC_ARENA_LO */

/* Reserve [PC_MEM1_BASE, PC_MEM1_BASE + PC_MEM_RESERVE_SIZE) and commit the first commit_size bytes (zeroed,
 * read / write). Returns 1 on success; on failure prints why and returns 0. Call once, first thing in main(). */
int pc_mem_reserve(size_t commit_size);

/* Grow the committed part to new_size bytes (never shrinks; at most PC_MEM_RESERVE_SIZE). Returns 1 on success. */
int pc_mem_commit(size_t new_size);

/* Committed size in bytes (0 before pc_mem_reserve succeeded). */
size_t pc_mem_committed(void);

/* Re-check the mapping after the rest of the program (SDL, Dawn, the GPU driver) has started: the whole committed
 * range still belongs to us and is read / write, a pattern written at both ends reads back, a pointer truncated to
 * 32 bits round-trips, and nothing else was mapped inside the reserved range. Prints one line per check and a
 * summary prefixed with `label`. Returns 1 when every check passes. */
int pc_mem_check(const char* label);

/* The CPU's locked cache, used by the engine's GSscratch allocator as 16 KB of fast scratch memory at 0xE0000000.
 * Mapped at the same address on the PC because GSscratchIsPtr tells scratch pointers from main memory by their top
 * four address bits (0xE vs 0x8). Returns 1 on success (also when already mapped). */
#define PC_LOCKED_CACHE_BASE 0xE0000000u
#define PC_LOCKED_CACHE_SIZE (16u << 10)
int pc_mem_map_locked_cache(void);

/* Host pointer <-> GameCube 32-bit address (valid only inside the MEM1 range). */
static inline uint32_t pc_mem_to_gc(const void* p) { return (uint32_t)(uintptr_t)p; }
static inline void* pc_mem_from_gc(uint32_t a) { return (void*)(uintptr_t)a; }

#ifdef __cplusplus
}
#endif

#endif /* PC_MEM_H */
