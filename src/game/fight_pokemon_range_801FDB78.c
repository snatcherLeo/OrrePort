/**
 * @file fight_pokemon_range_801FDB78.c
 * @brief game/pxdvs/app/fight/fightPokemon.cpp -- head portion, split
 *        from game/trainer.c (the XD fight-trainer bucket,
 *        0x801F7F80-0x80201764), address range 0x801FDB78-0x80201764,
 *        27 fns.
 *
 * XD source unit: game/pxdvs/app/fight/fightPokemon.cpp (this segment
 *        is the TU head only; the Colosseum TU continues past
 *        0x80201764 into game/fight_range_80201764.c per splits.txt).
 * Physically split out of the trainer.c bucket by address (functions
 * bucketed via their /* 0xADDR *\/ markers), byte-identical
 * reconstruction verified against the original file.
 */
#include "game/trainer.h"
#include "game/constants/moves.h"
#include "game/pokemon.h"
#include "game/constants/pokemon_status.h"

/* =========================================================================
 * External declarations (duplicated verbatim from the original
 * game/trainer.c preamble into every split segment, so each new
 * TU keeps the same external visibility it had before the split)
 * ========================================================================= */

/* pokemonGetStatus - Master data table resolver
 * The most-called function in the entire game (1769 calls).
 * Takes a context pointer, slot index, table ID, and flags.
 * Returns a pointer to the resolved data, or NULL. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u32 flags);
#endif

/* pokemonSetStatus - Master data table writer (544 calls) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonSetStatus(void* context, u32 slot, u16 tableId, u32 flags, u32 value);
#endif

/* pokemonGrowBasisStatus - Data table auxiliary writer */
extern void pokemonGrowBasisStatus(void* context, u32 value);

/* itemGetStatus - Secondary data accessor (169 calls) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemGetStatus(u32 context, u32 param, u16 field, u32 flags);
#endif

/* fightTargetGetPtr - PokemonSlotLookup (89 calls) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetPtr(u32 type, void* ptr, u32 param);
#endif

/* Category resolution sub-dispatchers (defined with real bodies at their
 * proper address-ordered locations, possibly in a sibling segment). */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void* fightTrainerDataBiosGetPtr(u16 slot); /* Battle trainer */
#endif
void* fightTrainerPokemonPartDataBiosGetPtr(u16 slot); /* Party config */
void* fightTrainerPokemonDataBiosGetPtr(u16 slot); /* Team roster */
void* fightTrainerAiDataBiosGetPtr(u16 slot); /* Story/event data */
void* fightTrainerAiValueAddsubDataBiosGetPtr(u16 slot); /* Misc attributes */

/* Event integration (defined with real bodies, possibly in a sibling segment) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightOutPokemonSetHensinPokemonStatusId(void* trainer, u16 eventId, u32 param1, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8   fightOutPokemonIsUseHensinBuff(void* trainer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightOutPokemonSetHensinFightPokemonStatusId(void* trainer, u16 eventId, u32 param);
#endif

/* Item/Pokemon field access helpers used by battle item flow. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 pokemonGetJoutai08(void* obj, s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 joutaiGetBuff(void* obj, s32 arg);
#endif

/* SDA table pointers for trainer data arrays */
extern u32* fight_trainer_pokemon_part_data_number;  /* trainer Pokémon parts: count */
extern u8*  fight_trainer_pokemon_part_data;  /* trainer Pokémon parts: records */
extern u32* lbl_80478F10;  /* Team roster header */
extern u8*  lbl_80478F14;  /* Team roster data */
extern u32* fight_trainer_kind_data_number;  /* trainer classes: count */
extern u8*  fight_trainer_kind_data;  /* trainer classes: records */
extern u32* lbl_80478F20;  /* Battle trainer header */
extern u8*  lbl_80478F24;  /* Battle trainer data */
extern u32* fight_trainer_ai_value_addsub_data_number;  /* trainer AI value adjustments: count */
extern u8*  fight_trainer_ai_value_addsub_data;  /* trainer AI value adjustments: records */
extern u32* lbl_80478F30;  /* Story/event header */
extern u8*  lbl_80478F34;  /* Story/event data */

/* Forward declarations for converted functions (defined with real bodies,
 * possibly in a sibling segment). */
u16 fightTrainerDataBiosGetAiDataId(u8* ptr);
u8 fn_801FCCAC(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightTrainerSetStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightTrainerGetStatus(void);
#endif

#if !defined(FIGHT_POKEMON_EXACT_80201248_ONLY)
#if defined(FIGHT_POKEMON_EXACT_801FDB78_ONLY) || defined(FIGHT_POKEMON_EXACT_801FE168_ONLY) || \
    defined(FIGHT_POKEMON_EXACT_802010C8_ONLY) || defined(FIGHT_POKEMON_EXACT_80200E00_ONLY)
/* A wrapper unit that links one exact function of this file. */
#define FIGHT_POKEMON_EXACT_ISLAND
#endif

/* Three joutai (status-condition) ids, copied from a .sdata2 table owned by
 * the data carve game/data/sdata2_8047E508. */
typedef struct { u16 ids[3]; } JoutaiIdTable3;

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

/* Status word the joutai `id` saved, read from where fightPokemonIsJoutai looks. */
static inline u32 fightPokemonGetJoutaiStatus(void* pokemon, u16 id) {
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

    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8 || joutaiDataBiosGetStatus(id) == 0xCD) {
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
    } else if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return 0;
    } else {
        return joutaiGetBuff(pokemon, id);
    }
}

/* Byte 0 of the random status the joutai `id` saved (-1 when none). */
static inline s32 fightPokemonGetJoutaiRnd0(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
    extern s32 pokemonGetJoutaiKaisuu(void* obj, u16 id);
    extern s32 joutaiGetKaisuuJoutaiDataId(void* obj, u16 id);
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
            return pokemonGetJoutaiKaisuu(data, id);
        } else if (joutaiDataBiosGetStatus(id) != 0xCD) {
            return -1;
        } else {
            return joutaiGetKaisuuJoutaiDataId(slot, id);
        }
    } else if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return -1;
    } else {
        return joutaiGetKaisuuJoutaiDataId(pokemon, id);
    }
}

