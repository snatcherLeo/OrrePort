/**
 * @file fight_range_exact_80228DAC.c
 * @brief Exact pure-C fight status-check island, 0x80228DAC - 0x80229704.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/conditions.h"

extern u8* lbl_8047B610;
extern u32 menuCBRule_ProhibitionSuicide(void);

#pragma opt_propagation reset
#define fn_8011BEB4 wazaGetStatus
#define fn_801254B4 pokemonSetStatus
#define fn_8012640C pokemonGetStatus
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F453C fightFloorGetNowTenkouDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F54A4 fightFloorGetStatus
#define fn_802096E8 fightWazaIsHit
u32 _fightCheckAccuracySpecial(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8011BEB4();
    extern s32 fn_801F453C(void* param_1, u32 param_2);
    extern u16 fn_801F0134();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802096E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightCheckTokuseiWazaKyuushuu();
#endif
    extern u32 ServerStatusFlag;
  u8 bVar1;
  u16 uVar8;
  u16 sVar9;
  u32 lateMoveType;
  u8 cVar11;
  u16 sVar10;
  u32 uVar2;
  u32 uVar3;
  u32 uVar4;
  u32 uVar5;
  u32 uVar6;
  u32 uVar7;

  uVar8 = fn_801F54A4(0,0,0x14,0);
  fn_8011BEB4(0,r5,9,0);
  fn_801F453C(0,1);
  sVar9 = fn_801F0134(r3,uVar8);
  cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_LOCK_ON));
  if (cVar11 != 1) {
    goto first_check_done;
  }
  sVar10 = fightOutPokemonGetJoutaiParam(PC_PTR(r4),PC_PTR(0x1d));
  if (sVar10 == sVar9) {
    void* savedAttacker;
    savedAttacker = (void*)fn_801F025C(0x11,0);
    uVar3 = (0,(int)fn_8012640C(savedAttacker,0,0xd9,0));
    uVar4 = PC_PTR32_IMPLICIT((0,fn_801F025C(0x12,0)));
    cVar11 = fn_802096E8(PC_PTR(uVar3));
    if (cVar11 == 0) {
    fn_801254B4((void*)PC_PTR(uVar4),0,0xf3,0,0);
    fn_801254B4((void*)PC_PTR(uVar4),0,0xf4,0,9);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
    else {
      uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,PC_PTR32_IMPLICIT(fn_801F025C(0x11,0))));
      uVar2 = 0;
      uVar2 += PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
      uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar2));
      cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar2),PC_PTR(0x15));
      if (((cVar11 == 1) && (uVar5 != uVar3)) &&
         ((ServerStatusFlag & 0x1000000) == 0)) {
        ServerStatusFlag = ServerStatusFlag | 0x40;
      }
      cVar11 = fightCheckTokuseiWazaKyuushuu(PC_PTR32(savedAttacker),uVar4,r5);
      if (cVar11 == 0) {
        lbl_8047B610 = lbl_8047B610 + 7;
      }
    }
    return 1;
  }
first_check_done:
  if (((ServerStatusFlag & 0x10000) == 0) &&
     (cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_IN_THE_AIR)), cVar11 == 1)) {
    fn_801F4C14(0,0,0x3b,0,0x40);
    uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
    uVar3 = (int)fn_8012640C(PC_PTR(uVar2),0,0xd9,0);
    uVar4 = 0;
    uVar4 += PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
    cVar11 = fn_802096E8(PC_PTR(uVar3));
    if (cVar11 == 0) {
    fn_801254B4((void*)PC_PTR(uVar4),0,0xf3,0,0);
    fn_801254B4((void*)PC_PTR(uVar4),0,0xf4,0,9);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
    else {
      uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
      uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar3));
      uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
      uVar6 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar3));
      cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar3),PC_PTR(0x15));
      if (((cVar11 == 1) && (uVar5 != uVar6)) &&
         ((ServerStatusFlag & 0x1000000) == 0)) {
        ServerStatusFlag = ServerStatusFlag | 0x40;
      }
      cVar11 = fightCheckTokuseiWazaKyuushuu(uVar2,uVar4,r5);
      if (cVar11 == 0) {
        lbl_8047B610 = lbl_8047B610 + 7;
      }
    }
    return 1;
  }
  uVar7 = ServerStatusFlag;
  ServerStatusFlag = uVar7 & 0xfffeffff;
  if (((uVar7 & 0x20000) == 0) && (cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_UNDERGROUND)), cVar11 == 1)) {
    fn_801F4C14(0,0,0x3b,0,0x40);
    uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
    uVar4 = (int)fn_8012640C(PC_PTR(uVar3),0,0xd9,0);
    uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
    cVar11 = fn_802096E8(PC_PTR(uVar4));
    if (cVar11 == 0) {
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf3,0,0);
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf4,0,9);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
    else {
      uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
      uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
      uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
      uVar6 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
      cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(0x15));
      if (((cVar11 == 1) && (uVar5 != uVar6)) &&
         ((ServerStatusFlag & 0x1000000) == 0)) {
        ServerStatusFlag = ServerStatusFlag | 0x40;
      }
      cVar11 = fightCheckTokuseiWazaKyuushuu(uVar3,uVar2,r5);
      if (cVar11 == 0) {
        lbl_8047B610 = lbl_8047B610 + 7;
      }
    }
    return 1;
  }
  uVar7 = ServerStatusFlag;
  ServerStatusFlag = uVar7 & 0xfffdffff;
  if (((uVar7 & 0x40000) == 0) && (cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_UNDERWATER)), cVar11 == 1)) {
    fn_801F4C14(0,0,0x3b,0,0x40);
    uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
    uVar4 = (int)fn_8012640C(PC_PTR(uVar3),0,0xd9,0);
    uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
    cVar11 = fn_802096E8(PC_PTR(uVar4));
    if (cVar11 == 0) {
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf3,0,0);
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf4,0,9);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
    else {
      uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
      uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
      uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
      uVar6 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
      cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(0x15));
      if (((cVar11 == 1) && (uVar5 != uVar6)) &&
         ((ServerStatusFlag & 0x1000000) == 0)) {
        ServerStatusFlag = ServerStatusFlag | 0x40;
      }
      cVar11 = fightCheckTokuseiWazaKyuushuu(uVar3,uVar2,r5);
      if (cVar11 == 0) {
        lbl_8047B610 = lbl_8047B610 + 7;
      }
    }
    return 1;
  }
  ServerStatusFlag = ServerStatusFlag & 0xfffbffff;
  lateMoveType = fn_8011BEB4(0,r5,9,0);
  lateMoveType = (u16)lateMoveType;
  cVar11 = (int)fn_801F453C(0,1);
  if ((cVar11 == 2) && (lateMoveType == 0x98)) {
    bVar1 = 1;
  }
  else {
    bVar1 = 0;
  }
  if (bVar1 == 1) {
    uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
    uVar4 = (int)fn_8012640C(PC_PTR(uVar3),0,0xd9,0);
    uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
    cVar11 = fn_802096E8(PC_PTR(uVar4));
    if (cVar11 == 0) {
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf3,0,0);
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf4,0,9);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
    else {
      uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
      uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
      uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
      uVar6 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
      cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(0x15));
      if (((cVar11 == 1) && (uVar5 != uVar6)) &&
         ((ServerStatusFlag & 0x1000000) == 0)) {
        ServerStatusFlag = ServerStatusFlag | 0x40;
      }
      cVar11 = fightCheckTokuseiWazaKyuushuu(uVar3,uVar2,r5);
      if (cVar11 == 0) {
        lbl_8047B610 = lbl_8047B610 + 7;
      }
    }
    uVar2 = 1;
  }
  else {
    lateMoveType = fn_8011BEB4(0,r5,9,0);
    if ((lateMoveType == 0x11) || (lateMoveType == 0x4e)) {
      bVar1 = 1;
    }
    else {
      bVar1 = 0;
    }
    if (bVar1 == 1) {
      uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
      uVar4 = (int)fn_8012640C(PC_PTR(uVar3),0,0xd9,0);
      uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
      cVar11 = fn_802096E8(PC_PTR(uVar4));
      if (cVar11 == 0) {
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf3,0,0);
    fn_801254B4((void*)PC_PTR(uVar2),0,0xf4,0,9);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
      }
      else {
        uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
        uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
        uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
        uVar6 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar4));
        cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(0x15));
        if (((cVar11 == 1) && (uVar5 != uVar6)) &&
           ((ServerStatusFlag & 0x1000000) == 0)) {
          ServerStatusFlag = ServerStatusFlag | 0x40;
        }
        cVar11 = fightCheckTokuseiWazaKyuushuu(uVar3,uVar2,r5);
        if (cVar11 == 0) {
          lbl_8047B610 = lbl_8047B610 + 7;
        }
      }
      uVar2 = 1;
    }
    else {
      uVar2 = 0;
    }
  }
  return uVar2;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F0134
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F453C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802096E8
#endif

#define fn_801F54A4 fightFloorGetStatus
#define fn_8011BEB4 wazaGetStatus
u8 fn_8022967C(u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F54A4();
#endif
    u8 a = (u8)fn_801F54A4(0, 0, 0x34, 0);
    u32 b = menuCBRule_ProhibitionSuicide();
    u16 c = (u16)fn_8011BEB4(0, param2, 9, 0);

    if (a == 1 && (u8)b == 1 && c == 7) {
        return 1;
    }
    return 0;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
