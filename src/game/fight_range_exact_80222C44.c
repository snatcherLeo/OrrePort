/**
 * @file fight_range_exact_80222C44.c
 * @brief Strict target-order fight island, 0x80222C44 - 0x802230BC.
 */
#include "dolphin/types.h"
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
extern u32 battleCameraSelect(); /* returns u8 (battle_main.c: useSimpleCamera check); same type as the block-scope declaration below */
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



/* wsCmd24 */
#define fn_8011BEB4 wazaGetStatus
#define fn_801C2D5C battleCameraDoFull
#define fn_801C2D68 battleCameraDoSimple
#define fn_801C3430 battleGridUpdate
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F1700 fightFloorIsUseFightTimerCommand
#define fn_801F2B5C fightFloorLoopValidFightTrainer
#define fn_801F32B0 fightFloorIsIrekaeFightOutPokemon
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
#define fn_801F3984 fightFloorSetFightResultId
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F7174 fightSideGetFightPokemonNokoriHp
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_80261954 fightMenuCloseInfoMenu
#define fn_80261AD0 fightMenuOpenInfoMenu
#define fn_802658C8 fightTimerCommandTerminate
#define fn_802659A4 fightTimerCommandBlock
#define fn_802659F8 fightTimerCommandStart
#define fn_80265A6C fightTimerCommandInit
void wsCmd24(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205184();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F7174(u32, u16, u16);
#endif
    extern u8  fn_801F3984();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
    extern u32 menuCBRule_ProhibitionSuicide();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
    extern u8  fn_801F32B0();
    extern void fightDbgMenuCheck();
    extern u8  fn_801F1700();
    extern void fn_80265A6C();
    extern void fn_802659F8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801C3430(void);
#endif
    extern u32 battleCameraSelect();
    extern void fn_80261AD0();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F2B5C();
#endif
    extern void fn_80261954();
    extern void fn_801C2D68(void);
    extern void fn_801C2D5C(void);
    extern void fn_802659A4();
    extern void fn_802658C8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F37B0();
#endif
    extern void _wsCmd24SelectIrekaeSub();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightCheckFaintedFightOutPokemonSub();
#endif

    u16 a17;
    u16 a16;
    u8 a34;
    u32 ctx1;
    u32 moveId;
    u8 opByte;
    u32 ctx2;
    u32 val77AF4;
    u16 t9;
    u8 flag;
    u32 ctx4;
    u32 ctx5;
    u32 h;

    opByte = (u8)DISC_U32(lbl_8047B610 + 1);
    ctx1 = PC_PTR32_IMPLICIT(fn_801F025C(0x11, 0));
    moveId = fn_80205184(PC_PTR(ctx1));
    a16 = (u16)fn_801F54A4(0, 0, 0x16, 0);
    a17 = (u16)fn_801F54A4(0, 0, 0x17, 0);

    ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(5, 0));
    if (fn_801F7174(ctx2, a16, a17) == 0) {
        fn_801F3984(0, 2);
    }

    ctx2 = PC_PTR32_IMPLICIT(fn_801F025C(4, 0));
    if (fn_801F7174(ctx2, a16, a17) == 0) {
        fn_801F3984(0, 3);
    }

    if ((u16)std103_battleGetResult() == 0) goto opByteCheck;

    a34 = (u8)fn_801F54A4(0, 0, 0x34, 0);
    val77AF4 = menuCBRule_ProhibitionSuicide();
    t9 = (u16)fn_8011BEB4(0, moveId, 9, 0);

    if (a34 == 1 && (u8)val77AF4 == 1 && t9 == 7) {
        flag = 1;
    } else {
        flag = 0;
    }

    if (flag != 1) goto advance;
    if ((u16)std103_battleGetResult() != 7) goto advance;

    fn_801F3984(0, 0);
    ctx4 = PC_PTR32_IMPLICIT(fn_801F025C(2, ctx1));
    ctx5 = PC_PTR32_IMPLICIT(fn_801F025C(4, 0));
    if (ctx5 == ctx4) {
        fn_801F3984(0, 3);
    } else {
        fn_801F3984(0, 2);
    }
    goto advance;