/* Byte 1 of the random status the joutai `id` saved (-1 when none). */
static inline s32 fightPokemonGetJoutaiRnd1(void* pokemon, u16 id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
    extern s32 fn_8012182C(void* obj, u16 id);
    extern s32 joutaiGetNowKaisuuJoutaiDataId(void* obj, u16 id);
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
            return fn_8012182C(data, id);
        } else if (joutaiDataBiosGetStatus(id) != 0xCD) {
            return -1;
        } else {
            return joutaiGetNowKaisuuJoutaiDataId(slot, id);
        }
    } else if (joutaiDataBiosGetStatus(id) != 0xD8) {
        return -1;
    } else {
        return joutaiGetNowKaisuuJoutaiDataId(pokemon, id);
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

/* The Pokemon record held in `pokemon`'s 0xD6 slot, or NULL. */
static inline void* fightPokemonGetSlotPokemon(void* pokemon) {
    void* slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (slot == NULL) {
        return NULL;
    }
    return PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

void fightOutPokemonGetRndStatus(void* pokemon, u32* outStatus, u32* outRndStatus);

#if !defined(FIGHT_POKEMON_EXACT_ISLAND) || defined(FIGHT_POKEMON_EXACT_801FDB78_ONLY)
/* 0x801FDB78 | size: 0x5F0 | large */
void fightOutPokemonGetRndStatus(void* pokemon, u32* outStatus, u32* outRndStatus) {
    void* data;
    u32 status;
    u32 rnd;
    u8 rnd0;
    u8 rnd1;

    if (fightPokemonIsJoutai(pokemon, 0x10) == 1) {
        status = fightPokemonGetJoutaiStatus(pokemon, 0x10);
        rnd0 = fightPokemonGetJoutaiRnd0(pokemon, 0x10);
        rnd1 = fightPokemonGetJoutaiRnd1(pokemon, 0x10);
        rnd = rnd0 | (rnd1 << 8) | (fightPokemonGetJoutaiRnd2(pokemon, 0x10) << 16);
    } else {
        if (pokemon == NULL) {
            data = NULL;
        } else {
            data = fightPokemonGetSlotPokemon(pokemon);
        }
        rnd = (u32)pokemonGetStatus(data, 0, POKEMON_STATUS_OT_ID, 0);
        status = (u32)pokemonGetStatus(data, 0, POKEMON_STATUS_PERSONALITY, 0);
    }
    if (outStatus != NULL) {
        *outStatus = status;
    }
    if (outRndStatus != NULL) {
        *outRndStatus = rnd;
    }
}
#endif

#if !defined(FIGHT_POKEMON_EXACT_ISLAND) || defined(FIGHT_POKEMON_EXACT_801FE168_ONLY)
/* 0x801FE168 | size: 0x290 | large */
void fightOutPokemonToMenuPokemonStatus(void* pokemon, u8* dest) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosCopy(u32* dst, u32* src);
#endif
    extern void pokemonToMenuPokemonStatus(void* bios, u8* dest);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonToMenuPokemonStatusSubBar(void* pokemon, u8* dest);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 menuSubGetPokemonSexForFightDisp(void* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32 context, void* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerIsGcHero(void* trainer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetStatus(u32 a, u32 b, u32 field, u32 c);
#endif
    u32 bios[0x4E];
    u32 status;
    u32 rndStatus;
    void* slot;
    void* partner;
    void* trainer;
    u8 isHero;

    pokemonBiosCopy(bios, fightPokemonGetSlotPokemon(pokemon));
    if (fightPokemonIsJoutai(pokemon, 0x10) == 1) {
        fightOutPokemonGetRndStatus(pokemon, &status, &rndStatus);
        pokemonSetStatus(bios, 0, POKEMON_STATUS_PERSONALITY, 0, status);
        pokemonSetStatus(bios, 0, POKEMON_STATUS_OT_ID, 0, rndStatus);
    }
    pokemonToMenuPokemonStatus(bios, dest);
    slot = PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0));
    if (slot == NULL) {
        partner = NULL;
    } else {
        partner = PC_PTR(pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
    }
    if (partner != NULL) {
        pokemonToMenuPokemonStatusSubBar(partner, dest);
        dest[0x28] = menuSubGetPokemonSexForFightDisp(partner);
    }
    dest[0x16] = 1;
    trainer = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, pokemon);
    if (trainer == NULL) {
        isHero = 0;
    } else if (fightTrainerIsGcHero(trainer) == 1) {
        isHero = 1;
    } else {
        isHero = 0;
    }
    if (isHero == 1) {
        dest[0x16] = 0;
    }
    if (fightFloorGetStatus(0, 0, 0x32, 0) == 0) {
        dest[0x16] = 1;
    }
}
#endif

#if !defined(FIGHT_POKEMON_EXACT_ISLAND)

/* 0x801FE3F8 | size: 0x70 */
void fightOutPokemonToMenuWazaStatus(void* context, u8* dest) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonToMenuWazaStatus(void* pokemon, u8* dest);
#endif
    void* partyList;
    void* pokemon;

    partyList = pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0);
    if (partyList == NULL) {
        pokemon = NULL;
    } else {
        pokemon = pokemonGetStatus(partyList, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
    }
    if (pokemon != NULL) {
        pokemonToMenuWazaStatus(pokemon, dest);
    }
    *(u32*)(dest + 0x40) = (u32)context;
}

/* 0x801FE468 | size: 0xF4 */
void fightPokemonToMenuLvupStatus(void* context, u8* dest) {
    void* pokemon;

    if (context != NULL && dest != NULL) {
        if (context == NULL) {
            pokemon = NULL;
        } else {
            pokemon = pokemonGetStatus(context, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
        }
        *(u16*)(dest + 0x2) = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MAX_HP, 0);
        *(u16*)(dest + 0x4) = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_ATTACK, 0);
        *(u16*)(dest + 0x6) = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_DEFENSE, 0);
        *(u16*)(dest + 0xA) = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SP_ATTACK, 0);
        *(u16*)(dest + 0xC) = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SP_DEFENSE, 0);
        *(u16*)(dest + 0x8) = (u16)(u32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPEED, 0);
        *(u8*)(dest + 0x0) = 0;
    }
}

