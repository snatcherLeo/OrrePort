/**
 * @file field_range_80114AE0.c
 * @brief field code, 0x80114AE0 - 0x80115280 (merged: original range-file +
 *        the floorRead / _unload tail segment split from gs_field_world.c).
 *
 * Range unit assigned from the propagated subsystem map
 * (tools/subsystem_propagation.py, >=80% single-label dominance;
 * campaign 2026-07-01). The 0x80114D6C-0x80115280 tail is the floorRead*
 * resource-handler module (floorReadGFL/Particle/WZX/PKX/Tex/Script/Font/
 * Msg/Normal PreFunc + matching _unload*__FPvUlUl family), physically split
 * from gs_field_world.c (XD 0x8002E524-0x8002F120). All functions asm-only
 * until matched; the range name stays honest until internal TU structure
 * is fully proven.
 */
#include "dolphin/types.h"
#include "game/world/gs_field.h"
#include "game/world/floor_archive.h" /* FLOOR_ARCHIVE_HEADER */
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
/* Real named labels referenced by remaining inline-asm wrappers in this TU. */
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
extern void psSetParticleVisibility();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void sin();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void statusGetStatus();
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
void gsParticleKillAll(void);
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
extern u32 _unloadScript__FPvUlUl(); /* K&R: asm void wrapper, used as function pointer */
extern u32 _unloadFont__FPvUlUl(); /* K&R: asm void wrapper, used as function pointer */
extern u32 _unloadMsg__FPvUlUl(); /* K&R: asm void wrapper, used as function pointer */
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
extern void gsParticleKillOldest(void);
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
extern s32 psGetGeneratorChildMaxLife(u32);
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
extern void GSmodelGetPart(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSpartGetTransform(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSpartFree();
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
extern void set__5GSvecFfff(void* obj, f32 f1, f32 f2, f32 f3);
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
extern void fn_80177A38(void); /* referenced by asm .inc wrappers (fn_801171C8/80117330/8011791C/8012E388/8012EBD4); was undefined -> broke the TU parse */
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
extern void psGetParticleChildCount(void* ptr);
extern void psKillFamily();
extern void GSmodelSet60fpsAnimFlag();
extern void psUnlinkChildGensFromJObj();
extern void psKillGenerator();
extern u32 lbl_8047AD9C;
extern u32 lbl_8047ADA0;
void gsParticleUpdateAll(void);
extern void GSquatFromEuler(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecTransformQuat(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E0108(void);
#endif
extern void psInterpretParticles(void);
extern void psExecGenerator(void);
extern void GSgetVersionMajor(void);
extern void jumptable_8035BB88();
extern u8 lbl_8047ADB0;
void gsParticleUpdate(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void gsParticleKill(void);
#endif
extern void psSetParticleVisibility(); /* K&R: called with 0 or 1 args */
extern void psSetGeneratorAngleRadiusScale(void);
extern void psLinkChildGensToJObj(void);
extern f32 lbl_8047CFE8;
extern f32 lbl_8047CFEC;
extern void psCreateGeneratorID(void);
extern void fn_800D3094(void);
extern u32 lbl_8047ADAC;
extern u32 lbl_8047ADA8;
void gsParticleCreate(void);
extern void psInitDataBank(void);
extern void DCFlushRange();
extern u8 lbl_802727D8[];
void gsParticleBankLoad(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8016A01C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void psInitGenerator(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8016AAF4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void HSD_JObjSetSPtclCallback(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void HSD_JObjSetPtclCallback(void);
#endif
extern void psSetPointJObjNodup(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void HSD_JObjSetDPtclCallback(void);
#endif
extern u16 lbl_8047AD98;
extern u16 lbl_8047ADA4;
extern void GSmodelSearchModelList(void);
extern void GSmodelGetLinkedGSparticleBank(void);
extern void GSmodelIsRootNullAdded(void);
extern void GSpartGetJObjIndex(void);
extern void fn_800E3CBC(void);
extern void GSmodelGetGSparticleLinkAttachMode(void);
extern void GSmodelGetVisibility(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80135E44(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void joutaiCopy(void);
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
extern void GSmodelSetParticleVisibility(u8* arg1, void* arg2, u32 arg3, u32 arg4, u32 arg5);
extern void fn_80118C88(void* obj);
extern void fn_80118CAC(void* obj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118CD0(void* obj);
#endif
extern void fn_80118CF4(void* obj);
extern void fn_80118D18(void* obj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118D3C(void* obj);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118D60(void* obj);
#endif
extern void fn_80118D84(void* obj);
extern s32 fn_80118DA8(u8* ptr);
extern void fn_80118DE0(u8* arg1, f32* arg2, u32 arg3, u32 arg4);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118E8C(u8* arg1, f32* arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gsParticleSetVecB(u8* arg1, f32* arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80118F7C(u8* obj, void* arg);
#endif
extern void gsParticleLinkToJObj(u8* obj, u8* desc, u32 state, u32 byte5, u32 init_from_zero, u32 attach_model);
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
extern void joutaiInitJoutaiDataId(void* obj, u16 id);
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

/* ===================================================================
 * Generated: 1 pattern-matched + 515 stubs
 * Range: 0x80114CA8 - 0x80130CD8
 * =================================================================== */
extern u8 lbl_804083D0[0x30];
extern u32 lbl_8047AD68;
extern u32 lbl_8047AD6C;
/* Forward declarations for functions called before definition */
/* Forward declarations for converted functions */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 pokemonWazaGetMaxPP(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonBiosGetPokemonWazaPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonResetBasisStatus(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void heroItemGetItemKindToItemAryPtr(void);
#endif
void heroSetStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void heroGetStatus(void);
#endif

#include "pc/disc_types.h"
/* The map archive's "scene_data": disc data, big-endian with 4-byte pointer slots on the PC (A, 2026-10-15; DISC_PTR is a
   plain pointer on the GameCube). The model and light lists are NULL-terminated slot arrays. */
typedef DISC_PTR(void) FloorMapSlot;
typedef struct DISC_STRUCT FloorMapPublic {
    DISC_PTR(FloorMapSlot) models;
    DISC_PTR(void) camera;
    DISC_PTR(FloorMapSlot) lights;
    DISC_PTR(FloorMapSlot) extra;
} FloorMapPublic;

extern void HSD_ArchiveParse(void* archive, void* buffer, u32 size);
extern void* HSD_ArchiveGetPublicAddress(void* archive, const char* name);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresRegisterResource(void* resource, u32 owner, u32 id,
                                   void* unload);
#endif
extern void* GSlightLoad(void* light);
extern void* GScameraCreateFromDesc(void* camera);
extern void floorInitMap(u32 owner, u32 param);
extern u8 lbl_80272200[];

/*
 * floorReadMapPostFunc - 0x80114AE0 | Size: 0x1C8
 * Parse a map archive and register its models, lights, camera (the inlined
 * _registerCamera, whose two log strings sit at +0x1CC/+0x200), and auxiliary
 * resource with the floor resource manager.
 *
 * Register allocation notes (retail r24-r31): the model and light lists are
 * walked with byte offsets (no strength-reduced temporaries), the light
 * group, camera id, id mask and extra pointer are their own variables, and
 * `public` holds the archive, then its scene_data block. The declaration
 * order was solved with the colouring simulator.
 */
void* floorReadMapPostFunc(u32 owner, u32 param, u32 size)
{
    FloorMapPublic* public;
    void* resource;
    u32 lightGroup;
    u32 cameraId;
    u32 modelOffset;
    u32 modelGroup;
    u32 index;
    FloorMapSlot* extra;
    u32 lightOffset;
    u32 mask;
    u32 lightIndex;
    /* RULE-EXCEPTION(title-path): base pointer into the TU's .rodata strings
     * (lbl_80272200, a data object shared with the other floorRead units)
     * standing in for the string literals - see docs/RULE_EXCEPTIONS.md */
    const char* strings = (const char*) lbl_80272200;

    index = 0;
    lightIndex = 0;
    public = GSresGetResource(owner, param);
    HSD_ArchiveParse(public, (u8*) public + FLOOR_ARCHIVE_HEADER, size);
    public = HSD_ArchiveGetPublicAddress(public, FLOOR_READ_STRING(strings, 0xAC));
    if (public == NULL) {
        return NULL;
    }

    floorDataBiosSetMapResID(floorDataBiosGetCurrentPtr(), param);
    if (DP(public->models) != NULL) {
        modelOffset = 0;
        modelGroup = (param & 0x7FFF0000) | 0x1000;
        while ((resource = DP(*(FloorMapSlot*) ((u8*) DP(public->models) + modelOffset))) != NULL) {
            GSresRegisterResource(resource, owner, modelGroup | index, NULL);
            modelOffset += 4;
            index++;
        }
    }

    if (DP(public->lights) != NULL) {
        lightOffset = 0;
        lightGroup = (param & 0x7FFF0000) | 0x1600;
        while ((resource = DP(*(FloorMapSlot*) ((u8*) DP(public->lights) + lightOffset))) != NULL) {
            resource = GSlightLoad(resource);
            if (resource == NULL) {
                GSlogWrite(FLOOR_READ_STRING(strings, 0x298), lightIndex);
                GSlogWrite(FLOOR_READ_STRING(strings, 0x2D0));
            } else {
                GSresRegisterResource(resource, owner, lightGroup | lightIndex,
                                      (void*) _unloadLight__FPvUlUl);
            }
            lightOffset += 4;
            lightIndex++;
        }
    }

    mask = param & 0x7FFF0000;
    cameraId = mask | 0x1800;
    if (public == NULL) {
        GSlogWrite(FLOOR_READ_STRING(strings, 0x1CC));
    } else {
        resource = GScameraCreateFromDesc(DP(public->camera));
        if (resource == NULL) {
            GSlogWrite(FLOOR_READ_STRING(strings, 0x200));
        } else {
            GSresRegisterResource(resource, owner, cameraId,
                                  (void*) _unloadCamera__FPvUlUl);
        }
    }

    extra = DP(public->extra);
    if (extra != NULL && DP(*extra) != NULL) {
        GSresRegisterResource(DP(*extra), owner, mask | 0x1A00, NULL);
    }
    floorInitMap(owner, param);
    return public;
}
