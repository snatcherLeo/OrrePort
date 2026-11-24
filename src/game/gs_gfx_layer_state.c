/**
 * @file gs_gfx_layer_state.c
 * @brief GS render engine segment -- split from gs_render.c.
 *
 * XD source unit: game/pxdvs/GSAPI/GSgfxM/layerState.cpp (structural)
 * Address range: 0x800D6B00 - 0x800D9AF0 (62 functions)
 *
 * No anchors but strong structural mirror of XD layerState.cpp (0x3D8C): our 0x730 lead fn ~ SetupGPUState (XD 0x1510 after inlining), a 31-fn tiny accessor farm (0x14-0x34) = dirty-state helpers, then ~20 setters sized 0x78-0x1D8 mirroring the GSgfx_GCSet{Fog,AlphaCompare,BlendMode,TevSwap,TevKColor,Ind*,Tev*,MatColor,AmbColor,ChanCtrl,Scissor,Projection} farm, closing with renderState trio 0x910/0x400/0x4B4 mirroring XD SetupRenderState/renderState_NL_VC_T/NL_NVC_T/NL_NVC_NT...
 *
 * Split from src/game/gs_render.c (physical XD source-unit split).
 * The dead #ifdef PCPORT reference block (never defined in configure.py,
 * same situation as gs_gfx.c) was stripped during the split.
 */

#include "dolphin/types.h"
#include "game/gs_gfx_layer.h"
#include "game/gs_gfx_override.h"

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
extern void gfxRecordCommand(u32, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u16);
#endif
extern void gfxSetupChannelsTev(GSVtxDesc*);

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
extern void gfxVtxDescClear(GSVtxDesc*);
extern void gfxVtxDescSetAttr(GSVtxDesc*, u32, u32, u32, u32, u8, void*, u8);
extern void fn_800D7940(GSVtxDesc*, u16);
extern void gfxLoadVtxDesc(GSVtxDesc*);
extern void _gfxFlushTevState(void);
extern void gfxSetTevOp(u32, s32);
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

