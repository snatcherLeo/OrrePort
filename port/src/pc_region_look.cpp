/*
 * pc_region_look.cpp: the Japanese release's look, from one table of regional differences (3.5 steps 2 and 3; the project's
 * helper, 2026-11-19; docs/port/word-pictures.md "The Japanese release look").
 *
 * What it does: reads ui/regions/jp_look.txt (names and descriptions only, no disc bytes; the file's own comment
 * explains every column) and answers the three places that act on it:
 *   - pc_word_pictures.cpp swaps the "sprite" rows: pictures the game draws in pieces from its sprite table (the blue
 *     Nintendo logo, Rui's title picture, three sheets that differ only in colour depth). The piece's record gets the
 *     Japanese picture's id, and the picture is read from the player's Japanese disc when first drawn: the same path the
 *     Japanese word pictures take;
 *   - pc_lang_files.cpp swaps the "entry" rows: pictures the game's code looks up by id with no sprite record naming
 *     them (the marbled menu background, a trainer's VS portrait), and Rui's four 3D models (3.5 step 4). Their archive
 *     is served with that one entry taken from the Japanese disc: the same path the Japanese title logo takes;
 *   - pc_region_text.cpp's disc read notes every entry of the Japanese disc it passes, and the check that every row's
 *     entry is where the table says is logged when the disc is done.
 * A "none" row (none today) is listed and logged as skipped.
 * *Updated 2026-11-19 (3.5 step 4):* the models were "none" until then. Why the archive path and not a hook in the
 * game's archive loader: the loader sizes every buffer from the archive's own entry record (fsysLoadEntrySync and the
 * async path read +0x08 for the unpacked size, +0x14 for the stored size, +0x04 for the offset; verified in the decomp),
 * and the served archive's record carries the Japanese entry's values, so three models of another size than the US
 * ones get buffers of the right size with no change to game code; the ARAM cache then simply caches the Japanese entry.
 *
 * When it applies (snatcherLeo, 2026-11-17): the game's language is Japanese AND the player's Japanese disc is added; all rows
 * together, no switch of their own. ORRE_NO_REGION_LOOK=1 turns every swap of the table off, for side-by-side
 * pictures. Look only: no random numbers, no timing, in every mode (accurate mode included; likely, the determinism
 * test checks it).
 *
 * Why a data file and not a list in the code: the plan's "Showing players what changes" (snatcherLeo approved, 2026-11-17)
 * keeps one table that the swap, the hub's "What changes" list and later Studio's "region variants" all read, so
 * nothing is redone when Studio comes, and a row can be checked without reading code.
 *
 * Platforms: plain C++ and SDL's base path (the same on Windows, Linux and macOS; verified on Windows, likely elsewhere:
 * nothing here is OS-specific). XD: likely a table of its own (XD's Japanese release has its own differences); the
 * format and this reader carry over unchanged (likely).
 */
#include "pc_region_look.h"
#include "pc_paths.h" /* the UI folder (4.1) */

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace {

struct Row {
    std::string f[9]; /* id, kind, entry, archives, region, condition, what, how, note */
    PcRegionLookRow view{};
};
std::vector<Row> s_rows;
std::once_flag s_once;
bool s_off = false;
std::mutex s_mutex;                                  /* s_found: written by the disc read's loader thread */
std::set<std::pair<std::string, std::string>> s_found; /* (entry, archive) seen on the Japanese disc */
std::string s_lastState;                             /* the last state line logged (logged again only when it changes) */

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t at = 0;
    for (;;) {
        const size_t p = s.find(sep, at);
        out.push_back(trim(s.substr(at, p == std::string::npos ? std::string::npos : p - at)));
        if (p == std::string::npos) return out;
        at = p + 1;
    }
}

/* the one condition the code knows (snatcherLeo, 2026-11-17): the game's language Japanese and the Japanese disc added. Any
 * other condition is a row this build can't honour, so it is left out (logged) rather than applied wrongly */
bool known_condition(const std::string& c) {
    const std::vector<std::string> parts = split(c, ' ');
    bool lang = false, disc = false;
    for (const std::string& p : parts) {
        if (p.empty()) continue;
        if (p == "lang=ja") lang = true;
        else if (p == "disc=GC6J01") disc = true;
        else return false;
    }
    return lang && disc;
}

std::string table_path() {
    return pc_paths_str(PC_PLACE_UI, "regions/jp_look.txt"); /* app\UI\ (flat layout: ui\), 4.1 */
}

void load() {
    s_off = SDL_getenv("ORRE_NO_REGION_LOOK") != nullptr;
    const std::string path = table_path();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::printf("[region_look] %s is missing: the Japanese release look stays off (the US pictures)\n",
                    path.c_str());
        return;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        const std::vector<std::string> cols = split(t, '|');
        const char* bad = nullptr;
        if (cols.size() != 9) bad = "not 9 columns";
        else if (cols[1] != "picture" && cols[1] != "model") bad = "kind isn't picture or model";
        else if (cols[2].empty() || cols[3].empty() || cols[0].empty()) bad = "an id, entry or archive is empty";
        else if (cols[4] != "JP") bad = "a region other than JP";
        else if (!known_condition(cols[5])) bad = "a condition other than lang=ja disc=GC6J01";
        else if (cols[7] != "sprite" && cols[7] != "entry" && cols[7] != "none") bad = "how isn't sprite, entry or none";
        /* a model has no sprite record: only the archive-entry path can swap it */
        else if (cols[1] == "model" && cols[7] == "sprite") bad = "a model can't be swapped through the sprite records";
        if (bad != nullptr) {
            std::printf("[region_look] jp_look.txt line %d left out: %s\n", lineNo, bad);
            continue;
        }
        Row r;
        for (int i = 0; i < 9; ++i) r.f[i] = cols[(size_t)i];
        s_rows.push_back(std::move(r));
    }
    /* the views point into the rows' strings: made once the vector stops growing */
    for (Row& r : s_rows)
        r.view = PcRegionLookRow{r.f[0].c_str(), r.f[1].c_str(), r.f[2].c_str(), r.f[3].c_str(), r.f[4].c_str(),
                                 r.f[5].c_str(), r.f[6].c_str(), r.f[7].c_str(), r.f[8].c_str()};
    std::string sprites, entries, none;
    for (const Row& r : s_rows) {
        std::string& to = r.f[7] == "sprite" ? sprites : r.f[7] == "entry" ? entries : none;
        to += (to.empty() ? "" : ", ") + r.f[2];
    }
    std::printf("[region_look] ui/regions/jp_look.txt: %zu rows; swapped through the sprite records: %s; as archive "
                "entries: %s; not swapped yet: %s%s\n", s_rows.size(), sprites.empty() ? "none" : sprites.c_str(),
                entries.empty() ? "none" : entries.c_str(), none.empty() ? "none" : none.c_str(),
                s_off ? " (ORRE_NO_REGION_LOOK: every swap off)" : "");
}

