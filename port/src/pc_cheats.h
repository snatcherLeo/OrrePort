/*
 * pc_cheats.h: OrrePort's quick cheats (snatcherLeo, 2026-11-13, mockup 63 v2: "the in-game quick cheats should be its own
 * toolbar menu thing"). The Cheats menu in the menu bar over the game (pc_settings_overlay.cpp) switches them; the
 * game code only has small decision hooks whose default is the console's behaviour (docs/port/hook-points.md), set
 * here while a cheat is on and cleared when it is off, so with every cheat off the game runs exactly as on the
 * GameCube.
 *
 * Why it is built this way:
 * - Each switch takes effect at once and can be turned off any time (snatcherLeo: "They act immediately and can be flipped
 *   mid-game"). Switches are never saved: every launch starts with all cheats off, so nobody carries a cheat into a
 *   later sitting without knowing (the project's decision, 2026-11-13).
 * - Every switch is written to a user log, logs/Cheats log.txt next to the program (snatcherLeo: "Every switch is noted in a
 *   user log"), never pruned like the run logs, so a player (or a bug report) can see when cheats were on.
 * - Off in GameCube-accurate mode: that mode keeps every decision hook vanilla (hook-points.md).
 * - "Put in the PC" is a one-off action that changes the save (tops the item up to 99 in the PC's Item Storage); it
 *   runs on the game thread at the frame boundary, like the debug toolkit's commands.
 *
 * Platforms: plain C with SDL's clock and the C library; the same on Windows, Linux and macOS (likely; only Windows
 * built so far).
 */
#ifndef PC_CHEATS_H
#define PC_CHEATS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_CHEAT_INSTANT_WIN,
    PC_CHEAT_INSTANT_CATCH,
    PC_CHEAT_INFINITE_MONEY,
    PC_CHEAT_ITEMS_NEVER_RUN_OUT,
    PC_CHEAT_SWITCH_COUNT
};

int pc_cheats_allowed(void);            /* 0 in GameCube-accurate mode */
int pc_cheat_on(int which);             /* a PC_CHEAT_* switch */
void pc_cheat_set(int which, int on);   /* logs the switch; sets or clears its game hook */
int pc_cheat_exp_multiplier(void);      /* 1 (normal), 2, 5 or 10 */
void pc_cheat_set_exp_multiplier(int times);
int pc_cheats_on_count(void);           /* switches on, the EXP multiplier counting as one (the bar's tag) */
const char* pc_cheat_name_key(int which); /* the text key of a switch's name (cheats.instant_win, ...) */
/* "Instant win, EXP ×2" in the player's language, or "" when none is on (Report a problem) */
void pc_cheats_list(char* out, unsigned size);

/* "Put in the PC": the items of the menu, by index 0..5 (Full Heal added 2026-11-13) */
int pc_cheat_put_count(void);
const char* pc_cheat_put_key(int index); /* the item's text key (cheats.item.master_ball, ...) */
void pc_cheat_put_in_pc(int index);      /* queued; done at the next frame boundary */
int pc_cheat_put_mark(int index);        /* the open list's mark: 0 none, 1 done (a check mark), 2 failed */
void pc_cheat_put_clear_marks(void);     /* the list closed: the marks go */
int pc_cheat_put_take_changed(void);     /* 1 once after a mark changed (the overlay redraws the list) */

void pc_cheats_frame(void);              /* pc_vi.c, each VIWaitForRetrace: runs a queued "Put in the PC" */
/* the last "Put in the PC" result as one line, once ("Put in the PC: Master Balls, now 99"); 0 when there is none.
 * The overlay shows it as a notice over the game. */
int pc_cheats_take_notice(char* out, unsigned size);

/* The game side (src/game/pc_debug_game.c) tops an item up in the PC's Item Storage: returns the count after, -1 if
 * the storage is full, -2 if no game is loaded. Registered at start-up; NULL until then. */
extern int (*pc_cheats_pc_top_up)(int item, int target);
/* ... and fills the wallet to the game's cap (9,999,999) when Infinite money is switched on: returns the money after,
 * -2 if no game is loaded */
extern int (*pc_cheats_money_fill)(void);

#ifdef __cplusplus
}
#endif

#endif
