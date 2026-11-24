/**
 * @file fight_range_80218BD4.c
 * @brief Fight-sequence functions, 0x80218BD4 - 0x80218D24.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */

extern u8* lbl_8047B610;
extern u32 ServerStatusFlag;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetPtrAsNowFightType(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s8 fn_802025B8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int fightCheckAccuracyRules();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetUseWazaDataId();
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
static inline u32 fightGetDefenderContext(void) {
    return PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
}

void wsCmdAB(void) {
    u32 sub1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(
        2, PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0))));
    u32 ctx2 = fightGetDefenderContext();
    u32 sub2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, ctx2));

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx2), PC_PTR(0x15)) == 1 && sub1 != sub2 &&
        (ServerStatusFlag & 0x1000000) == 0) {
        ServerStatusFlag |= 0x40;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset

#pragma optimize_for_size on
void WS_MICHIDURE(void) {
    u32 context;
    u32 move;
    u32 target;
    u8 result;

    context = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = fightOutPokemonGetUseWazaDataId((void*)PC_PTR(context));
    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    result = fightCheckAccuracyRules(move, context, target);
    if (result == 1) {
        lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    } else {
        result = fn_802025B8(PC_PTR(context), PC_PTR(0x15));
        if (result == 2) {
            fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(context), PC_PTR(0x15), 0);
        }
        lbl_8047B610 = lbl_8047B610 + 5;
    }
}
#pragma optimize_for_size reset
