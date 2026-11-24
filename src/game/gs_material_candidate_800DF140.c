/* RULE-EXCEPTION(user-approved): constant_import (temporary) — remove when this file is merged back into one unit — see docs/RULE_EXCEPTIONS.md */
/** GSmaterialResetAlpha, 0x800DF140 - 0x800DF188. */
#include "dolphin/types.h"
#include "game/gs_material_object.h"

void GSmaterialResetAlpha(GSmaterial* obj) {
    extern void HSD_MObjSetAlpha(HSD_MObj*, f32);
    HSD_MObjSetAlpha(obj->mobj, (f32)obj->alpha / 255.0f);
}
