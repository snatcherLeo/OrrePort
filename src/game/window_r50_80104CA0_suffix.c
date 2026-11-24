/**
 * @file window_r50_80104CA0_suffix.c
 * @brief windowCreateCursorSprite, windowOpen and _winCalcWindowSize
 *        (0x80104CA0 - 0x80105410), exact standalone owner.
 *
 * Not linked (coordinator, 2026-09-28): lbl_8047CDEC is not a shared global
 * but the window TU's own compiler-pool 1.0f. Only windowOpen and
 * windowDrawSprite2 (another chunk) read it, and it is misfiled in
 * gs_model_sdata2_8047CD98.c. Reading it through an extern stand-in is the
 * rejected "synthetic compiler data" form, and the pool can't move into this
 * unit while windowDrawSprite2 shares it. The unit stays CodeCandidate until
 * the window TU's pool pairs honestly (e.g. a unit that also holds
 * windowDrawSprite2 and owns the literal).
 *
 * Window TU flags (GC/2.5, -O4,p, "-opt nopeephole"; see
 * window_exact_80104318.c). Data stays extern (winSysWork, the 1.0f
 * constant lbl_8047CDEC and the _winGetNewWork error string lbl_80271E94).
 *
 * Pokemon XD references: TeamOrre/xd-decomp config/GXXE01/symbols.txt
 * @4989794e, bodies in trevor403/xd-asm @b1087f18.
 *
 * windowOpen (XD 0x80115FAC, code/func_FUN_80115fac.s) calls two XD statics
 * that Colosseum expands in place:
 * - winGetNewWork: XD _winGetNewWork__Fv (0x80116430, 0xA0,
 *   code/func_FUN_80116430.s). It scans the work array with a CTR loop
 *   (lhz count; mtctr; ... bdnz), memsets the first free slot and marks it
 *   (state 7, the two 1.0f fields, -1), or calls GSlog with the "Error!!
 *   _winGetNewWork()" message and returns NULL. The calls (memset, then
 *   GSlog on failure) are the same, in the same order, and the retail
 *   expansion has the same shape (0x80104FDC - 0x80105054).
 * - winInsertWork: XD _winInsertWork__FP14tagWINDOW_WORKSc (0x801163A0,
 *   0x90, no calls, code/func_FUN_801163a0.s). It inserts the window into
 *   the open list in priority order; the retail expansion
 *   (0x80105118 - 0x80105198) is the same code.
 * Both are named after the XD functions (leading underscore dropped):
 * sister-title clause, docs/CAMPAIGN_OPERATIONS.md.
 *
 * Colosseum's own same-TU functions that retail inlines here, kept as
 * compile-only copies (as in musyx seq_exact_80146E88.c):
 * windowGetActiveID (0x801046B8) and windowSearchID (0x80104704), expanded
 * four times in windowOpen, and windowGetCursorToItem (0x80104318), expanded
 * in windowCreateCursorSprite. XD's windowCreateCursorSprite calls
 * windowGetCursorToItem (0x80115608) at the same point. Each copy compiled
 * alone gives the linked function's bytes (window_exact_801046B8.c,
 * window_exact_80104704.c, window_exact_80104318.c).
 *
 * cursorBiosGetPos returns a 2-byte struct in r3: retail stores its value
 * with `srwi r0,r3,16; sth`, which is the struct copy. The menu tables are
 * read through const pointers, as the XD mangling of _winCalcWindowSize
 * (PC13MENU_ITEM_dd, pointer to const MENU_ITEM_dd) shows.
 * _winCalcWindowSize keeps that XD-derived symbol name, but Colosseum's
 * takes (item, width, height), and menuItemBiosGetPtr here takes only the
 * item index.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT: the menu tables stay big-endian bytes on the PC (port-gaps.md 6.2, 10) */
#include "crt/stdarg.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_CursorSpriteAt (word pictures per language) */
#endif

typedef struct WindowWork WindowWork;

typedef struct WindowCursor {
    s8 x;
    s8 y;
} WindowCursor;

