/**
 * @file fight_pokemon_r58_801FF1BC_o1.c
 * @brief fightOutPokemon move-selection checks, 0x801FF1BC - 0x80200A5C:
 *        fightOutPokemonCheckFightActionWazaSelect,
 *        fightOutPokemonGetOutOkWazaBanmeAry and
 *        fightOutPokemonCheckCanOutOkWazaBanme.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"
#include "game/constants/moves.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u16 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonWazaCheckValid(void* pokemon, u16 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus(u32 context, u16 wazaId, u16 field, u32 flags);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightOutPokemonCheckCanOutOkWazaBanme(void* fighter, u16 slot, u8 mode, u16* output);
#endif
u8 fightOutPokemonGetOutOkWazaBanmeAry(void* pokemon);

/* The Pokemon record held in `pokemon`'s 0xD6 slot, or NULL. */
static inline void* fightPokemonGetSlotPokemon(void* pokemon) {
    void* slot;

    if (pokemon == NULL) {
        return NULL;
    }
    slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (slot == NULL) {
        return NULL;
    }
    return PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

/* The Pokemon record held in `pokemon`'s 0xD6 slot, or NULL. */
static inline void* fightPokemonGetSlotRecord(void* pokemon) {
    void* slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));

    if (slot == NULL) {
        return NULL;
    }
    return PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

/* Asks whether the joutai `id` is set. Moves 0x7C/0xC8/0xCD keep it in the
 * 0xD6 slot's data (0x7C/0xC8 through its 0xCC record), 0xD8 on the
 * Pokemon itself; any other kind is never set. */
static inline u8 fightPokemonIsJoutai(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(void* obj, u16 id);
#endif
    void* slot;
    void* data;

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
            if (slot == NULL) {
                data = NULL;
            } else {
                data = PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonIsJoutaiDataId(data, id);
        } else if (joutaiDataBiosGetStatus(id) != 0xCD) {
            return 0;
        } else {
            return joutaiIsJoutaiDataId(slot, id);
        }
    } else if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    } else {
        return joutaiIsJoutaiDataId(pokemon, id);
    }
}

/* Status word of the joutai `id` kept in `pokemon`'s 0xD6 slot data. */
static inline u32 fightPokemonGetSlotJoutaiStatus(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetJoutai08(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiGetBuff(void* obj, u16 id);
#endif
    void* slot;
    void* data;

    slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        if (slot == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        return pokemonGetJoutai08(data, id);
    } else if (joutaiDataBiosGetStatus(id) != 0xCD) {
        return 0;
    } else {
        return joutaiGetBuff(slot, id);
    }
}

/* Status word the joutai `id` saved, read from where fightPokemonIsJoutai looks. */
static inline u32 fightPokemonGetJoutaiStatus(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiGetBuff(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        return fightPokemonGetSlotJoutaiStatus(pokemon, id);
    } else if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    } else {
        return joutaiGetBuff(pokemon, id);
    }
}

/* Asks whether the joutai `id` is set in the 0xD6 slot data `slot` (moves
 * 0x7C/0xC8 keep it in the slot's 0xCC record, 0xCD on the slot itself). */
static inline u8 fightPokemonSlotIsJoutai(void* slot, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(void* obj, u16 id);
#endif
    void* data;

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        if (slot == NULL) {
            data = NULL;
        } else {
            data = PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        }
        return pokemonIsJoutaiDataId(data, id);
    } else if (joutaiDataBiosGetStatus(id) != 0xCD) {
        return 0;
    } else {
        return joutaiIsJoutaiDataId(slot, id);
    }
}

