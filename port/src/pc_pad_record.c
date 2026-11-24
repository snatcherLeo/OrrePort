/*
 * pc_pad_record.c: every sitting's controller input, recorded exactly as the game read it (the reference playthrough Part 1 item 3, crash
 * safety, and item 4, the PC-to-Dolphin check, 2026-11-01, for B to review; docs/port/pad-recording.md).
 *
 * Aurora's PADRead calls PADSetReadHook's hook with the four statuses it gives the game, so this file sees each read
 * without touching game code. When the run is not an automated one and has a game folder, each start writes
 * sitting_<YYYYMMDD_HHMMSS>.orrepad in the recordings' folder (pc_paths.h PC_PLACE_RECORDINGS: data\system\recordings\Pokemon
 * Colosseum\ in the portable layout since 2026-11-21, Colosseum\Recordings\ in the flat one). ORRE_PAD_RECORD=0
 * turns it off; ORRE_PAD_RECORD=<file> records to that file in any run (tests).
 *
 * The file (little-endian):
 *   header:  "ORREPAD2" (8 bytes), u32 header size, u32 record size (60), u64 the replay clock's start (OSTime ticks
 *            since 2000-01-01; ORRE_VIRTUAL_EPOCH for a replay; 0 when the run was not on the replay clock), then the
 *            build stamp (zero-terminated). ("ORREPAD1", the first night's test files, had no clock start.)
 *   records: one each time the statuses differ from the previous read (the first read always):
 *            u32 read index (PADRead calls since the start, counting from 0), u32 retrace (VIGetRetraceCount),
 *            u32 game frame (pc_vi_heartbeat, the frame count pad scripts use), then 4 x 12 bytes, one per port: u16 button, s8 stickX, s8 stickY, s8 substickX, s8 substickY,
 *            u8 triggerLeft, u8 triggerRight, u8 analogA, u8 analogB, s8 err, u8 0
 *   A read with no record repeats the previous record's statuses. The file is flushed about once a second, so a crash
 *   loses at most the last second.
 */
#include <dolphin/os/OSTime.h>
#include <dolphin/pad.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_paths.h"    /* the recordings' folder and the one game-folder check (4.1) */
#include "pc_settings.h" /* recordings_keep */

extern u32 VIGetRetraceCount(void);
extern volatile u32 pc_vi_heartbeat; /* pc_vi.c: the game frame count */
extern int pc_vi_virtual_clock(void);  /* pc_vi.c: the replay clock in use */
extern OSTime pc_vi_clock_epoch(void); /* pc_vi.c: its start */

enum { PORTS = 4, PORT_BYTES = 12, HEAD = 12, RECORD_BYTES = HEAD + PORTS * PORT_BYTES, FLUSH_EVERY = 60 };

static FILE* s_file;
static u32 s_reads, s_records, s_since_flush;
static unsigned char s_last[PORTS * PORT_BYTES];
static char s_path[1300];

static void put_u32(unsigned char* p, u32 v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void pack(unsigned char* p, const PADStatus* s) {
    p[0] = (unsigned char)s->button;
    p[1] = (unsigned char)(s->button >> 8);
    p[2] = (unsigned char)s->stickX;
    p[3] = (unsigned char)s->stickY;
    p[4] = (unsigned char)s->substickX;
    p[5] = (unsigned char)s->substickY;
    p[6] = s->triggerLeft;
    p[7] = s->triggerRight;
    p[8] = s->analogA;
    p[9] = s->analogB;
    p[10] = (unsigned char)s->err;
    p[11] = 0;
}

static void on_read(const PADStatus* status) {
    unsigned char rec[RECORD_BYTES];
    const u32 index = s_reads++;
    for (int i = 0; i < PORTS; i++) {
        pack(rec + HEAD + i * PORT_BYTES, &status[i]);
    }
    if (s_file == NULL) {
        return;
    }
    if (index == 0 || memcmp(rec + HEAD, s_last, sizeof(s_last)) != 0) {
        memcpy(s_last, rec + HEAD, sizeof(s_last));
        put_u32(rec, index);
        put_u32(rec + 4, VIGetRetraceCount());
        put_u32(rec + 8, pc_vi_heartbeat);
        fwrite(rec, 1, sizeof(rec), s_file);
        s_records++;
    }
    if (++s_since_flush >= FLUSH_EVERY) {
        s_since_flush = 0;
        fflush(s_file);
    }
}

static int automated(void) {
    const char* window = getenv("ORRE_WINDOW");
    return getenv("ORRE_PAD_SCRIPT") != NULL || (window != NULL && strcmp(window, "minimized") == 0);
}

static void close_recording(void) {
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
        printf("[pad_record] %u reads, %u records: %s\n", (unsigned)s_reads, (unsigned)s_records, s_path);
    }
}

/* How many sittings the recordings' folder keeps (snatcherLeo, 2026-11-21: recordings live beside the logs, "Yes do all of
 * that"): the newest 20 by default, the same number of runs the logs keep (pc_log.c KEEP_LOGS), so every kept log still
 * has its recording and old pairs go together. Report a problem only ever needs the current sitting's (it goes into the
 * zip), so nothing a report needs is lost. settings.ini's recordings_keep = 0 keeps everything (snatcherLeo's own play folder:
 * his the reference playthrough sittings); any other number is the count to keep. Only sitting_*.orrepad and the .rolls beside each are
 * removed, oldest first (the names sort by date and time); recovery folders and other files are left alone. All
 * platforms (SDL's file calls). */
