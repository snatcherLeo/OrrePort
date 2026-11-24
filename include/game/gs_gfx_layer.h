/**
 * @file gs_gfx_layer.h
 * @brief GSgfx layer state: the channel / texgen / TEV block that the
 * GSgfx layer setters fill and _gfxFlushTevState flushes to GX.
 *
 * Also defines GSgfxState, the whole state behind gsgfxState (one
 * struct for every user; gs_gfx.h includes this header). Every offset is
 * taken from the target's loads and stores.
 */
#ifndef GS_GFX_LAYER_H
#define GS_GFX_LAYER_H

#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#ifdef TARGET_PC
#include "dolphin/os.h" /* OS_BUS_CLOCK, used by GS_OS_BUS_CLOCK */
#endif
#include "pc/disc_types.h" /* DISC_STRUCT / DISC_PTR: the display-list parser reads disc vertex lists */
/* A 16-bit value in a display list from the disc (big-endian on the PC, a plain load on the GameCube). */
#ifdef TARGET_PC
#define GS_DL_U16(p) __builtin_bswap16(*(u16*)(p))
#else
#define GS_DL_U16(p) (*(u16*)(p))
#endif
/* A 16- / 32-bit value written into a display list GSgfx builds itself (the capture): display lists are big-endian
 * GX command bytes, so the PC swaps each numeric value (the GameCube stores it as is). */
#ifdef TARGET_PC
#define GS_DL_BE16(v) __builtin_bswap16((u16)(v))
#define GS_DL_BE32(v) __builtin_bswap32((u32)(v))
#else
#define GS_DL_BE16(v) (v)
#define GS_DL_BE32(v) (v)
#endif
/* A float in disc vertex data or a display list, as GSgfxParseDisplayList hands it to a vertex callback (the callback
 * gets a pointer into the disc bytes). */
#ifdef TARGET_PC
static inline f32 GS_DL_F32(const void* p) {
    union { u32 u; f32 f; } v;
    v.u = __builtin_bswap32(*(const u32*)p);
    return v.f;
}
#else
#define GS_DL_F32(p) (*(f32*)(p))
#endif

typedef struct GSChanCtrl {
    u8 enable;
    u8 ambSrc;
    u8 matSrc;
    u8 lightMask;
    u8 diffFn;
    u8 attnFn;
} GSChanCtrl;

typedef struct GSTevOrder {
    u8 coord;
    u8 map;
    u8 color;
} GSTevOrder;

typedef struct GSTevOp {
    u8 op;
    u8 bias;
    u8 scale;
    u8 clamp;
    u8 out;
} GSTevOp;

typedef struct GSTevIn {
    u8 a;
    u8 b;
    u8 c;
    u8 d;
} GSTevIn;

typedef struct GSTevInd {
    u8 type;
    u8 indStage;
    u8 mtxSel;
    u8 format;
    u8 biasSel;
    u8 alphaSel;
    u8 wrapS;
    u8 wrapT;
    u8 addPrev;
    u8 utcLod;
    u8 warpSigned;
    u8 warpReplace;
    u16 tileSizeS;
    u16 tileSizeT;
    u16 tileSpacingS;
    u16 tileSpacingT;
} GSTevInd;

typedef struct GSIndOrder {
    u8 coord;
    u8 map;
    u8 scaleS;
    u8 scaleT;
} GSIndOrder;

typedef struct GSIndMtx {
    f32 mtx[2][3];
    u8 scale;
} GSIndMtx;

/*
 * The GX channel / texgen / TEV block. It lives at +0x60 in the GSgfx
 * state, and a second copy (lbl_80400B28) holds a saved block that
 * fn_800D892C loads back field by field; offsets below are relative to
 * the block, with the GSgfx-state offset in brackets.
 */
