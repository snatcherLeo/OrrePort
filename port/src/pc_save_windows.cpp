/*
 * pc_save_windows.cpp: PC MENU saves made in the middle of a story scene (the softlock fix; 2026-11-20, the project's helper;
 * snatcherLeo approved "Build the refined softlock fix: an unsafe-window table with rollbacks ..., rolled-back saves that stay
 * GameCube-valid, and a 20-second safety net"). How it works, in full: docs/port/pc-menu-anywhere.md, "Saves made in
 * the middle of a scene"; the cause: the project notes
 *
 * Why it exists: PC MENU (pc_field_menu.cpp) lets the player save anywhere. On the GameCube the story only saves at a
 * PC, where the room's setup rebuilds everything from the story flags. Saved between two parts of a scene, a save can
 * hold flags whose room state only the scene itself builds. snatcherLeo's case (verified): Phenac's plaza after the Folly
 * battle, flag 993 set (the sack's scene ended) and 996 clear (Rui not untied): the room's setup has no branch for it,
 * hides the sack, and the woman's touch event waits forever, so Continue hangs. The save holds no people, positions or
 * running scripts, so nothing but its flags can be fixed.
 *
 * Two parts:
 *   1. The table (ui/game/save_windows.ini, read once): known open windows, each a room, a flag condition and a
 *      rollback. When the PC's Save runs from PC MENU's PC screen and a row matches the room PC MENU was opened in:
 *        - step 0 (OrreHook_PcSaveStep, just before the game's "Would you like to save your progress?"): the game's
 *          message window shows game.save_window.notice ("This scene will start again when you continue this save.")
 *          with the wait-for-A arrow, so the player knows before answering; A closes it and the question follows;
 *        - step 2 (just before the card write, memory-card task 4): the row's flags are set to their rollback values
 *          in the live save image with the game's own setter, so the card task writes them in the card's byte order
 *          like any save (the image in memory is little-endian on the PC, the .gci big-endian: verified 2026-11-20, so
 *          a .gci can't be patched from memory directly);
 *        - step 3 (the write returned): the flags are put back to what they were, so the game in memory goes on
 *          exactly as before; nothing else in the image was touched. The row's spot rule then keeps or drops the PC
 *          MENU spot line (pc_field_menu.cpp wrote it inside the card task).
 *      The written .gci is a state the game itself was in a moment earlier (Phenac: 993 clear with 992 / 994 / 995
 *      set, the after-battle part of the scene not yet finished), which the room's setup handles: valid on a GameCube
 *      and in Dolphin too (their Continue goes to the last PC; walking into the plaza replays the scene). So the
 *      GameCube copy's checklist needs no rule: the .gci is the game's own kind of state.
 *   2. The hang watch (the safety net for windows the table doesn't know yet): after a Continue from a PC MENU spot
 *      (OrreHook_ContinueSpot, chained), while the player is still in that room, if a talk / touch script run by the
 *      hero step (OrreHook_HeroEventScript) holds Wes (his people flag 0x80000008) with nothing on screen to answer (no
 *      message window, no window reading buttons) for 30 seconds (20 until 2026-11-20, when the save-check sweep found
 *      The Under's subway ride holding the player ~22.2 s with nothing to answer), the game's message window asks
 *      game.save_window.stuck_question ("The game seems to be stuck. Go back to your previous save?"). YES puts the
 *      slot's previous version back (Backups, the same Restore the Saves page offers, with Undo) and goes to the main
 *      menu the way the PC screen's Main Menu does (pc_pc_screen.cpp), which re-reads the slot; NO leaves things. Every
 *      time the watch would fire it logs, also when it doesn't offer (not after a PC MENU Continue), so test runs
 *      show it never fires in normal play. ORRE_STUCK_WATCH=1 turns the logging on in every mode (observation only:
 *      reads flags and windows, no random draws); ORRE_STUCK_SECONDS=<n> changes the 30 s for a test.
 *
 * The switch: built in, the same as PC MENU (enabled(): off in GameCube-accurate mode, where PC MENU itself is off,
 * and in automated runs unless ORRE_FIELD_PC_MENU=1). Off, every hook here only passes on to the one before it.
 * Random numbers: none drawn (likely: flag writes, the game's message window and YES / NO box, file copies).
 *
 * Platforms: all the same (plain C++ over the game's own functions and pc_saves' file calls; no OS calls). Linux and
 * macOS need nothing different (likely).
 * For XD: XD's rooms have the same setup / main loop / scene structure (likely, the shared script engine), so the same
 * kind of window exists; the table is per game (XD's own rooms and flags), the hooks are the same concepts (the PC's
 * save steps, the hero step's event scripts), and XD's save writes through its own card task the same way (likely).
 */
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_autosave.h" /* the autosave, the hang watch's other way back (2026-11-20) */
#include "pc_options.h"
#include "pc_saves.h"
#include "pc_text.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
void pc_game_text_set_own_wait(u32 key, const char* utf8, u32 fontRef); /* pc_game_text.cpp: ends with the A arrow */
int pc_platform_automated_run(void);
const char* pc_game_text_language(void);
u32 VIGetRetraceCount(void);
int pc_field_menu_pc_spot(OrreFieldSpot* out);   /* pc_field_menu.cpp: the room PC MENU's PC screen was opened in */
void pc_pc_screen_leave_for_main_menu(void);     /* pc_pc_screen.cpp: Main Menu's way back, from the field */
}
#include "pc_game_ui.h"
#include "pc_paths.h" /* the UI folder (4.1) */

