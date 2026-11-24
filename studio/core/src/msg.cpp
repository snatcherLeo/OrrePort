// msg.cpp: message banks (port of tools/local/colo/msg.py; format: docs/engine/messages.md).
// Why: every name Studio shows (species, moves, abilities, items, categories, Pokédex text) is a message id looked up
// in these banks, read from the player's own disc. Platforms: plain C++, every OS.
#include <algorithm>
#include <cstdio>

#include "internal.hpp"

namespace orre {

namespace {
// Argument bytes after each 0xFFFF control code (msg.py CONTROL: the handlers in src/game/msgctrl*.c read them);
// every other code takes none.
int control_args(int code) {
    switch (code) {
        case 0x07: case 0x09: case 0x38: case 0x52: case 0x53: case 0x54: case 0x5B: case 0x5C: return 1;
        case 0x08: return 4;
        default: return 0;
    }
}

void put_utf8(std::string& s, uint32_t c) {
    if (c < 0x80) s += (char)c;
    else if (c < 0x800) { s += (char)(0xC0 | c >> 6); s += (char)(0x80 | (c & 0x3F)); }
    else { s += (char)(0xE0 | c >> 12); s += (char)(0x80 | (c >> 6 & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
}

bool is_hex_upper(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }

// Python's str.isspace for the characters strip() removes (so clean() matches msg.py exactly)
bool py_space(uint32_t c) {
    return c == 9 || c == 10 || c == 11 || c == 12 || c == 13 || (c >= 0x1C && c <= 0x20) || c == 0x85 || c == 0xA0 ||
           c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
           c == 0x3000;
}

uint32_t decode_at(const std::string& s, size_t i, size_t& len) {
    const uint8_t c = (uint8_t)s[i];
    if (c < 0x80) { len = 1; return c; }
    if ((c >> 5) == 6 && i + 1 < s.size()) { len = 2; return (c & 0x1F) << 6 | (s[i + 1] & 0x3F); }
    if ((c >> 4) == 14 && i + 2 < s.size()) { len = 3; return (c & 0x0F) << 12 | (s[i + 1] & 0x3F) << 6 | (s[i + 2] & 0x3F); }
    len = 1;
    return c;
}
}  // namespace

// One bank: u16 count at +4, then 8-byte {u32 id, u32 offset} records from +0x10; each text is UTF-16 big-endian up to
// a 0 character, with 0xFFFF + code (+ argument bytes) for control codes. Written out as msg.py does: <XX> or
// <XX:args in hex>, and an unpaired surrogate as [XXXX].
void bank_text(const uint8_t* d, size_t n, std::vector<std::pair<uint32_t, std::string>>& out) {
    if (n < 0x10) return;
    const unsigned count = be16(d + 4);
    for (unsigned i = 0; i < count && 0x18 + 8 * (size_t)i <= n; ++i) {
        const uint32_t mid = be32(d + 0x10 + 8 * i), o = be32(d + 0x14 + 8 * i);
        std::string s;
        size_t j = o;
        while (j + 1 < n) {
            const unsigned c = be16(d + j);
            j += 2;
            if (c == 0) break;
            if (c == 0xFFFF) {
                if (j >= n) break;
                const int code = d[j++];
                const int na = control_args(code);
                char buf[16];
                std::snprintf(buf, sizeof buf, "<%02X", code);
                s += buf;
                if (na) {
                    s += ':';
                    for (int k = 0; k < na && j + k < n; ++k) {
                        std::snprintf(buf, sizeof buf, "%02X", d[j + k]);
                        s += buf;
                    }
                }
                s += '>';
                j += na;
                continue;
            }
            if (c >= 0xD800 && c < 0xE000) {
                char buf[16];
                std::snprintf(buf, sizeof buf, "[%04X]", c);
                s += buf;
            } else {
                put_utf8(s, c);
            }
        }
        out.emplace_back(mid, std::move(s));
    }
}

// msg.py clean(): "<00>" (a line break) becomes a space, every other control code is removed, then outer whitespace.
std::string clean_text(const std::string& raw) {
    std::string s;
    s.reserve(raw.size());
    for (size_t i = 0; i < raw.size();) {
        if (raw.compare(i, 4, "<00>") == 0) { s += ' '; i += 4; continue; }
        if (raw[i] == '<' && i + 3 < raw.size() && is_hex_upper(raw[i + 1]) && is_hex_upper(raw[i + 2])) {
            if (raw[i + 3] == '>') { i += 4; continue; }
            if (raw[i + 3] == ':') {
                size_t k = i + 4;
                while (k < raw.size() && is_hex_upper(raw[k])) ++k;
                if (k < raw.size() && raw[k] == '>') { i = k + 1; continue; }
            }
        }
        s += raw[i++];
    }
    // strip
    size_t a = 0, len = 0;
    while (a < s.size() && py_space(decode_at(s, a, len))) a += len;
    size_t b = s.size();
    while (b > a) {
        size_t p = b - 1;
        while (p > a && ((uint8_t)s[p] & 0xC0) == 0x80) --p;
        if (!py_space(decode_at(s, p, len))) break;
        b = p;
    }
    return s.substr(a, b - a);
}

// The three banks inside common_rel (msg.py REL_BANKS, section-4 offsets): species, move, item, ability and other
// shared names. Read first, as msg.py's all_text does (the first bank to hold an id wins).
void Disc::load_rel_text() {
    std::lock_guard<std::recursive_mutex> g(lock);
    if (text_rel_done) return;
    text_rel_done = true;
    const Bytes& r = rel();
    static const uint32_t kBanks[] = {0x58044, 0x647b4, 0x76c94};
    for (uint32_t off : kBanks) {
        if (kSec4 + off >= r.size()) continue;
        std::vector<std::pair<uint32_t, std::string>> lines;
        bank_text(r.data() + kSec4 + off, r.size() - kSec4 - off, lines);
        for (auto& [id, s] : lines) text_raw.emplace(id, std::move(s));
    }
}

// Then every type-5 entry of every archive, archives in path order, each entry id once (msg.py all_text). This reads
// the whole disc (~1 s), so it only happens when a message isn't in common_rel's banks.
void Disc::load_all_text() {
    std::lock_guard<std::recursive_mutex> g(lock);
    load_rel_text();
    if (text_all_done) return;
    text_all_done = true;
    std::vector<uint32_t> seen;
    for (const auto& [path, idx] : files) {
        if (path.size() < 5 || path.compare(path.size() - 5, 5, ".fsys") != 0) continue;
        Bytes arc;
        if (!read_file(path, arc)) continue;
        for (const auto& e : fsys_entries(arc)) {
            if (e.type != 5 || std::find(seen.begin(), seen.end(), e.id) != seen.end()) continue;
            seen.push_back(e.id);
            Bytes b = fsys_load(arc, e);
            std::vector<std::pair<uint32_t, std::string>> lines;
            bank_text(b.data(), b.size(), lines);
            for (auto& [id, s] : lines) text_raw.emplace(id, std::move(s));
        }
    }
}

// The banks most lookups need, read before any full scan: pda_menu.fsys holds the species categories ("Seed") for
// 275 of the 276 category ids (verified 2026-11-18, US disc). Found because Studio's memory watch flagged a 362 MB read
// on the first page: one category lookup had fallen through to the full scan below.
void Disc::load_known_banks() {
    std::lock_guard<std::recursive_mutex> g(lock);
    if (text_known_done) return;
    text_known_done = true;
    for (const char* name : {"pda_menu"}) {
        Bytes arc;
        if (!read_archive(name, arc)) continue;
        for (const auto& e : fsys_entries(arc)) {
            if (e.type != 5) continue;
            Bytes b = fsys_load(arc, e);
            std::vector<std::pair<uint32_t, std::string>> lines;
            bank_text(b.data(), b.size(), lines);
            for (auto& [id, s] : lines) text_raw.emplace(id, std::move(s));
        }
    }
}

const std::string* Disc::find_text(uint32_t id, bool clean) {
    std::lock_guard<std::recursive_mutex> g(lock);
    load_rel_text();
    auto it = text_raw.find(id);
    if (it == text_raw.end()) {
        // Note: msg.py's order is common_rel's banks, then every archive in path order (first bank wins); pda_menu
        // early can only differ from it for an id two banks both hold with different text (none seen for the species
        // table's ids; the golden test compares names and categories with the Python tools).
        load_known_banks();
        it = text_raw.find(id);
    }
    if (it == text_raw.end()) {
        load_all_text();  // the last resort: every bank on the disc (~360 MB read, ~0.2 s)
        it = text_raw.find(id);
        if (it == text_raw.end()) return nullptr;
    }
    if (!clean) return &it->second;
    auto c = text_clean.find(id);
    if (c == text_clean.end()) c = text_clean.emplace(id, clean_text(it->second)).first;
    return &c->second;
}

}  // namespace orre

extern "C" const char* orre_text(OrreDisc* od, uint32_t id) {
    if (!od) return "";
    const std::string* s = od->d.find_text(id, true);
    return s ? s->c_str() : "";
}

extern "C" const char* orre_text_raw(OrreDisc* od, uint32_t id) {
    if (!od) return "";
    const std::string* s = od->d.find_text(id, false);
    return s ? s->c_str() : "";
}

extern "C" int orre_text_count(OrreDisc* od) {
    if (!od) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    od->d.load_all_text();
    return (int)od->d.text_raw.size();
}
