/*
 * gs_thread_host.c: the GS engine's 7 thread-switch assembly routines, re-done on host fibres for OrrePort
 * (platform-layer plan decision 6; docs/port/asm-routines.md; design: docs/port/gs-fibres.md).
 *
 * The GameCube versions (src/game/gs_thread_exact_800F0030.c, first-party assembly) save and load the whole
 * PowerPC register file into a GSThreadCtx block and swap the stack pointer. That cannot exist on a PC, but
 * everything around it can stay exactly as decompiled: GSthreadCreate, GSthreadSetArgs, GSthreadTerminate, the
 * dispatcher fn_800F0A74 and the exit trampoline fn_800F0F4C (src/game/gs_thread_exact_800F0424.c) run unchanged
 * and only call these routines. So the port replaces just these 7, and the scheduler's own logic (which thread runs
 * next, priorities, blocking, termination) is the original's.
 *
 * How each routine maps (the scheduler's globals are the original ones):
 *   threadSaveGPRRegisters  called by GSthreadCreate to seed a new context: here it clears the GPR slots, which
 *                           also marks the context "not started" (see GS_HOST_CTX_LIVE).
 *   threadSaveFPRRegisters  same, for the FPR slots (GSthreadCreate with usesFPU).
 *   threadLoadGPRRegisters, threadLoadFPRRegisters
 *                           only ever called from inside the two switch routines on the GameCube; nothing to do.
 *   threadExecute           the dispatcher's "run this thread until it yields": switch from the scheduler's fibre
 *                           to the thread's fibre (creating it on the thread's first run).
 *   _threadSwitch           a thread's "yield until next time" (the per-frame wait the game calls everywhere):
 *                           switch back to the scheduler's fibre.
 *   fn_800F02F4             the landing pad threadExecute's frame returns through; a fibre switch simply returns,
 *                           so it is never reached (the dispatcher still stores its address in the scheduler
 *                           context, which is harmless).
 *
 * Data model: the scheduler keeps thread and context pointers in u32 globals (_activeThread etc.), so GSmem and the
 * thread array must live inside the GameCube memory range mapped at 0x80000000 (pc_mem.h). Function pointers are
 * not truncated: the entry point comes from GSThread.entryFunc (a real pointer), and the exit path calls
 * fn_800F0F4C directly instead of through the u32 ctx->ctr slot.
 */
#include "dolphin/types.h"
#include "game/gs_thread.h"

#include "pc_fibre.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The original scheduler globals (names from symbols.txt; meaning in gs_thread_exact_800F0424.c) */
extern u32 _activeThread;       /* current thread (GSThread*) */
extern u32 _destRegStatePtr;       /* context block for the register save / load routines (GSThreadCtx*) */
extern u32 _threadRegStatePtr;       /* context to run (GSThreadCtx*) */
extern GSThread* _threadList; /* thread array */
extern u32 _maxThreads;       /* maximum thread count */

extern void fn_800F0F4C(u32 result); /* exit trampoline: records the result, retires the thread, yields for good */

#define GS_HOST_MAX_THREADS 64
/* Host stack per GS thread. The GameCube sizes (GSthreadCreate's stackSize) are for compact PowerPC frames; x86-64
 * frames and host library calls (Aurora, printf) need far more, and only the touched part is committed. */
#define GS_HOST_STACK_SIZE (1024 * 1024)
/* Marker in a context's gpr[2] slot (r2 is the small-data base on the GameCube and means nothing on the PC): the
 * thread's fibre exists. threadSaveGPRRegisters clears it, so a recreated thread in a reused slot gets a new fibre. */
#define GS_HOST_CTX_LIVE 0x4F525245u /* "ORRE" */

typedef struct GsHostThread {
    PcFibre* fibre;
    PcFibre* caller; /* the fibre whose threadExecute is running this thread now: _threadSwitch returns there */
    GSThread* thread;
    u32 args[8]; /* r3-r10 as GSthreadSetArgs left them, copied at the first run */
} GsHostThread;

/* Why each thread remembers its own caller (B, 2026-10-16; docs/port/gs-fibres.md, "Dispatch from an interrupt"):
 * the dispatcher does not only run from the main loop. The retrace handler (gapp task mainRetraceDraw -> fn_801E0FB4)
 * runs a thread group while the main game is blocked, and on the PC a "retrace interrupt" is taken on whichever SDK
 * thread is current (a THP decoder, for example). A GS thread started there can wait at the SDK level, the main
 * thread can run the dispatcher meanwhile, and a single global "scheduler fibre" was then overwritten: the first
 * thread later yielded into the main thread's fibre while the SDK scheduler still had the decoder as current, and
 * threads resumed in the wrong places (the THP audio decoder ran past its own OSSuspendThread and read past the
 * movie: the crash in tpc.thp). So the return fibre is per thread, and "inside a GS thread" is decided by which
 * fibre is running, not by a global. */
