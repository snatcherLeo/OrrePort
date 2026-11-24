/**
 * @file menuColosseumBattle_candidate_80058DCC.c
 * @brief menuColosseumBattle.cpp candidate range, 0x80058DCC - 0x80058F08.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC (port-gaps 54) */

typedef struct MenuWindow {
    u8 field_00;
    s8 state;
    s8 initialized;
    u8 field_03;
    s32 windowId;
    u8 field_08[0x58];
    PTR32_FIELD(void) partyState; /* window +0x60: 32-bit slot on the PC too (pc/ptr32.h) */
    u8 field_64[0x31];
    s8 cursor;
    u8 field_96[2];
    u8 accepted;
    u8 canceled;
} MenuWindow;

typedef struct DISC_STRUCT MenuCursorItem { /* PC: big-endian menu item table entry (port-gaps 54) */
    u8 field_00[2];
    s16 row;
    s16 column;
} MenuCursorItem;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroGetStatus(s32, s32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MenuCursorItem* windowGetCursorToItem(MenuWindow*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 windowGetActiveID(void);
#endif
extern void fn_80057830(s32, s32, s32);
extern s32 fn_80058AF0(MenuWindow*);

s32 fn_80058DCC(MenuWindow* window)
{
    MenuCursorItem* item;
    s32 slot;
    s32* party;

    party = (s32*)P32(window->partyState);
    switch (window->state) {
    case 0:
        if (!window->initialized) {
            window->initialized = 1;
        }
        if (*party != 0) {
            for (slot = 0; slot < 6; slot++) {
                if (!pokemonCheckValid(PC_PTR(heroGetStatus(0, 3, (u16)slot)))) {
                    break;
                }
            }
            if (slot >= 6) {
                slot = -1;
            }
            if (slot >= 0) {
                window->cursor = (s8)slot;
            }
        }
        item = windowGetCursorToItem(window);
        fn_80057830(item->row, item->column, 0);
        break;
    case 2:
        if (window->windowId == windowGetActiveID() &&
            fn_80058AF0(window) == 0) {
            window->accepted = 1;
            window->canceled = 1;
        }
        break;
    case 3:
        if (!window->initialized) {
            window->initialized = 1;
        }
        break;
    }
    return 0;
}
