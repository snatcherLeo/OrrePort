/**
 * @file menuColosseumBattle_tail_exact_8005D6A8.c
 * @brief Byte-exact menuColosseumBattle.cpp range, 0x8005D6A8 - 0x8005D7F8.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/disc_types.h" /* the menu sound table is common_rel data */

typedef struct MenuKeyInfo {
    u8 pad_00[4];
    u16 buttons;
    u16 repeatButtons;
} MenuKeyInfo;

typedef struct MenuWindow {
    u8 pad_00[0x4C];
    s32 nextMenu;
    u8 pad_50[0x10];
    PTR32_FIELD(void) partyState; /* window +0x60: 32-bit slot on the PC too (pc/ptr32.h) */
    u8 pad_64[0x31];
    s8 cursor;
    u8 pad_96;
    s8 previousCursor;
    u8 accepted;
    u8 canceled;
} MenuWindow;

typedef struct DISC_STRUCT MenuSeTable { /* common_rel menu sound table: big-endian view on the PC */
    u16 field_00;
    u16 field_02;
    u16 field_04;
    u16 field_06;
    u16 field_08;
} MenuSeTable;

typedef union ColosseumBitMasks {
    u32 word;
    u8 bytes[4];
} ColosseumBitMasks;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStart(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuCloseSync(s32, s32);
#endif

extern u8 lbl_8047A5A8;
extern const ColosseumBitMasks lbl_8047BF40;

/* Address: 0x8005D6A8 | Size: 0x90 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
void fn_8005D6A8(MenuWindow* window) {
    MenuKeyInfo* keys = (MenuKeyInfo*)windowGetKeyInfo();
    ColosseumBitMasks masks;
    s32 cursor = window->cursor;

    masks.word = lbl_8047BF40.word;
    if (cursor >= 0 && cursor <= 3 && (keys->buttons & MENU_KEY_A) != 0) {
#ifdef TARGET_PC
        /* port-gaps 62 (the reference playthrough P06 note 4): the table is the u32 0x01020408 (sdata2_8047BF10.c) and the GameCube reads
         * its bytes in memory order, 01 02 04 08 (circle, square, triangle, heart). The PC stores the word
         * little-endian, so bytes[0] was 08 and A on the circle toggled the heart. Take the byte the console sees. */
        lbl_8047A5A8 ^= (u8)(masks.word >> (24 - 8 * cursor));
#else
        lbl_8047A5A8 ^= masks.bytes[cursor];
#endif
        soundStart(0x3C6, 0, 0xFF, 0);
    } else {
        menuButtonNormal(window);
    }
}
#pragma pop

/* Address: 0x8005D738 | Size: 0x60 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
u32 fn_8005D738(u8 flags) {
    s32 menuResult;

    lbl_8047A5A8 = flags;
    menuResult = menuOpen(0x9D, 1);
    menuClose(0x9D);
    menuCloseSync(0x9D, 1);
    if (menuResult == 0x73D) {
        return lbl_8047A5A8;
    } else {
        return 0xFF;
    }
}
#pragma pop

/* Address: 0x8005D798 | Size: 0x60 */
u32 menuSeTableLookup(MenuSeTable* table, u8 index) {
    u32 value = 0;

    switch (index) {
    case 1:
        value = table->field_04;
        break;
    case 2:
        value = table->field_06;
        break;
    case 3:
        value = table->field_08;
        break;
    case 4:
        value = table->field_00;
        break;
    case 5:
        value = table->field_02;
        break;
    }

    return value;
}
