/*
 * pc_pad_script.c: scripted controller input for unattended runs (2026-10-16; extended the same day for
 * the test routes, testing-plan.md 4.1).
 *
 *   ORRE_PAD_SCRIPT=6000:START,6010:-,6300:A,6310:-
 *   ORRE_PAD_SCRIPT=@file:routes/new_game.pad      (or the plain path: a value ending in .pad or with a slash)
 *   ORRE_ROUTE_CHECK=0x390@7000,0xC8@23000         (each room entered by that frame, or the run stops: exit 4)
 *
 * A script that gives no steps stops the run at once (exit 3), rather than letting it idle unscripted.
 *
 * A script is a list of steps, separated by commas or new lines; in a file, '#' starts a comment. Each step is
 * WHEN:WHAT and sets the whole state of controller 1 until the next step:
 *
 *   WHEN   N        at game frame N (VIWaitForRetrace calls, the count ORRE_FRAME_LOG / ORRE_DUMP_FRAMES use)
 *          +N       N frames after the previous step
 *          @R+N     N frames after room R is entered (R decimal or 0x hex; the first entry of R after the entry the
 *                   previous "@" step used, so the next visit when a script returns to a room; pushed menu rooms such
 *                   as name entry count as entries. Changed 2026-10-18: it was "at or after the previous step")
 *          @R+N~G   the same, but if R hasn't come G frames after the previous step, fire then without it (2026-10-19:
 *                   battle window anchors, so one message window the PC doesn't show can't stall a replay)
 *   WHAT   -        nothing held, sticks centred
 *          tokens joined by '+': buttons A B X Y Z L R START UP DOWN LEFT RIGHT, and analog values
 *          SX=v SY=v (main stick) CX=v CY=v (C-stick), 0..255 with 128 the centre (SY > 128 is up), and
 *          LT=v RT=v (analog triggers, 0..255). Anything not named is released / centred.
 *
 *   e.g. 812:SX=200+SY=128+A     @0x1+600:A     +30:-
 *
 * WHAT can also be MASH: tap A (4 frames down, 4 up) until the next step fires (2026-10-21, battle-tolerant
 * replays: message boxes the PC shows after a battle that the recording has no presses for).
 *
 * WHAT can also be !command: a debug toolkit command (pc_debug.h; needs ORRE_DEBUG=1), queued when the step
 * fires, e.g. @0x384+60:!warp 0x1 0   or   +1:!flag 120 1. It does not change the pad state.
 *
 * It uses Aurora's virtual pad status (PADSetVirtualStatus), so the game reads it through PADRead as a connected
 * controller. Real controllers and the keyboard are ignored while a script runs: PADSetVirtualExclusive (an aurora:
 * change, 2026-10-17; before it, Aurora merged the virtual status with real input, and a person's controller reached a
 * scripted run). When the script ends they work again, except in unattended runs (script_finished). Room entries come from the project's game-side hook
 * (pc_room_entered_hook in gs_floor.c), connected in pc_game_main.c to pc_pad_script_room_entered.
 */
#include <dolphin/pad.h>

#include "pc_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WHEN_FRAME, WHEN_AFTER_PREV, WHEN_AFTER_ROOM };

typedef struct {
    int when;
    unsigned frames; /* the frame (WHEN_FRAME) or the delay */
    unsigned room;   /* WHEN_AFTER_ROOM */
    unsigned fallback; /* WHEN_AFTER_ROOM "~G": due G frames after the previous step even if the anchor never comes */
    PADStatus pad;
    char* command;   /* WHEN:!command: a debug toolkit command instead of a pad state (pc_debug.h) */
    int mash;        /* WHEN:MASH: tap A (4 frames down, 4 up) until the next step fires */
} PadStep;

static PadStep* s_steps;
static int s_count = -1;
static int s_cap;
static int s_next;
static unsigned s_prev_fired; /* frame the previous step fired */

/* the rooms entered so far (room, frame); a short history is enough: steps wait for an entry after the previous step */
#define ROOM_LOG 64
static struct {
    unsigned room, frame;
    int used; /* an "@" step was keyed to it (same-frame window anchors, see step_due) */
} s_rooms[ROOM_LOG];
static int s_nrooms;
static volatile unsigned s_frame_now;

