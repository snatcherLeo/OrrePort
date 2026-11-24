/**
 * @file gs_model_struct.h
 * @brief GSmodel: the game's model object (one pool entry per placed model) and its helper types.
 *
 * One definition for every GSmodel unit (2026-10-15): gs_model_main / _exact / _suffix, gs_model_anim*, gs_model_bound*
 * and gs_model_shadow* each carried their own copy (ten in all, some padded with GameCube byte counts); the names here
 * merge what each of them knew.
 */
#ifndef GAME_GS_MODEL_STRUCT_H
#define GAME_GS_MODEL_STRUCT_H

#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT / DISC_PTR for the disc-layout types included below */

typedef struct GSvec {
    f32 x;
    f32 y;
    f32 z;
} GSvec;

typedef struct GSquat {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
} GSquat;

typedef struct GScolor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} GScolor;

#include "game/gs_material_object.h" /* GSmaterial (was a two-field partial view here) */

/* A model's bounding box (GSmodel +0x4C; the code at 0x80190E60-0x80191484, hsd_object.c, fills and tests it). The
 * three pointers are set by fn_8019147C / fn_80191474 / fn_8019146C (position / rotation / scale, from the GS callers'
 * arguments); on the PC they are 8 bytes each, so this record (and everything after it in GSmodel) is larger there. */
typedef struct GSbound {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 empty;          /* 1 until the first point is added (fn_80191460 / fn_80191358) */
    /* 0x02 */ u8 _pad02[2];
    /* 0x04 */ GSvec* position;
    /* 0x08 */ GSvec* rotation;
    /* 0x0C */ GSvec* scale;
    /* 0x10 */ GSvec centerStart;  /* box minimum */
    /* 0x1C */ GSvec centerEnd;    /* box maximum */
    /* 0x28 */ GSvec size;         /* maximum - minimum */
} GSbound; /* 0x34 on the GameCube */
#include "game/gs_bound_offsets.h" /* the raw offsets some callers use; kept equal to the fields here */
#if defined(TARGET_PC) && !defined(__cplusplus)
#include <stddef.h>
_Static_assert(offsetof(GSbound, centerStart) == GS_BOUND_OFF_MIN, "GS_BOUND_OFF_MIN must match GSbound");
_Static_assert(offsetof(GSbound, centerEnd) == GS_BOUND_OFF_MAX, "GS_BOUND_OFF_MAX must match GSbound");
_Static_assert(offsetof(GSbound, size) == GS_BOUND_OFF_SIZE, "GS_BOUND_OFF_SIZE must match GSbound");
#endif

#ifdef TARGET_PC
#define GSMODEL_BOUND_OFFSET ((u32)__builtin_offsetof(struct GSmodel, bound)) /* GSmodel +0x4C on the GameCube */
#else
#define GSMODEL_BOUND_OFFSET 0x4cU
#endif

typedef struct GSmodelMaterialList {
    GSmaterial* materials[1];
} GSmodelMaterialList;

#include "game/gs_model_resource.h" /* GSjobjDesc, GSmodelResource: disc layout */

#include "game/gs_part.h" /* GSpart */

/* GS's view of HSD_JObj (hsd/hsd_jobj.h) with GS's field names. It used to pad its way to the fields with GameCube
 * byte counts; now every member has HSD_JObj's type in HSD_JObj's order, so the offsets match HSD_JObj on the PC too
 * (8-byte pointers). Checked against hsd_jobj.h's layout below (GameCube sizes). */
typedef struct HSDJObj {
    /* 0x000 */ struct {
        void* classInfo;            /* HSD_Class */
        u16 refCount;
        u16 refCountIndividual;
    } object;                       /* HSD_Obj */
    /* 0x008 */ struct HSDJObj* next;
    /* 0x00C */ struct HSDJObj* parent;
    /* 0x010 */ struct HSDJObj* child;
    /* 0x014 */ u32 flags;
    /* 0x018 */ void* data;
    /* 0x01C */ GSvec rotation;
    /* 0x028 */ f32 rotationW;
    /* 0x02C */ GSvec scale;
    /* 0x038 */ GSvec translate;
    /* 0x044 */ f32 matrix[3][4];
} HSDJObj;
#ifndef TARGET_PC
typedef char HSDJObj_check_matrix[(u32)&((HSDJObj*)0)->matrix == 0x44 ? 1 : -1]; /* HSD_JObj::mtx */
#endif

typedef struct GSmodel GSmodel;
typedef void (*GSmodelAnimEndedCallback)(GSmodel* model, void* arg);

