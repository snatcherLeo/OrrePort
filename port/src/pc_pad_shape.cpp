/*
 * pc_pad_shape.cpp: Player 1's stick shaping and the OrrePort menu shortcut (2026-11-12; snatcherLeo's hand check of
 * the controller setup screen; docs/port/controls.md "Sticks" and "The menu shortcut"; the decision log, the Controls
 * rework rows).
 *
 * THE STICKS, the way Dolphin does it (read in Dolphin's source, InputCommon/ControllerEmu: ReshapableInput,
 * OctagonAnalogStick, GCPadEmu): the raw position is moved by the calibrated centre, divided by how far this stick
 * really reaches in that direction (the calibration: the edge recorded while the player rolls the stick), optionally
 * snapped to the 8 directions, scaled by the walk button, put through the dead zone, and laid on the GameCube's
 * octagonal gate whose corners sit at the range (default 0.7937 / 0.7221 of 127, Dolphin's figures "averaged from
 * multiple genuine GameCube controllers": 101 for the Control stick, 92 for the C-Stick).
 *  - Normal mode: the dead zone rescales (the stick eases in from its edge); the range is the player's.
 *  - GameCube-accurate mode (snatcherLeo: dead zone "capped at 10 in accurate mode", 2026-11-12, from Chase's port): the
 *    range is the GameCube's; the dead zone only zeroes tilts inside it and passes every other value unchanged; a
 *    real GameCube controller (the adapter) is not reshaped at all beyond that dead zone and the walk button.
 * Why it matters (verified, engine/player-movement.md): the game ignores tilts within ±10 and caps the stick at 56
 * (C-Stick 44); walking speed is the tilt / 28. A modern stick read raw reaches 127, so it ran at a smaller push than
 * a real controller; the keyboard (always full tilt) could not walk at all without the walk key.
 *
 * THE MENU SHORTCUT: one or two buttons (or keys). A member that is also a GameCube button (Z, L, START...) is held
 * back from the game for up to kHold reads when it goes down alone, waiting for the other: the generalised form of the
 * Z + START hold of 2026-11-09 (the game's scripts count Z as "any button"; a held L or R switches PC boxes,
 * menus.md). Completed: the menu opens and the game sees neither member until both are let go. Not completed: the
 * button reaches the game late, or as a one-read tap if let go sooner. A held-back L or R also has its analog value
 * held back. A single member the game doesn't use (Esc, View) is opened by the press event instead
 * (pc_settings_overlay.cpp), as before.
 *
 * Runs only when no pad script drives the game (replays and automated runs feed what the game was given). Port code
 * over Aurora's pad API, no game code: XD uses the same GameCube pad, so it carries over (likely).
 */
#include "pc_pad_shape.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring> /* std::strcmp: the fast-forward button's "none" */
#include <string>

#include "pc_settings.h"

extern "C" {
int pc_option_gamecube_accurate(void); /* pc_options.c */
int pc_option_accurate_locked(void);   /* pc_options.c: the mode is fixed (a game has started) */
const char* pc_controls_native_label(int native); /* pc_controls_ui.cpp: a controller button as printed on it */
const char* pc_controls_key_label(int scancode);
}

namespace {

constexpr int kHold = 4;           /* reads a lone member is held back, waiting for the other (as the Z hold) */
constexpr float kSnapDeg = 12.0f;  /* within this of one of the 8 directions, snap to it */
constexpr float kPi = 3.14159265358979f;

const char* key_of(int stick, const char* what) {
    static std::string k;
    k = std::string("stick") + (stick == PC_STICK_C ? "1_" : "0_") + what;
    return k.c_str();
}

/* the calibration: PC_CAL_SAMPLES radii around the circle (thousandths in settings.ini) and the centre */
struct Cal {
    bool loaded = false;
    bool has = false;
    float r[PC_CAL_SAMPLES] = {};
    float cx = 0, cy = 0;
};
Cal s_cal[2];

Cal& cal(int stick) {
    Cal& c = s_cal[stick == PC_STICK_C ? 1 : 0];
    if (!c.loaded) {
        c.loaded = true;
        const char* s = pc_settings_string(key_of(stick, "cal"), "");
        int n = 0;
        for (const char* p = s; *p != '\0' && n < PC_CAL_SAMPLES;) {
            char* end = nullptr;
            const long v = std::strtol(p, &end, 10);
            if (end == p) break;
            c.r[n++] = SDL_clamp(v / 1000.0f, 0.3f, 1.5f);
            p = *end == ',' ? end + 1 : end;
        }
        c.has = n == PC_CAL_SAMPLES;
        c.cx = pc_settings_int(key_of(stick, "cx"), 0) / 1000.0f;
        c.cy = pc_settings_int(key_of(stick, "cy"), 0) / 1000.0f;
    }
    return c;
}

/* how far the stick reaches at angle a (radians), from the calibration; 1 without one */
float reach(int stick, float a) {
    const Cal& c = cal(stick);
    if (!c.has) return 1.0f;
    float pos = a / (2 * kPi) * PC_CAL_SAMPLES;
    pos = std::fmod(pos + PC_CAL_SAMPLES, (float)PC_CAL_SAMPLES);
    const int i0 = (int)pos;
    const float f = pos - i0;
    return c.r[i0 % PC_CAL_SAMPLES] * (1 - f) + c.r[(i0 + 1) % PC_CAL_SAMPLES] * f;
}

/* the GameCube's octagonal gate: radius 1 at its 8 corners (the 8 directions), less between them */
float gate(float a) {
    const float step = kPi / 4;
    float m = std::fmod(a, step);
    if (m < 0) m += step;
    return std::cos(step / 2) / std::cos(m - step / 2);
}

float snap(float a) {
    const float step = kPi / 4;
    const float k = std::round(a / step) * step;
    return std::fabs(a - k) < kSnapDeg * kPi / 180 ? k : a;
}

/* ---- the shortcut ---- */
std::string encode(const PcShortcutMember* m, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) {
        if (i > 0) s += "+";
        s += (m[i].type == PC_SC_NATIVE ? "n" : m[i].type == PC_SC_GC ? "g" : "k") + std::to_string(m[i].value);
    }
    return s;
}

