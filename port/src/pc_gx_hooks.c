/*
 * pc_gx_hooks.c: GX callbacks delivered the GameCube way, on the game thread (docs/port/sdk-threads.md).
 *
 * On the GameCube the GPU raises an interrupt when it reaches a draw-done token, and the draw-done callback runs as
 * that interrupt's handler on the one CPU. Aurora processes GX commands on its FIFO worker thread and calls the
 * callback there, so game code would run on a second thread, in parallel with the game. Here Aurora calls a
 * trampoline, which only posts the callback; the SDK thread scheduler runs it on the game thread at the next point
 * where interrupts are enabled (pc_os_post_interrupt, pc_os_thread.c).
 *
 * Who uses it: HSD's video code installs fn_801BFCB0 (its draw-done handler, src/hsd/video.c) through
 * fn_800B90A4, the GameCube address of GXSetDrawDoneCallback; GSgfx's own handler (fn_800D3F50) hangs off HSD's.
 * The real name GXSetDrawDoneCallback stays Aurora's (direct callers get Aurora's thread); fn_800B90A4 is this
 * file's until the address names are mapped for the link.
 */
#include <dolphin/types.h>

#include <SDL3/SDL_atomic.h>
#include <math.h>
#include <stdio.h>

typedef void (*GXDrawDoneCallback)(void);
typedef void (*GXDrawSyncCallback)(u16 token);
typedef struct {
    u16 r[10];
} GXFogAdjTable; /* Aurora's dolphin/gx/GXStruct.h */

extern GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb); /* Aurora's */
extern void pc_os_post_interrupt(void (*handler)(void));
extern int pc_os_wait_posted_interrupt(u32 timeout_ms);

static GXDrawDoneCallback s_draw_done;

static void deliver_draw_done(void) { /* game thread, interrupts disabled */
    GXDrawDoneCallback cb = s_draw_done;
    if (cb != NULL) {
        cb();
    }
}

/* The replay clock (pc_vi.c): a posted handler would run at whichever interrupt point follows the GPU worker, a moment
 * set by the host's speed. So with the replay clock the worker only counts the draw-done, and the handler runs at a
 * fixed point: the first interrupt point after the game issued it (GXSetDrawDone, through Aurora's issued hook), or
 * GXWaitDrawDone, each after draining the GPU worker. Corrected 2026-10-19: holding it for GXWaitDrawDone or the next
 * retrace only made GSgfxRenderEnd (which issues one and spins on OSYieldThread until the handler sets its flag) run
 * into its timer fallback, so every frame took two retraces. */
extern int pc_vi_virtual_clock(void);
extern void AuroraGXSync(void);
extern void AuroraSetDrawDoneIssuedHook(void (*hook)(void));
extern void pc_os_set_deliver_hook(void (*hook)(void));
static SDL_AtomicInt s_draw_done_arrived;
static u32 s_draw_done_issued, s_draw_done_synced; /* game thread only */

static void draw_done_issued(void) { /* the caller's thread: the game thread */
    s_draw_done_issued++;
}

static void draw_done_trampoline(void) { /* Aurora's FIFO worker */
    if (pc_vi_virtual_clock()) {
        SDL_AddAtomicInt(&s_draw_done_arrived, 1);
        return;
    }
    pc_os_post_interrupt(deliver_draw_done);
}

/* game thread, interrupts disabled: every draw-done the GPU has reached by now */
static int run_arrived_draw_done(void) {
    int n = 0;
    AuroraGXSync();
    while (SDL_GetAtomicInt(&s_draw_done_arrived) > 0) {
        SDL_AddAtomicInt(&s_draw_done_arrived, -1);
        deliver_draw_done();
        n++;
    }
    return n;
}

/* pc_os_thread.c, at each interrupt point with the replay clock: a draw-done issued since the last point is handed
 * over now (after draining the GPU worker, so it has certainly arrived) */
static void deliver_issued_draw_done(void) {
    if (s_draw_done_issued != s_draw_done_synced) {
        s_draw_done_synced = s_draw_done_issued;
        run_arrived_draw_done();
    } else if (SDL_GetAtomicInt(&s_draw_done_arrived) > 0) {
        /* from GXDrawDone, which drains the GPU before it returns: already here at a fixed point. (One from an issued
         * GXSetDrawDone can't be seen earlier: the first point after the issue drains and delivers it.) */
        while (SDL_GetAtomicInt(&s_draw_done_arrived) > 0) {
            SDL_AddAtomicInt(&s_draw_done_arrived, -1);
            deliver_draw_done();
        }
    }
}

/* pc_vi.c, in the retrace interrupt with the replay clock (kept as a no-op entry point: delivery is per interrupt
 * point now) */
