/**
 * @file window_exact_8010474C.c
 * @brief windowCloseMain (0x8010474C - 0x80104828).
 *
 * Runs the menu's close callback (state 5), unlinks the window from the
 * open-window list at winSysWork + 0x0C, releases its two sprite lists
 * and its work memory, and clears the slot.
 *
 * Window TU flags (GC/2.5, -O4,p, "-opt nopeephole"; see
 * window_exact_80104318.c): exact without the local optimization_level /
 * peephole pragmas it used to need. Carved so it can link while
 * windowClose stays a candidate; data stays extern.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_U32: the menu table stays big-endian on the PC */

extern u8 winSysWork[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* menuDataBiosGetPtr(void* key);
#endif
extern void winSpriteRelease(void* head);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u16 handle); /* GSmemLock */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);  /* GSmemFree */
#endif

#ifdef TARGET_PC /* the window record keeps its GameCube layout: +0x04 is the 32-bit menu id, +0x10 / +0x14 the
 * list links (32-bit slots, pc/ptr32.h); menuDataBiosGetPtr takes the id (canonical prototype) */
#define WINDOW_MENU_ID(w) (*(s32*)((w) + 0x04))
#else
#define WINDOW_MENU_ID(w) (*(void**)((w) + 0x04))
#endif
typedef void (*WindowCallback)(u8* window);

/* 0x8010474C | 0xDC */
void windowCloseMain(u8* window)
{
    u8* menuData;
    u8* next;

    menuData = menuDataBiosGetPtr(WINDOW_MENU_ID(window));
#ifdef TARGET_PC
    /* the menu table's tick callback is a 32-bit big-endian slot (MenuData, menu.c) */
    if (DISC_U32(menuData + 0x14) != 0) {
        window[1] = 5;
        ((WindowCallback)PC_PTR(DISC_U32(menuData + 0x14)))(window);
    }
    if (window != NULL) {
        if (PTR32_AT_T(u8, window, 0x14) == NULL) {
            PTR32_AT_SET_T(u8, winSysWork, 0x0C, PTR32_AT_T(u8, window, 0x10));
        } else {
            PTR32_AT_SET_T(u8, PTR32_AT_T(u8, window, 0x14), 0x10, PTR32_AT_T(u8, window, 0x10));
        }
        next = PTR32_AT_T(u8, window, 0x10);
        if (next != NULL) {
            PTR32_AT_SET_T(u8, next, 0x14, PTR32_AT_T(u8, window, 0x14));
        }
#else
    if (*(WindowCallback*)(menuData + 0x14) != NULL) {
        window[1] = 5;
        (*(WindowCallback*)(menuData + 0x14))(window);
    }
    if (window != NULL) {
        if (*(u8**)(window + 0x14) == NULL) {
            *(u8**)(winSysWork + 0x0C) = *(u8**)(window + 0x10);
        } else {
            *(u8**)(*(u8**)(window + 0x14) + 0x10) = *(u8**)(window + 0x10);
        }
        next = *(u8**)(window + 0x10);
        if (next != NULL) {
            *(u8**)(next + 0x14) = *(u8**)(window + 0x14);
        }
#endif
        winSpriteRelease(window + 0x1C);
        winSpriteRelease(window + 0x20);
        if (*(u16*)(window + 0xAC) != 0) {
            fn_800E24B0(*(u16*)(window + 0xAC));
            GSmemFree(*(u16*)(window + 0xAC));
            *(u32*)(window + 0xB0) = 0;
            *(u16*)(window + 0xAC) = 0;
        }
        window[0] = 0;
        *(u32*)(window + 0x04) = 0;
    }
}
