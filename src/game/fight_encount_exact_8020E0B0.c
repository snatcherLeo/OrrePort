/**
 * @file fight_encount_exact_8020E0B0.c
 * @brief Strict encounter field and table accessors, 0x8020E0B0-0x8020E124.
 */

#include "game/colosseum.h"
#include "pc/disc_types.h" /* DISC_STRUCT: common_rel tables are big-endian on the PC */

typedef struct DISC_STRUCT FightEncountTrainerSlot { /* common_rel record: big-endian view on the PC */
    u16 fightTrainerDataId;
    u16 pad_02;
    u32 gsInputDevice;
} FightEncountTrainerSlot;

typedef struct DISC_STRUCT FightEncountData { /* common_rel record: big-endian view on the PC */
    u8 fightKind;
    u8 trainer;
    u8 zenmetuFlag;
    u8 pad_03;
    u16 fightFloorDataId;
    u16 pad_06;
    u32 fightName;
    u32 bgmSndId;
    u32 wipeId;
    u32 syoukaiWzxDataId;
    FightEncountTrainerSlot trainerSlots[4];
} FightEncountData;

u32 fightEncountDataBiosGetFightFloorDataId(FightEncountData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->fightFloorDataId;
}

u8 fightEncountDataBiosGetTrainer(FightEncountData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->trainer;
}

u8 fightEncountDataBiosGetFightKind(FightEncountData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->fightKind;
}

FightEncountData* fightEncountDataBiosGetPtr(u16 index)
{
    extern u32* fight_encount_data_number;
    extern FightEncountData* fight_encount_data;

    if (index >= *fight_encount_data_number) {
        return NULL;
    }
    return &fight_encount_data[index];
}
