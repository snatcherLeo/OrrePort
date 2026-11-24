/*
 * pc_settings_log.cpp: the logs carry every setting (step 3.7, the project's build helper, 2026-11-20; decided in
 * docs/port/tweaks-37-research.md section 8, the last item of 3.7).
 *
 * Why: a tester's log (and a problem report, which carries the log) must say exactly how the game was set up, so a bug
 * or a replay difference can be put down to a setting. Before this the log had no snapshot: settings appeared as
 * scattered lines, many only when a feature first ran or only when it was on, change lines rarely said when, and the
 * monitor never appeared at all (research 8, "What the log prints today").
 *
 * What the log now carries, in order:
 *   1. At start-up, before the hub (pc_settings_log_snapshot("start"), pc_game_main.c): the display (monitor, its mode
 *      and refresh rate, scaling), the controllers SDL sees and whether Player 1 is a GameCube controller, every
 *      settings.ini key as stored (the Settings rows, controller bindings and shortcuts, the walk button, the stick's
 *      range and dead zone...; absent keys are at their defaults), every Tweaks entry (pc_options.c: the player's choice
 *      and what it is now), and every ORRE_* variable set for this run.
 *   2. When the Settings rows have read their values (the overlay's set-up, in the hub or at the game's first frame):
 *      every Settings row as the page shows it, one line per category (pc_settings_overlay.cpp log_rows), so no row
 *      can be missing (a new row is logged with no extra work).
 *   3. When the game starts and GameCube-accurate mode is fixed for the run (pc_options.c, pc_option_accurate_lock):
 *      the mode and every Tweaks entry as it applies in this run, then the rows again (locked ones marked).
 *   4. Every change: "[settings] <key>: <old> -> <new> (game frame N | hub +N.N s)" from the settings file's setters
 *      (pc_settings.c), and the Tweaks rows' own "[options] <id> on/off (...)" line with the same time.
 *
 * Platforms: SDL's display, gamepad and environment calls, the same on Windows, Linux and macOS (likely: run on Windows
 * only). What differs per OS: "scaling" is Windows' display scaling (SDL's content scale); on macOS it is the Retina
 * factor and on Linux the desktop's scale, which SDL reports the same way (likely).
 * For XD: the same file serves XD's port unchanged (OrrePort's own settings, not the game's).
 */
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "pc_options.h"
#include "pc_settings.h"

/* the pad layer's (pc_pad_shape.cpp): in the Settings overlay's builds only, so weak (absent: "unknown") */
extern "C" int pc_stick_player1_is_gamecube(void) __attribute__((weak));

namespace {

void log_display() {
    int n = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&n);
    for (int i = 0; displays != nullptr && i < n; ++i) {
        const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(displays[i]);
        const char* name = SDL_GetDisplayName(displays[i]);
        std::printf("[settings] display %d%s: %s, %d x %d at %.2f Hz, scaling %.0f%%\n", i + 1,
                    displays[i] == SDL_GetPrimaryDisplay() ? " (main)" : "", name != nullptr ? name : "(no name)",
                    m != nullptr ? m->w : 0, m != nullptr ? m->h : 0, m != nullptr ? m->refresh_rate : 0.0f,
                    SDL_GetDisplayContentScale(displays[i]) * 100.0f);
    }
    if (displays == nullptr || n == 0) std::printf("[settings] display: none reported (%s)\n", SDL_GetError());
    SDL_free(displays);
}

void log_controllers() {
    int n = 0;
    SDL_JoystickID* pads = SDL_GetGamepads(&n);
    std::string line;
    for (int i = 0; pads != nullptr && i < n; ++i) {
        const char* name = SDL_GetGamepadNameForID(pads[i]);
        line += (line.empty() ? "" : "; ") + std::string(name != nullptr ? name : "(no name)");
    }
    SDL_free(pads);
    std::printf("[settings] controllers: %s; Player 1 %s\n", line.empty() ? "none" : line.c_str(),
                pc_stick_player1_is_gamecube == nullptr  ? "unknown"
                : pc_stick_player1_is_gamecube() != 0    ? "is a GameCube controller"
                                                         : "is not a GameCube controller");
}

void log_file() {
    std::string line;
    for (int i = 0; i < pc_settings_count(); ++i) {
        line += (line.empty() ? "" : "; ") + std::string(pc_settings_key_at(i)) + " = " + pc_settings_value_at(i);
    }
    std::printf("[settings] settings.ini (%d keys; keys not listed are at their defaults): %s\n", pc_settings_count(),
                line.empty() ? "(empty: every setting at its default)" : line.c_str());
}

void log_env() {
    std::string line;
    if (char** vars = SDL_GetEnvironmentVariables(SDL_GetEnvironment())) {
        for (char** v = vars; *v != nullptr; ++v) {
            if (std::strncmp(*v, "ORRE_", 5) == 0) line += (line.empty() ? "" : "; ") + std::string(*v);
        }
        SDL_free(vars);
    }
    std::printf("[settings] ORRE_ variables: %s\n", line.empty() ? "none" : line.c_str());
}

}  // namespace

/* every Tweaks entry (pc_options.c): the player's choice and the value that applies now (0 in accurate mode, the
 * automated value in recordings). Called at start-up and when accurate mode is fixed for the run. All platforms */
extern "C" void pc_settings_log_options(const char* when) {
    std::string line;
    for (int i = 0; i < pc_option_count(); ++i) {
        const PcOption* o = pc_option_at(i);
        if (o == nullptr) continue;
        const int choice = pc_option_choice(o->id), now = pc_option(o->id);
        line += (line.empty() ? "" : "; ") + std::string(o->id) + " " + (now ? "on" : "off");
        /* *2026-11-21 (snatcherLeo: seven tweaks built in):* a built-in entry has no player's choice, so the log names it as
         * built in instead; "(test value)" when an automated run set it apart from the built-in default */
        if (o->built_in) {
            line += " (built in";
            if (choice != o->normal_default) line += ", test value";
            line += ")";
        } else if (choice != now) {
            line += std::string(" (chosen ") + (choice ? "on" : "off") + ")";
        }
    }
    std::printf("[settings] tweaks %s: %s\n", when, line.c_str());
}

/* the start-up snapshot (pc_game_main.c, after the platform and the controls are set up, before the hub) */
extern "C" void pc_settings_log_snapshot(const char* when) {
    std::printf("[settings] snapshot %s\n", when);
    log_display();
    log_controllers();
    log_file();
    pc_settings_log_options(when);
    log_env();
}
