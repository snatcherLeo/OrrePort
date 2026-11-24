/*
 * pc_savecheck.cpp: the save-check sweep's test switch (2026-11-20, the project's helper; why and the results:
 * the project notes, "The save-check sweep"). Test runs only; nothing here runs unless its switch is set.
 *
 * Why it exists: OrrePort lets the player save anywhere (PC MENU) and autosaves; the game itself only saves at a PC,
 * where the room is rebuilt from the story flags. A save taken elsewhere can hold a room state the room's setup can't
 * rebuild (Rui's sack in Phenac: it hung after Continue until the open-scene table, ui/game/save_windows.ini). The sweep
 * finds any other such moment across the story by replaying the reference playthrough's recordings and writing a real save at every moment
 * a player could have saved, then loading each one in a second run (tools/local/testing/savecheck.py) and checking it.
 *
 * Two switches:
 *   ORRE_SAVECHECK=<folder>  during a replay, write a numbered save (NNNN.gci + NNNN.ini) into that folder at every
 *       candidate moment:
 *         - "room entry": the first moment the player has control after a room has settled (the autosave's entry point,
 *           without its once-a-minute rule and without its "no flag changed" rule: the point is to test the risky
 *           moments too);
 *         - "control back": every frame control returns after a scene, a talk / touch script, a message, a battle or a
 *           menu room (the hero event lock clear, no talk / touch script running, nothing on screen to answer, a
 *           resumable field spot), after the signals have held kStableFrames frames, as the autosave waits. Not the field
 *           camera (the autosave's extra guard): a scene can hand the player back with its own camera still on, as the
 *           sack scene does (held_why says more);
 *         - "timer": every 5 minutes of the same room (the autosave's timer, again without its rules);
 *         - ORRE_SAVECHECK_EVERY=<frames>[@<from>-<to>]: also every that many frames while the player has control (dense
 *           coverage of a known window, e.g. Rui's sack), optionally only within those game frames.
 *       Each save is the autosave's own bytes (pc_autosave_write_copy: the same capture, field swap, cipher and writer
 *       thread), so loading it is loading an autosave taken at that moment. The .ini beside it (the autosave's format, so
 *       it can be dropped into Slot N\Autosave\ as is) adds notes: the frame, the room, why this moment, the flags changed
 *       since the room was entered (first old value > value now, and the frame), the open-scene table's row if one
 *       matches (what a PC MENU save there would roll back), and every person in the room (who is shown, where, standing
 *       or wandering), so the reload can be compared with the room the game itself had.
 *       Filters: ORRE_SAVECHECK_ROOMS=<hex,hex,...> (only those rooms), ORRE_SAVECHECK_FRAMES=<from>-<to> (game frames);
 *       ORRE_SAVECHECK_TEMPLATE=<.gci> (the card header and other slots; needed when the sitting has no save of its own
 *       yet, as P01, a new game).
 *   ORRE_SAVECHECK_PROBE=1  in a reload run, after Continue: logs the frame the player gets control, every hold after
 *       that (what held him and for how long; 20 s held with nothing on screen to answer says HELD), where Wes stands
 *       every 60 frames, and the people
 *       at the moment of control, all as "[savecheck] probe" lines the reload checker reads.
 *
 * Random numbers and the replay: none drawn and nothing in the game written (likely, and checked: a sitting replayed with
 * the switch on gives the same rolls and rooms as the live sitting, the project notes). Every read is
 * the game's own state (pc_game_ui: save_capture copies the image into OrrePort's own buffer, field_spot, people_rows,
 * place_text, the flag and lock reads); the file work runs on the autosave's writer thread. The flag watch only records.
 *
 * Platforms: plain C++ over SDL's environment calls and the autosave's writer; the same on Windows, Linux and macOS
 * (likely; run on Windows only). Folder paths in the switches are whatever the OS takes.
 * For XD: the same sweep idea carries (likely): XD's rooms have the same setup / main loop / scene structure; the signals
 * (hero event lock, the hero step's scripts, message windows) and the save capture would be XD's own (docs/xd-insights.md).
 */
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_autosave.h"
#include "pc_lowmem.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
int pc_platform_automated_run(void);
extern volatile u32 pc_vi_heartbeat;              /* pc_vi.c: the game frame count the logs and pad scripts use */
int pc_save_windows_script_depth(void);           /* pc_save_windows.cpp: the hero step's talk / touch scripts running */
int pc_save_windows_window_desc(unsigned room, char* out, int cap); /* pc_save_windows.cpp: the table row that matches */
}
#include "pc_game_ui.h"

