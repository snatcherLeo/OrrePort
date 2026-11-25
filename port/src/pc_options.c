/*
 * pc_options.c: the preset system (2026-10-18; snatcherLeo: "one central list where each enhancement registers
 * with its default per preset (normal and accurate), and the accurate preset forces every entry off in one place, so
 * no feature needs its own check").
 *
 * Presets: normal, or GameCube-accurate (every entry below off). *Changed 2026-11-16 (2.12):* the mode is each save's
 * own setting (below, "GameCube-accurate mode, per save"), no longer settings.ini "preset"; ORRE_PRESET still
 * overrides it for one run. The effective value of an entry is decided only in pc_option():
 *   1. accurate mode: 0, whatever the player's choice, the setting or the environment;
 *   2. the entry's environment variable, if set (tests);
 *   3. an automated run (pad script / test window): with ORRE_PLAYER_DEFAULTS=1 the normal-preset default (the test
 *      matrix: players' defaults get tested too); otherwise the entry's automated value (normally 0, vanilla for
 *      comparisons with Dolphin; fast loading 1 until the disc model is validated and the routes re-recorded);
 *   4. settings.ini <id>, else the entry's normal-preset default.
 * *Built-in entries (2026-11-21, snatcherLeo: "Get rid of those as slider options, they'll be built in"):* an entry with
 * built_in 1 has no Settings row or menu-bar item. Outside automated runs it is always its normal default (on), and
 * steps 2 and 4 are skipped: neither its environment variable nor settings.ini can turn it off (an old "0" in
 * settings.ini is logged as ignored). In automated runs steps 2 and 3 still apply, so a test can turn it off or on
 * with its ORRE_ variable (the test-only off switch; the reference playthrough's replays have it off through the automated value 0, the
 * closing sweep's tweaks pass turns it on). Step 1 still applies: accurate mode turns it off, as before.
 * Game code reaches an entry only through a named hook point (docs/port/hook-points.md), whose vanilla default is the
 * console's behaviour. A new enhancement adds one line to s_options and reads pc_option("its_id").
 */
#include "pc_options.h"

#include "pc_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pc_platform_automated_run(void); /* pc_platform.c */

