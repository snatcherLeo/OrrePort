/**
 * @file gs_model_anim.c
 * @brief GSmodel animation prefix, 0x800EBEEC - 0x800EC4D0.
 *
 * Mechanical address partition of the recovered GSmodel animation source.
 */
#include "dolphin/types.h"
#include "game/gs_model_anim.h"
#include "hsd/hsd_aobj.h"
#include "hsd/hsd_jobj.h"


#include "game/gs_model_struct.h" /* GSmodel and its helper types (a private copy with GameCube padding was here) */

/* This code's names for the shared GSmodel's attachment / animation union */
#define force_fractional_frames animation.forceFractionalFrames
#define fractional_frame_data animation.fractionalFrameData
#define attachment_type transformOverride
#define attachment_model attachmentModel
#define attachment_part_index attachmentPartIndex
#define attachment_position overridePosition
#define attachment_rotation overrideRotation
#define attachment_scale overrideScale


extern u32 lbl_8047AB78;

extern const f32 lbl_8047CC58;
extern const f32 lbl_8047CC5C;
extern const f32 lbl_8047CC60;
extern const f32 lbl_8047CC64;
extern const f32 lbl_8047CC78; /* defined SDATA2 in src/game/data/sdata2_8047CBE0.c = 0.0001f */
extern char lbl_8047CC90[] __attribute__((section(".sdata2")));
extern char lbl_8047CC98[] __attribute__((section(".sdata2")));
extern char lbl_8047CC68[] __attribute__((section(".sdata2")));
extern char lbl_8047CC70[] __attribute__((section(".sdata2")));
extern char lbl_80270EE8[];

void HSD_ForeachAnim(void* obj, u32 type, u32 mask, void* func, u32 arg_type, ...);
void HSD_JObjReqAnimAllByFlags(HSD_JObj* jobj, u32 flags, f32 frame);
void fn_801A2B5C(HSD_JObj* jobj, void* anim, void* mat_anim,
                 void* shape_anim);
void modelCalculateBlendModel__FP8_GSmodelf(GSmodel* model);
f32 fn_800ED8C4(u32 type, u32 fractional_frames, f32 frame,
                f32 requested_frame, f32 end_frame, f32 rate);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_800D37CC(void);
