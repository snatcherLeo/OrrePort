/**
 * @file fight_out_pokemon.c
 * @brief Residual fightOutPokemon candidate range
 *        0x80203EDC-0x80206A04, 34 functions.
 *
 * OutPokemon/Pokemon field accessors, sequence/status writers, and
 * damage-calc support the seq/waza layers call into (statusGetStatus,
 * fadeEffectGetRandom callers, etc). Corresponds to XD's
 * fight.cpp fightOutPokemon+fightPokemon cluster (0x80200644-0x80208288).
 */

#include "game/colosseum.h"
#include "game/fight_action.h"
#include "game/trainer.h"
#include "game/pokemon.h"
#include "game/constants/abilities.h"
#include "game/constants/pokemon_status.h"

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
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif

/* SDA table pointers for event data arrays */
extern u32 lbl_80478D38;   /* Event table count */
extern ColosseumEventRow6 lbl_80478D30[]; /* Event table base (6 bytes per entry) */
extern u32 lbl_80478D28; /* Pair-row table count */
extern ColosseumEventPairRow lbl_80375A08[]; /* 0x18-byte pair rows */

/* Static copies of this TU's accessors that the fightOutPokemon and fightPokemon
 * getters expand (XD calls them out of line): fightPokemonGetPokemonPtr
 * (0x80205BE8) and the equipped-item state check on status 0x3D. */
static inline void* fightPokemonGetPokemonPtrInline(void* fp)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus();
#endif
    void* p;

    if (fp == NULL) {
        p = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(fp, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        p = tmp;
    }
    return p;
}

static inline u8 fightPokemonCheckJoutaiInline(void* fp, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        return pokemonIsJoutaiDataId(fightPokemonGetPokemonPtrInline(fp), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xCD) {
        return 0;
    }
    return joutaiIsJoutaiDataId(fp, id);
}

/* fightPokemonCheckValid (0x80206A04, the next object), expanded here.
 * pokemonCheckValid takes the pokemon pointer, and the 0xCC lookups go
 * through fightPokemonGetPokemonPtr (here and in fightPokemonCheckFightOut).
 * The code is the same as the open-coded ternary, but the inline's own
 * result copies are extra r3-precoloured nodes. They raise the enemy
 * pointer's degree in fightOutPokemonSetMeetEnemyFightPokemonEnemySideAll
 * to exactly K (29) at the simplify stall, so it is pushed right after the
 * trainer count and coloured before it (retail r25/r24). */
static inline u8 fightPokemonCheckValidInline(void* fp)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
    void* p;

    if (fp == NULL) {
        return 0;
    }
    if (std103_battleGetResult() == 1) {
        return 0;
    }
    p = PC_PTR(pokemonGetStatus(fp, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0));
    if (p == NULL) {
        return 0;
    }
    if (pokemonCheckValid(p) == 0) {
        return 0;
    }
    p = fightPokemonGetPokemonPtrInline(fp);
    if (p == NULL) {
        return 0;
    }
    if (pokemonCheckValid(p) == 0) {
        return 0;
    }
    if ((s32)pokemonGetStatus(fp, 0, POKEMON_STATUS_ENTRY_ID, 0) < 0) {
        return 0;
    }
    return 1;
}

/* fightPokemonCheckFightOut (0x80206608) and fightOutPokemonCheckValid
 * (0x80206780), expanded by fightOutPokemonCheckFightOut before them. */
static inline u8 fightPokemonCheckFightOutInline(void* fp)
{
    extern u8 pokemonCheckFightOut();
    void* p;

    if (fp == NULL) {
        return 0;
    }
    if (fightPokemonCheckValidInline(fp) == 0) {
        return 0;
    }
    if ((s32)pokemonGetStatus(fp, 0, POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG, 0) == 1) {
        return 0;
    }
    p = fightPokemonGetPokemonPtrInline(fp);
    if (p == NULL) {
        return 0;
    }
    if (pokemonCheckFightOut(p) == 0) {
        return 0;
    }
    return 1;
}

static inline u8 fightOutPokemonCheckValidInline(void* p1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
    void* fp;

    if (p1 == NULL) {
        return 0;
    }
    if (std103_battleGetResult() == 1) {
        return 0;
    }
    fp = PC_PTR(pokemonGetStatus(p1, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (fp == NULL) {
        return 0;
    }
    if (fightPokemonCheckValidInline(fp) == 0) {
        return 0;
    }
    return 1;
}

void _fightOutPokemonCheckFightActionSelectSub__FP15FightOutPokemonUsUs(void* param_1, u32 param_2, u32 param_3);

static inline u8 fightOutPokemonCheckFightOutInline(void* fo)
{
    if (fo == NULL) {
        return 0;
    }
    if (fightOutPokemonCheckValidInline(fo) == 0) {
        return 0;
    }
    if ((s32)pokemonGetStatus(fo, 0, POKEMON_STATUS_FAINTED_FLAG, 0) == 1) {
        return 0;
    }
    if (fightPokemonCheckFightOutInline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0))) == 0) {
        return 0;
    }
    return 1;
}

