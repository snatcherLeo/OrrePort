// fsys.cpp: FSYS archives and their LZSS packing (port of tools/local/colo/fsys.py; format: docs/engine/disc-files.md).
// Why here: every Pokémon picture, model and most text sit inside .fsys archives, so this is the first format every
// editor needs. Platforms: plain C++, every OS.
#include "internal.hpp"

namespace orre {

// LZSS as the game packs it: a 4 KB window starting at 0xFEE, flag byte LSB first (1 = literal byte; 0 = a 2-byte
// reference: 12-bit window offset, 4-bit length + 3). The packed data starts after the 16-byte "LZSS" header.
Bytes lzss(const uint8_t* src, size_t n) {
    Bytes out;
    out.reserve(n * 3);
    uint8_t win[0x1000] = {};
    unsigned w = 0xFEE, flags = 0;
    size_t i = 0x10;
    while (i < n) {
        flags >>= 1;
        if (!(flags & 0x100)) {
            flags = src[i++] | 0xFF00u;
            if (i >= n) break;
        }
        if (flags & 1) {
            const uint8_t b = src[i++];
            out.push_back(b);
            win[w] = b;
            w = (w + 1) & 0xFFF;
        } else {
            if (i + 1 >= n) break;
            const unsigned a = src[i], b = src[i + 1];
            i += 2;
            const unsigned off = a | ((b << 4) & 0xF00);
            for (unsigned k = 0; k < (b & 0xF) + 3; ++k) {
                const uint8_t c = win[(off + k) & 0xFFF];
                out.push_back(c);
                win[w] = c;
                w = (w + 1) & 0xFFF;
            }
        }
    }
    return out;
}

// An archive's entries: header "FSYS", +0x0C entry count, +0x18 the info block whose first word is the entry-pointer
// table; each 0x50-byte entry {u32 id, u32 data offset, u32 unpacked size, u32 flags (bit 31 = LZSS), ..., +0x14 stored
// size, +0x20 type, +0x24 name offset}.
std::vector<FsysEntry> fsys_entries(const Bytes& arc) {
    std::vector<FsysEntry> out;
    if (arc.size() < 0x20 || std::memcmp(arc.data(), "FSYS", 4) != 0) return out;
    const uint32_t count = be32(arc, 0x0C), blk = be32(arc, 0x18), tab = be32(arc, blk);
    for (uint32_t k = 0; k < count && k < 100000; ++k) {
        const uint32_t eo = be32(arc, tab + 4 * k);
        if (eo + 0x50 > arc.size()) continue;
        FsysEntry e;
        e.index = (int)k;
        e.id = be32(arc, eo);
        e.offset = be32(arc, eo + 4);
        e.packed = (be32(arc, eo + 0x0C) & 0x80000000u) != 0;
        e.stored = be32(arc, eo + 0x14);
        e.type = be32(arc, eo + 0x20);
        const uint32_t no = be32(arc, eo + 0x24);
        if (no < arc.size()) e.name.assign(reinterpret_cast<const char*>(arc.data() + no), strnlen(reinterpret_cast<const char*>(arc.data() + no), arc.size() - no));
        out.push_back(std::move(e));
    }
    return out;
}

Bytes fsys_load(const Bytes& arc, const FsysEntry& e) {
    if (e.offset >= arc.size()) return {};
    const size_t n = std::min<size_t>(e.stored, arc.size() - e.offset);
    if (e.packed) return lzss(arc.data() + e.offset, n);
    return Bytes(arc.begin() + e.offset, arc.begin() + e.offset + n);
}

}  // namespace orre

extern "C" uint8_t* orre_fsys_entry(OrreDisc* od, const char* archive, int entry, size_t* size) {
    if (!od || !archive) return nullptr;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    const orre::Bytes* arc = od->d.archive(archive);
    if (!arc) return nullptr;
    for (const auto& e : orre::fsys_entries(*arc)) {
        if (e.index != entry) continue;
        orre::Bytes b = orre::fsys_load(*arc, e);
        auto* p = static_cast<uint8_t*>(std::malloc(b.size() ? b.size() : 1));
        if (!b.empty()) std::memcpy(p, b.data(), b.size());
        if (size) *size = b.size();
        return p;
    }
    return nullptr;
}

extern "C" void orre_free(void* p) { std::free(p); }