#endif
void _modelGetAObjFunc__FP9_HSD_AObjPv(HSD_AObj* aobj, HSD_AObj** out);
void _modelResetPartAnimMixes__FP8_GSmodel(GSmodel* model);
void _modelSetLoopFlag__FP9_HSD_AObjUl(HSD_AObj* aobj, u32 enable);
void _modelGetEndFrame(HSD_AObj* aobj, f32* end_frame);
void fn_800ED6E4(GSmodel* model, u8 tex_anim);
void fn_800ED7E4(GSmodel* model, u8 tex_anim, f32 delta);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
GSpart* GSmodelGetPart(GSmodel* model, s32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSpartGetTransform(GSpart* part, GSvec* position, GSvec* rotation,
                        GSvec* scale);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSpartFree(GSpart* part);
#endif
void GSquatFromEuler(GSquat* quaternion, GSvec* rotation);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSvecTransformQuat(GSvec* dst, GSquat* quaternion, GSvec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSvecAdd(GSvec* dst, GSvec* lhs, GSvec* rhs);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_800E0108(GSvec* dst, GSvec* lhs, GSvec* rhs);
#endif
void modelSetPos(GSmodel* model, GSvec* position);
void modelSetRot(GSmodel* model, GSvec* rotation);
void modelSetScl(GSmodel* model, GSvec* scale);
void fn_8019D620(HSD_JObj* jobj);
void HSD_JObjSetupMatrixSub(HSD_JObj* jobj);

typedef struct GSmodelAnimEndedInfo {
    u32 event_flags;
    u32 anim_index;
    void* arg;
} GSmodelAnimEndedInfo;

#define MODEL_RESET_PART_ANIM_MIX_DIRTY(jobj_)                                \
    do {                                                                      \
        HSD_JObj* dirty_jobj = (jobj_);                                        \
        s32 dirty;                                                            \
        if (!(dirty_jobj->flags & JOBJ_MTX_INDEP_SRT)) {                      \
            if (dirty_jobj != NULL) {                                         \
                if (dirty_jobj == NULL) {                                     \
                    __assert(lbl_8047CC68, 0x25D, lbl_8047CC70);              \
                }                                                             \
                dirty = 0;                                                    \
                if (!(dirty_jobj->flags & JOBJ_USER_DEF_MTX)) {               \
                    if (dirty_jobj->flags & JOBJ_MTX_DIRTY) {                 \
                        dirty = 1;                                            \
                    }                                                         \
                }                                                             \
                if (dirty == 0) {                                             \
                    fn_8019D620(dirty_jobj);                                  \
                }                                                             \
            }                                                                 \
        }                                                                     \
    } while (0)

#define MODEL_RESET_PART_ANIM_MIX_SET_ROT(jobj_, value_, null_line_,          \
                                          spline_line_, field_)               \
    do {                                                                      \
        HSD_JObj* set_jobj = (jobj_);                                          \
        f32 set_value = (value_);                                             \
        if (set_jobj == NULL) {                                               \
            __assert(lbl_8047CC68, (null_line_), lbl_8047CC70);               \
        }                                                                     \
        if (set_jobj->flags & JOBJ_USE_QUATERNION) {                          \
            __assert(lbl_8047CC68, (spline_line_), lbl_80270EE8);             \
        }                                                                     \
        set_jobj->field_ = set_value;                                         \
        MODEL_RESET_PART_ANIM_MIX_DIRTY(set_jobj);                            \
    } while (0)

#define MODEL_RESET_PART_ANIM_MIX_ADD_ROT(jobj_, value_, null_line_, field_)  \
    do {                                                                      \
        HSD_JObj* add_jobj = (jobj_);                                          \
        f32 add_value = (value_);                                             \
        if (add_jobj == NULL) {                                               \
            __assert(lbl_8047CC68, (null_line_), lbl_8047CC70);               \
        }                                                                     \
        add_jobj->field_ += add_value;                                        \
        MODEL_RESET_PART_ANIM_MIX_DIRTY(add_jobj);                            \
    } while (0)

#undef MODEL_RESET_PART_ANIM_MIX_ADD_ROT
#undef MODEL_RESET_PART_ANIM_MIX_SET_ROT
#undef MODEL_RESET_PART_ANIM_MIX_DIRTY

/* sModelJObjCount / sModelJObjLastIndex are .sbss globals owned by the
 * gs_model_anim split (config/GC6E01/symbols.txt). GSmodelWalkJObjs uses them
 * as a shared counter/scratch during a JObj traversal driven by fn_800EE20C. */
extern s32 sModelJObjCount;
extern s32 sModelJObjLastIndex;
typedef void (*GSmodelAObjApplyFunc)(HSD_AObj* aobj, void* arg);
void HSD_JObjWalkTree(HSD_JObj* jobj, GSmodelAObjApplyFunc func, u32 arg);
void fn_800EE20C(HSD_AObj* aobj, void* arg);

#if defined(GS_MODEL_ANIM_800EC0E8_800EC35C)

void GSmodelForceAnimTransformUpdate(GSmodel* model)
{
    u32 flags = model->flags.raw;

    if (!(flags & MODEL_FLAG_FORCE_UPDATE) ||
        !(flags & MODEL_FLAG_SKIP_APPLY)) {
        modelApplyAnimation__FP8_GSmodel(model);
        modelUpdateAttachments__FP8_GSmodel(model);
    }
}
void fn_800EC134(GSmodel* model)
{
    modelApplyAnimation__FP8_GSmodel(model);
}
void GSmodelSetAnimEndedCallback(GSmodel* model,
                                 GSmodelAnimEndedCallback callback, void* arg)
{
    model->animEndedCallback = callback;
    model->animEndedCallbackArg = arg;
}
void GSmodelSet60fpsAnimFlag(GSmodel* model, u8 enable)
{
    if (enable) {
        model->flags.raw |= MODEL_FLAG_60FPS_ANIM;
    } else {
        model->flags.raw = model->flags.raw & ~MODEL_FLAG_60FPS_ANIM;
    }
}
void GSmodelLinkTexAnimToAnim(GSmodel* model, u8 enable)
{
    if (enable) {
        model->flags.raw |= MODEL_FLAG_LINK_TEX_TO_ANIM;
    } else {
        model->flags.raw = model->flags.raw & ~MODEL_FLAG_LINK_TEX_TO_ANIM;
    }
}
u32 GSmodelCanTexAnimate(GSmodel* model)
{
    return (model->flags.raw >> 3) & 1;
}
u32 GSmodelCanAnimate(GSmodel* model)
{
    return (model->flags.raw >> 2) & 1;
}
u32 GSmodelHasTexAnimationEnded(GSmodel* model)
{
    return (model->flags.raw >> 15) & 1;
}
void GSmodelStopTexAnimation(GSmodel* model)
{
    model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIMATING;
}
void GSmodelStartTexAnimation(GSmodel* model)
{
    u32 flags = model->flags.raw;

    if (flags & MODEL_FLAG_HAS_TEX_ANIM) {
        model->flags.raw = flags | MODEL_FLAG_TEX_ANIMATING;
        model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
    }
}
void GSmodelSetTexAnimType(GSmodel* model, u32 type)
{
    HSD_JObj* jobj = model->renderHsdJObj;

    if (model->flags.raw & MODEL_FLAG_USE_JOBJ_CHILD) {
        jobj = jobj->child;
    }

    model->texAnimType = type;
    switch (model->texAnimType) {
    case 0:
        HSD_ForeachAnim(jobj, 6, 0x64DB,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 0);
        break;
    case 1:
        HSD_ForeachAnim(jobj, 6, 0x64DB,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 1);
        break;
    }
}
void GSmodelSetTexAnimFrame(GSmodel* model, f32 frame)
{
    HSD_JObj* jobj;
    u32 flags = model->flags.raw;

    jobj = model->renderHsdJObj;
    if (flags & MODEL_FLAG_HAS_TEX_ANIM) {
        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
            jobj = jobj->child;
        }
        model->texAnimFrame = frame;
        model->texAnimRequestedFrame = frame;
        HSD_JObjReqAnimAllByFlags(jobj, 0x634, model->texAnimRequestedFrame);
        model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
    }
}
void GSmodelSetTexAnimRate(GSmodel* model, f32 rate)
{
    if (model->flags.raw & MODEL_FLAG_HAS_TEX_ANIM) {
        model->texAnimRate = rate;
        if (fn_800D37CC() == 50) {
            model->texAnimRate *= lbl_8047CC58;
        }
    }
}

