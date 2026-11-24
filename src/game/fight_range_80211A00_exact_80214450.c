/**
 * @file fight_range_80211A00_exact_80214450.c
 * @brief Strict target-order fight island, 0x80214450 - 0x80216A58.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/moves.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */

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

/* 0x80214450 size 0x178: wsCmdEE (owner line 5506) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E98 fightSideIsJoutaiDataId
#define fn_801F6EEC fightSideInitJoutaiDataId
void wsCmdEE(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F6E98();
#endif
    u32 ctxA = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(3, ctxA));

    if (fn_801F6E98(ctx, 0x49) == 1) {
        goto L_check49_recheck;
    }
    if (fn_801F6E98(ctx, 0x48) != 1) {
        goto L_zero;
    }

L_check49_recheck:
    if (fn_801F6E98(ctx, 0x49) != 1) {
        goto L_check48_only;
    }
    if (fn_801F6E98(ctx, 0x48) != 1) {
        goto L_check48_only;
    }
    fn_801F6EEC(ctx, 0x49);
    fn_801F6EEC(ctx, 0x48);
    lbl_80379F58[0x16002] = 3;
    lbl_80379F58[0x160a1] = 3;
    goto L_done;

L_check48_only:
    if (fn_801F6E98(ctx, 0x49) == 1) {
        fn_801F6EEC(ctx, 0x49);
        lbl_80379F58[0x16002] = 1;
        lbl_80379F58[0x160a1] = 1;
        goto L_done;
    }
    if (fn_801F6E98(ctx, 0x48) == 1) {
        fn_801F6EEC(ctx, 0x48);
        lbl_80379F58[0x16002] = 2;
        lbl_80379F58[0x160a1] = 2;
    }
    goto L_done;

L_zero:
    lbl_80379F58[0x16002] = 0;
    lbl_80379F58[0x160a1] = 0;

L_done:
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6EEC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E98
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802145C8 size 0xEC: wsCmdED (owner line 13017) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
void wsCmdED(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
  u32 uVar1;
  int iVar2;
  int iVar3;
  int originalTarget;
  int iVar4;
  struct {
    int value;
  } status43;

  iVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  iVar3 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  iVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x19,0));
  originalTarget = iVar2;
  fn_801F4C14(0,0,0x47,0,iVar2);
  uVar1 = __cntlzw(iVar3 - iVar2);
  if (uVar1 >> 5 != 0) {
    status43.value = iVar4;
    iVar2 = iVar4;
  }
  if ((s32)(uVar1 >> 5) == 0) {
    status43.value = iVar4;
  }
  fn_801F4C14(0,0,0x4b,0,originalTarget);
  fn_801F4C14(0,0,0x36,0,iVar2);
  fn_801F4C14(0,0,0x43,0,status43.value);
  lbl_8047B610 += 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802146B4 size 0x10: wsCmdEC (owner line 212) */
u8* wsCmdEC(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 5;
    return pc;
}