/* Upper half of the random status the joutai `id` saved. */
static inline u32 fightPokemonGetJoutaiRnd2(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
    extern u32 pokemonGetJoutai0C(void* obj, u16 id);
    extern u32 joutaiGetIdBuff(void* obj, u16 id);
    void* slot;
    void* data;

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
            if (slot == NULL) {
                data = NULL;
            } else {
                data = PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonGetJoutai0C(data, id);
        } else if (joutaiDataBiosGetStatus(id) != 0xCD) {
            return 0;
        } else {
            return joutaiGetIdBuff(slot, id);
        }
    } else if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    } else {
        return joutaiGetIdBuff(pokemon, id);
    }
}

/* Clears the joutai `id`, from the same place fightPokemonIsJoutai reads. */
static inline void fightPokemonClearJoutai(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitJoutaiDataId(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai(void* obj, u16 id);
#endif
    void* slot;
    void* data;

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
            if (slot == NULL) {
                data = NULL;
            } else {
                data = PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            pokemonInitPersistentJoutai(data, id);
        } else if (joutaiDataBiosGetStatus(id) == 0xCD) {
            joutaiInitJoutaiDataId(slot, id);
        }
    } else if (joutaiDataBiosGetStatus(id) == 0xD8) {
        joutaiInitJoutaiDataId(pokemon, id);
    }
}

typedef struct {
    u16 ids[9];
} JoutaiIdTable9;

/* fightOutPokemonInitJoutaiKeep (0x80200B10), which retail inlines here:
 * clears the nine "keep" joutai and cancels their 0xEE effects. */
static inline void fightPokemonInitJoutaiKeep(void* pokemon) {
    extern JoutaiIdTable9 lbl_80279C90;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerClearState(void* effect, s32 trajType);
#endif
    JoutaiIdTable9 table = lbl_80279C90;
    void* effect;
    u8 i;
    u16 id;
    u16* ids;

    if (pokemon == NULL) {
        return;
    }
    ids = table.ids;
    for (i = 0; i < 9; i++) {
        id = ids[i];
        if (fightPokemonIsJoutai(pokemon, id) == 1) {
            effect = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (id == 0) {
                if (effect != NULL) {
                    wazaSequenceOwnerClearState(effect, 1);
                    wazaSequenceOwnerClearState(effect, 2);
                }
            } else if (effect != NULL) {
                if (id == 8) {
                    wazaSequenceOwnerClearState(effect, 1);
                }
                if (id == 7) {
                    wazaSequenceOwnerClearState(effect, 2);
                }
            }
            fightPokemonClearJoutai(pokemon, id);
        }
    }
}

/* Number of `pokemon`'s moves it may use now. */
static inline u8 fightPokemonCountOutOkWaza(void* pokemon) {
    u8 ok[4];
    u8 count;
    u8 i;
    u16 waza;

    if (pokemon == NULL) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        ok[i] = 0;
    }
    count = 0;
    for (i = 0; i < 4; i++) {
        if (pokemonWazaCheckValid(fightPokemonGetSlotPokemon(pokemon), i) != 0) {
            waza = (u16)(u32)pokemonGetStatus(fightPokemonGetSlotPokemon(pokemon), 0, POKEMON_STATUS_MOVE, i);
            if (waza != 0 && waza != MOVE_SHADOW_LOCKED && waza != MOVE_UNUSED_355 &&
                fightOutPokemonCheckCanOutOkWazaBanme(pokemon, i, 0, NULL) == 0) {
                count++;
            }
        }
    }
    return count;
}

