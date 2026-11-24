/*
 * pc_controls.c: the controls' defaults (Settings > Controls; snatcherLeo, 2026-11-12: "Yes, build it").
 *
 * Controllers already get their own defaults from Aurora, by controller type (extern/aurora/lib/dolphin/pad/pad.cpp,
 * default_buttons). The keyboard had none: Aurora leaves every key unbound and a port's keyboard off until something
 * turns it on, and nothing in OrrePort did, so the keyboard could not play the game at all. This gives Player 1 the
 * keyboard layout snatcherLeo chose ("I think WASD"), set before the game's PADInit (Aurora takes defaults only before it):
 *
 *   control stick W A S D      A B X Y   J K L I      L / R  Q / E      Z  Left Shift
 *   START         Enter        D-pad     the arrows   C-Stick T F G H (the WASD diamond one step right)
 *
 * (Esc stays the OrrePort menu's key, pc_settings_overlay.cpp.) The keyboard and any controller drive Player 1 at the
 * same time. A keyboard_bindings.dat in the user folder (written once the Controls screen saves a change) replaces
 * these. Automated runs leave the keyboard off and unbound, so replays and recordings are untouched.
 */
#include <dolphin/pad.h>
#include <SDL3/SDL_scancode.h>
#include <stdio.h>

extern int pc_platform_automated_run(void);

void pc_controls_init(void) {
    if (pc_platform_automated_run()) {
        return;
    }
    static const PADDefaultKeyBindings kKeyboard = {
        .buttons =
            {
                {SDL_SCANCODE_LEFT, PAD_BUTTON_LEFT},
                {SDL_SCANCODE_RIGHT, PAD_BUTTON_RIGHT},
                {SDL_SCANCODE_DOWN, PAD_BUTTON_DOWN},
                {SDL_SCANCODE_UP, PAD_BUTTON_UP},
                {SDL_SCANCODE_LSHIFT, PAD_TRIGGER_Z},
                {SDL_SCANCODE_E, PAD_TRIGGER_R},
                {SDL_SCANCODE_Q, PAD_TRIGGER_L},
                {SDL_SCANCODE_J, PAD_BUTTON_A},
                {SDL_SCANCODE_K, PAD_BUTTON_B},
                {SDL_SCANCODE_L, PAD_BUTTON_X},
                {SDL_SCANCODE_I, PAD_BUTTON_Y},
                {SDL_SCANCODE_RETURN, PAD_BUTTON_START},
            },
        /* the stick's Y is positive upwards (the GameCube's own direction, Aurora's PAD_AXIS_LEFT_Y_POS);
         * "influence" is not read by Aurora (each key gives the full tilt) */
        .axes =
            {
                {SDL_SCANCODE_D, PAD_AXIS_LEFT_X_POS, 1},
                {SDL_SCANCODE_A, PAD_AXIS_LEFT_X_NEG, 1},
                {SDL_SCANCODE_W, PAD_AXIS_LEFT_Y_POS, 1},
                {SDL_SCANCODE_S, PAD_AXIS_LEFT_Y_NEG, 1},
                {SDL_SCANCODE_H, PAD_AXIS_RIGHT_X_POS, 1},
                {SDL_SCANCODE_F, PAD_AXIS_RIGHT_X_NEG, 1},
                {SDL_SCANCODE_T, PAD_AXIS_RIGHT_Y_POS, 1},
                {SDL_SCANCODE_G, PAD_AXIS_RIGHT_Y_NEG, 1},
                {SDL_SCANCODE_Q, PAD_AXIS_TRIGGER_L, 1},
                {SDL_SCANCODE_E, PAD_AXIS_TRIGGER_R, 1},
            },
    };
    const BOOL ok = PADSetDefaultKeyBindings(0, &kKeyboard);
    /* on before PADInit: PADInit keeps this switch and only a saved keyboard file replaces it */
    PADSetKeyboardActive(0, TRUE);
    printf("[controls] keyboard for Player 1: W A S D, J K L I = A B X Y, Q / E = L / R, Left Shift = Z, Enter = "
           "START, the arrows = D-pad, T F G H = C-Stick (%s)\n",
           ok ? "default keys set" : "defaults NOT set: PADInit had already run");
    /* Aurora's PADInit now, before the hub (snatcherLeo, 2026-11-13: keys set on the Controls screen "still says "None""):
     * PADInit is what puts these defaults in place and loads keyboard_bindings.dat, and until it runs the keyboard
     * reads as unbound. It used to run only when the game started, after the hub, so the screen opened from the hub
     * showed every key as "none" (and Reset the keyboard did nothing). The game's own PADInit later returns at once.
     * Automated runs return above, so replays are untouched. */
    PADInit();
    /* A keyboard_bindings.dat saved while the keyboard was off (by builds before the line above, which saved from the
     * hub before the keyboard was ever turned on) turns it off again on every start: every key then reads "none" on
     * the Controls screen and the keyboard does nothing in game, while the keys stored in the file are fine (snatcherLeo,
     * 2026-11-13: "it keeps on saying "none""; his file's first port byte was 0). OrrePort never turns Player 1's
     * keyboard off, so it is turned back on here, keeping the file's keys; the next save writes the flag right. */
    {
        u32 count = 0;
        if (PADGetKeyButtonBindings(0, &count) == NULL) {
            PADSetKeyboardActive(0, TRUE);
            printf("[controls] keyboard_bindings.dat had Player 1's keyboard off: turned back on (its saved keys kept)\n");
        }
    }
}
