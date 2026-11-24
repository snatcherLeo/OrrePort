#include "dolphin/types.h"
#include "game/gs_render_util.h"
#include "hsd/hsd_object.h"

extern GSRenderCamera* _activeCamera;

void GScameraFree(GSRenderCamera* camera)
{
    void* cobj;

    if (camera == _activeCamera) {
        _activeCamera = NULL;
    }
    cobj = camera->cobj;
    if (cobj != NULL && ref_DEC(cobj)) {
        hsdDelete(cobj);
    }
    camera->active = 0;
}
