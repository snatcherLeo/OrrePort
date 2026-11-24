/**
 * @file fight_range_8023A740.c
 * @brief Strict target-order island, 0x8023A740 - 0x8023B498 (1 fn).
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightSideGetHikaeFightPokemonNum(u32 side, u32 trainerLimit, u32 param_3, u32 param_4);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightTrainerAiGetValueAryMaxBanme(s32*, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightTrainerAiAddValue(s32, s32);
#endif

#define AI_SCORE(code)                                                        \
    do {                                                                      \
        score[i] = fn_80239984(score[i], trainer, (code));                    \
        fn_80239EE8(0xEC64, trainerPtr,                                       \
                    fightOutPokemonGetPokemonPtr(pokemon), 0, 0, move, 0,    \
                    (code));                                                  \
    } while (0)

#define AI_SCORE_VALUE(code, value)                                           \
    do {                                                                      \
        u8 scoreValue = (u8)(value);                                          \
        score[i] = fightTrainerAiAddValueScaled(score[i], scoreValue, trainer, (code));        \
        fightTrainerAiAddValueLog(0xEC64, trainer,                                          \
                    fightOutPokemonGetPokemonPtr(pokemon), 0, 0, move, 0,    \
                    (code), scoreValue);                                      \
    } while (0)

s32 fightTrainerAiScoreWazaToTarget(u32 trainer, u32 pokemon, u32 moveCount,
                 register u16* moveList,
                 s16* volatile moveData, u32 target, u32 unused) {
    u16 i;
    u32 move;
    u16 k;
    u8 sharedValue;
    u16 new_var;
    s32 data;
    u32 j;
    s32 maxDamageFirst;
    s32 maxMetric;
    u32 trainerPtr;
    typedef s32 (*AiMoveFunc)(u32, u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerAiGetValueAryMaxBanme();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus(u32, u16, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
    extern u32 fightTrainerAiGetPartDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_802376EC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonPtrAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
    extern s32 fightTrainerAiWazaDamageNull();
    extern s32 fightTrainerAiWazaValueNull();
    extern s32 fightTrainerAiWazaValueToriaezutukae();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerAiGetWazaTenkouValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetAisyou();
#endif
    extern u8 fightTrainerAiIsWazaTypeId();
    extern u32 fightAiGetWazaRangeId();
    extern u8 fightTrainerAiGetWazaAvg(u32, u32);
    extern u8 fightAiGetWazaTypeId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightAiGetWazaIryoku();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetWazaHitValue(s32, u32, u32, u32);
#endif
    extern u8 fightTrainerAiGetHpRatio();
    extern u8 fightTrainerAiGetWazaAvgAdjusted();
    extern u8 fightTrainerAiGetWazaPpRatio(u32, u32, u8);
    extern u8 fightTrainerAiGetWazaRiskFlag();
    extern u8 dbgMenuFightGetFightPokeStMenuOpenFlag();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetValidFightOutPokemonCount();
#endif
    extern s32 fightTrainerAiAddValueScaled();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80239984();
#endif
    extern void fightTrainerAiAddValueLog();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTrainerAiLogText();
#endif

    s32 score[10];
    s32 damage[10];
    s32 difference[10];
    s32 moveMetric[10];
    u32 scratch[8];
    u16 trainerValue16;
    u32 side;
    u16 reserve16;
    u16 reserve17;
    u16 reserve18;
    u32 trainerData;
    u32 trainerValue;
    u16 baseDamage;
    struct {
        s32 value;
    } maxDamage;
    u16 damageIndex;
    s32 maxDifference;
    u16 initIndex;
    u32 moveType;
    u32 moveKind;
    struct {
        AiMoveFunc value;
    } damageFunc;
    AiMoveFunc valueFunc;
    AiMoveFunc checkFunc;
    s32 damageValue;
    s32 scoreValueTemp;
    s32 delta;
    s32 selected;
    s32 randomRange;
    u8 randomBase;

    (void)unused;
    side = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(3, trainer));
    reserve16 = (u16)fightFloorGetStatus(0, 0, 0x16, 0);
    reserve17 = (u16)fightFloorGetStatus(0, 0, 0x17, 0);
    reserve18 = (u16)fightFloorGetStatus(0, 0, 0x18, 0);
    new_var = (u16)fightTrainerGetStatus(
        0, (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0), 2, 0);
    trainerData = new_var;
    randomBase = (u8)fightTrainerGetStatus(0, trainerData, 0x38, 0);
    trainerPtr = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, pokemon));
    trainerValue = fightTrainerAiGetPartDataId(trainer, pokemon);
    fightSideGetHikaeFightPokemonNum(side, reserve16, reserve17, reserve18);

    for (initIndex = 0; initIndex < 10; initIndex++) {
        score[initIndex] = 0;
        damage[initIndex] = 0;
        difference[initIndex] = 0;
        moveMetric[initIndex] = 0;
    }

    baseDamage = fn_802376EC(trainer, target);
    maxDamageFirst = -0xFFFF;
    maxDifference = -0xFFFF;
    maxMetric = 0;

    for (damageIndex = 0; damageIndex < (u16)moveCount; damageIndex++) {
        i = moveList[damageIndex];
        if (i == 0 || i == 0x165) {
            continue;
        }

        if ((u8)fightTrainerGetStatus(
                0,
                (u16)fightTrainerGetStatus(
                    0,
                    (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0),
                    2, 0),
                0x2D, 0) != 1) {
            goto damage_zero;
        }
        damageFunc.value = (AiMoveFunc)PC_PTR(wazaGetStatus(0, i, 0x1E, 0));
        if (damageFunc.value != 0) {
            goto damage_call;
        }
        damageFunc.value = fightTrainerAiWazaDamageNull;
        goto damage_call;
damage_zero:
        damageValue = 0;
        goto damage_done;
damage_call:
        fightFloorGetFightTrainerFightOutPokemonPtrAry(
            0, trainer, scratch, 0, 1);
        wazaGetStatus(0, i, 5, 0);
        damageValue = damageFunc.value(trainer, pokemon, i, target);
damage_done:
        damage[damageIndex] = damageValue;

        difference[damageIndex] =
            (s32)baseDamage - damage[damageIndex];
        if (fightTrainerAiIsWazaTypeId(trainer, i, 1) == 1) {
            if (maxDamageFirst < damage[damageIndex]) {
                maxDamageFirst = damage[damageIndex];
            }
            if (difference[damageIndex] <= 0) {
                if (maxDifference < difference[damageIndex]) {
                    maxDifference = difference[damageIndex];
                }
                moveMetric[damageIndex] = fightTrainerAiGetWazaAvg(trainer, i);
                if (maxMetric < moveMetric[damageIndex]) {
                    maxMetric = moveMetric[damageIndex];
                }
            }
        }
    }

    maxDamage.value = maxDamageFirst;
    randomRange = randomBase * 2 + 1;
    trainerValue16 = (u8)trainerValue;

    for (i = 0; i < (u16)moveCount; i++) {
        move = moveList[i];
        if (move == 0) {
            continue;
        }
        data = moveData[i];
        if (data < 0) {
            continue;
        }

        moveType = fightTrainerAiGetWazaTenkouValue(trainer, move, pokemon);
        moveKind = fightAiGetWazaRangeId(trainer, move);
        checkFunc = (AiMoveFunc)PC_PTR(wazaGetStatus(0, move, 0x1C, 0));
        if ((u8)fightTrainerGetStatus(
                0,
                (u16)fightTrainerGetStatus(
                    0,
                    (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0),
                    2, 0),
                0x32, 0) != 1) {
            goto value_zero;
        }
        valueFunc = (AiMoveFunc)PC_PTR(wazaGetStatus(0, move, 0x1C, 0));
        if (valueFunc != 0) {
            goto value_call;
        }
        valueFunc = fightTrainerAiWazaValueNull;
        goto value_call;
value_zero:
        scoreValueTemp = 0;
        goto value_done;
value_call:
        scoreValueTemp = valueFunc(trainer, pokemon, move, target);
value_done:
        score[i] = scoreValueTemp;

        fightTrainerAiLogText(0xEC64, 0xEC2C, 0xEC32,
                    trainerPtr, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(pokemon))), 0, 0,
                    move, 0, 0x227, score[i]);

        if (fightTrainerAiGetWazaHitValue(PC_PTR(trainer), pokemon, move, target) == 1) {
            if (checkFunc == fightTrainerAiWazaValueToriaezutukae) {
                AI_SCORE(0x3E);
            } else {
                AI_SCORE(0x3C);
            }
        } else {
            AI_SCORE(0x3D);
        }

        if ((u16)moveType != 9 && fightTrainerAiIsWazaTypeId(trainer, move, 1) == 1) {
            if ((u16)fightTrainerAiGetAisyou(trainer, target, moveType,
                                  fightAiGetWazaIryoku(PC_PTR(trainer), move)) == 0x42) {
                AI_SCORE(0x3F);
            }
            if ((u16)fightTrainerAiGetAisyou(trainer, target, moveType,
                                  fightAiGetWazaIryoku(PC_PTR(trainer), move)) == 0x43) {
                AI_SCORE(0x40);
            }
        }

        for (j = 0; (u16)j < 3; j++) {
            u8 effect = fightAiGetWazaTypeId(trainer, move, j & 0xFF);
            if (effect != 0) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
                extern u32 fightTrainerGetStatus(u32, u16, u32, u32);
#endif
                sharedValue = (u8)fightTrainerGetStatus(
                    0, trainerValue16, 0xC, effect);
                AI_SCORE_VALUE(0x41, sharedValue);
            }
        }

        if ((u16)moveType != 9) {
            for (j = 0; (u16)j < 2; j++) {
                u16 type = (u16)fightTrainerGetStatus(
                    0, trainerData, 0x39, j);
                if (type != 9 && (u16)moveType == type) {
                    sharedValue = (u8)fightTrainerGetStatus(
                        0, trainerData, 0x3A, j);
                    AI_SCORE_VALUE(0x42, sharedValue);
                }
            }
        }

        for (k = 0; k < 3; k++) {
            u8 effect = fightAiGetWazaTypeId(trainer, move, k & 0xFF);
            if (effect != 0) {
                for (j = 0; (u16)j < 2; j++) {
                    u8 type = (u8)fightTrainerGetStatus(
                        0, trainerData, 0x3B, j);
                    if (type != 0 && effect == type) {
                        sharedValue = (u8)fightTrainerGetStatus(
                            0, trainerData, 0x3C, j);
                        AI_SCORE_VALUE(0x43, sharedValue);
                    }
                }
            }
        }

        if (fightTrainerAiIsWazaTypeId(trainer, move, 1) == 1) {
            if (maxDamage.value <= damage[i]) {
                AI_SCORE(0x44);
            }
            delta = difference[i];
            if (delta < 0) {
                AI_SCORE(0x45);
            }
            if (delta <= 0) {
                AI_SCORE(0x46);
            }
            if (delta <= 0 && maxMetric <= moveMetric[i]) {
                AI_SCORE(0x47);
            }
        }

        if (fightTrainerAiIsWazaTypeId(trainer, move, 2) == 1 &&
            fightTrainerAiGetHpRatio(trainer, pokemon) == 1) {
            AI_SCORE(0x48);
        }
        if (fightTrainerAiGetWazaAvgAdjusted(trainer, move) == 1) {
            AI_SCORE(0x49);
        }
        if ((u8)moveKind == 4 &&
            fightFloorGetValidFightOutPokemonCount(0, 1, target, 1) >= 2) {
            AI_SCORE(0x4A);
        }
        if (fightTrainerAiGetWazaPpRatio(trainer, pokemon, (u8)data) == 1) {
            AI_SCORE(0x4B);
        }
        if (fightTrainerAiGetWazaRiskFlag(trainer, move) == 1) {
            AI_SCORE(0x4C);
        }

        if (dbgMenuFightGetFightPokeStMenuOpenFlag() == 1) {
            delta = (u16)GSrand() % randomRange - randomBase;
            score[i] = fightTrainerAiAddValue(score[i], delta);
            fightTrainerAiLogText(0xEC64, 0xEC2C, 0xEC32,
                        trainerPtr, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(pokemon))), 0,
                        0, move, 0, 0x225, delta);
        }

        fightTrainerAiLogText(0xEC64, 0xEC2C, 0xEC32,
                    trainerPtr, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(pokemon))), 0, 0,
                    move, 0, 0x226, score[i]);
    }

#ifdef TARGET_PC
    {
        /* ORRE_BATTLE_LOG (observe only, 2026-10-30, P20's battle 345): each move's final AI score, whether the
         * random spread was added (dbgMenuFightGetFightPokeStMenuOpenFlag) and its half-width, before the pick */
        static int enabled = -1;
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        if (enabled < 0) {
            extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
            enabled = pc_battle_log_on();
        }
        if (enabled) {
            OSReport("[battle] ai scores: mon %08X target %08X moves %u [%d %d %d %d] dmg [%d %d %d %d] spread %u base %u\n",
                     (unsigned)pokemon, (unsigned)target, (unsigned)(u16)moveCount, (int)score[0], (int)score[1],
                     (int)score[2], (int)score[3], (int)damage[0], (int)damage[1], (int)damage[2], (int)damage[3],
                     (unsigned)dbgMenuFightGetFightPokeStMenuOpenFlag(), (unsigned)randomBase);
        }
    }
#endif
    selected = fightTrainerAiGetValueAryMaxBanme(score, moveCount, 1);
    if (selected < 0) {
        return -1;
    }
    fightTrainerAiLogText(0xEC64, 0xEC2C, 0xEC32,
                trainerPtr, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(pokemon))), 0, 0,
                moveList[selected], 0, 0x228, score[selected]);
    return selected;
}

#undef AI_SCORE_VALUE
#undef AI_SCORE