/* One part-animation mix entry (GSmodel +0xE4, four of them). */
typedef struct GSmodelPartAnimMix {
    s32 type;
    s32 partIndex;
    f32* value;
} GSmodelPartAnimMix;

/* Run-time flags. MWCC allocates bit-fields from the high bit, GCC on x86 from the low bit, so on the PC the fields
 * are listed in reverse: `raw` stays an ordinary native u32 there and every mask the code applies to it (and every
 * raw u32 access) agrees with the named bits on both. *Corrected 2026-10-15:* this used DISC_STRUCT (big-endian
 * storage on the PC), which fixed the bit order but broke every raw u32 access to a model's first word. */
typedef union GSmodelFlags {
    u32 raw;
    struct {
#ifndef TARGET_PC
        u32 pad00_10 : 11;
        u32 boundCheck : 1;
        u32 pad12_13 : 2;
        u32 rootNullAdded : 1;
        u32 pad15_20 : 6;
        u32 particleLinked : 1;
        u32 particleTexStage : 1;
        u32 pad23 : 1;
        u32 renderAltJObj : 1;
        u32 pad25_29 : 5;
        u32 visible : 1;
        u32 active : 1;
#else
        u32 active : 1;
        u32 visible : 1;
        u32 pad25_29 : 5;
        u32 renderAltJObj : 1;
        u32 pad23 : 1;
        u32 particleTexStage : 1;
        u32 particleLinked : 1;
        u32 pad15_20 : 6;
        u32 rootNullAdded : 1;
        u32 pad12_13 : 2;
        u32 boundCheck : 1;
        u32 pad00_10 : 11;
#endif
    } bits;
} GSmodelFlags;

/* GSmodel flags.raw bits (one list; these were copied into six units). Two naming sets are in use and kept: the
 * animation units say MODEL_FLAG_*, the main / draw units GSMODEL_FLAG_*. Where both name a bit they mean the same
 * thing seen from two sides (0x80: an animation blend is running, so the alternate joint tree is drawn; 0x20000: a null
 * root joint was added at load, so animations go to the root's child). */
#define MODEL_FLAG_IN_USE              0x00000001
#define MODEL_FLAG_HAS_ANIM            0x00000004
#define MODEL_FLAG_HAS_TEX_ANIM        0x00000008
#define MODEL_FLAG_HAS_SHAPE_ANIM      0x00000010
#define MODEL_FLAG_ANIMATING           0x00000020
#define MODEL_FLAG_TEX_ANIMATING       0x00000040
#define MODEL_FLAG_BLENDING            0x00000080
#define MODEL_FLAG_PAUSED              0x00000100
#define MODEL_FLAG_60FPS_ANIM          0x00000800
#define MODEL_FLAG_SKIP_APPLY          0x00001000
#define MODEL_FLAG_LINK_TEX_TO_ANIM    0x00002000
#define MODEL_FLAG_ANIM_ENDED          0x00004000
#define MODEL_FLAG_TEX_ANIM_ENDED      0x00008000
#define MODEL_FLAG_FORCE_UPDATE        0x00010000
#define MODEL_FLAG_USE_JOBJ_CHILD      0x00020000
#define MODEL_FLAG_ATTACHMENT_ONCE     0x00040000
#define MODEL_FLAG_UPDATE_ATTACHMENT   0x00080000
#define GSMODEL_FLAG_ACTIVE            0x00000001U
#define GSMODEL_FLAG_VISIBLE           0x00000002U
#define GSMODEL_FLAG_HAS_ANIM          0x00000004U
#define GSMODEL_FLAG_HAS_TEX_ANIM      0x00000008U
#define GSMODEL_FLAG_HAS_SHAPE_ANIM    0x00000010U
#define GSMODEL_FLAG_RENDER_ALT_JOBJ   0x00000080U
#define GSMODEL_FLAG_PARTICLE_TEXSTAGE 0x00000200U
#define GSMODEL_FLAG_PARTICLE_LINKED   0x00000400U
#define GSMODEL_FLAG_LINK_TEX_TO_ANIM  0x00002000U
#define GSMODEL_FLAG_ROOT_NULL_ADDED   0x00020000U
#define GSMODEL_FLAG_BOUND_CHECK       0x00100000U
#define GSMODEL_FLAG_UPDATE_BOUND      0x00200000U
#define GSMODEL_FLAG_BOUND_VISIBLE     0x00400000U
/* Shadow bits (GSmodelSetShadowFlags): 0x10000000, 0x20000000 (casts a shadow), 0x40000000; see gs_model_shadow*.c. */

