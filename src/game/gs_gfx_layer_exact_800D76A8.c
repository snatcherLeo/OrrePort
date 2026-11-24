/* GSgfx: draw a vertex list with its vertex format, 0x800D76A8 - 0x800D7820. */
#include "dolphin/types.h"
#include "game/gs_gfx_layer.h"

extern u8 lbl_804001F0[];
extern u8 lbl_80314370[];
extern u8 lbl_803143A8[];
extern u8 lbl_803143B4[];
extern u8 lbl_803143D8[];

extern void gfxRecordCommand(u32, ...);
extern void fn_800D7940(GSVtxDesc*, u16);
extern void gfxSetupChannelsTev(GSVtxDesc*);
extern void GXSetVtxAttrFmt(u32, u32, u32, u32, u8);
extern void GXClearVtxDesc(void);
extern void GXSetVtxDesc(u32, u32);
#ifdef TARGET_PC /* PC: the port's 3-argument GXSetArray (port/src/pc_gx_arrays.c); Aurora's GXSetArray takes 5 */
#define GXSetArray fn_800B84E0
#endif
extern void GXSetArray(u32, void*, u8);

/*
 * Body of fn_800D7A70 (out-of-line at 0x800D7A70; source in
 * gs_gfx_layer_state.c). Retail also expands it in place in gfxSetVtxDescRaw, so
 * the vertex-descriptor load is expanded here as the same body.
 */
static inline void GSgfxLoadVtxDesc(GSVtxDesc* desc) {
    u32 i;
    GSVtxAttr* entry;
    u32 attr;

    for (i = 0; (s32)i < 14; i++) {
        entry = &desc->attr[i];
        if (entry->enabled != 0 && (s32)i != 0) {
            attr = ((u32*)lbl_80314370)[i];
            GXSetVtxAttrFmt(desc->vtxFmt, attr,
                        ((u32*)lbl_803143B4)[entry->count],
                        ((u32*)lbl_803143D8)[entry->compType],
                        entry->frac);
        }
    }

    GXClearVtxDesc();

    for (i = 0; (s32)i < 14; i++) {
        entry = &desc->attr[i];
        if (entry->enabled != 0) {
            GXSetVtxDesc(((u32*)lbl_80314370)[i], ((u32*)lbl_803143A8)[entry->type]);
            if (entry->data != NULL) {
                GXSetArray(((u32*)lbl_80314370)[i], entry->data, entry->stride);
            }
        }
    }

    *(u32*)(lbl_804001F0 + 0x14) += 1;
}

void gfxSetVtxDescRaw(GSVtxDesc* desc, u16 vertCount) {
    GSVtxDesc* oldDesc;

    /* a display list being captured records the vertices instead (with desc current); recording stores one word */
    if (gsgfxState->captureActive == 1) {
        oldDesc = gsgfxState->vtxDesc;
        gsgfxState->vtxDesc = desc;
        fn_800D7940(desc, vertCount);
        gsgfxState->vtxDesc = oldDesc;
        return;
    }

    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x47, 2, PC_PTR32(desc), (u32)vertCount);
        return;
    }

    GSgfxLoadVtxDesc(desc);
    gfxSetupChannelsTev(desc);
    fn_800D7940(desc, vertCount);
}
