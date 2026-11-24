/*
 * pc_font_files.cpp: the shipped fonts' folder, the Noto last-resort chain, the CJK region rule and the stand-in codes
 * (pc_font_files.h says what each is for). the project's build helper, 2026-11-16.
 *
 * Platforms: plain C++ over SDL; Windows, Linux and macOS alike (verified on Windows; likely elsewhere, nothing here
 * asks the OS for anything but the exe's folder and the preferred languages, which SDL answers on all three).
 */
#include "pc_font_files.h"
#include "pc_paths.h" /* the Fonts folder beside the exe (4.1) */

#include <SDL3/SDL.h>

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

/* The Noto chain (decided 2026-11-15, snatcherLeo: "Yes with emoji"; Arabic and Hebrew added 2026-11-16 at the project's request,
 * drawn unjoined until the right-to-left work). The order, and why:
 *   1. Noto Sans: Latin with every mark, Greek, Cyrillic, Vietnamese. First, because most missing letters in European
 *      names are these, and its letters sit closest to the game's own sans-serif Latin.
 *   2. Noto Sans CJK (one collection; the region face per character, below). Before the symbol fonts, so CJK
 *      punctuation and full-width signs take the CJK design.
 *   3-4. Noto Sans Symbols and Symbols 2: arrows, signs, dingbats, chess, the black-and-white forms of many symbols.
 *   5-10. One font per script: Thai, Devanagari, Georgian, Armenian, Arabic, Hebrew.
 *   11-80. *Added 2026-11-16 (snatcherLeo: "Let's add all the missing noto faces now, which can cover all languages"):* one
 *      font for every other living script Noto covers (Bengali to Yi below; Tibetan, Toto, Nyiakeng Puachue Hmong and
 *      Yezidi exist only as Noto Serif). Historic and extinct scripts (Linear B, cuneiform, hieroglyphs...) are left
 *      out on purpose. Each is only asked for its own script's code points (`ranges`).
 *   81. Noto Sans Math (mathematical letters and operators the symbol fonts lack).
 *   82. Noto Color Emoji, last: a symbol the black-and-white fonts already have (♥, ☺) keeps its text form, as text in
 *      the game's letters should; only real emoji reach it.
 * No shaping: Indic, Thai, Khmer, Myanmar and the other complex scripts draw letter by letter (a typed base letter is
 * right; vowel signs and conjuncts don't join or move; Arabic and Hebrew aren't joined or reversed yet). That needs a
 * shaping engine (HarfBuzz, not in the build: translation.md), the next items after this one.
 * *Updated 2026-11-16:* HarfBuzz is in. Names are shaped with these same fonts (pc_shape.h: each per-script font is
 * also the shaping font for its script), and the hub's pages through RmlUi's HarfBuzz engine (hub_text/).
 * Weights: the variable fonts at 500 (guess: between the game's 22-px letters, a regular MS PGothic drawn a touch
 * bolder, and its heavier small fonts; to be tuned in the font lab like the others), the fonts with one Regular weight
 * emboldened a little (16, an eighth of a pixel a side at the game's size) to sit near them. One face per script, no
 * extra weights, to keep the download small (the 70 added are 17.3 MB). */
