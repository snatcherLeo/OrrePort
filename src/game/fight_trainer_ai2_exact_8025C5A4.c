/**
 * @file fight_trainer_ai2_exact_8025C5A4.c
 * @brief Strict exact prefix of fightTrainerAi2.cpp (0x8025C5A4-0x8025C6BC).
 */

#include "game/colosseum.h"
#include "game/constants/conditions.h"
#include "game/constants/move_effects.h"
#include "game/constants/moves.h"

typedef u32 (*FightFloorCallback)(void*, u32, void*);

typedef struct HorobinoutaContext {
    void* trainer;
    u32 count;
} HorobinoutaContext;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 wazaGetStatus(void*, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorLoopValidFightOutPokemon(u32, FightFloorCallback, void*, u32);
#endif
extern u32 _fightTrainerAiCheckHorobinoutaSub(void*, u32, void*);

s32 fightTrainerAiCheckJoutaiKieWazaHitWazaDataId(
    void* trainer,
    u32 conditionId,
    u32 moveId,
    u32 unused)
{
    u32 value = wazaGetStatus(0, moveId, 9, 0) & 0xFFFF;

    if ((u16)conditionId == CONDITION_IN_THE_AIR &&
        (value == MOVE_EFFECT_TWISTER || value == MOVE_EFFECT_GUST || value == MOVE_EFFECT_THUNDER ||
         value == MOVE_EFFECT_SKY_UPPERCUT)) {
        return 1;
    }
    if ((u16)conditionId == CONDITION_UNDERGROUND && value == MOVE_EFFECT_EARTHQUAKE) {
        return 1;
    }
    if ((u16)conditionId == CONDITION_UNDERWATER &&
        ((u16)moveId == MOVE_SURF || (u16)moveId == MOVE_WHIRLPOOL)) {
        return 1;
    }
    if (value == MOVE_EFFECT_LOCK_ON) {
        return 1;
    }
    return 0;
}

u32 fightTrainerAiCheckHorobinouta(void* trainer)
{
    HorobinoutaContext context;

    context.trainer = trainer;
    context.count = 0;
    fightFloorLoopValidFightOutPokemon(
        0, _fightTrainerAiCheckHorobinoutaSub, &context, 0);
    return context.count & 0xFFFF;
}
