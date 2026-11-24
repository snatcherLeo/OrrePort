/*
 * pc_os_thread.c: the SDK's threads (OSCreateThread and the rest), which Aurora declares but does not define
 * (docs/port/sdk-gaps.md), on host fibres (pc_fibre.h). Design: docs/port/sdk-threads.md.
 *
 * Who uses them: the movie player (THP loader, gs_movie_801E189C.c), boot / menu sound loading (movie.c), a menu loader
 * (menu_exact_8007B350.c), GSgapp's background tasks (the idle function, gs_gapp.c), the crash-report display
 * (main_tail.c) and the GBA link (skipped).
 *
 * The GameCube's scheduler, kept here:
 *   - the highest-priority ready thread runs (0 = highest, 31 = idle; the game's main thread is 16);
 *   - a thread that wakes or is resumed with a higher priority than the running one takes over at once;
 *     equal priorities never preempt each other, they take turns only at OSYieldThread or when one blocks;
 *   - interrupts (here: the retrace and the alarms it runs, pc_vi.c) arrive whenever interrupts are enabled; the
 *     PC checks for a due retrace each time a thread re-enables them (OSRestoreInterrupts / OSEnableInterrupts
 *     dropping to "enabled", pc_os_sdk.c's hook) and at OSYieldThread. GSgapp's idle loop never returns and never
 *     blocks, but it disables and re-enables interrupts every pass, which is exactly where the GameCube would take
 *     the retrace and switch back to the main thread;
 *   - when no thread can run, the processor waits for the next interrupt (here: sleeps until the next field).
 * Everything runs on the game's one OS thread, one fibre at a time, as on the GameCube's single core.
 *
 * The game's OSThread / OSThreadQueue memory is never written: the game sizes it for the GameCube (0x318 bytes,
 * sometimes inside its own structs at a raw offset, e.g. menu context + 0x28), and Aurora's PC layout is larger. The
 * scheduler keeps its own record per thread, keyed by the OSThread address; queues are keyed by address too.
 * Thread stacks are the port's own (1 MB from low memory): the game's stacks (0x1000 bytes) are sized for PowerPC
 * frames.
 *
 * Interrupt masking per thread: on the GameCube each thread's context holds its own interrupt-enable bit. Here the
 * mask is one recursive lock (pc_os_sdk.c) whose depth belongs to the OS thread, so a fibre switch hands the
 * outgoing thread's depth back and takes the incoming thread's depth (otherwise the audio thread would stay locked
 * out while a sleeping thread "held" interrupts disabled).
 */
#include <dolphin/os.h>

#include "pc_fibre.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* pc_os_sdk.c */
extern int pc_os_intr_release_all(void);
extern void pc_os_intr_reacquire(int depth);
extern void pc_os_set_interrupt_hook(void (*hook)(void));
/* pc_vi.c */
extern u64 pc_vi_retrace_due_ns(void);
extern void pc_vi_retrace_interrupt(void);

#define PC_THREAD_MAX 32
#define PC_THREAD_STACK (1024 * 1024)
#define PC_DEFAULT_PRIORITY 16

enum { T_FREE = 0, T_READY, T_RUNNING, T_WAITING, T_MORIBUND };

typedef struct PcThread {
    OSThread* key;      /* the game's OSThread (address only) */
    PcFibre* fibre;     /* own fibre; NULL for the default (main) thread */
    PcFibre* resume_at; /* the fibre to switch to when this thread runs again (a GS thread fibre, for the main thread) */
    void* (*func)(void*);
    void* param;
    void* val;            /* exit value */
    const void* wait_key; /* the queue it sleeps on (T_WAITING) */
    s64 seq;              /* order among ready threads of one priority */
    s32 priority;
    s32 suspend;
    int state;
    int started;
    int detached;
    int intr_depth; /* interrupt-lock depth while switched out */
    int in_interrupt; /* s_in_interrupt while switched out: a handler that blocked keeps it to itself */
} PcThread;

