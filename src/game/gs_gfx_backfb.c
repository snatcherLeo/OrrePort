/**
 * @file gs_gfx_backfb.c
 * @brief GS render engine segment -- split from gs_render.c.
 *
 * XD source unit: game/pxdvs/GSAPI/GSgfxM/backfb.cpp
 * Address range: 0x800DC298 - 0x800DC560 (3 functions)
 *
 * 3 anchors monotonic (0x802A6E90 < 0x802A6F88 < 0x802A72D8) covering the whole XD TU; GSgfxEndBackFBCapture size 0xF8 EXACT; internal order End->Begin->Init identical in both games; XD TU end 0x802A72F8 = Init end, ours likewise ends the TU at 0x800DC560.
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
extern void  VIGetNextField(void);                        /* GXSetViewport */
extern void  GXSetViewportJitter(void);                        /* GXSetProjection */
extern void  GXSetViewport(void);                        /* GXLoadPosMtxImm */
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
extern void GXSetPointSize(u32, u32);
extern f32 lbl_8047CA30;
extern f32 lbl_8047CA34;
extern f32 lbl_8047CA38;
extern void GXSetLineWidth(u32, u32);
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
extern void GXLoadPosMtxImm(u32, u32);
extern void GXLoadNrmMtxImm(u32, u32);
extern void GXSetCurrentMtx(u32);
extern u8 lbl_8047AAC8;

extern f32 (*lbl_8047AAC0)[3][4]; /* GSMtxStackEntry* */
extern u8 lbl_80400948[];
extern f32 (*lbl_8047AAC4)[3][4]; /* GSMtxStackEntry* */
extern f32 (*lbl_8047AABC)[3][4]; /* GSMtxStackEntry* */
extern u16 lbl_8047AAB8;
extern void GXSetTexCoordGen2(u32, u32, u32, u32, u32, u32);
extern void GXLoadTexMtxImm(void*, u32, u32);




extern void GXInitTexObjWrapMode();
extern void GXInitTexObjLOD();
extern void GXLoadTlut();
extern void GXLoadTexObj();

extern u32 lbl_8047CA40;



extern void GXSetTevDirect(u32);
extern void GXSetNumIndStages(u32);
extern void GXSetNumChans();
extern void GXSetChanCtrl();
extern void GXSetNumTexGens();
extern void GXSetNumTevStages();
extern void GXSetIndTexOrder();
extern void GXSetIndTexCoordScale();
extern void GXSetIndTexMtx();
extern void GXSetTevOrder();
extern void GXSetTevColorOp();
extern void GXSetTevAlphaOp();
extern void GXSetTevColorIn();
extern void GXSetTevAlphaIn();
extern void GXSetTevKColorSel();
extern void GXSetTevKAlphaSel();
extern void GXSetTevIndirect();
extern void fn_800BBC7C();
extern void fn_800BBCE0();
extern void fn_800BBE8C();
extern void fn_800BBF98();
extern void fn_800BBFDC();
extern void GXSetTevKColor();
extern void GXSetTevSwapModeTable();
extern void GXSetProjection(void*, u32);
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

extern void GXSetDither(u32);
extern void GXSetAlphaCompare(u32, u8, u32, u32, u8);


extern void GXSetZMode(u32, u32, u32);
extern void GXSetZCompLoc(u32);

extern void GXSetDstAlpha(u32);
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
extern void GXCallDisplayList(u32, u32);
extern void fn_800E2AF8(u16);
extern u16 GSmemAlloc(u32, u32);
extern u32 lbl_8047AAD8;

extern void jumptable_80315364();
extern u32 GStextureUnlockImage(void*);
/* lbl_80400EE0 (capture slots) and lbl_8047AAE0: declared in game/gs_gfx_backfb.h */

extern u32 GStextureGetFormat(void*);
extern void GStextureSetWrap(void*, u32, u32);
extern void GStextureSetFilter(void*, u32, u32, u32);
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
extern void GXDrawDone(u32);
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
extern f64 cos(f32);
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

#include "game/gs_gfx_backfb.h" /* GSbackFBCapture, the slot table lbl_80400EE0, lbl_8047AAE0 */
#ifdef TARGET_PC
GSbackFBCapture lbl_80400EE0[4]; /* PC: defined here with its C type (4 x 40 bytes; the GameCube slot is 0x50) */
#endif










static inline GSbackFBCapture* GSbackFBFindCapture(void* texture)
{
    GSbackFBCapture* capture;
    u32 i;

    capture = (GSbackFBCapture*)lbl_80400EE0;
    for (i = 0; i < 4; i++, capture++) {
        if (capture->active == 1 && capture->texture == texture) {
            return capture;
        }
    }
    return NULL;
}

/*
 * Free-slot search.  Retail inlines it into GSgfxBeginBackFBCapture as the
 * same unrolled scan as GSbackFBFindCapture, with the helper's NULL return
 * routed through r4 and copied into the caller's home register (mr r31,r4).
 */
static inline GSbackFBCapture* GSbackFBFindFree(void)
{
    GSbackFBCapture* capture;
    u32 i;

    capture = (GSbackFBCapture*)lbl_80400EE0;
    for (i = 0; i < 4; i++, capture++) {
        if (capture->active == 0) {
            return capture;
        }
    }
    return NULL;
}

u32 GSgfxEndBackFBCapture(void* texture) {
    GSbackFBCapture* capture;
    u32 i;

    capture = GSbackFBFindCapture(texture);
    if (capture == NULL) {
        return 0;
    }

    GStextureUnlockImage(capture->texture);
    capture->active = 0;
    lbl_8047AAE0 = 0;
    for (i = 0; i < 4; i++) {
        if (((GSbackFBCapture*)lbl_80400EE0)[i].active == 1) {
            lbl_8047AAE0 = 1;
            break;
        }
    }
    return 1;
}

extern u8 lbl_8047AAE0;
u32 GSgfxBeginBackFBCapture(void* texture, void* callback, void* userData) {
    GSbackFBCapture* capture;

    switch (GStextureGetFormat(texture)) {
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x90:
        break;
    default:
        return 0;
    }

    if (GSbackFBFindCapture(texture) != NULL) {
        return 0;
    }

    capture = GSbackFBFindFree();
    if (capture == NULL) {
        return 0;
    }

    capture->active = 1;
    capture->texture = texture;
    capture->callback = (GSbackFBCallback)callback;
    capture->userData = userData;
    capture->frame = 0;
    GStextureSetWrap(capture->texture, 0, 0);
    GStextureSetFilter(capture->texture, 2, 2, 0);
    GStextureLockImage(capture->texture, 0);
    lbl_8047AAE0 = 1;
    return 1;
}


/* GSgfxBackFBInit(void): frees every capture slot. */
void GSgfxBackFBInit__Fv(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        ((GSbackFBCapture*)lbl_80400EE0)[i].active = 0;
    }
    lbl_8047AAE0 = 0;
}