#define V(f, w, r) {f, 0, 500.f, 0, 0, w, r} /* a variable font at weight 500 */
#define S(f, w, r) {f, 0, 0.f, 16, 0, w, r}  /* one static Regular, emboldened a little */
const PcNotoFont kChain[] = {
    {"NotoSans[wdth,wght].ttf", 0, 500.f, 0, 0, "Noto Sans", nullptr},
    {PC_NOTO_CJK_FILE, -1, 0.f, 0, 0, "Noto Sans CJK", nullptr},
    {"NotoSansSymbols[wght].ttf", 0, 500.f, 0, 0, "Noto Sans Symbols", nullptr},
    {"NotoSansSymbols2-Regular.ttf", 0, 0.f, 16, 0, "Noto Sans Symbols 2", nullptr},
    V("NotoSansThai[wdth,wght].ttf", "Noto Sans Thai", "0E00-0E7F"),
    V("NotoSansDevanagari[wdth,wght].ttf", "Noto Sans Devanagari", "0900-097F A8E0-A8FF 11B00-11B5F 1CD0-1CFF"),
    V("NotoSansGeorgian[wdth,wght].ttf", "Noto Sans Georgian", "10A0-10FF 2D00-2D2F 1C90-1CBF"),
    V("NotoSansArmenian[wdth,wght].ttf", "Noto Sans Armenian", "0530-058F FB13-FB17"),
    V("NotoSansArabic[wdth,wght].ttf", "Noto Sans Arabic", "0600-06FF 0750-077F 0870-08FF FB50-FDFF FE70-FEFF 10EC0-10EFF"),
    V("NotoSansHebrew[wdth,wght].ttf", "Noto Sans Hebrew", "0590-05FF FB1D-FB4F"),
    V("NotoSansBengali[wdth,wght].ttf", "Noto Sans Bengali", "0980-09FF"),
    V("NotoSansGurmukhi[wdth,wght].ttf", "Noto Sans Gurmukhi", "0A00-0A7F"),
    V("NotoSansGujarati[wdth,wght].ttf", "Noto Sans Gujarati", "0A80-0AFF"),
    V("NotoSansOriya[wdth,wght].ttf", "Noto Sans Oriya", "0B00-0B7F"),
    V("NotoSansTamil[wdth,wght].ttf", "Noto Sans Tamil", "0B80-0BFF 11FC0-11FFF"),
    V("NotoSansTelugu[wdth,wght].ttf", "Noto Sans Telugu", "0C00-0C7F"),
    V("NotoSansKannada[wdth,wght].ttf", "Noto Sans Kannada", "0C80-0CFF"),
    V("NotoSansMalayalam[wdth,wght].ttf", "Noto Sans Malayalam", "0D00-0D7F"),
    V("NotoSansSinhala[wdth,wght].ttf", "Noto Sans Sinhala", "0D80-0DFF 111E0-111FF"),
    V("NotoSansLao[wdth,wght].ttf", "Noto Sans Lao", "0E80-0EFF"),
    V("NotoSerifTibetan[wght].ttf", "Noto Serif Tibetan", "0F00-0FFF"),
    V("NotoSansMyanmar[wdth,wght].ttf", "Noto Sans Myanmar", "1000-109F A9E0-A9FF AA60-AA7F"),
    V("NotoSansEthiopic[wdth,wght].ttf", "Noto Sans Ethiopic", "1200-139F 2D80-2DDF AB00-AB2F 1E7E0-1E7FF"),
    V("NotoSansCherokee[wght].ttf", "Noto Sans Cherokee", "13A0-13FF AB70-ABBF"),
    V("NotoSansCanadianAboriginal[wght].ttf", "Noto Sans Canadian Aboriginal", "1400-167F 18B0-18FF 11AB0-11ABF"),
    V("NotoSansKhmer[wdth,wght].ttf", "Noto Sans Khmer", "1780-17FF 19E0-19FF"),
    S("NotoSansMongolian-Regular.ttf", "Noto Sans Mongolian", "1800-18AF 11660-1167F"),
    S("NotoSansTagalog-Regular.ttf", "Noto Sans Tagalog", "1700-171F"),
    S("NotoSansHanunoo-Regular.ttf", "Noto Sans Hanunoo", "1720-173F"),
    S("NotoSansBuhid-Regular.ttf", "Noto Sans Buhid", "1740-175F"),
    S("NotoSansTagbanwa-Regular.ttf", "Noto Sans Tagbanwa", "1760-177F"),
    S("NotoSansLimbu-Regular.ttf", "Noto Sans Limbu", "1900-194F"),
    S("NotoSansTaiLe-Regular.ttf", "Noto Sans Tai Le", "1950-197F"),
    V("NotoSansNewTaiLue[wght].ttf", "Noto Sans New Tai Lue", "1980-19DF"),
    S("NotoSansBuginese-Regular.ttf", "Noto Sans Buginese", "1A00-1A1F"),
    V("NotoSansTaiTham[wght].ttf", "Noto Sans Tai Tham", "1A20-1AAF"),
    V("NotoSansBalinese[wght].ttf", "Noto Sans Balinese", "1B00-1B7F"),
    V("NotoSansSundanese[wght].ttf", "Noto Sans Sundanese", "1B80-1BBF 1CC0-1CCF"),
    S("NotoSansBatak-Regular.ttf", "Noto Sans Batak", "1BC0-1BFF"),
    S("NotoSansLepcha-Regular.ttf", "Noto Sans Lepcha", "1C00-1C4F"),
    V("NotoSansOlChiki[wght].ttf", "Noto Sans Ol Chiki", "1C50-1C7F"),
    S("NotoSansCoptic-Regular.ttf", "Noto Sans Coptic", "03E2-03EF 2C80-2CFF 102E0-102FF"),
    S("NotoSansTifinagh-Regular.ttf", "Noto Sans Tifinagh", "2D30-2D7F"),
    S("NotoSansYi-Regular.ttf", "Noto Sans Yi", "A000-A4CF"),
    V("NotoSansLisu[wght].ttf", "Noto Sans Lisu", "A4D0-A4FF 11FB0-11FBF"),
    S("NotoSansVai-Regular.ttf", "Noto Sans Vai", "A500-A63F"),
    V("NotoSansBamum[wght].ttf", "Noto Sans Bamum", "A6A0-A6FF 16800-16A3F"),
    S("NotoSansSylotiNagri-Regular.ttf", "Noto Sans Syloti Nagri", "A800-A82F"),
    S("NotoSansSaurashtra-Regular.ttf", "Noto Sans Saurashtra", "A880-A8DF"),
    V("NotoSansKayahLi[wght].ttf", "Noto Sans Kayah Li", "A900-A92F"),
    V("NotoSansJavanese[wght].ttf", "Noto Sans Javanese", "A980-A9DF"),
    V("NotoSansCham[wght].ttf", "Noto Sans Cham", "AA00-AA5F"),
    S("NotoSansTaiViet-Regular.ttf", "Noto Sans Tai Viet", "AA80-AADF"),
    V("NotoSansMeeteiMayek[wght].ttf", "Noto Sans Meetei Mayek", "AAE0-AAFF ABC0-ABFF"),
    S("NotoSansNKo-Regular.ttf", "Noto Sans NKo", "07C0-07FF"),
    V("NotoSansThaana[wght].ttf", "Noto Sans Thaana", "0780-07BF"),
    V("NotoSansSyriac[wght].ttf", "Noto Sans Syriac", "0700-074F 0860-086F"),
    S("NotoSansMandaic-Regular.ttf", "Noto Sans Mandaic", "0840-085F"),
    S("NotoSansOsage-Regular.ttf", "Noto Sans Osage", "104B0-104FF"),
    V("NotoSansHanifiRohingya[wght].ttf", "Noto Sans Hanifi Rohingya", "10D00-10D3F"),
    V("NotoSerifYezidi[wght].ttf", "Noto Serif Yezidi", "10E80-10EBF"),
    V("NotoSansSoraSompeng[wght].ttf", "Noto Sans Sora Sompeng", "110D0-110FF"),
    S("NotoSansChakma-Regular.ttf", "Noto Sans Chakma", "11100-1114F"),
    S("NotoSansNewa-Regular.ttf", "Noto Sans Newa", "11400-1147F"),
    S("NotoSansWarangCiti-Regular.ttf", "Noto Sans Warang Citi", "118A0-118FF"),
    S("NotoSansSunuwar-Regular.ttf", "Noto Sans Sunuwar", "11BC0-11BFF"),
    S("NotoSansMasaramGondi-Regular.ttf", "Noto Sans Masaram Gondi", "11D00-11D5F"),
    V("NotoSansGunjalaGondi[wght].ttf", "Noto Sans Gunjala Gondi", "11D60-11DAF"),
    V("NotoSansTangsa[wght].ttf", "Noto Sans Tangsa", "16A70-16ACF"),
    V("NotoSansBassaVah[wght].ttf", "Noto Sans Bassa Vah", "16AD0-16AFF"),
    S("NotoSansPahawhHmong-Regular.ttf", "Noto Sans Pahawh Hmong", "16B00-16B8F"),
    V("NotoSansMedefaidrin[wght].ttf", "Noto Sans Medefaidrin", "16E40-16E9F"),
    S("NotoSansMiao-Regular.ttf", "Noto Sans Miao", "16F00-16F9F"),
    V("NotoSerifNPHmong[wght].ttf", "Noto Serif Nyiakeng Puachue Hmong", "1E100-1E14F"),
    V("NotoSerifToto[wght].ttf", "Noto Serif Toto", "1E290-1E2BF"),
    S("NotoSansWancho-Regular.ttf", "Noto Sans Wancho", "1E2C0-1E2FF"),
    V("NotoSansNagMundari[wght].ttf", "Noto Sans Nag Mundari", "1E4D0-1E4FF"),
    S("NotoSansMendeKikakui-Regular.ttf", "Noto Sans Mende Kikakui", "1E800-1E8DF"),
    V("NotoSansAdlam[wght].ttf", "Noto Sans Adlam", "1E900-1E95F"),
    S("NotoSansMath-Regular.ttf", "Noto Sans Math", "2190-23FF 25A0-25FF 27C0-27FF 2900-2AFF 1D400-1D7FF 1EE00-1EEFF"),
    {"NotoColorEmoji.ttf", 0, 0.f, 0, 1, "Noto Color Emoji", nullptr},
};
#undef V
#undef S

