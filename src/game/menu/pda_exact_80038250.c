/**
 * @file pda_exact_80038250.c
 * @brief Byte-exact PDA menu callbacks, 0x80038250 - 0x8003842C.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"

extern void winSeqSetMenu(s32 id, u32 seqId);

typedef struct PdaMenuState {
    u8 pad00;
    s8 mode;
    s8 menuSet;
} PdaMenuState;

typedef struct PdaSelectionWork {
    u8 pad00[4];
#ifdef TARGET_PC
    u32 menu; /* PC: the window's 32-bit id; a void* moved selectedIndex off +0x95 */
#else
    void* menu;
#endif
    u8 pad08[0x8d];
    s8 selectedIndex;
} PdaSelectionWork;

typedef struct PdaKeyInfo {
    u8 pad00[4];
    u16 trigger;
    u16 buttons;
} PdaKeyInfo;

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 fn_80038250(PdaMenuState* state)
{

    switch (state->mode) {
    case 0:
        if (state->menuSet == 0) {
            winSeqSetMenu(0x1b, 0xc4);
            state->menuSet = 1;
        }
        break;
    case 3:
        if (state->menuSet == 0) {
            winSeqSetMenu(0x1b, 0xc8);
            state->menuSet = 1;
        }
        break;
    }
    return 0;
}
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole reset; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole reset

#pragma peephole off
s32 fn_800382E8(PdaMenuState* state)
{
    switch (state->mode) {
    case 0:
        if (state->menuSet == 0) {
            winSeqSetMenu(0x1a, 0xbc);
            state->menuSet = 1;
        }
        break;
    case 3:
        if (state->menuSet == 0) {
            winSeqSetMenu(0x1a, 0xc0);
            state->menuSet = 1;
        }
        break;
    }
    return 0;
}
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole reset; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole reset

#pragma peephole off
s32 fn_80038380(PdaSelectionWork* work)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern PdaKeyInfo* windowGetKeyInfo(void);
#endif
    extern s32 menuGetSelectItemNum();
    PdaKeyInfo* keyInfo;
    s32 itemCount;

    keyInfo = windowGetKeyInfo();
#ifdef TARGET_PC
    {
        /* OrreHook_PcMenuCursor (docs/port/hook-points.md; 2026-11-15): with OrrePort's rows on the PC screen the
         * screen order differs from the choice order (Main Menu is choice 4 but sits above Exit, choice 3), so the hook
         * moves the row along the screen order, clamped as the game does. Nothing registered or 0: the game's own +1 /
         * -1. Kept in step with pda_range_80037158.c. All platforms */
        extern s32 (*OrreHook_PcMenuCursor)(s8* row, u16 keys);
        if (OrreHook_PcMenuCursor != NULL && OrreHook_PcMenuCursor(&work->selectedIndex, keyInfo->buttons) != 0) {
            return 0;
        }
    }
#endif
    if (keyInfo->buttons & MENU_KEY_DOWN) {
        itemCount = menuGetSelectItemNum(work->menu);
        itemCount = (s8)itemCount;
        if ((s8)++work->selectedIndex >= itemCount) {
            work->selectedIndex = itemCount - 1;
        }
    }
    if (keyInfo->buttons & MENU_KEY_UP) {
        if ((s8)--work->selectedIndex < 0) {
            work->selectedIndex = 0;
        }
    }
    return 0;
}
#pragma peephole reset
