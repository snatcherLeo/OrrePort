/**
 * @file floor_event_exact_80115CB4.c
 * @brief floorEventGetTresureList, floorEventSetTresureDisp and
 *        floorEventChangeTresure, 0x80115CB4 - 0x80115E6C.
 *
 * Function-boundary carve of the floor-event TU (see floor_event.c): find
 * the n-th treasure placed on the current floor, show or hide a treasure by
 * its raw table index, and rewrite a treasure's contents. No jump table, no
 * pooled constant, no data of its own. GC/1.3 -O4,p with the TU's unit-wide
 * -opt nopeephole, no pragmas.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the treasure records are common_rel data, big-endian on the PC (floor.c's view) */

extern void GSlogWrite(const char* fmt, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* std092_getCurrentRoomID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* std093_getCurrentGroupID(void);
#endif
extern void peopleSetVisible(void*, u32, u32);

extern u32 floor_tresure_list; /* treasure table */
extern u32 floor_tresure_list_number; /* treasure count */

/* RULE-EXCEPTION(user-approved): extern named stand-ins for the TU's own
 * strings -- see docs/RULE_EXCEPTIONS.md. The format (.rodata, also read by
 * floor_character) and the function name (.data) stay with the unlinked
 * rest of the TU. */
extern u8 lbl_80272708[];
extern u8 lbl_8035BB70[];

/* 0x80115CB4 | 0xB0 */
void* floorEventGetTresureList(u32 param)
{
    u32 type;
    u8* entry;
    u32 index;
    u32 found;
    u32 target;

    entry = 0;
    found = 0;
    type = param & 0x7FFF0000;
    if (type != 0x7FFF0000) {
        return 0;
    }

    target = param & 0x1FF;
    for (index = 0; index < *(u32*)PC_PTR(floor_tresure_list_number); index++) {
        entry = (u8*)PC_PTR(floor_tresure_list) + index * 0x1C;
        /* big-endian on the PC (2026-10-25, P08: read natively, no floor id ever matched) */
        if (DISC_U16(entry + 4) == PC_PTR32(std092_getCurrentRoomID())) {
            if (target == found++) {
                break;
            }
        }
    }

    if (index == *(u32*)PC_PTR(floor_tresure_list_number)) {
        return 0;
    }
    return entry;
}

/* 0x80115D64 | 0xA0 */
void floorEventSetTresureDisp(u32 rawIndex, u32 display)
{
    u8* entry;
    u32 index;
    u32 found = 0;
    u32 encoded;

    for (index = 0; index < *(u32*)PC_PTR(floor_tresure_list_number); index++) {
        entry = (u8*)PC_PTR(floor_tresure_list) + index * 0x1C;
        if (DISC_U16(entry + 4) == PC_PTR32(std092_getCurrentRoomID())) {
            if (index == rawIndex) {
                encoded = found | 0x7FFF0000;
                break;
            }
            found++;
        }
    }

    if (index != *(u32*)PC_PTR(floor_tresure_list_number)) {
        peopleSetVisible(PC_PTR(std093_getCurrentGroupID()), encoded, display);
    }
}

/* 0x80115E04 | 0x68 */
s32 floorEventChangeTresure(u32 index, u16 val, u8 byte)
{
    u32 count;
    u8* entry;

    count = *(u32*)PC_PTR(floor_tresure_list_number);
    if (index >= count) {
        GSlogWrite((const char*)lbl_80272708, lbl_8035BB70);
        return -1;
    }
    entry = (u8*)PC_PTR(floor_tresure_list) + index * 0x1c;
    DISC_STORE32(u32, entry + 0xc, val); /* the item word, big-endian like the rest of the record */
    *(u8*)(entry + 0x1) = byte;
    return 0;
}