namespace {

struct Cond {
    int32_t flag;
    u32 value;
};
struct Window {
    std::string id;
    u32 room = 0;
    std::vector<Cond> when, rollback;
    bool keepSpot = true;
};

std::vector<Window> s_windows;
bool s_read = false;
u32 s_keyNotice = 0xFFF00033u, s_keyStuck = 0xFFF00034u;
int s_stuckSeconds = 30; /* the ini's [watch] seconds overrides; 30 since 2026-11-20 (The Under's subway holds ~22 s) */
s16 s_ynX = 440, s_ynY = 150; /* above the message window, as the PC screen's Main Menu question (pc_menu.ini) */

/* the same gate as PC MENU (pc_field_menu.cpp): the table only matters for PC MENU saves */
bool enabled() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_FIELD_PC_MENU");
    return force != nullptr && force[0] == '1';
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

/* "992=1 993=1 996=0": false when any piece can't be read (the row is then left out, logged) */
bool parse_conds(const std::string& v, std::vector<Cond>& out) {
    size_t p = 0;
    while (p < v.size()) {
        while (p < v.size() && (v[p] == ' ' || v[p] == '\t')) ++p;
        if (p >= v.size()) break;
        size_t e = p;
        while (e < v.size() && v[e] != ' ' && v[e] != '\t') ++e;
        const std::string tok = v.substr(p, e - p);
        p = e;
        const size_t eq = tok.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= tok.size()) return false;
        char* end = nullptr;
        const long long f = std::strtoll(tok.c_str(), &end, 0);
        if (end != tok.c_str() + eq || f < 0 || f > 0xFFFF) return false;
        /* through 64 bits (strtol's long is 32-bit on Windows; pc_pc_screen.cpp's pitfall) */
        const long long val = std::strtoll(tok.c_str() + eq + 1, &end, 0);
        if (*end != '\0' || val < 0 || val > 0xFFFFFFFFll) return false;
        out.push_back(Cond{(int32_t)f, (u32)val});
    }
    return !out.empty();
}

