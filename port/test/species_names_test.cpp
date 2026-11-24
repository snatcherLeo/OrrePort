/* species_names_test: un-nicknamed Pokémon in imported saves, named in every play language (Phase 3 step 3.5, step 10;
 * the project's build helper, 2026-11-19).
 *
 * Why: the game's name insert (pc_names.cpp) shows a Pokémon whose stored name is some language's name of a species as
 * that species' name in the play language, using pc_game_text_species.h's lookup. Before that is trusted on imported
 * saves, real saves from each region must map the way they should: the Japanese save's kana to the English names, the
 * PAL saves' names to German / Japanese, and nicknames left alone. This program feeds the same lookup the species names
 * of the player's own three discs (never in the repo), the way the port feeds it (English first, then Japanese, German,
 * French, Italian, Spanish), reads every Pokémon in each save's newest slot, and prints what each name maps to.
 *
 *   species_names_test --disc E <USA.iso> --disc P <Europe.iso> --disc J <Japan.iso> --save <file.gci> ...
 *                      [--expect <save file name> <party place 0-5> <language> <name>] ...
 *
 * Checks: every stored name that matches a species matches the Pokémon's own species (or, for an evolved Pokémon whose
 * stored name is its old species', is noted, the game's own edge); the species table gives 413 species and 386 names
 * per language; each --expect holds (what the party Pokémon is shown as in that language). Ends with "ALL PASSED" or
 * "FAILED".
 *
 * How the discs are read: the disc's file table (0x424), common.fsys, its first entry common_rel (LZSS-unpacked), the
 * same module the port reads (pc_region_text.cpp; docs/engine/disc-files.md). The species table and English names come
 * through pc_gcopy_region.c (the GameCube copy's own reader, reused), the other languages from the European disc's
 * banks tagged GE / FR / IT / SP and the Japanese disc's untagged banks, found by the same message keys (the same keys on
 * all three discs, verified 2026-11-19). Plain C++, no SDL: every OS. */
#include "../src/pc_game_text_species.h"

#include "../src/pc_gcopy_region.h"
#include "../src/pc_save_cipher.h"
#include "../src/pc_save_region.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
/* Windows only: the arguments as UTF-8. Windows hands a plain main() its arguments in the system's ANSI code page, so a
 * Japanese --expect name (ブラッキー) arrives as question marks; the wide command line is converted instead. Linux and
 * macOS pass UTF-8 arguments as they are, so nothing is needed there (likely; not built there yet). Declared alone, so
 * windows.h's names don't meet the rest. */
extern "C" __declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);
extern "C" __declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t*, int*);
extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned, unsigned long, const wchar_t*, int, char*, int,
                                                                    const char*, int*);
static std::vector<std::string> s_args_utf8;
static std::vector<char*> s_argv_utf8;
static void args_as_utf8(int* argc, char*** argv) {
    int n = 0;
    wchar_t** w = CommandLineToArgvW(GetCommandLineW(), &n);
    if (w == nullptr) return;
    for (int i = 0; i < n; i++) {
        const int len = WideCharToMultiByte(65001 /* CP_UTF8 */, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(len > 0 ? (size_t)len : 1, '\0');
        if (len > 0) WideCharToMultiByte(65001, 0, w[i], -1, &s[0], len, nullptr, nullptr);
        s_args_utf8.push_back(s.c_str());
    }
    for (std::string& s : s_args_utf8) s_argv_utf8.push_back(&s[0]);
    s_argv_utf8.push_back(nullptr);
    *argc = n;
    *argv = s_argv_utf8.data();
}
#endif

namespace {

int s_failed = 0, s_checks = 0;
void check(bool ok, const std::string& what) {
    s_checks++;
    if (!ok) {
        s_failed++;
        std::printf("    FAIL: %s\n", what.c_str());
    }
}

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> out;
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return out;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        out.resize((size_t)n);
        if (std::fread(out.data(), 1, out.size(), f) != out.size()) out.clear();
    }
    std::fclose(f);
    return out;
}

bool read_at(FILE* f, uint32_t off, uint8_t* out, size_t n) {
    return std::fseek(f, (long)off, SEEK_SET) == 0 && std::fread(out, 1, n, f) == n;
}

/* LZSS (docs/engine/disc-files.md): a 16-byte header, flag bytes low bit first (1 = a literal), a 4 KB window from
 * 0xFEE, a reference of 2 bytes: offset a | (b & 0xF0) << 4, length (b & 0x0F) + 3 */
std::vector<uint8_t> lzss(const uint8_t* src, size_t n) {
    std::vector<uint8_t> out;
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
            for (unsigned k = 0; k < (b & 0xF) + 3; k++) {
                const uint8_t c = win[(off + k) & 0xFFF];
                out.push_back(c);
                win[w] = c;
                w = (w + 1) & 0xFFF;
            }
        }
    }
    return out;
}

