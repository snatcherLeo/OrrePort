/**
 * @file fight_range_exact_8021A6CC.c
 * @brief Strict target-order fight island, 0x8021A6CC - 0x8021B910.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"
#include "game/constants/moves.h"
#include "game/constants/types.h"
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

/* 0x8021A6CC size 0x98: wsCmd9A (owner line 1445) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
void wsCmd9A(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    void* ctx = (void*)fn_801F025C(0x11, 0);
    if (fn_802025B8(ctx, PC_PTR(0xf)) != 2) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x45);
        lbl_80478D78[5] = 1;
    } else {
        fightOutPokemonWritePokemonJoutaiDataId(ctx, PC_PTR(0xf), 0);
        lbl_80478D78[5] = 0;
    }
    lbl_8047B610++;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021A764 size 0xA8: wsCmd99 (owner line 1654) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E44 fightSideCheckWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F6DF0 fightSideWriteJoutaiDataId
void wsCmd99(void) {
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
    if (fn_801F6E44(a, 0x4c) != 2) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x45);
        lbl_80478D78[5] = 1;
    } else {
        fn_801F6DF0(a, 0x4c, 0);
        lbl_80478D78[5] = 0;
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

/* 0x8021A80C size 0x6C: wsCmd98 (owner line 1360) */
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_802653FC fightMenuFightOutPokemonRenewStatusMenu
void wsCmd98(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802653FC(void*, u16, s32);
#endif
    u16 count = (u16)fn_801F54A4(0, 0, 0x14, 0);
    u8 op = lbl_8047B610[1];
    void* ctx = (void*)fn_801F025C(op, 0);
    if (ctx != 0) {
        fn_802653FC(PC_PTR32_IMPLICIT(ctx), count, 1);
    }
    lbl_8047B610 += 2;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802653FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x8021A878 size 0x10C: wsCmd97 (owner line 4214) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_802016A4 fightOutPokemonGetSex
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F0134 fightTargetGetTragetPtrToRelativeHostSideFightTargetId
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
void wsCmd97(void) {
    extern u8 fn_802016A4();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F0134();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207BF4();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u8 statusA = fn_802016A4(ctx1);
    u16 cand = fn_801F54A4(0, 0, 0x14, 0);
    u32 ctx2;
    u32 newPoke = fn_801F0134(ctx1, cand);
    u8 statusB;
    u16 res;
    ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12, 0));
    statusB = fn_802016A4(ctx2);
    res = fn_80207BF4(PC_PTR(ctx2));

    if (res == ABILITY_OBLIVIOUS) {
        lbl_8047B610 = (u8*)&lbl_8037984D;
        return;
    }
    if (statusA == statusB) {
        goto takeJump;
    }
    if (fn_802025B8(PC_PTR(ctx2), PC_PTR(0xa)) != 2) {
        goto takeJump;
    }
    if (statusA == 2) {
        goto takeJump;
    }
    if (statusB != 2) {
        goto doCall;
    }
takeJump:
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    return;
doCall:
    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx2), PC_PTR(0xa), PC_PTR(newPoke));
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F0134
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802016A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021A984 size 0x194: wsCmd96 (owner line 14563) */
#pragma optimize_for_size reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_801F453C fightFloorGetNowTenkouDataId
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80207AE0 fightOutPokemonIsZokuseiDataId
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_8011BBD8 wazaSetStatus
void wsCmd96(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F453C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80203B5C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80207AE0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4();
#endif
  u32 uVar1;
  u32 uVar2;
  u32 sVar3;
  u32 uVar4;
  u8 cVar6;
  u32 cVar5;

  uVar4 = 0;
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd9,0);
  sVar3 = fn_80207BF4(PC_PTR(uVar1));
  cVar5 = (int)fn_801F453C(0,1);
  cVar6 = fn_802062FC(PC_PTR(uVar1));
  if (cVar6 == 0) {
    uVar4 = 0;
    goto LAB_00217ae0;
  }
  if (cVar5 == 3) {
    cVar6 = fn_80207AE0(PC_PTR(uVar1),5);
    if (cVar6 == 0) {
      cVar6 = fn_80207AE0(PC_PTR(uVar1),8);
      if ((cVar6 == 0) && (cVar6 = fn_80207AE0(PC_PTR(uVar1),4), cVar6 == 0)) {
        if ((sVar3 & 0xffff) != 8) {
          cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(CONDITION_UNDERGROUND));
          if ((cVar6 == 0) && (cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(CONDITION_UNDERWATER)), cVar6 == 0)) {
            uVar4 = fn_80203B5C(PC_PTR(uVar1),0x10) & 0xffff;
            goto LAB_00217a84;
          }
        }
      }
    }
    uVar4 = 0;
  }
