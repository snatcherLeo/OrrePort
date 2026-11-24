#include "dolphin/types.h"

typedef struct Window {
    u8 padding_000[0xB0];
    PTR32_FIELD(void) allocation; /* 4-byte slot on the PC too (window.c; 2026-10-17: an 8-byte read crashed the battle menu) */
} Window;

void* windowGetAllocPtr(Window* window)
{
    if (window != NULL) {
        return P32(window->allocation);
    }
    return NULL;
}
