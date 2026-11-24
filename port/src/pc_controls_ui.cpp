/*
 * pc_controls_ui.cpp: the controller setup screen (Settings > Controls > Set up controller; snatcherLeo, 2026-11-12: "Yes,
 * build it"; mockups 17 / 18; reworked the same day after his hand check, mockups 20-22 in
 * build-pc/mockups/ingame/c20, "Build it"; docs/port/controls.md).
 *
 * A GameCube controller (the outline traced from the real one, OrrePort's own flat buttons with darker edges:
 * ui/controls/, written by tools/local/brand/gcpad_stage.py) with each button labelled by what presses it on the
 * device being set up. The labels' lines end in a dot on their button and never cross; the selected label's line is
 * lit. The D-pad has a row per direction. A button pressed now lights its ring.
 *
 * The side panel, per stick (Control stick / C-Stick): a live view (the GameCube's octagonal gate, the dead zone,
 * where the stick physically is and what the game is sent), Calibrate (roll the stick around its edge: its reach and
 * centre are recorded), Range (the GameCube's by default, locked in GameCube-accurate mode), the dead zone (capped at
 * 10 in accurate mode), the walk button or key and its speed, snap to 8 directions; then rumble, printed letters and
 * the OrrePort menu shortcut (one or two buttons). The shaping itself is pc_pad_shape.cpp's.
 *
 * Changing a button: choose its label, then press what you want (5 seconds); if that button or key already did
 * something else, the two swap, and the note under the controller says exactly what changed. The menu shortcut: hold
 * one or two buttons, then let go. Moving around: by position (up / down within a column, left / right to the nearest
 * row of the next column); sliders take left / right, and the mouse drags them. Leaving after a change asks
 * Save / Discard; Discard puts back everything as it was when the screen opened.
 *
 * Buttons go through Aurora's own remapping (dolphin/pad.h: PADSetButtonMapping, PADSetKeyButtonBinding,
 * PADSetRumbleIntensity, PADSerializeMappings), saved in Aurora's per-controller files and keyboard_bindings.dat; the
 * stick and shortcut settings in settings.ini. Player 1 only for now. Platform layer only.
 */
#include <aurora/input.hpp>
#include <aurora/rmlui.hpp>
#include <dolphin/pad.h>

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <optional>
#include <span>
#include <cstring> /* std::strcmp: shortcut_shown */
#include <string>
#include <tuple>
#include <vector>

#include "pc_pad_shape.h"
#include "pc_settings.h"
#include "pc_text.h" /* every word in the player's language (ui/lang/<code>.txt) */
#include "pc_paths.h" /* UI\ and data\settings\profiles\ (4.1) */

bool pc_svg_render(const std::string& svg, int width, int& height, std::vector<unsigned char>& rgba,
                   const std::vector<std::string>& skip_fills);

extern "C" {
int pc_ui_theme_is_dark(void);         /* pc_settings_overlay.cpp */
int pc_option_gamecube_accurate(void); /* pc_options.c */
int pc_option_accurate_locked(void);   /* pc_options.c: the mode is fixed (a game has started) */
int pc_platform_automated_run(void);
const char* pc_platform_data_path(const char* name, char* out, size_t size); /* pc_platform.c: <exe folder>/<name> */
}

namespace {
namespace input = aurora::input;

/* GameCube-accurate mode's lock on this screen (the stick's range, the dead zone note): only while a game runs in
 * accurate mode (2.12, 2026-11-16). Since accurate mode is each save's own setting, the hub (before a game starts) shows
 * the player's own choices, like Settings' rows do; the stick itself uses the GameCube's range in an accurate game
 * whatever is set (pc_pad_shape.cpp asks the mode directly). All platforms. */
bool accurate_in_game() { return pc_option_accurate_locked() != 0 && pc_option_gamecube_accurate() != 0; }

std::string ui_path(const char* file) { return pc_paths_str(PC_PLACE_UI, file); }

/* ---- the device and its names ---- */
enum Kind { KIND_XBOX, KIND_PLAYSTATION, KIND_GAMECUBE, KIND_DECK };

SDL_Gamepad* port_gamepad() {
    const s32 index = PADGetIndexForPort(0);
    return index >= 0 ? PADGetSDLGamepadForIndex((u32)index) : nullptr;
}

Kind kind_of(SDL_Gamepad* gp) {
    /* ORRE_SETTINGS_TEST_CTRL (test switch, look only; pc_settings_overlay.cpp): name the buttons as that kind of
     * controller prints them, so pictures of each device's labels need no such controller plugged in */
    if (const char* t = SDL_getenv("ORRE_SETTINGS_TEST_CTRL")) {
        if (SDL_strcmp(t, "playstation") == 0) return KIND_PLAYSTATION;
        if (SDL_strcmp(t, "xbox") == 0) return KIND_XBOX;
    }
    const char* name = gp != nullptr ? SDL_GetGamepadName(gp) : nullptr;
    if (PADIsGCAdapter(0) || (name != nullptr && SDL_strcasestr(name, "gamecube") != nullptr)) {
        return KIND_GAMECUBE;
    }
    if (name != nullptr && SDL_strcasestr(name, "steam deck") != nullptr) {
        return KIND_DECK;
    }
    switch (gp != nullptr ? SDL_GetGamepadType(gp) : SDL_GAMEPAD_TYPE_UNKNOWN) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5: return KIND_PLAYSTATION;
    default: return KIND_XBOX;
    }
}

/* a controller button's name as printed on that kind of controller */
std::string button_name(u32 native, Kind kind) {
    switch ((SDL_GamepadButton)native) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return kind == KIND_PLAYSTATION ? "&#10005;" : "A";
    case SDL_GAMEPAD_BUTTON_EAST:
        return kind == KIND_PLAYSTATION ? "&#9675;" : kind == KIND_GAMECUBE ? "X" : "B";
    case SDL_GAMEPAD_BUTTON_WEST:
        return kind == KIND_PLAYSTATION ? "&#9633;" : kind == KIND_GAMECUBE ? "B" : "X";
    case SDL_GAMEPAD_BUTTON_NORTH:
        return kind == KIND_PLAYSTATION ? "&#9651;" : "Y";
    case SDL_GAMEPAD_BUTTON_BACK:
        return kind == KIND_PLAYSTATION ? "Create" : kind == KIND_GAMECUBE ? "Z" : "View";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Home";
    case SDL_GAMEPAD_BUTTON_START:
        return kind == KIND_PLAYSTATION ? "Options" : kind == KIND_GAMECUBE ? "START" : "Menu";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return kind == KIND_PLAYSTATION ? "L3" : "LS";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return kind == KIND_PLAYSTATION ? "R3" : "RS";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return kind == KIND_PLAYSTATION ? "L1" : kind == KIND_GAMECUBE ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return kind == KIND_PLAYSTATION ? "R1" : kind == KIND_GAMECUBE ? "Z" : "RB";
    /* *Updated 2026-11-13 (every word translatable):* names printed on the controllers (A, LB, L1, Create, Options,
     * View, Menu, Home, Share) stay as printed; descriptive words (D-pad, Paddle, Touchpad, none) are text keys */
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return tx("controls.dpad") + " &#8593;";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return tx("controls.dpad") + " &#8595;";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return tx("controls.dpad") + " &#8592;";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return tx("controls.dpad") + " &#8594;";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Share";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return tx("controls.paddle", {{"name", "R1"}});
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return tx("controls.paddle", {{"name", "L1"}});
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return tx("controls.paddle", {{"name", "R2"}});
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return tx("controls.paddle", {{"name", "L2"}});
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return tx("controls.touchpad");
    case SDL_GAMEPAD_BUTTON_MISC3: return "L";
    case SDL_GAMEPAD_BUTTON_MISC4: return "R";
    default: break;
    }
    if (native == PAD_NATIVE_BUTTON_INVALID) {
        return tx("controls.none");
    }
    const char* s = SDL_GetGamepadStringForButton((SDL_GamepadButton)native);
    return s != nullptr ? s : "?";
}

std::string trigger_name(bool left, Kind kind) {
    return kind == KIND_PLAYSTATION ? (left ? "L2" : "R2") : kind == KIND_GAMECUBE ? (left ? "L" : "R")
                                                                                    : (left ? "LT" : "RT");
}

/* a keyboard key's name: the few OrrePort names itself are text keys (keyboards print them in their own language, e.g.
 * German "Eingabe", "Umschalt"); the rest are SDL's names, which come in English from SDL on every OS (an exception to
 * the every-word rule, like the system's own error text) */
std::string key_name(s32 scancode) {
    if (scancode < 0) {
        return tx(scancode == PAD_KEY_INVALID ? "controls.none" : "controls.mouse");
    }
    switch (scancode) {
    case SDL_SCANCODE_RETURN: return tx("settings.key.enter");
    case SDL_SCANCODE_LSHIFT: return tx("controls.key.lshift");
    case SDL_SCANCODE_RSHIFT: return tx("controls.key.rshift");
    case SDL_SCANCODE_LCTRL: return tx("controls.key.lctrl");
    case SDL_SCANCODE_RCTRL: return tx("controls.key.rctrl");
    case SDL_SCANCODE_LALT: return tx("controls.key.lalt");
    case SDL_SCANCODE_ESCAPE: return "Esc";
    case SDL_SCANCODE_UP: return "&#8593;";
    case SDL_SCANCODE_DOWN: return "&#8595;";
    case SDL_SCANCODE_LEFT: return "&#8592;";
    case SDL_SCANCODE_RIGHT: return "&#8594;";
    default: break;
    }
    const char* s = SDL_GetScancodeName((SDL_Scancode)scancode);
    return s != nullptr && *s != '\0' ? s : "?";
}

/* a GameCube button's own name */
std::string gc_name(u32 pad) {
    switch (pad) {
    case PAD_BUTTON_A: return "A";
    case PAD_BUTTON_B: return "B";
    case PAD_BUTTON_X: return "X";
    case PAD_BUTTON_Y: return "Y";
    case PAD_TRIGGER_Z: return "Z";
    case PAD_TRIGGER_L: return "L";
    case PAD_TRIGGER_R: return "R";
    case PAD_BUTTON_START: return "START";
    case PAD_BUTTON_UP: return tx("controls.dpad.up");
    case PAD_BUTTON_DOWN: return tx("controls.dpad.down");
    case PAD_BUTTON_LEFT: return tx("controls.dpad.left");
    case PAD_BUTTON_RIGHT: return tx("controls.dpad.right");
    default: return "?";
    }
}

/* ---- the screen's slots ---- */
enum SlotKind {
    S_BUTTON, S_STICK, S_DEVICE, S_PROFILE, S_CALIB, S_DIR, S_RANGE, S_DEAD, S_WALK, S_WALKPCT, S_SNAP, S_RUMBLE,
    S_PRINTED, S_MENU, S_FF, S_QUITSC, S_RESET, S_SAVE
};
struct Slot {
    SlotKind kind;
    const char* key;  /* the label / ring / line id in the layout (controls/gcpad-layout.rcss) */
    const char* name; /* the GameCube's name for it: a button letter as printed, or a text key ("controls.slot.*"),
                       * shown through slot_name() */
    u32 pad;          /* S_BUTTON: the GameCube button; S_DIR: the stick direction (PAD_AXIS_); S_CALIB / S_RANGE /
                       * S_DEAD: the stick (0 the Control stick, 1 the C-Stick) */
};
const std::vector<Slot> kSlots = {
    {S_BUTTON, "L", "L", PAD_TRIGGER_L},
    {S_STICK, "STICK", "controls.slot.control_stick", 0},
    {S_BUTTON, "DPAD", "controls.slot.up", PAD_BUTTON_UP},
    {S_BUTTON, "DPAD", "controls.slot.down", PAD_BUTTON_DOWN},
    {S_BUTTON, "DPAD", "controls.slot.left", PAD_BUTTON_LEFT},
    {S_BUTTON, "DPAD", "controls.slot.right", PAD_BUTTON_RIGHT},
    {S_BUTTON, "START", "START", PAD_BUTTON_START},
    {S_BUTTON, "R", "R", PAD_TRIGGER_R},
    {S_BUTTON, "Z", "Z", PAD_TRIGGER_Z},
    {S_BUTTON, "Y", "Y", PAD_BUTTON_Y},
    {S_BUTTON, "X", "X", PAD_BUTTON_X},
    {S_BUTTON, "A", "A", PAD_BUTTON_A},
    {S_BUTTON, "B", "B", PAD_BUTTON_B},
    {S_STICK, "C", "controls.slot.c_stick", 0},
    {S_DEVICE, "", "controls.slot.device", 0},
    {S_PROFILE, "", "controls.slot.profile", 0},
    /* the side panel's two stick columns, as Dolphin lays them out (snatcherLeo, 2026-11-12, mockups 41 / 42) */
    {S_CALIB, "", "controls.slot.calibrate", 0},
    {S_DIR, "", "controls.slot.up", PAD_AXIS_LEFT_Y_POS},
    {S_DIR, "", "controls.slot.down", PAD_AXIS_LEFT_Y_NEG},
    {S_DIR, "", "controls.slot.left", PAD_AXIS_LEFT_X_NEG},
    {S_DIR, "", "controls.slot.right", PAD_AXIS_LEFT_X_POS},
    {S_CALIB, "", "controls.slot.calibrate", 1},
    {S_DIR, "", "controls.slot.up", PAD_AXIS_RIGHT_Y_POS},
    {S_DIR, "", "controls.slot.down", PAD_AXIS_RIGHT_Y_NEG},
    {S_DIR, "", "controls.slot.left", PAD_AXIS_RIGHT_X_NEG},
    {S_DIR, "", "controls.slot.right", PAD_AXIS_RIGHT_X_POS},
    {S_DEAD, "", "controls.slot.dead_zone", 0},
    {S_RANGE, "", "controls.slot.range", 0},
    {S_DEAD, "", "controls.slot.dead_zone", 1},
    {S_RANGE, "", "controls.slot.range", 1},
    {S_WALK, "", "controls.slot.walk", 0},
    {S_WALKPCT, "", "controls.slot.walk_speed", 0},
    {S_SNAP, "", "controls.slot.snap", 0},
    {S_RUMBLE, "", "controls.slot.rumble", 0},
    {S_PRINTED, "", "controls.slot.printed", 0},
    {S_MENU, "", "controls.slot.menu", 0},
    /* the fast-forward button (snatcherLeo, 2026-11-16, mockup s4: under the menu shortcut; one button or key per device,
     * pc_pad_shape.cpp; Settings › Enhancements › Speed's link opens this screen on it). UI id controls.slot.ff */
    {S_FF, "", "controls.slot.ff", 0},
    {S_QUITSC, "", "settings.quit", 0},
    {S_RESET, "", "controls.slot.reset", 0},
    {S_SAVE, "", "common.save", 0},
};

/* a slot's name in the player's language (a button letter stays as printed) */
std::string slot_name(const Slot& s) {
    const std::string n = s.name;
    return n.find('.') != std::string::npos ? tx(s.name) : n;
}

/* a value shown bold inside a sentence ("A is now <b>X</b>.") */
std::string bold(const std::string& s) { return "<b>" + s + "</b>"; }

/* a built-in profile's name in the player's language; "Default" and "GameCube Controller" are also their files' names,
 * so the name itself never changes, only how it is shown. A player's own profile keeps the name they gave it. */
std::string profile_shown(const std::string& name) {
    if (name == "Default") return tx("controls.profile.default");
    if (name == "GameCube Controller") return tx("controls.profile.gamecube");
    return name;
}

/* a shortcut's name as shown ("Z + L"); "None" (no shortcut) in the player's language */
std::string shortcut_shown(const char* name) {
    return name == nullptr || std::strcmp(name, "None") == 0 ? tx("common.none") : std::string(name);
}
const char* const kArrows[] = {"&#8593;", "&#8595;", "&#8592;", "&#8594;"};

int slot_of(SlotKind k) {
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        if (kSlots[i].kind == k) return i;
    }
    return 0;
}
int slot_of_key(const std::string& key) {
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        if (key == kSlots[i].key) return i;
    }
    return -1;
}
bool is_dpad(int i) { return std::string(kSlots[i].key) == "DPAD"; }
/* the stick a side-panel slot belongs to (its column) */
int stick_of(int i) {
    const Slot& s = kSlots[i];
    switch (s.kind) {
    case S_DIR: return s.pad >= PAD_AXIS_RIGHT_X_POS ? PC_STICK_C : PC_STICK_MAIN;
    case S_CALIB:
    case S_RANGE:
    case S_DEAD: return (int)s.pad;
    default: return PC_STICK_MAIN;
    }
}
int slot_of_stick(SlotKind k, int stick) {
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        if (kSlots[i].kind == k && stick_of(i) == stick) return i;
    }
    return 0;
}
int slot_of_axis(u32 axis) {
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        if (kSlots[i].kind == S_DIR && kSlots[i].pad == axis) return i;
    }
    return 0;
}
/* each slot's element: the labels by their layout id, the D-pad's rows, the rest "slot-N" */
std::string elem_id(int i) {
    const Slot& s = kSlots[i];
    if ((s.kind == S_BUTTON || s.kind == S_STICK) && !is_dpad(i)) return std::string("lab-") + s.key;
    if (s.kind == S_DEVICE) return "dev";
    if (s.kind == S_PROFILE) return "prof";
    return "slot-" + std::to_string(i);
}

/* the GameCube's own colours for each button's badge */
struct Badge {
    const char* text;
    const char* bg;
    const char* fg;
    bool sq;
};
const std::map<std::string, Badge> kBadges = {
    {"A", {"A", "#22B39A", "#FFFFFF", false}},     {"B", {"B", "#E0262A", "#FFFFFF", false}},
    {"X", {"X", "#D9DCE3", "#2A2550", false}},     {"Y", {"Y", "#D9DCE3", "#2A2550", false}},
    {"Z", {"Z", "#4C3FD6", "#FFFFFF", false}},     {"START", {"START", "#D9DCE3", "#2A2550", true}},
    {"L", {"L", "#D9DCE3", "#2A2550", true}},      {"R", {"R", "#D9DCE3", "#2A2550", true}},
    {"STICK", {"&#9673;", "#D9DCE3", "#2A2550", true}}, {"C", {"C", "#F2C318", "#2A2550", false}},
    {"DPAD", {"+", "#D9DCE3", "#2A2550", true}},
};

