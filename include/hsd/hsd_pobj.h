/**
 * @file hsd_pobj.h
 * @brief HSD PObj - Primitive/polygon objects.
 *
 * PObj manages mesh/polygon data: vertex descriptor lists, display
 * list data, skinning (rigid, envelope, shape animation), and
 * rendering dispatch.
 *
 * Colosseum address range: 0x801AA608 (PObjInfoInit)
 * Adapted from the Melee decompilation (doldecomp/melee).
 */
#ifndef HSD_POBJ_H
#define HSD_POBJ_H

#include "dolphin/types.h"
#include "hsd/hsd_aobj.h"
#include "hsd/hsd_class.h"
#include "hsd/hsd_forward.h"

/* ========================================================================= */
/*  Matrix type constants                                                    */
/* ========================================================================= */

#define HSD_MTX_RIGID    1
#define HSD_MTX_ENVELOPE 2

/* ========================================================================= */
/*  PObj structure                                                           */
/* ========================================================================= */

struct HSD_PObj {
    HSD_Class parent;
    HSD_PObj* next;
    HSD_VtxDescList* verts;
    u16 flags;
    u16 n_display;
    u8* display;               /* display list: primitive, vtxcnt, indices */
    union {
        HSD_JObj* jobj;
        HSD_ShapeSet* shape_set;
        HSD_SList* envelope_list;
    } u;
    HSD_AObj* aobj;
};

/* ========================================================================= */
/*  PObj descriptor (data format)                                            */
/* ========================================================================= */

/* 16-bit values in a disc vertex array (shape-animation blending reads them on the CPU): big-endian on the PC. */
#ifdef TARGET_PC
#define HSD_DISC_U16(v) ((u16) __builtin_bswap16(v))
#define HSD_DISC_S16(v) ((s16) __builtin_bswap16(v))
#else
#define HSD_DISC_U16(v) (v)
#define HSD_DISC_S16(v) (v)
#endif

typedef DISC_PTR(HSD_EnvelopeDesc) HSD_EnvelopeDescSlot; /* one entry of a disc envelope list */
typedef DISC_PTR(u8) HSD_U8Slot;                          /* one entry of a disc index-list table */

struct DISC_STRUCT HSD_PObjDesc {
    DISC_PTR(char) class_name;
    DISC_PTR(HSD_PObjDesc) next;
    DISC_PTR(HSD_VtxDescList) verts;
    u16 flags;
    u16 n_display;
    DISC_PTR(u8) display;
    union DISC_STRUCT {
        DISC_PTR(HSD_Joint) joint;
        DISC_PTR(HSD_ShapeSetDesc) shape_set;
        DISC_PTR(HSD_EnvelopeDescSlot) envelope_p;
    } u;
};

/* ========================================================================= */
/*  Vertex descriptor list                                                   */
/* ========================================================================= */

struct DISC_STRUCT HSD_VtxDescList { /* disc data, used in place by the runtime PObj */
    s32 attr;          /* GXAttr */
    s32 attr_type;     /* GXAttrType */
    s32 comp_cnt;      /* GXCompCnt */
    s32 comp_type;     /* GXCompType */
    u8 frac;
    u16 stride;
    DISC_PTR(void) vertex;
};

/* ========================================================================= */
/*  Envelope (skeletal skinning weights)                                     */
/* ========================================================================= */

struct HSD_Envelope {
    HSD_Envelope* next;
    HSD_JObj* jobj;
    f32 weight;
};

struct DISC_STRUCT HSD_EnvelopeDesc {
    DISC_PTR(HSD_Joint) joint;
    f32 weight;
};

/* ========================================================================= */
/*  Shape set (morph target / shape animation)                               */
/* ========================================================================= */

struct HSD_ShapeSet {
    u16 flags;
    u16 nb_shape;
    s32 nb_vertex_index;
    HSD_VtxDescList* vertex_desc;
    HSD_U8Slot* vertex_idx_list; /* disc table, used in place */
    s32 nb_normal_index;
    HSD_VtxDescList* normal_desc;
    HSD_U8Slot* normal_idx_list; /* disc table, used in place */
    union {
        f32* bp;
        f32 bl;
    } blend;
};

