/**
 * @file battle_grid_exact_801C3C98.c
 * @brief Exact battle-grid Pokemon removal and replacement helpers.
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
extern const char lbl_80275808[];

extern void GSlogWrite(const char*, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerSetVisibility(void*, u32);
#endif

void battleGridRemovePokemon(u8* pokemon)
{
    BattleGridGroupEntry* group = gBattleGrid.entries;
    u16 i;
    u16 j;

    if (pokemon != NULL) {
        for (i = 0; i < 4; i++, group++) {
            for (j = 0; j < 2; j++) {
                if (P32(group->pokemon[j]) == pokemon) {
                    P32_SET(group->pokemon[j], NULL);
                    group->memberCount--;
                    gBattleGrid.pokemonCount--;
                    wazaSequenceOwnerSetVisibility(pokemon, 0);
                    pokemon[0x76] = 0;
                    return;
                }
            }
        }
    }
}

void battleGridReplacePokemon(u8* pokemon, u8* replacement)
{
    BattleGridGroupEntry* group = gBattleGrid.entries;
    u16 i;
    u16 j;

    if (pokemon == NULL || replacement == NULL) {
        GSlogWrite(lbl_80275808);
        return;
    }

    for (i = 0; i < 4; i++, group++) {
        for (j = 0; j < 2; j++) {
            if (P32(group->pokemon[j]) == pokemon) {
                P32_SET(group->pokemon[j], replacement);
                wazaSequenceOwnerSetVisibility(pokemon, 0);
                replacement[0x76] = pokemon[0x76];
                pokemon[0x76] = 0;
                return;
            }
        }
    }
}
