// disc.cpp: the disc image through nod (port of tools/local/colo/disc.py).
// Why nod: the port already reads the player's disc through it (port/src/pc_region_text.cpp), and it opens every
// format players keep their discs in (ISO, GCM, RVZ, WIA, CISO, NFS), so Studio accepts the same files OrrePort does.
// Read-only: nod opens the image for reading only, so OrrePort may have the same file open at the same time (likely:
// nod is written in Rust, whose standard file open on Windows shares read, write and delete; not yet tried with the
// game running). Platforms: nod ships for Windows, Linux and macOS; nothing here is OS-specific.
#include <nod.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include "internal.hpp"

namespace orre {

namespace {
// read exactly n bytes at offset (nod may return fewer per call)
bool read_at(NodHandle* f, int64_t offset, uint8_t* out, size_t n) {
    if (nod_seek(f, offset, 0) != offset) return false;
    while (n > 0) {
        const int64_t got = nod_read(f, out, n);
        if (got <= 0) return false;
        out += got;
        n -= (size_t)got;
    }
    return true;
}

// The file table (fst.bin), walked as disc.py walks it: 12-byte nodes {u8 is_dir, u24 name offset, u32 a, u32 b};
// a directory's b is the index after its last child. Full paths joined with "/".
void parse_fst(const uint8_t* fst, size_t size, std::map<std::string, uint32_t>& files) {
    if (size < 12) return;
    const uint32_t n = be32(fst + 8);
    if ((size_t)n * 12 > size) return;
    const char* names = reinterpret_cast<const char*>(fst + (size_t)n * 12);
    const size_t names_len = size - (size_t)n * 12;
    std::vector<std::pair<uint32_t, std::string>> ends;  // (end index, directory path)
    for (uint32_t i = 1; i < n; ++i) {
        while (!ends.empty() && i >= ends.back().first) ends.pop_back();
        const uint8_t* e = fst + (size_t)i * 12;
        const uint32_t w0 = be32(e), b = be32(e + 8);
        const uint32_t no = w0 & 0xFFFFFF;
        if (no >= names_len) continue;
        const std::string nm(names + no, strnlen(names + no, names_len - no));
        const std::string full = ends.empty() ? nm : ends.back().second + "/" + nm;
        if (w0 >> 24) ends.emplace_back(b, full);
        else files[full] = i;
    }
}
}  // namespace

bool Disc::read_file(const std::string& path, Bytes& out) {
    std::lock_guard<std::recursive_mutex> g(lock);
    auto it = files.find(path);
    if (it == files.end() || nod_part == nullptr) return false;
    NodHandle* f = nullptr;
    if (nod_partition_open_file(static_cast<NodHandle*>(nod_part), it->second, &f) != NOD_RESULT_OK) return false;
    const int64_t len = nod_seek(f, 0, 2);
    bool ok = len >= 0 && len < (512ll << 20);
    if (ok) {
        out.resize((size_t)len);
        ok = read_at(f, 0, out.data(), out.size());
    }
    nod_free(f);
    return ok;
}

const Bytes* Disc::archive(const std::string& name) {
    std::lock_guard<std::recursive_mutex> g(lock);
    auto it = archive_cache.find(name);
    if (it != archive_cache.end()) return &it->second;
    Bytes b;
    if (!read_archive(name, b)) return nullptr;
    return &(archive_cache[name] = std::move(b));
}

// main.dol's bytes at a GameCube address (section table at the start: 18 offsets, 18 addresses, 18 sizes)
bool Disc::dol_read(uint32_t addr, size_t n, Bytes& out) {
    if (dol.size() < 0x100) return false;
    for (int s = 0; s < 18; ++s) {
        const uint32_t o = be32(dol, 4 * s), a = be32(dol, 72 + 4 * s), z = be32(dol, 144 + 4 * s);
        if (addr >= a && addr + n <= (uint64_t)a + z && o + (addr - a) + n <= dol.size()) {
            out.assign(dol.begin() + o + (addr - a), dol.begin() + o + (addr - a) + n);
            return true;
        }
    }
    return false;
}

void Disc::load_toc() {
    std::lock_guard<std::recursive_mutex> g(lock);
    if (toc_done) return;
    toc_done = true;
    Bytes t;
    if (!read_file("GSfsys.toc", t)) return;
    const uint32_t cnt = be32(t, 8), tab = be32(t, 0x10);
    for (uint32_t k = 0; k < cnt && tab + 8 * (k + 1) <= t.size(); ++k) {
        const uint32_t id = be32(t, tab + 8 * k), o = be32(t, tab + 8 * k + 4);
        if (o >= t.size()) continue;
        toc[id] = std::string(reinterpret_cast<const char*>(t.data() + o), strnlen(reinterpret_cast<const char*>(t.data() + o), t.size() - o));
    }
}

}  // namespace orre

