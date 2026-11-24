/*
 * pc_settings_overlay.cpp: the RmlUi trial's in-game Settings overlay (docs/port/rmlui-trial.md, the hub plan's step 1),
 * shaped after the project's mockups 13 (dark) and 13a (light) in docs/design/mockups/.
 *
 * Phase A: the overlay draws over the running game and dims it; while it is open, the keyboard, mouse and controllers
 * go to it only (an Aurora input layer above the game's captures them, so the game's pad reads see no buttons). The
 * game is not paused yet (phase B). Closed, the layer passes every event through untouched, except the open keys.
 *
 * Open keys (plan section 4, decided): Esc, Xbox View (Back), GameCube Z + START. The trial keeps F2 too. Z + START
 * holds Z back about 4 reads (snatcherLeo, 2026-11-09, decided; pad_filter below). *Updated 2026-11-12:* the shortcut is
 * chosen in Controls, one or two buttons; a GameCube controller's default is Z + L (snatcherLeo), held back the same way
 * (pc_pad_shape.cpp). Z is the right bumper on most controllers
 * and the Back button on the Switch Online GameCube controller (Aurora's pad.cpp), so on a GameCube-type controller
 * Back is Z, not "View", and does not open the menu on its own.
 *
 * Platform layer only: nothing in the game code. Built only with AURORA_ENABLE_RMLUI (port/CMakeLists.txt).
 */
#include <aurora/aurora.h> /* aurora_set_present_inset_top: the game picture below an always-shown menu bar */
#include <aurora/gfx.h>
#include <aurora/input.hpp>
#include <aurora/rmlui.hpp>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <SDL3/SDL.h>
#include <dolphin/pad.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

/* pc_svg.cpp: OrrePort's flat SVG art into an RGBA picture (the hub's reader; the menu's icons use it too) */
bool pc_svg_render(const std::string& svg, int width, int& height, std::vector<unsigned char>& rgba,
                   const std::vector<std::string>& skip_fills);
/* pc_hub_saves.cpp: GameCube-accurate mode's "What it turns off" list as RML (General's link shows it too; 2.12) */
std::string hub_accurate_list_rml();
std::string pc_whatadds_rml(); /* pc_whatadds.cpp: the "What OrrePort adds" list (5.13) */

#include "pc_pad_shape.h" /* the menu shortcut and Player 1's read filter */
#include "pc_speed.h"     /* the speed badge's number (pc_speed_badge_serial) */
#include "pc_text.h"      /* the pages' words in the player's language (C++ parts too, so outside extern "C") */
#include "pc_cheats.h"    /* the Cheats menu's switches (mockup 63 v2) */
#include "pc_adapter_setup.h" /* Controls' "GameCube adapter" row (Windows only does anything) */
#include "pc_font_files.h"    /* the shipped fonts' folder and the Noto fallback chain (2026-11-16) */
#include "pc_paths.h"         /* the UI folder, Reports\, the OrrePort folder (4.1) */

extern "C" {
#include "pc_settings.h"
#include "pc_options.h" /* the Tweaks rows: pc_option, pc_option_set, pc_option_gamecube_accurate */
#include "pc_smooth.h"  /* Graphics › Frame rate: pc_smooth_setting_changed (2026-11-18) */
#include "pc_update.h"  /* the About tab's update rows (4.4) */
#include "pc_apps.h"    /* Go back's "Studio is open" check and its Close Studio (2026-11-23) */
#include "pc_log.h"     /* pc_log_drain before About's Restart now / Go back exit */
void pc_platform_toggle_fullscreen(void);
int pc_platform_is_fullscreen(void);
void pc_settings_overlay_frame(void);
void pc_settings_overlay_open_report(void);
int pc_ui_load_fonts(void);
const char* pc_platform_data_path(const char* name, char* out, size_t size);         /* pc_platform.c */
const char* pc_debug_build_stamp(void);                                              /* pc_debug.c */
int pc_debug_enabled(void);    /* pc_debug.c: the debug toolkit is on (ORRE_DEBUG, or Settings › General › Debug tools) */
void pc_debug_ui_toggle(void); /* pc_debug_ui.cpp: Help › Debug menu, as F1 */
void pc_frame_dump_request_screenshot_to(const char* path);                          /* pc_frame_dump.c */
int pc_frame_dump_screenshot_busy(void);
int pc_report_write(const char* note, const char* screenshot, char* out_zip, size_t out_size); /* pc_report.cpp */
void pc_report_show_in_folder(const char* zip);
int pc_clipboard_set_file(const char* utf8_path); /* pc_clip_file.c: the saved report, for Ctrl+V */
int pc_platform_automated_run(void); /* pc_platform.c */
int pc_platform_restart_to_hub(void); /* pc_platform.c: Quit to hub (returns only if it failed) */
int pc_card_used_within(unsigned ms); /* pc_card_timing.c: the game touched the memory card in the last ms */
void pc_window_icon_set(const char* colour); /* pc_window_icon.cpp */
void pc_window_icon_game(int on);
int pc_window_icon_game_available(void);
void pc_controls_ui_open(void);                 /* pc_controls_ui.cpp: the controller setup screen */
void pc_controls_ui_open_ff(int keyboard);      /* ... opened on its Fast-forward row, for the keyboard or controller */
int pc_controls_claim_gamecube_pad(void);       /* pc_controls_ui.cpp: the adapter's controller made Player 1 */
int pc_hub_open_labels(void);                   /* pc_hub.cpp: the disc label page (Appearance > Disc label) */
int pc_hub_open_langdiscs(void);                /* pc_hub.cpp: the Language discs panel (General > More languages) */
int pc_hub_disc_ready(void);                    /* pc_hub.cpp: 1 when the hub has a disc (disc art can open) */
void pc_controls_ui_frame(void);
int pc_controls_ui_is_open(void);
const char* pc_controls_menu_name(int keyboard); /* the menu shortcut's name (pc_pad_shape.cpp pc_shortcut_name) */
/* pc_audio.c; weak: the platform's test programs link this file without the sound code */
void pc_audio_set_volumes(int master, int music, int effects) __attribute__((weak));
void VISetFrameBufferScale(float scale);                        /* Aurora: 0 = the window's size, N = N x 640x480 */
void pc_window_title_bar_style(int dark);
void pc_window_cloak(int on);
void pc_platform_set_title(const char* title); /* pc_platform.c (an automated run keeps its own warning title) */
}