static void log_entry(unsigned room, unsigned frame) {
    s_rooms[s_nrooms % ROOM_LOG].room = room;
    s_rooms[s_nrooms % ROOM_LOG].frame = frame;
    s_rooms[s_nrooms % ROOM_LOG].used = 0;
    s_nrooms++;
}

/* ORRE_ROUTE_CHECK=ROOM@FRAME[,ROOM@FRAME...] (2026-10-18): checkpoints for an unattended run. Each room must have been
 * entered by that game frame, or the run stops (exit 4) and says where it is; a route that drifts (a walk that opens
 * the world map instead of reaching the diner) no longer runs on for minutes. e.g. ORRE_ROUTE_CHECK=0x390@7000,0xC8@23000 */
/* Stop an unattended run now with a code saferun reports: the log is written out first, then the process ends without
 * running the exit handlers (a normal exit from the game thread tore Aurora down under its render worker, and the
 * resulting "Device lost" fatal turned exit 4 into exit 3) */
void pc_log_exit_clean(const char* how); /* pc_log.c */
static void stop_run(int code) {
    char how[64];
    fflush(stderr);
    /* a test run stopped on purpose (with its result code) is a clean end too: the next start doesn't call it a crash */
    snprintf(how, sizeof how, "test run stopped (code %d)", code);
    pc_log_exit_clean(how);
    _Exit(code);
}

#define ROUTE_CHECKS 16
static struct {
    unsigned room, frame;
    int met;
} s_checks[ROUTE_CHECKS];
static int s_nchecks = -1;
static unsigned s_last_room = 0xFFFFFFFFu;

static void route_checks_parse(void) {
    const char* env = getenv("ORRE_ROUTE_CHECK");
    s_nchecks = 0;
    while (env != NULL && *env != '\0' && s_nchecks < ROUTE_CHECKS) {
        char* end;
        unsigned room = (unsigned)strtoul(env, &end, 0);
        if (*end != '@') {
            fprintf(stderr, "[route_check] cannot read '%s' (ROOM@FRAME)\n", env);
            break;
        }
        s_checks[s_nchecks].room = room;
        s_checks[s_nchecks].frame = (unsigned)strtoul(end + 1, &end, 0);
        s_checks[s_nchecks].met = 0;
        s_nchecks++;
        env = *end == ',' ? end + 1 : end;
    }
    if (s_nchecks > 0) {
        fprintf(stderr, "[route_check] %d checkpoint(s)\n", s_nchecks);
    }
}

static void route_checks_frame(unsigned frame) {
    for (int i = 0; i < s_nchecks; i++) {
        if (!s_checks[i].met && frame > s_checks[i].frame) {
            fprintf(stderr, "[route_check] ERROR: room 0x%X not entered by frame %u (now %u, last room 0x%X): the "
                            "route drifted; stopping the run (exit 4)\n",
                    s_checks[i].room, s_checks[i].frame, frame, s_last_room);
            stop_run(4);
        }
    }
}

void pc_pad_script_room_entered(unsigned room) {
    log_entry(room, s_frame_now);
    s_last_room = room;
    for (int i = 0; i < s_nchecks; i++) {
        if (!s_checks[i].met && s_checks[i].room == room) {
            s_checks[i].met = 1;
            fprintf(stderr, "[route_check] room 0x%X entered at frame %u (deadline %u): ok\n", room, s_frame_now,
                    s_checks[i].frame);
        }
    }
    if (s_count > 0) {
        fprintf(stderr, "[pad_script] room 0x%X entered at frame %u\n", room, s_frame_now);
    }
#if PC_HAVE_AURORA_DVD
    {
        extern void pc_dvd_trace_room_entered(unsigned room); /* pc_dvd.c: ORRE_DVD_TRACE's per-room read counts */
        pc_dvd_trace_room_entered(room);
    }
#endif
}

/* Note (2026-11-19, found's helper on the 3.5 Rui-model runs): returning to the field after a battle does not
 * count as entering the room ("@<room>" anchors don't fire then), while the player's reappearance does: use
 * "@0x1<room>" to time presses after a battle. All platforms. */
