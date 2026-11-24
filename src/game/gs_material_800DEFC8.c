/**
 * GSmaterialResetTexture (0x800DEFC8-0x800DF028).
 *
 * First function of the GSmaterial code: GSmaterialSetTexture, right after
 * it at 0x800DF028, keeps the material's original image descriptor at +0x38
 * (0xFEFEFEFE = nothing saved) before binding a GStexture image to the
 * MObj's TObj; this puts the saved descriptor back and frees the one
 * GSmaterialSetTexture allocated. It references no data, so it links as a
 * text-only object; the rest of the GSmaterial range stays in its own
 * candidates.
 */
#include "dolphin/types.h"
#include "hsd/hsd_mobj.h"
#include "hsd/hsd_tobj.h"
#include "game/gs_material_object.h"

#define GSMAT_NO_SAVED_IMAGE ((HSD_ImageDesc*)0xFEFEFEFE)

extern void GXDrawDone(void);

void GSmaterialResetTexture(GSmaterial* material)
{
    HSD_TObj* tobj;
    HSD_ImageDesc* bound;

    if (material->savedImageDesc == GSMAT_NO_SAVED_IMAGE) {
        return;
    }
    /* The GPU may still be reading the bound image. */
    GXDrawDone();
    tobj = material->mobj->tobj;
    bound = tobj->imagedesc;
    tobj->imagedesc = material->savedImageDesc;
    HSD_ImageDescFree(bound);
    material->savedImageDesc = GSMAT_NO_SAVED_IMAGE;
}
