/**
 * @file gs_gfx.c
 * @brief GSgfx -- Genius Sonority graphics subsystem.
 *
 * Only the fn_-named leaf functions below (fn_800D3094, fn_800D30A0,
 * fn_800D30AC, GSgfxRenderEnd, GSgfxRenderLayer, fn_800D36B4, fn_800D377C) are
 * real members of this unit; several are already byte-matched.
 *
 * This file previously also carried a second, parallel implementation of
 * the unit under invented friendly names (GSgfxInit, GSgfxSetVideoMode,
 * GSgfxEnableRendering, GSgfxSwapBuffers, GSgfxSetDrawMode,
 * GSgfxGetFrameCount, GSgfxGetTickCount), guarded by `#ifdef PCPORT`.
 * PCPORT is never defined anywhere in this build (no such target exists
 * in configure.py), so that whole block was dead code that could never
 * be compiled into any real target. None of the seven names appear in
 * config/GC6E01/symbols.txt, and their claimed sizes didn't consistently
 * match the real fn_ symbols living at those addresses (e.g. fn_800D3088
 * is really 0xC bytes, not the claimed 0x68; GSgfxSwapBuffers is really 0xA0,
 * not the claimed 0xF0). It was fiction from the old campaign transplant
 * and has been deleted wholesale.
 *
 * Address range: 0x800D3074 - 0x800D3E4C (approx.)
 */

#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#include "game/gs_gfx.h"
#include "game/gs_model.h"

/* ===== Data referenced by the real fn_ functions below ===== */
extern u8 lbl_804001F0[];  /* GX state / FIFO state block */

#ifndef PCPORT
void fn_800D3074(u32 flag) {
    if (flag == 0) {
        return;
    }

    gsgfxState->minFrameRetraces = flag;
}

u32 GSgetFrameStep(void) {
    return gsgfxState->frameRetraces;
}

u32 fn_800D3094(void) {
    return gsgfxState->retraceCount;
}
void fn_800D30A0(GSgfxFrameCallback callback) {
    gsgfxState->retraceCallback = callback;
}
extern void gfxRecordCommand(s32 arg0, ...);
extern void GXFlush(void);
#if 0
asm void fn_800D30AC(void) {
#include "src/game/gs_gfx_fn_800D30AC.inc"
}
#else
void fn_800D30AC(void) {
    GSgfxState* state = gsgfxState;

    if (state->recordMode == 1) {
        gfxRecordCommand(4, 0);
    } else {
        GXFlush();
    }
}
#endif
extern void GXSetDrawDone(void);
extern void OSYieldThread(void);
extern void fn_8019C6FC(void);
extern void GSgfxBackFBDoFrame(void);
extern void fn_801BF8A0(s32 a);
extern void fn_801E16F0(void);
extern void HSD_VIFinishDrawXFB(void);
extern void GStextureConvertFromHW(void* a, s32 b);
extern void GXPixModeSync(void);
extern void GXInvalidateTexAll(void);
extern void fn_800D1070(void* a);
extern void GSlightUpdate(void* a);
extern void GSmodelAdvanceAll(void* a, s32 b);
extern void gsParticleUpdateAll(s32 a);
extern u32 OSGetTick(void);
extern u8 lbl_8047AA91;
extern u8 lbl_8047AA90;

void GSgfxSwapBuffers(u32 flag)
{
    u32 sc = (u32)gsgfxState->frameTarget;

    if ((u32)(sc + 0x01020000U) == 0xFEFEU) {
        return;
    }

    if (gsgfxState->frameBegun == 0) {
        GSgfxBackFBDoFrame();
        fn_801BF8A0(0);
        fn_801E16F0();
        HSD_VIFinishDrawXFB();
    } else {
        if (sc != 0) {
            GStextureConvertFromHW((void*)sc, 1);
            flag = 0;
        }
    }

    gsgfxState->frameBegun = 1;

    if ((u8)flag != 0) {
        GXPixModeSync();
        if (gsgfxState->frameTarget != NULL) {
            GXInvalidateTexAll();
        }
    }
}

