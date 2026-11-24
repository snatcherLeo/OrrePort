/*
 * pc_party_ui.cpp: step 3.7's Smooth Party UI (the project's build helper, 2026-11-20; decided in
 * docs/port/tweaks-37-research.md section 6b and "snatcherLeo's answers (2026-11-20)" 8: "All the portraits appear together, as
 * fast as makes sense, for a polished look ... the first frame if it can be done safely"; rebuilt 2026-11-21 after snatcherLeo's
 * hand check, section 6d: "I personally don't like the pause, and think it's strange, we should look to get rid of that,
 * it shouldn't pause at all"; how the portrait cache works: docs/engine/menus.md "Party portraits"; the hooks:
 * include/pc/orre_hooks.h, "Step 3.7's Smooth Party UI").
 *
 * The console's pop-in (verified from the code, research 6b): portraits live in a 24-slot cache kept in
 * least-recently-used order; battles, the PC and other screens push the party's out over time. When the party screen
 * opens it asks for its six portraits as one chain (a cached one is only moved to the front; a missing one is read from
 * the disc, and its completion asks for the next: one read at a time) and opens at once; each portrait is drawn only
 * once its slot is ready, so the missing ones flicker in one by one.
 *
 * What this does now ("smooth_party_screen" in pc_options.c, the "Smooth Party UI" row):
 *   - **Stand-ins (2026-11-21, the default):** OrrePort reads the portrait archive (poke_face.fsys) from the disc once,
 *     at start-up, into its own memory. When the party screen opens, each party portrait the game hasn't loaded yet is
 *     unpacked from that copy into a stand-in texture of OrrePort's own (pc_lowmem, run through the game's own
 *     GStextureLoad, kept for the run), and while the game's slot is still loading the portrait is drawn from the
 *     stand-in (OrreHook_FaceStandIn, in faceDraw, where the party list draws its portraits by key, and in
 *     faceDrawPokemon). So the list opens at once, as on the GameCube, with all six portraits on its first
 *     frame, with fast loading on or off. The game's own loads, their order and timing, its cache and GSres never see the
 *     stand-in: once the game's slot is ready its own texture is drawn (the same picture; checked once per portrait, the
 *     "matches" log line). Look only.
 *   - **The hold (fallback; 2026-11-20's level 1, OrreHook_FaceDrawHold):** if a stand-in can't be made (the archive
 *     wasn't read, an entry is missing) or with the test switch ORRE_PARTY_UI_HOLD_ONLY=1, a ready portrait of the party
 *     screen is held back until all of the screen's are ready (at most kHoldCap frames), so they appear on the same
 *     frame, a moment after the list. Look only.
 *   - **Never a wait.** 2026-11-20's level 2 (the list waited about 18 frames for its portraits, OrreHook_PartyPortraitWait
 *     returning 1) is gone: the hook now only arms the screen's keys and always lets the list open.
 * The field party's screen only (from the pause menu, the bag's "use on which Pokémon", the PC's party view); a battle's
 * party screen (its Pokémon come from the battle's own records) is left as the game has it.
 *
 * Why not load the portraits into the game's cache ahead of time (the first idea, snatcherLeo: "load the party's portraits
 * ahead of time, for example when the pause menu opens")? Research 6d: the preload itself draws no random numbers, but
 * what the cache holds when a battle starts decides how many frames its send-out waits for its portraits, and the battle
 * draws random numbers every one of those frames. A portrait cached by an early load (the pause menu opened, POKéMON not
 * picked, then a battle) would shorten that wait and shift every later roll: exactly why the 2026-10-19 background
 * preload was retired. So the copy ahead of time lives outside the game.
 *
 * Retired earlier (2026-11-20): the first Smooth Party UI (built 2026-10-19, pc_party_portraits_frame in
 * src/game/pc_debug_game.c, loading the party's portraits into the cache while walking; its switch answers 0).
 *
 * GameCube-accurate mode and recordings: off (pc_option is 0 there; automated value 0). Both parts are look only and
 * could stay on in accurate mode once the the reference playthrough replay check passes with them forced on (a decision for snatcherLeo: research
 * 6b, "Proof"). ORRE_SMOOTH_PARTY_SCREEN=1 turns it on in a test run.
 *
 * Random numbers: none drawn here or by the portrait code (verified by search, research 6b), and the stand-ins touch no
 * game memory but their own pc_lowmem buffers (the draw itself reads them). ORRE_RNG_COUNT over a pause-menu-to-party
 * sequence: research 6d.
 *
 * Platforms: plain C++ over the game's own portrait cache and Aurora's disc reader (aurora_dvd_read_entry, nod); no OS
 * calls; Windows, Linux and macOS the same (likely: built and run on Windows only).
 * For XD: XD's party screen uses the same portrait-cache family (menuFace* in its menu code, likely); the two hook places
 * would be its party screen's request and its face draw, and its portrait archive would be read the same way (guess
 * until XD is checked). docs/xd-insights.md has the entry.
 */