struct WindowWork {
    s8 state;             /* 0x00 */
    u8 step;              /* 0x01 */
    s8 reopen;            /* 0x02 */
    u8 pad_03;
    s32 id;               /* 0x04 */
    u8 pad_08;
    s8 priority;          /* 0x09 */
    u8 closeFlag;         /* 0x0A */
    u8 port;              /* 0x0B */
    PTR32_FIELD(WindowWork) parent; /* 0x0C: 32-bit slots on the PC too (pc/ptr32.h) */
    PTR32_FIELD(WindowWork) next;   /* 0x10 */
    PTR32_FIELD(WindowWork) prev;   /* 0x14 */
    u8 unk_18;            /* 0x18 */
    u8 itemCount;         /* 0x19 */
    u8 unk_1A;            /* 0x1A */
    u8 openFlags;         /* 0x1B */
    PTR32_FIELD(void) itemSprites;   /* 0x1C */
    PTR32_FIELD(void) cursorSprites; /* 0x20 */
    u8 pad_24[0x60 - 0x24];
    u32 args[8];          /* 0x60 */
    u8 pad_80[4];
    s16 unk_84;           /* 0x84 */
    s16 unk_86;           /* 0x86 */
    s32 unk_88;           /* 0x88 */
    f32 unk_8C;           /* 0x8C */
    f32 unk_90;           /* 0x90 */
    WindowCursor cursor;  /* 0x94 */
    u8 pad_96[2];
    u8 unk_98;            /* 0x98 */
    u8 unk_99;            /* 0x99 */
    u8 pad_9A[0xB4 - 0x9A];
};

typedef struct WindowSystemWork {
    u16 count;            /* 0x00 */
    u8 pad_02[2];
    s32 activeID;         /* 0x04 */
    PTR32_FIELD(WindowWork) works; /* 32-bit slot on the PC too (pc/ptr32.h) */    /* 0x08 */
    PTR32_FIELD(WindowWork) windows; /* 32-bit slot on the PC too (pc/ptr32.h) */  /* 0x0C */
} WindowSystemWork;

typedef struct DISC_STRUCT MenuItem {
    u8 selectable : 1;    /* 0x00 bit 7 */
    u8 last : 1;          /* 0x00 bit 6 */
    u8 unk0_0 : 6;
    u8 unk_01;
    s16 x;                /* 0x02 */
    s16 y;                /* 0x04 */
    s16 width;            /* 0x06 */
    s16 height;           /* 0x08 */
    s16 spriteIndex;      /* 0x0A */
    s16 cursorSprite;     /* 0x0C */
    u8 pad_0E[2];
#ifdef TARGET_PC
    u32 message;          /* 0x10: a message key (GSmsgGetRect takes it as a key), big-endian in the table */
#else
    void* message;        /* 0x10 */
#endif
    u8 pad_14[4];
    s16 next;             /* 0x18 */
} MenuItem;

typedef struct DISC_STRUCT MenuSprite {
    u16 last : 1;         /* 0x00 bit 15 */
    u16 unk0_14 : 1;
    u16 type : 2;         /* 0x00 bits 12-13 */
    u16 sequence : 12;    /* 0x00 bits 0-11 */
    u8 unk_02[3];
    s8 offsetX;           /* 0x05 */
    s8 offsetY;           /* 0x06 */
    u8 unk_07;
    s16 unk_08;
    s16 unk_0A;
    s16 unk_0C;
    s16 unk_0E;
    u32 unk_10;
    s16 next;             /* 0x14 */
} MenuSprite;

typedef struct WinSprite {
    u8 unk_00[5];
    u8 flags;             /* 0x05 */
    s16 unk_06;
    u32 unk_08;
    u8 sequence[0x50 - 0x0C]; /* 0x0C */
    s16 unk_50;
    s16 unk_52;
    s16 unk_54;
    s16 unk_56;
    u32 unk_58;
    s16 unk_5C;
    s16 unk_5E;
    s16 unk_60;
    s16 unk_62;
    u8 unk_64[3];
    u8 unk_67;
} WinSprite;

typedef struct DISC_STRUCT MenuData {
    u8 unk0_5 : 3;
    u8 topmost : 1;       /* 0x00 bit 4: always opens at priority 100 */
    u8 unk0_0 : 4;
    u8 unk1_5 : 3;        /* 0x01 bits 5-7 */
    u8 unk1_2 : 3;        /* 0x01 bits 2-4: 0 = no active focus */
    u8 unk1_0 : 2;
    u8 unk_02;
    u8 cursorId;          /* 0x03 */
    s16 itemIndex;        /* 0x04 */
    s16 unk_06;
    s16 unk_08;
    u8 pad_0A[0x14 - 0x0A];
#ifdef TARGET_PC
    DISC_PTR(void) callback; /* 0x14: 32-bit big-endian slot in the table */
#else
    void (*callback)(WindowWork* window); /* 0x14 */
#endif
} MenuData;

