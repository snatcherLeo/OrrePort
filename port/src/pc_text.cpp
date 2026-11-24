/*
 * pc_text.cpp: OrrePort's words from ui/lang/<code>.txt (pc_text.h says why and how a translator uses it).
 * Platforms: SDL file and locale calls only; the same on Windows, Linux and macOS (verified on Windows; likely
 * elsewhere).
 */
#include "pc_text.h"
#include "pc_paths.h" /* the UI folder's lang\ (4.1) */

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cwchar>

/* clock_24h: the computer's 12 / 24-hour setting, read the OS's own way (Windows / macOS / Linux) */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <clocale>
#include <langinfo.h>
#endif

#ifdef ORRE_SETTINGS_OVERLAY
#include <aurora/rmlui.hpp>
#endif

extern "C" const char* pc_settings_string(const char* key, const char* fallback); /* pc_settings.c */

namespace {

using Table = std::unordered_map<std::string, std::string>;
Table s_en;     /* ui/lang/en.txt: the source, and the fallback for any key a translation lacks */
Table s_lang;   /* the chosen language's file (empty for English) */
std::string s_code = "en";
std::string s_plural = "one-other"; /* the chosen language's plural rule (its "plural" line) */
std::mutex s_missing_mutex;
std::unordered_set<std::string> s_missing; /* keys already reported as missing, so the log names each once */

std::string lang_folder() { return pc_paths_str(PC_PLACE_UI, "lang/"); /* app\UI\lang\ (flat: ui\lang\), 4.1 */ }

/* "key = text" lines, '#' comments, \n for a new line, UTF-8 (a byte order mark at the start is skipped) */
bool read_file(const std::string& code, Table& out) {
    size_t len = 0;
    void* data = SDL_LoadFile((lang_folder() + code + ".txt").c_str(), &len);
    if (data == nullptr) {
        return false;
    }
    std::string text(static_cast<const char*>(data), len);
    SDL_free(data);
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) {
        text.erase(0, 3);
    }
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(first, eq - first);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        size_t v = eq + 1;
        while (v < line.size() && (line[v] == ' ' || line[v] == '\t')) ++v;
        std::string value;
        for (size_t i = v; i < line.size(); ++i) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                const char n = line[i + 1];
                if (n == 'n') { value += '\n'; ++i; continue; }
                if (n == '\\') { value += '\\'; ++i; continue; }
            }
            value += line[i];
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
        if (!key.empty()) out[key] = value;
    }
    return true;
}

/* A language file that holds only OrrePort's words inside the game's screens so far (its line
 * "language.scope = game"): the save and card messages, LOAD SAVE's help, the Options row. Why (2026-11-15): a
 * language disc gives the game's text in German, French, Italian and Spanish, and snatcherLeo asked for OrrePort's in-game
 * lines in those languages too ("we would have to do that as well, do translations"), before the hub is translated.
 * Such a file is used for the game's screens only (pc_text_lang, by the game's language); the hub isn't offered in it
 * and "auto" doesn't pick it, so the hub never shows half a language. Removing the line makes it a full language. */
bool game_only(const Table& t) {
    const auto it = t.find("language.scope");
    return it != t.end() && it->second == "game";
}

/* the made-up test language: English with accents and about 40% more, in brackets, so text not yet on keys (plain
 * English) and boxes that can't grow (cut-off brackets) both stand out. {placeholders}, <markup> and &entities; stay. */
std::string pseudo(const std::string& en) {
    static const std::unordered_map<char, const char*> accents = {
        {'a', "à"}, {'e', "é"}, {'i', "î"}, {'o', "ö"}, {'u', "ü"}, {'c', "ç"}, {'n', "ñ"}, {'s', "š"}, {'y', "ý"},
        {'z', "ž"}, {'A', "Å"}, {'E', "É"}, {'I', "Î"}, {'O', "Ö"}, {'U', "Û"}, {'C', "Ç"}, {'N', "Ñ"}, {'S', "Š"},
        {'Y', "Ý"}, {'Z', "Ž"}};
    std::string out = "[";
    int letters = 0;
    char inside = 0; /* '{', '<' or '&' while inside one */
    for (const char c : en) {
        if (inside == 0 && (c == '{' || c == '<' || c == '&')) inside = c;
        if (inside != 0) {
            out += c;
            if ((inside == '{' && c == '}') || (inside == '<' && c == '>') || (inside == '&' && c == ';')) inside = 0;
            continue;
        }
        const auto it = accents.find(c);
        out += it != accents.end() ? it->second : std::string(1, c);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) ++letters;
    }
    if (letters > 0) {
        out += ' ';
        out.append((size_t)(letters * 2 / 5 + 1), '~');
    }
    return out + "]";
}

