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
    u8 keyInfo;
} WindowSystemWork;

extern WindowSystemWork winSysWork;

u32 windowGetActiveID(void)
{
    return winSysWork.activeID;
}