extern u8 lbl_804001F0[];
extern void fn_800D6A80(u16, s32, u32*, u32*);
extern u8 lbl_804007E8[];
extern void GXSetVtxAttrFmt(u32, u32, u32, u32, u8);
extern void GXClearVtxDesc(void);
extern void GXSetVtxDesc(u32, u32);
#ifdef TARGET_PC /* PC: the port's 3-argument GXSetArray (port/src/pc_gx_arrays.c); Aurora's GXSetArray takes 5 */
#define GXSetArray fn_800B84E0
#endif
extern void GXSetArray(u32, void*, u8);
extern u8 lbl_80314370[];
extern u8 lbl_803143B4[];
extern u8 lbl_803143D8[];
extern u8 lbl_803143A8[];
extern u32 lbl_8047AAB0;
extern u32 lbl_8047AAAC;
extern u8 lbl_803144D0[];
extern u32 lbl_8047AAB4;
extern u16 lbl_8047AAA8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GScameraGetActiveCamera(void);
#endif
extern void* GScameraGetMatrixPtr(void* camera);             /* the camera's view matrix */
extern void* GScameraGetInvMatrixPtr(void* camera);             /* view x projection */
extern void* GScameraGetProjMatrixPtr(void* camera);
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadPosMtxImm(void*, u32);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadNrmMtxImm(void*, u32);
#endif
extern void GXSetCurrentMtx(u32);
extern u8 lbl_8047AAC8;
extern u8 lbl_80314610[];
extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_80400948[];
extern GSMtxStackEntry* lbl_8047AAC4;
extern GSMtxStackEntry* lbl_8047AABC;
extern u16 lbl_8047AAB8;
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXSetTexCoordGen2(u32, u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXLoadTexMtxImm(void*, u32, u32);
#endif
extern u8 lbl_80314404[];
extern u8 lbl_80314454[];
extern u8 lbl_803144A8[];
extern u8 lbl_80314424[];
extern void GXInitTexObjWrapMode();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GXInitTexObjLOD();
#endif
extern void GXLoadTlut();
extern void GXLoadTexObj();
extern u8 lbl_80314530[];
extern u32 lbl_8047CA40;
extern u32 lbl_8047CA48;
extern u8 lbl_80314510[];
extern u8 lbl_803144F0[];
extern u32 lbl_80478AE0[];
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



/* ==================================================================
 * Matrix accessor functions (0x800D7230 - 0x800D75D0)
 *
 * A block of ~40 tiny functions (0x14-0x24 bytes each) that get/set
 * individual matrix elements, indices, and stack pointers.
 *
 * These follow two patterns:
 *   Pattern A (getter, 0x1C bytes):
 *     lwz r3, offset(r13/r0)   ; load from sda
 *     lwz r3, field(r3)        ; read struct field
 *     blr
 *
 *   Pattern B (setter, 0x24 bytes):
 *     lwz r4, offset(r13/r0)   ; load state ptr
 *     stw r3, field(r4)        ; store value
 *     blr
 * ================================================================== */

#if defined(GS_GFX_LAYER_800D7230_800D76A8)
void _gfxEmitMtxIdx(void) {
    GSgfxState* state = gsgfxState;
    u8 value = state->mtxIdx;

    GS_WGPIPE_U8(value + (value << 1));
}
#endif

/* ==================================================================
 * fn_800D892C -- GSgfx_ConfigurePipeline
 *
 * Configure the full GX rendering pipeline. At 2320 bytes, this is
 * a major setup function. Called once per render mode change.
 * ================================================================== */
/* fn_800D892C: owner is the standalone gs_gfx_layer_candidate_800D892C.c. */

/* gfxFlushPendingVertex: owner is the standalone gs_gfx_layer_candidate_800D6B00.c. */

#if defined(GS_GFX_LAYER_800D7230_800D76A8)
#if 0
asm void fn_800D724C(void) {
#include "src/game/gs_render_fn_800D724C.inc"
}
#else
void _gfxEmitTexCoord1x8(u32 idx) { GS_WGPIPE_U8(gsgfxState->tex[idx].b[0]); }
#endif

#if 0
asm void fn_800D7268(void) {
#include "src/game/gs_render_fn_800D7268.inc"
}
#else
void _gfxEmitTexCoord1x16(u32 idx) { GS_WGPIPE_U16(gsgfxState->tex[idx].s[0]); }
#endif

#if 0
asm void fn_800D7284(void) {
#include "src/game/gs_render_fn_800D7284.inc"
}
#else
void _gfxEmitTexCoord2s8(u32 idx) { u8 v = gsgfxState->tex[idx].b[0]; GS_WGPIPE_U8(v); GS_WGPIPE_U8(v); }
#endif

#if 0
asm void fn_800D72A4(void) {
#include "src/game/gs_render_fn_800D72A4.inc"
}
#else
void _gfxEmitTexCoord2u8(u32 idx) { u8 v = gsgfxState->tex[idx].b[0]; GS_WGPIPE_U8(v); GS_WGPIPE_U8(v); }
#endif

#if 0
asm void fn_800D72C4(void) {
#include "src/game/gs_render_fn_800D72C4.inc"
}
#else
void _gfxEmitTexCoord2s16(u32 idx) { u16 v = gsgfxState->tex[idx].s[0]; GS_WGPIPE_U16(v); GS_WGPIPE_U16(v); }
#endif

#if 0
asm void fn_800D72E4(void) {
#include "src/game/gs_render_fn_800D72E4.inc"
}
#else
void _gfxEmitTexCoord2u16(u32 idx) { u16 v = gsgfxState->tex[idx].s[0]; GS_WGPIPE_U16(v); GS_WGPIPE_U16(v); }
#endif

#if 0
asm void fn_800D7304(void) {
#include "src/game/gs_render_fn_800D7304.inc"
}
#else
void _gfxEmitTexCoord2f32(u32 idx) {
    u8* base = (u8*)gsgfxState + idx * sizeof(GSVtxValue2);
    f32 a = *(f32*)(base + GSGFX_OFFSET(tex[0].f[0]));
    f32 b = *(f32*)(base + GSGFX_OFFSET(tex[0].f[1]));
    GS_WGPIPE_F32(a);
    GS_WGPIPE_F32(b);
}
#endif

#if 0
asm void fn_800D7328(void) {
#include "src/game/gs_render_fn_800D7328.inc"
}
#else
void _gfxEmitColor1x8(u32 idx) { GS_WGPIPE_U8(gsgfxState->clr[idx].b[0]); }
#endif

#if 0
asm void fn_800D7344(void) {
#include "src/game/gs_render_fn_800D7344.inc"
}
#else
void _gfxEmitColor1x16(u32 idx) { GS_WGPIPE_U16(gsgfxState->clr[idx].s); }
#endif

#if 0
asm void fn_800D7360(void) {
#include "src/game/gs_render_fn_800D7360.inc"
}
#else
void _gfxEmitColor1u16(u32 idx) { GS_WGPIPE_U16(gsgfxState->clr[idx].s); }
#endif

#if 0
asm void fn_800D737C(void) {
#include "src/game/gs_render_fn_800D737C.inc"
}
#else
void _gfxEmitColor1u32(u32 idx) { GS_WGPIPE_U32(gsgfxState->clr[idx].w); }
#endif

#if 0
asm void fn_800D7398(void) {
#include "src/game/gs_render_fn_800D7398.inc"
}
#else
void _gfxEmitColor3u8(u32 idx) {
    u8 b;
    u8 c;
    u8 a;
    u8* base = (u8*)gsgfxState + idx * sizeof(GSVtxColor);
    c = *(u8*)(base + GSGFX_OFFSET(clr[0].b[2]));
    b = *(u8*)(base + GSGFX_OFFSET(clr[0].b[1]));
    a = *(u8*)(base + GSGFX_OFFSET(clr[0].b[0]));
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
    GS_WGPIPE_U8(c);
}
#endif

#if 0
asm void fn_800D73C4(void) {
#include "src/game/gs_render_fn_800D73C4.inc"
}
#else
void _gfxEmitColor4u8(u32 idx) {
    u8 b;
    u8 c;
    u8 d;
    u8 a;
    u8* base = (u8*)gsgfxState + idx * sizeof(GSVtxColor);
    d = *(u8*)(base + GSGFX_OFFSET(clr[0].b[3]));
    c = *(u8*)(base + GSGFX_OFFSET(clr[0].b[2]));
    b = *(u8*)(base + GSGFX_OFFSET(clr[0].b[1]));
    a = *(u8*)(base + GSGFX_OFFSET(clr[0].b[0]));
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
    GS_WGPIPE_U8(c);
    GS_WGPIPE_U8(d);
}
#endif

#if 0
asm void fn_800D73F8(void) {
#include "src/game/gs_render_fn_800D73F8.inc"
}
#else
void _gfxEmitNormal1x8(void) { GS_WGPIPE_U8(gsgfxState->nrm.b[0]); }
#endif

#if 0
asm void fn_800D740C(void) {
#include "src/game/gs_render_fn_800D740C.inc"
}
#else
void _gfxEmitNormal1x16(void) { GS_WGPIPE_U16(gsgfxState->nrm.s[0]); }
#endif

#if 0
asm void fn_800D7420(void) {
#include "src/game/gs_render_fn_800D7420.inc"
}
#else
void _gfxEmitNormal3s8(void) {
    u8 a, b, c;
    u8 *base = (u8*)gsgfxState;
    c = base[GSGFX_OFFSET(nrm.b[2])];
    b = base[GSGFX_OFFSET(nrm.b[1])];
    a = base[GSGFX_OFFSET(nrm.b[0])];
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
    GS_WGPIPE_U8(c);
}
#endif

#if 0
asm void fn_800D7444(void) {
#include "src/game/gs_render_fn_800D7444.inc"
}
#else
void _gfxEmitNormal3s16(void) {
    u16 a, b, c;
    u8 *base = (u8*)gsgfxState;
    c = *(u16*)(base + GSGFX_OFFSET(nrm.s[2]));
    b = *(u16*)(base + GSGFX_OFFSET(nrm.s[1]));
    a = *(u16*)(base + GSGFX_OFFSET(nrm.s[0]));
    GS_WGPIPE_U16(a);
    GS_WGPIPE_U16(b);
    GS_WGPIPE_U16(c);
}
#endif

#if 0
asm void fn_800D7468(void) {
#include "src/game/gs_render_fn_800D7468.inc"
}
#else
void _gfxEmitNormal3f32(void) {
    f32 b;
    f32 c;
    f32 a;
    c = gsgfxState->nrm.f[2];
    b = gsgfxState->nrm.f[1];
    a = gsgfxState->nrm.f[0];
    GS_WGPIPE_F32(a);
    GS_WGPIPE_F32(b);
    GS_WGPIPE_F32(c);
}
#endif

#if 0
asm void fn_800D748C(void) {
#include "src/game/gs_render_fn_800D748C.inc"
}
#else
void _gfxEmitPos1x8(void) { GS_WGPIPE_U8(gsgfxState->pos.b[0]); }
#endif

#if 0
asm void fn_800D74A0(void) {
#include "src/game/gs_render_fn_800D74A0.inc"
}
#else
void _gfxEmitPos1x16(void) { GS_WGPIPE_U16(gsgfxState->pos.s[0]); }
#endif

#if 0
asm void fn_800D74B4(void) {
#include "src/game/gs_render_fn_800D74B4.inc"
}
#else
void _gfxEmitPos2s8(void) {
    u8 a, b;
    u8 *base = (u8*)gsgfxState;
    b = *(u8*)(base + GSGFX_OFFSET(pos.b[1]));
    a = *(u8*)(base + GSGFX_OFFSET(pos.b[0]));
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
}
#endif

#if 0
asm void fn_800D74D0(void) {
#include "src/game/gs_render_fn_800D74D0.inc"
}
#else
void _gfxEmitPos2u8(void) {
    u8 a, b;
    u8 *base = (u8*)gsgfxState;
    b = *(u8*)(base + GSGFX_OFFSET(pos.b[1]));
    a = *(u8*)(base + GSGFX_OFFSET(pos.b[0]));
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
}
#endif

#if 0
asm void fn_800D74EC(void) {
#include "src/game/gs_render_fn_800D74EC.inc"
}
#else
void _gfxEmitPos2s16(void) {
    u16 a, b;
    u8 *base = (u8*)gsgfxState;
    b = *(u16*)(base + GSGFX_OFFSET(pos.s[1]));
    a = *(u16*)(base + GSGFX_OFFSET(pos.s[0]));
    GS_WGPIPE_U16(a);
    GS_WGPIPE_U16(b);
}
#endif

#if 0
asm void fn_800D7508(void) {
#include "src/game/gs_render_fn_800D7508.inc"
}
#else
void _gfxEmitPos2u16(void) {
    u16 a, b;
    u8 *base = (u8*)gsgfxState;
    b = *(u16*)(base + GSGFX_OFFSET(pos.s[1]));
    a = *(u16*)(base + GSGFX_OFFSET(pos.s[0]));
    GS_WGPIPE_U16(a);
    GS_WGPIPE_U16(b);
}
#endif

#if 0
asm void fn_800D7524(void) {
#include "src/game/gs_render_fn_800D7524.inc"
}
#else
void _gfxEmitPos2f32(void) {
    f32 b = gsgfxState->pos.f[1];
    f32 a = gsgfxState->pos.f[0];
    GS_WGPIPE_F32(a);
    GS_WGPIPE_F32(b);
}
#endif

#if 0
asm void fn_800D7540(void) {
#include "src/game/gs_render_fn_800D7540.inc"
}
#else
void _gfxEmitPos3s8(void) {
    u8 a, b, c;
    u8 *base = (u8*)gsgfxState;
    c = *(u8*)(base + GSGFX_OFFSET(pos.b[2]));
    b = *(u8*)(base + GSGFX_OFFSET(pos.b[1]));
    a = *(u8*)(base + GSGFX_OFFSET(pos.b[0]));
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
    GS_WGPIPE_U8(c);
}
#endif

#if 0
asm void fn_800D7564(void) {
#include "src/game/gs_render_fn_800D7564.inc"
}
#else
void _gfxEmitPos3u8(void) {
    u8 a, b, c;
    u8 *base = (u8*)gsgfxState;
    c = *(u8*)(base + GSGFX_OFFSET(pos.b[2]));
    b = *(u8*)(base + GSGFX_OFFSET(pos.b[1]));
    a = *(u8*)(base + GSGFX_OFFSET(pos.b[0]));
    GS_WGPIPE_U8(a);
    GS_WGPIPE_U8(b);
    GS_WGPIPE_U8(c);
}
#endif

#if 0
asm void fn_800D7588(void) {
#include "src/game/gs_render_fn_800D7588.inc"
}
#else
void _gfxEmitPos3s16(void) {
    u16 a, b, c;
    u8 *base = (u8*)gsgfxState;
    c = *(u16*)(base + GSGFX_OFFSET(pos.s[2]));
    b = *(u16*)(base + GSGFX_OFFSET(pos.s[1]));
    a = *(u16*)(base + GSGFX_OFFSET(pos.s[0]));
    GS_WGPIPE_U16(a);
    GS_WGPIPE_U16(b);
    GS_WGPIPE_U16(c);
}
#endif

#if 0
asm void fn_800D75AC(void) {
#include "src/game/gs_render_fn_800D75AC.inc"
}
#else
void _gfxEmitPos3u16(void) {
    u16 a, b, c;
    u8 *base = (u8*)gsgfxState;
    c = *(u16*)(base + GSGFX_OFFSET(pos.s[2]));
    b = *(u16*)(base + GSGFX_OFFSET(pos.s[1]));
    a = *(u16*)(base + GSGFX_OFFSET(pos.s[0]));
    GS_WGPIPE_U16(a);
    GS_WGPIPE_U16(b);
    GS_WGPIPE_U16(c);
}
#endif

#if 0
asm void fn_800D75F4(void) {
#include "src/game/gs_render_fn_800D75F4.inc"
}
#else
void _gfxEmitPos3f32(void) {
    GSgfxState* state = gsgfxState;
    f32 x;
    f32 y;
    f32 z;

    z = state->pos.f[2];
    y = state->pos.f[1];
    x = state->pos.f[0];

    GS_WGPIPE_F32(x);
    GS_WGPIPE_F32(y);
    GS_WGPIPE_F32(z);
}

void gfxVtxDescFree(GSVtxDesc* desc) {
    if (gsgfxState->vtxDesc == desc) gsgfxState->vtxDesc = NULL;
    desc->attr[0].enabled = 0; desc->attr[1].enabled = 0; desc->attr[2].enabled = 0; desc->attr[3].enabled = 0;
    desc->attr[4].enabled = 0; desc->attr[5].enabled = 0; desc->attr[6].enabled = 0; desc->attr[7].enabled = 0;
    desc->attr[8].enabled = 0; desc->attr[9].enabled = 0; desc->attr[10].enabled = 0; desc->attr[11].enabled = 0;
    desc->attr[12].enabled = 0; desc->attr[13].enabled = 0; desc->active = 0;
}
#endif

#if 0
asm void fn_800D7650(void) {
#include "src/game/gs_render_fn_800D7650.inc"
}
#else
void gfxVtxDescClear(GSVtxDesc* desc) {
    if (gsgfxState->vtxDesc == desc) gsgfxState->vtxDesc = NULL;
    desc->attr[0].enabled = 0; desc->attr[1].enabled = 0; desc->attr[2].enabled = 0; desc->attr[3].enabled = 0;
    desc->attr[4].enabled = 0; desc->attr[5].enabled = 0; desc->attr[6].enabled = 0; desc->attr[7].enabled = 0;
    desc->attr[8].enabled = 0; desc->attr[9].enabled = 0; desc->attr[10].enabled = 0; desc->attr[11].enabled = 0;
    desc->attr[12].enabled = 0; desc->attr[13].enabled = 0;
}
#endif
#endif

extern u8 lbl_80314370[];
extern u8 lbl_803143B4[];
extern u8 lbl_803143D8[];
extern u8 lbl_803143A8[];
#if defined(GS_GFX_LAYER_800D76A8_800D7820)
#if 0
asm void fn_800D76A8(void) {
#include "src/game/gs_render_fn_800D76A8.inc"
}
#else
void gfxSetVtxDescRaw(GSVtxDesc* desc, u16 vertCount) {
    GSgfxState* state;
    GSVtxDesc* oldDesc;

    state = gsgfxState;
    if (state->captureActive == 1) {
        oldDesc = state->vtxDesc;
        state->vtxDesc = desc;
        fn_800D7940(desc, vertCount);
        state->vtxDesc = oldDesc;
        return;
    }

    if (state->recordMode == 1) {
        gfxRecordCommand(0x47, 2, PC_PTR32(desc), (u32)vertCount);
        return;
    }

    gfxLoadVtxDesc(desc);
    gfxSetupChannelsTev(desc);
    fn_800D7940(desc, vertCount);
}
#endif
#endif

#if defined(GS_GFX_LAYER_800D7820_800D7894)
#if 0
asm void fn_800D7820(void) {
#include "src/game/gs_render_fn_800D7820.inc"
}
#else
/* desc: a vertex-descriptor table in .data (callers pass lbl_80314638 and the like, or NULL). */
void gfxSetVtxDesc(void* desc) {
    /* Recording stores desc in one 32-bit word of the request buffer (gfxRecordCommand reads every word
     * argument as u32), so on the PC it goes through PC_PTR32 (snatcherLeo's option (a): game memory below 4 GB);
     * gfxPlayCommands turns it back with PC_PTR (command 0x46). */
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x46, 1, PC_PTR32(desc)); }
    else { gsgfxState->vtxDesc = (GSVtxDesc*)desc; }
}
#endif

