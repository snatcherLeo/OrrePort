/**
 * @file gs_model_main_exact_800E3928.c
 * @brief GSmodel exact model-list, bounds, transforms, and detach island
 *
 * Exact target range 0x800E3928-0x800E4AC0; functions follow target order.
 */
#include "dolphin/types.h"
#include "game/data/sdata2_8047CAA0.h"
#include "game/gs_model.h"
#include "game/gs_model_anim.h"
#include "game/gs_render.h"

#define GSJOBJ_DESC_FLAG_INSTANCE 0x00001000U
#define JOBJ_MTX_DIRTY 0x00000040U
#define JOBJ_USE_QUATERNION 0x00020000U
#define JOBJ_USER_DEF_MTX 0x00800000U
#define JOBJ_MTX_INDEP_SRT 0x02000000U
#define JOBJ_SET_MTX_FLAGS 0x03800000U

#include "game/gs_model_struct.h" /* GSmodel and its helper types (one definition for the GSmodel units) */

extern GSmodel* lbl_8047AB74;
extern u32 lbl_8047AB78;
extern u16 lbl_8047AB70;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(GSvec* dst, const GSvec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimFrame(GSmodel* model, f32 frame);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u32 handle);
#endif
extern void modelShadowInit__Fv(void);
extern void fn_800EC134(GSmodel* model);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gsParticleKill(void* bank, u32 destroyChildren);
#endif
extern void GSmodelSetParticleVisibility(void* bank, u32 visible);
extern u32 fn_80191118(GSbound* bound);
extern u32 GSmaterialGetEnabledExtensions(void* material);
extern void GSmaterialGetModulate(void* material, GScolor* color);
extern void GSmaterialSetModulate(void* material, const GScolor* color);
extern void GSmaterialSetEnvMapBlendValue(void* material, f32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmaterialSetAlpha(void* material, f32 alpha);
#endif
extern void GSmaterialDisableExtension(void* material, u32 extension);
extern void GSmaterialResetTexture(void* material);
extern void GSmaterialResetPEdescr(void* material);
extern void GSmaterialResetAlpha(void* material);
extern void GSmaterialResetFlags(void* material);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmaterialRemoveExtraTObj(void* material);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u32 handle);
#endif
extern void modelShadowFreeModelList__FP8_GSmodel(GSmodel* model);
extern void GSmodelRemoveNull(GSmodel* model);
extern void modelRemoveCenterNull(GSmodel* model);
extern HSDJObj* fn_801A02B0(HSDJObj* jobj);
extern void HSD_JObjUnref(void* object);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int value, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif
extern void HSD_JObjSetupMatrixSub(HSDJObj* jobj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* message);
#endif
extern char lbl_8047CB60[] __attribute__((section(".sdata2")));
extern char lbl_8047CB68[] __attribute__((section(".sdata2")));
extern char lbl_8047CB70[] __attribute__((section(".sdata2")));
extern char lbl_8047CB78[] __attribute__((section(".sdata2")));
extern char lbl_8047CB9C[] __attribute__((section(".sdata2")));
extern char lbl_8047CBA4[] __attribute__((section(".sdata2")));
extern char lbl_8047CBAC[] __attribute__((section(".sdata2")));
extern char lbl_80270E6C[];
extern char lbl_80270E28[];
extern char lbl_80270E50[];
extern char lbl_80270E60[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void clear__5GSvecFv(GSvec* vec);
#endif
extern u8 modelProcessAt60fps__FP8_GSmodel(GSmodel* model);
extern void GSmodelAdvanceAnimation(GSmodel* model, f32 frames);
extern void PSMTXCopy(const f32 src[3][4], f32 dst[3][4]);
extern void fn_8019D620(HSDJObj* jobj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern GSpart* GSmodelGetPart(GSmodel* model, s32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSpartGetTransform(GSpart* part, GSvec* position, GSvec* rotation, GSvec* scale);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSpartFree(GSpart* part);
#endif
extern HSDJObj* HSD_JObjAlloc(void);
extern void HSD_JObjAddChild(HSDJObj* parent, HSDJObj* child);
extern u32 HSD_JObjGetFlags(HSDJObj* jobj);
extern void HSD_JObjSetFlags(HSDJObj* jobj, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimIndex(GSmodel* model, u32 index);
#endif
extern void GSmodelSetAnimType(GSmodel* model, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelStartAnimation(GSmodel* model);
#endif

void fn_800E85E8(GSmodel* model);

static inline void modelUpdateBoundCenter(GSmodel* model)
{
    if (model->flags.raw & GSMODEL_FLAG_RENDER_ALT_JOBJ) {
        memcpy(&model->boundCenter, &model->position, sizeof(GSvec));
    } else {
        GSpart* part;

        part = GSmodelGetPart(model, 1);
        if (part == NULL) {
            memcpy(&model->boundCenter, &model->position, sizeof(GSvec));
        } else {
            GSpartGetTransform(part, &model->boundCenter, NULL, NULL);
            GSpartFree(part);
            model->boundCenter.y -= model->boundYOffset;
        }
    }
}

void GSmodelApplyAll(void* unused)
{
    u32 i;
    GSmodel* model;

    i = 0;
    while (i < lbl_8047AB78) {
        u32 flags;

        model = &lbl_8047AB74[i];
        flags = model->flags.raw;
        if (flags & GSMODEL_FLAG_ACTIVE) {
            if (flags & GSMODEL_FLAG_VISIBLE) {
                if (flags & GSMODEL_FLAG_BOUND_CHECK) {
                    u8 boundVisible;

                    boundVisible = fn_80191118(&model->bound) != 0;
                    if (boundVisible == 1) {
                        model->flags.raw |= GSMODEL_FLAG_BOUND_VISIBLE;
                    } else {
                        model->flags.raw &= ~GSMODEL_FLAG_BOUND_VISIBLE;
                    }
                } else {
                    model->flags.raw |= GSMODEL_FLAG_BOUND_VISIBLE;
                }
            } else {
                model->flags.raw &= ~GSMODEL_FLAG_BOUND_VISIBLE;
            }
        }
        i++;
    }

    i = 0;
    while (i < lbl_8047AB78) {
        u32 flags;

        model = &lbl_8047AB74[i];
        flags = model->flags.raw;
        if ((flags & GSMODEL_FLAG_ACTIVE) &&
            (flags & (GSMODEL_FLAG_BOUND_VISIBLE |
                      GSMODEL_FLAG_PARTICLE_LINKED))) {
            modelApplyAnimation__FP8_GSmodel(model);
        }
        i++;
    }

    i = 0;
    while (i < lbl_8047AB78) {
        model = &lbl_8047AB74[i];
        if (model->flags.raw & GSMODEL_FLAG_ACTIVE) {
            u32 flags;

            modelUpdateAttachments__FP8_GSmodel(model);
            modelUpdateBoundCenter(model);

            flags = model->flags.raw;
            if ((flags & GSMODEL_FLAG_BOUND_VISIBLE) &&
                (flags & GSMODEL_FLAG_UPDATE_BOUND)) {
                fn_80190E60(&model->bound);
            }
        }
        i++;
    }
}

GSmodel* fn_800E3B08(u32 index)
{
    GSmodel* model;

    if (index >= lbl_8047AB78) {
        return NULL;
    }

    model = &lbl_8047AB74[index];
    if (model->flags.raw & GSMODEL_FLAG_ACTIVE) {
        return model;
    }

    return NULL;
}

u32 fn_800E3B3C(void)
{
    return lbl_8047AB78;
}

void fn_800E3B44(GSmodel* model, u8 enable)
{
    if (enable) {
        model->flags.raw |= GSMODEL_FLAG_PARTICLE_LINKED;
        return;
    }

    model->flags.raw &= ~GSMODEL_FLAG_PARTICLE_LINKED;
}

GSmodel* GSmodelSearchModelList(void* jobj)
{
    u32 i;
    GSmodel* model;
    u32 flags;

    i = 0;
    model = lbl_8047AB74;

    while (i < lbl_8047AB78) {
        flags = model->flags.raw;
        if (flags & GSMODEL_FLAG_ACTIVE) {
            void* renderJObj;

            if (flags & GSMODEL_FLAG_RENDER_ALT_JOBJ) {
                renderJObj = model->renderJObjAlt;
            } else {
                renderJObj = model->renderJObj;
            }

            if (renderJObj == jobj) {
                return model;
            }
        }

        model++;
        i++;
    }

    return NULL;
}

void GSmodelDestroyLinkedParticles(GSmodel* model)
{
    void* bank;

    bank = model->linkedGSparticleBank;
    if (bank != NULL) {
        gsParticleKill(bank, 1);
    }
}

u32 GSmodelGetGSparticleLinkAttachMode(GSmodel* model)
{
    return model->gsParticleLinkAttachMode;
}

void* GSmodelGetLinkedGSparticleBank(GSmodel* model)
{
    return model->linkedGSparticleBank;
}

void GSmodelSetGSparticleLinkAttachMode(GSmodel* model, u32 mode)
{
    model->gsParticleLinkAttachMode = mode;
}

void GSmodelLinkToGSparticleBank(GSmodel* model, void* bank)
{
    if (model->linkedGSparticleBank != bank) {
        model->linkedGSparticleBank = bank;
        if (model->linkedGSparticleBank != NULL) {
            GSmodelSetAnimFrame(model, model->animFrame);
            fn_800EC134(model);
        }
    }
}

GSbound* GSmodelGetBound(GSmodel* model)
{
    return &model->bound;
}

u32 fn_800E3C64(GSmodel* model)
{
    return fn_80191118(&model->bound) != 0;
}

void GSmodelSetBoundCheck(GSmodel* model, u8 enable)
{
    if (enable) {
        model->flags.raw |= GSMODEL_FLAG_BOUND_CHECK;
        return;
    }

    model->flags.raw &= ~GSMODEL_FLAG_BOUND_CHECK;
}

u32 fn_800E3CBC(GSmodel* model)
{
    return (model->flags.raw & GSMODEL_FLAG_PARTICLE_TEXSTAGE) != 0;
}

void fn_800E3CC8(GSmodel* model, u8 enable)
{
    if (enable) {
        model->flags.raw |= GSMODEL_FLAG_PARTICLE_TEXSTAGE;
        return;
    }

    model->flags.raw &= ~GSMODEL_FLAG_PARTICLE_TEXSTAGE;
}

GSvec* GSmodelGetScalePtr(GSmodel* model)
{
    return &model->scale;
}

GSvec* GSmodelGetRotationPtr(GSmodel* model)
{
    return &model->rotation;
}

GSvec* GSmodelGetPositionPtr(GSmodel* model)
{
    return &model->position;
}

u32 GSmodelGetVisibility(GSmodel* model)
{
    return (model->flags.raw & GSMODEL_FLAG_VISIBLE) != 0;
}

void fn_800E3D14(GSmodel* model, GSvec* out)
{
    GSvecCopy(out, &model->boundCenter);
}

void GSmodelGetScale(GSmodel* model, GSvec* out)
{
    GSvecCopy(out, &model->scale);
}

void GSmodelGetRotation(GSmodel* model, GSvec* out)
{
    GSvecCopy(out, &model->rotation);
}

void GSmodelGetPosition(GSmodel* model, GSvec* out)
{
    GSvecCopy(out, &model->position);
}

void fn_800E3DC4(GSmodel* model, const GSvec* rotation)
{
    GSvec newRotation;
    HSDJObj* jobj;
    f32 value;

    GSvecCopy(&newRotation, &model->rotation);
    GSvecAdd(&newRotation, &newRotation, rotation);

    if (model->transformOverride == 0) {
        GSvecCopy(&model->rotation, &newRotation);

        jobj = model->renderJObj;
        value = newRotation.x;
        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x2A4, lbl_8047CB68);
        }

        if (jobj->flags & JOBJ_USE_QUATERNION) {
            __assert(lbl_8047CB60, 0x2A5, lbl_80270E28);
        }

        jobj->rotation.x = value;
        if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
            if (jobj != NULL) {
                s32 dirty;
                u32 flags;

                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = jobj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(jobj);
                }
            }
        }

        jobj = model->renderJObj;
        value = newRotation.y;
        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x2B8, lbl_8047CB68);
        }

        if (jobj->flags & JOBJ_USE_QUATERNION) {
            __assert(lbl_8047CB60, 0x2B9, lbl_80270E28);
        }

        jobj->rotation.y = value;
        if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
            if (jobj != NULL) {
                s32 dirty;
                u32 flags;

                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = jobj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(jobj);
                }
            }
        }

        {
            HSDJObj* zJObj;

            zJObj = model->renderJObj;
            value = newRotation.z;
            if (zJObj == NULL) {
                __assert(lbl_8047CB60, 0x2CC, lbl_8047CB68);
            }

            if (zJObj->flags & JOBJ_USE_QUATERNION) {
                __assert(lbl_8047CB60, 0x2CD, lbl_80270E28);
            }

            zJObj->rotation.z = value;
            if (!(zJObj->flags & JOBJ_MTX_INDEP_SRT)) {
                if (zJObj != NULL) {
                    s32 dirty;
                    u32 flags;

                    if (zJObj == NULL) {
                        __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                    }

                    flags = zJObj->flags;
                    dirty = 0;
                    if (!(flags & JOBJ_USER_DEF_MTX)) {
                        if (flags & JOBJ_MTX_DIRTY) {
                            dirty = 1;
                        }
                    }

                    if (dirty == 0) {
                        fn_8019D620(zJObj);
                    }
                }
            }
        }
    } else {
        GSvecCopy(&model->overrideRotation, &newRotation);
    }
}

