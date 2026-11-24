#ifndef GAME_EFFECT_EFFECT_UTIL_TYPES_H
#define GAME_EFFECT_EFFECT_UTIL_TYPES_H

/**
 * @file effect_util_types.h
 * @brief Shared types and cross-TU declarations for the functions split
 *        out of the former game/effect/effect_util.c CodeCandidate bucket
 *        (0x8013151C - 0x80137114) into game/sex.c, game/msgctrl.c,
 *        game/dbgMenu.c, game/pcbox.c, game/gamedata.c, game/gamedataBios.c,
 *        game/gamedatasaveBios.c, game/status.c, game/koukaBios.c,
 *        game/kouka.c, game/tenkouBios.c and game/tikeiBios.c.
 *
 * All functions declared here are asm-only/partially-decompiled
 * CodeCandidate units (not linked); this header exists purely so each
 * split TU can see the others' signatures without redeclaration
 * mismatches.
 */

#include "dolphin/types.h"
#include "game/msgctrl_obj.h"

typedef struct EffectUtilByteEntry {
    u8 value;
    u8 pad[7];
} EffectUtilByteEntry;

extern EffectUtilByteEntry sex_data[];

extern u32 lbl_8047ADC8;

extern u32 lbl_8047ADCC;

extern u32 lbl_8047ADD0;

extern u32 lbl_8047ADE4;

extern u32 lbl_8047ADE8;

extern u32 lbl_8047ADEC;

extern u32 lbl_8047ADF0;

extern u32 lbl_8047ADF4;

extern u32 lbl_8047ADF8;

extern u32 lbl_8047ADFC;

extern u32 lbl_8047AE00;

extern u32 lbl_8047AE04;

extern u32 lbl_8047AE08;

extern u32 _SPEABI_NAMET;

extern u32 _TRAINER_TYPE;

extern u32 _TRAINER_NAME;

extern u32 _TRAINER_LOSE;

extern u32 _TRAINER_ENENAME;

extern u32 _TRAINER_ENENAME2;

extern u32 _TRAINER_CLIENTNO;

extern u32 _WAZA_NAME;

extern u32 _ITEM_NAME;

extern u32 _PASO_NAME;

extern u32 lbl_8047AE5C;

extern u32 _Pokemon;

extern u32 _Pokemon2;

extern u32 _MenuPokemon;

extern u32 _MenuMsg;

extern u32 _MenuMsg2;

extern u32 _String;

extern u32 _String2;

extern u32 _MenuMsgID;

extern u32 _MenuMsgID2;

extern u16 _Npc;

extern u8 _dbgMenuEnable;

typedef struct EffectLinkedStatusRow {
    u16 links[8];
} EffectLinkedStatusRow;

typedef struct EffectTraceFxEntry {
    u8 kind;
    u8 pad_01[7];
    u32 effectId;
    /* the weather's three battle messages (verified from the US data: sandstorm 0x764F "brewed", 0x7650 "rages",
     * 0x7651 "subsided"; rain and hail the same pattern). Typed so the PC's data generator converts them: as padding
     * they stayed big-endian and the battle read "rages" as 0x50760000 (P12's battle 208, 2026-10-26) */
    u32 msgIds[3];
} EffectTraceFxEntry;

typedef struct EffectTraceEntry {
    u8 kind;
    u8 pad_01;
    u16 value0;
    u16 value1;
    u8 pad_06[6];
} EffectTraceEntry;

typedef union EffectParamFirstWord {
    struct {
        u8 field_00;
        u8 field_01;
        u8 field_02;
        u8 field_03;
    } byte;
    u32 word;
} EffectParamFirstWord;

typedef struct EffectParamBlock {
    EffectParamFirstWord word0;
    u32 field_04;
    u32 field_08;
    u32 field_0C;
    u32 field_10;
    u32 field_14;
    f32 field_18;
    u32 field_1C;
    u8 field_20;
    u8 field_21;
    u8 field_22;
} EffectParamBlock;


/* lbl_803635C0 (GSEffectGlobals) is only used by GSeffectGetData, which was
 * merged into game/effect/gs_effect.c (where it is already declared as
 * `extern u8 lbl_803635C0[];` -- see that file). Not redeclared here to
 * avoid depending on gs_effect.h's GSEffectGlobals type in every TU. */

