/**
 * @file fight_seq.c
 * @brief fightSeq module (head) -- split from colosseum_event.c (the fight
 *        engine bucket, 0x80202810-0x80211A00), address range
 *        0x80211170-0x80211A00, 7 fns.
 *
 * fightSeq's normal-waza damage calculator (hit chance / damage / crit
 * / normal-damage combined into one function here vs 4 in XD), plus
 * fightSeqInit/Post and the EffectAnimFlag accessors. Corresponds to
 * XD's fightSeq head (0x8020D858-0x8020E1A0).
 */

#include "game/colosseum.h"
#include "game/constants/move_effects.h"
#include "game/constants/pokemon_status.h"
#include "game/constants/species.h"
#include "game/constants/moves.h"
#include "game/constants/abilities.h"
#include "crt/string.h"
#include "game/fight_action.h"
#include "game/trainer.h"
#include "game/pokemon.h"
#include "game/constants/conditions.h"

typedef struct ColosseumEventRow6 {
    u8 mode;
    u8 field_01;
    u16 eventIndex;
    u16 nextIndex;
} ColosseumEventRow6;

typedef struct ColosseumEventSubRow {
    u8 valueMode;
    u8 scaleMode;
    s16 scaleNumerator;
    s16 scaleDenominator;
    u16 minValue;
    u16 maxValue;
} ColosseumEventSubRow;

typedef struct ColosseumEventPairRow {
    u8 resultFuncId;
    u8 field_01;
    u16 firstLinkIndex;
    ColosseumEventSubRow slots[2];
} ColosseumEventPairRow;

/* =========================================================================
 * External declarations
 * ========================================================================= */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   pokemonSetStatus();
#endif
extern void  pokemonGrowBasisStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   itemGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  joutaiDataBiosGetStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  pokemonIsJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  joutaiIsJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  pokemonGetSoubiItemDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   wazaGetStatus(void* ptr, u16 dataId, u16 status, u32 index);
#endif

/* SDA table pointers for event data arrays */
extern u32 lbl_80478D38;   /* Event table count */
extern ColosseumEventRow6 lbl_80478D30[]; /* Event table base (6 bytes per entry) */
extern u32 lbl_80478D28; /* Pair-row table count */
extern ColosseumEventPairRow lbl_80375A08[]; /* 0x18-byte pair rows */

#if !defined(PR424_FIGHT_SEQ_SPLIT) || \
    defined(PR424_FIGHT_SEQ_80211170_802117FC)
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonBiosCopy();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 figthOutPokemonGetPokemonDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetSoubiItemSoubiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightWazaIsHit();
#endif
extern void fightWazaCreate();
extern void fightWazaInit();
extern u32 fightWazaCriticalDataBiosGetBunbo();
extern u8* fightWazaCriticalDataBiosGetPtr(u16 index);
extern void fightWazaBiosCopy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightApplyDamageVariance();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightSeqWazaTypeCalc();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int fightSeqCalcDamage(void* attacker, void* defender, void* side, u16 wazaId,
                       u16 power, u16 type);
#endif
extern u32 fight_waza_critical_data_number;   /* critical-hit table length */
extern u8 lbl_80478D78[8];
extern u32 lbl_8047B610;   /* waza-sequence cursor */
extern u32 ServerStatusFlag;

/*
 * The critical-hit check and the damage calculation below are the bodies of
 * the waza-sequence commands WS_CRITICAL_CHECK (0x80227DB8) and wsCmd05
 * (0x80227C40), which retail expands inline here; their locals get inline
 * numbering, which gives retail's register allocation.
 */
