/**
 * @file battle_grid_exact_801C3F10.c
 * @brief Exact battle-grid trainer replacement and insertion helpers.
 */
#include "dolphin/types.h"

typedef struct BattleGridGroupEntry {
    PTR32_FIELD(u8) slot; /* 32-bit slots on the PC too: the group array is also read at raw +0x40 */
    PTR32_FIELD(u8) pokemon[2];
    u16 memberCount;
    u8 arg1;
    u8 arg2;
} BattleGridGroupEntry;

typedef struct BattleGridGroupTable {
    BattleGridGroupEntry entries[4];
    u16 count;
    u16 pokemonCount;
} BattleGridGroupTable;

extern BattleGridGroupTable gBattleGrid;
extern const char lbl_80275830[];

extern void GSlogWrite(const char*, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerSetVisibility(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void*, int, u32);
#endif

void battleGridReplaceTrainer(u8* trainer, u8* replacement)
{
    BattleGridGroupEntry* group = gBattleGrid.entries;
    u16 i;

    if (trainer == NULL || replacement == NULL) {
        GSlogWrite(lbl_80275830);
        return;
    }

    for (i = 0; i < 4; i++, group++) {
        if (P32(group->slot) == trainer) {
            P32_SET(group->slot, replacement);
            wazaSequenceOwnerSetVisibility(trainer, 0);
            replacement[0x76] = trainer[0x76];
            trainer[0x76] = 0;
            return;
        }
    }
}

void battleGridAddTrainer(u8* slot, u8 arg1, u8 arg2)
{
    BattleGridGroupEntry* group;
    s8 state;

    if (gBattleGrid.count < 4) {
        group = &gBattleGrid.entries[0];
        if (P32(group->slot) != NULL) {
            group = &gBattleGrid.entries[1];
            if (P32(group->slot) != NULL) {
                group++;
                if (P32(group->slot) != NULL) {
                    group++;
                    if (P32(group->slot) != NULL) {
                        group++;
                    }
                }
            }
        }
        memset(group, 0, sizeof(*group));
        P32_SET(group->slot, slot);
        state = 1;
        group->arg1 = arg1;
        group->arg2 = arg2;
        if (arg1 != 0) {
            state = -1;
        }
        slot[0x76] = state;
        gBattleGrid.count = gBattleGrid.count + 1;
    }
}