typedef struct GSgfxTevState {
    u8 numChans;                 /* 0x000 [0x060] */
    GSChanCtrl chanCtrl[4];      /* 0x001 [0x061]: colour0, alpha0, colour1, alpha1 */
    u8 numTexGens;               /* 0x019 [0x079] */
    u8 numTevStages;             /* 0x01A [0x07A] */
    GSTevOrder tevOrder[16];     /* 0x01B [0x07B] */
    GSTevOp colorOp[16];         /* 0x04B [0x0AB] */
    GSTevOp alphaOp[16];         /* 0x09B [0x0FB] */
    GSTevIn colorIn[16];         /* 0x0EB [0x14B] */
    GSTevIn alphaIn[16];         /* 0x12B [0x18B] */
    u32 kcolorSel[16];           /* 0x16C [0x1CC] */
    u32 kalphaSel[16];           /* 0x1AC [0x20C] */
    GXColor kcolor[4];           /* 0x1EC [0x24C] */
    u8 indEnable[16];            /* 0x1FC [0x25C] */
    GSTevInd ind[16];            /* 0x20C [0x26C] */
    u8 numIndStages;             /* 0x34C [0x3AC] */
    GSIndOrder indOrder[4];      /* 0x34D [0x3AD] */
    GSIndMtx indMtx[3];          /* 0x360 [0x3C0] */
} GSgfxTevState;

/* Per-stage sources: indices into the colour, texcoord and texmap tables,
 * and the TEV preset mode for fn_800D963C. */
typedef struct GSTevStageSrc {
    u8 color;
    u8 coord;
    u8 map;
    u8 mode;
} GSTevStageSrc;

/* One vertex attribute of a GSgfx vertex descriptor (see gfxVtxDescSetAttr). */
typedef struct GSVtxAttr {
    u8 enabled;                  /* 0x00 */
    u32 type;                    /* 0x04: GXSetVtxDesc type, index into lbl_803143A8 (none / direct / index8 / index16) */
    u32 count;                   /* 0x08: GXSetVtxAttrFmt count, index into lbl_803143B4 */
    u32 compType;                /* 0x0C: GXSetVtxAttrFmt component type, index into lbl_803143D8 */
    u8 frac;                     /* 0x10: GXSetVtxAttrFmt fraction bits */
    void* data;                  /* 0x14: GXSetArray base (NULL: no array) */
    u8 stride;                   /* 0x18: GXSetArray stride */
} GSVtxAttr;                     /* 0x1C on the GameCube */

/* GSgfx vertex descriptor: attribute 0 matrix index, 1 position,
 * 2 normal, 4-5 colours, 6-13 texture coordinates. */
typedef struct GSVtxDesc {
    u8 active;                   /* 0x000: taken from the pool (gfxVtxDescAlloc) */
    u32 vtxFmt;                  /* 0x004: GX vertex format slot, round robin over lbl_803144D0 (8 values) */
    GSVtxAttr attr[14];          /* 0x008 */
} GSVtxDesc;                     /* 0x190 on the GameCube */

/* One display-list capture slot (GSgfxDLBegin / GSgfxDLEnd). */
typedef struct GSgfxDLCapture {
    u8 active;
    u8 overflow;
    u16 handle;
    void* data;
    u32 size;
    GSVtxDesc* desc;
    u32 totalVerts;
    u32 totalPrims;
} GSgfxDLCapture;

typedef void (*GSVtxEmitFn)(u32 index);

/* Last value written for a three-component attribute, kept in every
 * component format the emitters use. */
typedef struct GSVtxValue3 {
    u8 b[3];
    u16 s[3];
    f32 f[3];
} GSVtxValue3;

/* Last colour written, as RGBA8 bytes, packed 16-bit and packed 32-bit. */
typedef struct GSVtxColor {
    u8 b[4];
    u16 s;
    u32 w;
} GSVtxColor;

/* The same for a two-component texture coordinate. */
typedef struct GSVtxValue2 {
    u8 b[2];
    u16 s[2];
    f32 f[2];
} GSVtxValue2;

