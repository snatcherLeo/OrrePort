/**
 * @file field_exact_801ED3B8.c
 * @brief fn_801ED3B8 (0x801ED3B8 - 0x801ED640): the field step callback
 *        registered through heroMoveAddStepCallback. Each step it
 *        advances the purification counter of every party Shadow Pokemon
 *        (resetting it and applying the DP form at 0x100), bumps the step
 *        flag 0xD1 (capped at 10000) while flag 0xD0 is set, and for the
 *        Pokemon held in save block 0xB either advances its Shadow counter
 *        or gives it one experience point (learning moves on level-up),
 *        then stores the step count in that block.
 *
 * Function-boundary carve of the field TU (field_range_801ECFE0.c), GC/1.3
 * -O4,p, text only.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* savedataGetStatus(u8* data, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* heroBiosGetPokemonPtr(u8* hero, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(u8* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonBiosGetDarkpokemonDataId(u8* pokemon);
#endif
extern u8 darkPokemonCheckFlag14(u16 darkPokemonId);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonBiosGetDp(u8* pokemon);
#endif
extern u16 darkPokemonSaveGetWalkSteps(u16 darkPokemonId);
extern void darkPokemonSaveSetWalkSteps(u16 darkPokemonId, u16 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std082_flagGetValue(u16 flag);
#endif
extern void _flagSet(u32 flag, u32 value);
extern void pokemonAddDpFormPokemonDpFilterId(u8* pokemon, u32 filter, u32 form);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonBiosGetLevel(u8* pokemon);
#endif
extern u32 pokemonBiosGetExp(u8* pokemon);
extern void pokemonGrowBasisStatus(u8* pokemon, u32 experience);
extern void pokemonOboeWaza(u8* pokemon, u8 level, u32 flag, u8* learned);

/* 0x801ED3B8 | 0x288 */
void fn_801ED3B8(void)
{
    u16 i;
    u8* hero;
    u16 progress;

    hero = PC_PTR(savedataGetStatus(NULL, 2));
    if (hero != NULL) {
        for (i = 0; i < 6; i++) {
            u8* pokemon;
            u16 dark;
            u16 value;

            pokemon = heroBiosGetPokemonPtr(hero, i);
            if (!pokemonCheckValid(pokemon) || pokemon == NULL) {
                continue;
            }
            dark = pokemonBiosGetDarkpokemonDataId(pokemon);
            if (dark != 0 && darkPokemonCheckFlag14(dark)) {
                dark = 0;
            }
            if (dark != 0 && pokemonBiosGetDp(pokemon) != 0) {
                if (darkPokemonSaveGetWalkSteps(dark) >= 0x100) {
                    pokemonAddDpFormPokemonDpFilterId(pokemon, 0, 1);
                    darkPokemonSaveSetWalkSteps(dark, 0);
                } else {
                    value = darkPokemonSaveGetWalkSteps(dark);
                    darkPokemonSaveSetWalkSteps(dark, value + 1);
                }
            }
        }
    }

    if (std082_flagGetValue(0xD0) != 0) {
        progress = std082_flagGetValue(0xD1);
        if (progress < 10000) {
            progress++;
        }
        _flagSet(0xD1, progress);
    }
#ifdef TARGET_PC
    else {
        /* The GameCube leaves `progress` unset here; the day-care store below then writes r31's low half, which on
         * this path still holds the step dispatcher's value (heroMoveMain loads the hero-move work's first word, the
         * leading party member 0 / 1, into r31 before calling the step callbacks): verified from the assembly,
         * needs-review.md 2026-10-16. */
        extern u8 HeroMove[];
        progress = (u16)*(s32*)HeroMove;
    }
#endif

    if (((u8*)PC_PTR(savedataGetStatus(NULL, 0xB)))[0] != 0) {
        u8* pokemon;
        u16 dark;
        u16 value;
        u32 exp;
        u8 learned;

        pokemon = PC_PTR(savedataGetStatus(NULL, 0xB) + 8);
        dark = 0;
        if (pokemon != NULL) {
            dark = pokemonBiosGetDarkpokemonDataId(pokemon);
            if (dark != 0 && darkPokemonCheckFlag14(dark)) {
                dark = 0;
            }
            if (dark != 0 && darkPokemonCheckFlag14(dark)) {
                dark = 0;
            }
        }
        if (dark != 0) {
            if (pokemonBiosGetDp(pokemon) != 0) {
                if (darkPokemonSaveGetWalkSteps(dark) >= 0x100) {
                    pokemonAddDpFormPokemonDpFilterId(pokemon, 0, 3);
                    darkPokemonSaveSetWalkSteps(dark, 0);
                } else {
                    value = darkPokemonSaveGetWalkSteps(dark);
                    darkPokemonSaveSetWalkSteps(dark, value + 1);
                }
            }
        } else if (pokemonBiosGetLevel(pokemon) < 100) {
            exp = pokemonBiosGetExp(pokemon);
            if (exp < 0xFFFFFFFF) {
                exp++;
            }
            pokemonGrowBasisStatus(pokemon, exp);
            learned = 0;
            pokemonOboeWaza(pokemon, pokemonBiosGetLevel(pokemon), 1, &learned);
        }
        hero = PC_PTR(savedataGetStatus(NULL, 0xB));
        if (hero != NULL) {
            *(u16*)(hero + 2) = progress;
        }
    }
}
