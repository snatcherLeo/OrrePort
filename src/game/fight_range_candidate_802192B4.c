/**
 * @file fight_range_candidate_802192B4.c
 * @brief Fight sequence commands, 0x802192B4 - 0x80219804 (wsCmdA7,
 *        wsCmdA6, wsCmdA5, wsCmdA4).
 *
 * wsCmdA6 picks a random entry of its stack type array as
 * `p = typeArray; p += index; *p`, written after the random call: that
 * creates the array base before the modulo operands, which gives retail's
 * volatile registers (random r4, count r3, base r5).
 *
 * wsCmdA4 needs fightOutPokemonSetPokemonJoutai08's third parameter prototyped as a full
 * word (s32): the slot is then sign-extended again at that call instead
 * of reusing the extension made for pokemonGetStatus.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/conditions.h"
#include "game/constants/move_effects.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif

void wsCmdA7(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
    void* attacker;
    void* target;
    u16 floor;
    void* targetId;

    attacker = (void*)fightTargetGetPtrAsNowFightType(0x11, 0);
    floor = fightFloorGetStatus(0, 0, 0x14, 0);
    targetId = PC_PTR(fightTargetGetTragetPtrToRelativeHostSideFightTargetId(PC_PTR32_IMPLICIT(attacker), floor));
    target = (void*)fightTargetGetPtrAsNowFightType(0x12, 0);
    if (fn_802025B8(target, PC_PTR(CONDITION_LOCK_ON)) == 2) {
        fightOutPokemonWritePokemonJoutaiDataId(target, PC_PTR(CONDITION_LOCK_ON), targetId);
    }
    lbl_8047B610++;
}

void wsCmdA6(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
    extern u32 zokuseiDataBiosGetName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    extern u32 fightOutPokemonGetTeikouZokuseiDataIdAry();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonSetZokuseiDataId(u32, u8, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    u8 specialMove;
    u16 hostSide;
    u32 pokemon;
    u16 type;
    u16 sourceType;
    u32 target;
    s32 random;
    u32 count;
    u32 selected;
    u16 i;
    u32 typeArray[18];
    u32* p;

    hostSide = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    pokemon = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    type = (u16)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_LAST_HIT_BY_MOVE, 0);
    pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_PHYSICAL_DAMAGE_TAKEN, 0);
    pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_SPECIAL_DAMAGE_TAKEN, 0);
    sourceType = (u16)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE, 0);
    target = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(
        (u16)pokemonGetStatus(PC_PTR(pokemon), 0, 0xf2, 0), hostSide);

    if (type == 0 || type == 0x165 || type == 0xffff || type == 0x164) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    } else {
        if (target != 0) {
            type = (u16)wazaGetStatus(0, type, 9, 0);
            if (type == MOVE_EFFECT_SKULL_BASH || type == MOVE_EFFECT_RAZOR_WIND || type == MOVE_EFFECT_SKY_ATTACK ||
                type == MOVE_EFFECT_SOLARBEAM || type == MOVE_EFFECT_SEMI_INVULNERABLE || type == MOVE_EFFECT_BIDE) {
                specialMove = 1;
            } else {
                specialMove = 0;
            }
            if (specialMove && fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(target), PC_PTR(0x22)) == 1) {
                lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
                return;
            }
        }
        count = fightOutPokemonGetTeikouZokuseiDataIdAry(pokemon, sourceType, typeArray);
        if ((count & 0xffff) == 0) {
            lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
        } else {
            random = GSrand();
            p = typeArray;
            p += (u16)((u16)random % (s32)(u16)count);
            random = *p;
            if (random < 0) {
                lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
            } else {
                selected = random & 0xffff;
                for (i = 0; i < 2; i++) {
                    fightOutPokemonSetZokuseiDataId(PC_PTR(pokemon), (u8)i, selected);
                }
                msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(zokuseiDataBiosGetName(selected))));
                lbl_8047B610 += 5;
            }
        }
    }
}

void wsCmdA5(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern s32 lbl_8047B608;
    u32 attacker;
    u32 defender;
    u32 move;
    u32 attackerPokemon;
    s32 attackerHp;
    u32 defenderPokemon;
    s32 defenderHp;
    s32 average;

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    attackerPokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
    attackerHp = (u16)pokemonGetStatus(PC_PTR(attackerPokemon), 0, POKEMON_STATUS_HP, 0);
    defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    defenderPokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(defender));
    defenderHp = (u16)pokemonGetStatus(PC_PTR(defenderPokemon), 0, POKEMON_STATUS_HP, 0);

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(defender), PC_PTR(0x14)) == 0) {
        average = (attackerHp + defenderHp) / 2;
        lbl_8047B608 = defenderHp - average;
        wazaSetStatus(PC_PTR(move), 0, 0x2d, 0, attackerHp - average);
        pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0, 0xFFFF);
        lbl_8047B610 += 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}

void wsCmdA4(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonSetPokemonJoutai08(u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    u8 blocked;
    u32 target;
    u32 pokemon;
    u16 lastMove;
    s8 slot;
    u8 pp;
    u16 move;

    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(target));
    lastMove = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0);
    slot = pokemonSearchWazaDataId(PC_PTR(pokemon), lastMove);
    if (slot < 0) {
        pp = 0;
    } else {
        pp = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slot);
    }
    if (lastMove == 0xa5 || lastMove == 0xe3 || lastMove == 0x77 || lastMove == 0xffff) {
        blocked = 1;
    } else {
        blocked = 0;
    }
    if (blocked == 1) {
        pp = 0;
        slot = -1;
    }
    if (fn_802025B8(PC_PTR(target), PC_PTR(0x2a)) == 2 && slot >= 0 && pp != 0) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(target), PC_PTR(0x2a), 0);
        move = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE, slot);
        fightOutPokemonSetPokemonJoutai0C(PC_PTR(target), PC_PTR(0x2a), move);
        fightOutPokemonSetPokemonJoutai08(PC_PTR(target), PC_PTR(0x2a), PC_PTR(slot));
        lbl_8047B610 += 5;
        return;
    }
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
}
