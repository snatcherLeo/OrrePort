/*
 * pc_debug.c: the debug toolkit's command queue and command language (see pc_debug.h). Port side only; the game side
 * registers its functions with pc_debug_register().
 */
#include "pc_debug.h"
#include "pc_crash.h"
#include "pc_cheats.h" /* the "cheat" command */
#include "pc_options.h" /* pc_option_gamecube_accurate: the hotkeys' accurate-mode gate */
#include "pc_paths.h" /* spots.json's places (4.1) */

void pc_heap_report(void); /* pc_heap.c: the game's SDK heaps */

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_timer.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define QUEUE_MAX 32
#define LINE_MAX_LEN 160
#define LOG_MAX 64

static const PcDebugApi* s_api;
static int s_enabled = -1;
static SDL_Mutex* s_lock;
static char s_queue[QUEUE_MAX][LINE_MAX_LEN];
static int s_queue_head, s_queue_count;
static char s_log[LOG_MAX][LINE_MAX_LEN];
static int s_log_total;
static int s_startup_done;
static int s_title_seen;         /* the title room has been entered (pc_debug_room_entered) */
static unsigned s_title_frame;   /* the frame it was first seen in pc_debug_frame */
static unsigned s_frame;         /* the current frame (pc_debug_frame) */
static int s_pending_load = -1;  /* a checkpoint load waiting for the title, or -1 */
static unsigned s_title_groups;  /* which of the title's sound groups have been pushed (bits, title_group_bit) */
static unsigned s_groups_frame;  /* the frame the last of them arrived */
static void jump(const char* id);                                    /* quick jumps, below */
static int s_paused, s_step;                                         /* the Pause key / 'pause' command, below */
static void toggle_pause(void);
void pc_debug_step(void);
extern void pc_frame_dump_request_pause_capture(void (*done)(const unsigned char* rgba, int width, int height));
void (*pc_debug_audio_pause)(int on); /* pc_game_main.c: MusyX's PC output (sndPCPauseAudio); NULL elsewhere */
extern void pc_frame_dump_request_screenshot(void);                  /* pc_frame_dump.c */
static void write_slot_label(int slot, const char* label);          /* named checkpoint slots, below */
extern const char* pc_platform_data_path(const char* name, char* out, size_t size); /* pc_platform.c */
extern const char* pc_platform_checkpoints_dir(char* out, size_t size); /* pc_platform.c: checkpoints' folder (4.1) */
#define TITLE_ROOM 0x39A
/* The title screen pushes MusyX groups 5, 1 and 8 while it loads (room hook first, then the pushes over the next
 * frames; a load right at the room hook landed mid-load and the game went on to room 0x384). A load waits until all
 * three are pushed, plus a short margin, however long the PC takes; if they never arrive (another boot path), it
 * goes ahead 10 s after the title room. */
#define TITLE_GROUPS_ALL 7u
#define TITLE_MARGIN_FRAMES 30
#define TITLE_FALLBACK_FRAMES 600

static unsigned title_group_bit(unsigned gid) { return gid == 5 ? 1u : gid == 1 ? 2u : gid == 8 ? 4u : 0u; }

/* pc_game_main.c connects this to the MusyX bridge's push listener */
void pc_debug_sound_group_pushed(unsigned gid) {
    if (s_title_seen != 0 && title_group_bit(gid) != 0 && s_title_groups != TITLE_GROUPS_ALL) {
        s_title_groups |= title_group_bit(gid);
        if (s_title_groups == TITLE_GROUPS_ALL) {
            s_groups_frame = s_frame;
        }
    }
}

/* the title's sound setup is done: all three groups pushed and the margin passed, or the fallback time */
static int title_ready(void) {
    if (s_title_seen != 2) {
        return 0;
    }
    if (s_title_groups == TITLE_GROUPS_ALL) {
        return s_frame - s_groups_frame >= TITLE_MARGIN_FRAMES;
    }
    return s_frame - s_title_frame >= TITLE_FALLBACK_FRAMES;
}

static SDL_Mutex* lock(void) {
    if (s_lock == NULL) {
        s_lock = SDL_CreateMutex();
    }
    return s_lock;
}

/* On with ORRE_DEBUG=1 (test runs, set before start), or with Settings › General › Debug tools (settings.ini
 * debug_tools = 1; snatcherLeo, 2026-11-13: "Can you enable the Debug menu to be opened firstly, within our build?"): a
 * build started by double-click had no way to turn it on. ORRE_DEBUG, when set at all, decides alone (ORRE_DEBUG=0
 * keeps it off whatever is saved), and an automated run never takes the saved setting, so recorded checks run as
 * before. Read once: the toolkit sets itself up at start, so the row says "Applies the next time OrrePort starts".
 * Platform-neutral. */