static const PcOption s_options[] = {
    /* off by default (snatcherLeo, 2026-10-25: "Uncheck that, as the dolphin will have the hitch" / "Make sure it's unchecked
     * by default"); was on by default in the normal preset (snatcherLeo 2026-10-18, switched on 2026-10-19 by Main after the
     * M39 fix). Automated runs had it off all along (automated value 0). Related enhancement: a party-screen portrait
     * preload, its own switch (snatcherLeo: "2 separate switches for now") */
    {"smooth_battle_start", "Gameplay", "Smooth battle start",
     "Loads the Pokemon's portraits while the trainers walk in, so the battle doesn't pause after a send-out. Off: the "
     "original pause, which can change random-number timing (matters for RNG manipulation). Related to Smooth party "
     "screen (both preload portraits)",
     /* *Hidden 2026-11-20 while it is parked* (snatcherLeo: "we can get rid of 'Smooth battle start' since we are parking it";
      * research 6a, 1k): not in the menu bar (in_menu 0) nor in Settings (pc_settings_overlay.cpp); its code and its
      * default (off) stay, so it comes back by showing the rows again */
     0, "ORRE_SMOOTH_BATTLE_START", 0, 0},
    /* related to smooth battle start (the same portrait queue; snatcherLeo 2026-10-19: "2 separate switches for now, but we
     * can note that they are related 'enhancement' fixes in their checkbox, and we can maybe make them together later").
     * Normal default off until tested */
    /* *Renamed 2026-11-14 (snatcherLeo: "rename it to "Smooth Party UI""); was "Smooth party screen (experimental)". The id
     * and settings key stay smooth_party_screen, so saved choices carry over */
    /* *Rebuilt 2026-11-20 (step 3.7, the project's build helper; port/src/pc_party_ui.cpp; research 6b, snatcherLeo's answer 8):* the
     * party screen waits for its own portraits (with fast loading) or holds the ready ones until all are, so they appear
     * together; the earlier background preload (which could change the rolls) is retired. On by default now (snatcherLeo: "for
     * a polished look"); off in the accurate preset and automated runs (automated value 0) until the replay check allows
     * the look-only part there (research 6b, "Proof") */
    /* *Changed 2026-11-21 (snatcherLeo: "it shouldn't pause at all"; research 6d):* the list never waits now; a portrait still
     * loading is drawn from OrrePort's own copy of it (read from the disc at start-up), so all six are there on the
     * list's first frame with fast loading on or off; the hold remains the fallback */
    {"smooth_party_screen", "Gameplay", "Smooth Party UI",
     "Your party's portraits are all there on the party screen's first frame, instead of popping in one by one. "
     "Off: the original one-by-one pop-in",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1); off in automated runs unless
      * ORRE_SMOOTH_PARTY_SCREEN=1 or ORRE_PLAYER_DEFAULTS=1 (=0 then turns it off again; automated runs only) */
     1, "ORRE_SMOOTH_PARTY_SCREEN", 0, 0, 1},
    /* *Updated 2026-11-20 (step 3.7, research section 5):* it joins the Tweaks as it is (on by default, off in accurate
     * mode) and switches mid-game since fe7348486; its words now name saving too, which it always covered (the memory
     * card's console timing, pc_card_timing.c, follows this switch). The card's write time outside replays is still the
     * modelled estimate (the console's saving time is still being measured, memory-card.md) */
    {"fast_loading", "Gameplay", "Fast loading",
     "Loading and saving finish at once. Off: disc reads and memory card operations take about as long as on the "
     "GameCube (a model calibrated on Dolphin's timing), so load times and anything timed by them match the console",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1). Automated runs keep their value 1 (as before);
      * ORRE_FAST_LOADING=0 (automated runs only) gives the console's timing, the way timing tests switch it off.
      * Accurate mode keeps the console's timing (pc_dvd.c asks pc_option, which is 0 there) */
     1, "ORRE_FAST_LOADING", 0, 1, 1},
    /* *Removed 2026-11-22 (snatcherLeo: "Our goal is to have the "Mods" folder house everything, so we don't want that texture
     * pack thing in settings.ini at all"):* the "texture_pack" entry (snatcherLeo's 2026-10-17 experiment, off by default,
     * hidden for 0.1) is gone, so settings.ini never carries it; pc_settings.c drops an old line on load. Texture packs
     * return after the playtest release, through the Mods folder and Studio (Studio recognises a pack and applies it).
     * Until then the loader runs only for a developer run with ORRE_TEXTURE_PACK=1 (pc_textures.cpp) */
    /* the Continue question ("Is it okay to continue from here?") starts on YES, from the main menu's CONTINUE and from
     * a save picked in LOAD SAVE (snatcherLeo, 2026-11-14: "By default, so we're changing the game slightly, a tweak, YES
     * everywhere by default"; "in accurate-mode, let's just keep it on NO"). Off in the accurate preset (every entry
     * is) and in automated runs (automated value 0): Dolphin recordings press Up then A there, so YES pre-selected would
     * make a replay answer wrongly. Draws no random numbers (the cursor's start isn't a random caller; likely) */
    /* *Widened 2026-11-14 (snatcherLeo):* also the New Game question ("Is it okay to start a new Story?"), on an empty save
     * only: a save with a story keeps NO, "just to protect users" (pc_title_loadsave.cpp, new_game_initial) */
    {"continue_yes", "Gameplay", "Continue and New Game start on YES",
     "When you continue a save, the question \"Is it okay to continue from here?\" starts on YES, so one press plays. "
     "On an empty save, New Game's question starts on YES too; a save with a story keeps NO there. "
     "Off: both start on NO, as on the GameCube (Up, then A)",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1); automated runs keep NO (automated value 0) unless
      * ORRE_CONTINUE_YES=1 or ORRE_PLAYER_DEFAULTS=1 */
     1, "ORRE_CONTINUE_YES", 0, 0, 1},
    /* Step 3.7's item tweaks (2026-11-20, the project's build helper; port/src/pc_bag_tweaks.cpp; docs/port/tweaks-37-research.md
     * sections 1-1i). One entry each, so the switches can be grouped later without touching the game hooks (snatcherLeo's Q9,
     * research 7a, still open). All on in the normal mode (snatcherLeo decided each), off in the accurate preset (every entry
     * is) and in automated runs (automated value 0): they change which presses lead where in the bag, so a recorded
     * route's presses would mean something else. None draws random numbers (the bag, the party list and the item use
     * functions draw none; verified by search, research section 1) */
    /* *Widened 2026-11-20 (snatcherLeo's hand check, research 1k):* the list stays open without closing between uses, the party
     * list starts on the last Pokemon chosen for a bag item, and TMs follow the game's flow (taught: the bag; not
     * learned: the list stays) */
    {"keep_using_item", "Gameplay", "Keep using an item",
     "After you use or give an item from the bag on a Pokemon, the party list stays open while you have more of it, "
     "and it starts on the last Pokemon you chose. A TM that isn't learned stays on the list; a taught TM goes back to "
     "the bag. B goes back to the bag. Off: back to the bag after every use, as on the GameCube",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1); off in automated runs unless ORRE_KEEP_USING_ITEM=1
      * or ORRE_PLAYER_DEFAULTS=1 */
     1, "ORRE_KEEP_USING_ITEM", 0, 0, 1},
    /* LEARNED for a Pokemon that already knows the move (research 1d). *Renamed 2026-11-20* (snatcherLeo's hand check, research
     * 1k; was "TMs: stay on the party list"): the TM loop moved into the game's own flow under "Keep using an item", so
     * only the label is left here. The id and settings key stay tm_party_list, so saved choices carry over */
    {"tm_party_list", "Gameplay", "TMs: the LEARNED label",
     "On the party list for a TM, a Pokemon that already knows the move reads LEARNED instead of ABLE. Off: ABLE, as "
     "on the GameCube",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1); off in automated runs unless ORRE_TM_PARTY_LIST=1
      * or ORRE_PLAYER_DEFAULTS=1 */
     1, "ORRE_TM_PARTY_LIST", 0, 0, 1},
    /* infinite TMs (snatcherLeo, 2026-11-20: "a built-in tweak", then "its own switch"): turning it off only makes TMs used up
     * again; the TM menu behaviour above stays */
    {"infinite_tms", "Gameplay", "Infinite TMs",
     "Teaching a TM keeps it, as HMs already do. Off: a taught TM is used up, as on the GameCube",
     1, "ORRE_INFINITE_TMS", 1, 0},
    /* the scents (research 1f, snatcherLeo's remembered-cursor design; scents strongest first) */
    {"scent_memory", "Gameplay", "Scents: remember the last one",
     "The Cologne Case lists Vivid first and starts on the scent you used last, and the party list on the Pokemon it "
     "went to. Off: Joy first and the first rows, as on the GameCube",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1); off in automated runs unless ORRE_SCENT_MEMORY=1
      * or ORRE_PLAYER_DEFAULTS=1 */
     1, "ORRE_SCENT_MEMORY", 0, 0, 1},
    /* Step 3.7's Relic Forest shortcuts (2026-11-20, the project's build helper; port/src/pc_relic_forest.cpp; research sections
     * 2-3): one switch for PURIFY in the party menu and the Time Flute anywhere in the forest (snatcherLeo's answer 6). On in the
     * normal mode, off in the accurate preset and in automated runs (automated value 0): the extra row moves CANCEL and
     * the flute no longer needs Wes facing the stone, so a console route's presses would mean something else. Draws no
     * random numbers (verified by search, research sections 2-3) */
    {"relic_forest", "Gameplay", "Relic Forest shortcuts",
     "In the Relic Forest, PURIFY appears in the menu of a Shadow Pokemon whose heart is open, and the Time Flute works "
     "anywhere in the forest. The Relic Stone works as always. Off: only at the stone, as on the GameCube",
     /* *Built in 2026-11-21 (snatcherLeo):* no switch (in_menu 0, built_in 1); off in automated runs unless ORRE_RELIC_FOREST=1
      * or ORRE_PLAYER_DEFAULTS=1 */
     1, "ORRE_RELIC_FOREST", 0, 0, 1},
    /* Step 3.7's right-stick walking (2026-11-20, the project's build helper; port/src/pc_cstick_walk.cpp; research section 4,
     * snatcherLeo's answer 7: a full push runs). On in the normal mode, off in the accurate preset and in automated runs
     * (automated value 0): it changes which inputs move Wes. Draws no random numbers (verified by search) */
    /* *Changed 2026-11-21 (snatcherLeo: the right stick copies the left one in story mode, as in XD):* the same switch and key;
     * the right stick now also moves every menu's cursor; off in battles, Battle Mode and the P★DA's model page.
     * *Changed again the same evening (snatcherLeo: "in a battle, (in story mode) right stick should work just like left
     * stick"):* story battles too; off in Battle Mode and on the P★DA's model page */
    {"cstick_walk", "Gameplay", "The right stick works like the left one",
     "In story mode the right stick (C-Stick) does everything the left stick does: walking, running, battles and every "
     "menu. Battle Mode and the P*DA's Pokemon pictures keep its own use. Off: as on the GameCube",
     1, "ORRE_CSTICK_WALK", 1, 0},
    /* *Changed 2026-11-14:* the New Game question's YES-first (on an empty save only) is part of this same switch, and
     * the naming keyboard is no switch at all (snatcherLeo: "that has to be apart of the same tweak option"; "OrrePort
     * keyboard ... has to not be an enhancement that is toggle-able, it's a built-in default for OrrePort, and the
     * only way to toggle it off is ... Gamecube Accurate Mode"). The earlier entries new_game_yes and name_keyboard
     * are gone; pc_name_keyboard.cpp asks the accurate preset itself. */
};
enum { OPTION_COUNT = (int)(sizeof(s_options) / sizeof(s_options[0])) };

