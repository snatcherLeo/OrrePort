/**
 * @file fight_range_exact_8022DCB8.c
 * @brief Strict target-order fight island, 0x8022DCB8 - 0x8022F2F8.
 *
 * Bodies are mechanically extracted from the shared pure-C candidate.
 * Compiler state and active symbol aliases are re-established per body.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/battle_messages.h"
#include "game/constants/species.h"
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

/* fightCheckTokuseiWazaKyuushuu */
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_8011BEB4 wazaGetStatus
#define fn_80205134 fightOutPokemonGetWazaZokuseiDataId
#define fn_8012640C pokemonGetStatus
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80201704 fightOutPokemonIsHpMantan
#define fn_80203B5C fightOutPokemonMaxHpWaruValue
#define fn_8011BBD8 wazaSetStatus
u8 fightCheckTokuseiWazaKyuushuu(u32 p1, u32 p2, u32 extra) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207BF4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8011BEB4();
#endif
    extern u16 fn_80205134();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
    extern u8  lbl_80379714[];
    extern u8  lbl_80379715[];
    extern u8  lbl_80379752[];
    extern u8  lbl_80379753[];
    extern u8  lbl_80379783[];
    extern u8  lbl_80379784[];
    extern u8  fn_80201704();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203B5C();
#endif

    u16 val26 = fn_80207BF4(PC_PTR(p2));
    u16 val25 = (u16)fn_8011BEB4(0, extra, 7, 0);
    u16 val27 = fn_80205134(p1);
    u8 val31 = (u8)fn_8012640C(PC_PTR(p1), 0, 0x111, 0);
    u8 result = 0;
    u32 fieldD9 = fn_8012640C(PC_PTR(p1), 0, 0xD9, 0);
    u16 val4;

    if ((u8)fn_802062FC(PC_PTR(p2)) == 0) {
        return 0;
    }
    if ((u16)extra != 0 && (u16)extra != 0x165) {
        switch (val26) {
        case 0xa:
            if (val27 == 0xd && val25 != 0) {
                lbl_8047B610 = (val31 != 0) ? (u8*)&lbl_80379715 : (u8*)&lbl_80379714;
                result = 1;
            }
            break;
        case 0xb:
            if (val27 == 0xb && val25 != 0) {
                lbl_8047B610 = (val31 != 0) ? (u8*)&lbl_80379715 : (u8*)&lbl_80379714;
                result = 1;
            }
            break;
        case 0x12:
            if (val27 == 0xa && (u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(p2), PC_PTR(CONDITION_FREEZE)) == 0) {
                if ((u8)fn_802025B8(PC_PTR(p2), PC_PTR(0x3a)) == 2) {
                    fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(p2), PC_PTR(0x3a), 0);
                    lbl_80478D78[5] = 0;
                } else {
                    lbl_80478D78[5] = 1;
                }
                lbl_8047B610 = (val31 != 0) ? (u8*)&lbl_80379784 : (u8*)&lbl_80379783;
                result = 2;
            }
            break;
        }

        if (result == 1) {
            if ((u8)fn_80201704(p2) == 1) {
                if (val31 != 0) {
                    lbl_8047B610 = (u8*)&lbl_80379753;
                } else {
                    lbl_8047B610 = (u8*)&lbl_80379752;
                }
                goto done;
            }
            fn_8011BBD8(PC_PTR(fieldD9), 0, 0x2d, 0, -fn_80203B5C(PC_PTR(p2), 4));
        }
    }
done:
    return result;
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
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205134
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif

