/*
 * pc_speed.c: Settings > Enhancements > Speed and the fast-forward key (snatcherLeo, 2026-11-13, mockups 64-66; rebuilt
 * 2026-11-16 after the mockups of "A rearranged": "SOunds good"; the decision log "The Speed settings and the fast-forward
 * key", "The fast-forward button's default on a controller: LB", "The speed-up is allowed in GameCube-accurate mode").
 *
 * What it does, once per game frame (pc_vi.c, before the frame's pacing wait):
 *   - picks the speed: the fast-forward speed while the key is held (or switched on, in "Press to switch"), which
 *     overrides the rest; else Battles' own speed while a battle runs (unless it is "Same as Game speed"); else Game
 *     speed. snatcherLeo: the key "speeds up everything while held (the player's hand picks what)";
 *   - sets the game's pacing (pc_vi_set_enhancement_speed): a faster game runs more of its own 1/60-second steps per
 *     second, the same mechanism as the debug menu's speed, so every step is the one the console would run;
 *   - tells the sound (pc_audio_set_speed) the game's speed and "Music when faster": "Normal tempo" (the default: the
 *     sound split, the music at its usual tempo while the game's own sound engine stays on the console's schedule) or
 *     "Faster too" (music and sound sped up with the game, higher in pitch);
 *   - logs a "[speed]" line when fast-forward starts or stops and when any speed setting changes (testers' logs);
 *   - raises the badge's number (pc_speed_badge_serial) when fast-forward starts or the speed changes to a faster one.
 *
 * Why the sound matters for the random rolls (verified: sound-split.md, music.md "When a move waits for its sound"):
 * the game waits for some sounds to end (a move's sound, the evolution cry, the level-up jingle, a room's load step)
 * by asking every frame, and draws random numbers every frame (blinks, the idle battle camera). Before the split, with
 * the music at normal tempo and the game faster, those waits lasted more game frames: moves lingered and later rolls
 * came out different (verified: P01 desynced at frame 356 at 3x). The split keeps the game's own sound engine on the
 * console's blocks per frame whatever the speed, so the waits keep their 1x frame counts (verified: the x3ring pass,
 * all 23 the reference playthrough sittings byte-identical to 1x).
 *
 * GameCube-accurate mode: the speed-up is allowed there (2026-11-16, on snatcherLeo's condition "accurate mode only if
 * the 3x pass matches"; it matched). *Changed 2026-11-16:* before, accurate mode forced the sound to follow the game's
 * speed here (the only way the waits kept their frame counts then); nothing else relied on that (checked: Settings
 * locked the Music row for it, the accurate-mode list said so; both updated). "Faster too" in accurate mode is likely
 * safe too (the game's sound engine is on the 1x schedule either way, by construction), but not yet proven by its own
 * the reference playthrough pass. Without the replay clock (a development build outside the portable layout) the split can't run and
 * "Normal tempo" lets the waits linger, as before; accurate play always runs on the replay clock in the portable layout.
 *
 * Automated runs (recorded checks against Dolphin) always play at 1x with the game's sound, whatever is saved, and the
 * fast-forward key is never read; the test switches below are the only way to speed one up.
 *
 * Platforms: plain C; the same on Windows, Linux and macOS (SDL reads the button, SDL plays the sound).
 */
#include "pc_speed.h"
#include "pc_debug.h"
#include "pc_settings.h"

#include <stdio.h>
#include <stdlib.h> /* getenv, atoi: the test-only ORRE_TEST_SPEED / ORRE_TEST_FF */
#include <string.h>

void pc_vi_set_enhancement_speed(unsigned percent); /* pc_vi.c */
int pc_platform_automated_run(void);                /* pc_platform.c */
extern volatile unsigned int pc_vi_heartbeat;       /* pc_vi.c: the game frame count */
/* pc_audio.c (the orre_audio library, which has MusyX): weak, so a build without the game's sound still links, as
 * the overlay does with pc_audio_set_volumes */
void pc_audio_set_speed(unsigned game_percent, int music_follows) __attribute__((weak));
/* the fast-forward button (pc_pad_shape.cpp) and the menus that take the controller (pc_settings_overlay.cpp,
 * pc_controls_ui.cpp): built only with RmlUi, so weak; without them there is no key */
int pc_ff_held(void) __attribute__((weak));
void pc_ff_first_start_check(void) __attribute__((weak));
int pc_settings_overlay_is_open(void) __attribute__((weak));
int pc_controls_ui_is_open(void) __attribute__((weak));

/* the choices' percentages, by saved code (settings.ini; pc_speed.h); 5x added 2026-11-16 (snatcherLeo: "maybe up to 5x";
 * pc_vi.c and MusyX's playback clamp raised to 500 % with it); 4x added 2026-11-17 at the end of each list (snatcherLeo,
 * 2026-11-16: "yeah add 4x next time"), so every older saved code keeps its speed */
