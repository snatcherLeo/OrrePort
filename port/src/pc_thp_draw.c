/*
 * pc_thp_draw.c: the THP movie player's YUV -> RGB drawing for the PC (2026-10-16).
 *
 * The GameCube code is src/dolphin/thp/THPDraw.c (0x801E1E1C - 0x801E25C8). It can't be compiled for the PC as it is:
 * it declares its own GX types and writes the quad's vertices straight into the GPU's write-gather FIFO at
 * 0xCC008000. This file does the same three things with Aurora's GX (same GX state, same TEV setup, same quad), under
 * the names THPPlayer.c calls (*updated 2026-10-18:* upstream named the three addresses, so the address names
 * fn_801E1FF8 / fn_801E1E1C / fn_801E24B0 became these real names; Aurora defines none of them):
 *   THPGXYuv2RgbSetup (render mode -> orthographic projection, 4-stage YUV -> RGB TEV),
 *   THPGXYuv2RgbDraw (three I8 textures: Y full size, U / V half size; one textured quad),
 *   THPGXRestore (back to a plain one-stage pass-through).
 * The TEV constants (the YUV -> RGB coefficients as konst colours and a signed register colour) are the GameCube
 * file's. Built against Aurora's headers only (orre_platform_vi).
 */
#include <dolphin/gx.h>
#include <dolphin/mtx.h>

void THPGXRestore(void) {
    GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapMode(GX_TEVSTAGE3, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
}

void THPGXYuv2RgbSetup(GXRenderModeObj* rmode) {
    const s32 w = rmode->fbWidth;
    const s32 h = rmode->efbHeight;
    Mtx44 m;
    Mtx e_m;

    GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    C_MTXOrtho(m, 0.0f, (f32)h, 0.0f, (f32)w, 0.0f, -1.0f);
    GXSetProjection(m, GX_ORTHOGRAPHIC);
    GXSetViewport(0.0f, 0.0f, (f32)w, (f32)h, 0.0f, 1.0f);
    GXSetScissor(0, 0, (u32)w, (u32)h);
    PSMTXIdentity(e_m);
    GXLoadPosMtxImm(e_m, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetDispCopyGamma(GX_GM_1_0);
    GXSetNumChans(0);
    GXSetNumTexGens(2);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXInvalidateTexAll();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
    GXSetNumTevStages(4);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_C0);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
    GXSetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP2, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_CPREV);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_FALSE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_APREV);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K1);
    GXSetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_K1_A);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_CPREV);
    GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
    GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE3, GX_CC_APREV, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE3, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K2);

    GXSetTevColorS10(GX_TEVREG0, (GXColorS10){-90, 0, -114, 135});
    GXSetTevKColor(GX_KCOLOR0, (GXColor){0, 0, 226, 88});
    GXSetTevKColor(GX_KCOLOR1, (GXColor){179, 0, 0, 182});
    GXSetTevKColor(GX_KCOLOR2, (GXColor){255, 0, 255, 128});
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
}

void THPGXYuv2RgbDraw(u8* y_data, u8* u_data, u8* v_data, s16 x, s16 y, s16 textureWidth, s16 textureHeight,
                 s16 polygonWidth, s16 polygonHeight) {
    /* PC: the three plane objects persist across frames. The GameCube initialises fresh texture objects every frame,
     * which is free there; in Aurora every GXInitTexObj gets a new object id, and each id's texture stays cached for
     * 600 frames after its last use, so a movie kept ~10 s of frames (three new GPU textures each) alive: about
     * 1.5-1.8 GB extra while a movie plays (B, 2026-10-16, measured: 2.5 GB in the attract demo, 0.73 GB with the
     * movie decoded but not drawn). GXInitTexObjData keeps the id and bumps its version, so the entry is replaced. */
    static GXTexObj tobj0, tobj1, tobj2;
    static s16 s_width, s_height;
    extern volatile int pc_vi_movie_drew; /* pc_vi.c: a movie frame must be shown even without HSD's display copy */

    pc_vi_movie_drew = 1;
    if (textureWidth != s_width || textureHeight != s_height) {
        s_width = textureWidth;
        s_height = textureHeight;
        GXInitTexObj(&tobj0, y_data, (u16)textureWidth, (u16)textureHeight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&tobj0, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GXInitTexObj(&tobj1, u_data, (u16)(textureWidth >> 1), (u16)(textureHeight >> 1), GX_TF_I8, GX_CLAMP,
                     GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&tobj1, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GXInitTexObj(&tobj2, v_data, (u16)(textureWidth >> 1), (u16)(textureHeight >> 1), GX_TF_I8, GX_CLAMP,
                     GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&tobj2, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    } else {
        GXInitTexObjData(&tobj0, y_data);
        GXInitTexObjData(&tobj1, u_data);
        GXInitTexObjData(&tobj2, v_data);
    }
    GXLoadTexObj(&tobj0, GX_TEXMAP0);
    GXLoadTexObj(&tobj1, GX_TEXMAP1);
    GXLoadTexObj(&tobj2, GX_TEXMAP2);

    GXBegin(GX_QUADS, GX_VTXFMT7, 4);
    GXPosition3s16(x, y, 0);
    GXTexCoord2u16(0, 0);
    GXPosition3s16((s16)(x + polygonWidth), y, 0);
    GXTexCoord2u16(1, 0);
    GXPosition3s16((s16)(x + polygonWidth), (s16)(y + polygonHeight), 0);
    GXTexCoord2u16(1, 1);
    GXPosition3s16(x, (s16)(y + polygonHeight), 0);
    GXTexCoord2u16(0, 1);
    GXEnd();
}
