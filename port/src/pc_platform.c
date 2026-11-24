/*
 * pc_platform.c: the port's startup in one call, in the order it has to happen (docs/port/platform-layer-plan.md,
 * docs/port/low-memory.md, saves.md, sdk-threads.md). Used by orreport and by the game's PC main.
 *
 *   1. crash handler (a crash prints a call stack for addr2line);
 *   2. GameCube main memory at 0x80000000, the locked cache at 0xE0000000, the port's low-memory region: before
 *      any library can take those ranges;
 *   3. Aurora (SDL, the GPU), told to use our MEM1 for the game's arena;
 *   4. DVD completion callbacks on the game thread (before any read); the disc image, if given (before DVDInit,
 *      which the game calls);
 *   5. the memory card (before the game's own CARDInit(), which then does nothing);
 *   6. OSInit (the game calls it again first thing; safe twice);
 *   7. the first frame is begun, so the game's first GX commands have a frame to go into.
 * The game then runs its main; VIWaitForRetrace (pc_vi.c) ends each frame. Audio is set up by the game's own sound
 * init (sndInit); the PC side adds sndPCSetSynchronization (the interrupt lock) there: see musyx-evaluation.md.
 */
#include <aurora/aurora.h>
#include <aurora/main.h>
#include <limits.h> /* INT_MIN: "no window place saved" */
#ifdef PC_HAVE_AURORA_DVD
#include <aurora/dvd.h>
#include <dolphin/dvd.h> /* DVDGetCurrentDiskID: the open image's game ID */
#endif
#include <dolphin/os.h>
#include <dolphin/pad.h> /* PADControlMotor: rumble stops when the window loses focus */
#include <dolphin/gx/GXAurora.h> /* AuroraSetViewportPolicy */

#include "pc_crash.h"
#include "pc_lowmem.h"
#include "pc_mem.h"

#include "pc_paths.h" /* where every file lives (the portable and flat layouts, 4.1) */
#include "pc_settings.h"
#include "pc_text.h" /* the player's language, for the message boxes */

#include <aurora/event.h>

#include <SDL3/SDL_audio.h> /* SDL_GetAudioDeviceName: sound outputs in the log */
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_timer.h> /* SDL_GetTicks: when the fullscreen switch's animations come back on */
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pc_vi_begin(void);
void pc_dvd_install(void);
void pc_card_init(void);

/* Aurora's windowCreatedCallback: the window's look before it is first shown. With the RmlUi trial's overlay
 * (pc_settings_overlay.cpp) that is the saved icon colour, the title-bar colours and the trial's title, so the window
 * never appears with SDL's default icon, "OrrePort" or the build stamp first (snatcherLeo, 2026-11-09: "Fix teh flickering
 * too o fthe older icon, and the older name") */
void pc_startup_mark(const char* what); /* pc_log.c: "[startup] <ms> ms: <what>" */
static int automated_run(void);           /* below */
static int automated_window_minimized(void); /* below */
#ifdef ORRE_SETTINGS_OVERLAY
void pc_window_style_early(void);
#endif
static void window_created(SDL_Window* window) {
    pc_startup_mark("window created, graphics device ready");
#ifdef ORRE_SETTINGS_OVERLAY
    pc_window_style_early();
    pc_startup_mark("window styled (icon, title bar)");
#endif
    /* An automated run's window is minimized before it is first shown (2026-11-15; found by the piece 4 helper
     * in its logs): it used to be shown at its normal size and minimized only after aurora_initialize returned (the GPU
     * pipeline cache, ImGui, RmlUi and ARAM setup), so for that while a test window could come up over snatcherLeo's screen.
     * SDL keeps a minimize asked for on a hidden window and applies it when the window is shown (likely, SDL 3's
     * pending window flags; the log's "[window] flags right after creation" line says what happened). It isn't
     * activated when shown either (SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, read by SDL_ShowWindow), so it never takes the
     * keyboard from the game being played. apply_window_mode below still runs as the backstop.
     * Platforms: the same calls on Windows, Linux and macOS. Windows honours both (likely: SDL's Windows code shows a
     * window with a pending minimize minimized); on X11 and Wayland minimizing is only a request the window manager may
     * refuse, and Wayland can't minimize a window that was never shown (guess, unchecked: the backstop then applies);
     * macOS minimizes into the Dock (likely). */
    if (automated_window_minimized()) {
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
        printf("[window] minimize requested before the first show (automated run)%s\n",
               SDL_MinimizeWindow(window) ? "" : ": FAILED");
    }
}

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int len) {
    static const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    FILE* out = level >= LOG_ERROR ? stderr : stdout;
    (void)len;
    fprintf(out, "[%s: %s; %s]\n", (unsigned)level < 5 ? names[level] : "?", module, message);
    if (level == LOG_FATAL) {
        fflush(out);
        pc_crash_print_stack("fatal log message; called from:");
        abort();
    }
}

/* An automated run: scripted input (ORRE_PAD_SCRIPT) or a test window (ORRE_WINDOW=minimized). Such a window starts
 * minimized and says so in its title, so it is never mistaken for the build being played (2026-10-17: a scripted
 * battle run of the project's exe popped up titled "MAIN BUILD (play this one)"). */
extern volatile u32 pc_vi_heartbeat; /* pc_vi.c: the game frame count */
void pc_textures_init(const char* pack_folder, int automated); /* pc_textures.cpp */

static int automated_run(void) {
    const char* mode = getenv("ORRE_WINDOW");
    return getenv("ORRE_PAD_SCRIPT") != NULL || (mode != NULL && strcmp(mode, "minimized") == 0);
}

/* whether an automated run's window is kept minimized: every automated run, except ORRE_WINDOW=normal (below) */
static int automated_window_minimized(void) {
    const char* mode = getenv("ORRE_WINDOW");
    return automated_run() && !(mode != NULL && strcmp(mode, "normal") == 0);
}

/* settings.ini "anisotropy" (Settings > Graphics > Texture filtering): 1 (None, the game's own filtering; the default
 * since 2026-11-13, snatcherLeo: "should be on none for new users"; was 16), 2, 4, 8 or 16. A settings.ini that already has a
 * value keeps it; only a missing or unknown value falls back to None */
static int aniso_setting(void) {
    const int v = pc_settings_int("anisotropy", 1);
    return v == 2 || v == 4 || v == 8 || v == 16 ? v : 1;
}

/* The window title, e.g. with the build stamp (which stays in the log either way). Not through AuroraConfig.appName:
 * Aurora also named the save folder after it, which must not change with every build. An automated run's title
 * names the lane that started it (ORRE_RUN_WHY, saferun's --why: the text before ':', e.g. "D") instead. */
void pc_platform_set_title(const char* title) {
    char automated[96];
    int count = 0;
    SDL_Window** windows;

    if (automated_run()) {
        const char* why = getenv("ORRE_RUN_WHY");
        const char* colon = why != NULL ? strchr(why, ':') : NULL;
        int len = colon != NULL ? (int)(colon - why) : 0;
        if (len <= 0 || len > 24) {
            snprintf(automated, sizeof(automated), "OrrePort AUTOMATED TEST RUN (test) - do not play");
        } else {
            snprintf(automated, sizeof(automated), "OrrePort AUTOMATED TEST RUN (%.*s) - do not play", len, why);
        }
        title = automated;
    }
    printf("[window] title: %s\n", title);
    windows = SDL_GetWindows(&count);
    for (int i = 0; windows != NULL && i < count; i++) {
        SDL_SetWindowTitle(windows[i], title);
    }
    SDL_free(windows);
}

/* Automated runs (ORRE_WINDOW=minimized or any ORRE_PAD_SCRIPT) start with the window minimized, so nobody mistakes
 * a test window for the game they are playing (2026-10-17: snatcherLeo played a lane test window; a scripted run popped up
 * on his screen). The game keeps running and drawing. */
static void apply_window_mode(void) {
    int count = 0;
    SDL_Window** windows;

    if (!automated_run()) { /* also every pad-scripted run: it never needs focus */
        return;
    }
    {
        /* ORRE_WINDOW=normal (2026-10-18, A's request for A14): a scripted run stays un-minimized, so its window is never
         * minimized, restored or resized (the wedges may come from re-creating the frame buffers on a resize). Still
         * titled as a test run and still ignoring real input; use it only when snatcherLeo isn't playing */
        const char* mode = getenv("ORRE_WINDOW");
        if (mode != NULL && strcmp(mode, "normal") == 0) {
            printf("[window] automated run, left un-minimized (ORRE_WINDOW=normal)\n");
            return;
        }
    }
    windows = SDL_GetWindows(&count);
    for (int i = 0; windows != NULL && i < count; i++) {
        printf("[window] minimize requested (automated run)%s\n",
               SDL_MinimizeWindow(windows[i]) ? "" : ": FAILED");
    }
    SDL_free(windows);
}

