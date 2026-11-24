/**
 * @file field_range_801ECFE0.c
 * @brief field/hero (nursery), 0x801ECFE0 - 0x801ED218.
 *
 * The rest of the TU (0x801ED218 onwards) is carved into the
 * field_exact_/field_candidate_ units that follow.
 */
#include "dolphin/types.h"

typedef struct PokemonCopy {
    u32 words[0x4e];
} PokemonCopy;

s32 daycareDeposit(void* arg0, void* arg1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(u8* arg0, u16 arg1);
#endif
    extern u8 pokemonBiosGetDarkFlag(u8* pokemon);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosGetDp(u8* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit(u8* pokemon);
#endif
    u8* source;
    void* status;
    void* resolved;

    status = arg0;
    if (arg0 == NULL) {
        status = PC_PTR(savedataGetStatus(NULL, 0xb));
    }
    if (arg1 == NULL) {
        return 0;
    }

    resolved = status;
    if (status == NULL) {
        resolved = PC_PTR(savedataGetStatus(NULL, 0xb));
    }
    source = (u8*)resolved + 8;
    if (source == NULL) {
        return 0;
    }

    *(PokemonCopy*)arg1 = *(PokemonCopy*)source;
    if (pokemonBiosGetDarkFlag(source)) {
        pokemonBiosGetDp(source);
    }

    if (status == NULL) {
        status = PC_PTR(savedataGetStatus(NULL, 0xb));
    }
    if (status != NULL) {
        *(u8*)status = 0;
    }

    pokemonInit(source);
    return 1;
}

s32 daycareWithdraw(u8* arg0, u8* pokemon)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(u8* data, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonBiosGetLevel(u8* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(u8* pokemon, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai(u8* pokemon, u32 value);
#endif
    extern u8 pokemonBiosGetDarkFlag(u8* pokemon);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetDp(u8* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit(u8* pokemon);
#endif
    u8* status;
    u8* stored;
    u8* dest;
    u8 level;

    status = arg0;
    if (arg0 == NULL) {
        status = PC_PTR(savedataGetStatus(NULL, 0xB));
    }
    if (pokemon == NULL) {
        return 0;
    }

    level = pokemonBiosGetLevel(pokemon);
    stored = status;
    if (stored == NULL) {
        stored = PC_PTR(savedataGetStatus(NULL, 0xB));
    }
    if (stored != NULL) {
        stored[1] = level;
    }

    stored = status;
    if (stored == NULL) {
        stored = PC_PTR(savedataGetStatus(NULL, 0xB));
    }
    dest = stored + 8;
    if (dest == NULL) {
        return 0;
    }

    if (pokemonIsJoutaiDataId(pokemon, 0x3E) != 0) {
        pokemonInitPersistentJoutai(pokemon, 0x3E);
    }
    *(PokemonCopy*)dest = *(PokemonCopy*)pokemon;
    if (pokemonBiosGetDarkFlag(pokemon) != 0) {
        *(u32*)(status + 4) = pokemonBiosGetDp(pokemon);
    } else {
        *(u32*)(status + 4) = 0;
    }

    if (status == NULL) {
        status = PC_PTR(savedataGetStatus(NULL, 0xB));
    }
    if (status != NULL) {
        status[0] = 1;
    }
    pokemonInit(pokemon);
    return 1;
}
