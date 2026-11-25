/*
 * pc_bag_tweaks.cpp: step 3.7's item tweaks (the project's build helper, 2026-11-20; decided in
 * docs/port/tweaks-37-research.md, sections 1-1i and "snatcherLeo's answers (2026-11-20)"; how the bag and the party list work:
 * docs/engine/bag-and-pc.md; the hooks: include/pc/orre_hooks.h, "Step 3.7's item tweaks").
 *
 * What it does, piece by piece (each piece is its own pc_options.c entry, so the switches can be regrouped later; the
 * grouping is still open with snatcherLeo, research section 7a):
 *   1. Keep using an item ("keep_using_item"): after an item is used from the bag on a party Pokémon, the party list
 *      comes straight back with the same item still chosen and the cursor on that Pokémon, while copies are left
 *      (Rare Candy, stones, medicine, vitamins, PP items: every use function that picks a Pokémon). "No effect" and
 *      refusals stay on the list too; B goes back to the bag; using the last copy goes back to the bag, as the game does.
 *      Giving an item to hold from the bag joins the loop the same way (snatcherLeo, 2026-11-20).
 *      *Changed 2026-11-20 after snatcherLeo's hand check (research 1k):* the list no longer closes and opens again between two
 *      uses ("it just remains on the party menu"): OrreHook_PartyListHold skips the use function's close while the bag's
 *      loop runs, and the next use only resumes the list (an evolution scene still closes it first, and the list comes
 *      back on the evolved Pokémon). The party list also starts on the last Pokémon chosen for any bag item (Use, Give,
 *      TMs) until the game closes. TMs: a taught TM goes back to the bag; a TM not learned (a refusal, or declining)
 *      stays on the list without asking again; every new use from the bag asks as the game does.
 *   2. LEARNED ("tm_party_list", now "TMs: the LEARNED label"): a Pokémon that already knows the move reads LEARNED
 *      instead of the game's ABLE. *Changed 2026-11-20:* the TM loop and its "Booted up a TM" skip after a teach were
 *      removed (snatcherLeo's hand check); what stays of the TM design is this label and infinite TMs.
 *   3. Infinite TMs ("infinite_tms"): a taught TM reports "nothing used", the path an HM takes; the bag count stays.
 *      Turning it off only makes TMs used up again (piece 2 stays as it is).
 *   4. Scents ("scent_memory"): snatcherLeo's remembered-cursor design (research 1f). The Cologne Case's list starts on the scent
 *      used last (if still owned), the party list on the Pokémon it went to, and the scents are listed strongest first
 *      (Vivid, Excite, Joy: by price, highest first, so a modded scent sorts by its own price). The scent itself plays
 *      exactly as on the GameCube (the menus close, the effect runs over the field).
 *   5. The USE rule (built in, no switch; research 1i and Appendix A): from the item's data, USE is dropped where it can
 *      never do anything and GIVE or TOSS stays (the held items, valuables, X items in the field, repels, shards,
 *      scarves, the confusion / pinch / Pokéblock berries); the Ein Files read READ; the items OrrePort's item data
 *      marks "information only" (the Small Tablet, Data ROM, Steel Teeth, Powerup Part: ui/game/item_menu.ini) read CHECK,
 *      which shows the item's description again; TMs and HMs get TOSS (USE, GIVE, TOSS, CANCEL) through the game's own
 *      toss flow ("Toss out how many?", "Is it okay to toss out ...?").
 *   6. The stone label (0.1.2, built in, no switch; 2026-11-24): with an evolution stone (any item whose bag use is the
 *      stones'), each party row shows the game's ABLE / NOT ABLE in the TM label's place; ABLE only when the stone would
 *      evolve that Pokémon (not a Shadow Pokémon, no Everstone, its own evolution data names the item). Below.
 *
 * GameCube-accurate mode: all six off (pieces 1-4 through pc_option, which is 0 there; pieces 5-6 ask the mode itself), so
 * accurate saves get the game's own menus and presses. Automated runs (recordings driving the pad) get the entries'
 * automated value 0 and piece 5 off, so recorded presses keep their meaning; test switches turn each on: ORRE_KEEP_USING_ITEM,
 * ORRE_TM_PARTY_LIST, ORRE_INFINITE_TMS, ORRE_SCENT_MEMORY (=1) and ORRE_BAG_OPTIONS=1.
 *
 * Random numbers: none drawn here, and none by the code the hooks touch (the bag, the party list, the item use functions:
 * verified by search, research section 1). The tweaks only change which buttons lead where, so later rolls differ only
 * through the frames that pass, as on a console.
 *
 * Saves: nothing stored outside the game's own save. Infinite TMs keeps a TM count that simply doesn't drop, which is a
 * normal GameCube save (pc_gcopy.c's checklist says so). The remembered scent and slot are in memory only, for the run.
 *
 * Platforms: plain C++ over the game's own menu and item code (no OS calls); Windows, Linux and macOS the same (likely:
 * built and run on Windows only so far).
 * For XD: XD's bag and party list are the same engine family (its menuUseItem.c has the same use functions, research
 * section 1g); the hook places would likely be the same functions at other addresses; the item ids, pockets and message
 * keys differ (guess until XD is checked). docs/xd-insights.md has the entry.
 */
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_options.h"
#include "pc_text.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
void pc_game_text_set_own_wait(u32 key, const char* utf8, u32 fontRef);
int pc_platform_automated_run(void);
const char* pc_game_text_language(void);
}
#include "pc_game_ui.h"
#include "pc_paths.h" /* the UI folder (4.1) */

