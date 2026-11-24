// app.hpp: the Win32 shell's shared declarations (OrrePort Studio, Windows only).
// Why one shell over the core: the plan's architecture ("a thin shell per system"): this shell only draws the windows,
// lists and pages and passes what the user does to the core (orre_core.h); it holds no editor logic. Linux gets a Qt
// shell and macOS an AppKit shell later, each replacing this folder, never the core.
#pragma once

#include <windows.h>
#include <d2d1_3.h>
#include <dwrite_3.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "orre_core.h"

namespace studio {

// ---- lang.cpp ----
std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& w);
void lang_load(const std::wstring& dir, const std::string& code);
// OrrePort's own lines that both programs share word for word (the disc messages): lang.cpp says which and why
void lang_load_shared(const std::wstring& orreport_lang_dir, const std::string& code);
std::wstring tx(const char* key, std::initializer_list<std::pair<const char*, std::wstring>> args = {});
bool has_text(const char* key);  // (a key with a text in the chosen language or English)

// ---- log.cpp: logs and the memory watch ----
// snatcherLeo's philosophy, section 8 "Watching the program" (2026-11-18): plain-text logs on the PC only, no personal data,
// rotated, detailed levels off by default; a built-in watch of Studio's own memory and disk reads / writes against
// budgets, anything out of place logged and shown in a small developer view (Ctrl+Shift+D).
enum class Level { Error, Warn, Info, Detail };
void log_open(const std::wstring& folder, bool detail);
void logf(Level lv, const char* area, const char* fmt, ...);
struct Usage {
    double working_mb = 0, private_mb = 0, peak_mb = 0;
    double read_mb = 0, written_mb = 0;
};
Usage usage_now();
// checks memory and disk use against the budgets; logs a warning when one is passed. Called after each piece of work
// (a load, a selection) and on a slow timer while the window is in front, never in a loop.
void watch_check(const char* after_what);
std::vector<std::wstring> watch_lines();  // for the developer view
// The performance log (part 4 of the loggers, snatcherLeo 2026-11-22; the project's format, pc_health.c): one row a second into
// studio-<stamp>_perf.csv beside the log. perf_tick is called by a 1 s timer with where Studio is ("moves", "home"...);
// it reads the 3D view's frame times (viewer_frame_stats), memory, CPU and graphics memory, writes the row, and logs a
// warning (at most one a minute each) when the 3D view stalls or runs slow, or the processor stays busy.
void perf_tick(const char* where);
// The trail's sink: each action as a line in the log, area "do" (studio/diag/trail.h)
void log_trail_line(const char* line);
// this run's log and performance file (Report a problem puts both in its zip), with everything written so far on disk
std::wstring log_file_path();   // empty when no log is open
std::wstring perf_file_path();  // empty when no performance file was started
void log_flush();
// the clean-exit line (the last line of a log that ended normally), and the previous run's log when it lacked one (a
// crash or a kill: the next start offers a report); empty when the previous run ended cleanly
void log_exit(int windows, size_t tabs);
// (4.3 part 3, the logging list's items 12, 14 and 16) the window the UI thread's lines belong to ("w2" in each line);
// the program's state every 30 s (the clock, the windows and tabs given, memory, the budgets checked); the most windows
// and tabs open, and the slowest step, for the closing summary
void log_set_window(int window_no);
void watch_forget_io();  // (Report a problem: its own temporary files left out of the disk check)
void log_state(const char* windows);
void log_note_open(int windows, int tabs);
void log_note_step(const char* what, double ms);
std::wstring previous_run_unclean();

// the 3D views' frame times since the last call (all windows together): frames drawn, average and worst CPU time to
// draw one, in milliseconds (the GPU's own time isn't measured). viewer.cpp
void viewer_frame_stats(int& frames, double& avg_ms, double& worst_ms);
// the page's draws since the last call (BeginDraw to EndDraw, all windows). app.cpp
void page_frame_stats(int& frames, double& avg_ms, double& worst_ms);

// ---- theme.cpp ----
struct Palette {
    bool dark = true;
    D2D1_COLOR_F bg, panel, panel2, line, text, mute, accent, on_accent, stage_hi, stage_lo, chip;
    // the v3 tokens (the design sandbox's v3, after the design chat's mockup 23, 2026-11-18): the window strip (tab bar), cards
    // and their raised fill, hover, the stronger line, secondary and faint text, the light accent, and the accent's
    // tinted background and line
    D2D1_COLOR_F win, card, card2, hover, line2, text2, faint, accent2, accent_bg, accent_line;
};
// theme: 0 follow Windows, 1 light, 2 dark (OrrePort's shared setting, settings.ini "ui_appearance")
Palette make_palette(int theme);
bool windows_uses_dark();
void apply_title_bar(HWND hwnd, bool dark);
void apply_menu_theme(HWND hwnd, bool dark);  // Windows' popup menus dark or light with Studio's theme (theme.cpp)
// the O (OrrePort's icons recoloured to sage), as the window's icons; switches with the theme
void apply_window_icons(HWND hwnd, const std::wstring& icon_dir, bool dark);
std::string load_text_file(const std::wstring& path);
void svg_fit(std::string& svg);

