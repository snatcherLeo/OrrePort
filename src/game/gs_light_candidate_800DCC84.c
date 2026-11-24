/**
 * @file gs_light_candidate_800DCC84.c
 * @brief GSlight: colour/type setters, pool create/load/free/init and the
 *        per-frame light setup.
 *
 * Address range: 0x800DCC84 - 0x800DD270 (retail GSlight.cpp tail).
 */

#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#include "hsd/hsd_forward.h"
#include "hsd/hsd_aobj.h"
#include "hsd/hsd_object.h"

#include "game/gs_light.h" /* GSlight and its descriptions (one definition for every GSlight unit) */
#include "hsd/hsd_lobj.h" /* HSD_LObj (the light list is linked through its next field) */

extern void HSD_LObjSetColor(HSD_LObj* lobj, GXColor color);
extern void HSD_LObjClearFlags(HSD_LObj* lobj, u32 flags);
extern void HSD_LObjSetFlags(HSD_LObj* lobj, u32 flags);
extern HSD_LObj* HSD_LObjLoadDesc(HSD_LightDesc* desc);
extern void HSD_LObjRemoveAnimAll(HSD_LObj* lobj);
extern void HSD_LObjAddAnimAll(HSD_LObj* lobj, HSD_LightAnim* anim);
extern void HSD_LObjReqAnimAll(HSD_LObj* lobj, f32 frame);
extern void HSD_LObjDeleteCurrentAll(HSD_LObj* lobj);
extern void HSD_LObjAddCurrentAll(HSD_LObj* lobj);
extern void HSD_LObjSetup(HSD_CObj* cobj);
extern void HSD_ForeachAnim(void* obj, u32 type, u32 mask, void (*cb)(HSD_AObj*), ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* condition);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u32 handle);
#endif

extern u16 lbl_8047AAE8;       /* light pool GSmem handle */
extern GSlight* _lightStructList;  /* light pool */
extern u32 _lightStructCount;       /* light pool size */
extern f32 lbl_8047AAF4;       /* animation end frame scratch */
extern const f32 lbl_8047CA70; /* 1.0f */
extern const f32 lbl_8047CA78; /* 0.0f */
extern const f32 lbl_8047CA8C;
extern const char lbl_8047CA90[] __attribute__((section(".sdata2")));
extern const char lbl_8047CA98[] __attribute__((section(".sdata2")));

void lightGetFrameCount__FP9_HSD_AObj(HSD_AObj* aobj);

typedef struct GSlightColor {
    f32 r;
    f32 g;
    f32 b;
} GSlightColor;

void GSlightSetColor(GSlight* light, GSlightColor* rgb)
{
    GXColor color;

    /* Retail leaves the alpha byte unset. */
    color.r = rgb->r;
    color.g = rgb->g;
    color.b = rgb->b;
    HSD_LObjSetColor(light->lobj, color);
}

void GSlightSetType(GSlight* light, s32 type)
{
    HSD_LObjClearFlags(light->lobj, 3);
    switch (type) {
    case 0:
        HSD_LObjSetFlags(light->lobj, 0);
        break;
    case 1:
        HSD_LObjSetFlags(light->lobj, 1);
        break;
    case 2:
        HSD_LObjSetFlags(light->lobj, 2);
        break;
    case 3:
        HSD_LObjSetFlags(light->lobj, 3);
        break;
    }
    light->type = type;
}

void GSlightFree(GSlight* light)
{
    HSD_LObj* lobj = light->lobj;

    if (lobj != NULL && ref_DEC(lobj)) {
        hsdDelete(lobj);
    }
    light->active = 0;
    light->allocated = 0;
}

static inline GSlight* lightFindFree(void)
{
    GSlight* light = _lightStructList;
    u32 i;

    for (i = 0; i < _lightStructCount; i++, light++) {
        if (light->allocated == 0) {
            return light;
        }
    }
    return NULL;
}

/*
 * Same body as GSlightSetAnimIndex, expanded here by the target with the
 * unsigned index/count compare against the constant 0 still present.
 */
static inline void lightSetAnimIndex(GSlight* light, u32 animIndex)
{
    if (light->hasAnimation != 0) {
        HSD_LObjRemoveAnimAll(light->lobj);
        if (animIndex <= light->animCount) {
            light->animIndex = animIndex;
            HSD_LObjAddAnimAll(light->lobj, DP(DP(light->desc->animations)[light->animIndex]));
            HSD_LObjReqAnimAll(light->lobj, lbl_8047CA78);
            lbl_8047AAF4 = lbl_8047CA78;
            HSD_ForeachAnim(light->lobj, 7, 0xFFFF, lightGetFrameCount__FP9_HSD_AObj, 0);
            light->animEndFrame = lbl_8047AAF4;
        }
    }
}

