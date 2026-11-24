/**
 * @file hsd_robj.h
 * @brief HSD RObj - Reference/constraint objects.
 *
 * RObj represents constraints and references between objects in the
 * scene graph. Types include expression constraints, joint references,
 * limits, bytecode expressions, and IK hints.
 *
 * Adapted from the Melee decompilation (doldecomp/melee).
 */
#ifndef HSD_ROBJ_H
#define HSD_ROBJ_H

#include "dolphin/types.h"
#include "hsd/hsd_forward.h"

/* ========================================================================= */
/*  RObj type flags                                                          */
/* ========================================================================= */

#define ROBJ_TYPE_MASK   0x70000000
#define REFTYPE_EXP      0x00000000
#define REFTYPE_JOBJ     0x10000000
#define REFTYPE_LIMIT    0x20000000
#define REFTYPE_BYTECODE 0x30000000
#define REFTYPE_IKHINT   0x40000000

/* ========================================================================= */
/*  Supporting structures                                                    */
/* ========================================================================= */

struct HSD_Rvalue {
    HSD_Rvalue* next;
    u32 flags;
    HSD_JObj* jobj;
};

struct DISC_STRUCT HSD_RvalueList {
    u32 flags;
    DISC_PTR(HSD_Joint) joint;
};

struct HSD_IKHint {
    f32 bone_length;
    f32 rotate_x;
};

struct DISC_STRUCT HSD_IKHintDesc {
    f32 bone_length;
    f32 rotate_x;
};

struct HSD_Exp {
    union {
        f32 (*func)(void*);
        u8* bytecode;
    } expr;
    HSD_Rvalue* rvalue;
    u32 nb_args;
    u8 is_bytecode;
};

struct HSD_ExpDesc {
    f32 (*func)(void*);
    HSD_RvalueList* rvalue;
};

struct DISC_STRUCT HSD_ByteCodeExpDesc {
    DISC_PTR(u8) bytecode;
    DISC_PTR(HSD_RvalueList) rvalue;
};

/* ========================================================================= */
/*  RObj structure                                                           */
/* ========================================================================= */

struct HSD_RObj {
    HSD_RObj* next;
    u32 flags;
    union {
        HSD_JObj* jobj;
        HSD_Exp exp;
        f32 limit;
        HSD_IKHint ik_hint;
    } u;
    HSD_AObj* aobj;
};

/* ========================================================================= */
/*  RObj descriptor (data format)                                            */
/* ========================================================================= */

struct DISC_STRUCT HSD_RObjDesc {
    DISC_PTR(HSD_RObjDesc) next;
    u32 flags;
    union DISC_STRUCT {
        u32 i;
        DISC_PTR(HSD_ExpDesc) exp; /* HSD_ExpDesc holds a code pointer: built by game code, not on the disc (likely) */
        DISC_PTR(HSD_ByteCodeExpDesc) bcexp;
        DISC_PTR(HSD_IKHintDesc) ik_hint;
        DISC_PTR(HSD_Joint) joint;
        f32 limit;
    } u;
};

/* ========================================================================= */
/*  RObj animation joint                                                     */
/* ========================================================================= */

struct DISC_STRUCT HSD_RObjAnimJoint {
    DISC_PTR(HSD_RObjAnimJoint) next;
    DISC_PTR(HSD_AObjDesc) aobjdesc;
};

/* ========================================================================= */
/*  Function declarations                                                    */
/* ========================================================================= */

HSD_RObj* HSD_RObjAlloc(void);
void HSD_RObjFree(HSD_RObj* robj);
void HSD_RObjSetFlags(HSD_RObj* robj, u32 flags);
HSD_RObj* HSD_RObjGetByType(HSD_RObj* robj, u32 type, u32 subtype);
void HSD_RObjAnimAll(HSD_RObj* robj);
void HSD_RObjRemoveAnimAll(HSD_RObj* robj);
void HSD_RObjReqAnimAll(HSD_RObj* robj, f32 startframe);
void HSD_RObjAddAnimAll(HSD_RObj* robj, HSD_RObjAnimJoint* anim);
void HSD_RObjAddAnimAll(HSD_RObj* robj, HSD_RObjAnimJoint* anim);
void HSD_RObjRemove(HSD_RObj* robj);
void HSD_RObjRemoveAll(HSD_RObj* robj);
void HSD_RObjResolveRefs(HSD_RObj* robj, HSD_RObjDesc* desc);
void HSD_RObjResolveRefsAll(HSD_RObj* robj, HSD_RObjDesc* desc);
HSD_RObj* HSD_RObjLoadDesc(HSD_RObjDesc* desc);
void HSD_RObjUpdateAll(HSD_RObj* robj, void* obj, HSD_ObjUpdateFunc func);


/* Disc layout of the description structs (GameCube sizes; the port keeps them, see disc_types.h). */
#ifndef TARGET_PC
typedef char HSD_RObjDesc_check_disc_size[sizeof(struct HSD_RObjDesc) == 0xC ? 1 : -1];
#endif

#endif /* HSD_ROBJ_H */
