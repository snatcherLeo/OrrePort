/**
 * @file menu_debug_range_8005DA48.c
 * @brief debug menu TU, 0x8005DA48 - 0x8005DFC8.
 *
 * Split out of the former game/menu/menuCB_Battle.c bucket (2026-07-07) into
 * true XD source-unit segments. dbgMenuLog-family / dbgMenuFieldCamera-family /
 * menuDbgItem-family cluster in XD's dbg menu source (XD 0x8000DEE8-0x8000E53C). 2 XD anchors
 * (dbgMenuFieldCameraChangeDisp, menuDbgItemCreate), monotonic with exact
 * size matches. All functions asm-only.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"

#if !defined(MENU_DEBUG_RANGE_SPLIT) || defined(MENU_DEBUG_LOG_CHANGE_DISP)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 dbgMenuLogChangeDisp(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 menuIsCheck(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, ...);
#endif

    if (menuIsCheck(0xBB)) {
        menuClose(0xBB);
    } else {
        menuOpenCustom(0xBB, 0, 0, 0, 1, 0);
    }

    return 0;
}
#pragma peephole reset
#endif

typedef struct DebugMenuKeyInfo {
    u8 _0[4];
    u16 trigger;
    u16 repeat;
} DebugMenuKeyInfo;

#if !defined(MENU_DEBUG_RANGE_SPLIT) || defined(MENU_DEBUG_LOG_DRAW)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 dbgMenuLogDraw(void* window)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern DebugMenuKeyInfo* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetParam(void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void windowSetParam(void*, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetZMode(s32, s32, s32);
#endif
    extern void menuDrawRectOutline(s32, s32, s32, s32, u32*);
    extern s32 GSlogGetLineCount(void);
    extern char* GSlogGetLine(s32);
    extern void GSmsgPrintf(s32, s32, u32, char*, ...);
    extern u32 lbl_8047BF48;
    u32 color[2];
    s32 y;
    s32 i;
    s32 lines;
    s32 line;

    color[1] = lbl_8047BF48;
    if (((DebugMenuKeyInfo*)windowGetKeyInfo())->trigger & MENU_KEY_Z) {
        windowSetParam(window, 0, (windowGetParam(window, 0) + 1) % 4);
    }

    {
        s32 width;

        switch (windowGetParam(window, 0)) {
        case 0:
        default:
            lines = 10;
            width = 0x82;
            y = 0x145;
            break;
        case 1:
            lines = 10;
            width = 0x82;
            y = 0x27;
            break;
        case 2:
            lines = 0x20;
            width = 0x1A0;
            y = 0x27;
            break;
        case 3:
            lines = 1;
            width = 0xD;
            y = 0x1BA;
            break;
        }

        gfxSetZMode(0, 7, 2);
        color[0] = color[1];
        menuDrawRectOutline(0xF, y - 5, 0x25D, width + 0x12, &color[0]);
    }
    for (i = 0; i < lines; i++) {
        line = i + GSlogGetLineCount() - lines;
        if (line >= 0) {
            GSmsgPrintf(0x14, y, 0xC0C0C0FF, GSlogGetLine(line));
        }
        y += 13;
    }
    return 0;
}
#pragma pop
#endif

#if !defined(MENU_DEBUG_RANGE_SPLIT) || defined(MENU_DEBUG_ITEM_CURSOR)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 menuDbgItemCreateCursor(u8* window)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern DebugMenuKeyInfo* windowGetKeyInfo(void);
#endif
    extern s32 Num;
    extern s32 ItemID;
    extern s32 lbl_80478BD8;
    DebugMenuKeyInfo* keys;

    keys = windowGetKeyInfo();
    if (keys->repeat & MENU_KEY_UP) {
        if ((s8)--window[0x95] < 0) {
            window[0x95] = 0;
        }
    }
    if (keys->repeat & MENU_KEY_DOWN) {
        if ((s8)++window[0x95] > 1) {
            window[0x95] = 1;
        }
    }

    switch ((s8)window[0x95]) {
    case 0:
        keys = windowGetKeyInfo();
        if (keys->repeat & MENU_KEY_RIGHT) {
            ItemID++;
            if (ItemID >= lbl_80478BD8) ItemID = lbl_80478BD8 - 1;
        }
        if (keys->repeat & MENU_KEY_R) {
            ItemID += 10;
            if (ItemID >= lbl_80478BD8) ItemID = lbl_80478BD8 - 1;
        }
        if (keys->repeat & MENU_KEY_LEFT) {
            ItemID--;
            if (ItemID < 0) ItemID = 0;
        }
        if (keys->repeat & MENU_KEY_L) {
            ItemID -= 10;
            if (ItemID < 0) ItemID = 0;
        }
        break;
    case 1:
        keys = windowGetKeyInfo();
        if (keys->repeat & MENU_KEY_RIGHT) {
            Num++;
            if (Num > 999) Num = 999;
        }
        if (keys->repeat & MENU_KEY_R) {
            Num += 10;
            if (Num > 999) Num = 999;
        }
        if (keys->repeat & MENU_KEY_LEFT) {
            Num--;
            if (Num < 0) Num = 0;
        }
        if (keys->repeat & MENU_KEY_L) {
            Num -= 10;
            if (Num < 0) Num = 0;
        }
        break;
    }
    return 0;
}
#pragma pop
#endif

#if !defined(MENU_DEBUG_RANGE_SPLIT) || defined(MENU_DEBUG_FIELD_CAMERA)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 dbgMenuFieldCameraChangeDisp(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 menuIsCheck(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuSetPosition(s32 menuId, s32 x, s32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std092_getCurrentRoomID(void);
#endif
    extern void fn_801176C8(u32 floorId);
    extern u32 fn_80117AD4(void);
    extern u32 gSelected_row;
    extern u32 gSelected_column;
    extern u32 gSelectedMenu_column;
    extern u8 gSaveResult;
    u32 floorId;

    if (!menuIsCheck(0xCA)) {
        floorId = std092_getCurrentRoomID();
        if (floorId != fn_80117AD4()) {
            fn_801176C8(floorId);
            gSelected_row = 0;
            gSelected_column = 0;
            gSelectedMenu_column = 0;
        }

        gSaveResult = 0;
        menuOpenCustom(0xCA, 0, 0, 0, 1, 0);
        menuSetPosition(0xCA, 0xC, 0xA);
    }

    return 0;
}
#pragma peephole reset
#endif

#if !defined(MENU_DEBUG_RANGE_SPLIT) || defined(MENU_DEBUG_ITEM_CREATE)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
void menuDbgItemCreate(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpen(s32 menuId, s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80142984(u16 itemId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom(s32 menuId, s32 mode, s32 wait);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 heroItemAddItemDataId(void* hero, u16 itemId, u16 count,
                                     s32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32 menuId, s32 wait);
#endif
    extern s32 lbl_8047A5C0;
    extern s32 Num;
    extern s32 ItemID;
    s32 result;

    if (lbl_8047A5C0 == 0) {
        ItemID = 1;
        lbl_8047A5C0 = 1;
    }

    Num = 1;
    for (;;) {
        result = menuOpen(0xCB, 1);
        if (result == -1) {
            break;
        }
        if (fn_80142984((u16)ItemID) == 0) {
            continue;
        }
        if (Num < 1 || Num > 999) {
            continue;
        }

        result = menuOpen(0x44, 1);
        menuCloseCustom(0x44, 0, 1);
        if (result == 0) {
            heroItemAddItemDataId(0, (u16)ItemID,
                                  (u16)Num, -1);
        }
    }

    menuClose(0xCB);
    menuCloseSync(0xCB, 1);
}
#pragma peephole reset
#endif
