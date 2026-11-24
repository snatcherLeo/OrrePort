/*
 * pc_region_text.cpp: the European languages' text, read from the player's own European disc (2.2 step 8, piece 1;
 * 2026-11-15; docs/port/translation.md "Text from a language disc").
 *
 * What it does: when the player has put a European Colosseum disc (GC6P01) in Colosseum\ROM Other Regions\, this file
 * reads the German, French, Italian and Spanish text from it at start-up and keeps it by message key, so
 * pc_game_text.cpp can hand the game the chosen language's text for the same key. Nothing is written anywhere: the
 * text lives in memory while OrrePort runs, read from the player's disc each start (nothing from a disc ships).
 *
 * Why this way (decided 2026-11-15, snatcherLeo: "Approve, start piece 1"; what is taken from the disc: the decision log "What
 * OrrePort takes from the European disc"):
 *   - Every European language lines up with the US text message for message (verified 2026-11-15 by a read-only study
 *     of both discs: 11,673 messages per European language, the same keys as the US disc's 11,647 plus 26 more, all in
 *     bank group 0). So a language is only "the text for each key", and the game's lookup hook swaps it in.
 *   - The US program stays (the European program's 50 / 60 Hz choice and Language row aren't needed): only text is
 *     read. Every control code the European text uses is one the US program handles (verified: French, German and
 *     Spanish also use 0x21 / 0x44, the "no" form of the side names, whose US handlers fetch the side name by message
 *     key, which this text also supplies; that the result reads right in a battle is likely until hand-checked).
 *   - Where the text is on the European disc (verified 2026-11-15):
 *       * 162 .fsys archives carry one message bank (resource type 5) per language, named <archive>_ge / _fr / _it /
 *         _sp / _uk; the bank's own bytes 6-7 say its language ("GE", "FR", "IT", "SP", "UK");
 *       * common.fsys's code module common_rel (type 14) holds 3 banks per language;
 *       * main.dol holds 1 bank per language (690 messages each).
 *     UK English is never read (decided: one English, the US text).
 *   - Read once, in the background: about 2 MB per language after unpacking. If the player starts in one of these
 *     languages, start-up waits for it (a second or so from an .iso; guess for compressed images), so the first message
 *     is already in that language.
 *
 * Formats (docs/engine/disc-files.md, messages.md; verified with tools/local/colo/fsys.py on both discs):
 *   - .fsys: entry count at +0x0C, the entry-table block at +0x18 (its first word: the table of entry offsets), the
 *     first file's data at +0x1C (so [0, that) is every header, entry and name); an entry (0x50 bytes) has the data
 *     offset at +0x04, flags at +0x0C (0x80000000: LZSS-packed), the stored size at +0x14, the type at +0x20, the name's
 *     offset at +0x24.
 *   - LZSS: a 16-byte header, then flag bytes (low bit first; 1 = a literal byte), a 4 KB window starting at 0xFEE, a
 *     reference of 2 bytes: offset = a | (b & 0xF0) << 4, length = (b & 0x0F) + 3.
 *   - A message bank: u16 group id, u8, u8 font, u16 count, 2 bytes of language tag, 8 zero bytes, then count entries
 *     (u32 message id ascending, u32 offset of its text from the bank's start). Text: big-endian UTF-16 with control
 *     codes (0xFFFF + a code byte + its argument bytes), ended by 0x0000. The game's key is group << 20 | id.
 *
 * Platforms: plain C++ with nod (the disc reader Aurora already uses for the US disc): the same on Windows, Linux and
 * macOS (verified on Windows; likely elsewhere, nothing here is OS-specific). The folder name is OrrePort's own.
 * XD: its European disc likely carries banks the same way (guess until XD is checked; docs/xd-insights.md).
 */
#include <SDL3/SDL.h>

#include "pc_save_cipher.h" /* pc_save_sha1: a language disc's main.dol against the original (2026-11-23) */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef PC_HAVE_AURORA_DVD
#include <aurora/dvd.h> /* the US disc the game plays: read here only for the GameCube copy's species names */
#include <dolphin/dvd.h>
#include <nod.h>
#endif

#include "pc_region_look.h"   /* the Japanese release look's table (3.5): the disc read checks its entries are there */
#include "pc_region_text.h"
#include "pc_word_pictures.h" /* the word pictures (piece 4): the disc read notes their textures and sprite table */
#include "pc_paths.h"         /* ROM Other Regions\ (4.1) */

/* pc_gcopy.c (3.5 step 9). Weak: pc_gcopy.c comes with Aurora's card library, which a build may lack */
extern "C" void pc_gcopy_note_dol(char region, const uint8_t* dol, size_t size) __attribute__((weak));
namespace {
void note_card_header(char region, const uint8_t* dol, size_t size) {
    if (pc_gcopy_note_dol != nullptr) pc_gcopy_note_dol(region, dol, size);
}
} // namespace

extern "C" const char* pc_platform_rom_folder(char* out, size_t size); /* pc_platform.c: <exe>/Colosseum/ROM/ */

