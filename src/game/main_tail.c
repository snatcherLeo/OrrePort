/**
 * @file main_tail.c
 * @brief Matching retrace callbacks and OS error handlers.
 *
 * Address range: 0x80005FA8 - 0x80006630
 */

#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif
extern void OSSetErrorHandler(u32 error, void* handler);
extern void OSLoadContext(void* context);
extern void OSEnableScheduler(void);
extern void GSthreadExecuteAll(void);
extern u8 fn_8000DAA8(void);
extern void peopleMain(u32 a, u32 b);
extern u8 GSinputIsConnected(u32 padIdx);
extern void fn_8008AC34(u32 padIdx);
extern u8 fn_8008ABA0(u32 padIdx);
extern void GSinputRead(u32 padIdx);
extern void* soundSetListener(void* color);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSresRegisterResource(void* work, u32 time, u32 numFrames, u32 arg);
#endif
extern void OSProtectRange(u32 region, u32 addr, u32 size, u32 perm);
extern s32 DBIsDebuggerPresent(void);
extern void OSCreateThread(void* thread, void* func, void* arg,
                           void* stackBase, u32 stackSize,
                           u32 priority, u32 detached);
extern void OSResumeThread(void* thread);
extern void fn_800A1E54(void* thread, u32 prio);
extern void __OSSetExceptionHandler(u32 exception, void* handler);
extern void* __va_arg(void* ap, u32 type);
extern u32 OSGetStackPointer(void);

void errorPanicStop(const char* file, s32 line, const char* fmt, ...);
void errorExceptionHandler(u32 error, void* context, ...);
void errorDecrementerHandler(u32 error, void* context);
s32 errorReportThreadMain(void);

extern s32 lbl_80478990;
extern u32 lbl_8047A260;
extern u32 lbl_8047A264;
extern u32 lbl_8047A268;
extern u8 lbl_80266438[];
extern u8 lbl_803A0700[];
extern u8 lbl_803A1700[];
extern u8 lbl_8039E700[];
extern u8 lbl_802E2888[];
extern const char lbl_80266448[];
extern char lbl_8047B6B8[8];
extern char lbl_8047B6C0[8];

#ifndef TARGET_PC /* MWCC's PowerPC va_list; the PC uses the host's <stdarg.h> */
typedef struct __va_list_struct {
    u8 gpr;
    u8 fpr;
    u16 padding;
    u32* overflow_arg_area;
    u32* reg_save_area;
} __va_list_struct;
typedef __va_list_struct va_list[1];
#define va_start(ap, last) ((void)last, __builtin_va_info(&ap))
#define va_end(ap) ((void)0)
#else
#include <stdarg.h>
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int vsprintf(char* buf, const char* fmt, va_list ap);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int sprintf(char* buf, const char* fmt, ...);
#endif

/* =========================================================================
 *  mainLogicTask / mainLogicTask
 *  Address: 0x80005FA8, Size: 0x54
 *
 *  Audio retrace callback. Called with (error, context) from the task
 *  system. Updates the audio frame, then checks the locale flag to
 *  potentially trigger a reconnect handler.
 * =========================================================================
 */
void mainLogicTask(u32 a, u32 b)
{
    GSthreadExecuteAll();
    if (fn_8000DAA8() == 0) {
        peopleMain(a, b);
    }
}

/* =========================================================================
 *  fn_80005FFC / fn_80005FFC
 *  Address: 0x80005FFC, Size: 0x50
 *
 *  Controller polling task. Iterates pads 1-4:
 *  if a pad is NOT responding to GSinputIsConnected (connection check),
 *  it calls fn_8008AC34 to recalibrate that pad.
 * =========================================================================
 */
void fn_80005FFC(void) {
    s32 i;

    for (i = 1; i <= 4; i++) {
        if (GSinputIsConnected(i) == 0) {
            fn_8008AC34(i);
        }
    }
}

/* =========================================================================
 *  fn_8000604C / fn_8000604C
 *  Address: 0x8000604C, Size: 0x50
 *
 *  Controller rumble update task. Iterates pads 1-4:
 *  if a pad is NOT responding to fn_8008ABA0 (motor check),
 *  it calls GSinputRead to reset that pad's rumble state.
 * =========================================================================
 */
void fn_8000604C(void) {
    s32 i;

    for (i = 1; i <= 4; i++) {
        if (fn_8008ABA0(i) == 0) {
            GSinputRead(i);
        }
    }
}

/* =========================================================================
 *  InitBackgroundColor / fn_8000609C
 *  Address: 0x8000609C, Size: 0x54
 *
 *  Copies the default background color from rodata (lbl_80266438) into
 *  a local buffer and calls the floor system to set the clear color.
 *  Also configures a render schedule with a 2000-tick timeout.
 * =========================================================================
 */
void fn_8000609C(void) {
    struct BgColor12 { u32 c[3]; } buf;
    buf = *(struct BgColor12*)lbl_80266438;
    GSresRegisterResource(soundSetListener(&buf), 0, 0x7D0, 0);
}

