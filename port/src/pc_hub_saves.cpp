/*
 * pc_hub_saves.cpp: the hub's Saves page (step 2.1; mockup 72, snatcherLeo, 2026-11-13: "Let's not have a "Found in Dolphin".
 * All we need to have that say is "Import a save" ... "Restore from previous version""; the project's reviews). The save slots
 * themselves are pc_saves.c; this file only draws and drives the page (hub.rml #saves, hub.rcss body.saves).
 *
 * What the page does, and why:
 * - Every save, one row each (its name as typed, GCI / ORRESAVE, the trainer, play time, when it was last saved), in
 *   the page's own order (a duplicate sits right under its original), with search and a sort. Rows are drawn up to
 *   kMaxRows at a time (search narrows a long list); likely enough for any player, measured later.
 * - The chosen save's details and actions: Play this save (the save the hub's Play starts; back to the hub), Rename,
 *   Duplicate, Show in folder, Delete... (asks; the Recycle Bin), Restore from previous version... (the go-back window,
 *   mockup 74: the save as it is now, then its earlier versions in plain words; one click goes back, then the notice
 *   under the list offers Undo until that save is played or restored again; snatcherLeo, 2026-11-13).
 * - Import a save...: the system's file window, opened in Dolphin's save folder when there is one, several files at
 *   once; dropping .gci / .raw files anywhere on the hub does the same. Each import becomes "Imported save", tagged
 *   NEW; the player's file is never changed. Then the import question (3.5), "How do you want to play this save?",
 *   unless it was turned off (import_ask_open says how; saves found loose in Saves\ at start get it too).
 * - Every word is a key in ui/lang/en.txt (pc_text.h). Controller: d-pad up / down choose, A plays this save, X
 *   renames, Y previous versions (Undo while it is offered), B back; LB / RB nothing here.
 *
 * Platforms: RmlUi and SDL3 (SDL_ShowOpenFileDialog, SDL_OpenURL): the same on Windows, Linux and macOS. Where
 * Dolphin keeps saves differs per system (dolphin_folder below: Windows' Documents / AppData; Linux
 * ~/.local/share/dolphin-emu; macOS ~/Library/Application Support/Dolphin: likely, from Dolphin's own docs).
 */
#include <aurora/input.hpp>
#include <aurora/rmlui.hpp>

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementUtilities.h> /* hub_fit_name's measure */
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <SDL3/SDL.h>

#include "pc_autosave.h" /* each save's autosave: its line and Play button in the details (2026-11-20) */
#include "pc_gcopy.h"
#include "pc_grapheme.h" /* hub_fit_name cuts by visible letters (2026-11-16) */
#include "pc_hub_saves.h"
#include "pc_names.h"
#include "pc_options.h" /* GameCube-accurate mode's list of what it turns off (2.12) */
#include "pc_rtl.h" /* right-to-left names drawn as read (hub_shown_name) */
#include "pc_saves.h"
#include "pc_settings.h" /* saves_ask_same: "Don't ask me again" for a save already here */
#include "pc_text.h"
#include "hub_text/pc_ui_shaping.h" /* whether the pages shape their own text (hub_shown_name; 2026-11-16) */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime> /* the autosave line's "12 min ago" */
#include <mutex>
#include <string>
#include <vector>

namespace {
namespace input = aurora::input;

constexpr int kMaxRows = 400;

Rml::ElementDocument* s_doc = nullptr;
std::function<void()> s_changed; /* the hub's card and PLAY follow the chosen save */
bool s_open = false;
std::vector<PcSaveInfo> s_all;   /* every save, in the page's order */
std::vector<int> s_shown;        /* indexes into s_all after search and sort */
int s_sel = 0;                   /* index into s_shown */
int s_sort = 0;                  /* 0 your order, 1 last played, 2 name, 3 play time */
std::string s_search;
/* GCOPY: "Make a GameCube copy…"; GCACC: accurate mode's offer of one (2.4, mockups 104 / 104b); WHATOFF: the "What it
 * turns off" list (2.12) */
/* IMPORT_ASK: "How do you want to play this save?" after an import (3.5; import_ask_open below) */
enum Dialog { NONE, RENAME, RESTORE, DELETE_ASK, ALREADY, GCOPY, GCACC, WHATOFF, IMPORT_ASK };
bool s_gc_blocked = false;          /* the copy window: a "Can't be copied" note switched the button off */
/* the copy window was opened for accurate play (from accurate mode's offer: PLAY on an accurate .orresave, or turning
 * the switch on for an .orresave): the copy is then also added to the saves with accurate mode on (2.12) */
bool s_gc_accurate = false;
std::vector<std::string> s_gc_dirs; /* the folder picked for a copy (its window's callback may run on another thread) */
Dialog s_dialog = NONE;
std::vector<PcSaveVersion> s_versions; /* only the ones shown: not identical to the save now, not Undo's copy */
int s_vsel = 0;
bool s_note_is_undo = false;      /* the notice under the list is the "Went back ... Undo" one (it follows the chosen save) */
int s_focus_frames = 0;           /* frames until a text box takes the keyboard (it can't while still hidden) */
std::string s_already_path;       /* the file waiting on "Import again as a new save?" */
bool s_noask_ticked = false;      /* that window's "Don't ask me again" box */
/* the import question (3.5): the saves it decides, the controller's place in it (0 Enhanced, 1 GameCube-accurate, 2 the
 * "Use this for every import" tick) and the tick itself; when a test switch answers it (ORRE_HUB_TEST_IMPORT_ANSWER) */
std::vector<int> s_ask_slots;
int s_ask_focus = 0;
bool s_ask_always = false;
Uint64 s_ask_test_at = 0;
std::mutex s_pick_mutex;
std::vector<std::string> s_picked; /* from the file window (its callback may run on another thread) */

std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

Rml::Element* el(const char* id) { return s_doc != nullptr ? s_doc->GetElementById(id) : nullptr; }
void set(const char* id, const std::string& rml) { if (Rml::Element* e = el(id)) e->SetInnerRML(rml); }
void body_class(const char* name, bool on) { if (Rml::Element* e = el("body")) e->SetClass(name, on); }

std::string play_time(unsigned seconds) {
    char buf[32];
    SDL_snprintf(buf, sizeof(buf), "%u:%02u", seconds / 3600, (seconds / 60) % 60);
    return buf;
}

std::string date_of(long long unix_seconds) {
    SDL_DateTime dt;
    if (unix_seconds <= 0 || !SDL_TimeToDateTime((SDL_Time)unix_seconds * 1000000000LL, &dt, true)) return "";
    /* the Saves page writes the month out (snatcherLeo, 2026-11-15: "full month name in saves screen, and shortened version in
     * the hub main homepage") */
    return tx_date(dt.year, dt.month, dt.day, dt.hour, dt.minute, true);
}

/* "Today, 5:20 AM" / "Yesterday, 10:10 PM" / "November 12, 2026, 2:36 AM" (the go-back window: snatcherLeo, 2026-11-13, "EASY
 * TO READ"). Today and yesterday by the computer's own clock and time zone; the time 12- or 24-hour by its setting. */
std::string friendly_date(int y, int mo, int d, int h, int mi) {
    SDL_Time now;
    SDL_DateTime t;
    const std::string time = tx_time(h, mi);
    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &t, true)) {
        if (t.year == y && t.month == mo && t.day == d) return tx("saves.today", {{"time", time}});
        SDL_DateTime yd;
        if (SDL_TimeToDateTime(now - 86400LL * 1000000000LL, &yd, true) && yd.year == y && yd.month == mo && yd.day == d) {
            return tx("saves.yesterday", {{"time", time}});
        }
    }
    return tx_date(y, mo, d, h, mi, true);
}

std::string friendly_of(long long unix_seconds) {
    SDL_DateTime dt;
    if (unix_seconds <= 0 || !SDL_TimeToDateTime((SDL_Time)unix_seconds * 1000000000LL, &dt, true)) return "";
    return friendly_date(dt.year, dt.month, dt.day, dt.hour, dt.minute);
}

std::string friendly_of_stamp(const char* stamp) { /* "20261113_025512" (a version folder's name) */
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (std::sscanf(stamp, "%4d%2d%2d_%2d%2d%2d", &y, &mo, &d, &h, &mi, &s) < 5) return stamp;
    return friendly_date(y, mo, d, h, mi);
}

/* "WES · 41:22 played · 12 Nov 2026, 02:36", or "Not started · New Game begins here" */
} // namespace

/* A trainer name cut with "…" when it is wider than an ordinary long name (*Changed 2026-11-15*, snatcherLeo: "WWWWWWWW" on
 * the hub's card should have the dots; a letter count let 8 W's through, which are wider than most 11-letter names):
 * measured with the page's own font on `ref` against "Christopher" (an ordinary 11-letter name, so real names stay
 * whole), letters dropped from the end until it fits with the "…". All OSes the same (RmlUi's measure). */
/* *Changed 2026-11-16 (the project's build helper; right-to-left names, pc_rtl.h):* the name is measured and returned in its
 * drawn form (hub_shown_name), and cut by typed letters: a right-to-left name loses letters from its own end, the
 * drawn left, and its "…" goes on that side, where reading ends. */
std::string hub_fit_name(Rml::Element* ref, const std::string& name) {
    if (name.empty()) return name;
    const bool fromRight = pc_rtl_base_rtl_utf8(name.c_str()) != 0;
    /* *Checked 2026-11-16 (shaping):* the same with the HarfBuzz engine: the pages' lines are left to right, so a "…"
     * typed before a right-to-left name sits on its drawn left, where reading ends (the bidi algorithm's rule N1) */
    auto drawn = [&](const std::string& typed, bool cut) {
        const std::string d = hub_shown_name(typed);
        if (!cut) return d;
        return fromRight ? "\xE2\x80\xA6" + d : d + "\xE2\x80\xA6";
    };
    if (ref == nullptr) return drawn(name, false);
    const int room = Rml::ElementUtilities::GetStringWidth(ref, "Christopher");
    if (Rml::ElementUtilities::GetStringWidth(ref, drawn(name, false)) <= room) return drawn(name, false);
    std::string cut = name;
    while (!cut.empty()) {
        /* drop the last letter typed, whole. *Changed 2026-11-16 (visible letters, pc_grapheme.h; snatcherLeo: the 12 letters
         * are visible letters):* a visible letter, not one character: before, the cut could leave half an emoji family
         * (👨‍👩‍ and a joiner), a skin tone without its hand, or a Hindi consonant without its vowel sign before the "…" */
        cut.resize(pc_grapheme_utf8_last(cut.data(), cut.size()));
        if (Rml::ElementUtilities::GetStringWidth(ref, drawn(cut, true)) <= room) break;
    }
    return drawn(cut, true);
}

/* *Added 2026-11-21 (snatcherLeo saw an 80-letter save name run across the PLAY button and over "PLAY"):* a save name cut
 * with "…" to a room in pixels, measured in ref's own font, as hub_fit_name cuts a trainer name (whole visible letters,
 * the mark on the drawn end for a right-to-left name). wrap makes the printed text from the (cut) name, so a name
 * inside a sentence ("<name> (autosave)") is cut so the whole sentence fits. Returns the text, page-ready (escaped).
 * All platforms (RmlUi's own measure) */
std::string hub_fit_name_px(Rml::Element* ref, const std::string& name, int room,
                            const std::function<std::string(const std::string&)>& wrap) {
    const bool fromRight = pc_rtl_base_rtl_utf8(name.c_str()) != 0;
    auto drawn = [&](const std::string& typed, bool cut) {
        const std::string d = hub_shown_name(typed);
        return cut ? (fromRight ? "\xE2\x80\xA6" + d : d + "\xE2\x80\xA6") : d;
    };
    auto fits = [&](const std::string& text) { return Rml::ElementUtilities::GetStringWidth(ref, text) <= room; };
    if (ref == nullptr || room <= 0 || fits(wrap(drawn(name, false)))) return esc(wrap(drawn(name, false)));
    std::string cut = name;
    while (!cut.empty()) {
        cut.resize(pc_grapheme_utf8_last(cut.data(), cut.size()));
        /* no space left before the "…" (the first picture showed "Wes and Rui …") */
        while (!cut.empty() && cut.back() == ' ') cut.pop_back();
        if (fits(wrap(drawn(cut, true)))) break;
    }
    return esc(wrap(drawn(cut, true)));
}

