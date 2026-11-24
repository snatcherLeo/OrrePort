/*
 * pc_thread_test: the SDK threads on fibres (port/src/pc_os_thread.c, docs/port/sdk-threads.md), without a window.
 * Exit code 0 = all passed. The thread objects are GameCube-sized (0x318 bytes) and filled with a pattern: the
 * scheduler must never write them (the game's own are GameCube-sized too).
 */
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <string.h>

typedef int BOOL;
typedef struct {
    unsigned char bytes[0x318];
} OSThread; /* the GameCube size */
typedef struct {
    unsigned char bytes[8];
} OSThreadQueue;

BOOL OSDisableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);
int pc_os_interrupt_depth(void);
void pc_os_audio_enter(void);
void pc_os_audio_leave(void);
BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, unsigned stackSize, int priority,
                    unsigned short attr);
int OSResumeThread(OSThread* thread);
int OSSuspendThread(OSThread* thread);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);
void OSYieldThread(void);
void OSExitThread(void* val);
BOOL OSJoinThread(OSThread* thread, void** val);
BOOL OSIsThreadTerminated(OSThread* thread);
OSThread* OSGetCurrentThread(void);
OSThread* OSSetIdleFunction(void (*fn)(void*), void* param, void* stack, unsigned stackSize);
void VIWaitForRetrace(void);
unsigned VIGetRetraceCount(void);
int pc_os_thread_count(void);
void pc_os_post_interrupt(void (*handler)(void));

static void note(char c);

/* GameCube-sized sync objects (OSMutex 0x18, OSCond 8, OSMessageQueue 0x20) */
typedef struct { unsigned char bytes[0x18]; } OSMutex;
typedef struct { unsigned char bytes[8]; } OSCond;
typedef struct { unsigned char bytes[0x20]; } OSMessageQueue;
void OSInitMutex(OSMutex* m);
void OSLockMutex(OSMutex* m);
void OSUnlockMutex(OSMutex* m);
BOOL OSTryLockMutex(OSMutex* m);
void OSInitCond(OSCond* c);
void OSWaitCond(OSCond* c, OSMutex* m);
void OSSignalCond(OSCond* c);
void OSInitMessageQueue(OSMessageQueue* q, void** msgs, int count);
BOOL OSSendMessage(OSMessageQueue* q, void* msg, int flags);
BOOL OSReceiveMessage(OSMessageQueue* q, void** msg, int flags);
BOOL OSJamMessage(OSMessageQueue* q, void* msg, int flags);

static OSMutex s_mutex;
static OSCond s_cond;
static OSMessageQueue s_mq;
static unsigned s_mq_store[4]; /* GameCube-sized message array: 4 bytes each */
static int s_ready;
static void* s_got;

static void* mutex_waiter(void* arg) {
    (void)arg;
    note('w');
    OSLockMutex(&s_mutex); /* held by main: sleeps */
    note('L');
    OSUnlockMutex(&s_mutex);
    return NULL;
}

static void* cond_waiter(void* arg) {
    (void)arg;
    OSLockMutex(&s_mutex);
    while (!s_ready) {
        note('c');
        OSWaitCond(&s_cond, &s_mutex);
    }
    note(OSTryLockMutex(&s_mutex) ? 'R' : 'x'); /* holds it again after the wait (recursive try succeeds) */
    OSUnlockMutex(&s_mutex);
    OSUnlockMutex(&s_mutex);
    return NULL;
}

static void* receiver(void* arg) {
    (void)arg;
    note('r');
    OSReceiveMessage(&s_mq, &s_got, 1); /* empty: sleeps */
    note('G');
    return NULL;
}

