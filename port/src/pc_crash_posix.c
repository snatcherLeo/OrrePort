/*
 * pc_crash on Linux / macOS: signal handler for fatal signals (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT) that
 * prints the signal, the faulting address and the call stack. See pc_crash.h.
 *
 * The stack comes from backtrace() (glibc and macOS <execinfo.h>); addresses inside the executable are printed
 * relative to its load address when dladdr can tell, so `addr2line -f -e <exe> <address>` (Linux) or
 * `atos -o <exe> <address>` (macOS) gives function names and lines. The handler runs on its own signal stack, so a
 * stack overflow is reported too. Written for the POSIX layer; not yet built or run on Linux / macOS (needs WSL2 or
 * a Mac: docs/port/session-b-status.md).
 */
#define _GNU_SOURCE
#include "pc_crash.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_address(const char* label, void* address) {
    Dl_info info;
    if (dladdr(address, &info) != 0 && info.dli_fname != NULL) {
        const char* base = strrchr(info.dli_fname, '/');
        fprintf(stderr, "  %s %s+0x%llx\n", label, base ? base + 1 : info.dli_fname,
                (unsigned long long)((uintptr_t)address - (uintptr_t)info.dli_fbase));
    } else {
        fprintf(stderr, "  %s %p\n", label, address);
    }
}

void (*pc_crash_report_hook)(void);

static void on_crash(int sig, siginfo_t* info, void* context) {
    void* frames[64];
    int n, i;
    (void)context;
    fprintf(stderr, "\n[pc_crash] signal %d (%s), address %p\n", sig, strsignal(sig), info ? info->si_addr : NULL);
    n = backtrace(frames, 64);
    for (i = 0; i < n; i++) {
        print_address(i == 0 ? "at" : "called from", frames[i]);
    }
    fprintf(stderr, "[pc_crash] decode with: addr2line -f -e <exe> <offsets> (Linux), atos -o <exe> (macOS)\n");
    if (pc_crash_report_hook != NULL) {
        pc_crash_report_hook();
    }
    signal(sig, SIG_DFL); /* then let the default action end the program (core dump where enabled) */
    raise(sig);
}

void pc_crash_install(void) {
    static char alt_stack[64 * 1024];
    static const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    struct sigaction sa;
    stack_t ss;
    size_t i;

    ss.ss_sp = alt_stack;
    ss.ss_size = sizeof(alt_stack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        sigaction(signals[i], &sa, NULL);
    }
}

void pc_crash_print_stack(const char* reason) {
    void* frames[64];
    int n, i;

    fprintf(stderr, "\n[pc_crash] %s\n", reason);
    n = backtrace(frames, 64);
    for (i = 0; i < n; i++) {
        print_address(i == 0 ? "at" : "called from", frames[i]);
    }
}

/* The watchdog needs to suspend another thread and read its registers: not done on POSIX yet (does nothing). */
void pc_crash_watchdog(volatile const unsigned* heartbeat, unsigned seconds) {
    (void)heartbeat;
    (void)seconds;
}

int pc_crash_request_samples(unsigned count) {
    (void)count;
    return 0; /* no watchdog on POSIX yet */
}

/* No minidump on POSIX yet (pc_crash.h): Linux and macOS keep a core file their own way (likely: ulimit -c, systemd's
 * coredumpctl, macOS's crash reporter); the printed stack is in the log either way. */
void pc_crash_set_dump_path(const char* utf8_path) { (void)utf8_path; }

void pc_crash_watch_add(unsigned long long addr, unsigned len) {
    (void)len;
    fprintf(stderr, "[watch] write-watches are Windows-only for now; 0x%llx ignored\n", addr);
}

void pc_crash_watch_install(void) {
    if (getenv("ORRE_WATCH") != NULL) {
        fprintf(stderr, "[watch] ORRE_WATCH is Windows-only for now; ignored\n");
    }
}
