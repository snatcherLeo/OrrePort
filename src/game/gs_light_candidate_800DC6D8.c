/*
 * GS light animation step, 0x800DC6D8 - 0x800DC874 (XD _lightPreUpdate
 * area). Advances each active light's animation frame and applies its
 * end-of-range mode. The unit owns the (f32)delta conversion's unsigned
 * 0x4330 bias at 0x8047CA80, which only this function uses.
 */
#include "dolphin/types.h"
#include "game/gs_light.h" /* GSlight fields instead of offsets */

extern u32 _lightStructList; /* light array */
extern u32 _lightStructCount; /* light count */
extern f32 lbl_8047CA70;
extern f32 lbl_8047CA74;
extern f32 lbl_8047CA78;
extern void HSD_LObjReqAnimAll(void* lobj, f32 frame);
extern void HSD_LObjAnimAll(void* lobj);

void GSlightUpdate(u32 delta) {
    u32 offset;
    u32 i;
    u8* obj;
    s32 animType;
    f32 step;
    f32 limit;
    f32 end;

    offset = 0;
    for (i = 0; i < _lightStructCount; offset += sizeof(GSlight), i++) {
        obj = (u8*)PC_PTR(_lightStructList) + offset;
        if (obj[0] != 1 || obj[3] != 1) {
            continue;
        }

        HSD_LObjReqAnimAll(((GSlight*)obj)->lobj, ((GSlight*)obj)->animFrame);
        HSD_LObjAnimAll(((GSlight*)obj)->lobj);

        step = ((GSlight*)obj)->animRate * (f32)delta;
        limit = ((GSlight*)obj)->animEndFrame - lbl_8047CA70;
        animType = ((GSlight*)obj)->animMode;
        if ((s8)((GSlight*)obj)->animDirection == -1) {
            ((GSlight*)obj)->animFrame -= step;
        } else if ((s8)((GSlight*)obj)->animDirection == 1) {
            ((GSlight*)obj)->animFrame += step;
        }

        switch (animType) {
            case 0:
                if (((GSlight*)obj)->animFrame >= (end = limit - lbl_8047CA74)) {
                    ((GSlight*)obj)->animEnded = 1;
                    ((GSlight*)obj)->animDirection = 0;
                    ((GSlight*)obj)->animFrame = end;
                }
                break;
            case 1:
                if (((GSlight*)obj)->animFrame >= limit) {
                    ((GSlight*)obj)->animFrame -= limit;
                }
                break;
            case 2:
                if (((GSlight*)obj)->animFrame >= limit) {
                    ((GSlight*)obj)->animDirection = -1;
                } else if (((GSlight*)obj)->animFrame <= lbl_8047CA78) {
                    ((GSlight*)obj)->animDirection = 1;
                }
                break;
        }
    }
}
