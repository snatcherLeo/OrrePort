/*
 * pc_names.cpp: the full names beside the save's GameCube names, and the extras file (step 2.4, 2026-11-14).
 *
 * Why: OrrePort's names take 12 letters (Wes, Rui, nicknames, boxes; decided 2026-11-10 / 2026-11-14), but the game
 * stores at most 10 letters of any name (its setters copy with GScharLenCpy(dst, src, 11)), a box name at most 8, and
 * a GameCube can show only 7 for the player and Rui. The rule (snatcherLeo, 2026-11-13): **the save always holds a
 * GameCube-safe name**, so the `.gci` in a slot is always Dolphin-ready, and OrrePort keeps the full name beside it
 * and shows it wherever the game draws a name. A name that differs from its GameCube version is what makes the save
 * an `.orresave`. docs/port/orresave.md has the plan and the map of the game's name code.
 *
 * How:
 *   - The naming keyboard (pc_name_keyboard.cpp) types up to 12 letters and gives the game the GameCube name. When
 *     the screen stores the name (OrreHook_NameEntryCommit) it calls pc_names_commit with both.
 *   - The full names live here: the player's, Rui's, each box's, and each Pokémon's nickname keyed by its record's
 *     personality value (+0x04) and trainer ID (+0x14), so it follows the Pokémon between the party, the PC and the
 *     daycare.
 *   - Showing them: OrreHook_MessageInsert, where the game inserts a name into any text (dialogue, battle messages,
 *     menus built from text): an inserted string equal to a stored GameCube name is swapped for the full name. For a
 *     nickname shared by two Pokémon, the record around the string (text - 0x2E) picks the right one; for one whose
 *     GameCube form is its species name, the record around it, the party menu's copy (OrreHook_MenuNameCopy) or, in
 *     battle menus, the Pokémon on the field (battle_owner; 2026-11-16).
 *   - The extras file, `Slot N\extras.ini`: written right after each save lands (OrreHook_CardTaskEnd, task 4 result
 *     4) and read when a save loads (task 1 result 3). It records the `.gci`'s SHA-1, so a save changed elsewhere is
 *     noticed. Names whose GameCube name no longer matches the save are simply not used (the save's own name wins).
 *     A slot with an extras file is an ORRESAVE; when a save no longer needs one, it is removed.
 *   - Everything the game compares (whose Pokémon this is, evolution's rename check) keeps seeing the GameCube name,
 *     since the save only ever holds that.
 *
 * Accurate mode: the game's own name screen and limits run there (pc_name_keyboard.cpp), so no long name is ever made.
 * A save with extras plays in accurate mode only through a GameCube copy (the hub, part 4).
 * Platforms: plain C++ over SDL's file calls; the same on Windows, Linux and macOS.
 * XD: the same method (the message walker and card tasks are shared engine code; likely); XD's record offsets and name
 * screens are its own.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#ifdef _WIN32
/* the fit log's caller trace (ORRE_FIT_LOG=2; Windows only, a test aid): declared alone, so windows.h's names don't
 * meet the GameCube types below */
extern "C" __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long, unsigned long, void**,
                                                                                   unsigned long*);
#endif

#include "pc_font_files.h" /* the stand-in codes for characters past U+FFFF (emoji in names) */
#include "pc_game_text_species.h" /* which species a stored name is, in any language (3.5 step 10) */
#include "pc_region_text.h"       /* the other languages' species names, from the player's language discs */
#include "pc_grapheme.h"   /* PC_NAME_MAX_CPS: how many characters a 12-letter name can hold (2026-11-16) */
#include "pc_lowmem.h"
#include "pc_names.h"
#include "pc_options.h"
#include "pc_rtl.h" /* right-to-left names drawn as read (shown, below) */
#include "pc_shape.h" /* names shaped by HarfBuzz in SheenBidi's order (shown, below; 2026-11-16) */
#include "pc_saves.h"

extern "C" {
#include "dolphin/types.h"
extern void* (*OrreHook_MessageInsert)(u32 control, void* text);
extern void (*OrreHook_CardTaskEnd)(s32 task_kind, s32 result);
extern void (*OrreHook_MenuNameCopy)(void* copy, const void* record);
extern void (*OrreHook_MessageFit)(u32 key, u8* task);
extern s32 (*OrreHook_MessageFitRect)(u32 key, s32 rect);
int pc_name_keyboard_full(const void* text, uint16_t* out, int cap); /* pc_name_keyboard.cpp: "Is <name> OK?" */
const char* pc_game_text_language(void);                                   /* pc_game_text.cpp */
const void* pc_game_text_translate(uint32_t key, const void* original);     /* pc_game_text.cpp */
}
#include "pc_game_ui.h"

