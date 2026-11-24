/* GS camera: load the active camera's matrices, 0x800D2248 - 0x800D2584. */
#include "dolphin/types.h"
#include "game/gs_render_util.h"
#include "game/gs_camera_build_matrix.h"
#include "hsd/hsd_cobj.h" /* HSD_CObj fields (flags, projection type, view matrix) instead of GameCube offsets */

extern u32 _activeCamera; /* active camera */
extern char lbl_8047C9C4[] __attribute__((section(".sdata2"))); /* "cobj.h" */
extern char lbl_8047C9CC[] __attribute__((section(".sdata2"))); /* "cobj" */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* msg);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void PSMTXCopy(void* src, void* dst);
#endif
extern void gfxMultMatrix(void* mtx);
extern void gfxLoadIdentity(void);
extern void gfxSetPerspective(f32 fovy, f32 aspect, f32 near, f32 far);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(f32 a, f32 b, f32 c, f32 d);
#endif

void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(void) {
    GSRenderCamera* c = (GSRenderCamera*)PC_PTR(_activeCamera);
    void* cobj;
    /* One set of four projection values for both paths: retail gives them a
     * single set of stack slots (0x14/0x10/0xC/0x8), and because the ortho
     * path takes all four addresses the perspective path stores near/far to
     * those slots as well. */
    f32 p0, p1, p2, p3;

    if (c == NULL) {
        return;
    }
    if (c->isAnimating != 0) {
        cobj = c->cobj;
        if (cobj == NULL) {
            __assert(lbl_8047C9C4, 0x1ae, lbl_8047C9CC);
        }
        ((HSD_CObj*)cobj)->flags &= ~0x2u;
        HSD_CObjGetPerspective(c->cobj, &p0, &p1);
        p2 = HSD_CObjGetNear(c->cobj);
        p3 = HSD_CObjGetFar(c->cobj);
        gfxSetPerspective(p0, p1, p2, p3);
        fn_800E0628(c->viewMtx, HSD_CObjGetViewingMtxPtr(c->cobj));
        gfxLoadIdentity();
        gfxMultMatrix(c->viewMtx);
        HSD_CObjGetEyePosition(c->cobj, (Vec*)&c->eye);
        HSD_CObjGetUpVector(c->cobj, (Vec*)&c->upVector);
        HSD_CObjGetInterest(c->cobj, (Vec*)&c->interest);
    } else if (((HSD_CObj*)c->cobj)->projection_type == 1) {
        if (c->dirty != 0) {
            cameraBuildMatrix(c);
        }
        cobj = c->cobj;
        if (cobj == NULL) {
            __assert(lbl_8047C9C4, 0x1a2, lbl_8047C9CC);
        }
        ((HSD_CObj*)cobj)->flags |= 0x80000002u;
        PSMTXCopy(c->viewMtx, ((HSD_CObj*)cobj)->view_mtx);
        HSD_CObjGetPerspective(c->cobj, &p0, &p1);
        p2 = HSD_CObjGetNear(c->cobj);
        p3 = HSD_CObjGetFar(c->cobj);
        gfxSetPerspective(p0, p1, p2, p3);
        gfxLoadIdentity();
        gfxMultMatrix(c->viewMtx);
    } else {
        HSD_CObjGetOrtho(c->cobj, &p1, &p3, &p0, &p2);
        gfxSetOrtho(p0, p1, p2, p3);
        gfxLoadIdentity();
    }
}