/* Copies of fight_out_pokemon.c's accessors, which retail expands here. */
static inline void* fightPokemonGetPokemonPtrInline(void* fp)
{
    void* p;

    if (fp == NULL) {
        p = NULL;
    } else {
        void* tmp = PC_PTR(pokemonGetStatus(fp, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        p = tmp;
    }
    return p;
}

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

static inline u8 fightPokemonCheckFightOutInline(void* fp)
{
    extern u8 pokemonCheckFightOut(void*);
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

static inline u8 fightOutPokemonCheckValidInline(void* p1)
{
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

static inline u8 fightPokemonCheckJoutaiInline(void* fp, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        return pokemonIsJoutaiDataId(fightPokemonGetPokemonPtrInline(fp), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xCD) {
        return 0;
    }
    return joutaiIsJoutaiDataId(fp, id);
}

static inline u8 fightOutPokemonCheckJoutaiInline(void* fo, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        return fightPokemonCheckJoutaiInline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0)), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    }
    return joutaiIsJoutaiDataId(fo, id);
}

static inline u32 fightPokemonGetJoutaiRnd2Inline(void* fp, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
    extern u32 pokemonGetJoutai0C(void* obj, u16 id);
    extern u32 joutaiGetIdBuff(void* obj, u16 id);

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        return pokemonGetJoutai0C(fightPokemonGetPokemonPtrInline(fp), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xCD) {
        return 0;
    }
    return joutaiGetIdBuff(fp, id);
}

static inline u32 fightOutPokemonGetJoutaiRnd2Inline(void* fo, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
    extern u32 joutaiGetIdBuff(void* obj, u16 id);

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        return fightPokemonGetJoutaiRnd2Inline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0)), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    }
    return joutaiGetIdBuff(fo, id);
}

static inline u32 fightPokemonGetJoutaiStatusInline(void* fp, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetJoutai08(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiGetBuff(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        return pokemonGetJoutai08(fightPokemonGetPokemonPtrInline(fp), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xCD) {
        return 0;
    }
    return joutaiGetBuff(fp, id);
}

static inline u32 fightOutPokemonGetJoutaiStatusInline(void* fo, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiGetBuff(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        return fightPokemonGetJoutaiStatusInline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0)), id);
    }
    if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    }
    return joutaiGetBuff(fo, id);
}

static inline void fightPokemonClearJoutaiInline(void* fp, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitJoutaiDataId(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        pokemonInitPersistentJoutai(fightPokemonGetPokemonPtrInline(fp), id);
    } else if (joutaiDataBiosGetStatus(id) == 0xCD) {
        joutaiInitJoutaiDataId(fp, id);
    }
}

static inline void fightOutPokemonClearJoutaiInline(void* fo, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitJoutaiDataId(void* obj, u16 id);
#endif

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
        fightPokemonClearJoutaiInline(PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0)), id);
    } else if (joutaiDataBiosGetStatus(id) == 0xD8) {
        joutaiInitJoutaiDataId(fo, id);
    }
}

/* Ends the joutai `id`, cancelling the 0xEE effect trajectories it drives. */
static inline void fightOutPokemonCureJoutaiInline(void* fo, u16 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerClearState(void* effect, s32 trajType);
#endif
    void* effect = PC_PTR(pokemonGetStatus(fo, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));

    if (id == 0) {
        if (effect != NULL) {
            wazaSequenceOwnerClearState(effect, 1);
            wazaSequenceOwnerClearState(effect, 2);
        }
    } else if (effect != NULL) {
        if (id == 8) {
            wazaSequenceOwnerClearState(effect, 1);
        }
        if (id == 7) {
            wazaSequenceOwnerClearState(effect, 2);
        }
    }
    fightOutPokemonClearJoutaiInline(fo, id);
}

/* 0x801FF1BC: whether `fighter` must struggle (1) or is locked into its
 * encore move (2); with `doIt` set, queues that move as its action. */
