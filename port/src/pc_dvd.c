/*
 * pc_dvd.c: DVD completion callbacks delivered the GameCube way, on the game thread (docs/port/sdk-threads.md,
 * "Posted interrupts").
 *
 * Aurora reads the disc on its DVD worker thread and used to call the completion callback there, so game code
 * (gs_dvd.c's _AsyncCallback, the movie and sound loaders) would run in parallel with the game. On the GameCube the
 * callback runs as the DVD interrupt's handler on the one CPU, with interrupts disabled. With the dispatcher hook
 * (an `aurora:` addition, aurora_dvd_set_callback_dispatcher) the worker only posts the call, and the SDK thread
 * scheduler runs it on the game thread at the next point where interrupts are enabled; a thread sleeping until the
 * read is done wakes from there, as on the GameCube.
 */
#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <dolphin/types.h>

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_timer.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_options.h"

extern void pc_os_post_call(void (*fn)(s32 result, void* block), s32 result, void* block);

/* ORRE_FRAME_TIMES (pc_vi.c): disc reads started by the game through DVDRead / DVDReadAsync, their bytes, and async
 * completions delivered, as running totals the per-frame log takes differences of */
volatile u32 pc_dvd_reads_started, pc_dvd_bytes_started, pc_dvd_completions;

/* Console-like disc timing (golden-slice D5 / M38, 2026-10-18): the preset system's "fast_loading" entry off (the
 * GameCube-accurate preset) makes each read finish when the GameCube's drive would, counted in video fields
 * (retraces, 59.94 a second: the frames below):
 *   frames = 0.065 + 0.8 * log2(1 + seek / 1 MB) + length / 82 KB
 * where seek = the distance on the disc from where the previous read ended (the file's disc offset from the FST,
 * Aurora's DVDFastOpen, plus the offset in the file). One read at a time, like the drive: a read starts when it is
 * issued (part-way through a field: pc_vi_retrace_clock) or when the previous one finishes, whichever is later. A read
 * is delivered in the field it finishes in: without a wait if that is the current one; otherwise an async completion
 * comes at that field's retrace and a sync read sleeps until then.
 * *Corrected 2026-10-19:* the first version rounded each read up to whole frames (at least one per read). The
 * capture it was fitted on sees at most one read per frame (see ORRE_DVD_TRACE below), and the console fits about 8
 * of the sound banks' 4 KB reads into a frame (Dolphin loads snd_music.samp's 990 reads in 113 frames), so reads now
 * keep fractional frame times and share frames; 0.065 frames is the per-read cost that stretch implies with the
 * 82 KB/frame rate (likely; a refit on a capture of every read follows).
 * Calibration (likely; Dolphin-calibrated, not a real console yet): fitted on the capture of 579 reads from power-on to
 * Willie's first command menu, Dolphin HLE with "speed up disc transfer" off, measured from outside at one-frame
 * resolution: exact for 510 reads, within one frame for 558, mean error 0.18 frames. Written from those measurements
 * only (Dolphin's disc timing code is GPL: never copied, snatcherLeo).
 * *Updated 2026-10-24 (snatcherLeo's decision, option A):* replays may use Dolphin's drive rules, rewritten here in our own
 * code from their description (no Dolphin code copied): ORRE_DVD_DRIVE=dolphin, the read-ahead drive below. The
 * formula above stays the default. */
static int s_model = -1;
static u32 s_model_last_end; /* disc position where the previous read ended */
static double s_model_free;  /* the retrace (fractional) when the drive finishes its current read */
enum { MODEL_PENDING = 64 };
/* async reads in flight, from issue until the game is handed the completion (2026-10-19: also while the worker
 * holds it, so the read's status stays busy until then: pending_status) */
static struct {
    const struct DVDCommandBlock* block;
    u32 due;                    /* the retrace it completes at (0: as soon as the worker has it, fast loading) */
    double due_frac;            /* how far into that field (0 to 1) */
    double alt_due;             /* ORRE_DVD_SHORT_MODEL: the drive's end (retraces), used once busy-waited; or -1 */
    u32 polls;                  /* status polls while it was still busy (a busy wait polls it continuously) */
    u32 seq;                    /* issue order */
    int has_callback;           /* the game passed a callback (else the worker never dispatches one) */
    AuroraDvdCallback callback; /* from the worker's dispatch */
    s32 result;
    SDL_AtomicInt done; /* the worker has finished the read (and handed over the callback, if any) */
} s_model_pending[MODEL_PENDING];
static SDL_SpinLock s_pending_lock;
static u32 s_pending_seq;

/* Recorded read timings for replays (snatcherLeo, 2026-10-19: "PC replays use Dolphin's measured read-completion times for
 * that recording"): ORRE_DVD_TRACE=<the dvd_reads.csv for that recording or section> (columns start_frame, end_frame,
 * frames, disc_offset, length, ...; the same frame count as the recording's pad steps).
 * The recording is sampled, not complete (verified 2026-10-19): Dolphin's capture looks at the drive once per frame,
 * so it sees at most one read per frame; the console does about 8 of the sound banks' 4 KB reads in a frame, and the
 * first 700 frames have 180 recorded reads where the PC makes 2,924 (the same reads: the PC loads the whole of each
 * bank, as the console does over those frames). So the recorded reads are anchors on Dolphin's timeline:
 * - the first PC read that matches a recorded read (same disc offset and length, searching the next TRACE_LOOKAHEAD
 *   entries, so a skipped or extra read does not derail the rest) fixes the offset between the PC's retrace count
 *   and the recording's frames;
 * - a matched read finishes when it finished in the recording (on the PC's clock), or its recorded duration after
 *   the drive is free if the PC got there later;
 * - a read the recording did not see takes the model's time, but finishes no later than the next recorded read
 *   started (on the console it was done by then: one read at a time).
 * So the timeline snaps back to Dolphin's at every recorded read and can't drift. Unmatched reads are counted per
 * room (pc_dvd_trace_room_entered), so a desynced replay points at a room. Turns console-like timing on by itself (a
 * replay setting, not a preset entry). */
