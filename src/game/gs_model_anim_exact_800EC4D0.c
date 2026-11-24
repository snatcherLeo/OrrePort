/**
 * @file gs_model_anim_exact_800EC4D0.c
 * @brief Exact GSmodel animation accessor/apply island, 0x800EC4D0 - 0x800ED4D4.
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

extern GSmodel* lbl_8047AB74;
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

#pragma force_active on
void GSmodelGetFrameCount(GSmodel* model, f32* anim_frames, f32* tex_frames)
{
    if (!(model->flags.raw & MODEL_FLAG_BLENDING)) {
        if (anim_frames != NULL) {
            *anim_frames = lbl_8047CC60 + model->animEndFrame;
        }
        if (tex_frames != NULL) {
            *tex_frames = lbl_8047CC64;
        }
    } else {
        if (anim_frames != NULL) {
            *anim_frames = lbl_8047CC60 + model->blendAnimEndFrameA;
        }
        if (tex_frames != NULL) {
            *tex_frames = lbl_8047CC60 + model->blendAnimEndFrameB;
        }
    }
}
f32 GSmodelGetAnimFrame(GSmodel* model)
{
    u32 flags = model->flags.raw;

    if (!(flags & MODEL_FLAG_HAS_ANIM)) {
        return lbl_8047CC5C;
    }
    if (flags & MODEL_FLAG_BLENDING) {
        return model->blendAnimFrameB;
    }
    return model->animFrame;
}
u32 GSmodelGetAnimType(GSmodel* model)
{
    return model->animType;
}
f32 GSmodelGetAnimRate(GSmodel* model)
{
    return model->animRate;
}
void GSmodelGetAnimIndex(GSmodel* model, u32* index_a, u32* index_b)
{
    if (!(model->flags.raw & MODEL_FLAG_BLENDING)) {
        *index_a = model->animIndex;
        *index_b = -1;
    } else {
        *index_a = model->blendAnimIndexA;
        *index_b = model->blendAnimIndexB;
    }
}
u32 GSmodelIsBlending(GSmodel* model)
{
    return (model->flags.raw >> 7) & 1;
}
void GSmodelSetBlendFactor(GSmodel* model, f32 factor)
{
    u32 flags = model->flags.raw;

    if (flags & MODEL_FLAG_HAS_ANIM) {
        if (flags & MODEL_FLAG_BLENDING) {
            goto clamp_factor;
        }
    }
    return;

clamp_factor:
    if (factor < lbl_8047CC5C) {
        factor = lbl_8047CC5C;
    } else if (factor > lbl_8047CC60) {
        factor = lbl_8047CC60;
    }
    model->blendFactor = factor;
}
void GSmodelSetAnimBlend(GSmodel* model, u32 index_a, u32 index_b)
{
    HSD_JObj* jobj;
    u32 flags;
    u32 type;

    flags = model->flags.raw;
    if (!(flags & MODEL_FLAG_HAS_ANIM)) {
        return;
    }
    if (index_a > model->animCount || index_b > model->animCount) {
        return;
    }

    model->flags.raw = flags | MODEL_FLAG_BLENDING;
    model->blendAnimIndexA = index_a;
    model->blendAnimIndexB = index_b;
    fn_801A2B5C(model->blendJObjA,
                DP(DP(model->resource->anims)[model->blendAnimIndexA]), NULL,
                NULL);
    fn_801A2B5C(model->blendJObjB,
                DP(DP(model->resource->anims)[model->blendAnimIndexB]), NULL,
                NULL);

    model->blendFactor = lbl_8047CC5C;
    model->blendAnimFrameA = lbl_8047CC5C;
    model->blendAnimFrameB = lbl_8047CC5C;
    HSD_JObjReqAnimAllByFlags(model->blendJObjA, 0x1CB, model->blendAnimFrameA);
    HSD_JObjReqAnimAllByFlags(model->blendJObjB, 0x1CB, model->blendAnimFrameB);
    HSD_JObjAnimAll(model->blendJObjA);
    HSD_JObjAnimAll(model->blendJObjB);

    model->blendAnimEndFrameA = lbl_8047CC5C;
    HSD_ForeachAnim(model->blendJObjA, 6, 0x9B2F,
                    (void*)_modelGetEndFrame, 2,
                    &model->blendAnimEndFrameA);
    model->blendAnimEndFrameB = lbl_8047CC5C;
    HSD_ForeachAnim(model->blendJObjB, 6, 0x9B2F,
                    (void*)_modelGetEndFrame, 2,
                    &model->blendAnimEndFrameB);
    model->blendFrameScale =
        (lbl_8047CC60 + model->blendAnimEndFrameA) /
        (lbl_8047CC60 + model->blendAnimEndFrameB);

    flags = model->flags.raw;
    if (flags & MODEL_FLAG_LINK_TEX_TO_ANIM) {
        jobj = model->renderHsdJObj;
        if ((flags & MODEL_FLAG_HAS_TEX_ANIM) &&
            index_b < model->texAnimCount) {
            if (index_b != model->texAnimIndex) {
                if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
                    jobj = jobj->child;
                }
                model->texAnimIndex = index_b;
                fn_801A2B5C(
                    jobj, NULL,
                    DP(DP(model->resource->texAnims)[model->texAnimIndex]), NULL);
                model->texAnimEndFrame = lbl_8047CC5C;
                HSD_ForeachAnim(jobj, 6, 0x64DB,
                                (void*)_modelGetEndFrame, 2,
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
                model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
            }

            type = model->texAnimType;
            flags = model->flags.raw;
            jobj = model->renderHsdJObj;
            if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
                jobj = jobj->child;
            }
            model->texAnimType = type;
            switch (model->texAnimType) {
            case 0:
                HSD_ForeachAnim(
                    jobj, 6, 0x64DB,
                    (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 0);
                break;
            case 1:
                HSD_ForeachAnim(
                    jobj, 6, 0x64DB,
                    (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 1);
                break;
            }
        }
    }

    model->flags.raw = model->flags.raw & ~MODEL_FLAG_SKIP_APPLY;
}
void GSmodelSetBlendAnimFrameForce(GSmodel* model, f32 frame_a, f32 frame_b)
{
    model->blendFrameScale = lbl_8047CC5C;
    model->blendAnimFrameA = frame_a;
    model->blendAnimFrameB = frame_b;
}
void GSmodelAllUnpauseAnimation(void)
{
    u32 i;

    for (i = 0; i < lbl_8047AB78; i++) {
        if (lbl_8047AB74[i].flags.raw & MODEL_FLAG_IN_USE) {
            lbl_8047AB74[i].flags.raw &= ~(u32)MODEL_FLAG_PAUSED;
        }
    }
}
void GSmodelAllPauseAnimation(void)
{
    u32 i;

    for (i = 0; i < lbl_8047AB78; i++) {
        if (lbl_8047AB74[i].flags.raw & MODEL_FLAG_IN_USE) {
            lbl_8047AB74[i].flags.raw |= MODEL_FLAG_PAUSED;
        }
    }
}
u32 GSmodelHasAnimationEnded(GSmodel* model)
{
    return (model->flags.raw >> 14) & 1;
}
u32 GSmodelIsAnimating(GSmodel* model)
{
    return (model->flags.raw >> 5) & 1;
}
void GSmodelStopAnimation(GSmodel* model)
{
    model->flags.raw = model->flags.raw & ~MODEL_FLAG_ANIMATING;

    if (model->flags.raw & MODEL_FLAG_LINK_TEX_TO_ANIM) {
        model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIMATING;
    }
}
void GSmodelStartAnimation(GSmodel* model)
{
    u32 flags = model->flags.raw;

    if (!(flags & MODEL_FLAG_HAS_ANIM)) {
        return;
    }

    model->flags.raw = flags | MODEL_FLAG_ANIMATING;
    model->flags.raw = model->flags.raw & ~MODEL_FLAG_ANIM_ENDED;

    flags = model->flags.raw;
    if (!(flags & MODEL_FLAG_LINK_TEX_TO_ANIM)) {
        return;
    }
    if (!(flags & MODEL_FLAG_HAS_TEX_ANIM)) {
        return;
    }

    model->flags.raw = flags | MODEL_FLAG_TEX_ANIMATING;
    model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
}
void GSmodelSetAnimRate(GSmodel* model, f32 rate)
{
    if (model->flags.raw & MODEL_FLAG_HAS_ANIM) {
        model->animRate = rate;
        if (fn_800D37CC() == 50) {
            model->animRate *= lbl_8047CC58;
        }

        if ((model->flags.raw & MODEL_FLAG_LINK_TEX_TO_ANIM) &&
            (model->flags.raw & MODEL_FLAG_HAS_TEX_ANIM)) {
            model->texAnimRate = rate;
            if (fn_800D37CC() == 50) {
                model->texAnimRate *= lbl_8047CC58;
            }
        }
    }
}
void GSmodelSetAnimFrame(GSmodel* model, f32 frame)
{
    HSD_JObj* jobj;
    u32 flags = model->flags.raw;

    jobj = model->renderHsdJObj;
    if (!(flags & MODEL_FLAG_HAS_ANIM)) {
        return;
    }

    if (!(flags & MODEL_FLAG_BLENDING)) {
        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
            jobj = jobj->child;
        }
        model->animFrame = frame;
        model->animRequestedFrame = frame;
        HSD_JObjReqAnimAllByFlags(jobj, 0x1CB, model->animRequestedFrame);
    } else {
        model->blendAnimFrameA = frame * model->blendFrameScale;
        model->blendAnimFrameB = frame;
        HSD_JObjReqAnimAllByFlags(model->blendJObjA, 0x1CB, model->blendAnimFrameA);
        HSD_JObjReqAnimAllByFlags(model->blendJObjB, 0x1CB, model->blendAnimFrameB);
    }

    model->flags.raw &= ~(MODEL_FLAG_SKIP_APPLY | MODEL_FLAG_ANIM_ENDED);

    flags = model->flags.raw;
    if (flags & MODEL_FLAG_LINK_TEX_TO_ANIM) {
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
}
void GSmodelSetAnimType(GSmodel* model, u32 type)
{
    HSD_JObj* jobj;

    jobj = model->renderHsdJObj;
    if (model->flags.raw & MODEL_FLAG_USE_JOBJ_CHILD) {
        jobj = jobj->child;
    }

    model->animType = type;
    switch (model->animType) {
    case 0:
        HSD_ForeachAnim(jobj, 6, 0x9B2F,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 0);
        break;
    case 1:
        HSD_ForeachAnim(jobj, 6, 0x9B2F,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 1);
        break;
    }

    if (model->flags.raw & MODEL_FLAG_LINK_TEX_TO_ANIM) {
        jobj = model->renderHsdJObj;
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
}
void GSmodelSetAnimIndex(GSmodel* model, u32 index)
{
    HSD_JObj* jobj = model->renderHsdJObj;
    u32 flags = model->flags.raw;
    u32 type;

    if (!(flags & MODEL_FLAG_HAS_ANIM)) {
        return;
    }
    if (index >= model->animCount) {
        return;
    }

    if (index != model->animIndex || (flags & MODEL_FLAG_BLENDING)) {
        void* shape_anim;

        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
            jobj = jobj->child;
        }
        model->flags.raw = ((volatile GSmodel*)model)->flags.raw &
                       ~MODEL_FLAG_BLENDING;
        model->animIndex = index;
        if (model->flags.raw & MODEL_FLAG_HAS_SHAPE_ANIM) {
            shape_anim = DP(DP(model->resource->shapeAnims)[model->animIndex]);
        } else {
            shape_anim = NULL;
        }
        fn_801A2B5C(jobj, DP(DP(model->resource->anims)[model->animIndex]), NULL,
                    shape_anim);
        model->animEndFrame = lbl_8047CC5C;
        HSD_ForeachAnim(jobj, 6, 0x9B2F, (void*)_modelGetEndFrame, 2,
                        &model->animEndFrame);
    }

    flags = model->flags.raw;
    jobj = model->renderHsdJObj;
    if (flags & MODEL_FLAG_HAS_ANIM) {
        if (!(flags & MODEL_FLAG_BLENDING)) {
            if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
                jobj = jobj->child;
            }
            model->animFrame = lbl_8047CC5C;
            model->animRequestedFrame = lbl_8047CC5C;
            HSD_JObjReqAnimAllByFlags(jobj, 0x1CB, model->animRequestedFrame);
        } else {
            model->blendAnimFrameA =
                lbl_8047CC5C * model->blendFrameScale;
            model->blendAnimFrameB = lbl_8047CC5C;
            HSD_JObjReqAnimAllByFlags(model->blendJObjA, 0x1CB,
                        model->blendAnimFrameA);
            HSD_JObjReqAnimAllByFlags(model->blendJObjB, 0x1CB,
                        model->blendAnimFrameB);
        }
        model->flags.raw &= ~(MODEL_FLAG_SKIP_APPLY | MODEL_FLAG_ANIM_ENDED);

        flags = model->flags.raw;
        if (flags & MODEL_FLAG_LINK_TEX_TO_ANIM) {
            jobj = model->renderHsdJObj;
            if (flags & MODEL_FLAG_HAS_TEX_ANIM) {
                if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
                    jobj = jobj->child;
                }
                model->texAnimFrame = lbl_8047CC5C;
                model->texAnimRequestedFrame = lbl_8047CC5C;
                HSD_JObjReqAnimAllByFlags(jobj, 0x634, model->texAnimRequestedFrame);
                model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
            }
        }
    }

    type = model->animType;
    flags = model->flags.raw;
    jobj = model->renderHsdJObj;
    if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
        jobj = jobj->child;
    }
    model->animType = type;
    switch (model->animType) {
    case 0:
        HSD_ForeachAnim(jobj, 6, 0x9B2F,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 0);
        break;
    case 1:
        HSD_ForeachAnim(jobj, 6, 0x9B2F,
                        (void*)_modelSetLoopFlag__FP9_HSD_AObjUl, 3, 1);
        break;
    }
    if (model->flags.raw & MODEL_FLAG_LINK_TEX_TO_ANIM) {
        jobj = model->renderHsdJObj;
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

    flags = model->flags.raw;
    if (flags & MODEL_FLAG_LINK_TEX_TO_ANIM) {
        jobj = model->renderHsdJObj;
        if (!(flags & MODEL_FLAG_HAS_TEX_ANIM) ||
            index >= model->texAnimCount) {
            goto done;
        }
        if (index != model->texAnimIndex) {
            if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
                jobj = jobj->child;
            }
            model->texAnimIndex = index;
            fn_801A2B5C(jobj, NULL,
                        DP(DP(model->resource->texAnims)[model->texAnimIndex]), NULL);
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
            model->flags.raw = model->flags.raw & ~MODEL_FLAG_TEX_ANIM_ENDED;
        }

        type = model->texAnimType;
        flags = model->flags.raw;
        jobj = model->renderHsdJObj;
        if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
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

done:
    model->flags.raw = model->flags.raw & ~MODEL_FLAG_SKIP_APPLY;
}
void GSmodelAdvanceAnimation(GSmodel* model, f32 delta)
{
    u32 flags = model->flags.raw;

    if (flags & MODEL_FLAG_PAUSED) {
        return;
    }

    if (flags & MODEL_FLAG_ANIM_ENDED) {
        fn_800ED6E4(model, FALSE);
    }
    if (model->flags.raw & MODEL_FLAG_TEX_ANIM_ENDED) {
        fn_800ED6E4(model, TRUE);
    }

    flags = model->flags.raw;
    if (flags & MODEL_FLAG_BLENDING) {
        if (flags & MODEL_FLAG_ANIMATING) {
            fn_800ED7E4(model, FALSE, delta * model->animRate);
            model->blendAnimFrameA =
                model->blendAnimFrameB * model->blendFrameScale;
        }
    } else {
        if (flags & MODEL_FLAG_ANIMATING) {
            fn_800ED7E4(model, FALSE, delta * model->animRate);
        }
        if (model->flags.raw & MODEL_FLAG_TEX_ANIMATING) {
            fn_800ED7E4(model, TRUE, delta * model->texAnimRate);
        }
    }

    model->flags.raw &= ~(MODEL_FLAG_FORCE_UPDATE | MODEL_FLAG_SKIP_APPLY);
}
void modelApplyAnimation__FP8_GSmodel(GSmodel* model)
{
    HSD_JObj* jobj;
    HSD_AObj* anim_aobj;
    HSD_AObj* tex_anim_aobj;
    f32 anim_rate;
    f32 tex_anim_rate;
    u32 flags;
    u32 fractional_frames;

    flags = model->flags.raw;
    if (flags & MODEL_FLAG_SKIP_APPLY) {
        return;
    }

    if (flags & MODEL_FLAG_BLENDING) {
        HSD_JObjReqAnimAllByFlags(model->blendJObjA, 0x1CB,
                    model->blendAnimFrameA);
        HSD_JObjReqAnimAllByFlags(model->blendJObjB, 0x1CB,
                    model->blendAnimFrameB);
        HSD_JObjAnimAll(model->blendJObjA);
        HSD_JObjAnimAll(model->blendJObjB);
        modelCalculateBlendModel__FP8_GSmodelf(model);
        goto done;
    }

    jobj = model->renderHsdJObj;
    if (flags & MODEL_FLAG_USE_JOBJ_CHILD) {
        jobj = jobj->child;
    }

    anim_rate = lbl_8047CC5C;
    tex_anim_rate = anim_rate;
    if (flags & MODEL_FLAG_ANIMATING) {
        if ((flags & MODEL_FLAG_60FPS_ANIM) == MODEL_FLAG_60FPS_ANIM ||
            (model->force_fractional_frames != 0 &&
             model->fractional_frame_data != NULL)) {
            fractional_frames = TRUE;
        } else {
            fractional_frames = FALSE;
        }
        anim_rate = fn_800ED8C4(model->animType, fractional_frames,
                                model->animFrame, model->animRequestedFrame,
                                model->animEndFrame, model->animRate);
    }

    flags = model->flags.raw;
    if (flags & MODEL_FLAG_TEX_ANIMATING) {
        if ((flags & MODEL_FLAG_60FPS_ANIM) == MODEL_FLAG_60FPS_ANIM ||
            (model->force_fractional_frames != 0 &&
             model->fractional_frame_data != NULL)) {
            fractional_frames = TRUE;
        } else {
            fractional_frames = FALSE;
        }
        tex_anim_rate = fn_800ED8C4(
            model->texAnimType, fractional_frames, model->texAnimFrame,
            model->texAnimRequestedFrame, model->texAnimEndFrame,
            model->texAnimRate);
    }

#ifdef TARGET_PC
    { /* Port correctness fix (2026-11-13, port-gaps 78; all platforms, game logic only): menu Pokemon models
       * (the summary page's Espeon) show only whole animation frames, as on the console. Why: those models are made for
       * whole frames (no 60fps-animation flag) and animate at rate 0.5; fn_800ED8C4 snaps a half frame to the next whole
       * one when a game frame lasts two retraces, and the console always runs these pages at two (Dolphin: 30 poses a
       * second, verified). The PC runs them at one retrace, where the snap adds nothing, so HSD evaluated the half
       * frames the console never draws, and Espeon's body curve overshoots at frame 0.5 just after each loop (verified
       * from the per-joint log: the body joint jumps 0.055 for one frame every 3 s). Here a half frame keeps the last
       * whole-frame pose (HSD's step 0); the model's own clock, loop and blink timing are untouched (look only).
       * menu_model.c names the model each frame it draws one; the name lapses a frame after it stops. */
        extern void* pc_console_cadence_model;
        extern u32 pc_console_cadence_frame;
        extern volatile u32 pc_vi_heartbeat;
        if (pc_console_cadence_model == (void*)model && pc_vi_heartbeat - pc_console_cadence_frame <= 1 &&
            GSgetFrameStep() == 1 && (model->flags.raw & MODEL_FLAG_60FPS_ANIM) == 0 &&
            !(model->force_fractional_frames != 0 && model->fractional_frame_data != NULL)) {
            if ((model->flags.raw & MODEL_FLAG_ANIMATING) &&
                model->animFrame - (f32)(s32)model->animFrame > 0.0001f) {
                anim_rate = 0.0f;
            }
            if ((model->flags.raw & MODEL_FLAG_TEX_ANIMATING) &&
                model->texAnimFrame - (f32)(s32)model->texAnimFrame > 0.0001f) {
                tex_anim_rate = 0.0f;
            }
        }
    }
