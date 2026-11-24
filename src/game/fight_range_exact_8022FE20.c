/**
 * @file fight_range_exact_8022FE20.c
 * @brief Strict target-order fight island, 0x8022FE20 - 0x80230568.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
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


/* _fightClearRageSub */
#define fn_80205224 fightOutPokemonGetMotoWazaDataId
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
u8 _fightClearRageSub(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80205224();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x13)) == 1) {
        if ((u16)fn_80205224(ctx) != 0x63) {
            fn_80202810(ctx, PC_PTR(0x13));
        }
    }
    return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205224
#endif

/* wsRunEndOfTurnScript */
/* RULE-EXCEPTION(user-approved): opt_propagation off for wsRunEndOfTurnScript and wsRunEndOfTurnScript2 only; the unit builds with -O4,s — see docs/RULE_EXCEPTIONS.md */
#pragma opt_propagation off
void wsRunEndOfTurnScript(void) {
    extern u8 lbl_8027A00C[];
    extern u8 lbl_80378798[];
    extern u8 lbl_8047B614;
    extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightDoTraceSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightDoIkakuSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightCheckForecastSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80230088();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightDoKizetuScriptSub();
#endif
    u8 sp[2];
    u32 saved610;
    u8 saved614;
    u32 saved62c;
    u8* start;
    u32 dispatch;

    fightFloorLoopValidFightOutPokemon(0, _fightDoKizetuScriptSub, 0, 0);
    saved62c = PC_PTR32(lbl_8047B62C);
    start = lbl_80378798;
    saved614 = lbl_8047B614;
    dispatch = PC_PTR32(lbl_8027A00C);
    saved610 = PC_PTR32(lbl_8047B610);
    lbl_8047B614 = 0;
    lbl_8047B610 = start;
    lbl_8047B62C = (void*)PC_PTR(saved62c);
    do {
#ifdef TARGET_PC /* the command table is a native function-pointer array on the PC (pcdata_types.txt) */
        ((void (**)(void))lbl_8027A00C)[*lbl_8047B610]();
#else
        ((void (*)(void))PC_PTR(*(u32*)PC_PTR((dispatch + (*lbl_8047B610 * 4)))))();
#endif
    } while (lbl_8047B614 != 1 && lbl_8047B614 != 2);

    lbl_8047B62C = (void*)PC_PTR(saved62c);
    lbl_8047B614 = saved614;
    lbl_8047B610 = (u8*)PC_PTR(saved610);
    sp[1] = 1;
    fightFloorLoopValidFightOutPokemon(0, _fightDoIkakuSub, &sp[1], 0);
    fightFloorLoopValidFightOutPokemon(0, _fightDoTraceSub, 0, 0);
    fightFloorLoopValidFightOutPokemon(0, fn_80230088, 0, 0);
    sp[0] = 0;
    fightFloorLoopValidFightOutPokemon(0, _fightCheckForecastSub, &sp[0], 0);
}

/* wsRunEndOfTurnScript2 */
void wsRunEndOfTurnScript2(void) {
    extern u8 lbl_8027A00C[];
    extern u8 lbl_80378798[];
    extern u8 lbl_8047B614;
    extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightDoTraceSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightDoIkakuSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightCheckForecastSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80230088();
#endif
    u8 sp[2];
    u32 saved610;
    u8 saved614;
    u32 saved62c;
    u8* start;
    u32 dispatch;

    start = lbl_80378798;
    dispatch = PC_PTR32(lbl_8027A00C);
    saved62c = PC_PTR32(lbl_8047B62C);
    saved614 = lbl_8047B614;
    saved610 = PC_PTR32(lbl_8047B610);
    lbl_8047B614 = 0;
    lbl_8047B610 = start;
    lbl_8047B62C = (void*)PC_PTR(saved62c);
    do {
#ifdef TARGET_PC /* the command table is a native function-pointer array on the PC (pcdata_types.txt) */
        ((void (**)(void))lbl_8027A00C)[*lbl_8047B610]();
#else
        ((void (*)(void))PC_PTR(*(u32*)PC_PTR((dispatch + (*lbl_8047B610 * 4)))))();
#endif
    } while (lbl_8047B614 != 1 && lbl_8047B614 != 2);

    lbl_8047B62C = (void*)PC_PTR(saved62c);
    lbl_8047B614 = saved614;
    lbl_8047B610 = (u8*)PC_PTR(saved610);
    sp[1] = 1;
    fightFloorLoopValidFightOutPokemon(0, _fightDoIkakuSub, &sp[1], 0);
    fightFloorLoopValidFightOutPokemon(0, _fightDoTraceSub, 0, 0);
    fightFloorLoopValidFightOutPokemon(0, fn_80230088, 0, 0);
    sp[0] = 0;
    fightFloorLoopValidFightOutPokemon(0, _fightCheckForecastSub, &sp[0], 0);
}

