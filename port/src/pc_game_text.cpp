/*
 * pc_game_text.cpp: the game's own messages in the player's language (step 2.2 step 4, the language switch,
 * 2026-11-13; docs/port/translation.md "The game's own text in other languages").
 *
 * What it does: the game finds every message by a 32-bit key (bank key >> 20, entry key & 0xFFFFF) in the message
 * banks it loaded from the disc (GSmsgFindMessage, gs_msg.c). OrreHook_MessageText (orre_hooks.h) lets this file hand
 * back another text for the same key, in the disc's own encoding, so the game draws, measures, wraps and inserts it
 * exactly like its own. One setting decides both OrrePort's words and the game's: settings.ini "language" (or
 * ORRE_LANG for a test run), read by pc_text.
 *
 * Why this design:
 *   - The key is the one thing every language shares, so a language is "the text for each key" and nothing in the game
 *     code changes but one hook at the lookup's end. With English (or no file) the hook isn't registered and the game
 *     reads the disc's text, as on the GameCube.
 *   - The text keeps the disc's encoding (big-endian UTF-16 code units; 0xFFFF + a code byte + that code's argument
 *     bytes for control codes; 0x0000 at the end), so names inserted by control codes, colours, waits and line breaks
 *     work unchanged in every language.
 *   - Texts live below 4 GB (pc_lowmem) and are never freed: the game keeps text pointers in 32-bit slots and may hold
 *     one for as long as a box is open. A text is made once per key and per disc text (some banks are reloaded with
 *     the room, so the disc text is part of the cache's key), a few MB at most for the whole game.
 *
 * Built so far: the "pseudo" language (a test language, never shipped as a choice for players): every letter
 * swapped for an accented look-alike, the length unchanged (the game's boxes are fixed sizes; longer text is a
 * separate test). Some look-alikes are in the game's fonts and some are not, so the font renderer's part D (letters
 * the game lacks) shows in real dialogue. Control codes and their argument bytes are copied untouched. snatcherLeo's own
 * languages (2.2 step 8: recreated by hand, the discs for study only) will be read from files here, in the format
 * translation.md describes.
 *
 * Control codes' argument bytes (verified 2026-11-13 from the msgctrl handlers; tools/local/colo/msg.py CONTROL):
 * Font 0x07, Wait 0x09, Palette 0x38, Shadow 0x52, Align 0x53, TalkSE 0x54, LineSpace 0x5B and BaseLineBias 0x5C take 1
 * byte, Color 0x08 takes 4, every other code none. After a code with an odd count the letters start on an odd byte, so
 * the text is walked byte by byte, not as an array of u16.
 *
 * Platforms: plain C++ (the same on Windows, Linux and macOS; verified on Windows, likely elsewhere). Shared engine:
 * XD's message lookup is likely the same shape (guess until XD is checked; docs/xd-insights.md).
 */
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "pc_game_ui.h"
#include "pc_lowmem.h"
#include "pc_settings.h"
#include "pc_text.h"

extern "C" void* (*OrreHook_MessageText)(uint32_t key, const void* original);
extern "C" int pc_platform_automated_run(void); /* pc_platform.c: a recording drives the pad (replays, test runs) */
extern "C" void* (*OrreHook_MessageExtra)(uint32_t key, void** outGroup);
extern "C" int OrreHook_MessageForCopy; /* gs_msg.c GSmsgGetGSchar: the game takes a message as a string to copy */
/* pc_region_text.cpp: the European languages' text from the player's own European disc (2.2 step 8, piece 1) */
extern "C" void pc_region_text_start(int wait);
extern "C" int pc_region_text_has(const char* code);
extern "C" const uint8_t* pc_region_text_get(const char* code, uint32_t key, size_t* size);
extern "C" void pc_word_pictures_start(void); /* pc_word_pictures.cpp: the word pictures per language (piece 4) */

