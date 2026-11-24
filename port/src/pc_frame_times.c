/*
 * pc_frame_times.c: replay mode plays the console's frame timing (2026-10-18; snatcherLeo's decision, gamecube-accuracy.md
 * row 4b).
 *
 *   ORRE_REPLAY_FRAMES=@file:<sitting>.frames   (made by tools/local/testing/frames_to_pc.py from a Dolphin replay;
 *                                                not the project's ORRE_FRAME_TIMES=FROM-TO, the frame-time log in pc_vi.c)
 *
 * Why: some scenes ask for 60 fps but the console needs two retraces to draw a frame there, so the game logic runs 30
 * times a second and its per-update work (eye-blink timers, NPC wandering, the random draws they make) runs half as
 * often as on the PC. A Dolphin movie replayed on the PC drifts at the first such scene (sitting 2: Phenac's entrance).
 * With this file, each game frame lasts at least as many retraces as the console's frame did, so the logic steps the
 * same way. Test aid only: never in normal play or the accurate preset (that one gets its own model later).
 *
 * The file has one line per room entry, `0x<room> count*retraces ...`. Each line starts at the PC's own entry of that
 * room (the next line for that room after the line in use), so the PC's faster loads don't shift it; past the end of a
 * line the last recorded frame length stays in force. The lines are split at the same moment the PC's room hook fires
 * (the replay's "entered" events; frames_to_pc.py, corrected 2026-10-19). It registers for the decision hook OrreHook_FrameRetraces
 * (src/game/gs_gfx_range_800D2B90.c, GSgfxWaitRetrace; default vanilla: no hook, nothing changes).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t (*OrreHook_FrameRetraces)(void); /* NULL: the game's own frame timing (vanilla) */
uint32_t (*OrreHook_FrameStep)(void);      /* NULL: the frame's measured length is its step (vanilla) */
int (*OrreHook_FrameSkipWait)(void);       /* NULL: every game frame waits for a retrace (vanilla) */

/* Logic passes (2026-10-26; the project's candidate 2, option (a)): a run may read `count*retraces/step`. The console's
 * fade-in into some rooms holds a logic pass for a retrace and later runs two in one, while its frame step stays one
 * retrace (P01 replay_38's steps probe). frames_logic.py writes those stretches per logic pass: a held pass as `1*2/1`
 * (two retraces, step 1) and the extra pass as `1*0/1` (no wait, step 1). Plain runs keep the measured step */
typedef struct {
    unsigned room;
    unsigned* runs;  /* pairs: count, retraces */
    unsigned* steps; /* per run: the step to report, 0 = the measured length */
    int nruns;
} FrameLine;

static uint32_t s_step; /* the step of the frame handed out last (0: none) */
static uint32_t s_len;  /* the recorded length of the frame handed out last (0: none) */

static FrameLine* s_lines;
static int s_nlines;
static int s_cur = -1;      /* the line in use */
static int s_run, s_left;   /* position inside it */

/* Timeline mode (2026-10-19; the old "length" rule and its ORRE_REPLAY_FRAMES_MODE switch removed 2026-11-01,
 * snatcherLeo's OK, timing-knobs-cleanup.md): game frame k of a room ends no earlier than the room's entry + the recording's frame lengths summed through k, instead of "lasts at least
 * its recorded length". A PC frame that runs late no longer pushes every later frame back (S02's Phenac: the load's
 * stalls and a few late reads left Wes appearing 9 frames late, and the camera, which starts with the room, had
 * settled by the time he walked, so his first steps went straight where the console's curved). */
extern uint32_t VIGetRetraceCount(void);
static uint32_t s_entry_vi, s_frame_start_vi;
static unsigned long s_sum;
static unsigned long s_frame_index; /* the game frame now running, counted from the line's start (0 = the first) */

static uint32_t frame_length_next(void);

/* ORRE_REPLAY_RNG=@file:<sitting>.rngframes (2026-10-25, a test aid like this whole file; made by
 * tools/local/testing/rngframes_to_pc.py): the console's random state at the start of every game frame, one line per
 * room entry split like the frame-length file. At the start of each frame the generator is set to it, so every roll
 * gets the console's number and what still differs is behaviour, not timing (P05's battles parted on single rolls
 * one frame or two draws off; option C's resync, done per frame). ORRE_REPLAY_RNG_ROOMS=0x23,0x38F limits it to those
 * rooms. s_item counts the recorded frames this line has handed out, so the state follows the frame lengths exactly */