enum { TRACE_LOOKAHEAD = 16 };
typedef struct {
    u32 start, end, offset, length;
    s32 room_frame; /* frames since the capture's room changed when the read started (-1: no such column) */
} TraceRead;
static TraceRead* s_trace;
static int s_trace_count, s_trace_cursor, s_trace_matched, s_trace_missed, s_trace_synced;
static int s_trace_room_matched, s_trace_room_missed, s_trace_room_skipped;
static s32 s_trace_delta; /* PC retrace = recorded frame + delta */
static int s_trace_last; /* how trace_due timed the last read: 0 modelled before the recording, 1 recorded, 2 not on it */
/* A playthrough section's capture starts at a savestate inside a room (the files with room / room_frame columns:
 * S02 starts in the logo room 0x39A) while the PC replays from power-on: matching waits until the PC enters the
 * capture's first room, so no power-on read can be taken for a recorded one. -1 = no room column (frames from
 * power-on): matching starts at once. */
static int s_trace_arm_room = -1, s_trace_armed, s_trace_before;

static void trace_load(void) {
    const char* path = getenv("ORRE_DVD_TRACE");
    FILE* f;
    char line[256];
    int cap = 0;
    if (path == NULL || *path == '\0') {
        return;
    }
    f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "[dvd] ORRE_DVD_TRACE: cannot open '%s': no recorded timings, the model is used\n", path);
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        unsigned start, end, frames, length;
        unsigned long offset;
        if (sscanf(line, "%u,%u,%u,%lx,%u", &start, &end, &frames, &offset, &length) != 5) {
            continue; /* the header, or a malformed line */
        }
        if (s_trace_count == cap) {
            TraceRead* grown;
            cap = cap ? cap * 2 : 1024;
            grown = (TraceRead*)realloc(s_trace, (size_t)cap * sizeof(TraceRead));
            if (grown == NULL) {
                break;
            }
            s_trace = grown;
        }
        (void)frames; /* = end - start */
        if (s_trace_count == 0) {
            /* the 7th column, when there is one: the room the first read started in */
            const char* p = line;
            unsigned room;
            for (int commas = 0; commas < 6 && p != NULL; commas++) {
                p = strchr(p, ',');
                p = p != NULL ? p + 1 : NULL;
            }
            if (p != NULL && sscanf(p, "%u", &room) == 1) {
                s_trace_arm_room = (int)room;
            }
        }
        s_trace[s_trace_count].start = start;
        s_trace[s_trace_count].end = end;
        s_trace[s_trace_count].offset = (u32)offset;
        s_trace[s_trace_count].length = length;
        {
            /* columns 6-8 when present: seek, room, room_frame */
            long seek;
            unsigned room, room_frame;
            unsigned s0, e0, f0, l0;
            unsigned long o0;
            s_trace[s_trace_count].room_frame =
                sscanf(line, "%u,%u,%u,%lx,%u,%ld,%u,%u", &s0, &e0, &f0, &o0, &l0, &seek, &room, &room_frame) == 8
                    ? (s32)room_frame
                    : -1;
        }
        s_trace_count++;
    }
    fclose(f);
    fprintf(stderr, "[dvd] ORRE_DVD_TRACE: %d recorded reads from %s: reads take the recording's measured time\n",
            s_trace_count, path);
    if (s_trace_arm_room >= 0) {
        fprintf(stderr, "[dvd] trace: the recording starts in room 0x%X: reads before the PC enters it are modelled\n",
                s_trace_arm_room);
    } else {
        s_trace_armed = 1;
    }
}

static double model_frames(u32 position, u32 length);
extern u32 VIGetRetraceCount(void);

/* ORRE_DVD_DRIVE=dolphin (2026-10-24; snatcherLeo's option A): the drive as Dolphin emulates it, rewritten from the
 * description of its rules (our own code; the recordings are Dolphin's, so its drive is the reference). Why: after a
 * read the drive keeps reading ahead into a 1 MiB buffer while the game works, so the next read in sequence comes
 * from the buffer almost at once. The formula above has no buffer and made large sequential reads 1-2 frames long
 * where the console takes none (P02's Phenac: a 121 KB read, 0 frames on the console, 1.5 on the PC; with in-field
 * hand-over the next read started a frame late and Wakin's battle went another way). Rules:
 * - the drive reads in 32 KiB blocks; a read costs 600 us, then per block: from the buffer at 32 MiB/s; else, at the
 *   head's position, the raw rate (2.1 MiB/s at the inner edge to 3.325 MiB/s at 38 mm, linear in the radius); else a
 *   seek (|r1 - r2| * 50 + 35 ms under 1 mm, * 4.5 + 75 ms beyond) and the turn until the block comes round
 *   (28.5 turns a second; a block's angle from its radius over the 0.74 um track pitch);
 * - the radius of a disc offset: sqrt(offset / 0x118240000 * (58 mm^2 - 24 mm^2) + 24 mm^2);
 * - afterwards (unless the read stayed in the buffer's first two blocks) the drive reads ahead from the end of the read
 *   (or the buffer's end if that is further) to 1 MiB - 64 KiB past it (at least 1 MiB past the start after a seek),
 *   at the raw rate, starting when the read ends; data counts as buffered once the read-ahead has reached it (whole
 *   blocks); the buffer holds the last 1 MiB, and a read before it empties it (the head stays where it was).
 * The disc's turn is in phase with the PC's clock, not the console's (an unknown offset), so seeks can differ by up to
 * one turn (35 ms, two fields); the recording times most seeks itself (header reads). Time base: fields of 1001/60000 s. */
static int s_drive = -1;
static u64 s_ra_start_off, s_ra_end_off; /* the read-ahead's range on the disc */
static double s_ra_start_t, s_ra_end_t;  /* when it starts and ends (seconds); equal: no buffer */
static int s_ra_updated;                 /* the last drive_read set a new read-ahead */
#define DRIVE_ECC 32768ull
#define DRIVE_MIB 1048576ull
#define FIELD_SECONDS (1001.0 / 60000.0)

static int drive_on(void) {
    if (s_drive < 0) {
        const char* env = getenv("ORRE_DVD_DRIVE");
        s_drive = env != NULL && strcmp(env, "dolphin") == 0;
        if (s_drive) {
            fprintf(stderr, "[dvd] ORRE_DVD_DRIVE=dolphin: reads the recording didn't time follow the read-ahead drive\n");
        }
    }
    return s_drive;
}
static double drive_radius(u64 off) {
    const double layer = (double)0x118240000ull;
    double o = fmod((double)off, 2.0 * layer);
    if (o > layer) {
        o = 2.0 * layer - o;
    }
    return sqrt(o / layer * (0.058 * 0.058 - 0.024 * 0.024) + 0.024 * 0.024);
}
static double drive_seek_s(u64 from, u64 to) {
    const double d = fabs(drive_radius(from) - drive_radius(to));
    return d < 0.001 ? d * 50.0 + 0.035 : d * 4.5 + 0.075;
}
/* Removed 2026-10-26 (snatcherLeo: PC time; the project notes): ORRE_DVD_TURN / _TURN_PHASE, the disc's
 * turn from whole seconds of the console's clock (neutral on P01, worse with =2). The turn runs on the PC's clock */