namespace {

/* the game's message keys this file swaps or borrows the font of (extracted pocket_menu text; research 1d) */
constexpr u32 kKeyAble = 0x2B64;      /* ABLE on the party list with a TM */
constexpr u32 kKeyNotAble = 0x2B65;   /* NOT ABLE on the party list with a TM (the stone label borrows both) */
/* OrrePort's own message keys (0xFFF000nn: OrrePort's range; 0x30-0x34 are PC MENU's and the save notice's) */
constexpr u32 kKeyRead = 0xFFF00040;    /* READ, the Ein Files' USE */
constexpr u32 kKeyCheck = 0xFFF00041;   /* CHECK, the information-only key items' USE */
constexpr u32 kKeyLearned = 0xFFF00042; /* LEARNED on the party list */
constexpr u32 kKeyCheckText = 0xFFF00043; /* the description CHECK shows, in a message window */
constexpr u8 kKindTm = 4;    /* item kind of TMs and HMs (the party list's ABLE label code tests the same, menuPokemonDrawItem) */
constexpr u8 kKindScent = 6; /* item kind of the scents (the Cologne Case's list collects kind 6, fn_80019754) */
constexpr s32 kModeItemUse = 3, kModeGive = 5; /* menuPokemonSub modes: item use in the field, give to hold */

/* A TM staying on the party list after it was not learned: the TM, armed by item_use_again and taken by the next
 * itemFieldUseTM (tm_skip_ask), which then goes straight back to the list without its boot-up text and question */
u16 s_tmStay = 0;

/* The party list held open between uses (snatcherLeo's hand check, 2026-11-20; research 1k; OrreHook_PartyListHold):
 * s_inAction from the bag's ORRE_HOLD_BEGIN to its ORRE_HOLD_END (a field Use or Give from the bag is running), s_held
 * while a use function's close was skipped (the list is still on screen). */
bool s_inAction = false;
bool s_held = false;

/* The last Pokémon chosen for a bag item (snatcherLeo, 2026-11-20: "the cursor should still remain on last Pokemon I used a
 * consumable on"): one shared memory for every bag item that opens the party list (Use, Give, TMs), set at each pick,
 * kept until the game closes (like the scents' memory, which stays its own). Used only while "keep_using_item" is on. */
s32 s_lastSlot = -1;

/* The scents (piece 4): the last scent used and the slot it went to, this run only */
u16 s_lastScent = 0;
s32 s_lastScentSlot = -1;

/* The information-only items (piece 5's CHECK), from ui/game/item_menu.ini */
std::vector<u16> s_checkItems;
std::vector<u16> s_readItems; /* extra READ items besides the reader function's (modded items) */
std::vector<u16> s_tossItems; /* extra tossable items besides the TM kind's */
bool s_iniRead = false;

std::string s_madeLang; /* the language the own labels were last made in */

bool on(const char* id) { return pc_game_ui.ready && pc_option(id) != 0; }

/* piece 5 is built in: off in GameCube-accurate mode and in automated runs (whose recorded presses were made on the
 * game's own menus); ORRE_BAG_OPTIONS=1 turns it on in a test run */
bool bag_options_on() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_BAG_OPTIONS");
    return force != nullptr && force[0] == '1';
}