// ---- viewer.cpp: the 3D view (WebGPU through Dawn, in a child window) ----
class Viewer {
public:
    // A Viewer moves when windows' states are swapped (multi-window plan step 2), so its child window doesn't keep the
    // Viewer's address: it keeps its Impl's (on the heap, never moving), and the Impl points back at whichever Viewer
    // owns it now (moving a Viewer updates that pointer). Copying is not allowed.
    Viewer() = default;
    Viewer(Viewer&& o) noexcept { take(o); }
    Viewer& operator=(Viewer&& o) noexcept { if (this != &o) take(o); return *this; }
    Viewer(const Viewer&) = delete;
    Viewer& operator=(const Viewer&) = delete;
    bool create(HWND parent, HINSTANCE inst);
    // frees the view: its child window, then everything it holds (model, textures, frames, particles). Called when a
    // window that isn't the last closes (2026-11-22: a closed window's view was never freed, about 12 MB a window, found
    // by the --window-cycle test after snatcherLeo's multi-window stress test). Not a destructor on purpose: Studio's last
    // window's view lives in a static that would be destroyed after the graphics device at exit.
    void destroy();
    HWND hwnd() const { return hwnd_; }
    void set_model(OrreModel* model);  // takes ownership (frees it later); nullptr clears
    void set_colours(const D2D1_COLOR_F& hi, const D2D1_COLOR_F& lo);
    void move(int x, int y, int w, int h);
    void set_spin(bool on);
    bool spin() const { return spin_; }
    // what the model does (snatcherLeo, 2026-11-18: "Yes, idle default"): true = its idle animation (the PKX's slot 0) loops
    // at the battle's speed; false = the standing (bind) pose
    void set_idle(bool on);
    bool idle() const { return idle_; }
    bool has_idle() const;  // the current model has an idle animation
    bool has_model() const; // a model is shown (false while the next one loads)
    void hold_frame(int frame) { hold_ = frame; }  // screenshot tests: freeze the idle loop on one frame (-1: run)
    // screenshot tests: render the `frames` move frames before the held one, one after another, and leave the next
    // render on the held frame with a last picture behind it, so a screen blur (Agility's, Double Team's trails) shows
    // in the shot as it does while playing. Why: the blur lays the last finished picture over the new one, and a held
    // frame alone has none (every platform; the render itself is the viewer's WebGPU path)
    void walk_into_hold(int frames);
    void set_paused(bool on);  // a tab drag: the idle loop and spin hold, then carry on from where they were
    // the page cuts the view to nothing (an empty window region) instead of hiding it, so it keeps its last picture
    // and comes back without a white flash; frozen while cut. framed(): a frame with a model has been presented
    void set_cut(bool on);
    bool cut() const;  // cut to nothing now (set_cut)
    bool framed() const;
    // The 3D view drawn into the page (2026-11-22): a texture the page made on its Direct3D 11 device, shared by its
    // DXGI NT handle with a keyed mutex; each frame is copied into it and the page told (WM_APP + 30). true when Dawn
    // took it (the sharing features are there and the import worked). nullptr / 0 stops sharing. Windows only
    bool share_into(void* nt_handle, int w, int h);
    int shared_frames() const;
    void shared_frame_size(int& w, int& h) const;  // the last shared frame's size (the texture's top-left part)
    void set_page_mode(bool on);  // frames into a texture of its own, for the page (see share_into)
    void request_frame();         // a new frame, asked for (a repaint, or a posted message in page mode)
    // comparisons: the game's camera (view rows as logged, fov in degrees, aspect) and the model's placement
    void set_game_camera(const float view_rows[12], float fov_deg, float aspect, const float pos[3], const float rot[3]);
    // one frame of the game's battle camera (eye, interest, fov in degrees; the battle's space) held while a move plays
    void set_battle_camera_shot(const float eye[3], const float interest[3], float fov_deg);
    bool paused() const;
    // The Moves tab's preview (2026-11-19, studio/moves-tab-plan.md): one animation played once from its first frame,
    // then back to the idle loop (or the standing pose); the parent gets WM_APP + 22 when it ends. held: paused on a
    // frame by the user (Play / Pause), and step() moves one whole frame while held (starting `anim` held on its first
    // frame when nothing plays). Animations are the model's own indices (slot_animation turns a PKX slot into one).
    // the move's own effect files for the next play of `anim`: the user's half and the target's (played on the target
    // box; NULL when none). Owned by the viewer; NULL clears; release step 4
    void set_move_effect(OrreWzx* wzx, OrreWzx* target, int move, int part, int anim);
    // the user's third part (its part 3 effect file) played after the target's half ends (the project's preview rule for the
    // drains, 2026-11-20: Absorb, Mega Drain...); called after set_move_effect; owned by the viewer; NULL = none
    void set_move_after(OrreWzx* after);
    // a target Pokémon in the target box's place (owned by the viewer; NULL = the box): the move-sweep comparisons
    void set_target_model(OrreModel* model);
    bool play_once(int anim);
    void stop_once();
    int playing() const;  // the animation being played once, or -1
    void set_held(bool on);
    bool held() const;
    void step(int anim, int delta);
    int once_frame() const;
    int animation_frames(int anim) const;  // whole frames (0: no such animation)
    // A held ending (snatcherLeo, 2026-11-19, for the faint: "extend the duration timeline, in case someone wants to see the
    // entire thing of the Pokémon on the ground"): `anim` plays on for `frames` more on its last pose; the timeline
    // shows them. One animation at a time (anim -1: none)
    void set_play_tail(int anim, int frames);
    int timeline_frames(int anim) const;  // animation_frames plus its held ending
    int slot_animation(int slot) const;    // the PKX slot's animation on this model (-1: none)
    // the slot's own animation without the game's fallbacks: -1 empty (11 plays the idle), -2 not in this model's table
    // (the Moves tab's Reactions grey these out instead of showing a copy of the idle, 2026-11-19)
    int slot_own(int slot) const;
    // the shown model's animations (the Animations tab, 2026-11-22): how many, and whether the eyes' texture animations
    // (0 closing, 1 opening, 2 the blink) use animation `anim`
    int animation_count() const;
    bool eye_animation(int anim) const;
    // the target Pokémon's own animation for the move's target half (-3 none to show; -1 / -2 as slot_own)
    int target_reaction(int* slot) const;
    // hold animation anim on frame (the timeline's playhead; starts it when another one plays)
    void seek(int anim, int frame);
    // the camera also keeps this animation in view (the Moves tab's chosen move; -1: only the idle / standing pose)
    void set_frame_animation(int anim);
    // Plain stage: the developers' move viewer's camera angle (eye (-40, 44, 70) looking at (-40, 14, 0), vertical fov
    // 60, the model at (-40, 0, 0) turned +90 degrees about y: the project's [waza_viewer] log, docs/port/move-viewer.md) on its
    // flat grey backdrop (0x7F7F7F), the Pokémon framed to fill the stage (the game's own distance shows it tiny);
    // dragging turns around it from that angle; the window's own aspect, so nothing is stretched
    void set_plain_stage(bool on);
    void set_stage_camera(bool studio);  // Studio's camera on every stage (the Overview) or the game's (the Moves tab)
    // the Moves tab's Arena stage (snatcherLeo, 2026-11-19): the Phenac Colosseum room around the Pokémon, with Plain
    // stage's camera; the model is handed over once (taken: m is cleared)
    void set_arena_model(OrreModel* m);
    bool has_arena_model() const;
    void set_arena(bool on);
    bool plain_stage() const;
    // the model's own particle effects (Charizard's tail flame...), simulated by the core on the animation's frames
    void set_particles(bool on);
    bool particles() const;
    // in-game lighting: the game's arena lights per vertex and its TEV order (on) or Studio's own light (off)
    void set_game_lighting(bool on);
    // the battle's camera films a move with its effects (on), or Studio's own camera stays (off; release step 4)
    void set_battle_camera(bool on);
    bool game_lighting() const;
    // the preview rate: 0 = the game's (whole frames, 30 a second), else a rate in Hz (60, 120, 144, 165, 240) at which
    // the pose moves between frames (smoothing); capped by the screen's own refresh (the view waits for vsync)
    void set_rate(int hz);
    int rate() const;
    bool has_particles() const;
    void set_particle_warmup(int frames);  // screenshot tests: start the effect this many frames before a held frame
    void set_yaw(float yaw);                       // screenshot tests and comparisons: a fixed angle (radians)
    void reset_view();
    bool view_moved() const;  // turned or zoomed away from reset_view's place
    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }
    void render();
    LRESULT proc(UINT msg, WPARAM wp, LPARAM lp);

private:
    struct Impl;
    Impl* impl_ = nullptr;
    HWND hwnd_ = nullptr;
    void take(Viewer& o);  // moves o's view into this one (and points its Impl here)
    void update_timer();
    bool animating() const;  // something moves: the spin, a move playing, or the idle loop
    void frame_model();  // aims the camera at what is drawn (standing pose, or across the idle loop)
    bool advance_idle();  // poses and uploads the next whole animation frame; true when the pose changed
    void show_standing();  // uploads the standing (bind) pose
    bool ok_ = false, spin_ = false, idle_ = true;  // spin off by default (snatcherLeo, 2026-11-18)
    int hold_ = -1;
    int hold_walk_ = -1;  // walk_into_hold: the frame shown instead of hold_ while walking in (-1: none)
    std::string error_;
};

}  // namespace studio
