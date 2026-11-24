/**
 * @file fight_range_8021FAD4.c
 * @brief wsCmd49 (move-script command), 0x8021FAD4 - 0x80220778, with its
 *        switch jump table (.data 0x8039A0A8 - 0x8039A0F0).
 *
 * Moved out of the shared fight_range_80211A00.c so the unit compiles only
 * this function and owns its jump table. In the shared file it sat inside an
 * optimize_for_size region; the unit builds at -O4,s instead.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the move-effect script table lbl_80379BFF is big-endian disc data on the PC */
#include "game/constants/conditions.h"
#include "game/constants/move_effects.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
extern u32 ServerStatusFlag;
extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void*, void*, u8);
#endif
extern u8 lbl_80478D78[8];

extern void wazaSequenceOwnerClearState(void* effect, s32 trajType);

void wsCmd49(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
    extern void fightFloorReplaceFightOutPokemonAttackToDefense();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuAllFightTrainerCloseStatusMenu();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuAllFightOutPokemonCloseStatusMenu();
#endif
    u16* status;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuCloseMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
    extern u8 fightOutPokemonCheckNoAttackFlag();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckValid();
#endif
    extern u8 fightOutPokemonIsAlly();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonIsJoutaiKie();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetSoubiItemSoubiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetMotoWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
    extern void fightOutPokemonInitJoutaiKie();
    extern void fightOutPokemonSetVisibility();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
    extern void fightOutPokemonSetOumuWazaDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsJoutaiDataId();
#endif
    extern void fightWazaInitLoop();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
    /* RULE-EXCEPTION(user-approved): pokemonSetStatus prototype with a u16 last parameter (include/game/pokemon_evolution.h says u32) so the (u16) conversions follow the target move — see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(u32, int, int, int, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightOutPokemonGetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
    extern void fn_8022D2CC();
    extern u8 _wsTokuseiAfterDamage();
    extern void _fightTokuseiCureJoutaiSub();
    extern void _wsCheckSynchronizeAttacker();
    extern u32 _wsSoubiItemAfterDamage();
    extern void _fightOutPokemonDoStockItemSub();
    extern void _fightOutPokemonSoubiItemEffectSub();
    extern void _WsWkcActSubJoutaiMigawari__FPvUsPv();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerSetState();
#endif
    extern u8 lbl_8047B627;
    extern u8 lbl_80378D02[];
    extern u8 lbl_803792C9[];
    extern u8 lbl_80379BF4[];
    extern u32 lbl_80379BFF[];
    u16 floorStatus[1];
    u32 stopState;
    u8 modeValue;
    u32 mode;
    u32 attacker;
    u32 defender;
    u16 heldItem;
    u16 movePower;
    u32 usedMove;
    u16 moveEffect;
    u32 originalMove;
    u32 moveStatus;
    u8 moveType;
    u8 stop;
    u8 moveHit;
    s16 defenderValue11C;
    s16 defenderValue11E;
    u16 moveJoutai;
    u32 validOriginalMove;
    s8 count;
    u32 state;
    u32 other;
    u32 flags1;
    u32 flags2;

    floorStatus[0] = fightFloorGetStatus(0, 0, 0x14, 0);
    stop = 0;
    /* RULE-EXCEPTION(user-approved): codegen-only cast - the (u16) widening of the u8 script byte keeps stopState a plain u32 coloured as in retail (replaces a volatile local) - see docs/RULE_EXCEPTIONS.md */
    stopState = (u16)lbl_8047B610[2];
    mode = lbl_8047B610[1];
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    if (attacker != 0) {
        heldItem = fightOutPokemonGetSoubiItemSoubiDataId(PC_PTR(attacker));
        moveStatus = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
        moveHit = fightWazaIsHit(PC_PTR(moveStatus));
        usedMove = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
        originalMove = fightOutPokemonGetMotoWazaDataId(PC_PTR(attacker));
        validOriginalMove = originalMove & ~-((u16)originalMove == 0xFFFF);
#ifdef TARGET_PC /* the 5th argument only shapes MWCC's register use (r7 is a scratch for the code after the call); wazaGetStatus reads r4-r6 */
        movePower = wazaGetStatus(PC_PTR(moveStatus), 0, 0x30, 0);
#else
        movePower = wazaGetStatus(PC_PTR(moveStatus), 0, 0x30, 0, (u16)originalMove);
#endif
        moveType = wazaGetStatus(0, usedMove, 5, 0);
        moveEffect = wazaGetStatus(0, usedMove, 9, 0);
        moveJoutai = wazaGetStatus(0, usedMove, 7, 0);
    } else {
        heldItem = 0;
        moveStatus = 0;
        moveHit = 0;
        usedMove = 0;
        originalMove = 0;
        validOriginalMove = 0;
        movePower = 9;
        moveType = 1;
        moveEffect = 0;
        moveJoutai = 0;
    }

    defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    if (defender != 0) {
        defenderValue11C = pokemonGetStatus(PC_PTR(defender), 0, 0x11C, 0);
        defenderValue11E = pokemonGetStatus(PC_PTR(defender), 0, 0x11E, 0);
    } else {
        defenderValue11C = 0;
        defenderValue11E = 0;
    }
    modeValue = (u8)mode;

    do {
        switch (lbl_8047B627) {
        case 0:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(defender), PC_PTR(0x13)) == 1 &&
                fightOutPokemonCheckFightOut(PC_PTR(defender)) == 1 &&
                attacker != defender &&
                fightOutPokemonIsAlly(attacker, defender) == 0 &&
                moveHit == 1 &&
                (defenderValue11C != 0 || defenderValue11E != 0) &&
                moveJoutai != 0) {
                count = pokemonGetStatus(PC_PTR(defender), 0, POKEMON_STATUS_STAGE_ATTACK, 0);
                if (count < 12) {
                    pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_STAGE_ATTACK, 0, (s8)(count + 1));
                    wsRunScript(lbl_8047B62C, lbl_80378D02, 0);
                    stop = 1;
                }
            }
            lbl_8047B627++;
            break;

        case 1:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(defender), PC_PTR(CONDITION_FREEZE)) == 1 &&
                fightOutPokemonCheckFightOut(PC_PTR(defender)) == 1 &&
                attacker != defender &&
                pokemonGetStatus(PC_PTR(defender), 0, 0x11E, 0) != 0 &&
                fightWazaIsHit(PC_PTR(moveStatus)) == 1 &&
                (u16)wazaGetStatus(0, usedMove, 3, 0) == 10) {
                fightOutPokemonWriteJoutaiDataId(PC_PTR(defender), PC_PTR(CONDITION_FREEZE));
                if (fightOutPokemonIsUseHensinBuff(PC_PTR(defender)) == 1) {
                    fightOutPokemonSetHensinPokemonStatusId(PC_PTR(defender), 0x7C, 0, 0);
                }
                wsRunScript(lbl_8047B62C, lbl_803792C9, 0);
                stop = 1;
            }
            lbl_8047B627++;
            break;

        case 2:
            fn_8022D2CC(attacker, defender);
            lbl_8047B627++;
            break;

        case 3:
            if (_wsTokuseiAfterDamage(attacker, defender) != 0) {
                stop = 1;
            }
            lbl_8047B627++;
            break;

        case 4:
            fightFloorLoopValidFightOutPokemon(0, _fightTokuseiCureJoutaiSub, 0, 0);
            lbl_8047B627++;
            break;

        case 5:
            _wsCheckSynchronizeAttacker(attacker, defender);
            lbl_8047B627++;
            break;

        case 6:
            if ((ServerStatusFlag & 0x02000000) != 0 && heldItem == 0x1D &&
                (u16)originalMove != 0xA5 && (u16)originalMove != 0x164 &&
                fn_802025B8(PC_PTR(attacker), PC_PTR(CONDITION_CHOICE_LOCK)) == 2) {
                if ((u16)originalMove == 0xE2 && fightWazaIsJoutaiDataId(PC_PTR(moveStatus), 0x45) == 1) {
                    lbl_8047B627++;
                    break;
                }
                fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(CONDITION_CHOICE_LOCK), 0);
                fightOutPokemonSetPokemonJoutai0C(PC_PTR(attacker), PC_PTR(0x36), originalMove);
            }
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(CONDITION_CHOICE_LOCK)) == 1) {
                other = fightOutPokemonGetPokemonJoutai0C(PC_PTR(attacker), PC_PTR(0x36));
                if (pokemonSearchWazaDataId(fightOutPokemonGetPokemonPtr(PC_PTR(attacker)), other) < 0) {
                    fightOutPokemonWriteJoutaiDataId(PC_PTR(attacker), PC_PTR(CONDITION_CHOICE_LOCK));
                }
            }
            lbl_8047B627++;
            break;

        case 7:
            fightFloorLoopValidFightOutPokemon(0, _fightOutPokemonDoStockItemSub, 0, 0);
            lbl_8047B627++;
            break;

        case 8:
            fightFloorLoopValidFightOutPokemon(0, _fightOutPokemonSoubiItemEffectSub, 0, 0);
            lbl_8047B627++;
            break;

        case 9:
            _wsSoubiItemAfterDamage(attacker, defender);
            lbl_8047B627++;
            break;

        case 10:
            if (fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1) {
                state = fightOutPokemonIsJoutaiKie(PC_PTR(attacker));
                if (state != 0) {
                    if (state != 0x1F) {
                        fightOutPokemonSetVisibility(attacker, 0);
                    } else {
                        other = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
                        wazaSequenceOwnerSetState(PC_PTR(other), 3);
                    }
                }
            }
            lbl_8047B627++;
            break;

        case 11:
            if (fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
                (moveHit == 0 || fightOutPokemonIsJoutaiKie(PC_PTR(attacker)) == 0 ||
                 fightOutPokemonCheckNoAttackFlag(attacker) == 1)) {
                fightOutPokemonInitJoutaiKie(attacker);
                pokemonSetStatus(PC_PTR(attacker), 0, 0x115, 0, 1);
                fightOutPokemonSetVisibility(attacker, 1);
                other = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
                if (other != 0) {
                    wazaSequenceOwnerClearState((void*)PC_PTR(other), 3);
                }
            }
            lbl_8047B627++;
            break;

        case 12:
            if (fightOutPokemonCheckFightOut(PC_PTR(defender)) == 1 &&
                pokemonGetStatus(PC_PTR(defender), 0, 0x115, 0) == 0 &&
                fightOutPokemonIsJoutaiKie(PC_PTR(defender)) == 0) {
                fightOutPokemonSetVisibility(defender, 1);
                other = pokemonGetStatus(PC_PTR(defender), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
                if (other != 0) {
                    wazaSequenceOwnerClearState((void*)PC_PTR(other), 3);
                }
            }
            lbl_8047B627++;
            break;

        case 13:
            fightFloorLoopValidFightOutPokemon(0, _WsWkcActSubJoutaiMigawari__FPvUsPv, 0, 0);
            lbl_8047B627++;
            break;

        case 14:
            if ((ServerStatusFlag & 0x1000) != 0) {
                fightFloorReplaceFightOutPokemonAttackToDefense(0);
                ServerStatusFlag &= 0xFFFFEFFF;
                attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
                if (attacker != 0) {
                    heldItem = fightOutPokemonGetSoubiItemSoubiDataId(PC_PTR(attacker));
                    moveStatus = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
                    moveHit = fightWazaIsHit(PC_PTR(moveStatus));
                    usedMove = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
                    originalMove = fightOutPokemonGetMotoWazaDataId(PC_PTR(attacker));
                    validOriginalMove = originalMove & ~-((u16)originalMove == 0xFFFF);
#ifdef TARGET_PC /* the 5th argument only shapes MWCC's register use (r7 is a scratch for the code after the call); wazaGetStatus reads r4-r6 */
                    movePower = wazaGetStatus(PC_PTR(moveStatus), 0, 0x30, 0);
#else
                    movePower = wazaGetStatus(PC_PTR(moveStatus), 0, 0x30, 0, (u16)originalMove);
#endif
                    moveType = wazaGetStatus(0, usedMove, 5, 0);
                    moveEffect = wazaGetStatus(0, usedMove, 9, 0);
                    moveJoutai = wazaGetStatus(0, usedMove, 7, 0);
                } else {
                    heldItem = 0;
                    moveStatus = 0;
                    moveHit = 0;
                    usedMove = 0;
                    originalMove = 0;
                    validOriginalMove = 0;
                    movePower = 9;
                    moveType = 1;
                    moveEffect = 0;
                    moveJoutai = 0;
                }
                defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
                if (defender != 0) {
                    defenderValue11C = pokemonGetStatus(PC_PTR(defender), 0, 0x11C, 0);
                    defenderValue11E = pokemonGetStatus(PC_PTR(defender), 0, 0x11E, 0);
                } else {
                    defenderValue11C = 0;
                    defenderValue11E = 0;
                }
            }

            if ((u16)wazaGetStatus(0, validOriginalMove, 9, 0) != MOVE_EFFECT_BATON_PASS ||
                ((u16)wazaGetStatus(0, validOriginalMove, 9, 0) == MOVE_EFFECT_BATON_PASS && moveHit == 0)) {
                pokemonSetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_SKETCH_MOVE, 0, (u16)originalMove);
            }
            if (fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
                (u16)wazaGetStatus(0, validOriginalMove, 9, 0) != MOVE_EFFECT_BATON_PASS) {
                if ((ServerStatusFlag & 0x02000000) != 0) {
                    pokemonSetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0, (u16)originalMove);
                    pokemonSetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_LAST_USED_MOVE, 0, (u16)usedMove);
                } else {
                    pokemonSetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0, 0xFFFF);
                    pokemonSetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_LAST_USED_MOVE, 0, 0xFFFF);
                }
                if (fightOutPokemonCheckFightOut(PC_PTR(defender)) == 1) {
                    /* RULE-EXCEPTION(user-approved): carrier pointer - status only re-reads floorStatus[0]; going through it gives the read the register order retail has - see docs/RULE_EXCEPTIONS.md */
                    status = floorStatus;
                    state = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(attacker, status[0]);
                    pokemonSetStatus(PC_PTR(defender), 0, 0xF2, 0, state);
                }
                if ((ServerStatusFlag & 0x02000000) != 0 && moveHit == 1) {
                    if ((u16)originalMove == 0xFFFF) {
                        pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_LAST_HIT_BY_MOVE, 0, (u16)originalMove);
                    } else {
                        pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_LAST_HIT_BY_MOVE, 0, (u16)usedMove);
                        /* RULE-EXCEPTION(user-approved): no-op (u16) cast on a u16 local for argument order — see docs/RULE_EXCEPTIONS.md */
                        pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE, 0, (u16)movePower);
                    }
                } else {
                    pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_LAST_HIT_BY_MOVE, 0, 0xFFFF);
                }
            }
            lbl_8047B627++;
            break;

        case 15:
            if (fightOutPokemonCheckValid(PC_PTR(attacker)) == 1 &&
                (u8)wazaGetStatus(0, validOriginalMove, 0x11, 0) == 1 &&
                (ServerStatusFlag & 0x02000000) != 0 && attacker != defender &&
                fightOutPokemonCheckValid(PC_PTR(defender)) == 1 && moveHit == 1) {
                fightOutPokemonSetOumuWazaDataId(defender, attacker, originalMove);
            }
            lbl_8047B627++;
            break;

        case 16:
            if ((ServerStatusFlag & 0x80000) == 0 &&
                (u16)fightFloorGetStatus(0, 0, 0x19, 0) >= 2 &&
                pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_CHARGING_MOVE_FLAG, 0) == 0 &&
                moveType == 4 && (ServerStatusFlag & 0x200) == 0) {
                other = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0xE, defender));
                if (fightOutPokemonCheckFightOut(PC_PTR(other)) == 1) {
                    fightFloorSetStatus(0, 0, 0x43, 0, other);
                    fightWazaInitLoop(moveStatus);
                    {
                    flags1 = ServerStatusFlag & 0xFFFFFFBF;
                    lbl_80478D78[3] = 0;
                    *(volatile u32*)&ServerStatusFlag = flags1;
                    flags2 = ServerStatusFlag & 0xFFFFBFFF;
                    *(volatile u32*)&ServerStatusFlag = flags2;
                    lbl_80478D78[6] = 0;
                    ServerStatusFlag = flags2 | 0x200;
                    lbl_8047B627 = 0;
                    wsRunScript(lbl_8047B62C, lbl_80379BF4, 0);
                    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(&lbl_80379BFF[moveEffect]));
                    return;
                    }
                }
                ServerStatusFlag |= 0x200;
            }
            lbl_8047B627++;
            break;

        case 17:
            if (modeValue == 0) {
                fightMenuAllFightTrainerCloseStatusMenu(0);
                fightMenuAllFightOutPokemonCloseStatusMenu(0);
                fightMenuCloseMsg();
            }
            break;
        }

        if (modeValue == 1 && stop == 0) {
            lbl_8047B627 = 0x11;
        }
        if (modeValue == 2 && (u8)stopState == lbl_8047B627) {
            lbl_8047B627 = 0x11;
        }
    } while (lbl_8047B627 != 0x11 && stop == 0);

    if (lbl_8047B627 == 0x11 && stop == 0) {
        lbl_8047B610 += 3;
    }
}