bool contains(const std::vector<u16>& v, u16 item) { return std::find(v.begin(), v.end(), item) != v.end(); }

void read_ini() {
    s_iniRead = true;
    const std::string path = pc_paths_str(PC_PLACE_UI, "game/item_menu.ini"); /* app\UI\ (flat layout: ui\), 4.1 */
    size_t len = 0;
    char* text = static_cast<char*>(SDL_LoadFile(path.c_str(), &len));
    if (text == nullptr) {
        std::printf("[bag_tweaks] %s can't be read (%s): no CHECK items; READ and TOSS from the item data only\n",
                    path.c_str(), SDL_GetError());
        return;
    }
    const std::string all(text, len);
    SDL_free(text);
    std::string section;
    size_t at = 0;
    while (at < all.size()) {
        size_t end = all.find('\n', at);
        if (end == std::string::npos) end = all.size();
        std::string line = all.substr(at, end - at);
        at = end + 1;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t lead = 0;
        while (lead < line.size() && (line[lead] == ' ' || line[lead] == '\t')) ++lead;
        line.erase(0, lead);
        if (line.empty()) continue;
        if (line.front() == '[') {
            section = line.substr(1, line.find(']') == std::string::npos ? std::string::npos : line.find(']') - 1);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        if (key != "items") continue;
        std::vector<u16>* into = section == "check" ? &s_checkItems : section == "read" ? &s_readItems
                                 : section == "toss" ? &s_tossItems : nullptr;
        if (into == nullptr) continue;
        const char* p = line.c_str() + eq + 1;
        while (*p != '\0') {
            char* stop = nullptr;
            const long v = std::strtol(p, &stop, 0);
            if (stop == p) {
                ++p;
                continue;
            }
            if (v > 0 && v < 0x10000) into->push_back((u16)v);
            p = stop;
        }
    }
    std::printf("[bag_tweaks] item_menu.ini read: %d CHECK, %d extra READ, %d extra TOSS item(s)\n", (int)s_checkItems.size(),
                (int)s_readItems.size(), (int)s_tossItems.size());
}

/* OrrePort's three labels in the game's language (made again when the language changes). Fonts: READ and CHECK borrow
 * the font of the option they replace (USE's label key, passed in), LEARNED the font of the game's ABLE. */
void make_labels(u32 useKey) {
    const char* lang = pc_game_text_language();
    const std::string want = std::string(lang != nullptr ? lang : "") + "/" + std::to_string(useKey);
    if (want == s_madeLang) return;
    s_madeLang = want;
    pc_game_text_set_own(kKeyRead, pc_text_lang(lang, "game.bag.read"), useKey != 0 ? useKey : 0xF0000001u);
    pc_game_text_set_own(kKeyCheck, pc_text_lang(lang, "game.bag.check"), useKey != 0 ? useKey : 0xF0000001u);
    pc_game_text_set_own(kKeyLearned, pc_text_lang(lang, "game.bag.learned"), kKeyAble);
}

/* ---- pieces 1-2: the loop ---- */

/* OrreHook_ItemUseAgain: after a Use or a Give from the field bag. kind 1 Use, 2 Give; result 0 done, 3 nothing used.
 * *Changed 2026-11-20 (snatcherLeo's hand check, research 1k):* TMs follow the game's own flow plus two things: a taught TM
 * goes back to the bag (snatcherLeo: "Teach succeeds -> back to the bag / items menu"), and a TM not learned (the game's
 * refusals, or the player declining at "forget a move?") stays on the party list. Both are part of "keep_using_item";
 * the old "tm_party_list" switch now only gives the LEARNED label. */
s32 item_use_again(s32 kind, u16 item, s32 page, s32 slot, s32 result, u32* row) {
    s_tmStay = 0;
    if (slot < 0) return 0; /* B on the party list, or an item that never opened it: back to the bag, as the game does */
    s_lastSlot = slot;
    if (!on("keep_using_item")) return 0;
    const bool tm = kind == 1 && pc_game_ui.item_kind(item) == kKindTm;
    if (tm && result == 0) {
        std::printf("[bag_tweaks] TM item %u taught on slot %d: back to the bag\n", (unsigned)item, (int)slot);
        return 0;
    }
    uint32_t total = 0;
    const int32_t at = pc_game_ui.bag_row_of(page, item, &total);
    if (at < 0 || total == 0) {
        std::printf("[bag_tweaks] %s item %u on slot %d: none left, back to the bag\n", kind == 2 ? "gave" : "used",
                    (unsigned)item, (int)slot);
        return 0; /* the last copy: the bag, as the game does */
    }
    *row = (u32)at;
    if (tm) s_tmStay = item;
    std::printf("[bag_tweaks] %s item %u on slot %d: %u left (row %d), the party list again%s\n",
                kind == 2 ? "gave" : "used", (unsigned)item, (int)slot, (unsigned)total, (int)at,
                s_held ? " (kept open)" : "");
    return 1;
}

/* OrreHook_PartyListHold: keep the party list open between two uses (research 1k). Only while the bag's own loop runs
 * (s_inAction) and only after a Pokémon was picked: B closes the list as the game does, and an evolution or the end of
 * the loop closes a held list where the game's own close would have been. */
s32 party_list_hold(s32 step, s32 slot) {
    switch (step) {
    case ORRE_HOLD_BEGIN:
        s_inAction = true;
        s_held = false;
        return 0;
    case ORRE_HOLD_PICKED:
        if (s_inAction && slot >= 0 && slot < 6) s_lastSlot = slot;
        return 0;
    case ORRE_HOLD_CLOSE:
        if (!s_inAction || slot < 0 || slot >= 6) return 0;
        s_lastSlot = slot;
        if (!on("keep_using_item")) return 0;
        s_held = true;
        return 1;
    case ORRE_HOLD_LEAVE:
        if (!s_held) return 0;
        s_held = false;
        std::printf("[bag_tweaks] the party list closes for the evolution scene\n");
        return 1;
    case ORRE_HOLD_END: {
        const bool held = s_held;
        s_inAction = false;
        s_held = false;
        return held ? 1 : 0;
    }
    default:
        return 0;
    }
}

/* OrreHook_PartyCursorStart */
s32 party_cursor_start(s32 mode, u16 item) {
    if (s_inAction && (mode == kModeItemUse || mode == kModeGive) && s_lastSlot >= 0 && on("keep_using_item")) {
        return s_lastSlot; /* the last Pokémon chosen for a bag item (menuPokemonSub ignores an empty slot) */
    }
    if (mode == kModeItemUse && s_lastScentSlot >= 0 && pc_game_ui.item_kind(item) == kKindScent && on("scent_memory")) {
        std::printf("[bag_tweaks] scent %u: the party list starts on slot %d (the last scent's)\n", (unsigned)item,
                    (int)s_lastScentSlot);
        return s_lastScentSlot;
    }
    return -1;
}

/* OrreHook_TmSkipAsk: staying on the list after a TM was not learned skips the boot-up text and its question (the same
 * use going on); every new use from the bag asks, as the game does */
s32 tm_skip_ask(u16 item) {
    const bool stay = s_tmStay != 0 && item == s_tmStay && on("keep_using_item");
    s_tmStay = 0;
    return stay ? 1 : 0;
}

/* OrreHook_TmUsedCount: piece 3 */
u32 tm_used_count(u16 item, u32 count) {
    if (!on("infinite_tms")) return count;
    std::printf("[bag_tweaks] infinite TMs: TM item %u taught, none used\n", (unsigned)item);
    return 0;
}

/* OrreHook_TmLabel: LEARNED where the game says ABLE and the Pokémon already knows the move */
u32 tm_label(u32 key, void* pokemon, u16 item) {
    if (key != kKeyAble || !on("tm_party_list") || pc_game_ui.pokemon_knows_move(pokemon, item) == 0) return key;
    make_labels(0);
    return kKeyLearned;
}

/* ---- piece 6: the stone label (0.1.2, 2026-11-24) ----
 * snatcherLeo (2026-11-24): "If you tried using a water stone on Growlithe, it will say "NOT USABLE", because, from the game
 * database/evolutionary database, "Fire Stone" is the item that evolves Growlithe. USABLE only shows up for that
 * specific Pokemon if it can use that to evolve"; then "ABLE / NOT ABLE then, matching the TMs"; built in, no switch.
 * The party list in the field's item-use mode shows the game's own ABLE / NOT ABLE (the TMs' words, translated on each
 * region's disc) for every item whose bag use is the evolution stones' (menuUseItemEvolution): no item list here, so a
 * mod stone that picks that use gets the label. ABLE exactly when the stone's own use would evolve the Pokémon
 * (pc_game_ui.stone_able: Shadow first, then the evolution check, which also refuses an Everstone holder).
 * Off in GameCube-accurate mode (the game's own screen there, like LEARNED) and in automated runs unless
 * ORRE_STONE_LABEL=1 (recorded runs compare frames against the game's own screen). Look only, no random numbers. */
bool stone_label_on() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_STONE_LABEL");
    return force != nullptr && force[0] == '1';
}

