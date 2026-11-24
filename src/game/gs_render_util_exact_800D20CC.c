#include "dolphin/types.h"
#include "game/gs_render_util.h"
#include "hsd/hsd_cobj.h"

void GScameraSetPerspective(GSRenderCamera* camera, f32 fov, f32 aspect,
                            f32 near, f32 far)
{
#ifdef TARGET_PC
    {
        /* PC (observe only, 2026-11-03, port-gaps 52): ORRE_CAMPROJ_LOG=A-B logs each perspective set in game
         * frames A..B (camera, fov, aspect, near, far, caller), to see whether the view distance after a battle
         * camera cut is too short */
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        extern volatile u32 pc_vi_heartbeat __attribute__((weak));
        static int on = -1;
        static unsigned lo, hi;
        const unsigned f = &pc_vi_heartbeat != NULL ? (unsigned)pc_vi_heartbeat : 0u;
        if (on < 0) {
            const char* env = getenv("ORRE_CAMPROJ_LOG");
            on = env != NULL && sscanf(env, "%u-%u", &lo, &hi) == 2;
        }
        if (on && f >= lo && f <= hi) {
            OSReport("[camproj] frame %u camera %p fov %.3f aspect %.3f near %.3f far %.3f caller %p\n", f,
                     (void*)camera, (double)fov, (double)aspect, (double)near, (double)far,
                     __builtin_return_address(0));
        }
    }
#endif
    HSD_CObjSetProjectionType((HSD_CObj*) camera->cobj, 1);
    HSD_CObjSetPerspective((HSD_CObj*) camera->cobj, fov, aspect);
    HSD_CObjSetNear((HSD_CObj*) camera->cobj, near);
    HSD_CObjSetFar((HSD_CObj*) camera->cobj, far);
    camera->dirty = 1;
}
