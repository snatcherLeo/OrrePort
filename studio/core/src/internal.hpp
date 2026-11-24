// internal.hpp: the reader core's shared C++ pieces (not part of the C API).
// Why: each disc format lives in its own file (disc, fsys, msg, species, textures, models) so each can be ported, tested
// and later reused one at a time, as the plan asks ("port them one format at a time"). Platforms: plain C++20, every OS.
#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "orre_core.h"

namespace orre {

using Bytes = std::vector<uint8_t>;

// Big-endian reads with bounds checks (disc data is big-endian; a bad offset reads 0 instead of crashing, so a damaged
// or modded file can't take Studio down).
inline uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
inline uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
inline uint32_t be32(const Bytes& b, size_t o) { return o + 4 <= b.size() ? be32(b.data() + o) : 0; }
inline uint16_t be16(const Bytes& b, size_t o) { return o + 2 <= b.size() ? be16(b.data() + o) : 0; }
inline uint8_t u8(const Bytes& b, size_t o) { return o < b.size() ? b[o] : 0; }
inline float bef32(const Bytes& b, size_t o) {
    uint32_t w = be32(b, o);
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

// ---- fsys.cpp ----
Bytes lzss(const uint8_t* src, size_t n);
struct FsysEntry {
    int index;
    std::string name;
    uint32_t type;
    uint32_t id;      // the entry's resource id (its first word)
    uint32_t offset;  // data offset in the archive
    uint32_t stored;  // stored size
    bool packed;      // LZSS
};
std::vector<FsysEntry> fsys_entries(const Bytes& arc);
Bytes fsys_load(const Bytes& arc, const FsysEntry& e);

// ---- gx_texture.cpp ----
// GS texture format code (the game's own numbering, GStextureGetGXformat) -> GX format; -1 if unknown
int gs_to_gx_format(int gs);
// GX tiled pixels -> RGBA8 (w*h*4). palette: for C4 / C8 / C14X2, the TLUT entries already as RGBA8. false when the
// data is too short.
bool decode_gx(const uint8_t* data, size_t size, int gx_format, int w, int h, const std::vector<uint32_t>* palette,
               std::vector<uint8_t>& out);
// a TLUT (palette) in GX tlut format 0 IA8, 1 RGB565, 2 RGB5A3 -> RGBA8 words (bytes r, g, b, a in memory order)
std::vector<uint32_t> decode_tlut(const uint8_t* data, size_t size, int tlut_format, int count);
// a type-9 GS texture file (GStextureHandle header + pixels): w, h, rgba; false if not decodable
bool decode_texture_file(const Bytes& d, int& w, int& h, std::vector<uint8_t>& rgba);

// ---- hsd_model.cpp ----
bool parse_hsd_model(const Bytes& dat, OrreModel& out);

// ---- the disc and its caches (disc.cpp, msg.cpp, species.cpp) ----
struct Disc {
    std::recursive_mutex lock;
    void* nod_disc = nullptr;
    void* nod_part = nullptr;
    std::string game_id;
    Bytes dol;
    std::map<std::string, uint32_t> files;  // full path -> FST index (sorted by path, as the Python tools sort them)

    bool read_file(const std::string& path, Bytes& out);
    bool read_archive(const std::string& name, Bytes& out) { return read_file(name + ".fsys", out); }

    // caches
    Bytes common_rel;
    bool rel_loaded = false;
    std::unordered_map<uint32_t, std::string> text_raw, text_clean;
    bool text_rel_done = false, text_known_done = false, text_all_done = false;
    std::map<uint32_t, std::pair<std::string, FsysEntry>> face_tex;  // entry id -> archive name, entry (poke_face / poke_body)
    bool face_tex_done = false;
    std::map<std::string, Bytes> archive_cache;
    std::map<uint32_t, std::string> toc;  // GSfsys.toc: archive id -> archive name
    bool toc_done = false;

    const Bytes& rel();
    void load_rel_text();
    void load_known_banks();
    void load_all_text();
    const std::string* find_text(uint32_t id, bool clean);
    const Bytes* archive(const std::string& name);  // cached whole archive
    void load_face_textures();
    void load_toc();
    bool dol_read(uint32_t addr, size_t n, Bytes& out);
};

// message banks (msg.cpp)
void bank_text(const uint8_t* d, size_t n, std::vector<std::pair<uint32_t, std::string>>& out);
std::string clean_text(const std::string& raw);

// common_rel tables (species.cpp)
constexpr size_t kSec4 = 0x184C;  // section 4's place in the unpacked common_rel (docs/engine/game-data-tables.md)
std::vector<const uint8_t*> table_rows(const Bytes& rel, uint32_t off, uint32_t count_off, uint32_t size);
// an archive's entry by the archive's id (GSfsys.toc) and the entry's id (species.cpp)
bool find_entry(Disc& d, uint32_t archive_id, uint32_t entry_id, Bytes& out);

}  // namespace orre

struct OrreDisc {
    orre::Disc d;
};