/* The log line (snatcherLeo, 2026-11-24, after the hand check: "does logger show what you're looking for?"): one line per party
 * Pokémon when its label first shows or changes (the list redraws every frame), so a log, and an automated run with
 * ORRE_STONE_LABEL=1, can check the label without a screenshot. Keyed by the record's address and the item; six rows */
struct LabelSeen {
    void* pokemon;
    u16 item;
    s32 result;
};
LabelSeen s_labelSeen[6] = {};

/* OrreHook_ItemLabel */
u32 item_label(u16 item, void* pokemon, s32 mode) {
    if (mode != kModeItemUse || pokemon == nullptr || !stone_label_on()) return 0;
    if (pc_game_ui.item_field_use == nullptr || pc_game_ui.stone_able == nullptr) return 0;
    if (pc_game_ui.item_use_evolution_addr == 0 || pc_game_ui.item_field_use(item) != pc_game_ui.item_use_evolution_addr)
        return 0;
    u16 species = 0;
    const s32 able = pc_game_ui.stone_able(pokemon, item, &species);
    LabelSeen* free_row = nullptr;
    bool seen = false;
    for (LabelSeen& s : s_labelSeen) {
        if (s.pokemon == pokemon) {
            seen = s.item == item && s.result == able;
            free_row = &s;
            break;
        }
        if (s.pokemon == nullptr && free_row == nullptr) free_row = &s;
    }
    if (!seen) {
        if (free_row == nullptr) free_row = &s_labelSeen[0];
        *free_row = LabelSeen{pokemon, item, able};
        std::printf("[bag_tweaks] stone label: item %u on species %u: %s\n", (unsigned)item, (unsigned)species,
                    able > 0 ? "ABLE" : able < 0 ? "NOT ABLE (Shadow)" : "NOT ABLE");
    }
    return able > 0 ? kKeyAble : kKeyNotAble;
}