#if 0
asm void fn_800D3190(void) {
#include "src/game/gs_gfx_fn_800D3190.inc"
}
#else
void GSgfxRenderEnd(void) {
    GSgfxState* state;
    u32 sc;
    u32 startTick;
    u32* r31ptr;
    u8 r30;
    u32 tick;
    u32 div;
    s32 r29count;
    u8 r29b;
    GSgfxState* s4;
    u32 chk;

    state = gsgfxState;
    sc = (u32)state->frameTarget;
    if ((u32)(sc + 0x01020000U) == 0xFEFEU) {
        return;
    }

    lbl_8047AA91 = 0;
    GXFlush();
    GXSetDrawDone();
    startTick = OSGetTick();
    r31ptr = (u32*)0x80000000;
    r30 = 1;

    while (lbl_8047AA91 == 0) {
        OSYieldThread();
        if (lbl_8047AA91 == 0) {
            tick = OSGetTick();
            div = (tick - startTick) / (r31ptr[0xF8 / 4] >> 2);
            if (div > 3) {
                lbl_8047AA91 = r30;
            }
        }
    }

    fn_8019C6FC();

    s4 = gsgfxState;
    sc = (u32)s4->frameTarget;
    if (sc != 0) {
        chk = sc + 0x01020000U;
        r29b = 1;
        if (chk != 0xFEFEU) {
            if (s4->frameBegun == 0) {
                GSgfxBackFBDoFrame();
                fn_801BF8A0(0);
                fn_801E16F0();
                HSD_VIFinishDrawXFB();
            } else {
                if (sc != 0) {
                    GStextureConvertFromHW((void*)sc, 1);
                    r29b = 0;
                }
            }
            gsgfxState->frameBegun = 1;
            if (r29b != 0) {
                GXPixModeSync();
                if (gsgfxState->frameTarget != NULL) {
                    GXInvalidateTexAll();
                }
            }
        }
    } else {
        u32* p = (u32*)&lbl_804001F0;
        p[0x0 / 4] = p[0x4 / 4];
        p[0x8 / 4] = p[0xC / 4];
        p[0x10 / 4] += 1;
        p[0x4 / 4] = 0;
        p[0xC / 4] = 0;
    }

    gsgfxState->frameTarget = GSGFX_NO_FRAME;

    if (lbl_8047AA90 == 0) {
        return;
    }

    startTick = OSGetTick();
    fn_800D1070((void*)gsgfxState->frameRetraces);
    tick = OSGetTick();
    ((u32*)&lbl_804001F0)[0x2C / 4] += tick - startTick;

    startTick = OSGetTick();
    GSlightUpdate((void*)gsgfxState->frameRetraces);
    tick = OSGetTick();
    ((u32*)&lbl_804001F0)[0x30 / 4] += tick - startTick;

    startTick = OSGetTick();
    GSmodelAdvanceAll((void*)gsgfxState->frameRetraces, 0);
    tick = OSGetTick();
    ((u32*)&lbl_804001F0)[0x34 / 4] += tick - startTick;

    r29count = ((s32)gsgfxState->frameRetraces);
    ((u32*)&lbl_804001F0)[0x38 / 4] = 0;

    while (r29count-- != 0) {
        startTick = OSGetTick();
        GSmodelAdvanceAll((void*)1, 1);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x34 / 4] += tick - startTick;

        startTick = OSGetTick();
        gsParticleUpdateAll(1);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x38 / 4] += tick - startTick;
    }
}
#endif
extern void fn_800D13C4(void* a);
extern void fn_800DC874(void* a);
extern void gsParticleUpdate(void* a);
extern void modelShadowRender__FP10GSgfxLayer();
extern void HSD_StartRender(s32 a);
extern void HSD_SetEraseColor(u8 a, u8 b, u8 c, u8 d);
extern void HSD_EraseRect(f32 top, f32 bottom, f32 left, f32 right, f32 z, s32 color, s32 alpha, s32 depth);
extern u8 lbl_8047AA90;
extern f32 lbl_8047CA00;
extern f32 lbl_8047CA08;
extern f32 lbl_8047CA04;
#if 0
asm void fn_800D3410(void) {
#include "src/game/gs_gfx_fn_800D3410.inc"
}
#else
void GSgfxRenderLayer(void* arg0, u8 arg1) {
    GSgfxState* state;
    u32 sc;
    u8 r30;
    u32 startTick;
    u32 tick;

    state = gsgfxState;
    sc = (u32)state->frameTarget;

    if ((u32)(sc + 0x01020000U) == 0xFEFEU) {
        /* no frame open: arg0 becomes the frame target */
        state->frameTarget = arg0;
        r30 = 0;

        {
            GSgfxState* s4 = gsgfxState;
            sc = (u32)s4->frameTarget;
            if ((u32)(sc + 0x01020000U) != 0xFEFEU) {
                if (s4->frameBegun == 0) {
                    GSgfxBackFBDoFrame();
                    fn_801BF8A0(0);
                    fn_801E16F0();
                    HSD_VIFinishDrawXFB();
                } else {
                    if (sc != 0) {
                        GStextureConvertFromHW((void*)sc, 1);
                        r30 = 0;
                    }
                }
                gsgfxState->frameBegun = 1;
                if (r30 != 0) {
                    GXPixModeSync();
                    if (gsgfxState->frameTarget != NULL) {
                        GXInvalidateTexAll();
                    }
                }
            }
        }

    /* store arg1 into lbl_8047AA90 (SDA21 var) */
    lbl_8047AA90 = arg1;

    if (arg1 != 0) {
        /* reload state sub ptr and timing base inline each call (matches target) */
        startTick = OSGetTick();
        fn_800D13C4((void*)gsgfxState->frameRetraces);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x2C / 4] = tick - startTick;

        startTick = OSGetTick();
        fn_800DC874((void*)gsgfxState->frameRetraces);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x30 / 4] = tick - startTick;

        startTick = OSGetTick();
        GSmodelApplyAll((void*)gsgfxState->frameRetraces);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x34 / 4] = tick - startTick;

        startTick = OSGetTick();
        gsParticleUpdate((void*)gsgfxState->frameRetraces);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x38 / 4] = tick - startTick;
    }

    if (arg0 == 0) {
        startTick = OSGetTick();
        modelShadowRender__FP10GSgfxLayer(startTick);
        tick = OSGetTick();
        ((u32*)&lbl_804001F0)[0x3C / 4] = tick - startTick;

        HSD_StartRender(0);

        if (gsgfxState->eraseEnabled != 0) {
            HSD_SetEraseColor(
                gsgfxState->eraseColor.r,
                gsgfxState->eraseColor.g,
                gsgfxState->eraseColor.b,
                gsgfxState->eraseColor.a
            );
            HSD_EraseRect(lbl_8047CA00,
                lbl_8047CA08,
                lbl_8047CA00,
                lbl_8047CA04,
                lbl_8047CA00,
                1, 1, 0);
        }

        gsgfxState->frameBegun = 0;
    } else {
        HSD_StartRender(3);
    }
    }
}
#endif