/* 0x801FE55C | size: 0x78 */
void fightOutPokemonAddFightOutPokemonEnemyDamage(void* self, void* other, u32 offset) {
    extern void* fightOutPokemonEnemySearchAry(void* data, u32 mode, void* key);
    extern u32 fightOutPokemonEnemyBiosGetDamage(void* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonEnemyBiosSetDamage(void* ptr, u32 val);
#endif
    void* data;
    void* result;
    u32 val;

    if (self != NULL) {
        if (other != NULL) {
            if (self != other) {
                data = pokemonGetStatus(other, 0, 0x122, 0);
                if ((result = fightOutPokemonEnemySearchAry(data, 4, self)) != NULL) {
                    val = fightOutPokemonEnemyBiosGetDamage(result);
                    val += offset;
                    fightOutPokemonEnemyBiosSetDamage(result, val);
                }
            }
        }
    }
}

/* 0x801FE5D4 | size: 0x13C */
typedef struct { u16 fields[18]; } FieldTable18;
static inline void* fn_801FE5D4_getSrcSlot(void* context)
{
    return pokemonGetStatus(context, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
}

static inline void* fn_801FE5D4_getPokemon(void* slot)
{
    return pokemonGetStatus(slot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
}

void fightOutPokemonSetHensinStatusAfterLevelUp(void* context) {
    void* destPokemon;
    FieldTable18 table;
    extern FieldTable18 lbl_80279CE4;
    void* srcPokemon;
    u16 fieldId;
    void* destSlot;
    void* srcSlot;
    u8 i;

    table = lbl_80279CE4;
    for (i = 0; i < 18; i++) {
        fieldId = table.fields[i];
        srcSlot = fn_801FE5D4_getSrcSlot(context);
        destSlot = pokemonGetStatus(context, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
        if (srcSlot != NULL && destSlot != NULL) {
            if (srcSlot == NULL) {
                srcPokemon = NULL;
            } else {
                {
                    void* tmp = pokemonGetStatus(srcSlot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
                    srcPokemon = tmp;
                }
            }
            if (destSlot == NULL) {
                if (context != 0) {
                }
                destPokemon = NULL;
            } else {
                destPokemon = fn_801FE5D4_getPokemon(destSlot);
            }
            pokemonSetStatus(destPokemon, 0, fieldId, 0, (u32)pokemonGetStatus(srcPokemon, 0, fieldId, 0));
        }
    }
}

/* 0x801FE710 | size: 0xDC | ClearTrainerEventState */
void fightOutPokemonSetHensinFightPokemonStatusId(void* trainer, u16 eventId, u32 param) {
    void* src;
    void* dst;
    u32 val;

    if ((u8)param == 1) {
        src = pokemonGetStatus(trainer, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
        dst = pokemonGetStatus(trainer, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
    } else {
        src = pokemonGetStatus(trainer, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
        dst = pokemonGetStatus(trainer, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    }
    if (src == NULL) {
        return;
    }
    if (dst == NULL) {
        return;
    }
    val = (u32)pokemonGetStatus(src, 0, eventId, 0);
    pokemonSetStatus(dst, 0, eventId, 0, val);
}

/* 0x801FE7EC | size: 0x130 */
void fightOutPokemonSetHensinPokemonStatusId(void* trainer, u16 eventId, u32 param1, u32 param2) {
    void* srcSlot;
    void* srcPokemon;
    void* dst;
    u32 value;

    if ((u8)param2 == 1) {
        srcSlot = pokemonGetStatus(trainer, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
        srcPokemon = pokemonGetStatus(trainer, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
        dst = srcPokemon;
    } else {
        srcSlot = pokemonGetStatus(trainer, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
        dst = pokemonGetStatus(trainer, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    }
    if (srcSlot != NULL && dst != NULL) {
        if (srcSlot == NULL) {
            srcPokemon = NULL;
        } else {
            void* temp = pokemonGetStatus(srcSlot, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
            srcPokemon = temp;
        }
        if (dst == NULL) {
            dst = NULL;
        } else {
            dst = fn_801FE5D4_getPokemon(dst);
        }
        value = (u32)pokemonGetStatus(srcPokemon, 0, eventId, param1);
        pokemonSetStatus(dst, 0, eventId, param1, value);
    }
}

/* 0x801FE91C | size: 0x2F4 | large */
void fightOutPokemonCopyHensinStatus(void) {
    extern u8 lbl_80279CB8[];
    extern u8 lbl_80279CD4[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    u8 sp[0x60];
    u32 r0 = 0;
    u32 r1 = (u32)sp;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r7 = 0;
    u32 r25 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;
    void (*ctr_fn)(void) = 0;
    u32 ctr = 0;

    r5 = (u32)lbl_80279CB8;
    r0 = 0x3;
    r5 = (u32)lbl_80279CB8;
    r7 = (u32)sp + 0x14;
    r31 = r4;
    r30 = r3;
    /* subi r4, r5, 0x4 */;
    ctr_fn = (void(*)(void))r0;
    do {
        r3 = *(u32*)((u8*)r4 + 0x4);
        r0 = *(u32*)((u8*)r4 + 0x8);
        *(u32*)((u8*)r7 + 0x4) = r3;
        r7 += 8; *(u32*)r7 = r0;
    } while (--ctr != 0);
    r0 = *(u32*)((u8*)r4 + 0x4);
    r3 = (u32)lbl_80279CD4;
    r6 = (u32)lbl_80279CD4;
    *(u32*)((u8*)r7 + 0x4) = r0;
    r5 = *(u32*)((u8*)r6 + 0x0);
    r4 = *(u32*)((u8*)r6 + 0x4);
    r3 = *(u32*)((u8*)r6 + 0x8);
    r0 = *(u16*)((u8*)r6 + 0xC);
    *(u16*)(sp + 0x14) = r0;
    if (r30 == (u32)0x0) return;
    if (r31 == (u32)0x0) return;
    if (r30 == (u32)0x0) {
        r28 = 0x0;
    } else {

        r3 = r30;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        if (r3 == (u32)0x0) {
            r3 = 0x0;
        } else {

            r4 = 0x0;
            r5 = 0xcc;
            r6 = 0x0;
            ((void(*)(void))pokemonGetStatus)();
        }
        r28 = r3;
    }
    if (r31 == (u32)0x0) {
        r27 = 0x0;
    } else {

        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        if (r3 == (u32)0x0) {
            r3 = 0x0;
        } else {

            r4 = 0x0;
            r5 = 0xcc;
            r6 = 0x0;
            ((void(*)(void))pokemonGetStatus)();
        }
        r27 = r3;
    }
    r29 = (u32)sp + 0x18;
    r25 = 0x0;
    while (1) {
        r0 = r25 & 0xFF;
        if (r0 >= (u32)0xe) break;
        /* clrlslwi r0, r25, 24, 1 */;
        r3 = r28;
        r26 = *(u16*)(r29 + r0);
        r4 = 0x0;
        r6 = 0x0;
        r5 = r26;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r0 = r3;
        r3 = r27;
        r7 = r0;
        r5 = r26;
        r4 = 0x0;
        r6 = 0x0;
        ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
        r25 = r25 + 0x1;

    }
    r29 = 0x0;
    while (1) {
        r0 = r29 & 0xFF;
        if (r0 >= (u32)0x4) break;
        r26 = r29 & 0xFF;
        r3 = r28;
        r6 = r26;
        r4 = 0x0;
        r5 = 0x7f;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r25 = r3;
        r3 = r27;
        r6 = r26;
        r4 = 0x0;
        r7 = r25;
        r5 = 0x7f;
        ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
        r4 = r25 & 0xFFFF;
        r3 = 0x0;
        r5 = 0x2;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))wazaGetStatus)(r3, r4, r5, r6);
        r0 = r3 & 0xFF;
        if (r0 < (u32)0x5) {
            r3 = r28;
            r6 = r26;
            r4 = 0x0;
            r5 = 0x80;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            r7 = r3;
            r3 = r27;
            r6 = r26;
            r4 = 0x0;
            r5 = 0x80;
            ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
        } else {

            r3 = r27;
            r6 = r26;
            r4 = 0x0;
            r5 = 0x80;
            r7 = 0x5;
            ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
        }
        r29 = r29 + 0x1;

    }
    r29 = (u32)sp + 0x8;
    r27 = 0x0;
    while (1) {
        r0 = r27 & 0xFF;
        if (r0 >= (u32)0x7) break;
        /* clrlslwi r0, r27, 24, 1 */;
        r3 = r30;
        r26 = *(u16*)(r29 + r0);
        r4 = 0x0;
        r6 = 0x0;
        r5 = r26;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r7 = r3;
        r3 = r31;
        r5 = r26;
        r4 = 0x0;
        r6 = 0x0;
        ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
        r27 = r27 + 0x1;

    }
    r3 = r30;
    r4 = 0x0;
    r5 = 0x100;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    r7 = r3 & 0xFFFF;
    r3 = r31;
    r4 = 0x0;
    r5 = 0x100;
    r6 = 0x0;
    ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
    r27 = 0x0;
    while (1) {
        r0 = r27 & 0xFF;
        if (r0 >= (u32)0x2) break;
        r26 = r27 & 0xFF;
        r3 = r30;
        r6 = r26;
        r4 = 0x0;
        r5 = 0xff;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r7 = r3 & 0xFFFF;
        r3 = r31;
        r6 = r26;
        r4 = 0x0;
        r5 = 0xff;
        ((void (*)(RegWord, RegWord, RegWord, RegWord, RegWord))pokemonSetStatus)(r3, r4, r5, r6, r7);
        r27 = r27 + 0x1;

    }

    return;
}

/* 0x801FEC10 | size: 0xC4 */
typedef struct { u8 data[0x154]; } CopyBlock_0x154;
u32 fightOutPokemonUseHensinBuff(void* context) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonSetStatus();
#endif
    CopyBlock_0x154* src;
    CopyBlock_0x154* dest;
    register CopyBlock_0x154* dest2;

    if (context == NULL) {
        return 0;
    }
    src = (CopyBlock_0x154*)pokemonGetStatus(context, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    dest = (CopyBlock_0x154*)pokemonGetStatus(context, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
    dest2 = dest;
    if (src == NULL) {
        return 0;
    }
    if (dest2 == NULL) {
        return 0;
    }
    *dest2 = *src;
    pokemonSetStatus(context, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0, (u32)dest2);
    return 1;
}

/* 0x801FECD4 | size: 0x68 | CheckTrainerEventState */
u8 fightOutPokemonIsUseHensinBuff(void* trainer) {
    void* a;
    void* b;

    if (trainer == NULL) {
        return 0;
    }
    a = pokemonGetStatus(trainer, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0);
    b = pokemonGetStatus(trainer, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0);
    return (u8)(a == b);
}

/* 0x801FED3C | size: 0x238 | large */
void fightOutPokemonSetJoutaiMigawariHp(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiSetBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiIsJoutaiDataId();
#endif
    extern void pokemonSetJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonIsJoutaiDataId();
#endif
    u8 sp[0x20];
    u32 r0 = 0;
    u32 r1 = (u32)sp;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;

    /* mr. r30, r3 */;
    r31 = r4;
    if ((s32)r0 == (s32)0) return;
    r3 = 0x14;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x14;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r30;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r29 = r3;
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x14;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r29 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r29;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x14;
            pokemonIsJoutaiDataId();
            goto L_801FEE58;
            }
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;
            goto L_801FEE58;
        }
        r3 = r29;
        r4 = 0x14;
        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

            } else {
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r3 = 0x0;

        } else {
            r3 = r30;
            r4 = 0x14;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
            }
    L_801FEE58: ;
    r0 = r3 & 0xFF;
    if (r0 != (u32)0x1) return;
    r3 = 0x14;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x14;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r30;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r29 = r3;
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x14;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r29 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r29;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r5 = r31;
            r4 = 0x14;
            pokemonSetJoutai08();
            return;
            }
        r3 = 0x14;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) return;
        r3 = r29;
        r5 = r31;
        r4 = 0x14;
        ((void (*)(RegWord, RegWord, RegWord))joutaiSetBuff)(r3, r4, r5);
        return;
            }
    r3 = 0x14;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0xd8) return;
    r3 = r30;
    r5 = r31;
    r4 = 0x14;
    ((void (*)(RegWord, RegWord, RegWord))joutaiSetBuff)(r3, r4, r5);

    return;
}

/* 0x801FEF74 | size: 0x248 | large */
/* 0x801FEF74 | size: 0x248 | large | 99.25%: real correct C, RELOC/scheduling-tie wall (see WALLS/equivalent.txt) */
u8 fightOutPokemonGetJoutaiMigawariHp(void* trainer) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiIsJoutaiDataId(void* obj, s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(void* obj, u32 field);
#endif
    void* obj;
    void* target;
    void* obj2;
    void* target2;
    u8 status;

    if (trainer == NULL) {
        return 0;
    }
    if (joutaiDataBiosGetStatus(0x14) == 0x7C || joutaiDataBiosGetStatus(0x14) == 0xC8 || joutaiDataBiosGetStatus(0x14) == 0xCD) {
        obj = pokemonGetStatus(trainer, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0);
        if (joutaiDataBiosGetStatus(0x14) == 0x7C || joutaiDataBiosGetStatus(0x14) == 0xC8) {
            if (obj == NULL) {
                target = NULL;
            } else {
                target = pokemonGetStatus(obj, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
            }
            status = pokemonIsJoutaiDataId(target, 0x14);
        } else if (joutaiDataBiosGetStatus(0x14) != 0xCD) {
            status = 0;
        } else {
            status = joutaiIsJoutaiDataId(obj, 0x14);
        }
    } else if (joutaiDataBiosGetStatus(0x14) != 0xD8) {
        status = 0;
    } else {
        status = joutaiIsJoutaiDataId(trainer, 0x14);
    }
    if (status == 1) {
        if (joutaiDataBiosGetStatus(0x14) == 0x7C || joutaiDataBiosGetStatus(0x14) == 0xC8 || joutaiDataBiosGetStatus(0x14) == 0xCD) {
            obj2 = pokemonGetStatus(trainer, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0);
            if (joutaiDataBiosGetStatus(0x14) == 0x7C || joutaiDataBiosGetStatus(0x14) == 0xC8) {
                if (obj2 == NULL) {
                    target2 = NULL;
                } else {
                    target2 = pokemonGetStatus(obj2, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0);
                }
                return pokemonGetJoutai08(target2, 0x14);
            } else if (joutaiDataBiosGetStatus(0x14) != 0xCD) {
                return 0;
            } else {
                return joutaiGetBuff(obj2, 0x14);
            }
        } else if (joutaiDataBiosGetStatus(0x14) != 0xD8) {
            return 0;
        } else {
            return joutaiGetBuff(trainer, 0x14);
        }
    }
    return 0;
}

/* 0x801FF1BC | size: 0x974 | massive */
u8 fightOutPokemonCheckFightActionWazaSelect(void* fighter, u8 mode) {
    extern u8 lbl_80375CA8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiGetBuff();
#endif
    extern void joutaiGetIdBuff();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonGetJoutai08();
#endif
    extern void pokemonGetJoutai0C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai();
#endif
    extern void pokemonCheckFightOut();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std103_battleGetResult();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
    extern void fightActionCreate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorGetStatus();
#endif
    extern void fightOutPokemonGetOutOkWazaBanmeAry();
    extern void fightWazaCreate();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightActionBiosSetBuffDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightSeqSelectTarget();
#endif
    u8 sp[0x20];
    u32 r0 = 0;
    u32 r1 = (u32)sp;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r7 = 0;
    u32 r8 = 0;
    u32 r9 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = mode;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = (u32)fighter;

    r5 = 0x14;
    r6 = 0x0;
    r3 = 0x0;
    r4 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))fightFloorGetStatus)(r3, r4, r5, r6);
    r30 = r3 & 0xFFFF;
    if (r31 == (u32)0x0) {
        r3 = 0x0;
        return;
    }
    if (r31 != (u32)0x0) {

    if (r31 != (u32)0x0) {

    r3 = ((u32 (*)(void))std103_battleGetResult)();
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x1) {

    r3 = r31;
    r4 = 0x0;
    r5 = 0xd6;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    /* mr. r26, r3 */;
    if (r0 != (u32)0x1) {

    if (r0 != (u32)0x1) {

    r3 = ((u32 (*)(void))std103_battleGetResult)();
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x1) {

    r3 = r26;
    r4 = 0x0;
    r5 = 0xcb;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    if (r3 != (u32)0x0) {

    pokemonCheckValid();
    r0 = r3 & 0xFF;
    if (r3 != (u32)0x0) {

    if (r26 == (u32)0x0) {
    r3 = 0x0;
    } else {

    r3 = r26;
    r4 = 0x0;
    r5 = 0xcc;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    }
    if (r3 != (u32)0x0) {

    pokemonCheckValid();
    r0 = r3 & 0xFF;
    if (r3 != (u32)0x0) {

    r3 = r26;
    r4 = 0x0;
    r5 = 0xce;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    if ((s32)r3 >= (s32)0x0) {

    r0 = 0x1;
    }
    }
    }
    }
    }
    }
    }
    r0 = r0 & 0xFF;
    if ((s32)r3 != (s32)0x0) {

    r0 = 0x1;
    }
    }
    }
    }
    r0 = r0 & 0xFF;
    if ((s32)r3 != (s32)0x0) {

        r3 = r31;
        r4 = 0x0;
        r5 = 0x120;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    if ((s32)r3 != (s32)0x1) {

            r3 = r31;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            /* mr. r26, r3 */;
            if ((s32)r3 != (s32)0x1) {

            if ((s32)r3 != (s32)0x1) {

            r3 = ((u32 (*)(void))std103_battleGetResult)();
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0x1) {

            r3 = r26;
            r4 = 0x0;
            r5 = 0xcb;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            if (r3 != (u32)0x0) {

            pokemonCheckValid();
            r0 = r3 & 0xFF;
            if (r3 != (u32)0x0) {

            if (r26 == (u32)0x0) {
            r3 = 0x0;
            } else {

            r3 = r26;
            r4 = 0x0;
            r5 = 0xcc;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            if (r3 != (u32)0x0) {

            pokemonCheckValid();
            r0 = r3 & 0xFF;
            if (r3 != (u32)0x0) {

            r3 = r26;
            r4 = 0x0;
            r5 = 0xce;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            if ((s32)r3 >= (s32)0x0) {

            r0 = 0x1;
            }
    }
    }
    }
    }
    }
    }
            r0 = r0 & 0xFF;
            if ((s32)r3 != (s32)0x0) {

            r3 = r26;
            r4 = 0x0;
            r5 = 0xd2;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            if ((s32)r3 != (s32)0x1) {

            if (r26 == (u32)0x0) {
            r3 = 0x0;
            } else {

            r3 = r26;
            r4 = 0x0;
            r5 = 0xcc;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            if (r3 != (u32)0x0) {

            pokemonCheckFightOut();
            r0 = r3 & 0xFF;
            if (r3 != (u32)0x0) {

            r0 = 0x1;
            }
    }
    }
    }
    }
            r0 = r0 & 0xFF;
    if (r3 != (u32)0x0) {

                r0 = 0x1;
    }
    }
    }
    }
    r0 = r0 & 0xFF;
    if (r3 == (u32)0x0) {
        r3 = 0x0;
        return;
    }
    if (r31 == (u32)0x0) {
        r29 = 0x0;
    } else {

        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        if (r3 == (u32)0x0) {
            r3 = 0x0;
        } else {

            r4 = 0x0;
            r5 = 0xcc;
            r6 = 0x0;
            ((void(*)(void))pokemonGetStatus)();
        }
        r29 = r3;
    }
    r3 = r31;
    r3 = ((u32 (*)(RegWord))fightOutPokemonGetOutOkWazaBanmeAry)(r3);
    r0 = r3 & 0xFF;
    if (r0 == (u32)0x1) {
        r0 = r28 & 0xFF;
        if (r0 != (u32)0x1) {
            r3 = r31;
            r5 = r30;
            r4 = 0xa5;
            r6 = 0x0;
            r7 = 0x1;
            r8 = 0x1;
            r9 = -0x1;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord, RegWord, RegWord, RegWord))fightSeqSelectTarget)(r3, r4, r5, r6, r7, r8, r9);
            r4 = r30;
            r3 = ((u32 (*)(RegWord, RegWord))fightTargetGetTragetPtrToRelativeHostSideFightTargetId)(r3, r4);
            r0 = r3;
            r3 = r31;
            r26 = r0;
            r4 = 0x0;
            r5 = 0xd9;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            if (r3 != (u32)0x0) {
                r6 = r26;
                r4 = -0x1;
                r5 = 0xa5;
                r7 = 0x1;
                fightWazaCreate();
                r3 = r31;
                r4 = 0x0;
                r5 = 0xfe;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                /* mr. r26, r3 */;
                if (r3 != (u32)0x0) {
                    r4 = (u32)lbl_80375CA8;
                    r5 = r31;
                    r8 = (u32)lbl_80375CA8;
                    r6 = 0x13;
                    r4 = 0x0;
                    r7 = 0x0;
                    fightActionCreate();
                    r0 = r3 & 0xFF;
                    if (r0 == (u32)0x1) {
                        r3 = r26;
                        r4 = 0xa5;
                        ((void (*)(RegWord, RegWord))fightActionBiosSetBuffDataId)(r3, r4);
        }
        }
        }
        }
        r3 = 0x1;
        return;
    }
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r26 = r3;
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r26 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r26;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x2a;
            pokemonIsJoutaiDataId();
            goto L_801FF714;
            }
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;
            goto L_801FF714;
        }
        r3 = r26;
        r4 = 0x2a;
        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

            } else {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r3 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x2a;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
            }
    L_801FF714: ;
    r0 = r3 & 0xFF;
    if (r0 != (u32)0x1) { r3 = 0x0; return; }
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r26 = r3;
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r26 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r26;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x2a;
            pokemonGetJoutai0C();

            } else {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) {
                r3 = 0x0;

            } else {
                r3 = r26;
                r4 = 0x2a;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiGetIdBuff)(r3, r4);
            }
            }
        r27 = r3;

            } else {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r27 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x2a;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiGetIdBuff)(r3, r4);
            r27 = r3;
        }
            }
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r26 = r3;
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r26 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r26;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x2a;
            pokemonGetJoutai08();

            } else {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) {
                r3 = 0x0;

            } else {
                r3 = r26;
                r4 = 0x2a;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiGetBuff)(r3, r4);
            }
            }
        r26 = r3;

            } else {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r26 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x2a;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiGetBuff)(r3, r4);
            r26 = r3;
        }
            }
    r3 = r29;
    r6 = r26 & 0xFFFF;
    r4 = 0x0;
    r5 = 0x7f;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    r0 = r28 & 0xFF;
    r28 = r3 & 0xFFFF;
    if (r0 == (u32)0xd8) { r3 = 0x2; return; }
    r0 = r27 & 0xFFFF;
    do {
    if (r0 == (u32)r28) break;
    r3 = r31;
    r4 = 0x0;
    r5 = 0xee;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r27 = r3;
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r27 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r27;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x2a;
            pokemonInitPersistentJoutai();
            break;
            }
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) break;
        r3 = r27;
        r4 = 0x2a;
        ((void (*)(RegWord, RegWord))joutaiInitJoutaiDataId)(r3, r4);
        break;
            }
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0xd8) break;
    r3 = r31;
    r4 = 0x2a;
    ((void (*)(RegWord, RegWord))joutaiInitJoutaiDataId)(r3, r4);
    } while (0);
    r3 = r31;
    r4 = r28;
    r5 = r30;
    r27 = (s8)r26;
    r6 = 0x0;
    r7 = 0x1;
    r8 = 0x1;
    r9 = -0x1;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord, RegWord, RegWord, RegWord))fightSeqSelectTarget)(r3, r4, r5, r6, r7, r8, r9);
    r4 = r30;
    r3 = ((u32 (*)(RegWord, RegWord))fightTargetGetTragetPtrToRelativeHostSideFightTargetId)(r3, r4);
    r26 = r3;
    r3 = r31;
    r4 = 0x0;
    r5 = 0xd9;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    if (r3 == (u32)0x0) { r3 = 0x2; return; }
    r4 = r27;
    r5 = r28;
    r6 = r26;
    r7 = 0x1;
    fightWazaCreate();
    r3 = r31;
    r4 = 0x0;
    r5 = 0xfe;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    /* mr. r26, r3 */;
    if (r3 == (u32)0x0) { r3 = 0x2; return; }
    r4 = (u32)lbl_80375CA8;
    r5 = r31;
    r8 = (u32)lbl_80375CA8;
    r6 = 0x13;
    r4 = 0x0;
    r7 = 0x0;
    fightActionCreate();
    r0 = r3 & 0xFF;
    if (r0 != (u32)0x1) { r3 = 0x2; return; }
    r3 = r26;
    r4 = r28;
    ((void (*)(RegWord, RegWord))fightActionBiosSetBuffDataId)(r3, r4);

    r3 = 0x2;
    return;

    r3 = 0x0;

    return;
}