#endif
    HSD_ForeachAnim(jobj, 6, 0x9B2F, (void*)HSD_AObjSetRate, 1,
                    anim_rate);
    HSD_ForeachAnim(jobj, 6, 0x64DB, (void*)HSD_AObjSetRate, 1,
                    tex_anim_rate);
    HSD_JObjAnimAll(jobj);

    if (model->flags.raw & MODEL_FLAG_ANIMATING) {
        anim_aobj = NULL;
        HSD_ForeachAnim(jobj, 6, 0x20,
                        (void*)_modelGetAObjFunc__FP9_HSD_AObjPv, 2,
                        &anim_aobj);
        model->animRequestedFrame = anim_aobj->curr_frame;
    }

    if (model->flags.raw & MODEL_FLAG_TEX_ANIMATING) {
        tex_anim_aobj = NULL;
        HSD_ForeachAnim(jobj, 6, 0x480,
                        (void*)_modelGetAObjFunc__FP9_HSD_AObjPv, 2,
                        &tex_anim_aobj);
        model->texAnimRequestedFrame = tex_anim_aobj->curr_frame;
    }

#ifdef TARGET_PC
    { /* PC (observe only, 2026-11-13, port-gaps 78: Espeon's summary model jitters at 60 frames a second, smooth
       * at the console's 30): with ORRE_MENUMODEL_LOG=1, menu_model.c names the summary page's model and this logs each
       * time its animation is applied: the model's frame and rate, the step handed to HSD's joint player (fn_800ED8C4's
       * result), the 30-at-60 switches it reads, and two joints' rotations, to see where the in-between poses go uneven.
       * All platforms the same (game logic, no OS calls). */
        extern void* pc_menumodel_watch;
        if (pc_menumodel_watch != NULL && pc_menumodel_watch == (void*)model) {
            extern void OSReport(const char*, ...);
            extern volatile u32 pc_vi_heartbeat;
            extern u8 lbl_80478AF8;
            extern s32 fn_800D37CC(void);
            HSD_JObj* j1 = jobj != NULL ? jobj->child : NULL;
            HSD_JObj* j2 = j1;
            int depth;
            for (depth = 0; depth < 4 && j2 != NULL && j2->child != NULL; depth++) {
                j2 = j2->child;
            }
            OSReport("[menumodel] apply: game frame %u, flags %08x, 30at60 %u/%d, frame %.4f, req %.4f, end %.1f, "
                     "rate %.4f, step %.4f, j1 rot %.5f %.5f %.5f, j2 rot %.5f %.5f %.5f\n",
                     (unsigned)pc_vi_heartbeat, (unsigned)model->flags.raw, (unsigned)lbl_80478AF8, (int)fn_800D37CC(),
                     model->animFrame, model->animRequestedFrame, model->animEndFrame, model->animRate, anim_rate,
                     j1 != NULL ? j1->rotate_x : 0.0f, j1 != NULL ? j1->rotate_y : 0.0f,
                     j1 != NULL ? j1->rotate_z : 0.0f, j2 != NULL ? j2->rotate_x : 0.0f,
                     j2 != NULL ? j2->rotate_y : 0.0f, j2 != NULL ? j2->rotate_z : 0.0f);
            { /* then every joint (depth first: child, then next), one line each, for the first 1,200 applies: which of
               * Espeon's joints step unevenly at the PC's half frames (the two above don't) */
                static int joint_frames;
                if (joint_frames < 1200 && jobj != NULL) {
                    HSD_JObj* stack[64];
                    int top = 0;
                    int index = 0;
                    joint_frames++;
                    stack[top++] = jobj;
                    while (top > 0 && index < 256) {
                        HSD_JObj* j = stack[--top];
                        OSReport("[mmj] %u %d %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f\n", (unsigned)pc_vi_heartbeat,
                                 index, j->rotate_x, j->rotate_y, j->rotate_z, j->translate_x, j->translate_y,
                                 j->translate_z, j->scale_x, j->scale_y, j->scale_z);
                        index++;
                        if (j->next != NULL && top < 64) stack[top++] = j->next;
                        if (j->child != NULL && top < 64) stack[top++] = j->child;
                    }
                }
            }
        }
    }
