#include "dolphin/types.h"

typedef struct WindowWork {
    u8 padding[0x60];
    u32 params[15];
    u8 freeWork[1];
} WindowWork;

#ifdef TARGET_PC
/* 36 callers declare these accessors themselves, many with the window as an s32 (a 32-bit slot value): a window
 * address with the top bit set arrives sign-extended (0xFFFFFFFF8058E1E0), and using a Potion in Willie's battle crashed
 * in menuPokemonDrawItem (2026-10-17). Everything the game sees lives below 4 GB in OrrePort's layout, so the low 32
 * bits are the window for every caller (as in windowDrawSprite2). */
#define WINDOW_LOW32(window) ((window) = (WindowWork*)(uintptr_t)(u32)(uintptr_t)(window))
#else
#define WINDOW_LOW32(window) ((void)0)
#endif

void* windowGetFreeWork(WindowWork* window)
{
    WINDOW_LOW32(window);
    if (window != 0) {
        return window->freeWork;
    }
    return 0;
}

void windowSetParam(WindowWork* window, u32 index, u32 value)
{
    WINDOW_LOW32(window);
    if (window == 0) {
        return;
    }
    window->params[index] = value;
}

u32 windowGetParam(WindowWork* window, u32 index)
{
    WINDOW_LOW32(window);
    if (window != 0) {
        return window->params[index];
    }
    return 0;
}
