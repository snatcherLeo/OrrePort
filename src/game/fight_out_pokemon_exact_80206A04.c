/**
 * @file fight_out_pokemon_exact_80206A04.c
 * @brief Matching fightOutPokemon + fightPokemon range
 *        0x80206A04-0x80207C6C, 17 functions.
 *
 * OutPokemon/Pokemon field accessors, sequence/status writers, and
 * damage-calc support the seq/waza layers call into (statusGetStatus,
 * fadeEffectGetRandom callers, etc). Corresponds to XD's
 * fight.cpp fightOutPokemon+fightPokemon cluster (0x80200644-0x80208288).
 */

#include "game/colosseum.h"
#include "game/constants/pokemon_status.h"
#include "game/trainer.h"
#include "game/pokemon.h"

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

typedef struct StatusIdTable7 {
    u16 id[7];
} StatusIdTable7;

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
extern void* fightActionGetPri(void* p);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif

/* SDA table pointers for event data arrays */
extern u32 lbl_80478D38;   /* Event table count */
extern ColosseumEventRow6 lbl_80478D30[]; /* Event table base (6 bytes per entry) */
extern u32 lbl_80478D28; /* Pair-row table count */
extern ColosseumEventPairRow lbl_80375A08[]; /* 0x18-byte pair rows */

/* Retail marks every function in this physical island active. */
#pragma force_active on

/* Address: 0x80206A04 | Size: 0xe8 | Ghidra import */
u32 fightPokemonCheckValid(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
    u32 uVar1;
    u16 sVar3;
    void* iVar2;
    u8 cVar4;
    if (ctx == 0) { return 0; }
    sVar3 = std103_battleGetResult();
    if (sVar3 == 1) { return 0; }
    iVar2 = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0));
    if (iVar2 == 0) { return 0; }
    cVar4 = pokemonCheckValid(iVar2);
    if (cVar4 == 0) { return 0; }
    if (ctx == 0) {
        iVar2 = 0;
    } else {
        iVar2 = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
    }
    if (iVar2 == 0) { return 0; }
    cVar4 = pokemonCheckValid(iVar2);
    if (cVar4 == 0) { return 0; }
    uVar1 = (u32)pokemonGetStatus(ctx, 0, POKEMON_STATUS_ENTRY_ID, 0);
    return uVar1 >> 0x1f ^ 1;
}

