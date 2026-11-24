#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

typedef struct {
    u8 data[0x154];
} CopyBlock_0x154;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, s32 arg1, s32 status, s32 arg3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonSetStatus(void* context, s32 arg1, s32 status, s32 arg3, u32 value);
#endif

u32 fightOutPokemonUseHensinBuff(void* context)
{
    CopyBlock_0x154* src;
    CopyBlock_0x154* dest;

    if (context == NULL) {
        return 0;
    }
    src = (CopyBlock_0x154*) PC_PTR(pokemonGetStatus(context, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0));
    dest = (CopyBlock_0x154*) PC_PTR(pokemonGetStatus(context, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0));
    if (src == NULL) {
        return 0;
    }
    if (dest == NULL) {
        return 0;
    }
    *dest = *src;
    pokemonSetStatus(context, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0, PC_PTR32(dest));
    return 1;
}

u8 fightOutPokemonIsUseHensinBuff(void* trainer)
{
    void* a;
    void* b;

    if (trainer == NULL) {
        return 0;
    }
    a = PC_PTR(pokemonGetStatus(trainer, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    b = PC_PTR(pokemonGetStatus(trainer, 0, POKEMON_STATUS_TRANSFORM_COPY_PTR, 0));
    return (u8) (a == b);
}
