/**
 * @file fight_out_pokemon_suffix_8020A8E0.c
 * @brief fightOutPokemon + fightPokemon final suffix, address range
 *        0x8020A8E0-0x8020AE30, 1 function.
 *
 * OutPokemon/Pokemon field accessors, sequence/status writers, and
 * damage-calc support the seq/waza layers call into (statusGetStatus,
 * fadeEffectGetRandom callers, etc). Corresponds to XD's
 * fight.cpp fightOutPokemon+fightPokemon cluster (0x80200644-0x80208288).
 */

#include "game/colosseum.h"
#include "game/trainer.h"
#include "game/pokemon.h"

typedef struct ColosseumEventRow6 {
    u8 mode;
    u8 field_01;
    u16 eventIndex;
    u16 nextIndex;
} ColosseumEventRow6;

typedef struct ColosseumEventSubRow {
    u8 valueMode;
    u8 scaleMode;
    s16 scaleNumerator;
    s16 scaleDenominator;
    u16 minValue;
    u16 maxValue;
} ColosseumEventSubRow;

typedef struct ColosseumEventPairRow {
    u8 resultFuncId;
    u8 field_01;
    u16 firstLinkIndex;
    ColosseumEventSubRow slots[2];
} ColosseumEventPairRow;

typedef struct StatusIdTable7 {
    u16 id[7];
} StatusIdTable7;

/* =========================================================================
 * External declarations
 * ========================================================================= */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   pokemonSetStatus();
#endif
extern void  pokemonGrowBasisStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   itemGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  joutaiDataBiosGetStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  pokemonIsJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  joutaiIsJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  pokemonGetSoubiItemDataId(void);
#endif
extern void* fightActionGetPri(void* p);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif

/* SDA table pointers for event data arrays */
extern u32 lbl_80478D38;   /* Event table count */
extern ColosseumEventRow6 lbl_80478D30[]; /* Event table base (6 bytes per entry) */
extern u32 lbl_80478D28; /* Pair-row table count */
extern ColosseumEventPairRow lbl_80375A08[]; /* 0x18-byte pair rows */
/* Address: 0x8020A8E0 | Size: 0x550
 *
 * Evaluates a jouken (condition) row and its linked rows. Each row's test is
 * the same inline body (retail keeps one value array per inlined copy). */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int _fadeEffectGetRandom__FUl();
#endif
extern u32 fightTargetDataBiosGetStatusKid();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTargetDataBiosGetPtr(u16 targetId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTargetGetPtr(u16 targetId, void* target, u16 floorStatus);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 statusGetStatus(u32 statusId, void* ptr, u16 numerator,
                           u16 parameter, u16 denominator);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif
extern u16 fightJoukenLinkDataBiosGetNextLinkDataId(u16 idx);
extern u32 fightJoukenLinkDataBiosGetJoukenDataId(u16 idx);
extern u8 fightJoukenLinkDataBiosGetJoint();
extern s16 fightJoukenDataBiosGetValueWaru();
extern s16 fightJoukenDataBiosGetValueKake();
extern u8 fightJoukenDataBiosGetValueHoseiFlag();
extern u16 fightJoukenDataBiosGetValueBuff();
extern u16 fightJoukenDataBiosGetValueTarget();
extern u8 fightJoukenDataBiosGetValueKind();
extern u16 fightJoukenDataBiosGetLink();
extern u8 fightJoukenDataBiosGetForm();

/* RULE-EXCEPTION(user-approved): single-use inline helper - the per-slot value read is its own inline with an int slot parameter, which gives the two inlined fightJoukenCheck copies retail's conversion-web order - see docs/RULE_EXCEPTIONS.md */
static inline s32 fightJoukenGetValue(u32 id, int slot, void* target)
{
    s32 value;
    u16 targetId;
    u8 scaleMode;
    u8 valueMode;
    u32 statusId;
    u16 parameter;
    s16 denominator;
    s16 numerator;
    void* targetPtr;

    value = 0;
    valueMode = fightJoukenDataBiosGetValueKind(id, slot);
    targetId = fightJoukenDataBiosGetValueTarget(id, slot);
    parameter = fightJoukenDataBiosGetValueBuff(id, slot);
    numerator = fightJoukenDataBiosGetValueKake(id, slot);
    denominator = fightJoukenDataBiosGetValueWaru(id, slot);
    scaleMode = fightJoukenDataBiosGetValueHoseiFlag(id, slot);
    switch (valueMode) {
    case 0:
        break;
    case 1:
        value = targetId;
        break;
    case 2:
        value = targetId + _fadeEffectGetRandom__FUl(parameter - targetId);
        break;
    case 3:
        targetPtr = PC_PTR(fightTargetGetPtr(
            targetId, PC_PTR32_IMPLICIT(target), (u16)fightFloorGetStatus(0, 0, 0x14, 0)));
        if (targetPtr == NULL) {
            return 0;
        }
#ifdef TARGET_PC /* the GameCube passes GetPtr's result on in r3 (zeroargs.py class) */
        statusId = ((u32 (*)(void*))fightTargetDataBiosGetStatusKid)(fightTargetDataBiosGetPtr(targetId));
#else
        fightTargetDataBiosGetPtr(targetId);
        statusId = fightTargetDataBiosGetStatusKid();
#endif
        if (scaleMode == 0) {
            value = statusGetStatus(statusId, PC_PTR32_IMPLICIT(targetPtr), (u16)numerator,
                                    parameter, (u16)denominator);
        } else {
            value = statusGetStatus(statusId, PC_PTR32_IMPLICIT(targetPtr), 0, parameter, 0);
        }
        break;
    }
    if (scaleMode == 1) {
        value *= numerator;
        if (denominator != 0) {
            value /= denominator;
        }
    }
    return value;
}

static inline u8 fightJoukenCheck(u32 id, void* target)
{
    s32 values[2];
    u8 result;
    u8 slot;

    for (slot = 0; slot < 2; slot++) {
        values[slot] = fightJoukenGetValue(id, slot, target);
    }

    result = 0;
    switch (fightJoukenDataBiosGetForm(id)) {
    case 0:
        result = 1;
        break;
    case 1:
        if (values[0] == values[1]) {
            result = 1;
        }
        break;
    case 2:
        if (values[0] != values[1]) {
            result = 1;
        }
        break;
    case 3:
        if (values[0] >= values[1]) {
            result = 1;
        }
        break;
    case 4:
        if (values[0] <= values[1]) {
            result = 1;
        }
        break;
    case 5:
        if (values[0] < values[1]) {
            result = 1;
        }
        break;
    case 6:
        if (values[0] > values[1]) {
            result = 1;
        }
        break;
    }
    return result;
}

u8 fn_8020A8E0(u32 conditionId, void* target)
{
    u8 result;
    u8 rowResult;
    u32 row;
    u16 next;

    result = fightJoukenCheck(conditionId, target);
    next = fightJoukenDataBiosGetLink(conditionId);
    if (next == 0) {
        return result;
    }
    while (1) {
        row = fightJoukenLinkDataBiosGetJoukenDataId(next);
        rowResult = fightJoukenCheck(row, target);
        switch (fightJoukenLinkDataBiosGetJoint(row)) {
        case 1:
            if (result || rowResult) {
                result = 1;
            } else {
                result = 0;
            }
            break;
        case 2:
            if (result && rowResult) {
                result = 1;
            } else {
                result = 0;
            }
            break;
        }
        next = fightJoukenLinkDataBiosGetNextLinkDataId(next);
        if (next == 0) {
            return result;
        }
    }
}