/* Address: 0x80206AEC | Size: 0x150 | Ghidra import */
void fightPokemonCreate(void* p1, void* p2, s16 p3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosCopy();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit();
#endif
    u32 uVar1;

    if (p1 != 0 && p2 != 0) {
        if (p1 != 0) {
            pokemonSetStatus(p1, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0, 0);

            pokemonInit(PC_PTR(pokemonGetStatus(p1, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0)));
            uVar1 = (u32)pokemonGetStatus(p1, 0, POKEMON_STATUS_BATTLE_CONDITION_SLOT, 0);
            joutaiInitAry(PC_PTR(uVar1), 1);
            pokemonSetStatus(p1, 0, POKEMON_STATUS_ENTRY_ID, 0, PC_PTR32_IMPLICIT((void*)0xffffffff));
            pokemonSetStatus(p1, 0, 0xcf, 0, 0);
            pokemonSetStatus(p1, 0, POKEMON_STATUS_LEVEL_UP_FLAG, 0, 0);
            pokemonSetStatus(p1, 0, 0xd1, 0, 0);
            pokemonSetStatus(p1, 0, POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG, 0, 0);
        }
        pokemonSetStatus(p1, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0, PC_PTR32_IMPLICIT(p2));
        uVar1 = (u32)pokemonGetStatus(p1, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
        pokemonBiosCopy(PC_PTR(uVar1), p2);
        pokemonSetStatus(p1, 0, POKEMON_STATUS_ENTRY_ID, 0, (s32)p3);
    }
}

/* 0x80206C3C | size: 0x58 | small */
void fightOutPokemonInitAry(u32 param_1, u16 param_2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonInit(u32);
#endif
    u32 n;
    int iVar1;

    if (param_1 != 0) {
        iVar1 = 0;
        n = param_2;
        for (; (u16)iVar1 < n; iVar1 = iVar1 + 1) {
            fightOutPokemonInit(param_1 + (u32)(u16)iVar1 * 0x6E0);
        }
    }
}

/* Address: 0x80206C94 | Size: 0x72c | Ghidra import */
void fightOutPokemonInit(int r3)

{
    extern StatusIdTable7 lbl_80279C60;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit();
#endif
    extern void fightActionInit();
    extern void fn_801FD830();
    extern void fightWazaInit();
    extern void fightItemInit();
    extern void fightOutPokemonEnemyInitAry();
  u32 uVar2;
  u32 *puVar3;
  u8 i;
  u8 j;
  u8 k;
  void* iVar1;
  StatusIdTable7 local_28;

  if (r3) {
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_BATTLE_POKEMON_PTR,0,0);
    if ((iVar1 = PC_PTR(pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_TRANSFORM_COPY_PTR,0))) != NULL) {
      pokemonSetStatus(iVar1,0,POKEMON_STATUS_PARTY_RECORD_PTR,0,0);
      pokemonInit(PC_PTR(pokemonGetStatus(iVar1,0,POKEMON_STATUS_RECORD_COPY_PTR,0)));
      uVar2 = (u32)pokemonGetStatus(iVar1,0,POKEMON_STATUS_BATTLE_CONDITION_SLOT,0);
      joutaiInitAry(PC_PTR(uVar2),1);
      pokemonSetStatus(iVar1,0,POKEMON_STATUS_ENTRY_ID,0, PC_PTR32_IMPLICIT((void*)0xffffffff));
      pokemonSetStatus(iVar1,0,0xcf,0,0);
      pokemonSetStatus(iVar1,0,POKEMON_STATUS_LEVEL_UP_FLAG,0,0);
      pokemonSetStatus(iVar1,0,0xd1,0,0);
      pokemonSetStatus(iVar1,0,POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG,0,0);
    }
    uVar2 = (int)pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_FIELD_CONDITION_SLOT,0);
    joutaiInitAry(PC_PTR(uVar2),0x34);
    local_28 = lbl_80279C60;
    for (i = 0; i < 7; i++) {
      pokemonSetStatus(PC_PTR(r3),0,local_28.id[i],0,6);
    }
    fn_801FD830(r3,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_TURNS_ON_FIELD,0,2);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SEQUENCE_PTR,0,0);
    for (j = 0; j < 0xc; j++) {
      pokemonSetStatus(PC_PTR(r3),0,0xfd,j, PC_PTR32_IMPLICIT((void*)0xffffffff));
    }
    iVar1 = PC_PTR(pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_ACTION_PTR,0));
    if (iVar1 != NULL) {
      fightActionInit(iVar1);
      fightWazaInit(pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0));
      fightItemInit(pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_BATTLE_ITEM_PTR,0));
    }
    fightWazaInit(pokemonGetStatus(PC_PTR(r3),0,0xf8,0));
    for (k = 0; k < 2; k++) {
      pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_BATTLE_TYPE,k,9);
    }
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_BATTLE_ABILITY,0,0);
    puVar3 = (u32 *)PC_PTR(pokemonGetStatus(PC_PTR(r3),0,0x101,0));
    if (puVar3 != (void *)0) {
      *puVar3 = 0;
    }
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SKETCH_MOVE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_LAST_SELECTED_MOVE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_LAST_USED_MOVE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0xf2,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_LAST_HIT_BY_MOVE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE,0,9);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_BIDE_DAMAGE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_BIDE_TARGET,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_MIRROR_MOVE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_TRUANT_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_PROTECT_COUNT,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0xfb,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_PHYSICAL_DAMAGE_TAKEN,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_PHYSICAL_DAMAGE_FROM,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SPECIAL_DAMAGE_TAKEN,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SPECIAL_DAMAGE_FROM,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_FULLY_PARALYZED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_HURT_ITSELF_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x108,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_CHARGING_MOVE_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x10a,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_IMPRISON_BLOCKED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_INFATUATED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_DISABLE_BLOCKED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_TAUNT_BLOCKED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_TORMENT_BLOCKED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_FLINCHED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x111,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x112,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_CLEAR_BODY_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_LIGHTNINGROD_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x115,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_INTIMIDATE_DONE_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_TRACE_DONE_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x118,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SWITCHED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_FOCUS_BAND_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SHELL_BELL_DAMAGE,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x11c,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x11d,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x11e,0,0);
    pokemonSetStatus(PC_PTR(r3),0,0x11f,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_FAINTED_FLAG,0,0);
    pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_SWITCH_TARGET,0, PC_PTR32_IMPLICIT((void*)0xffffffff));
    uVar2 = (int)pokemonGetStatus(PC_PTR(r3),0,0x122,0);
    fightOutPokemonEnemyInitAry(uVar2,4);
  }
  return;
}

