/**
 * @file gs_gfx_layer_candidate_800D6B00.c
 * @brief GSgfx vertex flush for line primitives, 0x800D6B00 - 0x800D7230.
 *
 * gfxFlushPendingVertex replays the pending vertex
 * through the installed emitters; for primitive type 7 it keeps the
 * previous vertex in two save slots and emits the extra segment vertex.
 * The emitter sequence is expanded three times (repeated expansion), so
 * it is recovered as the static inline GSgfxEmitVertex.
 *
 * The unit owns the save slots: the pooled .bss at 0x804007E8-0x80400948
 * (0x160 bytes, retail label sTex0). Retail addresses them from one
 * base register and passes the first as "addi r3,r31,0x0", which MWCC
 * emits for three or more pooled objects under GC/1.3.2 (see the GS VM
 * note in 488eeb59), so the unit builds with GC/1.3.2.
 *
 * Retail lays the objects out as tex0, clr0, pos0, tex1, clr1, pos1
 * (0x0/0x80/0x98/0xB0/0x130/0x148). Without deferred inlining MWCC orders
 * pooled .bss by first reference (pos0 first); with -inline deferred it
 * uses reverse definition order, the same rule the linked pslist.c relies
 * on. The slots are declared held vertex (slot 1) first, each in the
 * vertex state's own pos/clr/tex field order, which gives retail's
 * layout. The object links as Matching with retail DOL/REL SHA-1.
 */
#include "game/gs_gfx_layer.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
extern void fn_800DB098(void);

/* Save slot 0 (the vertex being replaced) and slot 1 (the held vertex). */
static GSVtxValue3 sPos1;
static GSVtxColor sClr1[2];
static GSVtxValue2 sTex1[8];
static GSVtxValue3 sPos0;
static GSVtxColor sClr0[2];
static GSVtxValue2 sTex0[8];

static inline void GSgfxEmitVertex(void) {
    GSVtxDesc* desc;
    s32 i;
    s32 n;

    if (gsgfxState->captureActive == 1) {
        fn_800DB098();
    } else if (gsgfxState->layerDraw == gsgfxState->layerCur &&
               (gsgfxState->drawMask & gsgfxState->drawEnable)) {
        desc = gsgfxState->vtxDesc;
        if (desc->attr[0].enabled) {
            gsgfxState->emitMtxIdx(0);
        }
        gsgfxState->emitPos(0);
        if (desc->attr[2].enabled) {
            gsgfxState->emitNrm(0);
        }
        for (i = 4; i <= 5; i++) {
            if (desc->attr[i].enabled) {
                n = i - 4;
                gsgfxState->emitClr[n](n);
            }
        }
        for (i = 6; i <= 13; i++) {
            if (desc->attr[i].enabled) {
                n = i - 6;
                gsgfxState->emitTex[n](n);
            }
        }
    }
}

void gfxFlushPendingVertex(void) {
    s32 k;

    if (gsgfxState->vtxPending == 0) {
        return;
    }
    if ((gsgfxState->captureActive == 0 && gsgfxState->primType == 7) ||
        (gsgfxState->captureActive == 1 && gsgfxState->capturePrimType == 7)) {
        if (gsgfxState->lineHalf == 1) {
            memcpy(&sPos0, &gsgfxState->pos, sizeof(GSVtxValue3));
            memcpy(&sClr0, gsgfxState->clr, sizeof(gsgfxState->clr));
            memcpy(sTex0, gsgfxState->tex, sizeof(sTex0));
            gsgfxState->pos.b[0] = sPos1.b[0];
            gsgfxState->pos.s[0] = sPos1.s[0];
            gsgfxState->pos.f[0] = sPos1.f[0];
            memcpy(&gsgfxState->clr, sClr1, sizeof(sClr1));
            for (k = 0; k < 8; k++) {
                gsgfxState->tex[k].b[0] = sTex1[k].b[0];
                gsgfxState->tex[k].s[0] = sTex1[k].s[0];
                gsgfxState->tex[k].f[0] = sTex1[k].f[0];
            }
            GSgfxEmitVertex();
            memcpy(&gsgfxState->pos, &sPos0, sizeof(GSVtxValue3));
            memcpy(&gsgfxState->clr, sClr0, sizeof(sClr0));
            memcpy(gsgfxState->tex, sTex0, sizeof(sTex0));
            GSgfxEmitVertex();
            gsgfxState->pos.b[1] = sPos1.b[1];
            gsgfxState->pos.s[1] = sPos1.s[1];
            gsgfxState->pos.f[1] = sPos1.f[1];
            for (k = 0; k < 8; k++) {
                gsgfxState->tex[k].b[1] = sTex1[k].b[1];
                gsgfxState->tex[k].s[1] = sTex1[k].s[1];
                gsgfxState->tex[k].f[1] = sTex1[k].f[1];
            }
            gsgfxState->lineHalf = 0;
        } else {
            memcpy(&sPos1, &gsgfxState->pos, sizeof(GSVtxValue3));
            memcpy(&sClr1, gsgfxState->clr, sizeof(gsgfxState->clr));
            memcpy(sTex1, gsgfxState->tex, sizeof(sTex1));
            gsgfxState->lineHalf = 1;
        }
    }
    GSgfxEmitVertex();
    gsgfxState->vtxPending = 0;
}