static double drive_turn_s(u64 off, double t) {
    const double target = fmod(drive_radius(off) / 0.00000074, 1.0);
    const double now = fmod(t * 28.5, 1.0);
    return fmod(target + 1.0 - now, 1.0) / 28.5;
}
static double drive_raw_s(u64 off, u64 len) {
    const double r = drive_radius(off + len / 2);
    const double inner = 1024.0 * 1024.0 * 2.1, outer = 1024.0 * 1024.0 * 3.325;
    return (double)len / ((r - 0.024) / (0.038 - 0.024) * (outer - inner) + inner);
}
/* a read issued to the drive at `now_s` (seconds): how long it takes (seconds); updates the read-ahead */
static double drive_read(u64 offset, u64 length, double now_s) {
    u64 block = offset & ~(DRIVE_ECC - 1);
    const u64 first = block;
    u64 buf_start = 0, buf_end = 0, head = 0;
    u64 pos = offset, left = length;
    double t = 600e-6;
    int seek = 0;
    if (s_ra_start_t != s_ra_end_t) {
        buf_start = s_ra_end_off > DRIVE_MIB ? s_ra_end_off - DRIVE_MIB : 0;
        if (now_s >= s_ra_end_t) {
            buf_end = s_ra_end_off;
        } else if (now_s <= s_ra_start_t) {
            buf_end = s_ra_start_off;
        } else {
            const double done = (now_s - s_ra_start_t) * (double)(s_ra_end_off - s_ra_start_off) /
                                (s_ra_end_t - s_ra_start_t);
            buf_end = s_ra_start_off + ((u64)done & ~(DRIVE_ECC - 1));
        }
        head = buf_end;
        if (block < buf_start) {
            buf_start = buf_end = 0;
        }
    }
    do {
        u64 chunk = ((pos + 1 + DRIVE_ECC - 1) & ~(DRIVE_ECC - 1)) - pos;
        if (chunk > left) {
            chunk = left;
        }
        if (block >= buf_start && block < buf_end) {
            t += (double)chunk / (32.0 * 1024.0 * 1024.0);
        } else {
            if (block != head) {
                seek = 1;
                t += drive_seek_s(head, block);
                t += drive_turn_s(block, now_s + t);
            } else {
                t += drive_raw_s(block, DRIVE_ECC);
            }
            head = block + DRIVE_ECC;
        }
        pos += chunk;
        left -= chunk;
        block += DRIVE_ECC;
    } while (left > 0);
    s_ra_updated = 0;
    if (!(block - buf_start <= 2 * DRIVE_ECC && buf_start != buf_end)) {
        const u64 start = block >= buf_end ? block : buf_end;
        u64 end = block + DRIVE_MIB - 2 * DRIVE_ECC;
        if (seek && end < first + DRIVE_MIB) {
            end = first + DRIVE_MIB;
        }
        s_ra_start_off = start;
        s_ra_end_off = end;
        s_ra_start_t = now_s + t;
        s_ra_end_t = s_ra_start_t + drive_raw_s(start, end - start);
        s_ra_updated = 1;
    }
    return t;
}
static double s_drive_frames = -1.0; /* the drive's time for the read model_issue is timing (fields), or -1 */
static double s_short_alt = -1.0;    /* ORRE_DVD_SHORT_MODEL: the drive's end for a short recorded read, or -1 */

/* when the drive finishes this read under the recording (a fractional retrace count); start = when the drive is free
 * for it */