/* The player's choices (s_value, read once per entry) are kept apart from the mode: pc_option applies the mode on every
 * call, so an entry first read in the hub (the texture pack, at start-up) can't keep an answer from before the mode was
 * fixed. */
static int s_value[OPTION_COUNT];
static int s_loaded[OPTION_COUNT];

/* ---- GameCube-accurate mode, per save (2.12, 2026-11-16) ----
 * Why per save (snatcherLeo, 2026-11-08 / 11-10, the project plan's section 5): accurate mode promises that the save stays a
 * pure .gci and that the same button presses give the console's results, so it belongs to the save it protects: an
 * accurate save always opens accurate, a normal save always normal. Each save's setup file (Saves\Slot N\slot.ini,
 * "gamecube_accurate=0/1") keeps it; pc_saves.c reads and writes it; the hub's Saves page changes it while the game is
 * closed (the hub only runs before the game; Quit to hub starts OrrePort again).
 * How the mode of a run is decided, first match wins:
 *   1. ORRE_PRESET=accurate / normal: a test run's override (the replay scripts and test matrices use it);
 *   2. fixed: from pc_card_open on (pc_option_accurate_lock), the value the game started with, for the rest of the run;
 *   3. the portable layout's save slots: the current save's setting (the hub's pick);
 *   4. no save slots (a development build without the portable folder): settings.ini "preset" as before 2.12.
 * The old program-wide "preset" line is no longer written; it seeds the new-saves default once (below), so a folder
 * that played in accurate mode before keeps doing so (the migration: pc_saves.c gives each existing save that value).
 * Platforms: plain C; the same on Windows, Linux and macOS. */
