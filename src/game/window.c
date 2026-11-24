/**
 * @file window.c
 * @brief windowAllocMemory (0x80103FFC - 0x801040A0) of the window TU.
 *
 * The window TU is built with -opt nopeephole (configure.py; see
 * window_exact_80104318.c), so no local pragma is needed.
 */
#include "dolphin/types.h"

typedef struct WindowWork {
    /* 0x00 */ u8 _00[0xAC];
    /* 0xAC */ u16 memoryHandle;
    /* 0xB0 */ PTR32_FIELD(void) memory; /* 4 bytes on the PC too: the window record keeps its GameCube layout (2026-10-17) */
} WindowWork;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif

/* 0x80103FFC | 0xA4: free the window's memory block, then allocate size
 * bytes (none when size <= 0). */
void* windowAllocMemory(WindowWork* window, s32 size)
{
    if (window == NULL) {
        return NULL;
    }
    if (window->memoryHandle != 0) {
        fn_800E24B0(window->memoryHandle);
        GSmemFree(window->memoryHandle);
        P32_SET(window->memory, NULL);
    }
    if (size <= 0) {
        return NULL;
    }
    window->memoryHandle = _toolentryAlloc__FUl(size);
    if (window->memoryHandle != 0) {
        P32_SET(window->memory, GSmemLock(window->memoryHandle));
    } else {
#ifdef TARGET_PC
        { /* PC diagnostic (2026-10-17, temporary): a failed window work allocation */
            extern void OSReport(const char*, ...);
            OSReport("[window] windowAllocMemory: no handle for %d bytes (window %p)\n", (int)size, (void*)window);
        }
#endif
        return NULL;
    }
    return P32(window->memory);
}