/* fn_8022DF08 */
void fn_8022DF08(u32 ctx)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetNowTenkouDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    extern u8 fightOutPokemonIsHpMantan();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonMaxHpWaruValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsJoutaiNormal();
#endif
    extern u32 pokemonGetJoutaiMsgId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
    extern void fightOutPokemonResetSeqStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern u8 lbl_8037967E[];
    extern u8 lbl_8037969F[];
    extern u8 lbl_803796B5[];
    extern u8 lbl_80379F58[];
    extern void* lbl_8047B62C;
    u32 pokemon;
    u32 weather;
    u32 ability;
    u32 move;

    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(ctx));
    {
        u32 callResult = fightFloorGetNowTenkouDataId(0, 1);
        weather = (u8)callResult;
    }
    if (ctx == 0) {
        return;
    }
    if (fightOutPokemonCheckFightOut(PC_PTR(ctx)) == 0) {
        return;
    }

    ability = fightOutPokemonGetTokuseiDataId(PC_PTR(ctx));
    fightFloorSetStatus(0, 0, 0x36, 0, ctx);
    move = pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);

    switch ((u16)ability) {
    case ABILITY_RAIN_DISH:
        if (weather == 2 && fightOutPokemonIsHpMantan(ctx) == 0) {
            u32 amount = (u16)fightOutPokemonMaxHpWaruValue(PC_PTR(ctx), 0x10);
            wazaSetStatus(PC_PTR(move), 0, 0x2D, 0, -(s32)amount);
            wsRunScript(lbl_8047B62C, lbl_8037967E, 0);
        }
        break;

    case ABILITY_SHED_SKIN:
        if (fightOutPokemonIsJoutaiNormal(PC_PTR(ctx)) == 0) {
            u16 random = GSrand();
            s32 divisor = 3;
            if (random % divisor == 0) {
                fightFloorSetStatus(0, 0, 0x4B, 0, ctx);
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(pokemonGetJoutaiMsgId(pokemon))));
                pokemonInitJoutai(PC_PTR(pokemon));
                fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(0x17));
                fightOutPokemonResetSeqStatus(ctx, 0);
                if (fightOutPokemonIsUseHensinBuff(PC_PTR(ctx)) == 1) {
                    fightOutPokemonSetHensinPokemonStatusId(PC_PTR(ctx), 0x7C, 0, 0);
                }
                wsRunScript(lbl_8047B62C, lbl_8037969F, 0);
            }
        }
        break;

    case ABILITY_SPEED_BOOST: {
        s8 count = pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_STAGE_SPEED, 0);
        if (count < 12 && (u16)pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_TURNS_ON_FIELD, 0) != 2) {
            fightFloorSetStatus(0, 0, 0x4B, 0, ctx);
            pokemonSetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_STAGE_SPEED, 0, (s8)(count + 1));
            lbl_80379F58[0x160A4] = 0x11;
            lbl_80379F58[0x160A5] = 0;
            wsRunScript(lbl_8047B62C, lbl_803796B5, 0);
        }
        break;
    }

    case ABILITY_TRUANT: {
        u32 value = pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_TRUANT_FLAG, 0);
        value = __cntlzw(value & 0xFF);
        pokemonSetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_TRUANT_FLAG, 0, (value & 0x1FE0) >> 5);
        break;
    }
    }
}