/* pc_saves.c is built only with Aurora's card library (port/CMakeLists.txt): weak, so a build without it has no save
 * slots and takes step 4 */
int pc_saves_ready(void) __attribute__((weak));
int pc_saves_current(void) __attribute__((weak));
int pc_saves_accurate_get(int slot) __attribute__((weak));

static int s_locked = -1; /* -1: not fixed yet; 0 / 1 once the game has started */

static int env_mode(void) {
    const char* env = getenv("ORRE_PRESET");
    if (env == NULL) return -1;
    return strcmp(env, "accurate") == 0;
}

static int legacy_preset(void) { return strcmp(pc_settings_string("preset", "normal"), "accurate") == 0; }

static int current_mode(const char** why) {
    const int env = env_mode();
    if (env >= 0) {
        *why = "ORRE_PRESET (a test run's override)";
        return env;
    }
    if (pc_saves_ready && pc_saves_ready() && pc_saves_current && pc_saves_accurate_get) {
        *why = "the save's own setting (slot.ini gamecube_accurate)";
        return pc_saves_accurate_get(pc_saves_current()) != 0;
    }
    *why = "settings.ini preset (no save slots: not the portable layout)";
    return legacy_preset();
}

int pc_option_gamecube_accurate(void) {
    if (s_locked >= 0) return s_locked;
    const char* why;
    return current_mode(&why);
}