LAB_00217a84:
  if (cVar5 == 4) {
    cVar5 = fn_80207AE0(PC_PTR(uVar1),0xf);
    if (cVar5 == 0) {
      cVar5 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(CONDITION_UNDERGROUND));
      if ((cVar5 == 0) && (cVar5 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(CONDITION_UNDERWATER)), cVar5 == 0)) {
        uVar4 = fn_80203B5C(PC_PTR(uVar1),0x10) & 0xffff;
        goto LAB_00217ae0;
      }
    }
    uVar4 = 0;
  }
LAB_00217ae0:
#ifdef TARGET_PC
  {
    /* ORRE_BATTLE_LOG (observe only, 2026-10-27, P12's battle 208: the console's Gengar took sandstorm damage
     * at the end of a turn, the PC's took none): this command's inputs and the damage it hands on */
    static int logWeather = -1;
    extern char* getenv(const char*);
    extern void OSReport(const char*, ...);
    if (logWeather < 0) {
      extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
      logWeather = pc_battle_log_on();
    }
    if (logWeather) {
      OSReport("[battle] weather damage: mon %08X work %08X weather %u ability %u fighting %u types 5/8/4/15 %u%u%u%u "
               "cond 20/21 %d%d -> %u\n", (unsigned)uVar1, (unsigned)uVar2, (unsigned)fn_801F453C(0, 1),
               (unsigned)(sVar3 & 0xFFFF),
               (unsigned)fn_802062FC(PC_PTR(uVar1)), (unsigned)fn_80207AE0(PC_PTR(uVar1), 5),
               (unsigned)fn_80207AE0(PC_PTR(uVar1), 8), (unsigned)fn_80207AE0(PC_PTR(uVar1), 4),
               (unsigned)fn_80207AE0(PC_PTR(uVar1), 0xf),
               (int)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1), PC_PTR(CONDITION_UNDERGROUND)),
               (int)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1), PC_PTR(CONDITION_UNDERWATER)), (unsigned)uVar4);
    }
  }
#endif
  fn_8011BBD8(PC_PTR(uVar2),0,0x2d,0,uVar4);
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207AE0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F453C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021AB18 size 0x84: WS_WEATHER_CHANGE (owner line 14636) */
#define fn_801F2988 fightFloorCheckWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F2934 fightFloorWriteJoutaiDataId
void WS_WEATHER_CHANGE(void)

