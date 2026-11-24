/**
 * @file tikeiBios.c
 * @brief Decompiled functions.
 *
 * Address range: 0x801363E8 - 0x801364A8
 *
 * Split out of the former game/effect/effect_util.c CodeCandidate
 * bucket (0x8013151C - 0x80137114); see effect_util_types.h for
 * shared cross-TU declarations.
 */

#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

#if 0
asm void tikeiDataBiosGetWazaId(void) {
#include "src/game/effect/effect_util_fn_801363E8.inc"
}
#else
u32 tikeiDataBiosGetWazaId(u16 index) {
    EffectTraceEntry* entry;
    if ((u32)index > tikei_data_number) {
        entry = NULL;
    } else {
        entry = &tikei_data[index];
    }
    if (entry == NULL) {
        return 0;
    }
    return entry->value1;
}
#endif

#if 0
asm void tikeiDataBiosGetZokuseiDataId(void) {
#include "src/game/effect/effect_util_fn_80136428.inc"
}
#else
u32 tikeiDataBiosGetZokuseiDataId(u16 index) {
    EffectTraceEntry* entry;
    if ((u32)index > tikei_data_number) {
        entry = NULL;
    } else {
        entry = &tikei_data[index];
    }
    if (entry == NULL) {
        return 0;
    }
    return entry->kind;
}
#endif

#if 0
asm void tikeiDataBiosGetFightKoukaId(void) {
#include "src/game/effect/effect_util_fn_80136468.inc"
}
#else
u32 tikeiDataBiosGetFightKoukaId(u16 index) {
    EffectTraceEntry* entry;
    if ((u32)index > tikei_data_number) {
        entry = NULL;
    } else {
        entry = &tikei_data[index];
    }
    if (entry == NULL) {
        return 0;
    }
    return entry->value0;
}
#endif
