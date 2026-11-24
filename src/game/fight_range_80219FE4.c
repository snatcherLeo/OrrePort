/**
 * @file fight_range_80219FE4.c
 * @brief Fight-sequence move filter, 0x80219FE4 - 0x8021A054.
 */
#include "dolphin/types.h"
#include "game/constants/moves.h"

extern u16 YubihuriNGTbl[];

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
u32 fightSeqCheckYubiwohuruWazaDataId(u16 move_id) {
    u16 entry;
    s32 index;

    if (move_id == MOVE_NONE) {
        return 0;
    }
    if (move_id == MOVE_SHADOW_LOCKED) {
        return 0;
    }
    if (move_id == MOVE_UNUSED_355) {
        return 0;
    }
    index = 0;
    while (1) {
        entry = YubihuriNGTbl[index];
        if (entry == 0xffff) {
            break;
        }
        if (move_id == entry) {
            break;
        }
        index = index + 1;
    }
    if (entry != 0xffff) {
        return 0;
    }
    return 1;
}
#pragma optimize_for_size reset
