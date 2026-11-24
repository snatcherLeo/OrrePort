/**
 * @file window_exact_80104318.c
 * @brief windowGetCursorToItem, windowGetValue, fn_801044D0
 *        (0x80104318 - 0x80104530).
 *
 * Flags: the window TU is built with the project's -O4,p but with the
 * peephole pass off, like the winMsg TU that follows it (see
 * win_msg_exact_80105A3C.c). Retail keeps "clrlwi r0,rX,24; cmplwi r0,0"
 * pairs and uninverted "bne; b" branches. With "-opt nopeephole" and no
 * local pragmas, windowGetCursorToItem, fn_801044D0, windowCloseMain and
 * windowInit (all previously exact only under local optimization_level /
 * peephole pragmas) are exact, and so are windowGetValue and
 * windowCheckCursor. The compiler is GC/2.5, as for winMsg: every exact
 * window function is identical under GC/1.3 and GC/2.5, while fn_80105634
 * (93.5 -> 94.8%) and windowOpen (80.6 -> 81.9%) only move closer with
 * GC/2.x. That one flag set (GC/2.5, -O4,p, "-opt nopeephole") also
 * replaces the old per-unit "-schedule off" (windowSearchItemID), "-O2",
 * "-O1" and GC/2.0 settings without losing any function. Carved at
 * function boundaries so these can link while the rest of the window TU
 * stays a candidate; data stays extern.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT: the menu tables stay big-endian bytes on the PC (port-gaps.md 6.2, 10) */
#include "game/window_search.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuDataBiosGetPtr(void* key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuItemBiosGetPtr(s16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorItemID(s32 windowId);
#endif

/* Menu item record from the menu item BIOS table. */
typedef struct DISC_STRUCT WinMenuItem {
    u8 selectable : 1;
    u8 last : 1;
    u8 flags : 6;
    u8 pad_01[0x17];
    s16 next;
} WinMenuItem;

#ifdef TARGET_PC /* the window record keeps its GameCube layout: +0x04 is the 32-bit menu id, +0x10 / +0x14 the
 * list links (32-bit slots, pc/ptr32.h); menuDataBiosGetPtr takes the id (canonical prototype) */
#define WINDOW_MENU_ID(w) (*(s32*)((w) + 0x04))
#else
#define WINDOW_MENU_ID(w) (*(void**)((w) + 0x04))
#endif
/* 0x80104318 | 0x8C */
WinMenuItem* windowGetCursorToItem(u8* window)
{
    WinMenuItem* item;
    s32 index;

    item = menuDataBiosGetPtr(WINDOW_MENU_ID(window));
    item = menuItemBiosGetPtr(DISC_S16((u8*)item + 0x04));
    index = 0;
    while (1) {
        if (item->selectable) {
            if ((s8)window[0x95] == index) {
                return item;
            }
            index++;
        }
        if (item->last) {
            break;
        }
        item = menuItemBiosGetPtr(item->next);
    }
    return NULL;
}

/* 0x801043A4 | 0x12C */
s32 windowGetValue(s32 id)
{
    u8* window = windowSearchIDInline(id);
    u8* menuData;
    s32 result;

    if (window != NULL) {
        if (window[0x99] != 0) {
            result = -1;
        } else {
            menuData = menuDataBiosGetPtr(WINDOW_MENU_ID(window));
            if (menuData != NULL) {
                /* The value kind is a 2-bit field, so every case is
                 * covered; retail has no default store either. */
                switch ((menuData[0] >> 6) & 3) {
                case 0:
                    result = 0;
                    break;
                case 1:
                    result = (s8)window[0x94] + (s8)window[0x95];
                    break;
                case 2:
                    result = *(s32*)(window + 0x80);
                    break;
                case 3:
                    result = menuGetCursorItemID(id);
                    break;
                }
            } else {
                result = (s8)window[0x94] + (s8)window[0x95];
            }
        }
    } else {
        result = -1;
    }
    return result;
}

/* 0x801044D0 | 0x60 */
s32 fn_801044D0(s32 id, u16* cursor)
{
    u8* window = windowSearchIDInline(id);

    if (window != NULL) {
        *(u16*)(window + 0x94) = *cursor;
        return 1;
    }
    return 0;
}
