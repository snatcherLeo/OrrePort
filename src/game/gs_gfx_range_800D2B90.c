/**
 * @file gs_gfx_range_800D2B90.c
 * @brief GS render/graphics TU span 0x800D2B90 - 0x800D377C with its
 *        .sdata2 literal pool 0x8047C9F0 - 0x8047CA10.
 *
 * One object for the erase-colour setter fn_800D2B90, the camera screen projections
 * gfxProjectPoint/fn_800D2F34, the render-state accessors fn_800D305C/
 * fn_800D3068, the GSgfx frame functions fn_800D3074-GSgfxWaitRetrace and the
 * erase-colour setter fn_800D36B4. The pool entries 255.0f, the unsigned
 * int-to-float bias, 0.0f, 640.0f, 480.0f and 1.0f are used only by these
 * functions, so they are written as literals and the object owns them.
 * This replaces the former per-function carves and their extern stand-ins
 * (lbl_8047C9F0, lbl_8047CA00-lbl_8047CA0C).
 *
 * Compiler: GC/2.5 with the project defaults (-O4,p), no pragmas.
 * fn_800D36B4 schedules its by-value colour loads ahead of its stack frame
 * and the enable-byte store. GC/2.5-2.7 emit that; GC/1.0-2.0 keep the
 * loads behind the frame and the store. Every other function here is
 * identical under GC/1.3 and GC/2.5.
 *
 * The render-state block at gsgfxState is reached through two partial
 * reconstructions: GSgfxState (game/gs_gfx.h) for the frame functions and
 * GSgfxState (game/gs_gfx_layer.h) for the erase colour and accessors.
 */
#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#include "game/gs_gfx.h"
#include "game/gs_render_util.h"
#include "game/gs_model.h"
#include "hsd/hsd_object.h"
#include "hsd/hsd_class.h"

/* GSRenderColor: game/gs_render_util.h */

typedef f32 GSMtx34[3][4];
typedef f32 GSMtx44[4][4];