opByteCheck:
    if (opByte != 1) goto advance;
    if ((u8)fn_801F32B0(0) == 0) goto msgAdvance;

    fightDbgMenuCheck();
    if ((u16)std103_battleGetResult() != 0) {
        lbl_8047B610 = lbl_8047B610 + 5;
        return;
    }

    if ((u8)fn_801F1700(0) == 1) {
        fn_80265A6C();
        fn_802659F8();
    }
    fn_801C3430();
    h = battleCameraSelect(0);
    fn_80261AD0(-1);
    fn_801F2B5C(0, PC_PTR32(_wsCmd24SelectIrekaeSub), 0, 1);
    fn_80261954(0);
    if ((u8)h == 1) {
        fn_801C2D68();
    } else {
        fn_801C2D5C();
    }

    if ((u8)fn_801F1700(0) == 1) {
        fn_802659A4();
        fn_802658C8();
    }
    if ((u16)std103_battleGetResult() == 0) goto msgAdvance;
    lbl_8047B610 = lbl_8047B610 + 5;
    return;

msgAdvance:
    fn_801F37B0(0, PC_PTR32(_fightCheckFaintedFightOutPokemonSub), 0, 0);
advance:
    lbl_8047B610 = lbl_8047B610 + 5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801C2D5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801C2D68
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801C3430
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1700
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F32B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F3984
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F7174
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80261954
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80261AD0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802658C8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802659A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802659F8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80265A6C
#endif

/* _fightCheckFaintedFightOutPokemonSub */
u32 _fightCheckFaintedFightOutPokemonSub(u32 r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void battleGridUpdate();
#endif
    extern void fightFloorGetFightOutPokemonPtrToHeroPtr();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonInit();
#endif
    extern void fightPokemonInit();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern void* lbl_8047B62C;
    extern u8 lbl_8037879E[];
  void* ctx;
  u32 uVar1;
  u32 uVar2;
  u32 uVar3;
  u32 uVar4;
  u8 cVar6;
  u8 cVar7;
  u8 cVar8;
  int iVar5;

  ctx = (void*)PC_PTR(r3);
  fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0,ctx);
  fightFloorGetFightOutPokemonPtrToHeroPtr(0,ctx);
  uVar1 = (int)pokemonGetStatus(ctx,0,POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR,0);
  uVar2 = (int)pokemonGetStatus(ctx,0,POKEMON_STATUS_BATTLE_POKEMON_PTR,0);
  uVar3 = (int)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_PARTY_RECORD_PTR,0);
  uVar4 = (int)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_RECORD_COPY_PTR,0);
  cVar6 = (int)pokemonGetStatus(ctx,0,POKEMON_STATUS_FAINTED_FLAG,0);
  cVar7 = (int)pokemonGetStatus(PC_PTR(uVar2),0,POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG,0);
  cVar8 = fightOutPokemonCheckFightOut(ctx);
  if (cVar8 == 0) {
    iVar5 = (int)pokemonGetStatus(ctx,0,POKEMON_STATUS_SWITCH_TARGET,0);
    if (iVar5 < 0) {
      if (cVar6 == 1) {
        fightOutPokemonInit(PC_PTR32_IMPLICIT(ctx));
        battleGridUpdate();
      }
      if (cVar7 == 1) {
        pokemonInit(PC_PTR(uVar3));
        pokemonInit(PC_PTR(uVar4));
        fightPokemonInit(uVar1);
        fightPokemonInit(uVar2);
        fightOutPokemonInit(PC_PTR32_IMPLICIT(ctx));
        battleGridUpdate();
      }
    }
    else {
      fightFloorSetStatus(0,0,0x45,0,PC_PTR32_IMPLICIT(ctx));
      fightFloorSetStatus(0,0,0x46,0,PC_PTR32_IMPLICIT(ctx));
      if (cVar7 == 1) {
        pokemonInit(PC_PTR(uVar3));
        pokemonInit(PC_PTR(uVar4));
        fightPokemonInit(uVar1);
        fightPokemonInit(uVar2);
      }
      wsRunScript(lbl_8047B62C,PC_PTR32(lbl_8037879E),0);
    }
  }
  return 1;
}
