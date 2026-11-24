/**
 * GSmaterialDisableExtension, GSmaterialEnableExtension and
 * GSmaterialGetFlags (0x800DF248 - 0x800DF470).
 *
 * XD source unit: game/pxdvs/GSAPI/GSmaterial/GSmaterial.cpp. Standalone
 * source for this split range; text only.
 */
#include "dolphin/types.h"

#include "game/gs_material_mobj.h" /* GSmaterial, GSmatObj */
#include "hsd/hsd_tobj.h"

extern void _matGSmatEnableEnvMapExt(GSmaterial*);
extern void HSD_ImageDescFree(HSD_ImageDesc*);
extern void HSD_TObjRemove(HSD_TObj*);
extern void HSD_MObjCompileTev(HSD_MObj*);
extern u32 HSD_MObjGetFlags(HSD_MObj*);

void GSmaterialDisableExtension(GSmaterial* obj, u32 flags) {
    GSmaterial* material = obj;
    HSD_TObj* image;
    HSD_TObj* it;
    HSD_MObj* list;
    HSD_ImageDesc* desc;
    u32 bits;
    HSD_MObj* mobj;

    bits = flags & material->extensions;
    mobj = material->mobj;
    if (bits & 1) {
        material->modulate[3] = 0x7f;
        material->modulate[2] = 0x7f;
        material->modulate[1] = 0x7f;
        material->modulate[0] = 0x7f;
    }
    if (bits & 2) {
        material->channels[0] = 0;
        material->channels[1] = 1;
        material->channels[2] = 2;
        material->channels[3] = 3;
    }
    if (bits & 4) {
        list = material->mobj;
        image = material->envTObj;
        it = list->tobj;
        desc = material->envImageDesc;
        if (it != 0 && image != 0) {
            if (it == image) {
                list->tobj = image->next;
            } else {
                while (it != 0) {
                    if (it->next == image) {
                        it->next = image->next;
                    }
                    it = it->next;
                }
            }
            if (desc != 0) {
                HSD_ImageDescFree(desc);
            }
            if (image != 0) {
                HSD_TObjRemove(image);
            }
        }
        material->envTexture = 0;
    }

    material->extensions &= ~bits;
    if (material->extensions == 0) {
        ((GSmatObj*)mobj)->material = 0;
    }
    HSD_MObjCompileTev(mobj);
}

void GSmaterialEnableExtension(GSmaterial* obj, u32 flags) {
    u32 new_bits;
    HSD_MObj* ptr;
    u16 cur;
    cur = obj->extensions;
    ptr = obj->mobj;
    new_bits = flags & ~cur;
    if (!new_bits) return;
    ((GSmatObj*)ptr)->material = obj;
    if (new_bits & (1 << 2)) _matGSmatEnableEnvMapExt(obj);
    obj->extensions = obj->extensions | new_bits;
    HSD_MObjCompileTev(ptr);
}

u32 GSmaterialGetFlags(GSmaterial* obj) {
    u32 v = HSD_MObjGetFlags(obj->mobj);
    u32 flags = 0;
    if (v & 0x1) flags |= 0x1;
    if (v & 0x2) flags |= 0x2;
    if (v & 0x4) flags |= 0x4;
    if (v & 0x8) flags |= 0x8;
    if (v & 0x40000000) flags |= 0x10;
    if (v & 0x2000) flags |= 0x20;
    if (v & 0x4000) flags |= 0x40;
    return flags;
}