void pc_option_accurate_lock(void) {
    const char* why;
    if (s_locked >= 0) return;
    s_locked = current_mode(&why);
    /* the line test tools look for ("[options] preset ..."), kept in the old form so older log readers still work */
    printf("[options] preset %s (%s): %s; fixed for this run\n", s_locked ? "GameCube-accurate" : "normal", why,
           s_locked ? "every enhancement off" : "enhancements at their settings");
    {
        /* *Added 2026-11-20 (step 3.7, the logs carry every setting; pc_settings_log.cpp):* every tweak as it applies in
         * this run, and the Settings rows again (locked ones marked), now that the mode is fixed. Weak: the rows' log
         * is in the RmlUi Settings overlay, absent from builds without it. All platforms */
        extern void pc_settings_log_options(const char* when);
        extern void pc_settings_overlay_log_rows(const char* when) __attribute__((weak));
        pc_settings_log_options("in this run");
        if (pc_settings_overlay_log_rows) pc_settings_overlay_log_rows("in this run");
    }
}

int pc_option_accurate_locked(void) { return s_locked >= 0; }

/* The default for new saves (Settings › General's first row; snatcherLeo, 2026-11-16, wording approved): settings.ini
 * "accurate_new_saves". Never set yet: the old "preset" line's value, so a folder that played in accurate mode before
 * this build starts new saves the same way. Per game later (XD gets its own key when XD support starts; the row then
 * names the game). */
int pc_option_accurate_new_saves(void) { return pc_settings_int("accurate_new_saves", legacy_preset()) != 0; }

void pc_option_set_accurate_new_saves(int on) {
    pc_settings_set_int("accurate_new_saves", on != 0);
    if (!pc_platform_automated_run()) pc_settings_save();
    printf("[options] GameCube-accurate mode for new saves: %s (Settings)\n", on ? "on" : "off");
}

/* What accurate mode changes, as built (2026-11-16, checked against the code: each row names where it is decided).
 * The "What it turns off" list shows these in order, by kind. */
