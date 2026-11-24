/*
 * memmap_probe.c: can OrrePort map GameCube main memory at its real address on Windows? (platform-layer plan,
 * decision 5, snatcherLeo's checks (a) and (b), 2026-10-15)
 *
 * The data model keeps disc pointers as 32-bit values, so the game's memory (MEM1, 24 MB at 0x80000000 on the
 * GameCube) must sit at that same address in the PC process. This probe:
 *   1. reserves [0x80000000, 0x80000000 + size) at a fixed address (VirtualAlloc, MEM_RESERVE), for several sizes:
 *      24 MB (retail MEM1) up to 1 GB (room to grow for mods, check (b));
 *   2. commits it, writes a pattern at both ends, reads it back;
 *   3. prints one result line per size.
 * Build (MSYS2 mingw64 GCC, linked non-PIE low in memory, as melee-pc does):
 *   gcc -O2 -o memmap_probe.exe memmap_probe.c -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase
 * Run it several times (and after a reboot) to check reliability (check (a)).
 */
#include <stdio.h>
#include <stdint.h>
#include <windows.h>

static int probe(uintptr_t base, size_t size) {
    void* p = VirtualAlloc((void*)base, size, MEM_RESERVE, PAGE_NOACCESS);
    if (p != (void*)base) {
        printf("  %4zu MB at 0x%08llX: RESERVE FAILED (got %p, error %lu)\n", size >> 20, (unsigned long long)base, p,
               GetLastError());
        if (p) VirtualFree(p, 0, MEM_RELEASE);
        return 0;
    }
    void* c = VirtualAlloc((void*)base, size, MEM_COMMIT, PAGE_READWRITE);
    if (c != (void*)base) {
        printf("  %4zu MB at 0x%08llX: COMMIT FAILED (error %lu)\n", size >> 20, (unsigned long long)base, GetLastError());
        VirtualFree(p, 0, MEM_RELEASE);
        return 0;
    }
    volatile uint32_t* lo = (volatile uint32_t*)base;
    volatile uint32_t* hi = (volatile uint32_t*)(base + size - 4);
    *lo = 0x47433645; /* "GC6E" */
    *hi = 0x30315858;
    int ok = (*lo == 0x47433645) && (*hi == 0x30315858);
    /* a disc pointer slot holds the GameCube address; truncating the host pointer to 32 bits must give it back */
    uint32_t slot = (uint32_t)(uintptr_t)(base + 0x1234);
    ok = ok && ((uintptr_t)slot == base + 0x1234);
    printf("  %4zu MB at 0x%08llX: %s\n", size >> 20, (unsigned long long)base, ok ? "OK" : "READBACK FAILED");
    VirtualFree(p, 0, MEM_RELEASE);
    return ok;
}

int main(void) {
    static const size_t sizes[] = {24u << 20, 64u << 20, 256u << 20, 512u << 20, 1024u << 20};
    int i, all = 1;
    printf("memmap_probe: executable image at %p\n", (void*)GetModuleHandleA(NULL));
    for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
        all &= probe(0x80000000u, sizes[i]);
    }
    printf("result: %s\n", all ? "ALL OK" : "SOME FAILED");
    return all ? 0 : 1;
}