static double trace_due(double start, u32 position, u32 length) {
    /* the retrace this read starts in, for the recording's clock offset (ORRE_DVD_SUBFIELD's floor() for in-field
     * starts was removed 2026-11-01, snatcherLeo's OK; timing-knobs-cleanup.md) */
    const double clock_start = ceil(start);
    s_trace_last = 0;
    if (!s_trace_armed) {
        s_trace_before++;
        return start + (s_drive_frames >= 0.0 ? s_drive_frames : model_frames(position, length));
    }
    /* Until the first match the whole capture is searched (corrected 2026-10-21, verified): P02's capture starts
     * in room 0x384 with sound-bank reads the PC had already made before it entered that room, so with a 16-entry
     * window from entry 0 the PC never matched one read in the whole run (all 794 recorded reads unused, every read
     * modelled) and its battles could not line up with Dolphin's */
    const int lookahead = s_trace_matched == 0 ? s_trace_count : TRACE_LOOKAHEAD;
    for (int i = s_trace_cursor; i < s_trace_count && i < s_trace_cursor + lookahead; i++) {
        if (s_trace[i].offset == position && s_trace[i].length == length) {
            const TraceRead* r = &s_trace[i];
            /* A match the recording started far ahead of the PC's clock is not this read (2026-10-20, from the
             * per-read log of P01): in Willie's battle the PC read wzx_monsterball_out at battle +427, the recording
             * had that read only at +902, and "the recorded end time wins" held the drive 509 frames, so the whole
             * battle waited and the replay's presses fell out of step. Such a read is taken as one the recording
             * didn't see (the model's time), and the recorded entry stays for the read that really comes then. The
             * limit is 60 frames (the replays' drift is under 30; its ORRE_DVD_TRACE_AHEAD knob removed 2026-11-01) */
            enum { AHEAD_MAX = 60 };
            if (s_trace_synced && (double)((s32)r->start + s_trace_delta) > start + (double)AHEAD_MAX) {
                break; /* the entries are in start order: every later candidate is further ahead */
            }
            const double after_start = start + (double)(r->end - r->start);
            double on_timeline;
            if (!s_trace_synced && s_trace_matched > 0 &&
                abs(((s32)clock_start - (s32)r->start) - s_trace_delta) > 600) {
                /* A room's first match far from the previous room's offset is not this read (2026-10-21,
                 * verified): P03's bag visit inside Miror B.'s battle matched a read the recording made 1,700 frames
                 * later (offset -1724 after +1277), every read of the room then finished at once and the game hung.
                 * The replays drift by tens of frames between rooms, never by thousands; such a read is taken as one
                 * the recording didn't see */
                break;
            }
            if (!s_trace_synced) {
                s_trace_synced = 1;
                s_trace_delta = (s32)clock_start - (s32)r->start;
                fprintf(stderr, "[dvd] trace: first recorded read (entry %d, frame %u) at retrace %.0f: the PC's clock "
                                "is the recording's %+d\n",
                        i, r->start, clock_start, (int)s_trace_delta);
            }
            if (i > s_trace_cursor) {
                s_trace_room_skipped += i - s_trace_cursor;
            }
            s_trace_cursor = i + 1;
            s_trace_last = 1;
            s_trace_matched++;
            s_trace_room_matched++;
            {
                on_timeline = (double)((s32)r->end + s_trace_delta);
                /* Removed 2026-10-26 (snatcherLeo: PC time; the project notes), each measured and off:
                 * ORRE_DVD_TRACE_MIN_ANCHOR (moving the anchor down to an earlier issue), ORRE_DVD_TRACE_PER_READ
                 * (durations from the PC's issue time), ORRE_DVD_TURN=2 (the drive's end within a field of the
                 * record) and ORRE_DVD_FRAME_ROUND (completions inside a long frame at its start). Removed 2026-11-01
                 * (snatcherLeo's OK, the same page; no final replay used them): ORRE_DVD_TRACE_SHIFT (recorded ends + N
                 * frames), ORRE_DVD_TRACE_AHEAD (now fixed at 60), ORRE_DVD_ANCHOR=room (each room's clock from its
                 * start) and ORRE_DVD_SUBFIELD (completions inside a field) */
                {
                    /* ORRE_DVD_READ_FIX=ROW:DELTA,... (2026-10-25; snatcherLeo's option C, the resync at a timing-only
                     * split): the recorded read on trace row ROW (0-based, the "recording entry" of the trace log) ends
                     * DELTA retraces later (negative: earlier). For a split the ledger has traced to one read's end
                     * inside its refresh, which the per-frame recording can't give (P01: Return's archive header in
                     * Folly's battle, the console's loader saw it a pass earlier). The fix list is per sitting. Since
                     * 2026-10-26 (snatcherLeo: PC time; the CPU clock in docs/port/gamecube-cpu-clock.md shelved) this
                     * resync is how such a split is bridged for good */
                    static int fix_loaded;
                    static int fix_row[64], fix_delta[64], fix_n;
                    if (!fix_loaded) {
                        const char* env = getenv("ORRE_DVD_READ_FIX");
                        fix_loaded = 1;
                        while (env != NULL && *env != '\0' && fix_n < 64) {
                            int row, delta, used = 0;
                            if (sscanf(env, "%d:%d%n", &row, &delta, &used) != 2) {
                                break;
                            }
                            fix_row[fix_n] = row;
                            fix_delta[fix_n++] = delta;
                            env += used;
                            if (*env == ',') {
                                env++;
                            }
                        }
                    }
                    for (int k = 0; k < fix_n; k++) {
                        if (fix_row[k] == i) {
                            on_timeline += (double)fix_delta[k];
                            fprintf(stderr, "[dvd] ORRE_DVD_READ_FIX: trace row %d ends %+d retrace(s) off its record\n",
                                    i, fix_delta[k]);
                        }
                    }
                }
            }
            {
                /* ORRE_DVD_FRAMES=<file> (2026-10-20; snatcherLeo's decision (b), "replay disc completions by game
                 * frame"): for a recorded read the file maps (tools/local/testing/dvd_frames.py), the completion is
                 * handed over at the start of the game frame that first saw it on the console (the archive loader's
                 * reaction there), on this room's recorded timeline, instead of at a retrace. The console's processor
                 * is busy for part of each frame, so the frame that sees a read depends on how far that frame's work
                 * had got; no retrace rule reproduces it (the project notes, the exact-battle decision) */
                static int s_map_loaded;
                static s32* s_map_line;
                static u32* s_map_frame;
                static s32* s_map_label; /* the seen frame's label on the recording's clock (dvdseen_labels.py), -1 */
                if (!s_map_loaded) {
                    const char* path = getenv("ORRE_DVD_FRAMES");
                    FILE* f = path != NULL && *path != '\0' ? fopen(path, "r") : NULL;
                    char line[256];
                    int n = 0;
                    s_map_loaded = 1;
                    if (f != NULL) {
                        s_map_line = (s32*)malloc(sizeof(s32) * (size_t)s_trace_count);
                        s_map_frame = (u32*)malloc(sizeof(u32) * (size_t)s_trace_count);
                        for (int j = 0; s_map_line != NULL && j < s_trace_count; j++) {
                            s_map_line[j] = -1;
                        }
                        s_map_label = (s32*)malloc(sizeof(s32) * (size_t)s_trace_count);
                        while (s_map_line != NULL && s_map_frame != NULL && s_map_label != NULL &&
                               fgets(line, sizeof(line), f) != NULL) {
                            int idx, li, got;
                            unsigned long fr;
                            long lab = -1;
                            got = line[0] != '#' ? sscanf(line, "%d %d %lu %ld", &idx, &li, &fr, &lab) : 0;
                            if (got >= 3 && idx >= 0 && idx < s_trace_count) {
                                s_map_line[idx] = li;
                                s_map_frame[idx] = (u32)fr;
                                s_map_label[idx] = got == 4 ? (s32)lab : -1;
                                n++;
                            }
                        }
                        fclose(f);
                        fprintf(stderr, "[dvd] ORRE_DVD_FRAMES: %d recorded reads complete by game frame (%s)\n", n,
                                path);
                    }
                }
                if (s_map_line != NULL && s_map_line[i] >= 0 && s_map_label != NULL && s_map_label[i] >= 0) {
                    /* the label on the recording's clock, mapped like the trace's own times (+ s_trace_delta): the
                     * PC's room entries can sit up to ~100 frames off Dolphin's room change, so the room-relative
                     * position below put P01's 0x3A1 reads ~560 frames late. Only ever a delay past the recorded end
                     * (P01: 236 of 261 seen reads were seen in the retrace they ended, 25 one to four later, most in
                     * the battles) */
                    double t = (double)(s_map_label[i] + s_trace_delta);
                    if (t < on_timeline) {
                        t = on_timeline;
                    }
                    return t > start ? t : start; /* (the match was counted above) */
                }
                if (s_map_line != NULL && s_map_line[i] >= 0) {
                    extern int pc_frame_times_start_of(int line, unsigned long frame, u32* out);
                    extern int pc_frame_times_frame_of_vi(int line, unsigned long vi_off, unsigned long* frame);
                    unsigned long fr;
                    u32 at;
                    /* the map's third column is retraces (probe labels) since the room's entry: the game frame that
                     * contains it, then that frame's start on the PC */
                    if (pc_frame_times_frame_of_vi(s_map_line[i], s_map_frame[i], &fr) &&
                        pc_frame_times_start_of(s_map_line[i], fr, &at)) {
                        /* only ever later than the recorded end: when the PC's room runs a few frames ahead, the
                         * seen frame can come before it, and handing reads over early compounded (P01: the random
                         * state parted at room entry 6, rooms -102 frames) */
                        double t = (double)at > on_timeline ? (double)at : on_timeline;
                        return t > start ? t : start; /* (the match was counted above) */
                    }
                }
            }
            {
                /* ORRE_DVD_SHORT_MODEL=1 (2026-10-25, a test knob, with ORRE_DVD_DRIVE=dolphin): a recorded read
                 * that lasted at most one frame on the recording ended somewhere inside that frame, and the per-frame
                 * capture can't tell where; when the drive says it ends inside the field, its time is used. P01's
                 * Folly battle: three busy-waited table-of-contents reads recorded as one frame long (the console's
                 * full capture: none) held the frame across a retrace each */
                static int short_model = -1;
                if (short_model < 0) {
                    const char* env = getenv("ORRE_DVD_SHORT_MODEL");
                    short_model = env != NULL ? atoi(env) : 0;
                }
                /* Corrected the same night: only for a read the game busy-waits on (pending_status counts the polls;
                 * virtual_deliver hands it over at the drive's time once it is polled continuously). Used for every
                 * short read, P03's Phenac load (a chain of small reads, one per frame) ran 5 frames long */
                s_short_alt = short_model && s_drive_frames >= 0.0 && s_drive_frames < 1.0 && r->end - r->start <= 1 &&
                                      start + s_drive_frames < on_timeline
                                  ? start + s_drive_frames
                                  : -1.0;
                if (short_model == 2 && s_short_alt >= 0.0) {
                    return s_short_alt; /* =2: every short recorded read (the first version), for comparison runs */
                }
            }
            /* The recording's end time wins, even when the PC issued this read later than the console did (
             * 2026-10-19): the PC hands a finished read to the game only at a retrace, so it starts the next read about
             * one frame later than the console, and with "start + duration" as a floor those frames piled up over a
             * room's load (S02's Phenac: 31 reads, ~27 frames late, Wes got control late). The recording's durations
             * are whole frames from per-frame polling anyway. A read the PC issues after its recorded end finishes at
             * once. */
            (void)after_start;
            return on_timeline > start ? on_timeline : start;
        }
    }
    s_trace_last = 2;
    s_trace_missed++;
    s_trace_room_missed++;
    /* Removed 2026-10-26 (snatcherLeo: PC time; the project notes): ORRE_DVD_DURATIONS, the console's own
     * durations for unseen reads from the dvdx capture (superseded by ORRE_DVD_SUBFIELD) */
    {
        double due = start + (s_drive_frames >= 0.0 ? s_drive_frames : model_frames(position, length));
        /* ORRE_DVD_UNRECORDED_MIN=F (2026-10-19, a test knob): a read the recording didn't see takes at least F
         * frames. S02's battle: each such read in a chain cost the console about one frame more than the PC's model
         * (Dolphin: recorded end 29, two hops through an unseen 256-byte read, next start 33; the PC: 31 -> 33), and
         * three of them just before the opponents are created put pokemonCreate a frame early */
        static double unrecorded_min = -1.0;
        if (unrecorded_min < 0.0) {
            const char* env = getenv("ORRE_DVD_UNRECORDED_MIN");
            unrecorded_min = env != NULL ? atof(env) : 0.0;
        }
        if (due < start + unrecorded_min) {
            due = start + unrecorded_min;
        }
        if (s_trace_synced && s_trace_cursor < s_trace_count) {
            const double next_start = (double)((s32)s_trace[s_trace_cursor].start + s_trace_delta);
            if (due > next_start) {
                due = next_start > start ? next_start : start;
            }
        }
        return due;
    }
}