namespace {
namespace input = aurora::input;

/* OPEN: a row that opens another screen. GROUP: a small caps label (and on the full page its short line) above the rows
 * after it, never a stop for the controller (the Speed settings' three groups, snatcherLeo 2026-11-16, mockup s7) */
enum Kind { SELECT, TOGGLE, SLIDER, INFO, DISABLED, ACTION, OPEN, GROUP };
enum Live {
    NONE, LIVE_WINDOW, LIVE_THEME, LIVE_PAUSE, LIVE_ANIM, LIVE_GAMEICON, LIVE_MENUBG, LIVE_MENUHINT, LIVE_MENUSIZE, LIVE_QUITASK,
    LIVE_MENUBAR, LIVE_DISCLABEL, LIVE_DEBUG, LIVE_LANG, LIVE_LANGDISCS,
    LIVE_BACKEND, LIVE_RES, LIVE_VSYNC, LIVE_MSAA, LIVE_ANISO, LIVE_VOL_MASTER, LIVE_VOL_MUSIC, LIVE_VOL_EFFECTS,
    LIVE_SPEED_GAME, LIVE_SPEED_BATTLE, LIVE_SPEED_SOUND, LIVE_ADAPTER,
    LIVE_FF_SPEED, /* Enhancements › Speed › Fast-forward (2×, 3×, 4×, 5×) */
    LIVE_FF_LINK,  /* ... its green link naming the button ("LB · Change ›"): its own controller stop, drawn in the row */
    LIVE_FF_MODE,  /* Fast-forward works: Hold / Press to switch */
    LIVE_FF_BADGE, /* Speed badge: Show / Hide */
    LIVE_OPTION, /* a Tweaks row: one pc_options.c switch, its id in options[0] */
    LIVE_ACC_DEFAULT, /* General: GameCube-accurate mode for new saves (2.12) */
    LIVE_ACC_WHAT,    /* General: "What it turns off" (2.12) */
    LIVE_IMPORT_ASK,  /* General: Ask how to play imported saves (3.5, the import question; settings.ini import_ask) */
    LIVE_UPD_CHECK,   /* About: Check for updates (4.4; settings.ini update_check; off = fully offline) */
    LIVE_UPD_AUTO,    /* About: Download updates automatically (4.4; settings.ini update_auto_download, off by default) */
    LIVE_UPD_VERSION, /* About: "OrrePort 0.1.1", its status, and Check for updates / Download / Restart now */
    LIVE_UPD_BACK,    /* About: Go back to <previous version> (only once one is kept) */
    LIVE_LOGS,        /* About: the logs folder (where both programs' logs are), Open folder */
    LIVE_ADDS,        /* About: "What OrrePort adds" (5.13): its list in the "What it turns off" window */
    LIVE_FRAME_RATE  /* Graphics: Frame rate, frame smoothing's choice (step 3.4, 2026-11-18; pc_smooth.c) */
};
const char* const kIconColour = "rust"; /* the O's one colour (snatcherLeo, 2026-11-11: the signature red, "Rust") */
/* the window's title, as in mockups 11 and 13 (snatcherLeo, 2026-11-10, comparing the trial's title bar with the mockup:
 * "It's subelt, but could we make it like that?"; the "(trial build)" suffix dropped; the build stamp stays in the
 * log and the report's about.txt) */
/* "OrrePort · Pokémon Colosseum": the game's name in the player's language (its official name there, if the language
 * file gives one); "OrrePort" is a name and stays */
const char* window_title() {
    static std::string title;
    title = std::string("OrrePort · ") + pc_text("hub.game.colosseum.title");
    return title.c_str();
}
/* ... and just "OrrePort" while the hub is up, before a game starts (snatcherLeo, same day: "Could we have the top bar too
 * just say "OrrePort" for now, and then, it can change when Colosseum is being played?"); the game's title is set
 * when the overlay starts with the game's first frame (pc_settings_overlay_frame) */
const char* const kHubTitle = "OrrePort";

/* *Updated 2026-11-13 (every word translatable; snatcherLeo: "make it translateable, and we'll ensure that moving forward, it's
 * always like this"):* name and sub are text keys in ui/lang/en.txt ("settings.<row>", "settings.<row>.sub"); an
 * option is a key when it starts with "settings." or "common." (word() below), else shown as written (numbers, product
 * names such as Vulkan, the › arrow) */
struct RowDef {
    int tab;
    Kind kind;
    const char* name;
    const char* sub;
    std::vector<std::string> options; /* SELECT: the choices; SLIDER: the steps' labels; INFO / DISABLED: the text */
    int value;
    Live live;
    /* GameCube-accurate mode's lock (2.12, mode_values below): set only while a game runs in accurate mode; the row is
     * greyed, can't be changed, and says why (lock_reason, a text key, in place of its second line); lock_tag, when
     * set, replaces its control with the amber "off in accurate mode" tag and a lock (orreport-design-overview.md 4.1) */
    bool locked = false;
    const char* lock_reason = nullptr;
    const char* lock_tag = nullptr;
    /* the side menu's shorter tag (one 46 dp line; snatcherLeo's polish list 2026-11-16, mockup item 3 "After B"); nullptr:
     * lock_tag there too */
    const char* lock_tag_panel = nullptr;
    /* the choice tagged "Recommended" (a sage pill in the box on the full page, and in the open list); -1 none */
    int rec = -1;
};

const char* const kTabs = "General Appearance Graphics Enhancements Audio Controls About"; /* for the log only */
enum { TAB_COUNT = 7 }; /* About (6) added 2026-11-23, shown last */
/* The categories' ids (RowDef.tab, kTabNames, the pages' "tab-<id>" elements, ORRE_SETTINGS_TAB) and the order they are
 * shown in are kept apart. *Changed 2026-11-16 (snatcherLeo: "yeah move Enhancements up"):* the order is General, Appearance,
 * Graphics, Enhancements, Audio, Controls (Enhancements was last); every id stays as it was (Enhancements is still 5),
 * so test switches, the disc art page's way back (Appearance) and every row keep pointing at the same category. The
 * side menu's category list, the page's column (settings.rml lists its tabs in this order), Up / Down on the column and
 * L / R follow kTabOrder. All platforms */
const int kTabOrder[TAB_COUNT] = {0, 1, 2, 5, 3, 4, 6};
int tab_pos(int tab) {
    for (int p = 0; p < TAB_COUNT; ++p) {
        if (kTabOrder[p] == tab) return p;
    }
    return 0;
}
int tab_at(int pos) { return kTabOrder[((pos % TAB_COUNT) + TAB_COUNT) % TAB_COUNT]; }

std::vector<RowDef> s_rows = {
    /* GameCube-accurate mode's default for new saves, on its own line at the top of General (2.12; snatcherLeo, 2026-11-16: "A,
     * top of General", then "Yes, approve the wording, build it"): Off / On, saved as settings.ini accurate_new_saves
     * (pc_options.c). Each save keeps its own setting (the Saves page); in game the row is greyed, "Close the game to
     * change this" (mode_values). UI ids: settings.general.accurate_default, and accurate.what_off for the link row */
    {0, TOGGLE, "settings.general.accurate_default", "settings.general.accurate_default.sub", {}, 0, LIVE_ACC_DEFAULT},
    {0, OPEN, "accurate.what_off.link", "settings.general.accurate_what.sub", {"&#8250;"}, 0, LIVE_ACC_WHAT},
    /* the import question's way back (3.5; snatcherLeo, 2026-11-19: the tick "makes the answer the default for new saves ...
     * unticking that setting brings the question back"): an ordinary switch right under the accurate card, on by
     * default, saved as settings.ini import_ask. The hub's question turns it off when "Use this for every import" is
     * ticked (pc_hub_saves.cpp import_ask_answer); turning it on here brings the question back. Its own row, as Ask
     * before Quit to hub is, rather than a third stop inside the card: the card stays as approved (mockup 1b), and the
     * row is found where the player looks for the default it works with. Never locked (it only changes the hub). UI id
     * settings.general.import_ask */
    {0, TOGGLE, "settings.general.import_ask", "settings.general.import_ask.sub", {}, 1, LIVE_IMPORT_ASK},
    {0, SELECT, "settings.window", "settings.window.sub", {"settings.window.windowed", "settings.window.fullscreen"}, 0,
     LIVE_WINDOW},
    /* the language (snatcherLeo, 2026-11-13: every word translatable): Follow Windows, then each ui/lang file by its own name
     * (filled in at start, lang_rows); saved as "language" (auto or a file's code); applies at the next start, as every
     * page reads its words when it loads */
    {0, SELECT, "settings.language", "settings.language.sub", {"settings.language.auto"}, 0, LIVE_LANG},
    /* the way into the hub's Language discs from Settings (snatcherLeo, 2026-11-15: mockup S, "S for the Settings screen
     * makes sense"): opens the hub's panel unfolded (pc_hub_open_langdiscs); in game it says where to find it */
    {0, OPEN, "settings.lang_discs", "settings.lang_discs.sub", {"&#8250;"}, 0, LIVE_LANGDISCS},
    {0, TOGGLE, "settings.pause", "settings.pause.sub", {}, 0, LIVE_PAUSE},
    /* off by default since 2026-11-12 (snatcherLeo: "No, don't pause", "useres can change that, and have it pause if they
     * want"); it was on by default from 2026-11-09 */
    /* the hint at game start (v2's first-run hint; snatcherLeo, 2026-11-12: "users can remove that in settings, to never see
     * that hint again", "that needs to go away faster"): on by default, 4 seconds; saved as ui_menu_hint */
    {0, TOGGLE, "settings.menu_hint", "settings.menu_hint.sub", {}, 1, LIVE_MENUHINT},
    /* which menu the menu key opens in game (snatcherLeo, 2026-11-12: "users can also decide if, by default, it opens up in
     * teh smaller window, or the larger window, just for when they're playing in-game. In the hub, it always opens up
     * in teh larger window, no matter what"): the side panel (default) or the full window (the hub's page layout,
     * over the game); saved as menu_full */
    {0, SELECT, "settings.menu_size", "settings.menu_size.sub",
     {"settings.menu_size.side_panel", "settings.menu_size.full_window"}, 0, LIVE_MENUSIZE},
    /* Quit to hub's question, which its "Don't ask again" box turns off (snatcherLeo, 2026-11-13); saved as quit_hub_ask */
    {0, TOGGLE, "settings.quit_ask", "settings.quit_ask.sub", {}, 1, LIVE_QUITASK},
    {0, ACTION, "settings.report", "settings.report.sub", {"settings.report.open"}, 0, NONE},
    /* the developers' debug menu (snatcherLeo, 2026-11-13: "Can you enable the Debug menu to be opened firstly, within our
     * build? Where would we put it under?"). Before, only ORRE_DEBUG=1 at start turned the toolkit on (pc_debug.c), so
     * a build started by double-click never had it. *Updated the same night (snatcherLeo: "On default, Debug tools should
     * always be on, but can be turned off ofc with the togle we have"):* on by default (was off). Room jumps, flags and
     * items can change a save in ways the game never could; the row says so. Applies at the next start because the
     * toolkit sets itself up once (pc_debug_enabled reads it then). Saved as debug_tools; ORRE_DEBUG, when set, still
     * wins (test runs) */
    /* *Corrected 2026-11-13 (night):* off by default, and listed only while it is on (tab_rows): players never see it
     * unless they turned it on by hand (snatcherLeo: "I don't think even playtesters need debug menu"; pc_debug.c) */
    {0, TOGGLE, "settings.debug", "settings.debug.sub", {}, 0, LIVE_DEBUG},
    {1, SELECT, "settings.theme", "settings.theme.sub",
     {"settings.theme.follow", "settings.theme.light", "settings.theme.dark"}, 0, LIVE_THEME},
    /* no Icon colour row: the O is always the Rust design (snatcherLeo, 2026-11-11: "Let's get rid of "Icon color". Keep it
     * simple, we'll keep the signature red we got, the "Rust" design"); the sage and white files stay unused */
    /* the title bar's top-left icon (snatcherLeo, 2026-11-11: "allow them to change upper left-hand icon with the espeon and
     * umbreon logo from the gamecube disc itself", "The onwe with Umbreon's thing fully lit"; then, seeing it: "It
     * should say "OrrePort logo" ... in real-time, the top left corner should change ... for XD, all we do is just
     * simply add that as another option ... change this settings option to just say "Window icon"", "Window Icon
     * doesn't have to be the last settings row"): the OrrePort logo, or a game's save icon read from the player's own
     * files (Colosseum: frame 4, Espeon and Umbreon), shown at once, in the hub and in game; the taskbar always shows
     * the O ("O is the only thing users will have in the taskbar/desktop icon"). Listed only when an imported game
     * offers an icon ("only shows up in apperance (in settings) AFTER the colosseum ROM has successfully been
     * imported"); XD's icon becomes a third choice. Saved as ui_window_icon (0 the logo, 1 Espeon and Umbreon) */
    {1, SELECT, "settings.window_icon", "settings.window_icon.sub",
     {"settings.window_icon.logo", "settings.window_icon.espeon_umbreon"}, 0, LIVE_GAMEICON},
    /* "Button hints above the menu" (snatcherLeo, 2026-11-09) is gone with the banner it hid: v2 (snatcherLeo, 2026-11-12, "Yes,
     * build v2") shows the controller's own buttons in the panel's footer and a first-run hint instead */
    /* connected 2026-11-11 (snatcherLeo: "when animations are off, that just means teh disc never moves right? ... if not,
     * code it in. BY default, the animations are on ... we can change it to "On:""): the hub's disc sway and its
     * Play lift and spin (pc_hub.cpp, hub.rcss body.noanim); saved as ui_animations */
    {1, TOGGLE, "settings.animations", "settings.animations.sub", {}, 1, LIVE_ANIM}, /* snatcherLeo, 2026-11-11: "more concise" */
    /* the hub disc's label (mockup 47's label shelf, 2026-11-13): opens the hub's disc art page (pc_hub.cpp); in game
     * it says where to find it, since the disc is only in the hub. Named "Change disc art" (snatcherLeo, same day: "change it
     * to "Change Disc Art""; written in the rows' sentence case) */
    {1, OPEN, "settings.disc_art", "settings.disc_art.sub", {"&#8250;"}, 0, LIVE_DISCLABEL},
    /* the menu bar in game (mockup 46) appears only while the mouse is at the top of the window, unless this is on
     * (snatcherLeo, 2026-11-13: "We can void teh timer altogether ... a settings togle to always show Top Toolbar ... but by
     * default, we always have it disappear"); saved as menu_bar_always */
    {1, TOGGLE, "settings.menu_bar", "settings.menu_bar.sub", {}, 0, LIVE_MENUBAR},
    /* the in-game panel's background (snatcherLeo, 2026-11-11: "Transparent by default, users can make it solid if they
     * want"); saved as ui_menu_solid */
    {1, SELECT, "settings.menu_bg", "settings.menu_bg.sub", {"settings.menu_bg.see_through", "settings.menu_bg.solid"}, 0,
     LIVE_MENUBG},
    /* Graphics and Audio, connected 2026-11-11 (snatcherLeo: "Let's do it, actually make those things real, for all of those
     * things"). The defaults are what the port really does (the earlier mock values were placeholders): the renderer
     * Vulkan (settings.ini gpu_backend), the resolution the window's (render_scale 0, live), V-Sync off (vsync, live;
     * Aurora then presents without waiting and the game paces itself), no MSAA (msaa 1 / 4; WebGPU has only 1 or 4
     * samples), texture filtering None (anisotropy 1; was 16 until 2026-11-13, snatcherLeo: "should be on none for new users").
     * Automated runs keep anisotropy 16 whatever is saved (pc_platform.c), so recorded checks are unchanged. Renderer, MSAA and anisotropy are read at start-up (pc_platform.c), so their
     * texts say so. Volumes live (pc_audio.c: master as the output's gain, music and effects through a mixer hook). */
    /* *Direct3D 11 added 2026-11-24 (snatcherLeo: "A: an option"; port-gaps 90):* on Intel graphics it is the one renderer
     * that neither freezes on a restore from maximized (Vulkan, gap 90) nor draws gap 76's glitches (Direct3D 12),
     * verified on snatcherLeo's laptop. Its cost: Windows' old shader compiler builds the big ubershader slowly, up to 80 s the
     * first time a variant is needed (cached after). Vulkan stays the default. Windows only, as is Direct3D 12; Linux
     * and macOS would list Vulkan / Metal instead (likely). */
    {2, SELECT, "settings.renderer", "settings.renderer.sub", {"Vulkan", "Direct3D 12", "Direct3D 11"}, 0, LIVE_BACKEND},
    /* the Graphics page as approved (snatcherLeo, 2026-11-13, mockup 45: "Yes, and for Native, do 640 x 480 480p ... Default
     * should be on "Match the Window""): Dolphin's pairing (n x for an n x 360-line screen, drawn taller and scaled
     * down) in our own words, up to 8x for 5K; Match the window for anything bigger. The box shows a short form
     * (select_box_text) */
    {2, SELECT, "settings.resolution", "settings.resolution.sub",
     {"settings.resolution.0", "settings.resolution.1", "settings.resolution.2", "settings.resolution.3",
      "settings.resolution.4", "settings.resolution.5", "settings.resolution.6", "settings.resolution.7",
      "settings.resolution.8"},
     0, LIVE_RES},
    /* moved here from Enhancements (snatcherLeo, 2026-11-12: "should that not be under "Graphics" when we eventually put it,
     * along with aspect ratio?"), then right under Internal resolution (snatcherLeo, 2026-11-19, at the 3.4 hand check: "we
     * can put Framerate right below Internal Resolution, I think that would make sense").
     * *Built 2026-11-18 (step 3.4; snatcherLeo, 2026-11-16, the decision log "Frame smoothing's dropdown keeps every entry" and
     * "The frame-rate row names the monitor's rate"):* frame smoothing's choice, every entry selectable: Off (original),
     * Match (the default and Recommended; its label names the monitor's rate, "Match (120 Hz)", from the monitor the
     * window is on; *shortened 2026-11-19* from "Match your display (120 Hz)", snatcherLeo: "the drop down box can also be less
     * wide ... we can shorten length on match monitor"; the row's second line says what it matches), then fixed caps
     * 60, 120, 144, 165, 240 (the options hold the numbers; frame_rate_option words them). Saved as settings.ini
     * frame_smoothing in ORRE_SMOOTH's own words (off, match, 60 ...), applied live (pc_smooth.c, at the next game
     * step). Its second line says when a cap is above the monitor's rate (it then runs at the monitor's rate, like
     * Match), and on Windows when the monitor could run faster than Windows has it set (frame_rate_sub). Locked off in
     * GameCube-accurate mode until the the reference playthrough gate passes (mode_values). UI id: settings.frame_rate (the row's name key,
     * as for the Speed rows); its notes are the keys settings.frame_rate.above and settings.frame_rate.held_back */
    {2, SELECT, "settings.frame_rate", "settings.frame_rate.sub",
     {"settings.frame_rate.off", "settings.frame_rate.match", "60", "120", "144", "165", "240"}, 0, LIVE_FRAME_RATE,
     false, nullptr, nullptr, nullptr, 1},
    /* *Changed 2026-11-23 (snatcherLeo: "where the frame rate will match the monitor's framerate, let's not do that. Instead,
     * the FPS should be on original"):* a new player starts on Off (original), the game's own 30 / 60 pictures a
     * second; Match stays in the list, still tagged Recommended (the last 1). Only a missing frame_smoothing key takes
     * the default (PC_SMOOTH_SETTING_DEFAULT, pc_smooth.h); a settings.ini that already says "match" keeps it. */
    {2, TOGGLE, "settings.vsync", "settings.vsync.sub", {}, 0, LIVE_VSYNC},
    /* WebGPU (Aurora's graphics layer) allows 1 or 4 samples, so no 8x / SSAA (snatcherLeo asked, 2026-11-12) */
    {2, SELECT, "settings.msaa", "settings.msaa.sub", {"common.none", "4× MSAA"}, 0, LIVE_MSAA},
    /* a list as Dolphin's (was a slider); None is the game's own filtering (anisotropy 1). None is also the default
     * for new users, like Anti-aliasing (snatcherLeo, 2026-11-13: "by default, texture filtering and the anti-aliasing should
     * be on none for new users"): a first start looks as the GameCube drew it; sharper is a choice (was 16x) */
    {2, SELECT, "settings.aniso", "settings.aniso.sub",
     {"common.none", "settings.aniso.2", "settings.aniso.4", "settings.aniso.8", "settings.aniso.16"}, 0, LIVE_ANISO},
    /* Aspect ratio: faded until built. Widescreen left the playtest (snatcherLeo, 2026-11-20) and comes with the UI Editor:
     * 16:9, 16:10 and 21:9 together (docs/port/widescreen-plan.md section 1). Until then the row shows the game's own
     * shape as its value, with "Coming in a future update." as its second line (snatcherLeo: "have it faded out and say
     * "Coming in a future update" for now ... it'll be on "Default" (or whatever consistent word we used for that)";
     * "original" is that word, as in Frame rate's "Off (original)"). UI id: settings.aspect. Frame rate moved up under
     * Internal resolution (above) */
    {2, DISABLED, "settings.aspect", "settings.aspect.sub", {"settings.aspect.original"}, 0, NONE},
    /* Speed (snatcherLeo, 2026-11-13, mockup 64 v4; pc_speed.c applies them each frame): the note first ("put that at the
     * beginning of "Speed""), then Game speed, Walking speed (above Battles: "Walking speed makes sense, that should be
     * above Battles"; not built yet), Battles, then Music and sound. Saved as speed_game, speed_battle, speed_sound.
     * *Replaced the same night:* the "Speed up the sound too" switch (snatcherLeo: "is confusing") became the Music and sound
     * row with two choices that say what you hear (mockup 66; snatcherLeo: "I want both - the ability to speed up the world
     * but not the music, and then, the ability to have that sped up too") */
    /* *Rebuilt 2026-11-16 (snatcherLeo, after the mockups of "A rearranged": "SOunds good"; mockup s7, the decision log "The Speed
     * settings and the fast-forward key"):* the Speed heading, then three groups, each a caps label with one short line
     * on the full page (GROUP rows; in the side menu the label alone, the line in the help box of a row without its
     * own): FAST-FORWARD (Fast-forward 2× / 3× / 5× with the button's green link, Fast-forward works, Speed badge),
     * ALWAYS FASTER (Game speed, Battles), SOUND (Music when faster). The mockup's Fast-forward row repeated its group's
     * line, so the row has none of its own (sub nullptr): the group says it. pc_speed.c applies them each frame;
     * settings.ini keys in pc_speed.h (the older three keep their names and values; 5× was added at the end).
     * *4× added 2026-11-17* (snatcherLeo, 2026-11-16: "yeah add 4x next time"): shown in order between 3× and 5×, but saved
     * under the next free code at the end of each list, so a saved 5× stays 5×; the rows' values are row positions,
     * turned into saved codes with pc_speed_code_for_row / pc_speed_row_for_code (pc_speed.h). Labels stay plain "×"
     * text, the same in every language (no keys).
     * UI ids (for the UI editor): the rows' name keys; the link settings.speed_ff.link; the groups
     * settings.speed.group_ff / _always / _sound. Earlier history: Walking speed dropped, "Built-in improvements"
     * removed, "(only)" removed from the side menu's names (2026-11-16); "Music and sound" became "Music when faster" */
    {5, INFO, "settings.speed_note", "settings.speed_note.sub", {""}, 0, NONE},
    {5, GROUP, "settings.speed.group_ff", "settings.speed.group_ff.sub", {}, 0, NONE},
    {5, SELECT, "settings.speed_ff", nullptr, {"2×", "3×", "4×", "5×"}, 1, LIVE_FF_SPEED},
    {5, OPEN, "settings.speed_ff.link", "settings.speed_ff.link.sub", {"&#8250;"}, 0, LIVE_FF_LINK},
    {5, SELECT, "settings.speed_ff_mode", "settings.speed_ff_mode.sub",
     {"settings.speed_ff_mode.hold", "settings.speed_ff_mode.press"}, 0, LIVE_FF_MODE},
    {5, SELECT, "settings.speed_badge", "settings.speed_badge.sub",
     {"settings.speed_badge.show", "settings.speed_badge.hide"}, 0, LIVE_FF_BADGE},
    {5, GROUP, "settings.speed.group_always", "settings.speed.group_always.sub", {}, 0, NONE},
    {5, SELECT, "settings.speed_game", "settings.speed_game.sub",
     {"settings.speed.normal", "1.5×", "2×", "3×", "4×", "5×"}, 0, LIVE_SPEED_GAME},
    {5, SELECT, "settings.speed_battle", "settings.speed_battle.sub",
     {"settings.speed.same", "settings.speed.normal", "1.5×", "2×", "3×", "4×", "5×"}, 0, LIVE_SPEED_BATTLE},
    {5, GROUP, "settings.speed.group_sound", "settings.speed.group_sound.sub", {}, 0, NONE},
    {5, SELECT, "settings.speed_sound", "settings.speed_sound.sub",
     {"settings.speed_sound.normal", "settings.speed_sound.faster"}, 0, LIVE_SPEED_SOUND, false, nullptr, nullptr,
     nullptr, 0},
    /* Tweaks (snatcherLeo, 2026-11-14, through C: "Tweaks", the project's suggestion): the on/off gameplay switches of pc_options.c,
     * after Speed, one row each as each is built (with 2.6: Access PC menu anywhere, B closes the pause menu). The
     * option's id rides in options[0] (a TOGGLE uses no options); the value is pc_option's, saved with pc_option_set,
     * so the menu bar's Gameplay menu (the quick toggle for the same switches) and this list always agree. In
     * GameCube-accurate mode every switch is forced off (pc_options.c), so the rows say so instead of toggling (setup
     * in open_settings). The removed "Built-in improvements" placeholder stays gone: only switches that exist.
     * *Changed 2026-11-16 (snatcherLeo: "For tweaks, we can have it be smaller, like "SOUND""):* a GROUP caps label like the
     * Speed groups, no longer a big focusable heading row; its line stays on the full page */
    /* *Changed 2026-11-21 (snatcherLeo: "Get rid of sliders (or options) for these tweaks, since I don't want people to be
     * messing with them ... they'll be built in"):* the rows of Continue and New Game start on YES, Smooth Party UI,
     * Fast loading, Keep using an item, TMs: the LEARNED label, Scents remember the last one and Relic Forest shortcuts
     * are gone. Their features stay, always on in normal play and off in GameCube-accurate mode (pc_options.c,
     * built_in 1); the accurate-mode "What it turns off" list still names them. Only Infinite TMs and the right stick
     * keep a switch here */
    {5, GROUP, "settings.tweaks", "settings.tweaks.sub", {}, 0, NONE},
    /* Smooth battle start's row is hidden while it is parked (snatcherLeo, 2026-11-20: "we can get rid of 'Smooth battle start'
     * since we are parking it"; research 6a, 1k): its option, code and default (off) stay in pc_options.c, and the row
     * comes back by putting this line back:
     * {5, TOGGLE, "settings.tweak.smooth_battle_start", "settings.tweak.smooth_battle_start.sub", {"smooth_battle_start"},
     *  0, LIVE_OPTION}, */
    /* step 3.7's infinite TMs (2026-11-20; pc_bag_tweaks.cpp): keeps its switch (snatcherLeo, 2026-11-21), UI id = its name key */
    {5, TOGGLE, "settings.tweak.infinite_tms", "settings.tweak.infinite_tms.sub", {"infinite_tms"}, 0, LIVE_OPTION},
    /* step 3.7's right-stick walking (2026-11-20; pc_cstick_walk.cpp): its own row and pc_options.c entry */
    {5, TOGGLE, "settings.tweak.cstick_walk", "settings.tweak.cstick_walk.sub", {"cstick_walk"}, 0, LIVE_OPTION},
    /* *Updated (snatcherLeo, after v7 worked: "IT WORKS! Just some text cleanup"):* the follow choice reads "Match current
     * Game speed"; Game speed is plain "Game speed" ("it's not really everything": the music can stay normal); Walking
     * speed removed ("we got enough enhancements for speed, we got the ones people care about") */
    {3, SLIDER, "settings.volume_master", "settings.volume_master.sub",
     {"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"}, 10, LIVE_VOL_MASTER},
    {3, SLIDER, "settings.volume_music", "settings.volume_music.sub",
     {"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"}, 10, LIVE_VOL_MUSIC},
    {3, SLIDER, "settings.volume_effects", "settings.volume_effects.sub",
     {"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"}, 10, LIVE_VOL_EFFECTS},
    /* the controller setup screen (pc_controls_ui.cpp; snatcherLeo, 2026-11-12: "Yes, build it") */
    {4, OPEN, "settings.controller", "settings.controller.sub", {"&#8250;"}, 0, NONE},
    /* Nintendo's GameCube adapter on a driver Windows can't read it with: one click sets it up (snatcherLeo, 2026-11-13:
     * "as long as they don't need Zadig, and it just converts it for them"; "Separate helper (1)"). Listed only while
     * it is needed or running (tab_rows); pc_adapter_setup.c */
    {4, OPEN, "settings.adapter", "settings.adapter.sub", {"&#8250;"}, 0, LIVE_ADAPTER},
    /* About, the last tab (snatcherLeo, 2026-11-23: "we should put it in About tab, last, just like how Studio has it ... keep
     * the download updates automatically and check for updates in there too, to match Studio's look (so move it from
     * General)"): the version both programs share with its status and one button (Check for updates / Download /
     * Restart now), the two update switches, Go back (only once an older version is kept), and the logs folder. The same
     * words as Studio's About (update.* keys). The version row's name, second line and button are filled in by state
     * (about_*, below). UI ids: settings.tab.about, update.version, update.auto, update.check_setting, update.go_back,
     * settings.logs */
    {6, ACTION, "update.version", "update.shared_version", {"update.check"}, 0, LIVE_UPD_VERSION},
    {6, TOGGLE, "update.auto", "update.auto_help", {}, 0, LIVE_UPD_AUTO},
    {6, TOGGLE, "update.check_setting", "update.check_help", {}, 1, LIVE_UPD_CHECK},
    {6, ACTION, "update.go_back", "update.go_back_help", {"update.go_back_button"}, 0, LIVE_UPD_BACK},
    /* What OrrePort adds (5.13, 2026-11-24; the approved public page, in the app): every change from the original game,
     * in the "What it turns off" window (pc_whatadds.cpp builds it from ui/adds.txt). UI id: adds.link */
    {6, OPEN, "adds.link", "adds.link.sub", {"&#8250;"}, 0, LIVE_ADDS},
    {6, ACTION, "settings.logs", "settings.logs.sub", {"settings.logs.open"}, 0, LIVE_LOGS},
};

/* ---- About's update rows (2026-11-23): the shared updater's state, in Studio's words ---- */
#ifdef ORRE_VERSION
const char* const kVersion = ORRE_VERSION;
#else
const char* const kVersion = "0.0.0";
#endif
/* Go back's "are you sure" pop-up (snatcherLeo, 2026-11-23: "Yes use it"; Studio's the same words): open, and the
 * controller's place in it (0 Cancel, where it starts; 1 Go back). It replaces the press-twice note */
bool s_gb = false;
int s_gbFocus = 0;

PcUpdateStatus about_status() {
    PcUpdateStatus st;
    pc_update_poll(&st);
    return st;
}

/* Studio open from this OrrePort folder (pc_apps.c). *Fixed 2026-11-23 (Studio's finding):* Go back used the poll's
 * other_running, which the updater fills in only while an update is READY, so with Studio open after a Go back the hub
 * would ask for it, exit, and its launcher would then skip the swap (app\ in use). Go back now asks pc_apps directly */
bool about_studio_open() {
    static char root[1300];
    if (root[0] == '\0') pc_paths_dir(PC_PLACE_ROOT, root, sizeof root);
    return pc_apps_running(PC_APP_STUDIO, root) != 0;
}
void about_close_studio() {
    char root[1300];
    pc_paths_dir(PC_PLACE_ROOT, root, sizeof root);
    std::printf("[settings_overlay] About: Close Studio (asked: %d)\n", pc_apps_close(PC_APP_STUDIO, root));
}

/* the version row's status line (under "The hub and Studio share one version.") */
std::string about_status_line(const PcUpdateStatus& st) {
    const std::string pct = std::to_string(st.size > 0 ? (int)(st.done * 100 / st.size) : 0);
    switch (st.state) {
    case PC_UPDATE_OFF: return tx("update.off");
    case PC_UPDATE_CHECKING: return tx("update.checking");
    case PC_UPDATE_UP_TO_DATE: return tx("update.up_to_date");
    case PC_UPDATE_AVAILABLE: return tx("update.available", {{"version", st.latest}});
    case PC_UPDATE_DOWNLOADING: return tx("update.downloading", {{"version", st.latest}, {"percent", pct}});
    case PC_UPDATE_READY:
        return tx("update.ready", {{"version", st.latest}}) +
               (st.other_running ? " " + tx("update.close_other", {{"program", "Studio"}}) : std::string());
    case PC_UPDATE_FAILED: return tx(st.error_key[0] ? st.error_key : "update.failed");
    default: /* never checked: a copy that can't update (a test run, or not the portable folder) */
        return pc_settings_int("update_check", 1) ? tx("update.unavailable") : tx("update.off");
    }
}

/* *Changed 2026-11-23 (snatcherLeo, agreed with Studio):* the version row's button is a small icon: a download arrow when an
 * update is available (or a failed download can be tried again), a circular arrow when one is ready (in the hub, with
 * nothing else from the folder open); faded otherwise (nothing to do; in game a ready update waits for the next start).
 * Returns whether it is active; *icon gets its picture */
bool about_button_on(const PcUpdateStatus& st, const char** icon);
bool about_in_hub(); /* s_in_hub, declared further down */

/* the version row's action, by state (the icon above; its words for the log) */
std::string about_button_key(const PcUpdateStatus& st) {
    if (st.state == PC_UPDATE_AVAILABLE) return "update.download";
    if (st.state == PC_UPDATE_FAILED && st.latest[0]) return "update.retry";
    if (st.state == PC_UPDATE_READY) return "update.restart";
    return "update.check";
}

/* a row's name and second line where they hold a value (the version, the version to go back to) */
std::string about_name(const RowDef& r) {
    if (r.live == LIVE_UPD_VERSION) return tx("update.version", {{"version", kVersion}});
    if (r.live == LIVE_UPD_BACK) return tx("update.go_back", {{"version", about_status().previous}});
    return tx(r.name);
}
/* "Version 0.1.1 is available" shows only while the button can be clicked (snatcherLeo: "It'll only say "Version XX is
 * availble" if that button is hoverable and lcickable as well"); the line is in the accent colour then, soft red for a
 * failure, faint otherwise. "Check now" under it (Studio's idea, agreed): the manual check, past the three-hour wait,
 * shown while checking is on and nothing is under way */
std::string about_version_sub() {
    const PcUpdateStatus st = about_status();
    const char* icon = "download";
    const bool on = about_button_on(st, &icon);
    std::string line = about_status_line(st);
    if (st.state == PC_UPDATE_READY && !on && !about_in_hub()) line = tx("update.note.ready"); /* applies at the next start */
    const char* cls = on ? "ust acc" : st.state == PC_UPDATE_FAILED ? "ust err" : "ust";
    /* *2026-11-24 (snatcherLeo, the laptop's update test: "The green indicator that also says "Restart to update" should also
     * be clickable for this"):* while the button is active, its green line is a second way to press it (id ustgo) */
    std::string out = tx("update.shared_version") + "<br/><span class='" + cls + (on ? " ustgo' id='ustgo'>" : "'>") +
                      line + "</span>";
    const bool idle = st.state == PC_UPDATE_UP_TO_DATE || st.state == PC_UPDATE_FAILED || st.state == PC_UPDATE_IDLE;
    if (idle && pc_settings_int("update_check", 1) != 0 && st.current[0] != '\0')
        out += " · <span class='checknow' id='checknow'>" + tx("update.check_now") + "</span>";
    return out;
}

bool about_button_on(const PcUpdateStatus& st, const char** icon) {
    if (st.state == PC_UPDATE_READY) {
        *icon = "restart";
        return about_in_hub() && !st.other_running;
    }
    *icon = "download";
    return st.state == PC_UPDATE_AVAILABLE || (st.state == PC_UPDATE_FAILED && st.latest[0] != '\0');
}

std::string about_sub(const RowDef& r) {
    if (r.live == LIVE_UPD_VERSION) return about_version_sub();
    if (r.live == LIVE_UPD_BACK) return tx("update.go_back_help", {{"version", about_status().previous}});
    return std::string();
}
bool about_row(const RowDef& r) { return r.live == LIVE_UPD_VERSION || r.live == LIVE_UPD_BACK; }

/* Restart now / Go back in the hub: OrrePort exits with the update code and its launcher swaps app\ (the hub's own panel
 * does the same, pc_hub.cpp upd_go). In game the swap waits for the next start (nothing is closed under the player) */
[[noreturn]] void about_exit_for_update() {
    pc_settings_save();
    pc_update_shutdown();
    std::fflush(stdout);
    std::fflush(stderr);
    pc_log_exit_clean("restarting for the update (Settings › About)");
    pc_update_pass_foreground(); /* the new copy comes back in front (pc_update.h) */
    _Exit(PC_UPDATE_EXIT_RESTART);
}
bool s_adapter_running = false; /* the setup program is running (the row stays, saying so; the busy screen shows) */
bool s_adapter_shown = false;   /* the row stays until Settings closes once it has shown its result */
bool s_adapter_claim = false;   /* set up: the adapter's controller becomes Player 1 when it appears (snatcherLeo, 2026-11-13) */
Uint64 s_adapter_started = 0;   /* when it started (the busy screen's moving bar) */
std::string s_note_big;         /* a note shown larger (the adapter's result) while s_note is still this text */

/* focus: a row of the current tab (0 .. n-1), then RESUME (n) and Quit to hub (n + 1) */
bool s_tried = false;
bool s_open = false;
/* *Changed 2026-11-15 (snatcherLeo: "should the expanded settings always be on General as a default?"):* the first page of a
 * run is General (it was Graphics, after an early mockup); after that, the page last looked at */
int s_tab = 0;
int s_focus = 0;
/* ORRE_SETTINGS_OPEN_AT (the screenshot hook) opens it without pausing, so a scripted run can't stop for good, unless
 * ORRE_SETTINGS_TEST_PAUSE=<presents> asks for a pause that ends by itself after that many presented frames */
bool s_test_no_pause = false;
long s_test_pause_left = -1;
Rml::ElementDocument* s_doc = nullptr;
input::LayerId s_layer = input::kInvalidLayerId;
std::string s_note;
/* a note that is a problem the player must fix (shown red, settings.rcss #live.err; snatcherLeo, 2026-11-13: "Make the font red
 * please"): red only while s_note is still this text, so any later note shows in the usual colour */
std::string s_note_err;

/* Opened from the hub's gear (snatcherLeo, 2026-11-11: "Just an extra window you can open up by clicking the gear button";
 * decided in-window, as a full page: "A, full page"): the same rows and controls, laid out as a page (settings.rcss
 * body.inhub), with BACK for RESUME, no "Quit to hub", no Report (the hub has its own Report a problem, and there is
 * no game picture before Play), no pause badge, and the hub's own screen scale left alone. Cleared on close.
 * *Corrected 2026-11-13 (snatcherLeo: "the app hub is glitching"):* cleared on close only once the game is running
 * (s_game_running). Before, closing Settings in the hub cleared it too, so the hub then counted as the game: with
 * Always show the menu bar on, the game's bar showed over the hub and the hub's picture was pushed down under the
 * game's strip (aurora_set_present_inset_top), cropping it; the log repeated "game picture below the menu bar's
 * strip" thousands of times as the hub's screen scale moved the strip's size every frame (verified from his log). */
bool s_in_hub = false;
bool about_in_hub() { return s_in_hub; }
void refresh(); /* below */
/* About's one button, by state (Studio's): Check for updates (now, past the three-hour wait), Download, Restart now (in
 * the hub; in game the update applies at the next start, nothing is closed under the player). The version row's icon
 * button and, since 2026-11-24, its green status line (id ustgo, about_version_sub) both press it. */
void about_button_press() {
    const PcUpdateStatus st = about_status();
    if (st.state == PC_UPDATE_READY) {
        if (st.other_running) s_note = s_note_err = tx("update.close_other", {{"program", "Studio"}});
        else if (s_in_hub) about_exit_for_update();
        else s_note = tx("update.note.ready");
    } else if (st.state == PC_UPDATE_AVAILABLE || (st.state == PC_UPDATE_FAILED && st.latest[0])) {
        pc_update_download();
    } else if (pc_settings_int("update_check", 1) != 0) {
        pc_update_check(2);
    }
    std::printf("[settings_overlay] About: %s\n", about_button_key(st).c_str());
    refresh();
}
/* the game has started (pc_settings_overlay_frame, the game's frames only; Quit to hub restarts OrrePort, so it never
 * goes back to false) */
bool s_game_running = false;
/* in game, General > In-game menu set to "Full window": the hub's page layout over the game (set as it opens) */
bool s_full = false;
/* the page layout (the hub's, or in game when chosen) rather than the side panel */
bool page() { return s_in_hub || s_full; }
/* the full Settings window opened from inside the game (not the hub's page): it has Quit to hub above BACK */
bool full_in_game() { return s_full && !s_in_hub; }

/* the rows' values as the table above gives them, before the saved settings are read: X resets a row to this */
std::vector<int> s_defaults;
/* the Language row's choices after "Follow Windows": each language file's code ("en", "de"), in the row's order */
std::vector<std::string> s_lang_codes;

/* ---- the in-game panel (snatcherLeo, 2026-11-11, option B: "B, mock it up", "Yes, build it"; mockups 5-7) ----
 * Level 0, the first screen: Master volume, Music and Window as shortcuts, the six categories, Report a problem and
 * Quit to hub. Level 1: one category's rows, slid into the same panel (B goes back to level 0). The hub's full page
 * is always "level 1" (its categories are the column on the left). s_focus indexes items(). */
enum ItemKind { IT_ROW, IT_CAT, IT_FULL, IT_RESUME, IT_NOASK, IT_REPORT, IT_QUIT };
/* Quit to hub's question (snatcherLeo, 2026-11-13: it "asks confirmation, but you can click a checkbox "Don't show this
 * again""): while asked, Report / Quit become Cancel / Quit to hub, with the box above them */
bool s_quit_ask = false;
/* Quit to hub in the full Settings window in game (snatcherLeo, 2026-11-15: "should the full settings menu have "Quit to Hub"
 * somewhere in there, ONLY if you opened it while the game was opened? I think so"; mockup A, above BACK, and R2, its
 * question as a dialog over the dimmed window): s_colQuit = the controller is on the column's QUIT TO HUB (Down from
 * the last category, Up from the first); s_fq = the dialog is open, s_fqFocus its focus (0 Cancel, where it starts;
 * 1 Quit to hub; 2 Don't ask again), s_fqNoAsk its box */
bool s_colQuit = false;
/* s_colBack: the controller is on the column's BACK, after QUIT TO HUB in game (or after the last category on the hub's
 * page). *Added 2026-11-15 (snatcherLeo: "if I scroll down from "QUIT TO HUB" I cannot go to "BACK""):* A on it closes Settings */
bool s_colBack = false;
bool s_fq = false;
int s_fqFocus = 0;
bool s_fqNoAsk = false;
/* General › "What it turns off" (2.12): GameCube-accurate mode's list, a window over the page (#whatdlg); Up / Down
 * scroll it, A, B or a click outside close it. The same list as the hub's Saves page (hub_accurate_list_rml) */
bool s_what = false;
bool s_what_adds = false; /* the window shows "What OrrePort adds" (About, 5.13) instead of "What it turns off" */
bool s_quit_noask = false;
Uint64 s_quit_fade_since = 0; /* Quit to hub's fade-out: when it began; 0 none */
/* the menu bar in game (snatcherLeo, 2026-11-13, mockup 46: "I like the mockup, very good, just don't have logo and shift File
 * and Help to the left"; both fullscreen keys named, "Can we say that in the File menu, both F11 and Alt+enter?"):
 * File (Fullscreen, Open OrrePort folder, Settings..., Quit to hub) and Help (Report a problem..., About OrrePort), in
 * RmlUi over the game, replacing the developer toolkit's bar (pc_menu_bar.cpp). Mouse only. */
Uint64 s_cover_since = 0;  /* a fullscreen switch's dip to black: when it began; 0 none */
Uint64 s_cover_switched = 0; /* ... when the switch itself was made (the screen black); 0 not yet */
bool s_bar = false;        /* shown */
int s_bar_menu = -1;       /* the open menu: 0 File, 1 Cheats, 2 Help; -1 none */
int s_bar_sub = -1;        /* Cheats' open side list: 0 EXP, 1 Put in the PC; -1 none (mockup 63 v2) */
enum { BAR_FILE, BAR_CHEATS, BAR_HELP, BAR_MENUS };
bool s_about = false;      /* About OrrePort shown */
void bar_rml();
void request_quit_to_hub();
bool s_fade_only = false;     /* ... started with the menu closed (the shortcut): only the fade shows */
struct Item {
    ItemKind kind;
    int index; /* IT_ROW: the s_rows index; IT_CAT: the category */
};
int s_level = 0;
/* the page's two levels (snatcherLeo, 2026-11-13: "the selection should just be on "Graphics", and, if I click whatever the A
 * button is, that gets me into the sub-menu. If I click "B" to go back, that gets me out of a sub-menu"): true while
 * the controller is on the category column (the page opens there); A or Right goes into the category's rows, B back */
bool s_cats = true;
int s_drop = -1;       /* an open drop-down: its highlighted option; -1: none open */
int s_drag = -1;        /* a slider being dragged with the mouse: its item; -1: none */
float s_drag_left = 0, s_drag_width = 0; /* that slider's track, taken as the drag began (the rows are rebuilt) */
int s_hold_dir = 0;     /* a held direction (D-pad or stick): 1 up, 2 down, 3 left, 4 right; 0 none */
Uint64 s_hold_next = 0; /* when it repeats next */
/* the categories' names: text keys (ui/lang/en.txt), shown with tx() */
const char* const kTabNames[TAB_COUNT] = {"settings.tab.general",  "settings.tab.appearance", "settings.tab.graphics",
                                 "settings.tab.audio",    "settings.tab.controls",   "settings.tab.enhancements",
                                 "settings.tab.about"};

/* a row's option or a text: a "settings." / "common." key becomes the player's words; anything else (a number, a
 * product name, an arrow) stays as written */
std::string word(const std::string& s) {
    return s.rfind("settings.", 0) == 0 || s.rfind("common.", 0) == 0 ? tx(s.c_str()) : s;
}

/* ---- Graphics › Frame rate (step 3.4, 2026-11-18) ---- */

/* The monitor the window is on: its refresh rate now (Windows' current mode), and the fastest its modes offer at the
 * same size (SDL's list of fullscreen modes), both rounded (59.94 reads 60). Cached, read again at most once a second
 * or when the window is on another monitor (frame_rate_frame below redraws the page then). 0: unknown.
 * Platforms: SDL's display calls exist on all three. The fastest mode is read on Windows only for now (snatcherLeo,
 * 2026-11-16: "this would be a Windows thing (for now)"): Windows often leaves a 144 Hz monitor at 60, and the note
 * points to Windows' own display settings; Linux and macOS get their own wording later (likely the same SDL call; how
 * reliably X11, Wayland and macOS list the modes is unchecked) */
struct DisplayRates {
    int now = 0;  /* the current mode's rate, Hz */
    int best = 0; /* the fastest mode at the current size (Windows only; 0 elsewhere) */
};
SDL_DisplayID s_rates_display = 0;
DisplayRates display_rates() {
    static DisplayRates cached;
    static Uint64 read_at = 0;
    SDL_Window* window = aurora_get_window();
    const SDL_DisplayID id = window != nullptr ? SDL_GetDisplayForWindow(window) : 0;
    const Uint64 now = SDL_GetTicks();
    if (read_at != 0 && now - read_at < 1000 && id == s_rates_display) return cached;
    read_at = now;
    s_rates_display = id;
    cached = DisplayRates{};
    if (id == 0) return cached;
    if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(id)) {
        cached.now = (int)(m->refresh_rate + 0.5f);
    }
#ifdef _WIN32
    const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(id);
    int count = 0;
    if (SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(id, &count)) {
        /* the same size in pixels (a mode's w and h are in points; times its pixel density gives pixels): a monitor
         * that reaches 144 Hz only at a lower resolution isn't "held back" at its own */
        auto px = [](const SDL_DisplayMode* d, int v) { return (int)(v * d->pixel_density + 0.5f); };
        for (int i = 0; i < count; ++i) {
            if (desk != nullptr && (px(modes[i], modes[i]->w) != px(desk, desk->w) ||
                                    px(modes[i], modes[i]->h) != px(desk, desk->h))) {
                continue;
            }
            cached.best = SDL_max(cached.best, (int)(modes[i]->refresh_rate + 0.5f));
        }
        SDL_free(modes);
    }
#endif
    return cached;
}

/* the row's choices as shown: Off (original), Match with the monitor's rate in it ("Match (120 Hz)"), then "120 FPS" */
std::string frame_rate_option(const RowDef& r, int j) {
    if (j == 1) {
        const int hz = display_rates().now;
        return hz > 0 ? tx("settings.frame_rate.match_hz", {{"hz", std::to_string(hz)}}) : tx("settings.frame_rate.match");
    }
    if (j >= 2 && j < (int)r.options.size()) {
        return tx("settings.frame_rate.cap", {{"fps", r.options[j]}});
    }
    return word(r.options[j]);
}

/* the row's choice in settings.ini's words (pc_smooth.h: off, match, or the cap's number) and back */
std::string frame_rate_code(const RowDef& r) {
    return r.value <= 0 ? "off" : r.value == 1 ? "match" : r.options[r.value];
}
int frame_rate_row(const RowDef& r, const std::string& code) {
    if (code == "off") return 0;
    for (int j = 2; j < (int)r.options.size(); ++j) {
        if (r.options[j] == code) return j;
    }
    return 1; /* "match", and anything unknown: the default */
}

/* the row's second line (and its help): what the choice will do on this monitor.
 *   - On Windows, a monitor that could run faster than Windows has it set (a 144 Hz monitor left at 60): says so and
 *     points to Windows' display settings; OrrePort never changes the system setting (snatcherLeo, 2026-11-16).
 *   - A cap above the monitor's rate: it runs at the monitor's rate, like Match (pc_smooth.c live_target).
 *   - Otherwise the row's own line. The help box adds the one-frame delay, for players who time inputs to the frame */
std::string frame_rate_sub(const RowDef& r, bool for_help) {
    const DisplayRates d = display_rates();
    const int cap = r.value >= 2 ? SDL_atoi(r.options[r.value].c_str()) : 0;
    std::string line;
    if (r.value != 0 && d.now > 0 && d.best > d.now) {
        line = tx("settings.frame_rate.held_back", {{"hz", std::to_string(d.now)}, {"best", std::to_string(d.best)}});
    } else if (cap > 0 && d.now > 0 && cap > d.now) {
        line = tx("settings.frame_rate.above", {{"hz", std::to_string(d.now)}});
    } else {
        line = tx("settings.frame_rate.sub");
    }
    /* the help's pattern puts the line in ({line}), so each language spaces its sentences its own way */
    return for_help ? tx("settings.frame_rate.help", {{"line", line}}) : line;
}

