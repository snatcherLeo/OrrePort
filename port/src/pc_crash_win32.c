/* pc_crash on Windows x64: unhandled-exception filter + stack walk with RtlVirtualUnwind. See pc_crash.h. */
#include "pc_crash.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <tlhelp32.h> /* the hang watch's list of every thread (report_other_threads) */
#include <dbghelp.h> /* the minidump's types only; the function is loaded at run time (pc_crash_set_dump_path) */

/* The exe's preferred image base, as addr2line uses it, read from the exe FILE once at install (Studio's finding,
 * 2026-11-22): when the loader relocates an exe (ASLR, a MinGW exe linked with --dynamicbase, as Studio's is), it also
 * rewrites ImageBase in the in-memory header to the actual load address, so reading it there printed absolute
 * addresses (0x7ff6...) that addr2line can't place. OrrePort's own exe is linked at a fixed base without DYNAMIC_BASE
 * (verified from its header: ImageBase 0x10000000, DllCharacteristics 0x100), so its printed addresses were already
 * right; the shared code (Studio links this file) needs the file's value. 0 when the file can't be read: then the
 * in-memory value, as before. Windows only (Linux / macOS crash reports take their own path: pc_crash.h). */
static DWORD64 s_preferred_base;

static DWORD64 exe_preferred_base(HMODULE exe) {
    if (s_preferred_base != 0) {
        return s_preferred_base;
    }
    {
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)exe;
        const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)((const uint8_t*)exe + dos->e_lfanew);
        return nt->OptionalHeader.ImageBase;
    }
}

static void read_preferred_base(void) {
    wchar_t path[MAX_PATH * 2];
    uint8_t head[4096];
    DWORD got = 0;
    HANDLE f;
    DWORD n = GetModuleFileNameW(NULL, path, (DWORD)(sizeof(path) / sizeof(path[0])));
    if (n == 0 || n >= sizeof(path) / sizeof(path[0])) {
        return;
    }
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        return;
    }
    if (ReadFile(f, head, sizeof(head), &got, NULL) && got >= 0x40) {
        const uint32_t pe = (uint32_t)head[0x3c] | (uint32_t)head[0x3d] << 8 | (uint32_t)head[0x3e] << 16 |
                            (uint32_t)head[0x3f] << 24;
        /* "PE\0\0", then the 20-byte file header, then the PE32+ optional header (magic 0x20b), ImageBase at +24 */
        if ((uint64_t)pe + 24 + 32 <= got && memcmp(head + pe, "PE\0\0", 4) == 0 && head[pe + 24] == 0x0b &&
            head[pe + 25] == 0x02) {
            DWORD64 base = 0;
            int i;
            for (i = 7; i >= 0; i--) {
                base = base << 8 | head[pe + 24 + 24 + i];
            }
            s_preferred_base = base;
        }
    }
    CloseHandle(f);
}

/* The address as addr2line expects it: relative to the preferred image base in the PE header, not where the loader
 * actually put the module (ASLR may move it). Addresses outside the executable are printed as module+offset. */
static void print_address(const char* label, DWORD64 address) {
    HMODULE exe = GetModuleHandleA(NULL);
    HMODULE module = NULL;
    char name[MAX_PATH] = "?";

    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)(uintptr_t)address, &module) && module != NULL) {
        GetModuleFileNameA(module, name, sizeof(name));
    }
    if (module == exe && exe != NULL) {
        DWORD64 preferred = exe_preferred_base(exe) + (address - (DWORD64)(uintptr_t)exe);
        fprintf(stderr, "  %s 0x%llx\n", label, (unsigned long long)preferred);
    } else {
        const char* base = strrchr(name, '\\');
        fprintf(stderr, "  %s %s+0x%llx\n", label, base ? base + 1 : name,
                (unsigned long long)(address - (DWORD64)(uintptr_t)module));
    }
}

static void walk_stack(CONTEXT ctx);

/* The frames from ctx outwards as raw addresses, with no C runtime and no loader calls: the watchdog runs this while the
 * watched thread is suspended (Studio's finding, 2026-11-22: if that thread was stopped inside the C runtime's stderr
 * lock, an fprintf here would wait for it forever and the thread would never be resumed, turning a passing stall into
 * a permanent hang; GetModuleHandleEx in print_address takes the loader lock, the same danger). pcs[0] is "at", the
 * rest "called from" (already minus one, as printed). RtlLookupFunctionEntry reads the system's function table under
 * a lock that only module loading holds exclusively (likely; a stall during a DLL load is the one case left). */