extern void VIWaitForRetrace(void);
void GSgfxWaitRetrace(u8 mode) {
    u32 diff;

    VIWaitForRetrace();
    diff = gsgfxState->retraceCount -
           gsgfxState->lastRetrace;
    gsgfxState->frameRetraces = diff;

    if ((mode == 1) != 0) {
        while (gsgfxState->frameRetraces <
               gsgfxState->minFrameRetraces) {
            VIWaitForRetrace();
            diff = gsgfxState->retraceCount -
                   gsgfxState->lastRetrace;
            gsgfxState->frameRetraces = diff;
        }

        if (gsgfxState->waitRetrace == 0) {
            gsgfxState->frameRetraces =
                gsgfxState->minFrameRetraces;
        }
    }

    gsgfxState->lastRetrace =
        gsgfxState->retraceCount;
}

/* fn_800D36B4 (fog colour setter) is linked from gs_gfx_range_800D2B90.c. */
extern void fn_8019C690();
void fn_800D377C(s32 mode) {
    switch (mode) {
    case 1:
        fn_8019C690(0, 0);
        break;
    case 2:
        fn_8019C690(1, 0);
        break;
    }
}

extern s32 GSgfxVideoVsyncRate;
s32 fn_800D37CC(void) {
    return GSgfxVideoVsyncRate;
}

/* fn_800D37D4 and GSgfxInit, with the render-mode tables they own. */
#include "src/game/gs_gfx_candidate_800D37D4.c"
#endif /* !PCPORT */
