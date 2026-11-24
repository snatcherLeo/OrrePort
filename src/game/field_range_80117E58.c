/**
 * @file field_range_80117E58.c
 * @brief GSfield world segment -- split from gs_field_world.c.
 *
 * XD source unit: floor main/exec + joutai accessor cluster (XD counterparts discontiguous)
 * Address range: 0x80117E58 - 0x8011BA0C (~62 functions)
 *
 * Split from src/game/gs_field_world.c (physical XD source-unit split of
 * the 734-function field-world bucket into its 12 constituent XD source
 * units). See gs_field_world.c split history for the address-range
 * evidence (anchor-name monotonicity checks) used to place this boundary.
 */
#include "dolphin/types.h"
#include "game/ps.h"
#include "game/world/gs_field.h"
#include "hsd/hsd_jobj.h"
#include "pc/disc_types.h" /* DISC_U16 / DISC_U32: room data stays big-endian on the PC */

typedef struct GSpart GSpart;
struct GSmodel;

extern void psSetParticleVisibility(PSGeneratorState* generator, u32 visible);
/* ===== External SDK / engine functions ===== */
extern void  GSlogWrite(const char* fmt, ...);         /* OSReport / GSlog */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
/* GSmem */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   _toolentryAlloc__FUl(u32 size);                     /* GSmemAllocRaw */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);                   /* GSmemGetPtr */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u16 handle);                   /* GSmemLock */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmemFree(u16 handle);                   /* GSmemFree */
#endif
/* Matrix / vector */
extern void  PSMTXMultVec(void* mtx, void* vec, void* out); /* MTXMultVec3 */
extern void  PSVECSubtract(void* out, void* in, f32 s);     /* VEC normalize */
/* GSgfx renderer */
extern void  gfxVtxDescSetAttr(void* handle, u32 a, u32 b, u32 c,
                          u32 d, u32 e, u32 f, u32 g);     /* GSgfx draw setup */
extern void* gfxVtxDescAlloc(void);                             /* GSgfx create render obj */
/* Real named labels referenced by sibling field translation units. */
extern void _modelSetRotateEulerToQuatAll__FP9_HSD_JObj();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void cos();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataAttestCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataAttestCreate();
#endif
extern void gamedataAttestInit();
extern void gamedataCreate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharCmp();
#endif
extern void GScharLenCpy();
extern void GSflagClear();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgFontClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 itemGetStatus();
#endif
extern void LCStoreData();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void memoDataSet();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuSubGetPokemonSexForFightDisp();
#endif
extern void OSGetTick();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void psInitAppSRT();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void psInitParticle();
#endif
extern void psSetGeneratorAngleRadiusScale();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void sin();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* statusGetStatus(u32, u8*, u32, u32, u32);
#endif
/* GSfloor / GScolsys */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* std092_getCurrentRoomID(void);                             /* GSfloor get active */
#endif
extern void  GScolsys2GetObjEnable(u32 triIdx, void* outFlag);        /* GScolsys query */
/* ===== Index lookup globals ===== */
extern u8 lbl_8035BBA8[];  /* NPC table (BSS) */
extern u8 waza_func_data[];  /* the move-function table (trainer AI per-move handlers) */
extern u32 lbl_80478B48;  /* NPC count (SDA) */
extern u32 waza_func_data_number;  /* its entry count */
extern u32 lbl_80478F90;  /* obj header ptr (SDA) */
extern u32 lbl_80478F94;  /* obj data base (SDA) */

typedef struct FieldStatusEntry {
    u16 id;
    u16 sub_id;
    u8 byte4;
    u8 byte5;
    u8 count;
    u8 pad7;
    u32 value;
    u16 half_c;
    u16 pad_e;
} FieldStatusEntry;

#include "game/world/field_particle.h"
#ifdef TARGET_PC
#include "game/gs_part.h" /* GSpart, read by field in gsParticlePtclHook on the PC */
#include "game/gs_render_util.h" /* GSRenderCamera, read by field in gsParticleDraw on the PC */
#endif