void read_table() {
    s_read = true;
    const std::string path = pc_paths_str(PC_PLACE_UI, "game/save_windows.ini"); /* app\UI\ (flat layout: ui\), 4.1 */
    size_t len = 0;
    char* text = static_cast<char*>(SDL_LoadFile(path.c_str(), &len));
    if (text == nullptr) {
        /* no built-in rows: a missing table only means no rollbacks (the hang watch still works), and the log says so */
        std::printf("[save_windows] %s can't be read (%s): no open-scene windows known; the hang watch still runs\n", path.c_str(),
                    SDL_GetError());
        return;
    }
    const std::string all(text, len);
    SDL_free(text);
    std::string section;
    Window* cur = nullptr;
    bool bad = false;
    auto finish = [&]() {
        if (cur == nullptr) return;
        if (bad || cur->room == 0 || cur->when.empty() || cur->rollback.empty()) {
            std::printf("[save_windows] save_windows.ini: window \"%s\" is incomplete or can't be read: left out\n", cur->id.c_str());
            s_windows.pop_back();
        }
        cur = nullptr;
        bad = false;
    };
    size_t at = 0;
    while (at < all.size()) {
        size_t nl = all.find('\n', at);
        if (nl == std::string::npos) nl = all.size();
        const std::string line = trim(all.substr(at, nl - at));
        at = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            finish();
            section = trim(line.substr(1, line.find(']') - 1));
            if (section.compare(0, 7, "window ") == 0) {
                s_windows.push_back(Window{});
                cur = &s_windows.back();
                cur->id = trim(section.substr(7));
            }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        const long long n = std::strtoll(v.c_str(), nullptr, 0);
        if (section == "notice") {
            if (k == "key") s_keyNotice = (u32)n;
        } else if (section == "watch") {
            if (k == "seconds" && n > 0) s_stuckSeconds = (int)n;
            else if (k == "key") s_keyStuck = (u32)n;
            else if (k == "yes_no_x") s_ynX = (s16)n;
            else if (k == "yes_no_y") s_ynY = (s16)n;
        } else if (cur != nullptr) {
            if (k == "room") cur->room = (u32)n;
            else if (k == "when") bad |= !parse_conds(v, cur->when);
            else if (k == "rollback") bad |= !parse_conds(v, cur->rollback);
            else if (k == "spot") {
                if (v == "keep") cur->keepSpot = true;
                else if (v == "pc") cur->keepSpot = false;
                else bad = true;
            }
        }
    }
    finish();
    std::printf("[save_windows] save_windows.ini read: %d open-scene window(s); hang watch after %d s\n", (int)s_windows.size(),
                s_stuckSeconds);
}

/* the row matching where PC MENU's PC screen was opened, or none (no PC MENU screen up, or nothing matches) */
const Window* match(u32* roomOut) {
    if (!s_read) read_table();
    OrreFieldSpot spot{};
    if (!enabled() || pc_game_ui.flag_get == nullptr || !pc_field_menu_pc_spot(&spot)) return nullptr;
    if (roomOut != nullptr) *roomOut = spot.room;
    for (const Window& w : s_windows) {
        if (w.room != spot.room) continue;
        bool all = true;
        for (const Cond& c : w.when) all = all && pc_game_ui.flag_get(c.flag) == c.value;
        if (all) return &w;
    }
    return nullptr;
}

std::string conds_text(const std::vector<Cond>& cs) {
    std::string s;
    char b[32];
    for (const Cond& c : cs) {
        std::snprintf(b, sizeof(b), "%s%d=%u", s.empty() ? "" : " ", (int)c.flag, (unsigned)c.value);
        s += b;
    }
    return s;
}

/* ---- part 1: the save ---- */
bool s_noticeOpen = false;
const Window* s_active = nullptr;  /* the row whose rollback is in the image during the write */
std::vector<Cond> s_restore;       /* the flags' values before the rollback */

