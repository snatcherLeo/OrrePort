/*
 * pc_autosave.cpp: OrrePort's autosave (2026-11-20, the project's helper; the design snatcherLeo approved the same day:
 * docs/port/autosave.md, "Oh yeah, let's build it ... Go ahead, build ilt"; how it is built: that page, "How it's built").
 *
 * Why it exists: the GameCube only saves at a PC, so turning the game off (or a crash) loses everything since the last
 * PC. The decided frame (snatcherLeo, 2026-11-19, the project plan's step 3.5): its own slot, never over a manual save, never
 * mid-scene, no switch, off in GameCube-accurate mode, Continue still loads the player's own save.
 *
 * The rule it follows (the project notes, sections 4-5): the save holds the story flags, party, bag,
 * boxes and a return point, but not where people stand or what a scene is doing; a room's setup rebuilds all of that from
 * the flags. So a save is safe exactly when the room could be rebuilt from the flags alone: right after a room was
 * entered and has settled, before any story flag changed in it.
 *
 * When it writes (all on the game thread, from the room manager's per-frame tick, OrreHook_RoomLeave, chained):
 *   1. a room entered: once the field room at the bottom of the room stack has settled (isActive 4, nothing pushed on
 *      it), and every "may save now" signal below has held for kStableFrames frames, and no flag changed since the room
 *      settled;
 *   2. a timer: kTimerSeconds in the same room since it was entered or last autosaved, with no flag changed in the room;
 *   3. at most once every kGapSeconds: a room entry inside that time waits (and is still written once the time is up,
 *      if nothing changed meanwhile), so walking back and forth through a door doesn't write over and over.
 * The "may save now" signals (the softlock notes' section 5): no menu room pushed (the PC, P★DA, bag, evolution and
 * purification rooms, battles: all pushed rooms) and the pause menu closed; no battle (fightIsBattleActive); Wes not held
 * by a scene (people flag 0x80000008, heroMoveInitEvent); no talk / touch script running (the hero step's scripts,
 * counted by pc_save_windows.cpp's OrreHook_HeroEventScript); nothing on screen waiting for a button (a message window, a
 * window reading keys); the field camera on (a scene's scripted camera turns it off); a field room whose spot can be
 * resumed (kind 1, no story area's own return point: the same rule as PC MENU's spot); no known open-scene window from
 * ui/game/save_windows.ini; and a save of this slot in memory (see "Which slot" below). Before all of them: not in
 * Battle Mode (*added 2026-11-20*, battle_mode_why below: the top menu's Battle Mode choice or the game mode variable
 * 0x8AE; Battle Mode's Mt. Battle course is walked in the story's own field rooms, where every other signal can pass).
 * Loading an autosave keeps Battle Mode's record (save section 14) from the player's save (load_into_memory).
 *
 * What it writes: the game's own save format, Slot N\Autosave\autosave.gci, with autosave.ini beside it (the spot: room,
 * room before, entry, position and facing, tied to the .gci's SHA-1; the place name; the time). The one before is kept
 * as autosave.prev.gci / .ini. A new one is written to autosave.tmp first and checked, then the files are renamed into
 * place, so a crash mid-write can't break either. *Decided 2026-11-20 (here; the design said "in the slot's folder
 * beside its .gci"):* a folder of its own, because the memory card opens every .gci in the slot's folder by its header
 * (extern/aurora CardGciFolder.cpp, as Dolphin does) and two copies of the same save there would clash: the card keeps
 * the first by name and skips the other, so an "autosave.gci" beside "Nuzlocke - Colosseum.gci" could even be taken for
 * the player's save. A subfolder is never read by the card (the Backups folder already relies on that).
 *
 * How the file is made, and why not through the game's card task (the design asked to use the card task if it can run
 * quietly): *Decided 2026-11-20 (here):* the card task can't. Its save setup draws 20 random numbers with the game's own
 * generator for the slot's padding (memcardSaveSetup -> memcardFillRandom -> GSrand, memcard.c), which would shift every
 * random roll after it (battle damage, encounters), and it bumps the card's save counter in memory and writes the
 * player's own .gci (its file is the card). So the autosave builds the same bytes itself, the way the card task does:
 *   - on the game thread (well under a millisecond): pc_game_ui.save_capture copies the save image with what the PC's
 *     Save adds before the write (Rui's flag, the last PC's return point, the save count + 1) applied to the copy only
 *     (src/game/pc_game_ui.c), and the spot is read (pc_game_ui.field_spot);
 *   - on a background thread (file reads, ciphers, writes: kept off the frame): the copy's fields swapped to the
 *     card's byte order with the same table memcard.c uses (pc_save_swap_fields), put into the player's .gci in the slot
 *     the game would write next ((newest + 1) % 3, the counter + 1; the other two slots and the card header copied),
 *     sealed and encrypted with pc_save_cipher.c (the game's rules, proven inverse by save_cipher_test), the card time
 *     (+0x28) set to now as the card library does (Aurora's getGCTime), then decrypted again as a check before the swap.
 * The result is a normal save: on a GameCube or in Dolphin it continues from the last PC, as any PC MENU save does; in
 * OrrePort it continues from the spot. The player's own .gci and slot.ini are never written.
 *
 * Which slot (*decided 2026-11-20, here; the design: "the autosave waits for you to pick a slot (the first time you
 * save)"*): only a game that belongs to the slot the card has open: one continued from it (OrreHook_ContinueSpot runs
 * exactly on Continue's YES) or saved into it (a card write, OrreHook_CardTaskEnd). The title's card read forgets it, so
 * a New Game started over a slot's save never autosaves into that slot until its first save.
 *
 * Getting it back: pc_autosave_request_load(slot) arms the next title card read of that slot (LOAD's SAVE FILES list,
 * the hub's Saves page, the hang watch): when the read ends with a save loaded (task 1, result 3), the image in memory is
 * replaced by the autosave's, swapped back to this machine's order; the card's own bookkeeping (its save counter) stays
 * the player's file's, so the next save to the card writes the player's .gci normally. Continue then goes on from the
 * autosave's spot (OrreHook_ContinueSpot, registered before the hang watch's, so the watch arms after it too). Continue
 * itself is unchanged: it loads the player's own save. *Added 2026-11-20:* the autosave's image reaches play only
 * through Continue: a card write before Continue (Options) gets the player's own image (OrreHook_CardTaskStart), and
 * Battle Mode picked instead puts the player's own image back for good (s_playerImage says how and why).
 *
 * Random numbers and the game's state: none drawn, nothing in the game's memory written while playing (likely; checked by
 * the determinism test, docs/port/autosave.md). Off entirely in GameCube-accurate mode (enabled()), and in automated runs
 * unless ORRE_AUTOSAVE=1 (test runs; recordings never write files of their own). Test switches: ORRE_AUTOSAVE_GAP=<s>,
 * ORRE_AUTOSAVE_TIMER=<s> (the minute and the five minutes); ORRE_AUTOSAVE_LOG=1 logs every skipped chance with why.
 *
 * Platforms: plain C++ over SDL's file, thread and time calls and the C library's time functions; the same on Windows,
 * Linux and macOS (likely; built and tested on Windows only). The renames rely on SDL_RenamePath replacing an existing
 * file (MoveFileEx on Windows, rename() on POSIX: both replace).
 * For XD: the same idea carries (likely): XD's rooms have the same setup / main loop / scene structure and its save goes
 * through its own card task; XD saves from its pause menu, so its "may save now" signals and its save layout (field
 * swap table, cipher) are XD's own and must be found first (docs/xd-insights.md).
 */