int pc_platform_automated_run(void); /* pc_platform.c */
int pc_settings_int(const char* key, int fallback); /* pc_settings.c */
int pc_debug_enabled(void) {
    if (s_enabled < 0) {
        const char* v = getenv("ORRE_DEBUG");
        if (v != NULL && *v != '\0') {
            s_enabled = strcmp(v, "0") != 0;
        } else {
            /* *Corrected 2026-11-13 (night):* off by default again (snatcherLeo: "I don't think even playtesters need debug
             * menu. We can let users know tho, if they want access to it, how to get it"; earlier the same day it was
             * on by default: "On default, Debug tools should always be on"). It can warp, set story flags and give
             * items, so a player could break a save without meaning to. It is turned on by hand, settings.ini
             * debug_tools = 1 (the public guide says how), and then Settings shows its row so it can be turned off */
            s_enabled = !pc_platform_automated_run() && pc_settings_int("debug_tools", 0) != 0;
        }
    }
    return s_enabled;
}

void pc_debug_register(const PcDebugApi* api) {
    s_api = api;
    fprintf(stderr, "[debug] game side registered\n");
}

const PcDebugApi* pc_debug_api(void) { return s_api; }

static void logf_(const char* fmt, ...) {
    char line[LINE_MAX_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[debug] %s\n", line);
    snprintf(s_log[s_log_total % LOG_MAX], LINE_MAX_LEN, "%s", line);
    s_log_total++;
}

int pc_debug_log_count(void) { return s_log_total < LOG_MAX ? s_log_total : LOG_MAX; }

const char* pc_debug_log_line(int i) {
    int first = s_log_total > LOG_MAX ? s_log_total - LOG_MAX : 0;
    if (i < 0 || i >= pc_debug_log_count()) {
        return NULL;
    }
    return s_log[(first + i) % LOG_MAX];
}

/* Run one command at once on the calling (game) thread, not at the next frame boundary (2026-10-27; its first
 * user, ORRE_PAD_SEED_AT_HOOK, was removed 2026-11-01, and the helper stays for debug use) */
static void run(char* line);
void pc_debug_run_now(const char* line) {
    char buf[256];
    if (!pc_debug_enabled() || line == NULL) {
        return;
    }
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    run(buf);
}

void pc_debug_queue(const char* line) {
    const char* p = line;
    if (!pc_debug_enabled()) {
        fprintf(stderr, "[debug] ignored (ORRE_DEBUG is off): %s\n", line);
        return;
    }
    SDL_LockMutex(lock());
    while (*p != '\0') {
        size_t len = strcspn(p, ";");
        while (len > 0 && (*p == ' ' || *p == '\t')) {
            p++;
            len--;
        }
        if (len > 0) {
            if (s_queue_count < QUEUE_MAX) {
                char* slot = s_queue[(s_queue_head + s_queue_count++) % QUEUE_MAX];
                size_t n = len < LINE_MAX_LEN - 1 ? len : LINE_MAX_LEN - 1;
                memcpy(slot, p, n);
                slot[n] = '\0';
            } else {
                fprintf(stderr, "[debug] queue full, dropped: %.*s\n", (int)len, p);
            }
        }
        p += len;
        if (*p == ';') {
            p++;
        }
    }
    SDL_UnlockMutex(lock());
}

static int parse_u32(const char* s, PcDebugU32* out) {
    char* end;
    unsigned long v;
    if (s == NULL) {
        return 0;
    }
    v = strtoul(s, &end, 0);
    if (end == s) {
        return 0;
    }
    *out = (PcDebugU32)v;
    return 1;
}

/* src/game/pc_debug_game.c: a plain function rather than a PcDebugApi slot, so neither side's struct changes;
 * weak, so builds without it (the tests, an older game side) still link and the command says it is missing */
extern int pc_debug_game_talk(unsigned group, unsigned index) __attribute__((weak));
/* the game side's own commands (2026-10-18: givefull, moves, name): returns 1 when it knew the command, *ok = result */
extern int pc_debug_game_command(int argc, char** argv, int* ok) __attribute__((weak));
/* MusyX (the game's sound library); weak, since the test programs link this file without it */
extern void sndMasterVolume(unsigned char volume, unsigned short time, unsigned char music, unsigned char fx)
    __attribute__((weak));

static int on_off(const char* s) { return s != NULL && (strcmp(s, "on") == 0 || strcmp(s, "1") == 0); }

/* "not available" when the game side has not registered or left the entry NULL */
#define NEED(fn)                                                                                                        \
    if (s_api == NULL || s_api->fn == NULL) {                                                                          \
        logf_("%s: not available (%s)", cmd,                                                                           \
              s_api == NULL ? "game side not connected" : "this build's game side does not provide " #fn);              \
        return;                                                                                                        \
    }

static void run(char* line) {
    char* argv[16]; /* "save N label words..." */
    int argc = 0;
    char* cmd;
    PcDebugU32 a = 0, b = 0;
    for (char* tok = strtok(line, " \t"); tok != NULL && argc < 16; tok = strtok(NULL, " \t")) {
        argv[argc++] = tok;
    }
    if (argc == 0) {
        return;
    }
    cmd = argv[0];
    if (strcmp(cmd, "help") == 0) {
        logf_("warp ROOM [ENTRY] | flag ID [VALUE] | item ID COUNT | pokemon SPECIES LEVEL | talk GROUP INDEX | team NAME "
              "| battle win|lose "
              "| seed N | speed PERCENT | skipmovies on|off | save SLOT | load SLOT | fight ID | heaps");
    } else if (strcmp(cmd, "crash") == 0) {
        /* tests the crash report and the run log (pc_log.c): an access violation on purpose */
        logf_("crash: writing to address 0 on purpose");
        *(volatile int*)(uintptr_t)0 = 0;
    } else if (strcmp(cmd, "stack") == 0) {
        /* where is it stuck: stack samples of the game thread, one a second, in the log */
        a = 5;
        if (argc >= 2) {
            parse_u32(argv[1], &a);
        }
        logf_("stack: %s", pc_crash_request_samples(a) ? "sampling (see the log)" : "no watchdog running");
    } else if (strcmp(cmd, "speed") == 0) {
        if (argc < 2 || !parse_u32(argv[1], &a) || a < 10 || a > 1000) {
            logf_("speed: give a percent, 10..1000 (100 = normal)");
            return;
        }
        pc_vi_set_speed_percent(a);
        logf_("speed %u%%", a);
    } else if (strcmp(cmd, "warp") == 0) {
        NEED(warp);
        if (argc < 2 || !parse_u32(argv[1], &a)) {
            logf_("warp: give a room id");
            return;
        }
        if (argc >= 3) {
            parse_u32(argv[2], &b);
        }
        logf_("warp 0x%X entry %u: %s", a, b, s_api->warp(a, b) ? "ok" : "refused");
    } else if (strcmp(cmd, "flag") == 0) {
        if (argc < 2 || !parse_u32(argv[1], &a)) {
            logf_("flag: give a flag id");
            return;
        }
        if (argc >= 3 && parse_u32(argv[2], &b)) {
            NEED(set_flag);
            logf_("flag %u = %u: %s", a, b, s_api->set_flag(a, b) ? "ok" : "refused");
        } else {
            PcDebugU32 v = 0;
            NEED(get_flag);
            if (s_api->get_flag(a, &v)) {
                logf_("flag %u is %u", a, v);
            } else {
                logf_("flag %u: refused", a);
            }
        }
    } else if (strcmp(cmd, "item") == 0) {
        NEED(give_item);
        if (argc < 3 || !parse_u32(argv[1], &a) || !parse_u32(argv[2], &b)) {
            logf_("item: give ID COUNT");
            return;
        }
        logf_("item %u x%u: %s", a, b, s_api->give_item(a, b) ? "ok" : "refused");
    } else if (strcmp(cmd, "pokemon") == 0) {
        NEED(give_pokemon);
        if (argc < 3 || !parse_u32(argv[1], &a) || !parse_u32(argv[2], &b)) {
            logf_("pokemon: give SPECIES LEVEL");
            return;
        }
        logf_("pokemon %u level %u: %s", a, b, s_api->give_pokemon(a, b) ? "ok" : "refused");
    } else if (strcmp(cmd, "musicvol") == 0 || strcmp(cmd, "fxvol") == 0) {
        /* B1 measurements (2026-10-18): the game's own master volume for all music (or all effects), 0..127, at once;
         * "musicvol 0" isolates the effects so each sound's level can be measured against Dolphin, where D sets the
         * same faders. MusyX's sndMasterVolume (the game's code); a later game call can change it again */
        const int music = strcmp(cmd, "musicvol") == 0;
        if (sndMasterVolume == NULL) {
            logf_("%s: not available (no MusyX in this program)", cmd);
            return;
        }
        if (argc < 2 || !parse_u32(argv[1], &a) || a > 127) {
            logf_("%s: give a volume 0..127", cmd);
            return;
        }
        sndMasterVolume((unsigned char)a, 0, music ? 1 : 0, music ? 0 : 1);
        logf_("%s %u: ok", cmd, a);
    } else if (strcmp(cmd, "talk") == 0) {
        /* the project's NPC-talk routes (2026-10-17): arms the game side's one-shot OrreHook_TalkTarget, so the next A press on
         * the field talks to that person through the game's normal talk path */
        if (pc_debug_game_talk == NULL) {
            logf_("talk: not available (this build's game side does not provide pc_debug_game_talk)");
            return;
        }
        if (argc < 3 || !parse_u32(argv[1], &a) || !parse_u32(argv[2], &b)) {
            logf_("talk: give GROUP INDEX");
            return;
        }
        logf_("talk %u %u: %s", a, b, pc_debug_game_talk(a, b) ? "ok" : "refused");
    } else if (strcmp(cmd, "cheat") == 0) {
        /* the Cheats menu's switches from the command line and pad scripts, so test routes can check each cheat
         * without clicking the menu (2026-11-13): cheat win|catch|money|items on|off, cheat exp 1|2|5|10,
         * cheat put 0..5 (Master Ball, Rare Candy, Max Revive, Max Elixir, Full Restore, Full Heal) */
        static const char* const names[PC_CHEAT_SWITCH_COUNT] = {"win", "catch", "money", "items"};
        int which = -1;
        if (argc >= 3 && strcmp(argv[1], "exp") == 0 && parse_u32(argv[2], &a)) {
            pc_cheat_set_exp_multiplier((int)a);
            logf_("cheat exp %u: now x%d", a, pc_cheat_exp_multiplier());
            return;
        }
        if (argc >= 3 && strcmp(argv[1], "put") == 0 && parse_u32(argv[2], &a) && (int)a < pc_cheat_put_count()) {
            pc_cheat_put_in_pc((int)a);
            logf_("cheat put %u: queued", a);
            return;
        }
        for (int i = 0; argc >= 3 && i < PC_CHEAT_SWITCH_COUNT; i++) {
            if (strcmp(argv[1], names[i]) == 0) which = i;
        }
        if (which < 0 || (strcmp(argv[2], "on") != 0 && strcmp(argv[2], "off") != 0)) {
            logf_("cheat: win|catch|money|items on|off, exp 1|2|5|10, put 0..5");
            return;
        }
        pc_cheat_set(which, strcmp(argv[2], "on") == 0);
        logf_("cheat %s %s: %s", argv[1], argv[2], pc_cheat_on(which) ? "on" : "off");
    } else if (strcmp(cmd, "team") == 0) {
        NEED(set_team);
        if (argc < 2) {
            logf_("team: give a preset name");
            return;
        }
        logf_("team %s: %s", argv[1], s_api->set_team(argv[1]) ? "ok" : "refused");
    } else if (strcmp(cmd, "battle") == 0) {
        NEED(battle_result);
        if (argc < 2 || (strcmp(argv[1], "win") != 0 && strcmp(argv[1], "lose") != 0)) {
            logf_("battle: win or lose");
            return;
        }
        logf_("battle %s: %s", argv[1], s_api->battle_result(strcmp(argv[1], "win") == 0) ? "ok" : "refused");
    } else if (strcmp(cmd, "seed") == 0) {
        NEED(set_rng_seed);
        if (argc < 2 || !parse_u32(argv[1], &a)) {
            logf_("seed: give a value");
            return;
        }
        /* "seed X keep A/B/..." (2026-10-26, P12's battle 207; ',' also read, but a pad script ends a step at
         * ','): a replay resync that leaves the game alone when
         * its random state is already one of the console's states around the seed's frame (the game is in step; the
         * seed's one sample can land inside a heavy logic pass that runs past the refresh, and would rewind it) */
        if (argc >= 4 && strcmp(argv[2], "keep") == 0 && s_api->get_rng_state != NULL) {
            PcDebugU32 now;
            if (s_api->get_rng_state(&now)) {
                const char* p = argv[3];
                while (*p != '\0') {
                    char* end;
                    unsigned long v = strtoul(p, &end, 0);
                    if (end == p) {
                        break;
                    }
                    if ((PcDebugU32)v == now) {
                        logf_("seed 0x%X: kept 0x%X (already one of the console's states)", a, now);
                        return;
                    }
                    p = *end == ',' || *end == '/' ? end + 1 : end;
                }
            }
        }
        logf_("seed 0x%X: %s", a, s_api->set_rng_seed(a) ? "ok" : "refused");
    } else if (strcmp(cmd, "npc") == 0) {
        /* "npc SLOT x y z tx ty tz pause yaw state sub" (2026-10-27): a replay resync that copies one wandering
         * NPC's state from the console's npc capture at one frame (frame-length timing moved its walk; P02) */
        float v[10];
        int i;
        NEED(set_npc);
        if ((argc != 5 && argc < 12) || !parse_u32(argv[1], &a)) {
            logf_("npc: SLOT x y z [tx ty tz pause yaw state sub]");
            return;
        }
        for (i = 0; i < 10; i++) {
            /* "npc SLOT x y z": the position only (slot 0 is the player: his state stays his own); NaN marks it */
            v[i] = 2 + i < argc ? strtof(argv[2 + i], NULL) : __builtin_nanf("");
        }
        logf_("npc %u: %s", a, s_api->set_npc(a, v) ? "ok" : "refused");
    } else if (strcmp(cmd, "skipmovies") == 0) {
        NEED(skip_movies);
        logf_("skipmovies %s: %s", on_off(argc >= 2 ? argv[1] : NULL) ? "on" : "off",
              s_api->skip_movies(on_off(argc >= 2 ? argv[1] : NULL)) ? "ok" : "refused");
    } else if (strcmp(cmd, "save") == 0 || strcmp(cmd, "load") == 0) {
        int save = cmd[0] == 's';
        if (argc < 2 || !parse_u32(argv[1], &a)) {
            logf_("%s: give a slot number", cmd);
            return;
        }
        if (save) {
            int ok;
            NEED(checkpoint_save);
            ok = s_api->checkpoint_save((int)a);
            logf_("save checkpoint %u: %s", a, ok ? "ok" : "refused");
            if (ok) { /* "save N some label": the rest of the line names the slot */
                char label[96] = "";
                for (int k = 2; k < argc; k++) {
                    size_t used = strlen(label);
                    snprintf(label + used, sizeof(label) - used, "%s%s", k > 2 ? " " : "", argv[k]);
                }
                write_slot_label((int)a, label);
            }
        } else {
            NEED(checkpoint_load);
            if (!title_ready()) {
                /* the title screen pushes sound groups the rest of the game needs (Willie's battle lost its cries and
                 * Poke Ball sounds after a load from boot, 2026-10-17): load once the title room has been entered */
                s_pending_load = (int)a;
                logf_("load checkpoint %u: waits for the title screen (its sound setup)", a);
                return;
            }
            logf_("load checkpoint %u: %s", a, s_api->checkpoint_load((int)a) ? "ok" : "refused");
        }
    } else if (strcmp(cmd, "screenshot") == 0) {
        pc_frame_dump_request_screenshot();
        logf_("screenshot: saving the next frame");
    } else if (strcmp(cmd, "pause") == 0) {
        toggle_pause();
    } else if (strcmp(cmd, "step") == 0) {
        pc_debug_step();
    } else if (strcmp(cmd, "jump") == 0) {
        NEED(warp);
        NEED(set_flag);
        if (argc < 2) {
            logf_("jump: give a spot id (e.g. jump step-6 for Willie's battle; the menu lists them)");
            return;
        }
        jump(argv[1]);
    } else if (strcmp(cmd, "fight") == 0) {
        NEED(fight_start);
        if (argc < 2 || !parse_u32(argv[1], &a)) {
            logf_("fight: give a battle id (e.g. fight 10 for Willie)");
            return;
        }
        logf_("fight %u: %s", a, s_api->fight_start(a) ? "ok" : "refused (a battle running, or not in a field room)");
    } else if (strcmp(cmd, "heaps") == 0) {
        pc_heap_report(); /* the game's SDK heaps: size, in use, peak, failures (pc_heap.c) */
    } else {
        int ok = 0;
        if (pc_debug_game_command != NULL && pc_debug_game_command(argc, argv, &ok)) {
            logf_("%s: %s", cmd, ok ? "ok" : "refused");
        } else {
            logf_("unknown command '%s' (try help)", cmd);
        }
    }
}

void pc_debug_frame(unsigned frame) {
    if (!pc_debug_enabled()) {
        return;
    }
    s_frame = frame;
    {
        /* ORRE_MUSIC_MUTE=1 (B1 measurements, 2026-10-18): all music at master volume 0 for the whole run, re-applied
         * every 30 frames in case the game sets it again, so the effects can be measured alone; D applies the same to
         * the game's music faders in Dolphin. Test switch only */
        static int mute = -1;
        if (mute < 0) {
            mute = getenv("ORRE_MUSIC_MUTE") != NULL;
            if (mute) {
                logf_("ORRE_MUSIC_MUTE: music at master volume 0 (re-applied every 30 frames)");
            }
        }
        if (mute && sndMasterVolume != NULL && frame % 30 == 0) {
            sndMasterVolume(0, 0, 1, 0);
        }
    }
    if (!s_startup_done) {
        const char* cmds = getenv("ORRE_DEBUG_CMD");
        s_startup_done = 1;
        logf_("debug toolkit on (F1: menu)%s", s_api != NULL ? "" : "; game side not connected yet");
        if (cmds != NULL) {
            pc_debug_queue(cmds);
        }
    }
    {
        /* ORRE_DEBUG_AT=@file:<path> (2026-10-27; replays only): "FRAME command" lines, each run once at that
         * frame (the pad script's frame count), whatever the pad script is doing. Logged resyncs that must land on an
         * exact frame (a battle roll's seed and blink timers, an NPC placement) no longer hang on pad steps, whose
         * frames move once a resync changes the battle's course */
        static int loaded;
        static unsigned* at_frame;
        static char** at_cmd;
        static int at_n, at_next;
        if (!loaded) {
            const char* env = getenv("ORRE_DEBUG_AT");
            loaded = 1;
            if (env != NULL && strncmp(env, "@file:", 6) == 0) {
                FILE* f = fopen(env + 6, "r");
                char buf[LINE_MAX_LEN];
                int cap = 0;
                while (f != NULL && fgets(buf, sizeof(buf), f) != NULL) {
                    char* end;
                    unsigned long fr = strtoul(buf, &end, 10);
                    if (end == buf || buf[0] == '#') {
                        continue;
                    }
                    while (*end == ' ' || *end == '\t') {
                        end++;
                    }
                    end[strcspn(end, "#\r\n")] = '\0'; /* a trailing "# ..." comment */
                    {
                        size_t len = strlen(end);
                        while (len > 0 && (end[len - 1] == ' ' || end[len - 1] == '\t')) {
                            end[--len] = '\0';
                        }
                    }
                    if (at_n == cap) {
                        cap = cap ? cap * 2 : 64;
                        at_frame = (unsigned*)realloc(at_frame, cap * sizeof(*at_frame));
                        at_cmd = (char**)realloc(at_cmd, cap * sizeof(*at_cmd));
                    }
                    at_frame[at_n] = (unsigned)fr;
                    at_cmd[at_n] = strdup(end);
                    at_n++;
                }
                if (f != NULL) {
                    fclose(f);
                }
                logf_("ORRE_DEBUG_AT: %d scheduled commands", at_n);
            }
        }
        while (at_next < at_n && frame >= at_frame[at_next]) {
            char line[LINE_MAX_LEN];
            strncpy(line, at_cmd[at_next], LINE_MAX_LEN - 1);
            line[LINE_MAX_LEN - 1] = '\0';
            logf_("frame %u: scheduled '%s'", frame, line);
            run(line);
            at_next++;
        }
    }
    for (;;) {
        char line[LINE_MAX_LEN];
        SDL_LockMutex(lock());
        if (s_queue_count == 0) {
            SDL_UnlockMutex(lock());
            break;
        }
        memcpy(line, s_queue[s_queue_head], LINE_MAX_LEN);
        s_queue_head = (s_queue_head + 1) % QUEUE_MAX;
        s_queue_count--;
        SDL_UnlockMutex(lock());
        run(line);
    }
    if (s_title_seen == 1) {
        s_title_seen = 2;
        s_title_frame = frame;
    }
    if (s_pending_load >= 0 && title_ready() && s_api != NULL && s_api->checkpoint_load != NULL) {
        int slot = s_pending_load;
        s_pending_load = -1;
        logf_("load checkpoint %d (after the title%s): %s", slot,
              s_title_groups == TITLE_GROUPS_ALL ? "'s sound groups" : "; its sound groups never arrived",
              s_api->checkpoint_load(slot) ? "ok" : "refused");
    }
}

/* Hotkeys (window-and-debug-plan.md, batch 2 items 3, 6, 7; keys approved by snatcherLeo 2026-10-17), read from SDL's
 * keyboard state once per frame (pc_vi.c), so they work with the menu closed: F5 saves and F9 loads the menu's
 * checkpoint slot, F12 (or Print Screen, if a debugger takes F12) saves a screenshot, Pause stops the game at the frame
 * boundary and '.' then lets exactly one frame run. Debug only (ORRE_DEBUG=1), never in GameCube-accurate mode. */
static int s_hotkey_slot;

void pc_debug_set_hotkey_slot(int slot) { s_hotkey_slot = slot; }
int pc_debug_paused(void) { return s_paused; }

static int key_pressed(const bool* keys, SDL_Scancode code, unsigned char* was) {
    const int now = keys[code] ? 1 : 0;
    const int edge = now && !was[code];
    was[code] = (unsigned char)now;
    return edge;
}

void pc_debug_hotkeys(void) {
    static unsigned char was[SDL_SCANCODE_COUNT];
    const bool* keys;
    if (!pc_debug_enabled()) {
        return;
    }
    keys = SDL_GetKeyboardState(NULL);
    if (keys == NULL) {
        return;
    }
    /* Never in GameCube-accurate mode (the comment above promised it; nothing enforced it until 2026-11-16, found when
     * the accurate-mode list was checked against the code): checkpoint save / load (F5 / F9), Pause and frame step
     * change what a console route can do, so an accurate save plays without them. The screenshot key stays: it only
     * reads the picture. Test runs that force debug with ORRE_DEBUG keep the keys (they drive the game by script, not
     * by these keys). All platforms the same. */
    if (pc_option_gamecube_accurate() && getenv("ORRE_DEBUG") == NULL) {
        if (key_pressed(keys, SDL_SCANCODE_F12, was) | key_pressed(keys, SDL_SCANCODE_PRINTSCREEN, was)) {
            pc_frame_dump_request_screenshot();
            logf_("screenshot: saving the next frame");
        }
        return;
    }
    if (key_pressed(keys, SDL_SCANCODE_F5, was)) {
        char line[32];
        snprintf(line, sizeof(line), "save %d", s_hotkey_slot);
        pc_debug_queue(line);
    }
    if (key_pressed(keys, SDL_SCANCODE_F9, was)) {
        char line[32];
        snprintf(line, sizeof(line), "load %d", s_hotkey_slot);
        pc_debug_queue(line);
    }
    if (key_pressed(keys, SDL_SCANCODE_F12, was) | key_pressed(keys, SDL_SCANCODE_PRINTSCREEN, was)) {
        pc_frame_dump_request_screenshot();
        logf_("screenshot: saving the next frame");
    }
    if (key_pressed(keys, SDL_SCANCODE_PAUSE, was)) {
        toggle_pause();
    }
    if (key_pressed(keys, SDL_SCANCODE_PERIOD, was)) {
        pc_debug_step();
    }
}

/* Pausing keeps a picture (the black-window report, 2026-10-17): Pause first captures the current frame through the
 * frame-dump readback (a frame or two), then holds the game; while held, pc_vi.c presents frames that draw that
 * picture full-size with the F1 menu over it (pc_debug_ui.cpp), so the window never goes black and the menu works.
 * If no picture arrives within 10 frames (readback unavailable), it pauses without one. */
enum { PAUSE_W = 640, PAUSE_H = 480 };
static unsigned char s_pause_image[PAUSE_W * PAUSE_H * 4];
static int s_pause_image_serial; /* bumped for each new picture, so the menu uploads it once */
static int s_pause_image_valid;
static int s_pause_pending;
static unsigned s_pause_wait;

static void enter_pause(void) {
    s_pause_pending = 0;
    s_paused = 1;
    if (pc_debug_audio_pause != NULL) {
        pc_debug_audio_pause(1);
    }
    logf_("paused at game frame %u%s", s_frame, s_pause_image_valid ? "" : " (no picture: readback unavailable)");
}

static void pause_captured(const unsigned char* rgba, int width, int height) {
    if (width != PAUSE_W || height != PAUSE_H) {
        return;
    }
    memcpy(s_pause_image, rgba, sizeof(s_pause_image));
    s_pause_image_valid = 1;
    s_pause_image_serial++;
    if (s_pause_pending) {
        enter_pause();
    }
}

static void toggle_pause(void) {
    if (s_paused || s_pause_pending) {
        s_paused = 0;
        s_pause_pending = 0;
        if (pc_debug_audio_pause != NULL) {
            pc_debug_audio_pause(0);
        }
        logf_("running from game frame %u", s_frame);
        return;
    }
    /* no capture any more: pc_vi.c re-presents the last finished frame itself (aurora_overlay_frame_present) */
    s_pause_image_valid = 1;
    enter_pause();
}

/* one frame while paused: the game runs a frame, and its picture is captured to become the paused picture */
void pc_debug_step(void) {
    if (!s_paused) {
        return;
    }
    s_step = 1; /* the stepped frame is presented normally, then re-presented while paused */
}

/* the paused picture for the menu: NULL unless paused with one; *serial changes when a new picture arrives */
const unsigned char* pc_debug_pause_image(int* serial) {
    /* 2026-10-17: superseded by re-presenting the real frame (pc_vi.c); kept for the frame-dump capture path, which
     * nothing requests any more, so this returns NULL and the menu draws no picture of its own */
    *serial = s_pause_image_serial;
    return NULL;
}

/* pc_vi.c, in VIWaitForRetrace: while paused, wait here (window events, hotkeys and the menu keep running; present()
 * shows the paused picture with the menu) until Pause again or '.' (one frame). Returns at once when not paused. */
void pc_debug_pause_point(void (*poll)(void), void (*present)(void), const int* exit_requested) {
    extern volatile unsigned int pc_vi_alive; /* pc_vi.c: the watchdog's counter keeps ticking while paused */
    if (s_pause_pending && ++s_pause_wait > 10) {
        enter_pause();
    }
    while (s_paused && !s_step && !*exit_requested) {
        ++pc_vi_alive;
        poll();
        pc_debug_hotkeys();
        pc_debug_frame(s_frame); /* menu buttons and commands ('pause', 'step') still work */
        present();
        SDL_Delay(15);
    }
    s_step = 0;
}

/* Quick jumps (window-and-debug-plan.md, batch 2 item 4): the spots.json (docs/testing/spots.json, built from the
 * story route by tools/local/testing/spots_build.py), one spot per route step: a name, the room id, the entry point,
 * the story flags to set and clear. Looked for next to the exe, then in the repository two folders up (the first
 * link's exe is in build-pc/firstlink). A jump sets the flags directly (up to ~290, too many for the command queue),
 * then warps; D labels every spot "likely right until a jump is checked in the game". */
enum { SPOT_MAX = 160, SPOT_FLAGS = 320 };
typedef struct {
    char id[24];
    char name[64];
    PcDebugU32 room, entry;
    unsigned short set[SPOT_FLAGS], clear[16];
    int nset, nclear;
} PcSpot;
static PcSpot s_spots[SPOT_MAX];
static int s_spot_count = -1; /* -1: not read yet */

/* the text of a top-level key's value inside one JSON object (no nesting of objects in spots.json's entries) */
static const char* json_value(const char* obj, const char* end, const char* key) {
    char pattern[40];
    const char* p;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    for (p = obj; p != NULL && p < end; p++) {
        p = strstr(p, pattern);
        if (p == NULL || p >= end) {
            return NULL;
        }
        p += strlen(pattern);
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p < end && *p == ':') {
            p++;
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
                p++;
            }
            return p;
        }
    }
    return NULL;
}