#define PC_CRASH_MAX_FRAMES 33
static int capture_stack(CONTEXT ctx, DWORD64* pcs) {
    int n = 0, frame;
    pcs[n++] = ctx.Rip;
    for (frame = 0; frame < 32 && (ctx.Rip != 0 || frame == 0); frame++) {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fn = ctx.Rip != 0 ? RtlLookupFunctionEntry(ctx.Rip, &imageBase, NULL) : NULL;
        if (fn == NULL) {
            ctx.Rip = *(DWORD64*)(uintptr_t)ctx.Rsp;
            ctx.Rsp += 8;
        } else {
            void* handlerData = NULL;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &establisher, NULL);
        }
        if (ctx.Rip != 0 && n < PC_CRASH_MAX_FRAMES) {
            pcs[n++] = ctx.Rip - 1;
        }
    }
    return n;
}

/* ...and printed later, once the thread runs again (the same lines walk_stack prints) */
static void print_captured(const DWORD64* pcs, int n) {
    int i;
    for (i = 0; i < n; i++) {
        print_address(i == 0 ? "at" : "called from", pcs[i]);
    }
    fprintf(stderr, "[pc_crash] decode with: addr2line -f -e <exe> <addresses>\n");
    fflush(stderr);
}

static void print_crash(EXCEPTION_POINTERS* info) {
    CONTEXT ctx = *info->ContextRecord;

    fprintf(stderr, "\n[pc_crash] exception 0x%08lX", info->ExceptionRecord->ExceptionCode);
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2) {
        fprintf(stderr, " (%s address 0x%llx)", info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
    }
    fprintf(stderr, "\n");
    walk_stack(ctx);
}

/* Prints the frames from ctx outwards: "at" for ctx itself, "called from" for each caller (frame 0 may be a call
 * through a null pointer, Rip == 0: its return address is on top of the stack; a leaf function's too). For the
 * crashing thread itself; another thread's stack is captured while it is stopped and printed after (the watchdog). */
static void walk_stack(CONTEXT ctx) {
    DWORD64 pcs[PC_CRASH_MAX_FRAMES];
    print_captured(pcs, capture_stack(ctx, pcs));
}

static volatile LONG s_printed;

void (*pc_crash_report_hook)(void);

/* ---- the minidump (pc_crash.h says why and where) ----
 * dbghelp is loaded when the path is set, not at the crash: loading a DLL inside the exception filter can deadlock on
 * the loader lock. Its type set: the threads and their stacks (MiniDumpNormal), each thread's state, the unloaded
 * modules, and the memory around pointers found on the stacks (MiniDumpWithIndirectlyReferencedMemory), enough to see
 * the variables near the crash in a debugger, without the whole address space (OrrePort maps 4 GB of GameCube memory;
 * a full dump would be gigabytes). */
typedef BOOL(WINAPI* MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
static MiniDumpWriteDumpFn s_write_dump;
static wchar_t s_dump_path[1024];

void pc_crash_set_dump_path(const char* utf8_path) {
    if (utf8_path == NULL || MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, s_dump_path, 1024) <= 0) {
        s_dump_path[0] = 0;
        return;
    }
    if (s_write_dump == NULL) {
        HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
        s_write_dump = dbghelp != NULL ? (MiniDumpWriteDumpFn)(void*)GetProcAddress(dbghelp, "MiniDumpWriteDump") : NULL;
    }
}

static void write_dump(EXCEPTION_POINTERS* info) {
    MINIDUMP_EXCEPTION_INFORMATION ex;
    HANDLE f;
    BOOL ok;
    if (s_write_dump == NULL || s_dump_path[0] == 0) {
        return;
    }
    f = CreateFileW(s_dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[pc_crash] no minidump: the file can't be created\n");
        return;
    }
    ex.ThreadId = GetCurrentThreadId();
    ex.ExceptionPointers = info;
    ex.ClientPointers = FALSE;
    ok = s_write_dump(GetCurrentProcess(), GetCurrentProcessId(), f,
                      (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                                      MiniDumpWithIndirectlyReferencedMemory),
                      &ex, NULL, NULL);
    CloseHandle(f);
    fprintf(stderr, ok ? "[pc_crash] minidump written next to this log (.dmp)\n"
                       : "[pc_crash] the minidump couldn't be written\n");
}