/* pc_pad_script.c: a room was entered; with a recording, how this room's reads went ("count unmatched reads
 * per room in the log so a desync points at a room") */
void pc_dvd_trace_room_entered(unsigned room) {
    if (s_trace_count == 0) {
        return;
    }
    if (!s_trace_armed) {
        if ((int)room != s_trace_arm_room) {
            return;
        }
        s_trace_armed = 1;
        fprintf(stderr, "[dvd] trace: room 0x%X entered at retrace %u: the recording starts here (%d reads before it, "
                        "modelled)\n",
                room, VIGetRetraceCount(), s_trace_before);
        return;
    }
    fprintf(stderr, "[dvd] trace: before room 0x%X: %d reads on the recording, %d not seen by it (modelled), %d "
                    "recorded reads skipped; recording entry %d of %d, clock offset %+d\n",
            room, s_trace_room_matched, s_trace_room_missed, s_trace_room_skipped, s_trace_cursor, s_trace_count,
            (int)s_trace_delta);
    s_trace_room_matched = s_trace_room_missed = s_trace_room_skipped = 0;
    /* Re-anchor the recording's clock at this room's first recorded read (2026-10-19). The pad script times the
     * presses from each room's entry, so the loads must keep the recording's timing from the room's start too: with
     * one clock from power-on, a PC that reached a room 39 frames early (S02's Phenac) waited those 39 frames inside
     * the room's load, Wes got control late and the presses made while the console was already walking were lost. */
    s_trace_synced = 0;
}

/* *Changed 2026-11-20 (found by the 3.7 research):* the choice was read once per run, so the Settings row "Fast
 * loading" (marked live) only took effect after a restart. Now a replay (a disc trace) still fixes it for the whole run,
 * and otherwise it follows the option at every read, so a change applies from the next disc read. Every OS the same. */
static int model_on(void) {
    int on;
    if (s_model < 0) {
        trace_load();
    }
    if (s_trace_count > 0) {
        s_model = 1;
        return 1;
    }
    on = !pc_option("fast_loading");
    if (on != s_model) {
        if (on) {
            fprintf(stderr, "[dvd] console-like disc timing on (fast loading off): reads take the drive's time, "
                            "frames = 0.065 + 0.8 log2(1 + seek / 1 MB) + length / 82 KB, one read at a time\n");
        } else if (s_model > 0) {
            fprintf(stderr, "[dvd] fast loading on: disc reads finish at once (from the next read)\n");
        }
        s_model = on;
    }
    return s_model;
}