#if 0
asm void fn_800D7868(void) {
#include "src/game/gs_render_fn_800D7868.inc"
}
#else
void gfxVtxDescSetAttr(GSVtxDesc* desc, u32 idx, u32 type, u32 count, u32 compType, u8 frac, void* data, u8 stride) {
    /* attribute idx is addressed as (desc + idx * attribute size)->attr[0], the form the target computes */
    GSVtxDesc* p = (GSVtxDesc*)((u8*)desc + idx * sizeof(GSVtxAttr));
    p->attr[0].enabled = 1;
    p->attr[0].type = type;
    p->attr[0].count = count;
    p->attr[0].compType = compType;
    p->attr[0].frac = frac;
    p->attr[0].data = data;
    p->attr[0].stride = stride;
}
#endif
#endif

#if defined(GS_GFX_LAYER_800D7894_800D7940)
extern u32 lbl_8047AAB0;
extern u32 lbl_8047AAAC;
extern u8 lbl_803144D0[];
extern u32 lbl_8047AAB4;
#if 0
asm void fn_800D7894(void) {
#include "src/game/gs_render_fn_800D7894.inc"
}
#else
GSVtxDesc* gfxVtxDescAlloc(void) {
    u32 i;
    u32 cnt = lbl_8047AAB0;
    GSVtxDesc* o = (GSVtxDesc*)PC_PTR(lbl_8047AAAC);
    for (i = 0; i < cnt; i++) {
        if (o->active == 0) {
            o->active = 1;
            o->vtxFmt = *(u32*)(lbl_803144D0 + lbl_8047AAB4 * 4);
            lbl_8047AAB4++;
            if (lbl_8047AAB4 >= 8) {
                lbl_8047AAB4 = 0;
            }
            o->attr[0].enabled = 0;
            o->attr[1].enabled = 0;
            o->attr[2].enabled = 0;
            o->attr[3].enabled = 0;
            o->attr[4].enabled = 0;
            o->attr[5].enabled = 0;
            o->attr[6].enabled = 0;
            o->attr[7].enabled = 0;
            o->attr[8].enabled = 0;
            o->attr[9].enabled = 0;
            o->attr[10].enabled = 0;
            o->attr[11].enabled = 0;
            o->attr[12].enabled = 0;
            o->attr[13].enabled = 0;
            return o;
        }
        o++;
    }
    return 0;
}

