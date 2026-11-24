#include "dolphin/types.h"
#include "game/gs_material_object.h" /* GSmaterial (a partial view was here) */

u16 GSmaterialGetEnabledExtensions(const GSmaterial* material)
{
    return material->extensions;
}