/* The portable folder (snatcherLeo's rule: nothing in AppData): the port keeps its files in its own folder, so a copied folder
 * carries everything and each build has its own memory card (a lane test build no longer writes the card snatcherLeo plays
 * on). Where each file goes is pc_paths.c's table (4.1, 2026-11-21): in the portable layout (exe in app\) Aurora's
 * userPath is data\settings\ and its cachePath data\system\cache\; in the flat layout (test folders) user\ and cache\ next to the
 * exe, as before. s_base is the program folder (the exe's own, for Quit to hub's restart); s_root the OrrePort folder. */
static char s_base[1024], s_root[1024], s_user[1100], s_cache[1100];

/* <OrrePort folder>/<name> (the folder the player opens; the exe's folder in the flat layout). Kept for the callers
 * that name a file at the top; everything with a place of its own asks pc_paths_get. */
const char* pc_platform_data_path(const char* name, char* out, size_t size) {
    return pc_paths_get(PC_PLACE_ROOT, name, out, size);
}

/* The debug checkpoints' folder without a trailing separator (callers add "/slotN.bin"): data\system\checkpoints in the
 * portable layout, checkpoints next to the exe in the flat one. For pc_debug.c and the game side's
 * src/game/pc_debug_game.c, which has no access to pc_paths.h's enum through its forced game headers. */
const char* pc_platform_checkpoints_dir(char* out, size_t size) {
    return pc_paths_dir(PC_PLACE_CHECKPOINTS, out, size);
}

/* *Removed 2026-11-21 (4.1, the plan's part 3a):* copy_old_card, which copied a card from Aurora's old default folder
 * (%APPDATA%\OrrePort\USA\Card A\ on Windows, ~/.local/share/OrrePort/... elsewhere) into the settings folder on every
 * start with an empty card. Why: it read outside OrrePort's own folder, against the portable rule ("nothing in
 * AppData"), and the first portable check showed it really firing (this PC still has an old AppData card, from before
 * October's portable folder), quietly putting an unrelated save into a fresh folder. No tester ever had an AppData card
 * (0.1 is the first build anyone else gets), and snatcherLeo's own saves move with the copy-only migration tool
 * (tools/local/port/migrate_play_folder.py). Nothing in AppData is touched or deleted: the old card stays where it is.
 * The flat layout's user\USA\Card A\ → Slot 1 copy (pc_card.c) is unchanged. */

/* Can the port write its files? Creates and removes a small probe file in the settings folder (the first one written;
 * app\ itself is never written in the portable layout). */
static int folder_writable(void) {
    char probe[1200];
    FILE* f;

    snprintf(probe, sizeof(probe), "%s.write_test", s_user);
    f = fopen(probe, "wb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    remove(probe);
    return 1;
}

/* Returns 0 (after telling the player) when the OrrePort folder is not writable, e.g. under Program Files: saves,
 * settings and logs live there, so the port does not start rather than lose a save. */
static int portable_paths(void) {
    pc_paths_get(PC_PLACE_PROGRAM, "", s_base, sizeof(s_base));
    pc_paths_get(PC_PLACE_ROOT, "", s_root, sizeof(s_root));
    pc_paths_get(PC_PLACE_SETTINGS, "", s_user, sizeof(s_user));
    pc_paths_get(PC_PLACE_CACHE, "", s_cache, sizeof(s_cache));
    pc_paths_log();
    SDL_CreateDirectory(s_user);
    SDL_CreateDirectory(s_cache);
    if (pc_paths_is_portable()) {
        /* the portable layout's game folder exists from the first start (pc_paths_game_folder_exists makes it too):
         * the player sees where the disc goes, and live play is on the replay clock from the first frame */
        pc_paths_game_folder_exists();
    }
    if (!folder_writable()) {
        char message[1400];
        /* in the player's language: the settings aren't read yet (they live in the folder that can't be written), so
         * the language follows the OS ("auto"); pc_text_init only reads ui/lang */
        pc_text_init();
        pc_text_format(message, sizeof(message), "platform.unwritable.text", "folder", s_root, NULL);
        fprintf(stderr, "[orreport] the folder %s is not writable; move OrrePort somewhere writable\n", s_root);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pc_text("platform.unwritable.title"), message, NULL);
        return 0;
    }
    return 1;
}

/* The window (window-and-debug-plan.md, batch 1 items 2 and 3, snatcherLeo 2026-10-17): its size and place are remembered in
 * user/settings.ini, and F11 or Alt+Enter switches fullscreen (borderless, the desktop's own resolution, so switching
 * is instant; saved too). The place is saved only for a normal window: never while fullscreen or minimized (test runs
 * start minimized), checked about once a second after a move or resize, so a crash still keeps it. */
static const char* s_gpu_choice;  /* settings.ini gpu (pc_platform_start) */
static AuroraBackend s_gpu_backend; /* settings.ini gpu_backend */
static int s_window_dirty;
static unsigned s_window_frames;

static SDL_Window* main_window(void) {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* w = windows != NULL && count > 0 ? windows[0] : NULL;
    SDL_free(windows);
    return w;
}

/* The switch happens inside a quick dip to black (pc_settings_overlay.cpp): for a few frames around it Windows shows a
 * picture made for the old size, stretched or cropped into the new one (snatcherLeo, 2026-11-13: "there is a weird clicker
 * with the fullscreen shortcuts"; a 30 fps recording showed it). A cover drawn after the switch came too late (the
 * stale frames are Windows' own, before OrrePort draws at the new size), so the screen goes black first and the switch
 * is made then: the stale picture is black too. The overlay returns 1 when it will call
 * pc_platform_toggle_fullscreen_now itself; else the switch is made at once.
 * Platforms: the stale frames were seen on Windows (verified, snatcherLeo's PC). The dip itself is plain drawing, so it works
 * everywhere; whether Linux and macOS need it is untested (guess: X11 / Wayland compositors may show the same stale
 * frame; macOS by default plays its own full-screen animation into a separate Space, which SDL can turn off with
 * SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, so the Mac port should decide which one it wants before keeping the dip). */
int pc_settings_overlay_cover_switch(void) __attribute__((weak));
void pc_window_transitions(int on) __attribute__((weak)); /* pc_window_icon.cpp (the RmlUi build) */
static Uint64 s_transitions_back = 0; /* when Windows' window animations come back on after a switch (0: they're on) */
static void toggle_fullscreen_now(void);
static void toggle_fullscreen(void) {
    if (pc_settings_overlay_cover_switch != NULL && !automated_run() && pc_settings_overlay_cover_switch()) {
        return;
    }
    toggle_fullscreen_now();
}
void pc_platform_toggle_fullscreen_now(void) { toggle_fullscreen_now(); }
static void toggle_fullscreen_now(void) {
    SDL_Window* w = main_window();
    int on;
    if (w == NULL) {
        return;
    }
    on = (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) == 0;
    /* no maximize / restore animation during the switch: leaving fullscreen into a maximized window went to the
     * normal size first and grew back, the desktop showing around it (pc_window_icon.cpp pc_window_transitions says
     * why); back on a moment later, once the switch has settled (pc_platform_window_frame) */
    if (pc_window_transitions != NULL) {
        pc_window_transitions(0);
        s_transitions_back = SDL_GetTicks() + 800;
    }
    SDL_SetWindowFullscreenMode(w, NULL); /* borderless desktop fullscreen */
    SDL_SetWindowFullscreen(w, on);
    pc_settings_set_int("fullscreen", on);
    pc_settings_save();
}

