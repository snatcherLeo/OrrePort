/**
 * @file menuFight_exact_800119A8.c
 * @brief menuFightCloseTop / fn_80011A1C, 0x800119A8 - 0x80011B4C.
 *
 * Function-boundary carve of the menuFight TU (see menuFight.c): close the
 * top fight menu (window 0x4B, or 0xF6 on the alternate layout), and open
 * it, asking for confirmation through window 0xFB when a cancel comes back
 * and the menu is not locked. No jump table, no pooled constant, no data.
 * GC/1.3 -O4,p with the TU's unit-wide -opt nopeephole, no pragmas.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 menuIsCheck(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom(s32 id, s32 arg1, s32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpenCustom(s32 id, ...);
#endif

u32 menuFightCloseTop(void* obj) {
    if ((u8)menuIsCheck(0x4b) != 0) menuCloseCustom(0x4b, 0, (s32)PC_PTR32(obj));
    if ((u8)menuIsCheck(0xf6) != 0) menuCloseCustom(0xf6, 0, (s32)PC_PTR32(obj));
    return 0;
}

s32 fn_80011A1C(u8* obj, s32 a1, s32 a2) {
    u32 sp8;
    s32 ret;
    u8 locked;
    s32 ret2;

    locked = obj[0x16];
#ifdef TARGET_PC
    {
        /* observe only (the project's T16, the project's ORRE_FRAME_TIMES): the battle command menu opens. This carve is the copy the PC
         * links (the address owner); the same log point in menuFight.c is dropped by the dedupe step. */
        extern int pc_vi_frame_times_active(void) __attribute__((weak));
        extern void OSReport(const char*, ...);
        if (pc_vi_frame_times_active != 0 && pc_vi_frame_times_active()) {
            OSReport("[frametime] battle command menu opens (menu 0x%X)\n", obj[0x17] != 0 ? 0xF6u : 0x4Bu);
        }
    }
#endif
    do {
        if (obj[0x17] != 0) {
            ret = menuOpenCustom(0xF6, 0, PC_PTR(a1), 0, a2, 1, obj);
        } else {
            ret = menuOpenCustom(0x4B, 0, PC_PTR(a1), 0, a2, 1, obj);
        }
        if (locked != 0) break;
        if (ret != 3) break;
        sp8 = 0;
        if ((u8)menuIsCheck(0x4B) != 0) {
            menuCloseCustom(0x4B, 0, 1);
        }
        if ((u8)menuIsCheck(0xF6) != 0) {
            menuCloseCustom(0xF6, 0, 1);
        }
        ret2 = menuOpenCustom(0xFB, 0, &sp8, 0, 1, 0);
        menuCloseCustom(0xFB, 0, 1);
    } while (ret2 != 0x1207);

    return ret;
}
