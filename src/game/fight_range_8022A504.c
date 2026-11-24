/**
 * @file fight_range_8022A504.c
 * @brief Exact pure-C fight island, 0x8022A504 - 0x8022B29C (2 fns).
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
extern void fightGetRandomOpponent(s32);

u32 _fightCheckYokodoriSub(u32 r3, u32 r4, u32* r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern void* lbl_8047B62C;
    extern u8 lbl_8037919D[];
    void* context;
    u32* state;
    u32 pokemon;
    u8 moveSlot;
    s32 slot;
    u32 iVar7;
    u32 uVar1;
    u8 cVar4;
    u8 cVar5;
    u16 sVar3;
    s8 bVar6;
    u32 uVar2;

    context = (void*)PC_PTR(r3);
    state = r5;
    iVar7 = *state;
    uVar1 = fightOutPokemonGetUseWazaDataId(PC_PTR(iVar7));
    cVar4 = wazaGetStatus(0, uVar1, 0x10, 0);
    cVar5 = fightOutPokemonCheckFightOut(context);
    if (cVar5 == 0) {
        return 1;
    }
    cVar5 = fightOutPokemonIsPokemonJoutaiDataId(context, PC_PTR(0x33));
    if ((cVar5 == 1) && (cVar4 == 1)) {
        if ((iVar7 != 0) && (context != 0) &&
            (sVar3 = fightOutPokemonGetTokuseiDataId(PC_PTR(iVar7)), sVar3 == ABILITY_PRESSURE)) {
            pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(context));
            bVar6 = pokemonSearchWazaDataId(PC_PTR(pokemon), 0x121);
            if (bVar6 >= 0) {
                slot = bVar6;
                cVar4 = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slot);
                if (cVar4 != 0) {
                    cVar4--;
                }
                pokemonSetStatus((void*)PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slot, cVar4);
                moveSlot = bVar6;
                cVar5 = fightOutPokemonIsPokemonJoutaiDataId(context, PC_PTR(0x10));
                if ((cVar5 == 0) &&
                    (cVar5 = fightOutPokemonIsPokemonJoutaiDataId(context, PC_PTR(0x31)), cVar5 == 1) &&
                    (uVar2 = fightOutPokemonGetPokemonJoutai08(context, 0x31),
                     (uVar2 & (1 << (u32)moveSlot)) == 0) &&
                    (cVar5 = fightOutPokemonIsUseHensinBuff(context), cVar5 == 1)) {
                    fightOutPokemonSetHensinPokemonStatusId(context, 0x80, moveSlot, 0);
                }
            }
        }
        fightOutPokemonWriteJoutaiDataId(context, PC_PTR(0x33));
        fightFloorSetStatus(0, 0, 0x4b, 0, PC_PTR32_IMPLICIT(context));
        wsRunScript(lbl_8047B62C, lbl_8037919D, 0);
        state[1] = PC_PTR32(context);
    }
    return 1;
}
#define fn_800FA280 GSmsgGetGSchar
#define fn_8011BBD8 wazaSetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_8011CE18 pokemonSeikakuDataBiosGetPtr
#define fn_8011FC74 pokemonIsDarkPokemon
#define fn_80123CD4 pokemonWazaCheckValid
#define fn_801254B4 pokemonSetStatus
#define fn_8012640C pokemonGetStatus
#define fn_80132A38 msgctrlSetValue
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4354 fightFloorGetFightOutPokemonPtrToFightTrainerPtr
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F8100 fightTrainerGetNamePtr
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_801FFEC8 fightOutPokemonCheckCanOutOkWazaBanme
#define fn_80203E0C figthOutPokemonGetLevel
#define fn_80203FE4 fightOutPokemonGetSoubiItemSoubiDataId
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80208404 fightOutPokemonDarkPokemonEffect
#define fn_802085C4 fightOutPokemonWazaEffect
#define fn_802086E8 fightOutPokemonFreeWazaEffect
#define fn_80208750 fightOutPokemonLoadWazaEffect
#define fn_80208ED0 fightOutPokemonModosuEffect
#define fn_802099AC fightWazaCreate
#define fn_8026246C fightMenuCloseMsg
#define fn_802624CC fightMenuOpenMsg
/* RULE-EXCEPTION(user-approved): opt_lifetimes off for _wsHitCheckSub only; the unit builds with -O4,s — see docs/RULE_EXCEPTIONS.md */
#pragma opt_lifetimes off
u32 _wsHitCheckSub(u32 r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800FA280();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
    extern u32 fn_8011CC54();
    extern u32 fn_8011CC6C();
    extern u32 fn_8011CC84();
    extern u32 fn_8011CC9C();
    extern u32 fn_8011CCB4();
    extern u32 fn_8011CCCC();
    extern u32 fn_8011CCE4();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* fn_8011CE18();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011FC74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80123CD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80142984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F0134();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F4354();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F8100();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTrainerGetIrekaeFightPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FB1C0();
#endif
    extern void fightTrainerWazaSeq74();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FFEC8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80203E0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80203FE4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetSoubiItemDataId();
#endif
    extern void fn_80208404();
    extern void fn_80208554();
    extern void fn_802085C4();
    extern void fn_802086E8();
    extern void fn_80208750();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80208ED0();
#endif
    extern void fn_802099AC(u32, s8, u16, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSeqSelectTarget();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonDoSoubiItemCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSeqCalcDamage();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8026246C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802624CC();
#endif
    extern u16 lbl_8047B60C;
    extern u32 ServerStatusFlag;
    extern u8 lbl_803799ED[];
    extern u8 lbl_803799EF[];
    extern u8 lbl_803799F4[];
    extern u8 lbl_803799FE[];
  u8 bVar1;
  u16 uVar11;
  int iVar5;
  u32 uVar6;
  u8 uVar14;
  s32 uVar17;
  u32 uVar3;
  u32 uVar4;
  u32 uVar7;
  u32 uVar2;
  u32 uVarMove;
  u32 activeMove;
  u32 uVar8;
  u8 cVar15;
  u32 uVar9;
  u32 uVar10;
  u16 local_48;
  short local_44[4];

  uVar11 = fn_801F54A4(0,0,0x14,0);
  uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r3));
  uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(2,r3));
  uVar4 = (int)fn_8012640C(PC_PTR(r3),0,0xd9,0);
  fn_80203E0C(PC_PTR(r3));
  iVar5 = PC_PTR32_IMPLICIT(fn_801F4354(0,r3));
  if (iVar5 == 0) {
    return 0;
  }
  uVar6 = PC_PTR32_IMPLICIT(fn_801F025C(9,iVar5));
  fn_801FB1C0(PC_PTR(iVar5),0,0x44,0);
  activeMove = 0;
  activeMove += fn_80205184((void*)PC_PTR(r3));
  uVar7 = fightOutPokemonGetSoubiItemDataId(PC_PTR(r3));
  fn_80203FE4(PC_PTR(r3));
  uVar8 = PC_PTR32_IMPLICIT(fn_80205B8C(r3));
  uVar14 = (int)fn_8012640C(PC_PTR(uVar8),0,0xbf,0);
  cVar15 = (u8)fn_8011BEB4(PC_PTR(uVar4),0,0x32,0);
  if ((cVar15 != 1) && ((u8)fn_801F54A4(0,0,0x31,0) != 0)) {

    cVar15 = (u8)fn_8011FC74(fn_80205B8C(PC_PTR(r3)));
    if ((cVar15 != 0) && ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3e)) != 0) &&
        ((u16)activeMove != 0x164)) {
      fn_80132A38(0x13,PC_PTR32_IMPLICIT(fn_801F8100(PC_PTR(iVar5))));
      uVar17 = GSrand() % 100;
      /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
      uVarMove = fn_8011CCE4(fn_8011CE18(uVar14));
      if (uVar17 < (uVarMove & 0xff)) {
        u8 initIndex;
        u32 bVar16;
        u32 sVar12;
        for (initIndex = 0; initIndex < 4; initIndex++) {
          local_44[initIndex] = -1;
        }
        for (bVar16 = 0, sVar12 = 0; (s16)sVar12 < 4; sVar12 = sVar12 + 1) {
          u16 moveId;
          activeMove = sVar12 & 0xffff;
          cVar15 = fn_80123CD4(PC_PTR(uVar2),activeMove);
          if ((((cVar15 != 0) && (cVar15 = fn_801FFEC8(PC_PTR(r3),activeMove,0,0), cVar15 == 0)) &&
              (moveId = (int)fn_8012640C(PC_PTR(uVar2),0,0x7f,activeMove), moveId != 0)) &&
             ((moveId != 0x165 && (moveId != 0x163)))) {
            local_44[(u8)bVar16] = sVar12;
            bVar16 = bVar16 + 1;
          }
        }
        if ((u8)bVar16 != 0) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
          extern u32 fn_8012640C(u32, u32, u32, u16);
#endif
          u32 randomValue;
          s32 quotient;
          short selectedSlot;
          randomValue = GSrand();
          quotient = (s32)(randomValue & 0xffff) / (u8)bVar16;
          selectedSlot = *(short *)PC_PTR(((int)PC_PTR32(local_44) +
                                   (((randomValue & 0xffff) - quotient * (u8)bVar16)
                                    * 2 & 0x1fe)));
          if (((selectedSlot >= 0) && (uVar2 = (u16)fn_8012640C(PC_PTR(uVar2),0,0x7f,selectedSlot), (u16)uVar2 != 0)) &&
             (((u16)uVar2 != 0x165 && ((u16)uVar2 != 0x163)))) {
            u32 targetOutPokemon;
            u32 flags;
            u32 relativeTarget;
            lbl_8047B60C = (u16)uVar2;
            flags = ServerStatusFlag & 0xfffffbff;
            *(volatile u32*)&ServerStatusFlag = flags;
            *(volatile u32*)&ServerStatusFlag = flags | 0x200000;
            targetOutPokemon = fightSeqSelectTarget(r3,(u16)uVar2,uVar11,PC_PTR32(fightGetRandomOpponent),1,1, PC_PTR32_IMPLICIT((void*)0xffffffff));
            relativeTarget = fn_801F0134(targetOutPokemon, uVar11);
            fn_802099AC(uVar4, selectedSlot, (u16)uVar2, relativeTarget, 0);
            fn_801F4C14(0,0,0x43,0,targetOutPokemon);

            fn_80132A38(0x28,PC_PTR32_IMPLICIT(fn_800FA280(fn_8011BEB4(0,(u16)uVar2,1,0))));
            fn_80208404(r3,0,1,0);
            fn_80208404(r3,0,2,0);
            fn_80208404(r3,0,1,1);
            fn_80208404(r3,0,1,2);
            fn_80208404(r3,0,2,1);
            fn_802624CC(0x770c);
            fn_80208404(r3,0,2,2);
            fn_8026246C();
            fn_80208404(r3,0,1,3);
            fn_80208404(r3,0,2,3);
            ServerStatusFlag = ServerStatusFlag | 0x400;
            lbl_8047B610 = lbl_803799ED;
            return 2;
          }
        }
        goto fallback_7710;
      }
      else {
        /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
        uVar10 = fn_8011CCCC(fn_8011CE18(uVar14));
        uVarMove = (uVarMove & 0xff) + (uVar10 & 0xff);
        if (uVar17 < uVarMove) {
          fn_80208404(r3,0,1,0);
          fn_80208750(r3,1,1,0);
          fightTrainerWazaSeq74(uVar6,0);
          fn_80208404(r3,0,1,1);
          fn_80208404(r3,0,1,2);
          fn_802085C4(r3,1,1,0, (void*)0xffffffff);
          fn_802624CC(0x770d);
          fn_80208554(r3,1,1,6);
          fn_8026246C();
          fightTrainerWazaSeq74(uVar6,1);
          fightTrainerWazaSeq74(uVar6,2);
          fn_80208404(r3,0,1,3);
          fn_802086E8(r3,1,1);
          fightTrainerWazaSeq74(uVar6,3);
          lbl_8047B610 = lbl_803799F4;
          return 1;
        }
        /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
        uVar10 = fn_8011CCB4(fn_8011CE18(uVar14));
        uVarMove = uVarMove + (uVar10 & 0xff);
        if (uVar17 < uVarMove) {
          fn_80208404(r3,0,1,0);
          fn_80208750(r3,1,1,0);
          fightTrainerWazaSeq74(iVar5,0);
          fn_80208404(r3,0,1,1);
          fn_80208404(r3,0,1,2);
          fn_802085C4(r3,1,1,0, (void*)0xffffffff);
          fn_802624CC(0x770e);
          fn_80208554(r3,1,1,6);
          fn_8026246C();
          fightTrainerWazaSeq74(iVar5,1);
          fightTrainerWazaSeq74(iVar5,2);
          fn_80208404(r3,0,1,3);
          fn_802086E8(r3,1,1);
          fightTrainerWazaSeq74(iVar5,3);
          lbl_8047B610 = lbl_803799F4;
          return 1;
        }
        /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
        uVar10 = fn_8011CC9C(fn_8011CE18(uVar14));
        uVarMove = uVarMove + (uVar10 & 0xff);
        if (uVar17 < uVarMove) {
          fn_80208404(r3,0,1,0);
          fn_80208404(r3,0,2,0);
          fn_801F4C14(0,0,0x43,0,r3);
          uVar10 = fightSeqCalcDamage(r3,r3,uVar3,1,0x28, PC_PTR32_IMPLICIT((void*)0xffffffff));
          fn_8011BBD8(PC_PTR(uVar4),0,0x2d,0,uVar10);
          fn_801254B4((void*)PC_PTR(r3),0,0x107,0,1);
          ServerStatusFlag = ServerStatusFlag | 0x80000;
          fn_80208404(r3,0,1,1);
          fn_80208404(r3,0,1,2);
          fn_80208404(r3,0,2,1);
          fn_802624CC(0x770f);
          fn_80208404(r3,0,2,2);
          fn_8026246C();
          fn_80208404(r3,0,1,3);
          fn_80208404(r3,0,2,3);
          lbl_8047B610 = lbl_803799EF;
          return 2;
        }
        /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
        uVar10 = fn_8011CC84(fn_8011CE18(uVar14));
        uVarMove = uVarMove + (uVar10 & 0xff);
        if (uVar17 >= uVarMove) {
          goto after_fallback_7710;
        }
fallback_7710:
        fn_80208404(r3,0,1,0);
        fn_80208750(r3,0x85,3,0);
        fn_80208404(r3,0,1,1);
        fn_80208404(r3,0,1,2);
        fn_802085C4(r3,0x85,3,0, (void*)0xffffffff);
        fn_802624CC(0x7710);
        fn_80208554(r3,0x85,3,6);
        fn_8026246C();
        fn_80208404(r3,0,1,3);
        fn_802086E8(r3,0x85,3);
        lbl_8047B610 = lbl_803799F4;
        return 1;
after_fallback_7710:
        /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
        uVar10 = fn_8011CC6C(fn_8011CE18(uVar14));
        uVarMove = uVarMove + (uVar10 & 0xff);
        if (uVar17 < uVarMove) {
          fn_80208404(r3,0,1,0);
          fn_80208750(r3,0x85,3,0);
          fn_80208404(r3,0,1,1);
          fn_80208404(r3,0,1,2);
          cVar15 = fn_80142984(uVar7);
          if (cVar15 == 0) {
            fn_802085C4(r3,0x85,3,0, (void*)0xffffffff);
            fn_802624CC(0x7712);
            fn_80208554(r3,0x85,3,6);
            fn_8026246C();
          }
          else {
            uVar9 = uVar7 & 0xffff;
            if ((((uVar9 == 0x2c) || ((uVar7 - 0x85 & 0xffff) <= 0xe)) ||
                ((uVar7 - 0xa8 & 0xffff) <= 6)) || ((uVar9 == 0xb4 || (uVar9 == 0xb9)))) {
              bVar1 = 1;
            }
            else {
              bVar1 = 0;
            }
            if ((bVar1 == 1) && (cVar15 = fightOutPokemonDoSoubiItemCheck(r3,2), cVar15 != 0)) {
              fn_80208404(r3,0,1,3);
              fn_802086E8(r3,0x85,3);
              lbl_8047B610 = lbl_803799F4;
              return 1;
            }
            fn_802085C4(r3,0x85,3,0, (void*)0xffffffff);
            fn_802624CC(0x7711);
            fn_80208554(r3,0x85,3,6);
            fn_8026246C();
          }
          fn_80208404(r3,0,1,3);
          fn_802086E8(r3,0x85,3);
          lbl_8047B610 = lbl_803799F4;
          return 1;
        }
        /* the nature record (pokemonSeikakuDataBiosGetPtr), r3 into the getter (assembly) */
        uVar7 = fn_8011CC54(fn_8011CE18(uVar14));
        if (uVar17 < uVarMove + (uVar7 & 0xff)) {
          fn_80208404(r3,0,1,0);
          fn_80208404(r3,0,0,0);
          fn_80208ED0(PC_PTR(r3),0);
          fn_80208404(r3,0,1,1);
          fn_80208404(r3,0,1,2);
          local_48 = 0;
          uVar8 = PC_PTR32_IMPLICIT(fightTrainerGetIrekaeFightPokemon(PC_PTR(iVar5),&local_48));
          if (uVar8 != 0) {
            fn_80208ED0(PC_PTR(r3),1);
            fn_802624CC(0x7713);
            fn_80208ED0(PC_PTR(r3),2);
            fn_8026246C();
            fn_80208404(r3,0,1,3);
            fn_80208404(r3,0,0,3);
            fn_80208ED0(PC_PTR(r3),3);
            fn_80208ED0(PC_PTR(r3),4);
            lbl_8047B610 = lbl_803799FE;
            return 1;
          }
          fn_80208404(r3,0,0,1);
          fn_802624CC(0x7714);
          fn_80208404(r3,0,0,2);
          fn_8026246C();
          fn_80208404(r3,0,1,3);
          fn_80208404(r3,0,0,3);
          fn_80208ED0(PC_PTR(r3),3);
          lbl_8047B610 = lbl_803799F4;
          return 1;
        }
      }
    }
  }
  return 0;
}
#pragma opt_lifetimes reset
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_800FA280
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011CE18
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011FC74
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80123CD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F0134
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4354
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F8100
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FFEC8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203E0C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203FE4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80208404
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802085C4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802086E8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80208750
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80208ED0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802099AC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8026246C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802624CC
#endif