/* For the window's menu bar (pc_menu_bar.cpp) and the RmlUi trial's Settings window (pc_settings_overlay.cpp) */
void pc_platform_toggle_fullscreen(void) { toggle_fullscreen(); }
int pc_platform_is_fullscreen(void) {
    SDL_Window* w = main_window();
    return w != NULL && (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0;
}
int pc_platform_automated_run(void) { return automated_run(); }

/* Quit to hub (snatcherLeo, 2026-11-13: "that should actually close out the game and take us back to the hub"): OrrePort
 * starts itself again (the hub is its first screen) and this program ends, as closing the window does. A fresh start
 * is the clean way back: the game's state, threads and memory are all left behind with the old process. Returns only
 * if the new one could not be started.
 * Platforms: SDL's process functions work on Windows, Linux and macOS, so the code is shared (Windows verified; the
 * others likely). One known difference (likely): in a macOS .app bundle the program sits in Contents/MacOS/ while
 * SDL_GetBasePath (s_base) points at Contents/Resources/, so the Mac port must build the path to the program
 * differently (or reopen the bundle) before this works there. */
void pc_log_drain(void);
void pc_log_exit_clean(const char* how); /* pc_log.c: the clean-exit line, then the drain (5.1, 2026-11-23) */
void pc_platform_window_frame(void);
/* the disc named on the command line (--disc), passed on by Quit to hub's restart; set by pc_platform_start */
static char s_disc_arg[1500];

int pc_platform_restart_to_hub(void) {
    char exe[1200];
#ifdef _WIN32
    snprintf(exe, sizeof(exe), "%sorrecolo.exe", s_base);
#else
    snprintf(exe, sizeof(exe), "%sorrecolo", s_base);
#endif
    /* *Corrected 2026-11-13:* a disc given with --disc goes to the new OrrePort too (snatcherLeo's Quit to hub from a
     * run_game.ps1 launch came back to "No disc yet": the build folder's own ROM\ is empty, the disc came on the
     * command line). A disc found in ROM\ needs nothing: the new one finds it the same way */
    const char* args[] = {exe, s_disc_arg[0] != '\0' ? "--disc" : NULL, s_disc_arg, NULL};
    /* the new window opens where this one is (snatcherLeo: "NO way to make that more... aesthetic?"): the place and size are
     * saved now, not after the usual second of waiting; a maximized window is passed on as ORRE_START_MAXIMIZED */
    SDL_Window* w = main_window();
    const int maximized = w != NULL && (SDL_GetWindowFlags(w) & SDL_WINDOW_MAXIMIZED) != 0;
    if (s_window_dirty) {
        s_window_frames = 60;
        pc_platform_window_frame();
    }
    SDL_Environment* env = SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(env, "ORRE_START_MAXIMIZED", maximized ? "1" : "0", true);
    /* a handover, not a second launch: the new OrrePort waits for this one to end instead of stepping aside for it
     * (pc_single_instance.c). *Fixed 2026-11-14:* without this, Quit to hub left nothing running (snatcherLeo: "the app is not
     * running anymore"): the one-OrrePort rule saw this one still open and the new one exited */
    SDL_SetEnvironmentVariable(env, "ORRE_HANDOVER", "1", true);
    /* one-shot test switches stay with this run (a tested Quit to hub would otherwise play and quit again forever) */
    SDL_UnsetEnvironmentVariable(env, "ORRE_HUB_AUTOPLAY");
    SDL_UnsetEnvironmentVariable(env, "ORRE_SETTINGS_TEST_QUIT");
    /* the ready file (snatcherLeo, 2026-11-15: "make it more... professional", "it should be for all OS's", the Steam Deck in
     * mind): the new OrrePort creates it once its hub is on screen (pc_platform_handover_ready); this one keeps its
     * closing screen up until then. A file in the user folder, not a window search, so it works the same on Windows,
     * Linux and macOS and no window system has to let one program see another's windows (Wayland doesn't) */
    char ready[1300], system_dir[1200];
    pc_paths_get(PC_PLACE_SYSTEM, "", system_dir, sizeof(system_dir)); /* data\system\ (flat layout: user\, as before 4.1) */
    SDL_CreateDirectory(system_dir);
    snprintf(ready, sizeof(ready), "%shandover-%llu.ready", system_dir, (unsigned long long)SDL_GetCurrentThreadID());
    SDL_RemovePath(ready);
    SDL_SetEnvironmentVariable(env, "ORRE_HANDOVER_READY", ready, true);
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    /* the new OrrePort gets no stdout / stderr of ours (snatcherLeo, 2026-11-13: after two Quit to hubs in a row the old window
     * froze on "Returning to the hub..." and a second process stayed open). Why: this process's stdout and stderr are
     * the log pipe (pc_log.c), so an inherited stdout made the new run copy its console text into OUR pipe, which
     * nobody reads once we are gone. After about 64 KB that write blocked the new run's log thread while it held the log
     * lock, so its own Quit to hub waited forever in pc_log_drain (both restarted runs' logs stopped mid-line at about
     * 64 KB: verified from the logs). The new run writes its own log file as always; only a console copy is lost.
     * Platforms: the same on all three (SDL opens the null device on Windows, /dev/null elsewhere). */
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_Process* child = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    if (child == NULL) {
        fprintf(stderr, "[platform] quit to hub: could not start %s (%s)\n", exe, SDL_GetError());
        return 0;
    }
    /* *Changed 2026-11-15 (snatcherLeo: "it stays on that black screen ... and use that black screen to then load the main hub
     * screen"):* this window keeps its "Returning to the hub..." screen until the new OrrePort's hub is on screen (its
     * ready file), so the black screen turns straight into the hub with no desktop between. Events keep being pumped,
     * so the window never shows as "not responding". At most 15 s; then (or if the new one ended) this one ends
     * anyway, as before. All OSes alike.
     * Still to do in the Linux / macOS ports (likely): the new window must come to the front. macOS doesn't always
     * bring forward an app another app started (cooperative activation): the new one asks to be raised when it shows
     * (SDL_RaiseWindow). Wayland's focus-stealing protection wants an activation token handed from this program to the
     * new one (XDG_ACTIVATION_TOKEN in its environment, which SDL reads); X11 and the Steam Deck's game mode need
     * nothing. */
    fprintf(stderr, "[platform] quit to hub: a new OrrePort started; waiting for its hub\n");
    const Uint64 t0 = SDL_GetTicks();
    int shown = 0;
    while (SDL_GetTicks() - t0 < 15000) {
        SDL_PumpEvents();
        if (SDL_GetPathInfo(ready, NULL)) {
            shown = 1;
            break;
        }
        int code = 0;
        if (SDL_WaitProcess(child, false, &code)) break; /* the new one ended (failed) */
        SDL_Delay(10);
    }
    SDL_RemovePath(ready);
    fprintf(stderr, "[platform] quit to hub: %s after %llu ms; this one ends\n",
            shown ? "the hub is on screen" : "no new window seen", (unsigned long long)(SDL_GetTicks() - t0));
    fflush(stderr);
    pc_log_exit_clean("Quit to hub (a new OrrePort took over)");
    _Exit(0);
}
/* The new OrrePort's side of Quit to hub (pc_hub.cpp calls it once the hub's window is shown): asked to come to the
 * front (macOS and Wayland may otherwise open it behind; on Windows it already is), then the ready file the old
 * OrrePort waits for is created, so the old one's closing screen gives way to this hub. Nothing outside a hand-over. */
void pc_platform_handover_ready(void) {
    const char* ready = SDL_getenv("ORRE_HANDOVER_READY");
    if (ready == NULL || ready[0] == '\0') return;
    SDL_Window* w = main_window();
    if (w != NULL) SDL_RaiseWindow(w);
    SDL_IOStream* f = SDL_IOFromFile(ready, "wb");
    if (f != NULL) SDL_CloseIO(f);
    fprintf(stderr, "[platform] quit to hub: the hub is on screen; the previous OrrePort may end\n");
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "ORRE_HANDOVER_READY"); /* once */
}
int pc_platform_stretched(void) { return pc_settings_int("stretch", 0) != 0; }
void pc_platform_set_stretched(int on) { /* the menu bar's Graphics > Stretch to window; saved, except in test runs */
    AuroraSetViewportPolicy(on ? AURORA_VIEWPORT_STRETCH : AURORA_VIEWPORT_FIT);
    pc_settings_set_int("stretch", on != 0);
    if (!automated_run()) {
        pc_settings_save();
    }
}