namespace {

constexpr u32 kBlock = 0x1DFD0;      /* the save image (save-data.md) */
constexpr int kStableFrames = 2;     /* the same wait the autosave makes before it writes */
constexpr u32 kTimerFrames = 300 * 60; /* the autosave's five minutes (the retrace and game frame run at 60 a second) */
constexpr u32 kLongHold = 20 * 60;   /* the hang watch's 20 s */

/* ---- the switches ---- */
bool s_on = false, s_probe = false;
std::string s_dir, s_templ;
u32 s_every = 0, s_from = 0, s_to = 0xFFFFFFFFu, s_everyFrom = 0, s_everyTo = 0xFFFFFFFFu;
std::vector<u32> s_rooms;

bool room_wanted(u32 room) {
    if (s_rooms.empty()) return true;
    for (u32 r : s_rooms)
        if (r == room) return true;
    return false;
}

/* ---- flags changed since the room was entered (recorded, never written) ---- */
struct Change {
    int32_t id;
    u32 first, now, frame;
};
std::vector<Change> s_changes;

/* ---- the moment tracker ---- */
u32 s_baseRoom = 0, s_enteredAt = 0, s_leftAt = 0, s_lastWrite = 0, s_lastTimer = 0, s_heldSince = 0;
bool s_entryPending = false, s_wasHeld = false;
bool s_settled = false; /* the room manager's own "settled" (isActive 4) this frame: the autosave needs it, PC MENU not */
std::string s_heldWhy;
int s_stable = 0, s_count = 0;
uint8_t* s_capture = nullptr; /* below 4 GB (pc_game_ui.save_capture) */

/* the probe's state (reload runs) */
bool s_armed = false, s_controlled = false, s_probeHeld = false, s_longLogged = false;
u32 s_continueAt = 0, s_probeHeldAt = 0, s_lastPos = 0, s_silentAt = 0;
std::string s_probeWhy;

/* why the player has no control now ("" = he has it; "pause" = the pause menu, which is neither). The same signals as
 * the autosave's "may save now" (pc_autosave.cpp, blocked()), without its own rules (the minute, the open-scene table,
 * a bound slot): the sweep wants exactly the moments those rules would refuse */
std::string held_why(s32 pauseOpen, uint32_t ids[3], float xyz[3], int32_t* facing) {
    const PcGameUi& g = pc_game_ui;
    if (pauseOpen) return "pause";
    if (g.battle_active != nullptr && g.battle_active()) return "a battle";
    if (g.hero_event_locked != nullptr && g.hero_event_locked()) return "a scene (hero event lock)";
    if (pc_save_windows_script_depth() > 0) return "a talk or touch script";
    if (g.player_prompt_up != nullptr && g.player_prompt_up() != 0) return "a message or window";
    /* *Changed 2026-11-20 (the first P01 runs):* the field camera is not a sign of control. After the Folly battle the
     * sack scene hands the player back (heroMoveTermEvent) with its own camera still on, and the whole sack window
     * (frames 38954-42140 of P01) had the field camera off while Wes walked and talked, and PC MENU opened there in
     * snatcherLeo's sitting. So it is recorded (camera_off, below) but doesn't hold the player. The autosave keeps it as one
     * of its own guards, which is one reason it never writes in that window */
    if (g.field_spot == nullptr || !g.field_spot(ids, xyz, facing)) return "not a resumable field spot";
    return "";
}

bool camera_off() {
    float hdy[3];
    return pc_game_ui.field_camera_state != nullptr && (pc_game_ui.field_camera_state(hdy) & 0xFF) == 0;
}

std::string changes_text() {
    std::string s;
    char b[64];
    for (const Change& c : s_changes) {
        if (c.now == c.first) continue; /* set and put back: no change */
        std::snprintf(b, sizeof b, "%s%d:%u>%u@%u", s.empty() ? "" : " ", (int)c.id, (unsigned)c.first, (unsigned)c.now,
                      (unsigned)c.frame);
        s += b;
    }
    return s;
}

std::string people_text(const char* prefix) {
    static char rows[48 * 1024];
    if (pc_game_ui.people_rows == nullptr || pc_game_ui.people_rows(rows, (int32_t)sizeof rows) <= 0) return "";
    std::string out;
    for (const char* p = rows; *p != '\0';) {
        const char* nl = std::strchr(p, '\n');
        const size_t n = nl != nullptr ? (size_t)(nl - p) : std::strlen(p);
        out += prefix;
        out.append(p, n);
        out += '\n';
        p += n + (nl != nullptr ? 1 : 0);
    }
    return out;
}

void write_save(const char* why, u32 now, const uint32_t ids[3], const float xyz[3], int32_t facing) {
    if (now < s_from || now > s_to || !room_wanted(ids[0])) return;
    if (s_capture == nullptr) s_capture = static_cast<uint8_t*>(pc_lowmem_alloc(kBlock + 0x20));
    if (s_capture == nullptr || pc_game_ui.save_capture == nullptr || pc_game_ui.save_capture(s_capture) != kBlock) {
        std::printf("[savecheck] frame %u: the save image can't be copied: no save\n", (unsigned)now);
        return;
    }
    char place[160] = "";
    if (pc_game_ui.place_text != nullptr) pc_game_ui.place_text(ids[0], place, sizeof place);
    char window[200] = "";
    pc_save_windows_window_desc(ids[0], window, (int)sizeof window);
    const std::string flags = changes_text();
    char head[600];
    std::snprintf(head, sizeof head,
                  "# the save-check sweep's notes (pc_savecheck.cpp)\nframe=%u\nroom=0x%X\nwhy=%s\nentered=%u\n"
                  "settled=%d\ncamera_off=%d\nheld_frames=%u\nwindow=%s\nflags_changed=",
                  (unsigned)now, (unsigned)ids[0], why, (unsigned)s_enteredAt, s_settled ? 1 : 0, camera_off() ? 1 : 0,
                  (unsigned)(s_heldSince != 0 && now >= s_heldSince ? now - s_heldSince : 0), window);
    const std::string extra = std::string(head) + flags + "\n" + people_text("npc=");
    char gci[1400], ini[1400];
    ++s_count;
    std::snprintf(gci, sizeof gci, "%s/%04d.gci", s_dir.c_str(), s_count);
    std::snprintf(ini, sizeof ini, "%s/%04d.ini", s_dir.c_str(), s_count);
    PcAutosaveCopySpot spot{ids[0], ids[1], ids[2], xyz[0], xyz[1], xyz[2], (int)facing};
    if (!pc_autosave_write_copy(s_capture, s_templ.c_str(), gci, ini, &spot, place, why, extra.c_str())) {
        std::printf("[savecheck] frame %u: save %04d couldn't be queued\n", (unsigned)now, s_count);
        return;
    }
    s_lastWrite = now;
    std::printf("[savecheck] save %04d at frame %u in room 0x%X (%.1f, %.1f, %.1f)%s: %s; flags changed in the room: %s%s%s\n",
                s_count, (unsigned)now, (unsigned)ids[0], xyz[0], xyz[1], xyz[2],
                !s_settled ? " (room not settled: no autosave possible)"
                           : camera_off() ? " (a scene's camera: no autosave possible)" : "",
                why, flags.empty() ? "none" : flags.c_str(),
                window[0] != '\0' ? "; open-scene window " : "", window);
}

/* the reload probe, each settled field frame after Continue */
void probe_tick(u32 now, const std::string& why, const uint32_t ids[3], const float xyz[3], int32_t facing) {
    if (!s_controlled) {
        if (!why.empty()) return;
        if (s_stable < kStableFrames) return;
        s_controlled = true;
        s_lastPos = now;
        std::printf("[savecheck] probe: control at frame %u (%u frames after Continue) in room 0x%X at (%.2f, %.2f, %.2f) "
                    "facing %d\n", (unsigned)now, (unsigned)(now - s_continueAt), (unsigned)ids[0], xyz[0], xyz[1], xyz[2],
                    (int)facing);
        std::fputs(people_text("[savecheck] probe npc ").c_str(), stdout);
        return;
    }
    const bool held = !why.empty() && why != "pause";
    if (held && !s_probeHeld) {
        s_probeHeld = true;
        s_probeHeldAt = now;
        s_probeWhy = why;
        s_longLogged = false;
        std::printf("[savecheck] probe: held from frame %u (%s)\n", (unsigned)now, why.c_str());
    } else if (!held && s_probeHeld && why.empty()) {
        s_probeHeld = false;
        std::printf("[savecheck] probe: free again at frame %u after %u frames (%s)\n", (unsigned)now,
                    (unsigned)(now - s_probeHeldAt), s_probeWhy.c_str());
    }
    /* a hang is a hold with nothing on screen to answer (the hang watch's own rule, pc_save_windows.cpp): a long scene
     * with messages and menus (Rui joining: 3,145 frames, P01) is not one. *Changed 2026-11-20 (the first rollback
     * reload):* the 20 s first counted every hold, and that scene was taken for a hang */
    const bool prompt = pc_game_ui.player_prompt_up != nullptr && pc_game_ui.player_prompt_up() != 0;
    if (!s_probeHeld || prompt) {
        s_silentAt = 0;
    } else {
        if (s_silentAt == 0) s_silentAt = now != 0 ? now : 1;
        if (!s_longLogged && now - s_silentAt >= kLongHold) {
            s_longLogged = true;
            std::printf("[savecheck] probe: HELD 20 s from frame %u with nothing on screen to answer (%s, now: %s)\n",
                        (unsigned)s_silentAt, s_probeWhy.c_str(), why.c_str());
        }
    }
    if (now - s_lastPos >= 60 && pc_game_ui.hero_spot != nullptr) {
        uint32_t room = 0;
        float p[3];
        int32_t f = 0;
        s_lastPos = now;
        if (pc_game_ui.hero_spot(&room, p, &f))
            std::printf("[savecheck] probe: wes frame %u room 0x%X %.2f %.2f %.2f %d\n", (unsigned)now, (unsigned)room, p[0],
                        p[1], p[2], (int)f);
    }
}

/* ORRE_SAVECHECK_WHY=1: every change of the tracker's state is logged (which signal holds the player), within
 * ORRE_SAVECHECK_FRAMES; for finding why a stretch of play had no candidate moment */
int s_whyLog = -1;
std::string s_lastWhyLogged;
void why_log(u32 now, u32 room, s32 depth, s32 settled, const std::string& why) {
    if (s_whyLog < 0) {
        const char* v = SDL_getenv("ORRE_SAVECHECK_WHY");
        s_whyLog = v != nullptr && v[0] == '1';
    }
    if (!s_whyLog || now < s_from || now > s_to) return;
    char b[300];
    std::snprintf(b, sizeof b, "room 0x%X depth %d settled %d: %s", (unsigned)room, (int)depth, (int)settled,
                  why.empty() ? "free" : why.c_str());
    if (s_lastWhyLogged == b) return;
    s_lastWhyLogged = b;
    std::printf("[savecheck] why frame %u %s\n", (unsigned)now, b);
}

void tick(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    const u32 now = pc_vi_heartbeat;
    s_settled = settled != 0;
    if (depth != 0 || (!settled && room != s_baseRoom)) why_log(now, room, depth, settled, "pushed or entering");
    /* "Not settled" in the room already entered is not a new entry: after a pushed room (a battle, a menu) pops, the
     * field goes through its entry states again for a few frames (P01: 3 frames at 45566) with the player still held by
     * the scene; PC MENU doesn't look at the room manager's state at all. So only a new room's entry (or a pushed room)
     * waits for the room to settle; in the same room the player's own signals decide. (A first guess here, that the
     * whole sack window was unsettled, was wrong: ORRE_SAVECHECK_WHY showed the field camera, see held_why) */
    if (depth == 0 && !settled && room == s_baseRoom && s_baseRoom != 0) settled = 1;
    if (depth != 0 || !settled) {
        /* a menu room or battle pushed, or a room change: control comes back afterwards ("control back") */
        if (depth == 0 && s_leftAt == 0) s_leftAt = now != 0 ? now : 1;
        if (!s_wasHeld) {
            s_wasHeld = true;
            s_heldSince = now;
            s_heldWhy = depth != 0 ? "a menu room or battle" : "a room change";
        }
        s_stable = 0;
        if (s_probe && s_controlled && !s_probeHeld && depth != 0) {
            s_probeHeld = true, s_probeHeldAt = now, s_probeWhy = "a menu room or battle", s_longLogged = false;
            std::printf("[savecheck] probe: held from frame %u (a menu room or battle)\n", (unsigned)now);
        }
        return;
    }
    if (room != s_baseRoom) {
        /* a new room at the bottom of the stack (a battle's or menu's return is the same room: not an entry). The flags
         * changed while it was being entered stay with it (its own setup's), the old room's go */
        std::vector<Change> keep;
        for (const Change& c : s_changes)
            if (s_leftAt != 0 && c.frame >= s_leftAt) keep.push_back(c);
        s_changes.swap(keep);
        s_baseRoom = room;
        s_enteredAt = now;
        s_lastTimer = now;
        s_entryPending = true;
        if (!s_wasHeld) s_heldSince = now;
        s_wasHeld = true;
        s_heldWhy = "a room change";
        s_stable = 0;
    }
    s_leftAt = 0;
    uint32_t ids[3] = {0, 0, 0};
    float xyz[3] = {0, 0, 0};
    int32_t facing = 0;
    const std::string why = held_why(pauseOpen, ids, xyz, &facing);
    why_log(now, room, depth, s_settled ? 1 : 0, why);
    if (why.empty()) ++s_stable;
    else s_stable = 0;
    if (s_probe && s_armed) probe_tick(now, why, ids, xyz, facing);
    if (!s_on) return;
    if (why == "pause") return; /* the pause menu is neither a scene nor control */
    if (!why.empty()) {
        if (!s_wasHeld) {
            s_wasHeld = true;
            s_heldSince = now;
            s_heldWhy = why;
        }
        return;
    }
    if (s_stable < kStableFrames) return;
    if (s_wasHeld) {
        const std::string what = s_entryPending ? std::string("room entry") : "control back after " + s_heldWhy;
        write_save(what.c_str(), now, ids, xyz, facing);
        s_wasHeld = false;
        s_entryPending = false;
        s_heldSince = 0;
        return;
    }
    if (s_every != 0 && now >= s_everyFrom && now <= s_everyTo && now - s_lastWrite >= s_every) {
        char w[64];
        std::snprintf(w, sizeof w, "every %u frames", (unsigned)s_every);
        write_save(w, now, ids, xyz, facing);
    } else if (now - s_lastTimer >= kTimerFrames) {
        s_lastTimer = now;
        write_save("timer", now, ids, xyz, facing);
    }
}

/* ---- hooks (chained: each passes on to the one registered before it first) ---- */
s32 (*s_prevRoomLeave)(u32, s32, s32, s32) = nullptr;
s32 room_leave(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    const s32 r = s_prevRoomLeave != nullptr ? s_prevRoomLeave(room, depth, settled, pauseOpen) : 0;
    tick(room, depth, settled, pauseOpen);
    return r;
}

s32 (*s_prevContinueSpot)(OrreFieldSpot*) = nullptr;
s32 continue_spot(OrreFieldSpot* out) {
    const s32 r = s_prevContinueSpot != nullptr ? s_prevContinueSpot(out) : 0;
    s_armed = true;
    s_controlled = false;
    s_probeHeld = false;
    s_continueAt = pc_vi_heartbeat;
    if (r == 1)
        std::printf("[savecheck] probe: Continue at frame %u goes to the spot: room 0x%X at (%.2f, %.2f, %.2f) facing %d\n",
                    (unsigned)s_continueAt, (unsigned)out->room, out->x, out->y, out->z, (int)out->facingDegrees);
    else
        std::printf("[savecheck] probe: Continue at frame %u goes to the save's own return point\n", (unsigned)s_continueAt);
    return r;
}

} // namespace

