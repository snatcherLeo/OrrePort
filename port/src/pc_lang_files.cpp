/*
 * pc_lang_files.cpp: files from the language discs (language piece 5, 2026-11-15;
 * docs/port/sequences-per-language.md).
 *
 * Why: after the text (piece 1-3) and the menu pictures (piece 4), the last words the game shows in English are in
 * whole files: the credits movie, the title-screen demo (Japanese only) and Battle Mode's round titles and arena intros
 * (19 small WZX archives). The European and Japanese discs carry their own versions (verified 2026-11-15, the three
 * discs compared). The game opens those files by path through the SDK's DVDOpen, which Aurora serves from the US disc.
 *
 * How: Aurora's overlay files (aurora/dvd.h, aurora_dvd_overlay_files; already in Aurora, unused by the port until now)
 * let the port answer for chosen disc paths with its own open / read / seek. For the game's language, this file
 * registers:
 *   - movie/staffroll.thp: the language disc's credits (the European one for fr / de / it / es, one video for all five
 *     European languages; the Japanese one for ja), read from that disc in pieces as the movie streams;
 *   - movie/autodemo01.thp: the Japanese disc's title demo, for ja only (the European disc's is the US one);
 *   - the 19 wzx_battle_name_* / wzx_colo_intro_* archives: the language's own (European: wzx_<fre/ger/ita/spa>_...;
 *     Japanese: the same names), held in memory (6-41 KB each). The US program asks for an archive by its id (the
 *     archive header's +0x08, looked up in GSfsys.toc) and then for its one entry by the entry id; a European language
 *     archive has other ids (German round 1: archive 0x07A0, entry 0x14E22000; US 0x065E, 0x10FB2000) and a longer
 *     entry name ("ger_battle_name_01"), so the copy gets the US archive's id, entry id and name (the entry's data
 *     never mentions them: verified by a byte search on round 1, docs). The US values are read from the US disc's own
 *     file once, before anything is overlaid.
 * Each language's set is made once and kept (they are small; movies are only a disc handle), so a language change in
 * Options just registers another set. Aurora rebuilds its file table under its lock; files already open keep reading
 * what they opened.
 *
 * Fail safe: no language disc, a file missing, or an archive that isn't one WZX entry as measured: that file stays the
 * US disc's (logged). English registers nothing (the US disc's files exactly). ORRE_NO_LANGUAGE_FILES=1 turns it off,
 * for side-by-side pictures.
 *
 * Modes: the files follow the language in every mode, like the text and the word pictures (look only; no random
 * numbers). A European round title or credits video of another length could change frame counts: the determinism test
 * checks it (likely harmless: no input is read during either).
 * *Updated 2026-11-19:* the console-like disc timing (pc_dvd.c) times every read of an overlaid archive as the read
 * of the US archive it stands in for (pc_lang_files_disc_timing, TimingPiece below), and the game's ARAM cache reserves
 * and evicts as for the US archive (OrreHook_FsysCacheRoom, CacheRoom below), so swapping an entry costs no frames
 * (verified: P03 and P07 replays identical with the Japanese look on and off); the two movies are timed at the US
 * file's place on the disc but by their own reads (their frames differ).
 *
 * Threads: Aurora calls open / read / seek on its disc thread, any time; each language disc has its own nod disc and
 * partition handles here (never pc_region_text.cpp's background reader's), and a mutex per disc around every nod call.
 *
 * Platforms: plain C++ over nod; Windows, Linux and macOS alike (verified only on Windows).
 */
#include "pc_region_look.h" /* the Japanese release look's table (3.5): its "entry" rows are served from here */
#include "pc_region_text.h"
#include "pc_game_ui.h" /* fsys_cache_forget: the game's cached copies of the overlaid archives (below) */

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#ifdef PC_HAVE_AURORA_DVD
#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <nod.h>
#endif

extern "C" const char* pc_game_text_language(void);

/* the archive cache's room hooks (orre_hooks.h; set below while a language's archives are overlaid) and the place that
 * keeps a bigger piece's extra bytes (pc_arq.c); *added 2026-11-19* */
extern "C" {
#include "dolphin/types.h"
extern u32 (*OrreHook_FsysCacheRoom)(u32 size, u32 fileHandle, u32 key1, u32 key2);
extern void (*OrreHook_FsysCacheReserved)(u32 aram, u32 room, u32 size);
void pc_arq_short_block(u32 aram, u32 room, u32 size);
}