/* ---- piece 4: the scents ---- */

/* OrreHook_ScentListOpen: strongest first (highest price first; the game lists them by id: Joy, Excite, Vivid), and the
 * cursor on the scent used last while it is still owned */
s32 scent_list_open(u16* ids, s32 count) {
    if (!on("scent_memory") || count <= 0) return -1;
    std::stable_sort(ids, ids + count, [](u16 a, u16 b) {
        const uint32_t pa = pc_game_ui.item_price(a), pb = pc_game_ui.item_price(b);
        return pa != pb ? pa > pb : a > b;
    });
    for (s32 i = 0; i < count; ++i) {
        if (ids[i] == s_lastScent && s_lastScent != 0) {
            std::printf("[bag_tweaks] the scent list starts on scent %u (row %d)\n", (unsigned)s_lastScent, (int)i);
            return i;
        }
    }
    return -1;
}

/* OrreHook_ScentUsed: remembered always (cheap), used only while the switch is on */
void scent_used(u16 item, s32 slot) {
    s_lastScent = item;
    s_lastScentSlot = slot;
    std::printf("[bag_tweaks] scent %u used on slot %d\n", (unsigned)item, (int)slot);
}

/* ---- piece 5: the USE rule ---- */

void put_utf8(std::string& out, uint32_t c) {
    if (c < 0x80) {
        out += (char)c;
    } else if (c < 0x800) {
        out += (char)(0xC0 | (c >> 6));
        out += (char)(0x80 | (c & 0x3F));
    } else {
        out += (char)(0xE0 | (c >> 12));
        out += (char)(0x80 | ((c >> 6) & 0x3F));
        out += (char)(0x80 | (c & 0x3F));
    }
}