#endif

#endif

#if defined(GS_GFX_LAYER_800D7940_800D7A70)
#if 0
asm void fn_800D7940(void) {
#include "src/game/gs_render_fn_800D7940.inc"
}
#else
extern void gfxVtxPos1x16(u16);
extern void gfxVtxPos1x8(u8);
extern void gfxVtxNormal1x16(u16);
extern void gfxVtxNormal1x8(u8);
extern void gfxVtxColor1x16(u32, u16);
extern void gfxVtxColor1x8(u32, u8);
extern void gfxVtxTexCoord1x16(u32, u16);
extern void gfxVtxTexCoord1x8(u32, u8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif

void fn_800D7940(GSVtxDesc* desc, u16 vertCount) {
    u16 vertex;
    s32 i;
    u32 layer;
    GSVtxAttr* entry;

    gfxBegin(vertCount);
    for (vertex = 0; vertex < vertCount; vertex++) {
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
#endif
#endif

#if defined(GS_GFX_LAYER_800D7A70_800D7D90)
#if 0
asm void fn_800D7A70(u32) {
#include "src/game/gs_render_fn_800D7A70.inc"
}
#else
void gfxLoadVtxDesc(GSVtxDesc* desc) {
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
#endif

extern u32 lbl_8047AAB0;
extern u16 lbl_8047AAA8;
extern u32 lbl_8047AAAC;
extern u32 lbl_8047AAB4;
#if 0
asm void fn_800D7B80(void) {
#include "src/game/gs_render_fn_800D7B80.inc"
}
#else
void gfxVtxDescPoolInit(u32 count) {
    u16 handle;
    u32 off;
    u32 i;
    lbl_8047AAB0 = count;
    off = _toolentryAlloc__FUl(count * sizeof(GSVtxDesc));
    handle = off;
    lbl_8047AAA8 = handle;
    if (handle != 0) {
        lbl_8047AAAC = PC_PTR32(GSmemLock(handle));
        for (off = 0, i = 0; i < lbl_8047AAB0; i++, off += sizeof(GSVtxDesc)) {
            *(u8*)PC_PTR(lbl_8047AAAC + off) = 0; /* GSVtxDesc.active */
        }
        lbl_8047AAB4 = 0;
    }
}
#endif

#if 0
asm void fn_800D7BF8(void) {
#include "src/game/gs_render_fn_800D7BF8.inc"
}
#else
u32 gfxGetCameraMatrix(s32 mode) {
    /* the active camera is passed on to each getter (it stays in r3 in the original: callsite.py) */
    void* camera = (void*)GScameraGetActiveCamera();
    if (!camera) return 0;
    switch (mode) {
        case 0: return PC_PTR32(GScameraGetMatrixPtr(camera));
        case 1: return PC_PTR32(GScameraGetInvMatrixPtr(camera));
        case 2: return PC_PTR32(GScameraGetProjMatrixPtr(camera));
        default: return 0;
    }
}
#endif

extern u8 lbl_8047AAC8;
extern u8 lbl_80314610[];
extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_80400948[];
#if 0
asm void fn_800D7C74(void) {
#include "src/game/gs_render_fn_800D7C74.inc"
}
#else
void gfxFlushMatrix(void) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x45, 0);
    } else if (lbl_8047AAC8) {
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

#if 0
asm void fn_800D7D10(void) {
#include "src/game/gs_render_fn_800D7D10.inc"
}
#else
void gfxGetMatrix(u8 idx, void* src) {
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x44, 2, (u32)idx, src); }
    else if (idx > 9) { GSlogWrite(lbl_80270440); }
    else { fn_800E0628(src, &lbl_80400948[idx * 0x30]); }
}
#endif
#endif

#if defined(GS_GFX_LAYER_800D7D90_800D7E5C)
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D7D90(void) {
#include "src/game/gs_render_fn_800D7D90.inc"
}
#else
void gfxSetMatrix(u8 idx, void* src) {
    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x43, 0x11, (u32)idx, src);
    } else if (idx > 9) {
        GSlogWrite(lbl_80270440);
    } else {
        GXLoadPosMtxImm(src, *(u32*)(lbl_80314610 + idx * 4));
        fn_800E0628(lbl_80400948 + idx * 0x30, src);
        if (idx == 9) {
            lbl_8047AAC8 = 1;
        }
    }
}
#endif
#endif

