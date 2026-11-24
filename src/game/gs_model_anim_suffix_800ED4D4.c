/**
 * @file gs_model_anim_suffix_800ED4D4.c
 * @brief GSmodel animation suffix, 0x800ED4D4 - 0x800EE150.
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
f32 fn_800ED8C4(s32 type, u8 fractional_frames, f32 frame,
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
        u32 flags;                                                            \
        if (!(dirty_jobj->flags & JOBJ_MTX_INDEP_SRT)) {                      \
            if (dirty_jobj != NULL) {                                         \
                if (dirty_jobj == NULL) {                                     \
                    __assert(lbl_8047CC68, 0x25D, lbl_8047CC70);              \
                }                                                             \
                dirty = 0;                                                    \
                flags = dirty_jobj->flags;                                    \
                if (!(flags & JOBJ_USER_DEF_MTX)) {                           \
                    if (flags & JOBJ_MTX_DIRTY) {                             \
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


extern s32 lbl_8047ABAC;
extern s32 lbl_8047ABB0;
typedef void (*GSmodelAObjApplyFunc)(HSD_AObj* aobj, void* arg);
void HSD_JObjWalkTree(HSD_JObj* jobj, GSmodelAObjApplyFunc func, u32 arg);
void fn_800EE20C(HSD_AObj* aobj, void* arg);

#if !defined(GS_MODEL_ANIM_SUFFIX_SPLIT) || \
    defined(GS_MODEL_ANIM_SUFFIX_PREFIX)
void modelUpdateAttachments__FP8_GSmodel(GSmodel* model)
{
    GSvec position;
    GSvec rotation;
    GSvec scale;
    GSquat quaternion;
    GSvec transformed_position;
    GSpart* part;
    u32 flags;

    if (model->attachment_type != 0 &&
        (model->flags.raw & MODEL_FLAG_UPDATE_ATTACHMENT)) {
        part = GSmodelGetPart(model->attachment_model,
                              model->attachment_part_index);
        GSpartGetTransform(part, &position, &rotation, &scale);
        GSpartFree(part);

        GSquatFromEuler(&quaternion, &rotation);
        GSvecTransformQuat(&transformed_position, &quaternion,
                           &model->attachment_position);
        GSvecAdd(&position, &position, &transformed_position);
        GSvecAdd(&rotation, &rotation, &model->attachment_rotation);
        fn_800E0108(&scale, &scale, &model->attachment_scale);

        modelSetPos(model, &model->attachment_position);
        modelSetRot(model, &model->attachment_rotation);
        modelSetScl(model, &model->attachment_scale);

        switch (model->attachment_type) {
        case 1:
            modelSetPos(model, &position);
            break;
        case 2:
            modelSetRot(model, &rotation);
            break;
        case 3:
            modelSetScl(model, &scale);
            break;
        case 4:
            modelSetPos(model, &position);
            modelSetRot(model, &rotation);
            break;
        case 5:
            modelSetRot(model, &rotation);
            modelSetScl(model, &scale);
            break;
        case 6:
            modelSetPos(model, &position);
            modelSetScl(model, &scale);
            break;
        case 7:
            modelSetPos(model, &position);
            modelSetRot(model, &rotation);
            modelSetScl(model, &scale);
            break;
        }

        flags = model->flags.raw;
        if (flags & MODEL_FLAG_ATTACHMENT_ONCE) {
            model->flags.raw = flags & ~MODEL_FLAG_UPDATE_ATTACHMENT;
        } else {
            model->flags.raw = flags | MODEL_FLAG_UPDATE_ATTACHMENT;
        }
    }

    model->flags.raw = model->flags.raw | MODEL_FLAG_FORCE_UPDATE;
}
u32 modelProcessAt60fps__FP8_GSmodel(GSmodel* model)
{
    if ((model->flags.raw & MODEL_FLAG_60FPS_ANIM) == MODEL_FLAG_60FPS_ANIM) {
        goto yes;
    }
    if (model->attachment_type == 0) {
        goto no;
    }
    if (model->attachment_model == NULL) {
        goto no;
    }
yes:
    return TRUE;
no:
    return FALSE;
}
void fn_800ED6E4(GSmodel* model, u8 tex_anim)
{
    u32 end_flag_mask;
    u32 anim_flag_mask;
    s32 anim_type;
    u32 anim_index;
    GSmodelAnimEndedInfo info;

    if (!(model->flags.raw & MODEL_FLAG_BLENDING)) {
        if (!tex_anim) {
            anim_index = model->animIndex;
            end_flag_mask = MODEL_FLAG_ANIM_ENDED;
            anim_type = model->animType;
            anim_flag_mask = MODEL_FLAG_ANIMATING;
        } else {
            end_flag_mask = MODEL_FLAG_TEX_ANIM_ENDED;
            anim_index = model->texAnimIndex;
            anim_type = model->texAnimType;
            anim_flag_mask = MODEL_FLAG_TEX_ANIMATING;
        }
    } else if (!tex_anim) {
        anim_index = model->blendAnimIndexB;
        end_flag_mask = MODEL_FLAG_ANIM_ENDED;
        anim_type = model->animType;
        anim_flag_mask = MODEL_FLAG_ANIMATING;
    }

    switch (anim_type) {
    case 0:
        model->flags.raw &= ~anim_flag_mask;
        break;
    case 1:
        model->flags.raw &= ~end_flag_mask;
        break;
    }

    if (model->animEndedCallback != NULL) {
        info.event_flags = 0;
        if (tex_anim == 0) {
            info.event_flags |= 1;
        } else {
            info.event_flags |= 2;
        }
        if (anim_type == 1) {
            info.event_flags |= 4;
        }
        info.anim_index = anim_index;
        info.arg = model->animEndedCallbackArg;
        model->animEndedCallback(model, &info);
    }
}
void fn_800ED7E4(GSmodel* model, u8 tex_anim, f32 delta)
{
    u32 ended_flag;
    f32* frame_ptr;
    f32* req_ptr;
    s32 type;
    f32 end_frame;

    if (!(model->flags.raw & MODEL_FLAG_BLENDING)) {
        if (!tex_anim) {
            end_frame = model->animEndFrame;
            frame_ptr = &model->animFrame;
            type = model->animType;
            req_ptr = &model->animRequestedFrame;
            ended_flag = MODEL_FLAG_ANIM_ENDED;
        } else {
            end_frame = model->texAnimEndFrame;
            type = model->texAnimType;
            frame_ptr = &model->texAnimFrame;
            req_ptr = &model->texAnimRequestedFrame;
            ended_flag = MODEL_FLAG_TEX_ANIM_ENDED;
        }
    } else if (!tex_anim) {
        frame_ptr = &model->blendAnimFrameB;
        end_frame = model->blendAnimEndFrameB;
        type = model->animType;
        req_ptr = frame_ptr;
        ended_flag = MODEL_FLAG_ANIM_ENDED;
    }

    *frame_ptr += delta;
    if (*frame_ptr >= end_frame - lbl_8047CC78) {
        switch (type) {
        case 0:
            *frame_ptr = end_frame;
            break;
        case 1:
            *frame_ptr -= end_frame;
            break;
        }
    }
    if (*req_ptr >= end_frame - lbl_8047CC78) {
        if (type == 0) {
            model->flags.raw |= ended_flag;
        }
    }
}
#endif

#if !defined(GS_MODEL_ANIM_SUFFIX_SPLIT) || \
    defined(GS_MODEL_ANIM_SUFFIX_MIDDLE)
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 fmod(f64 x, f64 y);
#endif
extern u8 lbl_80478AF8;

/*
 * The two int-to-float biases are this object's own literal pool
 * (0x8047CC80/0x8047CC88). The 0.0f, 1.0f and 0.0001f it shares with the
 * rest of the original animation TU stay in sdata2_8047CBE0.c.
 * RULE-EXCEPTION(title-path): extern named stand-ins for the TU's own pool
 * literals (lbl_8047CC5C/CC60/CC78, and lbl_8047CC68/CC70 in
 * _modelResetPartAnimMixes) - see docs/RULE_EXCEPTIONS.md
 */