extern "C" OrreDisc* orre_disc_open(const char* path, char* err, size_t err_len) {
    auto fail = [&](const char* why) -> OrreDisc* {
        if (err && err_len) {
            const char* nodwhy = nod_error_message();
            std::snprintf(err, err_len, "%s%s%s", why, nodwhy ? ": " : "", nodwhy ? nodwhy : "");
        }
        return nullptr;
    };
    if (path == nullptr || *path == 0) return fail("No disc image was given");
    NodHandle* disc = nullptr;
    if (nod_disc_open(path, nullptr, &disc) != NOD_RESULT_OK) return fail("The disc image couldn't be opened");
    NodHandle* part = nullptr;
    if (nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, nullptr, &part) != NOD_RESULT_OK) {
        nod_free(disc);
        return fail("The disc image has no game data");
    }
    auto* od = new OrreDisc();
    od->d.nod_disc = disc;
    od->d.nod_part = part;
    NodPartitionMeta meta{};
    if (nod_partition_meta(part, &meta) == NOD_RESULT_OK) {
        if (meta.raw_boot.data && meta.raw_boot.size >= 6) od->d.game_id.assign(reinterpret_cast<const char*>(meta.raw_boot.data), 6);
        if (meta.raw_dol.data) od->d.dol.assign(meta.raw_dol.data, meta.raw_dol.data + meta.raw_dol.size);
        if (meta.raw_fst.data) orre::parse_fst(meta.raw_fst.data, meta.raw_fst.size, od->d.files);
    }
    if (od->d.files.empty()) {
        orre_disc_close(od);
        return fail("The disc image's file table couldn't be read");
    }
    return od;
}

extern "C" void orre_disc_close(OrreDisc* od) {
    if (!od) return;
    nod_free(static_cast<NodHandle*>(od->d.nod_part));
    nod_free(static_cast<NodHandle*>(od->d.nod_disc));
    delete od;
}

extern "C" const char* orre_disc_game_id(OrreDisc* od) { return od ? od->d.game_id.c_str() : ""; }

// orre_disc_is_original's table: each Colosseum release's main.dol SHA-1.
// - GC6E01 (USA): config/GC6E01/build.sha1's main.dol line, the file the decomp rebuilds byte for byte (verified).
// - GC6P01 (Europe), GC6J01 (Japan): read from snatcherLeo's own discs on 2026-11-23 (likely the original releases: not yet
//   compared with an outside list such as Redump's; a second revision of either, if one exists, would need its row).
extern "C" int orre_disc_is_original(OrreDisc* od) {
    static const char* const kOriginal[][2] = {
        {"GC6E01", "870e8b9693ca780782d80f22a6a4572d8ba9458f"},
        {"GC6P01", "c3fe2adb3b3bbe3743221dbe2f8478734b57135e"},
        {"GC6J01", "76ddb5f75cf11d9574900b5fe1c99a9c8fc2ae78"},
    };
    if (!od) return -1;
    char sha[41];
    for (const auto& row : kOriginal)
        if (od->d.game_id == row[0]) return orre_disc_main_dol_sha1(od, sha) && std::strcmp(sha, row[1]) == 0 ? 1 : 0;
    return -1;
}