#if defined(GS_GFX_LAYER_800D7E5C_800D85D4)
extern GSMtxStackEntry* lbl_8047AAC0;
extern GSMtxStackEntry* lbl_8047AAC4;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D7E5C(void) {
#include "src/game/gs_render_fn_800D7E5C.inc"
}
#else
void gfxPopMatrix(void) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x42, 0);
    } else if (lbl_8047AAC0 >= lbl_8047AAC4) {
        GSlogWrite(lbl_80270460);
    } else {
        lbl_8047AAC0 = lbl_8047AAC0 + 1;
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern GSMtxStackEntry* lbl_8047AABC;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D7F14(void) {
#include "src/game/gs_render_fn_800D7F14.inc"
}
#else
void gfxPushMultMatrix(void* src) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x41, 0x10, src);
    } else if (lbl_8047AAC0 <= lbl_8047AABC) {
        GSlogWrite(lbl_80270480);
    } else {
        lbl_8047AAC0 = lbl_8047AAC0 - 1;
        GSmtxConcat((void*)lbl_8047AAC0, (void*)(lbl_8047AAC0 + 1), src);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D7FE4(void) {
#include "src/game/gs_render_fn_800D7FE4.inc"
}
#else
void gfxMultMatrix(void* param) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x40, 0x10, param);
    } else {
        GSmtxConcat((void*)lbl_8047AAC0, (void*)lbl_8047AAC0, param);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern GSMtxStackEntry* lbl_8047AABC;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D8088(void) {
#include "src/game/gs_render_fn_800D8088.inc"
}
#else
void gfxPushLoadMatrix(void* src) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x3f, 0x10, src);
    } else if (lbl_8047AAC0 <= lbl_8047AABC) {
        GSlogWrite(lbl_80270480);
    } else {
        lbl_8047AAC0 = lbl_8047AAC0 - 1;
        fn_800E0628((void*)lbl_8047AAC0, src);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D8154(void) {
#include "src/game/gs_render_fn_800D8154.inc"
}
#else
void gfxScaleMatrix(f32 a, f32 b, f32 c) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x3e, 0xd, a, b, c);
    } else {
        fn_800E02C4((void*)lbl_8047AAC0, a, b, c);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D81EC(void) {
#include "src/game/gs_render_fn_800D81EC.inc"
}
#else
void gfxTranslateMatrix(f32 a, f32 b, f32 c) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x3d, 0xd, a, b, c);
    } else {
        fn_800E03E8((void*)lbl_8047AAC0, a, b, c);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D8284(void) {
#include "src/game/gs_render_fn_800D8284.inc"
}
#else
void gfxRotateXYZ(f32 a, f32 b, f32 c) {
    f32 y;
    f32 z;
    u32 matrixIndex;
    y = b;
    z = c;
    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x3c, 0xd, a, y, z);
    } else {
        GSmtxRotateX((void*)lbl_8047AAC0, a);
        GSmtxRotateY((void*)lbl_8047AAC0, y);
        GSmtxRotateZ((void*)lbl_8047AAC0, z);
        matrixIndex = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, matrixIndex);
        GXLoadNrmMtxImm(lbl_8047AAC0, matrixIndex);
        GXSetCurrentMtx(matrixIndex);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D834C(void) {
#include "src/game/gs_render_fn_800D834C.inc"
}
#else
void gfxLoadIdentity(void) {
    GSgfxState* state = gsgfxState;
    u32 mtx;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x3b, 0);
    } else {
        fn_800E064C((void*)lbl_8047AAC0);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern u16 lbl_8047AAB8;
extern GSMtxStackEntry* lbl_8047AABC;
extern GSMtxStackEntry* lbl_8047AAC4;
extern GSMtxStackEntry* lbl_8047AAC0;
extern u8 lbl_8047AAC8;
#if 0
asm void fn_800D83E4(void) {
#include "src/game/gs_render_fn_800D83E4.inc"
}
#else
void gfxMatrixStackInit(u32 count) {
    u16 handle;
    u32 mtx;
    GSMtxStackEntry* ptr;
    u32 val;
    handle = _toolentryAlloc__FUl(count * sizeof(GSMtxStackEntry));
    val = (u16)handle;
    lbl_8047AAB8 = handle;
    if (val != 0) {
        ptr = GSmemLock((u16)val);
        lbl_8047AABC = ptr;
        ptr = ptr + (count - 1);
        lbl_8047AAC4 = ptr;
        lbl_8047AAC0 = ptr;
        fn_800E064C(ptr);
        mtx = *(u32*)(lbl_80314610 + 0x24);
        GXLoadPosMtxImm(lbl_8047AAC0, mtx);
        GXLoadNrmMtxImm(lbl_8047AAC0, mtx);
        GXSetCurrentMtx(mtx);
        fn_800E0628((void*)(lbl_80400948 + 0x1b0), (void*)lbl_8047AAC0);
        lbl_8047AAC8 = 0;
    }
}
#endif

extern u8 lbl_80314404[];
extern u8 lbl_80314454[];
extern u8 lbl_803144A8[];
extern u8 lbl_80314424[];
#if 0
asm void fn_800D848C(void) {
#include "src/game/gs_render_fn_800D848C.inc"
}
#else
void gfxLoadTexMatrix(u32 idx, s32 mode, u32 arg, void* mtx) {
    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(0x27, 0x13, idx, mode, arg, mtx);
    } else {
        if (mode == 0) {
            GXSetTexCoordGen2(((u32*)lbl_80314404)[idx], 1, ((u32*)lbl_80314454)[arg], 0x3c, 0, 0x7d);
        } else {
            if (mode == 1) {
                GXLoadTexMtxImm(mtx, ((u32*)lbl_803144A8)[idx], 0);
            } else if (mode == 2) {
                GXLoadTexMtxImm(mtx, ((u32*)lbl_803144A8)[idx], 1);
            }
            GXSetTexCoordGen2(((u32*)lbl_80314404)[idx], ((u32*)lbl_80314424)[mode], ((u32*)lbl_80314454)[arg], ((u32*)lbl_803144A8)[idx], 0, 0x7d);
        }
    }
}
#endif
#endif