#ifdef PC_HAVE_AURORA_DVD
namespace {

/* the 19 Battle Mode sequence archives, by their US names (without ".fsys") */
const char* const kSequences[] = {
    "wzx_battle_name_01", "wzx_battle_name_01_scenario", "wzx_battle_name_02", "wzx_battle_name_02_scenario",
    "wzx_battle_name_03", "wzx_battle_name_04", "wzx_battle_name_05", "wzx_battle_name_06",
    "wzx_battle_name_semi", "wzx_battle_name_semi_scenario", "wzx_battle_name_final", "wzx_battle_name_final_scenario",
    "wzx_colo_intro_ancient", "wzx_colo_intro_bottom", "wzx_colo_intro_casino", "wzx_colo_intro_crater",
    "wzx_colo_intro_cylinder", "wzx_colo_intro_earth", "wzx_colo_intro_water",
};

uint32_t be32(const std::vector<uint8_t>& b, size_t o) {
    return o + 4 <= b.size() ? (uint32_t)b[o] << 24 | (uint32_t)b[o + 1] << 16 | (uint32_t)b[o + 2] << 8 | b[o + 3] : 0;
}
void put32(std::vector<uint8_t>& b, size_t o, uint32_t v) {
    b[o] = (uint8_t)(v >> 24), b[o + 1] = (uint8_t)(v >> 16), b[o + 2] = (uint8_t)(v >> 8), b[o + 3] = (uint8_t)v;
}

/* an archive's total as the ARAM cache's eviction check sees it (slot->totalDecompSize, the slot's +0x20): the
 * slot starts with a copy of the archive's 0x40-byte header, so for an archive on the disc it is the header's +0x20,
 * the archive's length (only archives of external files sum their entries instead: none is overlaid). 0 if it isn't
 * an archive. *Added 2026-11-19* (CacheRoom below; verified: the first try summed the entries and changed nothing) */
uint32_t archive_total(const std::vector<uint8_t>& a) {
    if (a.size() < 0x40 || std::memcmp(a.data(), "FSYS", 4) != 0) return 0;
    return be32(a, 0x20);
}

/* one FSYS archive with exactly one entry: where its archive id, entry id and entry name are (fsys-catalog.md: count
 * at +0x0C, the entry-table block at +0x18, the table's first word the first entry's place; an entry: id at +0, name
 * offset at +0x24). False if it isn't one */
struct OneEntry {
    uint32_t archiveId = 0, entryId = 0, entryAt = 0, nameAt = 0;
    std::string name;
    /* for the disc timing (2026-11-19): the archive's length, its table's size (+0x1C, what the loader reads after the
     * 0x40-byte header), the entry's data offset (+4) and stored size (+0x14) */
    uint32_t size = 0, tocSize = 0, dataOff = 0, stored = 0, total = 0;
};
bool one_entry(const std::vector<uint8_t>& a, OneEntry& out) {
    if (a.size() < 0x80 || std::memcmp(a.data(), "FSYS", 4) != 0 || be32(a, 0x0C) != 1) return false;
    const uint32_t blk = be32(a, 0x18), tab = be32(a, blk), eo = be32(a, tab);
    if (blk == 0 || tab == 0 || eo == 0 || (size_t)eo + 0x50 > a.size()) return false;
    const uint32_t noff = be32(a, eo + 0x24);
    if (noff == 0 || noff >= a.size()) return false;
    const auto* s = reinterpret_cast<const char*>(a.data() + noff);
    const size_t n = strnlen(s, a.size() - noff);
    if (n == 0 || noff + n >= a.size()) return false;
    out.archiveId = be32(a, 0x08), out.entryId = be32(a, eo), out.entryAt = eo, out.nameAt = noff;
    out.name.assign(s, n);
    out.size = (uint32_t)a.size(), out.tocSize = be32(a, 0x1C), out.dataOff = be32(a, eo + 4);
    out.stored = be32(a, eo + 0x14);
    out.total = archive_total(a);
    return true;
}

/* a language disc, opened on first use: its own nod handles and a mutex for every call on them */
struct Disc {
    std::mutex m;
    std::string path;
    NodHandle* disc = nullptr;
    NodHandle* part = nullptr;
    std::map<std::string, std::pair<uint32_t, uint32_t>> files; /* lower-case name -> FST index, size */
};
Disc s_discs[2]; /* 0 the European, 1 the Japanese */

uint32_t list_files(uint32_t index, NodNodeKind kind, const char* name, uint32_t size, void* user) {
    if (kind == NOD_NODE_KIND_FILE) {
        std::string n = name;
        for (char& c : n) c = (char)SDL_tolower((unsigned char)c);
        static_cast<Disc*>(user)->files.emplace(n, std::make_pair(index, size)); /* names are unique on these discs */
    }
    return index + 1;
}

/* open the disc if it isn't (caller holds d.m); false without it */
bool disc_open(Disc& d, int jp) {
    const std::string path = pc_region_disc_path(jp);
    if (path.empty()) return false;
    if (d.part != nullptr && d.path == path) return true;
    if (d.part != nullptr) nod_free(d.part), d.part = nullptr;
    if (d.disc != nullptr) nod_free(d.disc), d.disc = nullptr;
    d.files.clear();
    if (nod_disc_open(path.c_str(), nullptr, &d.disc) != NOD_RESULT_OK) return d.disc = nullptr, false;
    if (nod_disc_open_partition_kind(d.disc, NOD_PARTITION_KIND_DATA, nullptr, &d.part) != NOD_RESULT_OK) {
        nod_free(d.disc), d.disc = nullptr, d.part = nullptr;
        return false;
    }
    d.path = path;
    nod_partition_iterate_fst(d.part, list_files, &d);
    return true;
}

/* a whole file from a language disc, up to 8 MB (the title archive is 4 MB on the Japanese disc); "" on failure */
std::vector<uint8_t> read_whole(int jp, const std::string& name) {
    Disc& d = s_discs[jp];
    const std::lock_guard lock(d.m);
    if (!disc_open(d, jp)) return {};
    const auto it = d.files.find(name);
    if (it == d.files.end() || it->second.second > (8u << 20)) return {};
    NodHandle* f = nullptr;
    if (nod_partition_open_file(d.part, it->second.first, &f) != NOD_RESULT_OK) return {};
    std::vector<uint8_t> out(it->second.second);
    size_t at = 0;
    while (at < out.size()) {
        const int64_t got = nod_read(f, out.data() + at, out.size() - at);
        if (got <= 0) break;
        at += (size_t)got;
    }
    nod_free(f);
    if (at != out.size()) out.clear();
    return out;
}

/* Disc timing of an overlaid file (*Added 2026-11-19*, the Japanese release look's determinism fix; docs/port/
 * word-pictures.md "A side effect on timing"). Why: the port's console-like disc timing (pc_dvd.c) charges each read by
 * where it is on the disc and how long it is. An overlaid archive is the US archive with some entries appended (or a
 * language's own archive), so the game reads the swapped entries at other offsets and lengths, and the loads ended a
 * frame or two later than with the US archive; in one test the next battle's rolls landed on other frames. So every
 * read of an overlay is timed as the read the game makes of the US file it stands in for: Aurora gives the overlay the
 * US file's disc offset (startAddr), and these pieces map an overlay range to the US range read in its place. A read
 * outside every piece is at the same place in both files (swap_entries leaves the US bytes where they were). */
struct TimingPiece {
    uint32_t ovOff, ovLen; /* the range in the overlay (an entry's stored bytes, exact) */
    uint32_t usOff, usLen; /* the range the game reads in the US file in its place (exact) */
};

/* The archive cache's room for an overlaid piece (*Added 2026-11-19*, with the timing above): the FSYS loader caches
 * archive pieces in ARAM under (archive id, group id, resource id), or (archive id, 0, 1) for the table, and its
 * allocator's choices depend on their sizes; a swapped entry of another size made other pieces fit or not, so a later
 * load came from ARAM in one run and from the disc in the other (P03: a room's events a frame apart). So the cache
 * reserves the US piece's room (OrreHook_FsysCacheRoom below; pc_arq.c keeps any bytes past it aside) */
struct CacheRoom {
    uint32_t key1, key2;    /* the entry's group id (+0x20) and resource id (+0), or 0 and 1 for the table */
    uint32_t ovSize, usSize; /* the piece's size in the overlay and in the US archive (as the loader passes it) */
};

/* what an overlaid file is: bytes in memory, or a file on a language disc (read in pieces) */
struct Source {
    std::vector<uint8_t> bytes;
    int jp = -1;          /* >= 0: on that disc */
    uint32_t index = 0;   /* its FST index there */
    uint32_t size = 0;
    std::string path; /* the disc path it stands in for (the log) */
    std::vector<TimingPiece> timing; /* (above) empty: reads timed at their own offsets in the US file's place */
    uint32_t usSize = 0;             /* the US file's length (a read past it is timed up to it), 0 = not known */
    uint32_t archiveId = 0;          /* the archive id the game knows it by (the US one), for the cache rooms */
    std::vector<CacheRoom> rooms;    /* (CacheRoom above) */
};

/* a read [off, off + len) of an overlay, timed as the US file's: the piece it starts in maps it (a read that runs to or
 * past the piece's end ends where the US read would: the game reads an entry as its stored size, or that rounded up
 * to 32 bytes, so either form maps to the same form of the US size); outside every piece it stays, cut at the US
 * file's end (0 bytes left: 1, so it is still one read). True when anything changed */
bool time_as_us(const Source& s, uint32_t& off, uint32_t& len) {
    const auto up32 = [](uint32_t v) { return (v + 0x1Fu) & ~0x1Fu; };
    const uint64_t end = (uint64_t)off + len;
    for (const TimingPiece& p : s.timing) {
        if (off < p.ovOff || off >= (uint64_t)p.ovOff + up32(p.ovLen)) continue;
        const uint32_t rel = off - p.ovOff;
        const uint32_t start = p.usOff + (rel < p.usLen ? rel : p.usLen);
        uint64_t usEnd;
        if (end - p.ovOff == up32(p.ovLen)) usEnd = (uint64_t)p.usOff + up32(p.usLen);    /* the rounded-up form */
        else if (end >= (uint64_t)p.ovOff + p.ovLen) usEnd = (uint64_t)p.usOff + p.usLen + (end - p.ovOff - p.ovLen);
        else usEnd = (uint64_t)p.usOff + (end - p.ovOff < p.usLen ? end - p.ovOff : p.usLen);
        const uint32_t newLen = usEnd > start ? (uint32_t)(usEnd - start) : 1;
        const bool changed = start != off || newLen != len;
        off = start, len = newLen;
        return changed;
    }
    if (s.usSize != 0 && end > s.usSize) {
        const uint32_t start = off < s.usSize ? off : s.usSize;
        const uint32_t newLen = s.usSize > start ? s.usSize - start : 1;
        const bool changed = start != off || newLen != len;
        off = start, len = newLen;
        return changed;
    }
    return false;
}

struct Open {
    std::shared_ptr<Source> src;
    NodHandle* f = nullptr; /* the disc file, for a disc source */
    int64_t pos = 0;
};

void* cb_open(void* user) {
    auto* holder = static_cast<std::shared_ptr<Source>*>(user);
    auto* o = new Open{*holder, nullptr, 0};
    if (o->src->jp >= 0) {
        Disc& d = s_discs[o->src->jp];
        const std::lock_guard lock(d.m);
        if (!disc_open(d, o->src->jp) || nod_partition_open_file(d.part, o->src->index, &o->f) != NOD_RESULT_OK) {
            std::printf("[lang_files] could not open a file on the language disc (index %u)\n", o->src->index);
            delete o;
            return nullptr;
        }
    }
    std::printf("[lang_files] the game opened %s: the language disc's (%u bytes)\n", o->src->path.c_str(), o->src->size);
    return o;
}
void cb_close(void* h) {
    auto* o = static_cast<Open*>(h);
    if (o->f != nullptr) {
        const std::lock_guard lock(s_discs[o->src->jp].m);
        nod_free(o->f);
    }
    delete o;
}
int64_t cb_read(void* h, uint8_t* buf, size_t len) {
    auto* o = static_cast<Open*>(h);
    const int64_t size = o->src->size;
    if (o->pos >= size) return 0;
    if ((int64_t)len > size - o->pos) len = (size_t)(size - o->pos);
    if (o->src->jp < 0) {
        std::memcpy(buf, o->src->bytes.data() + o->pos, len);
        o->pos += (int64_t)len;
        return (int64_t)len;
    }
    const std::lock_guard lock(s_discs[o->src->jp].m);
    if (nod_seek(o->f, o->pos, 0) != o->pos) return -1;
    size_t at = 0;
    while (at < len) {
        const int64_t got = nod_read(o->f, buf + at, len - at);
        if (got < 0) return -1;
        if (got == 0) break;
        at += (size_t)got;
    }
    o->pos += (int64_t)at;
    return (int64_t)at;
}
int64_t cb_seek(void* h, int64_t offset, int32_t whence) {
    auto* o = static_cast<Open*>(h);
    const int64_t base = whence == 1 ? o->pos : whence == 2 ? (int64_t)o->src->size : 0;
    const int64_t to = base + offset;
    if (to < 0) return -1;
    o->pos = to;
    return to;
}

/* a whole file of the US disc, read through Aurora's own reader (the base disc's file, whatever is overlaid) */
std::vector<uint8_t> read_us_file(const char* path) {
    const s32 entry = DVDConvertPathToEntrynum(path);
    std::vector<uint8_t> a;
    if (entry < 0 || aurora_dvd_read_entry(entry, 1 << 16, [](void* u, const u8* p, size_t n) {
            static_cast<std::vector<uint8_t>*>(u)->insert(static_cast<std::vector<uint8_t>*>(u)->end(), p, p + n);
        }, &a) < 0)
        a.clear();
    return a;
}

/* the US archives' ids and names, and the two title archives, read from the US disc once, before anything is
 * overlaid */
std::map<std::string, OneEntry> s_us;
std::vector<uint8_t> s_usTitle, s_usExTitle;
bool s_usRead = false;
void read_us() {
    if (s_usRead) return;
    s_usRead = true;
    for (const char* name : kSequences) {
        const std::vector<uint8_t> a = read_us_file((std::string("/") + name + ".fsys").c_str());
        OneEntry e;
        if (one_entry(a, e)) s_us[name] = e;
    }
    s_usTitle = read_us_file("/title.fsys");
    s_usExTitle = read_us_file("/ex_title.fsys");
    std::printf("[lang_files] the US disc's %zu of %zu Battle Mode sequence archives read for their ids\n", s_us.size(),
                sizeof(kSequences) / sizeof(kSequences[0]));
}

/* an archive's entry by name: its record's place, or 0 (the layout as in one_entry, any number of entries) */
uint32_t find_entry(const std::vector<uint8_t>& a, const char* name) {
    if (a.size() < 0x80 || std::memcmp(a.data(), "FSYS", 4) != 0) return 0;
    const uint32_t n = be32(a, 0x0C), tab = be32(a, be32(a, 0x18));
    for (uint32_t k = 0; k < n && tab + 4 * k + 4 <= a.size(); ++k) {
        const uint32_t eo = be32(a, tab + 4 * k), no = be32(a, eo + 0x24);
        if ((size_t)eo + 0x50 > a.size() || no >= a.size()) return 0;
        if (std::strncmp(reinterpret_cast<const char*>(a.data() + no), name, a.size() - no) == 0) return eo;
    }
    return 0;
}

/* The title logo (piece 5, step 4): the US title archive with only the logo's three entries (the logo model and its two
 * camera moves) taken from the language disc's title archive, so the menu pictures and everything else stay the US
 * disc's (piece 4 cuts US pieces from them). The donor's stored bytes are appended at the end, 32-byte aligned, and
 * the US entry records point there with the donor's sizes and its packed flag (the Japanese entries are stored
 * unpacked; each entry carries its own flag); the header's file size (+0x20) follows. An entry whose donor bytes and
 * flag equal the US ones is left alone (the European cameras). Empty when nothing differs or anything isn't as
 * expected (the US archive then). An FSYS entry record (verified on the three discs): +0 id, +4 data offset, +8
 * unpacked size, +0x0C flags (0x80000000 packed), +0x14 stored size, +0x24 name offset. */
/* *Generalised 2026-11-19 (3.5, the Japanese release look):* the same step for any named entries of any archive
 * (swap_entries); logo_archive is it with the logo's three. The look's "entry" rows (the marbled menu background in
 * common, a trainer's VS portrait in chara_big; ui/regions/jp_look.txt) use it for their archives, because the game's
 * code looks those pictures up by id with no sprite record naming them, so the word pictures' record swap can't reach
 * them (verified 2026-11-19: no US sprite record uses 0x00EF1200 or 0x0FA21200; fn_8001DACC looks up 0x00EF1200 by id) */
/* *Updated 2026-11-19:* `timing` receives one piece per swapped entry (the appended range -> the US entry's range, both
 * as the game reads them: the stored size rounded up to 32 bytes, fsys's entry read), for the disc timing above */
std::vector<uint8_t> swap_entries(const std::vector<uint8_t>& us, const std::vector<uint8_t>& donor,
                                  const std::vector<std::string>& names, std::vector<TimingPiece>* timing = nullptr,
                                  std::vector<CacheRoom>* rooms = nullptr) {
    std::vector<uint8_t> out = us;
    std::vector<TimingPiece> pieces;
    std::vector<CacheRoom> cacheRooms; /* (CacheRoom above): the ARAM cache's key and both stored sizes per entry */
    int changed = 0;
    for (const std::string& n : names) {
        const char* name = n.c_str();
        const uint32_t ue = find_entry(out, name), de = find_entry(donor, name);
        if (ue == 0 || de == 0) return {};
        if (be32(out, ue) != be32(donor, de)) return {}; /* the same resource id on both discs, or not the same thing */
        const uint32_t dOff = be32(donor, de + 4), dStored = be32(donor, de + 0x14), dFlags = be32(donor, de + 0x0C);
        if ((size_t)dOff + dStored > donor.size()) return {};
        const uint32_t uOff = be32(out, ue + 4), uStored = be32(out, ue + 0x14);
        if (uStored == dStored && be32(out, ue + 0x0C) == dFlags && (size_t)uOff + uStored <= out.size() &&
            std::memcmp(out.data() + uOff, donor.data() + dOff, dStored) == 0)
            continue;
        const size_t at = (out.size() + 0x1F) & ~(size_t)0x1F;
        out.resize(at + ((dStored + 0x1F) & ~0x1Fu), 0);
        std::memcpy(out.data() + at, donor.data() + dOff, dStored);
        put32(out, ue + 4, (uint32_t)at);
        put32(out, ue + 8, be32(donor, de + 8));
        put32(out, ue + 0x0C, dFlags);
        put32(out, ue + 0x14, dStored);
        pieces.push_back({(uint32_t)at, dStored, uOff, uStored});
        cacheRooms.push_back({be32(out, ue + 0x20), be32(out, ue), dStored, uStored});
        ++changed;
    }
    if (changed == 0) return {};
    put32(out, 0x20, (uint32_t)out.size());
    cacheRooms.push_back({0xFFFFFFFFu, 0xFFFFFFFFu, archive_total(out), archive_total(us)}); /* the eviction check */
    if (timing != nullptr) *timing = std::move(pieces);
    if (rooms != nullptr) *rooms = std::move(cacheRooms);
    return out;
}

std::vector<uint8_t> logo_archive(const std::vector<uint8_t>& us, const std::vector<uint8_t>& donor,
                                  std::vector<TimingPiece>* timing = nullptr, std::vector<CacheRoom>* rooms = nullptr) {
    return swap_entries(us, donor, {"logo_demo", "cam_logo_demo_start", "cam_logo_demo_stop"}, timing, rooms);
}

/* one language's overlay set (path -> source), made once */
struct Set {
    std::vector<std::string> paths;
    std::vector<std::shared_ptr<Source>*> sources; /* kept forever: Aurora holds these as userData */
};
std::map<std::string, Set> s_sets;

/* the US archive ids of the look's overlaid archives (forget_cached_archives drops them on a language change) */
std::vector<uint32_t> s_lookIds;

/* The Japanese release look's "entry" rows (3.5 steps 2 and 3; pc_region_look.cpp): for Japanese with the Japanese
 * disc, each row's archive is served as the US archive with that entry taken from the Japanese disc's (swap_entries).
 * Rows sharing an archive are swapped together; the two title archives are left alone (already overlaid for the logo
 * above; no "entry" row is in them today, so this is only a guard). Fail safe: an archive that can't be read or doesn't
 * hold the entry under the same id stays the US disc's (logged). The US archive is read from the base disc
 * (read_us_file), the Japanese one whole from the player's disc (common 0.5 MB, chara_big 5.8 MB; held for the run).
 * *Updated 2026-11-19 (3.5 step 4):* Rui's four model rows take this path too (pkx_hizuki_a1, M1_out with two of
 * them, field_common, world_map). Why it works for a model of another size: the game's archive loader sizes each
 * buffer from the entry record (+0x08 unpacked, +0x14 stored, +0x04 where; fsysLoadEntrySync and the async path,
 * verified in the decomp), and swap_entries writes the Japanese entry's three values there, so no hook in the loader
 * is needed and the ARAM cache caches the Japanese entry like any other. Measured (2026-11-19, read-only on both
 * discs): served sizes about 0.24 MB, 2.78 MB, 0.33 MB and 1.84 MB, about 5.2 MB held for the run in Japanese with the
 * Japanese disc only; the Japanese archive read for it is freed right after */
void add_look_entries(Set& set) {
    if (pc_region_look_off()) return;
    std::map<std::string, std::vector<std::string>> wanted; /* archive -> entry names */
    for (int i = 0; i < pc_region_look_count(); ++i) {
        const PcRegionLookRow* r = pc_region_look_row(i);
        /* pictures and models alike: "entry" is the one path for both (the table's loader refuses a "sprite" model) */
        if (r == nullptr || std::strcmp(r->how, "entry") != 0) continue;
        std::string list = r->archives;
        size_t at = 0;
        while (at <= list.size()) {
            const size_t comma = list.find(',', at);
            std::string a = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
            while (!a.empty() && a.front() == ' ') a.erase(a.begin());
            while (!a.empty() && a.back() == ' ') a.pop_back();
            if (!a.empty()) wanted[a].push_back(r->entry);
            if (comma == std::string::npos) break;
            at = comma + 1;
        }
    }
    for (const auto& [archive, names] : wanted) {
        std::string what;
        for (const std::string& n : names) what += (what.empty() ? "" : ", ") + n;
        if (archive == "title" || archive == "ex_title" || archive == "main.dol") {
            std::printf("[region_look] %s (%s): not an archive this path serves: the US one\n", archive.c_str(),
                        what.c_str());
            continue;
        }
        const std::vector<uint8_t> us = read_us_file(("/" + archive + ".fsys").c_str());
        std::string lower = archive + ".fsys";
        for (char& c : lower) c = (char)SDL_tolower((unsigned char)c); /* the disc listing's names are lower-case */
        const std::vector<uint8_t> donor = read_whole(1, lower);
        std::vector<TimingPiece> timing;
        std::vector<CacheRoom> rooms;
        std::vector<uint8_t> a =
            us.empty() || donor.empty() ? std::vector<uint8_t>{} : swap_entries(us, donor, names, &timing, &rooms);
        if (a.empty()) {
            std::printf("[region_look] %s.fsys (%s): not as measured on one of the discs: the US archive\n",
                        archive.c_str(), what.c_str());
            continue;
        }
        s_lookIds.push_back(be32(us, 0x08));
        auto src = std::make_shared<Source>();
        /* *Added 2026-11-19:* its reads timed as the US archive's (TimingPiece above) and its pieces cached in the US
         * pieces' room (CacheRoom above), so the look costs no frames */
        src->timing = std::move(timing);
        src->usSize = (uint32_t)us.size();
        src->archiveId = be32(us, 0x08);
        src->rooms = std::move(rooms);
        src->bytes = std::move(a);
        src->size = (uint32_t)src->bytes.size();
        src->path = "/" + archive + ".fsys";
        set.paths.push_back(src->path);
        set.sources.push_back(new std::shared_ptr<Source>(src));
        std::printf("[region_look] %s.fsys served with %s from the Japanese disc (%u bytes)\n", archive.c_str(),
                    what.c_str(), src->size);
    }
}

const char* eu_prefix(const std::string& lang) {
    if (lang == "fr") return "fre";
    if (lang == "de") return "ger";
    if (lang == "it") return "ita";
    if (lang == "es") return "spa";
    return nullptr;
}

void add_disc_file(Set& set, int jp, const char* discName, const char* path) {
    Disc& d = s_discs[jp];
    std::pair<uint32_t, uint32_t> f;
    {
        const std::lock_guard lock(d.m);
        if (!disc_open(d, jp)) return;
        const auto it = d.files.find(discName);
        if (it == d.files.end()) {
            std::printf("[lang_files] %s isn't on the language disc: %s stays the US disc's\n", discName, path);
            return;
        }
        f = it->second;
    }
    auto src = std::make_shared<Source>();
    src->jp = jp, src->index = f.first, src->size = f.second, src->path = path;
    set.paths.push_back(path);
    set.sources.push_back(new std::shared_ptr<Source>(src));
}

Set& make_set(const std::string& lang) {
    const auto found = s_sets.find(lang);
    if (found != s_sets.end()) return found->second;
    Set& set = s_sets[lang];
    const char* eu = eu_prefix(lang);
    const int jp = lang == "ja" ? 1 : eu != nullptr ? 0 : -1;
    if (jp < 0 || pc_region_disc_path(jp)[0] == '\0') return set;
    add_disc_file(set, jp, "staffroll.thp", "/movie/staffroll.thp");
    if (jp == 1) add_disc_file(set, jp, "autodemo01.thp", "/movie/autodemo01.thp");
    int made = 0;
    for (const char* name : kSequences) {
        const auto us = s_us.find(name);
        if (us == s_us.end()) continue;
        std::string discName = name;
        if (eu != nullptr) discName = std::string("wzx_") + eu + "_" + (name + 4);
        std::vector<uint8_t> a = read_whole(jp, discName + ".fsys");
        OneEntry e;
        if (a.empty() || !one_entry(a, e) || e.name.size() < us->second.name.size()) {
            std::printf("[lang_files] %s: %s isn't one entry as measured: the US disc's\n", lang.c_str(), discName.c_str());
            continue;
        }
        /* the US program's names for it: the archive id, the entry id and the entry name (written over the language's
         * longer one, the rest of it zeroed) */
        put32(a, 0x08, us->second.archiveId);
        put32(a, e.entryAt, us->second.entryId);
        std::memset(a.data() + e.nameAt, 0, e.name.size());
        std::memcpy(a.data() + e.nameAt, us->second.name.data(), us->second.name.size());
        auto src = std::make_shared<Source>();
        /* disc timing (2026-11-19): the language's archive has its own table size and entry place and size, so its
         * two reads after the header (the table, the entry) are timed as the US archive's */
        if (e.tocSize > 0x40 && us->second.tocSize > 0x40)
            src->timing.push_back({0x40, e.tocSize - 0x40, 0x40, us->second.tocSize - 0x40});
        src->timing.push_back({e.dataOff, e.stored, us->second.dataOff, us->second.stored});
        src->usSize = us->second.size;
        /* the cache rooms: the table (key 0 / 1, its size as the loader passes it, rounded up to 32) and the entry
         * (its group id as served, the US resource id written above) */
        src->archiveId = us->second.archiveId;
        src->rooms.push_back({0, 1, (e.tocSize + 0x1F) & ~0x1Fu, (us->second.tocSize + 0x1F) & ~0x1Fu});
        src->rooms.push_back({be32(a, e.entryAt + 0x20), us->second.entryId, e.stored, us->second.stored});
        src->rooms.push_back({0xFFFFFFFFu, 0xFFFFFFFFu, e.total, us->second.total}); /* the eviction check */
        src->bytes = std::move(a);
        src->size = (uint32_t)src->bytes.size();
        src->path = std::string("/") + name + ".fsys";
        set.paths.push_back(src->path);
        set.sources.push_back(new std::shared_ptr<Source>(src));
        ++made;
    }
    /* the title logo: the language disc's own (the Japanese disc's; the European disc's one for its five languages),
     * in both title archives (the title and the one after the story, ex_title, carry the same logo) */
    if (SDL_getenv("ORRE_NO_LANGUAGE_LOGO") == nullptr) {
        const std::vector<uint8_t> donor = read_whole(jp, "title.fsys");
        int logos = 0;
        for (const auto& [path, us] : {std::make_pair("/title.fsys", &s_usTitle), std::make_pair("/ex_title.fsys", &s_usExTitle)}) {
            std::vector<TimingPiece> timing;
            std::vector<CacheRoom> rooms;
            std::vector<uint8_t> a = logo_archive(*us, donor, &timing, &rooms);
            if (a.empty()) continue;
            auto src = std::make_shared<Source>();
            /* disc timing and cache rooms (2026-11-19): read and cached as the US archive */
            src->timing = std::move(timing);
            src->usSize = (uint32_t)us->size();
            src->archiveId = be32(*us, 0x08);
            src->rooms = std::move(rooms);
            src->bytes = std::move(a);
            src->size = (uint32_t)src->bytes.size();
            src->path = path;
            set.paths.push_back(src->path);
            set.sources.push_back(new std::shared_ptr<Source>(src));
            ++logos;
        }
        if (logos == 0) std::printf("[lang_files] %s: the title logo stays the US disc's (none differs, or not as measured)\n", lang.c_str());
    }
    /* the Japanese release look's "entry" rows (3.5; Japanese only: the European discs' non-word pictures are the US
     * ones, docs/engine/regions.md) */
    if (jp == 1) add_look_entries(set);
    std::printf("[lang_files] %s: %zu files from the language disc (%d Battle Mode sequences)\n", lang.c_str(),
                set.paths.size(), made);
    return set;
}

std::string s_applied = "?";

/* The cache rooms of the overlay set now registered (CacheRoom above): (archive id, key1, key2) -> (overlay size, US
 * size). Replaced on every language change; read on the game thread by the hook below (a mutex, as the language
 * change may come from the hub's thread) */
std::mutex s_roomsLock;
std::map<std::tuple<uint32_t, uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>> s_rooms;

void publish_rooms(const Set& set) {
    const std::lock_guard lock(s_roomsLock);
    s_rooms.clear();
    for (const auto* holder : set.sources)
        for (const CacheRoom& r : (*holder)->rooms)
            if (r.ovSize != r.usSize) s_rooms[{(*holder)->archiveId, r.key1, r.key2}] = {r.ovSize, r.usSize};
}

/* OrreHook_FsysCacheRoom (orre_hooks.h): the US piece's room for an overlaid piece, in the form the loader passed
 * (exact, or rounded up to 32 bytes); any other piece keeps its size */
u32 cache_room(u32 size, u32 fileHandle, u32 key1, u32 key2) {
    const std::lock_guard lock(s_roomsLock);
    const auto it = s_rooms.find({fileHandle, key1, key2});
    if (it == s_rooms.end()) return size;
    const auto up32 = [](uint32_t v) { return (v + 0x1Fu) & ~0x1Fu; };
    if (size == it->second.first) return it->second.second;
    if (size == up32(it->second.first)) return up32(it->second.second);
    return size; /* not the piece this was made for (another size): left alone */
}

/* *Added 2026-11-16 (snatcherLeo: "if I say, switched to Japanese in the language settings, it would, for example, switch all
 * the UI at that point in time"):* after a language change, the archives this file answers for are dropped from the
 * game's ARAM cache, so each one's next load reads the new language's bytes. Why: the archive loader keeps what it
 * read in ARAM and serves the next load from there (verified 2026-11-16 in a test log: back on the title after
 * switching to Japanese in Options, title.fsys was reloaded with slot status 0xA1, from the cache, and the logo stayed
 * English). The archives: the two title archives and the 19 Battle Mode sequences, by their US ids (the overlays keep
 * those ids). The movies are streamed, not cached, so they need nothing. An archive loading at that moment is left
 * (logged; it then keeps the old language until the cache drops it). Platforms: all (the game's own cache code). */
void forget_cached_archives() {
    if (pc_game_ui.fsys_cache_forget == nullptr) return;
    std::vector<uint32_t> ids;
    if (s_usTitle.size() >= 0x0C) ids.push_back(be32(s_usTitle, 0x08));
    if (s_usExTitle.size() >= 0x0C) ids.push_back(be32(s_usExTitle, 0x08));
    for (const auto& [name, e] : s_us) ids.push_back(e.archiveId);
    /* the Japanese release look's archives (common, chara_big, and Rui's models' pkx_hizuki_a1, M1_out, field_common,
     * world_map). Likely not enough for common and field_common: they may be loaded once and kept, so a language change
     * mid-game shows the other release's background / follower from the next start or the next field load (guess until
     * a test switches) */
    ids.insert(ids.end(), s_lookIds.begin(), s_lookIds.end());
    int skipped = 0;
    for (uint32_t id : ids) skipped += pc_game_ui.fsys_cache_forget(id) ? 0 : 1;
    std::printf("[lang_files] the game's cached copies of %zu overlaid archives dropped (%d loading now, kept)\n",
                ids.size() - (size_t)skipped, skipped);
}

} // namespace
#endif