#endif

#if defined(GS_MODEL_ANIM_800EC35C_800EC4D0)

void GSmodelSetTexAnimIndex(GSmodel* model, u32 index)
{
    HSD_JObj* jobj = model->renderHsdJObj;
    u32 flags = model->flags.raw;

    if (!(flags & MODEL_FLAG_HAS_TEX_ANIM)) {
        return;
    }
    if (index >= model->texAnimCount) {
        return;
    }
    if (index != model->texAnimIndex) {
        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
            jobj = jobj->child;
        }
        model->texAnimIndex = index;
        fn_801A2B5C(jobj, 0, DP(DP(model->resource->texAnims)[model->texAnimIndex]),
                    NULL);
        model->texAnimEndFrame = lbl_8047CC5C;
        HSD_ForeachAnim(jobj, 6, 0x64DB, (void*)_modelGetEndFrame, 2,
                        &model->texAnimEndFrame);
    }

    flags = model->flags.raw;
    jobj = model->renderHsdJObj;
    if (flags & MODEL_FLAG_HAS_TEX_ANIM) {
        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
            jobj = jobj->child;
        }
        model->texAnimFrame = lbl_8047CC5C;
        model->texAnimRequestedFrame = lbl_8047CC5C;
        HSD_JObjReqAnimAllByFlags(jobj, 0x634, model->texAnimRequestedFrame);
        flags = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
        model->flags.raw = flags;
    }

    flags = model->flags.raw;
    {
        /* retail loads tex_anim_type here, before the jobj block, and stores
           it back after -- written adjacently MWCC elides the pair */
        u32 texType = model->texAnimType;
        jobj = model->renderHsdJObj;
        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
            jobj = jobj->child;
        }
        model->texAnimType = texType;
    }
    switch (model->texAnimType) {
    case 0:
        HSD_ForeachAnim(jobj, 6, 0x64DB,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 0);
        break;
    case 1:
        HSD_ForeachAnim(jobj, 6, 0x64DB,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 1);
        break;
    }
}

#endif
