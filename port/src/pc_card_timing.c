/*
 * pc_card_timing.c: console-like memory-card timing (2026-10-19, snatcherLeo's S02 replay sync: the PC reached the Continue
 * screen's next room 53 frames before Dolphin, with the disc reads already on Dolphin's times).
 *
 * The game's card code (memcard.c) is a per-frame state machine: each state starts an async CARD operation and the
 * next state polls CARDGetResultCode every frame until it stops answering CARD_RESULT_BUSY (no completion callbacks;
 * verified from memcard.c). Aurora finishes every operation at once, so on the PC each state took at most a frame. On
 * the GameCube the card keeps the channel busy while it transfers. Here each operation keeps its channel busy
 * (Aurora's busy query, aurora_card_set_timing_hooks) until a due retrace:
 *   - with ORRE_CARD_TRACE=<the card capture for the recording> (one row per change: frame, task_kind, state,
 *     resume_state, sdk_result, ...): the recording's busy stretches (the SDK's result -1 from one row to the next
 *     row where it isn't), matched in order to the PC's operations by kind (the game state the stretch started in:
 *     3 = mount, 5 = check, 27 / 30 / 34 = read), and each takes its recorded number of fields;
 *   - otherwise a model: a fixed cost per operation plus the transfer for reads and writes. *Guess:* the numbers are
 *     placeholders sized so the Continue screen's operations (mount, check, a 16 KB read, three 512-byte reads, a
 *     120 KB read) add up to about the 53 missing frames; the card capture replaces them.
 * On whenever disc reads take the console's time (pc_dvd.c: replays with ORRE_DVD_TRACE, and fast loading off, the
 * GameCube-accurate preset). Normal play keeps instant card operations. Each operation is logged ("[card] ...").
 */
#include <aurora/card.h>
#include <dolphin/card.h>
#include <dolphin/types.h>

#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern u32 VIGetRetraceCount(void);            /* pc_vi.c */
extern volatile u32 pc_vi_heartbeat;           /* pc_vi.c: the game frame count */
extern int pc_dvd_console_timing(void);        /* pc_dvd.c */

typedef struct {
    char kind[8]; /* "mount", "check", "read", or "" (any) */
    int state;    /* the game state the stretch started in (the polling state) */
    u32 frames;
} CardStretch;

/* The game's memcard task (memcard.c's MemcardTaskState; state at +0x0C). Weak, so the test programs link without the
 * game. When a call starts, the game is still in the issuing state; it moves to the polling state (+1: 2 -> 3 mount,
 * 4 -> 5 check, 26 -> 27, 29 -> 30, 33 -> 34 reads) right after, so a recorded stretch is matched on that exact
 * state: a short operation the once-a-frame capture missed can't take a longer one's time. The game exe registers
 * the task pointer's address (pc_game_main.c), so the test programs link without the game. */
static void* const* s_task;
void pc_card_timing_set_task(void* const* task) { s_task = task; }
static int game_state(void) {
    if (s_task == NULL || *s_task == NULL) {
        return -1;
    }
    return *(const s32*)((const u8*)*s_task + 0x0C);
}

static CardStretch* s_trace;
static int s_trace_count, s_trace_cursor, s_trace_loaded;
static u32 s_busy_until[2];
static int s_on = -1;

static const char* kind_of_state(int state) {
    switch (state) {
    case 3:
        return "mount";
    case 5:
        return "check";
    case 27:
    case 30:
    case 34:
        return "read";
    default:
        return "";
    }
}

static void trace_add(int state, u32 frames) {
    static int cap;
    if (s_trace_count == cap) {
        CardStretch* grown;
        cap = cap ? cap * 2 : 64;
        grown = (CardStretch*)realloc(s_trace, (size_t)cap * sizeof(CardStretch));
        if (grown == NULL) {
            return;
        }
        s_trace = grown;
    }
    strncpy(s_trace[s_trace_count].kind, kind_of_state(state), sizeof(s_trace[0].kind) - 1);
    s_trace[s_trace_count].kind[sizeof(s_trace[0].kind) - 1] = '\0';
    s_trace[s_trace_count].state = state;
    s_trace[s_trace_count].frames = frames;
    s_trace_count++;
}

/* the memcard capture (<section>_memcard.csv): one row per change, columns frame, task, task_kind, state,
 * resume_state, card_result, random_delay, sdk_result, room, room_frame (a header line is skipped). A busy stretch is
 * the SDK result -1 from one row to the next row where it isn't; a state change while busy starts the next operation
 * (the save's mount and read run back to back: 13540-13545 mount, 13545-13547 read, S02). */
static void trace_load(void) {
    const char* path = getenv("ORRE_CARD_TRACE");
    FILE* f;
    char line[256];
    int busy = 0, busy_state = 0;
    unsigned busy_from = 0;
    s_trace_loaded = 1;
    if (path == NULL || *path == '\0') {
        return;
    }
    f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "[card] ORRE_CARD_TRACE: cannot open '%s': card operations use the model\n", path);
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        unsigned frame;
        char task[24];
        int task_kind, state, resume_state, card_result, random_delay, sdk_result;
        if (sscanf(line, "%u,%23[^,],%d,%d,%d,%d,%d,%d", &frame, task, &task_kind, &state, &resume_state, &card_result,
                   &random_delay, &sdk_result) != 8) {
            continue;
        }
        (void)task_kind;
        (void)resume_state;
        (void)card_result;
        (void)random_delay;
        if (sdk_result == -1) {
            if (busy && state != busy_state) {
                trace_add(busy_state, frame - busy_from);
                busy = 0;
            }
            if (!busy) {
                busy = 1;
                busy_from = frame;
                busy_state = state;
            }
        } else if (busy) {
            busy = 0;
            trace_add(busy_state, frame - busy_from);
        }
    }
    fclose(f);
    fprintf(stderr, "[card] ORRE_CARD_TRACE: %d busy stretches from %s: card operations take the recording's time\n",
            s_trace_count, path);
}