extern u32 lbl_8047AA8C; /* current fog */
extern GSRenderColor lbl_80270350;
extern void* HSD_FogLoadDesc(void* desc);
extern void HSD_FogSet(u32 fog);
extern void fn_8016EA88(void);
extern void psSetFog();
extern void* gfxGetCameraMatrix(u32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecTransform(f32* out, GSMtx34 m, const f32* in);
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void GXProject(f32 x, f32 y, f32 z, GSMtx34 mtx, const f32* pm,
                      const f32* vp, f32* sx, f32* sy, f32* sz);
#endif
extern void gfxRecordCommand(s32 arg0, ...);
extern void GXFlush(void);
extern void GSgfxBackFBDoFrame(void);
extern void fn_801BF8A0(s32 value);
extern void fn_801E16F0(void);
extern void HSD_VIFinishDrawXFB(void);
extern void GStextureConvertFromHW(void* texture, s32 mode);
extern void GXPixModeSync(void);
extern void GXInvalidateTexAll(void);
extern u32 lbl_804001F0[]; /* frame statistics block */
extern u8 lbl_8047AA90;    /* profiling enabled */
extern u8 lbl_8047AA91;    /* GX draw done */
extern void GXSetDrawDone(void);
extern void OSYieldThread(void);
extern u32 OSGetTick(void);
extern void fn_8019C6FC(void);
extern void fn_800D1070(u32 frames);
extern void GSlightUpdate(u32 frames);
extern void GSmodelAdvanceAll(u32 frames, s32 mode);
extern void gsParticleUpdateAll(s32 frames);
extern void fn_800D13C4(void* a);
extern void fn_800DC874(void* a);
extern void gsParticleUpdate(u32 frames); /* defined in field_range_80117E58.c */
extern void modelShadowRender__FP10GSgfxLayer();
extern void HSD_StartRender(s32 a);
extern void HSD_SetEraseColor(u8 a, u8 b, u8 c, u8 d);
extern void HSD_EraseRect(f32 top, f32 bottom, f32 left, f32 right, f32 z, s32 color, s32 alpha, s32 depth);
extern void VIWaitForRetrace(void);

/* fn_800D36B4's body, expanded into fn_800D2B90 with the colour passed by
 * value (retail copies it to a stack temporary first).
 * RULE-EXCEPTION(title-path): single-use inline helper duplicating
 * fn_800D36B4's body - see docs/RULE_EXCEPTIONS.md. fn_800D36B4 lies after
 * fn_800D2B90, so this non-deferred TU cannot inline the real function. */
static inline void gsSetEraseColor(GSRenderColor color) {
    gsgfxState->eraseEnabled = 1;
    gsgfxState->eraseColor.r = 255.0f * color.r;
    gsgfxState->eraseColor.g = 255.0f * color.g;
    gsgfxState->eraseColor.b = 255.0f * color.b;
    gsgfxState->eraseColor.a = 255.0f * color.a;
    if (gsgfxState->eraseColor.r == 0 && gsgfxState->eraseColor.g == 0 && gsgfxState->eraseColor.b == 0 &&
        gsgfxState->eraseColor.a == 0) {
        gsgfxState->eraseEnabled = 0;
    }
}

void gfxSetFog(void* arg1) {
    GSRenderColor color;
    void* prevLight;
    color = lbl_80270350;
    prevLight = (void*)lbl_8047AA8C;
    if (prevLight != NULL) {
        if (prevLight != NULL && ref_DEC(prevLight)) {
            hsdDelete(prevLight);
        }
        lbl_8047AA8C = 0;
    }
    if (arg1 == 0) {
        HSD_FogSet(0);
        fn_8016EA88();
        return;
    }
    lbl_8047AA8C = (u32)HSD_FogLoadDesc(arg1);
    psSetFog(lbl_8047AA8C);
    color.r = *(u8*)((u8*)arg1 + 0x10) / 255.0f;
    color.g = *(u8*)((u8*)arg1 + 0x11) / 255.0f;
    color.b = *(u8*)((u8*)arg1 + 0x12) / 255.0f;
    color.a = *(u8*)((u8*)arg1 + 0x13) / 255.0f;
    gsSetEraseColor(color);
}

/* 0x800D2DE8 | 0x14C */
s32 gfxProjectPoint(f32* points, f32* screen, u32 count)
{
    f32 projection[7];
    f32 viewport[6];
    f32 view[3];
    GSMtx44* proj;
    GSMtx34* viewMtx;
    u32 i;
    f32* src;
    f32* dst;
    s32 result;

    src = points;
    dst = screen;
    viewMtx = gfxGetCameraMatrix(0);
    proj = gfxGetCameraMatrix(2);
    if (viewMtx == NULL || proj == NULL) {
        return 0;
    }

    projection[0] = 0.0f;
    projection[1] = (*proj)[0][0];
    projection[2] = (*proj)[0][2];
    projection[3] = (*proj)[1][1];
    projection[4] = (*proj)[1][2];
    projection[5] = (*proj)[2][2];
    projection[6] = (*proj)[2][3];
    viewport[0] = 0.0f;
    viewport[1] = 0.0f;
    viewport[2] = 640.0f;
    viewport[3] = 480.0f;
    viewport[4] = 0.0f;
    viewport[5] = 1.0f;

    result = 2;
    for (i = 0; i < count; i++, src += 3, dst += 3) {
        GSvecTransform(view, *viewMtx, src);
        if (view[2] >= (*proj)[2][3]) {
            result = 3;
            dst[0] = dst[1] = dst[2] = 1.0f;
        } else {
            GXProject(src[0], src[1], src[2], *viewMtx, projection,
                      viewport, &dst[0], &dst[1], &dst[2]);
        }
    }
    return result;
}

/* 0x800D2F34 | 0x128 */
s32 gfxWorldToScreen(f32* point, f32* screen)
{
    f32 projection[7];
    f32 viewport[6];
    f32 view[3];
    GSMtx34* viewMtx;
    GSMtx44* proj;
    f32 far;

    viewMtx = gfxGetCameraMatrix(0);
    proj = gfxGetCameraMatrix(2);
    if (viewMtx == NULL || proj == NULL) {
        return 0;
    }

    GSvecTransform(view, *viewMtx, point);
    if (view[2] >= (far = (*proj)[2][3])) {
        return 1;
    }

    projection[0] = 0.0f;
    projection[1] = (*proj)[0][0];
    projection[2] = (*proj)[0][2];
    projection[3] = (*proj)[1][1];
    projection[4] = (*proj)[1][2];
    projection[5] = (*proj)[2][2];
    projection[6] = far;
    viewport[0] = 0.0f;
    viewport[1] = 0.0f;
    viewport[2] = 640.0f;
    viewport[3] = 480.0f;
    viewport[4] = 0.0f;
    viewport[5] = 1.0f;
    GXProject(point[0], point[1], point[2], *viewMtx, projection, viewport,
              &screen[0], &screen[1], &screen[2]);
    return 2;
}

void fn_800D305C(u8 level)
{
    gsgfxState->waitRetrace = level;
}

u32 fn_800D3068(void)
{
    return gsgfxState->minFrameRetraces;
}

void fn_800D3074(u32 flag)
{
    if (flag == 0) {
        return;
    }

#ifdef TARGET_PC
    {
        /* diagnostic (2026-10-25; docs/port/hook-points.md "Diagnostics"): ORRE_FPS_LOG=1 logs every frame-rate
         * change with its caller, to find which step of a battle's teardown sets 60 fps (P01's Folly battle) */
        static int s_log = -1;
        if (s_log < 0) {
            extern char* getenv(const char*);
            s_log = getenv("ORRE_FPS_LOG") != NULL;
        }
        if (s_log && gsgfxState->minFrameRetraces != flag) {
            extern volatile u32 pc_vi_heartbeat;
            extern void OSReport(const char* fmt, ...);
            OSReport("[fps] %u -> %u retraces per frame at game frame %u, from %p\n",
                     (unsigned)gsgfxState->minFrameRetraces, (unsigned)flag, (unsigned)pc_vi_heartbeat,
                     __builtin_return_address(0));
        }
    }
#endif
    gsgfxState->minFrameRetraces = flag;
}

u32 GSgetFrameStep(void)
{
    return gsgfxState->frameRetraces;
}

u32 fn_800D3094(void)
{
    return gsgfxState->retraceCount;
}

void fn_800D30A0(GSgfxFrameCallback callback)
{
    gsgfxState->retraceCallback = callback;
}

void fn_800D30AC(void)
{
    if (gsgfxState->recordMode == 1) {
        gfxRecordCommand(4, 0);
    } else {
        GXFlush();
    }
}

void GSgfxSwapBuffers(u32 flag)
{
    void* renderTarget = gsgfxState->frameTarget;

    if (renderTarget == GSGFX_NO_FRAME) {
        return;
    }

    if (gsgfxState->frameBegun == 0) {
        GSgfxBackFBDoFrame();
        fn_801BF8A0(0);
        fn_801E16F0();
        HSD_VIFinishDrawXFB();
    } else if (renderTarget != NULL) {
        GStextureConvertFromHW(renderTarget, 1);
        flag = 0;
    }

    gsgfxState->frameBegun = 1;

    if ((u8)flag != 0) {
        GXPixModeSync();
        if (gsgfxState->frameTarget != NULL) {
            GXInvalidateTexAll();
        }
    }
}

void GSgfxRenderEnd(void)
{
    u32 start;
    u32 end;
    void* renderTarget;
    u8 flag;
    s32 count;

    if (gsgfxState->frameTarget == GSGFX_NO_FRAME) {
        return;
    }

    lbl_8047AA91 = 0;
    GXFlush();
    GXSetDrawDone();
    start = OSGetTick();

    while (lbl_8047AA91 == 0) {
        OSYieldThread();
        if (lbl_8047AA91 == 0) {
            end = OSGetTick();
            if ((end - start) / (GS_OS_BUS_CLOCK / 4) > 3) {
                lbl_8047AA91 = 1;
            }
        }
    }

    fn_8019C6FC();

    renderTarget = gsgfxState->frameTarget;
    if (renderTarget != NULL) {
        flag = 1;
        if (renderTarget != GSGFX_NO_FRAME) {
            if (gsgfxState->frameBegun == 0) {
                GSgfxBackFBDoFrame();
                fn_801BF8A0(0);
                fn_801E16F0();
                HSD_VIFinishDrawXFB();
            } else if (renderTarget != NULL) {
                GStextureConvertFromHW(renderTarget, 1);
                flag = 0;
            }
            gsgfxState->frameBegun = 1;
            if (flag != 0) {
                GXPixModeSync();
                if (gsgfxState->frameTarget != NULL) {
                    GXInvalidateTexAll();
                }
            }
        }
    } else {
        lbl_804001F0[0] = lbl_804001F0[1];
        lbl_804001F0[2] = lbl_804001F0[3];
        lbl_804001F0[4] += 1;
        lbl_804001F0[1] = 0;
        lbl_804001F0[3] = 0;
    }

    gsgfxState->frameTarget = GSGFX_NO_FRAME;

    if (lbl_8047AA90 == 0) {
        return;
    }

    start = OSGetTick();
    fn_800D1070(gsgfxState->frameRetraces);
    end = OSGetTick();
    lbl_804001F0[11] += end - start;

    start = OSGetTick();
    GSlightUpdate(gsgfxState->frameRetraces);
    end = OSGetTick();
    lbl_804001F0[12] += end - start;

    start = OSGetTick();
    GSmodelAdvanceAll(gsgfxState->frameRetraces, 0);
    end = OSGetTick();
    lbl_804001F0[13] += end - start;

    count = gsgfxState->frameRetraces;
    lbl_804001F0[14] = 0;
#ifdef TARGET_PC
    {
        /* ORRE_PARTICLE_STEP_LOG=1 (2026-10-21; observes only): this path's per-retrace particle steps, with the
         * frame step and the pacing mode's fields. P02's battle 20: the console stepped the particles once more than
         * the PC in the frame that creates Wakin's team */
        static int on = -1;
        extern void OSReport(const char*, ...);
        extern u32 VIGetRetraceCount(void);
        if (on < 0) {
            extern char* getenv(const char*);
            const char* e = getenv("ORRE_PARTICLE_STEP_LOG");
            on = e != NULL && *e != '\0' && *e != '0';
        }
        if (on) {
            OSReport("[pstep] logic retrace %u steps %u min %u wait %u\n", (unsigned)VIGetRetraceCount(),
                     (unsigned)gsgfxState->frameRetraces, (unsigned)gsgfxState->minFrameRetraces,
                     (unsigned)gsgfxState->waitRetrace);
        }
    }
#endif
    while (count-- != 0) {
        start = OSGetTick();
        GSmodelAdvanceAll(1, 1);
        end = OSGetTick();
        lbl_804001F0[13] += end - start;

        start = OSGetTick();
        gsParticleUpdateAll(1);
        end = OSGetTick();
        lbl_804001F0[14] += end - start;
    }
}

void GSgfxRenderLayer(void* arg0, u8 arg1) {
    GSgfxState* state;
    u32 sc;
    u8 r30;
    u32 startTick;
    u32 tick;

    state = gsgfxState;
    sc = PC_PTR32(state->frameTarget);
#ifdef TARGET_PC
    {
        /* PC (observe only, 2026-11-02, port-gaps 52): ORRE_SHADOW_LOG=A-B also logs each render-layer call in
         * frames A..B (its target, NULL = the screen; whether a frame was already open), next to the shadow passes, to
         * see the order inside a frame at a battle camera cut */
        extern void OSReport(const char*, ...);
        extern char* getenv(const char*);
        extern volatile u32 pc_vi_heartbeat __attribute__((weak));
        static int on = -1;
        static unsigned lo, hi;
        const unsigned f = &pc_vi_heartbeat != NULL ? (unsigned)pc_vi_heartbeat : 0u;
        if (on < 0) {
            const char* env = getenv("ORRE_SHADOW_LOG");
            on = env != NULL && sscanf(env, "%u-%u", &lo, &hi) == 2;
        }
        extern u32 pc_jobj_disp_count; /* hsd/jobj.c: joints drawn since the last layer call */
        if (on && f >= lo && f <= hi) {
            OSReport("[layer] frame %u target %p updates %d open %d joints_drawn_since_last %u\n", f, arg0, (int)arg1,
                     (u32)(sc + 0x01020000U) != 0xFEFEU, (unsigned)pc_jobj_disp_count);
        }
        pc_jobj_disp_count = 0;
    }
#endif

    if ((u32)(sc + 0x01020000U) == 0xFEFEU) {
        state->frameTarget = arg0;
        r30 = 0;

        {
            GSgfxState* s4 = gsgfxState;
            sc = PC_PTR32(s4->frameTarget);
            if ((u32)(sc + 0x01020000U) != 0xFEFEU) {
                if (s4->frameBegun == 0) {
                    GSgfxBackFBDoFrame();
                    fn_801BF8A0(0);
                    fn_801E16F0();
                    HSD_VIFinishDrawXFB();
                } else {
                    if (sc != 0) {
                        GStextureConvertFromHW((void*)PC_PTR(sc), 1);
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

        lbl_8047AA90 = arg1;

        if (arg1 != 0) {
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

#ifdef TARGET_PC
            extern char* getenv(const char*);
            if (getenv("ORRE_PARTICLE_STEP_LOG") != NULL) { /* ORRE_PARTICLE_STEP_LOG: the render path's update */
                extern void OSReport(const char*, ...);
                extern u32 VIGetRetraceCount(void);
                OSReport("[pstep] render retrace %u steps %u\n", (unsigned)VIGetRetraceCount(),
                         (unsigned)gsgfxState->frameRetraces);
            }
#endif
            startTick = OSGetTick();
            gsParticleUpdate(gsgfxState->frameRetraces);
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
                HSD_EraseRect(0.0f,
                    480.0f,
                    0.0f,
                    640.0f,
                    0.0f,
                    1, 1, 0);
            }

            gsgfxState->frameBegun = 0;
        } else {
            HSD_StartRender(3);
        }
    }
}

void GSgfxWaitRetrace(u8 mode) {
#ifdef TARGET_PC
    /* OrreHook_FrameSkipWait (decision hook; docs/port/hook-points.md): nonzero = this game frame ends without waiting
     * for a retrace, a second logic pass in the same retrace. Default vanilla: NULL, the game always waits. Replay mode
     * registers it (port/src/pc_frame_times.c) where the console ran two logic passes in one retrace (a held fade-in
     * step caught up; gamecube-accuracy.md, "Logic passes at fade-ins"). It skips the frame-pacing wait below too */
    extern int (*OrreHook_FrameSkipWait)(void);
    const int skipWait = OrreHook_FrameSkipWait != NULL && OrreHook_FrameSkipWait();
    if (!skipWait)
#endif
    VIWaitForRetrace();
    gsgfxState->frameRetraces = gsgfxState->retraceCount - gsgfxState->lastRetrace;

    if (mode == 1) {
        while (
#ifdef TARGET_PC
               !skipWait &&
#endif
               gsgfxState->frameRetraces < gsgfxState->minFrameRetraces) {
            VIWaitForRetrace();
            gsgfxState->frameRetraces = gsgfxState->retraceCount - gsgfxState->lastRetrace;
        }

        if (gsgfxState->waitRetrace == 0) {
            gsgfxState->frameRetraces = gsgfxState->minFrameRetraces;
        }
    }
#ifdef TARGET_PC
    {
        /* OrreHook_FrameRetraces (decision hook; docs/port/hook-points.md): the least number of retraces this game frame
         * lasts. Default vanilla: nothing registered, nothing changes. Replay mode registers the console's frame
         * lengths (port/src/pc_frame_times.c), since heavy scenes ask for 60 fps but take two retraces on the console
         * (gamecube-accuracy.md row 4b; snatcherLeo, 2026-10-18) */
        extern u32 (*OrreHook_FrameRetraces)(void);
        u32 want = OrreHook_FrameRetraces != NULL ? OrreHook_FrameRetraces() : 0;
        if (gsgfxState->frameRetraces < want) {
            while (gsgfxState->frameRetraces < want) {
                VIWaitForRetrace();
                gsgfxState->frameRetraces = gsgfxState->retraceCount - gsgfxState->lastRetrace;
            }
            /* The hook stretches the frame; the step the game is told must stay the console's (2026-10-21): with
             * the wait-for-retrace mode off the console reports the minimum step however long the frame took (above),
             * and the recount here reported the stretched length instead, so particles, models and lights stepped
             * 2-3 times in the console's heavy frames (P02's battle 20: the intro particles a frame out of phase) */
            if (mode == 1 && gsgfxState->waitRetrace == 0) {
                gsgfxState->frameRetraces = gsgfxState->minFrameRetraces;
            }
        }
    }
    {
        /* OrreHook_FrameStep (decision hook; docs/port/hook-points.md): the step this game frame reports
         * (GSgetFrameStep), whatever its length. Default vanilla: NULL / 0, the measured length. Replay mode registers
         * it for the console's held and doubled logic passes: there one logic pass spans two retraces (or none) while
         * the console's step stays one retrace, so fades and timers advance as on the console */
        extern u32 (*OrreHook_FrameStep)(void);
        u32 step = OrreHook_FrameStep != NULL ? OrreHook_FrameStep() : 0;
        if (step != 0) {
            gsgfxState->frameRetraces = step;
        }
    }
#endif

    gsgfxState->lastRetrace = gsgfxState->retraceCount;
}

void fn_800D36B4(GSRenderColor color)
{
    gsgfxState->eraseEnabled = 1;
    gsgfxState->eraseColor.r = 255.0f * color.r;
    gsgfxState->eraseColor.g = 255.0f * color.g;
    gsgfxState->eraseColor.b = 255.0f * color.b;
    gsgfxState->eraseColor.a = 255.0f * color.a;
    if (gsgfxState->eraseColor.r == 0 && gsgfxState->eraseColor.g == 0 && gsgfxState->eraseColor.b == 0 &&
        gsgfxState->eraseColor.a == 0) {
        gsgfxState->eraseEnabled = 0;
    }
}