u8 fightOutPokemonCheckFightActionWazaSelect(void* fighter, u8 doIt) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32 floor, u32 index, u32 status, u32 subindex);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightSeqSelectTarget(void* fighter, u16 waza, u16 target, u32 a, u32 b, u32 c, s32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId(u32 side, u16 target);
#endif
    extern void fightWazaCreate(void* waza, s8 slot, u16 wazaId, u32 target, u32 flag);
    extern u8 fightActionCreate(void* action, void* parent, void* fighter, u32 kind, u32 arg, void* data);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId(void* action, u16 wazaId);
#endif
    extern u8 lbl_80375CA8[];
    u16 target;
    void* pokemon;
    u32 lockWaza;
    u32 slot;
    u16 waza;
    u32 targetPtr;
    s8 wazaSlot;
    void* ptr;

    target = fightFloorGetStatus(0, 0, 0x14, 0);
    if (fighter == NULL) {
        return 0;
    }
    if (fightOutPokemonCheckFightOutInline(fighter) == 0) {
        return 0;
    }
    pokemon = fightOutPokemonGetPokemonPtrInline(fighter);
    if (fightOutPokemonGetOutOkWazaBanmeAry(fighter) == 1) {
        if (doIt != 0) {
            targetPtr = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(
                fightSeqSelectTarget(PC_PTR32_IMPLICIT(fighter), 0xA5, target, 0, 1, 1, -1), target);
            ptr = PC_PTR(pokemonGetStatus(fighter, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
            if (ptr != NULL) {
                fightWazaCreate(ptr, -1, 0xA5, targetPtr, 1);
                ptr = PC_PTR(pokemonGetStatus(fighter, 0, POKEMON_STATUS_ACTION_PTR, 0));
                if (ptr != NULL && fightActionCreate(ptr, NULL, fighter, 0x13, 0, lbl_80375CA8) == 1) {
                    fightActionBiosSetBuffDataId(ptr, 0xA5);
                }
            }
        }
        return 1;
    }
    if (fightOutPokemonCheckJoutaiInline(fighter, 0x2A) == 1) {
        lockWaza = fightOutPokemonGetJoutaiRnd2Inline(fighter, 0x2A);
        slot = fightOutPokemonGetJoutaiStatusInline(fighter, 0x2A);
        waza = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, slot);
        if (doIt != 0) {
            if ((u16)lockWaza != waza) {
                fightOutPokemonCureJoutaiInline(fighter, 0x2A);
            }
            wazaSlot = slot;
            targetPtr = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(
                fightSeqSelectTarget(PC_PTR32_IMPLICIT(fighter), waza, target, 0, 1, 1, -1), target);
            ptr = PC_PTR(pokemonGetStatus(fighter, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
            if (ptr != NULL) {
                fightWazaCreate(ptr, wazaSlot, waza, targetPtr, 1);
                ptr = PC_PTR(pokemonGetStatus(fighter, 0, POKEMON_STATUS_ACTION_PTR, 0));
                if (ptr != NULL && fightActionCreate(ptr, NULL, fighter, 0x13, 0, lbl_80375CA8) == 1) {
                    fightActionBiosSetBuffDataId(ptr, waza);
                }
            }
        }
        return 2;
    }
    return 0;
}

/* 0x801FFB30: 1 when `pokemon` has no move it may use (or its encore
 * (joutai 0x2A) move cannot be used), so it must struggle. */
u8 fightOutPokemonGetOutOkWazaBanmeAry(void* pokemon) {
    if (fightPokemonCountOutOkWaza(pokemon) == 0) {
        return 1;
    }
    if (fightPokemonIsJoutai(pokemon, 0x2A) == 1) {
        if (fightOutPokemonCheckCanOutOkWazaBanme(pokemon, fightPokemonGetJoutaiStatus(pokemon, 0x2A), 0, NULL) != 0) {
            return 1;
        }
    }
    return 0;
}

/* The Pokemon record of the 0xD6 slot data `slot`. */
static inline void* fightPokemonSlotGetRecord(void* slot) {
    return PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

/* The item `fighter` holds, 0 when it has none or joutai 0x3D (embargo)
 * blocks it. */
static inline u16 fightPokemonGetSoubiItem(void* fighter) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonGetSoubiItemSoubiDataId(void* pokemon);
#endif
    void* slot;
    void* record;

    slot = PC_PTR(pokemonGetStatus(fighter, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (slot == NULL) {
        record = NULL;
    } else {
        record = fightPokemonSlotGetRecord(slot);
    }
    if (record == NULL) {
        return 0;
    }
    if (fightPokemonSlotIsJoutai(slot, 0x3D) == 1) {
        return 0;
    }
    return pokemonGetSoubiItemSoubiDataId(record);
}

/* fightOutPokemonInitJoutaiKeep (0x80200B10), which retail expands here:
 * ends the nine "keep" joutai listed in lbl_80279C90. */
static inline void fightOutPokemonInitJoutaiKeepInline(void* fo) {
    extern JoutaiIdTable9 lbl_80279C90;
    JoutaiIdTable9 table = lbl_80279C90;
    u16 id;
    u8 i;
    u16* ids;

    if (fo == NULL) {
        return;
    }
    ids = table.ids;
    for (i = 0; i < 9; i++) {
        id = ids[i];
        if (fightOutPokemonCheckJoutaiInline(fo, id) == 1) {
            fightOutPokemonCureJoutaiInline(fo, id);
        }
    }
}

static inline u16 fightOutPokemonGetSoubiItemSoubiDataIdInline(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(void* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonGetSoubiItemSoubiDataId(void* pokemon);
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

/* 0x801FFEC8: why `fighter` may not use the move in `slot` now (0 = it
 * may): 1 encore, 2 the move it just used under joutai 0x1B (clears the
 * keep joutai when `mode` is 1), 3 taunt, 4 imprison, 5 choice item
 * lock (the locked move goes to `output`), 6 invalid or no PP. */
u8 fightOutPokemonCheckCanOutOkWazaBanme(void* fighter, u16 slot, u8 mode, u16* output) {
    extern u8 fightFloorCheckHuuinWazaFightOutPokemon(u32 floor, void* fighter, u16 waza);
    u8 result;
    void* pokemon;
    u16 waza;
    u16 power;
    u8 pp;
    u16 item;
    u16 lastWaza;
    u16 lockWaza;
    u32 choice;

    result = 0;
    if (fighter == NULL) {
        return 6;
    }
    pokemon = fightOutPokemonGetPokemonPtrInline(fighter);
    waza = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, slot);
    power = wazaGetStatus(0, waza, 7, 0);
    pp = (u8)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE_PP, slot);
    item = fightOutPokemonGetSoubiItemSoubiDataIdInline(fighter);
    lastWaza = (u16)(u32)pokemonGetStatus(fighter, 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0);
    if (fightOutPokemonCheckJoutaiInline(fighter, 0x29) == 1) {
        lockWaza = fightOutPokemonGetJoutaiRnd2Inline(fighter, 0x29);
        if (lockWaza != 0 && lockWaza == waza && lockWaza != MOVE_SHADOW_LOCKED) {
            result = 1;
        }
    }
    if (fightOutPokemonCheckJoutaiInline(fighter, 0x1B) == 1 && waza == lastWaza && waza != MOVE_STRUGGLE) {
        if (mode == 1) {
            fightOutPokemonInitJoutaiKeepInline(fighter);
        }
        result = 2;
    }
    if (fightOutPokemonCheckJoutaiInline(fighter, 0x30) == 1 && power == 0) {
        result = 3;
    }
    if (fightFloorCheckHuuinWazaFightOutPokemon(0, fighter, waza) == 1) {
        result = 4;
    }
    if (item == 0x1D) {
        choice = fightOutPokemonCheckJoutaiInline(fighter, 0x36) == 1 ? fightOutPokemonGetJoutaiRnd2Inline(fighter, 0x36) : 0;
        lockWaza = choice;
        if (lockWaza != 0 && lockWaza != MOVE_SHADOW_LOCKED && lockWaza != 0xFFFF && lockWaza != waza) {
            result = 5;
        }
        if (output != NULL) {
            *output = choice;
        }
    }
    if (pokemonWazaCheckValid(pokemon, slot) == 0 || pp == 0) {
        result = 6;
    }
    return result;
}