#endif

    if (model->flags.raw & MODEL_FLAG_ANIMATING) {
        _modelResetPartAnimMixes__FP8_GSmodel(model);
    }

    if (model->flags.raw & MODEL_FLAG_USE_JOBJ_CHILD) {
        jobj = model->renderHsdJObj->child;
        if (jobj != NULL) {
            s32 dirty;
            u32 jobj_flags;

            if (jobj == NULL) {
                __assert(lbl_8047CC68, 0x25D, lbl_8047CC70);
            }
            jobj_flags = jobj->flags;
            dirty = FALSE;
            if (!(jobj_flags & JOBJ_USER_DEF_MTX)) {
                if (jobj_flags & JOBJ_MTX_DIRTY) {
                    dirty = TRUE;
                }
            }
            if (dirty != FALSE) {
                HSD_JObjSetupMatrixSub(jobj);
            }
        }
    }

    jobj = model->renderHsdJObj;
    if (jobj != NULL) {
        s32 dirty;
        u32 jobj_flags;

        if (jobj == NULL) {
            __assert(lbl_8047CC68, 0x25D, lbl_8047CC70);
        }
        jobj_flags = jobj->flags;
        dirty = FALSE;
        if (!(jobj_flags & JOBJ_USER_DEF_MTX)) {
            if (jobj_flags & JOBJ_MTX_DIRTY) {
                dirty = TRUE;
            }
        }
        if (dirty != FALSE) {
            HSD_JObjSetupMatrixSub(jobj);
        }
    }