namespace {

/* OrrePort's language codes and the bank tags the European disc uses for them. Japanese (piece 3, 2026-11-15): the
 * Japanese disc has one language, and its banks carry no tag (bytes 6-7 are 00 00, or 00 01 in six banks: verified),
 * so its banks are given the language rather than read from a tag (kJapanese) */
struct Lang {
    const char* code;
    char tag[3];
};
constexpr Lang kLangs[] = {{"de", "GE"}, {"fr", "FR"}, {"it", "IT"}, {"es", "SP"}, {"ja", ""}};
constexpr int kEuropeanCount = 4; /* the tagged ones */
constexpr int kJapanese = 4;
constexpr int kLangCount = 5;

/* one language's text: every message's bytes back to back, and where each key's text starts */
struct Text {
    std::vector<uint8_t> bytes;
    std::unordered_map<uint32_t, uint32_t> at; /* key -> offset into bytes */
};
Text s_text[kLangCount];
std::atomic<bool> s_ready{false}; /* the background read is done (s_text is then never changed again) */
std::string s_disc;               /* the European disc found, or empty */
std::string s_disc_jp;            /* the Japanese disc found, or empty */

/* The Japanese disc's own letters (piece 3, 2026-11-15). The Japanese game loads its kanji per room and menu (font
 * files of resource type 6 in each archive: font 0 in 22 x 22 cells, font 1 in 18 x 18) plus one large kanji font
 * (2,938 kanji); the US disc's files hold far fewer, so the US game showing Japanese text lacks some (65 in the
 * opening, verified in a test run). Every glyph of the Japanese disc's font files is kept here by font and character,
 * as the disc's own bytes: a bank header and an 8-byte glyph record laid out exactly as the game reads them, so
 * pc_font_render.cpp's part D hands the game the Japanese game's real picture of a letter its US fonts lack (before
 * the open fonts), and part B traces it like any of the game's letters. */
struct JpGlyph {
    const uint8_t* bank;   /* the font's bank header (its font header + 8) */
    const uint8_t* record; /* the glyph's 8-byte record */
    uint8_t cellH;
};
std::unordered_map<uint32_t, JpGlyph> s_jp_glyphs;                /* fontId << 16 | character */
std::vector<std::unique_ptr<std::vector<uint8_t>>> s_jp_fontfiles; /* the font files the glyphs point into */
std::thread s_loader;

int lang_index(const char* code) {
    if (code == nullptr) return -1;
    for (int i = 0; i < kLangCount; ++i)
        if (std::strcmp(kLangs[i].code, code) == 0) return i;
    return -1;
}

int tag_index(const uint8_t* tag) {
    for (int i = 0; i < kEuropeanCount; ++i)
        if (tag[0] == (uint8_t)kLangs[i].tag[0] && tag[1] == (uint8_t)kLangs[i].tag[1]) return i;
    return -1;
}

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

/* control codes' argument bytes: the same table as pc_game_text.cpp's control_args (verified from the msgctrl
 * handlers): Color 0x08 takes 4 bytes; Font, Wait, Palette, Shadow, Align, TalkSE, LineSpace, BaseLineBias take 1 */
int control_args(uint8_t code) {
    switch (code) {
    case 0x08: return 4;
    case 0x07: case 0x09: case 0x38: case 0x52: case 0x53: case 0x54: case 0x5B: case 0x5C: return 1;
    default: return 0;
    }
}

/* a text's length in bytes, its 0x0000 end included; 0 when it runs past the end of the data */
size_t text_length(const uint8_t* t, size_t limit) {
    size_t i = 0;
    while (i + 1 < limit) {
        const uint16_t c = be16(t + i);
        if (c == 0) return i + 2;
        i += c == 0xFFFF ? 3 + (size_t)(i + 2 < limit ? control_args(t[i + 2]) : 0) : 2;
    }
    return 0;
}

/* LZSS as the game packs its archive entries (see the file comment) */
std::vector<uint8_t> lzss(const uint8_t* src, size_t n) {
    std::vector<uint8_t> out;
    if (n >= 8) out.reserve(be32(src + 4)); /* the header's unpacked size */
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

/* The bank at `at` in `b`, if the bytes there are one: count > 0, 8 zero bytes at +8, ids ascending, offsets inside the
 * data (the offsets themselves aren't in order: the US main.dol bank's aren't). Its texts are added to the tagged
 * language (`lang`, or the bank's own tag when -1); a key already read stays (the first bank wins). Returns whether it
 * was a bank. */
bool add_bank(const uint8_t* b, size_t size, size_t at, int lang, size_t minCount) {
    if (at + 0x10 > size) return false;
    const uint8_t* h = b + at;
    const uint16_t count = be16(h + 4);
    if (count == 0 || count < minCount) return false;
    for (int k = 8; k < 16; ++k)
        if (h[k] != 0) return false;
    const size_t table = 0x10 + 8 * (size_t)count;
    if (at + table > size) return false;
    if (lang < 0) lang = tag_index(h + 6);
    if (lang < 0) return false;
    uint32_t last = 0;
    for (size_t k = 0; k < count; ++k) {
        const uint32_t id = be32(h + 0x10 + 8 * k), off = be32(h + 0x14 + 8 * k);
        if ((k > 0 && id <= last) || off < table || at + off >= size) return false;
        last = id;
    }
    Text& t = s_text[lang];
    const uint32_t group = be16(h);
    for (size_t k = 0; k < count; ++k) {
        const uint32_t key = group << 20 | be32(h + 0x10 + 8 * k);
        const size_t off = at + be32(h + 0x14 + 8 * k);
        const size_t len = text_length(b + off, size - off);
        if (len == 0 || t.at.count(key) != 0) continue;
        t.at[key] = (uint32_t)t.bytes.size();
        t.bytes.insert(t.bytes.end(), b + off, b + off + len);
    }
    return true;
}

/* a Japanese font file (resource type 6): a chain of fonts, each a 0x18-byte header (u16 font id, u8 cell width, u8
 * cell height, u32 size to the next font or 0 for the last, then the bank: u16 count, 2 bytes, u32 data offset, two
 * links) and its glyph records (8 bytes each, sorted by character); docs/engine/fonts.md, tools/local/colo/font.py.
 * Glyphs not seen before are kept (the file too, when it gave any) */
/* the glyphs of the font chain starting at o in b (kept by the caller); true when any was new */
bool add_chain(const std::vector<uint8_t>& b, size_t o) {
    bool used = false;
    while (o + 0x18 <= b.size()) {
        const uint16_t id = be16(&b[o]);
        const uint8_t cw = b[o + 2], ch = b[o + 3];
        const uint32_t size = be32(&b[o + 4]);
        const uint16_t count = be16(&b[o + 8]);
        if (id == 0xFFFF || count == 0 || cw == 0 || ch == 0 || o + 0x18 + 8 * (size_t)count > b.size()) break;
        for (uint16_t g = 0; g < count; ++g) {
            const uint8_t* rec = &b[o + 0x18 + 8 * (size_t)g];
            const uint32_t key = (uint32_t)id << 16 | be16(rec);
            if (s_jp_glyphs.count(key) != 0) continue;
            s_jp_glyphs[key] = JpGlyph{&b[o + 8], rec, ch};
            used = true;
        }
        if (size == 0) break;
        o += size;
    }
    return used;
}

void add_fonts(std::vector<uint8_t>&& file) {
    auto keep = std::make_unique<std::vector<uint8_t>>(std::move(file));
    if (add_chain(*keep, 0)) s_jp_fontfiles.push_back(std::move(keep));
}

/* The Japanese game's main fonts (its common letters: 大 発 先 団 ...) are inside its main.dol, not in font files or
 * its code module (verified 2026-11-15: six fonts at 0x272BC0-0x2AE8E0 of the Japanese main.dol; the US game keeps its
 * main fonts in common_rel). Found by their headers: a font id 0-4, a squarish cell of 8-32 pixels, 10-8,000 glyphs,
 * the data after the records, and the first records' characters ascending */
void add_fonts_found(const uint8_t* data, size_t size) {
    auto keep = std::make_unique<std::vector<uint8_t>>(data, data + size);
    const std::vector<uint8_t>& b = *keep;
    bool used = false;
    for (size_t o = 0; o + 0x20 < b.size();) {
        const uint16_t id = be16(&b[o]), count = be16(&b[o + 8]);
        const uint8_t cw = b[o + 2], ch = b[o + 3];
        const uint32_t fsize = be32(&b[o + 4]), doff = be32(&b[o + 0x0C]);
        bool ok = id <= 4 && cw >= 8 && cw <= 32 && ch >= 8 && ch <= 32 && count >= 10 && count <= 8000 &&
                  doff >= 0x10 + 8u * count && o + 0x18 + 8 * (size_t)count <= b.size() &&
                  (fsize == 0 || fsize >= 0x18 + 8u * count);
        for (uint16_t g = 1; ok && g < count && g < 60; ++g)
            ok = be16(&b[o + 0x18 + 8 * (size_t)g]) > be16(&b[o + 0x10 + 8 * (size_t)g]);
        if (!ok) {
            o += 4;
            continue;
        }
        /* this font alone (its chain link may not be one: a size of 0 ends it) */
        used |= add_chain(b, o);
        o += 0x18 + 8 * (size_t)count;
    }
    if (used) s_jp_fontfiles.push_back(std::move(keep));
}

/* every tagged bank in a blob of code or data (main.dol, a .rel module), looked for at 4-byte steps; at least 3
 * messages, so a stray match on a few bytes isn't taken (the study found none, guess that 3 is a safe floor) */
void scan_blob(const uint8_t* b, size_t size, int lang = -1) {
    for (size_t at = 0; at + 0x10 <= size; at += 4) {
        /* the European disc: a bank says its language; the Japanese disc: an untagged bank (00 00 or 00 01) */
        if (lang < 0 ? tag_index(b + at + 6) < 0 : (b[at + 6] != 0 || b[at + 7] > 1)) continue;
        if (add_bank(b, size, at, lang, 3)) {
            /* skip past this bank's table (its texts may follow anywhere) */
            at += 8 * (size_t)be16(b + at + 4);
        }
    }
}

#ifdef PC_HAVE_AURORA_DVD

/* read exactly n bytes at offset (nod may return fewer per call) */
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

struct FsysFile {
    uint32_t index;
    std::string name;
};

uint32_t list_fsys(uint32_t index, NodNodeKind kind, const char* name, uint32_t size, void* user) {
    (void)size;
    const size_t n = std::strlen(name);
    if (kind == NOD_NODE_KIND_FILE && n > 5 && SDL_strcasecmp(name + n - 5, ".fsys") == 0) {
        static_cast<std::vector<FsysFile>*>(user)->push_back({index, name});
    }
    return index + 1;
}

/* one archive: its type-5 banks for the four languages (the _uk ones are skipped by name, never read), and in
 * common.fsys the code module common_rel, scanned for its banks. lang >= 0 (the Japanese disc): every type-5 bank,
 * named without a language suffix, is that language's. Texture entries (type 9) are only noted, for the word pictures
 * (piece 4; read later, one at a time, when the game first draws one: pc_region_read_entry) */
void read_fsys(NodHandle* part, const FsysFile& file, int lang) {
    NodHandle* f = nullptr;
    if (nod_partition_open_file(part, file.index, &f) != NOD_RESULT_OK) return;
    /* the archive's name without ".fsys" (the word pictures tell a European language's copy by its suffix) */
    const std::string archive = file.name.size() > 5 ? file.name.substr(0, file.name.size() - 5) : file.name;
    uint8_t head[0x20];
    if (read_at(f, 0, head, sizeof(head)) && std::memcmp(head, "FSYS", 4) == 0) {
        const uint32_t count = be32(head + 0x0C), metaEnd = be32(head + 0x1C);
        std::vector<uint8_t> meta(metaEnd);
        if (metaEnd >= 0x20 && metaEnd < (16u << 20) && read_at(f, 0, meta.data(), meta.size())) {
            const uint32_t block = be32(head + 0x18);
            const uint32_t table = block + 4 <= metaEnd ? be32(meta.data() + block) : metaEnd;
            for (uint32_t k = 0; k < count && table + 4 * (k + 1) <= metaEnd; ++k) {
                const uint32_t eo = be32(meta.data() + table + 4 * k);
                if (eo + 0x50 > metaEnd) continue;
                const uint8_t* e = meta.data() + eo;
                const uint32_t type = be32(e + 0x20), nameOff = be32(e + 0x24);
                if (nameOff >= metaEnd) continue;
                const std::string name(reinterpret_cast<const char*>(meta.data() + nameOff),
                                       strnlen(reinterpret_cast<const char*>(meta.data() + nameOff), metaEnd - nameOff));
                const bool bank = type == 5 && (lang >= 0 || (name.size() > 3 && name.compare(name.size() - 3, 3, "_uk") != 0 &&
                                                              name[name.size() - 3] == '_'));
                const bool rel = type == 14 && name == "common_rel";
                const bool font = type == 6 && lang == kJapanese; /* the Japanese disc's letters (add_fonts) */
                const uint32_t dataOff = be32(e + 0x04), flags = be32(e + 0x0C), stored = be32(e + 0x14);
                /* every entry's name and archive, for the Japanese release look's check that each of its table's
                 * entries is on the player's disc where the table says (pc_region_look.cpp; a name compare, no read) */
                if (lang == kJapanese) pc_region_look_note_found('J', archive.c_str(), name.c_str());
                if (type == 9) {
                    /* a texture: only where it is is noted (the word pictures, piece 4, pc_word_pictures.cpp, keep the
                     * approved pictures' and read one from the disc when the game first draws it); the entry's first
                     * word is its resource id */
                    pc_word_pictures_note_entry(lang == kJapanese ? PC_WP_DISC_JP : PC_WP_DISC_EU, archive.c_str(),
                                                file.index, name.c_str(), be32(e), dataOff, stored,
                                                (flags & 0x80000000u) != 0);
                    continue;
                }
                if (!bank && !rel && !font) continue;
                if (stored == 0 || stored > (64u << 20)) continue;
                std::vector<uint8_t> raw(stored);
                if (!read_at(f, dataOff, raw.data(), raw.size())) continue;
                std::vector<uint8_t> data = (flags & 0x80000000u) ? lzss(raw.data(), raw.size()) : raw;
                if (font) add_fonts(std::move(data));
                else if (bank) add_bank(data.data(), data.size(), 0, lang, 1);
                else scan_blob(data.data(), data.size(), lang);
            }
        }
    }
    nod_free(f);
}

/* one disc read whole: every archive, then main.dol. lang -1: the European disc (each bank's tag says its language);
 * kJapanese: the Japanese disc (verified 2026-11-15: 9,414 messages, every one under a key the US disc uses for the same
 * message; in 169 fsys banks, one in common_rel and one in main.dol) */
void read_one(const std::string& path, int lang) {
    const auto start = std::chrono::steady_clock::now();
    NodHandle* disc = nullptr;
    NodHandle* part = nullptr;
    if (nod_disc_open(path.c_str(), nullptr, &disc) == NOD_RESULT_OK &&
        nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, nullptr, &part) == NOD_RESULT_OK) {
        std::vector<FsysFile> files;
        nod_partition_iterate_fst(part, list_fsys, &files);
        for (const FsysFile& f : files) read_fsys(part, f, lang);
        NodPartitionMeta meta{};
        if (nod_partition_meta(part, &meta) == NOD_RESULT_OK && meta.raw_dol.data != nullptr) {
            scan_blob(meta.raw_dol.data, meta.raw_dol.size, lang);
            /* the Japanese game's main fonts live in its main.dol (add_fonts_found) */
            if (lang == kJapanese) add_fonts_found(meta.raw_dol.data, meta.raw_dol.size);
            /* and its sprite table, for the word pictures (piece 4: which piece of which picture each language draws) */
            pc_word_pictures_note_dol(lang == kJapanese ? PC_WP_DISC_JP : PC_WP_DISC_EU, meta.raw_dol.data,
                                      meta.raw_dol.size);
            /* and its save file's card header (banner, title, comment), for a GameCube copy for that region (3.5
             * step 9, pc_gcopy.c; snatcherLeo's call 1 of 2026-11-19): a few KB kept, not the whole main.dol */
            note_card_header(lang == kJapanese ? 'J' : 'P', meta.raw_dol.data, meta.raw_dol.size);
        }
    } else {
        std::printf("[region_text] %s couldn't be read: %s\n", path.c_str(),
                    nod_error_message() != nullptr ? nod_error_message() : "?");
    }
    nod_free(part);
    nod_free(disc);
    pc_word_pictures_disc_done(lang == kJapanese ? PC_WP_DISC_JP : PC_WP_DISC_EU);
    if (lang == kJapanese) pc_region_look_disc_done('J'); /* the look table's entries found on this disc (logged) */
    const long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    if (lang < 0)
        std::printf("[region_text] European text read in %ld ms: de %zu, fr %zu, it %zu, es %zu messages\n", ms,
                    s_text[0].at.size(), s_text[1].at.size(), s_text[2].at.size(), s_text[3].at.size());
    else
        std::printf("[region_text] Japanese text read in %ld ms: %zu messages, %zu letters from its fonts\n", ms,
                    s_text[kJapanese].at.size(), s_jp_glyphs.size());
}

/* the loader thread: both language discs, whichever are there */
void read_disc(std::string eu, std::string jp) {
    if (!eu.empty()) read_one(eu, -1);
    if (!jp.empty()) read_one(jp, kJapanese);
    s_ready.store(true);
}

/* The word pictures' reads (piece 4, pc_word_pictures.cpp): one handle per language disc, opened when the game first
 * draws one of its pictures and kept open (each picture is read once, then kept), closed when the discs are looked at
 * again. Used on the game thread only, apart from the loader's own handles. */
struct LazyDisc {
    NodHandle* disc = nullptr;
    NodHandle* part = nullptr;
};
LazyDisc s_lazy[2]; /* PC_WP_DISC_EU, PC_WP_DISC_JP */
std::mutex s_lazyMutex;

void close_lazy() {
    const std::lock_guard lock(s_lazyMutex);
    for (LazyDisc& d : s_lazy) {
        nod_free(d.part);
        nod_free(d.disc);
        d = LazyDisc{};
    }
}

#endif

/* *Updated 2026-11-21 (4.1):* asked of the paths table, no longer made by cutting "ROM/" off the ROM folder's path */
std::string folder_path() { return pc_paths_str(PC_PLACE_ROM_OTHER); }

} // namespace

