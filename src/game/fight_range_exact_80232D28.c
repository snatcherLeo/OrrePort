/**
 * @file fight_range_exact_80232D28.c
 * @brief Exact island 0x80232D28 - 0x80232FE4 (_fightCheckLockOnSub), moved out of
 *        fight_range_80211A00.c.
 */
#include "dolphin/types.h"
#include "game/constants/conditions.h"

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
u32 _fightCheckLockOnSub(s32 pokemonArg, u32 hostSide, u32* data)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
    extern void fightOutPokemonSetOumuWazaDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80201FDC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
    extern u8 fightOutPokemonIsAlly();
    extern void fightOutPokemonEnemyClearOumu();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
    u32 pokemon;
    u16 statusTarget;
    u32 other;
    u32 targetId;
    u32 relative;
    u8 matched;

    pokemon = pokemonArg;
    statusTarget = (u16)data[1];
    other = data[0];
    if (fightOutPokemonCheckFightOut(PC_PTR(pokemonArg)) == 0) {
        return 1;
    }
    if (pokemon == other) {
        return 1;
    }

    if (statusTarget == 0x7f) {
        if (fightOutPokemonIsAlly(other, pokemon) == 0) {
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(CONDITION_LOCK_ON)) == 1 &&
                (targetId = fightOutPokemonGetJoutaiParam(PC_PTR(pokemon), PC_PTR(0x1d)), (targetId & 0xffff) != 0) &&
                (relative = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(
                     targetId, hostSide)) != 0 &&
                relative == other) {
                matched = 1;
            } else {
                matched = 0;
            }
            if (matched == 1) {
                fn_80201FDC(PC_PTR(pokemon), PC_PTR(0x1d), 0);
            }
        }
    } else {
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(0x16)) == 1 &&
            (targetId = fightOutPokemonGetJoutaiParam(PC_PTR(pokemon), PC_PTR(0x16)), (targetId & 0xffff) != 0) &&
            (relative = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(
                 targetId, hostSide)) != 0 &&
            relative == other) {
            matched = 1;
        } else {
            matched = 0;
        }
        if (matched == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(pokemon), PC_PTR(0x16));
        }
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(CONDITION_LOCK_ON)) == 1 &&
            (targetId = fightOutPokemonGetJoutaiParam(PC_PTR(pokemon), PC_PTR(0x1d)), (targetId & 0xffff) != 0) &&
            (relative = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(
                 targetId, hostSide)) != 0 &&
            relative == other) {
            matched = 1;
        } else {
            matched = 0;
        }
        if (matched == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(pokemon), PC_PTR(CONDITION_LOCK_ON));
        }
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(10)) == 1 &&
        (targetId = fightOutPokemonGetJoutaiParam(PC_PTR(pokemon), PC_PTR(10)), (targetId & 0xffff) != 0) &&
        (relative = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(
             targetId, hostSide)) != 0 &&
        relative == other) {
        matched = 1;
    } else {
        matched = 0;
    }
    if (matched == 1) {
        fightOutPokemonWriteJoutaiDataId(PC_PTR(pokemon), PC_PTR(10));
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(0xe)) == 1 &&
        (targetId = fightOutPokemonGetJoutaiParam(PC_PTR(pokemon), PC_PTR(0xe)), (targetId & 0xffff) != 0) &&
        (relative = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(
             targetId, hostSide)) != 0 &&
        relative == other) {
        matched = 1;
    } else {
        matched = 0;
    }
    if (matched == 1) {
        fightOutPokemonWriteJoutaiDataId(PC_PTR(pokemon), PC_PTR(0xe));
    }
    if (fightOutPokemonIsAlly(pokemon, other) == 0) {
        fightOutPokemonSetOumuWazaDataId(pokemon, other, 0);
        fightOutPokemonEnemyClearOumu(pokemon, other);
    }
    return 1;
}
#pragma optimize_for_size reset
