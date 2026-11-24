/**
 * @file fight_pokemon_r58_801FED3C_o1.c
 * @brief fightOutPokemonSetJoutaiMigawariHp carve, 0x801FED3C - 0x801FEF74.
 *
 * Writer twin of fightOutPokemonGetJoutaiMigawariHp (the next carve): when
 * the substitute joutai (0x14) is set, store its hit points where the
 * reader looks for them.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u32 flags);
#endif

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

/* Stores `value` as the status word of the joutai `id`, where
 * fightPokemonIsJoutai looks for it. */
static inline void fightPokemonSetJoutaiStatus(void* pokemon, u16 id, u32 value) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus(u16 id);
#endif
    extern void pokemonSetJoutai08(void* obj, u16 id, u32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiSetBuff(void* obj, u16 id, u32 value);
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
            pokemonSetJoutai08(data, id, value);
        } else if (joutaiDataBiosGetStatus(id) == 0xCD) {
            joutaiSetBuff(slot, id, value);
        }
    } else if (joutaiDataBiosGetStatus(id) == 0xD8) {
        joutaiSetBuff(pokemon, id, value);
    }
}

void fightOutPokemonSetJoutaiMigawariHp(void* trainer, u32 hp) {
    if (trainer == NULL) {
        return;
    }
    if (fightPokemonIsJoutai(trainer, 0x14) == 1) {
        fightPokemonSetJoutaiStatus(trainer, 0x14, hp);
    }
}