/* ---- the screen's state ---- */
Rml::ElementDocument* s_doc = nullptr;
input::LayerId s_layer = input::kInvalidLayerId;
bool s_open = false;
bool s_kb = false;      /* setting up the keyboard (else Player 1's controller) */
bool s_dirty = false;   /* something changed since the screen opened */
bool s_asking = false;  /* the Save / Discard question */
bool s_listing = false; /* the device list open */
int s_list_focus = 0;
int s_prompt_focus = 0;
int s_focus = 0;
int s_stick = PC_STICK_MAIN; /* the stick the side panel shows */
int s_listen = -1;           /* the slot waiting for a press; -1: none */
bool s_listen_armed = false; /* everything was let go since the wait began */
int s_listen_event = -1;     /* a press event that came while waiting (a key's scancode or a controller button) */
Uint64 s_listen_until = 0;
std::vector<PcShortcutMember> s_combo; /* the menu shortcut being recorded, in press order */
std::string s_note;
bool s_note_ok = false;      /* the note says what changed (green) */
int s_drag = -1;             /* a slider being dragged with the mouse: its slot */
float s_drag_left = 0, s_drag_width = 1;
/* profiles (snatcherLeo, 2026-11-13: "Can we allow for profiles now, where I can save something, and maybe it lets me name
 * this profile, and I can select a profile?"; mockup 43, "Build it") */
std::string s_profile;               /* the profile on screen */
std::vector<std::string> s_profiles; /* this device's: the built-in one first, then the saved ones by name */
bool s_plisting = false;             /* the profile list open */
int s_plist_focus = 0;
/* D_FF: "{button} is your {what} in the game. Use it for fast-forward anyway, or press another button?" (2026-11-16) */
enum Dialog { D_NONE, D_SWITCH, D_NAME_NEW, D_NAME_RENAME, D_DELETE, D_FF };
Dialog s_dialog = D_NONE;
PcShortcutMember s_ff_pending{}; /* D_FF: the button or key picked, and the game button it already presses */
unsigned s_ff_pending_gc = 0;
int s_scroll_frames = 0; /* frames until the side panel scrolls its focused row into view (0: nothing pending) */

/* the side panel's focused (or listening) row into view, when the panel is taller than the window (2026-11-16) */
void scroll_side_to_focus() {
    const int lit = s_listen >= 0 ? s_listen : s_focus;
    if (lit < 0 || s_doc == nullptr) return;
    const std::string id = elem_id(lit);
    Rml::Element* row = id.rfind("slot-", 0) == 0 ? s_doc->GetElementById(id) : nullptr;
    if (row != nullptr) {
        row->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest,
                                                       Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest));
    }
}
int s_dialog_focus = 0;
std::string s_pending;               /* D_SWITCH: the profile to load once the question is answered */
int s_name_focus_frames = 0;         /* frames until the name box takes the keyboard (once it is laid out) */
std::string s_name_error;
/* calibrating: the reach in each of PC_CAL_SAMPLES directions, and how far the stick has turned */
bool s_calib = false;
float s_cal_bins[PC_CAL_SAMPLES];
bool s_cal_was_complete = false;
int s_cal_last_bin = -1; /* the direction at the last look, for filling the ones a quick roll skips */
float s_cal_last_r = 0;
/* every direction reached (Dolphin's rule for offering Finish) */
bool cal_complete() {
    for (float b : s_cal_bins) {
        if (b <= 0) return false;
    }
    return true;
}

/* ---- the settings the screen changes, for Discard ---- */
const char* const kIntKeys[] = {"stick0_range", "stick1_range", "stick0_dead", "stick1_dead", "stick_snap",
                                "walk_button",  "walk_key",     "walk_pct",    "stick0_cx",   "stick0_cy",
                                "stick1_cx",    "stick1_cy",    "menu_key",    "menu_button"};
const int kIntDefaults[] = {101, 92, 8, 8, 0, -1, SDL_SCANCODE_LCTRL, 50, 0, 0, 0, 0, SDL_SCANCODE_ESCAPE, -1};
/* ff_pad / ff_keys: the fast-forward button (2026-11-16): kept with the screen's other settings, so Discard puts it
 * back and a profile carries it (appended: older profile files simply lack the lines and keep the current value) */
const char* const kStrKeys[] = {"stick0_cal", "stick1_cal", "menu_pad", "menu_keys", "quit_pad", "ff_pad", "ff_keys"};

struct Snapshot {
    std::array<PADButtonMapping, PAD_BUTTON_COUNT> buttons{};
    u32 button_count = 0;
    std::array<PADKeyButtonBinding, PAD_BUTTON_COUNT> keys{};
    u32 key_count = 0;
    std::array<PADAxisMapping, PAD_AXIS_COUNT> axes{}; /* the stick directions on the controller */
    u32 axis_count = 0;
    std::array<PADKeyAxisBinding, PAD_AXIS_COUNT> key_axes{}; /* ... and on the keyboard */
    u32 key_axis_count = 0;
    u16 rumble_low = 0, rumble_high = 0;
    bool has_rumble = false;
    std::vector<int> ints;
    std::vector<std::string> strs;
};
Snapshot s_before, s_defaults;

Snapshot take() {
    Snapshot s;
    u32 n = 0;
    if (PADButtonMapping* m = PADGetButtonMappings(0, &n)) {
        s.button_count = SDL_min(n, (u32)PAD_BUTTON_COUNT);
        for (u32 i = 0; i < s.button_count; ++i) s.buttons[i] = m[i];
    }
    if (PADKeyButtonBinding* k = PADGetKeyButtonBindings(0, &n)) {
        s.key_count = SDL_min(n, (u32)PAD_BUTTON_COUNT);
        for (u32 i = 0; i < s.key_count; ++i) s.keys[i] = k[i];
    }
    if (PADAxisMapping* a = PADGetAxisMappings(0, &n)) {
        s.axis_count = SDL_min(n, (u32)PAD_AXIS_COUNT);
        for (u32 i = 0; i < s.axis_count; ++i) s.axes[i] = a[i];
    }
    if (PADKeyAxisBinding* ka = PADGetKeyAxisBindings(0, &n)) {
        s.key_axis_count = SDL_min(n, (u32)PAD_AXIS_COUNT);
        for (u32 i = 0; i < s.key_axis_count; ++i) s.key_axes[i] = ka[i];
    }
    s.has_rumble = PADGetRumbleIntensity(0, &s.rumble_low, &s.rumble_high) != FALSE;
    for (int i = 0; i < (int)SDL_arraysize(kIntKeys); ++i) s.ints.push_back(pc_settings_int(kIntKeys[i], kIntDefaults[i]));
    for (const char* k : kStrKeys) s.strs.push_back(pc_settings_string(k, ""));
    return s;
}

void put(const Snapshot& s) {
    for (u32 i = 0; i < s.button_count; ++i) PADSetButtonMapping(0, s.buttons[i]);
    for (u32 i = 0; i < s.key_count; ++i) PADSetKeyButtonBinding(0, s.keys[i]);
    for (u32 i = 0; i < s.axis_count; ++i) PADSetAxisMapping(0, s.axes[i]);
    for (u32 i = 0; i < s.key_axis_count; ++i) PADSetKeyAxisBinding(0, s.key_axes[i]);
    if (s.has_rumble) PADSetRumbleIntensity(0, s.rumble_low, s.rumble_high);
    for (int i = 0; i < (int)s.ints.size(); ++i) pc_settings_set_int(kIntKeys[i], s.ints[i]);
    for (int i = 0; i < (int)s.strs.size(); ++i) pc_settings_set_string(kStrKeys[i], s.strs[i].c_str());
    pc_stick_clear_calibration(PC_STICK_MAIN); /* drops the cached calibration; the strings above set it again */
    pc_settings_set_string(kStrKeys[0], s.strs.size() > 0 ? s.strs[0].c_str() : "");
    pc_settings_set_int("stick0_cx", s.ints.size() > 8 ? s.ints[8] : 0);
    pc_settings_set_int("stick0_cy", s.ints.size() > 9 ? s.ints[9] : 0);
    pc_stick_clear_calibration(PC_STICK_C);
    pc_settings_set_string(kStrKeys[1], s.strs.size() > 1 ? s.strs[1].c_str() : "");
    pc_settings_set_int("stick1_cx", s.ints.size() > 10 ? s.ints[10] : 0);
    pc_settings_set_int("stick1_cy", s.ints.size() > 11 ? s.ints[11] : 0);
}

/* the buttons' defaults, read by restoring them and putting the current ones back; the settings' defaults from the
 * table above */
Snapshot read_defaults() {
    const Snapshot now = take();
    PADRestoreDefaultMapping(0);
    PADRestoreDefaultKeyBindings(0);
    Snapshot d = take();
    put(now);
    d.ints.assign(std::begin(kIntDefaults), std::end(kIntDefaults));
    d.strs.assign(SDL_arraysize(kStrKeys), std::string());
    d.has_rumble = true;
    d.rumble_low = d.rumble_high = 32767;
    return d;
}

/* ---- profiles: everything on this screen under a name, one small text file each ----
 * user\profiles\controller\<name>.txt and user\profiles\keyboard\<name>.txt, next to the exe (the portable folder):
 * the player owns them, can back them up and share them. A controller profile holds the buttons, the stick directions
 * and rumble; a keyboard profile the keys; both hold the screen's settings (sticks, walk, snap, the menu shortcut).
 * Each line is "<what> <values>", so the files stay readable. The built-in profile ("Default", or "GameCube Controller"
 * on a GameCube controller) is a file too once saved; with no file it is the factory defaults. */
/* data\settings\profiles\ in the portable layout (the project's call, 2026-11-21), user\profiles\ in the flat one (4.1) */
std::string profiles_dir(bool kb) { return pc_paths_str(PC_PLACE_SETTINGS, kb ? "profiles/keyboard/" : "profiles/controller/"); }
std::string deleted_dir(bool kb) {
    return pc_paths_str(PC_PLACE_SETTINGS, kb ? "profiles/keyboard/Deleted/" : "profiles/controller/Deleted/");
}
std::string builtin_profile() {
    return !s_kb && kind_of(port_gamepad()) == KIND_GAMECUBE ? "GameCube Controller" : "Default";
}
/* a name cut to fit its box with "..." (snatcherLeo, 2026-11-13: a long profile name broke the Save button; "we just use a 3
 * dot for things that are logner"): at most `max` characters, never splitting a UTF-8 character */
std::string short_text(const std::string& s, size_t max) {
    if (s.size() <= max) return s;
    size_t cut = max > 1 ? max - 1 : 0;
    while (cut > 0 && (s[cut] & 0xC0) == 0x80) --cut;
    std::string out = s.substr(0, cut);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out + "&#8230;";
}
bool is_builtin(const std::string& name) { return name == "Default" || name == "GameCube Controller"; }
/* the setting that remembers a device's profile: the keyboard's, or a controller model's (its USB vendor and product) */
std::string remember_key() {
    if (s_kb) return "profile_keyboard";
    SDL_Gamepad* gp = port_gamepad();
    char key[64];
    SDL_snprintf(key, sizeof key, "profile_pad_%04x_%04x", gp != nullptr ? SDL_GetGamepadVendor(gp) : 0,
                 gp != nullptr ? SDL_GetGamepadProduct(gp) : 0);
    return key;
}
/* a name a file can carry: letters, digits, spaces and - _ ( ) ' . up to 24 characters, trimmed */
std::string clean_name(const std::string& in) {
    std::string out;
    for (char c : in) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
                        c == '-' || c == '_' || c == '(' || c == ')' || c == '\'' || c == '.';
        if (ok && out.size() < 24) out += c;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return out;
}

void list_profiles() {
    s_profiles.clear();
    s_profiles.push_back(builtin_profile());
    std::vector<std::string> saved;
    int count = 0;
    if (char** files = SDL_GlobDirectory(profiles_dir(s_kb).c_str(), "*.txt", 0, &count)) {
        for (int i = 0; i < count; ++i) {
            std::string n = files[i];
            n = n.substr(0, n.size() - 4);
            if (n.find('/') == std::string::npos && n.find('\\') == std::string::npos && !is_builtin(n)) saved.push_back(n);
        }
        SDL_free(files);
    }
    std::sort(saved.begin(), saved.end(), [](const std::string& a, const std::string& b) {
        return SDL_strcasecmp(a.c_str(), b.c_str()) < 0;
    });
    s_profiles.insert(s_profiles.end(), saved.begin(), saved.end());
}

bool write_profile(const std::string& name, const Snapshot& s) {
    const std::string dir = profiles_dir(s_kb);
    SDL_CreateDirectory(dir.c_str());
    std::string text = std::string("OrrePort ") + (s_kb ? "keyboard" : "controller") + " profile 1\n";
    char line[256];
    if (s_kb) {
        for (u32 i = 0; i < s.key_count; ++i) {
            SDL_snprintf(line, sizeof line, "key %u %d\n", (unsigned)s.keys[i].padButton, (int)s.keys[i].scancode);
            text += line;
        }
        for (u32 i = 0; i < s.key_axis_count; ++i) {
            SDL_snprintf(line, sizeof line, "keyaxis %u %d %d\n", (unsigned)s.key_axes[i].padAxis,
                         (int)s.key_axes[i].scancode, (int)s.key_axes[i].influence);
            text += line;
        }
    } else {
        for (u32 i = 0; i < s.button_count; ++i) {
            SDL_snprintf(line, sizeof line, "button %u %u\n", (unsigned)s.buttons[i].padButton,
                         (unsigned)s.buttons[i].nativeButton);
            text += line;
        }
        for (u32 i = 0; i < s.axis_count; ++i) {
            SDL_snprintf(line, sizeof line, "axis %u %d %d %d\n", (unsigned)s.axes[i].padAxis,
                         (int)s.axes[i].nativeAxis.nativeAxis, (int)s.axes[i].nativeAxis.sign,
                         (int)s.axes[i].nativeButton);
            text += line;
        }
        if (s.has_rumble) {
            SDL_snprintf(line, sizeof line, "rumble %u %u\n", (unsigned)s.rumble_low, (unsigned)s.rumble_high);
            text += line;
        }
    }
    for (int i = 0; i < (int)s.ints.size() && i < (int)SDL_arraysize(kIntKeys); ++i) {
        SDL_snprintf(line, sizeof line, "int %s %d\n", kIntKeys[i], s.ints[i]);
        text += line;
    }
    for (int i = 0; i < (int)s.strs.size() && i < (int)SDL_arraysize(kStrKeys); ++i) {
        text += std::string("str ") + kStrKeys[i] + " " + s.strs[i] + "\n";
    }
    const std::string path = dir + name + ".txt";
    const bool ok = SDL_SaveFile(path.c_str(), text.data(), text.size());
    std::printf("[controls_ui] profile \"%s\" written (%s)%s\n", name.c_str(), s_kb ? "keyboard" : "controller",
                ok ? "" : ": FAILED");
    return ok;
}

/* a profile's file read onto a snapshot of what is set now (lines it doesn't have keep their current value); false
 * when there is no file */
bool read_profile(const std::string& name, Snapshot& s) {
    size_t size = 0;
    char* data = (char*)SDL_LoadFile((profiles_dir(s_kb) + name + ".txt").c_str(), &size);
    if (data == nullptr) return false;
    const std::string text(data, size);
    SDL_free(data);
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string ln = text.substr(at, end - at);
        at = end + 1;
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        unsigned a = 0, b = 0;
        int x = 0, y = 0, z = 0;
        char word[64] = {0};
        if (SDL_sscanf(ln.c_str(), "button %u %u", &a, &b) == 2) {
            for (u32 i = 0; i < s.button_count; ++i) {
                if (s.buttons[i].padButton == a) s.buttons[i].nativeButton = b;
            }
        } else if (SDL_sscanf(ln.c_str(), "axis %u %d %d %d", &a, &x, &y, &z) == 4) {
            for (u32 i = 0; i < s.axis_count; ++i) {
                if (s.axes[i].padAxis == a) s.axes[i] = PADAxisMapping{{x, y < 0 ? AXIS_SIGN_NEGATIVE : AXIS_SIGN_POSITIVE}, z, (PADAxis)a};
            }
        } else if (SDL_sscanf(ln.c_str(), "keyaxis %u %d %d", &a, &x, &y) == 3) {
            for (u32 i = 0; i < s.key_axis_count; ++i) {
                if (s.key_axes[i].padAxis == a) s.key_axes[i] = PADKeyAxisBinding{x, (PADAxis)a, (s16)y};
            }
        } else if (SDL_sscanf(ln.c_str(), "key %u %d", &a, &x) == 2) {
            for (u32 i = 0; i < s.key_count; ++i) {
                if (s.keys[i].padButton == a) s.keys[i].scancode = x;
            }
        } else if (SDL_sscanf(ln.c_str(), "rumble %u %u", &a, &b) == 2) {
            s.has_rumble = true;
            s.rumble_low = (u16)a;
            s.rumble_high = (u16)b;
        } else if (SDL_sscanf(ln.c_str(), "int %63s %d", word, &x) == 2) {
            for (int i = 0; i < (int)SDL_arraysize(kIntKeys) && i < (int)s.ints.size(); ++i) {
                if (SDL_strcmp(word, kIntKeys[i]) == 0) s.ints[i] = x;
            }
        } else if (ln.rfind("str ", 0) == 0) {
            const size_t sp = ln.find(' ', 4);
            const std::string k = ln.substr(4, sp == std::string::npos ? std::string::npos : sp - 4);
            const std::string v = sp == std::string::npos ? std::string() : ln.substr(sp + 1);
            for (int i = 0; i < (int)SDL_arraysize(kStrKeys) && i < (int)s.strs.size(); ++i) {
                if (k == kStrKeys[i]) s.strs[i] = v;
            }
        }
    }
    return true;
}

/* ---- reading and changing one control ---- */
u32 native_for(u32 pad) {
    u32 n = 0;
    PADButtonMapping* m = PADGetButtonMappings(0, &n);
    for (u32 i = 0; m != nullptr && i < n; ++i) {
        if (m[i].padButton == pad) return m[i].nativeButton;
    }
    return PAD_NATIVE_BUTTON_INVALID;
}

s32 key_for(u32 pad) {
    u32 n = 0;
    PADKeyButtonBinding* k = PADGetKeyButtonBindings(0, &n);
    for (u32 i = 0; k != nullptr && i < n; ++i) {
        if (k[i].padButton == pad) return k[i].scancode;
    }
    return PAD_KEY_INVALID;
}

