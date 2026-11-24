#include "dolphin/types.h"

#include "game/gs_model_struct.h" /* GSmodel (a private padded copy was here) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif

void modelShadowFreeModelList__FP8_GSmodel(GSmodel* model)
{
    if (model->shadowSurfaceHandle != 0) {
        fn_800E24B0(model->shadowSurfaceHandle);
        GSmemFree(model->shadowSurfaceHandle);
        model->shadowSurfaceHandle = 0;
        model->shadowSurfaceCount = 0;
        model->shadowSurfaces = NULL;
    }
}