bool hangul(uint32_t c) {
    return (c >= 0x1100 && c <= 0x11FF) || (c >= 0x3130 && c <= 0x318F) || (c >= 0xA960 && c <= 0xA97F) ||
           (c >= 0xAC00 && c <= 0xD7FF);
}
bool bopomofo(uint32_t c) { return (c >= 0x3100 && c <= 0x312F) || (c >= 0x31A0 && c <= 0x31BF); }
bool kana(uint32_t c) { return (c >= 0x3040 && c <= 0x30FF) || (c >= 0x31F0 && c <= 0x31FF) || (c >= 0xFF66 && c <= 0xFF9F); }

/* a CJK language code to its face; -1 when the code isn't one */
int face_of(const char* lang, const char* country) {
    if (lang == nullptr) return -1;
    auto is = [&](const char* l) { return SDL_strncasecmp(lang, l, 2) == 0; };
    if (is("ja")) return PC_NOTO_CJK_JP;
    if (is("ko")) return PC_NOTO_CJK_KR;
    if (is("zh")) {
        /* the code may carry its script or country ("zh-Hant", "zh-TW") or SDL may give the country apart */
        const std::string full = std::string(lang) + "-" + (country != nullptr ? country : "");
        auto has = [&](const char* s) { return SDL_strcasestr(full.c_str(), s) != nullptr; };
        if (has("HK") || has("MO")) return PC_NOTO_CJK_HK;
        if (has("Hant") || has("TW")) return PC_NOTO_CJK_TC;
        return PC_NOTO_CJK_SC;
    }
    return -1;
}

