#ifndef GAME_STATUS_VALUE_H
#define GAME_STATUS_VALUE_H
#include "dolphin/types.h"

/* The 32-bit value returned by the game's generic field getters (fightTrainerGetStatus and the other *GetStatus
 * functions): depending on the field id it is a number or a pointer. Our name (the original type is unknown).
 * GameCube: u32, so the code is unchanged. PC (OrrePort): uintptr_t, so pointer fields keep their full width and
 * number fields still mask and compare like integers. */
#ifdef TARGET_PC
#include <stdint.h>
typedef uintptr_t StatusValue;
#else
typedef u32 StatusValue;
#endif

#endif
