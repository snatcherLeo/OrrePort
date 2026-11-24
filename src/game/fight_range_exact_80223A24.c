/**
 * @file fight_range_exact_80223A24.c
 * @brief Strict target-order fight island with compiler-owned jump table.
 */
#include "dolphin/types.h"
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
#ifndef TARGET_PC /* PC: fn_80202998 is fightOutPokemonResetSeqStatus (game_pc_alias.h), prototyped in wsCmd1A below */
extern void fn_80202998();
#endif
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



/* WS_CHECK_TYPE */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80207AE0 fightOutPokemonIsZokuseiDataId
void WS_CHECK_TYPE(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80207AE0();
#endif
  u32 uVar1;
  u8 cVar2;
  u8 typeId;
  u8* pc;
  u8* jumpTarget;
  u8* nextPc;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(*(u8 *)(lbl_8047B610 + 1),0));
  pc = lbl_8047B610;
  cVar2 = fn_80207AE0(PC_PTR(uVar1),
      (typeId = *(u8 *)(pc + 2),
       jumpTarget = (u8*)PC_PTR(DISC_U32(pc + 3)), typeId));
  if (cVar2 == 1) {
    nextPc = jumpTarget;
  }
  else {
    nextPc = lbl_8047B610 + 7;
  }
  lbl_8047B610 = nextPc;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207AE0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* WS_WAZAKOUKA_CHECK */
#define fn_801F025C fightTargetGetPtrAsNowFightType
void WS_WAZAKOUKA_CHECK(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
  u32 uVar1;
  u8 cVar2;

  u8* jumpTarget;
  u8* nextPc;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(*(u8 *)(lbl_8047B610 + 1),0));
  jumpTarget = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 7));
  cVar2 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(DISC_U32(lbl_8047B610 + 2) & 0xffff));
  if (*(u8 *)(lbl_8047B610 + 6) == cVar2) {
    nextPc = jumpTarget;
  }
  else {
    nextPc = lbl_8047B610 + 0xb;
  }
  lbl_8047B610 = nextPc;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* wsCmd20 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd20(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
    u32 ctx;
    u32 statusId;
    u8 result;
    s16 mode;
    s16 expected;
    u8 status;
    u8 *pc;

    result = 0;
    ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(lbl_8047B610[1], 0));
    mode = lbl_8047B610[2];

    switch (lbl_8047B610[3]) {
    case 1:
        statusId = 0xe6;
        break;
    case 2:
        statusId = 0xe7;
        break;
    case 3:
        statusId = 0xea;
        break;
    case 4:
        statusId = 0xe8;
        break;
    case 5:
        statusId = 0xe9;
        break;
    case 6:
        statusId = 0xeb;
        break;
    case 7:
        statusId = 0xec;
        break;
    default:
        statusId = 0;
        break;
    }

    status = (u8)pokemonGetStatus(PC_PTR(ctx), 0, statusId, 0);
    pc = lbl_8047B610;
    expected = (u8)pc[4];
    switch ((u8)(s8)mode) {
    case 0:
        if ((u8)status == (u8)expected) {
            result = 1;
        }
        break;
    case 1:
        if ((u8)status != (u8)expected) {
            result = 1;
        }
        break;
    case 2:
        if ((u8)status > (u8)expected) {
            result = 1;
        }
        break;
    case 3:
        if ((u8)status < (u8)expected) {
            result = 1;
        }
        break;
    case 4:
        if (((u8)status & (u8)expected) != 0) {
            result = 1;
        }
        break;
    case 5:
        if (((u8)status & (u8)expected) == 0) {
            result = 1;
        }
        break;
    }

    if (result != 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, pc + 5);
    } else {
        lbl_8047B610 += 9;
    }
}

/* WS_SIDECONDITION_CHECK */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E98 fightSideIsJoutaiDataId
void WS_SIDECONDITION_CHECK(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F6E98();
#endif
  u32 uVar1;
  u8 cVar2;
  u16 conditionId;
  u8* pc;
  u8* jumpTarget;
  u8* nextPc;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(*(u8 *)(lbl_8047B610 + 1),0));
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(2,uVar1));
  pc = lbl_8047B610;
  cVar2 = fn_801F6E98(uVar1,
      (conditionId = DISC_U16(pc + 2),
       jumpTarget = (u8*)PC_PTR(DISC_U32(pc + 4)), conditionId));
  if (cVar2 == 1) {
    nextPc = jumpTarget;
  }
  else {
    nextPc = lbl_8047B610 + 8;
  }
  lbl_8047B610 = nextPc;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E98
