/**
 * @file fight_range_exact_802134D4.c
 * @brief Strict turn-check callback island at 0x802134D4.
 */
#include "dolphin/types.h"
#include "game/constants/conditions.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonCheckFightOut(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWriteJoutaiDataId(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonIsPokemonJoutaiDataId(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonInitJoutaiKeep(void*);
#endif

s32 _fightSeqTurnCheckSubFightOutPokemon__FPvUsPv(
    void* ctx, u16 param2, void* param3)
{
    if (!fightOutPokemonCheckFightOut(ctx)) {
        return 1;
    }
    fightOutPokemonWriteJoutaiDataId(ctx, PC_PTR(0x11));
    if (fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(CONDITION_SLEEP)) == 1 && fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x22)) == 1) {
        fightOutPokemonInitJoutaiKeep(ctx);
    }
    return 1;
}
