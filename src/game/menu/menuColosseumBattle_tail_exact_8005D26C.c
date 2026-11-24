/**
 * @file menuColosseumBattle_tail_exact_8005D26C.c
 * @brief Byte-exact menuColosseumBattle.cpp range, 0x8005D26C - 0x8005D2E8.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuCloseSync(s32, s32);
#endif

extern const u32 lbl_8047BF30;
extern const u32 lbl_8047BF34;

/* Address: 0x8005D26C | Size: 0x7C */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 fn_8005D26C(void) {
    s32 menuResult;
    s32 results[2];

    results[0] = lbl_8047BF30;
    results[1] = lbl_8047BF34;
    menuResult = menuOpen(0x9E, 1);
    menuClose(0x9E);
    menuCloseSync(0x9E, 1);
    if (menuResult < -1 || menuResult >= 2) {
        return 1;
    }
#ifdef TARGET_PC
    if (menuResult == -1) {
        /* PC: the game's range check lets B's -1 through, so the console reads results[-1], the stack word at sp+4:
         * the return address menuCloseSync (the last call, which saves its LR there) left, 0x8005D2B0 (assembly). The
         * box's caller ignores anything but 0, so B means "no"; read on the PC's stack it was 0, "yes" (P12: B on the
         * box bar's question rebuilt the box, 2026-10-26) */
        return 0x8005D2B0;
    }
#endif
    return results[menuResult];
}
#pragma pop