void pc_save_step(s32 step) {
    const PcGameUi& g = pc_game_ui;
    if (step == 0) {
        s_noticeOpen = false;
        u32 room = 0;
        const Window* w = match(&room);
        if (w == nullptr || g.msg_window_open == nullptr) return;
        /* the game's language and its message font (font 0), made each time: the language can change. *Changed
         * 2026-11-20 (the first picture):* shown together with the question, the notice sat under the question's YES /
         * NO box (the game's own place for it, 60, 280, inside the message window), so it now ends with the game's
         * wait-for-A arrow and comes first: the window waits for A, closes, and the game's question follows on its own */
        std::printf("[save_windows] PC MENU save in room 0x%X matches the open-scene window \"%s\" (%s): the notice is shown\n",
                    (unsigned)room, w->id.c_str(), conds_text(w->when).c_str());
        pc_game_text_set_own_wait(s_keyNotice, pc_text_lang(pc_game_text_language(), "game.save_window.notice"), 0xF0000000u);
        g.msg_window_open(s_keyNotice);
        s_noticeOpen = true;
        if (g.msg_window_close != nullptr) g.msg_window_close();
        s_noticeOpen = false;
        std::printf("[save_windows] the notice was read (A): the game's save question follows\n");
    } else if (step == 1) {
        if (s_noticeOpen && g.msg_window_close != nullptr) g.msg_window_close();
        s_noticeOpen = false;
    } else if (step == 2) {
        s_active = nullptr;
        s_restore.clear();
        u32 room = 0;
        const Window* w = match(&room);
        if (w == nullptr) return;
        for (const Cond& c : w->rollback) {
            s_restore.push_back(Cond{c.flag, g.flag_get(c.flag)});
            g.flag_set(c.flag, c.value);
        }
        s_active = w;
        std::printf("[save_windows] window \"%s\" (room 0x%X): the save is written with %s (was %s); the game goes on unchanged\n",
                    w->id.c_str(), (unsigned)room, conds_text(w->rollback).c_str(), conds_text(s_restore).c_str());
    } else if (step == 3) {
        if (s_active == nullptr) return;
        for (const Cond& c : s_restore) g.flag_set(c.flag, c.value);
        std::printf("[save_windows] window \"%s\": the write returned; flags back to %s in memory\n", s_active->id.c_str(),
                    conds_text(s_restore).c_str());
        if (!s_active->keepSpot && pc_saves_ready()) {
            pc_saves_spot_set(pc_saves_current(), "");
            std::printf("[save_windows] window \"%s\": the PC MENU spot is dropped (Continue goes on from the last PC)\n",
                        s_active->id.c_str());
        }
        s_active = nullptr;
        s_restore.clear();
    }
}

/* ---- part 2: the hang watch ---- */
s32 (*s_prevContinueSpot)(OrreFieldSpot*) = nullptr;
void (*s_prevCardTaskEnd)(s32, s32) = nullptr;
s32 (*s_prevRoomLeave)(u32, s32, s32, s32) = nullptr;

bool s_armed = false;
u32 s_armedRoom = 0;
int s_scriptDepth = 0;   /* the hero step's scripts running now (OrreHook_HeroEventScript) */
u32 s_baseRoom = 0;      /* the room at the bottom of the room stack, last seen */
u32 s_heldSince = 0;     /* the retrace the current hold began at (0: not held) */
bool s_fired = false;    /* this hold already reached the limit */
int s_logAlways = -1;    /* ORRE_STUCK_WATCH=1 */
int s_limitFrames = 0;

int limit_frames() {
    if (s_limitFrames == 0) {
        if (!s_read) read_table();
        int sec = s_stuckSeconds;
        const char* v = SDL_getenv("ORRE_STUCK_SECONDS");
        if (v != nullptr && std::atoi(v) > 0) sec = std::atoi(v);
        s_limitFrames = sec * 60; /* the retrace count runs at 60 a second (VIGetRetraceCount) */
    }
    return s_limitFrames;
}

void hero_event_script(s32 running) {
    s_scriptDepth += running ? 1 : -1;
    if (s_scriptDepth < 0) s_scriptDepth = 0;
}

s32 continue_spot(OrreFieldSpot* out) {
    const s32 r = s_prevContinueSpot != nullptr ? s_prevContinueSpot(out) : 0;
    if (r == 1 && out->room != 0 && enabled()) {
        s_armed = true;
        s_armedRoom = out->room;
        s_scriptDepth = 0;
        s_heldSince = 0;
        s_fired = false;
        s_baseRoom = 0;
        std::printf("[save_windows] hang watch on: Continue from a saved spot (PC MENU's or an autosave's) in room 0x%X\n",
                    (unsigned)out->room);
    }
    return r;
}

void card_task_end(s32 kind, s32 result) {
    if (s_prevCardTaskEnd != nullptr) s_prevCardTaskEnd(kind, result);
    const bool wrote = result == 4 && (kind == 4 || kind == 9 || kind == 10 || kind == 11);
    if (wrote && s_armed) {
        s_armed = false;
        std::printf("[save_windows] hang watch off: a new save was made\n");
    }
}

