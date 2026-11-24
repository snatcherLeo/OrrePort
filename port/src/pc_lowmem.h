/*
 * pc_lowmem: memory below 4 GB for everything the port layer hands to the game (snatcherLeo's decision on 32-bit
 * pointers, option (a), 2026-10-15; docs/port/low-memory.md).
 *
 * The game keeps pointers in u32 slots. Its own heaps live in MEM1 at 0x80000000 (pc_mem.h), but the port layer
 * also creates things the game holds on to: GS thread stacks (pc_fibre), strings, buffers, file data. Those come
 * from this allocator, a fixed region at PC_LOWMEM_BASE, so any pointer to them survives a round trip through 32
 * bits. Thread-safe. Alignment: 32 bytes for every block.
 */
#ifndef PC_LOWMEM_H
#define PC_LOWMEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_LOWMEM_BASE 0x40000000u
#define PC_LOWMEM_SIZE (512u << 20)

/* Reserve the region (idempotent; called by the first allocation too). Returns 1 on success. */
int pc_lowmem_init(void);

/* Zeroed block of `size` bytes, 32-byte aligned, below 4 GB; NULL when the region is full. */
void* pc_lowmem_alloc(size_t size);
void pc_lowmem_free(void* p);

/* A stack for a fibre: `size` usable bytes (16-byte aligned top), with an inaccessible guard page below it so an
 * overflow crashes instead of corrupting the block underneath. *top receives the initial stack pointer. */
void* pc_lowmem_alloc_stack(size_t size, uintptr_t* top);
void pc_lowmem_free_stack(void* stack);

/* Bytes in use (for checks and debugging). */
size_t pc_lowmem_used(void);

/* 1 when [p, p + size) lies entirely below 4 GB. */
static inline int pc_is_low(const void* p, size_t size) {
    return (uint64_t)(uintptr_t)p + size <= 0x100000000ull;
}

#ifdef __cplusplus
}
#endif

#endif /* PC_LOWMEM_H */