/* pc_card_timing.c: card operations take the console's time whenever disc reads do (replays, accurate mode) */
int pc_dvd_console_timing(void) { return model_on(); }
int pc_dvd_replaying(void) { return model_on() && s_trace_count > 0; }

static double model_frames(u32 position, u32 length) {
    const u32 seek = position > s_model_last_end ? position - s_model_last_end : s_model_last_end - position;
    return 0.065 + 0.8 * log2(1.0 + seek / 1048576.0) + length / (82.0 * 1024.0);
}

extern volatile u32 pc_vi_heartbeat; /* pc_vi.c: the game frame count */
/* The model's clock is the retrace count (pc_vi.c, VIGetRetraceCount), not the game frame count. Corrected
 * 2026-10-19: counting game frames hung the PC at start-up (the watchdog at game frame 20): the game waits for a read
 * before it finishes its next frame, so a completion held until "frame N" waited for a frame that was waiting for it.
 * Retraces keep coming while the game waits (the retrace interrupt runs at the game thread's interrupt points, as the
 * GameCube's video interrupt keeps firing while a thread waits for the drive). */
extern u32 VIGetRetraceCount(void);
#define MODEL_HOLD_CAP_NS 2000000000ull /* never hold one completion longer than 2 s of real time (a safety net) */

static double s_issue_frac; /* model_issue's last read: how far into its field it ends */
static double s_issue_alt = -1.0; /* model_issue's last read: ORRE_DVD_SHORT_MODEL's end, or -1 */

/* a read is issued: when the drive will finish it (a retrace count) */
static u32 model_issue(u32 position, u32 length) {
    extern double pc_vi_retrace_clock(void);
    const double now = pc_vi_retrace_clock();
    const double start = s_model_free > now ? s_model_free : now;
    /* ORRE_DVD_DRIVE=dolphin: the drive's own time for this read; a recorded read keeps its recorded end, and the
     * drive's read-ahead then starts from that end */
    s_drive_frames = drive_on() ? drive_read(position, length, start * FIELD_SECONDS) / FIELD_SECONDS : -1.0;
    s_short_alt = -1.0;
    const double due = s_trace_count > 0 ? trace_due(start, position, length)
                                         : start + (s_drive_frames >= 0.0 ? s_drive_frames : model_frames(position, length));
    s_issue_alt = s_short_alt;
    if (s_drive_frames >= 0.0 && s_ra_updated) {
        const double shift = due * FIELD_SECONDS - s_ra_start_t;
        s_ra_start_t += shift;
        s_ra_end_t += shift;
    }
    const u32 field = (u32)floor(due);
    /* ORRE_DVD_TRACE_LOG=1 (replays): every read with when it was issued, when the drive was free for it, when it
     * finishes and how that was timed, so a room that loads late shows which read held the drive */
    static int s_log = -1;
    if (s_log < 0) {
        const char* env = getenv("ORRE_DVD_TRACE_LOG");
        s_log = env != NULL && *env == '1';
    }
    if (s_log && s_trace_count > 0) {
        static const char* const how[] = {"modelled (before the recording)", "recorded", "not on the recording"};
        fprintf(stderr, "[dvd] read 0x%08X+%u issued %.2f, drive free %.2f, done %.2f: %s", (unsigned)position,
                (unsigned)length, now, start, due, how[s_trace_last]);
        if (s_drive_frames >= 0.0) {
            fprintf(stderr, " (drive %.2f%s)", s_drive_frames, s_issue_alt >= 0.0 ? ", short" : "");
        }
        fprintf(stderr, "\n");
    }
    {
        /* ORRE_FSYS_LOG=2: the loader's slots as this read is issued (src/game/pc_debug_game.c) */
        extern void pc_fsys_log_now(const char* why) __attribute__((weak));
        if (pc_fsys_log_now != NULL) {
            char why[64];
            snprintf(why, sizeof(why), "issue 0x%08X+%u", (unsigned)position, (unsigned)length);
            pc_fsys_log_now(why);
        }
    }
    {
        /* ORRE_DVD_STACK=<disc offset, hex> (2026-10-21; observes only): the call stack of every read at that
         * offset, to find who asks for a load the console doesn't make (P01: wzx_monsterball_out in Willie's battle) */
        static long long s_stack_at = -2;
        if (s_stack_at == -2) {
            const char* env = getenv("ORRE_DVD_STACK");
            s_stack_at = env != NULL && *env != '\0' ? (long long)strtoull(env, NULL, 16) : -1;
        }
        if (s_stack_at >= 0 && (long long)position == s_stack_at) {
            extern void pc_crash_print_stack(const char* reason);
            pc_crash_print_stack("ORRE_DVD_STACK: a read at the watched disc offset");
        }
    }
    s_model_free = due;
    s_model_last_end = position + length;
    s_issue_frac = due - floor(due);
    /* delivered in the field the read finishes in: a read that ends inside the current field is done without a wait
     * (corrected 2026-10-19: rounding up to the next retrace made each of the sound banks' 4 KB sync reads cost a whole
     * field, 990 fields for snd_music.samp where Dolphin takes 113) */
    return field > 0 ? field : 1;
}

static void model_remember(const void* block, u32 due, double frac, int has_callback) {
    SDL_LockSpinlock(&s_pending_lock);
    for (int i = 0; i < MODEL_PENDING; i++) {
        if (s_model_pending[i].block == NULL || s_model_pending[i].block == block) {
            s_model_pending[i].block = (const struct DVDCommandBlock*)block;
            s_model_pending[i].due = due;
            s_model_pending[i].due_frac = frac;
            s_model_pending[i].alt_due = s_issue_alt;
            s_model_pending[i].polls = 0;
            s_model_pending[i].seq = ++s_pending_seq;
            s_model_pending[i].has_callback = has_callback;
            s_model_pending[i].callback = NULL;
            s_model_pending[i].result = 0;
            SDL_SetAtomicInt(&s_model_pending[i].done, 0);
            SDL_UnlockSpinlock(&s_pending_lock);
            return;
        }
    }
    SDL_UnlockSpinlock(&s_pending_lock);
    fprintf(stderr, "[dvd] more than %d reads in flight: one is delivered without its modelled time\n", MODEL_PENDING);
}

