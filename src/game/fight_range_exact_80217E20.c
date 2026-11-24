/**
 * @file fight_range_exact_80217E20.c
 * @brief Strict target-order fight island, 0x80217E20 - 0x80218BD4.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/moves.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/pokemon_status.h"

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

/* 0x80217E20 size 0x1F8: wsCmdB3 (owner line 13060) */
/* RULE-EXCEPTION(user-approved): fightOutPokemonGetUseWazaDataId is defined u16 in fight_out_pokemon.c, but retail uses the result here unextended (mr, no clrlwi); declared u32 like the file's fn_80205184 - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetUseWazaDataId(void* ctx);
#endif
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_802096E8 fightWazaIsHit
#define fn_80200B10 fightOutPokemonInitJoutaiKeep
#define fn_8020A2B8 fightWazaBiosCopy
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_8011BEB4 wazaGetStatus
#define fn_8011BBD8 wazaSetStatus
void wsCmdB3(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetMaxCount();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern short fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80200B10();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonJoutaiStack();
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
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802096E8();
#endif
    extern void fn_8020A2B8();
  u32 uVar3;
  u32 uVar1;
  u32 uVar2;
  int iVar3;
  u32 uVar4;
  u8 cVar8;
  u8 uVar5;
  u32 uVar6;
  u16 sVar7;
  u32 uVar9;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  uVar3 = (int)fn_8012640C(PC_PTR(uVar1),0,0xf8,0);
  uVar4 = fn_80205184((void*)PC_PTR(uVar1));
  cVar8 = fn_802096E8(PC_PTR(uVar2));
  if (cVar8 == 0) {
    fn_80200B10(PC_PTR(uVar1));
    lbl_8047B610 = lbl_80375FDF;
  }
  else {
    cVar8 = fn_802025B8(PC_PTR(uVar1),PC_PTR(0x2f));
    if (cVar8 == 2) {
      fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x2f),0);
      cVar8 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x22));
      if (cVar8 == 0) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x22),0);
        fn_8020A2B8(uVar3,uVar2);
      }
    }
    uVar6 = 1;
    cVar8 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x2f));
    if (cVar8 == 1) {
      uVar5 = joutaiDataBiosGetMaxCount(0x2f);
      uVar6 = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(uVar1),PC_PTR(0x2f));
      if ((short)uVar6 == uVar5) {
        fn_80202810(PC_PTR(uVar1),PC_PTR(0x2f));
        fn_80202810(PC_PTR(uVar1),PC_PTR(0x22));
      }
    }
    sVar7 = fn_8011BEB4(0,uVar4,7,0);
    for (uVar9 = 1; (int)(uVar9 & 0xffff) < (int)(short)uVar6; uVar9 = uVar9 + 1) {
      sVar7 = sVar7 << 1;
    }
    fn_8011BBD8(PC_PTR(uVar2),0,0x2f,0,sVar7);
    cVar8 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x1a));
    if (cVar8 == 1) {
      uVar9 = (u16)fn_8011BEB4(PC_PTR(uVar2),0,0x2f,0);
      fn_8011BBD8(PC_PTR(uVar2),0,0x2f,0,(uVar9 << 1) & 0xfffe);
    }
    lbl_8047B610 = lbl_8047B610 + 1;
  }
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020A2B8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80200B10
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802096E8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80218018 size 0x1C0: wsCmdB2 (owner line 13139) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmdB2(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId();
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
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fightSeqWsHorobinoutaSub__FPvUsPv();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 attacker;
    u32 pokemon;
    s8 slot;
    u8 slot8;
    int slotIndex;
    u32 move;
    u32 target;
    u32 ability;
    u8 pp;
    u8 newPp;
    u32 flags;
    int callbackResult;
    u8* nextPc;

    callbackResult = 0;
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    if (fightCheckAccuracyRules(move, attacker, target) == 1) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    } else {
        fightFloorLoopValidFightOutPokemon(
            0, _fightSeqWsHorobinoutaSub__FPvUsPv, &callbackResult, 0);
        if (attacker != 0) {
            ability = (u8)fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId(
                0, ABILITY_PRESSURE, 0, attacker);
            if (ability != 0) {
                pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
                slot = pokemonSearchWazaDataId(PC_PTR(pokemon), MOVE_PERISH_SONG);
                if (slot >= 0) {
                    slotIndex = slot;
                    pp = (u8)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slotIndex);
                    newPp = ability < pp ? (u8)(pp - ability) : 0;
                    pokemonSetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slotIndex, newPp);
                    slot8 = (u8)slot;
                    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x10)) == 0 &&
                        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x31)) == 1) {
                        flags = fightOutPokemonGetPokemonJoutai08(attacker, 0x31);
                        if ((flags & (1 << slot8)) == 0 &&
                            fightOutPokemonIsUseHensinBuff(PC_PTR(attacker)) == 1) {
                            fightOutPokemonSetHensinPokemonStatusId(
                                PC_PTR(attacker), 0x80, slot8, 0);
                        }
                    }
                }
            }
        }
        if (callbackResult == 0) {
            nextPc = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        } else {
            nextPc = lbl_8047B610 + 5;
        }
        lbl_8047B610 = nextPc;
    }
}

/* 0x802181D8 size 0x98: _fightSeqWsHorobinoutaSub__FPvUsPv (owner line 13214) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
u32 _fightSeqWsHorobinoutaSub__FPvUsPv(void* r3, u16 r4, void* r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4();
#endif
  u32 pokemon;
  int* count;
  u8 cVar2;
  u32 sVar1;

  count = (int*)r5;
  pokemon = PC_PTR32(r3);
  cVar2 = fn_802062FC(PC_PTR(pokemon));
  if (cVar2 == 0) {
    return 1;
  }
  sVar1 = fn_80207BF4(PC_PTR(pokemon));
  cVar2 = fn_802025B8(PC_PTR(pokemon),PC_PTR(0x1e));
  if (cVar2 != 2 || (u16)sVar1 == ABILITY_SOUNDPROOF) {
    return 1;
  }
  fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(pokemon),PC_PTR(0x1e),0);
  *count = *count + 1;
  return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif

/* 0x80218270 size 0x64: WS_MIYABURU (owner line 13245) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
void WS_MIYABURU(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fn_802025B8();
#endif
  u32 uVar1;
  u8 cVar2;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  cVar2 = fn_802025B8(PC_PTR(uVar1),PC_PTR(0x19));
  if (cVar2 == 2) {
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x19),0);
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802182D4 size 0xE8: wsCmdB0 (owner line 4398) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E44 fightSideCheckWriteJoutaiDataId
#define fn_801F6E98 fightSideIsJoutaiDataId
#define fn_801F6D9C fightSideGetCountAsJoutaiDataId
#define fn_801254B4 pokemonSetStatus
#define fn_801F6DF0 fightSideWriteJoutaiDataId
void wsCmdB0(void) {
    extern u8 fn_801F6E44(u32, u32);
    extern void fn_801F6DF0(u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801254B4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_801F6E98();
#endif
    extern s16 fn_801F6D9C();
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 tmp = PC_PTR32_IMPLICIT(fn_801F025C(3, ctx1));
    s16 val;

    if (fn_801F6E44(tmp, 0x4a) != 2) {
        goto matched;
    }
    if (fn_801F6E98(tmp, 0x4a) == 1) {
        val = fn_801F6D9C(tmp, 0x4a);
    } else {
        val = 0;
    }
    if (val != joutaiDataBiosGetMaxCount(0x4a)) {
        goto notmatched;
    }
matched:
    fn_801254B4(PC_PTR(ctx1), 0, 0x118, 0, 1);
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    return;
notmatched:
    fn_801F6DF0(tmp, 0x4a, 0);
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6DF0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6D9C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E98
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E44
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802183BC size 0xC0: wsCmdAF (owner line 1583) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_8011BBD8 wazaSetStatus
void wsCmdAF(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203B5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    void* ctx = (void*)fn_801F025C(0x11, 0);
    u32 ctx2;
    u32 resolved = (u32)fn_8012640C(ctx, 0, 0xd9, 0);
    ctx2 = PC_PTR32(fn_801F025C(0x12, 0));
    if (fn_802025B8(PC_PTR(ctx2), PC_PTR(0x18)) != 2) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx2), PC_PTR(0x18), 0);
        fn_8011BBD8(PC_PTR(resolved), 0, 0x2d, 0, (u16)fn_80203B5C(ctx, 2));
        lbl_8047B610 += 5;
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021847C size 0x3A8: wsCmdAE (owner line 13282) */
#define fn_801248C4 pokemonGetTokuseiDataId
#define fn_801252E0 pokemonInitJoutai
#define fn_8012A5B0 heroGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F7258 fightSideGetValidFightTrainerPtr
#define fn_801F9930 fightTrainerCheckTemotiPokemonFightEntry
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_80202998 fightOutPokemonResetSeqStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80205BE8 fightPokemonGetPokemonPtr
#define fn_80206780 fightOutPokemonCheckValid
#define fn_80206A04 fightPokemonCheckValid
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
void wsCmdAE(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_801248C4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801252E0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012A5B0();
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
    extern int fn_801F7258();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F9930();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
    extern void fn_80202998();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205BE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80206780();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80206A04();
#endif
  u32 uVar1;
  u32 sVar7;
  u32 iVar2;
  u32 uVar3;
  u32 uVar4;
  u8 cVar8;
  u32 uVar5;
  u32 trainerStatus;
  int iVar6;
  u32 uVar9;
  u32 uVar10;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  sVar7 = fn_80205184((void*)PC_PTR(uVar1));
  iVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0xe,uVar1));
  uVar3 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar1));
  uVar4 = (u16)fn_801F54A4(0,0,0x16,0);
  if ((sVar7 & 0xffff) == 0xd7) {
    lbl_80478D78[5] = 0;
    sVar7 = fn_80207BF4(PC_PTR(uVar1));
    if ((sVar7 & 0xffff) != 0x2b) {

      fn_801252E0(fn_80205B8C(PC_PTR(uVar1))); /* r3 = the Pokemon pointer from the call before (assembly) */
      fn_80202998(uVar1,0);
      fn_80202810(PC_PTR(uVar1),PC_PTR(0x17));
      cVar8 = fn_801FECD4(PC_PTR(uVar1));
      if (cVar8 == 1) {
        fn_801FE7EC(PC_PTR(uVar1),0x7c,0,0);
      }
    }
    else {
      lbl_80478D78[5] |= 1;
    }
    if ((iVar2 != 0) && (cVar8 = fn_80206780(PC_PTR(iVar2)), cVar8 == 1)) {
      fn_801F4C14(0,0,0x4b,0,iVar2);
      sVar7 = fn_80207BF4(PC_PTR(iVar2));
      if ((sVar7 & 0xffff) != 0x2b) {

        fn_801252E0(fn_80205B8C(PC_PTR(iVar2))); /* r3 = the Pokemon pointer from the call before (assembly) */
        fn_80202998(iVar2,0);
        fn_80202810(PC_PTR(iVar2),PC_PTR(0x17));
        cVar8 = fn_801FECD4(PC_PTR(iVar2));
        if (cVar8 == 1) {
          fn_801FE7EC(PC_PTR(iVar2),0x7c,0,0);
        }
      }
      else {
        lbl_80478D78[5] |= 2;
      }
    }
    for (iVar6 = 0; iVar6 < uVar4; iVar6 = iVar6 + 1) {
      iVar2 = fn_801F7258(uVar3,iVar6 & 0xffff);
      if (iVar2 != 0) {
        uVar10 = fn_801FB1C0(PC_PTR(iVar2),0,0x44,0);
        uVar9 = 0;
        do {
          trainerStatus = PC_PTR32_IMPLICIT(fn_801F9930(iVar2,
              fn_8012A5B0(PC_PTR(uVar10),3,uVar9 & 0xffff)));
          if ((trainerStatus != 0) && (cVar8 = fn_80206A04(PC_PTR(trainerStatus)), cVar8 != 0)) {
            uVar1 = PC_PTR32_IMPLICIT(fn_80205BE8(trainerStatus));
            sVar7 = fn_801248C4(PC_PTR(uVar1));
            if ((sVar7 & 0xffff) != 0x2b) {
              fn_801252E0(PC_PTR(uVar1));
            }
          }
          uVar9 = uVar9 + 1;
        } while (uVar9 < 6);
      }
    }
  }
  else {
    lbl_80478D78[5] = 4;

    fn_801252E0(fn_80205B8C(PC_PTR(uVar1))); /* r3 = the Pokemon pointer from the call before (assembly) */
    fn_80202810(PC_PTR(uVar1),PC_PTR(0x17));
    cVar8 = fn_801FECD4(PC_PTR(uVar1));
    if (cVar8 == 1) {
      fn_801FE7EC(PC_PTR(uVar1),0x7c,0,0);
    }
    if ((iVar2 != 0) && (cVar8 = fn_80206780(PC_PTR(iVar2)), cVar8 == 1)) {
      fn_801F4C14(0,0,0x4b,0,iVar2);

      fn_801252E0(fn_80205B8C(PC_PTR(iVar2))); /* r3 = the Pokemon pointer from the call before (assembly) */
      fn_80202998(iVar2,0);
      fn_80202810(PC_PTR(iVar2),PC_PTR(0x17));
      cVar8 = fn_801FECD4(PC_PTR(iVar2));
      if (cVar8 == 1) {
        fn_801FE7EC(PC_PTR(iVar2),0x7c,0,0);
      }
    }
    for (uVar1 = 0; uVar1 < uVar4; uVar1 = uVar1 + 1) {
      iVar2 = fn_801F7258(uVar3,uVar1 & 0xffff);
      if (iVar2 != 0) {
        iVar6 = fn_801FB1C0(PC_PTR(iVar2),0,0x44,0);
        uVar10 = 0;
        do {
          uVar9 = PC_PTR32_IMPLICIT(fn_801F9930(iVar2,
              fn_8012A5B0(PC_PTR(iVar6),3,uVar10 & 0xffff)));
          if ((uVar9 != 0) && (cVar8 = fn_80206A04(PC_PTR(uVar9)), cVar8 != 0)) {

            fn_801252E0(fn_80205BE8(PC_PTR(uVar9)));
          }
          uVar10 = uVar10 + 1;
        } while (uVar10 < 6);
      }
    }
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80206A04
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80206780
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205BE8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202998
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F9930
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F7258
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012A5B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801252E0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801248C4
#endif

/* 0x80218824 size 0x248: wsCmdAD (owner line 13431) */
void wsCmdAD(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonInitJoutaiKeep();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 outPokemon;
    u32 pokemon;
    u16 slotMove;
    u16 move;
    u8 i;
    u8 pp;
    u8 amount;
    u32 mask;

    fightTargetGetPtrAsNowFightType(0x11, 0);
    outPokemon = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(outPokemon));
    move = pokemonGetStatus(PC_PTR(outPokemon), 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0);

    if (move != 0 && move != 0x165 && move != 0xFFFF && move != 0x164) {
        pp = 0;
        for (i = 0; i < 4; i++) {
            if (pokemonWazaCheckValid(PC_PTR(pokemon), i) != 0) {
                slotMove = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE, i);
                if (move == slotMove) {
                    pp = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, i);
                    break;
                }
            }
        }

        if (i >= 4) {
            lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
        } else {
            if (pp < 2) {
                lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
            } else {
                amount = (u8)((GSrand() % 4) + 2);
                if (pp < amount) {
                    amount = pp;
                }

                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, move, 1, 0))));
                msgctrlSetValue(0x2F, amount);
                pp -= amount;
                pokemonSetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, i, pp);

                if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(outPokemon), PC_PTR(0x10)) == 0 &&
                    fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(outPokemon), PC_PTR(0x31)) == 1) {
                    mask = fightOutPokemonGetPokemonJoutai08(outPokemon, 0x31);
                    if ((mask & (1 << i)) == 0 &&
                        fightOutPokemonIsUseHensinBuff(PC_PTR(outPokemon)) == 1) {
                        fightOutPokemonSetHensinPokemonStatusId(PC_PTR(outPokemon), 0x80, i, 0);
                    }
                }

                if (pp == 0) {
                    fightOutPokemonInitJoutaiKeep(PC_PTR(outPokemon));
                }
                lbl_8047B610 += 5;
            }
        }
        return;
    }
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
}