s32 key_for_axis(PADAxis axis) {
    u32 n = 0;
    PADKeyAxisBinding* k = PADGetKeyAxisBindings(0, &n);
    for (u32 i = 0; k != nullptr && i < n; ++i) {
        if (k[i].padAxis == axis) return k[i].scancode;
    }
    return PAD_KEY_INVALID;
}

/* give pad the native button; whatever had it before gets pad's old one (a swap). Returns the swapped GameCube
 * button, 0 for none. */
u32 assign_native(u32 pad, u32 native) {
    const u32 old = native_for(pad);
    u32 swapped = 0;
    u32 n = 0;
    PADButtonMapping* m = PADGetButtonMappings(0, &n);
    for (u32 i = 0; m != nullptr && i < n; ++i) {
        if (m[i].nativeButton == native && m[i].padButton != pad && native != PAD_NATIVE_BUTTON_INVALID) {
            swapped = m[i].padButton;
            PADSetButtonMapping(0, PADButtonMapping{old, m[i].padButton});
        }
    }
    PADSetButtonMapping(0, PADButtonMapping{native, (PADButton)pad});
    return swapped;
}

u32 assign_key(u32 pad, s32 scancode) {
    const s32 old = key_for(pad);
    u32 swapped = 0;
    u32 n = 0;
    PADKeyButtonBinding* k = PADGetKeyButtonBindings(0, &n);
    for (u32 i = 0; k != nullptr && i < n; ++i) {
        if (k[i].scancode == scancode && k[i].padButton != pad) {
            swapped = k[i].padButton;
            PADSetKeyButtonBinding(0, PADKeyButtonBinding{old, k[i].padButton});
        }
    }
    PADSetKeyButtonBinding(0, PADKeyButtonBinding{scancode, (PADButton)pad});
    return swapped;
}

/* ---- the stick directions (snatcherLeo, 2026-11-12: "You click on the "Up" for example, it stays highlighted ... you then
 * move "Up" on your joystick. Afterwards, it would say "Left Y+" like Dolphin"). Aurora keeps a mapping per GameCube
 * stick direction (PADAxisMapping: an SDL axis and its sign) and saves it with the controller's buttons; the keyboard's
 * are its key-axis bindings. */