static PcThread s_threads[PC_THREAD_MAX];
static PcThread* s_current;
static OSThread s_default_thread; /* OSGetCurrentThread() of the main thread; game code never writes it */
static OSThread s_idle_thread;
static void (*s_idle_func)(void*);
static void* s_idle_param;
static s64 s_seq_back, s_seq_front;
static s32 s_reschedule; /* OSDisableScheduler count */
/* Inside an interrupt handler (posted interrupts, the retrace). Per thread, swapped in switch_to: on the GameCube a
 * handler that ends up sleeping (the retrace handler runs GS threads while the game is blocked, and those can wait)
 * leaves its state with the sleeping thread, and the next thread runs with interrupts taken normally. As one global,
 * a blocked handler would stop every later interrupt (disc completions, retraces) for all threads. (B, 2026-10-16) */
static int s_in_interrupt;
static int s_need_preempt;
static PcFibre* s_zombie; /* fibre of a thread that exited; freed by the next thread to run */

static void interrupt_point(void);

/* Interrupts posted from other OS threads (pc_os_post_interrupt): Aurora's workers finish GPU and disc work there,
 * and the GameCube would raise an interrupt; the handler runs here, on the game thread, at the next point where
 * interrupts are enabled */
#define PC_POSTED_MAX 64
static SDL_SpinLock s_posted_lock;
static void (*s_posted[PC_POSTED_MAX])(void);
static int s_posted_count;
static SDL_AtomicInt s_posted_pending;

/* Posted calls with arguments (DVD completion callbacks: callback, result, command block), run in the order posted */
#define PC_CALLS_MAX 256
typedef struct PcPostedCall {
    void (*fn)(s32 result, void* block);
    s32 result;
    void* block;
} PcPostedCall;
static PcPostedCall s_calls[PC_CALLS_MAX];
static int s_calls_head, s_calls_count;

static PcThread* find(const OSThread* key) {
    int i;
    for (i = 0; i < PC_THREAD_MAX; i++) {
        if (s_threads[i].state != T_FREE && s_threads[i].key == key) {
            return &s_threads[i];
        }
    }
    return NULL;
}

/* The game's OS thread: the only one that may switch fibres. MusyX's audio thread runs game callbacks as its
 * "interrupt" (the THP stream callback hands decoded buffers back with OSSendMessage, which wakes the THP audio
 * decoder); a wake there must not switch threads on the audio thread. On the GameCube the woken thread runs when the
 * interrupt ends; here the reschedule is left pending and taken at the game thread's next interrupt point.
 * Before (B, 2026-10-16): the audio thread switched fibres itself, the fibre layer's "current" then belonged to the
 * wrong OS thread, the next switch saved one thread's stack under another's name, and the THP audio decoder resumed
 * past its own OSSuspendThread and read past the movie (the tpc.thp crash, about 1 boot in 3). */
static SDL_ThreadID s_game_tid;
static SDL_AtomicInt s_foreign_preempt;

static int on_game_thread(void) { return SDL_GetCurrentThreadID() == s_game_tid; }
int pc_os_on_game_thread(void) { return s_game_tid != 0 && on_game_thread(); }

/* The replay clock (pc_vi.c, pc_vi_virtual_clock): no timer, so the scheduler says when the game would be waiting
 * for the retrace. On the GameCube, once every thread above the idle loop is blocked (the main thread in
 * VIWaitForRetrace, the loaders waiting for the disc), the CPU runs the idle loop (GSgapp's background tasks: the
 * file loader's jobs) until the retrace. Here the idle loop gets IDLE_POINTS interrupt points, then the retrace comes;
 * with nothing runnable at all it comes at once. A thread that spins without blocking (waiting on a flag) still lets
 * the retrace come after SPIN_POINTS interrupt points, as time would pass on the console. Both counts are the game's
 * own progress, so two runs with the same inputs take the same retraces at the same points. */
enum { IDLE_POINTS = 256, SPIN_POINTS = 200000 };
extern int pc_vi_virtual_clock(void);
extern void pc_vi_request_retrace_for(int cause); /* 1 idle loop, 2 nothing runnable, 3 spin */
extern int pc_vi_retrace_due_now(void);
static u32 s_field_points, s_idle_points;
static void (*s_deliver_hooks[4])(void);

/* Removed 2026-11-01 (snatcherLeo's OK; the project notes): ORRE_DVD_SUBFIELD (+ _PARTS), disc completions
 * handed over part-way through a field by the share of interrupt points used, and pc_os_field_fraction, which the
 * audio catch-up used too (P01's and P02's old resyncs; no final replay uses them) */