/* The report, the minidump, then the hook (where the game was, the run log flushed: pc_game_main.c), from either path
 * below. The dump comes after the printed report, so the log has the stack even if writing the dump fails. */
static void report(EXCEPTION_POINTERS* info) {
    print_crash(info);
    write_dump(info);
    if (pc_crash_report_hook != NULL) {
        pc_crash_report_hook();
    }
}

static LONG WINAPI on_crash(EXCEPTION_POINTERS* info) {
    if (InterlockedExchange(&s_printed, 1) == 0) {
        report(info);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

/* First-chance note, written before anything else can fail: the code, the faulting address and the stack pointer, with
 * almost no stack of its own. Without it a crash on an exhausted (fibre) stack left no output at all, because the
 * unhandled-exception filter itself had no stack left to run on. Only fatal codes; the game's own exceptions don't
 * reach here. */
static LONG WINAPI on_first_chance(EXCEPTION_POINTERS* info) {
    static const char hex[] = "0123456789abcdef";
    static char line[96];
    DWORD code = info->ExceptionRecord->ExceptionCode;
    DWORD64 values[3];
    DWORD written;
    int pos = 0, v, i;

    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    values[0] = code;
    values[1] = info->ContextRecord->Rip;
    values[2] = info->ContextRecord->Rsp;
    for (i = 0; "[pc_crash] first chance code/rip/rsp"[i]; i++) {
        line[pos++] = "[pc_crash] first chance code/rip/rsp"[i];
    }
    for (v = 0; v < 3; v++) {
        line[pos++] = ' ';
        for (i = 15; i >= 0; i--) {
            line[pos++] = hex[(values[v] >> (i * 4)) & 0xF];
        }
    }
    line[pos++] = '\n';
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, (DWORD)pos, &written, NULL);
    /* A fault inside the game executable itself is never expected: walk its stack now, because the unhandled-exception
     * filter is not always reached (seen 2026-10-15: no output from it at all). */
    /* A call through a NULL function pointer (rip == 0, 2026-10-17: Willie's battle wipe): no module owns address 0,
     * so the report below never ran. The caller's return address is at [rsp]: unwind that one step by hand, so the walk
     * starts in the caller (the frame that made the bad call). */
    /* 2026-10-17: also a call to an address no module owns (rip = 0xFFFFFFFFFFFFFFFF after choosing a move in
     * Willie's battle): unwind it the same way. */
    HMODULE ripOwner = NULL;
    BOOL ripOwned = info->ContextRecord->Rip != 0 &&
                    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)(uintptr_t)info->ContextRecord->Rip, &ripOwner) && ripOwner != NULL;
    if (!ripOwned && info->ContextRecord->Rsp != 0 && InterlockedExchange(&s_printed, 1) == 0) {
        static CONTEXT copy; /* a copy: the live context is left untouched for the other handlers */
        EXCEPTION_POINTERS shifted;
        static const char note[] = "[pc_crash] called a NULL function pointer; the stack below starts at the caller\n";
        DWORD w2;
        copy = *info->ContextRecord;
        copy.Rip = *(DWORD64*)(uintptr_t)copy.Rsp;
        copy.Rsp += 8;
        shifted.ExceptionRecord = info->ExceptionRecord;
        shifted.ContextRecord = &copy;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), note, (DWORD)(sizeof(note) - 1), &w2, NULL);
        report(&shifted);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    {
        HMODULE exe = GetModuleHandleA(NULL);
        HMODULE module = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)info->ContextRecord->Rip, &module) &&
            (module == exe || getenv("ORRE_CRASH_ANY_MODULE") != NULL) && InterlockedExchange(&s_printed, 1) == 0) {
            report(info);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* Integer divide errors handled the way the GameCube does (2026-11-04; port-gaps 58). PowerPC's divide
 * instructions never trap: dividing by zero (or the lowest 32-bit number by -1) just gives a fixed result, and the game
 * goes on. x86's DIV / IDIV stop the program instead, so a division the console survives crashed the PC (port-gaps 44:
 * the battle camera's `random % duration` with a duration of 0; port-gaps 30: a party HP bar dividing by a max HP of
 * 0). Those two stay fixed at their sites; this handler catches the CPU's divide error in the
 * executable's own code, decodes the DIV / IDIV that raised it, writes PowerPC's result and resumes after it:
 *   - signed (divw):   quotient -1 if the dividend is negative, else 0 (also for the lowest number divided by -1);
 *   - unsigned (divwu): quotient 0;
 *   - remainder: dividend - quotient * divisor, which is how the game's compiler builds `%` from the quotient.
 * These are the results Dolphin gives (its interpreter's divw / divwu; likely the hardware's, not checked on a console).
 * Each one is logged (the first 50) with its address, so we learn where the game does it. ORRE_DIVZERO=0 turns it off
 * (the divide error crashes as before: useful to find a division by zero in our own code). ORRE_TEST_DIVZERO=1 runs a
 * self-test at start-up. Linux / Mac (a SIGFPE handler) come with those ports. */
