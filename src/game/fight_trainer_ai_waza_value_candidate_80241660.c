/**
 * @file fight_trainer_ai_waza_value_candidate_80241660.c
 * @brief Candidate fightTrainerAiWazaValue.cpp range, 0x80241660 - 0x80241B70.
 *
 * Physically split from fight_trainer_ai_waza_value.c so this
 * translation unit owns only the functions in the stated range.
 */
#include "game/colosseum.h"
#include "game/trainer.h"
#include "game/pokemon.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"

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
extern u32 lbl_8047B668;
extern u32 lbl_8047B66C;
extern u32 lbl_8047B670;

/* Data labels used by asm wrappers */
extern u8  lbl_8039A6B8[];
extern u8  lbl_8039A6A8[];
extern int lbl_804782BC[];
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
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u16 fightFloorGetFightTrainerFightPokemonPtrAry(u32, void*, void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fn_80237F74(void*, u32, u32);
#endif
u8 fn_802384B4(void*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightTrainerAiGetNowTenkou(void*, u32, u32);
#endif
u16 fightTrainerAiGetPokemonDataId2(void*, u32);
u8 fightTrainerAiGetPokemonZokusei(void*, u32, u32);
u8 fightTrainerAiIsPokemonTokusei(void*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif


/* Address: 0x80241660 | Size: 0x510 (1296 bytes) */
u32 fightTrainerAiWazaValueTedasuke(void *ctx, u32 param1, u32 param2, u32 param3)
{
  extern u32 *waza_data_number;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u8 fightActionCheckValid(void *);
#endif
  extern u16 fightActionGetKindDataId(void *);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightOutPokemonGetUseWazaDataId(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u16 fightFloorGetFightTrainerFightOutPokemonPtrAry(u32, void *, u32 *, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern s32 fightOutPokemonGetNowKaisuuJoutaiDataId(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern s32 fightOutPokemonGetKaisuuJoutaiDataId(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u16 fightTrainerAiGetFightPokemonWazaAry(void *, u32, u16 *, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u8 fightTrainerAiIsPokemonJoutai(void *, u32, u32);
#endif
  extern u8 fightTrainerAiIsWazaTypeId(void *, u32, u32);
  unsigned int new_var2;
  u32 enemy[6];
  u16 waza[10];
  u32 handle;
  u16 enemyCount;
  u16 wazaCount;
  u16 i;
  u16 j;
  u8 supportWazaOut;
  u8 allEnemySupport;
  void *action;
  u32 pokemon;
  s32 need;
  s8 enough;
  /* RULE-EXCEPTION(user-approved): chained zeroing, need reused as the move loop counter and a split need computation, chosen for register allocation — see docs/RULE_EXCEPTIONS.md */
  allEnemySupport = (handle = 0);
  enemyCount = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), enemy, 1, 1);
  supportWazaOut = 0;
  for (need = 0; (need & 0xFFFF) < (*waza_data_number); need++)
  {
    if (((((u16) need) == 0) || (((u16) need) == 0x165)) || (((u16) need) == 0x163))
    {
      continue;
    }
    if (fightTrainerAiIsWazaTypeId(ctx, need, 1) == 0)
    {
      continue;
    }
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(need), 0) == 1)
    {
      supportWazaOut = 1;
    }
  }

  if (supportWazaOut == 1)
  {
    handle = fn_80239984(0, ctx, 0x1b0);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, allEnemySupport, 0x1b0);
  }
  for (i = 0; i < enemyCount; i++)
  {
    if (param1 == enemy[i])
    {
      continue;
    }
    wazaCount = fightTrainerAiGetFightPokemonWazaAry(ctx, enemy[i], waza, 0, 1);
    if (wazaCount == 0)
    {
      continue;
    }
    for (j = 0; j < wazaCount; j++)
    {
      if (fightTrainerAiIsWazaTypeId(ctx, waza[j], 1) != 0)
      {
        handle = fn_80239984(handle, ctx, 0x1b1);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x1b1);
        break;
      }
    }

  }

  allEnemySupport = 1;
  for (i = 0; i < enemyCount; i++)
  {
    if (param1 == enemy[i])
    {
      continue;
    }
    action = PC_PTR(pokemonGetStatus(PC_PTR(enemy[i]), 0, POKEMON_STATUS_ACTION_PTR, 0));
    if (action == ((void *) 0))
    {
      continue;
    }
    if (fightActionCheckValid(action) == 0)
    {
      continue;
    }
    if (fightActionGetKindDataId(action) != 0x13)
    {
      allEnemySupport = 0;
      break;
    }
    pokemon = fightOutPokemonGetUseWazaDataId(PC_PTR(enemy[i]));
    if (fightTrainerAiIsWazaTypeId(ctx, pokemon, 1) == 0)
    {
      allEnemySupport = 0;
      break;
    }
  }

  if (allEnemySupport == 0)
  {
    handle = fn_80239984(handle, ctx, 0x1b2);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x1b2);
  }
  for (i = 0; i < enemyCount; i++)
  {
    if (param1 == enemy[i])
    {
      continue;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(enemy[i]), PC_PTR(0x12)) != 1)
    {
      continue;
    }
    handle = fn_80239984(handle, ctx, 0x1b3);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x1b3);
    break;
  }

  for (i = 0; i < enemyCount; i++)
  {
    if (param1 == enemy[i])
    {
      continue;
    }
    if (((u8) ((u32) pokemonGetStatus(PC_PTR(enemy[i]), 0, POKEMON_STATUS_TRUANT_FLAG, 0))) == 0)
    {
      continue;
    }
    handle = fn_80239984(handle, ctx, 0x1b4);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x1b4);
    break;
  }

  for (i = 0; i < enemyCount; i++)
  {
    pokemon = enemy[i];
    if (param1 == pokemon)
    {
      continue;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(pokemon), PC_PTR(8)) == 0)
    {
      enough = -1;
    }
    else
    {
      need = fn_80237F74(ctx, pokemon, 0x30);
      need = (need == 1) ? 2 : 1;
      new_var2 = fightOutPokemonGetNowKaisuuJoutaiDataId(PC_PTR(pokemon), PC_PTR(CONDITION_SLEEP));
      need = new_var2 + need;
      if (((s8) need) >= ((s8) fightOutPokemonGetKaisuuJoutaiDataId(PC_PTR(pokemon), PC_PTR(CONDITION_SLEEP))))
      {
        enough = 1;
      }
      else
      {
        enough = 0;
      }
    }
    if (enough != 0)
    {
      continue;
    }
    handle = fn_80239984(handle, ctx, 0x1b5);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x1b5);
    break;
  }

  return handle;
}
