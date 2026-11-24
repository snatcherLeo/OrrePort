/**
 * @file fight_range_8021A338.c
 * @brief Fight sequence carve, 0x8021A338 - 0x8021A6CC (wsCmd9C,
 *        wsCmd9B), linked.
 *
 * Built with GC/1.3.2 at -O4,s (configure.py). Under GC/1.3 the scheduler
 * issues the flag-byte store ahead of the ori/addi that feeds the following
 * word store, which the old candidate imitated with duplicate stores and
 * per-function pragmas; GC/1.3.2 schedules the plain source as retail does.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
extern u8 lbl_80478D78[1];
extern u32 ServerStatusFlag;

void wsCmd9C(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai08();
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
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonMaxHpWaruValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    extern u8 lbl_80478D78[1];
    extern u32 ServerStatusFlag;
  u32 attacker;
  u32 move;
  u32 pokemon;
  u16 hp;
  u16 amount;

  attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
  pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
  hp = pokemonGetStatus(PC_PTR(pokemon),0,POKEMON_STATUS_HP,0);
  move = pokemonGetStatus(PC_PTR(attacker),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
  amount = fightOutPokemonMaxHpWaruValue(PC_PTR(attacker),4);
  if ((s32)hp > (s32)amount) {
    goto check_status;
  }
failed:
  wazaSetStatus(PC_PTR(move),0,0x2d,0,0);
  lbl_80478D78[5] = 1;
  goto done;
check_status:
  if (fn_802025B8(PC_PTR(attacker),PC_PTR(0x14)) != 2) {
    goto failed;
  }
  wazaSetStatus(PC_PTR(move),0,0x2d,0,amount);
  fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(attacker),PC_PTR(0x14),0);
  fightOutPokemonSetPokemonJoutai08(PC_PTR(attacker),PC_PTR(0x14),PC_PTR(amount));
  if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker),PC_PTR(0xe)) == 1) {
    fightOutPokemonWriteJoutaiDataId(PC_PTR(attacker),PC_PTR(0xe));
  }
  lbl_80478D78[5] = 0;
  ServerStatusFlag |= 0x100;
done:
  lbl_8047B610++;
  return;
}

void wsCmd9B(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsDarkPokemon(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
    extern void fightOutPokemonCopyHensinStatus();
    extern u8 fightOutPokemonUseHensinBuff();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonIsJoutaiKie();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai08();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai0C(u32, u32, u16);
#endif
    extern void fightOutPokemonSetPokemonJoutaiKaisuu();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80201FDC(u32, u32, s8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
    extern u32 fightOutPokemonGetNamePtr();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
  u32 uVar1;
  u32 uVar2;
  u32 uVar3;
  u32 uVar4;
  u32 uVar5;
  u32 uVar6;
  u8 cVar9;
  u16 sVar8;
  u32 *puVar7;
  u8 lowByte;

  uVar1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
  uVar2 = pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
  uVar3 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12,0));
  uVar4 = pokemonGetStatus(fightOutPokemonGetPokemonPtr(PC_PTR(uVar3)),0,POKEMON_STATUS_PERSONALITY,0);
  uVar5 = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(uVar3));
  uVar6 = pokemonGetStatus(PC_PTR(uVar5),0,POKEMON_STATUS_OT_ID,0);
  wazaSetStatus(PC_PTR(uVar2),0,0x27,0,0xffff);
  cVar9 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar3),PC_PTR(0x10));
  if (cVar9 == 1) {
    goto failed;
  }
  sVar8 = fightOutPokemonIsJoutaiKie(PC_PTR(uVar3));
  if (sVar8 != 0) {
    goto failed;
  }
  cVar9 = pokemonIsDarkPokemon(PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(uVar3)));
  if (cVar9 != 1) {
    goto check_hensin;
  }

failed:
  fightFloorSetStatus(0,0,0x3b,0,0x45);
  lbl_80478D78[5] = 1;
  lbl_8047B610++;
  goto done;

check_hensin:
  cVar9 = fightOutPokemonIsUseHensinBuff(PC_PTR(uVar1));
  if (cVar9 != 0) {
    goto apply_hensin;
  }
  cVar9 = fightOutPokemonUseHensinBuff(uVar1);
  if (cVar9 == 0) {
    goto failed;
  }

apply_hensin:
  cVar9 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x10));
  if (cVar9 == 0) {
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x10),0);
  }
  fightOutPokemonSetPokemonJoutai08(PC_PTR(uVar1),PC_PTR(0x10),PC_PTR(uVar4));
  lowByte = (u8)uVar6;
  fightOutPokemonSetPokemonJoutaiKaisuu(uVar1,0x10,(s8)lowByte);
  fn_80201FDC(PC_PTR(uVar1),PC_PTR(0x10),(s8)(u8)(uVar6 >> 8));
  fightOutPokemonSetPokemonJoutai0C(PC_PTR(uVar1),PC_PTR(0x10),uVar6 >> 0x10);
  cVar9 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x29));
  if (cVar9 == 1) {
    fightOutPokemonWriteJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x29));
  }
  cVar9 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x31));
  if (cVar9 == 1) {
    fightOutPokemonWriteJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x31));
  }
  msgctrlSetValue(0xd,fightOutPokemonGetNamePtr(uVar3));
  lbl_80478D78[5] = 0;
  fightOutPokemonCopyHensinStatus(uVar3,uVar1);
  puVar7 = (u32 *)PC_PTR(pokemonGetStatus(PC_PTR(uVar1),0,0x101,0));
  if (puVar7 != (void *)0) {
    *puVar7 = 0;
  }
  lbl_8047B610 = lbl_8047B610 + 1;
done:
  return;
}