static volatile LONG s_div_hits;

static uint32_t read_u32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static LONG WINAPI on_divide_error(EXCEPTION_POINTERS* info) {
    CONTEXT* c = info->ContextRecord;
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    DWORD64* regs = &c->Rax; /* Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi, R8-R15: the x86 register numbers' order */
    const uint8_t* p = (const uint8_t*)(uintptr_t)c->Rip;
    HMODULE module = NULL;
    int i = 0, rex = 0, size = 4, mod, reg, rm, is_signed;
    uint64_t divisor = 0, mask, a, q, r;

    if (code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_INT_OVERFLOW) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)p, &module) ||
        module != GetModuleHandleA(NULL)) {
        return EXCEPTION_CONTINUE_SEARCH; /* only the executable's own code (the game, the port, Aurora) */
    }
    if (p[i] == 0x66) {
        size = 2;
        i++;
    }
    if ((p[i] & 0xF0) == 0x40) {
        rex = p[i++];
        if (rex & 8) {
            size = 8;
        }
    }
    if (p[i] == 0xF6) {
        size = 1;
    } else if (p[i] != 0xF7) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    i++;
    mod = p[i] >> 6;
    reg = (p[i] >> 3) & 7;
    rm = p[i] & 7;
    i++;
    if (reg != 6 && reg != 7) {
        return EXCEPTION_CONTINUE_SEARCH; /* not DIV (/6) or IDIV (/7) */
    }
    is_signed = reg == 7;
    if (mod == 3) {
        const int n = rm | ((rex & 1) << 3);
        divisor = (size == 1 && rex == 0 && n >= 4) ? (regs[n - 4] >> 8) : regs[n]; /* AH..BH without a REX prefix */
    } else {
        uint64_t ea = 0;
        if (rm == 4) {
            const uint8_t sib = p[i++];
            const int index = ((sib >> 3) & 7) | ((rex & 2) << 2), base = (sib & 7) | ((rex & 1) << 3);
            if (index != 4) {
                ea += regs[index] << (sib >> 6);
            }
            if ((sib & 7) == 5 && mod == 0) {
                ea += (uint64_t)(int64_t)(int32_t)read_u32(p + i);
                i += 4;
            } else {
                ea += regs[base];
            }
        } else if (rm == 5 && mod == 0) {
            const int32_t disp = (int32_t)read_u32(p + i);
            i += 4;
            ea = c->Rip + (uint64_t)i + (uint64_t)(int64_t)disp; /* RIP-relative: from the next instruction */
        } else {
            ea = regs[rm | ((rex & 1) << 3)];
        }
        if (mod == 1) {
            ea += (uint64_t)(int64_t)(int8_t)p[i];
            i += 1;
        } else if (mod == 2) {
            ea += (uint64_t)(int64_t)(int32_t)read_u32(p + i);
            i += 4;
        }
        memcpy(&divisor, (const void*)(uintptr_t)ea, (size_t)size);
    }
    mask = size == 8 ? ~0ull : (1ull << (size * 8)) - 1;
    divisor &= mask;
    a = regs[0] & mask; /* the dividend's low part: EAX / RAX (AX for 16-bit, AL for 8-bit) */
    if (size == 1) {
        a = regs[0] & 0xFF;
    }
    {
        const uint64_t sign = 1ull << (size * 8 - 1);
        const int negative = is_signed && (a & sign) != 0;
        const int overflow = is_signed && divisor == mask && a == sign; /* the lowest number divided by -1 */
        if (divisor != 0 && !overflow) {
            return EXCEPTION_CONTINUE_SEARCH; /* some other divide error (a quotient too big): a real crash */
        }
        q = negative ? mask : 0;
        r = (a - q * divisor) & mask;
    }
    if (size == 1) {
        regs[0] = (regs[0] & ~0xFFFFull) | (q & 0xFF) | ((r & 0xFF) << 8); /* AL = quotient, AH = remainder */
    } else if (size == 2) {
        regs[0] = (regs[0] & ~0xFFFFull) | q;
        regs[2] = (regs[2] & ~0xFFFFull) | r;
    } else {
        regs[0] = q; /* a 32-bit result clears the upper half, as the CPU's own would */
        regs[2] = r;
    }
    if (InterlockedIncrement(&s_div_hits) <= 50) {
        fprintf(stderr,
                "[divzero] %s %d-bit division %s at 0x%llx (addr2line): dividend 0x%llx; the GameCube's result: "
                "quotient 0x%llx, remainder 0x%llx\n",
                is_signed ? "signed" : "unsigned", size * 8, divisor == 0 ? "by zero" : "overflow (lowest / -1)",
                (unsigned long long)(exe_preferred_base(module) + (c->Rip - (DWORD64)(uintptr_t)module)),
                (unsigned long long)a, (unsigned long long)q, (unsigned long long)r);
    }
    c->Rip += (DWORD64)i;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* ORRE_TEST_DIVZERO=1: each case divided at start-up, printed next to the expected PowerPC result */