/* the system's first CJK language, once (SDL_GetPreferredLocales: the OS's own list, in the player's order) */
int system_face() {
    static int s_face = -2;
    if (s_face != -2) return s_face;
    s_face = -1;
    int count = 0;
    if (SDL_Locale** locales = SDL_GetPreferredLocales(&count)) {
        for (int i = 0; i < count && s_face < 0; ++i) s_face = face_of(locales[i]->language, locales[i]->country);
        SDL_free(locales);
    }
    return s_face;
}

/* --- stand-ins --- */
constexpr uint16_t kStandinFirst = 0xE000, kStandinLast = 0xF8FF; /* Unicode's private-use area in 16 bits */
std::mutex s_standinLock;
std::unordered_map<uint32_t, uint16_t> s_toStandin;
std::vector<uint32_t> s_fromStandin; /* index = code - kStandinFirst */
/* shaped clusters (pc_shape.h): their record -> code, and per code its record ("" for a plain stand-in) */
std::unordered_map<std::string, uint16_t> s_clusterToStandin;
std::vector<std::string> s_clusterKey; /* index = code - kStandinFirst, filled lazily */

} // namespace

extern "C" const char* pc_font_dir(char* out, size_t size) {
    if (out == nullptr || size == 0) return "";
    return pc_paths_get(PC_PLACE_FONTS, "", out, size); /* <exe folder>/Fonts/ in both layouts (app\Fonts\), 4.1 */
}