#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "pc_lowmem.h"
#include "pc_options.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
u32 VIGetRetraceCount(void);            /* the screen's refresh count: the hold's time limit */
int pc_platform_automated_run(void);    /* pc_platform.c */
}
#ifdef PC_HAVE_AURORA_DVD
#include <aurora/dvd.h> /* aurora_dvd_read_entry: the archive read on the calling thread, outside the game's drive */
#include <dolphin/dvd.h>
#endif
#include "pc_game_ui.h"

namespace {

/* the hold's cap: generous against a load that never finishes (a portrait can't be read, the disc is slow), short
 * enough that the screen never seems stuck */
constexpr u32 kHoldCap = 120;      /* frames a ready portrait may be held back (two seconds) */
constexpr u32 kFaceBytes = 0x6EC0; /* a portrait texture file, as the game's cache slot holds it (_menuFaceLoad's copy) */

u16 s_keys[6];
u32 s_ids[6];
int s_count = 0;
bool s_armed = false;  /* the stand-ins and the hold apply to s_keys */
bool s_hold = false;   /* this opening holds instead (a stand-in couldn't be made, or the test switch) */
u32 s_asked = 0;       /* ORRE_PARTY_UI_LOG: the screen's portraits already logged on this opening */
u32 s_armAt = 0;

/* ---- the archive copy (read once) and the stand-ins (made once per portrait, kept for the run) ---- */
struct Entry {
    uint32_t dataOff, stored;
    bool packed;
};
std::vector<uint8_t> s_arc;                    /* poke_face.fsys, whole (about 4.5 MB) */
std::unordered_map<uint32_t, Entry> s_entries; /* entry id -> where its data is */
bool s_arcTried = false;
std::unordered_map<uint32_t, void*> s_standIns; /* entry id -> its texture (GStextureLoad'ed, pc_lowmem) */
std::unordered_map<uint32_t, bool> s_checked;   /* entry id -> compared with the game's own once */
/* entry id -> the unpacked file's length: the picture is shorter than the game's cache slot (a portrait is 42 x 84, 0x3A40
 * bytes with its header; the slot holds 0x6EC0, its tail left from whatever was there before), so only this much is
 * compared */
std::unordered_map<uint32_t, uint32_t> s_sizes;

bool on() { return pc_game_ui.ready && pc_game_ui.party_face_key != nullptr && pc_option("smooth_party_screen") != 0; }

uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

/* the game's LZSS (4 KB window, start 0xFEE, 16-byte "LZSS" header; the same decoder as pc_title_word.cpp and
 * pc_region_text.cpp, kept here so this file reads on its own) */
std::vector<uint8_t> lzss(const uint8_t* src, size_t n) {
    std::vector<uint8_t> out;
    out.reserve(kFaceBytes);
    uint8_t win[0x1000] = {};
    int w = 0xFEE;
    size_t i = 0x10;
    unsigned flags = 0;
    while (i < n) {
        flags >>= 1;
        if (!(flags & 0x100)) {
            flags = src[i++] | 0xFF00;
            if (i >= n) break;
        }
        if (flags & 1) {
            const uint8_t b = src[i++];
            out.push_back(b), win[w] = b, w = (w + 1) & 0xFFF;
        } else {
            if (i + 1 >= n) break;
            const uint8_t a = src[i], b = src[i + 1];
            i += 2;
            const int off = a | ((b << 4) & 0xF00);
            for (int k = 0; k < (b & 0xF) + 3; ++k) {
                const uint8_t c = win[(off + k) & 0xFFF];
                out.push_back(c), win[w] = c, w = (w + 1) & 0xFFF;
            }
        }
    }
    return out;
}

/* poke_face.fsys read whole and its table indexed by entry id (each entry's first word: the id the game's portrait
 * records name and the file system matches, docs/engine/fsys-catalog.md). Through aurora_dvd_read_entry: Aurora's own
 * nod handle on the calling thread, never the game's drive (no drive-timing model, no read counters, nothing queued
 * ahead of the game's reads). Called at start-up (before the game runs), or on the first party screen if the switch was
 * off then */
void read_archive() {
    if (s_arcTried) return;
#ifdef PC_HAVE_AURORA_DVD
    const auto start = std::chrono::steady_clock::now();
    s32 entry = DVDConvertPathToEntrynum("/poke_face.fsys");
    if (entry < 0) entry = DVDConvertPathToEntrynum("poke_face.fsys");
    if (entry < 0) return; /* no disc open yet (the hub adds one later): tried again at the first party screen */
    s_arcTried = true;
    auto take = [](void* user, const u8* data, size_t n) {
        auto* v = static_cast<std::vector<uint8_t>*>(user);
        v->insert(v->end(), data, data + n);
    };
    if (aurora_dvd_read_entry(entry, 1u << 20, take, &s_arc) <= 0 || s_arc.size() < 0x20 ||
        std::memcmp(s_arc.data(), "FSYS", 4) != 0) {
        std::printf("[party_ui] poke_face.fsys couldn't be read: the party screen holds its portraits instead\n");
        s_arc.clear();
        return;
    }
    const uint32_t count = be32(&s_arc[0x0C]), block = be32(&s_arc[0x18]);
    const uint32_t table = block + 4 <= s_arc.size() ? be32(&s_arc[block]) : 0;
    for (uint32_t k = 0; table != 0 && k < count && table + 4 * (k + 1) <= s_arc.size(); ++k) {
        const uint32_t eo = be32(&s_arc[table + 4 * k]);
        if ((uint64_t)eo + 0x28 > s_arc.size()) continue;
        const uint8_t* e = &s_arc[eo];
        const uint32_t dataOff = be32(e + 0x04), flags = be32(e + 0x0C), stored = be32(e + 0x14);
        if ((uint64_t)dataOff + stored > s_arc.size() || stored == 0) continue;
        s_entries[be32(e)] = Entry{dataOff, stored, (flags & 0x80000000u) != 0};
    }
    const long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::printf("[party_ui] poke_face.fsys read for the stand-in portraits: %zu bytes, %zu portraits, %ld ms\n", s_arc.size(),
                s_entries.size(), ms);
#else
    s_arcTried = true;
    std::printf("[party_ui] no disc reader in this build: the party screen holds its portraits instead\n");
#endif
}

/* the stand-in for a portrait id, made now if needed: the entry unpacked into OrrePort's own low memory (the game's
 * textures must sit below 4 GB, 32-byte aligned) and run through the game's GStextureLoad, which makes the header
 * native, turns its picture offsets into pointers and builds its GX objects, exactly as for the copy the game's cache
 * gets. Nothing is registered with GSres. NULL when it can't be made */
void* stand_in(uint32_t id) {
    const auto have = s_standIns.find(id);
    if (have != s_standIns.end()) return have->second;
    const auto it = s_entries.find(id);
    if (it == s_entries.end() || pc_game_ui.texture_load == nullptr) return nullptr;
    const Entry& e = it->second;
    std::vector<uint8_t> data = e.packed ? lzss(&s_arc[e.dataOff], e.stored)
                                         : std::vector<uint8_t>(s_arc.begin() + e.dataOff, s_arc.begin() + e.dataOff + e.stored);
    if (data.size() < 0x80) return nullptr;
    const size_t size = (std::max<size_t>(data.size(), kFaceBytes) + 31) & ~(size_t)31;
    auto* buf = static_cast<uint8_t*>(pc_lowmem_alloc(size));
    if (buf == nullptr) return nullptr;
    std::memset(buf, 0, size);
    std::memcpy(buf, data.data(), data.size());
    void* tex = pc_game_ui.texture_load(buf);
    if (tex == nullptr) return nullptr;
    s_standIns[id] = tex;
    s_sizes[id] = (uint32_t)data.size();
    return tex;
}

int ready_count() {
    int n = 0;
    for (int i = 0; i < s_count; ++i) n += pc_game_ui.face_ready(s_keys[i]) != 0;
    return n;
}

int index_of(u16 key) {
    for (int i = 0; i < s_count; ++i)
        if (s_keys[i] == key) return i;
    return -1;
}

/* OrreHook_PartyPortraitWait: called once as the field party screen opens (right after it asked for its portraits).
 * Arms the screen's keys, makes the missing portraits' stand-ins, and never asks the list to wait (returns 0) */
s32 party_portrait_wait(s32 fromBattle, s32 waited) {
    if (waited != 0) return 0; /* never reached: the first answer is always 0 */
    s_armed = false;
    s_hold = false;
    s_count = 0;
    s_asked = 0;
    if (fromBattle != 0 || !on()) return 0;
    for (int i = 0; i < 6; ++i) {
        const u16 key = pc_game_ui.party_face_key(i);
        if (key != 0) {
            s_ids[s_count] = pc_game_ui.face_resource_id != nullptr ? pc_game_ui.face_resource_id(key) : 0;
            s_keys[s_count++] = key;
        }
    }
    const int ready = ready_count();
    if (ready == s_count) {
        std::printf("[party_ui] the party screen opens with all %d portraits cached\n", s_count);
        return 0; /* nothing to smooth */
    }
    s_armed = true;
    s_armAt = VIGetRetraceCount();
    static int s_holdOnly = -1;
    if (s_holdOnly < 0) {
        /* ORRE_PARTY_UI_HOLD_ONLY=1 (a test switch for hand checks, 2026-11-20): no stand-ins, only the hold */
        const char* v = SDL_getenv("ORRE_PARTY_UI_HOLD_ONLY");
        s_holdOnly = v != nullptr && v[0] == '1';
    }
    read_archive(); /* normally done at start-up already */
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < s_count && !s_holdOnly; ++i) {
        if (pc_game_ui.face_ready(s_keys[i])) continue;
        /* one missing stand-in: the whole screen holds, so they still appear together */
        if (s_ids[i] == 0 || stand_in(s_ids[i]) == nullptr) s_hold = true;
    }
    if (s_holdOnly) s_hold = true;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (s_hold)
        std::printf("[party_ui] the party screen opens with %d of %d portraits ready (%s): the list opens now, the "
                    "portraits appear together when ready\n",
                    ready, s_count, s_holdOnly ? "ORRE_PARTY_UI_HOLD_ONLY" : "a stand-in couldn't be made");
    else
        std::printf("[party_ui] the party screen opens with %d of %d portraits ready: %d stand-in(s) drawn until the game's "
                    "own are loaded (%.2f ms)\n",
                    ready, s_count, s_count - ready, ms);
    return 0;
}