static int pending_find(const void* block) {
    for (int i = 0; i < MODEL_PENDING; i++) {
        if (s_model_pending[i].block != NULL && s_model_pending[i].block == block) {
            return i;
        }
    }
    return -1;
}

/* DVDGetCommandBlockStatus (Aurora's status hook): a read the game hasn't been handed yet is still busy, as on the
 * GameCube, where the state changes in the interrupt that runs the callback. Corrected 2026-10-19: the worker marked
 * a read done as soon as it had the bytes, so fsys's slot state machine (which polls the status each frame) saw it
 * finish before its modelled or recorded time, and at a moment set by the host's speed. */
static s32 pending_status(const struct DVDCommandBlock* block, s32 state) {
    int busy;
    SDL_LockSpinlock(&s_pending_lock);
    {
        const int i = pending_find(block);
        busy = i >= 0;
        if (busy) {
            s_model_pending[i].polls++; /* ORRE_DVD_SHORT_MODEL: a busy wait polls continuously */
        }
    }
    SDL_UnlockSpinlock(&s_pending_lock);
    return busy && state == DVD_STATE_END ? DVD_STATE_BUSY : state;
}

/* on Aurora's DVD worker (never a game thread): wait until the retrace count reaches `due`, at most MODEL_HOLD_CAP_NS
 * of real time. Game threads never spin here: a sync read sleeps (pc_vi_sleep_until_retrace) */
static void model_wait_until(u32 due) {
    const Uint64 start = SDL_GetTicksNS();
    static int s_capped_logged;
    while ((s32)(due - VIGetRetraceCount()) > 0) {
        if (SDL_GetTicksNS() - start > MODEL_HOLD_CAP_NS) {
            if (!s_capped_logged) {
                s_capped_logged = 1;
                fprintf(stderr, "[dvd] a read was held over 2 s waiting for retrace %u (now %u): delivered now; the "
                                "retrace count is not advancing during this wait (logged once)\n",
                        due, VIGetRetraceCount());
            }
            return;
        }
        SDL_DelayNS(1000000); /* 1 ms */
    }
}

/* on Aurora's DVD worker: hold the completion until its retrace (the worker never runs game code itself) */
static void model_wait(const void* block) {
    u32 due = 0;
    SDL_LockSpinlock(&s_pending_lock);
    {
        const int i = pending_find(block);
        if (i >= 0) {
            due = s_model_pending[i].due;
        }
    }
    SDL_UnlockSpinlock(&s_pending_lock);
    if (due != 0) {
        model_wait_until(due);
    }
}

/* on the game thread (a posted call): the read is handed over now, so its status may say done; then the callback */
static void deliver_posted(s32 result, void* block) {
    AuroraDvdCallback callback = NULL;
    SDL_LockSpinlock(&s_pending_lock);
    {
        const int i = pending_find(block);
        if (i >= 0) {
            callback = s_model_pending[i].callback;
            s_model_pending[i].block = NULL;
        }
    }
    SDL_UnlockSpinlock(&s_pending_lock);
    if (callback != NULL) {
        callback(result, (struct DVDCommandBlock*)block);
    }
}

extern int pc_vi_virtual_clock(void); /* pc_vi.c: the replay clock */

static void dispatch(AuroraDvdCallback callback, s32 result, struct DVDCommandBlock* block) {
    int i;
    ++pc_dvd_completions;
    SDL_LockSpinlock(&s_pending_lock);
    i = pending_find(block);
    if (i >= 0) {
        s_model_pending[i].callback = callback;
        s_model_pending[i].result = result;
    }
    SDL_UnlockSpinlock(&s_pending_lock);
    if (i >= 0 && pc_vi_virtual_clock()) {
        SDL_SetAtomicInt(&s_model_pending[i].done, 1); /* the game thread delivers it (pc_dvd_virtual_deliver) */
        return;
    }
    if (model_on()) {
        model_wait(block);
    }
    if (i >= 0) {
        pc_os_post_call(deliver_posted, result, block);
    } else {
        pc_os_post_call((void (*)(s32, void*))callback, result, block);
    }
}

/* The replay clock (pc_vi.c): disc completions at fixed points. At each interrupt point of the game thread, every
 * read due by the current retrace is handed over, in issue order; if the worker hasn't finished one yet (the host's
 * disc is slower than the model), the game thread waits for it here, so the moment the game sees it never depends on
 * the host. A read without a callback just stops being busy. */
static void virtual_deliver(double passed);
static void pc_dvd_virtual_deliver(void) {
    virtual_deliver(1.0); /* the whole field (ORRE_DVD_SUBFIELD's share of a field removed 2026-11-01) */
}
/* passed: how much of the current field has gone by (1: every read due in it) */
static void virtual_deliver(double passed) {
    const u32 now = VIGetRetraceCount();
    for (;;) {
        int best = -1;
        AuroraDvdCallback callback;
        s32 result;
        const struct DVDCommandBlock* block;
        SDL_LockSpinlock(&s_pending_lock);
        for (int i = 0; i < MODEL_PENDING; i++) {
            if (s_model_pending[i].block != NULL &&
                ((s32)(s_model_pending[i].due - now) < 0 ||
                 (s_model_pending[i].due == now && s_model_pending[i].due_frac <= passed) ||
                 (s_model_pending[i].alt_due >= 0.0 && s_model_pending[i].polls >= 32 &&
                  ((s32)((u32)floor(s_model_pending[i].alt_due) - now) < 0 ||
                   ((u32)floor(s_model_pending[i].alt_due) == now &&
                    s_model_pending[i].alt_due - floor(s_model_pending[i].alt_due) <= passed)))) &&
                (best < 0 || (s32)(s_model_pending[i].seq - s_model_pending[best].seq) < 0)) {
                best = i;
            }
        }
        SDL_UnlockSpinlock(&s_pending_lock);
        if (best < 0) {
            return;
        }
        {
            const Uint64 start = SDL_GetTicksNS();
            for (;;) {
                const int physically_done =
                    s_model_pending[best].has_callback
                        ? SDL_GetAtomicInt(&s_model_pending[best].done)
                        : (__atomic_load_n(&s_model_pending[best].block->state, __ATOMIC_ACQUIRE) != DVD_STATE_BUSY &&
                           __atomic_load_n(&s_model_pending[best].block->state, __ATOMIC_ACQUIRE) != DVD_STATE_WAITING);
                if (physically_done) {
                    break;
                }
                if (SDL_GetTicksNS() - start > 10000000000ull) {
                    fprintf(stderr, "[dvd] replay clock: a read is still not done after 10 s: handed over unfinished\n");
                    break;
                }
                SDL_DelayNS(50000);
            }
        }
        SDL_LockSpinlock(&s_pending_lock);
        callback = s_model_pending[best].callback;
        result = s_model_pending[best].result;
        block = s_model_pending[best].block;
        s_model_pending[best].block = NULL;
        SDL_UnlockSpinlock(&s_pending_lock);
        {
            /* ORRE_DVD_TRACE_LOG=1: when each read is handed to the game (2026-10-24: which pass of the archive
             * loader sees it) */
            static int s_log = -1;
            if (s_log < 0) {
                const char* env = getenv("ORRE_DVD_TRACE_LOG");
                s_log = env != NULL && *env == '1';
            }
            if (s_log) {
                fprintf(stderr, "[dvd] handed over: due %u+%.2f at retrace %u, field share %.3f, game frame %u\n",
                        (unsigned)s_model_pending[best].due, s_model_pending[best].due_frac, VIGetRetraceCount(),
                        passed, pc_vi_heartbeat);
            }
        }
        if (callback != NULL) {
            callback(result, (struct DVDCommandBlock*)block);
        }
        {
            /* ORRE_FSYS_LOG=2: the loader's slots right after the hand-over (src/game/pc_debug_game.c) */
            extern void pc_fsys_log_now(const char* why) __attribute__((weak));
            if (pc_fsys_log_now != NULL) {
                pc_fsys_log_now(callback != NULL ? "handed over (callback run)" : "handed over (polled)");
            }
        }
    }
}

