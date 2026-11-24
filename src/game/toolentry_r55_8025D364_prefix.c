/**
 * @file toolentry_r55_8025D364_prefix.c
 * @brief toolentryGetTrainerBicFaceResID (0x8025D364 - 0x8025D3F4): the
 *        big-face resource ID of the entry's trainer.
 *
 * Function-boundary carve of toolentry.c, text only, GC/1.3 -O4,p. Retail
 * calls savedataBattleGetEntryPtr on both arms of the context test (the NULL arm passes
 * the NULL context through), so the body keeps both calls. The same body is
 * in toolentry.c.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the tool menu table is room-module data, big-endian on the PC */

u32 toolentryGetTrainerBicFaceResID(void* ctx, u32 param1) {
    extern u32 lbl_80478E04;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataBattleGetEntryPtr(void*);
#endif
    extern u32 fn_801FCBA4(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* fightTrainerDataBiosGetPtr(u32);
#endif
    u16 id16;
    u32 id;
    u32 base;
    u32 offset;
    u32* entry;
    u32 ret;

    if ((s32)PC_PTR32(ctx) != 0) {
        id16 = *(u16*)savedataBattleGetEntryPtr(PC_PTR32_IMPLICIT(ctx));
    } else {
        id16 = *(u16*)savedataBattleGetEntryPtr(PC_PTR32_IMPLICIT(ctx));
    }
    id = id16;
    if (id == 0) {
        return 0;
    }
    /* r3 from the previous call (assembly) */
    offset = fn_801FCBA4(fightTrainerDataBiosGetPtr(id));
    offset *= 0x14;
    base = lbl_80478E04;
    entry = (u32*)PC_PTR((base + offset));
    /* big-endian room-module data on the PC (2026-10-25, the room-module audit) */
    if ((s32)param1 == 0) {
        return DISC_U32(&entry[1]);
    }
    ret = DISC_U32(&entry[2]);
    if (ret == 0) {
        return 0xf991200;
    }
    return ret;
}
