/**
 * @file gs_gfx_range_800D3FA4.c
 * @brief GS render engine segment -- split from gs_render.c.
 *
 * XD source unit: GSgfxM unknown (best guess layer.cpp analog)
 * Address range: 0x800D3FA4 - 0x800D6B00 (44 functions)
 *
 * Zero anchors -> gate-2 trivially passes. Shape: big fns 0x654/0x97C/0x56C followed by ~35 medium 0x64-0xB4 methods, resembling XD layer.cpp (0x3AD4, RenderStats 0xF74 monster + many mid-size GSgfxLayer methods), but not conclusive; honest range name.
 *
 * Split from src/game/gs_render.c (physical XD source-unit split).
 * The dead #ifdef PCPORT reference block (never defined in configure.py,
 * same situation as gs_gfx.c) was stripped during the split.
 */

#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#include "game/gs_gfx_layer.h"
#include "game/gs_render_util.h" /* GSRenderCamera */

/* ===== External SDK / engine functions ===== */
extern void  GSlogWrite(const char*, ...);             /* OSReport / GSlog */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif

/* External functions referenced from asm wrappers */
extern void DCFlushRange(void* addr, u32 size);
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern u64 OSGetTime(void);
#endif
extern void GSgfxSetViewportJitter(s32, f32, f32, f32, f32, f32, f32);
extern void gfxRecordCommand(u32, u32, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u16);
#endif
extern void gfxSetupChannelsTev(struct GSVtxDesc*);
extern u32 OSGetTick(void);
extern void fn_801E17A8(void);
extern void HSD_StateInvalidate(u32);
extern s32 HSD_CObjSetCurrent(void*);
extern void GSlightSetupLights(void*);
extern void GSmodelDrawAll(u32, u32);
extern void gsParticleDraw(u32, u32);
extern void HSD_CObjEndCurrent(void);
extern void _gfxSetFlagsImm(s32);
extern u32* gfxPlayCommands(u32*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetAlphaCompare(u32, u32, u8, u32, u32, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(f32, f32, f32, f32);
#endif
extern void gfxSetClipMode(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(void* desc);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(u32);
#endif
extern void gfxVtxPos3u16(u16, u16, u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(u32, u8, u8, u8, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif

/* GSmem */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   _toolentryAlloc__FUl(u32 size);                    /* GSmemAllocRaw */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);                  /* GSmemGetPtr */
#endif

/* SDK GX functions */
extern void  VIGetNextField(void);                        /* GXSetViewport */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GXSetViewportJitter(void);                        /* GXSetProjection */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GXSetViewport(void);                        /* GXLoadPosMtxImm */
#endif
extern void  GXInvalidateTexAll(void);                        /* GXInvalidateTexAll */



/* ===== String constants (rodata) ===== */
extern const char lbl_80270440[]; /* "GSgfx: invalid matrix index" */
extern const char lbl_80270460[]; /* "GSgfx: matrix stack underflow!" */
extern const char lbl_80270480[]; /* "GSgfx: matrix stack overflow!" */
extern const char lbl_802704A0[]; /* "0123456789ABCDEF" */
extern const char lbl_80270528[]; /* "GSmaterialSetPEdescr: Warning..." */
extern const char lbl_8027056C[]; /* "GSmaterialCreate: Run out of materials..." */
extern const char lbl_802705C0[]; /* "GSmaterial MObj" */
extern const char lbl_802705D0[]; /* "GSmaterial: Unsupported texture format..." */
extern const char lbl_80270610[]; /* "GSmaterial: Error creating environment map..." */

/* ===== BSS / global state ===== */
extern u32 lbl_8047AA8C;


extern u8 lbl_803147C8[];
extern f32 lbl_8047CA20;
extern f32 lbl_8047CA24;
extern f32 lbl_8047CA28;

/* ===== Combined forward-decls (duplicated across split segments) ===== */

/* No-op functions (1) */
/* Address: 0x800DC874 | Size: 0x4 */
/* Forward declarations for self-referencing asm blocks */
extern void gfxFlushPendingVertex(void);
extern void _gfxEmitTexCoord1x8(u32 idx);
extern void _gfxEmitTexCoord1x16(u32 idx);
extern void _gfxEmitTexCoord2u8(u32 idx);
extern void _gfxEmitTexCoord2s16(u32 idx);
extern void _gfxEmitTexCoord2u16(u32 idx);
extern void _gfxEmitTexCoord2f32(u32 idx);
extern void _gfxEmitColor1x8(u32 idx);
extern void _gfxEmitColor1x16(u32 idx);
extern void _gfxEmitColor1u16(u32 idx);
extern void _gfxEmitColor1u32(u32 idx);
extern void _gfxEmitColor3u8(u32 idx);
extern void _gfxEmitColor4u8(u32 idx);
extern void _gfxEmitNormal1x8(void);
extern void _gfxEmitNormal1x16(void);
extern void _gfxEmitNormal3s8(void);
extern void _gfxEmitNormal3s16(void);
extern void _gfxEmitNormal3f32(void);
extern void _gfxEmitPos1x8(void);
extern void _gfxEmitPos1x16(void);
extern void _gfxEmitPos2s8(void);
extern void _gfxEmitPos2u8(void);
extern void _gfxEmitPos2s16(void);
extern void _gfxEmitPos2u16(void);
extern void _gfxEmitPos2f32(void);
extern void _gfxEmitPos3s8(void);
extern void _gfxEmitPos3u8(void);
extern void _gfxEmitPos3s16(void);
extern void _gfxEmitPos3u16(void);
extern void gfxVtxDescClear(struct GSVtxDesc*);
extern void gfxVtxDescSetAttr(struct GSVtxDesc*, u32, u32, u32, u32, u8, void*, u8);
extern void fn_800D7940(struct GSVtxDesc*, u16);
extern void gfxLoadVtxDesc(struct GSVtxDesc*);
extern void fn_800DB098(void);
extern void fn_800DB758(u16);
extern void lightGetFrameCount__FP9_HSD_AObj(u8*);
extern void GSlogVPrintf(void);
extern void GSlogFormat(void);
extern void fn_800E09E8(void*, void*, u32);
extern u8 GSmemDumpMap(u32, u32);
extern u32 _matGSmatObjMakeTExp(void*, void*, void*, void*, void*);
extern void _matGSmatEnableEnvMapExt(u8*);
extern s32 _matGSmatObjLoad(u8*);
extern void GSmtxConcat(void*, void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E02C4(void*, f32, f32, f32); /* scale the matrix by x, y, z */
#endif
extern void GSmtxRotateZ(void*, f32);
extern void GSmtxRotateY(void*, f32);
extern void GSmtxRotateX(void*, f32);
extern void fn_800E03E8(void*, f32, f32, f32);
extern void fn_800E0628(void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E064C(void*);
#endif
extern void GSmtx44Perspective(u8*);
extern void GSmtx44Ortho(void*, f32, f32, f32, f32, f32, f32);
extern void fn_800E0C78(void);
extern void GSmathInitCosTable(void);


/* ===== Combined externs (duplicated across all gs_render.c split segments;
 * de-duplicated by identifier from the whole original TU so any
 * cross-segment call/reference resolves regardless of which segment
 * the callee's real definition ended up in). ===== */
extern u8 lbl_8047AA91;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetPointSize(u32, u32);
#endif
extern f32 lbl_8047CA30;
extern f32 lbl_8047CA34;
extern f32 lbl_8047CA38;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetLineWidth(u32, u32);
#endif
extern void _gfxEmitMtxIdx(void);
extern void _gfxEmitPos3f32(void);
extern void GXBegin(u32, u32, u16);
extern u8 lbl_80314350[];
extern u8 lbl_804001F0[];
extern void fn_800D6A80(u16, s32, u32*, u32*);
extern u8 lbl_804007E8[];
extern void GXSetVtxAttrFmt(u32, u32, u32, u32, u8);
extern void GXClearVtxDesc(void);
extern void GXSetVtxDesc(u32, u32);
#ifdef TARGET_PC /* PC: the port's 3-argument GXSetArray (port/src/pc_gx_arrays.c); Aurora's GXSetArray takes 5 */
#define GXSetArray fn_800B84E0
#endif
extern void GXSetArray(u32, u32, u8);




extern u32 lbl_8047AAB0;
extern u32 lbl_8047AAAC;

extern u32 lbl_8047AAB4;
extern u16 lbl_8047AAA8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GScameraGetActiveCamera(void);
#endif
extern u32 GScameraGetMatrixPtr(void);
extern u32 GScameraGetInvMatrixPtr(void);
extern u32 GScameraGetProjMatrixPtr(void);
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadPosMtxImm(u32, u32);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadNrmMtxImm(u32, u32);
#endif
extern void GXSetCurrentMtx(u32);
extern u8 lbl_8047AAC8;


extern u8 lbl_80400948[];


extern u16 lbl_8047AAB8;
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetTexCoordGen2(u32, u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadTexMtxImm(void*, u32, u32);
#endif




extern void GXInitTexObjWrapMode();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXInitTexObjLOD();
#endif
extern void GXLoadTlut();
extern void GXLoadTexObj();

extern u32 lbl_8047CA40;



extern void GXSetTevDirect(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetNumIndStages(u32);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetNumChans();
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetChanCtrl();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetNumTexGens();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetNumTevStages();
#endif
extern void GXSetIndTexOrder();
extern void GXSetIndTexCoordScale();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetIndTexMtx();
#endif
extern void GXSetTevOrder();
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetTevColorOp();
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetTevAlphaOp();
#endif
extern void GXSetTevColorIn();
extern void GXSetTevAlphaIn();
extern void GXSetTevKColorSel();
extern void GXSetTevKAlphaSel();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetTevIndirect();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800BBC7C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800BBCE0();
#endif
extern void fn_800BBE8C();
extern void fn_800BBF98();
extern void fn_800BBFDC();
extern void GXSetTevKColor();
extern void GXSetTevSwapModeTable();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetProjection(void*, u32);
#endif
extern f32 lbl_8047CA50;
extern f32 lbl_8047CA54;
extern void GScameraSetViewport(void*, u16, u16, u16, u16);


extern u32 lbl_8047CA58;
extern void GXSetScissor(u32, u32, u32, u32);
extern void GScameraSetScissor(u32, u16, u16, u16, u16);
extern void HSD_FogSet(u32);
extern u32 lbl_8047AA8C;
extern void GXSetClipMode(u32);
extern void GXSetCullMode(u32);

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetDither(u32);
#endif
extern void GXSetAlphaCompare(u32, u8, u32, u32, u8);


#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetZMode(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXSetZCompLoc(u32);
#endif

#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetDstAlpha(u32);
#endif
extern void GXSetBlendMode(u32, u32, u32, u32);



extern void jumptable_803152B8();
extern void jumptable_80315340();
extern void jumptable_80315320();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXCallDisplayList(u32, u32);
#endif
extern void fn_800E2AF8(u16);
extern u16 GSmemAlloc(u32, u32);
extern u32 lbl_8047AAD8;

extern void jumptable_80315364();
extern u32 GStextureUnlockImage(void*);

extern u8 lbl_8047AAE0;
extern void GStextureGetFormat(void);
extern void GStextureSetWrap(void);
extern void GStextureSetFilter(void);
extern void* GStextureLockImage(void*, u32);
extern void GStextureConvertFromHW(void);
extern void HSD_LObjReqAnimAll(void*, f32);
extern void HSD_LObjAnimAll(void*);


extern f32 lbl_8047CA70;
extern u32 lbl_8047CA74;
extern f32 lbl_8047CA78;
extern u32 _lightStructCount;
extern void HSD_LObjSetPosition();
extern void HSD_LObjSetInterest();
extern void HSD_LObjRemoveAnimAll(void*);
extern void HSD_LObjAddAnimAll(void*, void*);
extern void HSD_ForeachAnim(void*, u32, u32, void*, u32, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_800D37CC(void);
#endif
extern void HSD_AObjSetRate(void);
extern f32 lbl_8047AAF4;
extern f32 lbl_8047CA88;
extern void GSlightSetAnimIndex(u8*, u32);
extern void HSD_LObjGetPosition(void*, void*);
extern void HSD_LObjGetInterest(void*, void*);
extern void HSD_LObjSetColor(u32, u8*);
extern void HSD_LObjClearFlags(u32, u32);
extern void HSD_LObjSetFlags(u32, u32);
extern u32 HSD_LObjLoadDesc(void*);
extern u32 lbl_8047CA8C;
extern u16 lbl_8047AAE8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(u8*, u32, u8*);
#endif
extern u8 lbl_8047CA90;
extern u8 lbl_8047CA98;
extern void HSD_LObjDeleteCurrentAll(void*);
extern void HSD_LObjAddCurrentAll(void);
extern void HSD_LObjSetup(void*);
extern u32 lbl_8047AAF8;
extern u32 lbl_8047AB08;
extern u32 lbl_8047AAFA;
extern u32 lbl_8047AAFC;
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void OSTicksToCalendarTime(void);
#endif
extern void logVsnprintf_float(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 strlen(const char* s);
#endif
extern u32 lbl_8047AB11;
extern u8 lbl_80400F30[];
extern u8 lbl_802704B4[];
extern u8 lbl_80400F44[];
extern u32 lbl_8047AB0C;
extern u32 lbl_8047AB00;
extern u32 lbl_8047AB04;
extern u8 lbl_80401044[];
extern u8 lbl_80401058[];
extern u32 lbl_8047AB10;
extern void jumptable_80315388();
extern void __va_arg();
extern u8 lbl_80401168[];
extern u8 lbl_80401178[];

extern u8 lbl_8047CAA0[];
extern u8 lbl_8047CAA8[];
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXDrawDone(u32);
#endif
extern void HSD_ImageDescFree(u32);
extern void* HSD_ImageDescAlloc(void);
extern u16 GStextureGetXsize(void*);
extern u16 GStextureGetYsize(void*);
extern void* GStextureGetGXformat(void*, u32);
extern u8 GStextureGetMiplevels(void*);
extern f32 lbl_8047CAC8;
extern void HSD_MObjSetAlpha(u32, ...);
extern f64 lbl_8047CAD0;
extern f32 lbl_8047CACC;
extern void HSD_TObjRemove(void*);
extern void HSD_MObjCompileTev(void*);
extern u32 HSD_MObjGetFlags(void*);
extern void HSD_MObjClearFlags(void*, u32);
extern void HSD_MObjSetFlags(void*, void*);
extern u32 lbl_8047AB20;

extern void HSD_MObjSetDefaultClass(void*);
extern u16 lbl_8047AB18;

extern void hsdInitClassInfo(void*, void*, void*, void*, u32, u32);
extern u8 hsdMObj[];
extern void HSD_TExpGetType();
extern void fn_801B6DC0();
extern void HSD_TExpCnst();
extern void HSD_TExpTev();
extern void HSD_TExpColorOp();
extern void HSD_TExpColorIn();
extern void HSD_TExpAlphaOp();
extern void HSD_TExpAlphaIn();
extern void HSD_ImageDescRemove(void);
extern void HSD_TObjLoadDesc(void);
extern void HSD_MObjGetTObj(void);
extern void HSD_MObjAddTObjNext(void);
extern void HSD_MObjAddTObj(void);

extern void PSMTXMultVec(void*, void*, void*);
extern void PSVECCrossProduct(void*, void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void PSVECDotProduct(void);
#endif
extern void PSVECSquareDistance(void);
extern void PSVECDistance(void);
extern void PSVECNormalize(void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void PSVECMag(void);
#endif
extern void PSVECScale(void*, void*, f32);
extern const f32 lbl_8047CAD8;
extern void PSVECSubtract(void*, void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void PSVECAdd(void*, void*, void*);
#endif
extern f32 lbl_8047CADC;
extern void C_MTXLookAt(void);
extern void PSMTXTranspose(void*, void*);
extern void PSMTXInverse(void*, void*);
extern void PSMTXConcat(void*, void*, void*);
extern void PSMTXScaleApply(void*, void*);
extern void PSMTXRotRad(void*, u32);
extern void PSMTXTransApply(void*, void*, f32, f32, f32);
extern void PSMTXQuat(void);

extern void C_MTXPerspective(void);
extern void C_MTXOrtho(void);
extern void C_QUATSlerp(void*, void*, void*);
extern void EulerToQuat(void*, void*);
extern void C_QUATRotAxisRad(void);
extern void PSQUATMultiply(void*, void*, void*);
extern u32 lbl_8047CAE4;
extern u32 lbl_8047CAE0;
extern u32 lbl_8047CAE8;
extern f32 lbl_8047CAF0;
extern f32 lbl_8047CAF4;

extern u32 lbl_8047CAF8;

extern u32 lbl_8047CAFC;
extern f32 HSD_Randf(void);
extern f32 lbl_8047CB10;
extern u32 HSD_Rand(void);
extern u32 lbl_80478C94;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 fmod(f64 x, f64 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 __cvt_fp2unsigned(f32 x);
#endif
extern f32 lbl_8047CB20;
extern f32 lbl_8047CB1C;
extern f32 lbl_8047CB24;
extern f64 lbl_8047CB28;
extern f32 lbl_8047CB18;
extern f32 lbl_8047CB34;
extern f32 lbl_8047CB30;
extern f32 lbl_804011B8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
#ifndef TARGET_PC /* PC: the C library's prototype in pc/prototypes.h (double cos(double)) */
extern f64 cos(f32);
#endif
#endif
extern f32 lbl_8047CB38;
extern f64 lbl_8047CB40;







extern u32 lbl_8047AB4C;
extern u32 lbl_8047AB48;
extern u32 lbl_8047AB60;
extern u32 lbl_8047AB5C;
extern u32 lbl_8047AB58;
extern u32 lbl_8047AB54;
extern u32 lbl_8047AB50;
extern u32 lbl_8047CB50;
extern u32 lbl_8047CB48;
extern u32 lbl_8047AB3C;


#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D3FA4_800D45F8)
/* ==================================================================
 * GSgfxDrawScene -- GSgfx_BeginFrame
 *
 * Replays the recorded command queue into the GX pipeline for a new
 * frame: two render passes over the 0x10/0x1000/0x2000 layers (timed
 * into the stats block), with the screen-quad clear between them.
 *
 * Reconstructed helpers, each admitted by repeated expansion in retail:
 * gfxRenderPass (the pass body, twice), gfxSetupLayerCamera (six times,
 * differing only in the layer and pass constants) and gfxRunQueue (four
 * times). The second pass drops flags 0x1 and 0x100 inside the pass
 * body; retail still tests bit 0 there, because the TU was built without
 * copy propagation (-opt nopropagation on this one-function unit; see
 * configure.py). The 0.0f/640.0f/480.0f literals are the TU's own pool
 * (.sdata2 0x8047CA20-0x8047CA30).
 * ================================================================== */
static inline void gfxSetupLayerCamera(u32 layer, u8 pass) {
    u32 camera = PC_PTR32_IMPLICIT(GScameraGetActiveCamera());

    if (camera != 0) {
        HSD_StateInvalidate(0x7F);
        if (HSD_CObjSetCurrent(((GSRenderCamera*)camera)->cobj) != 0) {
            GSlightSetupLights(((GSRenderCamera*)camera)->cobj);
            GSmodelDrawAll(layer, pass);
            gsParticleDraw(layer, pass);
            HSD_FogSet(lbl_8047AA8C);
            HSD_CObjEndCurrent();
        }
        _gfxSetFlagsImm(-1);
    }
}

static inline void gfxRunQueue(void) {
    u32* cursor = gsgfxState->requestBuffer;

    while (cursor < gsgfxState->requestBufferPos) {
        cursor = gfxPlayCommands(cursor);
    }
}

static inline void gfxRenderPass(u32 flags, u8 setupCamera, u8 pass) {
    u32 start;

    if (pass != 0) {
        flags &= ~0x101;
    }
    gsgfxState->layerDraw = pass;
    *(u32*)(lbl_804001F0 + 0x40 + pass * 12) = 0;
    *(u32*)(lbl_804001F0 + 0x44 + pass * 12) = 0;
    *(u32*)(lbl_804001F0 + 0x48 + pass * 12) = 0;

    if (flags & 1) {
        fn_801E17A8();
    }

    if (flags & 0x10) {
        start = OSGetTick();
        gsgfxState->drawMask = 0x10;
        if (setupCamera != 0) {
            gfxSetupLayerCamera(0x10, pass);
        }
        gfxRunQueue();
        *(u32*)(lbl_804001F0 + 0x40 + pass * 12) = OSGetTick() - start;
    }

    if (flags & 0x1000) {
        start = OSGetTick();
        gsgfxState->drawMask = 0x1000;
        if (setupCamera != 0) {
            gfxSetupLayerCamera(0x1000, pass);
        }
        *(u32*)(lbl_804001F0 + 0x44 + pass * 12) = OSGetTick() - start;
    }

    if (flags & 0x2000) {
        start = OSGetTick();
        gsgfxState->drawMask = 0x2000;
        if (setupCamera != 0) {
            gfxSetupLayerCamera(0x2000, pass);
        }
        gfxRunQueue();
        *(u32*)(lbl_804001F0 + 0x48 + pass * 12) = OSGetTick() - start;
    }
}

void GSgfxDrawScene(u32 flags, u8 setupCamera, u8 resetQueue) {
    GSgfxState* state;
    u32 oldMode;
    u32 oldMask;
    u8 oldAlpha;
    u32 cursor;

    state = gsgfxState;

    cursor = (u8*)state->requestBufferPos - (u8*)state->requestBuffer;
    if (cursor > *(u32*)(lbl_804001F0 + 0x28)) {
        *(u32*)(lbl_804001F0 + 0x28) = cursor;
    }

    oldMode = ((u32)state->recordMode);
    oldMask = state->drawMask;
    state->recordMode = 2;
    HSD_FogSet(lbl_8047AA8C);

    gfxRenderPass(flags, setupCamera, 0);

    gsgfxState->drawMask = -1;
    oldAlpha = gsgfxState->layerCur;
    gsgfxState->layerCur = gsgfxState->layerDraw;
    gfxSetUpdates(gsgfxState->overlayColorUpdate, 0, 1);
    gfxSetZMode(1, 7, 2);
    gfxSetAlphaCompare(0, 7, 0, 1, 7, 0);
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    gfxSetClipMode(0);
    gfxSetCullMode(0);
    gfxSetVtxDesc(lbl_803147C8);
    gfxSetPrimitive(4);
    gfxBegin(4);
    gfxVtxPos3u16(0, 0, 0x752F);
    gfxVtxColor4u8(0, 0, 0, 0, 0xC4);
    gfxVtxPos3u16(0x280, 0, 0x752F);
    gfxVtxColor4u8(0, 0, 0, 0, 0xC4);
    gfxVtxPos3u16(0, 0x1E0, 0x752F);
    gfxVtxColor4u8(0, 0, 0, 0, 0xC4);
    gfxVtxPos3u16(0x280, 0x1E0, 0x752F);
    gfxVtxColor4u8(0, 0, 0, 0, 0xC4);
    gfxEnd();
    gsgfxState->layerCur = oldAlpha;
    gfxSetUpdates(1, 1, 1);
    gfxSetZMode(1, 7, 2);
    gfxSetClipMode(1);

    gfxRenderPass(flags, setupCamera, 1);

    gsgfxState->layerDraw = 0;
    gfxSetUpdates(1, 1, 1);
    gfxSetZMode(1, 2, 2);
    gsgfxState->recordMode = oldMode;
    gsgfxState->drawMask = oldMask;
    if (resetQueue != 0) {
        gsgfxState->requestBufferPos = gsgfxState->requestBuffer;
    }
}
#endif


/* Render-command stream callees. */
extern void fn_800D30AC(void);
extern void gfxSetPointSize(f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetLineWidth(f32);
#endif
extern void gfxVtxMtxIdx(u8);
extern void gfxVtxTexCoord1x8(u32, u8);
extern void gfxVtxTexCoord1x16(u32, u16);
extern void gfxVtxTexCoord2u8(u32, u8, u8);
extern void gfxVtxTexCoord2s16(u32, s16, s16);
extern void gfxVtxTexCoord2u16(u32, u16, u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxTexCoord(u32, f32, f32);
#endif
extern void gfxVtxColor1x8(u32, u8);
extern void gfxVtxColor1x16(u32, u16);
extern void gfxVtxColor1u16(u32, u16);
extern void gfxVtxColor1u32(u32, u32);
extern void gfxVtxColor(u32, u8, u8, u8);
extern void gfxVtxNormal1x8(u8);
extern void gfxVtxNormal1x16(u16);
extern void gfxVtxNormal3s8(s8, s8, s8);
extern void gfxVtxNormal3s16(s16, s16, s16);
extern void gfxVtxNormal3f32(f32, f32, f32);
extern void gfxVtxPos1x8(u8);
extern void gfxVtxPos1x16(u16);
extern void gfxVtxPos2s8(s8, s8);
extern void gfxVtxPos2u8(u8, u8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos2s16(s16, s16);
#endif
extern void gfxVtxPos2u16(u16, u16);
extern void gfxVtxPos2f32(f32, f32);
extern void gfxVtxPos3s8(s8, s8, s8);
extern void gfxVtxPos3u8(u8, u8, u8);
extern void gfxVtxPos3s16(s16, s16, s16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos(f32, f32, f32);
#endif
extern void gfxSetVtxDescRaw(struct GSVtxDesc*, u16);
extern void gfxFlushMatrix(void);
extern void gfxGetMatrix(u8, u32);
extern void gfxSetMatrix(u8, void*);
extern void gfxPopMatrix(void);
extern void gfxPushMultMatrix(void*);
extern void gfxMultMatrix(void*);
extern void gfxPushLoadMatrix(void*);
extern void gfxScaleMatrix(f32, f32, f32);
extern void gfxTranslateMatrix(f32, f32, f32);
extern void gfxRotateXYZ(f32, f32, f32);
extern void gfxLoadIdentity(void);
extern void gfxLoadTexMatrix(u32, u32, u32, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxLoadTexture(u32, u32);
#endif
extern void GSgfxDLDraw(struct GSgfxDLCapture*);
extern void gfxSetPerspective(f32, f32, f32, f32);
extern void gfxSetViewport(u16, u16, u16, u16);
extern void gfxSetScissor(u16, u16, u16, u16);
extern void fn_800D9E4C(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(u32);
#endif
extern void gfxSetCoPlanar(u32);
extern void gfxSetDither(u32);
extern void gfxSetDstAlpha(u32, u8);
extern void gfxSetBlendModeRaw(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(u32, u32, u32);
#endif
extern void fn_800DB900(u32, void*, s8);
extern void fn_800DB988(u32, u32, u32);
extern void fn_800DB9F0(u32, u32, u32);
extern void fn_800DBA54(u8);
extern void fn_800DBAA4(u32);
extern void fn_800DBB0C(u32, u32, u32);
extern void fn_800DBB84(u32, u32, u32);
extern void fn_800DBBFC(u32, u32, u16, u16, u16, u16, u32, u32, u32, u32);
extern void fn_800DBCE4(u32, u32, u8, u8, u32);
extern void fn_800DBD70(u32, u32, u32, u32, u32, u32, u32, u8, u8, u32);
extern void fn_800DBE5C(u32);
extern void fn_800DBEB4(u32, GXColor);
extern void fn_800DBF1C(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DBF78(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DBFD4(u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DC04C(u32, u32, u32, u32, u8, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DC0D4(u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DC14C(u32, u32, u32, u32, u8, u32);
#endif
extern void fn_800DC1D4(u8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetTevStage(u32, u32, u32, u32, u32);
#endif

#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D461C_800D55D0)
/* ==================================================================
 * fn_800D461C -- render-command interpreter
 *
 * Replays one recorded GSgfx command: reads the command's argument words,
 * advances the cursor and calls the handler, returning the next command.
 * The unit owns its switch table (.data 0x80314188) and links as
 * Matching. Case 39 reads its three words with `*p++` (retail advances the
 * cursor before passing it); case 91 is a documented title-path exception
 * (see docs/RULE_EXCEPTIONS.md).
 * ================================================================== */
u32* gfxPlayCommands(u32* command)
{
    u32* p = command + 1;
    GXColor copied;
    u32 argument;

    switch (command[0]) {
    case 1:
        p += 1;
        gfxSetPrimitive(p[-1]); break;
    case 2:
        p += 1;
        gfxBegin((u16)p[-1]); break;
    case 3:
        gfxEnd(); break;
    case 4:
        fn_800D30AC(); break;
    case 5:
        p += 3;
        gfxVtxPos(((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 6:
        p += 3;
        gfxVtxPos3u16((u16)p[-3], (u16)p[-2], (u16)p[-1]); break;
    case 7:
        p += 3;
        gfxVtxPos3s16((s16)p[-3], (s16)p[-2], (s16)p[-1]); break;
    case 8:
        p += 3;
        gfxVtxPos3u8((u8)p[-3], (u8)p[-2], (u8)p[-1]); break;
    case 9:
        p += 3;
        gfxVtxPos3s8((s8)p[-3], (s8)p[-2], (s8)p[-1]); break;
    case 10:
        p += 2;
        gfxVtxPos2f32(((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 11:
        p += 2;
        gfxVtxPos2u16((u16)p[-2], (u16)p[-1]); break;
    case 12:
        p += 2;
        gfxVtxPos2s16((s16)p[-2], (s16)p[-1]); break;
    case 13:
        p += 2;
        gfxVtxPos2u8((u8)p[-2], (u8)p[-1]); break;
    case 14:
        p += 2;
        gfxVtxPos2s8((s8)p[-2], (s8)p[-1]); break;
    case 15:
        p += 1;
        gfxVtxPos1x16((u16)p[-1]); break;
    case 16:
        p += 1;
        gfxVtxPos1x8((u8)p[-1]); break;
    case 17:
        p += 3;
        gfxVtxNormal3f32(((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 18:
        p += 3;
        gfxVtxNormal3s16((s16)p[-3], (s16)p[-2], (s16)p[-1]); break;
    case 19:
        p += 3;
        gfxVtxNormal3s8((s8)p[-3], (s8)p[-2], (s8)p[-1]); break;
    case 20:
        p += 1;
        gfxVtxNormal1x16((u16)p[-1]); break;
    case 21:
        p += 1;
        gfxVtxNormal1x8((u8)p[-1]); break;
    case 22:
        p += 5;
        gfxVtxColor4u8(p[-5], (u8)p[-4], (u8)p[-3], (u8)p[-2], (u8)p[-1]); break;
    case 23:
        p += 4;
        gfxVtxColor(p[-4], (u8)p[-3], (u8)p[-2], (u8)p[-1]); break;
    case 24:
        p += 2;
        gfxVtxColor1u32(p[-2], p[-1]); break;
    case 25:
        p += 2;
        gfxVtxColor1u16(p[-2], (u16)p[-1]); break;
    case 26:
        p += 2;
        gfxVtxColor1x16(p[-2], (u16)p[-1]); break;
    case 27:
        p += 2;
        gfxVtxColor1x8(p[-2], (u8)p[-1]); break;
    case 28:
        p += 3;
        gfxVtxTexCoord(p[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 29:
        p += 3;
        gfxVtxTexCoord2u16(p[-3], (u16)p[-2], (u16)p[-1]); break;
    case 30:
        p += 3;
        gfxVtxTexCoord2s16(p[-3], (s16)p[-2], (s16)p[-1]); break;
    case 31:
        p += 3;
        gfxVtxTexCoord2u8(p[-3], (u8)p[-2], (u8)p[-1]); break;
    case 32:
        p += 3;
        gfxVtxTexCoord2s16(p[-3], (s8)p[-2], (s8)p[-1]); break;
    case 33:
        p += 2;
        gfxVtxTexCoord1x16(p[-2], (u16)p[-1]); break;
    case 34:
        p += 2;
        gfxVtxTexCoord1x8(p[-2], (u8)p[-1]); break;
    case 35:
        p += 1;
        gfxVtxMtxIdx((u8)p[-1]); break;
    case 36:
        p += 1;
        gfxSetLineWidth(((f32*)p)[-1]); break;
    case 37:
        p += 1;
        gfxSetPointSize(((f32*)p)[-1]); break;
    case 38:
        p += 2;
        gfxLoadTexture(p[-2], PC_PTR(p[-1])); break;
    case 39:
        {
            u32 a = *p++;
            u32 b = *p++;
            u32 c = *p++;

            gfxLoadTexMatrix(a, b, c, p);
            p += 12;
        }
        break;
    case 40:
        p += 1;
        gfxSetFlags(p[-1]); break;
    case 41:
        p += 1;
        gfxClearFlags(p[-1]); break;
    case 42:
        p += 1;
        GSgfxDLDraw((struct GSgfxDLCapture*)PC_PTR(p[-1])); break;
    case 43:
        p += 3;
        gfxSetBlendMode(p[-3], p[-2], p[-1]); break;
    case 44:
        p += 1;
        gfxSetBlendModeRaw(p[-1]); break;
    case 45:
        p += 2;
        gfxSetDstAlpha(p[-2], (u8)p[-1]); break;
    case 46:
        p += 3;
        gfxSetUpdates(p[-3], p[-2], p[-1]); break;
    case 47:
        p += 3;
        gfxSetZMode(p[-3], p[-2], p[-1]); break;
    case 48:
        p += 6;
        gfxSetAlphaCompare(p[-6], p[-5], (u8)p[-4], p[-3], p[-2], (u8)p[-1]); break;
    case 49:
        p += 1;
        gfxSetDither(p[-1]); break;
    case 50:
        p += 1;
        gfxSetCullMode(p[-1]); break;
    case 51:
        p += 1;
        gfxSetClipMode(p[-1]); break;
    case 52:
        p += 1;
        gfxSetCoPlanar(p[-1]); break;
    case 53:
        p += 1;
        fn_800D9ED8(p[-1]); break;
    case 54:
        p += 1;
        fn_800D9E4C(p[-1]); break;
    case 55:
        p += 4;
        gfxSetScissor((u16)p[-4], (u16)p[-3], (u16)p[-2], (u16)p[-1]); break;
    case 56:
        p += 4;
        gfxSetViewport((u16)p[-4], (u16)p[-3], (u16)p[-2], (u16)p[-1]); break;
    case 57:
        p += 4;
        gfxSetPerspective(((f32*)p)[-4], ((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 58:
        p += 4;
        gfxSetOrtho(((f32*)p)[-4], ((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 59:
        gfxLoadIdentity(); break;
    case 60:
        p += 3;
        gfxRotateXYZ(((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 61:
        p += 3;
        gfxTranslateMatrix(((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 62:
        p += 3;
        gfxScaleMatrix(((f32*)p)[-3], ((f32*)p)[-2], ((f32*)p)[-1]); break;
    case 63:
        gfxPushLoadMatrix(p); p += 12; break;
    case 64:
        gfxMultMatrix(p); p += 12; break;
    case 65:
        gfxPushMultMatrix(p); p += 12; break;
    case 66:
        gfxPopMatrix(); break;
    case 67:
        argument = (u8)*p++; gfxSetMatrix(argument, p); p += 12; break;
    case 68:
        p += 2;
        gfxGetMatrix((u8)p[-2], p[-1]); break;
    case 69:
        gfxFlushMatrix(); break;
    case 70:
        p += 1;
        gfxSetVtxDesc((void*)PC_PTR(p[-1])); break; /* the recorded word is a descriptor pointer */
    case 71:
        p += 2;
        gfxSetVtxDescRaw((GSVtxDesc*)PC_PTR(p[-2]), (u16)p[-1]); break;
    case 72:
        p += 5;
        gfxSetTevStage(p[-5], p[-4], p[-3], p[-2], p[-1]); break;
    case 73:
        p += 1;
        fn_800DC1D4((u8)p[-1]); break;
    case 74:
        p += 6;
        fn_800DC14C(p[-6], p[-5], p[-4], p[-3], (u8)p[-2], p[-1]); break;
    case 75:
        p += 5;
        fn_800DC0D4(p[-5], p[-4], p[-3], p[-2], p[-1]); break;
    case 76:
        p += 6;
        fn_800DC04C(p[-6], p[-5], p[-4], p[-3], (u8)p[-2], p[-1]); break;
    case 77:
        p += 5;
        fn_800DBFD4(p[-5], p[-4], p[-3], p[-2], p[-1]); break;
    case 78:
        p += 2;
        fn_800DBF78(p[-2], p[-1]); break;
    case 79:
        p += 2;
        fn_800DBF1C(p[-2], p[-1]); break;
    case 80:
        argument = *p++;
        memcpy(&copied, p, sizeof(copied));
        p++;
        fn_800DBEB4(argument, copied);
        break;
    case 81:
        p += 1;
        fn_800DBE5C(p[-1]); break;
    case 82:
        p += 10;
        fn_800DBD70(p[-8], p[-7], p[-6], p[-5], p[-4], p[-3], p[-2], (u8)p[-1], (u8)p[-10], p[-9]); break;
    case 83:
        p += 5;
        fn_800DBCE4(p[-5], p[-4], (u8)p[-3], (u8)p[-2], p[-1]); break;
    case 84:
        p += 10;
        fn_800DBBFC(p[-8], p[-7], (u16)p[-6], (u16)p[-5], (u16)p[-4], (u16)p[-3], p[-2], p[-1], p[-10], p[-9]); break;
    case 85:
        p += 3;
        fn_800DBB84(p[-3], p[-2], p[-1]); break;
    case 86:
        p += 3;
        fn_800DBB0C(p[-3], p[-2], p[-1]); break;
    case 87:
        p += 1;
        fn_800DBAA4(p[-1]); break;
    case 88:
        p += 1;
        fn_800DBA54((u8)p[-1]); break;
    case 89:
        p += 3;
        fn_800DB9F0(p[-3], p[-2], p[-1]); break;
    case 90:
        p += 3;
        fn_800DB988(p[-3], p[-2], p[-1]); break;
    case 91:
        {
            u32* data;

            argument = *p++;
            /* RULE-EXCEPTION(title-path): pure copy of the cursor used only for scheduling - see docs/RULE_EXCEPTIONS.md
             * Retail advances the cursor past the 25-word block before the
             * call. */
            data = p;
            p += 25;
            fn_800DB900(argument, data, (s8)data[24]);
        }
        break;
#ifdef TARGET_PC
    case 0x7E:
        /* OrrePort, frame smoothing's 2D at 120 (round 11; pc_orre_2d_tag in src/game/pc_debug_game.c): a PC-only
         * tag recorded around a named 2D piece (the HP / EXP bars' fills, the pause menu's pieces). Played back here,
         * in order with the draws it names, it tells OrreHook_2DPiece (kind, variant, id, value as float bits). The
         * tag is only recorded while the hook is set, so with it unset this case never runs (the vanilla buffer).
         * Observation only: no GX state, no game state. All platforms. */
        p += 4;
        {
            extern void (*OrreHook_2DPiece)(u32 kind, u32 variant, u32 id, u32 valueBits);
            if (OrreHook_2DPiece != NULL) {
                OrreHook_2DPiece(p[-4], p[-3], p[-2], p[-1]);
            }
        }
        break;
#endif
    }

    return p;
}
#endif


/* fn_800D4F98 (request recorder): src/game/gs_gfx_exact_800D4F98.c */

#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D45F8_800D461C)
#if 0
asm void fn_800D45F8(void) {
#include "src/game/gs_render_fn_800D45F8.inc"
}
#else
u32 fn_800D45F8(void) { return ((u32)gsgfxState->recordMode); }
#endif

#if 0
asm void fn_800D4604(void) {
#include "src/game/gs_render_fn_800D4604.inc"
}
#else
void fn_800D4604(u32 val) { gsgfxState->recordMode = val; }
#endif

void fn_800D4610(u8 val) {
    GSgfxState* state = gsgfxState;

    state->overlayColorUpdate = val;
}
#endif

/* gfxAllocRequestBuffer (request buffer allocator): src/game/gs_gfx_exact_800D4F98.c */

#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D55D0_800D56C0)
/* gfxSetPointSize/gfxSetLineWidth: the range and scale constants are this unit's
 * own literals (.sdata2 0x8047CA30-0x8047CA40: 0.0f, 42.5f, 6.0f, pooled in
 * first-use order and shared by both functions). As literals MWCC may load
 * the lower bound before the LR save, as retail does; an extern variable
 * load cannot be scheduled above that store. */
#if 0
asm void fn_800D55D0(void) {
#include "src/game/gs_render_fn_800D55D0.inc"
}
#else
void gfxSetPointSize(f32 val) {
    if (val < 0.0f || val > 42.5f) return;
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x25, 0xb, val); }
    else {
        u32 tmp;
        tmp = (u32)(s32)(6.0f * val);
        GXSetPointSize(tmp, 0);
    }
}
#endif

#if 0
asm void fn_800D5648(void) {
#include "src/game/gs_render_fn_800D5648.inc"
}
#else
void gfxSetLineWidth(f32 val) {
    if (val < 0.0f || val > 42.5f) return;
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x24, 0xb, val); }
    else {
        u32 tmp;
        tmp = (u32)(s32)(6.0f * val);
        GXSetLineWidth(tmp, 0);
    }
}
#endif
#endif

#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D56C0_800D67BC)
#if 0
asm void fn_800D56C0(void) {
#include "src/game/gs_render_fn_800D56C0.inc"
}
#else
void gfxVtxMtxIdx(u8 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x23, 1, (u8)val);
    } else {
        gsgfxState->emitMtxIdx = (GSVtxEmitFn)_gfxEmitMtxIdx;
        gsgfxState->mtxIdx = val;
    }
}
#endif

#if 0
asm void fn_800D5724(void) {
#include "src/game/gs_render_fn_800D5724.inc"
}
#else
void gfxVtxTexCoord1x8(u32 idx, u8 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x22, 2, idx, (u32)val);
    } else {
        gsgfxState->emitTex[idx] = (GSVtxEmitFn)_gfxEmitTexCoord1x8;
        gsgfxState->tex[idx].b[0] = val;
    }
}
#endif

#if 0
asm void fn_800D579C(void) {
#include "src/game/gs_render_fn_800D579C.inc"
}
#else
void gfxVtxTexCoord1x16(u32 idx, u16 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x21, 2, idx, (u32)val);
    } else {
        gsgfxState->emitTex[idx] = (GSVtxEmitFn)_gfxEmitTexCoord1x16;
        gsgfxState->tex[idx].s[0] = val;
    }
}
#endif

#if 0
asm void fn_800D5814(void) {
#include "src/game/gs_render_fn_800D5814.inc"
}
#else
void gfxVtxTexCoord2u8(u32 idx, u8 a, u8 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x1f, 3, idx, (u32)a, (u32)b); }
    else {
        state->emitTex[idx] = (GSVtxEmitFn)_gfxEmitTexCoord2u8;
        gsgfxState->tex[idx].b[0] = a;
        gsgfxState->tex[idx].b[1] = b;
    }
}
#endif

#if 0
asm void fn_800D58A0(void) {
#include "src/game/gs_render_fn_800D58A0.inc"
}
#else
void gfxVtxTexCoord2s16(u32 idx, s16 a, s16 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x1e, 3, idx, (s32)a, (s32)b); }
    else {
        state->emitTex[idx] = (GSVtxEmitFn)_gfxEmitTexCoord2s16;
        gsgfxState->tex[idx].s[0] = a;
        gsgfxState->tex[idx].s[1] = b;
    }
}
#endif

#if 0
asm void fn_800D592C(void) {
#include "src/game/gs_render_fn_800D592C.inc"
}
#else
void gfxVtxTexCoord2u16(u32 idx, u16 a, u16 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x1d, 3, idx, (u32)a, (u32)b); }
    else {
        state->emitTex[idx] = (GSVtxEmitFn)_gfxEmitTexCoord2u16;
        gsgfxState->tex[idx].s[0] = a;
        gsgfxState->tex[idx].s[1] = b;
    }
}
#endif

#if 0
asm void fn_800D59B8(void) {
#include "src/game/gs_render_fn_800D59B8.inc"
}
#else
void gfxVtxTexCoord(u32 idx, f32 x, f32 y) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x1c, 0xf, idx, x, y); }
    else {
        state->emitTex[idx] = (GSVtxEmitFn)_gfxEmitTexCoord2f32;
        gsgfxState->tex[idx].f[0] = x;
        gsgfxState->tex[idx].f[1] = y;
    }
}
#endif

#if 0
asm void fn_800D5A38(void) {
#include "src/game/gs_render_fn_800D5A38.inc"
}
#else
void gfxVtxColor1x8(u32 idx, u8 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x1b, 2, idx, (u32)val);
    } else {
        gsgfxState->emitClr[idx] = (GSVtxEmitFn)_gfxEmitColor1x8;
        gsgfxState->clr[idx].b[0] = val;
    }
}
#endif

#if 0
asm void fn_800D5AB0(void) {
#include "src/game/gs_render_fn_800D5AB0.inc"
}
#else
void gfxVtxColor1x16(u32 idx, u16 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x1a, 2, idx, (u32)val);
    } else {
        gsgfxState->emitClr[idx] = (GSVtxEmitFn)_gfxEmitColor1x16;
        gsgfxState->clr[idx].s = val;
    }
}
#endif

#if 0
asm void fn_800D5B28(void) {
#include "src/game/gs_render_fn_800D5B28.inc"
}
#else
void gfxVtxColor1u16(u32 idx, u16 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x19, 2, idx, (u32)val);
    } else {
        gsgfxState->emitClr[idx] = (GSVtxEmitFn)_gfxEmitColor1u16;
        gsgfxState->clr[idx].s = val;
    }
}
#endif

#if 0
asm void fn_800D5BA0(void) {
#include "src/game/gs_render_fn_800D5BA0.inc"
}
#else
void gfxVtxColor1u32(u32 idx, u32 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x18, 2, idx, val);
    } else {
        gsgfxState->emitClr[idx] = (GSVtxEmitFn)_gfxEmitColor1u32;
        gsgfxState->clr[idx].w = val;
    }
}
#endif

#if 0
asm void fn_800D5C18(void) {
#include "src/game/gs_render_fn_800D5C18.inc"
}
#else
void gfxVtxColor(u32 idx, u8 a, u8 b, u8 c) {
    GSgfxState* state = gsgfxState;
    u32 off;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0x17, 4, idx, (u32)a, (u32)b, (u32)c);
    } else {
        off = idx * 0xc;
        {
            GSgfxState* s = gsgfxState;
            s->emitClr[idx] = (GSVtxEmitFn)_gfxEmitColor3u8;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[0]) + off) = a;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[1]) + off) = b;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[2]) + off) = c;
        }
    }
}
#endif

#if 0
asm void fn_800D5CB8(void) {
#include "src/game/gs_render_fn_800D5CB8.inc"
}
#else
void gfxVtxColor4u8(u32 idx, u8 a, u8 b, u8 c, u8 d) {
    GSgfxState* state = gsgfxState;
    u32 off;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0x16, 5, idx, (u32)a, (u32)b, (u32)c, (u32)d);
    } else {
        off = idx * 0xc;
        {
            GSgfxState* s = gsgfxState;
            s->emitClr[idx] = (GSVtxEmitFn)_gfxEmitColor4u8;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[0]) + off) = a;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[1]) + off) = b;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[2]) + off) = c;
        }
        {
            u8* s = (u8*)gsgfxState;
            *(s + GSGFX_OFFSET(clr[0].b[3]) + off) = d;
        }
    }
}
#endif

#if 0
asm void fn_800D5D6C(void) {
#include "src/game/gs_render_fn_800D5D6C.inc"
}
#else
void gfxVtxNormal1x8(u8 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x15, 1, (u8)val);
    } else {
        gsgfxState->emitNrm = (GSVtxEmitFn)_gfxEmitNormal1x8;
        gsgfxState->nrm.b[0] = val;
    }
}
#endif

#if 0
asm void fn_800D5DD0(void) {
#include "src/game/gs_render_fn_800D5DD0.inc"
}
#else
void gfxVtxNormal1x16(u16 val) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x14, 1, (u16)val);
    } else {
        gsgfxState->emitNrm = (GSVtxEmitFn)_gfxEmitNormal1x16;
        gsgfxState->nrm.s[0] = val;
    }
}
#endif

#if 0
asm void fn_800D5E34(void) {
#include "src/game/gs_render_fn_800D5E34.inc"
}
#else
void gfxVtxNormal3s8(s8 a, s8 b, s8 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x13, 3, (s32)a, (s32)b, (s32)c); }
    else {
        state->emitNrm = (GSVtxEmitFn)_gfxEmitNormal3s8;
        gsgfxState->nrm.b[0] = a;
        gsgfxState->nrm.b[1] = b;
        gsgfxState->nrm.b[2] = c;
    }
}
#endif

