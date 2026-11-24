/*
 * pc_health.c: OrrePort watching itself (2026-11-13; design inbox "watching the apps themselves").
 *
 * Why: snatcherLeo, after Quit to hub froze in a hand check with nothing in the log: "We will need a memory watch and to ensure
 * this program (and especialy STudio) are monitored, in case those programs themselves ever have issues, as we are also
 * making sure we make a quality app for users to rely on"; "loggers, whatever is needed. SEparate loggers for the app
 * performance specifically". Three parts, all on by default and all local (nothing is sent anywhere: the user owns
 * their data, the project notes):
 *
 * 1. The hang watch. The crash watchdog (pc_crash_win32.c) already printed a stuck thread's stack, but only test runs
 *    turned it on (ORRE_WATCHDOG). It now runs for everyone, watching the main thread's frame counter (pc_vi_alive:
 *    the game's frames, the hub's, the menu's held frames). After 10 s without a frame the log gets the main thread's
 *    stack. Started after the start-up checks, so the first start's whole-disc check (tens of seconds) isn't taken for
 *    a hang. Known false alarms (logged, harmless): a window held by its title bar for 10 s (Windows' move loop), a
 *    message box left open.
 * 2. The performance log, next to each run's log: logs/orreport_<date>_perf.csv, one row a second: where (hub or
 *    game), frames drawn, their average and worst time, and the program's memory. Its own file so it stays small and
 *    easy to graph; Report a problem includes it. The newest 20 are kept, as the logs.
 * 3. The memory watch: every 10 minutes a line in the main log (memory now and its change), and a warning when it grows
 *    by more than 1 GB in 10 minutes or passes 4 GB. Typical use is about 0.6-1.3 GB (saferun's peaks in test runs).
 *
 * Studio (phase 5) is to reuse the same pieces (orreport-plan, Studio's own logs and Report a problem).
 *
 * Platforms: the thread, the timing and the files are SDL, the same everywhere. Reading the program's memory is per
 * OS: Windows GetProcessMemoryInfo (verified), Linux /proc/self/statm (likely; resident pages only, so "private" is
 * left 0), macOS task_info (not written yet: 0 for now). The hang watch prints stacks on Windows only so far
 * (pc_crash_posix.c's watchdog is a no-op; the Linux / Mac ports need one).
 */
#include "pc_health.h"
#include "pc_crash.h"
#include "pc_log.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

extern volatile unsigned int pc_vi_alive;     /* pc_vi.c: the frame counter the hang watch follows */
extern volatile unsigned int pc_vi_heartbeat; /* pc_vi.c: the game's frames (0 while in the hub) */
int pc_platform_automated_run(void);          /* pc_platform.c */

#define HANG_SECONDS 10
#define KEEP_PERF 20
#define MEMORY_NOTE_SECONDS 600

static SDL_Mutex* s_lock;
static Uint64 s_last_frame_ns; /* main thread only */
static unsigned s_frames;      /* under s_lock: this second's frames, */
static Uint64 s_sum_ns;        /* their total time */
static Uint64 s_worst_ns;      /* and the longest */
static char s_perf_path[1100];
static FILE* s_perf;

const char* pc_health_perf_path(void) { return s_perf_path[0] != '\0' ? s_perf_path : NULL; }

void pc_health_frame(void) {
    const Uint64 now = SDL_GetTicksNS();
    if (s_lock == NULL) {
        return; /* not started (pc_health_start) */
    }
    if (s_last_frame_ns != 0) {
        const Uint64 took = now - s_last_frame_ns;
        SDL_LockMutex(s_lock);
        ++s_frames;
        s_sum_ns += took;
        if (took > s_worst_ns) s_worst_ns = took;
        SDL_UnlockMutex(s_lock);
    }
    s_last_frame_ns = now;
}

/* the program's memory in MB: private (what it has committed) and the working set (what is in RAM now); 0 when the OS
 * can't say (see the platforms note above) */
static void memory_mb(double* private_mb, double* resident_mb) {
    *private_mb = 0;
    *resident_mb = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    memset(&pmc, 0, sizeof(pmc));
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        *private_mb = (double)pmc.PrivateUsage / (1024.0 * 1024.0);
        *resident_mb = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
    }
#elif defined(__linux__)
    FILE* f = fopen("/proc/self/statm", "r");
    if (f != NULL) {
        unsigned long size = 0, resident = 0;
        if (fscanf(f, "%lu %lu", &size, &resident) == 2) {
            *resident_mb = (double)resident * (double)sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0);
        }
        fclose(f);
    }
#endif
}

