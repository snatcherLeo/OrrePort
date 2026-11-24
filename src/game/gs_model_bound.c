/**
 * @file gs_model_bound.c
 * @brief GSmodel bound-recalc + anim-blend interpolation (XD anim.c sub-range)
 *
 * Split from gs_range_800E202C.c (0x800EB268-0x800EBEEC) - one XD source unit per
 * segment (Fable re-split, 2026-07-07). Functions asm-only until matched.
 */
#include "dolphin/types.h"
#include "hsd/hsd_pobj.h"

#include "game/gs_model_struct.h" /* GSmodel and its helper types (a private padded copy was here) */

typedef void (*GSModelPObjDisp)(HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4],
                               f32 smtx[3][4], void* arg);

extern const f32 lbl_8047CC28;
extern const char lbl_8047CC2C[7];
extern const char lbl_8047CC34[5];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* condition);
#endif
extern HSD_JObj* HSD_JObjLoadJoint(HSD_Joint* joint);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimIndex(GSmodel* model, u32 index);
#endif
extern void fn_80191460(GSbound* bound);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimFrame(GSmodel* model, f32 frame);
#endif
extern void fn_800EC134(GSmodel* model);
extern void HSD_JObjSetupMatrixSub(HSDJObj* jobj);
extern void GSmodelParse(GSmodel* model, BOOL is_visible, GSModelPObjDisp disp,
                         void* arg);
extern void _modelBoundPObj__FP9_HSD_PObjP5GSmtxP5GSmtxP5GSmtxPv(
    HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4], f32 smtx[3][4], void* arg);

#if !defined(GS_MODEL_BOUND_800EB464_SUFFIX_ACTIVE)
static inline BOOL HSD_JObjMtxIsDirty(HSDJObj* jobj)
{
    BOOL result;

    if (jobj == NULL) {
        __assert(lbl_8047CC2C, 0x25D, lbl_8047CC34);
    }
    result = FALSE;
    if (!(jobj->flags & 0x00800000) && (jobj->flags & 0x40)) {
        result = TRUE;
    }
    return result;
}

void fn_800EB268(GSmodel* model, u32 anim_index)
{
    void* particle_bank;
    HSDJObj* jobj;

    GSmodelSetAnimIndex(model, anim_index);
    fn_80191460(&model->bound);
    particle_bank = model->linkedGSparticleBank;
    model->linkedGSparticleBank = NULL;
    if (model->flags.raw & 4) {
        GSmodelSetAnimFrame(model, lbl_8047CC28);
        fn_800EC134(model);
        jobj = model->renderJObj;
        if (jobj != NULL && HSD_JObjMtxIsDirty(jobj)) {
            HSD_JObjSetupMatrixSub(jobj);
        }
    }
    GSmodelParse(
        model, TRUE,
        _modelBoundPObj__FP9_HSD_PObjP5GSmtxP5GSmtxP5GSmtxPv, model);
    model->linkedGSparticleBank = particle_bank;
}

void GSmodelRecalculateBound(GSmodel* model)
{
    HSDJObj* jobj;
    void* particle_bank;

    fn_80191460(&model->bound);
    particle_bank = model->linkedGSparticleBank;
    model->linkedGSparticleBank = NULL;
    if (model->flags.raw & 4) {
        GSmodelSetAnimFrame(model, lbl_8047CC28);
        fn_800EC134(model);
        jobj = model->renderJObj;
        if (jobj != NULL && HSD_JObjMtxIsDirty(jobj)) {
            HSD_JObjSetupMatrixSub(jobj);
        }
    }
    GSmodelParse(
        model, TRUE,
        _modelBoundPObj__FP9_HSD_PObjP5GSmtxP5GSmtxP5GSmtxPv, model);
    model->linkedGSparticleBank = particle_bank;
}
#endif

#if defined(GS_MODEL_BOUND_800EB464_SUFFIX_ACTIVE) && \
    !defined(GS_MODEL_BOUND_DECLARATIONS_ONLY)
#include "game/gs_gfx_layer.h" /* GS_DL_F32 */
void _modelBoundVertex__FUlPvPv(u32 flags, void* vertex, void* arg)
{
    typedef f32 GSmtx[3][4];
    typedef struct ModelBoundArgs {
        GSmodel* model;
        GSmtx* matrix;
        GSmtx* matrices;
        u32 flags;
    } ModelBoundArgs;
    extern u32 lbl_8047AB98;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void set__5GSvecFfff(GSvec* vec, f32 x, f32 y, f32 z);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecTransform(GSvec* dst, GSmtx* matrix, GSvec* src);
#endif
    extern GSbound* GSmodelGetBound(GSmodel* model);
    extern void fn_80191358(GSbound* bound, f32 x, f32 y, f32 z);
    ModelBoundArgs* args = arg;

    if (flags & 1) {
        lbl_8047AB98 = *(u8*)vertex / 3;
    }
    if (flags & 2) {
        GSmtx* matrix;
        GSvec position;

        if (args->flags & 1) {
            matrix = &args->matrices[lbl_8047AB98];
        } else {
            matrix = args->matrix;
        }
        set__5GSvecFfff(&position, GS_DL_F32((f32*)vertex + 0), GS_DL_F32((f32*)vertex + 1),
                        GS_DL_F32((f32*)vertex + 2)); /* disc vertex data: big-endian on the PC */
        GSvecTransform(&position, matrix, &position);
        fn_80191358(GSmodelGetBound(args->model), position.x, position.y,
                    position.z);
    }
}

void _modelBoundBeginSurface__F13GSgfxPrimTypeUsUlPv(s32 prim, u16 count, u32 attr, void *ctx) {
    (void)prim;
    (void)count;
    *(u32 *)((u8 *)ctx + 0xC) = attr;
}

void GSmodelEnableAnimBlend(GSmodel* model)
{
    extern void _modelSetRotateEulerToQuatAll__FP9_HSD_JObj(HSD_JObj* jobj);

    if (model->renderJObjAlt == NULL && !(model->flags.raw & 0x20000)) {
        model->renderJObjAlt = HSD_JObjLoadJoint((HSD_Joint*)DP(model->resource->joint));
        model->blendJObjA = HSD_JObjLoadJoint((HSD_Joint*)DP(model->resource->joint));
        model->blendJObjB = HSD_JObjLoadJoint((HSD_Joint*)DP(model->resource->joint));
        _modelSetRotateEulerToQuatAll__FP9_HSD_JObj(model->renderJObjAlt);
    }
}
#endif