/* "@0x1<room>+N" (2026-10-19): N frames after the player appears in that room (room id 0x10000 + room), from
 * the game side's pc_hero_watch_frame (pc_debug_game.c). Recordings time the presses after a room's load from the
 * player's appearance, so a few frames' difference in how long the PC takes to build the room no longer moves them
 * (sitting 2's Phenac: the PC created Wes 8 frames later than the console, and his walk drifted off the recorded
 * path). Kept in the same entry log as rooms, so "@" steps count it the same way. */
void pc_pad_script_hero_appeared(unsigned room, unsigned frame) {
    log_entry(0x10000u | room, frame);
    if (s_count > 0) {
        fprintf(stderr, "[pad_script] player appeared in room 0x%X at frame %u (anchor 0x%X)\n", room, frame,
                0x10000u | room);
    }
}

/* "@0x3<room>+N" (2026-10-26; snatcherLeo's step 3 of "Comparing with Dolphin"): N frames after the room's fade-in finishes
 * (the screen fade's pending flag 1 -> 0 with type 2, from the game side's pc_hero_watch_frame). The evidence table
 * showed the game reads each press at the same refresh after the fade-in's end on the console and the PC, while the
 * room change before it is 1-2 refreshes shorter on the PC, so presses timed from the room entry came early. Kept in
 * the same entry log as rooms (id 0x30000 + room), so "@" steps count it the same way. */
void pc_pad_script_fadein_done(unsigned room, unsigned frame) {
    log_entry(0x30000u | (room & 0xFFFFu), frame);
    if (s_count > 0) {
        fprintf(stderr, "[pad_script] fade-in finished in room 0x%X at frame %u (anchor 0x%X)\n", room, frame,
                0x30000u | (room & 0xFFFFu));
    }
}

/* "@0x2<window>+N" (2026-10-19; snatcherLeo: "time battle and menu inputs from when each menu opens"): N frames after a
 * window finishes opening and starts reading buttons (its phase 1 -> 2 in menuDaemon; OrreHook_WindowReady, connected
 * in pc_game_main.c). Menus take input from one exact frame, so a press timed from the room entry is lost when the PC
 * opens the menu a frame later than the console did (sitting 2's battle: a send-out a frame late, and Mewtwo's
 * command never registered). dtm_to_pad.py --windows anchors every window opening in the recording, in order. Kept in
 * the same entry log as rooms (id 0x20000 + window id), so "@" steps count it the same way. */
void pc_pad_script_window_ready(unsigned id) {
    log_entry(0x20000u | (id & 0xFFFFu), s_frame_now);
    if (s_count > 0) {
        fprintf(stderr, "[pad_script] window 0x%X ready at frame %u (anchor 0x%X)\n", id, s_frame_now,
                0x20000u | (id & 0xFFFFu));
    }
}

static unsigned short button_bit(const char* name, size_t len) {
    static const struct {
        const char* name;
        unsigned short bit;
    } names[] = {
        {"A", PAD_BUTTON_A},       {"B", PAD_BUTTON_B},     {"X", PAD_BUTTON_X},       {"Y", PAD_BUTTON_Y},
        {"Z", PAD_TRIGGER_Z},      {"L", PAD_TRIGGER_L},    {"R", PAD_TRIGGER_R},      {"START", PAD_BUTTON_START},
        {"UP", PAD_BUTTON_UP},     {"DOWN", PAD_BUTTON_DOWN}, {"LEFT", PAD_BUTTON_LEFT}, {"RIGHT", PAD_BUTTON_RIGHT},
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strlen(names[i].name) == len && strncmp(names[i].name, name, len) == 0) {
            return names[i].bit;
        }
    }
    fprintf(stderr, "[pad_script] unknown button '%.*s'\n", (int)len, name);
    return 0;
}