/* pc_vi.c's event loop, for each Aurora event */
void pc_platform_window_event(const AuroraEvent* event) {
    /* an automated run says in its log when it was minimized and whether someone opened or clicked its window, so a
     * run can be checked afterwards (the request, 2026-10-17) */
    if (event->type == AURORA_SDL_EVENT && automated_run()) {
        static int minimized_logged, restored_logged;
        if (event->sdl.type == SDL_EVENT_WINDOW_MINIMIZED && !minimized_logged) {
            minimized_logged = 1;
            printf("[window] minimized\n");
        } else if ((event->sdl.type == SDL_EVENT_WINDOW_RESTORED || event->sdl.type == SDL_EVENT_WINDOW_FOCUS_GAINED) &&
                   minimized_logged && !restored_logged) {
            restored_logged = 1;
            printf("[window] WARNING: the automated run's window was %s by someone (game frame %u); input stays "
                   "scripted\n", event->sdl.type == SDL_EVENT_WINDOW_RESTORED ? "restored" : "focused",
                   (unsigned)pc_vi_heartbeat);
        }
    }
    /* The window's own events in a player's log (snatcherLeo, 2026-11-22, the logging review: "our logger should be able to
     * tell us EVERYTHING we need to know about that user's app before it crashed"): minimized, restored, maximized,
     * focus lost and back, fullscreen on and off, moved to another monitor. Why: reports like "it froze when I
     * alt-tabbed" or "the picture went wrong on my second screen" need to show what the window did and when. The size
     * is already logged by [graphics] when the drawing size changes (pc_platform_window_frame). Automated runs keep
     * their own two lines above instead (their windows are minimized on purpose). SDL's events, the same on every
     * system; "monitor" names come from SDL_GetDisplayName (Windows: the monitor's name; likely the same elsewhere). */
    if (event->type == AURORA_SDL_EVENT && !automated_run()) {
        const char* what = NULL;
        switch (event->sdl.type) {
        case SDL_EVENT_WINDOW_MINIMIZED: what = "minimized"; break;
        case SDL_EVENT_WINDOW_MAXIMIZED: what = "maximized"; break;
        case SDL_EVENT_WINDOW_RESTORED: what = "restored"; break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: what = "lost focus (another window in front)"; break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: what = "has focus again"; break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN: what = "fullscreen on"; break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: what = "fullscreen off"; break;
        default: break;
        }
        char when[48];
        if (what != NULL) {
            pc_settings_when(when, sizeof(when));
            printf("[window] %s (%s)\n", what, when);
        } else if (event->sdl.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED) {
            const SDL_DisplayID d = (SDL_DisplayID)event->sdl.window.data1;
            const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(d);
            const char* name = SDL_GetDisplayName(d);
            pc_settings_when(when, sizeof(when));
            if (m != NULL) {
                printf("[window] moved to monitor %u (%s): %d x %d at %.2f Hz, scaling %.0f%% (%s)\n", (unsigned)d,
                       name != NULL ? name : "unnamed", m->w, m->h, m->refresh_rate,
                       SDL_GetDisplayContentScale(d) * 100.0f, when);
            } else {
                printf("[window] moved to monitor %u (%s) (%s)\n", (unsigned)d, name != NULL ? name : "unnamed", when);
            }
        }
    }
    /* Sound outputs coming and going (the same logging review): SDL reports each playback device as it appears (at
     * start-up too, so the log lists the PC's outputs) or disappears (headphones unplugged, a Bluetooth headset off),
     * and a format change when the system's default output switches; the game's sound follows the default output
     * (SDL's default device). Why: a "no sound" or "the sound stopped" report then shows which outputs there were
     * and when one went away. Recording devices (microphones) are left out. SDL's events, every system (likely the
     * same names elsewhere; verified on Windows only). */
    if (event->type == AURORA_SDL_EVENT &&
        (event->sdl.type == SDL_EVENT_AUDIO_DEVICE_ADDED || event->sdl.type == SDL_EVENT_AUDIO_DEVICE_REMOVED ||
         event->sdl.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) &&
        !event->sdl.adevice.recording) {
        const SDL_AudioDeviceID id = event->sdl.adevice.which;
        const char* name = SDL_GetAudioDeviceName(id);
        char when[48];
        pc_settings_when(when, sizeof(when));
        printf("[sound_output] %s: %u (%s) (%s)\n",
               event->sdl.type == SDL_EVENT_AUDIO_DEVICE_ADDED     ? "output available"
               : event->sdl.type == SDL_EVENT_AUDIO_DEVICE_REMOVED ? "output gone"
                                                                    : "output format changed (the default output may have switched)",
               (unsigned)id, name != NULL ? name : "name unknown", when);
    }
    /* rumble never keeps going behind another window (the comparison with Dolphin, 2026-11-12: Aurora starts rumble
     * with no time limit, so a controller could buzz on after the window lost focus); the game starts it again when
     * it next asks */
    if (event->type == AURORA_SDL_EVENT && event->sdl.type == SDL_EVENT_WINDOW_FOCUS_LOST && !automated_run()) {
        for (u32 chan = 0; chan < 4; ++chan) {
            PADControlMotor(chan, PAD_MOTOR_STOP_HARD);
        }
    }
    if (event->type == AURORA_WINDOW_MOVED || event->type == AURORA_WINDOW_RESIZED) {
        s_window_dirty = 1;
    } else if (event->type == AURORA_SDL_EVENT && event->sdl.type == SDL_EVENT_KEY_DOWN && !event->sdl.key.repeat) {
        const SDL_Keycode key = event->sdl.key.key;
        if (key == SDLK_F11 || (key == SDLK_RETURN && (event->sdl.key.mod & SDL_KMOD_ALT) != 0)) {
            toggle_fullscreen();
        }
    }
}

/* pc_vi.c, once per frame */
void pc_platform_window_frame(void) {
    SDL_Window* w;
    SDL_WindowFlags flags;
    int x, y, width, height;
    /* the size the game is drawn at, logged when it changes (2026-11-13, for the Graphics page's check: "those
     * actually work right? Not just for show?"). Why: Internal resolution only asks Aurora for a scale; this shows what
     * it really draws (Settings › Graphics › Internal resolution, render_scale; 0 follows the window) in every log, so a
     * report says it too. Platform-neutral. */
    if (s_transitions_back != 0 && SDL_GetTicks() >= s_transitions_back) {
        s_transitions_back = 0;
        pc_window_transitions(1); /* Windows' animations again for an ordinary maximize or minimize */
    }
    {
        static uint32_t shown_w, shown_h;
        const AuroraWindowSize size = aurora_get_window_size();
        if (size.fb_width != shown_w || size.fb_height != shown_h) {
            shown_w = size.fb_width;
            shown_h = size.fb_height;
            printf("[graphics] drawing at %ux%u (window %ux%u px, render_scale %d)\n", (unsigned)size.fb_width,
                   (unsigned)size.fb_height, (unsigned)size.native_fb_width, (unsigned)size.native_fb_height,
                   pc_settings_int("render_scale", 0));
        }
    }

    /* maximized or not, kept for the next start (snatcherLeo, 2026-11-13). Read from the window's own state each frame (cheap)
     * rather than from maximize / restore events, which come in different orders after a minimize on different OSes.
     * Fullscreen and minimized leave it as it was: leaving fullscreen goes back to the window as before, and a
     * minimized window opens as it was before being minimized. Saved only when it changes. */
    if (!automated_run()) {
        SDL_Window* mw = main_window();
        if (mw != NULL) {
            const SDL_WindowFlags f = SDL_GetWindowFlags(mw);
            if ((f & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED)) == 0) {
                const int maxed = (f & SDL_WINDOW_MAXIMIZED) != 0;
                if (maxed != pc_settings_int("window_maximized", 0)) {
                    pc_settings_set_int("window_maximized", maxed);
                    pc_settings_save();
                }
            }
        }
    }
    if (!s_window_dirty || ++s_window_frames < 60 || automated_run()) { /* a test run never moves snatcherLeo's window */
        return;
    }
    s_window_frames = 0;
    s_window_dirty = 0;
    w = main_window();
    if (w == NULL) {
        return;
    }
    flags = SDL_GetWindowFlags(w);
    if ((flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED)) != 0) {
        return;
    }
    if (!SDL_GetWindowPosition(w, &x, &y) || !SDL_GetWindowSize(w, &width, &height)) {
        return;
    }
    if (x == pc_settings_int("window_x", -1) && y == pc_settings_int("window_y", -1) &&
        width == pc_settings_int("window_w", 0) && height == pc_settings_int("window_h", 0)) {
        return;
    }
    pc_settings_set_int("window_x", x);
    pc_settings_set_int("window_y", y);
    pc_settings_set_int("window_w", width);
    pc_settings_set_int("window_h", height);
    pc_settings_save();
}

/* Where the window opens (snatcherLeo, 2026-11-13: "it should remember it was maximized ... Should we make it so that, it
 * remembers which monitor it was on last"). The saved place already names the monitor (each monitor has its own
 * stretch of the desktop's coordinates, negative for one left of or above the main one), so reopening at it puts the
 * window back on the same screen. This checks the place still makes sense, as modern apps do:
 * - the middle of the title bar must be on a monitor that still exists (one may have been unplugged or a laptop
 *   undocked); otherwise -1, -1: centred on the main monitor;
 * - the window is shrunk to fit that monitor's usable area (the taskbar left out) if the monitor got smaller, and
 *   moved fully onto it.
 * The size is checked against the monitor it lands on. Returns the monitor (0 if none). SDL's display functions work
 * the same on Windows, Linux and macOS (likely for the last two; Wayland doesn't let apps place their windows at all,
 * so there it just opens where the compositor puts it). */