namespace {

using Units = std::vector<uint16_t>; /* the game's text: big-endian code units as they sit in memory, no 0 */

struct Name {
    Units full;    /* as typed, up to 12 visible letters (at most PC_NAME_MAX_CPS characters) */
    Units gc;      /* the GameCube name the save holds */
    Units species; /* a nickname's species name (the hub's label, read without the game running) */
};

Name s_hero, s_partner, s_box[3];
std::map<uint64_t, Name> s_nick; /* key: personality value << 32 | trainer ID, as the record holds them */
std::map<Units, uint8_t*> s_low;  /* full names copied below 4 GB (the game keeps text pointers in 32-bit slots) */
std::map<const void*, uint64_t> s_copy; /* a menu copy of a nickname -> its Pokémon (OrreHook_MenuNameCopy) */
void* (*s_prevInsert)(u32, void*) = nullptr;

/* *Changed 2026-11-16 (visible letters, pc_grapheme.h):* a name of 12 visible letters can be up to PC_NAME_MAX_CPS
 * (132) characters (twelve emoji families are 60), so the cap that keeps a stray unended text from running on is that,
 * not 64: a full name taken from the keyboard is never cut */
Units take(const uint16_t* p) {
    Units u;
    for (int i = 0; p != nullptr && p[i] != 0 && i < PC_NAME_MAX_CPS; i++) u.push_back(p[i]);
    return u;
}
bool same(const uint16_t* p, const Units& u) {
    if (p == nullptr || u.empty()) return false;
    size_t i = 0;
    for (; i < u.size(); i++)
        if (p[i] != u[i]) return false;
    return p[i] == 0;
}
uint8_t* low_copy(const Units& u) {
    auto it = s_low.find(u);
    if (it != s_low.end()) return it->second;
    uint8_t* p = static_cast<uint8_t*>(pc_lowmem_alloc(u.size() * 2 + 2));
    if (p == nullptr) return nullptr;
    std::memcpy(p, u.data(), u.size() * 2);
    p[u.size() * 2] = p[u.size() * 2 + 1] = 0;
    s_low[u] = p;
    return p;
}
/* a nickname's key: the Pokémon's personality value (+0x04) and its trainer's ID (+0x14), so extras.ini's
 * "nick.<16 hex digits>" is the real PID then the real OT ID, the numbers the .gci holds big-endian (a tool can match
 * them). Both words are read in the PC's own byte order: the save's numbers are swapped to native order when the card
 * file loads (memcard.c with pc_save_fields.c, whose list covers +0x04 and +0x14 of every party, box and daycare
 * record), and records the game makes in memory (a catch, a battle's copy) are native already. On a big-endian host the
 * same read gives the same numbers. Platforms: all.
 * *Corrected 2026-11-16 (the project's build helper; verified):* version 2 files (written 2026-11-15 to 2026-11-16) read the
 * words big-endian, on the belief that the record stays in GameCube order in memory; it doesn't, so their keys have each
 * 32-bit half byte-swapped (snatcherLeo's Slot 1: RAIKOU's real PID / OT ID 72723B52 / CB58D825, decoded from its .gci, sat
 * under "nick.523b727225d858cb"). Lookups still worked, since every key in a run came from the same read (and version 1
 * files were swapped on loading to match), but the file didn't mean what it said. Version 1 files were right all along.
 * Files are now written as version 3 (real numbers, as version 1); version 2 files are swapped back on reading. */
uint64_t key_of(const uint8_t* record) {
    uint32_t pid, ot;
    std::memcpy(&pid, record + 0x04, 4);
    std::memcpy(&ot, record + 0x14, 4);
    return (uint64_t)pid << 32 | ot;
}

/* a version 2 key (each 32-bit half byte-swapped, above) as the real one */
uint64_t key_from_v2(uint64_t k) {
    auto swap = [](uint32_t v) { return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24); };
    return (uint64_t)swap((uint32_t)(k >> 32)) << 32 | swap((uint32_t)k);
}

/* the game's text <-> UTF-8, for the extras file (one code unit is one character: the game's text has no pairs).
 * *Updated 2026-11-16 (the Noto wiring):* a character past U+FFFF (an emoji in a name) lives in the game's text as a
 * stand-in code for this run only (pc_font_files.h); the extras file always holds the real character, four UTF-8
 * bytes, and reading it back gives it a stand-in again. So an emoji survives a save and reload, and the file means the
 * same to any tool (and to a later run, whose stand-ins may be numbered differently). */
std::string to_utf8(const Units& u) {
    std::string out;
    for (uint16_t be : u) {
        const uint16_t unit = (uint16_t)(*reinterpret_cast<const uint8_t*>(&be) << 8 | reinterpret_cast<const uint8_t*>(&be)[1]);
        const uint32_t c = pc_text_real(unit);
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) out += (char)(0xC0 | c >> 6), out += (char)(0x80 | (c & 0x3F));
        else if (c < 0x10000) out += (char)(0xE0 | c >> 12), out += (char)(0x80 | ((c >> 6) & 0x3F)), out += (char)(0x80 | (c & 0x3F));
        else
            out += (char)(0xF0 | c >> 18), out += (char)(0x80 | ((c >> 12) & 0x3F)), out += (char)(0x80 | ((c >> 6) & 0x3F)),
                out += (char)(0x80 | (c & 0x3F));
    }
    return out;
}
/* *Changed 2026-11-16 (visible letters):* read up to PC_NAME_MAX_CPS characters (was 32, which cut a name of seven emoji
 * families on reloading) */
Units from_utf8(const std::string& s) {
    Units u;
    for (size_t i = 0; i < s.size() && u.size() < (size_t)PC_NAME_MAX_CPS;) {
        const unsigned char b = (unsigned char)s[i];
        uint32_t c;
        int n;
        if (b < 0x80) c = b, n = 1;
        else if (b >= 0xF0) c = b & 0x07, n = 4;
        else if (b >= 0xE0) c = b & 0x0F, n = 3;
        else c = b & 0x1F, n = 2;
        for (int k = 1; k < n && i + k < s.size(); k++) c = c << 6 | ((unsigned char)s[i + k] & 0x3F);
        i += n;
        if (c >= 0x10000) c = pc_text_standin(c);
        if (c == 0) c = 0xFFFD; /* the stand-in table is full (6,400 different characters in a run): unreadable */
        uint16_t be;
        reinterpret_cast<uint8_t*>(&be)[0] = (uint8_t)(c >> 8);
        reinterpret_cast<uint8_t*>(&be)[1] = (uint8_t)c;
        u.push_back(be);
    }
    return u;
}

void clear_all() {
    s_hero = Name{};
    s_partner = Name{};
    for (Name& b : s_box) b = Name{};
    s_nick.clear();
}