/* One token of WHAT: a button or NAME=value */
static void apply_token(PADStatus* pad, const char* t, size_t len) {
    const char* eq = memchr(t, '=', len);
    if (len == 1 && *t == '-') {
        return;
    }
    if (eq != NULL) {
        size_t nlen = (size_t)(eq - t);
        char value[16];
        size_t vlen = len - nlen - 1;
        long v;
        if (vlen >= sizeof(value)) {
            vlen = sizeof(value) - 1;
        }
        memcpy(value, eq + 1, vlen); /* only this token's characters, never what follows it */
        value[vlen] = '\0';
        v = strtol(value, NULL, 0);
        if (v < 0) {
            v = 0;
        } else if (v > 255) {
            v = 255;
        }
        if (nlen == 2 && strncmp(t, "SX", 2) == 0) {
            pad->stickX = (s8)(v - 128);
        } else if (nlen == 2 && strncmp(t, "SY", 2) == 0) {
            pad->stickY = (s8)(v - 128);
        } else if (nlen == 2 && strncmp(t, "CX", 2) == 0) {
            pad->substickX = (s8)(v - 128);
        } else if (nlen == 2 && strncmp(t, "CY", 2) == 0) {
            pad->substickY = (s8)(v - 128);
        } else if (nlen == 2 && strncmp(t, "LT", 2) == 0) {
            pad->triggerLeft = (u8)v;
        } else if (nlen == 2 && strncmp(t, "RT", 2) == 0) {
            pad->triggerRight = (u8)v;
        } else {
            fprintf(stderr, "[pad_script] unknown value '%.*s'\n", (int)len, t);
        }
        return;
    }
    pad->button |= button_bit(t, len);
}

static int add_step(const PadStep* step) {
    if (s_count == s_cap) {
        int cap = s_cap ? s_cap * 2 : 64;
        PadStep* grown = (PadStep*)realloc(s_steps, (size_t)cap * sizeof(PadStep));
        if (grown == NULL) {
            return 0;
        }
        s_steps = grown;
        s_cap = cap;
    }
    s_steps[s_count++] = *step;
    return 1;
}

/* One step "WHEN:WHAT" (len bytes, no separators inside) */
static void parse_step(const char* p, size_t len) {
    PadStep step;
    const char* colon = memchr(p, ':', len);
    const char* end = p + len;
    char* num_end;
    memset(&step, 0, sizeof(step));
    if (colon == NULL) {
        fprintf(stderr, "[pad_script] cannot read '%.*s' (WHEN:WHAT)\n", (int)len, p);
        return;
    }
    if (*p == '@') {
        step.when = WHEN_AFTER_ROOM;
        step.room = (unsigned)strtoul(p + 1, &num_end, 0);
        step.frames = *num_end == '+' ? (unsigned)strtoul(num_end + 1, &num_end, 10) : 0;
        if (*num_end == '~') { /* "@X+N~G": fire G frames after the previous step if X hasn't come (see step_due) */
            step.fallback = (unsigned)strtoul(num_end + 1, &num_end, 10);
        }
    } else if (*p == '+') {
        step.when = WHEN_AFTER_PREV;
        step.frames = (unsigned)strtoul(p + 1, &num_end, 10);
    } else {
        step.when = WHEN_FRAME;
        step.frames = (unsigned)strtoul(p, &num_end, 10);
    }
    if (num_end != colon) {
        fprintf(stderr, "[pad_script] cannot read the time in '%.*s'\n", (int)len, p);
        return;
    }
    step.pad.err = PAD_ERR_NONE;
    if (colon + 1 < end && colon[1] == '!') { /* a debug toolkit command (pc_debug.h), not a pad state */
        size_t clen = (size_t)(end - colon - 2);
        step.command = (char*)malloc(clen + 1);
        if (step.command != NULL) {
            memcpy(step.command, colon + 2, clen);
            step.command[clen] = '\0';
        }
        if (!add_step(&step)) {
            fprintf(stderr, "[pad_script] out of memory\n");
        }
        return;
    }
    if ((size_t)(end - colon - 1) == 4 && memcmp(colon + 1, "MASH", 4) == 0) {
        /* "WHEN:MASH" (2026-10-21; battle-tolerant replays, snatcherLeo's option A): tap A until the next step fires,
         * for message boxes the PC shows and the recording has no presses for (sitting 9: Umbreon's level-up after
         * Folly's battle, which came from Willie's battle rolling other damage); dtm_to_pad.py puts it before the
         * player's reappearance after a battle */
        step.mash = 1;
        if (!add_step(&step)) {
            fprintf(stderr, "[pad_script] out of memory\n");
        }
        return;
    }
    for (p = colon + 1; p < end;) {
        const char* plus = memchr(p, '+', (size_t)(end - p));
        size_t tlen = plus != NULL ? (size_t)(plus - p) : (size_t)(end - p);
        if (tlen > 0) {
            apply_token(&step.pad, p, tlen);
        }
        p += tlen + (plus != NULL ? 1 : 0);
    }
    if (!add_step(&step)) {
        fprintf(stderr, "[pad_script] out of memory\n");
    }
}