extern "C" const char* pc_lang_discs_folder(char* out, size_t size) {
    SDL_strlcpy(out, folder_path().c_str(), size);
    return out;
}

/* a disc's game ID, with nod's own handle: never through Aurora's DVD layer, which holds the one US disc the game
 * plays (opening another there would close it) */
extern "C" int pc_lang_disc_identify(const char* path, char* id) {
    id[0] = '\0';
#ifdef PC_HAVE_AURORA_DVD
    NodHandle* disc = nullptr;
    if (path == nullptr || nod_disc_open(path, nullptr, &disc) != NOD_RESULT_OK) return 0;
    NodDiscHeader h{};
    const bool ok = nod_disc_header(disc, &h) == NOD_RESULT_OK;
    nod_free(disc);
    if (!ok) return 0;
    std::memcpy(id, h.game_id, 6);
    id[6] = '\0';
    return 1;
#else
    (void)path;
    return 0;
#endif
}

/* Is a European or Japanese disc the original release? (snatcherLeo, 2026-11-23, through Studio: "Yes, refuse all three, use
 * the hub's wording. Hub and Studio should share exact wording for these things".) Its main.dol's SHA-1 against the
 * one value per game ID that Studio's shared core holds (orre_disc_is_original): GC6P01 and GC6J01 read from snatcherLeo's own
 * discs (likely the only release of each; a second revision, if one exists, would be refused until added here). The US
 * disc keeps its own check (pc_disc_check.c, config/GC6E01/build.sha1). 1: the original; 0: not, or unreadable.
 * nod, as the text reader; the same on every system. */