std::vector<int> tab_rows() {
    std::vector<int> out;
    for (int i = 0; i < (int)s_rows.size(); ++i) {
        if (s_rows[i].live == LIVE_GAMEICON && pc_window_icon_game_available() == 0) {
            continue; /* no disc with the save icon: no Window icon row */
        }
        if (s_rows[i].live == LIVE_UPD_BACK && about_status().previous[0] == '\0') {
            continue; /* About's Go back: only once an older version is kept (as in Studio) */
        }
        /* Debug tools: only once turned on by hand (settings.ini debug_tools = 1), so it can be turned off here again;
         * a player who never asked for it never sees it (pc_debug.c) */
        if (s_rows[i].live == LIVE_DEBUG && pc_settings_int("debug_tools", 0) == 0 && pc_debug_enabled() == 0) {
            continue;
        }
        /* the GameCube adapter row: only while an adapter needs the setup, while it runs, and until Settings closes
         * after it (so its result can be read) */
        if (s_rows[i].live == LIVE_ADAPTER && !pc_adapter_needs_setup() && !s_adapter_running && !s_adapter_shown) {
            continue;
        }
        /* Report a problem is a button now (the panel's first screen; the hub has its own); a group's label is drawn
         * above its first row (refresh), never a stop */
        /* About's action rows (the version's button, Go back, the logs folder; 2026-11-23) are rows of their tab: the
         * rule above was for Report a problem only, and dropped them (snatcherLeo saw an About with only the two switches) */
        const bool about_action = s_rows[i].live == LIVE_UPD_VERSION || s_rows[i].live == LIVE_UPD_BACK ||
                                  s_rows[i].live == LIVE_LOGS;
        if (s_rows[i].tab == s_tab && (s_rows[i].kind != ACTION || about_action) && s_rows[i].kind != GROUP) {
            out.push_back(i);
        }
    }
    return out;
}

/* the group a row sits in (the nearest GROUP row above it in the same category), or -1 */
int group_of(int index) {
    for (int i = index - 1; i >= 0 && s_rows[i].tab == s_rows[index].tab; --i) {
        if (s_rows[i].kind == GROUP) return i;
        if (s_rows[i].kind == INFO) return -1; /* a heading (Speed, Tweaks) ends the search */
    }
    return -1;
}

/* The Renderer row (2026-11-24): its values 0 Vulkan, 1 Direct3D 12, 2 Direct3D 11, saved as settings.ini gpu_backend
 * ("auto" or missing is Vulkan, pc_platform.c), and the value OrrePort started with, so the row can say a change
 * waits for the next start and offer "Restart now" in the hub. Why (snatcherLeo, the laptop sitting): he switched the
 * renderer and tested in the same run, so the "Direct3D 12" test ran on Vulkan without anything saying so. */
int s_backend_at_start = -1;
int backend_value(const std::string& code) { return code == "d3d12" ? 1 : code == "d3d11" ? 2 : 0; }
const char* backend_code(int value) { return value == 1 ? "d3d12" : value == 2 ? "d3d11" : "vulkan"; }
const char* backend_name(int value) { return value == 1 ? "Direct3D 12" : value == 2 ? "Direct3D 11" : "Vulkan"; }

/* "Restart now" after a renderer change (the hub only: in game nothing is closed under the player). OrrePort exits
 * with the launcher's "start again" code, as the US disc's Replace does (pc_hub_langdiscs.cpp); the launcher starts it
 * again, which reads the new gpu_backend. Windows' launcher today; Linux and macOS would restart from their launch
 * scripts the same way (likely). */
[[noreturn]] void restart_for_renderer() {
    std::printf("[settings_overlay] Renderer: restarting for %s\n", pc_settings_string("gpu_backend", "auto"));
    pc_settings_save();
    pc_update_shutdown();
    std::fflush(stdout);
    std::fflush(stderr);
    pc_log_exit_clean("restarting for the new renderer (Settings › Graphics)");
    pc_update_pass_foreground(); /* the new copy comes back in front (pc_update.h) */
    _Exit(PC_UPDATE_EXIT_RESTART);
}

/* a row's description: its own second line; a row without one (the Fast-forward row, whose group says it) gives its
 * group's line in the side menu's help box (for_help), and no second line on the page */
std::string row_sub(int index, bool for_help) {
    const RowDef& r = s_rows[index];
    if (r.locked && r.lock_reason != nullptr) return tx(r.lock_reason);
    if (r.live == LIVE_BACKEND && s_backend_at_start >= 0 && r.value != s_backend_at_start) {
        /* a change waiting for the next start; in the hub a "Restart now" link (id restartnow, the click handler) */
        std::string out = tx(r.sub) + "<br/>" + tx("settings.renderer.pending", {{"renderer", backend_name(r.value)}});
        if (s_in_hub && !for_help) out += " · <span class='checknow' id='restartnow'>" + tx("update.restart") + "</span>";
        return out;
    }
    if (r.live == LIVE_FRAME_RATE) return frame_rate_sub(r, for_help); /* the monitor's notes (step 3.4) */
    if (about_row(r)) return about_sub(r); /* About: the version's status, the version to go back to */
    if (r.sub != nullptr) return tx(r.sub);
    const int g = for_help ? group_of(index) : -1;
    return g >= 0 && s_rows[g].sub != nullptr ? tx(s_rows[g].sub) : std::string();
}

std::vector<Item> items() {
    std::vector<Item> out;
    if (!page() && s_level == 0) {
        /* the quick row: Volume and Window (snatcherLeo, 2026-11-13, "Volume + Window": music moved back to Audio only) */
        for (Live l : {LIVE_VOL_MASTER, LIVE_WINDOW}) {
            for (int i = 0; i < (int)s_rows.size(); ++i) {
                if (s_rows[i].live == l) {
                    out.push_back({IT_ROW, i});
                }
            }
        }
        for (int p = 0; p < TAB_COUNT; ++p) { /* in the shown order (kTabOrder) */
            out.push_back({IT_CAT, tab_at(p)});
        }
        /* a big way back to the game (snatcherLeo, 2026-11-12: "we need a big button for "Get back to the game" ... a
         * clickable button/pressable button to quickly get back to the game"), then Report and Quit */
        /* the full window from here, for this time (snatcherLeo, 2026-11-13: "we should have a big button to get to "Bigger
         * Settings Screen" or something, just to make it easier to get there") */
        if (s_quit_ask) { /* the question takes the place of the two big buttons, so nothing above it moves */
            out.push_back({IT_NOASK, 0});
        } else {
            out.push_back({IT_FULL, 0});
            out.push_back({IT_RESUME, 0});
        }
        out.push_back({IT_REPORT, 0});
        out.push_back({IT_QUIT, 0});
        return out;
    }
    for (int i : tab_rows()) {
        out.push_back({IT_ROW, i});
    }
    return out;
}

std::string ui_path(const char* file) { return pc_paths_str(PC_PLACE_UI, file); /* app\UI\ (flat: ui\), 4.1 */ }
} // namespace

/* The fonts for every RmlUi screen (the hub and this overlay), loaded once; 1 when the mockups' fonts loaded.
 * The mockups' fonts (snatcherLeo, 2026-11-09: "Yes for the fonts"): Inter for text, Saira for the headings, both SIL Open
 * Font License, variable fonts (RmlUi loads each named weight inside as its own face), in Fonts/ next to the exe
 * (ui/fonts/ until 2026-11-16) with their licence files; the shipped Noto Sans stands in when they are missing (Windows'
 * Segoe UI until 2026-11-16). */
extern "C" int pc_ui_load_fonts(void) {
    static int loaded = -1;
    if (loaded >= 0) {
        return loaded;
    }
    /* the pages' "[key]" texts in the player's language (pc_text.h), hooked once before the first page loads; here
     * because the hub and the Settings panel both come through this first */
    pc_text_hook_pages();
    /* *Moved 2026-11-16* from ui/fonts/ to Fonts/ next to the exe (app\Fonts\ in the portable layout; the decision
     * "Fonts in the portable layout"), one folder for every shipped font */
    char dir[1024];
    const std::string fonts = pc_font_dir(dir, sizeof dir);
    loaded = Rml::LoadFontFace(fonts + "Inter-Variable.ttf") && Rml::LoadFontFace(fonts + "Saira-Variable.ttf");
    /* The fallback faces: Google's Noto set, shipped in the same folder on every OS, the same chain as the game's part D
     * (pc_font_files.cpp: the files, the order and why). *Replaced 2026-11-16 (the Noto wiring; decided 2026-11-15,
     * snatcherLeo: "Yes with emoji"):* these were the player's own Windows fonts (Segoe UI when Inter was missing, Segoe UI
     * Symbol for ▼ ⧉ …, and Yu Gothic or Meiryo for Japanese, added 2026-11-15 for the IME test), so Linux and macOS
     * had none and a name in kanji drew as boxes there. Now: Noto Sans (also the stand-in for Inter / Saira if those are
     * missing), Noto Sans CJK with the one region face the language or the system asks for (RmlUi takes one CJK face:
     * the first fallback that has a character wins, so two faces would only ever use the first), the symbols, the other
     * scripts (Arabic and Hebrew unjoined until the right-to-left work) and Noto Color Emoji in colour (RmlUi draws a
     * font's colour pictures as they are, FreeType reading their PNGs). RmlUi reads each file whole when it loads it:
     * about 53 MB in all, once (the CJK collection 20 MB, the emoji 11 MB, one font for each of about 76 other
     * scripts 17 MB, Noto Sans and the symbols 4 MB; *widened 2026-11-16*, snatcherLeo: "Let's add all the missing noto faces
     * now, which can cover all languages"). RmlUi has no shaping either: complex scripts draw letter by letter, Arabic
     * and Hebrew unjoined and left to right, until the right-to-left / shaping work. The same on every OS.
     * *Updated 2026-11-16:* shaping is in. These calls now load into RmlUi's HarfBuzz font engine (hub_text/
     * pc_ui_shaping.h), which picks a face per run from this same list in this order and shapes and orders each run. */
    int fallbacks = 0;
    for (int i = 0; i < pc_noto_count(); ++i) {
        const PcNotoFont* f = pc_noto_font(i);
        const int index = f->index < 0 ? pc_noto_cjk_default(pc_text_language()) : f->index;
        /* a variable font at the chain's weight (500, Medium), else its Regular. Why (found 2026-11-16 in the Saves
         * page's picture: Thai, Arabic, Hebrew, Bengali and Khmer drew hairline-thin and grey): with "Auto" RmlUi loads
         * every named weight of a variable font as its own fallback face, lightest first, and a missing letter takes
         * the first fallback face that has it: the Thin one. Static fonts have one weight and load as they are. */
        const std::string path = fonts + f->file;
        bool ok = false;
        if (f->wght > 0) {
            ok = Rml::LoadFontFace(path, true, (Rml::Style::FontWeight)(int)f->wght, index) ||
                 Rml::LoadFontFace(path, true, Rml::Style::FontWeight::Normal, index);
        } else {
            ok = Rml::LoadFontFace(path, true, Rml::Style::FontWeight::Auto, index);
        }
        if (ok) {
            ++fallbacks;
        } else {
            std::printf("[ui] fallback font %s not found in %s (run port/tools/firstlink.py)\n", f->what, fonts.c_str());
        }
    }
    std::printf("[ui] %d of %d Noto fallback fonts loaded for OrrePort's pages (CJK region face %d)\n", fallbacks,
                pc_noto_count(), pc_noto_cjk_default(pc_text_language()));
    return loaded;
}

namespace {

RowDef* live_row(Live live) {
    for (auto& r : s_rows) {
        if (r.live == live) {
            return &r;
        }
    }
    return nullptr;
}

/* Settings > Audio's three sliders (0-10 steps of 10) to the sound output, at once */
void apply_volumes() {
    const RowDef* master = live_row(LIVE_VOL_MASTER);
    const RowDef* music = live_row(LIVE_VOL_MUSIC);
    const RowDef* effects = live_row(LIVE_VOL_EFFECTS);
    if (master != nullptr && music != nullptr && effects != nullptr && !pc_platform_automated_run() &&
        pc_audio_set_volumes != nullptr) {
        pc_audio_set_volumes(master->value * 10, music->value * 10, effects->value * 10);
    }
}

void sync_live_values() {
    if (RowDef* r = live_row(LIVE_WINDOW)) {
        r->value = pc_platform_is_fullscreen() ? 1 : 0;
    }
}

/* what a select's box shows: its option, or for the internal resolution a short form ("3× · for 1080p", "Native ·
 * 480p", "5× · 3200 × 2400"); the open list shows every option in full (mockup 45) */
std::string select_box_text(const RowDef& r) {
    if (r.live == LIVE_FRAME_RATE) return frame_rate_option(r, r.value); /* "Match your display (120 Hz)", "120 FPS" */
    const std::string o = word(r.options[r.value]);
    if (r.live != LIVE_RES || r.value == 0) return o;
    const size_t a = o.find(" · "), b = o.rfind(" · ");
    if (a == std::string::npos || a == b) return o;  /* "5× · 3200 × 2400": in full */
    return o.substr(0, a) + o.substr(b);             /* the first and last parts */
}

/* a switch (settings.rcss .tog; mockup item 2, snatcherLeo's polish list 2026-11-16): the knob holds a check mark shown when on;
 * acc: GameCube-accurate mode's own switch, amber when on (every other switch keeps the sage "on", snatcherLeo's decision);
 * ring: the controller's ring round it, where the switch itself is what the controller is on (the accurate card).
 * All platforms (RmlUi) */
std::string switch_rml(bool on, bool acc, bool ring, const char* id = nullptr) {
    /* *Changed 2026-11-23 (snatcherLeo: "these sliders ... are they supposed to look jagged? Any way to make them look
     * smoother?"):* RmlUi draws rounded borders with no smoothing, so the track, its edge and the knob are smooth SVG
     * pictures now (icons/tog-*.svg, tinted per state in settings.rcss); the shape and sizes are unchanged */
    return std::string("<div class='tog") + (on ? " on" : "") + (acc ? " acc" : "") + "'" +
           (id != nullptr ? std::string(" id='") + id + "'" : std::string()) + ">" +
           "<img class='tf' src='menuicon://tog-fill'/><img class='te' src='menuicon://tog-edge'/>" +
           (ring ? "<span class='tring'></span>" : "") +
           "<div class='knob'><img class='kf' src='menuicon://tog-knob'/><span class='ck'>&#10003;</span></div></div>";
}

std::string control_rml(const RowDef& r) {
    switch (r.kind) {
    case SELECT:
        /* "Recommended" in the box on the full page (the side menu's box is too narrow: its open list shows it) */
        /* the words in their own box, cut before the caret (settings.rcss .sel .st; 2026-11-16: in pseudo, "Same as Game
         * speed" ran under the caret), then the caret floating right as before */
        return "<div class='sel'><span class='st'>" + select_box_text(r) +
               /* not in Frame rate's box: "Match your display (120 Hz)" fills it and the pill was cut (2026-11-18
                * picture); its open list still shows it */
               (page() && r.value == r.rec && r.live != LIVE_FRAME_RATE
                    ? "<span class='rec'>" + tx("settings.recommended") + "</span>"
                                           : std::string()) +
               "</span><span class='caret'>▼</span></div>";
    case GROUP:
        return "";
    case TOGGLE:
        return switch_rml(r.value != 0, r.live == LIVE_ACC_DEFAULT, false);
    case SLIDER: {
        const int pct = (int)r.options.size() > 1 ? r.value * 100 / ((int)r.options.size() - 1) : 0;
        /* the thumb is the switches' smooth circle (icons/tog-knob.svg, tinted; 2026-11-23, snatcherLeo: "ensure that smoothing
         * is there for the audio sliders"): RmlUi drew the round thumb with jagged edges */
        return "<div class='sl'><span class='track'><span class='fillbar' style='width: " + std::to_string(pct) +
               "%;'></span><span class='thumb' style='left: " + std::to_string(pct) +
               "%;'><img class='tk' src='menuicon://tog-knob'/></span></span><span class='val'>" + r.options[r.value] +
               "</span></div>";
    }
    case INFO:
    case DISABLED:
    case OPEN:
        return "<div class='infotext'>" + word(r.options[0]) + "</div>";
    case ACTION:
        if (r.live == LIVE_UPD_VERSION || r.live == LIVE_LOGS) { /* About's small icon buttons (about_icon) */
            const char* icon = "folder";
            const bool on = r.live == LIVE_LOGS || about_button_on(about_status(), &icon);
            return std::string("<div class='ibtn") + (on ? "" : " off") + "'><img src='menuicon://" + icon + "'/></div>";
        }
        return "<div class='sel'>" + (r.live == LIVE_UPD_BACK ? tx("update.go_back_button") : word(r.options[0])) +
               "<span class='caret'>›</span></div>";
    }
    return "";
}

/* ---- Report a problem (mockup 08) ---- */
bool s_report = false;      /* the report panel shows instead of the Settings panel */
/* the report was opened on its own (F8, Help > Report a problem) rather than from the menu: closing it then closes the
 * menu too, back to the game (snatcherLeo, 2026-11-13: "if I click F8, and then cancel out, the settings menu should not pop
 * out") */
bool s_report_alone = false;
int s_report_save = 0;      /* frames waited for the screenshot before saving (0: no save pending) */
bool s_report_done = false; /* the report panel shows its saved screen (Studio's flow, 2026-11-23) */
std::string s_report_zip;   /* ...for that zip (Open folder) */
std::string rml_escape(const std::string& s); /* below */
std::string s_shot_file;    /* the report's screenshot, taken as the panel opens */
int s_focus_note = 0;       /* frames until the text box gets the keyboard focus (0: nothing pending) */

/* light or dark now (Appearance > Theme, or Windows' own when it follows Windows) */
bool theme_is_dark() {
    const RowDef* theme = live_row(LIVE_THEME);
    const int t = theme != nullptr ? theme->value : 0;
    return t == 2 || (t == 0 && SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT);
}

/* the window's title bar in the theme's colours, set again only when light / dark changes */
void apply_title_bar() {
    static int applied = -1;
    const int dark = theme_is_dark() ? 1 : 0;
    if (dark != applied) {
        applied = dark;
        pc_window_title_bar_style(dark);
    }
}

/* ---- v2's pieces (snatcherLeo, 2026-11-12: "Yes, build v2"; mockups 8-11) ---- */

/* the controller in the player's hands, from the last button or key pressed: the footer shows its own buttons
 * (a PlayStation pad's symbols are pictures, ui/icons/menu-ps-*.svg, never missing font glyphs) */
enum Ctrl { CTRL_KEYBOARD, CTRL_GAMECUBE, CTRL_XBOX, CTRL_PLAYSTATION, CTRL_DECK };
Ctrl s_ctrl = CTRL_KEYBOARD;

Ctrl ctrl_of_gamepad(SDL_JoystickID id) {
    const char* name = id != 0 ? SDL_GetGamepadNameForID(id) : nullptr;
    if (name != nullptr && SDL_strcasestr(name, "gamecube") != nullptr) {
        return CTRL_GAMECUBE;
    }
    if (name != nullptr && SDL_strcasestr(name, "steam deck") != nullptr) {
        return CTRL_DECK;
    }
    switch (SDL_GetGamepadTypeForID(id)) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5: return CTRL_PLAYSTATION;
    default: return CTRL_XBOX;
    }
}

/* one button for the footer: a coloured round key with a letter or a symbol picture (sq: a rectangular key) */
std::string key_rml(const char* label, const char* bg, const char* fg = "#FFFFFF", bool sq = false) {
    std::string inner = label;
    if (SDL_strncmp(label, "ps-", 3) == 0) {
        inner = std::string("<img class='kimg' src='menuicon://") + label + "'/>";
    }
    return std::string("<span class='k") + (sq ? " sq" : "") + "' style='background-color: " + bg + "; color: " + fg +
           ";'>" + inner + "</span>";
}

/* the footer: Back, Choose, Default (on a setting) and Close, as the controller in hand names them */
std::string hint_bar(bool on_row) {
    const bool drop = s_drop >= 0;
    std::string back, choose, deflt, close;
    switch (s_ctrl) {
    case CTRL_GAMECUBE:
        back = key_rml("B", "#D3392F");
        choose = key_rml("A", "#1FA35A");
        deflt = key_rml("X", "#9AA39D", "#1B1F1D");
        close = key_rml(pc_controls_menu_name(0), "#3A423E", "#E9EDEA", true); /* Z + L by default (snatcherLeo) */
        break;
    case CTRL_PLAYSTATION:
        back = key_rml("ps-circle", "#C8382E");
        choose = key_rml("ps-cross", "#2F6BD2");
        deflt = key_rml("ps-square", "#C9469B");
        close = key_rml(pc_controls_menu_name(0), "#3A423E", "#E9EDEA", true); /* the shortcut chosen in Controls */
        break;
    case CTRL_XBOX:
    case CTRL_DECK:
        back = key_rml("B", s_ctrl == CTRL_DECK ? "#4A524E" : "#C8382E");
        choose = key_rml("A", s_ctrl == CTRL_DECK ? "#4A524E" : "#3C9A3C");
        deflt = key_rml("X", s_ctrl == CTRL_DECK ? "#4A524E" : "#2F6BD2");
        /* the shortcut chosen in Controls (snatcherLeo, 2026-11-13: the start hint said View, the menu said Menu; "Menu" was
         * written here by hand, an Xbox START, not the shortcut) */
        close = key_rml(pc_controls_menu_name(0), "#3A423E", "#E9EDEA", true);
        break;
    case CTRL_KEYBOARD:
        /* keyboard keys as the player's keyboard prints them (German "Entf" for Delete): text keys too */
        back = key_rml(pc_text("settings.key.backspace"), "#3A423E", "#E9EDEA", true);
        choose = key_rml(pc_text("settings.key.enter"), "#3A423E", "#E9EDEA", true);
        deflt = key_rml(pc_text("settings.key.delete"), "#3A423E", "#E9EDEA", true);
        close = key_rml(pc_controls_menu_name(1), "#3A423E", "#E9EDEA", true); /* the key chosen in Controls */
        break;
    }
    std::string h = "<span class='hi'>" + back + tx(drop ? "common.cancel" : "common.back") + "</span>";
    h += "<span class='hi'>" + choose + tx(drop ? "settings.hint.pick" : "settings.hint.choose") + "</span>";
    if (!drop && on_row) {
        h += "<span class='hi'>" + deflt + tx("settings.hint.default") + "</span>";
    }
    h += "<span class='rt'>" + close + tx("settings.hint.close") + "</span>";
    return h;
}

/* the Fast-forward row's green link (Settings › Enhancements › Speed; snatcherLeo, 2026-11-16, mockup s7_links): the button
 * for the device in hand as a key cap and "Change ›" ("[LB] · Change ›" on Xbox-style pads, "[L1]" on PlayStation,
 * "[Tab]" on the keyboard), or "Not set · Set ›" when that device has none (a GameCube pad by default). Its own stop
 * (item-<item>): A or a click opens Controls on the Fast-forward row; Right from the box reaches it. UI id
 * settings.speed_ff.link. All platforms (RmlUi) */
std::string ff_link_rml(bool focused, int item) {
    const bool kb = s_ctrl == CTRL_KEYBOARD;
    const std::string name = pc_ff_name(kb ? 1 : 0);
    const std::string what = name.empty() ? tx("settings.speed_ff.not_set") : "<span class='kc'>" + name + "</span>";
    return std::string("<span class='dlink") + (focused ? " f" : "") + "' id='item-" + std::to_string(item) + "'>" + what +
           "<span class='dot'>·</span>" + tx(name.empty() ? "settings.speed_ff.set" : "settings.speed_ff.change") +
           "<span class='chev'>&#8250;</span></span>";
}

/* a category's one-line summary of what is set (the first screen) */
std::string summary(int tab) {
    auto val = [](Live l) -> std::string {
        const RowDef* r = live_row(l);
        return r != nullptr && r->value >= 0 && r->value < (int)r->options.size() ? select_box_text(*r) : "";
    };
    auto pct = [](Live l) {
        const RowDef* r = live_row(l);
        return std::to_string(r != nullptr ? r->value * 10 : 100);
    };
    /* each summary is one sentence pattern per language (ui/lang "settings.summary.*"), with the values put in */
    switch (tab) {
    case 0: {
        const RowDef* pause = live_row(LIVE_PAUSE);
        return tx("settings.summary.general",
                  {{"window", val(LIVE_WINDOW)},
                   {"pause", tx(pause != nullptr && pause->value ? "settings.summary.pauses" : "settings.summary.runs")}});
    }
    case 1: {
        if (pc_window_icon_game_available() != 0) {
            return tx("settings.summary.appearance_icon",
                      {{"theme", val(LIVE_THEME)}, {"icon", val(LIVE_GAMEICON)}, {"background", val(LIVE_MENUBG)}});
        }
        return tx("settings.summary.appearance", {{"theme", val(LIVE_THEME)}, {"background", val(LIVE_MENUBG)}});
    }
    case 2:
        return tx("settings.summary.graphics",
                  {{"renderer", val(LIVE_BACKEND)}, {"resolution", val(LIVE_RES)}, {"filtering", val(LIVE_ANISO)}});
    case 3:
        return tx("settings.summary.audio",
                  {{"master", pct(LIVE_VOL_MASTER)}, {"music", pct(LIVE_VOL_MUSIC)}, {"effects", pct(LIVE_VOL_EFFECTS)}});
    case 4: return tx("settings.summary.controls", {{"button", pc_controls_menu_name(s_ctrl == CTRL_KEYBOARD)}});
    case 6: return tx("update.version", {{"version", kVersion}}); /* About: the version both programs share */
    /* *Updated 2026-11-13:* the speed setting, like the other categories' values (was "Coming after the reference playthrough"; snatcherLeo: "Get
     * rid of" it) */
    default: return tx("settings.summary.enhancements", {{"speed", val(LIVE_SPEED_GAME)}});
    }
}

/* one per tab id (TAB_COUNT entries: About, 6, added 2026-11-23; a missing one crashed the menu's side list) */
const char* const kTabIcons[TAB_COUNT] = {"general", "appearance", "graphics", "audio", "controls", "enhancements", "about"};

/* the first-run hint (mockup 10): until when it shows (SDL ticks; 0: not showing) */
Uint64 s_toast_until = 0;
/* the notice's text when it isn't the menu hint (empty: the hint). *Updated 2026-11-13 (snatcherLeo):* "Report saved" uses
 * the same notice ("we could have that text serve as a notification, just like the "Press esc to open up menu""),
 * instead of a line drawn over Back to the game */
std::string s_toast_msg;