static void json_string(const char* v, char* out, size_t size) {
    size_t n = 0;
    if (v == NULL || *v != '"') {
        out[0] = '\0';
        return;
    }
    for (v++; *v != '\0' && *v != '"' && n + 1 < size; v++) {
        if (*v == '\\' && v[1] != '\0') {
            v++;
        }
        out[n++] = *v;
    }
    out[n] = '\0';
}

static int json_numbers(const char* v, const char* end, unsigned short* out, int max) {
    int n = 0;
    if (v == NULL || *v != '[') {
        return 0;
    }
    for (v++; v < end && *v != ']';) {
        char* next;
        unsigned long x = strtoul(v, &next, 10);
        if (next == v) {
            v++;
            continue;
        }
        if (n < max) {
            out[n++] = (unsigned short)x;
        }
        v = next;
    }
    return n;
}

static void load_spots(void) {
    /* *Updated 2026-11-21 (4.1):* ORRE_SPOTS=<file> first (a portable folder's app\ is not two folders below the
     * repository, so the old guess can't find it there), then spots.json in the test files' place (data\system\ in the
     * portable layout, the exe's folder in the flat one), then the repository's copy two folders above the exe
     * (build-pc/firstlink, the flat layout's dev build). Debug only; all OSes. */
    char path[1200];
    char* text = NULL;
    long size = 0;
    s_spot_count = 0;
    for (int i = 0; i < 3 && text == NULL; i++) {
        FILE* f;
        if (i == 0) {
            const char* env = getenv("ORRE_SPOTS");
            if (env == NULL || env[0] == '\0') continue;
            snprintf(path, sizeof(path), "%s", env);
        } else {
            pc_paths_get(i == 1 ? PC_PLACE_TEST : PC_PLACE_PROGRAM, i == 1 ? "spots.json" : "../../docs/testing/spots.json",
                         path, sizeof(path));
        }
        f = fopen(path, "rb");
        if (f == NULL) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        text = (char*)malloc((size_t)size + 1);
        if (text != NULL) {
            size = (long)fread(text, 1, (size_t)size, f);
            text[size] = '\0';
        }
        fclose(f);
    }
    if (text == NULL) {
        logf_("quick jumps: no spots.json found (next to the exe, or docs/testing/ in the repository)");
        return;
    }
    {
        const char* p = strstr(text, "\"spots\"");
        p = p != NULL ? strchr(p, '[') : NULL;
        while (p != NULL && s_spot_count < SPOT_MAX) {
            const char* obj = strchr(p, '{');
            const char* end;
            PcSpot* s;
            if (obj == NULL) {
                break;
            }
            end = strchr(obj, '}');
            if (end == NULL) {
                break;
            }
            s = &s_spots[s_spot_count];
            json_string(json_value(obj, end, "id"), s->id, sizeof(s->id));
            json_string(json_value(obj, end, "name"), s->name, sizeof(s->name));
            {
                const char* r = json_value(obj, end, "room_id");
                const char* e = json_value(obj, end, "entry");
                s->room = r != NULL ? (PcDebugU32)strtoul(r, NULL, 10) : 0;
                s->entry = e != NULL ? (PcDebugU32)strtoul(e, NULL, 10) : 0;
            }
            s->nset = json_numbers(json_value(obj, end, "flags_set"), end, s->set, SPOT_FLAGS);
            s->nclear = json_numbers(json_value(obj, end, "flags_clear"), end, s->clear, 16);
            if (s->id[0] != '\0' && s->room != 0) {
                s_spot_count++;
            }
            p = end + 1;
        }
    }
    free(text);
    logf_("quick jumps: %d spots from %s", s_spot_count, path);
}