/* pc_dvd.c (disc completions due by now), pc_gx_hooks.c (draw-done issued since the last point): delivered at an
 * interrupt point (replay clock) */
void pc_os_set_deliver_hook(void (*hook)(void)) {
    for (int i = 0; i < 4; i++) {
        if (s_deliver_hooks[i] == NULL || s_deliver_hooks[i] == hook) {
            s_deliver_hooks[i] = hook;
            return;
        }
    }
}

static void init(void) {
    if (s_current != NULL) {
        return;
    }
    s_game_tid = SDL_GetCurrentThreadID();
    pc_fibre_init_main();
    s_current = &s_threads[0];
    memset(s_current, 0, sizeof(*s_current));
    s_current->key = &s_default_thread;
    s_current->priority = PC_DEFAULT_PRIORITY;
    s_current->state = T_RUNNING;
    s_current->started = 1;
    pc_os_set_interrupt_hook(interrupt_point);
}

static int runnable(const PcThread* t) {
    return (t->state == T_READY || t->state == T_RUNNING) && t->suspend <= 0;
}

static void make_ready(PcThread* t, int front) {
    t->state = T_READY;
    t->seq = front ? --s_seq_front : ++s_seq_back;
}

/* The thread that should run now: highest priority, then longest waiting */
static PcThread* pick(void) {
    PcThread* best = NULL;
    int i;
    for (i = 0; i < PC_THREAD_MAX; i++) {
        PcThread* t = &s_threads[i];
        if (!runnable(t)) {
            continue;
        }
        if (best == NULL || t->priority < best->priority ||
            (t->priority == best->priority && t->seq < best->seq)) {
            best = t;
        }
    }
    return best;
}

static void free_zombie(void) {
    if (s_zombie != NULL && s_zombie != pc_fibre_current()) {
        pc_fibre_destroy(s_zombie);
        s_zombie = NULL;
    }
}

static void thread_entry(void* arg);

static void switch_to(PcThread* next) {
    PcThread* prev = s_current;
    PcFibre* target;
    if (next == prev) {
        prev->state = T_RUNNING;
        return;
    }
    if (prev->state == T_RUNNING) {
        prev->state = T_READY;
    }
    if (s_in_interrupt) {
        static int warned;
        if (!warned) {
            warned = 1;
            fprintf(stderr, "[pc_os_thread] a thread blocked inside an interrupt handler (thread %p); its interrupt "
                            "state stays with it\n", (void*)prev->key);
        }
    }
    prev->in_interrupt = s_in_interrupt;
    s_in_interrupt = next->started ? next->in_interrupt : 0;
    prev->resume_at = pc_fibre_current();
    prev->intr_depth = pc_os_intr_release_all();
    if (!next->started) {
        next->started = 1;
        next->fibre = pc_fibre_create(PC_THREAD_STACK, thread_entry, next);
        if (next->fibre == NULL) {
            fprintf(stderr, "[pc_os_thread] cannot create a fibre for thread %p\n", (void*)next->key);
            next->state = T_MORIBUND;
            s_in_interrupt = prev->in_interrupt;
            pc_os_intr_reacquire(prev->intr_depth);
            prev->state = T_RUNNING;
            return;
        }
        next->resume_at = next->fibre;
    }
    target = next->resume_at;
    s_current = next;
    next->state = T_RUNNING;
    pc_fibre_switch(target);
    /* back in prev: someone switched to it */
    free_zombie();
    pc_os_intr_reacquire(prev->intr_depth);
}