/* 0x80203EDC | size: 0x108 */
u16 figthOutPokemonGetSoubiItemBuff(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
    extern u32 pokemonGetSoubiItemBuff();
    void* ccData;
    void* d6Data;
    u8 result;

    d6Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (d6Data == NULL) {
        ccData = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        ccData = tmp;
    }
    if (ccData == NULL) { return 0; }
    if (joutaiDataBiosGetStatus(0x3D) == 0x7C || joutaiDataBiosGetStatus(0x3D) == 0xC8) {
        void* data;

        if (d6Data == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        result = pokemonIsJoutaiDataId(data, 0x3D);
    } else {
        if (joutaiDataBiosGetStatus(0x3D) != 0xCD) {
            result = 0;
        } else {
            result = joutaiIsJoutaiDataId(d6Data, 0x3D);
        }
    }
    if (result == 1) { return 0; }
    return pokemonGetSoubiItemBuff(ccData);
}

/* 0x80203FE4 | size: 0x104 */
u32 fightOutPokemonGetSoubiItemSoubiDataId(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemSoubiDataId();
#endif
void* ccData;
void* d6Data;
u8 result;

    d6Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (d6Data == NULL) {
        ccData = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        ccData = tmp;
    }
    if (ccData == NULL) { return 0; }
    if (joutaiDataBiosGetStatus(0x3D) == 0x7C || joutaiDataBiosGetStatus(0x3D) == 0xC8) {
        void* data;

        if (d6Data == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        result = pokemonIsJoutaiDataId(data, 0x3D);
    } else if (joutaiDataBiosGetStatus(0x3D) != 0xCD) {
        result = 0;
    } else {
        result = joutaiIsJoutaiDataId(d6Data, 0x3D);
    }
    if (result == 1) { return 0; }
    return pokemonGetSoubiItemSoubiDataId(ccData);
}



u32 fightOutPokemonGetSoubiItemDataId(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemDataId();
#endif
    void* ccData;
    void* d6Data;
    u8 result;

    d6Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (d6Data == NULL) {
        ccData = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        ccData = tmp;
    }
    if (ccData == NULL) { return 0; }
    if (joutaiDataBiosGetStatus(0x3D) == 0x7C || joutaiDataBiosGetStatus(0x3D) == 0xC8) {
        void* data;

        if (d6Data == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        result = pokemonIsJoutaiDataId(data, 0x3D);
    } else if (joutaiDataBiosGetStatus(0x3D) != 0xCD) {
        result = 0;
    } else {
        result = joutaiIsJoutaiDataId(d6Data, 0x3D);
    }
    if (result == 1) { return 0; }
    return pokemonGetSoubiItemDataId(PC_PTR32_IMPLICIT(ccData));
}


/* 0x802041EC | size: 0xF4 | medium */
u32 fightPokemonGetSoubiItemSoubiDataId(void* param_1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemSoubiDataId(void*);
#endif
    void* item;

    if (param_1 == NULL) {
        item = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(param_1, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        item = tmp;
    }
    if (item == NULL) {
        return 0;
    }
    if (fightPokemonCheckJoutaiInline(param_1, 0x3D) == 1) {
        return 0;
    }
    return pokemonGetSoubiItemSoubiDataId(item);
}

/* 0x802042E0 | size: 0xF4 | medium */
u32 fightPokemonGetSoubiItemDataId(void* param_1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemDataId(void*);
#endif
    void* item;

    if (param_1 == NULL) {
        item = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(param_1, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        item = tmp;
    }
    if (item == NULL) {
        return 0;
    }
    if (fightPokemonCheckJoutaiInline(param_1, 0x3D) == 1) {
        return 0;
    }
    return pokemonGetSoubiItemDataId(PC_PTR32_IMPLICIT(item));
}

/* Address: 0x802043D4 | Size: 0x480 | Ghidra import */
/* fightOutPokemonGetPokemonPtr (0x80205BAC) and the status check of a
 * fightOutPokemon, expanded by the callers before them. */
static inline u32 figthOutPokemonGetSoubiItemBuffInline(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
    extern u32 pokemonGetSoubiItemBuff();
    void* ccData;
    void* d6Data;
    u8 result;

    d6Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (d6Data == NULL) {
        ccData = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        ccData = tmp;
    }
    if (ccData == NULL) { return 0; }
    if (joutaiDataBiosGetStatus(0x3D) == 0x7C || joutaiDataBiosGetStatus(0x3D) == 0xC8) {
        void* data;

        if (d6Data == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        result = pokemonIsJoutaiDataId(data, 0x3D);
    } else {
        if (joutaiDataBiosGetStatus(0x3D) != 0xCD) {
            result = 0;
        } else {
            result = joutaiIsJoutaiDataId(d6Data, 0x3D);
        }
    }
    if (result == 1) { return 0; }
    return (u16)pokemonGetSoubiItemBuff(ccData);
}

static inline u32 fightOutPokemonGetSoubiItemSoubiDataIdInline(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemSoubiDataId();
#endif
void* d6Data;
void* ccData;
u8 result;

    d6Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (d6Data == NULL) {
        ccData = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        ccData = tmp;
    }
    if (ccData == NULL) { return 0; }
    if (joutaiDataBiosGetStatus(0x3D) == 0x7C || joutaiDataBiosGetStatus(0x3D) == 0xC8) {
        void* data;

        if (d6Data == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(d6Data, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        result = pokemonIsJoutaiDataId(data, 0x3D);
    } else if (joutaiDataBiosGetStatus(0x3D) != 0xCD) {
        result = 0;
    } else {
        result = joutaiIsJoutaiDataId(d6Data, 0x3D);
    }
    if (result == 1) { return 0; }
    return pokemonGetSoubiItemSoubiDataId(ccData);
}

static inline void* fightOutPokemonGetPokemonPtrInline(void* fo)
{
    void* p;

    if (fo == NULL) {
        p = NULL;
    } else {
        void* tmp = fightPokemonGetPokemonPtrInline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0)));
        p = tmp;
    }
    return p;
}

static inline u8 fightOutPokemonCheckJoutaiInline(void* fo, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId();
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        return fightPokemonCheckJoutaiInline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0)), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    }
    return joutaiIsJoutaiDataId(fo, id);
}

u32 fightOutPokemonGetNowNimbleness(void* r3, u8 r4, u8 r5, u32 r6, void* r7)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus();
#endif
    extern u32 fightAbicntDoKakeWaru();
    void* pokemon;
    u16 tokusei;
    u32 soubi;
    u32 buff;
    u8 abicnt;
    u8 heroFlag;
    u32 nimble;

    pokemon = fightOutPokemonGetPokemonPtrInline(r3);
    if (pokemon == NULL) {
        return 0;
    }
    tokusei = (u16)pokemonGetStatus(r3, 0, POKEMON_STATUS_BATTLE_ABILITY, 0);
    soubi = fightOutPokemonGetSoubiItemSoubiDataIdInline(r3);
    buff = figthOutPokemonGetSoubiItemBuffInline(r3);
    abicnt = (u8)pokemonGetStatus(r3, 0, POKEMON_STATUS_STAGE_SPEED, 0);
    if (r7 != NULL) {
        heroFlag = heroGetStatus(r7, 0x11, 0);
    } else {
        heroFlag = 0;
    }
    nimble = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPEED, 0);
    if (tokusei == ABILITY_SWIFT_SWIM && r5 == 2) {
        nimble <<= 1;
    } else if (tokusei == ABILITY_CHLOROPHYLL && r5 == 1) {
        nimble <<= 1;
    }
    nimble = fightAbicntDoKakeWaru(abicnt, nimble);
    if (r4 == 1 && heroFlag == 1) {
        nimble = nimble * 110 / 100;
    }
    if ((u16)soubi == 0x18) {
        nimble >>= 1;
    }
    if (fightOutPokemonCheckJoutaiInline(r3, 5) == 1) {
        nimble >>= 2;
    }
    if ((u16)soubi == 0x1A && (u16)r6 < (s32)(buff * 0xFFFF) / 100) {
        nimble = -1;
    }
    return nimble;
}