static const unsigned kFF[] = {200, 300, 500, 400};
static const unsigned kGame[] = {100, 150, 200, 300, 500, 400};
static const unsigned kBattle[] = {0 /* same as Game speed */, 100, 150, 200, 300, 500, 400};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* the rows' order (port/src/pc_settings_overlay.cpp): each row's saved code, so the rows read in order (3x, 4x, 5x)
 * while the codes stay what older settings.ini files saved. Must list every code once, as many as the tables above */
static const int kFFRows[] = {0, 1, 3, 2};
static const int kGameRows[] = {0, 1, 2, 3, 5, 4};
static const int kBattleRows[] = {0, 1, 2, 3, 4, 6, 5};

static const int* rows_of(int list, int* count) {
    switch (list) {
    case PC_SPEED_LIST_FF: *count = COUNT(kFFRows); return kFFRows;
    case PC_SPEED_LIST_GAME: *count = COUNT(kGameRows); return kGameRows;
    case PC_SPEED_LIST_BATTLE: *count = COUNT(kBattleRows); return kBattleRows;
    default: *count = 0; return NULL;
    }
}

int pc_speed_code_for_row(int list, int row) {
    int count;
    const int* rows = rows_of(list, &count);
    return rows != NULL && row >= 0 && row < count ? rows[row] : -1;
}

int pc_speed_row_for_code(int list, int code) {
    int count, i;
    const int* rows = rows_of(list, &count);
    for (i = 0; rows != NULL && i < count; ++i) {
        if (rows[i] == code) return i;
    }
    return -1;
}

static unsigned s_applied_game = 0; /* the pacing last set (0: never) */
static int s_applied_sound = -1;    /* the sound speed last set (-1: never) */
static int s_ff_on;                 /* fast-forward is on now */
static int s_ff_toggled;            /* "Press to switch": switched on */
static int s_ff_was;                /* the button was down at the last frame (presses count once) */
static unsigned s_badge_serial, s_badge_percent;

static unsigned pick(int index, const unsigned* table, int count, unsigned fallback) {
    return index >= 0 && index < count ? table[index] : fallback;
}

unsigned pc_speed_badge_serial(unsigned* percent) {
    if (percent != NULL) *percent = s_badge_percent;
    return s_badge_serial;
}

/* the six settings, read each frame (the menu changes them live); a change is logged once, by name */
typedef struct {
    int ff, ff_mode, badge, game, battle, sound;
} Choices;

static Choices read_choices(void) {
    Choices c;
    c.ff = pc_settings_int("speed_ff", PC_SPEED_FF_DEFAULT);
    c.ff_mode = pc_settings_int("speed_ff_mode", 0);
    c.badge = pc_settings_int("speed_badge", 1);
    c.game = pc_settings_int("speed_game", 0);
    c.battle = pc_settings_int("speed_battle", 0);
    c.sound = pc_settings_int("speed_sound", 0);
    return c;
}

static void log_changes(const Choices* now) {
    static Choices last;
    static int have;
    if (have) {
        if (now->ff != last.ff) printf("[speed] setting: Fast-forward %u%%\n", pick(now->ff, kFF, COUNT(kFF), 300));
        if (now->ff_mode != last.ff_mode) {
            printf("[speed] setting: Fast-forward works: %s\n", now->ff_mode ? "Press to switch" : "Hold");
        }
        if (now->badge != last.badge) printf("[speed] setting: Speed badge %s\n", now->badge ? "shown" : "hidden");
        if (now->game != last.game) printf("[speed] setting: Game speed %u%%\n", pick(now->game, kGame, COUNT(kGame), 100));
        if (now->battle != last.battle) {
            const unsigned b = pick(now->battle, kBattle, COUNT(kBattle), 0);
            if (b == 0) printf("[speed] setting: Battles same as Game speed\n");
            else printf("[speed] setting: Battles %u%%\n", b);
        }
        if (now->sound != last.sound) {
            printf("[speed] setting: Music when faster: %s\n", now->sound ? "Faster too" : "Normal tempo");
        }
    } else {
        printf("[speed] settings: Fast-forward %u%% (%s, badge %s), Game speed %u%%, Battles %s, Music when faster: %s\n",
               pick(now->ff, kFF, COUNT(kFF), 300), now->ff_mode ? "Press to switch" : "Hold",
               now->badge ? "shown" : "hidden", pick(now->game, kGame, COUNT(kGame), 100),
               now->battle == 0 ? "same as Game speed" : "own speed", now->sound ? "Faster too" : "Normal tempo");
    }
    last = *now;
    have = 1;
}

/* ORRE_TEST_FF=<from>-<to> (test only; game frames): fast-forward on between those frames, as if the key were held,
 * in live and automated runs alike, so a replay can switch speed mid-sitting through the same path as the key (and a
 * picture run can show the badge). 0 when unset or outside the range. */
