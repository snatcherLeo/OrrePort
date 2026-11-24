// lang.cpp: Studio's words from the language files (the plan: "every word translatable from the first line"; the rules
// are the hub's, docs/port/translation.md and the header of port/ui/lang/en.txt).
// Format: "key = value" lines, '#' notes, \n for a line break, {name} placeholders, key.one / key.other for counts.
// Which language: OrrePort's own setting (settings.ini "language"), else Windows' display language, else English;
// "pseudo" makes every text longer and accented to test layouts. Anything missing falls back to English, then to the
// key itself in brackets so a missing line is easy to spot.
// Why Studio has its own files (studio/lang/<code>.txt, 2026-11-18): Studio is its own program, and two sessions
// never edit one file; the format and rules are OrrePort's, and lang_check.py checks both folders.
// Platforms: the parser is plain C++ and would move into the shared core for the Qt and AppKit shells; only the
// Windows language query (GetUserDefaultUILanguage) is Windows-only (Linux: $LANG / $LC_MESSAGES; macOS:
// CFLocaleCopyPreferredLanguages).
#include "app.hpp"

#include <fstream>
#include <sstream>

namespace studio {

namespace {
std::map<std::string, std::string> s_en, s_lang;
bool s_pseudo = false;

void parse(const std::wstring& path, std::map<std::string, std::string>& out) {
    std::istringstream f(load_text_file(path));
    std::string line;
    bool first = true;
    while (std::getline(f, line)) {
        if (first && line.size() >= 3 && (uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF) line.erase(0, 3);
        first = false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos || line[a] == '#') continue;
        const size_t eq = line.find('=', a);
        if (eq == std::string::npos) continue;
        std::string key = line.substr(a, eq - a);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        std::string val = line.substr(eq + 1);
        const size_t v0 = val.find_first_not_of(" \t");
        val = v0 == std::string::npos ? "" : val.substr(v0);
        std::string un;
        for (size_t i = 0; i < val.size(); ++i) {
            if (val[i] == '\\' && i + 1 < val.size() && val[i + 1] == 'n') { un += '\n'; ++i; }
            else un += val[i];
        }
        out[key] = un;
    }
}

// pseudo: accented letters and about a third longer, so cut-off text shows up in layouts
std::wstring pseudo(const std::wstring& s) {
    static const wchar_t* from = L"aeiouAEIOUcnyCNY";
    static const wchar_t* to = L"åéîöûÅÉÎÖÛçñýÇÑÝ";
    std::wstring r = L"[";
    bool in_brace = false;
    for (wchar_t c : s) {
        if (c == L'{') in_brace = true;
        if (c == L'}') in_brace = false;
        const wchar_t* p = in_brace ? nullptr : std::wcschr(from, c);
        r += p ? to[p - from] : c;
    }
    r.append(s.size() / 3 + 1, L'~');
    return r + L"]";
}
}  // namespace

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

void lang_load(const std::wstring& dir, const std::string& wanted) {
    s_en.clear();
    s_lang.clear();
    parse(dir + L"\\en.txt", s_en);
    std::string code = wanted;
    if (code.empty() || code == "auto") {
        // Windows: the display language, e.g. "de-DE" -> "de"
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = L"";
        if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) > 0)
            code = narrow(std::wstring(name).substr(0, 2));
    }
    s_pseudo = code == "pseudo";
    if (!s_pseudo && !code.empty() && code != "en") parse(dir + L"\\" + widen(code) + L".txt", s_lang);
}

// The lines both programs say word for word, read from OrrePort's own language files (snatcherLeo, 2026-11-23: "Hub and Studio
// should share exact wording for these things", the messages for a wrong or bad disc): its keys starting "disc.",
// "hub.lang.", "check." and "report." are added over Studio's, so one text (and its translations) serves both. OrrePort's folder:
// app\UI\lang\ beside Studio's own UI\lang\Studio\ in the portable layout, build-pc\studio\ui\lang\ (copied from
// port\ui\lang\ by the build) in development. A key OrrePort doesn't have yet keeps Studio's own line. Plain C++.
void lang_load_shared(const std::wstring& orreport_dir, const std::string& wanted) {
    // ("report." added 2026-11-23: OrrePort's Report a problem took Studio's saved screen, snatcherLeo: "copy what Studio does
    // for file reports"; its report.saved_body, saved_not_copied, size, open_folder and done are both programs' words)
    // ("update." added the same day: the updater's lines, one text for both programs, agreed with Main)
    // ("settings.logs" too: About's logs-folder row, the same in both programs' About, 2026-11-23)
    // ("discs." and "common.cancel" too: the Discs rows' words, the same in the hub's Discs panel and Studio's Settings ›
    // Discs since the project's c4de3f0b7, 2026-11-23)
    static const char* const kPrefixes[] = {"disc.", "hub.lang.", "check.", "report.", "update.", "settings.logs", "discs.", "common.cancel"};
    auto take = [](const std::wstring& path, std::map<std::string, std::string>& into) {
        std::map<std::string, std::string> all;
        parse(path, all);
        for (auto& [k, v] : all)
            for (const char* p : kPrefixes)
                if (k.rfind(p, 0) == 0) { into[k] = v; break; }
        return !all.empty();
    };
    if (!take(orreport_dir + L"\\en.txt", s_en)) return;
    std::string code = wanted;
    if (code.empty() || code == "auto") {
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = L"";
        if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) > 0)
            code = narrow(std::wstring(name).substr(0, 2));
    }
    if (!code.empty() && code != "en" && code != "pseudo") take(orreport_dir + L"\\" + widen(code) + L".txt", s_lang);
}

// Whether a key has a text (the chosen language's or English): for a line that falls back to a general one when an
// item has none of its own (the "Coming soon" pages, 2026-11-22). Plain C++, every platform
bool has_text(const char* key) { return s_lang.count(key) > 0 || s_en.count(key) > 0; }

std::wstring tx(const char* key, std::initializer_list<std::pair<const char*, std::wstring>> args) {
    const std::string* v = nullptr;
    if (auto it = s_lang.find(key); it != s_lang.end()) v = &it->second;
    else if (auto e = s_en.find(key); e != s_en.end()) v = &e->second;
    if (!v) return L"[" + widen(key) + L"]";
    std::wstring s = widen(*v);
    if (s_pseudo) s = pseudo(s);
    for (const auto& [name, value] : args) {
        const std::wstring ph = L"{" + widen(name) + L"}";
        for (size_t p; (p = s.find(ph)) != std::wstring::npos;) s.replace(p, ph.size(), value);
    }
    return s;
}

}  // namespace studio