/* 0x80204854 | size: 0xD4 | medium */
u32 fightOutPokemonCheckIrekaeReserveFightPokemon(void* param_1, void* param_2) {
    void* iVar2;
    s16 sVar3;
    u32 uVar1;
    s16 sVar4;
    u8 cVar5;

    sVar3 = (s16)(u32)pokemonGetStatus(param_2, 0, POKEMON_STATUS_ENTRY_ID, 0);
    if (sVar3 < 0) {
        uVar1 = 0;
    } else {
        sVar4 = (s16)(u32)pokemonGetStatus(param_1, 0, POKEMON_STATUS_SWITCH_TARGET, 0);
        if (sVar3 == sVar4) {
            uVar1 = 1;
        } else {
            iVar2 = PC_PTR(pokemonGetStatus(param_1, 0, POKEMON_STATUS_ACTION_PTR, 0));
            if ((((iVar2 != NULL) && (cVar5 = fightActionCheckValid(iVar2), cVar5 == 1)) &&
                (fightActionBiosGetKind((FightAction*)iVar2) == 9)) &&
               (sVar4 = fightActionBiosGetBuffDataId((FightAction*)iVar2),
                sVar3 == sVar4)) {
                uVar1 = 1;
            } else {
                uVar1 = 0;
            }
        }
    }
    return uVar1;
}

/* 0x80204928 | size: 0x48 | small */
u8 fightPokemonCheckMotoFightPokemon(u32 expected, void* ctx) {
    u32 result = (u32)pokemonGetStatus(ctx, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    return (result == expected) ? 1 : 0;
}

/* 0x80204970 | size: 0xA0 */
void fightPokemonBiosSwap(void* first, void* second)
{
    typedef struct FightPokemonCopy {
        u8 data[0x154];
    } FightPokemonCopy;
    FightPokemonCopy* firstPokemon = first;
    FightPokemonCopy* secondPokemon = second;

    if (firstPokemon != NULL) {
        FightPokemonCopy temporary;
        if (secondPokemon != NULL) {
            temporary = *firstPokemon;
            *firstPokemon = *secondPokemon;
            *secondPokemon = temporary;
        }
    }
}

/* fightOutPokemonIsGcHeroFightOutPokemon | Size: 0x4C | Check if trainer slot is active */
u8 fightOutPokemonIsGcHeroFightOutPokemon(u32 slotId) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32 context, u32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerIsGcHero(void* trainer);
#endif
    void* trainer = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, PC_PTR(slotId));
    if (trainer == NULL) {
        return 0;
    }
    return fightTrainerIsGcHero(trainer) == 1;
}