{
    extern void fn_801F2934();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F2988();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
    extern u8 lbl_80478D78[1];
  u8 cVar1;

  cVar1 = fn_801F2988(0,0x55);
  if (cVar1 != 2) {
    fn_801F4C14(0,0,0x3b,0,0x40);
    lbl_80478D78[5] = 2;
  }
  else {
    fn_801F2934(0,0x55,0);
    lbl_80478D78[5] = 3;
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
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

/* 0x8021AB9C size 0x80: wsCmd94 (owner line 1400) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_80203ADC fightOutPokemonNowHpWaruValue
#define fn_8011BBD8 wazaSetStatus
void wsCmd94(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    extern u32 fn_80203ADC();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    void* ctx1 = (void*)fn_801F025C(0x11, 0);
    void* resolved = PC_PTR(fn_8012640C(ctx1, 0, 0xd9, 0));
    void* ctx2 = (void*)fn_801F025C(0x12, 0);
    u16 val = (u16)fn_80203ADC(ctx2, 2);
    fn_8011BBD8(resolved, 0, 0x2d, 0, val);
    lbl_8047B610++;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BBD8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203ADC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021AC1C size 0x390: fightCheckOneHitKO (owner line 14662) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma opt_propagation off
void fightCheckOneHitKO(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
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
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 figthOutPokemonGetLevel();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int figthOutPokemonGetSoubiItemBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetSoubiItemSoubiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetSoubiItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u8 lbl_803797A0[];
  u16 canSteal;
  u32 attacker;
  u16 floorTarget;
  u32 relativeTarget;
  u32 attackerMove;
  u16 remainingPP;
  u32 attackerStatus;
  u32 attackerLevel;
  u32 itemDataId;
  u32 itemSubDataId;
  int itemBuff;
  u32 defenderPokemon;
  u8 stealBonus;
  u32 ability;
  u32 defenderLevel;
  u16 randomValue;
  u8 condition;
  register u16 threshold;
  int levelDelta;

  attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
  floorTarget = fightFloorGetStatus(0,0,0x14,0);
  relativeTarget = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(attacker,
      floorTarget);
  attackerMove = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
  attackerStatus = pokemonGetStatus(PC_PTR(attacker),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
  attackerLevel = figthOutPokemonGetLevel(PC_PTR(attacker));
  attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12,0));
  itemDataId = fightOutPokemonGetSoubiItemDataId(PC_PTR(attacker));
  itemSubDataId = fightOutPokemonGetSoubiItemSoubiDataId(PC_PTR(attacker));
  itemBuff = figthOutPokemonGetSoubiItemBuff(PC_PTR(attacker));
  defenderPokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
  remainingPP = pokemonGetStatus(PC_PTR(defenderPokemon),0,POKEMON_STATUS_HP,0);
  ability = fightOutPokemonGetTokuseiDataId(PC_PTR(attacker));
  defenderLevel = figthOutPokemonGetLevel(PC_PTR(attacker));
  fightFloorSetStatus(0,0,0x49,0,attacker);
  if ((u16)itemSubDataId == 0x27) {
    randomValue = (u16)GSrand();
    if ((int)randomValue % 100 < itemBuff) {
      pokemonSetStatus(PC_PTR(attacker),0,POKEMON_STATUS_FOCUS_BAND_FLAG,0,1);
    }
  }
  if ((u16)ability == ABILITY_STURDY) {
    fightFloorSetStatus(0,0,0x3b,0,0x40);
    lbl_8047B610 = lbl_803797A0;
    return;
  }
  condition = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker),PC_PTR(CONDITION_LOCK_ON));
  if ((condition == 1) &&
      ((u16)relativeTarget == (u16)fightOutPokemonGetJoutaiParam(PC_PTR(attacker),PC_PTR(0x1d)))) {
    if ((u8)attackerLevel >= (u8)defenderLevel) {
      canSteal = 1;
    }
    else {
      canSteal = 0;
    }
  }
  else {
    stealBonus = wazaGetStatus(0,attackerMove,6,0);
    levelDelta = (u8)attackerLevel - (u8)defenderLevel;
    threshold = levelDelta + stealBonus;
    randomValue = (u16)GSrand();
    if (((int)randomValue % 100 + 1 < (int)threshold) &&
        ((u8)attackerLevel >= (u8)defenderLevel)) {
      canSteal = 1;
    }
    else {
      canSteal = 0;
    }
  }
  if (canSteal) {
    condition = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker),PC_PTR(CONDITION_ENDURE));
    if (condition != 0) {
      wazaSetStatus(PC_PTR(attackerStatus),0,0x2d,0,remainingPP - 1);
      fightFloorSetStatus(0,0,0x3b,0,0x46);
    }
    else {
      if ((int)pokemonGetStatus(PC_PTR(attacker),0,POKEMON_STATUS_FOCUS_BAND_FLAG,0) != 0) {
        wazaSetStatus(PC_PTR(attackerStatus),0,0x2d,0,remainingPP - 1);
        fightFloorSetStatus(0,0,0x3b,0,0x47);
        fightFloorSetStatus(0,0,0x56,0,(u16)itemDataId);
      }
      else {
        wazaSetStatus(PC_PTR(attackerStatus),0,0x2d,0,remainingPP);
        fightFloorSetStatus(0,0,0x3b,0,0x44);
      }
    }
    lbl_8047B610 = lbl_8047B610 + 5;
  }
  else {
    fightFloorSetStatus(0,0,0x3b,0,0x40);
    if ((u8)attackerLevel >= (u8)defenderLevel) {
      lbl_80478D78[5] = 0;
    }
    else {
      lbl_80478D78[5] = 1;
    }
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
  }
}

/* 0x8021AFAC size 0x104: wsCmd92 (owner line 4060) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#pragma scheduling reset
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma switch_tables reset
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E44 fightSideCheckWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F6DF0 fightSideWriteJoutaiDataId
#define fn_801F54A4 fightFloorGetStatus
void wsCmd92(void) {
    extern u8 fn_801F6E44(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F4C14(u32, u16, u32, u16, u32);
#endif
    extern void fn_801F6DF0(u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(2, ctx1));
    u16 count = fightFloorGetValidFightOutPokemonCount(0, 1, ctx1, 1);

    if (fn_801F6E44(ctx2, 0x49) != 2) {
        fn_801F4C14(0, 0, 0x3b, 0, 0x40);
        lbl_80478D78[5] = 0;
    } else {
        u16 x;
        fn_801F6DF0(ctx2, 0x49, 0);
        x = fn_801F54A4(0, 0, 0x19, 0);
        if (x < 2) {
            goto setThree;
        }
        if (count < 2) {
            goto setThree;
        }
        lbl_80478D78[5] = 4;
        goto doneAfac;
    setThree:
        lbl_80478D78[5] = 3;
    doneAfac:;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
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

/* 0x8021B0B0 size 0xF4: wsCmd91 (owner line 4338) */
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F2A7C fightFloorGetGcHeroFightTrainerPtr
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_80132A38 msgctrlSetValue
#define fn_8012A5B0 heroGetStatus
void wsCmd91(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_801F2A7C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8012A5B0();
#endif
    u32 handle;
    u32 c;
    u8 b;
    u32 a;

    if (fn_801F54A4(0, 0, 0x25, 0) != 1) {
        goto skipB0B0;
    }
    handle = fn_801F2A7C(0);
    if (handle == 0) {
        goto skipB0B0;
    }
    a = fn_801FB1C0(PC_PTR(handle), 0, 0x49, 0);
    if (a == 0) {
        goto skipB0B0;
    }
    b = fn_801FB1C0(PC_PTR(handle), 0, 0x48, 0);
    a = a * b;
    c = fn_801FB1C0(PC_PTR(handle), 0, 0x44, 0);
    if (c == 0) {
        goto skipB0B0;
    }
    heroAddPokedoru(c, a);
    fn_80132A38(0x2f, a);
    fn_80132A38(0x13, fn_8012A5B0(PC_PTR(c), 1, 0));
    wsRunScript(lbl_8047B62C, lbl_8037939C, 0);
skipB0B0:
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012A5B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80132A38
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2A7C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif

/* 0x8021B1A4 size 0x1C0: wsCmd90 (owner line 14787) */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma opt_lifetimes off
void wsCmd90(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
    extern u32 zokuseiDataBiosGetName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetZokuseiDataId();
#endif
    u32 attacker;
    struct {
        u32 value;
    } pokemon;
    struct {
        u32 value;
    } type;
    struct {
        u32 value;
    } slot;
    u16 move;
    u32 selected;
    struct {
        u32 count;
        s32 i;
    } state;
    u32 index;
    u32 candidates[4];

    type.value = 0;
    slot.value = 0;
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    pokemon.value = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
    {
        u32 init_i;
        for (init_i = 0; (u8)init_i < 4; init_i++) {
            candidates[(u8)init_i] = -1;
        }
    }

    state.count = 0;
    for (state.i = state.count; (u8)state.i < 4; state.i++) {
        slot.value = (u8)state.i;
        if (pokemonWazaCheckValid(PC_PTR(pokemon.value), slot.value) == 1) {
            move = (u16)pokemonGetStatus(PC_PTR(pokemon.value), 0, POKEMON_STATUS_MOVE, slot.value);
            if (move != MOVE_STRUGGLE && move != MOVE_SHADOW_RUSH) {
                type.value = wazaGetStatus(0, move, 3, 0) & 0xffff;
                if (type.value == 9) {
                    if (fightOutPokemonIsZokuseiDataId(PC_PTR(attacker), TYPE_GHOST) == 1) {
                        type.value = 7;
                    } else {
                        type.value = 0;
                    }
                }
                if (fightOutPokemonIsZokuseiDataId(PC_PTR(attacker), type.value) == 0) {
                    candidates[(u8)state.count] = (u16)type.value;
                    state.count++;
                }
            }
        }
    }

    if ((u8)state.count == 0) {
failed:
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        goto done;
    }

    selected = (pokemon.value,
        candidates[(u8)((s32)(u16)GSrand() % (s32)(u8)state.count)]);
    if ((s32)selected < 0) {
        goto failed;
    }

    state.count = selected & 0xffff;
    for (index = 0; (u8)index < 2; index++) {
        fightOutPokemonSetZokuseiDataId(PC_PTR(attacker), index, state.count);
    }
    msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(zokuseiDataBiosGetName(state.count))));
    lbl_8047B610 += 5;
done:
    return;
}

/* 0x8021B364 size 0x120: fightAiCollectWazaTypes (owner line 14882) */
#pragma opt_lifetimes reset
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_80123CD4 pokemonWazaCheckValid
#define fn_8012640C pokemonGetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_80207AE0 fightOutPokemonIsZokuseiDataId
u32 fightAiCollectWazaTypes(u32 r3, u32 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80123CD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80207AE0();
#endif
  u32 uVar6;
  u32 uVar1;
  u8 cVar4;
  u16 sVar3;
  u32 uVar2;
  int bVar5;

  uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r3)); /* r3 untouched before the call (assembly) */
  for (bVar5 = 0; (u8)bVar5 < 4; bVar5 = bVar5 + 1) {
    *(u32 *)PC_PTR((r4 + (u32)(u8)bVar5 * 4)) = 0xffffffff;
  }
  uVar6 = 0;
  for (bVar5 = 0; (u8)bVar5 < 4; bVar5 = bVar5 + 1) {
    cVar4 = fn_80123CD4(PC_PTR(uVar1),(u8)bVar5);
    if (cVar4 == 1) {
      sVar3 = (int)fn_8012640C(PC_PTR(uVar1),0,0x7f,(u8)bVar5);
      if ((sVar3 != MOVE_STRUGGLE) && (sVar3 != MOVE_SHADOW_RUSH)) {
        uVar2 = fn_8011BEB4(0,sVar3,3,0);
        uVar2 = uVar2 & 0xffff;
        if (uVar2 == 9) {
          cVar4 = fn_80207AE0(PC_PTR(r3),7);
          if (cVar4 == 1) {
            uVar2 = 7;
          }
          else {
            uVar2 = 0;
          }
        }
        cVar4 = fn_80207AE0(PC_PTR(r3),uVar2);
        if (cVar4 == 0) {
          *(u32 *)PC_PTR((r4 + (uVar6 & 0xff) * 4)) = uVar2 & 0xffff;
          uVar6 = uVar6 + 1;
        }
      }
    }
  }
  return uVar6;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207AE0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80123CD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8021B484 size 0x18C: wsCmd8F (owner line 14934) */
