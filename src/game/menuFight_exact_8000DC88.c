/**
 * @file menuFight_exact_8000DC88.c
 * @brief menuFightCtrlTimer .. menuFightOpenCountDown, 0x8000DC88 - 0x8000DE24.
 *
 * Function-boundary carve of the menuFight TU (see menuFight.c): the timer
 * window control and the total-timer / count-down window open/close
 * wrappers. No jump table (the two-case switch is a compare tree), no
 * pooled constant, no data. GC/1.3 -O4,p with the TU's unit-wide
 * -opt nopeephole, no pragmas.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 fightTimerAllGetNokoriTime(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 fightTimerCommandGetNokoriTime(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowSetParam(u8* window, u32 index, s32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 menuIsCheck(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom(s32 id, s32 arg1, s32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpenCustom(s32 id, ...);
#endif

/* Show the remaining battle / command time in the timer windows. */
u32 menuFightCtrlTimer(u8* ptr) {
    switch (*(s32*)(ptr + 4)) {
        case 0x10a:
            windowSetParam(ptr, 0, (s32)fightTimerAllGetNokoriTime());
            break;
        case 0x10b:
            windowSetParam(ptr, 0, (s32)fightTimerCommandGetNokoriTime());
            break;
    }
    return 0;
}

u32 menuFightCloseCheckTotalTimer(void) { return menuIsCheck(0x10a); }

void menuFightCloseTotalTimer(void) { menuCloseCustom(0x10a, 0, 0); }

void menuFightOpenTotalTimer(void) { menuOpenCustom(0x10a, -1, 0, 0, 0, 0); }

u32 menuFightCloseCheckCountDown(void) { return menuIsCheck(0x10b); }

void menuFightCloseCountDown(void) { menuCloseCustom(0x10b, 0, 0); }

void menuFightOpenCountDown(void) { menuOpenCustom(0x10b, -1, 0, 0, 0, 0); }