/*
 * The GSgfx state: one block, allocated by GSgfxInit (scratch slot 3, or
 * the tool heap when that is taken) and pointed to by gsgfxState. Every GS graphics setter reads it; while
 * recordMode is 1 the setters append commands to the request buffer instead
 * of touching GX, and the frame loop (GSgfxDrawScene) plays them back with
 * recordMode 2. Offsets are the GameCube layout (0x5A0 bytes, checked
 * below); on the PC the pointer fields grow, so code must use the field
 * names, never raw offsets. Names marked "guess" are inferred from use.
 */
typedef void (*GSgfxFrameCallback)(u32);

/*
 * One write to the GX write-gather pipe (the GameCube's FIFO, address 0xCC008000), as the vertex emitters do it.
 * On the PC there is no such address: Aurora emulates the FIFO as a byte stream, and its GXParam1* functions append
 * exactly one value to it (its own GXPosition3f32 etc. are sequences of the same writes), so each write maps 1:1.
 */
/* Aurora requires GXEnd after every GXBegin (on the GameCube GXEnd does nothing and gfxEnd never calls it): on the
 * PC, end the primitive when gfxBegin started one (the same layer / pass test gfxBegin uses). */
#ifdef TARGET_PC
#define GS_PC_GXEND(s) do { if ((s)->layerDraw == (s)->layerCur && ((s)->drawMask & (s)->drawEnable)) GXEnd(); } while (0)
#else
#define GS_PC_GXEND(s) ((void)0)
#endif

/* The bus clock, read by the GameCube code from the OS globals in low memory (0x800000F8); Aurora's constant on the PC. */
#ifdef TARGET_PC
#define GS_OS_BUS_CLOCK OS_BUS_CLOCK
#else
#define GS_OS_BUS_CLOCK (*(u32*)0x800000F8)
#endif

#ifdef TARGET_PC
#define GS_WGPIPE_U8(v)  GXParam1u8((u8)(v))
#define GS_WGPIPE_U16(v) GXParam1u16((u16)(v))
#define GS_WGPIPE_U32(v) GXParam1u32((u32)(v))
#define GS_WGPIPE_F32(v) GXParam1f32((f32)(v))
#else
#define GS_WGPIPE_U8(v)  (*(volatile u8*)0xCC008000 = (v))
#define GS_WGPIPE_U16(v) (*(volatile u16*)0xCC008000 = (v))
#define GS_WGPIPE_U32(v) (*(volatile u32*)0xCC008000 = (v))
#define GS_WGPIPE_F32(v) (*(volatile f32*)0xCC008000 = (v))
#endif

