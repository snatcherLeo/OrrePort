/** Residual retail-only GSmodelAttachToGSpart island at 0x800E4AC0. */
#define GSMODEL_SUFFIX_ISOLATED
#include "src/game/gs_model_main_suffix_800E4AC0.c"

typedef GSpart GSmodelAttachPart; /* include/game/gs_part.h (was a private view) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSvecCopy(GSvec* dst, const GSvec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void set__5GSvecFfff(GSvec* dst, f32 x, f32 y, f32 z);
#endif

void GSmodelAttachToGSpart(GSmodel* model, GSmodelAttachPart* part,
                           s32 attachmentType, s32 updateOnce, u8 resetTransform)
{
    u32 flags;

    if (model->transformOverride != 0) {
        return;
    }
    if (attachmentType == 0) {
        return;
    }

    model->attachmentModel = (GSmodel*)part->model;
    model->attachmentPartIndex = part->index;
    if (resetTransform == 1) {
        set__5GSvecFfff(&model->overridePosition, lbl_8047CB7C,
                    lbl_8047CB7C, lbl_8047CB7C);
        set__5GSvecFfff(&model->overrideRotation, lbl_8047CB7C,
                    lbl_8047CB7C, lbl_8047CB7C);
        set__5GSvecFfff(&model->overrideScale, lbl_8047CB80,
                    lbl_8047CB80, lbl_8047CB80);
    } else {
        GSvecCopy(&model->overridePosition, &model->position);
        GSvecCopy(&model->overrideRotation, &model->rotation);
        GSvecCopy(&model->overrideScale, &model->scale);
    }

    set__5GSvecFfff(&model->position, lbl_8047CB7C, lbl_8047CB7C,
                lbl_8047CB7C);
    set__5GSvecFfff(&model->rotation, lbl_8047CB7C, lbl_8047CB7C,
                lbl_8047CB7C);
    set__5GSvecFfff(&model->scale, lbl_8047CB80, lbl_8047CB80,
                lbl_8047CB80);
    model->transformOverride = attachmentType;

    flags = model->flags.raw | 0x80000;
    model->flags.raw = flags;
    if ((u8)updateOnce != 0) {
        flags = model->flags.raw | 0x40000;
        model->flags.raw = flags;
    }
}