/* The current thread can no longer run (sleeping, suspended, finished): run another, or wait for an interrupt */
static void block(void) {
    if (!on_game_thread()) {
        /* a blocking wait from another OS thread (an audio callback): it cannot sleep the game's threads; give up */
        static SDL_AtomicInt warned;
        if (SDL_CompareAndSwapAtomicInt(&warned, 0, 1)) {
            fprintf(stderr, "[pc_os_thread] a blocking wait from another OS thread was refused\n");
        }
        return;
    }
    {
        /* ORRE_VI_MIDFRAME (pc_vi.c): the frame thread blocks outside its retrace wait (a sync read, a message) */
        extern volatile int pc_vi_in_retrace_wait;
        extern void* volatile pc_vi_frame_thread;
        extern int pc_vi_midframe_log_active(void);
        extern u32 VIGetRetraceCount(void);
        if (!pc_vi_in_retrace_wait && s_current->key == pc_vi_frame_thread && pc_vi_midframe_log_active()) {
            extern void pc_crash_print_stack(const char* reason);
            fprintf(stderr, "[vi] the frame thread blocks mid-frame (retrace %u)\n", VIGetRetraceCount());
            pc_crash_print_stack("ORRE_VI_MIDFRAME: the frame thread blocks");
        }
    }
    for (;;) {
        PcThread* next = pick();
        if (next != NULL) {
            switch_to(next);
            if (runnable(s_current)) {
                return;
            }
            continue;
        }
        /* nothing can run: the processor idles until the next interrupt */
        if (pc_vi_virtual_clock()) { /* the replay clock: the retrace is the next thing to happen */
            pc_vi_request_retrace_for(2);
            interrupt_point();
            continue;
        }
        {
            u64 due = pc_vi_retrace_due_ns(), now = SDL_GetTicksNS();
            if (due > now) {
                int depth = pc_os_intr_release_all(); /* an idle processor holds no interrupt mask: audio runs */
                /* sleep until the field, in slices of at most 1 ms so a posted interrupt is not held back long */
                while (now < due && !SDL_GetAtomicInt(&s_posted_pending)) {
                    SDL_DelayPrecise(due - now < 1000000ull ? due - now : 1000000ull);
                    now = SDL_GetTicksNS();
                }
                pc_os_intr_reacquire(depth);
            }
            interrupt_point();
        }
    }
}

/* A thread that woke or was resumed may outrank the running one */
static void check_preempt(void) {
    PcThread* best;
    if (!on_game_thread()) {
        static SDL_AtomicInt warned;
        if (SDL_CompareAndSwapAtomicInt(&warned, 0, 1)) {
            fprintf(stderr, "[pc_os_thread] a thread was woken from another OS thread (the audio mixer's callbacks); "
                            "the switch waits for the game thread's next interrupt point\n");
        }
        SDL_SetAtomicInt(&s_foreign_preempt, 1);
        SDL_SetAtomicInt(&s_posted_pending, 1); /* so the game thread's next interrupt point looks */
        return;
    }
    if (s_in_interrupt) {
        s_need_preempt = 1;
        return;
    }
    s_need_preempt = 0;
    if (s_reschedule > 0) {
        return;
    }
    best = pick();
    if (best != NULL && best != s_current && (!runnable(s_current) || best->priority < s_current->priority)) {
        if (s_current->state == T_RUNNING) {
            make_ready(s_current, 1); /* preempted: first in line at its priority, as on the GameCube */
        }
        switch_to(best);
    }
}

/* Any thread: queue an interrupt handler for the game thread (the same handler twice before it ran = once, as a
 * hardware interrupt line stays raised until handled) */
void pc_os_post_interrupt(void (*handler)(void)) {
    int i, found = 0;
    SDL_LockSpinlock(&s_posted_lock);
    for (i = 0; i < s_posted_count; i++) {
        found |= s_posted[i] == handler;
    }
    if (!found && s_posted_count < PC_POSTED_MAX) {
        s_posted[s_posted_count++] = handler;
    } else if (!found) {
        fprintf(stderr, "[pc_os_thread] more than %d posted interrupts pending\n", PC_POSTED_MAX);
    }
    SDL_UnlockSpinlock(&s_posted_lock);
    SDL_SetAtomicInt(&s_posted_pending, 1);
}

/* Any thread: queue a call fn(result, block) for the game thread (a DVD command's completion: each one runs once,
 * in order) */
void pc_os_post_call(void (*fn)(s32 result, void* block), s32 result, void* block) {
    SDL_LockSpinlock(&s_posted_lock);
    if (s_calls_count < PC_CALLS_MAX) {
        PcPostedCall* c = &s_calls[(s_calls_head + s_calls_count++) % PC_CALLS_MAX];
        c->fn = fn;
        c->result = result;
        c->block = block;
    } else {
        fprintf(stderr, "[pc_os_thread] more than %d posted calls pending\n", PC_CALLS_MAX);
    }
    SDL_UnlockSpinlock(&s_posted_lock);
    SDL_SetAtomicInt(&s_posted_pending, 1);
}