done:
    model->flags.raw |= MODEL_FLAG_SKIP_APPLY;
}
#pragma force_active off

#ifdef TARGET_PC
/* PC: GSmodel.animType for code that read it at the raw GameCube offset 0x8C (people.c's motion waits) */
u32 pc_model_anim_type(void* model)
{
    return ((GSmodel*)model)->animType;
}
#endif

#ifdef TARGET_PC
/* PC diagnostic (2026-11-07, the reference playthrough P12 note 9: playing one Realgam badge orb's animation lit another orb too):
 * a model's resource, joints, DObjs, MObjs (material), TObjs (texture, image, palette, animation image table) and
 * animation objects, so two models can be compared for shared parts. Observe-only; called with ORRE_MAPOBJ_LOG. */
#include "hsd/hsd_dobj.h"
static void pcModelWalkJoints(HSD_JObj* j, int depth)
{
    extern void OSReport(const char*, ...);
    for (; j != NULL && depth < 8; j = j->next) {
        OSReport("[mapobj]   %*sjobj %p flags 0x%08X aobj %p\n", depth * 2, "", (void*)j, (unsigned)j->flags,
                 (void*)j->aobj);
        if (!(j->flags & (JOBJ_PTCL | JOBJ_SPLINE))) {
            HSD_DObj* d;
            for (d = j->u.dobj; d != NULL; d = d->next) {
                HSD_MObj* m = d->mobj;
                HSD_TObj* t;
                OSReport("[mapobj]   %*s dobj %p aobj %p mobj %p mat %p maobj %p\n", depth * 2, "", (void*)d,
                         (void*)d->aobj, (void*)m, m ? (void*)m->mat : NULL, m ? (void*)m->aobj : NULL);
                for (t = m ? m->tobj : NULL; t != NULL; t = t->next) {
                    OSReport("[mapobj]   %*s  tobj %p imagedesc %p tlut %p imagetbl %p tluttbl %p aobj %p\n",
                             depth * 2, "", (void*)t, (void*)t->imagedesc, (void*)t->tlut, (void*)t->imagetbl,
                             (void*)t->tluttbl, (void*)t->aobj);
                }
            }
        }
        pcModelWalkJoints(j->child, depth + 1);
    }
}