#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <string>
#include <vector>

#include "pc_autosave.h"
#include "pc_lowmem.h"
#include "pc_options.h"
#include "pc_save_cipher.h"
#include "pc_save_region.h"
#include "pc_saves.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
int pc_platform_automated_run(void);
u32 VIGetRetraceCount(void);
void pc_save_swap_fields(uint8_t* block);          /* pc_save_fields.c: the card's byte order <-> this machine's */
int pc_save_windows_script_depth(void);           /* pc_save_windows.cpp: the hero step's talk / touch scripts running */
int pc_save_windows_window_open(unsigned room);   /* pc_save_windows.cpp: a known open-scene window in this room */
void pc_savecheck_flag_changed(s32 flagId, u32 oldValue, u32 newValue); /* pc_savecheck.cpp: the sweep's flag watch */
}
#include "pc_game_ui.h"

namespace {

constexpr u32 kBlock = 0x1DFD0;            /* the save image (save-data.md), what the PC's Save writes */
constexpr size_t kGciSize = 0x40 + 0x6000 + 3 * 0x1E000; /* a Colosseum .gci: header, card header, three slots */
constexpr int kStableFrames = 2;           /* every signal holds this long first (a scene starting on entry shows) */
int s_gapSeconds = 60, s_timerSeconds = 300; /* the design's "once a minute" and "every 5 minutes" (test switches) */

/* ---- the switch ---- */
int s_log = -1; /* ORRE_AUTOSAVE_LOG=1 */
bool verbose() {
    if (s_log < 0) {
        const char* v = SDL_getenv("ORRE_AUTOSAVE_LOG");
        s_log = v != nullptr && v[0] == '1';
    }
    return s_log == 1;
}

bool enabled() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate() || !pc_saves_ready()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* v = SDL_getenv("ORRE_AUTOSAVE");
    return v != nullptr && v[0] == '1';
}

/* ---- small helpers ---- */
uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

/* the card library's clock: local wall time in seconds since 2000-01-01, the console's clock (Aurora's getGCTime,
 * extern/aurora/lib/card/Util.cpp, which every save the card writes gets): the same arithmetic, so the SAVE FILES help
 * box shows an autosave's date the way it shows a save's. C library calls: the same on every OS */
uint32_t gc_time_now() {
    time_t now = std::time(nullptr);
    std::tm local{}, utc{};
#ifdef _WIN32 /* the thread-safe forms: localtime_s / gmtime_s on Windows, localtime_r / gmtime_r elsewhere */
    localtime_s(&local, &now);
    gmtime_s(&utc, &now);
#else
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
#endif
    const time_t dst = local.tm_isdst == 1 ? 3600 : 0;
    const time_t diff = now - std::mktime(&utc);
    return (uint32_t)((long long)now + diff + dst - 0x386D4380LL);
}

/* "20261120_031500" in local time, the Backups folders' own form (pc_saves.c), so the hang watch can compare the two */
std::string when_text(long long unixSeconds) {
    SDL_DateTime dt{};
    if (!SDL_TimeToDateTime((SDL_Time)unixSeconds * 1000000000LL, &dt, true)) return "";
    char b[32];
    std::snprintf(b, sizeof b, "%04d%02d%02d_%02d%02d%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    return b;
}

std::string hex_sha1(const uint8_t* data, size_t len) {
    uint8_t d[20];
    pc_save_sha1(data, len, d);
    char h[41];
    for (int i = 0; i < 20; ++i) std::snprintf(h + i * 2, 3, "%02x", d[i]);
    return h;
}

std::string folder_of(int slot) {
    char dir[1200];
    pc_saves_slot_dir(slot, dir, sizeof dir);
    return std::string(dir) + "Autosave/";
}

/* "key=value" lines of a small setup file */
std::string ini_get(const std::string& text, const char* key) {
    const std::string k = std::string(key) + "=";
    size_t at = 0;
    while (at < text.size()) {
        size_t nl = text.find('\n', at);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(at, nl - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, k.size(), k) == 0) return line.substr(k.size());
        at = nl + 1;
    }
    return "";
}

bool parse_spot(const std::string& line, std::string& sha, OrreFieldSpot& s) {
    char want[41] = {0};
    unsigned room = 0, prev = 0, entry = 0;
    int facing = 0;
    s = OrreFieldSpot{};
    if (std::sscanf(line.c_str(), "%40s %X %X %u %f %f %f %d", want, &room, &prev, &entry, &s.x, &s.y, &s.z, &facing) != 8 ||
        room == 0)
        return false;
    sha = want;
    s.room = room, s.prevRoom = prev, s.entry = entry, s.facingDegrees = facing;
    return true;
}

/* ---- the writer thread ---- */
struct Job {
    int slot = 0;
    std::string gciPath;   /* the slot's folder; then (writer thread) the player's own .gci, read only: the template for
                            * the card header and the other slots */
    std::string dir;       /* Slot N\Autosave\ */
    std::vector<uint8_t> block; /* the captured image, this machine's byte order */
    OrreFieldSpot spot{};
    std::string place, trigger;
    unsigned room = 0;
    /* *Added 2026-11-20 (the save-check sweep, pc_savecheck.cpp):* a numbered test save instead of the slot's autosave:
     * written straight to these two files (no autosave.prev rotation, not counted as an autosave), its card header and
     * other slots taken from `templ` when set (a sitting that hasn't saved yet has no .gci of its own), and `extra`
     * appended to the .ini (the sweep's notes). Empty: the autosave itself */
    std::string outGci, outIni, templ, extra;
};

SDL_Mutex* s_mutex = nullptr;
SDL_Condition* s_cond = nullptr;
SDL_Thread* s_thread = nullptr;
/* the jobs waiting for the writer, oldest first. *Changed 2026-11-20 (the save-check sweep):* a queue instead of one job,
 * because the sweep writes several saves close together; the autosave itself still has at most one in it (s_busy) */
std::deque<Job> s_queue;
std::atomic<bool> s_busy{false};
std::atomic<int> s_written{0}; /* how many autosaves this run (the tests count them) */