extern "C" int pc_lang_disc_is_original(const char* path, const char* id) {
#ifdef PC_HAVE_AURORA_DVD
    static const struct { const char* id; const char* sha1; } originals[] = {
        {"GC6E01", "870e8b9693ca780782d80f22a6a4572d8ba9458f"}, /* the US disc too (config/GC6E01/build.sha1; verified),
                                                                  for the hub's folder watch (pc_us_disc_pick) */
        {"GC6P01", "c3fe2adb3b3bbe3743221dbe2f8478734b57135e"},
        {"GC6J01", "76ddb5f75cf11d9574900b5fe1c99a9c8fc2ae78"},
    };
    const char* want = nullptr;
    for (const auto& o : originals)
        if (id != nullptr && std::strcmp(id, o.id) == 0) want = o.sha1;
    if (want == nullptr || path == nullptr) return 0;
    NodHandle* disc = nullptr;
    NodHandle* part = nullptr;
    int ok = 0;
    if (nod_disc_open(path, nullptr, &disc) == NOD_RESULT_OK &&
        nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, nullptr, &part) == NOD_RESULT_OK) {
        NodPartitionMeta meta{};
        if (nod_partition_meta(part, &meta) == NOD_RESULT_OK && meta.raw_dol.data != nullptr) {
            uint8_t sum[20];
            char hex[41];
            pc_save_sha1(meta.raw_dol.data, meta.raw_dol.size, sum);
            for (int i = 0; i < 20; ++i) std::snprintf(hex + 2 * i, 3, "%02x", sum[i]);
            ok = std::strcmp(hex, want) == 0;
            if (!ok) std::printf("[langdiscs] %s: main.dol's SHA-1 is %s, not the original %s's\n", path, hex, id);
        }
    }
    if (part != nullptr) nod_free(part);
    if (disc != nullptr) nod_free(disc);
    return ok;
