#include "dolphin/gx/GX.h"
#include "dolphin/types.h"
#include "game/gs_material_object.h" /* GSmaterial (a partial view was here) */
#include "hsd/hsd_tobj.h"           /* HSD_TObj (the environment map's texture object) */

void GSmaterialSetEnvMapBlendValue(GSmaterial* material, f32 blend)
{
    HSD_TObj* envTObj = material->envTObj;

    material->envBlend = blend;
    if (envTObj != 0) {
        envTObj->blending = blend;
    }
}

/* mode: 0 / 1 / 2 (TObj blend flags 1 / 5 / 6), type: 0 or not (the coordinate kind), as _matGSmatEnableEnvMapExt
 * reads them. The float comes after the integers here; on the GameCube that order is invisible (separate register
 * files), so callers must use this prototype on the PC. */
void GSmaterialSetEnvMapParams(GSmaterial* material, s32 mode, s32 type, f32 blend,
                               struct GStextureHandle* texture)
{
    material->envMode = mode;
    material->envType = type;
    material->envBlend = blend;
    material->envTexture = texture;
}

void GSmaterialSetModulate(GSmaterial* material, const GXColor* color)
{
    material->modulate[0] = color->r;
    material->modulate[1] = color->g;
    material->modulate[2] = color->b;
    material->modulate[3] = color->a;
}

void GSmaterialSetColorChannels(GSmaterial* material, u32 channel0,
                                u32 channel1, u32 channel2, u32 channel3)
{
    material->channels[0] = channel0;
    material->channels[1] = channel1;
    material->channels[2] = channel2;
    material->channels[3] = channel3;
}
