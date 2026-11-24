/** Exact menuShop varargs callbacks, 0x80029FAC - 0x8002A3D4. */
#include "dolphin/types.h"

extern void* __va_arg(void*, s32);
extern u32 lbl_80478E54;
extern u32 lbl_80478E4C;
extern u32 lbl_80478E3C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenWithSE(s32, u32, s32, s32, u8);
extern void msgctrlSetValue(u32 id, u32 value);
extern void winMsgClose(u32 wait);
#endif

typedef struct ShopVaList {
    u8 gpr;
    u8 fpr;
    u16 padding;
    u32* overflow_arg_area;
    u32* reg_save_area;
} ShopVaList;
typedef ShopVaList ShopVaListArray[1];

#ifdef TARGET_PC
/* PC: the same four functions with the host's variable arguments. The GameCube versions below build a PowerPC va_list by
 * hand, which reads garbage under the PC ABI; and the shop message tables (lbl_80478E4C: 0x4C-byte records, lbl_80478E3C:
 * 0x3C-byte records; byte 0 = the sound, then u32 message ids) are big-endian disc data, so the ids are read with
 * DISC_U32 (natively 0x00003BE0 came out as 0xE03B0000: "no data", then a crash on the diner shop's Buy, snatcherLeo's
 * playtest 2026-10-17). The loop is the original's: (key, value) pairs for msgctrlSetValue until a negative word. */
#include <stdarg.h>
#include "pc/disc_types.h"

static u8* shop_msg_record(u32 table, s32 shop, u32 stride)
{
    u8* map = (u8*)PC_PTR(lbl_80478E54);
    return (u8*)PC_PTR(table) + map[shop << 2] * stride;
}

static void shop_msg_values(s32 word, va_list ap)
{
    s32 key = 0;
    s32 expectKey = 1;

    while (word >= 0) {
        if (expectKey) {
            key = word;
            expectKey = 0;
        } else {
            expectKey = 1;
            msgctrlSetValue(key, (u32)word);
        }
        word = va_arg(ap, s32);
    }
}

u32 fn_80029FAC(u8* r3, s32 r4, s32 r5, s32 r6, ...)
{
    u8* record = shop_msg_record(lbl_80478E4C, r4, 0x4c);
    va_list ap;

    *r3 = record[0];
    va_start(ap, r6);
    shop_msg_values(r6, ap);
    va_end(ap);
    return DISC_U32(record + 4 + (r5 << 2));
}

u32 fn_8002A0B8(u8* r3, s32 r4, s32 r5, s32 r6, ...)
{
    u8* record = shop_msg_record(lbl_80478E3C, r4, 0x3c);
    va_list ap;

    *r3 = record[0];
    va_start(ap, r6);
    shop_msg_values(r6, ap);
    va_end(ap);
    return DISC_U32(record + 4 + (r5 << 2));
}

void fn_8002A1C4(u8* r3, s32 r4, s32 r5, ...)
{
    u8* record = shop_msg_record(lbl_80478E4C, (s32)PC_PTR32(r3), 0x4c);
    va_list ap;

    va_start(ap, r5);
    shop_msg_values(r5, ap);
    va_end(ap);
    winMsgOpenWithSE(2, DISC_U32(record + 4 + (r4 << 2)), 1, 0, record[0]);
    winMsgClose(1);
}

void fn_8002A2CC(u8* r3, s32 r4, s32 r5, ...)
{
    u8* record = shop_msg_record(lbl_80478E3C, (s32)PC_PTR32(r3), 0x3c);
    va_list ap;

    va_start(ap, r5);
    shop_msg_values(r5, ap);
    va_end(ap);
    winMsgOpenWithSE(2, DISC_U32(record + 4 + (r4 << 2)), 1, 0, record[0]);
    winMsgClose(1);
}
#else
/* RULE-EXCEPTION(user-approved): code-generation-shaped varargs/table setup;
 * see docs/RULE_EXCEPTIONS.md. */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