/* 0x802146C4 size 0xD0: wsCmdEB (owner line 13061) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80136428 tikeiDataBiosGetZokuseiDataId
#define fn_80207AE0 fightOutPokemonIsZokuseiDataId
#define fn_80207B5C fightOutPokemonSetZokuseiDataId
#define fn_800FA280 GSmsgGetGSchar
#define fn_80132A38 msgctrlSetValue
void wsCmdEB(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800FA280();
#endif
    extern u32 zokuseiDataBiosGetName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
    extern u8 fn_80136428();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80207AE0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80207B5C();
#endif
  u16 uVar2;
  u32 uVar1;
  u8 uVar3;
  u8 cVar4;

  u32 uVar5;

  uVar2 = fn_801F54A4(0,0,0xf,0);
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar3 = fn_80136428(uVar2);
  cVar4 = fn_80207AE0(PC_PTR(uVar1),uVar3);
  if (cVar4 == 0) {
    for (uVar5 = 0; (uVar5 & 0xff) < 2; uVar5 = uVar5 + 1) {
      fn_80207B5C(PC_PTR(uVar1),uVar5,uVar3);
    }

    uVar1 = PC_PTR32_IMPLICIT(fn_800FA280(zokuseiDataBiosGetName(uVar3)));
    fn_80132A38(0xd,uVar1);
    lbl_8047B610 = lbl_8047B610 + 5;
  }
  else {
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
  }
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_800FA280
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207AE0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80136428
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x80214794 size 0xD0: wsCmdEA (owner line 9863) */
#pragma optimize_for_size reset
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_802040E8 fightOutPokemonGetSoubiItemDataId
#define fn_8012640C pokemonGetStatus
#define fn_801F4C14 fightFloorSetStatus
#define fn_8020147C fightOutPokemonDoItemSoubi
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
void wsCmdEA(void) {
    extern u8* lbl_8047B610;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802040E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4(void* trainer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC(void* trainer, u32 eventId, u32 param1, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F4C14(u32, u16, u32, u16, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8020147C(void* context, u16 moveId, u8 slot, u8 updateFlag);
#endif
    void* ctx;
    u32 val2;
    u16 val3;
    ctx = fn_801F025C(0x11, 0);
    val2 = fn_802040E8(ctx);
    val3 = (u16)fn_8012640C(PC_PTR32(ctx), 0, 0xfa, 0);
    if (val3 != 0 && (u16)val2 == 0) {
        fn_801F4C14(0, 0, 0x56, 0, val3);
        fn_8020147C(ctx, val3, 1, 1);
        if (fn_801FECD4(ctx) == 1) {
            fn_801FE7EC(ctx, 0x82, 0, 0);
        }
        lbl_8047B610 += 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020147C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802040E8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214864 size 0x154: wsCmdE9 (owner line 5349) */
#pragma optimize_for_size reset
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F453C fightFloorGetNowTenkouDataId
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8011BBD8 wazaSetStatus
void wsCmdE9(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F453C();
#endif
    u32 fieldD9;
    u8 sel = (u8)fn_801F453C(0, 1);
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);

    if (sel == 2) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x30, 0, 0xb);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2c, 0, 2);
    } else if (sel == 3) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x30, 0, 5);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2c, 0, 2);
    } else if (sel == 1) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x30, 0, 0xa);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2c, 0, 2);
    } else if (sel == 4) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x30, 0, 0xf);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2c, 0, 2);
    } else {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x30, 0, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F453C
#endif

/* 0x802149B8 size 0xFC: wsCmdE8 (owner line 9904) */
#pragma optimize_for_size reset
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_8011BEB4 wazaGetStatus
void wsCmdE8(void) {
    extern u8* lbl_8047B610;
    extern u8  lbl_80478D78[1];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207BF4(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
    void* ctx;
    u32 val;
    u16 result;
    u8 flag;
    ctx = fn_801F025C(0x11, 0);
    fn_80207BF4(ctx);
    val = fn_80205184(ctx);
    result = (u16)fn_8011BEB4(0, val, 9, 0);
    flag = 0;
    if (result == 0xc9) {
        if ((u8)fn_802025B8(ctx, PC_PTR(0x38)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(ctx, PC_PTR(0x38), 0);
            lbl_80478D78[5] = 0;
            flag = 1;
        }
    } else {
        if ((u8)fn_802025B8(ctx, PC_PTR(0x39)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(ctx, PC_PTR(0x39), 0);
            lbl_80478D78[5] = 1;
            flag = 1;
        }
    }
    if (flag != 0) {
        lbl_8047B610 += 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214AB4 size 0x48: wsCmdE7 (owner line 2776) */
#pragma optimize_for_size reset
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
void wsCmdE7(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F37B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightCheckForecastSub();
#endif
    u8 local = 0;
    fn_801F37B0(0, _fightCheckForecastSub, &local, 0);
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif

/* 0x80214AFC size 0x5C: wsCmdE6 (owner line 498) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
void wsCmdE6(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonIsPokemonJoutaiDataId(u32, u16);
#endif
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR32(fn_801F025C(0x19, 0)), PC_PTR(0x14)) == 0) {
        lbl_80379F58[0x1609B] |= 0x80;
    }
    lbl_8047B610++;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214B58 size 0x10: wsCmdE5 (owner line 202) */
u8* wsCmdE5(void) { return lbl_8047B610++; }

/* 0x80214B68 size 0x4C: WS_HIMITUNOTIKARA (owner line 13108) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma opt_propagation off
#define fn_801F54A4 fightFloorGetStatus
#define fn_80136468 tikeiDataBiosGetFightKoukaId
void WS_HIMITUNOTIKARA(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80136468();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
    extern u8 lbl_80478D78[1];
  u16 uVar1;
  u8 uVar2;
  u32 pc;
  u8* status;
  uVar1 = fn_801F54A4(0,0,0xf,0);
  uVar2 = fn_80136468(uVar1);
  status = lbl_80478D78;
  pc = *(volatile u32*)&lbl_8047B610;
  status[3] = uVar2;
  lbl_8047B610 = (u8*)PC_PTR((pc + 1));
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80136468
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x80214BB4 size 0x50: wsCmdE3 (owner line 2789) */
#pragma opt_propagation reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_802062FC fightOutPokemonCheckFightOut
void wsCmdE3(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));
    if (fn_802062FC(PC_PTR(ctx)) == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 2);
    } else {
        lbl_8047B610 = lbl_8047B610 + 6;
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214C04 size 0xAC: wsCmdE2 (owner line 2948) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_801252E0 pokemonInitJoutai
#define fn_80202998 fightOutPokemonResetSeqStatus
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
void wsCmdE2(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801252E0();
#endif
    extern void fn_80202998();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));
    u32 tmp = PC_PTR32_IMPLICIT(fn_80205B8C(ctx));
    if ((u16)fn_80207BF4(PC_PTR(ctx)) == ABILITY_NATURAL_CURE) {
        fn_801252E0(PC_PTR(tmp));
        fn_80202998(ctx, 0);
        fn_80202810(PC_PTR(ctx), PC_PTR(0x17));
        if (fn_801FECD4(PC_PTR(ctx)) == 1) {
            fn_801FE7EC(PC_PTR(ctx), 0x7c, 0, 0);
        }
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202998
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801252E0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214CB0 size 0x4C: wsCmdE1 (owner line 2806) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
void wsCmdE1(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F37B0();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x19, 0));
    fn_801F37B0(0, _wsAnnounceEnemyTokuseiSub, PC_PTR(ctx), 0);
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214CFC size 0xB4: _wsAnnounceEnemyTokuseiSub (owner line 13138) */
/* RULE-EXCEPTION(user-approved): local optimize_for_size / dont_inline / opt_propagation pragmas; _wsAnnounceEnemyTokuseiSub needs all three and shares its unit with 46 functions that do not - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#pragma opt_propagation off
#define fn_800FA280 GSmsgGetGSchar
#define fn_8011CB54 pokemonTokuseiDataBiosGetName
#define fn_8011CB6C pokemonTokuseiDataBiosGetPtr
#define fn_80132A38 msgctrlSetValue
#define fn_801F4C14 fightFloorSetStatus
#define fn_80202B88 fightOutPokemonIsAlly
#define fn_802062FC fightOutPokemonCheckFightOut
u32 _wsAnnounceEnemyTokuseiSub(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800FA280();
#endif
    extern u32 fn_8011CB54();
    extern u32 fn_8011CB6C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    extern u8 fn_80202B88();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
  extern u8 lbl_8037960A[];
  extern void* lbl_8047B62C;
  u32 ctx;
  u32 other;

  ctx = r3;
  other = r5;
  {
    u32 value;

    value = fightOutPokemonGetTokuseiDataId(PC_PTR(other));
    if (fn_802062FC(PC_PTR(ctx)) == 0) {
      return 1;
    }
    if (fn_80202B88(ctx,other) == 0) {
      fn_801F4C14(0,0,0x42,0,ctx);


      value = PC_PTR32_IMPLICIT(fn_800FA280(fn_8011CB54(fn_8011CB6C(value)))); /* ability record -> name key -> text */
      fn_80132A38(0xd,value);
      wsRunScript(lbl_8047B62C,lbl_8037960A,0);
    }
  }
  return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202B88
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011CB6C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011CB54
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_800FA280
#endif

/* 0x80214DB0 size 0xA0: wsCmdE0 (owner line 2610) */
#pragma optimize_for_size reset
#pragma opt_propagation reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801254B4 pokemonSetStatus
#define fn_801F221C fightFloorIsLastActionFightOutPokemon
void wsCmdE0(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801254B4();
#endif
    extern u8 fn_801F221C();
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    fn_801254B4(PC_PTR(ctx), 0, 0x118, 0, 1);

    if (fn_801F221C(0) != 1) {
        goto check2;
    }
deref:
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    return;
check2:
    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x33)) != 2) {
        goto deref;
    }
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x33), 0);
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F221C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214E50 size 0xC0: wsCmdDF (owner line 2638) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
#define fn_801254B4 pokemonSetStatus
#define fn_801F221C fightFloorIsLastActionFightOutPokemon
void wsCmdDF(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801254B4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    extern u8 fn_801F221C();
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    fn_801F4C14(0, 0, 0x43, 0, ctx);
    fn_801254B4(PC_PTR(ctx), 0, 0x118, 0, 1);

    if (fn_801F221C(0) != 1) {
        goto check2;
    }
deref:
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    return;
check2:
    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x37)) != 2) {
        goto deref;
    }
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x37), 0);
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F221C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80214F10 size 0xF8: wsCmdDE (owner line 5462) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4354 fightFloorGetFightOutPokemonPtrToFightTrainerPtr
#define fn_801F4C14 fightFloorSetStatus
void wsCmdDE(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F4354();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    u32 ctx11;
    u16 id;
    u32 trainerCtx;
    s32 divisor;
    u16 val;
    u32 result;
    u16 buf[0x1C];

    id = (u16)fn_801F54A4(0, 0, 0x14, 0);
    ctx11 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    trainerCtx = PC_PTR32_IMPLICIT(fn_801F4354(0, ctx11));
    divisor = fightSeqGetTemotiPokemonNekonoteWazaDataAry(trainerCtx, buf, 0x18, ctx11);
    if (divisor != 0) {
        val = buf[(u16)GSrand() % divisor];
        ServerStatusFlag = ServerStatusFlag & ~0x400;
        lbl_8047B60C = val;
        result = fightSeqSelectTarget(ctx11, val, id, 0, 1, 1, -1);
        fn_801F4C14(0, 0, 0x43, 0, result);
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4354
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x80215008 size 0x1B8: fightSeqGetTemotiPokemonNekonoteWazaDataAry (owner line 13185) */
#pragma optimize_for_size reset
/* RULE-EXCEPTION(user-approved): local optimize_for_size / dont_inline pragmas; fightSeqGetTemotiPokemonNekonoteWazaDataAry needs them and shares its unit with 46 functions that do not - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
/* NAME-T1 fn_80215008 */
int fightSeqGetTemotiPokemonNekonoteWazaDataAry(u32 trainer, u16* buffer, u32 maxCount, u32 context)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u32, u32, u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u32, u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerCheckTemotiPokemonFightEntry(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightPokemonCheckFightOut(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaCheckValid(u32, u16);
#endif
    extern u16 YubihuriNGTbl[];
    u32 excludedPokemon;
    u32 fightPokemon;
    u32 heroPokemon;
    u32 pokemon;
    u16 moveId;
    s32 moveIndex;
    u16 tableMove;
    u8 blocked;
    s32 capacity;
    s32 heroIndex;
    u32 trainerData;
    s32 clearIndex;
    s32 count;
    u32 tableOffset;

    trainerData = fightTrainerGetStatus(PC_PTR(trainer), 0, 0x44, 0);
    excludedPokemon = pokemonGetStatus(PC_PTR(context), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    capacity = maxCount & 0xffff;
    for (clearIndex = 0; clearIndex < capacity; clearIndex++) {
        buffer[clearIndex] = 0;
    }

    count = 0;
    heroIndex = 0;
    do {
        heroPokemon = heroGetStatus(PC_PTR(trainerData), 3, heroIndex);
        fightPokemon = PC_PTR32_IMPLICIT(fightTrainerCheckTemotiPokemonFightEntry(trainer, heroPokemon));
        if (fightPokemon != 0 && fightPokemonCheckFightOut(PC_PTR(fightPokemon)) != 0 &&
            excludedPokemon != fightPokemon) {
            pokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(fightPokemon));
            moveIndex = 0;
            do {
                if (pokemonWazaCheckValid(PC_PTR(pokemon), moveIndex) != 0) {
                    moveId = pokemonGetStatus(PC_PTR((unsigned long)pokemon), 0, POKEMON_STATUS_MOVE, moveIndex);
                    if (moveId == 0 || moveId == 0x165 || moveId == 0xd6 ||
                        moveId == 0x112 || moveId == 0x77 || moveId == 0x76) {
                        blocked = 1;
                    } else {
                        blocked = 0;
                    }
                    if (blocked == 0) {
                        tableOffset = 0;
                        while ((tableMove = YubihuriNGTbl[tableOffset]) != 0xffff) {
                            if (moveId == tableMove) {
                                break;
                            }
                            tableOffset++;
                        }
                        if (tableMove == 0xffff && moveId != 0 && moveId != 0x165 &&
                            count < capacity) {
                            buffer[count] = moveId;
                            count++;
                        }
                    }
                }
                moveIndex++;
            } while (moveIndex < 4);
        }
        heroIndex++;
    } while (heroIndex < 6);
    return count;
}

/* 0x802151C0 size 0xE8: wsCmdDD (owner line 10061) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80203D3C figthOutPokemonGetPokemonDataId
#define fn_8011BBD8 wazaSetStatus
void wsCmdDD(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80203D3C(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    extern u16 KetaguriTbl[];
    void* ctx1 = fn_801F025C(0x11, 0);
    void* obj = PC_PTR(fn_8012640C(ctx1, 0, 0xD9, 0));
    void* ctx2 = fn_801F025C(0x12, 0);
    u32 level = fn_80203D3C(ctx2);
    u16 result = (u16)(u32)fn_8012640C(0, level, 0x5F, 0);
    u16 i;
    u16 thr;
    u16 val;

    for (i = 0; (thr = KetaguriTbl[i]) != 0xFFFF; i += 2) {
        if (thr > result) break;
    }
    val = (thr != 0xFFFF) ? KetaguriTbl[i + 1] : 0x78;
    fn_8011BBD8(obj, 0, 0x2f, 0, val);
    lbl_8047B610 += 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203D3C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802152A8 size 0x58: fightSeqGetKetaguriIryoku (owner line 9237) */
u16 fightSeqGetKetaguriIryoku(u16 weight) {
    extern u16 KetaguriTbl[];
    u16 i;
    u16 thr;
    for (i = 0; (thr = KetaguriTbl[i]) != 0xFFFF; i += 2) {
        if (thr > weight) break;
    }
    if (thr != 0xFFFF) return KetaguriTbl[i + 1];
    return 0x78;
}

/* fightTargetGetPtrAsNowFightType returns fightTargetGetPtr's result (retail
 * leaves it in r3); fight_target.c's definition is typed void. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetPtrAsNowFightType(u32 slotType, u32 idx);
#endif
extern u16 fightOutPokemonGetUseWazaDataId(void* ctx);
extern void fightOutPokemonWriteJoutaiDataId(void* ctx, void* typeObj);

/* 0x80215300 size 0x74: WS_ONNEN (owner line 10091) */
void WS_ONNEN(void) {
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));

    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x28)) == 2) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x28), 0);
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
}