#else
    (void)path;
    (void)id;
    return 0;
#endif
}

/* Which US disc in ROM\ the shared rule picks now, read with nod so the disc the game has open is left alone (the hub's
 * folder watch, 2026-11-23, step 4.4): the newest original (main.dol checked), then the first by name; with no original,
 * the newest of the rest (as pc_platform_disc_scan). out gets its full path ("" when there is no US disc); returns how
 * many US discs there are. Every OS the same (SDL's listing, nod). */
extern "C" int pc_us_disc_pick(char* out, size_t size) {
    char folder[1300];
    int count = 0, n = 0, best_orig = -1;
    Sint64 best_time = 0;
    std::string best, best_name;
    if (out != nullptr && size > 0) out[0] = '\0';
    pc_paths_dir(PC_PLACE_ROM, folder, sizeof folder);
    if (char** files = SDL_GlobDirectory(folder, "*", 0, &count)) {
        for (int i = 0; i < count; ++i) {
            const std::string path = std::string(folder) + files[i];
            if (path.size() > 5 && path.compare(path.size() - 5, 5, ".part") == 0) continue;
            SDL_PathInfo info;
            char id[7];
            if (!SDL_GetPathInfo(path.c_str(), &info) || info.type != SDL_PATHTYPE_FILE) continue;
            if (!pc_lang_disc_identify(path.c_str(), id) || std::strcmp(id, "GC6E01") != 0) continue;
            ++n;
            const int orig = pc_lang_disc_is_original(path.c_str(), id);
            const Sint64 t = (Sint64)info.modify_time;
            if (best.empty() || orig > best_orig || (orig == best_orig && (t > best_time || (t == best_time && files[i] < best_name)))) {
                best = path;
                best_name = files[i];
                best_time = t;
                best_orig = orig;
            }
        }
        SDL_free(files);
    }
    if (out != nullptr && size > 0) SDL_strlcpy(out, best.c_str(), size);
    return n;
}