static uint32_t** s_rng;
static unsigned* s_rng_n;
static int s_rng_lines;
static const char* s_rng_rooms;
static unsigned long s_item;
#define PC_RNG_NONE 0xFFFFFFFFu /* "-" in the file: no recorded state; also a valid state, never seen in practice */

/* OrreHook_RngFrameState (decision hook, docs/port/hook-points.md): a state posted here is taken by the game's
 * generator (src/hsd/random.c, HSD_Rand / HSD_Randf) before its next step. Default vanilla: nothing posted. Defined
 * here, in the platform layer, because the platform library also links into test programs without the game */
uint32_t OrreHook_RngFrameState;
int OrreHook_RngFrameStatePending;

static void rng_frame_start(void) {
    if (s_rng == NULL || s_cur < 0 || s_cur >= s_rng_lines || s_item >= s_rng_n[s_cur]) {
        return;
    }
    if (s_rng_rooms != NULL) {
        const char* p = s_rng_rooms;
        int listed = 0;
        while (*p != '\0') {
            char* end;
            if (strtoul(p, &end, 0) == s_lines[s_cur].room) {
                listed = 1;
            }
            if (end == p || *end != ',') {
                break;
            }
            p = end + 1;
        }
        if (!listed) {
            return;
        }
    }
    if (s_rng[s_cur][s_item] != PC_RNG_NONE) {
        OrreHook_RngFrameState = s_rng[s_cur][s_item];
        OrreHook_RngFrameStatePending = 1;
    }
}

static uint32_t frame_times_next(void) {
    uint32_t n = frame_length_next();
    uint32_t target, now, want;
    /* n == 0 with a line in use is a recorded zero-length logic pass (frames_logic.py): it still counts as a frame */
    if (s_cur < 0) {
        return n;
    }
    s_sum += n;
    target = s_entry_vi + (uint32_t)s_sum;
    now = VIGetRetraceCount();
    want = target > s_frame_start_vi ? target - s_frame_start_vi : 0;
    {
        /* ORRE_FRAME_TIMES_LOG=N (2026-10-21): the first N frames after each room entry, one line each: the
         * recorded length, where this frame should end and where it started and is now (retraces from the entry), and
         * the retraces it is asked to last. Sitting 9's Phenac: the PC's 30 fps frames ran one retrace behind the
         * console's from the room's load on, so presses fell into the next game frame */
        static long log_frames = -2;
        static unsigned long logged;
        static uint32_t logged_entry;
        if (log_frames == -2) {
            const char* env = getenv("ORRE_FRAME_TIMES_LOG");
            log_frames = env != NULL ? atol(env) : -1;
        }
        if (logged_entry != s_entry_vi) {
            logged_entry = s_entry_vi;
            logged = 0;
        }
        if (log_frames > 0 && logged < (unsigned long)log_frames) {
            logged++;
            fprintf(stderr, "[ft] room 0x%X frame %lu: length %u, ends +%u, started +%u, now +%u, wait %u\n",
                    s_lines[s_cur].room, logged, n, (unsigned)(target - s_entry_vi),
                    (unsigned)(s_frame_start_vi - s_entry_vi), (unsigned)(now - s_entry_vi), (unsigned)want);
        }
    }
    s_frame_start_vi = target > now ? target : now; /* this frame ends then, and the next one starts */
    s_frame_index++;
    rng_frame_start(); /* ORRE_REPLAY_RNG: the next frame starts from the console's state */
    return want;
}

static uint32_t frame_length_next(void) {
    s_step = 0;
    s_len = 0;
    if (s_cur < 0) {
        return 0;
    }
    while (s_left == 0) {
        if (s_run >= s_lines[s_cur].nruns) {
            /* past the recording of this entry (the PC stayed longer in the room than the console did): keep the last
             * recorded frame length, the console's pace in that scene. Corrected 2026-10-19: was the game's own timing
             * (0), which ran a 30 fps scene at 60 on the PC from there on. A trailing logic-pass run (a step given)
             * isn't a pace: its step stands in */
            if (s_lines[s_cur].nruns == 0) {
                return 0;
            }
            {
                const int last = s_lines[s_cur].nruns - 1;
                return s_lines[s_cur].steps[last] ? s_lines[s_cur].steps[last] : s_lines[s_cur].runs[2 * last + 1];
            }
        }
        s_left = (int)s_lines[s_cur].runs[2 * s_run];
        s_run++;
    }
    s_left--;
    s_item++; /* ORRE_REPLAY_RNG: the recorded frame handed out */
    s_step = s_lines[s_cur].steps[s_run - 1];
    s_len = s_lines[s_cur].runs[2 * (s_run - 1) + 1];
    return s_len;
}

