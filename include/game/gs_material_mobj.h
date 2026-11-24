/**
 * @file gs_material_mobj.h
 * @brief GSmatObj: the MObj class GSmaterial gives its models' materials, an HSD_MObj with a back pointer to the
 *        GSmaterial record that owns it (set while any material extension is enabled, NULL otherwise).
 *
 * One definition for the material units (gs_material_800DF498.c and gs_material_exact_800DFE98.c carried copies, and
 * GSmaterialEnableExtension / DisableExtension used the raw +0x20).
 */
#ifndef GAME_GS_MATERIAL_MOBJ_H
#define GAME_GS_MATERIAL_MOBJ_H

#include "hsd/hsd_mobj.h"
#include "game/gs_material_object.h"

typedef struct GSmatObj {
    /* 0x00 */ HSD_MObj mobj;
    /* 0x20 */ GSmaterial* material;
} GSmatObj; /* size 0x24 on the GameCube */

#ifndef TARGET_PC
typedef char GSmatObj_check_size[sizeof(GSmatObj) == 0x24 ? 1 : -1];
#endif

#endif /* GAME_GS_MATERIAL_MOBJ_H */