u32 fn_80029FAC(u8* r3, s32 r4, s32 r5, s32 r6, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    s32 r29;
    u8* r28;
    u8* map;
    u8* table;
    s32 idx;
    s32 offset;
    u32* new_var;

    *(u32*)list = 0x04000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = r4 << 2;
    map = (u8*)((((((((((((lbl_80478E54 & 0xFFFFFFFFFFFFFFFFu) &
                           0xFFFFFFFFFFFFFFFFu) &
                          0xFFFFFFFFFFFFFFFFu) &
                         0xFFFFFFFFFFFFFFFFu) &
                        0xFFFFFFFFFFFFFFFFu) &
                       0xFFFFFFFFFFFFFFFFu) &
                      0xFFFFFFFFFFFFFFFFu) &
                     0xFFFFFFFFFFFFFFFFu) &
                    0xFFFFFFFFFFFFFFFFu) &
                   0xFFFFFFFFFFFFFFFFu) &
                  0xFFFFFFFFFFFFFFFFu) &
                 0xFFFFFFFFFFFFFFFFu);
    r31 = r5 << 2;
    table = (u8*)PC_PTR(lbl_80478E4C);
    r30 = 1;
    offset = map[idx] * 0x4c;
    *r3 = table[offset];
    r28 = (u8*)PC_PTR(*(volatile u32*)(new_var = &lbl_80478E4C)) + offset + 4;
    while (r6 >= 0) {
        if (r30 != 0) {
            r29 = r6;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, PC_PTR32_IMPLICIT((void*)r6));
        }
        r6 = *(s32*)__va_arg(list, 1);
    }
    return *(u32*)(r28 + r31);
}
#else
u32 fn_80029FAC(u8* r3, s32 r4, s32 r5, s32 r6, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    s32 r29;
    u8* r28;
    u8* map;
    u8* table;
    s32 idx;
    s32 offset;
    u32* new_var;

    *(u32*)list = 0x04000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = r4 << 2;
    map = (u8*)((((((((((((lbl_80478E54 & 0xFFFFFFFFFFFFFFFFu) &
                           0xFFFFFFFFFFFFFFFFu) &
                          0xFFFFFFFFFFFFFFFFu) &
                         0xFFFFFFFFFFFFFFFFu) &
                        0xFFFFFFFFFFFFFFFFu) &
                       0xFFFFFFFFFFFFFFFFu) &
                      0xFFFFFFFFFFFFFFFFu) &
                     0xFFFFFFFFFFFFFFFFu) &
                    0xFFFFFFFFFFFFFFFFu) &
                   0xFFFFFFFFFFFFFFFFu) &
                  0xFFFFFFFFFFFFFFFFu) &
                 0xFFFFFFFFFFFFFFFFu);
    r31 = r5 << 2;
    table = (u8*)PC_PTR(lbl_80478E4C);
    r30 = 1;
    offset = map[idx] * 0x4c;
    *r3 = table[offset];
    r28 = (u8*)PC_PTR(*(volatile u32*)(new_var = &lbl_80478E4C)) + offset + 4;
    while (r6 >= 0) {
        if (r30 != 0) {
            r29 = r6;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, r6);
        }
        r6 = *(s32*)__va_arg(list, 1);
    }
    return *(u32*)(r28 + r31);
}
#endif

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
u32 fn_8002A0B8(u8* r3, s32 r4, s32 r5, s32 r6, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    u8 new_var;
    s32 r29;
    u8* r28;
    u8* map;
    u8* table;
    s32 idx;
    s32 offset;

    *(u32*)list = 0x04000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = r4 << 2;
    map = (u8*)(((((((lbl_80478E54 & 0xFFFFFFFFFFFFFFFFu) &
                     0xFFFFFFFFFFFFFFFFu) &
                    0xFFFFFFFFFFFFFFFFu) &
                   0xFFFFFFFFFFFFFFFFu) &
                  0xFFFFFFFFFFFFFFFFu) &
                 0xFFFFFFFFFFFFFFFFu) &
                0xFFFFFFFFFFFFFFFFu);
    r31 = r5 << 2;
    new_var = map[idx];
    table = (u8*)PC_PTR(lbl_80478E3C);
    r30 = 1;
    offset = new_var * 0x3c;
    *r3 = table[offset];
    r28 = (u8*)PC_PTR(*(volatile u32*)&lbl_80478E3C) + offset + 4;
    while (r6 >= 0) {
        if (0 != r30) {
            r29 = r6;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, PC_PTR32_IMPLICIT((void*)r6));
        }
        r6 = *(s32*)__va_arg(list, 1);
    }
    return *(u32*)(r28 + r31);
}
#else
u32 fn_8002A0B8(u8* r3, s32 r4, s32 r5, s32 r6, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    u8 new_var;
    s32 r29;
    u8* r28;
    u8* map;
    u8* table;
    s32 idx;
    s32 offset;

    *(u32*)list = 0x04000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = r4 << 2;
    map = (u8*)(((((((lbl_80478E54 & 0xFFFFFFFFFFFFFFFFu) &
                     0xFFFFFFFFFFFFFFFFu) &
                    0xFFFFFFFFFFFFFFFFu) &
                   0xFFFFFFFFFFFFFFFFu) &
                  0xFFFFFFFFFFFFFFFFu) &
                 0xFFFFFFFFFFFFFFFFu) &
                0xFFFFFFFFFFFFFFFFu);
    r31 = r5 << 2;
    new_var = map[idx];
    table = (u8*)PC_PTR(lbl_80478E3C);
    r30 = 1;
    offset = new_var * 0x3c;
    *r3 = table[offset];
    r28 = (u8*)PC_PTR(*(volatile u32*)&lbl_80478E3C) + offset + 4;
    while (r6 >= 0) {
        if (0 != r30) {
            r29 = r6;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, r6);
        }
        r6 = *(s32*)__va_arg(list, 1);
    }
    return *(u32*)(r28 + r31);
}
#endif

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_8002A1C4(u8* r3, s32 r4, s32 r5, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    s32 r29;
    u8* r28;
    u8* map;
    s32 idx;
    u8 r27;

    *(u32*)list = 0x03000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = (s32)PC_PTR32(r3) << 2;
    map = (u8*)(lbl_80478E54 & 0xFFFFFFFFFFFFFFFFu);
    r31 = r4 << 2;
    r3 = (u8*)PC_PTR(lbl_80478E4C) + map[idx] * 0x4c;
    r27 = r3[0];
    r28 = r3 + 4;
    r30 = 1;
    while (r5 >= 0) {
        if (r30 != 0) {
            r29 = r5;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, PC_PTR32_IMPLICIT((void*)r5));
        }
        r5 = *(s32*)__va_arg(list, 1);
    }
    winMsgOpenWithSE(2, *(u32*)(r28 + r31), 1, 0, r27);
    winMsgClose(1);
}
#else
void fn_8002A1C4(u8* r3, s32 r4, s32 r5, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    s32 r29;
    u8* r28;
    u8* map;
    s32 idx;
    u8 r27;

    *(u32*)list = 0x03000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = (s32)PC_PTR32(r3) << 2;
    map = (u8*)(lbl_80478E54 & 0xFFFFFFFFFFFFFFFFu);
    r31 = r4 << 2;
    r3 = (u8*)PC_PTR(lbl_80478E4C) + map[idx] * 0x4c;
    r27 = r3[0];
    r28 = r3 + 4;
    r30 = 1;
    while (r5 >= 0) {
        if (r30 != 0) {
            r29 = r5;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, r5);
        }
        r5 = *(s32*)__va_arg(list, 1);
    }
    winMsgOpenWithSE(2, *(u32*)(r28 + r31), 1, 0, r27);
    winMsgClose(1);
}
#endif

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_8002A2CC(u8* r3, s32 r4, s32 r5, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    s32 r29;
    u8* r28;
    u8* map;
    s32 idx;
    u8 r27;

    *(u32*)list = 0x03000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = (s32)PC_PTR32(r3) << 2;
    map = (u8*)PC_PTR(lbl_80478E54);
    r31 = r4 << 2;
    r3 = (u8*)PC_PTR(lbl_80478E3C) + map[idx] * (0x3c & 0xFFFFFFFFFFFFFFFFu);
    r27 = r3[0];
    r28 = r3 + 4;
    r5 = r5;
    r30 = 1;
    while (r5 >= 0) {
        if (r30 != 0) {
            r29 = r5;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, PC_PTR32_IMPLICIT((void*)r5));
        }
        r5 = *(s32*)__va_arg(list, 1);
    }
    winMsgOpenWithSE(2, *(u32*)(r28 + r31), 1, 0, r27);
    winMsgClose(1);
}
#else
void fn_8002A2CC(u8* r3, s32 r4, s32 r5, ...)
{
    ShopVaListArray list;
    s32 r31;
    s32 r30;
    s32 r29;
    u8* r28;
    u8* map;
    s32 idx;
    u8 r27;

    *(u32*)list = 0x03000000;
    list[0].overflow_arg_area = (u32*)((u8*)list + 0x30);
    list[0].reg_save_area = (u32*)((u8*)list - 0x60);
    idx = (s32)PC_PTR32(r3) << 2;
    map = (u8*)PC_PTR(lbl_80478E54);
    r31 = r4 << 2;
    r3 = (u8*)PC_PTR(lbl_80478E3C) + map[idx] * (0x3c & 0xFFFFFFFFFFFFFFFFu);
    r27 = r3[0];
    r28 = r3 + 4;
    r5 = r5;
    r30 = 1;
    while (r5 >= 0) {
        if (r30 != 0) {
            r29 = r5;
            r30 = 0;
        } else {
            r30 = 1;
            msgctrlSetValue(r29, r5);
        }
        r5 = *(s32*)__va_arg(list, 1);
    }
    winMsgOpenWithSE(2, *(u32*)(r28 + r31), 1, 0, r27);
    winMsgClose(1);
}
#endif
#endif