/* 0x801FFB30 | size: 0x398 | large */
void fightOutPokemonGetOutOkWazaBanmeAry(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiGetBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonGetJoutai08();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonCheckCanOutOkWazaBanme();
#endif
    u8 sp[0x20];
    u32 r0 = 0;
    u32 r1 = (u32)sp;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;

    /* mr. r31, r3 */;
    if ((s32)r0 == (s32)0) {
        r30 = 0x0;

    } else {
        r3 = 0x0;
        while (1) {
            r0 = r3 & 0xFF;
            if (r0 >= (u32)0x4) break;
            r3 = r3 + 0x1;

        }
        r30 = 0x0;
        r28 = r30;
        while (1) {
            r0 = r28 & 0xFF;
            if (r0 >= (u32)0x4) break;
            if (r31 != (u32)0x0) {

            r3 = r31;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            if (r3 != (u32)0x0) {

                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                ((void(*)(void))pokemonGetStatus)();
            }
        }
            r29 = r28 & 0xFF;
            r4 = r29;
            pokemonWazaCheckValid();
            r0 = r3 & 0xFF;
            do {
            if (r3 == (u32)0x0) break;
            if (r31 != (u32)0x0) {

            r3 = r31;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            if (r3 != (u32)0x0) {

                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                ((void(*)(void))pokemonGetStatus)();
            }
        }
            r6 = r29;
            r4 = 0x0;
            r5 = 0x7f;
            ((void(*)(void))pokemonGetStatus)();
            r0 = r3 & 0xFFFF;
            if (r3 == (u32)0x0 || r0 == (u32)MOVE_SHADOW_LOCKED || r0 == (u32)MOVE_UNUSED_355) break;

            r3 = r31;
            r4 = r29;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))fightOutPokemonCheckCanOutOkWazaBanme)(r3, r4, r5, r6);
            r0 = r3 & 0xFF;
            if (r0 != (u32)0x163) break;
            r30 = r30 + 0x1;
            } while (0);
            r28 = r28 + 0x1;

        }
    }
    r0 = r30 & 0xFF;
    if (r0 == (u32)0x4) {
        r3 = 0x1;
        return;
    }
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r30 = r3;
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r30 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r30;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x2a;
            pokemonIsJoutaiDataId();
            goto L_801FFD78;
            }
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;
            goto L_801FFD78;
        }
        r3 = r30;
        r4 = 0x2a;
        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

            } else {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r3 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x2a;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
            }
    L_801FFD78: ;
    r0 = r3 & 0xFF;
    if (r0 != (u32)0x1) { r3 = 0x0; return; }
    r3 = 0x2a;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r30 = r3;
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r30 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r30;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x2a;
            pokemonGetJoutai08();

            } else {
            r3 = 0x2a;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) {
                r3 = 0x0;

            } else {
                r3 = r30;
                r4 = 0x2a;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiGetBuff)(r3, r4);
            }
            }
        r0 = r3;

            } else {
        r3 = 0x2a;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r0 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x2a;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiGetBuff)(r3, r4);
            r0 = r3;
        }
            }
    r3 = r31;
    r4 = r0 & 0xFFFF;
    r5 = 0x0;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))fightOutPokemonCheckCanOutOkWazaBanme)(r3, r4, r5, r6);
    r0 = r3 & 0xFF;
    if (r0 == (u32)0xd8) { r3 = 0x0; return; }
    r3 = 0x1;
    return;

    r3 = 0x0;

    return;
}