/* 0x80215374 size 0x1B4: wsCmdDB (owner line 13260) */
void wsCmdDB(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
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
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    extern u32 fightCheckTargetHasWaza();
    u32 pokemon;
    u32 stateMask;
    u32 amount;
    u32 pokemonData;
    u32 newValue;
    u8 currentValue;
    u8 slot;
    s8 moveSlot;
    s32 signedMoveSlot;
    s32 loopState[2];

    pokemon = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    if (fn_802025B8(PC_PTR(pokemon), PC_PTR(0x27)) != 2) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }

    if (pokemon != 0) {
        amount = fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId(
            0, ABILITY_PRESSURE, 2, pokemon);
        if (amount != 0) {
            pokemonData = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(pokemon));
            moveSlot = pokemonSearchWazaDataId(PC_PTR(pokemonData), MOVE_IMPRISON);
            if (moveSlot >= 0) {
                signedMoveSlot = moveSlot;
                currentValue = pokemonGetStatus(PC_PTR(pokemonData), 0, POKEMON_STATUS_MOVE_PP, signedMoveSlot);
                newValue = amount < currentValue ? (u8)(currentValue - amount) : 0;
                pokemonSetStatus(PC_PTR(pokemonData), 0, POKEMON_STATUS_MOVE_PP, signedMoveSlot, (u8)newValue);
                slot = moveSlot;
                if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(0x10)) == 0 &&
                    fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(0x31)) == 1) {
                    stateMask = fightOutPokemonGetPokemonJoutai08(pokemon, 0x31);
                    if ((stateMask & (1 << slot)) == 0 &&
                        fightOutPokemonIsUseHensinBuff(PC_PTR(pokemon)) == 1) {
                        fightOutPokemonSetHensinPokemonStatusId(
                            PC_PTR(pokemon), 0x80, slot, 0);
                    }
                }
            }
        }
    }

    loopState[0] = pokemon;
    loopState[1] = 0;
    fightFloorLoopValidFightOutPokemon(0, fightCheckTargetHasWaza, loopState, 0);
    if (loopState[1] == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(pokemon), PC_PTR(0x27), 0);
        lbl_8047B610 += 5;
    }
}