static void divide_self_test(void) {
    volatile int32_t zero = 0, seven = 7, minus7 = -7, lowest = INT32_MIN, minus1 = -1;
    volatile uint32_t uzero = 0, useven = 7;
    volatile int16_t hzero = 0, hseven = 7;
    int32_t q1 = seven / zero, r1 = seven % zero;
    int32_t q2 = minus7 / zero, r2 = minus7 % zero;
    int32_t q3 = lowest / minus1, r3 = lowest % minus1;
    uint32_t q4 = useven / uzero, r4 = useven % uzero;
    int16_t q5 = (int16_t)(hseven / hzero);
    fprintf(stderr, "[divzero] self-test: 7/0 = %d r %d (want 0 r 7); -7/0 = %d r %d (want -1 r -7); "
                    "INT_MIN/-1 = %d r %d (want -1 r 2147483647); 7u/0u = %u r %u (want 0 r 7); short 7/0 = %d (want 0)\n",
            q1, r1, q2, r2, q3, r3, q4, r4, q5);
}

void pc_crash_install(void) {
    const char* div = getenv("ORRE_DIVZERO");
    read_preferred_base(); /* now, not inside a crash: file reads there are best avoided */
    AddVectoredExceptionHandler(1, on_first_chance);
    if (div == NULL || strcmp(div, "0") != 0) {
        AddVectoredExceptionHandler(1, on_divide_error); /* added last, so it runs first */
        if (getenv("ORRE_TEST_DIVZERO") != NULL) {
            divide_self_test();
        }
    }
    SetUnhandledExceptionFilter(on_crash);
}

/* Hardware write-watch (the project's request, golden-slice M15: who writes a thread record's link at Willie's battle start).
 * ORRE_WATCH=<address>[:<length 1/2/4/8>][,...] (up to 4; the address as the game sees it: MEM1 is at its GameCube
 * address, so 0x8049EBF0 is that byte) arms the x86 debug registers DR0-DR3 / DR7 for writes on the game's OS thread
 * (every game fibre, GS threads included, runs on it; writes from other OS threads, audio or Aurora's workers, are not
 * seen). Each hit prints the address, its new value and the call stack of the writing code (the trap comes right after
 * the write), then the game continues; after 20 hits the watch turns itself off. */
static DWORD64 s_watch_addr[4];
static unsigned s_watch_len[4];
static int s_watch_count;
static volatile LONG s_watch_hits;
static HANDLE s_watch_thread;

