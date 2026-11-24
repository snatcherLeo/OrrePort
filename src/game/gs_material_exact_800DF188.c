#include "dolphin/types.h"
#include "hsd/hsd_mobj.h"
#include "game/gs_material_object.h"

extern f32 lbl_8047CACC;

/* RULE-EXCEPTION(user-approved): local scheduling control -- see docs/RULE_EXCEPTIONS.md. */
#pragma scheduling off
void GSmaterialStoreAlpha(GSmaterial* obj)
{
    f32 scale = lbl_8047CACC;

    obj->alpha =
        (u8)(s32)(scale * obj->mobj->mat->alpha);
}
