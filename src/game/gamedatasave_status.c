#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

/**
 * @file gamedatasave_status.c
 * @brief Save-status accessors.
 *
 * Address range: 0x80135030 - 0x8013528C
 */

typedef struct GameDataSave GameDataSave;
typedef struct GameDataSaveStatus GameDataSaveStatus;

void gamedatasaveSetStatus(GameDataSave* ptr, u16 kind, u32 value)
{
    GameDataSaveStatus* sub;

    if (kind == 0 || kind >= 0xB) {
        return;
    }

    if (ptr == NULL) {
        ptr = (GameDataSave*)PC_PTR(savedataGetStatus(0, 0));
        if (ptr == NULL) {
            return;
        }
        ptr = PC_PTR((GameDataSave*)PC_PTR(savedataGetStatus(PC_PTR32(ptr), 1)));
        if (ptr == NULL) {
            return;
        }
    }

    sub = gamedatasaveBiosGetPtr(ptr);
    if (sub == NULL) {
        return;
    }

    switch (kind) {
    case 1:
        gamedatasaveBiosSetPtr(ptr, (GameDataSave*)PC_PTR(value));
        break;
    case 2:
        gamedatasaveBiosSetSavernd(sub, value);
        break;
    case 3:
        break;
    case 4:
        gamedatasaveBiosSetSavecount(sub, value);
        break;
    case 5:
        gamedatasaveBiosSetFloorid(sub, value);
        break;
    case 6:
        gamedatasaveBiosSetPlaytime(sub, (f32)(s32)value);
        break;
    case 7:
        gamedatasaveBiosSetPrevfloorid(sub, value);
        break;
    case 8:
        gamedatasaveBiosSetFloorposindex(sub, (u8)value);
        break;
    case 9:
        gamedatasaveBiosSetOptionNoVibration(sub, (u8)value);
        break;
    case 10:
        gamedatasaveBiosSetOptionAudio(sub, (u8)value);
        break;
    default:
        break;
    }
}

u32 gamedatasaveGetStatus(GameDataSave* ptr, u16 kind)
{
    GameDataSave* base = NULL;
    GameDataSaveStatus* sub;

    if (kind == 0 || kind >= 0xB) {
        return 0;
    }
    if (ptr == NULL) {
        base = (GameDataSave*)PC_PTR(savedataGetStatus(0, 0));
        if (base == NULL) {
            return 0;
        }
        ptr = PC_PTR((GameDataSave*)PC_PTR(savedataGetStatus(PC_PTR32(base), 1)));
        if (ptr == NULL) {
            return 0;
        }
    }
    sub = gamedatasaveBiosGetPtr(ptr);
    if (sub == NULL) {
        return 0;
    }
    switch (kind) {
    case 1:
        return PC_PTR32(base);
    case 2:
        return gamedatasaveBiosGetSavernd(sub);
    case 3:
        break;
    case 4:
        return gamedatasaveBiosGetSavecount(sub);
    case 5:
        return gamedatasaveBiosGetFloorid(sub);
    case 6:
        return (s32)gamedatasaveBiosGetPlaytime(sub);
    case 7:
        return gamedatasaveBiosGetPrevfloorid(sub);
    case 8:
        return gamedatasaveBiosGetFloorposindex(sub);
    case 9:
        return gamedatasaveBiosGetOptionNoVibration(sub);
    case 10:
        return gamedatasaveBiosGetOptionAudio(sub);
    default:
        break;
    }
    return 0;
}