/* the question, on its own game task (the message window and the YES / NO box wait) */
void offer_task() {
    const PcGameUi& g = pc_game_ui;
    static PcSaveVersion versions[16]; /* static: the GS thread's stack is small */
    const int slot = pc_saves_ready() ? pc_saves_current() : 0;
    const int n = slot != 0 ? pc_saves_versions(slot, versions, 16) : 0;
    const PcSaveVersion* prev = nullptr;
    for (int i = 0; i < n && prev == nullptr; ++i)
        if (!versions[i].same_as_now && !versions[i].is_undo) prev = &versions[i];
    /* *Added 2026-11-20 (autosave, docs/port/autosave.md section 3):* the slot's autosave is the other way back, offered
     * instead when it is newer than the previous version (both times are local "YYYYMMDD_HHMMSS", so the text order is
     * the time order). Not when the game in memory came from that autosave (it is what got stuck). *Fixed 2026-11-20*
     * (the save-check sweep's finding): this used pc_autosave_loaded_now(), which is spent at Continue, so a stuck
     * autosave was offered back; pc_autosave_playing_from() lasts from Continue until the next title read or save */
    static PcAutosaveInfo autosave;
    const bool haveAuto = slot != 0 && pc_autosave_info(slot, &autosave) != 0 &&
                          !pc_autosave_playing_from(slot, autosave.when);
    if (haveAuto && (prev == nullptr || std::strcmp(autosave.when, prev->when) > 0)) {
        pc_game_text_set_own(s_keyStuck, pc_text_lang(pc_game_text_language(), "game.save_window.stuck_autosave"), 0xF0000000u);
        std::printf("[save_windows] hang watch: asking to go back to slot %d's autosave %s (newer than %s)\n", slot,
                    autosave.when, prev != nullptr ? prev->when : "any previous version");
        g.msg_window_open(s_keyStuck);
        const int answer = g.yes_no(0 /* YES first */, s_ynX, s_ynY);
        g.msg_window_close();
        if (answer != 0) {
            std::printf("[save_windows] hang watch: %s: left as it is\n", answer == 1 ? "NO" : "B");
            return;
        }
        /* the player's own save stays as it is; the main menu's re-read loads the autosave in its place (pc_autosave),
         * and Continue goes on from the autosave's spot */
        s_armed = false;
        pc_autosave_request_load(slot);
        std::printf("[save_windows] hang watch: YES: slot %d's autosave %s; to the main menu\n", slot, autosave.when);
        pc_pc_screen_leave_for_main_menu();
        return;
    }
    if (prev == nullptr) {
        std::printf("[save_windows] hang watch: slot %d has no previous version to go back to: not offered\n", slot);
        return;
    }
    pc_game_text_set_own(s_keyStuck, pc_text_lang(pc_game_text_language(), "game.save_window.stuck_question"), 0xF0000000u);
    std::printf("[save_windows] hang watch: asking to go back to slot %d's previous version %s (%s)\n", slot, prev->when, prev->why);
    g.msg_window_open(s_keyStuck);
    const int answer = g.yes_no(0 /* YES first */, s_ynX, s_ynY);
    g.msg_window_close();
    if (answer != 0) {
        std::printf("[save_windows] hang watch: %s: left as it is\n", answer == 1 ? "NO" : "B");
        return;
    }
    if (!pc_saves_restore(slot, prev->folder)) {
        std::printf("[save_windows] hang watch: YES, but the version couldn't be put back: left as it is\n");
        return;
    }
    s_armed = false;
    std::printf("[save_windows] hang watch: YES: slot %d's version %s put back (the stuck save kept in Backups, Undo offered); "
                "to the main menu\n", slot, prev->when);
    pc_pc_screen_leave_for_main_menu();
}