/* the flag watch (pc_autosave.cpp passes every flag change on): records only */
extern "C" void pc_savecheck_flag_changed(s32 flagId, u32 oldValue, u32 newValue) {
    if (!s_on) return;
    for (Change& c : s_changes)
        if (c.id == flagId) {
            c.now = newValue;
            c.frame = pc_vi_heartbeat;
            return;
        }
    s_changes.push_back(Change{flagId, oldValue, newValue, pc_vi_heartbeat});
}

/* called once at start-up (pc_platform.c), after pc_save_windows_start, so it chains the room-leave tick and Continue's
 * spot hook after every other listener. Off (nothing chained) unless a switch is set in an automated run */
extern "C" void pc_savecheck_start(void) {
    if (!pc_platform_automated_run()) return;
    const char* dir = SDL_getenv("ORRE_SAVECHECK");
    const char* probe = SDL_getenv("ORRE_SAVECHECK_PROBE");
    s_on = dir != nullptr && dir[0] != '\0';
    s_probe = probe != nullptr && probe[0] == '1';
    if (!s_on && !s_probe) return;
    if (s_on) {
        s_dir = dir;
        while (!s_dir.empty() && (s_dir.back() == '/' || s_dir.back() == '\\')) s_dir.pop_back();
        SDL_CreateDirectory(s_dir.c_str());
        if (const char* t = SDL_getenv("ORRE_SAVECHECK_TEMPLATE")) s_templ = t;
        if (const char* e = SDL_getenv("ORRE_SAVECHECK_EVERY")) {
            /* "<frames>" or "<frames>@<from>-<to>": the dense saves only in that stretch (a known window inside a long
             * sitting, P10's Vander), the candidate moments everywhere */
            char* end = nullptr;
            s_every = (u32)std::strtoul(e, &end, 0);
            if (end != nullptr && *end == '@') {
                s_everyFrom = (u32)std::strtoul(end + 1, &end, 0);
                if (end != nullptr && *end == '-') s_everyTo = (u32)std::strtoul(end + 1, nullptr, 0);
            }
        }
        if (const char* f = SDL_getenv("ORRE_SAVECHECK_FRAMES")) {
            char* end = nullptr;
            s_from = (u32)std::strtoul(f, &end, 0);
            if (end != nullptr && *end == '-') s_to = (u32)std::strtoul(end + 1, nullptr, 0);
        }
        if (const char* rs = SDL_getenv("ORRE_SAVECHECK_ROOMS")) {
            for (const char* p = rs; *p != '\0';) {
                char* end = nullptr;
                const unsigned long v = std::strtoul(p, &end, 16);
                if (end == p) break;
                s_rooms.push_back((u32)v);
                p = *end == ',' ? end + 1 : end;
            }
        }
        std::printf("[savecheck] on: saves into %s (template %s; every %u frames; frames %u-%u; %d room(s) chosen)\n",
                    s_dir.c_str(), s_templ.empty() ? "the slot's own save" : s_templ.c_str(), (unsigned)s_every,
                    (unsigned)s_from, (unsigned)s_to, (int)s_rooms.size());
    }
    if (s_probe) std::printf("[savecheck] probe on: control, holds and people after Continue are logged\n");
    s_prevRoomLeave = OrreHook_RoomLeave;
    OrreHook_RoomLeave = room_leave;
    if (s_probe) {
        s_prevContinueSpot = OrreHook_ContinueSpot;
        OrreHook_ContinueSpot = continue_spot;
    }
}
