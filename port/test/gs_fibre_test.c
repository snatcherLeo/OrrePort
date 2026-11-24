/*
 * gs_fibre_test: runs the game's real GSthread scheduler (src/game/gs_thread_exact_800F0424.c, compiled unchanged)
 * on the host fibre layer (port/src/gs_thread_host.c), with GSmem replaced by a small allocator inside the
 * GameCube memory range at 0x80000000 (port/src/pc_mem.h). Prints one line per check; exit code 0 = all passed.
 *
 *   cmake --build build-pc --target gs_fibre_test && build-pc/gs_fibre_test.exe
 */
#include "dolphin/types.h"
#include "game/gs_thread.h"

#include "pc_mem.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* --- the scheduler's globals (in the real port they come with the rest of the game's data) --- */
u8 lbl_804019F0[0x188];
u32 _activeThread, lbl_8047AC04, lbl_8047AC08;
u8 _rescan;
u32 _usesFPURegisters, _tempGPR2, _tempGPR1, _destRegStatePtr, _threadRegStatePtr, _localRegStatePtr;
GSThread* _threadList;
u16 lbl_8047AC2C;
u32 _maxThreads;
const char lbl_80271008[] = "GSthreadCreate. Warning: 'usesFPU==FALE' OK?\n";
const char lbl_80271038[] = "GSthread: Init OK, maximum of %d threads\n";

/* --- GSmem stand-in: handles to blocks bump-allocated inside MEM1 --- */
#define MAX_HANDLES 256
static void* s_handle_ptr[MAX_HANDLES];
static uintptr_t s_bump = PC_MEM1_BASE + 0x1000;

static u16 mem_alloc(u32 size) {
    int h;
    for (h = 1; h < MAX_HANDLES; h++) {
        if (s_handle_ptr[h] == NULL) {
            s_handle_ptr[h] = (void*)s_bump;
            memset(s_handle_ptr[h], 0xCD, size);
            s_bump += (size + 31) & ~31u;
            return (u16)h;
        }
    }
    return 0;
}

/* The game sizes the thread array as maxThreads * 0x24 (the GameCube sizeof(GSThread)); GSThread has pointers, so
 * it is bigger on a 64-bit PC. Allocate generously here; the real fix belongs in the game code (see gs-fibres.md). */
u16 _toolentryAlloc__FUl(u32 size) { return mem_alloc(size * 2); }
u16 GSmemAlloc(u32 size, u32 alignment) { (void)alignment; return mem_alloc(size); }
void* GSmemLock(u16 handle) { return s_handle_ptr[handle]; }
s32 fn_800E24B0(u16 handle) { (void)handle; return 1; } /* unlock: the stand-in never moves blocks */
s32 GSmemFree(u16 handle) { s_handle_ptr[handle] = NULL; return 1; }

void GSlogWrite(const void* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("  log: ");
    vprintf((const char*)fmt, ap);
    va_end(ap);
}
void GSscratchFree(void* p) { (void)p; }
u8 GSscratchIsPtr(void* p) { (void)p; return 0; } /* the PC has no locked cache: stacks never move to scratch */
void GSscratchStore(void* a, void* b, s32 n) { (void)a; (void)b; (void)n; }
void GSscratchWaitForCompletion(void) {}
void DCFlushRange(void* p, s32 n) { (void)p; (void)n; }
u32* __va_arg(void* ap, u32 type) { static u32 zero; (void)ap; (void)type; return &zero; }

extern void GSthreadExecuteAll(void);
extern void GSthreadExecuteGroup(u32 priority);
extern void GSthreadTerminate(u32 thr);
extern u32 GSthreadClose(GSThread* thr);
extern s32 GSthread(u32 maxThreads);
extern void _threadSwitch(void);

/* --- test threads --- */
static char s_trace[256];
static int s_trace_len;
static void trace(char c) {
    if (s_trace_len < (int)sizeof(s_trace) - 1) {
        s_trace[s_trace_len++] = c;
        s_trace[s_trace_len] = 0;
    }
}

/* Each runs a few frames, yielding once per frame; locals (an int and a double) must survive every yield */
static uintptr_t s_local_addr; /* address of a local inside a GS thread: must be below 4 GB (low-memory.md) */
static u32 thread_a(void) {
    volatile int n;
    volatile double f = 0.5;
    s_local_addr = (uintptr_t)&n;
    for (n = 0; n < 3; n++) {
        trace('a');
        f *= 2.0;
        _threadSwitch();
    }
    return f == 4.0 ? 0xA0 : 0xBAD;
}

