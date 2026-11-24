/**
 * @file fight_encount_exact_8020DD44.c
 * @brief Strict encounter audio, wipe, and basic field helpers,
 *        0x8020DD44-0x8020DF10.
 */

#include "game/colosseum.h"
#include "pc/disc_types.h" /* DISC_STRUCT: common_rel tables are big-endian on the PC */

typedef struct FightEncountWipeData {
    u8 snapshotUse;
    u8 pad_01;
    u16 wipeEffectSndId;
    f32 wipeEffectTime;
#ifdef TARGET_PC
    void* wipeFunction; /* PC: a native function pointer (data_80375938.c defines the table as these records) */
#else
    u32 wipeFunction;
#endif
} FightEncountWipeData;

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

extern FightEncountWipeData fight_encount_wipe_data[];

u32 fightEncountGetEnvSndDataId(u16 encountDataIndex)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightFloorGetStatus(u8*, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightEncountDataBiosGetFightFloorDataId(FightEncountData* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern FightEncountData* fightEncountDataBiosGetPtr(u16 index);
#endif
    FightEncountData* encountData;
    u32 fightFloorDataId;

    encountData = fightEncountDataBiosGetPtr(encountDataIndex);
    fightFloorDataId = fightEncountDataBiosGetFightFloorDataId(encountData);
    return fightFloorGetStatus(NULL, fightFloorDataId, 0x7, 0);
}

u32 fightEncountGetBgmSndDataId(u16 encountDataIndex)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightFloorGetStatus(u8*, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus(u32, u16, u32, u16);
#endif
    extern u32 fightTrainerKindDataBiosGetBgmSndId(u8* trainerKindData);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* fightTrainerKindDataBiosGetPtr(u16);
#endif
    extern u32 fightEncountDataBiosGetBgmSndId(FightEncountData* ptr);
    extern u16 fightEncountDataBiosGetFightTrainerDataId(FightEncountData* base, u8 slot);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightEncountDataBiosGetFightFloorDataId(FightEncountData* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern FightEncountData* fightEncountDataBiosGetPtr(u16 index);
#endif
    FightEncountData* encountData;
    u32 bgmSndId;
    u32 fightFloorDataId;
    u16 fightTrainerDataId;
    u16 trainerStatus;
    u16 i;

    encountData = fightEncountDataBiosGetPtr(encountDataIndex);
    bgmSndId = fightEncountDataBiosGetBgmSndId(encountData);
    if (bgmSndId != 0) {
        return bgmSndId;
    }
    fightFloorDataId = fightEncountDataBiosGetFightFloorDataId(fightEncountDataBiosGetPtr(encountDataIndex));
    bgmSndId = fightFloorGetStatus(0, fightFloorDataId, 6, 0);
    if (bgmSndId != 0) {
        return bgmSndId;
    }
    for (i = 0; i < 4; i++) {
        fightTrainerDataId = fightEncountDataBiosGetFightTrainerDataId(encountData, (u8)i);
        if (fightTrainerDataId != 0) {
            trainerStatus = fightTrainerGetStatus(0, fightTrainerDataId, 4, 0);
            if (trainerStatus != 0) {
                bgmSndId = fightTrainerKindDataBiosGetBgmSndId(
                    fightTrainerKindDataBiosGetPtr(trainerStatus));
                if (bgmSndId != 0) {
                    return bgmSndId;
                }
            }
        }
    }
    return 1;
}

u16 fightEncountDataBiosGetWipeEffectSndID(FightEncountWipeData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->wipeEffectSndId;
}

f32 fightEncountDataBiosGetWipeEffectTime(FightEncountWipeData* ptr)
{
    extern f32 lbl_8047E530;

    if (ptr == NULL) {
        return lbl_8047E530;
    }
    return ptr->wipeEffectTime;
}

u8 fightEncountDataBiosGetWipeSnapshotUse(FightEncountWipeData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->snapshotUse;
}

u32 fightEncountDataBiosGetWipeFunction(FightEncountWipeData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
#ifdef TARGET_PC
    return (u32)(uintptr_t)ptr->wipeFunction; /* below 4 GB: the executable is linked low */
#else
    return ptr->wipeFunction;
#endif
}

FightEncountWipeData* fightEncountWipeDataBiosGetPtr(u32 index)
{
    extern u32 fight_encount_wipe_data_number;

    if (index >= fight_encount_wipe_data_number) {
        return NULL;
    }
    return &fight_encount_wipe_data[index];
}

u32 fightEncountDataBiosGetFightName(FightEncountData* ptr)
{
    if (ptr == NULL) {
        return 0;
    }
    return ptr->fightName;
}

void fightEncountDataBiosSetSyoukaiWzxDataId(FightEncountData* ptr, u32 val)
{
    if (ptr == NULL) {
        return;
    }
    ptr->syoukaiWzxDataId = val;
}

void fightEncountDataBiosSetBgmSndId(FightEncountData* ptr, u32 val)
{
    if (ptr == NULL) {
        return;
    }
    ptr->bgmSndId = val;
}