/* =========================================================================
 *  errorPanicStop (0x800060F0, size 0x160): the game's panic routine (the original file is error.c, which this
 *  routine and errorReportThreadMain pass as their own file name; verified from the strings).
 *
 *  Called by the game and the disc code (DVD, the particle loader, HSD_Panic, the GBA uploader) with a file name,
 *  a line and a printf-style message. It formats into the shared report buffer (lbl_803A0700): the message,
 *  "in <file> on line <n>" and a walk of up to 32 stack frames (back chain, return address), then calls itself with
 *  that report, again and again, so it never returns: on the console the game simply stops (verified from the
 *  code; the reference playthrough P12's crash, port-gaps 74, came through here).
 * =========================================================================
 */
/* NAME-T3 fn_800060F0 */
void errorPanicStop(const char* file, s32 line, const char* fmt, ...) {
    va_list ap;
    int len;
    register u32 i;
    register u32* p;
    register int pos;
    u32 table = PC_PTR32(lbl_80266448);

    va_start(ap, fmt);
    len = vsprintf((char*)lbl_803A0700, fmt, ap);
    pos = len + sprintf((char*)lbl_803A0700 + len, (const char*)PC_PTR((table + 0x178)), file, line);
    pos += sprintf((char*)lbl_803A0700 + pos, (const char*)PC_PTR((table + 0x190)));
    for (i = 0, p = (u32*)PC_PTR(OSGetStackPointer()); p && PC_PTR32(p) != 0xFFFFFFFF && i++ < 0x20; p = (u32*)PC_PTR(*p)) {
        pos += sprintf((char*)lbl_803A0700 + pos, (const char*)PC_PTR((table + 0x1B4)), p, p[0], p[1]);
    }
#ifdef TARGET_PC
    /* PC, observe only (2026-11-07, port-gaps 74): the original calls itself here with the formatted report, so
     * on the console a panic hangs; on the PC the recursion overflows the stack and Windows closes the game with
     * nothing in the log (the reference playthrough P12). Print the first panic's message and the host stack once, then carry on as the
     * original does. */
    {
        static int s_reported;
        if (!s_reported) {
            extern void pc_crash_print_stack(const char* reason);
            s_reported = 1;
            fprintf(stderr, "\n[panic] the game's panic routine (errorPanicStop) was called: %s, line %d: %s\n",
                    file != NULL ? file : "(null)", (int)line, (const char*)lbl_803A0700);
            fflush(stderr);
            pc_crash_print_stack("the game panicked (the original hangs here; on the PC the stack overflows next)");
            fflush(stderr);
        }
    }
#endif
    errorPanicStop(lbl_8047B6B8, 0x196, (char*)lbl_803A0700);
}

/* =========================================================================
 *  errorInit (0x80006250, size 0x128): the error module's start-up, called once from the game's start-up
 *  (main.c). Verified from the code:
 *
 *  Sets up memory protection regions and installs errorExceptionHandler for these OS error types:
 *    0  = SYSTEM_RESET
 *    1  = MACHINE_CHECK
 *    2  = DSI
 *    3  = ISI
 *    5  = ALIGNMENT
 *    11 = FLOATING_POINT
 *    13 = PERFORMACE_MONITOR
 *    14 = BREAKPOINT
 *    15 = SYSTEM_INTERRUPT
 *    16 = THERMAL_INTERRUPT
 *  If the debugger is not connected (DBIsDebuggerPresent returns 0), also
 *  installs handlers for:
 *    6  = PROGRAM
 *    10 = SYSTEM_CALL
 *    12 = TRACE
 *
 *  Memory protection regions configured:
 *    Region 0: 0x00000000, size 0x80000000, permission 3 (RW)
 *    Region 1: 0x81800000, size 0x01800000, permission 3 (RW)
 * =========================================================================
 */
/* NAME-T3 fn_80006250 */
void errorInit(void) {
    /* Set up memory protection regions */
    OSProtectRange(0, 0, 0x80000000, 3);
    OSProtectRange(1, 0x81800000, 0x01800000, 3);

    /* Install custom error handler for standard exceptions */
    OSSetErrorHandler(0, errorExceptionHandler);   /* SYSTEM_RESET */
    OSSetErrorHandler(1, errorExceptionHandler);   /* MACHINE_CHECK */
    OSSetErrorHandler(2, errorExceptionHandler);   /* DSI */
    OSSetErrorHandler(3, errorExceptionHandler);   /* ISI */
    OSSetErrorHandler(5, errorExceptionHandler);   /* ALIGNMENT */
    OSSetErrorHandler(11, errorExceptionHandler);  /* FLOATING_POINT */
    OSSetErrorHandler(13, errorExceptionHandler);  /* PERFORMACE_MONITOR */
    OSSetErrorHandler(14, errorExceptionHandler);  /* BREAKPOINT */
    OSSetErrorHandler(15, errorExceptionHandler);  /* SYSTEM_INTERRUPT */

    /* lbl_80478990 = 16 just before the last install (guess: the highest error number handled) */
    lbl_80478990 = 16;
    OSSetErrorHandler(16, errorExceptionHandler);  /* THERMAL_INTERRUPT */

    /* If debugger is not attached, also handle program/trace/syscall */
    if (DBIsDebuggerPresent() == 0) {
        OSSetErrorHandler(6, errorExceptionHandler);   /* PROGRAM */
        OSSetErrorHandler(10, errorExceptionHandler);  /* SYSTEM_CALL */
        OSSetErrorHandler(12, errorExceptionHandler);  /* TRACE */
    }
}

