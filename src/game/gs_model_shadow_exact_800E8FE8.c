#include "dolphin/types.h"
#include "game/gs_model.h"

#include "game/gs_light.h" /* GSlight (a private copy was here) */
#include "hsd/hsd_lobj.h"

#include "game/gs_model_struct.h" /* GSmodel (a private padded copy was here) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif

void GSmodelSetShadowLight(GSmodel* model, GSlight* light)
{
    if (light != NULL && (light->lobj->flags & 0x3) == 0) {
        light = NULL;
    }
    model->shadowLight = light;
}

void GSmodelSetShadowSurface(GSmodel* model, s32 count, const void* data)
{
    if (count != model->shadowSurfaceCount) {
        if (model->shadowSurfaceHandle != 0) {
            fn_800E24B0(model->shadowSurfaceHandle);
            GSmemFree(model->shadowSurfaceHandle);
            model->shadowSurfaceHandle = 0;
            model->shadowSurfaceCount = 0;
            model->shadowSurfaces = NULL;
        }
        if (count == 0 || data == NULL) {
            return;
        }
        model->shadowSurfaceCount = count;
        model->shadowSurfaceHandle =
            _toolentryAlloc__FUl(model->shadowSurfaceCount * 4);
        model->shadowSurfaces = GSmemLock(model->shadowSurfaceHandle);
    }
    memcpy(model->shadowSurfaces, data, model->shadowSurfaceCount * 4);
}