typedef struct GSgfxState {
    s32 recordMode;              /* 0x000: 1 record commands, 2 execute (frame playback / init) */
    u32 drawMask;                /* 0x004: pass being played back: 0x10 opaque, 0x1000, 0x2000 translucent, -1 */
    u32 drawEnable;              /* 0x008: pass the current blend mode draws in (0x10 / 0x2000) */
    void* frameTarget;           /* 0x00C: texture the frame renders to (NULL = screen); GSGFX_NO_FRAME between frames */
    u32 flags;                   /* 0x010: gfxSetFlags / gfxClearFlags */
    s32 primType;                /* 0x014 */
    u8 lineHalf;                 /* 0x018 */
    u8 eraseEnabled;             /* 0x019: clear the frame to eraseColor (HSD_EraseRect) */
    u8 layerCur;                 /* 0x01A */
    u8 layerDraw;                /* 0x01B: render pass index (0 / 1) */
    GXColor eraseColor;          /* 0x01C: set by fn_800D36B4; all zero turns eraseEnabled off */
    GSVtxDesc* defaultVtxDesc;   /* 0x020: allocated by GSgfxInit (gfxVtxDescAlloc) */
    GSVtxDesc* vtxDesc;          /* 0x024: current vertex descriptor (gfxSetVtxDesc) */
    void* boundTexture[8];       /* 0x028: texture loaded into each GX texture slot (gfxLoadTexture skips a rebind) */
    GSgfxFrameCallback retraceCallback; /* 0x048: called by the retrace handler fn_800D3F5C with retraceCount */
    u32 retraceCount;            /* 0x04C: VI retraces, counted by fn_800D3F5C */
    u32 lastRetrace;             /* 0x050: retraceCount at the previous frame */
    u32 frameRetraces;           /* 0x054: retraces the last frame took */
    u32 minFrameRetraces;        /* 0x058: frame pacing: wait until this many retraces passed (1 = 60 fps) */
    u8 waitRetrace;              /* 0x05C: 0 skips the frame-pacing wait */
    u8 fogActive;                /* 0x05D: fn_800D9E4C: HSD fog on / off */
    u8 pad_05E[2];
    GSgfxTevState tev;           /* 0x060 */
    u32 dirty;                   /* 0x414: 1 channels, 2 texgens, 4 TEV */
    u8 dstAlpha;                 /* 0x418: GXSetDstAlpha */
    u8 colorUpdate;              /* 0x419: GXSetColorUpdate */
    u8 alphaUpdate;              /* 0x41A: GXSetAlphaUpdate */
    u8 zUpdate;                  /* 0x41B: GXSetZMode update_enable */
    u8 zCompare;                 /* 0x41C: GXSetZMode compare_enable */
    u8 pad_41D[3];
    u32 zFunc;                   /* 0x420: GXSetZMode func */
    u8 zCompLoc;                 /* 0x424: GXSetZCompLoc */
    u8 alphaCompare;             /* 0x425: gfxSetAlphaCompare: alpha test on / off */
    u8 dither;                   /* 0x426: GXSetDither */
    u8 pad_427;
    s32 cullMode;                /* 0x428: GXSetCullMode */
    u8 clipMode;                 /* 0x42C: GXSetClipMode (1 = clipping off) */
    u8 coPlanar;                 /* 0x42D: guess: GXSetCoPlanar (GXSetCoPlanar follows GXSetCullMode in the SDK) */
    GSTevStageSrc stageSrc[16];  /* 0x42E */
    u16 scissor[4];              /* 0x46E: x1, y1, x2, y2 (gfxSetScissor) */
    u16 viewport[4];             /* 0x476: x1, y1, x2, y2, inclusive (gfxSetViewport sets, fn_800D9AF0 reads) */
    u8 captureActive;            /* 0x47E: a display-list capture is open */
    u8 pad_47F;
    GSgfxDLCapture* captureDL;   /* 0x480 */
    u8* captureCursor;           /* 0x484 */
    s32 capturePrimType;         /* 0x488 */
    u16 requestBufferHandle;     /* 0x48C */
    u16 pad_48E;
    u32* requestBuffer;          /* 0x490: recorded commands (gfxAllocRequestBuffer) */
    u32* requestBufferPos;       /* 0x494: write position */
    u32 requestBufferSize;       /* 0x498 */
    u8 overlayColorUpdate;       /* 0x49C: guess: colour-update flag for the end-of-frame overlay quad (set by fn_800D4610) */
    u8 frameBegun;               /* 0x49D: the frame target has been prepared this frame */
    u8 pad_49E;
    u8 vtxPending;               /* 0x49F */
    GSVtxEmitFn emitMtxIdx;      /* 0x4A0 */
    u8 mtxIdx;                   /* 0x4A4: matrix index the emitter writes (times 3) */
    u8 pad_4A5[3];
    GSVtxEmitFn emitPos;         /* 0x4A8 */
    GSVtxValue3 pos;             /* 0x4AC */
    GSVtxEmitFn emitNrm;         /* 0x4C4 */
    GSVtxValue3 nrm;             /* 0x4C8 */
    GSVtxEmitFn emitClr[2];      /* 0x4E0 */
    GSVtxColor clr[2];           /* 0x4E8 */
    GSVtxEmitFn emitTex[8];      /* 0x500 */
    GSVtxValue2 tex[8];          /* 0x520 */
} GSgfxState;                    /* 0x5A0 */

