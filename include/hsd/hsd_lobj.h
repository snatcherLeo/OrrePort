/**
 * @file hsd_lobj.h
 * @brief HSD LObj - Light objects.
 *
 * LObj manages hardware lights for the GX graphics pipeline.
 * Supports ambient, infinite (directional), point, and spot lights
 * with attenuation, color, and animation.
 *
 * Colosseum address range: 0x801A4000 (LObjInfoInit)
 * Adapted from the Melee decompilation (doldecomp/melee).
 */
#ifndef HSD_LOBJ_H
#define HSD_LOBJ_H

#include "dolphin/mtx.h"
#include "dolphin/gx/GX.h"
#include "dolphin/types.h"
#include "hsd/hsd_debug.h"
#include "hsd/hsd_forward.h"
#include "hsd/hsd_object.h"

/* ========================================================================= */
/*  Constants                                                                */
/* ========================================================================= */

#define MAX_GXLIGHT 9

/* ========================================================================= */
/*  Light parameter structures                                               */
/* ========================================================================= */

struct HSD_LightPoint {
    f32 cutoff;
    u32 point_func;
    f32 ref_br;
    f32 ref_dist;
    u32 dist_func;
};

struct DISC_STRUCT HSD_LightPointDesc {
    f32 ref_br;
    f32 ref_dist;
    u32 dist_func;
};

struct HSD_LightSpot {
    f32 cutoff;
    u32 spot_func;
    f32 ref_br;
    f32 ref_dist;
    u32 dist_func;
};

struct DISC_STRUCT HSD_LightSpotDesc {
    f32 cutoff;
    u32 spot_func;
    f32 ref_br;
    f32 ref_dist;
    u32 dist_func;
};

struct HSD_LightAttn {
    f32 a0;
    f32 a1;
    f32 a2;
    f32 k0;
    f32 k1;
    f32 k2;
};
/* the attenuation block as stored in a light description (the runtime LObj keeps a native copy) */
#ifdef TARGET_PC
typedef struct DISC_STRUCT HSD_DiscLightAttn { f32 a0, a1, a2, k0, k1, k2; } HSD_DiscLightAttn;
#else
#define HSD_DiscLightAttn HSD_LightAttn
#endif

/* ========================================================================= */
/*  LObj structure                                                           */
/* ========================================================================= */

struct HSD_LObj {
    /* 0x00 */ HSD_Obj parent;
    /* 0x08 */ u16 flags;
    /* 0x0A */ u16 priority;
    /* 0x0C */ HSD_LObj* next;
    /* 0x10 */ GXColor color;
    /* 0x14 */ GXColor hw_color;   /* Colour last pushed to the hardware */
    /* 0x18 */ HSD_WObj* position;
    /* 0x1C */ HSD_WObj* interest;
    /* 0x20 */ union {
        HSD_LightPoint point;
        HSD_LightSpot spot;
        HSD_LightAttn attn;
    } u;
    /* 0x38 */ f32 shininess;
    /* 0x3C */ Vec lvec;
    /* 0x48 */ HSD_AObj* aobj;
    /* 0x4C */ u32 id;             /* GXLightID */
    /* 0x50 */ GXLightObj lightobj;
    /* 0x90 */ s32 spec_id;        /* GXLightID for specular */
    /* 0x94 */ GXLightObj spec_lightobj;
};

/* ========================================================================= */
/*  Light descriptor (data format)                                           */
/* ========================================================================= */

struct DISC_STRUCT HSD_LightDesc {
    DISC_PTR(char) class_name;
    DISC_PTR(HSD_LightDesc) next;
    u16 flags;
    u16 attnflags;
    GXColor color;
    DISC_PTR(HSD_WObjDesc) position;
    DISC_PTR(HSD_WObjDesc) interest;
    union DISC_STRUCT {
        DISC_PTR(void) p;
        DISC_PTR(f32) shininess; /* a big-endian float on the PC (no reader in HSD) */
        DISC_PTR(HSD_LightPointDesc) point;
        DISC_PTR(HSD_LightSpotDesc) spot;
        DISC_PTR(HSD_DiscLightAttn) attn;
    } u;
};

/* ========================================================================= */
/*  Light animation                                                          */
/* ========================================================================= */