#if defined(GS_GFX_LAYER_800D85D4_800D87AC)
extern u8 lbl_80314530[];
extern u32 lbl_8047CA40;
extern u32 lbl_8047CA48;
extern u8 lbl_80314510[];
extern u8 lbl_803144F0[];
#if 0
asm void fn_800D85D4(void) {
#include "src/game/gs_render_fn_800D85D4.inc"
}
#else
void gfxLoadTexture(s32 slot, void* model) {
    GSgfxState* state;
    u8* obj;
    s32 mode;

    if (model == 0) {
        return;
    }

    state = gsgfxState;
    obj = (u8*)model;
    if (state->recordMode == 1) {
        gfxRecordCommand(0x26, 2, slot, PC_PTR32(model));
        return;
    }
    if (state->boundTexture[slot] == model) {
        return;
    }

    if (obj[0x7] != 0) {
        GXInitTexObjWrapMode(obj + 0x54, ((u32*)lbl_80314530)[*(u32*)(obj + 0x10)],
                    ((u32*)lbl_80314530)[*(u32*)(obj + 0x14)]);
        switch (*(s32*)(obj + 0x20)) {
        case 0:
            mode = (*(s32*)(obj + 0x18) == 2);
            break;
        case 1:
            mode = (*(s32*)(obj + 0x18) == 2) + 2;
            break;
        case 2:
            mode = (*(s32*)(obj + 0x18) == 2) + 4;
            break;
        }
        GXInitTexObjLOD(obj + 0x54, mode, (*(u32*)(obj + 0x1c) == 2),
                    *(f32*)&lbl_8047CA40, (f32)(obj[0x5] - 1),
                    *(f32*)&lbl_8047CA48, 0, 0, 0);
        obj[0x7] = 0;
    }

    state->boundTexture[slot] = model;
    if (*(u32*)(obj + 0x48) != 0) {
        GXLoadTlut(obj + 0x74, ((u32*)lbl_80314510)[slot]);
    }
    GXLoadTexObj(obj + 0x54, ((u32*)lbl_803144F0)[slot]);
    *(u32*)(lbl_804001F0 + 0x24) += 1;
}
#endif
#endif