/* OrreHook_FrameStep: the step of the frame frame_times_next just handed out (0: the measured length). (Its
 * ORRE_REPLAY_FRAMES_STEP=1 option, the recorded length as the step, was removed 2026-11-01: unused since the
 * logic-pass steps.) */
static uint32_t frame_times_step(void) { return s_step; }

/* OrreHook_FrameSkipWait: is the next recorded frame a zero-length logic pass? Asked before the game's retrace wait,
 * so it peeks without handing the frame out (frame_times_next does that after the wait) */
static int frame_times_skip_wait(void) {
    int run = s_run;
    if (s_cur < 0) {
        return 0;
    }
    if (s_left == 0) {
        if (run >= s_lines[s_cur].nruns) {
            return 0;
        }
        run++;
    }
    return s_lines[s_cur].runs[2 * (run - 1) + 1] == 0;
}

/* For pc_dvd.c's ORRE_DVD_FRAMES (2026-10-20): which game frame of line `line` contains the retrace `vi_off`
 * retraces after the line's first frame, on the recorded frame lengths. dvd_seen.py's map gives probe labels (retraces)
 * since the room's entry, not game frames; in a 30 fps room the two differ by 2x (P01's first run with the map read
 * them as frames and held reads hundreds of frames). 1 and *frame when the line exists and is long enough */
int pc_frame_times_frame_of_vi(int line, unsigned long vi_off, unsigned long* frame) {
    unsigned long at = 0, k = 0;
    int run;
    if (line < 0 || line >= s_nlines) {
        return 0;
    }
    for (run = 0; run < s_lines[line].nruns; run++) {
        unsigned long count = s_lines[line].runs[2 * run], n = s_lines[line].runs[2 * run + 1];
        if (n == 0) {
            k += count; /* zero-length logic passes (frames_logic.py) take no time */
            continue;
        }
        if (vi_off < at + count * n) {
            *frame = k + (vi_off - at) / n;
            return 1;
        }
        at += count * n;
        k += count;
    }
    return 0;
}

/* For pc_dvd.c's ORRE_DVD_FRAMES (2026-10-20; snatcherLeo's decision (b)): the retrace at which game frame `frame` of
 * line `line` starts on the recorded timeline (its line index in the .frames file, the boot line 0). 1 and *out when
 * that line is the one in use; a frame already started gives the current frame's start (deliver now) */
int pc_frame_times_start_of(int line, unsigned long frame, uint32_t* out) {
    int run = s_run, left = s_left;
    unsigned long k = s_frame_index;
    uint32_t at = s_frame_start_vi;
    if (s_cur < 0 || line != s_cur) {
        return 0;
    }
    while (k < frame) {
        uint32_t n;
        while (left == 0) {
            if (run >= s_lines[s_cur].nruns) {
                return 0; /* past the recorded line */
            }
            left = (int)s_lines[s_cur].runs[2 * run];
            run++;
        }
        left--;
        n = s_lines[s_cur].runs[2 * (run - 1) + 1];
        at += n;
        k++;
    }
    *out = at;
    return 1;
}

/* The debug command "frameat K" (2026-10-28; src/game/pc_debug_game.c): the next frame handed out is frame K of
 * the line in use, and the timeline is re-based so that frame starts where the current one ends. The pad writes it at
 * anchored window openings, so the recorded lengths follow the windows inside a long menu visit instead of drifting
 * with the time since the room entry. 0 when no line is in use or K is past the line; *was = the frame it replaces */
int pc_frame_times_seek(unsigned long frame, unsigned long* was) {
    unsigned long k = 0;
    unsigned long sum = 0;
    int run;
    if (was != NULL) {
        *was = s_frame_index;
    }
    if (s_cur < 0) {
        return 0;
    }
    for (run = 0; run < s_lines[s_cur].nruns; run++) {
        unsigned long count = s_lines[s_cur].runs[2 * run], n = s_lines[s_cur].runs[2 * run + 1];
        if (frame < k + count) {
            s_run = run + 1;
            s_left = (int)(count - (frame - k));
            sum += (frame - k) * n;
            s_frame_index = frame;
            s_item = frame;
            s_sum = sum;
            s_entry_vi = s_frame_start_vi - (uint32_t)sum;
            return 1;
        }
        k += count;
        sum += count * n;
    }
    return 0;
}