static GsHostThread s_threads[GS_HOST_MAX_THREADS];

/* The GS thread whose fibre is running now, or NULL */
static GsHostThread* running_host(void) {
    PcFibre* cur = pc_fibre_current();
    int i;
    if (cur == NULL) {
        return NULL;
    }
    for (i = 0; i < GS_HOST_MAX_THREADS; i++) {
        if (s_threads[i].fibre == cur) {
            return &s_threads[i];
        }
    }
    return NULL;
}

#define GC_PTR(type, u32value) ((type*)(uintptr_t)(u32value))

typedef u32 (*GsThreadEntry)(u32, u32, u32, u32, u32, u32, u32, u32);

static void gs_host_fibre_main(void* param) {
    GsHostThread* host = (GsHostThread*)param;
    /* The GameCube jumps to the entry with r3-r10 loaded from the context (GSthreadSetArgs puts arguments there)
     * and LR = fn_800F0F4C, so the entry's return value lands in fn_800F0F4C's first argument. Passing all eight
     * slots is safe for entries that take fewer (the caller cleans up on both x86-64 ABIs). Arguments past eight
     * (spilled to the thread's GameCube stack by GSthreadSetArgs) are not supported: no caller is known to use them. */
    GsThreadEntry entry = (GsThreadEntry)host->thread->entryFunc;
    u32 result = entry(host->args[0], host->args[1], host->args[2], host->args[3], host->args[4], host->args[5],
                       host->args[6], host->args[7]);
    fn_800F0F4C(result);
    /* fn_800F0F4C ends in _threadSwitch and the scheduler never runs a finished thread again */
    for (;;) {
        fprintf(stderr, "[gs_thread_host] a finished GS thread was resumed\n");
        pc_fibre_switch(host->caller);
    }
}

void threadSaveGPRRegisters(void) {
    GSThreadCtx* ctx = GC_PTR(GSThreadCtx, _destRegStatePtr);
    memset(ctx->gpr, 0, sizeof(ctx->gpr));
    ctx->lr = 0;
    ctx->ctr = 0;
}

void threadSaveFPRRegisters(void) {
    GSThreadCtx* ctx = GC_PTR(GSThreadCtx, _destRegStatePtr);
    memset(ctx->fpr, 0, sizeof(ctx->fpr));
}

void threadLoadGPRRegisters(void) {}

void threadLoadFPRRegisters(void) {}

void threadExecute(void) {
    GSThreadCtx* ctx = GC_PTR(GSThreadCtx, _threadRegStatePtr);
    GSThread* thread = GC_PTR(GSThread, _activeThread);
    long slot = thread - _threadList;
    GsHostThread* host;
    GsHostThread* inside = running_host();

    if (inside != NULL) {
        /* The GameCube has one scheduler context (lbl_804019F0), so a GS thread cannot run the dispatcher itself */
        fprintf(stderr, "[gs_thread_host] threadExecute called from inside GS thread %ld; ignored\n",
                (long)(inside - s_threads));
        return;
    }
    if (slot < 0 || slot >= GS_HOST_MAX_THREADS || (u32)slot >= _maxThreads) {
        fprintf(stderr, "[gs_thread_host] thread %p is outside the thread array (slot %ld)\n", (void*)thread, slot);
        abort();
    }
    host = &s_threads[slot];

    if (pc_fibre_current() == NULL) {
        (void)pc_fibre_init_main();
    }
    if (ctx->gpr[2] != GS_HOST_CTX_LIVE) {
        /* First run of a thread created in this slot. A fibre left from an earlier thread in the slot is dead (its
         * thread finished or was terminated, and the scheduler never resumes it), so free it. */
        if (host->fibre != NULL) {
            pc_fibre_destroy(host->fibre);
        }
        host->thread = thread;
        memcpy(host->args, &ctx->gpr[3], sizeof(host->args));
        host->fibre = pc_fibre_create(GS_HOST_STACK_SIZE, gs_host_fibre_main, host);
        if (host->fibre == NULL) {
            fprintf(stderr, "[gs_thread_host] cannot create a fibre for GS thread slot %ld\n", slot);
            abort();
        }
        ctx->gpr[2] = GS_HOST_CTX_LIVE;
    }

    host->caller = pc_fibre_current();
    pc_fibre_switch(host->fibre);
}

void fn_800F02F4(void) {
    fprintf(stderr, "[gs_thread_host] fn_800F02F4 (GameCube-only landing pad) was called\n");
}

void _threadSwitch(void) {
    GsHostThread* host = running_host();
    if (host == NULL) {
        /* The GameCube would load the scheduler context from here and jump into the dispatcher; outside a GS
         * thread that is a game bug, so make it visible instead of corrupting anything. */
        static int warned;
        if (!warned) {
            warned = 1;
            fprintf(stderr, "[gs_thread_host] _threadSwitch called outside a GS thread; ignored\n");
        }
        return;
    }
    pc_fibre_switch(host->caller);
}
