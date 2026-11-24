/**
 * @file fight_range_80211A00_suffix_80216A58.c
 * @brief Beat Up damage and neighbours, 0x80216A58 - 0x80217018
 *        (wsCmdC4, fn_80216CF8, wsCmdC3, wsCmdC2).
 *
 * Standalone source at GC/1.3 -O4,s with no pragmas. wsCmdC4 walks the
 * party with heroGetStatus(party, 3, index): the index is the third argument,
 * which is why retail loads the loop test into r5. Its locals follow the
 * declaration order found with the colouring simulator; the damage division is
 * two statements (divide by the defense, then /50 + 2).
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/pokemon_status.h"
extern u8* lbl_8047B610;
extern u8 lbl_80478D78[1];
void wsCmdC4(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerCheckTemotiPokemonFightEntry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightPokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
    extern u8 pokemonCheckFightOut();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiNormal();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 figthOutPokemonGetPokemonDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 figthOutPokemonGetLevel();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
    s32 damage;
    u32 attacker;
    u32 power;
    u32 defender;
    u32 fightPokemon;
    u32 trainer;
    u32 pokemon;
    u32 party;
    u32 move;
    u8 initialIndex;
    u16 species;
    u32 moveData;
    s32 result;
    u32 defenderPokemon;
    u8 level;
    s32 defense;

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    moveData = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    move = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
    trainer = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, attacker));
    party = fightTrainerGetStatus(PC_PTR(trainer), 0, 0x44, 0);
    defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    if (fightOutPokemonCheckFightOut(PC_PTR(defender)) == 0) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
        return;
    }

    initialIndex = lbl_80478D78[0];
    pokemon = 0;
    for (; lbl_80478D78[0] < 6; lbl_80478D78[0]++) {
        pokemon = heroGetStatus(PC_PTR(party), 3, lbl_80478D78[0]);
        fightPokemon = PC_PTR32_IMPLICIT(fightTrainerCheckTemotiPokemonFightEntry(trainer, pokemon));
        if (fightPokemon != 0 && fightPokemonCheckValid(PC_PTR(fightPokemon)) != 0) {
            pokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(fightPokemon));
            if (pokemonCheckFightOut(pokemon) == 1 && pokemonIsJoutaiNormal(PC_PTR(pokemon)) == 1) {
                break;
            }
        }
    }
    if (lbl_80478D78[0] < 6) {
        species = (u16)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_SPECIES, 0);
        level = (u8)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_LEVEL, 0);
        defenderPokemon = figthOutPokemonGetPokemonDataId(PC_PTR(defender));
        figthOutPokemonGetLevel(PC_PTR(defender));
        msgctrlSetValue(0xd, pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_NICKNAME, 0));
        damage = (s32)pokemonGetStatus(0, species, POKEMON_STATUS_BASE_ATTACK, 0);
        power = wazaGetStatus(0, move, 7, 0);
        damage *= power & 0xffff;
        damage *= (((s32)level << 1) / 5) + 2;
        defense = (s32)pokemonGetStatus(0, defenderPokemon, POKEMON_STATUS_BASE_DEFENSE, 0);
        if (defense <= 0) {
            defense = 1;
        }
        damage /= defense;
        damage = damage / 50 + 2;
        result = damage;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x32)) == 1) {
            result = damage * 15 / 10;
        }
        wazaSetStatus(PC_PTR(moveData), 0, 0x2d, 0, result);
        lbl_80478D78[0]++;
        lbl_8047B610 += 9;
        return;
    }
    if (initialIndex != 0) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
        return;
    }
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 5));
}

#define fn_8012640C pokemonGetStatus
#define fn_8011BEB4 wazaGetStatus
u32 fn_80216CF8(u32 p1, u32 p2, u16 p3, u32 p4, u8 p5) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8011BEB4();
#endif
    s32 acc = fn_8012640C(0, p2, 0x4, 0);
    s32 sub = (u16)fn_8011BEB4(0, p1, 0x7, 0);
    s32 scale = (s32)((p3 << 1) / 5) + 2;
    s32 divisor;

    acc = acc * sub;
    acc = acc * scale;
    divisor = fn_8012640C(0, p4, 0x5, 0);
    if (divisor <= 0) {
        divisor = 1;
    }
    acc = acc / divisor;
    return acc / 50 + 2;
}

#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_8011BEB4 wazaGetStatus
void wsCmdC3(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F0134();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai08();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSeqCalcDamage();
#endif
  u16 uVar7;
  u32 uVar1;
  u32 uVar11;
  u32 uVar2;
  u32 uVar3;
  u32 uVar4;
  u16 uVar10;
  u16 uVar8;
  int iVar6;
  u32 uVar5;
  u8 cVar9;

  uVar7 = fn_801F54A4(0,0,0x14,0);
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = fn_801F0134(uVar1,uVar7);
  uVar11 = (u32)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  uVar4 = fn_80205184((void*)PC_PTR(uVar1));
  uVar10 = fn_8011BEB4(PC_PTR(uVar11),0,0x2f,0);
  uVar8 = fn_8011BEB4(PC_PTR(uVar11),0,0x30,0);
  uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar3));
  cVar9 = fn_802025B8(PC_PTR(uVar3),PC_PTR(0x34));
  if (cVar9 != 2) {
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    return;
  }
  fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar3),PC_PTR(0x34),PC_PTR(uVar2));
  fightOutPokemonSetPokemonJoutai0C(PC_PTR(uVar3),PC_PTR(0x34),uVar4);
  iVar6 = fightSeqCalcDamage(uVar1,uVar3,uVar5,uVar4,uVar10,uVar8);
  cVar9 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x32));
  if (cVar9 == 1) {
    iVar6 = iVar6 * 15 / 10;
  }
  fightOutPokemonSetPokemonJoutai08(PC_PTR(uVar3),PC_PTR(0x34),PC_PTR(iVar6));
  if ((uVar4 & 0xffff) == 0x161) {
    lbl_80478D78[5] = 1;
  }
  else {
    lbl_80478D78[5] = 0;
  }
  lbl_8047B610 = lbl_8047B610 + 5;
  return;
}

void wsCmdC2(void)
{
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
    extern u8 fightOutPokemonCheckFightOut();
#endif
    extern u8 lbl_8047B648;
    extern u8 lbl_8047B649;
  u32 context;
  u32 candidate;
  u32 selected;

  context = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
  lbl_8047B649 = 8;
  lbl_8047B648 = 0;
  selected = 0;
  while (lbl_8047B648 < lbl_8047B649) {
    candidate = fightFloorGetStatus(0,0,0x5d,lbl_8047B648);
    if (candidate != 0 && fightOutPokemonCheckFightOut(PC_PTR(candidate)) != 0 &&
        context != candidate) {
      selected = candidate;
      break;
    }
    lbl_8047B648++;
  }
  if (selected != 0) {
    fightFloorSetStatus(0,0,0x43,0,selected);
  }
  lbl_8047B610++;
  return;
}