/* A name as the page draws it (*added 2026-11-16*, right-to-left names): RmlUi lays letters out left to right with no
 * shaping, like the game's text, so an Arabic or Hebrew name goes through the same pass as in the game (pc_rtl.h:
 * Arabic's joined forms, the right-to-left runs reversed), so the hub and the game draw a name alike. A mark with no
 * spacing form stays after its letter (PC_RTL_MARKS_AFTER_BASE): RmlUi draws a zero-width mark over the letter before
 * it. Other names come back unchanged. Draw time only: the save's files keep the name as typed. All OSes the same. */
/* *Changed 2026-11-16 (HarfBuzz and SheenBidi in the hub, hub_text/pc_ui_shaping.cpp):* when the pages' font engine
 * shapes text itself (the default), the name goes to the page as typed: the engine joins Arabic, places Indic and Thai
 * marks, joins emoji sequences and orders right-to-left runs, so running pc_rtl's pass first would reverse it twice. The
 * pass below is kept for ORRE_UI_SHAPING=0 (RmlUi's own engine, no shaping). */
std::string hub_shown_name(const std::string& typed) {
    if (typed.empty()) return typed;
    /* The name inside first-strong isolate marks (U+2068 ... U+2069; HTML's <bdi>): the bidi algorithm then orders it
     * as one block in the line, whatever its letters. Without them (seen 2026-11-16) the Saves row "محمد سلام · 14:46
     * played" drew as "14:46 · محمد سلام played": the time after an Arabic name counts as an Arabic number (rule W2),
     * so it joined the name's right-to-left run. HarfBuzz hides the two marks (they are default-ignorable). */
    if (pc_ui_text_shaped()) return "\xE2\x81\xA8" + typed + "\xE2\x81\xA9";
    std::string out(typed.size() * 2 + 16, '\0'); /* a joined form is at most 3 bytes for a 2-byte letter */
    const size_t n = pc_rtl_display_utf8(typed.c_str(), out.data(), out.size(), PC_RTL_MARKS_AFTER_BASE);
    out.resize(n);
    return out;
}

/* ---- The coloured region word (Phase 3 step 3.5 step 6, 2026-11-19) ----
 * What snatcherLeo decided (2026-11-17 / 20; the project plan "A region badge on every save", proposal B, mockup 22): every save
 * shows where it comes from as a coloured word leading the line under its name: US blue, PAL green, JPN raspberry red,
 * the word always written so colour is never the only cue. The Saves page writes a European save's language out ("PAL ·
 * Spanish"), the hub's card, whose line is tight, the short code ("PAL · ES"). An empty slot shows none. The word comes
 * from the save's own stamp (PcSaveInfo.region / language, pc_saves.c), so an imported European save stays "PAL" after
 * it is played here, and a save started in OrrePort says "US" (it is a US save underneath).
 * How: the line is one whole translatable sentence with {region} (saves.row.region), never glued from pieces; the word
 * goes in as a marker that survives escaping and is then swapped for its coloured span (hub.rcss .rword: .rus / .rpal /
 * .rjpn, dark and light). Stable ids for the UI Editor: saves.row.region (the Saves rows), saves.details.region (the
 * details panel's row, element sv-region), hub.card.region (the hub's card, element curregion). RmlUi only: the same on
 * every OS. */
namespace {
const char kRegionMark[] = "\x01\x02"; /* never in a typed name or a translation: control characters */

const char* region_class(int region) { return region == 1 ? "rjpn" : region == 2 ? "rus" : "rpal"; }

/* the word itself, in the hub's language: "US", "JPN", "PAL", "PAL · Spanish" (full) / "PAL · ES"; "" when unknown */
std::string region_word(const PcSaveInfo& s, bool full) {
    const char* key = s.region == 1 ? "saves.region.jpn" : s.region == 2 ? "saves.region.us"
                    : s.region == 3 ? "saves.region.pal" : nullptr;
    if (key == nullptr) return "";
    std::string word = tx(key);
    /* a European disc has five languages (English 2, German 3, French 4, Italian 5, Spanish 6; pc_saves.h), so its
     * saves say which; an unknown number shows the region alone */
    if (s.region == 3 && s.language >= 2 && s.language <= 6) {
        const std::string lang = std::string(full ? "saves.language." : "saves.language.short.") + std::to_string(s.language);
        word = tx("saves.region.with_language", {{"region", word}, {"language", tx(lang.c_str())}});
    }
    return word;
}
} // namespace

std::string hub_region_rml(const PcSaveInfo& s, bool full, const char* id) {
    const std::string word = region_word(s, full);
    if (word.empty()) return "";
    return std::string("<span class='rword ") + region_class(s.region) + "'" +
           (id != nullptr ? std::string(" id='") + id + "'" : std::string()) + ">" + esc(word) + "</span>";
}

std::string hub_save_line_rml(const PcSaveInfo& s, const std::string& trainer, const std::string& time,
                              const std::string& date, bool full, const char* id) {
    const std::string word = hub_region_rml(s, full, id);
    if (word.empty()) return esc(tx("saves.row", {{"trainer", trainer}, {"time", time}, {"date", date}}));
    std::string line = esc(tx("saves.row.region", {{"region", kRegionMark}, {"trainer", trainer}, {"time", time},
                                                   {"date", date}}));
    const size_t at = line.find(kRegionMark);
    if (at != std::string::npos) line.replace(at, sizeof(kRegionMark) - 1, word);
    return line;
}

namespace {

/* the row's line under the save's name, as RML (the region word is a coloured span: 3.5, above) */
std::string facts_line(const PcSaveInfo& s) {
    if (!s.has_save) return esc(tx("saves.not_started"));
    /* the trainer cut with "…" when wider than an ordinary long name (snatcherLeo, 2026-11-15); the details panel shows it
     * whole */
    return hub_save_line_rml(s, hub_fit_name(el("svlist"), s.trainer), play_time(s.play_seconds), date_of(s.saved_time),
                             true, nullptr);
}

const PcSaveInfo* selected() {
    if (s_shown.empty() || s_sel < 0 || s_sel >= (int)s_shown.size()) return nullptr;
    return &s_all[(size_t)s_shown[(size_t)s_sel]];
}

void reload(int keep_slot) {
    std::vector<PcSaveInfo> all(4096);
    const int n = pc_saves_list(all.data(), (int)all.size());
    all.resize((size_t)std::max(n, 0));
    s_all.swap(all);
    s_shown.clear();
    std::string needle = s_search;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return (char)SDL_tolower(c); });
    for (size_t i = 0; i < s_all.size(); ++i) {
        std::string hay = std::string(s_all[i].name) + " " + s_all[i].trainer;
        std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return (char)SDL_tolower(c); });
        if (needle.empty() || hay.find(needle) != std::string::npos) s_shown.push_back((int)i);
    }
    std::stable_sort(s_shown.begin(), s_shown.end(), [](int a, int b) {
        const PcSaveInfo& x = s_all[(size_t)a];
        const PcSaveInfo& y = s_all[(size_t)b];
        switch (s_sort) {
        case 1: return x.saved_time > y.saved_time;
        case 2: return SDL_strcasecmp(x.name, y.name) < 0;
        case 3: return x.play_seconds > y.play_seconds;
        default: return false; /* your order: pc_saves_list's */
        }
    });
    s_sel = 0;
    for (size_t i = 0; i < s_shown.size(); ++i) {
        if (s_all[(size_t)s_shown[i]].slot == keep_slot) s_sel = (int)i;
    }
}

/* *Added 2026-11-21:* whether the save's last save was made through PC MENU: the slot's spot line (slot.ini "spot=",
 * written by pc_field_menu.cpp) starts with the SHA-1 of the slot's current .gci, the same test Continue makes. Only
 * such a save continues from somewhere other than its last PC, and only in the normal mode. The answer is kept per
 * slot, spot line and file time, so a redraw doesn't hash the save again. All platforms (SDL file reads) */
bool has_pc_menu_spot(const PcSaveInfo& s) {
    static int cachedSlot = -1;
    static long long cachedTime = -1;
    static std::string cachedLine;
    static bool cachedAnswer = false;
    char line[200] = {0};
    if (!s.has_save || !pc_saves_spot_get(s.slot, line, sizeof(line)) || line[0] == '\0') return false;
    if (s.slot == cachedSlot && s.saved_time == cachedTime && cachedLine == line) return cachedAnswer;
    char want[41] = {0}, sha[41] = {0};
    const bool answer = std::sscanf(line, "%40s", want) == 1 && pc_saves_save_digest(s.slot, sha) &&
                        std::strcmp(sha, want) == 0;
    cachedSlot = s.slot, cachedTime = s.saved_time, cachedLine = line, cachedAnswer = answer;
    return answer;
}

