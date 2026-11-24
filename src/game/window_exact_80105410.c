/**
 * @file window_exact_80105410.c
 * @brief windowInit (0x80105410 - 0x801054B8).
 *
 * Clears the window system work (winSysWork), allocates the window
 * table (count * 0xB4 bytes) and initialises the sprite pool.
 *
 * Window TU flags (GC/2.5, -O4,p, "-opt nopeephole"; see
 * window_exact_80104318.c): exact without the local optimization_level /
 * peephole pragmas it used to need. Carved so it can link while its
 * neighbours stay candidates; data stays extern.
 */
#include "dolphin/types.h"

typedef struct WindowSystemWork {
    u16 count;       /* 0x00 */
    u16 tableHandle; /* 0x02 */
    u32 activeID;    /* 0x04 */
    PTR32_FIELD(void) table;   /* 0x08: 32-bit slot on the PC too, as in the other views (pc/ptr32.h) */
    PTR32_FIELD(void) windows; /* 0x0C: open-window list (32-bit slot) */
    u8 pad_10[0x9C - 0x10];
} WindowSystemWork;

extern WindowSystemWork winSysWork;
extern char lbl_80271EC4[]; /* "windowInit" + "initialisation failed\n" (SJIS) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int value, u32 size);
#endif
extern void GSlogWrite(const char* fmt, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size); /* GSmemAllocRaw */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);      /* GSmemGetPtr */
#endif
extern void winSpriteInit(void);

/* 0x80105410 | 0xA8 */
void windowInit(u16 count)
{
    u32 size;

    memset(&winSysWork, 0, sizeof(winSysWork));
    size = count * 0xB4;
    winSysWork.tableHandle = _toolentryAlloc__FUl(size);
    if (winSysWork.tableHandle == 0) {
        GSlogWrite(lbl_80271EC4);
    } else {
        P32_SET(winSysWork.table, GSmemLock(winSysWork.tableHandle));
        winSysWork.count = count;
        memset(P32(winSysWork.table), 0, size);
        winSpriteInit();
    }
}