/* 0x80215528 size 0xEC: fightCheckTargetHasWaza (owner line 13331) */
u32 fightCheckTargetHasWaza(u32 r3, u32 r4, u32* r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaCheckValid();
#endif
    extern u8 fightOutPokemonIsAlly();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
  u32 otherPokemon;
  u32 other;
  s32 value;
  u32 pokemon;
  s8 slot;
  s8 moveSlot;
  u16 move;

  other = *r5;
  value = r3;
  if (fightOutPokemonCheckFightOut(PC_PTR(r3)) == 0) {
    return 1;
  }
  if (fightOutPokemonIsAlly(value,other) == 1) {
    return 1;
  }
  otherPokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(other));
  pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(value)));
  for (slot = 0; slot < 4; slot++) {
    value = (int)slot;
    if (pokemonWazaCheckValid(PC_PTR(otherPokemon),value) == 0) {
      continue;
    }
    move = (int)pokemonGetStatus(PC_PTR(otherPokemon),0,POKEMON_STATUS_MOVE,value);
    moveSlot = pokemonSearchWazaDataId(PC_PTR(pokemon),move);
    if (moveSlot < 0) {
      continue;
    }
    r5[1] = 1;
    return 0;
  }
  return 1;
}

/* 0x80215614 size 0x10C: wsCmdDA (owner line 13380) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_802096E8 fightWazaIsHit
#define fn_80207BC0 fightOutPokemonSetTokuseiDataId
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
void wsCmdDA(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184();
#endif
    extern void fn_80207BC0();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802096E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightCheckAccuracyRules();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4();
#endif
  u32 uVar1;
  u32 uVar2;
  u32 uVar5;
  u32 uVar3;
  u32 uVar6;
  u32 uVar4;
  u8 cVar7;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  uVar3 = fn_80207BF4(PC_PTR(uVar1));
  uVar4 = fn_80205184((void*)PC_PTR(uVar1));
  uVar5 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  uVar6 = fn_80207BF4(PC_PTR(uVar5));
  if (((((uVar3 & 0xffff) == 0) && ((uVar6 & 0xffff) == 0)) || ((u16)uVar3 == ABILITY_WONDER_GUARD)) ||
     ((((uVar6 & 0xffff) == 0x19 || (cVar7 = fn_802096E8(PC_PTR(uVar2)), cVar7 == 0)) ||
      (cVar7 = fightCheckAccuracyRules(uVar4,uVar1,uVar5), cVar7 == 1)))) {
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    return;
  }
  fn_80207BC0(uVar1,uVar6);
  fn_80207BC0(uVar5,uVar3);
  lbl_8047B610 = lbl_8047B610 + 5;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BC0
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

/* 0x80215720 size 0xE8: wsCmdD9 (owner line 13428) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_8011BEB4 wazaGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_8011BBD8 wazaSetStatus
void wsCmdD9(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
    extern u32 ServerStatusFlag;
  u32 uVar1;
  u32 uVar2;
  u16 uVar3;
  u32 uVar4;
  u16 uVar5;
  u16 uVar6;

  if ((ServerStatusFlag & 0x200) == 0) {
    uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
    uVar2 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
    fn_80205184((void*)PC_PTR(uVar1));
    uVar3 = fn_8011BEB4(PC_PTR(uVar2),0,0x2f,0);
    uVar4 = PC_PTR32_IMPLICIT(fn_80205B8C(uVar1));
    uVar5 = (int)fn_8012640C(PC_PTR(uVar4),0,0x83,0);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(uVar1));
    uVar6 = (int)fn_8012640C(PC_PTR(uVar1),0,0x87,0);
    uVar3 = (u16)((s32)(uVar3 * uVar5) / (s32)uVar6);
    if (uVar3 == 0) {
      uVar3 = 1;
    }
    fn_8011BBD8(PC_PTR(uVar2),0,0x2f,0,uVar3);
  }
  *(u32*)&lbl_8047B610 = *(u32*)&lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
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

/* 0x80215808 size 0xC8: wsCmdD8 (owner line 2670) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level reset; optimize_for_size reset; scheduling reset; dont_inline reset; opt_lifetimes reset; opt_propagation reset; opt_common_subs reset; switch_tables reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level reset
#pragma optimize_for_size reset
#pragma scheduling reset
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma switch_tables reset
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_8011BBD8 wazaSetStatus
void wsCmdD8(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    u32 poke1 = PC_PTR32_IMPLICIT(fn_80205B8C(ctx1));
    s32 statA = (u16)fn_8012640C(PC_PTR(poke1), 0, 0x83, 0);

    u32 ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12, 0));
    u32 poke2 = PC_PTR32_IMPLICIT(fn_80205B8C(ctx2));
    s32 statB = (u16)fn_8012640C(PC_PTR(poke2), 0, 0x83, 0);

    if (statB <= statA) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    } else {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, statB - statA);
        lbl_8047B610 = lbl_8047B610 + 5;
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802158D0 size 0x84: WS_AKUBI (owner line 10135) */
#pragma optimize_for_size reset
void WS_AKUBI(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsJoutaiNormal();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));

    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x26)) != 2) {
        goto deref;
    }
    if (fightOutPokemonIsJoutaiNormal(PC_PTR(ctx)) != 0) {
        goto plus5;
    }