/* =========================================================================
 *  errorExceptionHandler (0x80006378, size 0x128): the handler errorInit installs for the OS errors. Verified from
 *  the code: it keeps the two extra values the OS passes (likely DSISR and DAR, the SDK's error-handler arguments),
 *  copies the CPU context (0x2C8 bytes) to lbl_803A1700 and the error number to lbl_8047A260, installs
 *  errorDecrementerHandler for exception 8, turns the scheduler back on, and starts a thread running
 *  errorReportThreadMain, which writes the report.
 * =========================================================================
 */
typedef struct ErrVaList {
    u8 gpr;
    u8 fpr;
    u16 padding;
    u32* overflow_arg_area;
    u32* reg_save_area;
} ErrVaList;
typedef ErrVaList ErrVaListArray[1];

typedef struct ErrThread {
    u64 pad[0x318 / 8];
} ErrThread;

/* NAME-T3 fn_80006378 */
void errorExceptionHandler(u32 error, void* context, ...) {
    ErrThread thread;
    ErrVaListArray args;

#ifdef TARGET_PC
    {
        /* the host's variadic list; the GameCube walks MWCC's register-save structure (port-gaps 16) */
        __builtin_va_list pcArgs;
        __builtin_va_start(pcArgs, context);
        lbl_8047A264 = __builtin_va_arg(pcArgs, u32);
        lbl_8047A268 = __builtin_va_arg(pcArgs, u32);
        __builtin_va_end(pcArgs);
    }
#else
    __builtin_va_info(&args);
    lbl_8047A264 = *(u32*)__va_arg(args, 1);
    lbl_8047A268 = *(u32*)__va_arg(args, 1);
#endif
    memcpy(lbl_803A1700, context, 0x2C8);
    lbl_8047A260 = (u16)error;
    __OSSetExceptionHandler(8, errorDecrementerHandler);
    OSEnableScheduler();
    OSCreateThread(&thread, errorReportThreadMain, NULL, lbl_8039E700 + 0x1FFC, 0x2000, 1, 0);
    OSResumeThread(&thread);
    fn_800A1E54(&thread, 0);
}

/* =========================================================================
 *  errorDecrementerHandler (0x800064A0, size 0x24): installed by errorExceptionHandler for exception 8 (the
 *  decrementer, the OS timer). It only reloads the given context (verified from the code), so the report thread
 *  keeps running while the error is shown.
 * =========================================================================
 */
/* NAME-T3 fn_800064A0 */
void errorDecrementerHandler(u32 error, void* context) {
    OSLoadContext(context);
}

/* =========================================================================
 *  errorReportThreadMain (0x800064C4, size 0x16C): the body of the thread errorExceptionHandler starts. Verified from
 *  the code and strings: it formats into the report buffer (lbl_803A0700) the error's name from the OS_ERROR_... table
 *  (lbl_802E2888, "OS_ERROR_SYSTEM_RESET" ... "OS_ERROR_PROTECTION"), four saved CPU values (the context's words at
 *  +0x84 / +0x80 and +0x198 / +0x19C: likely LR / CR and SRR0 / SRR1), the two extra values errorExceptionHandler kept,
 *  and up to 32 stack frames, then hands the report to errorPanicStop with file "error.c".
 * =========================================================================
 */
/* NAME-T3 fn_800064C4 */
s32 errorReportThreadMain(void) {
    const char* strs = (const char*)lbl_80266448;
    s32 n;
    u32 i;
    u32* sp;
    s32 len;

    n = sprintf((char*)lbl_803A0700, lbl_8047B6C0,
                    ((char**)lbl_802E2888)[lbl_8047A260]);
    len = n + sprintf((char*)lbl_803A0700 + n, strs + 0x1CC,
                       *(u32*)(lbl_803A1700 + 0x84), *(u32*)(lbl_803A1700 + 0x80));
    len += sprintf((char*)lbl_803A0700 + len, strs + 0x1EC,
                       *(u32*)(lbl_803A1700 + 0x198), *(u32*)(lbl_803A1700 + 0x19C));
    len += sprintf((char*)lbl_803A0700 + len, strs + 0x20C,
                       lbl_8047A264, lbl_8047A268);
    len += sprintf((char*)lbl_803A0700 + len, strs + 0x190);

    i = 0;
    sp = (u32*)PC_PTR(*(u32*)(lbl_803A1700 + 0x4));
    while (sp != NULL && PC_PTR32(sp) != 0xFFFFFFFF && i++ < 0x20) {
        len += sprintf((char*)lbl_803A0700 + len, strs + 0x1B4,
                           sp, sp[0], sp[1]);
        sp = (u32*)PC_PTR(sp[0]);
    }

    errorPanicStop(lbl_8047B6B8, 0xD5, (char*)lbl_803A0700);
    return 0;
}