static int test_ff(void) {
    static long from = -2, to = -1;
    if (from == -2) {
        const char* v = getenv("ORRE_TEST_FF");
        from = -1;
        if (v != NULL && sscanf(v, "%ld-%ld", &from, &to) == 2 && from >= 0 && to >= from) {
            printf("[speed] test: fast-forward on from game frame %ld to %ld (ORRE_TEST_FF)\n", from, to);
        } else {
            from = -1;
        }
    }
    return from >= 0 && (long)pc_vi_heartbeat >= from && (long)pc_vi_heartbeat <= to;
}

/* the fast-forward key's state this frame (live play): Hold, or Press to switch; never while one of OrrePort's menus
 * has the controller (LB is "the previous category" there, Tab a key a text box may want) */
static int ff_from_key(const Choices* c) {
    const int raw = pc_ff_held != NULL && pc_ff_held();
    const int menus = (pc_settings_overlay_is_open != NULL && pc_settings_overlay_is_open()) ||
                      (pc_controls_ui_is_open != NULL && pc_controls_ui_is_open());
    const int pressed = !menus && raw && !s_ff_was;
    s_ff_was = raw; /* a press that began in a menu doesn't count once the menu closes */
    if (c->ff_mode == 0) {
        s_ff_toggled = 0;
        return !menus && raw;
    }
    if (pressed) s_ff_toggled = !s_ff_toggled;
    return s_ff_toggled;
}

void pc_speed_frame(void) {
    unsigned percent = 100;
    int sound_faster = 0;
    int ff = 0;
    int badge = 0;

    if (!pc_platform_automated_run()) {
        const Choices c = read_choices();
        const unsigned game = pick(c.game, kGame, COUNT(kGame), 100);
        const unsigned battle = pick(c.battle, kBattle, COUNT(kBattle), 0);
        const PcDebugApi* api = pc_debug_api();
        const int in_battle = api != NULL && api->battle_number != NULL && api->battle_number() != 0;
        if (pc_ff_first_start_check != NULL) pc_ff_first_start_check();
        log_changes(&c);
        ff = ff_from_key(&c) || test_ff();
        percent = ff ? pick(c.ff, kFF, COUNT(kFF), 300) : in_battle && battle != 0 ? battle : game;
        sound_faster = c.sound != 0;
        badge = c.badge != 0;
    } else {
        /* Test only (automated runs): an automated run normally plays at 1x with the game's sound whatever is saved;
         * ORRE_TEST_SPEED=<percent> (100-500) runs it at that game speed, with ORRE_TEST_SPEED_SOUND=follow for "Faster
         * too" (default: "Normal tempo", the sound split, which pc_sound_split.c turns on for such a run), so a recorded
         * sitting can be replayed sped up and compared with its 1x replay. ORRE_TEST_FF (above) at 3x. Neither is read
         * outside automated runs (ORRE_TEST_FF is: a picture run is live). */
        static int test_percent = -1, test_follow;
        if (test_percent < 0) {
            const char* p = getenv("ORRE_TEST_SPEED");
            const char* s = getenv("ORRE_TEST_SPEED_SOUND");
            test_percent = p != NULL ? atoi(p) : 0;
            test_follow = s != NULL && strcmp(s, "follow") == 0;
            if (test_percent >= 100) {
                printf("[speed] test speed %d%%, music %s (ORRE_TEST_SPEED)\n", test_percent,
                       test_follow ? "faster too" : "at its normal tempo");
            }
        }
        if (test_percent >= 100) {
            percent = (unsigned)test_percent;
            sound_faster = test_follow;
        }
        if (test_ff()) {
            ff = 1;
            percent = 300;
        }
    }
    if (ff != s_ff_on) {
        printf("[speed] fast-forward %s at game frame %u (%u%%)\n", ff ? "on" : "off", (unsigned)pc_vi_heartbeat, percent);
        if (ff && badge && !pc_platform_automated_run()) { /* the badge: fast-forward starts (both modes) */
            s_badge_percent = percent;
            ++s_badge_serial;
        }
        s_ff_on = ff;
    }
    if (percent != s_applied_game) {
        pc_vi_set_enhancement_speed(percent);
        if (s_applied_game != 0) {
            printf("[speed] game speed %u%%\n", percent); /* each change in the run log */
        }
        /* the badge: a change to a faster speed (fast-forward's start was counted above) */
        if (percent > 100 && !ff && badge && !pc_platform_automated_run()) {
            s_badge_percent = percent;
            ++s_badge_serial;
        }
        s_applied_game = percent;
    }
    {
        /* the sound's state: the game's speed, and whether the music follows it (changes either way are applied) */
        const int sound = (int)percent * 2 + (sound_faster ? 1 : 0);
        if (sound != s_applied_sound) {
            if (pc_audio_set_speed) pc_audio_set_speed(percent, sound_faster);
            if (s_applied_sound >= 0 && (s_applied_sound & 1) != (sound & 1)) {
                printf("[speed] music %s\n", sound_faster ? "faster too" : "at its normal tempo");
            }
            s_applied_sound = sound;
        }
    }
}
