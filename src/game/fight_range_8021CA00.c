/**
 * @file fight_range_8021CA00.c
 * @brief Exact pure-C fight-sequence helper, 0x8021CA00 - 0x8021CB58.
 *
 * Prefix before the exact 0x8021CB58 - 0x8021D40C island.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"
#include "game/constants/types.h"

extern u8* lbl_8047B610;
extern u8 lbl_80478D78[8];

/*
 * wsCmd7F (0x8021CA00)
 * Sequence opcode: when the current move misses or the defender's state
 * 0x1c is not 2, sets floor status 0x3b to 0x40 and result 1; when the
 * defender has type 0xc, the same with result 2; otherwise records the
 * attacker as the 0x1c source on the defender and result 0. Advances the
 * sequence PC by one byte.
 */
/* RULE-EXCEPTION(user-approved): local optimize_for_size pragma — see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd7F(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
    u32 fightTarget;
    u32 move;
    u32 relativeTarget;
    u16 floorId;

    fightTarget = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = pokemonGetStatus(PC_PTR(fightTarget), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    floorId = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    relativeTarget = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(fightTarget, floorId);
    fightTarget = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));

    if (fightWazaIsHit(PC_PTR(move)) == 0 || fn_802025B8(PC_PTR(fightTarget), PC_PTR(0x1c)) != 2) {
        fightFloorSetStatus(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 1;
        lbl_8047B610++;
    } else if (fightOutPokemonIsZokuseiDataId(PC_PTR(fightTarget), TYPE_GRASS) == 1) {
        fightFloorSetStatus(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 2;
        lbl_8047B610++;
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(fightTarget), PC_PTR(0x1c), PC_PTR(relativeTarget));
        lbl_80478D78[5] = 0;
        lbl_8047B610++;
    }
}
#pragma optimize_for_size reset