PADAxisMapping axis_mapping(u32 axis) {
    u32 n = 0;
    PADAxisMapping* a = PADGetAxisMappings(0, &n);
    for (u32 i = 0; a != nullptr && i < n; ++i) {
        if (a[i].padAxis == axis) return a[i];
    }
    return PADAxisMapping{{-1, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, (PADAxis)axis};
}

/* Dolphin's names: "Left X+", "Right Y-"; Y+ is up (SDL's Y axis points down, so its negative half is Y+) */
std::string native_axis_name(PADSignedNativeAxis a) {
    const bool neg = a.sign == AXIS_SIGN_NEGATIVE;
    switch (a.nativeAxis) {
    case SDL_GAMEPAD_AXIS_LEFTX: return tx("controls.axis.left", {{"axis", neg ? "X-" : "X+"}});
    case SDL_GAMEPAD_AXIS_LEFTY: return tx("controls.axis.left", {{"axis", neg ? "Y+" : "Y-"}});
    case SDL_GAMEPAD_AXIS_RIGHTX: return tx("controls.axis.right", {{"axis", neg ? "X-" : "X+"}});
    case SDL_GAMEPAD_AXIS_RIGHTY: return tx("controls.axis.right", {{"axis", neg ? "Y+" : "Y-"}});
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return tx("controls.axis.left_trigger");
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return tx("controls.axis.right_trigger");
    default: return tx("common.none");
    }
}

/* what drives a stick direction on the device being set up */
std::string dir_text(u32 axis, Kind kind) {
    if (s_kb) return key_name(key_for_axis((PADAxis)axis));
    const PADAxisMapping m = axis_mapping(axis);
    if (m.nativeAxis.nativeAxis >= 0) return native_axis_name(m.nativeAxis);
    if (m.nativeButton != SDL_GAMEPAD_BUTTON_INVALID) return button_name((u32)m.nativeButton, kind);
    return tx("common.none");
}

/* how far a stick direction is pushed (0..1) on the device being set up */
float dir_now(u32 axis) {
    if (s_kb) {
        const s32 sc = key_for_axis((PADAxis)axis);
        const bool* keys = SDL_GetKeyboardState(nullptr);
        return sc >= 0 && keys != nullptr && keys[sc] ? 1.0f : 0.0f;
    }
    SDL_Gamepad* gp = port_gamepad();
    if (gp == nullptr) return 0.0f;
    const PADAxisMapping m = axis_mapping(axis);
    if (m.nativeAxis.nativeAxis >= 0) {
        const float v = SDL_GetGamepadAxis(gp, (SDL_GamepadAxis)m.nativeAxis.nativeAxis) / 32767.0f * (float)m.nativeAxis.sign;
        return SDL_clamp(v, 0.0f, 1.0f);
    }
    if (m.nativeButton != SDL_GAMEPAD_BUTTON_INVALID) return SDL_GetGamepadButton(gp, (SDL_GamepadButton)m.nativeButton) ? 1.0f : 0.0f;
    return 0.0f;
}

/* give a stick direction an axis half; the direction that had it takes this one's old one (a swap, as buttons do).
 * Returns the swapped direction, or -1. */
int assign_axis(u32 axis, PADSignedNativeAxis src) {
    const PADAxisMapping old = axis_mapping(axis);
    int swapped = -1;
    for (u32 other = PAD_AXIS_LEFT_X_POS; other <= PAD_AXIS_RIGHT_Y_NEG; ++other) {
        if (other == axis) continue;
        const PADAxisMapping m = axis_mapping(other);
        if (m.nativeAxis.nativeAxis == src.nativeAxis && m.nativeAxis.sign == src.sign) {
            PADSetAxisMapping(0, PADAxisMapping{old.nativeAxis, old.nativeButton, (PADAxis)other});
            swapped = (int)other;
        }
    }
    PADSetAxisMapping(0, PADAxisMapping{src, SDL_GAMEPAD_BUTTON_INVALID, (PADAxis)axis});
    return swapped;
}

int assign_key_axis(u32 axis, s32 scancode) {
    const s32 old = key_for_axis((PADAxis)axis);
    int swapped = -1;
    u32 n = 0;
    PADKeyAxisBinding* k = PADGetKeyAxisBindings(0, &n);
    for (u32 i = 0; k != nullptr && i < n; ++i) {
        if (k[i].scancode == scancode && k[i].padAxis != axis && k[i].padAxis <= PAD_AXIS_RIGHT_Y_NEG) {
            swapped = k[i].padAxis;
            PADSetKeyAxisBinding(0, PADKeyAxisBinding{old, k[i].padAxis, k[i].influence});
        }
    }
    s16 influence = 0;
    for (u32 i = 0; k != nullptr && i < n; ++i) {
        if (k[i].padAxis == axis) influence = k[i].influence;
    }
    PADSetKeyAxisBinding(0, PADKeyAxisBinding{scancode, (PADAxis)axis, influence});
    return swapped;
}

std::string dir_label(u32 axis) {
    const int i = slot_of_axis(axis);
    return tx(axis >= PAD_AXIS_RIGHT_X_POS ? "controls.dir.c_stick" : "controls.dir.control_stick",
              {{"dir", slot_name(kSlots[i])}});
}

/* what presses a GameCube button on the device being set up */
std::string pad_text(u32 pad, Kind kind) {
    if (s_kb) return key_name(key_for(pad));
    const u32 native = native_for(pad);
    if (native == PAD_NATIVE_BUTTON_INVALID && (pad == PAD_TRIGGER_L || pad == PAD_TRIGGER_R)) {
        return trigger_name(pad == PAD_TRIGGER_L, kind); /* the analog trigger, its click near full travel */
    }
    return button_name(native, kind);
}

std::string stick_text(const Slot& slot) {
    if (!s_kb) return tx(std::string(slot.key) == "STICK" ? "controls.left_stick" : "controls.right_stick");
    auto keys4 = [](PADAxis up, PADAxis left, PADAxis down, PADAxis right) {
        return key_name(key_for_axis(up)) + " " + key_name(key_for_axis(left)) + " " + key_name(key_for_axis(down)) +
               " " + key_name(key_for_axis(right));
    };
    if (std::string(slot.key) == "STICK") {
        return keys4(PAD_AXIS_LEFT_Y_POS, PAD_AXIS_LEFT_X_NEG, PAD_AXIS_LEFT_Y_NEG, PAD_AXIS_LEFT_X_POS);
    }
    return keys4(PAD_AXIS_RIGHT_Y_POS, PAD_AXIS_RIGHT_X_NEG, PAD_AXIS_RIGHT_Y_NEG, PAD_AXIS_RIGHT_X_POS);
}

/* where a trigger's click registers, in % of its travel: 90 % when the click comes from the analog trigger (the
 * read filter's Dolphin-style triggers, pc_pad_shape.cpp); 0 when there is no such point (a button or a key gives
 * the click) */
int trigger_click_pct(bool left) {
    if (s_kb || native_for(left ? PAD_TRIGGER_L : PAD_TRIGGER_R) != PAD_NATIVE_BUTTON_INVALID) return 0;
    return PC_TRIGGER_CLICK_PCT;
}

/* how far a trigger is squeezed (0..1), and whether its click is down, on the device being set up */
void trigger_now(bool left, float& amount, bool& click) {
    const u32 pad = left ? PAD_TRIGGER_L : PAD_TRIGGER_R;
    amount = 0;
    click = false;
    if (s_kb) {
        const s32 k = key_for(pad);
        const bool* keys = SDL_GetKeyboardState(nullptr);
        click = k >= 0 && keys != nullptr && keys[k];
        amount = click ? 1.0f : 0.0f;
        return;
    }
    SDL_Gamepad* gp = port_gamepad();
    if (gp == nullptr) return;
    amount = SDL_GetGamepadAxis(gp, left ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    const u32 native = native_for(pad);
    if (native != PAD_NATIVE_BUTTON_INVALID) {
        click = SDL_GetGamepadButton(gp, (SDL_GamepadButton)native);
        if (click && amount < 0.01f) amount = 1.0f; /* a digital-only button: full when pressed */
    } else {
        const int pct = trigger_click_pct(left);
        click = pct > 0 && amount * 100.0f >= pct;
    }
    amount = SDL_clamp(amount, 0.0f, 1.0f);
}

/* snatcherLeo, 2026-11-13: "when I go to change rumble on gamecube, it does rumble, but it stays at 100". A GameCube
 * controller's motor is either on or off, so Aurora takes no strength for it (PADSetRumbleIntensity returns FALSE when
 * the controller is a GameCube one, verified in pad.cpp); the screen offers a Test instead of the slider */
bool rumble_fixed() { return !s_kb && kind_of(port_gamepad()) == KIND_GAMECUBE; }

int rumble_pct() {
    u16 lo = 0, hi = 0;
    return PADGetRumbleIntensity(0, &lo, &hi) ? (int)SDL_lround(SDL_max(lo, hi) * 100.0 / 32767.0) : 100;
}

/* the printed letters: A, B, X and Y on the buttons that carry those letters (Xbox layout and the Steam Deck) */
bool printed_on() {
    return native_for(PAD_BUTTON_A) == SDL_GAMEPAD_BUTTON_SOUTH && native_for(PAD_BUTTON_B) == SDL_GAMEPAD_BUTTON_EAST &&
           native_for(PAD_BUTTON_X) == SDL_GAMEPAD_BUTTON_WEST && native_for(PAD_BUTTON_Y) == SDL_GAMEPAD_BUTTON_NORTH;
}

bool walk_held_now() {
    if (s_kb) {
        const int k = pc_stick_walk_key();
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        return k >= 0 && keys != nullptr && k < count && keys[k];
    }
    const int b = pc_stick_walk_button();
    SDL_Gamepad* gp = port_gamepad();
    return b >= 0 && gp != nullptr && SDL_GetGamepadButton(gp, (SDL_GamepadButton)b);
}

/* the raw position of a stick on the device being set up (x, y in -1..1, up positive) */
void raw_stick(int stick, float& x, float& y) {
    x = y = 0;
    if (s_kb) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto k = [&](PADAxis a) {
            const s32 sc = key_for_axis(a);
            return sc >= 0 && keys != nullptr && keys[sc] ? 1.0f : 0.0f;
        };
        if (stick == PC_STICK_MAIN) {
            x = k(PAD_AXIS_LEFT_X_POS) - k(PAD_AXIS_LEFT_X_NEG);
            y = k(PAD_AXIS_LEFT_Y_POS) - k(PAD_AXIS_LEFT_Y_NEG);
        } else {
            x = k(PAD_AXIS_RIGHT_X_POS) - k(PAD_AXIS_RIGHT_X_NEG);
            y = k(PAD_AXIS_RIGHT_Y_POS) - k(PAD_AXIS_RIGHT_Y_NEG);
        }
        return;
    }
    /* through the stick directions' mapping, as the game reads it */
    const u32 base = stick == PC_STICK_MAIN ? PAD_AXIS_LEFT_X_POS : PAD_AXIS_RIGHT_X_POS;
    x = dir_now(base + 0) - dir_now(base + 1); /* X_POS, X_NEG */
    y = dir_now(base + 2) - dir_now(base + 3); /* Y_POS, Y_NEG */
}

/* ---- drawing ---- */
/* a key chip: keyboard keys (square) take their colours from the theme (controls.rcss .k.sq); round pad buttons carry
 * their own colour (an inline background would override the theme and leave dark text on a dark chip in light mode) */
std::string kkey(const char* label, const char* bg = nullptr, bool sq = true) {
    std::string style = bg != nullptr ? std::string(" style='background-color: ") + bg + ";'" : std::string();
    return std::string("<span class='k") + (sq ? " sq" : "") + "'" + style + ">" + label + "</span>";
}

std::string badge(const char* key) {
    const Badge& b = kBadges.at(key);
    return std::string("<span class='gc") + (b.sq ? " sq" : "") + "' style='background-color: " + b.bg +
           "; color: " + b.fg + ";'>" + b.text + "</span>";
}

/* a slider's track, its fill and thumb at pct, an optional tick (the default) */
std::string slider(int slot, int pct, int tick = -1) {
    std::string s = "<span class='track' id='track-" + std::to_string(slot) + "'><span class='fill' style='width: " +
                    std::to_string(pct) + "%;'></span>";
    if (tick >= 0) s += "<span class='tick' style='left: " + std::to_string(tick) + "%;'></span>";
    /* the thumb: Settings' smooth circle picture (icons/tog-knob.svg, tinted; 2026-11-23, snatcherLeo: the sliders' round
     * thumbs looked jagged, "wherever else we use sliders") */
    return s + "<span class='thumb' style='left: " + std::to_string(pct) +
           "%;'><img class='tk' src='menuicon://tog-knob'/></span></span>";
}

bool visible(int i);
bool focusable(int i);
int plist_count();
void rumble_test();
std::string free_name(const std::string& base);
std::string name_box_text();

std::string listen_text() {
    return tx("controls.press_countdown", {{"n", std::to_string((int)((s_listen_until - SDL_GetTicks()) / 1000) + 1)}});
}

void refresh() {
    if (s_doc == nullptr) return;
    const bool dark = pc_ui_theme_is_dark() != 0;
    SDL_Gamepad* gp = port_gamepad();
    const Kind kind = kind_of(gp);
    const bool accurate = accurate_in_game();
    if (Rml::Element* body = s_doc->GetElementById("body")) {
        body->SetClass("dark", dark);
        body->SetClass("light", !dark);
        body->SetClass("listing", s_listing);
        body->SetClass("plisting", s_plisting);
        body->SetClass("asking", s_asking || s_dialog != D_NONE);
    }
    if (Rml::Element* lines = s_doc->GetElementById("piece-lines")) { /* the lines' casing follows the page */
        lines->SetAttribute("src", dark ? "controlsart://piece-lines-dark" : "controlsart://piece-lines-light");
    }
    auto cls = [&](int i, const char* base) {
        std::string c = base;
        if (i == s_focus) c += " f";
        if (i == s_listen) c += " listen";
        return c;
    };
    /* the device and profile pickers (the profile: "GameCube Controller" on a GameCube controller, snatcherLeo) */
    if (Rml::Element* e = s_doc->GetElementById("pick")) {
        /* a controller's own name (from the controller) stays as it reports itself */
        const char* gp_name = gp != nullptr ? SDL_GetGamepadName(gp) : nullptr;
        const std::string dev = s_kb ? tx("controls.keyboard") : gp == nullptr ? tx("controls.no_controller")
                                : gp_name != nullptr                          ? std::string(gp_name)
                                                                              : tx("controls.controller");
        e->SetInnerRML("<span class='" + cls(slot_of(S_DEVICE), "dsel") + "' id='dev'>" + short_text(dev, 28) +
                       "<span class='c'>&#9660;</span></span>" + "<span class='" + cls(slot_of(S_PROFILE), "dsel") +
                       "' id='prof'>" + tx("controls.profile", {{"name", short_text(profile_shown(s_profile), 16)}}) +
                       "<span class='c'>" + (s_plisting ? "&#9650;" : "&#9660;") + "</span></span>");
    }
    /* the labels, and the D-pad's group */
    std::string labels, dpad;
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        const Slot& slot = kSlots[i];
        if (slot.kind != S_BUTTON && slot.kind != S_STICK) continue;
        const std::string on = i == s_listen ? listen_text() : slot.kind == S_STICK ? stick_text(slot) : pad_text(slot.pad, kind);
        if (is_dpad(i)) {
            const int dir = (int)(slot.pad == PAD_BUTTON_UP ? 0 : slot.pad == PAD_BUTTON_DOWN ? 1 : slot.pad == PAD_BUTTON_LEFT ? 2 : 3);
            dpad += "<div class='" + cls(i, "dr") + "' id='" + elem_id(i) + "'><span class='ar'>" + kArrows[dir] +
                    "</span>" + slot_name(slot) + "<span class='on'>" + on + "</span></div>";
            continue;
        }
        const std::string key = slot.key;
        const std::string fill = key == "L" || key == "R" ? "<span class='lf'><span class='lfi' id='lfi-" + key + "'></span></span>"
                                                          : std::string();
        labels += "<div class='" + cls(i, "lab") + "' id='" + elem_id(i) + "'>" + badge(slot.key) + slot_name(slot) +
                  "<span class='on'>" + on + "</span>" + fill + "</div>";
    }
    const bool dpad_f = s_focus >= 0 && s_focus < (int)kSlots.size() && is_dpad(s_focus);
    labels += std::string("<div class='grp") + (dpad_f ? " f" : "") + "' id='lab-DPAD'><span class='gh'>" + badge("DPAD") +
              tx("controls.dpad") + "</span>" + dpad + "</div>";
    if (Rml::Element* e = s_doc->GetElementById("labels")) e->SetInnerRML(labels);
    if (Rml::Element* e = s_doc->GetElementById("note")) {
        e->SetInnerRML(s_note);
        e->SetClass("ok", s_note_ok);
    }
    /* the lit line: the selected (or listening) label's */
    {
        const int lit = s_listen >= 0 ? s_listen : s_focus;
        const std::string key = lit >= 0 && lit < (int)kSlots.size() ? kSlots[lit].key : "";
        for (const auto& [k, b] : kBadges) {
            const bool on = k == key;
            if (Rml::Element* l = s_doc->GetElementById("lit-" + k)) l->SetClass("on", on);
            if (Rml::Element* l = s_doc->GetElementById("litdot-" + k)) l->SetClass("on", on);
        }
    }
    /* the side panel: both sticks live (snatcherLeo liked Dolphin's two views; mockups 23 / 24). 127 GameCube units are 50 dp:
     * the gate's corners sit at the stick's range, the dotted outline at how far the physical stick reaches (1.0 of
     * its travel = 127 units), the dead zone, the stick (a ring) and what the game is sent (a dot) */
    /* Laid out as Dolphin's window does it (snatcherLeo, 2026-11-12, mockups 41 / 42): a column per stick, each with its live
     * picture, the Calibrate button, a row per direction saying what drives it (Dolphin's "Left Y+"; a key on the
     * keyboard), then on the Control stick the walk button (Dolphin's Modifier) and its speed, then the dead zone and
     * the range */
    const int st = s_stick;
    std::string side = "<span class='h first'>" + tx("controls.head.sticks") + "</span>";
    auto rowi = [&](int i, const std::string& base, const std::string& inner) {
        if (!visible(i)) return;
        side += "<div class='" + cls(i, base.c_str()) + "' id='" + elem_id(i) + "'>" + inner + "</div>";
    };
    for (int g = 0; g < 2; ++g) {
        const float k = 50.0f / 127.0f;
        const float corner = pc_stick_range(g) * k;
        const float w = corner * 128.0f / 54.0f;
        const float dz = pc_stick_dead(g) * k;
        const std::string n = std::to_string(g);
        const bool calibrating = s_calib && g == st; /* Dolphin clears the picture: only the new trace shows */
        side += std::string("<div class='scol' id='scol-") + n + "'><span class='sct'>" +
                tx(g == 0 ? "controls.stick.control" : "controls.stick.c") + "</span><div class='gbox' id='gate-" + n + "'>" +
                "<img class='gimg' src='controlsart://" + (g == 0 ? "gate-" : "gatec-") + (dark ? "dark" : "light") +
                "' style='left: " + std::to_string(56 - w / 2) + "dp; top: " + std::to_string(56 - w / 2) +
                "dp; width: " + std::to_string(w) + "dp; height: " + std::to_string(w) + "dp;" +
                (calibrating ? " visibility: hidden;" : "") + "'/>";
        float reach[PC_CAL_SAMPLES];
        pc_stick_get_calibration(g, reach);
        for (int b = 0; b < PC_CAL_SAMPLES; ++b) {
            const float a = (float)b / PC_CAL_SAMPLES * 2 * 3.14159265f;
            const float r = calibrating ? 0.0f : reach[b] * 127 * k;
            side += std::string("<div class='") + (calibrating ? "cd" : "rd") + "' id='" +
                    (calibrating ? std::string("cd-") : "rd-" + n + "-") + std::to_string(b) + "' style='left: " + std::to_string(56 + std::cos(a) * r - 1.5f) + "dp; top: " +
                    std::to_string(56 - std::sin(a) * r - 1.5f) + "dp;" + (calibrating ? " visibility: hidden;" : "") + "'></div>";
        }
        side += "<div class='dzc' style='left: " + std::to_string(56 - dz) + "dp; top: " + std::to_string(56 - dz) +
                "dp; width: " + std::to_string(2 * dz) + "dp; height: " + std::to_string(2 * dz) + "dp; border-radius: " +
                std::to_string(dz) + "dp;'></div><div class='rawc' id='raw-" + n + "'></div><div class='sentc' id='sent-" +
                n + "'></div></div><span class='gnum' id='num-" + n + "'>&#160;</span>";
        /* the same rows for every device, so nothing moves when the device changes (snatcherLeo, 2026-11-13: "Layout shouldn't
         * change this much"); on the keyboard the stick-only rows are shown greyed ("off": not selectable) */
        const char* off = s_kb ? " off" : "";
        {
            /* calibration as Dolphin does it (snatcherLeo, 2026-11-12, after showing it: "you click calibrate, we let the user
             * move their stick, then we click "Finish calibration" and that's it"): one button. Calibrate clears the
             * stick's picture; while the stick is rolled round its edge the button reads Cancel Calibration, and once
             * every direction has been reached, Finish Calibration */
            const std::string label = tx(!calibrating     ? "controls.slot.calibrate"
                                         : cal_complete() ? "controls.calibrate.finish"
                                                          : "controls.calibrate.cancel");
            rowi(slot_of_stick(S_CALIB, g), std::string(calibrating && cal_complete() ? "btn cb pri" : "btn cb") + off,
                 label + (!s_kb && !calibrating && pc_stick_has_calibration(g) ? " &#160;&#10003;" : ""));
        }
        /* the directions: select one (it stays lit, waiting), push the stick that way, and the row shows the axis it
         * caught ("Left Y+") */
        const u32 base = g == 0 ? PAD_AXIS_LEFT_X_POS : PAD_AXIS_RIGHT_X_POS;
        for (const u32 axis : {base + 2, base + 3, base + 1, base + 0}) { /* Up, Down, Left, Right */
            const int i = slot_of_axis(axis);
            const std::string what = i == s_listen ? tx(s_kb ? "controls.listen.key" : "controls.listen.stick") : dir_text(axis, kind);
            rowi(i, "drow", std::string("<span class='dl'>") + slot_name(kSlots[i]) + "</span><span class='dchip' id='dc-" +
                                std::to_string(axis) + "'>" + what + "</span>");
        }
        if (g == 0) { /* walking: Dolphin's Modifier row */
            const int wb = s_kb ? pc_stick_walk_key() : pc_stick_walk_button();
            const std::string wname = s_listen == slot_of(S_WALK) ? listen_text()
                                      : wb < 0 ? tx("common.none") : s_kb ? key_name(wb) : button_name((u32)wb, kind);
            rowi(slot_of(S_WALK), "drow", "<span class='dl'>" + tx("controls.slot.walk") + "</span><span class='dchip'>" +
                                              wname + "</span>");
            const int wp = pc_stick_walk_pct();
            rowi(slot_of(S_WALKPCT), "crow", "<span class='cl'>" + tx("controls.speed") + "</span><span class='v'>" +
                                                 std::to_string(wp) + "%</span>" +
                                                 slider(slot_of(S_WALKPCT), (wp - 10) * 100 / 80));
        } else {
            side += "<div class='cgap'></div>";
        }
        {
            const int ri = slot_of_stick(S_RANGE, g), di = slot_of_stick(S_DEAD, g);
            const int dead = pc_stick_dead(g), lim = pc_stick_dead_limit();
            side += "<div class='csep'></div>";
            rowi(di, std::string("crow") + off,
                 "<span class='cl'>" + tx("controls.dead") + "</span><span class='v'>" + std::to_string(dead) + "</span>" +
                     slider(di, dead * 100 / lim));
            const int range = pc_stick_range(g), def = pc_stick_range_default(g);
            const std::string range_label = "<span class='cl'>" + tx("controls.slot.range") + "</span>";
            if (accurate) {
                rowi(ri, std::string("crow") + off,
                     range_label + "<span class='v'>" + std::to_string(range) + "</span><span class='lock'>" +
                         tx("controls.locked") + "</span>");
            } else {
                rowi(ri, std::string("crow") + off, range_label + "<span class='v'>" + std::to_string(range) + "</span>" +
                                                       slider(ri, (range - 60) * 100 / 67, (def - 60) * 100 / 67));
            }
        }
        side += "</div>";
    }
    if (accurate) {
        /* wording and width from snatcherLeo's hand check (2026-11-13: "say "In Gamecube-Accurate Mode"", "we can lengthen it
         * horizontally"): the note spans both stick columns, edge to edge (controls.rcss .acc) */
        side += "<span class='acc'>" + tx("controls.accurate_note") + "</span>";
    }
    auto row = [&](SlotKind k, const std::string& base, const std::string& inner) {
        const int i = slot_of(k);
        if (!visible(i)) return;
        side += "<div class='" + cls(i, base.c_str()) + "' id='" + elem_id(i) + "'>" + inner + "</div>";
    };
    {
        row(S_SNAP, s_kb ? "srow off" : "srow", tx("controls.slot.snap") + "<span class='tog" +
                                                    (pc_stick_snap() ? " on" : "") + "'><span class='knob'></span></span>");
        /* the triggers, live: how far each is squeezed, where its click registers (Aurora's activation zone when the
         * click comes from the analog trigger), and the click */
        side += "<span class='h'>" + tx("controls.head.triggers") + "</span>";
        for (const char* t : {"L", "R"}) {
            const int click = trigger_click_pct(t[0] == 'L');
            side += std::string("<div class='trig'>") + t + "<span class='tbar'><span class='tfill' id='tf-" + t +
                    "'></span>" + (click > 0 ? "<span class='tclick' style='left: " + std::to_string(click) + "%;'></span>" : "") +
                    "</span><span class='tpc' id='tp-" + t + "'>0%</span><span class='tdig' id='td-" + t + "'></span></div>";
        }
        {
            side += "<span class='h'>" + tx("controls.head.controller") + "</span>";
            const int rp = rumble_pct();
            const std::string rumble = tx("controls.slot.rumble");
            if (s_kb) { /* kept in place, greyed: the keyboard has no motor */
                row(S_RUMBLE, "srow two off", rumble + "<span class='sm'>" + tx("controls.rumble.keyboard") + "</span>");
            } else if (rumble_fixed()) { /* a GameCube controller: its motor is on or off (Aurora refuses a strength for it) */
                row(S_RUMBLE, "srow two", rumble + "<span class='sm'>" + tx("controls.rumble.gamecube") + "</span>" +
                                              "<span class='chip'>" + tx("controls.rumble.test") + "</span>");
            } else {
                row(S_RUMBLE, "srow", rumble + "<span class='v'>" + std::to_string(rp) + "</span>" + slider(slot_of(S_RUMBLE), rp));
            }
            /* (the Printed letters switch was removed, snatcherLeo, 2026-11-13: "we don't need that slider"; A / B / X / Y are
             * changed one by one like any button) */
        }
        side += "<span class='h'>" + tx("controls.head.shortcuts") + "</span>";
        std::string sc;
        const bool combo_listen = s_listen == slot_of(S_MENU) || s_listen == slot_of(S_QUITSC);
        if (combo_listen) { /* what is held so far, or the countdown */
            for (size_t m = 0; m < s_combo.size(); ++m) {
                const PcShortcutMember& c = s_combo[m];
                sc += (m > 0 ? " + " : "") + (c.type == PC_SC_KEY ? key_name(c.value)
                                              : c.type == PC_SC_GC ? gc_name((u32)c.value)
                                                                   : button_name((u32)c.value, kind));
            }
            if (sc.empty()) {
                sc = tx("controls.hold_countdown", {{"n", std::to_string((int)((s_listen_until - SDL_GetTicks()) / 1000) + 1)}});
            }
        }
        row(S_MENU, "srow", tx("controls.slot.menu") + "<span class='chip'>" +
                                (s_listen == slot_of(S_MENU) ? sc : shortcut_shown(pc_shortcut_name(s_kb ? 1 : 0))) + "</span>");
        /* Fast-forward (2026-11-16, mockup s4): this device's one button or key; "None" when it has none (a GameCube
         * controller by default, or a default button that something else already uses) */
        {
            const std::string ffname = pc_ff_name(s_kb ? 1 : 0);
            row(S_FF, "srow", tx("controls.slot.ff") + "<span class='chip'>" +
                                  (s_listen == slot_of(S_FF) ? listen_text() : ffname.empty() ? tx("common.none") : ffname) +
                                  "</span>");
        }
        /* Quit to hub (snatcherLeo, 2026-11-13: a controller shortcut, "by default, it will never be set ... users have to make
         * a concious decision"); greyed on the keyboard, which has none */
        row(S_QUITSC, s_kb ? "srow off" : "srow",
            tx("settings.quit") + "<span class='chip'>" +
                (s_listen == slot_of(S_QUITSC) ? sc : shortcut_shown(pc_quit_shortcut_name())) + "</span>");
        if (combo_listen) {
            side += "<span class='warn'>" + tx("controls.shortcut_how") + "</span>";
        }
        if (s_listen == slot_of(S_FF)) { /* what the button will do (mockup s4, "Choosing it") */
            side += "<span class='warn'>" + tx("controls.ff_how") + "</span>";
        }
    }
    if (Rml::Element* e = s_doc->GetElementById("side")) e->SetInnerRML(side);
    /* the side panel follows the controller (2026-11-16): the focused or listening row is kept in view when the panel
     * is taller than the window (it scrolls at 1600 x 960 since the Fast-forward row; before, a row below the fold
     * could only be reached by the mouse wheel). Only the panel's own rows ("slot-N"); the labels on the picture never
     * scroll. All platforms (RmlUi) */
    /* done two frames later (pc_controls_ui_frame), once the rows just written have been laid out: right away it
     * measured the old rows and did nothing */
    s_scroll_frames = 2;
    /* the bottom bar: the buttons that work here, then Reset and Save */
    if (Rml::Element* e = s_doc->GetElementById("bot")) {
        std::string h;
        if (s_listen >= 0) {
            h = "<span class='hi'>" + tx(s_listen == slot_of(S_MENU) || s_listen == slot_of(S_QUITSC) ? "controls.bot.hold"
                                         : s_kb                                                         ? "controls.bot.press_key"
                                                                                                        : "controls.bot.press_button") +
                "</span>";
        } else if (s_calib) {
            h = "<span class='hi'>" + tx("controls.bot.roll") + "</span><span class='hi'>" + kkey("A", "#3C9A3C", false) +
                tx(cal_complete() ? "controls.bot.finish" : "common.cancel") + "</span><span class='hi'>" +
                kkey("B", "#C8382E", false) + tx("common.cancel") + "</span>";
        } else if (s_kb) {
            h = "<span class='hi'>" + kkey(pc_text("settings.key.backspace")) + tx("common.back") + "</span><span class='hi'>" +
                kkey(pc_text("settings.key.enter")) + tx("controls.bot.change") + "</span><span class='hi'>" +
                kkey(pc_text("settings.key.delete")) + tx("settings.hint.default") + "</span><span class='hi'>" +
                tx("controls.bot.any_key") + "</span>";
        } else {
            const bool gc = kind == KIND_GAMECUBE;
            /* menus use the controller's own buttons, whatever the game's layout (snatcherLeo asked which "B" gets him out:
             * the one printed on his pad); said so here, where the GameCube letters are everywhere */
            h = "<span class='hi'>" + tx("controls.bot.physical") + "</span><span class='hi'>" + kkey("B", "#C8382E", false) +
                tx("common.back") + "</span><span class='hi'>" + kkey("A", "#3C9A3C", false) + tx("controls.bot.change") +
                "</span><span class='hi'>" + kkey("X", gc ? "#7B8580" : "#2F6BD2", false) + tx("settings.hint.default") +
                "</span><span class='hi'>" + tx("controls.bot.any_button") + "</span>";
        }
        h = "<span class='hl'>" + h + "</span>"; /* the hints stop before the buttons (controls.rcss #bot .hl) */
        const int r = slot_of(S_RESET), sv = slot_of(S_SAVE);
        h += "<span class='rt'><span class='" + cls(r, "btn") + "' id='" + elem_id(r) + "'>" +
             tx(s_kb ? "controls.reset.keyboard" : "controls.reset.controller") + "</span><span class='" + cls(sv, "btn pri") +
             "' id='" + elem_id(sv) + "'>" +
             (s_dirty ? tx("controls.save_to", {{"name", short_text(profile_shown(s_profile), 10)}}) : tx("controls.saved")) +
             "</span></span>";
        e->SetInnerRML(h);
    }
    /* the device list */
    if (Rml::Element* e = s_doc->GetElementById("list")) {
        std::string l;
        if (s_listing) {
            /* every connected controller, then the keyboard (snatcherLeo, 2026-11-13: with an Xbox controller plugged in "the
             * gmaecube controller option is gone. Ideally, it should remain there") */
            std::vector<std::string> opts;
            const u32 n = PADCount();
            for (u32 j = 0; j < n; ++j) {
                const char* name = PADGetNameForControllerIndex(j);
                const bool p1 = (s32)j == PADGetIndexForPort(0);
                opts.push_back(short_text(name != nullptr ? std::string(name) : tx("controls.controller"), p1 ? 20 : 30) +
                               (p1 ? " &#183; " + tx("controls.player1") : std::string()));
            }
            if (n == 0) opts.push_back(tx("controls.none_connected"));
            opts.push_back(tx("controls.keyboard"));
            for (int j = 0; j < (int)opts.size(); ++j) {
                l += std::string("<div class='opt") + (j == s_list_focus ? " f" : "") + "' id='opt-" + std::to_string(j) +
                     "'>" + opts[j] + "</div>";
            }
        }
        e->SetInnerRML(l);
    }
    /* the profile list (mockup 43): the profiles, then New, Rename and Delete */
    if (Rml::Element* e = s_doc->GetElementById("plist")) {
        std::string l;
        if (s_plisting) {
            l = "<span class='lh'>" + tx(s_kb ? "controls.profiles.keyboard" : "controls.profiles.controller") + "</span>";
            const int n = (int)s_profiles.size();
            for (int j = 0; j < plist_count(); ++j) {
                if (j == n) l += "<div class='psep'></div>";
                std::string text, tag;
                if (j < n) {
                    text = std::string("<span class='ck'>") + (s_profiles[j] == s_profile ? "&#10003;" : "") + "</span>" +
                           short_text(profile_shown(s_profiles[j]), j == 0 ? 14 : 22);
                    if (j == 0) tag = "<span class='tag'>" + tx("controls.profiles.built_in") + "</span>";
                } else if (j == n) {
                    text = "+ " + tx("controls.profiles.new");
                } else if (j == n + 1) {
                    text = tx("controls.profiles.rename", {{"name", short_text(profile_shown(s_profile), 14)}});
                } else {
                    text = tx("controls.profiles.delete", {{"name", short_text(profile_shown(s_profile), 14)}});
                }
                l += std::string("<div class='opt") + (j >= n ? " act" : "") + (j == s_plist_focus ? " f" : "") +
                     "' id='popt-" + std::to_string(j) + "'>" + text + tag + "</div>";
            }
        }
        e->SetInnerRML(l);
    }
    /* the save question, and the profiles' questions; rebuilt only when it changes, so a name being typed survives */
    if (Rml::Element* e = s_doc->GetElementById("prompt")) {
        std::string p, key;
        /* the buttons' names are text keys */
        auto buttons = [&](std::initializer_list<const char*> names, int focus, int primary) {
            int j = 0;
            p += "<div class='pbtns n" + std::to_string(names.size()) + "'>";
            for (const char* name : names) {
                p += std::string("<span class='btn") + (j == primary ? " pri" : "") + (j == focus ? " f" : "") +
                     "' id='ask-" + std::to_string(j) + "'>" + tx(name) + "</span>";
                ++j;
            }
            p += "</div>";
        };
        if (s_asking) {
            p = "<span class='q'>" + tx("controls.ask.save") + "</span><span class='t'>" +
                tx(s_kb ? "controls.ask.save.keyboard" : "controls.ask.save.controller",
                   {{"name", "<b>" + profile_shown(s_profile) + "</b>"}}) +
                "</span>";
            buttons({"common.save", "controls.discard"}, s_prompt_focus, 0);
        } else if (s_dialog == D_FF) {
            /* the fast-forward button picked is a game button (snatcherLeo's rule, 2026-11-16: ask, never swap silently) */
            p = "<span class='q'>" + tx("controls.ask.ff", {{"button", bold(pc_ff_member_name(&s_ff_pending))},
                                                           {"what", bold(gc_name(s_ff_pending_gc))}}) +
                "</span><span class='t'>" + tx("controls.ask.ff.then") + "</span>";
            buttons({"controls.ff.use_anyway", "controls.ff.press_another"}, s_dialog_focus, 1);
        } else if (s_dialog == D_SWITCH) {
            p = "<span class='q'>" + tx("controls.ask.switch", {{"name", profile_shown(s_profile)}}) + "</span><span class='t'>" +
                tx("controls.ask.switch.then", {{"name", profile_shown(s_pending)}}) + "</span>";
            buttons({"common.save", "common.dont_save", "common.cancel"}, s_dialog_focus, 0);
        } else if (s_dialog == D_NAME_NEW || s_dialog == D_NAME_RENAME) {
            const bool fresh = s_doc->GetElementById("pname") == nullptr;
            std::string value = fresh ? (s_dialog == D_NAME_NEW ? free_name(is_builtin(s_profile) ? "Profile" : s_profile)
                                                                 : s_profile)
                                      : clean_name(name_box_text());
            std::string quoted;
            for (char c : value) quoted += c == '\'' ? std::string("&#39;") : std::string(1, c);
            value = quoted;
            /* compared without the typed text: typing never rebuilds the box */
            key = "name|" + std::to_string((int)s_dialog) + "|" + s_name_error + "|" + std::to_string(s_dialog_focus);
            p = "<span class='q'>" + tx(s_dialog == D_NAME_NEW ? "controls.name.new" : "controls.name.rename") +
                "</span><span class='t'>" + tx(s_dialog == D_NAME_NEW ? "controls.name.new.how" : "controls.name.rename.how") +
                "</span><input type='text' id='pname' maxlength='24' value='" + value + "'/>" +
                (s_name_error.empty() ? "" : "<span class='perr'>" + s_name_error + "</span>");
            buttons({"common.cancel", s_dialog == D_NAME_NEW ? "controls.create" : "label.rename"}, s_dialog_focus, 1);
        } else if (s_dialog == D_DELETE) {
            p = "<span class='q'>" + tx("controls.ask.delete", {{"name", profile_shown(s_profile)}}) +
                "</span><span class='t'>" +
                tx(s_kb ? "controls.ask.delete.keyboard" : "controls.ask.delete.controller",
                   {{"name", profile_shown(builtin_profile())}}) +
                "</span>";
            buttons({"common.cancel", "controls.delete"}, s_dialog_focus, 1);
        }
        if (key.empty()) key = p;
        static std::string s_last_prompt;
        if (key != s_last_prompt || (e->GetNumChildren() == 0 && !p.empty())) {
            s_last_prompt = key;
            e->SetInnerRML(p);
            if (s_doc->GetElementById("pname") != nullptr) s_name_focus_frames = 2; /* the new box takes the keyboard */
        }
    }
}

