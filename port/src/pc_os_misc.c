/*
 * pc_os_misc.c: the remaining small SDK OS functions Aurora declares but does not define (docs/port/sdk-gaps.md,
 * "Errors / reset / context"): error handlers, reset, the system settings the game reads, and the context calls of
 * the crash-report path.
 *
 * Error handlers (OSSetErrorHandler). The game installs its crash-report display (main_tail.c errorExceptionHandler) for every
 * CPU exception. On the PC a crash is a host exception, caught by pc_crash (call stack for addr2line); the game's
 * handlers are recorded (pc_os_error_handler) but not called, because they read the PowerPC register context.
 *
 * Reset (OSRegisterResetFunction / OSResetSystem). The game resets when the console's reset button is released
 * (main.c fn_80005C3C, "back to the system menu"). The SDK first calls every registered reset function with
 * final = FALSE until all agree, then once more with final = TRUE, then reboots. On the PC, "reboot" ends the program
 * (decision: B, 2026-10-15; a restart into the title screen is a later option). The PC has no reset button for now:
 * OSGetResetButtonState is FALSE (a hotkey is a later option).
 *
 * Settings: progressive scan off (matches VIGetDTVStatus = 0 in pc_vi.c), stereo sound, cold boot (reset code 0).
 */
#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include <dolphin/os/OSException.h>
#include <dolphin/os/OSReset.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- error handlers ---- */

#define PC_OS_ERROR_COUNT 17
OSErrorHandler __OSErrorTable[PC_OS_ERROR_COUNT];

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    OSErrorHandler old;
    if (error >= PC_OS_ERROR_COUNT) {
        return NULL;
    }
    old = __OSErrorTable[error];
    __OSErrorTable[error] = handler;
    return old;
}

OSErrorHandler pc_os_error_handler(OSError error) {
    return error < PC_OS_ERROR_COUNT ? __OSErrorTable[error] : NULL;
}

/* ---- reset ---- */

static OSResetFunctionQueue s_reset_queue;
static void (*s_reset_exit)(int code) = exit; /* tests replace it */

/* Kept sorted by priority, lowest first, as in the SDK (equal priorities in registration order) */
void OSRegisterResetFunction(OSResetFunctionInfo* info) {
    OSResetFunctionInfo* it = s_reset_queue.head;
    while (it != NULL && it->priority <= info->priority) {
        it = it->next;
    }
    info->next = it;
    if (it == NULL) {
        info->prev = s_reset_queue.tail;
        if (s_reset_queue.tail != NULL) {
            s_reset_queue.tail->next = info;
        } else {
            s_reset_queue.head = info;
        }
        s_reset_queue.tail = info;
    } else {
        info->prev = it->prev;
        it->prev = info;
        if (info->prev != NULL) {
            info->prev->next = info;
        } else {
            s_reset_queue.head = info;
        }
    }
}

void OSUnregisterResetFunction(OSResetFunctionInfo* info) {
    if (info->next != NULL) {
        info->next->prev = info->prev;
    } else {
        s_reset_queue.tail = info->prev;
    }
    if (info->prev != NULL) {
        info->prev->next = info->next;
    } else {
        s_reset_queue.head = info->next;
    }
    info->next = info->prev = NULL;
}

static BOOL call_reset_functions(BOOL final) {
    OSResetFunctionInfo* it;
    BOOL ok = TRUE;
    for (it = s_reset_queue.head; it != NULL; it = it->next) {
        ok &= it->func(final) != 0; /* every function is called, even after one says "not yet" (SDK behaviour) */
    }
    return ok;
}

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    int tries;
    for (tries = 0; tries < 600 && !call_reset_functions(FALSE); tries++) {
    }
    call_reset_functions(TRUE);
    printf("[orreport] the game asked for a %s (code 0x%08X%s): ending the program\n",
           reset == OS_RESET_HOTRESET ? "hot reset" : reset == OS_RESET_SHUTDOWN ? "shutdown" : "restart",
           resetCode, forceMenu ? ", back to the system menu" : "");
    fflush(stdout);
    {
        /* the game's own reset or shutdown is a normal end: the clean-exit line (5.1, 2026-11-23; pc_log.h). Weak: the
         * SDK test programs link this file without the log */
        extern void pc_log_exit_clean(const char* how) __attribute__((weak));
        if (pc_log_exit_clean) pc_log_exit_clean("the game asked to reset or shut down");
    }
    s_reset_exit(0);
}

/* For tests: run OSResetSystem without ending the program */
void pc_os_set_reset_exit(void (*fn)(int code)) { s_reset_exit = fn != NULL ? fn : exit; }

u32 OSGetResetCode(void) { return 0; } /* cold boot (a restart would be OS_RESETCODE_RESTART) */
BOOL OSGetResetButtonState(void) { return FALSE; }
BOOL OSGetResetSwitchState(void) { return FALSE; }

/* ---- settings ---- */

/* The game sets them from its options menu (fn_800A0FC8 OSSetProgressiveMode, fn_800A0EB4 OSSetSoundMode); on the
 * GameCube they live in the console's battery-backed SRAM. PORT: kept for this run (the port's settings file later). */
static u32 s_progressive_mode = 0;
static u32 s_sound_mode = OS_SOUND_MODE_STEREO;