void GSmodelSetVisibility(GSmodel* model, u8 visible)
{
    if (visible) {
        model->flags.raw |= GSMODEL_FLAG_VISIBLE;
        if (model->linkedGSparticleBank != NULL) {
            GSmodelSetParticleVisibility(model->linkedGSparticleBank, 1);
        }
    } else {
        model->flags.raw &= ~GSMODEL_FLAG_VISIBLE;
        if (model->linkedGSparticleBank != NULL) {
            GSmodelSetParticleVisibility(model->linkedGSparticleBank, 0);
        }
    }
}

void GSmodelSetScale(GSmodel* model, GSvec* scale)
{
    if (model->transformOverride == 0) {
        HSDJObj* jobj;

        GSvecCopy(&model->scale, scale);

        jobj = model->renderJObj;
        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x316, lbl_8047CB68);
        }

        if (scale == NULL) {
            __assert(lbl_8047CB60, 0x317, lbl_8047CB70);
        }

        jobj->scale = *scale;

        if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
            if (jobj != NULL) {
                s32 dirty;
                u32 flags;

                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = jobj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(jobj);
                }
            }
        }
    } else {
        GSvecCopy(&model->overrideScale, scale);
    }
}

void GSmodelSetRotation(GSmodel* model, GSvec* rotation)
{
    HSDJObj* jobj;
    f32 value;

    if (model->transformOverride == 0) {
        GSvecCopy(&model->rotation, rotation);

        jobj = model->renderJObj;
        value = rotation->x;
        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x2A4, lbl_8047CB68);
        }

        if (jobj->flags & JOBJ_USE_QUATERNION) {
            __assert(lbl_8047CB60, 0x2A5, lbl_80270E28);
        }

        jobj->rotation.x = value;
        if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
            if (jobj != NULL) {
                s32 dirty;
                u32 flags;

                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = jobj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(jobj);
                }
            }
        }

        jobj = model->renderJObj;
        value = rotation->y;
        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x2B8, lbl_8047CB68);
        }

        if (jobj->flags & JOBJ_USE_QUATERNION) {
            __assert(lbl_8047CB60, 0x2B9, lbl_80270E28);
        }

        jobj->rotation.y = value;
        if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
            if (jobj != NULL) {
                s32 dirty;
                u32 flags;

                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = jobj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(jobj);
                }
            }
        }

        {
            HSDJObj* zJObj;

            zJObj = model->renderJObj;
            value = rotation->z;
            if (zJObj == NULL) {
                __assert(lbl_8047CB60, 0x2CC, lbl_8047CB68);
            }

            if (zJObj->flags & JOBJ_USE_QUATERNION) {
                __assert(lbl_8047CB60, 0x2CD, lbl_80270E28);
            }

            zJObj->rotation.z = value;
            if (!(zJObj->flags & JOBJ_MTX_INDEP_SRT)) {
                if (zJObj != NULL) {
                s32 dirty;
                u32 flags;

                if (zJObj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = zJObj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(zJObj);
                }
            }
        }
        }
    } else {
        GSvecCopy(&model->overrideRotation, rotation);
    }
}

