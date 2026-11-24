/**
 * @file msgctrl_exact_80131BA0.c
 * @brief Strict message-control getters, 0x80131BA0 - 0x80131BF8.
 */
#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

u32 msgctrlSeOn(void)
{
    return 0;
}

u32 msgctrlSeOff(void)
{
    return 0;
}

u32 msgctrlPasoName(void)
{
    return _PASO_NAME;
}

u32 msgctrlItemName(void)
{
    return _ITEM_NAME;
}

u32 msgctrlWazaName(void)
{
    return _WAZA_NAME;
}

u32 msgctrlTrainerClientno(void)
{
    return _TRAINER_CLIENTNO;
}

u32 msgctrlTrainerEnename2(void)
{
    return _TRAINER_ENENAME2;
}

u32 msgctrlTrainerEnename(void)
{
    return _TRAINER_ENENAME;
}

u32 msgctrlTrainerLose(void)
{
    return _TRAINER_LOSE;
}

u32 msgctrlTrainerName(void)
{
    return _TRAINER_NAME;
}

u32 msgctrlTrainerType(void)
{
    return _TRAINER_TYPE;
}