/* the slot's own save file: the newest .gci directly in its folder (the rule pc_saves.c's slot_save_file follows: one
 * save per folder; the pattern doesn't reach into Autosave\ or Backups\). Here, on the writer thread, so the game thread
 * never waits for it (*changed 2026-11-20:* first through pc_saves_get on the game thread, which re-reads and decrypts a
 * save whose file just changed: 5.9 ms in the frame of the first autosave after a save, P01) */
std::string slot_gci(const std::string& slotDir) {
    int count = 0;
    char** files = SDL_GlobDirectory(slotDir.c_str(), "*.gci", SDL_GLOB_CASEINSENSITIVE, &count);
    std::string best;
    Sint64 newest = -1;
    for (int i = 0; files != nullptr && i < count; ++i) {
        const std::string path = slotDir + files[i];
        SDL_PathInfo info;
        if (SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE && (Sint64)info.modify_time > newest)
            newest = (Sint64)info.modify_time, best = path;
    }
    SDL_free(files);
    return best;
}

bool write_job(Job& j) {
    const Uint64 t0 = SDL_GetTicksNS();
    size_t len = 0;
    j.gciPath = !j.templ.empty() ? j.templ : slot_gci(j.gciPath);
    uint8_t* gci = j.gciPath.empty() ? nullptr : static_cast<uint8_t*>(SDL_LoadFile(j.gciPath.c_str(), &len));
    if (gci == nullptr || len < kGciSize) {
        std::printf("[autosave] slot %d: the save file can't be read (%s): nothing written\n", j.slot, j.gciPath.c_str());
        SDL_free(gci);
        return false;
    }
    std::vector<uint8_t> out(gci, gci + len);
    SDL_free(gci);
    static uint8_t plain[PC_SAVE_SLOT_SIZE], enc[PC_SAVE_SLOT_SIZE], check[PC_SAVE_SLOT_SIZE]; /* this thread only */
    const int newest = pc_save_gci_newest_slot(out.data(), out.size(), plain);
    if (newest < 0) {
        std::printf("[autosave] slot %d: the save file has no valid slot: nothing written\n", j.slot);
        return false;
    }
    /* the slot as memcardSaveSetup makes it: valid / initialized / 0 / 0, the counter one past the newest, the block in
     * the card's byte order; the padding after the block (the game's 20 random bytes) stays the newest slot's */
    const uint32_t counter = be32(plain + 4) + 1;
    plain[0] = 1, plain[1] = 1, plain[2] = 0, plain[3] = 0;
    put_be32(plain + 4, counter);
    std::memcpy(plain + PC_SAVE_BLOCK_AT, j.block.data(), kBlock);
    pc_save_swap_fields(plain + PC_SAVE_BLOCK_AT);
    pc_save_slot_encrypt(plain, enc);
    const int target = (newest + 1) % 3;
    std::memcpy(out.data() + PC_GCI_HEADER + PC_GCI_CARD_HEADER + (size_t)target * PC_SAVE_SLOT_SIZE, enc, PC_SAVE_SLOT_SIZE);
    put_be32(out.data() + 0x28, gc_time_now()); /* the directory entry's time (pc_saves.h: PcSaveDetails.card_time) */
    /* the check before anything is replaced: the new slot decrypts with a good digest and is the one the game will load */
    if (!pc_save_slot_decrypt(enc, check) || pc_save_gci_newest_slot(out.data(), out.size(), check) != target) {
        std::printf("[autosave] slot %d: the new file failed its own check: nothing replaced\n", j.slot);
        return false;
    }
    SDL_CreateDirectory(j.dir.c_str());
    const bool copy = !j.outGci.empty(); /* a save-check test save (see Job) */
    const std::string tmpGci = copy ? j.outGci + ".tmp" : j.dir + "autosave.tmp";
    const std::string tmpIni = copy ? j.outIni + ".tmp" : j.dir + "autosave.ini.tmp";
    const std::string gciNow = copy ? j.outGci : j.dir + "autosave.gci", iniNow = copy ? j.outIni : j.dir + "autosave.ini";
    const std::string gciPrev = j.dir + "autosave.prev.gci", iniPrev = j.dir + "autosave.prev.ini";
    char spot[200];
    std::snprintf(spot, sizeof spot, "%s %X %X %u %.9g %.9g %.9g %d", hex_sha1(out.data(), out.size()).c_str(),
                  (unsigned)j.spot.room, (unsigned)j.spot.prevRoom, (unsigned)j.spot.entry, j.spot.x, j.spot.y, j.spot.z,
                  (int)j.spot.facingDegrees);
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    const long long unixNow = (long long)(now / 1000000000LL);
    const std::string ini = "# OrrePort's autosave of this save (written by OrrePort; docs: autosave). The .gci beside it is a "
                            "normal Colosseum save;\n# spot = where Continue goes on in OrrePort (the .gci's SHA-1 first)\n"
                            "version=1\ntime=" + std::to_string(unixNow) + "\nspot=" + spot + "\nplace=" + j.place +
                            "\ntrigger=" + j.trigger + "\n" + j.extra;
    if (!SDL_SaveFile(tmpGci.c_str(), out.data(), out.size()) || !SDL_SaveFile(tmpIni.c_str(), ini.data(), ini.size())) {
        std::printf("[autosave] slot %d: %s can't be written (%s): the previous autosave stays\n", j.slot, j.dir.c_str(),
                    SDL_GetError());
        return false;
    }
    /* the swap: the current pair becomes the previous one, then the new pair takes its place (each rename replaces) */
    SDL_PathInfo pi;
    if (!copy && SDL_GetPathInfo(gciNow.c_str(), &pi)) {
        SDL_RenamePath(gciNow.c_str(), gciPrev.c_str());
        if (SDL_GetPathInfo(iniNow.c_str(), &pi)) SDL_RenamePath(iniNow.c_str(), iniPrev.c_str());
    }
    if (!SDL_RenamePath(tmpGci.c_str(), gciNow.c_str()) || !SDL_RenamePath(tmpIni.c_str(), iniNow.c_str())) {
        std::printf("[autosave] slot %d: the new autosave couldn't be put in place (%s)\n", j.slot, SDL_GetError());
        return false;
    }
    if (copy) {
        std::printf("[savecheck] written %s (%s; card slot %d, counter %u; %.1f ms on the writer thread)\n", j.outGci.c_str(),
                    j.trigger.c_str(), target, (unsigned)counter, (double)(SDL_GetTicksNS() - t0) / 1e6);
        return true;
    }
    ++s_written;
    std::printf("[autosave] slot %d: written (%s, room 0x%X at (%.1f, %.1f, %.1f), \"%s\"; card slot %d, counter %u; "
                "%.1f ms on the writer thread; autosave %d this run)\n",
                j.slot, j.trigger.c_str(), j.room, j.spot.x, j.spot.y, j.spot.z, j.place.c_str(), target, (unsigned)counter,
                (double)(SDL_GetTicksNS() - t0) / 1e6, s_written.load());
    return true;
}