static SDL_DisplayID fit_saved_place(int* x, int* y, int* w, int* h) {
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    SDL_DisplayID found = 0;
    const int cx = *x + (*w > 0 ? *w : 1280) / 2, cy = *y + 16; /* the middle of the title bar */
    for (int i = 0; displays != NULL && i < count && found == 0; i++) {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(displays[i], &r) && cx >= r.x && cy >= r.y && cx < r.x + r.w && cy < r.y + r.h) {
            found = displays[i];
        }
    }
    SDL_free(displays);
    if (found == 0) {
        *x = *y = -1;
        return 0;
    }
    SDL_Rect u;
    if (SDL_GetDisplayUsableBounds(found, &u) && *w > 0 && *h > 0) {
        if (*w > u.w) *w = u.w;
        if (*h > u.h) *h = u.h;
        /* a few px over the edge is normal (Windows' invisible resize border on a snapped window); more is moved in */
        if (*x + *w > u.x + u.w + 16) *x = u.x + u.w - *w;
        if (*y + *h > u.y + u.h + 16) *y = u.y + u.h - *h;
        if (*x < u.x - 16) *x = u.x;
        if (*y < u.y) *y = u.y; /* never above the top: the title bar would be out of reach */
        if (*x == -1 && *y == -1) *x = 0; /* -1, -1 means "centred" to Aurora; keep a real place real */
    }
    return found;
}

/* The disc from the portable folder (the reference playthrough Part 1 item 2a; snatcherLeo's layout, design inbox 2026-10-31, 2026-11-01,
 * for B to review): with no --disc, the game looks in <exe>/Colosseum/ROM/ and opens every file there until one is the
 * USA disc (game ID GC6E01, read from the image itself, so the file name and format don't matter: any image Aurora's
 * DVD layer reads, ISO / GCM / CISO / RVZ and likely GCZ / WIA). The player is told what's wrong otherwise. */
static int s_search_disc;
void pc_platform_search_disc(void) { s_search_disc = 1; }

#ifdef PC_HAVE_AURORA_DVD
static char s_disc_image[1500]; /* the image opened, for the whole-disc check's cache key */
static int s_scan_saw_other;    /* the last ROM\ scan found another game's or version's image */

/* a wrong disc in the player's words (2026-11-02): which game and version a game ID is, or NULL if unknown */
static const char* disc_name(const DVDDiskID* id) {
    /* the names in the player's language (ui/lang, "disc.name.<id>"; pc_text.h) */
    static const struct { const char* id; const char* key; } known[] = {
        {"GC6P", "disc.name.GC6P"},  {"GC6J", "disc.name.GC6J"},  {"GXXE", "disc.name.GXXE"}, {"GXXP", "disc.name.GXXP"},
        {"GXXJ", "disc.name.GXXJ"},  {"PCSJ", "disc.name.bonus"}, {"PCSE", "disc.name.bonus"},
        /* *Fixed 2026-11-23 (Studio's finding):* the US Bonus Disc's game ID is PC6E01 (read from snatcherLeo's disc with
         * Studio's command-line tool; likely the European one is PC6P), so it was named "another game"; Studio treats
         * PC6E, PC6J, PC6P, PCSE and PCSJ as the bonus disc, and so does the hub now */
        {"PC6E", "disc.name.bonus"}, {"PC6J", "disc.name.bonus"}, {"PC6P", "disc.name.bonus"},
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (memcmp(id->gameName, known[i].id, 4) == 0) {
            return pc_text(known[i].key);
        }
    }
    return NULL;
}

/* The European or Japanese Colosseum disc: the key of the languages it will add as a language disc (2.2 step 8: they
 * go in the same ROM folder, told apart by game ID), or NULL for any other disc. All OSes. */
static const char* region_disc(const DVDDiskID* id) {
    if (memcmp(id->gameName, "GC6P", 4) == 0) return "disc.languages.GC6P";
    if (memcmp(id->gameName, "GC6J", 4) == 0) return "disc.languages.GC6J";
    return NULL;
}

/* The game folder's ROM/, where the disc image lives (Pokemon Colosseum\ROM\; flat layout: Colosseum\ROM\) */
const char* pc_platform_rom_folder(char* out, size_t size) {
    return pc_paths_get(PC_PLACE_ROM, "", out, size);
}

/* Is the image at path the USA disc? 1: yes; 0: no, with the reason in why (the hub checks a dropped or chosen file
 * before copying it into ROM\, plan "Adding a disc": "Checked first"). The image is closed again either way. */
int pc_platform_disc_identify(const char* path, char* why, size_t size) {
    const DVDDiskID* id;
    int ok;
    if (path == NULL || !SDL_GetPathInfo(path, NULL) || !aurora_dvd_open(path)) {
        /* an archive gets its own line (the project plan's import flow, 2026-11-13: "zip / 7z: unpack it first") */
        const size_t n = path != NULL ? strlen(path) : 0;
        const char* ext = n > 4 ? path + n - 4 : "";
        const int archive = n > 4 && (SDL_strcasecmp(ext, ".zip") == 0 || SDL_strcasecmp(ext, ".rar") == 0 ||
                                      SDL_strcasecmp(n > 3 ? path + n - 3 : "", ".7z") == 0);
        pc_text_format(why, size, archive ? "disc.archive" : "disc.unreadable", NULL);
        return 0;
    }
    id = DVDGetCurrentDiskID();
    ok = memcmp(id->gameName, "GC6E", 4) == 0 && memcmp(id->company, "01", 2) == 0;
    if (!ok) {
        const char* name = disc_name(id);
        char game_id[8];
        snprintf(game_id, sizeof(game_id), "%.4s%.2s", id->gameName, id->company);
        if (region_disc(id) != NULL) {
            /* the European or Japanese Colosseum: not the disc OrrePort plays, but a language disc later (snatcherLeo,
             * 2026-11-13: "if a user tries to select PAL or Japanese ROM, for the main ROM, we give them a special message
             * telling them they need a NTSC ROM to play, and the other 2 can work for translations") */
            pc_text_format(why, size, "disc.wrong.region", "game", name, "id", game_id, "languages",
                           pc_text(region_disc(id)), NULL);
        } else {
            pc_text_format(why, size, "disc.wrong", "game", name != NULL ? name : pc_text("disc.name.unknown"), "id",
                           game_id, NULL);
        }
    }
    aurora_dvd_close();
    return ok;
}

/* Look through ROM\ for the USA disc and open it. 1: open; 0: not there, with the player's message in message (or
 * nothing when the folder is empty). Files ending in ".part" (the hub's copy in progress) are skipped. */