void wsCmd8F(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
    extern u32 fightTrainerGetIrekaeFightPokemonAry();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 figthOutPokemonGetLevel();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u8 lbl_80378CCE[];
    u32 attacker;
    u32 defender;
    u32 trainer;
    u32 count;
    u32 attackerLevel;
    u32 defenderLevel;
    u32 random;
    u32 selected;
    s16 status;
    u32 proceed;
#ifdef TARGET_PC
    /* PC: fightTrainerGetIrekaeFightPokemonAry fills six native pointers (void**); a u32[6] took 48 bytes into 24,
     * overwrote the saved registers and the move-script loop crashed after Roar (sitting 42, 2026-10-27) */
    void* choices[6];
#else
    u32 choices[6];
#endif

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    defender = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    trainer = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, defender));
    count = fightTrainerGetIrekaeFightPokemonAry(trainer, choices);
    if ((u16)count != 0) {
        goto have_choices;
    }
failed:
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    goto done;

have_choices:
    attackerLevel = figthOutPokemonGetLevel(PC_PTR(attacker));
    defenderLevel = figthOutPokemonGetLevel(PC_PTR(defender));
    if ((u8)attackerLevel < (u8)defenderLevel) {
        goto random_check;
    }
success:
    proceed = 1;
    lbl_8047B610 = lbl_80378CCE;
    goto check_proceed;