/* The map objects std115 animated last (up to 8), for pc_mapobj_tick's per-frame material report. */
static struct {
    u32 resource;
    GSmodel* model;
} s_pcMapObjs[8];

/* Once per game frame (pc_vi.c), with ORRE_MAPOBJ_LOG and inside ORRE_MAPOBJ_FRAMES=<from>-<to>: each tracked map
 * object's material colours (ambient, diffuse, specular, alpha) and render mode, DObj by DObj under its first joints. */
void pc_mapobj_tick(unsigned frame)
{
    extern void OSReport(const char*, ...);
    extern char* getenv(const char*);
    extern unsigned long strtoul(const char*, char**, int);
    static int init;
    static unsigned from, to;
    int i;
    if (!init) {
        const char* r = getenv("ORRE_MAPOBJ_FRAMES");
        char* end;
        init = 1;
        if (r != NULL) {
            from = (unsigned)strtoul(r, &end, 10);
            to = (*end == '-') ? (unsigned)strtoul(end + 1, NULL, 10) : from;
        }
    }
    if (to == 0 || frame < from || frame > to) {
        return;
    }
    for (i = 0; i < 8; i++) {
        GSmodel* model = s_pcMapObjs[i].model;
        HSD_JObj* j;
        if (model == NULL) {
            continue;
        }
        OSReport("[mapmat] %u obj %u anim %u frame %g end %g type %u flags 0x%08X:", frame,
                 (unsigned)s_pcMapObjs[i].resource, (unsigned)model->texAnimIndex, (double)model->texAnimFrame,
                 (double)model->texAnimEndFrame, (unsigned)model->texAnimType, (unsigned)model->flags.raw);
        for (j = model->renderHsdJObj ? model->renderHsdJObj->child : NULL; j != NULL; j = j->next) {
            HSD_DObj* d;
            if (j->flags & (JOBJ_PTCL | JOBJ_SPLINE)) {
                continue;
            }
            for (d = j->u.dobj; d != NULL; d = d->next) {
                HSD_MObj* mo = d->mobj;
                HSD_Material* mat = mo ? mo->mat : NULL;
                if (mat != NULL) {
                    OSReport(" [d %p m %p mat %p rm %08X amb %02X%02X%02X dif %02X%02X%02X spc %02X%02X%02X a %.2f",
                             (void*)d, (void*)mo, (void*)mat, (unsigned)mo->rendermode, mat->ambient.r, mat->ambient.g, mat->ambient.b, mat->diffuse.r,
                             mat->diffuse.g, mat->diffuse.b, mat->specular.r, mat->specular.g, mat->specular.b,
                             (double)mat->alpha);
                    if (mo->aobj != NULL) { /* the material animation: flags, frame, rewind, end, rate */
                        HSD_AObj* ao = mo->aobj;
                        extern void* pc_watch_fobj;
                        if (s_pcMapObjs[i].resource == 0x591003u && d == j->u.dobj->next) {
                            static int s_armed;
                            const char* armAt = getenv("ORRE_MAPOBJ_ARM"); /* a frame: arm a write-watch then */
                            pc_watch_fobj = ao->fobj; /* the red orb's overlay material, current (P12 note 9) */
                            if (!s_armed && armAt != NULL && frame >= (unsigned)strtoul(armAt, NULL, 10)) {
                                extern void pc_crash_watch_add(unsigned long long addr, unsigned len);
                                s_armed = 1;
                                pc_crash_watch_add((unsigned long long)(uintptr_t)&mat->alpha, 4);
                            }
                        }
                        HSD_FObj* fo;
                        int nf = 0;
                        OSReport(" ao %p %08X cur %g rew %g end %g rate %g", (void*)ao, (unsigned)ao->flags,
                                 (double)ao->curr_frame, (double)ao->rewind_frame, (double)ao->end_frame,
                                 (double)ao->framerate);
                        for (fo = ao->fobj; fo != NULL && nf < 4; fo = fo->next, nf++) {
                            OSReport(" {fo %p fl %02X op %u/%u ty %u pk %u sf %d ft %u t %g ad +%d/%u p %g>%g}",
                                     (void*)fo, fo->flags, fo->op, fo->op_intrp, fo->obj_type, fo->nb_pack,
                                     (int)fo->startframe, (unsigned)fo->fterm, (double)fo->time,
                                     (int)(fo->ad - fo->ad_head), (unsigned)fo->length, (double)fo->p0,
                                     (double)fo->p1);
                        }
                    }
                    OSReport("]");
                }
            }
        }
        OSReport("\n");
    }
}

