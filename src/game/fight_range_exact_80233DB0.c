/**
 * @file fight_range_exact_80233DB0.c
 * @brief Strict target-order fight island, 0x80233DB0 - 0x80234A0C.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"

typedef struct FightSeqOpU8Operand {
    u8 opcode;
    u8 operand;
} FightSeqOpU8Operand;
typedef struct { u16 v[11]; } SpecialMoveList22;
typedef struct { u16 a, b; } U16Pair;

#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
extern f32 lbl_8047E630;
#endif
extern u8 lbl_80378724[];
extern u8 lbl_80378B30[];
extern u8 lbl_80378B5B[];
extern u8 lbl_80378A5F[];
extern u8 lbl_80378968[];
extern u8 lbl_80378A4D[];
extern u8 lbl_80378A7C[];
extern u8 lbl_80378A8E[];
extern u32 fn_80232024();
extern void fn_80234A0C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif
extern u8* fn_801440A0(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* itemDataBiosGetPtr(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   fn_80143DFC(u8* p);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   itemDataBiosGetItemEffectParam(u8* p);
#endif
extern u8*  fn_80143A94(u8 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8*  itemParamGetPtr(u8 idx);
#endif
extern u8   fn_801437E0(u8* p);
extern u8   itemParamGetHPUp(u8* p);
extern u8   fn_80143940(u8* p);
extern u8   itemParamGetSleepFlag(u8* p);
extern u8   fn_80143918(u8* p);
extern u8   itemParamGetPoisonFlag(u8* p);
extern u8   fn_801438F0(u8* p);
extern u8   itemParamGetBurnFlag(u8* p);
extern u8   fn_801438C8(u8* p);
extern u8   itemParamGetFreezeFlag(u8* p);
extern u8   fn_801438A0(u8* p);
extern u8   itemParamGetParalyzeFlag(u8* p);
extern u8   fn_80143878(u8* p);
extern u8   itemParamGetConfuseFlag(u8* p);
extern u8   fn_80143A44(u8* p);
extern u8   itemParamGetCriticalFlag(u8* p);
extern u8   fn_80143A28(u8* p);
extern u8   itemParamGetAttackUp(u8* p);
extern u8   fn_80143A0C(u8* p);
extern u8   itemParamGetDefenceUp(u8* p);
extern u8   fn_801439F0(u8* p);
extern u8   itemParamGetQuickUp(u8* p);
extern u8   fn_801439D4(u8* p);
extern u8   itemParamGetHitUp(u8* p);
extern u8   fn_801439B8(u8* p);
extern u8   itemParamGetSpAttackUp(u8* p);
extern u8   fn_80143990(u8* p);
extern u8   itemParamGetGuardFlag(u8* p);
extern u8* lbl_8047B610;
extern u8  lbl_8047B614;
extern u8  lbl_8047B626;
extern void fn_80207448(void* p);
extern void fightOutPokemonInitOneSelfTurn(void* p);
extern void fightSeqSetTsuikaKouka();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightApplyDamageVariance(char, char);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightSeqWazaTypeCalc(u32, char, char, char);
#endif
extern void* fn_801F025C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_802624CC();
#endif
extern u8 lbl_80478D78[1];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801F37B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _fightDoTraceSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 _fightSeqWsKuroikiriSub__FPvUsPv(void*, u16, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8  fn_80136468();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8  fn_802025B8();
#endif
extern u32 _wsAnnounceEnemyTokuseiSub();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fn_801F000C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fightMainWaitFrame();
#endif
extern void fn_8026246C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightMenuCloseMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801F2598();
#endif
extern u8 lbl_80379F58[];
extern void fn_80201600();
extern void* lbl_8047B64C;
extern u8    lbl_80478278[0x10];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  wazaSequenceOwnerClearState(void* obj, u32 val);
#endif
extern void  fn_80209484(void* ctx, u32 param);
extern u16   fn_8020147C(void* context, u16 moveId, u8 slot, u8 updateFlag);
extern void  fn_801FE468(void* context, u8* dest);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fightMenuSubMenuLvupStatus(void* a, void* b, void* c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fightMenuOpenLevelUpStatusMenu(void* a, u32 flag);
#endif
extern void  fn_802622E4(void);
extern u32 _fightCheckKiaipanchiSub();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceSysFreeAll();
#endif
extern u8   lbl_8047B625;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  wsRunScript(void* a, void* b, u8 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fn_80207AE0(void* obj, u8 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightFloorGetFightOutPokemonPtrAryPokemonTokuseiDataIdFirst(u32 a, u8 b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_801254B4(void* a, u32 b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fn_80205B8C(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_80239EE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_80239CCC();
#endif
extern u16  fightTrainerAiGetPokemonDataId(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  fn_801F1990(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  fn_801F1C18(u32, void*, void*, u32, u32);
#endif
extern u8   fn_801F4C14(u32, u16, u32, u16, u32);
extern u32  fn_802096E8(void*);
extern u32  menuCBRule_ProhibitionSuicide(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    wsStatBuffChange();
#endif
extern u8    JitabataTbl[12];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_802653FC(void* ptr, u16 a, s32 b);
#endif
extern u8    fn_80203E0C(void* ctx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_8011BBD8();
#endif
extern u32   fn_80203ADC(void* ctx, u32 param);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fn_801F2988(u32 param1, u32 param2);
#endif
extern void  fn_801F2934(u32 param1, u32 param2, u32 param3);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_801F0134();
#endif
extern u8    fn_801F6E44(u32 param1, u32 param2);
extern void  fn_801F6DF0(u32 param1, u32 param2, u32 param3);
extern u32   fn_80203B5C(void* ctx, u32 param);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   fn_80205184(void* ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fightCheckAccuracyRules(u32 param2, u32 param1, u32 param3);
#endif
extern u16  fn_80205224();
extern void fn_80202810();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   fn_802062FC();
#endif
extern u8   lbl_80378721[];
extern u8   lbl_80379249[];
extern u32 ServerStatusFlag;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   fn_80203CCC();
#endif
extern u8   fn_801F221C();
extern void fn_80120B00();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void statusSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  statusGetStatus();
#endif
extern void fn_801252E0();
extern void fn_80202998();
extern u8   fn_80204A10();
extern void* fn_802037DC(void* ctx);
extern void battleCameraSelect();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightMenuAllFightTrainerCloseStatusMenu();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightMenuAllFightOutPokemonCloseStatusMenu();
#endif
extern u8   lbl_8037889D[0x23];
extern void fn_80132A38();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std130_soundUnk_801653cc(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStart(s32, s32, s32, s32);
#endif
extern void fn_801F22D8(u32 obj);
extern u32  fn_80262308(void);
extern u8   lbl_8047B642;
extern u32  fn_802036D4();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fn_801F4354();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonBiosCopy();
#endif
extern u8   fn_801F2020();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fightTrainerGetIrekaeFightPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fn_800FA280();
#endif
extern u32  fn_80203848();
extern u16  lbl_8047B61C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32  fn_801FEF74();
#endif
extern void fn_802086B0();
extern void fn_8020F108();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s8   wazaSequenceSysIsAllEnd();
#endif
extern u8   lbl_80379A22[0x14];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fn_8020912C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fn_802656AC();
#endif
extern s32  fn_80102620();
extern u32  fn_80011C78();
extern u8   fn_802038A4();
extern void fn_80207C24();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonSetPokemonJoutai08();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32  fightSeqCalcDamage();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _fightDoIkakuSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   fn_801F453C();
#endif
extern u8   lbl_8047B628;
extern u8   lbl_80379945[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  fn_802040E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  fn_80203FE4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80203EDC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801FAA58();
#endif
extern u16  lbl_80279FD0[8];
extern u8   lbl_80379B06[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_80232FE4();
#endif
extern void fn_8020A2B8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   joutaiDataBiosGetMaxCount();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  fightFloorGetValidFightOutPokemonCount(u32, u8, u32, u8);
#endif
extern u8   fn_802016A4();
extern u8   lbl_8037984D[27];
extern u8   lbl_80375FDF[9];
extern void heroAddPokedoru(u32, s32);
extern u8   lbl_8037939C[9];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8  wazaSequenceOwnerLoadWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceSysStartWaza();
#endif
extern u16 lbl_8047B60C;
extern int fightSeqGetTemotiPokemonNekonoteWazaDataAry();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightSeqSelectTarget();
#endif
extern void fn_801F6EEC();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightSideInitJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801FBC20();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80208C18();
#endif
extern void fn_80205AD4();
extern void fn_80205A7C();
extern void fn_80206C94();
extern SpecialMoveList22 lbl_8027A408;
extern U16Pair lbl_8047E628;
extern U16Pair lbl_8047E62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8  fightTrainerIsAllyFightTargetPtr(u32 ctx, u32 poke, u16 floorVal);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetZokuseiDataId(u32 poke, u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetTokuseiDataId(u32 poke);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 zokuseiGetAisyouDualType(u16 waza, u16* types, u16 typeCount);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiIsAbicntOverMax();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerAiGetFightPokemonWazaAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetFightTrainerFightOutPokemonPtrAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightTrainerAiAddValue();
#endif
extern u32* waza_data_number;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut();
#endif
extern s32 fightTrainerAiWazaValueJisin();
extern s32 fightTrainerAiWazaValueJibaku();
extern s32 fightTrainerAiWazaValueNull();


/* fightTrainerCreateTemotiFightPokemon */
#define fn_801233F4 pokemonCheckFightOut
#define fn_80123FBC pokemonCheckValid
#define fn_8012640C pokemonGetStatus
#define fn_8012A5B0 heroGetStatus
#define fn_801F4804 fightFloorGetFightPokemonEntryCntInc
#define fn_801F54A4 fightFloorGetStatus
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_80205BE8 fightPokemonGetPokemonPtr
#define fn_80206AEC fightPokemonCreate
u32 fightTrainerCreateTemotiFightPokemon(u32 r3, u32 r4, int r5, u16 r6, char r7, int r8)
{
    extern u8 fn_801233F4();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80123FBC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_8012A5B0();
#endif
    extern u32 fn_801F4804();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205BE8();
#endif
    extern void fn_80206AEC();
  struct cpy85 { u32 d[85]; };
  u32 uVar1;
  u32 *puVar2;
  u32 *puVar3;
  u32 uVar4;
  u16 uVar9;
  struct cpy85 *iVar7;
  u32 uVar15;
  u32 uVar6;
  int iVar16;
  struct cpy85 *iVar5;
  u8 cVar11;
  int iVar8;
  u16 uVar10;
  u32 uVar13;
  u32 *puVar12;
  u32 *puVar14;
  struct cpy85 uStack_198;
  struct cpy85 uStack_2ec;

  uVar9 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  fn_801FB1C0(0,uVar9,2,0);
  for (uVar13 = 0; (u16)uVar13 < 6; uVar13 = uVar13 + 1) {
    *(u32 *)PC_PTR((r8 + (u32)(u16)uVar13 * 4)) = 0;
  }
  iVar5 = (struct cpy85 *)fn_801FB1C0(PC_PTR(r3),0,0x45,0);
  if (iVar5 == 0) {
    return 0;
  }
  else {
    iVar7 = (struct cpy85 *)fn_801FB1C0(PC_PTR(r3),0,0x45,1);
    if (iVar7 == 0) {
      return 0;
    }
    else {
      uStack_198 = *iVar5;
      uStack_2ec = *iVar7;
      for (uVar13 = 0; (u16)uVar13 < r6; uVar13 = uVar13 + 1) {
      }
      uVar6 = 0;
      for (uVar15 = 0; (uVar15 & 0xffff) < 6; uVar15 = uVar15 + 1) {
        iVar16 = fn_8012A5B0(PC_PTR(r4),3,uVar15);
        if (((iVar16 != 0) && (cVar11 = fn_80123FBC(PC_PTR(iVar16)), cVar11 != 0)) &&
           (cVar11 = fn_801233F4(iVar16), cVar11 != 0)) {
          uVar13 = 0;
          goto _wcond;
_wbody:
          iVar8 = *(int *)PC_PTR((r5 + (u32)(u16)uVar13 * 4));
          if ((u32)iVar8 == 0) goto _wincr;
          if ((u32)iVar8 == (u32)iVar16) goto _wafter;
_wincr:
          uVar13 = uVar13 + 1;
_wcond:
          if ((u16)uVar13 < r6) goto _wbody;
_wafter:
          if ((u16)uVar13 < r6) goto LAB_0023124c;
          {
            fn_80206AEC(iVar5,iVar16,fn_801F4804(0));
            if (r7 != 0) {
              if (r7 == 1) {
                fn_801F54A4(0,0,0x14,0);
                uVar10 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
                uVar10 = fn_801FB1C0(0,uVar10,2,0);
                uVar4 = PC_PTR32_IMPLICIT(fn_80205BE8(iVar5));
                uVar9 = (int)fn_8012640C(PC_PTR(uVar4),0,0xc9,0);
                cVar11 = fn_801FB1C0(0,uVar10,0x23,0);
                if (cVar11 == 1) {
                  cVar11 = fn_801FB1C0(0,uVar9,0x1c,0);
                }
                else {
                  cVar11 = 0;
                }
                if (cVar11 != 2) goto LAB_0023124c;
              }
              else if (r7 == 2) {
                fn_801F54A4(0,0,0x14,0);
                uVar9 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
                uVar9 = fn_801FB1C0(0,uVar9,2,0);
                uVar4 = PC_PTR32_IMPLICIT(fn_80205BE8(iVar5));
                uVar10 = (int)fn_8012640C(PC_PTR(uVar4),0,0xc9,0);
                cVar11 = fn_801FB1C0(0,uVar9,0x23,0);
                if (cVar11 == 1) {
                  cVar11 = fn_801FB1C0(0,uVar10,0x1c,0);
                }
                else {
                  cVar11 = 0;
                }
                if (cVar11 != 3) goto LAB_0023124c;
              }
              else if (r7 == 3) {
                fn_801F54A4(0,0,0x14,0);
                uVar9 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
                uVar9 = fn_801FB1C0(0,uVar9,2,0);
                uVar4 = PC_PTR32_IMPLICIT(fn_80205BE8(iVar5));
                uVar10 = (int)fn_8012640C(PC_PTR(uVar4),0,0xc9,0);
                cVar11 = fn_801FB1C0(0,uVar9,0x23,0);
                if (cVar11 == 1) {
                  cVar11 = fn_801FB1C0(0,uVar10,0x1c,0);
                }
                else {
                  cVar11 = 0;
                }
                if (cVar11 != 2) {
                  fn_801F54A4(0,0,0x14,0);
                  uVar9 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
                  uVar9 = fn_801FB1C0(0,uVar9,2,0);
                  uVar4 = PC_PTR32_IMPLICIT(fn_80205BE8(iVar5));
                  uVar10 = (int)fn_8012640C(PC_PTR(uVar4),0,0xc9,0);
                  cVar11 = fn_801FB1C0(0,uVar9,0x23,0);
                  if (cVar11 == 1) {
                    cVar11 = fn_801FB1C0(0,uVar10,0x1c,0);
                  }
                  else {
                    cVar11 = 0;
                  }
                  if (cVar11 != 3) goto LAB_00231240;
                }
                goto LAB_0023124c;
              }
            }
LAB_00231240:
            uVar1 = uVar6 & 0xffff;
            uVar6 = uVar6 + 1;
            *(int *)PC_PTR((r8 + uVar1 * 4)) = iVar16;
          }
        }
LAB_0023124c: (void)0;
      }
      *iVar5 = uStack_198;
      *iVar7 = uStack_2ec;
    }
  }
  return uVar6;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80206AEC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205BE8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4804
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012A5B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80123FBC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801233F4
#endif

/* fightTrainerAiSelectFightAction */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fightTrainerAiSelectFightAction(u32 trainer, u32 fightType) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTypeDataBiosGetFightoutPokemonNum();
#endif
    extern u8 fightOutPokemonCheckFightActionSelect();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 dbgMenuFightGetFightTrainerAiFlowDispFlag();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightMenuOpenMsg();
#endif
    extern u8 fightFloorCheckFightActionFightOutPokemonIrekaeSelect();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetStatus();
#endif
    extern u8 fightOutPokemonCheckFightActionWazaSelect();
    u32 noAction[8];
    u16 keyFinal[14];
    u16 keyOk1[14];
    u16 keyFail1[14];
    u16 keyOk2[14];
    u16 keyFail2[14];
    u16 keyOk3[14];
    u16 keyFail3[14];
    u32 fightTypeData;
    u32 pokemon;
    u32 i;
    u16 j;
    u32 count;
    u16 species;
    u8 selected;
    u16 selectedCount;

    fightTypeData = PC_PTR32_IMPLICIT(fightTypeDataBiosGetPtr(fightType));
    count = (u8)fightTypeDataBiosGetFightoutPokemonNum(PC_PTR(fightTypeData));
    fn_80234A0C(trainer);

    for (i = 0; (u16)i < count; i++) {
        pokemon = PC_PTR32_IMPLICIT(fightTrainerGetValidFightOutPokemonPtr(trainer, i));
        if (pokemon != 0 && fightOutPokemonCheckFightActionSelect(pokemon, 1) != 0) {
            fightOutPokemonInitFightActionBuff(PC_PTR(pokemon));
        }
    }

    species = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
    species = (u16)fightTrainerGetStatus(0, species, 2, 0);
    j = (u8)fightTrainerGetStatus(0, species, 0x26, 0);
    if ((s32)j > (GSrand() % 100)) {
        msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec04)));
        if (trainer != 0) {
            msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo(keyOk1, 1);
            if ((keyOk1[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 1;
    } else {
        msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec04)));
        if (trainer != 0) {
            msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo(keyFail1, 1);
            if ((keyFail1[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec68) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 0;
    }

    if (selected == 1) {
        for (i = 0; (u16)i < count; i++) {
            pokemon = PC_PTR32_IMPLICIT(fightTrainerGetNoActionFightOutPokemonPtr(trainer, i));
            if (pokemon != 0 && fightFloorCheckFightActionFightOutPokemonIrekaeSelect(0, pokemon, 0) == 0) {
#ifdef TARGET_PC /* the fourth parameter is never read (the callee reads r3-r5 only): any value */
                fightTrainerAiSelectFightActionIrekae(PC_PTR(trainer), pokemon, fightType, 0);
#else
                fightTrainerAiSelectFightActionIrekae(trainer, pokemon, fightType);
#endif
            }
        }
    }

    if (fightFloorGetStatus(0, 0, 0x20, 0) == 1) {
        species = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
        species = (u16)fightTrainerGetStatus(0, species, 2, 0);
        j = (u8)fightTrainerGetStatus(0, species, 0x27, 0);
        if ((s32)j > (GSrand() % 100)) {
            msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec46)));
            if (trainer != 0) {
                msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
            }
            msgctrlSetValue(0x2f, j);
            if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
                menuGetKeyInfo(keyOk2, 1);
                if ((keyOk2[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
                    fightMenuCloseMsg();
                }
            }
            selected = 1;
        } else {
            msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec46)));
            if (trainer != 0) {
                msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
            }
            msgctrlSetValue(0x2f, j);
            if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
                menuGetKeyInfo(keyFail2, 1);
                if ((keyFail2[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec68) == 1) {
                    fightMenuCloseMsg();
                }
            }
            selected = 0;
        }

        if (selected == 1) {
            for (i = 0; (u16)i < count; i++) {
                pokemon = PC_PTR32_IMPLICIT(fightTrainerGetNoActionFightOutPokemonPtr(trainer, i));
                if (pokemon != 0) {
                    fightTrainerAiSelectFightActionItem(PC_PTR(trainer), pokemon, fightType);
                }
            }
        }
    }

    species = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
    species = (u16)fightTrainerGetStatus(0, species, 2, 0);
    j = (u8)fightTrainerGetStatus(0, species, 0x25, 0);
    if ((s32)j > (GSrand() % 100)) {
        msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec47)));
        if (trainer != 0) {
            msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo(keyOk3, 1);
            if ((keyOk3[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 1;
    } else {
        msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec47)));
        if (trainer != 0) {
            msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo(keyFail3, 1);
            if ((keyFail3[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec68) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 0;
    }

    if (selected == 1) {
        for (i = 0; (u16)i < count; i++) {
            pokemon = PC_PTR32_IMPLICIT(fightTrainerGetNoActionFightOutPokemonPtr(trainer, i));
            if (pokemon != 0) {
                fn_8024E534(PC_PTR(trainer), pokemon, fightType);
            }
        }
    }
    {
        /* ORRE_BATTLE_LOG (observe only, 2026-10-25): the AI data's switch / item / combo chances (the three
         * "value > rand % 100" checks above) and whether the combo pass ran */
        static int enabled = -1;
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        if (enabled < 0) {
            extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
            enabled = pc_battle_log_on();
        }
        if (enabled) {
            OSReport("[battle] ai checks: ai data %u switch %u item %u combo %u (combo ran %u) out %u\n",
                     (unsigned)species, (unsigned)(u8)fightTrainerGetStatus(0, species, 0x26, 0),
                     (unsigned)(u8)fightTrainerGetStatus(0, species, 0x27, 0),
                     (unsigned)(u8)fightTrainerGetStatus(0, species, 0x25, 0), (unsigned)selected, (unsigned)count);
        }
    }

    msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xec2c)));
    if (trainer != 0) {
        msgctrlSetValue(0x13, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
    }
    msgctrlSetValue(0x2f, 100);
    if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
        menuGetKeyInfo(keyFinal, 1);
        if ((keyFinal[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
            fightMenuCloseMsg();
        }
    }

    for (j = 0; j < 8; j++) {
        noAction[j] = 0;
    }

    selectedCount = 0;
    for (i = 0; (u16)i < count; i++) {
        pokemon = PC_PTR32_IMPLICIT(fightTrainerGetNoActionFightOutPokemonPtr(trainer, i));
        if (pokemon != 0 && fightOutPokemonCheckFightActionWazaSelect(pokemon, 1) == 0) {
            noAction[selectedCount] = pokemon;
            selectedCount++;
        }
    }

    if (selectedCount != 0) {
        fightFloorSortFightOutPokemonPtrArySub(0, noAction, 8, 0);
        {
            /* ORRE_BATTLE_LOG (observe only, 2026-10-25, P05's battle 56): which Pokemon get a move choice, in
             * the order they get it, after the speed sort */
            static int enabled = -1;
            extern char* getenv(const char*);
            extern void OSReport(const char*, ...);
            if (enabled < 0) {
                extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
                enabled = pc_battle_log_on();
            }
            if (enabled) {
                OSReport("[battle] ai order: %u [%08X %08X %08X %08X]\n", (unsigned)selectedCount,
                         (unsigned)noAction[0], (unsigned)noAction[1], (unsigned)noAction[2], (unsigned)noAction[3]);
            }
        }
        for (count = 0; (u16)count < selectedCount; count++) {
            if (noAction[(u16)count] != 0) {
                fightTrainerAiSelectWaza(trainer, noAction[(u16)count], fightType);
            }
        }
    }
}
#else
void fightTrainerAiSelectFightAction(u32 trainer, u32 fightType) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTypeDataBiosGetFightoutPokemonNum();
#endif
    extern u8 fightOutPokemonCheckFightActionSelect();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 dbgMenuFightGetFightTrainerAiFlowDispFlag();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightMenuOpenMsg();
#endif
    extern u8 fightFloorCheckFightActionFightOutPokemonIrekaeSelect();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetStatus();
#endif
    extern u8 fightOutPokemonCheckFightActionWazaSelect();
    /* RULE-EXCEPTION(user-approved): caller-side prototype with a u32 fight type (the definition takes u16); retail passes the untruncated value (mr r3, no clrlwi) - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern struct FightTypeData* fightTypeDataBiosGetPtr(u32 index);
#endif
    extern void* fightTrainerGetValidFightOutPokemonPtr(void* arg1, u32 arg2);
    extern void fightOutPokemonInitFightActionBuff(void* param_1);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetStatus(void* ctx, u16 slot, u32 field, u32 idx);
#endif
    extern void msgctrlSetValue(u32 id, u32 value);
    extern void* GSmsgGetGSchar(u32 key);
    extern void* fightTrainerGetNamePtr(void* context);
    extern void menuGetKeyInfo(struct MenuKeyInfo* out, s32 port);
    extern void* fightTrainerGetNoActionFightOutPokemonPtr(void* context, u32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiSelectFightActionIrekae(void* ctx, u32 param1, u32 param2);
#endif
    extern s32 fightTrainerAiSelectFightActionItem(void* ctx, u32 param1, u32 param2);
    extern u32 fn_8024E534(void* ctx, u32 slot, u32 param);
    extern void fightFloorSortFightOutPokemonPtrArySub(void* floor, u32* ary_, u16 num, u8 usePri);
    extern int fightTrainerAiSelectWaza(u32 r3, u32 r4, u32 r5);
    u32 noAction[8];
    u16 keyFinal[14];
    u16 keyOk1[14];
    u16 keyFail1[14];
    u16 keyOk2[14];
    u16 keyFail2[14];
    u16 keyOk3[14];
    u16 keyFail3[14];
    u32 fightTypeData;
    u32 pokemon;
    u32 i;
    u16 j;
    u32 count;
    u16 species;
    u8 selected;
    u16 selectedCount;

    fightTypeData = (u32)fightTypeDataBiosGetPtr(fightType);
    count = (u8)fightTypeDataBiosGetFightoutPokemonNum(fightTypeData);
    fn_80234A0C(trainer);

    for (i = 0; (u16)i < count; i++) {
        pokemon = (u32)fightTrainerGetValidFightOutPokemonPtr((void*)trainer, i);
        if (pokemon != 0 && fightOutPokemonCheckFightActionSelect(pokemon, 1) != 0) {
            fightOutPokemonInitFightActionBuff((void*)pokemon);
        }
    }

    species = (u16)fightTrainerGetStatus((void*)trainer, 0, 0x43, 0);
    species = (u16)fightTrainerGetStatus(0, species, 2, 0);
    j = (u8)fightTrainerGetStatus(0, species, 0x26, 0);
    if ((s32)j > (GSrand() % 100)) {
        msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec04));
        if (trainer != 0) {
            msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo((struct MenuKeyInfo*)keyOk1, 1);
            if ((keyOk1[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 1;
    } else {
        msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec04));
        if (trainer != 0) {
            msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo((struct MenuKeyInfo*)keyFail1, 1);
            if ((keyFail1[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec68) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 0;
    }

    if (selected == 1) {
        for (i = 0; (u16)i < count; i++) {
            pokemon = (u32)fightTrainerGetNoActionFightOutPokemonPtr((void*)trainer, i);
            if (pokemon != 0 && fightFloorCheckFightActionFightOutPokemonIrekaeSelect(0, pokemon, 0) == 0) {
                fightTrainerAiSelectFightActionIrekae((void*)trainer, pokemon, fightType);
            }
        }
    }

    if (fightFloorGetStatus(0, 0, 0x20, 0) == 1) {
        species = (u16)fightTrainerGetStatus((void*)trainer, 0, 0x43, 0);
        species = (u16)fightTrainerGetStatus(0, species, 2, 0);
        j = (u8)fightTrainerGetStatus(0, species, 0x27, 0);
        if ((s32)j > (GSrand() % 100)) {
            msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec46));
            if (trainer != 0) {
                msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
            }
            msgctrlSetValue(0x2f, j);
            if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
                menuGetKeyInfo((struct MenuKeyInfo*)keyOk2, 1);
                if ((keyOk2[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
                    fightMenuCloseMsg();
                }
            }
            selected = 1;
        } else {
            msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec46));
            if (trainer != 0) {
                msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
            }
            msgctrlSetValue(0x2f, j);
            if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
                menuGetKeyInfo((struct MenuKeyInfo*)keyFail2, 1);
                if ((keyFail2[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec68) == 1) {
                    fightMenuCloseMsg();
                }
            }
            selected = 0;
        }

        if (selected == 1) {
            for (i = 0; (u16)i < count; i++) {
                pokemon = (u32)fightTrainerGetNoActionFightOutPokemonPtr((void*)trainer, i);
                if (pokemon != 0) {
                    fightTrainerAiSelectFightActionItem((void*)trainer, pokemon, fightType);
                }
            }
        }
    }

    species = (u16)fightTrainerGetStatus((void*)trainer, 0, 0x43, 0);
    species = (u16)fightTrainerGetStatus(0, species, 2, 0);
    j = (u8)fightTrainerGetStatus(0, species, 0x25, 0);
    if ((s32)j > (GSrand() % 100)) {
        msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec47));
        if (trainer != 0) {
            msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo((struct MenuKeyInfo*)keyOk3, 1);
            if ((keyOk3[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 1;
    } else {
        msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec47));
        if (trainer != 0) {
            msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
        }
        msgctrlSetValue(0x2f, j);
        if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
            menuGetKeyInfo((struct MenuKeyInfo*)keyFail3, 1);
            if ((keyFail3[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec68) == 1) {
                fightMenuCloseMsg();
            }
        }
        selected = 0;
    }

    if (selected == 1) {
        for (i = 0; (u16)i < count; i++) {
            pokemon = (u32)fightTrainerGetNoActionFightOutPokemonPtr((void*)trainer, i);
            if (pokemon != 0) {
                fn_8024E534((void*)trainer, pokemon, fightType);
            }
        }
    }

    msgctrlSetValue(0xd, (u32)GSmsgGetGSchar(0xec2c));
    if (trainer != 0) {
        msgctrlSetValue(0x13, (u32)fightTrainerGetNamePtr((void*)trainer));
    }
    msgctrlSetValue(0x2f, 100);
    if (dbgMenuFightGetFightTrainerAiFlowDispFlag() == 1) {
        menuGetKeyInfo((struct MenuKeyInfo*)keyFinal, 1);
        if ((keyFinal[0] & MENU_KEY_START) == 0 && fightMenuOpenMsg(0xec67) == 1) {
            fightMenuCloseMsg();
        }
    }

    for (j = 0; j < 8; j++) {
        noAction[j] = 0;
    }

    selectedCount = 0;
    for (i = 0; (u16)i < count; i++) {
        pokemon = (u32)fightTrainerGetNoActionFightOutPokemonPtr((void*)trainer, i);
        if (pokemon != 0 && fightOutPokemonCheckFightActionWazaSelect(pokemon, 1) == 0) {
            noAction[selectedCount] = pokemon;
            selectedCount++;
        }
    }

    if (selectedCount != 0) {
        fightFloorSortFightOutPokemonPtrArySub(0, noAction, 8, 0);
        for (count = 0; (u16)count < selectedCount; count++) {
            if (noAction[(u16)count] != 0) {
                fightTrainerAiSelectWaza(trainer, noAction[(u16)count], fightType);
            }
        }
    }
}
#endif