namespace {

/* the argument bytes after 0xFFFF + code (see above); 0 for every code not listed */
int control_args(uint8_t code) {
    switch (code) {
    case 0x08: return 4; /* Color: r, g, b, a */
    case 0x07: case 0x09: case 0x38: case 0x52: case 0x53: case 0x54: case 0x5B: case 0x5C: return 1;
    default: return 0;
    }
}

/* the disc text's length in bytes, the 0x0000 end included, and a hash of it (FNV-1a); 0 when it runs past `limit`
 * (never seen: a guard against a text without its end) */
size_t measure(const uint8_t* t, size_t limit, uint64_t* hash) {
    uint64_t h = 1469598103934665603ull;
    size_t i = 0;
    while (i + 1 < limit) {
        const uint16_t c = (uint16_t)(t[i] << 8 | t[i + 1]);
        size_t n = 2;
        if (c == 0xFFFF && i + 2 < limit) n = 3 + (size_t)control_args(t[i + 2]);
        for (size_t k = 0; k < n && i + k < limit; ++k) h = (h ^ t[i + k]) * 1099511628211ull;
        i += n;
        if (c == 0) {
            *hash = h;
            return i;
        }
    }
    return 0;
}

/* pseudo: one accented look-alike per letter, the same length. Two kinds on purpose: letters the game's fonts have
 * and letters they lack, so part D is exercised in real text. Verified 2026-11-13 (P13's battle stretch, part D's log
 * names each letter it supplied): á é ï ô ü ñ ç Â Ë Ö Û are the game's own; ÿ and the Latin Extended-A ones
 * (š ŵ ğ ř Š Ť) came from part D in fonts 0 and 1 (Ť also in font 4); ž and Ž were not seen in that stretch. */
uint16_t pseudo_letter(uint16_t c) {
    switch (c) {
    case 'a': return 0x00E1; case 'e': return 0x00E9; case 'i': return 0x00EF; case 'o': return 0x00F4;
    case 'u': return 0x00FC; case 'n': return 0x00F1; case 'c': return 0x00E7; case 'y': return 0x00FF;
    case 's': return 0x0161; case 'z': return 0x017E; case 'w': return 0x0175; case 'g': return 0x011F;
    case 'r': return 0x0159;
    case 'A': return 0x00C2; case 'E': return 0x00CB; case 'O': return 0x00D6; case 'U': return 0x00DB;
    case 'S': return 0x0160; case 'T': return 0x0164; case 'Z': return 0x017D;
    default: return c;
    }
}

void pseudo_text(const uint8_t* t, size_t size, uint8_t* out) {
    size_t i = 0;
    while (i + 1 < size) {
        const uint16_t c = (uint16_t)(t[i] << 8 | t[i + 1]);
        if (c == 0xFFFF) {
            const size_t n = 3 + (size_t)control_args(i + 2 < size ? t[i + 2] : 0);
            std::memcpy(out + i, t + i, std::min(n, size - i));
            i += n;
            continue;
        }
        const uint16_t p = pseudo_letter(c);
        out[i] = (uint8_t)(p >> 8);
        out[i + 1] = (uint8_t)p;
        i += 2;
    }
    if (i < size) out[i] = t[i];
}

/* recursive: an own message's font lookup (message_extra -> the game's lookup) comes back into message_text on the
 * same thread. A plain mutex deadlocked there the moment the language was pseudo (the first Options test froze on the
 * RIGHT press, 2026-11-13) */
std::recursive_mutex s_mutex;
/* (key, disc text's hash) -> our text below 4 GB; never freed (the game may hold any of them) */
std::unordered_map<uint64_t, std::unordered_map<uint64_t, void*>> s_made;
size_t s_bytes = 0;
/* the game text's language now: "en" (the disc's text) or "pseudo"; changed at once by the Options screen's Language
 * row (pc_options_lang.cpp), so the next message shown is in the new language */
std::string s_lang = "en";

/* OrrePort's own messages in the game's screens (bank 0xFFF, which no disc bank uses): the text in UTF-8 and a game
 * message key whose bank's font it borrows (the same font as the game's own text in that place) */
struct Own {
    std::string utf8;
    uint32_t fontRef = 0;
    /* end with the game's wait-for-A arrow (control code 0x02, msgctrlKeyEnd), as the card messages do, so the message
     * window waits for A before it is done (pc_game_text_set_own_wait; 2026-11-20, the softlock fix's notice) */
    bool waitA = false;
    std::string madeFor; /* the text and language the encoded copy was made from */
    void* encoded = nullptr;
};
std::unordered_map<uint32_t, Own> s_own;
uint8_t* s_fallbackGroup = nullptr; /* a 16-byte group header with font 0, when the borrowed bank isn't loaded */

/* UTF-8 to the game's encoding: big-endian UTF-16, '\n' as control code 0x00 (a new line, as the game's own help
 * lines have it), 0x0000 at the end; pseudo letters when the language is pseudo */
std::vector<uint8_t> encode(const std::string& utf8, bool pseudo) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i < utf8.size();) {
        uint32_t c = (unsigned char)utf8[i++];
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        while (extra-- > 0 && i < utf8.size() && ((unsigned char)utf8[i] & 0xC0) == 0x80) {
            c = (c << 6) | ((unsigned char)utf8[i++] & 0x3F);
        }
        if (c == '\n') {
            out.insert(out.end(), {0xFF, 0xFF, 0x00});
            continue;
        }
        if (c > 0xFFFF) c = '?'; /* the game's text is 16-bit: nothing above the BMP */
        const uint16_t u = pseudo ? pseudo_letter((uint16_t)c) : (uint16_t)c;
        out.push_back((uint8_t)(u >> 8));
        out.push_back((uint8_t)u);
    }
    out.push_back(0);
    out.push_back(0);
    return out;
}