void pc_mapobj_debug(u32 resource, void* m, u32 anim, const char* why)
{
    extern void OSReport(const char*, ...);
    extern char* getenv(const char*);
    static int on = -1;
    GSmodel* model = (GSmodel*)m;
    int i, slot = -1;
    if (on < 0) {
        on = getenv("ORRE_MAPOBJ_LOG") != NULL;
    }
    if (!on || model == NULL) {
        return;
    }
    for (i = 0; i < 8 && slot < 0; i++) { /* track it for pc_mapobj_tick: its own slot, else the next in turn */
        if (s_pcMapObjs[i].resource == resource) {
            slot = i;
        }
    }
    if (slot < 0) {
        static int s_next;
        slot = s_next;
        s_next = (s_next + 1) & 7;
    }
    s_pcMapObjs[slot].resource = resource;
    s_pcMapObjs[slot].model = model;
    OSReport("[mapobj] %s object %u anim %u: model %p resource %p texAnims %p texAnim %u/%u flags 0x%08X\n", why,
             (unsigned)resource, (unsigned)anim, (void*)model, (void*)model->resource,
             model->resource ? (void*)DP(model->resource->texAnims) : NULL, (unsigned)model->texAnimIndex,
             (unsigned)model->texAnimCount, (unsigned)model->flags.raw);
    pcModelWalkJoints(model->renderHsdJObj, 0);
}
#endif