/* keep the newest KEEP_PERF - 1 performance logs, so the new one makes KEEP_PERF (as pc_log.c does for the logs) */
static int SDLCALL compare_names(const void* a, const void* b) { return strcmp(*(char* const*)a, *(char* const*)b); }
static void prune(const char* dir) {
    int count = 0;
    char** names = SDL_GlobDirectory(dir, "orreport_*_perf.csv", 0, &count);
    if (names == NULL) {
        return;
    }
    if (count >= KEEP_PERF) {
        char** sorted = (char**)SDL_malloc(sizeof(char*) * (size_t)count);
        if (sorted != NULL) {
            memcpy(sorted, names, sizeof(char*) * (size_t)count);
            SDL_qsort(sorted, (size_t)count, sizeof(char*), compare_names); /* the names sort by date and time */
            for (int i = 0; i < count - (KEEP_PERF - 1); i++) {
                char path[1200];
                SDL_snprintf(path, sizeof(path), "%s%s", dir, sorted[i]);
                SDL_RemovePath(path);
            }
            SDL_free(sorted);
        }
    }
    SDL_free(names);
}

/* the performance log's thread: one row a second, the memory note every 10 minutes; low priority, it only reads */
static int SDLCALL health_main(void* unused) {
    (void)unused;
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    const Uint64 start = SDL_GetTicks();
    double note_mb = -1;      /* private memory at the last note */
    int warned = 0;
    for (;;) {
        SDL_Delay(1000);
        unsigned frames;
        Uint64 sum, worst;
        SDL_LockMutex(s_lock);
        frames = s_frames;
        sum = s_sum_ns;
        worst = s_worst_ns;
        s_frames = 0;
        s_sum_ns = 0;
        s_worst_ns = 0;
        SDL_UnlockMutex(s_lock);
        double priv, res;
        memory_mb(&priv, &res);
        const unsigned secs = (unsigned)((SDL_GetTicks() - start) / 1000);
        if (s_perf != NULL) {
            fprintf(s_perf, "%u,%s,%u,%.1f,%.1f,%.0f,%.0f\n", secs, pc_vi_heartbeat > 0 ? "game" : "hub", frames,
                    frames > 0 ? (double)sum / frames / 1e6 : 0.0, (double)worst / 1e6, priv, res);
            fflush(s_perf); /* a crash or a hang still leaves every finished row on disk */
        }
        if (secs % MEMORY_NOTE_SECONDS == 0 && secs > 0) {
            const double grew = note_mb >= 0 ? priv - note_mb : 0;
            printf("[health] memory: %.0f MB private, %.0f MB in RAM (%+.0f MB in 10 min)\n", priv, res, grew);
            if (!warned && (priv > 4096 || grew > 1024)) {
                warned = 1;
                printf("[health] WARNING: memory is high or still growing (a leak?); the performance log has the curve: "
                       "%s\n", s_perf_path);
            }
            note_mb = priv;
        }
    }
    return 0;
}

void pc_health_start(void) {
    static int started;
    if (started) {
        return;
    }
    started = 1;
    /* 1. the hang watch: 10 s for everyone; a test run that set ORRE_WATCHDOG already has its own (that call wins) */
    pc_crash_watchdog((volatile const unsigned*)&pc_vi_alive, HANG_SECONDS);
    /* 2. the performance log, named after this run's log */
    const char* log = pc_log_path();
    if (log == NULL) {
        return;
    }
    s_lock = SDL_CreateMutex();
    SDL_strlcpy(s_perf_path, log, sizeof(s_perf_path));
    char* dot = strrchr(s_perf_path, '.');
    if (dot != NULL && SDL_strcasecmp(dot, ".log") == 0) {
        *dot = '\0';
    }
    SDL_strlcat(s_perf_path, "_perf.csv", sizeof(s_perf_path));
    {
        char dir[1100];
        SDL_strlcpy(dir, s_perf_path, sizeof(dir));
        char* slash = strrchr(dir, '/');
        char* back = strrchr(dir, '\\');
        if (back != NULL && (slash == NULL || back > slash)) slash = back;
        if (slash != NULL) {
            slash[1] = '\0';
            prune(dir);
        }
    }
    s_perf = fopen(s_perf_path, "w");
    if (s_perf == NULL) {
        printf("[health] cannot create %s\n", s_perf_path);
        s_perf_path[0] = '\0';
        return;
    }
    fprintf(s_perf, "# OrrePort performance log: one row a second. seconds since the start; where (hub or game); frames "
                    "drawn; their average and worst time (ms); memory (MB): private, in RAM\n");
    fprintf(s_perf, "seconds,where,frames,avg_ms,worst_ms,private_mb,ram_mb\n");
    fflush(s_perf);
    if (SDL_CreateThread(health_main, "orre health", NULL) == NULL) {
        printf("[health] no thread: the performance log stays empty\n");
        return;
    }
    printf("[health] hang watch on (%d s); performance log: %s%s\n", HANG_SECONDS, s_perf_path,
           pc_platform_automated_run() ? " (test run)" : "");
}
