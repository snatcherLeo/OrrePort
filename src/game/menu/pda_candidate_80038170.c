/** PDA model-turntable sprite: shown once all four spinners stop, 0x80038170 - 0x80038250. */
#include "dolphin/types.h"

typedef struct PdaSprite {
    u8 pad00[0x4];
    s8 flags;
    u8 pad05;
    s16 eventId;
    u8 pad08[0x44];
    s32 messageId;
    s16 field_50;
    s16 field_52;
    s16 x;
    s16 y;
    u8 pad58[0xc];
    u8 colorR;
    u8 colorG;
    u8 colorB;
    u8 alpha;
    u8 pad68[0x8];
    f32 value;
    u8 pad74[0x17];
    u8 alphaByte;
    u8 pad8c[9];
    s8 selectedIndex;
} PdaSprite;

extern f32 lbl_8047BA58;

static inline u8 pdaAllStopped(void)
{
    extern u8 lbl_803A654C[];
    f32* p = (f32*)lbl_803A654C;
    s32 i;

    for (i = 0; i < 4; i++, p += 6) {
        if (lbl_8047BA58 != p[4]) {
            return 0;
        }
    }
    return 1;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
/* RULE-EXCEPTION(user-approved): local opt_loop_invariants pragma — see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma opt_loop_invariants off
void fn_80038170(PdaSprite* context, PdaSprite* sprite)
{
    extern f32 lbl_802E5288[][2];
    extern s8 lbl_8047A47C;
    s8 index;

    if (pdaAllStopped()) {
        sprite->flags |= 2;
    } else {
        sprite->flags &= ~2;
    }
    index = context->selectedIndex;
    lbl_8047A47C = index;
#ifdef TARGET_PC
    if ((u8)index >= 4) {
        /* OrreHook_PcMenuRowPlace (docs/port/hook-points.md; 2026-11-15): a row OrrePort added (Main Menu, row 4):
         * the place table has exactly four entries, so the lit piece's place comes from the hook. Never reached with
         * the game's own four rows. Kept in step with pda_range_80037158.c. All platforms */
        extern void (*OrreHook_PcMenuRowPlace)(s32 row, f32* x, f32* y);
        f32 x = 0.0f, y = 0.0f;
        if (OrreHook_PcMenuRowPlace != NULL) {
            OrreHook_PcMenuRowPlace(index, &x, &y);
        }
        sprite->field_50 = (s16)x;
        *(s16*)((u8*)sprite + 0x52) = (s16)y;
        return;
    }
#endif
    sprite->field_50 = (s16)lbl_802E5288[index][0];
    *(s16*)((u8*)sprite + 0x52) = (s16)lbl_802E5288[index][1];
}
#pragma pop
#pragma peephole reset
