/*
 * pc_crash: print where the program crashed (platform-layer plan, "crash handler").
 *
 * pc_crash_install() sets a handler for fatal exceptions (access violations and the like). On a crash it prints the
 * exception code, the faulting address and the call stack to stderr, every address rebased to the executable's
 * preferred base, so `addr2line -f -e <exe> <address>...` (MSYS2 binutils) turns them into function names and source
 * lines. Windows: pc_crash_win32.c (exception filter + unwinder); Linux / macOS: pc_crash_posix.c (fatal signals +
 * backtrace).
 */
#ifndef PC_CRASH_H
#define PC_CRASH_H

#ifdef __cplusplus
extern "C" {
#endif

void pc_crash_install(void);

/* Where a crash writes its minidump (2026-11-22; logging list item 10, shared with Studio): a UTF-8 path,
 * normally the run's log with ".dmp" for ".log", so the two sit side by side and a report can take both. Without it no
 * dump is written. Windows: MiniDumpWriteDump (dbghelp.dll, part of Windows, loaded here, not linked) with the threads,
 * their stacks and the memory those point at, a few hundred KB to a few MB; Linux / macOS: nothing yet (a core file is
 * the system's own way there; likely). */
void pc_crash_set_dump_path(const char* utf8_path);

/* Prints the current call stack (same format as a crash), for fatal errors that end the program without an exception,
 * such as a fatal Aurora / WebGPU log message. */
void pc_crash_print_stack(const char* reason);

/* Starts a watchdog for the calling thread: when *heartbeat stops changing for `seconds`, that thread's stack is
 * printed once per stall (Windows; nothing yet on POSIX). */
void pc_crash_watchdog(volatile const unsigned* heartbeat, unsigned seconds);

/* Asks the watchdog thread to print the watched thread's stack `count` times, one second apart, without stopping it:
 * where a game that still draws frames but makes no progress is waiting (debug command "stack"). Returns 0 when no
 * watchdog runs (POSIX for now). */
int pc_crash_request_samples(unsigned count);

/* Called once after a crash or watchdog report (the game's thread is stopped or dead): last-chance diagnostics such as
 * ORRE_DUMP_FLAGS (pc_game_main.c). It must only read memory and write files. NULL = nothing. */
extern void (*pc_crash_report_hook)(void);

/* ORRE_WATCH=<address>[:<length>][,...]: hardware write-watch on the calling thread (Windows; a no-op elsewhere for
 * now): each write prints "[watch] write to ..." with the new value and the writer's call stack (pc_crash_win32.c). */
void pc_crash_watch_install(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_CRASH_H */