/* Field subsystems -- forward declarations (defined below) */
u32  _unloadFlare__FPvUlUl(void);
u32 floorDataBiosGetFileGroupID(u8* entry);
void tvRelease(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonResetBasisStatus(void* ptr);
#endif
/* ===== String constants (rodata) ===== */
extern const char lbl_80272770[]; /* "floorUpdateFieldCamera: error updating..." */
extern const char lbl_802724E8[]; /* "floorReadMapPreFunc: can't alloc..." */
extern const char lbl_80272520[]; /* "floorReadScriptPreFunc(): can't alloc..." */
extern const char lbl_8027255C[]; /* "floorReadFontPreFunc(): can't alloc..." */
extern const char lbl_80272594[]; /* "floorReadMsgPreFunc(): can't alloc..." */
extern const char lbl_802725CC[]; /* "floorReadNormalPreFunc(): can't alloc..." */
/* ===== Combined cross-segment declarations (file-scope forward
 * declarations duplicated across all gs_field_world.c split segments;
 * de-duplicated by identifier from the whole original TU -- includes
 * both the original file's own top-level externs/prototypes AND a
 * declaration synthesized from every function's own definition
 * signature, since in the original single-TU file a definition also
 * served as a forward declaration for any later same-file caller; now
 * that callers may live in a sibling segment file they need an
 * explicit declaration. Function-local (per-call-site) extern
 * declarations are NOT hoisted here -- they travel with their own
 * function body. ===== */
extern u8 lbl_8047AD71;
extern u32 lbl_8047ADC0;
extern u32 now_savedata_ptr;
extern u8 lbl_804083D0[0x30];
extern u32 lbl_8047AD68;
extern u32 lbl_8047AD6C;
extern void flareRelease(void);
void gsParticleKillAll(FieldParticleBank* bank);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonWazaGetMaxPP(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosGetPokemonWazaPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonResetBasisStatus(void* ptr);
#endif
void pokemonSetLevelBasisStatus(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroItemGetItemKindToItemAryPtr(void);
#endif
extern void heroSetStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroGetStatus(void);
#endif
extern void* GSresAllocResourceAlign(); /* K&R: called with 5 args, returns void* */
extern u8 fn_800FF548(void);
extern u32 _unloadScript__FPvUlUl(); /* K&R: used as function pointer */
extern u32 _unloadFont__FPvUlUl(); /* K&R: used as function pointer */
extern u32 _unloadMsg__FPvUlUl(); /* K&R: used as function pointer */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GStextureFree(void);
#endif
extern const char lbl_80272608[];
extern const char lbl_8027262C[];
extern u8 lbl_8035BB30[];
extern u8 lbl_8035BB50[];
extern u8 lbl_8035BB10[];
extern u8 lbl_8035BAF4[];
extern const char lbl_80272658[];
extern u8 lbl_8035BAD8[];
extern const char lbl_80272680[];
extern u8 lbl_8035BABC[];
extern u32 lbl_80478FB8;
extern u32 lbl_80478FBC;
extern u8 lbl_802726D4[];
extern u8 lbl_8035B8A0[];
extern void* floorDataBiosGetFieldCameraListPtr();
extern u32 floorDataBiosGetGroupID();
extern void* floorDataBiosGetPtr(u32 key);
extern void fn_8011791C(void);
void gsParticleKillOldest(FieldParticleInstanceList* list);
extern void gsParticlePtclHook();
extern u8 joutaiDataBiosGetKaisuuDataId(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiDataBiosGetMaxCount(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 joutaiDataBiosGetSameJoutai(u16 idx);
#endif
extern u8 joutaiDataBiosGetWriteMode(u16 idx);
extern u8 joutaiDataBiosGetUseBanme(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 joutaiDataBiosGetStatus(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiDataBiosGetObjectKind(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 joutaiDataBiosGetName(u16 idx);
#endif
extern void wazaDataBiosSetFightWazaWzxVariationFuncPtr(u8* ptr, u32 val);
extern void wazaDataBiosSetFightWazaWzxTypeFuncPtr(u8* ptr, u32 val);
extern u32 wazaDataBiosGetFightWazaWzxVariationFuncPtr(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaDataBiosGetTypeId(u8* ptr, u16 idx);
#endif
extern u32 wazaDataBiosGetFightWazaWzxTypeFuncPtr(u8* ptr);
extern void wazaDataBiosSetFightTrainerAiWazaDamageFuncPtr(u8* ptr, u32 val);
extern void wazaDataBiosSetFightTrainerAiWazaHitFuncPtr(u8* ptr, u32 val);
extern void wazaDataBiosSetFightTrainerAiWazaValueFuncPtr(u8* ptr, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaDataBiosSetTypeId(u8* ptr, u16 idx, u8 val);
#endif
extern u32 wazaDataBiosGetFightTrainerAiWazaDamageFuncPtr(u8* ptr);
extern u32 wazaDataBiosGetFightTrainerAiWazaHitFuncPtr(u8* ptr);
extern u32 wazaDataBiosGetFightTrainerAiWazaValueFuncPtr(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonGetDarkPokemonLevel(void);
#endif
extern u32 pokemonDataCheckValid(u32 a, u16 key);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonIsJoutaiDataId(u8* ptr, u32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetWazaStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonWazaCheckValid(u8* ptr, u32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInit(u8* ptr);
#endif
extern void pokemonEvolutionCreateAddPokemon(void);
extern void pokemonEvolution(void);
extern void savedataInit(void);
extern void heroAddPokedoru(u8* ptr, u32 offset);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroItemAddItemDataId(u8* ptr, u32 arg2, u32 arg3, u32 arg4);
#endif
extern u32 heroAddPokemon(u8* ptr, void* arg2);
extern void heroCreate(u8* ptr, u32 arg2, u8 arg3);
extern void heroInit();
extern void heroBiosSetPokecouponAll(); /* K&R: typed impl or conflict */
extern void heroBiosSetPokecoupon(); /* K&R: typed impl or conflict */
extern void heroBiosSetPokedoru(); /* K&R: typed impl or conflict */
extern void heroBiosSetHizukiNamePtr(); /* K&R: typed impl or conflict */
extern void heroBiosSetNamePtr(); /* K&R: typed impl or conflict */
extern void heroMoveTermEvent(void);
extern void heroMoveInitEvent(void);
extern void partnerApplyMove();
extern void heroPokemonGetEifie(u32 arg1);
extern void heroPokemonGetBlacky(u32 arg1);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* wazaDataBiosGetPtr(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus();
#endif
extern u16 pokemonDataBiosGetSinkaPokemonDataId(u8* ptr, u16 idx);
extern u16 pokemonDataBiosGetSinkaBuff(u8* ptr, u16 idx);
extern u8 pokemonDataBiosGetSinkaKind(u8* ptr, u16 idx);
extern u8 floorUpdateFieldCamera();
extern void updateAnimation__Ff15HEROMOVE_MEMBER(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetPokemonPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetHizukiNamePtr(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetHizukiItemPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetItemKoronPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetItemSeedPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetItemSkillPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetItemBallPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetExtraItemPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetItemNormalPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroBiosGetNamePtr(void* ptr);
#endif
extern u32 floor_tresure_list;
extern u32 floor_tresure_list_number;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std093_getCurrentGroupID(void);
#endif
extern void peopleSetVisible(void);
extern void soundPlayFanfare(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(void);
#endif
extern void pcboxDelItem(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std011_dialogDisplayYesNoMenu(void);
#endif
void floorEventGetTresure(void);
extern void std051_characterSetAnimation(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleSetFlagsByIndex(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std081_flagCheckTrue(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStartSE(void);
#endif
extern void peopleWaitSyncMotion(void);
extern void std078_flagSetFalse(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void floorEventCtrlTresure(void);
#endif
extern void floorGetResource(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimIndex(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimFrame(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimRate(void);
#endif
extern void GSmodelSetAnimType(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelStartAnimation(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
extern void GSmodelIsAnimating(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern GSpart* GSmodelGetPart(struct GSmodel* model, s32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSpartGetTransform(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSpartFree(GSpart* part);
#endif
extern void peopleWalkTo(void);
extern void peopleMoveCheck(u32 groupId, u32 index, u8 waitFlag);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleStartTurn(void);
#endif
extern void std168_characterDetachFromCharacter(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleSetPositionByIndex(void);
#endif
extern void soundStopSE(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelCanAnimate(void);
#endif
extern void std070_characterAttachToCharacter(void);
extern void std059_characterSetPosition(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetFrameCount(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D37CC(void);
#endif
extern f32 lbl_8047CFA0;
extern u32 lbl_80478EC8;
extern u32 lbl_80478ECC;
extern f32 lbl_8047CFA4;
extern f32 lbl_8047CFA8;
extern f32 lbl_8047CFAC;
extern f64 lbl_8047CFB8;
extern f32 lbl_8047CFB4;
extern f32 lbl_8047CFB0;
void floorEventCtrlElevator(void);
extern void scriptSetCol(void);
extern void std079_flagSetTrue(void);
extern void fn_8018C558(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8018C8F4(void);
#endif
void floorEventCtrlDoor(void);
extern void EvlogSet__FScUl();
extern void scriptSetEventColID();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void cmsRunSync();
#endif
extern u8 lbl_80272708[];
extern u32 lbl_80478EC4;
extern u32 lbl_80478EC0;
extern void* peopleInfoBiosGetPtrFromIndex(u16);
extern f32 lbl_8047CFD0;
extern u8 lbl_8047AD70;
extern f32 lbl_8047AD74;
extern f32 lbl_8047AD78;
extern f32 lbl_8047AD7C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 GSscene_GetMode(void);
#endif
extern void cameraSetHeight(f32);
extern void cameraSetDistance(f32);
extern void cameraSetRotY(f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSscene_GetCameraPositionVector(void*);
#endif
extern f32 lbl_8047CFD8;
extern f32 lbl_8047CFD4;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetPosition(void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSscene_GetCameraViewVector(void*);
#endif
extern f32 cameraGetHeight(void);
extern f32 cameraGetDistance(void);
extern f32 cameraGetRotY(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void set__5GSvecFfff(f32* vec, f32 x, f32 y, f32 z);
#endif
extern void GSmtxMakeYRotation(void*, f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecTransform(void*, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecAdd(void*, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 atan2(f32, f32);
#endif
extern void cameraMoveTargetPos(u32, void*, f32);
extern void cameraMovePosition(u32, void*, f32);
extern void cameraMoveRotation(u32, void*, f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgappTerminate(void);
#endif
extern void GSgappCreate(void);
void fn_801176C8(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GScameraGetActiveCamera();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy();
#endif
extern u8 lbl_802727B8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80177A38(void); /* referenced by sibling field functions */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelResetTextureChange(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelFree(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GStextureCreate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorOpenModel(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetVisibility(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelLinkTexAnimToAnim(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetTextureChange(void);
#endif
extern u32 lbl_80478B40;
extern u32 lbl_8047AD88;
extern u32 lbl_8047AD8C;
extern u32 lbl_8047AD90;
extern u32 lbl_8047AD94;
extern u32 lbl_8047AD80;
extern u32 lbl_8047AD84;
extern void fn_800EC134(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D4604();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D377C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgfxRenderLayer();
#endif
extern void fn_800D9B24();
extern void fn_800D9AF0();
extern void fn_800D258C();
extern void gfxSetScissor();
extern void gfxSetViewport();
extern void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelDrawModel();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgfxRenderEnd(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FF4D4(void);
#endif
extern u32 lbl_802727C8[];
extern void psSetBillboardCamera();
extern void psDispMain();
extern void GSmodelSet60fpsAnimFlag();
extern void psKillGenerator();
extern FieldParticleBank* lbl_8047AD9C;
extern u32 lbl_8047ADA0;
void gsParticleUpdateAll(void);
extern void GSquatFromEuler(void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecTransformQuat(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E0108(void*, void*, void*);
#endif
extern void psInterpretParticles(u32);
extern void psExecGenerator(u32);
extern s32 GSgetVersionMajor(void);
extern void jumptable_8035BB88();
extern u8 lbl_8047ADB0;
void gsParticleUpdate(u32 particleCount);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void gsParticleKill(u8* list, u32 notify);
#endif
extern void psSetGeneratorAngleRadiusScale(); /* (gen, scale, motion): reads r3-r5 */
extern const f32 lbl_8047CFE8; /* 0.0f */
extern const f32 lbl_8047CFEC; /* 1.0f */
void* psCreateGeneratorID(u32 use_alt, u8 texture_type, u32 selector);
void* fn_800D3094(void);
extern u32 lbl_8047ADAC;
extern u32 lbl_8047ADA8;
u8* gsParticleCreate(u8* texture, u32 selector, u32 subid);
#ifdef TARGET_PC /* particle.c's parameters: the last one is the form bank, a pointer (0 here) */
extern void psInitDataBank(s32 bank, s32* cmdBank, s32* texBank, u32* ref, s32* formBank);
#else
extern void psInitDataBank(u8 texture_type, u8* description, u8* data, u8* texture, u32 flags);
#endif
extern void DCFlushRange();
extern u8 lbl_802727D8[];
void* gsParticleBankLoad(void* resource);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8016A01C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void psInitGenerator(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8016AAF4(void);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void HSD_JObjSetSPtclCallback(void);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void HSD_JObjSetPtclCallback(void);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void HSD_JObjSetDPtclCallback(void);
#endif
extern u16 lbl_8047AD98;
extern u16 lbl_8047ADA4;
extern struct GSmodel* GSmodelSearchModelList(void* jobj);
extern void* GSmodelGetLinkedGSparticleBank(struct GSmodel* model);
extern u32 GSmodelIsRootNullAdded(struct GSmodel* model);
extern u32 GSpartGetJObjIndex(void* jobj, void* root);
extern u32 fn_800E3CBC(struct GSmodel* model);
extern u32 GSmodelGetGSparticleLinkAttachMode(struct GSmodel* model);
extern u32 GSmodelGetVisibility(struct GSmodel* model);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80135E44(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void joutaiCopy(u8*, u8*, u16);
#endif
extern s32 kaisuuGetKaisuu(u32);
extern void jumptable_8035C260();
extern void fightWazaBiosSetWazaBanme(void);
extern void fightWazaBiosSetMotoWazaDataId(void);
extern void fightWazaBiosSetUseWazaDataId(void);
extern void fightWazaBiosSetTargetDataId(void);
extern void fightWazaBiosSetCritical(void);
extern void fightWazaBiosSetDamageValue(void);
extern void fightWazaBiosSetDamage(void);
extern void fightWazaBiosSetHitDamage(void);
extern void fightWazaBiosSetIryoku(void);
extern void fightWazaBiosSetZokusei(void);
extern void fightWazaBiosSetKaisuu(void);
extern void fightWazaBiosSetAutoMakeFlag(void);
extern void jumptable_8035C290();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void wazaSetStatus(void);
#endif
extern void fightWazaBiosGetWazaBanme(void);
extern void fightWazaBiosGetMotoWazaDataId(void);
extern void fightWazaBiosGetUseWazaDataId(void);
extern void fightWazaBiosGetTargetDataId(void);
extern void fightWazaBiosGetJoutaiPtr(void);
extern void fightWazaBiosGetCritical(void);
extern void fightWazaBiosGetDamageValue(void);
extern void fightWazaBiosGetDamage(void);
extern void fightWazaBiosGetHitDamage(void);
extern void fightWazaBiosGetIryoku(void);
extern void fightWazaBiosGetZokusei(void);
extern void fightWazaBiosGetKaisuu(void);
extern void fightWazaBiosGetAutoMakeFlag(void);
extern void jumptable_8035C35C();
extern u32 waza_data_number;
extern u32 waza_data;
extern u32 pokemon_nakigoe_data_number;
extern u32 pokemon_nakigoe_data[];
extern u32 pokemon_dp_filter_data_number;
extern u32 pokemon_dp_filter_data;
extern u32 pokemon_friend_filter_data_number;
extern u8 pokemon_friend_filter_data[];
extern u32 pokemon_tokusei_data_number;
extern u8 pokemon_tokusei_data[];
extern u32 pokemon_seikaku_rate_data_number;
extern u32 pokemon_seikaku_rate_data;
extern u32 pokemon_seikaku_data_number;
extern u32 pokemon_seikaku_data;
extern u32 pokemon_grow_data_number;
extern u8 pokemon_grow_data[];
extern void _flagSet(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* std082_flagGetValue(u32);
#endif
extern u32 lbl_80478F90;
extern u32 lbl_80478F94;
extern u8 lbl_80478B58[4];
extern u8 lbl_80478B5C[4];
extern f32 lbl_8047CFF0;
extern f64 lbl_8047D008;
extern f32 lbl_8047CFF4;
extern f64 lbl_8047D010;
extern f32 lbl_8047CFF8;
extern f32 lbl_8047CFFC;
extern f32 lbl_8047D000;
extern f32 lbl_8047D004;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetKind(void);
#endif
extern void itemDataBiosGetBuff(void);
extern f32 lbl_8047D018;
void pokemonAddDpFormPokemonDpFilterId(void);
extern void* darkPokemonSaveGet4A6();
void pokemonSetDarkPokemonStatus(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharCpy(void);
#endif
extern void pokemonGetFaceSpriteId(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8001D994(void);
#endif
void pokemonToMenuPokemonStatus(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgGetGSchar(void);
#endif
extern u32 zokuseiDataBiosGetName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8010C46C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSrand(void);
#endif
extern u8 lbl_8027296C[];
void pokemonCheckSetMonohiroi(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonAllKaihuku(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaSequencePokemonCreate(u32 idx, u32 base, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerSetState(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerClearState(void);
#endif
extern u32 lbl_80478F90; /* obj header ptr (SDA) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80135530(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonGetFriendFormPokemonFriendFilterId(void);
#endif
extern u8 lbl_80272948[];
void pokemonGetEffortFromPokemon(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void dbgMenuFightGetSoubikoukaFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80142CF4(void);
#endif
extern u32 sexGetPokemonSexRaitoKotei(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801EE958(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801EEB34(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void memoDataSet(void);
#endif
void pokemonCheckFightOut(void);
extern void gamedataAttestBiosCopy(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonCreate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonCreateRndFit(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeSet(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeCheck(void);
#endif
extern void evolutionOpen(void);
extern f32 lbl_8047D020;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonEvolutionAll(void);
#endif
extern u8 lbl_802729A4[];
extern u8 lbl_80272998[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800F9EE4(void);
#endif
extern void jumptable_80363468();
void getEvoPokemonLevelUp(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetItemSoubiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonEvolutionCheck(void);
#endif
extern u8 pokecolo_savedata[];
extern void fn_8013528C(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharMakeFromSJIS(void);
#endif
extern void gamedatasaveSetStatus(void);
extern u8 lbl_8047D028[8];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void savedataCreate(void);
#endif
extern void gamedataInit(void);
extern void pcboxInit(void);
extern void fn_801908D4(void);
extern void mailInitMailbox(void);
extern void sodateyaInit(void);
extern void savedataBattleInit(void);
extern void memoInit(void);
extern void fn_80083CBC(void);
extern void fn_801EF128(void);
extern void exribbonInit(void);
extern void jumptable_803634A8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80140A9C(void);
#endif
extern void fn_80140ACC(void);
extern void fn_80141308(void);
extern void fn_80142368(void);
extern void fn_80140588(void);
extern void pcboxAddPokemon(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80142A88(void);
#endif
extern void jumptable_803634F0();
extern void jumptable_80363558();
extern u8 HeroMove[];
extern void fadeEffectDokuStart(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleClearFlagsByIndex(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std151_characterFaceDirection(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenField(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgCloseField(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801D0AFC(void);
#endif
extern void floorReturnToSavedRoom(void);
extern f32 lbl_8047D030;
extern f32 lbl_8047D034;
extern f32 lbl_8047D038;
void cbPoison__Fl15FootStepCounterl(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleIsInGroup(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleSearchID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleInfoBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8018F5E4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void colsysWallSweep(void);
#endif
extern void PSVECScale(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void PSVECAdd(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScolsys2HumanCollision(void);
#endif
extern f32 lbl_8047D03C;
extern f32 lbl_8047D040;
extern f64 lbl_8047D048;
extern f64 lbl_8047D050;
extern f64 lbl_8047D058;
extern u8 lbl_80478AC0[4];
extern f32 lbl_8047D060;
void fn_8012B19C(void);
void heroMoveChkHinderClear(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgetFrameStep(void);
#endif
extern f64 lbl_8047D068;
void getStep__FP8FOOTSTEPP8_GSmodelPiP8FOOTWORK(void);
extern void peopleFindTalkTarget(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleGetPositionPtr(void);
#endif
extern void fn_8018FC50(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800CE148(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800CDBE0(void);
#endif
extern void GScolsys2CheckGetEventID(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleStopTurn(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSinputResetLeftSmoothing(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSinputResetRightSmoothing(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleGetHeadGroundPos(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleTurnToPoint(void);
#endif
extern void peopleIsTreasure(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80183730(void);
#endif
extern void peopleGetTalkScript(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleTalkPause(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std055_characterEndSpeech(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80183688(void);
#endif
extern f32 lbl_8047D070;
extern f32 lbl_8047D074;
extern f32 lbl_8047D078;
extern f32 lbl_8047D07C;
extern f32 lbl_8047D080;
void updateChat__F15HEROMOVE_MEMBER(void);
void heroMoveCheckEvent(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8018F4C8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetAnimIndex(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetAnimFrame(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimBlend(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetBlendFactor(void);
#endif
extern f32 lbl_8047D084;
extern f32 lbl_8047D088;
extern f32 lbl_8047D08C;
extern f32 lbl_8047D090;
extern f32 lbl_8047D0A8;
void circleLineIntersect(void);
extern f32 lbl_8047D0AC;
void partnerGetTarget(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E3C64(void);
#endif
extern f32 lbl_8047D0B0;
void partnerMove(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSinputGetLeftStickXData(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSinputGetLeftStickYData(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800F7BC4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleSetSpeedFromAmount(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundUpdateListener(void);
#endif
extern f64 lbl_8047D0C8;
extern f32 lbl_8047D0B4;
extern f32 lbl_8047D0B8;
extern f32 lbl_8047D0BC;
extern f32 lbl_8047D0C0;
extern f32 lbl_8047D0C4;
void heroReadStick(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSinputButtonsChanged(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleAmountFromMove(void);
#endif
extern void PSVECDistance(void);
extern f32 lbl_8047D0D0;
extern f32 lbl_8047D0D4;
void moveLeader__F15HEROMOVE_MEMBER(void);
extern void dbgMenuIsOpen(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuIsCheck(void);
#endif
extern void peopleTestFlagsByID(void);
extern void heroMoveRunEvent(void);
extern u8 lbl_80272A38[];
extern f32 lbl_8047D064;
void heroMoveMain(void);
extern void std067_characterStopLookAt(u32, u32);
extern void std065_characterLookAtPlayer(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_8012F1FC(void);
#endif
void fn_8012F40C(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetRotation(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScolsys2WalkGetHeight(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetRotation(void);
#endif
extern f32 lbl_8047D0D8;
void initFloor__Fv(void);
extern void floorGetNextFloorID(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataBattleGetSavedHeroKind(void);
#endif
extern u8 lbl_802729C0[];
extern u8 lbl_80272A10[];
void heroMoveGetKenObjID(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleOpen(void);
#endif
extern void GSmodelEnableAnimBlend(void);
extern void peopleRegisterHumanCollision(void);
extern void fn_80189328(void);
extern void peopleSetRotation(void);
void heroMoveInit(void);
void heroMoveSyncWithHero(void);
void fn_8013024C(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataGetStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataAttestCreate(void);
#endif
extern void cms_RegisterScript();
extern void floorStartSoundScript(void);
extern void GSmsgFontOpen();
extern void GSmsgOpen();
extern u8 fn_800FF554(void);
extern void cms_UnregisterScript();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FC2A8();
#endif
extern void GSmsgClose();
extern u8 lbl_8035BA98[];
extern u8 lbl_8035BA7C[];
extern u8 lbl_8035BA60[];
extern u8 lbl_8035BA48[];
extern u8 lbl_8035BA2C[];
extern u8 lbl_8035BA10[];
extern u8 lbl_8035B9F8[];
extern u8 lbl_8035B9DC[];
extern u8 lbl_8035B9C0[];
extern u8 lbl_8035B9A4[];
extern u8 lbl_8035B988[];
extern u8 lbl_8035B96C[];
extern u8 lbl_8035B950[];
extern u8 lbl_8035B938[];
extern u8 lbl_8035B904[];
extern u16 fightGetFloorKind();
extern u8 lbl_8035B8E8[];
extern const char lbl_802726AC[];
extern const char lbl_8035B8CC[];
extern u8 lbl_8035B8B4[];
extern u8 lbl_8035BB70[];
extern u32 lbl_8047CFC0;
extern u32 lbl_8047CFC8;
extern u32 lbl_8047CFC4;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecSquareDistance(void);
#endif
extern f32 lbl_8047CFDC;
extern f32 lbl_8047CFE0;
extern u32 lbl_80478B48; /* NPC count (SDA) */
extern u32 lbl_80478F94; /* obj data base (SDA) */
extern void* tasteDataGetPtr(void* ptr);
extern void tasteDataGetAisyou(void* ptr, u8 val);
extern void fn_80135708(void);
extern void fightPokemonBiosSetMotoPokemonPtr(void);
extern void fightPokemonBiosSetEntryId(void);
extern void fightPokemonBiosSetCatchEntryFlag(void);
extern void fightPokemonBiosSetLevelUpFlag(void);
extern void fightPokemonBiosSetDarkOutFlag(void);
extern void fightPokemonBiosSetHokakuFlag(void);
extern void fightOutPokemonBiosSetMotoFightPokemonPtr(void);
extern void fightOutPokemonBiosSetFightPokemonPtr(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightWazaCheckWriteJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightWazaWriteJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightWazaIsJoutaiDataId(void);
#endif
extern void fightWazaInitJoutaiDataId(void);
extern void fightWazaInitJoutai(void);
extern void fightOutPokemonBiosSetAbicntPhyAtk(void);
extern void fightOutPokemonBiosSetAbicntPhyDef(void);
extern void fightOutPokemonBiosSetAbicntSpeAtk(void);
extern void fightOutPokemonBiosSetAbicntSpeDef(void);
extern void fightOutPokemonBiosSetAbicntNimbleness(void);
extern void fightOutPokemonBiosSetAbicntAverage(void);
extern void fightOutPokemonBiosSetAbicntAvoid(void);
extern void fightOutPokemonBiosSetFightoutTurnCount(void);
extern void fightOutPokemonBiosSetSequencePtr(void);
extern void fightOutPokemonBiosSetSketchWazaDataId(void);
extern void fightOutPokemonBiosSetLastSelectWazaDataId(void);
extern void fightOutPokemonBiosSetLastUseWazaDataId(void);
extern void fightOutPokemonBiosSetLastReceiveWazaTargetDataId(void);
extern void fightOutPokemonBiosSetHitWazaDataId(void);
extern void fightOutPokemonBiosSetHitWazaZokuseiDataId(void);
extern void fightOutPokemonBiosSetGamanDamageValue(void);
extern void fightOutPokemonBiosSetGamanDamageTargetId(void);
extern void fightOutPokemonBiosSetOumuWazaDataId(void);
extern void fightOutPokemonBiosSetNamakeFlag(void);
extern void fightOutPokemonBiosSetUsedItemDataId(void);
extern void fightOutPokemonBiosSetStockItemDataId(void);
extern void fightOutPokemonBiosSetSuccessCnt(void);
extern void fightOutPokemonBiosSetMeetEnemyFightPokemonEntryId(void);
extern void fightOutPokemonBiosSetZokuseiDataId(void);
extern void fightOutPokemonBiosSetTokuseiDataId(void);
extern void fightOutPokemonBiosSetDamageAtkValue(void);
extern void fightOutPokemonBiosSetDamageAtkTargetId(void);
extern void fightOutPokemonBiosSetDamageSpeValue(void);
extern void fightOutPokemonBiosSetDamageSpeTargetId(void);
extern void fightOutPokemonBiosSetMahiNoAttackFlag(void);
extern void fightOutPokemonBiosSetKonranMyselfAttackFlag(void);
extern void fightOutPokemonBiosSetOutWazaKoukanaiFlag(void);
extern void fightOutPokemonBiosSetTameWazaFlag(void);
extern void fightOutPokemonBiosSetItemNigeruFlag(void);
extern void fightOutPokemonBiosSetHuuinNoAttackFlag(void);
extern void fightOutPokemonBiosSetMeroMeroNoAttackFlag(void);
extern void fightOutPokemonBiosSetKanashibariNoAttackFlag(void);
extern void fightOutPokemonBiosSetChouhatsuNoAttackFlag(void);
extern void fightOutPokemonBiosSetIchamonNoAttackFlag(void);
extern void fightOutPokemonBiosSetHirumuNoAttackFlag(void);
extern void fightOutPokemonBiosSetPassPpdecFlag(void);
extern void fightOutPokemonBiosSetFightActionFlag(void);
extern void fightOutPokemonBiosSetDoClearbodyFlag(void);
extern void fightOutPokemonBiosSetReceivesWazaHiraishinFlag(void);
extern void fightOutPokemonBiosSetVanishoffFlag(void);
extern void fightOutPokemonBiosSetDoIkakuFlag(void);
extern void fightOutPokemonBiosSetDoTraceFlag(void);
extern void fightOutPokemonBiosSetNoPressureFlag(void);
extern void fightOutPokemonBiosSetIrekaetaFlag(void);
extern void fightOutPokemonBiosSetItemKoraetaFlag(void);
extern void fightOutPokemonBiosSetKaigaraDamageValue(void);
extern void fightOutPokemonBiosSetMyselfDamageAtkValue(void);
extern void fightOutPokemonBiosSetMyselfDamageAtkTargetId(void);
extern void fightOutPokemonBiosSetMyselfDamageSpeValue(void);
extern void fightOutPokemonBiosSetMyselfDamageSpeTargetId(void);
extern void fightOutPokemonBiosSetKizetuFlag(void);
extern void fightOutPokemonBiosSetIrekaeTargetEntryId(void);
extern void jumptable_8035E028();
extern void pokemonDataBiosGetItemDataId(void);
extern void fightPokemonBiosGetMotoPokemonPtr(void);
extern void fightPokemonBiosGetPokemonBuffPtr(void);
extern void fightPokemonBiosGetFightJoutaiPtr(void);
extern void fightPokemonBiosGetEntryId(void);
extern void fightPokemonBiosGetCatchEntryFlag(void);
extern void fightPokemonBiosGetLevelUpFlag(void);
extern void fightPokemonBiosGetDarkOutFlag(void);
extern void fightPokemonBiosGetHokakuFlag(void);
extern void fightOutPokemonBiosGetMotoFightPokemonPtr(void);
extern void fightOutPokemonBiosGetFightPokemonPtr(void);
extern void fightOutPokemonBiosGetFightPokemonHensinBuffPtr(void);
extern void fightOutPokemonBiosGetFightoutJoutaiPtr(void);
extern void fightOutPokemonBiosGetFightWazaPtr(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonGetUseWazaDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonGetMotoWazaDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightWazaIsHit(void);
#endif
extern void fightOutPokemonBiosGetFightItemPtr(void);
extern void fightOutPokemonBiosGetAbicntPhyAtk(void);
extern void fightOutPokemonBiosGetAbicntPhyDef(void);
extern void fightOutPokemonBiosGetAbicntSpeAtk(void);
extern void fightOutPokemonBiosGetAbicntSpeDef(void);
extern void fightOutPokemonBiosGetAbicntNimbleness(void);
extern void fightOutPokemonBiosGetAbicntAverage(void);
extern void fightOutPokemonBiosGetAbicntAvoid(void);
extern void fightOutPokemonBiosGetFightoutTurnCount(void);
extern void fightOutPokemonBiosGetSequencePtr(void);
extern void fightOutPokemonBiosGetSketchWazaDataId(void);
extern void fightOutPokemonBiosGetLastSelectWazaDataId(void);
extern void fightOutPokemonBiosGetLastUseWazaDataId(void);
extern void fightOutPokemonBiosGetLastReceiveWazaTargetDataId(void);
extern void fightOutPokemonBiosGetHitWazaDataId(void);
extern void fightOutPokemonBiosGetHitWazaZokuseiDataId(void);
extern void fightOutPokemonBiosGetGamanDamageValue(void);
extern void fightOutPokemonBiosGetGamanDamageTargetId(void);
extern void fightOutPokemonBiosGetOumuWazaDataId(void);
extern void fightOutPokemonBiosGetKeepFightWazaPtr(void);
extern void fightOutPokemonBiosGetNamakeFlag(void);
extern void fightOutPokemonBiosGetUsedItemDataId(void);
extern void fightOutPokemonBiosGetStockItemDataId(void);
extern void fightOutPokemonBiosGetSuccessCnt(void);
extern void fightOutPokemonBiosGetMeetEnemyFightPokemonEntryId(void);
extern void fightOutPokemonBiosGetFightActionBuffPtr(void);
extern void fightOutPokemonBiosGetZokuseiDataId(void);
extern void fightOutPokemonBiosGetTokuseiDataId(void);
extern void fightOutPokemonBiosGetWazaMenuCurPtr(void);
extern void fightOutPokemonBiosGetDamageAtkValue(void);
extern void fightOutPokemonBiosGetDamageAtkTargetId(void);
extern void fightOutPokemonBiosGetDamageSpeValue(void);
extern void fightOutPokemonBiosGetDamageSpeTargetId(void);
extern void fightOutPokemonBiosGetMahiNoAttackFlag(void);
extern void fightOutPokemonBiosGetKonranMyselfAttackFlag(void);
extern void fightOutPokemonBiosGetOutWazaKoukanaiFlag(void);
extern void fightOutPokemonBiosGetTameWazaFlag(void);
extern void fightOutPokemonBiosGetItemNigeruFlag(void);
extern void fightOutPokemonBiosGetHuuinNoAttackFlag(void);
extern void fightOutPokemonBiosGetMeroMeroNoAttackFlag(void);
extern void fightOutPokemonBiosGetKanashibariNoAttackFlag(void);
extern void fightOutPokemonBiosGetChouhatsuNoAttackFlag(void);
extern void fightOutPokemonBiosGetIchamonNoAttackFlag(void);
extern void fightOutPokemonBiosGetHirumuNoAttackFlag(void);
extern void fightOutPokemonBiosGetPassPpdecFlag(void);
extern void fightOutPokemonBiosGetFightActionFlag(void);
extern void fightOutPokemonBiosGetDoClearbodyFlag(void);
extern void fightOutPokemonBiosGetReceivesWazaHiraishinFlag(void);
extern void fightOutPokemonBiosGetVanishoffFlag(void);
extern void fightOutPokemonBiosGetDoIkakuFlag(void);
extern void fightOutPokemonBiosGetDoTraceFlag(void);
extern void fightOutPokemonBiosGetNoPressureFlag(void);
extern void fightOutPokemonBiosGetIrekaetaFlag(void);
extern void fightOutPokemonBiosGetItemKoraetaFlag(void);
extern void fightOutPokemonBiosGetKaigaraDamageValue(void);
extern void fightOutPokemonBiosGetMyselfDamageAtkValue(void);
extern void fightOutPokemonBiosGetMyselfDamageAtkTargetId(void);
extern void fightOutPokemonBiosGetMyselfDamageSpeValue(void);
extern void fightOutPokemonBiosGetMyselfDamageSpeTargetId(void);
extern void fightOutPokemonBiosGetKizetuFlag(void);
extern void fightOutPokemonBiosGetIrekaeTargetEntryId(void);
extern void fightOutPokemonBiosGetFightOutPokemonEnemyPtr(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonCheckFightOut(void);
#endif
extern void jumptable_8035E4B0();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleMoveByVector(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void PSVECDotProduct(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void charModelGetHeadYawMin(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void charModelGetHeadYawMax(void);
#endif
extern f32 lbl_8047D094;
extern f32 lbl_8047D098;
extern f32 lbl_8047D09C;
extern f32 lbl_8047D0A0;
extern f32 lbl_8047D0A4;
extern u8 fn_801174EC(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiBiosSetIdBuff(u8* ptr, u16 val);
#endif
extern void joutaiBiosSetBuff(u8* ptr, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80119FB0(u8* ptr, u8 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiBiosSetNowKaisuu(u8* ptr, u8 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80119FD0(u8* ptr, u8 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80119FE0(u8* ptr, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80119FF0(u8* ptr, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 joutaiBiosGetIdBuff(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 joutaiBiosGetBuff(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiBiosGetCount(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiBiosGetNowKaisuu(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiBiosGetKaisuu(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 joutaiBiosGetUserFightTargetId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
extern void wazaDataBiosSetRiskFlag(u8* ptr, u8 val);
extern u8 wazaDataBiosGetRiskFlag(u8* ptr);
extern void wazaDataBiosSetHidenFlag(u8* ptr, u8 val);
extern u8 wazaDataBiosGetHidenFlag(u8* ptr);
extern void fn_8011C5E0(u8* ptr, u8 val);
extern void fn_8011C5F0(u8* ptr, u8 val);
extern void wazaDataBiosSetDoc(u8* ptr, u32 val);
extern void wazaDataBiosSetWazawzxdataId(u8* ptr, u32 val);
extern void wazaDataBiosSetPressure(u8* ptr, u8 val);
extern void wazaDataBiosSetBouon(u8* ptr, u8 val);
extern void wazaDataBiosSetNegoto(u8* ptr, u8 val);
extern void wazaDataBiosSetNekonote(u8* ptr, u8 val);
extern void wazaDataBiosSetMonomane(u8* ptr, u8 val);
extern void wazaDataBiosSetYubiwohuru(u8* ptr, u8 val);
extern void wazaDataBiosSetOujanosirusi(u8* ptr, u8 val);
extern void wazaDataBiosSetOumugaesi(u8* ptr, u8 val);
extern void wazaDataBiosSetYokodori(u8* ptr, u8 val);
extern void wazaDataBiosSetMajikku(u8* ptr, u8 val);
extern void wazaDataBiosSetMamoru(u8* ptr, u8 val);
extern void wazaDataBiosSetDageki(u8* ptr, u8 val);
extern void wazaDataBiosSetAddFightKoukaAvg(u8* ptr, u8 val);
extern void wazaDataBiosSetFightAttackMsgId(u8* ptr, u32 val);
extern void wazaDataBiosSetSeqId(u8* ptr, u16 val);
extern void wazaDataBiosSetFightKoukaDataId(u8* ptr, u16 val);
extern void wazaDataBiosSetIryoku(u8* ptr, u16 val);
extern void wazaDataBiosSetAvg(u8* ptr, u8 val);
extern void wazaDataBiosSetRangeId(u8* ptr, u8 val);
extern void wazaDataBiosSetPri(u8* ptr, u8 val);
extern void wazaDataBiosSetZokuseiDataId(u8* ptr, u8 val);
extern void wazaDataBiosSetPp(u8* ptr, u8 val);
extern void wazaDataBiosSetName(u8* ptr, u32 val);
extern u8 fn_8011C790(u8* ptr);
extern u8 fn_8011C7A8(u8* ptr);
extern u32 wazaDataBiosGetDoc(u8* ptr);
extern u32 wazaDataBiosGetWazawzxdataId(u8* ptr);
extern u8 wazaDataBiosGetPressure(u8* ptr);
extern u8 wazaDataBiosGetBouon(u8* ptr);
extern u8 wazaDataBiosGetNegoto(u8* ptr);
extern u8 wazaDataBiosGetNekonote(u8* ptr);
extern u8 wazaDataBiosGetMonomane(u8* ptr);
extern u8 wazaDataBiosGetYubiwohuru(u8* ptr);
extern u8 wazaDataBiosGetOujanosirusi(u8* ptr);
extern u8 wazaDataBiosGetOumugaesi(u8* ptr);
extern u8 wazaDataBiosGetYokodori(u8* ptr);
extern u8 wazaDataBiosGetMajikku(u8* ptr);
extern u8 wazaDataBiosGetMamoru(u8* ptr);
extern u8 wazaDataBiosGetDageki(u8* ptr);
extern u8 wazaDataBiosGetAddFightKoukaAvg(u8* ptr);
extern u32 wazaDataBiosGetFightAttackMsgId(u8* ptr);
extern u32 wazaDataBiosGetFightAttackTunagiMsgId(u8* ptr);
extern u16 wazaDataBiosGetSeqId(u8* ptr);
extern u16 wazaDataBiosGetFightKoukaDataId(u8* ptr);
extern s16 wazaDataBiosGetIryoku(u8* ptr);
extern u8 wazaDataBiosGetAvg(u8* ptr);
extern u8 wazaDataBiosGetRangeId(u8* ptr);
extern s32 wazaDataBiosGetPri(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaDataBiosGetZokuseiDataId(u8* ptr);
#endif
extern u8 wazaDataBiosGetPp(u8* ptr);
extern u32 wazaDataBiosGetName(u8* ptr);
extern s32 pokemonDpFilterDataBiosGetValue(u8* ptr);
extern u32 pokemonTokuseiDataBiosGetDoc(u8* ptr);
extern u32 pokemonTokuseiDataBiosGetName(u8* ptr);
extern u8 pokemonSeikakuRateDataBiosGetWaru(u8* ptr);
extern u8 pokemonSeikakuRateDataBiosGetKake(u8* ptr);
extern u8 fn_8011CC54(u8* ptr);
extern u8 fn_8011CC6C(u8* ptr);
extern u8 fn_8011CC84(u8* ptr);
extern u8 fn_8011CC9C(u8* ptr);
extern u8 fn_8011CCB4(u8* ptr);
extern u8 fn_8011CCCC(u8* ptr);
extern u8 fn_8011CCE4(u8* ptr);
extern s32 pokemonSeikakuDataBiosGetReliveNadenade(u8* ptr);
extern s32 pokemonSeikakuDataBiosGetReliveSodateya(u8* ptr);
extern s32 pokemonSeikakuDataBiosGetReliveCall(u8* ptr);
extern s32 pokemonSeikakuDataBiosGetReliveWalk(u8* ptr);
extern s32 pokemonSeikakuDataBiosGetReliveFightout(u8* ptr);
extern u8 pokemonSeikakuDataBiosGetNimblenessRateDataId(u8* ptr);
extern u8 pokemonSeikakuDataBiosGetSpeDefRateDataId(u8* ptr);
extern u8 pokemonSeikakuDataBiosGetSpeAtkRateDataId(u8* ptr);
extern u8 pokemonSeikakuDataBiosGetPhyDefRateDataId(u8* ptr);
extern u8 pokemonSeikakuDataBiosGetPhyAtkRateDataId(u8* ptr);
extern u32 pokemonSeikakuDataBiosGetName(u8* ptr);
extern void fn_8011CEA0(u8* ptr, u8 val);
extern void fn_8011CEB0(u8* ptr, u8 val);
extern void fn_8011CEC0(u8* ptr, u16 val);
extern void pokemonDataBiosSetPkxDataId(u8* ptr, u32 val);
extern void pokemonDataBiosSetNumPokemon(u8* ptr, u16 val);
extern void pokemonDataBiosSetNumZukan(u8* ptr, u16 val);
extern void pokemonDataBiosSetVoice(u8* ptr, u16 val);
extern void pokemonDataBiosSetDoc(u8* ptr, u32 val);
extern void pokemonDataBiosSetWeight(u8* ptr, u16 val);
extern void pokemonDataBiosSetHeight(u8* ptr, u16 val);
extern void pokemonDataBiosSetTypeName(u8* ptr, u32 val);
extern void pokemonDataBiosSetInitFriend(u8* ptr, u16 val);
extern void pokemonDataBiosSetSexRatio(u8* ptr, u8 val);
extern void pokemonDataBiosSetGet(u8* ptr, u8 val);
extern void pokemonDataBiosSetGrowDataId(u8* ptr, u8 val);
extern void pokemonDataBiosSetGiveExp(u8* ptr, u16 val);
extern void pokemonDataBiosSetName(u8* ptr, u32 val);
extern void pokemonBiosSetFightTrainerPokemonDataId(u8* ptr, u16 val);
extern void pokemonBiosSetPara1Amari(u8* ptr, u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosSetAmari(u8* ptr, u16 val);
#endif
extern void pokemonBiosSetMailId(u8* ptr, u8 val);
extern void pokemonBiosSetPcboxMark(u8* ptr, u8 val);
extern void pokemonBiosSetFlagAmari(u8* ptr, u8 val);
extern void pokemonBiosSetFuseiFlag(u8* ptr, u8 val);
extern void pokemonBiosSetTamagoFlag(u8* ptr, u8 val);
extern void pokemonBiosSetPokerus(u8* ptr, u8 val);
extern void pokemonBiosSetFur(u8* ptr, u8 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosSetDarkpokemonDataId(u8* ptr, u16 val);
#endif
extern void pokemonBiosSetItemDataId(u8* ptr, u16 val);
extern void pokemonBiosSetConditionAmari(u8* ptr, u32 val);
extern void pokemonBiosSetPoolExp(u8* ptr, u32 val);
extern void pokemonBiosSetExp(u8* ptr, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosSetCatchTrainerRnd(u8* ptr, u32 val);
#endif
extern void pokemonBiosSetCatchTrainerSex(u8* ptr, u8 val);
extern void pokemonBiosSetCatchBallId(u8* ptr, u8 val);
extern void pokemonBiosSetCatchLevel(u8* ptr, u8 val);
extern void pokemonBiosSetCatchFloorId(u8* ptr, u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosSetRnd(u8* ptr, u32 val);
#endif
extern void pokemonBiosSetPokemonDataId(u8* ptr, u16 val);
extern u8 fn_8011E000(u8* ptr);
extern u8 fn_8011E018(u8* ptr);
extern u16 fn_8011E030(u8* ptr);
extern u32 pokemonDataBiosGetPkxDataId(u8* ptr);
extern u16 pokemonDataBiosGetNumPokemon(u8* ptr);
extern u16 pokemonDataBiosGetNumZukan(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonDataBiosGetVoice(u8* ptr);
#endif
extern u32 pokemonDataBiosGetDoc(u8* ptr);
extern u16 pokemonDataBiosGetWeight(u8* ptr);
extern u16 pokemonDataBiosGetHeight(u8* ptr);
extern u32 pokemonDataBiosGetTypeName(u8* ptr);
extern u16 pokemonDataBiosGetInitFriend(u8* ptr);
extern u8 pokemonDataBiosGetSexRatio(u8* ptr);
extern u8 pokemonDataBiosGetGet(u8* ptr);
extern u8 pokemonDataBiosGetGrowDataId(u8* ptr);
extern u16 pokemonDataBiosGetGiveExp(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonDataBiosGetName(u8* ptr);
#endif
extern u16 pokemonBiosGetFightTrainerPokemonDataId(u8* ptr);
extern u16 pokemonBiosGetPara1Amari(u8* ptr);
extern u16 pokemonBiosGetAmari(u8* ptr);
extern u8 pokemonBiosGetMailId(u8* ptr);
extern u8 pokemonBiosGetPcboxMark(u8* ptr);
extern u8 pokemonBiosGetFlagAmari(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonBiosGetFuseiFlag(u8* ptr);
#endif
extern u8 pokemonBiosGetTamagoFlag(u8* ptr);
extern u8 pokemonBiosGetPokerus(u8* ptr);
extern u8 pokemonBiosGetFur(u8* ptr);
extern u16 pokemonBiosGetPoolFriend(u8* ptr);
extern u32 pokemonBiosGetPoolExp(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonBiosGetDp(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonBiosGetDarkpokemonDataId(u8* ptr);
#endif
extern u16 pokemonBiosGetFriend(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonBiosGetHp(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonBiosGetItemDataId(u8* ptr);
#endif
extern u32 pokemonBiosGetConditionAmari(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonBiosGetLevel(u8* ptr);
#endif
extern u32 pokemonBiosGetExp(u8* ptr);
extern u32 pokemonBiosGetCatchTrainerRnd(u8* ptr);
extern u8 pokemonBiosGetCatchTrainerSex(u8* ptr);
extern u8 pokemonBiosGetCatchBallId(u8* ptr);
extern u8 pokemonBiosGetCatchLevel(u8* ptr);
extern u16 pokemonBiosGetCatchFloorId(u8* ptr);
extern u32 pokemonBiosGetRnd(u8* ptr);
extern void heroBiosSetHomePlace(u8* ptr, u8 val);
extern u8 heroBiosGetHomePlace(u8* ptr);
extern u8 heroBiosGetHizukiFlag(u8* ptr);
extern void heroBiosSetHizukiFlag(u8* ptr, u8 val);
extern u32 heroBiosGetPokecouponAll(u8* ptr);
extern u32 heroBiosGetPokecoupon(u8* ptr);
extern u32 heroBiosGetPokedoru(u8* ptr);
extern u8 heroBiosGetBadge08Flag(u8* ptr);
extern u8 heroBiosGetBadge07Flag(u8* ptr);
extern u8 heroBiosGetBadge06Flag(u8* ptr);
extern u8 heroBiosGetBadge05Flag(u8* ptr);
extern u8 heroBiosGetBadge04Flag(u8* ptr);
extern u8 heroBiosGetBadge03Flag(u8* ptr);
extern u8 heroBiosGetBadge02Flag(u8* ptr);
extern u8 heroBiosGetBadge01Flag(u8* ptr);
extern void heroBiosSetBadge08Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge07Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge06Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge05Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge04Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge03Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge02Flag(u8* ptr, u8 val);
extern void heroBiosSetBadge01Flag(u8* ptr, u8 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 heroBiosGetSexDataId(u8* ptr);
#endif
extern void heroBiosSetSexDataId(u8* ptr, u8 val);
extern void heroBiosSetRnd(u8* ptr, u32 val);
extern u32 heroBiosGetRnd(u8* ptr);
extern u32 GSeffectGetUpdateGapp(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonBiosGetPokemonDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 savedataBiosGetNowSavedataPtr(void);
#endif
extern void* floorReadMapPreFunc(void* owner, u32 param, u32 alloc_size);
extern void* floorReadScriptPreFunc(void* owner, u32 param, u32 alloc_size);
extern void* floorReadFontPreFunc(void* owner, u32 param, u32 alloc_size);
extern void* floorReadMsgPreFunc(void* owner, u32 param, u32 alloc_size);
extern u32 _unloadFlare__FPvUlUl(void);
extern u32 _unloadParticles__FPvUlUl(void);
extern u32 _unloadCamera__FPvUlUl(void);
extern u32 _unloadLight__FPvUlUl(void);
extern u32 _unloadColsys__FPvUlUl(void);
extern u32 _unloadTexture__FPvUlUl(void);
extern u32 floorReadMakeFogResID(u32 val);
extern u32 floorReadMakeCameraResID(u32 val);
extern u32 floorReadMakeLightResID(u32 val);
extern u32 floorReadMakeModelResID(u32 val);
extern u32 floorDataBiosGetShadowReciveNum(void* ptr);
extern void* floorDataBiosGetShadowReciveID(void* ptr, u32 idx);
extern void* floorDataBiosGetShadowLightID(void* ptr);
extern void* floorDataBiosGetSunResID(void* ptr);
extern void floorDataBiosSetMainSctID(void* obj, u32 val);
extern void floorDataBiosSetMapResID(void* obj, u32 val);
extern u32 floorDataBiosGetFileGroupID(u8* entry);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* floorDataBiosGetCurrentPtr(void);
#endif
extern void* fn_80115CB4(u32 param);
extern void fn_80115D64(u32 r25, u32 r26);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroDispatchEvent(u32 kind, u32 arg);
#endif
extern void floorCharacterBiosSetVisibility(u8* ptr, u8 val);
extern void floorCharacterBiosSetPos(u8* dst, f32* src);
extern u32 floorCharacterBiosGetTalkSctID(void* ptr);
extern u32 floorCharacterBiosGetMoveSctID(void* ptr);
extern u32 floorCharacterBiosGetNameID(void* ptr);
extern u32 floorCharacterBiosGetTalkWallThrough(u8* ptr);
extern u32 floorCharacterBiosGetTalkEndType(u8* ptr);
extern u32 floorCharacterBiosGetTalkStartType(u8* ptr);
extern u32 floorCharacterBiosGetMoveType(u8* ptr);
extern u32 floorCharacterBiosGetLoadInit(u8* ptr);
extern u32 floorCharacterBiosGetVisibility(u8* ptr);
extern void* floorCharacterBiosGetPeopleInfoPtr(u8* ptr);
extern void floorCharacterBiosGetByIndex(u32 arg);
extern void fn_80117154(void);
extern void fieldCameraLoadList(void);
extern void fn_801171C8(void);
extern void fn_80117330(f32 arg);
extern u32 fn_801174C4(void);
extern void std136_fieldClearFlag_8047ad71(void);
extern void fn_80117500(void);
extern u32 fn_80117AD4(void);
extern u8 std171_mapSetTextureChange(u32 arg1);
extern void tvRelease(void);
extern void tvRender(void);
extern void tvInitFloor(void* arg);
extern void fn_80118020(void);
extern void tvRelease2(void);
extern void fn_80118100(void);
extern void gsParticleDraw(u32 a, u8 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gsParticleKillOwned(u8* obj, u32 notify);
#endif
extern void GSmodelSetParticleVisibility(void* bank, u32 visible);
extern void fn_80118C88(void* obj, u32 visible);
extern void fn_80118CAC(void* obj, u8 enabled);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118CD0(void* obj, u8 enabled);
#endif
extern void fn_80118CF4(void* obj, u8 enabledA, u8 enabledB);
extern void fn_80118D18(void* obj, u8 enabled);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118D3C(void* obj, u8 enabledA, u8 enabledB);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118D60(void* obj, u8 enabled);
#endif
extern u32 fn_80118D84(void* obj);
extern s32 fn_80118DA8(u8* ptr);
extern void fn_80118DE0(u8* arg1, f32* arg2, u32 arg3, u32 arg4);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118E8C(u8* arg1, f32* arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gsParticleSetVecB(u8* arg1, f32* arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118F7C(u8* obj, f32* arg);
#endif
extern void gsParticleLinkToJObj(u8* obj, GSpart* part, u32 state, u32 byte5,
                        u32 init_from_zero, u32 attach_model);
extern void gsParticleInit(u32 count1, u32 count2);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiSetBuff(u8* arg1, u16 arg2, u32 arg3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 joutaiGetBuff(void* obj, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiSetIdBuff(u8* arg1, u16 arg2, u32 arg3);
#endif
extern s32 joutaiGetIdBuff(void* obj, u16 val);
extern s32 joutaiGetUserFightTargetId(void* obj, u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiSetKaisuuJoutaiDataId(u8* arg1, u16 arg2, u32 arg3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiSetNowKaisuuJoutaiDataId(u8* arg1, u16 arg2, u32 arg3);
#endif
extern s32 joutaiGetNowKaisuuJoutaiDataId(void* obj, u16 val);
extern s32 joutaiGetKaisuuJoutaiDataId(void* obj, u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiSetCountAsJoutaiDataId(u8* arg1, u16 arg2, u32 arg3);
#endif
extern s32 joutaiGetCountAsJoutaiDataId(void* obj, u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiWriteJoutaiDataId(void* obj, u16 id, u16 arg3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 joutaiCheckWriteJoutaiDataId(void* obj, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 joutaiIsJoutaiDataId(void* obj, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiInitJoutaiDataId(u8* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiInitAry(u8* base, u16 count);
#endif
extern u32 fn_8011BA0C(u8 type);
extern u32 wazaIsWazaTypeId(u32 key, u8 target);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaGetMaxPP(u32 arg1, u8 arg2);
#endif
extern u32 pokemonNakigoeDataBiosGetDataAddress(u8* ptr);
extern void* pokemonDpFilterDataBiosGetPtr(u16 idx);
extern s8 pokemonFriendFilterDataBiosGetValue(u8* ptr, u8 idx);
extern void* pokemonFriendFilterDataBiosGetPtr(u16 idx);
extern void* pokemonTokuseiDataBiosGetPtr(u16 idx);
extern void* pokemonSeikakuRateDataBiosGetPtr(u8 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonSeikakuDataBiosGetCallRate(u8* ptr, u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonSeikakuDataBiosGetHyperRate(u8* ptr, u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonSeikakuDataBiosGetPtr(u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGrowDataBiosGetExp(u8* ptr, u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGrowDataBiosGetPtr(u8 idx);
#endif
extern void fn_8011CED0(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetKowaza(u8* ptr, u16 idx, u16 val);
extern void pokemonDataBiosSetTukamaetaFlag(u8* ptr);
extern void pokemonDataBiosSetMitaFlag(u8* ptr);
extern void pokemonDataBiosSetColor(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetStatusFaceMenuSpriteId(u8* ptr, u16 idx, u16 val);
extern void pokemonDataBiosSetPokebodyId(u8* ptr, u16 idx, u32 val);
extern void pokemonDataBiosSetWazaMcn(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetGetWazaDataId(u8* ptr, u16 idx, u16 val);
extern void pokemonDataBiosSetGetWazaLevel(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetSinkaPokemonDataId(u8* ptr, u16 idx, u16 val);
extern void pokemonDataBiosSetSinkaBuff(u8* ptr, u16 idx, u16 val);
extern void pokemonDataBiosSetSinkaKind(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetTokuseiDataId(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetZokuseiDataId(u8* ptr, u16 idx, u8 val);
extern void pokemonDataBiosSetItemDataId(u8* ptr, u16 idx, u16 val);
extern void pokemonDataBiosSetGiveNimblenessEffort(void* ptr, u16 val);
extern void pokemonDataBiosSetGiveSpeDefEffort(void* ptr, u16 val);
extern void pokemonDataBiosSetGiveSpeAtkEffort(void* ptr, u16 val);
extern void pokemonDataBiosSetGivePhyDefEffort(void* ptr, u16 val);
extern void pokemonDataBiosSetGivePhyAtkEffort(void* ptr, u16 val);
extern void pokemonDataBiosSetGiveMaxHpEffort(void* ptr, u16 val);
extern void pokemonDataBiosSetBasisNimbleness(void* ptr, u16 val);
extern void pokemonDataBiosSetBasisSpeDef(void* ptr, u16 val);
extern void pokemonDataBiosSetBasisSpeAtk(void* ptr, u16 val);
extern void pokemonDataBiosSetBasisPhyDef(void* ptr, u16 val);
extern void pokemonDataBiosSetBasisPhyAtk(void* ptr, u16 val);
extern void pokemonDataBiosSetBasisMaxHp(void* ptr, u16 val);
extern void pokemonBiosSetEventGetFlag(u8* ptr, u8 val);
extern void pokemonBiosSetTokuseiFlag(u8* ptr, u8 val);
extern void pokemonBiosSetAmariRibbon(void* ptr, u8 val);
extern void pokemonBiosSetWorldRibbon(void* ptr, u8 val);
extern void pokemonBiosSetEarthRibbon(void* ptr, u8 val);
extern void pokemonBiosSetNationalRibbon(void* ptr, u8 val);
extern void pokemonBiosSetCountryRibbon(void* ptr, u8 val);
extern void pokemonBiosSetSkyRibbon(void* ptr, u8 val);
extern void pokemonBiosSetLandRibbon(void* ptr, u8 val);
extern void pokemonBiosSetMarineRibbon(void* ptr, u8 val);
extern void pokemonBiosSetGanbaRibbon(void* ptr, u8 val);
extern void pokemonBiosSetBromideRibbon(void* ptr, u8 val);
extern void pokemonBiosSetVictoryRibbon(void* ptr, u8 val);
extern void pokemonBiosSetWinningRibbon(void* ptr, u8 val);
extern void pokemonBiosSetChampRibbon(void* ptr, u8 val);
extern void pokemonBiosSetStrongMedal(void* ptr, u8 val);
extern void pokemonBiosSetCleverMedal(void* ptr, u8 val);
extern void pokemonBiosSetCuteMedal(void* ptr, u8 val);
extern void pokemonBiosSetBeautifulMedal(void* ptr, u8 val);
extern void pokemonBiosSetStyleMedal(void* ptr, u8 val);
extern void pokemonBiosSetStrong(void* ptr, u8 val);
extern void pokemonBiosSetClever(void* ptr, u8 val);
extern void pokemonBiosSetCute(void* ptr, u8 val);
extern void pokemonBiosSetBeautiful(void* ptr, u8 val);
extern void pokemonBiosSetStyle(void* ptr, u8 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosSetDp(u8* ptr, s32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosSetFriend(u8* ptr, u16 val);
#endif
extern void pokemonBiosSetNimblenessRnd(u8* ptr, u16 val);
extern void pokemonBiosSetSpeDefRnd(u8* ptr, u16 val);
extern void pokemonBiosSetSpeAtkRnd(u8* ptr, u16 val);
extern void pokemonBiosSetPhyDefRnd(u8* ptr, u16 val);
extern void pokemonBiosSetPhyAtkRnd(u8* ptr, u16 val);
extern void pokemonBiosSetMaxHpRnd(u8* ptr, u16 val);
extern void pokemonBiosSetNimblenessEffort(u8* ptr, u16 val);
extern void pokemonBiosSetSpeDefEffort(u8* ptr, u16 val);
extern void pokemonBiosSetSpeAtkEffort(u8* ptr, u16 val);
extern void pokemonBiosSetPhyDefEffort(u8* ptr, u16 val);
extern void pokemonBiosSetPhyAtkEffort(u8* ptr, u16 val);
extern void pokemonBiosSetMaxHpEffort(u8* ptr, u16 val);
extern void pokemonBiosSetNimbleness(void* ptr, u16 val);
extern void pokemonBiosSetSpeDef(void* ptr, u16 val);
extern void pokemonBiosSetSpeAtk(void* ptr, u16 val);
extern void pokemonBiosSetPhyDef(void* ptr, u16 val);
extern void pokemonBiosSetPhyAtk(void* ptr, u16 val);
extern void pokemonBiosSetMaxHp(void* ptr, u16 val);
extern void pokemonBiosSetHp(u8* ptr, u16 val);
extern void pokemonBiosSetPokemonWazaPpCount(u8* ptr, u32 arg2, u8 arg3);
extern void pokemonBiosSetPokemonWazaPp(u32 arg1, s32 arg2, u8 maxVal);
extern void pokemonBiosSetPokemonWazaDataId(void* ctx, u32 p1, u32 value);
extern void pokemonBiosSetLevel(u8* ptr, u8 val);
extern void pokemonBiosSetPoolFriend(u8* ptr, u16 val);
extern void pokemonBiosSetNicknameOrgPtr(u8* ptr, void* src);
extern void pokemonBiosSetNicknamePtr(u8* ptr, void* src);
extern void pokemonBiosSetCatchTrainerNamePtr(u8* ptr, void* src);
extern u8 fn_8011E048(u8* ptr, u16 idx);
extern u16 pokemonDataBiosGetKowaza(u8* ptr, u16 idx);
extern void* pokemonDataBiosGetTukamaetaFlag(u8* ptr);
extern void* pokemonDataBiosGetMitaFlag(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonDataBiosGetWazaMcn(u8* ptr, u16 idx);
#endif
extern u8 pokemonDataBiosGetTokuseiDataId(u8* ptr, u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonDataBiosGetZokuseiDataId(u8* ptr, u16 idx);
#endif
extern u16 pokemonDataBiosGetGiveSpeDefEffort(void* ptr);
extern u16 pokemonDataBiosGetGiveSpeAtkEffort(void* ptr);
extern u16 pokemonDataBiosGetGivePhyDefEffort(void* ptr);
extern u16 pokemonDataBiosGetGivePhyAtkEffort(void* ptr);
extern u16 pokemonDataBiosGetGiveMaxHpEffort(void* ptr);
extern u16 pokemonDataBiosGetBasisNimbleness(void* ptr);
extern u16 pokemonDataBiosGetBasisSpeDef(void* ptr);
extern u16 pokemonDataBiosGetBasisSpeAtk(void* ptr);
extern u16 pokemonDataBiosGetBasisPhyDef(void* ptr);
extern u16 pokemonDataBiosGetBasisPhyAtk(void* ptr);
extern u16 pokemonDataBiosGetBasisMaxHp(void* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonDataBiosGetPtr(u16 idx);
#endif
extern u8 pokemonBiosGetEventGetFlag(u8* ptr);
extern u8 pokemonBiosGetAmariRibbon(void* ptr);
extern u8 pokemonBiosGetWorldRibbon(void* ptr);
extern u8 pokemonBiosGetEarthRibbon(void* ptr);
extern u8 pokemonBiosGetNationalRibbon(void* ptr);
extern u8 pokemonBiosGetCountryRibbon(void* ptr);
extern u8 pokemonBiosGetSkyRibbon(void* ptr);
extern u8 pokemonBiosGetLandRibbon(void* ptr);
extern u8 pokemonBiosGetMarineRibbon(void* ptr);
extern u8 pokemonBiosGetGanbaRibbon(void* ptr);
extern u8 pokemonBiosGetBromideRibbon(void* ptr);
extern u8 pokemonBiosGetVictoryRibbon(void* ptr);
extern u8 pokemonBiosGetWinningRibbon(void* ptr);
extern u8 pokemonBiosGetChampRibbon(void* ptr);
extern u8 pokemonBiosGetStrongMedal(void* ptr);
extern u8 pokemonBiosGetCleverMedal(void* ptr);
extern u8 pokemonBiosGetCuteMedal(void* ptr);
extern u8 pokemonBiosGetBeautifulMedal(void* ptr);
extern u8 pokemonBiosGetStyleMedal(void* ptr);
extern u8 pokemonBiosGetStrong(void* ptr);
extern u8 pokemonBiosGetClever(void* ptr);
extern u8 pokemonBiosGetCute(void* ptr);
extern u8 pokemonBiosGetBeautiful(void* ptr);
extern u8 pokemonBiosGetStyle(void* ptr);
extern void* pokemonBiosGetDarkJoutaiPtr(u8* ptr, u16 idx);
extern u16 pokemonBiosGetNimblenessRnd(void* ptr);
extern u16 pokemonBiosGetSpeDefRnd(void* ptr);
extern u16 pokemonBiosGetSpeAtkRnd(void* ptr);
extern u16 pokemonBiosGetPhyDefRnd(void* ptr);
extern u16 pokemonBiosGetPhyAtkRnd(void* ptr);
extern u16 pokemonBiosGetMaxHpRnd(void* ptr);
extern u16 pokemonBiosGetNimblenessEffort(void* ptr);
extern u16 pokemonBiosGetSpeDefEffort(void* ptr);
extern u16 pokemonBiosGetSpeAtkEffort(void* ptr);
extern u16 pokemonBiosGetPhyDefEffort(void* ptr);
extern u16 pokemonBiosGetPhyAtkEffort(void* ptr);
extern u16 pokemonBiosGetMaxHpEffort(void* ptr);
extern u16 pokemonBiosGetNimbleness(void* ptr);
extern u16 pokemonBiosGetSpeDef(void* ptr);
extern u16 pokemonBiosGetSpeAtk(void* ptr);
extern u16 pokemonBiosGetPhyDef(void* ptr);
extern u16 pokemonBiosGetPhyAtk(void* ptr);
extern u16 pokemonBiosGetMaxHp(void* ptr);
extern u8 pokemonBiosGetPokemonWazaPpCount(void* ctx, u32 p1);
extern u8 pokemonBiosGetPokemonWazaPp(void* ctx, u32 p1);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonBiosGetPokemonWazaDataId(void* ctx, u32 p1);
#endif
extern void* pokemonBiosGetJoutaiPtr(u8* ptr, u16 idx);
extern void* pokemonBiosGetNicknameOrgPtr(void* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonBiosGetNicknamePtr(void* ptr);
#endif
extern void* pokemonBiosGetCatchTrainerNamePtr(void* ptr);
extern void* pokemonBiosGetAttest(void* ptr);
extern void pokemonWazaBiosCopy(u32* dst, u32* src);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosCopy(u32* dst, u32* src);
#endif
extern u32 pokemonGetCallRate(u8* ptr);
extern u32 pokemonGetHyperModeRate(u8* ptr);
extern void pokemonSetDp(u8* ptr, f32 f1);
extern f32 pokemonGetDp(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonIsDarkPokemon(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonToMenuPokemonStatusSubBar(u8* ptr, u8* out);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonToMenuWazaStatus(u8* ptr, u8* out);
#endif
extern void pokemonGetMezamerupower(u8* ptr, u16* out1, u16* out2);
extern u32 pokemonGetNowLevelToExp(u8* ptr);
extern u32 pokemonGetJoutaiMsgId(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetJoutaiMenuSpriteId(u8* ptr);
#endif
extern u32 pokemonGetJoutaiDataId(u8* ptr);
extern void fn_80121484(void* obj, u32 arg2, u32 arg3);
extern void pokemonSetJoutai08(void* obj, u32 arg2, u32 arg3);
extern void pokemonSetJoutai0C(void* obj, u32 arg2, u32 arg3);
extern void pokemonSetJoutaiKaisuu(void* obj, u32 arg2, u32 arg3);
extern void fn_801217B4(void* obj, u32 arg2, u32 arg3);
extern void fn_8012190C(void* obj, u32 arg2, u32 arg3);
extern void pokemonWriteJoutaiDataId(void* obj, u32 arg2, u32 arg3);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInitPersistentJoutai(void* obj, u32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonReplace(u32* arg1, u32* arg2);
#endif
extern void* pokemonCreateSequence(void* arg);
extern void pokemonSetSequenceStatus(u8* ptr, void* obj);
extern u8 pokemonIsNokoriHpFollowing(u8* ptr, s32 b);
extern u32 pokemonIsJoutaiKaragenki(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonIsJoutaiNormal(u8* ptr);
#endif
extern u16 pokemonGetSoubiItemBuff(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonGetSoubiItemDataId(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonDoItemSoubi(u8* ptr, register u32 arg2, u8 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonGetSex(u8* ptr);
#endif
extern void pokemonSetOnDarkPokemonFlag(u8* ptr, u8 flag);
extern void pokemonSetOnZukanFlag(u8* ptr, u8 flag);
extern u32 pokemonGetOboeWazaDataBanme(u8* ptr, u32 arg2);
extern s32 pokemonOboeWaza(u8* ptr, u8 target, u8* buf_ptr, u8* counter_ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonGetOboeWazaDataId(u8* ptr, u8 arg2, u8* counter_ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonSearchWazaDataId(u8* ptr, u16 target);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonWazaReplace(void* ptr, u32 idx, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonWazaCreate(u8* ptr, u32 slot, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetCatchStatus(u8* arg1, u32 arg2, u8 arg3, u16 arg4, u8 arg5, u32 arg6, u32 arg7);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonCheckValid(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonGetTokuseiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInitAry(u8* ptr, u16 count);
#endif
extern void pokemonInitDarkPokemon(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInitJoutai(u8* ptr);
#endif
extern void pokemonWazaInit(u8* ptr, u32 arg2);
extern u32 pokemonCheckRare(void* ctx);
extern void pokemonGrowBasisStatus(void* ptr, u32 arg2);
extern void* savedataBiosGetExRibbonPtr(void* ptr);
extern void* savedataBiosGetDarkpokemonPtr(void* ptr);
extern void* savedataBiosGetE3E8Ptr(void* ptr);
extern void* savedataBiosGetCardEPtr(void* ptr);
extern void* savedataBiosGetMemoDataPtr(void* ptr);
extern void* savedataBiosGetSodateyaPtr(void* ptr);
extern void* savedataBiosGetMailboxPtr(void* ptr);
extern void* savedataBiosGetFlagLevel2Ptr(void* ptr);
extern void* savedataBiosGetFlagLevel1Ptr(void* ptr);
extern void* savedataBiosGetFlagLevel0Ptr(void* ptr);
extern void* savedataBiosGetPcboxPtr(void* ptr);
extern void* savedataBiosGetHeroPtr(void* ptr);
extern u32 savedataBiosGetGamedataPtr(void* ptr);
extern void savedataBiosSetNowSavedataPtr(void* ptr);
extern void* savedataBiosGetSavedataBufferPtr(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 savedataGetStatus(u8* arg1, u16 arg2);
#endif
extern void heroDecPokecoupon(u8* ptr, s32 offset);
extern void heroAddPokecoupon(u8* ptr, s32 offset);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroDecPokedoru(u8* ptr, u32 offset);
#endif
extern void fn_80129514(u8* ptr, s32 arg2, s32 arg3);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_8012959C(u8* ptr, u32 arg2, u32 arg3, u32 arg4);
#endif
extern s32 fn_80129650(u8* ptr, u32 arg2, u32 arg3, u32 arg4);
extern u32 fn_80129718(u8* ptr, u32 arg2);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroHizukiItemGetItemAryPtr(u8* ptr, u16* out_a, u16* out_b, u8* out_c, u8* out_d);
#endif
extern void heroCheckSetMonohiroiAllTemotiPokemon(u8* arg1);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroItemCheckAddItemDataId(u8* ptr, u32 arg2);
#endif
extern void fn_80129948(u8* arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5, u32 arg6, u32 arg7);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroItemDecItemDataId(u8* ptr, u32 arg2, u32 arg3, u32 arg4);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroItemCheckHaveItemDataId(u8* ptr, u32 arg2);
#endif
extern u32 fn_80129D64(u8* ptr, u8* arg2);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroGetPokemon(u8* ptr, void* buf, u8 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroCatchPokemon(u8* ptr, u8* buf, u32 arg3, u16 arg4, u8 flag);
#endif
extern void heroBiosCopy(u32* dst, u32* src);
extern void cbTsureFriend__Fl15FootStepCounterl(void);
extern void heroMoveSetLockFrame(s32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroMoveAddAutoEvent(u32 a, u32 b, u32 c, u32 d, u32 e);
#endif
extern void fn_8012BAF0(u8 type, void* src, u32 val);
extern u32 fn_8012BDE0(u32 r3, u32 r4);
extern u32 heroMoveSetNeckMode(s32 idx, s32 state);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroMoveIsMember(s32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroMoveDismissMember(s32 idx);
#endif
extern void heroPokemonGetCelebi(u8* arg1);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80130770(u8* arg1);
#endif
extern void heroPokemonGetHouou(u8* arg1);
extern void fn_801309A0(u32 arg1);
extern void* floorReadScriptPostFunc(u32 a, u32 b);
extern void* floorReadFontPostFunc(u32 a, u32 b);
extern void* floorReadMsgPostFunc(u32 a, u32 b);
extern void* floorReadNormalPreFunc(u32 a, u32 b, u32 size);
extern void* floorDataBiosGetPosListPtr(u8* ptr);
extern void* floorDataBiosGetCharInfo(u8* ptr, u32 idx);
extern u32 floorDataBiosGetCharNum(u8* ptr);
extern u32 floorDataBiosGetPostFunc(u8* ptr);
extern u32 floorDataBiosGetMainFunc(u8* ptr);
extern u32 floorDataBiosGetPreFunc(u8* ptr);
extern u32 floorDataBiosGetPlaceName(u8* ptr);
extern u32 floorDataBiosGetPostSctID(u8* ptr);
extern u32 floorDataBiosGetMainSctID(u8* ptr);
extern u32 floorDataBiosGetPreSctID(u8* ptr);
extern u32 floorDataBiosGetModifySctID(u8* ptr);
extern u32 floorDataBiosGetSoundSctID(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 floorDataBiosGetFloorID(u8* ptr);
#endif
extern u8 floorDataBiosGetFloorKind(u8* ptr);
extern u32 floorDataBiosGetMapResID(u8* ptr);
extern u8 floorDataBiosGetArea(u8* ptr);
extern s32 floorEventChangeTresure(u32 index, u16 val, u8 byte);
extern s32 floorCharacterBiosGetRot(void* a, void* b);
extern u32 floorCharacterBiosGetPos(u8* ptr, void* obj);
extern u32 floorCharacterGetInfo(u8* arg1, u32 arg2);
extern u8 pokemonDataBiosGetColor(u8* ptr, s32 idx);
extern u16 pokemonDataBiosGetStatusFaceMenuSpriteId(u8* ptr, s32 idx);
extern u32 pokemonDataBiosGetPokebodyId(u8* ptr, s32 idx);
extern u16 pokemonDataBiosGetGetWazaDataId(u8* ptr, s32 idx);
extern u8 pokemonDataBiosGetGetWazaLevel(u8* ptr, s32 idx);
extern u16 pokemonDataBiosGetGiveNimblenessEffort(u8* ptr);
extern u8 pokemonBiosGetTokuseiFlag(u8* ptr);
extern void* pokemonBiosGetInitDp(u8* ptr);
extern u32 pokemonBiosGetDarkFlag(u8* ptr);
extern s32 pokemonGetTasteLike(u8* ptr, void* arg2);
extern s32 pokemonIsHpMantan(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonGetJoutai08(void* a, u16 b);
#endif
extern s32 pokemonGetJoutai0C(void* a, u16 b);
extern s32 pokemonGetJoutaiParam(void* a, u16 b);
extern s32 fn_8012182C(void* a, u16 b);
extern s32 pokemonGetJoutaiKaisuu(void* a, u16 b);
extern s32 pokemonGetJoutaiStack(void* a, u16 b);
extern s32 pokemonCheckWriteJoutaiDataId(void* a, u16 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonGetAnnonKatati(u32 val);
#endif
extern u32 pokemonGetLevelToExp(u8* ptr, u8 idx);
extern s32 pokemonGetNowHpPercentage(u8* ptr);
extern u16 pokemonGetNowHpWaruValue(u8* ptr, s32 b);
extern u16 pokemonGetMaxHpWaruValue(u8* ptr, s32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonGetSoubiItemSoubiDataId(u8* ptr);
#endif
extern void pokemonSetTokuseiFlag(u8* ptr, u32 arg2);
extern u32 heroCheckValid(u8* ptr);
extern void heroMoveGetHeroRot(u32 param);
extern void heroMoveGetHeroPos(u32 param);
extern u32 heroMoveGetResID(u32* out_zero, u32* out_val, s32 index);

#ifdef TARGET_PC
/* PC: the same steps by field (the GameCube version below reads the node, generator and bank slots at GameCube
 * offsets); as gsParticleKillAll does it. field_48 holds the attached model as a 32-bit word. */
static inline void destroyFieldParticleInstance(u8* obj, u32 notify) {
    FieldParticleNode* node = (FieldParticleNode*)obj;
    FieldParticleBank* bank;
    u32 i;

    if ((u8)notify == 1) {
        psKillFamily(node->generator->family_id, node->generator->link_no);
    }
    if (node->transform_state != 0) {
        GSmodelSet60fpsAnimFlag(PC_PTR(node->field_48), 0);
        node->field_48 = 0;
        node->field_4C = 0;
        node->field_06 = 0;
        node->field_05 = 0;
        psUnlinkChildGensFromJObj((PSGeneratorState*)node->generator);
        node->transform_state = 0;
        GSvecCopy(node->local_position, node->transformed_position);
        node->generator->position[0] = node->transformed_position[0];
        node->generator->position[1] = node->transformed_position[1];
        node->generator->position[2] = node->transformed_position[2];
        GSvecCopy(node->local_rotation, node->transformed_rotation);
        node->generator->rotation[0] = node->transformed_rotation[0];
        node->generator->rotation[1] = node->transformed_rotation[1];
        node->generator->rotation[2] = node->transformed_rotation[2];
        GSvecCopy(node->local_scale, node->transformed_scale);
        node->generator->scale[0] = node->transformed_scale[0];
        node->generator->scale[1] = node->transformed_scale[1];
        node->generator->scale[2] = node->transformed_scale[2];
    }
    psKillGenerator(node->generator);
    bank = node->bank;
    for (i = 0; i < 0x40; i++) {
        if (bank->slots[i] == node) {
            bank->slots[i] = NULL;
            break;
        }
    }
    node->active = 0;
}
#else
static inline void destroyFieldParticleInstance(u8* obj, u32 notify) {
    u8* model;
    u32* base;
    u32* scan;
    u32 i;
    s32* active;

    if ((u8)notify == 1) {
        model = *(u8**)(obj + 0x10);
        psKillFamily(*(u16*)(model + 0x18), model[0x15]);
    }

    active = (s32*)(obj + 0x44);
    if (*active != 0 && *active != 0) {
        GSmodelSet60fpsAnimFlag(*(u32*)(obj + 0x48), 0);
        *(u32*)(obj + 0x48) = 0;
        *(u32*)(obj + 0x4C) = 0;
        obj[6] = 0;
        obj[5] = 0;
        psUnlinkChildGensFromJObj(
            *(PSGeneratorState**)(obj + 0x10));
        *active = 0;

        if (*active == 0) {
            GSvecCopy(obj + 0x14, obj + 0x50);
            *(f32*)(*(u8**)(obj + 0x10) + 0x20) = *(f32*)(obj + 0x50);
            *(f32*)(*(u8**)(obj + 0x10) + 0x24) = *(f32*)(obj + 0x54);
            *(f32*)(*(u8**)(obj + 0x10) + 0x28) = *(f32*)(obj + 0x58);
        } else {
            GSvecCopy(obj + 0x50, obj + 0x50);
        }

        if (*active == 0) {
            GSvecCopy(obj + 0x20, obj + 0x5C);
            *(f32*)(*(u8**)(obj + 0x10) + 0x8C) = *(f32*)(obj + 0x5C);
            *(f32*)(*(u8**)(obj + 0x10) + 0x90) = *(f32*)(obj + 0x60);
            *(f32*)(*(u8**)(obj + 0x10) + 0x94) = *(f32*)(obj + 0x64);
        } else {
            GSvecCopy(obj + 0x5C, obj + 0x5C);
        }

        if (*active == 0) {
            GSvecCopy(obj + 0x2C, obj + 0x68);
            *(f32*)(*(u8**)(obj + 0x10) + 0x98) = *(f32*)(obj + 0x68);
            *(f32*)(*(u8**)(obj + 0x10) + 0x9C) = *(f32*)(obj + 0x6C);
            *(f32*)(*(u8**)(obj + 0x10) + 0xA0) = *(f32*)(obj + 0x70);
        } else {
            GSvecCopy(obj + 0x68, obj + 0x68);
        }
    }

    psKillGenerator(*(u32*)(obj + 0x10));

    base = *(u32**)(obj + 0x0C);
    scan = base;
    for (i = 0; i < 0x40; i++) {
        if (scan[2] == PC_PTR32(obj)) {
            base[i + 2] = 0;
            break;
        }
        scan++;
    }

    obj[0] = 0;
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_80117E58_80118100)
/* 0x80117E58 | 0x1C8 */
extern u32 lbl_8047AD80;
extern u32 lbl_8047AD84;
extern u32 lbl_80478B40;
extern u32 lbl_8047AD88;
extern u32 lbl_8047AD8C;
extern u32 lbl_8047AD90;
extern u32 lbl_8047AD94;
void tvInitFloor(void* arg) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelResetTextureChange(void* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GStextureFree(void* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelFree(void* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GStextureCreate(u16 a, u16 b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorOpenModel(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetVisibility(void* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkTexAnimToAnim(void* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(void* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(void* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetTextureChange(void* a, void* b);
#endif
    u8* p48;
    u8* ptr;
    u32 count;
    u8 found;

#ifdef TARGET_PC
    /* floor record (big-endian) -> per-room slot (native 32-bit, written by the room's prolog) -> room data
     * (big-endian): +0 points to the entry count, +4 to the entries (0x18 bytes each) */
    p48 = DISC_PTR_AT(u8, (u8*)arg + 0x48);
    if (p48 == NULL) { return; }
    lbl_8047AD80 = DISC_U32(PTR32_AT_T(u8, p48, 0) + 0x4);
    if (lbl_8047AD80 == 0) { return; }
    lbl_8047AD84 = DISC_U32(DISC_PTR_AT(u8, PTR32_AT_T(u8, p48, 0)));
    if (lbl_8047AD84 == 0) { return; }
#else
    p48 = *(u8**)((u8*)arg + 0x48);
    if (p48 == NULL) { return; }
    lbl_8047AD80 = *(u32*)(*(u8**)p48 + 0x4);
    if (lbl_8047AD80 == 0) { return; }
    lbl_8047AD84 = *(u32*)(*(u8**)(*(u8**)(*(u8**)((u8*)arg + 0x48))) + 0x0);
    if (lbl_8047AD84 == 0) { return; }
#endif
    if ((s32)lbl_80478B40 == 0) { return; }

    ptr = (u8*)PC_PTR(lbl_8047AD88);
    if (ptr != NULL) {
        GSmodelResetTextureChange(GSresGetResource(std093_getCurrentGroupID(), DISC_U32(ptr + 8)));
        if (lbl_8047AD8C != 0) {
            GStextureFree((void*)PC_PTR(lbl_8047AD8C));
            lbl_8047AD8C = 0;
        }
        if (lbl_8047AD90 != 0) {
            GSmodelFree((void*)PC_PTR(lbl_8047AD90));
            lbl_8047AD90 = 0;
        }
        lbl_8047AD94 = 0;
        lbl_80478B40 = (u32)-1;
    }

    count = lbl_8047AD84;
    found = 0;
    lbl_8047AD88 = lbl_8047AD80;
    while (count != 0) {
        u8* e = (u8*)PC_PTR(lbl_8047AD88);
        if (DISC_U32(e + 4) == 0) {
            found = 1;
            break;
        }
        lbl_8047AD88 = PC_PTR32(e) + 0x18;
        count--;
    }
    if (!found) {
        lbl_8047AD88 = 0;
        return;
    }

    lbl_8047AD8C = PC_PTR32(GStextureCreate(DISC_U16((u8*)PC_PTR(lbl_8047AD88) + 0), DISC_U16((u8*)PC_PTR(lbl_8047AD88) + 2), 0x44, 0, 0));
    if (lbl_8047AD8C == 0) {
        lbl_8047AD88 = 0;
        return;
    }
    lbl_8047AD90 = PC_PTR32(floorOpenModel(std093_getCurrentGroupID(), DISC_U32((u8*)PC_PTR(lbl_8047AD88) + 0xc)));
    GSmodelSetVisibility((void*)PC_PTR(lbl_8047AD90), 0);
    GSmodelLinkTexAnimToAnim((void*)PC_PTR(lbl_8047AD90), 1);
    GSmodelSetAnimIndex((void*)PC_PTR(lbl_8047AD90), DISC_U32((u8*)PC_PTR(lbl_8047AD88) + 0x10));
    GSmodelStartAnimation((void*)PC_PTR(lbl_8047AD90));
    lbl_8047AD94 = PC_PTR32(GSresGetResource(std093_getCurrentGroupID(), DISC_U32((u8*)PC_PTR(lbl_8047AD88) + 0x14)));
    GSmodelSetTextureChange(GSresGetResource(std093_getCurrentGroupID(), DISC_U32((u8*)PC_PTR(lbl_8047AD88) + 8)), (void*)PC_PTR(lbl_8047AD8C));
    lbl_80478B40 = 0;
}
/* 0x48 | fn_80118020 | single_call_straight */
extern u32 lbl_802727C8[];
void fn_80118020(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800FF4D4(void* ptr, u32 val);
#endif
#ifdef TARGET_PC
    /* PC: the generated table holds 32-bit addresses; the handler needs native pointers */
    void* local[3];
    local[0] = PC_PTR(lbl_802727C8[0]);
    local[1] = PC_PTR(lbl_802727C8[1]);
    local[2] = PC_PTR(lbl_802727C8[2]);
#else
    u32 local[3];
    local[0] = lbl_802727C8[0];
    local[1] = lbl_802727C8[1];
    local[2] = lbl_802727C8[2];
#endif
    fn_800FF4D4(local, 1);
}
/* Address: 0x80118068 | Size: 0x8 | Pattern: return_constant */
u32 fn_80118068(void) { return 0; }
/* 0x80118070 | 0x90 */
extern u32 lbl_8047AD88;
extern u32 lbl_8047AD8C;
extern u32 lbl_8047AD90;
extern u32 lbl_8047AD94;
extern u32 lbl_80478B40;
extern u32 lbl_8047AD80;
extern u32 lbl_8047AD84;
void tvRelease2(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelResetTextureChange(void* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GStextureFree(void* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelFree(void* a);
#endif
    u8* ptr = (u8*)PC_PTR(lbl_8047AD88);
    if (ptr != NULL) {
        GSmodelResetTextureChange(GSresGetResource(std093_getCurrentGroupID(), DISC_U32(ptr + 8)));
        if (lbl_8047AD8C != 0) {
            GStextureFree((void*)PC_PTR(lbl_8047AD8C));
            lbl_8047AD8C = 0;
        }
        if (lbl_8047AD90 != 0) {
            GSmodelFree((void*)PC_PTR(lbl_8047AD90));
            lbl_8047AD90 = 0;
        }
        lbl_8047AD94 = 0;
        lbl_8047AD88 = 0;
        lbl_80478B40 = (u32)-1;
    }
    lbl_8047AD80 = 0;
    lbl_8047AD84 = 0;
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_80118100_801183EC)
/* 0x80118100 | 0x4 | void_stub */
void fn_80118100(void) {
}
/* 0x80118104 | 0xAC */
void gsParticleDraw(u32 a, u8 b) {
    void* result;
    u32 val;

    result = GScameraGetActiveCamera();
    if (result != NULL) {
#ifdef TARGET_PC /* the camera's HSD camera by field (GSRenderCamera, +0x0C on the GameCube) */
        psSetBillboardCamera(((GSRenderCamera*)result)->cobj);
#else
        psSetBillboardCamera(*(void**)((u8*)result + 0xC));
#endif
        switch (a) {
        case 0x10:
            val = 0;
            break;
        case 0x1000:
            val = 1;
            break;
        case 0x2000:
            val = 2;
            break;
        }
        if ((u8)b == 0) {
            psDispMain(1, val);
        } else {
            psDispMain(2, val);
        }
    }
}
/* 0x801181B0 | 0x23C */
extern FieldParticleBank* lbl_8047AD9C;
extern u32 lbl_8047ADA0;
void gsParticleUpdateAll(void) {
    FieldParticleBankCursor slot_cursor;
    u32 outer_index;
    u32 inner_index;
    FieldParticleNode* node;
    FieldParticleBank* bank;
    u32 max_life;

    outer_index = 0;
    while (outer_index < lbl_8047ADA0) {
        bank = &lbl_8047AD9C[outer_index];
        if (bank->active == 1) {
            inner_index = 0;
            slot_cursor.bank = bank;
            do {
                node = slot_cursor.slot_window[2];
                if (node != NULL && node->field_01 == 0) {
                    if (node->field_01 == 1) {
                        max_life = -1;
                    } else {
                        max_life = psGetGeneratorChildMaxLife(
                            (PSGeneratorState*)node->generator);
                    }
                    if (max_life == 0 &&
                        psGetParticleChildCount((PSGeneratorState*)node->generator) == 0) {
                        u32 i;
                        FieldParticleBankCursor owner_bank;
                        FieldParticleNode** base_slot;
                        FieldParticleNode** bank_slot;

                        psKillFamily(node->generator->family_id, node->generator->link_no);
                        if (node->transform_state != 0 && node->transform_state != 0) {
                            GSmodelSet60fpsAnimFlag(PC_PTR(node->field_48), 0); /* the model as a 32-bit word: a pointer again (PC) */
                            node->field_48 = 0;
                            node->field_4C = 0;
                            node->field_06 = 0;
                            node->field_05 = 0;
                            psUnlinkChildGensFromJObj(
                                (PSGeneratorState*)node->generator);
                            node->transform_state = 0;

                            if (node->transform_state == 0) {
                                GSvecCopy(node->local_position, node->transformed_position);
                                node->generator->position[0] = node->transformed_position[0];
                                node->generator->position[1] = node->transformed_position[1];
                                node->generator->position[2] = node->transformed_position[2];
                            } else {
                                GSvecCopy(node->transformed_position, node->transformed_position);
                            }

                            if (node->transform_state == 0) {
                                GSvecCopy(node->local_rotation, node->transformed_rotation);
                                node->generator->rotation[0] = node->transformed_rotation[0];
                                node->generator->rotation[1] = node->transformed_rotation[1];
                                node->generator->rotation[2] = node->transformed_rotation[2];
                            } else {
                                GSvecCopy(node->transformed_rotation, node->transformed_rotation);
                            }

                            if (node->transform_state == 0) {
                                GSvecCopy(node->local_scale, node->transformed_scale);
                                node->generator->scale[0] = node->transformed_scale[0];
                                node->generator->scale[1] = node->transformed_scale[1];
                                node->generator->scale[2] = node->transformed_scale[2];
                            } else {
                                GSvecCopy(node->transformed_scale, node->transformed_scale);
                            }
                        }

                        psKillGenerator(node->generator);

                        owner_bank.bank = node->bank;
                        base_slot = owner_bank.slot_window;
                        bank_slot = base_slot;
                        for (i = 0; i < 0x40; i++) {
                            if (bank_slot[2] == node) {
                                base_slot[i + 2] = NULL;
                                break;
                            }
                            bank_slot++;
                        }

                        node->active = 0;
                    }
                }
                inner_index++;
                slot_cursor.slot_window++;
            } while (inner_index < 0x40);
        }
        outer_index++;
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_CANDIDATE_801183EC_80118874)
/* 0x801183EC | 0x488 */
extern FieldParticleBank* lbl_8047AD9C;
extern u32 lbl_8047ADA0;
extern u8 lbl_8047ADB0;
/* Copy a vector into the node's local transform and mirror it into the
 * generator (each helper is expanded two to four times below). */
static inline void fieldParticleSetPosition(FieldParticleNode* node, f32* v) {
    GSvecCopy(node->local_position, v);
    node->generator->position[0] = v[0];
    node->generator->position[1] = v[1];
    node->generator->position[2] = v[2];
}

static inline void fieldParticleSetRotation(FieldParticleNode* node, f32* v) {
    GSvecCopy(node->local_rotation, v);
    node->generator->rotation[0] = v[0];
    node->generator->rotation[1] = v[1];
    node->generator->rotation[2] = v[2];
}

static inline void fieldParticleSetScale(FieldParticleNode* node, f32* v) {
    GSvecCopy(node->local_scale, v);
    node->generator->scale[0] = v[0];
    node->generator->scale[1] = v[1];
    node->generator->scale[2] = v[2];
}

void gsParticleUpdate(u32 particleCount) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmodelGetPart(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSpartGetTransform(void*, f32*, f32*, f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSpartFree(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecTransformQuat(f32*, f32*, f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecAdd(f32*, f32*, f32*);
#endif

    f32 pos[3];
    f32 rot[3];
    f32 scale[3];
    f32 quat[4];
    f32 offset[3];
    u32 i;
    u32 outerIdx = 0;

    while (outerIdx < lbl_8047ADA0) {
        FieldParticleBank* bank = &lbl_8047AD9C[outerIdx];
        if (bank->active == 1) {
            u32 innerIdx = 0;
            do {
                FieldParticleNode* node = bank->slots[innerIdx];
                if (node != NULL && node->transform_state != 0 && node->field_06 != 0) {
                    void* part = GSmodelGetPart(PC_PTR(node->field_48), node->field_4C);

                    GSpartGetTransform(part, pos, rot, scale);
                    GSpartFree(part);
                    GSquatFromEuler(quat, rot);
                    GSvecTransformQuat(offset, quat, node->transformed_position);
                    GSvecAdd(pos, pos, offset);
                    GSvecAdd(rot, rot, node->transformed_rotation);
                    fn_800E0108(scale, scale, node->transformed_scale);

                    fieldParticleSetPosition(node, node->transformed_position);
                    fieldParticleSetRotation(node, node->transformed_rotation);
                    fieldParticleSetScale(node, node->transformed_scale);

                    /* Retail's jump table: 1 position, 2 rotation, 3 scale,
                     * 4 position+rotation, 5 rotation+scale,
                     * 6 position+scale, 7 all three; 0 does nothing. */
                    switch (node->transform_state) {
                    case 1:
                        fieldParticleSetPosition(node, pos);
                        break;
                    case 2:
                        fieldParticleSetRotation(node, rot);
                        break;
                    case 3:
                        fieldParticleSetScale(node, scale);
                        break;
                    case 4:
                        fieldParticleSetPosition(node, pos);
                        fieldParticleSetRotation(node, rot);
                        break;
                    case 5:
                        fieldParticleSetRotation(node, rot);
                        fieldParticleSetScale(node, scale);
                        break;
                    case 6:
                        fieldParticleSetPosition(node, pos);
                        fieldParticleSetScale(node, scale);
                        break;
                    case 7:
                        fieldParticleSetPosition(node, pos);
                        fieldParticleSetRotation(node, rot);
                        fieldParticleSetScale(node, scale);
                        break;
                    }

                    node->field_06 = node->field_05 == 0;
                }
                innerIdx++;
            } while (innerIdx < 0x40);
        }
        outerIdx++;
    }

    for (i = 0; i < particleCount; i++) {
        psInterpretParticles(0);
        psExecGenerator(0);
    }

    if (GSgetVersionMajor() == 2) {
        if (++lbl_8047ADB0 >= 5) {
            psInterpretParticles(0);
            psExecGenerator(0);
            lbl_8047ADB0 = 0;
        }
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_80118874_801195AC)
/* 0x80118A68 | 0x1B8 */
/* 0x80118874 | 0x1F4 */
#ifdef TARGET_PC
/* PC: the bank and its nodes are the native structs here (FieldParticleBank / FieldParticleNode, 8-byte pointers: the
 * node slots start at +0x10 and every node field after +0x08 sits 0x10 bytes later than the GameCube offsets the
 * versions below use). The same code by field name; the GameCube text is unchanged under #else. */
void gsParticleKill(u8* list, u32 notify) {
    u32 i;

    for (i = 0; i < 0x40; i++) {
        u8* obj = (u8*)((FieldParticleBank*)list)->slots[i];
        if (obj != NULL) {
            destroyFieldParticleInstance(obj, notify);
        }
    }
}
void gsParticleKillOwned(u8* obj, u32 notify) {
    destroyFieldParticleInstance(obj, notify);
}
void GSmodelSetParticleVisibility(void* bank, u32 visible) {
    u32 i;

    for (i = 0; i < 0x40; i++) {
        FieldParticleNode* node = ((FieldParticleBank*)bank)->slots[i];
        if (node) {
            psSetParticleVisibility((PSGeneratorState*)node->generator, visible);
        }
    }
}
#define FIELD_NODE_GEN(obj) ((PSGeneratorState*)((FieldParticleNode*)(obj))->generator)
void fn_80118C88(void* obj, u32 visible) {
    psSetParticleVisibility(FIELD_NODE_GEN(obj), visible);
}
void fn_80118CAC(void* obj, u8 enabled) {
    psSetRandomVelocityScaling(FIELD_NODE_GEN(obj), enabled);
}
void fn_80118CD0(void* obj, u8 enabled) {
    psSetNodeScaling(FIELD_NODE_GEN(obj), enabled);
}
void fn_80118CF4(void* obj, u8 enabledA, u8 enabledB) {
    psSetTornadoScaling(FIELD_NODE_GEN(obj), enabledA, enabledB);
}
void fn_80118D18(void* obj, u8 enabled) {
    psSetParticleTexScaling(FIELD_NODE_GEN(obj), enabled);
}
void fn_80118D3C(void* obj, u8 enabledA, u8 enabledB) {
    psSetOffsetRotationInLocal(FIELD_NODE_GEN(obj), enabledA, enabledB);
}
void fn_80118D60(void* obj, u8 enabled) {
    psSetVelocityRotationInLocal(FIELD_NODE_GEN(obj), enabled);
}
u32 fn_80118D84(void* obj) {
    return psGetParticleChildCount(FIELD_NODE_GEN(obj));
}
s32 fn_80118DA8(u8* ptr) {
    FieldParticleNodePtr node;

    node.raw = ptr;
    if (node.typed->field_01 == 1) { return -1; }
    return psGetGeneratorChildMaxLife((PSGeneratorState*)node.typed->generator);
}
/* scale (GameCube +0x2C / +0x68, generator +0x98) */
void fn_80118DE0(u8* arg1, f32* arg2, u32 arg3, u32 arg4) {
    extern void psSetGeneratorAngleRadiusScale();
    FieldParticleNode* node = (FieldParticleNode*)arg1;
    if (node->transform_state == 0) {
        GSvecCopy(node->local_scale, arg2);
        node->generator->scale[0] = arg2[0];
        node->generator->scale[1] = arg2[1];
        node->generator->scale[2] = arg2[2];
    } else {
        GSvecCopy(node->transformed_scale, arg2);
    }
    if ((arg3 & 0xFF) == 1) {
        psSetGeneratorAngleRadiusScale(node->generator, arg2, (void*)PC_PTR(arg4));
    }
}
/* rotation (GameCube +0x20 / +0x5C, generator +0x8C) */
void fn_80118E8C(u8* arg1, f32* arg2) {
    FieldParticleNode* node = (FieldParticleNode*)arg1;
    if (node->transform_state == 0) {
        GSvecCopy(node->local_rotation, arg2);
        node->generator->rotation[0] = arg2[0];
        node->generator->rotation[1] = arg2[1];
        node->generator->rotation[2] = arg2[2];
    } else {
        GSvecCopy(node->transformed_rotation, arg2);
    }
}
/* position (GameCube +0x14 / +0x50, generator +0x20) */
void gsParticleSetVecB(u8* arg1, f32* arg2) {
    FieldParticleNode* node = (FieldParticleNode*)arg1;
    if (node->transform_state == 0) {
        GSvecCopy(node->local_position, arg2);
        node->generator->position[0] = arg2[0];
        node->generator->position[1] = arg2[1];
        node->generator->position[2] = arg2[2];
    } else {
        GSvecCopy(node->transformed_position, arg2);
    }
}
void fn_80118F7C(u8* obj, f32* arg) {
    FieldParticleNode* node = (FieldParticleNode*)obj;
    set__5GSvecFfff(arg, node->position[0], node->position[1], node->position[2]);
}
extern const f32 lbl_8047CFE8; /* 0.0f */
extern const f32 lbl_8047CFEC; /* 1.0f */
void gsParticleLinkToJObj(u8* obj, GSpart* part, u32 state, u32 byte5,
                 u32 init_from_zero, u32 attach_model) {
    FieldParticleNode* node = (FieldParticleNode*)obj;
    f32 zero;
    f32 one;

    if (node->transform_state == 0 && (s32)state != 0) {
        node->field_48 = PC_PTR32(part->model); /* the model as a 32-bit word (read back through PC_PTR) */
        node->field_4C = part->index;
        if ((u8)init_from_zero == 1) {
            zero = lbl_8047CFE8;
            set__5GSvecFfff(node->transformed_position, zero, zero, zero);
            zero = lbl_8047CFE8;
            set__5GSvecFfff(node->transformed_rotation, zero, zero, zero);
            one = lbl_8047CFEC;
            set__5GSvecFfff(node->transformed_scale, one, one, one);
        } else {
            GSvecCopy(node->transformed_position, node->local_position);
            GSvecCopy(node->transformed_rotation, node->local_rotation);
            GSvecCopy(node->transformed_scale, node->local_scale);
        }
        zero = lbl_8047CFE8;
        set__5GSvecFfff(node->local_position, zero, zero, zero);
        zero = lbl_8047CFE8;
        set__5GSvecFfff(node->local_rotation, zero, zero, zero);
        one = lbl_8047CFEC;
        set__5GSvecFfff(node->local_scale, one, one, one);
        if ((u8)attach_model != 0) {
            psLinkChildGensToJObj((PSGeneratorState*)node->generator, part->jobj);
        }
        node->transform_state = state;
        obj[5] = (u8)byte5;
        obj[6] = 1;
    }
}
#else
void gsParticleKill(u8* list, u32 notify) {
    u8* entry = list;
    u8* obj;
    u32 i = 0;

    do {
        obj = ((FieldParticleInstancePtr*)(entry + 8))->raw;
        if (obj != NULL) {
            destroyFieldParticleInstance(obj, notify);
        }
        i++;
        entry += 4;
    } while (i < 0x40);
}
void gsParticleKillOwned(u8* obj, u32 notify) {
    destroyFieldParticleInstance(obj, notify);
}
/* 0x68 | GSmodelSetParticleVisibility | guarded_call */
void GSmodelSetParticleVisibility(void* bank, u32 visible) {
    u8* scan = bank;
    u32 i = 0;
    for (i = 0; i < 0x40; i++, scan += 4) {
        void* entry = *(void**)(scan + 8);
        if (entry) {
            psSetParticleVisibility(
                *(PSGeneratorState**)((u8*)entry + 0x10), visible);
        }
    }
}
/* 0x80118C88 | 36 bytes | load_then_call */
void fn_80118C88(void* obj, u32 visible) {
    psSetParticleVisibility(*(PSGeneratorState**)((u8*)obj + 0x10), visible);
}
/* 0x80118CAC | 36 bytes | load_then_call */
void fn_80118CAC(void* obj, u8 enabled) {
    psSetRandomVelocityScaling(*(PSGeneratorState**)((u8*)obj + 0x10), enabled);
}
/* 0x80118CD0 | 36 bytes | load_then_call */
void fn_80118CD0(void* obj, u8 enabled) {
    psSetNodeScaling(*(PSGeneratorState**)((u8*)obj + 0x10), enabled);
}
/* 0x80118CF4 | 36 bytes | load_then_call */
void fn_80118CF4(void* obj, u8 enabledA, u8 enabledB) {
    psSetTornadoScaling(*(PSGeneratorState**)((u8*)obj + 0x10), enabledA,
                        enabledB);
}
/* 0x80118D18 | 36 bytes | load_then_call */
void fn_80118D18(void* obj, u8 enabled) {
    psSetParticleTexScaling(*(PSGeneratorState**)((u8*)obj + 0x10), enabled);
}
/* 0x80118D3C | 36 bytes | load_then_call */
void fn_80118D3C(void* obj, u8 enabledA, u8 enabledB) {
    psSetOffsetRotationInLocal(*(PSGeneratorState**)((u8*)obj + 0x10),
                               enabledA, enabledB);
}
/* 0x80118D60 | 36 bytes | load_then_call */
void fn_80118D60(void* obj, u8 enabled) {
    psSetVelocityRotationInLocal(*(PSGeneratorState**)((u8*)obj + 0x10),
                                 enabled);
}
/* 0x80118D84 | 36 bytes | load_then_call */
u32 fn_80118D84(void* obj) {
    return psGetParticleChildCount(*(PSGeneratorState**)((u8*)obj + 0x10));
}
/* 0x80118DA8 | 0x38 */
s32 fn_80118DA8(u8* ptr) {
    FieldParticleNodePtr node;

    node.raw = ptr;
    if (node.typed->field_01 == 1) { return -1; }
    return psGetGeneratorChildMaxLife((PSGeneratorState*)node.typed->generator);
}
/* 0x80118DE0 | 0xAC */
void fn_80118DE0(u8* arg1, f32* arg2, u32 arg3, u32 arg4) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecCopy();
#endif
    extern void psSetGeneratorAngleRadiusScale();
    if ((s32)*(u32*)(arg1 + 0x44) == 0) {
        GSvecCopy(arg1 + 0x2c, arg2); /* r4 = arg2 untouched (assembly) */
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x98) = arg2[0];
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x9c) = arg2[1];
        *(f32*)(*(u8**)(arg1 + 0x10) + 0xa0) = arg2[2];
    } else {
        GSvecCopy(arg1 + 0x68, arg2); /* r4 = arg2 untouched (assembly) */
    }
    if ((arg3 & 0xFF) == 1) {
        psSetGeneratorAngleRadiusScale(*(void**)(arg1 + 0x10), arg2, (void*)PC_PTR(arg4));
    }
}
/* 0x78 | fn_80118E8C | two_call_arg_check */
void fn_80118E8C(u8* arg1, f32* arg2) {
    if ((s32)*(u32*)(arg1 + 0x44) == 0) {
        GSvecCopy(arg1 + 0x20, arg2); /* r4 = arg2 untouched (assembly) */
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x8c) = arg2[0];
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x90) = arg2[1];
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x94) = arg2[2];
    } else {
        GSvecCopy(arg1 + 0x5c, arg2); /* r4 = arg2 untouched (assembly) */
    }
}
/* 0x78 | gsParticleSetVecB | two_call_arg_check */
void gsParticleSetVecB(u8* arg1, f32* arg2) {
    if ((s32)*(u32*)(arg1 + 0x44) == 0) {
        GSvecCopy(arg1 + 0x14, arg2); /* r4 = arg2 untouched (assembly) */
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x20) = arg2[0];
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x24) = arg2[1];
        *(f32*)(*(u8**)(arg1 + 0x10) + 0x28) = arg2[2];
    } else {
        GSvecCopy(arg1 + 0x50, arg2); /* r4 = arg2 untouched (assembly) */
    }
}
/* 0x80118F7C | 0x34 */
void fn_80118F7C(u8* obj, f32* arg) {
    f32 f1 = *(f32*)(&obj[0x38]);
    f32 f2 = *(f32*)(&obj[0x3C]);
    f32 f3 = *(f32*)(&obj[0x40]);
    set__5GSvecFfff(arg, f1, f2, f3);
}
/* 0x80118FB0 | 0x12C */
extern const f32 lbl_8047CFE8; /* 0.0f */
extern const f32 lbl_8047CFEC; /* 1.0f */
void gsParticleLinkToJObj(u8* obj, GSpart* part, u32 state, u32 byte5,
                 u32 init_from_zero, u32 attach_model) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecCopy(void* dst, void* src);
#endif
    u8* desc = (u8*)part;
    f32 zero;
    f32 one;

    if (*(s32*)(obj + 0x44) == 0 && (s32)state != 0) {
        *(u32*)(obj + 0x48) = *(u32*)(desc + 0x4);
        *(u32*)(obj + 0x4C) = *(u16*)(desc + 0x2);
        if ((u8)init_from_zero == 1) {
            zero = lbl_8047CFE8;
            set__5GSvecFfff((f32*)(obj + 0x50), zero, zero, zero);
            zero = lbl_8047CFE8;
            set__5GSvecFfff((f32*)(obj + 0x5C), zero, zero, zero);
            one = lbl_8047CFEC;
            set__5GSvecFfff((f32*)(obj + 0x68), one, one, one);
        } else {
            GSvecCopy(obj + 0x50, obj + 0x14);
            GSvecCopy(obj + 0x5C, obj + 0x20);
            GSvecCopy(obj + 0x68, obj + 0x2C);
        }
        zero = lbl_8047CFE8;
        set__5GSvecFfff((f32*)(obj + 0x14), zero, zero, zero);
        zero = lbl_8047CFE8;
        set__5GSvecFfff((f32*)(obj + 0x20), zero, zero, zero);
        one = lbl_8047CFEC;
        set__5GSvecFfff((f32*)(obj + 0x2C), one, one, one);
        if ((u8)attach_model != 0) {
            psLinkChildGensToJObj(
                *(PSGeneratorState**)(obj + 0x10), *(void**)(desc + 0x8));
        }
        *(u32*)(obj + 0x44) = state;
        obj[5] = (u8)byte5;
        obj[6] = 1;
    }
}
#endif /* TARGET_PC */
/* 0x801190DC | 0x2E0 */
extern u32 lbl_8047ADAC;
extern u32 lbl_8047ADA8;
static inline u8* fn_801190DC_findFreeNode(void) {
    FieldParticleNodeRaw* node = (FieldParticleNodeRaw*)PC_PTR(lbl_8047ADA8);
    u32 i;

    for (i = 0; i < lbl_8047ADAC; i++, node++) { /* FieldParticleNodeRaw: sizeof(FieldParticleNode) on the PC */
        if (node->bytes[0] == 0) {
            return node->bytes;
        }
    }
    return NULL;
}

static inline s32 fn_801190DC_findFreeSlot(u8* texture) {
    u32 i;

    for (i = 0; i < 64; i++) {
#ifdef TARGET_PC /* the bank's node slots are native pointers on the PC (FieldParticleBank) */
        if (((FieldParticleBank*)texture)->slots[i] == NULL) {
#else
        if (*(u32*)(texture + 0x08 + i * 4) == 0) {
#endif
            return i;
        }
    }
    return -1;
}

/* TODO: use FieldParticleBank/FieldParticleNode fields throughout once MWCC
 * preserves this function's exact pointer-CSE and indexed-store shapes. */
u8* gsParticleCreate(u8* texture, u32 selector, u32 subid) {
    u8* node;
    s32 slot;
    u32 use_alt;

    if (texture[0] == 0) {
        return NULL;
    }

    node = fn_801190DC_findFreeNode();
    if (node == NULL) {
        return NULL;
    }

    if ((slot = fn_801190DC_findFreeSlot(texture)) == -1) {
        gsParticleKillOldest((FieldParticleInstanceList*)texture);
        if ((slot = fn_801190DC_findFreeSlot(texture)) == -1) {
            return NULL;
        }
    }

    use_alt = 0;
    if ((u8)subid == 1) {
        use_alt = 1;
    }
    ((FieldParticleNode*)node)->generator = (FieldParticleGenerator*)psCreateGeneratorID(use_alt, texture[1], selector);
    if (((FieldParticleNode*)node)->generator == NULL) {
        return NULL;
    }

    ((FieldParticleNode*)node)->generator->enabled = 1;
    node[0] = 1;
    ((FieldParticleNode*)node)->bank = ((FieldParticleBank*)texture);
    ((FieldParticleNode*)node)->selector = (u16)selector;
    ((FieldParticleNode*)node)->transform_state = 0;
    ((FieldParticleNode*)node)->field_48 = 0;
    ((FieldParticleNode*)node)->field_4C = 0;
    node[5] = 0;
    node[6] = 0;

    set__5GSvecFfff(((FieldParticleNode*)node)->position,
                    ((FieldParticleNode*)node)->generator->position[0],
                    ((FieldParticleNode*)node)->generator->position[1],
                    ((FieldParticleNode*)node)->generator->position[2]);
    node[4] = (u8)subid;
    ((FieldParticleNode*)node)->field_08 = (void*)fn_800D3094();
#ifdef TARGET_PC
    ((FieldParticleBank*)texture)->slots[slot] = (FieldParticleNode*)node;
#else
    ((u32*)(texture + 0x08))[slot] = PC_PTR32(node);
#endif

    if (((FieldParticleNode*)node)->generator->field_10 == 0) {
        node[1] = 1;
    } else {
        node[1] = 0;
    }
    return node;
}
/* 0x801193BC | 0x1F0 */
void gsParticleKillAll(FieldParticleBank* bank) {
    struct FieldParticleBankIteration {
        u32 index;
        FieldParticleBankCursor cursor;
    } iteration;
    FieldParticleNode* node;

    for (iteration.index = 0, iteration.cursor.bank = bank;
         iteration.index < 0x40;
         iteration.index++, iteration.cursor.slot_window++) {
        node = iteration.cursor.slot_window[2];
        if (node != NULL) {
            u32 j;
            FieldParticleBankCursor owner_bank;
            FieldParticleNode** base_slot;
            FieldParticleNode** bank_slot;

            psKillFamily(node->generator->family_id, node->generator->link_no);
            if (node->transform_state != 0 && node->transform_state != 0) {
                GSmodelSet60fpsAnimFlag(PC_PTR(node->field_48), 0); /* the model as a 32-bit word: a pointer again (PC) */
                node->field_48 = 0;
                node->field_4C = 0;
                node->field_06 = 0;
                node->field_05 = 0;
                psUnlinkChildGensFromJObj(
                    (PSGeneratorState*)node->generator);
                node->transform_state = 0;

                if (node->transform_state == 0) {
                    GSvecCopy(node->local_position, node->transformed_position);
                    node->generator->position[0] = node->transformed_position[0];
                    node->generator->position[1] = node->transformed_position[1];
                    node->generator->position[2] = node->transformed_position[2];
                } else {
                    GSvecCopy(node->transformed_position, node->transformed_position);
                }

                if (node->transform_state == 0) {
                    GSvecCopy(node->local_rotation, node->transformed_rotation);
                    node->generator->rotation[0] = node->transformed_rotation[0];
                    node->generator->rotation[1] = node->transformed_rotation[1];
                    node->generator->rotation[2] = node->transformed_rotation[2];
                } else {
                    GSvecCopy(node->transformed_rotation, node->transformed_rotation);
                }

                if (node->transform_state == 0) {
                    GSvecCopy(node->local_scale, node->transformed_scale);
                    node->generator->scale[0] = node->transformed_scale[0];
                    node->generator->scale[1] = node->transformed_scale[1];
                    node->generator->scale[2] = node->transformed_scale[2];
                } else {
                    GSvecCopy(node->transformed_scale, node->transformed_scale);
                }
            }

            psKillGenerator(node->generator);

            owner_bank.bank = node->bank;
            base_slot = owner_bank.slot_window;
            bank_slot = base_slot;
            for (j = 0; j < 0x40; j++) {
                if (bank_slot[2] == node) {
                    base_slot[j + 2] = NULL;
                    break;
                }
                bank_slot++;
            }
            node->active = 0;
        }
    }
    bank->active = 0;
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_CANDIDATE_801195AC_80119824)
/* 0x801195AC | 0x278 */
extern u8 lbl_802727D8[];
extern FieldParticleBank* lbl_8047AD9C;
extern u32 lbl_8047ADA0;
typedef struct FieldParticleFile { /* the GPT1 header: offsets from the file start, turned into pointers in place */
    u32 magic;
    PTR32_FIELD(u8) description; /* 32-bit slots on the PC too (pc/ptr32.h) */
    PTR32_FIELD(u8) data;
    u32 dataSize;
    PTR32_FIELD(u8) texture;
} FieldParticleFile;

static inline FieldParticleBank* fieldParticleGetFreeBank(void)
{
    FieldParticleBank* bank = lbl_8047AD9C;
    u32 i;

    for (i = 0; i < lbl_8047ADA0; i++, bank++) {
        if (bank->active == 0) {
            return bank;
        }
    }
    return NULL;
}

static inline u8 fieldParticleTextureTypeUsed(u8 texture_type)
{
    FieldParticleBank* bank = lbl_8047AD9C;
    u32 i;

    for (i = 0; i < lbl_8047ADA0; i++, bank++) {
        if (bank->texture_type == texture_type) {
            return 1;
        }
    }
    return 0;
}

static inline u8 fieldParticleGetFreeTextureType(void)
{
    u8 texture_type;

    for (texture_type = 0; texture_type < 0x40; texture_type++) {
        if (!fieldParticleTextureTypeUsed(texture_type)) {
            return texture_type;
        }
    }
    return 0xFF;
}

void* gsParticleBankLoad(void* resource)
{
    FieldParticleFile* file = resource;
    FieldParticleBank* bank;
    u8 texture_type;
    u32 i;
    char* messages = (char*)lbl_802727D8;
    s32 version;

#ifdef TARGET_PC
    /* the GPT1 header is big-endian: magic and four 32-bit offsets / sizes, made native here (the offsets then become
     * 32-bit pointers in place, as on the GameCube) */
    {
        u32* words = (u32*)file;
        if (words[0] == 0x31545047 || words[0] == 0x30545047) { /* "GPT1" / "GPT0" read little-endian: not yet swapped */
            u32 w;
            for (w = 0; w < 5; w++) {
                words[w] = __builtin_bswap32(words[w]);
            }
        }
    }
#endif
    switch (file->magic) {
    case 0x47505430:
        GSlogWrite(messages + 0x50);
        GSlogWrite(messages + 0xAC);
        GSlogWrite(messages + 0x50);
        return NULL;
    case 0x47505431:
        break;
    default:
        GSlogWrite(messages + 0x108);
        return NULL;
    }

#ifdef TARGET_PC
    file->description.raw += PC_PTR32(file);
    file->data.raw += PC_PTR32(file);
    file->texture.raw += PC_PTR32(file);
    version = __builtin_bswap16(*(u16*)P32(file->description)); /* still big-endian: psInitDataBank converts the bank */
#else
    file->description += PC_PTR32(file);
    file->data += PC_PTR32(file);
    file->texture += PC_PTR32(file);
    version = *(u16*)file->description;
#endif
    if (version != 0x43) {
        GSlogWrite(messages + 0x128, 0x43, version);
        return NULL;
    }
    DCFlushRange(P32(file->data), (file->dataSize + 0x1F) & ~0x1F);

    bank = fieldParticleGetFreeBank();
    if (bank == NULL) {
        return NULL;
    }
    texture_type = fieldParticleGetFreeTextureType();
    if (texture_type == 0xFF) {
        return NULL;
    }

    bank->active = 1;
    bank->texture_type = texture_type;
    bank->file = file;
    for (i = 0; i < 0x40; i++) {
        bank->slots[i] = NULL;
    }
    psInitDataBank(bank->texture_type, P32(file->description), P32(file->data), P32(file->texture), 0);
    return bank;
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_80119824_80119BD0)
/* 0x80119824 | 0x10C */
extern u32 lbl_8047ADA0;
extern u16 lbl_8047AD98;
extern FieldParticleBank* lbl_8047AD9C;
extern u32 lbl_8047ADAC;
extern u16 lbl_8047ADA4;
extern u32 lbl_8047ADA8;
void gsParticleInit(u32 count1, u32 count2) {
    extern u16 lbl_8047AD98;
    extern FieldParticleBank* lbl_8047AD9C;
    extern u32 lbl_8047ADA0;
    extern u16 lbl_8047ADA4;
    extern u32 lbl_8047ADA8;
    extern u32 lbl_8047ADAC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void psInitParticle(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void psInitGenerator(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void psInitAppSRT(s32 a, s32 b);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
    extern void HSD_JObjSetSPtclCallback(void* fp);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
    extern void HSD_JObjSetPtclCallback(void* fp);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
    extern void HSD_JObjSetDPtclCallback(void* fp);
#endif
    extern void gsParticlePtclHook(void);
    u32 h;
    u32 i;
    lbl_8047ADA0 = count1;
#ifdef TARGET_PC /* the PC's records are larger (8-byte pointers) */
    h = _toolentryAlloc__FUl(count1 * sizeof(FieldParticleBank));
#else
    h = _toolentryAlloc__FUl(count1 * 0x108);
#endif
    lbl_8047AD98 = (u16)h;
    if ((u16)h == 0) { return; }
    lbl_8047AD9C = GSmemLock((u16)h);
    for (i = 0; i < lbl_8047ADA0; i++) {
        lbl_8047AD9C[i].active = 0;
    }
    lbl_8047ADAC = count2;
#ifdef TARGET_PC
    h = _toolentryAlloc__FUl(count2 * sizeof(FieldParticleNodeRaw));
#else
    h = _toolentryAlloc__FUl(count2 * 0x74);
#endif
    lbl_8047ADA4 = (u16)h;
    if ((u16)h == 0) { return; }
    lbl_8047ADA8 = PC_PTR32(GSmemLock((u16)h));
    for (i = 0; i < lbl_8047ADAC; i++) {
#ifdef TARGET_PC
        ((FieldParticleNodeRaw*)PC_PTR(lbl_8047ADA8))[i].bytes[0] = 0;
#else
        *(u8*)PC_PTR((lbl_8047ADA8 + i * 0x74)) = 0;
#endif
    }
    psInitParticle(0);
    psInitGenerator(0);
#ifdef TARGET_PC
    psInitAppSRT(0, FIELD_PS_APPSRT_SIZE);
#else
    psInitAppSRT(0, 0x74);
#endif
    HSD_JObjSetSPtclCallback(gsParticlePtclHook);
    HSD_JObjSetPtclCallback(gsParticlePtclHook);
    HSD_JObjSetDPtclCallback(psSetPointJObjNodup);
}
/* 0x80119930 | 0x2A0 */
void gsParticleKillOldest(FieldParticleInstanceList* list) {
    u32 i;
    FieldParticleInstance* instance;
    u32 oldest_age = (u32)-1;
    s32 oldest_index = -1;

    for (i = 0; i < 0x40; i++) {
        instance = list->entries[i].instance;

        if (instance->active == 0) {
            list->entries[i].raw = NULL;
        } else if (instance->age < oldest_age) {
            oldest_age = instance->age;
            oldest_index = i;
        }
    }

    if (oldest_index != -1) {
        destroyFieldParticleInstance(list->entries[oldest_index].raw, 1);
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_CANDIDATE_80119BD0_80119D90)
/* 0x80119BD0 | 0x1C0 */
extern const f32 lbl_8047CFE8; /* 0.0f */
extern const f32 lbl_8047CFEC; /* 1.0f */
#ifdef TARGET_PC
/* ORRE_WAZA_VIEWER_PTCLHOOK's test log (src/game/pc_waza_viewer.c; docs/port/setup.md, "Booting into the move viewer";
 * 2026-11-19, Studio's particle-count question): prints each emitter a model's animation key (or a static particle
 * joint) starts here: the model, the key's two values, the part index and the generator made. Observe-only (it reads
 * what this function already made; nothing is changed) and not a hook: the pointer is NULL except while that move-viewer
 * test boot logs. Weak, as pobj.c's DRAWMTX log, so builds that link this unit without the viewer file still link.
 * Plain C, every OS (verified on Windows only). */
extern void (*pc_waza_viewer_ptclhook_log)(const void* model, u32 key_bank, u32 gen_index, u32 part,
                                           const void* jobj, const void* generator) __attribute__((weak));
#endif
void gsParticlePtclHook(u32 arg1, u32 arg2, u32 arg5, u8* arg6) {
    u8* node = arg6;
    struct GSmodel* resource;
    u8* texture;
    GSpart* entry;
    u8* entryBytes;
    u32 index;
    s32 handle;

    while ((node == NULL ? NULL : (u8*)((HSD_JObj*)node)->parent) != NULL) {
        if (node == NULL) {
            node = NULL;
        } else {
            node = (u8*)((HSD_JObj*)node)->parent;
        }
    }

    resource = GSmodelSearchModelList(node);
    if (resource == NULL) {
        return;
    }

    texture = GSmodelGetLinkedGSparticleBank(resource);
    if (texture == NULL) {
        return;
    }

    if ((GSmodelIsRootNullAdded(resource) & 0xFF) != 0) {
        node = (u8*)((HSD_JObj*)node)->child;
    }

    index = GSpartGetJObjIndex(node, arg6);
    if (index == 0xFFFFFFFF) {
        return;
    }

    entry = GSmodelGetPart(resource, index);
    if (entry == NULL) {
        return;
    }
    entryBytes = (u8*)entry;

#ifdef TARGET_PC
    {
        /* PC (observe only, 2026-11-02): marks a model animation's particle draws for the replay's per-draw
         * resync (ORRE_ROLL_SEEDS, src/game/pc_debug_game.c), which leaves them out of the battle list: they run on
         * whichever thread animates the model (port-gaps 49, P24: once on the console's battle thread) */
        extern s32 pc_anim_particle_depth;
        pc_anim_particle_depth++;
        node = gsParticleCreate(texture, arg5, fn_800E3CBC(resource));
        pc_anim_particle_depth--;
        if (&pc_waza_viewer_ptclhook_log != 0 && pc_waza_viewer_ptclhook_log != 0) {
            /* ORRE_WAZA_VIEWER_PTCLHOOK (above): read-only; NULL generator = none made (bank off or full) */
            pc_waza_viewer_ptclhook_log(resource, arg2, arg5, index, arg6,
                                        node != NULL ? (const void*)((FieldParticleNode*)node)->generator : NULL);
        }
    }
#else
    node = gsParticleCreate(texture, arg5, fn_800E3CBC(resource));
#endif
    if (node != NULL) {
        handle = GSmodelGetGSparticleLinkAttachMode(resource);
        if (((FieldParticleNode*)node)->transform_state == 0 && handle != 0) {
#ifdef TARGET_PC /* the GSpart's model (+0x04) and index (+0x02) by field: the model pointer is 8 bytes on the PC */
            ((FieldParticleNode*)node)->field_48 = PC_PTR32(entry->model);
            ((FieldParticleNode*)node)->field_4C = entry->index;
#else
            ((FieldParticleNode*)node)->field_48 = *(u32*)(entryBytes + 0x04);
            ((FieldParticleNode*)node)->field_4C = *(u16*)(entryBytes + 0x02);
#endif
            set__5GSvecFfff(((FieldParticleNode*)node)->transformed_position, lbl_8047CFE8, lbl_8047CFE8, lbl_8047CFE8);
            set__5GSvecFfff(((FieldParticleNode*)node)->transformed_rotation, lbl_8047CFE8, lbl_8047CFE8, lbl_8047CFE8);
            set__5GSvecFfff(((FieldParticleNode*)node)->transformed_scale, lbl_8047CFEC, lbl_8047CFEC, lbl_8047CFEC);
            set__5GSvecFfff(((FieldParticleNode*)node)->local_position, lbl_8047CFE8, lbl_8047CFE8, lbl_8047CFE8);
            set__5GSvecFfff(((FieldParticleNode*)node)->local_rotation, lbl_8047CFE8, lbl_8047CFE8, lbl_8047CFE8);
            set__5GSvecFfff(((FieldParticleNode*)node)->local_scale, lbl_8047CFEC, lbl_8047CFEC, lbl_8047CFEC);
            ((FieldParticleNode*)node)->transform_state = handle;
            ((FieldParticleNode*)node)->field_05 = 0;
            ((FieldParticleNode*)node)->field_06 = 1;
        }
    }

    GSpartFree(entry);
    if ((GSmodelGetVisibility(resource) & 0xFF) == 0) {
        psSetParticleVisibility((PSGeneratorState*)((FieldParticleNode*)node)->generator, 0);
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_CANDIDATE_8011A0A8_8011A280)
/* 0x8011A0A8 | 0x1D8 */
static inline u8* fieldStatusEntryGet(u8* obj, u16 id) {
    extern u8 joutaiDataBiosGetUseBanme(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
    u16 idx;
    u8* base;
    idx = joutaiDataBiosGetUseBanme(id);
    base = PC_PTR(joutaiDataBiosGetObjectKind(id));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(id), 0));
    if (base == NULL) {
        return NULL;
    }
    return base + idx * 16;
}

static inline u8 fieldStatusEntryActive(u8* entry) {
    if (entry == NULL) {
        return 0;
    }
    if (joutaiBiosGetJoutaiDataId(entry) == 0) {
        return 0;
    }
    return 1;
}

static inline u8 fieldStatusEntryHas(u8* obj, u16 id) {
    u8* entry;
    if (id == 0) {
        return 0;
    }
    entry = fieldStatusEntryGet(obj, id);
    if (entry == NULL) {
        return 0;
    }
    if (fieldStatusEntryActive(entry) == 1) {
        if (joutaiBiosGetJoutaiDataId(entry) == id) {
            return 1;
        }
    }
    return 0;
}

/* NAME-T1 fn_8011A0A8 */ /* XD's map, joutai.o: the condition API, in our order; each calls the same joutaiBios accessor as its XD twin */
void joutaiCopy(u8* dst, u8* src, u16 type)
{
    u8* dst_entry;
    u8* src_entry;

    if (type == 0) {
        return;
    }
    dst_entry = fieldStatusEntryGet(dst, type);
    if (dst_entry == NULL) {
        return;
    }
    if (fieldStatusEntryHas(src, type) == 0) {
        return;
    }
    src_entry = fieldStatusEntryGet(src, type);
    if (src_entry != NULL) {
        *(FieldStatusEntry*)dst_entry = *(FieldStatusEntry*)src_entry;
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_8011A280_8011B2C0)
/* 0x8011A280 | 0x164 */
/* NAME-T1 fn_8011A280 */
void joutaiSetBuff(u8* arg1, u16 arg2, u32 arg3) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, u8* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
    extern void joutaiBiosSetBuff(u8* ptr, u32 val);
    u8* base;
    u16 idx;
    u8 flag;

    if (arg2 == 0) { goto exit; }
    if (arg2 != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(arg2)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == arg2) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { goto exit; }

    /* Phase 2 */
    idx = joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR(joutaiDataBiosGetObjectKind(arg2));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) {
        joutaiBiosSetBuff(base, arg3);
    }

exit:
    return;
}
/* 0x8011A3E4 | 0x18C */
/* NAME-T1 fn_8011A3E4 */
s32 joutaiGetBuff(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 joutaiBiosGetBuff(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return 0; }
    if (val != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { return 0; }

    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR(joutaiDataBiosGetObjectKind(val));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base == NULL) { return 0; }
    return joutaiBiosGetBuff(base);
}
/* 0x8011A570 | 0x164 */
/* NAME-T1 fn_8011A570 */
void joutaiSetIdBuff(u8* arg1, u16 arg2, u32 arg3) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, u8* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetIdBuff(u8* ptr, u32 val);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (arg2 == 0) { goto exit; }
    if (arg2 != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(arg2)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == arg2) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { goto exit; }

    /* Phase 2 */
    idx = joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR(joutaiDataBiosGetObjectKind(arg2));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) {
        joutaiBiosSetIdBuff(base, arg3);
    }

exit:
    return;
}
/* 0x8011A6D4 | 0x18C */
/* NAME-T1 fn_8011A6D4 */
s32 joutaiGetIdBuff(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 joutaiBiosGetIdBuff(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return 0; }
    if (val != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { return 0; }

    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR(joutaiDataBiosGetObjectKind(val));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) { goto p2_return; }
    return 0;
p2_return:
    return joutaiBiosGetIdBuff(base);
}
/* 0x8011A860 | 0x18C */
/* NAME-T1 fn_8011A860 */
s32 joutaiGetUserFightTargetId(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 joutaiBiosGetUserFightTargetId(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return 0; }
    if (val != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { return 0; }

    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR(joutaiDataBiosGetObjectKind(val));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) { goto p2_return; }
    return 0;
p2_return:
    return joutaiBiosGetUserFightTargetId(base);
}
/* 0x8011A9EC | 0x164 */
/* NAME-T1 fn_8011A9EC */
void joutaiSetKaisuuJoutaiDataId(u8* arg1, u16 arg2, u32 arg3) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, u8* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FD0(u8* ptr, u32 val);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (arg2 == 0) { goto exit; }
    if (arg2 != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(arg2)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == arg2) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { goto exit; }

    /* Phase 2 */
    idx = joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR(joutaiDataBiosGetObjectKind(arg2));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) {
        fn_80119FD0(base, arg3);
    }

exit:
    return;
}
/* 0x8011AB50 | 0x164 */
/* NAME-T1 fn_8011AB50 */
void joutaiSetNowKaisuuJoutaiDataId(u8* arg1, u16 arg2, u32 arg3) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, u8* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetNowKaisuu(u8* ptr, u32 val);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (arg2 == 0) { goto exit; }
    if (arg2 != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(arg2)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == arg2) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { goto exit; }

    /* Phase 2 */
    idx = joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR(joutaiDataBiosGetObjectKind(arg2));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) {
        joutaiBiosSetNowKaisuu(base, arg3);
    }

exit:
    return;
}
/* 0x8011ACB4 | 0x18C */
/* NAME-T1 fn_8011ACB4 */
s32 joutaiGetNowKaisuuJoutaiDataId(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 joutaiBiosGetNowKaisuu(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return -1; }
    if (val != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { return -1; }

    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR(joutaiDataBiosGetObjectKind(val));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) { goto p2_return; }
    return -1;
p2_return:
    return joutaiBiosGetNowKaisuu(base);
}
/* 0x8011AE40 | 0x18C */
/* NAME-T1 fn_8011AE40 */
s32 joutaiGetKaisuuJoutaiDataId(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 joutaiBiosGetKaisuu(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return -1; }
    if (val != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { return -1; }

    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR(joutaiDataBiosGetObjectKind(val));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) { goto p2_return; }
    return -1;
p2_return:
    return joutaiBiosGetKaisuu(base);
}
/* 0x8011AFCC | 0x164 */
/* NAME-T1 fn_8011AFCC */
void joutaiSetCountAsJoutaiDataId(u8* arg1, u16 arg2, u32 arg3) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, u8* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FB0(u8* ptr, u8 val);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (arg2 == 0) { goto exit; }
    if (arg2 != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(arg2)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == arg2) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { goto exit; }

    /* Phase 2 */
    idx = joutaiDataBiosGetUseBanme(arg2);
    base = PC_PTR(joutaiDataBiosGetObjectKind(arg2));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(arg1), 0, joutaiDataBiosGetStatus(arg2), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base != NULL) {
        fn_80119FB0(base, arg3);
    }

exit:
    return;
}
/* 0x8011B130 | 0x190 */
/* NAME-T1 fn_8011B130 */
s32 joutaiGetCountAsJoutaiDataId(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiBiosGetCount(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return -1; }
    if (val != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { return -1; }

    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR(joutaiDataBiosGetObjectKind(val));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto p2_offset; }
    base = NULL;
    goto p2_check;
p2_offset:
    base = base + idx * 16;
p2_check:
    if (base == NULL) { return -1; }
    return (u8)joutaiBiosGetCount(base);
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_CANDIDATE_8011B2C0_8011B444)
/* 0x8011B2C0 | 0x184 */
/* Status-entry lookup: the entry table from joutaiDataBiosGetObjectKind/statusGetStatus,
 * indexed by joutaiDataBiosGetUseBanme in 16-byte records. joutaiCheckWriteJoutaiDataId expands the same
 * sequence twice in place; as an inline, the table result feeds the offset
 * without the extra register copy a flat local produces. The block-scope
 * prototypes follow the widths retail's callers assume (no masks after
 * joutaiDataBiosGetObjectKind/joutaiDataBiosGetStatus/joutaiDataBiosGetMaxCount). */
static inline u8* fieldEntryGet(void* obj, u16 id) {
    extern u8 joutaiDataBiosGetUseBanme(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
    u16 idx;
    u8* base;
    idx = joutaiDataBiosGetUseBanme(id);
    base = PC_PTR(joutaiDataBiosGetObjectKind(id));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(id), 0));
    if (base == NULL) {
        return NULL;
    }
    return base + idx * 16;
}
/* NAME-T1 fn_8011B2C0 */
void joutaiWriteJoutaiDataId(void* obj, u16 id, u16 arg3) {
    extern u8 joutaiDataBiosGetKaisuuDataId(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetMaxCount(u16 idx);
#endif
    extern u8 joutaiDataBiosGetWriteMode(u16 idx);
    extern u8 joutaiDataBiosGetUseBanme(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetIdBuff(u8* ptr, u16 val);
#endif
    extern void joutaiBiosSetBuff(u8* ptr, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FB0(u8* ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetNowKaisuu(u8* ptr, u8 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FD0(u8* ptr, s8 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FE0(u8* ptr, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FF0(u8* ptr, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiBiosGetCount(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
    u8 type;
    u32 old_count;
    u8* elem;

    if (id == 0) { return; }

    elem = fieldEntryGet(obj, id);
    if (elem == NULL) { return; }

    type = joutaiDataBiosGetWriteMode(id);
    if (type == 4) {
        old_count = joutaiBiosGetCount(elem);
    } else {
        old_count = 0;
    }
    if (elem != NULL) {
        fn_80119FF0(elem, 0);
        fn_80119FE0(elem, 0);
        fn_80119FD0(elem, 0);
        joutaiBiosSetNowKaisuu(elem, 0);
        fn_80119FB0(elem, 0);
        joutaiBiosSetBuff(elem, 0);
        joutaiBiosSetIdBuff(elem, 0);
    }
    fn_80119FF0(elem, id);
    fn_80119FE0(elem, arg3);
    fn_80119FD0(elem, (s8)kaisuuGetKaisuu(joutaiDataBiosGetKaisuuDataId(id)));
    if (type == 4) {
        u32 count = old_count + 1;
        u32 limit = joutaiDataBiosGetMaxCount(id);
        if ((u8)limit < (u8)count) {
            count = limit;
        }
        fn_80119FB0(elem, count);
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_8011B444_8011BA0C)
/* 0x8011B444 | 0x238 */
/* NAME-T1 fn_8011B444 */
s32 joutaiCheckWriteJoutaiDataId(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
    extern u8 joutaiDataBiosGetWriteMode(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetSameJoutai(u32 val);
#endif
    u8* base;
    u16 idx;
    u32 entry_val;
    s32 kind;
    u8 flag;

    if (val == 0) { return 1; }
    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) { return 0; }

    entry_val = joutaiBiosGetJoutaiDataId(base);
    kind = (u8)joutaiDataBiosGetWriteMode(val);

    if (kind == 2) { goto case2; }
    if (kind >= 2) { goto ge2; }
    if (kind == 0) { goto case0; }
    if (kind >= 0) { goto case1; }
    goto default_case;
ge2:
    if (kind == 4) { goto case4; }
    if (kind >= 4) { goto default_case; }
    goto case3;

case0:
    return 2;
case1:
    return 2;
case2:
    if (base != NULL) { goto case2_check; }
    flag = 0; goto case2_done;
case2_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto case2_done; }
    flag = 1;
case2_done:
    {
        u32 f = (u8)flag;
        u32 lz = __cntlzw(f);
        return (s32)(lz >> 5) + 1;
    }

case3:
    if (base != NULL) { goto case3_check; }
    flag = 0; goto case3_flag_done;
case3_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto case3_flag_done; }
    flag = 1;
case3_flag_done:
    if ((u8)flag == 0) { return 2; }
    if ((u16)entry_val == val) { goto case3_return1; }
    if ((u16)joutaiDataBiosGetSameJoutai(entry_val) == val) { goto case3_return1; }
    return 2;
case3_return1:
    return 1;

case4:
    if (base != NULL) { goto case4_check; }
    flag = 0; goto case4_flag_done;
case4_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto case4_flag_done; }
    flag = 1;
case4_flag_done:
    if ((u8)flag == 0) { return 2; }
    if ((u16)entry_val == val) { goto case4_return2; }
    if ((u16)joutaiDataBiosGetSameJoutai(entry_val) != val) { goto case4_return1; }
case4_return2:
    return 2;
case4_return1:
    return 1;
default_case:
    return 0;
}
/* 0x8011B67C | 0x10C */
/* NAME-T1 fn_8011B67C */
s32 joutaiIsJoutaiDataId(void* obj, u16 val) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (val == 0) { return 0; }
    idx = joutaiDataBiosGetUseBanme(val);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(val)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(val), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) { return 0; }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto flag_done;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto flag_done; }
    flag = 1;
flag_done:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == val) {
            return 1;
        }
    }
    return 0;
}
/* 0x8011B788 | 0x1C8 */
/* NAME-T1 fn_8011B788 */
void joutaiInitJoutaiDataId(u8* obj, u16 id) {
    extern u8 joutaiDataBiosGetUseBanme(u16 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* joutaiDataBiosGetObjectKind(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetStatus(u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* statusGetStatus(u8* a, void* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiBiosGetJoutaiDataId(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FF0(u8* ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FE0(u8* ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FD0(u8* ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetNowKaisuu(u8* ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FB0(u8* ptr, u32 val);
#endif
    extern void joutaiBiosSetBuff(u8* ptr, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetIdBuff(u8* ptr, u32 val);
#endif
    u8* base;
    u16 idx;
    u8 flag;

    if (id == 0) { goto exit; }
    if (id != 0) { goto body_start; }
    flag = 0; goto flag_check;
body_start:
    idx = (u8)joutaiDataBiosGetUseBanme(id);
    base = PC_PTR((0, joutaiDataBiosGetObjectKind(id)));
    base = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(id), 0));
    if (base != NULL) { goto offset_calc; }
    base = NULL;
    goto check_base;
offset_calc:
    base = base + idx * 16;
check_base:
    if (base == NULL) {
        flag = 0;
        goto flag_check;
    }
    if (base != NULL) { goto a090_check; }
    flag = 0; goto inner_flag_check;
a090_check:
    if ((u16)joutaiBiosGetJoutaiDataId(base) == 0) { flag = 0; goto inner_flag_check; }
    flag = 1;
inner_flag_check:
    if ((u8)flag == 1) {
        if ((u16)joutaiBiosGetJoutaiDataId(base) == id) {
            flag = 1;
            goto flag_check;
        }
    }
    flag = 0;
flag_check:
    if ((u8)flag == 0) { goto exit; }

    idx = joutaiDataBiosGetUseBanme(id);
    base = PC_PTR(joutaiDataBiosGetObjectKind(id));
    obj = PC_PTR(statusGetStatus(PC_PTR32_IMPLICIT(base), PC_PTR32_IMPLICIT(obj), 0, joutaiDataBiosGetStatus(id), 0));
    if (obj != NULL) { goto p2_offset; }
    obj = NULL;
    goto p2_check;
p2_offset:
    obj = obj + idx * 16;
p2_check:
    if (obj == NULL) { goto exit; }
    if (obj == NULL) { goto exit; }
    fn_80119FF0(obj, 0);
    fn_80119FE0(obj, 0);
    fn_80119FD0(obj, 0);
    joutaiBiosSetNowKaisuu(obj, 0);
    fn_80119FB0(obj, 0);
    joutaiBiosSetBuff(obj, 0);
    joutaiBiosSetIdBuff(obj, 0);

exit:
    return;
}
/* 0x8011B950 | 0xBC */
/* NAME-T1 fn_8011B950 */
void joutaiInitAry(u8* base, u16 count) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FF0(u8* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FE0(u8* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FD0(u8* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetNowKaisuu(u8* a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80119FB0(u8* a, u32 b);
#endif
    extern void joutaiBiosSetBuff(u8* a, u32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiBiosSetIdBuff(u8* a, u32 b);
#endif
    u8* elem;
    u16 i;
    for (i = 0; i < count; i++) {
        elem = base + ((u16)i << 4);
        if (elem != NULL) {
            fn_80119FF0(elem, 0);
            fn_80119FE0(elem, 0);
            fn_80119FD0(elem, 0);
            joutaiBiosSetNowKaisuu(elem, 0);
            fn_80119FB0(elem, 0);
            joutaiBiosSetBuff(elem, 0);
            joutaiBiosSetIdBuff(elem, 0);
        }
    }
}
#endif

#if !defined(FIELD_BANK_ACTIVE) || defined(FIELD_EXACT_80119D90_8011A0A8)
extern u32 lbl_8047AD68;
extern u32 lbl_8047AD6C;
extern f32 lbl_8047CFD0;
extern f32 lbl_8047CFDC;
extern f32 lbl_8047CFE0;
u8 floorUpdateFieldCamera(void);
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* NAME-T1 fn_80119D90 */ /* XD's map, joutaiDataBios: the condition-table getters, same order and fields as ours */
u8 joutaiDataBiosGetKaisuuDataId(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return entry[0x4];
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* NAME-T1 fn_80119DD0 */
u8 joutaiDataBiosGetMaxCount(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return entry[0x3];
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* NAME-T1 fn_80119E10 */
u16 joutaiDataBiosGetSameJoutai(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return DISC_U16(entry + 0xa); /* the condition table stays big-endian on the PC (pcdata: an unsure byte array) */
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
u8 joutaiDataBiosGetWriteMode(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return entry[0x2];
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* NAME-T1 fn_80119E90 */
u8 joutaiDataBiosGetUseBanme(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return entry[0x1];
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* NAME-T1 fn_80119ED0 */
u16 joutaiDataBiosGetStatus(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    /* big-endian on the PC, like the rest of this table. 2026-10-19 (verified): read natively it gave 0x2A00 for
     * kind 0x2A, so the battle's move-result flags (super effective 0x41, not very effective 0x42, no effect 0x43) were
     * never recorded: no "super effective" messages, and an immune target still took the hit's side effects */
    return DISC_U16(entry + 0x8);
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* XD's map calls this getter "GetStatusKind" (field +0x00); it keeps ObjectKind because that spelling was this
 * family's old name for the +0x08 getter (now joutaiDataBiosGetStatus), and a retired name is never reused */
u8 joutaiDataBiosGetObjectKind(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return entry[0x0];
}
extern u32 lbl_80478B48;  /* NPC count (SDA) */
/* NAME-T1 fn_80119F50 */
u32 joutaiDataBiosGetName(u16 idx) {
    u8* entry;
    if ((u32)idx >= lbl_80478B48) { entry = NULL; } else { entry = lbl_8035BBA8 + (u32)idx * 0x14; }
    if (entry == NULL) { return 0; }
    return DISC_U32(entry + 0x10); /* a message id, big-endian on the PC (no relocation in this table) */
}
/* Address: 0x80119F90 | Size: 0x10 | Pattern: nullcheck_setter */
/* NAME-T1 fn_80119F90 */ /* XD's map, joutaiBios.o: the status-condition record's accessors (setters, then getters) */
void joutaiBiosSetIdBuff(u8* ptr, u16 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->half_c = val;
}
/* Address: 0x80119FA0 | Size: 0x10 | Pattern: nullcheck_setter */
/* NAME-T1 fn_80119FA0 */
void joutaiBiosSetBuff(u8* ptr, u32 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->value = val;
}
/* Address: 0x80119FB0 | Size: 0x10 | Pattern: nullcheck_setter */
void fn_80119FB0(u8* ptr, u8 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->count = val;
}
/* Address: 0x80119FC0 | Size: 0x10 | Pattern: nullcheck_setter */
/* NAME-T1 fn_80119FC0 */
void joutaiBiosSetNowKaisuu(u8* ptr, u8 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->byte5 = val;
}
/* Address: 0x80119FD0 | Size: 0x10 | Pattern: nullcheck_setter */
void fn_80119FD0(u8* ptr, u8 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->byte4 = val;
}
/* Address: 0x80119FE0 | Size: 0x10 | Pattern: nullcheck_setter */
void fn_80119FE0(u8* ptr, u16 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->sub_id = val;
}
/* Address: 0x80119FF0 | Size: 0x10 | Pattern: nullcheck_setter */
void fn_80119FF0(u8* ptr, u16 val) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return; }
    entry->id = val;
}
/* Address: 0x8011A000 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A000 */ /* was joutaiSlotGet0C (renamed with snatcherLeo's OK) */
u16 joutaiBiosGetIdBuff(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->half_c;
}
/* Address: 0x8011A018 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A018 */ /* was joutaiSlotGet08 */
u32 joutaiBiosGetBuff(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->value;
}
/* Address: 0x8011A030 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A030 */
u8 joutaiBiosGetCount(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->count;
}
/* Address: 0x8011A048 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A048 */ /* was joutaiSlotGet05 */
u8 joutaiBiosGetNowKaisuu(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->byte5;
}
/* Address: 0x8011A060 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A060 */
u8 joutaiBiosGetKaisuu(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->byte4;
}
/* Address: 0x8011A078 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A078 */
u16 joutaiBiosGetUserFightTargetId(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->sub_id;
}
/* Address: 0x8011A090 | Size: 0x18 | Pattern: nullcheck_getter */
/* NAME-T1 fn_8011A090 */
u16 joutaiBiosGetJoutaiDataId(u8* ptr) {
    FieldStatusEntry* entry = (FieldStatusEntry*)ptr;
    if (entry == NULL) { return 0; }
    return entry->id;
}
#endif