int pc_platform_disc_scan(char* message, size_t message_size) {
    char folder[1200], path[1500], wrong[1200] = "";
    char** files;
    int count = 0, found = 0, unreadable = 0, region = 0; /* region: a European / Japanese disc seen (a language disc) */
    /* *Changed 2026-11-23 (4.4, the shared "which disc" rule; docs/port/disc-sync-review.md):* every US disc in ROM\ is
     * looked at, and with two or more the newest (last change) wins, then the first by name, as Studio's
     * orre_disc_find in the shared reader core decides, so both apps pick the same file. Before, the hub took the
     * first in the folder's listing order. A copy or move into ROM\ (by the hub or Studio) gets the current time, so the
     * disc just added is the one used. Same on every system (SDL's listing and file times). */
    char best[1500] = "", best_name[512] = "";
    Sint64 best_time = 0;
    int matches = 0, best_original = 0;

    pc_platform_rom_folder(folder, sizeof(folder));
    SDL_CreateDirectory(folder); /* so the player sees where the disc goes */
    files = SDL_GlobDirectory(folder, "*", 0, &count);
    for (int i = 0; files != NULL && i < count; i++) {
        const DVDDiskID* id;
        SDL_PathInfo info;
        const size_t name_len = strlen(files[i]);
        snprintf(path, sizeof(path), "%s%s", folder, files[i]);
        if (name_len > 5 && strcmp(files[i] + name_len - 5, ".part") == 0) {
            continue;
        }
        if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE || !aurora_dvd_open(path)) {
            unreadable++;
            continue;
        }
        id = DVDGetCurrentDiskID();
        if (memcmp(id->gameName, "GC6E", 4) == 0 && memcmp(id->company, "01", 2) == 0) {
            /* an original disc (its main.dol matches build.sha1) beats a modified one whatever their times, as the
             * shared core's orre_disc_find skips one that isn't (snatcherLeo, 2026-11-23: "refuse all three"); with no
             * original at all, the newest modified one is still opened so the start-up check (check.dol) tells the
             * player why it can't be used, instead of a blank disc with no reason */
            extern int pc_disc_check_main_dol(int show_box);
            const int original = pc_disc_check_main_dol(0);
            aurora_dvd_close(); /* opened again below, once the rule has chosen */
            matches++;
            if (best[0] == 0 || original > best_original ||
                (original == best_original &&
                 ((Sint64)info.modify_time > best_time ||
                  ((Sint64)info.modify_time == best_time && strcmp(files[i], best_name) < 0)))) {
                best_original = original;
                snprintf(best, sizeof(best), "%s", path);
                snprintf(best_name, sizeof(best_name), "%s", files[i]);
                best_time = (Sint64)info.modify_time;
            }
        } else {
            size_t used = strlen(wrong);
            const char* name = disc_name(id);
            char game_id[8], line[400];
            snprintf(game_id, sizeof(game_id), "%.4s%.2s", id->gameName, id->company);
            region |= region_disc(id) != NULL;
            pc_text_format(line, sizeof(line), "disc.missing.other_line", "file", files[i], "game",
                           name != NULL ? name : pc_text("disc.name.unknown"), "id", game_id, NULL);
            snprintf(wrong + used, sizeof(wrong) - used, "\n  %s", line);
            aurora_dvd_close();
        }
    }
    SDL_free(files);
    if (best[0] != 0 && aurora_dvd_open(best)) {
        if (matches > 1) {
            printf("[orreport] %d US discs in the ROM folder; the newest %s is used (the shared rule)\n", matches,
                   best_original ? "original" : "(none is the original)");
        }
        printf("[orreport] disc: %s (GC6E01)\n", best);
        snprintf(s_disc_image, sizeof(s_disc_image), "%s", best);
        found = 1;
    }
    s_scan_saw_other = wrong[0] != 0;
    if (!found) {
        /* the player's message in their language (ui/lang "disc.missing.*", pc_text.h), in pieces that each stand as a
         * whole sentence, so a translation never has to fit English word order */
        char part[1600];
        size_t used;
        pc_text_format(message, message_size, "disc.missing.text", "folder", folder, NULL);
        used = strlen(message);
        snprintf(message + used, message_size - used, "\n\n%s", pc_text("disc.missing.formats"));
        if (count == 0) {
            used = strlen(message);
            snprintf(message + used, message_size - used, "\n\n%s", pc_text("disc.missing.empty"));
        }
        if (wrong[0]) {
            used = strlen(message);
            snprintf(message + used, message_size - used, "\n\n%s%s\n\n%s", pc_text("disc.missing.others"), wrong,
                     pc_text("disc.missing.usa_only"));
            if (region) { /* a European / Japanese disc may stay: it becomes a language disc next to the US one */
                used = strlen(message);
                snprintf(message + used, message_size - used, "\n\n%s", pc_text("disc.missing.region_stays"));
            }
        }
        if (unreadable > 0 && wrong[0] == 0 && count > 0) {
            char n[16];
            snprintf(n, sizeof(n), "%d", unreadable);
            pc_text_format(part, sizeof(part), "disc.missing.unreadable", "n", n, NULL);
            used = strlen(message);
            snprintf(message + used, message_size - used, "\n\n%s", part);
        }
        fprintf(stderr, "[orreport] no GC6E01 disc image in %s (%d file(s); other discs: %s)\n", folder, count,
                wrong[0] ? wrong : "none");
        /* the player's message too, so the log (and a report zip) says exactly what the window said */
        fprintf(stderr, "[orreport] the window says:\n%s\n", message);
    }
    return found;
}

/* 1 when the last scan found disc images that are another game or version (the hub says so; it used to look for
 * English words in the message, which a translation would break) */
int pc_platform_disc_scan_saw_other(void) { return s_scan_saw_other; }

/* the disc image the game has open ("" before one is), for the hub's folder watch (2026-11-23) */
const char* pc_platform_disc_image(void) { return s_disc_image; }

#ifdef ORRE_SETTINGS_OVERLAY
int pc_hub_wait_for_disc(void); /* pc_hub.cpp: the hub with a blank disc until one is added; 0: window closed */
#endif

static int open_disc_from_folder(void) {
    char message[3200];
    if (pc_platform_disc_scan(message, sizeof(message))) {
        return 1;
    }
#ifdef ORRE_SETTINGS_OVERLAY
    /* the hub (snatcherLeo, 2026-11-09: "It'll be a blank disc, until the ISO is recognized"): a blank disc and the ways to
     * add one, instead of the message box */
    if (!automated_run()) {
        return pc_hub_wait_for_disc();
    }
#endif
    if (!automated_run()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pc_text("disc.missing.title"), message, NULL);
    }
    return 0;
}
#endif

/* Returns 1 when everything is up; on failure prints why and returns 0. disc may be NULL (no disc yet; or, after
 * pc_platform_search_disc, the disc is looked for in the portable folder). */
extern volatile u32 pc_vi_heartbeat;