/* the game's text of an item description as UTF-8 for the bag's message window. The disc's descriptions are written
 * for the bag's narrow help box (three short lines); the message window is wide but shows two lines (verified on
 * screen 2026-11-20: the text joined into one line ran past the box, and re-wrapping is off in test runs). So control
 * codes are dropped, the line breaks (codes 0x00 / 0x01) become spaces, and the text is wrapped again into at most two
 * lines of kLineUnits (a Latin letter 1 unit, a full-width character 2: the longest US description, 61 letters, fits in
 * two lines). Argument bytes per code as pc_game_text.cpp's table. Platforms: all */
constexpr int kLineUnits = 40;
std::string description_text(const u8* t) {
    std::vector<uint16_t> cs;
    if (t == nullptr) return std::string();
    for (size_t i = 0; i < 4096; i += 2) {
        const uint16_t c = (uint16_t)(t[i] << 8 | t[i + 1]);
        if (c == 0) break;
        if (c == 0xFFFF) {
            const u8 code = t[i + 2];
            const int args = code == 0x08 ? 4 : (code == 0x07 || code == 0x09 || code == 0x38 || code == 0x52 ||
                                                 code == 0x53 || code == 0x54 || code == 0x5B || code == 0x5C) ? 1 : 0;
            if ((code == 0x00 || code == 0x01) && !cs.empty() && cs.back() != ' ') cs.push_back(' ');
            i += 1 + args; /* with the loop's += 2: past 0xFFFF, the code byte and its arguments (odd counts too) */
            continue;
        }
        cs.push_back(c);
    }
    while (!cs.empty() && cs.back() == ' ') cs.pop_back();
    /* greedy wrap: break at the last space (or before any full-width character) that keeps the line within the limit */
    std::string out;
    int units = 0, lines = 1;
    size_t lastBreak = std::string::npos;
    std::vector<uint16_t> line;
    auto flush = [&](size_t upto, bool dropSpace) {
        for (size_t k = 0; k < upto; ++k) put_utf8(out, line[k]);
        line.erase(line.begin(), line.begin() + (std::ptrdiff_t)(upto + (dropSpace ? 1 : 0)));
    };
    for (const uint16_t c : cs) {
        const bool wide = c >= 0x2E80;
        if (c == ' ') lastBreak = line.size();
        else if (wide) lastBreak = line.size(); /* a break may go before a full-width character */
        line.push_back(c);
        units += wide ? 2 : 1;
        if (units > kLineUnits && lines < 2 && lastBreak != std::string::npos && lastBreak > 0) {
            const bool space = line[lastBreak] == ' ';
            flush(lastBreak, space);
            out += '\n';
            ++lines;
            units = 0;
            for (const uint16_t r : line) units += r >= 0x2E80 ? 2 : 1;
            lastBreak = std::string::npos;
        }
    }
    flush(line.size(), false);
    return out;
}

}  // namespace

/* CHECK's action (a bag option callback, called by bagUseItemOnPokemon as the game's own USE would be): the item's
 * description in the bag's message window, waited for with A; nothing used, back to the bag (3, as "can't be used"
 * returns). C linkage: the game stores the address in a 32-bit option cell (the exe's image is below 4 GB). */
extern "C" s32 pc_bag_check_option(s32 page, s32 row, s32* out) {
    if (out != nullptr) *out = 0;
    const u16 item = pc_game_ui.bag_item_at(page, row);
    const u32 doc = item != 0 ? pc_game_ui.item_doc(item) : 0;
    const std::string text = doc != 0 && pc_game_ui.msg_gschar != nullptr
                                 ? description_text(static_cast<const u8*>(pc_game_ui.msg_gschar(doc))) : std::string();
    if (text.empty()) return 3;
    pc_game_text_set_own_wait(kKeyCheckText, text.c_str(), 0xF0000000u); /* font 0: the bag's message window font */
    std::printf("[bag_tweaks] CHECK item %u: its description (message 0x%X) in a message window\n", (unsigned)item,
                (unsigned)doc);
    pc_game_ui.bag_message(kKeyCheckText);
    return 3;
}

