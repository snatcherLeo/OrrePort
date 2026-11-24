/**
 * @file menu_r50_80102014_prefix.c
 * @brief menu engine, 0x80102014-0x80102F38: off-screen helpers, cursor and
 *        item queries, open/close, and the per-frame menuDaemon.
 *
 * Standalone source for this split range, in retail address order. The
 * retail menu TU was built with the peephole optimizer off as a whole
 * (configure.py: -opt nopeephole); no function here needs a local
 * compiler-control pragma. -inline auto expands menuCloseSync,
 * menuCloseCustom and menuGetCursor into their later callers (menuClose,
 * menuCloseCustom, menuOpenCustom) as retail does.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/disc_types.h" /* DISC_STRUCT: the menu tables stay big-endian bytes on the PC (port-gaps.md 6.2, 10) */
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_Ui2DDraw (the 2D inventory; observation only) */
#endif

#ifndef TARGET_PC /* MWCC's PowerPC va_list; the PC uses the host's <stdarg.h> */
typedef struct __va_list_struct {
    u8 gpr;
    u8 fpr;
    u16 padding;
    u32* overflow_arg_area;
    u32* reg_save_area;
} __va_list_struct;
typedef __va_list_struct va_list[1];
#define va_start(ap, last) ((void)last, __builtin_va_info(&ap))
#define va_end(ap) ((void)0)
#else
#include <stdarg.h>
#endif

typedef struct MenuWindow MenuWindow;
typedef void (*MenuWindowCallback)(MenuWindow* window);

/* Visible cursor: row offset of the list plus the cursor within it. */
typedef struct MenuCursor {
    s8 base;
    s8 offset;
} MenuCursor;

/* Static menu definition returned by menuDataBiosGetPtr. */
typedef struct DISC_STRUCT MenuData {
    u8 soundGroup;
    u8 actionType;
    u8 pad_02;
    u8 cursorSlot;
    s16 firstItem;
    u8 pad_06[0x06];
#ifdef TARGET_PC
    DISC_PTR(void) cursorCallback; /* 32-bit big-endian slot in the table */
#else
    MenuWindowCallback cursorCallback;
#endif
#ifdef TARGET_PC
    DISC_PTR(void) buttonCallback; /* 32-bit big-endian slot in the table */
#else
    MenuWindowCallback buttonCallback;
#endif
#ifdef TARGET_PC
    DISC_PTR(void) tickCallback; /* 32-bit big-endian slot in the table */
#else
    MenuWindowCallback tickCallback;
#endif
#ifdef TARGET_PC
    DISC_PTR(void) drawCallback; /* 32-bit big-endian slot in the table */
#else
    MenuWindowCallback drawCallback;
#endif
} MenuData;
#ifdef TARGET_PC /* a table callback: a 32-bit slot read through DP() (pc/disc_types.h) */
#define MENU_DATA_CB(f) ((MenuWindowCallback)DP(f))
#else
#define MENU_DATA_CB(f) (f)
#endif

/* Menu item definition returned by menuItemBiosGetPtr. */
typedef struct DISC_STRUCT MenuItem {
    u8 selectable : 1;
    u8 last : 1;
    u8 pad_00 : 6;
    u8 pad_01[0x17];
    s16 next;
} MenuItem;

typedef struct MenuSprite { /* runtime sprite list (real pointers), not the table view: native */
    PTR32_FIELD(struct MenuSprite) next; /* 32-bit slot on the PC too (pc/ptr32.h) */
} MenuSprite;

/* Open window (tagWINDOW_WORK). */
struct MenuWindow {
    s8 flags;
    s8 phase;
    u8 phaseFrame;
    u8 pad_03;
    u32 id;
    u8 pad_08;
    s8 fadePriority;
    u8 transitionDone;
    u8 pad_0B[5];
    PTR32_FIELD(MenuWindow) next;
    u8 pad_14[5];
    s8 cursorCount;
    u8 cursorMode;
    u8 cursorFlags;
    PTR32_FIELD(MenuSprite) sprites;
    PTR32_FIELD(MenuSprite) overlaySprites;
    u8 pad_24[0x60];
    s16 x;
    s16 y;
    u8 pad_88[0x0C];
    MenuCursor cursor;
    MenuCursor previousCursor;
    u8 close;
    u8 back;
};