/* ---- un-nicknamed Pokémon in the chosen language (decided 2026-11-15, snatcherLeo approved the project's draft: "a Pokémon with no
 * nickname shows its species name in the current language, not the name stored when it was caught") ----
 * The game's own rule for "no nickname" is "the nickname equals the species name" (an evolution renames exactly those,
 * pokemon_range_8011F5FC.c). The save keeps the US names (pc_game_text.cpp keeps species names the disc's whenever the
 * game copies or compares them), so a name inserted into text that equals some species' US name is shown as that
 * species' name in the chosen language. A Pokémon nicknamed exactly as a species' US name counts as un-nicknamed, as it
 * does for the game itself. Where the game prints a name some other way than by inserting it, it stays as stored
 * (to be found in hand checks). Platforms: all.
 * *Widened 2026-11-19 (3.5 step 10; snatcherLeo's decision of 2026-11-17, "Pokémon names follow the play language when not
 * nicknamed"; the project's build helper):* imported saves too. A European or Japanese save stores its un-nicknamed Pokémon
 * under the species name in its own language (ブラッキー, NACHTARA; Spanish and Italian keep the English names), so a
 * stored name is now looked up in every language the player's discs have (pc_game_text_species.h): the US text from the
 * game itself, then Japanese, German, French, Italian and Spanish from pc_region_text.cpp, each added once its disc's
 * text is ready (the background read: until then the language is simply not matched yet; nothing waits). The English
 * early return is gone, so a Japanese save played in English shows UMBREON. A name that is no species' name in any of
 * those languages is a nickname and stays as stored.
 * The game's own edge (its behaviour, not changed here): the US program's "no nickname" test compares with the US name
 * only, so an imported un-nicknamed ブラッキー's pre-evolution (イーブイ) isn't renamed by the game when it evolves; its
 * stored name stays イーブイ, and this display then names the old species (EEVEE for an UMBREON). The save itself is
 * never changed by this; the GameCube copy for another region renames such names in the copy (pc_gcopy_region.c). */
const void* species_in_language(const uint16_t* t) {
    static pc_species::ReverseMap names; /* any language's species name -> its message key (the US key) */
    static std::vector<uint32_t> keys;    /* every species' name key, from the game's table (built once) */
    static bool added[5] = {};            /* kOther's languages, each added once its text is ready */
    static const char* const kOther[5] = {"ja", "de", "fr", "it", "es"}; /* after English: the order conflicts keep */
    const char* lang = pc_game_text_language();
    if (lang == nullptr || pc_game_ui.species_key == nullptr || pc_game_ui.msg_string == nullptr) return nullptr;
    if (keys.empty()) {
        for (uint16_t id = 1; id < 0x19C; ++id) {
            const uint32_t key = pc_game_ui.species_key(id);
            if (key == 0) continue;
            keys.push_back(key);
            names.add(key, reinterpret_cast<const uint8_t*>(pc_game_ui.msg_string(key)), "en");
        }
        if (keys.empty()) return nullptr;
    }
    for (int i = 0; i < 5; ++i) {
        /* not found, or still being read (pc_region_text_get answers NULL until the background read is done: a lookup,
         * never a wait). 386 lookups once per language: well under a millisecond */
        if (added[i] || !pc_region_text_has(kOther[i]) || pc_region_text_get(kOther[i], keys.front(), nullptr) == nullptr)
            continue;
        for (const uint32_t k : keys) names.add(k, pc_region_text_get(kOther[i], k, nullptr), kOther[i]);
        added[i] = true;
    }
    const uint32_t key = names.find(reinterpret_cast<const uint8_t*>(t));
    if (key == 0) return nullptr; /* a nickname */
    const uint16_t* us = pc_game_ui.msg_string(key);
    /* the play language's name; English (or a key that language lacks): the game's own US text */
    const void* shownName = std::strcmp(lang, "en") == 0 ? nullptr : pc_game_text_translate(key, us);
    if (shownName == nullptr) shownName = us;
    if (shownName == nullptr || same(t, take(static_cast<const uint16_t*>(shownName)))) return nullptr; /* already so */
    return shownName;
}

/* ---- showing the full names ---- */
/* *Added 2026-11-16 (the project's build helper; right-to-left names, pc_rtl.h):* the name as it is drawn. An Arabic or
 * Hebrew name is kept as typed (letters in the order they were typed, Arabic in its plain letters), but the game's text
 * draws one letter after another, left to right, with no shaping; so the copy handed to the game gets Arabic's joined
 * forms and the name's right-to-left runs reversed (pc_rtl_display). Any other name comes back as it is. Draw time only:
 * s_hero / s_nick / s_box and the extras file keep the typed order, and the GameCube name (what the game stores and
 * compares) is never changed. Stand-in codes (an emoji, pc_font_files.h) are classed by their real letter and kept as
 * the same stand-in. Platforms: all. */
/* *Changed 2026-11-16 (the project's build helper; HarfBuzz and SheenBidi, pc_shape.h):* a name that needs shaping (Indic,
 * Thai, Khmer, Arabic, Hebrew, an emoji with a skin tone or a family) is shaped as one run: each shaped cluster becomes
 * one stand-in code that part D draws from its glyphs, in SheenBidi's drawn order; characters outside the shaped runs
 * stay the typed unit (or its mirror). If shaping fails, or with ORRE_SHAPING=0, the pc_rtl pass below as before.
 * Stored as typed either way: only this drawn copy changes. */
Units shown(const Units& typed) {
    std::vector<uint32_t> cps(typed.size());
    for (size_t i = 0; i < typed.size(); ++i) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&typed[i]);
        cps[i] = pc_text_real((uint16_t)(b[0] << 8 | b[1]));
    }
    auto unit = [](uint32_t c) {
        uint16_t be;
        reinterpret_cast<uint8_t*>(&be)[0] = (uint8_t)(c >> 8);
        reinterpret_cast<uint8_t*>(&be)[1] = (uint8_t)c;
        return be;
    };
    if (pc_shape_needed(cps.data(), (int)cps.size())) {
        std::vector<PcShapedItem> items(cps.size());
        const int m = pc_shape_name(cps.data(), (int)cps.size(), items.data(), (int)items.size());
        if (m >= 0) {
            Units out;
            for (int i = 0; i < m; ++i) {
                const PcShapedItem& it = items[(size_t)i];
                if (it.cluster != 0) out.push_back(unit(it.cluster));
                else if (it.cp == cps[(size_t)it.first]) out.push_back(typed[(size_t)it.first]); /* keeps a stand-in */
                else if (it.cp <= 0xFFFF) out.push_back(unit(it.cp)); /* a mirrored bracket */
            }
            return out;
        }
    }
    /* without shaping, an emoji's modifiers (a skin tone, a joiner) are left out of the drawn copy, so the first emoji
     * shows alone as before (pc_font_files.cpp's pc_text_is_modifier) */
    bool mods = false;
    for (uint32_t c : cps) mods |= pc_text_is_modifier(c) != 0;
    if (mods) {
        Units kept;
        std::vector<uint32_t> keptCps;
        for (size_t i = 0; i < cps.size(); ++i) {
            if (pc_text_is_modifier(cps[i]) || (i > 0 && cps[i - 1] == 0x200D)) continue;
            kept.push_back(typed[i]);
            keptCps.push_back(cps[i]);
        }
        if (!pc_rtl_needed(keptCps.data(), (int)keptCps.size())) return kept;
        return shown(kept);
    }
    if (!pc_rtl_needed(cps.data(), (int)cps.size())) return typed;
    std::vector<uint32_t> vis(cps.size());
    std::vector<int> from(cps.size());
    const int m = pc_rtl_display(cps.data(), (int)cps.size(), vis.data(), from.data(), 0);
    Units out;
    for (int i = 0; i < m; ++i) {
        const int k = from[(size_t)i];
        if (vis[(size_t)i] == cps[(size_t)k]) { /* unchanged: the typed unit itself (keeps a stand-in a stand-in) */
            out.push_back(typed[(size_t)k]);
            continue;
        }
        uint16_t be; /* a presentation form or mirrored bracket: all within 16 bits */
        reinterpret_cast<uint8_t*>(&be)[0] = (uint8_t)(vis[(size_t)i] >> 8);
        reinterpret_cast<uint8_t*>(&be)[1] = (uint8_t)vis[(size_t)i];
        out.push_back(be);
    }
    return out;
}
uint8_t* low_shown(const Units& typed) { return low_copy(shown(typed)); }