void ensure() { std::call_once(s_once, load); }

} // namespace

extern "C" void pc_region_look_start(void) { ensure(); }

extern "C" int pc_region_look_off(void) {
    ensure();
    return s_off ? 1 : 0;
}

extern "C" int pc_region_look_count(void) {
    ensure();
    return (int)s_rows.size();
}

extern "C" const PcRegionLookRow* pc_region_look_row(int i) {
    ensure();
    return i >= 0 && (size_t)i < s_rows.size() ? &s_rows[(size_t)i].view : nullptr;
}

extern "C" int pc_region_look_how(char region, const char* entry) {
    ensure();
    if (s_off || region != 'J' || entry == nullptr) return PC_RL_NONE;
    for (const Row& r : s_rows) {
        if (r.f[1] != "picture" || r.f[2] != entry) continue;
        return r.f[7] == "sprite" ? PC_RL_SPRITE : r.f[7] == "entry" ? PC_RL_ENTRY : PC_RL_NONE;
    }
    return PC_RL_NONE;
}

extern "C" void pc_region_look_note_found(char region, const char* archive, const char* entry) {
    if (region != 'J' || archive == nullptr || entry == nullptr) return;
    ensure();
    for (const Row& r : s_rows) {
        if (r.f[2] != entry) continue;
        const std::lock_guard lock(s_mutex);
        s_found.emplace(entry, archive);
        return;
    }
}

extern "C" void pc_region_look_reset(void) {
    const std::lock_guard lock(s_mutex);
    s_found.clear();
}

/* Step 2's check (the plan's order of work): every row's entry is on the player's Japanese disc in every archive the
 * table names (main.dol isn't an archive: the logo's copy in the program can't be checked by name, so its row names
 * the nintendo_logo archive too, which is where OrrePort reads it from) */
extern "C" void pc_region_look_disc_done(char region) {
    if (region != 'J') return;
    ensure();
    const std::lock_guard lock(s_mutex);
    unsigned ok = 0;
    std::string missing;
    for (const Row& r : s_rows) {
        bool rowOk = true;
        for (const std::string& a : split(r.f[3], ',')) {
            if (a.empty() || a == "main.dol") continue;
            if (s_found.count({r.f[2], a}) != 0) continue;
            rowOk = false;
            missing += (missing.empty() ? "" : ", ") + r.f[2] + " in " + a;
        }
        ok += rowOk ? 1 : 0;
    }
    std::printf("[region_look] Japanese disc: %u of %zu rows' entries found where the table says%s%s\n", ok,
                s_rows.size(), missing.empty() ? "" : "; missing: ", missing.c_str());
}

extern "C" void pc_region_look_log_state(const char* lang, int haveDisc, int filesOff) {
    ensure();
    const bool ja = lang != nullptr && std::strcmp(lang, "ja") == 0;
    std::string applied, skipped;
    for (const Row& r : s_rows) {
        const bool on = !s_off && ja && haveDisc && (r.f[7] == "sprite" || (r.f[7] == "entry" && !filesOff));
        std::string& to = on ? applied : skipped;
        to += (to.empty() ? "" : ", ") + r.f[2];
        if (!on && r.f[7] == "none") to += " (not swapped yet)";
        else if (!on && r.f[7] == "entry" && filesOff && ja && haveDisc && !s_off) to += " (ORRE_NO_LANGUAGE_FILES)";
    }
    char why[96];
    if (s_off) std::snprintf(why, sizeof(why), "off (ORRE_NO_REGION_LOOK)");
    else if (!ja) std::snprintf(why, sizeof(why), "doesn't apply (the language is %s)", lang != nullptr ? lang : "?");
    else if (!haveDisc) std::snprintf(why, sizeof(why), "doesn't apply (no Japanese disc added)");
    else std::snprintf(why, sizeof(why), "applies (language ja, Japanese disc added)");
    /* when it applies, which rows are swapped and which skipped (and why); otherwise only why, and how many rows wait */
    const bool applies = !s_off && ja && haveDisc;
    std::string line = std::string("[region_look] the Japanese release look ") + why + ": ";
    if (applies)
        line += (applied.empty() ? std::string("nothing swapped") : "swapped: " + applied) +
                (skipped.empty() ? "" : "; skipped: " + skipped);
    else
        line += std::to_string(s_rows.size()) + " rows, nothing swapped";
    line += "\n";
    if (line == s_lastState) return;
    s_lastState = line;
    std::fputs(line.c_str(), stdout);
}
