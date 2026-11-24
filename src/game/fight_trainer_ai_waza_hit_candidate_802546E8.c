/**
 * @file fight_trainer_ai_waza_hit_candidate_802546E8.c
 * @brief Candidate fightTrainerAiWazaHit.cpp range, 0x802546E8 - 0x80254810.
 */
#include "game/fight_trainer_ai_waza_hit_shared.h"

/* True when any Pokemon the opposing trainer has out knows a move that the
 * attacker also knows. */
s32 fightTrainerAiWazaHit192(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetFightPokemonWazaAry(void* ctx, void* pokemon, u16* waza, s32 a, s32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetFightTrainerFightOutPokemonPtrAry(s32 side, void* ctx, void** out,
                                                              s32 a, s32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void* ctx, u32 pokemon, s32 kind);
#endif
#ifdef TARGET_PC
    /* PC: the out-Pokemon list is filled as 4-byte cells (fight_floor.c, u32* arr); 8-byte cells read two Pokemon
     * glued into one pointer (port-gaps 36, P16) */
    u32 out[10];
#else
    void* out[10];
#endif
    u16 ownWaza[10];
    u16 otherWaza[10];
    u16 ownCount;
    u16 outCount;
    u16 otherCount;
    u16 i;
    u16 k;
    u16 j;

    ownCount = fightTrainerAiGetFightPokemonWazaAry(ctx, PC_PTR32_IMPLICIT((void*)PC_PTR(param1)), ownWaza, 0, 0);
    outCount = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), out, 0, 1);
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x27)) == 1) {
        return 0;
    }
    for (i = 0; i < outCount; i++) {
#ifdef TARGET_PC
        if (out[i] != 0) {
#else
        if (out[i] != NULL) {
#endif
            otherCount = fightTrainerAiGetFightPokemonWazaAry(ctx, PC_PTR32_IMPLICIT(out[i]), otherWaza, 0, 0);
            for (j = 0; j < otherCount; j++) {
                for (k = 0; k < ownCount; k++) {
                    if (otherWaza[j] == ownWaza[k]) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