/* the model: fields for one operation. Likely (one capture, the S02 Dolphin replay, 2026-10-19; once-a-frame, so +-1):
 * mount 4-5 fields; reads about 7.9 KB a field (the 110 KB save in 14, the 16 KB icon file in 1-2); the 122 KB save
 * write 80 fields with its erases (about 1.6 KB a field); the check and the 512-byte header reads never showed busy
 * (under a field). Corrected 2026-10-19: the first numbers were a guess (mount 8, check 4, 4 KB a field for reads). */
static u32 model_frames(const char* op, s32 length) {
    if (strcmp(op, "mount") == 0) {
        return 5;
    }
    if (strcmp(op, "check") == 0) {
        return 0;
    }
    if (strcmp(op, "read") == 0) {
        return (u32)length / 7900;
    }
    if (strcmp(op, "write") == 0) {
        return 2 + (u32)length / 1600;
    }
    return 1; /* create, delete, format, set status / attributes, rename: not measured yet */
}

/* *Changed 2026-11-20:* follows the disc timing at every operation (it was read once per run), so the live
 * "Fast loading" row applies to the card too from its next operation; logged when it changes. Every OS the same. */
static int timing_on(void) {
    const int on = pc_dvd_console_timing();
    if (on != s_on) {
        if (on || s_on >= 0) { /* the first check with fast loading on stays quiet, as before */
            fprintf(stderr, on ? "[card] console-like card timing on: each operation keeps the card busy for its time\n"
                               : "[card] console-like card timing off (fast loading): card operations finish at once\n");
        }
        s_on = on;
    }
    return s_on;
}

/* when the game last touched the memory card (every operation, timing on or off): Quit to hub waits while the game is
 * saving (pc_settings_overlay.cpp; snatcherLeo, 2026-11-13) */
static Uint64 s_last_card_op_ms;
int pc_card_used_within(unsigned ms) { return s_last_card_op_ms != 0 && SDL_GetTicks() - s_last_card_op_ms < ms; }

static void op_started(s32 chan, const char* op, s32 length) {
    u32 now, start, frames;
    const char* how = "modelled";
    s_last_card_op_ms = SDL_GetTicks();
    if (chan < 0 || chan > 1 || !timing_on()) {
        return;
    }
    if (!s_trace_loaded) {
        trace_load();
    }
    frames = model_frames(op, length);
    if (s_trace_count > 0) {
        frames = 0; /* replaying: an operation the recording never saw busy finished inside a field there */
        how = "not busy in the recording";
    }
    {
        const int issuing = game_state();
        for (int i = s_trace_cursor; i < s_trace_count && i < s_trace_cursor + 4; i++) {
            const int match = issuing >= 0 ? s_trace[i].state == issuing + 1
                                           : (s_trace[i].kind[0] == '\0' || strcmp(s_trace[i].kind, op) == 0);
            if (match) {
                frames = s_trace[i].frames;
                s_trace_cursor = i + 1;
                how = "recorded";
                break;
            }
        }
    }
    now = VIGetRetraceCount();
    start = (s32)(s_busy_until[chan] - now) > 0 ? s_busy_until[chan] : now;
    s_busy_until[chan] = start + frames;
    fprintf(stderr, "[card] game frame %u: slot %c %s (%d B, game state %d): busy for %u fields (%s), until retrace "
                    "%u\n",
            pc_vi_heartbeat, 'A' + chan, op, (int)length, game_state(), frames, how, s_busy_until[chan]);
}

static bool busy(s32 chan) {
    return chan >= 0 && chan <= 1 && s_on > 0 && (s32)(s_busy_until[chan] - VIGetRetraceCount()) > 0;
}

void pc_card_timing_init(void) { aurora_card_set_timing_hooks(op_started, busy); }

/* pc_vi.c, each game frame: with card timing on (or ORRE_CARD_LOG=1), the game's card task in the format of the
 * memcard capture, one line per change, so the PC and Dolphin can be compared state by state (2026-10-19: the
 * remaining 12 frames of the S02 Continue load are outside the card's busy time) */
void pc_card_timing_frame(unsigned frame) {
    static int log = -1;
    static s32 last[4] = {-99, -99, -99, -99};
    const u8* task;
    s32 now[4];
    if (log < 0) {
        const char* env = getenv("ORRE_CARD_LOG");
        log = env != NULL ? atoi(env) != 0 : -2;
    }
    if (log == -2) {
        if (s_on < 0) {
            return; /* not decided yet: decided at the first card operation */
        }
        log = s_on > 0;
    }
    if (!log || s_task == NULL) {
        return;
    }
    task = (const u8*)*s_task;
    if (task == NULL) {
        return;
    }
    now[0] = *(const s32*)(task + 0x00); /* task_kind */
    now[1] = *(const s32*)(task + 0x0C); /* state */
    now[2] = *(const s32*)(task + 0x10); /* resume_state */
    now[3] = *(const s32*)(task + 0x28); /* card_result */
    if (memcmp(now, last, sizeof(now)) != 0) {
        memcpy(last, now, sizeof(now));
        fprintf(stderr, "[card] task frame %u: kind %d state %d resume %d card_result %d sdk %d\n", frame, (int)now[0],
                (int)now[1], (int)now[2], (int)now[3], CARDGetResultCode(0));
    }
}