/* ---- which slots show, and which can be focused ---- */
bool visible(int i) {
    const SlotKind k = kSlots[i].kind;
    const bool accurate = accurate_in_game();
    switch (k) {
    case S_PRINTED: return false; /* removed from the screen (snatcherLeo, 2026-11-13) */
    default: (void)accurate; return true;
    }
}
bool focusable(int i) {
    if (!visible(i)) return false;
    switch (kSlots[i].kind) { /* on the keyboard these stay in view, greyed, so the layout never moves */
    case S_CALIB:
    case S_RANGE:
    case S_DEAD:
    case S_SNAP:
    case S_RUMBLE:
    case S_QUITSC: /* a controller's shortcut only */
        if (s_kb) return false;
        break;
    default: break;
    }
    if (kSlots[i].kind == S_RANGE && accurate_in_game()) return false; /* locked */
    if (s_calib) return kSlots[i].kind == S_CALIB && stick_of(i) == s_stick; /* the rest stays in view (Dolphin) */
    return true;
}

/* ---- actions ---- */
void note(const std::string& text, bool ok) {
    s_note = text;
    s_note_ok = ok;
}

void close_screen() {
    s_open = false;
    s_asking = s_listing = s_calib = s_plisting = false;
    s_dialog = D_NONE;
    s_listen = -1;
    s_drag = -1;
    if (s_doc != nullptr) s_doc->Hide();
    input::reconcile();
    std::printf("[controls_ui] closed\n");
}

/* the live setup (Aurora's controller files, keyboard_bindings.dat, settings.ini) and the profile on screen remember
 * the same thing: saving writes both, and the device remembers which profile it uses */
void commit_live() {
    PADSerializeMappings();
    pc_settings_set_string(remember_key().c_str(), s_profile.c_str());
    pc_settings_set_string(s_kb ? "profile_active_keyboard" : "profile_active_pad", (remember_key() + "=" + s_profile).c_str());
    pc_settings_save();
}

void save() {
    write_profile(s_profile, take());
    commit_live();
    s_before = take();
    s_dirty = false;
    note("&#10003; " + tx("controls.note.saved_to", {{"name", bold(profile_shown(s_profile))}}), true);
    std::printf("[controls_ui] saved to profile \"%s\" (and Aurora's controller files and keyboard_bindings.dat, "
                "settings.ini)\n", s_profile.c_str());
}

/* load a profile: its file onto what is set now, or the factory defaults for a built-in one with no file yet */
void load_profile(const std::string& name) {
    Snapshot s = take();
    const bool from_file = read_profile(name, s);
    if (!from_file) {
        s = s_defaults;
        if (!s_kb) {
            pc_stick_clear_calibration(PC_STICK_MAIN);
            pc_stick_clear_calibration(PC_STICK_C);
        }
    }
    put(s);
    s_profile = name;
    commit_live();
    s_before = take();
    s_dirty = false;
    note("&#10003; " + tx("controls.note.using", {{"name", bold(profile_shown(name))}}), true);
    std::printf("[controls_ui] profile \"%s\" loaded (%s)\n", name.c_str(), from_file ? "its file" : "the defaults");
}

/* the profile list's entries: the profiles, then New, Rename and Delete (Rename and Delete not for the built-in one) */
int plist_count() { return (int)s_profiles.size() + (is_builtin(s_profile) ? 1 : 3); }

void open_name_dialog(Dialog mode) {
    s_dialog = mode;
    s_dialog_focus = 1;
    s_name_error.clear();
    s_name_focus_frames = 2;
}

/* a free name: "<base>", else "<base> 2", "<base> 3"... */
std::string free_name(const std::string& base) {
    auto taken = [&](const std::string& n) {
        for (const std::string& p : s_profiles) {
            if (SDL_strcasecmp(p.c_str(), n.c_str()) == 0) return true;
        }
        return false;
    };
    if (!taken(base)) return base;
    for (int i = 2; i < 100; ++i) {
        const std::string n = clean_name(base.substr(0, 20) + " " + std::to_string(i));
        if (!taken(n)) return n;
    }
    return base;
}

void plist_choose() {
    const int n = (int)s_profiles.size();
    const int i = s_plist_focus;
    s_plisting = false;
    if (i < n) {
        if (s_profiles[i] == s_profile) {
            refresh();
            return;
        }
        if (s_dirty) {
            s_pending = s_profiles[i];
            s_dialog = D_SWITCH;
            s_dialog_focus = 0;
        } else {
            load_profile(s_profiles[i]);
        }
    } else if (i == n) {
        open_name_dialog(D_NAME_NEW);
    } else if (i == n + 1) {
        open_name_dialog(D_NAME_RENAME);
    } else {
        s_dialog = D_DELETE;
        s_dialog_focus = 0;
    }
    refresh();
}

std::string name_box_text() {
    if (s_doc != nullptr) {
        if (Rml::Element* e = s_doc->GetElementById("pname")) {
            return e->GetAttribute<Rml::String>("value", "");
        }
    }
    return "";
}

/* the name box's Create / Rename */
void name_confirm() {
    const std::string name = clean_name(name_box_text());
    if (name.empty()) {
        s_name_error = tx("controls.name.empty");
        refresh();
        return;
    }
    for (const std::string& p : s_profiles) {
        if (SDL_strcasecmp(p.c_str(), name.c_str()) == 0 && !(s_dialog == D_NAME_RENAME && p == s_profile)) {
            s_name_error = tx("controls.name.taken", {{"name", profile_shown(p)}});
            refresh();
            return;
        }
    }
    if (s_dialog == D_NAME_NEW) {
        s_profile = name; /* a copy of what is on screen now, unsaved changes included */
        save();
        note("&#10003; " + tx("controls.note.new", {{"name", bold(name)}}), true);
    } else {
        const std::string from = profiles_dir(s_kb) + s_profile + ".txt", to = profiles_dir(s_kb) + name + ".txt";
        if (SDL_GetPathInfo(from.c_str(), nullptr)) {
            SDL_RenamePath(from.c_str(), to.c_str());
        } else {
            write_profile(name, s_before);
        }
        std::printf("[controls_ui] profile \"%s\" renamed to \"%s\"\n", s_profile.c_str(), name.c_str());
        s_profile = name;
        pc_settings_set_string(remember_key().c_str(), s_profile.c_str());
        pc_settings_save();
        note("&#10003; " + tx("label.note.renamed", {{"name", bold(name)}}), true);
    }
    s_dialog = D_NONE;
    list_profiles();
    refresh();
}

/* Delete: the file moves to the profiles' Deleted folder (it can be brought back), and the device goes back to the
 * built-in profile */
void delete_profile() {
    const std::string from = profiles_dir(s_kb) + s_profile + ".txt";
    const std::string dir = deleted_dir(s_kb);
    SDL_CreateDirectory(dir.c_str());
    std::string to = dir + s_profile + ".txt";
    for (int i = 2; SDL_GetPathInfo(to.c_str(), nullptr) && i < 1000; ++i) {
        to = dir + s_profile + " (" + std::to_string(i) + ").txt";
    }
    const bool moved = !SDL_GetPathInfo(from.c_str(), nullptr) || SDL_RenamePath(from.c_str(), to.c_str());
    std::printf("[controls_ui] profile \"%s\" moved to %s%s\n", s_profile.c_str(), to.c_str(), moved ? "" : ": FAILED");
    const std::string gone = s_profile;
    s_dialog = D_NONE;
    list_profiles();
    load_profile(builtin_profile());
    note("&#10003; " + tx("controls.note.deleted", {{"name", bold(gone)}}), true);
    refresh();
}

void start_listen(int i);
void ff_commit(const PcShortcutMember& m, unsigned gc);

void dialog_choose() {
    switch (s_dialog) {
    case D_FF: /* Use it anyway / Press another */
        s_dialog = D_NONE;
        if (s_dialog_focus == 0) {
            ff_commit(s_ff_pending, s_ff_pending_gc);
        } else {
            start_listen(slot_of(S_FF));
        }
        return;
    case D_SWITCH: /* Save / Don't save / Cancel */
        s_dialog = D_NONE;
        if (s_dialog_focus == 0) {
            save();
            load_profile(s_pending);
        } else if (s_dialog_focus == 1) {
            put(s_before);
            load_profile(s_pending);
        }
        break;
    case D_NAME_NEW:
    case D_NAME_RENAME: /* Cancel / Create */
        if (s_dialog_focus == 1) {
            name_confirm();
            return;
        }
        s_dialog = D_NONE;
        break;
    case D_DELETE: /* Cancel / Delete */
        if (s_dialog_focus == 1) {
            delete_profile();
            return;
        }
        s_dialog = D_NONE;
        break;
    default: break;
    }
    refresh();
}

int dialog_buttons() { return s_dialog == D_SWITCH ? 3 : 2; }

/* the device's remembered profile; loaded if it isn't what the live setup holds (another controller model was used
 * since, or the profile was chosen for this one before): "each controller remembers its profile" */
void pick_device_profile() {
    list_profiles();
    std::string want = pc_settings_string(remember_key().c_str(), builtin_profile().c_str());
    if (std::find(s_profiles.begin(), s_profiles.end(), want) == s_profiles.end()) want = builtin_profile();
    s_profile = want;
    const std::string marker = remember_key() + "=" + want;
    const std::string active = pc_settings_string(s_kb ? "profile_active_keyboard" : "profile_active_pad", "");
    if (!active.empty() && active != marker && SDL_GetPathInfo((profiles_dir(s_kb) + want + ".txt").c_str(), nullptr)) {
        load_profile(want);
    }
}

void back() {
    if (s_dialog != D_NONE) {
        s_dialog = D_NONE;
    } else if (s_plisting) {
        s_plisting = false;
    } else if (s_listen >= 0) {
        s_listen = -1;
        note(tx("controls.note.no_change"), false);
    } else if (s_calib) {
        s_calib = false;
        note(tx("controls.note.calib_cancelled"), false);
    } else if (s_listing) {
        s_listing = false;
    } else if (s_asking) {
        s_asking = false;
    } else if (s_dirty) {
        s_asking = true;
        s_prompt_focus = 0;
    } else {
        close_screen();
        return;
    }
    refresh();
}

void changed(const std::string& text, bool ok = true) {
    s_dirty = true;
    note(text, ok);
    refresh();
}

void start_listen(int i) {
    s_listen = i;
    s_listen_armed = false;
    s_listen_event = -1;
    s_combo.clear();
    s_listen_until = SDL_GetTicks() + 5000;
    note("", false);
    refresh();
}

void start_calib() {
    s_calib = true;
    for (float& b : s_cal_bins) b = 0;
    s_cal_was_complete = false;
    s_cal_last_bin = -1;
    s_focus = slot_of_stick(S_CALIB, s_stick);
    note("", false);
    refresh();
}

/* the button while calibrating: Finish once every direction is reached, otherwise Cancel (as in Dolphin) */
void finish_calib() {
    if (!cal_complete()) {
        s_calib = false;
        int reached = 0;
        std::string missing;
        for (int b = 0; b < PC_CAL_SAMPLES; ++b) {
            if (s_cal_bins[b] > 0) ++reached;
            else missing += " " + std::to_string(b);
        }
        std::printf("[controls_ui] calibration cancelled: %d of %d directions reached (missing:%s; direction 0 is "
                    "right, counting anticlockwise)\n", reached, PC_CAL_SAMPLES, missing.c_str());
        note(tx("controls.note.calib_cancelled"), false);
        refresh();
        return;
    }
    float bins[PC_CAL_SAMPLES];
    for (int i = 0; i < PC_CAL_SAMPLES; ++i) bins[i] = s_cal_bins[i];
    const int filled = PC_CAL_SAMPLES;
    float x = 0, y = 0;
    raw_stick(s_stick, x, y);
    const bool centred = std::sqrt(x * x + y * y) < 0.25f; /* let go: its rest position is the centre */
    pc_stick_set_calibration(s_stick, bins, centred ? x : 0.0f, centred ? y : 0.0f);
    s_calib = false;
    std::printf("[controls_ui] %s calibrated (%d of %d directions reached; centre %.3f %.3f)\n",
                s_stick == PC_STICK_MAIN ? "Control stick" : "C-Stick", filled, PC_CAL_SAMPLES, centred ? x : 0.0f,
                centred ? y : 0.0f);
    changed(tx(s_stick == PC_STICK_MAIN ? "controls.note.calibrated.control" : "controls.note.calibrated.c"));
}