void draw_detail() {
    const PcSaveInfo* s = selected();
    if (s == nullptr) {
        set("svdetail", "<div class='svempty'>" + esc(tx("saves.empty")) + "</div>");
        return;
    }
    /* sgci / sorre, not the hub card's gci / orre: those show only the current save's kind (hub.rcss body.orresave) */
    const char* fmt = s->is_orresave ? "sorre" : "sgci";
    /* *Changed 2026-11-21 (snatcherLeo chose A: "the badges go on their own line under the save's name"):* the name has the
     * header's full width, and the badges (the format, GameCube-accurate, and any later ones) sit on a line of their
     * own under it (.svdtags), so a long name can't push them out of the panel (a 29-letter name ran the badge past
     * its edge once it said "GameCube-accurate") */
    std::string h = std::string("<div class='svdh'><img class='ic ") + fmt + "' src='hubicon://" +
                    (s->is_orresave ? "orresave" : "gci") + "'/><div class='svdhl'><span class='svdn'>" + esc(s->name) +
                    "</span><div class='svdtags'><span class='ftag " + fmt + "'>" + (s->is_orresave ? "ORRESAVE" : "GCI") +
                    "</span>" +
                    (s->accurate ? "<span class='ftag acc'>" + esc(tx("saves.badge.accurate")) + "</span>" : std::string()) +
                    "</div></div></div>";
    h += "<div class='svfacts'>";
    if (s->has_save) {
        h += "<div><span class='fk'>" + esc(tx("saves.trainer")) + "</span>" + esc(hub_shown_name(s->trainer)) + "</div>";
        h += "<div><span class='fk'>" + esc(tx("saves.play_time")) + "</span>" + play_time(s->play_seconds) + "</div>";
        h += "<div><span class='fk'>" + esc(tx("saves.last_saved")) + "</span>" + esc(date_of(s->saved_time)) + "</div>";
        /* the region row (UI id saves.details.region; 3.5 step 6): the coloured word written out ("PAL · Spanish"). A save
         * made here says why it is US (snatcherLeo, 2026-11-17, "k": "Region: US · started in OrrePort (OrrePort plays the US
         * version of the game)"), only while its stamp really says US: a European .gci put into Slot 1 by hand is
         * "made" by its setup file but not a US save */
        const std::string word = hub_region_rml(*s, true, "sv-region");
        if (!word.empty()) {
            std::string value = word;
            if (s->region == 2 && std::strcmp(s->came_from[0] != '\0' ? s->came_from : "made", "made") == 0) {
                value = esc(tx("saves.region.made", {{"region", kRegionMark}}));
                const size_t at = value.find(kRegionMark);
                if (at != std::string::npos) value.replace(at, sizeof(kRegionMark) - 1, word);
            }
            h += "<div><span class='fk'>" + esc(tx("saves.region")) + "</span>" + value + "</div>";
        }
    } else {
        h += "<div>" + esc(tx("saves.not_started")) + "</div>";
    }
    const std::string from = std::string("saves.came_from.") + (s->came_from[0] != '\0' ? s->came_from : "made");
    h += "<div><span class='fk'>" + esc(tx("saves.came_from")) + "</span>" + esc(tx(from.c_str())) + "</div></div>";
    /* GameCube-accurate mode for this save (UI id saves.details.accurate; 2.12, snatcherLeo 2026-11-16): a switch with a line
     * saying what it means for this save, and the "What it turns off" link (UI id accurate.what_off). The hub only runs
     * while the game is closed (Quit to hub starts OrrePort again), so the switch can only change between games. On an
     * .orresave the switch stays off and turning it on offers a GameCube copy (pc_saves_accurate_set refuses) */
    const char* acc_sub = s->accurate ? "saves.details.accurate.on" : s->is_orresave ? "saves.details.accurate.orresave"
                                                                                     : "saves.details.accurate.off";
    /* *Added 2026-11-21 (snatcherLeo, "Yes, add both"; UI id saves.details.pc_menu_spot):* a save last made through PC MENU
     * continues from that spot only in the normal mode; in accurate mode (and on a GameCube) it goes on from the last
     * PC, the save's own return point. snatcherLeo met that unexplained (Slot 17 beside Vander: Continue went to the Pokémon
     * Center), so the switch's box says it, in both states, while the slot's spot belongs to its current .gci */
    const bool spot = has_pc_menu_spot(*s);
    h += "<div class='svacc'><span class='svaccl'><span class='svaccn'>" + esc(tx("saves.details.accurate")) +
         "</span><span class='svaccs'>" + esc(tx(acc_sub)) + "</span>" +
         (spot ? "<span class='svaccs svspot' id='sv-accspot'>" + esc(tx("saves.pc_menu_spot")) + "</span>" : std::string()) +
         "<span class='svlink' id='sv-accwhat'>" +
         esc(tx("accurate.what_off.link")) + " ›</span></span><span class='svtog" + (s->accurate ? " on" : "") +
         "' id='sv-acc'><span class='tring'></span><span class='knob'><span class='ck'>&#10003;</span></span></span></div>";
    /* *Changed 2026-11-16 (snatcherLeo's polish list, mockup item 2):* the switch holds the controller's ring (.tring, shown
     * with .kfocus) and the knob's check mark, as Settings' accurate switch (hub.rcss .svtog) */
    h += "<div class='svacts'><span class='btn svgo' id='sv-play'>▶ " + esc(tx("saves.play_this")) + "</span>" +
         "<span class='btn' id='sv-rename'>" + esc(tx("saves.rename")) + "</span>" +
         "<span class='btn' id='sv-dup'>" + esc(tx("saves.duplicate")) + "</span>" +
         "<span class='btn' id='sv-folder'>" + esc(tx("saves.show_folder")) + "</span>" +
         "<span class='btn svdanger' id='sv-delete'>" + esc(tx("saves.delete")) + "</span></div>";
    /* the save's autosave (2026-11-20; docs/port/autosave.md section 3: "its own entry under the slot ... in the hub's
     * save list, for example "Autosave · Phenac City · 12 min ago""): a line with where and when, and a button that
     * plays it (UI ids saves.autosave.line, sv-playauto). The place is the one the game named when it wrote the autosave
     * (autosave.ini: the hub has no game text loaded). Not for a save played in GameCube-accurate mode, which never
     * autosaves */
    static PcAutosaveInfo autosave; /* about 2 KB: off the stack */
    if (s->has_save && !s->accurate && pc_autosave_info(s->slot, &autosave)) {
        const long long now = (long long)std::time(nullptr);
        const long long ago = now > autosave.unix_time ? now - autosave.unix_time : 0;
        const std::string age = ago < 60       ? tx("saves.autosave.ago.now")
                                : ago < 3600   ? tx_count("saves.autosave.ago.minutes", (long)(ago / 60))
                                : ago < 86400  ? tx_count("saves.autosave.ago.hours", (long)(ago / 3600))
                                               : tx_count("saves.autosave.ago.days", (long)(ago / 86400));
        const std::string line = autosave.place[0] != '\0'
                                     ? tx("saves.autosave.line", {{"place", autosave.place}, {"ago", age}})
                                     : tx("saves.autosave.line_noplace", {{"ago", age}});
        h += "<div class='svhr'></div><div class='svacts'><span class='btn' id='sv-playauto'>▶ " +
             esc(tx("saves.autosave.play")) + "</span></div><div class='svsmall'>" + esc(line) + "</div>";
    }
    h += "<div class='svhr'></div><span class='btn' id='sv-restore'>↺ " + esc(tx("saves.restore")) + "</span>" +
         "<div class='svsmall'>" + esc(tx("saves.versions_note")) + "</div>";
    /* 2.4: the GameCube copy, for every save with a game in it (snatcherLeo, 2026-11-14: "we could keep that box for all").
     * The details stay minimal: the save's long names aren't listed here (snatcherLeo: "we don't need to display EVERYTING
     * ... in the hub area"); the ORRESAVE tag says it keeps some, and the copy window lists each one where it matters.
     * A button in the page's button row, like Rename and Duplicate (a lone .btn lost the row's look) */
    if (s->has_save) {
        /* a save with long names can also travel as one .orresave file (2.4 part 5; the project's (c), approved): beside the copy */
        h += "<div class='svhr'></div><div class='svacts'><span class='btn' id='sv-gcopy'>" + esc(tx("saves.gcopy")) +
             "</span>" + (s->is_orresave ? "<span class='btn' id='sv-orre'>" + esc(tx("saves.export_orresave")) + "</span>"
                                         : std::string()) +
             "</div><div class='svsmall'>" + esc(tx("saves.gcopy_note")) + "</div>";
    }
    set("svdetail", h);
    /* *Added 2026-11-21:* the header's name cut with "…" to the header's width (its badges have their own line since
     * the same day), measured once laid out (hub_fit_name_px) */
    s_doc->UpdateDocument();
    if (Rml::Element* box = el("svdetail")) {
        Rml::ElementList names;
        box->GetElementsByClassName(names, "svdn");
        if (!names.empty() && names[0]->GetParentNode() != nullptr) {
            const int room = (int)names[0]->GetParentNode()->GetClientWidth();
            if (room > 0)
                names[0]->SetInnerRML(hub_fit_name_px(names[0], s->name, room, [](const std::string& n) { return n; }));
        }
    }
}

/* "Make a GameCube copy…" (mockup 104b): the window is built from the checklist's notes (pc_gcopy.c), one section per
 * kind in a fixed order, empty sections hidden, a section folding after 5 rows. A new feature adds a rule and its
 * notes; nothing here changes for it (snatcherLeo: "we'll have this going forward and add to it") */
void open_dialog(Dialog d);
void draw();
void notice(const std::string& text);
void toast(const std::string& text, bool ok);
std::string file_of(const std::string& path);

std::string gcopy_row(const PcGcopyNote& n) {
    std::string what = n.what;
    if (what.empty() && n.edit_id[0] != '\0') { /* no species given: label it by which name it is */
        const std::string id = n.edit_id;
        if (id == "hero") what = tx("saves.names.trainer");
        else if (id == "partner") what = tx("saves.names.partner");
        else if (id.rfind("box", 0) == 0) what = tx("saves.names.box", {{"n", id.substr(3)}});
        else what = tx("saves.names.pokemon"); /* a nickname saved before its species was recorded */
    }
    /* the name kept in OrrePort in its drawn form (an Arabic or Hebrew name joined and reordered; 2026-11-16) */
    const std::string text = tx(n.text_key, {{"kept", hub_shown_name(n.kept)}, {"copy", n.copy}});
    return "<div class='gcr'><span class='gcw'>" + esc(what) + "</span><span class='gct'>" + esc(text) + "</span></div>";
}

/* Which version the copy is for (3.5 step 9; snatcherLeo's calls 1-3 of 2026-11-19): a row of three region words (US, PAL,
 * JPN), and for PAL a row of its five languages. Each piece has a stable id for the UI Editor later ("svg-r-E" / "-P" /
 * "-J", "svg-l-2" .. "svg-l-6"), its words are keys (saves.region.*, saves.language.*, gcopy.pick.*), its look is in
 * hub.rcss (.gcpick, .gcreg). The mouse clicks a word; the controller's Left / Right change the region and Up / Down
 * the language (hub_saves_event). Defaults: the save's own region, the play language when European, else English
 * (pc_gcopy_default_target). Every OS the same (RmlUi). */
PcGcopyTarget s_gc_target = {'E', 2};

std::string gcopy_picker() {
    static const char kRegions[3] = {'E', 'P', 'J'};
    static const char* const kWords[3] = {"saves.region.us", "saves.region.pal", "saves.region.jpn"};
    static const char* const kColours[3] = {"rus", "rpal", "rjpn"};
    std::string h = "<div class='gcpick'><span class='gcpl'>" + esc(tx("gcopy.pick.region")) + "</span>";
    for (int i = 0; i < 3; ++i)
        h += std::string("<span class='gcreg ") + kColours[i] + (s_gc_target.region == kRegions[i] ? " on" : "") +
             "' id='svg-r-" + kRegions[i] + "'>" + esc(tx(kWords[i])) + "</span>";
    h += "</div>";
    if (s_gc_target.region == 'P') {
        h += "<div class='gcpick'><span class='gcpl'>" + esc(tx("gcopy.pick.language")) + "</span>";
        for (int l = 2; l <= 6; ++l) {
            const std::string key = "saves.language." + std::to_string(l);
            h += std::string("<span class='gcreg") + (s_gc_target.language == l ? " on" : "") + "' id='svg-l-" +
                 std::to_string(l) + "'>" + esc(tx(key.c_str())) + "</span>";
        }
        h += "</div>";
    }
    return h;
}

void gcopy_render();

/* accurate: opened from accurate mode's offer (2.12), so the copy is also added as a save that plays accurate */
void gcopy_start(bool accurate) {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    s_gc_accurate = accurate;
    pc_gcopy_default_target(s->slot, &s_gc_target);
    gcopy_render();
}

/* the controller in the copy window: Left / Right step through the regions, Up / Down through PAL's languages */
void gcopy_step(int dRegion, int dLanguage) {
    static const char kRegions[3] = {'E', 'P', 'J'};
    if (dRegion != 0) {
        int i = 0;
        while (i < 3 && kRegions[i] != s_gc_target.region) ++i;
        s_gc_target.region = kRegions[(i + dRegion + 3) % 3];
    }
    if (dLanguage != 0 && s_gc_target.region == 'P') s_gc_target.language = std::clamp(s_gc_target.language + dLanguage, 2, 6);
    gcopy_render();
}