/* Steps separated by ',' or line ends; '#' comments to the end of the line; spaces and tabs ignored */
static void parse_text(const char* text) {
    char token[256];
    size_t n = 0;
    int comment = 0;
    for (const char* p = text;; p++) {
        char c = *p;
        if (c == '\0' || c == ',' || c == '\n' || c == '\r') {
            if (n > 0) {
                /* end the step here: the buffer is reused, and without the terminator a value read past the step
                 * into a longer earlier step's leftovers ("+63:SY=27" after "+4328:SX=27" read as 2727, verified) */
                token[n] = '\0';
                parse_step(token, n);
            }
            n = 0;
            if (c == '\n' || c == '\r') {
                comment = 0;
            }
            if (c == '\0') {
                break;
            }
            continue;
        }
        if (c == '#') {
            comment = 1;
        }
        if (comment) {
            continue;
        }
        /* spaces matter only inside a debug command (WHEN:!warp 0x1 0) */
        if ((c == ' ' || c == '\t') && !(n >= 2 && memchr(token, '!', n) != NULL && memchr(token, ':', n) != NULL)) {
            continue;
        }
        if (n < sizeof(token) - 1) {
            token[n++] = c;
        }
    }
}

static void parse(void) {
    const char* env = getenv("ORRE_PAD_SCRIPT");
    s_count = 0;
    if (env == NULL || *env == '\0') {
        return;
    }
    /* A file is "@file:PATH" or (2026-10-18) a plain path: a value ending in ".pad" or containing a slash. A bare path
     * used to be parsed as script text, gave 0 steps, and an 8-minute recording idled in the title demo (B). */
    size_t n = strlen(env);
    const char* path = NULL;
    if (strncmp(env, "@file:", 6) == 0) {
        path = env + 6;
    } else if ((n > 4 && (strcmp(env + n - 4, ".pad") == 0 || strcmp(env + n - 4, ".PAD") == 0)) ||
               strchr(env, '/') != NULL || strchr(env, '\\') != NULL) {
        path = env;
    }
    if (path != NULL) {
        FILE* f = fopen(path, "rb");
        long size;
        char* text = NULL;
        if (f != NULL) {
            fseek(f, 0, SEEK_END);
            size = ftell(f);
            fseek(f, 0, SEEK_SET);
            text = (char*)malloc((size_t)size + 1);
            if (text != NULL) {
                size = (long)fread(text, 1, (size_t)size, f);
                text[size] = '\0';
                parse_text(text);
                free(text);
            }
            fclose(f);
        } else {
            fprintf(stderr, "[pad_script] cannot open '%s'\n", path);
        }
    } else {
        parse_text(env);
    }
    fprintf(stderr, "[pad_script] %d steps%s%s\n", s_count, path != NULL ? " from " : "", path != NULL ? path : "");
    if (s_count == 0) {
        /* fail fast: a script was asked for but nothing usable came of it, so the run would only idle (2026-10-18) */
        fprintf(stderr, "[pad_script] ERROR: ORRE_PAD_SCRIPT is set but gave no steps: stopping the run (exit 3) "
                        "instead of running unscripted\n");
        stop_run(3);
    }
}

