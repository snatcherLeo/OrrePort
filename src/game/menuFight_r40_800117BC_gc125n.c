/**
 * @file menuFight_r40_800117BC_gc125n.c
 * @brief menuFightOpenWaza, 0x800117BC - 0x800119A8.
 *
 * Function-boundary carve of the menuFight TU (see menuFight.c, which keeps
 * the same definition): open the move menu (window 0x4C, or 0xF7 on the
 * alternate layout) until a usable move is picked; an unusable pick shows
 * the reason in the fight message window until a key or the battle timer
 * dismisses it. No jump table, no pooled constant, no data. Built like its
 * neighbours: GC/1.3 -O4,p with the TU's unit-wide -opt nopeephole, no
 * pragmas. (The file name keeps its old probe suffix to match splits.txt.)
 */
#include "dolphin/types.h"
#include "dolphin/pad/Pad.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqSetMenu(void* ctx, s32 state);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_801F18DC(s32 controller);
#endif
extern u8 fightFloorIsUseFightTimerCommand(s32 controller);
extern u8 fightTimerCommandIsOver(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 std103_battleGetResult(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSinputButtonsChanged(s32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_800F7BC4(s32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 menuIsCheck(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpenCustom(s32 id, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenFight();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgCloseFight();
#endif

/* TRUE when the battle timer forces the current menu closed. */
static inline u8 menuFightIsTimeUp(void)
{
    if (fn_801F18DC(0) != 0) {
        if (fightFloorIsUseFightTimerCommand(0) == 1 && fightTimerCommandIsOver() == 1) {
            return 1;
        }
        if (std103_battleGetResult() == 1) {
            return 1;
        }
    }
    return 0;
}

s32 menuFightOpenWaza(u8* status, u32 arg1, u32 arg2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _menuFightIsUse__FP16MENU_WAZA_STATUSUs(u8* status, u16 waza);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
    s32 menu;
    s32 sel;
    u32 msg;

    if (status[0x44] == 0) {
        menu = 0x4c;
    } else {
        menu = 0xf7;
    }
    while (1) {
        winSeqSetMenu(PC_PTR32_IMPLICIT((void*)PC_PTR(menu)), 0x1e);
        sel = menuOpenCustom(menu, 0, PC_PTR(arg1), 0, arg2, 1, status);
        if (sel < 0) {
            break;
        }
        msg = _menuFightIsUse__FP16MENU_WAZA_STATUSUs(status, sel);
        if (msg == 0) {
            break;
        }
        if ((u8)menuIsCheck(0x4c) != 0) {
            menuCloseCustom(0x4c, 0, 1);
        }
        if ((u8)menuIsCheck(0xf9) != 0) {
            menuCloseCustom(0xf9, 0, 1);
        }
        if ((u8)menuIsCheck(0xfa) != 0) {
            menuCloseCustom(0xfa, 0, 1);
        }
        if ((u8)menuIsCheck(0xf7) != 0) {
            menuCloseCustom(0xf7, 0, 1);
        }
        winMsgOpenFight(msg, 1, 1);
        while ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & (PAD_BUTTON_B | PAD_BUTTON_A)) == 0) {
            if (menuFightIsTimeUp() != 0) {
                break;
            }
            _threadSwitch();
        }
        winMsgCloseFight(1);
    }
    winSeqSetMenu(PC_PTR32_IMPLICIT((void*)PC_PTR(menu)), 0x20);
    return sel;
}