void* message_extra(uint32_t key, void** outGroup) {
    if ((key >> 20) != 0xFFF) return nullptr;
    const std::lock_guard lock(s_mutex);
    const auto it = s_own.find(key);
    if (it == s_own.end()) return nullptr;
    Own& o = it->second;
    const std::string want = s_lang + (o.waitA ? "\nA\n" : "\n") + o.utf8;
    if (o.encoded == nullptr || o.madeFor != want) {
        std::vector<uint8_t> bytes = encode(o.utf8, s_lang == "pseudo");
        /* the wait-for-A arrow before the end (FF FF 02, three bytes like the line break FF FF 00) */
        if (o.waitA) bytes.insert(bytes.end() - 2, {0xFF, 0xFF, 0x02});
        auto* mem = static_cast<uint8_t*>(pc_lowmem_alloc(bytes.size()));
        if (mem == nullptr) return nullptr;
        std::memcpy(mem, bytes.data(), bytes.size());
        o.encoded = mem; /* the old copy stays: the game may still show it this frame (a few bytes per change) */
        o.madeFor = want;
    }
    if (outGroup != nullptr) {
        /* a font asked for directly (fontRef 0xF000000n = font n, 2026-11-14: the SAVE FILES list prints in font 1,
         * the Items bag's font, whose bank isn't loaded on the title screen): a group header with that font byte (+3),
         * the rest zero, as the fallback below */
        if ((o.fontRef >> 28) == 0xF) {
            static uint8_t* byFont[8] = {};
            const unsigned f = o.fontRef & 7;
            if (byFont[f] == nullptr && (byFont[f] = static_cast<uint8_t*>(pc_lowmem_alloc(16))) != nullptr) {
                std::memset(byFont[f], 0, 16);
                byFont[f][3] = (uint8_t)f;
            }
            if (byFont[f] != nullptr) {
                *outGroup = byFont[f];
                return o.encoded;
            }
        }
        /* the borrowed bank (through the game side's table, pc_game_ui.h; NULL in test programs) */
        void* group = o.fontRef != 0 && pc_game_ui.msg_group_of != nullptr ? pc_game_ui.msg_group_of(o.fontRef) : nullptr;
        if (group == nullptr) {
            if (s_fallbackGroup == nullptr) s_fallbackGroup = static_cast<uint8_t*>(pc_lowmem_alloc(16));
            group = s_fallbackGroup;
        }
        *outGroup = group;
    }
    return o.encoded;
}