void watch_tick(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    if (s_logAlways < 0) {
        const char* v = SDL_getenv("ORRE_STUCK_WATCH");
        s_logAlways = v != nullptr && v[0] == '1';
    }
    if (depth == 0 && settled && room != s_baseRoom) {
        /* a new room at the bottom of the stack: its scripts are new (a room change ends the old ones, even one that
         * never returned), and the watch only covers the room the PC MENU Continue went to */
        s_scriptDepth = 0;
        s_baseRoom = room;
        s_heldSince = 0;
        s_fired = false;
        if (s_armed && room != s_armedRoom) {
            s_armed = false;
            std::printf("[save_windows] hang watch off: room 0x%X left for 0x%X\n", (unsigned)s_armedRoom, (unsigned)room);
        }
    }
    if (!s_armed && !s_logAlways) return;
    const PcGameUi& g = pc_game_ui;
    if (g.hero_event_locked == nullptr || g.player_prompt_up == nullptr) return;
    const bool held = s_scriptDepth > 0 && depth == 0 && settled && !pauseOpen && g.hero_event_locked() && g.player_prompt_up() == 0;
    if (!held) {
        s_heldSince = 0;
        s_fired = false;
        return;
    }
    const u32 now = VIGetRetraceCount();
    if (s_heldSince == 0) s_heldSince = now != 0 ? now : 1;
    if (s_fired || (int)(now - s_heldSince) < limit_frames()) return;
    s_fired = true;
    std::printf("[save_windows] hang watch: a talk / touch script has held the player %d s with nothing on screen to answer "
                "(room 0x%X, retrace %u)\n", limit_frames() / 60, (unsigned)room, (unsigned)now);
    if (!s_armed || !enabled()) {
        std::printf("[save_windows] hang watch: not offered (%s)\n", s_armed ? "PC MENU is off" : "not after a PC MENU Continue");
        return;
    }
    if (g.start_task == nullptr || !g.start_task(offer_task))
        std::printf("[save_windows] hang watch: the question's task couldn't be started\n");
}

s32 room_leave(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    const s32 r = s_prevRoomLeave != nullptr ? s_prevRoomLeave(room, depth, settled, pauseOpen) : 0;
    watch_tick(room, depth, settled, pauseOpen);
    return r;
}

} // namespace

/* For the autosave (pc_autosave.cpp; 2026-11-20): how many of the hero step's talk / touch scripts are running now (the
 * same count the hang watch keeps from OrreHook_HeroEventScript), and whether a known open-scene window of the table
 * matches this room with the flags as they are now. The autosave never writes while either is true. Read only; the table
 * is read on first use. Platforms: all. */
extern "C" int pc_save_windows_script_depth(void) { return s_scriptDepth; }
extern "C" int pc_save_windows_window_open(unsigned room) {
    if (!s_read) read_table();
    if (pc_game_ui.flag_get == nullptr) return 0;
    for (const Window& w : s_windows) {
        if (w.room != room) continue;
        bool all = true;
        for (const Cond& c : w.when) all = all && pc_game_ui.flag_get(c.flag) == c.value;
        if (all) return 1;
    }
    return 0;
}

/* For the save-check sweep (pc_savecheck.cpp; 2026-11-20): the table row that matches this room with the flags as they
 * are now, as text ("<id> rollback=<flag=value ...> spot=keep|pc"), so each test save's note says whether a PC MENU
 * save at that moment would be written rolled back, and how; "" when none. Read only; all platforms */
extern "C" int pc_save_windows_window_desc(unsigned room, char* out, int cap) {
    if (out == nullptr || cap <= 0) return 0;
    out[0] = '\0';
    if (!s_read) read_table();
    if (pc_game_ui.flag_get == nullptr) return 0;
    for (const Window& w : s_windows) {
        if (w.room != room) continue;
        bool all = true;
        for (const Cond& c : w.when) all = all && pc_game_ui.flag_get(c.flag) == c.value;
        if (!all) continue;
        std::snprintf(out, (size_t)cap, "%s rollback=%s spot=%s", w.id.c_str(), conds_text(w.rollback).c_str(),
                      w.keepSpot ? "keep" : "pc");
        return 1;
    }
    return 0;
}

/* called once at start-up (pc_platform.c), after pc_pc_screen_start: it chains Continue's spot hook (PC MENU's), the
 * card-task hook (the names' and PC MENU's) and the room-leave tick (the PC screen's Main Menu) */
extern "C" void pc_save_windows_start(void) {
    OrreHook_PcSaveStep = pc_save_step;
    OrreHook_HeroEventScript = hero_event_script;
    s_prevContinueSpot = OrreHook_ContinueSpot;
    OrreHook_ContinueSpot = continue_spot;
    s_prevCardTaskEnd = OrreHook_CardTaskEnd;
    OrreHook_CardTaskEnd = card_task_end;
    s_prevRoomLeave = OrreHook_RoomLeave;
    OrreHook_RoomLeave = room_leave;
}