/* *Added 2026-11-16 (the project's build helper; the battle HP box showed ESPEON for a Hebrew nickname, verified):* which
 * Pokémon on the battle field a name in a battle menu belongs to, as its key (0: none, or no way to tell).
 * Why it's needed: a nickname with no GameCube letters keeps the species name in the save (the closest-fit rule), and
 * that text is also every other Pokémon of that species' name, so it is swapped only where the Pokémon is known. Battle
 * messages insert the name from the battle's copy of the record (the record around the text names it), the party menu
 * from a copy OrreHook_MenuNameCopy saw; but the HP boxes and the command menu's name line print from copies of that
 * copy: fightOutPokemonToMenuPokemonStatus fills a record on the stack (the hook sees that address), then the status
 * window and the battle menus copy it again (fight_menu.c: `copy = status`, menuOpenCustom), so the text the insert
 * gets sits at an address nobody registered (verified with a name log, 2026-11-16: the HP boxes' names at 0x80B7EDE0
 * and 0x809D4480, the registered copy at 0x40E12CD0).
 * How: the Pokémon on the field now (pc_game_ui.battle_field_records), those whose nickname is this text. One of them
 * (or several with the same key): that one. Two different Pokémon with the same name on the field (the player's
 * Hebrew-nicknamed ESPEON against a foe's ESPEON): the text is taken as a battle status record copy (MenuPokemonStatus:
 * level at +0x17, maximum HP at +0x18, native order) and matched on level and maximum HP; still two, or a print that
 * isn't such a record (the command menu's name line): no key, so the species name shows, as before (rare, and never
 * the wrong Pokémon's name). Outside a battle: none. Platforms: all. */
uint64_t battle_owner(const uint16_t* t) {
    if (pc_game_ui.battle_field_records == nullptr) return 0;
    void* recs[8];
    const int n = pc_game_ui.battle_field_records(recs, 8);
    const Units text = take(t);
    const uint8_t* match[8];
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const uint8_t* r = static_cast<const uint8_t*>(recs[i]);
        if (reinterpret_cast<const uint8_t*>(t) == r + 0x2E) return key_of(r); /* the record's own name: it says whose */
        if (take(reinterpret_cast<const uint16_t*>(r + 0x2E)) != text) continue;
        bool dup = false;
        for (int j = 0; j < m; ++j) dup |= key_of(match[j]) == key_of(r);
        if (!dup) match[m++] = r;
    }
    if (m == 1) return key_of(match[0]);
    if (m < 2) return 0;
    const uint8_t* b = reinterpret_cast<const uint8_t*>(t);
    uint16_t maxHp;
    std::memcpy(&maxHp, b + 0x18, 2);
    const uint8_t* only = nullptr;
    for (int j = 0; j < m; ++j) {
        uint16_t recHp;
        std::memcpy(&recHp, match[j] + 0x8C, 2); /* stats[0], the maximum HP (pokemon-data.md) */
        if (match[j][0x60] != b[0x17] || recHp != maxHp) continue;
        if (only != nullptr) return 0;
        only = match[j];
    }
    return only != nullptr ? key_of(only) : 0;
}

void* insert(u32 control, void* text) {
    if (s_prevInsert != nullptr) { /* a test switch (ORRE_TEST_HERO_NAME) wins */
        if (void* other = s_prevInsert(control, text)) return other;
    }
    const uint16_t* t = static_cast<const uint16_t*>(text);
    if (t == nullptr || t[0] == 0) return nullptr;
    if (pc_option_gamecube_accurate()) /* accurate: the save's own long names; species names still in the language */
        return const_cast<void*>(species_in_language(t));
    uint16_t typed[PC_NAME_MAX_CPS + 1]; /* a whole 12-letter name (pc_grapheme.h: up to 132 characters) */
    /* every full name goes out in its drawn form (shown, above: right-to-left names joined and reordered) */
    if (pc_name_keyboard_full(t, typed, PC_NAME_MAX_CPS + 1)) return low_shown(take(typed)); /* "Is <name> OK?" before it's stored */
    if (!s_hero.full.empty() && same(t, s_hero.gc)) return low_shown(s_hero.full);
    if (!s_partner.full.empty() && same(t, s_partner.gc)) return low_shown(s_partner.full);
    if (auto c = s_copy.find(t); c != s_copy.end()) { /* the party menu's or HP box's copy of a nickname */
        auto it = s_nick.find(c->second);
        if (it != s_nick.end() && same(t, it->second.gc)) return low_shown(it->second.full);
    }
    const Name* found = nullptr;
    int matches = 0;
    bool fieldDone = false;
    uint64_t fieldKey = 0;
    for (const auto& kv : s_nick) {
        if (!same(t, kv.second.gc)) continue;
        /* *Added 2026-11-15 (the IME work):* a nickname with no GameCube letters at all (all kanji, say) keeps the
         * game's fallback, which for a Pokémon without a nickname is its species name; that text is every same-species
         * Pokémon's name too (a wild one, an opponent's), so it counts only inside this Pokémon's own record.
         * *Fixed 2026-11-16:* or, in a battle, when the field says whose it is (battle_owner, above): the HP boxes and
         * the command menu's name line */
        if (kv.second.gc == kv.second.species) {
            if (key_of(reinterpret_cast<const uint8_t*>(t) - 0x2E) == kv.first) return low_shown(kv.second.full);
            if (!fieldDone) fieldKey = battle_owner(t), fieldDone = true;
            if (fieldKey == kv.first) return low_shown(kv.second.full);
            continue;
        }
        found = &kv.second, matches++;
    }
    if (matches > 1) { /* two Pokémon share the GameCube name: the record around the string decides */
        auto it = s_nick.find(key_of(reinterpret_cast<const uint8_t*>(t) - 0x2E));
        found = (it != s_nick.end() && same(t, it->second.gc)) ? &it->second : nullptr;
    }
    if (found != nullptr) return low_shown(found->full);
    for (const Name& b : s_box)
        if (!b.full.empty() && same(t, b.gc)) return low_shown(b.full);
    return const_cast<void*>(species_in_language(t));
}