int SDLCALL writer_main(void*) {
    /* low priority: it only reads, ciphers and writes files, and must never take time from the game's own threads (SDL's
     * call: a thread priority on Windows, a nice / scheduling hint on Linux and macOS; likely enough everywhere) */
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    for (;;) {
        SDL_LockMutex(s_mutex);
        while (s_queue.empty()) SDL_WaitCondition(s_cond, s_mutex);
        Job j = std::move(s_queue.front());
        s_queue.pop_front();
        SDL_UnlockMutex(s_mutex);
        const bool copy = !j.outGci.empty();
        write_job(j);
        if (!copy) s_busy = false; /* only the autosave's own job holds the next autosave back */
    }
    return 0;
}

bool writer_start() {
    if (s_thread != nullptr) return true;
    s_mutex = SDL_CreateMutex();
    s_cond = SDL_CreateCondition();
    if (s_mutex == nullptr || s_cond == nullptr) return false;
    /* one thread for the whole run, made at the first autosave and left waiting for the next job (detached: it ends
     * with the process; a job half written then leaves only autosave.tmp behind, never a broken autosave.gci) */
    s_thread = SDL_CreateThread(writer_main, "orre_autosave", nullptr);
    if (s_thread != nullptr) SDL_DetachThread(s_thread);
    return s_thread != nullptr;
}

/* ---- the game-thread side ---- */
uint8_t* s_capture = nullptr; /* below 4 GB (pc_game_ui.save_capture) */
int s_boundSlot = 0;          /* the slot whose save the game in memory is (0: none, e.g. a new game not saved yet) */
u32 s_baseRoom = 0;           /* the room at the bottom of the room stack, last seen settled */
u32 s_enteredAt = 0;          /* the retrace it settled at */
u32 s_lastWriteAt = 0;        /* the retrace of the last autosave (0: none yet this run) */
bool s_entryPending = false;  /* this room's entry autosave is still to come */
bool s_flagsChanged = false;  /* a flag changed since the room settled */
int s_stable = 0;             /* frames every signal has held */
/* the room a Continue goes to is not an entry to autosave (*decided 2026-11-20, from the first test run, where the
 * Continue's own room wrote one at once):* its state is exactly the save just loaded, so there is nothing new to keep,
 * and if that save is one that gets stuck (a scene's open window the table doesn't know yet), an autosave there would
 * replace the good autosave the hang watch offers with a copy of the stuck one */
bool s_skipNextEntry = false;
std::string s_lastWhy;        /* the last skip reason logged (ORRE_AUTOSAVE_LOG) */

/* the autosave just loaded into memory (pc_autosave_request_load), for Continue's spot */
int s_loadPending = 0;
int s_loadedSlot = 0;
bool s_loadedSpotOk = false;
OrreFieldSpot s_loadedSpot{};
std::string s_loadedWhen; /* the loaded autosave's time ("YYYYMMDD_HHMMSS"), which file it was */
/* the game being played came from this slot's autosave of this time (*added 2026-11-20*, the save-check sweep's finding):
 * s_loadedSlot is spent at Continue (it only says "use the autosave's spot"), so the hang watch couldn't tell that the
 * game it watches came from the autosave, and offered that same stuck autosave back. This mark lasts from Continue until
 * the next title read or the next save to the card */
int s_playingSlot = 0;
std::string s_playingWhen;

/* *Added 2026-11-20 (the Battle Mode guard, part 2: "the autosave image only ever reaches play through
 * Continue"):* the autosave's image is put into memory at the title's read (so the Continue screen shows the autosave's
 * trainer, time and counts), but between that read and Continue's YES the player can still pick something else on the
 * top menu, and two of those write the image in memory to the player's own .gci: Options (card task 7, the sound mode
 * lives in the save) and Battle Mode (its record saves 5 / 6 / 0xD and the Ho-Oh gift's 4 write the whole image, story
 * included). Without this, either would write the autosave's story into the player's save, which the design promises
 * never happens. So the player's own image is kept from the read, and:
 *   - before any card write while the autosave waits for Continue on the top menu (Options), the player's own image
 *     goes back for the write and the autosave's comes back after it (swap_for_write / the card-task end);
 *   - once Battle Mode is entered instead of Continue, the player's own image goes back for good and the load is
 *     dropped: Battle Mode registers its team from the image and saves it, so it must see the player's save;
 *   - any other way out of the top menu (New Game: the game makes a new image itself) only drops the load.
 * In each case section 14 (Battle Mode's record) stays as it is in memory: it is the card's already (load_into_memory)
 * plus whatever Battle Mode changed since. Bytes the game changed outside section 14 after the load (Options' sound
 * mode) are carried into both copies, so neither swap undoes them. The room ids: the title screen 0x384 (where the read
 * runs) and the top menu 0x3A1 (rooms.md). All platforms */
constexpr u32 kRoomTitle = 0x384, kRoomTopMenu = 0x3A1;
std::vector<uint8_t> s_playerImage; /* the player's own image as the card read it (this machine's order) */
std::vector<uint8_t> s_autoImage;   /* the image the load put in memory */
const std::vector<uint8_t>* s_inMemory = nullptr; /* which of the two is in memory now (nullptr: no load to track) */
u32 s_tickRoom = 0;                 /* the room at the bottom of the stack, every frame (tick) */

u32 frames(int seconds) { return (u32)seconds * 60u; } /* the retrace count runs at 60 a second */

/* *Added 2026-11-20 (the Battle Mode guard; snatcherLeo, 2026-11-20: "maybe no autosave during battle mode, that would be
 * simple", decided in docs/port/autosave.md "It never saves while"):* Battle Mode runs on the story save's image and its
 * Mt. Battle course is walked in the story's own field room 0x4C with the player in control, so every other signal can
 * say "may save now" there. The save-check sweep found the autosave held back in Battle Mode only by the slot binding
 * ("no save of this slot in memory yet"), an indirect guard: one of Battle Mode's own card writes (kind 4: the save after
 * its Ho-Oh gift) would have bound it. This is the direct one, from the game's own state
 * (pc_game_ui.battle_mode, src/game/pc_game_ui.c: the top menu's Battle Mode choice and the game mode variable 0x8AE).
 * Returns the reason for the log, or nullptr outside Battle Mode. All platforms */
const char* battle_mode_why() {
    const int bits = pc_game_ui.battle_mode != nullptr ? pc_game_ui.battle_mode() : 0;
    switch (bits & 3) {
    case 1: return "Battle Mode (the top menu's Battle Mode choice)";
    case 2: return "Battle Mode (a Battle Mode run: game mode variable 0x8AE)";
    case 3: return "Battle Mode (the top menu's Battle Mode choice; a run: variable 0x8AE)";
    default: return nullptr;
    }
}

void skip(const char* why) {
    s_stable = 0;
    if (verbose() && s_lastWhy != why) {
        s_lastWhy = why;
        std::printf("[autosave] waiting in room 0x%X: %s\n", (unsigned)s_baseRoom, why);
    }
}