int pc_platform_start(int argc, char** argv, const char* disc) {
    if (disc != NULL) {
        snprintf(s_disc_arg, sizeof(s_disc_arg), "%s", disc);
    }
    pc_crash_install();
    pc_crash_watch_install(); /* ORRE_WATCH: hardware write-watch on this (the game's) thread */
    if (getenv("ORRE_WATCHDOG") != NULL) { /* first-run diagnostics: where the game hangs */
        extern volatile u32 pc_vi_alive; /* pc_vi.c: ticks per frame and while the debug Pause holds the game */
        pc_crash_watchdog((volatile const unsigned*)&pc_vi_alive, (unsigned)atoi(getenv("ORRE_WATCHDOG")));
    }
    if (!pc_mem_reserve(PC_MEM1_RETAIL_SIZE) || !pc_mem_map_locked_cache() || !pc_lowmem_init()) {
        fprintf(stderr, "[orreport] cannot map GameCube memory below 4 GB; see the messages above\n");
        return 0;
    }
    if (!portable_paths()) {
        return 0;
    }
    pc_settings_load(s_user);
    pc_text_init(); /* the player's language (settings.ini "language"), before any text is shown (pc_text.h) */
    {
        /* the GPU (snatcherLeo, 2026-10-17: Dawn had picked the integrated Intel GPU on a PC with an RTX 4090):
         * settings.ini "gpu" = high-performance (default: the discrete GPU) or low-power (the integrated one, the
         * low-end test path), or software (2026-11-11: the backend's software renderer, WARP on d3d12, for frame
         * checks on a second implementation); "gpu_backend" = auto (default), d3d12, vulkan or d3d11. ORRE_GPU
         * overrides "gpu" for one run. The log names the adapter picked ("Graphics adapter information"). */
        const char* gpu = getenv("ORRE_GPU") != NULL ? getenv("ORRE_GPU") : pc_settings_string("gpu", "high-performance");
        const char* api = pc_settings_string("gpu_backend", "auto");
        static char gpu_choice[32];
        AuroraBackend backend = BACKEND_AUTO;
        snprintf(gpu_choice, sizeof(gpu_choice), "%s", gpu);
        if (strcmp(api, "d3d12") == 0) {
            backend = BACKEND_D3D12;
        } else if (strcmp(api, "vulkan") == 0) {
            backend = BACKEND_VULKAN;
        } else if (strcmp(api, "d3d11") == 0) {
            backend = BACKEND_D3D11;
        }
#ifdef _WIN32
        /* "auto" on Windows tries Vulkan first. Since 2026-11-11 this is the intended default (snatcherLeo: "Yes, Vulkan
         * default"), not only a workaround: two problems show only on Dawn's D3D12 path, the 4K black wedges
         * (golden-slice A14, 2026-10-18) and Intel graphics' text blocks and box-screen square (port-gaps 76: clean on
         * Vulkan on the same laptop, and faster there). the reference playthrough was played on Vulkan. If Vulkan fails to start (an old
         * Intel driver Dawn blocks), Aurora falls back to its usual order (D3D12 first). gpu_backend = d3d12 keeps the
         * D3D12 path testable and selectable. */
        if (backend == BACKEND_AUTO) {
            backend = BACKEND_VULKAN;
            api = "auto (Vulkan first, A14 workaround)";
        }
#endif
        printf("[gpu] preference %s, backend %s (settings.ini gpu / gpu_backend)\n", gpu_choice, api);
        s_gpu_choice = gpu_choice;
        s_gpu_backend = backend;
    }
    {
        /* the saved place, or -1, -1 (none: centred). *Updated 2026-11-13:* a negative place counts now (a monitor left
         * of or above the main one, a snapped window); before, only places at 0 or more were used. A key that isn't in
         * settings.ini reads as INT_MIN, so a real -1 is told apart from "never saved" */
        int win_x = pc_settings_int("window_x", INT_MIN), win_y = pc_settings_int("window_y", INT_MIN);
        int win_w = pc_settings_int("window_w", 0), win_h = pc_settings_int("window_h", 0);
        if (win_x == INT_MIN || win_y == INT_MIN) {
            win_x = win_y = -1;
        } else if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { /* SDL needs its video part for the display list; Aurora's own
                                                          * SDL_Init comes later and is harmless twice */
            win_x = win_y = -1;
        } else if (fit_saved_place(&win_x, &win_y, &win_w, &win_h) == 0) {
            printf("[window] the saved place is on no monitor now (one unplugged?): centred on the main one\n");
        }
        const AuroraConfig config = {
            .windowPosX = win_x, /* -1, -1: centred (Aurora's default) */
            .windowPosY = win_y,
            .windowWidth = (uint32_t)win_w, /* 0: Aurora's default size */
            .windowHeight = (uint32_t)win_h,
            .startFullscreen = pc_settings_int("fullscreen", 0) != 0 && !automated_run(),
            /* ORRE_TEXTURE_REPORT=1: Aurora logs which texture-pack entries apply and dumps the textures that
             * have none (pc_textures.cpp; snatcherLeo's texture-pack experiment) */
            .allowTextureDumps = getenv("ORRE_TEXTURE_REPORT") != NULL,
            .desiredBackend = s_gpu_backend,
            /* Settings > Graphics (snatcherLeo, 2026-11-11: "actually make those things real"). Automated runs keep the
             * defaults, the values every recorded check was made with: V-Sync off (Aurora then presents with Mailbox,
             * or Immediate where Mailbox is missing; the game paces itself), no MSAA, anisotropy up to 16. MSAA and
             * anisotropy are read only here (a change applies at the next start); V-Sync also changes live
             * (aurora_enable_vsync, pc_settings_overlay.cpp). WebGPU offers 1 or 4 samples only. */
            .vsync = !automated_run() && pc_settings_int("vsync", 0) != 0,
            .msaa = automated_run() || pc_settings_int("msaa", 1) != 4 ? 1u : 4u,
            .maxTextureAnisotropy = (uint16_t)(automated_run() ? 16 : aniso_setting()),
            .gpuPreference = s_gpu_choice,
            /* 2026-11-11: a software renderer (WARP) reports itself as a CPU adapter, which Aurora otherwise refuses
             * (and then quietly tries the next backend on the real GPU); allow it only when it was asked for */
            .allowCpuAdapter = s_gpu_choice != NULL && strcmp(s_gpu_choice, "software") == 0,
            .appName = "OrrePort",
            .userPath = s_user,   /* the portable folder, not AppData (portable_paths) */
            .cachePath = s_cache,
            .logCallback = &log_callback,
            .mem1Size = PC_MEM1_RETAIL_SIZE,
            .mem1Base = pc_mem_from_gc(PC_MEM1_BASE), /* aurora: change; the game's heaps go into our MEM1 */
            /* aurora: change; the console's arena (pc_mem.h), so main.dol's data fits below it at its own addresses */
            .arenaLoOffset = PC_ARENA_LO - PC_MEM1_BASE,
            .arenaHiOffset = PC_ARENA_HI - PC_MEM1_BASE,
            /* ARAM, the GameCube's 16 MB auxiliary memory (sound samples, fsys's ARQ transfers). Without it Aurora's
             * ARInit warns "ARAM will not be available" and the game's ARAM allocations fail (the first boot to
             * reach the sound setup, 2026-10-16). */
            .mem2Size = 16 * 1024 * 1024,
            /* the look before the first show (with the overlay) and an automated run's minimize (2026-11-15): every
             * build, not only the overlay's */
            .windowCreatedCallback = window_created,
        };
        /* the controllers' fuller reports (PlayStation and Switch pads over Bluetooth: rumble, lights, motion), as
         * Dolphin asks SDL for (the comparison of 2026-11-12, the decision log); set before Aurora starts SDL's
         * gamepads. Port side, so Aurora's own start-up is unchanged. */
        SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
#ifdef _WIN32
        /* The input method's composition and choice list drawn by the app (the naming keyboard), set before SDL's IME
         * support first starts: on Windows SDL reads the hint only then (pc_ime_win32.cpp says why and how the hub's
         * text fields keep Windows' own IME windows). Windows only: Linux's IBus / fcitx read the hint live, so the
         * name keyboard sets it around its own typing there; macOS needs nothing (pc_name_keyboard.cpp) */
        {
            extern void pc_ime_win32_hint(void);
            pc_ime_win32_hint();
        }
#endif
#ifdef ORRE_SETTINGS_OVERLAY
        /* the hub's and overlay's text shaped by HarfBuzz in SheenBidi's order (2026-11-16; hub_text/pc_ui_shaping.h):
         * RmlUi reads its font engine once, in Rml::Initialise inside aurora_initialize, so it is set just before.
         * All platforms. */
        {
            extern void pc_ui_font_engine_install(void);
            pc_ui_font_engine_install();
        }
#endif
        pc_startup_mark("graphics start (Aurora)");
        aurora_initialize(argc, argv, &config);
        pc_startup_mark("Aurora ready (pipeline cache, RmlUi)");
        if (automated_run() && main_window() != NULL) {
            /* the check for window_created's minimize (2026-11-15): an automated run's window should already be
             * minimized here, shown since Aurora's show_window */
            const SDL_WindowFlags f = SDL_GetWindowFlags(main_window());
            printf("[window] flags right after creation: %s, %s\n", (f & SDL_WINDOW_MINIMIZED) != 0 ? "minimized" : "NOT minimized",
                   (f & SDL_WINDOW_HIDDEN) != 0 ? "hidden" : "shown");
        }
#ifdef _WIN32
        {
            /* the game window made once by aurora_initialize (window.cpp; never remade later): its IME messages
             * wrapped (pc_ime_win32.cpp), Windows only */
            extern void pc_ime_win32_install(SDL_Window * window);
            pc_ime_win32_install(main_window());
        }
#endif
        {
            SDL_Window* w = main_window();
            int x = 0, y = 0, width = 0, height = 0;
            if (w != NULL && SDL_GetWindowPosition(w, &x, &y) && SDL_GetWindowSize(w, &width, &height)) {
                printf("[window] at %d,%d, %dx%d%s (%s)\n", x, y, width, height,
                       (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0 ? ", fullscreen" : "",
                       config.windowPosX >= 0 ? "settings.ini" : "default place");
            }
        }
        apply_window_mode();
        {
            /* Opens maximized when it was maximized at the last close (snatcherLeo, 2026-11-13: "it should remember it was
             * maximized before I closed it out"; settings.ini window_maximized, kept by pc_platform_window_frame), or
             * when Quit to hub restarted it from a maximized window (ORRE_START_MAXIMIZED). The window is first made at
             * its saved normal place and size, so it maximizes on that monitor and the restore button brings that
             * size back. Without Windows' grow animation (pc_window_transitions, as for fullscreen), back on a moment
             * later. Not under fullscreen (that has its own key), never in automated runs. */
            const char* max = getenv("ORRE_START_MAXIMIZED");
            SDL_Window* w = main_window();
            const int want = max != NULL ? strcmp(max, "1") == 0 : pc_settings_int("window_maximized", 0) != 0;
            if (want && w != NULL && !automated_run() && (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) == 0) {
                if (pc_window_transitions != NULL) {
                    pc_window_transitions(0);
                    s_transitions_back = SDL_GetTicks() + 800;
                }
                SDL_MaximizeWindow(w);
                printf("[window] maximized, as it was at the last close\n");
            }
        }
        {
            /* the render scale, and texture packs for developer runs only (ORRE_TEXTURE_PACK=1; no player setting since
             * 2026-11-22: pc_textures.cpp); the pack folder from the paths table (Pokemon Colosseum\Textures\; flat
             * layout: user\textures\GC6E01\, as before 4.1) */
            char textures[1200];
            pc_paths_get(PC_PLACE_TEXTURES, "", textures, sizeof(textures));
            pc_textures_init(textures, automated_run());
        }
        /* The picture keeps the console's 4:3 shape with black bars (snatcherLeo, 2026-10-17, until the widescreen work).
         * Aurora's GX state starts as FIT, but its frame-buffer sizing follows the policy only once this is called, so
         * without it the picture took the window's shape and looked stretched (A, verified from Aurora's code; snatcherLeo
         * saw the bars in a 4K window). settings.ini "stretch = 1" fills the window instead. */
        {
            const int stretch = pc_settings_int("stretch", 0) != 0;
            AuroraSetViewportPolicy(stretch ? AURORA_VIEWPORT_STRETCH : AURORA_VIEWPORT_FIT);
            printf("[window] picture %s (settings.ini stretch = %d)\n", stretch ? "stretched to the window" : "4:3 with bars",
                   stretch);
        }
    }
#ifdef PC_HAVE_AURORA_DVD
    pc_dvd_install();
    /* Aurora is up by now, so a failure here shuts it down first: otherwise its worker threads are still running at the
     * program's end and the C++ runtime aborts ("terminate called without an active exception", exit code 3) */
    if (disc != NULL && !aurora_dvd_open(disc)) {
        fprintf(stderr, "[orreport] cannot open the disc image %s\n", disc);
        aurora_shutdown();
        return 0;
    }
    if (disc != NULL) {
        /* a disc given with --disc is the open one too (the hub's Discs panel names it, 2026-11-23); the whole-disc check
         * below already takes this path for it, so nothing else changes */
        snprintf(s_disc_image, sizeof(s_disc_image), "%s", disc);
    }
    if (disc == NULL && s_search_disc && !open_disc_from_folder()) {
        aurora_shutdown();
        return 0;
    }
    {
        /* the disc's main.dol must be the USA one the build is verified against (pc_disc_check.c; Part 1 item 2c) */
        extern int pc_disc_check_main_dol(int show_box);
        if ((disc != NULL || s_search_disc) && !pc_disc_check_main_dol(!automated_run())) {
            aurora_shutdown();
            return 0;
        }
    }
    if (disc != NULL || s_search_disc) {
        /* every file on the disc against the build's reference list, once per image (pc_disc_check.c,
         * 2026-11-02): a bad dump or a modified image is named before the game starts */
        extern int pc_disc_verify_files(const char* image, const char* cache, int interactive);
        char cache[1300];
        /* data\system\cache\Pokemon Colosseum\ in the portable layout (snatcherLeo, 2026-11-21; flat: Colosseum\ as before). The
         * folder is made first, since fopen won't; SDL_CreateDirectory makes missing parents too. An old
         * Pokemon Colosseum\disc-check.txt is no longer read: the first start re-checks once (a few seconds). */
        pc_paths_dir(PC_PLACE_GAME_CACHE, cache, sizeof(cache));
        SDL_CreateDirectory(cache);
        pc_paths_get(PC_PLACE_GAME_CACHE, "disc-check.txt", cache, sizeof(cache));
        if (!pc_disc_verify_files(disc != NULL ? disc : s_disc_image, cache, !automated_run())) {
            aurora_shutdown();
            return 0;
        }
    }
#else
    if (disc != NULL || s_search_disc) {
        fprintf(stderr, "[orreport] this build has no disc support (Aurora's DVD library was not built)\n");
        return 0;
    }
#endif
#ifdef PC_HAVE_AURORA_CARD
    pc_card_init();
#endif
    OSInit();
    pc_vi_begin();
    pc_startup_mark("disc checked, save slots registered (the card opens after the hub)");
    /* the app's own health (pc_health.c): the hang watch, the performance log, the memory watch; after the disc
     * checks, whose first run takes long enough to look like a hang */
    {
        extern void pc_health_start(void);
        pc_health_start();
    }
    /* the font renderer (pc_font_render.cpp, step 2.2 part A): before the game draws its first letter */
    {
        extern void pc_font_render_start(void);
        pc_font_render_start();
    }
    /* the game's own messages in the player's language (pc_game_text.cpp, 2.2 step 4): the same "language" setting
     * pc_text_init read above, before the game looks up its first message */
    {
        extern void pc_game_text_start(void);
        pc_game_text_start();
        /* the Language row on the game's Options screen (pc_options_lang.cpp) */
        extern void pc_options_lang_start(void);
        pc_options_lang_start();
        /* the main menu's LOAD SAVE button and the Continue question's YES-first tweak (pc_title_loadsave.cpp); after
         * the Language row, whose item hook it chains onto */
        extern void pc_title_loadsave_start(void);
        pc_title_loadsave_start();
        /* OrrePort's naming keyboard, version B (pc_name_keyboard.cpp; 2026-11-14): built in; its hooks step aside
         * in GameCube-accurate mode and recordings, which keep the game's own name screen */
        extern void pc_name_keyboard_start(void);
        pc_name_keyboard_start();
        /* dialogue re-wrapping (pc_msg_rewrap.cpp; 2026-11-14): off in GameCube-accurate mode and recordings */
        extern void pc_msg_rewrap_start(void);
        pc_msg_rewrap_start();
        /* the full names beside the save's GameCube names and the extras file (pc_names.cpp; 2.4): after the re-wrap
         * module, whose test switch it chains */
        extern void pc_names_start(void);
        pc_names_start();
        /* the field pause menu's PC MENU and Start closing it, and a PC MENU save's exact spot (pc_field_menu.cpp; Main
         * 2026-11-15): built in, off in GameCube-accurate mode and recordings; last, as it chains the item hook (the
         * naming keyboard's) and the card-task hook (the names') */
        extern void pc_field_menu_start(void);
        pc_field_menu_start();
        /* OrrePort's rows on the PC screen, first Main Menu (pc_pc_screen.cpp; 2026-11-15): built in, off in
         * GameCube-accurate mode and recordings; after the field menu and LOAD SAVE, whose item hook and main-menu
         * hook it chains */
        extern void pc_pc_screen_start(void);
        pc_pc_screen_start();
        /* the autosave (pc_autosave.cpp; 2026-11-20, docs/port/autosave.md): off in GameCube-accurate mode and in
         * automated runs unless ORRE_AUTOSAVE=1. Before the softlock fix's start: it chains Continue's spot hook (PC
         * MENU's) so the hang watch, which wraps it next, also arms after an autosave's Continue */
        extern void pc_autosave_start(void);
        pc_autosave_start();
        /* PC MENU saves made in the middle of a scene, the softlock fix (pc_save_windows.cpp; 2026-11-20): the
         * open-scene table's rollbacks and the hang watch; off with PC MENU (accurate mode, recordings). Last, as it
         * chains Continue's spot hook (PC MENU's), the card-task hook and the room-leave tick (Main Menu's) */
        extern void pc_save_windows_start(void);
        pc_save_windows_start();
        /* the save-check sweep's test switches (pc_savecheck.cpp; 2026-11-20): off unless ORRE_SAVECHECK or
         * ORRE_SAVECHECK_PROBE is set in an automated run. After the softlock fix's start, so it chains last */
        extern void pc_savecheck_start(void);
        pc_savecheck_start();
        /* the pose rows (pc_pose_rows.cpp; 2026-11-21, Vander back on the floor): after a Continue from a saved spot,
         * people a scene left in a pose are put back (ui/game/pose_rows.ini); off with PC MENU (accurate mode,
         * recordings). After the save-check sweep's start, so it chains Continue's spot hook last and sees its final
         * answer */
        extern void pc_pose_rows_start(void);
        pc_pose_rows_start();
        /* step 3.7's item tweaks (pc_bag_tweaks.cpp; 2026-11-20): the party list after a use or a give, the TM design,
         * infinite TMs, the scents' remembered cursors, the bag's USE / READ / CHECK / TOSS rule; each hook asks its
         * switch when called (off in GameCube-accurate mode and recordings). Chains nothing */
        extern void pc_bag_tweaks_start(void);
        pc_bag_tweaks_start();
        /* step 3.7's Relic Forest shortcuts (pc_relic_forest.cpp; 2026-11-20): PURIFY in the party menu and the Time
         * Flute anywhere in the forest, one switch; each hook asks it when called (off in GameCube-accurate mode and
         * recordings). Chains the menu item hook, so it comes after the other menus' */
        extern void pc_relic_forest_start(void);
        pc_relic_forest_start();
        /* step 3.7's right-stick walking (pc_cstick_walk.cpp; 2026-11-20): the C-stick walks Wes in the field; the
         * hook asks its switch when called (off in GameCube-accurate mode and recordings). Chains nothing */
        extern void pc_cstick_walk_start(void);
        pc_cstick_walk_start();
        /* step 3.7's Smooth Party UI (pc_party_ui.cpp; 2026-11-20): the party screen's portraits appear together; the
         * hooks ask its switch when called (off in GameCube-accurate mode and recordings). Chains nothing */
        extern void pc_party_ui_start(void);
        pc_party_ui_start();
    }
    return 1;
}

void pc_platform_stop(void) { aurora_shutdown(); }