void choose() {
    if (s_asking) {
        if (s_prompt_focus == 0) {
            save();
        } else {
            put(s_before);
            s_dirty = false;
            std::printf("[controls_ui] changes discarded\n");
        }
        close_screen();
        return;
    }
    if (s_dialog != D_NONE) {
        dialog_choose();
        return;
    }
    if (s_plisting) {
        plist_choose();
        return;
    }
    if (s_listing) {
        /* the entries: each connected controller (choosing one makes it Player 1's; Aurora remembers it), then the
         * keyboard */
        const int n = (int)PADCount();
        const int kb_entry = SDL_max(n, 1);
        if (s_list_focus < n && s_list_focus != PADGetIndexForPort(0) && s_dirty) {
            note(tx("controls.note.save_first"), false); /* each pad has its own setup */
            s_listing = false;
            refresh();
            return;
        }
        if (s_list_focus < n) {
            PADSetPortForIndex((u32)s_list_focus, 0);
            std::printf("[controls_ui] Player 1 now uses controller %d (%s)\n", s_list_focus,
                        PADGetNameForControllerIndex((u32)s_list_focus));
            s_defaults = read_defaults(); /* this controller's own factory buttons */
            s_before = take();
        }
        s_kb = s_list_focus >= kb_entry || n == 0;
        s_listing = false;
        note("", false);
        pick_device_profile(); /* the other device's own profile */
        refresh();
        return;
    }
    if (s_calib) {
        finish_calib();
        return;
    }
    if (s_focus < 0) { /* nothing selected (a click on empty space): Enter / A selects A first */
        s_focus = slot_of_key("A");
        refresh();
        return;
    }
    const Slot& slot = kSlots[s_focus];
    switch (slot.kind) {
    case S_BUTTON:
    case S_MENU:
    case S_FF:
    case S_QUITSC:
    case S_DIR:
    case S_WALK: start_listen(s_focus); return;
    case S_STICK: /* the stick's label: to its column's Up row */
        s_stick = std::string(slot.key) == "C" ? PC_STICK_C : PC_STICK_MAIN;
        s_focus = slot_of_axis(s_stick == PC_STICK_C ? PAD_AXIS_RIGHT_Y_POS : PAD_AXIS_LEFT_Y_POS);
        refresh();
        return;
    case S_CALIB:
        s_stick = stick_of(s_focus);
        start_calib();
        return;
    case S_RUMBLE: /* a GameCube controller's Test (the others' slider is set by dragging or left / right) */
        if (rumble_fixed()) {
            rumble_test();
            note(tx("controls.note.rumble_test"), false);
            refresh();
        }
        return;
    case S_SNAP:
        pc_stick_set_snap(!pc_stick_snap());
        changed(tx(pc_stick_snap() ? "controls.note.snap_on" : "controls.note.snap_off"));
        return;
    case S_PRINTED:
        if (printed_on()) {
            for (u32 pad : {PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y}) {
                for (u32 i = 0; i < s_defaults.button_count; ++i) {
                    if (s_defaults.buttons[i].padButton == pad) assign_native(pad, s_defaults.buttons[i].nativeButton);
                }
            }
            changed(tx("controls.note.printed_off"));
        } else {
            assign_native(PAD_BUTTON_A, SDL_GAMEPAD_BUTTON_SOUTH);
            assign_native(PAD_BUTTON_B, SDL_GAMEPAD_BUTTON_EAST);
            assign_native(PAD_BUTTON_X, SDL_GAMEPAD_BUTTON_WEST);
            assign_native(PAD_BUTTON_Y, SDL_GAMEPAD_BUTTON_NORTH);
            changed(tx("controls.note.printed_on"));
        }
        return;
    case S_RESET:
        put(s_defaults);
        if (!s_kb) {
            pc_stick_clear_calibration(PC_STICK_MAIN);
            pc_stick_clear_calibration(PC_STICK_C);
        }
        changed(tx(s_kb ? "controls.note.reset.keyboard" : "controls.note.reset.controller"), false);
        return;
    case S_SAVE:
        save();
        refresh();
        return;
    case S_DEVICE:
        s_listing = !s_listing; /* a second click closes it (snatcherLeo: "click again to get the dropdown to go away") */
        s_plisting = false;
        s_list_focus = s_kb ? SDL_max((int)PADCount(), 1) : SDL_max(PADGetIndexForPort(0), 0);
        refresh();
        return;
    case S_PROFILE:
        list_profiles();
        s_plisting = !s_plisting;
        s_listing = false;
        s_plist_focus = 0;
        for (int j = 0; j < (int)s_profiles.size(); ++j) {
            if (s_profiles[j] == s_profile) s_plist_focus = j;
        }
        refresh();
        return;
    default: return;
    }
}

/* a short buzz at the strength just set (snatcherLeo: "it should give me rumble feedback"): the motor on now, off again a
 * quarter of a second later (live_frame) */
Uint64 s_rumble_until = 0;
void rumble_test() {
    if (s_kb) return;
    PADControlMotor(0, PAD_MOTOR_RUMBLE);
    s_rumble_until = SDL_GetTicks() + 250;
}

/* a slider's value from 0..1 along its track */
void set_slider(int i, float t) {
    t = SDL_clamp(t, 0.0f, 1.0f);
    switch (kSlots[i].kind) {
    case S_RANGE: pc_stick_set_range(stick_of(i), 60 + (int)SDL_lround(t * 67)); break;
    case S_DEAD: pc_stick_set_dead(stick_of(i), (int)SDL_lround(t * pc_stick_dead_limit())); break;
    case S_WALKPCT: pc_stick_set_walk_pct(10 + (int)SDL_lround(t * 16) * 5); break;
    case S_RUMBLE: {
        const u16 v = (u16)(SDL_lround(t * 10) * 32767 / 10);
        PADSetRumbleIntensity(0, v, v);
        rumble_test();
        break;
    }
    default: return;
    }
    changed("", false);
}

/* left / right on the focused row: a slider's step, the stick switch; false when it isn't such a row */
bool adjust(int d) {
    if (s_focus < 0) return false;
    const SlotKind k = kSlots[s_focus].kind;
    switch (k) {
    case S_RANGE: pc_stick_set_range(stick_of(s_focus), pc_stick_range(stick_of(s_focus)) + d); break;
    case S_DEAD: pc_stick_set_dead(stick_of(s_focus), pc_stick_dead(stick_of(s_focus)) + d); break;
    case S_WALKPCT: pc_stick_set_walk_pct(pc_stick_walk_pct() + d * 5); break;
    case S_RUMBLE: {
        if (rumble_fixed()) return false;
        const int p = SDL_clamp(rumble_pct() + d * 10, 0, 100);
        const u16 v = (u16)(p * 32767 / 100);
        PADSetRumbleIntensity(0, v, v);
        rumble_test();
        break;
    }
    default: return false;
    }
    changed("", false);
    return true;
}

void reset_one() {
    if (s_focus < 0) return;
    const Slot& slot = kSlots[s_focus];
    if (slot.kind == S_BUTTON) {
        for (u32 i = 0; i < (s_kb ? s_defaults.key_count : s_defaults.button_count); ++i) {
            if (s_kb && s_defaults.keys[i].padButton == slot.pad) assign_key(slot.pad, s_defaults.keys[i].scancode);
            if (!s_kb && s_defaults.buttons[i].padButton == slot.pad) assign_native(slot.pad, s_defaults.buttons[i].nativeButton);
        }
        changed(tx("controls.note.default", {{"name", gc_name(slot.pad)}}), false);
    } else if (slot.kind == S_QUITSC) {
        pc_quit_shortcut_set(nullptr, 0);
        changed(tx("controls.note.quit_cleared"), false);
    } else if (slot.kind == S_FF) {
        /* back to the default (LB / Tab; none on a GameCube controller), which counts only while that button is free */
        pc_ff_reset(s_kb ? 1 : 0);
        const std::string name = pc_ff_name(s_kb ? 1 : 0);
        std::printf("[controls_ui] fast-forward back to its default (%s)\n", name.empty() ? "none" : name.c_str());
        changed(name.empty() ? tx("controls.note.ff_default_none") : tx("controls.note.ff_default", {{"name", bold(name)}}),
                false);
    } else if (slot.kind == S_MENU) {
        pc_shortcut_set(s_kb ? 1 : 0, nullptr, 0);
        changed(tx("controls.note.menu_reset", {{"name", pc_shortcut_name(s_kb ? 1 : 0)}}), false);
    } else if (slot.kind == S_RANGE && !accurate_in_game()) {
        pc_stick_set_range(stick_of(s_focus), pc_stick_range_default(stick_of(s_focus)));
        changed("", false);
    } else if (slot.kind == S_DEAD) {
        pc_stick_set_dead(stick_of(s_focus), 8);
        changed("", false);
    } else if (slot.kind == S_DIR) {
        if (s_kb) {
            for (u32 i = 0; i < s_defaults.key_axis_count; ++i) {
                if (s_defaults.key_axes[i].padAxis == slot.pad) assign_key_axis(slot.pad, s_defaults.key_axes[i].scancode);
            }
        } else {
            for (u32 i = 0; i < s_defaults.axis_count; ++i) {
                if (s_defaults.axes[i].padAxis == slot.pad) assign_axis(slot.pad, s_defaults.axes[i].nativeAxis);
            }
        }
        changed(tx("controls.note.default", {{"name", dir_label(slot.pad)}}), false);
    } else if (slot.kind == S_WALK) {
        if (s_kb) pc_stick_set_walk_key(SDL_SCANCODE_LCTRL);
        else pc_stick_set_walk_button(-1);
        changed("", false);
    } else if (slot.kind == S_WALKPCT) {
        pc_stick_set_walk_pct(50);
        changed("", false);
    } else if (slot.kind == S_CALIB) {
        pc_stick_clear_calibration(stick_of(s_focus));
        changed(tx("controls.note.calib_cleared"), false);
    }
}

/* moving by position (snatcherLeo: "going up or down, or left and right should take me properly to where I need to be"):
 * the nearest focusable slot in that direction, weighting the sideways distance */
struct Box {
    float x, y, w, h;
};
std::optional<Box> box_of(int i) {
    if (s_doc == nullptr) return std::nullopt;
    Rml::Element* e = s_doc->GetElementById(elem_id(i));
    if (e == nullptr) return std::nullopt;
    return Box{e->GetAbsoluteLeft(), e->GetAbsoluteTop(), e->GetClientWidth(), e->GetClientHeight()};
}

void move(int dx, int dy) {
    if (s_asking) {
        s_prompt_focus = SDL_clamp(s_prompt_focus + (dx != 0 ? dx : dy), 0, 1);
        refresh();
        return;
    }
    if (s_dialog != D_NONE) {
        s_dialog_focus = SDL_clamp(s_dialog_focus + (dx != 0 ? dx : dy), 0, dialog_buttons() - 1);
        refresh();
        return;
    }
    if (s_plisting) {
        s_plist_focus = SDL_clamp(s_plist_focus + dy, 0, plist_count() - 1);
        refresh();
        return;
    }
    if (s_listing) {
        s_list_focus = SDL_clamp(s_list_focus + dy, 0, SDL_max((int)PADCount(), 1));
        refresh();
        return;
    }
    if (s_calib) return;
    if (dx != 0 && adjust(dx)) return;
    const auto from = s_focus >= 0 ? box_of(s_focus) : std::nullopt;
    if (!from) {
        s_focus = slot_of_key("A");
        refresh();
        return;
    }
    const float fx = from->x + from->w / 2, fy = from->y + from->h / 2;
    int best = -1;
    float best_score = 1e9f;
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        if (i == s_focus || !focusable(i)) continue;
        const auto b = box_of(i);
        if (!b) continue;
        const float cx = b->x + b->w / 2, cy = b->y + b->h / 2;
        const float along = dx != 0 ? (cx - fx) * dx : (cy - fy) * dy;
        const float across = dx != 0 ? SDL_fabsf(cy - fy) : SDL_fabsf(cx - fx);
        if (along <= 4.0f) continue;
        const float score = along + across * (dx != 0 ? 1.5f : 3.0f);
        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best >= 0) s_focus = best;
    refresh();
}

/* ---- listening for a press, calibrating, and the live view ---- */

/* ---- the fast-forward button (2026-11-16; pc_pad_shape.cpp says the rules). Never swap silently (snatcherLeo's question,
 * 2026-11-16): a game button picked for it is asked about first; a button or key given to something else stops being
 * the fast-forward button, and the note says so ---- */
/* called before a button or key (value, of this screen's device) is given to the game, the walk or a shortcut: if it
 * was the fast-forward button, fast-forward has none now. Returns the note's extra sentence ("" when nothing changed) */
std::string ff_release(int value) {
    const int kb = s_kb ? 1 : 0;
    PcShortcutMember cur;
    if (pc_ff_get(kb, &cur) && cur.type == (s_kb ? PC_SC_KEY : PC_SC_NATIVE) && cur.value == value) {
        pc_ff_set(kb, nullptr);
        std::printf("[controls_ui] fast-forward has no %s now (its button was given to something else)\n",
                    s_kb ? "key" : "button");
        return " " + tx("controls.note.ff_cleared");
    }
    return "";
}

void ff_commit(const PcShortcutMember& m, unsigned gc) {
    pc_ff_set(s_kb ? 1 : 0, &m);
    const std::string name = pc_ff_member_name(&m);
    std::printf("[controls_ui] fast-forward set to %s (%s)%s\n", name.c_str(), s_kb ? "key" : "controller button",
                gc != 0 ? ", also a game button: held back from the game while held" : "");
    std::string text = "&#10003; " + tx("controls.note.now", {{"name", tx("controls.slot.ff")}, {"what", bold(name)}});
    if (gc != 0) text += " " + tx("controls.note.ff_hidden", {{"what", bold(gc_name(gc))}});
    changed(text, true);
}

/* a press chosen for the Fast-forward row */
void ff_pick(int pressed) {
    if (pressed == 1000) { /* an analog trigger is the game's L or R */
        note(tx("controls.note.ff_trigger"), false);
        refresh();
        return;
    }
    const PcShortcutMember m{s_kb ? PC_SC_KEY : PC_SC_NATIVE, pressed};
    unsigned gc = 0;
    switch (pc_ff_conflict(s_kb ? 1 : 0, m, &gc)) {
    case PC_FF_ORREPORT: /* the menu or Quit to hub shortcut, the walk button, F2 / F8 / F11: refused */
        note(tx("controls.note.ff_taken"), false);
        refresh();
        return;
    case PC_FF_STICK: /* a stick direction can't be held back from the game: refused */
        note(tx("controls.note.ff_stick"), false);
        refresh();
        return;
    case PC_FF_GAME: /* a game button: ask first ("Use it anyway" / "Press another"; the second is where it starts) */
        s_ff_pending = m;
        s_ff_pending_gc = gc;
        s_dialog = D_FF;
        s_dialog_focus = 1;
        refresh();
        return;
    default: ff_commit(m, 0); return;
    }
}

void commit_button(const Slot& slot, int pressed, Kind kind) {
    u32 swapped = 0;
    std::string now;
    const std::string ffnote = pressed != 1000 ? ff_release(pressed) : std::string();
    if (s_kb) {
        swapped = assign_key(slot.pad, pressed);
        now = key_name(pressed);
    } else if (pressed == 1000) {
        swapped = assign_native(slot.pad, PAD_NATIVE_BUTTON_INVALID); /* the analog trigger again */
        now = trigger_name(slot.pad == PAD_TRIGGER_L, kind);
    } else {
        swapped = assign_native(slot.pad, (u32)pressed);
        now = button_name((u32)pressed, kind);
    }
    std::printf("[controls_ui] %s set to %d (%s)\n", gc_name(slot.pad).c_str(), pressed, s_kb ? "key" : "controller button");
    std::string text = "&#10003; " + tx("controls.note.now", {{"name", gc_name(slot.pad)}, {"what", bold(now)}});
    if (swapped != 0) {
        text += " " + tx("controls.note.swapped", {{"name", gc_name(swapped)}, {"what", bold(pad_text(swapped, kind))}});
    }
    changed(text + ffnote, true);
}

/* OrrePort's own keys, which a game button, a stick direction or the walk key can't take (snatcherLeo, 2026-11-13: "Do we
 * simply block those shortcuts as options and give a reason to the user?"); null for any other key. Alt alone is fine:
 * only Alt+Enter is fullscreen. Esc cancels the choice (above). Why block them rather than allow the clash: a game
 * button on F11 would toggle fullscreen every time it is pressed, and the player could lose the way into Settings.
 * Platforms: these keys are Windows' usual ones (verified here). On macOS (likely) F11 shows the desktop by default and
 * the usual full-screen key is Ctrl+Cmd+F, so the Mac port should pick its own reserved set; Linux likely keeps F11. */
const char* reserved_key_reason(int scancode) {
    switch (scancode) {
    case SDL_SCANCODE_F11: return pc_text("controls.reserved.f11");
    case SDL_SCANCODE_F2: return pc_text("controls.reserved.f2");
    case SDL_SCANCODE_F8: return pc_text("controls.reserved.f8");
    default: return nullptr;
    }
}