/* ---- The save and card messages in OrrePort's words (snatcherLeo approved each, 2026-11-14; docs/port/save-messages.md) ----
 * Why: the game's memory-card messages speak of "the Memory Card in Slot A" and the POWER Button; on the PC a save is a
 * file in the Saves folder. The key -> text table below swaps them at the game's own lookup (this hook), in every mode
 * (wording is look-only, decided 2026-11-08 / 10), so the game still draws, times and closes each box itself.
 * How: the disc text's leading control codes (its font, 0x07) and trailing ones (the wait-for-A arrow 0x02, or none
 * for the boxes the game closes by itself) are kept around OrrePort's lines (\n = the line break, code 0x00): verified
 * 2026-11-14 from the extracted text, every one of these messages has that shape. Codes in the middle (a page break
 * 0x03, "Slot A" insertions) are dropped with the old words. The keys are the game's own message keys as its code
 * passes them (winMsgOpen(.., 0x3C22, ..)); the words are translatable (ui/lang, game.card.*). Platforms: all. */
struct CardText {
    uint32_t key;
    const char* text; /* a ui/lang key */
};
const CardText kCard[] = {
    {0x3C29, "game.card.load_reading"}, {0x177E, "game.card.load_reading"},
    {0x44E5, "game.card.load_done"},
    {0x177A, "game.card.no_save"}, {0x3C31, "game.card.no_save"}, {0x4424, "game.card.no_save_bang"},
    {0x3C36, "game.card.create_ask"}, {0x3C38, "game.card.create_ask"}, {0x3BFA, "game.card.create_ask"},
    {0x3BFB, "game.card.creating"},
    {0x3C4A, "game.card.created"}, {0x3BFD, "game.card.created"},
    {0x3C35, "game.card.newgame_replace"}, {0x44C9, "game.card.newgame_replace_battle"},
    {0x444B, "game.card.no_progress"}, {0x44DB, "game.card.no_story"}, {0x44EA, "game.card.not_loaded"},
    {0x3C2B, "game.card.will_save"}, {0x177F, "game.card.will_save"}, {0x44D9, "game.card.will_save"},
    {0x3C2C, "game.card.saving"}, {0x3C04, "game.card.saving"}, {0x1780, "game.card.saving"},
    {0x3C05, "game.card.saving"}, {0x3C39, "game.card.saving"},
    {0x3C2E, "game.card.saved"}, {0x1781, "game.card.saved"}, {0x3C07, "game.card.saved"}, {0x3C3B, "game.card.saved"},
    {0x3C02, "game.card.overwrite"}, {0x44D8, "game.card.not_saved"}, {0x1797, "game.card.save_failed"},
    {0x44EC, "game.card.save_ask"}, {0x3D82, "game.card.options_save"},
    {0x3C03, "game.card.coupons_progress"}, {0x3C37, "game.card.coupons_pokemon"},
    {0x3C0B, "game.card.fail_none"}, {0x3C3D, "game.card.fail_none"},
    {0x3C0C, "game.card.fail_mismatch"}, {0x3C3E, "game.card.fail_mismatch"},
    {0x3C0D, "game.card.fail_damaged"}, {0x3C3F, "game.card.fail_damaged"},
    {0x3C48, "game.card.suspend_saving"},
    {0x3DBC, "game.card.suspend_fail_none"}, {0x3DBD, "game.card.suspend_fail_mismatch"},
    {0x3DBE, "game.card.suspend_fail_damaged"},
    {0x3C5C, "game.card.registering"}, {0x3C60, "game.card.reg_lost"}, {0x4415, "game.card.no_suspended"},
    {0x444D, "game.card.reg_no_story"}, {0x44EB, "game.card.reg_not_loaded"}, {0x44B1, "game.card.rules_save"},
    {0x44C2, "game.card.reg_over_suspend"}, {0x44D1, "game.card.players_differ"},
    {0x4419, "game.card.coupon_prizes"},
    {0x3C59, "game.card.damaged"}, {0x3D2F, "game.card.damaged"},
    {0x4421, "game.card.read_error_restoring"}, {0x4422, "game.card.read_error_damaged"},
    {0x4423, "game.card.restored"},
};

const char* card_text_key(uint32_t key) {
    for (const CardText& c : kCard)
        if (c.key == key) return c.text;
    return nullptr;
}

/* the disc text's leading and trailing control codes around OrrePort's lines; size is the disc text's length (its
 * 0x0000 end included). Returns the new text's bytes, ended by 0x0000 */
