/**
 * @file fight_range_exact_80212840.c
 * @brief Strict fight-sequence status-initializer island at 0x80212840.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonIsPokemonJoutaiDataId(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWriteJoutaiDataId(void*, u32);
#endif

void fightSeqSpecificationActionCounterInit(void* ctx)
{
    if (fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x2e)) == 1) {
        fightOutPokemonWriteJoutaiDataId(ctx, PC_PTR(0x2e));
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x15)) == 1) {
        fightOutPokemonWriteJoutaiDataId(ctx, PC_PTR(0x15));
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x28)) == 1) {
        fightOutPokemonWriteJoutaiDataId(ctx, PC_PTR(0x28));
    }
}