/* fightDoTraceAll */
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
void fightDoTraceAll(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F37B0();
#endif
    fn_801F37B0(0, _fightDoTraceSub, 0, 0);
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif

/* _fightDoTraceSub */
#define fn_800FA280 GSmsgGetGSchar
#define fn_8011CB54 pokemonTokuseiDataBiosGetName
#define fn_8011CB6C pokemonTokuseiDataBiosGetPtr
#define fn_80132A38 msgctrlSetValue
#define fn_801F2598 fightFloorGetFightOutPokemonPtrRandom
#define fn_801F4C14 fightFloorSetStatus
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_802037DC fightOutPokemonGetNicknamePtr
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80207BC0 fightOutPokemonSetTokuseiDataId
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
u32 _fightDoTraceSub(int r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800FA280();
#endif
    extern u32 fn_8011CB54();
    extern u32 fn_8011CB6C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80132A38();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F2598();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802037DC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fn_802062FC();
#endif
    extern void fn_80207BC0();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern void* lbl_8047B62C;
    extern u8 lbl_80379667[];
  u32 local;
  u32 sVar3;
  u8 cVar4;
  u32 uVar1;
  u32 uVar2;

  local = r3;
  sVar3 = fn_80207BF4(PC_PTR(r3)); /* r3 untouched before the call (assembly) */
  cVar4 = fn_802062FC(PC_PTR(local));
  if (cVar4 == 0) {
    return 1;
  }
  if (((sVar3 & 0xffff) == 0x24) && (cVar4 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(local),PC_PTR(0x3c)), cVar4 == 1)) {
    uVar1 = PC_PTR32_IMPLICIT(fn_801F2598(0,1,2,local));
    cVar4 = fn_802062FC(PC_PTR(uVar1));
    if (cVar4 == 1) {
      fn_80202810(PC_PTR(local),PC_PTR(0x3c));
      uVar2 = fn_80207BF4(PC_PTR(uVar1));
      fn_80207BC0(local,uVar2);
      fn_801F4C14(0,0,0x4b,0,local);
      uVar1 = PC_PTR32_IMPLICIT(fn_802037DC(uVar1));
      fn_80132A38(0xd,uVar1);


      uVar1 = PC_PTR32_IMPLICIT(fn_800FA280(fn_8011CB54(fn_8011CB6C(uVar2)))); /* ability record -> name key -> text */
      fn_80132A38(0xe,uVar1);
      wsRunScript(lbl_8047B62C,PC_PTR32(lbl_80379667),0);
    }
  }
  return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BC0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802037DC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2598
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

/* fightDoIkakuAll */
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
void fightDoIkakuAll(u8 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F37B0();
#endif
    u8 local = param;
    fn_801F37B0(0, _fightDoIkakuSub, &local, 0);
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif

/* _fightDoIkakuSub */
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
u32 _fightDoIkakuSub(u32 r3, u32 r4, char* r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207BF4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern u8 lbl_803795FE[];
    extern u8 lbl_803795F5[];
    extern void* lbl_8047B62C;
  u16 sVar1;
  u32 uVar3;
  void* context;
  u8 cVar2;

  context = (void*)PC_PTR(r3);
  sVar1 = fn_80207BF4(context);
  cVar2 = fn_802062FC(context);
  if (cVar2 == 0) {
    return 1;
  }
  uVar3 = PC_PTR32(((*r5 == 1) ? lbl_803795F5 : lbl_803795FE));
  if ((sVar1 == ABILITY_INTIMIDATE) &&
      (cVar2 = fightOutPokemonIsPokemonJoutaiDataId(context,PC_PTR(0x3b)), cVar2 == 1)) {
      fn_80202810(context,PC_PTR(0x3b));
      fn_801F4C14(0,0,0x4b,0,PC_PTR32_IMPLICIT(context));
      wsRunScript(lbl_8047B62C,PC_PTR(uVar3),0);
  }
  return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif

/* fightOutPokemonTokuseiCheckAppear */
extern void pokemonSetStatus(u8* obj, u32 id, u32 selector, u32 subindex, u32 value);
/* RULE-EXCEPTION(user-approved): fightOutPokemonGetUseWazaDataId is defined u16 in fight_out_pokemon.c, but retail keeps the result unextended here (mr, no clrlwi); declared u32 like the file's fn_80205184 - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetUseWazaDataId(void* ctx);
#endif
#define fn_801254B4 pokemonSetStatus
#define fn_8012640C pokemonGetStatus
#define fn_801F2934 fightFloorWriteJoutaiDataId
#define fn_801F2988 fightFloorCheckWriteJoutaiDataId
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
#define fn_801F4C14 fightFloorSetStatus
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
void fightOutPokemonTokuseiCheckAppear(int r3)

{
    extern void fn_801F2934();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F2988();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F37B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
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
    extern void wsRunScript();
#endif
    extern u8 fightGetForecastForm();
    extern void* lbl_8047B62C;
    extern u8 lbl_80379F58[];
    extern u8 lbl_803795BB[];
    extern u8 lbl_8037959E[];
    extern u8 lbl_803795D8[];
    extern u8 lbl_803796D7[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _fightCheckForecastSub();
#endif
  u8 cVar3;
  int uVar2;

  u8 local_18 [12];

  if ((r3 != 0) && (cVar3 = fn_802062FC(PC_PTR(r3)), cVar3 != 0)) {
    uVar2 = (int)(u16)fn_80207BF4(PC_PTR(r3));
    switch (uVar2) {
    case 0x2:
      cVar3 = fn_801F2988(0,0x50);
      if (cVar3 != 2) {
        return;
      }
      fn_801F2934(0,0x50,0);
      fn_801F4C14(0,0,0x4b,0,r3);
      wsRunScript(lbl_8047B62C,PC_PTR32(lbl_8037959E),0);
      return;
    case 0x2d:
      cVar3 = fn_801F2988(0,0x51);
      if (cVar3 == 2) {
        fn_801F2934(0,0x51,0);
        fn_801F4C14(0,0,0x4b,0,r3);
        wsRunScript(lbl_8047B62C,PC_PTR32(lbl_803795BB),0);
      }
      break;
    case 0x46:
      cVar3 = fn_801F2988(0,0x4f);
      if (cVar3 != 2) {
        return;
      }
      fn_801F2934(0,0x4f,0);
      fn_801F4C14(0,0,0x4b,0,r3);
      wsRunScript(lbl_8047B62C,PC_PTR32(lbl_803795D8),0);
      return;
    case 0x16:
      if ((int)fn_8012640C(PC_PTR(r3),0,0x116,0) != 0) {
        return;
      }
      cVar3 = fn_802025B8(PC_PTR(r3),PC_PTR(0x3b));
      if (cVar3 == 2) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3b),0);
      }
      fn_801254B4((void*)PC_PTR(r3),0,0x116,0,1);
      return;
    case 0x24:
      if ((int)fn_8012640C(PC_PTR(r3),0,0x117,0) != 0) {
        return;
      }
      cVar3 = fn_802025B8(PC_PTR(r3),PC_PTR(0x3c));
      if (cVar3 == 2) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3c),0);
      }
      fn_801254B4((void*)PC_PTR(r3),0,0x117,0,1);
      return;
    case 0x3b:
      cVar3 = fightGetForecastForm(r3);
      if (cVar3 == 0) {
        return;
      }
      lbl_80379F58[0x1609b] = cVar3 - 1;
      fn_801F4C14(0,0,0x4b,0,r3);
      wsRunScript(lbl_8047B62C,PC_PTR32(lbl_803796D7),0);
      return;
    case 0xd:
    case 0x4d:
      local_18[0] = 0;
      fn_801F37B0(0,PC_PTR32(_fightCheckForecastSub),local_18,0);
      break;
    default:
      return;
    }
  }
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F37B0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2988
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2934
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif

/* fn_8022E6F0 */
#define fn_8011F910 pokemonAddDpFormPokemonDpFilterId
#define fn_8011FC74 pokemonIsDarkPokemon
#define fn_801254B4 pokemonSetStatus
#define fn_8012640C pokemonGetStatus
#define fn_801C3430 battleGridUpdate
#define fn_801F000C fightMainWaitFrame
#define fn_801F2A7C fightFloorGetGcHeroFightTrainerPtr
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F54A4 fightFloorGetStatus
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_801FE710 fightOutPokemonSetHensinFightPokemonStatusId
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_80204A10 fightOutPokemonIsGcHeroFightOutPokemon
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80208404 fightOutPokemonDarkPokemonEffect
#define fn_802085C4 fightOutPokemonWazaEffect
#define fn_802086E8 fightOutPokemonFreeWazaEffect
#define fn_80208750 fightOutPokemonLoadWazaEffect
#define fn_8026246C fightMenuCloseMsg
#define fn_802624CC fightMenuOpenMsg
#define fn_802653FC fightMenuFightOutPokemonRenewStatusMenu
void fightCheckRuiShadowSense(u32 r3, u8 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
    extern void fn_8011F910();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8011FC74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int std082_flagGetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801C3430();
#endif
    extern void battleGridReplaceTrainer();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerSetVisibility();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysStopWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceSysIsPlayingWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysStartWaza();
#endif
    extern void wazaSequenceOwnerFree();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerLoadWaza();
#endif
    extern u32 wazaSequenceTrainerCreate();
    extern void fightMainSetAllDispFlag();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F000C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F2A7C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
    extern u8 fightTrainerCheckItemKind();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801FB1C0();
#endif
    extern void fightTrainerWazaSeq74();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE710();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80204A10();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
    extern void fn_80208404();
    extern void fn_80208554();
    extern void fn_802085C4();
    extern void fn_802086E8();
    extern void fn_80208750();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8026246C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802624CC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802653FC();
#endif
  u8 cVar8;
  u32 uVar1;
  u16 uVar6;
  u8 cVar9;

  fn_801F54A4(0,0,0x14,0);
  if (r3 != 0) {
    cVar8 = fn_802062FC(PC_PTR(r3));
    if (cVar8 != 0) {

      cVar8 = fn_8011FC74(fn_80205B8C(PC_PTR(r3)));
      if (cVar8 != 0) {
        u32 uVar4;
        u32 uVar5;
        uVar1 = (int)fn_8012640C(PC_PTR(r3),0,0xd6,0);
        cVar8 = fn_80204A10(r3);
        if (cVar8 == 1) {
          cVar8 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd1,0);
          if (cVar8 == 0) {
            u32 uVar2;
            uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r3));
            fn_8011F910(uVar2,0,0);
            cVar8 = fn_801FECD4(PC_PTR(r3));
            if (cVar8 == 1) {
              fn_801FE7EC(PC_PTR(r3),0xc5,0,0);
            }
            uVar6 = fn_801F54A4(0,0,0x14,0);
            fn_802653FC(r3,uVar6,1);
            fn_801254B4((void*)PC_PTR(uVar1),0,0xd1,0,1);
            cVar8 = fn_801FECD4(PC_PTR(r3));
            if (cVar8 == 1) {
              fn_801FE710(PC_PTR(r3),0xd1,0);
            }
          }
        }
        else if (r4 == 0) {
          u32 uVar3;
          u32 uVar2;
          uVar2 = fn_801F2A7C(0);
          uVar3 = fn_801FB1C0(PC_PTR(uVar2),0,0x4c,0);
          cVar8 = fightTrainerCheckItemKind(uVar2);
          cVar9 = (int)fn_8012640C(PC_PTR(uVar1),0,0xd1,0);
          if (((cVar9 == 0) && (cVar9 = fn_801F54A4(0,0,0x2b,0), cVar9 == 1)) &&
             (uVar3 != 0)) {
            u16 sVar7;
            u32 uVar10;
            sVar7 = fn_801F54A4(0,0,0xd,0);
            if (sVar7 != 0x11) {
              uVar10 = 0;
            }
            else {
              uVar10 = 3;
              fn_80208750(r3,1,1,0);
              fightTrainerWazaSeq74(uVar2,0);
            }
            uVar4 = fn_801F54A4(0,0,0x36,0);
            fn_801F4C14(0,0,0x36,0,r3);
            uVar5 = wazaSequenceTrainerCreate(0x32);
            wazaSequenceOwnerLoadWaza(PC_PTR(uVar5),0xa0,4,0);
            fn_80208404(r3,0,uVar10,0);
            battleGridReplaceTrainer(uVar3,uVar5);
            fn_801C3430();
            wazaSequenceSysStartWaza(PC_PTR(uVar5),0xa0,4);
            fightMainSetAllDispFlag(0);
            wazaSequenceOwnerSetVisibility(PC_PTR(uVar5),1);
            while (1) {
              cVar9 = wazaSequenceSysIsPlayingWaza(PC_PTR(uVar5),0xa0,4);
              if (cVar9 == 0) break;
              _threadSwitch();
            }
            battleGridReplaceTrainer(uVar5,uVar3);
            fn_801C3430();
            fightMainSetAllDispFlag(1);
            wazaSequenceOwnerSetVisibility(PC_PTR(uVar5),0);
            fn_80208404(r3,0,uVar10,1);
            if (sVar7 == 0x11) {
              fn_802624CC(BATTLE_MSG_PARTNER_FIRST_SHADOW);
              fn_8026246C();
              fn_80208404(r3,0,uVar10,4);
              fn_802085C4(r3,1,1,0, (void*)0xffffffff);
              fn_80208554(r3,1,1,6);
              fightTrainerWazaSeq74(uVar2,1);
              fightTrainerWazaSeq74(uVar2,2);
              fn_80208404(r3,0,uVar10,1);
              fn_802624CC(BATTLE_MSG_PARTNER_FIRST_SNAG_URGE);
              if (cVar8 == 0) {
                fn_8026246C();
                fn_801F000C(0x40);
                fn_802624CC(0x772b);
              }
              fn_8026246C();
              fn_80208404(r3,0,uVar10,4);
            }
            else {
              uVar3 = std082_flagGetValue(0x9a0);
              if (uVar3 == 0) {
                fn_802624CC(BATTLE_MSG_PARTNER_BLACK_AURA);
              }
              else {
                fn_802624CC(BATTLE_MSG_PARTNER_SHADOW_SPOTTED);
              }
              fn_80208404(r3,0,uVar10,2);
              fn_8026246C();
            }
            fn_801254B4((void*)PC_PTR(uVar1),0,0xd1,0,1);
            cVar8 = fn_801FECD4(PC_PTR(r3));
            if (cVar8 == 1) {
              fn_801FE710(PC_PTR(r3),0xd1,0);
            }
            fn_801F4C14(0,0,0x36,0,uVar4);
            fn_80208404(r3,0,uVar10,3);
            if (sVar7 == 0x11) {
              fn_802086E8(r3,1,1);
              fightTrainerWazaSeq74(uVar2,3);
            }
            wazaSequenceSysStopWaza(PC_PTR(uVar5),0xa0,4);
            wazaSequenceOwnerFree(uVar5);
          }
        }
      }
    }
  }
  return;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802653FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802624CC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8026246C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80208750
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802086E8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802085C4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80208404
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80204A10
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE710
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F2A7C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F000C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801C3430
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011FC74
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011F910
#endif

/* _fightCheckForecastSub */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (opt_propagation off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma opt_propagation off
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_801F4C14 fightFloorSetStatus
u32 _fightCheckForecastSub(u32 r3, u32 r4, u8* r5)

{
    extern u8 lbl_80379F58[];
    extern u8 lbl_803796D7[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern u8 fightGetForecastForm();
    extern void* lbl_8047B62C;
  u8 cVar1;
  register u32 id = r3;
  u8* out = r5;

  cVar1 = fn_802062FC(PC_PTR(id));
  if (cVar1 == 0) {
    return 1;
  }
  cVar1 = fightGetForecastForm(id);
  if (cVar1 != 0) {
    lbl_80379F58[0x1609b] = cVar1 + -1;
    fn_801F4C14(0,0,0x4b,0,id);
    wsRunScript(lbl_8047B62C,lbl_803796D7,0);
    if (out != (void *)0) {
      *out = 1;
    }
  }
  return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif

/* fightGetForecastForm */
#pragma opt_propagation reset
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80203D3C figthOutPokemonGetPokemonDataId
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId
#define fn_801F453C fightFloorGetNowTenkouDataId
#define fn_80207AE0 fightOutPokemonIsZokuseiDataId
#define fn_80207B5C fightOutPokemonSetZokuseiDataId
u8 fightGetForecastForm(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802062FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203D3C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207BF4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fn_801F453C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207AE0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207B5C(void*, u8, u32);
#endif

    u16 x;
    u16 y;
    u8 kind;
    u8 count;

    if ((u8)fn_802062FC(ctx) == 0) {
        return 0;
    }
    x = fn_80203D3C(ctx);
    y = fn_80207BF4(ctx);
    if (x != SPECIES_CASTFORM || y != ABILITY_FORECAST) {
        return 0;
    }

    kind = (u8)fn_801F453C(0, 1);
    count = 0;

    if (kind == 0 || kind == 3) {
        if ((u8)fn_80207AE0(ctx, 0) == 0) {
            for (count = 0; count < 2; count++) {
                fn_80207B5C(ctx, count, 0);
            }
            count = 1;
        }
    }
    if (kind == 1) {
        if ((u8)fn_80207AE0(ctx, 0xa) == 0) {
            for (count = 0; count < 2; count++) {
                fn_80207B5C(ctx, count, 0xa);
            }
            count = 2;
        }
    }
    if (kind == 2) {
        if ((u8)fn_80207AE0(ctx, 0xb) == 0) {
            for (count = 0; count < 2; count++) {
                fn_80207B5C(ctx, count, 0xb);
            }
            count = 3;
        }
    }
    if (kind == 4) {
        if ((u8)fn_80207AE0(ctx, 0xf) == 0) {
            for (count = 0; count < 2; count++) {
                fn_80207B5C(ctx, count, 0xf);
            }
            count = 4;
        }
    }
    return count;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207B5C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207AE0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F453C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80207BF4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80203D3C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif

/* _fightCheckDarkPokemonAppear */
#define fn_8011FC74 pokemonIsDarkPokemon
#define fn_8012640C pokemonGetStatus
#define fn_801F4C14 fightFloorSetStatus
#define fn_801F54A4 fightFloorGetStatus
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
#define fn_80204A10 fightOutPokemonIsGcHeroFightOutPokemon
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_802062FC fightOutPokemonCheckFightOut
#define fn_80208404 fightOutPokemonDarkPokemonEffect
#define fn_8026246C fightMenuCloseMsg
#define fn_802624CC fightMenuOpenMsg
#define fn_802653FC fightMenuFightOutPokemonRenewStatusMenu
u32 _fightCheckDarkPokemonAppear(u32 r3, u8 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
    extern u32 pokemonGetHyperModeRate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8011FC74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
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
    extern u8 fn_80204A10();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802062FC();
#endif
    extern void fightOutPokemonDarkPokemonKaijoEffect();
    extern void fn_80208404();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8026246C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802624CC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuStatusMenuClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802653FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80265598();
#endif
  u16 uVar4;
  u32 uVar1;
  u8 cVar6;
  u32 uVar2;
  u32 sVar5;
  int uVar3;
  int randomResult;

  uVar4 = fn_801F54A4(0,0,0x14,0);
  if (r3 == 0) {
    uVar1 = 0;
  }
  else {
    cVar6 = fn_802062FC(PC_PTR(r3));
    if (cVar6 == 0) {
      uVar1 = 0;
    }
    else {
      cVar6 = fn_80204A10(r3);
      if (cVar6 == 0) {
        uVar1 = 0;
      }
      else {
        cVar6 = fn_801F54A4(0,0,0x31,0);
        if (cVar6 == 0) {
          uVar1 = 0;
        }
        else {

          cVar6 = fn_8011FC74(fn_80205B8C(PC_PTR(r3))); /* r3 = the Pokemon pointer from the call before (assembly) */
          if (cVar6 == 0) {
            uVar1 = 0;
          }
          else {
            fn_8012640C(PC_PTR(r3),0,0xee,0);
            if (r4 == 0) {
              cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3e));
              if (cVar6 == 1) {
                return 0;
              }
              cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(CONDITION_SLEEP));
              if (cVar6 == 1) {
                return 0;
              }

              uVar2 = pokemonGetHyperModeRate(fn_80205B8C(PC_PTR(r3)));
              sVar5 = fn_80205184((void*)PC_PTR(r3));
              uVar3 = GSrand();
              if (((int)((uVar3 & 0xffff) % 100) < (int)(uVar2 & 0xff)) &&
                  ((sVar5 & 0xffff) == 0x164)) {
                fn_80208404(r3,1,1,0);
                uVar1 = fn_801F54A4(0,0,0x36,0);
                fn_801F4C14(0,0,0x36,0,r3);
                cVar6 = fn_802025B8(PC_PTR(r3),PC_PTR(0x3e));
                if (cVar6 == 2) {
                  fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3e),0);
                }
                cVar6 = fn_801FECD4(PC_PTR(r3));
                if (cVar6 == 1) {
                  fn_801FE7EC(PC_PTR(r3),200,0,0);
                }
                fn_80208404(r3,1,1,1);
                fn_802653FC(r3,uVar4,1);
                fn_802624CC(BATTLE_MSG_ENTERED_HYPER_MODE);
                fn_80208404(r3,1,1,2);
                fn_8026246C();
                fightMenuStatusMenuClose(r3,uVar4,0);
                fn_801F4C14(0,0,0x36,0,uVar1);
                fn_80208404(r3,1,1,3);
              }
              else {
                return 0;
              }
            }
            else if (r4 == 1) {
              cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3e));
              if (cVar6 == 0) {
                return 0;
              }
              fn_80208404(r3,0,2,0);
              uVar1 = fn_801F54A4(0,0,0x36,0);
              fn_801F4C14(0,0,0x36,0,r3);
              fn_80208404(r3,0,2,1);
              fn_80265598(r3,uVar4,1);
              fn_802624CC(BATTLE_MSG_IS_IN_HYPER_MODE);
              fn_80208404(r3,0,2,2);
              fn_8026246C();
              fightMenuStatusMenuClose(r3,uVar4,0);
              fn_801F4C14(0,0,0x36,0,uVar1);
              fn_80208404(r3,0,2,3);
            }
            else if (r4 == 2) {
              cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x3e));
              if (cVar6 == 0) {
                return 0;
              }
              randomResult = (u16)GSrand();
              if ((randomResult % 0x100) != 0) {
                return 0;
              }
              if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(CONDITION_SLEEP)) == 1) {
                __memcpy((void*)PC_PTR(r3),(void*)PC_PTR(r3),0);
              }
              fightOutPokemonDarkPokemonKaijoEffect(r3,1,1,0,0);
              uVar1 = fn_801F54A4(0,0,0x36,0);
              fn_801F4C14(0,0,0x36,0,r3);
              fn_80202810(PC_PTR(r3),PC_PTR(0x3e));
              fn_80202810(PC_PTR(r3),PC_PTR(8));
              fn_80202810(PC_PTR(r3),PC_PTR(0x17));
              cVar6 = fn_801FECD4(PC_PTR(r3));
              if (cVar6 == 1) {
                fn_801FE7EC(PC_PTR(r3),200,0,0);
              }
              cVar6 = fn_801FECD4(PC_PTR(r3));
              if (cVar6 == 1) {
                fn_801FE7EC(PC_PTR(r3),0x7c,0,0);
              }
              fightOutPokemonDarkPokemonKaijoEffect(r3,1,1,0x771e,1);
              fightOutPokemonDarkPokemonKaijoEffect(r3,1,1,0,2);
              fn_8026246C();
              fightMenuStatusMenuClose(r3,uVar4,0);
              fn_801F4C14(0,0,0x36,0,uVar1);
              fightOutPokemonDarkPokemonKaijoEffect(r3,1,1,0,3);
            }
            uVar1 = 1;
          }
        }
      }
    }
  }
  return uVar1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802653FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802624CC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8026246C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80208404
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802062FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205184
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80204A10
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
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F4C14
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011FC74
#endif