random_check:
    random = GSrand();
    if ((u16)(((s32)(((u8)attackerLevel + (u8)defenderLevel) *
                     (u8)random) >> 8) + 1) >
        (s32)((defenderLevel >> 2) & 0x3f)) {
        goto success;
    }
    proceed = 0;
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));

check_proceed:
    if ((u8)proceed == 0) {
        goto done;
    }
    random = GSrand();
#ifdef TARGET_PC
    selected = PC_PTR32(choices[(u16)((s32)(u16)random % (s32)(u16)count)]);
#else
    selected = choices[(u16)((s32)(u16)random % (s32)(u16)count)];
#endif
    if (selected == 0) {
        goto failed;
    }
    status = (s16)pokemonGetStatus(PC_PTR(selected), 0, POKEMON_STATUS_ENTRY_ID, 0);
    if (status < 0) {
        goto failed;
    }
    fightFloorSetStatus(0, 0, 0x45, 0, defender);
    pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_SWITCH_TARGET, 0, status);
    pokemonSetStatus(PC_PTR(defender), 0, POKEMON_STATUS_SWITCHED_FLAG, 0, 1);
done:
    return;
}

/* 0x8021B610 size 0x18: wsCmd8E (owner line 232) */
u8* wsCmd8E(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B626 = 0;
    lbl_8047B610 = pc + 1;
    return pc;
}

