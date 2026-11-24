/**
 * gba_misc 0x8008C5D4 - 0x8008C6FC: GameCube status condition to GBA
 * status word. Same code as gbaPokemonConditonFromGC in gba_misc.c, kept
 * as a standalone unit (like the gba_misc_exact_* siblings) so it can be
 * linked.
 */
#include "dolphin/types.h"
#include "game/constants/conditions.h"

extern s16 pokemonGetJoutaiStack(void* pokemon, s32 kind);
extern s8 pokemonGetJoutaiKaisuu(void* pokemon, s32 kind);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonIsJoutaiDataId(void* pokemon, s32 kind);
#endif

/* 0x8008C5D4 | size: 0x128 */
u16 gbaPokemonConditonFromGC(void* pokemon) {
    u16 status = 0;

    if (pokemonIsJoutaiDataId(pokemon, CONDITION_BAD_POISON) != 0) {
        status = (pokemonGetJoutaiStack(pokemon, 4) << 8) | 0x80;
    } else if (pokemonIsJoutaiDataId(pokemon, CONDITION_PARALYSIS) != 0) {
        status |= 0x40;
    } else if (pokemonIsJoutaiDataId(pokemon, CONDITION_FREEZE) != 0) {
        status |= 0x20;
    } else if (pokemonIsJoutaiDataId(pokemon, CONDITION_BURN) != 0) {
        status |= 0x10;
    } else if (pokemonIsJoutaiDataId(pokemon, CONDITION_POISON) != 0) {
        status |= 0x08;
    } else if (pokemonIsJoutaiDataId(pokemon, CONDITION_SLEEP) != 0) {
        status = (s16)pokemonGetJoutaiKaisuu(pokemon, 8);
    }
    return status;
}