/* a disc image's common_rel, unpacked (empty when it can't be read) */
std::vector<uint8_t> disc_common_rel(const char* iso) {
    std::vector<uint8_t> none;
    FILE* f = std::fopen(iso, "rb");
    if (f == nullptr) return none;
    uint8_t h[8];
    std::vector<uint8_t> fst, arc;
    if (read_at(f, 0x424, h, 8)) {
        fst.resize(be32(h + 4));
        if (fst.size() < 12 || !read_at(f, be32(h), fst.data(), fst.size())) fst.clear();
    }
    if (!fst.empty()) {
        const uint32_t count = be32(fst.data() + 8);
        const size_t names = (size_t)count * 12;
        for (uint32_t k = 1; k < count && names < fst.size(); k++) {
            const uint8_t* e = fst.data() + (size_t)k * 12;
            if (e[0] != 0) continue; /* a folder */
            const size_t at = names + (be32(e) & 0xFFFFFF);
            if (at < fst.size() && std::strcmp(reinterpret_cast<const char*>(fst.data() + at), "common.fsys") == 0) {
                arc.resize(be32(e + 8));
                if (!read_at(f, be32(e + 4), arc.data(), arc.size())) arc.clear();
                break;
            }
        }
    }
    std::fclose(f);
    /* .fsys: entry count +0x0C, entry-table block +0x18 (its first word: the table of entry offsets); an entry: data at
     * +0x04, flags +0x0C (0x80000000 packed), stored size +0x14, name offset +0x24 */
    if (arc.size() < 0x60 || std::memcmp(arc.data(), "FSYS", 4) != 0) return none;
    const uint32_t count = be32(arc.data() + 0x0C), blk = be32(arc.data() + 0x18);
    if (count == 0 || blk + 4 > arc.size()) return none;
    const uint32_t tab = be32(arc.data() + blk);
    for (uint32_t k = 0; k < count && tab + 4 * k + 4 <= arc.size(); k++) {
        const uint32_t eo = be32(arc.data() + tab + 4 * k);
        if (eo + 0x50 > arc.size()) continue;
        const uint8_t* e = arc.data() + eo;
        const uint32_t name = be32(e + 0x24), data = be32(e + 0x04), stored = be32(e + 0x14);
        if (name >= arc.size() || std::strcmp(reinterpret_cast<const char*>(arc.data() + name), "common_rel") != 0) continue;
        if ((size_t)data + stored > arc.size()) return none;
        if (be32(e + 0x0C) & 0x80000000u) return lzss(arc.data() + data, stored);
        return std::vector<uint8_t>(arc.begin() + data, arc.begin() + data + stored);
    }
    return none;
}

std::string utf8(const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) out += (char)(0xC0 | c >> 6), out += (char)(0x80 | (c & 0x3F));
        else out += (char)(0xE0 | c >> 12), out += (char)(0x80 | ((c >> 6) & 0x3F)), out += (char)(0x80 | (c & 0x3F));
    }
    return out;
}

/* the languages, in the order the port adds them (pc_names.cpp) */
struct Lang {
    const char* code;
    char disc;
    char tag[2];
};
const Lang kLangs[] = {{"en", 'E', {'U', 'S'}}, {"ja", 'J', {0, 1}}, {"de", 'P', {'G', 'E'}},
                       {"fr", 'P', {'F', 'R'}}, {"it", 'P', {'I', 'T'}}, {"es", 'P', {'S', 'P'}}};
constexpr int kLangCount = 6;
PcGcopyNames s_names[kLangCount];
bool s_have[kLangCount];
uint32_t s_keys[PC_GCOPY_SPECIES_MAX];
int s_key_count = 0;

int species_of_key(uint32_t key) {
    for (int id = 0; id < s_key_count; id++)
        if (s_keys[id] == key) return id;
    return -1;
}

/* what pc_names.cpp shows for a stored name in a play language: the species' name there, else its English name (the
 * game's text stays English for a key the language lacks); the stored name when it is no species' name (a nickname) */
std::u16string shown(const pc_species::ReverseMap& map, const uint8_t* stored, int lang) {
    const uint32_t key = map.find(stored);
    const int id = key != 0 ? species_of_key(key) : -1;
    if (id < 0) return pc_species::read_be(stored, pc_species::kReadMax);
    const uint8_t* name = s_have[lang] ? s_names[lang].name[id] : nullptr;
    if (name == nullptr) name = s_names[0].name[id];
    return pc_species::read_be(name, pc_species::kReadMax);
}