deref:
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    return;
plus5:
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x26), 0);
    lbl_8047B610 = lbl_8047B610 + 5;
}

/* 0x80215954 size 0x124: wsCmdD6 (owner line 13466) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_8011BEB4 wazaGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_8011BBD8 wazaSetStatus
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_801F54A4 fightFloorGetStatus
void wsCmdD6(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
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
  u32 uVar1;
  u32 uVar2;
  u16 uVar5;
  short sVar4;
  short sVar6;
  u16 uVar7;
  u32 uVar8;
  u16 uVar3;

  uVar3 = fn_801F54A4(0,0,0x14,0);
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = (u32)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  sVar4 = (int)fn_8012640C(PC_PTR(uVar1),0,0x102,0);
  uVar5 = (int)fn_8012640C(PC_PTR(uVar1),0,0x103,0);
  sVar6 = (int)fn_8012640C(PC_PTR(uVar1),0,0x104,0);
  uVar7 = (int)fn_8012640C(PC_PTR(uVar1),0,0x105,0);
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  uVar8 = fn_801F0134(uVar1,uVar3);
  if (((sVar4 != 0) && (uVar5 == (u16)uVar8)) ||
      ((sVar6 != 0) && (uVar7 == (u16)uVar8))) {
    fn_8011BBD8(PC_PTR(uVar2),0,0x2c,0,2);
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F0134
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
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

/* 0x80215A78 size 0x74: WS_NEWOHARU (owner line 10124) */
void WS_NEWOHARU(void) {
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));

    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x25)) != 2) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x25), 0);
        lbl_8047B610 = lbl_8047B610 + 5;
    }
}

