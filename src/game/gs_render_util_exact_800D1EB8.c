#include "game/gs_render_util.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(GSRenderVec3* dst, const GSRenderVec3* src);
#endif

void GScameraGetLookAt(GSRenderCamera* camera, GSRenderVec3* up,
                       GSRenderVec3* interest)
{
    GSvecCopy(up, &camera->upVector);
    GSvecCopy(interest, &camera->interest);
}

void GScameraLookAt(GSRenderCamera* camera, const GSRenderVec3* up,
                    const GSRenderVec3* interest)
{
    GSvecCopy(&camera->upVector, up);
    GSvecCopy(&camera->interest, interest);
    camera->dirty = 1;
    camera->useLookAt = 1;
}
