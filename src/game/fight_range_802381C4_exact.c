/**
 * @file fight_range_802381C4_exact.c
 * @brief Exact pure-C fight-engine island, 0x802381C4 - 0x80238B0C.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

extern u8 lbl_80375CA8[];
extern u32 ServerStatusFlag;

u8 fn_802381C4(void *ctx, void *param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
  void *resolved = PC_PTR(pokemonGetStatus(param2, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
  fightFloorGetStatus(0, 0, 0x14, 0);
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  fightPokemonGetPokemonPtr(resolved);
  return (u8)pokemonGetStatus(fightPokemonGetPokemonPtr(resolved), 0, POKEMON_STATUS_MOVE_PP,
                              (u8)param3);
}

u16 fightAiGetWazaId(void *ctx, void *param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
  void *resolved = PC_PTR(pokemonGetStatus(param2, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
  fightFloorGetStatus(0, 0, 0x14, 0);
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  fightPokemonGetPokemonPtr(resolved);
  return (u16)pokemonGetStatus(fightPokemonGetPokemonPtr(resolved), 0, POKEMON_STATUS_MOVE,
                               (u8)param3);
}

u32 fightTrainerAiGetSoubiItem(ctx, param) void *ctx;
u32 param;
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightPokemonGetSoubiItemSoubiDataId(void *ctx);
#endif
  void *resolved = PC_PTR(pokemonGetStatus(PC_PTR(param), 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
  fightFloorGetStatus(0, 0, 0x14, 0);
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return fightPokemonGetSoubiItemSoubiDataId(resolved);
}

u32 fightAiGetSoubiItemOf(void *ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
  extern u32 fightPokemonGetSoubiItemDataId(void *ctx);
  void *resolved = PC_PTR(pokemonGetStatus(PC_PTR(param), 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
  fightFloorGetStatus(0, 0, 0x14, 0);
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return fightPokemonGetSoubiItemDataId(resolved);
}

u16 fn_8023842C(void *ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
  void *resolved = PC_PTR(pokemonGetStatus(PC_PTR(param), 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return (u16)pokemonGetStatus(fightPokemonGetPokemonPtr(resolved), 0, POKEMON_STATUS_FRIENDSHIP, 0);
}

u32 fn_802384B4(void *ctx, void *param2, void *param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightPokemonCheckWriteJoutaiDataId(void *ctx, void *typeObj);
#endif
  u16 val = (u16)fightTrainerGetStatus(
      0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
  if ((u8)fightTrainerGetStatus(0, val, 0x24, 0) == 1) {
    return fightPokemonCheckWriteJoutaiDataId(param2, param3);
  }
  return 0;
}

u8 fightAiCheckSwitchFlag33(void *ctx, void *param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 pokemonGetStatus();
#endif
  u16 val, val2;
  fightFloorGetStatus(0, 0, 0x14, 0);
  val = (u16)fightTrainerGetStatus(
      0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
  val2 = (u16)pokemonGetStatus(fightPokemonGetPokemonPtr(param2), 0, POKEMON_STATUS_SHADOW_CONDITION_SLOT, 0);
  if ((u8)fightTrainerGetStatus(0, val, 0x33, 0) == 1) {
    return (u8)fightTrainerGetStatus(0, val2, 0x1D, 0);
  }
  return 1;
}

u8 fightTrainerAiGetPokemonPartData1C(void *ctx, void *param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 pokemonGetStatus();
#endif
  u16 val, val2;
  fightFloorGetStatus(0, 0, 0x14, 0);
  val = (u16)fightTrainerGetStatus(
      0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
  val2 = (u16)pokemonGetStatus(fightPokemonGetPokemonPtr(param2), 0, POKEMON_STATUS_SHADOW_CONDITION_SLOT, 0);
  if ((u8)fightTrainerGetStatus(0, val, 0x23, 0) == 1) {
    return (u8)fightTrainerGetStatus(0, val2, 0x1C, 0);
  }
  return 0;
}

u32 fn_802386C8(void *ctx, void *param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 pokemonGetStatus();
#endif
  fightFloorGetStatus(0, 0, 0x14, 0);
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return pokemonGetStatus(fightPokemonGetPokemonPtr(param), 0, POKEMON_STATUS_SPEED, 0);
}

u32 fightAiIsJoutaiNormal(void *ctx, void *param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void *fightPokemonGetPokemonPtr(void *ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 pokemonIsJoutaiNormal(u8 * ptr);
#endif
  u16 val = (u16)fightTrainerGetStatus(
      0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
  if ((u8)fightTrainerGetStatus(0, val, 0x24, 0) == 1) {
    return pokemonIsJoutaiNormal(fightPokemonGetPokemonPtr(param));
  }
  return 1;
}

u32 fightAiGetHpPercent(void *ctx, void *param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
  extern u32 fightOutPokemonGetNowHpPercentage(void *ctx);
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return fightOutPokemonGetNowHpPercentage(param);
}

u32 fn_8023881C(u32 r3, u32 r4) {
  u16 statusValues[2];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 pokemonGetStatus();
#endif
  u16 uVar5;
  u16 uVar6;
  u32 uVar1;
  u8 uVar4;
  u8 bVar7;
  uVar5 = fightTrainerGetStatus(PC_PTR(r3), 0, 0x43, 0);
  uVar5 = fightTrainerGetStatus(0, uVar5, 2, 0);
  uVar6 = fightTrainerGetStatus(PC_PTR(r3), 0, 0x43, 0);
  fightTrainerGetStatus(0, uVar6, 2, 0);
  uVar1 = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(r4));
  statusValues[0] = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_HP, 0);
  uVar6 = fightTrainerGetStatus(PC_PTR(r3), 0, 0x43, 0);
  fightTrainerGetStatus(0, uVar6, 2, 0);
  uVar1 = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(r4));
  statusValues[1] = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_MAX_HP, 0);
  uVar4 = fightTrainerGetStatus(0, uVar5, 0x2c, 0);
  bVar7 = (statusValues[0] * 100) / statusValues[1] <= uVar4;
  return bVar7;
}

u32 fightAiGetLevel2(void *ctx, void *param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 figthPokemonGetLevel(u32 ctx);
#endif
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return figthPokemonGetLevel(PC_PTR32(param));
}

u16 fightTrainerAiGetPokemonDataId2(void *ctx, void *param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u16 figthPokemonGetPokemonDataId(void *ctx);
#endif
  fightTrainerGetStatus(0, fightTrainerGetStatus(ctx, 0, 0x43, 0) & 0xFFFF, 0x2,
                        0);
  return figthPokemonGetPokemonDataId(param);
}

int fightTrainerAiGetPokemonAisyouScore(u32 r3, u32 r4) {
  extern int zokuseiGetAisyouScore();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 pokemonGetStatus(u32, u32, u32, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u8 fightTrainerIsAllyFightTargetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightPokemonGetPokemonPtr();
#endif
  u16 uVar5;
  u16 uVar4;
  u16 uVar3;
  u32 uVar1;
  u8 cVar7;
  u32 sVar6;
  int iVar2;
  u32 bVar9;
  int iVar8;
  iVar8 = 0;
  for (bVar9 = 0; (bVar9 & 0xff) < 2; bVar9 = bVar9 + 1) {
    uVar3 = fightFloorGetStatus(0, 0, 0x14, 0);
    uVar4 = fightTrainerGetStatus(0, (u16)fightTrainerGetStatus(PC_PTR(r3), 0, 0x43, 0),
                                  2, 0);
    uVar1 = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(r4));
    uVar5 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_SPECIES, 0);
    cVar7 = fightTrainerGetStatus(0, uVar4, 0x2a, 0);
    if (cVar7 == 1) {
      cVar7 = fightTrainerIsAllyFightTargetPtr(PC_PTR(r3), PC_PTR(r4), uVar3);
      if (cVar7 == 0) {
        sVar6 = pokemonGetStatus(0, uVar5, POKEMON_STATUS_SPECIES_TYPE, bVar9) & 0xffff;
      } else {
        sVar6 = pokemonGetStatus(0, uVar5, POKEMON_STATUS_SPECIES_TYPE, bVar9) & 0xffff;
      }
    } else {
      sVar6 = 9;
    }
    if ((u16)sVar6 != 9) {
      iVar2 = zokuseiGetAisyouScore(sVar6, 0);
      iVar8 = iVar8 + iVar2;
    }
  }
  return iVar8;
}