GSlight* GSlightLoad(GSlightDesc* desc)
{
    GSlight* light = lightFindFree();

    if (light == NULL) {
        return NULL;
    }

    light->desc = desc;
    light->lobj = HSD_LObjLoadDesc(DP(light->desc->lobjDesc));
    light->allocated = 1;
    light->active = 0;
    light->isAnimating = 0;
    if (DP(light->desc->animations) != NULL) {
        light->hasAnimation = 1;
        light->animRate = lbl_8047CA70;
        light->animMode = 1;
        light->animEnded = 0;
        light->animCount = 0;
        while (DP(DP(light->desc->animations)[light->animCount]) != NULL) {
            light->animCount++;
        }
        lightSetAnimIndex(light, 0);
    } else {
        light->hasAnimation = 0;
    }
    return light;
}

GSlight* GSlightCreate(void)
{
    GSlight* light = lightFindFree();

    if (light == NULL) {
        return NULL;
    }

    DP_SET(light->positionDesc.className, NULL);
    light->positionDesc.pos[0] = lbl_8047CA78;
    light->positionDesc.pos[1] = lbl_8047CA78;
    light->positionDesc.pos[2] = lbl_8047CA78;
    DP_SET(light->positionDesc.robjDesc, NULL);
    DP_SET(light->interestDesc.className, NULL);
    light->interestDesc.pos[0] = lbl_8047CA78;
    light->interestDesc.pos[1] = lbl_8047CA78;
    light->interestDesc.pos[2] = lbl_8047CA78;
    DP_SET(light->interestDesc.robjDesc, NULL);
    light->shininess = lbl_8047CA8C;
    DP_SET(light->lobjDesc.className, NULL);
    DP_SET(light->lobjDesc.next, NULL);
    light->lobjDesc.flags = 4;
    light->lobjDesc.attnFlags = 0;
    light->lobjDesc.color.r = 0x80;
    light->lobjDesc.color.g = 0x80;
    light->lobjDesc.color.b = 0x80;
    light->lobjDesc.color.a = 0;
    DP_SET(light->lobjDesc.position, &light->positionDesc);
    DP_SET(light->lobjDesc.interest, &light->interestDesc);
    DP_SET(light->lobjDesc.param, &light->shininess);
    light->lobj = HSD_LObjLoadDesc((HSD_LightDesc*)&light->lobjDesc);
    light->allocated = 1;
    light->active = 0;
    return light;
}

void GSlightInit(u32 count)
{
    u32 i;

    _lightStructCount = count;
    lbl_8047AAE8 = _toolentryAlloc__FUl(count * sizeof(GSlight));
    if (lbl_8047AAE8 != 0) {
        _lightStructList = GSmemLock(lbl_8047AAE8);
        for (i = 0; i < _lightStructCount; i++) {
            _lightStructList[i].allocated = 0;
        }
    }
}

void lightGetFrameCount__FP9_HSD_AObj(HSD_AObj* aobj)
{
    if (aobj == NULL) {
        __assert(lbl_8047CA90, 0xAB, lbl_8047CA98);
    }
    lbl_8047AAF4 = lbl_8047CA70 + aobj->end_frame;
}

static inline s32 lightFindFirstActive(GSlight* light)
{
    u32 i;

    for (i = 0; i < _lightStructCount; light++, i++) {
        if (light->allocated == 1 && light->active != 0) {
            return i;
        }
    }
    return -1;
}

void GSlightSetupLights(HSD_CObj* cobj)
{
    GSlight* tail;
    s32 first;
    u32 i;

    HSD_LObjDeleteCurrentAll(NULL);

    tail = _lightStructList;
    first = lightFindFirstActive(tail);
    if (first != -1) {
        tail = &tail[first];
        for (i = first + 1; i < _lightStructCount; i++) {
            GSlight* light = &_lightStructList[i];
            if (light->allocated == 1 && light->active != 0) {
                tail->lobj->next = light->lobj;
                tail = light;
            }
        }
        tail->lobj->next = NULL;
        HSD_LObjAddCurrentAll(_lightStructList[first].lobj);
    }
    HSD_LObjSetup(cobj);
}
