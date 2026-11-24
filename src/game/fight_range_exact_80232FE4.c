/**
 * @file fight_range_exact_80232FE4.c
 * @brief Exact pure-C fight relationship cleanup island.
 */
#include "dolphin/types.h"

u32 fn_80232FE4(u32 r3, u32 r4, u32 r5)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
    extern void fightOutPokemonSetOumuWazaDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetJoutaiParam();
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
    u8 matched;
    u32 targetId;
    u32 relative;
    struct {
        u32 h;
        void* p;
        void* o;
    } values;
#define hostSide values.h
#define other ((u32)values.o)
#define pokemon ((u32)values.p)

    values.h = r4;
    values.p = (void*)PC_PTR(r3);
    values.o = (void*)PC_PTR(r5);

    if (fightOutPokemonCheckFightOut(PC_PTR(r3)) == 0) {
        return 1;
    }
    if (pokemon == other) {
        return 1;
    }
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
#undef pokemon
#undef other
#undef hostSide

#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
void fightOutPokemonUpdateHensinStatus(void* param1, void* param2)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif

#ifdef TARGET_PC /* fightOutPokemonIsUseHensinBuff reads only r3 */
    if ((u8)fn_801FECD4(param1) == 1) {
#else
    if ((u8)fn_801FECD4(param1, param2) == 1) {
#endif
        fn_801FE7EC(param1, PC_PTR32_IMPLICIT(param2), 0, 0);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