// orre_disc_find (orre_core.h says what and why). std::filesystem for the listing and the change times, so it is the
// same on every system; paths go in and out as UTF-8 (u8path), so a folder with letters outside the ANSI code page
// works on Windows too.
extern "C" int orre_disc_find(const char* folder, const char* game_id, char* out, size_t out_len) {
    return orre_disc_find_checked(folder, game_id, out, out_len, nullptr, 0);
}
extern "C" int orre_disc_find_checked(const char* folder, const char* game_id, char* out, size_t out_len, char* refused, size_t refused_len) {
    namespace fs = std::filesystem;
    if (out && out_len) out[0] = 0;
    if (refused && refused_len) refused[0] = 0;
    if (!folder || !game_id) return 0;
    std::error_code ec;
    const fs::path dir(reinterpret_cast<const char8_t*>(folder));  // (C++20: a char8_t string is read as UTF-8)
    if (!fs::is_directory(dir, ec)) return 0;
    auto utf8 = [](const fs::path& p) { const std::u8string s = p.u8string(); return std::string(s.begin(), s.end()); };
    struct Hit { fs::file_time_type when; std::string name, path; };
    std::vector<Hit> hits, bad;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        const std::string name = utf8(e.path().filename());
        if (name.size() > 5 && name.compare(name.size() - 5, 5, ".part") == 0) continue;
        const std::string path = utf8(e.path());
        char err[256];
        OrreDisc* d = orre_disc_open(path.c_str(), err, sizeof err);
        if (!d) continue;
        const bool match = d->d.game_id == game_id;
        const bool original = !match || orre_disc_is_original(d) != 0;  // (-1, no known fingerprint: not refused)
        orre_disc_close(d);
        if (match) (original ? hits : bad).push_back({e.last_write_time(ec), name, path});
    }
    auto newest_first = [](const Hit& a, const Hit& b) { return a.when != b.when ? a.when > b.when : a.name < b.name; };
    if (!bad.empty() && refused && refused_len) {
        std::sort(bad.begin(), bad.end(), newest_first);
        std::snprintf(refused, refused_len, "%s", bad.front().path.c_str());
    }
    if (hits.empty()) return 0;
    std::sort(hits.begin(), hits.end(), newest_first);
    if (out && out_len) std::snprintf(out, out_len, "%s", hits.front().path.c_str());
    return (int)hits.size();
}

// main.dol's SHA-1 (2026-11-22; 4.3 part 3, the logging list's item 3): the same fingerprint OrrePort checks
// before it starts (port/src/pc_disc_check.c, against config/GC6E01/build.sha1), so a log or report says exactly which
// release and revision was opened. The standard algorithm (FIPS 180-1), written out so the core needs no crypto library
// (main.dol is about 3.6 MB: a few milliseconds). Nothing OS-specific.
extern "C" int orre_disc_main_dol_sha1(OrreDisc* od, char out[41]) {
    if (out) out[0] = 0;
    if (!od || !out || od->d.dol.empty()) return 0;
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    auto block = [&h](const uint8_t* p) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        auto rol = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            const uint32_t f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            const uint32_t k = i < 20 ? 0x5A827999u : i < 40 ? 0x6ED9EBA1u : i < 60 ? 0x8F1BBCDCu : 0xCA62C1D6u;
            const uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rol(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    };
    const orre::Bytes& m = od->d.dol;
    const size_t whole = m.size() / 64 * 64;
    for (size_t i = 0; i < whole; i += 64) block(m.data() + i);
    // the tail: the leftover bytes, 0x80, zeros, then the length in bits (big-endian) in the last 8 bytes
    uint8_t tail[128] = {};
    const size_t left = m.size() - whole;
    std::copy(m.begin() + (ptrdiff_t)whole, m.end(), tail);
    tail[left] = 0x80;
    const size_t tail_len = left < 56 ? 64 : 128;
    const uint64_t bits = (uint64_t)m.size() * 8;
    for (int i = 0; i < 8; ++i) tail[tail_len - 1 - i] = (uint8_t)(bits >> (8 * i));
    block(tail);
    if (tail_len == 128) block(tail + 64);
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 20; ++i) {
        const uint8_t v = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
        out[2 * i] = hex[v >> 4];
        out[2 * i + 1] = hex[v & 15];
    }
    out[40] = 0;
    return 1;
}