/* From the room hook (pc_game_main.c): move to the next line for this room */
void pc_frame_times_room_entered(unsigned room) {
    for (int i = s_cur + 1; i < s_nlines; i++) {
        if (s_lines[i].room == room) {
            s_cur = i;
            s_run = 0;
            s_left = 0;
            s_entry_vi = s_frame_start_vi = VIGetRetraceCount();
            /* Removed 2026-10-26 (snatcherLeo: PC time; the project notes): ORRE_REPLAY_FRAMES_ENTRY_SHIFT,
             * a room's timeline started N retraces after the hook (no change on P01 / P02) */
            s_sum = 0;
            s_frame_index = 0;
            s_item = 0;
            /* Removed 2026-11-01 (snatcherLeo's OK; timing-knobs-cleanup.md): ORRE_REPLAY_FRAMES_SKIP_ENTRY (=1, =2 or a room
             * list), skipping a line's first recorded frame, the console's entry frame (P01's second name screen, an
             * older run; no final replay uses it) */
            fprintf(stderr, "[frame_times] room 0x%X: line %d of %d\n", room, i + 1, s_nlines);
            rng_frame_start(); /* ORRE_REPLAY_RNG: the room's first frame starts from the console's state */
            return;
        }
    }
}

static void parse_line(char* line) {
    char* tok = strtok(line, " \t\r\n");
    FrameLine fl;
    int cap = 16;
    if (tok == NULL || tok[0] == '#') {
        return;
    }
    fl.room = (unsigned)strtoul(tok, NULL, 0);
    fl.nruns = 0;
    fl.runs = (unsigned*)malloc(sizeof(unsigned) * 2 * cap);
    fl.steps = (unsigned*)malloc(sizeof(unsigned) * cap);
    while ((tok = strtok(NULL, " \t\r\n")) != NULL) {
        char* star = strchr(tok, '*');
        char* slash;
        if (star == NULL) {
            continue;
        }
        if (fl.nruns == cap) {
            cap *= 2;
            fl.runs = (unsigned*)realloc(fl.runs, sizeof(unsigned) * 2 * cap);
            fl.steps = (unsigned*)realloc(fl.steps, sizeof(unsigned) * cap);
        }
        slash = strchr(star, '/');
        fl.runs[2 * fl.nruns] = (unsigned)strtoul(tok, NULL, 10);
        fl.runs[2 * fl.nruns + 1] = (unsigned)strtoul(star + 1, NULL, 10);
        fl.steps[fl.nruns] = slash != NULL ? (unsigned)strtoul(slash + 1, NULL, 10) : 0;
        fl.nruns++;
    }
    s_lines = (FrameLine*)realloc(s_lines, sizeof(FrameLine) * (size_t)(s_nlines + 1));
    s_lines[s_nlines++] = fl;
}

/* ORRE_TEST_FRAME_RETRACES=N (2026-11-13, port-gaps 78; a test switch, never a setting): every game frame lasts
 * at least N retraces, as the console's heavy scenes do (the summary page runs at 30 frames a second on the console,
 * 60 on the PC). For checking whether a look-only problem comes from the PC showing the in-between frames the console
 * never shows (Espeon's idle sway). Ignored when ORRE_REPLAY_FRAMES gives the console's real frame lengths. */
/* The menu model ORRE_MENUMODEL_LOG watches (port-gaps 78; observe only): set by menu_model.c, read by
 * gs_model_anim_exact_800EC4D0.c's per-apply log. NULL: nothing logged. Kept here, in a port file, so the game units
 * (several of which include the same source) don't each define it */
void* pc_menumodel_watch;

/* The menu Pokemon model shown at the console's whole animation frames, and the game frame it was last drawn (port-gaps
 * 78; set by menu_model.c, read by modelApplyAnimation). A port correctness fix, not a hook: the console never draws
 * these models' half frames. Kept in a port file for the same reason as the watch above */
void* pc_console_cadence_model;
uint32_t pc_console_cadence_frame;

static uint32_t s_test_retraces;
static uint32_t test_frame_retraces(void) { return s_test_retraces; }