/* The older name of the same struct. */
typedef GSgfxState GSgfxLayerState;

/* frameTarget between frames (fn_800D2B90 tests it as `+ 0x01020000 == 0xFEFE`). */
#define GSGFX_NO_FRAME ((void*)0xFEFEFEFE)

/* Byte offset of a GSgfxState field: a constant, so code that must keep its byte-pointer form to match (see the
 * vertex-cache emitters in gs_gfx_layer_state.c) stays correct on the PC, where the pointer fields grow. */
#ifdef TARGET_PC
#include <stddef.h>
#define GSGFX_OFFSET(field) offsetof(GSgfxState, field)
#else
#define GSGFX_OFFSET(field) ((u32)&((GSgfxState*)0)->field)
#endif

#ifndef TARGET_PC
/* GameCube layout check (no code is generated): the offsets the matched code was built against. */
typedef char GSgfxState_check_size[sizeof(GSgfxState) == 0x5A0 ? 1 : -1];
typedef char GSVtxDesc_check_size[sizeof(GSVtxDesc) == 0x190 ? 1 : -1];
typedef char GSVtxAttr_check_size[sizeof(GSVtxAttr) == 0x1C ? 1 : -1];
typedef char GSgfxState_check_vtxDesc[GSGFX_OFFSET(vtxDesc) == 0x024 ? 1 : -1];
typedef char GSgfxState_check_tev[GSGFX_OFFSET(tev) == 0x060 ? 1 : -1];
typedef char GSgfxState_check_dirty[GSGFX_OFFSET(dirty) == 0x414 ? 1 : -1];
typedef char GSgfxState_check_stageSrc[GSGFX_OFFSET(stageSrc) == 0x42E ? 1 : -1];
typedef char GSgfxState_check_captureActive[GSGFX_OFFSET(captureActive) == 0x47E ? 1 : -1];
typedef char GSgfxState_check_requestBuffer[GSGFX_OFFSET(requestBuffer) == 0x490 ? 1 : -1];
typedef char GSgfxState_check_vtxPending[GSGFX_OFFSET(vtxPending) == 0x49F ? 1 : -1];
typedef char GSgfxState_check_pos[GSGFX_OFFSET(pos) == 0x4AC ? 1 : -1];
typedef char GSgfxState_check_nrm[GSGFX_OFFSET(nrm) == 0x4C8 ? 1 : -1];
typedef char GSgfxState_check_clr[GSGFX_OFFSET(clr) == 0x4E8 ? 1 : -1];
typedef char GSgfxState_check_tex[GSGFX_OFFSET(tex) == 0x520 ? 1 : -1];
#endif

/* Per-frame GX state upload counters. */
typedef struct GSgfxStats {
    u32 pad_00[6];
    u32 chanUpdates;             /* 0x18 */
    u32 texGenUpdates;           /* 0x1C */
    u32 tevUpdates;              /* 0x20 */
} GSgfxStats;

extern GSgfxState* gsgfxState;

/*
 * GSgfx matrix stack (gfxMatrixStackInit allocates `count` 3x4 matrices): lbl_8047AABC is the first entry, lbl_8047AAC4 the
 * last, and lbl_8047AAC0 the current one (the model-view matrix loaded into GX position / normal slot 0). It starts at
 * the last entry; gfxPopMatrix moves it up (refused at the last entry), the pop functions move it down (refused at the
 * first). Likely, from the bounds checks and their messages.
 */
typedef f32 GSMtxStackEntry[3][4];
extern GSMtxStackEntry* lbl_8047AABC;
extern GSMtxStackEntry* lbl_8047AAC0;
extern GSMtxStackEntry* lbl_8047AAC4;