static void run_posted(void) {
    void (*handlers[PC_POSTED_MAX])(void);
    PcPostedCall calls[PC_CALLS_MAX];
    int i, n, ncalls;
    if (!SDL_GetAtomicInt(&s_posted_pending)) {
        return;
    }
    SDL_LockSpinlock(&s_posted_lock);
    n = s_posted_count;
    memcpy(handlers, s_posted, sizeof(handlers[0]) * n);
    s_posted_count = 0;
    ncalls = s_calls_count;
    for (i = 0; i < ncalls; i++) {
        calls[i] = s_calls[(s_calls_head + i) % PC_CALLS_MAX];
    }
    s_calls_head = (s_calls_head + ncalls) % PC_CALLS_MAX;
    s_calls_count = 0;
    SDL_SetAtomicInt(&s_posted_pending, 0);
    SDL_UnlockSpinlock(&s_posted_lock);
    for (i = 0; i < n; i++) {
        handlers[i]();
    }
    for (i = 0; i < ncalls; i++) {
        calls[i].fn(calls[i].result, calls[i].block);
    }
}

/* Called with interrupts enabled (pc_os_sdk.c's hook, OSYieldThread, idle waits): deliver posted interrupts and a
 * due retrace */
static void interrupt_point(void) {
    BOOL enabled;
    if (s_in_interrupt || s_current == NULL) {
        return;
    }
    if (SDL_GetAtomicInt(&s_posted_pending)) {
        s_in_interrupt = 1;
        enabled = OSDisableInterrupts();
        run_posted();
        OSRestoreInterrupts(enabled);
        s_in_interrupt = 0;
    }
    if (pc_vi_virtual_clock()) {
        if (s_current->priority >= OS_PRIORITY_IDLE && ++s_idle_points >= IDLE_POINTS) {
            pc_vi_request_retrace_for(1);
        }
        if (++s_field_points >= SPIN_POINTS) {
            pc_vi_request_retrace_for(3);
        }
    }
    if (pc_vi_retrace_due_now()) {
        s_in_interrupt = 1;
        enabled = OSDisableInterrupts();
        pc_vi_retrace_interrupt(); /* callbacks, alarms, wakes VIWaitForRetrace's sleepers */
        OSRestoreInterrupts(enabled);
        s_in_interrupt = 0;
        s_field_points = s_idle_points = 0;
    }
    if (s_deliver_hooks[0] != NULL && pc_vi_virtual_clock()) {
        s_in_interrupt = 1;
        enabled = OSDisableInterrupts();
        for (int i = 0; i < 4 && s_deliver_hooks[i] != NULL; i++) {
            s_deliver_hooks[i](); /* disc completions due by this retrace, draw-done: at this fixed point */
        }
        OSRestoreInterrupts(enabled);
        s_in_interrupt = 0;
    }
    if (SDL_GetAtomicInt(&s_foreign_preempt)) {
        SDL_SetAtomicInt(&s_foreign_preempt, 0);
        s_need_preempt = 1;
    }
    if (s_need_preempt) {
        check_preempt();
    }
}

static void wake_all(const void* key) {
    int i, woke = 0;
    for (i = 0; i < PC_THREAD_MAX; i++) {
        PcThread* t = &s_threads[i];
        if (t->state == T_WAITING && t->wait_key == key) {
            t->wait_key = NULL;
            make_ready(t, 0);
            woke = 1;
        }
    }
    if (woke) {
        check_preempt();
    }
}

static void finish(PcThread* t, void* val) {
    t->val = val;
    t->state = T_MORIBUND;
    wake_all(&t->key); /* joiners sleep on the record's key field */
    if (t->detached) {
        if (t == s_current) {
            s_zombie = t->fibre; /* the next thread to run frees our stack */
        } else if (t->fibre != NULL) {
            pc_fibre_destroy(t->fibre);
        }
        t->fibre = NULL;
        t->state = T_FREE;
    }
}