/* ---- the extras file ---- */
std::string extras_path() {
    char dir[1200];
    pc_saves_slot_dir(pc_saves_current(), dir, sizeof(dir));
    return std::string(dir) + "extras.ini";
}

/* only names still in the save: the player's and Rui's when the save holds their GameCube name, a nickname when its
 * Pokémon is still there with that name, a box name when the box still has it */
void prune() {
    if (!s_hero.full.empty() && !same(pc_game_ui.hero_name(), s_hero.gc)) s_hero = Name{};
    if (!s_partner.full.empty() && !same(pc_game_ui.partner_name(), s_partner.gc)) s_partner = Name{};
    for (int b = 0; b < 3; b++)
        if (!s_box[b].full.empty() && !same(pc_game_ui.box_name(b), s_box[b].gc)) s_box[b] = Name{};
    void* recs[128];
    const int n = pc_game_ui.pokemon_records(recs, 128);
    std::map<uint64_t, Name> kept;
    for (int i = 0; i < n; i++) {
        const uint8_t* r = static_cast<const uint8_t*>(recs[i]);
        auto it = s_nick.find(key_of(r));
        if (it != s_nick.end() && same(reinterpret_cast<const uint16_t*>(r + 0x2E), it->second.gc)) {
            kept[it->first] = it->second;
            if (pc_game_ui.species_name != nullptr) kept[it->first].species = take(pc_game_ui.species_name(r)); /* evolved? */
        }
    }
    s_nick.swap(kept);
}

void write_extras() {
    prune();
    const std::string path = extras_path();
    const bool any = !s_hero.full.empty() || !s_partner.full.empty() || !s_nick.empty() || !s_box[0].full.empty() ||
                     !s_box[1].full.empty() || !s_box[2].full.empty();
    if (!any) {
        /* the save needs no extras any more (every name fits a GameCube again): it is a plain .gci slot again. The
         * file is OrrePort's own, so it is removed, not kept beside a save it no longer describes */
        if (SDL_GetPathInfo(path.c_str(), nullptr)) {
            SDL_RemovePath(path.c_str());
            std::printf("[names] no long names left: extras.ini removed (a .gci slot again)\n");
        }
        return;
    }
    char digest[41];
    pc_saves_save_digest(pc_saves_current(), digest);
    std::string t = "# OrrePort's extras for this save: what a GameCube save can't hold (docs/port/orresave.md).\n"
                    "# gci = the SHA-1 of the .gci these belong to; each name: as typed (full) and as the save holds it (gc).\n"
                    "# nick.<PID><OT ID> (16 hex digits, the Pokemon's personality value then its trainer's ID).\n"
                    "version=3\ngci=" + std::string(digest) + "\n";
    auto put = [&](const std::string& k, const Name& nm) {
        if (nm.full.empty()) return;
        t += k + ".full=" + to_utf8(nm.full) + "\n" + k + ".gc=" + to_utf8(nm.gc) + "\n";
        if (!nm.species.empty()) t += k + ".species=" + to_utf8(nm.species) + "\n";
    };
    put("hero", s_hero);
    put("partner", s_partner);
    for (int b = 0; b < 3; b++) put("box" + std::to_string(b + 1), s_box[b]);
    for (const auto& kv : s_nick) {
        char k[40];
        SDL_snprintf(k, sizeof k, "nick.%016llx", (unsigned long long)kv.first);
        put(k, kv.second);
    }
    const std::string tmp = path + ".new"; /* written whole, then moved over, so a crash never leaves half a file */
    if (SDL_SaveFile(tmp.c_str(), t.data(), t.size()) && SDL_RenamePath(tmp.c_str(), path.c_str())) {
        std::printf("[names] extras.ini written (%zu nicknames)\n", s_nick.size());
    } else {
        std::printf("[names] extras.ini could not be written: %s\n", SDL_GetError());
    }
}