static const PcAccurateRow s_accurate_rows[] = {
    /* turned off (each one's code asks pc_option_gamecube_accurate or pc_option) */
    {PC_ACCURATE_OFF, "accurate.what_off.pc_menu"},      /* pc_field_menu.cpp: PC MENU, Start closing the pause menu */
    {PC_ACCURATE_OFF, "accurate.what_off.main_menu"},    /* pc_pc_screen.cpp: Main Menu on the PC screen */
    {PC_ACCURATE_OFF, "accurate.what_off.wrap"},         /* pc_field_menu.cpp / pc_pc_screen.cpp: cursors wrapping */
    {PC_ACCURATE_OFF, "accurate.what_off.pc_exit"},      /* pc_field_menu.cpp: the shorter PC-screen exit */
    {PC_ACCURATE_OFF, "accurate.what_off.continue_yes"}, /* pc_options.c: Continue and New Game start on YES */
    {PC_ACCURATE_OFF, "accurate.what_off.load_question"},/* pc_title_loadsave.cpp: LOAD's question, CONTINUE's line */
    {PC_ACCURATE_OFF, "accurate.what_off.keyboard"},     /* pc_name_keyboard.cpp, pc_names.cpp: the keyboard, long names */
    {PC_ACCURATE_OFF, "accurate.what_off.name_back"},    /* pc_name_keyboard.cpp: B on Wes's name menu */
    {PC_ACCURATE_OFF, "accurate.what_off.rewrap"},       /* pc_msg_rewrap.cpp: dialogue re-wrapping */
    {PC_ACCURATE_OFF, "accurate.what_off.options_b"},    /* pc_options_lang.cpp: B keeps the Options choices */
    {PC_ACCURATE_OFF, "accurate.what_off.smooth"},       /* pc_options.c: Smooth battle start, Smooth Party UI */
    /* step 3.7's item tweaks (2026-11-20; pc_bag_tweaks.cpp) */
    {PC_ACCURATE_OFF, "accurate.what_off.keep_using_item"}, /* pc_options.c: the party list after a use or a give */
    {PC_ACCURATE_OFF, "accurate.what_off.tm_party_list"},   /* pc_options.c: LEARNED (the TM loop: keep_using_item) */
    {PC_ACCURATE_OFF, "accurate.what_off.stone_label"},     /* pc_bag_tweaks.cpp piece 6: stones' ABLE / NOT ABLE (0.1.2) */
    {PC_ACCURATE_OFF, "accurate.what_off.infinite_tms"},    /* pc_options.c: infinite TMs */
    {PC_ACCURATE_OFF, "accurate.what_off.scent_memory"},    /* pc_options.c: the scents' order and remembered cursors */
    {PC_ACCURATE_OFF, "accurate.what_off.bag_options"},     /* pc_bag_tweaks.cpp: READ, CHECK, TOSS on TMs, no useless USE */
    /* step 3.7's Relic Forest shortcuts (2026-11-20; pc_relic_forest.cpp) */
    {PC_ACCURATE_OFF, "accurate.what_off.relic_forest"},    /* pc_options.c: PURIFY in the party menu, the flute anywhere */
    {PC_ACCURATE_OFF, "accurate.what_off.cstick_walk"},     /* pc_cstick_walk.cpp: the right stick walks Wes */
    /* frame smoothing (Settings › Graphics › Frame rate): was "off" here until the the reference playthrough gate passed for the rates.
     * *Moved 2026-11-20* to the "still works" display row below: every the reference playthrough sitting at 0, 1, 3 and 7 in-betweens gave
     * identical rolls and rooms (frame-smoothing-plan.md section 34) */
    /* the Cheats menu: *moved 2026-11-20* to the "still works" rows below (snatcherLeo: "we don't need to have them off") */
    {PC_ACCURATE_OFF, "accurate.what_off.extras"},       /* everything an .orresave holds (accurate plays .gci only) */
    /* kept at the GameCube's own way */
    {PC_ACCURATE_GC, "accurate.what_off.fast_loading"},  /* pc_options.c / pc_dvd.c: disc and card reads take console time */
    {PC_ACCURATE_GC, "accurate.what_off.stick"},         /* pc_pad_shape.cpp: the stick's range, dead zone at most 10 */
    /* still works (look and sound that keep the random numbers; OrrePort's own menus outside the game) */
    {PC_ACCURATE_SAME, "accurate.what_off.same_display"},  /* Graphics: resolution, frame rate, filtering, AA, window */
    /* The speed-up (*changed 2026-11-16*, the Speed settings' build, on snatcherLeo's condition "accurate mode only if
     * the 3x pass matches"): pass x3ring matched base1 in all 23 the reference playthrough sittings, so Game speed, Battles, the
     * fast-forward key and its badge work in accurate mode (pc_speed.c). Before, the GC row "music_speed" (music and
     * sound always follow Game speed) stood here: the sound split made it unneeded. "Music when faster: Faster too" is
     * likely safe as well (the game's sound engine keeps the 1x schedule either way) but not yet proven by its own pass;
     * it is the player's choice in both modes (same_music). */
    {PC_ACCURATE_SAME, "accurate.what_off.same_speed"},    /* pc_speed.c: Game speed */
    {PC_ACCURATE_SAME, "accurate.what_off.same_battles"},  /* pc_speed.c: Battles' own speed */
    {PC_ACCURATE_SAME, "accurate.what_off.same_ff"},       /* pc_speed.c, pc_pad_shape.cpp: the fast-forward key */
    {PC_ACCURATE_SAME, "accurate.what_off.same_badge"},    /* pc_settings_overlay.cpp: the speed badge (look only) */
    {PC_ACCURATE_SAME, "accurate.what_off.same_music"},    /* pc_sound_split.c: Music when faster */
    {PC_ACCURATE_SAME, "accurate.what_off.same_text"},     /* languages, the sharp letters, the fallback fonts */
    {PC_ACCURATE_SAME, "accurate.what_off.same_sound"},    /* volumes */
    {PC_ACCURATE_SAME, "accurate.what_off.same_buttons"},  /* button mapping */
    {PC_ACCURATE_SAME, "accurate.what_off.same_load_save"},/* pc_title_loadsave.cpp: LOAD SAVE, accurate saves only */
    {PC_ACCURATE_SAME, "accurate.what_off.same_menu"},     /* the OrrePort menu, Report a problem, Quit to hub */
    /* pc_cheats.c: the Cheats menu works in accurate mode too (2026-11-20, snatcherLeo); all start off, and while one is on the
     * game plays differently from a console, which the row says */
    {PC_ACCURATE_SAME, "accurate.what_off.same_cheats"},
};