/* "@R+N" counts from the first entry of R after the room entry the previous "@" step used (2026-10-18). It used
 * to be "the first entry of R at or after the previous step", which broke whenever the PC reached a room sooner than
 * the recording did: a Dolphin movie's last title press came before the main menu opened, but on the PC (instant
 * card check) the menu opened 2 frames before that press, so the next "@0x3A1+N" waited for a second opening and the
 * replay stood at the main menu. Counting entries keeps a return to the same room meaning "the next visit". */
static int s_last_anchor = -1; /* the entry number (index into the s_rooms sequence) the last "@" step used */
static int s_due_anchor = -1;  /* the entry the step step_due just accepted keyed to */

/* When the step can fire, or 0 if it is still waiting for a room */
static int step_due(const PadStep* step, unsigned frame) {
    int i;
    switch (step->when) {
    case WHEN_FRAME:
        return frame >= step->frames;
    case WHEN_AFTER_PREV:
        return frame >= s_prev_fired + step->frames;
    default:
        i = s_nrooms > ROOM_LOG ? s_nrooms - ROOM_LOG : 0;
        if (i < s_last_anchor + 1) {
            /* Window anchors opened in the same frame as the previous step's window anchor count too, unless a step
             * already used them (2026-10-26): the console's recorder lists a frame's ready windows in window-list
             * order, the PC logs them as they happen, and a window that reopens ready (windowOpen) is logged before
             * menuDaemon's phase 1 -> 2 for the others. P12: 0x45 logged before 0x50 in one frame, so "@0x20045" after
             * "@0x20050" missed it, fell back 30 frames later, and a battle seed landed late (a crit the console
             * didn't roll) */
            int first = i;
            i = s_last_anchor + 1;
            if (s_last_anchor >= first && (s_rooms[s_last_anchor % ROOM_LOG].room & 0xF0000u) == 0x20000u &&
                (step->room & 0xF0000u) == 0x20000u) {
                unsigned f = s_rooms[s_last_anchor % ROOM_LOG].frame;
                int j;
                for (j = s_last_anchor - 1; j >= first && s_rooms[j % ROOM_LOG].frame == f; j--) {
                }
                for (j++; j < s_last_anchor; j++) {
                    if (s_rooms[j % ROOM_LOG].room == step->room && !s_rooms[j % ROOM_LOG].used) {
                        s_due_anchor = j;
                        return frame >= f + step->frames;
                    }
                }
            }
        }
        for (; i < s_nrooms; i++) {
            unsigned at = s_rooms[i % ROOM_LOG].frame;
            if (s_rooms[i % ROOM_LOG].room == step->room) {
                s_due_anchor = i;
                return frame >= at + step->frames;
            }
        }
        /* "~G" (2026-10-19; snatcherLeo's option A, battle-tolerant replays): a battle window that doesn't open on the PC
         * (a message the console showed after a damage roll that came out differently) must not stall the rest of the
         * script: the step fires G frames after the previous one, keeps the anchor it had, and says so */
        if (step->fallback != 0 && frame >= s_prev_fired + step->fallback) {
            fprintf(stderr, "[pad_script] frame %u: anchor 0x%X did not come within %u frames of the previous step; "
                            "the step fires without it\n", frame, step->room, step->fallback);
            s_due_anchor = s_last_anchor;
            return 1;
        }
        return 0;
    }
}

/* Removed 2026-11-01 (snatcherLeo's OK; the project notes): ORRE_PAD_SEED_AT_HOOK, a room's own
 * "@<room>+0:!seed X" step run inside the room hook (an early P01 seed experiment; no final replay uses it) */

/* After the last step (snatcherLeo, 2026-10-19: a scripted start kept his controller locked out): once the last step's state
 * has been held HANDOVER_FRAMES, the script lets go of port 1 and real input reaches the game again, so a script can
 * bring the game to a point and hand it over. Unattended runs (ORRE_WINDOW=minimized, the standing rule for test
 * runs) keep real input off to the end, so a person's controller never reaches a test. ORRE_PAD_HANDOVER=1 / 0 forces
 * either way. */
