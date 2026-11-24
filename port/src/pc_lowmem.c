/*
 * pc_lowmem.c: a small first-fit allocator over a fixed region below 4 GB. See pc_lowmem.h.
 *
 * Blocks sit back to back from the region start; each starts with a 32-byte header (size including the header,
 * free flag). Allocation takes the first free block that fits (splitting off the rest); freeing merges the block
 * with free neighbours. The region is committed in 1 MB steps as it grows. Port allocations are few and mostly
 * long-lived (thread stacks, file buffers), so a linear search is fine.
 */
#include "pc_lowmem.h"
#include "pc_os.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define HDR 32u
#define COMMIT_STEP (1u << 20)

typedef struct Block {
    size_t size; /* bytes including this header */
    size_t prev_size; /* size of the block before (0 for the first) */
    uint32_t free;
    uint32_t magic;
    uint64_t pad;
} Block;

#define MAGIC 0x4C4F574Du /* "LOWM" */

static atomic_flag s_lock = ATOMIC_FLAG_INIT;
static int s_ready;
static uintptr_t s_end;       /* end of the blocks laid out so far */
static uintptr_t s_committed; /* end of the committed part */
static size_t s_used;

static void lock(void) {
    while (atomic_flag_test_and_set_explicit(&s_lock, memory_order_acquire)) {
    }
}
static void unlock(void) { atomic_flag_clear_explicit(&s_lock, memory_order_release); }

static int init_locked(void) {
    if (s_ready) {
        return 1;
    }
    if (!pc_os_reserve_fixed(PC_LOWMEM_BASE, PC_LOWMEM_SIZE)) {
        fprintf(stderr, "[pc_lowmem] cannot reserve %u MB at 0x%08X\n", PC_LOWMEM_SIZE >> 20, PC_LOWMEM_BASE);
        return 0;
    }
    s_end = s_committed = PC_LOWMEM_BASE;
    s_ready = 1;
    return 1;
}

int pc_lowmem_init(void) {
    int ok;
    lock();
    ok = init_locked();
    unlock();
    return ok;
}

static Block* next_of(Block* b) { return (uintptr_t)b + b->size < s_end ? (Block*)((uintptr_t)b + b->size) : NULL; }

void* pc_lowmem_alloc(size_t size) {
    size_t need = HDR + ((size + 31) & ~(size_t)31);
    Block* b;
    Block* last = NULL;
    void* result = NULL;

    lock();
    if (!init_locked()) {
        unlock();
        return NULL;
    }
    /* first fit among the existing blocks */
    for (b = s_end > PC_LOWMEM_BASE ? (Block*)(uintptr_t)PC_LOWMEM_BASE : NULL; b != NULL; b = next_of(b)) {
        last = b;
        if (b->free && b->size >= need) {
            if (b->size - need >= HDR + 32) {
                Block* rest = (Block*)((uintptr_t)b + need);
                Block* after = next_of(b);
                rest->size = b->size - need;
                rest->prev_size = need;
                rest->free = 1;
                rest->magic = MAGIC;
                if (after != NULL) {
                    after->prev_size = rest->size;
                }
                b->size = need;
            }
            b->free = 0;
            result = (void*)((uintptr_t)b + HDR);
            break;
        }
    }
    /* otherwise grow at the end */
    if (result == NULL && s_end + need <= PC_LOWMEM_BASE + PC_LOWMEM_SIZE) {
        if (s_end + need > s_committed) {
            uintptr_t want = (s_end + need + COMMIT_STEP - 1) & ~(uintptr_t)(COMMIT_STEP - 1);
            if (want > PC_LOWMEM_BASE + PC_LOWMEM_SIZE) {
                want = PC_LOWMEM_BASE + PC_LOWMEM_SIZE;
            }
            if (!pc_os_commit(s_committed, want - s_committed)) {
                unlock();
                fprintf(stderr, "[pc_lowmem] commit failed\n");
                return NULL;
            }
            s_committed = want;
        }
        b = (Block*)s_end;
        b->size = need;
        b->prev_size = last != NULL ? last->size : 0;
        b->free = 0;
        b->magic = MAGIC;
        s_end += need;
        result = (void*)((uintptr_t)b + HDR);
    }
    if (result != NULL) {
        s_used += ((Block*)((uintptr_t)result - HDR))->size;
        memset(result, 0, ((Block*)((uintptr_t)result - HDR))->size - HDR);
    }
    unlock();
    if (result == NULL) {
        fprintf(stderr, "[pc_lowmem] out of low memory (%zu bytes requested)\n", size);
    }
    return result;
}

void pc_lowmem_free(void* p) {
    Block* b;
    Block* n;
    if (p == NULL) {
        return;
    }
    lock();
    b = (Block*)((uintptr_t)p - HDR);
    if ((uintptr_t)p < PC_LOWMEM_BASE || (uintptr_t)p >= s_end || b->magic != MAGIC || b->free) {
        unlock();
        fprintf(stderr, "[pc_lowmem] bad free of %p\n", p);
        return;
    }
    s_used -= b->size;
    b->free = 1;
    /* merge with the next block, then with the previous one */
    n = next_of(b);
    if (n != NULL && n->free) {
        Block* nn;
        b->size += n->size;
        nn = next_of(b);
        if (nn != NULL) {
            nn->prev_size = b->size;
        }
    }
    if (b->prev_size != 0) {
        Block* prev = (Block*)((uintptr_t)b - b->prev_size);
        if (prev->free) {
            Block* nn;
            prev->size += b->size;
            nn = next_of(prev);
            if (nn != NULL) {
                nn->prev_size = prev->size;
            }
            b = prev;
        }
    }
    /* a free block at the very end shrinks the laid-out area (the memory stays committed for reuse) */
    if ((uintptr_t)b + b->size == s_end) {
        s_end = (uintptr_t)b;
    }
    unlock();
}

void* pc_lowmem_alloc_stack(size_t size, uintptr_t* top) {
    size_t usable = (size + PC_OS_PAGE - 1) & ~(size_t)(PC_OS_PAGE - 1);
    uint8_t* block = (uint8_t*)pc_lowmem_alloc(usable + 2 * PC_OS_PAGE);
    uintptr_t guard;
    if (block == NULL) {
        return NULL;
    }
    guard = ((uintptr_t)block + PC_OS_PAGE - 1) & ~(uintptr_t)(PC_OS_PAGE - 1);
    if (!pc_os_guard(guard, PC_OS_PAGE, 1)) {
        pc_lowmem_free(block);
        return NULL;
    }
    *top = (guard + PC_OS_PAGE + usable) & ~(uintptr_t)15;
    return block;
}

void pc_lowmem_free_stack(void* stack) {
    uintptr_t guard;
    if (stack == NULL) {
        return;
    }
    guard = ((uintptr_t)stack + PC_OS_PAGE - 1) & ~(uintptr_t)(PC_OS_PAGE - 1);
    pc_os_guard(guard, PC_OS_PAGE, 0);
    pc_lowmem_free(stack);
}

size_t pc_lowmem_used(void) { return s_used; }