/* which plural form a count takes, by the language file's "plural" rule (the forms follow Unicode CLDR's names):
 *   one-other      n = 1 is "one" (English, German, Spanish, Italian, Dutch, Swedish, ...)
 *   french         0 and 1 are "one" (French, Brazilian Portuguese)
 *   east-slavic    one / few / many (Russian, Ukrainian, Belarusian)
 *   polish         one / few / many (Polish)
 *   czech          one / few / other (Czech, Slovak)
 *   none           always "other" (Japanese, Chinese, Korean)
 * A rule this list lacks counts as one-other; a form the file lacks falls back to "other". */
const char* plural_form(long n) {
    const long a = n < 0 ? -n : n, d10 = a % 10, d100 = a % 100;
    if (s_plural == "none") return "other";
    if (s_plural == "french") return a <= 1 ? "one" : "other";
    if (s_plural == "east-slavic") {
        if (d10 == 1 && d100 != 11) return "one";
        if (d10 >= 2 && d10 <= 4 && (d100 < 12 || d100 > 14)) return "few";
        return "many";
    }
    if (s_plural == "polish") {
        if (a == 1) return "one";
        if (d10 >= 2 && d10 <= 4 && (d100 < 12 || d100 > 14)) return "few";
        return "many";
    }
    if (s_plural == "czech") return a == 1 ? "one" : (a >= 2 && a <= 4 ? "few" : "other");
    return a == 1 ? "one" : "other";
}

/* the language to show: ORRE_LANG (a test run), else settings.ini "language", else "auto": Windows' (the OS's) own
 * preferred languages in order, the first one with a file in ui/lang/ (its full code "pt-BR", then its language "pt"),
 * else English */
std::string choose_language() {
    const char* env = SDL_getenv("ORRE_LANG");
    std::string want = env != nullptr ? env : pc_settings_string("language", "auto");
    if (want != "auto" && !want.empty()) return want;
    int count = 0;
    std::string found = "en";
    if (SDL_Locale** locales = SDL_GetPreferredLocales(&count)) {
        for (int i = 0; i < count && found == "en"; ++i) {
            const std::string lang = locales[i]->language != nullptr ? locales[i]->language : "";
            const std::string full = locales[i]->country != nullptr ? lang + "-" + locales[i]->country : lang;
            for (const std::string& code : {full, lang}) {
                if (code.empty() || code == "en") continue;
                Table t;
                if (read_file(code, t) && !game_only(t)) {
                    found = code;
                    break;
                }
            }
            if (lang == "en") break; /* English comes before any other the player listed */
        }
        SDL_free(locales);
    }
    return found;
}

const std::string* lookup(const std::string& key) {
    if (const auto it = s_lang.find(key); it != s_lang.end()) return &it->second;
    if (const auto it = s_en.find(key); it != s_en.end()) return &it->second;
    return nullptr;
}

std::string fill(std::string text, std::initializer_list<std::pair<const char*, std::string>> values) {
    for (const auto& [name, value] : values) {
        const std::string token = std::string("{") + name + "}";
        for (size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size())) {
            text.replace(at, token.size(), value);
        }
    }
    return text;
}

bool looks_like_key(const std::string& s) {
    if (s.empty() || s.find('.') == std::string::npos || !(s[0] >= 'a' && s[0] <= 'z')) return false;
    for (const char c : s) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) return false;
    }
    return true;
}

} // namespace

