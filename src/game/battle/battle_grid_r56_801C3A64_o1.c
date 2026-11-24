/**
 * @file battle_grid_r56_801C3A64_o1.c
 * @brief Strict battle-grid distance island at 0x801C3A64.
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

static inline s32 battleGridGetMaxPokemonField(void)
{
    extern BattleGridGroupTable gBattleGrid;
    extern s32 wazaSequenceOwnerGetSizeClass(void*);
    u16 i;
    u16 j;
    BattleGridGroupEntry* group = gBattleGrid.entries;
    u8** pokemon;
    s32 maxField = -2;

    if (gBattleGrid.count == 0) {
        return 0;
    }
    for (i = 0; i < 4; i++, group++) {
        if (P32(group->slot) != NULL) {
#ifdef TARGET_PC /* the grid's Pokemon list is 32-bit slots (PTR32_FIELD): read each through P32 */
            (void)pokemon;
            for (j = 0; j < 2; j++) {
                u8* member = P32(group->pokemon[j]);
                if (member != NULL) {
                    s32 field = wazaSequenceOwnerGetSizeClass(member);
                    if (field > maxField) {
                        maxField = field;
                    }
                }
            }
#else
            pokemon = group->pokemon;
            for (j = 0; j < 2; j++, pokemon++) {
                if (*pokemon != NULL) {
                    s32 field = wazaSequenceOwnerGetSizeClass(*pokemon);
                    if (field > maxField) {
                        maxField = field;
                    }
                }
            }
#endif
        }
    }
    return maxField;
}

f32 battleGridGetDistance(u8 side)
{
    extern const f32 lbl_8047DF64;
    extern const f32 lbl_8047DF68;
    extern const f32 lbl_8047DF6C;
    extern const f32 lbl_8047DF70;
    extern const f32 lbl_8047DF74;
    extern const f32 lbl_8047DF78;
    extern const f32 lbl_8047DF7C;
    s32 maxField;
    f32 distance;

    maxField = battleGridGetMaxPokemonField();

    switch (maxField) {
    case -2:
    case -1:
        distance = lbl_8047DF6C;
        break;
    case 1:
        distance = lbl_8047DF64;
        break;
    case 2:
        distance = lbl_8047DF70;
        break;
    case 3:
        distance = lbl_8047DF74;
        break;
    default:
        distance = lbl_8047DF68;
        break;
    }

    if (side != 0) {
        return lbl_8047DF7C * distance;
    }
    return lbl_8047DF78 * distance;
}