/*
 * GX-style setters for the block above. None exists out of line; each is
 * expanded at every use (four TEV setters per case in fn_800D963C's five
 * presets, the channel and TEV setters again throughout fn_800D892C), and
 * each ends by setting its dirty bit.
 */
static inline void GSgfxSetNumChans(u8 num) {
    gsgfxState->tev.numChans = num;
    gsgfxState->dirty |= 1;
}

/* chan follows GXChannelID: 4 and 5 (GX_COLOR0A0 / GX_COLOR1A1) set a
 * colour channel and the entry after it. */
static inline void GSgfxSetChanCtrl(s32 chan, u8 enable, u8 ambSrc, u8 matSrc, u8 lightMask, u8 diffFn, u8 attnFn) {
    u8 both = FALSE;
    u8 done;
    GSChanCtrl* ctrl;

    if (chan == 4) {
        chan = 0;
        both = TRUE;
    } else if (chan == 5) {
        chan = 1;
        both = TRUE;
    }
    done = FALSE;
    while (!done) {
        ctrl = &gsgfxState->tev.chanCtrl[chan];
        ctrl->enable = enable;
        ctrl->ambSrc = ambSrc;
        ctrl->matSrc = matSrc;
        ctrl->lightMask = lightMask;
        ctrl->diffFn = diffFn;
        ctrl->attnFn = attnFn;
        if (both) {
            both = FALSE;
            chan++;
        } else {
            done = TRUE;
        }
    }
    gsgfxState->dirty |= 1;
}

static inline void GSgfxSetNumTexGens(u8 num) {
    gsgfxState->tev.numTexGens = num;
    gsgfxState->dirty |= 2;
}

static inline void GSgfxSetNumTevStages(u8 num) {
    gsgfxState->tev.numTevStages = num;
    gsgfxState->dirty |= 4;
}

static inline void GSgfxSetTevOrder(u32 stage, s32 coord, s32 map, s32 color) {
    GSTevOrder* order = &gsgfxState->tev.tevOrder[stage];

    order->coord = coord;
    order->map = map;
    order->color = color;
    gsgfxState->dirty |= 4;
}

static inline void GSgfxSetNumIndStages(u8 num) {
    gsgfxState->tev.numIndStages = num;
    gsgfxState->dirty |= 4;
}

static inline void GSgfxSetTevColorOp(u32 stage, u8 op, u8 bias, u8 scale, u8 clamp, u8 out) {
    GSTevOp* tevOp = &gsgfxState->tev.colorOp[stage];

    tevOp->op = op;
    tevOp->bias = bias;
    tevOp->scale = scale;
    tevOp->clamp = clamp;
    tevOp->out = out;
    gsgfxState->dirty |= 4;
}

static inline void GSgfxSetTevAlphaOp(u32 stage, u8 op, u8 bias, u8 scale, u8 clamp, u8 out) {
    GSTevOp* tevOp = &gsgfxState->tev.alphaOp[stage];

    tevOp->op = op;
    tevOp->bias = bias;
    tevOp->scale = scale;
    tevOp->clamp = clamp;
    tevOp->out = out;
    gsgfxState->dirty |= 4;
}

static inline void GSgfxSetTevColorIn(u32 stage, u8 a, u8 b, u8 c, u8 d) {
    GSTevIn* tevIn = &gsgfxState->tev.colorIn[stage];

    tevIn->a = a;
    tevIn->b = b;
    tevIn->c = c;
    tevIn->d = d;
    gsgfxState->dirty |= 4;
}

static inline void GSgfxSetTevAlphaIn(u32 stage, u8 a, u8 b, u8 c, u8 d) {
    GSTevIn* tevIn = &gsgfxState->tev.alphaIn[stage];

    tevIn->a = a;
    tevIn->b = b;
    tevIn->c = c;
    tevIn->d = d;
    gsgfxState->dirty |= 4;
}

#endif /* GS_GFX_LAYER_H */