extern "C" void pc_text_init(void) {
    s_en.clear();
    s_lang.clear();
    if (!read_file("en", s_en)) {
        std::printf("[text] ui/lang/en.txt is missing: OrrePort shows its text keys instead of words\n");
    }
    s_code = choose_language();
    s_plural = "one-other";
    if (s_code == "pseudo") {
        for (const auto& [key, value] : s_en) {
            s_lang[key] = key == "plural" || key == "language.name" ? value : pseudo(value);
        }
    } else if (s_code != "en" && !read_file(s_code, s_lang)) {
        std::printf("[text] ui/lang/%s.txt not found: English\n", s_code.c_str());
        s_code = "en";
    } else if (s_code != "en" && game_only(s_lang)) {
        /* only the in-game lines so far (game_only above): the hub stays English; the game's screens use the file */
        std::printf("[text] ui/lang/%s.txt has the in-game lines only: OrrePort's hub in English\n", s_code.c_str());
        s_lang.clear();
        s_code = "en";
    }
    if (const std::string* rule = lookup("plural")) s_plural = *rule;
    size_t have = 0;
    for (const auto& [key, value] : s_en) have += s_lang.count(key);
    std::printf("[text] language %s (%zu texts in English%s)\n", s_code.c_str(), s_en.size(),
                s_code == "en" ? "" : (", " + std::to_string(have) + " translated").c_str());
}

extern "C" const char* pc_text(const char* key) {
    if (key == nullptr) return "";
    if (const std::string* t = lookup(key)) return t->c_str();
    const std::lock_guard lock(s_missing_mutex);
    if (s_missing.insert(key).second) std::printf("[text] no text for the key %s (add it to ui/lang/en.txt)\n", key);
    return key;
}

/* One of OrrePort's words in a given language, for text inside the game's screens, which follows the game's language
 * (the Options row can change it while the game runs; pc_game_text.cpp) rather than the hub's. The file is read on
 * first use and kept; a key it lacks (or a language with no file) gives English. The pointer stays valid for the run. */
extern "C" const char* pc_text_lang(const char* code, const char* key) {
    static std::mutex mutex;
    static std::unordered_map<std::string, Table> tables;
    if (key == nullptr) return "";
    const std::string c = code != nullptr ? code : "en";
    if (c != "en" && c != "pseudo") {
        const std::lock_guard lock(mutex);
        auto it = tables.find(c);
        if (it == tables.end()) {
            Table t;
            read_file(c, t); /* none: an empty table, so English */
            it = tables.emplace(c, std::move(t)).first;
        }
        if (const auto f = it->second.find(key); f != it->second.end()) return f->second.c_str();
    }
    if (const auto f = s_en.find(key); f != s_en.end()) return f->second.c_str();
    return pc_text(key); /* logs the missing key once */
}

extern "C" const char* pc_text_format(char* out, size_t size, const char* key, ...) {
    std::string text = pc_text(key);
    va_list args;
    va_start(args, key);
    for (;;) {
        const char* name = va_arg(args, const char*);
        if (name == nullptr) break;
        const char* value = va_arg(args, const char*);
        text = fill(text, {{name, value != nullptr ? value : ""}});
    }
    va_end(args);
    if (size > 0) SDL_strlcpy(out, text.c_str(), size);
    return out;
}

extern "C" const char* pc_text_language(void) { return s_code.c_str(); }

std::string tx(const char* key, std::initializer_list<std::pair<const char*, std::string>> values) {
    return fill(pc_text(key), values);
}

std::string tx_lang(const char* code, const char* key,
                    std::initializer_list<std::pair<const char*, std::string>> values) {
    return fill(pc_text_lang(code, key), values);
}

std::string tx_count(const char* key, long n, std::initializer_list<std::pair<const char*, std::string>> values) {
    const std::string base = key;
    const std::string* form = lookup(base + "." + plural_form(n));
    if (form == nullptr) form = lookup(base + ".other");
    std::string text = form != nullptr ? *form : pc_text((base + ".other").c_str());
    text = fill(text, {{"n", std::to_string(n)}});
    return fill(text, values);
}

/* Whether this computer shows the time 24-hour (snatcherLeo, 2026-11-15: "I'd prefer 12 hour clock, but we could also just
 * match system clock of users computer, and this would hold true for linux and mac"). Read once from the system's own
 * setting, 12-hour when it can't be read:
 *   - Windows: the user's time format (GetLocaleInfoEx, LOCALE_STIMEFORMAT): "H" is the 24-hour hour (verified API).
 *   - macOS (likely, not built yet): the locale's time pattern for the hour ("j"), from CoreFoundation's date
 *     formatter template; "H" or "k" there means 24-hour.
 *   - Linux and the Steam Deck (likely, not built yet): the locale's LC_TIME time format (nl_langinfo(T_FMT)); %H / %k
 *     mean 24-hour, %I / %l / %r 12-hour. */
