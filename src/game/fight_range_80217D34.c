#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/conditions.h"

extern u8* lbl_8047B610;

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmdB4(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u8 index;
    u32 target;
    u32 statusId;
    s8 value;

    index = lbl_8047B610[1];
    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    switch ((u8)(s8)index) {
    case 1:
        statusId = 0xe6;
        break;
    case 2:
        statusId = 0xe7;
        break;
    case 3:
        statusId = 0xea;
        break;
    case 4:
        statusId = 0xe8;
        break;
    case 5:
        statusId = 0xe9;
        break;
    case 6:
        statusId = 0xeb;
        break;
    case 7:
        statusId = 0xec;
        break;
    case 0:
    default:
        statusId = 0;
        break;
    }
    value = pokemonGetStatus(PC_PTR(target), 0, statusId, 0);
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(target), PC_PTR(CONDITION_CONFUSION)) == 1 && value >= 12) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 2));
    } else {
        lbl_8047B610 += 6;
    }
}
#pragma optimize_for_size reset