/* the window's contents for the chosen target: the pickers, then the checklist's notes for that copy */
void gcopy_render() {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    const bool accurate = s_gc_accurate;
    std::vector<PcGcopyNote> notes(PC_GCOPY_MAX_NOTES);
    const int n = pc_gcopy_check(s->slot, &s_gc_target, notes.data(), (int)notes.size());
    set("svgcregion", gcopy_picker());
    static const char* const kSection[4] = {"changes", "stays", "off", "blocks"};
    static const char* const kNote[4] = {nullptr, "gcopy.section.stays_note", "gcopy.section.off_note", nullptr};
    std::string list;
    int differs = 0;
    s_gc_blocked = false;
    for (int kind = 0; kind < 4; ++kind) {
        std::vector<const PcGcopyNote*> rows;
        for (int i = 0; i < n; ++i)
            if (notes[(size_t)i].kind == kind) rows.push_back(&notes[(size_t)i]);
        if (rows.empty()) continue;
        if (kind == PC_GCOPY_BLOCKS) s_gc_blocked = true;
        else differs += (int)rows.size();
        list += std::string("<div class='gch ") + kSection[kind] + "'>" +
                esc(tx((std::string("gcopy.section.") + kSection[kind]).c_str())) + " <span>(" +
                std::to_string(rows.size()) + ")</span></div>";
        if (kNote[kind] != nullptr) list += "<div class='gcn'>" + esc(tx(kNote[kind])) + "</div>";
        for (size_t r = 0; r < rows.size() && r < 5; ++r) list += gcopy_row(*rows[r]);
        if (rows.size() > 5) list += "<div class='gcmore'>" + esc(tx("gcopy.more", {{"count", std::to_string(rows.size() - 5)}})) + "</div>";
    }
    set("svgct", esc(tx("gcopy.title", {{"name", s->name}})));
    set("svgcsum", esc(s_gc_blocked ? tx("gcopy.blocked")
                       : differs == 0 ? tx("gcopy.ready")
                       : differs == 1 ? tx("gcopy.summary.one")
                                      : tx("gcopy.summary.other", {{"count", std::to_string(differs)}})) +
                       /* said before the click: where the copy for accurate play goes (2.12) */
                       (accurate && !s_gc_blocked ? "<div class='gcacc'>" + esc(tx("gcopy.accurate_adds")) + "</div>"
                                                  : std::string()) +
                       /* *added 2026-11-21:* a copy of a PC MENU save goes on from the last PC (on a GameCube, in
                        * Dolphin and in accurate mode), not from the PC MENU spot (draw_detail's note says why) */
                       (has_pc_menu_spot(*s) ? "<div class='gcacc' id='svgc-spot'>" + esc(tx("saves.pc_menu_spot")) + "</div>"
                                             : std::string()));
    set("svgclist", list);
    if (Rml::Element* ok = el("svg-ok")) ok->SetClass("off", s_gc_blocked);
    if (s_dialog != GCOPY) open_dialog(GCOPY);
}

void SDLCALL gcopy_folder_picked(void*, const char* const* dirs, int) {
    if (dirs == nullptr || dirs[0] == nullptr) return;
    std::lock_guard<std::mutex> lock(s_pick_mutex);
    s_gc_dirs.push_back(dirs[0]);
}

/* the copy window's button: the system's Save As window, opened in Documents with the copy's name filled in
 * ("Save 1 - Colosseum.gci"), so the player keeps or types a name and picks the place in one step (snatcherLeo, 2026-11-15:
 * a folder picker "doesnt let me set a name"); then pc_gcopy_make_at in hub_saves_frame. SDL3's save window is the
 * system's own on Windows, macOS and Linux (likely the same; the portal on Linux) */
void gcopy_ok() {
    if (s_gc_blocked) return;
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    static const SDL_DialogFileFilter filters[] = {{"GameCube save (.gci)", "gci"}};
    char name[400];
    pc_gcopy_file_name(s->slot, &s_gc_target, name, sizeof(name)); /* with the region for a PAL or JPN copy (3.5) */
    std::string where;
    if (const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)) where = std::string(docs) + name;
    else where = name;
    SDL_ShowSaveFileDialog(gcopy_folder_picked, nullptr, SDL_GetKeyboardFocus(), filters, 1, where.c_str());
}

/* "Export to ORRESAVE…": the same Save As window, "<name> - Colosseum.orresave" filled in; pc_saves_export_orresave
 * in hub_saves_frame, then the bubble */
std::vector<std::string> s_orre_paths;
void SDLCALL orre_file_picked(void*, const char* const* files, int) {
    if (files == nullptr || files[0] == nullptr) return;
    std::lock_guard<std::mutex> lock(s_pick_mutex);
    s_orre_paths.push_back(files[0]);
}
void orre_export() {
    const PcSaveInfo* s = selected();
    if (s == nullptr || !s->is_orresave) return;
    static const SDL_DialogFileFilter filters[] = {{"OrrePort save (.orresave)", "orresave"}};
    char name[400];
    pc_saves_file_name(s->name, s->slot, name, sizeof(name)); /* "<name> - Colosseum.gci" */
    std::string file = name;
    if (file.size() > 4) file = file.substr(0, file.size() - 4) + ".orresave";
    const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    const std::string where = docs != nullptr ? std::string(docs) + file : file;
    SDL_ShowSaveFileDialog(orre_file_picked, nullptr, SDL_GetKeyboardFocus(), filters, 1, where.c_str());
}
void orre_write(const std::string& file) {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    char out[1600];
    const bool ok = pc_saves_export_orresave(s->slot, file.c_str(), out, sizeof(out)) != 0;
    toast(ok ? tx("saves.exported_orresave", {{"file", file_of(out)}}) : tx("saves.err.export"), ok);
}

void gcopy_write(const std::string& file) {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    char out[1500];
    const bool ok = pc_gcopy_make_at(s->slot, &s_gc_target, file.c_str(), out, sizeof(out)) != 0;
    const bool accurate = s_gc_accurate;
    s_gc_accurate = false;
    open_dialog(NONE);
    /* a copy made for accurate play (2.12; the GameCube copy's checklist, rule "accurate"): the same .gci added as a new
     * save under the original, accurate mode on, and made the save PLAY starts, so the player can play it at once; the
     * file written above stays where the player put it, for a GameCube or Dolphin */
    if (ok && accurate) {
        const int from = s->slot;
        const std::string name = tx("saves.gcopy_name", {{"name", s->name}});
        const int added = pc_saves_add_gcopy(from, out, name.c_str(), 1);
        if (added > 0) {
            pc_saves_set_current(added);
            reload(added);
            draw();
            if (s_changed) s_changed();
            toast(tx("gcopy.done_accurate", {{"file", file_of(out)}, {"name", name}}), true);
            return;
        }
        draw();
        toast(tx("gcopy.done", {{"file", file_of(out)}}) + " " + tx("saves.accurate.add_failed"), false);
        return;
    }
    draw();
    toast(ok ? tx("gcopy.done", {{"file", file_of(out)}}) : tx("gcopy.err"), ok);
}

/* ---- GameCube-accurate mode on the Saves page (2.12; snatcherLeo, 2026-11-16) ---- */
/* the switch: a .gci just switches (on or off); turning it on for an .orresave opens accurate mode's offer of a GameCube
 * copy instead (the save itself never changes), the same window PLAY opens for such a save */
void accurate_toggle() {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    const int slot = s->slot;
    const bool on = s->accurate == 0;
    const std::string name = s->name;
    const int r = pc_saves_accurate_set(slot, on ? 1 : 0);
    if (r == PC_SAVES_ACCURATE_NEEDS_COPY) {
        set("svgcacct", esc(tx("saves.accurate.orre_title")));
        set("svgcaccs", esc(tx("saves.accurate.orre_body", {{"name", name}})));
        open_dialog(GCACC);
        return;
    }
    reload(slot);
    draw();
    if (r != 1) {
        toast(tx("saves.accurate.err"), false);
        return;
    }
    if (s_changed) s_changed(); /* the hub's card shows the badge (or not) at once */
    toast(tx(on ? "saves.accurate.on_done" : "saves.accurate.off_done", {{"name", name}}), true);
}

}  // namespace

/* the "What it turns off" list as RML (2.12): pc_options.c's rows by kind, each heading then its rows; the words are
 * ui/lang keys, so it reads in every language. Shared by the Saves page and Settings › General (pc_settings_overlay.cpp
 * calls this too); the classes .awh / .awr are styled in hub.rcss and settings.rcss. Text without < or & (the language
 * files' rule), so it goes in as written. */
std::string hub_accurate_list_rml() {
    static const char* const kHeads[3] = {"accurate.what_off.head_off", "accurate.what_off.head_gc",
                                          "accurate.what_off.head_same"};
    std::string out = "<div class='awi'>" + tx("accurate.what_off.intro") + "</div>";
    for (int kind = 0; kind < 3; ++kind) {
        std::string rows;
        for (int i = 0; i < pc_accurate_list_count(); ++i) {
            const PcAccurateRow* r = pc_accurate_list_at(i);
            if (r != nullptr && r->kind == kind) rows += "<div class='awr'>" + tx(r->key) + "</div>";
        }
        if (!rows.empty()) out += std::string("<div class='awh k") + std::to_string(kind) + "'>" + tx(kHeads[kind]) + "</div>" + rows;
    }
    return out;
}

namespace {
void whatoff_open() {
    set("svwhatlist", hub_accurate_list_rml());
    if (Rml::Element* l = el("svwhatlist")) l->SetScrollTop(0.f);
    open_dialog(WHATOFF);
}

/* Mockup 74's Undo (snatcherLeo, 2026-11-13): while the chosen save has a restore that can be undone, the notice under the
 * list says what happened, with Undo, and Y's hint reads Undo. It is offered until the save is played or restored
 * again (pc_saves.c keeps it in slot.ini), so it is back whenever that save is chosen. */
bool undo_offered(char* to, size_t size, unsigned* secs) {
    const PcSaveInfo* s = selected();
    return s != nullptr && pc_saves_undo_info(s->slot, to, size, secs) != 0;
}

void refresh_undo_note() {
    char to[64];
    unsigned secs = 0;
    const bool undo = undo_offered(to, sizeof(to), &secs);
    set("hintsvy", esc(tx(undo ? "saves.undo" : "saves.hint.versions")));
    if (undo) {
        set("svnote", esc(tx("saves.went_back", {{"date", friendly_of_stamp(to)}, {"time", play_time(secs)}})) +
                          "<span class='btn' id='sv-undo'>" + esc(tx("saves.undo")) + " <span class='yb'>Y</span></span>");
        s_note_is_undo = true;
    } else if (s_note_is_undo) {
        set("svnote", "");
        s_note_is_undo = false;
    }
}

/* ---- the controller in the details panel (snatcherLeo, 2026-11-15: "If I'm in a save file, and I click to the right, it
 * should let me select these, and let me go down in here as well, and left to get back out") ----
 * Right from the list enters the panel's buttons; the D-pad or stick moves to the nearest button in that direction
 * (by where they sit, so a language whose words wrap the buttons differently still works); Left from the left edge,
 * or B, goes back to the list; A presses the button. The place shows as the orange edge (.kfocus, hub.rcss). */
std::string s_panel; /* the panel button with the controller's place; empty: the list has it */
const char* const kPanelIds[] = {"sv-acc",     "sv-accwhat", "sv-play",  "sv-rename", "sv-dup", "sv-folder",
                                 "sv-delete", "sv-restore", "sv-gcopy", "sv-orre", "sv-playauto"};

void panel_mark() {
    if (!s_panel.empty() && (el(s_panel.c_str()) == nullptr || !el(s_panel.c_str())->IsVisible(true))) s_panel.clear();
    for (const char* id : kPanelIds)
        if (Rml::Element* e = el(id)) e->SetClass("kfocus", s_panel == id);
}

Rml::Vector2f centre_of(Rml::Element* e) {
    return e->GetAbsoluteOffset(Rml::BoxArea::Border) + e->GetBox().GetSize(Rml::BoxArea::Border) * 0.5f;
}

/* the nearest shown button from `from` in direction (dx, dy); empty when none */
std::string panel_next(const std::string& from, int dx, int dy) {
    Rml::Element* cur = el(from.c_str());
    if (cur == nullptr) return {};
    const Rml::Vector2f c = centre_of(cur);
    std::string best;
    float bestScore = 1e30f;
    for (const char* id : kPanelIds) {
        Rml::Element* e = el(id);
        if (e == nullptr || from == id || !e->IsVisible(true)) continue;
        const Rml::Vector2f v = centre_of(e) - c;
        const float along = dx != 0 ? v.x * (float)dx : v.y * (float)dy;
        const float across = dx != 0 ? std::fabs(v.y) : std::fabs(v.x);
        if (along <= 1.f) continue;
        if (dx != 0 && across > cur->GetBox().GetSize().y) continue; /* Left / Right: the same row only */
        const float score = along + 2.f * across;
        if (score < bestScore) bestScore = score, best = id;
    }
    return best;
}

void draw() {
    if (s_doc == nullptr) return;
    std::string rows;
    const int count = std::min((int)s_shown.size(), kMaxRows);
    for (int i = 0; i < count; ++i) {
        const PcSaveInfo& s = s_all[(size_t)s_shown[(size_t)i]];
        const char* fmt = s.is_orresave ? "sorre" : "sgci";
        rows += std::string("<div class='svrow") + (i == s_sel ? " f" : "") + "' id='svr-" + std::to_string(i) + "'>" +
                "<img class='ic " + fmt + "' src='hubicon://" + (s.is_orresave ? "orresave" : "gci") + "'/>" +
                /* the name in its own span (.svnm, 2026-11-21): it gives way to the badges, so a long name is cut and
                 * the badges always show (the row's fixed 82 dp height leaves no room for a line of badges, as the
                 * details header has) */
                "<div class='svtxt'><div class='svname'><span class='svnm'>" + esc(s.name) + "</span><span class='ftag " + fmt + "'>" +
                (s.is_orresave ? "ORRESAVE" : "GCI") + "</span>" +
                /* the Accurate badge (UI id saves.badge.accurate; 2.12): this save plays in GameCube-accurate mode */
                (s.accurate ? "<span class='ftag acc'>" + esc(tx("saves.badge.accurate")) + "</span>" : std::string()) +
                (s.is_new ? " <span class='svnew'>" + esc(tx("saves.new")) + "</span>" : std::string()) + "</div>" +
                "<div class='svsub'>" + facts_line(s) + "</div></div>" + /* RML already (the region span) */
                /* on the chosen row (snatcherLeo, 2026-11-14: it "should, on the Saves screen, move with my click, so that it's
                 * very clear"): leaving the page keeps the chosen save as the one Play starts */
                (i == s_sel ? "<span class='svnext'>● " + esc(tx("saves.play_starts")) + "</span>" : std::string()) +
                "</div>";
    }
    if ((int)s_shown.size() > kMaxRows) {
        rows += "<div class='svmore'>" + esc(tx_count("saves.more", (long)s_shown.size() - kMaxRows, {})) + "</div>";
    }
    if (s_shown.empty()) rows = "<div class='svempty'>" + esc(tx(s_search.empty() ? "saves.none" : "saves.no_match")) + "</div>";
    set("svlist", rows);
    static const char* const kSorts[] = {"saves.sort.order", "saves.sort.last_played", "saves.sort.name", "saves.sort.play_time"};
    set("svsort", esc(tx(kSorts[s_sort])) + " ▾");
    draw_detail();
    /* *Fixed 2026-11-15 (snatcherLeo: "as I go down, the scrollbar doesn't actually move"):* the rows were just rebuilt, so
     * their places weren't laid out yet and the scroll asked for nothing; the page is laid out first, then the chosen
     * row scrolled only as far as it needs (the nearest edge) */
    s_doc->UpdateDocument();
    /* *Added 2026-11-21 (snatcherLeo saw an 80-letter name run over PLAY; every place a save name is printed was checked):*
     * each row's name cut with "…" to its line less the badges after it (hub_fit_name_px), now that the rows are laid
     * out; the badges keep their place (hub.rcss .svname) */
    for (int i = 0; i < count; ++i) {
        Rml::Element* row = el(("svr-" + std::to_string(i)).c_str());
        if (row == nullptr) continue;
        Rml::ElementList names;
        row->GetElementsByClassName(names, "svnm");
        if (names.empty() || names[0]->GetParentNode() == nullptr) continue;
        Rml::Element* line = names[0]->GetParentNode();
        float badges = 0.f;
        for (int c = 0; c < line->GetNumChildren(); ++c) {
            Rml::Element* b = line->GetChild(c);
            if (b != names[0]) badges += b->GetOffsetWidth() + b->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Left);
        }
        const int room = (int)(line->GetClientWidth() - badges);
        if (room > 0)
            names[0]->SetInnerRML(hub_fit_name_px(names[0], s_all[(size_t)s_shown[(size_t)i]].name, room,
                                                  [](const std::string& n) { return n; }));
    }
    s_doc->UpdateDocument();
    if (Rml::Element* row = el(("svr-" + std::to_string(s_sel)).c_str()))
        row->ScrollIntoView(Rml::ScrollIntoViewOptions{Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest});
    refresh_undo_note();
    panel_mark();
}