int pc_debug_spot_count(void) {
    if (s_spot_count < 0) {
        load_spots();
    }
    return s_spot_count;
}
const char* pc_debug_spot_id(int i) { return i >= 0 && i < s_spot_count ? s_spots[i].id : NULL; }
const char* pc_debug_spot_name(int i) { return i >= 0 && i < s_spot_count ? s_spots[i].name : NULL; }

static void jump(const char* id) {
    if (pc_debug_spot_count() <= 0) {
        logf_("jump: no spots loaded");
        return;
    }
    for (int i = 0; i < s_spot_count; i++) {
        const PcSpot* s = &s_spots[i];
        int failed = 0;
        if (strcmp(s->id, id) != 0) {
            continue;
        }
        for (int k = 0; k < s->nset; k++) {
            failed += !s_api->set_flag(s->set[k], 1);
        }
        for (int k = 0; k < s->nclear; k++) {
            failed += !s_api->set_flag(s->clear[k], 0);
        }
        logf_("jump %s (%s): %d flags set, %d cleared%s; warp 0x%X %u: %s", id, s->name, s->nset, s->nclear,
              failed ? " (some refused)" : "", s->room, s->entry, s_api->warp(s->room, s->entry) ? "ok" : "refused");
        return;
    }
    logf_("jump: no spot '%s' (ids look like step-6)", id);
}