extern "C" int pc_noto_count(void) { return (int)(sizeof(kChain) / sizeof(kChain[0])); }
extern "C" const PcNotoFont* pc_noto_font(int i) { return i >= 0 && i < pc_noto_count() ? &kChain[i] : nullptr; }

extern "C" int pc_noto_covers(const PcNotoFont* f, uint32_t cp) {
    if (f == nullptr) return 0;
    if (f->ranges == nullptr) return 1;
    for (const char* p = f->ranges; *p != 0;) {
        char* end = nullptr;
        const unsigned long a = SDL_strtoul(p, &end, 16);
        if (end == p) break;
        unsigned long b = a;
        p = end;
        if (*p == '-') {
            b = SDL_strtoul(p + 1, &end, 16);
            p = end;
        }
        if (cp >= a && cp <= b) return 1;
        while (*p == ' ') ++p;
    }
    return 0;
}

/* The region rule. Noto Sans CJK draws the same character in each region's own forms (a kanji and its Chinese
 * counterpart differ in small strokes), so the face is chosen by what the character is and who is typing:
 *   1. the script itself where it names a region: hangul -> Korean, bopomofo -> Traditional Chinese, kana -> Japanese
 *      (every face has these letters; this only gives each its home design);
 *   2. otherwise (kanji / hanzi, CJK signs, full-width forms) by the language: the text's language when it is a CJK
 *      one (Japanese is the only official CJK language today; Korean and the two Chinese come with translations);
 *   3. else the system's first CJK language (Windows / macOS / Linux preferred languages), so a player on a Chinese
 *      system typing hanzi in an English game sees Chinese forms;
 *   4. else Japanese: the game is Japanese-made, its own kanji are Japanese forms, and kanji in a name on a system with
 *      no CJK language most likely came from a Japanese input method (guess).
 * Kept per character, not per name: a name mixing hangul and kanji draws each in its home design. */
extern "C" int pc_noto_cjk_face(uint32_t cp, const char* lang) {
    if (hangul(cp)) return PC_NOTO_CJK_KR;
    if (bopomofo(cp)) return PC_NOTO_CJK_TC;
    if (kana(cp)) return PC_NOTO_CJK_JP;
    return pc_noto_cjk_default(lang);
}

extern "C" int pc_noto_cjk_default(const char* lang) {
    const int byLang = face_of(lang, nullptr);
    if (byLang >= 0) return byLang;
    const int bySystem = system_face();
    return bySystem >= 0 ? bySystem : PC_NOTO_CJK_JP;
}

extern "C" int pc_noto_is_cjk(uint32_t c) {
    return hangul(c) || bopomofo(c) || kana(c) || (c >= 0x2E80 && c <= 0x2FDF) /* radicals */ ||
           (c >= 0x3000 && c <= 0x303F) /* CJK signs */ || (c >= 0x3190 && c <= 0x31EF) || (c >= 0x3200 && c <= 0x33FF) ||
           (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xF900 && c <= 0xFAFF) ||
           (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFFEF) || (c >= 0x20000 && c <= 0x3FFFF);
}

extern "C" uint16_t pc_text_standin(uint32_t cp) {
    if (cp < 0x10000) return (uint16_t)cp;
    std::lock_guard<std::mutex> lock(s_standinLock);
    if (const auto it = s_toStandin.find(cp); it != s_toStandin.end()) return it->second;
    if (s_fromStandin.size() > (size_t)(kStandinLast - kStandinFirst)) return 0; /* 6,400 different ones in a run: full */
    const uint16_t code = (uint16_t)(kStandinFirst + s_fromStandin.size());
    s_fromStandin.push_back(cp);
    s_toStandin.emplace(cp, code);
    return code;
}

