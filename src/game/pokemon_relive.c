/**
 * @file pokemon_relive.c
 * @brief game/pxdvs/app/pokemon/pokemonRelive.cpp (possibly + pokemonWazaSequenceDisplay.cpp tail) -- split from colosseum_battle.c (the
 *        Colosseum battle-flow/AI bucket, 0x802405C0-0x80265EC4),
 *        address range 0x8025DC2C-0x8025F2FC, 19 fns.
 *
 * XD source unit: game/pxdvs/app/pokemon/pokemonRelive.cpp (possibly + pokemonWazaSequenceDisplay.cpp tail)
 * Physically split out of the pre/post-battle mega-file by address
 * (functions located and bucketed by name via config/GC6E01/symbols.txt,
 * since this TU uses plain named C bodies with no address-comment
 * markers).
 */

#include "game/colosseum.h"
#include "game/trainer.h"
#include "game/pokemon.h"
#include "game/constants/pokemon_status.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_RelivePickSlot (step 3.7, PURIFY in the party menu) */
#endif

/* =========================================================================
 * Duplicated declarations (verbatim from the original colosseum_battle.c
 * preamble, present in every split segment so each TU keeps the same
 * external visibility it had before the split)
 * ========================================================================= */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   pokemonSetStatus();
#endif

/* Battle system functions */
extern void battleCameraSelect();

/* Sound functions */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStop();     /* Stop sound */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std104_soundSetBackgroundMusic();     /* Fade out music */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std105_soundPlayEffect();     /* Start BGM */
#endif

/* SDA2 float constants used by asm wrappers */
extern f32 lbl_8047E678;
extern f32 lbl_8047E67C;

/* SDA1 globals used by asm wrappers */
#if !defined(POKEMON_RELIVE_MAIN_ONLY)
/* reliveMain's unit declares this and lbl_804782BC as their real struct types. */
extern u32 lbl_8047B668;
#endif
extern u32 lbl_8047B66C;
extern u32 lbl_8047B670;

/* Data labels used by asm wrappers */
extern u8  lbl_8039A6B8[];
extern u8  lbl_8039A6A8[];
#if !defined(POKEMON_RELIVE_MAIN_ONLY)
extern int lbl_804782BC[];
#endif
extern u8  lbl_804782E0[];
extern u8  lbl_804783E0[];

/* Forward declarations for functions used as addresses in asm wrappers */
void ShortCommandProc(int r3);
void ReadProc(int r3);
void WriteProc(int r3);
void __GBASyncCallback(int r3);
u32  __GBASync(int r3);
u32  __GBATransfer(int r3, u32 r4, u32 r5, u32 r6);

/* Forward declarations for asm wrapper bl targets (use () form for compat) */
extern void DSPInit();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void set__5GSvecFfff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  _fadeEffectGetRandom__FUl();
#endif
extern u32  pokemonBiosGetCatchTrainerRnd();
extern u32  pokemonBiosGetRnd();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  pokemonBiosGetPokemonDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  savedataGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fadeCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fadeSet();
#endif
extern int  wazaSequenceSysRelease();
extern int  wazaSequenceSysInitWithoutParticles();
extern void OSRegisterResetFunction();
extern void OSInitAlarm();
extern void OSInitThreadQueue();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy();
#endif

/* Forward declarations for converted functions */
u32 evolutionWazaLearn();
int fightTrainerAiWazaValueKuroikiri(void* ctx, u32 param1, u32 param2, u32 param3);
void fightTrainerAiWazaValueHimitunotikara(void* ctx, u32 param1, u32 param2, u32 param3);
s32 fightTrainerAiSelectIrekaeDasuFightPokemon(void* ctx, u32 param1, u32 param2, u32 param3);
u32 fightTrainerAiWazaHit045(void* trainerCtx, u32 trainerSlot, u32 resultSlot, u32 resultType);
u32 fightMenuFightTrainerGcHeroOpenMenu(void* ctx, u32 param1, u32 param2);
extern void reliveCeremonyAll(u32 pokemonIndex);

#if defined(POKEMON_RELIVE_PREFIX_ONLY)
/* Address: 0x8025DCBC | Size: 0x58 | Ghidra import */
void reliveRestoreSound(int *param)
{
    u32 *r3 = (u32 *)param;
    if (*r3 != 0) {
        std104_soundSetBackgroundMusic(*r3, 0x32, 0xff);
    }
    if (r3[1] != 0) {
        std105_soundPlayEffect(r3[1], 0x32, 0xff);
    }
}

/* Address: 0x8025DD14 | Size: 0x98 | Ghidra import */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma use_lmw_stmw off
void reliveSaveSound(int *r3)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int soundGetCurrentSubSound();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int std228_soundGetCurrentBackgroundMusic();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801656D8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic();
#endif
    int iVar1;
    int iVar2;
    int iVar3;
    int iVar4;

    iVar1 = std228_soundGetCurrentBackgroundMusic();
    if (iVar1 != 0) {
        iVar2 = fn_801656D8(iVar1);
        std104_soundSetBackgroundMusic(1, 0x32, 0xff);
    } else {
        iVar2 = 0;
    }
    iVar3 = soundGetCurrentSubSound();
    if (iVar3 != 0) {
        iVar4 = fn_801656D8(iVar3);
    } else {
        iVar4 = 0;
    }
    *r3 = iVar1;
    r3[1] = iVar3;
    r3[2] = iVar2;
    r3[3] = iVar4;
}
#pragma pop

/* Address: 0x8025DDAC | Size: 0x48 | Ghidra import */
void effectSetRotation(u32 *r3,u32 r4)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetRotation();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSequenceOwnerGetModel(u32);
#endif
  u32 iVar1;
  r3 = (u32*)PC_PTR(*r3);
  if (r3 == 0) return;
  iVar1 = wazaSequenceOwnerGetModel(PC_PTR32(r3));
  if (iVar1 == 0) return;
  GSmodelSetRotation(PC_PTR(iVar1),PC_PTR(r4));
}

/* Address: 0x8025DDF4 | Size: 0x18 | Ghidra import */
void effectClear(u32 *r3)

{
  if (*r3 == 0) {
    return;
  }
  *r3 = 0;
  return;
}

/* Address: 0x8025DE0C | Size: 0x48 | Ghidra import */
void effectSetPosition(u32 *r3,u32 r4)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetPosition();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSequenceOwnerGetModel(u32);
#endif
  u32 iVar1;
  r3 = (u32*)PC_PTR(*r3);
  if (r3 == 0) return;
  iVar1 = wazaSequenceOwnerGetModel(PC_PTR32(r3));
  if (iVar1 == 0) return;
  GSmodelSetPosition(PC_PTR(iVar1),PC_PTR(r4));
}

/* Address: 0x8025DE54 | Size: 0xE4 | Ghidra import */
void effectPlayWaza(u32 *r3,u16 *r4,int r5,int r6,int r7,
                 int r8)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int _threadSwitch();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fadeSet();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int wazaSequenceOwnerSetVisibility();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int wazaSequenceSysStopWaza();
#endif
    extern int wazaSequenceClearFlag16();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceSysIsPlayingWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int wazaSequenceSysStartWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int wazaSequenceSysUpdate();
#endif
    extern f32 lbl_8047E670;

  u16 *p;
  int iVar2;

  if (r7 == 1) {
    fadeSetDurationFirst((double)lbl_8047E670,2);
  }
  wazaSequenceOwnerSetVisibility(PC_PTR(*r3),1);
  p = r4;
  for (iVar2 = 0; iVar2 < r5; iVar2 = iVar2 + 1) {
    if (r6 == 0) {
      wazaSequenceClearFlag16(*r3,*p,p[1]);
    }
    wazaSequenceSysStartWaza(PC_PTR(*r3),*p,p[1]);
    while (wazaSequenceSysIsPlayingWaza(PC_PTR(*r3),*p,p[1]) != 0) {
      wazaSequenceSysUpdate();
      _threadSwitch();
    }
    if (r8 == 1) {
      wazaSequenceOwnerSetVisibility(PC_PTR(*r3),0);
    }
    wazaSequenceSysStopWaza(PC_PTR(*r3),*p,p[1]);
    p = p + 2;
  }
  return;
}

