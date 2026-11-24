#include "dolphin/types.h"
#include "game/gs_light.h" /* GSlight fields instead of offsets */

extern void HSD_LObjGetPosition(void* lobj, void* position);
extern void HSD_LObjGetInterest(void* lobj, void* interest);
extern void HSD_LObjRemoveAnimAll(void* lobj);
extern void HSD_LObjAddAnimAll(void* lobj, void* animation);
extern void HSD_LObjReqAnimAll(void* lobj, f32 frame);
extern void HSD_ForeachAnim(void* object, u32 type, u32 mask, void* function,
                            u32 argumentType, ...);
extern void HSD_AObjSetRate(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_800D37CC(void);
#endif
extern void lightGetFrameCount__FP9_HSD_AObj(u8* aobj);

extern f32 lbl_8047CA78;
extern f32 lbl_8047AAF4;
extern f32 lbl_8047CA88;

void GSlightPushState(u8* src, u8* dst)
{
    dst[0] = src[1];
    dst[1] = src[3];
    HSD_LObjGetPosition(((GSlight*)src)->lobj, dst + 4);
    HSD_LObjGetInterest(((GSlight*)src)->lobj, dst + 0x10);
    *(u32*)(dst + 0x1c) = ((GSlight*)src)->animIndex;
    *(f32*)(dst + 0x20) = ((GSlight*)src)->animFrame;
    *(f32*)(dst + 0x24) = ((GSlight*)src)->animRate;
    *(u32*)(dst + 0x28) = ((GSlight*)src)->animMode;
    dst[2] = ((GSlight*)src)->animEnded;
    dst[3] = ((GSlight*)src)->animDirection;
}

u8 GSlightHasAnimationEnded(u8* object)
{
    return ((GSlight*)object)->animEnded;
}

void GSlightStopAnimation(u8* object)
{
    object[0x3] = 0;
}

void GSlightStartAnimation(u8* object)
{
    if (!object[0x2]) {
        return;
    }
    object[0x3] = 1;
    ((GSlight*)object)->animEnded = 0;
    ((GSlight*)object)->animDirection = 1;
}

void GSlightSetAnimType(u8* object, u32 type)
{
    ((GSlight*)object)->animMode = type;
}

void GSlightSetAnimFrame(u8* object, f32 frame)
{
    if (object[0x2]) {
        ((GSlight*)object)->animFrame = frame;
    }
}

void GSlightSetAnimRate(u8* object, f32 rate)
{
    if (!object[0x2]) {
        return;
    }
    if (fn_800D37CC() == 0x32) {
        rate *= lbl_8047CA88;
    }
    ((GSlight*)object)->animRate = rate;
    HSD_ForeachAnim(((GSlight*)object)->lobj, 7, 0xFFFF,
                    (void*)HSD_AObjSetRate, 1, ((GSlight*)object)->animRate);
}

void GSlightSetAnimIndex(u8* object, u32 frame)
{
    u32 data;
    u32 currentFrame;
    u32 frames;

    if (!object[0x2]) {
        return;
    }
    HSD_LObjRemoveAnimAll(((GSlight*)object)->lobj);
    if (frame > ((GSlight*)object)->animCount) {
        return;
    }
    ((GSlight*)object)->animIndex = frame;
    data = PC_PTR32(((GSlight*)object)->desc);
    currentFrame = ((GSlight*)object)->animIndex;
    frames = PC_PTR32(DP(((GSlightDesc*)PC_PTR(data))->animations));
    HSD_LObjAddAnimAll(((GSlight*)object)->lobj,
                       DP(((GSlightAnimSlot*)PC_PTR(frames))[currentFrame]));
    HSD_LObjReqAnimAll(((GSlight*)object)->lobj, lbl_8047CA78);
    lbl_8047AAF4 = lbl_8047CA78;
    HSD_ForeachAnim(((GSlight*)object)->lobj, 7, 0xFFFF,
                    (void*)lightGetFrameCount__FP9_HSD_AObj, 0);
    ((GSlight*)object)->animEndFrame = lbl_8047AAF4;
}

u8 GSlightCanAnimate(u8* object)
{
    return object[0x2];
}

void GSlightSetActive(u8* object, u8 active)
{
    object[0x1] = active;
}
