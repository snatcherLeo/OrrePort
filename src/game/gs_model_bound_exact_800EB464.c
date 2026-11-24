/**
 * @file gs_model_bound_exact_800EB464.c
 * @brief Strict model-bound callbacks and animation-blend setup,
 *        0x800EB464 - 0x800EB5A0.
 */

#include "dolphin/types.h"
#include "hsd/hsd_pobj.h"

typedef f32 GSmtx[3][4];

#include "game/gs_model_struct.h" /* GSmodel and its helper types (a private padded copy was here) */
#include "game/gs_gfx_layer.h" /* GS_DL_F32 */

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
extern HSD_JObj* HSD_JObjLoadJoint(HSD_Joint* joint);
extern void _modelSetRotateEulerToQuatAll__FP9_HSD_JObj(HSD_JObj* jobj);

void _modelBoundVertex__FUlPvPv(u32 flags, void* vertex, void* arg)
{
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

void _modelBoundBeginSurface__F13GSgfxPrimTypeUsUlPv(
    s32 prim, u16 count, u32 attr, void* arg)
{
    ModelBoundArgs* args = arg;

    args->flags = attr;
}

void GSmodelEnableAnimBlend(GSmodel* model)
{
    if (model->renderHsdJObjAlt == NULL && !(model->flags.raw & 0x20000)) {
        model->renderHsdJObjAlt = HSD_JObjLoadJoint((HSD_Joint*)DP(model->resource->joint));
        model->blendJObjA = HSD_JObjLoadJoint((HSD_Joint*)DP(model->resource->joint));
        model->blendJObjB = HSD_JObjLoadJoint((HSD_Joint*)DP(model->resource->joint));
        _modelSetRotateEulerToQuatAll__FP9_HSD_JObj(model->renderHsdJObjAlt);
    }
}
