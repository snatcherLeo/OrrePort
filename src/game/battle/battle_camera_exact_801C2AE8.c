/** Exact battle-grid visibility accessors, 0x801C2AE8 - 0x801C2BE0. */
#include "dolphin/types.h"

typedef struct BattleGridGroupEntry {
    PTR32_FIELD(u8) slot; /* 32-bit slots on the PC too: the group array is also read at raw +0x40 */
    PTR32_FIELD(u8) pokemon[2];
    u16 memberCount;
    u8 arg1;
    u8 arg2;
} BattleGridGroupEntry;

extern BattleGridGroupEntry gBattleGrid[];

u16 battleGridGetNumPokemonsForTrainer(u32 id)
{
    BattleGridGroupEntry* group;
    u16 i;

    group = gBattleGrid;
    for (i = 0; i < 4; i++, group++) {
        if (PC_PTR32(P32(group->slot)) == id) {
            return group->memberCount;
        }
    }
    return 0;
}

void battleGridResetModelVisibilityFlags(void)
{
    extern u8 lbl_8047B39A;
#ifdef TARGET_PC /* GCC takes no section on a block-scope declaration */
    extern u8 lbl_8047B39C[12];
#else
    extern u8 lbl_8047B39C[12] __attribute__((section(".sdata")));
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerSetVisibility(void*, u32);
#endif
    BattleGridGroupEntry* group;
    u16 i;
    u16 j;
    u16 visibilityIndex;

    group = gBattleGrid;
    visibilityIndex = 0;
    if (lbl_8047B39A != 0) {
        for (i = 0; i < 4; i++, group++) {
            wazaSequenceOwnerSetVisibility(P32(group->slot), lbl_8047B39C[visibilityIndex++]);
            for (j = 0; j < 2; j++) {
                wazaSequenceOwnerSetVisibility(P32(group->pokemon[j]),
                            lbl_8047B39C[visibilityIndex++]);
            }
        }
        lbl_8047B39A = 0;
    }
}
