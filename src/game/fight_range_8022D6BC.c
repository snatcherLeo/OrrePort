/**
 * @file fight_range_8022D6BC.c
 * @brief Ability after-hit carve, 0x8022D6BC - 0x8022DCB8 (_wsTokuseiAfterDamage), with
 *        its switch table (.data 0x8039A478 - 0x8039A538), linked.
 *
 * Standalone copy of the shared candidate body, built with GC/1.3.2 at -O4,s
 * (configure.py) with no pragmas.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/gender.h"
#include "game/constants/moves.h"
#include "game/constants/pokemon_status.h"

extern u8 lbl_80478D78[1];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void*, void*, u8);
#endif

u32 _wsTokuseiAfterDamage(u32 attacker, u32 defender)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
    extern u32 fightOutPokemonGetWazaZokuseiDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
    extern u8 fightOutPokemonGetSex();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonMaxHpWaruValue();
#endif
    extern u32 zokuseiDataBiosGetName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
    extern u8 lbl_803798F3[];
    extern u8 lbl_80379907[];
    extern u8 lbl_8037992F[];
    extern u8 lbl_80379932[];
    extern void* lbl_8047B62C;
    extern u32 ServerStatusFlag;
    extern const u16 lbl_8047E600;
    extern const u8 lbl_8047E602;

    s16 status11C;
    s16 status11E;
    u32 defenderAbility;
    u32 moveId;
    u16 moveJoutai;
    u32 moveType;
    u8 moveStatus13;
    u32 moveStatus;
    u8 moveHit;
    u8 status107;
    u32 result;
    u8 attackerSex;
    u8 defenderSex;
    u32 target;
    u32 attackerAbility;
    u8 values[3];

    status11C = pokemonGetStatus(PC_PTR(defender), 0, 0x11C, 0);
    status11E = pokemonGetStatus(PC_PTR(defender), 0, 0x11E, 0);
    attackerAbility = fightOutPokemonGetTokuseiDataId(PC_PTR(attacker));
    defenderAbility = fightOutPokemonGetTokuseiDataId(PC_PTR(defender));
    moveId = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
    moveJoutai = (u16)wazaGetStatus(0, moveId, 7, 0);
    moveType = fightOutPokemonGetWazaZokuseiDataId(attacker);
    moveStatus13 = wazaGetStatus(0, moveId, 0xD, 0);
    moveStatus = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    moveHit = fightWazaIsHit(PC_PTR(moveStatus));
    status107 = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_HURT_ITSELF_FLAG, 0);
    /* RULE-EXCEPTION(user-approved): extern named stand-ins for the TU's own
     * {1, 2, 5} table - see docs/RULE_EXCEPTIONS.md. It stays in
     * sdata2_8047E538.c: MWCC aligns each .sdata2 section to 8, so a carve
     * cannot own 0x8047E600 - 0x8047E604 while fightOutPokemonDoSoubiItemCheck's table follows
     * at 0x8047E604. */
    *(u16*)values = lbl_8047E600;
    values[2] = lbl_8047E602;
    result = 0;
    attackerSex = fightOutPokemonGetSex(attacker);
    defenderSex = fightOutPokemonGetSex(defender);
    target = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(
        defender, fightFloorGetStatus(0, 0, 0x14, 0));

    switch ((u16)defenderAbility) {
    case ABILITY_COLOR_CHANGE:
        if (moveHit == 1 && (u16)moveId != MOVE_STRUGGLE && moveJoutai != 0 &&
            (u16)moveId != MOVE_SHADOW_RUSH &&(status11C != 0 || status11E != 0) &&
            fightOutPokemonIsZokuseiDataId(PC_PTR(defender), moveType) == 0 &&
            fightOutPokemonCheckFightOut(PC_PTR(defender)) == 1) {
            u32 i;
            for (i = 0; (u8)i < 2; i++) {
                fightOutPokemonSetZokuseiDataId(PC_PTR(defender), i, moveType);
            }
            msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(zokuseiDataBiosGetName(moveType))));
            wsRunScript(lbl_8047B62C, lbl_803798F3, 0);
            result = 1;
        }
        break;

    case ABILITY_ROUGH_SKIN:
        if (moveHit == 1 && fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
            status107 == 0 && (status11C != 0 || status11E != 0) &&
            moveStatus13 != 0) {
            wazaSetStatus(PC_PTR(moveStatus), 0, 0x2D, 0,
                          fightOutPokemonMaxHpWaruValue(PC_PTR(attacker), 0x10));
            wsRunScript(lbl_8047B62C, lbl_80379907, 0);
            result = 1;
        }
        break;

    case ABILITY_EFFECT_SPORE:
        if (moveHit == 1 && fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
            status107 == 0 && (status11C != 0 || status11E != 0) &&
            moveStatus13 != 0) {
            if (GSrand() % 10 == 0) {
                u32 pick = (u16)GSrand();
                u32 pickDivisor = 3;
                lbl_80478D78[3] = values[(u8)(pick % pickDivisor)];
                ServerStatusFlag |= 0x2000;
                lbl_80478D78[3] += 0x40;
                wsRunScript(lbl_8047B62C, lbl_8037992F, 0);
                result = 1;
            }
        }
        break;

    case ABILITY_POISON_POINT:
        if (moveHit == 1 && fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
            status107 == 0 && (status11C != 0 || status11E != 0) &&
            moveStatus13 != 0) {
            if (GSrand() % 3 == 0) {
                u32 value = 0x42;
                lbl_80478D78[3] = value;
                ServerStatusFlag = ServerStatusFlag | 0x2000;
                value = 0;
                wsRunScript(lbl_8047B62C, lbl_8037992F, value);
                result = 1;
            }
        }
        break;

    case ABILITY_STATIC:
        if (moveHit == 1 && fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
            status107 == 0 && (status11C != 0 || status11E != 0) &&
            moveStatus13 != 0) {
            if (GSrand() % 3 == 0) {
                u32 value = 0x45;
                lbl_80478D78[3] = value;
                ServerStatusFlag = ServerStatusFlag | 0x2000;
                value = 0;
                wsRunScript(lbl_8047B62C, lbl_8037992F, value);
                result = 1;
            }
        }
        break;

    case ABILITY_FLAME_BODY:
        if (moveHit == 1 && fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
            status107 == 0 && moveStatus13 != 0 &&
            (status11C != 0 || status11E != 0)) {
            if (GSrand() % 3 == 0) {
                u32 value = 0x43;
                lbl_80478D78[3] = value;
                ServerStatusFlag = ServerStatusFlag | 0x2000;
                value = 0;
                wsRunScript(lbl_8047B62C, lbl_8037992F, value);
                result = 1;
            }
        }
        break;

    case ABILITY_CUTE_CHARM:
        if (moveHit == 1 && fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1 &&
            status107 == 0 && moveStatus13 != 0 &&
            (status11C != 0 || status11E != 0) &&
            fightOutPokemonCheckFightOut(PC_PTR(defender)) == 1) {
            if (GSrand() % 3 == 0 &&
                (u16)attackerAbility != ABILITY_OBLIVIOUS &&
                attackerSex != defenderSex && fn_802025B8(PC_PTR(attacker), PC_PTR(0xA)) == 2 &&
                attackerSex != GENDER_GENDERLESS && defenderSex != GENDER_GENDERLESS) {
                fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0xA), PC_PTR(target));
                wsRunScript(lbl_8047B62C, lbl_80379932, 0);
                result = 1;
            }
        }
        break;
    }

    return result;
}