std::vector<uint8_t> card_text(const uint8_t* t, size_t size, const char* utf8) {
    /* where the letters start and end (the first and last non-control unit before the 0x0000 end) */
    size_t firstLetter = SIZE_MAX, afterLast = 0, i = 0;
    while (i + 1 < size) {
        const uint16_t c = (uint16_t)(t[i] << 8 | t[i + 1]);
        if (c == 0) break;
        if (c == 0xFFFF) {
            i += 3 + (size_t)control_args(i + 2 < size ? t[i + 2] : 0);
            continue;
        }
        if (firstLetter == SIZE_MAX) firstLetter = i;
        i += 2;
        afterLast = i;
    }
    const size_t end = i; /* the 0x0000 end */
    if (firstLetter == SIZE_MAX) firstLetter = afterLast = end;
    std::vector<uint8_t> out(t, t + firstLetter);                /* the leading codes (the font) */
    std::vector<uint8_t> words = encode(utf8, s_lang == "pseudo"); /* \n -> the line break code */
    out.insert(out.end(), words.begin(), words.end() - 2);       /* without its 0x0000 end */
    out.insert(out.end(), t + afterLast, t + end);               /* the trailing codes (the arrow, or none) */
    out.push_back(0);
    out.push_back(0);
    return out;
}

/* the species names' message keys (built once, when another language is first in use): they stay the disc's when the
 * game takes them as plain strings to copy or compare (OrreHook_MessageForCopy), so a snag or an evolution writes the
 * US name into the save and "is this nickname its species name?" keeps working; pc_names.cpp shows them in the chosen
 * language instead (decided 2026-11-15: un-nicknamed Pokémon follow the language) */
bool is_species_key(uint32_t key) {
    static std::vector<uint32_t> keys; /* sorted */
    static bool built = false;
    if (!built && pc_game_ui.species_key != nullptr) {
        for (uint16_t id = 1; id < 0x19C; ++id)
            if (const uint32_t k = pc_game_ui.species_key(id)) keys.push_back(k);
        std::sort(keys.begin(), keys.end());
        built = true;
    }
    return std::binary_search(keys.begin(), keys.end(), key);
}
bool s_display = false; /* pc_game_text_translate: a species name to show, not to copy */