/* why not now ("" = it may save now). Every read here is the game's own state, read only */
const char* blocked(s32 pauseOpen, uint32_t ids[3], float xyz[3], int32_t* facing) {
    const PcGameUi& g = pc_game_ui;
    if (s_boundSlot == 0 || s_boundSlot != pc_saves_current()) return "no save of this slot in memory yet";
    if (pauseOpen) return "the pause menu is open";
    if (g.battle_active != nullptr && g.battle_active()) return "a battle";
    if (g.hero_event_locked != nullptr && g.hero_event_locked()) return "a scene holds the player";
    if (pc_save_windows_script_depth() > 0) return "a talk or touch script is running";
    if (g.player_prompt_up != nullptr && g.player_prompt_up() != 0) return "a message or window is waiting";
    float hdy[3];
    if (g.field_camera_state != nullptr && (g.field_camera_state(hdy) & 0xFF) == 0) return "the field camera is off (a scene's camera)";
    if (g.field_spot == nullptr || !g.field_spot(ids, xyz, facing)) return "not a field room whose spot can be resumed";
    if (pc_save_windows_window_open(s_baseRoom)) return "a known open-scene window (save_windows.ini)";
    if (s_busy) return "the last autosave is still being written";
    return "";
}

void fire(const char* trigger, const uint32_t ids[3], const float xyz[3], int32_t facing) {
    const Uint64 t0 = SDL_GetTicksNS();
    char slotDir[1200];
    pc_saves_slot_dir(s_boundSlot, slotDir, sizeof slotDir); /* text only; the writer finds the save file in it */
    if (s_capture == nullptr) s_capture = static_cast<uint8_t*>(pc_lowmem_alloc(kBlock + 0x20));
    if (s_capture == nullptr || !writer_start() || pc_game_ui.save_capture(s_capture) != kBlock) {
        skip("the save image can't be copied");
        return;
    }
    char place[160] = "";
    if (pc_game_ui.place_text != nullptr) pc_game_ui.place_text(ids[0], place, sizeof place);
    Job job;
    job.slot = s_boundSlot;
    job.gciPath = slotDir;
    job.dir = folder_of(s_boundSlot);
    job.block.assign(s_capture, s_capture + kBlock);
    job.spot.room = ids[0], job.spot.prevRoom = ids[1], job.spot.entry = ids[2];
    job.spot.x = xyz[0], job.spot.y = xyz[1], job.spot.z = xyz[2], job.spot.facingDegrees = facing;
    job.place = place;
    job.trigger = trigger;
    job.room = ids[0];
    SDL_LockMutex(s_mutex);
    s_queue.push_back(std::move(job));
    s_busy = true;
    SDL_SignalCondition(s_cond);
    SDL_UnlockMutex(s_mutex);
    s_lastWriteAt = VIGetRetraceCount();
    if (s_lastWriteAt == 0) s_lastWriteAt = 1;
    s_lastWhy.clear();
    std::printf("[autosave] %s in room 0x%X (frame %u): the image copied in %.3f ms on the game thread; the file is "
                "written in the background\n",
                trigger, (unsigned)ids[0], (unsigned)VIGetRetraceCount(), (double)(SDL_GetTicksNS() - t0) / 1e6);
}

bool tracking();                          /* below, with the load (s_playerImage) */
void player_back_for_good(const char* why);
void forget_load();

void tick(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    s_tickRoom = room;
    /* an autosave loaded at the title that Continue didn't take: the game left the title and the top menu another way
     * (s_playerImage says why). Battle Mode: the player's own image back for good; anything else (New Game, whose image
     * the game made itself): the load is only dropped. Every frame, before anything else, so it happens on the first
     * frame of the new room, long before a Battle Mode menu can register a team or save */
    if (tracking() && room != 0 && room != kRoomTitle && room != kRoomTopMenu) {
        if (battle_mode_why() != nullptr) {
            player_back_for_good("Battle Mode was picked instead of Continue");
        } else {
            std::printf("[autosave] slot %d: room 0x%X entered without Continue: the autosave load is dropped (the image "
                        "is the game's own)\n", s_loadedSlot, (unsigned)room);
            forget_load();
        }
    }
    if (depth != 0 || !settled) { /* a menu room or battle pushed, or a room change under way */
        if ((room == s_baseRoom || depth != 0) && verbose() && enabled())
            skip(depth != 0 ? "a menu room or battle is pushed on the field" : "a room change");
        s_stable = 0;
        return;
    }
    const u32 now = VIGetRetraceCount();
    if (room != s_baseRoom) {
        /* a new room at the bottom of the stack (a battle's or menu's return is the same room: not an entry) */
        s_baseRoom = room;
        s_enteredAt = now;
        s_entryPending = !s_skipNextEntry;
        if (s_skipNextEntry && enabled() && verbose())
            std::printf("[autosave] room 0x%X: the room Continue went to (the save just loaded): no entry autosave\n", (unsigned)room);
        s_skipNextEntry = false;
        s_flagsChanged = false;
        s_stable = 0;
        s_lastWhy.clear();
    }
    if (!enabled()) return;
    /* Battle Mode first, before the flag check (Battle Mode changes flags all the time, which would hide the reason in
     * the log): never an autosave there, not even once it ends in a field room (the room after it is the top menu) */
    if (const char* bm = battle_mode_why()) {
        s_entryPending = false;
        skip(bm);
        return;
    }
    if (s_flagsChanged) {
        if (s_entryPending && verbose()) std::printf("[autosave] room 0x%X: a flag changed before it could save: waits for the next room\n", (unsigned)room);
        s_entryPending = false;
        return; /* the timer needs an unchanged room too: nothing more in this room */
    }
    uint32_t ids[3] = {0, 0, 0};
    float xyz[3] = {0, 0, 0};
    int32_t facing = 0;
    const char* why = blocked(pauseOpen, ids, xyz, &facing);
    if (why[0] != '\0') {
        skip(why);
        return;
    }
    if (++s_stable < kStableFrames) return;
    const bool gapOk = s_lastWriteAt == 0 || now - s_lastWriteAt >= frames(s_gapSeconds);
    if (s_entryPending) {
        if (!gapOk) {
            skip("less than the minimum time since the last autosave");
            s_stable = kStableFrames; /* still steady: write as soon as the time is up */
            return;
        }
        s_entryPending = false;
        fire("room entry", ids, xyz, facing);
        return;
    }
    const u32 since = s_lastWriteAt != 0 && s_lastWriteAt > s_enteredAt ? s_lastWriteAt : s_enteredAt;
    if (gapOk && now - since >= frames(s_timerSeconds)) fire("timer", ids, xyz, facing);
}

/* ---- hooks ---- */
s32 (*s_prevRoomLeave)(u32, s32, s32, s32) = nullptr;
s32 room_leave(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    const s32 r = s_prevRoomLeave != nullptr ? s_prevRoomLeave(room, depth, settled, pauseOpen) : 0;
    tick(room, depth, settled, pauseOpen);
    return r;
}