extern void pc_os_interrupt_point(void);
extern void pc_os_set_deliver_hook(void (*hook)(void));

/* Port startup, before the game's first DVD read. The poll hook: the game waits for some reads by spinning on a
 * status call (fsys's slot state machine on DVDGetCommandBlockStatus, run 31); on the GameCube the DVD interrupt
 * arrives in the middle of that loop, so each status poll is an interrupt point here too. Without it the posted
 * completion never ran and the main loop spun forever. The status hook keeps a read busy until it is handed over. */
/* Removed 2026-10-26 (snatcherLeo: PC time; the project notes): ORRE_CPU_LZSS and its hook
 * OrreHook_LzssUnpacked, the console's unpacking time charged to the game thread (tried, off: it double-counted) */
void pc_dvd_install(void) {
    aurora_dvd_set_callback_dispatcher(dispatch);
    aurora_dvd_set_poll_hook(pc_os_interrupt_point);
    aurora_dvd_set_status_hook(pending_status);
    pc_os_set_deliver_hook(pc_dvd_virtual_deliver);
}

/* DVDRead / DVDReadAsync are macros in Aurora's dvd.h (to the ...Prio versions); the game declares and calls them
 * as functions, so the PC provides them (priority 2, the SDK's default) */
#include <dolphin/dvd.h>
#include <stdio.h>
#undef DVDRead
#undef DVDReadAsync
/* with ORRE_FRAME_TIMES, each read inside the logged frames (pc_vi.c says whether a frame is in range): the file by
 * its place on the disc (its start offset in the image and its length identify it in the disc's file table), the
 * offset in the file and the size, and whether the game waits for it (sync) or gets a callback (async) */
extern int pc_vi_frame_times_active(void);
/* returns the modelled completion frame (0 when the model is off) */
static u32 note_read(const DVDFileInfo* fileInfo, s32 length, s32 offset, const char* how) {
    u32 due = 0;
    ++pc_dvd_reads_started;
    pc_dvd_bytes_started += (u32)length;
    if (model_on()) {
        /* *Added 2026-11-19 (the Japanese release look's determinism fix; docs/port/word-pictures.md):* a read of an
         * overlaid file (a language's or the Japanese look's archive, pc_lang_files.cpp) is timed as the read the game
         * makes of the US file it stands in for: Aurora gives the overlay the US file's disc offset (startAddr), and
         * pc_lang_files maps the offset and length inside it (a swapped entry appended at the end -> the US entry's
         * place and size). Why: the drive model charges seeks and bytes, so a bigger or moved entry finished its load a
         * frame or two later and later random rolls landed on other frames (P07, 2026-11-19). The bytes still come
         * from the overlay; only the modelled time changes. Platforms: all (plain C). */
        uint32_t t_off = (uint32_t)offset, t_len = (uint32_t)length;
        void* const overlay = aurora_dvd_overlay_handle(fileInfo);
        if (overlay != NULL) {
            extern int pc_lang_files_disc_timing(void* handle, uint32_t* offset, uint32_t* length);
            pc_lang_files_disc_timing(overlay, &t_off, &t_len);
        }
        due = model_issue((u32)fileInfo->startAddr + (u32)t_off, (u32)t_len);
    }
    if (pc_vi_frame_times_active()) {
        fprintf(stderr, "[frametime] %u  disc read %s: file at 0x%08X (%u B), offset 0x%X, %d B%s", pc_vi_heartbeat,
                how, (u32)fileInfo->startAddr, (u32)fileInfo->length, (u32)offset, (int)length, due ? "" : "\n");
        if (due) {
            fprintf(stderr, ", modelled to finish at retrace %u (now %u)\n", due, VIGetRetraceCount());
        }
    }
    return due;
}

s32 DVDRead(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset) {
    s32 result;
    const u32 due = note_read(fileInfo, length, offset, "sync");
    result = DVDReadPrio(fileInfo, addr, length, offset, 2);
    if (due) {
        /* the calling game thread sleeps until the drive is done, as on the console; other threads run meanwhile
         * (corrected 2026-10-19: a spinning wait starved the main thread, see pc_vi_sleep_until_retrace) */
        extern void pc_vi_sleep_until_retrace(u32 count);
        pc_vi_sleep_until_retrace(due);
    }
    return result;
}
BOOL DVDReadAsync(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback) {
    const u32 due = note_read(fileInfo, length, offset, "async");
    if (pc_vi_virtual_clock()) { /* every async read, so each is handed over at a fixed point */
        model_remember(&fileInfo->cb, due ? due : VIGetRetraceCount(), due ? s_issue_frac : 0.0, callback != NULL);
    } else if (due && callback != NULL) { /* (no callback: no dispatch would ever clear its busy status) */
        model_remember(&fileInfo->cb, due, s_issue_frac, 1);
    }
    return DVDReadAsyncPrio(fileInfo, addr, length, offset, callback, 2);
}