std::u16string from_utf8(const char* s) {
    std::u16string out;
    for (const unsigned char* p = (const unsigned char*)s; *p != 0;) {
        char16_t c;
        if (*p < 0x80) c = *p++;
        else if (*p >= 0xE0) c = (char16_t)((p[0] & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F)), p += 3;
        else c = (char16_t)((p[0] & 0x1F) << 6 | (p[1] & 0x3F)), p += 2;
        out.push_back(c);
    }
    return out;
}

const char* base_name(const char* path) {
    const char* a = std::strrchr(path, '/');
    const char* b = std::strrchr(path, '\\');
    const char* c = a > b ? a : b;
    return c != nullptr ? c + 1 : path;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    args_as_utf8(&argc, &argv);
#endif
    std::vector<uint8_t> rel[3]; /* E, P, J */
    std::vector<const char*> saves;
    struct Expect {
        const char* save;
        int place;
        int lang;
        std::u16string name;
        bool seen;
    };
    std::vector<Expect> expects;
    for (int a = 1; a < argc; a++) {
        if (std::strcmp(argv[a], "--disc") == 0 && a + 2 < argc) {
            const char r = argv[a + 1][0];
            const int i = r == 'E' ? 0 : r == 'P' ? 1 : r == 'J' ? 2 : -1;
            if (i < 0) return 2;
            rel[i] = disc_common_rel(argv[a + 2]);
            std::printf("disc %c (%s): common_rel %zu bytes%s\n", r, argv[a + 2], rel[i].size(), rel[i].empty() ? " NOT READ" : "");
            check(!rel[i].empty(), "common_rel read from the disc");
            a += 2;
        } else if (std::strcmp(argv[a], "--save") == 0 && a + 1 < argc) {
            saves.push_back(argv[++a]);
        } else if (std::strcmp(argv[a], "--expect") == 0 && a + 4 < argc) {
            int lang = -1;
            for (int l = 0; l < kLangCount; l++)
                if (std::strcmp(kLangs[l].code, argv[a + 3]) == 0) lang = l;
            if (lang < 0) return 2;
            expects.push_back(Expect{argv[a + 1], std::atoi(argv[a + 2]), lang, from_utf8(argv[a + 4]), false});
            a += 4;
        } else {
            std::fprintf(stderr, "unknown argument %s (the usage is in the file's comment)\n", argv[a]);
            return 2;
        }
    }
    if (rel[0].empty() || saves.empty()) {
        std::fprintf(stderr, "usage: species_names_test --disc E <USA.iso> [--disc P ...] [--disc J ...] --save <gci> ...\n");
        return 2;
    }

    /* the species table (US) and each language's names, the way the port has them */
    s_key_count = pc_gcopy_species_keys(rel[0].data(), rel[0].size(), s_keys, PC_GCOPY_SPECIES_MAX);
    check(s_key_count == 413, "413 species in the US table");
    pc_species::ReverseMap map;
    for (int l = 0; l < kLangCount; l++) {
        const std::vector<uint8_t>& r = rel[kLangs[l].disc == 'E' ? 0 : kLangs[l].disc == 'P' ? 1 : 2];
        if (r.empty()) continue;
        int got = pc_gcopy_blob_names(r.data(), r.size(), kLangs[l].tag, s_keys, s_key_count, &s_names[l]);
        if (kLangs[l].disc == 'J') {
            /* the Japanese banks are untagged: 00 01 (the species names' bank, found so 2026-11-19) or 00 00, the two
             * forms pc_region_text.cpp's scan_blob accepts; names the first form lacks are taken from the second */
            static PcGcopyNames other;
            const char untagged[2] = {0, 0};
            pc_gcopy_blob_names(r.data(), r.size(), untagged, s_keys, s_key_count, &other);
            for (int id = 0; id < s_key_count; id++)
                if (s_names[l].name[id] == nullptr && other.name[id] != nullptr) s_names[l].name[id] = other.name[id], got++;
        }
        s_have[l] = got > 0;
        int added = 0;
        for (int id = 0; id < s_key_count; id++)
            if (s_names[l].name[id] != nullptr && map.add(s_keys[id], s_names[l].name[id], kLangs[l].code)) added++;
        std::printf("names %s: %d found, %d added; the map now holds %zu names\n", kLangs[l].code, got, added, map.size());
        check(got == 386 && added == 386, std::string("386 species names in ") + kLangs[l].code);
    }
    std::printf("names cut to 10 letters: %zu; conflicts (one word, two species): %zu\n", map.cut(), map.conflicts().size());
    for (const pc_species::Conflict& c : map.conflicts()) {
        const int kept = species_of_key(c.kept), other = species_of_key(c.other);
        std::printf("  conflict: %s is species %d (kept) and, in %s, species %d\n", utf8(c.name).c_str(), kept,
                    c.other_language, other);
    }

    /* names the real saves don't hold (their Pokémon have no nicknames): a player's nickname, a species name in other
     * capitals (a nickname to the game, so one here), and the empty name */
    {
        auto be = [](const std::u16string& s) {
            std::vector<uint8_t> out;
            for (char16_t c : s) out.push_back((uint8_t)(c >> 8)), out.push_back((uint8_t)c);
            out.push_back(0), out.push_back(0);
            return out;
        };
        const struct { const char* utf8; bool species; } kNames[] = {
            {"Behemoth", false}, {"Umbreon", false}, {"umbreon", false}, {"", false},
            {"UMBREON", true},   {"NACHTARA", true}, {"ブラッキー", true}, {"NOCTALI", true}};
        for (const auto& k : kNames) {
            const uint32_t key = map.find(be(from_utf8(k.utf8)).data());
            std::printf("made-up name \"%s\": %s\n", k.utf8, key != 0 ? "a species' name" : "a nickname (shown as it is)");
            check((key != 0) == k.species, std::string("made-up name ") + k.utf8);
        }
    }

    static uint8_t plain[PC_SAVE_SLOT_SIZE];
    for (const char* path : saves) {
        const std::vector<uint8_t> gci = read_file(path);
        const int slot = gci.empty() ? -1 : pc_save_gci_newest_slot(gci.data(), gci.size(), plain);
        if (slot < 0) {
            std::printf("\n%s: not a readable Colosseum save\n", path);
            check(false, "a readable save");
            continue;
        }
        int area = 0, language = 0;
        pc_save_stamp(plain, &area, &language);
        std::printf("\n%s: newest slot %d, stamp area %d language %d\n", path, slot, area, language);
        static PcGcopySlotName names[256];
        const int n = pc_gcopy_slot_names(plain, names, 256);
        int species_named = 0, nicknamed = 0, other_species = 0;
        for (int i = 0; i < n; i++) {
            const PcGcopySlotName& nm = names[i];
            if (nm.kind != PC_GCOPY_NAME_POKEMON) continue;
            const std::u16string stored = pc_species::read_be(nm.text, (size_t)nm.units);
            const uint32_t key = map.find(nm.text);
            const int id = key != 0 ? species_of_key(key) : -1;
            const bool party = nm.where < 6;
            if (id < 0) nicknamed++;
            else if (id == nm.species) species_named++;
            else other_species++;
            /* a match must be the Pokémon's own species; another species' name is either an evolution of an imported
             * Pokémon (the US program didn't rename it: the game's edge, noted) or a nickname that is a species name */
            if (!party && id == nm.species) continue; /* boxes: counted, the party printed */
            std::printf("  %s %3d species %3d \"%s\": ", party ? "party" : (nm.where == 200 ? "daycare" : "box  "),
                        party ? nm.where : nm.where - 100, nm.species, utf8(stored).c_str());
            if (id < 0) {
                std::printf("no species' name in any language: a nickname, shown as it is\n");
                continue;
            }
            if (id != nm.species) std::printf("[the name of species %d, not its own] ", id);
            for (int l = 0; l < kLangCount; l++)
                std::printf("%s %s%s", kLangs[l].code, utf8(shown(map, nm.text, l)).c_str(), l + 1 < kLangCount ? " | " : "\n");
            for (Expect& e : expects)
                if (party && std::strcmp(e.save, base_name(path)) == 0 && e.place == nm.where) {
                    e.seen = true;
                    const std::u16string got = shown(map, nm.text, e.lang);
                    check(got == e.name, std::string(base_name(path)) + " party " + std::to_string(e.place) + " in " +
                                             kLangs[e.lang].code + ": " + utf8(got) + ", expected " + utf8(e.name));
                }
        }
        std::printf("  Pokémon: %d named their own species (mapped), %d nicknamed (kept), %d named another species\n",
                    species_named, nicknamed, other_species);
        /* expectations about party places that hold a nickname (shown unchanged) */
        for (Expect& e : expects)
            if (!e.seen && std::strcmp(e.save, base_name(path)) == 0)
                for (int i = 0; i < n; i++)
                    if (names[i].kind == PC_GCOPY_NAME_POKEMON && names[i].where == e.place) {
                        e.seen = true;
                        const std::u16string got = shown(map, names[i].text, e.lang);
                        check(got == e.name, std::string(base_name(path)) + " party " + std::to_string(e.place) + " in " +
                                                 kLangs[e.lang].code + ": " + utf8(got) + ", expected " + utf8(e.name));
                    }
    }
    for (const Expect& e : expects) check(e.seen, std::string("expected Pokémon found: ") + e.save + " " + std::to_string(e.place));
    std::printf("\n%d checks: %s\n", s_checks, s_failed == 0 ? "ALL PASSED" : "FAILED");
    return s_failed == 0 ? 0 : 1;
}
