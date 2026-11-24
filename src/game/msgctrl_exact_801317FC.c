/**
 * @file msgctrl_exact_801317FC.c
 * @brief Strict message-control getters, 0x801317FC - 0x8013182C.
 */
#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

u32 msgctrlPokemonID(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return pokemonDataBiosGetName(pokemonDataBiosGetPtr(_PokemonID)); /* r3 = the species data pointer (assembly) */
}

u32 msgctrlString(void)
{
    return _String;
}
