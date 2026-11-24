/**
 * @file fight_range_exact_80217018.c
 * @brief Strict target-order fight island, 0x80217018 - 0x80217D34.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/conditions.h"
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

/* 0x80217018 size 0x9C: wsCmdC1 (owner line 2495) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E44 fightSideCheckWriteJoutaiDataId
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_801F6DF0 fightSideWriteJoutaiDataId
#define fn_8012640C pokemonGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80120B00 pokemonGetMezamerupower
#define fn_8011BBD8 wazaSetStatus
void wsCmdC1(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
    extern void fn_80120B00();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx), 0, 0xD9, 0);
    u32 poke = PC_PTR32_IMPLICIT(fn_80205B8C(ctx));
    u16 outA, outB;

    fn_80120B00(poke, &outA, &outB);
    fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2f, 0, outA);
    fn_8011BBD8(PC_PTR(fieldD9), 0, 0x30, 0, outB);
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

/* 0x802170B4 size 0x108: wsCmdC0 (owner line 4949) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F453C fightFloorGetNowTenkouDataId
#define fn_80201704 fightOutPokemonIsHpMantan
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_8011BBD8 wazaSetStatus
void wsCmdC0(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F453C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203B5C();
#endif
    extern u8 fn_80201704();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    u8 sel;
    s32 val;

    fn_801F4C14(0, 0, 0x43, 0, ctx1);
    sel = (u8)fn_801F453C(0, 1);

    if (!fn_80201704(ctx1)) {
        if (sel == 0) {
            val = (u16)fn_80203B5C(PC_PTR(ctx1), 2);
        } else if (sel == 1) {
            val = (fn_80203B5C(PC_PTR(ctx1), 1) * 20) / 30;
        } else {
            val = (u16)fn_80203B5C(PC_PTR(ctx1), 4);
        }
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, -(s32)val);
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80201704
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F453C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802171BC size 0x64: WS_MARUKUNARU (owner line 12759) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
void WS_MARUKUNARU(void)

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
  cVar2 = fn_802025B8(PC_PTR(uVar1),PC_PTR(0x1a));
  if (cVar2 == 2) {
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x1a),0);
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80217220 size 0x1B4: wsCmdBE (owner line 9299) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_800FA280 GSmsgGetGSchar
#define fn_80132A38 msgctrlSetValue
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_801F6E98 fightSideIsJoutaiDataId
#define fn_801F6EEC fightSideInitJoutaiDataId
void wsCmdBE(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_801F6E98();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800FA280();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_801F6EEC();
#endif
    extern u8  lbl_80378EFD[];
    extern u8  lbl_80378F11[];
    extern u8  lbl_80378F25[];
    u16 val = (u16)fn_801F54A4(0, 0, 0x14, 0);
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 sub = PC_PTR32_IMPLICIT(fn_801F025C(2, ctx1));
    u32 ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12, 0));
    u32 tmp;
    u32 target;
    u16 tmpNarrow;

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0xe)) == 1) {
        tmp = fightOutPokemonGetJoutaiParam(PC_PTR(ctx1), PC_PTR(0xe));
        tmpNarrow = (u16)tmp;
        if (tmpNarrow != 0) {
            target = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(tmp, val);
            if (target != 0) {
                fn_801F4C14(0, 0, 0x42, 0, target);
                {
                    u32 t = fightOutPokemonGetPokemonJoutai0C(PC_PTR(ctx1), PC_PTR(0xe));
                    fn_80132A38(0xd, PC_PTR32_IMPLICIT(fn_800FA280(fn_8011BEB4(0, t, 1, 0))));
                }
                fn_80202810(PC_PTR(ctx1), PC_PTR(0xe));
                wsRunScript(lbl_8047B62C, (void*)lbl_80378EFD, 0);
                fn_801F4C14(0, 0, 0x42, 0, ctx2);
            }
        }
    }

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x1c)) == 1) {
        fn_80202810(PC_PTR(ctx1), PC_PTR(0x1c));
        wsRunScript(lbl_8047B62C, (void*)lbl_80378F11, 0);
    }

    if (fn_801F6E98(sub, 0x4a) == 1) {
        fn_801F6EEC(sub, 0x4a);
        wsRunScript(lbl_8047B62C, (void*)lbl_80378F25, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6EEC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E98
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_800FA280
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x802173D4 size 0x60: WS_JIKOANJI (owner line 12779) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80201600 fightOutPokemonCopyAllAbiCnt
void WS_JIKOANJI(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
    extern void fn_80201600();
  u32 uVar1;
  u32 uVar2;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  fn_80201600(uVar1,uVar2);
  lbl_8047B610 = lbl_8047B610 + 5;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80201600
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80217434 size 0xF0: wsCmdBC (owner line 2876) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_801254B4 pokemonSetStatus
#define fn_8011BBD8 wazaSetStatus
void wsCmdBC(void) {
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
    extern u16 fn_80203B5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801254B4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx), 0, 0xD9, 0);
    u16 statA = (u16)fn_8012640C(fn_80205B8C(PC_PTR(ctx)), 0, 0x83, 0);
    u16 val2 = fn_80203B5C(PC_PTR(ctx), 2);
    s8 e6field = (s8)fn_8012640C(PC_PTR(ctx), 0, 0xe6, 0);

    if (e6field < 0xc && statA > val2) {
        fn_801254B4(PC_PTR(ctx), 0, 0xe6, 0, 0xc);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, val2);
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
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

/* 0x80217524 size 0x84: wsCmdBB (owner line 4917) */
#pragma optimize_for_size reset
#define fn_801F2988 fightFloorCheckWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F2934 fightFloorWriteJoutaiDataId
void wsCmdBB(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F2988();
#endif
    extern void fn_801F2934();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    u8 val = (u8)fn_801F2988(0, 0x53);

    if (val != 2) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 2;
    } else {
        fn_801F2934(0, 0x53, 0);
        lbl_80478D78[5] = 4;
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

/* 0x802175A8 size 0x23C: wsCmdBA (owner line 12795) */
void wsCmdBA(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
    extern u16 fightActionGetKindDataId(void* action);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightActionCheckValid(void* action);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    extern u8 fightOutPokemonIsAlly();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u8 lbl_80379F58[];
    extern u32 ServerStatusFlag;
    extern u8 lbl_8047B648;
    extern u8 lbl_8047B649;
  u32 context;
  u32 relativeTarget;
  u32 candidate;
  u32 action;
  u32 waza;
  u32 selected;

  context = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x14,0));
  relativeTarget = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(
      context,(u16)fightFloorGetStatus(0,0,0x14,0));
  if (fightOutPokemonCheckFightOut(PC_PTR(context)) == 0) {
    lbl_8047B648 = lbl_8047B649;
  } else {
    candidate = 0;
    selected = 0;
    while (lbl_8047B648 < lbl_8047B649) {
      candidate = fightFloorGetStatus(0,0,0x5d,lbl_8047B648);
      if (candidate != 0 && fightOutPokemonCheckFightOut(PC_PTR(candidate)) != 0 &&
          context != candidate && fightOutPokemonIsAlly(context,candidate) != 1 &&
          (action = pokemonGetStatus(PC_PTR(candidate),0,POKEMON_STATUS_ACTION_PTR,0)) != 0 &&
          (u8)fightActionCheckValid((void*)PC_PTR(action)) != 0 &&
          fightActionGetKindDataId((void*)PC_PTR(action)) == 0x13) {
        waza = pokemonGetStatus(PC_PTR(candidate),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
        if (fightOutPokemonGetUseWazaDataId(PC_PTR(candidate)) == 0xe4 &&
            (u16)relativeTarget == wazaGetStatus(PC_PTR(waza),0,0x29,0) &&
            fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(candidate),PC_PTR(CONDITION_SLEEP)) != 1 &&
            fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(candidate),PC_PTR(CONDITION_FREEZE)) != 1 &&
            (s32)pokemonGetStatus(PC_PTR(candidate),0,POKEMON_STATUS_TRUANT_FLAG,0) != 1) {
          selected = candidate;
          break;
        }
      }
      lbl_8047B648++;
    }
    if (selected != 0) {
      fightFloorSetStatus(0,0,0x42,0,selected);
      lbl_80379F58[0x16002] = 1;
      ServerStatusFlag = ServerStatusFlag & 0xfffffbff;
      pokemonSetStatus(PC_PTR(candidate),0,0x112,0,1);
      lbl_8047B610 = lbl_8047B610 + 5;
      return;
    }
  }
  lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
  return;
}