bool clock_24h() {
    static int cached = -1;
    if (cached >= 0) return cached == 1;
    bool h24 = false;
#if defined(_WIN32)
    wchar_t fmt[80];
    if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, fmt, 80) > 0) h24 = std::wcschr(fmt, L'H') != nullptr;
#elif defined(__APPLE__)
    CFLocaleRef loc = CFLocaleCopyCurrent();
    if (CFStringRef pat = CFDateFormatterCreateDateFormatFromTemplate(nullptr, CFSTR("j"), 0, loc)) {
        char buf[32];
        if (CFStringGetCString(pat, buf, sizeof(buf), kCFStringEncodingUTF8))
            h24 = std::strchr(buf, 'H') != nullptr || std::strchr(buf, 'k') != nullptr;
        CFRelease(pat);
    }
    CFRelease(loc);
#else
    std::setlocale(LC_TIME, "");
    const char* t = nl_langinfo(T_FMT);
    h24 = t != nullptr && (std::strstr(t, "%H") != nullptr || std::strstr(t, "%k") != nullptr ||
                           std::strstr(t, "%T") != nullptr || std::strstr(t, "%R") != nullptr);
#endif
    cached = h24 ? 1 : 0;
    std::printf("[text] the clock shows %s (the computer's setting)\n", h24 ? "24-hour" : "12-hour");
    return h24;
}

std::string tx_time(int hour, int minute) {
    char h[8], hh[8], mm[8];
    std::snprintf(h, sizeof(h), "%d", hour % 12 == 0 ? 12 : hour % 12);
    std::snprintf(hh, sizeof(hh), "%02d", hour);
    std::snprintf(mm, sizeof(mm), "%02d", minute);
    return clock_24h() ? tx("time.24", {{"hh", hh}, {"mm", mm}})
                       : tx("time.12", {{"h", h}, {"mm", mm}, {"ampm", tx(hour < 12 ? "time.am" : "time.pm")}});
}

std::string tx_date(int year, int month, int day, int hour, int minute, bool longMonth) {
    char hh[8], mm[8];
    std::snprintf(hh, sizeof(hh), "%02d", hour);
    std::snprintf(mm, sizeof(mm), "%02d", minute);
    const std::string time = tx_time(hour, minute);
    const int m = month < 1 || month > 12 ? 1 : month;
    const std::string month_key = "month." + std::to_string(m), long_key = "month.long." + std::to_string(m);
    return tx(longMonth ? "date.time.long" : "date.time",
              {{"day", std::to_string(day)}, {"month", pc_text(month_key.c_str())},
               {"monthlong", pc_text(long_key.c_str())}, {"year", std::to_string(year)}, {"time", time},
               {"hh", hh}, {"mm", mm}});
}

std::vector<std::pair<std::string, std::string>> pc_text_languages() {
    std::vector<std::pair<std::string, std::string>> out;
    int count = 0;
    const std::string folder = lang_folder();
    if (char** files = SDL_GlobDirectory(folder.c_str(), "*.txt", SDL_GLOB_CASEINSENSITIVE, &count)) {
        for (int i = 0; i < count; ++i) {
            std::string code = files[i];
            code.resize(code.size() - 4); /* ".txt" */
            Table t;
            if (code.empty() || !read_file(code, t) || game_only(t)) continue;
            const auto name = t.find("language.name");
            out.emplace_back(code, name != t.end() ? name->second : code);
        }
        SDL_free(files);
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if ((a.first == "en") != (b.first == "en")) return a.first == "en";
        return a.second < b.second;
    });
    return out;
}

void pc_text_hook_pages(void) {
#ifdef ORRE_SETTINGS_OVERLAY
    /* a page's text: each "[key]" in it becomes that key's words; anything else stays as written */
    aurora::rmlui::set_text_translator([](std::string& translated, const std::string& input) {
        translated.clear();
        size_t at = 0;
        while (at < input.size()) {
            const size_t open = input.find('[', at);
            const size_t close = open == std::string::npos ? std::string::npos : input.find(']', open);
            if (close == std::string::npos) {
                translated.append(input, at, std::string::npos);
                break;
            }
            const std::string key = input.substr(open + 1, close - open - 1);
            translated.append(input, at, open - at);
            if (looks_like_key(key)) {
                translated += pc_text(key.c_str());
            } else {
                translated.append(input, open, close - open + 1);
            }
            at = close + 1;
        }
    });
#endif
}