/* Why clusters share the emoji stand-ins' area and allocator: both are "one letter in the game's text that part D draws
 * from something else", and the area's 6,400 codes are plenty for a run (a name's clusters are a few dozen at most, and
 * the same cluster reuses its code). */
extern "C" uint16_t pc_text_standin_cluster(const void* key, size_t len, uint32_t first_cp) {
    if (key == nullptr || len == 0) return 0;
    std::lock_guard<std::mutex> lock(s_standinLock);
    std::string k(static_cast<const char*>(key), len);
    if (const auto it = s_clusterToStandin.find(k); it != s_clusterToStandin.end()) return it->second;
    if (s_fromStandin.size() > (size_t)(kStandinLast - kStandinFirst)) return 0;
    const uint16_t code = (uint16_t)(kStandinFirst + s_fromStandin.size());
    s_fromStandin.push_back(first_cp);
    s_clusterKey.resize(s_fromStandin.size());
    s_clusterKey.back() = k;
    s_clusterToStandin.emplace(std::move(k), code);
    return code;
}

/* copied out under the lock: the table's strings move when it grows, so no pointer into it is handed out */
extern "C" size_t pc_text_cluster_key(uint16_t code, void* out, size_t cap) {
    if (code < kStandinFirst || code > kStandinLast) return 0;
    std::lock_guard<std::mutex> lock(s_standinLock);
    const size_t i = (size_t)(code - kStandinFirst);
    if (i >= s_clusterKey.size() || s_clusterKey[i].empty() || s_clusterKey[i].size() > cap) return 0;
    std::memcpy(out, s_clusterKey[i].data(), s_clusterKey[i].size());
    return s_clusterKey[i].size();
}

extern "C" uint32_t pc_text_real(uint16_t code) {
    if (code < kStandinFirst || code > kStandinLast) return code;
    std::lock_guard<std::mutex> lock(s_standinLock);
    const size_t i = (size_t)(code - kStandinFirst);
    return i < s_fromStandin.size() ? s_fromStandin[i] : code;
}

extern "C" int pc_text_is_standin(uint16_t code) {
    if (code < kStandinFirst || code > kStandinLast) return 0;
    std::lock_guard<std::mutex> lock(s_standinLock);
    return (size_t)(code - kStandinFirst) < s_fromStandin.size();
}

/* Why an emoji sequence keeps only its first emoji: joining 👨 + ZWJ + 👩 + ZWJ + 👧 into one family picture, or a hand
 * with a skin tone, is a ligature the font's GSUB table makes, which needs a shaping engine (HarfBuzz; not in the build,
 * translation.md). Without one, each part would draw as its own picture and take its own slot of the 12; keeping the
 * first emoji keeps one typed symbol as one letter. When HarfBuzz comes (the right-to-left work may bring it), whole
 * sequences can get one stand-in each.
 * *Updated 2026-11-16 (HarfBuzz added, pc_shape.h):* with shaping on (the default) the naming keyboard keeps every
 * character of a sequence as typed (each takes a letter of the 12), and the drawn copy shapes the sequence with Noto
 * Color Emoji into one picture, carried by one cluster stand-in (pc_text_standin_cluster). The first-emoji rule above
 * is kept for ORRE_SHAPING=0 and as the fallback when shaping fails (pc_names.cpp's shown drops the modifiers). */
extern "C" int pc_text_is_modifier(uint32_t c) {
    return (c >= 0xFE00 && c <= 0xFE0F) /* variation selectors */ || c == 0x200D /* zero-width joiner */ ||
           (c >= 0x1F3FB && c <= 0x1F3FF) /* skin tones */ || (c >= 0xE0020 && c <= 0xE007F) /* tags (flags) */ ||
           (c >= 0xE0100 && c <= 0xE01EF) /* variation selectors supplement */ || c == 0x20E3 /* keycap */;
}
