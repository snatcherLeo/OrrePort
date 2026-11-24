/* pc_options.h: the preset system (pc_options.c). Every enhancement, i.e. anything that changes the game's behaviour,
 * timing or look relative to the GameCube, is one entry in pc_options.c's list, with its default in the normal preset.
 * GameCube-accurate mode forces every entry off in one place (pc_option), so no feature checks the mode itself.
 * Features read their entry with pc_option(id) and never read settings.ini or the environment directly.
 * *Changed 2026-11-16 (2.12, accurate mode finalized):* accurate mode is a setting of each save, not of the program
 * (below); the old settings.ini "preset" only seeds the default for new saves once. */
#ifndef PC_OPTIONS_H
#define PC_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcOption {
    const char* id;          /* settings.ini key and pc_option() id */
    const char* category;    /* the menu bar menu it appears in */
    const char* name;        /* menu label */
    const char* description; /* tooltip: what it changes, and what "off" (the console's behaviour) means */
    int normal_default;      /* value in the normal preset when settings.ini doesn't say */
    const char* env;         /* per-run override (tests), e.g. ORRE_SMOOTH_BATTLE_START=0 / 1 */
    int in_menu;             /* 1: the menu bar lists it under its category; 0: shown by its own menu code */
    int automated_value;     /* value in automated runs (comparisons with Dolphin): normally 0 (vanilla); fast
                              * loading stays 1 until the disc model is validated and the routes re-recorded */
    int built_in;            /* *Added 2026-11-21 (snatcherLeo: "they'll be built in"):* 1: no switch for players. In normal
                              * play always the normal default (on); settings.ini's key and the environment variable are
                              * ignored (a stored 0 is logged as ignored), and pc_option_set refuses it. Automated runs
                              * keep the old order (environment variable, ORRE_PLAYER_DEFAULTS, automated value), so tests
                              * can still turn it off or on. Accurate mode still forces it off. 0: an ordinary switch */
} PcOption;

int pc_option(const char* id);                /* the effective value (accurate mode: always 0) */
int pc_option_choice(const char* id);         /* the player's own choice, whatever the mode (Settings shows it) */
void pc_option_set(const char* id, int value); /* the player's choice; saved, except in automated runs (refused for a
                                                * built-in entry) */
int pc_option_built_in(const char* id);       /* 1: a built-in entry (no switch; see PcOption.built_in) */

/* GameCube-accurate mode, per save (Phase 2's last item, 2.12; snatcherLeo 2026-11-08 / 11-10 / 11-16). pc_options.c says how
 * the mode is decided:
 * - pc_option_gamecube_accurate: is this run accurate? Until the game starts, the current save's setting (the hub's
 *   pick, so the hub's PLAY can check it); from pc_card_open on, fixed for the rest of the run (never switched while a
 *   game runs). ORRE_PRESET=accurate / normal overrides it for one run (tests).
 * - pc_option_accurate_lock: called once by pc_card_open, as the game is about to read its save.
 * - pc_option_accurate_locked: the mode is fixed (the game has started); Settings locks its rows only then.
 * - pc_option_accurate_new_saves / _set: the per-game default every new save takes (Settings › General). */
int pc_option_gamecube_accurate(void);
void pc_option_accurate_lock(void);
int pc_option_accurate_locked(void);
int pc_option_accurate_new_saves(void);
void pc_option_set_accurate_new_saves(int on);

int pc_option_count(void);
const PcOption* pc_option_at(int index);

/* What accurate mode changes, as the code really behaves (the "What it turns off" list in Settings › General and on the
 * Saves page). One row per OrrePort change; the words are ui/lang keys. A new tweak that accurate mode turns off adds a
 * row here in the same change, as it adds its row to the accurate-mode list (the project rules "The accurate-mode list"). */
typedef enum PcAccurateKind {
    PC_ACCURATE_OFF = 0,  /* turned off in accurate mode */
    PC_ACCURATE_GC = 1,   /* kept at the GameCube's own way (a forced value rather than plain off) */
    PC_ACCURATE_SAME = 2  /* still works: look and sound that keep the random numbers, and OrrePort's own menus */
} PcAccurateKind;
typedef struct PcAccurateRow {
    int kind;        /* PcAccurateKind */
    const char* key; /* its words: a ui/lang key under "accurate.what_off." */
} PcAccurateRow;
int pc_accurate_list_count(void);
const PcAccurateRow* pc_accurate_list_at(int index);

#ifdef __cplusplus
}
#endif

#endif
