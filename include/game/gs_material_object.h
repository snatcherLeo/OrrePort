/**
 * @file gs_material_object.h
 * @brief GSmaterial: the game's per-material record (pool entries of 0x40 bytes on the GameCube) wrapping an HSD_MObj.
 *
 * Layout recovered in gs_material_800DF498.c (all fields typed there); moved here so every material unit shares it
 * (2026-10-15). Several units had partial views of it ("GSmaterialObject" in gs_material_exact_*.c, a two-field
 * "GSmaterial" in gs_model_struct.h) and GSmaterialSetTexture used raw offsets.
 * Note: gs_material.h's GSmaterialEntry is an older documentation-only guess of a 0x170-byte layout; its fields from
 * 0x18 on disagree with this record (for example 0x38 is the saved image description here, inside its "specular").
 */
#ifndef GAME_GS_MATERIAL_OBJECT_H
#define GAME_GS_MATERIAL_OBJECT_H

#include "dolphin/types.h"
#include "hsd/hsd_forward.h"

struct GStextureHandle;

/* savedImageDesc / savedPEDesc before anything was saved */
#define GSMATERIAL_UNSAVED ((void*)0xFEFEFEFE)

typedef struct GSmaterial {
    /* 0x00 */ u8 inUse;
    /* 0x01 */ u8 alpha;
    /* 0x02 */ u16 extensions;            /* enabled material extensions (bit mask) */
    /* 0x04 */ u32 savedFlags;
    /* 0x08 */ HSD_MObj* mobj;
    /* 0x0C */ u8 modulate[4];            /* RGBA */
    /* 0x10 */ u32 channels[4];
    /* 0x20 */ HSD_TObj* envTObj;         /* environment map */
    /* 0x24 */ HSD_ImageDesc* envImageDesc;
    /* 0x28 */ struct GStextureHandle* envTexture;
    /* 0x2C */ s32 envMode;
    /* 0x30 */ s32 envType;
    /* 0x34 */ f32 envBlend;
    /* 0x38 */ HSD_ImageDesc* savedImageDesc; /* the first texture's original image description, or GSMATERIAL_UNSAVED */
    /* 0x3C */ HSD_PEDesc* savedPEDesc;
} GSmaterial; /* size 0x40 on the GameCube */

#ifndef TARGET_PC
typedef char GSmaterial_check_size[sizeof(GSmaterial) == 0x40 ? 1 : -1];
#endif

#endif /* GAME_GS_MATERIAL_OBJECT_H */