/* 0x80215AEC size 0x184: wsCmdD4 (owner line 13507) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmdD4(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai08();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonMaxHpWaruValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
    extern u8 fightOutPokemonIsHpMantan();

    switch (lbl_8047B610[1]) {
    case 0: {
        u32 value;
        u32 ctx;
        u32 finalValue;

        ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
        value = pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
        value = pokemonGetStatus(PC_PTR(value), 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0);
        finalValue = pokemonGetStatus(PC_PTR(value), 0, POKEMON_STATUS_NICKNAME, 0);
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x35)) != 2) {
            lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 2);
        } else {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x35), 0);
            fightOutPokemonSetPokemonJoutai08(PC_PTR(ctx), PC_PTR(0x35), PC_PTR(finalValue));
            lbl_8047B610 += 6;
        }
        break;
    }
    case 1: {
        u32 pokemon;
        u32 defender;
        u32 attacker;

        attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
        pokemon = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
        defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
        msgctrlSetValue(0xd, fightOutPokemonGetPokemonJoutai08(defender, 0x35));
        wazaSetStatus(PC_PTR(pokemon), 0, 0x2d, 0,
                      -(s32)(u16)fightOutPokemonMaxHpWaruValue(PC_PTR(defender), 2));
        if (fightOutPokemonIsHpMantan(defender) == 1) {
            lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 2);
        } else {
            lbl_8047B610 += 6;
        }
        break;
    }
    }
}

/* 0x80215C70 size 0x80: WS_NARIKIRI (owner line 13563) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_80207BC0 fightOutPokemonSetTokuseiDataId
void WS_NARIKIRI(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4();
#endif
    extern void fn_80207BC0();
  u32 uVar1;
  u32 uVar2;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  fn_80207BF4(PC_PTR(uVar1));

  uVar2 = fn_80207BF4(fn_801F025C(0x12,0));
  if (((uVar2 & 0xffff) == 0) || ((uVar2 & 0xffff) == 0x19)) {
    goto jump;
  }
  fn_80207BC0(uVar1,uVar2);
  lbl_8047B610 += 5;
  return;
jump:
  lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BC0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80215CF0 size 0x358: wsCmdD2 (owner line 13589) */
void wsCmdD2(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetSoubiItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsGcHeroFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80142984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonDoItemSoubi();
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
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
    extern u8 lbl_803798D8[];
    u32 attacker;
    u32 defender;
    u32 attackerItem;
    u32 ability;
    u32 defenderItem;
    u8 proceed;

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    attackerItem = fightOutPokemonGetSoubiItemDataId(PC_PTR(attacker));
    defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    ability = fightOutPokemonGetTokuseiDataId(PC_PTR(defender));
    defenderItem = fightOutPokemonGetSoubiItemDataId(PC_PTR(defender));

    if ((u8)fightFloorGetStatus(0, 0, 0x2f, 0) == 1 &&
        fightOutPokemonIsGcHeroFightOutPokemon(attacker) == 0) {
        proceed = 0;
    } else if ((u8)fightFloorGetStatus(0, 0, 0x2f, 0) == 1 &&
               fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x3d)) == 1) {
        proceed = 0;
    } else {
        proceed = 1;
    }

    if (proceed == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }

    if (fightFloorGetStatus(0, 0, 0x2f, 0) == 0) {
        goto second_check_true;
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x3d)) != 0) {
        goto second_check_false;
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(defender), PC_PTR(0x3d)) == 0) {
        goto second_check_true;
    }
second_check_false:
    proceed = 0;
    goto second_check_done;
second_check_true:
    proceed = 1;
second_check_done:

    if (proceed == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }

    if (!(((u16)attackerItem != 0 || (u16)defenderItem != 0) &&
          (u16)attackerItem != 0xaf && (u16)defenderItem != 0xaf &&
          ((u16)attackerItem == 0 || fn_80142984(attackerItem) != 0) &&
          ((u16)defenderItem == 0 || fn_80142984(defenderItem) != 0))) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }

    {
        if ((u16)ability == ABILITY_STICKY_HOLD) {
            lbl_8047B610 = lbl_803798D8;
            return;
        }

        if ((u16)attackerItem != 0) {
            fightOutPokemonDoItemSoubi(PC_PTR(attacker), 0, 0, 0);
        }
        if ((u16)defenderItem != 0) {
            fightOutPokemonDoItemSoubi(PC_PTR(defender), 0, 0, 0);
        }
        if ((u16)attackerItem != 0) {
            fightOutPokemonDoItemSoubi(PC_PTR(defender), attackerItem, 1, 0);
        }
        if ((u16)defenderItem != 0) {
            pokemonSetStatus(PC_PTR(attacker), 0, 0xfb, 0, (u16)defenderItem);
        }

        if (fightOutPokemonIsUseHensinBuff(PC_PTR(attacker)) == 1) {
            fightOutPokemonSetHensinPokemonStatusId(PC_PTR(attacker), 0x82, 0, 0);
        }
        if (fightOutPokemonIsUseHensinBuff(PC_PTR(defender)) == 1) {
            fightOutPokemonSetHensinPokemonStatusId(PC_PTR(defender), 0x82, 0, 0);
        }
        fightOutPokemonWriteJoutaiDataId(PC_PTR(attacker), PC_PTR(CONDITION_CHOICE_LOCK));
        fightOutPokemonWriteJoutaiDataId(PC_PTR(defender), PC_PTR(CONDITION_CHOICE_LOCK));

        msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(itemGetStatus(0, defenderItem, 1, 0))));
        msgctrlSetValue(0xe, PC_PTR32_IMPLICIT(GSmsgGetGSchar(itemGetStatus(0, attackerItem, 1, 0))));

        if ((u16)attackerItem == 0) {
            goto one_item_missing;
        }
        if ((u16)defenderItem == 0) {
            goto one_item_missing;
        }
        lbl_80478D78[5] = 2;
        goto item_state_done;
one_item_missing:
        if ((u16)defenderItem == 0) {
            goto defender_item_missing;
        }
        lbl_80478D78[5] = 0;
        goto item_state_done;
defender_item_missing:
        lbl_80478D78[5] = 1;
item_state_done:
        lbl_8047B610 += 5;
        return;
    }
}

