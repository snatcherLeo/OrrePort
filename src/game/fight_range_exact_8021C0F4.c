/**
 * @file fight_range_exact_8021C0F4.c
 * @brief Strict target-order fight island, 0x8021C0F4 - 0x8021CA00.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/abilities.h"
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

/* 0x8021C0F4 size 0x9C: wsCmd88 (owner line 1378) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_8011BBD8 wazaSetStatus
void wsCmd88(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    void* ctx = (void*)fn_801F025C(0x11, 0);
    void* resolved = PC_PTR(fn_8012640C(ctx, 0, 0xd9, 0));
    s32 v2;
    s32 neg;

    fn_8011BEB4(resolved, 0, 0x2d, 0);
    v2 = fn_8011BEB4(resolved, 0, 0x2e, 0);
    neg = -(v2 / 2);
    if (neg == 0) {
        neg = -1;
    }
    fn_8011BBD8(resolved, 0, 0x2d, 0, neg);
    lbl_8047B610++;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021C190 size 0x178: wsCmd87 (owner line 5486) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80201704 fightOutPokemonIsHpMantan
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_8011BBD8 wazaSetStatus
void wsCmd87(void) {
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
    extern u8 fn_80201704();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiDataBiosGetMaxCount();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203B5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif

    s16 val;
    u32 ctx1;
    u32 fieldD9;
    u8* jumpTarget;
    s16 diff;

    jumpTarget = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x2d)) == 0) {
        val = 0;
    } else {
        val = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(ctx1), PC_PTR(0x2d));
    }
    if (val <= 0) {
        lbl_80478D78[5] = 0;
        lbl_8047B610 = jumpTarget;
        return;
    }
    if (fn_80201704(ctx1) == 1) {
        fn_80202810(PC_PTR(ctx1), PC_PTR(0x2d));
        fn_801F4C14(0, 0, 0x43, 0, ctx1);
        lbl_80478D78[5] = 1;
        lbl_8047B610 = jumpTarget;
        return;
    }
    diff = (s16)joutaiDataBiosGetMaxCount(0x2d) - val;
    if (diff < 0) {
        diff = 0;
    }
    fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, -(s32)(u16)fn_80203B5C(PC_PTR(ctx1), (u16)(1 << diff)));
    lbl_80379F58[0x16002] = (u8)val;
    fn_80202810(PC_PTR(ctx1), PC_PTR(0x2d));
    fn_801F4C14(0, 0, 0x43, 0, ctx1);

    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80201704
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021C308 size 0x188: wsCmd86 (owner line 5555) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_8011BEB4 wazaGetStatus
#define fn_8011BBD8 wazaSetStatus
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
void wsCmd86(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 fightOutPokemonGetPokemonJoutaiStack();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightSeqCalcDamage();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif

    u32 ctx2;
    u8* jumpTarget;
    u32 aux;
    u16 v1;
    u16 v2;
    u32 ctx1;
    u32 fieldD9;
    s16 val;
    u32 obj;

    jumpTarget = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    aux = fn_80205184(PC_PTR(ctx1));
    v1 = (u16)fn_8011BEB4(PC_PTR(fieldD9), 0, 0x2f, 0);
    v2 = (u16)fn_8011BEB4(PC_PTR(fieldD9), 0, 0x30, 0);

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x2d)) == 0) {
        val = 0;
    } else {
        val = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(ctx1), PC_PTR(0x2d));
    }
    ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12, 0));
    obj = PC_PTR32_IMPLICIT(fn_801F025C(2, ctx2));
    if (val <= 0) {
        lbl_8047B610 = jumpTarget;
        return;
    }
    if (lbl_80478D78[6] != 1) {
        s32 result = fightSeqCalcDamage(ctx1, ctx2, obj, aux, v1, v2);
        ctx2 = (s32)val * result;
        lbl_80379F58[0x16002] = (u8)val;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x32)) == 1) {
            ctx2 = ((s32)ctx2 * 15) / 10;
        }
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, ctx2);
    }
    fn_80202810(PC_PTR(ctx1), PC_PTR(0x2d));
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
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

/* 0x8021C490 size 0xF8: wsCmd85 (owner line 3824) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
#define fn_80132A38 msgctrlSetValue
void wsCmd85(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 fightOutPokemonGetPokemonJoutaiStack();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    s16 field2d;

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x2d)) == 0) {
        field2d = 0;
    } else {
        field2d = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(ctx1), PC_PTR(0x2d));
    }
    if (field2d == joutaiDataBiosGetMaxCount(0x2d)) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 1;
    } else {
        if (fn_802025B8(PC_PTR(ctx1), PC_PTR(0x2d)) == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x2d), 0);
        }
        field2d = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(ctx1), PC_PTR(0x2d));
        fn_80132A38(0x2f, field2d);
        lbl_80478D78[5] = 0;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021C588 size 0xB0: wsCmd84 (owner line 9347) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
void wsCmd84(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16   fn_80207BF4(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void  fn_8021C638();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8    fn_801F37B0(u32, void*, void*, u32);
#endif
    void* ctx = fn_801F025C(0x12, 0);
    u16 moveId = fn_80207BF4(ctx);
    u32 target = DISC_U32(lbl_8047B610 + 1);
    u8 found = (u8)fn_801F37B0(0, fn_8021C638, ctx, 1) != 1;
    if (found != 0) {
        lbl_8047B610 = (u8*)PC_PTR(target);
        return;
    }
    if (moveId == ABILITY_INSOMNIA || moveId == ABILITY_VITAL_SPIRIT) {
        lbl_80478D78[5] = 2;
        lbl_8047B610 = (u8*)PC_PTR(target);
        return;
    }
    lbl_8047B610 += 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021C638 size 0xBC: fn_8021C638 (owner line 15056) */
