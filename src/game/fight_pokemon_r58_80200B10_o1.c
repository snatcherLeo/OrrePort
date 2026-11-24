/**
 * @file fight_pokemon_r58_80200B10_o1.c
 * @brief fightOutPokemonInitJoutaiKeep carve, 0x80200B10 - 0x80200E00.
 *
 * Ends the nine "keep" joutai (status-condition) ids listed in
 * lbl_80279C90, cancelling the 0xEE effect's trajectories like
 * fightOutPokemonInitJoutaiKie does for its three ids.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u16 flags);
#endif

typedef struct {
    u16 ids[9];
} JoutaiIdTable9;

/* Copies of fight_out_pokemon.c's accessor inlines. */
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

void fightOutPokemonInitJoutaiKeep(void* pokemon) {
    extern JoutaiIdTable9 lbl_80279C90;
    u8 i;
    JoutaiIdTable9 table = lbl_80279C90;

    if (pokemon == NULL) {
        return;
    }
    /* RULE-EXCEPTION(user-approved): do/while(0) wrapper around the loop, chosen for codegen (permuter farm) — see docs/RULE_EXCEPTIONS.md */
    do {
        for (i = 0; i < 9; i++) {
            if (fightOutPokemonCheckJoutaiInline(pokemon, table.ids[i]) == 1) {
                fightOutPokemonCureJoutaiInline(pokemon, table.ids[i]);
            }
        }
    } while (0);
}
