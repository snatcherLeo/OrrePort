/**
 * @file status.c
 * @brief Decompiled functions.
 *
 * Address range: 0x80135D10 - 0x80135F58
 *
 * Split out of the former game/effect/effect_util.c CodeCandidate
 * bucket (0x8013151C - 0x80137114); see effect_util_types.h for
 * shared cross-TU declarations.
 */

#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"


/* 0x80135D10 | 0x134 */
u32 statusSetStatus(u32 kind, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonSetStatus();
#endif
    extern u32 heroSetStatus();
    extern u32 pcboxSetStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 gamedataSetStatus();
#endif
    extern u32 itemSetStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorSetStatus();
#endif
    extern u32 fightSideSetStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerSetStatus();
#endif
    u32 result = 0;

    switch ((u8)kind) {
    case 0:
        break;
    case 1:
        gamedataSetStatus(PC_PTR(arg1), arg3, arg5);
        break;
    case 2:
        pcboxSetStatus(arg1, arg3, arg5);
        break;
    case 3:
        heroSetStatus(arg1, arg3, arg5);
        break;
    case 4:
        itemSetStatus(arg1, arg2, arg3, arg4, arg5);
        break;
    case 5:
        pokemonSetStatus(PC_PTR(arg1), arg2, arg3, arg4, arg5);
        break;
    case 6:
        wazaSetStatus(PC_PTR(arg1), arg2, arg3, arg4, arg5);
        break;
    case 7:
        result = fightFloorSetStatus(arg1, arg2, arg3, arg4, arg5);
        break;
    case 8:
        fightSideSetStatus(arg1, arg2, arg3, arg4, arg5);
        break;
    case 9:
        fightTrainerSetStatus(PC_PTR(arg1), arg2, arg3, arg4, arg5);
        break;
    default:
        break;
    }
    return result;
}


/* 0x80135E44 | 0x114 */
u32 statusGetStatus(u32 kind, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    switch ((u8)kind) {
    case 0:
        return 0;
    case 1:
        return gamedataGetStatus((void*)PC_PTR(arg1), arg3);
    case 2:
        return pcboxGetStatus(arg1, arg3, arg4);
    case 3:
        return heroGetStatus(PC_PTR(arg1), arg3, arg4);
    case 4:
        return itemGetStatus(PC_PTR(arg1), arg2, arg3, arg4);
    case 5:
        return pokemonGetStatus(PC_PTR(arg1), arg2, arg3, arg4);
    case 6:
        return wazaGetStatus(PC_PTR(arg1), arg2, arg3, arg4);
    case 7:
        return fightFloorGetStatus(PC_PTR(arg1), arg2, arg3, arg4);
    case 8:
        return fightSideGetStatus(PC_PTR(arg1), arg2, arg3, arg4);
    case 9:
        return fightTrainerGetStatus(PC_PTR(arg1), arg2, arg3, arg4);
    default:
        return 0;
    }
}