/* Named checkpoint slots (batch 2 item 2): a small text file next to each slot file, <exe>/checkpoints/slotN.txt
 * (the slots themselves are the game side's slotN.bin in the same folder): the label, the date and time, the room. */
static void write_slot_label(int slot, const char* label) {
    char dir[1100], path[1200];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    FILE* f;
    pc_platform_checkpoints_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/slot%d.txt", dir, slot);
    f = fopen(path, "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "%s\n%04d-%02d-%02d %02d:%02d\nroom 0x%X\n", label[0] != '\0' ? label : "(no label)",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min,
            s_api != NULL && s_api->current_room != NULL ? (unsigned)s_api->current_room() : 0u);
    fclose(f);
}

/* For the menu: "label (date time, room)" of a slot, or "" if it has none */
void pc_debug_slot_label(int slot, char* out, size_t size) {
    char dir[1100], path[1200], label[100] = "", when[32] = "", room[32] = "";
    FILE* f;
    out[0] = '\0';
    pc_platform_checkpoints_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/slot%d.txt", dir, slot);
    f = fopen(path, "r");
    if (f == NULL) {
        return;
    }
    if (fgets(label, sizeof(label), f) != NULL && fgets(when, sizeof(when), f) != NULL) {
        if (fgets(room, sizeof(room), f) == NULL) {
            room[0] = '\0';
        }
        label[strcspn(label, "\r\n")] = '\0';
        when[strcspn(when, "\r\n")] = '\0';
        room[strcspn(room, "\r\n")] = '\0';
        snprintf(out, size, "%s (%s, %s)", label, when, room);
    }
    fclose(f);
}

static const char* s_build_stamp;
void pc_debug_set_build_stamp(const char* stamp) { s_build_stamp = stamp; }
const char* pc_debug_build_stamp(void) { return s_build_stamp != NULL ? s_build_stamp : "OrrePort (build stamp unknown)"; }

void pc_debug_room_entered(unsigned room) {
    if (room == TITLE_ROOM && s_title_seen == 0) {
        s_title_seen = 1;
    }
}