/* the files in ROM Other Regions\ that are this game's language discs (any file name, any format nod reads); the
 * hub's copy in progress (".part") is skipped. One per region: *changed 2026-11-23* (4.4, the shared "which disc" rule,
 * docs/port/disc-sync-review.md) from the first file found to the newest (last change), then the first by name, as
 * Studio's orre_disc_find decides, so the hub, the game and Studio use the same European and Japanese disc. */
extern "C" int pc_lang_discs_list(const char* prefix, PcLangDisc* out, int max) {
    const std::string folder = folder_path();
    int count = 0, n = 0;
    Sint64 when[8] = {};       /* each kept disc's change time */
    std::string names[8];      /* ...and its file name, for the tie-break */
    if (max > 8) max = 8;
    if (char** files = SDL_GlobDirectory(folder.c_str(), "*", 0, &count)) {
        for (int i = 0; i < count; ++i) {
            const std::string path = folder + files[i];
            if (path.size() > 5 && path.compare(path.size() - 5, 5, ".part") == 0) continue;
            SDL_PathInfo info;
            if (!SDL_GetPathInfo(path.c_str(), &info) || info.type != SDL_PATHTYPE_FILE) continue;
            char id[7];
            if (!pc_lang_disc_identify(path.c_str(), id) || std::strncmp(id, prefix, std::strlen(prefix)) != 0) continue;
            if (id[3] != 'P' && id[3] != 'J') continue; /* a US disc here isn't a language disc */
            if (!pc_lang_disc_is_original(path.c_str(), id)) continue; /* a modified one is refused (2026-11-23) */
            int k = 0;
            while (k < n && out[k].id[3] != id[3]) ++k;
            if (k < n) { /* this region has one already: keep the newer, then the first by name */
                const Sint64 t = (Sint64)info.modify_time;
                if (t < when[k] || (t == when[k] && names[k] < std::string(files[i]))) continue;
            } else if (n >= max) {
                continue;
            } else {
                k = n++;
            }
            PcLangDisc& d = out[k];
            SDL_strlcpy(d.path, path.c_str(), sizeof(d.path));
            std::memcpy(d.id, id, 7);
            d.size = info.size;
            when[k] = (Sint64)info.modify_time;
            names[k] = files[i];
        }
        SDL_free(files);
    }
    std::sort(out, out + n, [](const PcLangDisc& a, const PcLangDisc& b) { return a.id[3] == 'P' && b.id[3] != 'P'; });
    return n;
}

namespace {
/* a language disc in ROM Other Regions\ by its game ID (GC6P01 European, GC6J01 Japanese); "" when there is none */
std::string find_disc(const char* id) {
    PcLangDisc discs[4];
    const int n = pc_lang_discs_list("GC6", discs, 4);
    for (int i = 0; i < n; ++i)
        if (std::strcmp(discs[i].id, id) == 0) return discs[i].path;
    return "";
}
} // namespace

/* Called once at start-up (pc_game_text_start). Finds the European disc and starts reading its text in the background;
 * `wait` != 0 (the player starts in one of its languages) waits for the read to finish. */
extern "C" void pc_region_text_start(int wait) {
#ifdef PC_HAVE_AURORA_DVD
    if (s_loader.joinable()) s_loader.join(); /* a read still running (a rescan right after start) finishes first */
    close_lazy();             /* the word pictures' handles: the discs may have changed */
    pc_word_pictures_reset(); /* and what the last read noted for them */
    pc_region_look_reset();   /* and the look table's entries the last read found */
    s_ready.store(false);
    for (Text& t : s_text) t = Text{};
    s_jp_glyphs.clear(); /* only before the game starts (the hub's rescans): nothing points into them yet */
    s_jp_fontfiles.clear();
    s_disc.clear();
    s_disc_jp.clear();
    note_card_header('P', nullptr, 0); /* the card headers noted by the last read: the discs may have changed */
    note_card_header('J', nullptr, 0);
    if (SDL_getenv("ORRE_NO_LANGUAGE_DISCS") != nullptr) return; /* a test switch: as if no language disc were added */
    s_disc = find_disc("GC6P01");
    s_disc_jp = find_disc("GC6J01");
    if (s_disc.empty() && s_disc_jp.empty()) return;
    if (!s_disc.empty())
        std::printf("[region_text] European disc: %s (German, French, Italian and Spanish text)\n", s_disc.c_str());
    if (!s_disc_jp.empty()) std::printf("[region_text] Japanese disc: %s (Japanese text)\n", s_disc_jp.c_str());
    s_loader = std::thread(read_disc, s_disc, s_disc_jp);
    if (wait) s_loader.join(); /* kept joinable otherwise: a rescan waits for it before clearing the text */
#else
    (void)wait;
#endif
}

/* Before the program ends without starting the game (the hub's window closed): waits for a read still running, so the
 * std::thread is not destroyed joinable on the way out (that calls std::terminate). pc_game_main.c. */
extern "C" void pc_region_text_finish(void) {
    if (s_loader.joinable()) s_loader.join();
}

/* the hub added or removed a language disc (pc_hub_langdiscs.cpp): read again, in the background; the game hasn't
 * started yet (the hub runs before it), so nothing reads the text meanwhile. Then the game's language is looked at
 * again (a language whose disc just arrived, or just left) */
