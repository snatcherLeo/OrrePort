/*
 * pc_mem on Windows (VirtualAlloc). See pc_mem.h. The checks repeat tools/local/port/memmap_probe.c inside the port,
 * now with SDL, Dawn and the GPU driver loaded (platform-layer plan, check 5(a)).
 */
#include "pc_mem.h"

#include <stdio.h>
#include <windows.h>

static size_t s_committed;

int pc_mem_reserve(size_t commit_size) {
    void* p;

    if (commit_size == 0 || commit_size > PC_MEM_RESERVE_SIZE) {
        fprintf(stderr, "[pc_mem] bad commit size %zu\n", commit_size);
        return 0;
    }
    p = VirtualAlloc((void*)(uintptr_t)PC_MEM1_BASE, PC_MEM_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
    if (p != (void*)(uintptr_t)PC_MEM1_BASE) {
        MEMORY_BASIC_INFORMATION mbi;
        fprintf(stderr, "[pc_mem] RESERVE FAILED at 0x%08X (%u MB): got %p, error %lu\n", PC_MEM1_BASE,
                PC_MEM_RESERVE_SIZE >> 20, p, GetLastError());
        if (VirtualQuery((void*)(uintptr_t)PC_MEM1_BASE, &mbi, sizeof(mbi)) != 0) {
            fprintf(stderr, "[pc_mem]   already there: base %p, size 0x%zX, state 0x%lX, type 0x%lX\n", mbi.AllocationBase,
                    (size_t)mbi.RegionSize, mbi.State, mbi.Type);
        }
        if (p != NULL) {
            VirtualFree(p, 0, MEM_RELEASE);
        }
        return 0;
    }
    if (!pc_mem_commit(commit_size)) {
        VirtualFree(p, 0, MEM_RELEASE);
        return 0;
    }
    return 1;
}

int pc_mem_commit(size_t new_size) {
    void* c;

    if (new_size <= s_committed) {
        return 1;
    }
    if (new_size > PC_MEM_RESERVE_SIZE) {
        fprintf(stderr, "[pc_mem] commit of %zu MB exceeds the %u MB reservation\n", new_size >> 20,
                PC_MEM_RESERVE_SIZE >> 20);
        return 0;
    }
    c = VirtualAlloc((void*)(uintptr_t)PC_MEM1_BASE, new_size, MEM_COMMIT, PAGE_READWRITE);
    if (c != (void*)(uintptr_t)PC_MEM1_BASE) {
        fprintf(stderr, "[pc_mem] COMMIT FAILED (%zu MB): error %lu\n", new_size >> 20, GetLastError());
        return 0;
    }
    s_committed = new_size;
    return 1;
}

size_t pc_mem_committed(void) { return s_committed; }

int pc_mem_map_locked_cache(void) {
    static int mapped;
    void* p;
    if (mapped) {
        return 1;
    }
    p = VirtualAlloc((void*)(uintptr_t)PC_LOCKED_CACHE_BASE, PC_LOCKED_CACHE_SIZE, MEM_RESERVE | MEM_COMMIT,
                     PAGE_READWRITE);
    if (p != (void*)(uintptr_t)PC_LOCKED_CACHE_BASE) {
        fprintf(stderr, "[pc_mem] cannot map the locked cache at 0x%08X: got %p, error %lu\n", PC_LOCKED_CACHE_BASE, p,
                GetLastError());
        if (p != NULL) {
            VirtualFree(p, 0, MEM_RELEASE);
        }
        return 0;
    }
    mapped = 1;
    return 1;
}

int pc_mem_check(const char* label) {
    const uintptr_t base = PC_MEM1_BASE;
    uintptr_t addr = base;
    int ok = 1, foreign = 0;
    volatile uint32_t* lo;
    volatile uint32_t* hi;
    uint32_t slot;

    if (s_committed == 0) {
        printf("[%s] memory: NOT RESERVED\n", label);
        return 0;
    }

    /* Walk the whole reservation: the committed part must be ours and read / write, the rest still reserved by us.
     * Any region with another allocation base means something else got mapped inside our range. */
    while (addr < base + PC_MEM_RESERVE_SIZE) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void*)addr, &mbi, sizeof(mbi)) == 0) {
            printf("[%s] memory: VirtualQuery failed at %p (error %lu)\n", label, (void*)addr, GetLastError());
            return 0;
        }
        if (mbi.AllocationBase != (void*)base) {
            printf("[%s] memory: FOREIGN region at %p (size 0x%zX, allocation base %p)\n", label, mbi.BaseAddress,
                   (size_t)mbi.RegionSize, mbi.AllocationBase);
            foreign = 1;
        } else if (addr < base + s_committed && (mbi.State != MEM_COMMIT || mbi.Protect != PAGE_READWRITE)) {
            printf("[%s] memory: committed part at %p has state 0x%lX, protect 0x%lX\n", label, mbi.BaseAddress,
                   mbi.State, mbi.Protect);
            ok = 0;
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }
    ok = ok && !foreign;

    /* The memmap_probe checks: a pattern at both ends of the committed part, and the 32-bit round trip */
    lo = (volatile uint32_t*)base;
    hi = (volatile uint32_t*)(base + s_committed - 4);
    {
        uint32_t save_lo = *lo, save_hi = *hi;
        *lo = 0x47433645; /* "GC6E" */
        *hi = 0x30315858;
        if (*lo != 0x47433645 || *hi != 0x30315858) {
            printf("[%s] memory: READBACK FAILED\n", label);
            ok = 0;
        }
        *lo = save_lo;
        *hi = save_hi;
    }
    slot = pc_mem_to_gc((void*)(base + 0x1234));
    if (pc_mem_from_gc(slot) != (void*)(base + 0x1234)) {
        printf("[%s] memory: 32-bit pointer round trip FAILED\n", label);
        ok = 0;
    }

    printf("[%s] memory: %u MB reserved at 0x%08X, %zu MB committed, exe image at %p: %s\n", label,
           PC_MEM_RESERVE_SIZE >> 20, PC_MEM1_BASE, s_committed >> 20, (void*)GetModuleHandleA(NULL),
           ok ? "OK" : "FAILED");
    return ok;
}

/* ===== pc_os.h on Windows ===== */
#include "pc_os.h"

int pc_os_reserve_fixed(uintptr_t base, size_t size) {
    void* p = VirtualAlloc((void*)base, size, MEM_RESERVE, PAGE_NOACCESS);
    if (p != (void*)base) {
        if (p != NULL) {
            VirtualFree(p, 0, MEM_RELEASE);
        }
        return 0;
    }
    return 1;
}

int pc_os_commit(uintptr_t addr, size_t size) {
    return VirtualAlloc((void*)addr, size, MEM_COMMIT, PAGE_READWRITE) == (void*)addr;
}

int pc_os_guard(uintptr_t addr, size_t size, int guard) {
    DWORD old;
    return VirtualProtect((void*)addr, size, guard ? PAGE_NOACCESS : PAGE_READWRITE, &old) != 0;
}

uintptr_t pc_os_image_base(void) { return (uintptr_t)GetModuleHandleA(NULL); }