/* 0x80216048 size 0xA4: fightAiCanStealItem (owner line 3025) */
#define fn_801F54A4 fightFloorGetStatus
#define fn_80204A10 fightOutPokemonIsGcHeroFightOutPokemon
u8 fightAiCanStealItem(u32 elem) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80204A10();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    if ((u8)fn_801F54A4(0, 0, 0x2f, 0) == 1) {
        if (fn_80204A10(elem) == 0) {
            return 0;
        }
    }
    if ((u8)fn_801F54A4(0, 0, 0x2f, 0) == 1) {
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(elem), PC_PTR(0x3d)) == 1) {
            return 0;
        }
    }
    return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80204A10
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x802160EC size 0x104: wsCmdD1 (owner line 3080) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F54A4 fightFloorGetStatus
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_801F4C14 fightFloorSetStatus
void wsCmdD1(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0xe, ctx1));

    if (ctx2 != 0 &&
        (u16)fn_801F54A4(0, 0, 0x19, 0) >= 2 &&
        fn_802062FC(PC_PTR(ctx2)) == 1 &&
        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x32)) == 0 &&
        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx2), PC_PTR(0x32)) == 0) {
        fn_801F4C14(0, 0, 0x43, 0, ctx2);
        if (fn_802025B8(PC_PTR(ctx2), PC_PTR(0x32)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx2), PC_PTR(0x32), 0);
        }
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802161F0 size 0x74: WS_CHOUHATSU (owner line 10102) */
#pragma optimize_for_size reset
void WS_CHOUHATSU(void) {
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));

    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x30)) != 2) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x30), 0);
        lbl_8047B610 = lbl_8047B610 + 5;
    }
}

/* 0x80216264 size 0x8C: wsCmdCF (owner line 2921) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level reset; optimize_for_size reset; scheduling reset; dont_inline reset; opt_lifetimes reset; opt_propagation reset; opt_common_subs reset; switch_tables reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level reset
#pragma optimize_for_size reset
#pragma scheduling reset
#pragma dont_inline reset
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma switch_tables reset
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
void wsCmdCF(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    s16 val1 = (s16)fn_8012640C(PC_PTR(ctx), 0, 0x102, 0);
    s16 val2 = (s16)fn_8012640C(PC_PTR(ctx), 0, 0x104, 0);
    if (val1 != 0 || val2 != 0) {
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802162F0 size 0x74: WS_ICHAMON (owner line 10113) */
#pragma optimize_for_size reset
void WS_ICHAMON(void) {
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));

    if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x1b)) != 2) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x1b), 0);
        lbl_8047B610 = lbl_8047B610 + 5;
    }
}

/* 0x80216364 size 0xAC: wsCmdCD (owner line 2990) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level reset; optimize_for_size reset; scheduling reset; dont_inline reset; opt_lifetimes reset; opt_propagation reset; opt_common_subs reset; switch_tables reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level reset
#pragma optimize_for_size reset
#pragma scheduling reset
#pragma dont_inline reset
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma switch_tables reset
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80203C5C fightOutPokemonIsJoutaiKaragenki
#define fn_801252E0 pokemonInitJoutai
#define fn_80202998 fightOutPokemonResetSeqStatus
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
void wsCmdCD(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801252E0();
#endif
    extern void fn_80202998();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h (fightOutPokemonIsJoutaiKaragenki) */
    extern u8 fn_80203C5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 tmp = PC_PTR32_IMPLICIT(fn_80205B8C(ctx));
    if (fn_80203C5C(PC_PTR(ctx)) == 1) {
        fn_801252E0(PC_PTR(tmp));
        fn_80202998(ctx, 0);
        lbl_8047B610 = lbl_8047B610 + 5;
        if (fn_801FECD4(PC_PTR(ctx)) == 1) {
            fn_801FE7EC(PC_PTR(ctx), 0x7c, 0, 0);
        }
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202998
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801252E0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203C5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80216410 size 0x140: wsCmdCC (owner line 13727) */
#pragma optimize_for_size reset
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_801363E8 tikeiDataBiosGetWazaId
#define fn_8011BEB4 wazaGetStatus
#define fn_80209C1C fightWazaSetUseWazaStatus
#define fn_800FA280 GSmsgGetGSchar
#define fn_80132A38 msgctrlSetValue
#define fn_801F4C14 fightFloorSetStatus
void wsCmdCC(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800FA280();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
    extern u32 fn_801363E8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
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
    extern void fn_80209C1C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSeqSelectTarget();
#endif
    extern u32 ServerStatusFlag;
    extern void* lbl_8047B62C;
    extern u8 lbl_80377EA4[];
    extern u32 lbl_80379BFF[];
  u16 uVar5;
  u32 uVar1;
  u32 uVar2;
  u32 uVar3;
  u32 uVar4;
  u16 uVar6;
  u32 uVar7;

  uVar5 = fn_801F54A4(0,0,0x14,0);
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  uVar6 = fn_801F54A4(0,0,0xf,0);
  uVar3 = fn_801363E8(uVar6);
  uVar4 = fn_8011BEB4(0,uVar3,9,0);
  fn_80209C1C(uVar2,uVar3);
#ifdef TARGET_PC /* real argument read from the assembly (the key wazaGetStatus returns, r3 unmasked); no GameCube-identical form found yet */
  uVar2 = PC_PTR32_IMPLICIT(fn_800FA280(fn_8011BEB4(0,uVar3,1,0)));
#else
  fn_8011BEB4(0,uVar3,1,0);
  uVar2 = fn_800FA280();
#endif
  fn_80132A38(0x28,uVar2);
  uVar7 = fightSeqSelectTarget(uVar1,uVar3,uVar5,0,1,1, PC_PTR32_IMPLICIT((void*)0xffffffff));
  fn_801F4C14(0,0,0x43,0,uVar7);
  ServerStatusFlag = ServerStatusFlag & 0xfffffbff;
  wsRunScript(lbl_8047B62C,PC_PTR32(lbl_80377EA4),0);
      lbl_8047B610 = (u8*)PC_PTR(DISC_U32(&lbl_80379BFF[(uVar4 & 0xffff)]));
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_800FA280
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80209C1C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801363E8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x80216550 size 0x64: WS_JUUDEN (owner line 13780) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
void WS_JUUDEN(void)

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

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  cVar2 = fn_802025B8(PC_PTR(uVar1),PC_PTR(CONDITION_CHARGE));
  if (cVar2 == 2) {
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(CONDITION_CHARGE),0);
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802165B4 size 0x9C: wsCmdCA (owner line 2563) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E44 fightSideCheckWriteJoutaiDataId
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_801F6DF0 fightSideWriteJoutaiDataId
#define fn_8012640C pokemonGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80120B00 pokemonGetMezamerupower
#define fn_8011BBD8 wazaSetStatus
void wsCmdCA(void) {
    extern u8 fn_801F6E44(u32, u32);
    extern void fn_801F6DF0(u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F0134();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 tmp = PC_PTR32_IMPLICIT(fn_801F025C(2, ctx1));

    if (fn_801F6E44(tmp, 0x4d) == 2) {
        u16 val = (u16)fn_801F54A4(0, 0, 0x14, 0);
        u32 val2 = fn_801F0134(ctx1, val);
        fn_801F6DF0(tmp, 0x4d, val2);
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80120B00
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6DF0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F0134
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E44
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80216650 size 0x130: wsCmdC9 (owner line 9675) */
#pragma optimize_for_size reset
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_802656AC fightMenuGetFightOutPokemonPtrToStatusMenuId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_8011BBD8 wazaSetStatus
void wsCmdC9(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802656AC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
    extern void menuFightStatusSetHP();
    u32 ctx1;
    u32 fieldD9;
    u32 ctx2;
    u8 flagA;
    u32 result;
    u8 flagB;
    u16 val = (u16)fn_801F54A4(0, 0, 0x14, 0);

    ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    result = fn_802656AC(ctx1, val, 1);
    ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12, 0));
    flagA = (u8)fn_8012640C(PC_PTR(ctx2), 0, 0xE6, 0);
    flagB = (u8)fn_8012640C(PC_PTR(ctx2), 0, 0xE8, 0);

    if (flagA == 0 && flagB == 0 && lbl_80478D78[6] != 1) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    } else {
        u16 poke_val = (u16)fn_8012640C(fn_80205B8C(PC_PTR(ctx1)), 0, 0x83, 0);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, poke_val);
        menuFightStatusSetHP(result, 0);
        lbl_8047B610 = lbl_8047B610 + 5;
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802656AC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x80216780 size 0x84: wsCmdC8 (owner line 5186) */
#define fn_801F2988 fightFloorCheckWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F2934 fightFloorWriteJoutaiDataId
void wsCmdC8(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F2988();
#endif
    extern void fn_801F2934();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    u8 val = (u8)fn_801F2988(0, 0x52);

    if (val != 2) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 2;
    } else {
        fn_801F2934(0, 0x52, 0);
        lbl_80478D78[5] = 5;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2934
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2988
#endif

/* 0x80216804 size 0x70: WS_CHIISAKUNARU (owner line 10153) */
void WS_CHIISAKUNARU(void) {
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));

    if ((ServerStatusFlag & 0x2000000) != 0) {
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(0x23)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x23), 0);
        }
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}