/* 0x802177E4 size 0x110: wsCmdB9 (owner line 4053) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8011BBD8 wazaSetStatus
#define fn_80132A38 msgctrlSetValue
void wsCmdB9(void) {
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
    extern void fn_80132A38();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u8 idx;
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx), 0, 0xD9, 0);
    u8 v = GSrand() % 100;
    u16 result;

    if (v < 5) {
        result = 10;
        idx = 4;
    } else if (v < 15) {
        result = 30;
        idx = 5;
    } else if (v < 35) {
        result = 50;
        idx = 6;
    } else if (v < 65) {
        result = 70;
        idx = 7;
    } else if (v < 85) {
        result = 90;
        idx = 8;
    } else if (v < 95) {
        result = 110;
        idx = 9;
    } else {
        result = 150;
        idx = 10;
    }
    fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2f, 0, result);
    fn_80132A38(0x2f, idx);
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x802178F4 size 0xA8: wsCmdB8 (owner line 1491) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E44 fightSideCheckWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F6DF0 fightSideWriteJoutaiDataId
void wsCmdB8(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
    extern u8 fn_801F6E44(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F4C14(u32, u16, u32, u16, u32);
#endif
    extern void fn_801F6DF0(u32, u32, u32);
    void* ctx1 = (void*)fn_801F025C(0x11, 0);
    u32 a = PC_PTR32(fn_801F025C(0x2, PC_PTR32_IMPLICIT(ctx1)));
    if (fn_801F6E44(a, 0x4b) != 2) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 0;
    } else {
        fn_801F6DF0(a, 0x4b, 0);
        lbl_80478D78[5] = 5;
    }
    lbl_8047B610++;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6DF0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E44
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021799C size 0x148: wsCmdB7 (owner line 5000) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level reset; optimize_for_size on; scheduling reset; dont_inline reset; opt_lifetimes reset; opt_propagation reset; opt_common_subs reset; switch_tables reset) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level reset
#pragma optimize_for_size on
#pragma scheduling reset
#pragma dont_inline reset
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma switch_tables reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8011BBD8 wazaSetStatus
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_80201704 fightOutPokemonIsHpMantan
#define fn_80209960 fightWazaInitJoutaiDataId
void wsCmdB7(void) {
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
    extern u16 GSrand(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203B5C();
#endif
    extern u8 fn_80201704();
    extern void fn_80209960();
    extern u8 lbl_80375F98[16];
    extern u8 lbl_80377AD9[0x2A];
    extern u8 lbl_80377AB8[0x21];
    u32 ctx2;
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    u8 val;
    ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12, 0));
    val = (u8)GSrand();

    if (val < 0x66) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2f, 0, 0x28);
    } else if (val < 0xb2) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2f, 0, 0x50);
    } else if (val < 0xcc) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2f, 0, 0x78);
    } else {
        u16 v4 = (u16)fn_80203B5C(PC_PTR(ctx2), 4);
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, -(s32)v4);
    }

    if (val < 0xcc) {
        lbl_8047B610 = (u8*)lbl_80375F98;
    } else if (fn_80201704(ctx2) == 1) {
        lbl_8047B610 = (u8*)lbl_80377AD9;
    } else {
        fn_80209960(fieldD9, 0x43);
        lbl_8047B610 = (u8*)lbl_80377AB8;
    }
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80209960
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80201704
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x80217AE4 size 0xEC: wsCmdB6 (owner line 12868) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_8011BBD8 wazaSetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_8012640C pokemonGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
void wsCmdB6(void)

{
    extern u16 fightOutPokemonGetUseWazaDataId(void* ctx);
#ifndef TARGET_PC /* PC: canonical prototypes in pc/prototypes.h */
    extern void fn_8011BBD8();
    extern u32 fn_8011BEB4(void* ptr, u16 dataId, u16 status, u32 index);
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
  u32 uVar1;
  u16 moveId;
  u32 uVar3;
  u16 sVar5;
  u32 uVar2;
  u16 uVar4;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(uVar1));
  uVar3 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  moveId = fn_80205184((void*)PC_PTR(uVar1));
  sVar5 = fn_8011BEB4(0,moveId,9,0);
  uVar4 = (int)fn_8012640C(PC_PTR(uVar2),0,0x99,0);
  if (sVar5 == 0x79) {
    uVar4 = (uVar4 * 10) / 0x19;
  }
  else {
    uVar4 = ((0xff - uVar4) * 10) / 0x19;
  }
  fn_8011BBD8(PC_PTR(uVar3),0,0x2f,0,uVar4);
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif

/* 0x80217BD0 size 0x1C: fightSeqGetOngaeshiIryoku (owner line 9421) */
u16 fightSeqGetOngaeshiIryoku(u16 x) { return (u16)(((0xFF - x) * 10) / 25); }

/* 0x80217BEC size 0x18: fightAiScale10Div25 (owner line 9420) */
u16 fightAiScale10Div25(u16 x) { return (u16)((x * 10) / 25); }

/* 0x80217C04 size 0x130: wsCmdB5 (owner line 4176) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_802096E8 fightWazaIsHit
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_8011BEB4 wazaGetStatus
#define fn_8011BBD8 wazaSetStatus
void wsCmdB5(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 fightOutPokemonGetPokemonJoutaiStack();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802096E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8011BEB4();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    u32 poke1 = fn_80205184(PC_PTR(ctx1));
    u8 flag = fn_802096E8(PC_PTR(fieldD9));
    s16 field2e;
    u16 n;
    u32 result;
    s32 i;

    if (flag == 0) {
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x2e)) == 1) {
            fn_80202810(PC_PTR(ctx1), PC_PTR(0x2e));
        }
        lbl_8047B610 = (u8*)lbl_80375FDF;
        return;
    }
    if (fn_802025B8(PC_PTR(ctx1), PC_PTR(0x2e)) == 2) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x2e), 0);
    }
    field2e = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(ctx1), PC_PTR(0x2e));
    n = fn_8011BEB4(0, poke1, 7, 0);
    result = n;
    for (i = 1; i < field2e; i++) {
        result = result * 2;
    }
    fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2f, 0, result);
    lbl_8047B610 = lbl_8047B610 + 1;
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
