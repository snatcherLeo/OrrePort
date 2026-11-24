/*
 * pc_game_text_species.h: which species a stored Pokémon name is, in any language the player's discs have (Phase 3
 * step 3.5, step 10; the project's build helper, 2026-11-19).
 *
 * Why: an un-nicknamed Pokémon shows its species name in the play language (decided 2026-11-15, widened 2026-11-17 by
 * snatcherLeo: display only, the save is never changed). The game decides "no nickname" by "the stored name equals the species
 * name" (pokemon_range_8011F5FC.c), and the stored name is in the language of the game that wrote it: the US program
 * writes English, an imported Japanese save holds kana (ブラッキー), a German one German (NACHTARA), a Spanish or
 * Italian one the English names (their discs keep them, Italian's MR.MIME aside; verified 2026-11-19 by
 * species_names_test, docs/engine/regions.md). So a stored name
 * is looked up in every available language's species names at once: a match gives the species, which is then shown in
 * the play language; no match in any language means the player named it, and the name stays.
 *
 * What this file is: the pure lookup, fed one language's names at a time (message key + the game's text), with no SDL,
 * no game and no disc code, so pc_names.cpp (the game's name insert, feeding it the US text from the game and the other
 * languages from pc_region_text.cpp) and port/test/species_names_test.cpp (feeding it the three discs' tables directly)
 * run exactly the same code. Header-only C++17.
 *
 * Rules (each checked on the four real region saves by the test):
 *   - Names compare exactly, code unit for code unit, as the game compares them. No case folding: the save stores the
 *     message's own capitals (verified 2026-11-19: the PAL and Japanese saves' names equal their disc's text exactly), and a
 *     Pokémon the player typed as "Umbreon" is a nickname to the game, so it stays one here.
 *   - A name longer than the game's 10 letters is also known by its first 10 (the game cuts what it stores to 10 with
 *     GScharLenCpy(dst, src, 11)). Gen 3 names are all 10 letters or fewer in the five languages (verified by the test's
 *     count of cut names: 0), so this is a guard only.
 *   - A text holding a control code (0xFFFF) is not a species name and is skipped.
 *   - The same word in two languages for the same species (Spanish and Italian keep the English names) is one entry. The
 *     same word for two different species would be a conflict: the language added first keeps it (English first, the
 *     language the US program itself writes) and the conflict is counted, so the test can list it. None exist across
 *     the six languages of the three discs (verified 2026-11-19).
 * Platforms: plain C++, the same on Windows, Linux and macOS.
 * XD: the same idea (likely: XD keeps a nickname-equals-species rule and per-language species messages); its species
 * keys and table are its own.
 */
#ifndef PC_GAME_TEXT_SPECIES_H
#define PC_GAME_TEXT_SPECIES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace pc_species {

/* the game keeps at most 10 letters of a name (its setters copy 11 code units with the end) */
constexpr size_t kNameLetters = 10;
/* the longest text read as a name: well past any species name (10) and any stored name, so a stray unended text stops */
constexpr size_t kReadMax = 32;

/* a text in the game's form (big-endian UTF-16 bytes, ended by 0x0000) as code units in host order, up to max units;
 * ok is false when it holds a control code (0xFFFF) */
inline std::u16string read_be(const uint8_t* be, size_t max, bool* ok = nullptr) {
    std::u16string s;
    bool good = be != nullptr;
    for (size_t i = 0; be != nullptr && i < max; ++i) {
        const char16_t c = (char16_t)(be[2 * i] << 8 | be[2 * i + 1]);
        if (c == 0) break;
        if (c == 0xFFFF) good = false;
        s.push_back(c);
    }
    if (ok != nullptr) *ok = good && !s.empty();
    return s;
}

struct Conflict {
    std::u16string name;
    uint32_t kept, other; /* message keys: the species kept for this word, and the other species also called so */
    const char* other_language;
};

class ReverseMap {
public:
    /* one language's name for the species whose name message is key (the game's text: big-endian UTF-16, ended by 0).
     * Returns whether it was taken (false: empty, or a control code in it) */
    bool add(uint32_t key, const uint8_t* be, const char* language) {
        bool ok = false;
        const std::u16string name = read_be(be, kReadMax, &ok);
        if (!ok || key == 0) return false;
        put(name, key, language);
        if (name.size() > kNameLetters) { /* what the game would store: the first 10 letters */
            put(name.substr(0, kNameLetters), key, language);
            cut_++;
        }
        return true;
    }
    /* the species name key a stored name (the game's text) is some language's name of, or 0 (a nickname) */
    uint32_t find(const uint8_t* be) const {
        bool ok = false;
        const std::u16string name = read_be(be, kReadMax, &ok);
        if (!ok) return 0;
        const auto it = by_name_.find(name);
        return it != by_name_.end() ? it->second : 0;
    }
    uint32_t find(const std::u16string& name) const {
        const auto it = by_name_.find(name);
        return it != by_name_.end() ? it->second : 0;
    }
    size_t size() const { return by_name_.size(); }
    size_t cut() const { return cut_; }
    const std::vector<Conflict>& conflicts() const { return conflicts_; }

private:
    void put(const std::u16string& name, uint32_t key, const char* language) {
        const auto r = by_name_.emplace(name, key);
        if (!r.second && r.first->second != key) conflicts_.push_back(Conflict{name, r.first->second, key, language});
    }
    std::unordered_map<std::u16string, uint32_t> by_name_;
    std::vector<Conflict> conflicts_;
    size_t cut_ = 0;
};

} // namespace pc_species

#endif