/* 0x80204A5C | size: 0x1AC | medium */
/* 0x80204A5C | size: 0x1AC */
u32 fightOutPokemonIsFightActionUseItemKind(void* ctx, u8 targetSlot, u8 mode) {
    extern u32 lbl_80478BD8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80142984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorGetStatus();
#endif
    u16 field1E;
    u16 field1F;
    int e5Data;
    void* feData;
    u8 valid;
    u32 i;

    for (i = 0; (u16)i < lbl_80478BD8; i++) {
        if ((u8)fn_80142984(i) == 0) { continue; }
        if (mode == 1) {
            if (targetSlot != (u8)itemGetStatus(0, i, 0x2, 0)) { continue; }
        } else {
            if (targetSlot == (u8)itemGetStatus(0, i, 0x2, 0)) { continue; }
        }
        fightFloorGetStatus(0, 0, 0x14, 0);
        if (ctx == NULL) { valid = 0; }
        else {
            feData = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ACTION_PTR, 0));
            if (feData == NULL) { valid = 0; }
            else if ((u8)fightActionCheckValid(feData) == 0) { valid = 0; }
            else if (fightActionGetKindDataId(feData) != 0x12) { valid = 0; }
            else {
                e5Data = (int)pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_ITEM_PTR, 0);
                if (e5Data == 0) { valid = 0; }
                else {
                    field1E = (u16)itemGetStatus(PC_PTR(e5Data), 0, 0x1E, 0);
                    field1F = (u16)itemGetStatus(PC_PTR(e5Data), 0, 0x1F, 0);
                    if ((u16)i != 0 && field1E != (u16)i) {
                        valid = 0;
                    } else {
                        valid = 1;
                    }
                }
            }
        }
        if (valid == 1) { return 1; }
    }
    return 0;
}

/* Address: 0x80204C08 | Size: 0xd8 | Ghidra import */
u16 fightOutPokemonGetFightActionUseItemDataId(void* r3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorGetStatus();
#endif
  void* iVar1;
  u8 cVar4;
  u16 sVar2;
  u16 uVar3;

  fightFloorGetStatus(0,0,0x14,0);
  if (r3 == 0) {
    uVar3 = 0;
  }
  else {
    iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_ACTION_PTR,0));
    if (iVar1 == 0) {
      uVar3 = 0;
    }
    else {
      cVar4 = fightActionCheckValid(iVar1);
      if (cVar4 == 0) {
        uVar3 = 0;
      }
      else {
        sVar2 = fightActionGetKindDataId(iVar1);
        if (sVar2 != 0x12) {
          uVar3 = 0;
        }
        else {
          iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_BATTLE_ITEM_PTR,0));
          if (iVar1 == 0) {
            uVar3 = 0;
          }
          else {
            uVar3 = itemGetStatus(iVar1,0,0x1e,0);
          }
        }
      }
    }
  }
  return uVar3;
}

/* fightOutPokemonCreateFightAction (0x8020505C), expanded by the Create*
 * callers that precede it. */
static inline void* fightOutPokemonCreateFightActionInline(void* ctx, void* target, u32 a,
                                                           u32 b, FightActionData* data,
                                                           u32 buff)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId();
#endif
    FightAction* action;

    action = (FightAction*)PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ACTION_PTR, 0));
    if (action == NULL) {
        return NULL;
    }
    if ((u8)fightActionCreate(action, target, ctx, a, b, data) == 1) {
        fightActionBiosSetBuffDataId(action, buff);
        return action;
    }
    return NULL;
}

/* 0x80204CE0 | size: 0x104 */
void* fightOutPokemonCreateFightActionUseItem(void* ctx, void* p2, u32 p3,
                                              u32 p4,
                                              FightActionData* p5, u32 p6,
                                              u32 p7, u32 p8, u8 p9) {
    extern void itemSetStatus();
    extern void fightItemCreate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId();
#endif
    void* e5Data;
    void* feData;
    u16 itemDataId = (u16)p6;

    e5Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_ITEM_PTR, 0));
    if (e5Data == NULL) { return NULL; }
    fightItemCreate(e5Data, itemDataId, p7, p8);
    itemSetStatus(e5Data, 0, 0x21, 0, (u32)p9);
    feData = fightOutPokemonCreateFightActionInline(ctx, p2, p3, p4, p5, p6);
#ifdef TARGET_PC
    {
        /* ORRE_BATTLE_LOG (observe only, 2026-10-25, P08's battle 101: the console throws a Net Ball where the
         * PC's turn has no throw): each item action as it is created */
        static int enabled = -1;
        extern void OSReport(const char*, ...);
        if (enabled < 0) {
            extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
            enabled = pc_battle_log_on();
        }
        if (enabled) {
            /* the user's species for the sweep's item list: ctx is the battle slot, its Pokémon record holds the
             * species at +0x00 (as the turn lines read it, fight_floor_bios.c) */
            extern void* fightOutPokemonGetPokemonPtr(void*);
            const u8* mon = (const u8*)fightOutPokemonGetPokemonPtr(ctx);
            OSReport("[battle] action item: species %u uses item %u (kind 0x%X, target %u, menu %u, slot %u) -> %s\n",
                     mon != NULL ? (unsigned)*(const u16*)mon : 0u, (unsigned)itemDataId, (unsigned)p3, (unsigned)p7,
                     (unsigned)p8, (unsigned)p9, feData != NULL ? "made" : "refused");
        }
    }
