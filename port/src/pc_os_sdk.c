/*
 * pc_os_sdk.c: SDK OS functions Aurora declares but does not define (docs/port/sdk-gaps.md): interrupt masking and
 * OSReport (alarms: pc_os_alarm.c). More of the list (threads, reset) follows here.
 *
 * Interrupt masking. On the GameCube, OSDisableInterrupts stops interrupt handlers (the audio DMA interrupt that runs
 * MusyX's mixer, DVD and VI callbacks) from running until OSRestoreInterrupts / OSEnableInterrupts; the game brackets
 * every update of state those handlers share. On the PC those "handlers" run on other threads (MusyX's audio thread
 * via SDL, Aurora's workers), so the bracket becomes one recursive lock that all of them take:
 *   - OSDisableInterrupts takes the lock and returns whether interrupts were enabled (the old state);
 *   - OSRestoreInterrupts(level) undoes one OSDisableInterrupts (the usual `old = Disable(); ...; Restore(old);`
 *     pairs nest correctly); with no Disable to undo and level FALSE it takes the lock (the GameCube would disable);
 *   - OSEnableInterrupts undoes all of this thread's Disables.
 * The depth is per thread, so each thread only ever releases what it took. MusyX's audio thread takes the same lock
 * through pc_os_audio_enter / pc_os_audio_leave (installed with sndPCSetSynchronization before sndInit), so the
 * mixer never runs in the middle of a game sound update. melee-pc solves the same problem the same way (studied, not
 * copied). The lock is SDL3's mutex, which is recursive and cross-platform.
 */
#include <dolphin/types.h>

#include <SDL3/SDL_mutex.h>

#include <stdarg.h>
#include <stdio.h>

static SDL_Mutex* s_intr_lock;
static _Thread_local int s_intr_depth;

/* "Interrupts are enabled again": the SDK thread scheduler (pc_os_thread.c) delivers a due retrace here, as the
 * GameCube takes a pending interrupt the moment the enable bit is set. Only on the game's OS thread. */
static void (*s_intr_hook)(void);
static _Thread_local int s_is_hook_thread;

void pc_os_set_interrupt_hook(void (*hook)(void)) {
    s_intr_hook = hook;
    s_is_hook_thread = 1;
}

static void enabled_again(void) {
    if (s_is_hook_thread && s_intr_hook != NULL) {
        s_intr_hook();
    }
}

static SDL_Mutex* intr_lock(void) {
    /* first use is on the game thread during startup, before any other thread exists */
    if (s_intr_lock == NULL) {
        s_intr_lock = SDL_CreateMutex();
    }
    return s_intr_lock;
}

/* An interrupt point outside OSRestoreInterrupts: a status poll the game may spin on (pc_dvd.c, DVD status). With
 * interrupts enabled, pending interrupts are taken here, as the GameCube would take the DVD interrupt mid-loop. */
void pc_os_interrupt_point(void) {
    if (s_intr_depth == 0) {
        enabled_again();
    }
}

BOOL OSDisableInterrupts(void) {
    BOOL wasEnabled = s_intr_depth == 0;
    SDL_LockMutex(intr_lock());
    ++s_intr_depth;
    return wasEnabled;
}

BOOL OSRestoreInterrupts(BOOL level) {
    BOOL wasEnabled = s_intr_depth == 0;
    if (s_intr_depth > 0) {
        --s_intr_depth;
        SDL_UnlockMutex(intr_lock());
        if (s_intr_depth == 0) {
            enabled_again();
        }
    } else if (!level) {
        SDL_LockMutex(intr_lock());
        ++s_intr_depth;
    }
    return wasEnabled;
}

BOOL OSEnableInterrupts(void) {
    BOOL wasEnabled = s_intr_depth == 0;
    while (s_intr_depth > 0) {
        --s_intr_depth;
        SDL_UnlockMutex(intr_lock());
    }
    if (!wasEnabled) {
        enabled_again();
    }
    return wasEnabled;
}

/* For the SDK thread switch (pc_os_thread.c): each SDK thread has its own interrupt state, but the lock's depth
 * belongs to the OS thread, so a switch hands the outgoing thread's depth back and takes the incoming one's. */
int pc_os_intr_release_all(void) {
    int depth = s_intr_depth;
    while (s_intr_depth > 0) {
        --s_intr_depth;
        SDL_UnlockMutex(intr_lock());
    }
    return depth;
}

void pc_os_intr_reacquire(int depth) {
    while (s_intr_depth < depth) {
        SDL_LockMutex(intr_lock());
        ++s_intr_depth;
    }
}

/* For MusyX's audio thread (sndPCSetSynchronization): the mixer counts as an interrupt handler */
void pc_os_audio_enter(void) { (void)OSDisableInterrupts(); }
void pc_os_audio_leave(void) { (void)OSRestoreInterrupts(FALSE); }

/* For tests: this thread's nesting depth */
int pc_os_interrupt_depth(void) { return s_intr_depth; }

void OSReport(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    fflush(stdout);
}
