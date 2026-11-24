/* pc_pad_shape.h: Player 1's stick shaping and the OrrePort menu shortcut (pc_pad_shape.cpp; docs/port/controls.md
 * "Sticks" and "The menu shortcut"). Platform layer only; nothing here runs while a pad script drives the game. */
#ifndef PC_PAD_SHAPE_H
#define PC_PAD_SHAPE_H

#include <dolphin/pad.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_STICK_MAIN 0
#define PC_STICK_C 1
#define PC_CAL_SAMPLES 32

/* ---- the sticks (snatcherLeo, 2026-11-12: "do what Dolphin does for stick sensitivity and rotation") ---- */
int pc_stick_range(int stick);           /* full tilt in GameCube units (101 / 92 by default, Dolphin's gate figures) */
int pc_stick_range_default(int stick);
void pc_stick_set_range(int stick, int units);
int pc_stick_dead(int stick);            /* the dead zone in GameCube units */
void pc_stick_set_dead(int stick, int units);
int pc_stick_dead_limit(void);           /* 10 in GameCube-accurate mode, 40 otherwise */
int pc_stick_snap(void);                 /* snap to 8 directions (0 / 1) */
void pc_stick_set_snap(int on);
int pc_stick_walk_button(void);          /* a controller button (SDL_GamepadButton) held to walk; -1 none */
int pc_stick_walk_key(void);             /* a key (SDL_Scancode) held to walk; -1 none */
int pc_stick_walk_pct(void);
int pc_stick_walk_held(void);            /* the walk button or key held right now (step 3.7's right-stick walking) */
void pc_stick_set_walk_button(int native);
void pc_stick_set_walk_key(int scancode);
void pc_stick_set_walk_pct(int pct);
int pc_stick_has_calibration(int stick);
/* the recorded reach (PC_CAL_SAMPLES radii, sample i at angle i / PC_CAL_SAMPLES of a turn); 0 if none (then 1s) */
int pc_stick_get_calibration(int stick, float* radii);
void pc_stick_set_calibration(int stick, const float* radii /* PC_CAL_SAMPLES, 0..1.5 */, float cx, float cy);
void pc_stick_clear_calibration(int stick);
int pc_stick_player1_is_gamecube(void);  /* Player 1's controller is a GameCube controller (the adapter) */

/* what the game is given for a raw stick position (x, y in -1..1, up positive): GameCube units, rounded and clamped */
void pc_stick_shape(int stick, float x, float y, int walk, int gamecube, int* out_x, int* out_y);

/* ---- the menu shortcut (one or two buttons; snatcherLeo: "allow for button combinations") ---- */
/* a shortcut member: a controller button (SDL), a GameCube button (its PAD_ mask), or a key (SDL scancode) */
enum { PC_SC_NATIVE = 0, PC_SC_GC = 1, PC_SC_KEY = 2 };
typedef struct {
    int type;
    int value;
} PcShortcutMember;
/* the controller's (keyboard 0) or the keyboard's (keyboard 1) shortcut; returns how many members (1 or 2) */
int pc_shortcut_get(int keyboard, PcShortcutMember out[2]);
void pc_shortcut_set(int keyboard, const PcShortcutMember* members, int count); /* count 0: back to the default */
int pc_shortcut_is_default(int keyboard);
const char* pc_shortcut_name(int keyboard);  /* "Z + L", "View", "Esc", "LB + Start"... */
int pc_shortcut_held(int keyboard);         /* every member held right now (the device's own state) */
int pc_shortcut_by_event(int keyboard);     /* one member, not a GameCube button: opened by the press event alone */

#define PC_TRIGGER_CLICK_PCT 90 /* a controller's analog trigger clicks past 90 % of its travel (Dolphin's default) */

/* ---- the Quit to hub shortcut: a controller's one or two buttons, none by default (snatcherLeo, 2026-11-13) ---- */
int pc_quit_shortcut_get(PcShortcutMember out[2]); /* how many members; 0: none set */
void pc_quit_shortcut_set(const PcShortcutMember* members, int count); /* count 0: none */
const char* pc_quit_shortcut_name(void); /* "None", "Z + START"... */

/* ---- the fast-forward button (Settings › Enhancements › Speed and Controls' "Fast-forward" row; snatcherLeo, 2026-11-16:
 * "Fine, LB works"): one controller button and one key, each its own (settings.ini ff_pad / ff_keys, in the menu
 * shortcut's form; "" the default, "none" not set). Default: LB on a controller (L1 on PlayStation; the game never
 * uses it: RB = Z, LT / RT = L / R), Tab on the keyboard, none on a GameCube controller. The default counts only while
 * that button does nothing else, so the game never sees the fast-forward button; a button the player chose although it
 * is a game button ("Use it anyway") is held back from the game by the read filter. pc_pad_shape.cpp says why. ---- */
enum { PC_FF_FREE = 0, PC_FF_GAME = 1, PC_FF_STICK = 2, PC_FF_ORREPORT = 3 };
int pc_ff_get(int keyboard, PcShortcutMember* out);           /* 1 and *out when set, 0 when not */
void pc_ff_set(int keyboard, const PcShortcutMember* member); /* NULL: not set */
void pc_ff_reset(int keyboard);                               /* back to the default */
const char* pc_ff_name(int keyboard);                         /* "LB", "L1", "Tab"; "" when not set */
const char* pc_ff_member_name(const PcShortcutMember* m);     /* a button's or key's name as printed */
int pc_ff_held(void);                                         /* either device's button held right now */
/* what a button (keyboard 0) or key (1) does already: PC_FF_FREE, PC_FF_GAME (*gc: its GameCube button's PAD_ mask),
 * PC_FF_STICK (a stick direction), PC_FF_ORREPORT (the menu or Quit to hub shortcut, the walk button, F2 / F8 / F11) */
int pc_ff_conflict(int keyboard, PcShortcutMember m, unsigned* gc);
const char* pc_ff_gc_name(unsigned gc); /* a GameCube button's name ("L", "Z") for the conflict question */
void pc_ff_first_start_check(void);     /* once per device: a default that is taken starts as "not set" (logged) */

/* Player 1's read filter (inside PADRead, before the recorder): the triggers, the shortcuts' hold-back and the stick
 * shaping.
 * Returns the shortcuts completed this read (PC_FILTER_ flags; 0 none). */
#define PC_FILTER_OPEN_MENU 1
#define PC_FILTER_QUIT_TO_HUB 2
int pc_pad_filter_player1(PADStatus* status);

#ifdef __cplusplus
}
#endif
#endif