extern WindowSystemWork winSysWork;
extern const f32 lbl_8047CDEC; /* RULE-EXCEPTION(title-path): extern stand-in for the window TU's pool literal - see docs/RULE_EXCEPTIONS.md */
extern char lbl_80271E94[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
extern void GSlogWrite(const char* fmt, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(void* message);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern const MenuData* menuDataBiosGetPtr(s32 id);
#else
#define menuDataBiosGetPtr(id) ((const MenuData*)menuDataBiosGetPtr(id)) /* canonical returns void*; this file reads MenuData */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern const MenuItem* menuItemBiosGetPtr(s32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MenuSprite* menuSpriteBiosGetPtr(s32 index);
#endif
extern void menuPlaySe(s32 id, s32 se);
extern u8 menuGetEnablePort(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern WindowCursor cursorBiosGetPos(u16 index);
#endif
extern s32 _windowCreateItemSprite__FP14tagWINDOW_WORK(WindowWork* window);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern WinSprite* winSpriteAdd(void* head);
#endif
extern void winSpriteRelease(void* head);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSetSequence(void* out, u32 index);
#endif

#ifdef TARGET_PC /* this file's own inline copy; the global function's canonical prototype differs */
#define windowGetCursorToItem windowGetCursorToItem_local
#endif
static inline const MenuItem* windowGetCursorToItem(WindowWork* window)
{
    const MenuItem* item = menuItemBiosGetPtr(menuDataBiosGetPtr(window->id)->itemIndex);
    s32 index = 0;

    while (TRUE) {
        if (item->selectable) {
            if (window->cursor.y == index) {
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

/* 0x80104CA0 | 0x1E0 */
void windowCreateCursorSprite(WindowWork* window)
{
    const MenuData* data;
    const MenuItem* item;
    MenuSprite* sprite;
    WinSprite* work;

    data = menuDataBiosGetPtr(window->id);
    item = windowGetCursorToItem(window);
#ifdef TARGET_PC
    /* PC (observe only, 2026-11-03, port-gaps 54): ORRE_CURSOR_LOG=1 logs what each cursor build finds (the
     * window, its cursor, the item and its cursor-sprite list), to see why the storage box's cursor never shows */
    extern char* getenv(const char*);
    extern void OSReport(const char*, ...);
    static int s_cursor_log = -1;
    if (s_cursor_log < 0) {
        const char* env = getenv("ORRE_CURSOR_LOG");
        s_cursor_log = env != NULL && *env != '\0' && *env != '0';
    }
    if (s_cursor_log) {
        OSReport("[cursor] window 0x%X cursor %d,%d item %p cursorSprite %d\n", (unsigned)window->id,
                 (int)window->cursor.x, (int)window->cursor.y, (const void*)item,
                 item != NULL ? (int)item->cursorSprite : -1);
    }
#endif
    if (item == NULL) {
        return;
    }
    winSpriteRelease(&window->cursorSprites);
    if (item->cursorSprite == 0) {
        return;
    }
    sprite = menuSpriteBiosGetPtr(item->cursorSprite);
#ifdef TARGET_PC
    /* where the cursor (the main menu's glove) sits for the chosen language (orre_hooks.h, OrreHook_CursorSpriteAt;
     * language piece 4, port/src/pc_word_pictures.cpp): the European and Japanese games move a few cursor places to
     * suit their words. Look only; the cursor's movement between items still reads the items. All platforms */
    s16 cursorX = item->x;
    s16 cursorY = item->y;
    if (OrreHook_CursorSpriteAt != NULL) {
        OrreHook_CursorSpriteAt(item, &cursorX, &cursorY);
    }
#endif
    while (TRUE) {
        work = winSpriteAdd(&window->cursorSprites);
#ifdef TARGET_PC
        if (s_cursor_log) {
            OSReport("[cursor]   sprite type %d seq %d off %d,%d unk_10 0x%08X last %d -> work %p\n",
                     (int)sprite->type, (int)sprite->sequence, (int)sprite->offsetX, (int)sprite->offsetY,
                     (unsigned)sprite->unk_10, (int)sprite->last, (void*)work);
        }
#endif
        if (work == NULL) {
            winSpriteRelease(&window->cursorSprites);
            return;
        }
        if (sprite->type == 1) {
            work->flags |= 1;
            work->unk_58 = sprite->unk_10;
        } else if (sprite->type == 2) {
            work->flags |= 2;
            work->unk_08 = sprite->unk_10;
        }
        work->unk_06 = data->itemIndex;
#ifdef TARGET_PC
        work->unk_50 = cursorX + sprite->offsetX;
        work->unk_52 = cursorY + sprite->offsetY;
#else
        work->unk_50 = item->x + sprite->offsetX;
        work->unk_52 = item->y + sprite->offsetY;
#endif
        work->unk_54 = sprite->unk_0C;
        work->unk_56 = sprite->unk_0E;
        work->unk_5C = sprite->unk_08;
        work->unk_5E = sprite->unk_0A;
        work->unk_60 = sprite->unk_0C;
        work->unk_62 = sprite->unk_0E;
        work->unk_67 = sprite->unk_07;
        if (sprite->sequence != 0) {
            winSetSequence(&work->sequence, sprite->sequence);
        }
        if (sprite->last) {
            break;
        }
        sprite = menuSpriteBiosGetPtr(sprite->next);
    }
}

#ifdef TARGET_PC /* this file's own inline copy; the global function's canonical prototype differs */
#define windowGetActiveID windowGetActiveID_local
#endif
static inline s32 windowGetActiveID(void)
{
    return winSysWork.activeID;
}

#ifdef TARGET_PC /* this file's own inline copy; the global function's canonical prototype differs */
#define windowSearchID windowSearchID_local
#endif
static inline WindowWork* windowSearchID(s32 id)
{
    WindowWork* window;

    if (id <= 0) {
        return NULL;
    }

    window = P32(winSysWork.windows);
    while (window != NULL) {
        if (window->id == id) {
            return window;
        }
        window = P32(window->next);
    }
    return NULL;
}

static inline WindowWork* winGetNewWork(void)
{
    WindowWork* work = P32(winSysWork.works);
    s32 i;

    for (i = 0; i < winSysWork.count; i++) {
        if (work->state == 0) {
            memset(work, 0, sizeof(WindowWork));
            work->state = 7;
            work->unk_8C = lbl_8047CDEC;
            work->unk_90 = lbl_8047CDEC;
            work->unk_88 = -1;
            return work;
        }
        work++;
    }
    GSlogWrite(lbl_80271E94);
    return NULL;
}

static inline s32 winInsertWork(WindowWork* window, s8 priority)
{
    WindowWork* work = P32(winSysWork.windows);

    if (work == NULL) {
        P32_SET(winSysWork.windows, window);
    } else {
        while (TRUE) {
            if (work->priority > priority) {
                window->prev = work->prev;
                P32_SET(window->next, work);
                if (P32(work->prev) == NULL) {
                    P32_SET(winSysWork.windows, window);
                } else {
                    P32_SET(P32(work->prev)->next, window);
                }
                P32_SET(work->prev, window);
                break;
            }
            if (P32(work->next) == NULL) {
                P32_SET(work->next, window);
                P32_SET(window->prev, work);
                P32_SET(window->next, NULL);
                break;
            }
            work = P32(work->next);
        }
    }
    return 0;
}

/* 0x80104E80 | 0x474 */
WindowWork* windowOpen(s32* cursor, s32 id, s32 parentId, u32 flags, s32 argc, va_list args)
{
    WindowWork* parent;
    const MenuData* data;
    WindowWork* window;
    s8 priority;
    const MenuItem* item;
    s32 i;

#ifdef TARGET_PC
    /* OrreHook_WindowReady below: whether this window was already ready (a reopen of a ready window keeps it ready) */
    const WindowWork* pc_was = windowSearchID(id);
    const int pc_was_ready = pc_was != NULL && pc_was->step == 2;
#endif
    parent = windowSearchID(parentId);
    data = menuDataBiosGetPtr(id);
    window = windowSearchID(id);
    if (window == NULL) {
        if (data->topmost) {
            priority = 100;
        } else if ((flags & 2) != 0) {
            priority = 100;
        } else {
            if (parentId == 0) {
                parentId = windowGetActiveID();
            }
            parent = windowSearchID(parentId);
            if (parent != NULL) {
                priority = parent->priority + 1;
            } else {
                priority = 0;
            }
            if (priority > 100) {
                priority = 100;
            }
        }
        window = winGetNewWork();
        if (window == NULL) {
            return NULL;
        }
        window->step = 0;
        window->id = id;
        window->priority = priority;
        window->unk_1A = data->unk1_5;
        P32_SET(window->parent, windowSearchID(parentId));
        window->unk_84 = data->unk_06;
        window->unk_86 = data->unk_08;
        item = menuItemBiosGetPtr(data->itemIndex);
        while (TRUE) {
            if (item->selectable) {
                window->itemCount++;
            }
            if (item->last) {
                break;
            }
            item = menuItemBiosGetPtr(item->next);
        }
        _windowCreateItemSprite__FP14tagWINDOW_WORK(window);
        winInsertWork(window, priority);
        menuPlaySe(id, 4);
    } else {
        window->unk_98 = 0;
        window->unk_99 = 0;
        window->step = 0;
        window->reopen = 1;
    }

    if (cursor == NULL) {
        if (data->cursorId != 0) {
#ifdef TARGET_PC /* MWCC returns this 2-byte struct in the top half of r3: cursorBiosGetPos returns (u16)pos << 16 and
                  * the caller stores the top half (assembly); the project notes 13 */
            {
                u16 raw = (u16)(cursorBiosGetPos(data->cursorId) >> 16); /* table u16 in native order: its bytes in memory order are x, y */
                memcpy(&window->cursor, &raw, sizeof(window->cursor));
            }
#else
            window->cursor = cursorBiosGetPos(data->cursorId);
#endif
        }
    } else {
        window->cursor.x = 0;
        window->cursor.y = *cursor;
    }

    windowCreateCursorSprite(window);
    window->port = menuGetEnablePort();
    window->openFlags = flags;
    if (argc > 8) {
        argc = 8;
    }
    for (i = 0; i < argc; i++) {
        window->args[i] = va_arg(args, u32);
    }

    if (data->unk1_2 == 0) {
        window->unk_98 = 1;
    } else if ((flags & 4) == 0) {
        winSysWork.activeID = id;
    }
    if (winSysWork.activeID != id) {
        window->unk_18 = 1;
    }
    if (DP(data->callback) != NULL) {
        ((void (*)(WindowWork*))DP(data->callback))(window);
    }
    if (window->reopen != 0) {
        window->closeFlag = 0;
        window->step = 2;
        window->reopen = 0;
    }
#ifdef TARGET_PC
    if (window->step == 2 && !pc_was_ready) {
        /* OrreHook_WindowReady (observation hook; docs/port/hook-points.md): a window that opens ready (the reopen
         * path or its open callback) skips menuDaemon's phase 1 -> 2, where the hook is otherwise reported; a reopen of
         * a window that was already ready is not a new opening (the console's window stays ready throughout) */
        extern void (*OrreHook_WindowReady)(u32 windowId);
        if (OrreHook_WindowReady != NULL) {
            OrreHook_WindowReady((u32)id);
        }
    }
#endif
    return window;
}

/* 0x801052F4 | 0x11C */
void _winCalcWindowSize__FlPC13MENU_ITEM_dd_PsPs(const MenuItem* item, s16* width, s16* height)
{
    s32 minX = 640;
    s32 minY = 480;
    s32 maxX = 0;
    s32 maxY = 0;
    u32 rect;
    s16 textWidth;
    s16 textHeight;

    *width = 0;
    *height = 0;
    while (TRUE) {
        menuSpriteBiosGetPtr(item->spriteIndex);
        if (item->message != NULL) {
            rect = GSmsgGetRect(item->message);
        } else {
            rect = 0;
        }
        textWidth = rect >> 16;
        textHeight = (u16)rect;
        if (minX > item->x) {
            minX = item->x;
        }
        if (maxX < item->x + item->width) {
            maxX = item->x + item->width;
        }
        if (maxX < item->x + textWidth) {
            maxX = item->x + textWidth;
        }
        if (minY > item->y) {
            minY = item->y;
        }
        if (maxY < item->y + item->height) {
            maxY = item->y + item->height;
        }
        if (maxY < item->y + textHeight) {
            maxY = item->y + textHeight;
        }
        if (item->last) {
            break;
        }
        item = menuItemBiosGetPtr(item->next);
    }
    *width = maxX - minX;
    *height = maxY - minY;
}