/* RULE-EXCEPTION(user-approved): single-use inline helper (body of WS_CRITICAL_CHECK) — see docs/RULE_EXCEPTIONS.md */
static inline void fightSeqCriticalCheck(void)
{
    void* atk;
    void* def;
    void* atkWaza;
    u16 wazaId;
    u16 item;
    u16 species;
    int lucky;
    int stick;
    u16 crit;
    u8 bunbo;

    atk = fightTargetGetPtrAsNowFightType(0x11, 0);
    fightOutPokemonGetTokuseiDataId(atk);
    item = fightOutPokemonGetSoubiItemSoubiDataId(atk);
    atkWaza = PC_PTR(pokemonGetStatus(atk, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    wazaId = fightOutPokemonGetUseWazaDataId(atk);
    species = figthOutPokemonGetPokemonDataId(atk);
    def = fightTargetGetPtrAsNowFightType(0x12, 0);

    /* Lucky Punch on Chansey, Stick on Farfetch'd. */
    lucky = item == 0x3f && species == SPECIES_CHANSEY;
    stick = item == 0x42 && species == SPECIES_FARFETCHD;
    crit = (fightOutPokemonIsPokemonJoutaiDataId(atk, PC_PTR(0xf)) == 1) * 2
         + ((u16)wazaGetStatus(0, wazaId, 9, 0) == MOVE_EFFECT_HIGH_CRITICAL)
         + ((u16)wazaGetStatus(0, wazaId, 9, 0) == MOVE_EFFECT_SKY_ATTACK)
         + ((u16)wazaGetStatus(0, wazaId, 9, 0) == MOVE_EFFECT_BLAZE_KICK)
         + ((u16)wazaGetStatus(0, wazaId, 9, 0) == MOVE_EFFECT_POISON_TAIL)
         + (item == 0x29)
         + lucky * 2
         + stick * 2;
    if (crit > fight_waza_critical_data_number - 1) {
        crit = fight_waza_critical_data_number - 1;
    }

    if (fightOutPokemonGetTokuseiDataId(def) != ABILITY_BATTLE_ARMOR &&
        fightOutPokemonGetTokuseiDataId(def) != ABILITY_SHELL_ARMOR &&
        (u8)fightFloorGetStatus(0, 0, 0x29, 0) == 1) {
        bunbo = fightWazaCriticalDataBiosGetBunbo(fightWazaCriticalDataBiosGetPtr(crit));
        if (GSrand() % bunbo == 0 ||
            (fightOutPokemonIsPokemonJoutaiDataId(atk, PC_PTR(0x3e)) == 1 && wazaId == MOVE_SHADOW_RUSH &&
             GSrand() % 100 < 90)) {
            wazaSetStatus(atkWaza, 0, 0x2b, 0, 2);
        } else {
            wazaSetStatus(atkWaza, 0, 0x2b, 0, 1);
        }
    } else {
        wazaSetStatus(atkWaza, 0, 0x2b, 0, 1);
    }
    lbl_8047B610++;
}

/* RULE-EXCEPTION(user-approved): single-use inline helper (body of wsCmd05) — see docs/RULE_EXCEPTIONS.md */
static inline void fightSeqDamageCalc(void)
{
    void* atk;
    void* def;
    void* side;
    u16 wazaId;
    void* atkWaza;
    u16 power;
    u16 type;
    int damage;

    atk = fightTargetGetPtrAsNowFightType(0x11, 0);
    def = fightTargetGetPtrAsNowFightType(0x12, 0);
    side = fightTargetGetPtrAsNowFightType(2, PC_PTR32_IMPLICIT(def));
    wazaId = fightOutPokemonGetUseWazaDataId(atk);
    atkWaza = PC_PTR(pokemonGetStatus(atk, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    power = wazaGetStatus(atkWaza, 0, 0x2f, 0);
    type = wazaGetStatus(atkWaza, 0, 0x30, 0);
    damage = fightSeqCalcDamage(PC_PTR32_IMPLICIT(atk), PC_PTR32_IMPLICIT(def), PC_PTR32_IMPLICIT(side), wazaId, power, type);
    damage *= (u8)wazaGetStatus(atkWaza, 0, 0x2b, 0);
    damage *= (u8)wazaGetStatus(atkWaza, 0, 0x2c, 0);
    if (fightOutPokemonIsPokemonJoutaiDataId(atk, PC_PTR(CONDITION_CHARGE)) == 1 && type == 0xd) {
        damage *= 2;
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(atk, PC_PTR(0x32)) == 1) {
        damage = damage * 15 / 10;
    }
    wazaSetStatus(atkWaza, 0, 0x2d, 0, damage);
    lbl_8047B610++;
}

/* Address: 0x80211170 | Size: 0x68c */
u32
fightSeqGetNromalWazaDamage(u32 r3, u32 r4, u32 r5, u32 r6, u8 r7, u8 r8, void *r9, void *r10)
{
    u32 saved610;
    void* waza;
    u32 savedFloor42;
    u32 saved618;
    u32 result;
    int i;
    u8 savedFlags[8];
    u8 savedBios[172];
    u8 savedAtk[1760];
    u8 savedDef[1760];
    u32 savedFloor36;

    savedFloor36 = fightFloorGetStatus(0, 0, 0x36, 0);
    savedFloor42 = fightFloorGetStatus(0, 0, 0x42, 0);
    saved610 = lbl_8047B610;
    saved618 = ServerStatusFlag;
    for (i = 0; i < 8; i++) {
        savedFlags[i] = lbl_80478D78[i];
    }
    fightOutPokemonBiosCopy(savedAtk, PC_PTR(r5));
    fightOutPokemonBiosCopy(savedDef, PC_PTR(r6));
    fightFloorSetStatus(0, 0, 0x36, 0, r5);
    fightFloorSetStatus(0, 0, 0x42, 0, r6);
    waza = PC_PTR(pokemonGetStatus(PC_PTR(r5), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    fightWazaBiosCopy(savedBios, waza);
    fightWazaInit(waza);
    fightWazaCreate(waza, 0, r4, 0, 0);
    ServerStatusFlag = 0;
    for (i = 0; i < 8; i++) {
        lbl_80478D78[i] = 0;
    }
    if (r9 != NULL) {
        ((void (*)())r9)(r3, r4, r5, r6);
    }
    if (r7 == 1) {
        fightSeqCriticalCheck();
    }
    fightSeqDamageCalc();
    fightSeqWazaTypeCalc(1, 1, 1, 0);
    if (r8 == 1) {
        fightApplyDamageVariance(1, 1);
        lbl_8047B610++;
    }
    if (r10 != NULL) {
        ((void (*)())r10)(r3, r4, r5, r6);
    }
    if (fightWazaIsHit(waza) == 1) {
        result = wazaGetStatus(waza, 0, 0x2d, 0);
    } else {
        result = 0;
    }
    fightFloorSetStatus(0, 0, 0x36, 0, savedFloor36);
    fightFloorSetStatus(0, 0, 0x42, 0, savedFloor42);
    fightWazaBiosCopy(waza, savedBios);
    lbl_8047B610 = saved610;
    ServerStatusFlag = saved618;
    for (i = 0; i < 8; i++) {
        lbl_80478D78[i] = savedFlags[i];
    }
    fightOutPokemonBiosCopy(PC_PTR(r5), savedAtk);
    fightOutPokemonBiosCopy(PC_PTR(r6), savedDef);
    return result;
}
#endif

#if !defined(PR424_FIGHT_SEQ_SPLIT)
/* 0x802117FC | size: 0x14 | tiny */
u32 fightSeqGetEffectAminFlag(void) {
    extern u32 ServerStatusFlag;
    return !(ServerStatusFlag & 0x80);
}
#endif

#if !defined(PR424_FIGHT_SEQ_SPLIT) || \
    defined(PR424_FIGHT_SEQ_80211810_80211830)
/* fightSeqSetEffectAminFlag | Size: 0x20 | Set/clear bit 0x80 in flags */
void fightSeqSetEffectAminFlag(u8 enable) {
    extern u32 ServerStatusFlag;
    u32 val;
    u32 result;
    enable = (u8)enable;
    val = ServerStatusFlag;
    result = val | 0x80u;
    if (enable == 1) {
        result = val & ~0x80u;
    }
    ServerStatusFlag = result;
}
#endif

#if !defined(PR424_FIGHT_SEQ_SPLIT) || \
    defined(PR424_FIGHT_SEQ_80211830_802118FC)
/* 0x80211830 | size: 0xCC */
void fightSeqInit(void) {
    extern u8 lbl_80478D78[1];
    extern u32 lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetValidFightSidePtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightSideInitJoutaiDataId();
#endif
    extern s32 _fightSeqInitSubFightOutPokemon__FPvUsPv();
    extern s32 _fightSeqTurnEndSubFightOutPokemon();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightOutPokemonInitOneSelfTurnSub();
#endif
    u8 localBuf[0x10];
    u32 i;
    s32 j;
    void* slotData;

    localBuf[0] = 0;
    fightFloorLoopValidFightOutPokemon(0, PC_PTR32(_fightSeqTurnEndSubFightOutPokemon), &localBuf[0], 0);
    for (i = 0; (i & 0xFFFF) < 2; i++) {
        slotData = fightFloorGetValidFightSidePtr(0, i);
        if (slotData != NULL) {
            fightSideInitJoutaiDataId(PC_PTR32_IMPLICIT(slotData), 0x4D);
        }
    }
    fightFloorLoopValidFightOutPokemon(0, PC_PTR32(_fightOutPokemonInitOneSelfTurnSub), 0, 0);
    for (j = 0; j < 8; j++) {
        lbl_80478D78[j] = 0;
    }
    lbl_8047B62C = 0;
    fightFloorLoopValidFightOutPokemon(0, PC_PTR32(_fightSeqInitSubFightOutPokemon__FPvUsPv), 0, 0);
}
#endif

#if !defined(PR424_FIGHT_SEQ_SPLIT) || \
    defined(PR424_FIGHT_SEQ_802118FC_802119D4)
/* _fightSeqInitSubFightOutPokemon__FPvUsPv | Size: 0x4C | Check state and optionally trigger event 0x11 */
#if !defined(NO_LOCAL_PRAGMAS_FIGHT_SEQ_R54_802118FC_SUFFIX)
#endif
#if !defined(NO_LOCAL_PRAGMAS_FIGHT_SEQ_R54_802118FC_SUFFIX)
#endif
s32 _fightSeqInitSubFightOutPokemon__FPvUsPv(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId(void* ctx, u32 eventId);
#endif
    if (!fightOutPokemonCheckFightOut(ctx)) { /* r3 untouched before the call (assembly) */
        return 1;
    }
    fightOutPokemonWriteJoutaiDataId(ctx, PC_PTR(0x11));
    return 1;
}
#if !defined(NO_LOCAL_PRAGMAS_FIGHT_SEQ_R54_802118FC_SUFFIX)
#endif

/* Address: 0x80211948 | Size: 0x8c | Ghidra import */
void fightSeqFightActionCreateAndFlowFifo(
    void* motoAction, void* actorTarget, u32 kind, u32 buff,
    FightActionData* actionData, void* buffData)
{
  s32 result;
  FightAction action;

  result = fightActionCreate(&action, motoAction, actorTarget, kind, buff,
                             actionData);
  /* RULE-EXCEPTION(user-approved): goto form with a zero-length __memcpy no-op on the
   * fall-through path keeps retail's "beq; b" layout for both tests (the same form
   * _fightCheckKiaipanchiSub uses) — see docs/RULE_EXCEPTIONS.md */
  if ((u8)result == 1) {
    goto set_buff;
  } else {
    __memcpy(&action, &action, 0);
    goto after_set_buff;
  }
set_buff:
  fightActionBiosSetBuffDataPtr(&action, buffData);
  result = 1;
after_set_buff:
  if ((u8)result == 1) {
    goto flow;
  } else {
    __memcpy(&action, &action, 0);
    goto done;
  }
flow:
  fightActionFlowFifo(&action);
done:
  return;
}
#endif

#if !defined(PR424_FIGHT_SEQ_SPLIT)
/* fightSeqPost | Size: 0x2C | Clear bit 20 in flags and call fightSeqTurnEnd */
void fightSeqPost(void) {
    extern u32 ServerStatusFlag;
    extern void fightSeqTurnEnd(void);
    ServerStatusFlag &= ~0x00100000u;
    fightSeqTurnEnd();
}
#endif

#if !defined(PR424_FIGHT_SEQ_SPLIT)
/* Address: 0x80211A00 | Size: 0x78 | Ghidra import */
void fightActionFlowAllFightOutPokemonDoFightActionPost(void)

{
    extern u32 DAT_8038ff5a;
    extern u32 DAT_8038fff9;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysFreeAll();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
    extern u8 lbl_80478D78[8];
    extern u8 lbl_8047B625;
    extern void _fightCheckKiaipanchiSub();
    extern void _fightClearRageSub();

  fightFloorLoopValidFightOutPokemon(0,(u32)_fightCheckKiaipanchiSub,0,1);
  fightFloorLoopValidFightOutPokemon(0,(u32)_fightClearRageSub,0,1);
  lbl_80478D78[3] = 0;
  lbl_8047B625 = 0;
  lbl_80478D78[4] = 0;
  DAT_8038ff5a = 0;
  DAT_8038fff9 = 0;
  wazaSequenceSysFreeAll();
  return;
}
/* 0x80211A78 | size: 0x11C */
u32 _fightCheckKiaipanchiSub(void* ctx) {
    extern u8 lbl_80375D30[];
    extern u8 lbl_803791FE[];
    extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetMotoWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
    u8 localBuf[0x30];
    void* feData;
    FightActionData* d920val;
    u8 result;

    if ((u8)fightOutPokemonCheckFightOut(ctx) == 0) { return 1; }
    feData = pokemonGetStatus(ctx, 0, POKEMON_STATUS_ACTION_PTR, 0);
    if (feData == NULL) { return 1; }
    if ((u8)fightActionCheckValid(feData) == 0) { return 1; }
    if (fightOutPokemonGetMotoWazaDataId(ctx) != 0x108) { goto done; }
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(ctx, CONDITION_SLEEP) != 0) { goto done; }
    if ((u8)(u32)pokemonGetStatus(ctx, 0, POKEMON_STATUS_TRUANT_FLAG, 0) != 0) { goto done; }
    d920val = fightActionBiosGetFightActionDataPtr((FightAction*)lbl_8047B62C);
    result = fightActionCreate((FightAction*)localBuf, d920val, ctx, 0xC, 0,
                               (FightActionData*)lbl_80375D30);
    if (result == 1) {
        fightActionBiosSetBuffDataPtr((FightAction*)localBuf, lbl_803791FE);
        result = 1;
    }
    if (result == 1) {
        fightActionFlowFifo(localBuf);
    }
done:
    return 1;
}

/* Address: 0x80211B94 | Size: 0x284 | Ghidra import */
void wsRunScript(void)

{
    u32 r3;
    u32 r4;
    char r5;

    extern u32 DAT_80378798;
    extern u32 DAT_8038ff5a;
    extern u32 DAT_8038fff9;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
    extern u8 lbl_80478D78[8];
    extern u32 lbl_8047B610;
    extern u8 lbl_8047B614;
    extern u32 ServerStatusFlag;
    extern u8 lbl_8047B625;
    extern u32 lbl_8047B62C;
  u8 uVar1;
  u8 uVar2;
  u32 uVar3;

  u32 uVar4;
  u32 uVar5;
  u32 uVar6;
  u32 uVar7;
  u8 local_38;
  u8 local_37 [15];
  
  uVar4 = lbl_8047B610;
  uVar5 = lbl_8047B62C;
  lbl_8047B610 = r4;
  uVar1 = lbl_8047B614;
  if (r5 == 0) {
    lbl_8047B614 = 0;
  }
  lbl_8047B62C = r3;
  do {
    while (((int (*)())*(u32 *)((u32)*(u8*)lbl_8047B610 * 4 + -0x7fd85ff4))(),
          r5 == 0) {
      if ((lbl_8047B614 == 1) || (lbl_8047B614 == 2))
      goto LAB_0020eddc;
    }
    if (lbl_8047B614 == 1) {
      fightFloorLoopValidFightOutPokemon(0,0x8023011c,0,0);
      uVar6 = lbl_8047B62C;
      uVar2 = lbl_8047B614;
      uVar7 = lbl_8047B610;
      lbl_8047B614 = 0;
      lbl_8047B610 = (u32)(u8 *)&DAT_80378798;
      lbl_8047B62C = uVar6;
      do {
        ((int (*)())*(u32 *)((u32)*(u8*)lbl_8047B610 * 4 + -0x7fd85ff4))();
        if (lbl_8047B614 == 1) break;
      } while ((char)lbl_8047B614 != 2);
      lbl_8047B62C = uVar6;
      lbl_8047B614 = uVar2;
      lbl_8047B610 = uVar7;
      local_37[0] = 1;
      fightFloorLoopValidFightOutPokemon(0,0x8022e34c,local_37,0);
      fightFloorLoopValidFightOutPokemon(0,0x8022e1f8,0,0);
      fightFloorLoopValidFightOutPokemon(0,0x80230088,0,0);
      local_38 = 0;
      fightFloorLoopValidFightOutPokemon(0,0x8022eb9c,&local_38,0);
      lbl_8047B614 = 2;
    }
  } while ((char)lbl_8047B614 != 2);
  uVar6 = fightTargetGetPtrAsNowFightType(0x11,0);
  uVar7 = (int)pokemonGetStatus(uVar6,0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
  fightFloorLoopValidFightOutPokemon(0,0x802136a4,0,0);
  lbl_80478D78[3] = 0;
  ServerStatusFlag = ServerStatusFlag & 0xf1e892af;
  lbl_8047B625 = 0;
  lbl_80478D78[4] = 0;
  DAT_8038ff5a = 0;
  DAT_8038fff9 = 0;
  pokemonSetStatus(uVar6,0,POKEMON_STATUS_LAST_HIT_BY_MOVE,0,0);
  pokemonSetStatus(uVar6,0,POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE,0,9);
  wazaSetStatus(uVar7,0,0x2d,0,0);
LAB_0020eddc:
  if (r5 != 0) {
    uVar3 = ServerStatusFlag;
    ServerStatusFlag = uVar3 & 0xfffffdff;
    ServerStatusFlag = uVar3 & 0xfff7fdff;
  }
  lbl_8047B62C = uVar5;
  lbl_8047B614 = uVar1;
  lbl_8047B610 = uVar4;
  return;
}
/* Address: 0x80211E18 | Size: 0x8ac | Ghidra import */

void fightActionUseItemMain(u32 r3,u32 r4)

{
    extern u32 DAT_80375e24;
    extern u32 DAT_8038ff76;
    extern u32 DAT_8038fffc;
    extern u32 _DAT_80279e80;
    extern u32 _DAT_80279e84;
    extern u32 _DAT_80279e88;
    extern u32 _DAT_80279e8c;
    extern u32 _DAT_80279e94;
    extern u32 _DAT_80375e24;
    extern u32 _DAT_80375e44;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroItemDecItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
    extern s8 itemParamGetHPUp();
    extern s8 itemParamGetConfuseFlag();
    extern s8 itemParamGetParalyzeFlag();
    extern s8 itemParamGetFreezeFlag();
    extern s8 itemParamGetBurnFlag();
    extern s8 itemParamGetPoisonFlag();
    extern s8 itemParamGetSleepFlag();
    extern s8 itemParamGetGuardFlag();
    extern s8 itemParamGetSpAttackUp();
    extern s8 itemParamGetHitUp();
    extern s8 itemParamGetQuickUp();
    extern s8 itemParamGetDefenceUp();
    extern s8 itemParamGetAttackUp();
    extern s8 itemParamGetCriticalFlag();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemParamGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemDataBiosGetItemEffectParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysFreeAll();
#endif
    extern void battleCameraSelect();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    extern u32 fightTrainerGetKindPrefixName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuAllFightTrainerCloseStatusMenu();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuAllFightOutPokemonCloseStatusMenu();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuCloseMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80265598();
#endif
    extern u8 lbl_80478D78[8];
    extern u8 lbl_8047B614;
  u8 bVar1;
  s8 cVar2;
  u16 uVar9;
  u32 uVar3;
  u32 uVar4;
  u32 uVar5;
  u32 uVar6;
  int iVar7;
  short sVar10;
  s8 cVar12;
  u32 uVar8;
  s8 cVar13;
  u16 uVar11;

  u32 uVar14;
  u32 uVar15;
  
  uVar9 = fightFloorGetStatus(0,0,0x14,0);
  uVar3 = fightTargetGetPtrAsNowFightType(0x11,0);
  uVar4 = fightTargetGetPtrAsNowFightType(0x12,0);
  uVar5 = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0,uVar3);
  uVar6 = fightTrainerGetStatus(uVar5,0,0x44,0);
  iVar7 = (int)pokemonGetStatus(uVar3,0,POKEMON_STATUS_BATTLE_ITEM_PTR,0);
  if (iVar7 != 0) {
    itemGetStatus(iVar7,0,0x1f,0);
    sVar10 = itemGetStatus(iVar7,0,0x20,0);
    cVar12 = itemGetStatus(iVar7,0,0x21,0);
    uVar8 = itemParamGetPtr(itemDataBiosGetItemEffectParam(itemDataBiosGetPtr(r4)));
    cVar2 = DAT_8038fffc;
    bVar1 = DAT_8038ff76;
    cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,0x2e);
    if (cVar13 == 1) {
      fightOutPokemonWriteJoutaiDataId(uVar3,0x2e);
    }
    cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,0x15);
    if (cVar13 == 1) {
      fightOutPokemonWriteJoutaiDataId(uVar3,0x15);
    }
    cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,0x28);
    if (cVar13 == 1) {
      fightOutPokemonWriteJoutaiDataId(uVar3,0x28);
    }
    uVar14 = _DAT_80375e24;
    if (cVar12 != 1) {
      cVar13 = itemGetStatus(0,r4,2,0);
      if (cVar13 == 1) {
        uVar14 = *(u32 *)((r4 & 0xffff) * 4 + -0x7fc8a210);
      }
      else {
        uVar14 = _DAT_80375e44;
        if (((r4 & 0xffff) != 0x50) && ((r4 & 0xffff) != 0x51)) {
          iVar7 = itemParamGetPtr(itemDataBiosGetItemEffectParam(itemDataBiosGetPtr(r4)));
          if (iVar7 == 0) {
            uVar15 = 7;
          }
          else if ((r4 & 0xffff) == 0x13) {
            uVar15 = 1;
          }
          else {
            cVar13 = itemParamGetHPUp();
            if (cVar13 == 0) {
              cVar13 = itemParamGetSleepFlag(iVar7);
              if ((((cVar13 == 1) || (cVar13 = itemParamGetPoisonFlag(iVar7), cVar13 == 1)) ||
                  (cVar13 = itemParamGetBurnFlag(iVar7), cVar13 == 1)) ||
                 (((cVar13 = itemParamGetFreezeFlag(iVar7), cVar13 == 1 ||
                   (cVar13 = itemParamGetParalyzeFlag(iVar7), cVar13 == 1)) ||
                  (cVar13 = itemParamGetConfuseFlag(iVar7), cVar13 == 1)))) {
                uVar15 = 3;
              }
              else {
                cVar13 = itemParamGetCriticalFlag(iVar7);
                if (cVar13 == 1) {
                  uVar15 = 4;
                }
                else {
                  cVar13 = itemParamGetAttackUp(iVar7);
                  if (((cVar13 == 0) && (cVar13 = itemParamGetDefenceUp(iVar7), cVar13 == 0)) &&
                     ((cVar13 = itemParamGetQuickUp(iVar7), cVar13 == 0 &&
                      ((cVar13 = itemParamGetHitUp(iVar7), cVar13 == 0 &&
                       (cVar13 = itemParamGetSpAttackUp(iVar7), cVar13 == 0)))))) {
                    cVar13 = itemParamGetGuardFlag(iVar7);
                    if (cVar13 == 1) {
                      uVar15 = 6;
                    }
                    else {
                      uVar15 = 7;
                    }
                  }
                  else {
                    uVar15 = 5;
                  }
                }
              }
            }
            else {
              uVar15 = 2;
            }
          }
          uVar14 = _DAT_80375e24;
          if (uVar15 != 7) {
            fightFloorSetStatus(0,0,0x4b,0,uVar3);
            lbl_80478D78[5] = 0;
            if (uVar15 == 4) {
              lbl_80478D78[5] = 5;
            }
            else if (uVar15 < 4) {
              if (2 < uVar15) {
                cVar13 = itemParamGetSleepFlag(uVar8);
                if ((((cVar13 == 1) && (cVar13 = itemParamGetPoisonFlag(uVar8), cVar13 == 1)) &&
                    (cVar13 = itemParamGetBurnFlag(uVar8), cVar13 == 1)) &&
                   ((cVar13 = itemParamGetFreezeFlag(uVar8), cVar13 == 1 &&
                    (cVar13 = itemParamGetParalyzeFlag(uVar8), cVar13 == 1)))) {
                  cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,CONDITION_SLEEP);
                  if (cVar13 == 1) {
                    lbl_80478D78[5] = 5;
                  }
                  else {
                    cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,CONDITION_POISON);
                    if (cVar13 == 1) {
                      lbl_80478D78[5] = 4;
                    }
                    else {
                      cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,CONDITION_BAD_POISON);
                      if (cVar13 == 1) {
                        lbl_80478D78[5] = 4;
                      }
                      else {
                        cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,CONDITION_BURN);
                        if (cVar13 == 1) {
                          lbl_80478D78[5] = 3;
                        }
                        else {
                          cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,CONDITION_FREEZE);
                          if (cVar13 == 1) {
                            lbl_80478D78[5] = 2;
                          }
                          else {
                            cVar13 = fightOutPokemonIsPokemonJoutaiDataId(uVar3,CONDITION_PARALYSIS);
                            if (cVar13 == 1) {
                              lbl_80478D78[5] = 1;
                            }
                          }
                        }
                      }
                    }
                  }
                }
                else {
                  cVar13 = itemParamGetSleepFlag(uVar8);
                  if (cVar13 == 1) {
                    lbl_80478D78[5] = 5;
                  }
                  else {
                    cVar13 = itemParamGetPoisonFlag(uVar8);
                    if (cVar13 == 1) {
                      lbl_80478D78[5] = 4;
                    }
                    else {
                      cVar13 = itemParamGetBurnFlag(uVar8);
                      if (cVar13 == 1) {
                        lbl_80478D78[5] = 3;
                      }
                      else {
                        cVar13 = itemParamGetFreezeFlag(uVar8);
                        if (cVar13 == 1) {
                          lbl_80478D78[5] = 2;
                        }
                        else {
                          cVar13 = itemParamGetParalyzeFlag(uVar8);
                          if (cVar13 == 1) {
                            lbl_80478D78[5] = 1;
                          }
                          else {
                            cVar13 = itemParamGetConfuseFlag(uVar8);
                            if (cVar13 == 1) {
                              lbl_80478D78[5] = 0;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
            else if ((uVar15 != 6) && (uVar15 < 6)) {
              lbl_80478D78[5] = 4;
              uVar11 = itemParamGetAttackUp(uVar8);
              uVar11 = uVar11 & 0xff;
              if (uVar11 == 0) {
                uVar11 = itemParamGetDefenceUp(uVar8);
                uVar11 = uVar11 & 0xff;
                if (uVar11 == 0) {
                  uVar11 = itemParamGetQuickUp(uVar8);
                  uVar11 = uVar11 & 0xff;
                  if (uVar11 == 0) {
                    uVar11 = itemParamGetHitUp(uVar8);
                    uVar11 = uVar11 & 0xff;
                    if (uVar11 == 0) {
                      uVar11 = itemParamGetSpAttackUp(uVar8);
                      uVar11 = uVar11 & 0xff;
                      if (uVar11 != 0) {
                        uVar3 = GSmsgGetGSchar(_DAT_80279e8c);
                        msgctrlSetValue(0xd,uVar3);
                        DAT_8038ff76 = 4;
                      }
                    }
                    else {
                      uVar3 = GSmsgGetGSchar(_DAT_80279e94);
                      msgctrlSetValue(0xd,uVar3);
                      DAT_8038ff76 = 6;
                    }
                  }
                  else {
                    uVar3 = GSmsgGetGSchar(_DAT_80279e88);
                    msgctrlSetValue(0xd,uVar3);
                    DAT_8038ff76 = 3;
                  }
                }
                else {
                  uVar3 = GSmsgGetGSchar(_DAT_80279e84);
                  msgctrlSetValue(0xd,uVar3);
                  DAT_8038ff76 = 2;
                }
              }
              else {
                uVar3 = GSmsgGetGSchar(_DAT_80279e80);
                msgctrlSetValue(0xd,uVar3);
                DAT_8038ff76 = 1;
              }
              if ((uVar11 == 1) || (uVar11 == 0xffff)) {
                uVar3 = GSmsgGetGSchar(0x76bd);
                msgctrlSetValue(0xe,uVar3);
                DAT_8038fffc = (DAT_8038ff76 & 0xf) + 0xe;
              }
              else {
                uVar3 = GSmsgGetGSchar(0x7626);
                msgctrlSetValue(0xe,uVar3);
                DAT_8038fffc = (DAT_8038ff76 & 0xf) + 0x26;
              }
              uVar3 = GSmsgGetGSchar(0x7627);
              msgctrlSetValue(0x41,uVar3);
            }
            uVar14 = *(u32 *)(&DAT_80375e24 + uVar15 * 4);
          }
        }
      }
    }
    lbl_8047B614 = 0;
    uVar3 = fightTrainerGetKindPrefixName(uVar5);
    msgctrlSetValue(0x22,uVar3);
    uVar3 = fightTrainerGetNamePtr(uVar5);
    msgctrlSetValue(0x23,uVar3);
    uVar3 = fightTrainerGetNamePtr(uVar5);
    msgctrlSetValue(0x13,uVar3);
    uVar3 = GSmsgGetGSchar(itemGetStatus(0,r4 & 0xffff,1,0));
    msgctrlSetValue(0x29,uVar3);
    battleCameraSelect(1);
    if (cVar12 == 0) {
      fn_80265598(uVar4,uVar9,1);
    }
    wsRunScript(r3,uVar14,1);
    if (cVar12 == 0) {
      heroItemDecItemDataId(uVar6,r4,1,(int)sVar10);
    }
    DAT_8038ff76 = bVar1;
    DAT_8038fffc = cVar2;
    wazaSequenceSysFreeAll();
    fightMenuAllFightTrainerCloseStatusMenu(0);
    fightMenuAllFightOutPokemonCloseStatusMenu(0);
    fightMenuCloseMsg();
  }
  return;
}
#endif
