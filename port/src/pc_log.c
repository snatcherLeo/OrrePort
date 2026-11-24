/*
 * pc_log.c: an automatic log file for every run (snatcherLeo's request, 2026-10-17): logs/orreport_YYYYMMDD_HHMMSS.log
 * (data\system\logs\ in the portable layout, <exe dir>/logs/ in the flat one: pc_paths.h, 4.1)
 * gets everything the program writes to stdout and stderr (the game's log and OSReport lines, [room] / [battle] lines,
 * the crash report), while the console (or saferun's log) still sees it all. Always on.
 *
 * How: at start-up stdout and stderr are pointed at a pipe; a small reader thread copies each chunk to the original
 * console and to the log file and flushes both. A crash or watchdog report calls pc_log_drain(), which waits until the
 * reader has written everything sent before it (a marker byte through the pipe), so the report is in the file before
 * the process ends. The newest 20
 * logs are kept; older ones in logs/ are deleted (only files named orreport_*.log). Portable: CRT / POSIX pipes, SDL
 * for the thread, the time, the folder and the directory listing.
 */
#include "pc_log.h"
#include "pc_paths.h" /* where the logs go (4.1) */

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_time.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
/* _O_NOINHERIT: a program OrrePort starts (Quit to hub's new run) must not get the pipe's read end; holding it kept the
 * pipe open after we ended, so writes into it blocked instead of failing (Windows only: POSIX pipe() handles are
 * inherited across fork/exec too, but SDL's process creation on Linux / macOS closes the parent's other descriptors,
 * likely; untested) */
#define pipe_(fds) _pipe(fds, 1 << 16, _O_BINARY | _O_NOINHERIT)
#define dup_ _dup
#define dup2_ _dup2
#define read_ _read
#define write_ _write
#else
#include <sys/ioctl.h>
#include <unistd.h>
#define pipe_(fds) pipe(fds)
#define dup_ dup
#define dup2_ dup2
#define read_ read
#define write_ write
#endif

#define KEEP_LOGS 20

static FILE* s_file;
static int s_pipe_read = -1;
static int s_console = -1;
static SDL_Mutex* s_write_lock;
static char s_path[1024];
/* pc_log_drain's marker (2026-11-01, for B to review): the drain writes one 0x1E byte into the pipe and waits
 * until the reader has seen it, so everything written before it is in the file. It replaced a PeekNamedPipe wait that
 * hung: on Windows a call on the pipe's read handle waits behind the reader thread's blocked read of the same handle,
 * so any drain while the pipe was idle never returned (an early start-up failure sat until saferun killed it). */
#define DRAIN_MARK '\x1e'
static SDL_AtomicInt s_drain_seen;

const char* pc_log_path(void) { return s_path[0] != '\0' ? s_path : NULL; }

/* the start-up marks (pc_log.h says why). The clock starts in pc_log_start, the first thing main does. */
static Uint64 s_start_ns;
void pc_startup_mark(const char* what) {
    static int first = 1;
    if (s_start_ns == 0) {
        s_start_ns = SDL_GetTicksNS();
    }
    if (first) {
        first = 0;
#ifdef _WIN32
        /* Windows only: how long the program and its DLLs took to load before main ran (the process's creation time
         * against now, less the time since the log started). Linux / macOS would read the same from /proc/self/stat or
         * sysctl(KERN_PROC); not done yet (it is a nice-to-have; the marks below are platform-neutral). */
        FILETIME created, exited, kernel, user, now;
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
            GetSystemTimeAsFileTime(&now);
            const ULONGLONG c = ((ULONGLONG)created.dwHighDateTime << 32) | created.dwLowDateTime;
            const ULONGLONG n = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
            const double since_launch = (double)(n - c) / 10000.0;
            const double since_log = (double)(SDL_GetTicksNS() - s_start_ns) / 1e6;
            printf("[startup] the program and its DLLs loaded in %.0f ms before main\n", since_launch - since_log);
        }
#endif
    }
    printf("[startup] %5.0f ms: %s\n", (double)(SDL_GetTicksNS() - s_start_ns) / 1e6, what);
}