/* fn_80230088 */
#pragma opt_propagation reset
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F4C14 fightFloorSetStatus
u32 fn_80230088(int r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonDoSoubiItemCheck();
#endif
  u32 local;
  u32 uVar1;
  u8 cVar2;

  local = r3;
  cVar2 = fn_802062FC(PC_PTR(r3)); /* r3 untouched before the call (assembly) */
  if (cVar2 == 0) {
    return 1;
  }
  uVar1 = fn_801F54A4(0,0,0x36,0);
  fn_801F4C14(0,0,0x36,0,local);
  fightOutPokemonDoSoubiItemCheck(local,1);
  fn_801F4C14(0,0,0x36,0,uVar1);
  return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif

/* _fightDoKizetuScriptSub */
#define fn_8012640C pokemonGetStatus
#define fn_801F4C14 fightFloorSetStatus
u8 _fightDoKizetuScriptSub(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    if ((u8)(u32)fn_8012640C(ctx, 0, 0x120, 0) == 1) {
        fn_801F4C14(0, 0, 0x46, 0, PC_PTR32(ctx));
        fn_801F4C14(0, 0, 0x42, 0, PC_PTR32(ctx));
        wsRunScript(lbl_8047B62C, (void*)lbl_80378721, 0);
    }
    return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif

/* _fightJoutai1ETurnEndSub */
u32 _fightJoutai1ETurnEndSub(u32 r3)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80201FDC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonGetNowKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonGetKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern void wsRunEndOfTurnScript();
    extern u32 ServerStatusFlag;
    extern void* lbl_8047B62C;
    extern u8 lbl_80378E1E[];
    extern u8 lbl_80378E46[];
    s8 threshold;
    s8 count;
    s32 signedCount;
    void* context;
    u32 moveStatus;
    u32 pokemon;
    u16 hp;
    u8* script;

    context = (void*)PC_PTR(r3);
    if (fightOutPokemonCheckFightOut(context) == 0) {
        return 1;
    }

    ServerStatusFlag = (ServerStatusFlag | 0x1000000) | 0x20;
    if (fightOutPokemonIsPokemonJoutaiDataId(context, PC_PTR(0x1E)) == 1) {
        threshold = fightOutPokemonGetKaisuuJoutaiDataId(context, PC_PTR(0x1E));
        count = fightOutPokemonGetNowKaisuuJoutaiDataId(context, PC_PTR(0x1E));
        fightFloorSetStatus(0, 0, 0x36, 0, PC_PTR32_IMPLICIT(context));
        moveStatus = pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
        signedCount = count;
        msgctrlSetValue(0x2F, (s8)threshold - signedCount);

        if ((int)count < threshold) {
            goto increment_counter;
        }
        pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(context));
        hp = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_HP, 0);
        wazaSetStatus(PC_PTR(moveStatus), 0, 0x2D, 0, hp);
        fightOutPokemonWriteJoutaiDataId(context, PC_PTR(0x1E));
        script = lbl_80378E1E;
        goto show_message;
increment_counter:
        fn_80201FDC(context, PC_PTR(0x1E), (s8)(signedCount + 1));
        script = lbl_80378E46;
show_message:
        wsRunScript(lbl_8047B62C, script, 0);
        wsRunEndOfTurnScript();
    }
    ServerStatusFlag = ServerStatusFlag & 0xfeffffdf;
    return 1;
}

/* fn_80230318 */
u32 fn_80230318(u32 input, u32 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80201FDC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonGetNowKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonGetKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    extern void fn_80202810();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
    extern void fightWazaCreate();
    extern void fightWazaBiosCopy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern void wsRunEndOfTurnScript();
    extern u8 lbl_80378F39[];
    extern u32 ServerStatusFlag;
    extern void* lbl_8047B62C;
  s8 cVar5;
  s8 cVar6;
  u8 hasState;
  void* r3;
  u32 uVar1;
  u32 uVar2;
  u32 uVar3;

  u8 auStack_c8 [172];

  r3 = (void*)PC_PTR(input);
  if (fightOutPokemonCheckFightOut(PC_PTR(input)) == 0) {
    return 1;
  }
  ServerStatusFlag = ServerStatusFlag | 0x1000020;
  hasState = fightOutPokemonIsPokemonJoutaiDataId(r3,PC_PTR(0x34));
  if (hasState == 1) {
    cVar5 = fightOutPokemonGetKaisuuJoutaiDataId(r3,PC_PTR(0x34));
    cVar6 = fightOutPokemonGetNowKaisuuJoutaiDataId(r3,PC_PTR(0x34));
    if (cVar6 < cVar5) {
      goto increment_state;
    }
        uVar1 = fightOutPokemonGetPokemonJoutai08(r3,0x34);
        uVar2 = fightOutPokemonGetPokemonJoutai0C(r3,PC_PTR(0x34));
        uVar3 = fightOutPokemonGetJoutaiParam(r3,PC_PTR(0x34));
        uVar3 = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(uVar3,r4);
        if (fightOutPokemonCheckFightOut(PC_PTR(uVar3)) == 0) {
          uVar3 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0xe,uVar3));
        }
        if ((uVar2 & 0xffff) == 0xf8) {
          lbl_80478D78[5] = 0;
        }
        else {
          lbl_80478D78[5] = 1;
        }
        msgctrlSetValue(0xd,PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0,uVar2,1,0))));
        fightFloorSetStatus(0,0,0x42,0,PC_PTR32_IMPLICIT(r3));
        r4 = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(PC_PTR32_IMPLICIT(r3),r4);
        fightFloorSetStatus(0,0,0x36,0,uVar3);
        {
          u32 callResult = pokemonGetStatus(PC_PTR(uVar3),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
          uVar3 = callResult;
        }
        fightWazaBiosCopy(auStack_c8,uVar3);
        fightWazaCreate(uVar3,0xffffffff,uVar2,r4,0);
        wazaSetStatus(PC_PTR(uVar3),0,0x2d,0,uVar1);
        pokemonSetStatus((void*)r3,0,POKEMON_STATUS_SHELL_BELL_DAMAGE,0,0xffff);
        fightOutPokemonWriteJoutaiDataId(r3,PC_PTR(0x34));
        wsRunScript(lbl_8047B62C,lbl_80378F39,0);
        fightWazaBiosCopy(uVar3,auStack_c8);
        wsRunEndOfTurnScript();
        goto clear_flags;
increment_state:
    fn_80201FDC(r3,PC_PTR(0x34),(s8)(cVar6 + 1));
  }
clear_flags:
  ServerStatusFlag = ServerStatusFlag & 0xfeffffdf;
  return 1;
}