int decode(const char* s, PcShortcutMember out[2]) {
    int n = 0;
    for (const char* p = s; *p != '\0' && n < 2;) {
        const char t = *p++;
        char* end = nullptr;
        const long v = std::strtol(p, &end, 10);
        if (end == p) break;
        out[n].type = t == 'n' ? PC_SC_NATIVE : t == 'g' ? PC_SC_GC : PC_SC_KEY;
        out[n].value = (int)v;
        ++n;
        p = *end == '+' ? end + 1 : end;
    }
    return n;
}

SDL_Gamepad* p1_gamepad() {
    const s32 index = PADGetIndexForPort(0);
    return index >= 0 ? PADGetSDLGamepadForIndex((u32)index) : nullptr;
}

/* the controller button that presses a GameCube button (PAD_NATIVE_BUTTON_INVALID: none, or L / R's analog trigger) */
u32 native_for(u32 pad) {
    u32 n = 0;
    PADButtonMapping* m = PADGetButtonMappings(0, &n);
    for (u32 i = 0; m != nullptr && i < n; ++i) {
        if (m[i].padButton == pad) return m[i].nativeButton;
    }
    return PAD_NATIVE_BUTTON_INVALID;
}

/* the GameCube button a controller button or a key presses (0: none) */
u32 gc_of(const PcShortcutMember& m) {
    if (m.type == PC_SC_GC) return (u32)m.value;
    u32 n = 0;
    if (m.type == PC_SC_NATIVE) {
        PADButtonMapping* b = PADGetButtonMappings(0, &n);
        for (u32 i = 0; b != nullptr && i < n; ++i) {
            if (b[i].nativeButton == (u32)m.value) return b[i].padButton;
        }
    } else {
        PADKeyButtonBinding* k = PADGetKeyButtonBindings(0, &n);
        for (u32 i = 0; k != nullptr && i < n; ++i) {
            if (k[i].scancode == m.value) return k[i].padButton;
        }
    }
    return 0;
}