void refresh() {
    if (s_doc == nullptr) {
        return;
    }
    sync_live_values();
    const bool dark = theme_is_dark();
    apply_title_bar();
    if (Rml::Element* body = s_doc->GetElementById("body")) {
        body->SetClass("dark", dark);
        body->SetClass("light", !dark);
        const RowDef* pause = live_row(LIVE_PAUSE);
        body->SetClass("pausing", pause != nullptr && pause->value != 0 && !s_test_no_pause && !s_in_hub);
        body->SetClass("reporting", s_report);
        body->SetClass("reportsaved", s_report && s_report_done);
        body->SetClass("inhub", page()); /* the page layout (the hub's, or the full window in game) */
        body->SetClass("ingame", !s_in_hub);
        body->SetClass("panel", !page()); /* the in-game side panel */
        body->SetClass("lvl0", !page() && s_level == 0);
        body->SetClass("lvl1", !page() && s_level == 1);
        /* the side menu's help box grown to four lines on Enhancements (Speed's explanations are longer; mockup s7_side) */
        body->SetClass("tallhelp", !page() && s_level == 1 && s_tab == 5);
        body->SetClass("quitask", s_quit_ask);
        /* a note is showing: on the panel's first screen the list ends higher, so the note sits between the list and
         * the buttons (*Fixed 2026-11-16*, snatcherLeo: the "still saving" note was drawn over the Quit to hub button) */
        body->SetClass("hasnote", !s_note.empty());
        body->SetClass("fullgame", full_in_game()); /* Quit to hub above BACK (settings.rcss #fquit) */
        /* its question's dialog, and Go back's (the same box and dimmed window, in the hub and in game alike) */
        body->SetClass("fq", (s_fq && full_in_game()) || s_gb);
        body->SetClass("gb", s_gb); /* ... Go back's is taller: raised to stay centred (settings.rcss) */
        body->SetClass("whatoff", s_what);            /* General › What it turns off (2.12) */
        body->SetClass("toastonly", !s_open && s_toast_until != 0);
        body->SetClass("fadeonly", s_fade_only); /* Quit to hub from the shortcut: only the fade, no menu */
        /* Always show the menu bar, at once (snatcherLeo, 2026-11-13: "that change doesn't get applied until i exist the
         * menu. Ideally, if I toggl eit on, that's when the top toolbar occurs"): in game, with it on, the bar shows
         * above the open menu too (settings.rcss body.barup; the menu moves down under it). It answers clicks once the
         * menu is closed, as before */
        body->SetClass("barup", s_open && !s_in_hub && !s_report && pc_settings_int("menu_bar_always", 0) != 0);
        /* *Updated 2026-11-13 (snatcherLeo: "I am unable to click on "File" or "Help" once the side settings menu is opened
         * up", and with Always show off "I cannot hover my mouse to get it to pop up"):* the bar now answers clicks over
         * the open side menu too (settings.rcss no longer passes the mouse through it), and with Always show off the
         * mouse at the top brings it up over the menu's header (barhover; on_event), without moving the menu down so
         * nothing jumps. Not over a report (its own box) */
        body->SetClass("barhover", s_open && !s_in_hub && !page() && !s_report && s_bar &&
                                       pc_settings_int("menu_bar_always", 0) == 0);
        const RowDef* bg = live_row(LIVE_MENUBG);
        body->SetClass("solid", bg != nullptr && bg->value == 1);
        if (s_report && !s_in_hub) {
            body->SetClass("pausing", true); /* a report always pauses (plan: "It pauses the game") */
        }
    }
    if (Rml::Element* e = s_doc->GetElementById("resume")) {
        e->SetInnerRML("&#8249; " + tx("settings.back")); /* the hub's page only (the panel has no buttons row) */
    }
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (Rml::Element* e = s_doc->GetElementById("tab-" + std::to_string(i))) {
            e->SetClass("on", i == s_tab);
            /* the controller is on the category column (and not on its Quit to hub) */
            e->SetClass("f", page() && s_cats && !s_colQuit && !s_colBack && i == s_tab);
        }
    }
    if (Rml::Element* e = s_doc->GetElementById("resume")) e->SetClass("f", page() && s_cats && s_colBack && !s_fq);
    /* the full window in game: QUIT TO HUB above BACK, lit while the controller is on it; its question as a dialog
     * (Cancel first; the ring as on the side panel's buttons, .fring) */
    if (Rml::Element* e = s_doc->GetElementById("fquit")) {
        e->SetInnerRML(tx("settings.quit"));
        e->SetClass("f", full_in_game() && s_cats && s_colQuit && !s_fq);
    }
    if (Rml::Element* e = s_doc->GetElementById("fqdlg")) {
        std::string d;
        if (s_gb) {
            /* Go back's pop-up: the title, the question (in the hub OrrePort restarts by itself; in game it waits for
             * the next start, _ask_later), the warning, then Cancel / Go back. Ids gb-cancel, gb-go */
            const PcUpdateStatus st = about_status();
            const char* ring = "<span class='fring'></span>";
            /* Studio open (in the hub): the question becomes "Close Studio first..." and Go back's button Close Studio
             * (Studio's pop-up does the same for OrrePort); it turns back once Studio has closed (the frame's check) */
            const bool studio = s_in_hub && about_studio_open();
            d = "<span class='fqt'>" + rml_escape(tx("update.go_back_title", {{"version", st.previous}})) + "</span><span class='fqs'>" +
                rml_escape(studio ? tx("update.go_back_close", {{"program", "Studio"}})
                                  : tx(s_in_hub ? "update.go_back_ask" : "update.go_back_ask_later",
                                       {{"version", st.previous}, {"program", "OrrePort"}})) +
                "</span><span class='fqs gbwarn'>" +
                rml_escape(tx("update.go_back_warn", {{"version", st.previous}, {"current", st.current}})) +
                "</span><span class='fqbtns'><span class='fqb" + std::string(s_gbFocus == 0 ? " f" : "") + "' id='gb-cancel'>" +
                (s_gbFocus == 0 ? ring : "") + tx("common.cancel") + "</span><span class='fqb" + (studio ? "" : " danger") +
                std::string(s_gbFocus == 1 ? " f" : "") + "' id='gb-go'>" + (s_gbFocus == 1 ? ring : "") +
                (studio ? rml_escape(tx("update.close_button", {{"program", "Studio"}})) : tx("update.go_back_button")) +
                "</span></span>";
        } else if (s_fq && full_in_game()) {
            const char* ring = "<span class='fring'></span>";
            d = "<span class='fqt'>" + tx("settings.quit.title") + "</span><span class='fqs'>" + tx("settings.quit.detail") +
                "</span><span class='fqchk" + std::string(s_fqFocus == 2 ? " f" : "") + "' id='fq-noask'>" +
                (s_fqFocus == 2 ? ring : "") + "<span class='cb" + (s_fqNoAsk ? " on" : "") + "'>" +
                (s_fqNoAsk ? "&#10003;" : "") + "</span>" + tx("settings.quit.dont_ask") + "</span><span class='fqbtns'>" +
                "<span class='fqb" + std::string(s_fqFocus == 0 ? " f" : "") + "' id='fq-cancel'>" +
                (s_fqFocus == 0 ? ring : "") + tx("common.cancel") + "</span><span class='fqb danger" +
                std::string(s_fqFocus == 1 ? " f" : "") + "' id='fq-quit'>" + (s_fqFocus == 1 ? ring : "") +
                tx("settings.quit") + "</span></span>";
        }
        e->SetInnerRML(d);
    }
    /* "What it turns off" (2.12): written once as it opens, so its scroll position stays while it is open */
    if (Rml::Element* e = s_doc->GetElementById("whatdlg")) {
        static bool shown = false;
        if (s_what && !shown) {
            e->SetInnerRML("<span class='fqt'>" + tx(s_what_adds ? "adds.title" : "accurate.what_off.title") +
                           "</span><div id='whatlist'>" + (s_what_adds ? pc_whatadds_rml() : hub_accurate_list_rml()) +
                           "</div><span class='fqbtns'><span class='fqb f' id='what-close'>" +
                           "<span class='fring'></span>" + tx("common.back") + "</span></span>");
        } else if (!s_what && shown) {
            e->SetInnerRML("");
        }
        shown = s_what;
    }
    /* the panel's head: the O (the theme's own, whatever the window icon is: snatcherLeo, 2026-11-12, "it will only show the
     * "O"") and "Menu" on the first screen, a back arrow and the category inside one; the game on the right */
    if (Rml::Element* e = s_doc->GetElementById("phead")) {
        std::string h = s_level == 0 ? std::string("<img id='plogo' src='menuicon://o-") + (dark ? "dark" : "light") +
                                           "'/><span id='ptitle'>" + tx("settings.menu") + "</span>"
                                     : std::string("<span id='pback'>&#8249;</span><span id='ptitle'>") +
                                           tx(kTabNames[s_tab]) + "</span>";
        h += "<span id='pgame'><span class='d'></span>" + tx("hub.game.colosseum.title") + "</span>";
        e->SetInnerRML(h);
    }
    const std::vector<Item> list = items();
    if (s_focus >= (int)list.size()) {
        s_focus = list.empty() ? 0 : (int)list.size() - 1;
    }
    const bool panel = !page();
    std::string rml, buttons;
    bool card_open = false;
    bool next_first = false; /* the row after General's accurate card has no line above it, as a list's first row */
    for (int k = 0; k < (int)list.size(); ++k) {
        const Item& it = list[k];
        const std::string id = "item-" + std::to_string(k);
        const bool f = k == s_focus && !(page() && s_cats); /* no row lit while on the category column */
        /* General's GameCube-accurate card (snatcherLeo's polish list 2026-11-16, mockups item 1 / 1b; UI id
         * settings.general.accurate_card): on the page layout the accurate row and its "What it turns off" row become
         * one card, the Saves page's accurate box at Settings' sizes: the name, its line (or the lock's reason in game),
         * a teaser counting what accurate mode turns off, the green link, the switch on the right. Both stay their own
         * stops for the controller (the switch item-k, the link item-k+1, so focus, A and clicks work as before); the
         * switch shows the controller's ring, the link an orange edge. The side menu keeps them as two plain rows (its
         * rows have no second line). All platforms (RmlUi) */
        if (page() && it.kind == IT_ROW && s_rows[it.index].live == LIVE_ACC_WHAT && k > 0 && list[k - 1].kind == IT_ROW &&
            s_rows[list[k - 1].index].live == LIVE_ACC_DEFAULT) {
            continue; /* drawn inside the card with the row before it */
        }
        /* the Fast-forward row's link (Speed; mockup s7): its own stop, drawn inside the row before it */
        if (it.kind == IT_ROW && s_rows[it.index].live == LIVE_FF_LINK && k > 0 && list[k - 1].kind == IT_ROW &&
            s_rows[list[k - 1].index].live == LIVE_FF_SPEED) {
            continue;
        }
        if (page() && it.kind == IT_ROW && s_rows[it.index].live == LIVE_ACC_DEFAULT) {
            const RowDef& r = s_rows[it.index];
            /* the teaser's number: the rows of accurate mode's list that it turns off (pc_options.c), counted, never
             * written in, so the card follows the list as it grows */
            long off = 0;
            for (int i = 0; i < pc_accurate_list_count(); ++i) {
                const PcAccurateRow* a = pc_accurate_list_at(i);
                if (a != nullptr && a->kind == PC_ACCURATE_OFF) ++off;
            }
            std::string link;
            if (k + 1 < (int)list.size() && list[k + 1].kind == IT_ROW && s_rows[list[k + 1].index].live == LIVE_ACC_WHAT) {
                const bool lf = k + 1 == s_focus && !s_cats;
                link = std::string("<span class='what") + (lf ? " f" : "") + "' id='item-" + std::to_string(k + 1) + "'>" +
                       tx(s_rows[list[k + 1].index].name) + "<span class='chev'>&#8250;</span></span>";
            }
            rml += std::string("<div class='acccard") + (r.locked ? " locked" : "") + "' id='acccard'><div class='l'>" +
                   "<span class='n'>" + tx(r.name) + "</span><span class='s'>" +
                   tx(r.locked && r.lock_reason != nullptr ? r.lock_reason : r.sub) + "</span>" +
                   "<span class='teaser' id='accteaser'>" + tx_count("settings.general.accurate_teaser", off) + "</span>" +
                   link + "</div><div class='c c-tog'>" + switch_rml(r.value != 0, true, f, id.c_str()) + "</div></div>";
            next_first = true;
            continue;
        }
        /* the panel's rows sit in a card (the shortcuts on the first screen, all of a category's rows) */
        if (panel && it.kind == IT_ROW && !card_open) {
            rml += "<div class='card'>";
            card_open = true;
        } else if (panel && it.kind != IT_ROW && card_open) {
            rml += "</div>";
            card_open = false;
        }
        if (it.kind == IT_CAT) {
            if (it.index == 0) {
                rml += "<div class='sec'>" + tx("settings.heading") + "</div>";
            }
            rml += std::string("<div class='cat") + (f ? " f" : "") + "' id='" + id +
                   "'><img class='ic' src='menuicon://" + kTabIcons[it.index] + "'/><div class='txt'><span class='cn'>" +
                   tx(kTabNames[it.index]) + "</span><span class='cs'>" + summary(it.index) +
                   "</span></div><span class='go'>&#8250;</span></div>";
            continue;
        }
        /* the controller's cursor on a button: a ring outside it with a gap, as the hub draws round PLAY (snatcherLeo,
         * 2026-11-15, mockup B: the 1 dp edge alone was hard to see from afar). RmlUi has no ::after, so the ring is a
         * child element (settings.rcss .fring) */
        const char* ring = f ? "<span class='fring'></span>" : "";
        if (it.kind == IT_RESUME) { /* Back to the game, full width, naming the shortcut that does the same */
            buttons += std::string("<span class='pbtn resume") + (f ? " f" : "") + "' id='" + id +
                       "'>" + ring + "&#9654;&#160; " + tx("settings.back_to_game") + " <span class='rk'>" +
                       pc_controls_menu_name(s_ctrl == CTRL_KEYBOARD) + "</span></span>";
            continue;
        }
        if (it.kind == IT_FULL) { /* the full Settings window, full width, above Back to the game */
            buttons += std::string("<span class='pbtn wide") + (f ? " f" : "") + "' id='" + id +
                       "'>" + ring + tx("settings.open_full") + "</span>";
            continue;
        }
        if (it.kind == IT_NOASK) { /* the question and its box, above Cancel / Quit to hub */
            buttons += "<div class='qask'><span class='qt'>" + tx("settings.quit.question") + "</span><span class='qbox" +
                       (f ? " f" : "") + "' id='" + id +
                       "'>" + ring + "<span class='cb" + (s_quit_noask ? " on" : "") + "'>" + (s_quit_noask ? "&#10003;" : "") +
                       "</span>" + tx("settings.quit.dont_ask") + "</span></div>";
            continue;
        }
        if (it.kind == IT_REPORT || it.kind == IT_QUIT) { /* the two buttons under the list */
            const std::string text = tx(s_quit_ask ? (it.kind == IT_REPORT ? "common.cancel" : "settings.quit")
                                                   : (it.kind == IT_REPORT ? "hub.link.report" : "settings.quit"));
            /* Quit to hub in red before its question too ("quitb": a red tint; snatcherLeo, 2026-11-15: "let's make QUIT TO HUB
             * more red, so users know to avoid it"), the full red once it asks */
            buttons += std::string("<span class='pbtn") +
                       (it.kind == IT_QUIT ? (s_quit_ask ? " danger" : " quitb") : "") +
                       (f ? " f" : "") + "' id='" + id + "'>" + ring + text + "</span>";
            continue;
        }
        const RowDef& r = s_rows[it.index];
        /* a group's label above its first row (Speed's FAST-FORWARD, ALWAYS FASTER, SOUND; mockup s7): caps, grey; on
         * the full page with its short line, in the side menu the label alone (the line shows in the help box of a
         * row that has none of its own). UI id: the group's key. All platforms (RmlUi) */
        if (it.index > 0 && s_rows[it.index - 1].kind == GROUP && s_rows[it.index - 1].tab == r.tab) {
            const RowDef& g = s_rows[it.index - 1];
            rml += std::string("<div class='grp' id='") + g.name + "'><span class='gl'>" + tx(g.name) + "</span>" +
                   (panel || g.sub == nullptr ? std::string() : "<span class='gs'>" + tx(g.sub) + "</span>") + "</div>";
            next_first = true; /* the row under a label has no line above it */
        }
        std::string cls = "row";
        cls += k == 0 || next_first ? " first" : "";
        next_first = false;
        cls += f ? " f" : "";
        cls += r.kind == DISABLED || r.locked ? " disabled" : "";
        cls += r.locked ? " locked" : "";
        std::string control = control_rml(r);
        /* *Changed 2026-11-16 (snatcherLeo's polish list, mockup item 3, "After B"):* in the side menu a locked row keeps its
         * one 46 dp line and its tag reads only "Accurate mode" (lock_tag_panel; the long "Off in GameCube-accurate
         * mode" ran past the 176 dp column and was cut); the reason shows in the help box below when the row has the
         * controller (as every row's description does). The full window keeps the long tag, with the reason on the
         * row's second line */
        const char* tag = panel && r.lock_tag_panel != nullptr ? r.lock_tag_panel : r.lock_tag;
        if (r.locked && tag != nullptr) {
            /* the one locked style (orreport-design-overview.md 4.1): a lock and the amber "off in accurate mode" tag in
             * place of the control; the lock is drawn from two boxes (its shackle and body), so no font needs a lock
             * symbol */
            control = "<div class='acclock'><span class='lki'><span class='lks'></span><span class='lkb'></span></span>" +
                      tx(tag) + "</div>";
        }
        if (f && s_drop >= 0 && r.kind == SELECT) {
            control.replace(control.find("class='sel'"), 11, "class='sel open'");
        }
        /* the Fast-forward row: a short box, then the green link naming the button for the device in hand ("LB ·
         * Change ›"), the link's own stop item-(k+1) (s_rows keeps it right after this row) */
        const bool ff_row = r.live == LIVE_FF_SPEED && k + 1 < (int)list.size() && list[k + 1].kind == IT_ROW &&
                            s_rows[list[k + 1].index].live == LIVE_FF_LINK;
        if (ff_row) {
            control += ff_link_rml(k + 1 == s_focus && !(page() && s_cats), k + 1);
        }
        /* on the quick row the master volume is just "Volume" (music lives in Audio) */
        /* *Changed 2026-11-16 (snatcherLeo's polish list, mockup item 3):* Music and sound and Battles carry their plain
         * names in the side menu too (they said "(only)" there from 2026-11-13; the help box below says what each does).
         * Game speed had "(everything)" for a while; *removed 2026-11-13* (snatcherLeo: "it's not really everything. Just have
         * a simple "Game speed" label") */
        const std::string name = about_row(r) ? about_name(r)
                                              : tx(panel && s_level == 0 && r.live == LIVE_VOL_MASTER ? "settings.volume" : r.name);
        /* a locked row says why in place of its second line (and in the help box below); a row with no line of its own
         * (Fast-forward: its group says it) shows none */
        const std::string sub = row_sub(it.index, false);
        /* the side menu's column for a control: a lock tag sizes to its short words (c-lock), so the row's name keeps
         * the rest of the line; Battles' choice box is wider (c-wide), as its "Match current Game speed" was cut off in
         * the 146 dp box and its name is short; the Fast-forward row's box and link share the wide column (c-ff2) */
        rml += "<div class='" + cls + "' id='" + id + "'><div class='l'><span class='n'>" + name + "</span>" +
               (sub.empty() ? std::string() : "<span class='s'>" + sub + "</span>") + "</div><div class='c " +
               (r.locked && tag != nullptr                 ? (panel ? "c-info c-lock" : "c-info")
                : ff_row                                   ? "c-ff2"
                /* *Changed 2026-11-19:* Frame rate is back in the normal box, its Match label shortened to "Match (120
                 * Hz)" (snatcherLeo: "the drop down box can also be less wide, no reason for it to be that wide") */
                : r.kind == SELECT && panel && r.live == LIVE_SPEED_BATTLE
                    ? "c-sel c-wide"
                : r.live == LIVE_UPD_VERSION || r.live == LIVE_LOGS ? "c-ib" /* About's icon buttons */
                : r.kind == SELECT || r.kind == ACTION ? "c-sel"
                : r.kind == TOGGLE                   ? "c-tog"
                : r.kind == SLIDER                   ? "c-sl"
                                                     : "c-info") +
               "'>" + control + "</div></div>";
        if (f && s_drop >= 0 && r.kind == SELECT) { /* the open drop-down's list, under its row */
            rml += "<div class='dd'>";
            for (int j = 0; j < (int)r.options.size(); ++j) {
                rml += std::string("<div class='opt") + (j == r.value ? " cur" : "") + (j == s_drop ? " f" : "") +
                       "' id='opt-" + std::to_string(j) + "'>" +
                       (r.live == LIVE_FRAME_RATE ? frame_rate_option(r, j) : word(r.options[j])) +
                       (j == r.rec ? "<span class='rec'>" + tx("settings.recommended") + "</span>" : std::string()) +
                       "</div>";
            }
            rml += "</div>";
        }
    }
    if (card_open) {
        rml += "</div>";
    }
    if (Rml::Element* e = s_doc->GetElementById("pbtns")) {
        e->SetInnerRML(buttons);
    }
    /* a category's help box: the focused setting's description, in one place (rows never change height) */
    if (Rml::Element* e = s_doc->GetElementById("phelp")) {
        std::string help;
        if (!list.empty() && list[s_focus].kind == IT_ROW) {
            const RowDef& r = s_rows[list[s_focus].index];
            help = "<span class='hn'>" + (about_row(r) ? about_name(r) : tx(r.name)) + "</span> · " +
                   row_sub(list[s_focus].index, true);
        }
        e->SetInnerRML(help);
    }
    if (Rml::Element* e = s_doc->GetElementById("list")) {
        e->SetInnerRML(rml);
        /* keep the focused row (or the list's highlighted option) in view when the list scrolls (a short window, the
         * Steam Deck) */
        Rml::Element* focused = s_drop >= 0 ? s_doc->GetElementById("opt-" + std::to_string(s_drop))
                                            : s_doc->GetElementById("item-" + std::to_string(s_focus));
        if (focused != nullptr) {
            focused->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest,
                                                               Rml::ScrollAlignment::Nearest,
                                                               Rml::ScrollBehavior::Instant,
                                                               Rml::ScrollParentage::Closest));
        }
    }
    /* the panel's button hints, for the controller in hand and what the focus is on */
    if (Rml::Element* e = s_doc->GetElementById("phint")) {
        e->SetInnerRML(hint_bar(!list.empty() && list[s_focus].kind == IT_ROW));
    }
    /* the first-run hint's text, for the controller connected (mockup 10) */
    if (Rml::Element* e = s_doc->GetElementById("toast"); e != nullptr && s_toast_until != 0) {
        /* the button chosen in Controls (pc_controls_ui.cpp), else the controller's default (snatcherLeo, 2026-11-12: the
         * shortcut "is referenced in tha tnotification") */
        /* one sentence with the button put in, bold ("Press {button} for the OrrePort menu") */
        const std::string how =
            tx("settings.toast.press", {{"button", std::string("<b>") + pc_controls_menu_name(s_ctrl == CTRL_KEYBOARD) + "</b>"}});
        e->SetInnerRML(std::string("<img id='tlogo' src='menuicon://o-") + (dark ? "dark" : "light") +
                       "'/><span id='ttext'>" +
                       (!s_toast_msg.empty() ? s_toast_msg
                                             : how + "<span id='tsub'>" + tx("settings.toast.sub") + "</span>") +
                       "</span>");
    }
    bar_rml(); /* the menu bar, its open menu and About */
    /* a note only when there is one (the trial's "aren't connected yet" line is gone: snatcherLeo, 2026-11-11) */
    if (Rml::Element* e = s_doc->GetElementById("live")) {
        e->SetInnerRML(s_note);
        e->SetClass("empty", s_note.empty());
        e->SetClass("err", !s_note.empty() && s_note == s_note_err);
        e->SetClass("big", !s_note.empty() && s_note == s_note_big); /* the adapter's result (snatcherLeo: "a little bigger") */
    }
    /* the full page's heading: the open category */
    if (Rml::Element* e = s_doc->GetElementById("mh")) {
        e->SetInnerRML(tx(kTabNames[s_tab >= 0 && s_tab < TAB_COUNT ? s_tab : 0]));
    }
}

/* Fit the panel (and the hint at start) to the window: RmlUi's dp units follow Windows' display scaling (2.25 on a 225%
 * screen), so a small window would cut the panel off. Scale down to fit, never above the display scaling: the 420-wide
 * panel takes at most 40% of the width, and the window shows at least 640 of its height (the list scrolls below that:
 * the Steam Deck's 800-line screen). Called right before the panel or the hint is first shown, as well as every frame
 * while either shows. *Corrected 2026-11-12 (snatcherLeo: "the orreport menu message flickers from max size to smaller size,
 * we can't have it flicker"):* the scale used to be set only by the frame after the first, so the hint's first frame
 * was drawn at the full display scaling and then jumped smaller. */
void fit_to_window() {
    Rml::Context* context = aurora::rmlui::get_context();
    if (context == nullptr) {
        return;
    }
    /* the display scaling, from Windows through SDL (the context's own ratio may already be the hub's override when
     * this first runs: the hint at start, which made the panel too small) */
    float system_scale = 1.0f;
    int count = 0;
    if (SDL_Window** windows = SDL_GetWindows(&count)) {
        if (count > 0) {
            system_scale = SDL_GetWindowDisplayScale(windows[0]);
        }
        SDL_free(windows);
    }
    if (system_scale <= 0.0f) {
        system_scale = 1.0f;
    }
    const Rml::Vector2i dims = context->GetDimensions();
    float scale = system_scale;
    /* the full window in game takes the hub page's size rule (pc_hub.cpp fit: 1600 x 960, at most 0.8 of the
     * display scaling); the side panel and the hint their own */
    if (s_full && s_open) {
        scale = SDL_min(SDL_max(0.8f * system_scale, 1.0f), SDL_min(dims.x / 1600.0f, dims.y / 960.0f));
    } else {
        scale = SDL_min(scale, dims.x / 1100.0f);
        scale = SDL_min(scale, dims.y / 640.0f);
    }
    if (scale > 0.0f && SDL_fabsf(scale - context->GetDensityIndependentPixelRatio()) > 0.01f) {
        aurora::rmlui::set_ui_scale(scale);
    }
}

/* the Tweaks rows' values: each switch's effective value (pc_option), set at start-up and again whenever the menu opens,
 * since the menu bar's Gameplay menu can change the same switches meanwhile; in GameCube-accurate mode a fixed "Off in
 * GameCube-accurate mode" line instead of a toggle (pc_options.c forces every switch off there) */
/* *Changed 2026-11-16 (2.12, accurate mode per save):* the rows show the player's own choices (pc_option_choice) and
 * are changeable in the hub, where no save has started yet (the choices are the normal mode's: either kind of save may
 * be picked next). Only while a game runs in accurate mode are the rows that accurate mode turns off locked (snatcherLeo,
 * 2026-11-16: "Locked off because this save is in accurate mode"), and General's accurate row greyed ("Close the game to
 * change this"). Look-only rows (resolution, filtering, fonts, languages, volume...) are never locked: they keep the
 * random numbers (snatcherLeo's rule, 2026-11-16). Which rows lock, and why:
 *   - the Tweaks switches (pc_options.c entries): pc_option returns 0 for each in accurate mode.
 * *Changed 2026-11-16 (the Speed settings' build):* Music and sound was locked too ("Follows Game speed"), because
 * accurate mode forced the sound to follow the game's speed; with the sound split the game's sound engine keeps the
 * console's schedule at any speed, so every Speed row works in accurate mode (the x3ring pass) and none is locked.
 * All platforms (RmlUi). */
void mode_values() {
    const bool game_accurate = pc_option_accurate_locked() && pc_option_gamecube_accurate();
    for (RowDef& r : s_rows) {
        if (r.live == LIVE_OPTION) {
            r.value = pc_option_choice(r.options[0].c_str()) != 0;
            r.locked = game_accurate;
            r.lock_reason = "settings.accurate.locked";
            r.lock_tag = "settings.tweaks.accurate_off";
            r.lock_tag_panel = "settings.accurate.tag"; /* "Accurate mode": the side menu's one-line rows */
        } else if (r.live == LIVE_FRAME_RATE) {
            /* frame smoothing (step 3.4): *unlocked 2026-11-20*. It was locked in an accurate-mode game until the the reference playthrough
             * gate passed for the rates. The gate passed (frame-smoothing-plan.md section 34: 0, 1, 3 and 7 in-betweens,
             * every sitting identical), so the row works in both modes, like the other Graphics rows */
            r.locked = false;
        } else if (r.live == LIVE_ACC_DEFAULT) {
            /* in game (either mode): the mode belongs to saves and changes only with the game closed, so the row is
             * locked there.
             * *Changed 2026-11-21 (snatcherLeo: "the settings row, if you are playing a save that is Gamecube-accurate, should
             * just simply have the slider toggled on, but its still greyed out, to show that 'the save you are playing is
             * on accurate mode'"):* while a game runs the row shows the running save's own mode (on: accurate, off:
             * Enhanced), named "GameCube-accurate mode" without "for new saves"; in the hub it stays the default for new
             * saves. The locked row is never applied (apply_live is reached only through an unlocked row), so the
             * default in settings.ini is never written from here */
            const bool in_game = s_game_running || pc_option_accurate_locked();
            r.value = in_game ? (game_accurate ? 1 : 0) : pc_option_accurate_new_saves();
            r.name = in_game ? "settings.general.accurate_running" : "settings.general.accurate_default";
            r.locked = in_game;
            r.lock_reason = game_accurate ? "settings.accurate.closed_this_save" : "settings.accurate.closed";
            r.lock_tag = nullptr; /* the switch stays, greyed, showing the running save's mode */
        } else if (r.live == LIVE_DISCLABEL) {
            /* *Added 2026-11-21 (snatcherLeo: with no disc added, "in settings, they cannot get to the disc art area"):* in the
             * hub with no disc yet, Appearance's "Change disc art" row is greyed and locked, its line saying why (the
             * words the row's old error note used), like the other rows that can't be used right now; once a disc is
             * added it opens the label page as before. In game it keeps saying where to find it */
            r.locked = s_in_hub && pc_hub_disc_ready() == 0;
            r.lock_reason = "settings.note.disc_art_no_disc";
            r.lock_tag = nullptr; /* the row's chevron stays, greyed */
        } else if (r.live == LIVE_IMPORT_ASK) {
            /* read again each time the menu opens: the hub's import question may have turned it off meanwhile */
            r.value = pc_settings_int("import_ask", 1) != 0;
        } else if (r.live == LIVE_UPD_CHECK) { /* read each time: Studio's Settings › About may have changed it */
            r.value = pc_settings_int("update_check", 1) != 0;
        } else if (r.live == LIVE_UPD_AUTO) {
            r.value = pc_settings_int("update_auto_download", 0) != 0;
        }
    }
}
void tweak_values() { mode_values(); } /* the older name, still called where the menu opens and at start-up */

/* ORRE_SETTINGS_FOCUS=<n> (a test switch, never a setting; 2026-11-16, for pictures of snatcherLeo's polish list): with the
 * menu just opened on a category's page (the hub's gear, or ORRE_SETTINGS_TAB in game), the controller's place goes to
 * that page's item n (0 the first row), as pressing Down n times would, so a scripted window can photograph a focused
 * row or switch without a controller. All platforms */
void test_focus() {
    const char* v = SDL_getenv("ORRE_SETTINGS_FOCUS");
    if (v == nullptr || !s_open) return;
    s_cats = false;
    s_focus = (int)SDL_strtol(v, nullptr, 10);
    refresh(); /* clamps s_focus to the page's items */
}

void set_open(bool open, const char* why) {
    if (open == s_open) {
        return;
    }
    if (open) tweak_values();
    s_open = open;
    s_what = false;
    s_note.clear();
    s_focus = 0;
    s_toast_until = 0; /* opening (or closing) the menu ends the first-run hint */
    s_level = 0;  /* the panel opens on its first screen */
    s_cats = true; /* the page opens on its category column */
    s_quit_ask = false;
    s_colQuit = false;
    s_colBack = false;
    s_fq = false;
    /* the menu bar gives way to the menu; and on closing, a bar brought up over the menu (barhover) goes with it, so
     * the closed menu's own mouse code shows it again from the start (otherwise it stayed "shown" in a hidden page) */
    s_bar = false;
    s_bar_menu = -1;
    s_about = false;
    if (!open) s_fade_only = false;
    s_drop = -1;
    s_drag = -1;
    s_hold_dir = 0;
    if (!open) {
        s_in_hub = !s_game_running; /* still the hub until the game has started (s_in_hub says why) */
        s_full = false;
    } else {
        s_full = !s_in_hub && pc_settings_int("menu_full", 0) == 1; /* General > In-game menu */
    }
    if (s_doc != nullptr) {
        if (open) {
            if (!s_in_hub) {
                fit_to_window(); /* the first frame already at its size (no jump) */
            }
            s_doc->Show();
            s_doc->PullToFront(); /* over the hub's page when opened from the gear */
        } else {
            s_doc->Hide();
        }
    }
    refresh();
    input::reconcile(); /* drop what the game held when the overlay took the controller; give it back on close */
    std::printf("[settings_overlay] %s (%s)\n", open ? "opened" : "closed", why);
}