/* an extras file's lines as key -> value ("#" lines are comments); empty when there is no file */
std::map<std::string, std::string> parse_extras(const std::string& path) {
    std::map<std::string, std::string> kv;
    size_t len = 0;
    char* text = static_cast<char*>(SDL_LoadFile(path.c_str(), &len));
    if (text == nullptr) return kv;
    std::string all(text, len);
    SDL_free(text);
    size_t at = 0;
    while (at < all.size()) {
        size_t end = all.find('\n', at);
        if (end == std::string::npos) end = all.size();
        std::string line = all.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return kv;
}

void read_extras() {
    clear_all();
    std::map<std::string, std::string> kv = parse_extras(extras_path());
    if (kv.empty()) return; /* a plain .gci slot */
    char digest[41];
    if (pc_saves_save_digest(pc_saves_current(), digest) && kv["gci"] != digest) {
        /* the save changed without its extras (a crash between the two writes, or the .gci played in Dolphin): names
         * are still used only where the save holds their GameCube name (prune below); the hub says so (part 4) */
        std::printf("[names] extras.ini belongs to another version of this save (changed outside OrrePort?)\n");
    }
    auto get = [&](const std::string& k, Name& nm) {
        auto f = kv.find(k + ".full");
        auto g = kv.find(k + ".gc");
        auto sp = kv.find(k + ".species");
        if (f != kv.end() && g != kv.end())
            nm = Name{from_utf8(f->second), from_utf8(g->second), sp != kv.end() ? from_utf8(sp->second) : Units{}};
    };
    get("hero", s_hero);
    get("partner", s_partner);
    for (int b = 0; b < 3; b++) get("box" + std::to_string(b + 1), s_box[b]);
    for (const auto& e : kv) {
        if (e.first.rfind("nick.", 0) != 0 || e.first.size() < 6 + 16 || e.first.compare(e.first.size() - 5, 5, ".full") != 0) continue;
        const std::string k = e.first.substr(0, e.first.size() - 5);
        Name nm;
        get(k, nm);
        const uint64_t key = std::strtoull(k.c_str() + 5, nullptr, 16);
        s_nick[kv["version"] == "2" ? key_from_v2(key) : key] = nm; /* 1 and 3: the real numbers */
    }
    prune();
    std::printf("[names] extras.ini read (%zu nicknames)\n", s_nick.size());
}

void card_task_end(s32 kind, s32 result) {
    std::printf("[names] card task %d ended: %d\n", (int)kind, (int)result);
    /* accurate mode plays plain .gci saves with the game's own names; automated runs have no slots of their own */
    if (!pc_saves_ready() || !pc_game_ui.ready || pc_option_gamecube_accurate()) return;
    if (kind == 1) {
        if (result == 3) read_extras();
        else clear_all(); /* no save on the card: a new game starts with no long names */
    } else if (result == 4 && (kind == 4 || kind == 9 || kind == 10 || kind == 11)) {
        /* *Fixed 2026-11-15 (snatcherLeo's hand check: a new game named 元旦, saved, and the hub and the game after loading
         * showed WES):* every write kind, not only 4. A new game's first save runs as kind 9 (memcardTaskMain switches
         * kind 4 with a save count of 0 to 9: create the file, then write; memory-card.md), and 10 / 11 write too; all
         * of them end with result 4. Only kind 4 wrote the names, so the full names of a brand-new save were never
         * kept. Platforms: all. */
        write_extras();
    }
}

} // namespace

/* the name screen stored a name (OrreHook_NameEntryCommit through the keyboard): keep the full name beside it, or
 * forget an old long name when the new one fits the GameCube as it is */
extern "C" void pc_names_commit(s32 mode, s32 index, void* record, const uint16_t* full, const uint16_t* stored) {
    Name nm{take(full), take(stored), Units{}};
    if (mode == 2 && record != nullptr && pc_game_ui.species_name != nullptr) nm.species = take(pc_game_ui.species_name(record));
    const bool longer = nm.full != nm.gc;
    if (!longer) nm = Name{};
    switch (mode) {
    case 0: s_hero = nm; break;
    case 1: s_partner = nm; break;
    case 2:
        if (record != nullptr) {
            const uint64_t k = key_of(static_cast<const uint8_t*>(record));
            if (longer) s_nick[k] = nm;
            else s_nick.erase(k);
        }
        break;
    case 3:
        if (index >= 0 && index < 3) s_box[index] = nm;
        break;
    default: break;
    }
    if (longer) std::printf("[names] name kept beside its GameCube name (mode %d)\n", (int)mode);
}

/* ---- long names in fixed menu spots (decided with snatcherLeo, 2026-11-14, mockups 102-102d) ----
 * Three prints hold a person's name in a place of fixed width. When the name (as drawn, full name included) is wider
 * than its place, the print's own scale fits it; a name that fits draws exactly as the game draws it, so names of the
 * console's lengths never change. The trigger is the measured width, never a letter count.
 *   - 0x1793, the Continue screen's Name (item 0x2CE at x 393): resized (smaller both ways, centred on its line),
 *     its place ending where "Snagged POKéMON" ends (the game's own 7-W maximum), and moved about 11 px left once
 *     resized (mockup 102d, snatcherLeo: "This is perfect").
 *   - 0x36DA, the P★DA card's name (item 0x758 at x 494): resized, its place ending at the card's text edge (the
 *     money rows' end).
 *   - 0x36D2, the P★DA menu's "<name>'s e-mail" row (item 0x439): narrowed so the row and its arrow stop before the
 *     card's frame. The print's scale applies to the whole row, so "'s e-mail" narrows with the name (likely close
 *     enough to mockup 102's name-only narrowing; to show snatcherLeo).
 * Widths are in the game's 640 x 480 screen, measured on snatcherLeo's screenshots 244-245 (likely; his hand check confirms). */
struct Fit {
    u32 key;
    float place;   /* the widest the print may be */
    bool resize;   /* both ways (true) or only narrower (false) */
    float dx;      /* how far it moves once fitted */
    bool baseline; /* resized: its bottom on the line's (true) or centred on the line (false) */
    bool centre = false; /* not scaled: a print much wider than place (over 1.5x) moves left by half the extra, so it
                            sits centred on where a print of place's width would (the Options values, below) */
    u32 at = 0;          /* a place (OrreHook_PrintPlace) instead of the key: for generic prints used on many screens */
    u32 label = 0;       /* with `at`: place is less this message's width (the label printed just before it) */
    /* *Added 2026-11-21 (snatcherLeo: "Yes, keep it on"):* a print that exists only in OrrePort (its own Options Language row,
     * pc_options_lang.cpp): fitted in GameCube-accurate mode too. Every other fit stays off there (snatcherLeo's rule: "in
     * Gamecube-accurate mode, just default to the default game's thing"), because those fit the game's own prints; a
     * row a GameCube doesn't have can't make the game look different from a console's */
    bool orreOnly = false;
    /* *Added 2026-11-21 (snatcherLeo, on the French Options screen in an accurate save: "this is not a fix ... we need to look
     * at how all language UI stuff is organized in a non gamecube-accurate save ... that's where we perfected it all"):*
     * a print of the title's Options screen (Rumble's ON / OFF): fitted in GameCube-accurate mode too, but only while
     * that screen is open (pc_names_options_screen, from pc_options_lang.cpp), so the Options screen looks the same in
     * both modes in every language, and the same keys anywhere else stay as the game draws them. Look only: a fit moves
     * or scales a print, it draws no random number and changes no state */
    bool optionsScreen = false;
};
bool s_optionsOpen = false; /* the title's Options screen is open (pc_names_options_screen) */
/* whether a fit applies now: not in GameCube-accurate mode, except an OrrePort-only print's and, while the Options screen
 * is open, that screen's own prints' */
