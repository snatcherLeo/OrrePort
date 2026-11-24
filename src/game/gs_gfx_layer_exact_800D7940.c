#include "game/gs_gfx_layer.h" /* GSVtxDesc; includes dolphin/types.h */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u16 count);
#endif
extern void gfxVtxPos1x16(u16 vertex);
extern void gfxVtxPos1x8(u8 vertex);
extern void gfxVtxNormal1x16(u16 vertex);
extern void gfxVtxNormal1x8(u8 vertex);
extern void gfxVtxColor1x16(u32 layer, u16 vertex);
extern void gfxVtxColor1x8(u32 layer, u8 vertex);
extern void gfxVtxTexCoord1x16(u32 layer, u16 vertex);
extern void gfxVtxTexCoord1x8(u32 layer, u8 vertex);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif

void fn_800D7940(GSVtxDesc* desc, u16 vertexCount)
{
    u16 vertex;
    s32 i;
    u32 layer;
    GSVtxAttr* entry;

    gfxBegin(vertexCount);
    for (vertex = 0; vertex < vertexCount; vertex++) {
        if ((s32)desc->attr[1].type == 2) {
            gfxVtxPos1x16(vertex);
        } else {
            gfxVtxPos1x8(vertex);
        }

        if (desc->attr[2].enabled == 1) {
            if ((s32)desc->attr[2].type == 2) {
                gfxVtxNormal1x16(vertex);
            } else {
                gfxVtxNormal1x8(vertex);
            }
        }

        for (i = 4; i <= 5; i++) {
            entry = &desc->attr[i];
            if (entry->enabled == 1) {
                layer = i - 4;
                if ((s32)entry->type == 2) {
                    gfxVtxColor1x16(layer, vertex);
                } else {
                    gfxVtxColor1x8(layer, vertex);
                }
            }
        }

        for (i = 6; i <= 13; i++) {
            entry = &desc->attr[i];
            if (entry->enabled == 1) {
                layer = i - 6;
                if ((s32)entry->type == 2) {
                    gfxVtxTexCoord1x16(layer, vertex);
                } else {
                    gfxVtxTexCoord1x8(layer, vertex);
                }
            }
        }
    }
    gfxEnd();
}
