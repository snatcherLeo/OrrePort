/**
 * @file gs_material.c
 * @brief GSmaterialSetTexture, 0x800DF028 - 0x800DF11C.
 *
 * XD source unit: game/pxdvs/GSAPI/GSmaterial/GSmaterial.cpp. Standalone
 * source for this split range; the rest of the material TU (research copy
 * used by the candidate wrappers) lives in gs_material_range_800DF028.c.
 */

#include "dolphin/types.h"
#include "hsd/hsd_mobj.h" /* HSD_MObj -> tobj -> imagedesc, and the HSD_ImageDesc fields (a disc layout) */
#include "hsd/hsd_tobj.h"
#include "game/gs_material_object.h" /* GSmaterial: obj is one (mobj at +0x8, the saved image description at +0x38) */

extern void* GStextureLockImage(void* texture, u32 wait);
extern u32 GStextureUnlockImage(void* texture);
extern u16 GStextureGetXsize(void* texture);
extern u16 GStextureGetYsize(void* texture);
extern void* GStextureGetGXformat(void* texture, u32 kind);
extern u8 GStextureGetMiplevels(void* texture);
extern void GXDrawDone(void);

extern f32 lbl_8047CAC8;

/* Point the material's first texture object at `image`, allocating the
 * HSD_ImageDesc on first use. */
void GSmaterialSetTexture(u8* obj, void* image) {
    void* desc;
    u32 transparent;
    f32 scale;

    if ((PC_PTR32(((GSmaterial*)obj)->savedImageDesc) + 0x01020000) == 0xfefe) { /* GSMATERIAL_UNSAVED */
        ((GSmaterial*)obj)->savedImageDesc = ((GSmaterial*)obj)->mobj->tobj->imagedesc;
        desc = HSD_ImageDescAlloc();
    } else {
        desc = ((GSmaterial*)obj)->mobj->tobj->imagedesc;
    }

    if (desc != 0) {
        DP_SET(((HSD_ImageDesc*)desc)->image_ptr, GStextureLockImage(image, 0));
        ((HSD_ImageDesc*)desc)->width = GStextureGetXsize(image);
        ((HSD_ImageDesc*)desc)->height = GStextureGetYsize(image);
        ((HSD_ImageDesc*)desc)->format = PC_PTR32(GStextureGetGXformat(image, 1)); /* declared void* */
        transparent = GStextureGetMiplevels(image);
        scale = lbl_8047CAC8;
        ((HSD_ImageDesc*)desc)->mipmap = ((0 - transparent) | transparent) >> 31;
        ((HSD_ImageDesc*)desc)->minLOD = scale;
        ((HSD_ImageDesc*)desc)->maxLOD = scale;
        GStextureUnlockImage(image);
        GXDrawDone();
        ((GSmaterial*)obj)->mobj->tobj->imagedesc = desc;
    }
}
