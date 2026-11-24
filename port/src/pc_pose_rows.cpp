/*
 * pc_pose_rows.cpp: people put back the way a story scene left them, after a Continue from a saved spot (2026-11-21,
 * the project's build helper; snatcherLeo: "yes, put Vander back on the floor", and to make it general, a data table that story mods
 * can add rows to). The table: port/ui/game/pose_rows.ini (its header explains every field). How it works:
 * docs/port/pc-menu-anywhere.md, "Pose rows"; the case behind it: the project notes, section 3.
 *
 * Why it exists: PC MENU (pc_field_menu.cpp) and the autosave (pc_autosave.cpp) continue the game from a saved spot.
 * On the GameCube a Continue always starts at a PC, so a room's setup script only handles the story states a player can
 * walk in with. Mt. Battle's valley is the known case (verified from the decoded script): after the Dakim battle (2678
 * set) and before Vander is talked to (1073 clear), Vander lies on the bridge where the scene left him, but the room's
 * setup only lays him down before the battle; a Continue from a spot saved there showed him standing. Cosmetic (the save
 * check found the story goes on), but wrong to look at.
 *
 * How:
 *   1. OrreHook_ContinueSpot (chained last, so it sees the final answer of PC MENU's, the autosave's and the hang
 *      watch's): a Continue that goes to a saved spot (answer 1) arms the room it goes to.
 *   2. OrreHook_RoomEntryDone (gs_floor.c): when that room's entry-phase scripts have all finished (its setup has placed
 *      its people) and before its main loop starts, every row for the room whose flags all hold poses its person with
 *      the game's own commands (pc_game_ui.person_look: std067 / std065 for the head, when the row has "look"; then
 *      pc_game_ui.person_pose: std051 animation, std059 placement, the facing at once). Then
 *      it disarms: the next room entry, and any entry not following such a Continue, is the GameCube's.
 * The switch: the same gate as PC MENU (off in GameCube-accurate mode, where PC MENU and the autosave are off too, and
 * in automated runs unless ORRE_FIELD_PC_MENU=1). The accurate-mode list gets a row (through C).
 * Random numbers: none drawn (likely: the three people calls set an animation, a place and a rotation; checked by the
 * random-number count in the test). The written save is never touched (a pose is not saved; the next Continue from the
 * same spot poses again), so the GameCube copy's checklist needs no rule.
 *
 * Platforms: all the same (plain C++ over the game's own people functions and SDL's file loading). Linux and macOS need
 * nothing different (likely).
 * For XD: XD's rooms have the same setup / main-loop structure and the same people commands (likely, the shared script
 * engine), so the same kind of row would work with XD's own rooms, flags and people (guess until XD is checked).
 */
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "pc_options.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
int pc_platform_automated_run(void);
}
#include "pc_game_ui.h"
#include "pc_paths.h" /* the UI folder (4.1) */

namespace {

struct Cond {
    int32_t flag;
    u32 value;
};
struct Row {
    std::string id;
    u32 room = 0;
    std::vector<Cond> when;
    u32 group = 0, index = 0;
    bool havePerson = false, havePosition = false;
    int32_t anim = -1, loop = 1;
    float x = 0, y = 0, z = 0;
    bool keepFacing = true;
    float facing = 0;
    int look = -1; /* the "look" field: -1 keep (the setup's), 0 off (std067), 1 follow the player (std065) */
};

std::vector<Row> s_rows;
bool s_read = false;
u32 s_armedRoom = 0; /* the room a spot Continue goes to (0: not armed) */

s32 (*s_prevContinueSpot)(OrreFieldSpot*) = nullptr;
void (*s_prevEntryDone)(u32, s32) = nullptr;

/* the same gate as PC MENU (pc_field_menu.cpp, pc_save_windows.cpp) */
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

std::vector<std::string> words(const std::string& v) {
    std::vector<std::string> out;
    size_t p = 0;
    while (p < v.size()) {
        while (p < v.size() && (v[p] == ' ' || v[p] == '\t')) ++p;
        size_t e = p;
        while (e < v.size() && v[e] != ' ' && v[e] != '\t') ++e;
        if (e > p) out.push_back(v.substr(p, e - p));
        p = e;
    }
    return out;
}

/* "2678=1 1073=0" (the same form as save_windows.ini): false when any piece can't be read */
bool parse_conds(const std::string& v, std::vector<Cond>& out) {
    for (const std::string& tok : words(v)) {
        const size_t eq = tok.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= tok.size()) return false;
        char* end = nullptr;
        const long long f = std::strtoll(tok.c_str(), &end, 0);
        if (end != tok.c_str() + eq || f < 0 || f > 0xFFFF) return false;
        const long long val = std::strtoll(tok.c_str() + eq + 1, &end, 0); /* 64-bit: Windows' long is 32-bit */
        if (*end != '\0' || val < 0 || val > 0xFFFFFFFFll) return false;
        out.push_back(Cond{(int32_t)f, (u32)val});
    }
    return !out.empty();
}

bool parse_float(const std::string& s, float* out) {
    char* end = nullptr;
    const double d = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    *out = (float)d;
    return true;
}