/* Address: 0x802073C0 | Size: 0x88 | Ghidra import */
void fightOutPokemonInitAbiCntAll(u32 r3)

{
    extern StatusIdTable7 lbl_80279C60;
  u8 bVar1;
  StatusIdTable7 local_28;

  local_28 = lbl_80279C60;
  for (bVar1 = 0; bVar1 < 7; bVar1++) {
    pokemonSetStatus(PC_PTR(r3),0,local_28.id[bVar1],0,6);
  }
  return;
}

/* 0x80207448 | size: 0x15C | medium */
void fightOutPokemonInitOneSelfTurn(void* param_1) {
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_CLEAR_BODY_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_LIGHTNINGROD_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, 0x115, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_INTIMIDATE_DONE_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_TRACE_DONE_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, 0x118, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_SWITCHED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_FOCUS_BAND_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0, 0);
    pokemonSetStatus(param_1, 0, 0x11C, 0, 0);
    pokemonSetStatus(param_1, 0, 0x11D, 0, 0);
    pokemonSetStatus(param_1, 0, 0x11E, 0, 0);
    pokemonSetStatus(param_1, 0, 0x11F, 0, 0);
}

/* 0x802075A4 | size: 0x1BC | medium */
void fightOutPokemonInitOneTurn(void* param_1) {
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_PHYSICAL_DAMAGE_TAKEN, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_PHYSICAL_DAMAGE_FROM, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_SPECIAL_DAMAGE_TAKEN, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_SPECIAL_DAMAGE_FROM, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_FULLY_PARALYZED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_HURT_ITSELF_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, 0x108, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_CHARGING_MOVE_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, 0x10A, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_IMPRISON_BLOCKED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_INFATUATED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_DISABLE_BLOCKED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_TAUNT_BLOCKED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_TORMENT_BLOCKED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, POKEMON_STATUS_FLINCHED_FLAG, 0, 0);
    pokemonSetStatus(param_1, 0, 0x111, 0, 0);
    pokemonSetStatus(param_1, 0, 0x112, 0, 0);
}

/* 0x80207760 | size: 0x74 | small */
void fightOutPokemonInitFightActionBuff(void* param_1) {
    extern void fightActionInit(void*);
    extern void fightWazaInit(void*);
    extern void fightItemInit(void*);
    void* iVar1;

    iVar1 = PC_PTR(pokemonGetStatus(param_1, 0, POKEMON_STATUS_ACTION_PTR, 0));
    if (iVar1 != NULL) {
        fightActionInit(iVar1);
        fightWazaInit(PC_PTR(pokemonGetStatus(param_1, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0)));
        fightItemInit(PC_PTR(pokemonGetStatus(param_1, 0, POKEMON_STATUS_BATTLE_ITEM_PTR, 0)));
    }
}

/* 0x802077D4 | size: 0x11C */
void fightPokemonInitAry(void* basePtr, u16 count) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit();
#endif
    void* entry;
    u16 i;

    if (basePtr == NULL) { return; }
    for (i = 0; i < count; i++) {
        entry = (void*)PC_PTR((PC_PTR32(basePtr) + i * 0x154));
        if (entry == NULL) { continue; }
        pokemonSetStatus(entry, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0, 0);
        pokemonInit(PC_PTR(pokemonGetStatus(entry, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0)));
        joutaiInitAry(PC_PTR(pokemonGetStatus(entry, 0, POKEMON_STATUS_BATTLE_CONDITION_SLOT, 0)), 1);
        pokemonSetStatus(entry, 0, POKEMON_STATUS_ENTRY_ID, 0, (u32)-1);
        pokemonSetStatus(entry, 0, 0xCF, 0, 0);
        pokemonSetStatus(entry, 0, POKEMON_STATUS_LEVEL_UP_FLAG, 0, 0);
        pokemonSetStatus(entry, 0, 0xD1, 0, 0);
        pokemonSetStatus(entry, 0, POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG, 0, 0);
    }
}