struct GSmodel {
    /* 0x000 */ GSmodelFlags flags;
    /* 0x004 */ GSmodelResource* resource;
    /* 0x008 */ union {
        HSDJObj* renderJObj;          /* the main code's view */
        struct HSD_JObj* renderHsdJObj; /* the same joint as HSD's type (gs_model_anim) */
    };
    /* 0x00C */ union {
        HSDJObj* renderJObjAlt;           /* the main code's view (drawn when the alternate-joint flag is set) */
        struct HSD_JObj* renderHsdJObjAlt; /* the same joint as HSD's type (gs_model_bound) */
    };
    /* 0x010 */ struct HSD_JObj* blendJObjA;   /* anim-blend joint trees (gs_model_bound.c) */
    /* 0x014 */ struct HSD_JObj* blendJObjB;
    /* 0x018 */ GSvec position;
    /* 0x024 */ GSvec rotation;
    /* 0x030 */ GSvec scale;
    /* 0x03C */ GSvec boundCenter;
    /* 0x048 */ f32 boundYOffset;
    /* 0x04C */ GSbound bound;
    /* 0x080 */ u8 centerNullState;
    /* 0x081 */ u8 _pad081[0x03];
    /* 0x084 */ u32 animCount;
    /* 0x088 */ u32 texAnimCount;
    /* 0x08C */ u32 animType;
    /* 0x090 */ u32 animIndex;
    /* 0x094 */ f32 animRate;
    /* 0x098 */ f32 animFrame;
    /* 0x09C */ f32 animRequestedFrame;
    /* 0x0A0 */ f32 animEndFrame;
    /* 0x0A4 */ u32 texAnimType;
    /* 0x0A8 */ u32 texAnimIndex;
    /* 0x0AC */ f32 texAnimRate;
    /* 0x0B0 */ f32 texAnimFrame;
    /* 0x0B4 */ f32 texAnimRequestedFrame;
    /* 0x0B8 */ f32 texAnimEndFrame;
    /* 0x0BC */ u32 blendAnimIndexA;   /* animation blending between two joint trees (gs_model_anim / _bound) */
    /* 0x0C0 */ u32 blendAnimIndexB;
    /* 0x0C4 */ f32 blendAnimFrameA;
    /* 0x0C8 */ f32 blendAnimFrameB;
    /* 0x0CC */ f32 blendAnimEndFrameA;
    /* 0x0D0 */ f32 blendAnimEndFrameB;
    /* 0x0D4 */ f32 blendFactor;
    /* 0x0D8 */ f32 blendFrameScale;
    /* 0x0DC */ GSmodelAnimEndedCallback animEndedCallback;
    /* 0x0E0 */ void* animEndedCallbackArg;
    /* 0x0E4 */ GSmodelPartAnimMix partAnimMix[4];
    /* 0x114 */ union {
        struct { /* attached to another model's part, or a transform override (gs_model_main's names) */
            s32 transformOverride;            /* attachment type; 0 = none */
            struct GSmodel* attachmentModel;
            s32 attachmentPartIndex;
            GSvec overridePosition;
            GSvec overrideRotation;
            GSvec overrideScale;
        };
        struct { /* gs_model_anim: fractional frames forced on */
            s32 forceFractionalFrames;
            void* fractionalFrameData;
        } animation;
    };
    /* 0x144 */ void* linkedGSparticleBank;
    /* 0x148 */ u32 gsParticleLinkAttachMode;
    /* 0x14C */ GSmodelMaterialList* materialList;
    /* 0x150 */ u16 materialCount;
    /* 0x152 */ u16 modulationRefCount;
    /* 0x154 */ u16 materialListHandle;
    /* 0x156 */ u8 _pad156[0x02];
    /* 0x158 */ u32 shadowSurfaceCount; /* GSmodelSetShadowSurface: the models this model's shadow falls on, */
    /* 0x15C */ u32* shadowSurfaces;    /* as 32-bit model words (callers store PC_PTR32; read with PC_PTR) */
    /* 0x160 */ struct GSlight* shadowLight;
    /* 0x164 */ u16 shadowSurfaceHandle; /* GSmem handle of shadowSurfaces */
    /* 0x166 */ u8 _pad166[0x02];
    /* 0x168 */ s32 vertexCount;
    /* 0x16C */ s32 polygonCount;
};

#ifndef TARGET_PC
typedef char GSmodel_check_size[sizeof(GSmodel) == 0x170 ? 1 : -1];
typedef char GSmodel_check_blend[(u32)&((GSmodel*)0)->blendFactor == 0xD4 ? 1 : -1];
typedef char GSmodel_check_shadow[(u32)&((GSmodel*)0)->shadowLight == 0x160 ? 1 : -1];
#endif

#endif /* GAME_GS_MODEL_STRUCT_H */