/* 0x801FFEC8 | size: 0xB94 | massive */
u8 fightOutPokemonCheckCanOutOkWazaBanme(void* fighter, u32 slot, u32 mode, u16* output) {
    extern u8 lbl_80279C90[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiDataBiosGetStatus();
#endif
    extern void joutaiGetIdBuff();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    extern void pokemonGetJoutai0C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonGetSoubiItemSoubiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerClearState();
#endif
    extern void fightFloorCheckHuuinWazaFightOutPokemon();
    u8 sp[0x50];
    u32 r0 = 0;
    u32 r1 = (u32)sp;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r20 = 0;
    u32 r21 = 0;
    u32 r22 = 0;
    u32 r23 = 0;
    u32 r24 = 0;
    u32 r25 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;
    void (*ctr_fn)(void) = 0;
    u32 ctr = 0;

    r31 = (u32)fighter;
    r25 = slot;
    r24 = mode;
    r23 = (u32)output;
    r30 = 0x0;
    if ((s32)r31 == (s32)0) {
        r3 = 0x6;
        return;
    }
    if ((s32)r24 == (s32)0) {
        r26 = 0x0;
    } else {

        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        ((void(*)(void))pokemonGetStatus)();
        if (r3 == (u32)0x0) {
            r3 = 0x0;
        } else {

            r4 = 0x0;
            r5 = 0xcc;
            r6 = 0x0;
            ((void(*)(void))pokemonGetStatus)();
        }
        r26 = r3;
    }
    r3 = r26;
    r6 = r25;
    r4 = 0x0;
    r5 = 0x7f;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    r29 = r3 & 0xFFFF;
    r3 = 0x0;
    r4 = r29;
    r5 = 0x7;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))wazaGetStatus)(r3, r4, r5, r6);
    r22 = r3 & 0xFFFF;
    r3 = r26;
    r6 = r25;
    r4 = 0x0;
    r5 = 0x80;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    r21 = r3 & 0xFF;
    r3 = r31;
    r4 = 0x0;
    r5 = 0xd6;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    if ((r28 = r3) == (u32)0x0) {
        r27 = 0x0;
    } else {

        r4 = 0x0;
        r5 = 0xcc;
        r6 = 0x0;
        ((void(*)(void))pokemonGetStatus)();
        r27 = r3;
    }
    if (r27 != (u32)0x0) {

    r3 = 0x3d;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x3d;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 == (u32)0xc8) {
        }
        if (r28 == (u32)0x0) {
        r3 = 0x0;
        } else {

        r3 = r28;
        r4 = 0x0;
        r5 = 0xcc;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        }
        r4 = 0x3d;
        pokemonIsJoutaiDataId();

        } else {
        r3 = 0x3d;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;

        } else {
            r3 = r28;
            r4 = 0x3d;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
        }
    r0 = r3 & 0xFF;
    if (r0 != (u32)0x1) {

        r3 = r27;
        r3 = ((u32 (*)(RegWord))pokemonGetSoubiItemSoubiDataId)(r3);
        r27 = r3;
    }
    }
    r3 = r31;
    r4 = 0x0;
    r5 = 0xf0;
    r6 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
    r28 = r3 & 0xFFFF;
    r3 = 0x29;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x29;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x29;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r20 = r3;
        r3 = 0x29;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x29;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r20 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r20;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x29;
            pokemonIsJoutaiDataId();
            goto L_80200180;
            }
        r3 = 0x29;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;
            goto L_80200180;
        }
        r3 = r20;
        r4 = 0x29;
        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

            } else {
        r3 = 0x29;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r3 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x29;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
            }
    L_80200180: ;
    r0 = r3 & 0xFF;
    if (r0 == (u32)0x1) {
        r3 = 0x29;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x29;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xc8) {
                r3 = 0x29;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xcd) {
        }
            }
            r3 = r31;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            r20 = r3;
            r3 = 0x29;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0x7c) {
                r3 = 0x29;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xc8) {
                }
                if (r20 == (u32)0x0) {
                    r3 = 0x0;
                } else {

                    r3 = r20;
                    r4 = 0x0;
                    r5 = 0xcc;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                }
                r4 = 0x29;
                pokemonGetJoutai0C();
                goto L_8020028C;
                }
            r3 = 0x29;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) {
                r3 = 0x0;
                goto L_8020028C;
            }
            r3 = r20;
            r4 = 0x29;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiGetIdBuff)(r3, r4);

                } else {
            r3 = 0x29;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xd8) {
                r3 = 0x0;

            } else {
                r3 = r31;
                r4 = 0x29;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiGetIdBuff)(r3, r4);
            }
                }
        L_8020028C: ;
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8 && r0 == (u32)r29 && r0 != (u32)0x165) {

            r30 = 0x1;
    }
    }
    r3 = 0x1b;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x1b;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x1b;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r20 = r3;
        r3 = 0x1b;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x1b;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r20 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r20;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x1b;
            pokemonIsJoutaiDataId();
            goto L_802003A8;
            }
        r3 = 0x1b;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;
            goto L_802003A8;
        }
        r3 = r20;
        r4 = 0x1b;
        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

            } else {
        r3 = 0x1b;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r3 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x1b;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
            }
    L_802003A8: ;
    r0 = r3 & 0xFF;
    if (r0 != (u32)0x1 || r29 != (u32)r28 || r29 != (u32)0xa5) {

        r0 = r24 & 0xFF;
        if (r0 == (u32)0x1) {
            r3 = (u32)lbl_80279C90;
            r0 = 0x2;
            r3 = (u32)lbl_80279C90;
            r5 = (u32)sp + 0x4;
            /* subi r4, r3, 0x4 */;
            ctr_fn = (void(*)(void))r0;
            do {
                r3 = *(u32*)((u8*)r4 + 0x4);
                r0 = *(u32*)((u8*)r4 + 0x8);
                *(u32*)((u8*)r5 + 0x4) = r3;
                r5 += 8; *(u32*)r5 = r0;
            } while (--ctr != 0);
            r0 = *(u16*)((u8*)r4 + 0x4);
            *(u16*)((u8*)r5 + 0x4) = r0;
            if (r31 != (u32)0x0) {
                r24 = (u32)sp + 0x8;
                r30 = 0x0;
                while (1) {
                    r0 = r30 & 0xFF;
                    if (r0 >= (u32)0x9) break;
                    /* clrlslwi r0, r30, 24, 1 */;
                    r28 = *(u16*)(r24 + r0);
                    r3 = r28;
                    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                    r0 = r3 & 0xFFFF;
                    if (r0 != (u32)0x7c) {
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0xc8) {
                            r3 = r28;
                            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                            r0 = r3 & 0xFFFF;
                            if (r0 == (u32)0xcd) {
                }
                        }
                        r3 = r31;
                        r4 = 0x0;
                        r5 = 0xd6;
                        r6 = 0x0;
                        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                        r20 = r3;
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0x7c) {
                            r3 = r28;
                            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                            r0 = r3 & 0xFFFF;
                            if (r0 == (u32)0xc8) {
                            }
                            if (r20 == (u32)0x0) {
                                r3 = 0x0;
                            } else {

                                r3 = r20;
                                r4 = 0x0;
                                r5 = 0xcc;
                                r6 = 0x0;
                                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                            }
                            r4 = r28;
                            pokemonIsJoutaiDataId();
                            goto L_80200520;
                            }
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0xcd) {
                            r3 = 0x0;
                            goto L_80200520;
                        }
                        r3 = r20;
                        r4 = r28;
                        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

                            } else {
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0xd8) {
                            r3 = 0x0;

                        } else {
                            r3 = r31;
                            r4 = r28;
                            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
                        }
                            }
                    L_80200520: ;
                    r0 = r3 & 0xFF;
                    do {
                    if (r0 != (u32)0x1) break;
                    r3 = r31;
                    r4 = 0x0;
                    r5 = 0xee;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                    r20 = r3;
                    if (r28 == (u32)0x0) {
                        if (r20 != (u32)0x0) {
                            r4 = 0x1;
                            wazaSequenceOwnerClearState();
                            r3 = r20;
                            r4 = 0x2;
                            ((void (*)(RegWord, RegWord))wazaSequenceOwnerClearState)(r3, r4);
                        }

                    } else {
                        if (r20 != (u32)0x0) {
                            if (r28 == (u32)0x8) {
                                r4 = 0x1;
                                wazaSequenceOwnerClearState();
                            }
                            if (r28 == (u32)0x7) {
                                r3 = r20;
                                r4 = 0x2;
                                ((void (*)(RegWord, RegWord))wazaSequenceOwnerClearState)(r3, r4);
                }
                        }
                    }
                    r3 = r28;
                    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                    r0 = r3 & 0xFFFF;
                    if (r0 != (u32)0x7c) {
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0xc8) {
                            r3 = r28;
                            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                            r0 = r3 & 0xFFFF;
                            if (r0 == (u32)0xcd) {
                }
                        }
                        r3 = r31;
                        r4 = 0x0;
                        r5 = 0xd6;
                        r6 = 0x0;
                        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                        r20 = r3;
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0x7c) {
                            r3 = r28;
                            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                            r0 = r3 & 0xFFFF;
                            if (r0 == (u32)0xc8) {
                            }
                            if (r20 == (u32)0x0) {
                                r3 = 0x0;
                            } else {

                                r3 = r20;
                                r4 = 0x0;
                                r5 = 0xcc;
                                r6 = 0x0;
                                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                            }
                            r4 = r28;
                            pokemonInitPersistentJoutai();
                            break;
                            }
                        r3 = r28;
                        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                        r0 = r3 & 0xFFFF;
                        if (r0 != (u32)0xcd) break;
                        r3 = r20;
                        r4 = r28;
                        ((void (*)(RegWord, RegWord))joutaiInitJoutaiDataId)(r3, r4);
                        break;
                            }
                    r3 = r28;
                    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                    r0 = r3 & 0xFFFF;
                    if (r0 != (u32)0xd8) break;
                    r3 = r31;
                    r4 = r28;
                    ((void (*)(RegWord, RegWord))joutaiInitJoutaiDataId)(r3, r4);
                    } while (0);
                    r30 = r30 + 0x1;

                }
        }
        }
        r30 = 0x2;
    }
    r3 = 0x30;
    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
    r0 = r3 & 0xFFFF;
    if (r0 != (u32)0x7c) {
        r3 = 0x30;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xc8) {
            r3 = 0x30;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xcd) {
        }
        }
        r3 = r31;
        r4 = 0x0;
        r5 = 0xd6;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r20 = r3;
        r3 = 0x30;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x30;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 == (u32)0xc8) {
            }
            if (r20 == (u32)0x0) {
                r3 = 0x0;
            } else {

                r3 = r20;
                r4 = 0x0;
                r5 = 0xcc;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            }
            r4 = 0x30;
            pokemonIsJoutaiDataId();
            goto L_8020079C;
            }
        r3 = 0x30;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xcd) {
            r3 = 0x0;
            goto L_8020079C;
        }
        r3 = r20;
        r4 = 0x30;
        r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

            } else {
        r3 = 0x30;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) {
            r3 = 0x0;

        } else {
            r3 = r31;
            r4 = 0x30;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
        }
            }
    L_8020079C: ;
    r0 = r3 & 0xFF;
    if ((r0 == (u32)0x1) && (r22 == (u32)0x0)) {

        r30 = 0x3;
    }
    r4 = r31;
    r5 = r29;
    r3 = 0x0;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord))fightFloorCheckHuuinWazaFightOutPokemon)(r3, r4, r5);
    r0 = r3 & 0xFF;
    if (r0 == (u32)0x1) {
        r30 = 0x4;
    }
    r0 = r27 & 0xFFFF;
    if (r0 == (u32)0x1d) {
        r3 = 0x36;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = 0x36;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xc8) {
                r3 = 0x36;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xcd) {
        }
            }
            r3 = r31;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            r20 = r3;
            r3 = 0x36;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0x7c) {
                r3 = 0x36;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xc8) {
                }
                if (r20 == (u32)0x0) {
                    r3 = 0x0;
                } else {

                    r3 = r20;
                    r4 = 0x0;
                    r5 = 0xcc;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                }
                r4 = 0x36;
                pokemonIsJoutaiDataId();
                goto L_802008E0;
                }
            r3 = 0x36;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) {
                r3 = 0x0;
                goto L_802008E0;
            }
            r3 = r20;
            r4 = 0x36;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

                } else {
            r3 = 0x36;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xd8) {
                r3 = 0x0;

            } else {
                r3 = r31;
                r4 = 0x36;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
            }
                }
        L_802008E0: ;
        r0 = r3 & 0xFF;
        if (r0 == (u32)0x1) {
            r3 = 0x36;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0x7c) {
                r3 = 0x36;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 != (u32)0xc8) {
                    r3 = 0x36;
                    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                    r0 = r3 & 0xFFFF;
                    if (r0 == (u32)0xcd) {
            }
                }
                r3 = r31;
                r4 = 0x0;
                r5 = 0xd6;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                r20 = r3;
                r3 = 0x36;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 != (u32)0x7c) {
                    r3 = 0x36;
                    r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                    r0 = r3 & 0xFFFF;
                    if (r0 == (u32)0xc8) {
                    }
                    if (r20 == (u32)0x0) {
                        r3 = 0x0;
                    } else {

                        r3 = r20;
                        r4 = 0x0;
                        r5 = 0xcc;
                        r6 = 0x0;
                        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                    }
                    r4 = 0x36;
                    pokemonGetJoutai0C();
                    goto L_802009F4;
                    }
                r3 = 0x36;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 != (u32)0xcd) {
                    r3 = 0x0;
                    goto L_802009F4;
                }
                r3 = r20;
                r4 = 0x36;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiGetIdBuff)(r3, r4);
                goto L_802009F4;
                    }
            r3 = 0x36;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xd8) {
                r3 = 0x0;
                goto L_802009F4;
            }
            r3 = r31;
            r4 = 0x36;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiGetIdBuff)(r3, r4);

        } else {
            r3 = 0x0;
        }
        L_802009F4: ;
        r0 = r3 & 0xFFFF;
        if ((r0 != (u32)0xd8) && (r0 != (u32)0x165) && (r0 != (u32)0xffff) && (r0 != (u32)r29)) {

            r30 = 0x5;
        }
        if (r23 != (u32)0x0) {
            *(u16*)((u8*)r23 + 0x0) = r3;
    }
    }
    r3 = r26;
    r4 = r25;
    r3 = ((u32 (*)(RegWord, RegWord))pokemonWazaCheckValid)(r3, r4);
    r0 = r3 & 0xFF;
    if (r23 != (u32)0x0) {
        if (r21 != (u32)0x0) { r3 = r30; return; }
    }
    r30 = 0x6;

    r3 = r30;

    return;
}