void GSmodelSetPosition(GSmodel* model, GSvec* position)
{
    if (model->transformOverride == 0) {
        HSDJObj* jobj;

        GSvecCopy(&model->position, position);

        jobj = model->renderJObj;
        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x3A9, lbl_8047CB68);
        }

        if (position == NULL) {
            __assert(lbl_8047CB60, 0x3AA, lbl_80270E50);
        }

        jobj->translate = *position;

        if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
            if (jobj != NULL) {
                s32 dirty;
                u32 flags;

                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                }

                flags = jobj->flags;
                dirty = 0;
                if (!(flags & JOBJ_USER_DEF_MTX)) {
                    if (flags & JOBJ_MTX_DIRTY) {
                        dirty = 1;
                    }
                }

                if (dirty == 0) {
                    fn_8019D620(jobj);
                }
            }
        }
    } else {
        GSvecCopy(&model->overridePosition, position);
    }

    if (model->flags.raw & GSMODEL_FLAG_RENDER_ALT_JOBJ) {
        memcpy(&model->boundCenter, &model->position, sizeof(GSvec));
    } else {
        GSpart* part;

        part = GSmodelGetPart(model, 1);
        if (part == NULL) {
            memcpy(&model->boundCenter, &model->position, sizeof(GSvec));
        } else {
            GSpartGetTransform(part, &model->boundCenter, NULL, NULL);
            GSpartFree(part);
            model->boundCenter.y -= model->boundYOffset;
        }
    }
}

