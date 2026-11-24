#include "dolphin/types.h"

typedef struct WindowWork WindowWork;

struct WindowWork {
    u8 pad_00[4];
    s32 id;
    u8 pad_08[8];
    PTR32_FIELD(WindowWork) next; /* 32-bit slot on the PC too (pc/ptr32.h) */
};

typedef struct WindowSystemWork {
    u32 field_00;
    u32 activeID;
    u8 pad_08[4];
    PTR32_FIELD(WindowWork) windows; /* 32-bit slot on the PC too (pc/ptr32.h) */
} WindowSystemWork;

extern WindowSystemWork winSysWork;

void* windowSearchID(s32 id)
{
    WindowWork* window;

    if (id <= 0) {
        return NULL;
    }

    window = P32(winSysWork.windows);
    while (window != NULL) {
        if (window->id == id) {
            return window;
        }
        window = P32(window->next);
    }
    return NULL;
}
