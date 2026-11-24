/**
 * @file fight_trainer_ai_waza_hit.c
 * @brief Candidate fightTrainerAiWazaHit.cpp range, 0x8025C264 - 0x8025C5A4.
 */
#include "game/fight_trainer_ai_waza_hit_shared.h"

/*
 * Whether the AI should let `self` use move `waza` on `target`: 1 to allow,
 * 0 to forbid, -1 when the target would benefit from the move. `kind` is
 * 0xFFFF / 0xFFFE for the two ally-side checks.
 */
/* RULE-EXCEPTION(user-approved): single-function inline helper — see docs/RULE_EXCEPTIONS.md */
inline int aiSideIdMatches(u16 sideA, u16 sideB) {
    return sideA == sideB;
}

s32 _fightTrainerAiWazaHitCheck(void* ai, void* self, u16 waza, void* target, u32 kind) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTargetGetTragetPtrToRelativeHostSideFightTargetId(void*, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTrainerAiGetNowTenkou(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus(u32, u16, u32, u32);
#endif
    extern u16 fightFloorGetFightOutPokemonPtrAry(u32, u32, u32, u32, void**);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSortFightOutPokemonPtrArySub(u32, void**, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetJoutaiParam(void*, u32);
#endif
    extern u8 fightTrainerAiIsJoutaiKie(void*, void*);
    extern u8 fightTrainerAiCheckGuard(void*, void*, u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightCheckAccuracyRules(u16, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightIsWazaThunderInRain(u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80229BD8(u16);
#endif

#ifdef TARGET_PC
    /* PC: the out-Pokemon list is 4-byte cells (fightFloorGetFightOutPokemonPtrAry, the sort); 2026-10-17 */
    u32 list[8];
#define AI_LIST_AT(n) ((void*)PC_PTR(list[(u16)(n)]))
#else
    void* list[8];
#define AI_LIST_AT(n) (list[(u16)(n)])
#endif
    u16 sideId;
    u16 wazaType;
    u16 count;
    u32 i;
    u32 selfIndex;
    u32 targetIndex;
    u32 behind;

    sideId = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(PC_PTR32_IMPLICIT(self), fightFloorGetStatus(0, 0, 0x14, 0));
    fightTrainerAiGetNowTenkou(ai, 0, 1);
    wazaType = wazaGetStatus(0, waza, 9, 0);
    if (target == NULL) {
        return 1;
    }

    count = fightFloorGetFightOutPokemonPtrAry(0, 1, 0, 0, list);
    fightFloorSortFightOutPokemonPtrArySub(0, list, 8, 0);
    selfIndex = 0;
    targetIndex = 0;
    for (i = 0; (u16)i < count; i++) {
        if (AI_LIST_AT(i) != NULL) {
            if (self == AI_LIST_AT(i)) {
                selfIndex = i;
            }
            if (target == AI_LIST_AT(i)) {
                targetIndex = i;
            }
        }
    }
    /* RULE-EXCEPTION(user-approved): truncating store kept for register allocation — see docs/RULE_EXCEPTIONS.md */
    selfIndex = (u16)selfIndex;
    behind = (u32)(selfIndex - (u16)targetIndex) >> 31;

    if (kind == 0xFFFF || kind == 0xFFFE) {
        if (kind == 0xFFFF && fightTrainerAiIsPokemonJoutai(ai, target, PC_PTR(0x1D)) == 1 &&
            sideId == fightOutPokemonGetJoutaiParam(target, PC_PTR(0x1D))) {
            return -1;
        }
        if (fightTrainerAiIsJoutaiKie(ai, self) == 1 && behind == 1) {
            return 0;
        }
        if (fightTrainerAiCheckGuard(ai, target, waza) == 1) {
            return 0;
        }
        return 1;
    }

    if (fightCheckAccuracyRules(waza, PC_PTR32_IMPLICIT(self), PC_PTR32_IMPLICIT(target)) == 1) {
        return 0;
    }
    if (fightTrainerAiCheckGuard(ai, target, waza) == 1) {
        return 0;
    }
    if (fightTrainerAiIsPokemonJoutai(ai, target, PC_PTR(0x1D)) == 1 && aiSideIdMatches(fightOutPokemonGetJoutaiParam(target, PC_PTR(0x1D)), sideId)) {
        return -1;
    }
    if (behind == 1) {
        if (fightTrainerAiIsPokemonJoutai(ai, target, PC_PTR(0x1F)) == 1 && wazaType != 0x92 && wazaType != 0x95 &&
            wazaType != 0x98 && wazaType != 0xCF) {
            return 0;
        }
        if (fightTrainerAiIsPokemonJoutai(ai, target, PC_PTR(0x20)) == 1 && wazaType != 0x93) {
            return 0;
        }
        if (fightTrainerAiIsPokemonJoutai(ai, target, PC_PTR(0x21)) == 1 && waza != 0x39 && waza != 0xFA) {
            return 0;
        }
    }
    if (fightIsWazaThunderInRain(waza) == 1) {
        return -1;
    }
    return fn_80229BD8(waza) == 1 ? -1 : 1;
}
