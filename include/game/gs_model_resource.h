/**
 * @file gs_model_resource.h
 * @brief A model's resource on the disc (one model set of an archive's scene_data) and the joint description layout.
 *
 * Both are disc layout: on the PC they are big-endian with 32-bit pointer slots (pc/disc_types.h; read pointers with
 * DP(), store with DP_SET()); on the GameCube the marks are the plain form, the layout and code are unchanged.
 * Before 2026-10-15 every gs_model_* file kept its own copy of these (with differing field names).
 *
 * GSmodelResource = a model set (docs/engine/hsd.md, "Model sets and their animations"): the root joint
 * description and three null-terminated lists (joint animations, material / texture animations, shape animations).
 * GSjobjDesc = HSD_Joint's layout (0x40 bytes); GSmodelClone builds one at run time as an instance joint.
 */
#ifndef GS_MODEL_RESOURCE_H
#define GS_MODEL_RESOURCE_H

#include "dolphin/types.h"
#include "pc/disc_types.h"

typedef struct DISC_STRUCT GSdiscVec {
    f32 x, y, z;
} GSdiscVec; /* a vector inside disc data (GSvec's layout) */

typedef struct DISC_STRUCT GSjobjDesc {
    /* 0x00 */ DISC_PTR(char) className;
    /* 0x04 */ u32 flags;
    /* 0x08 */ DISC_PTR(struct GSjobjDesc) child;
    /* 0x0C */ DISC_PTR(struct GSjobjDesc) next;
    /* 0x10 */ DISC_PTR(void) dobj;
    /* 0x14 */ GSdiscVec rotation;
    /* 0x20 */ GSdiscVec scale;
    /* 0x2C */ GSdiscVec position;
    /* 0x38 */ DISC_PTR(void) robj;
    /* 0x3C */ DISC_PTR(void) aobj;
} GSjobjDesc;

typedef DISC_PTR(void) GSanimSlot; /* one entry of a null-terminated animation list */

typedef struct DISC_STRUCT GSmodelResource {
    /* 0x00 */ DISC_PTR(GSjobjDesc) joint;
    /* 0x04 */ DISC_PTR(GSanimSlot) anims;      /* joint animations (HSD_AnimJoint trees) */
    /* 0x08 */ DISC_PTR(GSanimSlot) texAnims;   /* material / texture animations (likely) */
    /* 0x0C */ DISC_PTR(GSanimSlot) shapeAnims; /* shape animations (likely) */
} GSmodelResource;

#ifndef TARGET_PC
typedef char GSjobjDesc_check_size[sizeof(GSjobjDesc) == 0x40 ? 1 : -1];
typedef char GSmodelResource_check_size[sizeof(GSmodelResource) == 0x10 ? 1 : -1];
#endif

#endif /* GS_MODEL_RESOURCE_H */