static LONG WINAPI on_watch(EXCEPTION_POINTERS* info) {
    CONTEXT* ctx = info->ContextRecord;
    DWORD64 dr6 = ctx->Dr6;
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || (dr6 & 0xF) == 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    for (int i = 0; i < s_watch_count; i++) {
        if ((dr6 & (1ull << i)) == 0) {
            continue;
        }
        {
            const LONG hit = InterlockedIncrement(&s_watch_hits);
            DWORD64 value = 0;
            memcpy(&value, (const void*)(uintptr_t)s_watch_addr[i], s_watch_len[i]);
            fprintf(stderr, "\n[watch] write %ld to 0x%llx (%u bytes): now 0x%llx\n", hit,
                    (unsigned long long)s_watch_addr[i], s_watch_len[i], (unsigned long long)value);
            walk_stack(*ctx);
            if (hit >= 20) {
                fprintf(stderr, "[watch] 20 writes seen: watch off\n");
                ctx->Dr7 = 0;
            }
        }
    }
    ctx->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* A thread cannot load its own debug registers reliably; a helper suspends it, sets them, resumes it */
static DWORD WINAPI watch_arm(LPVOID unused) {
    CONTEXT ctx;
    (void)unused;
    if (SuspendThread(s_watch_thread) == (DWORD)-1) {
        return 1;
    }
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(s_watch_thread, &ctx)) {
        DWORD64 dr7 = 0;
        DWORD64* drs[4] = {&ctx.Dr0, &ctx.Dr1, &ctx.Dr2, &ctx.Dr3};
        for (int i = 0; i < s_watch_count; i++) {
            const unsigned len = s_watch_len[i];
            const DWORD64 lenBits = len == 1 ? 0 : len == 2 ? 1 : len == 8 ? 2 : 3;
            *drs[i] = s_watch_addr[i];
            dr7 |= 1ull << (i * 2);                /* local enable */
            dr7 |= 1ull << (16 + i * 4);           /* R/W = 01: data writes */
            dr7 |= lenBits << (18 + i * 4);        /* length */
        }
        ctx.Dr7 = dr7;
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        SetThreadContext(s_watch_thread, &ctx);
    }
    ResumeThread(s_watch_thread);
    return 0;
}

/* Arm one more write-watch at run time, from the game thread (2026-10-21: ORRE_WATCH_SCRIPT arms it on a script's
 * code bytes when that script is registered, since its address is only known then). Same rules as ORRE_WATCH: up to
 * 4 watches, writes from the game's OS thread only. */
void pc_crash_watch_add(unsigned long long addr, unsigned len) {
    HANDLE helper;
    if (s_watch_count >= 4) {
        fprintf(stderr, "[watch] no free watch for 0x%llx\n", addr);
        return;
    }
    if (len != 1 && len != 2 && len != 4 && len != 8) {
        len = 4;
    }
    addr -= addr % len;
    if (s_watch_thread == NULL) {
        AddVectoredExceptionHandler(1, on_watch);
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_watch_thread,
                        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, 0);
    }
    s_watch_addr[s_watch_count] = addr;
    s_watch_len[s_watch_count] = len;
    s_watch_count++;
    fprintf(stderr, "[watch] watching writes to 0x%llx (%u bytes) on the game thread\n", addr, len);
    helper = CreateThread(NULL, 0, watch_arm, NULL, 0, NULL);
    if (helper != NULL) {
        WaitForSingleObject(helper, INFINITE);
        CloseHandle(helper);
    }
}

void pc_crash_watch_install(void) {
    const char* spec = getenv("ORRE_WATCH");
    HANDLE helper;
    if (spec == NULL || *spec == '\0') {
        return;
    }
    while (*spec != '\0' && s_watch_count < 4) {
        char* end;
        unsigned long long addr = strtoull(spec, &end, 0);
        unsigned len = 4;
        if (end == spec) {
            break;
        }
        if (*end == ':') {
            len = (unsigned)strtoul(end + 1, &end, 0);
        }
        if (len != 1 && len != 2 && len != 4 && len != 8) {
            len = 4;
        }
        if (addr % len != 0) { /* the CPU needs an aligned address: widen down to the aligned unit holding it */
            addr -= addr % len;
        }
        s_watch_addr[s_watch_count] = addr;
        s_watch_len[s_watch_count] = len;
        fprintf(stderr, "[watch] watching writes to 0x%llx (%u bytes) on the game thread\n", addr, len);
        s_watch_count++;
        spec = *end == ',' ? end + 1 : end;
    }
    if (s_watch_count == 0) {
        return;
    }
    AddVectoredExceptionHandler(1, on_watch);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_watch_thread,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, 0);
    helper = CreateThread(NULL, 0, watch_arm, NULL, 0, NULL);
    if (helper != NULL) {
        WaitForSingleObject(helper, INFINITE);
        CloseHandle(helper);
    }
}