u32 fn_8021C638(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
  u8 cVar3;
  u32 originalR3 = r3;

  r3 = fightOutPokemonGetTokuseiDataId(PC_PTR(r5));
  cVar3 = fightOutPokemonCheckFightOut(PC_PTR(originalR3));
  if (cVar3 == 0) {
    return 1;
  }
  cVar3 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(originalR3),PC_PTR(0xb));
  if ((cVar3 == 1) && ((u16)r3 != ABILITY_SOUNDPROOF)) {
    fightFloorSetStatus(0,0,0x4b,0,originalR3);
    if (originalR3 == r5) {
      lbl_80478D78[5] = 0;
    }
    else {
      lbl_80478D78[5] = 1;
    }
    return 0;
  }
  return 1;
}

/* 0x8021C6F4 size 0x10: wsCmd83 (owner line 204) */
u8* wsCmd83(void) { return lbl_8047B610++; }

/* 0x8021C704 size 0x58: WS_NEKODAMASHI (owner line 15086) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
void WS_NEKODAMASHI(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8012640C();
#endif
  u32 uVar2;
  u16 sVar3;

  uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  sVar3 = (int)fn_8012640C(PC_PTR(uVar2),0,0xed,0);
  if (sVar3 != 0) {
    lbl_8047B610 = lbl_8047B610 + 5;
  }
  else {
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
  }
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021C75C size 0x1A4: wsCmd81 (owner line 8827) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_801F4C14 fightFloorSetStatus
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_8011BBD8 wazaSetStatus
#define fn_80201704 fightOutPokemonIsHpMantan
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
void wsCmd81(void) {
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
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203B5C();
#endif
    extern u8  fn_80201704();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
    extern void fightOutPokemonSetPokemonJoutaiKaisuu();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xd9, 0);
    u16 val;

    fn_801F025C(0x12, 0);
    fn_801F4C14(0, 0, 0x43, 0, ctx1);

    val = (u16)fn_80203B5C(PC_PTR(ctx1), 1);
    fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, -val);

    if ((u8)fn_80201704(ctx1) == 1) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_POISON)) == 1 ||
        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_BAD_POISON)) == 1 ||
        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_PARALYSIS)) == 1 ||
        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_BURN)) == 1 ||
        fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_FREEZE)) == 1) {
        lbl_80478D78[5] = 1;
    } else {
        lbl_80478D78[5] = 0;
    }

    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_SLEEP), 0);
    fightOutPokemonSetPokemonJoutaiKaisuu(ctx1, 8, 3);

    if (fn_801FECD4(PC_PTR(ctx1)) == 1) {
        fn_801FE7EC(PC_PTR(ctx1), 0x7c, 0, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80201704
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
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

/* 0x8021C900 size 0x100: wsCmd80 (owner line 9464) */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd80(void) {
    u32 ctx1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    u32 fieldD9 = pokemonGetStatus(PC_PTR(ctx1), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    s32 val = wazaGetStatus(PC_PTR(fieldD9), 0, 0x2d, 0);
    u32 ctx2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    u32 poke2 = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(ctx2));
    s32 stat = pokemonGetStatus(PC_PTR(poke2), 0, POKEMON_STATUS_MAX_HP, 0);
    u8 op = ((FightSeqOpU8Operand*)lbl_8047B610)->operand;

    switch (op) {
    case 0:
        val *= -1;
        break;
    case 1:
        val = val / 2;
        if (val == 0) {
            val = 1;
        }
        if (stat / 2 < val) {
            val = stat / 2;
        }
        break;
    case 2:
        val = val << 1;
        break;
    }
    wazaSetStatus(PC_PTR(fieldD9), 0, 0x2d, 0, val);
    lbl_8047B610 = lbl_8047B610 + 2;
}
#else
void wsCmd80(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType(u32 slotType, u32 idx);
#endif
    extern void wazaSetStatus(void* ptr, u16 dataId, u16 status, u32 index, u32 value);
    u32 ctx1 = fightTargetGetPtrAsNowFightType(0x11, 0);
    u32 fieldD9 = pokemonGetStatus(ctx1, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    s32 val = wazaGetStatus(fieldD9, 0, 0x2d, 0);
    u32 ctx2 = fightTargetGetPtrAsNowFightType(0x12, 0);
    u32 poke2 = fightOutPokemonGetPokemonPtr(ctx2);
    s32 stat = pokemonGetStatus(poke2, 0, POKEMON_STATUS_MAX_HP, 0);
    u8 op = ((FightSeqOpU8Operand*)lbl_8047B610)->operand;

    switch (op) {
    case 0:
        val *= -1;
        break;
    case 1:
        val = val / 2;
        if (val == 0) {
            val = 1;
        }
        if (stat / 2 < val) {
            val = stat / 2;
        }
        break;
    case 2:
        val = val << 1;
        break;
    }
    wazaSetStatus((void*)fieldD9, 0, 0x2d, 0, val);
    lbl_8047B610 = lbl_8047B610 + 2;
}
#endif