std::string note_text() {
    Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById("note") : nullptr;
    auto* control = dynamic_cast<Rml::ElementFormControl*>(e);
    return control != nullptr ? control->GetValue() : std::string();
}

void update_note_placeholder() {
    if (Rml::Element* body = s_doc != nullptr ? s_doc->GetElementById("body") : nullptr) {
        body->SetClass("hasnote", !note_text().empty());
    }
}

/* alone: opened by itself (F8, Help > Report a problem), not from the open menu (s_report_alone) */
void open_report(bool alone) {
    if (s_doc == nullptr) {
        return;
    }
    s_report = true;
    s_report_alone = alone;
    s_report_save = 0;
    if (auto* control = dynamic_cast<Rml::ElementFormControl*>(s_doc->GetElementById("note"))) {
        control->SetValue("");
    }
    /* the game picture as it is now, without the menu (the frame-dump capture, as F12's screenshot) */
    char dir[1300];
    pc_paths_dir(PC_PLACE_REPORTS, dir, sizeof(dir)); /* Pokemon Colosseum\Reports (flat: Reports), 4.1 */
    SDL_CreateDirectory(dir);
    s_shot_file = std::string(dir) + "/.report_screenshot.png";
    pc_frame_dump_request_screenshot_to(s_shot_file.c_str());
    refresh();
    update_note_placeholder();
    /* typing goes straight into the box: focused on the next frame, once the panel has been laid out (focusing it
     * here, while it was still display: none, silently failed, and typing only began after a click; snatcherLeo,
     * 2026-11-09: "It took a while to be able to type in the optional box field") */
    s_focus_note = 2;
    std::printf("[settings_overlay] Report a problem opened\n");
}

/* Cancel, Esc, B or a click outside the box. A report opened by itself closes the menu with it, so the game comes
 * back as it was; one opened from the menu goes back to the menu */
void close_report(const char* why) {
    const bool alone = s_report_alone;
    s_report = false;
    s_report_alone = false;
    s_report_save = 0;
    s_report_done = false;
    if (!s_shot_file.empty()) {
        SDL_RemovePath(s_shot_file.c_str()); /* a picture taken for a report that wasn't saved */
    }
    std::printf("[settings_overlay] Report a problem closed (%s)\n", why);
    if (alone) {
        set_open(false, why);
    } else {
        refresh();
    }
}

/* A short notice over the game, bottom left, the same box and time as the menu hint at start (s_toast_msg); shown
 * with the menu closed. text: RmlUi markup, a bold first line and an optional <span id='tsub'> line */
void show_notice(const std::string& text) {
    if (s_doc == nullptr) return;
    s_toast_msg = text;
    s_toast_until = SDL_GetTicks() + 4000;
    refresh();
    fit_to_window();
    s_doc->Show();
}

/* Done on the saved screen (A, B, Enter, Esc or a click outside): the menu closes, back to the game, as after a save
 * before (snatcherLeo, 2026-11-13: "the text overlayed on back to game, we don't need") */
void finish_report(const char* why) {
    s_report = false;
    s_report_alone = false;
    s_report_save = 0;
    s_report_done = false;
    std::printf("[settings_overlay] Report a problem closed (%s)\n", why);
    set_open(false, why);
}

/* Open folder on the saved screen: Explorer on the reports folder with the zip selected (never in a test run) */
void report_open_folder() {
    if (!s_report_done || s_report_zip.empty()) return;
    std::printf("[settings_overlay] report: Open folder\n");
    if (!pc_platform_automated_run()) pc_report_show_in_folder(s_report_zip.c_str());
}

/* Save report: waits (a few frames at most) for the screenshot, then writes the zip.
 * *Changed 2026-11-23 (snatcherLeo: "Studio's report thing is actually far more elegant, saves it to clipboard and lets user
 * know, and then tells them they can open file epxlorer afterwards to locate file ... we can match the UI and
 * process"):* the panel stays open on a saved screen, as Studio's and the hub's: what happened (copied or not), the
 * zip's name and size (red past about 10 MB), Open folder and Done. Explorer no longer opens by itself, and the
 * notice over the game is no longer used for a save (a failure still closes with the notice, as before). */
void save_report_now() {
    char zip[1300];
    const std::string note = note_text();
    const bool shot = !pc_frame_dump_screenshot_busy();
    const bool saved = pc_report_write(note.c_str(), shot ? s_shot_file.c_str() : nullptr, zip, sizeof(zip));
    if (!saved && !s_shot_file.empty()) {
        SDL_RemovePath(s_shot_file.c_str()); /* as a cancelled report's */
    }
    s_shot_file.clear();
    s_report_save = 0;
    if (!saved) {
        s_report = false;
        s_report_alone = false;
        set_open(false, "report not saved");
        show_notice("<b>" + tx("settings.report.failed") + "</b><span id='tsub'>" + tx("settings.report.failed.sub") +
                    "</span>");
        std::printf("[settings_overlay] Report a problem could not be saved\n");
        return;
    }
    /* copied as a file for Ctrl+V (snatcherLeo, 2026-11-22: the same habit as Studio's report; pc_clip_file.h), never in a test
     * run (it would overwrite the clipboard of whoever uses the PC); the screen says "copied" only when it was. No
     * channel named (snatcherLeo, 2026-11-13: "On Discord, the support thread may change ... We could keep it more
     * generalized") */
    const bool copied = !pc_platform_automated_run() && pc_clipboard_set_file(zip) != 0;
    s_report_zip = zip;
    s_report_done = true;
    if (Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById("note") : nullptr) {
        e->Blur(); /* Enter is Done now, not a new line in the note */
    }
    SDL_PathInfo info;
    const double mb = SDL_GetPathInfo(zip, &info) ? (double)info.size / (1024.0 * 1024.0) : 0.0;
    char size[32];
    std::snprintf(size, sizeof(size), "%.1f MB", mb);
    const bool big = mb > 9.5;
    const size_t slash = s_report_zip.find_last_of("/\\");
    auto set = [](const char* id, const std::string& rml) {
        if (Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById(id) : nullptr) e->SetInnerRML(rml);
    };
    set("rsavedmsg", rml_escape(tx(copied ? "report.saved_body" : "report.saved_not_copied")));
    set("rfilename", rml_escape(slash == std::string::npos ? s_report_zip : s_report_zip.substr(slash + 1)));
    set("rfilesize", rml_escape(tx("report.size", {{"size", size}})));
    set("rbig", big ? rml_escape(tx("report.too_big", {{"size", size}})) : std::string());
    if (Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById("rfilesize") : nullptr) e->SetClass("big", big);
    std::printf("[settings_overlay] Report a problem saved: %s, %s%s\n", size,
                copied ? "copied to the clipboard as a file" : "not copied", big ? " (over 9.5 MB)" : "");
    refresh();
}

/* tab: a category's id (not its place in the list; kTabOrder) */
void set_tab(int tab) {
    s_tab = tab >= 0 && tab < TAB_COUNT ? tab : 0;
    s_colQuit = false; /* a category has the controller again, not the column's Quit to hub or BACK */
    s_colBack = false;
    s_focus = 0;
    s_drop = -1;
    s_note.clear();
    refresh();
}

void apply_live(RowDef& r);

/* ---- the controls (snatcherLeo, 2026-11-11, the project's proposal: "B back one level", "A must not mean Resume"):
 *   Up / Down (D-pad, left stick, arrows; held: repeats)  move, or move in an open drop-down
 *   Left / Right                                          change the row in place (no wrapping)
 *   A (Enter, Space, a click)                             choose: open a drop-down / pick in it, flip a switch,
 *                                                         open a category, Report a problem, Quit to hub
 *   B (Backspace)                                         back one level: drop-down -> row, category -> first
 *                                                         screen, first screen -> the game
 *   START (Esc)                                           close, back to the game, from anywhere
 *   X (Delete)                                            the row back to its default
 *   Y                                                     Report a problem (in game)
 *   L / R (Q / E, Page Up / Down)                         the previous / next category (inside one; the hub)
 * ---- */
void nav(int d) {
    if (s_what) { /* "What it turns off": Up / Down scroll its list */
        if (Rml::Element* l = s_doc != nullptr ? s_doc->GetElementById("whatlist") : nullptr) {
            l->SetScrollTop(l->GetScrollTop() + 60.f * (float)d);
        }
        return;
    }
    if (s_gb) return; /* Go back's pop-up: only Left / Right between its two buttons */
    if (s_fq) { /* the Quit to hub dialog: Up to Don't ask again, Down back to the buttons (Cancel) */
        if (d < 0) s_fqFocus = 2;
        else if (s_fqFocus == 2) s_fqFocus = 0;
        refresh();
        return;
    }
    if (page() && s_cats && s_drop < 0) { /* up / down the category column; the page shows each one as it is reached */
        /* the column ends with QUIT TO HUB (in game only) and BACK: the categories, then QUIT TO HUB, then BACK, then
         * round to the first category again (Up goes the other way) */
        /* by place in the shown order (kTabOrder), not by id */
        if (s_colBack) {
            s_colBack = false;
            if (d > 0) set_tab(tab_at(0));
            else if (full_in_game()) {
                s_colQuit = true;
                refresh();
            } else set_tab(tab_at(TAB_COUNT - 1));
            return;
        }
        if (s_colQuit) {
            s_colQuit = false;
            if (d > 0) {
                s_colBack = true;
                refresh();
            } else set_tab(tab_at(TAB_COUNT - 1));
            return;
        }
        if (d > 0 && tab_pos(s_tab) == TAB_COUNT - 1) {
            (full_in_game() ? s_colQuit : s_colBack) = true;
            refresh();
            return;
        }
        if (d < 0 && tab_pos(s_tab) == 0) {
            s_colBack = true;
            refresh();
            return;
        }
        set_tab(tab_at(tab_pos(s_tab) + d));
        return;
    }
    if (s_drop >= 0) {
        const std::vector<Item> list = items();
        const RowDef& r = s_rows[list[s_focus].index];
        s_drop = SDL_clamp(s_drop + d, 0, (int)r.options.size() - 1);
        refresh();
        return;
    }
    const std::vector<Item> list = items();
    const int total = (int)list.size();
    /* up from the right-hand button of the bottom pair (Quit to hub) goes to what is above the pair, as from the
     * left one (snatcherLeo, 2026-11-13: "if I click "up" on the d-pad, that should take me to "Back to the game"") */
    if (d < 0 && s_focus < total && list[s_focus].kind == IT_QUIT) {
        for (int k = 0; k < total; ++k) {
            if (list[k].kind == IT_REPORT && k > 0) {
                s_focus = k - 1;
                refresh();
                return;
            }
        }
    }
    if (total > 0) {
        /* Up / Down pass over the Fast-forward row's link (it sits on the box's line; Right reaches it): from the link
         * they move as from its box */
        auto is_link = [&](int k) {
            return k >= 0 && k < total && list[k].kind == IT_ROW && s_rows[list[k].index].live == LIVE_FF_LINK;
        };
        if (is_link(s_focus) && s_focus > 0) --s_focus;
        s_focus = (s_focus + d + total) % total;
        if (is_link(s_focus)) s_focus = (s_focus + d + total) % total;
    }
    refresh();
}

void choose();

void adjust(int d) {
    if (s_drop >= 0 || s_what) {
        return;
    }
    if (s_gb) { /* Go back's pop-up: Left is Cancel, Right is Go back */
        s_gbFocus = d > 0 ? 1 : 0;
        refresh();
        return;
    }
    if (s_fq) { /* the dialog's two buttons: Left is Cancel, Right is Quit to hub */
        s_fqFocus = d > 0 ? 1 : 0;
        refresh();
        return;
    }
    if (page() && s_cats) { /* Right on the category column goes in, as A (not on its Quit to hub or BACK) */
        if (d > 0 && !s_colQuit && !s_colBack) choose();
        return;
    }
    const std::vector<Item> list = items();
    if (list.empty()) {
        return;
    }
    const Item& it = list[s_focus];
    if (it.kind == IT_CAT && d > 0) { /* Right on a category opens it, as A */
        choose();
        return;
    }
    /* the two buttons side by side (Report a problem / Quit to hub, or Cancel / Quit to hub): left and right move
     * between them (snatcherLeo, 2026-11-13: "I want to go "right" on the d-pad, to get to "Quit to hub"") */
    if ((it.kind == IT_REPORT && d > 0) || (it.kind == IT_QUIT && d < 0)) {
        s_focus = SDL_clamp(s_focus + d, 0, (int)list.size() - 1);
        refresh();
        return;
    }
    if (it.kind != IT_ROW) {
        return;
    }
    /* the Fast-forward row: Left / Right move between its box and its link, like the side-by-side buttons above (snatcherLeo,
     * the mockup's "move right from the box"; the project's choice, 2026-11-16: the box's speed changes through its list with
     * A, so Right never both changes the speed and moves) */
    if (s_rows[it.index].live == LIVE_FF_SPEED) {
        if (d > 0 && s_focus + 1 < (int)list.size() && list[s_focus + 1].kind == IT_ROW &&
            s_rows[list[s_focus + 1].index].live == LIVE_FF_LINK) {
            ++s_focus;
            refresh();
        }
        return;
    }
    if (s_rows[it.index].live == LIVE_FF_LINK) {
        if (d < 0 && s_focus > 0) {
            --s_focus;
            refresh();
        }
        return;
    }
    RowDef& r = s_rows[it.index];
    if (r.locked) return; /* accurate mode's lock (mode_values): nothing changes while this game runs */
    const int count = (int)r.options.size();
    const int before = r.value;
    switch (r.kind) {
    case SELECT:
    case SLIDER:
        r.value = SDL_clamp(r.value + d, 0, count - 1);
        break;
    case TOGGLE:
        r.value = d > 0 ? 1 : 0;
        break;
    default:
        return;
    }
    if (r.value != before) {
        apply_live(r);
    }
}

void quit_ask_end() {
    s_quit_ask = false;
    const std::vector<Item> list = items();
    for (int k = 0; k < (int)list.size(); ++k) {
        if (list[k].kind == IT_QUIT) s_focus = k;
    }
    refresh();
}

/* Quit to hub: never while the game is writing to the memory card (a save cut short could be lost); then OrrePort
 * starts again on the hub and this program ends (pc_platform.c) */
void quit_to_hub() {
    /* *Changed 2026-11-15 (snatcherLeo: "when I click "Quit to hub", it briefly flickers the quick save menu back"):* the
     * question stays on screen through the fade (it was cleared here first, so the panel redrew its usual buttons for
     * the fade's third of a second); it is put away only when the quit is held */
    if (pc_card_used_within(2500)) {
        s_quit_ask = false;
        s_fq = false;
        /* red, as a refusal (snatcherLeo, 2026-11-16: "reposition it and make it red") */
        s_note = s_note_err = tx("settings.note.saving");
        std::printf("[settings_overlay] quit to hub held: the memory card was used just now\n");
        refresh();
        return;
    }
    std::printf("[settings_overlay] quit to hub\n");
    pc_settings_save();
    /* the window fades to black first (snatcherLeo: "NO way to make that more... aesthetic?"); the restart happens when the
     * fade is done (quit_fade_frame) */
    if (!s_open) { /* the shortcut with "Don't ask again": the fade alone, never the menu (snatcherLeo, 2026-11-13) */
        s_fade_only = true;
        set_open(true, "Quit to hub");
    }
    s_quit_fade_since = SDL_GetTicks();
    refresh();
}

/* each frame: the fade's darkness, then the restart; it takes about a third of a second */
void quit_fade_frame() {
    if (s_quit_fade_since == 0 || s_doc == nullptr) return;
    const Uint64 t = SDL_GetTicks() - s_quit_fade_since;
    if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("quitting", true);
    if (Rml::Element* fade = s_doc->GetElementById("fade")) {
        fade->SetProperty("opacity", std::to_string(SDL_min(1.0, (double)t / 300.0)));
    }
    if (t < 380) return; /* fully black for a moment */
    s_quit_fade_since = 0;
    if (!pc_platform_restart_to_hub()) {
        s_fade_only = false;
        if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("quitting", false);
        s_note = tx("settings.note.restart_failed");
        refresh();
    }
}

/* ---- the menu bar (mockup 46) ---- */
/* plain text put into the bar's markup: a translation may hold & or < (RmlUi would read them as markup) */
std::string rml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

std::string bar_item(const char* id, const std::string& text, const std::string& right, bool check = false) {
    return std::string("<div class='mi' id='") + id + "'><span class='mck'>" + (check ? "&#10003;" : "") + "</span>" +
           text + "<span class='mr'>" + right + "</span></div>";
}