void* GSmodelGetMatrixPtr(GSmodel* model)
{
    HSDJObj* jobj;

    jobj = model->renderJObj;
    if (jobj != NULL) {
        s32 dirty;
        u32 flags;

        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
        }

        flags = jobj->flags;
        dirty = 0;
        if (!(flags & JOBJ_USER_DEF_MTX)) {
            if (flags & JOBJ_MTX_DIRTY) {
                dirty = 1;
            }
        }

        if (dirty != 0) {
            HSD_JObjSetupMatrixSub(jobj);
        }
    }

    return &model->renderJObj->matrix;
}

void GSmodelSetMatrix(GSmodel* model, const f32 matrix[3][4])
{
    HSDJObj* jobj;

    jobj = model->renderJObj;
    if (jobj == NULL) {
        __assert(lbl_8047CB60, 0x495, lbl_8047CB68);
    }

    if (matrix == NULL) {
        __assert(lbl_8047CB60, 0x496, lbl_8047CB78);
    }

    PSMTXCopy(matrix, jobj->matrix);
    jobj->flags |= JOBJ_SET_MTX_FLAGS;

    if (jobj != NULL) {
        s32 dirty;
        u32 flags;

        if (jobj == NULL) {
            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
        }

        flags = jobj->flags;
        dirty = 0;
        if (!(flags & JOBJ_USER_DEF_MTX)) {
            if (flags & JOBJ_MTX_DIRTY) {
                dirty = 1;
            }
        }

        if (dirty == 0) {
            fn_8019D620(jobj);
        }
    }
}