/* held on the device itself (not what the game was given) */
bool held_now(const PcShortcutMember& m) {
    if (m.type == PC_SC_KEY) {
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        return keys != nullptr && m.value >= 0 && m.value < count && keys[m.value];
    }
    SDL_Gamepad* gp = p1_gamepad();
    if (gp == nullptr) return false;
    if (m.type == PC_SC_NATIVE) return SDL_GetGamepadButton(gp, (SDL_GamepadButton)m.value);
    const u32 native = native_for((u32)m.value);
    if (native != PAD_NATIVE_BUTTON_INVALID) return SDL_GetGamepadButton(gp, (SDL_GamepadButton)native);
    if (m.value == PAD_TRIGGER_L || m.value == PAD_TRIGGER_R) {
        return SDL_GetGamepadAxis(gp, m.value == PAD_TRIGGER_L ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                                                               : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 22000;
    }
    return false;
}

const char* gc_name(u32 mask) {
    switch (mask) {
    case PAD_TRIGGER_Z: return "Z";
    case PAD_TRIGGER_L: return "L";
    case PAD_TRIGGER_R: return "R";
    case PAD_BUTTON_START: return "START";
    case PAD_BUTTON_A: return "A";
    case PAD_BUTTON_B: return "B";
    case PAD_BUTTON_X: return "X";
    case PAD_BUTTON_Y: return "Y";
    case PAD_BUTTON_UP: return "D-pad &#8593;";
    case PAD_BUTTON_DOWN: return "D-pad &#8595;";
    case PAD_BUTTON_LEFT: return "D-pad &#8592;";
    case PAD_BUTTON_RIGHT: return "D-pad &#8594;";
    default: return "?";
    }
}

/* one combination's hold-back state (the controller's and the keyboard's, each its own) */
struct Hold {
    int holding = -1;  /* the member held back, waiting for the other; -1 none */
    int reads = 0;
    bool swallow = false; /* completed: both kept from the game until both are let go */
    bool was[2] = {false, false};
};
Hold s_hold[2];
Hold s_hold_quit; /* the Quit to hub shortcut's (a controller's only) */

void hide(PADStatus& st, u32 mask) {
    st.button &= (u16)~mask;
    if (mask & PAD_TRIGGER_L) st.triggerLeft = 0;
    if (mask & PAD_TRIGGER_R) st.triggerRight = 0;
}

/* one combination over this read; true when it was completed. native_edge: a single button the game never sees counts
 * on its press here (the menu shortcut's is opened by the press event instead, pc_shortcut_by_event) */
bool run_members(const PcShortcutMember* m, int n, Hold& h, PADStatus& st, bool native_edge) {
    if (n <= 0) return false;
    u32 mask[2] = {0, 0};
    bool held[2] = {false, false};
    for (int i = 0; i < n; ++i) {
        mask[i] = gc_of(m[i]);
        /* a GameCube button counts as held when the game was about to get it (from either device); else the device */
        held[i] = mask[i] != 0 && m[i].type == PC_SC_GC ? (st.button & mask[i]) != 0 : held_now(m[i]);
    }
    const bool down[2] = {held[0] && !h.was[0], held[1] && !h.was[1]};
    h.was[0] = held[0];
    h.was[1] = held[1];
    if (n == 1 && mask[0] == 0) return native_edge && down[0]; /* else the press event opens it (pc_shortcut_by_event) */
    if (h.swallow) {
        hide(st, mask[0] | mask[1]);
        h.swallow = held[0] || held[1];
        return false;
    }
    if (n == 1) {
        if (down[0]) {
            h.swallow = true;
            hide(st, mask[0]);
            return true;
        }
        return false;
    }
    if (h.holding >= 0) {
        const int a = h.holding, b = 1 - a;
        if (held[b]) { /* completed */
            h.holding = -1;
            h.swallow = true;
            hide(st, mask[0] | mask[1]);
            return true;
        }
        if (!held[a]) { /* let go early: the game gets it now, as a one-read tap */
            h.holding = -1;
            st.button |= (u16)mask[a];
        } else if (++h.reads > kHold) { /* the other never came: it reaches the game, late */
            h.holding = -1;
        } else {
            hide(st, mask[a]);
        }
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        if (!down[i]) continue;
        if (held[1 - i]) { /* the other already held (or both this read) */
            h.swallow = true;
            hide(st, mask[0] | mask[1]);
            return true;
        }
        if (mask[i] != 0) {
            h.holding = i;
            h.reads = 1;
            hide(st, mask[i]);
        }
        return false;
    }
    return false;
}

/* the menu shortcut of the controller (keyboard 0) or the keyboard (1) */
bool run_combo(int keyboard, PADStatus& st) {
    PcShortcutMember m[2];
    const int n = pc_shortcut_get(keyboard, m);
    return run_members(m, n, s_hold[keyboard ? 1 : 0], st, false);
}

bool walk_held() {
    const int key = pc_stick_walk_key();
    if (key >= 0) {
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        if (keys != nullptr && key < count && keys[key]) return true;
    }
    const int b = pc_stick_walk_button();
    SDL_Gamepad* gp = b >= 0 ? p1_gamepad() : nullptr;
    return gp != nullptr && SDL_GetGamepadButton(gp, (SDL_GamepadButton)b);
}

/* THE TRIGGERS, the way Dolphin does it (its MixedTriggers: the analog squeeze and the digital click kept apart; its
 * default threshold 90 %, dead zone 0; snatcherLeo, 2026-11-12: "Yes, add them"): for a controller's analog trigger (one
 * not mapped to a button, and not pressed on the keyboard), the game gets the squeeze as it is and the click only past
 * 90 % of the travel, and a click reads as fully pressed. Aurora alone made the click from its own activation zone
 * and set the analog to a fixed value when clicked. Written from scratch; no Dolphin code. */
constexpr float kTriggerClick = PC_TRIGGER_CLICK_PCT / 100.0f;

void shape_triggers(PADStatus& st) {
    SDL_Gamepad* gp = p1_gamepad();
    if (gp == nullptr) return;
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    for (int side = 0; side < 2; ++side) {
        const u32 pad = side == 0 ? PAD_TRIGGER_L : PAD_TRIGGER_R;
        if (native_for(pad) != PAD_NATIVE_BUTTON_INVALID) continue; /* a button gives the click: Aurora's as is */
        u32 n = 0;
        bool key_held = false;
        PADKeyButtonBinding* k = PADGetKeyButtonBindings(0, &n);
        for (u32 i = 0; k != nullptr && keys != nullptr && i < n; ++i) {
            if (k[i].padButton == pad && k[i].scancode >= 0 && k[i].scancode < count && keys[k[i].scancode]) key_held = true;
        }
        if (key_held) continue; /* the keyboard drives it */
        const float a = SDL_clamp(SDL_GetGamepadAxis(gp, side == 0 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                                                                   : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f,
                                  0.0f, 1.0f);
        const bool click = a >= kTriggerClick;
        const u8 analog = click ? 255 : (u8)std::lround(a * 255);
        if (side == 0) st.triggerLeft = analog;
        else st.triggerRight = analog;
        if (click) st.button |= (u16)pad;
        else st.button &= (u16)~pad;
    }
}

/* a stick key held on the keyboard (then its values are the keyboard's, shaped as a non-GameCube device) */
bool stick_keys_held(int stick) {
    u32 n = 0;
    PADKeyAxisBinding* k = PADGetKeyAxisBindings(0, &n);
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    for (u32 i = 0; k != nullptr && keys != nullptr && i < n; ++i) {
        const bool main = k[i].padAxis == PAD_AXIS_LEFT_X_POS || k[i].padAxis == PAD_AXIS_LEFT_X_NEG ||
                          k[i].padAxis == PAD_AXIS_LEFT_Y_POS || k[i].padAxis == PAD_AXIS_LEFT_Y_NEG;
        const bool c = k[i].padAxis == PAD_AXIS_RIGHT_X_POS || k[i].padAxis == PAD_AXIS_RIGHT_X_NEG ||
                       k[i].padAxis == PAD_AXIS_RIGHT_Y_POS || k[i].padAxis == PAD_AXIS_RIGHT_Y_NEG;
        if ((stick == PC_STICK_MAIN ? main : c) && k[i].scancode >= 0 && k[i].scancode < count && keys[k[i].scancode]) {
            return true;
        }
    }
    return false;
}

} // namespace

/* ---- the sticks' settings (settings.ini) ---- */
extern "C" int pc_stick_range_default(int stick) { return stick == PC_STICK_C ? 92 : 101; }
/* GameCube-accurate mode's stick rules apply while a game runs in it (2.12, 2026-11-16): accurate mode is each save's
 * own setting and fixed when the game starts (pc_option_accurate_lock), so in the hub (before that) the Controls screen
 * reads and writes the player's own range and dead zone; before 2.12 a program-wide accurate preset clamped the stored
 * dead zone to 10 even for normal play. The game itself only reads the stick after the lock. All platforms. */
static bool accurate_in_game() { return pc_option_accurate_locked() != 0 && pc_option_gamecube_accurate() != 0; }
extern "C" int pc_stick_range(int stick) {
    if (accurate_in_game()) return pc_stick_range_default(stick);
    return SDL_clamp(pc_settings_int(key_of(stick, "range"), pc_stick_range_default(stick)), 60, 127);
}
extern "C" void pc_stick_set_range(int stick, int units) {
    pc_settings_set_int(key_of(stick, "range"), SDL_clamp(units, 60, 127));
}
extern "C" int pc_stick_dead_limit(void) { return accurate_in_game() ? 10 : 40; }
extern "C" int pc_stick_dead(int stick) {
    return SDL_clamp(pc_settings_int(key_of(stick, "dead"), 8), 0, pc_stick_dead_limit());
}
extern "C" void pc_stick_set_dead(int stick, int units) {
    /* stored up to the normal mode's 40 in every mode: the choice is the normal mode's; an accurate game reads it
     * capped at 10 (pc_stick_dead) without changing it */
    pc_settings_set_int(key_of(stick, "dead"), SDL_clamp(units, 0, 40));
}
extern "C" int pc_stick_snap(void) { return pc_settings_int("stick_snap", 0) != 0 ? 1 : 0; }
extern "C" void pc_stick_set_snap(int on) { pc_settings_set_int("stick_snap", on ? 1 : 0); }
extern "C" int pc_stick_walk_button(void) { return pc_settings_int("walk_button", -1); }
extern "C" int pc_stick_walk_key(void) { return pc_settings_int("walk_key", SDL_SCANCODE_LCTRL); }
extern "C" int pc_stick_walk_pct(void) { return SDL_clamp(pc_settings_int("walk_pct", 50), 10, 90); }
/* the walk button or key held right now (2026-11-20, step 3.7's right-stick walking, pc_cstick_walk.cpp: the C-stick
 * isn't reshaped by the walk button here, so the walking hook applies the percentage itself). Reads the device state,
 * so the same on every OS SDL runs on */
extern "C" int pc_stick_walk_held(void) { return walk_held() ? 1 : 0; }
extern "C" void pc_stick_set_walk_button(int native) { pc_settings_set_int("walk_button", native); }
extern "C" void pc_stick_set_walk_key(int scancode) { pc_settings_set_int("walk_key", scancode); }
extern "C" void pc_stick_set_walk_pct(int pct) { pc_settings_set_int("walk_pct", SDL_clamp(pct, 10, 90)); }
extern "C" int pc_stick_has_calibration(int stick) { return cal(stick).has ? 1 : 0; }
extern "C" int pc_stick_get_calibration(int stick, float* radii) {
    const Cal& c = cal(stick);
    for (int i = 0; i < PC_CAL_SAMPLES; ++i) radii[i] = c.has ? c.r[i] : 1.0f;
    return c.has ? 1 : 0;
}

extern "C" void pc_stick_set_calibration(int stick, const float* radii, float cx, float cy) {
    std::string s;
    for (int i = 0; i < PC_CAL_SAMPLES; ++i) {
        if (i > 0) s += ",";
        s += std::to_string((int)std::lround(SDL_clamp(radii[i], 0.3f, 1.5f) * 1000));
    }
    pc_settings_set_string(key_of(stick, "cal"), s.c_str());
    pc_settings_set_int(key_of(stick, "cx"), (int)std::lround(cx * 1000));
    pc_settings_set_int(key_of(stick, "cy"), (int)std::lround(cy * 1000));
    s_cal[stick == PC_STICK_C ? 1 : 0].loaded = false;
}

extern "C" void pc_stick_clear_calibration(int stick) {
    pc_settings_set_string(key_of(stick, "cal"), "");
    pc_settings_set_int(key_of(stick, "cx"), 0);
    pc_settings_set_int(key_of(stick, "cy"), 0);
    s_cal[stick == PC_STICK_C ? 1 : 0].loaded = false;
}

extern "C" int pc_stick_player1_is_gamecube(void) {
    if (PADIsGCAdapter(0)) return 1;
    SDL_Gamepad* gp = p1_gamepad();
    const char* name = gp != nullptr ? SDL_GetGamepadName(gp) : nullptr;
    return name != nullptr && SDL_strcasestr(name, "gamecube") != nullptr ? 1 : 0;
}

extern "C" void pc_stick_shape(int stick, float x, float y, int walk, int gamecube, int* out_x, int* out_y) {
    const bool accurate = pc_option_gamecube_accurate() != 0;
    const float dead = (float)pc_stick_dead(stick);
    const float walk_k = walk ? pc_stick_walk_pct() / 100.0f : 1.0f;
    float u = 0, a = 0;
    if (gamecube) {
        /* a real GameCube controller: its values are already the console's (127 per unit of x) */
        float vx = x * 127, vy = y * 127;
        u = std::sqrt(vx * vx + vy * vy);
        a = std::atan2(vy, vx);
        if (!accurate && pc_stick_snap()) a = snap(a);
        u *= walk_k;
        if (accurate) {
            u = u < dead ? 0 : u;
        } else {
            const float full = (float)pc_stick_range_default(stick);
            u = u <= dead ? 0 : (u - dead) * full / SDL_max(full - dead, 1.0f);
        }
    } else {
        const Cal& c = cal(stick);
        const float vx = x - c.cx, vy = y - c.cy;
        a = std::atan2(vy, vx);
        float d = SDL_min(std::sqrt(vx * vx + vy * vy) / reach(stick, a), 1.0f);
        if (pc_stick_snap() && d > 0.5f) a = snap(a);
        d *= walk_k;
        const float g = gate(a) * pc_stick_range(stick);
        if (accurate) {
            u = d * g;
            u = u < dead ? 0 : u;
        } else {
            const float dz = dead / g;
            u = d <= dz ? 0 : (d - dz) / (1 - dz) * g;
        }
    }
    *out_x = (int)SDL_clamp(std::lround(u * std::cos(a)), -127L, 127L);
    *out_y = (int)SDL_clamp(std::lround(u * std::sin(a)), -127L, 127L);
}

/* ---- the shortcut's settings (settings.ini menu_pad / menu_keys: "n<button>", "g<mask>", "k<scancode>", '+') ---- */
extern "C" int pc_shortcut_is_default(int keyboard) {
    return *pc_settings_string(keyboard ? "menu_keys" : "menu_pad", "") == '\0' ? 1 : 0;
}

extern "C" int pc_shortcut_get(int keyboard, PcShortcutMember out[2]) {
    int n = decode(pc_settings_string(keyboard ? "menu_keys" : "menu_pad", ""), out);
    if (n > 0) return n;
    if (keyboard) { /* Esc (or the single key chosen before combinations, menu_key) */
        out[0] = {PC_SC_KEY, pc_settings_int("menu_key", SDL_SCANCODE_ESCAPE)};
        return 1;
    }
    if (pc_stick_player1_is_gamecube()) { /* snatcherLeo, 2026-11-12: "Z+L" on a GameCube controller */
        out[0] = {PC_SC_GC, PAD_TRIGGER_Z};
        out[1] = {PC_SC_GC, PAD_TRIGGER_L};
        return 2;
    }
    const int b = pc_settings_int("menu_button", -1); /* the single button chosen before combinations */
    out[0] = {PC_SC_NATIVE, b >= 0 ? b : (int)SDL_GAMEPAD_BUTTON_BACK}; /* View / Create / Back */
    return 1;
}

extern "C" void pc_shortcut_set(int keyboard, const PcShortcutMember* members, int count) {
    pc_settings_set_string(keyboard ? "menu_keys" : "menu_pad", count > 0 ? encode(members, SDL_min(count, 2)).c_str() : "");
    if (count <= 0) pc_settings_set_int(keyboard ? "menu_key" : "menu_button", keyboard ? SDL_SCANCODE_ESCAPE : -1);
}

extern "C" const char* pc_shortcut_name(int keyboard) {
    static std::string name[2];
    PcShortcutMember m[2];
    const int n = pc_shortcut_get(keyboard, m);
    std::string& s = name[keyboard ? 1 : 0];
    s.clear();
    for (int i = 0; i < n; ++i) {
        if (i > 0) s += " + ";
        s += m[i].type == PC_SC_KEY ? pc_controls_key_label(m[i].value)
             : m[i].type == PC_SC_GC ? gc_name((u32)m[i].value)
                                     : pc_controls_native_label(m[i].value);
    }
    return s.c_str();
}

/* ---- the Quit to hub shortcut (snatcherLeo, 2026-11-13: "we should also allow users to set a shortcut in the controller to
 * "Quit to hub". by default, it will never be set"): a controller's one or two buttons, settings.ini quit_pad in the
 * menu shortcut's form; none by default ---- */
extern "C" int pc_quit_shortcut_get(PcShortcutMember out[2]) { return decode(pc_settings_string("quit_pad", ""), out); }

extern "C" void pc_quit_shortcut_set(const PcShortcutMember* members, int count) {
    pc_settings_set_string("quit_pad", count > 0 ? encode(members, SDL_min(count, 2)).c_str() : "");
}

extern "C" const char* pc_quit_shortcut_name(void) {
    static std::string s;
    PcShortcutMember m[2];
    const int n = pc_quit_shortcut_get(m);
    s = n == 0 ? "None" : "";
    for (int i = 0; i < n; ++i) {
        if (i > 0) s += " + ";
        s += m[i].type == PC_SC_GC ? gc_name((u32)m[i].value) : pc_controls_native_label(m[i].value);
    }
    return s.c_str();
}

extern "C" int pc_shortcut_held(int keyboard) {
    PcShortcutMember m[2];
    const int n = pc_shortcut_get(keyboard, m);
    for (int i = 0; i < n; ++i) {
        if (!held_now(m[i])) return 0;
    }
    return n > 0 ? 1 : 0;
}

extern "C" int pc_shortcut_by_event(int keyboard) {
    PcShortcutMember m[2];
    return pc_shortcut_get(keyboard, m) == 1 && gc_of(m[0]) == 0 ? 1 : 0;
}

/* ---- the fast-forward button (snatcherLeo, 2026-11-16: the Speed settings, "Fine, LB works"; pc_pad_shape.h) ----
 * Why one member, not a combination like the menu's: it is held for long stretches (Hold) or tapped (Press to
 * switch), and a two-button hold would hold back a game button every time. Why LB / Tab: the game never reads them
 * (Aurora's defaults map RB to Z and LT / RT to L / R; pc_controls.c leaves Tab unbound), so holding them needs no
 * hold-back at all; OrrePort's own menus use LB (the previous category), but fast-forward is ignored while a menu is
 * open (pc_speed.c). The rule for clashes (snatcherLeo's question, 2026-11-16): never swap silently.
 *   - The default (settings "" ) counts only while its button does nothing else (pc_ff_get checks every time), and the
 *     first start of this build writes "none" when it is taken (pc_ff_first_start_check, logged): the existing mapping
 *     keeps its job.
 *   - A button the player picks although it is a game button: Controls asks first ("Use it anyway"); then the read
 *     filter below hides that game button while it is held, so the game never sees the fast-forward button.
 * Platforms: SDL's gamepad and keyboard state, the same on Windows, Linux and macOS. */
namespace {
const char* ff_key(int keyboard) { return keyboard ? "ff_keys" : "ff_pad"; }

bool ff_default(int keyboard, PcShortcutMember& out) {
    if (keyboard) {
        out = {PC_SC_KEY, SDL_SCANCODE_TAB};
        return true;
    }
    if (pc_stick_player1_is_gamecube()) return false; /* a GameCube pad has no LB, and its L is the game's */
    out = {PC_SC_NATIVE, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER};
    return true;
}

bool same(const PcShortcutMember& a, const PcShortcutMember& b) { return a.type == b.type && a.value == b.value; }
} // namespace

extern "C" int pc_ff_conflict(int keyboard, PcShortcutMember m, unsigned* gc) {
    u32 n = 0;
    if (gc != nullptr) *gc = 0;
    /* OrrePort's own: the menu and Quit to hub shortcuts (any member), the walk button or key, F2 / F8 / F11 */
    PcShortcutMember sc[2];
    for (int which = 0; which < 3; ++which) {
        const int count = which == 2 ? pc_quit_shortcut_get(sc) : pc_shortcut_get(which, sc);
        for (int i = 0; i < count; ++i) {
            if (same(sc[i], m)) return PC_FF_ORREPORT;
            /* a GameCube member pressed through this controller button counts too */
            if (sc[i].type == PC_SC_GC && m.type == PC_SC_NATIVE && native_for((u32)sc[i].value) == (u32)m.value) {
                return PC_FF_ORREPORT;
            }
        }
    }
    if (m.type == PC_SC_KEY) {
        if (m.value == pc_stick_walk_key() || m.value == SDL_SCANCODE_F2 || m.value == SDL_SCANCODE_F8 ||
            m.value == SDL_SCANCODE_F11) {
            return PC_FF_ORREPORT;
        }
        PADKeyButtonBinding* k = PADGetKeyButtonBindings(0, &n);
        for (u32 i = 0; k != nullptr && i < n; ++i) {
            if (k[i].scancode == m.value) {
                if (gc != nullptr) *gc = k[i].padButton;
                return PC_FF_GAME;
            }
        }
        PADKeyAxisBinding* ka = PADGetKeyAxisBindings(0, &n);
        for (u32 i = 0; ka != nullptr && i < n; ++i) {
            if (ka[i].scancode == m.value) return PC_FF_STICK;
        }
        return PC_FF_FREE;
    }
    if (m.type == PC_SC_NATIVE) {
        if (m.value == pc_stick_walk_button()) return PC_FF_ORREPORT;
        PADButtonMapping* b = PADGetButtonMappings(0, &n);
        for (u32 i = 0; b != nullptr && i < n; ++i) {
            if (b[i].nativeButton == (u32)m.value) {
                if (gc != nullptr) *gc = b[i].padButton;
                return PC_FF_GAME;
            }
        }
        PADAxisMapping* a = PADGetAxisMappings(0, &n);
        for (u32 i = 0; a != nullptr && i < n; ++i) {
            if (a[i].nativeButton == m.value) return PC_FF_STICK;
        }
    }
    return PC_FF_FREE;
}

extern "C" int pc_ff_get(int keyboard, PcShortcutMember* out) {
    const char* s = pc_settings_string(ff_key(keyboard), "");
    if (std::strcmp(s, "none") == 0) return 0;
    PcShortcutMember m[2];
    if (decode(s, m) >= 1) {
        *out = m[0];
        return 1;
    }
    if (!ff_default(keyboard, *out)) return 0;
    /* the default only while its button is free (a mapping may have taken it since the first-start check) */
    return pc_ff_conflict(keyboard, *out, nullptr) == PC_FF_FREE ? 1 : 0;
}

extern "C" void pc_ff_set(int keyboard, const PcShortcutMember* member) {
    pc_settings_set_string(ff_key(keyboard), member != nullptr ? encode(member, 1).c_str() : "none");
}

extern "C" void pc_ff_reset(int keyboard) { pc_settings_set_string(ff_key(keyboard), ""); }

extern "C" const char* pc_ff_member_name(const PcShortcutMember* m) {
    return m->type == PC_SC_KEY ? pc_controls_key_label(m->value)
           : m->type == PC_SC_GC ? gc_name((u32)m->value)
                                 : pc_controls_native_label(m->value);
}

extern "C" const char* pc_ff_name(int keyboard) {
    static std::string s[2];
    PcShortcutMember m;
    s[keyboard ? 1 : 0] = pc_ff_get(keyboard, &m) ? pc_ff_member_name(&m) : "";
    return s[keyboard ? 1 : 0].c_str();
}

extern "C" const char* pc_ff_gc_name(unsigned gc) { return gc_name(gc); }

extern "C" int pc_ff_held(void) {
    PcShortcutMember m;
    for (int kb = 0; kb < 2; ++kb) {
        if (pc_ff_get(kb, &m) && held_now(m)) return 1;
    }
    return 0;
}

/* once per device, the first time this build runs with it (settings.ini ff_checked: 1 the keyboard, 2 a controller
 * that isn't a GameCube one): a default button that something already uses starts as "not set", so the player's
 * mapping keeps its job (snatcherLeo's rule, 2026-11-16); a button the player set already is left alone. Game thread, each
 * frame until both are done (cheap after). */
extern "C" void pc_ff_first_start_check(void) {
    static bool done = false;
    if (done) return;
    int checked = pc_settings_int("ff_checked", 0);
    const int before = checked;
    for (int kb = 1; kb >= 0; --kb) {
        const int bit = kb ? 1 : 2;
        if (checked & bit) continue;
        if (!kb && (p1_gamepad() == nullptr || pc_stick_player1_is_gamecube())) continue; /* until such a pad is used */
        checked |= bit;
        if (*pc_settings_string(ff_key(kb), "") != '\0') continue; /* chosen by the player already */
        PcShortcutMember m;
        if (!ff_default(kb, m)) continue;
        unsigned gc = 0;
        const int clash = pc_ff_conflict(kb, m, &gc);
        if (clash == PC_FF_FREE) {
            std::printf("[speed] fast-forward: %s on the %s (the default; it does nothing else)\n", pc_ff_member_name(&m),
                        kb ? "keyboard" : "controller");
        } else {
            pc_ff_set(kb, nullptr);
            std::printf("[speed] fast-forward: %s on the %s is already %s, so Fast-forward starts with no button there "
                        "(that mapping keeps its job; Controls › Fast-forward sets one)\n",
                        pc_ff_member_name(&m), kb ? "keyboard" : "controller",
                        clash == PC_FF_GAME    ? gc_name(gc)
                        : clash == PC_FF_STICK ? "a stick direction"
                                               : "an OrrePort shortcut");
        }
    }
    if (checked != before) {
        pc_settings_set_int("ff_checked", checked);
        pc_settings_save();
    }
    done = (checked & 3) == 3;
}

/* ---- the filter ---- */
extern "C" int pc_pad_filter_player1(PADStatus* status) {
    PADStatus& st = status[0];
    if (st.err != PAD_ERR_NONE) return 0;
    /* Aurora's own dead zone off: the dead zone is applied here, the Dolphin way */
    if (PADDeadZones* d = PADGetDeadZones(0); d != nullptr && d->useDeadzones) {
        d->useDeadzones = false;
    }
    shape_triggers(st); /* first: the shortcut's hold-back below may then hide a trigger */
    const bool open = run_combo(0, st) | run_combo(1, st);
    PcShortcutMember quit_m[2];
    const int quit_n = pc_quit_shortcut_get(quit_m);
    const bool quit = run_members(quit_m, quit_n, s_hold_quit, st, true);
    /* the fast-forward button never reaches the game: a game button the player chose for it anyway ("Use it anyway" in
     * Controls) is hidden while held (the default LB / Tab presses no game button, so nothing to hide) */
    for (int kb = 0; kb < 2; ++kb) {
        PcShortcutMember ff;
        if (pc_ff_get(kb, &ff)) {
            const u32 mask = gc_of(ff);
            if (mask != 0 && held_now(ff)) hide(st, mask);
        }
    }
    /* The "real GameCube values" path is for a direct adapter reader (the one Chase's port uses, planned): SDL's own
     * GameCube driver rescales each stick to its full range from the extremes it has seen (likely, from SDL's
     * source; to check with snatcherLeo's adapter), so through SDL a GameCube controller is shaped like any other pad. */
    const bool gc = false;
    const int walk = walk_held() ? 1 : 0;
    int x = 0, y = 0;
    pc_stick_shape(PC_STICK_MAIN, st.stickX / 127.0f, st.stickY / 127.0f, walk, gc && !stick_keys_held(PC_STICK_MAIN), &x, &y);
    st.stickX = (s8)x;
    st.stickY = (s8)y;
    pc_stick_shape(PC_STICK_C, st.substickX / 127.0f, st.substickY / 127.0f, 0, gc && !stick_keys_held(PC_STICK_C), &x, &y);
    st.substickX = (s8)x;
    st.substickY = (s8)y;
    return (open ? PC_FILTER_OPEN_MENU : 0) | (quit ? PC_FILTER_QUIT_TO_HUB : 0);
}