void read_table() {
    s_read = true;
    const std::string path = pc_paths_str(PC_PLACE_UI, "game/pose_rows.ini"); /* app\UI\ (flat layout: ui\), 4.1 */
    size_t len = 0;
    char* text = static_cast<char*>(SDL_LoadFile(path.c_str(), &len));
    if (text == nullptr) {
        std::printf("[pose_rows] %s can't be read (%s): no pose rows\n", path.c_str(), SDL_GetError());
        return;
    }
    const std::string all(text, len);
    SDL_free(text);
    Row* cur = nullptr;
    bool bad = false;
    auto finish = [&]() {
        if (cur == nullptr) return;
        if (bad || cur->room == 0 || cur->when.empty() || !cur->havePerson || !cur->havePosition) {
            std::printf("[pose_rows] pose_rows.ini: row \"%s\" is incomplete or can't be read: left out\n", cur->id.c_str());
            s_rows.pop_back();
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
            const std::string section = trim(line.substr(1, line.find(']') - 1));
            if (section.compare(0, 5, "pose ") == 0) {
                s_rows.push_back(Row{});
                cur = &s_rows.back();
                cur->id = trim(section.substr(5));
            }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos || cur == nullptr) continue;
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        const std::vector<std::string> w = words(v);
        if (k == "room") {
            cur->room = (u32)std::strtoll(v.c_str(), nullptr, 0);
        } else if (k == "when") {
            bad |= !parse_conds(v, cur->when);
        } else if (k == "person") {
            if (w.size() == 2) {
                cur->group = (u32)std::strtoll(w[0].c_str(), nullptr, 0);
                cur->index = (u32)std::strtoll(w[1].c_str(), nullptr, 0);
                cur->havePerson = true;
            } else {
                bad = true;
            }
        } else if (k == "anim") {
            cur->anim = (int32_t)std::strtoll(v.c_str(), nullptr, 0);
        } else if (k == "loop") {
            cur->loop = std::atoi(v.c_str()) != 0;
        } else if (k == "position") {
            cur->havePosition = w.size() == 3 && parse_float(w[0], &cur->x) && parse_float(w[1], &cur->y) &&
                                parse_float(w[2], &cur->z);
            bad |= !cur->havePosition;
        } else if (k == "facing") {
            if (v == "keep") {
                cur->keepFacing = true;
            } else {
                cur->keepFacing = false;
                bad |= !parse_float(v, &cur->facing);
            }
        } else if (k == "look") {
            /* 2026-11-21 (snatcherLeo: Vander's "head follows me when I don't think it should"): the head as the scene left it */
            if (v == "keep") cur->look = -1;
            else if (v == "off") cur->look = 0;
            else if (v == "player") cur->look = 1;
            else bad = true;
        }
    }
    finish();
    std::printf("[pose_rows] pose_rows.ini read: %d row(s)\n", (int)s_rows.size());
}

/* OrreHook_ContinueSpot, chained last: the final answer passes through; a spot Continue arms its room */
s32 continue_spot(OrreFieldSpot* out) {
    const s32 r = s_prevContinueSpot != nullptr ? s_prevContinueSpot(out) : 0;
    s_armedRoom = r == 1 && out->room != 0 && enabled() ? out->room : 0;
    return r;
}

/* OrreHook_RoomEntryDone: the armed room's setup has run; its rows whose flags hold pose their people */
void entry_done(u32 room, s32 depth) {
    if (s_prevEntryDone != nullptr) s_prevEntryDone(room, depth);
    if (s_armedRoom == 0) return;
    const u32 want = s_armedRoom;
    s_armedRoom = 0; /* one room entry only: the one the Continue went to */
    if (room != want || depth != 0 || !enabled() || pc_game_ui.flag_get == nullptr || pc_game_ui.person_pose == nullptr) {
        if (room != want) std::printf("[pose_rows] the Continue's room 0x%X wasn't the first room entered (0x%X): no pose\n",
                                      (unsigned)want, (unsigned)room);
        return;
    }
    if (!s_read) read_table();
    for (const Row& r : s_rows) {
        if (r.room != room) continue;
        bool all = true;
        for (const Cond& c : r.when) all = all && pc_game_ui.flag_get(c.flag) == c.value;
        if (!all) continue;
        /* the head first, in the scene's order (func73 stops Vander's look-at, then animates him); person_look does
         * nothing when the person isn't in the room, and person_pose then says so */
        int looked = -1; /* person_look's answer: 2 it had a look target before, 1 it hadn't, 0 not found */
        if (r.look >= 0 && pc_game_ui.person_look != nullptr) looked = pc_game_ui.person_look(r.group, r.index, r.look);
        const int found = pc_game_ui.person_pose(r.group, r.index, r.anim, r.loop, r.x, r.y, r.z, r.keepFacing ? 0 : 1,
                                                 r.facing);
        if (found)
            std::printf("[pose_rows] %s: person %u %u posed in room 0x%X (animation %d%s at %.1f, %.1f, %.1f; head %s%s)\n",
                        r.id.c_str(), (unsigned)r.group, (unsigned)r.index, (unsigned)room, (int)r.anim,
                        r.loop ? ", looping" : "", r.x, r.y, r.z,
                        r.look < 0 ? "as the setup left it" : r.look == 0 ? "still" : "following the player",
                        looked == 2 ? ", it had a look target" : looked == 1 ? ", it had no look target" : "");
        else
            std::printf("[pose_rows] %s: person %u %u isn't in room 0x%X: nothing done\n", r.id.c_str(), (unsigned)r.group,
                        (unsigned)r.index, (unsigned)room);
    }
}

} // namespace

/* called once at start-up (pc_platform.c), after pc_savecheck_start: it chains Continue's spot hook last, so it sees the
 * final answer of PC MENU's, the autosave's and the hang watch's */
extern "C" void pc_pose_rows_start(void) {
    s_prevContinueSpot = OrreHook_ContinueSpot;
    OrreHook_ContinueSpot = continue_spot;
    s_prevEntryDone = OrreHook_RoomEntryDone;
    OrreHook_RoomEntryDone = entry_done;
}