#endif

#if defined(POKEMON_RELIVE_PRE_SEQUENCE_ONLY)
/* Address: 0x8025DF38 | Size: 0x178
 * The body lives in a single-use inline helper whose parameters and locals
 * are numbered after the wrapper's, which gives retail's register order. */
/* RULE-EXCEPTION(user-approved): single-use inline helper carrying the whole body for register numbering - see docs/RULE_EXCEPTIONS.md */
static inline u32 impl_8025DF38(u32* r3, u32 r4, u16* r5, int r6)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fadeEffectGetRandom__FUl(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetBoundCheck(void* model, s32 on);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(void* model, u32 light);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(void* model, s32 count, u32* surfaces);
#endif
    extern void GSmodelClearShadowFlags(void* model, s32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(void* model, s32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std092_getCurrentRoomID(void);
#endif
    extern u32 floorGetResource(u32 floor, u32 id);
    extern u32 floorDataBiosGetShadowReciveNum(void* floor);
    extern u32 floorDataBiosGetShadowReciveID(void* floor, u32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorDataBiosGetCurrentPtr(void);
#endif
    extern u32 fn_8018F470(u32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceOwnerLoadWaza(u32 handle, u16 a, u16 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSequencePokemonCreate(u32 id, u32 random, s32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* wazaSequenceOwnerGetModel(u32 handle);
#endif
    u16* pair;
    u32 light;
    int k;
    u32 id;
    u32 i;
    void* floor;
    u32 count;
    s32 n;
    void* model;
    u32 handle;
    u32 surface;
    u32 surfaces[13];

    handle = PC_PTR32_IMPLICIT(wazaSequencePokemonCreate(r4, _fadeEffectGetRandom__FUl(0xFFFFFFFF), 0));
    *r3 = handle;
    if (handle == 0) {
        return 0;
    }
    pair = r5;
    for (k = 0; k < r6; k++) {
        if (wazaSequenceOwnerLoadWaza(PC_PTR(*r3), pair[0], pair[1], 0) == 0) {
            return 0;
        }
        pair += 2;
    }
    if (r3 != NULL && *r3 != 0 && (model = PC_PTR(wazaSequenceOwnerGetModel(PC_PTR(*r3)))) != NULL) {
        GSmodelClearShadowFlags(model, 1);
        floor = floorDataBiosGetCurrentPtr();
        if (floor != NULL) {
            light = fn_8018F470(1);
            count = floorDataBiosGetShadowReciveNum(floor);
            n = 0;
            if (count == 1) {
                for (i = 0; i < count; i++) {
                    id = floorDataBiosGetShadowReciveID(floor, i);
                    surface = floorGetResource(std092_getCurrentRoomID(), id);
                    if (surface != 0) {
                        surfaces[n] = surface;
                        n++;
                    }
                }
                GSmodelSetShadowFlags(model, 1);
                GSmodelSetShadowLight(model, light);
                GSmodelSetShadowSurface(model, n, surfaces);
                GSmodelSetBoundCheck(model, 1);
            }
        }
    }
    return 1;
}

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
u32 effectCreate(u32* r3, u32 r4, u16* r5, int r6)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fadeEffectGetRandom__FUl(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetBoundCheck(void* model, s32 on);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(void* model, u32 light);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(void* model, s32 count, u32* surfaces);
#endif
    extern void GSmodelClearShadowFlags(void* model, s32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(void* model, s32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std092_getCurrentRoomID(void);
#endif
    extern u32 floorGetResource(u32 floor, u32 id);
    extern u32 floorDataBiosGetShadowReciveNum(void* floor);
    extern u32 floorDataBiosGetShadowReciveID(void* floor, u32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorDataBiosGetCurrentPtr(void);
#endif
    extern u32 fn_8018F470(u32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceOwnerLoadWaza(u32 handle, u16 a, u16 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSequencePokemonCreate(u32 id, u32 random, s32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* wazaSequenceOwnerGetModel(u32 handle);
#endif
    s32 n;
    void* floor;
    u32 i;
    void* model;
    int k;
    u32 light;
    u32 surface;
    u32 surfaces[13];
    u32 count;
    u32 handle;
    u16* pair;

    handle = PC_PTR32_IMPLICIT(wazaSequencePokemonCreate(r4, _fadeEffectGetRandom__FUl(0xFFFFFFFF), 0));
    *r3 = handle;
    if (handle == 0) {
        return 0;
    }
    pair = r5;
    for (k = 0; k < r6; k++) {
        if (wazaSequenceOwnerLoadWaza(PC_PTR(*r3), pair[0], pair[1], 0) == 0) {
            return 0;
        }
        pair += 2;
    }
    if (r3 != NULL && *r3 != 0 && (model = PC_PTR(wazaSequenceOwnerGetModel(PC_PTR(*r3)))) != NULL) {
        GSmodelClearShadowFlags(model, 1);
        floor = floorDataBiosGetCurrentPtr();
        if (floor != NULL) {
            light = fn_8018F470(1);
            count = floorDataBiosGetShadowReciveNum(floor);
            n = 0;
            if (count == 1) {
                for (i = 0; i < count; i++) {
                    surface = floorGetResource(std092_getCurrentRoomID(), floorDataBiosGetShadowReciveID(floor, i));
                    if (surface != 0) {
                        surfaces[n] = surface;
                        n++;
                    }
                }
                GSmodelSetShadowFlags(model, 1);
                GSmodelSetShadowLight(model, light);
                GSmodelSetShadowSurface(model, n, surfaces);
                GSmodelSetBoundCheck(model, 1);
            }
        }
    }
    return 1;
}
#else
u32 effectCreate(u32* r3, u32 r4, u16* r5, int r6)
{
    return impl_8025DF38(r3, r4, r5, r6);
}
#endif
#endif

#if defined(POKEMON_RELIVE_SEQUENCE_EXACT_ONLY)
struct GSSceneVec3;
struct GSRenderCamera;
extern void* wazaSequencePokemonCreate(u16 index, void* model, u8 variant);
extern BOOL wazaSequenceOwnerLoadWaza(void* owner, u16 group, u16 index, u8 variant);
extern void GSscene_GetCameraDirectionVector(struct GSSceneVec3* direction);
extern void GSscene_GetCameraRotationVector(struct GSSceneVec3* rotation);
extern void GSscene_GetCameraPositionVector(struct GSSceneVec3* position);
extern void GSscene_GetCameraViewVector(struct GSSceneVec3* view);
extern struct GSRenderCamera* GScameraGetActiveCamera(void);
extern void GScameraGetPerspective(struct GSRenderCamera* camera, f32* fov, f32* aspect,
                                   f32* near, f32* far);
extern s32 menuPokemonStatusOpenWazaForget(u32 arg0, u16 arg1);
void effectPlayWaza(u32 *r3,u16 *r4,int r5,int r6,int r7,int r8);

/* Address: 0x8025E0B0 | Size: 0x10C | Ghidra import */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
u32 loadSequence(int *r3,u32 r4,u16 *r5,int r6)

{
    extern u32 pokemonBiosGetRnd();
  u32 uVar1;
  u32 uVar2;
  u16 sVar4;
  u16 sVar6;
  u32 iResult;
  int iVar3;
  u8 cVar5;

  sVar4 = (int)pokemonGetStatus(PC_PTR(r4),0,POKEMON_STATUS_SPECIES,0);
  if (sVar4 == 0) {
    uVar1 = 0;
  }
  else {
    sVar6 = (int)pokemonGetStatus(0,sVar4,POKEMON_STATUS_MODEL_ID,0);
    if (sVar6 == 0) {
      uVar1 = 0;
    }
    else {
      uVar1 = pokemonBiosGetRnd(r4);
      uVar2 = (int)pokemonGetStatus(PC_PTR(r4),0,POKEMON_STATUS_IS_SHINY,0);
      uVar2 = (-uVar2 | uVar2) >> 0x1f;
      iResult = PC_PTR32_IMPLICIT(wazaSequencePokemonCreate(sVar6,uVar1,uVar2));
      *r3 = iResult;
      if (iResult == 0) {
        uVar1 = 0;
      }
      else {
        u16 *p = r5;
        for (iVar3 = 0; iVar3 < r6; iVar3 = iVar3 + 1) {
          cVar5 = wazaSequenceOwnerLoadWaza(PC_PTR(*r3),*p,p[1],0);
          if (cVar5 == '\0') {
            return 0;
          }
          p = p + 2;
        }
        uVar1 = 1;
      }
    }
  }
  return uVar1;
}
#else
u32 loadSequence(int *r3,u32 r4,u16 *r5,int r6)

{
    extern u32 pokemonBiosGetRnd();
  u32 uVar1;
  u32 uVar2;
  u16 sVar4;
  u16 sVar6;
  u32 iResult;
  int iVar3;
  u8 cVar5;

  sVar4 = (int)pokemonGetStatus(PC_PTR(r4),0,POKEMON_STATUS_SPECIES,0);
  if (sVar4 == 0) {
    uVar1 = 0;
  }
  else {
    sVar6 = (int)pokemonGetStatus(0,sVar4,POKEMON_STATUS_MODEL_ID,0);
    if (sVar6 == 0) {
      uVar1 = 0;
    }
    else {
      uVar1 = pokemonBiosGetRnd(r4);
      uVar2 = (int)pokemonGetStatus(PC_PTR(r4),0,POKEMON_STATUS_IS_SHINY,0);
      uVar2 = (-uVar2 | uVar2) >> 0x1f;
      iResult = (u32)wazaSequencePokemonCreate(sVar6,(void*)uVar1,uVar2);
      *r3 = iResult;
      if (iResult == 0) {
        uVar1 = 0;
      }
      else {
        u16 *p = r5;
        for (iVar3 = 0; iVar3 < r6; iVar3 = iVar3 + 1) {
          cVar5 = wazaSequenceOwnerLoadWaza((void*)*r3,*p,p[1],0);
          if (cVar5 == '\0') {
            return 0;
          }
          p = p + 2;
        }
        uVar1 = 1;
      }
    }
  }
  return uVar1;
}
#endif

#if 0
asm void fn_8025E1BC(void) {
#include "src/game/colosseum_battle_fn_8025E1BC.inc"
}
#endif
static inline u32 *validptr_8025E1BC(u32 *p) {

  u32 *q = p;

  if (q != (u32 *)0 && *q != 0) return q;

  return (u32 *)0;

}

#pragma push
void std244_spawnEffectAtPosition(float f1, float f2, float f3, float f4, float f5, float f6) {
  extern u32 lbl_8047B658;
  extern u16 lbl_80478DA0;
  extern f32 lbl_8047E678;
  extern f32 lbl_8047E67C;
  extern void effectClear();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u8 effectCreate();
#endif
  extern void effectSetPosition();
  extern void effectSetRotation();
  extern void effectPlayWaza();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern void set__5GSvecFfff();
#endif
  u32 *r31;
  u32 *r30;
  float out_buf[3];
  float scale_buf[3];
  float sv1, sv2, sv3, sv4, sv5, sv6;
  float fscale;
  sv1 = f1; sv2 = f2; sv3 = f3;
  sv4 = f4; sv5 = f5; sv6 = f6;
  wazaSequenceSysInitWithoutParticles(1);
  effectClear(&lbl_8047B658);
  if ((u8)(effectCreate(&lbl_8047B658, 0x3f, &lbl_80478DA0, 1) == 1) == 1) {
    r31 = validptr_8025E1BC(&lbl_8047B658);
    r30 = validptr_8025E1BC(&lbl_8047B658);
    if (r30 != (u32 *)0 && *r30 != 0) {
      set__5GSvecFfff(out_buf, sv1, sv2, sv3); /* GSvec::set(this, x, y, z): the vector first (was written sv1 first; same registers on the GameCube) */
      effectSetPosition(r30, PC_PTR32(out_buf));
    }
    {
      u32 *r3;
      r3 = validptr_8025E1BC(&lbl_8047B658);
      if (r3 != (u32 *)0 && *r3 != 0) {
        fscale = lbl_8047E678;
        scale_buf[0] = fscale * sv4;
        scale_buf[1] = fscale * sv5;
        scale_buf[2] = fscale * sv6;
        effectSetRotation(r3, PC_PTR32(scale_buf));
      }
    }
    if (r31 != (u32 *)0 && *r31 != 0) {
      effectPlayWaza(r31, &lbl_80478DA0, 1, 0, 0, 1);
      fadeSet(5, lbl_8047E67C);
      fadeCheck(1);
    }
  }
  wazaSequenceSysRelease();
}
#pragma pop

#endif

#if defined(POKEMON_RELIVE_PARTICLE_ONLY)
/* Address: 0x8025E534 | Size: 0xC0 | Ghidra import */
void _preReliveDisplayHokoraParticleThreadFunc__Fv(void)
{
    extern u32 lbl_8027A478[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std093_getCurrentGroupID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int gsParticleKillOwned();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int gsParticleSetVecB();
#endif
    extern u32 gsParticleCreate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int waitTime__Ff();
#endif
    extern u32 lbl_8047B660;
    extern u32 lbl_8047B664;
    extern f32 lbl_8047E684;
    float fVar1;
    u32 uVar2;
    u32 state;
    u32 local_buf[3];

    local_buf[0] = lbl_8027A478[0];
    local_buf[1] = lbl_8027A478[1];
    local_buf[2] = lbl_8027A478[2];
    state = lbl_8047B664;
    if ((s32)state != 1) goto skip_cleanup;
    if ((s32)state != 1) goto skip_cleanup;
    gsParticleKillOwned(PC_PTR(lbl_8047B660), 1);
    lbl_8047B660 = 0;
    lbl_8047B664 = 0;
skip_cleanup:
    uVar2 = std093_getCurrentGroupID();
    uVar2 = PC_PTR32_IMPLICIT(GSresGetResource(uVar2, 0x108a1400));
    uVar2 = gsParticleCreate(uVar2, 0, 0);
    lbl_8047B660 = uVar2;
    gsParticleSetVecB(PC_PTR(uVar2), local_buf);
    fVar1 = lbl_8047E684;
    lbl_8047B664 = 1;
    waitTime__Ff(fVar1);
    if ((s32)lbl_8047B664 == 1) {
        gsParticleKillOwned(PC_PTR(lbl_8047B660), 1);
        lbl_8047B660 = 0;
        lbl_8047B664 = 0;
    }
}

/* Address: 0x8025E5F4 | Size: 0x4C | Ghidra import */
void preReliveDisplayHokoraParticle(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int GSthreadCreate();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 floorGetCurrentContext();
#endif
    extern void _preReliveDisplayHokoraParticleThreadFunc__Fv();
    extern u32 scriptGetDarkPointZeroPokemonNum();
  u32 uVar1;
  u32 uVar2;

  uVar1 = scriptGetDarkPointZeroPokemonNum();
  if ((uVar1 & 0xFFFF) != 0) {
    uVar2 = floorGetCurrentContext();
    GSthreadCreate(1, uVar2, 0x4000, 1, 1, PC_PTR32(_preReliveDisplayHokoraParticleThreadFunc__Fv));
  }
}

#endif

#if defined(POKEMON_RELIVE_EXP_ONLY)
/* Address: 0x8025E640 | Size: 0x37C */
typedef struct ReliveLevelUpStatus {
    u8 mode;
    u8 pad;
    s16 hp;
    s16 attack;
    s16 defense;
    s16 speed;
    s16 spAttack;
    s16 spDefense;
} ReliveLevelUpStatus;

int _cbWazaForget__FP7PokemonUsl(u8* pokemon, u32 addExp);

static inline s16 expRecoverGetStat(u8* p, u32 id)
{
    return (s16)pokemonGetStatus(p, 0, id, 0);
}

u32 _expRecover__FP7PokemonUl(u8* pokemon, u32 addExp)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgCloseLevelUpStatus(s32);
#endif
    extern void winMsgOpenLevelUpStatus(ReliveLevelUpStatus*, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpenField(s32, s32, s32);
    extern void pokemonBiosSetExp(u8*, u32);
    extern u32 pokemonBiosGetLevel(u8*);
    extern void pokemonGetFriendFormPokemonFriendFilterId(u8*, u32, s32);
    extern u32 pokemonGetLevelToExp(u8*, u8);
    extern u32 pokemonGetSoubiItemSoubiDataId(u8*);
    extern u32 pokemonGetOboeWazaDataId(u8*, u32, u8*);
    extern s8 pokemonSearchWazaDataId(u8*, u32);
    extern void pokemonWazaCreate(u8*, u8, u32);
    extern void pokemonResetBasisStatus(u8*);
    extern void msgctrlSetValue(s32, u32);
#else /* PC: these two have no canonical prototype */
    extern void pokemonBiosSetExp(u8*, u32);
    extern u32 pokemonGetLevelToExp(u8*, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std130_soundUnk_801653cc(s32, s32, s32);
#endif
    u8 slot;
    u32 next;
    ReliveLevelUpStatus gain;
    s16 hp;
    s16 attack;
    s16 defense;
    u8 level;
    s16 spAttack;
    s16 spDefense;
    u32 rest;
    ReliveLevelUpStatus status;
    u32 newLevel;
    u32 levelUp;
    u32 exp;
    u8 wazaIndex;
    u32 waza;
    u32 item;
    s16 speed;

    /* RULE-EXCEPTION(user-approved): scalar carrier, working copy of addExp chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
    rest = addExp;
    levelUp = 0;
    wazaIndex = 0;
    pokemonGetStatus(pokemon, 0, POKEMON_STATUS_LEVEL, 0);
    for (;;) {
        if (rest == 0) {
            pokemonSetStatus(pokemon, 0, POKEMON_STATUS_STORED_EXP, 0, rest);
            break;
        }
        level = (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_LEVEL, 0);
        if (level >= 100) {
            break;
        }
        exp = (u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_EXP, 0);
        next = pokemonGetLevelToExp(pokemon, level + 1);
        hp = expRecoverGetStat(pokemon, 0x87);
        attack = expRecoverGetStat(pokemon, 0x88);
        defense = expRecoverGetStat(pokemon, 0x89);
        spAttack = expRecoverGetStat(pokemon, 0x8A);
        spDefense = expRecoverGetStat(pokemon, 0x8B);
        speed = expRecoverGetStat(pokemon, 0x8C);
        exp += rest;
        if (exp >= next) {
            rest = exp - next;
            levelUp = 1;
            pokemonBiosSetExp(pokemon, next);
            pokemonResetBasisStatus(pokemon);
            item = pokemonGetSoubiItemSoubiDataId(pokemon);
            pokemonGetFriendFormPokemonFriendFilterId(pokemon, item, 0);
            newLevel = pokemonBiosGetLevel(pokemon);
            std130_soundUnk_801653cc(0x4CA, 0, 0xFF);
            msgctrlSetValue(0x2F, (u8)newLevel);
            winMsgOpenField(0x44CE, 1, 0);
            status.hp = expRecoverGetStat(pokemon, 0x87);
            status.attack = expRecoverGetStat(pokemon, 0x88);
            status.defense = expRecoverGetStat(pokemon, 0x89);
            status.spAttack = expRecoverGetStat(pokemon, 0x8A);
            status.spDefense = expRecoverGetStat(pokemon, 0x8B);
            status.speed = expRecoverGetStat(pokemon, 0x8C);
            gain.hp = status.hp - hp;
            gain.attack = status.attack - attack;
            gain.defense = status.defense - defense;
            gain.spAttack = status.spAttack - spAttack;
            gain.spDefense = status.spDefense - spDefense;
            gain.speed = status.speed - speed;
            gain.mode = 1;
            winMsgOpenLevelUpStatus(&gain, 1);
            status.mode = 0;
            winMsgOpenLevelUpStatus(&status, 1);
            winMsgCloseLevelUpStatus(1);
            wazaIndex = 0;
            while ((u16)(waza = pokemonGetOboeWazaDataId(pokemon, newLevel, &wazaIndex)) != 0) {
                if (pokemonSearchWazaDataId(pokemon, waza) == -1 &&
                    (s32)evolutionWazaLearn(pokemon, waza, &slot, 0, _cbWazaForget__FP7PokemonUsl, 0) != 0) {
                    pokemonWazaCreate(pokemon, slot, waza);
                }
                wazaIndex++;
            }
        } else {
            rest = 0;
            pokemonSetStatus(pokemon, 0, POKEMON_STATUS_EXP, 0, exp);
        }
    }
    return levelUp;
}

#endif

#if defined(POKEMON_RELIVE_PRE_MAIN_ONLY)
void reliveSaveSound(int *r3);
/* Address: 0x8025EF58 | Size: 0x354 | Ghidra import */

typedef struct ReliveSelection {
    s32 mode;
    u32 index;
} ReliveSelection;

typedef struct ReliveTarget {
    s32 mode;
    s16 species;
    s16 pad;
    u32 index;
} ReliveTarget;

void preReliveMain(void)
{
    extern void reliveRestoreSound(int* param);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuPokemonOpen(s32, s32, s32);
#endif
    extern void std098_mapWarpToMenuRoom(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgCloseField(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpenField(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonBiosGetDarkpokemonDataId(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int heroItemCheckHaveItemDataId(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroGetStatus(s32, s32, u16);
#endif
    extern u8 darkPokemonCheckFlag14(u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroBiosGetPokemonPtr(void*, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonIsDarkPokemon(void*);
#endif
    extern u32 scriptGetDarkPointZeroPokemonNum(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKillOwned(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std130_soundUnk_801653cc(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
    extern u32 lbl_8047B660;
    extern u32 lbl_8047B664;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E680;
#endif
#endif
    extern f32 lbl_8047E68C;
    s16 species;
    u16 zeroNum;
    void* party;
    void* pokemon;
    u16 remaining;
    s32 i;
    ReliveSelection* selection;
    u16 mode;
    s32 selMode;
    u16 action;
    s32 picked;
    u8 work[24];

    if (((ReliveSelection*)&lbl_8047B668)->mode == -1) {
        return;
    }
    remaining = 0;
    savedataGetStatus(0, 0);
    party = (void*)PC_PTR(savedataGetStatus(0, 2));
    for (i = 0; (u16)i < 6; i++) {
        pokemon = heroBiosGetPokemonPtr(party, (u16)i);
        if (pokemonCheckValid(pokemon) && (s32)(u8)pokemonIsDarkPokemon(pokemon) == 1 &&
            darkPokemonCheckFlag14(pokemonBiosGetDarkpokemonDataId(pokemon)) == 0) {
            remaining++;
        }
    }
    zeroNum = scriptGetDarkPointZeroPokemonNum();
    savedataGetStatus(0, 0);
    savedataGetStatus(0, 2);
    if ((s32)(u8)heroItemCheckHaveItemDataId(0, 0x219) == 1) {
        if (remaining != 0) {
            if (zeroNum != 0) {
                mode = 2;
            } else {
                mode = 1;
            }
        } else {
            mode = 3;
        }
    } else if (zeroNum != 0) {
        mode = 4;
    } else {
        mode = 5;
    }
    selMode = ((ReliveSelection*)&lbl_8047B668)->mode;
    action = mode;
    if (selMode == 0) {
        if (mode == 1) {
            action = 5;
        } else if (mode == 2) {
            action = 4;
        } else if (mode == 3) {
            action = 5;
        }
    } else if (selMode == 1) {
        action = 2;
    }

    if (action == 2) {
        reliveSaveSound((int*)work);
        std130_soundUnk_801653cc(0x3C8, 0, 0xFF);
        selection = (ReliveSelection*)&lbl_8047B668;
        species = (s16)pokemonGetStatus(PC_PTR(heroGetStatus(0, 3, selection->index)), 0, POKEMON_STATUS_SPECIES, 0);
        ((ReliveTarget*)lbl_804782BC)->mode = 1;
        ((ReliveTarget*)lbl_804782BC)->species = species;
        ((ReliveTarget*)lbl_804782BC)->index = selection->index;
        fadeSet(3, lbl_8047E680);
        fadeCheck(1);
        std098_mapWarpToMenuRoom(0x385);
        floorSetFadeScript(0, 0);
        _threadSwitch();
        fadeSet(2, lbl_8047E68C);
        fadeCheck(1);
        reliveRestoreSound((int*)work);
    } else if (action == 4) {
        if ((s32)lbl_8047B664 == 1) {
            gsParticleKillOwned(PC_PTR(lbl_8047B660), 1);
            lbl_8047B660 = 0;
            lbl_8047B664 = 0;
        }
        if ((u16)scriptGetDarkPointZeroPokemonNum() == 0) {
            lbl_8047B668 = 0xFFFFFFFF;
            ((u32*)&lbl_8047B668)[1] = 0xFFFFFFFF;
        } else {
#ifdef TARGET_PC
            /* OrreHook_RelivePickSlot (step 3.7, PURIFY in the party menu; pc/orre_hooks.h): the Pokémon already
             * chosen with PURIFY, so the player isn't asked twice; -1: "Which POKéMON..." and the stone's own picker.
             * Everything after (the target, the fade, the sound, room 0x385, the ceremony) is the game's own. NULL: as
             * on the GameCube. All platforms */
            picked = OrreHook_RelivePickSlot != NULL ? OrreHook_RelivePickSlot() : -1;
            if (picked < 0) {
                winMsgOpenField(0x3B0F, 1, 0);
                winMsgCloseField(1);
                picked = menuPokemonOpen(7, 0, 0);
            }
#else
            winMsgOpenField(0x3B0F, 1, 0);
            winMsgCloseField(1);
            picked = menuPokemonOpen(7, 0, 0);
#endif
            if (picked != -1) {
                species = (s16)pokemonGetStatus(PC_PTR(heroGetStatus(0, 3, picked)), 0, POKEMON_STATUS_SPECIES, 0);
                ((ReliveTarget*)lbl_804782BC)->mode = 0;
                ((ReliveTarget*)lbl_804782BC)->species = species;
                ((ReliveTarget*)lbl_804782BC)->index = picked;
                fadeSet(3, lbl_8047E680);
                fadeCheck(1);
                reliveSaveSound((int*)work);
                std098_mapWarpToMenuRoom(0x385);
                floorSetFadeScript(0, 0);
                _threadSwitch();
                fadeSet(2, lbl_8047E68C);
                fadeCheck(1);
                reliveRestoreSound((int*)work);
            }
        }
    } else {
        lbl_8047B668 = 0xFFFFFFFF;
        ((u32*)&lbl_8047B668)[1] = 0xFFFFFFFF;
    }
}
#endif

#if defined(POKEMON_RELIVE_SEQUENCE_EXACT_ONLY)
#pragma push
/* Address: 0x8025E3B0 | Size: 0x184 | Ghidra import */

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
int _cbWazaForget__FP7PokemonUsl(u32 r3,u32 r4)

{
    extern u8 lbl_80478288[];
    extern u32 lbl_80478DB0;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E680;
#endif

  u32 uVar1;
  int iVar2;
  int iVar3;
  u32 *snapshot;

  u32 view[3];
  u32 position[3];
  u32 rotation[3];
  u32 direction[3];
  u8 auStack_48 [4];
  u8 auStack_4c [4];
  u8 auStack_50 [4];
  f32 local_54;
  u8 auStack_58 [4];
  
  fadeSetDurationFirst((double)lbl_8047E680,3);
  fadeCheck(1);
  GSscene_GetCameraDirectionVector(direction);
  GSscene_GetCameraRotationVector(rotation);
  GSscene_GetCameraPositionVector(position);
  GSscene_GetCameraViewVector(view);
  uVar1 = PC_PTR32_IMPLICIT(GScameraGetActiveCamera());
  GScameraGetPerspective(PC_PTR(uVar1),&local_54,auStack_50,auStack_4c,auStack_48);
  snapshot = (u32 *)lbl_80478288;
  snapshot[0] = direction[0];
  snapshot[1] = direction[1];
  snapshot[2] = direction[2];
  snapshot[3] = rotation[0];
  snapshot[4] = rotation[1];
  snapshot[5] = rotation[2];
  snapshot[6] = position[0];
  snapshot[7] = position[1];
  snapshot[8] = position[2];
  snapshot[9] = view[0];
  snapshot[10] = view[1];
  snapshot[11] = view[2];
  ((f32 *)snapshot)[12] = local_54;
  wazaSequenceSysRelease();
  iVar2 = menuPokemonStatusOpenWazaForget(r3,r4);
  if (iVar2 >= 4) {
    iVar2 = -1;
  }
  pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_SPECIES,0);
  wazaSequenceSysInitWithoutParticles(1);
  iVar3 = loadSequence((int*)auStack_58,r3,(u16*)&lbl_80478DB0,1);
  if (iVar3 == 1) {
    fadeSetDurationFirst((double)lbl_8047E680,2);
    effectPlayWaza((u32*)auStack_58,(u16*)&lbl_80478DB0,1,1,1,0);
  }
  return (int)(signed char)iVar2;
}
#else
int _cbWazaForget__FP7PokemonUsl(u32 r3,u16 r4)

{
    extern u8 lbl_80478288[];
    extern u32 lbl_80478DB0;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E680;
#endif

  u32 uVar1;
  int iVar2;
  int iVar3;
  u32 *snapshot;

  u32 view[3];
  u32 position[3];
  u32 rotation[3];
  u32 direction[3];
  u8 auStack_48 [4];
  u8 auStack_4c [4];
  u8 auStack_50 [4];
  f32 local_54;
  u8 auStack_58 [4];
  
  fadeSetDurationFirst((double)lbl_8047E680,3);
  fadeCheck(1);
  GSscene_GetCameraDirectionVector((struct GSSceneVec3*)direction);
  GSscene_GetCameraRotationVector((struct GSSceneVec3*)rotation);
  GSscene_GetCameraPositionVector((struct GSSceneVec3*)position);
  GSscene_GetCameraViewVector((struct GSSceneVec3*)view);
  uVar1 = (u32)GScameraGetActiveCamera();
  GScameraGetPerspective((struct GSRenderCamera*)uVar1,&local_54,(f32*)auStack_50,(f32*)auStack_4c,(f32*)auStack_48);
  snapshot = (u32 *)lbl_80478288;
  snapshot[0] = direction[0];
  snapshot[1] = direction[1];
  snapshot[2] = direction[2];
  snapshot[3] = rotation[0];
  snapshot[4] = rotation[1];
  snapshot[5] = rotation[2];
  snapshot[6] = position[0];
  snapshot[7] = position[1];
  snapshot[8] = position[2];
  snapshot[9] = view[0];
  snapshot[10] = view[1];
  snapshot[11] = view[2];
  ((f32 *)snapshot)[12] = local_54;
  wazaSequenceSysRelease();
  iVar2 = menuPokemonStatusOpenWazaForget(r3,r4);
  if (iVar2 >= 4) {
    iVar2 = -1;
  }
  pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_SPECIES,0);
  wazaSequenceSysInitWithoutParticles(1);
  iVar3 = loadSequence((int*)auStack_58,r3,(u16*)&lbl_80478DB0,1);
  if (iVar3 == 1) {
    fadeSetDurationFirst((double)lbl_8047E680,2);
    effectPlayWaza((u32*)auStack_58,(u16*)&lbl_80478DB0,1,1,1,0);
  }
  return (int)(signed char)iVar2;
}
#endif
#pragma pop
#endif

#if defined(POKEMON_RELIVE_CEREMONY_ONLY)
extern u32 heroGetStatus(u8* ptr, u32 selector, u32 index);
/* RULE-EXCEPTION(user-approved): pokemonBiosGetDarkpokemonDataId is defined u16 in pokemon_data.c, but retail keeps its result in the u32 local unextended; the u16 prototype adds clrlwi - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonBiosGetDarkpokemonDataId(u8* ptr);
#endif
extern u32 std130_soundUnk_801653cc(u32 id, void* buffer, u32 size);
extern void msgctrlSetValue(u32 id, u32 value);
extern s32 winMsgOpenField(u32 message, u32 wait, u8 pause);
extern void winMsgCloseField(u32 wait);
u32 _expRecover__FP7PokemonUl(u8* pokemon, u32 addExp);
u32 loadSequence(int *r3,u32 r4,u16 *r5,int r6);
void effectPlayWaza(u32 *r3,u16 *r4,int r5,int r6,int r7,int r8);
extern u32 exribbonSetNarionalRibbon(u32 r3);
#pragma push
/* Address: 0x8025E9BC | Size: 0x390 | Ghidra import */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void reliveCeremonyAll(u32 r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 std011_dialogDisplayYesNoMenu();
#endif
    extern int menuNameEntryOpenNoFade();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonBiosSetFriend();
#endif
    extern int pokemonBiosSetPoolFriend();
    extern u16 pokemonBiosGetPoolFriend();
    extern u32 pokemonBiosGetPoolExp();
    extern u16 pokemonBiosGetFriend();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonBiosGetPokemonWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetNicknamePtr();
#endif
    extern int pokemonSetDp();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonInitPersistentJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonEvolutionAll();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonEvolutionCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid();
#endif
    extern int darkPokemonSetFlag14();
    extern u32 lbl_80478DB0;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E680;
#endif
    extern f32 lbl_8047E688;

  u32 uVar1;
  s8 cVar7;
  u8 uVar8;
  u32 uVar2;
  u16 sVar5;
  u16 sVar6;
  u16 sVar7;
  int iVar3;
  u32 uVar4;
  u32 evo;
  u32 nick;

  u8 auStack_18 [8];
  u8 auStack_1c [4];
  u8 auStack_20 [4];
  u8 auStack_24 [4];
  u16 local_28 [2];

  local_28[0] = 0;
  uVar1 = heroGetStatus(0,3,r3 & 0xffff);
  uVar8 = pokemonCheckValid(PC_PTR(uVar1));
  if (uVar8 == 1) {
    pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_SPECIES,0);
    uVar2 = pokemonBiosGetDarkpokemonDataId(PC_PTR(uVar1));
    darkPokemonSetFlag14(uVar2,1);
    uVar8 = pokemonIsJoutaiDataId(PC_PTR(uVar1),0x3e);
    if (uVar8 != 0) {
      pokemonInitPersistentJoutai(PC_PTR(uVar1),0x3e);
    }
    {
      /* PC: the real prototype is pokemonSetDp(u8* ptr, f32 dp) (pokemon_range_8011F77C.c). Called unprototyped with
       * the float first, fine on the GameCube (floats and pointers travel in separate registers), but on x64 the
       * arguments are positional, so the empty gauge went through a garbage pointer (P11: the next party Pokémon's max
       * HP and attack read 0 after the first purification; port-gaps 30) */
      extern void pokemonSetDp_typed(u8* ptr, f32 dp) __asm__("pokemonSetDp");
      pokemonSetDp_typed(PC_PTR(uVar1), lbl_8047E688);
    }
    std130_soundUnk_801653cc(0x3f7,0,0xff);
    sVar5 = pokemonBiosGetPokemonWazaDataId(PC_PTR(uVar1),0);
    if (sVar5 != 0) {
      nick = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr(uVar1));
      msgctrlSetValue(0x32,nick);
      msgctrlSetValue(0x39,sVar5);
      winMsgOpenField(0x3b10,1,0);
    }
    uVar4 = pokemonBiosGetPoolExp(uVar1);
    uVar2 = uVar4;
    msgctrlSetValue(0x2f,uVar2);
    winMsgOpenField(0x3b0b,1,0);
    winMsgCloseField(1);
    sVar7 = pokemonBiosGetFriend(uVar1);
    pokemonBiosSetFriend(PC_PTR(uVar1),(u16)(sVar7 + pokemonBiosGetPoolFriend(uVar1) + 0x46));
    pokemonBiosSetPoolFriend(uVar1,0);
    /* PC (2026-10-29; observes only): port-gaps 30, a Pokémon's max HP and attack read 0 after the ceremony */
    OSReport("[relive] slot %u species %u before: level %u exp %u pool %u hp %d/%d atk %d def %d\n", (unsigned)(r3 & 0xffff),
             (unsigned)(u16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_SPECIES,0), (unsigned)(u8)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_LEVEL,0),
             (unsigned)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_EXP,0), (unsigned)uVar2,
             (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_HP,0), (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_MAX_HP,0),
             (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_ATTACK,0), (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_DEFENSE,0));
    iVar3 = _expRecover__FP7PokemonUl(PC_PTR(uVar1),uVar2); /* PC: the merged _expRecover takes the Pokemon pointer */
    OSReport("[relive] after: level %u exp %u hp %d/%d atk %d def %d levelup %d\n",
             (unsigned)(u8)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_LEVEL,0), (unsigned)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_EXP,0),
             (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_HP,0), (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_MAX_HP,0),
             (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_ATTACK,0), (int)(s16)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_DEFENSE,0), iVar3);
    if (iVar3 == 1) {
      evo = pokemonEvolutionCheck(PC_PTR(uVar1),0,0,local_28,auStack_18);
      if (((evo & 0xffff) != 0) && ((evo & 0xffff) != 0xffff)) {
        fadeSetDurationFirst((double)lbl_8047E680,3);
        fadeCheck(1);
        wazaSequenceSysRelease();
        iVar3 = pokemonEvolutionAll(PC_PTR(uVar1),evo,local_28[0],auStack_18,0,1,1,0);
        if (iVar3 == 0) {
          wazaSequenceSysInitWithoutParticles(1);
          iVar3 = loadSequence((int*)auStack_1c,uVar1,(u16*)&lbl_80478DB0,1);
          if (iVar3 == 1) {
            fadeSetDurationFirst((double)lbl_8047E680,2);
            effectPlayWaza((u32*)auStack_1c,(u16*)&lbl_80478DB0,1,1,1,0);
          }
          uVar2 = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr(uVar1));
          msgctrlSetValue(0x32,uVar2);
        }
        else {
          uVar2 = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr(uVar1));
          msgctrlSetValue(0x32,uVar2);
          wazaSequenceSysInitWithoutParticles(1);
          iVar3 = loadSequence((int*)auStack_20,uVar1,(u16*)&lbl_80478DB0,1);
          if (iVar3 == 1) {
            fadeSetDurationFirst((double)lbl_8047E680,2);
            effectPlayWaza((u32*)auStack_20,(u16*)&lbl_80478DB0,1,1,1,0);
          }
        }
      }
    }
    std130_soundUnk_801653cc(0x3ca,0,0xff);
    exribbonSetNarionalRibbon(uVar1);
    winMsgOpenField(0x3b0c,1,0);
    winMsgOpenField(0x3b0d,1,1);
    cVar7 = std011_dialogDisplayYesNoMenu();
    if (cVar7 != '\0') {
      winMsgCloseField(1);
    }
    else {
      winMsgCloseField(1);
      fadeSetDurationFirst((double)lbl_8047E680,3);
      fadeCheck(1);
      wazaSequenceSysRelease();
      menuNameEntryOpenNoFade(2,r3);
      wazaSequenceSysInitWithoutParticles(1);
      iVar3 = loadSequence((int*)auStack_24,uVar1,(u16*)&lbl_80478DB0,1);
      if (iVar3 == 1) {
        fadeSetDurationFirst((double)lbl_8047E680,2);
        effectPlayWaza((u32*)auStack_24,(u16*)&lbl_80478DB0,1,1,1,0);
      }
    }
  }
  return;
}
#else
void reliveCeremonyAll(u32 r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 std011_dialogDisplayYesNoMenu();
#endif
    extern int menuNameEntryOpenNoFade();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonBiosSetFriend();
#endif
    extern int pokemonBiosSetPoolFriend();
    extern u16 pokemonBiosGetPoolFriend();
    extern u32 pokemonBiosGetPoolExp();
    extern u16 pokemonBiosGetFriend();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonBiosGetPokemonWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetNicknamePtr();
#endif
    extern int pokemonSetDp();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonInitPersistentJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int pokemonEvolutionAll();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonEvolutionCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid();
#endif
    extern int darkPokemonSetFlag14();
    extern u32 lbl_80478DB0;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E680;
#endif
#endif
    extern f32 lbl_8047E688;

  u32 uVar1;
  s8 cVar7;
  u8 uVar8;
  u32 uVar2;
  u16 sVar5;
  u16 sVar6;
  u16 sVar7;
  int iVar3;
  u32 uVar4;
  u32 evo;
  u32 nick;

  u8 auStack_18 [8];
  u8 auStack_1c [4];
  u8 auStack_20 [4];
  u8 auStack_24 [4];
  u16 local_28 [2];

  local_28[0] = 0;
  uVar1 = heroGetStatus(0,3,r3 & 0xffff);
  uVar8 = pokemonCheckValid(PC_PTR(uVar1));
  if (uVar8 == 1) {
    pokemonGetStatus(uVar1,0,POKEMON_STATUS_SPECIES,0);
    uVar2 = pokemonBiosGetDarkpokemonDataId((u8*)uVar1);
    darkPokemonSetFlag14(uVar2,1);
    uVar8 = pokemonIsJoutaiDataId(PC_PTR(uVar1),0x3e);
    if (uVar8 != 0) {
      pokemonInitPersistentJoutai(PC_PTR(uVar1),0x3e);
    }
    pokemonSetDp((double)lbl_8047E688,uVar1);
    std130_soundUnk_801653cc(0x3f7,0,0xff);
    sVar5 = pokemonBiosGetPokemonWazaDataId(PC_PTR(uVar1),0);
    if (sVar5 != 0) {
      nick = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr(uVar1));
      msgctrlSetValue(0x32,nick);
      msgctrlSetValue(0x39,sVar5);
      winMsgOpenField(0x3b10,1,0);
    }
    uVar4 = pokemonBiosGetPoolExp(uVar1);
    uVar2 = uVar4;
    msgctrlSetValue(0x2f,uVar2);
    winMsgOpenField(0x3b0b,1,0);
    winMsgCloseField(1);
    sVar7 = pokemonBiosGetFriend(uVar1);
    pokemonBiosSetFriend(PC_PTR(uVar1),(u16)(sVar7 + pokemonBiosGetPoolFriend(uVar1) + 0x46));
    pokemonBiosSetPoolFriend(uVar1,0);
    iVar3 = _expRecover__FP7PokemonUl((u8*)uVar1,uVar2);
    if (iVar3 == 1) {
      evo = pokemonEvolutionCheck(PC_PTR(uVar1),0,0,local_28,auStack_18);
      if (((evo & 0xffff) != 0) && ((evo & 0xffff) != 0xffff)) {
        fadeSetDurationFirst((double)lbl_8047E680,3);
        fadeCheck(1);
        wazaSequenceSysRelease();
        iVar3 = pokemonEvolutionAll(PC_PTR(uVar1),evo,local_28[0],auStack_18,0,1,1,0);
        if (iVar3 == 0) {
          wazaSequenceSysInitWithoutParticles(1);
          iVar3 = loadSequence((int*)auStack_1c,uVar1,(u16*)&lbl_80478DB0,1);
          if (iVar3 == 1) {
            fadeSetDurationFirst((double)lbl_8047E680,2);
            effectPlayWaza((u32*)auStack_1c,(u16*)&lbl_80478DB0,1,1,1,0);
          }
          uVar2 = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr(uVar1));
          msgctrlSetValue(0x32,uVar2);
        }
        else {
          uVar2 = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr(uVar1));
          msgctrlSetValue(0x32,uVar2);
          wazaSequenceSysInitWithoutParticles(1);
          iVar3 = loadSequence((int*)auStack_20,uVar1,(u16*)&lbl_80478DB0,1);
          if (iVar3 == 1) {
            fadeSetDurationFirst((double)lbl_8047E680,2);
            effectPlayWaza((u32*)auStack_20,(u16*)&lbl_80478DB0,1,1,1,0);
          }
        }
      }
    }
    std130_soundUnk_801653cc(0x3ca,0,0xff);
    exribbonSetNarionalRibbon(uVar1);
    winMsgOpenField(0x3b0c,1,0);
    winMsgOpenField(0x3b0d,1,1);
    cVar7 = std011_dialogDisplayYesNoMenu();
    if (cVar7 != '\0') {
      winMsgCloseField(1);
    }
    else {
      winMsgCloseField(1);
      fadeSetDurationFirst((double)lbl_8047E680,3);
      fadeCheck(1);
      wazaSequenceSysRelease();
      menuNameEntryOpenNoFade(2,r3);
      wazaSequenceSysInitWithoutParticles(1);
      iVar3 = loadSequence((int*)auStack_24,uVar1,(u16*)&lbl_80478DB0,1);
      if (iVar3 == 1) {
        fadeSetDurationFirst((double)lbl_8047E680,2);
        effectPlayWaza((u32*)auStack_24,(u16*)&lbl_80478DB0,1,1,1,0);
      }
    }
  }
  return;
}
#endif
#pragma pop
#endif

#if defined(POKEMON_RELIVE_MAIN_ONLY)
/* Address: 0x8025ED4C | Size: 0x20C */

typedef struct ReliveResetPair {
    s32 mode;
    u32 index;
} ReliveResetPair;
typedef struct ReliveTargetState {
    s32 mode;
    u16 species;
    u16 pad;
    u32 index;
    u8 rest[0x18];
} ReliveTargetState;
/* lbl_8047B668: the 8-byte relive selection in .sbss (sda-addressed under -sdata 8). */
extern ReliveResetPair lbl_8047B668;
/* lbl_804782BC: the 0x24-byte pending relive request in .bss. */
extern ReliveTargetState lbl_804782BC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroGetStatus(s32, s32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(void*);
#endif
extern void floorBattleEndTerminateScripts(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorSetFadeScript(s32, s32);
#endif
extern void _threadSwitch(void);
u32 loadSequence(int* r3, u32 r4, u16* r5, int r6);
void effectPlayWaza(u32* r3, u16* r4, int r5, int r6, int r7, int r8);

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void reliveMain(void)

{
    extern u32 lbl_80478DA8;
    extern u32 lbl_8047B66C;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E680;
#endif
    /* PC: the file now declares the relive request (lbl_804782BC) and the selection (lbl_8047B668) as structs;
       this copy reads the request as words */
#define lbl_804782BC ((s32*)&lbl_804782BC)
  BOOL bVar1;

		  u32 uVar2;
		  u8 cVar4;
		  int iVar3;

  u8 auStack_18 [16];

  {
    /* ORRE_EVENT_LOG (2026-10-29; observes only): the purification request as the Relic room reads it, "[relive]
     * mode M index I" (mode 0 / 1 run the ceremony, -1 none). P11: the console purifies at the Relic Stone with no press,
     * the PC shows nothing although two party Shadow Pokémon are at gauge 0 */
    extern char* getenv(const char*);
    static int on = -1;
    if (on < 0) {
      const char* env = getenv("ORRE_EVENT_LOG");
      on = env != NULL && *env != '\0' && *env != '0';
    }
    if (on) {
      OSReport("[relive] mode %d index %d\n", (int)lbl_804782BC[0], (int)lbl_804782BC[2]);
    }
  }
  if ((lbl_804782BC[0] == 0) || (lbl_804782BC[0] == 1)) {
    wazaSequenceSysInitWithoutParticles(1);
    if (lbl_804782BC[0] == 0) {
      uVar2 = heroGetStatus(0,3,lbl_804782BC[2] & 0xffff);
      cVar4 = pokemonCheckValid(PC_PTR(uVar2));
      if (cVar4 != '\x01') {
        bVar1 = 0;
      }
      else {
        pokemonGetStatus(PC_PTR(uVar2),0,POKEMON_STATUS_SPECIES,0);
        iVar3 = loadSequence((int*)auStack_18,uVar2,(u16*)&lbl_80478DA8,2);
        if (iVar3 == 1) {
          effectPlayWaza((u32*)auStack_18,(u16*)&lbl_80478DA8,2,1,1,0);
          bVar1 = 1;
        }
        else {
          bVar1 = 0;
        }
      }
      OSReport("[relive] mode 0: valid %d, sequence loaded %d\n", (int)cVar4, (int)bVar1);
      if (bVar1 == 1) {
        reliveCeremonyAll(lbl_804782BC[2]);
      }
    }
    else if (lbl_804782BC[0] == 1) {
      uVar2 = heroGetStatus(0,3,lbl_804782BC[2] & 0xffff);
      cVar4 = pokemonCheckValid(PC_PTR(uVar2));
      if (cVar4 != '\x01') {
        bVar1 = 0;
      }
      else {
        pokemonSetStatus(PC_PTR(uVar2),0,POKEMON_STATUS_HEART_GAUGE,0,0);
        pokemonGetStatus(PC_PTR(uVar2),0,POKEMON_STATUS_SPECIES,0);
        iVar3 = loadSequence((int*)auStack_18,uVar2,(u16*)lbl_8039A6A8,4);
        if (iVar3 == 1) {
          effectPlayWaza((u32*)auStack_18,(u16*)lbl_8039A6A8,4,1,1,0);
          bVar1 = 1;
        }
        else {
          bVar1 = 0;
        }
      }
      OSReport("[relive] mode 1: valid %d, sequence loaded %d\n", (int)cVar4, (int)bVar1);
      if (bVar1 == 1) {
        reliveCeremonyAll(lbl_804782BC[2]);
      }
    }
    fadeSetDurationFirst((double)lbl_8047E680,3);
    fadeCheck(1);
				    wazaSequenceSysRelease();
						    lbl_804782BC[0] = -1;
						    *(u32*)&lbl_8047B668 = 0xffffffff;
						    lbl_8047B66C = 0xffffffff;
				    *(u16 *)((u8 *)lbl_804782BC + 4) = 0;
    lbl_804782BC[2] = 0;
    floorBattleEndTerminateScripts();
    floorSetFadeScript(0,0);
    _threadSwitch();
  }
  return;
}
#undef lbl_804782BC
#else
void reliveMain(void)
{
    extern u32 lbl_80478DA8;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E680;
#endif
    BOOL ok;
    void* pokemon;
    s32 loaded;
    u8 seq[16];

    if ((lbl_804782BC.mode == 0) || (lbl_804782BC.mode == 1)) {
        wazaSequenceSysInitWithoutParticles(1);
        if (lbl_804782BC.mode == 0) {
            pokemon = heroGetStatus(0, 3, lbl_804782BC.index & 0xffff);
            if (pokemonCheckValid(pokemon) != 1) {
                ok = 0;
            } else {
                pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0);
                loaded = loadSequence((int*)seq, (u32)pokemon, (u16*)&lbl_80478DA8, 2);
                if (loaded == 1) {
                    effectPlayWaza((u32*)seq, (u16*)&lbl_80478DA8, 2, 1, 1, 0);
                    ok = 1;
                } else {
                    ok = 0;
                }
            }
            if (ok == 1) {
                reliveCeremonyAll(lbl_804782BC.index);
            }
        } else if (lbl_804782BC.mode == 1) {
            pokemon = heroGetStatus(0, 3, lbl_804782BC.index & 0xffff);
            if (pokemonCheckValid(pokemon) != 1) {
                ok = 0;
            } else {
                pokemonSetStatus(pokemon, 0, POKEMON_STATUS_HEART_GAUGE, 0, 0);
                pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0);
                loaded = loadSequence((int*)seq, (u32)pokemon, (u16*)lbl_8039A6A8, 4);
                if (loaded == 1) {
                    effectPlayWaza((u32*)seq, (u16*)lbl_8039A6A8, 4, 1, 1, 0);
                    ok = 1;
                } else {
                    ok = 0;
                }
            }
            if (ok == 1) {
                reliveCeremonyAll(lbl_804782BC.index);
            }
        }
        fadeSet((double)lbl_8047E680, 3);
        fadeCheck(1);
        wazaSequenceSysRelease();

        lbl_8047B668.mode = -1;
        lbl_8047B668.index = -1;
        lbl_804782BC.mode = -1;
        lbl_804782BC.species = 0;
        lbl_804782BC.index = 0;
        floorBattleEndTerminateScripts();
        floorSetFadeScript(0, 0);
        _threadSwitch();
    }
}
#endif
#endif

/* Address: 0x8025DC2C | Size: 0x90 | Ghidra import (PSQ removed) */


#if defined(POKEMON_RELIVE_WAITTIME_ONLY)
extern void _threadSwitch(void);
extern u32 GSgetFrameStep(void);
void waitTime__Ff(float r3)

{
    extern f32 lbl_8047E660;
    extern f32 lbl_8047E664;
    extern f64 lbl_8047E668;
  u32 uVar2;

  double dVar4;
  float fVar1;

  dVar4 = (double)(float)(lbl_8047E664 * r3);
  fVar1 = lbl_8047E660;
  while (fVar1 < dVar4) {
    _threadSwitch();
    uVar2 = GSgetFrameStep();
    fVar1 += (float)uVar2;
  }

  return;
}
#endif