void flag_changed(s32 id, u32 oldValue, u32 newValue) {
    if (!s_flagsChanged && verbose() && s_entryPending && enabled())
        std::printf("[autosave] flag %d changed (%u -> %u) in room 0x%X\n", (int)id, (unsigned)oldValue, (unsigned)newValue,
                    (unsigned)s_baseRoom);
    s_flagsChanged = true;
}

/* the autosave's image put into memory in place of the save the card just read (this machine's byte order) */
bool load_into_memory(int slot) {
    static PcAutosaveInfo a;
    if (!pc_autosave_info(slot, &a) || pc_game_ui.save_image == nullptr) return false;
    size_t len = 0;
    uint8_t* gci = static_cast<uint8_t*>(SDL_LoadFile(a.path, &len));
    static uint8_t plain[PC_SAVE_SLOT_SIZE];
    const bool ok = gci != nullptr && pc_save_gci_newest_slot(gci, len, plain) >= 0;
    std::string sha = ok ? hex_sha1(gci, len) : std::string();
    SDL_free(gci);
    uint32_t size = 0;
    uint8_t* image = const_cast<uint8_t*>(pc_game_ui.save_image(&size));
    if (!ok || image == nullptr || size != kBlock) return false;
    pc_save_swap_fields(plain + PC_SAVE_BLOCK_AT);
    /* *Changed 2026-11-20 (the save-check sweep's open question, the project notes section 7):* Battle
     * Mode's record (save section 14) stays the one just read from the card; everything else is the autosave's.
     * Why: an autosave is a whole save image, so one written before a Battle Mode session holds Battle Mode's record as
     * it was then; loaded after the session, it would bring the older record back and the next save would write it over
     * the newer one (a re-registered team, edited rules, courses opened or cleared, a suspended run lost, or a finished
     * one back). Section 14 belongs to Battle Mode alone (verified from the code, docs/engine/save-data.md "Who owns
     * section 14"): only Battle Mode writes it (and every one of its changes is saved to the card by Battle Mode
     * itself), and the story never reads it; its Mt. Battle memo is Battle Mode's run, not the story's climb (the
     * story's streak, variable 2673, is in the flags). The autosave can't be written in Battle Mode, so the card's record
     * is always the newer one. The story's flags, party, bag and coupons all come from the autosave, as before. The
     * image in memory is the player's save the card just read (task 1, result 3), in this machine's order, as is
     * `plain` now: the span is the same in both (taken from the game's section getters). All platforms */
    uint32_t keepAt = 0;
    const uint32_t keepLen = pc_game_ui.battle_record_span != nullptr ? pc_game_ui.battle_record_span(&keepAt) : 0;
    const uint8_t* from = plain + PC_SAVE_BLOCK_AT;
    s_playerImage.assign(image, image + kBlock); /* the player's own, kept until Continue (see s_playerImage) */
    if (keepLen != 0 && keepAt + keepLen <= kBlock) {
        std::memcpy(image, from, keepAt);
        std::memcpy(image + keepAt + keepLen, from + keepAt + keepLen, kBlock - keepAt - keepLen);
        std::printf("[autosave] slot %d: Battle Mode's record (save section 14, +0x%X, 0x%X bytes) kept from the save "
                    "just read; the rest is the autosave's\n", slot, (unsigned)keepAt, (unsigned)keepLen);
    } else {
        /* no section span (a test program without the game): the whole image, the earlier behaviour */
        std::memcpy(image, from, kBlock);
        std::printf("[autosave] slot %d: Battle Mode's record can't be located: the whole image is the autosave's\n", slot);
    }
    s_autoImage.assign(image, image + kBlock);
    s_inMemory = &s_autoImage;
    /* the spot only when it belongs to this very file (as slot.ini's spot= does for the player's .gci) */
    std::string want;
    s_loadedSpotOk = parse_spot(a.spot, want, s_loadedSpot) && want == sha;
    s_loadedSlot = slot;
    s_loadedWhen = a.when;
    std::printf("[autosave] slot %d: the autosave%s (%s) loaded in place of the save; Continue goes on from %s\n", slot,
                a.is_prev ? " before the last one" : "", a.when, s_loadedSpotOk ? "its spot" : "its last PC (no spot)");
    return true;
}

/* ---- the autosave image only through Continue (s_playerImage above says why) ---- */

/* the image in memory, or nullptr */
uint8_t* image_now() {
    uint32_t size = 0;
    uint8_t* image = pc_game_ui.save_image != nullptr ? const_cast<uint8_t*>(pc_game_ui.save_image(&size)) : nullptr;
    return image != nullptr && size == kBlock ? image : nullptr;
}

/* calls fn(i) for every block offset outside section 14 (the whole block when the span is unknown) */
template <typename F> void outside_s14(F fn) {
    uint32_t at = 0;
    uint32_t len = pc_game_ui.battle_record_span != nullptr ? pc_game_ui.battle_record_span(&at) : 0;
    if (len == 0 || at + len > kBlock) at = kBlock, len = 0;
    for (uint32_t i = 0; i < at; ++i) fn(i);
    for (uint32_t i = at + len; i < kBlock; ++i) fn(i);
}

/* the image in memory becomes `to` outside section 14, after the bytes the game changed since the last swap (against
 * the copy that was in memory) are carried into both copies. Returns how many bytes were carried */
int swap_to(uint8_t* image, std::vector<uint8_t>& to) {
    int carried = 0;
    const std::vector<uint8_t>& was = *s_inMemory;
    outside_s14([&](uint32_t i) {
        if (image[i] != was[i]) {
            s_playerImage[i] = s_autoImage[i] = image[i];
            ++carried;
        }
    });
    outside_s14([&](uint32_t i) { image[i] = to[i]; });
    s_inMemory = &to;
    return carried;
}

/* the load is over: Continue took it, or the game left the top menu another way */
void forget_load() {
    s_loadedSlot = 0;
    s_loadedSpotOk = false;
    s_inMemory = nullptr;
    std::vector<uint8_t>().swap(s_playerImage);
    std::vector<uint8_t>().swap(s_autoImage);
}

/* tracking a load: an autosave in memory (or swapped out for a write) that Continue hasn't taken yet */
bool tracking() { return s_loadedSlot != 0 && s_inMemory != nullptr && s_playerImage.size() == kBlock; }

/* the player's own image back for good: Battle Mode picked instead of Continue */
void player_back_for_good(const char* why) {
    uint8_t* image = image_now();
    if (!tracking() || image == nullptr) return;
    const int carried = swap_to(image, s_playerImage);
    std::printf("[autosave] slot %d: %s: the player's own save is back in memory (section 14 as it is, %d byte(s) the "
                "game changed since the load kept); the autosave load is dropped\n", s_loadedSlot, why, carried);
    forget_load();
}

