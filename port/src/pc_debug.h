/*
 * pc_debug.h: OrrePort's debug toolkit (snatcherLeo, 2026-10-16; testing-plan.md 4.1). PC only, off unless ORRE_DEBUG=1
 * (later a settings switch); never active in faithful mode unless turned on.
 *
 * Split (agreed with Main): the game side implements PcDebugApi in a PC-only game file and hands it over once with
 * pc_debug_register() during start-up; the port side (B: this header, pc_debug.c, pc_debug_ui.cpp) owns the in-game
 * menu (ImGui, F1), the command line (ORRE_DEBUG_CMD) and pad-script commands (WHEN:!command). Every command is
 * queued and run on the game thread at the frame boundary (VIWaitForRetrace), so the game side's functions are
 * called with the game in a consistent state, the same place the console's retrace callback would run.
 *
 * Commands (the same words in the menu's log, ORRE_DEBUG_CMD="a; b" and pad scripts):
 *   warp ROOM [ENTRY]          flag ID VALUE / flag ID (prints)      item ID COUNT
 *   pokemon SPECIES LEVEL      team NAME (a preset test team)        battle win|lose
 *   seed VALUE                 speed PERCENT (10..1000, port side)   skipmovies on|off
 *   save SLOT / load SLOT      (test checkpoints)                    help
 *   fight ID (start battle ID from the field)                       heaps (SDK heap use, pc_heap.c)
 *   crash (an access violation on purpose: tests the crash report and the run log)
 *   cheat win|catch|money|items on|off, cheat exp 1|2|5|10, cheat put 0..5 (the Cheats menu's switches; pc_cheats.h)
 *   bmcourse N [MODE] (Battle Mode's course 0-6 and mode, the stadium header's numbers; src/game/pc_debug_game.c,
 *   added 2026-11-15)
 * Numbers are decimal or 0x hex. Each command logs "[debug] ..." with its result.
 */
#ifndef PC_DEBUG_H
#define PC_DEBUG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int PcDebugU32;

/* Implemented by the game side. Each returns 1 on success, 0 if refused (for example "not possible in this room");
 * a NULL entry means "not available yet". All are called on the game thread at the frame boundary. */
typedef struct PcDebugApi {
    int (*warp)(PcDebugU32 room, PcDebugU32 entry);
    int (*set_flag)(PcDebugU32 id, PcDebugU32 value);
    int (*get_flag)(PcDebugU32 id, PcDebugU32* value);
    int (*give_item)(PcDebugU32 item, PcDebugU32 count);
    int (*give_pokemon)(PcDebugU32 species, PcDebugU32 level);
    int (*set_team)(const char* preset); /* a named preset test team (the game side defines the presets) */
    int (*battle_result)(int win);       /* end the current battle as a win (1) or a loss (0) */
    int (*set_rng_seed)(PcDebugU32 seed);
    int (*skip_movies)(int on);          /* movies and cutscenes skipped while on */
    int (*checkpoint_save)(int slot);
    int (*checkpoint_load)(int slot);
    PcDebugU32 (*current_room)(void);    /* for the menu's status line */
    /* Added 2026-10-17 (the project's request; new entries go at the end, so positional initialisers stay valid): */
    int (*fight_start)(PcDebugU32 battle_id); /* start battle ID from the field, as the developers' debug battle test
                                                * does; the game side refuses unsafe states (a battle running, a room
                                                * change or menu room in progress) */
    const char* (*room_name)(PcDebugU32 room); /* the room's archive name (e.g. "S1_out") for the menu header; NULL
                                                 * if unknown */
    PcDebugU32 (*battle_number)(void);   /* the current encounter id, 0 outside a battle */
    int (*get_rng_state)(PcDebugU32* out); /* the game's random state (2026-10-26: "seed ... keep") */
    /* A people slot's wander state (2026-10-27, a replay resync for P02's Phenac townsperson): v = position
     * x y z, walk target x y z, pause, yaw, state, sub-state (the columns of ORRE_NPC_LOG / Dolphin's npc probe) */
    int (*set_npc)(PcDebugU32 slot, const float* v);
} PcDebugApi;

/* Game side, once during start-up. The pointer must stay valid (a static table). */
void pc_debug_register(const PcDebugApi* api);

/* Port side */
int pc_debug_enabled(void);              /* ORRE_DEBUG=1 */
void pc_debug_queue(const char* line);   /* one or more commands separated by ';', from any thread */
void pc_debug_run_now(const char* line); /* one command, at once, on the game thread (room-hook replay seeds) */
void pc_debug_frame(unsigned frame);     /* pc_vi.c, each VIWaitForRetrace: runs queued commands */
void pc_debug_room_entered(unsigned room); /* pc_game_main.c's room hook: a checkpoint load waits for the title */
void pc_debug_set_build_stamp(const char* stamp); /* pc_game_main.c at start-up (the stamp is generated at link time) */
void pc_debug_hotkeys(void);             /* pc_vi.c, each frame: F5 / F9 / F12 / Pause / '.' (ORRE_DEBUG=1 only) */
void pc_debug_pause_point(void (*poll)(void), void (*present)(void), const int* exit_requested); /* pc_vi.c */
const unsigned char* pc_debug_pause_image(int* serial); /* the paused picture (640x480 RGBA8) or NULL */
void pc_debug_step(void);                /* '.' / 'step': one frame while paused */
int pc_debug_paused(void);
void pc_debug_set_hotkey_slot(int slot); /* the menu's checkpoint slot, used by F5 / F9 */
void pc_debug_slot_label(int slot, char* out, size_t size); /* "label (date, room)" or "" */
int pc_debug_spot_count(void);           /* quick jumps from spots.json (read on first use) */
const char* pc_debug_spot_id(int i);
const char* pc_debug_spot_name(int i);
extern void (*pc_debug_audio_pause)(int on); /* set by pc_game_main.c (MusyX's PC output) */
const char* pc_debug_build_stamp(void);  /* for the menu header; a placeholder until set */
const char* pc_debug_log_line(int i);    /* the menu's log, newest last; NULL past the end */
int pc_debug_log_count(void);
const PcDebugApi* pc_debug_api(void);
unsigned pc_vi_speed_percent(void);      /* pc_vi.c: retrace pacing, 100 = the GameCube's 59.94 Hz */
void pc_vi_set_speed_percent(unsigned percent);
void pc_debug_ui_draw(void);             /* pc_debug_ui.cpp: inside Aurora's frame, before aurora_end_frame */
void pc_debug_ui_toggle(void);           /* pc_debug_ui.cpp: open / close the debug menu, as F1 (Help › Debug menu) */

#ifdef __cplusplus
}
#endif

#endif