enum { HANDOVER_FRAMES = 10 };
static void script_finished(unsigned frame) {
    static int done;
    const char* env;
    const char* mode;
    int hand_over;
    if (done || s_count <= 0 || s_next < s_count || frame < s_prev_fired + HANDOVER_FRAMES) {
        return;
    }
    done = 1;
    env = getenv("ORRE_PAD_HANDOVER");
    mode = getenv("ORRE_WINDOW");
    hand_over = env != NULL ? atoi(env) != 0 : !(mode != NULL && strcmp(mode, "minimized") == 0);
    if (hand_over) {
        extern void pc_vi_pace_real_time(void); /* pc_vi.c: a fast replay slows to real time for the person */
        PADSetVirtualExclusive(FALSE);
        PADClearVirtualStatus(0);
        pc_vi_pace_real_time();
        fprintf(stderr, "[pad_script] frame %u: the script is finished: real controllers and the keyboard work again\n",
                frame);
    } else {
        fprintf(stderr, "[pad_script] frame %u: the script is finished; real input stays off (an unattended run: "
                        "ORRE_WINDOW=minimized, or ORRE_PAD_HANDOVER=0)\n",
                frame);
    }
}

/* Each game frame (pc_vi.c, VIWaitForRetrace) */
void pc_pad_script_frame(unsigned frame) {
    static unsigned check_at;
    s_frame_now = frame;
    if (s_count < 0) {
        parse();
    }
    if (s_nchecks < 0) {
        route_checks_parse();
    }
    route_checks_frame(frame);
    if (check_at != 0 && frame == check_at) { /* what a game read sees now (PADRead only reads) */
        PADStatus all[PAD_CHANMAX];
        PADRead(all);
        fprintf(stderr, "[pad_script] frame %u: PADRead port 1 buttons 0x%04x stick %d,%d err %d\n", frame,
                (unsigned)all[0].button, (int)all[0].stickX, (int)all[0].stickY, (int)all[0].err);
        check_at = 0;
    }
    static int mash = 0;   /* a MASH step is in force: A taps until the next step */
    static unsigned mash_from;
    while (s_next < s_count && step_due(&s_steps[s_next], frame)) {
        const PADStatus* pad = &s_steps[s_next].pad;
        if (s_steps[s_next].when == WHEN_AFTER_ROOM) {
            if (s_due_anchor >= 0) {
                s_rooms[s_due_anchor % ROOM_LOG].used = 1;
            }
            if (s_due_anchor > s_last_anchor) { /* a same-frame window anchor before it keeps the later one */
                s_last_anchor = s_due_anchor;
            }
        }
        if (s_steps[s_next].command == NULL) {
            mash = 0; /* any pad step ends a mash (a debug command doesn't) */
        }
        if (s_steps[s_next].mash) {
            fprintf(stderr, "[pad_script] frame %u: step %d mash A until the next step\n", frame, s_next + 1);
            mash = 1;
            mash_from = frame;
            s_prev_fired = frame;
            s_next++;
            continue;
        }
        if (s_steps[s_next].command != NULL) {
            fprintf(stderr, "[pad_script] frame %u: step %d debug command '%s'\n", frame, s_next + 1,
                    s_steps[s_next].command);
            pc_debug_queue(s_steps[s_next].command);
            s_prev_fired = frame;
            s_next++;
            continue;
        }
        PADStatus status = *pad;
        PADSetVirtualExclusive(TRUE); /* the script alone: a real controller or the keyboard never reaches the game */
        PADSetVirtualStatus(0, &status);
        fprintf(stderr, "[pad_script] frame %u: step %d buttons 0x%04x stick %d,%d c %d,%d triggers %u,%u\n", frame,
                s_next + 1, (unsigned)pad->button, (int)pad->stickX, (int)pad->stickY, (int)pad->substickX,
                (int)pad->substickY, (unsigned)pad->triggerLeft, (unsigned)pad->triggerRight);
        s_prev_fired = frame;
        s_next++;
        check_at = frame + 2;
    }
    if (mash) {
        PADStatus status;
        memset(&status, 0, sizeof(status));
        status.err = PAD_ERR_NONE;
        status.button = ((frame - mash_from) / 4) % 2 == 0 ? PAD_BUTTON_A : 0;
        PADSetVirtualExclusive(TRUE);
        PADSetVirtualStatus(0, &status);
    }
    script_finished(frame);
}
