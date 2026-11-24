/**
 * @file gs_part.h
 * @brief GSpart: one model part (a joint subtree of a GSmodel), one pool entry each (gs_part*.c).
 *
 * One definition for the GSpart units and the GSmodel animation code (each carried a copy; the animation code's was a
 * padded view of the joint field).
 */
#ifndef GAME_GS_PART_H
#define GAME_GS_PART_H

#include "dolphin/types.h"

struct HSD_JObj;

typedef struct GSpart {
    /* 0x00 */ u8 inUse;
    /* 0x01 */ u8 _pad;
    /* 0x02 */ u16 index;
    /* 0x04 */ void* model; /* the GSmodel the part belongs to */
    /* 0x08 */ struct HSD_JObj* jobj; /* root joint of the part */
} GSpart; /* size 0xC on the GameCube */

#ifndef TARGET_PC
typedef char GSpart_check_size[sizeof(GSpart) == 0xC ? 1 : -1];
#endif

#endif /* GAME_GS_PART_H */
