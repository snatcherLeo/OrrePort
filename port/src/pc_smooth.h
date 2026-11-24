/* pc_smooth.h: frame smoothing's port side, steps (a) and (b) (pc_smooth.c says how it works and why; the plan is
 * docs/port/frame-smoothing-plan.md). The player's Settings row (below) or a test switch turns it on:
 * ORRE_SMOOTH=match|<pictures a second>|off (the live presenter) or ORRE_TEST_SMOOTH=<n> (recorded runs: n in-between
 * pictures after every step); the test switches win over the row. */
#ifndef PC_SMOOTH_H
#define PC_SMOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

/* pc_vi.c's end_frame, just before aurora_end_frame: the step's retrace count and why it ended (PC_SMOOTH_STEP_*) */
enum {
    PC_SMOOTH_STEP_COPY = 0,    /* the game's display copy (a normal step) */
    PC_SMOOTH_STEP_NO_COPY = 1, /* ended at the retrace without a display copy (early boot, a long hold) */
    PC_SMOOTH_STEP_MOVIE = 2,   /* a movie frame */
    PC_SMOOTH_STEP_PAUSE = 4,   /* a frame of the Settings overlay's pause */
};
void pc_smooth_step(unsigned why, unsigned retrace);
void pc_smooth_after_end_frame(void); /* right after aurora_end_frame: the test mode waits for the in-betweens */
void pc_smooth_frame(void);           /* each VIWaitForRetrace (game thread): the rate, the log lines */
void pc_smooth_hold(int on, const char* why); /* a pause: no in-between pictures while held */
int pc_smooth_on(void);               /* smoothing is running (from the first game step on) */
void pc_smooth_check_rng_draw(void);  /* the tripwire: a random draw during an in-between picture stops the program */
void pc_smooth_room_entered(unsigned room); /* step (c): a room change resets the camera blend's history */

/* The Settings row (Settings › Graphics › Frame rate, 2026-11-18): settings.ini "frame_smoothing" = off | match | 60 |
 * 120 | 144 | 165 | 240; "off" (Off, original) by default since 2026-11-23 (snatcherLeo: "the FPS should be on original";
 * it was "match", Match your display, from 2026-11-16). The overlay saves the choice, then calls
 * pc_smooth_setting_changed; smoothing follows it from the next game step (test switches win; pc_smooth.c). */
#define PC_SMOOTH_SETTING_DEFAULT "off"
void pc_smooth_setting_changed(void);

#ifdef __cplusplus
}
#endif

#endif