void* message_text(uint32_t key, const void* original) {
    if (original == nullptr) return nullptr;
    if (OrreHook_MessageForCopy > 0 && !s_display && s_lang != "en") {
        const std::lock_guard lock(s_mutex);
        if (is_species_key(key)) return nullptr;
    }
    /* the game prints a message letter by letter, so other words take another number of frames: whenever a recording
     * drives the pad (an automated run) the disc's text stays, so recorded presses land as they did on the console.
     * ORRE_CARD_WORDING=1 turns it on in a test run (pictures of the new words) */
    static const bool cardInTests = SDL_getenv("ORRE_CARD_WORDING") != nullptr;
    if (const char* cardKey = card_text_key(key); cardKey != nullptr && (!pc_platform_automated_run() || cardInTests)) {
        uint64_t hash = 0;
        const size_t size = measure(static_cast<const uint8_t*>(original), 64 * 1024, &hash);
        if (size == 0) return nullptr;
        /* in the game's language (its ui/lang file's line; English until that file has it) */
        const std::string words = pc_text_lang(s_lang.c_str(), cardKey);
        /* made once per key, disc text, language and words (the language can change while the game runs) */
        uint64_t h = hash ^ 0x9E3779B97F4A7C15ull;
        for (const char ch : s_lang + '\n' + words) h = (h ^ (uint8_t)ch) * 1099511628211ull;
        const std::lock_guard lock(s_mutex);
        auto& byHash = s_made[key];
        if (const auto it = byHash.find(h); it != byHash.end()) return it->second;
        const std::vector<uint8_t> text = card_text(static_cast<const uint8_t*>(original), size, words.c_str());
        auto* out = static_cast<uint8_t*>(pc_lowmem_alloc(text.size()));
        if (out == nullptr) return nullptr; /* the low region is full: the disc's text, never a crash */
        std::memcpy(out, text.data(), text.size());
        byHash[h] = out;
        s_bytes += text.size();
        return out;
    }
    if (s_lang != "pseudo") {
        /* a language from a language disc (pc_region_text.cpp): its text for the same key, copied below 4 GB once per
         * language and key (the game keeps 32-bit text pointers); NULL (the disc's English) while it is still loading
         * or for a key that language lacks. Kept for the whole run, like every text here (about 2 MB a language at
         * most, only for messages actually shown) */
        if (s_lang == "en") return nullptr;
        size_t size = 0;
        const uint8_t* text = pc_region_text_get(s_lang.c_str(), key, &size);
        if (text == nullptr || size == 0) return nullptr;
        const std::lock_guard lock(s_mutex);
        uint64_t langHash = 1469598103934665603ull;
        for (const char ch : s_lang) langHash = (langHash ^ (uint8_t)ch) * 1099511628211ull;
        auto& byHash = s_made[key];
        if (const auto it = byHash.find(langHash); it != byHash.end()) return it->second;
        auto* out = static_cast<uint8_t*>(pc_lowmem_alloc(size));
        if (out == nullptr) return nullptr; /* the low region is full: the disc's text, never a crash */
        std::memcpy(out, text, size);
        /* *Fixed 2026-11-15 (snatcherLeo's hand check: on the title's Options screen the European languages' Rumble and Sound
         * values sat off from where English puts them; "we just gotta perfectly match those settings options alignment
         * with English"):* the European disc writes those four values (0x1900-0x1903: STEREO, MONO, ON, OFF and their
         * translations) as "font 0, align 1" (FFFF 07 00, FFFF 53 01: centred, for its own Options layout), the US
         * disc as "font 0, font 0" (FFFF 07 00, FFFF 07 00: from the game's x). The US codes go back in, so the
         * values start where English's do; the words stay the language's (verified on the disc's bytes, 2026-11-15) */
        const uint32_t id = key & 0xFFFFF;
        if (id >= 0x1900 && id <= 0x1903 && size >= 8) {
            static const uint8_t kEu[8] = {0xFF, 0xFF, 0x07, 0x00, 0xFF, 0xFF, 0x53, 0x01};
            if (std::memcmp(out, kEu, 8) == 0) out[6] = 0x07, out[7] = 0x00;
        }
        byHash[langHash] = out;
        s_bytes += size;
        return out;
    }
    uint64_t hash = 0;
    /* 64 KB: far above the longest message (guess: a few hundred bytes); a text without an end stays the disc's */
    const size_t size = measure(static_cast<const uint8_t*>(original), 64 * 1024, &hash);
    if (size == 0) return nullptr;
    const std::lock_guard lock(s_mutex);
    auto& byHash = s_made[key];
    if (const auto it = byHash.find(hash); it != byHash.end()) return it->second;
    auto* out = static_cast<uint8_t*>(pc_lowmem_alloc(size));
    if (out == nullptr) return nullptr; /* the low region is full: the disc's text, never a crash */
    pseudo_text(static_cast<const uint8_t*>(original), size, out);
    byHash[hash] = out;
    s_bytes += size;
    return out;
}

} // namespace

/* the language the player chose: ORRE_LANG (a test run), else settings.ini "language"; "auto" follows pc_text's choice
 * (the OS language, when OrrePort has a file for it). The setting itself, not pc_text_language(): a language whose
 * game text comes from a language disc may have no ui/lang file yet (pc_text then shows OrrePort's own words in
 * English; decided 2026-11-15) while the game's text is in that language */
extern "C" void pc_game_text_refresh(void);
static std::string wanted_language() {
    const char* env = SDL_getenv("ORRE_LANG");
    std::string code = env != nullptr ? env : pc_settings_string("language", "auto");
    if (code.empty() || code == "auto") code = pc_text_language();
    return code;
}

/* called once at start-up after pc_text_init (pc_platform.c). The hooks are always registered: with English the
 * text hook returns NULL at once (the disc's text) for every message but the save and card ones (kCard above), and the
 * Options screen can switch languages while the game runs. */