namespace {

struct Option {
    u32 label;
    u32 callback;
    u32 flags;
};
static_assert(sizeof(Option) == 0x0C, "a bag option record is 0x0C bytes (fn_80017CB8's SummaryMenuItem)");

/* OrreHook_BagOptions: the field list only (the battle lists keep the game's: removing USE there would leave CANCEL
 * alone, which snatcherLeo settled to keep as the game has it) */
s32 bag_options(void* out, s32 count, s32 max, s32 page, s32 mode, u16 item) {
    if (mode != 0 || out == nullptr || count <= 0 || !bag_options_on()) return count;
    if (!s_iniRead) read_ini();
    Option* o = static_cast<Option*>(out);
    s32 use = -1, give = -1, toss = -1;
    for (s32 i = 0; i < count; ++i) {
        if (o[i].callback == pc_game_ui.bag_use_addr && use < 0) use = i;
        if (o[i].callback == pc_game_ui.bag_give_addr && give < 0) give = i;
        if (o[i].callback == pc_game_ui.bag_toss_addr && toss < 0) toss = i;
    }
    const u32 field = pc_game_ui.item_field_use(item);
    const char* what = nullptr;
    if (use >= 0) {
        if (field == pc_game_ui.item_use_reader_addr || contains(s_readItems, item)) {
            make_labels(o[use].label);
            o[use].label = kKeyRead;
            what = "READ";
        } else if (contains(s_checkItems, item)) {
            make_labels(o[use].label);
            o[use].label = kKeyCheck;
            o[use].callback = (u32)(uintptr_t)&pc_bag_check_option;
            what = "CHECK";
        } else if ((field == 0 || field == pc_game_ui.item_use_not_now_addr || field == pc_game_ui.item_use_forbidden_addr ||
                    field == pc_game_ui.item_use_meaningless_addr) &&
                   (give >= 0 || toss >= 0)) {
            /* USE can never do anything in the field and a real option stays: drop it (research 1i, settled 2026-11-20) */
            std::memmove(&o[use], &o[use + 1], sizeof(Option) * (size_t)(count - use - 1));
            --count;
            if (give > use) --give;
            if (toss > use) --toss;
            what = "no USE";
        }
    }
    /* TOSS for TMs and HMs (and items the ini marks tossable), where the pocket has none: after GIVE (else after USE),
     * so the order is USE, GIVE, TOSS, CANCEL as in the other pockets */
    if (toss < 0 && count < max && (pc_game_ui.item_kind(item) == kKindTm || contains(s_tossItems, item))) {
        const u32 label = pc_game_ui.bag_option_label(pc_game_ui.bag_toss_addr);
        s32 at = give >= 0 ? give + 1 : 0;
        if (give < 0) {
            for (s32 i = 0; i < count; ++i)
                if (o[i].callback == pc_game_ui.bag_use_addr || o[i].callback == (u32)(uintptr_t)&pc_bag_check_option) at = i + 1;
        }
        if (label != 0) {
            std::memmove(&o[at + 1], &o[at], sizeof(Option) * (size_t)(count - at));
            o[at].label = label;
            o[at].callback = pc_game_ui.bag_toss_addr;
            o[at].flags = 0;
            ++count;
            what = what != nullptr ? what : "TOSS added";
        }
    }
    if (what != nullptr) {
        static u16 s_lastLogged = 0; /* one line per item chosen, not per redraw */
        if (s_lastLogged != item) {
            s_lastLogged = item;
            std::printf("[bag_tweaks] item %u (page %d): %s; %d option(s)\n", (unsigned)item, (int)page, what, (int)count);
        }
    }
    return count;
}

}  // namespace

/* called once at start-up (pc_platform.c): registers every hook; each one checks its switch when it is called, so a
 * Settings change takes effect at once (tweaks need no restart) */
extern "C" void pc_bag_tweaks_start(void) {
    OrreHook_ItemUseAgain = item_use_again;
    OrreHook_PartyListHold = party_list_hold;
    OrreHook_PartyCursorStart = party_cursor_start;
    OrreHook_TmSkipAsk = tm_skip_ask;
    OrreHook_TmUsedCount = tm_used_count;
    OrreHook_TmLabel = tm_label;
    OrreHook_ItemLabel = item_label;
    OrreHook_ScentListOpen = scent_list_open;
    OrreHook_ScentUsed = scent_used;
    OrreHook_BagOptions = bag_options;
}