/* The disc timing of a read of an overlaid file (*Added 2026-11-19*; pc_dvd.c's note_read asks before timing a read):
 * `handle` is what cb_open returned (aurora_dvd_overlay_handle), `offset` / `length` the read in the overlay; on return
 * the read the game makes of the US file in its place (TimingPiece above). Returns 1 when it changed them. Only the
 * timing changes: the bytes still come from the overlay. ORRE_DVD_TRACE_LOG=1 logs each changed read. Platforms: all. */
extern "C" int pc_lang_files_disc_timing(void* handle, uint32_t* offset, uint32_t* length) {
#ifdef PC_HAVE_AURORA_DVD
    if (handle == nullptr || offset == nullptr || length == nullptr) return 0;
    const Open* o = static_cast<const Open*>(handle);
    uint32_t off = *offset, len = *length;
    if (!time_as_us(*o->src, off, len)) return 0;
    static const bool log = [] {
        const char* env = SDL_getenv("ORRE_DVD_TRACE_LOG");
        return env != nullptr && *env == '1';
    }();
    if (log)
        std::printf("[lang_files] read of %s at 0x%X+%u timed as the US file's 0x%X+%u\n", o->src->path.c_str(),
                    (unsigned)*offset, (unsigned)*length, (unsigned)off, (unsigned)len);
    *offset = off, *length = len;
    return 1;
#else
    (void)handle, (void)offset, (void)length;
    return 0;
#endif
}

