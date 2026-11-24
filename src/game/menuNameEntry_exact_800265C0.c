/**
 * @file menuNameEntry_exact_800265C0.c
 * @brief Name-entry state visibility callbacks.
 */
#include "dolphin/types.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* ORRE_NAME_PIECE: OrrePort's 12-slot name field (PC only) */
#else
#define ORRE_NAME_PIECE(piece, type, window, draw)
#endif

extern u8 lbl_80266DD8[];

s32 fn_800265C0(void* owner, u8* draw)
{
    void* context;
    u8* base;
    s32* entry;
    ORRE_NAME_PIECE(1, 7, owner, draw);

    context = PTR32_AT_T(void, (u8*)owner, 0x60);
    base = lbl_80266DD8;
    base += *(s32*)((u8*)context + 0x1C) << 4;
    entry = (s32*)base;
    if (entry[1] != 7) {
        draw[0x67] = 0;
    } else {
        draw[0x67] = 0xFF;
    }
    return 0;
}

s32 fn_80026600(void* owner, u8* draw)
{
    void* context;
    u8* base;
    s32* entry;
    ORRE_NAME_PIECE(1, 8, owner, draw);

    context = PTR32_AT_T(void, (u8*)owner, 0x60);
    base = lbl_80266DD8;
    base += *(s32*)((u8*)context + 0x1C) << 4;
    entry = (s32*)base;
    if (entry[1] != 8) {
        draw[0x67] = 0;
    } else {
        draw[0x67] = 0xFF;
    }
    return 0;
}