/* Address: 0x802078F0 | Size: 0xec | Ghidra import */
void fightPokemonInit(void* r3)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit();
#endif
    void* ctx;
    u32 uVar1;

    if ((ctx = r3) != NULL) {
        pokemonSetStatus(ctx, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0, 0);

        pokemonInit(PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0)));
        uVar1 = (u32)pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_CONDITION_SLOT, 0);
        joutaiInitAry(PC_PTR(uVar1), 1);
        pokemonSetStatus(ctx, 0, POKEMON_STATUS_ENTRY_ID, 0, PC_PTR32_IMPLICIT((void*)0xffffffff));
        pokemonSetStatus(ctx, 0, 0xcf, 0, 0);
        pokemonSetStatus(ctx, 0, POKEMON_STATUS_LEVEL_UP_FLAG, 0, 0);
        pokemonSetStatus(ctx, 0, 0xd1, 0, 0);
        pokemonSetStatus(ctx, 0, POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG, 0, 0);
    }
}

/* 0x802079DC | size: 0x104 */
u32 fightOutPokemonGetTeikouZokuseiDataIdAry(void* ctx, void* battleCtx, u32* outSlots) {
    extern u16 zokuseiGetWazaJoutai(void*, u16);
    u16 i;
    int outCount;
    u16 slot0;
    u16 slot1;
    u16 result;
    u8 isPlayerSlot;

    for (i = 0; i < 0x12; i++) {
        outSlots[i] = (u32)-1;
    }
    outCount = 0;
    for (i = 0; i < 0x12; i++) {
        if (i == (u16)(u32)pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_TYPE, 0)) {
            goto _set1;
        }
        if (i == (u16)(u32)pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_TYPE, 1)) {
        _set1:
            isPlayerSlot = 1;
        } else {
            isPlayerSlot = 0;
        }
        if (isPlayerSlot == 1) { continue; }
        result = zokuseiGetWazaJoutai(battleCtx, i);
        if (result == 0x42 || result == 0x43) {
            outSlots[(u16)outCount] = i;
            outCount++;
        }
    }
    return outCount;
}

/* Address: 0x80207AE0 | Size: 0x7c | Ghidra import */
u32 fightOutPokemonIsZokuseiDataId(void* r3, u16 r4)

{
  u16 sVar2;
  u32 uVar1;

  sVar2 = (u16)(u32)pokemonGetStatus(r3,0,POKEMON_STATUS_BATTLE_TYPE,0);
  if ((r4 == sVar2) || (sVar2 = (u16)(u32)pokemonGetStatus(r3,0,POKEMON_STATUS_BATTLE_TYPE,1), r4 == sVar2)) {
    uVar1 = 1;
  }
  else {
    uVar1 = 0;
  }
  return uVar1;
}

/* 0x80207B5C | size: 0x30 */
u32 fightOutPokemonSetZokuseiDataId(void* context, u8 flags, u16 value) {
#ifdef TARGET_PC /* no caller uses this function's result (all C callers are statements): unused, likely */
    pokemonSetStatus(context, 0, POKEMON_STATUS_BATTLE_TYPE, flags, value);
    return 0;
#else
    return pokemonSetStatus(context, 0, POKEMON_STATUS_BATTLE_TYPE, flags, value);
#endif
}

/* 0x80207B8C | size: 0x34 */
u16 fightOutPokemonGetZokuseiDataId(void* context, u8 field) {
    return (u16)(u32)pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_TYPE, field);
}

/* 0x80207BC0 | size: 0x34 */
u32 fightOutPokemonSetTokuseiDataId(void* context, u16 value) {
#ifdef TARGET_PC /* no caller uses this function's result (all C callers are statements): unused, likely */
    pokemonSetStatus(context, 0, POKEMON_STATUS_BATTLE_ABILITY, 0, value);
    return 0;
#else
    return pokemonSetStatus(context, 0, POKEMON_STATUS_BATTLE_ABILITY, 0, value);
#endif
}

/* 0x80207BF4 | size: 0x30 */
u16 fightOutPokemonGetTokuseiDataId(void* context) {
    return (u16)(u32)pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_ABILITY, 0);
}

/* 0x80207C24 | size: 0x48 | small */
void fightOutPokemonSetWazaEffectDownFlag(void* ctx, u32 param) {
    extern void wazaSequenceSetDownFlag();
    void* obj = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
    if (obj != 0) {
        wazaSequenceSetDownFlag(obj, param);
    }
}