void listen_frame() {
    if (s_listen < 0) return;
    const Slot& slot = kSlots[s_listen];
    const Kind kind = kind_of(port_gamepad());
    const bool combo = slot.kind == S_MENU || slot.kind == S_QUITSC;
    /* what is held now: keys, controller buttons, and the analog triggers (as the GameCube's L / R) */
    std::vector<PcShortcutMember> held;
    if (s_kb) {
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        for (int sc = 0; keys != nullptr && sc < count; ++sc) {
            if (keys[sc]) held.push_back({PC_SC_KEY, sc});
        }
    } else if (SDL_Gamepad* gp = port_gamepad()) {
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
            if (SDL_GetGamepadButton(gp, (SDL_GamepadButton)b)) held.push_back({PC_SC_NATIVE, b});
        }
        if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 22000) held.push_back({PC_SC_GC, (int)PAD_TRIGGER_L});
        if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 22000) held.push_back({PC_SC_GC, (int)PAD_TRIGGER_R});
    }
    /* a stick direction on a controller: the stick axis pushed furthest past 60 %, once every stick is back near the
     * middle (Dolphin's detection, in spirit); its half becomes that direction ("Left Y+") */
    if (slot.kind == S_DIR && !s_kb) {
        SDL_Gamepad* gp = port_gamepad();
        int best = -1;
        float best_v = 0;
        bool centred = true;
        for (int a = SDL_GAMEPAD_AXIS_LEFTX; gp != nullptr && a <= SDL_GAMEPAD_AXIS_RIGHTY; ++a) {
            const float v = SDL_GetGamepadAxis(gp, (SDL_GamepadAxis)a) / 32767.0f;
            centred = centred && SDL_fabsf(v) < 0.3f;
            if (SDL_fabsf(v) > 0.6f && SDL_fabsf(v) > SDL_fabsf(best_v)) {
                best = a;
                best_v = v;
            }
        }
        if (!s_listen_armed) {
            s_listen_armed = centred && held.empty() && s_listen_event < 0;
            s_listen_event = -1;
            if (SDL_GetTicks() >= s_listen_until) {
                s_listen = -1;
                note(tx("controls.note.nothing_pushed"), false);
            }
            refresh();
            return;
        }
        s_listen_event = -1;
        if (best < 0) {
            if (SDL_GetTicks() >= s_listen_until) {
                s_listen = -1;
                note(tx("controls.note.nothing_pushed"), false);
            }
            refresh();
            return;
        }
        s_listen = -1;
        s_listen_armed = false;
        const PADSignedNativeAxis src{best, best_v < 0 ? AXIS_SIGN_NEGATIVE : AXIS_SIGN_POSITIVE};
        const int swapped = assign_axis(slot.pad, src);
        std::printf("[controls_ui] %s set to %s\n", dir_label(slot.pad).c_str(), native_axis_name(src).c_str());
        std::string text =
            "&#10003; " + tx("controls.note.now", {{"name", dir_label(slot.pad)}, {"what", bold(native_axis_name(src))}});
        if (swapped >= 0) {
            text += " " + tx("controls.note.swapped",
                             {{"name", dir_label((u32)swapped)}, {"what", bold(dir_text((u32)swapped, kind))}});
        }
        changed(text, true);
        return;
    }
    if (!s_listen_armed) {
        s_listen_armed = held.empty() && s_listen_event < 0; /* the press that chose the row first has to be let go */
        s_listen_event = -1;
        if (SDL_GetTicks() >= s_listen_until) {
            s_listen = -1;
            note(tx("controls.note.nothing_pressed"), false);
        }
        refresh();
        return;
    }
    if (combo) {
        for (const PcShortcutMember& m : held) {
            bool known = false;
            for (const PcShortcutMember& c : s_combo) known = known || (c.type == m.type && c.value == m.value);
            if (!known && s_combo.size() < 2) s_combo.push_back(m);
        }
        const bool done = !s_combo.empty() && held.empty();
        if (done || SDL_GetTicks() >= s_listen_until) {
            s_listen = -1;
            if (s_combo.empty()) {
                note(tx("controls.note.nothing_pressed"), false);
                refresh();
                return;
            }
            for (const PcShortcutMember& c : s_combo) { /* the keyboard's menu key can't be F11 or F2 either */
                if (c.type == PC_SC_KEY && reserved_key_reason(c.value) != nullptr) {
                    note(tx("controls.note.reserved", {{"why", reserved_key_reason(c.value)}}), false);
                    refresh();
                    return;
                }
            }
            if (slot.kind == S_QUITSC) { /* never the same buttons as the menu's */
                PcShortcutMember menu[2];
                const int mn = pc_shortcut_get(0, menu);
                bool same = mn == (int)s_combo.size();
                for (int i = 0; same && i < mn; ++i) {
                    bool found = false;
                    for (const PcShortcutMember& c : s_combo) found = found || (c.type == menu[i].type && c.value == menu[i].value);
                    same = found;
                }
                if (same) {
                    note(tx("controls.note.menu_taken"), false);
                    refresh();
                    return;
                }
            }
            /* a shortcut member that was the fast-forward button: fast-forward has none now (said in the note) */
            std::string ffnote;
            for (const PcShortcutMember& c : s_combo) {
                if (c.type == (s_kb ? PC_SC_KEY : PC_SC_NATIVE) && ffnote.empty()) ffnote = ff_release(c.value);
            }
            if (slot.kind == S_QUITSC) {
                pc_quit_shortcut_set(s_combo.data(), (int)s_combo.size());
                std::printf("[controls_ui] Quit to hub shortcut set to %s\n", pc_quit_shortcut_name());
                changed("&#10003; " + tx("controls.note.now", {{"name", tx("settings.quit")}, {"what", bold(pc_quit_shortcut_name())}}) +
                            ffnote,
                        true);
                return;
            }
            pc_shortcut_set(s_kb ? 1 : 0, s_combo.data(), (int)s_combo.size());
            std::printf("[controls_ui] menu shortcut set to %s\n", pc_shortcut_name(s_kb ? 1 : 0));
            changed("&#10003; " + tx("controls.note.now", {{"name", tx("controls.slot.menu")},
                                                          {"what", bold(pc_shortcut_name(s_kb ? 1 : 0))}}) +
                        ffnote,
                    true);
            return;
        }
        refresh();
        return;
    }
    int pressed = s_listen_event;
    s_listen_event = -1;
    if (pressed < 0 && !held.empty()) {
        const PcShortcutMember& m = held.front();
        pressed = m.type == PC_SC_GC ? 1000 : m.value; /* 1000: an analog trigger */
    }
    if (SDL_GetTicks() >= s_listen_until && pressed < 0) {
        s_listen = -1;
        note(tx("controls.note.nothing_pressed"), false);
        refresh();
        return;
    }
    if (pressed < 0) {
        refresh(); /* the countdown */
        return;
    }
    if (s_kb && pressed == SDL_SCANCODE_ESCAPE) {
        s_listen = -1;
        note(tx("controls.note.no_change"), false);
        refresh();
        return;
    }
    if (s_kb && reserved_key_reason(pressed) != nullptr) { /* OrrePort's own keys never become game keys */
        s_listen = -1;
        note(tx("controls.note.reserved", {{"why", reserved_key_reason(pressed)}}), false);
        refresh();
        return;
    }
    s_listen = -1;
    s_listen_armed = false;
    if (slot.kind == S_FF) { /* the fast-forward button (checked against what the press already does) */
        ff_pick(pressed);
        return;
    }
    if (slot.kind == S_DIR) { /* the keyboard: the key pressed */
        const std::string ffnote = ff_release(pressed);
        const int swapped = assign_key_axis(slot.pad, pressed);
        std::printf("[controls_ui] %s set to key %d\n", dir_label(slot.pad).c_str(), pressed);
        std::string text = "&#10003; " + tx("controls.note.now", {{"name", dir_label(slot.pad)}, {"what", bold(key_name(pressed))}});
        if (swapped >= 0) {
            text += " " + tx("controls.note.swapped",
                             {{"name", dir_label((u32)swapped)}, {"what", bold(dir_text((u32)swapped, kind))}});
        }
        changed(text + ffnote, true);
        return;
    }
    if (slot.kind == S_WALK) {
        const std::string ffnote = pressed != 1000 ? ff_release(pressed) : std::string();
        if (s_kb) pc_stick_set_walk_key(pressed);
        else if (pressed != 1000) pc_stick_set_walk_button(pressed);
        changed("&#10003; " + tx("controls.note.now",
                                 {{"name", tx("controls.slot.walk")},
                                  {"what", bold(s_kb ? key_name(pressed) : pressed == 1000 ? "?" : button_name((u32)pressed, kind))}}) +
                    ffnote,
                true);
        return;
    }
    if (pressed == 1000 && slot.pad != PAD_TRIGGER_L && slot.pad != PAD_TRIGGER_R) {
        note(tx("controls.note.triggers_only"), false); /* an analog trigger has no button of its own to give */
        refresh();
        return;
    }
    commit_button(slot, pressed, kind);
}

/* an element's place inside a gate box (112 dp, the centre at 56), from a point in dp with up positive */
void place(const std::string& id, float px, float py, float half) {
    if (Rml::Element* e = s_doc->GetElementById(id)) {
        e->SetProperty("left", std::to_string(56 + px - half) + "dp");
        e->SetProperty("top", std::to_string(56 - py - half) + "dp");
    }
}

void set_class(const std::string& id, const char* name, bool on) {
    if (Rml::Element* e = s_doc->GetElementById(id)) e->SetClass(name, on);
}

/* the press feedback's memory: what was held at the last frame, and until when a label flashes */
std::map<std::string, bool> s_was_held;
std::map<std::string, Uint64> s_flash_until;
constexpr Uint64 kFlashMs = 160;

void live_frame() {
    if (s_doc == nullptr) return;
    SDL_Gamepad* gp = port_gamepad();
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const Uint64 now = SDL_GetTicks();
    if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("dragging", s_drag >= 0); /* one cursor */
    if (s_rumble_until != 0 && now >= s_rumble_until) { /* the test buzz ends */
        PADControlMotor(0, PAD_MOTOR_STOP);
        s_rumble_until = 0;
    }
    /* what is held now, per picture key (the D-pad's arms each on their own) */
    std::map<std::string, bool> held;
    bool arm[4] = {false, false, false, false};
    for (const Slot& slot : kSlots) {
        if (slot.kind != S_BUTTON) continue;
        bool on = false;
        if (s_kb) {
            const s32 k = key_for(slot.pad);
            on = k >= 0 && keys != nullptr && keys[k];
        } else if (gp != nullptr) {
            const u32 native = native_for(slot.pad);
            if (native != PAD_NATIVE_BUTTON_INVALID) {
                on = SDL_GetGamepadButton(gp, (SDL_GamepadButton)native);
            } else if (slot.pad == PAD_TRIGGER_L || slot.pad == PAD_TRIGGER_R) {
                float amount = 0;
                bool click = false;
                trigger_now(slot.pad == PAD_TRIGGER_L, amount, click);
                on = click;
            }
        }
        held[slot.key] = held[slot.key] || on;
        if (on && slot.pad == PAD_BUTTON_UP) arm[0] = true;
        if (on && slot.pad == PAD_BUTTON_DOWN) arm[1] = true;
        if (on && slot.pad == PAD_BUTTON_LEFT) arm[2] = true;
        if (on && slot.pad == PAD_BUTTON_RIGHT) arm[3] = true;
    }
    /* the press-in (snatcherLeo, 2026-11-12: "button presses are perfect"; mockups 23-40): each held button's own picture
     * sinks (controls.rcss .piece.down), Z dips, the label flashes on the press; the D-pad rocks toward the pressed arm,
     * that arm shaded and the opposite one lit */
    for (const auto& [key, on] : held) {
        if (on && !s_was_held[key]) s_flash_until[key] = now + kFlashMs;
        s_was_held[key] = on;
        if (key != "DPAD" && key != "L" && key != "R") set_class("piece-btn-" + key, "down", on);
        set_class("lab-" + key, "flash", now < s_flash_until[key]);
    }
    static const char* const kArmNames[] = {"UP", "DOWN", "LEFT", "RIGHT"};
    static const int kOpposite[] = {1, 0, 3, 2};
    int rock = -1;
    for (int i = 0; i < 4 && rock < 0; ++i) {
        if (arm[i]) rock = i;
    }
    if (Rml::Element* wrap = s_doc->GetElementById("dpadwrap")) {
        for (int i = 0; i < 4; ++i) wrap->SetClass(std::string("rock-") + kArmNames[i], i == rock);
    }
    for (int i = 0; i < 4; ++i) {
        set_class(std::string("arm-") + kArmNames[i], "shade", i == rock);
        set_class(std::string("arm-") + kArmNames[i], "shine", rock >= 0 && i == kOpposite[rock]);
    }
    /* L and R slide into the shoulder as far as they are squeezed (R a little less: Z in front of it hides more of
     * its rim as it sinks, so the same travel read deeper; the mockups' balance) */
    for (int side = 0; side < 2; ++side) {
        float amount = 0;
        bool click = false;
        trigger_now(side == 0, amount, click);
        if (Rml::Element* e = s_doc->GetElementById(side == 0 ? "piece-trig-L" : "piece-trig-R")) {
            const float dx = (side == 0 ? 5.0f : -3.8f) * amount, dy = (side == 0 ? 11.0f : 8.3f) * amount;
            char t[96];
            SDL_snprintf(t, sizeof t, "translate(%.2fdp, %.2fdp)", dx, dy);
            e->SetProperty("transform", t);
            e->SetProperty("image-color", click ? "rgb(224, 224, 224)" : "rgb(255, 255, 255)");
        }
    }
    /* the triggers: the side's bars, and the fill under the L and R labels */
    for (const char* t : {"L", "R"}) {
        float amount = 0;
        bool click = false;
        trigger_now(t[0] == 'L', amount, click);
        const std::string pct = std::to_string((int)SDL_lround(amount * 100)) + "%";
        if (Rml::Element* e = s_doc->GetElementById(std::string("tf-") + t)) e->SetProperty("width", pct);
        if (Rml::Element* e = s_doc->GetElementById(std::string("lfi-") + t)) e->SetProperty("width", pct);
        if (Rml::Element* e = s_doc->GetElementById(std::string("tp-") + t)) {
            if (e->GetInnerRML() != pct) e->SetInnerRML(pct);
        }
        set_class(std::string("td-") + t, "on", click);
    }
    /* both sticks: the ring where the stick is (1.0 of its travel = 127 units = 50 dp), the dot what the game is sent;
     * on the picture a loose dot on each cap (deliberately rough: the side's view is the exact one) */
    const float k = 50.0f / 127.0f;
    for (int g = 0; g < 2; ++g) {
        float x = 0, y = 0;
        raw_stick(g, x, y);
        int sx = 0, sy = 0;
        pc_stick_shape(g, x, y, g == PC_STICK_MAIN && walk_held_now() ? 1 : 0, 0, &sx, &sy);
        const std::string n = std::to_string(g);
        place("raw-" + n, SDL_clamp(x, -1.3f, 1.3f) * 127 * k, SDL_clamp(y, -1.3f, 1.3f) * 127 * k, 6.5f);
        place("sent-" + n, sx * k, sy * k, 4.0f);
        if (Rml::Element* e = s_doc->GetElementById("num-" + n)) {
            const std::string text = "<span class='x'>X</span><b>" + std::to_string(sx) + "</b><span class='x'>Y</span><b>" +
                                     std::to_string(sy) + "</b>";
            if (e->GetInnerRML() != text) e->SetInnerRML(text);
        }
        const char* follow = g == 0 ? "follow-STICK" : "follow-C";
        const bool moving = std::sqrt(x * x + y * y) > 0.15f;
        set_class(follow, "on", moving);
        /* moved from its place in gcpad-layout.rcss by a transform, never by rewriting left / top (snatcherLeo, 2026-11-13: "a
         * yellow dot appears on the left-hand part of the UI"). Why: the resting place used to be read back from the
         * layout when the page was built, before RmlUi had applied the stylesheet, so it read 0, 0 and the dot showed
         * in the stage's top-left corner instead of on the stick */
        if (Rml::Element* e = s_doc->GetElementById(follow); e != nullptr && moving) {
            e->SetProperty("transform", "translate(" + std::to_string(SDL_clamp(x, -1.0f, 1.0f) * 9) + "dp, " +
                                            std::to_string(-SDL_clamp(y, -1.0f, 1.0f) * 9) + "dp)");
        }
    }
    /* the direction rows light while pushed (as Dolphin's do), except on the stick being calibrated (snatcherLeo: "When
     * calibrating, in Dolphin, the "up", "down", "left" "right' things don't light up") */
    for (int i = 0; i < (int)kSlots.size(); ++i) {
        if (kSlots[i].kind != S_DIR) continue;
        const bool lit = i != s_listen && !(s_calib && stick_of(i) == s_stick) && dir_now(kSlots[i].pad) > 0.3f;
        set_class(elem_id(i), "push", lit);
    }
    float x = 0, y = 0;
    raw_stick(s_stick, x, y);
    if (s_calib) {
        const float r = std::sqrt(x * x + y * y);
        const float a = std::atan2(y, x);
        if (r > 0.5f) {
            int b = (int)std::floor((a < 0 ? a + 2 * 3.14159265f : a) / (2 * 3.14159265f) * PC_CAL_SAMPLES);
            b = SDL_clamp(b, 0, PC_CAL_SAMPLES - 1);
            s_cal_bins[b] = SDL_max(s_cal_bins[b], SDL_min(r, 1.5f));
            /* a quick roll moves more than one direction (11.25 degrees) between two looks at the stick (snatcherLeo,
             * 2026-11-13: after the circle it still said "Cancel Calibration"): the directions passed on the way,
             * the short way round from the last look, take the smaller of the two reaches (never more than the
             * stick showed) */
            if (s_cal_last_bin >= 0 && s_cal_last_bin != b) {
                const int fwd = (b - s_cal_last_bin + PC_CAL_SAMPLES) % PC_CAL_SAMPLES;
                const int step = fwd <= PC_CAL_SAMPLES / 2 ? 1 : -1;
                const int span = step > 0 ? fwd : PC_CAL_SAMPLES - fwd;
                if (span <= PC_CAL_SAMPLES / 4) { /* up to a quarter turn between looks; more is a jump, not a roll */
                    const float reach = SDL_min(SDL_min(r, 1.5f), s_cal_last_r);
                    for (int i = 1; i < span; ++i) {
                        const int m = (s_cal_last_bin + step * i + PC_CAL_SAMPLES) % PC_CAL_SAMPLES;
                        s_cal_bins[m] = SDL_max(s_cal_bins[m], reach);
                    }
                }
            }
            s_cal_last_bin = b;
            s_cal_last_r = SDL_min(r, 1.5f);
            if (cal_complete() != s_cal_was_complete) { /* Cancel Calibration becomes Finish Calibration */
                s_cal_was_complete = cal_complete();
                refresh();
            }
        } else {
            s_cal_last_bin = -1; /* back near the middle: the next push starts afresh */
        }
        for (int b = 0; b < PC_CAL_SAMPLES; ++b) {
            const float ang = (float)b / PC_CAL_SAMPLES * 2 * 3.14159265f;
            const float rr = s_cal_bins[b] > 0 ? s_cal_bins[b] * 127 * k : 0;
            place("cd-" + std::to_string(b), std::cos(ang) * rr, std::sin(ang) * rr, 2.0f);
            if (Rml::Element* e = s_doc->GetElementById("cd-" + std::to_string(b))) {
                e->SetProperty("visibility", s_cal_bins[b] > 0 ? "visible" : "hidden");
            }
        }
    }
}

/* the screen's own size: the 960 x 640 frame plus the side (about 1,340 x 800 dp), never above the display scaling */
void fit() {
    Rml::Context* context = aurora::rmlui::get_context();
    if (context == nullptr) return;
    float system = 1.0f;
    int count = 0;
    if (SDL_Window** w = SDL_GetWindows(&count)) {
        if (count > 0) system = SDL_GetWindowDisplayScale(w[0]);
        SDL_free(w);
    }
    const Rml::Vector2i dims = context->GetDimensions();
    float scale = SDL_min(system > 0 ? system : 1.0f, SDL_min(dims.x / 1340.0f, dims.y / 800.0f));
    if (scale > 0.0f && SDL_fabsf(scale - context->GetDensityIndependentPixelRatio()) > 0.01f) {
        aurora::rmlui::set_ui_scale(scale);
    }
}