/* OrreHook_CardTaskStart (chained; pc_card.c chains after it): before a card write while the autosave waits for
 * Continue. Reads (kinds 1-3) put a save into memory themselves and need nothing */
void (*s_prevCardTaskStart)(s32) = nullptr;
void card_task_start(s32 kind) {
    if (s_prevCardTaskStart != nullptr) s_prevCardTaskStart(kind);
    if (kind >= 1 && kind <= 3) return;
    if (!tracking()) return;
    uint8_t* image = image_now();
    if (image == nullptr) return;
    if (s_tickRoom != kRoomTopMenu && battle_mode_why() != nullptr) {
        player_back_for_good("a Battle Mode save");
    } else if (s_tickRoom == kRoomTopMenu || s_tickRoom == kRoomTitle) {
        /* Options' save (task 7) on the top menu: the player's own image for the write, the autosave's after it */
        if (s_inMemory == &s_autoImage) {
            const int carried = swap_to(image, s_playerImage);
            std::printf("[autosave] slot %d: card task %d on the top menu: the player's own save goes to the card, not the "
                        "autosave (%d byte(s) changed since the load kept in both)\n", s_loadedSlot, (int)kind, carried);
        }
    } else {
        /* anywhere else the game made its own image (New Game): never touched, the load is only dropped */
        std::printf("[autosave] slot %d: card task %d in room 0x%X: the autosave load is dropped\n", s_loadedSlot, (int)kind,
                    (unsigned)s_tickRoom);
        forget_load();
    }
}

void (*s_prevCardTaskEnd)(s32, s32) = nullptr;
void card_task_end(s32 kind, s32 result) {
    if (s_prevCardTaskEnd != nullptr) s_prevCardTaskEnd(kind, result);
    /* after a write the swap above made: the autosave's image comes back, so Continue still plays it */
    if (tracking() && s_inMemory == &s_playerImage) {
        if (uint8_t* image = image_now()) {
            const int carried = swap_to(image, s_autoImage);
            std::printf("[autosave] slot %d: card task %d done (result %d): the autosave is back in memory for Continue "
                        "(%d byte(s) the write changed kept)\n", s_loadedSlot, (int)kind, (int)result, carried);
        }
    }
    if (!pc_saves_ready()) return;
    const int slot = pc_saves_current();
    /* *Added 2026-11-20 (the Battle Mode guard, battle_mode_why above):* a card task while Battle Mode runs is Battle
     * Mode's own: its record saves (kind 6, from a temporary image; kind 5 after registering; kind 0xD the rules), and
     * also the story's own write kind 4: the save after Battle Mode's Ho-Oh gift (menuSaveLoadReceiveGift, gba_conv.c:
     * verified) and, likely, the PokéCoupon shop's (memory-card.md). None of them makes the game in memory a story game
     * to follow, but "kind 4 with result 4" is exactly what binds a story save below, so the gift's save would have
     * bound the slot in the middle of Battle Mode. So nothing is bound here, and anything bound is dropped. The title's
     * own read (kind 1) comes before the top menu, never inside Battle Mode, and goes on below */
    if (kind != 1) {
        if (const char* bm = battle_mode_why()) {
            if (enabled())
                std::printf("[autosave] slot %d: card task %d (result %d) in %s: not a story save, autosaves don't "
                            "follow it\n", slot, (int)kind, (int)result, bm);
            s_boundSlot = 0;
            return;
        }
    }
    if (kind == 1) {
        /* the title's read: what is in memory now is the slot's save, but not played until Continue (New Game may
         * follow); an armed autosave load happens here */
        s_boundSlot = 0;
        forget_load(); /* a new read replaced the image: no earlier load to track */
        s_playingSlot = 0; /* a new read: whatever is played next is not the autosave played before */
        s_playingWhen.clear();
        if (s_loadPending != 0) {
            const int want = s_loadPending;
            s_loadPending = 0;
            if (result == 3 && want == slot && enabled() && !load_into_memory(slot))
                std::printf("[autosave] slot %d: the autosave couldn't be loaded: the save itself stays loaded\n", slot);
        }
        return;
    }
    if (result == 4 && (kind == 4 || kind == 9 || kind == 10 || kind == 11)) {
        if (s_boundSlot != slot && enabled()) std::printf("[autosave] slot %d: a save was made: autosaves follow it\n", slot);
        s_boundSlot = slot;
        s_playingSlot = 0; /* the game in memory is now the player's own save */
        s_playingWhen.clear();
    }
}

s32 (*s_prevContinueSpot)(OrreFieldSpot*) = nullptr;
s32 continue_spot(OrreFieldSpot* out) {
    const int slot = pc_saves_ready() ? pc_saves_current() : 0;
    s32 r;
    if (s_loadedSlot != 0 && s_loadedSlot == slot) {
        /* the autosave is in memory: its own spot, or its own return point (never the player's slot.ini spot, which
         * belongs to the player's .gci). If a write left the player's image in (never seen: the card-task end puts the
         * autosave back), the autosave's goes back first: Continue is the one way it reaches play */
        if (tracking() && s_inMemory == &s_playerImage)
            if (uint8_t* image = image_now()) swap_to(image, s_autoImage);
        r = s_loadedSpotOk ? 1 : 0;
        if (r == 1) *out = s_loadedSpot;
        std::printf("[autosave] Continue from slot %d's autosave: %s\n", slot,
                    r == 1 ? "its spot" : "the save's own return point (the last PC)");
        if (r == 1)
            std::printf("[autosave] the spot: room 0x%X at (%.1f, %.1f, %.1f), facing %d\n", (unsigned)out->room, out->x,
                        out->y, out->z, (int)out->facingDegrees);
        forget_load(); /* Continue took it: the copies aren't needed any more */
        s_playingSlot = slot;
        s_playingWhen = s_loadedWhen;
    } else {
        r = s_prevContinueSpot != nullptr ? s_prevContinueSpot(out) : 0;
        s_playingSlot = 0;
        s_playingWhen.clear();
    }
    s_boundSlot = slot; /* Continue: the game is now this slot's save */
    s_skipNextEntry = true;
    if (enabled() && slot != 0) std::printf("[autosave] slot %d continued: autosaves follow it\n", slot);
    return r;
}

} // namespace

