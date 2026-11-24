/**
 * @file gs_model_shadow.h
 * @brief The shadow pass's slot table (lbl_80401490, 6 slots): one slot per model a shadow falls on.
 *
 * One definition for every shadow unit (gs_model_shadow*.c; the flag / object view in
 * gs_model_shadow_exact_800E8EFC.c used GameCube offsets, wrong on the PC where the pointers are 8 bytes).
 * Roles verified from modelShadowRender (gs_model_shadow_candidate_800E8684.c); the field names are older and kept:
 * `model` is the model the shadow falls on (an entry of a caster's shadow-surface list), `receivers` are the models
 * whose shadows fall on it (they are rendered into the slot's shadow texture).
 */
#ifndef GAME_GS_MODEL_SHADOW_H
#define GAME_GS_MODEL_SHADOW_H

#include "dolphin/types.h"

struct GSmodel;
struct GSlight;

typedef struct GSshadowSlot {
    /* 0x00 */ struct GSmodel* model;         /* the model the shadow falls on */
    /* 0x04 */ struct GSlight* light;
    /* 0x08 */ struct GSmodel* receivers[16]; /* the models casting onto it */
    /* 0x48 */ u32 minSize;
    /* 0x4C */ u32 maxSize;
    /* 0x50 */ u8 flag;                       /* rendered this frame */
    /* 0x51 */ u8 _pad2[3];
    /* 0x54 */ void* obj;                     /* HSD_Shadow */
} GSshadowSlot; /* 0x58 on the GameCube */

#ifndef TARGET_PC
typedef char GSshadowSlot_check_size[sizeof(GSshadowSlot) == 0x58 ? 1 : -1];
#endif

extern GSshadowSlot lbl_80401490[6];

#endif /* GAME_GS_MODEL_SHADOW_H */
