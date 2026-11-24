/**
 * @file fight_range_exact_8022808C.c
 * @brief Exact island 0x8022808C - 0x802282D8 (wsCmd03), moved out of
 *        fight_range_80211A00.c.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
extern u8 lbl_80478D78[8];
extern u32 ServerStatusFlag;
extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void*, void*, u8);
#endif

void wsCmd03(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u32 ServerStatusFlag;
    u32 attacker;
    u32 statusSlot;
    u32 target;
    u32 ability;
    u32 move;
    u32 usedFlags;
    u8 deduction;
    u32 pokemon;
    u8 pp;
    u8 moveField;

    deduction = 1;
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    statusSlot = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    ability = fightOutPokemonGetTokuseiDataId(PC_PTR(target));

    if ((ServerStatusFlag & 0xa00) == 0) {
        move = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
        moveField = (u8)wazaGetStatus(0, move, 5, 0);
        statusSlot = (s8)wazaGetStatus(PC_PTR(statusSlot), 0, 0x26, 0);
        pp = (u8)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, (s8)statusSlot);

        if ((s32)pokemonGetStatus(PC_PTR(attacker), 0, 0x118, 0) == 0) {
            switch (moveField) {
            case 6:
                deduction = fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId(
                    0, ABILITY_PRESSURE, 0, attacker) + 1;
                break;
            case 4:
            case 7:
                deduction = fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId(
                    0, ABILITY_PRESSURE, 2, attacker) + 1;
                break;
            default:
                if (attacker != target && (u16)ability == ABILITY_PRESSURE) {
                    deduction = 2;
                }
                break;
            }
        }

        if (pp != 0) {
            pokemonSetStatus((void*)PC_PTR(attacker), 0, 0x111, 0, 1);
            if (deduction < pp) {
                pp -= deduction;
            } else {
                pp = 0;
            }
            pokemonSetStatus((void*)PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, (s8)statusSlot, pp);

            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x10)) == 0 &&
                fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x31)) == 1) {
                usedFlags = fightOutPokemonGetPokemonJoutai08(attacker, 0x31);
                if ((usedFlags & (1 << (u8)statusSlot)) == 0 &&
                    fightOutPokemonIsUseHensinBuff(PC_PTR(attacker)) == 1) {
                    fightOutPokemonSetHensinPokemonStatusId(
                        PC_PTR(attacker), 0x80, (u8)statusSlot, 0);
                }
            }
        }
    }

    ServerStatusFlag &= 0xfffff7ff;
    lbl_8047B610++;
}