static u32 thread_b(void) {
    volatile int n;
    for (n = 0; n < 5; n++) {
        trace('b');
        _threadSwitch();
    }
    return 0xB0;
}

static u32 thread_forever(void) {
    for (;;) {
        trace('f');
        _threadSwitch();
    }
}

static u32 thread_quick(void) {
    trace('q');
    return 0x51;
}

static int check(const char* what, int ok) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

int main(void) {
    int ok = 1, frame;
    GSThread *ta, *tb, *tf, *tq, *tq2;

    if (!pc_mem_reserve(PC_MEM1_RETAIL_SIZE)) {
        return 2;
    }
    ok &= check("MEM1 reserved at 0x80000000", pc_mem_check("gs_fibre_test"));

    /* The thread array must sit in MEM1 too: the scheduler stores thread pointers in u32 globals */
    ok &= check("GSthread(8)", GSthread(8) == 1);
    ok &= check("thread array inside MEM1", (uintptr_t)_threadList >= PC_MEM1_BASE);

    /* affinity decides the order within a frame: f (0) runs before b (1) before a (2) */
    ta = GSthreadCreate(2, 0, 0x2000, 1, 0, (void*)thread_a);
    tb = GSthreadCreate(1, 0, 0x2000, 1, 1, (void*)thread_b);
    tf = GSthreadCreate(0, 1, 0x2000, 1, 1, (void*)thread_forever);
    ok &= check("three threads created", ta && tb && tf);

    for (frame = 0; frame < 3; frame++) {
        GSthreadExecuteAll();
        trace('|');
    }
    ok &= check("frames 1-3 interleave by affinity (fba|fba|fba|)", strcmp(s_trace, "fba|fba|fba|") == 0);
    ok &= check("GS thread stack is below 4 GB", s_local_addr != 0 && s_local_addr < 0x100000000ull);

    /* group run: only priority-1 threads (f) */
    s_trace_len = 0;
    GSthreadExecuteGroup(1);
    ok &= check("GSthreadExecuteGroup(1) runs only f", strcmp(s_trace, "f") == 0);

    /* frame 4: a returns (non-autoStart: kept for GSthreadClose); b runs its 4th pass */
    s_trace_len = 0;
    GSthreadExecuteAll();
    ok &= check("frame 4: a returns through fn_800F0F4C", strcmp(s_trace, "fb") == 0 && ta->pad1 == 1);
    ok &= check("a's locals survived 3 yields (result 0xA0 for GSthreadClose)", GSthreadClose(ta) == 0xA0);

    /* frame 5-6: b finishes (autoStart: destroyed and unlinked by the trampoline) */
    s_trace_len = 0;
    GSthreadExecuteAll();
    GSthreadExecuteAll();
    ok &= check("frames 5-6: b finishes and is destroyed", strcmp(s_trace, "fbf") == 0 && tb->active == 0);

    /* a new thread in a freed slot gets a fresh fibre (the old one is dead) */
    tq = GSthreadCreate(3, 0, 0x1000, 1, 0, (void*)thread_quick);
    ok &= check("new thread reuses a freed slot", tq == ta || tq == tb);
    s_trace_len = 0;
    GSthreadExecuteAll();
    ok &= check("reused slot runs its own entry", strcmp(s_trace, "fq") == 0 && GSthreadClose(tq) == 0x51);

    /* terminate a thread that is suspended in the middle of its loop */
    GSthreadTerminate((u32)(uintptr_t)tf);
    s_trace_len = 0;
    GSthreadExecuteAll();
    ok &= check("terminated thread no longer runs", s_trace_len == 0 && tf->active == 0);

    /* a thread created after the termination runs normally (usesFPU = 0 here: the scheduler logs its warning) */
    tq2 = GSthreadCreate(0, 0, 0x1000, 0, 1, (void*)thread_quick);
    s_trace_len = 0;
    GSthreadExecuteAll();
    ok &= check("new thread after a termination runs", tq2 != NULL && strcmp(s_trace, "q") == 0);

    /* yielding outside any GS thread is ignored (a warning), not a crash */
    _threadSwitch();
    ok &= check("_threadSwitch outside a GS thread returns", 1);

    printf("result: %s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
