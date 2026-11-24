/**
 * @file gs_gfx_exact_800D9D68.c
 * @brief Matching GS graphics state range, 0x800D9D68-0x800DA578.
 */

#include "dolphin/types.h"
#include "game/gs_gfx_layer.h"

extern void gfxRecordCommand(u32, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GScameraGetActiveCamera(void);
#endif
extern void GXSetScissor(u32, u32, u32, u32);
extern void GScameraSetScissor(u32, u16, u16, u16, u16);
extern void HSD_FogSet(u32);
extern u32 lbl_8047AA8C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetCoPlanar(u32);
#endif
extern void GXSetClipMode(u32);
extern void GXSetCullMode(u32);
extern u8 lbl_8031453C[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetDither(u32);
#endif
extern void GXSetAlphaCompare(u32, u8, u32, u32, u8);
extern u8 lbl_8031457C[];
extern u8 lbl_8031456C[];
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetZMode(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetZCompLoc(u32);
#endif
extern u8 lbl_8031454C[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetColorUpdate(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetAlphaUpdate(u32);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetDstAlpha(u32 enable, u8 alpha);
#endif
extern void GXSetBlendMode(u32, u32, u32, u32);
extern u8 lbl_803145D0[];
extern u32 lbl_8031459C[];
extern u32 lbl_803145A8[];

/* Retail marks every function in this physical island active. */
#pragma force_active on

void gfxSetScissor(u16 x1, u16 y1, u16 x2, u16 y2) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x37, 4, (u32)x1, (u32)y1, (u32)x2, (u32)y2);
    } else {
        GXSetScissor((u32)x1, (u32)y1, (u32)(x2 - x1) + 1, (u32)(y2 - y1) + 1);
        gsgfxState->scissor[0] = x1;
        gsgfxState->scissor[1] = y1;
        gsgfxState->scissor[2] = x2;
        gsgfxState->scissor[3] = y2;
        {
            u32 result = PC_PTR32_IMPLICIT(GScameraGetActiveCamera());
            if (result) {
                GScameraSetScissor(result, x1, y1, x2, y2);
            }
        }
    }
}

void fn_800D9E4C(s32 val) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x36, 1, val);
    } else {
        if (val == 1) {
            state->fogActive = 1;
        } else if (val == 0) {
            state->fogActive = 0;
        }
        if (gsgfxState->fogActive == 1) {
            HSD_FogSet(lbl_8047AA8C);
        } else {
            HSD_FogSet(0);
        }
    }
}

void fn_800D9ED8(s32 val) {
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x35, 1, val); }
    else if (val == 1) { gsgfxState->layerCur = 1; }
    else if (val == 0) { gsgfxState->layerCur = 0; }
}

void gfxSetCoPlanar(s32 val) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) { gfxRecordCommand(0x34, 1, val); }
    else {
        if (val == 1) { state->coPlanar = 1; }
        else if (val == 0) { state->coPlanar = 0; }
        GXSetCoPlanar(gsgfxState->coPlanar);
    }
}

void gfxSetClipMode(s32 val) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) { gfxRecordCommand(0x33, 1, val); }
    else {
        if (val == 1) { state->clipMode = 0; }
        else if (val == 0) { state->clipMode = 1; }
        GXSetClipMode(gsgfxState->clipMode);
    }
}

void gfxSetCullMode(u32 idx) {
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x32, 1, idx); }
    else {
        gsgfxState->cullMode = ((u32*)lbl_8031453C)[idx];
        GXSetCullMode(gsgfxState->cullMode);
    }
}

void gfxSetDither(s32 val) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) { gfxRecordCommand(0x31, 1, val); }
    else {
        if (val == 1) { state->dither = 1; }
        else if (val == 0) { state->dither = 0; }
        GXSetDither(gsgfxState->dither);
    }
}

void gfxSetAlphaCompare(s32 enable, s32 comp0, u8 ref0, s32 op, s32 comp1, u8 ref1) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x30, 6, enable, comp0, (u32)ref0, op, comp1, (u32)ref1);
    } else {
        if (enable == 1) {
            state->alphaCompare = 1;
        } else if (enable == 0) {
            state->alphaCompare = 0;
        }
        if (gsgfxState->alphaCompare == 0) {
            GXSetAlphaCompare(7, 0, 1, 7, 0);
        } else {
            GXSetAlphaCompare(((u32*)lbl_8031457C)[comp0], ref0,
                        ((u32*)lbl_8031456C)[op],
                        ((u32*)lbl_8031457C)[comp1], ref1);
        }
    }
}

void gfxSetZMode(s32 zEnable, s32 zFunc, s32 zUpdate) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x2f, 3, zEnable, zFunc, zUpdate);
    } else {
        if (zEnable == 1) {
            state->zCompare = 1;
        } else if (zEnable == 0) {
            state->zCompare = 0;
        }
        if (zUpdate == 1) {
            gsgfxState->zCompLoc = 1;
        } else if (zUpdate == 0) {
            gsgfxState->zCompLoc = 0;
        }
        gsgfxState->zFunc = *(u32*)(lbl_8031454C + zFunc * 4);
        GXSetZMode(gsgfxState->zCompare, gsgfxState->zFunc, gsgfxState->zUpdate);
        GXSetZCompLoc(gsgfxState->zCompLoc);
    }
}

void gfxSetUpdates(s32 zCompare, s32 zBeforeTexture, s32 zFunc) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x2e, 3, zCompare, zBeforeTexture, zFunc);
    } else {
        if (zCompare == 1) {
            state->colorUpdate = 1;
        } else if (zCompare == 0) {
            state->colorUpdate = 0;
        }
        if (zBeforeTexture == 1) {
            gsgfxState->alphaUpdate = 1;
        } else if (zBeforeTexture == 0) {
            gsgfxState->alphaUpdate = 0;
        }
        if (zFunc == 1) {
            gsgfxState->zUpdate = 1;
        } else if (zFunc == 0) {
            gsgfxState->zUpdate = 0;
        }
        GXSetColorUpdate(gsgfxState->colorUpdate);
        GXSetAlphaUpdate(gsgfxState->alphaUpdate);
        GXSetZMode(gsgfxState->zCompare,
                   gsgfxState->zFunc,
                   gsgfxState->zUpdate);
    }
}

void gfxSetDstAlpha(s32 val, u8 b) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) { gfxRecordCommand(0x2d, 2, val, (u32)b); }
    else {
        if (val == 1) { state->dstAlpha = 1; }
        else if (val == 0) { state->dstAlpha = 0; }
        GXSetDstAlpha(gsgfxState->dstAlpha, b);
    }
}

void gfxSetBlendModeRaw(s32 val) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x2c, 1, val);
    } else if (val == 0) {
        GXSetBlendMode(0, 1, 1, 5);
        gsgfxState->drawEnable = 0x10;
    } else {
        GXSetBlendMode(2, 1, 1, *(u32*)(lbl_803145D0 + val * 4));
        gsgfxState->drawEnable = 0x2000;
    }
}

void gfxSetBlendMode(s32 a, s32 b, s32 c) {
    GSgfxState* state = gsgfxState;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x2b, 3, a, b, c);
    } else if (a == 0) {
        GXSetBlendMode(0, 1, 1, 5);
        gsgfxState->drawEnable = 0x10;
    } else {
        GXSetBlendMode(lbl_8031459C[a], lbl_803145A8[b], lbl_803145A8[c], 5);
        gsgfxState->drawEnable = 0x2000;
    }
}
