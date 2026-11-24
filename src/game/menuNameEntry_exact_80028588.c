/**
 * @file menuNameEntry_exact_80028588.c
 * @brief Name-entry selection phase callback.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqSetMenu(void* sequence, s32 menuId);
#endif

s32 menuNameEntrySelectCtrl(void* window)
{
    u8* work;
    s8 phase;

    work = window;
    phase = (s8)work[1];
    switch (phase) {
    case 0:
        if ((s8)work[2] == 0) {
            winSeqSetMenu(PC_PTR32_IMPLICIT(PTR32_AT_T(void, work, 4)) /* 32-bit slot in the GameCube-layout window work */, 0x56);
            work[2] = 1;
        }
        break;
    case 3:
        if ((s8)work[2] == 0) {
            winSeqSetMenu(PC_PTR32_IMPLICIT(PTR32_AT_T(void, work, 4)) /* 32-bit slot in the GameCube-layout window work */, 0x5A);
            work[2] = 1;
        }
        break;
    }
    return 0;
}