typedef struct MenuKeyInfo {
    u16 keys;
    u16 previousKeys;
    u16 pressedKeys;
    u16 repeatKeys;
    u16 heldKeys;
} MenuKeyInfo;

/* Window system work (WINDOW_SYS_WORK). */
typedef struct MenuSystem {
    u8 pad_00[0x0C];
    PTR32_FIELD(MenuWindow) head; /* 0x0C: 32-bit slot on the PC too (pc/ptr32.h) */
    u8 pad_10[0x84];
    u32 cursorChanged;
    u32 activeId;
} MenuSystem;

extern MenuSystem winSysWork;

/* Shared menu constants owned by game/gs_model_sdata2_8047CD98. */
extern const f32 lbl_8047CDC0;
extern const f32 lbl_8047CDC4;

/* GSlogWrite format and module name for the close-wait timeout. */
extern const char lbl_80271E10[];
extern const char lbl_8035B060[];

extern void GSlogWrite(const char* fmt, ...);
extern u32 GSthreadGetCurrentThread(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
extern void fn_800D9E4C(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(s32, s32, s32);
#endif
extern void fn_801D2404(void);
extern void fadeDaemon(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE6D0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE35C(void);
#endif
extern void spriteSetEnv(void);
extern u16 menuSeTableLookup(void*, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void cmsRunSync(void*, u32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int soundStartSE(u16);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern const MenuData* menuDataBiosGetPtr(u32 id);
#else
#define menuDataBiosGetPtr(id) ((const MenuData*)menuDataBiosGetPtr(id)) /* canonical returns void*; this file reads MenuData */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern const MenuItem* menuItemBiosGetPtr(u32 itemId);
#endif
extern void* menuSeBiosGetPtr(s32 group);
extern u32 cursorBiosSetPos(u16 slot, MenuCursor* cursor);

extern u8 menuOffScreenCheckEnable(u8 param);
extern void menuOffScreenFadeSet(f32 from, f32 to);
extern u8 menuOffScreenFadeSync(u8 param);
extern void menuOffScreenRelease(void);
extern u8 menuOffScreenCreate(u32 param);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuOffScreenSetDisp(u8 val);
#endif
extern u8 menuOffScreenSetPriority(u8 val);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MenuWindow* windowSearchID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MenuKeyInfo* windowGetKeyInfo(void);
#else
#define windowGetKeyInfo() ((MenuKeyInfo*)windowGetKeyInfo()) /* canonical returns void*; this file reads MenuKeyInfo */
#endif
extern void windowClose(MenuWindow* window, u32 mode);
extern void windowCloseMain(MenuWindow* window);
extern MenuWindow* windowOpen(s32* cursor_out, u32 menu_id, u32 parent_id,
                              s32 close_flags, s32 open_param, va_list args);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowCheckCursor(u32 id, u8 check);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 windowGetValue(u32 id);
#endif
extern void windowCreateCursorSprite(MenuWindow* window);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp(MenuSprite* sprite, u32 enable);
#endif
extern void winSpriteDraw(MenuWindow* window, MenuSprite* sprite);
extern void _menuUpdateKeyInfo__FP15WINDOW_SYS_WORK(MenuSystem* system);
extern void menuOffScreenDraw(void);
#ifdef TARGET_PC
/* frame smoothing's 2D at 120 (round 11; src/game/pc_debug_game.c): names a 2D piece (kind 0 ends it) for
 * OrreHook_2DPiece. Reads only; does nothing unless the hook is set. All platforms. (This file, not menu.c, is the
 * menu daemon the PC build compiles: build-pc/firstlink/compiled.txt.) */
extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
#endif
extern void fn_80106F98(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 winSeqCheckMove(u32 id);
#endif
extern void winSeqMoveMenu(MenuWindow* window);
extern void menuCursorNormal(MenuWindow* window);

/* 0x80102014 | 0x24 */
#ifdef TARGET_PC
/* PC: returns menuOffScreenCheckEnable's result (the GameCube body leaves its r3; menuFight.c waits on it; port-gaps
 * 37) */
u8 menuGetOffScreenFlag(void) {
    return menuOffScreenCheckEnable(0);
}
#else
void menuGetOffScreenFlag(void) {
    menuOffScreenCheckEnable(0);
}
#endif

/* 0x80102038 | 0x34 */
void menuReleaseOffScreen(f32 time) {
    menuOffScreenFadeSet(lbl_8047CDC0, time);
    menuOffScreenFadeSync(1);
    menuOffScreenRelease();
}

/* 0x8010206C | 0x54 */
void menuCreateOffScreen(f32 time) {
    menuOffScreenCreate(1);
    menuOffScreenSetDisp(1);
    menuOffScreenSetPriority(0);
    menuOffScreenFadeSet(lbl_8047CDC4, time);
}

/* 0x801020C0 | 0x78 */
s32 menuGetSelectItemNum(u32 id) {
    s32 count = 0;
    const MenuData* data;
    const MenuItem* item;
    s16 index;

    data = menuDataBiosGetPtr(id);
    if (data == NULL) {
        return 0;
    }

    index = data->firstItem;
    for (;;) {
        item = menuItemBiosGetPtr(index);
        if (item->selectable) {
            count++;
        }
        if (item->last) {
            break;
        }
        index = item->next;
    }
    return count;
}

/* 0x80102138 | 0xC0 */
s32 menuGetCursorFromItemID(u32 id, u32 itemId) {
    const MenuData* data;
    const MenuItem* item;
    s16 index;
    s32 cursor;

    data = menuDataBiosGetPtr(id);
    if (data == NULL) {
        return -3;
    }

    index = data->firstItem;
    cursor = 0;
    for (;;) {
        item = menuItemBiosGetPtr(index);
        if (index == itemId) {
            if (item->selectable) {
                return cursor;
            }
            return -1;
        }
        if (item->selectable) {
            cursor++;
        }
        if (item->last) {
            break;
        }
        index = item->next;
    }
    return -2;
}

/* 0x801021F8 | 0x5C */
void fn_801021F8(u32 id, u32 enable) {
    MenuWindow* window = windowSearchID(id);
    MenuSprite* sprite;

    if (window == NULL) {
        return;
    }
    for (sprite = P32(window->overlaySprites); sprite != NULL; sprite = P32(sprite->next)) {
        winSpriteSetDisp(sprite, enable);
    }
}

/* 0x80102254 | 0x64 */
void menuSetDisp(u32 id, u8 enable) {
    MenuWindow* window = windowSearchID(id);

    if (window == NULL) {
        return;
    }
    if (enable) {
        window->flags |= 2;
    } else {
        window->flags &= ~2;
    }
}

/* 0x801022B8 | 0xE0 */
s32 menuGetCursorItemID(u32 id) {
    MenuWindow* window;
    const MenuData* data;
    const MenuItem* item;
    s32 cursor;
    s16 index;
    s32 count;

    window = windowSearchID(id);
    if (window != NULL) {
        cursor = window->cursor.offset + window->cursor.base;
    } else {
        cursor = -1;
    }
    if (cursor == -1) {
        return 0;
    }

    data = menuDataBiosGetPtr(id);
    if (data == NULL) {
        return 0;
    }

    index = data->firstItem;
    count = 0;
    for (;;) {
        item = menuItemBiosGetPtr(index);
        if (item->selectable) {
            if (count == cursor) {
                return index;
            }
            count++;
        }
        if (item->last) {
            break;
        }
        index = item->next;
    }
    return 0;
}

/* 0x80102398 | 0x4C */
s32 menuSetCursor(u32 id, s32 cursor) {
    MenuWindow* window = windowSearchID(id);

    if (window != NULL) {
        window->cursor.offset = cursor;
    } else {
        return -1;
    }
    return 0;
}

/* 0x801023E4 | 0x44 */
s32 menuGetCursor(u32 id) {
    MenuWindow* window = windowSearchID(id);

    if (window != NULL) {
        return window->cursor.offset + window->cursor.base;
    }
    return -1;
}

/* 0x80102428 | 0x98 */
s32 menuCloseSync(u32 id, u8 wait) {
    if (wait) {
        for (;;) {
            if (windowSearchID(id) == NULL) {
                return 0;
            }
            if (GSthreadGetCurrentThread() == 0) {
                GSlogWrite(lbl_80271E10, lbl_8035B060, id);
                break;
            }
            _threadSwitch();
        }
    } else {
        return windowSearchID(id) != NULL;
    }
    return 0;
}

/* 0x801024C0 | 0x28 */
void menuCloseFloor(void) {
    windowClose(NULL, 4);
}

/* 0x801024E8 | 0x28 */
void fn_801024E8(void) {
    windowClose(NULL, 4);
}

/* 0x80102510 | 0x58 */
void menuClose(s32 id) {
    MenuWindow* window;

    if (id == 0) {
        id = windowGetActiveID();
    }
    window = windowSearchID(id);
    if (window != NULL) {
        windowClose(window, 0);
        menuCloseSync(id, 0);
    }
}

/* 0x80102568 | 0xB8 */
s32 menuCloseCustom(u32 id, u32 mode, u8 wait) {
    MenuWindow* window = windowSearchID(id);

    if (window == NULL) {
        return 1;
    }
    windowClose(window, mode);
    menuCloseSync(id, wait);
    return 0;
}

/* 0x80102620 | 0x2C */
s32 menuIsCheck(u32 id) {
    return windowSearchID(id) != NULL;
}

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 menuOpenCustom(u32 menu_id, u32 parent_id, s32* cursor_out,
                   u32 close_flags, u8 check_cursor, s32 open_param, ...);
#endif

/* 0x8010264C | 0x58 */
s32 menuOpen(u32 menu_id, u8 check_cursor) {
    return menuOpenCustom(menu_id, windowGetActiveID(), NULL, 0, check_cursor, 0);
}

/* 0x801026A4 | 0x1C4 */
s32 menuOpenCustom(u32 menu_id, u32 parent_id, s32* cursor_out,
                   u32 close_flags, u8 check_cursor, s32 open_param, ...) {
    va_list args;
    MenuWindow* window;
    const MenuData* data;
    MenuCursor cursor;
    s32 value = 0;

    va_start(args, open_param);
    window = windowOpen(cursor_out, menu_id, parent_id, close_flags,
                        open_param, args);

    if (check_cursor) {
        windowCheckCursor(menu_id, check_cursor);
        value = windowGetValue(menu_id);
        data = menuDataBiosGetPtr(menu_id);
        if (data->cursorSlot != 0) {
            cursor = window->cursor;
            cursorBiosSetPos(data->cursorSlot, &cursor);
        }
        if (cursor_out != NULL) {
            *cursor_out = menuGetCursor(menu_id);
        }
    }

    if (close_flags & 1) {
        menuCloseCustom(menu_id, 0, check_cursor);
    }
    return value;
}

/* 0x80102868 | 0x48 */
void menuSetPosition(u32 id, s16 x, s16 y) {
    MenuWindow* window = windowSearchID(id);

    if (window == NULL) {
        return;
    }
    window->x = x;
    window->y = y;
}

/* Plays the menu's sound effect for the given event, if it has one. */
#define MENU_PLAY_SE(window, event)                                         \
    do {                                                                    \
        void* sound = menuSeBiosGetPtr(                                     \
            menuDataBiosGetPtr((window)->id)->soundGroup & 7);              \
        if (sound != NULL) {                                                \
            u32 se = menuSeTableLookup(sound, (event));                           \
            if (se != 0) {                                                  \
                soundStartSE(se);                                            \
            }                                                               \
        }                                                                   \
    } while (0)

/* 0x801028B0 | 0x624 */
void menuDaemon(void) {
    MenuSystem* system;
    MenuWindow* active;
    MenuWindow* node;
    const MenuData* data;
    MenuSprite* sprite;
    u32 i;
    u16 keys;
    u8 doFade = 1;

    fn_801D2404();
    active = windowSearchID(windowGetActiveID());
    _menuUpdateKeyInfo__FP15WINDOW_SYS_WORK(&winSysWork);
    fn_800D9E4C(0);
    menuOffScreenDraw();
    fn_800D9ED8(1);
    system = &winSysWork;

    for (node = P32(system->head); node != NULL; node = P32(node->next)) {
        data = menuDataBiosGetPtr(node->id);
        switch (node->phase) {
        case 0:
            node->phase = 1;
            node->phaseFrame = 0;
            break;
        case 1:
            if ((u8)winSeqCheckMove(node->id) == 0) {
                if ((u8)((data->actionType >> 2) & 7) == 0) {
                    node->close = 1;
                }
                node->phase = 2;
                node->phaseFrame = 0;
#ifdef TARGET_PC
                {
                    /* OrreHook_WindowReady (observation hook; docs/port/hook-points.md): a window finished opening and
                     * reads buttons from now on. Default: nothing registered. Replays time menu presses from it. */
                    extern void (*OrreHook_WindowReady)(u32 windowId);
                    if (OrreHook_WindowReady != NULL) {
                        OrreHook_WindowReady(node->id);
                    }
                }
#endif
            }
            break;
        case 2:
            if (node->transitionDone != 0) {
                MENU_PLAY_SE(node, 5);
                node->phase = 3;
                node->phaseFrame = 0;
            }
            break;
        case 3:
#ifdef TARGET_PC
            {
                /* OrreHook_WindowCloseNow (docs/port/hook-points.md): this window's closing sequence is skipped and it
                 * is closed now (the PC screen's right-hand window, 0x1A, slides away for a second behind the black
                 * screen when PC MENU's PC is left). Nothing registered: the window closes when its sequence ends */
                extern s32 (*OrreHook_WindowCloseNow)(u32 windowId);
                if (OrreHook_WindowCloseNow != NULL && OrreHook_WindowCloseNow(node->id) != 0) {
                    node->phase = 4;
                    node->phaseFrame = 0;
                    break;
                }
            }
#endif
            if ((u8)winSeqCheckMove(node->id) == 0) {
                node->phase = 4;
                node->phaseFrame = 0;
            }
            break;
        case 4:
            break;
        }

        if (active != NULL && node == active) {
            if (node->close == 0) {
                switch (node->phase) {
                case 2:
                    switch ((data->actionType >> 2) & 7) {
                    case 0:
                        node->close = 1;
                        break;
                    case 1:
                        /* menuButtonNormal(node), expanded in place */
                        if (node != NULL) {
                            keys = ((MenuKeyInfo*)windowGetKeyInfo())->pressedKeys;
                            if (keys & MENU_KEY_A) {
                                node->close = 1;
                            }
                            if (keys & MENU_KEY_B) {
                                node->close = 1;
                                node->back = 1;
                            }
#ifdef TARGET_PC
                            {
                                /* OrreHook_MenuBackKeys (docs/port/hook-points.md): more buttons that act as B on this
                                 * menu (the field pause menu's Start). Default: nothing registered, B alone */
                                extern u32 (*OrreHook_MenuBackKeys)(u32 menuId);
                                if (OrreHook_MenuBackKeys != NULL && (keys & OrreHook_MenuBackKeys(node->id)) != 0) {
                                    node->close = 1;
                                    node->back = 1;
                                }
                            }
#endif
                        }
                        break;
                    case 3:
                        cmsRunSync(MENU_DATA_CB(data->buttonCallback), 0);
                        break;
                    case 4:
                        MENU_DATA_CB(data->buttonCallback)(node);
                        break;
                    }

                    if ((node->cursorFlags & 8) != 0 && node->back != 0) {
                        node->back = 0;
                        node->close = 0;
                    }

                    if (node->close != 0) {
                        system->cursorChanged = 0;
                        system->activeId = node->id;
                    } else if ((((MenuKeyInfo*)windowGetKeyInfo())->keys & 0x8000) != 0) {
                        system->cursorChanged = 1;
                        system->activeId = node->id;
                        if ((node->cursorFlags & 0x10) == 0) {
                            node->close = 1;
                            node->back = 1;
                        }
                    }
                    break;
                case 3:
                    break;
                }

                if (node->back != 0) {
                    MENU_PLAY_SE(node, 3);
                } else if (node->close != 0) {
                    MENU_PLAY_SE(node, 2);
                }
                if (node->close != 0) {
                    fn_80106F98(node->id);
                }
            }

            if (node->close == 0) {
                MenuCursor oldPos;
                MenuCursor newPos;
                u8 moved;

                switch (node->phase) {
                case 2:
                    node->previousCursor = node->cursor;
                    if (node->cursorCount > 0) {
                        switch (node->cursorMode) {
                        case 1:
                        case 2:
                            menuCursorNormal(node);
                            break;
                        case 3:
                            cmsRunSync(MENU_DATA_CB(data->cursorCallback), 0);
                            break;
                        case 4:
                            MENU_DATA_CB(data->cursorCallback)(node);
                            break;
                        }
                        oldPos = node->previousCursor;
                        newPos = node->cursor;
                        if (oldPos.base == newPos.base &&
                            oldPos.offset == newPos.offset) {
                            moved = 0;
                        } else {
                            moved = 1;
                        }
                        if (moved) {
                            windowCreateCursorSprite(node);
                            MENU_PLAY_SE(node, 1);
                        }
                    }
                    break;
                }
            }
        }

        if ((node->flags & 4) != 0) {
            winSeqMoveMenu(node);
            if (MENU_DATA_CB(data->tickCallback) != NULL) {
                for (i = 0; i < GSgetFrameStep(); i++) {
                    MENU_DATA_CB(data->tickCallback)(node);
                }
            }
        }
    }

    for (node = P32(system->head); node != NULL; node = P32(node->next)) {
        if (node->phase == 4) {
            windowCloseMain(node);
        }
    }

    for (node = P32(system->head); node != NULL; node = P32(node->next)) {
        if (doFade != 0 && node->fadePriority >= 0x50) {
            doFade = 0;
            fadeDaemon();
            fn_800D9ED8(1);
        }
        if ((node->flags & 2) != 0) {
#ifdef TARGET_PC
            if (OrreHook_Ui2DDraw != NULL) {
                /* OrreHook_Ui2DDraw (observation; widescreen groundwork's 2D inventory, docs/port/widescreen-plan.md
                 * section 6): this window is drawn at its origin (the whole-window offset hook's future place, just
                 * before fn_800FE6D0), and whether it has a draw callback (code-drawn content the sprite reports don't
                 * see). Plain reads only. All platforms */
                OrreUi2DPiece piece = {0};
                piece.source = ORRE_UI2D_WINDOW;
                piece.window = node->id;
                piece.activeWindow = windowGetActiveID();
                piece.x = node->x;
                piece.y = node->y;
                piece.recordMode = -1;
                piece.flags = MENU_DATA_CB(menuDataBiosGetPtr(node->id)->drawCallback) != NULL ? 1u : 0u;
                OrreHook_Ui2DDraw(&piece);
            }
#endif
            fn_800FE6D0(node->x, node->y);
            fn_800FE35C();
            spriteSetEnv();
            for (sprite = P32(node->sprites); sprite != NULL; sprite = P32(sprite->next)) {
#ifdef TARGET_PC
                /* OrreHook_2DPiece (observation; frame smoothing's 2D at 120, round 11): names this sprite for the
                 * draws it makes, its label's text included (group 1: a menu sprite, the menu's id; the sprite's own
                 * record as its id). The port decides which menus blend (pc_smooth.c: the pause menus) */
                pc_orre_2d_tag(0x10000 | (s32)(node->id & 0xFFFF), 0, sprite, 0.0f);
#endif
                winSpriteDraw(node, sprite);
#ifdef TARGET_PC
                pc_orre_2d_tag(0, 0, NULL, 0.0f);
#endif
            }
            for (sprite = P32(node->overlaySprites); sprite != NULL;
                 sprite = P32(sprite->next)) {
#ifdef TARGET_PC
                pc_orre_2d_tag(0x10000 | (s32)(node->id & 0xFFFF), 0, sprite, 0.0f); /* as above */
#endif
                winSpriteDraw(node, sprite);
#ifdef TARGET_PC
                pc_orre_2d_tag(0, 0, NULL, 0.0f);
#endif
            }
            data = menuDataBiosGetPtr(node->id);
            if (MENU_DATA_CB(data->drawCallback) != NULL) {
#ifdef TARGET_PC
                /* OrreHook_2DPiece (observation, round 11): the menu's own drawing (group 2: the menu's id; the
                 * window as its id) */
                pc_orre_2d_tag(0x20000 | (s32)(node->id & 0xFFFF), 0, node, 0.0f);
#endif
                MENU_DATA_CB(data->drawCallback)(node);
#ifdef TARGET_PC
                pc_orre_2d_tag(0, 0, NULL, 0.0f);
#endif
            }
        }
    }

    fn_800FE6D0(0, 0);
    fn_800FE35C();
    gfxSetBlendMode(1, 6, 7);
    if (doFade != 0) {
        fadeDaemon();
    }
    fn_800D9ED8(0);
    fn_800D9E4C(1);
}

/* 0x80102ED4 | 0x64 */
void menuButtonNormal(MenuWindow* window) {
    u16 keys;

    if (window == NULL) {
        return;
    }
    keys = ((MenuKeyInfo*)windowGetKeyInfo())->pressedKeys;
    if (keys & MENU_KEY_A) {
        window->close = 1;
    }
    if (keys & MENU_KEY_B) {
        window->close = 1;
        window->back = 1;
    }
}