#endif
    if (feData == NULL) {
        return NULL;
    }
    return feData;
}

/* 0x80204DE4 | size: 0x188 */
u32 fightOutPokemonIsFightActionAttackWazaOut(void* ctx, u16 slotId, void* tablePtr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetStatus();
#endif
    void* feData;
    void* d9Data;
    u16 partyCount;
    void* savedEntry;
    u16 field27;
    u16 field09;
    u32 field29;

    partyCount = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    if (ctx == NULL) { return 0; }
    savedEntry = !tablePtr ? NULL : fightTargetGetTragetPtrToRelativeHostSideFightTargetId(PC_PTR32_IMPLICIT(tablePtr), partyCount);
    feData = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ACTION_PTR, 0));
    if (feData == NULL) { return 0; }
    if ((u8)fightActionCheckValid(feData) == 0) { return 0; }
    if (fightActionGetKindDataId(feData) != 0x13) { return 0; }
    d9Data = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    if (d9Data == NULL) { return 0; }
    field27 = (u16)wazaGetStatus(d9Data, 0, 0x27, 0);
    field09 = (u16)wazaGetStatus(0, field27, 0x9, 0);
    if (slotId != 0 && field27 != slotId) { return 0; }
    field29 = (u32)wazaGetStatus(d9Data, 0, 0x29, 0);
    if (field09 == 0xB0) {
        field29 = PC_PTR32(fightTargetGetTragetPtrToRelativeHostSideFightTargetId(fightTargetGetPtrAsNowFightType(0xE, PC_PTR32_IMPLICIT(ctx)), partyCount));
    }
    if ((u16)PC_PTR32(savedEntry) != 0 && (u16)field29 != (u16)PC_PTR32(savedEntry)) { return 0; }
    return 1;
}

/* 0x80204F6C | size: 0xF0 */
void* fightOutPokemonCreateFightActionAttackWaza(void* ctx, void* p2, u32 p3,
                                                 u32 p4,
                                                 FightActionData* p5, u32 p6,
                                                 u32 p7, u32 p8, u8 p9) {
    extern void fightWazaCreate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId();
#endif
    u16 zokusei = (u16)p6;
    void* result;
    u8 created;

    result = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    if (result == NULL) {
        return NULL;
    } else {
        fightWazaCreate(result, p8, zokusei, p7, p9);
        result = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ACTION_PTR, 0));
        if (result == NULL) {
            result = NULL;
        } else {
            created = fightActionCreate((FightAction*)result, p2, ctx,
                                        p3, p4, p5);
            if (created == 1) {
                fightActionBiosSetBuffDataId(result, p6);
            } else {
                result = NULL;
            }
        }
#ifdef TARGET_PC
        {
            /* ORRE_BATTLE_LOG (observe only, 2026-10-25, P08): each move action as it is created */
            static int enabled = -1;
            extern void OSReport(const char*, ...);
            if (enabled < 0) {
                extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
                enabled = pc_battle_log_on();
            }
            if (enabled) {
                /* the user's species (ctx is the battle slot; its Pokémon record holds the species at +0x00) and
                 * the move's name from the message banks (read only), for the sweep's move list */
                extern const char* pc_event_log_move_name(u32 move);
                extern void* fightOutPokemonGetPokemonPtr(void*);
                const u8* mon = (const u8*)fightOutPokemonGetPokemonPtr(ctx);
                /* and its move-effect id (status 9, SeqId: which of the 214 battle scripts runs it), so the sweep
                 * can tie each effect routine to what it did on screen and name it */
                OSReport("[battle] action move: species %u uses move %u %s effect %u (kind 0x%X, target %u, select %u) "
                         "-> %s\n",
                         mon != NULL ? (unsigned)*(const u16*)mon : 0u, (unsigned)zokusei,
                         pc_event_log_move_name(zokusei), (unsigned)wazaGetStatus(0, (u16)zokusei, 9, 0), (unsigned)p3,
                         (unsigned)p7, (unsigned)p8, result != NULL ? "made" : "refused");
            }
        }
#endif
        if (result == NULL) {
            return NULL;
        }
    }
    return result;
}

/* Address: 0x8020505C | Size: 0x98 | Ghidra import */
void* fightOutPokemonCreateFightAction(void* r3, void* r4, u32 r5, u32 r6,
                                       FightActionData* r7, u32 r8)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId();
#endif
  FightAction* action;
  u8 cVar2;

  action = (FightAction*)PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_ACTION_PTR,0));
  if (action == NULL) {
    return NULL;
  }
  cVar2 = fightActionCreate(action,r4,r3,r5,r6,r7);
  if (cVar2 == 1) {
    fightActionBiosSetBuffDataId(action,r8);
    return action;
  }
  return NULL;
}

#if 0
asm void fightOutPokemonGetFightActionPri(void) {
#include "src/game/colosseum_event_fn_802050F4.inc"
}
#else
s32 fightOutPokemonGetFightActionPri(void* ctx) {
    void* p;
    p = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ACTION_PTR, 0));
    if (p == NULL) {
        return -0x80;
    }
    return fightActionGetPri(p);
}
#endif