extern "C" void pc_game_text_refresh(void);
extern "C" void pc_region_text_rescan(void) {
    pc_region_text_start(0);
    pc_game_text_refresh();
}

/* the language disc's file (jp 0: European, 1: Japanese), or "": for piece 5's files (pc_lang_files.cpp), which open
 * the disc with their own nod handle (they read on Aurora's disc thread, never this file's background reader) */
extern "C" const char* pc_region_disc_path(int jp) { return jp ? s_disc_jp.c_str() : s_disc.c_str(); }

/* 1 when the language's text can be offered: its disc was found (the text may still be loading) */
extern "C" int pc_region_text_has(const char* code) {
    const int lang = lang_index(code);
    if (lang == kJapanese) return !s_disc_jp.empty();
    return lang >= 0 && !s_disc.empty();
}

/* the disc text for a key in a European language (big-endian UTF-16 with control codes, ended by 0x0000), and its
 * length in bytes; NULL while the text is loading, or when the key isn't in that language (the game's own text then) */
extern "C" const uint8_t* pc_region_text_get(const char* code, uint32_t key, size_t* size) {
    const int lang = lang_index(code);
    if (lang < 0 || !s_ready.load()) return nullptr;
    const Text& t = s_text[lang];
    const auto it = t.at.find(key);
    if (it == t.at.end()) return nullptr;
    const uint8_t* p = t.bytes.data() + it->second;
    if (size != nullptr) *size = text_length(p, t.bytes.size() - it->second);
    return p;
}

/* One archive entry of a language disc (disc 0 the European, 1 the Japanese), read and LZSS-unpacked, for the word
 * pictures (piece 4): the entry's place as the disc read noted it (pc_word_pictures_note_entry). Returns a malloc'd
 * buffer (the caller std::frees it) and its size, or NULL (no such disc, a failed read). With nod's own handles, never
 * Aurora's DVD layer (which holds the US disc the game plays). */
extern "C" uint8_t* pc_region_read_entry(int disc, uint32_t fileIndex, uint32_t dataOff, uint32_t stored, int packed,
                                         size_t* outSize) {
    if (outSize != nullptr) *outSize = 0;
#ifdef PC_HAVE_AURORA_DVD
    if (disc < 0 || disc > 1 || stored == 0 || stored > (64u << 20)) return nullptr;
    const std::string& path = disc == PC_WP_DISC_JP ? s_disc_jp : s_disc;
    if (path.empty()) return nullptr;
    const std::lock_guard lock(s_lazyMutex);
    LazyDisc& d = s_lazy[disc];
    if (d.part == nullptr) {
        if (d.disc == nullptr && nod_disc_open(path.c_str(), nullptr, &d.disc) != NOD_RESULT_OK) {
            d.disc = nullptr;
            return nullptr;
        }
        if (nod_disc_open_partition_kind(d.disc, NOD_PARTITION_KIND_DATA, nullptr, &d.part) != NOD_RESULT_OK) {
            d.part = nullptr;
            return nullptr;
        }
    }
    NodHandle* f = nullptr;
    if (nod_partition_open_file(d.part, fileIndex, &f) != NOD_RESULT_OK) return nullptr;
    std::vector<uint8_t> raw(stored);
    const bool ok = read_at(f, dataOff, raw.data(), raw.size());
    nod_free(f);
    if (!ok) return nullptr;
    std::vector<uint8_t> data = packed ? lzss(raw.data(), raw.size()) : std::move(raw);
    if (data.empty()) return nullptr;
    auto* out = static_cast<uint8_t*>(std::malloc(data.size()));
    if (out == nullptr) return nullptr;
    std::memcpy(out, data.data(), data.size());
    if (outSize != nullptr) *outSize = data.size();
    return out;
#else
    (void)disc, (void)fileIndex, (void)dataOff, (void)stored, (void)packed;
    return nullptr;
#endif
}

/* The US disc's common_rel (the game's main code module in common.fsys, LZSS-unpacked), for the GameCube copy for
 * another region (3.5 step 9, pc_gcopy.c; snatcherLeo's call 3 of 2026-11-19): its species table gives each species' name key
 * and its "US" message banks the English names, the same way the game itself finds them, so un-nicknamed Pokémon can be
 * recognised and renamed while the game isn't running (the hub runs before it, so the game's own tables aren't loaded).
 * Read once, on first use, from the disc Aurora already has open (aurora_dvd_read_entry on the calling thread, outside
 * the DVD worker, as the start-up disc check reads), about 1.3 MB kept for the rest of the run. NULL without Aurora's
 * DVD library or when the file can't be read. Platform-neutral (Aurora's disc layer works the same on every OS). */