bool fit_active(const Fit& f) {
    return f.orreOnly || (f.optionsScreen && s_optionsOpen) || !pc_option_gamecube_accurate();
}
/* the Continue screen's name, after snatcherLeo's checks (2026-11-14): first -11 and centred (its end touched the disc's
 * edge), then -17 (it touched "Name" and sat high); now 136 px wide, -13, on the label's baseline */
/* *Added 2026-11-15 (snatcherLeo's Japanese hand check: "Behemothandd はどうする？" ran out of the battle move menu's tab;
 * "we can establish a narrowing rule, to ensure that, those longer names look fine in Japanese"):* 0x196, the battle
 * menus' name line (menuFight.c's name-line drawer, the only print of 0x196: its text is just the inserted string in
 * English, "ESPEON", but "<name>はどうする？" on the Japanese disc). Narrowed only, its place the tab's inside from
 * where the text starts (x 64 to 282 at 640 x 480, measured on a frame of the reference playthrough's P01 battle) less 4 px. Like every
 * fit, it acts on the measured width only, so any language's line that fits draws as the game draws it. */
std::vector<Fit> s_fits = {
    {0x1793, 136.f, true, -13.f, true},
    {0x36DA, 134.f, true, 0.f, false},
    {0x36D2, 236.f, false, 0.f, false},
    {0x196, 214.f, false, 0.f, false},
    /* *Added 2026-11-15 (snatcherLeo's hand check of the European Options values: French "MARCHE" ran into the box's edge and
     * "ARRET" past the others; "just adjust both options to the left more"; STEREO / MONO fine in all):* Rumble's
     * values, ON (0x1902) and OFF (0x1903), centred on where English's sit (their English widths, 31 and 42 px,
     * measured in the game). Only a much wider word moves (French); AN, AUS, SÌ, SÍ, NO stay where English's are. */
    /* *2026-11-21:* also in GameCube-accurate mode while the Options screen is open (Fit::optionsScreen says why) */
    {0x1902, 31.f, false, 0.f, false, true, 0, 0, false, true},
    {0x1903, 42.f, false, 0.f, false, true, 0, 0, false, true},
    /* *Added 2026-11-15 (snatcherLeo: on the summary page "OT/WWWWWWWWWWWW" ran over "ID No." and its number; "we just narrow
     * it there"):* the OT name (place 0x155, a generic 0xCF print; gbaCommunication.c sets the place). It starts right
     * after the "OT/" label (0x2BC1) and must end 6 px before "ID No.": the labels sit at x 253 and 440 on the page
     * (the replay's fit log, 2026-11-15), so its room is 440 - 253 - 6 = 181 px less the label's width in this
     * language. Narrowed only, by measured width: names that fit draw as the game draws them */
    {0xCF, 181.f, false, 0.f, false, false, 0x155, 0x2BC1},
};
bool s_inFit = false;

extern "C" u32 OrreHook_PrintPlace; /* pc_cheats.c: set by game code around a generic print */

const Fit* fit_of(u32 key) {
    if (OrreHook_PrintPlace != 0) {
        for (const Fit& f : s_fits)
            if (f.at == OrreHook_PrintPlace && f.key == key) return &f;
        return nullptr;
    }
    for (const Fit& f : s_fits)
        if (f.at == 0 && f.key == key) return &f;
    return nullptr;
}
/* the scale a print needs (1 = as the game draws it), from the game's own measure of it (full names included) */
float fit_scale(const Fit& f, u32 key) {
    if (!fit_active(f) || pc_game_ui.msg_rect == nullptr) return 1.f;
    s_inFit = true;
    const float natural = (float)(int16_t)((uint32_t)pc_game_ui.msg_rect(key) >> 16);
    const float label = f.label != 0 ? (float)(int16_t)((uint32_t)pc_game_ui.msg_rect(f.label) >> 16) : 0.f;
    s_inFit = false;
    const float place = f.place - label;
    return natural > place && natural > 0.f && place > 0.f ? place / natural : 1.f;
}

/* ORRE_FIT_LOG=1 (a test switch, never a setting; 2026-11-15): every print's key and measured width, once per
 * key, to find the print of a fixed place that needs a fit entry. Observes only (the measure is the game's own). */
/* ORRE_FIT_LOG=2: also where each print starts (x, y in the 640 x 480 screen), once per key and place, to tell apart a
 * generic print (an inserted name or number, used on many screens) by where it is drawn */
void fit_log(u32 key, const u8* task) {
    static int on = -1;
    if (on < 0) {
        const char* v = SDL_getenv("ORRE_FIT_LOG");
        on = v == nullptr ? 0 : (v[0] == '2' ? 2 : 1);
    }
    if (!on || pc_game_ui.msg_rect == nullptr) return;
    float x = 0.f, y = 0.f;
    std::memcpy(&x, task + 0x04, 4);
    std::memcpy(&y, task + 0x08, 4);
    static std::unordered_map<uint64_t, bool> seen;
    const uint64_t id = on == 2 ? ((uint64_t)key << 32 | (uint64_t)((int)x & 0xFFFF) << 16 | (uint64_t)((int)y & 0xFFFF))
                                : (uint64_t)key;
    if (seen[id]) return;
    seen[id] = true;
    s_inFit = true;
    const int w = (int16_t)((uint32_t)pc_game_ui.msg_rect(key) >> 16);
    s_inFit = false;
    if (on == 2) {
        std::printf("[fit] print key 0x%X width %d at %.0f,%.0f%s", (unsigned)key, w, x, y, fit_of(key) ? " (fitted)" : "");
#ifdef _WIN32
        /* who asked for it (Windows only: a test aid; decode with addr2line on the exe): the game function a few calls
         * up, so a generic print can be told apart by its caller */
        void* frames[6] = {};
        const unsigned short n = RtlCaptureStackBackTrace(0, 6, frames, nullptr);
        for (unsigned short i = 2; i < n; ++i) std::printf(" %p", frames[i]);
#endif
        std::printf("\n");
    } else
        std::printf("[fit] print key 0x%X width %d%s\n", (unsigned)key, w, fit_of(key) ? " (fitted)" : "");
}