/* fightOutPokemonGetWazaZokuseiDataId | Size: 0x50 | Get field 0x30 from resolved 0xD9, default 9 */
u16 fightOutPokemonGetWazaZokuseiDataId(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    void* resolved = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    if (resolved == NULL) {
        return 9;
    }
    return (u16)wazaGetStatus(resolved, 0, 0x30, 0);
}

u16 fightOutPokemonGetUseWazaDataId(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    void* resolved;
    resolved = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    if (resolved == 0) {
        return 0;
    }
    return (u16)wazaGetStatus(resolved, 0, 0x28, 0);
}

/* fightOutPokemonGetCmpNimblenessWazaDataId | Size: 0x50 | Get field 0x27 from resolved 0xD9, default 0 */
u16 fightOutPokemonGetCmpNimblenessWazaDataId(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    void* resolved = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    if (resolved == NULL) {
        return 0;
    }
    return (u16)wazaGetStatus(resolved, 0, 0x27, 0);
}

/* fightOutPokemonGetMotoWazaDataId | Size: 0x50 | Get field 0x27 from resolved 0xD9, default 0 */
u16 fightOutPokemonGetMotoWazaDataId(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    void* resolved = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    if (resolved == NULL) {
        return 0;
    }
    return (u16)wazaGetStatus(resolved, 0, 0x27, 0);
}

/* Address: 0x80205274 | Size: 0x690 | Ghidra import */

/* fightOutPokemonCheckMeetEnemyFightPokemon (0x80205904) and XD's
 * fightOutPokemonSetMeetEnemyFightPokemon (GXXE01 0x802046B4; stripped in
 * Colosseum), expanded by fightOutPokemonSetMeetEnemyFightPokemonEnemySideAll
 * as XD's calls them. */
static inline u8 fightOutPokemonCheckMeetEnemyInline(void* fo, void* fp)
{
    s16 entry;
    u8 i;

    if (fo == NULL) {
        return 0;
    }
    if (fightPokemonCheckValidInline(fp) == 0) {
        return 0;
    }
    entry = (s16)(s32)pokemonGetStatus(fp, 0, POKEMON_STATUS_ENTRY_ID, 0);
    for (i = 0; i < 12; i++) {
        s16 id = (s16)(s32)pokemonGetStatus(fo, 0, 0xfd, i);
        if (id >= 0 && id == entry) {
            return 1;
        }
    }
    return 0;
}

static inline void fightOutPokemonSetMeetEnemyInline(void* fo, void* fp)
{
    u8 i;
    s16 entry;

    if (fo == NULL) {
        return;
    }
    if (fightPokemonCheckValidInline(fp) == 0) {
        return;
    }
    if (fightOutPokemonCheckMeetEnemyInline(fo, fp) == 1) {
        return;
    }
    entry = (s16)(s32)pokemonGetStatus(fp, 0, POKEMON_STATUS_ENTRY_ID, 0);
    for (i = 0; i < 12; i++) {
        if ((s16)(s32)pokemonGetStatus(fo, 0, 0xfd, i) < 0) {
            pokemonSetStatus(fo, 0, 0xfd, i, entry);
            return;
        }
    }
}

void fightOutPokemonSetMeetEnemyFightPokemonEnemySideAll(void* fo, void* side)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightSideGetStatus(void*, u32, u32, u16);
#endif
    extern u8 fightTrainerCheckValid();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetStatus(void*, u32, u32, u16);
#endif
    u16 trainers;
    u16 pokemons;
    u16 p;
    u16 t;
    void* trainer;
    void* enemy;

    fightFloorGetStatus(0, 0, 0x14, 0);
    trainers = fightFloorGetStatus(0, 0, 0x16, 0);
    pokemons = fightFloorGetStatus(0, 0, 0x18, 0);
    for (t = 0; t < trainers; t++) {
        trainer = PC_PTR(fightSideGetStatus(side, 0, 7, t));
        if (fightTrainerCheckValid(trainer) == 0) {
            continue;
        }
        for (p = 0; p < pokemons; p++) {
            enemy = PC_PTR(fightTrainerGetStatus(trainer, 0, 0x46, p));
            if (fightOutPokemonCheckFightOutInline(enemy) != 0) {
                enemy = PC_PTR(pokemonGetStatus(enemy, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0));
                fightOutPokemonSetMeetEnemyInline(fo, enemy);
            }
        }
    }
}