struct DISC_STRUCT HSD_ShapeSetDesc {
    u16 flags;
    u16 nb_shape;
    s32 nb_vertex_index;
    DISC_PTR(HSD_VtxDescList) vertex_desc;
    DISC_PTR(HSD_U8Slot) vertex_idx_list;
    s32 nb_normal_index;
    DISC_PTR(HSD_VtxDescList) normal_desc;
    DISC_PTR(HSD_U8Slot) normal_idx_list;
};

/* ========================================================================= */
/*  Shape animation types                                                    */
/* ========================================================================= */

struct DISC_STRUCT HSD_ShapeAnim {
    DISC_PTR(HSD_ShapeAnim) next;
    DISC_PTR(HSD_AObjDesc) aobjdesc;
};

struct DISC_STRUCT HSD_ShapeAnimJoint {
    DISC_PTR(HSD_ShapeAnimJoint) child;
    DISC_PTR(HSD_ShapeAnimJoint) next;
    DISC_PTR(HSD_ShapeAnimDObj) shapeanimdobj;
};

/* ========================================================================= */
/*  PObj class info                                                          */
/* ========================================================================= */

struct HSD_PObjInfo {
    HSD_ClassInfo parent;
    void (*disp)(HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4],
                 u32 rendermode);
    void (*setup_mtx)(HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4],
                      u32 rendermode);
    s32 (*load)(HSD_PObj* pobj, HSD_PObjDesc* desc);
    /* Colosseum: the shape-animation update callback (PObjUpdateFunc). */
    void (*update)(void* obj, u32 type, HSD_ObjData* val);
};

/* ========================================================================= */
/*  Globals and macros                                                       */
/* ========================================================================= */

extern HSD_PObjInfo hsdPObj;

#define HSD_POBJ(o) ((HSD_PObj*) (o))
#define HSD_POBJ_INFO(i) ((HSD_PObjInfo*) (i))
#define HSD_POBJ_METHOD(o) HSD_POBJ_INFO(HSD_CLASS_METHOD(o))

/* ========================================================================= */
/*  Function declarations                                                    */
/* ========================================================================= */

HSD_PObj* HSD_PObjAlloc(void);
u32 HSD_PObjGetFlags(HSD_PObj* pobj);
HSD_PObj* HSD_PObjLoadDesc(HSD_PObjDesc* desc);
void HSD_PObjAddAnim(HSD_PObj* pobj, HSD_ShapeAnim* anim);
void HSD_PObjAddAnimAll(HSD_PObj* pobj, HSD_ShapeAnim* anim);
void HSD_PObjAnim(HSD_PObj* pobj);
void HSD_PObjAnimAll(HSD_PObj* pobj);
void HSD_PObjResolveRefs(HSD_PObj* pobj, HSD_PObjDesc* desc);
void HSD_PObjResolveRefsAll(HSD_PObj* pobj, HSD_PObjDesc* desc);
void HSD_PObjRemove(HSD_PObj* pobj);
void HSD_PObjRemoveAll(HSD_PObj* pobj);
void HSD_PObjDisp(HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4],
                  u32 rendermode);


/* Disc layout of the description structs (GameCube sizes; the port keeps them, see disc_types.h). */
#ifndef TARGET_PC
typedef char HSD_PObjDesc_check_disc_size[sizeof(struct HSD_PObjDesc) == 0x18 ? 1 : -1];
typedef char HSD_VtxDescList_check_disc_size[sizeof(struct HSD_VtxDescList) == 0x18 ? 1 : -1];
typedef char HSD_EnvelopeDesc_check_disc_size[sizeof(struct HSD_EnvelopeDesc) == 0x8 ? 1 : -1];
typedef char HSD_ShapeAnim_check_disc_size[sizeof(struct HSD_ShapeAnim) == 0x8 ? 1 : -1];
typedef char HSD_ShapeAnimJoint_check_disc_size[sizeof(struct HSD_ShapeAnimJoint) == 0xC ? 1 : -1];
typedef char HSD_ShapeSetDesc_check_disc_size[sizeof(struct HSD_ShapeSetDesc) == 0x1C ? 1 : -1];
#endif

#endif /* HSD_POBJ_H */
