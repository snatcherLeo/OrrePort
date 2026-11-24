/**
 * @file fight_pokemon_r58_801FEF74_middle.c
 * @brief fightOutPokemonGetJoutaiMigawariHp carve, 0x801FEF74 - 0x801FF1BC,
 *        linked.
 *
 * Standalone copy of the shared candidate body, built at -O4,s with
 * -schedule off (configure.py) in place of the shared file's
 * push/scheduling-off/pop pragmas.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonGetJoutai08(void* obj, s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiGetBuff(void* obj, s32 arg);
#endif

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
        obj = PC_PTR(pokemonGetStatus(trainer, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(0x14) == 0x7C || joutaiDataBiosGetStatus(0x14) == 0xC8) {
            if (obj == NULL) {
                target = NULL;
            } else {
                target = PC_PTR(pokemonGetStatus(obj, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
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
            obj2 = PC_PTR(pokemonGetStatus(trainer, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
            if (joutaiDataBiosGetStatus(0x14) == 0x7C || joutaiDataBiosGetStatus(0x14) == 0xC8) {
                if (obj2 == NULL) {
                    target2 = NULL;
                } else {
                    target2 = PC_PTR(pokemonGetStatus(obj2, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
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