/* Address: 0x80205904 | Size: 0x178 | Ghidra import */
u32 fightOutPokemonCheckMeetEnemyFightPokemon(void* r3, void* r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
  u8 bVar1;
  u32 iVar2;
  u8 cVar5;
  s16 sVar3;
  u16 flag;
  s16 sVar4;
  u8 bVar6;

  if (r3 == 0) {
    return 0;
  }
  {
    if (r4 == 0) {
      bVar1 = 0;
    }
    else {
      flag = std103_battleGetResult();
      if (flag == 1) {
        bVar1 = 0;
      }
      else {
        iVar2 = (u32)pokemonGetStatus(r4,0,POKEMON_STATUS_PARTY_RECORD_PTR,0);
        if (iVar2 == 0) {
          bVar1 = 0;
        }
        else {
          cVar5 = pokemonCheckValid(PC_PTR(iVar2));
          if (cVar5 == 0) {
            bVar1 = 0;
          }
          else {
            if (r4 == 0) {
              iVar2 = 0;
            }
            else {
              iVar2 = (u32)pokemonGetStatus(r4,0,POKEMON_STATUS_RECORD_COPY_PTR,0);
            }
            if (iVar2 == 0) {
              bVar1 = 0;
            }
            else {
              cVar5 = pokemonCheckValid(PC_PTR(iVar2));
              if (cVar5 == 0) {
                bVar1 = 0;
              }
              else {
                iVar2 = (int)pokemonGetStatus(r4,0,POKEMON_STATUS_ENTRY_ID,0);
                if ((s32)iVar2 < 0) {
                  bVar1 = 0;
                }
                else {
                  bVar1 = 1;
                }
              }
            }
          }
        }
      }
    }
    if (bVar1 == 0) {
      return 0;
    }
    sVar3 = (int)pokemonGetStatus(r4,0,POKEMON_STATUS_ENTRY_ID,0);
    for (bVar6 = 0; bVar6 < 0xc; bVar6++) {
      sVar4 = (int)pokemonGetStatus(r3,0,0xfd,bVar6);
      if ((sVar4 >= 0) && (sVar4 == sVar3)) {
        return 1;
      }
    }
  }
  return 0;
}

/* fightOutPokemonSetOnDarkPokemonFlag | Size: 0x58 | Two-hop resolve and call pokemonSetOnDarkPokemonFlag */
void fightOutPokemonSetOnDarkPokemonFlag(void* ctx, u32 param) {
    extern void pokemonSetOnDarkPokemonFlag(void* obj, u32 param);
    if (ctx == NULL) {
        return;
    }
    ctx = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0));
    ctx = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0));
    pokemonSetOnDarkPokemonFlag(ctx, param);
}

/* fightOutPokemonSetOnZukanFlag | Size: 0x58 | Two-hop resolve and call pokemonSetOnZukanFlag */
void fightOutPokemonSetOnZukanFlag(void* ctx, u32 param) {
    extern void pokemonSetOnZukanFlag(void* obj, u32 param);
    if (ctx == NULL) {
        return;
    }
    ctx = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0));
    ctx = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_PARTY_RECORD_PTR, 0));
    pokemonSetOnZukanFlag(ctx, param);
}

/* fightOutPokemonGetFightEntryId | Size: 0x60 | Two-hop resolve (0xD5 -> 0xCE), return s16 or -1 */
s16 fightOutPokemonGetFightEntryId(void* ctx) {
    void* hop1;
    if (ctx == NULL) {
        return -1;
    }
    hop1 = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0));
    if (hop1 == NULL) {
        return -1;
    }
    return (s16)(u32)pokemonGetStatus(hop1, 0, POKEMON_STATUS_ENTRY_ID, 0);
}

