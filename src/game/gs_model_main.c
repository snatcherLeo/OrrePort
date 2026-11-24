/**
 * @file gs_model_main.c
 * @brief GSmodel update prefix
 *
 * Target range 0x800E3604-0x800E3928; source owns GSmodelAdvanceAll.
 */
#include "dolphin/types.h"
#include "game/data/sdata2_8047CAA0.h"
#include "game/gs_model.h"
#include "game/gs_model_anim.h"
#include "game/gs_render.h"
#include "game/gs_render_util.h" /* GSRenderCamera: the active camera's HSD camera is its cobj field */

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

extern void HSD_StateInvalidate(u32 mask);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GScameraGetActiveCamera(void);
#endif
extern s32 HSD_CObjSetCurrent(void* camera);
extern void GSlightSetupLights(void* camera);
extern void modelShadowPrepare__FP8_GSmodelb(GSmodel* model, s32 active);
extern void HSD_JObjDispAll(HSDJObj* jobj, s32 arg1, s32 flags, s32 arg3);
extern void gfxAddDrawStats(s32 vertexCount, s32 polygonCount);
extern void HSD_CObjEndCurrent(void);
extern void _gfxSetFlagsImm(s32 mode);

void GSmodelDrawAll(s32 flags, u8 slot)
{
    void* camera;

    HSD_StateInvalidate(0x7F);
    camera = GScameraGetActiveCamera();
    if (camera != NULL && HSD_CObjSetCurrent(((GSRenderCamera*)camera)->cobj) != 0) {
        u32 i;

        GSlightSetupLights(((GSRenderCamera*)camera)->cobj);
        for (i = 0; i < lbl_8047AB78; i++) {
            GSmodel* model = &lbl_8047AB74[i];
            s32 modelFlags = model->flags.raw;

            if ((modelFlags & 1) != 0 && (modelFlags & 2) != 0 &&
                (modelFlags & 0x400400) != 0 &&
                ((modelFlags >> 9) & 1) == slot) {
                HSDJObj* jobj;

                modelShadowPrepare__FP8_GSmodelb(model, 1);
                jobj = (model->flags.raw & 0x80) ? model->renderJObjAlt
                                                 : model->renderJObj;
                if ((flags & 0x10) != 0) {
                    HSD_JObjDispAll(jobj, 0, 1, 0);
                    gfxAddDrawStats(model->vertexCount, model->polygonCount);
                }
                if ((flags & 0x1000) != 0) {
                    HSD_JObjDispAll(jobj, 0, 4, 0);
                }
                if ((flags & 0x2000) != 0) {
                    HSD_JObjDispAll(jobj, 0, 2, 0);
                }
                modelShadowPrepare__FP8_GSmodelb(model, 0);
            }
        }
        HSD_CObjEndCurrent();
    }
    _gfxSetFlagsImm(-1);
}

void GSmodelDrawModel(GSmodel* model, u32 flags)
{
    void* camera;

    if ((model->flags.raw & 1) == 0) {
        return;
    }

    HSD_StateInvalidate(0x7F);
    camera = GScameraGetActiveCamera();
    if (camera != NULL) {
        if (HSD_CObjSetCurrent(((GSRenderCamera*)camera)->cobj) != 0) {
            HSDJObj* jobj;

            GSlightSetupLights(((GSRenderCamera*)camera)->cobj);
            modelShadowPrepare__FP8_GSmodelb(model, 1);
            jobj = (model->flags.raw & 0x80) ? model->renderJObjAlt
                                             : model->renderJObj;
            if ((flags & 0x10) != 0) {
                HSD_JObjDispAll(jobj, 0, 1, 0);
                gfxAddDrawStats(model->vertexCount, model->polygonCount);
            }
            if ((flags & 0x1000) != 0) {
                HSD_JObjDispAll(jobj, 0, 4, 0);
            }
            if ((flags & 0x2000) != 0) {
                HSD_JObjDispAll(jobj, 0, 2, 0);
            }
            modelShadowPrepare__FP8_GSmodelb(model, 0);
            HSD_CObjEndCurrent();
        }
        if ((model->flags.raw & 0x200000) != 0) {
            fn_80190E60(&model->bound);
        }
    }
    _gfxSetFlagsImm(-1);
}







