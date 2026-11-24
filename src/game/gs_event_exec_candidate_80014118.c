/** Exact standalone owner for 0x80014118 - 0x80014198. */
#include "dolphin/types.h"

extern s32 fightTrainer_GetFightPokemonPtr(u32, u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroGetStatus(u32, s32, u16);
#endif
extern u32 lbl_8047A2E0;
extern u32 lbl_8047A2F4;
extern u32 lbl_8047A2F8;

void itemUseGetTargetPokemon(s32 arg, s32* out1, s32* out2) {
    s32 val;
    s32 other;

    if ((s32)lbl_8047A2E0 == 1) {
        other = fightTrainer_GetFightPokemonPtr(lbl_8047A2F4, (u16)arg);
        val = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(PC_PTR(other)));
    } else {
        val = heroGetStatus(PC_PTR(lbl_8047A2F8), 3, (u16)arg);
        other = 0;
    }
    *out1 = val;
    *out2 = other;
}