/* 0x8021B628 size 0xE4: wsCmd8D (owner line 3937) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8011BBD8 wazaSetStatus
void wsCmd8D(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    u8 flag = *(u8*)(lbl_8047B610 + 1);
    s32 roll;
    u8 val;

    if (flag != 0) {
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x31, 0, flag);
    } else {
        roll = GSrand();
        val = roll % 4;
        if (val < 2) {
            val = val + 2;
        } else {
            roll = GSrand();
            val = roll % 4 + 2;
        }
        fn_8011BBD8(PC_PTR(fieldD9), 0, 0x31, 0, val);
    }
    lbl_8047B610 = lbl_8047B610 + 2;
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

/* 0x8021B70C size 0x54: WS_ABARERU (owner line 15009) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
void WS_ABARERU(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802025B8();
#endif
    extern u8 lbl_80478D78[1];
  u32 uVar1;
  u8 cVar2;

  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  cVar2 = fn_802025B8(PC_PTR(uVar1),PC_PTR(0xd));
  if (cVar2 == 2) {
    lbl_80478D78[3] = 0x75;
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021B760 size 0xD0: wsCmd8B (owner line 3978) */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8020A2B8 fightWazaBiosCopy
#define fn_801254B4 pokemonSetStatus
void wsCmd8B(void) {
    extern void fn_8020A2B8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801254B4();
#endif
    u32 ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    u32 fieldD9 = fn_8012640C(PC_PTR(ctx1), 0, 0xD9, 0);
    u32 fieldF8 = fn_8012640C(PC_PTR(ctx1), 0, 0xF8, 0);

    if (fn_802025B8(PC_PTR(ctx1), PC_PTR(CONDITION_BIDE)) == 2) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(CONDITION_BIDE), 0);
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x22), 0);
    }
    fn_8020A2B8(fieldF8, fieldD9);
    fn_801254B4(PC_PTR(ctx1), 0, 0xf5, 0, 0);
    lbl_8047B610 = lbl_8047B610 + 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020A2B8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8021B830 size 0x40: WS_KUROIKIRI (owner line 15029) */
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
#define fn_8021B870 _fightSeqWsKuroikiriSub__FPvUsPv
void WS_KUROIKIRI(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F37B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8021B870();
#endif

  fn_801F37B0(0,fn_8021B870,0,0);
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8021B870
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif

/* 0x8021B870 size 0x48: _fightSeqWsKuroikiriSub__FPvUsPv (owner line 8952) */
#define fn_802062FC fightOutPokemonCheckFightOut
s32 _fightSeqWsKuroikiriSub__FPvUsPv(void* ctx, u16 param2, void* param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
    extern void fightOutPokemonInitAbiCntAll();
    if (!fn_802062FC(ctx)) return 0;
    fightOutPokemonInitAbiCntAll(ctx);
    return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif

/* 0x8021B8B8 size 0x58: wsCmd89 (owner line 1301) */
void wsCmd89(void) {
    u8 val = *(u8*)((u8*)lbl_80379F58 + (0x1 << 16) + 0x601e);
    if (wsStatBuffChange(val & 0xF0, val & 0x0F, lbl_8047B610[1],
                    DISC_U32(lbl_8047B610 + 2)) == 0) {
        lbl_8047B610 += 6;
    }
}