#if 0
asm void fn_800D5EB4(void) {
#include "src/game/gs_render_fn_800D5EB4.inc"
}
#else
void gfxVtxNormal3s16(s16 a, s16 b, s16 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x12, 3, (s32)a, (s32)b, (s32)c); }
    else {
        state->emitNrm = (GSVtxEmitFn)_gfxEmitNormal3s16;
        gsgfxState->nrm.s[0] = a;
        gsgfxState->nrm.s[1] = b;
        gsgfxState->nrm.s[2] = c;
    }
}
#endif

#if 0
asm void fn_800D5F34(void) {
#include "src/game/gs_render_fn_800D5F34.inc"
}
#else
void gfxVtxNormal3f32(f32 x, f32 y, f32 z) {
    if (!gsgfxState->captureActive && gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x11, 0xd, x, y, z);
    } else {
        gsgfxState->emitNrm = (GSVtxEmitFn)_gfxEmitNormal3f32;
        gsgfxState->nrm.f[0] = x;
        gsgfxState->nrm.f[1] = y;
        gsgfxState->nrm.f[2] = z;
    }
}
#endif

#if 0
asm void fn_800D5FA4(void) {
#include "src/game/gs_render_fn_800D5FA4.inc"
}
#else
void gfxVtxPos1x8(u8 val) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0x10, 1, (u32)val); }
    else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos1x8;
        gsgfxState->pos.b[0] = val;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6028(void) {
#include "src/game/gs_render_fn_800D6028.inc"
}
#else
void gfxVtxPos1x16(u16 val) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) { gfxRecordCommand(0xf, 1, (u32)val); }
    else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos1x16;
        gsgfxState->pos.s[0] = val;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D60AC(void) {
#include "src/game/gs_render_fn_800D60AC.inc"
}
#else
void gfxVtxPos2s8(s8 a, s8 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0xe, 2, (s32)a, (s32)b);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos2s8;
        gsgfxState->pos.b[0] = a;
        gsgfxState->pos.b[1] = b;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6148(void) {
#include "src/game/gs_render_fn_800D6148.inc"
}
#else
void gfxVtxPos2u8(u8 a, u8 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0xd, 2, (u32)a, (u32)b);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos2u8;
        gsgfxState->pos.b[0] = a;
        gsgfxState->pos.b[1] = b;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D61E4(void) {
#include "src/game/gs_render_fn_800D61E4.inc"
}
#else
void gfxVtxPos2s16(s16 a, s16 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0xc, 2, (s32)a, (s32)b);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos2s16;
        gsgfxState->pos.s[0] = a;
        gsgfxState->pos.s[1] = b;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6280(void) {
#include "src/game/gs_render_fn_800D6280.inc"
}
#else
void gfxVtxPos2u16(u16 a, u16 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0xb, 2, (u32)a, (u32)b);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos2u16;
        gsgfxState->pos.s[0] = a;
        gsgfxState->pos.s[1] = b;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D631C(void) {
#include "src/game/gs_render_fn_800D631C.inc"
}
#else
void gfxVtxPos2f32(f32 a, f32 b) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(0xa, 0xc, a, b);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos2f32;
        gsgfxState->pos.f[0] = a;
        gsgfxState->pos.f[1] = b;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D63B0(void) {
#include "src/game/gs_render_fn_800D63B0.inc"
}
#else
void gfxVtxPos3s8(s8 a, s8 b, s8 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(9, 3, (s32)a, (s32)b, (s32)c);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos3s8;
        gsgfxState->pos.b[0] = a;
        gsgfxState->pos.b[1] = b;
        gsgfxState->pos.b[2] = c;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6464(void) {
#include "src/game/gs_render_fn_800D6464.inc"
}
#else
void gfxVtxPos3u8(u8 a, u8 b, u8 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(8, 3, (u32)a, (u32)b, (u32)c);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos3u8;
        gsgfxState->pos.b[0] = a;
        gsgfxState->pos.b[1] = b;
        gsgfxState->pos.b[2] = c;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6518(void) {
#include "src/game/gs_render_fn_800D6518.inc"
}
#else
void gfxVtxPos3s16(s16 a, s16 b, s16 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(7, 3, (s32)a, (s32)b, (s32)c);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos3s16;
        gsgfxState->pos.s[0] = a;
        gsgfxState->pos.s[1] = b;
        gsgfxState->pos.s[2] = c;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D65CC(void) {
#include "src/game/gs_render_fn_800D65CC.inc"
}
#else
void gfxVtxPos3u16(u16 a, u16 b, u16 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(6, 3, (u32)a, (u32)b, (u32)c);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos3u16;
        gsgfxState->pos.s[0] = a;
        gsgfxState->pos.s[1] = b;
        gsgfxState->pos.s[2] = c;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6680(void) {
#include "src/game/gs_render_fn_800D6680.inc"
}
#else
void gfxVtxPos(f32 a, f32 b, f32 c) {
    GSgfxState* state = gsgfxState;
    if (!state->captureActive && state->recordMode == 1) {
        gfxRecordCommand(5, 0xd, a, b, c);
    } else {
        gfxFlushPendingVertex();
        gsgfxState->emitPos = (GSVtxEmitFn)_gfxEmitPos3f32;
        gsgfxState->pos.f[0] = a;
        gsgfxState->pos.f[1] = b;
        gsgfxState->pos.f[2] = c;
        gsgfxState->vtxPending = 1;
    }
}
#endif

#if 0
asm void fn_800D6728(void) {
#include "src/game/gs_render_fn_800D6728.inc"
}
#else
void gfxEnd(void) {
    GSgfxState* state = gsgfxState;
    if (state->captureActive == 1) {
        gfxFlushPendingVertex();
        return;
    }
    if (state->recordMode == 1) {
        gfxRecordCommand(3, 0);
        return;
    }
    gfxFlushPendingVertex(); GS_PC_GXEND(gsgfxState);
    {
        GSgfxState* s = gsgfxState;
        if (s->layerDraw == s->layerCur &&
            (s->drawMask & s->drawEnable) &&
            s->vtxDesc == s->defaultVtxDesc) {
            s->vtxDesc = NULL;
        }
    }
}
#endif
#endif

#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D67BC_800D6A00)
extern u8 lbl_80314350[];
extern u8 lbl_804001F0[];
#if 0
asm void fn_800D67BC(void) {
#include "src/game/gs_render_fn_800D67BC.inc"
}
#else
/* Linked owner: gs_gfx_candidate_800D67BC.c (standalone carve). */
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

    gfxLoadVtxDesc(gsgfxState->vtxDesc);
    gfxSetupChannelsTev(gsgfxState->vtxDesc);
    GXBegin(lbl_80314350[((u32)gsgfxState->primType)],
                gsgfxState->vtxDesc->vtxFmt, vertCount);
    fn_800D6A80(vertCount, gsgfxState->primType,
                (u32*)(lbl_804001F0 + 0xc), (u32*)(lbl_804001F0 + 0x4));
}
#endif
#endif

#if !defined(GS_GFX_RANGE_SPLIT) || \
    defined(GS_GFX_RANGE_800D6A00_800D6B00)
#if 0
asm void fn_800D6A00(void) {
#include "src/game/gs_render_fn_800D6A00.inc"
}
#else
void gfxSetPrimitive(u32 val) {
    if (gsgfxState->captureActive == 1) { gsgfxState->capturePrimType = val; }
    else if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x1, 1, val); }
    else { gsgfxState->primType = val; }
}
#endif

#if 0
asm void fn_800D6A5C(void) {
#include "src/game/gs_render_fn_800D6A5C.inc"
}
#else
void gfxAddDrawStats(u32 a, u32 b) { *(u32*)(lbl_804001F0 + 0xc) += a; *(u32*)(lbl_804001F0 + 0x4) += b; }
#endif

#if 0
asm void fn_800D6A80(void) {
#include "src/game/gs_render_fn_800D6A80.inc"
}
#else
void fn_800D6A80(u16 vertCount, s32 type, u32* totalVerts, u32* totalPrims) {
    *totalVerts += vertCount;
    switch (type) {
        case 3: *totalPrims += vertCount / 3; break;
        case 4:
        case 5: *totalPrims += vertCount - 2; break;
        case 6:
        case 7: *totalPrims += (vertCount >> 1); break;
    }
}
#endif
#endif