extern "C" const uint8_t* pc_region_us_common_rel(size_t* size) {
    static std::mutex s_mutex;
    static std::vector<uint8_t> s_rel;
    static bool s_tried = false;
    if (size != nullptr) *size = 0;
#ifdef PC_HAVE_AURORA_DVD
    const std::lock_guard lock(s_mutex);
    if (!s_tried) {
        s_tried = true;
        std::vector<uint8_t> fsys;
        const s32 entry = DVDConvertPathToEntrynum("/common.fsys");
        auto take = [](void* user, const u8* data, size_t n) {
            auto* v = static_cast<std::vector<uint8_t>*>(user);
            v->insert(v->end(), data, data + n);
        };
        if (entry >= 0 && aurora_dvd_read_entry(entry, 1u << 20, take, &fsys) > 0 && fsys.size() >= 0x20 &&
            std::memcmp(fsys.data(), "FSYS", 4) == 0) {
            /* the archive's entries (the file comment's layout): the one named common_rel, type 14 */
            const uint32_t count = be32(&fsys[0x0C]), block = be32(&fsys[0x18]);
            const uint32_t table = block + 4 <= fsys.size() ? be32(&fsys[block]) : 0;
            for (uint32_t k = 0; table != 0 && k < count && table + 4 * (k + 1) <= fsys.size(); ++k) {
                const uint32_t eo = be32(&fsys[table + 4 * k]);
                if (eo + 0x50 > fsys.size()) continue;
                const uint8_t* e = &fsys[eo];
                const uint32_t nameOff = be32(e + 0x24), dataOff = be32(e + 0x04), flags = be32(e + 0x0C),
                               stored = be32(e + 0x14);
                if (be32(e + 0x20) != 14 || nameOff >= fsys.size() || dataOff + (uint64_t)stored > fsys.size()) continue;
                if (std::strncmp(reinterpret_cast<const char*>(&fsys[nameOff]), "common_rel", fsys.size() - nameOff) != 0)
                    continue;
                s_rel = (flags & 0x80000000u) ? lzss(&fsys[dataOff], stored)
                                              : std::vector<uint8_t>(fsys.begin() + dataOff, fsys.begin() + dataOff + stored);
                break;
            }
        }
        std::printf("[region_text] the US common_rel for the GameCube copy's species names: %zu bytes\n", s_rel.size());
    }
    if (s_rel.empty()) return nullptr;
    if (size != nullptr) *size = s_rel.size();
    return s_rel.data();
#else
    return nullptr;
#endif
}

/* the Japanese disc's own glyph for a character the game's current font lacks (part D asks first): its 8-byte record
 * and, in *outBank, its bank header, both the disc's bytes as the game reads them; NULL while loading, without a
 * Japanese disc, or when its font of that id has no such character or uses other cell heights */
extern "C" const void* pc_region_font_glyph(uint16_t fontId, uint16_t code, int cellH, void** outBank) {
    if (!s_ready.load()) return nullptr;
    const auto it = s_jp_glyphs.find((uint32_t)fontId << 16 | code);
    if (it == s_jp_glyphs.end() || it->second.cellH != cellH) return nullptr;
    *outBank = const_cast<uint8_t*>(it->second.bank);
    return it->second.record;
}

/* The Japanese disc font's own picture of a character: its 4-bit pixels (rows of ceil(w / 2) bytes, high nibble
 * first, the disc's bytes as the game reads them), its font's cell height and its size; NULL while loading, without a
 * Japanese disc, or without the character. Why: the kanji tracer (pc_text_atlas.cpp, proposal 2 of the kanji review,
 * 2026-11-16) recognises a picture as one of these fonts' and traces the same character's picture from a larger font
 * when the small one is unreadable. The pixels sit at the bank's data offset plus the record's low 24 bits, as
 * pc_font_render.cpp reads them. A plain lookup; all platforms */
extern "C" const uint8_t* pc_region_font_picture(uint16_t fontId, uint16_t code, int* cellH, int* w, int* h) {
    if (!s_ready.load()) return nullptr;
    const auto it = s_jp_glyphs.find((uint32_t)fontId << 16 | code);
    if (it == s_jp_glyphs.end()) return nullptr;
    const uint8_t* rec = it->second.record;
    *cellH = it->second.cellH, *w = rec[2], *h = rec[3];
    return it->second.bank + be32(it->second.bank + 4) + (be32(rec + 4) & 0xFFFFFF);
}

/* whether the Japanese disc's font of that id has the character at all (any cell height): the naming keyboard asks
 * before it lets an input method's letter into a name, so a letter only the Japanese disc can draw still counts as
 * drawable (the project's IME work, 2026-11-15). A plain lookup; all platforms */
extern "C" int pc_region_font_has(uint16_t fontId, uint16_t code) {
    if (!s_ready.load()) return 0;
    return s_jp_glyphs.find((uint32_t)fontId << 16 | code) != s_jp_glyphs.end() ? 1 : 0;
}

/* Whether the Japanese game's own text uses a character: 1 yes, 0 no, -1 not known (no Japanese disc, or its text
 * still loading). Why (the rare-kanji rule, pc_text_atlas.cpp, 2026-11-16): a kanji whose disc picture can't be read is
 * drawn from Noto only when a player can meet it just by typing it in a name; one the game's own text uses keeps the
 * game's picture. Computed here from the Japanese disc's 9,414 messages (every code unit outside the control codes,
 * walked as text_length walks them), once, the first time it is asked after the disc is read: no list is kept in the
 * repo or shipped (a list of characters would be harmless, but this needs none and follows any disc). Thread-safe:
 * the kanji tracer's workers ask; s_text is never changed after s_ready. All platforms. */
extern "C" int pc_region_text_uses(uint16_t code) {
    static std::once_flag s_once;
    static std::vector<uint8_t> s_used; /* one bit per 16-bit code */
    if (!s_ready.load() || s_text[kJapanese].bytes.empty()) return -1;
    std::call_once(s_once, [] {
        s_used.assign(0x10000 / 8, 0);
        const std::vector<uint8_t>& b = s_text[kJapanese].bytes;
        size_t distinct = 0;
        for (size_t i = 0; i + 1 < b.size();) {
            const uint16_t c = be16(&b[i]);
            if (c == 0xFFFF) {
                i += 3 + (size_t)(i + 2 < b.size() ? control_args(b[i + 2]) : 0);
                continue;
            }
            if (c != 0 && !(s_used[c >> 3] & (1u << (c & 7)))) {
                s_used[c >> 3] |= (uint8_t)(1u << (c & 7));
                if ((c >= 0x4E00 && c <= 0x9FFF) || c == 0x3005) ++distinct;
            }
            i += 2;
        }
        std::printf("[region_text] the Japanese text uses %zu different kanji (the rare-kanji rule's \"used\")\n", distinct);
    });
    return (s_used[code >> 3] >> (code & 7)) & 1;
}
