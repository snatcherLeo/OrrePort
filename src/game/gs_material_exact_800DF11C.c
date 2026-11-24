#include "dolphin/gx/GX.h"
#include "dolphin/types.h"
#include "game/gs_material_object.h" /* GSmaterial (a partial view was here) */

void GSmaterialGetModulate(const GSmaterial* material, GXColor* color)
{
    color->r = material->modulate[0];
    color->g = material->modulate[1];
    color->b = material->modulate[2];
    color->a = material->modulate[3];
}