/* 0x80200A5C | size: 0xB4 */
typedef struct { u16 fields[9]; } FieldTable9;
u32 fightOutPokemonCheckNoAttackFlag(void* context) {
    extern FieldTable9 lbl_80279CA4;
    FieldTable9 table;
    u8 i;

    table = lbl_80279CA4;
    if (context == NULL) {
        return 0;
    }
    for (i = 0; i < 9; i++) {
        if ((s32)pokemonGetStatus(context, 0, table.fields[i], 0) == 1) {
            return 1;
        }
    }
    return 0;
}

/* 0x80200B10 | size: 0x2F0 | large */
void fightOutPokemonInitJoutaiKeep(void) {
    extern u8 lbl_80279C90[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerClearState();
#endif
    u8 sp[0x40];
    u32 r0 = 0;
    u32 r1 = (u32)sp;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;
    void (*ctr_fn)(void) = 0;
    u32 ctr = 0;

    r4 = (u32)lbl_80279C90;
    r4 = (u32)lbl_80279C90;
    r0 = 0x2;
    r5 = (u32)sp + 0x4;
    r30 = r3;
    /* subi r4, r4, 0x4 */;
    ctr_fn = (void(*)(void))r0;
    do {
        r3 = *(u32*)((u8*)r4 + 0x4);
        r0 = *(u32*)((u8*)r4 + 0x8);
        *(u32*)((u8*)r5 + 0x4) = r3;
        r5 += 8; *(u32*)r5 = r0;
    } while (--ctr != 0);
    r0 = *(u16*)((u8*)r4 + 0x4);
    *(u16*)((u8*)r5 + 0x4) = r0;
    if (r30 == (u32)0x0) return;
    r29 = (u32)sp + 0x8;
    r31 = 0x0;
    while (1) {
        r0 = r31 & 0xFF;
        if (r0 >= (u32)0x9) break;
        /* clrlslwi r0, r31, 24, 1 */;
        r28 = *(u16*)(r29 + r0);
        r3 = r28;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xc8) {
                r3 = r28;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xcd) {
            }
            }
            r3 = r30;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            r27 = r3;
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0x7c) {
                r3 = r28;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xc8) {
                }
                if (r27 == (u32)0x0) {
                    r3 = 0x0;
                } else {

                    r3 = r27;
                    r4 = 0x0;
                    r5 = 0xcc;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                }
                r4 = r28;
                pokemonIsJoutaiDataId();
                goto L_80200C74;
                }
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) {
                r3 = 0x0;
                goto L_80200C74;
            }
            r3 = r27;
            r4 = r28;
            r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);

                } else {
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xd8) {
                r3 = 0x0;

            } else {
                r3 = r30;
                r4 = r28;
                r3 = ((u32 (*)(RegWord, RegWord))joutaiIsJoutaiDataId)(r3, r4);
            }
                }
        L_80200C74: ;
        r0 = r3 & 0xFF;
        do {
        if (r0 != (u32)0x1) break;
        r3 = r30;
        r4 = 0x0;
        r5 = 0xee;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
        r27 = r3;
        if (r28 == (u32)0x0) {
            if (r27 != (u32)0x0) {
                r4 = 0x1;
                wazaSequenceOwnerClearState();
                r3 = r27;
                r4 = 0x2;
                ((void (*)(RegWord, RegWord))wazaSequenceOwnerClearState)(r3, r4);
            }

        } else {
            if (r27 != (u32)0x0) {
                if (r28 == (u32)0x8) {
                    r4 = 0x1;
                    wazaSequenceOwnerClearState();
                }
                if (r28 == (u32)0x7) {
                    r3 = r27;
                    r4 = 0x2;
                    ((void (*)(RegWord, RegWord))wazaSequenceOwnerClearState)(r3, r4);
            }
            }
        }
        r3 = r28;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0x7c) {
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xc8) {
                r3 = r28;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xcd) {
            }
            }
            r3 = r30;
            r4 = 0x0;
            r5 = 0xd6;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
            r27 = r3;
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0x7c) {
                r3 = r28;
                r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
                r0 = r3 & 0xFFFF;
                if (r0 == (u32)0xc8) {
                }
                if (r27 == (u32)0x0) {
                    r3 = 0x0;
                } else {

                    r3 = r27;
                    r4 = 0x0;
                    r5 = 0xcc;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))pokemonGetStatus)(r3, r4, r5, r6);
                }
                r4 = r28;
                pokemonInitPersistentJoutai();
                break;
                }
            r3 = r28;
            r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
            r0 = r3 & 0xFFFF;
            if (r0 != (u32)0xcd) break;
            r3 = r27;
            r4 = r28;
            ((void (*)(RegWord, RegWord))joutaiInitJoutaiDataId)(r3, r4);
            break;
                }
        r3 = r28;
        r3 = ((u32 (*)(RegWord))joutaiDataBiosGetStatus)(r3);
        r0 = r3 & 0xFFFF;
        if (r0 != (u32)0xd8) break;
        r3 = r30;
        r4 = r28;
        ((void (*)(RegWord, RegWord))joutaiInitJoutaiDataId)(r3, r4);
        } while (0);
        r31 = r31 + 0x1;

    }

    return;
}