static void thread_entry(void* arg) {
    PcThread* t = (PcThread*)arg;
    void* val;
    free_zombie();
    val = t->func(t->param);
    OSExitThread(val);
}

/* ---- the SDK functions ---- */

void OSInitThreadQueue(OSThreadQueue* queue) { (void)queue; /* waiters are kept by queue address */ }

BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize,
                    OSPriority priority, u16 attr) {
    PcThread* t;
    int i;
    (void)stack;
    (void)stackSize;
    if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
        return FALSE;
    }
    init();
    t = find(thread);
    if (t != NULL && t->state != T_MORIBUND) {
        fprintf(stderr, "[pc_os_thread] OSCreateThread on live thread %p\n", (void*)thread);
        return FALSE;
    }
    if (t == NULL) {
        for (i = 1; i < PC_THREAD_MAX && s_threads[i].state != T_FREE; i++) {
        }
        if (i == PC_THREAD_MAX) {
            fprintf(stderr, "[pc_os_thread] more than %d threads\n", PC_THREAD_MAX);
            return FALSE;
        }
        t = &s_threads[i];
    } else if (t->fibre != NULL) {
        pc_fibre_destroy(t->fibre); /* an undetached thread that finished and was never joined */
    }
    memset(t, 0, sizeof(*t));
    t->key = thread;
    t->func = func;
    t->param = param;
    t->priority = priority;
    t->detached = (attr & OS_THREAD_ATTR_DETACH) != 0;
    t->suspend = 1; /* created suspended */
    make_ready(t, 0);
    return TRUE;
}

s32 OSResumeThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();
    PcThread* t = find(thread);
    s32 old = 0;
    if (t != NULL) {
        old = t->suspend--;
        if (t->suspend == 0 && t->state == T_READY) {
            t->seq = ++s_seq_back;
        }
        if (t->suspend < 0) {
            t->suspend = 0;
        }
        if (old > 0 && t->suspend == 0) {
            check_preempt();
        }
    }
    OSRestoreInterrupts(enabled);
    return old;
}

s32 OSSuspendThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();
    PcThread* t = find(thread);
    s32 old = 0;
    if (t != NULL) {
        old = t->suspend++;
        if (old == 0 && t == s_current) {
            make_ready(t, 0);
            block();
        }
    }
    OSRestoreInterrupts(enabled);
    return old;
}

void OSSleepThread(OSThreadQueue* queue) {
    BOOL enabled = OSDisableInterrupts();
    init();
    s_current->state = T_WAITING;
    s_current->wait_key = queue;
    block();
    OSRestoreInterrupts(enabled);
}

void OSWakeupThread(OSThreadQueue* queue) {
    BOOL enabled = OSDisableInterrupts();
    wake_all(queue);
    OSRestoreInterrupts(enabled);
}

/* Wait until a posted interrupt has arrived and run (GXWaitDrawDone: HSD waits for its draw-done handler), at most
 * timeout_ms; the retrace still runs meanwhile. Returns whether one ran. */
int pc_os_wait_posted_interrupt(u32 timeout_ms) {
    u64 end = SDL_GetTicksNS() + (u64)timeout_ms * 1000000ull;
    int depth;
    init();
    for (;;) {
        if (SDL_GetAtomicInt(&s_posted_pending)) {
            depth = pc_os_intr_release_all(); /* deliver it even if the caller has interrupts disabled */
            interrupt_point();
            pc_os_intr_reacquire(depth);
            return 1;
        }
        if (SDL_GetTicksNS() >= end) {
            return 0;
        }
        depth = pc_os_intr_release_all();
        interrupt_point();
        SDL_DelayPrecise(50000); /* 50 us: the GPU worker is usually done within a frame */
        pc_os_intr_reacquire(depth);
    }
}

/* For pc_vi.c: VIWaitForRetrace's sleep and the retrace interrupt's wakeup */
void pc_os_thread_sleep_on(const void* key) {
    BOOL enabled = OSDisableInterrupts();
    init();
    s_current->state = T_WAITING;
    s_current->wait_key = key;
    block();
    OSRestoreInterrupts(enabled);
}

void pc_os_thread_wakeup(const void* key) { wake_all(key); }