extern EffectTraceFxEntry tenkou_data[];  /* trace fx table (BSS) */

extern EffectTraceEntry tikei_data[];  /* trace table (BSS) */

extern u32 tenkou_data_number;  /* effect count (SDA) */

extern u32 tikei_data_number;  /* trace count (SDA) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   menuIsCheck(u32 objID);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose(u32 arg1);
#endif

u32 _msgctrlSideName__FP15FightOutPokemonUc(u32 arg1, u32 arg2); /* returns the side's name text (the msgctrl callbacks pass it on) */

s32 _dbgMenuGetItemNo__FP14tagWINDOW_WORKl(void* obj, s32 offset);

extern u16  _Tribe;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTrainerKindDataBiosGetPtr(u16 handle);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerKindDataBiosGetPrefixName(); /* takes the kind data pointer */
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataAttestCreate(void*, u8, u8, u8, u8);
#endif

extern void gamedataAttestInit(void* ptr);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gamedataGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataSetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 statusGetStatus(u32, u32, u32, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 statusSetStatus(u32, u32, u32, u32, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowSearchID(s32 key);
#endif

extern void* _dbgMenuGetWin__Fl(s32 offset);

extern s32 _dbgMenuGetItemNo__FP14tagWINDOW_WORKl(void* obj, s32 offset);

extern s32 _dbgMenuGetIndex__FP14tagWINDOW_WORK(void* obj);

extern void* gamedatasaveBiosGetPtr(void* ptr);

extern u8 gamedatasaveBiosGetOptionNoVibration(void* ptr);

extern u32 gamedatasaveBiosGetFloorid(void* ptr);

extern u32 gamedatasaveBiosGetSavecount(void* ptr);

extern u8 gamedatasaveBiosGetFloorposindex(void* ptr);

extern u8 gamedatasaveBiosGetOptionAudio(void* ptr);

extern u32 gamedatasaveBiosGetSavernd(void* ptr);

extern u32 gamedatasaveBiosGetPrevfloorid(void* ptr);

extern u32 gamedataBiosGetGamedataAtttestPtr(void* ptr);

extern u8 gamedataAttestBiosGetVerId(void* ptr);

extern u8 gamedataAttestBiosGetGenId(void* ptr);

extern u8 gamedataAttestBiosGetAreaId(void* ptr);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 gamedataAttestBiosGetLangareaId(void* ptr);
#endif

extern u32 pcboxGetStatus();

extern u32 koukaLinkDataBiosGetKouka(u32 index, u32 sub);

extern u32 koukaDataBiosGetLink(u32 index);

extern u32 koukaDataBiosGetStatusKind(u32 index);

extern u32 koukaDataBiosGetVar(u32 index);

extern s16 koukaDataBiosGetValue(u32 index, u32 subIndex);

extern u16 koukaDataBiosGetStatus(u32 index);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpenCustom(u32, ...);
#endif

extern u16* _msgctrlMakeDigit__FPUslUll(void* output, s32 length, u32 value,
                                      s32 type);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(u32 id, u32 value);
#endif

extern s32 _dbgMenuSub__Fl(s32 offset);

extern u32 _dbgMenuGetMenuNum__FP14tagWINDOW_WORKPl(u32 arg0, u32* outMax);

extern u32 _dbgMenuCheckTerminate__FP14tagWINDOW_WORKl(void* obj, s32 offset);

extern s32 _dbgMenuGetMenuNo__Fl(s32 key);

extern u32 _dbgMenuGetMsgID__FP14tagWINDOW_WORKl(void* obj, s32 offset);

extern s32 _dbgMenuGetLink__Fl(s32 idx);

extern u32 dbgMenuGetRootMenu(void);

extern s32 dbgMenuGetLink__Fl(s32 idx);

extern u32 debugMenuGetNum__Fv(void);

extern void gamedataInit(void*);

extern void fn_801353C0(void*, u8, u8, u8, u8);

extern void fn_80135708(void*);

extern void gamedataAttestBiosSetLangareaId(void* ptr, u8 val);

extern void gamedataAttestBiosSetAreaId(void* ptr, u8 val);

extern void gamedataAttestBiosSetGenId(void* ptr, u8 val);

extern void gamedataAttestBiosSetVerId(void* ptr, u8 val);

extern void gamedataBiosSetGamedataAtttestPtr(u32* dst, u32* src);

extern void gamedatasaveBiosSetOptionAudio(void* ptr, u8 val);

extern void gamedatasaveBiosSetOptionNoVibration(void* ptr, u8 val);

extern void gamedatasaveBiosSetFloorposindex(void* ptr, u8 val);

extern void gamedatasaveBiosSetPrevfloorid(void* ptr, u32 val);

extern void gamedatasaveBiosSetPlaytime(void* ptr, f32 val);

extern void gamedatasaveBiosSetFloorid(void* ptr, u32 val);

extern void gamedatasaveBiosSetSavecount(void* ptr, u32 val);

extern void gamedatasaveBiosSetSavernd(void* ptr, u32 val);

extern f32 gamedatasaveBiosGetPlaytime(void* ptr);

extern void gamedatasaveBiosSetPtr(void* dst, void* src);

extern void gamedatasaveInit(void*);

extern s32 _koukaOneExec__FUlPvPvPl(u32 index, void* arg1, void* arg2, s32* out);

typedef s32 (*EffectUtilCountFunc)(void);

typedef s32 (*EffectUtilEntryCallback)(s32, s32);

typedef struct EffectUtilEntry {
    u8 flags;
    u8 pad_01;
    s16 link;
    u32 value;
    EffectUtilEntryCallback callback;
} EffectUtilEntry;

typedef EffectUtilEntry* (*EffectUtilEntryFunc)(s32);

typedef struct EffectStatusTableEntry {
    u8 statusKind;
    u8 mode;
    u16 statusSub;
    u16 linkedIndex;
    s16 amount;
    s16 divisor;
} EffectStatusTableEntry;

extern u8  lbl_80426FF0[];

extern u32 lbl_8047AE94;

extern u8  lbl_80427010[];

extern u32 lbl_8047AE68;

extern u16  _PokemonID;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonDataBiosGetPtr(u16 handle);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonDataBiosGetName(); /* takes the species data pointer */
#endif

extern u32 lbl_8047AE84;

extern u8 lbl_80427030[];

extern u32 lbl_8047AEA8;

extern u8 lbl_80427050[];

extern u8 lbl_80427070[];

extern u32 lbl_8047AE80;

extern u8 lbl_80427090[];

extern u32 lbl_8047AE6C;

extern u8 lbl_804270B0[];

extern u8 lbl_804270D0[];

extern u8 lbl_804270F0[];

extern u8 lbl_80427110[];

extern u8 lbl_80427130[];

extern u32 _SIDE_DEFENCE_NAMENO;

extern u32 _SIDE_DEFENCE_NAMEWO;

extern u32 _SIDE_DEFENCE_NAMEHA;

extern u32 _SIDE_ATTACK_NAMENO;

extern u32 _SIDE_ATTACK_NAMEWO;

extern u32 _SIDE_ATTACK_NAMEHA;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightFloorGetFightOutPokemonPtrToFightTrainerPtr(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801F18DC(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTrainerGetNamePtr(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonGetNicknamePtr(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgGetGSchar(void);
#endif

extern u32 _CLIENTNOWORK;

extern u32 _TSUIKA_MONS;

extern u32 _CLIENT_MONS;

extern u32 _DEFENCE_MONS;

extern u32 _ATTACK_MONS;

extern u16  _Waza;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaDataBiosGetPtr(u16 handle);
#endif

extern u32 wazaDataBiosGetName(); /* takes the move data pointer */

extern u8 lbl_80427150[];

extern u8 lbl_80427170[];

extern u8  lbl_80427190[];

extern u32 lbl_8047AE58;

extern u8  lbl_804271B0[];

extern u32 lbl_8047AE54;

extern u16  _Item2;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetPtr(u16 handle);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  itemDataBiosGetName(void);
#endif

extern u16 _Item;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  savedataGetStatus(u32 side, u32 slotType);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  heroBiosGetHizukiNamePtr(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroBiosGetNamePtr(void);
#endif

extern void jumptable_80363630();

extern u16 _SndID;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _toolentryAlloc__FUl(u32 size);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmemLock(u32 handle);
#endif

extern u32 lbl_8047AEB4;

extern u32 lbl_8047AEC0;

extern u32 lbl_8047AECC;

extern u32 lbl_8047AEC8;

extern u16 lbl_8047AEB8;

extern u32 lbl_8047AEB0;

extern u16 lbl_8047AEC4;

extern u32 lbl_8047AEBC;

extern void fn_800D37D4(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(f32, f32, f32, f32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(u32, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(u32, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(u32, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos(f32, f32, f32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(u32, u32, u32, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif

extern f32 lbl_8047D0F0;

extern f32 lbl_8047D0F4;

extern f32 lbl_8047D0F8;

extern u8 lbl_80478AC0[];

extern f32 lbl_8047D0FC;

extern u8 lbl_80478AC4[];

extern f32 lbl_8047D100;

extern f32 lbl_8047D104;

extern u32  GSmemOptimize(void);

extern void GSlogWrite(const char* fmt, ...);

extern const char lbl_80272AB8[];

extern u8 GSmemDumpMap(u32 a, u32 b);

extern const char lbl_80272AE0[];

extern const char lbl_80272AF0[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D3074(u32 mode);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSresGetResource(void);
#endif

extern u32 lbl_80478820;

extern u8   lbl_8047AED9;

extern void GSmodelSetShadowDebug(u32 val);

extern u8   lbl_8047AED8;

extern void fn_800D4610(u32 val);

extern u32  lbl_8047AED4;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowGetKeyInfo(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 menuDataBiosGetType(u32 arg);
#endif

extern u32  _menu_data_number;

extern u8   lbl_8047AED1;

extern u32 debugMenuDataGetNum;

extern u32 lbl_8047AEDC;

extern u32 debugMenuDataGetPtr;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(u32 val);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgetVersionMinor(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80140A9C(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(u8* ptr);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharCpy(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInit(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonAllKaihuku(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgGetText(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInitAry(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80142A88(void);
#endif

extern f64 lbl_8047D108;

extern void jumptable_80363A9C();

extern f32 lbl_8047D110;

extern u32 heroSetStatus();

extern u32 itemSetStatus();

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonSetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaSetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorSetStatus();
#endif

extern u32 fightSideSetStatus();

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerSetStatus();
#endif

extern void jumptable_80363AC8();

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightSideGetStatus();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetStatus();
#endif

extern void jumptable_80363AF0();

extern u32 kouka_link_data_number;

extern EffectLinkedStatusRow kouka_link_data[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId(void* arg, u16 handle);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D37CC(void);
#endif

extern u8* fn_80137780(u8* work, u8* params);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(void);
#endif

extern void GSmemAlloc(void);

extern void GStextureLoad(void);

extern void fn_800EFD14(void);

extern u8* fn_8013757C(u8* work, u8* params, s32 frames);

extern void GStextureSetWrap(void);

extern void wazaSequenceSysGetResID(void);

extern void loadArchivePtr(void);

extern void loadModel(void);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetVisibility(void);
#endif

extern void fn_8013735C(void);

extern void envMapAddKey(void);

extern void fn_80137114(void);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 n);
#endif

extern u32 jumptable_80363C70[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif

extern f64 lbl_8047D128;

extern f32 lbl_8047D118;

extern u8 lbl_80314AE8[];

extern u8 lbl_80314638[];

extern f32 lbl_8047D11C;

extern f32 lbl_8047D120;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSsndGetStatus(u16);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std104_soundSetBackgroundMusic(u16, u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgAdjustAlign(void);
#endif

extern void GSmsgSetColor(void);

extern void GSmsgInitRuby(void);

extern f64 lbl_8047D0E0;

extern void GSmovieClose(void);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif

extern u8 GSmovieIsActive(void);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpen(u32, u32);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void sprintf(u8*, u8*, ...);
#endif

extern void GSmovieOpen(u8*, u32);

extern u8 lbl_80272AA8[];

extern u8 lbl_8047D0E8[4];

extern void mailChkReceiveMail(void);

extern BOOL mailAddMailbox(s32 mailId);

extern void mailMainReceiveStart(void);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroMoveIsMember(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroMoveDismissMember(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8012F1FC(void);
#endif

#endif /* GAME_EFFECT_EFFECT_UTIL_TYPES_H */
