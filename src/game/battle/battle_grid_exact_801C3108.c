/**
 * @file battle_grid_exact_801C3108.c
 * @brief Exact battle-grid group-table accessor.
 */
#include "dolphin/types.h"

extern u8 gBattleGrid[];

void* battleGridGetPtr(void)
{
    return gBattleGrid;
}