static SDL_ThreadID s_main_tid;
static int s_posted_runs, s_posted_off_thread;
static void posted_handler(void) {
    ++s_posted_runs;
    s_posted_off_thread += SDL_GetCurrentThreadID() != s_main_tid;
}
void pc_os_post_call(void (*fn)(int result, void* block), int result, void* block);
static char s_calls_log[16];
static int s_ncalls, s_calls_off_thread;
static void posted_call(int result, void* block) {
    if (s_ncalls < 15) {
        s_calls_log[s_ncalls++] = (char)('0' + result);
        s_calls_log[s_ncalls] = 0;
    }
    s_calls_off_thread += SDL_GetCurrentThreadID() != s_main_tid || block != (void*)&s_calls_log;
}
static int SDLCALL call_poster_thread(void* arg) {
    int i;
    (void)arg;
    for (i = 1; i <= 3; i++) {
        pc_os_post_call(posted_call, i, &s_calls_log); /* like three DVD reads finishing */
    }
    return 0;
}

/* A handler that sleeps (the retrace handler runs GS threads that may wait): the interrupt state stays with the
 * sleeping thread, so other threads still take interrupts meanwhile */
static OSThreadQueue s_handler_q;
static OSThread s_waker;
static int s_sleeper_runs, s_sleeper_back, s_seen_while_asleep;
static void sleeping_handler(void) {
    ++s_sleeper_runs;
    OSSleepThread(&s_handler_q);
    ++s_sleeper_back;
}
static int SDLCALL plain_poster_thread(void* arg) {
    (void)arg;
    pc_os_post_interrupt(posted_handler);
    return 0;
}
static void* waker_main(void* arg) {
    SDL_Thread* poster;
    int before = s_posted_runs;
    (void)arg;
    poster = SDL_CreateThread(plain_poster_thread, "poster2", NULL);
    SDL_WaitThread(poster, NULL);
    VIWaitForRetrace(); /* an interrupt point: the posted handler must run here */
    s_seen_while_asleep = s_posted_runs > before && s_sleeper_back == 0;
    OSWakeupThread(&s_handler_q);
    return NULL;
}
static int SDLCALL sleeper_poster_thread(void* arg) {
    (void)arg;
    pc_os_post_interrupt(sleeping_handler);
    return 0;
}

/* A message sent from another OS thread (MusyX's audio thread runs the THP stream callback, which hands buffers back
 * with OSSendMessage) wakes a higher-priority SDK thread: that thread must run on the game thread, not the sender's */
static OSMessageQueue s_fq;
static void* s_fq_msgs[2];
static OSThread s_receiver;
static volatile int s_recv_done, s_recv_off_thread;
static void* receiver_main(void* arg) {
    void* msg;
    (void)arg;
    OSReceiveMessage(&s_fq, &msg, 1);
    s_recv_off_thread = SDL_GetCurrentThreadID() != s_main_tid;
    s_recv_done = 1;
    return NULL;
}
static int SDLCALL foreign_sender_thread(void* arg) {
    (void)arg;
    pc_os_audio_enter(); /* as the mixer does around its callbacks */
    OSSendMessage(&s_fq, (void*)1, 0);
    pc_os_audio_leave();
    return 0;
}

static int SDLCALL poster_thread(void* arg) {
    (void)arg;
    pc_os_post_interrupt(posted_handler);
    pc_os_post_interrupt(posted_handler); /* raised twice before handled: runs once */
    return 0;
}

static int check(const char* what, int ok) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

static char s_log[64];
static int s_nlog;
static void note(char c) {
    if (s_nlog < 63) {
        s_log[s_nlog++] = c;
        s_log[s_nlog] = 0;
    }
}

static OSThread s_t[4];
static OSThreadQueue s_q;
static unsigned char s_stack[0x1000];

static void* equal_prio(void* arg) {
    note('e');
    return arg;
}

static void* high_prio(void* arg) {
    (void)arg;
    note('h');
    OSSleepThread(&s_q); /* blocks: main continues */
    note('H');           /* woken by main: runs at once, before OSWakeupThread returns */
    return (void*)0x1234;
}

static void* yielder(void* arg) {
    int i;
    for (i = 0; i < 3; i++) {
        note(*(char*)arg);
        OSYieldThread();
    }
    return NULL;
}

static void* self_suspend(void* arg) {
    (void)arg;
    note('s');
    OSSuspendThread(OSGetCurrentThread());
    note('S');
    return NULL;
}