int pc_accurate_list_count(void) { return (int)(sizeof(s_accurate_rows) / sizeof(s_accurate_rows[0])); }
const PcAccurateRow* pc_accurate_list_at(int index) {
    return index >= 0 && index < pc_accurate_list_count() ? &s_accurate_rows[index] : NULL;
}

static int find(const char* id) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        if (strcmp(s_options[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

/* the player's choice for an entry (environment, automated value, settings.ini, default), read once */
int pc_option_choice(const char* id) {
    const int i = find(id);
    if (i < 0) {
        fprintf(stderr, "[options] unknown option '%s' (not in pc_options.c's list): treated as off\n", id);
        return 0;
    }
    if (!s_loaded[i]) {
        const PcOption* o = &s_options[i];
        const int automated = pc_platform_automated_run();
        /* a built-in entry reads its environment variable in automated runs only (the test-only switch): a player's
         * shell can't turn it off */
        const char* env = o->env != NULL && (!o->built_in || automated) ? getenv(o->env) : NULL;
        const char* why;
        s_loaded[i] = 1;
        if (o->built_in && !automated) {
            /* built in (2026-11-21, snatcherLeo): always the normal default; an old settings.ini value of 0 (from when it had
             * a switch) is ignored, and said so once. The key is left in the file untouched (harmless; it is never
             * written again) */
            s_value[i] = o->normal_default;
            why = "built in: no switch; on in normal play, off in GameCube-accurate mode";
            if (pc_settings_int(o->id, -1) == 0) {
                printf("[options] %s: settings.ini's %s=0 is ignored: this tweak is built in now (2026-11-21)\n", o->id,
                       o->id);
            }
        } else if (env != NULL) {
            s_value[i] = atoi(env) != 0;
            why = o->env;
        } else if (automated && getenv("ORRE_PLAYER_DEFAULTS") != NULL) {
            /* the test matrix: an automated run with every entry at its normal-preset default, so whatever players
             * get by default is exercised too (2026-10-18, after M39; settings.ini is not read, for repeatability) */
            s_value[i] = o->normal_default;
            why = "automated run with ORRE_PLAYER_DEFAULTS: the normal preset's default";
        } else if (automated) {
            s_value[i] = o->automated_value;
            why = "automated run: the entry's automated value, unless its environment variable says otherwise";
        } else {
            s_value[i] = pc_settings_int(o->id, o->normal_default) != 0;
            why = "settings.ini, or the normal preset's default";
        }
        printf("[options] %s %s (%s)\n", o->id, s_value[i] ? "on" : "off", why);
    }
    return s_value[i];
}

/* the effective value: 0 in GameCube-accurate mode (asked on every call: see s_value above), else the choice */
int pc_option(const char* id) {
    static int s_told[OPTION_COUNT]; /* the log says once per entry that accurate mode overrides the choice */
    const int choice = pc_option_choice(id);
    if (!pc_option_gamecube_accurate()) return choice;
    const int i = find(id);
    if (choice && i >= 0 && !s_told[i] && pc_option_accurate_locked()) {
        s_told[i] = 1;
        printf("[options] %s off (GameCube-accurate mode forces it off; the choice stays on for normal saves)\n", id);
    }
    return 0;
}

int pc_option_built_in(const char* id) {
    const int i = find(id);
    return i >= 0 && s_options[i].built_in;
}

void pc_option_set(const char* id, int value) {
    const int i = find(id);
    /* a game running in accurate mode keeps everything off and Settings shows the rows locked; in the hub (before the
     * mode is fixed) the choices stay changeable, since they are the normal mode's and a save of either kind may be
     * picked next */
    if (i < 0 || (pc_option_accurate_locked() && pc_option_gamecube_accurate())) {
        return;
    }
    /* a built-in entry has no switch (2026-11-21): nothing in the UI calls this for one; refused so a stray caller
     * can't write a key that is ignored anyway */
    if (s_options[i].built_in) {
        printf("[options] %s is built in: no switch, the change is ignored\n", id);
        return;
    }
    s_loaded[i] = 1;
    s_value[i] = value != 0;
    if (!pc_platform_automated_run()) {
        pc_settings_set_int(id, s_value[i]);
        pc_settings_save();
    }
    /* from the menu bar's Gameplay menu or Settings › Enhancements › Tweaks (2026-11-14); takes effect at once for every
     * feature that reads pc_option when it acts (snatcherLeo, through C: tweaks need no restart). *2026-11-20 (step 3.7, the
     * logs carry every setting):* with when it happened, since in automated runs the settings file isn't written and its
     * own change line doesn't come */
    {
        char when[48];
        pc_settings_when(when, sizeof(when));
        printf("[options] %s %s (menu bar or Settings; %s)\n", id, s_value[i] ? "on" : "off", when);
    }
}

int pc_option_count(void) { return OPTION_COUNT; }
const PcOption* pc_option_at(int index) { return index >= 0 && index < OPTION_COUNT ? &s_options[index] : NULL; }

/* asked by the game side's OrreHook_BattlePreloadPortraits (weak there: a build without this file stays vanilla) */
int pc_option_smooth_battle_start(void) { return pc_option("smooth_battle_start"); }
/* asked by the game side's pc_party_portraits_frame (weak there too) */
int pc_option_smooth_party_screen(void) { return pc_option("smooth_party_screen"); }
