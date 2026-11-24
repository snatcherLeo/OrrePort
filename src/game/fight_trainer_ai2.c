/**
 * @file fight_trainer_ai2.c
 * @brief Legacy exact-source candidate for the Horobinouta callback.
 *
 * This function remains CodeCandidate: its natural typed form is not exact,
 * so it is intentionally excluded from linked progress.
 */

#include "game/colosseum.h"

typedef struct HorobinoutaContext {
    u32 trainer;
    u32 count;
} HorobinoutaContext;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonCheckFightOut(
    void* pokemon, u32 slot, HorobinoutaContext* context);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerIsAllyFightTargetPtr(u32 trainer, void* pokemon, u32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiIsPokemonJoutai(u32 trainer, void* pokemon, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_80237F74(u32 trainer, void* pokemon, u32 flag);
#endif

s32 _fightTrainerAiCheckHorobinoutaSub(
    void* pokemon, u32 slot, HorobinoutaContext* context)
{
    u32 r31;
    u32 r30;
    u32 r28;
    u32 r29;

    r29 = PC_PTR32(context);
    r28 = slot;
    r31 = PC_PTR32(pokemon);
    r30 = *(u32*)PC_PTR(r29);
#ifdef TARGET_PC /* the callee reads only r3 (its first call takes nothing); the extra arguments shape register use */
    if (!fightOutPokemonCheckFightOut((void*)PC_PTR(r31))) {
#else
    if (!fightOutPokemonCheckFightOut(
            (void*)PC_PTR(r31), r28, (HorobinoutaContext*)PC_PTR(r29))) {
#endif
        return 1;
    }
    if (fightTrainerIsAllyFightTargetPtr(PC_PTR(r30), (void*)PC_PTR(r31), r28) == 1) {
        return 1;
    }
    if (fightTrainerAiIsPokemonJoutai(PC_PTR(r30), (void*)PC_PTR(r31), PC_PTR(0x1E)) != 1 &&
        fn_80237F74(PC_PTR(r30), PC_PTR32_IMPLICIT((void*)PC_PTR(r31)), 0x2B) != 1) {
        *(u32*)PC_PTR((r29 + 4)) = *(u32*)PC_PTR((r29 + 4)) + 1;
    }
    return 1;
}