/* Register the files for the game's language (pc_game_text.cpp calls it at start-up and on every language change).
 * Only once the US disc is open in Aurora (the hub's rescans before the game starts are skipped; the start-up call
 * after the disc opens does it). */
extern "C" void pc_lang_files_apply(void) {
#ifdef PC_HAVE_AURORA_DVD
    if (aurora_dvd_base_entry_count() <= 0) return;
    const bool filesOff = SDL_getenv("ORRE_NO_LANGUAGE_FILES") != nullptr;
    const std::string lang = pc_game_text_language();
    /* the Japanese release look's line (3.5 step 2): what of its table applies in this language, at start-up and on
     * every change (logged only when it changes) */
    pc_region_look_log_state(lang.c_str(), pc_region_text_has("ja"), filesOff ? 1 : 0);
    if (filesOff) return;
    if (lang == s_applied) return;
    static bool callbacks = false;
    if (!callbacks) {
        read_us(); /* before the first overlay: the US files' own ids */
        static const AuroraOverlayCallbacks cb = {cb_open, cb_close, cb_read, cb_seek};
        aurora_dvd_overlay_callbacks(&cb);
        /* the archive cache's rooms (2026-11-19; CacheRoom above): with no overlay set registered the table is empty
         * and every piece keeps its size, so English stays exactly the GameCube's behaviour */
        OrreHook_FsysCacheRoom = cache_room;
        OrreHook_FsysCacheReserved = pc_arq_short_block;
        callbacks = true;
    }
    const Set& set = make_set(lang);
    std::vector<AuroraOverlayFile> files;
    for (size_t i = 0; i < set.paths.size(); ++i)
        files.push_back({set.paths[i].c_str(), set.sources[i], (size_t)(*set.sources[i])->size});
    if (files.empty() && (s_applied == "?" || s_sets[s_applied].paths.empty())) {
        s_applied = lang; /* nothing overlaid before or now: Aurora's table stays as it is */
        return;
    }
    aurora_dvd_overlay_files(files.data(), files.size(), nullptr);
    publish_rooms(set);
    std::printf("[lang_files] %zu files now from the %s disc (%zu archive pieces cached in the US pieces' room)\n",
                files.size(), lang.c_str(), s_rooms.size());
    if (s_applied != "?") forget_cached_archives(); /* a change while the game runs (at start-up nothing is cached yet) */
    s_applied = lang;
#endif
}