#if defined(GS_GFX_LAYER_800D87AC_800D892C)
#if 0
asm void fn_800D87AC(void) {
#include "src/game/gs_render_fn_800D87AC.inc"
}
#else
void _gfxSetFlagsImm(u32 mask) {
    s32 i;
    u8* textureSlot;

    gsgfxState->dirty |= mask;
    if (mask & 2) {
        for (i = 0; i < 8; i++) {
            gsgfxState->boundTexture[i] = NULL;
            GXSetTexCoordGen2(((u32*)lbl_80314404)[i], 1, i + 4, 0x3c, 0, 0x7d);
        }
    }
    if (mask & 4) {
        for (i = 0; i < 0x10; i++) {
            textureSlot = &gsgfxState->tev.indEnable[i];
            if (*textureSlot) {
                *textureSlot = 0;
                GXSetTevDirect(i);
            }
        }
        gsgfxState->tev.numIndStages = 0;
        GXSetNumIndStages(0);
    }
}
#endif

#if 0
asm void fn_800D888C(void) {
#include "src/game/gs_render_fn_800D888C.inc"
}
#else
void gfxClearFlags(u32 mask) {
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x29, 1, mask); }
    else { gsgfxState->flags &= ~mask; }
}
#endif

#if 0
asm void fn_800D88DC(void) {
#include "src/game/gs_render_fn_800D88DC.inc"
}
#else
void gfxSetFlags(u32 mask) {
    if (gsgfxState->recordMode == 1) { gfxRecordCommand(0x28, 1, mask); }
    else { gsgfxState->flags |= mask; }
}
#endif
#endif

/* _gfxFlushTevState: linked owner is the standalone gs_gfx_layer_candidate_800D923C.c. */

/* fn_800D963C: linked owner is the standalone gs_gfx_layer_candidate_800D963C.c. */
