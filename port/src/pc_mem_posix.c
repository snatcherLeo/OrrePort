/*
 * pc_mem on Linux / macOS (mmap). See pc_mem.h.
 *
 * NOT BUILT OR TESTED YET (Windows comes first); written now so the interface stays cross-platform. Notes for
 * when it is: Linux has MAP_FIXED_NOREPLACE (kernel 4.17+), which fails instead of replacing an existing mapping;
 * macOS has no such flag, so the address hint is used and the result checked. On arm64 macOS the low 4 GB are
 * reserved by the __PAGEZERO segment of the executable, so it must be linked with a small -pagezero_size
 * (melee-pc's approach) or the "low 32 bits equal the GameCube address" scheme used instead (platform-layer plan).
 */
#include "pc_mem.h"

#include <stdio.h>
#include <sys/mman.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0
#endif

static size_t s_committed;

int pc_mem_reserve(size_t commit_size) {
    void* want = (void*)(uintptr_t)PC_MEM1_BASE;
    void* p = mmap(want, PC_MEM_RESERVE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                   -1, 0);
    if (p != want) {
        fprintf(stderr, "[pc_mem] RESERVE FAILED at 0x%08X: got %p\n", PC_MEM1_BASE, p);
        if (p != MAP_FAILED) {
            munmap(p, PC_MEM_RESERVE_SIZE);
        }
        return 0;
    }
    if (!pc_mem_commit(commit_size)) {
        munmap(p, PC_MEM_RESERVE_SIZE);
        return 0;
    }
    return 1;
}

int pc_mem_commit(size_t new_size) {
    if (new_size <= s_committed) {
        return 1;
    }
    if (new_size > PC_MEM_RESERVE_SIZE ||
        mprotect((void*)(uintptr_t)PC_MEM1_BASE, new_size, PROT_READ | PROT_WRITE) != 0) {
        fprintf(stderr, "[pc_mem] COMMIT FAILED (%zu MB)\n", new_size >> 20);
        return 0;
    }
    s_committed = new_size;
    return 1;
}

size_t pc_mem_committed(void) { return s_committed; }

int pc_mem_map_locked_cache(void) {
    static int mapped;
    void* want = (void*)(uintptr_t)PC_LOCKED_CACHE_BASE;
    void* p;
    if (mapped) {
        return 1;
    }
    p = mmap(want, PC_LOCKED_CACHE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != want) {
        fprintf(stderr, "[pc_mem] cannot map the locked cache at 0x%08X\n", PC_LOCKED_CACHE_BASE);
        if (p != MAP_FAILED) {
            munmap(p, PC_LOCKED_CACHE_SIZE);
        }
        return 0;
    }
    mapped = 1;
    return 1;
}

int pc_mem_check(const char* label) {
    volatile uint32_t* lo = (volatile uint32_t*)(uintptr_t)PC_MEM1_BASE;
    volatile uint32_t* hi = (volatile uint32_t*)(uintptr_t)(PC_MEM1_BASE + s_committed - 4);
    int ok;
    if (s_committed == 0) {
        printf("[%s] memory: NOT RESERVED\n", label);
        return 0;
    }
    *lo = 0x47433645;
    *hi = 0x30315858;
    ok = *lo == 0x47433645 && *hi == 0x30315858;
    *lo = 0;
    *hi = 0;
    printf("[%s] memory: %u MB reserved at 0x%08X, %zu MB committed: %s (region walk not implemented here)\n", label,
           PC_MEM_RESERVE_SIZE >> 20, PC_MEM1_BASE, s_committed >> 20, ok ? "OK" : "FAILED");
    return ok;
}

/* ===== pc_os.h on Linux / macOS (not built yet) ===== */
#include "pc_os.h"

#include <dlfcn.h>

int pc_os_reserve_fixed(uintptr_t base, size_t size) {
    void* p = mmap((void*)base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != (void*)base) {
        if (p != MAP_FAILED) {
            munmap(p, size);
        }
        return 0;
    }
    return 1;
}

int pc_os_commit(uintptr_t addr, size_t size) { return mprotect((void*)addr, size, PROT_READ | PROT_WRITE) == 0; }

int pc_os_guard(uintptr_t addr, size_t size, int guard) {
    return mprotect((void*)addr, size, guard ? PROT_NONE : PROT_READ | PROT_WRITE) == 0;
}

uintptr_t pc_os_image_base(void) {
    Dl_info info;
    return dladdr((void*)&pc_os_image_base, &info) ? (uintptr_t)info.dli_fbase : 0;
}