/* 0x80216874 size 0xEC: WS_KIERUTAME_AFTAR (owner line 10164) */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void WS_KIERUTAME_AFTAR(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    u16 moveId = fightOutPokemonGetUseWazaDataId(PC_PTR(ctx));

    switch (moveId) {
    case MOVE_FLY:
    case MOVE_BOUNCE:
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_IN_THE_AIR)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_IN_THE_AIR));
        }
        break;
    case MOVE_DIG:
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERGROUND)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERGROUND));
        }
        break;
    case MOVE_DIVE:
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERWATER)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERWATER));
        }
        break;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#else
void WS_KIERUTAME_AFTAR(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx = fightTargetGetPtrAsNowFightType(0x11, 0);
    u16 moveId = fightOutPokemonGetUseWazaDataId((void*)ctx);

    switch (moveId) {
    case MOVE_FLY:
    case MOVE_BOUNCE:
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, CONDITION_IN_THE_AIR) == 1) {
            fightOutPokemonWriteJoutaiDataId((void*)ctx, (void*)CONDITION_IN_THE_AIR);
        }
        break;
    case MOVE_DIG:
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, CONDITION_UNDERGROUND) == 1) {
            fightOutPokemonWriteJoutaiDataId((void*)ctx, (void*)CONDITION_UNDERGROUND);
        }
        break;
    case MOVE_DIVE:
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, CONDITION_UNDERWATER) == 1) {
            fightOutPokemonWriteJoutaiDataId((void*)ctx, (void*)CONDITION_UNDERWATER);
        }
        break;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#endif

/* 0x80216960 size 0xF8: WS_KIERUTAME (owner line 10190) */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void WS_KIERUTAME(void) {
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    u16 moveId = fightOutPokemonGetUseWazaDataId(PC_PTR(ctx));

    switch (moveId) {
    case MOVE_FLY:
    case MOVE_BOUNCE:
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(CONDITION_IN_THE_AIR)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_IN_THE_AIR), 0);
        }
        break;
    case MOVE_DIG:
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(CONDITION_UNDERGROUND)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERGROUND), 0);
        }
        break;
    case MOVE_DIVE:
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(CONDITION_UNDERWATER)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERWATER), 0);
        }
        break;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#else
void WS_KIERUTAME(void) {
    u32 ctx = fightTargetGetPtrAsNowFightType(0x11, 0);
    u16 moveId = fightOutPokemonGetUseWazaDataId((void*)ctx);

    switch (moveId) {
    case MOVE_FLY:
    case MOVE_BOUNCE:
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(CONDITION_IN_THE_AIR)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_IN_THE_AIR), 0);
        }
        break;
    case MOVE_DIG:
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(CONDITION_UNDERGROUND)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERGROUND), 0);
        }
        break;
    case MOVE_DIVE:
        if (fn_802025B8(PC_PTR(ctx), PC_PTR(CONDITION_UNDERWATER)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_UNDERWATER), 0);
        }
        break;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#endif