#endif /* !FIGHT_POKEMON_EXACT_ISLAND */

#if !defined(FIGHT_POKEMON_EXACT_ISLAND) || defined(FIGHT_POKEMON_EXACT_80200E00_ONLY)
/* 0x80200E00 | size: 0x2C8 | large */
void fightOutPokemonInitJoutaiKie(void* pokemon) {
    /* RULE-EXCEPTION(user-approved): extern-named table halves, pokemon carriers and declaration order for register allocation — see docs/RULE_EXCEPTIONS.md */
    extern u32 lbl_8047E518;
    extern u16 lbl_8047E51C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerClearState(void* effect, s32 trajType);
#endif
    void* clearTarget;
    u8 i;
    JoutaiIdTable3 table;
    void* target;
    void* effect;
    u16* ids;

    *(u32*)&table.ids[0] = lbl_8047E518;
    table.ids[2] = lbl_8047E51C;
    target = pokemon;
    if (target == NULL) {
        return;
    }
    ids = table.ids;
    for (i = 0; i < 3; i++) {
        u16 id;
        id = ids[i];
        if (fightPokemonIsJoutai(target, id) == 1) {
            effect = PC_PTR(pokemonGetStatus(target, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            clearTarget = target;
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
            fightPokemonClearJoutai(clearTarget, id);
        }
    }
}

#endif /* 80200E00 island */



#if !defined(FIGHT_POKEMON_EXACT_ISLAND) || defined(FIGHT_POKEMON_EXACT_802010C8_ONLY)
/* 0x802010C8 | size: 0x180 | medium */
u16 fightOutPokemonIsJoutaiKie(void* pokemon) {
    /* RULE-EXCEPTION(user-approved): extern-named stand-ins for the TU's own initialiser data — see docs/RULE_EXCEPTIONS.md */
    extern u32 lbl_8047E510;
    extern u16 lbl_8047E514;
    JoutaiIdTable3 table;
    u16 id;
    u8 i;

    *(u32*)&table.ids[0] = lbl_8047E510;
    table.ids[2] = lbl_8047E514;
    if (pokemon == NULL) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        id = table.ids[i];
        if (fightPokemonIsJoutai(pokemon, id) == 1) {
            return id;
        }
    }
    return 0;
}

#endif

#endif

#if !defined(FIGHT_POKEMON_CANDIDATE_801FDB78_ONLY)

/* 0x80201248 | size: 0xF8 */
u8 fightOutPokemonGetFightOutPokemonEnemyOumuWazaDataIdAry(void* context, u16* output) {
    extern u8 fightOutPokemonEnemyCheckValid(void* entry);
    extern u16 fightOutPokemonEnemyBiosGetOumuWazaDataId(void* entry);
    u8* entryList;
    u8* entry;
    u8 count;
    u8 i;
    u16 moveId;

    if (context == NULL) {
        return 0;
    }
    entryList = (u8*)PC_PTR(pokemonGetStatus(context, 0, 0x122, 0));
    for (i = 0; i < 4; i++) {
        if (output != NULL) {
            output[i] = 0;
        }
    }
    count = 0;
    for (i = 0; i < 4; i++) {
        entry = entryList + i * 0xC;
        if ((u8)fightOutPokemonEnemyCheckValid((void*)entry) != 0) {
            moveId = fightOutPokemonEnemyBiosGetOumuWazaDataId((void*)entry);
            if (moveId != 0) {
                if (moveId != MOVE_SHADOW_LOCKED) {
                    if (output != NULL) {
                        output[count] = moveId;
                    }
                    count++;
                }
            }
        }
    }
    return count;
}

/* 0x80201340 | size: 0xC0 */
typedef struct FightOutPokemonEnemyEntry {
    u8 bytes[0xC];
} FightOutPokemonEnemyEntry;

typedef struct FightOutPokemonEnemyList {
    FightOutPokemonEnemyEntry entries[4];
} FightOutPokemonEnemyList;

void fightOutPokemonSetOumuWazaDataId(void* context, void* target, u16 value) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonEnemyBiosSetOumuWazaDataId(void* entry, u16 val);
#endif
    extern void* fightOutPokemonEnemySearchAry(void* data, u32 mode, void* key);
    extern u8 fightOutPokemonEnemyCheckValid(void* entry);
    FightOutPokemonEnemyList* enemies;
    FightOutPokemonEnemyEntry* entry;
    u32 i;

    if (context == NULL) {
        return;
    }
    pokemonSetStatus(context, 0, POKEMON_STATUS_MIRROR_MOVE, 0, value);
    enemies = (FightOutPokemonEnemyList*)PC_PTR(pokemonGetStatus(context, 0, 0x122, 0));
    if (target != NULL) {
        entry = (FightOutPokemonEnemyEntry*)fightOutPokemonEnemySearchAry(enemies, 4, target);
        if (entry == NULL) {
            return;
        }
        fightOutPokemonEnemyBiosSetOumuWazaDataId(entry, value);
        return;
    }
    i = 0;
    do {
        entry = &enemies->entries[i];
        if ((u8)fightOutPokemonEnemyCheckValid(entry) != 0) {
            fightOutPokemonEnemyBiosSetOumuWazaDataId(entry, value);
        }
        i++;
    } while (i < 4);
}