extern "C" void pc_lang_files_apply(void); /* pc_lang_files.cpp */
extern "C" void pc_game_text_start(void) {
    OrreHook_MessageText = message_text;
    OrreHook_MessageExtra = message_extra;
    /* the word pictures follow the same language (piece 4, pc_word_pictures.cpp); their discs are read with the text */
    pc_word_pictures_start();
    const std::string code = wanted_language();
    /* the European disc's text (pc_region_text.cpp): read in the background, or waited for when the game starts in
     * one of its languages, so the first message is already in it */
    const bool fromDisc = code == "de" || code == "fr" || code == "it" || code == "es" || code == "ja";
    pc_region_text_start(fromDisc ? 1 : 0);
    pc_game_text_refresh();
    /* piece 5: the language's credits and Battle Mode titles from its disc (the US disc is open in Aurora by now) */
    pc_lang_files_apply();
}

/* the game's language from the setting, now that the language discs are known: at start-up, and again when the hub
 * added or removed one (pc_region_text_rescan) */
extern "C" void pc_game_text_refresh(void) {
    const std::string code = wanted_language();
    const std::lock_guard lock(s_mutex);
    s_lang = "en";
    if (code == "pseudo") {
        s_lang = "pseudo";
        std::printf("[game_text] the game's messages in the pseudo language (letters swapped, control codes kept)\n");
    } else if (pc_region_text_has(code.c_str())) {
        s_lang = code;
        std::printf("[game_text] the game's messages in %s, from the European disc\n", code.c_str());
    } else if (code != "en") {
        std::printf("[game_text] no game text for %s (its language disc isn't added): the game's messages stay the "
                    "disc's English\n", code.c_str());
    }
}

/* a message's text in the chosen language for showing it (pc_names.cpp: an un-nicknamed Pokémon's species name), given
 * the disc's text; NULL when that is the disc's own (English, or a key the language lacks) */
extern "C" const void* pc_game_text_translate(uint32_t key, const void* original) {
    const std::lock_guard lock(s_mutex);
    s_display = true;
    void* out = message_text(key, original);
    s_display = false;
    return out;
}

/* the game text's language: "en", "pseudo" (test runs), or a language from a language disc (the project's rule: the Options row
 * lists only languages with the game's text) */
extern "C" const char* pc_game_text_language(void) { return s_lang.c_str(); }
extern "C" int pc_game_text_has(const char* code) {
    const std::string c = code != nullptr ? code : "";
    return c == "en" || c == "pseudo" || pc_region_text_has(c.c_str());
}
extern "C" void pc_game_text_set_language(const char* code) {
    if (!pc_game_text_has(code)) return;
    {
        const std::lock_guard lock(s_mutex);
        if (s_lang != code) std::printf("[game_text] the game's messages now in %s\n", code);
        s_lang = code;
    }
    pc_lang_files_apply(); /* outside the lock: it asks for the language again */
}
/* one of OrrePort's own messages (key 0xFFFnnnnn): its UTF-8 text (from ui/lang, so it is translatable) and its font:
 * fontRef is either a game message key whose loaded bank's font it borrows (the bank must be loaded where the text
 * shows; the main menu's help uses 0x3C15), or 0xF000000n for font n directly (2026-11-14), for a font whose
 * bank isn't loaded there (the SAVE FILES list on the title screen uses the Items bag's font 1) */
extern "C" void pc_game_text_set_own(uint32_t key, const char* utf8, uint32_t fontRef) {
    const std::lock_guard lock(s_mutex);
    Own& o = s_own[key];
    o.utf8 = utf8 != nullptr ? utf8 : "";
    o.fontRef = fontRef;
    o.waitA = false;
}
/* the same, ending with the game's wait-for-A arrow: the message window shows the arrow and waits for A before the code
 * that opened it goes on (2026-11-20: the softlock fix's notice before the PC's save question, which otherwise sat
 * under the question's YES / NO box). Platforms: all. */
extern "C" void pc_game_text_set_own_wait(uint32_t key, const char* utf8, uint32_t fontRef) {
    const std::lock_guard lock(s_mutex);
    Own& o = s_own[key];
    o.utf8 = utf8 != nullptr ? utf8 : "";
    o.fontRef = fontRef;
    o.waitA = true;
}
