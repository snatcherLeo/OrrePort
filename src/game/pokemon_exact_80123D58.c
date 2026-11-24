/**
 * @file pokemon_exact_80123D58.c
 * @brief pokemonWazaCreate (0x80123D58 - 0x80123E70): clears a move slot,
 *        sets its move id and fills its PP with the move's max PP.
 *
 * One-function carve of the Pokemon TU (pokemon_range_8011F5FC.c), GC/1.3
 * -O4,p, text only. Retail expands pokemonWazaInit (0x80125314) and
 * pokemonWazaGetMaxPP (0x80123E70) here; the carve carries single-use
 * inline copies of them, as pokemonSetWazaStatus's candidate does.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus(u8* ptr, u32 a, u32 b, u16 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus(u8* obj, u32 param, u16 selector, u32 index, u16 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaGetMaxPP(u16 type_id, u8 val);
#endif

/* RULE-EXCEPTION(title-path): single-use inline copy of pokemonWazaInit (0x80125314) - see docs/RULE_EXCEPTIONS.md */
static inline void pokemonWazaInitInline(u8* ptr, u32 slot)
{
    if (ptr == NULL) {
        return;
    }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE, slot, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP, slot, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP_UPS, slot, 0);
}

/* RULE-EXCEPTION(title-path): single-use inline copy of pokemonWazaGetMaxPP (0x80123E70) - see docs/RULE_EXCEPTIONS.md */
static inline u8 pokemonWazaGetMaxPPInline(u8* ptr, u16 waza)
{
    u32 slot;
    u16 id;

    if (ptr == NULL) {
        return 0;
    }
    slot = waza + 4;
    id = pokemonGetStatus(ptr, 0, POKEMON_STATUS_MOVE, slot);
    return wazaGetMaxPP(id, pokemonGetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP_UPS, slot));
}

/* 0x80123D58 | 0x118 */
void pokemonWazaCreate(u8* ptr, u32 slot, u32 waza)
{
    if (ptr == NULL) {
        return;
    }
    pokemonWazaInitInline(ptr, slot);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE, slot, waza);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP, slot, pokemonWazaGetMaxPPInline(ptr, slot));
}