/* OrreHook_FaceStandIn: a party screen portrait the game hasn't loaded yet, drawn from its stand-in */
void* face_stand_in(u16 key, u32 resourceId) {
    if (!s_armed || s_hold) return nullptr;
    const int i = index_of(key);
    const auto it = i >= 0 && s_ids[i] == resourceId ? s_standIns.find(resourceId) : s_standIns.end();
    void* tex = it != s_standIns.end() ? it->second : nullptr;
    /* ORRE_PARTY_UI_LOG=1 (a test switch, log only): each portrait's first ask on this opening, answered or not */
    static int s_log = -1;
    if (s_log < 0) {
        const char* v = SDL_getenv("ORRE_PARTY_UI_LOG");
        s_log = v != nullptr && v[0] == '1';
    }
    if (s_log && i >= 0 && !(s_asked & (1u << i))) {
        s_asked |= 1u << i;
        std::printf("[party_ui] key 0x%04X (id 0x%08X) asked at retrace %u: %s\n", (unsigned)key, (unsigned)resourceId,
                    (unsigned)VIGetRetraceCount(), tex != nullptr ? "stand-in drawn" : "no stand-in");
    }
    return tex;
}

/* once per portrait id, when the game's own texture is ready while a stand-in was made: the two pictures' texel bytes
 * compared (after the 0x80-byte header, which differs only in pointers and the GX-object marker), so a stand-in that
 * ever differed from the game's own would be named in the log */