void OSYieldThread(void) {
    PcThread* next;
    BOOL enabled;
    init();
    interrupt_point();
    enabled = OSDisableInterrupts();
    make_ready(s_current, 0); /* to the back of its priority */
    next = pick();
    switch_to(next != NULL ? next : s_current);
    OSRestoreInterrupts(enabled);
}

OSThread* OSGetCurrentThread(void) {
    init();
    return s_current->key;
}

void OSExitThread(void* val) {
    init();
    (void)OSDisableInterrupts();
    finish(s_current, val);
    block(); /* never returns: the record is finished, so no one switches back */
    fprintf(stderr, "[pc_os_thread] a finished thread ran again\n");
    for (;;) {
        block();
    }
}

void OSCancelThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();
    PcThread* t = find(thread);
    if (t != NULL && t->state != T_MORIBUND) {
        if (t == s_current) {
            OSExitThread(NULL);
        }
        finish(t, NULL);
    }
    OSRestoreInterrupts(enabled);
}

BOOL OSJoinThread(OSThread* thread, void** val) {
    BOOL enabled = OSDisableInterrupts();
    PcThread* t = find(thread);
    if (t == NULL || t->detached || t == s_current) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    while (t->state != T_MORIBUND) {
        s_current->state = T_WAITING;
        s_current->wait_key = &t->key;
        block();
    }
    if (val != NULL) {
        *val = t->val;
    }
    if (t->fibre != NULL) {
        pc_fibre_destroy(t->fibre);
    }
    t->fibre = NULL;
    t->state = T_FREE;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

void OSDetachThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();
    PcThread* t = find(thread);
    if (t != NULL) {
        t->detached = 1;
        if (t->state == T_MORIBUND) {
            finish(t, t->val);
        }
    }
    OSRestoreInterrupts(enabled);
}

BOOL OSIsThreadSuspended(OSThread* thread) {
    PcThread* t = find(thread);
    return t != NULL && t->suspend > 0;
}

BOOL OSIsThreadTerminated(OSThread* thread) {
    PcThread* t = find(thread);
    return t == NULL || t->state == T_MORIBUND;
}

BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    BOOL enabled;
    PcThread* t;
    if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
        return FALSE;
    }
    enabled = OSDisableInterrupts();
    init();
    t = find(thread);
    if (t != NULL) {
        t->priority = priority;
        check_preempt();
    }
    OSRestoreInterrupts(enabled);
    return t != NULL;
}

s32 OSGetThreadPriority(OSThread* thread) {
    PcThread* t;
    init();
    t = find(thread);
    return t != NULL ? t->priority : -1;
}

s32 OSDisableScheduler(void) {
    BOOL enabled = OSDisableInterrupts();
    s32 old = s_reschedule++;
    OSRestoreInterrupts(enabled);
    return old;
}

s32 OSEnableScheduler(void) {
    BOOL enabled = OSDisableInterrupts();
    s32 old = s_reschedule--;
    OSRestoreInterrupts(enabled);
    return old;
}

static void* idle_entry(void* arg) {
    (void)arg;
    for (;;) {
        s_idle_func(s_idle_param); /* GSgapp's never returns; interrupt points inside it give the frame back */
        interrupt_point();
    }
    return NULL;
}

OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, void* stack, u32 stackSize) {
    init();
    if (idleFunction == NULL) {
        if (find(&s_idle_thread) != NULL) {
            OSCancelThread(&s_idle_thread);
        }
        s_idle_func = NULL;
        return NULL;
    }
    if (s_idle_func != NULL) {
        return NULL; /* the SDK installs only one */
    }
    s_idle_func = idleFunction;
    s_idle_param = param;
    OSCreateThread(&s_idle_thread, idle_entry, NULL, stack, stackSize, OS_PRIORITY_IDLE, OS_THREAD_ATTR_DETACH);
    OSResumeThread(&s_idle_thread);
    return &s_idle_thread;
}

OSThread* OSGetIdleFunction(void) { return s_idle_func != NULL ? &s_idle_thread : NULL; }

/* For tests: how many threads are not free */
int pc_os_thread_count(void) {
    int i, n = 0;
    for (i = 0; i < PC_THREAD_MAX; i++) {
        n += s_threads[i].state != T_FREE;
    }
    return n;
}