void bar_rml() {
    if (s_doc == nullptr) return;
    if (s_bar_menu != BAR_CHEATS) s_bar_sub = -1; /* a side list belongs to the open Cheats menu */
    if (Rml::Element* body = s_doc->GetElementById("body")) {
        body->SetClass("baronly", s_bar && !s_open);
        body->SetClass("aboutopen", s_about && !s_open);
    }
    /* where the open menu goes: under its name, read from the bar as last laid out (before it is rebuilt below), so
     * it sits right in every language, whatever the names' widths */
    /* An element made by the last rebuild has no layout yet (it reads 0) when two refreshes come in one frame, so the
     * last real positions are kept: every name and row sits well away from 0 */
    static float s_menu_left[BAR_MENUS] = {0.0f, 0.0f, 0.0f};
    static float s_row_top[2] = {0.0f, 0.0f};
    float drop_left = 10.0f;
    float sub_top = -1.0f;
    for (int i = 0; i < BAR_MENUS; ++i) {
        if (Rml::Element* m = s_doc->GetElementById("mb-" + std::to_string(i)); m != nullptr && m->GetAbsoluteLeft() > 0.5f) {
            s_menu_left[i] = m->GetAbsoluteLeft();
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (Rml::Element* row = s_doc->GetElementById(i == 0 ? "mi-exp" : "mi-put");
            row != nullptr && row->GetAbsoluteTop() > 0.5f) {
            s_row_top[i] = row->GetAbsoluteTop();
        }
    }
    if (s_bar_menu >= 0 && s_menu_left[s_bar_menu] > 0.0f) drop_left = s_menu_left[s_bar_menu];
    if (s_bar_menu == BAR_CHEATS && s_bar_sub >= 0 && s_row_top[s_bar_sub] > 0.0f) sub_top = s_row_top[s_bar_sub];
    if (Rml::Element* e = s_doc->GetElementById("mbar")) {
        std::string h;
        const char* names[BAR_MENUS] = {"menu.file", "menu.cheats", "menu.help"};
        for (int i = 0; i < BAR_MENUS; ++i) {
            h += std::string("<span class='mb") + (s_bar_menu == i ? " open" : "") + "' id='mb-" + std::to_string(i) +
                 "'>" + rml_escape(tx(names[i])) + "</span>";
        }
        /* *Removed 2026-11-13:* the "CHEATS ON · N" tag at the bar's right end (snatcherLeo: "we don't need that") */
        e->SetInnerRML(h);
    }
    if (Rml::Element* e = s_doc->GetElementById("msub")) {
        /* the side list beside an open Cheats row (EXP's choices, the items to put in the PC) */
        std::string d;
        if (s_bar_menu == BAR_CHEATS && s_bar_sub == 0) {
            const int times[] = {1, 2, 5, 10};
            for (int t : times) {
                const std::string name =
                    t == 1 ? tx("cheats.exp.normal") : tx("cheats.exp.times", {{"n", std::to_string(t)}});
                d += std::string("<div class='mi' id='mi-exp-") + std::to_string(t) + "'><span class='mck'>" +
                     (pc_cheat_exp_multiplier() == t ? "&#9679;" : "") + "</span>" + rml_escape(name) + "</div>";
            }
        } else if (s_bar_menu == BAR_CHEATS && s_bar_sub == 1) {
            d = "<div class='mhd2'>" + rml_escape(tx("cheats.put_in_pc.header")) + "</div>";
            /* a check mark once an item is in (snatcherLeo, 2026-11-13: the list stays open to stock up on several) */
            for (int i = 0; i < pc_cheat_put_count(); ++i) {
                d += bar_item(("mi-put-" + std::to_string(i)).c_str(), rml_escape(tx(pc_cheat_put_key(i))),
                              "<span class='msub'>" + rml_escape(tx("cheats.to_99")) + "</span>",
                              pc_cheat_put_mark(i) == 1);
            }
        }
        if (s_bar_menu != BAR_CHEATS || s_bar_sub != 1) pc_cheat_put_clear_marks(); /* the list closed: marks go */
        e->SetInnerRML(d);
        e->SetClass("show", !d.empty() && sub_top >= 0.0f && (!s_open || !s_report));
        e->SetClass("wide", s_bar_sub == 1);
        if (sub_top >= 0.0f) {
            /* beside the Cheats menu (its width is fixed in settings.rcss: 360dp), level with the row */
            Rml::Context* context = aurora::rmlui::get_context();
            const float ratio = context != nullptr ? context->GetDensityIndependentPixelRatio() : 1.0f;
            e->SetProperty("left", std::to_string(drop_left + 364.0f * ratio) + "px");
            e->SetProperty("top", std::to_string(sub_top - 7.0f * ratio) + "px");
        }
    }
    if (Rml::Element* e = s_doc->GetElementById("mdrop")) {
        std::string d;
        if (s_bar_menu == BAR_CHEATS) {
            /* mockup 63 v2 (snatcherLeo, 2026-11-13): the switches, EXP and Put in the PC, the debug menu at the bottom */
            const bool allowed = pc_cheats_allowed() != 0;
            /* (one header: cheats are allowed in accurate mode too since 2026-11-20, pc_cheats.c; its "Cheats are off
             * in GameCube-accurate mode" line was removed 2026-11-24 as it could never show) */
            d = "<div class='mhd'>" + rml_escape(tx("cheats.header")) + "</div>";
            if (allowed) {
                for (int i = 0; i < PC_CHEAT_SWITCH_COUNT; ++i) {
                    d += bar_item(("mi-cheat-" + std::to_string(i)).c_str(), rml_escape(tx(pc_cheat_name_key(i))),
                                  i == PC_CHEAT_INSTANT_WIN
                                      ? "<span class='msub'>" + rml_escape(tx("cheats.instant_win.sub")) + "</span>"
                                      : std::string(),
                                  pc_cheat_on(i) != 0);
                }
                const int exp = pc_cheat_exp_multiplier();
                d += bar_item("mi-exp", rml_escape(tx("cheats.exp")),
                              "<span class='msub'>" +
                                  rml_escape(exp > 1 ? tx("cheats.exp.times", {{"n", std::to_string(exp)}})
                                                     : tx("cheats.exp.off")) +
                                  "</span><span class='marr'>&#8250;</span>",
                              exp > 1) +
                     "<div class='msep'></div>" +
                     bar_item("mi-put", rml_escape(tx("cheats.put_in_pc")),
                              "<span class='msave'>" + rml_escape(tx("cheats.changes_save")) +
                                  "</span><span class='marr'>&#8250;</span>");
            }
            /* only while Settings › General › Debug tools is on (pc_debug.c says why); at the bottom of Cheats (snatcherLeo,
             * 2026-11-13: "under that, users can access debug menu in there somewhere") */
            if (pc_debug_enabled()) {
                d += "<div class='msep'></div>" +
                     bar_item("mi-debug", rml_escape(tx("menu.debug")), "<span class='kk'>F1</span>");
            }
        } else if (s_bar_menu == BAR_FILE) {
            d = bar_item("mi-full", tx("menu.fullscreen"), "<span class='kk'>F11</span><span class='kk'>Alt+Enter</span>",
                         pc_platform_is_fullscreen() != 0) +
                bar_item("mi-folder", tx("menu.open_folder"), "") + "<div class='msep'></div>" +
                bar_item("mi-settings", tx("menu.settings"), std::string("<span class='kk'>") + pc_controls_menu_name(1) + "</span>") +
                /* the controller's Quit to hub shortcut, when one is set (snatcherLeo, 2026-11-13: "if the controller and
                 * profile selected DOES have a shortcut for quitting to hub, that shortcut would then have to get
                 * reflected in the top toolbar"); none by default, so usually no chip */
                bar_item("mi-quit", tx("settings.quit"),
                         std::strcmp(pc_quit_shortcut_name(), "None") != 0
                             ? std::string("<span class='kk'>") + pc_quit_shortcut_name() + "</span>"
                             : std::string());
        } else if (s_bar_menu == BAR_HELP) {
            /* *Updated 2026-11-13:* the debug menu moved to the bottom of Cheats (mockup 63 v2) */
            d = bar_item("mi-report", tx("menu.report"), "<span class='kk'>F8</span>") + "<div class='msep'></div>" +
                bar_item("mi-about", tx("menu.about"), "");
        }
        e->SetInnerRML(d);
        e->SetClass("show", s_bar_menu >= 0 && (!s_open || !s_report)); /* over the open side menu too */
        e->SetClass("cheats", s_bar_menu == BAR_CHEATS);
        e->SetProperty("left", std::to_string(drop_left) + "px");
    }
    if (Rml::Element* e = s_doc->GetElementById("about"); e != nullptr && s_about) {
        /* the version, and the credits a release needs (docs/licensing.md) */
        /* the credits' sentences in the player's language; the names in them (Dolphin, Aurora, ...) stay as they are */
        e->SetInnerRML(std::string("<span class='at'>OrrePort</span><span class='av'>") + pc_debug_build_stamp() +
                       "</span><span class='ah'>" + tx("about.thanks") + "</span><span class='al'>" + tx("about.dolphin") +
                       "</span><span class='al'>" + tx("about.libraries") + "</span><span class='al'>" + tx("about.freetype") +
                       /* FreeType's credit (2026-11-16): its licence, the FTL, asks for this line in the documentation */
                       "</span><span class='al'>" + tx("about.decomp") +
                       "</span><span class='al'>" + tx("about.fonts") + "</span><span class='an'>" +
                       tx("about.rights") + "</span><span class='btn' id='about-close'>" + tx("about.close") + "</span>");
    }
}

void bar_show(bool on) {
    if (s_doc == nullptr || on == s_bar) return;
    s_bar = on;
    if (!on) {
        s_bar_menu = -1;
        s_about = false;
    }
    if (!s_open) {
        if (on) {
            s_doc->Show();
        } else if (s_toast_until == 0 && s_quit_fade_since == 0) {
            s_doc->Hide();
        }
    }
    refresh();
}

void bar_action(const std::string& id) {
    /* the Cheats menu (mockup 63 v2): switches stay in the open menu, so the check mark is seen to change; the side
     * lists open beside their row; a choice in a side list closes the menu */
    if (id.rfind("mi-cheat-", 0) == 0) {
        const int which = std::atoi(id.c_str() + 9);
        pc_cheat_set(which, !pc_cheat_on(which));
        s_bar_sub = -1;
        refresh();
        return;
    }
    if (id == "mi-exp" || id == "mi-put") {
        const int sub = id == "mi-exp" ? 0 : 1;
        s_bar_sub = s_bar_sub == sub ? -1 : sub;
        refresh();
        return;
    }
    if (id.rfind("mi-exp-", 0) == 0) {
        pc_cheat_set_exp_multiplier(std::atoi(id.c_str() + 7));
        s_bar_sub = -1;
        refresh();
        return;
    }
    if (id.rfind("mi-put-", 0) == 0) {
        /* done on the game thread; its check mark follows (overlay frame). The list stays open (snatcherLeo, 2026-11-13) */
        pc_cheat_put_in_pc(std::atoi(id.c_str() + 7));
        return;
    }
    s_bar_menu = -1;
    if (id == "mi-full") {
        pc_platform_toggle_fullscreen();
    } else if (id == "mi-folder") {
        char path[1100];
        const char* folder = pc_platform_data_path("", path, sizeof(path));
        std::string url = folder[0] == '/' ? "file://" : "file:///"; /* Linux / Mac paths start with '/' */
        url += folder;
        for (char& c : url) {
            if (c == '\\') c = '/';
        }
        SDL_OpenURL(url.c_str()); /* the system's file browser */
    } else if (id == "mi-settings") {
        bar_show(false);
        set_open(true, "the menu bar");
        s_full = true; /* the full Settings window, on the category column */
        s_cats = true;
        refresh();
        return;
    } else if (id == "mi-quit") {
        bar_show(false);
        request_quit_to_hub(); /* the same question (or "Don't ask again") as the shortcut */
        return;
    } else if (id == "mi-report") {
        /* *Corrected 2026-11-13:* this opened the report without opening the menu, so with the bar then hidden the
         * report stayed live but unseen, and came back the next time the bar showed (snatcherLeo: "now, the report shows up
         * again, making me realize it never got canceled out") */
        const bool alone = !s_open;
        bar_show(false);
        if (!s_open) set_open(true, "Help > Report a problem");
        open_report(alone);
        return;
    } else if (id == "mi-debug") {
        bar_show(false);
        if (s_open) set_open(false, "Help > Debug menu");
        pc_debug_ui_toggle();
        return;
    } else if (id == "mi-about") {
        if (s_open) { /* from the bar over the open side menu: About shows on its own, the menu closed (as before) */
            set_open(false, "About OrrePort");
            s_bar = true;
            s_doc->Show();
        }
        s_about = true;
    }
    std::printf("[settings_overlay] menu bar: %s\n", id.c_str());
    refresh();
}

/* the full window's QUIT TO HUB: its question as a dialog, or straight out when "Ask before Quit to hub" is off (or the
 * memory card was just used: then the side panel's note says why it waits, as from the panel) */
void full_quit_pressed() {
    if (pc_settings_int("quit_hub_ask", 1) != 0) {
        s_fq = true;
        s_fqFocus = 0; /* on Cancel */
        s_fqNoAsk = false;
        refresh();
        return;
    }
    quit_to_hub();
}

/* the dialog's answer: 1 Quit to hub (with Don't ask again kept), 0 Cancel */
void full_quit_answer(bool quit) {
    if (!quit) {
        s_fq = false;
        refresh();
        return;
    }
    if (s_fqNoAsk) { /* "Don't ask again": General > Ask before Quit to hub turns it back on */
        pc_settings_set_int("quit_hub_ask", 0);
        if (RowDef* ask = live_row(LIVE_QUITASK)) ask->value = 0;
    }
    quit_to_hub(); /* the dialog stays on screen through the fade */
}

/* Go back's pop-up answered: Cancel closes it; Go back asks the updater (pc_update_request_rollback), then in the hub
 * OrrePort exits with the update code so its launcher swaps both programs back and starts it again; in game it waits
 * for the next start (update.go_back_next says so) */
void gb_answer(bool go) {
    const PcUpdateStatus st = about_status();
    /* in the hub only (it restarts now, and the launcher's swap can't replace files Studio has open); in game Go back
     * waits for the next start anyway */
    if (go && s_in_hub && about_studio_open()) { /* the button reads Close Studio: ask it to close, the pop-up stays */
        about_close_studio();
        refresh();
        return;
    }
    s_gb = false;
    if (go && pc_update_request_rollback()) {
        std::printf("[settings_overlay] About: Go back to %s\n", st.previous);
        if (s_in_hub) about_exit_for_update();
        s_note = tx("update.go_back_next", {{"version", st.previous}});
    } else if (!go) {
        std::printf("[settings_overlay] About: Go back cancelled\n");
    }
    refresh();
}

void choose() {
    if (s_what) { /* A closes "What it turns off" (its one button, Back) */
        s_what = false;
        refresh();
        return;
    }
    if (s_gb) { /* A in Go back's pop-up */
        gb_answer(s_gbFocus == 1);
        return;
    }
    if (s_fq) { /* A in the dialog */
        if (s_fqFocus == 2) {
            s_fqNoAsk = !s_fqNoAsk;
            refresh();
        } else {
            full_quit_answer(s_fqFocus == 1);
        }
        return;
    }
    if (page() && s_cats && s_colQuit) { /* A on the column's QUIT TO HUB */
        full_quit_pressed();
        return;
    }
    if (page() && s_cats && s_colBack) { /* A on the column's BACK: as a click on it */
        set_open(false, "Back");
        return;
    }
    if (page() && s_cats) { /* A on a category: into its rows, on the first one */
        s_cats = false;
        s_focus = 0;
        s_note.clear();
        refresh();
        return;
    }
    const std::vector<Item> list = items();
    if (list.empty()) {
        return;
    }
    const Item& it = list[s_focus];
    switch (it.kind) {
    case IT_CAT:
        s_tab = it.index;
        s_level = 1;
        s_focus = 0;
        s_note.clear();
        refresh();
        return;
    case IT_FULL: /* this time only: the saved choice (General > In-game menu) is kept for the next opening */
        s_full = true;
        s_cats = true;
        s_focus = 0;
        s_note.clear();
        std::printf("[settings_overlay] the full Settings window (from the panel's button)\n");
        refresh();
        return;
    case IT_RESUME:
        set_open(false, "Back to the game");
        return;
    case IT_REPORT:
        if (s_quit_ask) { /* Cancel */
            quit_ask_end();
            return;
        }
        open_report(false); /* from the menu: closing it goes back to the menu */
        return;
    case IT_NOASK:
        s_quit_noask = !s_quit_noask;
        refresh();
        return;
    case IT_QUIT:
        if (!s_quit_ask && pc_settings_int("quit_hub_ask", 1) != 0) {
            s_quit_ask = true;
            s_quit_noask = false;
            const std::vector<Item> asked = items();
            for (int k = 0; k < (int)asked.size(); ++k) {
                if (asked[k].kind == IT_REPORT) s_focus = k; /* on Cancel */
            }
            refresh();
            return;
        }
        if (s_quit_ask && s_quit_noask) { /* "Don't ask again": General > Ask before Quit to hub turns it back on */
            pc_settings_set_int("quit_hub_ask", 0);
            if (RowDef* ask = live_row(LIVE_QUITASK)) ask->value = 0;
        }
        quit_to_hub();
        return;
    case IT_ROW:
        break;
    }
    RowDef& r = s_rows[it.index];
    if (r.locked) { /* accurate mode's lock (mode_values): the row already says why */
        refresh();
        return;
    }
    if (r.live == LIVE_ACC_WHAT || r.live == LIVE_ADDS) { /* "What it turns off" (2.12) or "What OrrePort adds" (5.13):
                                                            * its list over the page, in the same window */
        s_what = true;
        s_what_adds = r.live == LIVE_ADDS;
        refresh();
        return;
    }
    if (r.live == LIVE_FF_LINK) { /* the Fast-forward row's link: Controls, on its Fast-forward row, for that device */
        pc_controls_ui_open_ff(s_ctrl == CTRL_KEYBOARD ? 1 : 0);
        return;
    }
    if (r.kind == SELECT) {
        if (s_drop < 0) { /* open the list on the current choice */
            s_drop = r.value;
            refresh();
            return;
        }
        const int pick = s_drop;
        s_drop = -1;
        if (pick != r.value) {
            r.value = pick;
            apply_live(r);
        } else {
            refresh();
        }
    } else if (r.kind == TOGGLE) {
        r.value = !r.value;
        apply_live(r);
    } else if (r.kind == OPEN && r.live == LIVE_DISCLABEL) {
        if (s_in_hub && pc_hub_disc_ready() == 0) {
            /* no disc yet: disc art goes on a disc, so say to add one first and stay here (snatcherLeo, 2026-11-13: "with no
             * disc inserted, you can't change disc art ... we give an error message to insert disc first"); before,
             * Settings closed and nothing opened */
            s_note = s_note_err = tx("settings.note.disc_art_no_disc");
            refresh();
        } else if (s_in_hub) {
            set_open(false, "Disc label");
            pc_hub_open_labels(); /* the hub's disc label page, in place of this panel */
        } else {
            s_note = tx("settings.note.disc_art_in_hub");
            refresh();
        }
    } else if (r.kind == OPEN && r.live == LIVE_LANGDISCS) {
        if (s_in_hub && pc_hub_disc_ready() == 0) {
            s_note = s_note_err = tx("settings.note.lang_discs_no_disc"); /* the panel sits on the main page with a disc */
            refresh();
        } else if (s_in_hub) {
            set_open(false, "Language discs");
            pc_hub_open_langdiscs(); /* the hub's Language discs panel, unfolded */
        } else {
            s_note = tx("settings.note.lang_discs_in_hub");
            refresh();
        }
    } else if (r.kind == OPEN && r.live == LIVE_ADAPTER) {
        /* the one-click adapter setup: Windows asks once, then the setup program swaps the driver (pc_adapter_setup.c);
         * its result comes back in the frame below */
        if (s_adapter_running) return;
        const int started = pc_adapter_setup_start();
        s_note_err.clear();
        if (started == PC_ADAPTER_STARTED) {
            s_adapter_running = true;
            s_adapter_shown = true;
            s_adapter_started = SDL_GetTicks();
            s_note.clear(); /* the busy screen says it while it runs */
            if (s_doc != nullptr) {
                if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("busy", true);
            }
        } else if (started == PC_ADAPTER_DECLINED) {
            s_note = tx("settings.note.adapter_declined");
        } else {
            s_note = s_note_err = tx(started == PC_ADAPTER_NO_HELPER ? "settings.note.adapter_missing"
                                                                      : "settings.note.adapter_failed");
        }
        refresh();
    } else if (r.kind == ACTION && r.live == LIVE_UPD_VERSION) {
        about_button_press();
    } else if (r.kind == ACTION && r.live == LIVE_UPD_BACK) {
        /* Go back: *changed 2026-11-23 (snatcherLeo: a pop-up that asks "are you sure", with a warning; "Yes use it"):* it
         * opens the pop-up (gb_answer does the rest); Studio open blocks it as before (the swap can't replace its
         * files) */
        /* (with Studio open, the pop-up says so and offers Close Studio in Go back's place, as Studio's does for
         * OrrePort; it turns into Go back once Studio has closed) */
        const PcUpdateStatus st = about_status();
        s_gb = true;
        s_gbFocus = 0; /* on Cancel */
        std::printf("[settings_overlay] About: Go back to %s? (the pop-up%s)\n", st.previous,
                    about_studio_open() ? ", Studio is open" : "");
        refresh();
    } else if (r.kind == ACTION && r.live == LIVE_LOGS) {
        /* the logs folder (both programs write theirs there: OrrePort's orreport_*, Studio's studio-*), in the system's
         * file browser. Windows / Linux / macOS through SDL_OpenURL with a file:// address, as the menu's Open folder */
        char dir[1300];
        std::string url = pc_paths_dir(PC_PLACE_LOGS, dir, sizeof dir);
        url = (url[0] == '/' ? "file://" : "file:///") + url;
        for (char& c : url) {
            if (c == '\\') c = '/';
        }
        SDL_OpenURL(url.c_str());
        std::printf("[settings_overlay] About: Open the logs folder\n");
    } else if (r.kind == OPEN) {
        pc_controls_ui_open(); /* Controls > Set up controller: its own screen over this one */
    }
}

void back() {
    if (s_what) { /* B closes "What it turns off" */
        s_what = false;
        refresh();
        return;
    }
    if (s_gb) { /* B cancels Go back's pop-up */
        gb_answer(false);
    } else if (s_fq) { /* B cancels the full window's Quit to hub dialog */
        full_quit_answer(false);
    } else if (s_drop >= 0) {
        s_drop = -1;
        refresh();
    } else if (s_quit_ask) { /* B cancels Quit to hub's question */
        quit_ask_end();
    } else if (page() && !s_cats) { /* out of the category's rows, back onto its name in the column */
        s_cats = true;
        s_note.clear();
        refresh();
    } else if (!page() && s_level == 1) { /* back to the first screen, on the category just left */
        const int tab = s_tab;
        s_level = 0;
        const std::vector<Item> list = items();
        for (int k = 0; k < (int)list.size(); ++k) {
            if (list[k].kind == IT_CAT && list[k].index == tab) {
                s_focus = k;
            }
        }
        s_note.clear();
        refresh();
    } else {
        set_open(false, "B");
    }
}

void category(int d) {
    if (s_drop >= 0 || (!page() && s_level == 0)) {
        return;
    }
    set_tab(tab_at(tab_pos(s_tab) + d)); /* L / R: the next category in the shown order, round at the ends */
}

void reset_default() {
    const std::vector<Item> list = items();
    if (s_drop >= 0 || list.empty() || list[s_focus].kind != IT_ROW || (page() && s_cats)) {
        return;
    }
    const int i = list[s_focus].index;
    RowDef& r = s_rows[i];
    if (r.locked) return; /* accurate mode's lock */
    if (i < (int)s_defaults.size() && r.value != s_defaults[i] &&
        (r.kind == SELECT || r.kind == TOGGLE || r.kind == SLIDER)) {
        r.value = s_defaults[i];
        apply_live(r);
    }
}

/* a row's new value: saved and applied where it takes effect at once */
void apply_live(RowDef& r) {
    if (r.live == LIVE_WINDOW) {
        pc_platform_toggle_fullscreen();
    } else if (r.live == LIVE_PAUSE) {
        pc_settings_set_int("overlay_pause", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_ANIM) {
        pc_settings_set_int("ui_animations", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_GAMEICON) {
        pc_settings_set_int("ui_window_icon", r.value);
        pc_settings_save();
        pc_window_icon_game(r.value == 1 ? 1 : 0); /* at once, in the hub too */
    } else if (r.live == LIVE_THEME) {
        pc_settings_set_int("ui_appearance", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_MENUSIZE) {
        pc_settings_set_int("menu_full", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_MENUHINT) {
        pc_settings_set_int("ui_menu_hint", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_DEBUG) { /* read at the next start (the row says so) */
        pc_settings_set_int("debug_tools", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_QUITASK) {
        pc_settings_set_int("quit_hub_ask", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_LANG) { /* read at the next start (the row says so) */
        pc_settings_set_string("language", r.value > 0 && r.value - 1 < (int)s_lang_codes.size()
                                               ? s_lang_codes[r.value - 1].c_str()
                                               : "auto");
        pc_settings_save();
    } else if (r.live == LIVE_SPEED_GAME || r.live == LIVE_SPEED_BATTLE || r.live == LIVE_SPEED_SOUND) {
        /* live: pc_speed.c reads them at the next frame. Game speed and Battles save the choice's code, not its row
         * position (4× sits between 3× and 5× in the row but has the newest code; pc_speed.h) */
        const int code = r.live == LIVE_SPEED_GAME     ? pc_speed_code_for_row(PC_SPEED_LIST_GAME, r.value)
                         : r.live == LIVE_SPEED_BATTLE ? pc_speed_code_for_row(PC_SPEED_LIST_BATTLE, r.value)
                                                       : r.value;
        pc_settings_set_int(r.live == LIVE_SPEED_GAME     ? "speed_game"
                            : r.live == LIVE_SPEED_BATTLE ? "speed_battle"
                                                          : "speed_sound",
                            code >= 0 ? code : 0);
        pc_settings_save();
    } else if (r.live == LIVE_FF_SPEED || r.live == LIVE_FF_MODE) { /* live too (pc_speed.c logs each change) */
        if (r.live == LIVE_FF_SPEED) { /* the choice's code, as above */
            const int code = pc_speed_code_for_row(PC_SPEED_LIST_FF, r.value);
            pc_settings_set_int("speed_ff", code >= 0 ? code : PC_SPEED_FF_DEFAULT);
        } else {
            pc_settings_set_int("speed_ff_mode", r.value);
        }
        pc_settings_save();
    } else if (r.live == LIVE_FF_BADGE) { /* the row's 0 is Show; settings.ini speed_badge 1 = shown */
        pc_settings_set_int("speed_badge", r.value == 0 ? 1 : 0);
        pc_settings_save();
    } else if (r.live == LIVE_OPTION) { /* a Tweaks switch: saved by the options system (settings.ini, its own key) */
        pc_option_set(r.options[0].c_str(), r.value);
    } else if (r.live == LIVE_ACC_DEFAULT) { /* the default for new saves (2.12): pc_options.c saves it */
        pc_option_set_accurate_new_saves(r.value);
    } else if (r.live == LIVE_IMPORT_ASK) { /* the import question on / off (3.5; pc_hub_saves.cpp reads it) */
        pc_settings_set_int("import_ask", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_UPD_CHECK || r.live == LIVE_UPD_AUTO) {
        /* the update switches (4.4): saved for both programs; the automatic one applies at once, and turning
         * checking on looks for an update now (it's off = fully offline from the next start) */
        pc_settings_set_int(r.live == LIVE_UPD_CHECK ? "update_check" : "update_auto_download", r.value);
        pc_settings_save();
        if (r.live == LIVE_UPD_AUTO) pc_update_set_auto_download(r.value);
        else if (r.value) pc_update_check(1);
    } else if (r.live == LIVE_MENUBAR) {
        pc_settings_set_int("menu_bar_always", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_MENUBG) {
        pc_settings_set_int("ui_menu_solid", r.value);
        pc_settings_save();
    } else if (r.live == LIVE_BACKEND) {
        pc_settings_set_string("gpu_backend", backend_code(r.value));
        pc_settings_save();
    } else if (r.live == LIVE_RES) {
        pc_settings_set_int("render_scale", r.value); /* 0 the window's size, 1-8 times 640 x 480 */
        pc_settings_save();
        VISetFrameBufferScale((float)r.value);
    } else if (r.live == LIVE_VSYNC) {
        pc_settings_set_int("vsync", r.value);
        pc_settings_save();
        aurora_enable_vsync(r.value != 0);
    } else if (r.live == LIVE_FRAME_RATE) { /* live: pc_smooth.c follows it from the next game step (step 3.4) */
        pc_settings_set_string("frame_smoothing", frame_rate_code(r).c_str());
        pc_settings_save();
        pc_smooth_setting_changed();
        std::printf("[settings_overlay] Frame rate: %s (settings.ini frame_smoothing)\n", frame_rate_code(r).c_str());
    } else if (r.live == LIVE_MSAA) {
        pc_settings_set_int("msaa", r.value == 1 ? 4 : 1);
        pc_settings_save();
    } else if (r.live == LIVE_ANISO) {
        pc_settings_set_int("anisotropy", 1 << r.value); /* 1, 2, 4, 8, 16 */
        pc_settings_save();
    } else if (r.live == LIVE_VOL_MASTER || r.live == LIVE_VOL_MUSIC || r.live == LIVE_VOL_EFFECTS) {
        pc_settings_set_int(r.live == LIVE_VOL_MASTER  ? "volume_master"
                            : r.live == LIVE_VOL_MUSIC ? "volume_music"
                                                       : "volume_effects",
                            r.value * 10);
        pc_settings_save();
        apply_volumes();
    }
    refresh();
}

/* a dragged slider's value from the mouse's x on its track (taken as the drag began) */
void drag_to(float x) {
    const std::vector<Item> list = items();
    if (s_drag < 0 || s_drag >= (int)list.size() || list[s_drag].kind != IT_ROW) {
        return;
    }
    RowDef& r = s_rows[list[s_drag].index];
    const float t = s_drag_width > 0.f ? SDL_clamp((x - s_drag_left) / s_drag_width, 0.f, 1.f) : 1.f;
    const int v = (int)(t * ((int)r.options.size() - 1) + 0.5f);
    if (v != r.value) {
        r.value = v;
        apply_live(r);
    }
}

/* the mouse on sliders (snatcherLeo, 2026-11-11: "the sliders ... should be draggable"): pressing on a slider's track takes
 * the value under the mouse, moving with the button held follows it, letting go ends the drag */
class Drags : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        const float x = event.GetParameter<float>("mouse_x", -1.f);
        if (event.GetId() == Rml::EventId::Mouseup) {
            s_drag = -1;
            return;
        }
        if (event.GetId() == Rml::EventId::Mousemove) {
            drag_to(x);
            return;
        }
        for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) { /* Mousedown */
            const std::string id = e->GetId();
            if (id.rfind("item-", 0) != 0) {
                continue;
            }
            const int k = std::atoi(id.c_str() + 5);
            const std::vector<Item> list = items();
            Rml::Element* track = e->QuerySelector(".track");
            if (k < (int)list.size() && list[k].kind == IT_ROW && s_rows[list[k].index].kind == SLIDER &&
                track != nullptr && x >= track->GetAbsoluteLeft() - 12.f) {
                s_cats = false;
                s_focus = k;
                s_drop = -1;
                s_drag = k;
                s_drag_left = track->GetAbsoluteLeft();
                s_drag_width = track->GetClientWidth();
                drag_to(x);
                refresh();
            }
            return;
        }
    }
};
Drags s_drags;

/* mouse clicks: a category or a row is chosen as with A; a drop-down's option is picked; a click elsewhere closes an
 * open drop-down */
class Clicks : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        /* with the side menu open, a click anywhere but the bar and its menu closes the bar's open menu (the closed
         * menu's own mouse code does this for the bar alone, on_event) */
        if (s_open && s_bar_menu >= 0) {
            bool on_bar = false;
            for (Rml::Element* e = event.GetTargetElement(); e != nullptr && !on_bar; e = e->GetParentNode()) {
                on_bar = e->GetId() == "mbar" || e->GetId() == "mdrop" || e->GetId() == "msub";
            }
            if (!on_bar) {
                s_bar_menu = -1;
                refresh();
            }
        }
        if (s_gb) { /* Go back's pop-up takes every click: its two buttons, and nothing under it */
            for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
                if (e->GetId() == "gb-cancel" || e->GetId() == "gb-go") {
                    gb_answer(e->GetId() == "gb-go");
                    break;
                }
            }
            return;
        }
        if (s_what) { /* "What it turns off": its Back button or a click outside it closes it; inside, nothing else */
            bool inside = false;
            for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
                if (e->GetId() == "what-close") break;
                if (e->GetId() == "whatdlg") inside = true;
            }
            if (!inside) {
                s_what = false;
                refresh();
            }
            return;
        }
        for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
            const std::string id = e->GetId();
            if (id == "mb-0" || id == "mb-1" || id == "mb-2") { /* the menu bar: a menu's name opens or closes it */
                const int m = id[3] - '0';
                s_bar_menu = s_bar_menu == m ? -1 : m;
                s_about = false;
                refresh();
                return;
            }
            if (id.rfind("mi-", 0) == 0) {
                bar_action(id);
                return;
            }
            if (id == "about-close") {
                s_about = false;
                refresh();
                return;
            }
            if (id.rfind("tab-", 0) == 0) {
                s_cats = true;
                set_tab(std::atoi(id.c_str() + 4));
                return;
            }
            if (id.rfind("opt-", 0) == 0 && s_drop >= 0) {
                s_drop = std::atoi(id.c_str() + 4);
                choose(); /* picks it */
                return;
            }
            if (id == "pback") {
                back();
                return;
            }
            if (id.rfind("item-", 0) == 0) {
                const int k = std::atoi(id.c_str() + 5);
                const std::vector<Item> list = items();
                if (k >= (int)list.size()) {
                    return;
                }
                s_cats = false; /* a click on a row is inside its category */
                if (s_drop >= 0) { /* a click on the open drop-down's row (or another) closes it */
                    s_drop = -1;
                    s_focus = k;
                    refresh();
                    return;
                }
                s_focus = k;
                if (list[k].kind == IT_ROW && s_rows[list[k].index].kind == SLIDER) {
                    refresh(); /* the value was set by the press (Drags) */
                    return;
                }
                choose();
                return;
            }
            if (id == "resume") { /* the hub page's BACK */
                if (s_fq) return; /* not under the Quit to hub dialog */
                set_open(false, "Back");
                return;
            }
            if (id == "fquit" && full_in_game() && !s_fq) { /* the full window's QUIT TO HUB */
                s_cats = true;
                s_colQuit = true;
                s_colBack = false;
                full_quit_pressed();
                return;
            }
            if (s_fq && (id == "fq-cancel" || id == "fq-quit")) {
                full_quit_answer(id == "fq-quit");
                return;
            }
            if (s_fq && id == "fq-noask") {
                s_fqFocus = 2;
                s_fqNoAsk = !s_fqNoAsk;
                refresh();
                return;
            }
            if (id == "hubgear") { /* the hub's gear again: back to the hub, as BACK (snatcherLeo, 2026-11-11) */
                set_open(false, "the gear");
                return;
            }
            if (id == "cancel") {
                close_report("Cancel");
                return;
            }
            if (id == "savebtn") {
                s_report_save = 1; /* saved by the frame function once the picture is in */
                return;
            }
            if (id == "ustgo") { /* About's green status line: the same as the version row's button */
                about_button_press();
                return;
            }
            if (id == "restartnow" && s_in_hub) { /* the Renderer row's "Restart now" (row_sub) */
                restart_for_renderer();
            }
            if (id == "checknow") { /* About's "Check now": the manual check, past the three-hour wait */
                pc_update_check(2);
                std::printf("[settings_overlay] About: Check now\n");
                refresh();
                return;
            }
            if (id == "rdone") {
                finish_report("Done");
                return;
            }
            if (id == "ropen") {
                report_open_folder();
                return;
            }
        }
        if (s_drop >= 0) { /* a click outside the list */
            s_drop = -1;
            refresh();
        }
    }
};
Clicks s_clicks;

/* A held direction repeats (snatcherLeo, 2026-11-11: "users don't want to keep clicking"): the move happens at once, then,
 * still held after 0.4 s, again every 75 ms (frame_common). Directions: 1 up, 2 down, 3 left, 4 right. */
void step_direction(int dir) {
    switch (dir) {
    case 1: nav(-1); break;
    case 2: nav(1); break;
    case 3: adjust(-1); break;
    case 4: adjust(1); break;
    default: break;
    }
}

/* dir: the direction held now (0: none); released: for a D-pad button let go, which one (-1: the stick, whose 0
 * always ends the hold) */
void hold_direction(int dir, int released) {
    if (dir == 0) {
        if (released == -1 || released == s_hold_dir) {
            s_hold_dir = 0;
        }
        return;
    }
    if (dir == s_hold_dir) {
        return;
    }
    s_hold_dir = dir;
    s_hold_next = SDL_GetTicks() + 400;
    step_direction(dir);
}

bool is_gamecube_pad(const input::InputSource& source) {
    const SDL_JoystickID joystick = input::gamepad_for_source(source.id);
    const char* name = joystick != 0 ? SDL_GetGamepadNameForID(joystick) : nullptr;
    return name != nullptr && SDL_strcasestr(name, "gamecube") != nullptr;
}

input::EventResult on_event(const input::InputEvent& event, void*) {
    using E = input::InputEvent;
    { /* the controller in hand (v2's footer): a key, or a button on a gamepad, says which */
        Ctrl now = s_ctrl;
        if (const auto* key = event.payload.get_if<E::KeyChanged>(); key != nullptr && key->pressed) {
            now = CTRL_KEYBOARD;
        } else if (const auto* b = event.payload.get_if<E::ButtonChanged>(); b != nullptr && b->pressed) {
            now = ctrl_of_gamepad(input::gamepad_for_source(event.source.id));
        }
        if (now != s_ctrl) {
            s_ctrl = now;
            if (s_open) {
                refresh();
            }
        }
    }
    /* the menu bar while playing (mockup 46): the mouse shows it (on any move, or at the top edge only), RmlUi gets the
     * mouse while it shows; a click off the bar and its menu closes the menu; Esc closes an open menu or About */
    if (!s_open && !s_in_hub && s_doc != nullptr && !pc_platform_automated_run() && s_quit_fade_since == 0 &&
        s_cover_since == 0) { /* (not during a fullscreen switch: the window moving under the mouse isn't a move) */
        const bool always = pc_settings_int("menu_bar_always", 0) != 0; /* Appearance > Always show the menu bar */
        if (const auto* p = event.payload.get_if<E::PointerChanged>()) {
            Rml::Context* context = aurora::rmlui::get_context();
            const float ratio = context != nullptr ? context->GetDensityIndependentPixelRatio() : 1.0f;
            const float y = p->position.y / (ratio > 0 ? ratio : 1.0f); /* dp */
            if (!s_bar && (always || y < 8.0f)) bar_show(true);
            if (s_bar) {
                aurora::rmlui::process_input(event);
                Rml::Element* hover = context != nullptr ? context->GetHoverElement() : nullptr;
                const bool over = hover != nullptr && hover->GetId() != "body";
                if (p->phase == E::PointerChanged::Phase::Down && !over && (s_bar_menu >= 0 || s_about)) {
                    s_bar_menu = -1;
                    s_about = false;
                    refresh();
                }
                if (!always && y > 48.0f && s_bar_menu < 0 && !s_about) bar_show(false); /* the mouse left the top */
                return over ? input::EventResult::Consume : input::EventResult::Pass;
            }
        }
        if (const auto* key = event.payload.get_if<E::KeyChanged>();
            key != nullptr && key->pressed && key->keycode == SDLK_ESCAPE && s_bar && (s_bar_menu >= 0 || s_about)) {
            s_bar_menu = -1;
            s_about = false;
            refresh();
            return input::EventResult::Consume;
        }
    }
    /* the menu bar over the open side menu (refresh's barup / barhover say why): with Always show off, the mouse at the
     * top edge brings it up and leaving the top (no menu of it open) puts it away. Only the showing is decided here;
     * the mouse then goes to RmlUi as for the rest of the menu (below), so a click on File or Help reaches Clicks */
    if (s_open && !s_in_hub && !page() && !s_report && s_doc != nullptr && s_quit_fade_since == 0 &&
        s_cover_since == 0 && pc_settings_int("menu_bar_always", 0) == 0) {
        if (const auto* p = event.payload.get_if<E::PointerChanged>()) {
            Rml::Context* context = aurora::rmlui::get_context();
            const float ratio = context != nullptr ? context->GetDensityIndependentPixelRatio() : 1.0f;
            const float y = p->position.y / (ratio > 0 ? ratio : 1.0f); /* dp */
            if (!s_bar && y < 8.0f) {
                bar_show(true);
            } else if (s_bar && y > 48.0f && s_bar_menu < 0) {
                bar_show(false);
            }
        }
    }
    if (s_open && s_report && s_report_done) {
        /* the saved screen (2026-11-23): A, B, Enter or Esc is Done, Y opens the reports folder, a click outside the
         * box is Done too; the mouse clicks the two buttons */
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat &&
                (key->keycode == SDLK_ESCAPE || key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER)) {
                finish_report("Done");
            }
            return input::EventResult::Consume;
        }
        if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed && (button->button == SDL_GAMEPAD_BUTTON_SOUTH || button->button == SDL_GAMEPAD_BUTTON_EAST))
                finish_report("Done");
            else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_NORTH)
                report_open_folder();
            return input::EventResult::Consume;
        }
        if (event.payload.get_if<E::AxisChanged>() != nullptr) return input::EventResult::Consume;
        aurora::rmlui::process_input(event);
        if (const auto* p = event.payload.get_if<E::PointerChanged>();
            p != nullptr && p->phase == E::PointerChanged::Phase::Down && s_report_done) {
            Rml::Context* context = aurora::rmlui::get_context();
            bool inside = false;
            for (Rml::Element* e = context != nullptr ? context->GetHoverElement() : nullptr; e != nullptr;
                 e = e->GetParentNode())
                inside |= e->GetId() == "report";
            if (!inside) finish_report("a click outside");
        }
        return input::EventResult::Consume;
    }
    if (s_open && s_report) {
        /* Report a problem: typing goes into the text box (RmlUi); Esc or B cancels, A saves; the mouse works too */
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat && key->keycode == SDLK_ESCAPE) {
                close_report("Esc");
                return input::EventResult::Consume;
            }
        } else if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed) {
                if (button->button == SDL_GAMEPAD_BUTTON_SOUTH) {
                    s_report_save = 1;
                } else if (button->button == SDL_GAMEPAD_BUTTON_EAST || button->button == SDL_GAMEPAD_BUTTON_WEST) {
                    close_report("B");
                }
            }
            return input::EventResult::Consume;
        } else if (event.payload.get_if<E::AxisChanged>() != nullptr) {
            return input::EventResult::Consume;
        }
        aurora::rmlui::process_input(event);
        update_note_placeholder();
        /* a click outside the report's box cancels it (snatcherLeo, 2026-11-13: "If I use my mouse to click outside of the
         * window, onto the game window, that should "cancel" the idea of me making a report") */
        if (const auto* p = event.payload.get_if<E::PointerChanged>();
            p != nullptr && p->phase == E::PointerChanged::Phase::Down) {
            Rml::Context* context = aurora::rmlui::get_context();
            bool inside = false;
            for (Rml::Element* e = context != nullptr ? context->GetHoverElement() : nullptr; e != nullptr;
                 e = e->GetParentNode()) {
                if (e->GetId() == "report") {
                    inside = true;
                    break;
                }
            }
            if (!inside) {
                close_report("a click outside");
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        /* F2 opens and closes the menu; the keyboard's shortcut chosen in Controls (Esc by default; a combination is
         * opened by the read filter in game, pc_pad_shape.cpp) closes it too, and Esc always closes it */
        /* F8: Report a problem from anywhere in game (snatcherLeo, 2026-11-13: "Yes, F8"; reserved in Controls) */
        if (key->pressed && !key->repeat && key->keycode == SDLK_F8 && !s_report && !s_in_hub &&
            s_quit_fade_since == 0) {
            const bool alone = !s_open;
            bar_show(false);
            if (!s_open) set_open(true, "F8");
            open_report(alone);
            return input::EventResult::Consume;
        }
        if (key->pressed && !key->repeat) {
            const bool shortcut = pc_shortcut_held(1) != 0;
            if (key->keycode == SDLK_F2 || (s_open && (key->keycode == SDLK_ESCAPE || shortcut)) ||
                (!s_open && shortcut && pc_shortcut_by_event(1))) {
                set_open(!s_open, key->keycode == SDLK_F2 ? "F2" : "the menu key");
                return input::EventResult::Consume;
            }
        }
        if (!s_open) {
            return input::EventResult::Pass;
        }
        if (key->pressed) { /* the keyboard's own repeat moves on while a key is held */
            switch (key->keycode) { /* the arrows, and the number pad's arrows (8 2 4 6) */
            case SDLK_UP:
            case SDLK_KP_8: nav(-1); break;
            case SDLK_DOWN:
            case SDLK_KP_2: nav(1); break;
            case SDLK_LEFT:
            case SDLK_KP_4: adjust(-1); break;
            case SDLK_RIGHT:
            case SDLK_KP_6: adjust(1); break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (!key->repeat) choose();
                break;
            case SDLK_BACKSPACE:
                if (!key->repeat) back();
                break;
            case SDLK_DELETE:
                if (!key->repeat) reset_default();
                break;
            case SDLK_Q:
            case SDLK_PAGEUP: category(-1); break;
            case SDLK_E:
            case SDLK_PAGEDOWN: category(1); break;
            default: break;
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        const bool gc = is_gamecube_pad(event.source);
        /* the controller's shortcut chosen in Controls (View by default; Z + L on a GameCube controller): every member
         * held closes the menu; a single button the game doesn't use opens it here, anything with a GameCube button
         * in it is opened by the read filter in game (pc_pad_shape.cpp: the hold-back) */
        if (button->pressed && pc_shortcut_held(0)) {
            if (s_open || pc_shortcut_by_event(0)) {
                set_open(!s_open, "the menu button");
                return input::EventResult::Consume;
            }
        }
        if (!s_open) {
            return input::EventResult::Pass;
        }
        /* A / B / X by position: the GameCube pad's B is on the west face and its X on the east; other pads have B on
         * the east and X on the west (Xbox, PlayStation's circle and square, the Steam Deck) */
        const SDL_GamepadButton b_button = gc ? SDL_GAMEPAD_BUTTON_WEST : SDL_GAMEPAD_BUTTON_EAST;
        const SDL_GamepadButton x_button = gc ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_WEST;
        const int dir = button->button == SDL_GAMEPAD_BUTTON_DPAD_UP      ? 1
                        : button->button == SDL_GAMEPAD_BUTTON_DPAD_DOWN  ? 2
                        : button->button == SDL_GAMEPAD_BUTTON_DPAD_LEFT  ? 3
                        : button->button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ? 4
                                                                          : 0;
        if (dir != 0) {
            hold_direction(button->pressed ? dir : 0, dir);
            return input::EventResult::Consume;
        }
        if (button->pressed) {
            if (button->button == SDL_GAMEPAD_BUTTON_SOUTH) {
                choose(); /* A */
            } else if (button->button == b_button) {
                back();
            } else if (button->button == x_button) {
                reset_default();
            } else if (button->button == SDL_GAMEPAD_BUTTON_NORTH) { /* Y: Report a problem (in game) */
                if (!s_in_hub) {
                    open_report(false); /* from the menu */
                }
            } else if (button->button == SDL_GAMEPAD_BUTTON_START) {
                set_open(false, "START"); /* back to the game from anywhere */
            } else if (button->button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                category(-1);
            } else if (button->button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER && !gc) {
                category(1); /* on a GameCube pad the right shoulder is Z; its R is the analog trigger, below */
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* axis = event.payload.get_if<E::AxisChanged>()) {
        if (!s_open) {
            return input::EventResult::Pass;
        }
        /* the analog L / R triggers switch categories (a press past halfway) */
        static bool l_down, r_down;
        /* a shortcut with an analog trigger in it (Z + L on a GameCube controller) completed by the trigger */
        if ((axis->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER && axis->value > 0.5f && !l_down) ||
            (axis->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER && axis->value > 0.5f && !r_down)) {
            if (pc_shortcut_held(0)) {
                (axis->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? l_down : r_down) = true;
                set_open(false, "the menu button");
                return input::EventResult::Consume;
            }
        }
        if (axis->axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            if (axis->value > 0.5f && !l_down) {
                category(-1);
            }
            l_down = axis->value > 0.5f;
        } else if (axis->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            if (axis->value > 0.5f && !r_down) {
                category(1);
            }
            r_down = axis->value > 0.5f;
        } else if (axis->axis == SDL_GAMEPAD_AXIS_LEFTX || axis->axis == SDL_GAMEPAD_AXIS_LEFTY) {
            /* the left stick moves like the D-pad: a push past halfway is a press, held it repeats */
            static float sx, sy;
            (axis->axis == SDL_GAMEPAD_AXIS_LEFTX ? sx : sy) = axis->value;
            const int dir = sy < -0.5f ? 1 : sy > 0.5f ? 2 : sx < -0.5f ? 3 : sx > 0.5f ? 4 : 0;
            hold_direction(dir, -1);
        }
        return input::EventResult::Consume;
    }
    if (!s_open) {
        return input::EventResult::Pass;
    }
    aurora::rmlui::process_input(event); /* the mouse (hover, clicks) and the rest */
    return input::EventResult::Consume;
}

bool captures(const input::InputSource&, void*) { return s_open; }

/* ---- Z + START (snatcherLeo, 2026-11-09, decided: "hold") -----------------------------------------------------------------
 * Runs inside PADRead (Aurora's filter hook, before the pad recorder's read hook), on the GameCube buttons the game is
 * about to be given, so it is the same on every controller. When Z goes down, Z is held back from the game for up to
 * kZHold reads. START in that time opens the overlay, and the game sees neither button until both are let go.
 * Otherwise Z reaches the game late (after kZHold reads, or as a one-read tap if it was let go sooner). START alone
 * always reaches the game. Why hold at all: the game's script checks count Z as "any button"
 * (scriptIsTrigerPush / scriptIsAnyInput) and the menus read it as MENU_KEY_Z, so a lone Z would move things on.
 * Off whenever a pad script drives the game (replays and automated runs): those feed what the game was given. */
/* *Updated 2026-11-12 (snatcherLeo's hand check; the decision log, the Controls rework):* the hold is now generalised to any
 * one- or two-button shortcut chosen in Controls (Z + L by default on a GameCube controller, snatcherLeo: "if it
 * automatically senses it, then "Z+L" will open the OrrePort Settings Menu by default"), and the same read filter
 * shapes Player 1's sticks the Dolphin way; both live in pc_pad_shape.cpp. */
std::atomic<bool> s_open_request{false};
std::atomic<bool> s_quit_request{false}; /* the Quit to hub shortcut was pressed (handled in the frame) */
void request_quit_to_hub() { s_quit_request = true; } /* the menu bar's Quit to hub: the shortcut's path */

void pad_filter(PADStatus* status) {
    static int enabled = -1;
    if (enabled < 0) {
        enabled = SDL_getenv("ORRE_PAD_SCRIPT") == nullptr && !pc_platform_automated_run() ? 1 : 0;
    }
    if (!enabled || s_open) {
        return;
    }
    const int done = pc_pad_filter_player1(status);
    if (done & PC_FILTER_OPEN_MENU) {
        s_open_request = true;
    }
    if (done & PC_FILTER_QUIT_TO_HUB) {
        s_quit_request = true;
    }
}

/* ---- the speed badge (snatcherLeo, 2026-11-16, mockup s7_badge: "I don't want the badge to linger on there for long at
 * all"): "▶▶ 3×", the speed shown, for about one second, then it fades out over 0.3 s, whenever fast-forward starts or
 * the speed changes to a faster one (pc_speed.c raises its number), in Hold and Press to switch alike. Top-left in a
 * window wider than the picture (in the black bar beside the 4:3 picture, 24 dp in), top centre when the window is
 * about 4:3 (clear of both sides' HP boxes in battle). Hidden with Speed badge = Hide, never in automated runs. Its
 * own small RmlUi document, so it shows without the menu's page (whose visibility rules belong to the menu). Look
 * only. UI id: speed_badge. All platforms (RmlUi). ----
 * ORRE_TEST_BADGE_HOLD=1 (test switch, look only): the badge stays at full strength instead of fading, so a picture
 * run can photograph it. */
Rml::ElementDocument* s_badge_doc = nullptr;
Uint64 s_badge_since = 0; /* when it appeared (SDL ticks); 0 not showing */
unsigned s_badge_seen = 0;
constexpr Uint64 kBadgeFull = 1000, kBadgeFade = 300;

void badge_init(Rml::Context* context) {
    /* sizes are set in pixels from the window's height when it shows (badge_frame), not in dp: dp follows Windows'
     * display scaling (2.25 on a 225% screen), which made the badge wider than the black bar of a 16:9 window; the
     * mockup's proportions (30 px tall in a 720-line window) keep it inside the bar at any scaling. The body covers the
     * window but takes no mouse (pointer-events: none), so the menu bar and pages under it still get theirs; it is
     * hidden whenever the badge isn't showing anyway */
    static const char* const kRml =
        "<rml><head><style>"
        "body { position: absolute; left: 0px; top: 0px; right: 0px; bottom: 0px; font-family: Inter; "
        "       pointer-events: none; }"
        "#speed_badge { display: block; position: absolute; border-width: 1px; border-color: #2C322F; "
        "       background-color: rgba(17, 20, 19, 199); color: #E9EDEA; font-weight: bold; white-space: nowrap; "
        "       pointer-events: none; }"
        "#speed_badge .ar { color: #E8955A; }"
        "</style></head><body><div id='speed_badge'></div></body></rml>";
    s_badge_doc = context->LoadDocumentFromMemory(kRml, "speed_badge");
    if (s_badge_doc != nullptr) s_badge_doc->Hide();
}

/* each game frame (pc_settings_overlay_frame) */
void badge_frame() {
    if (s_badge_doc == nullptr || pc_platform_automated_run()) return;
    unsigned percent = 0;
    const unsigned serial = pc_speed_badge_serial(&percent);
    const Uint64 now = SDL_GetTicks();
    Rml::Element* badge = s_badge_doc->GetElementById("speed_badge");
    if (badge == nullptr) return;
    if (serial != s_badge_seen) {
        s_badge_seen = serial;
        /* "3×", "1.5×": the speed as the rows write it */
        char text[32];
        if (percent % 100 == 0) SDL_snprintf(text, sizeof text, "%u×", percent / 100);
        else SDL_snprintf(text, sizeof text, "%u.%u×", percent / 100, percent % 100 / 10);
        /* the size: the mockup's, scaled by the window's height (k: pixels per mockup pixel of a 720-line window) */
        Rml::Context* context = aurora::rmlui::get_context();
        const Rml::Vector2i dims = context != nullptr ? context->GetDimensions() : Rml::Vector2i(0, 0);
        const float k = dims.y > 0 ? dims.y / 720.0f : 1.0f;
        auto px = [k](float v) { return std::to_string(v * k) + "px"; };
        badge->SetInnerRML(std::string("<span class='ar' style='font-size: ") + px(11) + "; letter-spacing: " + px(-2) +
                           "; margin-right: " + px(7) + ";'>▶▶</span>" + text);
        badge->SetProperty("height", px(30));
        badge->SetProperty("line-height", px(28));
        badge->SetProperty("padding-left", px(10));
        badge->SetProperty("padding-right", px(12));
        badge->SetProperty("border-radius", px(15));
        badge->SetProperty("font-size", px(13.5f));
        badge->SetProperty("letter-spacing", px(1));
        badge->RemoveProperty("opacity");
        /* where: the window's width against a 4:3 picture of its height; a bar beside the picture wide enough for the
         * badge (about 92 mockup px, plus 24 on its left) puts it top-left in the bar, 24 in from the corner; else top
         * centre (the mockup's 4:3 case: clear of both sides' HP boxes in battle) */
        const float bar = (dims.x - dims.y * 4.0f / 3.0f) / 2.0f;
        const bool in_bar = bar >= (92.0f + 24.0f) * k;
        if (in_bar) {
            badge->SetProperty("left", px(24));
            badge->SetProperty("top", px(24));
            badge->RemoveProperty("transform");
        } else {
            badge->SetProperty("left", std::to_string(dims.x / 2) + "px");
            badge->SetProperty("top", px(14));
            badge->SetProperty("transform", "translateX(-50%)");
        }
        s_badge_since = now == 0 ? 1 : now;
        s_badge_doc->Show();
        s_badge_doc->PullToFront();
        std::printf("[speed] badge shown: %s (%s; window %d x %d)\n", text, in_bar ? "top-left, in the bar" : "top centre",
                    dims.x, dims.y);
        return;
    }
    if (s_badge_since == 0) return;
    static int hold = -1;
    if (hold < 0) hold = SDL_getenv("ORRE_TEST_BADGE_HOLD") != nullptr ? 1 : 0;
    const Uint64 age = now - s_badge_since;
    if (hold) return;
    if (age >= kBadgeFull + kBadgeFade) {
        s_badge_since = 0;
        s_badge_doc->Hide();
    } else if (age > kBadgeFull) {
        badge->SetProperty("opacity", std::to_string(1.0f - (float)(age - kBadgeFull) / (float)kBadgeFade));
    }
}

/* *Added 2026-11-20 (step 3.7, the logs carry every setting; docs/port/tweaks-37-research.md section 8):* every Settings
 * row's value as the page shows it, one line per category in the page's order, in English whatever the player's
 * language (logs are read by us and testers' reports, so one language). Why from the row table itself: a row added later
 * is in the log with no extra work, so no Settings row can be missing from it. Printed once the rows have read their
 * values (the overlay's set-up, in the hub or at the game's first frame) and again by pc_settings_overlay_log_rows.
 * Platforms: all (plain text over the row table) */
std::string en_word(const std::string& s) {
    return s.rfind("settings.", 0) == 0 || s.rfind("common.", 0) == 0 ? std::string(pc_text_lang("en", s.c_str())) : s;
}

void log_rows(const char* when) {
    sync_live_values();
    for (int pos = 0; pos < TAB_COUNT; ++pos) {
        const int tab = kTabOrder[pos];
        std::string line;
        for (const RowDef& r : s_rows) {
            if (r.tab != tab || r.name == nullptr) continue;
            std::string v;
            if (r.kind == TOGGLE) {
                v = r.value ? "on" : "off";
            } else if ((r.kind == SELECT || r.kind == SLIDER) && r.value >= 0 && r.value < (int)r.options.size()) {
                v = en_word(r.options[r.value]);
            } else if (r.kind == SLIDER || r.kind == SELECT) {
                v = std::to_string(r.value);
            } else {
                continue; /* headings, actions, links and information rows hold no value */
            }
            if (r.locked) v += " (locked while this game runs)"; /* accurate mode's lock, or General's mode row */
            line += (line.empty() ? "" : "; ") + en_word(r.name) + " = " + v;
        }
        if (!line.empty()) std::printf("[settings] rows %s, %s: %s\n", when, en_word(kTabNames[tab]).c_str(), line.c_str());
    }
}

void init() {
    s_tried = true;
    if (!aurora::rmlui::is_initialized()) {
        std::printf("[settings_overlay] RmlUi is not initialized: no Settings window\n");
        return;
    }
    Rml::Context* context = aurora::rmlui::get_context();
    const bool font = pc_ui_load_fonts() != 0; /* shared with the hub (pc_hub.cpp), loaded once */
    s_doc = context != nullptr ? context->LoadDocument(ui_path("settings.rml")) : nullptr;
    if (s_doc == nullptr) {
        std::printf("[settings_overlay] could not load %s (font %s)\n", ui_path("settings.rml").c_str(),
                    font ? "ok" : "missing");
        return;
    }
    /* the menu's pictures (v2): the category icons and the PlayStation symbols (ui/icons/menu-*.svg, drawn white and
     * tinted by the style sheet), and the O for the panel's head (dark: the icon's O, light: the light-mode O). Drawn
     * once each by the hub's SVG reader (pc_svg.cpp) and kept. */
    aurora::rmlui::register_texture_provider("menuicon", [](std::string_view source) {
        /* name -> (width, (height, pixels)); the switch's pieces render 256 wide (2026-11-23: a 56 dp switch is 126 px at
         * 225%, and a 64 px picture stretched to it looked soft), every other icon 64 */
        static std::map<std::string, std::pair<int, std::pair<int, std::vector<unsigned char>>>> s_pictures;
        std::string name(source);
        const size_t slash = name.rfind('/');
        name = slash == std::string::npos ? name : name.substr(slash + 1);
        auto found = s_pictures.find(name);
        if (found == s_pictures.end()) {
            /* About's small icon buttons (2026-11-23, the same files Studio draws): folder, download, restart; the
             * switch's smooth pieces (tog-fill, tog-edge, tog-knob) */
            const bool tog = name.rfind("tog-", 0) == 0;
            const bool plain = tog || name == "folder" || name == "download" || name == "restart";
            const int width = tog ? 256 : 64;
            const std::string file = name == "o-dark"    ? "icons/orreport-icon-rust.svg"
                                     : name == "o-light" ? "icons/orreport-icon-rust-light-o.svg"
                                     : plain             ? "icons/" + name + ".svg"
                                                         : "icons/menu-" + name + ".svg";
            size_t len = 0;
            void* data = SDL_LoadFile(ui_path(file.c_str()).c_str(), &len);
            std::pair<int, std::vector<unsigned char>> pic{0, {}};
            if (data != nullptr) {
                const std::string svg(static_cast<const char*>(data), len);
                SDL_free(data);
                pc_svg_render(svg, width, pic.first, pic.second, {}); /* 64 px: sharp up to about 3x scaling */
            }
            found = s_pictures.emplace(name, std::make_pair(width, std::move(pic))).first;
        }
        const int w = found->second.first;
        const auto& [h, px] = found->second.second;
        if (px.empty() || h <= 0) {
            return std::optional<aurora::rmlui::RuntimeTexture>{};
        }
        return std::optional<aurora::rmlui::RuntimeTexture>{aurora::rmlui::RuntimeTexture{
            (uint32_t)w, (uint32_t)h, std::span<const std::byte>(reinterpret_cast<const std::byte*>(px.data()), px.size()),
            false, true}};
    });
    s_doc->AddEventListener(Rml::EventId::Click, &s_clicks);
    s_doc->AddEventListener(Rml::EventId::Mousedown, &s_drags);
    s_doc->AddEventListener(Rml::EventId::Mousemove, &s_drags);
    s_doc->AddEventListener(Rml::EventId::Mouseup, &s_drags);
    badge_init(context); /* the speed badge's own small page */
    s_defaults.clear();
    for (const RowDef& r : s_rows) {
        s_defaults.push_back(r.value); /* before the saved settings below replace them */
    }
    if (RowDef* bg = live_row(LIVE_MENUBG)) {
        bg->value = pc_settings_int("ui_menu_solid", 0) == 1 ? 1 : 0;
    }
    if (RowDef* size = live_row(LIVE_MENUSIZE)) {
        size->value = pc_settings_int("menu_full", 0) == 1 ? 1 : 0;
    }
    if (RowDef* hint = live_row(LIVE_MENUHINT)) {
        hint->value = pc_settings_int("ui_menu_hint", 1) != 0;
    }
    if (RowDef* dbg = live_row(LIVE_DEBUG)) {
        dbg->value = pc_settings_int("debug_tools", 0) != 0; /* off by default (pc_debug.c says why) */
    }
    if (RowDef* ask = live_row(LIVE_QUITASK)) {
        ask->value = pc_settings_int("quit_hub_ask", 1) != 0;
    }
    if (RowDef* bar = live_row(LIVE_MENUBAR)) {
        bar->value = pc_settings_int("menu_bar_always", 0) != 0;
    }
    if (RowDef* theme = live_row(LIVE_THEME)) {
        const int saved = pc_settings_int("ui_appearance", 0);
        theme->value = saved >= 0 && saved <= 2 ? saved : 0;
    }
    if (RowDef* lang = live_row(LIVE_LANG)) {
        /* the languages there are: one choice per ui/lang file, by the name it gives itself ("Deutsch") */
        lang->options = {"settings.language.auto"};
        s_lang_codes.clear();
        for (const auto& [code, name] : pc_text_languages()) {
            s_lang_codes.push_back(code);
            lang->options.push_back(name);
        }
        const std::string saved = pc_settings_string("language", "auto");
        lang->value = 0;
        for (size_t i = 0; i < s_lang_codes.size(); ++i) {
            if (s_lang_codes[i] == saved) lang->value = (int)i + 1;
        }
    }
    if (RowDef* pause = live_row(LIVE_PAUSE)) {
        pause->value = pc_settings_int("overlay_pause", 0) != 0;
    }
    tweak_values();
    if (RowDef* anim = live_row(LIVE_ANIM)) {
        anim->value = pc_settings_int("ui_animations", 1) != 0;
    }
    /* Graphics and Audio from settings.ini (the same keys and defaults pc_platform.c, pc_textures.cpp and pc_audio.c
     * read at start-up) */
    if (RowDef* r = live_row(LIVE_BACKEND)) {
        r->value = backend_value(pc_settings_string("gpu_backend", "auto"));
        if (s_backend_at_start < 0) {
            s_backend_at_start = r->value; /* the first read is the start's: only this menu changes the key */
        }
    }
    if (RowDef* r = live_row(LIVE_RES)) {
        const int scale = pc_settings_int("render_scale", 0);
        r->value = scale >= 0 && scale <= 8 ? scale : 0; /* 0 the window's size (the default), 1-8 times 640 x 480 */
    }
    if (RowDef* r = live_row(LIVE_VSYNC)) {
        r->value = pc_settings_int("vsync", 0) != 0;
    }
    if (RowDef* r = live_row(LIVE_FRAME_RATE)) { /* off, match (the default) or a cap's number (pc_smooth.h) */
        r->value = frame_rate_row(*r, pc_settings_string("frame_smoothing", PC_SMOOTH_SETTING_DEFAULT));
    }
    /* Game speed, Battles and Fast-forward: settings.ini holds each choice's code, the row its position (pc_speed.h:
     * 4× has the newest code but sits before 5×); an unknown code falls back to the default */
    if (RowDef* r = live_row(LIVE_SPEED_GAME)) {
        const int v = pc_speed_row_for_code(PC_SPEED_LIST_GAME, pc_settings_int("speed_game", 0));
        r->value = v >= 0 && v < (int)r->options.size() ? v : 0;
    }
    if (RowDef* r = live_row(LIVE_SPEED_BATTLE)) {
        const int v = pc_speed_row_for_code(PC_SPEED_LIST_BATTLE, pc_settings_int("speed_battle", 0));
        r->value = v >= 0 && v < (int)r->options.size() ? v : 0;
    }
    if (RowDef* r = live_row(LIVE_SPEED_SOUND)) {
        r->value = pc_settings_int("speed_sound", 0) != 0; /* 0 Normal tempo (the default), 1 Faster too */
    }
    if (RowDef* r = live_row(LIVE_FF_SPEED)) { /* 2×, 3× (the default), 4×, 5× (pc_speed.h) */
        const int v = pc_speed_row_for_code(PC_SPEED_LIST_FF, pc_settings_int("speed_ff", PC_SPEED_FF_DEFAULT));
        r->value = v >= 0 && v < (int)r->options.size() ? v : r->value;
    }
    if (RowDef* r = live_row(LIVE_FF_MODE)) {
        r->value = pc_settings_int("speed_ff_mode", 0) == 1 ? 1 : 0; /* 0 Hold (the default), 1 Press to switch */
    }
    if (RowDef* r = live_row(LIVE_FF_BADGE)) {
        r->value = pc_settings_int("speed_badge", 1) != 0 ? 0 : 1; /* the row: 0 Show (the default), 1 Hide */
    }
    if (RowDef* r = live_row(LIVE_MSAA)) {
        r->value = pc_settings_int("msaa", 1) == 4 ? 1 : 0;
    }
    if (RowDef* r = live_row(LIVE_ANISO)) {
        const int a = pc_settings_int("anisotropy", 1); /* None by default (pc_platform.c aniso_setting) */
        r->value = a == 1 ? 0 : a == 2 ? 1 : a == 4 ? 2 : a == 8 ? 3 : 4;
    }
    for (auto [live, key] : {std::pair{LIVE_VOL_MASTER, "volume_master"}, std::pair{LIVE_VOL_MUSIC, "volume_music"},
                             std::pair{LIVE_VOL_EFFECTS, "volume_effects"}}) {
        if (RowDef* r = live_row(live)) {
            const int v = pc_settings_int(key, 100);
            r->value = (v < 0 ? 0 : v > 100 ? 100 : v) / 10;
        }
    }
    if (RowDef* game_icon = live_row(LIVE_GAMEICON)) {
        game_icon->value = pc_settings_int("ui_window_icon", 0) == 1 ? 1 : 0;
        pc_window_icon_game(game_icon->value);
    }
    apply_title_bar();
    /* the game's window title is set at the game's first frame (pc_settings_overlay_frame), not here: the panel can
     * now start in the hub (its gear), where the window stays "OrrePort" */
    s_doc->Hide();
    PADSetFilterHook(pad_filter); /* the menu shortcut's hold-back and Player 1's stick shaping */
    s_layer = input::register_layer({
        .label = "OrrePort Settings (RmlUi trial)",
        .priority = input::kRmlUiLayerPriority,
        .onEvent = on_event,
        .capturesSource = captures,
    });
    std::printf("[settings_overlay] ready: %s (keyboard), %s (controller) or F2 opens Settings (tabs: %s; font %s)\n",
                pc_shortcut_name(1), pc_shortcut_name(0), kTabs, font ? "ok" : "missing");
    log_rows("at set-up"); /* every row's value, now that the rows have read them (step 3.7, the settings snapshot) */
    /* ORRE_SETTINGS_OPEN_AT=<game frame>: open it by itself at that frame (a test hook for screenshots; the
     * automated runs cannot press a key) */
}
} // namespace

/* pc_game_main.c, right after the window is created: the icon, the title bar's colours and the title from the saved
 * settings at once, so the first moments don't show Windows' default icon and the build-stamp title (snatcherLeo,
 * 2026-11-09: "the top icon briefly is the older icon ... just briefly before changing"); the overlay's own start at
 * the first frame sets them again from the same settings */
extern "C" void pc_window_style_early(void) {
    pc_window_icon_set(kIconColour);
    /* the chosen window icon; a game's icon needs its ROM, which may not be open yet: then the logo until the hub
     * starts (pc_hub.cpp asks again) */
    pc_window_icon_game(pc_settings_int("ui_window_icon", 0) == 1 ? 1 : 0);
    const int theme = pc_settings_int("ui_appearance", 0);
    pc_window_title_bar_style(theme == 2 || (theme != 1 && SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT) ? 1 : 0);
    /* "OrrePort" while the hub is up, the game's title once it starts; an automated run gets "OrrePort AUTOMATED TEST
     * RUN ..." instead (pc_platform_set_title replaces it) */
    pc_platform_set_title(SDL_getenv("ORRE_SKIP_HUB") != nullptr ? window_title() : kHubTitle);
    /* invisible until the hub has drawn itself (pc_window_icon.cpp); automated runs keep their own window handling */
    if (!pc_platform_automated_run() && SDL_getenv("ORRE_SKIP_HUB") == nullptr) {
        pc_window_cloak(1);
    }
}

/* Keeps the hub (pc_hub.cpp) in the link: pc_game_main.c reaches pc_hub_run only through a weak reference, and a
 * weak reference alone never pulls an object out of a static library (the first D1 build ran without the hub). This
 * object is always linked (pc_vi.c calls it), so this one kept pointer brings the hub along with it. */
extern "C" int pc_hub_run(void);
[[gnu::used]] static int (*const s_keep_hub)(void) = pc_hub_run;

/* pc_vi.c, after each finished frame: hold the game there while this returns 1 (hold_for_overlay) */
extern "C" int pc_settings_overlay_wants_pause(void) {
    const RowDef* pause = live_row(LIVE_PAUSE);
    const bool pausing = (pause != nullptr && pause->value != 0) || s_report; /* a report always pauses */
    return s_open && pausing && !s_test_no_pause ? 1 : 0;
}

/* the menu bar's Help > Report a problem… (pc_menu_bar.cpp): opens the overlay on the report panel */
extern "C" void pc_settings_overlay_open_report(void) {
    if (s_doc == nullptr) {
        return;
    }
    const bool alone = !s_open;
    if (!s_open) {
        set_open(true, "Help > Report a problem");
    }
    open_report(alone);
}

/* pc_platform.c, asked to go into or out of fullscreen: the dip to black, with the switch made once it is black
 * (cover_frame); 0 when the overlay can't (not started yet, or a dip already running): the switch is made at once */
extern "C" int pc_settings_overlay_cover_switch(void) {
    if (s_doc == nullptr || !s_tried) {
        return 0;
    }
    if (s_cover_since != 0) {
        return 1; /* a second press during the dip: the one switch already coming */
    }
    s_cover_since = SDL_GetTicks();
    s_cover_switched = 0;
    if (Rml::Element* body = s_doc->GetElementById("body")) {
        body->SetClass("covering", true);
        body->SetClass("coveronly", !s_open); /* the menu closed: the cover alone */
    }
    if (Rml::Element* cover = s_doc->GetElementById("cover")) cover->SetProperty("opacity", "0");
    if (!s_open) s_doc->Show();
    s_doc->PullToFront(); /* over the hub too */
    return 1;
}

namespace {
void frame_common();
}

/* the hub's gear (pc_hub.cpp): the same Settings panel, opened over the hub (snatcherLeo, 2026-11-11, in-window) */
extern "C" void pc_settings_overlay_open_in_hub(void) {
    if (!s_tried) {
        init();
    }
    if (s_doc == nullptr || s_open) {
        /* why the gear did nothing, in the log (2026-11-13: with no disc yet it didn't open, with nothing said) */
        std::printf("[settings_overlay] not opened from the hub: %s\n", s_doc == nullptr ? "the page didn't load" : "already open");
        return;
    }
    s_in_hub = true;
    set_open(true, "the hub's gear");
    test_focus();
    /* ORRE_SETTINGS_TEST_GOBACK=1 (a test switch, never a setting; 2026-11-23, for pictures of Go back's pop-up): opens
     * it at once when there is a version to go back to (with ORRE_SETTINGS_TAB=6 the page under it is About) */
    if (SDL_getenv("ORRE_SETTINGS_TEST_GOBACK") != nullptr && about_status().previous[0] != '\0') {
        s_gb = true;
        s_gbFocus = 0;
        refresh();
    }
}

/* back from the hub's disc art page (its ‹ Settings arrow, the gear, B or Esc; snatcherLeo, 2026-11-13: "there should be a way
 * to get back to the settings menu"): Settings opens on Appearance with its "Change disc art" row selected, the page
 * the disc art page is reached from */
extern "C" void pc_settings_overlay_open_in_hub_disc_art(void) {
    if (!s_tried) {
        init();
    }
    if (s_doc == nullptr || s_open) {
        return;
    }
    s_in_hub = true;
    set_open(true, "back from disc art");
    s_tab = 1; /* Appearance (kTabNames) */
    s_cats = false;
    const std::vector<int> rows = tab_rows();
    for (int k = 0; k < (int)rows.size(); ++k) {
        if (s_rows[rows[k]].live == LIVE_DISCLABEL) s_focus = k;
    }
    refresh();
}

/* a test hook (pc_hub.cpp ORRE_HUB_TEST_SAVES=settings_whatoff; 2.12): General › "What it turns off" opened, for a picture */
extern "C" void pc_settings_overlay_test_whatoff(void) {
    if (!s_open) return;
    s_tab = 0;
    s_cats = false;
    s_what = true;
    s_what_adds = false;
    refresh();
}

/* the same for About › "What OrrePort adds" (ORRE_HUB_TEST_SAVES=settings_whatadds; 5.13), for a picture */
extern "C" void pc_settings_overlay_test_whatadds(void) {
    if (!s_open) return;
    s_tab = 6;
    s_cats = false;
    s_what = true;
    s_what_adds = true;
    refresh();
}

/* 1 when OrrePort's screens should be dark now: Appearance > Theme as saved (0 follow Windows, 1 light, 2 dark),
 * Windows' own theme for "Follow Windows". The hub follows it every frame (snatcherLeo, 2026-11-11: light mode changes the
 * whole hub, not only the title bar); Studio will read the same setting. */
extern "C" int pc_ui_theme_is_dark(void) {
    const int t = pc_settings_int("ui_appearance", 0);
    return t == 2 || (t != 1 && SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT) ? 1 : 0;
}

/* 1 when Appearance > Animations is on (the default): the hub's disc sways and spins on Play */
extern "C" int pc_ui_animations_on(void) { return pc_settings_int("ui_animations", 1) != 0 ? 1 : 0; }

/* 1 while the panel is open (the hub passes its input to it then) */
extern "C" int pc_settings_overlay_is_open(void) { return s_open ? 1 : 0; }

/* every Settings row's value in the log (step 3.7, the settings snapshot; log_rows above): only once the rows have read
 * their values (before the overlay's set-up they hold the table's defaults, which the log must not pass off as the
 * player's). All platforms */
extern "C" void pc_settings_overlay_log_rows(const char* when) {
    if (s_doc == nullptr) return;
    mode_values();
    log_rows(when);
}

/* the hub's frame (pc_hub.cpp), before it draws: the panel's per-frame work while it is open over the hub */
extern "C" void pc_settings_overlay_hub_frame(void) {
    if (s_tried) {
        frame_common();
    }
}

/* pc_vi.c, once per finished frame, before aurora_end_frame (where RmlUi draws) */
extern "C" void pc_settings_overlay_frame(void) {
    if (!s_tried) {
        init();
    }
    if (!s_game_running) { /* the game's first frame: from here on the menu is the game's (s_in_hub) */
        s_game_running = true;
        if (!s_open) s_in_hub = false;
    }
    static bool game_title = false;
    if (!game_title) { /* the game has started: its title (a test window keeps its "AUTOMATED TEST RUN" title) */
        game_title = true;
        if (s_doc != nullptr && !pc_platform_automated_run()) {
            pc_platform_set_title(window_title());
            /* the hint at game start (v2, mockup 10): how to open the menu, for 4 seconds (the last half fading), each
             * time the game starts, unless General > Menu hint at start is off; for the controller connected, else
             * the keyboard. *Updated 2026-11-12 (snatcherLeo):* at every start with a switch to turn it off, rather than once
             * ever, and shorter (was 7 s) */
            const RowDef* hint = live_row(LIVE_MENUHINT);
            if (hint != nullptr && hint->value != 0 && !s_open) {
                int count = 0;
                if (SDL_JoystickID* pads = SDL_GetGamepads(&count)) {
                    s_ctrl = count > 0 ? ctrl_of_gamepad(pads[0]) : CTRL_KEYBOARD;
                    SDL_free(pads);
                }
                s_toast_msg.clear(); /* the menu hint's own text */
                s_toast_until = SDL_GetTicks() + 4000;
                refresh();       /* its text and look first, */
                fit_to_window(); /* then its size, so the first frame shows it as it stays */
                s_doc->Show();
                std::printf("[settings_overlay] the menu hint shown (General > Menu hint at start)\n");
            }
        }
    }
    static long open_at = -2;
    static long frames = 0;
    if (open_at == -2) {
        const char* v = SDL_getenv("ORRE_SETTINGS_OPEN_AT");
        open_at = v != nullptr ? SDL_strtol(v, nullptr, 10) : -1;
    }
    if (open_at >= 0 && ++frames == open_at) {
        const char* tp = SDL_getenv("ORRE_SETTINGS_TEST_PAUSE");
        s_test_pause_left = tp != nullptr ? SDL_strtol(tp, nullptr, 10) : -1;
        s_test_no_pause = s_test_pause_left < 0;
        set_open(true, "ORRE_SETTINGS_OPEN_AT");
        /* ORRE_SETTINGS_TAB=<0-5> (2026-11-14, for pictures of a category's page, the Tweaks list first): open on
         * that category's page, as choosing it on the first screen does (0 General ... 5 Enhancements). The numbers are
         * the categories' ids, not their places: unchanged by the 2026-11-16 reorder (Enhancements is still 5, shown
         * fourth; kTabOrder) */
        if (const char* tab = SDL_getenv("ORRE_SETTINGS_TAB")) {
            const long t = SDL_strtol(tab, nullptr, 10);
            if (t >= 0 && t < TAB_COUNT) {
                s_tab = (int)t;
                s_level = 1;
                s_focus = 0;
                refresh();
                test_focus();
            }
        }
        /* ORRE_SETTINGS_TEST_CTRL=keyboard|xbox|playstation|gamecube (a test switch, look only; 2026-11-16, for pictures
         * of the Fast-forward link per device): the controller in hand as if that device had been used last (the
         * buttons' names follow it too, pc_controls_ui.cpp). ORRE_SETTINGS_TEST_FF_LINK=1: then choose the link, as A
         * on it does (Controls opens on its Fast-forward row). All platforms */
        if (const char* c = SDL_getenv("ORRE_SETTINGS_TEST_CTRL")) {
            s_ctrl = SDL_strcmp(c, "xbox") == 0          ? CTRL_XBOX
                     : SDL_strcmp(c, "playstation") == 0 ? CTRL_PLAYSTATION
                     : SDL_strcmp(c, "gamecube") == 0    ? CTRL_GAMECUBE
                                                         : CTRL_KEYBOARD;
            refresh();
        }
        if (SDL_getenv("ORRE_SETTINGS_TEST_FF_LINK") != nullptr) {
            pc_controls_ui_open_ff(s_ctrl == CTRL_KEYBOARD ? 1 : 0);
        }
        /* ORRE_SETTINGS_TEST_PICK=<choice> (a test switch, never a setting; 2026-11-18, for the Frame rate row's check:
         * set, quit, start again): on the row ORRE_SETTINGS_FOCUS put the controller on, pick that choice (0 the
         * first), as A, Down and A do: the same save and apply as a player's pick. ORRE_SETTINGS_TEST_PICK=open only
         * opens the row's drop-down (for a picture of its list). All platforms */
        if (const char* pick = SDL_getenv("ORRE_SETTINGS_TEST_PICK")) {
            const std::vector<Item> list = items();
            const int v = (int)SDL_strtol(pick, nullptr, 10);
            if (s_focus >= 0 && s_focus < (int)list.size() && list[s_focus].kind == IT_ROW &&
                SDL_strcmp(pick, "open") == 0) {
                const RowDef& r = s_rows[list[s_focus].index];
                if (r.kind == SELECT && !r.locked) {
                    s_drop = r.value;
                    refresh();
                }
            } else if (s_focus >= 0 && s_focus < (int)list.size() && list[s_focus].kind == IT_ROW) {
                RowDef& r = s_rows[list[s_focus].index];
                std::printf("[settings_overlay] ORRE_SETTINGS_TEST_PICK=%d on %s%s\n", v, r.name,
                            r.kind != SELECT ? " (not a choice row: nothing done)" : r.locked ? " (locked)" : "");
                if (r.kind == SELECT && !r.locked && v >= 0 && v < (int)r.options.size() && v != r.value) {
                    r.value = v;
                    apply_live(r);
                }
            }
        }
        if (SDL_getenv("ORRE_SETTINGS_TEST_REPORT") != nullptr) { /* a test of Report a problem: open and save it */
            open_report(false);
            if (auto* control = dynamic_cast<Rml::ElementFormControl*>(s_doc->GetElementById("note"))) {
                control->SetValue("Automated test of Report a problem (ORRE_SETTINGS_TEST_REPORT).");
            }
            update_note_placeholder();
            s_report_save = SDL_strcmp(SDL_getenv("ORRE_SETTINGS_TEST_REPORT"), "show") == 0 ? 0 : 1;
        }
    }
    if (s_open && s_test_pause_left > 0 && --s_test_pause_left == 0) {
        set_open(false, "ORRE_SETTINGS_TEST_PAUSE ran out");
    }
    /* ORRE_SETTINGS_TEST_QUIT=<frames> (a test switch, never a setting; 2026-11-15): Quit to hub by itself after
     * that many game frames, to check the hand-over to the new OrrePort's hub without a person (its log says how long
     * the closing screen stayed) */
    static long quit_at = -2;
    if (quit_at == -2) {
        const char* v = SDL_getenv("ORRE_SETTINGS_TEST_QUIT");
        quit_at = v != nullptr ? SDL_strtol(v, nullptr, 10) : -1;
    }
    if (quit_at > 0 && --quit_at == 0) {
        quit_to_hub();
    }
    if (s_open_request.exchange(false)) {
        set_open(true, "the menu shortcut"); /* asked for by the read filter during the game's pad read */
    }
    badge_frame(); /* the speed badge (fast-forward, a faster speed) */
    if (s_quit_request.exchange(false)) { /* the Quit to hub shortcut: its question in the side panel, or straight out */
        if (pc_settings_int("quit_hub_ask", 1) != 0 || pc_card_used_within(2500)) {
            if (!s_open) set_open(true, "the Quit to hub shortcut");
            s_full = false; /* the question lives on the panel's first screen */
            s_level = 0;
            s_quit_ask = true;
            s_quit_noask = false;
            const std::vector<Item> list = items();
            for (int k = 0; k < (int)list.size(); ++k) {
                if (list[k].kind == IT_REPORT) s_focus = k; /* on Cancel */
            }
            if (pc_card_used_within(2500)) {
                s_note = s_note_err = tx("settings.note.saving"); /* red, as a refusal (snatcherLeo, 2026-11-16) */
                s_quit_ask = false;
            }
            refresh();
        } else {
            quit_to_hub();
        }
    }
    frame_common();
}

namespace {
/* the per-frame work shared by the game (pc_settings_overlay_frame) and the hub (pc_settings_overlay_hub_frame) */
/* a fullscreen switch's dip to black: to black in 0.08 s, the switch (pc_platform.c) once black, black held 0.25 s while
 * the window settles at its new size, then a 0.15 s fade back */
extern "C" void pc_platform_toggle_fullscreen_now(void);
void cover_frame() {
    if (s_cover_since == 0 || s_doc == nullptr) return;
    const Uint64 now = SDL_GetTicks();
    Rml::Element* cover = s_doc->GetElementById("cover");
    if (s_cover_switched == 0) {
        const Uint64 t = now - s_cover_since;
        /* quicker since 2026-11-13 (snatcherLeo compared Dolphin, whose switch from a normal window is instant: "smooth and
         * fast"): 50 ms down, the switch at 60 ms, 160 ms black, 100 ms up, about a third of a second in all (it was
         * 80 / 100 / 250 / 150). Still long enough to hide Windows' leftover frames from the old size, which lasted 3-4
         * frames (about 70 ms) in the 30 fps recordings; Dolphin shows its own (the taskbar over its fullscreen) */
        if (cover != nullptr) cover->SetProperty("opacity", std::to_string(SDL_min(1.0, (double)t / 50.0)));
        if (t >= 60) { /* black for a few frames already: the switch */
            pc_platform_toggle_fullscreen_now();
            s_cover_switched = SDL_GetTicks();
        }
        return;
    }
    const Uint64 t = now - s_cover_switched;
    if (t >= 260) {
        s_cover_since = 0;
        if (Rml::Element* body = s_doc->GetElementById("body")) {
            body->SetClass("covering", false);
            body->SetClass("coveronly", false);
        }
        if (!s_open && !s_bar && s_toast_until == 0 && s_quit_fade_since == 0) s_doc->Hide();
        return;
    }
    if (cover != nullptr) cover->SetProperty("opacity", std::to_string(t < 160 ? 1.0 : 1.0 - (double)(t - 160) / 100.0));
}

void frame_common() {
    quit_fade_frame(); /* Quit to hub's fade, then the restart */
    cover_frame();     /* a fullscreen switch's dip to black */
    /* Graphics › Frame rate names the rate of the monitor the window is on (snatcherLeo, 2026-11-16: "updated when the window
     * moves to another monitor"): while Settings shows Graphics, a move to another monitor, or the monitor's rate
     * changed in Windows (read once a second), redraws the page. Asking SDL which monitor the window is on is cheap
     * (Windows: MonitorFromWindow); the rates themselves are cached (display_rates) */
    if (s_open && s_tab == 2 && s_doc != nullptr) {
        static SDL_DisplayID shown_id = 0;
        static int shown_now = -1, shown_best = -1;
        const DisplayRates d = display_rates();
        if (s_rates_display != shown_id || d.now != shown_now || d.best != shown_best) {
            shown_id = s_rates_display;
            shown_now = d.now;
            shown_best = d.best;
            refresh();
        }
    }
    /* Always show the menu bar: in game, whenever the menu is closed (not in the hub, not while quitting) */
    if (!s_bar && !s_open && !s_in_hub && s_doc != nullptr && s_quit_fade_since == 0 && !pc_platform_automated_run() &&
        pc_settings_int("menu_bar_always", 0) != 0) {
        bar_show(true);
    }
    if (s_bar && s_in_hub) bar_show(false); /* the hub has its own top */
    /* Always show the menu bar gives the bar its own strip: the game picture is fitted below it, so nothing of the game
     * is covered (snatcherLeo, 2026-11-13, mockup 49, "A + B": "it cannot be running over the game"); off, the bar only
     * appears over the top while the mouse is there and the picture never moves (B). The strip is the bar's 40 dp and
     * its 1 dp line, in pixels at the menu's scale (Aurora's aurora_set_present_inset_top moves only the game picture;
     * this overlay still has the whole window). Not in the hub, and never in automated runs (their pictures stay
     * exact). Platform-neutral. */
    {
        int inset = 0;
        if (!s_in_hub && s_doc != nullptr && !pc_platform_automated_run() &&
            pc_settings_int("menu_bar_always", 0) != 0) {
            const Rml::Context* context = aurora::rmlui::get_context();
            const float ratio = context != nullptr ? context->GetDensityIndependentPixelRatio() : 1.0f;
            inset = (int)SDL_lroundf(41.0f * (ratio > 0 ? ratio : 1.0f));
        }
        static int s_inset_shown = -1;
        if (inset != s_inset_shown) {
            s_inset_shown = inset;
            aurora_set_present_inset_top((uint32_t)inset);
            std::printf("[settings_overlay] game picture %s\n", inset > 0 ? "below the menu bar's strip" : "uses the whole window");
        }
    }
    pc_controls_ui_frame(); /* the controller setup screen, when open: the press it waits for, its live view */
    {   /* the GameCube adapter row: look for the adapter every two seconds while Settings is open (plugging it in shows
         * the row), and pick up the setup program's result (pc_adapter_setup.c) */
        static Uint64 s_adapter_looked = 0;
        const Uint64 now = SDL_GetTicks();
        if (s_open && !s_adapter_running && now - s_adapter_looked > 2000) {
            s_adapter_looked = now;
            const int before = pc_adapter_needs_setup();
            pc_adapter_refresh();
            if (before != pc_adapter_needs_setup() && s_doc != nullptr) refresh();
        }
        if (!s_open) s_adapter_shown = false;
        /* the claim below keeps watching for half a minute, even if Settings is closed (a controller can show up a
         * moment after Windows swaps the driver) */
        static Uint64 s_claim_until = 0;
        if (s_adapter_claim && s_claim_until == 0) s_claim_until = now + 30000;
        if (s_adapter_claim && now > s_claim_until) {
            s_adapter_claim = false;
            s_claim_until = 0;
        }
        int code = 0;
        /* The busy screen's progress bar (snatcherLeo, 2026-11-13: "have a progress bar too, as it's installing the driver").
         * The setup program writes its steps (find 5 %, prepare 15 %, install 40 %, done 100 %); libwdi reports nothing
         * finer, so within a step the bar creeps on towards the next one, slowing as it goes, and never passes it:
         * preparing took a few seconds and Windows' install about 40 s in snatcherLeo's test (adapter setup.log). */
        static std::string s_step;
        static Uint64 s_step_at = 0;
        static float s_shown_pct = 0.0f;
        if (s_adapter_running && s_doc != nullptr) {
            int pct = 0;
            char step[32] = "start";
            if (!pc_adapter_setup_progress(&pct, step, sizeof(step))) {
                pct = 0;
                SDL_snprintf(step, sizeof(step), "%s", s_step.empty() ? "start" : s_step.c_str());
            }
            if (s_step != step) {
                s_step = step;
                s_step_at = now;
            }
            const float secs = (float)(now - s_step_at) / 1000.0f;
            float target = (float)pct;
            if (s_step == "prepare") target = 15.0f + 23.0f * (1.0f - SDL_expf(-secs / 4.0f));        /* -> 38 */
            else if (s_step == "install") target = 40.0f + 55.0f * (1.0f - SDL_expf(-secs / 18.0f)); /* -> 95 */
            if (target > s_shown_pct) s_shown_pct = target;
            if (Rml::Element* bar = s_doc->GetElementById("busybar")) {
                bar->SetProperty("width", std::to_string((int)s_shown_pct) + "%");
            }
            if (Rml::Element* line = s_doc->GetElementById("busystep")) {
                const char* key = s_step == "install" ? "settings.adapter.step.install"
                                  : s_step == "prepare" ? "settings.adapter.step.prepare"
                                  : s_step == "done"    ? "settings.adapter.step.done"
                                                        : "settings.adapter.step.find";
                line->SetInnerRML(rml_escape(tx(key, {{"percent", std::to_string((int)s_shown_pct)}})));
            }
        } else {
            s_step.clear();
            s_shown_pct = 0.0f;
        }
        if (s_adapter_running && pc_adapter_setup_poll(&code) == 0) {
            s_adapter_running = false;
            if (s_doc != nullptr) {
                if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("busy", false);
            }
            s_note_err.clear();
            if (code == 0) {
                s_note = tx("settings.note.adapter_done_plug");
                s_adapter_claim = true; /* its controller becomes Player 1 as soon as it shows up (below) */
            } else if (code == 2) {
                s_note = tx("settings.note.adapter_none");
            } else if (code == 6) {
                s_note = tx("settings.note.adapter_declined");
            } else {
                s_note = s_note_err = tx("settings.note.adapter_failed");
            }
            s_note_big = s_note;
            if (s_doc != nullptr) refresh();
        }
        /* After the setup, the adapter's controller becomes Player 1 with its GameCube layout, without a trip to Set
         * up controller (snatcherLeo, 2026-11-13: "automatically change the default controller to gamecube after doing this
         * process"; in his test it worked only after opening Set up controller). Watched while Settings stays open. */
        if (s_adapter_claim && pc_controls_claim_gamecube_pad() != 0) {
            s_adapter_claim = false;
            s_claim_until = 0;
            s_note = s_note_big = tx("settings.note.adapter_done");
            s_note_err.clear();
            if (s_doc != nullptr) refresh();
        }
    }
    if (s_doc != nullptr) { /* one cursor while a slider is dragged (settings.rcss body.dragging) */
        if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("dragging", s_drag >= 0);
    }
    {   /* what a Cheats › Put in the PC choice did, once the game thread has done it (pc_cheats.c) */
        char line[512];
        if (pc_cheats_take_notice(line, sizeof(line)) && s_doc != nullptr && !s_open) {
            show_notice("<b>" + rml_escape(line) + "</b>"); /* only a failure ("the PC's Item Storage is full") */
        }
        if (pc_cheat_put_take_changed() && s_doc != nullptr) refresh(); /* the list's new check mark */
    }
    if (s_toast_until != 0 && s_doc != nullptr) { /* the hint at start: fades over its last half second, then goes */
        const Uint64 now = SDL_GetTicks();
        Rml::Element* toast = s_doc->GetElementById("toast");
        if (now >= s_toast_until) {
            s_toast_until = 0;
            s_toast_msg.clear();
            if (!s_open && !s_bar) { /* the menu bar, when shown (always, or the mouse at the top), keeps the page up */
                s_doc->Hide();
            }
            if (toast != nullptr) {
                toast->RemoveProperty("opacity");
            }
            refresh();
        } else if (toast != nullptr && s_toast_until - now < 500) {
            toast->SetProperty("opacity", std::to_string((float)(s_toast_until - now) / 500.0f));
        }
    }
    if (s_open && !s_report && s_hold_dir != 0 && SDL_GetTicks() >= s_hold_next) {
        s_hold_next = SDL_GetTicks() + 75; /* a held direction repeats */
        step_direction(s_hold_dir);
    }
    static unsigned theme_check = 0;
    if (s_doc != nullptr && ++theme_check % 120 == 0) {
        apply_title_bar(); /* "Follow Windows": the title bar follows a change made while the menu is closed */
    }
    /* About shows the update's progress as it goes (a download started here, in the hub's panel or in Studio): looked at
     * twice a second while About is the open tab, and redrawn only when something changed. *Fixed 2026-11-23 (snatcherLeo: "when
     * I hover my mouse on any of those options, the hover region flickers/dims fast"):* it redrew every time, and each
     * redraw rebuilt the rows, dropping the hover highlight */
    if (s_doc != nullptr && s_open && s_tab == 6 && !s_report && theme_check % 30 == 0 && s_drop < 0) {
        static PcUpdateStatus shown{};
        const PcUpdateStatus st = about_status();
        if (st.state != shown.state || st.done != shown.done || st.other_running != shown.other_running ||
            std::strcmp(st.latest, shown.latest) != 0 || std::strcmp(st.previous, shown.previous) != 0 ||
            std::strcmp(st.error_key, shown.error_key) != 0) {
            shown = st;
            refresh();
        }
    }
    /* Go back's pop-up follows Studio opening or closing (its Close Studio turns into Go back), twice a second while it
     * is open, whatever page is under it */
    if (s_doc != nullptr && s_open && theme_check % 30 == 0) {
        static int studio_shown = -1;
        const int studio = s_gb && s_in_hub ? (about_studio_open() ? 1 : 0) : -1;
        if (studio != studio_shown) {
            studio_shown = studio;
            if (studio >= 0) refresh();
        }
    }
    if (s_report && s_focus_note > 0 && --s_focus_note == 0) {
        if (Rml::Element* note = s_doc->GetElementById("note")) {
            const bool focused = note->Focus(); /* starts the keyboard's text input too (Aurora's ActivateKeyboard) */
            std::printf("[settings_overlay] report text box focused: %s\n", focused ? "yes" : "no");
        }
    }
    if (s_report && s_report_save > 0) {
        /* Save report: wait for the picture taken as the panel opened (a few frames), at most ~1.5 s */
        if (!pc_frame_dump_screenshot_busy() || ++s_report_save > 90) {
            save_report_now();
        }
    }
    /* over the hub, the hub's own scale stays (pc_hub.cpp fit()); the hint at start is scaled like the panel */
    if ((s_open || s_toast_until != 0 || s_bar) && s_doc != nullptr && !s_in_hub && !pc_controls_ui_is_open()) {
        fit_to_window(); /* follows a window that is resized while the panel or the hint shows */
    }
    if (s_open) {
        /* follow Windows' light / dark while open: rebuild only when it changes (a rebuild every frame would replace
         * the rows under the mouse and lose hover and clicks) */
        static int last_system = -1;
        const int system = SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT ? 1 : 0;
        const RowDef* theme = live_row(LIVE_THEME);
        if (theme != nullptr && theme->value == 0 && system != last_system && last_system != -1) {
            refresh();
        }
        last_system = system;
    }
}
} // namespace
