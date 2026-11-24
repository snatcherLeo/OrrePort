/*
 * GSgfx: bind a texture to a texture slot, 0x800D85D4 - 0x800D87AC.
 *
 * Written from the retail code: the LOD call passes 0.0f as both minLod and
 * lodBias, and each filter case is an if/else with no default. The 0.0f and
 * the signed int-to-float bias are this unit's own literal pool (.sdata2
 * 0x8047CA40-0x8047CA50; no other function uses it). As literals, MWCC
 * schedules the final call's argument setup as retail does; extern
 * variable loads cannot be moved across the stack store. Links as Matching.
 */
#include "game/gs_texture.h" /* GStextureHandle; includes dolphin/types.h */
#include "game/gs_gfx_layer.h"

extern u8 lbl_804001F0[];
extern u8 lbl_803144F0[];
extern u8 lbl_80314510[];
extern u8 lbl_80314530[];

extern void gfxRecordCommand(u32, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXInitTexObjWrapMode(void* texObj, u32 wrapS, u32 wrapT);     /* GXInitTexObjWrapMode */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXInitTexObjLOD(void* texObj, u32 minFilt, u32 magFilt, f32 minLod, f32 maxLod,
                        f32 lodBias, u8 biasClamp, u8 edgeLod, u32 maxAniso); /* GXInitTexObjLOD */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXLoadTlut(void* tlutObj, u32 tlutName);           /* GXLoadTlut */
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadTexObj(void* texObj, u32 id);
#endif

void gfxLoadTexture(s32 slot, void* model) {
    GStextureHandle* tex;
    u32 minFilt;

    if (model == NULL) {
        return;
    }
    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x26, 2, slot, PC_PTR32(model));
        return;
    }
    if (gsgfxState->boundTexture[slot] == model) {
        return;
    }

    tex = (GStextureHandle*)model;
    if (tex->dirty != 0) {
        GXInitTexObjWrapMode(GS_TEX_OBJ(tex), ((u32*)lbl_80314530)[tex->wrapS],
                    ((u32*)lbl_80314530)[tex->wrapT]);
        switch ((s32)tex->mipFilter) {
        case 0:
            if ((s32)tex->minFilter == 2) {
                minFilt = 1;
            } else {
                minFilt = 0;
            }
            break;
        case 1:
            if ((s32)tex->minFilter == 2) {
                minFilt = 3;
            } else {
                minFilt = 2;
            }
            break;
        case 2:
            if ((s32)tex->minFilter == 2) {
                minFilt = 5;
            } else {
                minFilt = 4;
            }
            break;
        }
        GXInitTexObjLOD(GS_TEX_OBJ(tex), minFilt, (s32)tex->magFilter == 2, 0.0f,
                    (f32)(tex->mipLevels - 1), 0.0f, 0, 0, 0);
        tex->dirty = 0;
    }

    gsgfxState->boundTexture[slot] = model;
    if (P32(tex->tlutData) != NULL) {
        GXLoadTlut(GS_TLUT_OBJ(tex), ((u32*)lbl_80314510)[slot]);
    }
    GXLoadTexObj((void*)(GS_TEX_OBJ(tex)), ((u32*)lbl_803144F0)[slot]);
    *(u32*)(lbl_804001F0 + 0x24) += 1;
}