/* a one-off notice under the list (replaces the Undo notice until the next draw) */
void notice(const std::string& text) {
    set("svnote", esc(text));
    s_note_is_undo = false;
}

/* *Added 2026-11-21 (snatcherLeo: "This message here, we should put it elsewhere, I say, near the top ... as a kind of green
 * badge that fades away quickly, to let the user know it was successfully imported"):* a small green badge in the page's
 * header, where the drop hint is (the hint fades out for that moment: the two share the spot), shown for about two
 * seconds and then faded out by its rcss transition (UI id saves.import_badge; hub.rcss #svbadge). Used for a
 * successful import; a problem keeps the line under the list, where there's room for the reason */
Uint64 s_badge_until = 0;
void badge(const std::string& text) {
    set("svbadge", "✓ " + esc(text));
    if (Rml::Element* b = el("svbadge")) b->SetClass("on", true);
    body_class("svbadging", true);
    s_badge_until = SDL_GetTicks() + 2200;
}

/* a bubble at the top of the page for a few seconds: green when it worked, red when not (snatcherLeo, 2026-11-15: the export's
 * line at the bottom was easy to miss) */
Uint64 s_toast_until = 0;
void toast(const std::string& text, bool ok) {
    set("svtoast", (ok ? "✓ " : "✕ ") + esc(text));
    if (Rml::Element* t = el("svtoast")) {
        t->SetClass("ok", ok);
        t->SetClass("bad", !ok);
    }
    body_class("svtoasting", true);
    s_toast_until = SDL_GetTicks() + 4500;
}

std::string file_of(const std::string& path) {
    const size_t a = path.find_last_of("/\\");
    return a == std::string::npos ? path : path.substr(a + 1);
}

/* one import, with the plan's messages; returns the new slot or 0 */
bool s_tampered = false; /* the last import was an .orresave whose long names don't belong to its save */
bool s_already_tampered = false; /* the waiting question is "Import just the save?" (2.4 part 5) */

/* force: 0 ask about a save already here, 1 import it again, 2 an .orresave's save alone (its long names left out) */
int import_one(const std::string& path, int force, std::string* message) {
    int made = 0, already = 0;
    s_tampered = false;
    const int code = pc_saves_import(path.c_str(), 0, force, &made, &already);
    const std::string f = file_of(path);
    switch (code) {
    case PC_SAVE_IMPORTED: *message = tx("saves.imported_one"); return made;
    case PC_SAVE_ERR_ALREADY: {
        PcSaveInfo other;
        const bool got = pc_saves_get(already, &other) != 0;
        /* the match keeps long names (an .orresave): the file is its GameCube copy, the same game with GameCube names;
         * importing it as a new save brings the long names back (pc_saves.c copies that save's extras) */
        /* only a plain .gci is "a GameCube copy"; an .orresave matching it is simply that save again (snatcherLeo's check,
         * 2026-11-15) */
        const bool is_orre = hub_saves_is_save_file(path.c_str()) && path.size() > 9 &&
                             SDL_strcasecmp(path.c_str() + path.size() - 9, ".orresave") == 0;
        *message = tx(got && other.is_orresave && !is_orre ? "saves.already_copy" : "saves.already",
                      {{"name", got ? other.name : "?"}});
        return -already;
    }
    case PC_SAVE_ERR_XD: *message = tx("saves.err.xd"); break;
    case PC_SAVE_ERR_REGION: *message = tx("saves.err.region", {{"file", f}}); break;
    case PC_SAVE_ERR_DAMAGED: *message = tx("saves.err.damaged", {{"file", f}}); break;
    case PC_SAVE_ERR_NO_COLOSSEUM: *message = tx("saves.err.no_colosseum"); break;
    case PC_SAVE_ERR_OTHER_GAME: *message = tx("saves.err.other_game", {{"file", f}}); break;
    case PC_SAVE_ERR_WRITE: *message = tx("saves.err.write"); break;
    case PC_SAVE_ERR_TAMPERED: *message = tx("saves.err.tampered", {{"file", f}}); s_tampered = true; break;
    case PC_SAVE_ERR_NOT_SAVE: *message = tx("saves.err.not_save", {{"file", f}}); break;
    default: *message = tx("saves.err.read", {{"file", f}}); break;
    }
    return 0;
}

void open_dialog(Dialog d) {
    s_dialog = d;
    body_class("svdlg", d != NONE);
    body_class("svrename", d == RENAME);
    body_class("svrestore", d == RESTORE);
    body_class("svdelete", d == DELETE_ASK);
    body_class("svalready", d == ALREADY);
    body_class("svgcopy", d == GCOPY);
    body_class("svgcacc", d == GCACC);
    body_class("svwhatoff", d == WHATOFF);
    body_class("svimportask", d == IMPORT_ASK);
}

/* ---- The import question (3.5; snatcherLeo, 2026-11-19: "yeah, we could plan that, to ask the user - or, they can assign a
 * quick checkmark instead of always being asked"; the project plan's "How do you want to play this save?" on import) ----
 * Why: an imported GameCube save can carry on with OrrePort's improvements (Enhanced, the recommended way to make
 * OrrePort its new home) or stay exactly as on the console (GameCube-accurate). Asking at the moment of import makes
 * that choice visible instead of silently applying Settings' default. What it does:
 * - after every import that made a save accurate mode can play (a .gci or a memory card's save: not an .orresave, nor a
 *   .gci that took an .orresave's long names, which keep today's rule: normal mode, accurate needs a GameCube copy), a
 *   window over the Saves page asks once for all the saves of that import (several files at once: one question);
 * - Enhanced / GameCube-accurate sets each save's own setting (slot.ini gamecube_accurate, pc_saves_accurate_set);
 * - with "Use this for every import (don't ask again)" ticked, the answer also becomes the default for new saves
 *   (settings.ini accurate_new_saves, Settings › General's first row) and settings.ini import_ask = 0 stops the
 *   question; Settings › General's "Ask how to play imported saves" (the row under the accurate card) brings it back;
 * - B, Escape or a click outside closes it without changing anything: the saves keep the default they got at import
 *   (the new-saves default), and each save's switch in its details changes it any time.
 * UI ids (for the UI Editor): saves.import_ask (the window), saves.import_ask.enhanced, saves.import_ask.accurate,
 * saves.import_ask.always (the tick); elements svimportbox, svimp-enh, svimp-acc, svimp-noask. Controller: Up / Down
 * choose, A picks an answer or ticks the box; keyboard: arrows, Enter or Space; mouse: click an answer or the tick.
 * All platforms the same (RmlUi, settings.ini). */
bool import_ask_wanted() { return pc_settings_int("import_ask", 1) != 0; }

void import_ask_render() {
    if (Rml::Element* e = el("svimp-enh")) e->SetClass("f", s_ask_focus == 0);
    if (Rml::Element* e = el("svimp-acc")) e->SetClass("f", s_ask_focus == 1);
    if (Rml::Element* e = el("svimp-noask")) e->SetClass("f", s_ask_focus == 2);
    if (Rml::Element* box = el("svimp-box")) {
        box->SetClass("on", s_ask_always);
        box->SetInnerRML(s_ask_always ? "✓" : "");
    }
}

/* returns true when the question opened (some save to ask about, and the question isn't turned off) */
bool import_ask_open(const std::vector<int>& slots) {
    std::vector<int> ask;
    for (int slot : slots) {
        PcSaveInfo info;
        if (slot > 0 && pc_saves_get(slot, &info) && !info.is_orresave) ask.push_back(slot);
    }
    if (ask.empty()) return false;
    if (!import_ask_wanted()) {
        std::printf("[hub] import question off (settings.ini import_ask=0): %zu imported save(s) keep the new-saves default (%s)\n",
                    ask.size(), pc_option_accurate_new_saves() ? "GameCube-accurate" : "Enhanced");
        return false;
    }
    s_ask_slots = ask;
    s_ask_focus = 0; /* Enhanced, the recommended answer */
    s_ask_always = false;
    set("svimpt", esc(tx_count("saves.import_ask.title", (long)ask.size())));
    import_ask_render();
    open_dialog(IMPORT_ASK);
    std::printf("[hub] import question: how to play %zu imported save(s)?\n", ask.size());
    /* a test switch (never a setting): ORRE_HUB_TEST_IMPORT_ANSWER=enhanced | accurate, with ",always" for the tick,
     * answers 2.5 s after the window opens (hub_saves_frame), so a scripted window can check what the answer writes */
    s_ask_test_at = SDL_getenv("ORRE_HUB_TEST_IMPORT_ANSWER") != nullptr ? SDL_GetTicks() + 2500 : 0;
    return true;
}