/* 0x80218A6C size 0x100: wsCmdAC (owner line 13509) */
void wsCmdAC(void)
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
    extern void wazaSetStatus();
#endif
    u32 ctx;
    u32 move;
    u32 pokemon;
    u16 currentHp;
    u16 maxHp;
    u8 key;
    s32 offset;
    s32 i;

    ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(ctx));
    currentHp = (u16)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_HP, 0);
    maxHp = (u16)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MAX_HP, 0);
    key = (u8)((currentHp * 0x30) / maxHp);
    if (key == 0 && currentHp != 0) {
        key = 1;
    }

    offset = 0;
    for (i = 6; i > 0; i--) {
        if (key <= JitabataTbl[offset]) {
            break;
        }
        offset += 2;
    }

    key = *(volatile u8*)PC_PTR((PC_PTR32(JitabataTbl) + offset + 1));
    wazaSetStatus(PC_PTR(move), 0, 0x2f, 0, key);
    lbl_8047B610++;
}

/* 0x80218B6C size 0x68: fightSeqGetJitabataIryoku (owner line 1323) */
u8 fightSeqGetJitabataIryoku(u16 a, u16 b) {
    u8 key = (u8)((a * 0x30) / b);
    s32 offset;
    s32 i;

    if (key == 0 && a != 0) {
        key = 1;
    }
    offset = 0;
    for (i = 6; i > 0; i--) {
        if (key <= JitabataTbl[offset]) {
            break;
        }
        offset += 2;
    }
    return JitabataTbl[offset + 1];
}
