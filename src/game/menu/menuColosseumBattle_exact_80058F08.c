/**
 * @file menuColosseumBattle_exact_80058F08.c
 * @brief Byte-exact menuColosseumBattle.cpp range, 0x80058F08 - 0x80059034.
 */
#include "dolphin/types.h"

typedef struct ColosseumRosterRow {
    s32 mode;
    s32 menuId;
    s32 partyIndex;
    s32 action;
} ColosseumRosterRow;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(void*);
#endif
extern const ColosseumRosterRow lbl_802677D0[7];

/* Address: 0x80058F08 | Size: 0x38 */
s32 fn_80058F08(s32* partyIndex, s32 cursor) {
    const ColosseumRosterRow* row;

    if (cursor < 0 || cursor >= 7) {
        return 2;
    }

    row = &lbl_802677D0[cursor];
    *partyIndex = row->partyIndex;
    return row->menuId;
}

/* Address: 0x80058F40 | Size: 0xF4 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
void* fn_80058F40(void* dflt, s32 dir, s32* state) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroGetStatus(void*, s32, u16);
#endif
    void* hero;
    s32 i;

    if (state[0] != 0) {
        return dflt;
    }
    if (dir == 1) {
        for (i = state[1] - 1; i >= 0; i--) {
            u32 v;
            hero = PC_PTR(heroGetStatus(NULL, 3, (u16)i));
            v = pokemonCheckValid(hero);
            if (v != 0) {
                break;
            }
        }
        if (i >= 0) {
            state[1] = i;
            return hero;
        }
    }
    if (dir == 2) {
        for (i = state[1] + 1; i < 6; i++) {
            u32 v;
            hero = PC_PTR(heroGetStatus(NULL, 3, (u16)i));
            v = pokemonCheckValid(hero);
            if (v != 0) {
                break;
            }
        }
        if (i < 6) {
            state[1] = i;
            return hero;
        }
    }
    return dflt;
}
#pragma pop