#ifdef TARGET_PC
/* PC diagnostic (2026-10-17, Outskirt Stand's truck_b1 isn't drawn): the model's draw-relevant state */
void pc_model_debug_print(void* m, const char* tag)
{
    extern void OSReport(const char*, ...);
    GSmodel* model = (GSmodel*)m;
    HSD_JObj* j = model->renderHsdJObj;
    OSReport("[model] %s: flags 0x%08X scale %g %g %g anim %u/%u type %u frame %g end %g jobj %p",
             tag, (unsigned)model->flags.raw, (double)model->scale.x, (double)model->scale.y, (double)model->scale.z,
             (unsigned)model->animIndex, (unsigned)model->animCount, (unsigned)model->animType,
             (double)model->animFrame, (double)model->animEndFrame, (void*)j);
    if (j != NULL) {
        OSReport(" jflags 0x%08X jscale %g %g %g child %p", (unsigned)j->flags, (double)j->scale_x,
                 (double)j->scale_y, (double)j->scale_z, (void*)j->child);
        if (j->child != NULL) {
            OSReport(" cflags 0x%08X cscale %g %g %g", (unsigned)j->child->flags, (double)j->child->scale_x,
                     (double)j->child->scale_y, (double)j->child->scale_z);
        }
    }
    OSReport("%c", 10);
}
#endif