void pc_crash_print_stack(const char* reason) {
    CONTEXT ctx;

    RtlCaptureContext(&ctx);
    fprintf(stderr, "\n[pc_crash] %s\n", reason);
    walk_stack(ctx);
}

/* Watchdog: a real OS thread that checks a heartbeat once a second. When it stops changing for `seconds`, the thread
 * that started the watchdog (the game's main thread: every game fibre runs on it) is suspended, its current stack is
 * printed (whichever fibre is running), and it is resumed; once per stall. Shows where a hang is without a debugger. */
static volatile const unsigned* s_heartbeat;
static unsigned s_stall_seconds;
static HANDLE s_watched;
static DWORD s_watched_id; /* its thread id, for the all-threads report (report_other_threads) */

static volatile LONG s_sample_requests;

int pc_crash_request_samples(unsigned count) {
    if (s_heartbeat == NULL) {
        return 0;
    }
    InterlockedExchange(&s_sample_requests, (LONG)count);
    return 1;
}

/* One stack sample of the watched thread, taken from the watchdog thread (2026-10-17: Willie's battle stopped at
 * the trainer intro while frames kept coming, so the stall watchdog never fired). */
static void sample_once(unsigned left) {
    CONTEXT ctx;
    DWORD64 pcs[PC_CRASH_MAX_FRAMES];
    int n = 0;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_FULL;
    if (SuspendThread(s_watched) == (DWORD)-1) {
        return;
    }
    if (GetThreadContext(s_watched, &ctx)) {
        n = capture_stack(ctx, pcs); /* nothing that takes a lock while the thread is stopped (capture_stack says why) */
    }
    ResumeThread(s_watched);
    if (n > 0) {
        fprintf(stderr, "\n[pc_crash] stack sample (%u left), heartbeat %u:\n", left, *s_heartbeat);
        print_captured(pcs, n);
    }
}

/* Every other thread's stack at a hang, after the main thread's (2026-11-24, the laptop sitting): the hub froze
 * twice on snatcherLeo's laptop (Intel Iris Xe, Vulkan) right after its window was restored from maximized, and the main
 * thread's stack only showed it waiting (once for Dawn's device lock inside CreateTexture, once for a free frame slot
 * from Aurora's render worker). What it waits on is in another thread (most likely the render worker, inside the
 * driver's GetCurrentTexture / Present), so the report now shows them all, each with its name where it has one
 * (Aurora's "Aurora render worker"). Same rule as the main thread's: each thread is captured while it is stopped,
 * with nothing that takes a lock, and everything is printed after all of them run again (capture_stack says why).
 * The thread list is taken before anything is stopped. At most PC_CRASH_MAX_THREADS threads; the watchdog itself and
 * the main thread (already printed) are skipped.
 * Windows only (Toolhelp's thread list, SuspendThread, GetThreadDescription from Windows 10 1607, looked up at run
 * time so older systems print no names). Linux would need each thread's stack another way (a signal sent to each
 * thread with tgkill, its handler capturing its own stack); macOS, task_threads + thread_get_state (guess, neither
 * written; pc_crash_posix.c prints only the main thread's). */
#define PC_CRASH_MAX_THREADS 64
typedef HRESULT(WINAPI* GetThreadDescriptionFn)(HANDLE, PWSTR*);
static DWORD s_thread_ids[PC_CRASH_MAX_THREADS];
static DWORD64 s_thread_pcs[PC_CRASH_MAX_THREADS][PC_CRASH_MAX_FRAMES];
static int s_thread_frames[PC_CRASH_MAX_THREADS];

