#include "dolphin/types.h"

extern u8 winSysWork[];

typedef struct MenuSystemState {
    u8 pad_00[0x94];
    s32 lastError;
} MenuSystemState;

s32 menuGetLastError(void)
{
    return ((MenuSystemState*)winSysWork)->lastError;
}