void* fightOutPokemonGetPokemonPtr(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus();
#endif
    if (ctx == 0) {
        return 0;
    }
    ctx = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (ctx == 0) {
        return 0;
    }
    return PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

/* 0x80205BE8 | size: 0x3C | small */
void* fightPokemonGetPokemonPtr(void* ctx) {
    if (ctx == 0) return 0;
    return PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

/* Address: 0x80205C24 | Size: 0x684 | Ghidra import */

u32 fightOutPokemonCheckFightActionSelect(void* r3, u8 r4)
{
    extern FightActionData lbl_80375CA8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    extern void fightWazaCreate();
    extern u8 fightWazaCheckValid();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightSeqSelectTarget();
#endif
    u16 count;
    void* waza;
    u16 wazaId;
    u32 target;
    s8 wazaArg;
    void* ptr;

    count = fightFloorGetStatus(0, 0, 0x14, 0);
    if (r3 == NULL) {
        return 0;
    }
    if (fightOutPokemonCheckFightOutInline(r3) == 0) {
        return 0;
    }
    if (fightOutPokemonCheckJoutaiInline(r3, 0x12) == 1 ||
        fightOutPokemonCheckJoutaiInline(r3, 0x22) == 1) {
        if (r4 != 0) {
            waza = PC_PTR(pokemonGetStatus(r3, 0, 0xf8, 0));
            if (fightWazaCheckValid(waza) != 0) {
                wazaId = wazaGetStatus(waza, 0, 0x28, 0);
                wazaArg = wazaGetStatus(waza, 0, 0x26, 0);
                target = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(
                    fightSeqSelectTarget(PC_PTR32_IMPLICIT(r3), wazaId, count,
                                PC_PTR32_IMPLICIT(_fightOutPokemonCheckFightActionSelectSub__FP15FightOutPokemonUsUs),
                                1, 0, -1),
                    count);
                ptr = PC_PTR(pokemonGetStatus(r3, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
                if (ptr != NULL) {
                    fightWazaCreate(ptr, wazaArg, wazaId, target, 1);
                    ptr = PC_PTR(pokemonGetStatus(r3, 0, POKEMON_STATUS_ACTION_PTR, 0));
                    if (ptr != NULL &&
                        (u8)fightActionCreate((FightAction*)ptr, NULL, r3, 0x13, 0,
                                              lbl_80375CA8) == 1) {
                        fightActionBiosSetBuffDataId((FightAction*)ptr, wazaId);
                    }
                }
            }
        }
        return 0;
    }
    return 1;
}

/* 0x802062A8 | size: 0x54 | small */
void _fightOutPokemonCheckFightActionSelectSub__FP15FightOutPokemonUsUs(void* param_1, u32 param_2, u32 param_3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
    void* uVar1;
    u16 uVar2;

    uVar1 = PC_PTR(pokemonGetStatus(param_1, 0, 0xF8, 0));
    uVar2 = (u16)wazaGetStatus(uVar1, 0, 0x29, 0);
    fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(uVar2, param_3);
}

/* Address: 0x802062FC | Size: 0x30c | Ghidra import */

u32 fightOutPokemonCheckFightOut(void* r3)
{
    if (r3 == NULL) {
        return 0;
    }
    if (fightOutPokemonCheckValidInline(r3) == 0) {
        return 0;
    }
    if ((s32)pokemonGetStatus(r3, 0, POKEMON_STATUS_FAINTED_FLAG, 0) == 1) {
        return 0;
    }
    return fightPokemonCheckFightOutInline(PC_PTR(pokemonGetStatus(r3, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0))) != 0;
}


/* Address: 0x80206608 | Size: 0x178 | Ghidra import */
u32 fightPokemonCheckFightOut(void* r3)

{
    extern u8 pokemonCheckFightOut(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
  u16 sVar2;
  void* iVar1;
  u8 cVar3;
  u8 bVar4;

  if (r3 == 0) {
    return 0;
  }
  if (r3 == 0) {
    bVar4 = 0;
  }
  else {
    sVar2 = std103_battleGetResult();
    if (sVar2 == 1) {
      bVar4 = 0;
    }
    else {
      iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_PARTY_RECORD_PTR,0));
      if (iVar1 == 0) {
        bVar4 = 0;
      }
      else {
        cVar3 = pokemonCheckValid(iVar1);
        if (cVar3 == 0) {
          bVar4 = 0;
        }
        else {
          if (r3 == 0) {
            iVar1 = 0;
          }
          else {
            iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_RECORD_COPY_PTR,0));
          }
          if (iVar1 == 0) {
            bVar4 = 0;
          }
          else {
            cVar3 = pokemonCheckValid(iVar1);
            if (cVar3 == 0) {
              bVar4 = 0;
            }
            else {
              iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_ENTRY_ID,0));
              if ((s32)PC_PTR32(iVar1) < 0) {
                bVar4 = 0;
              }
              else {
                bVar4 = 1;
              }
            }
          }
        }
      }
    }
  }
  if (bVar4 == 0) {
    return 0;
  }
  iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG,0));
  if ((s32)PC_PTR32(iVar1) == 1) {
    return 0;
  }
  if (r3 == 0) {
    iVar1 = 0;
  }
  else {
    iVar1 = PC_PTR(pokemonGetStatus(r3,0,POKEMON_STATUS_RECORD_COPY_PTR,0));
  }
  if (iVar1 == 0) {
    return 0;
  }
  return (u8)pokemonCheckFightOut(iVar1) != 0;
}

/* Address: 0x80206780 | Size: 0x148 | Ghidra import */
u32 fightOutPokemonCheckValid(void* p1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
    void* fp;

    if (p1 == NULL) {
        return 0;
    }
    if (std103_battleGetResult() == 1) {
        return 0;
    }
    fp = PC_PTR(pokemonGetStatus(p1, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (fp == NULL) {
        return 0;
    }
    return fightPokemonCheckValidInline(fp) != 0;
}

/* Address: 0x802068C8 | Size: 0x13c | Ghidra import */

void fightOutPokemonCreate(void* r3, void* r4, void* r5)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus(void*, u16, u32, u32);
#endif
    extern void fightFloorSetShadow();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonInit();
#endif
    u32 tokusei;
    u16 species;
    void* poke;
    u32 i;

    if ((r3 != 0) && (r4 != 0)) {
        poke = fightPokemonGetPokemonPtrInline(r4);
        fightOutPokemonInit(PC_PTR32_IMPLICIT(r3));
        pokemonSetStatus(r3, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0, PC_PTR32_IMPLICIT(r4));
        pokemonSetStatus(r3, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0, PC_PTR32_IMPLICIT(r4));
        if (r5 != 0) {
            pokemonSetStatus(r3, 0, POKEMON_STATUS_SEQUENCE_PTR, 0, PC_PTR32_IMPLICIT(r5));
            fightFloorSetShadow();
        }
        species = (u16)pokemonGetStatus(poke, 0, POKEMON_STATUS_SPECIES, 0);
        for (i = 0; (u16)i < 2; i++) {
            pokemonSetStatus(r3, 0, POKEMON_STATUS_BATTLE_TYPE, (u8)i, (u16)pokemonGetStatus(0, species, POKEMON_STATUS_SPECIES_TYPE, i));
        }
        tokusei = pokemonGetTokuseiDataId(poke);
        pokemonSetStatus(r3, 0, POKEMON_STATUS_BATTLE_ABILITY, 0, (u16)tokusei);
    }
}
