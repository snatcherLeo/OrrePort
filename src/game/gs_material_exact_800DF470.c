#include "dolphin/types.h"
#include "game/gs_material_object.h" /* GSmaterial (a partial view was here) */
#include "hsd/hsd_mobj.h"           /* HSD_MObj::pe */

void GSmaterialResetPEdescr(GSmaterial* material)
{
    u32 descriptor = PC_PTR32(material->savedPEDesc);

    if (descriptor + 0x01020000 == 0xFEFE) { /* GSMATERIAL_UNSAVED */
        return;
    }
    material->mobj->pe = (HSD_PEDesc*)PC_PTR(descriptor);
    material->savedPEDesc = GSMATERIAL_UNSAVED;
}