void GSmodelDetachFromGSpart(GSmodel* model, u8 applyTransform)
{
    HSDJObj* jobj;
    f32 value;

    if (model->transformOverride != 0) {
        model->attachmentModel = NULL;
        model->attachmentPartIndex = 0;
        model->flags.raw &= ~0xC0000U;
        model->transformOverride = 0;

        if (applyTransform != 0) {
            if (model->transformOverride == 0) {
                GSvecCopy(&model->position, &model->overridePosition);

                jobj = model->renderJObj;
                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x3A9, lbl_8047CB68);
                }
                if (&model->overridePosition == NULL) {
                    __assert(lbl_8047CB60, 0x3AA, lbl_80270E50);
                }
                jobj->translate = model->overridePosition;
                if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
                    if (jobj != NULL) {
                        s32 dirty;
                        u32 flags;

                        if (jobj == NULL) {
                            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                        }
                        flags = jobj->flags;
                        dirty = 0;
                        if (!(flags & JOBJ_USER_DEF_MTX) &&
                            (flags & JOBJ_MTX_DIRTY)) {
                            dirty = 1;
                        }
                        if (dirty == 0) {
                            fn_8019D620(jobj);
                        }
                    }
                }
            } else {
                GSvecCopy(&model->overridePosition, &model->overridePosition);
            }

            if (model->flags.raw & GSMODEL_FLAG_RENDER_ALT_JOBJ) {
                memcpy(&model->boundCenter, &model->position, sizeof(GSvec));
            } else {
                GSpart* part;

                part = GSmodelGetPart(model, 1);
                if (part == NULL) {
                    memcpy(&model->boundCenter, &model->position,
                           sizeof(GSvec));
                } else {
                    GSpartGetTransform(part, &model->boundCenter, NULL, NULL);
                    GSpartFree(part);
                    model->boundCenter.y -= model->boundYOffset;
                }
            }

            if (model->transformOverride == 0) {
                GSvecCopy(&model->rotation, &model->overrideRotation);

                jobj = model->renderJObj;
                value = model->overrideRotation.x;
                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x2A4, lbl_8047CB68);
                }
                if (jobj->flags & JOBJ_USE_QUATERNION) {
                    __assert(lbl_8047CB60, 0x2A5, lbl_80270E28);
                }
                jobj->rotation.x = value;
                if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
                    if (jobj != NULL) {
                        s32 dirty;
                        u32 flags;

                        if (jobj == NULL) {
                            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                        }
                        flags = jobj->flags;
                        dirty = 0;
                        if (!(flags & JOBJ_USER_DEF_MTX) &&
                            (flags & JOBJ_MTX_DIRTY)) {
                            dirty = 1;
                        }
                        if (dirty == 0) {
                            fn_8019D620(jobj);
                        }
                    }
                }

                jobj = model->renderJObj;
                value = model->overrideRotation.y;
                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x2B8, lbl_8047CB68);
                }
                if (jobj->flags & JOBJ_USE_QUATERNION) {
                    __assert(lbl_8047CB60, 0x2B9, lbl_80270E28);
                }
                jobj->rotation.y = value;
                if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
                    if (jobj != NULL) {
                        s32 dirty;
                        u32 flags;

                        if (jobj == NULL) {
                            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                        }
                        flags = jobj->flags;
                        dirty = 0;
                        if (!(flags & JOBJ_USER_DEF_MTX) &&
                            (flags & JOBJ_MTX_DIRTY)) {
                            dirty = 1;
                        }
                        if (dirty == 0) {
                            fn_8019D620(jobj);
                        }
                    }
                }

                jobj = model->renderJObj;
                value = model->overrideRotation.z;
                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x2CC, lbl_8047CB68);
                }
                if (jobj->flags & JOBJ_USE_QUATERNION) {
                    __assert(lbl_8047CB60, 0x2CD, lbl_80270E28);
                }
                jobj->rotation.z = value;
                if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
                    if (jobj != NULL) {
                        s32 dirty;
                        u32 flags;

                        if (jobj == NULL) {
                            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                        }
                        flags = jobj->flags;
                        dirty = 0;
                        if (!(flags & JOBJ_USER_DEF_MTX) &&
                            (flags & JOBJ_MTX_DIRTY)) {
                            dirty = 1;
                        }
                        if (dirty == 0) {
                            fn_8019D620(jobj);
                        }
                    }
                }
            } else {
                GSvecCopy(&model->overrideRotation, &model->overrideRotation);
            }

            if (model->transformOverride == 0) {
                GSvecCopy(&model->scale, &model->overrideScale);

                jobj = model->renderJObj;
                if (jobj == NULL) {
                    __assert(lbl_8047CB60, 0x316, lbl_8047CB68);
                }
                if (&model->overrideScale == NULL) {
                    __assert(lbl_8047CB60, 0x317, lbl_8047CB70);
                }
                jobj->scale = model->overrideScale;
                if (!(jobj->flags & JOBJ_MTX_INDEP_SRT)) {
                    if (jobj != NULL) {
                        s32 dirty;
                        u32 flags;

                        if (jobj == NULL) {
                            __assert(lbl_8047CB60, 0x25D, lbl_8047CB68);
                        }
                        flags = jobj->flags;
                        dirty = 0;
                        if (!(flags & JOBJ_USER_DEF_MTX) &&
                            (flags & JOBJ_MTX_DIRTY)) {
                            dirty = 1;
                        }
                        if (dirty == 0) {
                            fn_8019D620(jobj);
                        }
                    }
                }
            } else {
                GSvecCopy(&model->overrideScale, &model->overrideScale);
            }
        }
    }
}