void import_ask_answer(bool accurate) {
    int set_ok = 0;
    for (int slot : s_ask_slots)
        if (pc_saves_accurate_set(slot, accurate ? 1 : 0) == 1) ++set_ok;
    if (s_ask_always) { /* "Use this for every import": the default for new saves, and no more question */
        pc_option_set_accurate_new_saves(accurate ? 1 : 0); /* saves settings.ini (pc_options.c) */
        pc_settings_set_int("import_ask", 0);
        pc_settings_save();
    }
    std::printf("[hub] import question answered: %s for %d of %zu save(s)%s\n", accurate ? "GameCube-accurate" : "Enhanced",
                set_ok, s_ask_slots.size(), s_ask_always ? "; from now on every import plays so (import_ask=0)" : "");
    const int keep = s_ask_slots.empty() ? 0 : s_ask_slots.front();
    const bool all = set_ok == (int)s_ask_slots.size();
    s_ask_slots.clear();
    open_dialog(NONE);
    const PcSaveInfo* s = selected();
    reload(s != nullptr ? s->slot : keep);
    draw();
    std::string said = tx_count(accurate ? "saves.import_ask.done_accurate" : "saves.import_ask.done_enhanced", set_ok);
    if (s_ask_always) said += " " + tx("saves.import_ask.done_always");
    toast(all ? said : tx("saves.accurate.err"), all);
    if (s_changed) s_changed(); /* the hub's card shows the Accurate badge for the current save */
}

void import_ask_press() {
    if (s_ask_focus == 2) {
        s_ask_always = !s_ask_always;
        import_ask_render();
    } else {
        import_ask_answer(s_ask_focus == 1);
    }
}

/* Several files at once: each its own save; one plain summary. A single save already here asks "Import again?" */
void import_files(const std::vector<std::string>& paths) {
    if (paths.empty()) return;
    int made = 0, last = 0;
    std::string message, last_problem;
    /* "Don't ask me again" ticked once (snatcherLeo, 2026-11-13: the same save twice does no harm, "they'll know what they're
     * doing"; the question stays by default, as the plan decided): the same save just imports, with a note */
    const bool ask_same = pc_settings_int("saves_ask_same", 1) != 0;
    std::string same_note;
    std::vector<int> made_slots; /* every save this import made: one import question for all of them (3.5) */
    for (const std::string& p : paths) {
        int r = import_one(p, 0, &message);
        if (r < 0 && !ask_same) {
            PcSaveInfo other;
            same_note = tx("saves.imported_same", {{"name", pc_saves_get(-r, &other) ? other.name : "?"}});
            r = import_one(p, 1, &message);
        }
        if (r > 0) { made++; last = r; made_slots.push_back(r); }
        else if ((r < 0 || s_tampered) && paths.size() == 1) {
            /* the same window for both questions: "Import again?" (a save already here) and "Import just the save?"
             * (an .orresave that doesn't fit together; its checkbox hidden) */
            s_already_path = p;
            s_already_tampered = s_tampered;
            s_noask_ticked = false;
            if (Rml::Element* box = el("sva-box")) { box->SetClass("on", false); box->SetInnerRML(""); }
            set("svalreadyt", esc(message));
            set("sva-okt", esc(tx(s_tampered ? "saves.import_just_save" : "saves.import_again")));
            body_class("svtampered", s_tampered);
            open_dialog(ALREADY);
            return;
        } else last_problem = message;
    }
    if (!same_note.empty() && paths.size() == 1 && made == 1) {
        reload(last);
        draw();
        notice(same_note);
        if (s_changed) s_changed();
        import_ask_open(made_slots);
        return;
    }
    reload(last);
    draw();
    if (paths.size() == 1 && made == 1) badge(tx("saves.imported_one")); /* 2026-11-21: the green badge, not the line */
    else if (paths.size() == 1) notice(message);
    else notice(tx_count("saves.imported_many", made, {}) + (last_problem.empty() ? "" : "  " + last_problem));
    if (s_changed) s_changed();
    import_ask_open(made_slots); /* the import question (3.5), unless turned off or nothing it applies to */
}

/* Dolphin's save folder, where the file window opens ("keeps most of the scan's convenience") */
std::string dolphin_folder() {
    std::vector<std::string> tries;
#ifdef _WIN32
    if (const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)) tries.push_back(std::string(docs) + "Dolphin Emulator\\GC\\");
    if (const char* app = SDL_getenv("APPDATA")) tries.push_back(std::string(app) + "\\Dolphin Emulator\\GC\\");
#elif defined(__APPLE__)
    if (const char* home = SDL_getenv("HOME")) tries.push_back(std::string(home) + "/Library/Application Support/Dolphin/GC/");
#else
    if (const char* home = SDL_getenv("HOME")) tries.push_back(std::string(home) + "/.local/share/dolphin-emu/GC/");
#endif
    for (const std::string& gc : tries) {
        const std::string cardA = gc + "USA/Card A/";
        if (SDL_GetPathInfo(cardA.c_str(), nullptr)) return cardA;
        if (SDL_GetPathInfo(gc.c_str(), nullptr)) return gc;
    }
    return "";
}

void SDLCALL picked(void*, const char* const* files, int) {
    if (files == nullptr) return;
    std::lock_guard<std::mutex> lock(s_pick_mutex);
    for (int i = 0; files[i] != nullptr; ++i) s_picked.push_back(files[i]);
}

void import_dialog() {
    static const SDL_DialogFileFilter filters[] = {{"Saves (.orresave, .gci, .raw)", "orresave;gci;raw"}, {"All files", "*"}};
    const std::string where = dolphin_folder();
    SDL_ShowOpenFileDialog(picked, nullptr, SDL_GetKeyboardFocus(), filters, 2, where.empty() ? nullptr : where.c_str(), true);
}

void choose_row(int i) {
    if (i < 0 || i >= (int)s_shown.size()) return;
    s_sel = i;
    if (const PcSaveInfo* s = selected(); s != nullptr && s->is_new) {
        pc_saves_mark_seen(s->slot);
    }
    draw();
}

void play_this() {
    if (const PcSaveInfo* s = selected()) {
        pc_saves_set_current(s->slot);
        /* "Play this save" after "Play the autosave" plays the save itself (2026-11-21; the hub's card says which) */
        pc_autosave_cancel_request();
        std::printf("[hub] Saves: Play now starts slot %d (\"%s\")\n", s->slot, s->name);
        if (s_changed) s_changed();
        hub_saves_close();
    }
}

/* the details' "Play the autosave" (2026-11-20, sv-playauto): the same as Play this save, and the game's first read of
 * the save loads its autosave in its place (pc_autosave.cpp), so Continue goes on from the autosave's spot. The slot's
 * files are not changed */
void play_autosave() {
    if (const PcSaveInfo* s = selected()) {
        pc_saves_set_current(s->slot);
        pc_autosave_request_load(s->slot);
        std::printf("[hub] Saves: Play the autosave starts slot %d (\"%s\") from its autosave\n", s->slot, s->name);
        if (s_changed) s_changed();
        hub_saves_close();
    }
}

void rename_start() {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    if (auto* in = dynamic_cast<Rml::ElementFormControlInput*>(el("svnamein"))) in->SetValue(s->name);
    open_dialog(RENAME);
    s_focus_frames = 2;
}

void rename_done() {
    const PcSaveInfo* s = selected();
    auto* in = dynamic_cast<Rml::ElementFormControlInput*>(el("svnamein"));
    if (s != nullptr && in != nullptr) {
        const std::string name = in->GetValue();
        const int slot = s->slot;
        if (!name.empty() && pc_saves_rename(slot, name.c_str())) {
            reload(slot);
            notice("");
            if (s_changed) s_changed();
        } else if (!name.empty()) {
            notice(tx("saves.err.rename"));
        }
    }
    open_dialog(NONE);
    draw();
}

void duplicate() {
    if (const PcSaveInfo* s = selected()) {
        const int copy = pc_saves_duplicate(s->slot);
        reload(copy > 0 ? copy : s->slot);
        draw();
        notice(copy > 0 ? tx("saves.duplicated") : tx("saves.err.duplicate"));
    }
}

/* "New save" (snatcherLeo, 2026-11-14: "let's make it simple to start out with no save file"): an empty save at the top of
 * the list, chosen, so Play this save starts the game with no save file and it offers NEW GAME */
void new_empty() {
    const int slot = pc_saves_new_empty(tx("saves.new_save_name").c_str());
    reload(slot > 0 ? slot : 0);
    draw();
    notice(slot > 0 ? tx("saves.made_new") : tx("saves.err.new_save"));
}

void show_folder() {
    if (const PcSaveInfo* s = selected()) {
        char dir[1200];
        std::string url = "file:///";
        url += pc_saves_slot_dir(s->slot, dir, sizeof(dir));
        std::replace(url.begin(), url.end(), '\\', '/');
        SDL_OpenURL(url.c_str());
    }
}

void delete_ask() {
    if (const PcSaveInfo* s = selected()) {
        set("svdeletet", esc(tx("saves.delete_ask", {{"name", s->name}})));
        open_dialog(DELETE_ASK);
    }
}

void delete_done() {
    if (const PcSaveInfo* s = selected()) {
        const bool ok = pc_saves_delete(s->slot) != 0;
        reload(0);
        notice(ok ? tx("saves.deleted") : tx("saves.err.delete"));
        if (s_changed) s_changed();
    }
    open_dialog(NONE);
    draw();
}

/* The go-back window's rows (mockup 74; snatcherLeo, 2026-11-13: "SIMPLE and EFFORTLESS and EASY TO READ"): the date in
 * plain words, the play time, a small reason; "Go back" shows on the chosen / pointed-at row only (hub.rcss .svvr). */
void draw_versions() {
    std::string h;
    for (size_t i = 0; i < s_versions.size(); ++i) {
        const PcSaveVersion& v = s_versions[i];
        const std::string why = std::string("saves.why.") + v.why;
        h += std::string("<div class='svv") + ((int)i == s_vsel ? " f" : "") + "' id='svv-" + std::to_string(i) + "'>" +
             "<span class='svvw'>" + esc(friendly_of_stamp(v.when)) + "</span><span class='svvp'>" +
             esc(tx("saves.version_line", {{"time", play_time(v.play_seconds)}})) +
             "</span><span class='svvwhy'>" + esc(tx(why.c_str())) + "</span><span class='svvr'>" +
             esc(tx("saves.restore_one")) + " ›</span></div>";
    }
    if (s_versions.empty()) h = "<div class='svempty'>" + esc(tx("saves.no_versions")) + "</div>";
    set("svvlist", h);
    if (Rml::Element* row = el(("svv-" + std::to_string(s_vsel)).c_str())) row->ScrollIntoView(false);
}

void restore_start() {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    std::vector<PcSaveVersion> all(512);
    const int n = pc_saves_versions(s->slot, all.data(), (int)all.size());
    s_versions.clear();
    for (int i = 0; i < n; ++i) {
        /* hidden: a copy identical to the save now (going back would change nothing), and Undo's own copy while Undo
         * is offered (Undo is its way back; snatcherLeo: "do we even need a "Before you restored"?"). Once Undo has ended,
         * an older "before you went back" copy shows again, so that progress stays reachable (2026-11-13). */
        if (all[(size_t)i].same_as_now || all[(size_t)i].is_undo) continue;
        s_versions.push_back(all[(size_t)i]);
    }
    s_vsel = 0;
    set("svrestoren", esc(s->name));
    set("svvnow", s->has_save ? esc(tx("saves.now", {{"trainer", hub_shown_name(s->trainer)}, {"time", play_time(s->play_seconds)},
                                                     {"date", friendly_of(s->saved_time)}}))
                              : esc(tx("saves.not_started")));
    draw_versions();
    open_dialog(RESTORE);
}