void pc_gx_virtual_retrace(void) {}

GXDrawDoneCallback fn_800B90A4(GXDrawDoneCallback cb) {
    GXDrawDoneCallback old = s_draw_done;
    s_draw_done = cb;
    GXSetDrawDoneCallback(cb != NULL ? draw_done_trampoline : NULL);
    if (pc_vi_virtual_clock()) {
        AuroraSetDrawDoneIssuedHook(draw_done_issued);
        pc_os_set_deliver_hook(deliver_issued_draw_done);
    }
    return old;
}

/* GXWaitDrawDone (fn_800B8DA8): HSD loops "while (draw done still pending) GXWaitDrawDone();" and its posted
 * draw-done handler clears the flag; so wait until a posted interrupt has run (100 ms at most, then the loop simply
 * calls again). */
void GXWaitDrawDone(void) {
    if (pc_vi_virtual_clock()) {
        extern BOOL OSDisableInterrupts(void);
        extern BOOL OSRestoreInterrupts(BOOL level);
        const BOOL enabled = OSDisableInterrupts();
        if (run_arrived_draw_done() == 0) {
            fprintf(stderr, "[pc_gx] GXWaitDrawDone: no draw done after draining the GPU worker (replay clock)\n");
        }
        OSRestoreInterrupts(enabled);
        return;
    }
    if (!pc_os_wait_posted_interrupt(100)) {
        fprintf(stderr, "[pc_gx] GXWaitDrawDone: no draw done within 100 ms\n");
    }
}

/* Draw-sync tokens (GXSetDrawSync / GXReadDrawSync / GXSetDrawSyncCallback; GSgapp registers a callback). On the
 * GameCube the GPU reports the token when it reaches it in the command stream. Aurora has no token command yet, so
 * the token is reported at once (guess: good enough for GSgapp, which uses it to time its frame; recheck if a
 * timing display looks wrong). The callback runs on the game thread as a posted interrupt. */
static GXDrawSyncCallback s_draw_sync;
static u16 s_token, s_token_reported;

static void deliver_draw_sync(void) {
    s_token_reported = s_token;
    if (s_draw_sync != NULL) {
        s_draw_sync(s_token_reported);
    }
}

void GXSetDrawSync(u16 token) {
    s_token = token;
    pc_os_post_interrupt(deliver_draw_sync);
}

u16 GXReadDrawSync(void) { return s_token_reported; }

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {
    GXDrawSyncCallback old = s_draw_sync;
    s_draw_sync = cb;
    return old;
}

/* GXSetCopyClamp: Aurora leaves it as a TODO; the clamp only changes how the copy filter treats the top and bottom
 * edge lines. PORT: no effect yet. */
void GXSetCopyClamp(u32 clamp) { (void)clamp; }

/* Performance counters (GSgapp's debug display): no GPU counters on the PC; they read as 0. */
void GXSetGPMetric(u32 perf0, u32 perf1) {
    (void)perf0;
    (void)perf1;
}
void GXClearGPMetric(void) {}
u32 GXReadGP0Metric(void) { return 0; } /* GSgapp reads it in its draw-sync callback (token 3) */
u32 GXReadGP1Metric(void) { return 0; }
void GXReadGPMetric(u32* cnt0, u32* cnt1) {
    if (cnt0 != NULL) {
        *cnt0 = 0;
    }
    if (cnt1 != NULL) {
        *cnt1 = 0;
    }
}

/* GXInitFogAdjTable: the fog range-adjustment table for GXSetFogRangeAdj. Fog on the GameCube depends on depth (z),
 * not on the distance to the eye, so objects at the screen edge get too little fog; the table holds, for 10 steps
 * from the screen centre to the edge, the factor distance / depth = sqrt(1 + (x / focal)^2), in 8.8 fixed point
 * (256 = 1.0). Derived here from the projection, not copied: guess for the exact step positions and scale; Aurora
 * stores the registers but whether its renderer applies them is not checked. */
void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, const f32 projmtx[4][4]) {
    int i;
    f32 focal; /* the projection's x scale in pixels: x_screen = focal * X / Z */
    if (table == NULL || width == 0) {
        return;
    }
    focal = projmtx[0][0] * (f32)width * 0.5f;
    for (i = 0; i < 10; i++) {
        f32 x = ((f32)i + 0.5f) * (f32)width / 20.0f; /* pixels from the centre, steps across half the width */
        f32 r = projmtx[3][3] == 0.0f && focal > 0.0f ? sqrtf(1.0f + (x / focal) * (x / focal)) : 1.0f;
        u32 v = (u32)(r * 256.0f + 0.5f);
        table->r[i] = (u16)(v > 0xFFF ? 0xFFF : v);
    }
}