f32 fn_800ED8C4(s32 type, u8 fractional_frames, f32 frame,
                f32 requested_frame, f32 end_frame, f32 rate)
{
    f32 current;
    f32 advance;
    f32 result = lbl_8047CC5C;

    if (lbl_80478AF8 == 1 && fn_800D37CC() == 60) {
        if (!fractional_frames) {
            if (frame - (f32)(s32)frame > lbl_8047CC78) {
                advance = rate * (f32)(GSgetFrameStep() - 1);

                if (advance >= lbl_8047CC60) {
                    advance = lbl_8047CC5C;
                }
                current = frame + advance;
            } else {
                current = frame;
            }
        } else {
            current = frame;
        }
    } else {
        current = frame;
    }

    if (end_frame > lbl_8047CC5C && current >= end_frame - lbl_8047CC78) {
        switch (type) {
        case 0:
            current = end_frame;
            break;
        case 1:
            current = fmod(current, end_frame);
            break;
        }
    }

    if (current != requested_frame) {
        if (current < requested_frame) {
            if (type == 1) {
                result = current + (end_frame - requested_frame);
            } else {
                result = lbl_8047CC5C;
            }
        } else {
            result = current - requested_frame;
        }
    }
    if (result < lbl_8047CC5C) {
        result = lbl_8047CC5C;
    }
    return result;
}

