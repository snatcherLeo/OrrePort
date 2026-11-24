#include "dolphin/mtx.h"
#include "game/gs_render_util.h"
#include "hsd/hsd_cobj.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(void* destination, const void* source);
#endif

void GScameraSetPosition(GSRenderCamera* camera, GSRenderVec3* position)
{
    HSD_CObjSetEyePosition((HSD_CObj*) camera->cobj, (Vec*) position);
    GSvecCopy(&camera->eye, position);
    camera->dirty = 1;
}
