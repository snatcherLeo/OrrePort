/**
 * @file fight_range_exact_8022B29C.c
 * @brief Exact pure-C fight target dispatcher at 0x8022B29C.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetFightOutPokemonPtrRandom();
#endif

void fightGetRandomOpponent(s32 arg)
{
    fightFloorGetFightOutPokemonPtrRandom(0, 1, 3, arg);
}