static SDL_AtomicInt s_idle_passes;
static int s_idle_bad_depth;
static void idle_loop(void* arg) {
    (void)arg;
    for (;;) { /* like GSgapp's: never returns, never blocks, but disables / enables interrupts every pass */
        BOOL level = OSDisableInterrupts();
        SDL_AddAtomicInt(&s_idle_passes, 1);
        OSRestoreInterrupts(level);
        if (pc_os_interrupt_depth() != 0) {
            s_idle_bad_depth = 1;
        }
    }
}

static SDL_AtomicInt s_audio_ran;
static int SDLCALL audio_thread(void* arg) {
    (void)arg;
    pc_os_audio_enter();
    SDL_SetAtomicInt(&s_audio_ran, 1);
    pc_os_audio_leave();
    return 0;
}

int main(void) {
    int ok = 1, i, untouched = 1;
    void* val = NULL;
    unsigned before;
    Uint64 t0, t1;
    SDL_Thread* audio;

    for (i = 0; i < 4; i++) {
        memset(&s_t[i], 0xA5, sizeof(s_t[i]));
    }
    memset(&s_q, 0xA5, sizeof(s_q));

    /* equal priority: runs only when main blocks (here: the retrace wait) */
    OSCreateThread(&s_t[0], equal_prio, (void*)7, s_stack + sizeof(s_stack), sizeof(s_stack), 16, 0);
    OSResumeThread(&s_t[0]);
    note('m');
    VIWaitForRetrace();
    note('M');
    ok &= check("an equal-priority thread waits until main blocks, then runs", strcmp(s_log, "meM") == 0);
    ok &= check("OSJoinThread returns the thread's result", OSJoinThread(&s_t[0], &val) && val == (void*)7);

    /* higher priority: runs inside OSResumeThread; sleeping and being woken */
    s_nlog = 0;
    OSCreateThread(&s_t[1], high_prio, NULL, s_stack + sizeof(s_stack), sizeof(s_stack), 8, 0);
    note('1');
    OSResumeThread(&s_t[1]);
    note('2');
    OSWakeupThread(&s_q);
    note('3');
    ok &= check("a higher-priority thread runs at once; woken, it preempts the waker", strcmp(s_log, "1h2H3") == 0);
    ok &= check("... and its result is joined", OSJoinThread(&s_t[1], &val) && val == (void*)0x1234);

    /* equal-priority threads take turns at OSYieldThread */
    s_nlog = 0;
    OSCreateThread(&s_t[2], yielder, "a", NULL, 0, 16, 1);
    OSCreateThread(&s_t[3], yielder, "b", NULL, 0, 16, 1);
    OSResumeThread(&s_t[2]);
    OSResumeThread(&s_t[3]);
    while (!OSIsThreadTerminated(&s_t[2]) || !OSIsThreadTerminated(&s_t[3])) {
        OSYieldThread();
    }
    ok &= check("OSYieldThread alternates equal-priority threads", strcmp(s_log, "ababab") == 0);

    /* suspending itself; resumed by main */
    s_nlog = 0;
    OSCreateThread(&s_t[0], self_suspend, NULL, NULL, 0, 8, 0);
    OSResumeThread(&s_t[0]);
    note('m');
    OSResumeThread(&s_t[0]);
    ok &= check("a thread that suspends itself continues when resumed", strcmp(s_log, "smS") == 0);
    OSJoinThread(&s_t[0], NULL);

    /* the idle function: a never-ending loop still gives the frame back, at the field rate */
    OSSetIdleFunction(idle_loop, NULL, NULL, 0);
    VIWaitForRetrace();
    before = VIGetRetraceCount();
    t0 = SDL_GetTicksNS();
    for (i = 0; i < 30; i++) {
        VIWaitForRetrace();
    }
    t1 = SDL_GetTicksNS();
    printf("  30 fields in %.1f ms (500.5 expected), idle passes %d\n", (t1 - t0) / 1e6,
           SDL_GetAtomicInt(&s_idle_passes));
    ok &= check("main gets every retrace back from a never-ending idle loop", VIGetRetraceCount() == before + 30);
    ok &= check("... at the field rate", (t1 - t0) > 490000000ull && (t1 - t0) < 515000000ull);
    ok &= check("the idle loop ran meanwhile, with its own interrupt state",
                SDL_GetAtomicInt(&s_idle_passes) > 30 && !s_idle_bad_depth);

    /* main sleeping in VIWaitForRetrace with interrupts disabled must not lock the audio thread out */
    {
        BOOL level = OSDisableInterrupts();
        audio = SDL_CreateThread(audio_thread, "audio", NULL);
        for (i = 0; i < 6 && !SDL_GetAtomicInt(&s_audio_ran); i++) {
            VIWaitForRetrace();
        }
        OSRestoreInterrupts(level);
        SDL_WaitThread(audio, NULL);
        ok &= check("the audio thread runs while main waits for a retrace with interrupts disabled",
                    i < 6 && SDL_GetAtomicInt(&s_audio_ran));
    }

    /* mutexes, condition variables, message queues */
    memset(&s_mutex, 0xA5, sizeof(s_mutex));
    memset(&s_cond, 0xA5, sizeof(s_cond));
    memset(&s_mq, 0xA5, sizeof(s_mq));
    memset(s_mq_store, 0xA5, sizeof(s_mq_store));
    OSInitMutex(&s_mutex);
    OSInitCond(&s_cond);
    OSInitMessageQueue(&s_mq, (void**)s_mq_store, 2);

    s_nlog = 0;
    OSLockMutex(&s_mutex);
    OSLockMutex(&s_mutex); /* recursive */
    OSCreateThread(&s_t[0], mutex_waiter, NULL, NULL, 0, 8, 0);
    OSResumeThread(&s_t[0]);
    note('1');
    OSUnlockMutex(&s_mutex);
    note('2');
    OSUnlockMutex(&s_mutex); /* now free: the waiter (priority 8) takes it at once */
    note('3');
    OSJoinThread(&s_t[0], NULL);
    ok &= check("a mutex is recursive, a waiter sleeps until the last unlock, then takes over",
                strcmp(s_log, "w12L3") == 0);

    s_nlog = 0;
    s_ready = 0;
    OSCreateThread(&s_t[0], cond_waiter, NULL, NULL, 0, 8, 0);
    OSResumeThread(&s_t[0]);
    OSLockMutex(&s_mutex); /* free: the waiter released it in OSWaitCond */
    s_ready = 1;
    OSSignalCond(&s_cond);
    note('s');
    OSUnlockMutex(&s_mutex); /* the waiter gets the mutex back and finishes */
    OSJoinThread(&s_t[0], NULL);
    ok &= check("OSWaitCond releases the mutex, sleeps, and returns holding it again", strcmp(s_log, "csR") == 0);

    s_nlog = 0;
    OSCreateThread(&s_t[0], receiver, NULL, NULL, 0, 8, 0);
    OSResumeThread(&s_t[0]);
    note('m');
    OSSendMessage(&s_mq, (void*)0x77, 0);
    OSJoinThread(&s_t[0], NULL);
    ok &= check("a receiver sleeps on an empty queue and gets the message when it is sent",
                strcmp(s_log, "rmG") == 0 && s_got == (void*)0x77);
    {
        void* a = NULL;
        void* b = NULL;
        OSSendMessage(&s_mq, (void*)1, 0);
        OSJamMessage(&s_mq, (void*)2, 0);
        ok &= check("a full queue refuses a non-blocking send", !OSSendMessage(&s_mq, (void*)3, 0));
        OSReceiveMessage(&s_mq, &a, 0);
        OSReceiveMessage(&s_mq, &b, 0);
        ok &= check("OSJamMessage puts a message first", a == (void*)2 && b == (void*)1);
        ok &= check("an empty queue refuses a non-blocking receive", !OSReceiveMessage(&s_mq, &a, 0));
    }
    {
        size_t j;
        int sync_untouched = 1;
        for (j = 0; j < sizeof(s_mutex); j++) {
            sync_untouched &= s_mutex.bytes[j] == 0xA5;
        }
        for (j = 0; j < sizeof(s_cond); j++) {
            sync_untouched &= s_cond.bytes[j] == 0xA5;
        }
        for (j = 0; j < sizeof(s_mq); j++) {
            sync_untouched &= s_mq.bytes[j] == 0xA5;
        }
        for (j = 0; j < 4; j++) {
            sync_untouched &= s_mq_store[j] == 0xA5A5A5A5u;
        }
        ok &= check("the game's mutex, cond, queue and message memory is never written", sync_untouched);
    }

    /* an interrupt posted from another OS thread runs on the game thread, once interrupts are enabled */
    {
        BOOL level;
        SDL_Thread* poster;
        s_main_tid = SDL_GetCurrentThreadID();
        level = OSDisableInterrupts();
        poster = SDL_CreateThread(poster_thread, "poster", NULL);
        SDL_WaitThread(poster, NULL);
        ok &= check("a posted interrupt waits while interrupts are disabled", s_posted_runs == 0);
        OSRestoreInterrupts(level);
        ok &= check("... runs once, on the game thread, when they are enabled again",
                    s_posted_runs == 1 && s_posted_off_thread == 0);
        poster = SDL_CreateThread(poster_thread, "poster", NULL);
        SDL_WaitThread(poster, NULL);
        VIWaitForRetrace();
        ok &= check("... and while the game waits for a retrace", s_posted_runs == 2 && s_posted_off_thread == 0);
        poster = SDL_CreateThread(call_poster_thread, "dvd", NULL);
        SDL_WaitThread(poster, NULL);
        VIWaitForRetrace();
        ok &= check("posted calls (DVD completions) run once each, in order, with their arguments, on the game thread",
                    strcmp(s_calls_log, "123") == 0 && s_calls_off_thread == 0);

        /* main takes an interrupt whose handler sleeps; the (lower-priority) waker must still get interrupts */
        OSCreateThread(&s_waker, waker_main, NULL, NULL, 0, 20, 1);
        OSResumeThread(&s_waker); /* lower priority than main: runs only while main sleeps */
        poster = SDL_CreateThread(sleeper_poster_thread, "poster3", NULL);
        SDL_WaitThread(poster, NULL);
        level = OSDisableInterrupts();
        OSRestoreInterrupts(level); /* main takes sleeping_handler here, sleeps in it, the waker wakes it */
        ok &= check("a thread still takes interrupts while another sleeps inside a handler",
                    s_sleeper_runs == 1 && s_seen_while_asleep);
        ok &= check("... and the sleeping handler finishes when woken", s_sleeper_back == 1);
        VIWaitForRetrace(); /* let the waker finish */

        OSInitMessageQueue(&s_fq, s_fq_msgs, 2);
        OSCreateThread(&s_receiver, receiver_main, NULL, NULL, 0, 10, 1);
        OSResumeThread(&s_receiver); /* higher priority: runs now and waits for a message */
        poster = SDL_CreateThread(foreign_sender_thread, "mixer", NULL);
        SDL_WaitThread(poster, NULL);
        ok &= check("a wake from another OS thread does not run the woken thread there", s_recv_done == 0);
        level = OSDisableInterrupts();
        OSRestoreInterrupts(level); /* the game thread's next interrupt point */
        ok &= check("... it runs on the game thread at the next interrupt point", s_recv_done == 1 && !s_recv_off_thread);
    }

    for (i = 0; i < 4; i++) {
        size_t j;
        for (j = 0; j < sizeof(s_t[i]); j++) {
            untouched &= s_t[i].bytes[j] == 0xA5;
        }
    }
    for (i = 0; i < 8; i++) {
        untouched &= s_q.bytes[i] == 0xA5;
    }
    ok &= check("the game's OSThread / OSThreadQueue memory is never written", untouched);
    ok &= check("finished threads are freed (main + idle left)", pc_os_thread_count() == 2);

    printf("result: %s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