#define RECORDINGS_KEEP_DEFAULT 20

static int compare_names(const void* a, const void* b) { return strcmp(*(char* const*)a, *(char* const*)b); }

/* keeps the newest keep - 1, so the sitting about to start makes keep */
static void prune(const char* folder) {
    const int keep = pc_settings_int("recordings_keep", RECORDINGS_KEEP_DEFAULT);
    int count = 0, i;
    char** names;
    if (keep <= 0) {
        return;
    }
    names = SDL_GlobDirectory(folder, "sitting_*.orrepad", 0, &count);
    if (names == NULL) {
        return;
    }
    if (count >= keep) {
        char** sorted = (char**)SDL_malloc(sizeof(char*) * (size_t)count);
        if (sorted != NULL) {
            memcpy(sorted, names, sizeof(char*) * (size_t)count);
            SDL_qsort(sorted, (size_t)count, sizeof(char*), compare_names);
            for (i = 0; i < count - (keep - 1); i++) {
                char path[1400];
                size_t n;
                SDL_snprintf(path, sizeof(path), "%s%s", folder, sorted[i]);
                SDL_RemovePath(path);
                n = strlen(path);
                if (n > 8 && strcmp(path + n - 8, ".orrepad") == 0) {
                    SDL_snprintf(path + n - 8, sizeof(path) - (n - 8), ".rolls");
                    SDL_RemovePath(path);
                }
            }
            printf("[pad_record] kept the newest %d sittings: %d older removed (settings.ini recordings_keep)\n", keep,
                   count - (keep - 1));
            SDL_free(sorted);
        }
    }
    SDL_free(names);
}

/* Report a problem (pc_settings_overlay.cpp): this sitting's recording file, or "" when nothing is recorded */
const char* pc_pad_record_path(void) { return s_file != NULL ? s_path : ""; }

void pc_pad_record_start(const char* build_stamp) {
    const char* env = getenv("ORRE_PAD_RECORD");
    unsigned char head[24];
    size_t stamp_len;
    u64 epoch;

    if (env != NULL && strcmp(env, "0") == 0) {
        return;
    }
    if (env != NULL && *env != '\0') {
        char dir[1300];
        char* cut;
        SDL_snprintf(s_path, sizeof(s_path), "%s", env);
        SDL_snprintf(dir, sizeof(dir), "%s", env); /* its folder may not exist yet (a fresh portable folder) */
        cut = strrchr(dir, '/');
        if (strrchr(dir, '\\') > cut) {
            cut = strrchr(dir, '\\');
        }
        if (cut != NULL) {
            *cut = '\0';
            SDL_CreateDirectory(dir);
        }
    } else {
        char folder[1100];
        SDL_Time now;
        SDL_DateTime dt;
        if (automated()) {
            return;
        }
        /* *Updated 2026-11-21 (4.1):* the one game-folder check (pc_paths.h), the same answer the clock (pc_vi.c) and
         * the save slots (pc_card.c) get, so a sitting on the replay clock is always recorded */
        if (!pc_paths_game_folder_exists()) {
            return; /* no game folder (a flat test folder without Colosseum\) */
        }
        if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &dt, true)) {
            SDL_zero(dt);
        }
        pc_paths_get(PC_PLACE_RECORDINGS, "", folder, sizeof(folder));
        SDL_CreateDirectory(folder);
        prune(folder);
        SDL_snprintf(s_path, sizeof(s_path), "%ssitting_%04d%02d%02d_%02d%02d%02d.orrepad", folder, dt.year, dt.month,
                     dt.day, dt.hour, dt.minute, dt.second);
    }
    s_file = fopen(s_path, "wb");
    if (s_file == NULL) {
        fprintf(stderr, "[pad_record] cannot create %s; this sitting is not recorded\n", s_path);
        return;
    }
    stamp_len = strlen(build_stamp) + 1;
    epoch = pc_vi_virtual_clock() ? (u64)pc_vi_clock_epoch() : 0;
    memcpy(head, "ORREPAD2", 8);
    put_u32(head + 8, (u32)(sizeof(head) + stamp_len));
    put_u32(head + 12, RECORD_BYTES);
    put_u32(head + 16, (u32)epoch);
    put_u32(head + 20, (u32)(epoch >> 32));
    fwrite(head, 1, sizeof(head), s_file);
    fwrite(build_stamp, 1, stamp_len, s_file);
    fflush(s_file);
    PADSetReadHook(on_read);
    atexit(close_recording);
    printf("[pad_record] recording this sitting's input: %s\n", s_path);
    /* and the battle draws' states beside it, sitting_<...>.rolls (2026-11-02): ORRE_ROLL_RECORD in
     * pc_debug_game.c (read at the game's first frame, after this), for the Dolphin replay that gives the console the
     * PC's battle rolls (the roll injection; run3-guide section 1). Not when set already, or with ORRE_ROLL_SEEDS */
    if (getenv("ORRE_ROLL_RECORD") == NULL && getenv("ORRE_ROLL_SEEDS") == NULL) {
        char rolls[1300];
        const char* dot = strrchr(s_path, '.');
        SDL_snprintf(rolls, sizeof(rolls), "%.*s.rolls", dot != NULL ? (int)(dot - s_path) : (int)strlen(s_path),
                     s_path);
#ifdef _WIN32
        _putenv_s("ORRE_ROLL_RECORD", rolls);
#else
        setenv("ORRE_ROLL_RECORD", rolls, 0);
#endif
    }
}