#endif

/* wsCmd1E */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_801F4C14 fightFloorSetStatus
void wsCmd1E(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207BF4(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    u8* pc = lbl_8047B610;
    void* ctx = fn_801F025C(pc[1], 0);
    u16 ret = fn_80207BF4(ctx);
    u8* target;

    pc = lbl_8047B610;
    target = DISC_PTR_AT(u8, pc + 3);
    if (ret == pc[2]) {
        fn_801F4C14(0, 0, 0x48, 0, PC_PTR32(ctx));
        lbl_8047B610 = target;
    } else {
        lbl_8047B610 = lbl_8047B610 + 7;
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif

/* wsCmd1D */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80203CCC fightOutPokemonIsJoutaiNormal
#define fn_80203C5C fightOutPokemonIsJoutaiKaragenki
#define fn_802062FC fightOutPokemonCheckFightOut
void wsCmd1D(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80203CCC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h (fightOutPokemonIsJoutaiKaragenki) */
    extern u8 fn_80203C5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802062FC();
#endif
    u8* target;
    u32 ctx;
    u16 kind;

    ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));
    kind = DISC_U32(lbl_8047B610 + 2);
    target = DISC_PTR_AT(u8, lbl_8047B610 + 6);

    if (kind == 1) {
        if ((u8)fn_80203CCC(PC_PTR(ctx)) == 0) {
            lbl_8047B610 = target;
        } else {
            lbl_8047B610 = lbl_8047B610 + 10;
        }
    } else if (kind == 2) {
        if ((u8)fn_80203C5C(PC_PTR(ctx)) == 1) {
            lbl_8047B610 = target;
        } else {
            lbl_8047B610 = lbl_8047B610 + 10;
        }
    } else {
        if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(kind)) == 1 && (u8)fn_802062FC(PC_PTR(ctx)) == 1) {
            lbl_8047B610 = target;
        } else {
            lbl_8047B610 = lbl_8047B610 + 10;
        }
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203C5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203CCC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* WS_CONDITION_CHECK */
#pragma scheduling reset
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma switch_tables reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80203CCC fightOutPokemonIsJoutaiNormal
#define fn_80203C5C fightOutPokemonIsJoutaiKaragenki
#define fn_802062FC fightOutPokemonCheckFightOut
void WS_CONDITION_CHECK(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h (fightOutPokemonIsJoutaiKaragenki) */
    extern u8 fn_80203C5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80203CCC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802062FC();
#endif
  u32 ctx;
  u8* target;
  u16 kind;

  ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8 *)(lbl_8047B610 + 1),0));
  kind = DISC_U32(lbl_8047B610 + 2);
  target = DISC_PTR_AT(u8, lbl_8047B610 + 6);
  if (kind == 1) {
    if ((u8)fn_80203CCC(PC_PTR(ctx)) == 0) {
      lbl_8047B610 = target;
    }
    else {
      lbl_8047B610 += 10;
    }
  }
  else if (kind == 2) {
    if ((u8)fn_80203C5C(PC_PTR(ctx)) == 1) {
      lbl_8047B610 = target;
    }
    else {
      lbl_8047B610 += 10;
    }
  }
  else {
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(kind)) == 1 &&
        (u8)fn_802062FC(PC_PTR(ctx)) == 1) {
      lbl_8047B610 = target;
    }
    else {
      lbl_8047B610 += 10;
    }
  }
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203C5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203CCC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* wsCmd1B */
#define fn_801252E0 pokemonInitJoutai
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
#define fn_801F4354 fightFloorGetFightOutPokemonPtrToFightTrainerPtr
#define fn_801F54A4 fightFloorGetStatus
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_80202998 fightOutPokemonResetSeqStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_8020912C fightOutPokemonKizetuEffect
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd1B(void) {
    extern void fightOutPokemonResetSeqStatus(void* ctx, u16 mode); /* PC: fn_80202998 (alias); the merged file
                                                                       declares it only further down */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
    u32 ctx;
    u16 count;

    count = (u16)fn_801F54A4(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));

    fn_801F4354(0, PC_PTR(ctx));
    fn_8020912C(PC_PTR(ctx), 2);
    fightMenuStatusMenuClose(ctx, count, 0);
    fn_8020912C(PC_PTR(ctx), 3);
    fn_801F37B0(0, fn_80232FE4, PC_PTR(ctx), 0);
    fn_801252E0(fn_80205B8C(PC_PTR(ctx)));
    fn_80202998(PC_PTR(ctx), 0);
    fn_80202810(PC_PTR(ctx), PC_PTR(0x17));

    if ((u8)fn_801FECD4(PC_PTR(ctx)) == 1) {
        fn_801FE7EC(PC_PTR(ctx), 0x7c, 0, 0);
    }
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x3e)) == 1) {
        fn_80202810(PC_PTR(ctx), PC_PTR(0x3e));
        if ((u8)fn_801FECD4(PC_PTR(ctx)) == 1) {
            fn_801FE7EC(PC_PTR(ctx), 0xc8, 0, 0);
        }
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#else
void wsCmd1B(void) {
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(void* obj, void* search_val);
    extern void fightOutPokemonKizetuEffect(void* r3, u8 r4);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuStatusMenuClose(u32 r3, u32 r4, u8 r5);
#endif
    extern u8 fightFloorLoopValidFightOutPokemon(void* floor, void* callback, void* buf, u8 flag);
    extern void pokemonInitJoutai(u8* ptr);
    extern void* fightOutPokemonGetPokemonPtr(void* ctx);
    extern void fightOutPokemonResetSeqStatus(void* ctx, u16 mode);
    extern void fightOutPokemonWriteJoutaiDataId(void* ctx, void* typeObj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
    u32 ctx;
    u16 count;

    count = (u16)fn_801F54A4(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));

    fn_801F4354(0, (void*)ctx);
    fn_8020912C((void*)ctx, 2);
    fightMenuStatusMenuClose(ctx, count, 0);
    fn_8020912C((void*)ctx, 3);
    fn_801F37B0(0, (void*)fn_80232FE4, (void*)ctx, 0);
    fn_801252E0(fn_80205B8C((void*)ctx));
    fn_80202998((void*)ctx, 0);
    fn_80202810((void*)ctx, (void*)0x17);

    if ((u8)fn_801FECD4(PC_PTR(ctx)) == 1) {
        fn_801FE7EC(PC_PTR(ctx), 0x7c, 0, 0);
    }
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(ctx, 0x3e) == 1) {
        fn_80202810((void*)ctx, (void*)0x3e);
        if ((u8)fn_801FECD4(ctx) == 1) {
            fn_801FE7EC(ctx, 0xc8, 0, 0);
        }
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801252E0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4354
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
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
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020912C
#endif

/* wsCmd1A */
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801FEF74 fightOutPokemonGetJoutaiMigawariHp
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_802086B0 fightOutPokemonFreeAllSequenceWaza
#define fn_8020F108 fightWazaWzxTypeFuncMigawari
#define fn_8020912C fightOutPokemonKizetuEffect
void wsCmd1A(void) {
    extern void fn_80265598(u32 r3, u32 r4, u32 r5);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_801FEF74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802086B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8020F108();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8020912C();
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
    u32 ctx;
    u16 count;

    count = (u16)fn_801F54A4(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));

    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x14)) == 1 && fn_801FEF74(PC_PTR(ctx)) > 0) {
        fn_80202810(PC_PTR(ctx), PC_PTR(0x14));
        fn_802086B0(PC_PTR(ctx));
        fn_8020F108(0xa4, ctx, ctx, 0, 0);
        do {
            if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                break;
            }
            _threadSwitch();
        } while (1);
    }
    fn_802086B0(PC_PTR(ctx));
    fn_8020912C(PC_PTR(ctx), 0);
    fn_8020912C(PC_PTR(ctx), 1);
    fn_80265598(ctx, count, 1);
    lbl_8047B610 = lbl_8047B610 + 2;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FEF74
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802086B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020F108
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020912C
#endif