/* 0x80201400 | size: 0x7C */
s32 fightOutPokemonGetTasteLike(void* arg0, u32 arg1) {
    extern s32 pokemonGetTasteLike(void*, u32);
    void* result;

    result = PC_PTR(pokemonGetStatus(arg0, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (result == NULL) {
        return -1;
    }
    if (result == NULL) {
        result = NULL;
    } else {
        result = PC_PTR(pokemonGetStatus(result, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
    }
    if (result == NULL) {
        return -1;
    }
    return pokemonGetTasteLike(result, arg1);
}

/* 0x8020147C | size: 0xF0 */
u16 fightOutPokemonDoItemSoubi(void* context, u16 moveId, u8 slot, u8 updateFlag) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonDoItemSoubi(void* pokemon, u16 moveId, u8 slot);
#endif
    void* partyList;
    void* pokemon;
    u16 result;

    partyList = PC_PTR(pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (partyList == NULL) {
        return 0;
    }
    if (partyList == NULL) {
        pokemon = NULL;
    } else {
        pokemon = PC_PTR(pokemonGetStatus(partyList, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
    }
    if (pokemon == NULL) {
        result = 0;
    } else {
        result = pokemonDoItemSoubi(pokemon, moveId, slot);
    }
    if (updateFlag == 1) {
        if (slot == 0) {
            if (result != 0) {
                pokemonSetStatus(context, 0, POKEMON_STATUS_USED_ITEM, 0, result);
            }
        }
        if (slot == 1) {
            if (moveId != 0) {
                pokemonSetStatus(context, 0, POKEMON_STATUS_USED_ITEM, 0, 0);
            }
        }
    }
    return result;
}

/* 0x8020156C | size: 0x94 */
typedef struct { u16 fields[7]; } FieldTable7;
u32 fightOutPokemonGetAllAbiCnt(void* context) {
    extern FieldTable7 lbl_80279C80;
    FieldTable7 table;
    u8 i;
    u32 sum;

    table = lbl_80279C80;
    sum = 0;
    for (i = 0; i < 7; i++) {
        sum = (sum + (u32)pokemonGetStatus(context, 0, table.fields[i], 0)) & 0xFFFF;
    }
    return sum;
}

/* 0x80201600 | size: 0xA4 */
void fightOutPokemonCopyAllAbiCnt(void* dest, void* src) {
    extern FieldTable7 lbl_80279C70;
    FieldTable7 table;
    u16 fieldId;
    u8 i;

    table = lbl_80279C70;
    for (i = 0; i < 7; i++) {
        fieldId = table.fields[i];
        pokemonSetStatus(dest, 0, fieldId, 0, (u32)pokemonGetStatus(src, 0, fieldId, 0));
    }
}

/* 0x802016A4 | size: 0x60 */
void* fightOutPokemonGetSex(void* arg0) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetSex(void*);
#endif
    void* result;

    result = PC_PTR(pokemonGetStatus(arg0, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (result == NULL) {
        result = NULL;
    } else {
        result = PC_PTR(pokemonGetStatus(result, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
    }
    if (result == NULL) {
        result = NULL;
    } else {
        result = PC_PTR(pokemonGetSex(result));
    }
    return result;
}

/* 0x80201704 | size: 0x60 */
void* fightOutPokemonIsHpMantan(void* arg0) {
    extern void* pokemonIsHpMantan(void*);
    void* result;

    result = PC_PTR(pokemonGetStatus(arg0, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    if (result == NULL) {
        result = NULL;
    } else {
        result = PC_PTR(pokemonGetStatus(result, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
    }
    if (result == NULL) {
        result = NULL;
    } else {
        result = pokemonIsHpMantan(result);
    }
    return result;
}

#endif
