/**
 * @file fight_kouka_exact_8020D968.c
 * @brief Strict fight effect-data helpers, 0x8020D968-0x8020DA14.
 */

#include "game/colosseum.h"

typedef struct FightKoukaData {
    u16 fightJoukenDataId;
    u16 fightTargetDataId;
    u16 koukaDataId;
} FightKoukaData;

typedef struct FightCopyBlock {
    u32 data[12];
} FightCopyBlock;

extern FightKoukaData fight_kouka_data[];

void fightActionBiosCopy(FightCopyBlock* dst, FightCopyBlock* src)
{
    if (dst == 0) {
        return;
    }
    if (src == 0) {
        return;
    }
    *dst = *src;
}

u16 fightKoukaDataBiosGetKoukaDataId(FightKoukaData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->koukaDataId;
}

u16 fightKoukaDataBiosGetFightTargetDataId(FightKoukaData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->fightTargetDataId;
}

u16 fightKoukaDataBiosGetFightJoukenDataId(FightKoukaData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->fightJoukenDataId;
}

FightKoukaData* fightKoukaDataBiosGetPtr(u16 index)
{
    extern u32 fight_kouka_data_number;

    if (index >= fight_kouka_data_number) {
        return NULL;
    }
    return &fight_kouka_data[index];
}
