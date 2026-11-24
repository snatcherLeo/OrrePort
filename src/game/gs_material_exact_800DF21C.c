#include "game/gs_material.h"
#include "game/gs_material_object.h"
#include "hsd/hsd_mobj.h"

void GSmaterialSetAlpha(GSmaterial* material, f32 alpha) /* the real 0x40-byte record (A, 2026-10-15) */
{
    HSD_MObjSetAlpha(material->mobj, alpha);
}