/* At start-up (pc_game_main.c): read ORRE_REPLAY_FRAMES and register for the hook */
void pc_frame_times_init(void) {
    const char* env = getenv("ORRE_REPLAY_FRAMES");
    const char* path;
    FILE* f;
    static char line[1 << 16];
    if (env == NULL || *env == '\0') {
        const char* test = getenv("ORRE_TEST_FRAME_RETRACES");
        if (test != NULL && atoi(test) > 1) {
            s_test_retraces = (uint32_t)atoi(test);
            OrreHook_FrameRetraces = test_frame_retraces;
            fprintf(stderr, "[frame_times] ORRE_TEST_FRAME_RETRACES: every game frame lasts at least %u retraces\n",
                    (unsigned)s_test_retraces);
        }
        return;
    }
    path = strncmp(env, "@file:", 6) == 0 ? env + 6 : env;
    f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "[frame_times] cannot open '%s'\n", path);
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        parse_line(line);
    }
    fclose(f);
    OrreHook_FrameRetraces = frame_times_next;
    {
        /* the logic-pass hooks only when the file carries a step (frames_logic.py): otherwise nothing changes */
        int stepped = 0;
        for (int i = 0; i < s_nlines && !stepped; i++) {
            for (int r = 0; r < s_lines[i].nruns; r++) {
                if (s_lines[i].steps[r] != 0) {
                    stepped++;
                    break;
                }
            }
        }
        if (stepped) {
            OrreHook_FrameStep = frame_times_step;
            OrreHook_FrameSkipWait = frame_times_skip_wait;
        }
        fprintf(stderr, "[frame_times] %d room entries from %s (replay mode: the console's frame timing%s)\n", s_nlines,
                path, stepped ? ", with its logic passes" : "");
    }
    {
        /* ORRE_REPLAY_RNG=@file:<sitting>.rngframes (see rng_frame_start): one line per room entry in the same order,
         * read whole (a room's line can hold tens of thousands of states) */
        const char* renv = getenv("ORRE_REPLAY_RNG");
        FILE* rf;
        char* buf;
        long size;
        if (renv == NULL || *renv == '\0') {
            return;
        }
        rf = fopen(strncmp(renv, "@file:", 6) == 0 ? renv + 6 : renv, "rb");
        if (rf == NULL) {
            fprintf(stderr, "[rngframes] cannot open '%s'\n", renv);
            return;
        }
        fseek(rf, 0, SEEK_END);
        size = ftell(rf);
        fseek(rf, 0, SEEK_SET);
        buf = (char*)malloc((size_t)size + 1);
        size = (long)fread(buf, 1, (size_t)size, rf);
        buf[size] = '\0';
        fclose(rf);
        for (char* ln = buf; ln != NULL && *ln != '\0';) {
            char* next = strchr(ln, '\n');
            if (next != NULL) {
                *next++ = '\0';
            }
            if (ln[0] == '0') { /* "0x<room> <state> ...": a room-entry line ('#' comments and blanks skipped) */
                unsigned cap = 256, n = 0;
                uint32_t* states = (uint32_t*)malloc(sizeof(uint32_t) * cap);
                char* tok = strtok(ln, " \t\r");
                while ((tok = strtok(NULL, " \t\r")) != NULL) {
                    if (n == cap) {
                        cap *= 2;
                        states = (uint32_t*)realloc(states, sizeof(uint32_t) * cap);
                    }
                    states[n++] = tok[0] == '-' ? PC_RNG_NONE : (uint32_t)strtoul(tok, NULL, 16);
                }
                s_rng = (uint32_t**)realloc(s_rng, sizeof(uint32_t*) * (size_t)(s_rng_lines + 1));
                s_rng_n = (unsigned*)realloc(s_rng_n, sizeof(unsigned) * (size_t)(s_rng_lines + 1));
                s_rng[s_rng_lines] = states;
                s_rng_n[s_rng_lines] = n;
                s_rng_lines++;
            }
            ln = next;
        }
        free(buf);
        s_rng_rooms = getenv("ORRE_REPLAY_RNG_ROOMS");
        fprintf(stderr, "[rngframes] %d room entries (%s); the frame lengths have %d%s\n", s_rng_lines,
                s_rng_rooms != NULL ? s_rng_rooms : "every room", s_nlines,
                s_rng_lines == s_nlines ? "" : ": MISMATCH, the states won't line up");
    }
}
