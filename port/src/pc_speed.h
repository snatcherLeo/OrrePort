/* pc_speed.h: Settings > Enhancements > Speed and the fast-forward key (pc_speed.c says why it works this way).
 * settings.ini keys (each a saved code; for the three speed lists the code is not always the row's position, see
 * below; port/src/pc_settings_overlay.cpp):
 *   speed_ff       Fast-forward: 0 2x, 1 3x (the default), 2 5x, 3 4x
 *   speed_ff_mode  Fast-forward works: 0 Hold (the default), 1 Press to switch
 *   speed_badge    Speed badge: 1 Show (the default), 0 Hide
 *   speed_game     Game speed: 0 Normal, 1 1.5x, 2 2x, 3 3x, 4 5x, 5 4x
 *   speed_battle   Battles: 0 Same as Game speed, 1 Normal, 2 1.5x, 3 2x, 4 3x, 5 5x, 6 4x
 *   speed_sound    Music when faster: 0 Normal tempo (the default; the sound split), 1 Faster too
 * The three older keys keep their names and meanings (the 5x choices were added at the end of each list), so a
 * settings.ini from before the fast-forward key needs no change. The button itself: pc_pad_shape.h (ff_pad, ff_keys).
 *
 * Why the codes and the rows differ (2026-11-17; snatcherLeo, 2026-11-16: "yeah add 4x next time", the decision log "4x joins
 * the speed lists"): 4x came after 5x, so it took the next free code at the end of each list (a saved 5x still reads
 * as 5x, no migration, and an older exe reading a 4x code falls back to its default), while the rows show the speeds
 * in order (..., 3x, 4x, 5x). pc_speed_row_for_code / pc_speed_code_for_row translate between the two; the tables
 * are in pc_speed.c. Any later speed is added the same way: a new code at the end, placed where it belongs in the row
 * order. All platforms (plain C). */
#ifndef PC_SPEED_H
#define PC_SPEED_H

#ifdef __cplusplus
extern "C" {
#endif

void pc_speed_frame(void); /* pc_vi.c, each VIWaitForRetrace, before the pacing wait */

/* the speed badge ("▶▶ 3×"; pc_settings_overlay.cpp draws it): a number that goes up each time the badge should show
 * (fast-forward starts, or the speed changes to a faster one), with the speed in percent. 0 until the first time. */
unsigned pc_speed_badge_serial(unsigned* percent);

/* the fast-forward speed's default saved code (3x; row 1 too) */
#define PC_SPEED_FF_DEFAULT 1

/* the three speed lists whose saved code and row position differ (see the top of this file) */
enum { PC_SPEED_LIST_FF, PC_SPEED_LIST_GAME, PC_SPEED_LIST_BATTLE };
/* the saved code of the list's row-th choice, or -1 if out of range */
int pc_speed_code_for_row(int list, int row);
/* the row position of a saved code, or -1 for an unknown code (an older or newer settings.ini) */
int pc_speed_row_for_code(int list, int code);

#ifdef __cplusplus
}
#endif

#endif
