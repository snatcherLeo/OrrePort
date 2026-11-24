/**
 * @file gs_model_bound_exact_800EB414.c
 * @brief Exact model-bound display-list callback, 0x800EB414 - 0x800EB464.
 */

#include "dolphin/types.h"
#include "hsd/hsd_pobj.h"

extern void GSgfxParseDisplayList(void* verts, u8* display, u32 size,
                                  void* callbacks, void* args, void* pobj);
extern void* lbl_80315598[];

void _modelBoundPObj__FP9_HSD_PObjP5GSmtxP5GSmtxP5GSmtxPv(
    HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4], f32 smtx[3][4], void* arg)
{
    HSD_PObj* obj;
    HSD_PObj* vertex_obj;
#ifdef TARGET_PC
    /* The callbacks use this as ModelBoundArgs (gs_model_bound_exact_800EB464.c): model, matrix, matrices and a flags
     * word that _modelBoundBeginSurface writes at +0xC. On the GameCube that word lands in the stack slot after the
     * three pointers; on the PC it would land past the array, so the PC gives it a slot of its own. */
    struct { void* model; void* matrix; void* matrices; u32 flags; } args;

    obj = (vertex_obj = pobj);
    args.model = arg;
    args.matrix = pmtx;
    args.matrices = smtx;
    args.flags = 0;
    GSgfxParseDisplayList(vertex_obj->verts, obj->display,
                          vertex_obj->n_display << 5, lbl_80315598, &args, obj);
#else
    void* args[3];

    obj = (vertex_obj = pobj);
    args[0] = arg;
    args[1] = pmtx;
    args[2] = smtx;
    GSgfxParseDisplayList(vertex_obj->verts, obj->display,
                          vertex_obj->n_display << 5, lbl_80315598, args, obj);
#endif
}