/* ---- the mouse ---- */
int clicked(Rml::Element* e) {
    for (; e != nullptr; e = e->GetParentNode()) {
        const std::string id = e->GetId();
        if (id.rfind("lab-", 0) == 0 && id != "lab-DPAD") return slot_of_key(id.substr(4));
        if (id.rfind("slot-", 0) == 0) return std::atoi(id.c_str() + 5);
        if (id == "dev") return slot_of(S_DEVICE);
        if (id == "prof") return slot_of(S_PROFILE);
        if (id.rfind("popt-", 0) == 0) return 7000 + std::atoi(id.c_str() + 5);
        if (id == "pname" || id == "prompt") return 8000; /* inside a dialog, not a button */
        if (id.rfind("gate-", 0) == 0) return 4000 + std::atoi(id.c_str() + 5);
        if (id.rfind("opt-", 0) == 0) return 1000 + std::atoi(id.c_str() + 4);
        if (id.rfind("ask-", 0) == 0) return 2000 + std::atoi(id.c_str() + 4);
        if (id == "back") return 3000;
    }
    return -1;
}

class Clicks : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (s_listen >= 0) return;
        const int c = clicked(event.GetTargetElement());
        if (s_calib) { /* only the calibrate button works while calibrating (Dolphin) */
            if (c >= 0 && c < 1000 && kSlots[c].kind == S_CALIB && stick_of(c) == s_stick) finish_calib();
            return;
        }
        if (s_dialog != D_NONE) { /* a profile question: only its buttons (the name box takes its own clicks) */
            if (c >= 2000 && c < 3000) {
                s_dialog_focus = c - 2000;
                dialog_choose();
            }
            return;
        }
        if (c >= 7000 && c < 8000) { /* the profile list */
            s_plist_focus = c - 7000;
            plist_choose();
            return;
        }
        if (s_plisting && !(c >= 0 && c < 1000 && kSlots[c].kind == S_PROFILE)) {
            s_plisting = false; /* a click elsewhere closes the list */
            refresh();
            return;
        }
        if (c == 8000) return;
        if (c >= 4000) { /* a stick's picture: its column's first control */
            s_stick = c - 4000;
            s_focus = s_kb ? slot_of_axis(s_stick == PC_STICK_C ? PAD_AXIS_RIGHT_Y_POS : PAD_AXIS_LEFT_Y_POS)
                           : slot_of_stick(S_CALIB, s_stick);
            refresh();
        } else if (c == 3000) {
            back();
        } else if (c >= 2000) {
            s_prompt_focus = c - 2000;
            choose();
        } else if (c >= 1000) {
            s_list_focus = c - 1000;
            choose();
        } else if (c >= 0) {
            if (s_listing && kSlots[c].kind != S_DEVICE) {
                s_listing = false; /* a click elsewhere closes the list */
                refresh();
                return;
            }
            const SlotKind k = kSlots[c].kind;
            if (!focusable(c)) return; /* a greyed row (the keyboard's stick-only rows) */
            s_focus = c;
            if (k == S_RANGE || k == S_DEAD || k == S_WALKPCT || (k == S_RUMBLE && !rumble_fixed())) {
                refresh(); /* sliders are set by the press and drag (Drags) */
                return;
            }
            choose();
        } else if (s_listing) {
            s_listing = false;
            refresh();
        } else if (s_focus >= 0) { /* a click on empty space: nothing selected (snatcherLeo: "it should stop selecting") */
            s_focus = -1;
            refresh();
        }
    }
};
Clicks s_clicks;

/* dragging a slider (snatcherLeo: "we need sliders for those we can freely drag around") */
class Drags : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        const float x = event.GetParameter<float>("mouse_x", 0.0f);
        if (event.GetId() == Rml::EventId::Mouseup) {
            s_drag = -1;
            return;
        }
        if (event.GetId() == Rml::EventId::Mousemove) {
            if (s_drag >= 0) set_slider(s_drag, (x - s_drag_left) / s_drag_width);
            return;
        }
        if (s_listen >= 0 || s_calib) return;
        for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
            const std::string id = e->GetId();
            int slot = -1;
            Rml::Element* track = nullptr;
            if (id.rfind("track-", 0) == 0) {
                slot = std::atoi(id.c_str() + 6);
                track = e;
            } else if (id.rfind("slot-", 0) == 0) {
                slot = std::atoi(id.c_str() + 5);
                track = e->QuerySelector(".track");
            }
            if (slot < 0) continue;
            if (!focusable(slot)) return; /* a greyed row */
            if (track != nullptr && x >= track->GetAbsoluteLeft() - 12.f) {
                s_focus = slot;
                s_drag = slot;
                s_drag_left = track->GetAbsoluteLeft();
                s_drag_width = SDL_max(track->GetClientWidth(), 1.0f);
                set_slider(slot, (x - s_drag_left) / s_drag_width);
            }
            return;
        }
    }
};
Drags s_drags;

/* ---- input ---- */
input::EventResult on_event(const input::InputEvent& event, void*) {
    using E = input::InputEvent;
    if (!s_open) return input::EventResult::Pass;
    if (s_dialog == D_NAME_NEW || s_dialog == D_NAME_RENAME) {
        /* naming a profile: typing goes into the box (RmlUi); Enter or A confirms, Esc or B cancels; the mouse works */
        if (const auto* key = event.payload.get_if<E::KeyChanged>(); key != nullptr && key->pressed && !key->repeat) {
            if (key->keycode == SDLK_ESCAPE) {
                back();
                return input::EventResult::Consume;
            }
            if (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER) {
                name_confirm();
                return input::EventResult::Consume;
            }
        } else if (const auto* b = event.payload.get_if<E::ButtonChanged>()) {
            if (b->pressed) {
                const bool gc = kind_of(port_gamepad()) == KIND_GAMECUBE;
                if (b->button == SDL_GAMEPAD_BUTTON_SOUTH) name_confirm();
                if (b->button == (gc ? SDL_GAMEPAD_BUTTON_WEST : SDL_GAMEPAD_BUTTON_EAST)) back();
            }
            return input::EventResult::Consume;
        } else if (event.payload.get_if<E::AxisChanged>() != nullptr) {
            return input::EventResult::Consume;
        }
        aurora::rmlui::process_input(event);
        return input::EventResult::Consume;
    }
    if (s_listen >= 0) {
        /* waiting for a press: taken from the press events (a quick tap is never missed) and, each frame, from what
         * the device holds (listen_frame); Esc on the keyboard cancels when a controller is being set up */
        if (const auto* key = event.payload.get_if<E::KeyChanged>(); key != nullptr && key->pressed && !key->repeat) {
            if (!s_kb && key->keycode == SDLK_ESCAPE) {
                back();
            } else if (s_kb && kSlots[s_listen].kind != S_MENU && kSlots[s_listen].kind != S_QUITSC) {
                s_listen_event = (int)key->scancode;
            }
        } else if (const auto* b = event.payload.get_if<E::ButtonChanged>();
                   b != nullptr && b->pressed && !s_kb && kSlots[s_listen].kind != S_MENU &&
                   kSlots[s_listen].kind != S_QUITSC) {
            s_listen_event = (int)b->button;
        }
        return input::EventResult::Consume;
    }
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        if (key->pressed) {
            switch (key->keycode) {
            case SDLK_UP: case SDLK_KP_8: move(0, -1); break;
            case SDLK_DOWN: case SDLK_KP_2: move(0, 1); break;
            case SDLK_LEFT: case SDLK_KP_4: move(-1, 0); break;
            case SDLK_RIGHT: case SDLK_KP_6: move(1, 0); break;
            case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: if (!key->repeat) choose(); break;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: if (!key->repeat) back(); break;
            case SDLK_DELETE: if (!key->repeat) reset_one(); break;
            default: break;
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        if (button->pressed) {
            const bool gc = kind_of(port_gamepad()) == KIND_GAMECUBE;
            switch (button->button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP: move(0, -1); break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move(0, 1); break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: move(-1, 0); break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: move(1, 0); break;
            case SDL_GAMEPAD_BUTTON_SOUTH: choose(); break;
            case SDL_GAMEPAD_BUTTON_START: if (!s_calib) back(); break;
            default:
                if (button->button == (gc ? SDL_GAMEPAD_BUTTON_WEST : SDL_GAMEPAD_BUTTON_EAST)) back();
                if (button->button == (gc ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_WEST)) reset_one();
                break;
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* axis = event.payload.get_if<E::AxisChanged>()) {
        /* the left stick moves like the D-pad (not while calibrating: then it is the stick being measured) */
        static float sx = 0, sy = 0;
        static int held = 0;
        if (!s_calib && (axis->axis == SDL_GAMEPAD_AXIS_LEFTX || axis->axis == SDL_GAMEPAD_AXIS_LEFTY)) {
            (axis->axis == SDL_GAMEPAD_AXIS_LEFTX ? sx : sy) = axis->value;
            const int now = sy < -0.5f ? 1 : sy > 0.5f ? 2 : sx < -0.5f ? 3 : sx > 0.5f ? 4 : 0;
            if (now != 0 && now != held) {
                move(now == 3 ? -1 : now == 4 ? 1 : 0, now == 1 ? -1 : now == 2 ? 1 : 0);
            }
            held = now;
        }
        return input::EventResult::Consume;
    }
    aurora::rmlui::process_input(event); /* the mouse */
    return input::EventResult::Consume;
}

bool captures(const input::InputSource&, void*) { return s_open; }

bool init() {
    static int tried = -1;
    if (tried >= 0) return tried == 1;
    tried = 0;
    if (!aurora::rmlui::is_initialized()) return false;
    /* the controller picture and the stick gate for each theme, drawn once by the hub's SVG reader */
    aurora::rmlui::register_texture_provider("controlsart", [](std::string_view source) {
        static std::map<std::string, std::tuple<int, int, std::vector<unsigned char>>> pictures;
        std::string name(source);
        const size_t slash = name.rfind('/');
        name = slash == std::string::npos ? name : name.substr(slash + 1);
        auto found = pictures.find(name);
        if (found == pictures.end()) {
            /* the gate and the buttons' own pictures are small; the body and the lines span the whole frame */
            const bool small = name.rfind("gate", 0) == 0 ||
                               (name.rfind("piece-", 0) == 0 && name != "piece-base" && name.rfind("piece-lines", 0) != 0);
            const int width = small ? 400 : 1920;
            size_t len = 0;
            std::tuple<int, int, std::vector<unsigned char>> pic{width, 0, {}};
            if (void* data = SDL_LoadFile(ui_path(("controls/" + name + ".svg").c_str()).c_str(), &len)) {
                pc_svg_render(std::string(static_cast<const char*>(data), len), width, std::get<1>(pic),
                              std::get<2>(pic), {});
                SDL_free(data);
            }
            found = pictures.emplace(name, std::move(pic)).first;
        }
        const auto& [w, h, px] = found->second;
        if (px.empty() || h <= 0) return std::optional<aurora::rmlui::RuntimeTexture>{};
        return std::optional<aurora::rmlui::RuntimeTexture>{aurora::rmlui::RuntimeTexture{
            (uint32_t)w, (uint32_t)h, std::span<const std::byte>(reinterpret_cast<const std::byte*>(px.data()), px.size()),
            false, true}};
    });
    Rml::Context* context = aurora::rmlui::get_context();
    s_doc = context != nullptr ? context->LoadDocument(ui_path("controls.rml")) : nullptr;
    if (s_doc == nullptr) {
        std::printf("[controls_ui] could not load controls.rml\n");
        return false;
    }
    s_doc->AddEventListener(Rml::EventId::Click, &s_clicks);
    s_doc->AddEventListener(Rml::EventId::Mousedown, &s_drags);
    s_doc->AddEventListener(Rml::EventId::Mousemove, &s_drags);
    s_doc->AddEventListener(Rml::EventId::Mouseup, &s_drags);
    /* the controller in layers (gcpad_stage.py pieces(); places in gcpad-layout.rcss): the triggers behind the body,
     * the body, each pressable button its own picture, the D-pad with its arms' shading in a wrapper that rocks, the
     * lines on top */
    auto img = [](const std::string& name, const char* extra = "") {
        return "<img class='piece" + std::string(extra) + "' id='piece-" + name + "' src='controlsart://piece-" + name +
               "'/>";
    };
    std::string layers = img("trig-L") + img("trig-R") + img("base", " full");
    for (const char* b : {"btn-Z", "btn-START", "btn-A", "btn-B", "btn-X", "btn-Y"}) layers += img(b);
    layers += "<div id='dpadwrap'>" + img("btn-DPAD");
    for (const char* arm : {"UP", "DOWN", "LEFT", "RIGHT"}) layers += std::string("<div class='arm' id='arm-") + arm + "'></div>";
    layers += "</div>" + img("lines-dark", " full");
    if (Rml::Element* e = s_doc->GetElementById("layers")) e->SetInnerRML(layers);
    if (Rml::Element* e = s_doc->GetElementById("piece-lines-dark")) e->SetId("piece-lines");
    std::string rings, lits;
    for (const char* key : {"L", "R", "Z", "STICK", "DPAD", "START", "A", "B", "X", "Y", "C"}) {
        lits += std::string("<div class='lit' id='lit-") + key + "'></div><div class='litdot' id='litdot-" + key + "'></div>";
    }
    /* the sticks' loose followers (places in gcpad-layout.rcss) */
    rings += "<div class='follow' id='follow-STICK'></div><div class='follow' id='follow-C'></div>";
    if (Rml::Element* e = s_doc->GetElementById("rings")) e->SetInnerRML(rings);
    if (Rml::Element* e = s_doc->GetElementById("lits")) e->SetInnerRML(lits);
    s_doc->Hide();
    s_layer = input::register_layer({
        .label = "OrrePort controller setup",
        .priority = input::kRmlUiLayerPriority + 5,
        .onEvent = on_event,
        .capturesSource = captures,
    });
    tried = 1;
    return true;
}
} // namespace

/* Settings > Controls > Set up controller (pc_settings_overlay.cpp) */
extern "C" void pc_controls_ui_open(void) {
    if (!init() || s_open) return;
    s_open = true;
    s_kb = port_gamepad() == nullptr; /* the controller if one is connected, else the keyboard */
    s_focus = slot_of_key("A");
    s_stick = PC_STICK_MAIN;
    s_listen = -1;
    s_dirty = s_asking = s_listing = s_calib = s_plisting = false;
    s_dialog = D_NONE;
    s_drag = -1;
    note("", false);
    s_defaults = read_defaults();
    pick_device_profile(); /* the device's remembered profile (loaded if another one is live) */
    s_before = take();
    fit();
    refresh();
    s_doc->Show();
    s_doc->PullToFront();
    input::reconcile();
    std::printf("[controls_ui] opened (%s)\n", s_kb ? "the keyboard" : "Player 1's controller");
}

/* Settings › Enhancements › Speed's Fast-forward link (2026-11-16): this screen on its Fast-forward row, for the device
 * the link named (keyboard 1: the keyboard; 0: Player 1's controller, if one is connected) */
extern "C" void pc_controls_ui_open_ff(int keyboard) {
    if (!init() || s_open) return;
    pc_controls_ui_open();
    if (!s_open) return;
    const bool kb = keyboard != 0 || port_gamepad() == nullptr;
    if (kb != s_kb) { /* the other device's own setup, as choosing it in the device list does */
        s_kb = kb;
        s_defaults = read_defaults();
        pick_device_profile();
        s_before = take();
    }
    s_focus = slot_of(S_FF);
    refresh();
    std::printf("[controls_ui] opened on Fast-forward (%s), from Settings › Speed\n", s_kb ? "the keyboard" : "the controller");
}

extern "C" int pc_controls_ui_is_open(void) { return s_open ? 1 : 0; }

/* Right after the GameCube adapter setup (Settings > Controls' row; pc_settings_overlay.cpp): when a controller on
 * Nintendo's adapter (USB 057E:0337) is connected, it becomes Player 1 and gets its own remembered layout ("GameCube
 * Controller" by default), as choosing it here would do. 1 when it did (or it already was), 0 while none is there.
 * Why (snatcherLeo, 2026-11-13: "automatically change the default controller to gamecube after doing this process"): in his
 * test the controller worked only after he opened Set up controller, whose opening does the same steps (the device's
 * profile, the input layers reconciled). Platform-neutral (Aurora's PAD calls); only Windows needs the setup itself. */
extern "C" int pc_controls_claim_gamecube_pad(void) {
    const u32 n = PADCount();
    for (u32 i = 0; i < n; ++i) {
        SDL_Gamepad* gp = PADGetSDLGamepadForIndex(i);
        if (gp == nullptr || SDL_GetGamepadVendor(gp) != 0x057E || SDL_GetGamepadProduct(gp) != 0x0337) continue;
        if (PADGetIndexForPort(0) != (s32)i) PADSetPortForIndex(i, 0);
        if (init() && !s_open) {
            s_kb = false;
            s_defaults = read_defaults(); /* this controller's own factory buttons */
            pick_device_profile();        /* its remembered profile, if another one is live */
            s_before = take();
        }
        input::reconcile();
        std::printf("[controls_ui] Player 1 now uses the GameCube adapter's controller %u (%s), after the adapter setup\n", i,
                    PADGetNameForControllerIndex(i));
        return 1;
    }
    return 0;
}

/* every frame while Settings is up (pc_settings_overlay.cpp frame_common): the press being waited for, the live view */
extern "C" void pc_controls_ui_frame(void) {
    if (!s_open) return;
    fit();
    listen_frame();
    live_frame();
    if (s_scroll_frames > 0 && --s_scroll_frames == 0) scroll_side_to_focus();
    /* a profile's name box takes the keyboard once it is laid out (focusing it while hidden fails; the Settings
     * overlay's report box does the same) */
    if (s_name_focus_frames > 0 && --s_name_focus_frames == 0) {
        if (Rml::Element* box = s_doc->GetElementById("pname")) {
            box->Focus();
            if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(box)) {
                const int n = (int)input->GetValue().size();
                input->SetSelectionRange(0, n); /* typing replaces the suggested name */
            }
        }
    }
}

/* the menu shortcut's name, for the hint at game start and the menu's footer (pc_pad_shape.cpp) */
extern "C" const char* pc_controls_menu_name(int keyboard) { return pc_shortcut_name(keyboard); }

/* names for pc_pad_shape.cpp: a controller button as printed on Player 1's controller, and a key */
extern "C" const char* pc_controls_native_label(int native) {
    static std::string s;
    s = button_name((u32)native, kind_of(port_gamepad()));
    return s.c_str();
}
extern "C" const char* pc_controls_key_label(int scancode) {
    static std::string s;
    s = key_name(scancode);
    return s.c_str();
}