/* one click goes back, no "Are you sure?" (Undo follows in the notice; the plan snatcherLeo approved, 2026-11-13) */
void restore_pick(int i) {
    const PcSaveInfo* s = selected();
    if (s == nullptr || i < 0 || i >= (int)s_versions.size()) return;
    const int slot = s->slot;
    const bool ok = pc_saves_restore(slot, s_versions[(size_t)i].folder) != 0;
    open_dialog(NONE);
    reload(slot);
    draw(); /* shows "Went back to your save from ... Undo" (refresh_undo_note) */
    if (!ok) notice(tx("saves.err.restore"));
    if (s_changed) s_changed();
}

void undo_restore() {
    const PcSaveInfo* s = selected();
    if (s == nullptr) return;
    const int slot = s->slot;
    const bool ok = pc_saves_undo(slot) != 0;
    reload(slot);
    draw();
    notice(tx(ok ? "saves.undone" : "saves.err.undo"));
    if (s_changed) s_changed();
}

/* Y on the page: Undo while it is offered for the chosen save, else the go-back window (its hint follows) */
void y_button() {
    char to[64];
    if (undo_offered(to, sizeof(to), nullptr)) undo_restore();
    else restore_start();
}

void move_sel(int d) {
    if (s_dialog == WHATOFF) { /* Up / Down scroll the list (the mouse wheel does too) */
        if (Rml::Element* l = el("svwhatlist")) l->SetScrollTop(l->GetScrollTop() + 60.f * (float)d);
        return;
    }
    if (s_dialog == RESTORE) {
        s_vsel = std::clamp(s_vsel + d, 0, std::max((int)s_versions.size() - 1, 0));
        draw_versions();
        return;
    }
    if (s_dialog == GCOPY) { /* the copy window: Up / Down step through PAL's languages (3.5) */
        gcopy_step(0, d);
        return;
    }
    if (s_dialog == IMPORT_ASK) { /* the import question: Enhanced, GameCube-accurate, the tick */
        s_ask_focus = std::clamp(s_ask_focus + d, 0, 2);
        import_ask_render();
        return;
    }
    choose_row(std::clamp(s_sel + d, 0, std::max((int)s_shown.size() - 1, 0)));
}

void back() {
    if (s_dialog == IMPORT_ASK) { /* the import question closed unanswered: nothing changes */
        std::printf("[hub] import question closed: %zu save(s) keep the new-saves default (%s)\n", s_ask_slots.size(),
                    pc_option_accurate_new_saves() ? "GameCube-accurate" : "Enhanced");
        s_ask_slots.clear();
    }
    if (s_dialog != NONE) {
        open_dialog(NONE);
        draw();
        return;
    }
    /* leaving the page (the back arrow or B) keeps the save last chosen in the list as the current save, as Play this
     * save does, so choosing a save and going back is a second way to pick it (snatcherLeo, 2026-11-14: "just clicking on a
     * save, and then clicking the back arrow will have it be selected as the Current save") */
    if (const PcSaveInfo* s = selected()) {
        if (s->slot != pc_saves_current()) {
            pc_saves_set_current(s->slot);
            /* another save picked this way plays itself, not an autosave armed earlier for the save before (2026-11-21:
             * the card would otherwise forget the armed one, which would come back if that save were picked again) */
            pc_autosave_cancel_request();
            std::printf("[hub] Saves: back keeps slot %d (\"%s\") as the current save\n", s->slot, s->name);
            if (s_changed) s_changed();
        }
    }
    hub_saves_close();
}

void confirm() {
    switch (s_dialog) {
    case RENAME: rename_done(); break;
    case RESTORE: restore_pick(s_vsel); break;
    case DELETE_ASK: delete_done(); break;
    case GCOPY: gcopy_ok(); break;
    case GCACC: gcopy_start(true); break;
    case WHATOFF: open_dialog(NONE); draw(); break; /* A closes the list, as B does */
    case IMPORT_ASK: import_ask_press(); break;
    case ALREADY: {
        std::string message;
        if (s_noask_ticked) { /* "Don't ask me again": from now on the same save just imports */
            pc_settings_set_int("saves_ask_same", 0);
            pc_settings_save();
        }
        const int made = import_one(s_already_path, s_already_tampered ? 2 : 1, &message);
        open_dialog(NONE);
        reload(made);
        draw();
        if (made > 0) badge(tx("saves.imported_one")); /* 2026-11-21: the green badge, not the line */
        else notice(message);
        if (s_changed) s_changed();
        if (made > 0) import_ask_open({made}); /* imported again as a new save: the import question too (3.5) */
        break;
    }
    default: play_this(); break;
    }
}

/* Holding Up / Down (the D-pad or the stick) scrolls the list fast, as a held arrow key does (snatcherLeo, 2026-11-15: "if
 * users hold on up dpad/control stick, it scrolls up or down fast (like how users would fast scroll normally)"): one
 * step on the press, then after 400 ms a step every 60 ms while held (hub_saves_frame). Keyboard arrows already repeat
 * by the system's own key repeat. All OSes the same. */
bool s_name_focus_prev = false;       /* the Rename box had the keyboard at the last frame */
bool s_name_focused_at_press = false; /* ... when the mouse went down (hub_saves_name_press) */
int s_hold = 0;            /* -1 up, 1 down, 0 none */
Uint64 s_hold_next = 0;    /* when the next repeated step is due (ms) */
void hold(int dir) {
    s_hold = dir;
    s_hold_next = SDL_GetTicks() + 400;
}

/* a D-pad / stick move or A / B while no window is open: true when the details panel took it (Right from the list
 * enters it; inside it the moves, A presses, B and Left from the edge go back to the list) */
bool panel_button(SDL_GamepadButton b) {
    if (s_panel.empty()) {
        if (b != SDL_GAMEPAD_BUTTON_DPAD_RIGHT || el("sv-play") == nullptr) return false;
        s_panel = "sv-play";
        panel_mark();
        return true;
    }
    int dx = 0, dy = 0;
    switch (b) {
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: dx = -1; break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: dx = 1; break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: dy = -1; break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: dy = 1; break;
    case SDL_GAMEPAD_BUTTON_SOUTH:
        if (Rml::Element* e = el(s_panel.c_str())) e->Click();
        return true;
    case SDL_GAMEPAD_BUTTON_EAST:
        s_panel.clear();
        panel_mark();
        return true;
    default: return false;
    }
    const std::string next = panel_next(s_panel, dx, dy);
    if (!next.empty()) s_panel = next;
    else if (dx < 0) s_panel.clear(); /* Left from the left edge: back to the list */
    panel_mark();
    return true;
}
} // namespace

void hub_saves_attach(Rml::ElementDocument* doc, std::function<void()> changed) {
    s_doc = doc;
    s_changed = std::move(changed);
}

void hub_saves_open() {
    if (s_doc == nullptr || !pc_saves_ready()) return;
    s_open = true;
    s_panel.clear(); /* the controller starts in the list */
    s_search.clear();
    if (auto* in = dynamic_cast<Rml::ElementFormControlInput*>(el("svsearch"))) in->SetValue("");
    reload(pc_saves_current());
    open_dialog(NONE);
    notice("");
    body_class("saves", true);
    draw();
    std::printf("[hub] Saves page opened (%zu saves)\n", s_all.size());
}

void hub_saves_close() {
    s_open = false;
    open_dialog(NONE);
    body_class("saves", false);
}

bool hub_saves_is_open() { return s_open; }

void hub_saves_drop(const char* path) {
    if (!pc_saves_ready() || path == nullptr) return;
    if (!s_open) hub_saves_open();
    import_files({path});
}

/* the import question for the saves found loose in Saves\ at start-up (3.5; pc_saves_take_loose_imported): once the
 * hub shows, the Saves page opens on the first of them with the question, as for any other import. Nothing when none
 * was found, when the question is off, or when they are all .orresaves */
void hub_saves_ask_loose() {
    int slots[64];
    const int n = pc_saves_take_loose_imported(slots, 64);
    if (n <= 0 || s_doc == nullptr || !pc_saves_ready()) return;
    if (!import_ask_wanted()) {
        std::printf("[hub] import question off (settings.ini import_ask=0): %d save(s) found in Saves\\ keep the new-saves "
                    "default\n", n);
        return;
    }
    std::vector<int> made(slots, slots + n);
    bool any = false;
    for (int slot : made) {
        PcSaveInfo info;
        if (pc_saves_get(slot, &info) && !info.is_orresave) any = true;
    }
    if (!any) return;
    hub_saves_open();
    reload(made.front());
    draw();
    import_ask_open(made);
}

/* test hook (ORRE_HUB_TEST_SAVES=importask:<slot>[,<slot>...]): the question for those saves, without importing */
void hub_saves_test_import_ask(const char* slots) {
    hub_saves_open();
    if (!s_open) return;
    std::vector<int> made;
    for (const char* p = slots; p != nullptr && *p != '\0';) {
        made.push_back(std::atoi(p));
        p = std::strchr(p, ',');
        if (p != nullptr) ++p;
    }
    if (!made.empty()) reload(made.front());
    draw();
    import_ask_open(made);
}

/* for scripted test windows only (pc_hub.cpp, ORRE_HUB_TEST_SAVES=versions): photographs of the go-back window */
void hub_saves_test_versions() {
    hub_saves_open();
    if (s_open) restore_start();
}

/* the hub's PLAY with GameCube-accurate mode on and the current save holding long names (mockup 104 D; decided: an
 * .orresave plays in accurate mode only through a GameCube copy): the Saves page opens on that save with the offer */
void hub_saves_offer_copy(int slot) {
    if (s_doc == nullptr || !pc_saves_ready()) return;
    hub_saves_open();
    reload(slot);
    draw();
    if (const PcSaveInfo* s = selected()) {
        set("svgcacct", esc(tx("gcopy.accurate_title")));
        set("svgcaccs", esc(tx("gcopy.accurate_body", {{"name", s->name}})));
        open_dialog(GCACC);
    }
}

/* test hooks (pc_hub.cpp ORRE_HUB_TEST_SAVES, 2.12): pictures of the switch's copy offer and the list. "accurate:<slot>"
 * opens the page on that save and presses its switch; "whatoff" opens the list over the chosen save */
void hub_saves_test_accurate(int slot) {
    hub_saves_open();
    if (!s_open) return;
    reload(slot);
    draw();
    accurate_toggle();
}

/* test hook (pc_hub.cpp ORRE_HUB_TEST_SAVES=playauto:<slot>, 2026-11-21): the page opened on that save and its "Play
 * the autosave" pressed, so a picture of the hub's card shows the armed autosave */
void hub_saves_test_play_autosave(int slot) {
    hub_saves_open();
    if (!s_open) return;
    reload(slot);
    draw();
    play_autosave();
}

void hub_saves_test_whatoff() {
    hub_saves_open();
    if (s_open) whatoff_open();
}

/* "gcopyacc:<slot>": accurate mode's copy offer accepted for that save, the copy written to `path` without the system's
 * Save As window (which a test can't drive), then added as a save in accurate mode (gcopy_write) */
void hub_saves_test_gcopy(int slot, const char* path) {
    hub_saves_open();
    if (!s_open) return;
    reload(slot);
    draw();
    /* *added 2026-11-21:* path NULL ("gcopy:<slot>") only opens the ordinary copy window (Make a GameCube copy…), for a
     * picture of its notes (the PC MENU note) */
    if (path == nullptr) {
        gcopy_start(false);
        return;
    }
    gcopy_start(true);
    if (s_dialog == GCOPY) gcopy_write(path);
}