void check_against_game(int i) {
    const uint32_t id = s_ids[i];
    if (s_checked[id]) return;
    const auto it = s_standIns.find(id);
    if (it == s_standIns.end() || pc_game_ui.resource_find == nullptr) return;
    const uint8_t* mine = static_cast<const uint8_t*>(it->second);
    const uint8_t* game = static_cast<const uint8_t*>(pc_game_ui.resource_find(id));
    if (game == nullptr || game == mine) return;
    s_checked[id] = true;
    const uint32_t size = s_sizes.count(id) ? s_sizes[id] : kFaceBytes;
    const uint32_t n = size > 0x80 && size <= kFaceBytes ? size - 0x80 : kFaceBytes - 0x80;
    uint32_t firstDiff = n, diffs = 0;
    for (uint32_t k = 0; k < n; ++k)
        if (mine[0x80 + k] != game[0x80 + k]) {
            if (firstDiff == n) firstDiff = k;
            ++diffs;
        }
    if (diffs == 0)
        std::printf("[party_ui] stand-in 0x%04X (id 0x%08X) matches the game's own portrait (%u texel bytes; retrace %u)\n",
                    (unsigned)s_keys[i], (unsigned)id, (unsigned)n, (unsigned)VIGetRetraceCount());
    else
        std::printf("[party_ui] stand-in 0x%04X (id 0x%08X) DIFFERS FROM the game's own portrait: %u of %u texel bytes, "
                    "the first at +0x%X (%02X %02X %02X %02X / game %02X %02X %02X %02X)\n",
                    (unsigned)s_keys[i], (unsigned)id, (unsigned)diffs, (unsigned)n, (unsigned)firstDiff + 0x80,
                    mine[0x80 + firstDiff], mine[0x81 + firstDiff], mine[0x82 + firstDiff], mine[0x83 + firstDiff],
                    game[0x80 + firstDiff], game[0x81 + firstDiff], game[0x82 + firstDiff], game[0x83 + firstDiff]);
}