void _modelResetPartAnimMixes__FP8_GSmodel(GSmodel* model)
{
    GSmodelPartAnimMix* mix;
    s32 i;
    GSpart* part;
    HSD_JObj* jobj;

    mix = model->partAnimMix;
    for (i = 4; i-- != 0; mix++) {
        if (mix->type == 0) {
            continue;
        }

        part = GSmodelGetPart(model, mix->partIndex);
        if (part == NULL) {
            continue;
        }

        switch (mix->type) {
        case 1:
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_SET_ROT(jobj, mix->value[0], 0x2A4,
                                              0x2A5, rotate_x);
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_SET_ROT(jobj, mix->value[1], 0x2B8,
                                              0x2B9, rotate_y);
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_SET_ROT(jobj, mix->value[2], 0x2CC,
                                              0x2CD, rotate_z);
            break;
        case 2:
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_ADD_ROT(jobj, mix->value[0], 0x412,
                                             rotate_x);
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_ADD_ROT(jobj, mix->value[1], 0x41D,
                                             rotate_y);
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_ADD_ROT(jobj, mix->value[2], 0x428,
                                             rotate_z);
            break;
        case 3:
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_ADD_ROT(jobj, mix->value[0], 0x412,
                                             rotate_x);
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_SET_ROT(jobj, mix->value[1], 0x2B8,
                                              0x2B9, rotate_y);
            jobj = part->jobj;
            MODEL_RESET_PART_ANIM_MIX_ADD_ROT(jobj, mix->value[2], 0x428,
                                             rotate_z);
            break;
        }

        GSpartFree(part);
    }
}
#endif

#if !defined(GS_MODEL_ANIM_SUFFIX_SPLIT) || \
    defined(GS_MODEL_ANIM_SUFFIX_SUFFIX)
void _modelGetAObjFunc__FP9_HSD_AObjPv(HSD_AObj* aobj, HSD_AObj** out)
{
    if (aobj != NULL) {
        *out = aobj;
    }
}
void _modelSetLoopFlag__FP9_HSD_AObjUl(HSD_AObj* aobj, u32 enable)
{
    if (enable == 0) {
        HSD_AObjClearFlags(aobj, AOBJ_LOOP);
    } else {
        HSD_AObjSetFlags(aobj, AOBJ_LOOP);
    }
}
void _modelGetEndFrame(HSD_AObj* aobj, f32* end_frame)
{
    if (aobj == NULL) {
        __assert(lbl_8047CC90, 0xAB, lbl_8047CC98);
    }

    if (aobj->end_frame > *end_frame) {
        *end_frame = aobj->end_frame;
    }
}
s32 GSmodelWalkJObjs(GSmodel* model)
{
    lbl_8047ABB0 = 0;
    lbl_8047ABAC = -1;
    HSD_JObjWalkTree(model->renderHsdJObj, fn_800EE20C, 0);
    if (model->flags.raw & MODEL_FLAG_USE_JOBJ_CHILD) {
        lbl_8047ABB0 = lbl_8047ABB0 - 1;
    }
    return lbl_8047ABB0;
}
#endif

#undef MODEL_RESET_PART_ANIM_MIX_ADD_ROT
#undef MODEL_RESET_PART_ANIM_MIX_SET_ROT
#undef MODEL_RESET_PART_ANIM_MIX_DIRTY