static int SDLCALL reader_main(void* unused) {
    char buf[4096];
    (void)unused;
    for (;;) {
        int n = read_(s_pipe_read, buf, sizeof(buf));
        int i, kept = 0, marks = 0;
        if (n <= 0) {
            break;
        }
        for (i = 0; i < n; i++) { /* the drain's markers are not log text */
            if (buf[i] == DRAIN_MARK) {
                marks++;
            } else {
                buf[kept++] = buf[i];
            }
        }
        /* the file first, under the lock; the console copy after, outside it. Why: a console write can block (a full
         * pipe nobody reads, a console window in select mode), and while the lock was held through it pc_log_drain
         * waited forever, which froze Quit to hub (2026-11-13) */
        SDL_LockMutex(s_write_lock);
        fwrite(buf, 1, (size_t)kept, s_file);
        fflush(s_file);
        SDL_UnlockMutex(s_write_lock);
        if (s_console >= 0 && kept > 0) {
            (void)write_(s_console, buf, (unsigned)kept);
        }
        if (marks > 0) {
            SDL_AddAtomicInt(&s_drain_seen, marks);
        }
    }
    return 0;
}

void pc_log_drain(void) {
    static SDL_AtomicInt asked;
    int want, i;
    if (s_file == NULL) {
        return;
    }
    fflush(stdout);
    fflush(stderr);
    want = SDL_AddAtomicInt(&asked, 1) + 1; /* this call's marker number */
    {
        const char mark = DRAIN_MARK;
        if (write_(2, &mark, 1) != 1) {
            return;
        }
    }
    for (i = 0; i < 200 && SDL_GetAtomicInt(&s_drain_seen) < want; i++) { /* at most 2 s */
        SDL_Delay(10);
    }
    /* the reader holds the lock while it writes the last chunk to the file; never wait for it without a limit (a stuck
     * reader must not stop the program from ending: at most another 0.5 s) */
    for (i = 0; i < 50; i++) {
        if (SDL_TryLockMutex(s_write_lock)) {
            fflush(s_file);
            SDL_UnlockMutex(s_write_lock);
            return;
        }
        SDL_Delay(10);
    }
}

static int SDLCALL compare_names(const void* a, const void* b) {
    return strcmp(*(char* const*)a, *(char* const*)b);
}

/* ---- "didn't close cleanly" (pc_log.h says what for) ---- */
static char s_prev_unclean[1024];

void pc_log_exit_clean(const char* how) {
    printf("[exit] clean: %s\n", how != NULL ? how : "closed");
    pc_log_drain();
}
const char* pc_log_previous_unclean(void) { return s_prev_unclean[0] ? s_prev_unclean : NULL; }
void pc_log_previous_unclean_clear(void) { s_prev_unclean[0] = '\0'; }

/* the newest log in dir, before this run's is made: did it end with the clean line? Only logs that say "[log] exit
 * line: on" near their top are judged, so the first start after updating from a build without the line isn't taken
 * for a crash. Skipped in a Quit to hub hand-over (the previous OrrePort is still closing as this one starts: its line
 * comes a moment later) */
static void check_previous(const char* dir) {
    int count = 0, i;
    const char* handover = getenv("ORRE_HANDOVER");
    char** names;
    if (handover != NULL && handover[0] == '1') return;
    names = SDL_GlobDirectory(dir, "orreport_*.log", 0, &count);
    if (names == NULL) return;
    if (count > 0) {
        const char* newest = names[0];
        for (i = 1; i < count; i++)
            if (strcmp(names[i], newest) > 0) newest = names[i];
        {
            char path[1024], buf[8193];
            FILE* f;
            SDL_snprintf(path, sizeof(path), "%s%s", dir, newest);
            f = fopen(path, "rb");
            if (f != NULL) {
                size_t n = fread(buf, 1, 4096, f); /* the top: the "exit line: on" mark */
                int judged;
                long size;
                buf[n] = '\0';
                judged = strstr(buf, "[log] exit line: on") != NULL;
                fseek(f, 0, SEEK_END);
                size = ftell(f);
                fseek(f, size > 8192 ? size - 8192 : 0, SEEK_SET); /* the end: the clean line */
                n = fread(buf, 1, 8192, f);
                buf[n] = '\0';
                fclose(f);
                if (judged && strstr(buf, "[exit] clean") == NULL) SDL_snprintf(s_prev_unclean, sizeof(s_prev_unclean), "%s", path);
            }
        }
    }
    SDL_free(names);
}