static void report_other_threads(void) {
    /* the main thread's id from when the watch started (s_watched_id): GetThreadId on s_watched fails, its handle
     * has no query right, and the main thread was printed twice in the first test (2026-11-24) */
    const DWORD pid = GetCurrentProcessId(), self = GetCurrentThreadId(), main_id = s_watched_id;
    GetThreadDescriptionFn describe =
        (GetThreadDescriptionFn)(void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadDescription");
    THREADENTRY32 te;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    int count = 0, i;
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != self && te.th32ThreadID != main_id &&
                count < PC_CRASH_MAX_THREADS) {
                s_thread_ids[count++] = te.th32ThreadID;
            }
            te.dwSize = sizeof(te);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
    /* capture: one thread stopped at a time, nothing that locks while it is stopped */
    for (i = 0; i < count; i++) {
        HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, s_thread_ids[i]);
        s_thread_frames[i] = 0;
        if (t == NULL) {
            continue;
        }
        if (SuspendThread(t) != (DWORD)-1) {
            CONTEXT ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(t, &ctx)) {
                s_thread_frames[i] = capture_stack(ctx, s_thread_pcs[i]);
            }
            ResumeThread(t);
        }
        CloseHandle(t);
    }
    /* print, everything running again */
    fprintf(stderr, "[pc_crash] hang watch: the other %d thread(s):\n", count);
    for (i = 0; i < count; i++) {
        char name[128] = "";
        int f;
        if (describe != NULL) {
            HANDLE t = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, s_thread_ids[i]);
            PWSTR wide = NULL;
            if (t != NULL) {
                if (SUCCEEDED(describe(t, &wide)) && wide != NULL) {
                    WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, (int)sizeof(name), NULL, NULL);
                    LocalFree(wide);
                }
                CloseHandle(t);
            }
        }
        fprintf(stderr, "[pc_crash] thread %lu%s%s%s:\n", (unsigned long)s_thread_ids[i], name[0] ? " \"" : "", name,
                name[0] ? "\"" : "");
        for (f = 0; f < s_thread_frames[i]; f++) {
            print_address(f == 0 ? "at" : "called from", s_thread_pcs[i][f]);
        }
    }
    fflush(stderr);
}

static DWORD WINAPI watchdog_main(LPVOID unused) {
    unsigned last = *s_heartbeat, still = 0;
    int reported = 0;
    (void)unused;
    for (;;) {
        Sleep(1000);
        if (s_sample_requests > 0) {
            sample_once((unsigned)InterlockedDecrement(&s_sample_requests));
        }
        if (*s_heartbeat != last) {
            last = *s_heartbeat;
            still = 0;
            reported = 0;
            continue;
        }
        if (++still >= s_stall_seconds && !reported) {
            CONTEXT ctx;
            DWORD64 pcs[PC_CRASH_MAX_FRAMES];
            int n = 0;
            reported = 1;
            memset(&ctx, 0, sizeof(ctx));
            ctx.ContextFlags = CONTEXT_FULL;
            /* *Changed 2026-11-22 (Studio's finding):* the stack is only captured while the thread is stopped, and
             * everything is printed after it runs again, so a thread stopped inside the C runtime's stderr lock can't
             * deadlock the watchdog (capture_stack). The report hook (the game's room and frame, Studio's action
             * trail) also runs after the resume: in a real hang the thread is still stuck, so what it reads still
             * holds (likely; before, it ran while the thread was stopped, which carried the same lock danger). */
            if (SuspendThread(s_watched) != (DWORD)-1) {
                if (GetThreadContext(s_watched, &ctx)) {
                    n = capture_stack(ctx, pcs);
                }
                ResumeThread(s_watched);
            }
            if (n > 0) {
                /* "no frame", not "no game frame": since 2026-11-13 it watches the hub and the menus too, for
                 * everyone (pc_health.c); a window held by its title bar or an open message box also stops frames */
                fprintf(stderr, "\n[pc_crash] hang watch: no frame for %u s (heartbeat %u; a held window or an "
                                "open message box does this too); main thread:\n",
                        still, last);
                print_captured(pcs, n);
                report_other_threads();
                if (pc_crash_report_hook != NULL) {
                    pc_crash_report_hook();
                }
            }
        }
    }
    return 0;
}

void pc_crash_watchdog(volatile const unsigned* heartbeat, unsigned seconds) {
    if (s_heartbeat != NULL || heartbeat == NULL) {
        return;
    }
    s_heartbeat = heartbeat;
    s_stall_seconds = seconds ? seconds : 5;
    s_watched_id = GetCurrentThreadId();
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_watched,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, 0);
    CreateThread(NULL, 0, watchdog_main, NULL, 0, NULL);
}