/* ---- the C interface (pc_autosave.h) ---- */
extern "C" int pc_autosave_info(int slot, PcAutosaveInfo* out) {
    std::memset(out, 0, sizeof *out);
    if (slot <= 0 || !pc_saves_ready()) return 0;
    const std::string dir = folder_of(slot);
    for (int prev = 0; prev < 2; ++prev) {
        const std::string gci = dir + (prev ? "autosave.prev.gci" : "autosave.gci");
        const std::string ini = dir + (prev ? "autosave.prev.ini" : "autosave.ini");
        SDL_PathInfo pi;
        if (!SDL_GetPathInfo(gci.c_str(), &pi) || pi.type != SDL_PATHTYPE_FILE) continue;
        char trainer[64];
        unsigned play = 0;
        if (!pc_saves_read_info(gci.c_str(), trainer, sizeof trainer, &play)) continue; /* damaged: the one before */
        size_t len = 0;
        char* text = static_cast<char*>(SDL_LoadFile(ini.c_str(), &len));
        const std::string t = text != nullptr ? std::string(text, len) : std::string();
        SDL_free(text);
        out->exists = 1;
        out->is_prev = prev;
        std::snprintf(out->path, sizeof out->path, "%s", gci.c_str());
        out->play_seconds = play;
        out->unix_time = std::atoll(ini_get(t, "time").c_str());
        if (out->unix_time <= 0) out->unix_time = (long long)(pi.modify_time / 1000000000LL);
        std::snprintf(out->when, sizeof out->when, "%s", when_text(out->unix_time).c_str());
        std::snprintf(out->place, sizeof out->place, "%s", ini_get(t, "place").c_str());
        std::snprintf(out->spot, sizeof out->spot, "%s", ini_get(t, "spot").c_str());
        std::string sha;
        OrreFieldSpot s{};
        out->spot_room = parse_spot(out->spot, sha, s) ? s.room : 0;
        return 1;
    }
    return 0;
}

extern "C" int pc_autosave_on(void) { return enabled() ? 1 : 0; }

extern "C" void pc_autosave_request_load(int slot) {
    s_loadPending = slot;
    std::printf("[autosave] slot %d: its autosave loads at the next read of the save\n", slot);
}

/* for the hub's card (pc_hub.cpp show_save, 2026-11-21): what PLAY will start. Hub thread only, before the game runs,
 * the same thread that arms it, so no lock */
extern "C" int pc_autosave_armed(void) { return s_loadPending; }

extern "C" void pc_autosave_cancel_request(void) {
    if (s_loadPending != 0) std::printf("[autosave] slot %d: its autosave load is cancelled (the save itself plays)\n", s_loadPending);
    s_loadPending = 0;
}

extern "C" int pc_autosave_loaded_now(void) { return s_loadedSlot; }

extern "C" int pc_autosave_playing_from(int slot, const char* when) {
    return slot != 0 && slot == s_playingSlot && when != nullptr && s_playingWhen == when ? 1 : 0;
}

extern "C" int pc_autosave_written(void) { return s_written.load(); }

/* OrreHook_FlagChanged's listener (every flag write that changes a value, gs_range_8018FE30.c). The hook variable lives
 * in pc_game_main.c (the exe only, not in the test programs this library is also linked into), so the exe attaches this
 * there, before the event log, which chains it (pc_event_log.c) */
extern "C" void pc_autosave_flag_changed(s32 flagId, u32 oldValue, u32 newValue) {
    flag_changed(flagId, oldValue, newValue);
    /* the save-check sweep's own flag watch (pc_savecheck.cpp, 2026-11-20): passed on from here, because this is the one
     * listener the exe attaches (see above); it only records, and does nothing unless ORRE_SAVECHECK is set */
    pc_savecheck_flag_changed(flagId, oldValue, newValue);
}

/* the save-check sweep's writer (pc_autosave.h says what for): the same bytes the autosave writes, through the same
 * background thread, to the given files */
extern "C" int pc_autosave_write_copy(const unsigned char* block, const char* templateGci, const char* outGci,
                                      const char* outIni, const PcAutosaveCopySpot* spot, const char* place,
                                      const char* trigger, const char* extra) {
    if (block == nullptr || outGci == nullptr || outIni == nullptr || spot == nullptr || !writer_start()) return 0;
    Job job;
    job.slot = pc_saves_ready() ? pc_saves_current() : 0;
    if (templateGci == nullptr || templateGci[0] == '\0') {
        /* no template given: the slot's own save file, as the autosave uses */
        char slotDir[1200];
        pc_saves_slot_dir(job.slot, slotDir, sizeof slotDir);
        job.gciPath = slotDir;
    } else {
        job.templ = templateGci;
    }
    std::string out = outGci;
    const size_t cut = out.find_last_of("/\\");
    job.dir = cut == std::string::npos ? std::string("./") : out.substr(0, cut + 1);
    job.block.assign(block, block + kBlock);
    job.spot.room = spot->room, job.spot.prevRoom = spot->prev_room, job.spot.entry = spot->entry;
    job.spot.x = spot->x, job.spot.y = spot->y, job.spot.z = spot->z, job.spot.facingDegrees = spot->facing;
    job.place = place != nullptr ? place : "";
    job.trigger = trigger != nullptr ? trigger : "";
    job.room = spot->room;
    job.outGci = outGci;
    job.outIni = outIni;
    job.extra = extra != nullptr ? extra : "";
    SDL_LockMutex(s_mutex);
    s_queue.push_back(std::move(job));
    SDL_SignalCondition(s_cond);
    SDL_UnlockMutex(s_mutex);
    return 1;
}

/* called once at start-up (pc_platform.c), after pc_pc_screen_start and before pc_save_windows_start: it chains the
 * room-leave tick, the card-task hook and Continue's spot hook (PC MENU's), so the hang watch, registered next, wraps
 * the spot hook and arms after an autosave's Continue as well */
extern "C" void pc_autosave_start(void) {
    if (const char* v = SDL_getenv("ORRE_AUTOSAVE_GAP")) if (std::atoi(v) > 0) s_gapSeconds = std::atoi(v);
    if (const char* v = SDL_getenv("ORRE_AUTOSAVE_TIMER")) if (std::atoi(v) > 0) s_timerSeconds = std::atoi(v);
    s_prevRoomLeave = OrreHook_RoomLeave;
    OrreHook_RoomLeave = room_leave;
    s_prevCardTaskEnd = OrreHook_CardTaskEnd;
    OrreHook_CardTaskEnd = card_task_end;
    /* before every card task: the player's own image for a write while an autosave waits for Continue (pc_card.c's
     * own task-start hook chains this one: it is registered later, when the card opens) */
    s_prevCardTaskStart = OrreHook_CardTaskStart;
    OrreHook_CardTaskStart = card_task_start;
    s_prevContinueSpot = OrreHook_ContinueSpot;
    OrreHook_ContinueSpot = continue_spot;
    /* a test switch (automated runs only): ORRE_AUTOSAVE_LOAD=<slot> arms the first read of that slot to load its
     * autosave, the same call SAVE FILES' autosave row, the hub's button and the hang watch make, so a pad script can
     * test the loading and Continue's spot without driving those screens */
    if (const char* v = SDL_getenv("ORRE_AUTOSAVE_LOAD"))
        if (pc_platform_automated_run() && std::atoi(v) > 0) pc_autosave_request_load(std::atoi(v));
    std::printf("[autosave] ready (%s; once every %d s at most, timer %d s)\n",
                pc_option_gamecube_accurate() ? "off: GameCube-accurate mode" : enabled() ? "on" : "off in this run",
                s_gapSeconds, s_timerSeconds);
}