void message_fit(u32 key, u8* task) {
    fit_log(key, task);
    const Fit* f = fit_of(key);
    if (f == nullptr) return;
    if (f->centre) {
        if (!fit_active(*f) || pc_game_ui.msg_rect == nullptr) return;
        s_inFit = true;
        const float natural = (float)(int16_t)((uint32_t)pc_game_ui.msg_rect(key) >> 16);
        s_inFit = false;
        if (natural <= f->place * 1.5f) return;
        float x;
        std::memcpy(&x, task + 0x04, 4);
        x -= (natural - f->place) / 2.f;
        std::memcpy(task + 0x04, &x, 4);
        return;
    }
    const float s = fit_scale(*f, key);
    if (s >= 1.f) return;
    float v;
    std::memcpy(&v, task + 0x60, 4);
    v *= s;
    std::memcpy(task + 0x60, &v, 4);
    float x;
    std::memcpy(&x, task + 0x04, 4);
    x += f->dx;
    std::memcpy(task + 0x04, &x, 4);
    if (f->resize) {
        std::memcpy(&v, task + 0x64, 4);
        v *= s;
        std::memcpy(task + 0x64, &v, 4);
        float y;
        std::memcpy(&y, task + 0x08, 4);
        /* on the line's baseline: all the height it lost; centred: half of it */
        y += (float)task[0x23] * (1.f - s) / (f->baseline ? 1.f : 2.f);
        std::memcpy(task + 0x08, &y, 4);
    }
}

s32 message_fit_rect(u32 key, s32 rect) {
    if (s_inFit) return rect;
    const Fit* f = fit_of(key);
    if (f == nullptr || f->centre) return rect; /* moved, not resized: its size stays the game's */
    const float s = fit_scale(*f, key);
    if (s >= 1.f) return rect;
    const int w = (int)((float)(int16_t)((uint32_t)rect >> 16) * s);
    const int h = f->resize ? (int)((float)(int16_t)(rect & 0xFFFF) * s) : (int16_t)(rect & 0xFFFF);
    return (s32)(((uint32_t)w << 16) | ((uint32_t)h & 0xFFFF));
}

/* a menu record got a copy of a Pokémon's nickname (OrreHook_MenuNameCopy): remember whose it is. Menu records are
 * reused in place, so the map stays small (one entry per copy address) */
static void menu_name_copy(void* copy, const void* record) {
    if (copy == nullptr || record == nullptr) return;
    if (s_nick.empty()) {
        s_copy.erase(copy);
        return;
    }
    s_copy[copy] = key_of(static_cast<const uint8_t*>(record));
}

/* a slot's long names, read from its extras file (the hub and the GameCube copy's names rule; the game needn't run):
 * id ("hero", "partner", "box1"-"box3", "nick.<key>"), the species for a nickname (UTF-8, "" otherwise), the full
 * name and the GameCube name (UTF-8). The count */
extern "C" int pc_names_read_slot(int slot, PcLongName* out, int max) {
    char dir[1200];
    pc_saves_slot_dir(slot, dir, sizeof(dir));
    const std::map<std::string, std::string> kv = parse_extras(std::string(dir) + "extras.ini");
    int n = 0;
    for (const auto& e : kv) {
        const std::string& k = e.first;
        if (n >= max || k.size() < 6 || k.compare(k.size() - 5, 5, ".full") != 0) continue;
        const std::string id = k.substr(0, k.size() - 5);
        auto g = kv.find(id + ".gc");
        if (g == kv.end()) continue;
        auto sp = kv.find(id + ".species");
        PcLongName& o = out[n++];
        SDL_snprintf(o.id, sizeof o.id, "%s", id.c_str());
        SDL_snprintf(o.species, sizeof o.species, "%s", sp != kv.end() ? sp->second.c_str() : "");
        SDL_snprintf(o.full, sizeof o.full, "%s", e.second.c_str());
        SDL_snprintf(o.gc, sizeof o.gc, "%s", g->second.c_str());
    }
    return n;
}

/* another print of fixed width that should fit its place the same way (2026-11-15: the Options screen's language
 * names, pc_options_lang.cpp); call before the print is first drawn */
extern "C" void pc_names_add_fit(uint32_t key, float place, int resize, float dx, int baseline) {
    if (fit_of(key) == nullptr) s_fits.push_back(Fit{key, place, resize != 0, dx, baseline != 0});
}
/* the title's Options screen opened (1) or left (0): pc_options_lang.cpp, from OrreHook_OptionsScreen (2026-11-21;
 * Fit::optionsScreen says why) */
extern "C" void pc_names_options_screen(int open) { s_optionsOpen = open != 0; }

/* the same for a print that exists only in OrrePort (2026-11-21: its own Options Language row), fitted in
 * GameCube-accurate mode too (Fit::orreOnly says why) */
extern "C" void pc_names_add_fit_own(uint32_t key, float place, int resize, float dx, int baseline) {
    if (fit_of(key) != nullptr) return;
    Fit f{key, place, resize != 0, dx, baseline != 0};
    f.orreOnly = true;
    s_fits.push_back(f);
}

/* called once at start-up (pc_platform.c), after the re-wrap module (whose test switch this chains) */
extern "C" void pc_names_start(void) {
    s_prevInsert = OrreHook_MessageInsert;
    OrreHook_MessageInsert = insert;
    OrreHook_CardTaskEnd = card_task_end;
    OrreHook_MenuNameCopy = menu_name_copy;
    OrreHook_MessageFit = message_fit;
    OrreHook_MessageFitRect = message_fit_rect;
}