void GSmodelAdvanceAll(u32 frames, u8 phase)
{
    u32 i;

    i = 0;
    while (i < lbl_8047AB78) {
        GSmodel* model;

        model = &lbl_8047AB74[i];
        if (model->flags.raw & GSMODEL_FLAG_ACTIVE) {
            if (modelProcessAt60fps__FP8_GSmodel(model) == phase) {
                GSmodelAdvanceAnimation(model, (f32)frames);
            }
        }

        i++;
    }
}













































































































#define GSMODEL_JOBJ_FLUSH(jobj)                                                     \
    do {                                                                            \
        if (!((jobj)->flags & JOBJ_MTX_INDEP_SRT)) {                                \
            if ((jobj) != NULL) {                                                   \
                s32 dirty_ = 0;                                                     \
                u32 flags_;                                                         \
                if ((jobj) == NULL) {                                               \
                    __assert(lbl_8047CB9C, 0x25D, lbl_8047CBA4);                    \
                }                                                                   \
                flags_ = (jobj)->flags;                                             \
                if (!(flags_ & JOBJ_USER_DEF_MTX) && (flags_ & JOBJ_MTX_DIRTY)) {  \
                    dirty_ = 1;                                                     \
                }                                                                   \
                if (dirty_ == 0) {                                                  \
                    fn_8019D620(jobj);                                              \
                }                                                                   \
            }                                                                       \
        }                                                                           \
    } while (0)

#define GSMODEL_JOBJ_SET_POSITION(jobj, value)                      \
    do {                                                            \
        const GSvec* value_ = (value);                              \
        if ((jobj) == NULL) {                                       \
            __assert(lbl_8047CB9C, 0x3A9, lbl_8047CBA4);            \
        }                                                           \
        if (value_ == NULL) {                                       \
            __assert(lbl_8047CB9C, 0x3AA, lbl_80270E60);            \
        }                                                           \
        (jobj)->translate = *value_;                                \
        GSMODEL_JOBJ_FLUSH(jobj);                                   \
    } while (0)

#define GSMODEL_JOBJ_SET_ROTATION_COMPONENT(jobj, member, value, line, quat_line) \
    do {                                                                           \
        if ((jobj) == NULL) {                                                      \
            __assert(lbl_8047CB9C, line, lbl_8047CBA4);                            \
        }                                                                          \
        if ((jobj)->flags & JOBJ_USE_QUATERNION) {                                 \
            __assert(lbl_8047CB9C, quat_line, lbl_80270E6C);                       \
        }                                                                          \
        (jobj)->rotation.member = (value);                                         \
        GSMODEL_JOBJ_FLUSH(jobj);                                                  \
    } while (0)

#define GSMODEL_JOBJ_SET_ROTATION(jobj, value)                                  \
    do {                                                                        \
        const GSvec* value_ = (value);                                          \
        GSMODEL_JOBJ_SET_ROTATION_COMPONENT(jobj, x, value_->x, 0x2A4, 0x2A5); \
        GSMODEL_JOBJ_SET_ROTATION_COMPONENT(jobj, y, value_->y, 0x2B8, 0x2B9); \
        GSMODEL_JOBJ_SET_ROTATION_COMPONENT(jobj, z, value_->z, 0x2CC, 0x2CD); \
    } while (0)

#define GSMODEL_JOBJ_SET_SCALE(jobj, value)                       \
    do {                                                          \
        const GSvec* value_ = (value);                            \
        if ((jobj) == NULL) {                                     \
            __assert(lbl_8047CB9C, 0x316, lbl_8047CBA4);          \
        }                                                         \
        if (value_ == NULL) {                                     \
            __assert(lbl_8047CB9C, 0x317, lbl_8047CBAC);          \
        }                                                         \
        (jobj)->scale = *value_;                                  \
        GSMODEL_JOBJ_FLUSH(jobj);                                 \
    } while (0)



#undef GSMODEL_JOBJ_SET_SCALE
#undef GSMODEL_JOBJ_SET_ROTATION
#undef GSMODEL_JOBJ_SET_ROTATION_COMPONENT
#undef GSMODEL_JOBJ_SET_POSITION
#undef GSMODEL_JOBJ_FLUSH
