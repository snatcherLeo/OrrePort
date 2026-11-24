#include "game/gs_gfx_layer.h"
/**
 * @file gs_gfx_core.c
 * @brief GS render engine segment -- split from gs_render.c.
 *
 * XD source unit: game/pxdvs/GSAPI/GSgfxM/GSgfx.cpp
 * Address range: 0x800D3E4C - 0x800D3FA4 (4 functions)
 *
 * XD GSgfx.cpp = {RelocToScratch 0x48, Shutdown 0x24, Init 0xE0, _gfxScratchNotify 0x78}. Ours: Notify(0x78 exact) + GSgfxSetViewportJitter(0x8C~Init) + fn_800D3F50(0xC~Shutdown) + fn_800D3F5C(0x48 EXACT match RelocToScratch). Colosseum placed Notify first; same 4-function TU. Single anchor -> trivially monotonic.
 *
 * Split from src/game/gs_render.c (physical XD source-unit split).
 * The dead #ifdef PCPORT reference block (never defined in configure.py,
 * same situation as gs_gfx.c) was stripped during the split.
 */

#include "dolphin/types.h"

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
extern void gfxSetupChannelsTev(struct GSVtxDesc*);

/* GSmem */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   _toolentryAlloc__FUl(u32 size);                    /* GSmemAllocRaw */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);                  /* GSmemGetPtr */
#endif

/* SDK GX functions */
extern u32   VIGetNextField(void);                        /* VIGetNextField */
extern void  GXSetViewportJitter(f32, f32, f32, f32, f32, f32, u32); /* GXSetViewportJitter */
extern void  GXSetViewport(f32, f32, f32, f32, f32, f32); /* GXSetViewport */
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
#ifndef TARGET_PC
extern GSgfxState lbl_80400248;  /* GSgfx state backup while scratch slot 3 is lent out (linker data on the GameCube) */
#else
GSgfxState lbl_80400248;         /* PC: defined here with its C type (1568 bytes; the GameCube slot is 0x5A0) */
#endif


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


/* ==================================================================
 * _gfxScratchNotify__F15GSscratchNotifyPvUc -- GSgfx scratch-slot notifier
 *
 * GSgfxInit puts the state in scratch slot 3 and registers this callback.
 * When the slot is taken (mode 0) the state moves to the static backup
 * lbl_80400248; when the slot is handed back (mode 1) it moves into the
 * slot again. gsgfxState follows the state.
 *
 * From disassembly (0x800D3E4C, 0x78 bytes):
 *   cmpwi r3, 0x1
 *   beq .restore
 *   ; mode 0: memcpy(lbl_80400248, r4, 0x5A0)
 *   ; mode 1: memcpy(r4=r31, lbl_80400248, 0x5A0)
 * ================================================================== */
void _gfxScratchNotify__F15GSscratchNotifyPvUc(s32 mode, void* buffer, u8 unused)
{
    switch (mode) {
    case 0:
        memcpy(&lbl_80400248, buffer, sizeof(GSgfxState));
        gsgfxState = &lbl_80400248;
        break;
    case 1:
        memcpy(buffer, &lbl_80400248, sizeof(GSgfxState));
        gsgfxState = (GSgfxState*)buffer;
        break;
    }
}


/* ==================================================================
 * GSgfxSetViewportJitter -- GSgfx pre-retrace callback
 *
 * Called before VBlank retrace. Takes 6 float parameters for viewport
 * setup, dispatches through function pointer if set.
 *
 * From disassembly (0x800D3EC4, 0x8C bytes):
 *   fmr f26, f1 through fmr f31, f6  ; save 6 float params
 *   lwz r12, lbl_8047AA88            ; callback pointer
 *   cmplwi r12, 0x0
 *   beq .default
 *   bctrl                            ; call custom handler
 *   .default:
 *   bl VIGetNextField                   ; VIGetNextField (the field for the jitter)
 *   bl GXSetViewportJitter                   ; GXSetViewportJitter (else GXSetViewport, as HSD's own fn_80196C54)
 * ================================================================== */
typedef void (*GSgfxRetraceCallback)(s32, f32, f32, f32, f32, f32, f32);
extern GSgfxRetraceCallback lbl_8047AA88;

void GSgfxSetViewportJitter(s32 flag, f32 p1, f32 p2, f32 p3, f32 p4, f32 p5, f32 p6)
{
    if (lbl_8047AA88 != 0) {
        lbl_8047AA88(flag, p1, p2, p3, p4, p5, p6);
    } else if (flag != 0) {
        GXSetViewportJitter(p1, p2, p3, p4, p5, p6,
                    VIGetNextField());
    } else {
        GXSetViewport(p1, p2, p3, p4, p5, p6);
    }
}


/* ==================================================================
 * fn_800D3F50 -- GSgfx draw-done callback
 *
 * Set a flag indicating the GX pipeline has finished drawing.
 * 12 bytes.
 *
 * From disassembly (0x800D3F50, 0xC bytes):
 *   li r0, 0x1
 *   stb r0, lbl_8047AA91@sda21(r0)
 *   blr
 * ================================================================== */
extern u8 lbl_8047AA91;

void fn_800D3F50(void)
{
    lbl_8047AA91 = 1;
}


/* ==================================================================
 * fn_800D3F5C -- GSgfx frame end callback
 *
 * Increment frame counter and dispatch queued operations.
 * 72 bytes.
 * ================================================================== */
void fn_800D3F5C(void)
{
    GSgfxState* state = gsgfxState;
    GSgfxFrameCallback callback;

    state->retraceCount++;
    state = gsgfxState;
    callback = state->retraceCallback;
    if (callback != 0) {
        callback(state->retraceCount);
    }
}