/* keep the newest KEEP_LOGS - 1 so the new log makes KEEP_LOGS */
static void prune(const char* dir) {
    int count = 0, i;
    char** names = SDL_GlobDirectory(dir, "orreport_*.log", 0, &count);
    if (names == NULL) {
        return;
    }
    if (count >= KEEP_LOGS) {
        char** sorted = (char**)SDL_malloc(sizeof(char*) * (size_t)count);
        if (sorted != NULL) {
            memcpy(sorted, names, sizeof(char*) * (size_t)count);
            SDL_qsort(sorted, (size_t)count, sizeof(char*), compare_names); /* names sort by date and time */
            for (i = 0; i < count - (KEEP_LOGS - 1); i++) {
                char path[1024];
                size_t n;
                SDL_snprintf(path, sizeof(path), "%s%s", dir, sorted[i]);
                SDL_RemovePath(path);
                /* its crash's minidump, if it had one (same name, .dmp; pc_crash_set_dump_path, 2026-11-22) */
                n = SDL_strlen(path);
                if (n > 4) {
                    SDL_memcpy(path + n - 4, ".dmp", 5);
                    SDL_RemovePath(path);
                }
            }
            SDL_free(sorted);
        }
    }
    SDL_free(names);
}

int pc_log_start(const char* first_line) {
    char dir[1024];
    SDL_Time now;
    SDL_DateTime dt;
    int fds[2];

    if (s_file != NULL) {
        return 1;
    }
    s_start_ns = SDL_GetTicksNS(); /* the start-up marks count from here (pc_startup_mark) */
    /* data\system\logs\ in the portable layout, logs\ next to the exe in the flat one (pc_paths.h; 4.1). This is the first
     * path asked for in a run, so it is where the layout is worked out */
    pc_paths_get(PC_PLACE_LOGS, "", dir, sizeof(dir));
    if (!SDL_CreateDirectory(dir)) {
        fprintf(stderr, "[log] cannot create %s: %s\n", dir, SDL_GetError());
        return 0;
    }
    prune(dir);
    check_previous(dir); /* before this run's own log exists */
    if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &dt, true)) {
        SDL_zero(dt);
    }
    SDL_snprintf(s_path, sizeof(s_path), "%sorreport_%04d%02d%02d_%02d%02d%02d.log", dir, dt.year, dt.month, dt.day,
                 dt.hour, dt.minute, dt.second);
    s_file = fopen(s_path, "wb");
    if (s_file == NULL) {
        fprintf(stderr, "[log] cannot create %s\n", s_path);
        s_path[0] = '\0';
        return 0;
    }
    fprintf(s_file, "%s\n", first_line);
    fflush(s_file);

    /* stdout / stderr -> pipe -> reader thread -> console + file */
    fflush(stdout);
    fflush(stderr);
    if (pipe_(fds) != 0) {
        fprintf(stderr, "[log] no pipe; the log file gets only the first line\n");
        return 0;
    }
    s_write_lock = SDL_CreateMutex();
    s_console = dup_(2); /* -1 in a windowed exe started without a console: then only the file gets the log */
#ifdef _WIN32
    /* ORRE_CONSOLE=1: a windowed exe (firstlink.py --windowed, the hub trial) opens a console of its own for testers */
    if (s_console < 0 && getenv("ORRE_CONSOLE") != NULL && AllocConsole()) {
        s_console = _open("CONOUT$", _O_WRONLY | _O_BINARY);
    }
#endif
    s_pipe_read = fds[0];
#ifdef _WIN32
    /* a windowed exe started without a console has no stdout / stderr at all (file number -2): give each a stream
     * first, then point whichever number it got at the pipe (without this the log held only its first line) */
    if (_fileno(stdout) < 0) {
        (void)freopen("NUL", "wb", stdout);
    }
    if (_fileno(stderr) < 0) {
        (void)freopen("NUL", "wb", stderr);
    }
    dup2_(fds[1], _fileno(stdout));
    dup2_(fds[1], _fileno(stderr));
#else
    dup2_(fds[1], 1);
    dup2_(fds[1], 2);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (SDL_CreateThread(reader_main, "orre log", NULL) == NULL) {
        dup2_(s_console, 1);
        dup2_(s_console, 2);
        fprintf(stderr, "[log] no reader thread; logging to the console only\n");
        return 0;
    }
    /* the first line is already in the file (written directly above, so it is there even without the pipe); through
     * the pipe it went into the file a second time (every log started with the build line twice, found in the logging
     * review 2026-11-22), so the console gets its own copy directly */
    if (s_console >= 0) {
        (void)write_(s_console, first_line, (unsigned)strlen(first_line));
        (void)write_(s_console, "\n", 1);
    }
    fprintf(stderr, "[log] this run's log: %s\n", s_path);
    /* this build ends every normal run with "[exit] clean" (pc_log_exit_clean), so the next start may judge this log */
    fprintf(stderr, "[log] exit line: on\n");
    if (s_prev_unclean[0]) fprintf(stderr, "[log] the previous run ended unexpectedly: %s\n", s_prev_unclean);
    return 1;
}