u32 OSGetProgressiveMode(void) { return s_progressive_mode; }
void OSSetProgressiveMode(u32 on) { s_progressive_mode = on != 0; }
u32 OSGetSoundMode(void) { return s_sound_mode; }
void OSSetSoundMode(u32 mode) { s_sound_mode = mode == OS_SOUND_MODE_MONO ? OS_SOUND_MODE_MONO : OS_SOUND_MODE_STEREO; }

/* The memory-protection unit (OSProtectRange: the game guards memory ranges in debug builds): nothing to set on the
 * PC; real faults are caught by pc_crash */
void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
    (void)chan;
    (void)addr;
    (void)nBytes;
    (void)control;
}

/* No development-kit debugger on the PC */
BOOL DBIsDebuggerPresent(void) { return FALSE; }

/* ---- contexts (the crash-report path) ---- */

static OSContext* s_current_context;

/* The crash display walks the PowerPC stack's back chain from here; there is none on the PC, so 0 ends its walk */
u32 OSGetStackPointer(void) { return 0; }

OSContext* OSGetCurrentContext(void) { return s_current_context; }
void OSSetCurrentContext(OSContext* context) { s_current_context = context; }
void OSClearContext(OSContext* context) { (void)context; /* the game's context is GameCube-sized: not written */ }
void OSFillFPUContext(OSContext* context) { (void)context; }

/* Only the game's decrementer exception handler calls it (main_tail.c errorDecrementerHandler), and that exception does not
 * exist on the PC */
void OSLoadContext(OSContext* context) {
    fprintf(stderr, "[orreport] OSLoadContext(%p): PowerPC register contexts cannot be loaded on the PC\n",
            (void*)context);
    abort();
}

/* ---- memory card disk ID, SI sampling (CARDGetDiskID and CARDSetDiskID by name; fn_800D104C by address) ---- */

/* The memory card code tags files with the disc's ID (game code + maker), per channel. The SDK's card init points
 * both channels at the boot disc's ID at the start of MEM1 (0x80000000), which Aurora's OSInit fills on the PC too;
 * CARDSetDiskID(chan, NULL) goes back to it. Aurora's own CARD library does not use it: kept for the game's reads. */
static const DVDDiskID* s_card_disk_id[2];

const DVDDiskID* CARDGetDiskID(s32 chan) { /* the SDK's name (XD's map, Aurora's card.h); was __CARDGetDiskID */
    if (chan < 0 || chan >= 2) {
        return NULL;
    }
    return s_card_disk_id[chan] != NULL ? s_card_disk_id[chan] : (const DVDDiskID*)(uintptr_t)0x80000000u;
}

s32 CARDSetDiskID(s32 chan, const DVDDiskID* diskID) {
    if (chan < 0 || chan >= 2) {
        return -128; /* CARD_RESULT_FATAL_ERROR */
    }
    s_card_disk_id[chan] = diskID;
    return 0; /* CARD_RESULT_READY */
}

/* The SDK re-applies the controller sampling rate after a video mode change; the PC reads pads through SDL. */
void SIRefreshSamplingRate(void) {}

/* ---- the rest of the first link's SDK / runtime gaps (docs/port/first-link.md) ---- */

/* Audio interface and ARAM: the game's own calls (main.c AIInit; gs_dvd.c's ARAM streaming checks). MusyX's PC layer
 * owns audio output, and ARAM is host memory there, so nothing is ever in flight. */
void AIInit(void* stack) { (void)stack; }
u32 ARGetDMAStatus(void) { return 0; }
u32 ARQGetChunkSize(void) { return 4096; } /* the SDK's default chunk size */

/* SI sampling rate: the PC calls the pad sampling callback once per field (pc_vi.c) */
u32 SISetSamplingRate(u32 msec) {
    (void)msec;
    return 0;
}

/* Exception handlers the game installs for its crash display; recorded, not called (pc_crash catches PC crashes) */
static __OSExceptionHandler s_exception_handlers[16];
__OSExceptionHandler __OSSetExceptionHandler(__OSException exception, __OSExceptionHandler handler) {
    __OSExceptionHandler old = NULL;
    if (exception < 16) {
        old = s_exception_handlers[exception];
        s_exception_handlers[exception] = handler;
    }
    return old;
}

/* MSL runtime: double -> unsigned int (assembly on the GameCube). Its contract takes a double (f1 on the PowerPC, where
 * a float argument travels as a double anyway); __cntlzw / __abs / __memcpy / __fabs / __frsqrte are static inline in
 * include/pc/mwcc_intrinsics.h. Callers that declare it with an f32 parameter need the canonical prototype on the
 * PC (asked of Main): an x64 float argument is not a double. Saturating, as the runtime does. */
unsigned int __cvt_fp2unsigned(double x) {
    if (!(x > 0.0)) {
        return 0;
    }
    return x >= 4294967296.0 ? 0xFFFFFFFFu : (unsigned int)x;
}

/* SDK functions the game's data tables point at (reset / interrupt tables); never called on the PC */
static void never_called(const char* name) {
    fprintf(stderr, "[orreport] %s called: a GameCube-only SDK routine\n", name);
}
void __OSDoHotReset(void) { never_called("__OSDoHotReset"); }
void __GXShutdown_800C6260(void) { never_called("__GXShutdown"); }
void SIInterruptHandler_800CFA60(void) { never_called("SIInterruptHandler"); }
