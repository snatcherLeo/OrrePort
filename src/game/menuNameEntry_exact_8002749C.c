/* RULE-EXCEPTION(user-approved): constant_import (temporary) — remove when this file is merged back into one unit — see docs/RULE_EXCEPTIONS.md */
/* Name-entry keyboard cursor, 0x8002749C-0x800275F4. */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC */
#include "pc/orre_hooks.h" /* OrreHook_NameEntryCursorBox: OrrePort's naming keyboard (PC only; empty on the GameCube) */

extern u8 lbl_802EF0A8[];
/* RULE-EXCEPTION(user-approved): extern named stand-ins for the menuNameEntry TU's shared .sdata2 float pool (also read by 80026740, 80026FEC, 800280FC, 80028728, 80028830, 80028FBC) — see docs/RULE_EXCEPTIONS.md */
extern const f32 lbl_8047B93C, lbl_8047B934, lbl_8047B940, lbl_8047B938;

s32 menuNameEntryDraw50Cursor(void* window, u8* draw)
{
    u8* ctx;
    f32 width;
    f32 height;
    f32 scale;
    f32 dw;
    f32 dh;
    s32 column;
    s32 row;
    s16 left;
    s16 top;
    s16 right;
    s16 bottom;
    u8 alpha;
    s32 y;
    s32 x;

    ctx = ((u8*)PTR32_AT(window, 0x60));
    column = *PTR32_AT_T(s32, ctx, 0x28); /* the block's pointer fields are 32-bit slots on the PC (name_entry_arg.h) */
    width = DISC_EL_S16(((s16*)(lbl_802EF0A8 + *(s16*)(draw + 6) * 0x1c)), 3);
    height = DISC_EL_S16(((s16*)(lbl_802EF0A8 + *(s16*)(draw + 6) * 0x1c)), 4);
    row = *PTR32_AT_T(s32, ctx, 0x2c);
    scale = *PTR32_AT_T(f32, ctx, 0x30);
#ifdef TARGET_PC
    /* OrrePort's naming keyboard places the box on its own grid (orre_hooks.h; 2026-11-14): 1 = its place and
     * size, drawn with the game's own pulse; 2 = hidden. NULL or 0: the game's placement below. */
    if (OrreHook_NameEntryCursorBox != NULL) {
        s32 hx = *PTR32_AT_T(s32, ctx, 0x38) + column * 0x1b;
        s32 hy = *PTR32_AT_T(s32, ctx, 0x3c) + row * 0x23;
        s32 how = OrreHook_NameEntryCursorBox(window, &hx, &hy, &width, &height);
        if (how != 0) {
            dw = lbl_8047B93C * (width * scale);
            dh = lbl_8047B93C * (height * scale);
            if (how == 1) {
                alpha = lbl_8047B938 * (lbl_8047B934 - scale);
                *(s16*)(draw + 0x50) = (s16)((f32)hx - dw * lbl_8047B940);
                *(s16*)(draw + 0x52) = (s16)((f32)hy - dh * lbl_8047B940);
                *(s16*)(draw + 0x54) = (s16)(width + dw);
                *(s16*)(draw + 0x56) = (s16)(height + dh);
            } else {
                alpha = 0;
            }
            draw[0x67] = alpha;
            return 0;
        }
    }
#endif
    dw = lbl_8047B93C * (width * scale);
    dh = lbl_8047B93C * (height * scale);
    if (column < 0xf) {
        alpha = lbl_8047B938 * (lbl_8047B934 - scale);
        x = *PTR32_AT_T(s32, ctx, 0x38) + column * 0x1b;
        y = *PTR32_AT_T(s32, ctx, 0x3c) + row * 0x23;
        left = (f32)x - dw * lbl_8047B940;
        *(s16*)(draw + 0x50) = left;
        top = (f32)y - dh * lbl_8047B940;
        right = width + dw;
        bottom = height + dh;
        *(s16*)(draw + 0x52) = top;
        *(s16*)(draw + 0x54) = right;
        *(s16*)(draw + 0x56) = bottom;
    } else {
        alpha = 0;
    }
    draw[0x67] = alpha;
    return 0;
}