struct DISC_STRUCT HSD_LightAnim {
    DISC_PTR(HSD_LightAnim) next;
    DISC_PTR(HSD_AObjDesc) aobjdesc;
    DISC_PTR(HSD_WObjAnim) position_anim;
    DISC_PTR(HSD_WObjAnim) interest_anim;
};

/* ========================================================================= */
/*  LObj class info                                                          */
/* ========================================================================= */

struct HSD_LObjInfo {
    HSD_ObjInfo parent;
    int (*load)(HSD_LObj* lobj, HSD_LightDesc* ldesc);
    void (*update)(HSD_LObj* lobj, u32 type, void* value);
};

/* ========================================================================= */
/*  Globals and macros                                                       */
/* ========================================================================= */

extern HSD_LObjInfo hsdLObj;

#define HSD_LOBJ(o) ((HSD_LObj*) (o))
#define HSD_LOBJ_INFO(i) ((HSD_LObjInfo*) (i))
#define HSD_LOBJ_METHOD(o) HSD_LOBJ_INFO(HSD_OBJECT_METHOD(o))

/* ========================================================================= */
/*  Function declarations                                                    */
/* ========================================================================= */

u32 HSD_LObjGetFlags(HSD_LObj* lobj);
void HSD_LObjSetFlags(HSD_LObj* lobj, u32 flags);
void HSD_LObjClearFlags(HSD_LObj* lobj, u32 flags);
HSD_LObj* HSD_LObjGetActiveByIndex(s32 idx);
HSD_LObj* HSD_LObjGetActiveByID(s32 light_id);
s32 HSD_LObjGetNbActive(void);
void HSD_LObjClearActive(void);
u32 HSD_LObjGetLightMaskSpecular(void);
u32 HSD_LObjGetLightMaskAlpha(void);
s32 HSD_LObjGetLightMaskAttnFunc(void);
u32 HSD_LObjGetLightMaskDiffuse(void);
void HSD_LObjAddAnim(HSD_LObj* lobj, HSD_LightAnim* lanim);
void HSD_LObjAddAnimAll(HSD_LObj* lobj, HSD_LightAnim* lanim);
void HSD_LObjRemoveAnimAll(HSD_LObj* lobj);
void HSD_LObjAnim(HSD_LObj* lobj);
void HSD_LObjAnimAll(HSD_LObj* lobj);
void HSD_LObjReqAnim(HSD_LObj* lobj, f32 startframe);
void HSD_LObjReqAnimAll(HSD_LObj* lobj, f32 startframe);
void HSD_LObjSetup(HSD_CObj* cobj);
void HSD_LObjSetColor(HSD_LObj* lobj, GXColor color);
void HSD_LObjGetLightVector(HSD_LObj* lobj, Vec* dir);
HSD_LObj* HSD_LObjGetCurrentByType(u32 type);
void HSD_LObjAddCurrentAll(HSD_LObj* lobj);
void HSD_LObjDeleteCurrentAll(HSD_LObj* lobj);
s32 HSD_LObjGetPosition(HSD_LObj* lobj, Vec* out);
s32 HSD_LObjGetInterest(HSD_LObj* lobj, Vec* out);
void HSD_LObjSetPosition(HSD_LObj* lobj, Vec* position);
void HSD_LObjSetInterest(HSD_LObj* lobj, Vec* interest);
void HSD_LObjRemoveAll(HSD_LObj* lobj);
HSD_LObj* HSD_LObjAlloc(void);
HSD_LObj* HSD_LObjLoadDesc(HSD_LightDesc* ldesc);
void HSD_LObjSetupSpecular(HSD_LObj* lobj, GXColor color, f32 shininess);

static inline HSD_LObj* HSD_LObjGetNext(HSD_LObj* lobj)
{
    if (lobj == NULL) {
        return NULL;
    }
    return lobj->next;
}


/* Disc layout of the description structs (GameCube sizes; the port keeps them, see disc_types.h). */
#ifndef TARGET_PC
typedef char HSD_LightDesc_check_disc_size[sizeof(struct HSD_LightDesc) == 0x1C ? 1 : -1];
#endif

#endif /* HSD_LOBJ_H */