bool hub_saves_is_save_file(const char* path) {
    if (path == nullptr) return false;
    std::string p = path;
    std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return (char)SDL_tolower(c); });
    return (p.size() > 4 && (p.compare(p.size() - 4, 4, ".gci") == 0 || p.compare(p.size() - 4, 4, ".raw") == 0)) ||
           (p.size() > 9 && p.compare(p.size() - 9, 9, ".orresave") == 0); /* 2.4 part 5 */
}

/* the hub's mouse-press listener (pc_hub.cpp Presses), before the click: whether the Rename box already had the
 * keyboard before this press gave it (RmlUi focuses before it reports the press, so the last frame's state is used) */
void hub_saves_name_press() { s_name_focused_at_press = s_name_focus_prev; }

void hub_saves_frame() {
    {
        Rml::Element* box = el("svnamein");
        s_name_focus_prev = s_open && s_dialog == RENAME && box != nullptr && box->IsPseudoClassSet("focus");
    }
    if (!s_open || !s_panel.empty()) s_hold = 0;
    if (s_hold != 0 && SDL_GetTicks() >= s_hold_next) { /* a held Up / Down: the next step */
        move_sel(s_hold);
        s_hold_next = SDL_GetTicks() + 60;
    }
    std::vector<std::string> picked, gc_dirs, orre;
    {
        std::lock_guard<std::mutex> lock(s_pick_mutex);
        picked.swap(s_picked);
        gc_dirs.swap(s_gc_dirs);
        orre.swap(s_orre_paths);
    }
    if (!orre.empty() && s_open) orre_write(orre.back()); /* the .orresave's file was chosen */
    if (!gc_dirs.empty() && s_open && s_dialog == GCOPY) gcopy_write(gc_dirs.back()); /* the copy's file was chosen */
    if (!picked.empty()) {
        if (!s_open) hub_saves_open();
        import_files(picked);
    }
    if (!s_open) return;
    if (s_dialog == IMPORT_ASK && s_ask_test_at != 0 && SDL_GetTicks() >= s_ask_test_at) {
        /* the test switch ORRE_HUB_TEST_IMPORT_ANSWER (import_ask_open): the answer as a click would give it */
        s_ask_test_at = 0;
        const char* a = SDL_getenv("ORRE_HUB_TEST_IMPORT_ANSWER");
        if (a != nullptr) {
            s_ask_always = std::strstr(a, "always") != nullptr;
            import_ask_answer(std::strncmp(a, "accurate", 8) == 0);
        }
    }
    if (s_toast_until != 0 && SDL_GetTicks() >= s_toast_until) { /* the bubble's time is up */
        body_class("svtoasting", false);
        s_toast_until = 0;
    }
    if (s_badge_until != 0 && SDL_GetTicks() >= s_badge_until) { /* the import badge fades out (its rcss transition) */
        if (Rml::Element* b = el("svbadge")) b->SetClass("on", false);
        body_class("svbadging", false);
        s_badge_until = 0;
    }
    body_class("svsearching", !s_search.empty()); /* the search box's hint text hides once something is typed */
    if (s_focus_frames > 0 && --s_focus_frames == 0) {
        if (Rml::Element* e = el("svnamein")) e->Focus();
    }
    /* the search box, read each frame (typing goes to RmlUi) */
    if (auto* in = dynamic_cast<Rml::ElementFormControlInput*>(el("svsearch"))) {
        const std::string now = in->GetValue();
        if (now != s_search) {
            s_search = now;
            const PcSaveInfo* s = selected();
            reload(s != nullptr ? s->slot : 0);
            draw();
        }
    }
}

bool hub_saves_click(Rml::Element* target) {
    /* the Rename box: a click while it already has the keyboard selects all its text, so a new name can be typed
     * straight over the old one (snatcherLeo, 2026-11-15: "if I click again, I expect it to select all text in this text
     * bar"); the first click places the caret as usual. Whether it had the keyboard is noted at the press
     * (hub_saves_name_press), because the click comes after the press has already given it the keyboard */
    if (target != nullptr && target->GetId() == "svnamein" && s_name_focused_at_press) {
        if (auto* in = dynamic_cast<Rml::ElementFormControlInput*>(target)) in->Select();
        return true;
    }
    for (Rml::Element* e = target; e != nullptr; e = e->GetParentNode()) {
        const std::string id = e->GetId();
        if (id.empty()) continue;
        /* the Cancel buttons and the dimmed area outside a window close it (snatcherLeo, 2026-11-13: "if I click out the
         * window, that should get me out as well"). Checked before the version rows: "svv-cancel" also starts with the
         * rows' "svv-", and a Cancel click used to go back to the newest version (found by snatcherLeo's test) */
        if (id == "sv-gcopy") { gcopy_start(false); return true; }
        if (id == "svga-ok") { gcopy_start(true); return true; } /* accurate mode's offer: the copy plays accurate */
        if (id == "sv-acc") { accurate_toggle(); return true; }
        if (id == "sv-accwhat") { whatoff_open(); return true; }
        if (id == "svw-close") { back(); return true; }
        if (id == "sv-orre") { orre_export(); return true; }
        if (id == "svg-ok") { gcopy_ok(); return true; }
        if (id.size() == 7 && id.rfind("svg-r-", 0) == 0) { s_gc_target.region = id[6]; gcopy_render(); return true; }
        if (id.size() == 7 && id.rfind("svg-l-", 0) == 0) { s_gc_target.language = id[6] - '0'; gcopy_render(); return true; }
        if (id == "svn-cancel" || id == "svd-cancel" || id == "sva-cancel" || id == "svv-cancel" || id == "svg-cancel" ||
            id == "svga-cancel" || id == "svimp-later" || id == "svdim") {
            back();
            return true;
        }
        if (id == "sv-back") { back(); return true; }
        if (id == "sv-import") { import_dialog(); return true; }
        if (id == "sv-new") { new_empty(); return true; }
        if (id == "svsort") { s_sort = (s_sort + 1) % 4; const PcSaveInfo* s = selected(); reload(s ? s->slot : 0); draw(); return true; }
        if (id.rfind("svr-", 0) == 0) { choose_row(std::atoi(id.c_str() + 4)); return true; }
        if (id == "sv-play") { play_this(); return true; }
        if (id == "sv-playauto") { play_autosave(); return true; }
        if (id == "sv-rename") { rename_start(); return true; }
        if (id == "sv-dup") { duplicate(); return true; }
        if (id == "sv-folder") { show_folder(); return true; }
        if (id == "sv-delete") { delete_ask(); return true; }
        if (id == "sv-restore") { restore_start(); return true; }
        if (id == "sv-undo") { undo_restore(); return true; }
        if (id.rfind("svv-", 0) == 0 && id.size() > 4 && SDL_isdigit((unsigned char)id[4])) { /* a version row only */
            restore_pick(std::atoi(id.c_str() + 4));
            return true;
        }
        /* the import question (3.5): a click on an answer answers; the tick (its box or its words) toggles */
        if (id == "svimp-enh" || id == "svimp-acc") {
            if (s_dialog == IMPORT_ASK) import_ask_answer(id == "svimp-acc");
            return true;
        }
        if (id == "svimp-noask" || id == "svimp-box") {
            s_ask_always = !s_ask_always;
            s_ask_focus = 2;
            import_ask_render();
            return true;
        }
        if (id == "sva-noask" || id == "sva-box") {
            s_noask_ticked = !s_noask_ticked;
            if (Rml::Element* box = el("sva-box")) {
                box->SetClass("on", s_noask_ticked);
                box->SetInnerRML(s_noask_ticked ? "✓" : "");
            }
            return true;
        }
        if (id == "svn-ok" || id == "svd-ok" || id == "sva-ok") { confirm(); return true; }
        if (id == "svsearch" || id == "svnamein") return false; /* text boxes take the click themselves */
    }
    return false;
}

input::EventResult hub_saves_event(const input::InputEvent& event) {
    using E = input::InputEvent;
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        const bool typing = s_dialog == RENAME || (s_dialog == NONE && el("svsearch") != nullptr && el("svsearch")->IsPseudoClassSet("focus"));
        if (key->pressed && !key->repeat) {
            if (key->keycode == SDLK_ESCAPE) { back(); return input::EventResult::Consume; }
            if (key->keycode == SDLK_SPACE && s_dialog == IMPORT_ASK) { confirm(); return input::EventResult::Consume; }
            if ((key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER) && (s_dialog != NONE || !typing)) {
                confirm();
                return input::EventResult::Consume;
            }
        }
        if (!typing && key->pressed) {
            if (key->keycode == SDLK_UP) { move_sel(-1); return input::EventResult::Consume; }
            if (key->keycode == SDLK_DOWN) { move_sel(1); return input::EventResult::Consume; }
            if ((key->keycode == SDLK_LEFT || key->keycode == SDLK_RIGHT) && s_dialog == GCOPY) { /* the copy's region */
                gcopy_step(key->keycode == SDLK_LEFT ? -1 : 1, 0);
                return input::EventResult::Consume;
            }
            if (key->keycode == SDLK_DELETE && s_dialog == NONE) { delete_ask(); return input::EventResult::Consume; }
            if (key->keycode == SDLK_F2 && s_dialog == NONE) { rename_start(); return input::EventResult::Consume; }
        }
        aurora::rmlui::process_input(event);
        return input::EventResult::Consume;
    }
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        if (!button->pressed && (button->button == SDL_GAMEPAD_BUTTON_DPAD_UP || button->button == SDL_GAMEPAD_BUTTON_DPAD_DOWN))
            hold(0);
        if (button->pressed && s_dialog == NONE && panel_button(button->button)) return input::EventResult::Consume;
        if (button->pressed && s_dialog == GCOPY && (button->button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button->button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
            gcopy_step(button->button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? -1 : 1, 0); /* the copy window's region */
            return input::EventResult::Consume;
        }
        if (button->pressed) {
            switch (button->button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP: move_sel(-1); hold(-1); break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: move_sel(1); hold(1); break;
            case SDL_GAMEPAD_BUTTON_SOUTH: confirm(); break;
            case SDL_GAMEPAD_BUTTON_EAST: back(); break;
            case SDL_GAMEPAD_BUTTON_WEST: if (s_dialog == NONE) rename_start(); break;
            case SDL_GAMEPAD_BUTTON_NORTH: if (s_dialog == NONE) y_button(); break;
            default: break;
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* axis = event.payload.get_if<E::AxisChanged>()) {
        /* the left stick moves through the list as the D-pad does, one step each time it crosses half way (
         * 2026-11-15, with the hub's own controller navigation; all OSes) */
        static float lastY = 0.f, lastX = 0.f;
        if (axis->axis == SDL_GAMEPAD_AXIS_LEFTY || axis->axis == SDL_GAMEPAD_AXIS_LEFTX) {
            float& last = axis->axis == SDL_GAMEPAD_AXIS_LEFTY ? lastY : lastX;
            const int was = last > 0.5f ? 1 : last < -0.5f ? -1 : 0, now = axis->value > 0.5f ? 1 : axis->value < -0.5f ? -1 : 0;
            last = axis->value;
            if (now != 0 && now != was) {
                /* the stick as the D-pad: the same moves in the list, the panel and the windows */
                const bool vertical = axis->axis == SDL_GAMEPAD_AXIS_LEFTY;
                const SDL_GamepadButton as = vertical ? (now < 0 ? SDL_GAMEPAD_BUTTON_DPAD_UP : SDL_GAMEPAD_BUTTON_DPAD_DOWN)
                                                      : (now < 0 ? SDL_GAMEPAD_BUTTON_DPAD_LEFT : SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                if (s_dialog == GCOPY && !vertical) gcopy_step(now, 0); /* the copy window's region */
                else if (!(s_dialog == NONE && panel_button(as)) && vertical) {
                    move_sel(now);
                    hold(now);
                }
            }
            if (now == 0 && axis->axis == SDL_GAMEPAD_AXIS_LEFTY) hold(0); /* back to the middle: the repeat stops */
        }
        return input::EventResult::Consume;
    }
    aurora::rmlui::process_input(event); /* the mouse: clicks, hover, the list's wheel */
    return input::EventResult::Consume;
}
