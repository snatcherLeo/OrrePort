#include "game/gs_render_util.h"

extern GSRenderCamera* _activeCamera;

GSRenderCamera* GScameraGetActiveCamera(void)
{
    return _activeCamera;
}