/* OrreHook_FaceDrawHold: the game's own portrait is ready. With stand-ins it is drawn (and checked once against its
 * stand-in); in the hold it waits for the rest of the screen's */
s32 face_draw_hold(u16 key) {
    if (!s_armed) return 0;
    const int i = index_of(key);
    if (i < 0) return 0;
    if (!s_hold) {
        check_against_game(i);
        if (ready_count() == s_count) {
            s_armed = false;
            std::printf("[party_ui] all %d portraits are the game's own now\n", s_count);
        }
        return 0;
    }
    if (ready_count() == s_count) {
        s_armed = false;
        std::printf("[party_ui] all %d portraits shown together\n", s_count);
        return 0;
    }
    if (VIGetRetraceCount() - s_armAt > kHoldCap) {
        s_armed = false;
        std::printf("[party_ui] a portrait didn't load in time: the ready ones are shown\n");
        return 0;
    }
    return 1;
}

}  // namespace

/* called once at start-up (pc_platform.c, after the disc is open and before the game runs); each hook asks the switch
 * when called, so a Settings change applies at once. The archive is read here, on this thread, while nothing else
 * reads the disc: skipped in automated runs unless the switch is on there (test runs stay as they were) */
extern "C" void pc_party_ui_start(void) {
    OrreHook_PartyPortraitWait = party_portrait_wait;
    OrreHook_FaceDrawHold = face_draw_hold;
    OrreHook_FaceStandIn = face_stand_in;
    if (!pc_platform_automated_run() || pc_option("smooth_party_screen") != 0) read_archive();
}
