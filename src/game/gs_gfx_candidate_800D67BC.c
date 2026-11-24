/**
 * @file gs_gfx_candidate_800D67BC.c
 * @brief GS graphics vertex-batch draw, 0x800D67BC - 0x800D6A00.
 *
 * Function-boundary carve of fn_800D67BC. The range needs nothing its TU
 * would own: no jump table, no pooled constant, and the primitive table
 * (lbl_80314350) and draw counters (lbl_804001F0) stay extern.
 *
 * Retail expands fn_800D6A80 (the vertex/primitive counter, out of line at
 * 0x800D6A80 and owned by gs_gfx_exact_800D6A00.c) at the end of this
 * function, with the two counter addresses constant-folded. The callee
 * sits later in the TU than this caller, so the original build inlined it
 * with deferred inlining. The carve reproduces the same expansion by
 * defining the same fn_800D6A80 source `inline` ahead of the caller, which
 * emits no second fn_800D6A80 symbol.
 */
#include "dolphin/types.h"
#include "game/gs_gfx_layer.h"

extern void fn_800DB758(u16 vertCount);
extern void gfxRecordCommand(u32, u32, ...);
extern void gfxVtxDescClear(struct GSVtxDesc*);
extern void gfxVtxDescSetAttr(struct GSVtxDesc*, u32, u32, u32, u32, u8, void*, u8);
extern void gfxLoadVtxDesc(struct GSVtxDesc*);
extern void gfxSetupChannelsTev(struct GSVtxDesc*);
extern void GXBegin(u32, u32, u16);

extern u32 lbl_80314350[];
extern u8 lbl_804001F0[];

inline void fn_800D6A80(u16 vertCount, s32 type, u32* totalVerts, u32* totalPrims) {
    *totalVerts += vertCount;
    switch (type) {
        case 3: *totalPrims += vertCount / 3; break;
        case 4:
        case 5: *totalPrims += vertCount - 2; break;
        case 6:
        case 7: *totalPrims += (vertCount >> 1); break;
    }
}

void gfxBegin(u16 vertCount) {
    if (gsgfxState->captureActive == 1) {
        fn_800DB758(vertCount);
        return;
    }
    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(2, 1, vertCount);
        return;
    }
    if (gsgfxState->layerDraw != gsgfxState->layerCur) {
        return;
    }
    if ((gsgfxState->drawMask & gsgfxState->drawEnable) == 0) {
        return;
    }

    if (gsgfxState->vtxDesc == NULL) {
        /* no descriptor set: build the default one from the active attribute flags */
        gfxVtxDescClear(gsgfxState->defaultVtxDesc);
        gfxVtxDescSetAttr(gsgfxState->defaultVtxDesc, 1, 0, 1, 4, 0, 0, 0);
        if (gsgfxState->flags & 4) {
            gfxVtxDescSetAttr(gsgfxState->defaultVtxDesc, 2, 0, 2, 4, 0, 0, 0);
        }
        if (gsgfxState->flags & 1) {
            gfxVtxDescSetAttr(gsgfxState->defaultVtxDesc, 4, 0, 6, 10, 0, 0, 0);
        }
        if (gsgfxState->flags & 2) {
            gfxVtxDescSetAttr(gsgfxState->defaultVtxDesc, 6, 0, 8, 4, 0, 0, 0);
        }
        gsgfxState->vtxDesc = gsgfxState->defaultVtxDesc;
    }

    if (gsgfxState->primType == 7) {
        vertCount = (vertCount & 0x7fff) << 1;
    }

    /* the current descriptor goes to GX, then the channel / TEV setup it implies */
    gfxLoadVtxDesc(gsgfxState->vtxDesc);
    gfxSetupChannelsTev(gsgfxState->vtxDesc);
    GXBegin(lbl_80314350[((u32)gsgfxState->primType)],
                gsgfxState->vtxDesc->vtxFmt, vertCount);
    fn_800D6A80(vertCount, gsgfxState->primType,
                (u32*)(lbl_804001F0 + 0xc), (u32*)(lbl_804001F0 + 0x4));
}
