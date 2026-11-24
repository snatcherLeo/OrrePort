/**
 * @file menuColosseumBattle_exact_80057DE8.c
 * @brief Byte-exact menuColosseumBattle.cpp range, 0x80057DE8 - 0x80057E70.
 */
#include "dolphin/types.h"

typedef struct ColosseumBattleConnectState {
    s32 active;
    s32 connected;
    s32 busyRequest;
    s32 reserved;
} ColosseumBattleConnectState;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonIsDarkPokemon(void*);
#endif

extern u8 lbl_803A9A08[];

/* Address: 0x80057DE8 | Size: 0x58 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 fn_80057DE8(void* pokemon) {
    if (pokemonCheckValid(pokemon) == 0) {
        return 0;
    }
    return pokemonIsDarkPokemon(pokemon) != 0;
}
#pragma pop

/* Address: 0x80057E40 | Size: 0x30 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; scheduling off; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma scheduling off
#pragma peephole off
s32 fn_80057E40(void) {
    ColosseumBattleConnectState* state = (ColosseumBattleConnectState*)lbl_803A9A08;

    if (state->active != 0) {
        return state->connected != 0;
    }
    return 2;
}
#pragma pop
