// app.cpp: OrrePort Studio's main window on Windows (the Win32 shell; first stretch, 2026-11-18).
//
// What it shows (Studio's brief and the project's sandbox, build-pc/mockups/studio-sandbox): a slim editor rail on the left
// (Pokémon active, the other editors "Coming soon"), the Pokémon list with mug shots and a search box, the record page
// in the centre (name, types, numbers, the 3D model turning, the base stats), and a foldable inspector on the right
// (mug shots both facings normal and shiny, PC pictures, the model's facts). Everything comes from the player's disc
// through the reader core (orre_core.h); nothing is built in.
//
// How it is drawn (the Studio review's decision 1, "hybrid"): Studio's own surfaces with Direct2D and DirectWrite, in
// OrrePort's shipped fonts loaded privately (decision 6 of the architecture: never the system's fonts); Windows' own
// controls where they matter (the search box, so typing in any language and screen readers work; the file dialog).
// Sharp on every screen: per-monitor DPI v2 (the manifest and SetProcessDpiAwarenessContext), all layout in DIPs.
// Light / dark: OrrePort's shared Theme setting (theme.cpp). Every visible word is a key (lang.cpp, studio/lang/).
// Work off the interface thread: the disc's list and the models load on worker threads; the window never waits.
//
// Platforms: this whole file is the Windows shell. Linux (Qt) and macOS (AppKit) get their own shell over the same
// core; the layout numbers and behaviour below are what those shells copy (the plan: "porting Studio means writing a
// new shell, never changing how an editor behaves").
#include "app.hpp"

#include <commctrl.h>
#include <dwmapi.h>  // DwmSetWindowAttribute: cloaking the first window until its first full page (Windows only)
#include <commdlg.h>
#include <d3d11.h>   // the shared drawing device (multi-window plan step 1c; Windows only)
#include <dxgi1_2.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <mmsystem.h>  // (PlaySound: the cries' preview; Windows only, winmm)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <array>
#include <memory>
#include <sstream>
#include <thread>

#include <shlobj.h>   // SHCreateDirectoryExW, DROPFILES: Report a problem's folder and its clipboard copy (Windows only)
#include <wincodec.h> // WIC: a report's screenshots scaled down and saved as JPEG (Windows only)

#include "build_stamp.h"  // studio/diag: the build's identity (about.txt)
#include "sysinfo.h"      // studio/diag: the machine and its screens (the log's start, about.txt)
#include "pc_clip_file.h"  // port/src: the report on the clipboard as a file (shared with OrrePort)
#include "pc_scrub.h"      // port/src: the user's folder and name out of the report's text (shared with OrrePort)
#include "pc_zip.h"        // port/src: the report's zip (shared with OrrePort)
#include "pc_update.h"     // port/src: the updater both programs share (4.4; plain C, extern "C" inside)
#include "pc_apps.h"       // port/src: OrrePort and Studio finding, showing, starting and closing each other (4.4)
#include "pc_crash.h"  // port/src: OrrePort's crash handler (extern "C" inside)
#include "trail.h"     // studio/diag: the action trail (C API, moves to the shared diagnostics library before the
                       // laptop sitting, Phase 5 step 5.10; it was 4.4 until 2026-11-22)

namespace studio {

namespace {

// ---- messages from worker threads ----
// WM_VIEW_FRAMED: the 3D view presented its first frame with a model, so the page can reveal it (no white flash)
// WM_MOVE_ENDED: the Moves tab's move has played to its end (the Play button turns back)
enum : UINT { WM_LIST_LOADED = WM_APP + 1, WM_MODEL_LOADED = WM_APP + 2, WM_SPIN_STOPPED = WM_APP + 20, WM_VIEW_FRAMED = WM_APP + 21,
              WM_MOVE_ENDED = WM_APP + 22, WM_VIEW_SHARED = WM_APP + 30,  // (30: a frame is in the shared texture)
              WM_VIEW_MOVED = WM_APP + 33,  // (33: the model was turned, zoomed or reset by the mouse: the page redraws)
              WM_DISC_OP_DONE = WM_APP + 40,   // (40: Settings › Discs' copy or move finished; lp: its result)
              WM_DISCS_READ = WM_APP + 41 };   // (41: the ROM folders' watch read them in the background; lp: the rows)
// TIMER_PLAYHEAD: redraws the Moves tab's timeline while its animation plays (30 a second, the animation's own rate)
// TIMER_TABGROW: redraws the tab bar while a new tab grows in (about 60 a second, for kTabGrowMs)
enum : UINT_PTR { TIMER_WATCH = 1, TIMER_SHOT = 2, TIMER_TOAST = 3, TIMER_REVEAL = 4, TIMER_FLASH = 5, TIMER_PLAYHEAD = 9,
                  TIMER_TABGROW = 20, TIMER_DISCOP = 21 };  // (21: redraws Settings › Discs' progress, 10 a second)
constexpr unsigned kTabGrowMs = 180;  // how long a new tab takes to grow in (Chrome's is about as quick)

// ---- layout (DIPs) ----
// v3: a tab bar across the top (kTabBar), then the rail (56), the list (236) and the page using the rest (no inspector)
constexpr float kTabBar = 36, kRail = 56, kList = 236, kInspector = 0, kHeader = 92, kStats = 104, kRow = 50, kListTop = 104 + kTabBar,
                kGap = 22;

struct Entry {
    int index = 0;
    OrreSpecies s{};
    std::wstring name, category;  // read on the worker thread: text lookups can touch the disc
    std::vector<uint8_t> mug;     // 42 x 42 RGBA (normal, the cell the game's menus show)
    ID2D1Bitmap* bmp = nullptr;
};

struct Pic {
    ID2D1Bitmap* bmp = nullptr;
    int w = 0, h = 0;
    std::wstring label;
};

struct ListResult {
    std::vector<Entry> entries;
    std::string error;
    std::string identity;  // the disc in one line (disc_identity)
    double ms = 0;
};

// The disc in one line, for the log and a report (4.3 part 3, the logging list's item 3, approved by snatcherLeo): its game
// id, which game and release that is, main.dol's SHA-1 and whether it is the US release OrrePort is built for, then
// what OrrePort's own whole-disc check said about this exact file. Why: "Studio shows the wrong picture" on a modified
// or other-region disc is a different problem from a bug, and this line tells them apart at a glance.
// The whole-disc check's result is OrrePort's cache file (port/src/pc_disc_check.c: line 1 "<list id> <size> <change
// time in ns since 1970>", line 2 "ok" / "accepted" / other); Studio compares the size and change time with the file it
// opened. Windows: FILETIME counts 100 ns from 1601; Linux / macOS would take stat()'s st_size and st_mtim (likely).
// A Windows error code with its name, for error lines (4.3 part 3, the logging list's item 8: an error line names what
// was asked for and the system's own code): the graphics codes Studio meets by name, others with Windows' own short
// text. Windows only (HRESULT); Linux / macOS: errno with strerror, Vulkan's VkResult names (likely).
std::string hr_text(HRESULT hr) {
    static const std::pair<HRESULT, const char*> kNames[] = {
        {S_OK, "S_OK (none)"}, {E_OUTOFMEMORY, "E_OUTOFMEMORY"}, {E_INVALIDARG, "E_INVALIDARG"}, {E_FAIL, "E_FAIL"},
        {(HRESULT)0x887A0005, "DXGI_ERROR_DEVICE_REMOVED"}, {(HRESULT)0x887A0006, "DXGI_ERROR_DEVICE_HUNG"},
        {(HRESULT)0x887A0007, "DXGI_ERROR_DEVICE_RESET"}, {(HRESULT)0x887A0020, "DXGI_ERROR_DRIVER_INTERNAL_ERROR"},
        {(HRESULT)0x887A0001, "DXGI_ERROR_INVALID_CALL"}, {(HRESULT)0x887A0004, "DXGI_ERROR_UNSUPPORTED"},
        {(HRESULT)0x887A0022, "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE"}, {(HRESULT)0x8899000C, "D2DERR_RECREATE_TARGET"}};
    char b[200];
    for (const auto& [k, n] : kNames)
        if (k == hr) { std::snprintf(b, sizeof b, "0x%08lX %s", (unsigned long)hr, n); return b; }
    char msg[140] = "";
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, (DWORD)hr, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                       msg, sizeof msg, nullptr))
        msg[std::strcspn(msg, "\r\n")] = 0;
    std::snprintf(b, sizeof b, "0x%08lX%s%s", (unsigned long)hr, msg[0] ? " " : "", msg);
    return b;
}

constexpr char kUsMainDolSha1[] = "870e8b9693ca780782d80f22a6a4572d8ba9458f";  // config/GC6E01/build.sha1's main.dol
std::string disc_check_result(const std::wstring& image, const std::wstring& cache_file) {
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(image.c_str(), GetFileExInfoStandard, &fa)) return "OrrePort's disc check: the file couldn't be looked at";
    const unsigned long long size = (unsigned long long)fa.nFileSizeHigh << 32 | fa.nFileSizeLow;
    const long long ns = ((long long)((unsigned long long)fa.ftLastWriteTime.dwHighDateTime << 32 | fa.ftLastWriteTime.dwLowDateTime)
                          - 116444736000000000LL) * 100;
    FILE* f = _wfopen(cache_file.c_str(), L"r");
    if (!f) return "OrrePort's disc check: not run yet (OrrePort hasn't started with a disc)";
    char line[200] = "", state[40] = "";
    const bool ok = std::fgets(line, sizeof line, f) && std::fgets(state, sizeof state, f);
    std::fclose(f);
    if (!ok) return "OrrePort's disc check: its record couldn't be read";
    state[std::strcspn(state, "\r\n")] = 0;
    unsigned long long csize = 0;
    long long cns = 0;
    char id[64] = "";
    if (std::sscanf(line, "%63s %llu %lld", id, &csize, &cns) != 3) return "OrrePort's disc check: its record couldn't be read";
    if (csize != size || cns != ns) return "OrrePort's disc check: last run on another disc file (not this one)";
    if (std::strcmp(state, "ok") == 0) return "OrrePort's disc check: passed for this file (every file on the disc as the US release has it)";
    if (std::strcmp(state, "accepted") == 0) return "OrrePort's disc check: files differ from the US release; the player chose to play anyway";
    return std::string("OrrePort's disc check: ") + state;
}
std::string disc_identity(OrreDisc* d, const std::wstring& path, const std::wstring& cache_file) {
    const std::string id = orre_disc_game_id(d);
    const char* game = id.rfind("GC6", 0) == 0 ? "Pokémon Colosseum" : id.rfind("GXX", 0) == 0 ? "Pokémon XD" : "another game";
    const char r = id.size() >= 4 ? id[3] : '?';
    const char* region = r == 'E' ? "US" : r == 'P' ? "Europe" : r == 'J' ? "Japan" : "unknown region";
    char sha[41] = "";
    orre_disc_main_dol_sha1(d, sha);
    std::string s = std::string(id.empty() ? "no game id" : id) + ", " + game + " (" + region + "), main.dol " + (sha[0] ? sha : "missing");
    if (id == "GC6E01") s += std::strcmp(sha, kUsMainDolSha1) == 0 ? " (the US release OrrePort is built for)" : " (NOT the US release's main.dol: modified or another revision)";
    if (!cache_file.empty()) s += "; " + disc_check_result(path, cache_file);
    return s;
}

struct ModelResult {
    int generation = 0;
    OrreModel model{};
    bool ok = false;
    double ms = 0;
};

struct Rect { float x, y, w, h; bool in(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

// The editors (v3 rail, snatcherLeo through 2026-11-18): Pokémon, Move, Item | Trainer, People | Ability, Type, Battle
// (the last three dimmed "coming soon"; Battle holds the game rules, Friendship among them: snatcherLeo, "We can plan for the
// friendship stuff to be in Battle Editor"). Every one but Pokémon opens a "Coming soon" page for now.
const char* kEditors[] = {"pokemon", "move", "item", "trainer", "people", "type", "ability", "battle"};
const wchar_t* kSymbols[] = {L"01-pokemon.svg", L"02-move.svg", L"06-item.svg", L"07-trainer.svg", L"09-people.svg",
                             L"04-type.svg", L"03-ability.svg", L"21-battle-swords.svg"};
constexpr int kEditorCount = 8, kEditorItem = 2, kEditorType = 5, kEditorAbility = 6, kEditorBattle = 7;
// the rail's order and groups (-1 = a divider); dimmed from kRailDimFrom on
const int kRailOrder[] = {0, 1, 2, -1, 3, 4, -1, 6, 5, 7};
constexpr int kRailDimFrom = 7;  // position in kRailOrder

// the Pokémon page's tabs (snatcherLeo's v2 direction, 2026-11-18): only Overview and Looks have content so far
// (index 2, kTabLooks, is the Styles tab since 2026-11-22: snatcherLeo, "sub-tab should be "Styles"", placed early: it is drawn
// second, right after the Overview, kTabOrder; the indices stay, so tests and saved sessions keep their meaning)
const char* kTabs[] = {"studio.tab.overview", "studio.tab.moves", "studio.tab.styles", "studio.tab.sound",
                       "studio.tab.animations", "studio.tab.status"};
constexpr int kTabCount = 6, kTabOverview = 0, kTabMoves = 1, kTabLooks = 2, kTabSound = 3, kTabAnimations = 4;
// the tabs that show the shared 3D preview with a list beside it (Moves: the learnset; Animations: the model's own)
bool preview_tab(int tab) { return tab == kTabMoves || tab == kTabAnimations; }
// the stage the shown preview tab uses (0 Studio view, 1 Plain stage, 2 Arena): the Animations tab's own, or the Moves
// tab's (g.plain / g.arena). Defined after the window state, below (shown_stage)
int shown_stage();

// The Moves tab's learnset (version 1, 2026-11-19; studio/moves-tab-plan.md, mockup 25): one row per move the species
// learns, in three groups (0 level-up, 1 TM / HM, 2 egg), each with the move's record and the effect this Pokémon
// plays for it (its own version, or the general one: orre_move_effect). Read once per species (LearnSet below).
// the Moves tab's Reactions (group 3): the PKX animation slots shown, and the row ids they take (above every move id)
constexpr int kReactSlots[] = {8, 9, 10, 11, 16};
constexpr int kReactBase = 10000;
// The Animations tab (2026-11-22, mockup 28; snatcherLeo: "move the "Reactions" stuff out of the "Moves" sub-tab, put those in
// "Animations""): one row per animation in the model (group 4), its row id kAnimBase + the animation's index. The
// Reactions (group 3) are no longer listed on Moves; their code stays for the slot fallbacks they describe
constexpr int kAnimGroup = 4, kAnimBase = 20000;
// names marked as guesses in animation-catalog.md: their rows get a "name likely" hover line (2026-11-19)
bool react_name_likely(int slot) { return slot == 9 || slot == 16; }
struct LearnRow {
    int group = 0, move = 0, how = 0;  // how: the level (group 0), the machine index 0..57 (group 1), the slot (group 3)
    OrreMove mv{};
    std::wstring name;
    bool own = false;  // the per-Pokémon table names a version for this Pokémon's model
    bool has_effect = false;
    int slot = -1;  // the user's PKX animation slot the move's WZX names (-1: none)
    int anim = -1;  // group 4 (the Animations tab): the model's animation this row is
};
// The era bar starts at Gen 3 for every Pokémon (snatcherLeo, 2026-11-19, through C: "Gen 3/Colosseum should be where we
// start showcasing data, no reason to ever include Gen 1 and 2", then "So we can modify this for all Pokemon"): Gen 1 /
// 2 values can't be applied to a Gen 3 game (one Special stat, no abilities or natures, no Dark / Steel in Gen 1). A
// "History" view of the earliest numbers is a later idea.
// *Updated 2026-11-19:* Gen 3 and Colosseum are one button, "Gen 3 · Colosseum", first and shown by default (snatcherLeo,
// through C: "we could just have Gen 3 be the starting and merge", then "Yes, merge them"). Why: Colosseum's species
// and move tables are Ruby / Sapphire's for all 386 species and every regular move (pokemon-data.md, verified), and its
// battle code is Gen 3's plus Colosseum's own rules (battle-engine.md), which belong to the Battle editor's game rules,
// not the era bar. kEraFirstGen: the first numbered button after it (Gen 4); kEraGens: how many (Gen 4 to Gen 9).
constexpr int kEraFirstGen = 4, kEraGens = 9 - kEraFirstGen + 1;

// what the inspector shows (clicking a stat, the ability, a type or the model card)
enum SelKind { SEL_NONE = 0, SEL_STAT, SEL_ABILITY, SEL_TYPE, SEL_MODEL };

// ---- Studio's state: per window, and shared ----
// The multi-window plan (studio/multi-window-plan.md, step 1; snatcherLeo, 2026-11-18: "multi-tab workflow + multi window
// workflow"): everything one window shows or remembers lives in WinState; App adds what all windows share (the disc,
// the species list, fonts, language, theme, settings, the search vocabulary). App builds on WinState, so code keeps
// reading g.selected or g.page_tabs: those always mean the window being handled now. Each window keeps its own
// WinState; when a message arrives for another window, activate() swaps that window's state into g (one UI thread,
// so only one window is ever being handled at a time; the swap moves vectors and pointers, it copies nothing big).
// Windows; Linux / macOS: the same split, with the toolkit's window objects in place of HWNDs.
struct WinState {
    HWND hwnd = nullptr, search = nullptr;
    float scale = 1.0f;  // each window can sit on a monitor with its own DPI
    // Direct2D (multi-window plan step 1c): this window's device context on the shared Direct2D device, drawing into
    // the window's own DXGI swap chain. Pictures (bitmaps) made on any window's context belong to the shared device,
    // so every window can draw them. rt and dc are the same object (two names kept from the older code: rt for
    // drawing, dc for the SVG calls)
    ID2D1DeviceContext5* rt = nullptr;
    ID2D1DeviceContext5* dc = nullptr;
    IDXGISwapChain1* swap = nullptr;   // the window's swap chain (flip model)
    ID2D1Bitmap1* target = nullptr;    // the swap chain's back buffer as the context's target
    ID2D1SolidColorBrush* solid = nullptr;  // the one brush recoloured for each draw
    std::map<std::wstring, ID2D1SvgDocument*> svgs;  // the rail's symbols, made on dc
    // the global search (snatcherLeo, 2026-11-18: "Users can global search 2 ways - by having it be B with its own
    // drop-down, and/or CTRL K wth command palette. So, both, but both do the same exact thing - fuzzy searching"):
    // the top bar's field (gsearch) with its drop-down, and the Ctrl+K palette's field (psearch); one engine, one list
    HWND gsearch = nullptr, psearch = nullptr;
    bool palette_open = false, drop_open = false;
    // Report a problem's panel (4.3 part 3): open or not; its stage (0 the form, 1 saved); its two ticks (a screenshot
    // of each window, what's open); the note's edit box; the saved zip's file name (the stage-1 screen shows it)
    bool report_open = false;
    bool goback_open = false;  // (the update's "Go back" pop-up, 2026-11-23: draw_goback)
    int report_stage = 0;
    bool report_tick[2] = {true, true};
    HWND report_note = nullptr;
    std::wstring report_file, report_error;
    bool report_copied = false;  // the clipboard took the zip (the saved screen says "copied" only then)
    double report_mb = 0;        // the zip's size (shown, with a warning past about 10 MB)
    // the log's view of this window (4.3 part 3, the logging list's items 11 and 12): its number (w1, w2... in order of
    // opening, never reused in a run), and where it was last said to be ("now on: ..." lines)
    int win_no = 0;
    std::string place_logged;
    WINDOWPLACEMENT place_seen{};  // its place as last logged ("[window] w2 place: ..." lines, item 5)
    // the list's search box waits for the page's next paint before it shows (place_children; snatcherLeo, 2026-11-22: a white
    // bar where it sits while the Pokémon editor first loads)
    bool search_deferred = false;
    bool palette_target = false;  // the palette was opened to choose the Moves preview's target (Pokémon only)
    // a result: kind 0 a Pokémon (idx into all), 1 an editor (idx = editor); its tier (1 best .. 4) and score within
    // it; `via` what matched when it wasn't the name (a type, an ability, the category); `marks` the folded words to
    // colour in its name
    struct SRes { int kind; int idx; int tier; int score; std::wstring via; std::vector<std::wstring> marks; };
    std::vector<SRes> sres;
    int ssel = 0;
    std::wstring snote;  // what the search did with the words ("“pikchu” searched as “Pikachu”"), shown above the results
    // the list and the shown Pokémon
    std::vector<int> shown;  // indices into `all`, in National Dex order, after the search
    int selected = -1;       // index into `all`
    float scroll = 0;
    std::vector<Pic> mugs, pcs;
    // forms (snatcherLeo, 2026-11-18 through C: the proposal "Yes, with thumbnails"): the selected species' looks, each with
    // its mug shot as a thumbnail; `available` false = shown greyed (Deoxys's Formes that this disc doesn't have)
    struct Form { ID2D1Bitmap* bmp = nullptr; std::wstring label; bool available = true; };
    std::vector<Form> forms;
    int form = 0;
    int model_generation = 0;  // which model load this window is waiting for (a reply carries it)
    bool model_loading = false;
    std::wstring model_info;
    int editor = 0;
    bool inspector = true, dev_view = false;
    Viewer viewer;
    float motion_x = 0;  // where the motion button starts (its menu opens under it)
    float motion_y = 0;
    int tab = kTabOverview;
    int sel_kind = SEL_NONE, sel_value = 0;  // (v2's inspector subject; v3 explains on hover instead)
    float rscroll = 0, rscroll_max = 0;      // (v2: the right column's scroll)
    // v3 (snatcherLeo through 2026-11-18): page tabs (Home pinned, then one tab per open Pokémon; the list replaces the
    // current tab, Ctrl+click opens a new one), the page scrolling as a whole, hover cards instead of the inspector
    std::vector<int> page_tabs;  // indices into `all`
    int cur_tab = -1;            // -1 = Home
    float tab_scroll = 0;        // how far the tab strip is scrolled when the tabs don't all fit (DIPs)
    int tab_followed = -2;       // the shown tab the strip last scrolled into view (it follows only on a change, so
                                 // scrolling by hand isn't undone on the next paint)
    int tab_count_seen = 0;      // the tab count then (a new tab is scrolled into view too)
    // a new tab grows in (snatcherLeo, 2026-11-22, picking option A of the mockups: the tab widens from nothing, as in Chrome):
    // which tab and when it was made (GetTickCount64); -1 = none growing
    int tab_born = -1;
    unsigned long long tab_born_at = 0;
    // Back / Forward (snatcherLeo, 2026-11-18 through C: "Back and forward arrows yes"; per tab, the mouse's side buttons too):
    // each tab's visited Pokémon and where it is in that list; going back or forward moves without adding to it
    std::vector<std::vector<int>> tab_hist;
    std::vector<int> tab_pos;
    bool navigating = false;
    // several tabs selected (multi-window plan step 3; snatcherLeo: "CTRL click to selectively multiselect tabs, and SHIFT
    // click to just multi-select from a range"): one flag per tab beside page_tabs; the shown tab always counts as
    // selected; tab_anchor is where a Shift+click range starts
    std::vector<char> tab_sel;
    // the Pokémon this window showed last, so the rail's Pokémon icon can go back to it from a Home tab
    int last_mon = -1;
    // Each editor's last place in this window (snatcherLeo, 2026-11-22, asked whether every rail icon should "remember the
    // LAST PLACE we were in, if we ever entered that editor", then "Yes, build it"): the page it showed last and how far
    // that page was scrolled, noted on every paint (note_place). The rail's icon goes back there (Ctrl: in a new tab),
    // and the session keeps it. Index = editor (0 Pokémon .. 7 Battle); page INT_MIN = never visited. Today the Pokémon
    // editor's place is a Pokémon; the others are one page each until they're built, then their records' pages.
    struct Place { int page = INT_MIN; float scroll = 0; };
    Place places[kEditorCount];
    int settings_pane = 0;  // the Settings page's chosen pane (0 Appearance)
    int tab_anchor = -1;
    // the tab strip as last drawn (for dragging: where a pointer x lands): its start, one tab's step, its visible width
    float strip_x0 = 0, strip_step = 182, strip_avail = 0;
    float pscroll = 0, pscroll_max = 0;
    // the 3D view's window region as last set (place_children sets it only when this changes: see there)
    std::array<int, 11> view_region{};
    // the 3D view drawn into the page (ORRE_PAGE_VIEW trial, 2026-11-22): the texture the view's frames are copied into,
    // made here on the page's Direct3D 11 device, its keyed mutex and its Direct2D bitmap (ensure_shared)
    ID3D11Texture2D* share_tex = nullptr;
    IDXGIKeyedMutex* share_mutex = nullptr;
    ID2D1Bitmap1* share_bmp = nullptr;
    int share_w = 0, share_h = 0;
    // the page's own copy of the latest 3D frame (take_shared_frame), the one it draws, and the view's frame count when
    // the shared texture was last made (a newer count: the texture holds a real frame)
    ID3D11Texture2D* copy_tex = nullptr;
    ID2D1Bitmap1* copy_bmp = nullptr;
    int copy_w = 0, copy_h = 0;
    int copy_fw = 0, copy_fh = 0;  // the copied frame's own size: the copy's top-left part (the textures are sized in steps)
    int share_frames_at = 0;
    RECT view_px{};  // the visible part of the 3D view in the window, pixels (page mode's mouse: view_mouse)
    float mouse_x = -1, mouse_y = -1;  // DIPs, for the hover card
    std::wstring toast;                // a short note at the bottom (copied, coming soon), cleared by a timer
    bool shiny = false;                // the model card's Normal / Shiny switch
    // the Moves tab: the learnset group shown (0 level-up, 1 TM / HM, 2 egg), the chosen row (its group and move),
    // Plain stage on or off, and the rows as read for learn_for (an index into all)
    int learn_group = 0, learn_move = 0, learn_for = -1, learn_form = -1;
    // the Animations tab: the animation chosen (an index into the model's animations) and the Used by card's scroll
    int anim_sel = -1;  // (-1: the list's first, usually Idle; reset for each Pokémon, whose animations differ)
    int anim_for = -1;  // the Pokémon and form anim_sel belongs to (anim_row; -2: a restored session's, kept once)
    float used_scroll = 0;
    // the Animations tab's own stage (0 Studio view, 1 Plain stage, 2 Arena), kept apart from the Moves tab's (snatcherLeo,
    // 2026-11-22: "they should be different views, because, I don't want them mixed up"); Studio view until picked
    int an_stage = 0;
    // the tab the 3D view last played for (sync_preview): a switch stops the move playing and drops its effects, so
    // nothing played on one tab carries into another (snatcherLeo, 2026-11-22: Tail Whip's swirl kept playing on the
    // Animations tab, "not 100% separated"); fx_dirty asks sync_move_effects to set the effects up again
    int fx_tab = -1;
    bool fx_dirty = false;
    bool plain = false;
    // the Arena stage (snatcherLeo, 2026-11-19: "something really basic - but with a visible platform and area ... that Phenac
    // City arena"): Plain stage's camera with the move viewer's own room around the Pokémon (plain is true with it)
    bool arena = false;
    // the Overview's own stage (2026-11-22: its card's Studio view / Plain stage / Arena, kept apart from the Moves tab's
    // plain / arena; Studio view until picked there)
    int ov_stage = 0;
    // the two-turn moves' turn shown (the project's preview rule, 2026-11-20; the battle's order, fightWazaWzxTypeFuncTame,
    // verified 2026-11-21): 1 = the charge turn (part 3 alone), 2 = the attack turn (parts 1 and 2; the default, the
    // move's hit)
    int turn = 2;
    // the parts picker's choice for the moves whose part 3 plays at a battle event of its own (the project's preview rule,
    // 2026-11-20): 0 the move (its parts 1 and 2), 1 the follow-up (part 3 alone), 2 both in order
    int parts = 0;
    bool quiet_window = false;  // --quiet-window: a screenshot run's window all but invisible (see the window's creation)
    int era = 0;  // the era bar's picked button: 0 this disc (Gen 3 · Colosseum · XD), 1.. Gen 4..9 (not pickable yet)
    std::vector<LearnRow> learn;
    std::vector<std::pair<Rect, int>> hits;  // clickable areas this frame: id
    // the page drawn once (page_view: its base, everything under the 3D view and the overlays) and the clickable areas
    // of that draw; a 3D frame alone is stacked on it (compose) instead of the whole page being drawn again
    ID2D1Bitmap1* page_cache = nullptr;
    int cache_w = 0, cache_h = 0;
    std::vector<std::pair<Rect, int>> base_hits;
    int hover = -1;
};

struct App : WinState {
    HINSTANCE inst = nullptr;
    // the previous run ended unexpectedly (its log has no clean-exit line): a bar at the top of the first window offers a
    // report (4.3 part 3, the flow snatcherLeo approved), and a report made from it carries that previous log too
    bool crash_bar = false, report_prev = false;
    std::wstring exe_dir, ui_dir, lang_dir, fonts_dir, orreport_dir, disc_path;
    // The portable layout (step 4.1, snatcherLeo approved 2026-11-21; docs/port/portable-layout.md): root_dir is R\ when the
    // exe sits in a folder named "app" (R\app\orrestudio.exe), else empty (the development layout, build-pc\studio,
    // unchanged so the golden tests and worktree builds stay as they were). settings_dir holds studio.ini, logs_dir
    // the logs (R\data\settings\, R\data\system\logs\ portable; <exe>\user\, <exe>\logs\ in development).
    std::wstring root_dir, settings_dir, logs_dir;
    // OrrePort's own portable folder, where the shared settings.ini and the disc are: root_dir when Studio itself is
    // portable, or the --orreport folder when that is a portable folder (it has app\ or data\settings\settings.ini; a
    // development build started by run_studio.ps1 against snatcherLeo's play folder, 2026-11-21); else empty (the older
    // build-pc\firstlink layout: user\settings.ini, Colosseum\ROM\)
    std::wstring orreport_root;
    std::string lang_code;
    int theme_setting = 0;  // 0 follow Windows, 1 light, 2 dark (OrrePort's shared setting)
    bool update_check = true;   // OrrePort's settings.ini update_check (off: fully offline)
    bool update_auto = false;   // OrrePort's settings.ini update_auto_download (off: always ask first)
    int studio_theme = -1;  // the same values, picked in Studio's Settings (user\studio.ini); -1: follow OrrePort's
    std::wstring disc_pref;  // the disc path studio.ini holds (kept when the preferences are rewritten)
    bool cloaked = false;    // the first window is still cloaked, waiting for its first full page (reveal())
    int flash_test = 0;      // --flash-test N: flip Overview / Moves N times, reading the real screen (flash_test_*)
    int flash_watch = 0;     // --flash-watch S: read the screen for S seconds while the user clicks (no flips)
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();  // for the "window shown" log line
    int theme_forced = -1;
    Palette pal;
    // Direct2D / DirectWrite: the factories, the devices, fonts and text formats are shared by every window
    ID2D1Factory1* d2d = nullptr;
    ID3D11Device* d3d = nullptr;       // the Direct3D device Direct2D draws through (hardware, or WARP if none)
    ID2D1Device5* d2ddev = nullptr;    // the one Direct2D device: every window's context and every bitmap belong to it
    IDXGIFactory2* dxgi = nullptr;     // makes the windows' swap chains
    IDWriteFactory5* dw = nullptr;
    IDWriteFontCollection1* fonts = nullptr;
    IDWriteFontFallback* fallback = nullptr;
    std::wstring ui_family = L"Inter";
    std::map<std::pair<int, int>, IDWriteTextFormat*> formats;
    HFONT search_font = nullptr, gsearch_font = nullptr;
    HBRUSH search_brush = nullptr, win_brush = nullptr, card_brush = nullptr;
    // data: the disc and the species list, read once for every window
    OrreDisc* disc = nullptr;
    std::vector<Entry> all;
    std::map<int, int> by_index;  // internal species number -> index into `all` (links)
    // Styles (2026-11-22; snatcherLeo: "users can only select from different styles in here", the Styles tab; test "with Jynx
    // switching from normal to Japanese model"): a style is a Pokémon's whole presentation (model with its animations,
    // shiny, pictures, later cry and move versions; the project plan's Pokémon editor, "Looks: what the picker picks").
    // In 0.1 the styles are the game's own and, with the Japanese disc linked, the Japanese release's where it differs
    std::wstring region_disc_arg;  // --region-disc: the Japanese disc (tests; else ROM Other Regions\, as OrrePort keeps it)
    OrreDisc* jp_disc = nullptr;   // opened the first time a style needs it (japanese_disc)
    bool jp_tried = false;
    std::wstring jp_path;          // the Japanese disc's file, once opened
    std::map<int, int> style_of;   // species number -> the style viewed (0 the game's own, 1 the Japanese release)
    std::string load_error;
    std::vector<std::string> disc_lines;  // which discs were opened ("[disc]" log lines, repeated in a report's about.txt)
    bool disc_forced = false;  // the disc came from --disc, ORRE_DISC or (older layout) studio.ini: the ROM watch leaves it
    bool loading = false;
    bool load_stays = false;   // the disc being read is the first one added while Studio is open: stay on the page shown
    // command-line options (screenshot tests and comparisons)
    std::wstring screenshot;
    int select_at_start = 1;
    bool select_given = false;  // --select was passed (a test's start: the last session isn't restored then)
    bool test_session = false;  // --restore-session: a screenshot run restores the last session (it never saves one)
    std::vector<int> test_clicks;  // --click N,M,...: HIT_ ids clicked in order once the list is read (tests)
    std::vector<int> test_clicks_model;  // --click-after-model N,M,...: the same, once the first model has arrived
    std::wstring list_query;       // --list-query TEXT: typed into the list's search box once the list is read (tests)
    // the last session as read at start (studio-session.ini: restore_session); empty when there is none to restore
    std::map<std::string, std::string> session;
    int form_at_start = 0;  // --form N: open that form of the selected species (screenshot tests)
    bool shot_pending = false;
    bool no_3d = false;  // --no-3d: no GPU device at all (diagnosis, or a graphics driver that misbehaves)
    bool shot_model = false;  // --shot-model: the screenshot keeps only the 3D view (model reviews across species)
    int hold_frame = -1;  // --hold-frame N (screenshot tests)
    float yaw = 0;        // --yaw R: the model's turn in radians (screenshot tests, comparisons)
    bool has_yaw = false;
    // --game-camera FILE: the 3D view uses a camera copied from the game (a move-viewer shot's .camera.txt from
    // the project's runs: the view matrix, fov, aspect, and the model's position and rotation), for side-by-side checks
    std::wstring game_camera;
    // --shot-camera "ex,ey,ez,ix,iy,iz,fov": one frame of the game's battle camera (from its ORRE_BCAM_LOG) held while
    // the move plays, for the move sheets' side-by-sides (studio/tests/movebatch.py)
    std::wstring shot_camera;
    std::wstring test_query;   // --search-query: typed into the global search before a screenshot
    bool test_palette = false;   // --palette: into the Ctrl+K palette instead of the top field
    int second_window = 0;       // --second-window N: also open a second window showing species N, and capture that one
    // tab tests (the multi-window plan, steps 3-5): --tabs "1,4,7" opens those species in tabs; --tab-select "0,2"
    // selects those tabs; --tab-move "0,1>3" moves those tabs to place 3 (as a drag along the strip would)
    std::wstring test_tabs, test_tab_select, test_tab_move;
    bool test_play = false;  // --play: the Moves tab plays its move once the model is in
    // --flip-turns MS: once the move plays, click Turn 1 / Turn 2 every MS milliseconds, 20 times, then close (the crash
    // snatcherLeo hit on 2026-11-22 switching Solarbeam's turns while it played; a reproduction switch, like --flash-test)
    int flip_turns = 0, flips_done = 0;
    bool crash_test = false;  // --crash-test (run(), after the log opens)
    // The Moves preview's target (snatcherLeo, 2026-11-19: "allow a user to sub-in a Pokemon from their database to be the
    // "test enemy" ... Or, we still allow a box by default", then "the target picker for Pokemon you can pick is based
    // on your database ... so you can simulate how a Pokemon would react to a particle"): the box (-1, the default) or
    // a species from the list, which takes the hit with its own hit animation, attach points and its own version of
    // the move's target half. One choice for every window, kept in studio.ini ("target"); --target N sets it for
    // screenshot tests (the comparisons with the game's move sweep). target_recent: the last few chosen, for the menu
    int target_species = -1, target_loaded = -1;
    std::vector<int> target_recent;
    int any_move = 0;  // --any-move N: a move added to the level-up list for tests (0: none)
    // the Pokémon's own particle effects, on or off for every window and tab (snatcherLeo, 2026-11-19: "we can toggle those
    // off in the overview tab ... and that change gets reflected when we see them in action in the moves"); kept in
    // Studio's preferences (studio.ini "particles")
    bool particles_on = true;
    // Game lighting: the game's arena lights in the 3D view (release step 2, 2026-11-19); on by default ("the editor's
    // promise is to show the game as it is"); off = Studio's even light. Saved in studio.ini ("lighting")
    bool game_light = true;
    // Battle camera: a move with its effects is filmed as the battle films it (release step 4, 2026-11-19); off = Studio's
    // own camera stays, free to turn and zoom while the move plays (snatcherLeo: "can users turn off battle camera if they
    // wanted to?"). On by default; saved in studio.ini ("battle_camera")
    bool battle_cam = true;
    // the Moves tab's timeline as last drawn (its ruler's ends and the animation's frames) and whether it is being
    // dragged (the window holds the mouse until the button is released)
    // the preview rate for every 3D view: 0 the game's (30 frames a second), else Hz with smoothing; studio.ini "rate"
    int preview_rate = 0;
    float tl_x0 = 0, tl_x1 = 0;
    int tl_frames = 0;
    bool scrubbing = false;
};

App g;

// Every window's state but the one in g: windows[i] holds window i's WinState while another window is active (its
// slot is empty while it is the active one). One window today; step 2 opens more.
struct WindowSlot { HWND hwnd = nullptr; WinState parked; bool active = false; };
std::vector<WindowSlot> g_windows;
// makes hwnd's window the one g describes (a no-op when it already is); returns false for a window Studio doesn't know
bool activate(HWND hwnd) {
    if (hwnd == g.hwnd || !hwnd) return true;
    WindowSlot* to = nullptr;
    for (auto& w : g_windows) if (w.hwnd == hwnd) to = &w;
    if (!to) return false;
    for (auto& w : g_windows)
        if (w.active) { std::swap(static_cast<WinState&>(g), w.parked); w.active = false; }
    std::swap(static_cast<WinState&>(g), to->parked);
    to->active = true;
    log_set_window(g.win_no);  // (the log's lines from here on carry this window's tag)
    return true;
}
int g_windows_opened = 0;  // windows opened in this run (each window's number: WinState::win_no)
int g_windows_closing_no = 0;  // the number of the window being closed (WM_DESTROY's closing line)
// What holds Studio in one of Windows' own loops right now (4.3 part 3, the logging list's item 6): a window being moved
// or resized, a menu, the disc dialog; nullptr otherwise. A stall seen then is likely Windows' loop, not a bug, and the
// hang lines and a crash report say which. Windows' modal loops; Linux / macOS have their toolkit's equivalents (likely).
const char* g_modal = nullptr;
// The updater's state as Studio last read it (4.4; pc_update_poll, from the 1 s timer): what the rail's Home dot, Home's
// update line and Settings › About show. g_upd_on: the updater started (a portable or --orreport OrrePort folder, not a
// test run). g_exit_code: PC_UPDATE_EXIT_RESTART after "Restart now" or "Go back", for the launcher (run's return value)
PcUpdateStatus g_upd{};
bool g_upd_on = false, g_upd_notes = false;
// "Go back"'s pop-up (*changed 2026-11-23*, snatcherLeo after his update hand check: "it should be a pop-up instead, and it asks
// users are they sure (and a new warning message about this)"; the confirm was inside About's row before): open or not
// is per window (g.goback_open), like Report a problem's panel; draw_goback draws it.
// a dotted version ("0.1.10") newer than another ("0.1.9"), number by number; for hiding "Go back" when the version kept
// is the newer one (after a Go back, previous\ holds the version just left: going "back" to it reads wrong, and the
// update line already offers it; snatcherLeo's hand check showed "Go back to 0.1.1" on 0.1.0)
bool version_newer(const char* a, const char* b) {
    while (*a || *b) {
        char* ea = nullptr;
        char* eb = nullptr;
        const unsigned long na = std::strtoul(a, &ea, 10), nb = std::strtoul(b, &eb, 10);
        if (na != nb) return na > nb;
        a = *ea == '.' ? ea + 1 : ea;
        b = *eb == '.' ? eb + 1 : eb;
        if (a == ea && b == eb) break;  // (anything else in the text: no further numbers to compare)
    }
    return false;
}
// The other program (4.4, pc_apps): the OrrePort folder both use (UTF-8; "" when Studio runs without one: nothing to
// connect to), whether OrrePort (the hub, or its game) is open from it, and whether its game runs (read once a second)
std::string g_apps_root;
bool g_hub_open = false, g_game_running = false;
int g_exit_code = 0;
// the dot on the rail's Home icon: an update found, downloading or ready (a failed check alone shows none: offline is
// normal; a failed download of a found update does)
bool upd_attention() {
    if (!g_upd_on) return false;
    const PcUpdateState s = g_upd.state;
    return s == PC_UPDATE_AVAILABLE || s == PC_UPDATE_DOWNLOADING || s == PC_UPDATE_READY || (s == PC_UPDATE_FAILED && g_upd.latest[0]);
}
volatile unsigned g_heartbeat = 0;  // counted up once a second by the UI thread (perf_timer); pc_crash's watchdog reads it
bool g_hang_test = false;           // --hang-test: the UI thread stops for 12 s once, 3 s in (tests the hang watch)

// The startup timeline (4.3 part 3, the logging list's item 7): each step's time since Studio's process began, logged
// as one "[startup]" line when the first page shows (startup_done), so a slow start says which step was slow. Windows'
// process creation time (GetProcessTimes); Linux / macOS: the process's start from /proc/self/stat or sysctl (likely).
std::vector<std::pair<const char*, double>> g_startup;
bool g_startup_done = false;
double ms_since_process_start() {
    FILETIME c, e, k, u, now;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return 0;
    GetSystemTimeAsFileTime(&now);
    auto v = [](const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; };
    return (double)(v(now) - v(c)) / 10000.0;
}
void startup_mark(const char* step) {
    if (!g_startup_done) g_startup.push_back({step, ms_since_process_start()});
}
void startup_done() {
    if (g_startup_done) return;
    startup_mark("first page shown");
    g_startup_done = true;
    std::string line;
    double prev = 0, slow = 0;
    const char* slow_step = "";
    for (const auto& [step, ms] : g_startup) {
        char b[96];
        std::snprintf(b, sizeof b, "%s%s at %.0f ms", line.empty() ? "" : ", ", step, ms);
        line += b;
        if (ms - prev > slow) { slow = ms - prev; slow_step = step; }
        prev = ms;
    }
    logf(Level::Info, "startup", "ready %.0f ms after the program started: %s; the longest wait was before \"%s\" (%.0f ms)", prev,
         line.c_str(), slow_step, slow);
}

// Where a window is (4.3 part 3, the logging list's item 5, second half): its size and place on the desktop, maximised
// or not, and which display it is on (numbered as the "[system] display N" lines number them). Logged when the window
// opens and whenever that changes (a drag or resize ends, maximise, restore), so a drawing problem on one screen or at
// one size can be matched to it. Windows (GetWindowPlacement, MonitorFromWindow); Linux / macOS: the toolkit's window
// geometry and screen (likely).
int display_number(HMONITOR m) {
    struct Walk { HMONITOR want; int n, found; } w{m, 0, 0};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR h, HDC, LPRECT, LPARAM p) -> BOOL {
        auto* k = reinterpret_cast<Walk*>(p);
        ++k->n;
        if (h == k->want) { k->found = k->n; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&w));
    return w.found;
}
void log_window_place(const char* why) {
    WINDOWPLACEMENT wp{sizeof wp};
    if (!g.hwnd || !GetWindowPlacement(g.hwnd, &wp)) return;
    RECT r;
    GetWindowRect(g.hwnd, &r);
    const bool same = wp.showCmd == g.place_seen.showCmd && std::memcmp(&r, &g.place_seen.rcNormalPosition, sizeof r) == 0;
    if (same) return;
    g.place_seen.length = sizeof wp;  // (non-zero: logged once, so WM_SIZE may log changes from now on)
    g.place_seen.showCmd = wp.showCmd;
    g.place_seen.rcNormalPosition = r;  // (the window's rectangle as it is now, maximised or not)
    const char* state = wp.showCmd == SW_SHOWMAXIMIZED ? "maximised" : wp.showCmd == SW_SHOWMINIMIZED ? "minimised" : "normal";
    logf(Level::Info, "window", "place (%s): %ld x %ld at %ld,%ld, %s, on display %d, scaling %.0f%%", why, r.right - r.left,
         r.bottom - r.top, r.left, r.top, state, display_number(MonitorFromWindow(g.hwnd, MONITOR_DEFAULTTONEAREST)), g.scale * 100);
}

// A tab being dragged (multi-window plan steps 4 and 5; snatcherLeo: "click and drag a tab into another window ... move tab
// order around ... drag out those selections into a new window as well. Just like Chrome"). One drag at a time for
// all of Studio: pressed on a tab of `src`; `active` once the pointer has moved a few pixels; then either reordering
// inside src's strip (insert = the place among the other tabs), docking onto another window's strip (dock, dock_at),
// or torn off (a floating preview follows the pointer; dropping opens a new window there).
// *Updated 2026-11-22 (snatcherLeo, 4.3 part 2, after trying the mockups' options built: A's live window, then B's outline,
// then "Option mockup C woudl be better. No window, just the tab" and "but no dropzones, still use the vertical line
// yeah"):* torn-off tabs ride under the pointer as a tab-shaped chip (`preview`, with how many when several), and over another
// window's tab strip that strip shows a vertical line where they land. Letting go elsewhere opens a new window with its
// first tab under the pointer (or moves the window, when all its tabs are dragged).
struct TabDrag {
    bool pressed = false, active = false, torn = false, clear_on_up = false;
    HWND src = nullptr, dock = nullptr, preview = nullptr;
    POINT win_off{};              // the pointer's offset from src's top left when pressed (screen pixels: moving src)
    POINT new_off{};              // the pointer's offset from a new or moved window's top left (screen pixels; the drop)
    int press_tab = -1, insert = 0, dock_at = 0;
    POINT press_pt{};             // where it was pressed (screen pixels)
    float grab_dx = 0, mouse_x = 0;  // where on its tab it was grabbed, and the pointer's x in src (DIPs)
    std::vector<int> tabs;        // the tabs dragged (indices into src's tabs, ascending)
    std::vector<std::wstring> names;
};
TabDrag g_drag;

// A new window (multi-window plan step 2): Ctrl+Shift+N (empty, showing Home), or "Move to new window" (with tabs and
// their history). It opens over the window it came from, a little down and to the right, at the same size; when
// CreateWindowExW returns, g describes the new window. The tabs travel through g_new_window to its WM_CREATE.
// (at: where the new window's top left goes, in screen pixels: a torn-off tab's window opens under the pointer)
struct NewWindowTabs {
    std::vector<int> tabs;
    std::vector<std::vector<int>> hist;
    std::vector<int> pos;
    int cur = 0;
    bool has_at = false;
    POINT at{};
};
NewWindowTabs* g_new_window = nullptr;
HWND open_window(NewWindowTabs init) {
    RECT r;
    GetWindowRect(g.hwnd, &r);
    const int off = (int)std::lround(32 * g.scale);
    const int nx = init.has_at ? init.at.x : r.left + off, ny = init.has_at ? init.at.y : r.top + off;
    g_new_window = &init;
    HWND h = CreateWindowExW(0, L"OrrePortStudio", tx("studio.app.title").c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             nx, ny, r.right - r.left, r.bottom - r.top, nullptr, nullptr, g.inst, nullptr);
    g_new_window = nullptr;
    if (h) ShowWindow(h, g.screenshot.empty() ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE);
    logf(Level::Info, "window", "new window (%zu open), %zu tabs", g_windows.size(), init.tabs.size());
    if (h) log_window_place("opened");
    int tabs_all = 0;  // (the closing summary's "at most ... tabs open" counts the new window's tabs at once)
    for (const WindowSlot& w : g_windows) tabs_all += (int)(w.active ? g.page_tabs.size() : w.parked.page_tabs.size());
    log_note_open((int)g_windows.size(), tabs_all);
    return h;
}

// ---- helpers ----
float px(float dip) { return std::round(dip * g.scale); }

// the window's one brush, recoloured for each draw (it belongs to the window's render target: recreated with it)
ID2D1SolidColorBrush* brush(const D2D1_COLOR_F& c) {
    if (!g.solid) g.rt->CreateSolidColorBrush(c, &g.solid);
    else g.solid->SetColor(c);
    return g.solid;
}

D2D1_COLOR_F alpha(D2D1_COLOR_F c, float a) { c.a = a; return c; }
D2D1_COLOR_F hexc(uint32_t v) { return D2D1::ColorF(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f); }

IDWriteTextFormat* format(float size, DWRITE_FONT_WEIGHT weight) {
    const auto key = std::make_pair((int)(size * 10), (int)weight);
    auto it = g.formats.find(key);
    if (it != g.formats.end()) return it->second;
    IDWriteTextFormat* f = nullptr;
    g.dw->CreateTextFormat(g.ui_family.c_str(), g.fonts, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"",
                           &f);
    if (f) {
        IDWriteTextFormat1* f1 = nullptr;
        if (g.fallback && SUCCEEDED(f->QueryInterface(__uuidof(IDWriteTextFormat1), reinterpret_cast<void**>(&f1)))) {
            f1->SetFontFallback(g.fallback);
            f1->Release();
        }
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        // Inter's optical sizes (the typography pass, snatcherLeo through 2026-11-18): the shipped Inter-Variable.ttf has
        // an opsz axis (14-32), so DirectWrite picks the display shapes for big titles and the text shapes for small
        // text from the font size. Windows 10 1803+ (IDWriteTextFormat3); Linux / macOS: FreeType / Core Text set
        // opsz from the size the same way (likely)
        IDWriteTextFormat3* f3 = nullptr;
        if (SUCCEEDED(f->QueryInterface(__uuidof(IDWriteTextFormat3), reinterpret_cast<void**>(&f3)))) {
            f3->SetAutomaticFontAxes(DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
            f3->Release();
        }
        IDWriteInlineObject* ellipsis = nullptr;
        g.dw->CreateEllipsisTrimmingSign(f, &ellipsis);
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        f->SetTrimming(&trim, ellipsis);
        if (ellipsis) ellipsis->Release();
    }
    g.formats[key] = f;
    return f;
}

// Text styles a few places need (the typography pass, 2026-11-18): tabular figures (Inter's tnum: digits of one
// width, so stats, totals, numbers and percentages line up) and letter spacing in em (the small capital headings,
// 0.17 em). Set for a scope with TextStyle; text() and text_width() honour them. DirectWrite (Windows); Linux / macOS:
// HarfBuzz features and the toolkit's letter spacing.
struct TextStyleState { bool tnum = false; float track_em = 0; };
TextStyleState g_style;
struct TextStyle {
    TextStyleState saved;
    TextStyle(bool tnum, float track_em = 0) : saved(g_style) { g_style.tnum = tnum; g_style.track_em = track_em; }
    ~TextStyle() { g_style = saved; }
};
// a layout with the current style applied (the caller releases it)
IDWriteTextLayout* styled_layout(const std::wstring& s, IDWriteTextFormat* f, float size, float w, float h) {
    IDWriteTextLayout* l = nullptr;
    g.dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, w, h, &l);
    if (!l) return nullptr;
    const DWRITE_TEXT_RANGE all{0, (UINT32)s.size()};
    if (g_style.tnum) {
        IDWriteTypography* ty = nullptr;
        if (SUCCEEDED(g.dw->CreateTypography(&ty))) {
            ty->AddFontFeature({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1});
            l->SetTypography(ty, all);
            ty->Release();
        }
    }
    if (g_style.track_em != 0) {
        IDWriteTextLayout1* l1 = nullptr;
        if (SUCCEEDED(l->QueryInterface(__uuidof(IDWriteTextLayout1), reinterpret_cast<void**>(&l1)))) {
            l1->SetCharacterSpacing(0, size * g_style.track_em, 0, all);
            l1->Release();
        }
    }
    return l;
}

void text(const std::wstring& s, float x, float y, float w, float h, float size, const D2D1_COLOR_F& c,
          DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING,
          bool wrap = false) {
    IDWriteTextFormat* f = format(size, weight);
    if (!f) return;
    f->SetTextAlignment(align);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    f->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (g_style.tnum || g_style.track_em != 0) {
        if (IDWriteTextLayout* l = styled_layout(s, f, size, std::max(1.0f, w), std::max(1.0f, h))) {
            g.rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
            g.rt->DrawTextLayout(D2D1::Point2F(x, y), l, brush(c), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            g.rt->PopAxisAlignedClip();
            l->Release();
        }
        return;
    }
    g.rt->DrawTextW(s.c_str(), (UINT32)s.size(), f, D2D1::RectF(x, y, x + w, y + h), brush(c),
                    (D2D1_DRAW_TEXT_OPTIONS)(D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT | D2D1_DRAW_TEXT_OPTIONS_CLIP));
}

float text_width(const std::wstring& s, float size, DWRITE_FONT_WEIGHT weight) {
    IDWriteTextFormat* f = format(size, weight);
    if (!f) return 0;
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextLayout* l = styled_layout(s, f, size, 4000, 100);
    if (!l) return 0;
    DWRITE_TEXT_METRICS m{};
    l->GetMetrics(&m);
    l->Release();
    return m.widthIncludingTrailingWhitespace;
}

// Shared baselines (typography pass): a line's baseline and height for a size and weight, so text of different sizes
// on one line sits on one baseline instead of being centred in boxes. text_base draws with its baseline at `base`.
struct LineMetric { float baseline, height; };
LineMetric line_metric(float size, DWRITE_FONT_WEIGHT weight) {
    static std::map<std::pair<int, int>, LineMetric> cache;
    const auto key = std::make_pair((int)(size * 10), (int)weight);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    LineMetric r{size * 0.8f, size * 1.2f};
    IDWriteTextLayout* l = nullptr;
    g.dw->CreateTextLayout(L"Ag", 2, format(size, weight), 1000, 1000, &l);
    if (l) {
        DWRITE_LINE_METRICS lm{};
        UINT32 n = 0;
        if (SUCCEEDED(l->GetLineMetrics(&lm, 1, &n)) && n) r = {lm.baseline, lm.height};
        l->Release();
    }
    cache[key] = r;
    return r;
}
void text_base(const std::wstring& s, float x, float base, float w, float size, const D2D1_COLOR_F& c,
               DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
    const LineMetric m = line_metric(size, weight);
    text(s, x, base - m.baseline, w, m.height, size, c, weight, align);
}
// where a box of height h must start so text of this size, centred in it by text(), sits on `base`
float box_top_for_base(float base, float h, float size, DWRITE_FONT_WEIGHT weight) {
    const LineMetric m = line_metric(size, weight);
    return base - ((h - m.height) / 2 + m.baseline);
}

void fill(float x, float y, float w, float h, const D2D1_COLOR_F& c) { g.rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush(c)); }
void round_fill(float x, float y, float w, float h, float r, const D2D1_COLOR_F& c) {
    g.rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), r, r), brush(c));
}
void round_line(float x, float y, float w, float h, float r, const D2D1_COLOR_F& c, float t = 1) {
    g.rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + t / 2, y + t / 2, x + w - t / 2, y + h - t / 2), r, r), brush(c), t);
}

// RGBA (straight alpha, from the core) to a Direct2D bitmap (premultiplied BGRA)
ID2D1Bitmap* make_bitmap(const uint8_t* rgba, int w, int h) {
    std::vector<uint8_t> b((size_t)w * h * 4);
    for (size_t i = 0; i < b.size(); i += 4) {
        const unsigned a = rgba[i + 3];
        b[i] = (uint8_t)(rgba[i + 2] * a / 255);
        b[i + 1] = (uint8_t)(rgba[i + 1] * a / 255);
        b[i + 2] = (uint8_t)(rgba[i] * a / 255);
        b[i + 3] = (uint8_t)a;
    }
    ID2D1Bitmap* bmp = nullptr;
    g.rt->CreateBitmap(D2D1::SizeU(w, h), b.data(), w * 4,
                       D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bmp);
    return bmp;
}

// pixel art stays crisp: nearest-neighbour when enlarged
void draw_bitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h) {
    if (bmp) g.rt->DrawBitmap(bmp, D2D1::RectF(x, y, x + w, y + h), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
}

// A mug shot's size on screen, in DIPs, near `target`: a whole multiple of its 42 pixels in device pixels, so every
// one of the game's pixels becomes the same n x n block (every mug shot on the disc is 42 x 42: verified 2026-11-18,
// all 386). Why: at 66 DIPs some pixels came out one screen pixel wide and some two. snatcherLeo, 2026-11-18: "if so, we can
// resize that square to properly match it in-app, just for polish". Windows (DIP scale); Linux / macOS: the same rule
// with their own scale factor.
float mug_size(float target) {
    const float n = std::max(1.0f, std::floor(target * g.scale / 42.0f + 0.5f));
    return 42.0f * n / g.scale;
}
// A small mug shot (the list, tabs, Forms card, evolution cards) at the whole multiple of 42 device pixels nearest a
// `box`-DIP square, centred in that square and placed on whole device pixels (snatcherLeo, 2026-11-18: "We can then have our
// app be perfectly 42 x 42, for the mugshot icons"). Only where even one 42-pixel copy would overflow the box by more
// than a third (the tab bar's 20-DIP icon at 100 % scaling) does it fall back to the box's own size.
// A picture filling a rounded square: drawn through a bitmap brush, so the corners clip the picture and nothing behind
// shows around it; nearest-neighbour, so the game's pixels stay crisp. (Direct2D bitmap brush; Linux / macOS: the
// toolkit's clip path with an image pattern.)
void fill_rounded_picture(ID2D1Bitmap* bmp, float x, float y, float size, float radius, float opacity = 1.0f) {
    if (!bmp) return;
    ID2D1BitmapBrush* bb = nullptr;
    const D2D1_BITMAP_BRUSH_PROPERTIES bp = D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP,
                                                                        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    if (FAILED(g.rt->CreateBitmapBrush(bmp, bp, &bb))) return;
    const D2D1_SIZE_F bs = bmp->GetSize();
    bb->SetTransform(D2D1::Matrix3x2F::Scale(size / bs.width, size / bs.height) * D2D1::Matrix3x2F::Translation(x, y));
    bb->SetOpacity(opacity);
    g.rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + size, y + size), radius, radius), bb);
    bb->Release();
}
// Where a mug shot is an icon (the list, search results, tabs, the Forms card, evolution cards, the header), its
// corners are rounded, as an app frames its avatars. Where the picture itself is the subject (the Looks tab, exports,
// any raw view), it stays the exact 42 x 42 square. The pixels are the disc's either way: the rounding only frames
// them (snatcherLeo, 2026-11-19, through C: "I would prefer to have it look nicer in-app, personally, and have rounded
// corners, but would that be non-genuine in what the image actually is?", then "Yeah, tell Studio I'd like that idea").
// The disc's mug shots are opaque, on near-black backgrounds (verified 2026-11-19: Eevee, Jolteon, Umbreon).
float mug_radius(float size) { return std::max(3.0f, size * 0.2f); }
void draw_mug(ID2D1Bitmap* bmp, float x, float y, float box, float opacity = 1.0f) {
    if (!bmp) return;
    const float s = mug_size(box);
    if (s > box * 1.34f) { fill_rounded_picture(bmp, x, y, box, mug_radius(box), opacity); return; }
    const float ox = std::round((x + (box - s) / 2) * g.scale) / g.scale, oy = std::round((y + (box - s) / 2) * g.scale) / g.scale;
    fill_rounded_picture(bmp, ox, oy, s, mug_radius(s), opacity);
}
// The mug shot filling its frame exactly (the header): rounded as above, placed on whole device pixels, with the
// frame's line on top.
void draw_mug_framed(ID2D1Bitmap* bmp, float x, float y, float size, float radius, const D2D1_COLOR_F& line) {
    x = std::round(x * g.scale) / g.scale;
    y = std::round(y * g.scale) / g.scale;
    fill_rounded_picture(bmp, x, y, size, radius);
    round_line(x, y, size, size, radius, line);
}

// an SVG file drawn at (x, y), size x size DIPs, with colours swapped first (cached per file, colours and size)
void draw_svg(const std::wstring& file, const std::vector<std::pair<std::string, std::string>>& swaps, float x, float y, float size) {
    if (!g.dc) return;
    std::wstring key = file + L"|" + std::to_wstring((int)size);
    for (const auto& [a, b] : swaps) key += widen("|" + a + ">" + b);
    auto it = g.svgs.find(key);
    if (it == g.svgs.end()) {
        std::string svg = load_text_file(file);
        for (const auto& [a, b] : swaps)
            for (size_t p = 0; (p = svg.find(a, p)) != std::string::npos; p += b.size()) svg.replace(p, a.size(), b);
        svg_fit(svg);
        ID2D1SvgDocument* doc = nullptr;
        if (IStream* st = SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), (UINT)svg.size())) {
            g.dc->CreateSvgDocument(st, D2D1::SizeF(size, size), &doc);
            st->Release();
        }
        it = g.svgs.emplace(key, doc).first;
    }
    if (!it->second) return;
    D2D1_MATRIX_3X2_F old;
    g.dc->GetTransform(&old);
    g.dc->SetTransform(D2D1::Matrix3x2F::Translation(x, y) * old);
    g.dc->DrawSvgDocument(it->second);
    g.dc->SetTransform(old);
}

std::string hex_colour(const D2D1_COLOR_F& c) {
    char hex[8];
    std::snprintf(hex, sizeof hex, "#%02X%02X%02X", (int)std::lround(c.r * 255), (int)std::lround(c.g * 255), (int)std::lround(c.b * 255));
    return hex;
}

// an editor symbol (docs/design/brand/studio-symbols, drawn with currentColor) in a colour
void draw_symbol(int editor, float x, float y, float size, const D2D1_COLOR_F& c) {
    draw_svg(g.ui_dir + L"\\symbols\\" + kSymbols[editor], {{"currentColor", hex_colour(c)}}, x, y, size);
}
// The Settings gear, drawn in the editor symbols' style (a 24-unit grid, a round-cornered outline, the inside filled
// at 30 %), since the symbol set has no gear yet (24-advanced is a terminal prompt). Eight teeth, a hole in the middle;
// its outline is 2.2 units, a little lighter than the symbols' 2.75, so the small teeth stay open. Direct2D path geometry; Linux / macOS: the toolkit's path drawing, the same shape.
// The gear's colour wherever it stands for Settings (the Settings tab, Home's Settings button, the tab menu): halfway
// between the text colour and the dimmer second text colour (snatcherLeo, 2026-11-23: "Let's make this settings icon, darker
// slightly ... just a choice"; earlier the same day, the tab's and the button's were made to match)
D2D1_COLOR_F gear_colour() {
    const D2D1_COLOR_F a = g.pal.text, b = g.pal.text2;
    return D2D1::ColorF((a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2, 1.0f);
}
void draw_gear(float x, float y, float size, const D2D1_COLOR_F& c) {
    if (!g.d2d || !g.rt) return;
    const float u = size / 24.0f, cx = x + 12 * u, cy = y + 12 * u;
    ID2D1PathGeometry* geo = nullptr;
    if (FAILED(g.d2d->CreatePathGeometry(&geo))) return;
    ID2D1GeometrySink* sink = nullptr;
    if (SUCCEEDED(geo->Open(&sink))) {
        constexpr int kTeeth = 8;
        const float r_in = 7.4f * u, r_out = 9.8f * u, pi = 3.14159265f;
        bool first = true;
        for (int i = 0; i < kTeeth; ++i) {
            const float a = (float)i * 2 * pi / kTeeth;
            // each tooth: up its leading side, across its top, down its trailing side (angles in radians around a)
            const float pts[4][2] = {{r_in, a - 0.30f}, {r_out, a - 0.17f}, {r_out, a + 0.17f}, {r_in, a + 0.30f}};
            for (const auto& p : pts) {
                const D2D1_POINT_2F q = D2D1::Point2F(cx + p[0] * std::cos(p[1]), cy + p[0] * std::sin(p[1]));
                if (first) { sink->BeginFigure(q, D2D1_FIGURE_BEGIN_FILLED); first = false; }
                else sink->AddLine(q);
            }
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();
        sink->Release();
    }
    ID2D1StrokeStyle* round = nullptr;
    g.d2d->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                         D2D1_LINE_JOIN_ROUND),
                             nullptr, 0, &round);
    g.solid->SetColor(alpha(c, 0.3f * c.a));
    g.rt->FillGeometry(geo, g.solid);
    g.solid->SetColor(c);
    g.rt->DrawGeometry(geo, g.solid, 2.2f * u, round);
    g.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.0f * u, 3.0f * u), g.solid, 2.2f * u, round);
    if (round) round->Release();
    geo->Release();
}

// OrrePort's O, recoloured to sage (Studio's brief: #C8452E -> #8FA98A, #642217 -> #485445)
void draw_o(float x, float y, float size) {
    draw_svg(g.ui_dir + L"\\icons\\" + (g.pal.dark ? L"orreport-icon-rust.svg" : L"orreport-icon-rust-light-o.svg"),
             {{"#C8452E", "#8FA98A"}, {"#642217", "#485445"}}, x, y, size);
}

// while set (w > 0), hit spots are trimmed to this rectangle: the tab strip's visible part (a tab scrolled half out
// of view is only clickable where it shows)
Rect g_hit_clip{0, 0, 0, 0};
std::vector<bool> g_tab_cut;  // per Pokémon tab: its name was cut with "…" (the full name then shows on hover)
void hit(const Rect& r, int id) {
    if (g_hit_clip.w > 0) {
        const float x0 = std::max(r.x, g_hit_clip.x), x1 = std::min(r.x + r.w, g_hit_clip.x + g_hit_clip.w);
        if (x1 <= x0) return;
        g.hits.emplace_back(Rect{x0, r.y, x1 - x0, r.h}, id);
        return;
    }
    g.hits.emplace_back(r, id);
}

// ids for clickable areas
enum { HIT_RAIL = 100, HIT_ROW = 1000, HIT_ROW_END = 4999, HIT_EVO = 5000, HIT_EVO_END = 5999, HIT_INSPECTOR = 10, HIT_SPIN = 11,
       HIT_RESET = 12, HIT_CHOOSE_DISC = 13, HIT_MOTION = 14, HIT_MODEL = 15, HIT_OPEN_EDITOR = 16, HIT_TAB = 200,
       HIT_STAT = 300, HIT_ABILITY = 310, HIT_TYPE = 400, HIT_FORM = 6000, HIT_FORM_END = 6099,
       // v3: the hidden-ability row, training rows, era options, matchup chips (their own hover spots, same click as
       // a type), evolution method links, the model card's switches, the header's buttons, the page tabs
       HIT_ABILITY_HIDDEN = 312, HIT_TRAIN = 320, HIT_ERA = 340, HIT_MATCH = 420, HIT_EVO_METHOD = 6200, HIT_EVO_METHOD_END = 6299,
       HIT_IDLE = 19, HIT_STANDING = 20, HIT_NORMAL = 21, HIT_SHINY = 22, HIT_COPY = 23, HIT_CREATE_MOD = 24, HIT_PARTICLES = 25,
       HIT_LIGHTING = 26, HIT_BATTLECAM = 27, HIT_TARGET = 28,
       HIT_PTAB = 7000, HIT_PTAB_CLOSE = 7100, HIT_HOME = 7200, HIT_PTAB_ADD = 7201, HIT_BACK = 7202, HIT_FORWARD = 7203,
       HIT_TAB_MENU = 7204, HIT_TAB_FIRST = 7205, HIT_TAB_LAST = 7206,
       // Settings (2026-11-19): the gear on Home, the panes on the Settings page's left, the theme's three choices
       HIT_SETTINGS = 7300, HIT_SET_PANE = 7310, HIT_THEME_OPT = 7320,
       // Settings' Help and reports pane (4.3 part 3): Report a problem, open the reports folder, open the logs folder
       HIT_SET_REPORT = 7330, HIT_SET_REPORTS_DIR = 7331, HIT_SET_LOGS_DIR = 7332,
       // Report a problem's panel: the dimmed window under it, its two ticks, Save, Cancel, then Open folder and Done
       HIT_REP_BG = 7350, HIT_REP_TICK = 7351, HIT_REP_SAVE = 7355, HIT_REP_CANCEL = 7356, HIT_REP_OPEN = 7357, HIT_REP_DONE = 7358,
       // the bar after an unexpected close: Make a report, Not now
       HIT_CRASH_REPORT = 7360, HIT_CRASH_DISMISS = 7361,
       // Settings › Discs (4.4): per row (0 US, 1 European, 2 Japanese) Add or Replace, then Remove; the found disc's
       // Copy into OrrePort / Move / Cancel; a removal's confirm; the ROM folder
       HIT_DISC_ADD = 7370, HIT_DISC_REMOVE = 7373, HIT_DISC_COPY = 7376, HIT_DISC_MOVE = 7377, HIT_DISC_CANCEL = 7378,
       HIT_DISC_CONFIRM = 7379, HIT_DISC_SHOW = 7380,  // (7380 + row: the folder button, File Explorer at the disc)
       // updates (4.4): Home's line (Download, What's new, Restart now, Try again) and Settings › About (Check for updates,
       // the two switches, Go back with its confirm)
       // *Moved to 8300- on 2026-11-23:* they were 7390-7401, and 7396-7401 were already the global search's (HIT_SEARCH_CLEAR,
       // HIT_GSEARCH_KEY, HIT_PAL_BG, HIT_GSEARCH, HIT_SRES...: a second enum by the search's drawing), so "Open OrrePort"
       // acted as the first search result and the Check-for-updates switch as the list search's clear button (snatcherLeo: "Open
       // OrrePort button in studio doesn't work"). 8300- is free: the last range, HIT_USED, ends at 8299
       HIT_UPD_DOWNLOAD = 8300, HIT_UPD_NOTES = 8301, HIT_UPD_RESTART = 8302, HIT_UPD_RETRY = 8303, HIT_UPD_CHECK = 8304,
       HIT_UPD_SW_AUTO = 8305, HIT_UPD_SW_CHECK = 8306, HIT_UPD_GOBACK = 8307, HIT_UPD_GOBACK_YES = 8308, HIT_UPD_GOBACK_NO = 8309,
       // the other program (4.4): Home's "Open OrrePort" / "OrrePort is opened", and "Close OrrePort" to finish an update
       HIT_OPEN_ORREPORT = 8310, HIT_UPD_CLOSE_OTHER = 8311,
       // outside the "Go back" pop-up (draw_goback; takes the clicks so nothing behind it acts)
       HIT_UPD_GOBACK_BG = 8312,
       // Home's no-disc strip (2026-11-24): its Add disc… button
       HIT_HOME_ADD_DISC = 8313,
       // the removed-disc bar (2026-11-24): Discs (Settings › Discs), OK (hides it)
       HIT_DISC_GONE_OPEN = 8314, HIT_DISC_GONE_OK = 8315,
       // the Moves tab: the learnset groups, its rows, the preview's controls, and the faded later controls (hover
       // only: HIT_LATER + kLater*)
       HIT_LGROUP = 7500, HIT_PLAY = 7510, HIT_STEP_BACK = 7511, HIT_STEP_FWD = 7512, HIT_VIEW_STUDIO = 7513,
       HIT_VIEW_PLAIN = 7514, HIT_MOVE_EDITOR = 7515, HIT_TIMELINE = 7516, HIT_RATE = 7517, HIT_VIEW_ARENA = 7518, HIT_LATER = 7520,
       HIT_TURN1 = 7560, HIT_TURN2 = 7561, HIT_PARTS_MOVE = 7562, HIT_PARTS_FOLLOW = 7563, HIT_PARTS_BOTH = 7564, HIT_LROW = 7600,
       HIT_LROW_END = 7799, HIT_STYLE = 7800, HIT_STYLE_END = 7849, HIT_CHANGE_STYLE = 29,  // (styles: 2026-11-22)
       HIT_OV_STAGE = 7860,  // (the Overview card's stage menu button, 2026-11-22)
       HIT_CRY = 7861, HIT_CRY_FAINT = 7862, HIT_CRY_SOUND = 7863,  // (the cries' play buttons, the Sound tab link)
       // the Animations tab (2026-11-22): a move's "See it in Animations" link, the animation rows (+ the animation's
       // index), the Used by chips (+ the learnset row)
       HIT_SEE_ANIM = 7864, HIT_AROW = 7900, HIT_AROW_END = 7999, HIT_USED = 8000, HIT_USED_END = 8299 };
// a 3D view's stage: Studio view (Studio's own camera and backdrop), Plain stage (the game's move-viewer angle on its grey
// backdrop), Arena (that angle in Mt. Battle's valley)
constexpr int kStageStudio = 0, kStagePlain = 1, kStageArena = 2;
// (plain ints, not a second enum: adding one enum's value to another's is deprecated in C++20)
constexpr int kLaterEdit = 0, kLaterTest = 1, kLaterRate = 2, kLaterParticles = 3, kLaterSwapAnim = 4, kLaterSwapParticles = 5,
              kLaterAttach = 6, kLaterSounds = 7, kLaterReaction = 8, kLaterCount = 9;
enum { MENU_IDLE = 1, MENU_STANDING = 2 };

std::wstring number3(int n) { wchar_t b[16]; swprintf(b, 16, L"%03d", n); return b; }

std::wstring title_case(const std::wstring& s) {
    // the disc's names are upper case ("BULBASAUR"); shown in title case (likely what snatcherLeo wants; the raw name stays
    // available for Advanced later)
    std::wstring r = s;
    bool start = true;
    for (auto& c : r) {
        if (start) c = (wchar_t)towupper(c);
        else c = (wchar_t)towlower(c);
        start = c == L' ' || c == L'-' || c == L'.';
    }
    return r;
}

// ---- settings: OrrePort's shared settings.ini (theme, language) and Studio's own studio.ini (the disc) ----
std::map<std::string, std::string> read_ini(const std::wstring& path) {
    std::map<std::string, std::string> m;
    const std::string s = load_text_file(path);
    size_t a = 0;
    while (a < s.size()) {
        size_t e = s.find('\n', a);
        if (e == std::string::npos) e = s.size();
        std::string line = s.substr(a, e - a);
        a = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string v) {
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
            size_t i = v.find_first_not_of(" \t");
            return i == std::string::npos ? std::string() : v.substr(i);
        };
        m[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return m;
}

std::wstring studio_ini() { return g.settings_dir + L"\\studio.ini"; }

// The disc as studio.ini keeps it. Portable layout: relative to R\ with forward slashes ("Pokemon Colosseum/ROM/<file>")
// when the disc is inside R\, so a copied or moved folder still finds it (step 4.1's rule: no full paths in the
// settings); a disc elsewhere stays a full path. Development layout: the full path, as before.
std::wstring disc_for_ini(const std::wstring& path) {
    const std::wstring root = g.root_dir + L"\\";
    if (g.root_dir.empty() || path.size() <= root.size() || _wcsnicmp(path.c_str(), root.c_str(), root.size()) != 0) return path;
    std::wstring rel = path.substr(root.size());
    std::replace(rel.begin(), rel.end(), L'\\', L'/');
    return rel;
}

// ...and back: a relative value is joined to R\ (portable) or the exe's folder; a full one ("C:\...", "\\server\...")
// is read as it is, so an older studio.ini with a full path keeps working.
std::wstring disc_from_ini(std::wstring v) {
    if (v.empty()) return v;
    const bool full = (v.size() > 1 && v[1] == L':') || v[0] == L'\\' || v[0] == L'/';
    std::replace(v.begin(), v.end(), L'/', L'\\');
    if (full) return v;
    return (g.root_dir.empty() ? g.exe_dir : g.root_dir) + L"\\" + v;
}

void read_shared_settings() {
    // OrrePort's shared settings (theme, language): R\data\settings\settings.ini in a portable folder, else
    // <OrrePort's folder>\user\settings.ini (the older development layout: --orreport build-pc\firstlink)
    auto m = read_ini(g.orreport_root.empty() ? g.orreport_dir + L"\\user\\settings.ini" : g.orreport_root + L"\\data\\settings\\settings.ini");
    g.theme_setting = m.count("ui_appearance") ? std::atoi(m["ui_appearance"].c_str()) : 0;
    if (g.lang_code.empty()) g.lang_code = m.count("language") ? m["language"] : "auto";
    // the updater's two switches, shared with OrrePort (4.4; docs/port/updater-plan.md): checking on unless turned off
    // ("stay fully offline"), downloading by itself only when turned on (snatcherLeo: "we should always ask first")
    g.update_check = !m.count("update_check") || std::atoi(m["update_check"].c_str()) != 0;
    g.update_auto = m.count("update_auto_download") && std::atoi(m["update_auto_download"].c_str()) != 0;
}
// One line of OrrePort's shared settings.ini changed in place, the rest kept as written (Studio otherwise only reads
// that file): "key = value" replaced, or added at the end; written to a temporary file and moved over the old one, so a
// cut write never leaves half a file. Windows (MoveFileExW); Linux / macOS: rename(2) (likely). Used by Settings ›
// About's two update switches (4.4)
bool shared_setting_set(const char* key, const char* value) {
    const std::wstring path = g.orreport_root.empty() ? g.orreport_dir + L"\\user\\settings.ini" : g.orreport_root + L"\\data\\settings\\settings.ini";
    std::string text;
    if (FILE* f = _wfopen(path.c_str(), L"rb")) {
        char buf[4096];
        for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
        std::fclose(f);
    }
    const std::string line = std::string(key) + " = " + value;
    std::string out;
    bool found = false;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        std::string l = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? text.size() : end + 1;
        std::string t = l;
        t.erase(0, t.find_first_not_of(" \t"));
        const size_t eq = t.find('=');
        std::string k = eq == std::string::npos ? "" : t.substr(0, eq);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
        if (!found && k == key) { out += line + (l.size() && l.back() == '\r' ? "\r" : "") + "\n"; found = true; }
        else out += l + (end == std::string::npos ? "" : "\n");
    }
    if (!found) out += (out.empty() || out.back() == '\n' ? "" : "\n") + line + "\n";
    const std::wstring tmp = path + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok && MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

// Studio's own preferences file (snatcherLeo, 2026-11-19: "we'll have a user preferences kind of file for that kind of
// stuff"): studio.ini (R\data\settings\ in the portable layout, <exe>\user\ in development), every line rewritten together.
// "disc" is the disc picked last (relative to R\ when inside it, disc_for_ini); "theme"
// (windows / light / dark) is the choice in Settings > Appearance, and while it is absent Studio follows OrrePort's
// shared Theme setting (settings.ini "ui_appearance"), as it always did. Windows path; Linux / macOS: the same file
// in the portable folder.
// Every Studio preference as it is in effect, defaults included (4.3 part 3, the logging list's item 4, approved by
// snatcherLeo; OrrePort's step 3.7 does the same for settings.ini, port/src/pc_settings.c): one "[settings]" line at start
// with all of them, then one line per change, "<key>: <old> -> <new>", so a log says what was switched and when (each
// line already starts with the seconds since start). The disc is only its file name (logs hold no paths).
std::vector<std::pair<std::string, std::string>> prefs_values() {
    std::vector<std::pair<std::string, std::string>> v;
    const std::wstring disc = !g.disc_path.empty() ? g.disc_path : g.disc_pref;
    v.push_back({"disc", disc.empty() ? "none" : narrow(disc.substr(disc.find_last_of(L"\\/") + 1))});
    v.push_back({"theme", g.studio_theme == 1 ? "light" : g.studio_theme == 2 ? "dark" : g.studio_theme == 0 ? "windows" : "OrrePort's"});
    v.push_back({"particles", g.particles_on ? "on" : "off"});
    v.push_back({"lighting", g.game_light ? "the game's" : "studio"});
    v.push_back({"battle_camera", g.battle_cam ? "on" : "off"});
    v.push_back({"rate", g.preview_rate > 0 ? std::to_string(g.preview_rate) + " Hz" : "the game's"});
    v.push_back({"target", g.target_species >= 0 ? std::to_string(g.target_species) : "the box"});
    return v;
}
std::vector<std::pair<std::string, std::string>> g_prefs_logged;  // what the log last said (prefs_log)
void prefs_log(bool at_start) {
    auto now = prefs_values();
    if (at_start || g_prefs_logged.empty()) {
        std::string line;
        for (const auto& [k, val] : now) line += (line.empty() ? "" : ", ") + k + " " + val;
        logf(Level::Info, "settings", "studio.ini at start: %s (shared with OrrePort: language %s, theme setting %d)", line.c_str(),
             g.lang_code.c_str(), g.theme_setting);
    } else {
        for (size_t i = 0; i < now.size() && i < g_prefs_logged.size(); ++i)
            if (now[i].second != g_prefs_logged[i].second)
                logf(Level::Info, "settings", "%s: %s -> %s", now[i].first.c_str(), g_prefs_logged[i].second.c_str(), now[i].second.c_str());
    }
    g_prefs_logged = std::move(now);
}

void save_prefs() {
    prefs_log(false);  // (each change as "<key>: <old> -> <new>")
    CreateDirectoryW(g.settings_dir.c_str(), nullptr);
    if (FILE* f = _wfopen(studio_ini().c_str(), L"w")) {
        std::fprintf(f, "# OrrePort Studio's preferences (the language is shared with OrrePort, in its settings.ini)\n");
        const std::wstring disc = !g.disc_path.empty() ? disc_for_ini(g.disc_path) : g.disc_pref;
        if (!disc.empty()) std::fprintf(f, "disc = %s\n", narrow(disc).c_str());
        if (g.studio_theme >= 0) {
            std::fprintf(f, "# theme: windows (follow Windows), light or dark; without this line, OrrePort's Theme setting\n");
            std::fprintf(f, "theme = %s\n", g.studio_theme == 1 ? "light" : g.studio_theme == 2 ? "dark" : "windows");
        }
        if (!g.particles_on) {
            std::fprintf(f, "# the Pokémon's own particle effects in the 3D view (on unless this says off)\n");
            std::fprintf(f, "particles = off\n");
        }
        if (!g.game_light) {
            std::fprintf(f, "# the 3D view's lighting: the game's arena lights (unless this says studio: Studio's even light)\n");
            std::fprintf(f, "lighting = studio\n");
        }
        if (!g.battle_cam) {
            std::fprintf(f, "# moves filmed by the battle's camera in the Moves preview (on unless this says off)\n");
            std::fprintf(f, "battle_camera = off\n");
        }
        if (g.preview_rate > 0) {
            std::fprintf(f, "# the 3D views' rate in Hz, smoothed between the game's frames (without this line: the game's own rate)\n");
            std::fprintf(f, "rate = %d\n", g.preview_rate);
        }
        if (g.target_species >= 0 || !g.target_recent.empty()) {
            std::fprintf(f, "# the Moves preview's target: a species number (without this line: the box), and the last few chosen\n");
            if (g.target_species >= 0) std::fprintf(f, "target = %d\n", g.target_species);
            std::string list;
            for (int s : g.target_recent) list += (list.empty() ? "" : ",") + std::to_string(s);
            if (!list.empty()) std::fprintf(f, "target_recent = %s\n", list.c_str());
        }
        std::fclose(f);
    }
}
void save_disc_path() { save_prefs(); }

// ---- fonts: OrrePort's shipped set, loaded privately ----
void load_fonts() {
    const auto t0 = std::chrono::steady_clock::now();
    IDWriteFontSetBuilder1* b = nullptr;
    if (FAILED(g.dw->CreateFontSetBuilder(&b)) || !b) return;
    std::vector<std::wstring> families;
    int files = 0;
    WIN32_FIND_DATAW fd;
    for (const wchar_t* pat : {L"\\*.ttf", L"\\*.otf"}) {
        HANDLE h = FindFirstFileW((g.fonts_dir + pat).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            const std::wstring path = g.fonts_dir + L"\\" + fd.cFileName;
            IDWriteFontFile* file = nullptr;
            if (SUCCEEDED(g.dw->CreateFontFileReference(path.c_str(), nullptr, &file))) {
                if (SUCCEEDED(b->AddFontFile(file))) ++files;
                file->Release();
            }
            // the search box is a standard (GDI) control: the same files, private to Studio
            if (std::wstring(fd.cFileName).rfind(L"Inter", 0) == 0) AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    IDWriteFontSet* set = nullptr;
    if (SUCCEEDED(b->CreateFontSet(&set))) g.dw->CreateFontCollectionFromFontSet(set, &g.fonts);
    if (set) set->Release();
    b->Release();
    if (!g.fonts) {
        logf(Level::Warn, "fonts", "no shipped fonts found in Fonts\\ (%d files); using Windows' default font", files);
        g.ui_family = L"Segoe UI";
        return;
    }
    // fallback: every family of the shipped set, Inter first, so any script finds a shipped face
    for (UINT32 i = 0; i < g.fonts->GetFontFamilyCount(); ++i) {
        IDWriteFontFamily1* fam = nullptr;
        IDWriteLocalizedStrings* names = nullptr;
        if (SUCCEEDED(g.fonts->GetFontFamily(i, &fam)) && SUCCEEDED(fam->GetFamilyNames(&names))) {
            UINT32 idx = 0;
            BOOL found = FALSE;
            names->FindLocaleName(L"en-us", &idx, &found);
            UINT32 len = 0;
            names->GetStringLength(found ? idx : 0, &len);
            std::wstring n(len + 1, 0);
            names->GetString(found ? idx : 0, n.data(), len + 1);
            n.resize(len);
            families.push_back(n);
        }
        if (names) names->Release();
        if (fam) fam->Release();
    }
    std::stable_sort(families.begin(), families.end(), [](const std::wstring& a, const std::wstring& b2) {
        auto rank = [](const std::wstring& f) { return f == L"Inter" ? 0 : f.rfind(L"Noto Sans", 0) == 0 && f.size() == 9 ? 1 : 2; };
        return rank(a) < rank(b2);
    });
    IDWriteFontFallbackBuilder* fb = nullptr;
    if (SUCCEEDED(g.dw->CreateFontFallbackBuilder(&fb))) {
        std::vector<const wchar_t*> ptrs;
        for (auto& f : families) ptrs.push_back(f.c_str());
        DWRITE_UNICODE_RANGE all{0, 0x10FFFF};
        fb->AddMapping(&all, 1, ptrs.data(), (UINT32)ptrs.size(), g.fonts, nullptr, nullptr, 1.0f);
        fb->CreateFontFallback(&g.fallback);
        fb->Release();
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    logf(Level::Info, "fonts", "%d font files, %zu families from the shipped set in %.0f ms", files, families.size(), ms);
}

// the shared search-box fonts, applied to this window's three edit boxes (a later window only applies them: the other
// windows' boxes use the same fonts, so they are made once, at the first window's DPI; a window on a monitor with
// another DPI keeps them as they are until its own DPI change remakes them for all)
void apply_search_fonts() {
    for (HWND h : {g.search, g.psearch})
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)g.search_font, TRUE);
    if (g.gsearch) SendMessageW(g.gsearch, WM_SETFONT, (WPARAM)g.gsearch_font, TRUE);
}
void make_search_font() {
    if (g.search_font) DeleteObject(g.search_font);
    g.search_font = CreateFontW(-(int)px(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                g.ui_family.c_str());
    for (HWND h : {g.search, g.psearch})
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)g.search_font, TRUE);
    // the top bar's field is smaller (12.5 px), so "Search everything" fits beside its "Ctrl K" (snatcherLeo, 2026-11-18:
    // "have search everything be smaller so it fits next to "Ctrl K"")
    if (g.gsearch_font) DeleteObject(g.gsearch_font);
    g.gsearch_font = CreateFontW(-(int)px(12.5f), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                 g.ui_family.c_str());
    if (g.gsearch) SendMessageW(g.gsearch, WM_SETFONT, (WPARAM)g.gsearch_font, TRUE);
}

// ---- theme ----
void apply_theme() {
    const int t = g.theme_forced >= 0 ? g.theme_forced : g.studio_theme >= 0 ? g.studio_theme : g.theme_setting;
    const bool was_dark = g.pal.dark;
    g.pal = make_palette(t);
    apply_title_bar(g.hwnd, g.pal.dark);
    apply_window_icons(g.hwnd, g.ui_dir + L"\\icons", g.pal.dark);
    for (HWND h : {g.search, g.gsearch, g.psearch})
        if (h) SetWindowTheme(h, g.pal.dark ? L"DarkMode_CFD" : nullptr, nullptr);
    for (HBRUSH* b : {&g.search_brush, &g.win_brush, &g.card_brush})
        if (*b) DeleteObject(*b);
    auto cr = [](const D2D1_COLOR_F& c) { return RGB((int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255)); };
    g.search_brush = CreateSolidBrush(cr(g.pal.bg));
    g.win_brush = CreateSolidBrush(cr(g.pal.card));   // the top field sits on its own card-coloured frame
    g.card_brush = CreateSolidBrush(cr(g.pal.card));  // the palette's field
    g.viewer.set_colours(g.pal.stage_hi, g.pal.stage_lo);
    for (auto& [k, d] : g.svgs) if (d) d->Release();
    g.svgs.clear();
    if (was_dark != g.pal.dark) logf(Level::Info, "theme", "%s", g.pal.dark ? "dark" : "light");
    InvalidateRect(g.hwnd, nullptr, FALSE);
    for (HWND h : {g.search, g.gsearch, g.psearch})
        if (h) InvalidateRect(h, nullptr, TRUE);
}
// a theme picked in Settings reaches every window at once: each has its own title bar, search boxes and brushes
void apply_theme_all() {
    const HWND back = g.hwnd;
    for (size_t k = 0; k < g_windows.size(); ++k) {
        activate(g_windows[k].hwnd);
        apply_theme();
    }
    activate(back);
}

// ---- data ----
void filter() {
    wchar_t buf[128] = L"";
    if (g.search) GetWindowTextW(g.search, buf, 128);
    std::wstring q = buf;
    for (auto& c : q) c = (wchar_t)towlower(c);
    while (!q.empty() && q.back() == L' ') q.pop_back();
    g.shown.clear();
    for (int i = 0; i < (int)g.all.size(); ++i) {
        const Entry& e = g.all[i];
        if (!q.empty()) {
            std::wstring n = e.name;
            for (auto& c : n) c = (wchar_t)towlower(c);
            const bool digits = std::all_of(q.begin(), q.end(), iswdigit);
            if (n.find(q) == std::wstring::npos && !(digits && std::to_wstring(e.s.national_dex).rfind(std::to_wstring(std::wcstol(q.c_str(), nullptr, 10)), 0) == 0))
                continue;
        }
        g.shown.push_back(i);
    }
    g.scroll = 0;
}

// ---- the global search: one forgiving search for the top field and the Ctrl+K palette ----
// snatcherLeo, 2026-11-18: "fuzzy searching, just like Notion, so you can find approximately what you are looking for.
// Forgiving searching"; then the design he sent (from another app of his: "Some of this would apply, some of it
// won't"). What Studio takes from it:
// - normalisation: lower case, accents and full-width letters folded, the same for the text and the query ("Café",
//   "cafe" and "ＣＡＦＥ" match; "Mr. Mime" = "mr mime");
// - words in any order, each matched on its own, inside words too ("chu" finds Pikachu);
// - typos corrected from Studio's own vocabulary (every word of every name, type, ability and category), not a
//   dictionary: a word found nowhere becomes the nearest real word, one letter off for words up to 5 letters, two
//   for longer ones (insert, delete, change, or swap neighbours); preference to words that appear with the other
//   search words, then longer ones, then common ones; up to three candidates searched together; a word nothing is
//   close to is dropped and the rest searched; and a line above the results says so;
// - while typing, one or two letters on their own match the start of a word; after other words a short fragment
//   waits for a third letter;
// - ranking in tiers: every word in the name, the words together as typed, every word anywhere, some of the words;
//   within a tier a name match counts ten times a match elsewhere; ties keep National Dex order.
// What it leaves out, and why: SQLite's full-text search and the trigram / word indexes (a few hundred entries are
// searched in memory in well under a millisecond, and no new library is needed; substring matching already finds
// text inside words and in Japanese). *For later (snatcherLeo, same day: "the SQlite thing could be a thing when we add more
// and more stuff to our app (especially with world editor)"):* when Studio holds far more (maps, scripts, every text
// line), SQLite's FTS5 with those indexes is the planned engine behind the same interface.
// Plain C++ (towlower, small tables): the same on every platform.
wchar_t fold_char(wchar_t c) {
    static const std::pair<const wchar_t*, wchar_t> kAccents[] = {
        {L"àáâãäåā", L'a'}, {L"çć", L'c'}, {L"èéêëē", L'e'}, {L"ìíîïī", L'i'}, {L"ñ", L'n'}, {L"òóôõöøō", L'o'},
        {L"ùúûüū", L'u'}, {L"ýÿ", L'y'}, {L"ß", L's'}};
    if (c >= 0xFF01 && c <= 0xFF5E) c = (wchar_t)(c - 0xFEE0);  // full-width ASCII ("ＣＡＦＥ") to plain
    c = (wchar_t)towlower(c);
    for (const auto& [set, base] : kAccents)
        if (std::wcschr(set, c)) return base;
    return c;
}
bool word_char(wchar_t c) { return iswalnum(c) || c > 0x2E7F; }  // letters, digits, and CJK / kana as they are
// the folded text without spaces or punctuation ("Mr. Mime" -> "mrmime"); map, if given, gets each kept character's
// index in s (for colouring the matched letters)
std::wstring fold(const std::wstring& s, std::vector<size_t>* map = nullptr) {
    std::wstring out;
    for (size_t i = 0; i < s.size(); ++i) {
        const wchar_t c = fold_char(s[i]);
        if (word_char(c)) { out += c; if (map) map->push_back(i); }
    }
    return out;
}
// the folded words ("Seed Pokémon" -> seed, pokemon)
std::vector<std::wstring> fold_words(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c0 : s) {
        const wchar_t c = fold_char(c0);
        if (word_char(c)) cur += c;
        else if (c != L'\'' && c != L'.' && !cur.empty()) { out.push_back(cur); cur.clear(); }  // Farfetch'd, Mr. stay one word
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
// Damerau-Levenshtein (optimal string alignment) between a and b, stopping early past `limit`
int edit_distance(const std::wstring& a, const std::wstring& b, int limit) {
    const size_t n = a.size(), m = b.size();
    if ((int)(n > m ? n - m : m - n) > limit) return limit + 1;
    std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1));
    for (size_t i = 0; i <= n; ++i) d[i][0] = (int)i;
    for (size_t j = 0; j <= m; ++j) d[0][j] = (int)j;
    for (size_t i = 1; i <= n; ++i)
        for (size_t j = 1; j <= m; ++j) {
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
        }
    return d[n][m];
}
// What can be found: every Pokémon (its name; then its types, abilities and category) and every editor (its name),
// built once per disc and language (the names are the shown, translated ones)
std::wstring type_name(int t);
std::wstring ability_name(int a);
struct SField { std::vector<std::wstring> words; std::wstring joined, show; };
struct SDoc { int kind, idx; std::wstring title_joined; std::vector<std::wstring> title_words; std::vector<SField> body; };
std::vector<SDoc> g_sdocs;
std::map<std::wstring, int> g_vocab;                // folded word -> how many entries use it
std::map<std::wstring, std::wstring> g_vocab_show;  // folded word -> one way it is written (for the "searched as" line)
std::wstring g_sdocs_key;
void build_search_docs() {
    const std::wstring key = std::to_wstring(g.all.size()) + L"|" + tx("studio.editor.pokemon") + L"|" + tx("studio.search.field");
    if (key == g_sdocs_key) return;
    g_sdocs_key = key;
    g_sdocs.clear();
    g_vocab.clear();
    g_vocab_show.clear();
    auto add_words = [](const std::wstring& shown) {
        // the shown words, split the same way fold_words does, so each folded word keeps how it is written
        std::wstring cur, raw;
        auto flush = [&] {
            if (!cur.empty()) { ++g_vocab[cur]; g_vocab_show.emplace(cur, raw); }
            cur.clear();
            raw.clear();
        };
        for (wchar_t c0 : shown) {
            const wchar_t c = fold_char(c0);
            if (word_char(c)) { cur += c; raw += c0; }
            else if (c != L'\'' && c != L'.') flush();
            else raw += c0;
        }
        flush();
    };
    auto field = [&](const std::wstring& shown) {
        add_words(shown);
        return SField{fold_words(shown), fold(shown), shown};
    };
    for (int i = 0; i < (int)g.all.size(); ++i) {
        const Entry& e = g.all[(size_t)i];
        SDoc d{0, i, fold(e.name), fold_words(e.name), {}};
        add_words(e.name);
        d.body.push_back(field(tx("studio.search.via_type", {{"type", type_name(e.s.types[0])}})));
        if (e.s.types[1] != e.s.types[0]) d.body.push_back(field(tx("studio.search.via_type", {{"type", type_name(e.s.types[1])}})));
        for (int a : e.s.abilities)
            if (a > 0) d.body.push_back(field(tx("studio.search.via_ability", {{"ability", ability_name(a)}})));
        if (!e.category.empty()) d.body.push_back(field(tx("studio.page.category", {{"category", e.category}})));
        g_sdocs.push_back(std::move(d));
    }
    for (int ed = 0; ed < kEditorCount; ++ed) {
        char k[48];
        std::snprintf(k, sizeof k, "studio.editor.%s", kEditors[ed]);
        g_sdocs.push_back({1, ed, fold(tx(k)), fold_words(tx(k)), {}});
        add_words(tx(k));
    }
}
// the nearest real words to a word found nowhere (at most 3): one letter off for up to 5 letters, two above, against
// whole words and, while typing, against a word's start of the same length; preferring words that appear with the
// other search words, then longer, then more common
std::vector<std::wstring> correct_word(const std::wstring& w, const std::vector<std::wstring>& others) {
    const int allowed = w.size() <= 5 ? 1 : 2;
    struct Cand { std::wstring v; int dist, together, len, count; };
    std::vector<Cand> c;
    for (const auto& [v, count] : g_vocab) {
        int d = edit_distance(w, v, allowed);
        if (v.size() > w.size()) d = std::min(d, edit_distance(w, v.substr(0, w.size()), allowed));
        if (d > allowed) continue;
        int together = 0;
        if (!others.empty())
            for (const SDoc& doc : g_sdocs) {
                std::wstring all = doc.title_joined;
                for (const SField& f : doc.body) all += L" " + f.joined;
                if (all.find(v) == std::wstring::npos) continue;
                bool every = true;
                for (const auto& o : others) every = every && all.find(o) != std::wstring::npos;
                if (every) { together = 1; break; }
            }
        c.push_back({v, d, together, (int)v.size(), count});
    }
    std::sort(c.begin(), c.end(), [](const Cand& a, const Cand& b) {
        if (a.dist != b.dist) return a.dist < b.dist;
        if (a.together != b.together) return a.together > b.together;
        if (a.len != b.len) return a.len > b.len;
        return a.count > b.count;
    });
    std::vector<std::wstring> out;
    for (size_t i = 0; i < c.size() && i < 3; ++i) out.push_back(c[i].v);
    return out;
}
// fills g.sres (best first) and g.snote for a query
void run_search(const std::wstring& raw) {
    g.sres.clear();
    g.ssel = 0;
    g.snote.clear();
    build_search_docs();
    std::vector<std::wstring> words = fold_words(raw);
    if (words.empty()) return;
    // digits: National Dex numbers ("25", "025")
    if (words.size() == 1 && std::all_of(words[0].begin(), words[0].end(), iswdigit)) {
        const std::wstring q2 = std::to_wstring(std::wcstol(words[0].c_str(), nullptr, 10));
        for (int i = 0; i < (int)g.all.size(); ++i) {
            const std::wstring num = std::to_wstring(g.all[(size_t)i].s.national_dex);
            if (num == q2) g.sres.push_back({0, i, 1, 1000, L"", {}});
            else if (num.rfind(q2, 0) == 0) g.sres.push_back({0, i, 3, 500 - (int)num.size(), L"", {}});
        }
        std::stable_sort(g.sres.begin(), g.sres.end(), [](const App::SRes& a, const App::SRes& b) { return a.tier != b.tier ? a.tier < b.tier : a.score > b.score; });
        return;
    }
    // still typing: after other words, a last fragment of one or two letters waits for a third
    const bool typing_last = !raw.empty() && word_char(fold_char(raw.back()));
    if (words.size() > 1 && typing_last && words.back().size() < 3) words.pop_back();
    const bool short_alone = words.size() == 1 && words[0].size() < 3;  // one or two letters: the start of a word only
    // each word: itself if Studio's text has it anywhere, else its nearest real words, else dropped
    struct QWord { std::wstring typed; std::vector<std::wstring> alts; };
    std::vector<QWord> q;
    std::vector<std::wstring> corrected, dropped, known;
    for (const auto& w : words) {
        bool found = false;
        for (const auto& [v, n] : g_vocab)
            if (short_alone ? v.rfind(w, 0) == 0 : v.find(w) != std::wstring::npos) { found = true; break; }
        if (found || short_alone) { q.push_back({w, {w}}); known.push_back(w); }
    }
    for (const auto& w : words) {
        if (std::find(known.begin(), known.end(), w) != known.end()) continue;
        if (short_alone) continue;
        std::vector<std::wstring> alts = correct_word(w, known);
        if (alts.empty()) { dropped.push_back(w); continue; }
        q.push_back({w, alts});
        corrected.push_back(tx("studio.search.corrected", {{"typed", w}, {"word", g_vocab_show.count(alts[0]) ? g_vocab_show[alts[0]] : alts[0]}}));
    }
    // what the search did, in one line above the results
    for (const auto& c : corrected) g.snote += (g.snote.empty() ? L"" : L" · ") + c;
    for (const auto& d : dropped) g.snote += (g.snote.empty() ? L"" : L" · ") + tx("studio.search.dropped", {{"typed", d}});
    if (q.empty()) return;
    // the words together as typed (for tier 2)
    std::wstring phrase;
    for (const auto& w : q) phrase += w.alts[0];
    const int n = (int)q.size();
    for (const SDoc& d : g_sdocs) {
        int in_title = 0, anywhere = 0, prefix = 0, score = 0;
        std::wstring via;
        std::vector<std::wstring> marks;
        for (const QWord& w : q) {
            bool t = false, b = false;
            for (const auto& a : w.alts) {
                bool tw = false;
                for (const auto& tword : d.title_words)
                    if (short_alone ? tword.rfind(a, 0) == 0 : tword.find(a) != std::wstring::npos) {
                        tw = true;
                        if (tword.rfind(a, 0) == 0) ++prefix;
                        break;
                    }
                if (!tw && !short_alone && d.title_joined.find(a) != std::wstring::npos) tw = true;  // across words ("mrmime")
                if (tw) { t = true; marks.push_back(a); break; }
            }
            if (!t && !short_alone)
                for (const auto& a : w.alts) {
                    for (const SField& f : d.body)
                        for (const auto& fw : f.words)
                            if (fw.find(a) != std::wstring::npos) { b = true; if (via.empty()) via = f.show; break; }
                    if (b) break;
                }
            in_title += t ? 1 : 0;
            anywhere += t || b ? 1 : 0;
        }
        if (!anywhere) continue;
        int tier = 4;
        if (in_title == n) tier = 1;
        else if (n > 1 && std::any_of(d.body.begin(), d.body.end(), [&](const SField& f) { return f.joined.find(phrase) != std::wstring::npos; })) tier = 2;
        else if (anywhere == n) tier = 3;
        // a name match counts ten times a match elsewhere; starts of words and short names first; an editor a step
        // below a Pokémon of the same standing
        // (the short-name preference only where the name matched: a match by type or category keeps dex order)
        score = in_title * 10 + (anywhere - in_title) + prefix * 3 - (in_title ? (int)d.title_joined.size() / 4 : 0) - (d.kind == 1 ? 1 : 0);
        if (d.title_joined.rfind(q[0].alts[0], 0) == 0) score += 5;
        g.sres.push_back({d.kind, d.idx, tier, score, in_title == n ? L"" : via, marks});
    }
    std::stable_sort(g.sres.begin(), g.sres.end(), [](const App::SRes& a, const App::SRes& b) { return a.tier != b.tier ? a.tier < b.tier : a.score > b.score; });
    // (choosing the Moves preview's target: Pokémon only)
    if (g.palette_target) g.sres.erase(std::remove_if(g.sres.begin(), g.sres.end(), [](const App::SRes& s) { return s.kind != 0; }), g.sres.end());
    if (g.sres.size() > 60) g.sres.resize(60);
}

void release_pics() {
    for (auto& p : g.mugs) if (p.bmp) p.bmp->Release();
    for (auto& p : g.pcs) if (p.bmp) p.bmp->Release();
    g.mugs.clear();
    g.pcs.clear();
}

void place_children();

void release_forms() {
    for (auto& f : g.forms) if (f.bmp) f.bmp->Release();
    g.forms.clear();
}

// The selected species' forms with their thumbnails and labels. Unown: the letter from its model archive's suffix
// (pkx_unknown_a .. _z, _ex = !, _qu = ?); Castform: Normal and its three weather looks (_f / _w / _i); Deoxys: the
// Normal Forme plus the three Formes XD added, greyed (snatcherLeo, 2026-11-18: "we can still add that there is form
// information"). Everything else has one form and no row.
constexpr int kDeoxys = 410;
void load_forms(int sp) {
    release_forms();
    const int n = orre_species_form_count(g.disc, sp);
    auto thumb = [&](int form) -> ID2D1Bitmap* {
        OrreImage im{};
        if (!orre_species_form_mug_shot(g.disc, sp, form, 0, 0, &im)) return nullptr;
        ID2D1Bitmap* b = make_bitmap(im.rgba, im.width, im.height);
        orre_image_free(&im);
        return b;
    };
    if (sp == kDeoxys) {
        static const char* const kKeys[4] = {"studio.form.deoxys.normal", "studio.form.deoxys.attack", "studio.form.deoxys.defense",
                                             "studio.form.deoxys.speed"};
        for (int i = 0; i < 4; ++i) g.forms.push_back({thumb(0), tx(kKeys[i]), i == 0});
        return;
    }
    if (n <= 1) return;
    for (int f = 0; f < n; ++f) {
        char arc[64] = "";
        orre_species_form_archive(g.disc, sp, f, arc, sizeof arc);
        const std::string a = arc;
        const std::string suffix = a.rfind('_') != std::string::npos && a.rfind('_') + 1 < a.size() ? a.substr(a.rfind('_') + 1) : "";
        std::wstring label;
        if (sp == 385) {
            label = tx(suffix == "f" ? "studio.form.castform.sunny" : suffix == "w" ? "studio.form.castform.rainy"
                       : suffix == "i" ? "studio.form.castform.snowy" : "studio.form.normal");
        } else {
            label = suffix == "ex" ? L"!" : suffix == "qu" ? L"?" : widen(suffix);
            for (auto& c : label) c = (wchar_t)towupper(c);
        }
        g.forms.push_back({thumb(f), label, true});
    }
}

// the pictures and the model of the selected species' current form
// ---- styles ----
std::wstring find_rom_disc_id(const std::wstring& rom, const char* want);
// The Japanese disc, for the Japanese release's style: --region-disc, else the first GC6J01 in ROM Other Regions\
// (R\Pokemon Colosseum\ portable, <OrrePort>\Colosseum\ in development: where OrrePort's hub puts language discs).
// Opened once, on first need. Windows paths; Linux / macOS: the same folders under the portable root
OrreDisc* japanese_disc() {
    if (g.jp_tried) return g.jp_disc;
    g.jp_tried = true;
    std::wstring path = g.region_disc_arg;
    if (path.empty()) {
        const std::wstring dir = g.orreport_root.empty() ? g.orreport_dir + L"\\Colosseum\\ROM Other Regions"
                                                         : g.orreport_root + L"\\Pokemon Colosseum\\ROM Other Regions";
        path = find_rom_disc_id(dir, "GC6J01");
    }
    if (path.empty()) { logf(Level::Info, "styles", "no Japanese disc linked: the Japanese release's styles aren't offered"); return nullptr; }
    char err[512] = "";
    g.jp_path = path;  // (Settings › Discs' watch compares it with the folder's: a new or removed disc is picked up)
    g.jp_disc = orre_disc_open(narrow(path).c_str(), err, sizeof err);
    if (!g.jp_disc) logf(Level::Warn, "styles", "the Japanese disc couldn't be read: %s", err);
    else {
        logf(Level::Info, "styles", "Japanese disc linked (%s): its styles are offered", narrow(path.substr(path.find_last_of(L"\\/") + 1)).c_str());
        const std::string id = "Japanese disc: " + disc_identity(g.jp_disc, path, L"");
        logf(Level::Info, "disc", "%s", id.c_str());
        g.disc_lines.push_back(id);
    }
    return g.jp_disc;
}
// Where the Japanese release draws a Pokémon differently (docs/engine/regions.md, "Smaller differences on the Japanese
// disc"; verified: only Jynx's battle model, pkx_rougela, the row jp.rougela of OrrePort's ui/regions/jp_look.txt)
constexpr int kStyleJynx = 124;
int style_count(int sp) { return sp == kStyleJynx && japanese_disc() ? 2 : 1; }
int style_now(int sp) {
    const auto it = g.style_of.find(sp);
    const int s = it == g.style_of.end() ? 0 : it->second;
    return s < style_count(sp) ? s : 0;
}
// the style the game shows: OrrePort's Japanese release look swaps Jynx's model when its language is Japanese and the
// Japanese disc is linked (pc_region_look, the row's condition "lang=ja disc=GC6J01"); otherwise the game's own
int style_in_game(int sp) { return style_count(sp) > 1 && g.lang_code == "ja" ? 1 : 0; }

// The Pokémon shown, by name for the log (4.3 part 3, the logging list's item 15, snatcherLeo: plain names, no internal codes):
// "Taillow", or "Deoxys (Attack Forme)" where the species has forms; "no Pokémon" on other pages
std::string mon_name() {
    if (g.selected < 0 || g.selected >= (int)g.all.size()) return "no Pokémon";
    std::string s = narrow(g.all[g.selected].name);
    if (g.forms.size() > 1 && g.form >= 0 && g.form < (int)g.forms.size() && !g.forms[(size_t)g.form].label.empty())
        s += " (" + narrow(g.forms[(size_t)g.form].label) + ")";
    return s;
}

void show_form() {
    const auto t0 = std::chrono::steady_clock::now();
    release_pics();
    const int sp = g.all[g.selected].index;
    const int form = g.form;
    // mug shots: normal and shiny as the menus show them, then the same faces turned the other way where the disc has
    // them drawn (31 species; the others are not mirrored here: only what is on the disc is shown)
    for (int cell = 0; cell < 2; ++cell)
        for (int shiny = 0; shiny < 2; ++shiny) {
            OrreImage im{};
            if (!orre_species_form_mug_shot(g.disc, sp, form, shiny, cell, &im)) continue;
            const char* key = shiny ? (cell ? "studio.pic.shiny_other_way" : "studio.pic.shiny")
                                    : (cell ? "studio.pic.normal_other_way" : "studio.pic.normal");
            g.mugs.push_back({make_bitmap(im.rgba, im.width, im.height), im.width, im.height, tx(key)});
            orre_image_free(&im);
        }
    for (int shiny = 0; shiny < 2; ++shiny) {
        OrreImage im{};
        if (!orre_species_form_pc_picture(g.disc, sp, form, shiny, &im)) continue;
        g.pcs.push_back({make_bitmap(im.rgba, im.width, im.height), im.width, im.height, tx(shiny ? "studio.pic.shiny" : "studio.pic.normal")});
        orre_image_free(&im);
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    logf(Level::Info, "select", "%s: %zu mug shots, %zu PC pictures in %.1f ms", mon_name().c_str(), g.mugs.size(), g.pcs.size(), ms);
    // the model, on a worker thread (the newest request wins)
    const int gen = ++g.model_generation;
    g.model_loading = true;
    g.model_info = tx("studio.model.loading");
    g.viewer.set_model(nullptr);
    OrreDisc* disc = g.disc;
    HWND hwnd = g.hwnd;
    // (Shiny: the separate shiny model where the game has one, 35 species; the others are recoloured by the game at
    // draw time from the PKX's colour block, not applied yet, so they show their usual colours)
    const int shiny = g.shiny ? 1 : 0;
    // the style viewed: the Japanese release's model read from that disc (found through this disc's tables)
    OrreDisc* files = style_now(sp) == 1 ? japanese_disc() : disc;
    if (!files) files = disc;
    std::thread([disc, files, sp, form, shiny, gen, hwnd] {
        auto* r = new ModelResult();
        r->generation = gen;
        const auto t1 = std::chrono::steady_clock::now();
        r->ok = orre_species_form_model_from(disc, files, sp, form, shiny, &r->model) != 0;
        r->ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
        if (!PostMessageW(hwnd, WM_MODEL_LOADED, 0, (LPARAM)r)) { orre_model_free(&r->model); delete r; }
    }).detach();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// The Forms card's geometry (snatcherLeo, 2026-11-18: below Evolution on the Overview, "Just makes it nicer to click through,
// UI-wise"; it was a row under the era bar): thumbnails of kFormTile DIPs with their names under them, wrapping onto
// more lines when they don't fit the card (Unown's 28 take two lines). Height 0 when the species has a single look.
constexpr float kFormTile = 48, kFormGap = 8, kFormLine = 74, kFormCardPad = 16;
float page_width();
int forms_per_line() {
    const float w = page_width() - 2 * 28.0f - 2 * kFormCardPad;
    return std::max(1, (int)((w + kFormGap) / (kFormTile + kFormGap)));
}
float form_card_h() {
    if (g.forms.size() <= 1) return 0.0f;
    const int lines = ((int)g.forms.size() + forms_per_line() - 1) / forms_per_line();
    return 40 + lines * kFormLine + 6;
}

void ensure_visible() {
    auto it = std::find(g.shown.begin(), g.shown.end(), g.selected);
    if (it == g.shown.end()) return;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    const float view = rc.bottom / g.scale - kListTop;
    const float y = (float)(it - g.shown.begin()) * kRow;
    if (y < g.scroll) g.scroll = y;
    if (y + kRow > g.scroll + view) g.scroll = y + kRow - view;
}

// Home is a page a tab can show, like a browser's new-tab page (snatcherLeo, 2026-11-19: "The tab would change too, to just
// showing our app logo and saying "Home""): in page_tabs and the tabs' histories it is kHome instead of a Pokémon's
// index. The O on the rail turns the shown tab into Home (Back returns to the Pokémon); "+" opens a new Home tab.
// Settings is a page too (kSettings; snatcherLeo, 2026-11-19: "a "Gear icon" and this is the Settings for the overall app ...
// it'll be multi-pane"): reached from the gear on Home and Ctrl+comma, shown in a tab like a browser's settings page.
constexpr int kHome = -1, kSettings = -2;
// The other editors are pages too (snatcherLeo, 2026-11-22: "if I hold the "CTRL" button and click on one of the left-hand
// icons, it should open it in a new tab"): editor ed (1 Move .. 7 Battle; kEditors) is page -10 - ed, so a tab can hold
// it, Back / Forward step through it, and the session keeps it. The Pokémon editor's pages are the Pokémon themselves.
int editor_page(int ed) { return -10 - ed; }
// the editor a page shows (1..7), or 0 for a Pokémon, Home or Settings
int page_editor(int page) { return page <= -11 && page > -10 - kEditorCount ? -10 - page : 0; }
// the page the shown tab holds: a Pokémon's index, kHome or kSettings (kHome when no tab is open)
int cur_page() {
    return g.cur_tab < 0 || g.cur_tab >= (int)g.page_tabs.size() ? kHome : g.page_tabs[(size_t)g.cur_tab];
}
bool on_home() { return g.editor == 0 && cur_page() == kHome; }
bool on_settings() { return g.editor == 0 && cur_page() == kSettings; }
// Studio's own pages (Home, Settings) fill the window beside the rail: no Pokémon list, no 3D view
bool on_app_page() { return g.editor == 0 && cur_page() < 0; }
// a tab's name: the Pokémon's, "Home", "Settings" or the editor's
std::wstring tab_label(int idx) {
    if (const int ed = page_editor(idx)) return tx(("studio.editor." + std::string(kEditors[ed])).c_str());
    return idx == kHome ? tx("studio.home.tab") : idx == kSettings ? tx("studio.settings.title") : g.all[(size_t)idx].name;
}

// Opens a page (a Pokémon, kHome or kSettings) in the current tab, or in a new one (Ctrl+click, "+", or when no tab is
// open)
void open_page(int idx, bool new_tab = false) {
    if (idx >= (int)g.all.size() || (idx < 0 && idx != kHome && idx != kSettings && !page_editor(idx))) return;
    if (new_tab || g.cur_tab < 0 || g.page_tabs.empty()) {
        g.page_tabs.push_back(idx);
        g.tab_hist.push_back({idx});
        g.tab_pos.push_back(0);
        g.tab_sel.assign(g.page_tabs.size(), 0);  // a new tab clears a multi-selection, as in Chrome
        g.cur_tab = (int)g.page_tabs.size() - 1;
        // it grows in (Ctrl+click, Ctrl+T, "+", Duplicate all come here), only in a window already on screen: tabs
        // restored at start or carried into a new window just appear
        if (g.hwnd && IsWindowVisible(g.hwnd)) {
            g.tab_born = g.cur_tab;
            g.tab_born_at = GetTickCount64();
            SetTimer(g.hwnd, TIMER_TABGROW, 15, nullptr);
        }
    } else {
        const size_t t = (size_t)g.cur_tab;
        if (!g.navigating && g.page_tabs[t] != idx) {
            // a new visit: whatever was ahead of it is dropped, as a browser does
            auto& h = g.tab_hist[t];
            h.resize((size_t)g.tab_pos[t] + 1);
            h.push_back(idx);
            g.tab_pos[t] = (int)h.size() - 1;
        }
        g.page_tabs[t] = idx;
    }
    if (idx < 0) {
        // Home, Settings and the other editors show no Pokémon: the list and the 3D view hide (place_children), and the
        // next Pokémon loads afresh
        g.selected = -1;
        g.editor = page_editor(idx);
        g.pscroll = 0;  // (each page starts at its top: an editor's last place keeps its own scroll, WinState::places)
        place_children();
        InvalidateRect(g.hwnd, nullptr, FALSE);
        return;
    }
    g.editor = 0;  // (a Pokémon: the Pokémon editor)
    g.last_mon = idx;
    if (idx == g.selected) { place_children(); InvalidateRect(g.hwnd, nullptr, FALSE); return; }
    g.selected = idx;
    g.pscroll = 0;
    g.sel_kind = SEL_NONE;  // the inspector starts on the Pokémon itself
    g.rscroll = 0;
    ensure_visible();
    g.form = 0;
    load_forms(g.all[idx].index);
    show_form();
    place_children();  // the form row moves the page down or back up
}
// Opens a Pokémon (callers pass -1 for "none", which does nothing)
void select(int idx, bool new_tab = false) {
    if (idx < 0) return;
    open_page(idx, new_tab);
}

void select_shown(int pos) {
    if (g.shown.empty()) return;
    select(g.shown[std::clamp(pos, 0, (int)g.shown.size() - 1)]);
}

int shown_pos() {
    auto it = std::find(g.shown.begin(), g.shown.end(), g.selected);
    return it == g.shown.end() ? -1 : (int)(it - g.shown.begin());
}

void start_loading() {
    if (g.disc_path.empty()) return;
    g.loading = true;
    g.load_error.clear();
    const std::wstring path = g.disc_path;
    HWND hwnd = g.hwnd;
    // OrrePort's whole-disc check record (pc_paths' PC_PLACE_GAME_CACHE: data\system\cache\Pokemon Colosseum\ portable,
    // Colosseum\ in the older layout)
    const std::wstring check = g.orreport_root.empty() ? g.orreport_dir + L"\\Colosseum\\disc-check.txt"
                                                       : g.orreport_root + L"\\data\\system\\cache\\Pokemon Colosseum\\disc-check.txt";
    std::thread([path, hwnd, check] {
        auto* r = new ListResult();
        const auto t0 = std::chrono::steady_clock::now();
        char err[512] = "";
        OrreDisc* d = orre_disc_open(narrow(path).c_str(), err, sizeof err);
        if (d) r->identity = disc_identity(d, path, check);
        if (!d) r->error = err;
        else if (std::string(orre_disc_game_id(d)) != "GC6E01") {
            // only the US disc is read so far (the core's offsets are the US game's)
            r->error = "not the US Pokémon Colosseum disc (GC6E01)";
            orre_disc_close(d);
            d = nullptr;
        }
        if (d) {
            const int n = orre_species_count(d);
            for (int i = 0; i < n; ++i) {
                Entry e;
                e.index = i;
                if (!orre_species_get(d, i, &e.s) || e.s.national_dex == 0) continue;  // the empty slots and the egg
                e.name = title_case(widen(orre_text(d, e.s.name_msg)));
                e.category = title_case(widen(orre_text(d, e.s.category_msg)));
                OrreImage im{};
                if (orre_species_mug_shot(d, i, 0, 0, &im)) e.mug.assign(im.rgba, im.rgba + (size_t)im.width * im.height * 4);
                orre_image_free(&im);
                r->entries.push_back(std::move(e));
            }
            std::stable_sort(r->entries.begin(), r->entries.end(), [](const Entry& a, const Entry& b) { return a.s.national_dex < b.s.national_dex; });
        }
        r->ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        // the disc handle travels with the result (the window keeps it open for the session)
        static_assert(sizeof(LPARAM) >= sizeof(void*));
        if (!PostMessageW(hwnd, WM_LIST_LOADED, (WPARAM)d, (LPARAM)r)) { orre_disc_close(d); delete r; }
    }).detach();
}

// The file dialog for a disc image (Settings › Discs' Add and Replace, 2026-11-23; before, the "choose a disc" page's
// own button, which kept the picked path in studio.ini instead of copying it into the ROM folder). "" when cancelled.
int g_test_hover = -1;  // --hover <id> (screenshot tests: that spot drawn as hovered, 2026-11-23)
std::vector<std::wstring> g_test_picks;  // --pick-disc <file> (tests): the dialog's answers, in order, without the dialog
std::wstring pick_disc_file() {
    if (!g_test_picks.empty()) {
        std::wstring p = g_test_picks.front();
        g_test_picks.erase(g_test_picks.begin());
        return p;
    }
    // 4,096 characters, not MAX_PATH (260): the portable folder sits about 20 characters deeper, and Windows 10's long
    // paths go past 260 (Windows only; Linux / macOS dialogs return their own strings)
    static wchar_t file[4096];
    file[0] = 0;
    std::wstring filter_text = tx("studio.disc.filter");
    filter_text += L'\0';
    filter_text += L"*.iso;*.gcm;*.rvz;*.wia;*.ciso;*.gcz;*.nfs";
    filter_text += L'\0';
    filter_text += tx("studio.disc.filter_all");
    filter_text += L'\0';
    filter_text += L"*.*";
    filter_text += L'\0';
    const std::wstring title = tx("studio.disc.choose_title");
    OPENFILENAMEW of{};
    of.lStructSize = sizeof of;
    of.hwndOwner = g.hwnd;
    of.lpstrFilter = filter_text.c_str();
    of.lpstrFile = file;
    of.nMaxFile = (DWORD)(sizeof file / sizeof file[0]);
    of.lpstrTitle = title.c_str();
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_modal = "the disc dialog open";
    const BOOL picked = GetOpenFileNameW(&of);
    g_modal = nullptr;
    return picked ? std::wstring(file) : std::wstring();
}

// ---- layout ----
// The Pokémon page, v3 (snatcherLeo through 2026-11-18; reference: the project's sandbox v3, build-pc/mockups/studio-sandbox/v3.html
// in the main checkout, never committed). Top to bottom: the tab bar (Home, then one tab per open Pokémon); under it
// the rail, the list and the page. The page scrolls as a whole: the hero (a mug shot of 2 x 42 pixels, the name, type chips with
// coloured dots, category · National Dex number · height · weight, Copy / share and a faded "Create a mod", over a
// faint glow tinted by the first type), the era bar (a segmented control), the form row, the tabs, then the tab's body.
// Overview: three columns (the battle model; base stats over abilities; training over matchups) and evolution across
// the full width; two columns when the page is narrow. No inspector: explanations are hover cards.
constexpr float kPad = 28, kColGap = 14, kCardGap = 14, kCardHead = 40;
// the Cries card: its heading, two cry rows of 46, the "In Sound" link. Off since 2026-11-22 (snatcherLeo: the card made the
// grid "look odd"); the cries' buttons are in the page header now (draw_centre's information line), and kCriesCard
// brings the card back if wanted
constexpr bool kCriesCard = false;
constexpr float kCryRow = 46, kCriesH = kCardHead + 2 * kCryRow + 34;
constexpr float kFaintRowH = 64;  // the Sound tab's fainting cry row: two lines (in use today; the preview it plays)
constexpr float kHeroTop = 22, kHeroH = 76, kEraGap = 16, kEraH = 30, kTabsGap = 14, kTabsH = 38, kBodyGap = 18;
// the Moves tab's cards (mockup 25): the learnset on the left, the preview on the right (stacked, preview first, on a
// narrow page); the y values are the preview's parts, in window coordinates
constexpr float kLRow = 32, kLHead = 30, kNoteH = 72, kPickTile = 40, kPickGap = 8;
constexpr bool kShowOwnTag = false;  // the learnset's "own version" tag (hidden for now: snatcherLeo, 2026-11-19)
struct MovesGeo {
    Rect learn{}, prev{}, stage{};
    Rect used{};  // the Animations tab's Used by card, under its list (empty on the Moves tab)
    float forms_y = 0, forms_h = 0;  // the form picker (Pokémon with several forms: Castform, Unown, Deoxys)
    float view_y = 0, tl_y = 0, ctl_y = 0, opt_y = 0, info_y = 0, note_y = 0, sub_y = 0;
};
constexpr float kTimelineH = 34;  // the Moves tab's timeline bar
struct Layout {
    float w, h;
    Rect tabbar, list, page, centre, insp, content;
    // the Pokémon page, scrolled (window coordinates)
    float hero_y = 0, era_y = 0, tabs_y = 0, body_y = 0, bottom = 0;
    bool three = true;  // the Overview's three columns, or two on a narrow page
    Rect model, stats, train, abil, match, evo, forms, stage;
    // the Overview's Cries card (2026-11-22), under the abilities; empty while the card is off (kCriesCard). It must
    // start empty ({}): Rect has no defaults, and an unset one held leftover values, which drew pieces of the card at
    // random places and pushed Evolution down (snatcherLeo's screenshot, the same day)
    Rect cries{};
    MovesGeo mv;  // the Moves tab
};

// helpers defined further down, used by the page's geometry
float text_width(const std::wstring& s, float size, DWRITE_FONT_WEIGHT weight);
std::wstring type_name(int t);
std::vector<int> matchups(const OrreSpecies& s, int want);
float evo_height(int index, float width);
float match_height(const OrreSpecies& s, float width);

// The Overview's model card: where each control of its header and footer goes (*added 2026-11-22*, snatcherLeo: "We have to
// make it so that, even at a smaller window, things aren't obscured/hidden"). Before, a control that didn't fit its one
// row was left out (the title, the rate button, Game lighting on a smaller window); now the row wraps: the controls are
// laid right to left in order of importance, and one that doesn't fit starts a second row under the first, so the 3D
// view between them gets shorter instead. The same function places them for the page's geometry (layout: the stage's
// size) and for drawing (draw_model_card), so the two always agree.
struct BandItem {
    float w = 0, x = 0, y = 0;  // width; left edge and top once placed
    bool on = false;            // shown at all (Particles only for models with effects)
};
struct ModelBands {
    float head_h = 48, foot_h = 46;  // the header's and footer's heights (one row: 48 / 46; each more row +40)
    bool reset_word = false;         // Reset with its word, or the arrow alone
    BandItem idle, rate, reset;      // the header, right to left (the title stays at the left of the first row)
    BandItem shiny, stage, particles, lighting;  // the footer, right to left
    BandItem link;                   // "Change style… →", at the left of the footer's first row
    float hint_x = 0, hint_w = 0;    // the drag hint: on the footer's first row where room is left (a hint, not a control)
    float foot_y = 0;                // the footer's top
};
ModelBands model_bands(const Rect& M);  // (further down, beside draw_model_card)

// ---- the Moves tab's data and geometry (version 1, 2026-11-19) ----
// Reads the shown species' learnset once (moves-tab-plan.md section 1): level-up {level, move} (+0xBA), the TM / HM
// flags (+0x34) through main.dol's machine table, the egg moves (+0x74); each row with its move record and the effect
// this Pokémon's model plays (own version first, as wazaSequenceGetWazaResource looks). Then keeps the chosen row in
// the shown group (the group's first when it isn't there). Shadow Rush is in no learnset, so it never shows here.
int shown_stage() { return g.tab == kTabAnimations ? g.an_stage : g.arena ? 2 : g.plain ? 1 : 0; }
void ensure_learnset() {
    if (g.selected < 0 || !g.disc) { g.learn.clear(); g.learn_for = -1; return; }
    // (read again for another form too: the learnset is the species', but which moves have the form's own version
    // follows its model: Castform's weather forms have their own Sunny Day / Rain Dance / Hail, verified)
    if (g.learn_for != g.selected || g.learn_form != g.form) {
        g.learn_for = g.selected;
        g.learn_form = g.form;
        g.learn.clear();
        const OrreSpecies& s = g.all[g.selected].s;
        const uint32_t model_id = orre_species_form_model_id(g.disc, g.all[g.selected].index, g.form);
        auto add = [&](int group, int move, int how) {
            LearnRow r;
            r.group = group;
            r.move = move;
            r.how = how;
            if (move <= 0 || !orre_move_get(g.disc, move, &r.mv)) return;
            r.name = title_case(widen(orre_text(g.disc, r.mv.name_msg)));
            OrreMoveEffect fx{};
            r.has_effect = orre_move_effect(g.disc, move, model_id ? model_id : s.model_id, &fx) != 0;
            r.own = r.has_effect && fx.own_version;
            r.slot = r.has_effect ? fx.slot : -1;
            g.learn.push_back(std::move(r));
        };
        for (const auto& lm : s.level_moves)
            if (lm.move) add(0, lm.move, lm.level);
        for (int k = 0; k < 58; ++k)
            if (s.machines[k]) add(1, orre_machine_move(g.disc, k), k);
        for (uint16_t m : s.egg_moves)
            if (m) add(2, m, 0);
        // --any-move N (screenshot tests; the move-sweep comparisons play every move on Espeon, release step 4): that
        // move joins the level-up list when the Pokémon doesn't learn it by level (*corrected 2026-11-19:* the check
        // looked at every list, so a TM move such as Shadow Ball stayed out of the level-up list and the comparison
        // played that list's first move instead)
        if (g.any_move > 0 && std::none_of(g.learn.begin(), g.learn.end(),
                                           [](const LearnRow& r) { return r.move == g.any_move && r.group == 0; }))
            add(0, g.any_move, 0);
        // (Reactions, group 3, were listed here from 2026-11-19: Hit, Hit second motion, Faint, Idle 2, Idle 3. They
        // moved to the Animations tab on 2026-11-22, which lists every animation of the model: snatcherLeo, "move the
        // "Reactions" stuff out of the "Moves" sub-tab, put those in "Animations"". Group 3 is no longer filled.)
        // (the chosen move is kept when the next Pokémon learns it too: Thunderbolt on Pikachu, then on Raichu)
    }
    int first = 0;
    bool found = false;
    for (const LearnRow& r : g.learn) {
        if (r.group != g.learn_group) continue;
        if (!first) first = r.move;
        found = found || r.move == g.learn_move;
    }
    if (!found) g.learn_move = first;
}
const LearnRow* anim_row();  // (the Animations tab's chosen animation as a row: further down)
const LearnRow* chosen_row() {
    if (g.tab == kTabAnimations) return anim_row();
    for (const LearnRow& r : g.learn)
        if (r.group == g.learn_group && r.move == g.learn_move) return &r;
    return nullptr;
}
// the chosen move's body animation on the shown model (-1: none, or the model isn't read yet)
constexpr int kFaintTail = 45;  // the faint's held ending on the timeline, in battle frames (sync_preview)
// The two-turn moves (their part-loading class is fightWazaWzxTypeFuncTame in the game's waza_func_data table, verified
// from the code 2026-11-21): the charge turn plays part 3 alone, the attack turn parts 1 and 2
bool two_turn_move(int move) {
    static const int kTame[] = {13, 19, 76, 91, 117, 130, 143, 291, 340};  // Razor Wind, Fly, Solar Beam, Dig, Bide,
                                                                            // Skull Bash, Sky Attack, Dive, Bounce
    return std::find(std::begin(kTame), std::end(kTame), move) != std::end(kTame);
}
// The moves whose part 3 plays at a battle event of its own, not in the move's own calls (verified from the code,
// 2026-11-21): Substitute 164 (fightWazaWzxTypeFuncMigawari), Thief 168 and Covet 343 (the item-taking event, the
// battle-effect dispatcher), Helping Hand 270 (Tedasuke), Wish 273 (Negaigoto: when the wish comes true), Snatch 289
// (Spc). The preview offers them in a parts picker
bool event_part_move(int move) {
    static const int kEvent[] = {164, 168, 270, 273, 289, 343};
    return std::find(std::begin(kEvent), std::end(kEvent), move) != std::end(kEvent);
}
// whether the preview shows a move's part 3 alone (a two-turn move's Turn 1, or the picker's follow-up)
bool third_part_alone(int move) {
    return (g.turn == 1 && two_turn_move(move)) || (g.parts == 1 && event_part_move(move));
}
// the charge turn's own animation: the slot its part 3 file names (wazaSequencePokemonMotionStart plays the owner's
// slot animation with each sequence), read once per Pokémon, form and move
int turn1_slot(int move) {
    static int c_sp = -2, c_form = -2, c_move = -2, c_slot = -1;
    if (c_sp == g.selected && c_form == g.form && c_move == move) return c_slot;
    c_sp = g.selected;
    c_form = g.form;
    c_move = move;
    c_slot = -1;
    if (!g.disc || g.selected < 0) return -1;
    const uint32_t model_id = orre_species_form_model_id(g.disc, g.all[g.selected].index, g.form);
    OrreWzx* w = orre_move_wzx(g.disc, move, model_id ? model_id : g.all[g.selected].s.model_id, 3);
    OrreWzxHeader h{};
    if (w && orre_wzx_header(w, &h)) c_slot = h.slot;
    orre_wzx_free(w);
    return c_slot;
}
int chosen_animation() {
    const LearnRow* r = chosen_row();
    if (r && r->group == kAnimGroup) return g.viewer.animation_frames(r->anim) > 0 ? r->anim : -1;  // (the Animations tab)
    if (!r || r->slot < 0) return -1;
    // (a reaction plays only the model's own animation for its slot: an empty slot 11 would replay the idle)
    if (r->group == 3 && g.viewer.slot_own(r->slot) < 0) return -1;
    // (a two-turn move's charge turn, or the picker's follow-up: its part 3's own slot animation)
    if (r->group != 3 && third_part_alone(r->move)) {
        const int s = turn1_slot(r->move);
        const int a = s >= 0 ? g.viewer.slot_animation(s) : -1;
        if (g.viewer.animation_frames(a) > 0) return a;
    }
    const int a = g.viewer.slot_animation(r->slot);
    return g.viewer.animation_frames(a) > 0 ? a : -1;
}
// The 3D view follows the tab: on Moves, Plain stage as chosen and the camera framing the chosen move too; anywhere
// else the Overview's own view (no move playing, Studio's camera, the idle alone framed), so the Overview's pictures
// never change because of the Moves tab
void ensure_arena_model();
void sync_move_effects();
void sync_preview() {
    // a different tab: what played on the last one ends here, its effects (particles, trails, the target's half) too
    if (g.fx_tab != g.tab) {
        if (g.fx_tab >= 0) {
            g.viewer.stop_once();
            g.viewer.set_move_effect(nullptr, nullptr, 0, 0, -1);
            g.viewer.set_move_after(nullptr);
        }
        g.fx_tab = g.tab;
        g.fx_dirty = true;
    }
    const bool moves = preview_tab(g.tab) && g.selected >= 0 && !on_app_page();
    if (!moves) {
        g.viewer.stop_once();
        // the Overview's own stage (*added 2026-11-22*; snatcherLeo: "Overview stays Studio, so its own independent viewer ...
        // But, users can toggle across those 3 as well in Overview ... to view the model in those 3 different areas
        // just like Moves"): Studio view unless picked on the Overview's card, never the Moves tab's choice. Before,
        // the Arena chosen on Moves stayed on here (only the Plain stage was switched off)
        // (--game-camera with --plain: the move viewer's grey backdrop behind the Overview's model too, for the
        // all-models check against the game's _BACKDROP shots, 2026-11-19; screenshot tests only)
        g.viewer.set_plain_stage(g.ov_stage != kStageStudio || (g.plain && !g.game_camera.empty()));
        // the same angle on every stage: only the backdrop changes (snatcherLeo, 2026-11-22: "Studio view and the other 2
        // backgrounds should stay at the same angle/perspective in the Overview sub-tab")
        g.viewer.set_stage_camera(g.game_camera.empty());
        if (g.ov_stage == kStageArena) ensure_arena_model();
        g.viewer.set_arena(g.ov_stage == kStageArena);
        g.viewer.set_frame_animation(-1);
        return;
    }
    ensure_learnset();
    // The Moves tab: the game's move-viewer angle on the Plain stage and the Arena. The Animations tab: Studio's own
    // front camera on every stage, as the Overview (snatcherLeo, 2026-11-22: "why is the animations tab model viewer from the
    // side profile? We should just have it at a normal angle"), with its own stage choice (an_stage)
    const bool anims = g.tab == kTabAnimations;
    g.viewer.set_stage_camera(anims);
    g.viewer.set_plain_stage(shown_stage() != 0);
    ensure_arena_model();
    g.viewer.set_arena(shown_stage() == 2);
    const int anim = chosen_animation();
    g.viewer.set_frame_animation(anim);
    sync_move_effects();
}
// The arena's room for the 3D view (the Moves tab's Arena, and the Overview's own), read the first time it is shown
void ensure_arena_model() {
    // the arena's room, read from the disc the first time it is shown; nothing from the disc is kept outside memory.
    // *Changed 2026-11-22* (snatcherLeo: "can we replace "Phenac Stadium" with "Mt. Battle" battle arena instead? ... Would be
    // more plain of an area imo"; of five stages shown side by side he picked the valley, "This one is good"): Mt.
    // Battle's valley stage, D2_valley_1_bf entry 2 (the battle copy of the challenge's first area: a round platform,
    // rock walls, no crowd). It was waza_viewer entry 3, M1_water_colo.dat, the Phenac Colosseum (the game's own move
    // viewer room), which the move sweep still uses so its pictures match the game's (studio/tests/movebatch.py).
    // Test switch ORRE_ARENA=<archive>:<entry> loads any stage's map model instead (waza_viewer:3 for Phenac,
    // D2_crater_colo:6, D2_magma_1_bf:2, D2_cloud_1_bf:2 were the other candidates).
    if (g.disc && !g.viewer.has_arena_model()) {
        std::string arc = "D2_valley_1_bf";
        int entry = 2;
        if (const char* a = std::getenv("ORRE_ARENA")) {
            const std::string s = a;
            const size_t c = s.find(':');
            if (c != std::string::npos) { arc = s.substr(0, c); entry = std::atoi(s.c_str() + c + 1); }
        }
        OrreModel am{};
        if (orre_fsys_model(g.disc, arc.c_str(), entry, &am)) g.viewer.set_arena_model(&am);
        else logf(Level::Error, "viewer", "the arena's room (%s entry %d) couldn't be read", arc.c_str(), entry);
    }
}
// The Moves tab's move effects, target and battle camera for the chosen move (sync_preview's second half)
void sync_move_effects() {
    const int anim = chosen_animation();
    // the move's own effect file (release step 4): the user's half (part 1, or part 3 for moves aimed at the user),
    // this model's own version first, given to the 3D view for the move's next play. Read again only when the Pokémon,
    // its form, the move or its animation changes (a reaction has none)
    static int fx_species = -2, fx_form = -2, fx_move = -2, fx_anim = -2;
    const LearnRow* r = chosen_row();
    // the target: a Pokémon in the box's place (read once per choice), or the box again
    if (g.target_loaded != g.target_species && g.disc) {
        OrreModel tm{};
        if (g.target_species >= 0 && orre_species_model(g.disc, g.target_species, 0, &tm)) g.viewer.set_target_model(&tm);
        else g.viewer.set_target_model(nullptr);
        g.target_loaded = g.target_species;
    }
    // the faint's held ending (snatcherLeo, 2026-11-19: "for faint animations for each Pokemon, could we extend the duration
    // timeline, in case someone wants to see the entire thing of the Pokémon on the ground", then "Just slight
    // extension"): 45 more frames (1.5 seconds) on its last pose. Studio's own; the game moves on to the ball's return
    g.viewer.set_play_tail(r && r->group == 3 && r->slot == 10 ? anim : -1, kFaintTail);
    const int move = r && r->group != 3 && r->has_effect ? r->move : 0;
    static int fx_target = -2, fx_turn = -2, fx_parts = -2;
    if (g.fx_dirty || fx_species != g.selected || fx_form != g.form || fx_move != move || fx_anim != anim || fx_target != g.target_species ||
        fx_turn != g.turn || fx_parts != g.parts) {
        g.fx_dirty = false;
        fx_species = g.selected;
        fx_form = g.form;
        fx_move = move;
        fx_anim = anim;
        fx_target = g.target_species;
        fx_turn = g.turn;
        fx_parts = g.parts;
        OrreWzx *w = nullptr, *t = nullptr;
        int part = 1;
        // a two-turn move's charge turn (fightWazaWzxTypeFuncTame, verified), or the picker's follow-up: part 3 alone
        // on the user
        const bool charge = move > 0 && third_part_alone(move);
        if (charge && anim >= 0 && g.disc) {
            const uint32_t model_id = orre_species_form_model_id(g.disc, g.all[g.selected].index, g.form);
            part = 3;
            w = orre_move_wzx(g.disc, move, model_id ? model_id : g.all[g.selected].s.model_id, 3);
        } else if (move > 0 && anim >= 0 && g.disc) {
            const uint32_t model_id = orre_species_form_model_id(g.disc, g.all[g.selected].index, g.form);
            const uint32_t mid = model_id ? model_id : g.all[g.selected].s.model_id;
            w = orre_move_wzx(g.disc, move, mid, 1);
            if (!w) {
                part = 3;
                w = orre_move_wzx(g.disc, move, mid, 3);
            }
            // the target's half (part 2), played on the target box (snatcherLeo's design, moves-tab-plan.md 8b) or on the
            // chosen Pokémon. The box has no per-model versions, so the move's general file (model id 0xFFFF: matches
            // none); a Pokémon gets its own version first, as wazaSequenceGetWazaResource looks it up by the owner's
            // model (the move sweep's log names Umbreon's own Sand-Attack, for example)
            else {
                uint32_t tid = 0xFFFFu;
                if (g.target_species >= 0) {
                    OrreSpecies ts{};
                    if (orre_species_get(g.disc, g.target_species, &ts) && ts.model_id) tid = ts.model_id;
                }
                t = orre_move_wzx(g.disc, move, tid, 2);
            }
        }
        g.viewer.set_move_effect(w, t, move, part, anim);
        // the user's third part after the target's half. the project's preview rule (the project plan's "Moves with a third part in
        // the preview", 2026-11-20): the preview plays what a battle plays, in the battle's order. The battle's order
        // (verified from the code, 2026-11-21): each move's part-loading class is the fourth function of its row in the
        // game's waza_func_data table (0x8035C430, 5 functions per move id); the drains' class
        // (fightWazaWzxTypeFuncVampire: Absorb 71, Mega Drain 72, Leech Life 141, Giga Drain 202) and
        // fightWazaWzxTypeFuncAttDefSpc (Mimic 102, Dream Eater 138, Sketch 166, Trick 271, Skill Swap 285) play parts 1
        // and 2 in their first call and part 3 on the user in their second, after the hit. (The two-turn class,
        // fightWazaWzxTypeFuncTame, plays part 3 alone on the first turn: the Turn 1 / Turn 2 switch, later; Thief,
        // Covet, Substitute, Wish, Helping Hand and Snatch play theirs on their own events: the parts picker, later)
        OrreWzx* after = nullptr;
        static const int kAfterHit[] = {71, 72, 141, 202, 102, 138, 166, 271, 285};
        const bool after_hit = t && std::find(std::begin(kAfterHit), std::end(kAfterHit), move) != std::end(kAfterHit);
        const bool both = g.parts == 2 && event_part_move(move);  // (the picker's "Both": the move, then its follow-up)
        if (w && part == 1 && (after_hit || both)) {
            const uint32_t model_id = orre_species_form_model_id(g.disc, g.all[g.selected].index, g.form);
            after = orre_move_wzx(g.disc, move, model_id ? model_id : g.all[g.selected].s.model_id, 3);
        }
        g.viewer.set_move_after(after);
    }
}

// The Idle / Standing pose switch as shown: the user's choice, and Standing pose only when the shown model really has no
// idle (not while the next model loads)
bool shown_idle() { return g.viewer.idle() && (!g.viewer.has_model() || g.viewer.has_idle()); }

// The timeline's playhead follows a playing animation: the page is redrawn 30 times a second while one plays (the
// animation's own rate), and the timer stops when it ends or is held. Windows timer; Linux / macOS: the toolkit's
// frame or interval timer
void playhead_timer() {
    if (!g.hwnd) return;
    const bool run = preview_tab(g.tab) && g.viewer.playing() >= 0 && !g.viewer.held();
    if (run) SetTimer(g.hwnd, TIMER_PLAYHEAD, 33, nullptr);
    else KillTimer(g.hwnd, TIMER_PLAYHEAD);
}
// a point on the timeline -> the frame under it, held there. The page is painted at once (UpdateWindow): Windows hands
// out mouse moves before paints, so during a drag a plain invalidate left the playhead behind until the mouse stopped
// (snatcherLeo, 2026-11-19: "just make the cursor icon thingy move with my drag"). Other OSes: repaint the view synchronously
// the same way (the toolkit's immediate repaint)
void scrub_to(float x) {
    const int a = chosen_animation();
    if (a < 0 || g.tl_frames < 2 || g.tl_x1 <= g.tl_x0) return;
    const int f = std::clamp((int)std::lround((x - g.tl_x0) / (g.tl_x1 - g.tl_x0) * (float)(g.tl_frames - 1)), 0, g.tl_frames - 1);
    if (g.viewer.playing() == a && g.viewer.held() && g.viewer.once_frame() == f) return;  // (same frame: nothing to do)
    g.viewer.seek(a, f);
    playhead_timer();
    InvalidateRect(g.hwnd, nullptr, FALSE);
    UpdateWindow(g.hwnd);
}

// the faded "later" buttons under "Make it your own", wrapped into rows: their total height in width w
const char* kMakeOwn[] = {"studio.moves.swap_anim", "studio.moves.swap_particles", "studio.moves.attach", "studio.moves.sounds",
                          "studio.moves.reaction"};
float make_own_height(float w) {
    float x = 0, rows = 1;
    for (const char* k : kMakeOwn) {
        const float bw = text_width(tx(k), 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 24;
        if (x > 0 && x + bw > w) { ++rows; x = 0; }
        x += bw + 8;
    }
    return rows * 30 + (rows - 1) * 8;
}

// ---- the Animations tab (2026-11-22; mockups 28 and 29, snatcherLeo's notes: the model's animations by name, as a 3D tool
// lists them, no slots shown; "Used by" lists the moves that play the chosen one) ----
// The order the game's uses name an animation in: the idle, the move motions, then the reactions. A shared animation
// takes the name of its first use here; slot 9 (a hit then the faint, two animations in a row: animation-catalog.md,
// verified) names none. Names are the project's readings of the slots (likely), studio.anim.name.<slot>
const int kAnimNameOrder[] = {0, 1, 2, 3, 4, 6, 13, 12, 14, 15, 5, 7, 8, 10, 11, 16};
int anim_first_use(int anim) {
    for (int k = 0; k < (int)std::size(kAnimNameOrder); ++k)
        if (g.viewer.slot_own(kAnimNameOrder[k]) == anim) return kAnimNameOrder[k];
    return -1;
}
// Animations nothing in the game plays whose purpose is known, named as the use they were made for (species, animation,
// the slot whose name it takes). Hypno's animation 8 is its faint: its faint slot (10) points at the idle and the
// 119-frame animation 8 is played by no slot (likely; engine docs, unused-content.md and animation-catalog.md; XD plays
// it). snatcherLeo, 2026-11-22: "that is indeed its Fainting animation, that was not used in game ... We can call it Faint,
// and maybe not have that tag for now", so these rows show the name and no "not played" tag. Plain C++, every OS.
// *Updated 2026-11-22:* the faints while flying, each confirmed by snatcherLeo by eye in his stress test (snatcherLeo: "Those were all
// the ones I saw for specifically "Fainting while Fly"", and for the Pidgey line "Animation 6 is the unused fainting
// while flying animation"; the survey of every model's unplayed animations: animation-catalog.md). They're named
// "Faint while flying" with a note that the game doesn't use them (snatcherLeo: "mentioned how its unused in base game").
// Species by National Dex number (Hoenn's internal numbers differ). The Pidgey line's animation 7 is left unnamed (snatcherLeo's
// guess: the summary screen's idle, seen in XD's PC box).
struct KnownUnused {
    int dex, anim;
    const char* key;  // the name's language key
    bool note;        // show "unused in the game" beside the name
};
constexpr KnownUnused kKnownUnused[] = {
    {97, 8, "studio.anim.name.10", false},  // Hypno: its faint
    {16, 6, "studio.anim.faint_flying", true}, {17, 6, "studio.anim.faint_flying", true}, {18, 6, "studio.anim.faint_flying", true},
    {83, 8, "studio.anim.faint_flying", true},  {84, 8, "studio.anim.faint_flying", true}, {85, 9, "studio.anim.faint_flying", true},
    {144, 2, "studio.anim.faint_flying", true}, {146, 3, "studio.anim.faint_flying", true},
    {163, 7, "studio.anim.faint_flying", true}, {164, 8, "studio.anim.faint_flying", true},
    {225, 6, "studio.anim.faint_flying", true}, {249, 9, "studio.anim.faint_flying", true},
    {250, 2, "studio.anim.faint_flying", true}, {276, 7, "studio.anim.faint_flying", true},
    {277, 10, "studio.anim.faint_flying", true}, {279, 7, "studio.anim.faint_flying", true},
    {333, 10, "studio.anim.faint_flying", true}, {334, 11, "studio.anim.faint_flying", true},
    {357, 8, "studio.anim.faint_flying", true},
    // alternate idles the game doesn't play (snatcherLeo, the same evening: "For Noctowl Animation 1, just say it's an "Idle
    // (Alternate)" and we can say it is "unused in the game" ... Same thing for Spearow Animation 8")
    {164, 1, "studio.anim.idle_alt", true}, {21, 8, "studio.anim.idle_alt", true},
};
// the name shown for animation `anim`; `unused` set when nothing in the game plays it (no use, not the eyes') and its
// purpose isn't known; `noted` when it's a known one the game doesn't use (the "unused in the game" note)
std::wstring anim_name(int anim, bool* unused = nullptr, bool* noted = nullptr) {
    if (noted) *noted = false;
    if (g.selected >= 0)
        for (const auto& k : kKnownUnused)
            if (k.dex == g.all[g.selected].s.national_dex && k.anim == anim && anim_first_use(anim) < 0) {
                if (unused) *unused = false;
                if (noted) *noted = k.note;
                return tx(k.key);
            }
    const int s = anim_first_use(anim);
    if (unused) *unused = s < 0 && !g.viewer.eye_animation(anim);
    if (s >= 0) return tx(("studio.anim.name." + std::to_string(s)).c_str());
    if (g.viewer.eye_animation(anim)) return tx("studio.anim.eyes", {{"n", std::to_wstring(anim)}});
    return tx("studio.anim.numbered", {{"n", std::to_wstring(anim)}});
}
// the model's animations in list order: by first use (kAnimNameOrder), then the rest by number
std::vector<int> anim_order() {
    const int n = g.viewer.animation_count();
    std::vector<int> v(n);
    for (int a = 0; a < n; ++a) v[a] = a;
    auto rank = [](int a) {
        const int s = anim_first_use(a);
        for (int k = 0; k < (int)std::size(kAnimNameOrder); ++k) if (kAnimNameOrder[k] == s) return k;
        return 100 + a;
    };
    std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return rank(a) < rank(b); });
    return v;
}
// the Animations tab's chosen animation as a learnset-like row (group 4), so the Moves preview plays it: no move, so
// no effects, particles or target (snatcherLeo: "so no particles, just the model doing the animation")
const LearnRow* anim_row() {
    static LearnRow r;
    const int n = g.viewer.animation_count();
    if (n <= 0) return nullptr;
    const int key = g.selected * 64 + g.form;  // (another Pokémon or form: its list's first again)
    if (g.anim_for == -2) g.anim_for = key;
    else if (g.anim_for != key) { g.anim_for = key; g.anim_sel = -1; g.used_scroll = 0; }
    if (g.anim_sel < 0 || g.anim_sel >= n) g.anim_sel = anim_order().front();  // (the list's first: Idle, usually)
    r = LearnRow{};
    r.group = kAnimGroup;
    r.anim = g.anim_sel;
    r.move = kAnimBase + g.anim_sel;
    r.name = anim_name(g.anim_sel);
    return &r;
}
// the animation a learnset move plays on the shown model (the slot its effect file names; -1: none)
int move_anim(const LearnRow& r) {
    if (!r.has_effect || r.slot < 0) return -1;
    const int a = g.viewer.slot_animation(r.slot);
    return g.viewer.animation_frames(a) > 0 ? a : -1;
}
// The Used by card's chips: the moves of the learnset (level-up, TM / HM, egg; each move once) that play the chosen
// animation, laid side by side and wrapping in `width`. Past three rows they scroll inside the card (snatcherLeo: "a scrollable
// list form if we reach a certain length of moves, but still present them side to side")
std::wstring learn_how(const LearnRow& r);  // (the learnset's first column: further down)
// a Used by chip's small label: how the move is learned ("Lv 5", "TM01", "Egg")
std::wstring chip_how(const LearnRow& r) {
    if (r.group == 0) return tx("studio.anim.chip_level", {{"n", std::to_wstring(r.how)}});
    if (r.group == 1) return learn_how(r);
    return tx("studio.anim.chip_egg");
}
struct UsedChip { int row; float x, y, w; };
constexpr float kChipH = 28, kChipGap = 6, kUsedRows = 3;
std::vector<UsedChip> used_chips(float width, int* rows) {
    std::vector<UsedChip> out;
    std::vector<int> seen;
    float x = 0, y = 0;
    int lines = 0;
    for (int i = 0; i < (int)g.learn.size(); ++i) {
        const LearnRow& r = g.learn[(size_t)i];
        if (r.group > 2 || move_anim(r) != g.anim_sel || std::find(seen.begin(), seen.end(), r.move) != seen.end()) continue;
        seen.push_back(r.move);
        const float w = text_width(r.name, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + text_width(chip_how(r), 11, DWRITE_FONT_WEIGHT_NORMAL) + 30;
        if (x > 0 && x + w > width) { x = 0; y += kChipH + kChipGap; }
        if (out.empty() || x == 0) ++lines;
        out.push_back({i, x, y, std::min(w, width)});
        x += w + kChipGap;
    }
    if (rows) *rows = lines;
    return out;
}

MovesGeo moves_geo(float x0, float y0, float cw, float page_top) {
    MovesGeo G;
    const bool two = cw >= 900;
    const float lw = two ? std::floor((cw - kColGap) * 1.05f / 2.05f) : cw;
    const float pw = two ? cw - lw - kColGap : cw;
    int n = 0;
    const bool anims = g.tab == kTabAnimations;
    if (anims) n = g.viewer.animation_count() + 1;  // (+1: the faded "Add an animation" row)
    else for (const LearnRow& r : g.learn) n += r.group == g.learn_group;
    // (the Animations list has no group buttons: its rows start right under the title)
    const float lh = (anims ? 44 : 44 + 40) + kLHead + std::max(1, n) * kLRow + 16;
    // the preview: its header, the view row, the 3D stage, the controls, the facts, the version note, Make it your own
    const float sh = std::clamp(std::floor((pw - 32) * 0.64f), 240.0f, 380.0f);
    float y = 44;
    // the form picker (snatcherLeo, 2026-11-19: "The form picker to the moves tab will be good, as that'll help us later"):
    // one row of thumbnails (or more for Unown's 28 letters), named under them when there are few
    const float forms_at = y;
    float forms_h = 0;
    if (g.forms.size() > 1) {
        const int per = std::max(1, (int)std::floor((pw - 32 + kPickGap) / (kPickTile + kPickGap)));
        const int lines = ((int)g.forms.size() + per - 1) / per;
        forms_h = lines * (kPickTile + (g.forms.size() <= 8 ? 18.0f : 0.0f) + kPickGap) + 6;
    }
    y += forms_h;
    const float view = y;
    y += 40;
    const float stage = y;
    y += sh + 10;
    // the timeline under the stage (snatcherLeo, 2026-11-19: "like in Blender when you are viewing an animation")
    const float tl = y;
    y += kTimelineH + 10;
    const float ctl = y;
    y += 30 + 6;
    // the view's switches on their own row (Particles, Game lighting, Battle camera: release step 4; on the controls
    // row they didn't fit beside the frame counter in a narrow preview)
    const float opt = y;
    y += 30 + 14;
    const float info = y;
    y += 4 * 24 + 44 + 10;
    const float note = y;
    y += kNoteH + 16;
    const float sub = y;
    y += 1 + 12 + 26 + make_own_height(pw - 32) + 16;
    const float ph = y;
    if (two) {
        G.learn = {x0, y0, lw, lh};
        // the preview stays in view while a long learnset scrolls by (it never leaves the learnset's height)
        const float py = std::max(y0, std::min(page_top + 12, y0 + lh - ph));
        G.prev = {x0 + lw + kColGap, py, pw, ph};
    } else {
        G.prev = {x0, y0, pw, ph};
        G.learn = {x0, y0 + ph + kCardGap, lw, lh};
    }
    G.forms_y = G.prev.y + forms_at;
    G.forms_h = forms_h;
    G.view_y = G.prev.y + view;
    G.tl_y = G.prev.y + tl;
    G.ctl_y = G.prev.y + ctl;
    G.opt_y = G.prev.y + opt;
    G.info_y = G.prev.y + info;
    G.note_y = G.prev.y + note;
    G.sub_y = G.prev.y + sub;
    G.stage = {G.prev.x + 16, G.prev.y + stage, G.prev.w - 32, sh};
    // the Animations tab's Used by card, under the list: its chips' rows (at most three shown; more scroll), or one
    // line of text when no move plays the animation
    if (anims) {
        int rows = 0;
        used_chips(G.learn.w - 32, &rows);
        const float body = rows > 0 ? std::min<float>((float)rows, kUsedRows) * (kChipH + kChipGap) : 26;
        G.used = {G.learn.x, G.learn.y + G.learn.h + kCardGap, G.learn.w, kCardHead + body + 34};
    }
    return G;
}

// the page's width (between the list and the window's right edge)
float page_width() {
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    return std::max(200.0f, rc.right / g.scale - kRail - kList);
}

bool disc_gone_bar();  // (the US disc in use was removed: Settings › Discs, below)
Layout layout() {
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    Layout L{};  // (every rectangle starts empty: one a page doesn't set reads as nothing, never as leftover memory)
    L.w = rc.right / g.scale;
    L.h = rc.bottom / g.scale;
    L.tabbar = {0, 0, L.w, kTabBar};
    L.list = {kRail, kTabBar, kList, L.h - kTabBar};
    L.page = {kRail + kList, kTabBar, std::max(200.0f, L.w - kRail - kList), L.h - kTabBar};
    if (on_app_page() && g.disc) {  // (Settings too)
        // Home has no list (snatcherLeo, 2026-11-19: "just have clicking the home button not show Pokémon"): its page starts
        // at the rail. (The rail's editors will move onto Home later: "It'll eventually be where we put all our icons
        // for all the editors and stuff, but not yet")
        L.list = {kRail, kTabBar, 0, L.h - kTabBar};
        L.page = {kRail, kTabBar, std::max(200.0f, L.w - kRail), L.h - kTabBar};
    }
    // the bar after an unexpected close takes a strip at the page's top, so it covers nothing (draw_crash_bar); the
    // removed-disc bar uses the same strip (draw_disc_bar; the crash bar first when both would show)
    if ((g.crash_bar && !g.report_open && !g_windows.empty() && g_windows.front().hwnd == g.hwnd) || disc_gone_bar()) {
        constexpr float kCrashBar = 64;
        L.page.y += kCrashBar;
        L.page.h -= kCrashBar;
    }
    L.centre = L.page;
    L.insp = {L.w, 0, 0, 0};
    const float x0 = L.page.x + kPad, cw = L.page.w - 2 * kPad;
    L.hero_y = L.page.y + kHeroTop - g.pscroll;
    L.era_y = L.hero_y + kHeroH + kEraGap;
    L.tabs_y = L.era_y + kEraH + 12;
    L.body_y = L.tabs_y + kTabsH + kBodyGap;
    L.content = {x0, L.body_y, cw, std::max(100.0f, L.page.y + L.page.h - L.body_y - 18)};
    L.bottom = L.body_y + 200;
    if (g.selected < 0 || !g.disc) return L;
    const OrreSpecies& s = g.all[g.selected].s;
    // the Overview's cards
    L.three = cw >= 820;
    const float stats_h = kCardHead + 6 * 28 + 48, train_h = kCardHead + 8 * 27.5f + 12;
    const float row1 = std::max(stats_h, train_h);
    if (L.three) {
        const float c1 = std::floor((cw - 2 * kColGap) * 0.95f / 2.95f), c2 = std::floor((cw - 2 * kColGap - c1) / 2);
        const float x2 = x0 + c1 + kColGap, x3 = x2 + c2 + kColGap, c3 = x0 + cw - x3;
        const float abil_h = kCardHead + 3 * 46 + 6, match_h = std::max(abil_h, match_height(s, c3));
        const float row2 = std::max(abil_h, match_h);
        L.stats = {x2, L.body_y, c2, row1};
        L.train = {x3, L.body_y, c3, row1};
        // (the abilities card keeps its own height too, so it ends level with the model card; only the matchups card
        // grows past them when its chips need the room. 2026-11-22, with the model card's fixed height below)
        L.abil = {x2, L.body_y + row1 + kCardGap, c2, abil_h};
        L.match = {x3, L.body_y + row1 + kCardGap, c3, row2};
        // The model card's height doesn't follow the Pokémon (*changed 2026-11-22*, snatcherLeo: "Yes, fix its height"): it is
        // the stats / training row plus the abilities card, which never change, not the matchups card, whose rows of
        // type chips do. Before, a pick that changed the matchups' height resized the 3D view, and every white flash
        // seen came on such a pick (shrinking it; the 3D view's resize note in viewer.cpp); the card also jumped in
        // size between Pokémon. When the matchups card is taller, the model card ends above it.
        L.model = {x0, L.body_y, c1, std::max(440.0f, row1 + kCardGap + abil_h)};
        // the Cries card (snatcherLeo, 2026-11-22: his pick C of mockup 27, "a small cries card"): under the abilities, in the
        // middle column, so the model card above keeps its fixed height
        if (kCriesCard) L.cries = {x2, L.abil.y + L.abil.h + kCardGap, c2, kCriesH};
    } else {
        const float c = std::floor((cw - kColGap) / 2), x2 = x0 + c + kColGap;
        L.model = {x0, L.body_y, cw, 380};
        const float y1 = L.body_y + 380 + kCardGap;
        L.stats = {x0, y1, c, row1};
        L.train = {x2, y1, cw - c - kColGap, row1};
        const float abil_h = kCardHead + 3 * 46 + 6, row2 = std::max(abil_h, match_height(s, cw - c - kColGap));
        L.abil = {x0, y1 + row1 + kCardGap, c, row2};
        L.match = {x2, y1 + row1 + kCardGap, cw - c - kColGap, row2};
        if (kCriesCard) L.cries = {x0, L.abil.y + L.abil.h + kCardGap, c, kCriesH};
    }
    const float evo_y = std::max({L.model.y + L.model.h, L.match.y + L.match.h, L.cries.y + L.cries.h}) + kCardGap;
    L.evo = {x0, evo_y, cw, evo_height(g.all[g.selected].index, cw)};
    L.forms = {x0, L.evo.y + L.evo.h + kCardGap, cw, form_card_h()};
    // the 3D view: the model card between its header and its footer (one row each, 48 and 46, or more on a narrow card:
    // model_bands)
    const ModelBands mb = model_bands(L.model);
    L.stage = {L.model.x + 1, L.model.y + mb.head_h, L.model.w - 2, std::max(60.0f, L.model.h - mb.head_h - mb.foot_h)};
    L.bottom = g.tab != kTabOverview ? L.body_y + 420 : L.forms.h > 0 ? L.forms.y + L.forms.h : L.evo.y + L.evo.h;
    if (g.tab == kTabLooks) L.bottom = L.body_y + 860;  // (the Styles tab: its two cards, then the pictures under them)
    if (g.tab == kTabSound) L.bottom = L.body_y + 640;  // (the Sound tab: the cries and where they play, stacked when narrow)
    if (preview_tab(g.tab)) {
        ensure_learnset();
        L.mv = moves_geo(x0, L.body_y, cw, L.page.y);
        L.stage = L.mv.stage;
        // (the sticky preview never reaches below the learnset, so the page ends at the lower card's foot; on the
        // Animations tab the Used by card is the lower one)
        const float low = std::max(L.mv.learn.y + L.mv.learn.h, L.mv.used.y + L.mv.used.h);
        L.bottom = std::max(low, cw >= 900 ? L.body_y + L.mv.prev.h : L.mv.prev.y + L.mv.prev.h);
    }
    return L;
}

// the page's scroll range (the whole page scrolls; the tab bar stays)
void clamp_page_scroll() {
    const Layout L = layout();
    g.pscroll_max = std::max(0.0f, (L.bottom + g.pscroll + 24) - (L.page.y + L.page.h));
    g.pscroll = std::clamp(g.pscroll, 0.0f, g.pscroll_max);
}

// The global search's places (DIPs): the top bar's field over the rail and list (where Home was), its drop-down
// under it, and the palette centred near the top of the window
constexpr float kSResRow = 40, kSResDropRows = 8, kSResPalRows = 10;
// (from the list's right edge, not the page's: Home's page starts at the rail, and the bar mustn't move with it)
Rect gfield_rect(const Layout& L) { return {72, 5, std::max(120.0f, std::min(L.w, kRail + kList) - 72 - 14), 26}; }
constexpr float kSNote = 26;  // the "searched as" line above the results, when there is one
Rect drop_rect(const Layout& L) {
    const Rect f = gfield_rect(L);
    const float rows = std::max(1.0f, std::min(kSResDropRows, (float)g.sres.size()));
    return {f.x, kTabBar + 2, std::max(f.w, 360.0f), (g.snote.empty() ? 0 : kSNote) + rows * kSResRow + 12};
}
Rect pal_rect(const Layout& L) {
    const float w = std::min(560.0f, L.w - 40), rows = std::max(1.0f, std::min(kSResPalRows, (float)g.sres.size()));
    return {(L.w - w) / 2, 64, w, 52 + (g.snote.empty() ? 0 : kSNote) + rows * kSResRow + 12 + 30};
}

// ---- the 3D view drawn into the page (2026-11-22; snatcherLeo: "Let's just do it then") ----
// Why: the view as a child window with its own swap chain is a second picture Windows glues onto the page, and its
// resizes flashed white now and then (likely the GPU overlay path, MPO); one picture per window removes that, and the
// clipping workarounds. How: a texture on the page's Direct3D 11 device, shared by its DXGI NT handle with a keyed mutex
// (Dawn's SharedTextureMemoryDXGISharedHandle); the view copies each frame into it (Viewer::share_into) and the page
// draws it as a Direct2D bitmap in the model card. *Default since 2026-11-22* (after the trial behind ORRE_PAGE_VIEW=1:
// no flashes in snatcherLeo's mixed run, "looked smooth"); ORRE_CHILD_VIEW=1 keeps the old child window (comparisons), and a
// PC whose graphics can't share the texture falls back to it by itself (g_page_failed).
// Windows only (D3D11 / DXGI sharing); Linux / macOS: the core's picture shown by the shell (Vulkan / Metal sharing).
bool g_page_failed = false;
bool page_view() { static const bool child = std::getenv("ORRE_CHILD_VIEW") != nullptr; return !child && !g_page_failed; }
void release_shared() {
    if (g.share_bmp) { g.share_bmp->Release(); g.share_bmp = nullptr; }
    if (g.share_mutex) { g.share_mutex->Release(); g.share_mutex = nullptr; }
    if (g.share_tex) { g.share_tex->Release(); g.share_tex = nullptr; }
    g.share_w = g.share_h = 0;
}
// The page's own copy of the latest 3D frame (*added 2026-11-22*; snatcherLeo: "if i resize the window ... by dragging the
// right or left side to extend window slowly, the model viewer area does flicker"). Before, the page drew the shared
// texture itself, holding its keyed mutex for the whole draw, and two things left the 3D view's area empty for a frame:
// a paint that couldn't take the mutex within 20 ms (the view busy writing its frame) drew the page without the 3D
// picture, and after a resize the new texture was empty until the view's first frame at the new size. A resize drag
// repaints on nearly every step while the view redraws at each new size, so both came often: the flicker (likely; the
// scripted resize test couldn't show which, the screen isn't read). Now the mutex is held only to copy a new frame into
// the page's own texture (a GPU copy, CopyResource), and every draw shows that copy: the last frame when no newer one
// could be taken, stretched to the new size until the first frame at that size arrives. Windows only (D3D11 keyed
// mutex); Linux / macOS: the same idea with their shared textures.
void release_frame_copy() {
    if (g.copy_bmp) { g.copy_bmp->Release(); g.copy_bmp = nullptr; }
    if (g.copy_tex) { g.copy_tex->Release(); g.copy_tex = nullptr; }
    g.copy_w = g.copy_h = 0;
}
bool take_shared_frame() {
    if (!page_view() || !g.share_tex || !g.share_mutex || !g.d3d || !g.rt) return false;
    if (g.viewer.shared_frames() <= g.share_frames_at) return false;  // (the texture has no frame yet: the copy stays)
    if (g.share_mutex->AcquireSync(0, 20) != S_OK) return false;     // (being written: the copy stays, the next comes)
    if (!g.copy_tex || g.copy_w != g.share_w || g.copy_h != g.share_h) {
        release_frame_copy();
        D3D11_TEXTURE2D_DESC td{};
        g.share_tex->GetDesc(&td);
        td.MiscFlags = 0;  // (a plain texture: not shared, no keyed mutex)
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        IDXGISurface* surf = nullptr;
        const float dpi = 96.0f * g.scale;
        const D2D1_BITMAP_PROPERTIES1 bp =
            D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi, dpi);
        if (SUCCEEDED(g.d3d->CreateTexture2D(&td, nullptr, &g.copy_tex)) &&
            SUCCEEDED(g.copy_tex->QueryInterface(__uuidof(IDXGISurface), reinterpret_cast<void**>(&surf))) &&
            SUCCEEDED(g.rt->CreateBitmapFromDxgiSurface(surf, &bp, &g.copy_bmp))) {
            g.copy_w = g.share_w;
            g.copy_h = g.share_h;
        } else {
            release_frame_copy();
        }
        if (surf) surf->Release();
    }
    if (g.copy_tex) {
        ID3D11DeviceContext* ctx = nullptr;
        g.d3d->GetImmediateContext(&ctx);
        ctx->CopyResource(g.copy_tex, g.share_tex);  // (queued; the mutex's release orders it before the view's next write)
        ctx->Release();
        g.viewer.shared_frame_size(g.copy_fw, g.copy_fh);
    }
    g.share_mutex->ReleaseSync(0);
    return g.copy_bmp != nullptr;
}
// The shared texture for a view of w x h pixels (the page's copy stays until a new frame). *Changed 2026-11-22*: it is
// sized in steps of 256 pixels and kept while the view fits it (and isn't much smaller), the view writing its frame into
// the top-left part. Made at the exact size, a window resize made a new texture on every step, and Dawn's import of
// each, the page's copy and the old textures were freed only a few seconds later: private memory rose by about 300 MB
// during a long resize (338 -> ~700 MB, back to ~370 MB within 5 s of stopping; the child-window view, with no
// sharing, rose by ~24 MB). Measured by a scripted resize, 200 steps.
void ensure_shared(int w, int h) {
    if (!g.d3d || !g.rt || w <= 0 || h <= 0) return;
    constexpr int kStep = 256;
    const int bw = (w + kStep - 1) / kStep * kStep, bh = (h + kStep - 1) / kStep * kStep;
    if (g.share_tex && w <= g.share_w && h <= g.share_h && g.share_w <= bw + kStep && g.share_h <= bh + kStep) return;
    w = bw;
    h = bh;
    g.viewer.share_into(nullptr, 0, 0);
    release_shared();
    g.share_frames_at = g.viewer.shared_frames();
    D3D11_TEXTURE2D_DESC td{};
    td.Width = (UINT)w;
    td.Height = (UINT)h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    if (const HRESULT hr = g.d3d->CreateTexture2D(&td, nullptr, &g.share_tex); FAILED(hr)) {
        logf(Level::Error, "page view", "no shared texture (%u x %u): %s", td.Width, td.Height, hr_text(hr).c_str());
        return;
    }
    IDXGIResource1* res = nullptr;
    HANDLE handle = nullptr;
    IDXGISurface* surf = nullptr;
    bool ok = SUCCEEDED(g.share_tex->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&g.share_mutex))) &&
              SUCCEEDED(g.share_tex->QueryInterface(__uuidof(IDXGIResource1), reinterpret_cast<void**>(&res))) &&
              SUCCEEDED(res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handle)) &&
              SUCCEEDED(g.share_tex->QueryInterface(__uuidof(IDXGISurface), reinterpret_cast<void**>(&surf)));
    if (ok) {
        const float dpi = 96.0f * g.scale;
        const D2D1_BITMAP_PROPERTIES1 bp =
            D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi, dpi);
        ok = SUCCEEDED(g.rt->CreateBitmapFromDxgiSurface(surf, &bp, &g.share_bmp));
    }
    if (ok) ok = g.viewer.share_into(handle, w, h);
    if (handle) CloseHandle(handle);  // (Dawn has opened the texture by now)
    if (surf) surf->Release();
    if (res) res->Release();
    if (!ok) {
        release_shared();
        g.viewer.set_page_mode(false);
        g_page_failed = true;  // (the old child window from now on: place_children's other branch shows it)
        logf(Level::Warn, "page view", "sharing the 3D view's frames failed: the 3D view keeps its own window");
        place_children();
        return;
    }
    g.share_w = w;
    g.share_h = h;
    g.viewer.set_page_mode(true);  // (the view draws into its own texture from now on; the page shows it)
}
// The model's mouse in page mode: the view's window shows nothing, so a press, a double-click or the wheel over the
// visible view (g.view_px, set by place_children) is handed to the view as if it came to its window; a drag then goes
// to the view's window directly (it takes the mouse: SetCapture). Double-clicks are timed here (the page's window class
// has no CS_DBLCLKS: Windows would turn the second of two quick clicks on the list into a double-click message, and the
// pick would be lost). Windows only; Linux / macOS: the toolkit's events on the card
bool view_mouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!page_view() || !g.viewer.hwnd() || g.viewer.shared_frames() == 0 || g.palette_open) return false;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (msg == WM_MOUSEWHEEL) ScreenToClient(hwnd, &p);  // (the wheel's point is on the screen)
    if (p.x < g.view_px.left || p.x >= g.view_px.right || p.y < g.view_px.top || p.y >= g.view_px.bottom) return false;
    if (g.drop_open) {  // (the search's drop-down over the view keeps its clicks)
        const Layout L = layout();
        const Rect d = drop_rect(L);
        if (p.x >= px(d.x) && p.x < px(d.x + d.w) && p.y >= px(d.y) && p.y < px(d.y + d.h)) return false;
    }
    POINT v = p;
    MapWindowPoints(hwnd, g.viewer.hwnd(), &v, 1);
    UINT m = msg;
    if (msg == WM_LBUTTONDOWN) {
        static DWORD last_t = 0;
        static POINT last_p{};
        const DWORD now = GetMessageTime();
        const bool dbl = now - last_t <= GetDoubleClickTime() && std::abs(p.x - last_p.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2 &&
                         std::abs(p.y - last_p.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2;
        last_t = dbl ? 0 : now;
        last_p = p;
        if (dbl) m = WM_LBUTTONDBLCLK;
    }
    g.viewer.proc(m, wp, msg == WM_MOUSEWHEEL ? lp : MAKELPARAM(v.x, v.y));
    return true;
}
// the shared frame drawn over the model card (paint, between the page and the overlays), clipped to the page
void draw_shared_view(const Layout& L) {
    if (!page_view() || !g.viewer.framed() || g.viewer.shared_frames() == 0) return;
    const bool list_page = g.editor == 0 && g.disc && !on_app_page();
    if (!list_page || g.selected < 0 || !(g.tab == kTabOverview || preview_tab(g.tab))) return;
    // the page's own copy of the latest frame (take_shared_frame): at the view's size, pixel for pixel; just after a
    // resize, the last frame at the old size, stretched until the first one at the new size arrives
    if (!g.copy_bmp || g.copy_fw <= 0 || g.copy_fh <= 0) return;
    const Rect& S = L.stage;
    const bool same = g.copy_fw == (int)px(S.w) && g.copy_fh == (int)px(S.h);
    static int stretched = 0;  // (paints that showed a stretched frame since the last one at the view's size: the log)
    if (!same) ++stretched;
    else if (stretched) {
        logf(Level::Detail, "page view", "after a resize: %d paints showed the last frame stretched", stretched);
        stretched = 0;
    }
    g.rt->PushAxisAlignedClip(D2D1::RectF(L.page.x, L.page.y, L.page.x + L.page.w, L.page.y + L.page.h), D2D1_ANTIALIAS_MODE_ALIASED);
    // (the frame is the copy's top-left part; the source rectangle is in the bitmap's DIPs, its DPI being the window's)
    const D2D1_RECT_F src = D2D1::RectF(0, 0, g.copy_fw / g.scale, g.copy_fh / g.scale);
    g.rt->DrawBitmap(g.copy_bmp, D2D1::RectF(S.x, S.y, S.x + S.w, S.y + S.h), 1.0f,
                     same ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
    g.rt->PopAxisAlignedClip();
}

// The list's search box holds text: its clear button shows (draw_list), and the edit is narrowed so long text never
// runs under it
bool list_search_has_text() { return g.search && GetWindowTextLengthW(g.search) > 0; }
constexpr float kClearR = 8, kClearRoom = 24;  // the clear button's radius; the room the edit gives up for it

Rect report_note_rect(const Layout& L);  // (Report a problem's note box: further down, with the panel)
void place_children() {
    clamp_page_scroll();
    const Layout L = layout();
    const bool list_page = g.editor == 0 && g.disc && !on_app_page();
    // the global search's fields (native edits: they bring typing, selection, the IME for Japanese and screen readers)
    if (g.gsearch) {
        const Rect f = gfield_rect(L);
        ShowWindow(g.gsearch, g.disc && !g.palette_open && !g.report_open && !g.goback_open ? SW_SHOW : SW_HIDE);
        // the edit fills the room between the magnifier and the "Ctrl K" hint, vertically centred in the frame
        MoveWindow(g.gsearch, (int)px(f.x + 28), (int)px(f.y + 5), (int)px(f.w - 28 - 56), (int)px(16), TRUE);
    }
    if (g.psearch) {
        const Rect p = pal_rect(L);
        ShowWindow(g.psearch, g.palette_open ? SW_SHOW : SW_HIDE);
        MoveWindow(g.psearch, (int)px(p.x + 46), (int)px(p.y + 15), (int)px(p.w - 110), (int)px(22), TRUE);
    }
    // the palette dims the whole window: the other child windows would paint over it, so they hide while it is open
    // (and while the top field's drop-down is open: it lies over the list's search box, which would paint over it)
    // *Updated 2026-11-22 (snatcherLeo, with a screenshot of a white bar where the box sits while the Pokémon editor first
    // loads: "can we make sure, while Pokémon Editor loads ... that white flashing bar is not there?"):* a native edit
    // shown for the first time has no picture until it paints, and Windows fills it white meanwhile; the page's first
    // load (the 3D view's setup, the model) keeps the thread busy for about half a second right then. So the box, when
    // it is to appear, waits for the page's next paint (WM_PAINT shows it), and appears with the page. Hiding stays
    // immediate. Windows-only (child edit windows); Linux / macOS: the toolkit draws its fields with the page.
    const bool want_search = list_page && !g.palette_open && !g.drop_open && !g.report_open && !g.goback_open;
    // Report a problem's note box, inside the panel's card (the panel draws its frame), while the form shows
    if (g.report_note) {
        const bool note = g.report_open && g.report_stage == 0;
        if (note) {
            const Rect n = report_note_rect(L);
            MoveWindow(g.report_note, (int)px(n.x), (int)px(n.y), (int)px(n.w), (int)px(n.h), TRUE);
        }
        ShowWindow(g.report_note, note ? SW_SHOW : SW_HIDE);
    }
    if (!want_search) {
        ShowWindow(g.search, SW_HIDE);
        g.search_deferred = false;
    } else if (!IsWindowVisible(g.search)) {
        g.search_deferred = true;
        InvalidateRect(g.hwnd, nullptr, FALSE);  // (a paint is coming, so the box surely shows)
    }
    // the edit control inside the drawn frame (kTabBar + 46 .. + 80), one text line tall, centred
    // (narrower while it holds text: the clear button takes the frame's right end, draw_list)
    MoveWindow(g.search, (int)px(L.list.x + 22), (int)px(kTabBar + 54),
               (int)px(L.list.w - 44 - (list_search_has_text() ? kClearRoom - 6 : 0)), (int)px(20), TRUE);
    // the 3D view: a child window over the model card, clipped to the page (the page scrolls under the tab bar)
    const Rect& S = L.stage;
    const float vx0 = std::max(S.x, L.page.x), vy0 = std::max(S.y, L.page.y);
    const float vx1 = std::min(S.x + S.w, L.page.x + L.page.w), vy1 = std::min(S.y + S.h, L.page.y + L.page.h);
    const bool stage = list_page && !g.palette_open && !g.report_open && !g.goback_open && g.selected >= 0 && g.cur_tab >= 0 && g.viewer.ok() &&
                       (g.tab == kTabOverview || preview_tab(g.tab)) && vx1 > vx0 + 4 && vy1 > vy0 + 4;
    // The view's window is never hidden: off the Overview, or before its first frame, it is cut to nothing (an empty
    // window region) and frozen, and keeps its last picture. Hiding and showing it again flashed white until the next
    // frame was drawn, on every switch back to the Overview and at launch (snatcherLeo, 2026-11-19). It is revealed only once a
    // frame with a model has been presented (the view posts WM_APP + 21 then).
    if (g.viewer.hwnd()) ShowWindow(g.viewer.hwnd(), SW_SHOW);
    if (stage) g.viewer.move((int)px(S.x), (int)px(S.y), (int)px(S.w), (int)px(S.h));  // (drawn at its size even while cut)
    if (!stage || !g.viewer.framed()) g.viewer.set_cut(true);
    if (stage && g.viewer.framed() && page_view()) {
        // the trial: the view's window stays, drawing on its timer, but with nothing of it on screen; its frames reach
        // the page through the shared texture (draw_shared_view)
        if (g.viewer.cut()) g.viewer.set_cut(false);
        SetWindowRgn(g.viewer.hwnd(), CreateRectRgn(0, 0, 0, 0), FALSE);
        ensure_shared((int)px(S.w), (int)px(S.h));
        g.view_px = {(LONG)px(vx0), (LONG)px(vy0), (LONG)px(vx1), (LONG)px(vy1)};
    } else if (page_view()) {
        g.view_px = {};  // (no view on screen: the page keeps its clicks)
    } else if (stage && g.viewer.framed()) {
        // Windows only: the visible part as the child's window region (Qt / AppKit clip a child view by its parent),
        // less the search drop-down where it reaches over the view.
        // *Changed 2026-11-22* (snatcherLeo: "a strange white flash sometimes when I click from Pokemon to Pokemon in quick
        // succession ... in the model viewer area"): the region is set only when it changes. place_children runs on every
        // pick, and each SetWindowRgn (with its forced redraw) on the view's GPU-presented window is a moment where
        // Windows' compositor can show one blank frame (likely; not reproduced in 4 screen-reading runs, 80+ jumps and
        // snatcherLeo's own 60 s watch, so the cause isn't verified). Picking another Pokémon on the same page now leaves the
        // region alone. The cut always sets it again (set_cut empties it), so the key resets when the view was cut.
        const bool was_cut = g.viewer.cut();
        g.viewer.set_cut(false);
        const int rx0 = (int)px(vx0 - S.x), ry0 = (int)px(vy0 - S.y), rx1 = (int)px(vx1 - S.x), ry1 = (int)px(vy1 - S.y);
        Rect d{};
        if (g.drop_open) d = drop_rect(L);
        // (the view's size is part of the key: Viewer::move empties the region while it resizes)
        const std::array<int, 11> key{rx0, ry0, rx1, ry1, g.drop_open ? 1 : 0, (int)px(d.x - S.x), (int)px(d.y - S.y),
                                      (int)px(d.x + d.w - S.x), (int)px(d.y + d.h - S.y), (int)px(S.w), (int)px(S.h)};
        if (was_cut || key != g.view_region) {
            g.view_region = key;
            logf(Level::Info, "surface", "view region set: %d,%d to %d,%d of %dx%d%s", rx0, ry0, rx1, ry1, key[9], key[10],
                 g.drop_open ? ", less the search drop-down" : "");  // (flash suspects: logged on every change)
            HRGN rg = CreateRectRgn(rx0, ry0, rx1, ry1);
            if (g.drop_open) {
                HRGN hole = CreateRectRgn(key[5], key[6], key[7], key[8]);
                CombineRgn(rg, rg, hole, RGN_DIFF);
                DeleteObject(hole);
            }
            SetWindowRgn(g.viewer.hwnd(), rg, TRUE);
        }
    }
}

// ---- drawing ----
// The rail (v3): the green O on top, then the editors in groups with small dividers between them; the last group
// (Ability, Type, Battle) dimmed as coming soon. Each shows its name in a small label while hovered (drawn last, over
// the list: draw_rail_label).
float rail_item_y(int pos) {
    float y = kTabBar + 58;
    for (int k = 0; k < pos; ++k) y += kRailOrder[k] < 0 ? 11 : 46;
    return y;
}
void draw_rail(const Layout& L) {
    const Palette& P = g.pal;
    fill(0, kTabBar, kRail, L.h - kTabBar, P.panel);
    fill(kRail - 1, kTabBar, 1, L.h - kTabBar, P.line);
    // the O: OrrePort's mark (the same picture as the window's icon), and since 2026-11-18 the way Home (snatcherLeo: "to get
    // home, you click our "O" logo ... it doesn't glow the icon itself, but around it maybe, with the same hover
    // region as the rest of those large icons"): the rail icons' 42-DIP square behind it on hover, tinted while Home
    // is shown; the mark itself is drawn unchanged
    {
        const float oy = kTabBar + 8;
        const bool on = on_home(), hov = g.hover == HIT_HOME;
        if (on) round_fill(7, oy, 42, 42, 10, P.accent_bg);
        else if (hov) round_fill(7, oy, 42, 42, 10, P.hover);
        draw_o(11, kTabBar + 12, 34);
        // the update dot (4.4, snatcherLeo: "We could add that dot on the home icon"): the hub's orange while an update waits
        // to be downloaded, the accent once it's ready to apply; ringed in the rail's colour so it reads on the O
        if (upd_attention()) {
            const D2D1_COLOR_F c = g_upd.state == PC_UPDATE_READY ? P.accent : hexc(0xE8955A);
            g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(42, oy + 8), 6, 6), brush(P.panel));
            g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(42, oy + 8), 4, 4), brush(c));
        }
        hit({7, oy, 42, 42}, HIT_HOME);
    }
    const int n = (int)(sizeof kRailOrder / sizeof kRailOrder[0]);
    for (int pos = 0; pos < n; ++pos) {
        const int ed = kRailOrder[pos];
        const float y = rail_item_y(pos);
        if (ed < 0) { fill(14, y + 4, 28, 1, P.line); continue; }
        // (on Home no editor is lit: the O is; the Pokémon icon then leads back to the last Pokémon)
        const bool on = g.editor == ed && !(ed == 0 && on_app_page()), hov = g.hover == HIT_RAIL + ed, dim = pos >= kRailDimFrom;
        if (on) round_fill(7, y, 42, 42, 10, P.accent_bg);
        else if (hov) round_fill(7, y, 42, 42, 10, P.hover);
        if (on) round_fill(0, y + 10, 3, 22, 1.5f, P.accent);
        draw_symbol(ed, 17, y + 10, 22, on ? P.accent2 : hov ? P.text : dim ? P.faint : P.mute);
        hit({7, y, 42, 42}, HIT_RAIL + ed);
    }
}
// the hovered editor's name, beside the rail ("Ability · coming soon" for the dimmed ones)
// The search box is a native control (a child window), and child windows always paint over the main window's own
// drawing, so a rail label that reaches across it was cut by the box (snatcherLeo, 2026-11-18). While a label shows, the
// box's window region leaves out the label's rectangle, the same trick as the 3D view's clipping. Windows only (window
// regions); Linux / macOS: their toolkits draw popups above child widgets anyway (likely).
void search_cut_out(const D2D1_RECT_F* label) {
    // the region is changed only when the label's rectangle changes: setting it repaints the window, and this runs
    // during that paint, so setting it every time would repaint forever
    static bool cut = false;
    static RECT last{};
    if (!g.search) return;
    if (!label) {
        if (cut) { SetWindowRgn(g.search, nullptr, TRUE); cut = false; }
        return;
    }
    RECT sr;
    GetWindowRect(g.search, &sr);
    POINT o{sr.left, sr.top};
    ScreenToClient(g.hwnd, &o);
    // the label in the search box's own pixels
    const RECT h{(LONG)std::floor(label->left * g.scale) - o.x, (LONG)std::floor(label->top * g.scale) - o.y,
                 (LONG)std::ceil(label->right * g.scale) - o.x, (LONG)std::ceil(label->bottom * g.scale) - o.y};
    const int ww = sr.right - sr.left, wh = sr.bottom - sr.top;
    if (h.right <= 0 || h.bottom <= 0 || h.left >= ww || h.top >= wh) { search_cut_out(nullptr); return; }
    if (cut && EqualRect(&h, &last)) return;
    HRGN all = CreateRectRgn(0, 0, ww, wh), hole = CreateRectRgnIndirect(&h);
    CombineRgn(all, all, hole, RGN_DIFF);
    DeleteObject(hole);
    SetWindowRgn(g.search, all, TRUE);  // the window owns the region now
    last = h;
    cut = true;
}
// What the list's search box must leave out this paint (a rail label or the search drop-down), collected while
// drawing and applied once at the end of the paint (search_cut_out compares it with the last one)
D2D1_RECT_F g_cut{};
bool g_cut_set = false;

// ---- the global search's drawing: the top field's frame, the drop-down, the palette ----
// a small magnifier (drawn, so it needs no symbol font)
void magnifier(float x, float y, float s, const D2D1_COLOR_F& c) {
    g.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x + s * 0.42f, y + s * 0.42f), s * 0.3f, s * 0.3f), brush(c), 1.4f);
    g.rt->DrawLine(D2D1::Point2F(x + s * 0.64f, y + s * 0.64f), D2D1::Point2F(x + s * 0.9f, y + s * 0.9f), brush(c), 1.6f);
}
// (HIT_GSEARCH_KEY: the top field's "Ctrl K" chip, a click opens the palette; HIT_SEARCH_CLEAR: the list search's ⓧ)
enum { HIT_SRES = 7400, HIT_SRES_END = 7499, HIT_GSEARCH = 7399, HIT_PAL_BG = 7398, HIT_GSEARCH_KEY = 7397, HIT_SEARCH_CLEAR = 7396 };
void open_palette() {
    g.palette_open = true;
    g.drop_open = false;
    SetWindowTextW(g.psearch, L"");
    run_search(L"");
    place_children();
    SetFocus(g.psearch);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
// the palette opened to choose the Moves preview's target: Pokémon only, and a pick sets the target (choose_result)
void open_target_palette() {
    g.palette_target = true;
    open_palette();
}
void close_search() {
    const bool was = g.palette_open;
    g.palette_open = false;
    g.palette_target = false;
    g.drop_open = false;
    if (was) SetFocus(g.hwnd);
    place_children();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
void click(int id);
void set_target(int species);
// opens result k: a Pokémon in this tab (or a new one), an editor; then the search closes and the top field clears
void choose_result(int k, bool new_tab) {
    if (k < 0 || k >= (int)g.sres.size()) return;
    const auto s = g.sres[(size_t)k];
    if (g.palette_target) {
        // choosing the target: that Pokémon takes the move's hit; the page stays where it is
        close_search();
        SetFocus(g.hwnd);
        if (s.kind == 0) set_target(g.all[(size_t)s.idx].index);
        return;
    }
    SetWindowTextW(g.gsearch, L"");
    close_search();
    SetFocus(g.hwnd);
    if (s.kind == 0) {
        g.editor = 0;
        select(s.idx, new_tab || g.cur_tab < 0);
    } else {
        click(HIT_RAIL + s.idx);
    }
    place_children();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
// a name with the letters the search matched in the accent colour (the folded marks found in the folded name, mapped
// back to the shown letters); DirectWrite drawing effects (Windows; Linux / macOS: attributed text runs)
void text_marked(const std::wstring& s, const std::vector<std::wstring>& marks, float x, float y, float w, float h, float size,
                 const D2D1_COLOR_F& c, const D2D1_COLOR_F& mark, DWRITE_FONT_WEIGHT weight) {
    IDWriteTextFormat* f = format(size, weight);
    if (!f) return;
    f->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextLayout* l = nullptr;
    g.dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, std::max(1.0f, w), std::max(1.0f, h), &l);
    if (!l) return;
    ID2D1SolidColorBrush* mb = nullptr;
    g.rt->CreateSolidColorBrush(mark, &mb);
    std::vector<size_t> map;
    const std::wstring folded = fold(s, &map);
    for (const auto& m : marks) {
        const size_t at = folded.find(m);
        if (at == std::wstring::npos || m.empty() || !mb) continue;
        const size_t a = map[at], b = map[at + m.size() - 1] + 1;
        l->SetDrawingEffect(mb, {(UINT32)a, (UINT32)(b - a)});
    }
    g.rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
    g.rt->DrawTextLayout(D2D1::Point2F(x, y), l, brush(c), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    g.rt->PopAxisAlignedClip();
    if (mb) mb->Release();
    l->Release();
}
// the result rows from (x, y), at most `rows`, the selected one tinted; the "searched as" line first when there is one
void draw_sres_rows(float x, float y, float w, int rows) {
    const Palette& P = g.pal;
    if (!g.snote.empty()) {
        text(g.snote, x + 14, y, w - 28, kSNote, 12, P.mute);
        y += kSNote;
    }
    if (g.sres.empty()) {
        wchar_t q[128] = L"";
        GetWindowTextW(g.palette_open ? g.psearch : g.gsearch, q, 128);
        text(tx("studio.search.none", {{"query", q}}), x + 14, y, w - 28, kSResRow, 13, P.mute);
        return;
    }
    // keep the selected row in view
    const int n = (int)g.sres.size(), first = std::clamp(g.ssel - rows + 1, 0, std::max(0, n - rows));
    for (int r = 0; r < rows && first + r < n; ++r) {
        const int k = first + r;
        const auto& s = g.sres[(size_t)k];
        const float ry = y + (float)r * kSResRow;
        if (k == g.ssel) round_fill(x + 4, ry + 2, w - 8, kSResRow - 4, 7, P.accent_bg);
        else if (g.hover == HIT_SRES + k) round_fill(x + 4, ry + 2, w - 8, kSResRow - 4, 7, P.hover);
        std::wstring name, sub, kind;
        if (s.kind == 0) {
            Entry& e = g.all[(size_t)s.idx];
            if (!e.bmp && !e.mug.empty()) e.bmp = make_bitmap(e.mug.data(), 42, 42);
            draw_mug(e.bmp, x + 12, ry + 6, 28);
            name = e.name;
            sub = tx("studio.list.number", {{"number", number3(e.s.national_dex)}});
            kind = tx("studio.search.kind_pokemon");
        } else {
            draw_symbol(s.idx, x + 16, ry + 10, 20, P.text2);
            char key[48];
            std::snprintf(key, sizeof key, "studio.editor.%s", kEditors[s.idx]);
            name = tx(key);
            kind = tx("studio.search.kind_editor");
        }
        const float nx = x + 52, nw = text_width(name, 13.5f, DWRITE_FONT_WEIGHT_MEDIUM);
        text_marked(name, s.marks, nx, ry, w - 160, kSResRow, 13.5f, P.text, P.accent2, DWRITE_FONT_WEIGHT_MEDIUM);
        float sx = nx + nw + 10;
        if (!sub.empty()) {
            TextStyle st(true);
            text(sub, sx, ry, 80, kSResRow, 12, P.mute);
            sx += text_width(sub, 12, DWRITE_FONT_WEIGHT_NORMAL) + 10;
        }
        // what matched when it wasn't the name ("Fire type", "Ability: Blaze", "Seed Pokémon")
        if (!s.via.empty()) text(L"· " + s.via, sx, ry, std::max(10.0f, x + w - 116 - sx), kSResRow, 12, P.faint);
        text(kind, x + w - 110, ry, 96, kSResRow, 11.5f, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
        hit({x + 4, ry, w - 8, kSResRow}, HIT_SRES + k);
    }
}
void draw_search(const Layout& L) {
    const Palette& P = g.pal;
    if (!g.disc) return;
    // the top field's frame (the edit control sits inside it), the magnifier and the Ctrl K hint
    const Rect f = gfield_rect(L);
    const bool focus = GetFocus() == g.gsearch;
    round_fill(f.x, f.y, f.w, f.h, 8, P.card);
    round_line(f.x, f.y, f.w, f.h, 8, focus ? P.accent : g.hover == HIT_GSEARCH ? P.line2 : P.line);
    magnifier(f.x + 9, f.y + 6, 14, P.mute);
    // the "Ctrl K" chip is a button too: a click opens the palette, as the keys do (snatcherLeo, 2026-11-22: "if users do
    // click the button "Ctrl K" they can open up the palette for searching (so just another way to open it)"). It lies
    // outside the edit control (place_children), so the click reaches the page; hovered, it lights up like a button
    const Rect key{f.x + f.w - 50, f.y + 5, 42, f.h - 10};
    const bool key_hover = g.hover == HIT_GSEARCH_KEY;
    if (key_hover) round_fill(key.x, key.y, key.w, key.h, 4, P.hover);
    text(L"Ctrl K", key.x, key.y, key.w, key.h, 10.5f, key_hover ? P.text : P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    round_line(key.x, key.y, key.w, key.h, 4, key_hover ? P.line2 : P.line);
    hit({f.x, f.y, f.w, f.h}, HIT_GSEARCH);
    hit(key, HIT_GSEARCH_KEY);  // (after the field's: the later hit spot wins, hit_at)
    if (g.drop_open && !g.palette_open) {
        const Rect d = drop_rect(L);
        round_fill(d.x, d.y, d.w, d.h, 10, P.dark ? hexc(0x1D201D) : P.card);
        round_line(d.x, d.y, d.w, d.h, 10, P.line2);
        draw_sres_rows(d.x, d.y + 6, d.w, (int)kSResDropRows);
        g_cut = D2D1::RectF(d.x, d.y, d.x + d.w, d.y + d.h);  // the list's search box leaves it out
        g_cut_set = true;
    }
    if (g.palette_open) {
        // the palette: the whole window dimmed, a card near the top with its own field and the same results; the dim
        // layer takes every click under it (outside the card it closes the palette)
        fill(0, 0, L.w, L.h, D2D1::ColorF(0, 0, 0, P.dark ? 0.5f : 0.3f));
        hit({0, 0, L.w, L.h}, HIT_PAL_BG);
        const Rect p = pal_rect(L);
        round_fill(p.x, p.y, p.w, p.h, 12, P.card);
        round_line(p.x, p.y, p.w, p.h, 12, P.line2);
        magnifier(p.x + 16, p.y + 16, 20, P.mute);
        text(L"Esc", p.x + p.w - 50, p.y + 14, 36, 24, 11, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        fill(p.x, p.y + 52, p.w, 1, P.line);
        draw_sres_rows(p.x, p.y + 58, p.w, (int)kSResPalRows);
        fill(p.x, p.y + p.h - 30, p.w, 1, P.line);
        text(tx(g.palette_target ? "studio.moves.target_pick_hint" : "studio.search.hint"), p.x + 14, p.y + p.h - 30, p.w - 28, 30,
             11.5f, P.faint);
    }
}

void draw_rail_label() {
    if (g.hover == HIT_HOME) {
        // the O's label: "Home", like the rail icons' names
        const std::wstring s = tx("studio.home.tab");
        const float w = text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 20, y = kTabBar + 15;
        g_cut = D2D1::RectF(kRail - 4, y, kRail - 4 + w, y + 28);
        g_cut_set = true;
        round_fill(kRail - 4, y, w, 28, 7, g.pal.dark ? hexc(0x0F110F) : g.pal.card);
        round_line(kRail - 4, y, w, 28, 7, g.pal.line2);
        text(s, kRail - 4, y, w, 28, 12.5f, g.pal.text, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        return;
    }
    if (g.hover < HIT_RAIL || g.hover >= HIT_RAIL + kEditorCount) return;
    const int ed = g.hover - HIT_RAIL;
    const int n = (int)(sizeof kRailOrder / sizeof kRailOrder[0]);
    int pos = 0;
    while (pos < n && kRailOrder[pos] != ed) ++pos;
    char key[48];
    std::snprintf(key, sizeof key, "studio.editor.%s", kEditors[ed]);
    const std::wstring s = pos >= kRailDimFrom || ed != 0 ? tx("studio.rail.soon", {{"editor", tx(key)}}) : tx(key);
    const float w = text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 20, y = rail_item_y(pos) + 7;
    g_cut = D2D1::RectF(kRail - 4, y, kRail - 4 + w, y + 28);
    g_cut_set = true;
    round_fill(kRail - 4, y, w, 28, 7, g.pal.dark ? hexc(0x0F110F) : g.pal.card);
    round_line(kRail - 4, y, w, 28, 7, g.pal.line2);
    text(s, kRail - 4, y, w, 28, 12.5f, g.pal.text, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void draw_list(const Layout& L) {
    const Palette& P = g.pal;
    fill(L.list.x, kTabBar, L.list.w, L.h - kTabBar, P.panel);
    fill(L.list.x + L.list.w - 1, kTabBar, 1, L.h - kTabBar, P.line);
    {
        TextStyle st(false, 0.17f);  // a small capital heading (typography pass)
        text(tx("studio.list.title"), L.list.x + 14, kTabBar + 10, L.list.w - 28, 26, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    }
    // the search box's frame (the box itself is Windows' edit control)
    round_fill(L.list.x + 10, kTabBar + 46, L.list.w - 20, 34, 8, P.bg);
    round_line(L.list.x + 10, kTabBar + 46, L.list.w - 20, 34, 8, GetFocus() == g.search ? P.accent : P.line2);
    // the clear button (snatcherLeo, 2026-11-22: "a circle with an x at the end of the search field, to clear out what we
    // searched and get back to the list"; this field only, the top search has none): shown while there is text, at the
    // frame's right end; the edit is narrowed by kClearRoom then (place_children), so a query longer than the box
    // scrolls inside the edit and never runs under it. Drawn (a filled circle, an x cut out in the page colour), so it
    // needs no symbol font
    if (list_search_has_text()) {
        const float cx = L.list.x + L.list.w - 10 - 16, cy = kTabBar + 46 + 17;
        const bool hov = g.hover == HIT_SEARCH_CLEAR;
        // hovered: brighter, with the hand pointer (does_something), as every button here
        g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), kClearR, kClearR), brush(hov ? P.text : P.faint));
        const float a = kClearR * 0.4f;
        g.rt->DrawLine(D2D1::Point2F(cx - a, cy - a), D2D1::Point2F(cx + a, cy + a), brush(P.bg), 1.6f);
        g.rt->DrawLine(D2D1::Point2F(cx - a, cy + a), D2D1::Point2F(cx + a, cy - a), brush(P.bg), 1.6f);
        hit({cx - 12, cy - 12, 24, 24}, HIT_SEARCH_CLEAR);
    }
    const float top = kListTop - 8, view = L.h - top;
    g.rt->PushAxisAlignedClip(D2D1::RectF(L.list.x, top, L.list.x + L.list.w, L.h), D2D1_ANTIALIAS_MODE_ALIASED);
    const int first = std::max(0, (int)(g.scroll / kRow));
    // only the rows on screen are drawn (fast at any list length: the plan's tip 10)
    for (int k = first; k < (int)g.shown.size(); ++k) {
        const float y = top + k * kRow - g.scroll;
        if (y > L.h) break;
        Entry& e = g.all[g.shown[k]];
        const bool on = g.shown[k] == g.selected, hov = g.hover == HIT_ROW + k;
        if (on) round_fill(L.list.x + 6, y + 2, L.list.w - 13, kRow - 4, 8, P.accent_bg);
        else if (hov) round_fill(L.list.x + 6, y + 2, L.list.w - 13, kRow - 4, 8, P.hover);
        if (on) fill(L.list.x + 6, y + 6, 2, kRow - 12, P.accent);
        if (!e.bmp && !e.mug.empty()) e.bmp = make_bitmap(e.mug.data(), 42, 42);
        draw_mug(e.bmp, L.list.x + 16, y + 6, 38);
        text(e.name, L.list.x + 64, y + 7, L.list.w - 76, 20, 13.5f, P.text, on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
        TextStyle st(true);  // same-width digits
        text(tx("studio.list.number", {{"number", number3(e.s.national_dex)}}), L.list.x + 64, y + 26, L.list.w - 76, 18, 11.5f, P.mute);
        hit({L.list.x, std::max(top, y), L.list.w, kRow}, HIT_ROW + k);
    }
    if (g.shown.empty() && !g.all.empty()) {
        wchar_t q[128] = L"";
        GetWindowTextW(g.search, q, 128);
        text(tx("studio.list.empty", {{"text", q}}), L.list.x + 16, top + 8, L.list.w - 32, 60, 13, P.mute, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_LEADING, true);
    }
    // a thin scroll indicator
    const float total = g.shown.size() * kRow;
    if (total > view) {
        const float bar = std::max(30.0f, view * view / total), y = top + (view - bar) * (g.scroll / (total - view));
        round_fill(L.list.x + L.list.w - 6, y, 3, bar, 1.5f, alpha(P.mute, 0.45f));
    }
    g.rt->PopAxisAlignedClip();
}

// ---- the Pokémon page (v2) ----
void button(const std::wstring& s, float x_right, float y, int id, float& out_x);

// Type colours (v3: the sandbox's dot colours, the familiar ones from the series' own type colours; ??? is Studio's)
D2D1_COLOR_F type_dot(int t) {
    static const uint32_t k[18] = {0xA8A77A, 0xC22E28, 0xA98FF3, 0xA33EA1, 0xE2BF65, 0xB6A136, 0xA6B91A, 0x735797, 0xB7B7CE,
                                   0x68A090, 0xEE8130, 0x6390F0, 0x7AC74C, 0xF7D02C, 0xF95587, 0x96D9D6, 0x6F35FC, 0x705746};
    return hexc(k[t >= 0 && t < 18 ? t : 0]);
}

std::wstring type_name(int t) {
    char key[32];
    std::snprintf(key, sizeof key, "studio.type.%d", t);
    return tx(key);
}

// A type chip (v3): a pill with the type's coloured dot and its name, clickable (the Type editor, coming soon) and
// explained by a hover card. `id` lets the header's chips and the matchup chips have their own hover spots. x moves
// past it.
float chip_width(int t, float size) { return text_width(type_name(t), size, DWRITE_FONT_WEIGHT_MEDIUM) + (size > 12.5f ? 34 : 28); }
void type_chip(int t, float& x, float y, float size = 13.0f, float h = 26, int id = -1) {
    const Palette& P = g.pal;
    if (id < 0) id = HIT_TYPE + t;
    const std::wstring s = type_name(t);
    const float w = chip_width(t, size), pad = size > 12.5f ? 12 : 9;
    const bool hov = g.hover == id;
    round_fill(x, y, w, h, h / 2, size > 12.5f ? P.card : P.card2);
    round_line(x, y, w, h, h / 2, hov ? P.accent_line : P.line2);
    const float d = size > 12.5f ? 9 : 8;
    g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + pad + d / 2, y + h / 2), d / 2, d / 2), brush(type_dot(t)));
    text(s, x + pad + d + 6, y, w - pad - d - 6, h, size, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    hit({x, y, w, h}, id);
    x += w + 6;
}

// A card (v3): 12-DIP corners, the card fill, a thin line; its label in small capitals on the left, a note on the right
void card(const Rect& r, const std::wstring& title, const std::wstring& right_text = L"") {
    const Palette& P = g.pal;
    round_fill(r.x, r.y, r.w, r.h, 12, P.card);
    round_line(r.x, r.y, r.w, r.h, 12, P.line);
    {
        // the small capital heading: 11 px semibold, 0.17 em apart, muted (mockup 23's .lbl)
        TextStyle st(false, 0.17f);
        text(title, r.x + 16, r.y + 12, r.w - 32, 20, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    }
    if (!right_text.empty())
        text(right_text, r.x + 16, r.y + 12, r.w - 32, 20, 11.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
}

// a segmented control (Idle | Standing pose, Normal | Shiny): the options in a framed strip, the active one tinted;
// returns its width. Draws right-aligned at x_right.
float segmented(float x_right, float y, const std::vector<std::pair<std::wstring, int>>& opts, int active_id) {
    const Palette& P = g.pal;
    float w = 4;
    for (auto& [s, id] : opts) w += text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 18 + 2;
    float x = x_right - w;
    round_fill(x, y, w, 30, 8, P.bg);
    round_line(x, y, w, 30, 8, P.line2);
    x += 3;
    for (auto& [s, id] : opts) {
        const float bw = text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 18;
        const bool on = id == active_id;
        if (on) { round_fill(x, y + 3, bw, 24, 6, P.accent_bg); round_line(x, y + 3, bw, 24, 6, P.accent_line); }
        else if (g.hover == id) round_fill(x, y + 3, bw, 24, 6, P.hover);
        text(s, x, y + 3, bw, 24, 12.5f, on ? P.accent2 : P.text2, on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({x, y + 3, bw, 24}, id);
        x += bw + 2;
    }
    return w;
}

// the species' ability names (title case, from the disc's ability table)
std::wstring ability_name(int a) {
    uint32_t nm = 0, doc = 0;
    if (a <= 0 || !orre_ability_messages(g.disc, a, &nm, &doc)) return L"";
    return title_case(widen(orre_text(g.disc, nm)));
}

// The evolution family of a species: the first member (no one evolves into it), then every member reached through
// the species' evolution records (+0x9C, 5 x {kind, parameter, target}; kind 4 = level, as in Gen 3: likely), each with
// the method that leads to it. Branches (Eevee) list every target.
struct EvoStep { int index; int kind; int param; int parent = -1; int depth = 0; };
std::vector<EvoStep> evolution_family(int index) {
    auto entry = [](int sp) -> const Entry* {
        auto it = g.by_index.find(sp);
        return it == g.by_index.end() ? nullptr : &g.all[it->second];
    };
    int root = index;
    for (int guard = 0; guard < 8; ++guard) {  // walk back to whoever evolves into nobody's target
        int prev = -1;
        for (const Entry& e : g.all)
            for (const auto& ev : e.s.evolutions)
                if (ev.kind && ev.target == root) prev = e.index;
        if (prev < 0) break;
        root = prev;
    }
    std::vector<EvoStep> out = {{root, 0, 0}};
    for (size_t i = 0; i < out.size() && out.size() < 16; ++i)
        if (const Entry* e = entry(out[i].index))
            for (const auto& ev : e->s.evolutions)
                if (ev.kind && ev.target && entry(ev.target) &&
                    std::none_of(out.begin(), out.end(), [&](const EvoStep& s) { return s.index == ev.target; }))
                    out.push_back({ev.target, ev.kind, ev.param, (int)i, out[i].depth + 1});
    return out;
}

// how an evolution happens; kinds 6 and 7 name their item (the record's parameter is an item id)
std::wstring evo_method(int kind, int param) {
    char key[40];
    std::snprintf(key, sizeof key, "studio.evo.kind.%d", kind);
    std::wstring item;
    if (kind == 6 || kind == 7) item = title_case(widen(orre_text(g.disc, orre_item_name_msg(g.disc, param))));
    return tx(key, {{"level", std::to_wstring(param)}, {"item", item}});
}

// matchups from the type chart (tenths x tenths / 10): 40, 20, 5, 2 (x1/4), 0; the "???" type (9) left out
std::vector<int> matchups(const OrreSpecies& s, int want) {
    std::vector<int> out;
    for (int t = 0; t < 18; ++t) {
        if (t == 9) continue;
        int m = orre_type_effect(g.disc, t, s.types[0]);
        if (s.types[1] != s.types[0]) m = m * orre_type_effect(g.disc, t, s.types[1]) / 10;
        if (m == want) out.push_back(t);
    }
    return out;
}

const OrreSound* cry_for(int species);                                           // (the cries, further down)
void media_symbol(int kind, float cx, float cy, float s, const D2D1_COLOR_F& c);  // (the Moves tab's, further down)
void draw_header(const Layout& L, const Entry& e) {
    const Palette& P = g.pal;
    const Rect& C = L.page;
    const float x0 = C.x + kPad, right = C.x + C.w - kPad;
    // the glow behind the hero, tinted by the first type (the sandbox's radial gradient: 20% of the type's colour
    // fading out over about 900 x 220 DIPs, centred above the left part of the header)
    {
        D2D1_COLOR_F tint = type_dot(e.s.types[0]);
        // (much fainter since the typography pass, 2026-11-18: "the type-tinted glow behind the header much fainter")
        D2D1_GRADIENT_STOP stops[2] = {{0.0f, alpha(tint, P.dark ? 0.08f : 0.06f)}, {1.0f, alpha(tint, 0.0f)}};
        ID2D1GradientStopCollection* sc = nullptr;
        ID2D1RadialGradientBrush* rb = nullptr;
        if (SUCCEEDED(g.rt->CreateGradientStopCollection(stops, 2, &sc)) &&
            SUCCEEDED(g.rt->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(D2D1::Point2F(C.x + C.w * 0.18f, L.hero_y - kHeroTop - 40), D2D1::Point2F(0, 0), 900, 260),
                sc, &rb)))
            g.rt->FillRectangle(D2D1::RectF(C.x, std::max(C.y, L.hero_y - kHeroTop), C.x + C.w, L.hero_y - kHeroTop + 240), rb);
        if (rb) rb->Release();
        if (sc) sc->Release();
    }
    // the mug shot (the current form's normal one, whatever Normal / Shiny shows: the header names the Pokémon; snatcherLeo,
    // 2026-11-22, after trying a shiny header: "I would rather not do that"), the frame hugging the picture: about 76
    // DIPs, an exact multiple of its 42 pixels (84 DIPs at 100 % scaling)
    const float hy = L.hero_y, ms = mug_size(kHeroH);
    ID2D1Bitmap* mug = !g.mugs.empty() ? g.mugs[0].bmp : e.bmp;
    draw_mug_framed(mug, x0, hy + (kHeroH - ms) / 2, ms, 12, P.line);
    // name and type chips (a one-type Pokémon stores its type twice): the title 30 px bold in Inter's display cut,
    // the chips' words on the name's baseline
    const float nx = x0 + ms + 18, name_base = hy + 34;
    text_base(e.name, nx, name_base, right - nx, 30, P.text, DWRITE_FONT_WEIGHT_BOLD);
    float x = nx + text_width(e.name, 30, DWRITE_FONT_WEIGHT_BOLD) + 14;
    const float chip_y = box_top_for_base(name_base, 26, 13, DWRITE_FONT_WEIGHT_MEDIUM);
    type_chip(e.s.types[0], x, chip_y);
    if (e.s.types[1] != e.s.types[0]) type_chip(e.s.types[1], x, chip_y);
    // category · [No. 133 National Dex] · height · weight (snatcherLeo: "We also don't need No. 133 in this game"); the US
    // disc stores feet and inches and tenths of a pound
    wchar_t inches[8], pounds[24];
    swprintf(inches, 8, L"%02d", e.s.height % 100);
    swprintf(pounds, 24, L"%d.%d", e.s.weight / 10, e.s.weight % 10);
    // the information line: every piece on one baseline (the category, the dex box's "No. 133" and "National Dex", the
    // height and weight), the number in same-width digits
    float ix = nx;
    const float iy = hy + 50, ib = iy + 17;  // ib: the line's baseline
    const std::wstring cat = tx("studio.page.category", {{"category", e.category}});
    text_base(cat, ix, ib, 300, 14, P.text2);
    ix += text_width(cat, 14, DWRITE_FONT_WEIGHT_NORMAL) + 10;
    text_base(L"·", ix, ib, 10, 14, P.faint);
    ix += 16;
    const std::wstring dexn = tx("studio.page.dex", {{"national", number3(e.s.national_dex)}}), dexl = tx("studio.page.dex_label");
    float dnw;
    {
        TextStyle st(true);
        dnw = text_width(dexn, 13, DWRITE_FONT_WEIGHT_BOLD);
    }
    const float dw = dnw + text_width(dexl, 11.5f, DWRITE_FONT_WEIGHT_NORMAL) + 26;
    round_line(ix, iy, dw, 24, 7, P.line2);
    {
        TextStyle st(true);
        text_base(dexn, ix + 9, ib, dw, 13, P.text, DWRITE_FONT_WEIGHT_BOLD);
    }
    text_base(dexl, ix + 9 + dnw + 6, ib, dw, 11.5f, P.mute);
    ix += dw + 10;
    text_base(L"·", ix, ib, 10, 14, P.faint);
    ix += 16;
    // Create a mod (faded: it arrives with editing). *Updated 2026-11-19:* the Copy / share button is gone for now
    // (snatcherLeo: "let's just get rid of it (for now) in Studio"); its click handler and words (studio.page.copy,
    // studio.copy.text) stay, unused, for when it comes back. On a narrow page the button moves up to the name's line
    // if the type chips leave room, else it hides and the information line keeps the whole width.
    {
        const std::wstring cm = tx("studio.page.create_mod");
        const float w1 = text_width(cm, 13, DWRITE_FONT_WEIGHT_NORMAL) + 26;
        const std::wstring meas = tx("studio.page.measure", {{"height", tx("studio.page.height", {{"feet", std::to_wstring(e.s.height / 100)}, {"inches", inches}})},
                                                             {"weight", tx("studio.page.weight", {{"pounds", pounds}})}});
        const float mw = text_width(meas, 14, DWRITE_FONT_WEIGHT_NORMAL) + 4;
        const bool narrow = ix + mw > right - w1 - 12;
        const float name_room = right - (x + 8);
        const bool up = narrow && w1 <= name_room;
        const bool show_mod = !narrow || up;
        const float info_right = !narrow ? right - w1 - 12 : right;
        text_base(meas, ix, ib, std::max(10.0f, info_right - ix), 14, P.text2);
        // the cries' play buttons (*changed 2026-11-22*, snatcherLeo on the Cries card under the abilities: "I don't like
        // having the cries card there, the card layout we had earlier was perfect"; option A of mockup 27): "· ▶ Cry
        // ▶ Fainting cry" after the height and weight, framed like the dex box. The header stays on every tab, so the
        // cries play from anywhere on the page; on a line too short for them they go up beside the type chips
        {
            const std::wstring c1 = tx("studio.cries.cry"), c2 = tx("studio.cries.faint");
            const float b1 = text_width(c1, 13, DWRITE_FONT_WEIGHT_NORMAL) + 40, b2 = text_width(c2, 13, DWRITE_FONT_WEIGHT_NORMAL) + 40;
            const float need = 16 + b1 + 8 + b2;
            float bx = ix + text_width(meas, 14, DWRITE_FONT_WEIGHT_NORMAL) + 10, byy = iy;
            bool dot = true;
            if (bx + need > info_right) {  // (no room on the information line: up beside the type chips)
                bx = x + 4;
                byy = chip_y;
                dot = false;
                if (bx + need - 16 > right - (up ? w1 + 12 : 0)) bx = -1;  // (no room there either: the Sound tab has them)
            }
            if (bx >= 0 && cry_for(e.index)) {
                if (dot) { text_base(L"·", bx, ib, 10, 14, P.faint); bx += 16; }
                auto btn = [&](const std::wstring& label, float bw, int id) {
                    const bool hov = g.hover == id;
                    if (hov) round_fill(bx, byy, bw, 24, 7, P.hover);
                    round_line(bx, byy, bw, 24, 7, hov ? P.accent_line : P.line2);
                    media_symbol(0, bx + 15, byy + 12, 8, hov ? P.accent2 : P.text2);
                    text(label, bx + 27, byy, bw - 30, 24, 13, hov ? P.text : P.text2);
                    hit({bx, byy, bw, 24}, id);
                    bx += bw + 8;
                };
                btn(c1, b1, HIT_CRY);
                btn(c2, b2, HIT_CRY_FAINT);
            }
        }
        const float by = up ? hy - 4 : hy + 23;
        if (show_mod) {
            round_line(right - w1, by, w1, 30, 7, alpha(P.line2, 0.6f));
            text(cm, right - w1, by, w1, 30, 13, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            hit({right - w1, by, w1, 30}, HIT_CREATE_MOD);
        }
    }
    // the era bar: a segmented control; the later generations and the newer games wait for the outside data
    // (greyed, explained on hover); Colosseum is this disc
    float ex = x0;
    const float ey = L.era_y;
    {
        TextStyle st(false, 0.17f);  // a small capital heading (typography pass)
        text(tx("studio.era.label"), ex, ey, 80, kEraH, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        ex += text_width(tx("studio.era.label"), 11, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 18;
    }
    {
        // "Gen" is said once, in the first button; the later generations are bare numbers (snatcherLeo, 2026-11-19: "we can
        // only say Gen once at the very beginning I'd say, just to keep this better to read"; a small "Gen" label in
        // front of 4 was dropped)
        // *Updated 2026-11-19 (snatcherLeo): "when you click on a new Generation, it'll then say "Gen" in front of it ... when I
        // click "4" (when we make it allowed) it'll show its selected and say "Gen 4" and have that vertical line ... to
        // better help select it"*: the picked generation (g.era: 0 this disc, 1.. Gen 4..9) reads "Gen N" and has a
        // divider on each side. Picking isn't open yet (the later generations wait for the outside data); --era N shows
        // it in screenshot tests.
        struct Opt { std::wstring s; bool on; bool sep_before; };
        std::vector<Opt> opts;
        auto bw_of = [](const Opt& o) {
            return std::max(26.0f, text_width(o.s, 12.5f, o.on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL) + 16);
        };
        auto width_of = [&](const std::vector<Opt>& v) {
            float w = 4;
            for (auto& o : v) w += bw_of(o) + 2 + (o.sep_before ? 5 : 0);
            return w;
        };
        // first, this disc: Gen 3 and Colosseum are one button (kEraFirstGen)
        opts.push_back({tx("studio.era.gen3_colosseum"), g.era == 0, false});
        for (int gen = kEraFirstGen; gen <= 9; ++gen) {
            const int k = gen - kEraFirstGen + 1;
            const bool on = g.era == k;
            opts.push_back({on ? tx("studio.era.gen", {{"n", std::to_wstring(gen)}}) : std::to_wstring(gen), on,
                            gen == kEraFirstGen || on || g.era == k - 1});
        }
        opts.push_back({tx("studio.era.letsgo"), false, true});
        opts.push_back({tx("studio.era.legends_arceus"), false, false});
        opts.push_back({tx("studio.era.legends_za"), false, false});
        // a narrow page leaves out the three newer games (they wait for the outside data anyway); their hover ids keep
        // their numbers, so they are only drawn while there is room
        const bool games = ex + width_of(opts) <= right;
        if (!games) opts.erase(opts.begin() + 1 + kEraGens, opts.begin() + 1 + kEraGens + 3);
        const float w = width_of(opts);
        round_fill(ex, ey, w, kEraH, 8, P.bg);
        round_line(ex, ey, w, kEraH, 8, P.line2);
        float sx = ex + 2;
        for (size_t k = 0; k < opts.size(); ++k) {
            const Opt& o = opts[k];
            if (o.sep_before) { fill(sx + 1, ey + 6, 1, kEraH - 12, P.line2); sx += 5; }
            const float bw = bw_of(o);
            if (o.on) { round_fill(sx, ey + 3, bw, kEraH - 6, 6, P.accent_bg); round_line(sx, ey + 3, bw, kEraH - 6, 6, P.accent_line); }
            text(o.s, sx, ey + 3, bw, kEraH - 6, 12.5f, o.on ? P.accent2 : alpha(P.faint, 0.85f),
                 o.on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            if (!o.on) hit({sx, ey + 3, bw, kEraH - 6}, HIT_ERA + (int)k);
            sx += bw + 2;
        }
        text(tx("studio.era.note"), ex + w + 14, ey, right - (ex + w + 14), kEraH, 12, P.mute);
    }
    // the tabs
    float tx0 = x0;
    const float ty = L.tabs_y;
    // (Styles second: see kTabs. Animations before Moves since 2026-11-22, snatcherLeo: "we then move up the "Animations"
    // sub-tab to be before "Moves" sub-tab")
    static const int kTabOrder[] = {kTabOverview, kTabLooks, kTabAnimations, kTabMoves, kTabSound, 5};
    for (int i : kTabOrder) {
        const std::wstring s = tx(kTabs[i]);
        const bool on = g.tab == i;
        const DWRITE_FONT_WEIGHT wt = on ? DWRITE_FONT_WEIGHT_MEDIUM : DWRITE_FONT_WEIGHT_NORMAL;
        const float w = text_width(s, 14, wt);
        text(s, tx0, ty, w + 2, kTabsH - 2, 14, on || g.hover == HIT_TAB + i ? P.text : P.mute, wt);
        if (on) fill(tx0, ty + kTabsH - 3, w, 2, P.accent);
        hit({tx0 - 6, ty, w + 12, kTabsH}, HIT_TAB + i);
        tx0 += w + 24;
    }
    fill(x0, ty + kTabsH - 1, C.w - 2 * kPad, 1, P.line);
}

// ---- the Overview (v3) ----
const char* const kStatKeys[] = {"studio.statname.hp", "studio.statname.attack", "studio.statname.defense",
                                 "studio.statname.sp_atk", "studio.statname.sp_def", "studio.statname.speed"};

// the battle model card: Idle | Standing pose on the header, the 3D child window between, Normal | Shiny in the footer
// Under the 3D view, the same soft glow its own backdrop draws (the shader's bfs in viewer.cpp: centre at 50 % / 70 %
// of the stage, full stage_lo at 0.7 x the width across and 0.5 x the width up and down), or Plain stage's flat grey.
// Before the view's first frame (it stays cut to nothing until then) this is what shows, so the model just appears on
// the same backdrop at launch, with no change of shade (snatcherLeo, 2026-11-19: "make sure when we open the app, it doesn't
// look... strange"). Direct2D radial gradient, sRGB interpolation like the shader's; Linux / macOS: the toolkit's
// radial gradient.
void stage_backdrop(const Rect& S, bool plain) {
    const Palette& P = g.pal;
    if (plain) { fill(S.x, S.y, S.w, S.h, D2D1::ColorF(0x7F7F7F)); return; }
    ID2D1GradientStopCollection* stops = nullptr;
    const D2D1_GRADIENT_STOP st2[2] = {{0.0f, P.stage_hi}, {1.0f, P.stage_lo}};
    ID2D1RadialGradientBrush* rb = nullptr;
    if (SUCCEEDED(g.rt->CreateGradientStopCollection(st2, 2, &stops)) &&
        SUCCEEDED(g.rt->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(D2D1::Point2F(S.x + S.w * 0.5f, S.y + S.h * 0.7f), D2D1::Point2F(0, 0), S.w * 0.7f,
                                                S.w * 0.5f),
            stops, &rb)))
        g.rt->FillRectangle(D2D1::RectF(S.x, S.y, S.x + S.w, S.y + S.h), rb);
    else
        fill(S.x, S.y, S.w, S.h, P.stage_lo);
    if (rb) rb->Release();
    if (stops) stops->Release();
}

// The Particles switch (2026-11-19, particles step 1): the Pokémon's own effects on or off, one setting for every
// window and tab (App::particles_on, saved in studio.ini). Drawn right-aligned at x_right; returns its width.
// a switch with its label, right-aligned at x_right (Particles, Game lighting); returns its width
float toggle_switch(float x_right, float y, const std::wstring& label, bool on, int id) {
    const Palette& P = g.pal;
    const float w = text_width(label, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 46;
    const float x = x_right - w;
    const bool hov = g.hover == id;
    round_fill(x + 4, y + 7, 30, 16, 8, on ? P.accent : hov ? P.line2 : alpha(P.line2, 0.8f));
    g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(on ? x + 26 : x + 12, y + 15), 5.5f, 5.5f), brush(on ? P.on_accent : P.text2));
    text(label, x + 40, y, w - 40, 30, 12.5f, hov ? P.text : P.text2);
    hit({x, y, w, 30}, id);
    return w;
}
float toggle_width(const char* key) { return text_width(tx(key), 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 46; }
float particle_switch(float x_right, float y) {
    return toggle_switch(x_right, y, tx("studio.moves.particles"), g.particles_on, HIT_PARTICLES);
}
// Game lighting (release step 2, 2026-11-19; name, place and default C's, same day): the game's own arena lights in
// the 3D view, or Studio's even light to look at a model's own colours and textures. One setting for every window,
// kept in studio.ini like Particles
float lighting_switch(float x_right, float y) {
    return toggle_switch(x_right, y, tx("studio.view.lighting"), g.game_light, HIT_LIGHTING);
}
// Battle camera (release step 4): the Moves tab only, left of Game lighting when there is room
float battlecam_switch(float x_right, float y) {
    return toggle_switch(x_right, y, tx("studio.view.battle_camera"), g.battle_cam, HIT_BATTLECAM);
}
// the list's entry for a species number (-1: none)
int entry_of_species(int species) {
    for (size_t i = 0; i < g.all.size(); ++i)
        if (g.all[i].index == species) return (int)i;
    return -1;
}
// the target's name as the chip and the menu show it: the box, or the Pokémon's name
std::wstring target_name(int species) {
    const int e = species >= 0 ? entry_of_species(species) : -1;
    return e >= 0 ? g.all[(size_t)e].name : tx("studio.moves.target_box");
}
// The Moves preview's target (snatcherLeo, 2026-11-19; App::target_species): "Target" in the muted colour, then the box or
// the chosen Pokémon's name and a "▾", at (x, y) on the switches row, at most max_w wide; a click opens its menu.
// Returns its width
float target_chip_width(float max_w) {
    const float lw = text_width(tx("studio.moves.target"), 12.5f, DWRITE_FONT_WEIGHT_NORMAL);
    const float vw = std::min(text_width(target_name(g.target_species), 12.5f, DWRITE_FONT_WEIGHT_MEDIUM), std::max(20.0f, max_w - lw - 40));
    return 10 + lw + 8 + vw + 22;
}
float target_chip(float x, float y, float max_w) {
    const Palette& P = g.pal;
    const bool hov = g.hover == HIT_TARGET;
    const std::wstring label = tx("studio.moves.target"), value = target_name(g.target_species);
    const float lw = text_width(label, 12.5f, DWRITE_FONT_WEIGHT_NORMAL);
    const float w = target_chip_width(max_w), vw = w - (10 + lw + 8 + 22);
    round_fill(x, y + 3, w, 24, 6, hov ? P.hover : alpha(P.line, 0.5f));
    text(label, x + 10, y, lw + 2, 30, 12.5f, P.mute);
    text(value, x + 10 + lw + 8, y, vw, 30, 12.5f, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    text(L"▾", x + w - 18, y, 12, 30, 11, P.text2);
    hit({x, y, w, 30}, HIT_TARGET);
    return w;
}
// sets the target in every window (one choice, as the view switches) and remembers it
void set_target(int species) {
    if (species >= 0) {
        g.target_recent.erase(std::remove(g.target_recent.begin(), g.target_recent.end(), species), g.target_recent.end());
        g.target_recent.insert(g.target_recent.begin(), species);
        if (g.target_recent.size() > 5) g.target_recent.resize(5);
    }
    const std::vector<int> recent = g.target_recent;
    const HWND back = g.hwnd;
    for (size_t k = 0; k < g_windows.size(); ++k) {
        activate(g_windows[k].hwnd);
        g.target_species = species;
        g.target_recent = recent;
        sync_preview();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }
    activate(back);
    save_prefs();
    logf(Level::Info, "moves", "target: %s", species >= 0 ? narrow(target_name(species)).c_str() : "the box");
}

float rate_button_width();          // (the preview rate's button, further down: the Moves tab's)
float rate_button(float x, float y);
// The view's reset arrow, drawn (not a font glyph: every language's font draws it the same): a circle open at the top
// right, its arrowhead there. Direct2D path geometry (Windows); Linux / macOS: the toolkit's paths
void draw_reset_icon(float cx, float cy, float r, const D2D1_COLOR_F& c) {
    ID2D1PathGeometry* pg = nullptr;
    if (FAILED(g.d2d->CreatePathGeometry(&pg))) return;
    ID2D1GeometrySink* sk = nullptr;
    // *Redrawn 2026-11-22* (snatcherLeo: the first one "looks confusing", it read as a power button): the common "turn back"
    // arrow, as icon sets draw rotate-left: an arc round three quarters of the circle, open at the top left, ending in
    // a solid arrowhead that points back the way it came (counter-clockwise)
    const float a_end = -2.36f;               // (radians: the arrow's tip, up and to the left)
    const float a_start = a_end + 4.71f;      // (three quarters round, clockwise from the tip back to the start)
    const D2D1_POINT_2F tip{cx + r * std::cos(a_end), cy + r * std::sin(a_end)};
    if (SUCCEEDED(pg->Open(&sk))) {
        sk->BeginFigure(D2D1::Point2F(cx + r * std::cos(a_start), cy + r * std::sin(a_start)), D2D1_FIGURE_BEGIN_HOLLOW);
        sk->AddArc(D2D1::ArcSegment(tip, D2D1::SizeF(r, r), 0, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
        sk->EndFigure(D2D1_FIGURE_END_OPEN);
        sk->Close();
        sk->Release();
    }
    g.rt->DrawGeometry(pg, brush(c), 1.7f);
    pg->Release();
    // the solid arrowhead at the tip, pointing along the arc's direction there (counter-clockwise: the tangent turned)
    const float tx = std::sin(a_end), ty = -std::cos(a_end);  // (the counter-clockwise tangent at the tip)
    const float nx = std::cos(a_end), ny = std::sin(a_end);   // (outward from the centre)
    const float L = r * 0.75f, Wd = r * 0.55f;
    ID2D1PathGeometry* hg = nullptr;
    if (SUCCEEDED(g.d2d->CreatePathGeometry(&hg))) {
        ID2D1GeometrySink* hs = nullptr;
        if (SUCCEEDED(hg->Open(&hs))) {
            hs->BeginFigure(D2D1::Point2F(tip.x + tx * L * 0.6f, tip.y + ty * L * 0.6f), D2D1_FIGURE_BEGIN_FILLED);
            hs->AddLine(D2D1::Point2F(tip.x - tx * L * 0.4f + nx * Wd, tip.y - ty * L * 0.4f + ny * Wd));
            hs->AddLine(D2D1::Point2F(tip.x - tx * L * 0.4f - nx * Wd, tip.y - ty * L * 0.4f - ny * Wd));
            hs->EndFigure(D2D1_FIGURE_END_CLOSED);
            hs->Close();
            hs->Release();
        }
        g.rt->FillGeometry(hg, brush(c));
        hg->Release();
    }
}
// Reset view (snatcherLeo, 2026-11-22: "In case a user rotates too far by accident, and then wants to reset, we should have a
// "reset" button"; placed in the header, his pick C): a square button with the arrow, dimmed while the view is already
// at its start (the double-click on the model does the same)
// (with_word: the icon and "Reset", as snatcherLeo asked for something "very easy to understand", where the header has room;
// the icon alone where it doesn't)
float reset_button_width(bool with_word) {
    return with_word ? 30 + text_width(tx("studio.view.reset_short"), 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 10 : 30;
}
float reset_button(float x, float y, bool with_word) {
    const Palette& P = g.pal;
    const bool moved = g.viewer.view_moved(), hov = g.hover == HIT_RESET && moved;
    const float w = reset_button_width(with_word);
    const D2D1_COLOR_F c = moved ? (hov ? P.text : P.text2) : alpha(P.faint, 0.6f);
    round_fill(x, y, w, 30, 7, hov ? P.hover : P.card);
    round_line(x, y, w, 30, 7, P.line2);
    draw_reset_icon(x + 15, y + 15, 6.5f, c);
    if (with_word) text(tx("studio.view.reset_short"), x + 27, y, w - 30, 30, 12.5f, c, DWRITE_FONT_WEIGHT_NORMAL);
    hit({x, y, w, 30}, HIT_RESET);
    return w;
}

float segmented_width(const std::vector<std::pair<std::wstring, int>>& opts);  // (further down, beside segmented)
// the Overview's stage button: as wide as its widest choice, so picking one never moves the controls beside it
const char* const kStageKeys[] = {"studio.moves.view_studio", "studio.moves.view_plain", "studio.moves.view_arena"};
float ov_stage_width() {
    float w = 0;
    for (const char* k : kStageKeys) w = std::max(w, text_width(tx(k) + L"  ▾", 12.5f, DWRITE_FONT_WEIGHT_NORMAL));
    return w + 24;
}
// Lays a band's controls right to left (items[0] at the right edge), each on the first row with room for it; row 0
// keeps `reserve0` free at its left (the title, the link). Returns the number of rows. A control wider than a whole row
// still gets a row of its own.
int flow_band(const std::vector<BandItem*>& items, float x_left, float x_right, float y0, float reserve0, float gap) {
    int row = 0;
    float cur = x_right;
    bool row_used = false;
    for (BandItem* it : items) {
        if (!it->on) continue;
        const float left = x_left + (row == 0 ? reserve0 : 0);
        if (row_used && cur - it->w < left) { ++row; cur = x_right; row_used = false; }
        else if (!row_used && row == 0 && cur - it->w < left) { ++row; cur = x_right; }  // (not even one fits beside the title)
        it->x = cur - it->w;
        it->y = y0 + row * 40.0f;
        cur = it->x - gap;
        row_used = true;
    }
    return row + 1;
}
ModelBands model_bands(const Rect& M) {
    ModelBands b;
    const bool view = g.viewer.ok();
    // the header: Idle / Standing pose, the preview rate, Reset (snatcherLeo's pick 2026-11-22: "how you look" up here)
    b.idle = {segmented_width({{tx("studio.viewer.motion_idle"), HIT_IDLE}, {tx("studio.viewer.motion_standing"), HIT_STANDING}}), 0, 0, true};
    b.rate = {rate_button_width(), 0, 0, view};
    float title_w = 0;
    {
        TextStyle st(false, 0.17f);
        title_w = text_width(tx("studio.stage.title"), 11, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    }
    // Reset with its word when the whole header still fits on one row with it; the arrow alone otherwise (the title is
    // never left out for the word: snatcherLeo, 2026-11-22)
    int rows = 1;
    for (int pass = 0; pass < 2; ++pass) {
        b.reset_word = pass == 0;
        b.reset = {reset_button_width(b.reset_word), 0, 0, view};
        rows = flow_band({&b.idle, &b.rate, &b.reset}, M.x + 16, M.x + M.w - 12, M.y + 9, title_w + 16, 8);
        if (rows == 1) break;
    }
    b.head_h = 48 + (rows - 1) * 40.0f;
    // the footer: Normal / Shiny, the stage, Game lighting ("what you look at"), then Particles; the link at the left.
    // Particles is always there, greyed for the models without effects of their own (all but 12), so the rows, and so
    // the 3D view's size, are the same for every Pokémon (the fixed card height, 2026-11-22: the card shouldn't jump
    // between picks); before, it showed only for those 12 and could take a row of its own
    b.shiny = {segmented_width({{tx("studio.pic.normal"), HIT_NORMAL}, {tx("studio.pic.shiny"), HIT_SHINY}}), 0, 0, true};
    b.stage = {ov_stage_width(), 0, 0, view};
    b.lighting = {toggle_width("studio.view.lighting"), 0, 0, view};
    b.particles = {toggle_width("studio.moves.particles"), 0, 0, view};
    const float lkw = text_width(tx("studio.view.change_style"), 12.5f, DWRITE_FONT_WEIGHT_MEDIUM);
    // (the footer's rows are laid from a nominal top, then moved to the card's foot once their number is known)
    const int frows = flow_band({&b.shiny, &b.stage, &b.lighting, &b.particles}, M.x + 16, M.x + M.w - 12, 8, lkw + 20, 10);
    b.foot_h = 46 + (frows - 1) * 40.0f;
    b.foot_y = M.y + M.h - b.foot_h;
    for (BandItem* it : {&b.shiny, &b.stage, &b.particles, &b.lighting}) it->y += b.foot_y;
    b.link = {lkw, M.x + 16, b.foot_y, true};
    // the hint: on the first row, between the link and the first control there, where it fits whole
    float row0_left = M.x + M.w - 12;
    for (const BandItem* it : {&b.shiny, &b.stage, &b.particles, &b.lighting})
        if (it->on && std::fabs(it->y - (b.foot_y + 8)) < 1) row0_left = std::min(row0_left, it->x);
    b.hint_x = M.x + 16 + lkw + 20;
    b.hint_w = row0_left - 12 - b.hint_x;
    if (b.hint_w < text_width(tx("studio.viewer.hint"), 12, DWRITE_FONT_WEIGHT_NORMAL)) b.hint_w = 0;
    return b;
}

void draw_model_card(const Layout& L) {
    const Palette& P = g.pal;
    const Rect& M = L.model;
    card(M, L"");
    const ModelBands b = model_bands(M);
    // the header (rows: model_bands). Idle shows as chosen while the next model loads (Normal <-> Shiny, another form):
    // without a model "has an idle" is false, which flashed Standing pose for a moment (snatcherLeo, 2026-11-19)
    const bool idle = shown_idle();
    segmented(b.idle.x + b.idle.w, b.idle.y,
              {{tx("studio.viewer.motion_idle"), HIT_IDLE}, {tx("studio.viewer.motion_standing"), HIT_STANDING}},
              idle ? HIT_IDLE : HIT_STANDING);
    // the preview rate (one setting for every 3D view: the same as the Moves tab's button) and Reset view
    if (b.rate.on) rate_button(b.rate.x, b.rate.y);
    if (b.reset.on) reset_button(b.reset.x, b.reset.y, b.reset_word);
    {
        TextStyle st(false, 0.17f);  // a small capital heading (typography pass)
        text(tx("studio.stage.title"), M.x + 16, M.y + 9, M.w - 32, 30, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    }
    if (b.head_h > 48) fill(M.x + 1, M.y + b.head_h - 1, M.w - 2, 1, P.line);  // (a wrapped header: a line under it)
    const Rect& S = L.stage;
    stage_backdrop(S, false);
    if (!g.viewer.ok())
        text(tx("studio.model.none", {{"reason", widen(g.viewer.error())}}), S.x + 20, S.y, S.w - 40, S.h, 13, P.mute,
             DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, true);
    // the footer
    const float fy = b.foot_y;
    fill(M.x + 1, fy, M.w - 2, 1, P.line);
    segmented(b.shiny.x + b.shiny.w, b.shiny.y, {{tx("studio.pic.normal"), HIT_NORMAL}, {tx("studio.pic.shiny"), HIT_SHINY}},
              g.shiny ? HIT_SHINY : HIT_NORMAL);
    // the Overview's own stage (snatcherLeo, 2026-11-22: "users can toggle across those 3 as well in Overview ... to view the
    // model in those 3 different areas just like Moves"; Studio view until picked, never the Moves tab's choice): a
    // drop-down button (its name and ▾, a native menu: as the preview rate's), not the Moves tab's three buttons, which
    // need about 250 DIPs on a card about 370-640 wide
    if (b.stage.on) {
        const bool hov = g.hover == HIT_OV_STAGE;
        round_fill(b.stage.x, b.stage.y, b.stage.w, 30, 7, hov ? P.hover : P.card);
        round_line(b.stage.x, b.stage.y, b.stage.w, 30, 7, P.line2);
        text(tx(kStageKeys[std::clamp(g.ov_stage, 0, 2)]) + L"  ▾", b.stage.x, b.stage.y, b.stage.w, 30, 12.5f,
             hov ? P.text : P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({b.stage.x, b.stage.y, b.stage.w, 30}, HIT_OV_STAGE);
    }
    // the Pokémon's own effects (12 species: Charizard's tail flame...), on or off; greyed, with no click, for a model
    // without any (its place kept: model_bands)
    if (b.particles.on) {
        if (g.viewer.has_particles()) {
            particle_switch(b.particles.x + b.particles.w, b.particles.y);
        } else {
            const float x = b.particles.x, y = b.particles.y;
            round_fill(x + 4, y + 7, 30, 16, 8, alpha(P.line2, 0.4f));
            g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 12, y + 15), 5.5f, 5.5f), brush(alpha(P.faint, 0.6f)));
            // (no hit spot and no hover card: hover cards there covered the 3D view, which snatcherLeo asked to avoid; greyed is
            // the usual sign of "not for this one")
            text(tx("studio.moves.particles"), x + 40, y, b.particles.w - 40, 30, 12.5f, alpha(P.faint, 0.8f));
        }
    }
    // Game lighting: every model (it's how the game lights them all)
    if (b.lighting.on) lighting_switch(b.lighting.x + b.lighting.w, b.lighting.y);
    // "Change style… →" (snatcherLeo, 2026-11-22: "a click "Change Style... ->" in Overview mini-tab, in green text that links to
    // this mini tab of "Styles""): at the footer's first row's left
    const std::wstring link = tx("studio.view.change_style");
    const bool lhov = g.hover == HIT_CHANGE_STYLE;
    text(link, b.link.x, fy, b.link.w + 4, 46, 12.5f, lhov ? P.text : P.accent2, DWRITE_FONT_WEIGHT_MEDIUM);
    if (lhov) fill(b.link.x, fy + 31, b.link.w, 1, P.accent2);
    hit({b.link.x - 6, fy + 8, b.link.w + 12, 30}, HIT_CHANGE_STYLE);
    // the drag hint: only a hint (the model shows it can be turned), so it's the one thing left out where no room is
    if (b.hint_w > 0) text(tx("studio.viewer.hint"), b.hint_x, fy, b.hint_w, 46, 12, P.mute);
}

// base stats: bars coloured from low to high (the sandbox's scale), the total under them; hover explains each
void draw_stats_card(const Rect& R, const Entry& e) {
    const Palette& P = g.pal;
    card(R, tx("studio.card.stats"), tx("studio.era.colosseum"));
    float lw = 62;
    for (const char* k : kStatKeys) lw = std::max(lw, text_width(tx(k), 13.5f, DWRITE_FONT_WEIGHT_NORMAL) + 8);
    int total = 0;
    for (int i = 0; i < 6; ++i) {
        const int v = e.s.base_stats[i];
        total += v;
        const float ry = R.y + kCardHead + i * 28;
        // (no hover card on the stat rows: snatcherLeo, 2026-11-19, "We don't need hoverable tooltips for each stat for the
        // pokemon"; the stat names are plain enough. The words stay in studio/lang, studio.help.stat.*, unused for now)
        text(tx(kStatKeys[i]), R.x + 16, ry, lw, 28, 13.5f, P.text2);
        {
            TextStyle st(true);  // same-width digits, so the values line up
            text(std::to_wstring(v), R.x + 16 + lw, ry, 34, 28, 13.5f, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
        const float bx0 = R.x + 16 + lw + 34 + 12, bw = R.x + R.w - 16 - bx0;
        round_fill(bx0, ry + 11, bw, 6, 3, P.dark ? hexc(0x2A2F2A) : P.line);
        // every bar in the one sage accent, only the length showing the value (typography pass, 2026-11-18; it was a
        // four-colour ramp by value)
        round_fill(bx0, ry + 11, bw * std::min(1.0f, v / 170.0f), 6, 3, P.accent);
    }
    const float ty = R.y + kCardHead + 6 * 28 + 8;
    fill(R.x + 16, ty, R.w - 32, 1, P.line);
    text(tx("studio.statname.total"), R.x + 16, ty + 6, 120, 28, 13.5f, P.mute);
    TextStyle st(true);
    text(std::to_wstring(total), R.x + 16, ty + 6, R.w - 32, 28, 14, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_TRAILING);
}

// Training: what the species record says about raising it (verified field meanings: pokemon-data.md), plus a note
// where the outside data fills in later (egg groups: Colosseum has no breeding)
std::wstring effort_text(const OrreSpecies& s) {
    static const char* kShort[] = {"studio.statname.hp", "studio.statname.attack", "studio.statname.defense",
                                   "studio.statname.sp_atk", "studio.statname.sp_def", "studio.statname.speed"};
    std::wstring out;
    for (int i = 0; i < 6; ++i)
        if (s.effort[i]) {
            if (!out.empty()) out += L" · ";
            out += tx("studio.effort.item", {{"n", std::to_wstring(s.effort[i])}, {"stat", tx(kShort[i])}});
        }
    return out.empty() ? tx("studio.held.none") : out;
}
std::wstring held_text(const OrreSpecies& s) {
    std::wstring out;
    for (int i = 0; i < 2; ++i)
        if (s.held_items[i]) {
            const std::wstring n = title_case(widen(orre_text(g.disc, orre_item_name_msg(g.disc, s.held_items[i]))));
            if (n.empty()) continue;
            if (!out.empty()) out += L" · ";
            out += n;
        }
    return out.empty() ? tx("studio.held.none") : out;
}
void draw_train_card(const Rect& R, const Entry& e) {
    const Palette& P = g.pal;
    const OrreSpecies& s = e.s;
    card(R, tx("studio.card.training"), tx("studio.card.training_src"));
    char gk[32];
    std::snprintf(gk, sizeof gk, "studio.growth.%d", s.growth_rate <= 5 ? s.growth_rate : 0);
    const char* how = s.catch_rate <= 75 ? "studio.catch.hard" : s.catch_rate <= 150 ? "studio.catch.medium" : "studio.catch.easy";
    // gender: the record's ratio (255 genderless, 254 female only, 0 male only; else female when the personality's
    // low byte is below it), shown in eighths as the series does (31 -> 87.5% male)
    std::wstring gender;
    float male = -1;
    if (s.gender_ratio == 255) gender = tx("studio.gender.none");
    else if (s.gender_ratio == 254) { gender = tx("studio.gender.female_only"); male = 0; }
    else if (s.gender_ratio == 0) { gender = tx("studio.gender.male_only"); male = 1; }
    else {
        male = std::round((256 - s.gender_ratio) / 256.0f * 8) / 8;
        wchar_t pc[16];
        swprintf(pc, 16, male * 100 == std::floor(male * 100) ? L"%.0f" : L"%.1f", male * 100);
        gender = tx("studio.gender.male", {{"percent", pc}});
    }
    const std::pair<const char*, std::wstring> rows[] = {
        {"studio.train.exp_group", tx(gk)},
        {"studio.train.catch_rate", tx("studio.catch.value", {{"rate", std::to_wstring(s.catch_rate)}, {"how", tx(how)}})},
        {"studio.train.gender", gender},
        {"studio.train.friendship", std::to_wstring(s.base_friendship)},
        {"studio.train.exp_given", std::to_wstring(s.exp_yield)},
        {"studio.train.effort", effort_text(s)},
        {"studio.train.held", held_text(s)},
        {"studio.train.eggs", tx("studio.eggs.outside")}};
    float lw = 80;
    for (auto& r : rows) lw = std::max(lw, text_width(tx(r.first), 13, DWRITE_FONT_WEIGHT_NORMAL) + 14);
    // one row height for every row, hairlines between them; names regular in the softer grey, values semibold in the
    // main colour, numbers in same-width digits (typography pass, 2026-11-18); the gender bar sits beside its value
    float y = R.y + kCardHead - 4;
    const float rh = 27.5f;
    for (int i = 0; i < 8; ++i) {
        if (g.hover == HIT_TRAIN + i) round_fill(R.x + 10, y, R.w - 20, rh, 6, P.hover);
        if (i > 0) fill(R.x + 16, y, R.w - 32, 1, P.line);
        text(tx(rows[i].first), R.x + 16, y, lw, rh, 13, P.mute);
        {
            TextStyle st(i == 1 || i == 2 || i == 3 || i == 4);
            text(rows[i].second, R.x + 16 + lw, y, R.w - 32 - lw, rh, 13, i == 7 ? P.faint : P.text,
                 i == 7 ? DWRITE_FONT_WEIGHT_NORMAL : DWRITE_FONT_WEIGHT_SEMI_BOLD);
        }
        if (i == 2 && male >= 0) {  // the gender bar: male blue, female pink
            float vw;
            {
                TextStyle st(true);
                vw = text_width(rows[i].second, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            }
            const float bx = R.x + 16 + lw + vw + 12, bw = std::min(90.0f, R.x + R.w - 16 - bx);
            if (bw > 20) {
                round_fill(bx, y + rh / 2 - 3, bw, 6, 3, hexc(0xF95587));
                if (male > 0) round_fill(bx, y + rh / 2 - 3, bw * male, 6, 3, hexc(0x6390F0));
            }
        }
        hit({R.x + 10, y, R.w - 20, rh}, HIT_TRAIN + i);
        y += rh;
    }
}

// Abilities: ability 1 and 2 as framed rows (the second "none in Colosseum" when the species has one), then the
// hidden ability's row, which arrives with Gen 5's outside data; hover shows the game's own description
void draw_abil_card(const Rect& R, const Entry& e) {
    const Palette& P = g.pal;
    card(R, tx("studio.card.abilities"));
    for (int k = 0; k < 3; ++k) {
        const float ry = R.y + kCardHead + k * 46;
        const int a = k < 2 ? e.s.abilities[k] : 0;
        const bool live = a > 0, hov = live && g.hover == HIT_ABILITY + k;
        round_line(R.x + 14, ry, R.w - 28, 38, 9, hov ? P.accent_line : P.line);
        text(tx(k == 0 ? "studio.ability.slot1" : k == 1 ? "studio.ability.slot2" : "studio.ability.hidden"), R.x + 26, ry, 64, 38, 11,
             P.mute);
        if (live) {
            text(ability_name(a), R.x + 94, ry, R.w - 200, 38, 14, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            const std::wstring tag = tx("studio.ability.since");
            const float tw = text_width(tag, 11.5f, DWRITE_FONT_WEIGHT_NORMAL) + 14;
            round_line(R.x + R.w - 24 - tw, ry + 9, tw, 20, 5, P.line2);
            text(tag, R.x + R.w - 24 - tw, ry + 9, tw, 20, 11.5f, P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            hit({R.x + 14, ry, R.w - 28, 38}, HIT_ABILITY + k);
        } else {
            text(tx(k < 2 ? "studio.ability.none_colo" : "studio.ability.hidden_note"), R.x + 94, ry, R.w - 120, 38, 13.5f, P.faint);
            if (k == 2) hit({R.x + 14, ry, R.w - 28, 38}, HIT_ABILITY_HIDDEN);
        }
    }
}

// ---- Cries (2026-11-22; snatcherLeo's pick C of mockup 27: "a small cries card", then the Sound tab) ----
// The shown species' cry, read from the disc the first time it's asked for (core: orre_species_cry, the game's own
// sample, verified identical to the extractor's for Pikachu and Celebi), kept until another species is shown.
struct CryCache {
    OrreDisc* disc = nullptr;
    int species = -1;
    OrreSound snd{};
    bool ok = false;
};
CryCache g_cry;  // (one for the process: only one cry plays at a time, and the pick is cheap to read again)
void media_symbol(int kind, float cx, float cy, float s, const D2D1_COLOR_F& c);  // (further down: the Moves tab's)
void trail(const char* category, const std::string& what);                       // (the action trail, further down)
const OrreSound* cry_for(int species) {
    if (!g.disc || species < 0) return nullptr;
    if (g_cry.disc != g.disc || g_cry.species != species) {
        orre_sound_free(&g_cry.snd);
        g_cry.disc = g.disc;
        g_cry.species = species;
        g_cry.ok = orre_species_cry(g.disc, species, &g_cry.snd) != 0;
    }
    return g_cry.ok ? &g_cry.snd : nullptr;
}
// The fainting cry's preview: the cry played at 80% speed, so lower and slower together (snatcherLeo: "I believe fainting
// cries are just pitched from the normal cry"). Studio's own preview, not what the game plays: the game's code starts
// no cry at a faint (docs/engine/battle-engine.md "Cries in battle", verified from code; the Dolphin check is batched,
// visual-checks.md). The 80% is a guess at "lower and slower", to be tuned by ear.
constexpr double kFaintSpeed = 0.8;
std::vector<uint8_t> g_cry_wav;  // the WAV being played (PlaySound reads it while it plays, so it's kept here)
// Plays the shown species' cry (or the fainting preview) through Windows' sound player: an in-memory WAV, 16-bit mono,
// asynchronous; a new one stops the last. Windows only (winmm's PlaySound); Linux / macOS: the shell's audio output
// (likely miniaudio, the port's choice, when those shells come).
void play_cry(bool faint) {
    if (g.selected < 0) return;
    const OrreSound* c = cry_for(g.all[g.selected].index);
    if (!c || c->count <= 0) return;
    PlaySoundW(nullptr, nullptr, 0);  // (stops the one playing before its buffer is replaced)
    const uint32_t rate = (uint32_t)std::lround(c->rate * (faint ? kFaintSpeed : 1.0)), bytes = (uint32_t)c->count * 2;
    g_cry_wav.assign(44 + bytes, 0);
    uint8_t* w = g_cry_wav.data();
    auto put32 = [](uint8_t* p, uint32_t v) { for (int k = 0; k < 4; ++k) p[k] = (uint8_t)(v >> (8 * k)); };
    auto put16 = [](uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); };
    std::memcpy(w, "RIFF", 4); put32(w + 4, 36 + bytes); std::memcpy(w + 8, "WAVEfmt ", 8);
    put32(w + 16, 16); put16(w + 20, 1); put16(w + 22, 1); put32(w + 24, rate); put32(w + 28, rate * 2);
    put16(w + 32, 2); put16(w + 34, 16); std::memcpy(w + 36, "data", 4); put32(w + 40, bytes);
    for (int k = 0; k < c->count; ++k) put16(w + 44 + 2 * k, (uint16_t)c->pcm[k]);  // (little-endian, as WAV is)
    const bool ok = PlaySoundW(reinterpret_cast<LPCWSTR>(g_cry_wav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    trail("cry", faint ? "fainting preview" : "cry");
    logf(ok ? Level::Info : Level::Warn, "cry", "%s: species %d, %u Hz, %d samples%s", faint ? "fainting preview" : "cry",
         g.all[g.selected].index, (unsigned)rate, c->count, ok ? "" : " (Windows didn't play it)");
}
// the cry's length as shown ("1.03 s"; the fainting preview's is longer by its slower speed)
std::wstring cry_length(const OrreSound* c, bool faint) {
    if (!c || c->rate <= 0) return L"";
    wchar_t n[16];
    std::swprintf(n, 16, L"%.1f", c->count / (double)c->rate / (faint ? kFaintSpeed : 1.0));
    return tx("studio.cries.seconds", {{"n", n}});
}
// One cry row: a round play button (when `play_id` >= 0), its name, a small tag, the length at the right; the whole
// row is the button. Faded rows (`later`) are what modding brings: no button, no click.
void cry_row(float x, float y, float w, int play_id, const std::wstring& name, const std::wstring& tag,
             const std::wstring& right, bool later = false, bool in_use = false) {
    const Palette& P = g.pal;
    const bool hov = play_id >= 0 && g.hover == play_id;
    round_line(x, y, w, kCryRow - 8, 9, in_use ? P.accent_line : hov ? P.line2 : P.line);
    if (hov) round_fill(x + 1, y + 1, w - 2, kCryRow - 10, 8, P.hover);
    const float cx = x + 22, cy = y + (kCryRow - 8) / 2;
    if (play_id >= 0) {
        g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 12, 12), brush(hov ? P.accent : P.accent_bg));
        media_symbol(0, cx + 1, cy, 9, hov ? P.on_accent : P.text);
    } else if (later) {
        text(L"+", cx - 10, y, 20, kCryRow - 8, 15, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    const D2D1_COLOR_F ink = later ? P.faint : P.text;
    const float rw = right.empty() ? 0 : text_width(right, 12, DWRITE_FONT_WEIGHT_NORMAL) + 14;
    const float tagw = tag.empty() ? 0 : text_width(tag, 11, DWRITE_FONT_WEIGHT_NORMAL) + 12;
    const float nx = x + 44, nw = std::max(10.0f, w - 44 - rw - (tagw > 0 ? tagw + 8 : 0) - 8);
    text(name, nx, y, nw, kCryRow - 8, 13.5f, ink, DWRITE_FONT_WEIGHT_NORMAL);
    if (tagw > 0) {
        const float tx0 = nx + std::min(nw, text_width(name, 13.5f, DWRITE_FONT_WEIGHT_NORMAL)) + 8;
        round_fill(tx0, cy - 10, tagw, 20, 5, in_use ? P.accent_bg : P.chip);
        text(tag, tx0, cy - 10, tagw, 20, 11, in_use ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    if (rw > 0) text(right, x + w - rw - 8, y, rw, kCryRow - 8, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
    if (play_id >= 0) hit({x, y, w, kCryRow - 8}, play_id);
}
// The Overview's Cries card: the cry and the fainting preview, each with its play button and length, and the way to
// the Sound tab. Forms share their species' cries (snatcherLeo: "each form has the same ones"), so the card follows the species.
void draw_cries_card(const Rect& R, const Entry& e) {
    const Palette& P = g.pal;
    card(R, tx("studio.cries.title"));
    const OrreSound* c = cry_for(e.index);
    const float x = R.x + 14, w = R.w - 28;
    float y = R.y + kCardHead;
    if (!c) {
        text(tx("studio.cries.none"), x + 4, y, w - 8, 2 * kCryRow - 8, 13, P.faint, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_LEADING, true);
    } else {
        cry_row(x, y, w, HIT_CRY, tx("studio.cries.cry"), L"", cry_length(c, false));
        y += kCryRow;
        cry_row(x, y, w, HIT_CRY_FAINT, tx("studio.cries.faint"), tx("studio.cries.preview"), cry_length(c, true));
    }
    const std::wstring link = tx("studio.cries.in_sound");
    const float lw = text_width(link, 12.5f, DWRITE_FONT_WEIGHT_MEDIUM), ly = R.y + R.h - 34;
    const bool lh = g.hover == HIT_CRY_SOUND;
    text(link, R.x + 16, ly, lw + 4, 30, 12.5f, lh ? P.text : P.accent2, DWRITE_FONT_WEIGHT_MEDIUM);
    if (lh) fill(R.x + 16, ly + 23, lw, 1, P.accent2);
    hit({R.x + 10, ly, lw + 12, 30}, HIT_CRY_SOUND);
}
// The Sound tab (mockup 27, part 2; snatcherLeo: "part 2 makes sense. Eventually, users will be able to replace both (when I
// add in modding)"): the species' two cries with what each can come from (the game's, Studio's fainting preview, the
// player's own later), and where the game plays them. Side by side on a wide page, stacked on a narrow one.
void draw_sound(const Layout& L) {
    const Palette& P = g.pal;
    if (g.selected < 0) return;
    const Entry& e = g.all[g.selected];
    const OrreSound* c = cry_for(e.index);
    const float x0 = L.content.x, cw = L.content.w, top = L.body_y;
    const bool side = cw >= 760;
    const float lw = side ? std::floor(cw * 0.58f) : cw, rw = side ? cw - lw - kColGap : cw;
    // the cries card
    const float sub = 26;  // (each cry's heading line)
    const float lh = kCardHead + sub + 2 * kCryRow + 10 + sub + kFaintRowH + kCryRow + 52;
    const Rect A{x0, top, lw, lh};
    card(A, tx("studio.sound.cries_title"));
    const float x = A.x + 14, w = A.w - 28;
    float y = A.y + kCardHead;
    text(tx("studio.cries.cry"), x + 2, y, w, sub - 4, 13.5f, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    text(tx("studio.sound.cry_where"), x + 2 + text_width(tx("studio.cries.cry"), 13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 8, y,
         w, sub - 4, 12, P.mute);
    y += sub;
    if (c) cry_row(x, y, w, HIT_CRY, tx("studio.sound.game"), tx("studio.sound.in_use"), cry_length(c, false), false, true);
    else cry_row(x, y, w, -1, tx("studio.cries.none"), L"", L"", true);
    y += kCryRow;
    cry_row(x, y, w, -1, tx("studio.sound.add_cry"), L"", L"", true);
    y += kCryRow + 10;
    text(tx("studio.cries.faint"), x + 2, y, w, sub - 4, 13.5f, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    y += sub;
    // one row (*changed 2026-11-22*, snatcherLeo: "Could we consolidate those 2 things under Fainting Cry"): what the game does
    // at a faint today (in use) on the first line, and the preview it plays on the second, with its button and length
    {
        const float h2 = kFaintRowH - 8, cy = y + h2 / 2;
        const bool hov = c && g.hover == HIT_CRY_FAINT;
        round_line(x, y, w, h2, 9, P.accent_line);
        if (hov) round_fill(x + 1, y + 1, w - 2, h2 - 2, 8, P.hover);
        if (c) {
            g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 22, cy), 12, 12), brush(hov ? P.accent : P.accent_bg));
            media_symbol(0, x + 23, cy, 9, hov ? P.on_accent : P.text);
        }
        // (line 1 named as the Cry row's, snatcherLeo: "should it say something more direct as (Colosseum - the game's own.)
        // and then we mention how it's actually just pitched down lower"; line 2 says how it's made)
        const std::wstring l1 = tx("studio.sound.game"), tag = tx("studio.sound.in_use"), len = c ? cry_length(c, true) : L"";
        const float rw = len.empty() ? 0 : text_width(len, 12, DWRITE_FONT_WEIGHT_NORMAL) + 14;
        const float nx = x + 44, nw = std::max(10.0f, w - 44 - rw - 8);
        text(l1, nx, y + 7, nw, 22, 13.5f, P.text);
        const float tagw = text_width(tag, 11, DWRITE_FONT_WEIGHT_NORMAL) + 12;
        const float tx0 = nx + std::min(nw - tagw, text_width(l1, 13.5f, DWRITE_FONT_WEIGHT_NORMAL) + 8);
        round_fill(tx0, y + 8, tagw, 20, 5, P.accent_bg);
        text(tag, tx0, y + 8, tagw, 20, 11, P.accent2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (c) {
            text(tx("studio.sound.made"), nx, y + 30, nw, 20, 12, P.mute);
            text(len, x + w - rw - 8, y, rw, h2, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            hit({x, y, w, h2}, HIT_CRY_FAINT);
        }
    }
    y += kFaintRowH;
    cry_row(x, y, w, -1, tx("studio.sound.add_faint"), L"", L"", true);
    y += kCryRow;
    text(tx("studio.sound.forms"), x + 2, y, w - 4, 44, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
    // where the game plays them (verified from the code: battle-engine.md "Cries in battle", evolution.c, the P★DA)
    static const char* kWhere[][2] = {{"studio.sound.where.sent_out", "studio.sound.where.cry"},
                                      {"studio.sound.where.shadow", "studio.sound.where.cry"},
                                      {"studio.sound.where.evolution", "studio.sound.where.cry_both"},
                                      {"studio.sound.where.memo", "studio.sound.where.cry"},
                                      {"studio.sound.where.faint", "studio.sound.where.faint_val"}};
    const float rh = kCardHead + 5 * 34 + 70;
    const Rect B{side ? x0 + lw + kColGap : x0, side ? top : top + lh + kCardGap, rw, rh};
    card(B, tx("studio.sound.where_title"));
    float ry = B.y + kCardHead;
    for (int k = 0; k < 5; ++k) {
        if (k) fill(B.x + 16, ry, B.w - 32, 1, P.line);
        text(tx(kWhere[k][0]), B.x + 16, ry, B.w * 0.5f, 34, 13, P.mute);
        text(tx(kWhere[k][1]), B.x + B.w * 0.5f, ry, B.w * 0.5f - 16, 34, 13, P.text, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_TRAILING);
        ry += 34;
    }
    text(tx("studio.sound.style_note"), B.x + 16, ry + 6, B.w - 32, 60, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL,
         DWRITE_TEXT_ALIGNMENT_LEADING, true);
}

// Matchups (snatcherLeo's wording): Weak to (×4 / ×2), Resists (×½ / ×¼), Immune to (×0); each row's chips wrap
struct MatchRow { const char* key; int want; };
const MatchRow kMatchRows[] = {{"studio.match.x4", 40}, {"studio.match.x2", 20}, {"studio.match.half", 5},
                               {"studio.match.quarter", 2}, {"studio.match.zero", 0}};
float match_label_w(const OrreSpecies& s) {
    float lw = 100;
    for (const MatchRow& r : kMatchRows)
        if (!matchups(s, r.want).empty()) lw = std::max(lw, text_width(tx(r.key), 13, DWRITE_FONT_WEIGHT_NORMAL) + 14);
    return lw;
}
int chip_lines(const std::vector<int>& v, float x0, float x1) {
    int lines = 1;
    float x = x0;
    for (int t : v) {
        const float w = chip_width(t, 12) + 6;
        if (x + w > x1 && x > x0) { ++lines; x = x0; }
        x += w;
    }
    return lines;
}
float match_height(const OrreSpecies& s, float width) {
    const float lw = match_label_w(s), x0 = 16 + lw, x1 = width - 14;
    float h = kCardHead + 8;
    for (const MatchRow& r : kMatchRows) {
        auto v = matchups(s, r.want);
        if (!v.empty()) h += chip_lines(v, x0, x1) * 30 + 6;
    }
    return h;
}
void draw_match_card(const Rect& R, const Entry& e) {
    const OrreSpecies& s = e.s;
    const std::wstring types = s.types[1] != s.types[0] ? type_name(s.types[0]) + L" · " + type_name(s.types[1]) : type_name(s.types[0]);
    card(R, tx("studio.card.matchups"), types);
    const float lw = match_label_w(s), x0 = R.x + 16 + lw, x1 = R.x + R.w - 14;
    float y = R.y + kCardHead;
    for (const MatchRow& r : kMatchRows) {
        auto v = matchups(s, r.want);
        if (v.empty()) continue;
        text(tx(r.key), R.x + 16, y, lw, 24, 13, g.pal.mute, DWRITE_FONT_WEIGHT_MEDIUM);
        float x = x0, yy = y + 1;
        for (int t : v) {
            const float w = chip_width(t, 12) + 6;
            if (x + w > x1 && x > x0) { x = x0; yy += 30; }
            type_chip(t, x, yy, 12, 22, HIT_MATCH + t);
        }
        y += chip_lines(v, x0, x1) * 30 + 6;
    }
}

// The Forms card (only for species with more than one look: Unown, Castform, Deoxys): each form's mug shot as a
// thumbnail with its name under it, the shown one outlined; clicking one shows it in the model card and the Looks tab.
// Forms this disc doesn't have are greyed (Deoxys), with the note at the card's top right.
void draw_forms_card(const Rect& R, const Entry& e) {
    if (R.h <= 0) return;
    const Palette& P = g.pal;
    card(R, tx("studio.card.forms"), e.index == kDeoxys ? tx("studio.form.deoxys.note") : tx("studio.form.hint"));
    const float tile = kFormTile, fx0 = R.x + kFormCardPad;
    const int per = forms_per_line();
    float fx = fx0, fy = R.y + 40;
    for (size_t i = 0; i < g.forms.size(); ++i) {
        const auto& f = g.forms[i];
        if (i > 0 && i % (size_t)per == 0) { fx = fx0; fy += kFormLine; }
        const bool on = (int)i == g.form, hov = g.hover == HIT_FORM + (int)i && f.available && !on;
        if (on) round_line(fx, fy, tile, tile, 9, P.accent, 1.5f);
        else if (hov) round_fill(fx, fy, tile, tile, 9, P.hover);
        else round_line(fx, fy, tile, tile, 9, P.line);
        if (f.bmp) {
            if (f.available) draw_mug(f.bmp, fx + 4, fy + 4, tile - 8);
            else fill_rounded_picture(f.bmp, fx + 4, fy + 4, tile - 8, mug_radius(tile - 8), 0.3f);  // (faded: not on the disc)
        }
        text(f.label, fx - 12, fy + tile + 3, tile + 24, 18, 11, on ? P.text : f.available ? P.mute : P.faint,
             on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (f.available && !on) hit({fx, fy, tile, tile + 20}, HIT_FORM + (int)i);
        fx += tile + kFormGap;
    }
}

void draw_evo_card(const Rect& R, const Entry& e);
void draw_overview(const Layout& L, const Entry& e) {
    draw_model_card(L);
    draw_stats_card(L.stats, e);
    draw_train_card(L.train, e);
    draw_abil_card(L.abil, e);
    if (L.cries.h > 0) draw_cries_card(L.cries, e);
    draw_match_card(L.match, e);
    draw_evo_card(L.evo, e);
    draw_forms_card(L.forms, e);
}

// ---- Evolution (v3) ----
// The family is split where it branches: a straight line up to the first member with more than one evolution (Bulbasaur
// line: all of it), then a fan of cards for everything after that member (Eevee's five; Wurmple's four, the second
// stage saying which member it comes from). On the line, each step's method is a pill on the arrow ("↑ Level 16").
// Item methods (kinds 6 and 7) are links to the Item editor with the item symbol; friendship methods (1-3) are links to
// Friendship in the Battle editor's game rules (snatcherLeo: "We can plan for the friendship stuff to be in Battle Editor").
struct EvoSplit { std::vector<int> line; std::vector<int> fan; };  // indices into the family
EvoSplit split_family(const std::vector<EvoStep>& fam) {
    EvoSplit s;
    int at = 0;
    while (true) {
        s.line.push_back(at);
        std::vector<int> kids;
        for (size_t i = 0; i < fam.size(); ++i) if (fam[i].parent == at) kids.push_back((int)i);
        if (kids.size() != 1) {
            if (kids.size() > 1)
                for (size_t i = 0; i < fam.size(); ++i)  // every member after the branching one, in family order
                    for (int p = fam[i].parent; p >= 0; p = fam[(size_t)p].parent)
                        if (p == at) { s.fan.push_back((int)i); break; }
            break;
        }
        at = kids[0];
    }
    return s;
}
// The look (snatcherLeo, 2026-11-18 through C: "B arrow cards work. However, for then, in Gardevoir's case, when we eventually
// get to it's branched, we just do what E does for the branch but with an arrow"; mockup 23's .ev / .arr):
// - a straight line is arrow cards: each member a small card (mug shot, name, No.), joined by a thin arrow with the
//   method above it; the shown Pokémon's card has the accent border;
// - a member with exactly two evolutions (Wurmple, Poliwhirl, Slowpoke, Gloom, Nincada, Clamperl) keeps the first
//   on the line and forks the second off below it: a curved arrow from the branching card down to its card, the
//   method beside the curve, its own later stages continuing to the right;
// - three or more (Eevee, Tyrogue): snatcherLeo's V1 (2026-11-18 through C: "V1. Ofc, get rid of the "5 ways" and "this
//   one", users will already be able to see that. I like how the pokmeon's path is lit up. We wouldn't normally do
//   that for normal evolutions, only if there is a branch"): one vertical spine from the base card; each evolution
//   leaves it with its own arrow, the method a small chip on the arrow (day and night a sun and a moon), then its card.
// - wherever a family branches (the fork and the spine), the path to the shown Pokémon is lit: its arrows and chips
//   in the accent colour. Straight lines never light theirs. No "5 evolutions" / "this one" labels anywhere.
// Still open (not built): the item's own picture in the chip; two spines side by side for more than six branches.
constexpr float kEvoNodeH = 58, kEvoArrW = 132, kEvoArrMin = 64, kEvoRowGap = 44, kSpineIn = 26, kSpineGap = 10;
struct EvoNode { int i; Rect r; std::wstring sub; };
// an arrow to member i: 0 straight with the method above it, 1 the fork's curve (method beside it), 2 straight with
// the method as a chip on it (the spine); lit = on the path to the shown Pokémon in a branching family; label_w: the
// width the method may take before it wraps (0 = no limit)
struct EvoArrow { int i; float x0, y0, x1, y1; int style; bool lit; float label_w; };
struct EvoSeg { float x0, y0, x1, y1; bool lit; };  // the spine and its join to the base card (no head)
struct EvoPlan { std::vector<EvoNode> nodes; std::vector<EvoArrow> arrows; std::vector<EvoSeg> segs; float h = 0; };
Entry* evo_member(const std::vector<EvoStep>& fam, int i) {
    auto it = g.by_index.find(fam[(size_t)i].index);
    return it == g.by_index.end() ? nullptr : &g.all[it->second];
}
// a member and its single-child descendants (a chain ends at a member with no evolution, or with more than one)
std::vector<int> evo_chain(const std::vector<EvoStep>& fam, int start) {
    std::vector<int> c = {start};
    for (int at = start;;) {
        int only = -1, n = 0;
        for (size_t i = 0; i < fam.size(); ++i) if (fam[i].parent == at) { only = (int)i; ++n; }
        if (n != 1) return c;
        c.push_back(at = only);
    }
}
std::vector<int> evo_kids(const std::vector<EvoStep>& fam, int at) {
    std::vector<int> k;
    for (size_t i = 0; i < fam.size(); ++i) if (fam[i].parent == at) k.push_back((int)i);
    return k;
}
bool method_is_item(int kind) { return kind == 6 || kind == 7; }
bool method_is_friend(int kind) { return kind >= 1 && kind <= 3; }
float evo_node_w(const std::vector<EvoStep>& fam, int i, const std::wstring& sub) {
    const Entry* m = evo_member(fam, i);
    const float nw = m ? text_width(m->name, 13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) : 0, sw = text_width(sub, 11.5f, DWRITE_FONT_WEIGHT_NORMAL);
    return std::max(132.0f, std::ceil(8 + 42 + 12 + std::max(nw, sw) + 16));
}
// the method's words and symbol: an item symbol for item methods, a heart for friendship, and for day and night a sun
// and a moon with the plain "Friendship" (the chip's wording; snatcherLeo's V1)
std::wstring method_text(int kind, int param, bool chip) {
    return chip && (kind == 2 || kind == 3) ? evo_method(1, param) : evo_method(kind, param);
}
float method_text_w(int kind, int param, bool chip) {
    const float icon = method_is_item(kind) || method_is_friend(kind) ? 17 : 0;
    return text_width(method_text(kind, param, chip), 11.5f, DWRITE_FONT_WEIGHT_NORMAL) + icon;
}
// Where everything goes, for both the height (layout) and the drawing: x0 / y0 the card's inside top left, width its
// inside width, cur the shown member (-1: none). An arrow is as wide as its method's words (Wurmple's "Lv. 7 (by
// personality)") when the row fits; when it doesn't (a narrow page), the arrows share what is left (at least
// kEvoArrMin) and their words wrap.
EvoPlan evo_plan(const std::vector<EvoStep>& fam, int cur, float x0, float y0, float width) {
    EvoPlan p;
    const EvoSplit sp = split_family(fam);
    const int b = sp.line.back();
    const std::vector<int> kids = evo_kids(fam, b);
    const bool branching = kids.size() >= 2;
    // on the lit path: the shown member and its ancestors after the branching member (only in a branching family)
    std::vector<char> lit(fam.size(), 0);
    if (branching && cur >= 0)
        for (int a = cur; a >= 0 && a != b; a = fam[(size_t)a].parent) lit[(size_t)a] = 1;
    auto sub_of = [&](int i) {
        const Entry* m = evo_member(fam, i);
        return m ? tx("studio.list.number", {{"number", number3(m->s.national_dex)}}) : std::wstring();
    };
    const float my = kEvoNodeH / 2;
    // one row of cards joined by arrows, starting at x (the first card's arrow already placed by the caller when
    // `lead` is false); returns the node index of each member
    auto row = [&](const std::vector<int>& members, float x, float y, int style, bool first_has_arrow) {
        float nodes_w = 0, natural = 0;
        for (size_t k = 0; k < members.size(); ++k) {
            nodes_w += evo_node_w(fam, members[k], sub_of(members[k]));
            if (k > 0 || first_has_arrow) natural += std::max(kEvoArrW, method_text_w(fam[(size_t)members[k]].kind, fam[(size_t)members[k]].param, style == 2) + 24);
        }
        const int n_arr = (int)members.size() - (first_has_arrow ? 0 : 1);
        const bool fits = x + nodes_w + natural <= x0 + width || n_arr == 0;
        const float shared = n_arr > 0 ? std::max(kEvoArrMin, (x0 + width - x - nodes_w) / (float)n_arr) : 0;
        for (size_t k = 0; k < members.size(); ++k) {
            const int i = members[k];
            if (k > 0 || first_has_arrow) {
                const float tw = method_text_w(fam[(size_t)i].kind, fam[(size_t)i].param, style == 2);
                const float aw = fits ? std::max(kEvoArrW, tw + 24) : std::min(shared, std::max(kEvoArrW, tw + 24));
                p.arrows.push_back({i, x + 2, y + my, x + aw - 4, y + my, style, lit[(size_t)i] != 0, fits ? 0.0f : aw - 8});
                x += aw;
            }
            const std::wstring sub = sub_of(i);
            const float w = evo_node_w(fam, i, sub);
            p.nodes.push_back({i, {x, y, w, kEvoNodeH}, sub});
            x += w;
        }
    };
    if (kids.size() <= 2) {
        std::vector<int> row1 = sp.line;
        if (kids.size() == 2) for (int i : evo_chain(fam, kids[0])) row1.push_back(i);
        row(row1, x0, y0, 0, false);
        p.h = kEvoNodeH;
        if (kids.size() == 2) {
            // the fork: from the bottom of the branching card, curving down to the second evolution's card, which
            // sits under the line's next card; its later stages continue to the right
            const EvoNode* bn = nullptr;
            const EvoNode* kn = nullptr;
            for (const EvoNode& n : p.nodes) { if (n.i == b) bn = &n; if (n.i == kids[0]) kn = &n; }
            const Rect br = bn->r;
            const float y2 = y0 + kEvoNodeH + kEvoRowGap, x2 = kn->r.x;
            p.arrows.push_back({kids[1], br.x + br.w / 2, br.y + br.h + 2, x2 - 4, y2 + my, 1, lit[(size_t)kids[1]] != 0, 0});
            row(evo_chain(fam, kids[1]), x2, y2, 0, false);
            p.h = 2 * kEvoNodeH + kEvoRowGap;
        }
        return p;
    }
    // the spine: the line up to the branching member, then from its right edge a short join to a vertical spine;
    // each evolution's arrow leaves the spine with its method as a chip, its cards in one column
    row(sp.line, x0, y0, 0, false);
    const Rect br = p.nodes.back().r;
    const float sx = br.x + br.w + kSpineIn;
    float chip_w = 0;
    for (int k : kids) chip_w = std::max(chip_w, method_text_w(fam[(size_t)k].kind, fam[(size_t)k].param, true) + 22);
    const float arr = std::max(110.0f, chip_w + 40);
    float last_y = y0, lit_y = -1;
    for (size_t r = 0; r < kids.size(); ++r) {
        const float y = y0 + (float)r * (kEvoNodeH + kSpineGap);
        const std::vector<int> chain = evo_chain(fam, kids[r]);
        // the spine's own arrow to this evolution, then (rarely) its later stages with chip arrows
        p.arrows.push_back({kids[r], sx, y + my, sx + arr - 4, y + my, 2, lit[(size_t)kids[r]] != 0, 0});
        if (lit[(size_t)kids[r]]) lit_y = y;
        row(chain, sx + arr, y, 2, false);
        last_y = y;
    }
    p.segs.push_back({br.x + br.w + 2, y0 + my, sx, y0 + my, lit_y >= 0});
    p.segs.push_back({sx, y0 + my, sx, last_y + my, false});
    if (lit_y > y0) p.segs.push_back({sx, y0 + my, sx, lit_y + my, true});
    p.h = std::max(kEvoNodeH, (float)kids.size() * (kEvoNodeH + kSpineGap) - kSpineGap);
    return p;
}
float evo_height(int index, float width) {
    const auto fam = evolution_family(index);
    if (fam.size() <= 1) return kCardHead + 40;
    return kCardHead + 6 + evo_plan(fam, -1, 0, 0, width - 32).h + 18;
}
// a method as a quiet label (mockup 23's .arr text): centred on x or starting at it; item methods carry the item
// symbol and friendship methods a heart, both links (hover brightens them)
// (maxw > 0: the arrow is narrower than the words, so they wrap onto two lines above it, without the symbol)
void method_label(int kind, int param, float x, float y, int id, bool centre_on_x, bool lit, float maxw = 0) {
    const Palette& P = g.pal;
    const bool link = method_is_item(kind) || method_is_friend(kind);
    const std::wstring s = evo_method(kind, param);
    const float icon = link ? 17 : 0, w = text_width(s, 11.5f, DWRITE_FONT_WEIGHT_NORMAL) + icon + 2;
    const bool hov = link && g.hover == id;
    const D2D1_COLOR_F c = hov ? P.text : lit ? P.accent2 : P.text2;
    if (maxw > 0 && w > maxw) {
        const float lx = centre_on_x ? x - maxw / 2 : x;
        text(s, lx, y - 15, maxw, 36, 11.5f, c, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, true);
        if (link && id >= 0) hit({lx, y - 15, maxw, 36}, id);
        return;
    }
    if (centre_on_x) x -= w / 2;
    if (method_is_item(kind)) draw_symbol(kEditorItem, x, y + 3, 13, c);
    if (method_is_friend(kind)) text(L"♥", x, y, 14, 20, 10.5f, c);
    text(s, x + icon, y, w - icon + 4, 20, 11.5f, c);
    if (link && id >= 0) hit({x - 3, y - 2, w + 6, 22}, id);
}
// a method as a small chip sitting on a spine arrow, centred at (cx, cy): the item symbol, a heart for friendship, a
// sun for day and a moon for night (snatcherLeo's V1), then the words; lit = on the path to the shown Pokémon
void method_chip(int kind, int param, float cx, float cy, int id, bool lit) {
    const Palette& P = g.pal;
    const bool link = method_is_item(kind) || method_is_friend(kind);
    const std::wstring s = method_text(kind, param, true);
    const float icon = link ? 17 : 0, w = text_width(s, 11.5f, DWRITE_FONT_WEIGHT_NORMAL) + icon + 20, h = 22;
    const float x = cx - w / 2, y = cy - h / 2;
    const bool hov = link && g.hover == id;
    const D2D1_COLOR_F c = hov ? P.text : lit ? P.accent2 : P.text2;
    round_fill(x, y, w, h, h / 2, P.card);  // solid first: the arrow must not show through the tint
    if (lit) round_fill(x, y, w, h, h / 2, P.accent_bg);
    round_line(x, y, w, h, h / 2, lit || hov ? P.accent_line : P.line2);
    // the symbols: Segoe UI Symbol carries ☀ and ☾ where Inter has none (DirectWrite's fallback; Windows. Linux /
    // macOS: their own symbol fonts, or drawn shapes)
    const wchar_t* sym = kind == 2 ? L"☀" : kind == 3 ? L"☾" : kind == 1 ? L"♥" : nullptr;
    if (method_is_item(kind)) draw_symbol(kEditorItem, x + 9, y + 4, 13, c);
    else if (sym) text(sym, x + 8, y, 14, h, 11, c);
    text(s, x + 10 + icon, y, w - 10 - icon, h, 11.5f, c);
    if (link && id >= 0) hit({x, y, w, h}, id);
}
void draw_evo_card(const Rect& R, const Entry& e) {
    const Palette& P = g.pal;
    const auto fam = evolution_family(e.index);
    auto member = [&](int i) -> Entry* {
        auto it = g.by_index.find(fam[(size_t)i].index);
        return it == g.by_index.end() ? nullptr : &g.all[it->second];
    };
    const std::wstring line = fam.empty() || !member(0) ? L"" : tx("studio.evo.line", {{"name", member(0)->name}});
    card(R, tx("studio.card.evolution"), fam.size() > 1 ? tx("studio.evo.hint", {{"line", line}}) : L"");
    if (fam.size() <= 1) {
        text(tx("studio.evo.none"), R.x + 16, R.y + kCardHead, R.w - 32, 26, 13.5f, P.mute);
        return;
    }
    const float ty = R.y + kCardHead + 6;
    int cur = -1;
    for (size_t k = 0; k < fam.size(); ++k) if (fam[k].index == e.index) cur = (int)k;
    const EvoPlan pl = evo_plan(fam, cur, R.x + 16, ty, R.w - 32);
    // the spine and its join (unlit first, then the lit part over it)
    for (const EvoSeg& s : pl.segs)
        if (!s.lit) g.rt->DrawLine(D2D1::Point2F(s.x0, s.y0), D2D1::Point2F(s.x1, s.y1), brush(P.faint), 1.5f);
    for (const EvoSeg& s : pl.segs)
        if (s.lit) g.rt->DrawLine(D2D1::Point2F(s.x0, s.y0), D2D1::Point2F(s.x1, s.y1), brush(P.accent), 1.5f);
    // the arrows (under the cards): a thin line with a small head, straight or (the fork) a curve
    for (const EvoArrow& a : pl.arrows) {
        const D2D1_COLOR_F ac = a.lit ? P.accent : P.faint;
        float hx = a.x1, hy = a.y1;
        if (a.style != 1) {
            g.rt->DrawLine(D2D1::Point2F(a.x0, a.y0), D2D1::Point2F(a.x1, a.y1), brush(ac), 1.5f);
        } else if (g.d2d) {
            ID2D1PathGeometry* pg = nullptr;
            ID2D1GeometrySink* sk = nullptr;
            if (SUCCEEDED(g.d2d->CreatePathGeometry(&pg)) && SUCCEEDED(pg->Open(&sk))) {
                sk->BeginFigure(D2D1::Point2F(a.x0, a.y0), D2D1_FIGURE_BEGIN_HOLLOW);
                sk->AddBezier(D2D1::BezierSegment(D2D1::Point2F(a.x0, a.y0 + (a.y1 - a.y0) * 0.8f),
                                                  D2D1::Point2F(a.x0 + (a.x1 - a.x0) * 0.35f, a.y1), D2D1::Point2F(a.x1, a.y1)));
                sk->EndFigure(D2D1_FIGURE_END_OPEN);
                sk->Close();
                g.rt->DrawGeometry(pg, brush(ac), 1.5f);
            }
            if (sk) sk->Release();
            if (pg) pg->Release();
        }
        g.rt->DrawLine(D2D1::Point2F(hx - 6, hy - 5), D2D1::Point2F(hx, hy), brush(ac), 1.5f);
        g.rt->DrawLine(D2D1::Point2F(hx - 6, hy + 5), D2D1::Point2F(hx, hy), brush(ac), 1.5f);
        // the method: above a straight arrow (centred), beside the fork's curve, or a chip on a spine arrow
        const int i = a.i, kind = fam[(size_t)i].kind, param = fam[(size_t)i].param;
        if (a.style == 0) method_label(kind, param, (a.x0 + a.x1) / 2, a.y0 - 24, HIT_EVO_METHOD + i, true, a.lit, a.label_w);
        else if (a.style == 1) method_label(kind, param, a.x0 + 12, a.y0 + (a.y1 - a.y0) * 0.30f - 9, HIT_EVO_METHOD + i, false, a.lit);
        else method_chip(kind, param, (a.x0 + a.x1) / 2, a.y0, HIT_EVO_METHOD + i, a.lit);
    }
    // the cards: mug shot, name, No.; the shown one with the accent border
    for (const EvoNode& n : pl.nodes) {
        Entry* m = member(n.i);
        if (!m) continue;
        const bool cur = fam[(size_t)n.i].index == e.index, hov = g.hover == HIT_EVO + n.i;
        round_fill(n.r.x, n.r.y, n.r.w, n.r.h, 10, cur ? P.accent_bg : P.card2);
        round_line(n.r.x, n.r.y, n.r.w, n.r.h, 10, cur || hov ? P.accent_line : P.line);
        if (!m->bmp && !m->mug.empty()) m->bmp = make_bitmap(m->mug.data(), 42, 42);
        draw_mug(m->bmp, n.r.x + 8, n.r.y + 8, 42);
        text(m->name, n.r.x + 62, n.r.y + 9, n.r.w - 66, 20, 13.5f, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        {
            TextStyle st(true);  // the No. in same-width digits
            text(n.sub, n.r.x + 62, n.r.y + 30, n.r.w - 66, 18, 11.5f, P.mute);
        }
        if (!cur) hit({n.r.x, n.r.y, n.r.w, n.r.h}, HIT_EVO + n.i);
    }
}

// the Looks tab: every picture the disc has for the species, each the default (Colosseum's own, locked)
// The Looks tab (v3): two cards side by side, mug shots and PC pictures as large thumbnails (96 DIPs), each card
// marked "Default: Colosseum 🔒" (the pictures are the game's own; replacing them arrives with mods); the model's
// counts under them
void draw_looks(const Layout& L, float top = -1) {
    const Palette& P = g.pal;
    Rect C = L.content;
    if (top >= 0) { C.h -= top - C.y; C.y = top; }  // (the Styles tab draws these pictures under its own cards)
    const bool two = C.w >= 640;
    const float cw = two ? std::floor((C.w - kColGap) / 2) : C.w;
    auto section = [&](const char* key, const std::vector<Pic>& v, float x0, float y0) {
        const float cell = 96, per = std::max(1.0f, std::floor((cw - 32 + 14) / (cell + 14)));
        const float rows = std::max(1.0f, std::ceil(v.size() / per));
        const float h = kCardHead + rows * (cell + 46) + 6;  // (each row: the picture, its name, its size)
        card({x0, y0, cw, h}, tx(key), tx("studio.looks.default"));
        for (size_t k = 0; k < v.size(); ++k) {
            const Pic& p = v[k];
            const float x = x0 + 16 + (float)(k % (size_t)per) * (cell + 14), y = y0 + kCardHead + (float)(k / (size_t)per) * (cell + 46);
            round_fill(x, y, cell, cell, 12, P.card2);
            // here the picture is the subject: the exact square as on the disc, never rounded, each of its pixels an
            // equal block of whole screen pixels, placed on whole screen pixels, its size noted under it (snatcherLeo,
            // 2026-11-19, through C: the split by role, rounded where a mug shot is an icon)
            const float s = std::max(1.0f, std::floor(std::min((cell - 16) / p.w, (cell - 16) / p.h) * g.scale)) / g.scale;
            const float px0 = std::round((x + (cell - p.w * s) / 2) * g.scale) / g.scale;
            const float py0 = std::round((y + (cell - p.h * s) / 2) * g.scale) / g.scale;
            draw_bitmap(p.bmp, px0, py0, p.w * s, p.h * s);
            text(p.label, x - 10, y + cell + 4, cell + 20, 20, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            text(tx("studio.looks.size", {{"w", std::to_wstring(p.w)}, {"h", std::to_wstring(p.h)}}), x - 10, y + cell + 22, cell + 20, 16,
                 10.5f, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        return h;
    };
    const float h1 = section("studio.inspector.mug_shots", g.mugs, C.x, C.y);
    const float h2 = section("studio.inspector.pc_pictures", g.pcs, two ? C.x + cw + kColGap : C.x, two ? C.y : C.y + h1 + kCardGap);
    const float y = (two ? C.y + std::max(h1, h2) : C.y + h1 + kCardGap + h2) + 18;
    text(tx("studio.inspector.model"), C.x, y, C.w, 22, 10.5f, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    text(g.model_info, C.x, y + 24, C.w, 24, 13.5f, P.text);
}

// ---- the Styles tab (2026-11-22; snatcherLeo: "I like the visual", mockup in the Studio chat; the Looks tab renamed) ----
// Left, the Pokémon's styles (picking one views it: the 3D view and every model card show it); right, what the picked
// style uses, part by part; under both, the pictures the Looks tab showed (now the style's picture parts). Faded: what
// editing brings (each part "from" another style, your own files or a mod). Choosing styles happens only here (snatcherLeo);
// the Overview's model card links here ("Change style… →")
// a small rounded label (a style's source or mark)
float style_chip(const std::wstring& s, float x, float y, const D2D1_COLOR_F& bg, const D2D1_COLOR_F& fg) {
    const float w = text_width(s, 11, DWRITE_FONT_WEIGHT_NORMAL) + 12;
    round_fill(x, y, w, 18, 5, bg);
    text(s, x, y, w, 18, 11, fg, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    return w;
}
void draw_styles(const Layout& L) {
    const Palette& P = g.pal;
    const Rect& C = L.content;
    const Entry& e = g.all[(size_t)g.selected];
    const int sp = e.index, n = style_count(sp), cur = style_now(sp), live = style_in_game(sp);
    const float lw = std::min(340.0f, std::floor(C.w * 0.36f)), rx = C.x + lw + kColGap, rw = C.w - lw - kColGap;
    // left: the styles
    const float row = 60;
    const bool jynx_no_disc = sp == kStyleJynx && n == 1;  // (the Japanese disc isn't linked: say how to add it)
    const float lh = kCardHead + n * (row + 6) + (jynx_no_disc ? 58 : 6);
    card({C.x, C.y, lw, lh}, tx("studio.styles.title"), std::to_wstring(n));
    for (int i = 0; i < n; ++i) {
        const float y = C.y + kCardHead + i * (row + 6), x = C.x + 12, w = lw - 24;
        const bool on = i == cur, hov = g.hover == HIT_STYLE + i;
        if (on) { round_fill(x, y, w, row, 8, P.accent_bg); round_line(x, y, w, row, 8, P.accent_line); }
        else if (hov) round_fill(x, y, w, row, 8, P.hover);
        if (!g.mugs.empty() && g.mugs[0].bmp) draw_mug(g.mugs[0].bmp, x + 8, y + 8, 44);
        text(tx(i == 0 ? "studio.styles.game" : "studio.styles.japanese"), x + 62, y + 6, w - 70, 24, 13.5f, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        float cx = x + 62;
        cx += 6 + style_chip(tx(i == 0 ? "studio.styles.src_disc" : "studio.styles.src_jp"), cx, y + 34, P.card2, P.text2);
        if (on) cx += 6 + style_chip(tx("studio.styles.viewing"), cx, y + 34, P.accent_bg, P.accent2);
        if (i == live && n > 1) style_chip(tx("studio.styles.in_game"), cx, y + 34, alpha(P.accent, 0.25f), P.text);
        hit({x, y, w, row}, HIT_STYLE + i);
    }
    if (jynx_no_disc)
        text(tx("studio.styles.no_jp"), C.x + 16, C.y + kCardHead + row + 6, lw - 32, 50, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_LEADING, true);
    // right: what the picked style uses
    static const char* kParts[] = {"studio.styles.part.model", "studio.styles.part.shiny", "studio.styles.part.mug_shots",
                                   "studio.styles.part.pc_pictures", "studio.styles.part.cry", "studio.styles.part.moves"};
    const float prow = 34, rh = kCardHead + 6 * prow + 40;
    card({rx, C.y, rw, rh}, tx("studio.styles.parts_title"));
    for (int k = 0; k < 6; ++k) {
        const float y = C.y + kCardHead + k * prow;
        if (k > 0) fill(rx + 16, y, rw - 32, 1, P.line);
        text(tx(kParts[k]), rx + 16, y, 130, prow, 13, P.text2);
        const char* what = k == 0 ? (cur == 1 ? "studio.styles.model_jp" : "studio.styles.model_game")
                         : (k == 2 || k == 3) ? "studio.styles.pictures_below" : "studio.styles.game_part";
        text(tx(what), rx + 150, y, rw - 150 - 90, prow, 13, P.text);
        text(tx("studio.styles.from"), rx + rw - 86, y, 70, prow, 12, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    text(tx("studio.styles.later_note"), rx + 16, C.y + kCardHead + 6 * prow + 6, rw - 32, 30, 12, P.mute);
    // under both: the pictures (the style's picture parts)
    draw_looks(L, C.y + std::max(lh, rh) + kCardGap);
}

// ---- the Moves tab (version 1, 2026-11-19; studio/moves-tab-plan.md, mockup 25) ----
// Solid: the learnset (level-up, TM / HM, egg) with type, kind, power, accuracy and PP; the "own version" tag; Play
// with pause and frame stepping; Studio view / Plain stage; the general-or-own line with its link to the Move editor.
// Faded (later versions, explained on hover): Edit learnset, Test in game, the frame rate, Particles, and Make it your
// own. Version 1 plays only "This move" (the plan's section 6), so the mockup's move-set switch isn't drawn.

// a faded control: what a later version brings (the same look as the header's Create a mod: a dimmed outline and
// faint words; hover explains it). Returns its width.
float faded_button(const std::wstring& s, float x, float y, int id) {
    const Palette& P = g.pal;
    const float w = text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 24;
    round_line(x, y, w, 30, 7, alpha(P.line2, 0.6f));
    text(s, x, y, w, 30, 12.5f, P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    hit({x, y, w, 30}, id);
    return w;
}
// The preview rate (release step 3, 2026-11-19; the project plan's "One frame-rate choice for every preview in Studio",
// snatcherLeo: "the game's rate, or 120 Hz and higher, smoother at higher rates like everything else"): one setting for
// every 3D view, the game's own rate (30 frames a second, exactly as the game) or 60 / 120 / 144 / 165 / 240 Hz with
// the pose smoothed between the game's frames (OrrePort's Settings row's list). A button showing the current rate; a
// click opens the choices
constexpr int kRates[] = {0, 60, 120, 144, 165, 240};
// *Changed 2026-11-22* (snatcherLeo: "we need to make the size of the HZ the same size ... No matter what a user selects in
// there, it can't change the spacing of other stuff next to it. We could have original just say "30hz""): the button
// says "30 Hz" for the game's own rate (the menu still names it, studio.rate.menu_game) and is as wide as its widest
// choice, whatever is picked
std::wstring rate_label(int hz) { return tx("studio.rate.hz", {{"n", std::to_wstring(hz <= 0 ? 30 : hz)}}); }
float rate_button_width() {
    float w = 0;
    for (int hz : {0, 60, 120, 144, 165, 240}) w = std::max(w, text_width(rate_label(hz) + L"  ▾", 12.5f, DWRITE_FONT_WEIGHT_NORMAL));
    return w + 24;
}
float rate_button(float x, float y) {
    const Palette& P = g.pal;
    const std::wstring s = rate_label(g.preview_rate) + L"  ▾";
    const float w = rate_button_width();
    const bool hov = g.hover == HIT_RATE;
    round_fill(x, y, w, 30, 7, hov ? P.hover : P.card);
    round_line(x, y, w, 30, 7, P.line2);
    text(s, x, y, w, 30, 12.5f, hov ? P.text : P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    hit({x, y, w, 30}, HIT_RATE);
    return w;
}
float segmented_width(const std::vector<std::pair<std::wstring, int>>& opts) {
    float w = 4;
    for (auto& [s, id] : opts) w += text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 18 + 2;
    return w;
}
// the play / pause and step symbols, drawn (not font glyphs, so every language's font draws them the same)
void media_symbol(int kind, float cx, float cy, float s, const D2D1_COLOR_F& c) {
    // kind 0 play (a triangle), 1 pause (two bars), 2 step back (a bar and a left triangle), 3 step forward
    auto tri = [&](float x0, float x1) {
        ID2D1PathGeometry* pg = nullptr;
        ID2D1GeometrySink* sk = nullptr;
        if (SUCCEEDED(g.d2d->CreatePathGeometry(&pg)) && SUCCEEDED(pg->Open(&sk))) {
            sk->BeginFigure(D2D1::Point2F(x0, cy - s / 2), D2D1_FIGURE_BEGIN_FILLED);
            sk->AddLine(D2D1::Point2F(x1, cy));
            sk->AddLine(D2D1::Point2F(x0, cy + s / 2));
            sk->EndFigure(D2D1_FIGURE_END_CLOSED);
            sk->Close();
            g.rt->FillGeometry(pg, brush(c));
        }
        if (sk) sk->Release();
        if (pg) pg->Release();
    };
    if (kind == 0) tri(cx - s * 0.4f, cx + s * 0.5f);
    else if (kind == 1) { fill(cx - s * 0.4f, cy - s / 2, s * 0.28f, s, c); fill(cx + s * 0.12f, cy - s / 2, s * 0.28f, s, c); }
    else if (kind == 2) { fill(cx - s * 0.5f, cy - s / 2, s * 0.18f, s, c); tri(cx + s * 0.45f, cx - s * 0.25f); }
    else { fill(cx + s * 0.32f, cy - s / 2, s * 0.18f, s, c); tri(cx - s * 0.45f, cx + s * 0.25f); }
}
// what the learnset's first column says: the level, the machine ("TM06", "HM01" in the language's own letters)
std::wstring learn_how(const LearnRow& r) {
    if (r.group == 0) return std::to_wstring(r.how);
    if (r.group == 1) {
        wchar_t n[8];
        std::swprintf(n, 8, L"%02d", r.how < 50 ? r.how + 1 : r.how - 49);
        return tx(r.how < 50 ? "studio.moves.tm" : "studio.moves.hm", {{"n", n}});
    }
    return L"—";
}
// Gen 3's kind of move: status when it has no power, else by its type (the type table's category byte: data, so a mod
// that moves a type moves its moves; battle-types.md, verified)
std::wstring move_kind(const OrreMove& mv) {
    if (mv.power == 0) return tx("studio.moves.kind_status");
    const int c = orre_type_category(g.disc, mv.type);
    return tx(c == 1 ? "studio.moves.kind_physical" : c == 2 ? "studio.moves.kind_special" : "studio.moves.kind_status");
}
// the game's summary screen shows "---" for a power under 2 (the moves whose power is worked out in battle: Return,
// Hidden Power...) and for an accuracy of 0 (never misses), as Gen 3's does (likely: pokeemerald's PrintMoveDetails)
std::wstring move_number(int v, int below) { return v < below ? L"—" : std::to_wstring(v); }
float para(const std::wstring& s, float x, float y, float w, float size, const D2D1_COLOR_F& c,
           DWRITE_FONT_WEIGHT weight);

// The Animations tab's left side (mockup 28): the model's animations by name, a 3D tool's list, scrolling with the page
// when long; then the Used by card: the moves of the learnset that play the chosen animation, side by side, three rows
// shown and the rest scrolled inside the card; a move opens in the Move editor
void draw_anim_cards(const MovesGeo& G, const Entry& e) {
    const Palette& P = g.pal;
    const Rect& C = G.learn;
    const int n = g.viewer.animation_count();
    card(C, tx("studio.anim.title", {{"n", std::to_wstring(n)}}));
    const float cx = C.x + 16, iw = C.w - 32;
    const float hy = C.y + 44;
    const float c_len = std::max(90.0f, text_width(tx("studio.moves.col_frames"), 11, DWRITE_FONT_WEIGHT_SEMI_BOLD) * 1.15f + 16);
    {
        TextStyle st(false, 0.12f);
        text(tx("studio.anim.col_name"), cx, hy, iw - c_len, kLHead, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(tx("studio.moves.col_frames"), cx + iw - c_len, hy, c_len, kLHead, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD,
             DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    fill(cx - 8, hy + kLHead - 1, iw + 16, 1, P.line);
    float ry = hy + kLHead;
    if (n <= 0) text(tx("studio.model.loading"), cx, ry, iw, kLRow, 13, P.mute);
    for (int a : anim_order()) {
        bool unused = false, noted = false;
        const std::wstring name = anim_name(a, &unused, &noted);
        const bool sel = a == g.anim_sel, hov = g.hover == HIT_AROW + a;
        if (sel) {
            fill(cx - 8, ry, iw + 16, kLRow, P.accent_bg);
            fill(cx - 8, ry, 2, kLRow, P.accent);
        } else if (hov) {
            fill(cx - 8, ry, iw + 16, kLRow, P.hover);
        }
        fill(cx - 8, ry + kLRow - 1, iw + 16, 1, P.line);
        text(name, cx, ry, iw - c_len - 8, kLRow, 13, unused ? P.faint : P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        if (unused || noted) {
            // (nothing in the game plays it: no move, no reaction, not the eyes'; "unused in the game" when what it is
            // is known, "not played in the game?" when it isn't)
            const std::wstring t = tx(noted ? "studio.anim.unused_known" : "studio.anim.unused");
            const float nw = text_width(name, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD), tw = text_width(t, 11, DWRITE_FONT_WEIGHT_NORMAL) + 12;
            if (nw + 10 + tw < iw - c_len) {
                round_fill(cx + nw + 10, ry + (kLRow - 20) / 2, tw, 20, 5, P.chip);
                text(t, cx + nw + 10, ry + (kLRow - 20) / 2, tw, 20, 11, P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
        {
            TextStyle st(true);
            text(tx("studio.anim.frames", {{"n", std::to_wstring(g.viewer.animation_frames(a))}}), cx + iw - c_len, ry, c_len, kLRow,
                 12.5f, P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
        hit({cx - 8, ry, iw + 16, kLRow}, HIT_AROW + a);
        ry += kLRow;
    }
    // (adding animations comes with modding: faded, no click)
    text(tx("studio.anim.add"), cx, ry, iw, kLRow, 13, P.faint);

    // ---- Used by ----
    const Rect& U = G.used;
    if (U.h <= 0 || n <= 0) return;
    const std::wstring chosen = anim_name(g.anim_sel);
    int rows = 0;
    const std::vector<UsedChip> chips = used_chips(U.w - 32, &rows);
    card(U, tx("studio.anim.used_title"),
         chips.empty() ? chosen : tx("studio.anim.used_count", {{"anim", chosen}, {"n", std::to_wstring(chips.size())}, {"name", e.name}}));
    const float bx = U.x + 16, by = U.y + kCardHead, bw = U.w - 32;
    const float view = std::min<float>((float)std::max(rows, 1), kUsedRows) * (kChipH + kChipGap);
    const float total = rows * (kChipH + kChipGap);
    g.used_scroll = std::clamp(g.used_scroll, 0.0f, std::max(0.0f, total - view));
    if (chips.empty()) {
        text(tx("studio.anim.used_none", {{"name", e.name}}), bx, by, bw, 26, 13, P.mute);
    } else {
        g.rt->PushAxisAlignedClip(D2D1::RectF(bx - 2, by - 2, bx + bw + 2, by + view), D2D1_ANTIALIAS_MODE_ALIASED);
        for (const UsedChip& c : chips) {
            const LearnRow& r = g.learn[(size_t)c.row];
            const float x = bx + c.x, y = by + c.y - g.used_scroll;
            if (y + kChipH < by || y > by + view) continue;
            const bool hov = g.hover == HIT_USED + c.row;
            if (hov) round_fill(x, y, c.w, kChipH, kChipH / 2, P.hover);
            round_line(x, y, c.w, kChipH, kChipH / 2, hov ? P.accent_line : P.line2);
            const float nw = text_width(r.name, 12.5f, DWRITE_FONT_WEIGHT_NORMAL);
            text(r.name, x + 12, y, c.w - 16, kChipH, 12.5f, P.accent2);
            text(chip_how(r), x + 12 + nw + 6, y, std::max(4.0f, c.w - nw - 22), kChipH, 11, P.mute);
            const float vy0 = std::max(y, by), vy1 = std::min(y + kChipH, by + view);
            if (vy1 > vy0) hit({x, vy0, c.w, vy1 - vy0}, HIT_USED + c.row);
        }
        g.rt->PopAxisAlignedClip();
        if (total > view) {  // (a thin scroll bar at the right: more rows than fit)
            const float bar = std::max(16.0f, view * view / total), y = by + (view - bar) * (g.used_scroll / (total - view));
            round_fill(U.x + U.w - 8, y, 3, bar, 1.5f, alpha(P.mute, 0.45f));
        }
    }
    // what the animation is for, when the game plays it outside moves (the idle, a hit, the faint): one plain line
    const int use = anim_first_use(g.anim_sel);
    const char* role = use == 0 ? "studio.anim.role.idle" : use == 8 ? "studio.anim.role.hit" : use == 10 ? "studio.anim.role.faint"
                       : (use == 11 || use == 16) ? "studio.anim.role.idle_more" : nullptr;
    if (role) text(tx(role, {{"name", e.name}}), bx, U.y + U.h - 34, bw, 26, 12, P.mute);
}

void draw_moves(const Layout& L, const Entry& e) {
    const Palette& P = g.pal;
    const MovesGeo& G = L.mv;
    // ---- the learnset (on the Animations tab: the model's animations and the Used by card) ----
    if (g.tab == kTabAnimations) draw_anim_cards(G, e);
    else {
        const Rect& C = G.learn;
        card(C, tx("studio.moves.learnset"), tx("studio.era.gen3_colosseum"));
        const float cx = C.x + 16, iw = C.w - 32;
        // (Reactions left on 2026-11-22: they're on the Animations tab now)
        const std::vector<std::pair<std::wstring, int>> groups = {{tx("studio.moves.group_level"), HIT_LGROUP},
                                                                   {tx("studio.moves.group_tm"), HIT_LGROUP + 1},
                                                                   {tx("studio.moves.group_egg"), HIT_LGROUP + 2}};
        const float gy = C.y + 44;
        const float gw = segmented_width(groups);
        segmented(cx + gw, gy, groups, HIT_LGROUP + g.learn_group);
        // (move tutors: none in Colosseum; XD's arrive when XD is a source game, so the Tutor button stays hidden)
        const std::wstring edit = tx("studio.moves.edit");
        const float ew = text_width(edit, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 24;
        if (cx + gw + 12 + ew <= cx + iw) faded_button(edit, cx + iw - ew, gy, HIT_LATER + kLaterEdit);
        if (g.learn_group == 3) {
            // Reactions: the animation's name, its slot number and its length; a slot with no animation of its own is
            // greyed with why ("uses the idle" for an empty slot the game fills with the idle, "not in this model" for a
            // shorter slot table), not shown as a copy of the idle (2026-11-19)
            const float hy = gy + 40;
            const float c_slot = std::max(56.0f, text_width(tx("studio.moves.col_slot"), 11, DWRITE_FONT_WEIGHT_SEMI_BOLD) * 1.15f + 16);
            float c_len = std::max(90.0f, text_width(tx("studio.moves.col_frames"), 11, DWRITE_FONT_WEIGHT_SEMI_BOLD) * 1.15f + 16);
            for (const char* k : {"studio.moves.react_idle", "studio.moves.react_empty", "studio.moves.react_missing"})
                c_len = std::max(c_len, text_width(tx(k), 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 12);
            const float c_name = std::max(80.0f, iw - c_slot - c_len), x_slot = cx + c_name, x_len = x_slot + c_slot;
            {
                TextStyle st(false, 0.12f);
                const auto hw = DWRITE_FONT_WEIGHT_SEMI_BOLD;
                text(tx("studio.moves.col_anim"), cx, hy, c_name, kLHead, 11, P.mute, hw);
                text(tx("studio.moves.col_slot"), x_slot, hy, c_slot - 8, kLHead, 11, P.mute, hw, DWRITE_TEXT_ALIGNMENT_TRAILING);
                text(tx("studio.moves.col_frames"), x_len, hy, c_len, kLHead, 11, P.mute, hw, DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            fill(cx - 8, hy + kLHead - 1, iw + 16, 1, P.line);
            float ry = hy + kLHead;
            for (size_t i = 0; i < g.learn.size(); ++i) {
                const LearnRow& r = g.learn[i];
                if (r.group != 3) continue;
                const int own = g.viewer.slot_own(r.slot);
                const bool has = own >= 0, sel = r.move == g.learn_move, hov = g.hover == HIT_LROW + (int)i;
                if (sel) {
                    fill(cx - 8, ry, iw + 16, kLRow, P.accent_bg);
                    fill(cx - 8, ry, 2, kLRow, P.accent);
                } else if (hov) {
                    fill(cx - 8, ry, iw + 16, kLRow, P.hover);
                }
                fill(cx - 8, ry + kLRow - 1, iw + 16, 1, P.line);
                text(r.name, cx, ry, c_name - 8, kLRow, 13, has ? P.text : P.faint, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                TextStyle st(true);
                text(std::to_wstring(r.slot), x_slot, ry, c_slot - 8, kLRow, 12.5f, has ? P.text2 : P.faint, DWRITE_FONT_WEIGHT_NORMAL,
                     DWRITE_TEXT_ALIGNMENT_TRAILING);
                const std::wstring len = has ? std::to_wstring(g.viewer.animation_frames(own))
                                             : tx(own == -2 ? "studio.moves.react_missing"
                                                  : r.slot == 11 ? "studio.moves.react_idle"  // (the game's fallback 11 -> 0, verified)
                                                                 : "studio.moves.react_empty");  // (no known fallback for 9 / 16)
                text(len, x_len, ry, c_len, kLRow, has ? 13 : 12.5f, has ? P.text2 : P.faint, DWRITE_FONT_WEIGHT_NORMAL,
                     DWRITE_TEXT_ALIGNMENT_TRAILING);
                hit({cx - 8, ry, iw + 16, kLRow}, HIT_LROW + (int)i);
                ry += kLRow;
            }
        } else {
        // the columns; Kind drops out on a narrow card (the preview's facts still say it)
        // each column at least as wide as its heading (small capitals, 0.12 em apart: about 15 % wider than measured)
        // and its widest entry, so longer translations ("Puissance", "わざマシン06") never run into the next column
        auto head_w = [](const char* key, float minw) {
            return std::max(minw, text_width(tx(key), 11, DWRITE_FONT_WEIGHT_SEMI_BOLD) * 1.15f + 10);
        };
        float c_lv = head_w(g.learn_group == 1 ? "studio.moves.col_tm" : "studio.moves.col_level", 48);
        for (const LearnRow& lr : g.learn)
            if (lr.group == g.learn_group) c_lv = std::max(c_lv, text_width(learn_how(lr), 13, DWRITE_FONT_WEIGHT_NORMAL) + 14);
        float c_kind = 0;
        for (const char* k : {"studio.moves.kind_physical", "studio.moves.kind_special", "studio.moves.kind_status"})
            c_kind = std::max(c_kind, text_width(tx(k), 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 12);
        c_kind = std::max(c_kind, head_w("studio.moves.col_kind", 70));
        const float c_type = head_w("studio.moves.col_type", 96), c_pow = head_w("studio.moves.col_power", 52),
                    c_acc = head_w("studio.moves.col_acc", 48), c_pp = head_w("studio.moves.col_pp", 38);
        // the Animation column (2026-11-22, mockup 29; snatcherLeo: the Moves tab "would somehow show in an organized and clear
        // way which of those animations is being used for a specific attack"): the animation's name, as the Animations
        // tab names it, as wide as its widest; it stays before Kind, which drops out first on a narrow card
        float c_anim = head_w("studio.moves.col_animation", 90);
        for (const LearnRow& lr : g.learn)
            if (lr.group == g.learn_group) {
                const int a = move_anim(lr);
                if (a >= 0) c_anim = std::max(c_anim, text_width(anim_name(a), 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 14);
            }
        bool anim_col = iw - c_lv - c_type - c_anim - c_pow - c_acc - c_pp >= 120;
        if (!anim_col) c_anim = 0;
        const bool kind_col = iw - c_lv - c_type - c_anim - c_kind - c_pow - c_acc - c_pp >= 130;
        if (!kind_col) c_kind = 0;
        const float c_move = std::max(60.0f, iw - c_lv - c_type - c_anim - c_kind - c_pow - c_acc - c_pp);
        const float x_move = cx + c_lv, x_type = x_move + c_move, x_anim = x_type + c_type, x_kind = x_anim + c_anim,
                    x_pow = x_kind + c_kind, x_acc = x_pow + c_pow, x_pp = x_acc + c_acc;
        const float hy = gy + 40;
        {
            TextStyle st(false, 0.12f);
            const D2D1_COLOR_F hc = P.mute;
            const float hs = 11;
            const auto hw = DWRITE_FONT_WEIGHT_SEMI_BOLD;
            if (g.learn_group < 2)
                text(tx(g.learn_group == 0 ? "studio.moves.col_level" : "studio.moves.col_tm"), cx, hy, c_lv, kLHead, hs, hc, hw);
            text(tx("studio.moves.col_move"), x_move, hy, c_move, kLHead, hs, hc, hw);
            text(tx("studio.moves.col_type"), x_type, hy, c_type, kLHead, hs, hc, hw);
            if (anim_col) text(tx("studio.moves.col_animation"), x_anim, hy, c_anim, kLHead, hs, hc, hw);
            if (kind_col) text(tx("studio.moves.col_kind"), x_kind, hy, c_kind, kLHead, hs, hc, hw);
            text(tx("studio.moves.col_power"), x_pow, hy, c_pow - 8, kLHead, hs, hc, hw, DWRITE_TEXT_ALIGNMENT_TRAILING);
            text(tx("studio.moves.col_acc"), x_acc, hy, c_acc - 8, kLHead, hs, hc, hw, DWRITE_TEXT_ALIGNMENT_TRAILING);
            text(tx("studio.moves.col_pp"), x_pp, hy, c_pp, kLHead, hs, hc, hw, DWRITE_TEXT_ALIGNMENT_TRAILING);
        }
        fill(cx - 8, hy + kLHead - 1, iw + 16, 1, P.line);
        float ry = hy + kLHead;
        int shown = 0;
        for (size_t i = 0; i < g.learn.size(); ++i) {
            const LearnRow& r = g.learn[i];
            if (r.group != g.learn_group) continue;
            ++shown;
            const bool sel = r.move == g.learn_move, hov = g.hover == HIT_LROW + (int)i;
            if (sel) {
                fill(cx - 8, ry, iw + 16, kLRow, P.accent_bg);
                fill(cx - 8, ry, 2, kLRow, P.accent);
            } else if (hov) {
                fill(cx - 8, ry, iw + 16, kLRow, P.hover);
            }
            fill(cx - 8, ry + kLRow - 1, iw + 16, 1, P.line);
            {
                TextStyle st(true);  // same-width digits
                text(learn_how(r), cx, ry, c_lv - 6, kLRow, 13, P.text2);
            }
            // the name, and the "own version" tag after it when this Pokémon has its own
            const float nw = text_width(r.name, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            text(r.name, x_move, ry, c_move - 8, kLRow, 13, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            // the "own version" tag: hidden for now (snatcherLeo, 2026-11-19: "Hide for now"; it returns with modding). The
            // data stays: the preview's note below still says whether this Pokémon has its own version
            if (r.own && kShowOwnTag) {
                const std::wstring t = tx("studio.moves.own");
                const float tw = text_width(t, 11, DWRITE_FONT_WEIGHT_NORMAL) + 14;
                if (nw + 8 + tw <= c_move - 6) {
                    round_line(x_move + nw + 8, ry + 8, tw, 17, 8.5f, P.accent_line);
                    text(t, x_move + nw + 8, ry + 8, tw, 17, 11, P.accent2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }
            g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x_type + 4.5f, ry + kLRow / 2), 4.5f, 4.5f), brush(type_dot(r.mv.type)));
            text(type_name(r.mv.type), x_type + 15, ry, c_type - 17, kLRow, 12.5f, P.text2);
            if (anim_col) {
                const int a = move_anim(r);
                text(a >= 0 ? anim_name(a) : L"—", x_anim, ry, c_anim - 6, kLRow, 12.5f, a >= 0 ? P.text2 : P.faint);
            }
            if (kind_col) text(move_kind(r.mv), x_kind, ry, c_kind - 4, kLRow, 12.5f, P.text2);
            {
                TextStyle st(true);
                text(move_number(r.mv.power, 2), x_pow, ry, c_pow - 8, kLRow, 13, P.text2, DWRITE_FONT_WEIGHT_NORMAL,
                     DWRITE_TEXT_ALIGNMENT_TRAILING);
                text(move_number(r.mv.accuracy, 1), x_acc, ry, c_acc - 8, kLRow, 13, P.text2, DWRITE_FONT_WEIGHT_NORMAL,
                     DWRITE_TEXT_ALIGNMENT_TRAILING);
                text(std::to_wstring(r.mv.pp), x_pp, ry, c_pp, kLRow, 13, P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            hit({cx - 8, ry, iw + 16, kLRow}, HIT_LROW + (int)i);
            ry += kLRow;
        }
        if (!shown)
            text(tx(g.learn_group == 2 ? "studio.moves.none_egg" : g.learn_group == 1 ? "studio.moves.none_tm" : "studio.moves.none_level",
                    {{"name", e.name}}),
                 cx, ry, iw, kLRow, 13, P.mute);
        }  // (the moves' table)
    }
    // ---- the preview ----
    const Rect& R = G.prev;
    const LearnRow* r = chosen_row();
    card(R, tx("studio.moves.preview"), r ? tx("studio.moves.preview_on", {{"move", r->name}, {"name", e.name}}) : L"");
    const float px = R.x + 16, pw = R.w - 32;
    if (G.forms_h > 0) {
        // the form picker: the Forms card's thumbnails, small, sharing its choice (g.form); the move plays on the picked
        // form's model, and its "own version" tags follow that form (Castform's weather forms)
        const bool named = g.forms.size() <= 8;
        float fx = px, fy = G.forms_y;
        for (size_t i = 0; i < g.forms.size(); ++i) {
            const auto& f = g.forms[i];
            if (fx > px && fx + kPickTile > px + pw) { fx = px; fy += kPickTile + (named ? 18 : 0) + kPickGap; }
            const bool on = (int)i == g.form, hov = g.hover == HIT_FORM + (int)i && f.available && !on;
            if (on) round_line(fx, fy, kPickTile, kPickTile, 8, P.accent, 1.5f);
            else if (hov) round_fill(fx, fy, kPickTile, kPickTile, 8, P.hover);
            else round_line(fx, fy, kPickTile, kPickTile, 8, P.line);
            if (f.bmp) {
                if (f.available) draw_mug(f.bmp, fx + 3, fy + 3, kPickTile - 6);
                else fill_rounded_picture(f.bmp, fx + 3, fy + 3, kPickTile - 6, mug_radius(kPickTile - 6), 0.3f);
            }
            if (named)
                text(f.label, fx - 14, fy + kPickTile + 2, kPickTile + 28, 16, 10.5f, on ? P.text : f.available ? P.mute : P.faint,
                     on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            if (f.available && !on) hit({fx, fy, kPickTile, kPickTile + (named ? 16 : 0)}, HIT_FORM + (int)i);
            fx += kPickTile + kPickGap;
        }
    }
    {
        // Studio view / Plain stage; Test in game and the frame rate faded beside them (left out when they don't fit)
        const std::vector<std::pair<std::wstring, int>> views = {{tx("studio.moves.view_studio"), HIT_VIEW_STUDIO},
                                                                  {tx("studio.moves.view_plain"), HIT_VIEW_PLAIN},
                                                                  {tx("studio.moves.view_arena"), HIT_VIEW_ARENA}};
        const float vw = segmented_width(views);
        segmented(px + vw, G.view_y, views, shown_stage() == 2 ? HIT_VIEW_ARENA : shown_stage() == 1 ? HIT_VIEW_PLAIN : HIT_VIEW_STUDIO);
        float x = px + vw + 10;
        // Normal / Shiny: the same choice as the Overview's model card, so moves, reactions and the idle play on the
        // shiny model too (snatcherLeo, 2026-11-19: "allow the user to view the shiny doing these animations")
        const std::vector<std::pair<std::wstring, int>> pics = {{tx("studio.pic.normal"), HIT_NORMAL}, {tx("studio.pic.shiny"), HIT_SHINY}};
        const float nsw = segmented_width(pics);
        if (x + nsw <= px + pw) {
            segmented(x + nsw, G.view_y, pics, g.shiny ? HIT_SHINY : HIT_NORMAL);
            x += nsw + 10;
        }
        const std::wstring test = tx("studio.moves.test");
        // the Animations tab's Reset view (snatcherLeo, 2026-11-22: "we can include a "reset" button in case users deviate,
        // just like in Overview"), left of the rate button: the Studio camera there can be turned and zoomed
        const float resw = g.tab == kTabAnimations ? reset_button_width(false) + 8 : 0;
        const float tw = text_width(test, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 24, rw = rate_button_width();
        if (x + tw + 8 + resw + rw <= px + pw) { faded_button(test, x, G.view_y, HIT_LATER + kLaterTest); x += tw + 8; }
        if (x + resw + rw <= px + pw) {
            rate_button(px + pw - rw, G.view_y);
            if (resw > 0) reset_button(px + pw - rw - resw, G.view_y, false);
        }
    }
    stage_backdrop(L.stage, shown_stage() != 0);
    if (!g.viewer.ok())
        text(tx("studio.model.none", {{"reason", widen(g.viewer.error())}}), L.stage.x + 20, L.stage.y, L.stage.w - 40, L.stage.h, 13,
             P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, true);
    const int anim = chosen_animation();
    // the timeline's frames (the animation's, plus the faint's held ending) and the animation's own
    const int frames = anim >= 0 ? g.viewer.timeline_frames(anim) : 0;
    const int anim_len = anim >= 0 ? g.viewer.animation_frames(anim) : 0;
    // the timeline (snatcherLeo, 2026-11-19: "a "bar" or timeline represent how long the animation is, so users can drag back
    // and forth ... like in Blender when you are viewing an animation"): the animation's frames as a ruler, numbered
    // from 1 like the frame counter, with the playhead where the animation is; a click or a drag holds that frame (Play
    // carries on from it). Same bar for any length: the numbers thin out to stay readable
    {
        const float y = G.tl_y, h = kTimelineH;
        round_fill(px, y, pw, h, 7, P.card2);
        round_line(px, y, pw, h, 7, P.line);
        const float x0 = px + 14, x1 = px + pw - 14;
        g.tl_x0 = x0;
        g.tl_x1 = x1;
        g.tl_frames = frames;
        if (frames > 1) {
            const float per = (x1 - x0) / (float)(frames - 1);
            int step = 500;
            for (int s : {1, 2, 5, 10, 20, 50, 100, 200, 500})
                if (s * per >= 40) { step = s; break; }
            const int minor = step >= 10 ? step / 5 : (step >= 2 ? 1 : 0);
            // a held ending (the faint lying on the ground) shaded, so the animation's own end shows
            if (anim_len > 0 && frames > anim_len) {
                const float xe = std::floor(x0 + (anim_len - 1) * per);
                fill(xe, y + 3, x1 + 8 - xe, h - 6, alpha(P.accent, 0.10f));
                fill(xe, y + 3, 1, h - 6, alpha(P.accent, 0.45f));  // where the animation itself ends
            }
            TextStyle st(true);  // same-width digits
            for (int f = 0; f < frames; ++f) {
                const float x = std::floor(x0 + f * per);
                if (f == 0 || (f + 1) % step == 0) {
                    fill(x, y + h - 13, 1, 9, P.line2);
                    text(std::to_wstring(f + 1), x - 24, y + 3, 48, 15, 10.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
                } else if (minor && (f + 1) % minor == 0 && minor * per >= 6) {
                    fill(x, y + h - 8, 1, 4, P.line);
                }
            }
            // the playhead: where the animation is while it plays or is held
            if (g.viewer.playing() == anim) {
                const float x = std::floor(x0 + g.viewer.once_frame() * per);
                fill(x0, y + h - 4, std::max(0.0f, x - x0), 2, P.accent);
                fill(x - 1, y + 3, 2, h - 6, P.accent);
                g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y + 4), 4, 4), brush(P.accent));
            }
            hit({px, y, pw, h}, HIT_TIMELINE);
        }
    }
    // the controls: Play / Pause, a frame back and forward, the frame count; Particles faded on the right
    const bool running = anim >= 0 && g.viewer.playing() == anim && !g.viewer.held();
    const bool held = anim >= 0 && g.viewer.playing() == anim && g.viewer.held();
    {
        const float y = G.ctl_y;
        const std::wstring pl = tx(running ? "studio.moves.pause" : "studio.moves.play");
        const float bw = text_width(pl, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 44;
        if (anim >= 0) {
            round_fill(px, y, bw, 30, 7, g.hover == HIT_PLAY ? alpha(P.accent, 0.2f) : P.accent_bg);
            round_line(px, y, bw, 30, 7, P.accent_line);
            media_symbol(running ? 1 : 0, px + 17, y + 15, 10, P.accent2);
            text(pl, px + 28, y, bw - 34, 30, 12.5f, P.accent2, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            hit({px, y, bw, 30}, HIT_PLAY);
        } else {
            round_line(px, y, bw, 30, 7, alpha(P.line2, 0.6f));
            media_symbol(0, px + 17, y + 15, 10, P.faint);
            text(pl, px + 28, y, bw - 34, 30, 12.5f, P.faint, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        }
        float x = px + bw + 8;
        for (int k = 0; k < 2; ++k) {
            const int id = k ? HIT_STEP_FWD : HIT_STEP_BACK;
            const bool can = anim >= 0;
            round_fill(x, y, 30, 30, 7, can && g.hover == id ? P.hover : P.card);
            round_line(x, y, 30, 30, 7, can ? P.line2 : alpha(P.line2, 0.6f));
            media_symbol(k ? 3 : 2, x + 15, y + 15, 10, can ? P.text2 : P.faint);
            if (can) hit({x, y, 30, 30}, id);
            x += 34;
        }
        std::wstring fr;
        if (held) fr = tx("studio.moves.frame", {{"n", std::to_wstring(g.viewer.once_frame() + 1)}, {"count", std::to_wstring(frames)}});
        else if (anim_len > 0) fr = tx("studio.moves.frames", {{"count", std::to_wstring(anim_len)}});
        // the view's own hint (drag to turn, wheel to zoom: both views, while the move plays or is held), right-aligned
        // (snatcherLeo, 2026-11-19: "are we able to navigate the preview/rotate")
        // The target chip (snatcherLeo, 2026-11-19: the box or any Pokémon takes the hit) comes first, right-aligned; the hint
        // only when there is room left beside it
        float right = px + pw;
        const float fw = text_width(fr, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 6;
        const float cw = target_chip_width(pw * 0.4f);
        if (g.tab == kTabMoves && right - cw > x + 6 + fw + 8) {  // (the Animations tab plays the model alone: no target)
            target_chip(right - cw, y, pw * 0.4f);
            right -= cw + 14;
        }
        // a two-turn move's turn (Turn 1: the charge, part 3 alone; Turn 2: the attack), left of the target chip
        {
            const LearnRow* cr = chosen_row();
            if (cr && cr->group != 3 && two_turn_move(cr->move)) {
                const std::vector<std::pair<std::wstring, int>> turns = {{tx("studio.moves.turn1"), HIT_TURN1},
                                                                          {tx("studio.moves.turn2"), HIT_TURN2}};
                const float tw = segmented_width(turns);
                if (right - tw > x + 6 + fw + 8) {
                    segmented(right, y, turns, g.turn == 1 ? HIT_TURN1 : HIT_TURN2);
                    right -= tw + 14;
                }
            } else if (cr && cr->group != 3 && event_part_move(cr->move)) {
                // the parts picker (Substitute, Thief, Helping Hand, Wish, Snatch, Covet): the move, its follow-up, both
                const std::vector<std::pair<std::wstring, int>> parts = {{tx("studio.moves.parts_move"), HIT_PARTS_MOVE},
                                                                          {tx("studio.moves.parts_follow"), HIT_PARTS_FOLLOW},
                                                                          {tx("studio.moves.parts_both"), HIT_PARTS_BOTH}};
                const float tw = segmented_width(parts);
                if (right - tw > x + 6 + fw + 8) {
                    segmented(right, y, parts, g.parts == 1 ? HIT_PARTS_FOLLOW : g.parts == 2 ? HIT_PARTS_BOTH : HIT_PARTS_MOVE);
                    right -= tw + 14;
                }
            }
        }
        const std::wstring hint = tx("studio.viewer.hint");
        const float hw = text_width(hint, 12, DWRITE_FONT_WEIGHT_NORMAL) + 4;
        if (right - hw > x + 6 + fw + 16) text(hint, right - hw, y, hw, 30, 12, P.faint);
        TextStyle st(true);
        text(fr, x + 6, y, std::max(10.0f, right - x - 12), 30, 12.5f, P.mute);
    }
    // the view's switches, right-aligned on their own row: Particles (the Pokémon's own effects; faded when it has
    // none), Game lighting (the same setting as the Overview's), Battle camera (release step 4); each left out when the
    // row is too narrow for it
    {
        const float y = G.opt_y;
        float right = px + pw;
        const float left = px;
        // a target that can't react says so on the left (2026-11-19: never a silent fallback): it has no
        // animation of its own for the move's hit slot, so it stands as it is while the move hits it
        int tslot = 0;
        const int react = g.target_species >= 0 ? g.viewer.target_reaction(&tslot) : -3;
        const std::wstring react_note = react == -1 || react == -2
            ? tx("studio.moves.target_no_hit", {{"name", target_name(g.target_species)}, {"slot", std::to_wstring(tslot)}})
            : std::wstring();
        const std::wstring pt = tx("studio.moves.particles");
        const float ptw = text_width(pt, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 44;
        if (g.viewer.has_particles()) {
            particle_switch(right, y);
        } else {
            const float sx = right - ptw;
            round_fill(sx, y + 7, 30, 16, 8, alpha(P.line2, 0.6f));
            g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sx + 8, y + 15), 5.5f, 5.5f), brush(P.faint));
            text(pt, sx + 38, y, ptw - 38, 30, 12.5f, P.faint);
            hit({sx, y, ptw, 30}, HIT_LATER + kLaterParticles);
        }
        right -= ptw + 12;
        const float lgw = toggle_width("studio.view.lighting");
        if (right - lgw >= left) {
            lighting_switch(right + 2, y);
            right -= lgw + 12;
        }
        const float bcw = toggle_width("studio.view.battle_camera");
        if (g.tab == kTabMoves && right - bcw >= left) {  // (no move on the Animations tab, so no battle camera)
            battlecam_switch(right + 2, y);
            right -= bcw + 12;
        }
        if (!react_note.empty() && right - px > 80) text(react_note, px, y, right - px - 8, 30, 12, P.mute);
    }
    // the Animations tab's facts: the animation's name and length, and how many moves play it (the Used by card lists
    // them); nothing else, as the preview shows the model alone
    if (r && r->group == kAnimGroup) {
        const char* labels[] = {"studio.anim.info_name", "studio.anim.info_len"};
        float lw = 0;
        for (const char* k : labels) lw = std::max(lw, text_width(tx(k), 13, DWRITE_FONT_WEIGHT_NORMAL));
        lw += 16;
        const float vx = px + lw, vw = pw - lw, y = G.info_y;
        for (int k = 0; k < 2; ++k) text(tx(labels[k]), px, y + 24 * k, lw, 22, 13, P.mute);
        text(r->name, vx, y, vw, 22, 13, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(tx("studio.anim.frames", {{"n", std::to_wstring(anim_len)}}), vx, y + 24, vw, 22, 13, P.text);
        return;  // (no version note or "Make it your own" here: they're about moves)
    }
    // a reaction's facts: its slot and length (or why there is none), and a one-line note on what it is
    if (r && r->group == 3) {
        const char* labels[] = {"studio.moves.info_anim", "studio.moves.info_note"};
        float lw = 0;
        for (const char* k : labels) lw = std::max(lw, text_width(tx(k), 13, DWRITE_FONT_WEIGHT_NORMAL));
        lw += 16;
        const float vx = px + lw, vw = pw - lw, y = G.info_y;
        for (int k = 0; k < 2; ++k) text(tx(labels[k]), px, y + 24 * k, lw, 22, 13, P.mute);
        const int own = g.viewer.slot_own(r->slot);
        const std::wstring an = own >= 0 ? tx("studio.moves.react_info", {{"slot", std::to_wstring(r->slot)}, {"count", std::to_wstring(anim_len)}})
                                         : tx(own == -2 ? "studio.moves.react_info_missing"
                                              : r->slot == 11 ? "studio.moves.react_info_idle" : "studio.moves.react_info_empty",
                                              {{"slot", std::to_wstring(r->slot)}});
        text(an, vx, y, vw, 22, 13, P.text);
        para(tx(("studio.moves.react_note." + std::to_string(r->slot)).c_str()), vx, y + 24 + 2, vw, 13, P.text2, DWRITE_FONT_WEIGHT_NORMAL);
    } else
    // the facts: type and kind, power / accuracy / PP, how it's learned, the body animation, the game's description
    if (r) {
        // the animation first (2026-11-22, mockup 29; snatcherLeo: "we need to make it more clear, when you click on a move, the
        // Animation", "we don't need "Slot" names"): its name, its length and a link to it on the Animations tab
        const char* labels[] = {"studio.moves.info_anim", "studio.moves.info_type", "studio.moves.info_numbers",
                                "studio.moves.info_learned", "studio.moves.info_desc"};
        float lw = 0;
        for (const char* k : labels) lw = std::max(lw, text_width(tx(k), 13, DWRITE_FONT_WEIGHT_NORMAL));
        lw += 16;
        const float vx = px + lw, vw = pw - lw;
        float y = G.info_y;
        for (int k = 0; k < 5; ++k) text(tx(labels[k]), px, y + 24 * k, lw, 22, 13, P.mute);
        {
            const int ma = move_anim(*r);
            if (ma < 0 || anim < 0) {
                text(tx("studio.moves.anim_none"), vx, y, vw, 22, 13, P.text);
            } else {
                const std::wstring nm = anim_name(anim), len = L" · " + tx("studio.anim.frames", {{"n", std::to_wstring(anim_len)}}) + L" · ";
                const float nw = text_width(nm, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD), lnw = text_width(len, 13, DWRITE_FONT_WEIGHT_NORMAL);
                text(nm, vx, y, vw, 22, 13, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                text(len, vx + nw, y, std::max(4.0f, vw - nw), 22, 13, P.text2);
                const std::wstring link = tx("studio.anim.see");
                const float lx = vx + nw + lnw, lkw = text_width(link, 13, DWRITE_FONT_WEIGHT_NORMAL);
                if (lx + lkw <= px + pw) {
                    const bool lh = g.hover == HIT_SEE_ANIM;
                    text(link, lx, y, lkw + 2, 22, 13, lh ? P.text : P.accent2);
                    if (lh) fill(lx, y + 19, lkw, 1, P.accent2);
                    hit({lx - 2, y, lkw + 4, 22}, HIT_SEE_ANIM);
                }
            }
        }
        y += 24;
        g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(vx + 4.5f, y + 11), 4.5f, 4.5f), brush(type_dot(r->mv.type)));
        text(type_name(r->mv.type) + L" · " + move_kind(r->mv), vx + 15, y, vw - 15, 22, 13, P.text);
        {
            TextStyle st(true);
            text(move_number(r->mv.power, 2) + L" · " + move_number(r->mv.accuracy, 1) + L" · " + std::to_wstring(r->mv.pp), vx, y + 24,
                 vw, 22, 13, P.text);
        }
        const std::wstring learned = r->group == 0 ? tx("studio.moves.learned_level", {{"n", std::to_wstring(r->how)}})
                                     : r->group == 1 ? tx("studio.moves.learned_machine", {{"machine", learn_how(*r)}})
                                                     : tx("studio.moves.learned_egg");
        text(learned, vx, y + 48, vw, 22, 13, P.text);
        y -= 24;  // (the description below keeps its place: four lines above it, as before)
        // (top-aligned, so its first line sits level with its label; a long one wraps onto a second line)
        para(widen(orre_text(g.disc, r->mv.desc_msg)), vx, y + 96 + 2, vw, 13, P.text2, DWRITE_FONT_WEIGHT_NORMAL);
        // the version note: its own version, or the general one; either way the Move editor holds the move's looks
        const Rect N = {px, G.note_y, pw, kNoteH};
        round_fill(N.x, N.y, N.w, N.h, 9, P.card2);
        round_line(N.x, N.y, N.w, N.h, 9, P.line);
        text(tx(r->own ? "studio.moves.note_own" : "studio.moves.note_general", {{"name", e.name}, {"move", r->name}}), N.x + 12, N.y + 8,
             N.w - 24, 38, 12.5f, P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
        const std::wstring link = tx("studio.moves.open_editor", {{"move", r->name}});
        const float lkw = std::min(N.w - 24, text_width(link, 12.5f, DWRITE_FONT_WEIGHT_NORMAL));
        text(link, N.x + 12, N.y + 46, lkw + 2, 18, 12.5f, P.accent2);
        fill(N.x + 12, N.y + 63, lkw, 1, g.hover == HIT_MOVE_EDITOR ? P.accent2 : P.accent_line);
        hit({N.x + 10, N.y + 44, lkw + 4, 22}, HIT_MOVE_EDITOR);
    }
    // Make it your own: all later (the WZX writer and the mod system: moves-tab-plan.md section 5)
    {
        fill(R.x + 1, G.sub_y, R.w - 2, 1, P.line);
        {
            TextStyle st(false, 0.17f);
            text(tx("studio.moves.make_own"), px, G.sub_y + 12, pw, 20, 11, P.mute, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        }
        float x = px, y = G.sub_y + 12 + 26;
        for (int k = 0; k < 5; ++k) {
            const std::wstring s = tx(kMakeOwn[k]);
            const float bw = text_width(s, 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 24;
            if (x > px && x + bw > px + pw) { x = px; y += 38; }
            x += faded_button(s, x, y, HIT_LATER + kLaterSwapAnim + k) + 8;
        }
    }
}

void draw_planned(const Layout& L) {
    const Palette& P = g.pal;
    const Rect& C = L.content;
    card({C.x, C.y, C.w, 170}, L"");
    text(tx("studio.tab.planned_title", {{"tab", tx(kTabs[g.tab])}}), C.x, C.y + 40, C.w, 34, 20, P.text, DWRITE_FONT_WEIGHT_BOLD,
         DWRITE_TEXT_ALIGNMENT_CENTER);
    text(tx("studio.tab.planned_body"), C.x + C.w / 2 - 260, C.y + 80, 520, 60, 13.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL,
         DWRITE_TEXT_ALIGNMENT_CENTER, true);
}

void button(const std::wstring& s, float x_right, float y, int id, float& out_x) {
    const Palette& P = g.pal;
    const float w = text_width(s, 12.5f, DWRITE_FONT_WEIGHT_MEDIUM) + 24;
    const float x = x_right - w;
    round_fill(x, y, w, 28, 8, g.hover == id ? P.panel2 : P.panel);
    round_line(x, y, w, 28, 8, P.line);
    text(s, x, y, w, 28, 12.5f, P.text, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
    hit({x, y, w, 28}, id);
    out_x = x - 8;
}

void draw_home(const Layout& L);
void draw_settings(const Layout& L);
void draw_centre(const Layout& L) {
    const Rect& C = L.page;
    g.rt->PushAxisAlignedClip(D2D1::RectF(C.x, C.y, C.x + C.w, C.y + C.h), D2D1_ANTIALIAS_MODE_ALIASED);
    if (on_settings()) {
        draw_settings(L);
    } else if (g.cur_tab < 0 || g.selected < 0) {
        draw_home(L);
    } else {
        const Entry& e = g.all[g.selected];
        draw_header(L, e);
        if (g.tab == kTabOverview) draw_overview(L, e);
        else if (preview_tab(g.tab)) draw_moves(L, e);  // (Moves, and Animations with its own list: draw_anim_cards)
        else if (g.tab == kTabLooks) draw_styles(L);  // (the Styles tab: styles, their parts, then the pictures)
        else if (g.tab == kTabSound) draw_sound(L);   // (the Sound tab: the cries, 2026-11-22)
        else draw_planned(L);
    }
    g.rt->PopAxisAlignedClip();
    // a thin scroll indicator for the page
    if (g.pscroll_max > 0) {
        const float view = C.h, total = view + g.pscroll_max;
        const float bar = std::max(30.0f, view * view / total), y = C.y + (view - bar) * (g.pscroll / g.pscroll_max);
        round_fill(C.x + C.w - 6, y, 3, bar, 1.5f, alpha(g.pal.mute, 0.45f));
    }
}

// a text block that wraps; returns its height (DirectWrite measures it)
float para(const std::wstring& s, float x, float y, float w, float size, const D2D1_COLOR_F& c,
           DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) {
    IDWriteTextFormat* f = format(size, weight);
    if (!f) return 0;
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    f->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    IDWriteTextLayout* l = nullptr;
    g.dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, w, 2000, &l);
    if (!l) return 0;
    DWRITE_TEXT_METRICS m{};
    l->GetMetrics(&m);
    g.rt->DrawTextLayout(D2D1::Point2F(x, y), l, brush(c), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    l->Release();
    return m.height;
}

// ---- hover cards (v3: they replace v2's inspector; snatcherLeo through C: "no right-hand inspector ... explanations become
// hover cards") ----
// What the spot under the pointer means: a title and a body, both from the player's disc where the game has the
// words (ability descriptions, item names) and Studio's own explanations otherwise. Empty title = no card.
bool can_go(int step);

void hover_card_text(int id, std::wstring& title, std::wstring& body) {
    if (id < 0 || !g.disc) return;
    if (id >= HIT_PTAB && id < HIT_PTAB + 100) {
        // a tab whose name was cut with "…" shows its whole name
        const size_t k = (size_t)(id - HIT_PTAB);
        if (k < g_tab_cut.size() && g_tab_cut[k] && k < g.page_tabs.size()) title = tab_label(g.page_tabs[k]);
        return;
    }
    if ((id == HIT_BACK || id == HIT_FORWARD) && can_go(id == HIT_BACK ? -1 : 1)) {
        title = tx(id == HIT_BACK ? "studio.nav.back" : "studio.nav.forward");
        body = tx(id == HIT_BACK ? "studio.nav.back_help" : "studio.nav.forward_help");
        return;
    }
    if (g.selected < 0) return;  // (Home: the cards below are the Pokémon page's)
    const Entry& e = g.all[g.selected];
    if (id >= HIT_STAT && id < HIT_STAT + 6) {
        const int i = id - HIT_STAT;
        title = tx("studio.help.stat_title", {{"stat", tx(kStatKeys[i])}, {"value", std::to_wstring(e.s.base_stats[i])}});
        char key[32];
        std::snprintf(key, sizeof key, "studio.help.stat.%d", i);
        body = tx(key);
    } else if (id >= HIT_ABILITY && id < HIT_ABILITY + 2) {
        uint32_t nm = 0, doc = 0;
        orre_ability_messages(g.disc, e.s.abilities[id - HIT_ABILITY], &nm, &doc);
        title = title_case(widen(orre_text(g.disc, nm)));
        body = widen(orre_text(g.disc, doc)) + L"\n" + tx("studio.help.ability_open");
    } else if (id == HIT_ABILITY_HIDDEN) {
        title = tx("studio.ability.hidden");
        body = tx("studio.help.hidden");
    } else if ((id >= HIT_TYPE && id < HIT_TYPE + 18) || (id >= HIT_MATCH && id < HIT_MATCH + 18)) {
        const int t = id >= HIT_MATCH ? id - HIT_MATCH : id - HIT_TYPE;
        title = type_name(t);
        const int cat = orre_type_category(g.disc, t);
        body = tx(cat == 1 ? "studio.type.physical" : cat == 2 ? "studio.type.special" : "studio.type.neither", {{"type", title}}) + L"\n" +
               tx("studio.help.type_open");
    } else if (id >= HIT_TRAIN && id < HIT_TRAIN + 8) {
        char key[40];
        std::snprintf(key, sizeof key, "studio.help.train.%d", id - HIT_TRAIN);
        static const char* kRow[] = {"studio.train.exp_group", "studio.train.catch_rate", "studio.train.gender", "studio.train.friendship",
                                     "studio.train.exp_given", "studio.train.effort", "studio.train.held", "studio.train.eggs"};
        title = tx(kRow[id - HIT_TRAIN]);
        body = tx(key);
    } else if (id >= HIT_ERA && id < HIT_ERA + 20) {
        const int k = id - HIT_ERA;
        // (the first button, Gen 3 · Colosseum, is the shown one and has no hover card)
        title = k <= kEraGens ? tx("studio.era.gen", {{"n", std::to_wstring(k - 1 + kEraFirstGen)}})
                              : tx(k == kEraGens + 1 ? "studio.era.letsgo" : k == kEraGens + 2 ? "studio.era.legends_arceus" : "studio.era.legends_za");
        body = tx("studio.help.era");
    } else if (id >= HIT_EVO_METHOD && id <= HIT_EVO_METHOD_END) {
        const auto fam = evolution_family(e.index);
        const size_t i = (size_t)(id - HIT_EVO_METHOD);
        if (i >= fam.size()) return;
        title = evo_method(fam[i].kind, fam[i].param);
        body = tx(method_is_item(fam[i].kind) ? "studio.help.item_link" : "studio.help.friend_link");
    } else if (id >= HIT_LROW && id <= HIT_LROW_END) {
        // a Reactions row whose name is a guess (slots 9 and 16, animation-catalog.md): say so
        const size_t i = (size_t)(id - HIT_LROW);
        if (i < g.learn.size() && g.learn[i].group == 3 && react_name_likely(g.learn[i].slot)) {
            title = g.learn[i].name;
            body = tx("studio.moves.react_likely", {{"slot", std::to_wstring(g.learn[i].slot)}});
        }
    } else if (id >= HIT_LATER && id < HIT_LATER + kLaterCount) {
        // the Moves tab's faded controls: what each will do, and that it comes later
        static const char* kTitle[] = {"studio.moves.edit", "studio.moves.test", "studio.moves.rate_title", "studio.moves.particles",
                                       "studio.moves.swap_anim", "studio.moves.swap_particles", "studio.moves.attach",
                                       "studio.moves.sounds", "studio.moves.reaction"};
        const int k = id - HIT_LATER;
        title = tx(kTitle[k]);
        char key[40];
        std::snprintf(key, sizeof key, "studio.moves.help_later.%d", k);
        body = tx(key);
    } else if (id == HIT_PARTICLES) {
        title = tx("studio.moves.particles");
        body = tx(g.tab == kTabMoves ? "studio.help.particles_moves" : "studio.help.particles");
        // (no hover card on the rate button: it covered the stage; snatcherLeo, 2026-11-19, "get rid of tooltip here". The
        // words stay in the language files for a later help page)
    } else if (id == HIT_RESET && g.viewer.view_moved()) {
        // (only once there is something to reset; short, so it covers little of the model: snatcherLeo asked for the rate
        // button's and the stage buttons' cards to go because they covered it, 2026-11-19)
        title = tx("studio.view.reset");
        body = tx("studio.help.reset");
    } else if (id == HIT_LIGHTING) {
        // on: what it is; off: why a modder would leave it off (2026-11-19)
        title = tx("studio.view.lighting");
        body = tx(g.game_light ? "studio.help.lighting_on" : "studio.help.lighting_off");
    } else if (id == HIT_BATTLECAM) {
        title = tx("studio.view.battle_camera");
        body = tx(g.battle_cam ? "studio.help.battle_camera_on" : "studio.help.battle_camera_off");
    } else if (id == HIT_TARGET) {
        title = tx("studio.moves.target");
        body = tx("studio.help.target");
    } else if (id == HIT_MOVE_EDITOR) {
        title = tx("studio.moves.editor_title");
        body = tx("studio.moves.editor_help");
        // (no hover card on Studio view / Plain stage: it covered the stage awkwardly; snatcherLeo, 2026-11-19, "we don't need
        // them". The words stay in the language files for a later help page)
    } else if (id == HIT_STEP_BACK || id == HIT_STEP_FWD) {
        title = tx(id == HIT_STEP_BACK ? "studio.moves.step_back" : "studio.moves.step_forward");
        body = tx("studio.moves.help_step");
    } else if (id == HIT_CREATE_MOD) {
        title = tx("studio.page.create_mod");
        body = tx("studio.help.create_mod");
    } else if (id == HIT_COPY) {
        title = tx("studio.page.copy");
        body = tx("studio.help.copy");
    }
}

void draw_hover_card(const Layout& L) {
    std::wstring title, body;
    hover_card_text(g.hover, title, body);
    if (title.empty() || g.mouse_x < 0) return;
    // none while tabs are dragged (snatcherLeo's stress test, 2026-11-22: the pressed tab's card, its full name in a black
    // box, stayed on screen through the drag; "We don't need it right? Can we get rid of it?")
    if (g_drag.active) return;
    const Palette& P = g.pal;
    const float w = 290;
    // measure, then place beside the pointer, kept inside the window
    IDWriteTextFormat* f = format(12.5f, DWRITE_FONT_WEIGHT_NORMAL);
    float bh = 0;
    if (f) {
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        IDWriteTextLayout* l = nullptr;
        g.dw->CreateTextLayout(body.c_str(), (UINT32)body.size(), f, w - 24, 2000, &l);
        if (l) { DWRITE_TEXT_METRICS m{}; l->GetMetrics(&m); bh = m.height; l->Release(); }
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    const float h = 20 + 20 + bh + 4;
    const float x = std::min(L.w - w - 8, g.mouse_x + 14), y = g.mouse_y + 16 + h > L.h - 8 ? g.mouse_y - h - 8 : g.mouse_y + 16;
    round_fill(x, y, w, h, 9, P.dark ? hexc(0x0F110F) : P.card);
    round_line(x, y, w, h, 9, P.line2);
    text(title, x + 12, y + 10, w - 24, 20, 13, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    para(body, x + 12, y + 32, w - 24, 12.5f, P.text2);
}

// a tab's picture: the Pokémon's mug shot (made on first use), none for Home (its tab draws the O instead)
ID2D1Bitmap* tab_bitmap(int idx) {
    if (idx < 0) return nullptr;  // Home (the O) and Settings (the gear) draw a symbol instead
    Entry& e = g.all[(size_t)idx];
    if (!e.bmp && !e.mug.empty()) e.bmp = make_bitmap(e.mug.data(), 42, 42);
    return e.bmp;
}

// ---- the tab bar (v3): one tab per open page (a Pokémon's mug shot and name, or Home's O), then + ----
void draw_tab_bar(const Layout& L) {
    const Palette& P = g.pal;
    fill(0, 0, L.w, kTabBar, P.win);
    fill(0, kTabBar - 1, L.w, 1, P.line);
    float x = 8;
    // Back / Forward first, as a browser has them (snatcherLeo, 2026-11-18); faint when there is nowhere to go
    for (int step : {-1, 1}) {
        const int id = step < 0 ? HIT_BACK : HIT_FORWARD;
        const bool ok = can_go(step);
        if (ok && g.hover == id) round_fill(x, 7, 24, kTabBar - 12, 6, P.hover);
        text(step < 0 ? L"←" : L"→", x, 7, 24, kTabBar - 14, 15, ok ? P.mute : P.faint, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({x, 7, 24, kTabBar - 12}, id);
        x += 26;
    }
    x += 6;
    // fixed: a Pokémon tab's width (0 = sized to its label: Home); the label is cut with "…" when longer
    // (selected: one of several selected tabs; floating: a dragged tab drawn at the pointer, no hit spots; logo: the
    // Home tab, OrrePort's O in place of a mug shot)
    auto tab = [&](const std::wstring& label, ID2D1Bitmap* bmp, bool on, int id, int close_id, float fixed = 0, bool selected = false,
                   bool floating = false, int app_page = 0) {
        const bool logo = app_page < 0;  // Studio's own page (kHome: the O; kSettings: the gear) instead of a mug shot
        const bool pic = bmp || logo;
        const float tw = text_width(label, 13, on || selected ? DWRITE_FONT_WEIGHT_MEDIUM : DWRITE_FONT_WEIGHT_NORMAL);
        const float w = fixed > 0 ? fixed : 24 + (pic ? 28 : 0) + tw + (close_id >= 0 ? 24 : 0);
        const float room = w - 24 - (pic ? 28 : 0) - (close_id >= 0 ? 24 : 0);  // what the label may take
        if (id >= HIT_PTAB && id < HIT_PTAB + 100) {
            const size_t k = (size_t)(id - HIT_PTAB);
            if (g_tab_cut.size() <= k) g_tab_cut.resize(k + 1);
            g_tab_cut[k] = tw > room + 1;
        }
        // a selected tab's accent line: the whole top edge of the tab, following its rounded corners (the tab's own
        // shape, clipped to its top 2.5 DIPs; snatcherLeo, 2026-11-19: "shouldn't that green line be covering the entire top
        // portion of each tab selected")
        auto accent_top = [&] {
            g.rt->PushAxisAlignedClip(D2D1::RectF(x, 5, x + w, 7.5f), D2D1_ANTIALIAS_MODE_ALIASED);
            round_fill(x, 5, w, kTabBar + 12, 8, P.accent);
            g.rt->PopAxisAlignedClip();
        };
        if (on) {
            // the shown tab: rounded at the top only, open at the bottom into the page. It is drawn taller than the
            // bar and clipped at the bar's edge, so its bottom corners never show, and its fill covers the bar's
            // separator line under it (it used to poke out below the bar as two corner stubs: snatcherLeo, 2026-11-18)
            g.rt->PushAxisAlignedClip(D2D1::RectF(0, 0, L.w, kTabBar), D2D1_ANTIALIAS_MODE_ALIASED);
            round_fill(x, 5, w, kTabBar + 12, 8, P.bg);
            round_line(x, 5, w, kTabBar + 12, 8, P.line);
            g.rt->PopAxisAlignedClip();
            if (selected) accent_top();  // the shown tab is one of a multi-selection: it gets the line too
        } else if (floating) {
            round_fill(x, 7, w, kTabBar - 12, 8, P.card);  // a dragged tab: lifted, on a card of its own
            round_line(x, 7, w, kTabBar - 12, 8, P.accent_line);
        } else if (selected) {
            // one of several selected tabs (snatcherLeo, 2026-11-19: "make it look more refined/clear that I multi-selected
            // the tabs"): the shown tab's own shape, open into the page, in a softer fill, plus an accent line along its
            // top, so the group reads as one set of raised tabs (as Chrome and Edge draw a multi-selection); its name
            // brightens to the shown tab's colour
            g.rt->PushAxisAlignedClip(D2D1::RectF(0, 0, L.w, kTabBar - 1), D2D1_ANTIALIAS_MODE_ALIASED);
            round_fill(x, 5, w, kTabBar + 12, 8, alpha(P.bg, 0.55f));
            round_line(x, 5, w, kTabBar + 12, 8, alpha(P.line, 0.8f));
            g.rt->PopAxisAlignedClip();
            accent_top();
        } else if (g.hover == id) {
            round_fill(x, 7, w, kTabBar - 12, 8, P.hover);
        }
        float cx = x + 12;
        if (bmp) { draw_mug(bmp, cx, 10, 20); cx += 28; }
        else if (logo) {
            if (app_page == kSettings) draw_gear(cx, 10, 20, on || selected ? gear_colour() : P.text2);
            else if (page_editor(app_page)) draw_symbol(page_editor(app_page), cx, 10, 20, on || selected ? P.text : P.text2);
            else draw_o(cx, 10, 20);
            cx += 28;
        }
        const bool bright = on || selected;
        text(label, cx, 6, std::min(tw + 2, room), kTabBar - 8, 13, bright ? P.text : P.text2, bright ? DWRITE_FONT_WEIGHT_MEDIUM : DWRITE_FONT_WEIGHT_NORMAL);
        if (floating) { x += w + 2; return; }
        hit({x, 5, w - (close_id >= 0 ? 22 : 0), kTabBar - 5}, id);
        if (close_id >= 0) {
            const float bx = x + w - 24;
            if (g.hover == close_id) round_fill(bx, 11, 16, 16, 4, P.hover);
            text(L"✕", bx, 10, 16, 18, 10, g.hover == close_id ? P.text : P.faint, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            hit({bx, 9, 18, 20}, close_id);
        }
        x += w + 2;
    };
    // (Home is the O at the top of the rail since 2026-11-18, snatcherLeo: "get rid of "Home" ... to get home, you click our
    // "O" logo"; the room it leaves above the list is for a search field, still to be decided)
    // The Pokémon tabs (snatcherLeo, 2026-11-18: "Keeping it the same size works for me, and with the truncation "..." that
    // works too"): they start at the page's left edge, over the page they show (Back / Forward and Home stay over the
    // rail and list), all one width (kTabW), shrinking evenly to kTabMin when many are open; past that the strip
    // scrolls sideways (the wheel over it; the shown tab is kept in view) and "▾" lists every open tab.
    constexpr float kTabW = 180, kTabMin = 110, kTabGap = 2, kBtn = 28;
    const size_t n = g.page_tabs.size();
    const float x0 = std::max(x + 8, kRail + kList);  // (the list's edge even on Home, whose page starts further left)
    float avail = L.w - 8 - kBtn - x0;  // room left of "+"
    float tw = n ? std::clamp(avail / (float)n - kTabGap, kTabMin, kTabW) : kTabW;
    const bool overflow = n && (float)n * (tw + kTabGap) > avail + 0.5f;
    // When the tabs don't all fit. *Updated 2026-11-22 (snatcherLeo's 4.3 pass, with screenshots of a narrow window: the shown
    // tab at the right end cut off under "›", and a sliver of a tab beside "‹"; "if we are selected on the furthest tab
    // to the right ... It just clips and looks awkward"):* "‹" and "›" get their own room at the strip's ends instead
    // of lying over the tabs (faded when there's nothing more that way), the tabs widen a little so a whole number of
    // them fills the room between, and the strip scrolls by whole tabs. So no tab is ever cut off, as when every tab fits.
    constexpr float kEdge = 26;
    float sx0 = x0, savail = avail;  // the tabs' own room (between the arrows when they overflow)
    int vis = (int)n;                // how many tabs show at once
    if (overflow) {
        avail -= kBtn;  // room for "▾" too
        sx0 = x0 + kEdge;
        savail = avail - 2 * kEdge;
        vis = std::max(1, (int)std::floor((savail + kTabGap) / (kTabMin + kTabGap)));
        tw = std::max(40.0f, (savail + kTabGap) / (float)vis - kTabGap);
    }
    const float step = tw + kTabGap;
    // keep the shown tab in view when it changes (or a tab is added); scrolling by hand (the wheel, the arrows) stays
    // put otherwise; the scroll is always a whole number of tabs
    const int max_first = std::max(0, (int)n - vis);
    int first = overflow ? std::clamp((int)std::lround(g.tab_scroll / step), 0, max_first) : 0;
    if (overflow && g.cur_tab >= 0 && (g.cur_tab != g.tab_followed || (int)n != g.tab_count_seen)) {
        if (g.cur_tab < first) first = g.cur_tab;
        if (g.cur_tab > first + vis - 1) first = g.cur_tab - vis + 1;
        first = std::clamp(first, 0, max_first);
    }
    g.tab_followed = g.cur_tab;
    g.tab_count_seen = (int)n;
    g.tab_scroll = (float)first * step;
    // when scrolled, "‹" at the strip's start jumps back to the first tabs and "›" at its end to the last (snatcherLeo:
    // "quickly get back to the beginning tab")
    const bool more_left = overflow && first > 0, more_right = overflow && first < max_first;
    g.rt->PushAxisAlignedClip(D2D1::RectF(sx0, 0, sx0 + savail, kTabBar), D2D1_ANTIALIAS_MODE_ALIASED);
    g_hit_clip = {sx0, 0, savail, kTabBar};
    // (kept for dragging: where a pointer x lands among the tabs)
    g.strip_x0 = sx0 - g.tab_scroll;
    g.strip_step = step;
    g.strip_avail = savail;
    if (g.tab_sel.size() != n) g.tab_sel.resize(n, 0);
    // while tabs of this window are dragged along its strip: the others close up, leaving a gap where they'll land,
    // and the dragged ones follow the pointer on top (Chrome's sliding)
    const bool reordering = g_drag.active && !g_drag.torn && g_drag.src == g.hwnd;
    std::vector<char> dragged(n, 0);
    if (reordering)
        for (int t : g_drag.tabs) if (t >= 0 && t < (int)n) dragged[(size_t)t] = 1;
    const int gap = reordering ? (int)g_drag.tabs.size() : 0;
    int slot = 0;
    // several tabs selected: the shown one belongs to the selection too, and is drawn with its line
    bool multi = false;
    for (size_t i = 0; i < n; ++i) if (g.tab_sel[i] && (int)i != g.cur_tab) multi = true;
    // a new tab growing in (snatcherLeo, 2026-11-22, option A): it is revealed from its left edge as its width eases out from 0
    // to full over kTabGrowMs (ease-out cubic), and the tabs after it and "+" slide along by what it hasn't taken yet
    float grow = 1;
    int born = -1;
    if (g.tab_born >= 0 && g.tab_born < (int)n && !reordering) {
        const unsigned long long el = GetTickCount64() - g.tab_born_at;
        if (el < kTabGrowMs) {
            const float p = 1.0f - (float)el / (float)kTabGrowMs;
            grow = 1.0f - p * p * p;
            born = g.tab_born;
        } else {
            g.tab_born = -1;
        }
    }
    const float lag = born >= 0 ? (1.0f - grow) * (tw + kTabGap) : 0.0f;
    for (size_t i = 0; i < n; ++i) {
        if (dragged[i]) continue;
        if (reordering && slot == g_drag.insert) slot += gap;
        x = sx0 - g.tab_scroll + (float)slot * (tw + kTabGap) - (born >= 0 && (int)i > born ? lag : 0.0f);
        ++slot;
        if (x + tw >= sx0 && x <= sx0 + savail) {
            const bool growing = (int)i == born;
            if (growing) g.rt->PushAxisAlignedClip(D2D1::RectF(x, 0, x + tw * grow, kTabBar), D2D1_ANTIALIAS_MODE_ALIASED);
            tab(tab_label(g.page_tabs[i]), tab_bitmap(g.page_tabs[i]), (int)i == g.cur_tab, HIT_PTAB + (int)i, HIT_PTAB_CLOSE + (int)i, tw,
                g.tab_sel[i] != 0 || (multi && (int)i == g.cur_tab), false, std::min(0, g.page_tabs[i]));
            if (growing) g.rt->PopAxisAlignedClip();
        }
    }
    // tabs torn off this window (2026-11-22, snatcherLeo's 4.3 pass): faded where they were, so it's clear which ones travel
    if (g_drag.active && g_drag.torn && g_drag.src == g.hwnd)
        for (int t : g_drag.tabs)
            if (t >= 0 && t < (int)n) fill(sx0 - g.tab_scroll + (float)t * (tw + kTabGap), 4, tw, kTabBar - 5, alpha(P.win, 0.7f));
    x = sx0 - g.tab_scroll + (float)n * (tw + kTabGap) - lag;  // ("+" follows a growing tab)
    g_hit_clip = {0, 0, 0, 0};
    g.rt->PopAxisAlignedClip();
    if (reordering) {
        // the dragged tabs stay within the strip, between its start and the last tab's place: never over the "+" or
        // past the end (snatcherLeo, 2026-11-19: "Maybe it stops right before the "+" button"), and a window's only tab
        // can't slide sideways at all (it can still be dragged up or down, out of the window), as in Chrome
        const float k = (float)g_drag.tabs.size() * (tw + kTabGap);
        const float lo = std::max(sx0, sx0 - g.tab_scroll);
        const float hi = std::max(lo, std::min(sx0 - g.tab_scroll + (float)n * (tw + kTabGap) - k, sx0 + savail - k + kTabGap));
        float fx = std::clamp(g_drag.mouse_x - g_drag.grab_dx, lo, hi);
        for (int t : g_drag.tabs) {
            if (t < 0 || t >= (int)n) continue;
            x = fx;
            tab(tab_label(g.page_tabs[(size_t)t]), tab_bitmap(g.page_tabs[(size_t)t]), t == g.cur_tab, -1, HIT_PTAB_CLOSE + t, tw, false, true,
                std::min(0, g.page_tabs[(size_t)t]));
            fx += tw + kTabGap;
        }
        x = sx0 - g.tab_scroll + (float)n * (tw + kTabGap);
    }
    // tabs dragged from another window over this strip: a vertical line where they would be dropped. *Updated
    // 2026-11-22 (snatcherLeo, from mockup option B: "a clear indicator where you are moving it back into that window (the
    // vertical line)"):* the bar's full height with a cap at each end, an I-beam, where it was a short thin bar
    if (g_drag.active && g_drag.dock == g.hwnd) {
        const float bx = std::clamp(sx0 - g.tab_scroll + (float)g_drag.dock_at * (tw + kTabGap) - 2, sx0 + 3, sx0 + savail - 6);
        round_fill(bx, 4, 3, kTabBar - 8, 1.5f, P.accent);
        round_fill(bx - 3, 4, 9, 2.5f, 1.25f, P.accent);
        round_fill(bx - 3, kTabBar - 6.5f, 9, 2.5f, 1.25f, P.accent);
    }
    // the arrows in their own room at the strip's ends (overflow only); faded, and not clickable, when there's nothing
    // more that way
    auto edge_button = [&](float bx, const wchar_t* glyph, int id, bool live) {
        if (live && g.hover == id) round_fill(bx + 1, 8, kEdge - 2, kTabBar - 14, 6, P.hover);
        text(glyph, bx, 6, kEdge, kTabBar - 12, 16, !live ? P.faint : g.hover == id ? P.text : P.mute, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER);
        if (live) hit({bx, 5, kEdge, kTabBar - 8}, id);
    };
    if (overflow) {
        edge_button(x0, L"‹", HIT_TAB_FIRST, more_left);
        edge_button(x0 + avail - kEdge, L"›", HIT_TAB_LAST, more_right);
    }
    x = std::min(x, x0 + avail);
    if (overflow) {
        // "▾": every open tab in a list (a native menu, handled in click())
        const float mx = x0 + avail + 2;
        if (g.hover == HIT_TAB_MENU) round_fill(mx, 9, 24, 20, 6, P.hover);
        text(L"▾", mx, 7, 24, 22, 13, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({mx, 7, 24, 22}, HIT_TAB_MENU);
        x = mx + kBtn - 4;
    }
    // + : a new tab with the current Pokémon (Ctrl+click anything opens one too)
    if (g.hover == HIT_PTAB_ADD) round_fill(x + 4, 9, 24, 20, 6, P.hover);
    text(L"+", x + 4, 7, 24, 22, 15, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    hit({x + 4, 7, 24, 22}, HIT_PTAB_ADD);
}

// Home: what Studio is and how to open things (the list on the left; Ctrl+click for a new tab)
// ---- updates (4.4, 2026-11-23) ----
// snatcherLeo's design, from the mockups: Studio is a workbench, so it tells quietly: a dot on the rail's Home icon ("just a dot
// on the rail's Home icon (the quietest)"), Home's update line ("our homepage would somehow tell the users there is an
// update, and they can "update""), and Settings › About ("Users can also update from Settings as well"). Nothing in the
// tab bar or over the work. The check runs by itself on start; nothing downloads without Download ("we should always
// ask first") unless "Download updates automatically" is on; nothing installs until a restart. One update covers both
// programs, so most people meet it in the hub. The work underneath is the project's shared module (port/src/pc_update.c): one
// check, one download and one shared state for both programs; this file only draws and asks.
// Windows today (the module); Linux / macOS: pc_update_init returns 0 there, so Studio shows no update (likely).
void repaint_all();  // (every window redrawn: with Settings › Discs, further down)
#ifdef STUDIO_NO_UPDATER
// (a build without Monocypher: no updater, the calls do nothing)
extern "C" {
int pc_update_init(const char*, const char*, const char*) { return 0; }
void pc_update_set_auto_download(int) {}
void pc_update_check(int) {}
void pc_update_download(void) {}
int pc_update_poll(PcUpdateStatus*) { return 0; }
int pc_update_request_rollback(void) { return 0; }
void pc_update_shutdown(void) {}
}
#endif
#ifndef ORRE_VERSION
#define ORRE_VERSION "0.0.0"
#endif
// once at start: the OrrePort folder's updater (the one holding app\ and data\: the portable root, or --orreport's folder
// in development). Test runs leave it off unless ORRE_UPDATE_URL points at a test release, so a screenshot run never
// asks GitHub or touches the folder's update state
void updates_start() {
    if (g.orreport_root.empty()) return;
    const bool test_run = g.select_given || !g.screenshot.empty();
    wchar_t env[8];
    if (test_run && !GetEnvironmentVariableW(L"ORRE_UPDATE_URL", env, 8)) return;
    const std::string lang = g.lang_code == "auto" || g.lang_code.empty() ? "en" : g.lang_code;
    g_upd_on = pc_update_init(narrow(g.orreport_root).c_str(), ORRE_VERSION, lang.c_str()) != 0;
    if (!g_upd_on) { logf(Level::Info, "update", "updater not available here"); return; }
    pc_update_set_auto_download(g.update_auto);
    pc_update_check(g.update_check);
    logf(Level::Info, "update", "version %s; checking %s, downloading %s", ORRE_VERSION, g.update_check ? "on" : "off (fully offline)",
         g.update_auto ? "by itself" : "only when asked");
}
// once a second (perf_timer): the shared state, redrawn only when it changed; logged when its state changes
void updates_poll() {
    if (!g_upd_on) return;
    PcUpdateStatus s{};
    if (!pc_update_poll(&s)) return;
    if (s.state != g_upd.state) {
        static const char* names[] = {"idle", "off", "checking", "up to date", "available", "downloading", "ready", "failed"};
        logf(s.state == PC_UPDATE_FAILED ? Level::Warn : Level::Info, "update", "%s%s%s%s%s%s", names[std::clamp((int)s.state, 0, 7)],
             s.latest[0] ? ": " : "", s.latest, s.error_key[0] ? " (" : "", s.error_key, s.error_key[0] ? ")" : "");
    }
    // (an update's download and its checking read and write a few hundred MB on purpose: left out of the disk check, so
    // the log doesn't warn about Studio misbehaving; snatcherLeo's update hand check, 2026-11-23, showed both warnings)
    if (s.state == PC_UPDATE_DOWNLOADING || (s.state == PC_UPDATE_READY && g_upd.state != PC_UPDATE_READY)) watch_forget_io();
    g_upd = s;
    repaint_all();
}
// the switches changed here or in the hub (read again when Studio comes to the front)
void updates_settings_changed() {
    if (!g_upd_on) return;
    pc_update_set_auto_download(g.update_auto);
    pc_update_check(g.update_check);
}
// "Restart now" / "Go back": every window closes as on a normal close (the last one saves the session, so the tabs come
// back), and run returns PC_UPDATE_EXIT_RESTART for the launcher, which swaps app\ and starts Studio again. Only with a
// launcher (the portable layout) and with OrrePort closed (the swap can't happen while a program from app\ runs)
bool updates_can_restart() { return !g.root_dir.empty() && !g_upd.other_running; }
void updates_restart() {
    if (!updates_can_restart()) return;
    logf(Level::Info, "update", "restarting for the update (exit code for the launcher)");
    g_exit_code = PC_UPDATE_EXIT_RESTART;
    for (size_t k = g_windows.size(); k-- > 1;) DestroyWindow(g_windows[k].hwnd);
    if (!g_windows.empty()) DestroyWindow(g_windows.front().hwnd);
}
std::wstring mb_text(uint64_t bytes) {
    wchar_t b[32];
    swprintf(b, 32, L"%.0f", bytes / 1048576.0 < 1 ? 1.0 : bytes / 1048576.0);
    return tx("update.size", {{"mb", b}});
}
// Home's update line, under its buttons: what the state asks of the user, one card. Returns nothing; draws only while
// there is something to say (the dot's states)
void draw_update_line(float cx, float y, float w) {
    if (!upd_attention()) return;
    const Palette& P = g.pal;
    const PcUpdateState s = g_upd.state;
    const std::wstring ver = widen(g_upd.latest);
    std::wstring title, sub;
    D2D1_COLOR_F dot = hexc(0xE8955A);
    if (s == PC_UPDATE_AVAILABLE) {
        title = tx("update.available", {{"version", ver}});
        sub = tx("update.both") + L" · " + mb_text(g_upd.size);
    } else if (s == PC_UPDATE_DOWNLOADING) {
        const int pct = g_upd.size ? (int)(g_upd.done * 100 / g_upd.size) : 0;
        title = tx("update.downloading", {{"version", ver}, {"percent", std::to_wstring(pct)}});
        sub = tx("update.both");
    } else if (s == PC_UPDATE_READY) {
        dot = P.accent;
        title = tx("update.ready", {{"version", ver}});
        // (OrrePort open: "Close OrrePort" finishes it, 4.4; while its game runs, nothing is offered: update.wait_game)
        sub = g_upd.other_running ? (g_game_running ? tx("update.wait_game") : tx("update.close_other", {{"program", L"OrrePort"}}))
            : g.root_dir.empty() ? tx("studio.update.restart_by_hand") : tx("studio.update.ready_body");
    } else {
        dot = hexc(0xE0705E);
        title = tx(g_upd.error_key[0] ? g_upd.error_key : "update.failed");
        sub = tx("update.both");
    }
    // (layout, *changed 2026-11-23* after the first screenshots cut the second line: the text wraps to its full width,
    // "What's new" is a link under it, and the notes open below with room at the bottom)
    std::wstring notes = widen(g_upd.notes);
    for (size_t p; (p = notes.find(L"\\n")) != std::wstring::npos;) notes.replace(p, 2, L"\n");
    const bool has_notes = !notes.empty() && s != PC_UPDATE_FAILED;
    auto wrapped = [&](const std::wstring& t, float size, float width) {
        float hh = 0;
        if (IDWriteTextFormat* f = format(size, DWRITE_FONT_WEIGHT_NORMAL)) {
            f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            IDWriteTextLayout* l = nullptr;
            g.dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, width, 2000, &l);
            if (l) { DWRITE_TEXT_METRICS m{}; l->GetMetrics(&m); hh = m.height; l->Release(); }
            f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return hh;
    };
    const float x = cx - w / 2;
    // the button on the right, by state (its width first: the text wraps beside it)
    std::wstring blabel;
    int bid = -1;
    bool bprimary = true;
    if (s == PC_UPDATE_AVAILABLE) { blabel = tx("update.download"); bid = HIT_UPD_DOWNLOAD; }
    else if (s == PC_UPDATE_READY && updates_can_restart()) { blabel = tx("update.restart"); bid = HIT_UPD_RESTART; }
    else if (s == PC_UPDATE_READY && g_upd.other_running && !g_game_running && !g_apps_root.empty()) {
        blabel = tx("update.close_button", {{"program", L"OrrePort"}});
        bid = HIT_UPD_CLOSE_OTHER;
    }
    else if (s == PC_UPDATE_FAILED) { blabel = tx("update.retry"); bid = HIT_UPD_RETRY; bprimary = false; }
    const float bw = bid >= 0 ? text_width(blabel, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 28 : 0;
    const float tw = w - 34 - 16 - (bw > 0 ? bw + 14 : 0);
    const float sh = wrapped(sub, 12, tw);
    float body = 36 + sh + 6;  // (the title, the wrapped line under it)
    const float link_y = y + body;
    if (has_notes) body += 24;
    const float bar_y = y + body;
    if (s == PC_UPDATE_DOWNLOADING) body += 12;
    const float nh = g_upd_notes && has_notes ? wrapped(notes, 12.5f, w - 56) : 0;
    const float notes_y = y + body + 10;
    const float h = body + (nh > 0 ? nh + 22 : 8);
    round_fill(x, y, w, h, 12, P.card);
    round_line(x, y, w, h, 12, P.line);
    g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 20, y + 24), 4.5f, 4.5f), brush(dot));
    if (bid >= 0) {
        const float bx = x + w - 14 - bw, by = y + 15;
        const bool hov = g.hover == bid;
        round_fill(bx, by, bw, 34, 8, bprimary ? (hov ? alpha(P.accent, 0.2f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(bx, by, bw, 34, 8, bprimary ? P.accent_line : P.line2);
        text(blabel, bx, by, bw, 34, 12.5f, bprimary ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx, by, bw, 34}, bid);
    }
    text(title, x + 34, y + 12, tw, 22, 13.5f, s == PC_UPDATE_FAILED ? hexc(0xE0705E) : P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    text(sub, x + 34, y + 36, tw, sh + 4, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
    if (has_notes) {
        const std::wstring nl = tx(g_upd_notes ? "update.notes_hide" : "update.notes");
        const float lw = text_width(nl, 12, DWRITE_FONT_WEIGHT_MEDIUM) + 6;
        text(nl, x + 34, link_y, lw, 20, 12, g.hover == HIT_UPD_NOTES ? P.text : P.accent2, DWRITE_FONT_WEIGHT_MEDIUM);
        hit({x + 30, link_y, lw + 8, 20}, HIT_UPD_NOTES);
    }
    if (s == PC_UPDATE_DOWNLOADING) {
        const float pct = g_upd.size ? (float)g_upd.done / (float)g_upd.size : 0;
        round_fill(x + 34, bar_y + 2, w - 48, 4, 2, P.line);
        round_fill(x + 34, bar_y + 2, (w - 48) * pct, 4, 2, P.accent);
    }
    if (nh > 0) {
        fill(x + 16, notes_y - 6, w - 32, 1, P.line);
        text(notes, x + 34, notes_y + 4, w - 56, nh + 4, 12.5f, P.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
    }
}

void draw_home(const Layout& L) {
    const Palette& P = g.pal;
    const Rect& C = L.page;
    const float cy = C.y + C.h / 2 - 80;
    draw_o(C.x + C.w / 2 - 28, cy - 70, 56);
    text(tx("studio.home.title"), C.x + 40, cy, C.w - 80, 40, 24, P.text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    text(tx("studio.home.body"), C.x + C.w / 2 - 260, cy + 44, 520, 70, 14, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER,
         true);
    // the gear: Studio's Settings (snatcherLeo, 2026-11-19: "the homepage ... shows a "Gear icon" and this is the Settings for
    // the overall app"); a button with the gear, its name and its shortcut
    // *and Report a problem beside it (2026-11-22, the report flow snatcherLeo approved: "a Report a problem button on Home")*
    const std::wstring name = tx("studio.settings.title"), keys = tx("studio.settings.keys");
    const std::wstring rname = tx("studio.settings.report"), rkeys = tx("studio.report.keys");
    const float nw = text_width(name, 14, DWRITE_FONT_WEIGHT_MEDIUM), kw = text_width(keys, 12, DWRITE_FONT_WEIGHT_NORMAL);
    const float rnw = text_width(rname, 14, DWRITE_FONT_WEIGHT_MEDIUM), rkw = text_width(rkeys, 12, DWRITE_FONT_WEIGHT_NORMAL);
    // *and "Open OrrePort ↗" last (4.4, 2026-11-23, snatcherLeo's "apps connecting" design):* it starts OrrePort from this
    // folder, or, while OrrePort is open, reads "OrrePort is opened ↗" and brings it forward (the hub's link says
    // "Studio is opened ↗" the same way, snatcherLeo's wording). Only with an OrrePort folder to start it from
    const bool has_open = !g_apps_root.empty();
    const std::wstring oname = tx(g_hub_open ? "studio.home.orreport_opened" : "studio.home.open_orreport") + L" ↗";
    const float onw = has_open ? text_width(oname, 14, DWRITE_FONT_WEIGHT_MEDIUM) : 0;
    const float bw = 16 + 22 + 10 + nw + 14 + kw + 16, rbw = 20 + rnw + 14 + rkw + 16, obw = has_open ? 20 + onw + 20 : 0, bh = 44, gap = 12;
    const float bx = std::round(C.x + (C.w - bw - gap - rbw - (has_open ? gap + obw : 0)) / 2), by = cy + 128, rbx = bx + bw + gap;
    if (has_open) {
        const float obx = rbx + rbw + gap;
        const bool ohov = g.hover == HIT_OPEN_ORREPORT;
        round_fill(obx, by, obw, bh, 10, ohov ? P.panel2 : P.card);
        round_line(obx, by, obw, bh, 10, ohov ? P.accent_line : P.line2);
        text(oname, obx + 20, by, onw + 4, bh, 14, g_hub_open ? P.accent2 : P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        hit({obx, by, obw, bh}, HIT_OPEN_ORREPORT);
    }
    const bool hov = g.hover == HIT_SETTINGS, rhov = g.hover == HIT_SET_REPORT;
    round_fill(bx, by, bw, bh, 10, hov ? P.panel2 : P.card);
    round_line(bx, by, bw, bh, 10, hov ? P.accent_line : P.line2);
    // (the label's colour, as the Settings tab draws it: snatcherLeo, 2026-11-22, "the tab for Settings icon is brighter than
    // the actual icon ... Make icon in screenshot 1 and 2 match")
    draw_gear(bx + 16, by + 11, 22, gear_colour());
    text(name, bx + 48, by, nw + 4, bh, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    text(keys, bx + 48 + nw + 14, by, kw + 4, bh, 12, P.faint);
    hit({bx, by, bw, bh}, HIT_SETTINGS);
    round_fill(rbx, by, rbw, bh, 10, rhov ? P.panel2 : P.card);
    round_line(rbx, by, rbw, bh, 10, rhov ? P.accent_line : P.line2);
    text(rname, rbx + 20, by, rnw + 4, bh, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    text(rkeys, rbx + 20 + rnw + 14, by, rkw + 4, bh, 12, P.faint);
    hit({rbx, by, rbw, bh}, HIT_SET_REPORT);
    // *Added 2026-11-24 (5.9's laptop sitting: with no disc, Home said nothing; only Settings › Discs showed "Not
    // added"):* while there is no disc, a strip under the buttons says so, with Add disc… (the US row's Add: the hub's
    // checks and words, then Settings › Discs for Copy / Move). While the disc is read it says so instead, and a disc
    // that couldn't be read gives its reason. It goes once a disc is in use. Drawn for every system (only the file
    // dialog behind the button is Windows'; Linux / macOS: their own dialog, likely)
    float strip = 0;
    if (!g.disc) {
        const float sw = std::min(700.0f, C.w - 80), sx = std::round(C.x + (C.w - sw) / 2), sy = by + bh + 24, sh = 64;
        round_fill(sx, sy, sw, sh, 12, P.card);
        round_line(sx, sy, sw, sh, 12, P.accent_line);
        const std::wstring line = g.loading ? tx("studio.disc.reading")
                                  : g.load_error.empty() ? tx("studio.home.no_disc")
                                                         : tx("studio.disc.error", {{"reason", widen(g.load_error)}});
        float tw = sw - 40;
        if (!g.loading) {
            const std::wstring b = tx("studio.home.add_disc");
            const float abw = text_width(b, 14, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 36, abx = sx + sw - abw - 12, aby = sy + 12;
            round_fill(abx, aby, abw, 40, 10, g.hover == HIT_HOME_ADD_DISC ? alpha(P.accent, 0.85f) : P.accent);
            text(b, abx, aby, abw, 40, 14, P.on_accent, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            hit({abx, aby, abw, 40}, HIT_HOME_ADD_DISC);
            tw = abx - sx - 20 - 16;
        }
        text(line, sx + 20, sy, tw, sh, 13, P.text, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
        strip = sh + 24;
    }
    // the update line under them (4.4), only while there is one to tell about
    draw_update_line(C.x + C.w / 2, by + bh + 28 + strip, std::min(560.0f, C.w - 80));
}

// ---- Settings (2026-11-19): a page of panes, Appearance first ----
// snatcherLeo: "I reckon it'll be multi-pane, since there'll be several settings users can possibly change. For now, just
// have a pane for "Appearance" and have light/dark mode slider (or whatever makes sense) in there". The panes are a
// list on the left (as Windows' and most apps' settings are), the chosen pane's settings on the right. The theme is a
// three-way switch, Match Windows / Light / Dark (the choices OrrePort's own Theme setting has), not a two-way slider:
// following Windows is a choice of its own. Each pane, setting and choice has a name in studio/lang (translated) and
// a stable id (its hit-spot number), for the UI Editor later.
// ---- Settings › Discs (4.4, 2026-11-23) ----
// snatcherLeo: "A, Studio gets the same buttons ... would it be in Settings where you add it", then, after the mockups, "just
// make it ... more concise and simple ... Keep minimal info and what users actually need" and "Yes, build it". Decided
// with snatcherLeo (docs/port/disc-sync-review.md): ROM\ is the one place for both apps' discs (the US disc in
// Pokemon Colosseum\ROM\, the European and Japanese ones in ROM Other Regions\), discs are added the hub's way (identify,
// then Copy into OrrePort or Move, the hub's words), and both apps watch those folders once a second while open, so a
// disc added, replaced or removed in one app shows in the other with no second step.
// - Add / Replace: the file dialog, then the picked file is opened to read its game id (a European or Japanese disc
//   picked in the other language row goes to its own row). Copy or Move runs on a worker thread into "<name>.part"
//   (both apps' scans skip ".part"), then the replaced disc goes to the Recycle Bin and the ".part" is renamed. The new
//   file's change time is set to now, so the shared rule (the newest wins) picks it even when its own date is older.
// - Remove: asks first, then the Recycle Bin (never a permanent delete; Windows warns if the drive has no Recycle Bin).
// - The watch (discs_watch, from the 1 s timer): the folders' names, sizes and times; on a change the rows are read
//   again and a new US disc is loaded (tabs kept through the session file). A removed US disc stays in use until Studio
//   closes (its file handle shares delete: reading goes on from the Recycle Bin, likely), the next start asks for one. The
//   old disc handle is kept, not closed, while background reads may still use it (g_retired_discs, closed at exit).
// Windows only (CopyFileEx, MoveFileWithProgress, SHFileOperation's Recycle Bin, FindFirstFile); Linux / macOS: the same
// steps with the file manager's trash (gio trash / NSFileManager trashItemAtURL) and copy with progress (likely).
void save_session(const std::wstring& to = L"");  // (the session file: further down)
constexpr int kDiscRows = 3;
const char* const kDiscIds[kDiscRows] = {"GC6E01", "GC6P01", "GC6J01"};
// (the row words are OrrePort's shared discs.* since 2026-11-23: the hub's Discs panel took Studio's wording, Main
// c4de3f0b7, so both programs say the same; Studio keeps studio.discs.shared and in_use_gone, its own)
const char* const kDiscNames[kDiscRows] = {"discs.us", "discs.eu", "discs.jp"};
struct DiscsUi {
    std::wstring path[kDiscRows];  // each row's disc in the folders now ("" = not added)
    std::wstring bad[kDiscRows];   // a disc of the row's game id put there by hand whose program file isn't the original's
    // (snatcherLeo, 2026-11-23: what if someone points Studio at the wrong disc while tabs, maybe a mod's work, are open? Studio
    // never shuts down and never switches to it: the disc in use stays, the row shows the file in red with the reason)
    std::wstring sig;              // the folders as last read (names, sizes, times)
    int busy = -1;                 // the row a copy or move runs for
    bool busy_move = false;
    std::atomic<int> pct{0};
    int pick = -1;                 // the row whose found disc waits for Copy / Move
    std::wstring pick_path;
    unsigned long long pick_bytes = 0;
    int confirm = -1;              // the row whose Remove waits for its confirm
    int msg_row = -1;              // the last result, under its row
    std::wstring msg;
    bool msg_err = false;
    HWND shown_in = nullptr;       // the window showing the pane (discs_left: leaving it clears the result line)
    bool gone_logged = false;      // "the US disc was removed" already said in the log
    bool wait_logged = false;      // "a disc image is still being copied in" already said for this copy
    bool gone_hidden = false;      // the removed-disc bar's OK or Discs was clicked (shown again after the next removal)
    bool bar_shown = false;        // the removed-disc bar as last laid out (discs_apply lays the windows out on a change)
};
DiscsUi g_discs;
std::vector<OrreDisc*> g_retired_discs;
struct DiscOpResult { int row; bool move; bool ok; std::wstring error, file; };

std::wstring disc_folder(int row) {
    const std::wstring base = g.orreport_root.empty() ? g.orreport_dir + L"\\Colosseum" : g.orreport_root + L"\\Pokemon Colosseum";
    return base + (row == 0 ? L"\\ROM" : L"\\ROM Other Regions");
}
std::wstring file_name(const std::wstring& p) { return p.substr(p.find_last_of(L"\\/") + 1); }
std::wstring gb_text(unsigned long long bytes) {
    wchar_t b[32];
    swprintf(b, 32, L"%.1f", bytes / 1073741824.0);
    return tx("discs.size", {{"gb", b}});
}
std::wstring system_error(DWORD e) {
    wchar_t b[256] = L"";
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, b, 256, nullptr);
    std::wstring s = b;
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L'.')) s.pop_back();
    return s;
}
void repaint_all() { for (const WindowSlot& w : g_windows) InvalidateRect(w.hwnd, nullptr, FALSE); }
void discs_say(int row, const std::wstring& m, bool err) {
    g_discs.msg_row = row;
    g_discs.msg = m;
    g_discs.msg_err = err;
    repaint_all();
}
// the two folders' listing in one string (a change in any name, size or time shows as a different string)
std::wstring discs_signature() {
    std::wstring s;
    for (int k = 0; k < 2; ++k) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((disc_folder(k) + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) { s += L"|none"; continue; }
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wchar_t b[96];
            swprintf(b, 96, L"|%lu:%lu:%lu:%lu|", fd.nFileSizeHigh, fd.nFileSizeLow, fd.ftLastWriteTime.dwHighDateTime, fd.ftLastWriteTime.dwLowDateTime);
            s += fd.cFileName;
            s += b;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        s += L"#";
    }
    return s;
}
// The rows as the folders hold them: the shared rule with the program file checked (orre_disc_find_checked): a modified
// disc put into the folder by hand is never used, the newest original is, and the refused one is named in its row.
// Reads every disc image in the folders (each one opened, its program file fingerprinted), so it may take a while on a
// slow drive: the watch runs it on a worker thread (discs_watch); it touches no window state, so it is safe there
struct DiscsRead { std::wstring path[kDiscRows], bad[kDiscRows]; unsigned gen = 0; bool act = false; };
DiscsRead discs_read(const std::wstring (&folders)[kDiscRows]) {
    DiscsRead r;
    for (int k = 0; k < kDiscRows; ++k) {
        char out[2048], refused[2048];
        orre_disc_find_checked(narrow(folders[k]).c_str(), kDiscIds[k], out, sizeof out, refused, sizeof refused);
        r.path[k] = widen(out);
        r.bad[k] = widen(refused);
    }
    return r;
}
// a reading's generation: every reading done on the window's thread counts one up, so a background reading started
// before it (and so older) is dropped when it arrives instead of putting back what the folders held before
unsigned g_discs_gen = 0;
bool g_discs_reading = false;  // (a background reading is running: the watch starts no second one)
void discs_apply(const DiscsRead& r, bool act);
// the rows read again, and what changed acted on: a new US disc is loaded, a changed Japanese disc is opened afresh
void discs_scan(bool act) {
    std::wstring folders[kDiscRows];
    for (int k = 0; k < kDiscRows; ++k) folders[k] = disc_folder(k);
    ++g_discs_gen;
    discs_apply(discs_read(folders), act);
}
void discs_apply(const DiscsRead& r, bool act) {
    // logged only when a row changes (a disc being copied in by hand changes the folder every second until it is done)
    for (int k = 0; k < kDiscRows; ++k) {
        if (r.bad[k] != g_discs.bad[k] && !r.bad[k].empty())
            logf(Level::Warn, "disc", "%s folder: %s is a %s whose program file isn't the original release's: not used",
                 k == 0 ? "ROM" : "ROM Other Regions", narrow(file_name(r.bad[k])).c_str(), kDiscIds[k]);
        g_discs.path[k] = r.path[k];
        g_discs.bad[k] = r.bad[k];
    }
    if (!act) return;
    const std::wstring& us = g_discs.path[0];
    if (!g.disc_forced && !g.loading) {
        if (!us.empty() && _wcsicmp(us.c_str(), g.disc_path.c_str()) != 0) {
            logf(Level::Info, "disc", "ROM folder: the US disc is now %s; reading it", narrow(file_name(us)).c_str());
            if (g.disc) save_session(L"");  // (the reload brings the tabs back from it)
            // the first disc while Studio is open (added in Settings › Discs, by the hub, or copied in by hand): Studio
            // stays on the page it shows, so Settings' "US disc added." stays in sight (5.9's laptop sitting,
            // 2026-11-24: Studio used to jump into the Pokémon editor here, which wasn't wanted). A disc switched for
            // another one keeps the earlier behaviour: its tabs come back
            else g.load_stays = true;
            g.disc_path = us;
            g.load_error.clear();
            start_loading();  // (the old disc is kept open until Studio closes: WM_LIST_LOADED retires it)
        } else if (us.empty() && !g.disc_path.empty() && g.disc && !g_discs.gone_logged) {
            g_discs.gone_logged = true;  // (said once)
            logf(Level::Info, "disc", "ROM folder: %s; Studio keeps reading the one it has open until it closes",
                 g_discs.bad[0].empty() ? "the US disc was removed" : "the US disc there isn't the original");
        }
        if (!us.empty()) g_discs.gone_logged = false, g_discs.gone_hidden = false;
    }
    // the removed-disc bar came or went: every window lays out again (it takes a strip above the page, and the 3D view,
    // a child window, moves with the page)
    const bool bar = disc_gone_bar();
    if (bar != g_discs.bar_shown) {
        g_discs.bar_shown = bar;
        if (bar) logf(Level::Info, "disc", "the removed-disc bar is shown: Studio keeps reading the disc it has open");
        else if (!g_discs.path[0].empty()) logf(Level::Info, "disc", "the removed-disc bar went: a US disc is back in the ROM folder");
        const HWND back = g.hwnd;
        for (size_t k = 0; k < g_windows.size(); ++k) {
            activate(g_windows[k].hwnd);
            place_children();
        }
        activate(back);
    }
    if (g.region_disc_arg.empty() && g.jp_tried && _wcsicmp(g_discs.path[2].c_str(), g.jp_path.c_str()) != 0) {
        logf(Level::Info, "disc", "ROM Other Regions folder: the Japanese disc %s", g_discs.path[2].empty() ? "was removed" : "changed");
        if (g.jp_disc) g_retired_discs.push_back(g.jp_disc);
        g.jp_disc = nullptr;
        g.jp_tried = false;  // (opened afresh the next time a style needs it)
        g.jp_path.clear();
    }
    repaint_all();
}
// Is a disc image in the folders still being written (a copy into them by hand, File Explorer's or another program's)?
// Why (5.9's laptop sitting, 2026-11-24): the hub froze when a disc was copied into ROM\ by hand while it was open,
// reading the half-written file on its window's thread. A file open for writing refuses a reader that doesn't share
// writing (ERROR_SHARING_VIOLATION), so this asks for read access sharing only reading: it fails while the copy holds
// the file and works once it is done. Readers (Studio's own open disc, the hub's, Dolphin's) don't block it. ".part"
// files are skipped (the apps' own copies in progress; no scan uses them). Windows only (share modes); Linux / macOS
// have no such lock: there the watch would wait until the file's size and time stop changing for a second or two
// (likely), the same idea
bool discs_being_written(const std::wstring (&folders)[kDiscRows]) {
    for (int k = 0; k < 2; ++k) {  // (rows 1 and 2 share ROM Other Regions)
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((folders[k] + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        bool writing = false;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            const size_t n = wcslen(fd.cFileName);
            if (n > 5 && _wcsicmp(fd.cFileName + n - 5, L".part") == 0) continue;
            HANDLE f = CreateFileW((folders[k] + L"\\" + fd.cFileName).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
            if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
            else if (GetLastError() == ERROR_SHARING_VIOLATION) writing = true;
        } while (!writing && FindNextFileW(h, &fd));
        FindClose(h);
        if (writing) return true;
    }
    return false;
}
// once a second (perf_timer): the folders read again only when they changed, and not while Studio's own copy runs.
// *Changed 2026-11-24 (the hub's freeze above):* the reading runs on a worker thread (discs_read opens every image),
// and waits while an image is still being written; the folders' new state is only taken once it is read, so the next
// second looks again
void discs_watch() {
    if (g_discs.busy >= 0 || g.loading || g_discs_reading) return;
    std::wstring s = discs_signature();
    if (s == g_discs.sig) return;
    std::wstring folders[kDiscRows];
    for (int k = 0; k < kDiscRows; ++k) folders[k] = disc_folder(k);
    if (discs_being_written(folders)) {  // (not yet: the copy is still going; said once per copy)
        if (!g_discs.wait_logged) logf(Level::Info, "disc", "ROM folders: a disc image is still being copied in; read once it is done");
        g_discs.wait_logged = true;
        return;
    }
    g_discs.wait_logged = false;
    const bool first = g_discs.sig.empty();
    g_discs.sig = std::move(s);
    g_discs_reading = true;
    const unsigned gen = g_discs_gen;
    HWND hwnd = g.hwnd;
    std::thread([folders, gen, first, hwnd] {
        auto* r = new DiscsRead(discs_read(folders));
        r->gen = gen;
        r->act = !first;
        if (!PostMessageW(hwnd, WM_DISCS_READ, 0, (LPARAM)r)) delete r;
    }).detach();
}
// the worker's reading, back on the window's thread: used unless a reading on this thread came after it
void discs_read_done(DiscsRead* r) {
    std::unique_ptr<DiscsRead> res(r);
    g_discs_reading = false;
    if (r->gen != g_discs_gen) return;  // (older than what the rows show now)
    discs_apply(*r, r->act);
}
// Windows' Recycle Bin (FOF_ALLOWUNDO; FOF_WANTNUKEWARNING still asks when the drive has none, so nothing is deleted for
// good without the user's word)
bool recycle(const std::wstring& path) {
    std::wstring from = path;
    from.push_back(L'\0');  // (a list ending in two zeros)
    SHFILEOPSTRUCTW op{};
    op.hwnd = g.hwnd;
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING | FOF_SILENT | FOF_NOERRORUI;
    return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}
void discs_check(int row, const std::wstring& p);
// Add or Replace: pick a file, read its game id, and offer Copy / Move in its row
void discs_add(int row) {
    g_discs.confirm = -1;
    const std::wstring p = pick_disc_file();
    if (!p.empty()) discs_check(row, p);
}
// the picked file checked for that row (the hub's checks and words), then its Copy / Move question
void discs_check(int row, const std::wstring& p) {
    g_discs.confirm = -1;
    // *Updated 2026-11-23 (snatcherLeo: "Yes, refuse all three, use the hub's wording. Hub and Studio should share exact
    // wording for these things"):* the messages are OrrePort's own lines (disc.*, hub.lang.*, check.*: lang_load_shared),
    // the US row's as the hub's US disc says them, the language rows' as its language-disc panel does; and a Colosseum
    // disc whose program file isn't the original release's is refused in every row (orre_disc_is_original), as OrrePort
    // refuses such a US disc before the game starts
    const std::wstring ext = p.size() > 4 ? p.substr(p.size() - 4) : L"";
    const bool archive = _wcsicmp(ext.c_str(), L".zip") == 0 || _wcsicmp(ext.c_str(), L".rar") == 0 || _wcsicmp(ext.c_str(), L".7z") == 0 ||
                         (p.size() > 3 && _wcsicmp(p.c_str() + p.size() - 3, L".7z") == 0);
    char err[512] = "";
    OrreDisc* d = orre_disc_open(narrow(p).c_str(), err, sizeof err);
    if (!d) {
        discs_say(row, tx(archive ? "disc.archive" : row == 0 ? "disc.unreadable" : "hub.lang.unreadable"), true);
        logf(Level::Info, "disc", "Add: %s isn't a readable disc (%s)", narrow(file_name(p)).c_str(), err);
        return;
    }
    const std::string id = orre_disc_game_id(d);
    const int original = orre_disc_is_original(d);
    orre_disc_close(d);
    const int target = id == kDiscIds[0] ? 0 : id == kDiscIds[1] ? 1 : id == kDiscIds[2] ? 2 : -1;
    logf(Level::Info, "disc", "Add (%s row): %s is %s%s", row == 0 ? "US" : row == 1 ? "European" : "Japanese", narrow(file_name(p)).c_str(),
         id.c_str(), original == 0 ? ", its program file not the original release's" : "");
    // the game's name as OrrePort names it (disc.name.*: XD's three, the Colosseum bonus discs, the other Colosseums)
    auto game_name = [&]() {
        const std::string p4 = id.substr(0, 4);
        if (p4 == "PC6E" || p4 == "PC6J" || p4 == "PC6P" || p4 == "PCSE" || p4 == "PCSJ") return tx("disc.name.bonus");
        const std::string key = "disc.name." + p4;
        return has_text(key.c_str()) ? tx(key.c_str()) : tx("disc.name.unknown");
    };
    if (row == 0 && (target == 1 || target == 2)) {
        const std::string p4 = id.substr(0, 4), lang = "disc.languages." + p4;
        discs_say(0, tx("disc.wrong.region", {{"game", game_name()}, {"id", widen(id)}, {"languages", tx(lang.c_str())}}), true);
        return;
    }
    if (row == 0 && target != 0) { discs_say(0, tx("disc.wrong", {{"game", game_name()}, {"id", widen(id)}}), true); return; }
    if (row > 0 && target == 0) { discs_say(row, tx("hub.lang.us_disc"), true); return; }
    if (row > 0 && target < 0) { discs_say(row, tx("hub.lang.not_lang", {{"id", widen(id)}}), true); return; }
    if (original == 0) {
        discs_say(target, target == 0 ? tx("check.dol.text") + L" " + tx("check.verify_hint")
                                       : tx("hub.lang.not_original", {{"disc", tx(kDiscNames[target])}}) + L" " + tx("check.verify_hint"),
                  true);
        return;
    }
    if (!g_discs.path[target].empty() && _wcsicmp(g_discs.path[target].c_str(), p.c_str()) == 0) {
        discs_say(target, tx("hub.lang.already", {{"disc", tx(kDiscNames[target])}}), false);
        return;
    }
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa);
    g_discs.pick = target;
    g_discs.pick_path = p;
    g_discs.pick_bytes = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    g_discs.msg_row = -1;
    repaint_all();
}
// Copy into OrrePort or Move: on a worker thread, into "<name>.part", then the replaced disc to the Recycle Bin and the
// part renamed; WM_DISC_OP_DONE reports back
void discs_start(bool move) {
    const int row = g_discs.pick;
    if (row < 0) return;
    g_discs.pick = -1;
    g_discs.busy = row;
    g_discs.busy_move = move;
    g_discs.pct = 0;
    g_discs.msg_row = -1;
    const std::wstring src = g_discs.pick_path, folder = disc_folder(row),
                       old = g_discs.path[row].empty() ? g_discs.bad[row] : g_discs.path[row];  // (a refused disc is replaced too)
    const unsigned long long bytes = g_discs.pick_bytes;
    HWND hwnd = g.hwnd;
    SetTimer(hwnd, TIMER_DISCOP, 100, nullptr);
    logf(Level::Info, "disc", "%s %s into the %s folder", move ? "moving" : "copying", narrow(file_name(src)).c_str(), row == 0 ? "ROM" : "ROM Other Regions");
    std::thread([src, folder, old, bytes, row, move, hwnd] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);  // (SHFileOperation's Recycle Bin wants COM on its thread)
        auto* r = new DiscOpResult{row, move, false, L"", file_name(src)};
        SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);
        const std::wstring dest = folder + L"\\" + file_name(src), part = dest + L".part";
        wchar_t sv[MAX_PATH] = L"", dv[MAX_PATH] = L"";
        GetVolumePathNameW(src.c_str(), sv, MAX_PATH);
        GetVolumePathNameW(folder.c_str(), dv, MAX_PATH);
        const bool same_volume = _wcsicmp(sv, dv) == 0;
        ULARGE_INTEGER free_bytes{};
        GetDiskFreeSpaceExW(folder.c_str(), &free_bytes, nullptr, nullptr);
        if (!(move && same_volume) && free_bytes.QuadPart < bytes + 64ull * 1048576) {
            r->error = tx("discs.no_space", {{"size", gb_text(bytes)}});
        } else {
            DeleteFileW(part.c_str());  // (a part left by an interrupted copy: Studio's own temporary file)
            auto progress = [](LARGE_INTEGER total, LARGE_INTEGER done, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE,
                               LPVOID) -> DWORD {
                if (total.QuadPart > 0) g_discs.pct = (int)(done.QuadPart * 100 / total.QuadPart);
                return PROGRESS_CONTINUE;
            };
            const BOOL ok = move ? MoveFileWithProgressW(src.c_str(), part.c_str(), progress, nullptr, MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)
                                 : CopyFileExW(src.c_str(), part.c_str(), progress, nullptr, nullptr, 0);
            if (!ok) {
                r->error = system_error(GetLastError());
                DeleteFileW(part.c_str());
            } else {
                // the replaced disc to the Recycle Bin, then the new one under its own name, dated now (the newest wins)
                if (!old.empty()) {
                    std::wstring from = old;
                    from.push_back(L'\0');
                    SHFILEOPSTRUCTW op{};
                    op.wFunc = FO_DELETE;
                    op.pFrom = from.c_str();
                    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
                    SHFileOperationW(&op);
                }
                if (!MoveFileExW(part.c_str(), dest.c_str(), MOVEFILE_WRITE_THROUGH)) {
                    r->error = system_error(GetLastError());
                } else {
                    HANDLE f = CreateFileW(dest.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                           OPEN_EXISTING, 0, nullptr);
                    if (f != INVALID_HANDLE_VALUE) {
                        FILETIME now;
                        GetSystemTimeAsFileTime(&now);
                        SetFileTime(f, nullptr, nullptr, &now);
                        CloseHandle(f);
                    }
                    r->ok = true;
                }
            }
        }
        CoUninitialize();
        if (!PostMessageW(hwnd, WM_DISC_OP_DONE, 0, (LPARAM)r)) delete r;
    }).detach();
    repaint_all();
}
void discs_done(DiscOpResult* r) {
    std::unique_ptr<DiscOpResult> res(r);
    KillTimer(g.hwnd, TIMER_DISCOP);
    g_discs.busy = -1;
    const std::wstring disc = tx(kDiscNames[r->row]);
    if (r->ok) {
        logf(Level::Info, "disc", "%s %s: done", r->move ? "moved" : "copied", narrow(r->file).c_str());
        discs_say(r->row, tx("discs.added", {{"disc", disc}}), false);
    } else {
        logf(Level::Warn, "disc", "%s %s failed: %s", r->move ? "moving" : "copying", narrow(r->file).c_str(), narrow(r->error).c_str());
        discs_say(r->row, tx(r->move ? "discs.move_failed" : "discs.copy_failed", {{"error", r->error}}), true);
    }
    g_discs.sig = discs_signature();
    discs_scan(true);
}
void discs_remove(int row) {
    g_discs.confirm = -1;
    const std::wstring p = g_discs.path[row].empty() ? g_discs.bad[row] : g_discs.path[row], disc = tx(kDiscNames[row]);
    if (p.empty()) return;
    const bool ok = recycle(p);
    // *Added 2026-11-24 (5.9's laptop sitting, the two-way test):* with OrrePort open, Remove failed, because OrrePort
    // holds the image open without letting it be moved (its own Remove lets go first: the hub restarts). The plain "couldn't
    // be moved" didn't say why or what to do, so while OrrePort or its game runs from this folder the line says so (the
    // wording approved through Main: "OrrePort is using the US disc. Remove it from OrrePort, or close OrrePort first.")
    const bool held = !ok && (g_hub_open || g_game_running);
    logf(ok ? Level::Info : Level::Warn, "disc", "Remove %s: %s", narrow(file_name(p)).c_str(),
         ok ? "in the Recycle Bin" : held ? "couldn't be moved to the Recycle Bin (OrrePort is open, likely holding it)" : "couldn't be moved to the Recycle Bin");
    discs_say(row, tx(ok ? "discs.removed" : held ? "studio.discs.remove_in_use" : "discs.remove_failed", {{"disc", disc}}), !ok);
    g_discs.sig = discs_signature();
    discs_scan(true);
}

// The Discs pane: one card, a row per disc (its name; the file or "Not added"; Add, or Replace and Remove), the row's
// Copy / Move choice, progress or confirm under it when there is one, and "Shared with OrrePort." under the card
// The result line ("Japanese disc added.") and an unanswered Copy / Move or Remove question go when the pane is left
// (another pane, page or tab: snatcherLeo, 2026-11-23, "that stuff goes away if you get out of that screen and go back,
// right?"); a copy or move in progress keeps its progress. Called at the start of each window's paint.
void discs_left() {
    if (g_discs.shown_in != g.hwnd) return;
    if (cur_page() == kSettings && g.settings_pane == 1 && g.editor == 0) return;
    g_discs.shown_in = nullptr;
    g_discs.msg_row = g_discs.pick = g_discs.confirm = -1;
    g_discs.msg.clear();
}
void draw_discs(float rx, float ly, float rw) {
    const Palette& P = g.pal;
    g_discs.shown_in = g.hwnd;
    text(tx("studio.settings.discs"), rx, ly, rw, 26, 16, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    const float cy0 = ly + 34, base = 68;
    // each row's notes under it, in order: a refused disc's reason (red), the US disc removed while in use, the last
    // result (red for an error); OrrePort's long lines (disc.wrong.region, check.dol.text) wrap, so each is measured
    auto wrapped_h = [&](const std::wstring& s) {
        float hh = 18;
        if (IDWriteTextFormat* f = format(12, DWRITE_FONT_WEIGHT_NORMAL)) {
            f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            IDWriteTextLayout* l = nullptr;
            g.dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, rw - 40, 2000, &l);
            if (l) { DWRITE_TEXT_METRICS m{}; l->GetMetrics(&m); hh = m.height; l->Release(); }
            f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return hh + 8;
    };
    std::vector<std::pair<std::wstring, D2D1_COLOR_F>> notes[kDiscRows];
    const D2D1_COLOR_F red = hexc(0xE0705E);
    for (int k = 0; k < kDiscRows; ++k) {
        if (!g_discs.bad[k].empty()) {
            // (the refused file is named when a good disc is in use beside it; alone, it is the row's file line)
            const std::wstring why = k == 0 ? tx("check.dol.text") : tx("hub.lang.not_original", {{"disc", tx(kDiscNames[k])}});
            notes[k].push_back({g_discs.path[k].empty() ? why : file_name(g_discs.bad[k]) + L": " + why, red});
        }
        if (k == 0 && g_discs.path[0].empty() && g.disc && !g.disc_forced) notes[k].push_back({tx("studio.discs.in_use_gone"), P.mute});
        if (g_discs.msg_row == k && !g_discs.msg.empty()) notes[k].push_back({g_discs.msg, g_discs.msg_err ? red : P.accent2});
    }
    float h[kDiscRows];
    for (int k = 0; k < kDiscRows; ++k) {
        h[k] = base;
        if (g_discs.pick == k) h[k] += 96;
        if (g_discs.busy == k) h[k] += 34;
        if (g_discs.confirm == k) h[k] += 74;
        for (const auto& n : notes[k]) h[k] += wrapped_h(n.first);
    }
    const float total = h[0] + h[1] + h[2];
    round_fill(rx, cy0, rw, total, 12, P.card);
    round_line(rx, cy0, rw, total, 12, P.line);
    // a button: right-aligned at `right`, returns its left edge
    auto button = [&](float right, float y, const std::wstring& label, int id, bool primary) {
        const float bw = text_width(label, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 28, bx = right - bw;
        const bool hov = g.hover == id;
        round_fill(bx, y, bw, 32, 8, primary ? (hov ? alpha(P.accent, 0.2f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(bx, y, bw, 32, 8, primary ? P.accent_line : P.line2);
        text(label, bx, y, bw, 32, 12.5f, primary ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx, y, bw, 32}, id);
        return bx;
    };
    float y = cy0;
    const bool idle = g_discs.busy < 0;
    for (int k = 0; k < kDiscRows; ++k) {
        if (k) fill(rx + 16, y, rw - 32, 1, P.line);
        const std::wstring& p = g_discs.path[k];
        const std::wstring& bad = g_discs.bad[k];  // (a refused disc: Replace and Remove, no folder button)
        float left = rx + rw - 20;
        if (idle && g_discs.pick != k && g_discs.confirm != k) {
            if (p.empty() && bad.empty()) left = button(left, y + 18, tx("discs.add"), HIT_DISC_ADD + k, true);
            else {
                left = button(left, y + 18, tx("discs.remove"), HIT_DISC_REMOVE + k, false);
                left = button(left - 8, y + 18, tx("discs.replace"), HIT_DISC_ADD + k, false);
            }
        }
        if (!p.empty()) {
            // the folder button (snatcherLeo, 2026-11-23: "a basic folder icon, "See file location" ... a hoverable button they
            // can click to open up file explorer and see where the ISO is"): OrrePort's own folder.svg (the hub's),
            // tinted like the text. No hover card (snatcherLeo, after trying it: "no hoverable tooltip needed ... the icon alone
            // tells the user very clearly"). Shown only for a disc the folder scan recognised (the right game id): a
            // wrong file picked in Add / Replace is never copied, and its row says so in red instead
            const float fx = left - 8 - 32, fy = y + 18;
            const bool hov = g.hover == HIT_DISC_SHOW + k;
            round_fill(fx, fy, 32, 32, 8, hov ? P.hover : P.bg);
            round_line(fx, fy, 32, 32, 8, P.line2);
            draw_svg(g.ui_dir + L"\\icons\\folder.svg", {{"#FFFFFF", hex_colour(hov ? P.text : P.text2)}}, fx + 7, fy + 7, 18);
            hit({fx, fy, 32, 32}, HIT_DISC_SHOW + k);
            left = fx;
        }
        text(tx(kDiscNames[k]), rx + 20, y + 14, left - rx - 36, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        text(!p.empty() ? file_name(p) : !bad.empty() ? file_name(bad) : tx("discs.not_added"), rx + 20, y + 38, left - rx - 36, 20, 12.5f,
             !p.empty() ? P.mute : !bad.empty() ? red : P.faint);
        float sy = y + base;
        if (g_discs.pick == k) {
            const std::wstring sz = gb_text(g_discs.pick_bytes);
            text(tx("discs.found", {{"disc", tx(kDiscNames[k])}, {"file", file_name(g_discs.pick_path)}, {"size", sz}}), rx + 20, sy - 6,
                 rw - 40, 20, 12.5f, P.text);
            text(tx("discs.choice", {{"size", sz}}), rx + 20, sy + 16, rw - 40, 20, 12.5f, P.mute);
            float bx = button(rx + rw - 20, sy + 46, tx("common.cancel"), HIT_DISC_CANCEL, false);
            bx = button(bx - 8, sy + 46, tx("discs.move"), HIT_DISC_MOVE, false);
            button(bx - 8, sy + 46, tx("discs.copy"), HIT_DISC_COPY, true);
            sy += 96;
        }
        if (g_discs.busy == k) {
            const int pct = g_discs.pct;
            text(tx(g_discs.busy_move ? "discs.moving" : "discs.copying", {{"disc", tx(kDiscNames[k])}, {"percent", std::to_wstring(pct)}}),
                 rx + 20, sy - 6, rw - 40, 20, 12.5f, P.mute);
            round_fill(rx + 20, sy + 20, rw - 40, 4, 2, P.line);
            round_fill(rx + 20, sy + 20, (rw - 40) * pct / 100.0f, 4, 2, P.accent);
            sy += 34;
        }
        if (g_discs.confirm == k) {
            text(tx(k == 0 ? "discs.remove_ask_us" : "discs.remove_ask", {{"disc", tx(kDiscNames[k])}}), rx + 20, sy - 6, rw - 40, 36,
                 12.5f, P.text, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
            const float bx = button(rx + rw - 20, sy + 34, tx("common.cancel"), HIT_DISC_CANCEL, false);
            button(bx - 8, sy + 34, tx("discs.remove"), HIT_DISC_CONFIRM, true);
            sy += 74;
        }
        for (const auto& [s, c] : notes[k]) {
            const float nh = wrapped_h(s);
            text(s, rx + 20, sy - 6, rw - 40, nh, 12, c, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
            sy += nh;
        }
        y += h[k];
    }
    text(tx("studio.discs.shared"), rx + 4, cy0 + total + 10, rw - 8, 20, 11.5f, P.faint);
}

// Settings › About (4.4; snatcherLeo approved the mockup: "I'm fine in settings, and how users can download updates from
// settings"): one card. The version both programs share, with what the updater found and its button (Check for updates,
// Download, Restart now); "Download updates automatically" (off by default: snatcherLeo, "Should we allow users the option to
// download updates ahead of times, if they don't want to wait?"); "Check for updates" (off: fully offline); and "Go
// back to <previous>" once there is one, asking first. The switches are OrrePort's shared settings (settings.ini).
// The version "Go back" returns to, trimmed (the module read version.txt whole, its line break included, and the help
// line broke after "0.1.0"; snatcherLeo's update hand check, 2026-11-23; Main trims it at the source too since 372071698)
std::wstring goback_version() {
    std::wstring prev = widen(g_upd.previous);
    while (!prev.empty() && (prev.back() == L'\n' || prev.back() == L'\r' || prev.back() == L' ')) prev.pop_back();
    return prev;
}
// "Go back" is offered while a version is kept and it is older than this one (above, version_newer)
bool goback_offered() {
    return g_upd_on && g_upd.previous[0] && !version_newer(narrow(goback_version()).c_str(), g_upd.current);
}
void draw_about(float rx, float ly, float rw) {
    const Palette& P = g.pal;
    text(tx("studio.settings.about"), rx, ly, rw, 26, 16, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    const bool back = goback_offered();
    const float row = 78, ver_h = 100, logs_h = 96, cy0 = ly + 34, back_h = back ? row : 0;
    const float ch = ver_h + row * 2 + back_h + logs_h;  // (the version, two switches, Go back, the logs folder)
    // *Changed 2026-11-23 (snatcherLeo, both programs' About alike: "use the minimal "Folder" icon ... and have clicking THAT
    // be the way you open the logs folder"; for the version, "a button ... that is faded out, unless there is an option,
    // and somehow indicate ... that an update is available"; "It'll only say "Version XX is available" if that button is
    // hoverable and clickable as well"):* the version row and the logs row end in the same 32 x 32 icon button (OrrePort's
    // folder.svg, download.svg, restart.svg, tinted like the text), faded and not clickable when there's nothing to do
    // *Resized 2026-11-23 (snatcherLeo, on OrrePort's: "Make them bigger, so that, their left-edge starts at where the sliders
    // are" and, for Studio, "ensure they look exactly the same ... I care about icons"):* OrrePort's measures: the
    // switches' size and column, 56 x 32, right-aligned so the left edges line up; corner 8, a 1-DIP border, the icon 20
    // centred; faded to 35% when there's nothing to do (the colour mixed toward the card, as an opacity would show it)
    auto mix = [](D2D1_COLOR_F a, D2D1_COLOR_F b, float t) { return D2D1::ColorF(b.r + (a.r - b.r) * t, b.g + (a.g - b.g) * t, b.b + (a.b - b.b) * t, 1.0f); };
    auto icon_button = [&](float right, float y, const wchar_t* svg, int id, bool active) {
        const float bw = 56, bh = 32, bx = right - bw;
        const bool hov = active && g.hover == id;
        round_fill(bx, y, bw, bh, 8, hov ? P.hover : P.bg);
        round_line(bx, y, bw, bh, 8, active ? P.line2 : mix(P.line2, P.card, 0.35f));
        const D2D1_COLOR_F ic = !active ? mix(P.text2, P.card, 0.35f) : hov ? P.text : P.text2;
        draw_svg(g.ui_dir + L"\\icons\\" + svg, {{"#FFFFFF", hex_colour(ic)}}, bx + (bw - 20) / 2, y + (bh - 20) / 2, 20);
        if (active) hit({bx, y, bw, bh}, id);
        return bx;
    };
    round_fill(rx, cy0, rw, ch, 12, P.card);
    round_line(rx, cy0, rw, ch, 12, P.line);
    auto button = [&](float right, float y, const std::wstring& label, int id, bool primary) {
        const float bw = text_width(label, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 28, bx = right - bw;
        const bool hov = g.hover == id;
        round_fill(bx, y, bw, 32, 8, primary ? (hov ? alpha(P.accent, 0.2f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(bx, y, bw, 32, 8, primary ? P.accent_line : P.line2);
        text(label, bx, y, bw, 32, 12.5f, primary ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx, y, bw, 32}, id);
        return bx;
    };
    // a switch alone, right-aligned: 56 x 32 like the icon buttons (2026-11-23), so About's right-hand column lines up
    auto sw = [&](float right, float y, bool on, int id) {
        const bool hov = g.hover == id;
        round_fill(right - 56, y, 56, 32, 16, on ? P.accent : hov ? P.line2 : alpha(P.line2, 0.8f));
        g.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(on ? right - 16 : right - 40, y + 16), 11, 11), brush(on ? P.on_accent : P.text2));
        hit({right - 56, y, 56, 32}, id);
    };
    // 1. the version and what the updater says
    const PcUpdateState s = g_upd.state;
    const std::wstring ver = widen(g_upd_on && g_upd.current[0] ? g_upd.current : ORRE_VERSION), latest = widen(g_upd.latest);
    std::wstring status;
    if (!g_upd_on) status = tx("update.unavailable");
    else if (s == PC_UPDATE_OFF || !g.update_check) status = tx("update.off");
    else if (s == PC_UPDATE_CHECKING) status = tx("update.checking");
    else if (s == PC_UPDATE_UP_TO_DATE) status = tx("update.up_to_date");
    else if (s == PC_UPDATE_AVAILABLE) status = tx("update.available", {{"version", latest}});
    else if (s == PC_UPDATE_DOWNLOADING) status = tx("update.downloading", {{"version", latest}, {"percent", std::to_wstring(g_upd.size ? (int)(g_upd.done * 100 / g_upd.size) : 0)}});
    else if (s == PC_UPDATE_READY) status = tx("update.ready", {{"version", latest}});
    else if (s == PC_UPDATE_FAILED) status = tx(g_upd.error_key[0] ? g_upd.error_key : "update.failed");
    // the button: the download arrow while an update can be fetched (or a failed download tried again), the circular
    // arrow while one is ready and Studio can restart for it; faded otherwise (checking, downloading, up to date, off).
    // "Version X is available" and "Update ready: X" show only while it is clickable, in the accent colour; while it isn't,
    // the line says why (downloading, close OrrePort first, no launcher in this copy) in the plain colour
    const bool on = g_upd_on && g.update_check;
    const bool can_download = on && (s == PC_UPDATE_AVAILABLE || (s == PC_UPDATE_FAILED && g_upd.latest[0]));
    const bool can_restart = on && s == PC_UPDATE_READY && updates_can_restart();
    bool accent = can_download && s == PC_UPDATE_AVAILABLE;
    if (s == PC_UPDATE_READY) {
        if (can_restart) accent = true;
        else status = g_upd.other_running ? (g_game_running ? tx("update.wait_game") : tx("update.close_other", {{"program", L"OrrePort"}}))
                                          : tx("studio.update.restart_by_hand");
    }
    const D2D1_COLOR_F status_c = accent ? P.accent2 : s == PC_UPDATE_FAILED ? hexc(0xE0705E) : P.faint;
    const float left = s == PC_UPDATE_READY
                           ? icon_button(rx + rw - 20, cy0 + 22, L"restart.svg", HIT_UPD_RESTART, can_restart)
                           : icon_button(rx + rw - 20, cy0 + 22, L"download.svg", s == PC_UPDATE_FAILED ? HIT_UPD_RETRY : HIT_UPD_DOWNLOAD, can_download);
    text(tx("update.version", {{"version", ver}}), rx + 20, cy0 + 14, left - rx - 36, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    text(tx("update.shared_version"), rx + 20, cy0 + 36, left - rx - 36, 18, 12, P.mute);
    text(status, rx + 20, cy0 + 54, left - rx - 36, 18, 12, status_c, accent ? DWRITE_FONT_WEIGHT_MEDIUM : DWRITE_FONT_WEIGHT_NORMAL);
    // "Check now" under it (the project's and Studio's agreed design): the manual check, forced past the three-hour skip
    // (pc_update_check(2)), shown while checking is on and nothing is under way
    // ("Close OrrePort" in the same place while the update waits for it, 4.4; never while its game runs)
    if (on && s == PC_UPDATE_READY && g_upd.other_running && !g_game_running && !g_apps_root.empty()) {
        const std::wstring cl = tx("update.close_button", {{"program", L"OrrePort"}});
        const float cw = text_width(cl, 12, DWRITE_FONT_WEIGHT_MEDIUM) + 6;
        text(cl, rx + 20, cy0 + 74, cw, 18, 12, g.hover == HIT_UPD_CLOSE_OTHER ? P.text : P.accent2, DWRITE_FONT_WEIGHT_MEDIUM);
        hit({rx + 16, cy0 + 72, cw + 8, 22}, HIT_UPD_CLOSE_OTHER);
    }
    if (on && s != PC_UPDATE_CHECKING && s != PC_UPDATE_DOWNLOADING && s != PC_UPDATE_READY) {
        const std::wstring cn = tx("update.check_now");
        const float cw = text_width(cn, 12, DWRITE_FONT_WEIGHT_MEDIUM) + 6;
        text(cn, rx + 20, cy0 + 74, cw, 18, 12, g.hover == HIT_UPD_CHECK ? P.text : P.accent2, DWRITE_FONT_WEIGHT_MEDIUM);
        hit({rx + 16, cy0 + 72, cw + 8, 22}, HIT_UPD_CHECK);
    }
    // 2 and 3. the two switches
    struct Sw { const char* name; const char* help; bool on; int id; };
    const Sw sws[2] = {{"update.auto", "update.auto_help", g.update_auto, HIT_UPD_SW_AUTO},
                       {"update.check_setting", "update.check_help", g.update_check, HIT_UPD_SW_CHECK}};
    for (int k = 0; k < 2; ++k) {
        const float y = cy0 + ver_h + row * k;
        fill(rx + 16, y, rw - 32, 1, P.line);
        sw(rx + rw - 20, y + 22, sws[k].on, sws[k].id);
        text(tx(sws[k].name), rx + 20, y + 16, rw - 116, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        text(tx(sws[k].help), rx + 20, y + 40, rw - 116, 36, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
    }
    // 4. going back to the version before (its pop-up asks first, draw_goback; then restarts like an update)
    if (back) {
        const float y = cy0 + ver_h + row * 2;
        fill(rx + 16, y, rw - 32, 1, P.line);
        const std::wstring prev = goback_version();
        // (laid out as OrrePort's About lays it out, *2026-11-23*: the row's title "Go back to X", the help under it,
        // and a short "Go back" button; the same shared update.* lines)
        const float bx = button(rx + rw - 20, y + 22, tx("update.go_back_button"), HIT_UPD_GOBACK, false);
        text(tx("update.go_back", {{"version", prev}}), rx + 20, y + 14, bx - rx - 36, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        text(tx("update.go_back_help", {{"version", prev}}), rx + 20, y + 38, bx - rx - 36, 36, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_LEADING, true);
    }
    // 5. the logs folder, last (snatcherLeo, 2026-11-23: both programs' About end with it, "so we can match both "About" tabs
    // in both apps"; OrrePort's settings.logs lines, shared). The same folder in both programs, data\system\logs\; shown
    // by its last three names only, so the Windows user name never appears (as Help and reports shows it)
    {
        const float y = cy0 + ver_h + row * 2 + back_h;
        fill(rx + 16, y, rw - 32, 1, P.line);
        const float bx = icon_button(rx + rw - 20, y + 26, L"folder.svg", HIT_SET_LOGS_DIR, true);  // (the folder button)
        text(tx("settings.logs"), rx + 20, y + 14, bx - rx - 36, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        text(tx("settings.logs.sub"), rx + 20, y + 38, bx - rx - 36, 36, 12, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
        std::wstring tail = g.logs_dir;
        for (int cut = 0, at = (int)g.logs_dir.size(); cut < 3 && at > 0; ++cut) {
            at = (int)g.logs_dir.find_last_of(L"\\/", (size_t)at - 1);
            if (at == (int)std::wstring::npos) break;
            tail = L"…" + g.logs_dir.substr((size_t)at);
        }
        TextStyle st(true);
        text(tail, rx + 20, y + 72, bx - rx - 36, 18, 11.5f, P.faint);
    }
}

std::wstring reports_dir();  // (Report a problem's folder: with the panel, further down)
void draw_settings(const Layout& L) {
    const Palette& P = g.pal;
    const Rect& C = L.page;
    const float x0 = C.x + 40, y0 = C.y + 28;
    text(tx("studio.settings.title"), x0, y0, C.w - 80, 40, 24, P.text, DWRITE_FONT_WEIGHT_BOLD);
    // the panes (*Help and reports added 2026-11-22*, 4.3 part 3; snatcherLeo: "Settings screen additionally can open this
    // directory of where reports live")
    // (*Discs added 2026-11-23*, 4.4: between the two, as snatcherLeo saw it in the mockup; pane numbers: 0 Appearance, 1 Discs,
    // 2 Help and reports)
    static const char* kPanes[] = {"studio.settings.appearance", "studio.settings.discs", "studio.settings.help", "studio.settings.about"};
    constexpr int kPaneCount = 4;  // (*About added 2026-11-23*, 4.4: the version and updates; pane 3)
    const float lw = 200, ly = y0 + 64;
    for (int k = 0; k < kPaneCount; ++k) {
        const float y = ly + (float)k * 40;
        const bool on = g.settings_pane == k, hov = g.hover == HIT_SET_PANE + k;
        if (on) round_fill(x0, y, lw, 34, 8, P.accent_bg);
        else if (hov) round_fill(x0, y, lw, 34, 8, P.hover);
        if (on) round_fill(x0, y + 8, 3, 18, 1.5f, P.accent);
        text(tx(kPanes[k]), x0 + 14, y, lw - 20, 34, 13.5f, on ? P.text : P.text2, on ? DWRITE_FONT_WEIGHT_MEDIUM : DWRITE_FONT_WEIGHT_NORMAL);
        if (!on) hit({x0, y, lw, 34}, HIT_SET_PANE + k);
    }
    const float rx = x0 + lw + 32, rw = std::min(680.0f, C.x + C.w - 40 - rx);
    if (rw < 200) return;
    if (g.settings_pane == 1) { draw_discs(rx, ly, rw); return; }
    if (g.settings_pane == 3) { draw_about(rx, ly, rw); return; }
    if (g.settings_pane == 2) {
        // Help and reports: making a report, and the two folders a user may be asked for (reports, logs). Each row: its
        // name and what it does on the left, its button on the right
        text(tx("studio.settings.help"), rx, ly, rw, 26, 16, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        struct Row { const char* name; const char* help; const char* button; int id; };
        static const Row rows[] = {
            {"studio.settings.report", "studio.settings.report_help", "studio.settings.report_button", HIT_SET_REPORT},
            {"studio.settings.reports_dir", "studio.settings.reports_dir_help", "studio.settings.open_folder", HIT_SET_REPORTS_DIR},
            {"studio.settings.logs_dir", "studio.settings.logs_dir_help", "studio.settings.open_folder", HIT_SET_LOGS_DIR},
        };
        const float rh = 96, cy0 = ly + 34;
        round_fill(rx, cy0, rw, rh * 3, 12, P.card);
        round_line(rx, cy0, rw, rh * 3, 12, P.line);
        for (int k = 0; k < 3; ++k) {
            const float y = cy0 + (float)k * rh;
            if (k) fill(rx + 16, y, rw - 32, 1, P.line);
            const std::wstring b = tx(rows[k].button);
            const float bw = text_width(b, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 32, bx = rx + rw - 20 - bw, by = y + (rh - 34) / 2;
            text(tx(rows[k].name), rx + 20, y + 16, bx - rx - 36, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
            text(tx(rows[k].help), rx + 20, y + 40, bx - rx - 36, 40, 12.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
            if (k > 0) {
                // where the folder is (snatcherLeo, 2026-11-22: "settings screen should let user see the directory for reports"):
                // its last three folder names only, so the Windows user's name (early in the path) never shows, in a
                // report's screenshot of this page either
                const std::wstring full = k == 1 ? reports_dir() : g.logs_dir;
                std::wstring tail = full;
                for (int cut = 0, at = (int)full.size(); cut < 3 && at > 0; ++cut) {
                    at = (int)full.find_last_of(L"\\/", (size_t)at - 1);
                    if (at == (int)std::wstring::npos) break;
                    tail = L"…" + full.substr((size_t)at);
                }
                TextStyle st(true);
                text(tail, rx + 20, y + 64, bx - rx - 36, 20, 11.5f, P.faint);
            }
            const bool hov = g.hover == rows[k].id;
            round_fill(bx, by, bw, 34, 8, k == 0 ? (hov ? alpha(P.accent, 0.2f) : P.accent_bg) : (hov ? P.hover : P.bg));
            round_line(bx, by, bw, 34, 8, k == 0 ? P.accent_line : P.line2);
            text(b, bx, by, bw, 34, 12.5f, k == 0 ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            hit({bx, by, bw, 34}, rows[k].id);
        }
        return;
    }
    // the chosen pane: Appearance
    text(tx("studio.settings.appearance"), rx, ly, rw, 26, 16, P.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);  // the pane's heading
    const float cy0 = ly + 34, ch = 112;
    round_fill(rx, cy0, rw, ch, 12, P.card);
    round_line(rx, cy0, rw, ch, 12, P.line);
    // the theme: its name and what it does on the left, the three-way switch on the right
    const std::wstring opts[3] = {tx("studio.settings.theme_windows"), tx("studio.settings.theme_light"), tx("studio.settings.theme_dark")};
    float sw = 4;
    float ow[3];
    for (int k = 0; k < 3; ++k) { ow[k] = text_width(opts[k], 12.5f, DWRITE_FONT_WEIGHT_NORMAL) + 24; sw += ow[k] + 2; }
    const float sx0 = rx + rw - 20 - sw, sy = cy0 + 20, sh = 34;
    text(tx("studio.settings.theme"), rx + 20, cy0 + 16, sx0 - rx - 36, 22, 14, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    text(tx("studio.settings.theme_help"), rx + 20, cy0 + 40, sx0 - rx - 36, 40, 12.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL,
         DWRITE_TEXT_ALIGNMENT_LEADING, true);
    const int cur = g.studio_theme >= 0 ? g.studio_theme : g.theme_setting;
    round_fill(sx0, sy, sw, sh, 9, P.bg);
    round_line(sx0, sy, sw, sh, 9, P.line2);
    float sx = sx0 + 2;
    for (int k = 0; k < 3; ++k) {
        const bool on = cur == k, hov = g.hover == HIT_THEME_OPT + k;
        if (on) { round_fill(sx, sy + 3, ow[k], sh - 6, 7, P.accent_bg); round_line(sx, sy + 3, ow[k], sh - 6, 7, P.accent_line); }
        else if (hov) round_fill(sx, sy + 3, ow[k], sh - 6, 7, P.hover);
        text(opts[k], sx, sy + 3, ow[k], sh - 6, 12.5f, on ? P.accent2 : P.text2, on ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER);
        if (!on || g.studio_theme < 0) hit({sx, sy + 3, ow[k], sh - 6}, HIT_THEME_OPT + k);
        sx += ow[k] + 2;
    }
    // where the choice comes from: OrrePort's shared setting until one is picked here
    text(tx(g.studio_theme >= 0 ? "studio.settings.theme_saved" : "studio.settings.theme_shared"), rx + 20, cy0 + ch - 30, rw - 40, 20, 11.5f,
         P.faint);
}

// ---- Report a problem (4.3 part 3; the flow snatcherLeo approved 2026-11-22: "Yes, build it that way. Settings screen
// additionally can open this directory of where reports live") ----
// Where reports are saved: beside OrrePort's own, R\Pokemon Colosseum\Reports\ in the portable layout (pc_paths.c's
// PC_PLACE_REPORTS), so a tester finds both programs' reports in one folder; a development build without a portable
// folder keeps them beside its exe (Reports\), so test reports never land in snatcherLeo's real folder.
std::wstring reports_dir() {
    return g.root_dir.empty() ? g.exe_dir + L"\\Reports" : g.root_dir + L"\\Pokemon Colosseum\\Reports";
}
// File Explorer on a folder (made first if missing), with one file selected when given. Windows (explorer's /select);
// Linux: xdg-open on the folder; macOS: open -R on the file. (The trail names the folder by its role only: the log never
// holds a path, which would carry the user's Windows name)
void open_folder(const std::wstring& dir, const std::wstring& select_file) {
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    if (!select_file.empty()) {
        const std::wstring args = L"/select,\"" + dir + L"\\" + select_file + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    trail("folder", dir == g.logs_dir ? "logs folder opened" : select_file.empty() ? "reports folder opened" : "reports folder opened on the report");
}

// The panel: a card over the dimmed window (as the search palette is), with the note, the two ticks, the privacy line and
// Save report; after saving, the same card says it was saved and copied, with Open folder and Done. While it's open the
// other child windows hide (they would paint over it: place_children), and nothing behind it takes clicks.
// The note is a native multi-line edit (typing, selection, pasting, the IME for Japanese, screen readers); Escape in it
// closes the panel. Windows-only edit control; Linux / macOS: the toolkit's text field.
void place_children();
void save_report();
bool key(WPARAM vk);
Rect report_rect(const Layout& L) {
    const float w = std::min(580.0f, L.w - 40), h = g.report_stage == 0 ? 440.0f : 250.0f;
    return {(L.w - w) / 2, std::max(60.0f, (L.h - h) / 3), w, h};
}
Rect report_note_rect(const Layout& L) {
    const Rect r = report_rect(L);
    return {r.x + 28, r.y + 124, r.w - 56, 96};
}
LRESULT CALLBACK report_note_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    activate(GetParent(h));
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { key(VK_ESCAPE); return 0; }
    if (m == WM_CHAR && w == VK_ESCAPE) return 0;
    return DefSubclassProc(h, m, w, l);
}
void open_report() {
    if (g.report_open) return;
    g.report_open = true;
    g.report_stage = 0;
    g.report_error.clear();
    g.palette_open = g.drop_open = false;
    if (!g.report_note) {
        // (no scroll bar: Windows draws it light even in the dark theme; the box scrolls itself as the text grows)
        g.report_note = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 10, 10,
                                        g.hwnd, nullptr, g.inst, nullptr);
        SendMessageW(g.report_note, WM_SETFONT, (WPARAM)g.search_font, TRUE);
        SetWindowSubclass(g.report_note, report_note_proc, 1, 0);
    }
    SetWindowTextW(g.report_note, L"");
    place_children();
    SetFocus(g.report_note);
    InvalidateRect(g.hwnd, nullptr, FALSE);
    trail("report", "panel opened");
}
void close_report() {
    if (!g.report_open) return;
    g.report_open = false;
    if (g.report_note) ShowWindow(g.report_note, SW_HIDE);
    SetFocus(g.hwnd);
    place_children();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
// The bar after an unexpected close (the previous run's log has no clean-exit line): across the top of the page, in the
// first window only, until Make a report or Not now. Its words in studio/lang; Make a report opens the panel with that
// previous log added to the zip.
void draw_crash_bar(const Layout& L) {
    if (!g.crash_bar || g.report_open || g_windows.empty() || g_windows.front().hwnd != g.hwnd) return;
    const Palette& P = g.pal;
    const float x = L.page.x + 16, w = L.page.w - 32, y = kTabBar + 10, h = 44;  // (in the strip layout() leaves above the page)
    round_fill(x, y, w, h, 10, P.dark ? hexc(0x2A2220) : hexc(0xFBEDEA));
    round_line(x, y, w, h, 10, hexc(0xE0705E));
    auto button = [&](float right, const std::wstring& label, int id, bool main) {
        const float bw = text_width(label, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 28;
        const bool hov = g.hover == id;
        round_fill(right - bw, y + 7, bw, h - 14, 8, main ? (hov ? alpha(P.accent, 0.25f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(right - bw, y + 7, bw, h - 14, 8, main ? P.accent_line : P.line2);
        text(label, right - bw, y + 7, bw, h - 14, 12.5f, main ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({right - bw, y + 7, bw, h - 14}, id);
        return bw;
    };
    // (OrrePort's shared report.crash_* lines since 8ef34d4b8, 2026-11-23: the hub's bar says the same, with {program})
    const float b1 = button(x + w - 10, tx("report.crash_report"), HIT_CRASH_REPORT, true);
    const float b2 = button(x + w - 10 - b1 - 8, tx("report.crash_dismiss"), HIT_CRASH_DISMISS, false);
    text(tx("report.crash_bar", {{"program", L"Studio"}}), x + 16, y, w - b1 - b2 - 50, h, 13, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
}
// The US disc Studio is using was removed from the ROM folder while it runs (by the hub's Remove, or by hand). *Added
// 2026-11-24* (5.9's laptop sitting, check 4's two-way test: the hub removed the US disc, and Studio went on as before
// with only Settings' row saying "Not added", which left the question of what Studio should be doing). Studio keeps
// the image open (its handle shares deleting, so Windows lets the file go to the Recycle Bin or be deleted while Studio
// goes on reading it: verified 2026-11-23 for a delete by hand, likely the same for the Recycle Bin), so nothing open is
// closed or lost; a calm bar in every window says so, with "Discs" (Settings › Discs) and "OK" (hides it until the
// disc comes back and goes again). It goes by itself when a US disc is back (a different file is read, tabs kept, as
// before). Drawn for every system; whether another system's file handle survives a delete the same way is for its port
// to check (Linux and macOS keep an open file readable after unlinking it: likely)
bool disc_gone_bar() {
    return g.disc && !g.disc_forced && g_discs.path[0].empty() && !g_discs.gone_hidden && !g.report_open;
}
void draw_disc_bar(const Layout& L) {
    if (!disc_gone_bar() || (g.crash_bar && !g_windows.empty() && g_windows.front().hwnd == g.hwnd)) return;
    const Palette& P = g.pal;
    const float x = L.page.x + 16, w = L.page.w - 32, y = kTabBar + 10, h = 44;  // (the strip layout() leaves)
    round_fill(x, y, w, h, 10, P.card);
    round_line(x, y, w, h, 10, hexc(0xE8955A));
    auto button = [&](float right, const std::wstring& label, int id, bool main) {
        const float bw = text_width(label, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 28;
        const bool hov = g.hover == id;
        round_fill(right - bw, y + 7, bw, h - 14, 8, main ? (hov ? alpha(P.accent, 0.25f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(right - bw, y + 7, bw, h - 14, 8, main ? P.accent_line : P.line2);
        text(label, right - bw, y + 7, bw, h - 14, 12.5f, main ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({right - bw, y + 7, bw, h - 14}, id);
        return bw;
    };
    const float b1 = button(x + w - 10, tx("studio.discs.gone_ok"), HIT_DISC_GONE_OK, false);
    const float b2 = button(x + w - 10 - b1 - 8, tx("studio.discs.gone_open"), HIT_DISC_GONE_OPEN, true);
    text(tx("studio.discs.gone_bar"), x + 16, y, w - b1 - b2 - 50, h, 13, P.text, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_LEADING, true);
}
// The update's "Go back" pop-up (*2026-11-23*, snatcherLeo after his update hand check: "it should be a pop-up instead, and it
// asks users are they sure (and a new warning message about this)"; the words approved the same day, "Yes use it"): a
// card over the dimmed window, as Report a problem's is. The title asks; a line says both programs go back and Studio
// restarts; a warning box says what going back can cost (work made with the newer version) and what it never touches
// (saves, mods). While OrrePort is open from the same folder the launcher can't swap app\ (it is in use), so the card
// says to close it and offers "Close OrrePort" (pc_apps, as Home's update line does) in place of Go back; while its
// game runs, no button (nothing is closed for the player). The words are OrrePort's shared update.* lines, so both
// programs ask alike; Escape or Cancel closes it.
float para_height(const std::wstring& s, float w, float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) {
    IDWriteTextFormat* f = format(size, weight);
    if (!f) return 0;
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    IDWriteTextLayout* l = nullptr;
    g.dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, w, 2000, &l);
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (!l) return 0;
    DWRITE_TEXT_METRICS m{};
    l->GetMetrics(&m);
    l->Release();
    return m.height;
}
void draw_goback(const Layout& L) {
    if (!g.goback_open) return;
    const Palette& P = g.pal;
    fill(0, 0, L.w, L.h, D2D1::ColorF(0, 0, 0, P.dark ? 0.55f : 0.35f));
    hit({0, 0, L.w, L.h}, HIT_UPD_GOBACK_BG);
    const std::wstring prev = goback_version(), cur = widen(g_upd.current);
    const bool hub = g_upd.other_running || g_hub_open;  // (OrrePort open from this folder: app\ is in use)
    const std::wstring title = tx("update.go_back_title", {{"version", prev}});
    const std::wstring ask = tx("update.go_back_ask", {{"version", prev}, {"program", L"Studio"}});
    const std::wstring warn = tx("update.go_back_warn", {{"version", prev}, {"current", cur}});
    const std::wstring close = hub ? (g_game_running ? tx("update.wait_game") : tx("update.go_back_close", {{"program", L"OrrePort"}}))
                                   : std::wstring();
    const float w = std::min(520.0f, L.w - 40), inner = w - 56;
    const float ask_h = para_height(ask, inner, 13), warn_h = para_height(warn, inner - 44, 12.5f);
    const float close_h = close.empty() ? 0 : para_height(close, inner, 12.5f) + 12;
    const float h = 22 + 30 + 12 + ask_h + 16 + warn_h + 28 + close_h + 20 + 36 + 24;
    const float x0 = (L.w - w) / 2, y0 = std::max(60.0f, (L.h - h) / 3), x = x0 + 28;
    round_fill(x0, y0, w, h, 14, P.card);
    round_line(x0, y0, w, h, 14, P.line2);
    float y = y0 + 22;
    text(title, x, y, inner, 30, 20, P.text, DWRITE_FONT_WEIGHT_BOLD);
    y += 30 + 12;
    para(ask, x, y, inner, 13, P.text2);
    y += ask_h + 16;
    // the warning: a soft orange box with a "!" (the palette has no warning colour yet; the Home dot's orange)
    const D2D1_COLOR_F orange = hexc(0xE8955A);
    const float wh = warn_h + 28;
    round_fill(x, y, inner, wh, 9, alpha(orange, P.dark ? 0.10f : 0.12f));
    round_line(x, y, inner, wh, 9, alpha(orange, 0.45f));
    text(L"!", x + 12, y + 12, 20, 20, 14, orange, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    para(warn, x + 40, y + 14, inner - 44, 12.5f, P.text2);
    y += wh;
    if (!close.empty()) {
        y += 12;
        para(close, x, y, inner, 12.5f, P.mute);
        y += close_h - 12;
    }
    auto button = [&](float bx, float by, const std::wstring& label, int id, bool main) {
        const float bw = text_width(label, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 36;
        const bool hov = g.hover == id;
        round_fill(bx - bw, by, bw, 36, 9, main ? (hov ? alpha(P.accent, 0.25f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(bx - bw, by, bw, 36, 9, main ? P.accent_line : P.line2);
        text(label, bx - bw, by, bw, 36, 13, main ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx - bw, by, bw, 36}, id);
        return bw;
    };
    const float bx = x0 + w - 28, by = y0 + h - 24 - 36;
    float used = 0;
    if (!hub) {
        // Go back in OrrePort's red "danger" style, as the hub's pop-up has it (snatcherLeo, 2026-11-23: "Red, like the hub";
        // settings.rcss .fqb.danger: #B5452E, hover #C9533A, white words), the same in light and dark
        const std::wstring label = tx("update.go_back_button");
        const float bw = text_width(label, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 36;
        const D2D1_COLOR_F red = hexc(g.hover == HIT_UPD_GOBACK_YES ? 0xC9533A : 0xB5452E);
        round_fill(bx - bw, by, bw, 36, 9, red);
        round_line(bx - bw, by, bw, 36, 9, red);
        text(label, bx - bw, by, bw, 36, 13, hexc(0xFFFFFF), DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx - bw, by, bw, 36}, HIT_UPD_GOBACK_YES);
        used = bw;
    }
    else if (!g_game_running && !g_apps_root.empty())
        used = button(bx, by, tx("update.close_button", {{"program", L"OrrePort"}}), HIT_UPD_CLOSE_OTHER, true);
    button(bx - (used > 0 ? used + 10 : 0), by, tx("common.cancel"), HIT_UPD_GOBACK_NO, false);
}
void draw_report(const Layout& L) {
    if (!g.report_open) return;
    const Palette& P = g.pal;
    fill(0, 0, L.w, L.h, D2D1::ColorF(0, 0, 0, P.dark ? 0.55f : 0.35f));
    hit({0, 0, L.w, L.h}, HIT_REP_BG);
    const Rect r = report_rect(L);
    round_fill(r.x, r.y, r.w, r.h, 14, P.card);
    round_line(r.x, r.y, r.w, r.h, 14, P.line2);
    const float x = r.x + 28, w = r.w - 56;
    auto button = [&](float bx, float by, const std::wstring& label, int id, bool main) {
        const float bw = text_width(label, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 36;
        const bool hov = g.hover == id;
        round_fill(bx - bw, by, bw, 36, 9, main ? (hov ? alpha(P.accent, 0.25f) : P.accent_bg) : (hov ? P.hover : P.bg));
        round_line(bx - bw, by, bw, 36, 9, main ? P.accent_line : P.line2);
        text(label, bx - bw, by, bw, 36, 13, main ? P.accent2 : P.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx - bw, by, bw, 36}, id);
        return bw;
    };
    if (g.report_stage == 0) {
        text(tx("studio.report.title"), x, r.y + 22, w, 30, 20, P.text, DWRITE_FONT_WEIGHT_BOLD);
        text(tx("studio.report.intro"), x, r.y + 56, w, 36, 12.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
        text(tx("studio.report.note_label"), x, r.y + 98, w, 22, 13, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        const Rect n = report_note_rect(L);
        round_fill(n.x - 8, n.y - 8, n.w + 16, n.h + 16, 8, P.bg);
        round_line(n.x - 8, n.y - 8, n.w + 16, n.h + 16, 8, P.line2);
        text(tx("studio.report.note_hint"), x, n.y + n.h + 12, w, 20, 11.5f, P.faint);
        static const char* ticks[2] = {"studio.report.tick_shots", "studio.report.tick_open"};
        for (int k = 0; k < 2; ++k) {
            const float ty = n.y + n.h + 42 + (float)k * 30;
            const bool on = g.report_tick[k], hov = g.hover == HIT_REP_TICK + k;
            round_fill(x, ty + 3, 18, 18, 4, on ? P.accent : (hov ? P.hover : P.bg));
            round_line(x, ty + 3, 18, 18, 4, on ? P.accent : P.line2);
            if (on) text(L"✓", x, ty + 2, 18, 20, 12, P.bg, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            text(tx(ticks[k]), x + 28, ty, w - 28, 24, 13, P.text2);
            hit({x, ty, w, 24}, HIT_REP_TICK + k);
        }
        const float py = n.y + n.h + 108;
        text(tx("studio.report.privacy"), x, py, w, 40, 11.5f, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
        if (!g.report_error.empty())
            text(g.report_error, x, r.y + r.h - 50, w - 260, 36, 11.5f, hexc(0xE0705E), DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_LEADING, true);  // (a soft red: the palette has no warning colour yet)
        const float bx = r.x + r.w - 28, by = r.y + r.h - 56;
        const float sw = button(bx, by, tx("studio.report.save"), HIT_REP_SAVE, true);
        button(bx - sw - 10, by, tx("studio.report.cancel"), HIT_REP_CANCEL, false);
    } else {
        // (the panel's own title: the sentence under it, OrrePort's words, starts "Report saved")
        text(tx("studio.report.title"), x, r.y + 22, w, 30, 20, P.text, DWRITE_FONT_WEIGHT_BOLD);
        // (report.*: OrrePort's own lines, the same words in both programs since 2026-11-23: lang_load_shared)
        text(tx(g.report_copied ? "report.saved_body" : "report.saved_not_copied"), x, r.y + 60, w, 44, 13, P.text2,
             DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, true);
        round_fill(x, r.y + 116, w, 40, 8, P.bg);
        // the zip's name, and its size on the right (the project's draft, 2026-11-22: show it, and warn past about 10 MB)
        wchar_t mb[32];
        swprintf(mb, 32, L"%.1f MB", g.report_mb);
        const bool big = g.report_mb > 9.5;
        text(g.report_file, x + 14, r.y + 116, w - 120, 40, 12.5f, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
        text(tx("report.size", {{"size", mb}}), x + w - 110, r.y + 116, 96, 40, 12, big ? hexc(0xE0705E) : P.mute,
             DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
        if (big)
            text(tx("report.too_big", {{"size", mb}}) + L" " + tx("studio.report.too_big_hint"), x, r.y + 162, w, 36, 11.5f, hexc(0xE0705E), DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_LEADING, true);
        const float bx = r.x + r.w - 28, by = r.y + r.h - 56;
        const float dw = button(bx, by, tx("report.done"), HIT_REP_DONE, true);
        button(bx - dw - 10, by, tx("report.open_folder"), HIT_REP_OPEN, false);
    }
}

void draw_toast(const Layout& L) {
    if (g.toast.empty()) return;
    const Palette& P = g.pal;
    const float w = text_width(g.toast, 13, DWRITE_FONT_WEIGHT_NORMAL) + 28, x = L.page.x + (L.page.w - w) / 2, y = L.h - 54;
    round_fill(x, y, w, 34, 9, P.dark ? hexc(0x0F110F) : P.card);
    round_line(x, y, w, 34, 9, P.line2);
    text(g.toast, x, y, w, 34, 13, P.text, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void show_toast(const std::wstring& s) {
    g.toast = s;
    SetTimer(g.hwnd, TIMER_TOAST, 1800, nullptr);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

void draw_message_page(const Layout& L, const std::wstring& title, const std::wstring& body, int symbol, bool choose,
                       const std::wstring& note = L"") {
    const Palette& P = g.pal;
    const float x = kRail + (choose ? 0 : 0), w = L.w - kRail;
    const float cy = kTabBar + (L.h - kTabBar) / 2 - 90;
    // (the box narrows with the window, so a "Coming soon" line still wraps inside a small window)
    const float bw = std::min(520.0f, std::max(200.0f, w - 80));
    if (symbol >= 0) draw_symbol(symbol, x + w / 2 - 28, cy - 70, 56, P.mute);
    text(title, x + 40, cy, w - 80, 40, 24, P.text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    text(body, x + w / 2 - bw / 2, cy + 44, bw, 70, 14, P.mute, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, true);
    // a second, smaller line (a "Coming soon" page's edit note, 2026-11-22)
    if (!note.empty())
        text(note, x + w / 2 - bw / 2, cy + 118, bw, 40, 12, alpha(P.mute, 0.8f), DWRITE_FONT_WEIGHT_NORMAL,
             DWRITE_TEXT_ALIGNMENT_CENTER, true);
    if (choose) {
        const std::wstring b = tx("studio.disc.choose");
        const float bw = text_width(b, 14, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 40;
        const float bx = x + (w - bw) / 2, by = cy + 128;
        round_fill(bx, by, bw, 40, 10, g.hover == HIT_CHOOSE_DISC ? alpha(P.accent, 0.85f) : P.accent);
        text(b, bx, by, bw, 40, 14, P.on_accent, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        hit({bx, by, bw, 40}, HIT_CHOOSE_DISC);
    }
}

void draw_dev_view(const Layout& L) {
    const Palette& P = g.pal;
    const auto lines = watch_lines();
    const float w = 460, h = 30 + lines.size() * 20.0f, x = L.w - w - 16, y = L.h - h - 16;
    round_fill(x, y, w, h, 10, alpha(P.panel2, 0.97f));
    round_line(x, y, w, h, 10, P.accent);
    for (size_t i = 0; i < lines.size(); ++i) text(lines[i], x + 14, y + 14 + i * 20.0f, w - 28, 20, 12, P.text);
}

void release_target();
// the editor a page belongs to: 0 for a Pokémon, 1..7 for an editor's page, -1 for Home and Settings (no editor)
int page_owner(int page) { return page >= 0 ? 0 : page_editor(page) ? page_editor(page) : -1; }
// The shown page as its editor's last place (WinState::places), with its scroll: noted on every paint, so whatever the
// window showed last in each editor is what its rail icon brings back
void note_place() {
    const int page = cur_page(), ed = page_owner(page);
    if (ed < 0 || (page >= 0 && page != g.selected)) return;  // (Home, Settings; a Pokémon tab whose page is still loading)
    g.places[ed].page = page;
    g.places[ed].scroll = g.pscroll;
}

// The page's own draw times for the performance log (page_frames, page_avg_ms, page_worst_ms): BeginDraw to EndDraw,
// the vsync wait of Present left out. Added 2026-11-22 for the 3D view's move into the page (the page then redraws at
// the view's rate, so its draw time becomes the frame's). std::chrono: every platform
int g_page_frames = 0;
double g_page_ms_sum = 0, g_page_ms_worst = 0;
void page_stats_take(int& frames, double& avg_ms, double& worst_ms) {  // (page_frame_stats, below the namespace, calls it)
    frames = g_page_frames;
    avg_ms = g_page_frames ? g_page_ms_sum / g_page_frames : 0;
    worst_ms = g_page_ms_worst;
    g_page_frames = 0;
    g_page_ms_sum = g_page_ms_worst = 0;
}

// The page under the 3D view: the tab bar, the rail, and the editor's page (the list and the centre)
void paint_base(const Layout& L) {
    g.rt->Clear(g.pal.bg);
    draw_tab_bar(L);
    draw_rail(L);
    if (g.editor != 0) {
        // each editor's own "what it will do" line (the project's drafts from the project plan, 2026-11-22; the generic line when a
        // language has none), and on the editors decided as view-only first (Moves, Items, Trainers, People) the note
        // that changing comes with mods
        char t[64], b[64], n[64];
        std::snprintf(n, sizeof n, "studio.editor.%s", kEditors[g.editor]);
        std::snprintf(t, sizeof t, "studio.coming_soon.title");
        std::snprintf(b, sizeof b, "studio.coming_soon.%s", kEditors[g.editor]);
        const char* body_key = has_text(b) ? b : "studio.coming_soon.body";
        const bool view_first = g.editor >= 1 && g.editor <= 4;
        draw_message_page(L, tx(t, {{"editor", tx(n)}}), tx(body_key), g.editor, false,
                          view_first ? tx("studio.coming_soon.edit_note") : std::wstring());
    } else if (!g.disc && !on_app_page()) {  // (Home and Settings work without a disc: Settings › Discs adds one)
        if (g.loading) draw_message_page(L, tx("studio.disc.reading"), L"", 0, false);
        else draw_message_page(L, tx("studio.disc.none_title"),
                               g.load_error.empty() ? tx("studio.disc.none_body") : tx("studio.disc.error", {{"reason", widen(g.load_error)}}), 0, true);
    } else {
        if (!on_app_page()) draw_list(L);
        draw_centre(L);
    }
}
// Over the 3D view: the rail's label, the global search (its drop-down or the palette), the hover card, a toast; then
// the list's search box leaves out whatever of them reaches over it; the developer view last
void paint_top(const Layout& L) {
    g_cut_set = false;
    draw_rail_label();
    draw_search(L);
    search_cut_out(g_cut_set ? &g_cut : nullptr);
    if (!g.palette_open && !g.report_open && !g.goback_open) draw_hover_card(L);
    draw_toast(L);
    draw_crash_bar(L);  // (after an unexpected close: offers a report)
    draw_disc_bar(L);  // (the US disc in use was removed from the ROM folder)
    draw_goback(L);  // (the update's "Go back" pop-up)
    draw_report(L);  // (Report a problem's panel: over everything but the developer view)
    if (g.dev_view) draw_dev_view(L);
}
// The saved page for the window's size in pixels; false: no saved page (drawn whole then). *Changed 2026-11-22*: sized
// in steps of 256 pixels and kept while the window fits it (and isn't much smaller); the page uses its top-left part
// (draw_page_cache). Made at the exact size, a window resize made a new full-window bitmap on every step (about 24 MB at
// 4K), freed only seconds later: private memory rose by about 300 MB during a long resize (measured, see ensure_shared)
bool ensure_cache() {
    if (!g.rt || !g.target) return false;
    const D2D1_SIZE_U px_size = g.target->GetPixelSize();
    constexpr int kStep = 256;
    const int w = (int)px_size.width, h = (int)px_size.height;
    const int bw = (w + kStep - 1) / kStep * kStep, bh = (h + kStep - 1) / kStep * kStep;
    if (g.page_cache && w <= g.cache_w && h <= g.cache_h && g.cache_w <= bw + kStep && g.cache_h <= bh + kStep) return true;
    if (g.page_cache) { g.page_cache->Release(); g.page_cache = nullptr; }
    const D2D1_SIZE_U alloc = D2D1::SizeU((UINT32)bw, (UINT32)bh);
    const float dpi = 96.0f * g.scale;
    const D2D1_BITMAP_PROPERTIES1 bp =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi, dpi);
    // (drawn at the window's top-left at the window's DPI, DrawBitmap(page_cache): the part past the window's edge is
    // simply outside it)
    if (FAILED(g.rt->CreateBitmap(alloc, nullptr, 0, &bp, &g.page_cache))) { g.page_cache = nullptr; return false; }
    g.cache_w = bw;
    g.cache_h = bh;
    return true;
}
// A new 3D frame alone (WM_VIEW_SHARED): the saved page, the frame and the overlays, shown; the whole page is drawn
// again only when something on it changed (WM_PAINT). Without a saved page yet, a whole paint
void compose() {
    if (!g.rt || !g.page_cache || !g.target || !page_view()) { InvalidateRect(g.hwnd, nullptr, FALSE); return; }
    const auto t0 = std::chrono::steady_clock::now();
    if (!take_shared_frame()) return;  // (the frame is still being written: the next one comes soon)
    g.hits = g.base_hits;
    const Layout L = layout();
    g.rt->SetTarget(g.target);
    g.rt->BeginDraw();
    g.rt->DrawBitmap(g.page_cache);
    draw_shared_view(L);
    paint_top(L);
    HRESULT hr = g.rt->EndDraw();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ++g_page_frames;
    g_page_ms_sum += ms;
    g_page_ms_worst = std::max(g_page_ms_worst, ms);
    if (SUCCEEDED(hr)) hr = g.swap->Present(1, 0);
    if (FAILED(hr)) InvalidateRect(g.hwnd, nullptr, FALSE);  // (a lost device is handled by the whole paint)
}

void discs_left();
void paint() {
    if (!g.rt) return;
    const auto paint_t0 = std::chrono::steady_clock::now();
    discs_left();  // (Settings › Discs' result lines go once the pane is left, 4.4)
    // the newest 3D frame into the page's own copy (take_shared_frame: the shared texture's lock, key 0 as Dawn takes and
    // gives it, dawn::native::d3d::kDXGIKeyedMutexAcquireKey, held only for the copy); not taken within 20 ms, the copy
    // keeps the last frame, which is drawn instead (*changed 2026-11-22*: the 3D view was left out then, a flicker)
    take_shared_frame();
    note_place();
    g.hits.clear();
    const Layout L = layout();
    // page_view: the base drawn into the saved page (ensure_cache), then stacked with the 3D frame and the overlays;
    // otherwise all in one draw, as before
    const bool cached = page_view() && ensure_cache();
    if (cached) {
        g.rt->SetTarget(g.page_cache);
        g.rt->BeginDraw();
        paint_base(L);
        g.rt->EndDraw();
        g.base_hits = g.hits;
        g.rt->SetTarget(g.target);
        g.rt->BeginDraw();
        g.rt->DrawBitmap(g.page_cache);
        draw_shared_view(L);
    } else {
        g.rt->BeginDraw();
        paint_base(L);
        draw_shared_view(L);
    }
    paint_top(L);
    // show the frame: the swap chain presents it at the next vertical blank (vsync 1, as the old window render target did)
    HRESULT hr = g.rt->EndDraw();
    {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - paint_t0).count();
        ++g_page_frames;
        g_page_ms_sum += ms;
        g_page_ms_worst = std::max(g_page_ms_worst, ms);
    }
    if (SUCCEEDED(hr)) hr = g.swap->Present(1, 0);
    if (hr == (HRESULT)D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // the graphics device was lost (a driver update, a GPU reset): every picture belonged to it, so all go, and
        // the devices are made again on the next paint. (With several windows, each window's context and swap chain
        // goes too: plan step 2 walks them.)
        // (why, by name: the device's own removed reason, 4.3 part 3's item 8)
        const HRESULT why = g.d3d ? g.d3d->GetDeviceRemovedReason() : S_OK;
        logf(Level::Warn, "draw", "graphics device lost: %s, the device's reason %s; recreating", hr_text(hr).c_str(), hr_text(why).c_str());
        for (auto& e : g.all) if (e.bmp) { e.bmp->Release(); e.bmp = nullptr; }
        release_pics();
        release_target();
        if (g.d2ddev) { g.d2ddev->Release(); g.d2ddev = nullptr; }
        if (g.dxgi) { g.dxgi->Release(); g.dxgi = nullptr; }
        if (g.d3d) { g.d3d->Release(); g.d3d = nullptr; }
    }
}

// The shared devices (multi-window plan step 1c): one Direct3D 11 device (BGRA support, as Direct2D needs; a hardware
// one, else WARP, Windows' software renderer) and the one Direct2D device on it, made with the first window. Windows
// only (Direct3D / DXGI); Linux / macOS: the toolkit's own drawing device, shared by its windows the same way.
bool create_devices() {
    if (g.d2ddev) return true;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    HRESULT hw = S_OK, warp = S_OK;
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        const HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 4, D3D11_SDK_VERSION,
                                             &g.d3d, nullptr, nullptr);
        (type == D3D_DRIVER_TYPE_HARDWARE ? hw : warp) = hr;
        if (SUCCEEDED(hr)) break;
    }
    if (!g.d3d) {
        logf(Level::Error, "draw", "no Direct3D 11 device: the graphics card said %s, Windows' software renderer %s", hr_text(hw).c_str(), hr_text(warp).c_str());
        return false;
    }
    if (FAILED(hw)) logf(Level::Warn, "draw", "the graphics card gave no Direct3D 11 device (%s): drawing with Windows' software renderer", hr_text(hw).c_str());
    IDXGIDevice* dx = nullptr;
    IDXGIAdapter* ad = nullptr;
    ID2D1Factory6* f6 = nullptr;
    bool ok = SUCCEEDED(g.d3d->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dx))) &&
              SUCCEEDED(dx->GetAdapter(&ad)) && SUCCEEDED(ad->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&g.dxgi))) &&
              SUCCEEDED(g.d2d->QueryInterface(__uuidof(ID2D1Factory6), reinterpret_cast<void**>(&f6))) &&
              SUCCEEDED(f6->CreateDevice(dx, &g.d2ddev));
    if (f6) f6->Release();
    if (ad) ad->Release();
    if (dx) dx->Release();
    if (!ok) logf(Level::Error, "draw", "no Direct2D device");
    return ok;
}
// the swap chain's back buffer as the window's drawing target, at the window's DPI
void bind_target() {
    IDXGISurface* surf = nullptr;
    if (FAILED(g.swap->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&surf)))) return;
    const float dpi = 96.0f * g.scale;
    const D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), dpi, dpi);
    g.rt->CreateBitmapFromDxgiSurface(surf, &bp, &g.target);
    surf->Release();
    g.rt->SetTarget(g.target);
    g.rt->SetDpi(dpi, dpi);
}
// a new size (WM_SIZE): the back buffers follow the window
void resize_target(UINT w, UINT h) {
    if (!g.rt || !g.swap || !w || !h) return;
    g.rt->SetTarget(nullptr);
    if (g.target) { g.target->Release(); g.target = nullptr; }
    g.swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    bind_target();
}
// this window's device context and swap chain (and the shared devices with the first window)
void create_target() {
    if (!create_devices()) return;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    if (g.solid) { g.solid->Release(); g.solid = nullptr; }
    for (auto& [k, d] : g.svgs) if (d) d->Release();
    g.svgs.clear();
    if (FAILED(g.d2ddev->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g.rt))) { g.rt = nullptr; return; }
    g.dc = g.rt;
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = std::max<LONG>(1, rc.right);
    sd.Height = std::max<LONG>(1, rc.bottom);
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // the flip model: what DWM composes best (Windows 8+)
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    if (const HRESULT hr = g.dxgi->CreateSwapChainForHwnd(g.d3d, g.hwnd, &sd, nullptr, nullptr, &g.swap); FAILED(hr)) {
        logf(Level::Error, "draw", "no swap chain for the window (%u x %u): %s", sd.Width, sd.Height, hr_text(hr).c_str());
        g.rt->Release();
        g.rt = g.dc = nullptr;
        return;
    }
    bind_target();
    g.rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}
// drops this window's context and swap chain (the device was lost, or the window closes)
void release_frame_copy();
void release_target() {
    if (g.rt) g.rt->SetTarget(nullptr);
    release_frame_copy();  // (the 3D frame's copy: its bitmap belongs to the context)
    if (g.page_cache) { g.page_cache->Release(); g.page_cache = nullptr; }  // (the saved page belongs to the context)
    g.cache_w = g.cache_h = 0;
    if (g.target) { g.target->Release(); g.target = nullptr; }
    if (g.solid) { g.solid->Release(); g.solid = nullptr; }
    for (auto& [k, d] : g.svgs) if (d) d->Release();
    g.svgs.clear();
    if (g.swap) { g.swap->Release(); g.swap = nullptr; }
    if (g.rt) { g.rt->Release(); g.rt = g.dc = nullptr; }
}

// --game-camera: reads the [waza_viewer] lines the project's move-viewer runs log next to each shot ("camera view matrix row
// N: a b c d", "camera fov F ... aspect A ...", "model position x y z rotation x y z") and hands them to the 3D view
void apply_game_camera(const std::wstring& path) {
    std::FILE* f = _wfopen(path.c_str(), L"r");
    if (!f) { logf(Level::Error, "camera", "can't read %s", narrow(path).c_str()); return; }
    float rows[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, fov = 60, aspect = 1.333f, pos[3] = {}, rot[3] = {};
    int got = 0;
    char line[512];
    while (std::fgets(line, sizeof line, f)) {
        int r;
        float a, b, c, d;
        if (const char* p = std::strstr(line, "camera view matrix row ")) {
            if (std::sscanf(p, "camera view matrix row %d: %f %f %f %f", &r, &a, &b, &c, &d) == 5 && r >= 0 && r < 3) {
                rows[4 * r] = a; rows[4 * r + 1] = b; rows[4 * r + 2] = c; rows[4 * r + 3] = d;
                ++got;
            }
        } else if (const char* q = std::strstr(line, "camera fov ")) {
            std::sscanf(q, "camera fov %f", &fov);
            if (const char* s = std::strstr(q, "aspect ")) std::sscanf(s, "aspect %f", &aspect);
        } else if (const char* m = std::strstr(line, "model position ")) {
            std::sscanf(m, "model position %f %f %f rotation %f %f %f", &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]);
        }
    }
    std::fclose(f);
    if (got < 3) { logf(Level::Error, "camera", "%s has no view matrix", narrow(path).c_str()); return; }
    g.viewer.set_game_camera(rows, fov, aspect, pos, rot);
    logf(Level::Info, "camera", "game camera: fov %.1f aspect %.3f model at %.1f %.1f %.1f turned %.3f", fov, aspect, pos[0], pos[1],
         pos[2], rot[1]);
}

// ---- the screenshot tests' capture (the Studio review's decision 7): the whole window, 3D view included ----
void capture(const std::wstring& path) {
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    const int w = rc.right, h = rc.bottom;
    HDC wdc = GetDC(g.hwnd), mdc = CreateCompatibleDC(wdc);
    BITMAPINFO bi{};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mdc, bmp);
    PrintWindow(g.hwnd, mdc, PW_CLIENTONLY | 2 /* PW_RENDERFULLCONTENT */);
    // the area kept: the whole client, or with --shot-model only the 3D view's child window
    int x0 = 0, y0 = 0, cw = w, ch = h;
    if (g.shot_model && g.viewer.hwnd()) {
        RECT vr;
        GetWindowRect(g.viewer.hwnd(), &vr);
        MapWindowPoints(nullptr, g.hwnd, reinterpret_cast<POINT*>(&vr), 2);
        x0 = std::clamp((int)vr.left, 0, w); y0 = std::clamp((int)vr.top, 0, h);
        cw = std::clamp((int)vr.right, x0, w) - x0; ch = std::clamp((int)vr.bottom, y0, h) - y0;
        // (a 3D view reaching past the client is cut, and its picture no longer centred on the camera's aim: the move
        // sheets' comparisons need to know)
        if (cw != vr.right - vr.left || ch != vr.bottom - vr.top)
            logf(Level::Warn, "screenshot", "the 3D view (%ld x %ld at %ld, %ld) is cut to %d x %d by the %d x %d client",
                 vr.right - vr.left, vr.bottom - vr.top, vr.left, vr.top, cw, ch, w, h);
    }
    std::vector<uint8_t> rgba((size_t)cw * ch * 4);
    const uint8_t* src = static_cast<const uint8_t*>(bits);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            const uint8_t* s = src + ((size_t)(y0 + y) * w + (x0 + x)) * 4;
            uint8_t* o = &rgba[((size_t)y * cw + x) * 4];
            o[0] = s[2]; o[1] = s[1]; o[2] = s[0]; o[3] = 255;
        }
    OrreImage im{cw, ch, rgba.data()};
    const int ok = orre_png_write(narrow(path).c_str(), &im);
    logf(Level::Info, "shot", "%dx%d -> %s", w, h, ok ? "written" : "FAILED");
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(g.hwnd, wdc);
}

// ---- input ----
// Back / Forward in the current tab (the arrows, the mouse's side buttons, Alt+Left / Alt+Right)
bool can_go(int step) {
    if (g.cur_tab < 0 || g.cur_tab >= (int)g.tab_pos.size()) return false;
    const int p = g.tab_pos[(size_t)g.cur_tab] + step;
    return p >= 0 && p < (int)g.tab_hist[(size_t)g.cur_tab].size();
}
void go(int step) {
    if (!can_go(step)) return;
    const size_t t = (size_t)g.cur_tab;
    g.tab_pos[t] += step;
    g.navigating = true;
    g.editor = 0;
    open_page(g.tab_hist[t][(size_t)g.tab_pos[t]]);  // (a Pokémon, or Home)
    g.navigating = false;
    place_children();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// opens an editor that isn't built yet (its "Coming soon" page): types, abilities, items, the Battle editor's rules
// An editor from the rail (or a link to one), in the shown tab, or in a new tab with Ctrl (snatcherLeo, 2026-11-22: "if I hold
// the "CTRL" button and click on one of the left-hand icons, it should open it in a new tab"). The Pokémon icon goes back
// to the Pokémon this window showed last (or the list's first) when the tab shows anything else (Home has no list to
// pick from: without this the icon led nowhere, snatcherLeo, 2026-11-19: "now I'm stuck"); on a Pokémon it stays, unless Ctrl
// asks for a new tab (then the same Pokémon opens there).
// *Updated 2026-11-22* (snatcherLeo: each icon should "remember the LAST PLACE we were in, if we ever entered that editor",
// "Yes, build it"): the icon goes to its editor's last place in this window (WinState::places: the page and its scroll),
// else the editor's start (the Pokémon: the list's first; the others: their page). Already in that editor, a plain
// click stays where it is; Ctrl opens the last place in a new tab even then.
void open_editor(int ed, bool new_tab = false) {
    if (ed < 0 || ed >= kEditorCount) return;
    if (!new_tab && page_owner(cur_page()) == ed) { place_children(); return; }  // (already there)
    const WinState::Place place = g.places[ed];
    int page = place.page;
    if (ed == 0) {
        // with no disc there is no Pokémon to open: Home, where the no-disc strip has Add disc…, and a short note
        // (5.9's laptop sitting, 2026-11-24: the click used to do nothing at all)
        if (!g.disc) {
            if (!on_home()) open_page(kHome);
            show_toast(tx(g.loading ? "studio.disc.reading" : "studio.disc.needed"));
            logf(Level::Info, "disc", "the Pokémon editor needs a disc: %s", g.loading ? "it is still being read" : "Home's strip shown");
            return;
        }
        if (page < 0 || page >= (int)g.all.size()) page = g.last_mon >= 0 ? g.last_mon : g.shown.empty() ? -1 : g.shown.front();
        if (page < 0) return;
    } else if (page_editor(page) != ed) {
        page = editor_page(ed);
    }
    open_page(page, new_tab);
    if (place.page == page) g.pscroll = place.scroll;  // (where that page was scrolled to)
    place_children();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// ---- the action trail (part 3 of the loggers, snatcherLeo 2026-11-22: "you can see (either via tracking) what I was doing
// within the app"; studio/diag/trail.h) ----
// What's open, after every note: the page, the Pokémon and form, the tab, and on the Moves tab the move and the preview's
// switches, so each line reads on its own ("click Turn 1 | Bulbasaur (1), Moves, Solarbeam (76), turn 1, Arena")
// *Updated 2026-11-22 (4.3 part 3, the logging list's items 11 and 15, snatcherLeo: the place said once, in plain names):* no
// longer added to every line; a "now on: ..." line says it whenever it changes (note_place), and the words are the
// ones on screen ("Taillow, Animations tab, Idle"), without the internal numbers ("(304) form 0", "Idle (20000)",
// "parts 0") the earlier lines carried.
std::string trail_context() {
    char b[240];
    const int cp = cur_page();
    if (cp < 0) {
        if (page_editor(cp)) std::snprintf(b, sizeof b, "%s editor", kEditors[page_editor(cp)]);
        else std::snprintf(b, sizeof b, "%s", cp == kSettings ? "Settings" : "Home");
        return b;
    }
    if (g.selected < 0 || g.selected >= (int)g.all.size()) return "no Pokémon";
    static const char* tabs[] = {"Overview", "Moves", "Looks", "Sound", "Animations", "Status"};
    int n = std::snprintf(b, sizeof b, "%s, %s tab%s", mon_name().c_str(), g.tab >= 0 && g.tab < kTabCount ? tabs[g.tab] : "?",
                          g.shiny ? ", shiny" : "");
    if (preview_tab(g.tab) && n > 0 && n < (int)sizeof b) {
        const LearnRow* r = chosen_row();
        static const char* parts[] = {"the move", "the follow-up", "both parts"};
        if (g.tab == kTabAnimations)
            std::snprintf(b + n, sizeof b - (size_t)n, ", %s, %s", r ? narrow(r->name).c_str() : "no animation",
                          g.arena ? "Arena stage" : g.plain ? "Plain stage" : "Studio view");
        else
            std::snprintf(b + n, sizeof b - (size_t)n, ", %s, turn %d, %s, %s%s", r ? narrow(r->name).c_str() : "no move", g.turn,
                          parts[std::clamp(g.parts, 0, 2)], g.arena ? "Arena stage" : g.plain ? "Plain stage" : "Studio view",
                          g.battle_cam ? "" : ", battle camera off");
    }
    return b;
}
// "now on: ..." when the place has changed since this window last said it (before an action is noted, and once a
// second from the performance timer, so a change made without a click shows too)
void log_place() {
    std::string now = trail_context();
    if (now == g.place_logged) return;
    g.place_logged = std::move(now);
    trail_note("now on", g.place_logged.c_str());
}
// a click's target in words (the ids in the HIT_ enum)
std::string hit_words(int id) {
    static const std::pair<int, const char*> kNamed[] = {
        {HIT_INSPECTOR, "inspector"}, {HIT_SPIN, "spin"}, {HIT_RESET, "reset view"}, {HIT_CHOOSE_DISC, "choose disc"}, {HIT_HOME_ADD_DISC, "Home's Add disc"},
        {HIT_DISC_GONE_OPEN, "removed-disc bar: Discs"}, {HIT_DISC_GONE_OK, "removed-disc bar: OK"},
        {HIT_MOTION, "motion"}, {HIT_MODEL, "model"}, {HIT_OPEN_EDITOR, "open editor"}, {HIT_IDLE, "Idle"},
        {HIT_STANDING, "Standing pose"}, {HIT_NORMAL, "Normal"}, {HIT_SHINY, "Shiny"}, {HIT_COPY, "copy"},
        {HIT_CREATE_MOD, "Create a mod"}, {HIT_PARTICLES, "Particles switch"}, {HIT_LIGHTING, "Game lighting switch"},
        {HIT_BATTLECAM, "Battle camera switch"}, {HIT_TARGET, "Target"}, {HIT_HOME, "Home"}, {HIT_PTAB_ADD, "new tab"},
        {HIT_BACK, "Back"}, {HIT_FORWARD, "Forward"}, {HIT_TAB_MENU, "tab menu"}, {HIT_TAB_FIRST, "first tab"},
        {HIT_TAB_LAST, "last tab"}, {HIT_SETTINGS, "Settings"}, {HIT_PLAY, "Play / Pause"}, {HIT_STEP_BACK, "step back"},
        {HIT_STEP_FWD, "step forward"}, {HIT_VIEW_STUDIO, "Studio view"}, {HIT_VIEW_PLAIN, "Plain stage"},
        {HIT_VIEW_ARENA, "Arena"}, {HIT_MOVE_EDITOR, "open in Move editor"}, {HIT_TIMELINE, "timeline"},
        {HIT_RATE, "rate menu"}, {HIT_TURN1, "Turn 1"}, {HIT_TURN2, "Turn 2"}, {HIT_PARTS_MOVE, "parts: Move"},
        {HIT_PARTS_FOLLOW, "parts: Follow-up"}, {HIT_PARTS_BOTH, "parts: Both"}, {HIT_GSEARCH, "search everything"},
        {HIT_GSEARCH_KEY, "Ctrl K chip (palette)"}, {HIT_SEARCH_CLEAR, "clear the list search"},
        {HIT_OV_STAGE, "Overview stage menu"}, {HIT_CRY, "play the cry"}, {HIT_CRY_FAINT, "play the fainting preview"},
        {HIT_CRY_SOUND, "cries: to the Sound tab"}, {HIT_SEE_ANIM, "see the move's animation"},
        // Report a problem (named 2026-11-22: snatcherLeo's first saved report showed them as "id 7355" and "id 7356")
        {HIT_SET_REPORT, "Settings: Report a problem"}, {HIT_SET_REPORTS_DIR, "Settings: open the reports folder"},
        {HIT_SET_LOGS_DIR, "Settings: open the logs folder"}, {HIT_REP_BG, "outside the report panel"},
        {HIT_REP_TICK, "report: screenshots tick"}, {HIT_REP_TICK + 1, "report: what's open tick"}, {HIT_REP_SAVE, "report: Save report"},
        {HIT_REP_CANCEL, "report: Cancel"}, {HIT_REP_OPEN, "report: Open folder"}, {HIT_REP_DONE, "report: Done"},
        {HIT_CRASH_REPORT, "crash bar: Make a report"}, {HIT_CRASH_DISMISS, "crash bar: Not now"},
        {HIT_DISC_ADD, "discs: US Add / Replace"}, {HIT_DISC_ADD + 1, "discs: European Add / Replace"},
        {HIT_DISC_ADD + 2, "discs: Japanese Add / Replace"}, {HIT_DISC_REMOVE, "discs: US Remove"},
        {HIT_DISC_REMOVE + 1, "discs: European Remove"}, {HIT_DISC_REMOVE + 2, "discs: Japanese Remove"},
        {HIT_DISC_COPY, "discs: Copy into OrrePort"}, {HIT_DISC_MOVE, "discs: Move"}, {HIT_DISC_CANCEL, "discs: Cancel"},
        {HIT_DISC_CONFIRM, "discs: Remove (confirmed)"}, {HIT_DISC_SHOW, "discs: US Open file location"},
        {HIT_DISC_SHOW + 1, "discs: European Open file location"}, {HIT_DISC_SHOW + 2, "discs: Japanese Open file location"},
        {HIT_UPD_DOWNLOAD, "update: Download"}, {HIT_UPD_NOTES, "update: What's new"}, {HIT_UPD_RESTART, "update: Restart now"},
        {HIT_UPD_RETRY, "update: Try again"}, {HIT_UPD_CHECK, "update: Check for updates"},
        {HIT_UPD_SW_AUTO, "update: Download automatically switch"}, {HIT_UPD_SW_CHECK, "update: Check for updates switch"},
        {HIT_UPD_GOBACK, "update: Go back"}, {HIT_UPD_GOBACK_YES, "update: Go back (confirmed)"}, {HIT_UPD_GOBACK_NO, "update: Go back cancelled"},
        {HIT_UPD_GOBACK_BG, "outside the Go back pop-up"},
        {HIT_OPEN_ORREPORT, "Home: Open OrrePort / OrrePort is opened"}, {HIT_UPD_CLOSE_OTHER, "update: Close OrrePort"}};
    for (const auto& [k, w] : kNamed) if (k == id) return w;
    char b[64];
    auto range = [&](int lo, int hi, const char* what) {
        if (id < lo || id > hi) return false;
        std::snprintf(b, sizeof b, "%s %d", what, id - lo);
        return true;
    };
    if (id >= HIT_RAIL && id < HIT_RAIL + kEditorCount) { std::snprintf(b, sizeof b, "rail: %s editor", kEditors[id - HIT_RAIL]); return b; }
    if (id >= HIT_TAB && id < HIT_TAB + kTabCount) { std::snprintf(b, sizeof b, "tab %s", narrow(tx(kTabs[id - HIT_TAB])).c_str()); return b; }
    if (id >= HIT_ROW && id <= HIT_ROW_END) {
        const int i = id - HIT_ROW;
        if (i >= 0 && i < (int)g.shown.size() && g.shown[(size_t)i] >= 0 && g.shown[(size_t)i] < (int)g.all.size()) {
            const Entry& e = g.all[(size_t)g.shown[(size_t)i]];
            std::snprintf(b, sizeof b, "list: %s", narrow(e.name).c_str());  // (the name only: item 15, plain names)
            return b;
        }
    }
    if (id >= HIT_AROW && id <= HIT_AROW_END && id - HIT_AROW < g.viewer.animation_count()) {  // (the animation's name, as listed)
        bool unused = false, noted = false;
        std::snprintf(b, sizeof b, "animation: %s", narrow(anim_name(id - HIT_AROW, &unused, &noted)).c_str());
        return b;
    }
    if (range(HIT_ROW, HIT_ROW_END, "list row") || range(HIT_EVO, HIT_EVO_END, "evolution member") ||
        range(HIT_FORM, HIT_FORM_END, "form") || range(HIT_EVO_METHOD, HIT_EVO_METHOD_END, "evolution method") ||
        range(HIT_LROW, HIT_LROW_END, "learnset row") || range(HIT_LGROUP, HIT_LGROUP + 9, "learnset group") ||
        range(HIT_AROW, HIT_AROW_END, "animation") || range(HIT_USED, HIT_USED_END, "used by move") ||
        range(HIT_PTAB_CLOSE, HIT_PTAB_CLOSE + 99, "close page tab") || range(HIT_PTAB, HIT_PTAB + 99, "page tab") ||
        range(HIT_SRES, HIT_SRES_END, "search result") || range(HIT_SET_PANE, HIT_SET_PANE + 9, "settings pane") ||
        range(HIT_THEME_OPT, HIT_THEME_OPT + 9, "theme option") || range(HIT_TYPE, HIT_TYPE + 17, "type") ||
        range(HIT_MATCH, HIT_MATCH + 17, "matchup type") || range(HIT_STAT, HIT_STAT + 9, "stat") ||
        range(HIT_ABILITY, HIT_ABILITY + 9, "ability") || range(HIT_TRAIN, HIT_TRAIN + 19, "training row") ||
        range(HIT_ERA, HIT_ERA + 19, "era") || range(HIT_LATER, HIT_LATER + 39, "later button"))
        return b;
    std::snprintf(b, sizeof b, "id %d", id);
    return b;
}
void trail(const char* category, const std::string& what) {
    log_place();  // (the place, when it changed, on its own line first)
    trail_note(category, what.c_str());
}
// The performance log's tick (log.cpp perf_tick): one timer for the whole program (a thread timer, not a window's, so
// several windows don't tick several times), with where the front window is: "home", "settings", an editor's name,
// or "pokemon-<tab>". Windows thread timer; Linux / macOS: the toolkit's repeating timer
void CALLBACK perf_timer(HWND, UINT, UINT_PTR, DWORD) {
    static const char* tabs[] = {"overview", "moves", "looks", "sound", "animations", "status"};
    char where[48];
    const int cp = cur_page();
    if (cp == kHome) std::snprintf(where, sizeof where, "home");
    else if (cp == kSettings) std::snprintf(where, sizeof where, "settings");
    else if (page_editor(cp)) std::snprintf(where, sizeof where, "%s", kEditors[page_editor(cp)]);
    else std::snprintf(where, sizeof where, "pokemon-%s", g.tab >= 0 && g.tab < kTabCount ? tabs[g.tab] : "?");
    perf_tick(where);
    // the hang watch's heartbeat (item 6): it moves while this thread answers its messages; Windows' modal loops (a
    // window being dragged, a menu, the file dialog) still run this timer, so only a real stall stops it. OrrePort's
    // watchdog (pc_crash_watchdog, port/src/pc_crash_win32.c) prints this thread's stack and the crash report's trail
    // after 10 s without a beat; started on the first tick, so a slow start before the message loop isn't taken for a
    // hang. Windows only so far (the watchdog does nothing on POSIX yet).
    g_heartbeat = g_heartbeat + 1;  // (only this thread writes it)
    static bool watching = false;
    if (!watching) { watching = true; pc_crash_watchdog(&g_heartbeat, 10); }
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (last && now - last > 10000)  // (after the fact: the watchdog's stack shows where; this says how long)
        logf(Level::Warn, "hang", "Studio didn't answer for %.1f s%s%s", (now - last) / 1000.0, g_modal ? " while " : "", g_modal ? g_modal : "");
    last = now;
    log_place();  // ("now on: ..." when the front window's place changed without an action)
    discs_watch();  // (the ROM folders: a disc added, replaced or removed by the hub, or by hand, 4.4)
    updates_poll();  // (the updater's shared state: found, downloading or ready, by Studio or the hub, 4.4)
    // OrrePort open from this folder, and its game running (pc_apps: two named mutexes, cheap to look at); Home's button
    // and the update line follow, redrawn only on a change
    if (!g_apps_root.empty()) {
        const bool hub = pc_apps_running(PC_APP_HUB, g_apps_root.c_str()) != 0, game = pc_apps_game_running(g_apps_root.c_str()) != 0;
        if (hub != g_hub_open || game != g_game_running) {
            logf(Level::Info, "apps", "OrrePort %s%s", hub ? "is open" : "is closed", game ? ", its game running" : "");
            g_hub_open = hub;
            g_game_running = game;
            repaint_all();
        }
    }
    // the state line every 30 s (items 13 and 14): the clock, every window with its tabs, memory (log_state)
    // (the highs for the closing summary are checked every second, so a burst of tabs between two state lines counts)
    static int ticks = 0;
    const bool state_now = ++ticks % 30 == 0;
    if (g_hang_test && ticks == 3) {
        logf(Level::Info, "test", "hang test: the UI thread stops for 12 s now");
        Sleep(12000);
    }
    std::string wins;
    int tabs_all = 0;
    for (const WindowSlot& w : g_windows) {
        const WinState& s = w.active ? static_cast<const WinState&>(g) : w.parked;
        const int n = (int)s.page_tabs.size();
        tabs_all += n;
        if (!state_now) continue;
        wins += (wins.empty() ? "" : ", ") + std::string("w") + std::to_string(s.win_no) + " " + std::to_string(n) + (n == 1 ? " tab" : " tabs");
        if (w.hwnd == GetForegroundWindow()) wins += " (in front)";
    }
    log_note_open((int)g_windows.size(), tabs_all);
    if (state_now) log_state((std::to_string(g_windows.size()) + (g_windows.size() == 1 ? " window: " : " windows: ") + wins).c_str());
}
// A crash report's last part: where Studio was, Windows' loop if one held it, then the trail's newest 50 actions
// (pc_crash_report_hook: memory reads and a file write only; the place string is built before, never here)
void crash_trail() {
    static char buf[50 * 260];
    static const char head[] = "[pc_crash] the last actions before the crash (oldest first):\n";
    DWORD w;
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    static const char on[] = "[pc_crash] the front window was on: ";
    WriteFile(err, on, (DWORD)(sizeof on - 1), &w, nullptr);
    WriteFile(err, g.place_logged.c_str(), (DWORD)g.place_logged.size(), &w, nullptr);
    WriteFile(err, "\n", 1, &w, nullptr);
    if (const char* m = g_modal) {
        static const char in[] = "[pc_crash] Windows' own loop held Studio: ";
        WriteFile(err, in, (DWORD)(sizeof in - 1), &w, nullptr);
        WriteFile(err, m, (DWORD)strlen(m), &w, nullptr);
        WriteFile(err, "\n", 1, &w, nullptr);
    }
    trail_last(50, buf, sizeof buf);
    WriteFile(err, head, (DWORD)(sizeof head - 1), &w, nullptr);
    WriteFile(err, buf, (DWORD)strlen(buf), &w, nullptr);
}

void click(int id) {
    trail("click", hit_words(id));  // (before acting: if the click crashes Studio, the report's last line names it)
    const bool ctrl = GetKeyState(VK_CONTROL) < 0;  // Ctrl+click: open in a new tab (snatcherLeo through C, v3)
    if (id >= HIT_RAIL && id < HIT_RAIL + kEditorCount) {
        open_editor(id - HIT_RAIL, ctrl);
    } else if (id >= HIT_EVO && id <= HIT_EVO_END) {
        // an evolution member: go to that Pokémon (snatcherLeo: "you can click Ivysaur in here, and get to Ivy")
        const auto fam = g.selected >= 0 ? evolution_family(g.all[g.selected].index) : std::vector<EvoStep>{};
        const size_t i = (size_t)(id - HIT_EVO);
        if (i < fam.size()) {
            auto it = g.by_index.find(fam[i].index);
            if (it != g.by_index.end()) {
                // a member hidden by the search would be unreachable in the list: clear the search first
                if (std::find(g.shown.begin(), g.shown.end(), it->second) == g.shown.end()) SetWindowTextW(g.search, L"");
                select(it->second, ctrl);
            }
        }
    } else if (id >= HIT_EVO_METHOD && id <= HIT_EVO_METHOD_END) {
        // an item method opens the Item editor; a friendship method, Friendship in the Battle editor's rules
        const auto fam = g.selected >= 0 ? evolution_family(g.all[g.selected].index) : std::vector<EvoStep>{};
        const size_t i = (size_t)(id - HIT_EVO_METHOD);
        if (i < fam.size()) open_editor(method_is_item(fam[i].kind) ? kEditorItem : kEditorBattle, ctrl);
    } else if ((id >= HIT_TYPE && id < HIT_TYPE + 18) || (id >= HIT_MATCH && id < HIT_MATCH + 18)) {
        open_editor(kEditorType, ctrl);
    } else if (id >= HIT_ABILITY && id < HIT_ABILITY + 2) {
        open_editor(kEditorAbility, ctrl);
    } else if (id == HIT_BACK || id == HIT_FORWARD) {
        go(id == HIT_BACK ? -1 : 1);
    } else if (id == HIT_HOME) {
        // the shown tab becomes Home, a visit in its history like any other (Back returns to the Pokémon); with no
        // tab open, a Home tab opens
        g.editor = 0;
        if (!on_home()) open_page(kHome);
        place_children();
    } else if (id == HIT_SETTINGS) {
        open_page(kSettings);  // from Home: this tab becomes Settings (Back returns to Home)
    } else if (id >= HIT_SET_PANE && id < HIT_SET_PANE + 10) {
        g.settings_pane = id - HIT_SET_PANE;
    } else if (id == HIT_SET_REPORT) {
        open_report();
    } else if (id >= HIT_REP_TICK && id < HIT_REP_TICK + 2) {
        g.report_tick[id - HIT_REP_TICK] = !g.report_tick[id - HIT_REP_TICK];
    } else if (id == HIT_REP_SAVE) {
        save_report();
    } else if (id == HIT_REP_CANCEL || id == HIT_REP_DONE) {
        close_report();
    } else if (id == HIT_REP_OPEN) {
        open_folder(reports_dir(), g.report_file);
    } else if (id == HIT_DISC_GONE_OPEN || id == HIT_DISC_GONE_OK) {
        // (Discs: Settings › Discs, where the US row says the same and has Add…; both hide the bar in every window)
        g_discs.gone_hidden = true;
        if (id == HIT_DISC_GONE_OPEN) { g.settings_pane = 1; open_page(kSettings); }
        place_children();
        repaint_all();
    } else if (id == HIT_CRASH_REPORT || id == HIT_CRASH_DISMISS) {
        g.crash_bar = false;
        g.report_prev = id == HIT_CRASH_REPORT;
        place_children();  // (the page takes back the bar's strip)
        if (g.report_prev) open_report();
    } else if (id == HIT_SET_REPORTS_DIR || id == HIT_SET_LOGS_DIR) {
        open_folder(id == HIT_SET_REPORTS_DIR ? reports_dir() : g.logs_dir, L"");
    } else if (id >= HIT_THEME_OPT && id < HIT_THEME_OPT + 3) {
        // the theme picked in Settings: saved in Studio's preferences and shown in every window at once
        g.studio_theme = id - HIT_THEME_OPT;
        save_prefs();
        apply_theme_all();
        logf(Level::Info, "settings", "theme: %s", g.studio_theme == 0 ? "follow Windows" : g.studio_theme == 1 ? "light" : "dark");
    } else if (id >= HIT_PTAB && id < HIT_PTAB + 100) {
        const int t = id - HIT_PTAB;
        if (t < (int)g.page_tabs.size()) {
            g.cur_tab = t;
            g.editor = 0;
            const int idx = g.page_tabs[(size_t)t];
            if (idx != g.selected) { g.selected = -1; g.navigating = true; open_page(idx); g.navigating = false; }
            place_children();
        }
    } else if (id >= HIT_PTAB_CLOSE && id < HIT_PTAB_CLOSE + 100) {
        const int t = id - HIT_PTAB_CLOSE;
        if (t < (int)g.page_tabs.size()) {
            g.page_tabs.erase(g.page_tabs.begin() + t);
            g.tab_hist.erase(g.tab_hist.begin() + t);
            g.tab_pos.erase(g.tab_pos.begin() + t);
            if (t < (int)g.tab_sel.size()) g.tab_sel.erase(g.tab_sel.begin() + t);
            if (g.tab_anchor >= (int)g.page_tabs.size()) g.tab_anchor = -1;
            if (g.page_tabs.empty()) g.cur_tab = -1;
            else if (g.cur_tab >= t) g.cur_tab = std::max(0, g.cur_tab - 1);
            if (g.cur_tab >= 0) {
                const int idx = g.page_tabs[(size_t)g.cur_tab];
                if (idx != g.selected) { g.selected = -1; g.navigating = true; open_page(idx); g.navigating = false; }
            }
            place_children();
            // the last tab of a second or later window closes that window, as in Chrome (the last window stays, with a
            // fresh Home tab); posted, so the window isn't destroyed while this click is still being handled
            if (g.page_tabs.empty() && g_windows.size() > 1) PostMessageW(g.hwnd, WM_CLOSE, 0, 0);
            else if (g.page_tabs.empty()) open_page(kHome);
        }
    } else if (id >= HIT_SRES && id <= HIT_SRES_END) {
        choose_result(id - HIT_SRES, GetKeyState(VK_CONTROL) < 0);
    } else if (id == HIT_GSEARCH) {
        SetFocus(g.gsearch);
    } else if (id >= HIT_AROW && id <= HIT_AROW_END) {
        // the Animations tab: that animation, played on its own (sync_preview, the shared preview). A click plays it
        // from its start, the chosen one again included, as a move's row does on the Moves tab (snatcherLeo, 2026-11-22: "if I
        // click on the move name, it plays the animation. In animation sub-tab, if I click on the animation name, it
        // doesn't play the animation like moves do (but it should, without me having to hit play button)")
        if (id - HIT_AROW != g.anim_sel) {
            g.anim_sel = id - HIT_AROW;
            g.anim_for = g.selected * 64 + g.form;
            g.used_scroll = 0;
            g.viewer.stop_once();
            sync_preview();
        }
        const int a = chosen_animation();
        if (a >= 0) g.viewer.play_once(a);
        playhead_timer();
    } else if (id >= HIT_USED && id <= HIT_USED_END) {
        // a Used by chip: the move in the Move editor (snatcherLeo: "clicking on any move can bring us to the Move editor";
        // its page says it's coming until it's built)
        open_editor(1, GetKeyState(VK_CONTROL) < 0);
    } else if (id == HIT_SEE_ANIM) {
        // the Moves tab's "See it in Animations": that animation chosen there
        const int a = chosen_animation();
        if (a >= 0) {
            g.anim_sel = a;
            g.anim_for = g.selected * 64 + g.form;  // (so the Animations tab keeps it, not its list's first)
        }
        g.used_scroll = 0;
        click(HIT_TAB + kTabAnimations);
    } else if (id == HIT_CRY || id == HIT_CRY_FAINT) {
        play_cry(id == HIT_CRY_FAINT);
    } else if (id == HIT_CRY_SOUND) {
        click(HIT_TAB + kTabSound);  // (the Cries card's link: the Sound tab, where both cries are)
    } else if (id == HIT_GSEARCH_KEY) {
        open_palette();  // the chip names the keys; clicking it does what they do
    } else if (id == HIT_SEARCH_CLEAR) {
        // the whole list again (the edit's EN_CHANGE filters it and drops the button), the selected Pokémon kept and
        // scrolled into view, typing still in the box for a new search
        SetWindowTextW(g.search, L"");
        ensure_visible();
        SetFocus(g.search);
    } else if (id == HIT_PAL_BG) {
        if (!pal_rect(layout()).in(g.mouse_x, g.mouse_y)) close_search();
        else SetFocus(g.psearch);  // a click inside the card keeps typing in its field
    } else if (id == HIT_TAB_FIRST || id == HIT_TAB_LAST) {
        g.tab_scroll = id == HIT_TAB_FIRST ? 0.0f : 1e9f;  // clamped to the strip's end when drawn
    } else if (id == HIT_TAB_MENU) {
        // every open tab in a native menu at the "▾", the shown one checked (Windows' menus; Linux / macOS: the
        // toolkit's own popup menu)
        HMENU m = CreatePopupMenu();
        for (size_t i = 0; i < g.page_tabs.size(); ++i)
            AppendMenuW(m, MF_STRING | ((int)i == g.cur_tab ? MF_CHECKED : 0), 1 + i, tab_label(g.page_tabs[i]).c_str());
        POINT p;
        GetCursorPos(&p);
        const int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, p.x, p.y, 0, g.hwnd, nullptr);
        DestroyMenu(m);
        if (pick > 0) click(HIT_PTAB + pick - 1);
    } else if (id == HIT_PTAB_ADD) {
        // a new tab as Chrome and Edge make one (snatcherLeo, 2026-11-19): a Home tab at the end, shown (it used to copy the
        // shown Pokémon's tab). The search field is left alone: a cursor waiting there looked like a stray selection
        // (snatcherLeo, same day: "the search field at the top has a cursor where it shouldn't")
        open_page(kHome, true);
    } else if (id == HIT_RATE) {
        // the preview rate's choices in a native menu under the pointer, the current one checked (Windows' menus;
        // Linux / macOS: the toolkit's popup menu); one setting for every window, kept in studio.ini
        HMENU m = CreatePopupMenu();
        for (int k = 0; k < (int)std::size(kRates); ++k)
            AppendMenuW(m, MF_STRING | (kRates[k] == g.preview_rate ? MF_CHECKED : 0), 1 + k,
                        (kRates[k] == 0 ? tx("studio.rate.menu_game") : rate_label(kRates[k])).c_str());
        POINT p;
        GetCursorPos(&p);
        const int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, p.x, p.y, 0, g.hwnd, nullptr);
        DestroyMenu(m);
        if (pick > 0 && kRates[pick - 1] != g.preview_rate) {
            g.preview_rate = kRates[pick - 1];
            const HWND back = g.hwnd;
            for (size_t k = 0; k < g_windows.size(); ++k) {
                activate(g_windows[k].hwnd);
                g.viewer.set_rate(g.preview_rate);
                InvalidateRect(g.hwnd, nullptr, FALSE);
            }
            activate(back);
            save_prefs();
            logf(Level::Info, "viewer", "preview rate: %d (0 = the game's)", g.preview_rate);
        }
    } else if (id == HIT_PARTICLES) {
        // one setting for every window (each has its own 3D view), kept in studio.ini
        g.particles_on = !g.particles_on;
        const HWND back = g.hwnd;
        for (size_t k = 0; k < g_windows.size(); ++k) {
            activate(g_windows[k].hwnd);
            g.viewer.set_particles(g.particles_on);
            InvalidateRect(g.hwnd, nullptr, FALSE);
        }
        activate(back);
        save_prefs();
        logf(Level::Info, "viewer", "particles %s", g.particles_on ? "on" : "off");
    } else if (id == HIT_LIGHTING) {
        // one setting for every window, kept in studio.ini (as Particles)
        g.game_light = !g.game_light;
        const HWND back = g.hwnd;
        for (size_t k = 0; k < g_windows.size(); ++k) {
            activate(g_windows[k].hwnd);
            g.viewer.set_game_lighting(g.game_light);
            InvalidateRect(g.hwnd, nullptr, FALSE);
        }
        activate(back);
        save_prefs();
        logf(Level::Info, "viewer", "lighting: %s", g.game_light ? "the game's" : "Studio's even light");
    } else if (id == HIT_TARGET) {
        // the target's choices in a native menu under the chip (Windows' menus; Linux / macOS: the toolkit's popup
        // menu): the box, the last few Pokémon chosen (the current one checked), then any Pokémon through the palette
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING | (g.target_species < 0 ? MF_CHECKED : 0), 1, tx("studio.moves.target_box").c_str());
        int n = 0;
        for (int s : g.target_recent) {
            if (entry_of_species(s) < 0) continue;
            if (n++ == 0) AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(m, MF_STRING | (s == g.target_species ? MF_CHECKED : 0), (UINT_PTR)(100 + s), target_name(s).c_str());
        }
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 2, tx("studio.moves.target_choose").c_str());
        POINT p;
        GetCursorPos(&p);
        const int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, g.hwnd, nullptr);
        DestroyMenu(m);
        if (pick == 1 && g.target_species >= 0) set_target(-1);
        else if (pick == 2) open_target_palette();
        else if (pick >= 100 && pick - 100 != g.target_species) set_target(pick - 100);
    } else if (id == HIT_BATTLECAM) {
        // one setting for every window, kept in studio.ini
        g.battle_cam = !g.battle_cam;
        const HWND back = g.hwnd;
        for (size_t k = 0; k < g_windows.size(); ++k) {
            activate(g_windows[k].hwnd);
            g.viewer.set_battle_camera(g.battle_cam);
            InvalidateRect(g.hwnd, nullptr, FALSE);
        }
        activate(back);
        save_prefs();
        logf(Level::Info, "viewer", "battle camera %s", g.battle_cam ? "on" : "off");
    } else if (id == HIT_IDLE || id == HIT_STANDING) {
        g.viewer.set_idle(id == HIT_IDLE);
        logf(Level::Info, "viewer", "motion: %s", id == HIT_IDLE ? "idle" : "standing pose");
    } else if (id == HIT_NORMAL || id == HIT_SHINY) {
        if (g.shiny != (id == HIT_SHINY)) { g.shiny = id == HIT_SHINY; show_form(); }
    } else if (id == HIT_COPY && g.selected >= 0) {
        // Copy / share: the Pokémon as a line of text on the clipboard (more formats later: a Showdown set, a link)
        const Entry& e = g.all[g.selected];
        const std::wstring types = e.s.types[1] != e.s.types[0] ? type_name(e.s.types[0]) + L" / " + type_name(e.s.types[1]) : type_name(e.s.types[0]);
        std::wstring abil = ability_name(e.s.abilities[0]);
        if (e.s.abilities[1]) abil += L" / " + ability_name(e.s.abilities[1]);
        const std::wstring s = tx("studio.copy.text", {{"name", e.name}, {"category", e.category}, {"national", number3(e.s.national_dex)},
                                                       {"types", types}, {"hp", std::to_wstring(e.s.base_stats[0])},
                                                       {"atk", std::to_wstring(e.s.base_stats[1])}, {"def", std::to_wstring(e.s.base_stats[2])},
                                                       {"spa", std::to_wstring(e.s.base_stats[3])}, {"spd", std::to_wstring(e.s.base_stats[4])},
                                                       {"spe", std::to_wstring(e.s.base_stats[5])}, {"abilities", abil}});
        if (OpenClipboard(g.hwnd)) {  // Windows' clipboard (Qt: QClipboard; AppKit: NSPasteboard)
            EmptyClipboard();
            if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t))) {
                std::memcpy(GlobalLock(h), s.c_str(), (s.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
            }
            CloseClipboard();
            show_toast(tx("studio.toast.copied"));
        }
    } else if (id >= HIT_FORM && id <= HIT_FORM_END) {
        // another form of the same species: its pictures and model (the list keeps the species' first form)
        const int f = id - HIT_FORM;
        if (f >= 0 && f < (int)g.forms.size() && g.forms[(size_t)f].available && f != g.form) {
            g.form = f;
            show_form();
        }
    } else if (id >= HIT_ROW && id <= HIT_ROW_END) {
        const int pos = id - HIT_ROW;
        if (pos >= 0 && pos < (int)g.shown.size()) select(g.shown[(size_t)pos], ctrl);
    } else if (id >= HIT_TAB && id < HIT_TAB + kTabCount) {
        g.tab = id - HIT_TAB;
        sync_preview();
        playhead_timer();
        place_children();
    } else if (id >= HIT_LGROUP && id < HIT_LGROUP + 4) {
        // a learnset group: the chosen move stays when the group has it too (SolarBeam by level and by TM22)
        const int before = g.learn_move;
        g.learn_group = id - HIT_LGROUP;
        ensure_learnset();
        if (g.learn_move != before) g.viewer.stop_once();
        sync_preview();
        clamp_page_scroll();
        place_children();
    } else if (id >= HIT_LROW && id <= HIT_LROW_END) {
        const size_t i = (size_t)(id - HIT_LROW);
        // a click plays the row's animation from its start, the chosen row again included (snatcherLeo, 2026-11-19: "they
        // simply click on thing, and that is what allows the animation to play")
        if (i < g.learn.size()) {
            if (g.learn[i].move != g.learn_move) {
                g.learn_move = g.learn[i].move;
                g.viewer.stop_once();
                sync_preview();
            }
            const int a = chosen_animation();
            if (a >= 0) g.viewer.play_once(a);
            playhead_timer();
        }
    } else if (id == HIT_TIMELINE) {
        scrub_to(g.mouse_x);  // (a press starts a drag: WM_LBUTTONDOWN; this is for clicks from elsewhere)
    } else if (id == HIT_PLAY) {
        // Play from the first frame; while it plays, Pause holds the frame; Play again carries on from it
        const int a = chosen_animation();
        if (a >= 0 && g.viewer.playing() == a) g.viewer.set_held(!g.viewer.held());
        else if (a >= 0) g.viewer.play_once(a);
        playhead_timer();
    } else if (id == HIT_STEP_BACK || id == HIT_STEP_FWD) {
        const int a = chosen_animation();
        if (a >= 0) g.viewer.step(a, id == HIT_STEP_FWD ? 1 : -1);
        playhead_timer();
    } else if (id == HIT_TURN1 || id == HIT_TURN2) {
        g.turn = id == HIT_TURN1 ? 1 : 2;
        sync_preview();  // (the turn's own files and animation)
        logf(Level::Info, "viewer", "moves preview: turn %d", g.turn);
    } else if (id == HIT_PARTS_MOVE || id == HIT_PARTS_FOLLOW || id == HIT_PARTS_BOTH) {
        g.parts = id == HIT_PARTS_FOLLOW ? 1 : id == HIT_PARTS_BOTH ? 2 : 0;
        sync_preview();  // (the parts' own files and animation)
        logf(Level::Info, "viewer", "moves preview: parts %d", g.parts);
    } else if (id == HIT_OV_STAGE) {
        // the Overview's own stage, from a native menu under the pointer, the current one checked (Windows' menus;
        // Linux / macOS: the toolkit's popup menu). sync_preview applies it; the Moves tab keeps its own
        HMENU m = CreatePopupMenu();
        const char* keys[] = {"studio.moves.view_studio", "studio.moves.view_plain", "studio.moves.view_arena"};
        for (int k = 0; k < 3; ++k) AppendMenuW(m, MF_STRING | (k == g.ov_stage ? MF_CHECKED : 0), 1 + k, tx(keys[k]).c_str());
        POINT p;
        GetCursorPos(&p);
        const int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, p.x, p.y, 0, g.hwnd, nullptr);
        DestroyMenu(m);
        if (pick > 0 && pick - 1 != g.ov_stage) {
            g.ov_stage = pick - 1;
            sync_preview();
            logf(Level::Info, "viewer", "Overview stage: %s", g.ov_stage == kStageArena ? "arena" : g.ov_stage == kStagePlain ? "plain" : "studio");
        }
    } else if (id == HIT_VIEW_STUDIO || id == HIT_VIEW_PLAIN || id == HIT_VIEW_ARENA) {
        // the shown tab's own stage: the Animations tab keeps one apart from the Moves tab's (an_stage)
        const int st = id == HIT_VIEW_ARENA ? 2 : id == HIT_VIEW_PLAIN ? 1 : 0;
        if (g.tab == kTabAnimations) {
            g.an_stage = st;
        } else {
            g.plain = st != 0;
            g.arena = st == 2;
        }
        sync_preview();  // (the stage, and the arena's room read the first time)
        logf(Level::Info, "viewer", "%s preview: %s", g.tab == kTabAnimations ? "animations" : "moves",
             st == 2 ? "arena" : st == 1 ? "plain stage" : "Studio view");
    } else if (id == HIT_MOVE_EDITOR) {
        click(HIT_RAIL + 1);  // the Move editor (its "coming soon" page until it exists)
        return;
    } else if (id == HIT_SPIN) {
        g.viewer.set_spin(!g.viewer.spin());
    } else if (id >= HIT_STYLE && id <= HIT_STYLE_END) {
        // a style picked (the Styles tab): viewed at once, its model loaded (show_form: from its region's disc)
        if (g.selected >= 0) {
            const int sp = g.all[(size_t)g.selected].index, s = id - HIT_STYLE;
            if (s < style_count(sp) && s != style_now(sp)) {
                g.style_of[sp] = s;
                show_form();
                logf(Level::Info, "styles", "species %d: style %d viewed", sp, s);
            }
        }
    } else if (id == HIT_CHANGE_STYLE) {
        click(HIT_TAB + kTabLooks);  // (the Overview's "Change style… →": the Styles tab)
        return;
    } else if (id == HIT_RESET) {
        g.viewer.reset_view();
    } else if (id == HIT_CHOOSE_DISC || id == HIT_HOME_ADD_DISC) {
        // the "choose a disc" page and Home's no-disc strip: the US row's Add, then Settings › Discs for its Copy / Move
        // question and the "US disc added." line (the disc goes into the ROM folder, the hub's way, with the hub's checks
        // and words: discs_check). *Changed 2026-11-24:* the file dialog comes first, so Cancel leaves you where you were
        const std::wstring p = pick_disc_file();
        if (p.empty()) return;
        g.settings_pane = 1;
        open_page(kSettings);
        discs_check(0, p);
    } else if (id >= HIT_DISC_ADD && id < HIT_DISC_ADD + kDiscRows) {
        discs_add(id - HIT_DISC_ADD);
    } else if (id >= HIT_DISC_REMOVE && id < HIT_DISC_REMOVE + kDiscRows) {
        g_discs.pick = -1;
        g_discs.confirm = id - HIT_DISC_REMOVE;
        g_discs.msg_row = -1;
    } else if (id == HIT_DISC_COPY || id == HIT_DISC_MOVE) {
        discs_start(id == HIT_DISC_MOVE);
    } else if (id == HIT_OPEN_ORREPORT) {  // (4.4: OrrePort forward when open, else started from this folder's launcher)
        if (g_hub_open) {
            const int ok = pc_apps_show(PC_APP_HUB, g_apps_root.c_str());
            logf(Level::Info, "apps", "OrrePort %s", ok ? "brought forward" : "is open but its window isn't published yet");
        } else if (!pc_apps_start(PC_APP_HUB, g_apps_root.c_str())) {
            logf(Level::Warn, "apps", "OrrePort.exe isn't in this OrrePort folder: not started");
            show_toast(tx("studio.home.orreport_missing"));
        } else {
            logf(Level::Info, "apps", "OrrePort started");
        }
    } else if (id == HIT_UPD_CLOSE_OTHER) {  // ("Close OrrePort": it closes its usual way; refused while its game runs)
        const int r = pc_apps_close(PC_APP_HUB, g_apps_root.c_str());
        logf(Level::Info, "apps", "Close OrrePort: %s", r == PC_APPS_CLOSE_ASKED ? "asked" : r == PC_APPS_CLOSE_GAME_RUNS ? "refused, its game is running" : "it isn't open");
        if (r == PC_APPS_CLOSE_GAME_RUNS) { g_game_running = true; repaint_all(); }
    } else if (id == HIT_UPD_DOWNLOAD || id == HIT_UPD_RETRY) {  // (updates, 4.4: the shared module does the work)
        pc_update_download();
    } else if (id == HIT_UPD_NOTES) {
        g_upd_notes = !g_upd_notes;
    } else if (id == HIT_UPD_RESTART || id == HIT_UPD_GOBACK_YES) {
        if (id == HIT_UPD_GOBACK_YES) {
            g.goback_open = false;
            place_children();
            if (!pc_update_request_rollback()) { logf(Level::Warn, "update", "Go back: the request couldn't be written"); return; }
            // (Studio not started from an OrrePort folder's app\, a development run: no launcher to restart it, so the
            // switch happens at the next start through OrrePort, and the note says so; found by the screenshot test)
            if (!updates_can_restart()) {
                logf(Level::Info, "update", "Go back: asked; applies at the next start");
                show_toast(tx("update.go_back_next", {{"version", goback_version()}}));
                return;
            }
        }
        updates_restart();
        return;  // (the windows are gone)
    } else if (id == HIT_UPD_GOBACK || id == HIT_UPD_GOBACK_NO) {  // (the pop-up opens or closes: draw_goback)
        // (only while Go back is offered: a test click on the hidden row once opened it with no version in it)
        g.goback_open = id == HIT_UPD_GOBACK && goback_offered();
        place_children();
        // (the top search box, shown again on closing, came back as a white bar until something repainted it; the
        // first screenshot test caught it: repaint it now. Windows-only native edit; Linux / macOS: drawn with the page)
        if (!g.goback_open && g.gsearch) RedrawWindow(g.gsearch, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
    } else if (id == HIT_UPD_CHECK) {
        pc_update_check(g.update_check ? 2 : 0);  // ("Check now": 2 forces the check past the three-hour skip, the project's a02eba09c)
    } else if (id == HIT_UPD_SW_AUTO || id == HIT_UPD_SW_CHECK) {
        bool& v = id == HIT_UPD_SW_AUTO ? g.update_auto : g.update_check;
        const char* key = id == HIT_UPD_SW_AUTO ? "update_auto_download" : "update_check";
        const bool was = v;
        v = !v;
        if (!shared_setting_set(key, v ? "1" : "0")) logf(Level::Warn, "settings", "%s couldn't be saved to OrrePort's settings.ini", key);
        logf(Level::Info, "settings", "%s: %s -> %s", key, was ? "on" : "off", v ? "on" : "off");  // (the settings line format)
        updates_settings_changed();
    } else if (id >= HIT_DISC_SHOW && id < HIT_DISC_SHOW + kDiscRows) {
        const std::wstring& p = g_discs.path[id - HIT_DISC_SHOW];
        if (!p.empty()) open_folder(disc_folder(id - HIT_DISC_SHOW), file_name(p));  // (File Explorer, the disc selected)
    } else if (id == HIT_DISC_CANCEL) {
        g_discs.pick = g_discs.confirm = -1;
    } else if (id == HIT_DISC_CONFIRM) {
        if (g_discs.confirm >= 0) discs_remove(g_discs.confirm);
    }
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

int hit_at(float x, float y) {
    for (auto it = g.hits.rbegin(); it != g.hits.rend(); ++it)
        if (it->first.in(x, y)) return it->second;
    return -1;
}

// Whether clicking this spot would change anything: the pointer turns into Windows' hand over these only (snatcherLeo,
// 2026-11-18: the hand "for whenever we are hovering over anything in this app ... But Pikachu itself, since I'm on
// there, wouldn't be clickable"). So what is already showing (the open editor, the selected row, the open tab, the
// stat or type the inspector already explains) keeps the arrow, the same rule as the current evolution member,
// which registers no hit spot at all.
bool does_something(int id) {
    if (id < 0) return false;
    if (id >= HIT_RAIL && id < HIT_RAIL + kEditorCount) return g.editor != id - HIT_RAIL || (id == HIT_RAIL && on_app_page());
    if (id >= HIT_ROW && id <= HIT_ROW_END) {
        const size_t k = (size_t)(id - HIT_ROW);
        return k >= g.shown.size() || g.shown[k] != g.selected || g.cur_tab < 0;
    }
    if (id >= HIT_TAB && id < HIT_TAB + kTabCount) return g.tab != id - HIT_TAB;
    if (id >= HIT_FORM && id <= HIT_FORM_END) return id - HIT_FORM != g.form;
    // (v3) hover-only spots keep the arrow: stats, training rows, the hidden ability, the greyed eras, Create a mod
    if ((id >= HIT_STAT && id < HIT_STAT + 6) || (id >= HIT_TRAIN && id < HIT_TRAIN + 8) || id == HIT_ABILITY_HIDDEN ||
        (id >= HIT_ERA && id < HIT_ERA + 20) || id == HIT_CREATE_MOD || (id >= HIT_LATER && id < HIT_LATER + kLaterCount))
        return false;
    // the Moves tab: the shown group and the chosen row keep the arrow
    if (id >= HIT_LGROUP && id < HIT_LGROUP + 4) return id - HIT_LGROUP != g.learn_group;
    if (id >= HIT_AROW && id <= HIT_AROW_END) return id - HIT_AROW != g.anim_sel;  // (the chosen animation keeps the arrow)
    if (id >= HIT_LROW && id <= HIT_LROW_END) {
        const size_t i = (size_t)(id - HIT_LROW);
        return i < g.learn.size() && g.learn[i].move != g.learn_move;
    }
    if (id == HIT_VIEW_STUDIO || id == HIT_VIEW_PLAIN || id == HIT_VIEW_ARENA)
        return (shown_stage() == 2 ? HIT_VIEW_ARENA : shown_stage() == 1 ? HIT_VIEW_PLAIN : HIT_VIEW_STUDIO) != id;
    if (id == HIT_TURN1 || id == HIT_TURN2) return (g.turn == 1 ? HIT_TURN1 : HIT_TURN2) != id;
    if (id == HIT_PARTS_MOVE || id == HIT_PARTS_FOLLOW || id == HIT_PARTS_BOTH)
        return (g.parts == 1 ? HIT_PARTS_FOLLOW : g.parts == 2 ? HIT_PARTS_BOTH : HIT_PARTS_MOVE) != id;
    if (id == HIT_HOME) return !on_home();
    if (id == HIT_PAL_BG) return false;
    // (the list search's clear button gets the hand like any button, and lights up too: snatcherLeo, 2026-11-22, first "the
    // mouse cursor doesn't change", then "I take it back ... keeping in style of how hoverable stuff works")
    if (id == HIT_BACK) return can_go(-1);
    if (id == HIT_FORWARD) return can_go(1);
    if (id >= HIT_PTAB && id < HIT_PTAB + 100) return id - HIT_PTAB != g.cur_tab;
    if (id == HIT_IDLE) return !shown_idle();
    if (id == HIT_STANDING) return shown_idle();
    if (id == HIT_NORMAL) return g.shiny;
    if (id == HIT_SHINY) return !g.shiny;
    return true;  // buttons, evolution members and methods, type chips, abilities, tabs' close, choose disc
}

void scroll_list(float dy) {
    const Layout L = layout();
    const float max = std::max(0.0f, g.shown.size() * kRow - (L.h - kListTop));
    g.scroll = std::clamp(g.scroll + dy, 0.0f, max);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

std::vector<int> sel_list();
void drag_end();
// ---- the last session (snatcherLeo, 2026-11-22: "the app should remember where you LAST were, BEFORE you closed out the
// window. This is standard in apps like these, where you are working on something. This would include then, any opened
// tabs you had as well, or what your current tab was") ----
// When the last window closes, studio-session.ini (beside studio.ini: R\data\settings\ portable, <exe>\user\ in development)
// gets that window's place and size (maximized or not), its page tabs with each one's Back / Forward history, the shown
// tab, the editor and page tab inside it (Overview, Moves...), the form, the page's scroll, the Moves tab's choices
// (learnset group, move, turn, parts, stage, Shiny) and the Settings pane. The next start opens it all again once the
// disc's list is read. Pokémon are kept by their National Dex number (not list positions), so a session survives
// a changed list; anything that no longer exists is skipped. Kept apart from studio.ini because it's "where you were",
// rewritten on every close, not a preference. Like a browser, closing windows one by one keeps the last one's session.
// Not restored: test runs (--screenshot, --select) and --no-session. Windows (window placement); Linux / macOS: the
// toolkit's window geometry calls, the same file.
std::wstring session_ini() { return g.settings_dir + L"\\studio-session.ini"; }
// a page (an index into all, kHome, kSettings) as the file keeps it: the National Dex number, or -1 Home, -2 Settings
int page_number(int page) {
    if (page == kHome) return -1;
    if (page == kSettings) return -2;
    if (page_editor(page)) return page;  // (an editor: -11 Move .. -17 Battle)
    return page >= 0 && page < (int)g.all.size() ? g.all[(size_t)page].index : -1;
}
// ...and back; INT_MIN for a Pokémon this disc's list doesn't have
int page_from_number(int n) {
    if (n == -1) return kHome;
    if (n == -2) return kSettings;
    if (page_editor(n)) return n;
    if (n < 0) return INT_MIN;
    const auto it = g.by_index.find(n);
    return it == g.by_index.end() ? INT_MIN : it->second;
}
// (to: another file to write the same text to: Report a problem's "what's open", which can be reopened as a session;
// written even in a test run, which never saves the real session)
void save_session(const std::wstring& to) {
    if (to.empty() && (!g.screenshot.empty() || g.select_given)) return;
    if (!g.disc || g.all.empty() || !g.hwnd) return;
    WINDOWPLACEMENT wp{sizeof wp};
    if (!GetWindowPlacement(g.hwnd, &wp)) return;
    CreateDirectoryW(g.settings_dir.c_str(), nullptr);
    FILE* f = _wfopen((to.empty() ? session_ini() : to).c_str(), L"w");
    if (!f) return;
    static const char* pages[] = {"overview", "moves", "looks", "sound", "animations", "status"};
    std::fprintf(f, "# OrrePort Studio's last session: where you were when the window closed (rewritten on every close;\n"
                    "# delete this file to start fresh). Pokémon by National Dex number; -1 Home, -2 Settings,\n"
                    "# -11 .. -17 the Move, Item, Trainer, People, Type, Ability and Battle editors\n");
    const RECT& r = wp.rcNormalPosition;
    std::fprintf(f, "window = %ld,%ld,%ld,%ld\n", r.left, r.top, r.right, r.bottom);
    std::fprintf(f, "maximized = %s\n", wp.showCmd == SW_SHOWMAXIMIZED || (wp.flags & WPF_RESTORETOMAXIMIZED) ? "yes" : "no");
    std::fprintf(f, "# each tab: its page | its Back / Forward history | where it is in that history\n");
    for (size_t t = 0; t < g.page_tabs.size(); ++t) {
        // at most the last kSavedHistory pages of each tab's history, the current one kept (*added 2026-11-22*: a tab
        // clicked through the list for an evening had 584, all written on every close)
        constexpr int kSavedHistory = 100;
        std::string hist;
        int at = t < g.tab_pos.size() ? g.tab_pos[t] : 0;
        if (t < g.tab_hist.size()) {
            const auto& h = g.tab_hist[t];
            const int from = std::min(std::max(0, (int)h.size() - kSavedHistory), std::max(0, at));
            for (int k = from; k < (int)h.size() && k < from + kSavedHistory; ++k)
                hist += (hist.empty() ? "" : ",") + std::to_string(page_number(h[(size_t)k]));
            at -= from;
        }
        std::fprintf(f, "tab%zu = %d|%s|%d\n", t, page_number(g.page_tabs[t]), hist.c_str(), at);
    }
    std::fprintf(f, "current_tab = %d\n", g.cur_tab);
    std::fprintf(f, "page = %s\n", g.tab >= 0 && g.tab < kTabCount ? pages[g.tab] : "overview");
    std::fprintf(f, "form = %d\n", g.form);
    std::fprintf(f, "page_scroll = %.0f\n", g.pscroll);
    std::fprintf(f, "learn_group = %d\nlearn_move = %d\nturn = %d\nparts = %d\n", g.learn_group, g.learn_move, g.turn, g.parts);
    std::fprintf(f, "anim_sel = %d\nanimations_stage = %d\n", g.anim_sel, g.an_stage);  // (the Animations tab's choices)
    std::fprintf(f, "stage = %s\n", g.arena ? "arena" : g.plain ? "plain" : "studio");
    std::fprintf(f, "overview_stage = %s\n", g.ov_stage == kStageArena ? "arena" : g.ov_stage == kStagePlain ? "plain" : "studio");
    std::fprintf(f, "shiny = %s\n", g.shiny ? "yes" : "no");
    std::fprintf(f, "settings_pane = %d\n", g.settings_pane);
    // the styles viewed (2026-11-22): species number:style, for the ones not on the game's own
    std::string styles;
    for (const auto& [sp, s] : g.style_of) if (s > 0) styles += (styles.empty() ? "" : ",") + std::to_string(sp) + ":" + std::to_string(s);
    if (!styles.empty()) std::fprintf(f, "styles = %s\n", styles.c_str());
    std::fprintf(f, "# each editor's last place (its rail icon goes back there): page, scroll\n");
    for (int ed = 0; ed < kEditorCount; ++ed) {
        const auto& p = g.places[ed];
        if (p.page == INT_MIN) continue;
        std::fprintf(f, "place_%s = %d,%.0f\n", kEditors[ed], page_number(p.page), p.scroll);
    }
    std::fclose(f);
    if (to.empty()) logf(Level::Info, "session", "saved: %zu tabs, the shown one %d", g.page_tabs.size(), g.cur_tab);
}

// ---- Report a problem: the zip (4.3 part 3) ----
// What goes in, all of it the user's own session and nothing from the disc: note.txt (their words), about.txt (the
// build, Windows, the screen, the windows and tabs, memory), this run's log and performance file, studio.ini, a
// screenshot of each window and "what's open" (the session file, which reopens the same pages), each when ticked.
// Every text file has the Windows user's folder and name replaced by <user> (OrrePort's rule, pc_report.cpp), so a
// report can be shared in a public channel. The zip is saved beside OrrePort's reports and also put on the clipboard as
// a file, so Ctrl+V in Discord attaches it (snatcherLeo, 2026-11-22: "they can quickly send it to me via Discord").
std::string read_all(const std::wstring& path) {
    std::string s;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return s;
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    std::fclose(f);
    return s;
}
// the user's folder and name out of a text: OrrePort's shared rule (port/src/pc_scrub.c, 2026-11-22: the home
// folder in both slash styles and the user name become <user>), so both programs' reports are cleaned alike
void scrub_user(std::string& s) {
    size_t n = 0;
    if (char* out = pc_scrub(s.data(), s.size(), &n)) {
        s.assign(out, n);
        pc_scrub_free(out);
    }
}
// A report's screenshot made small: the capture (a full-size PNG) scaled so its longer side is at most 1920 pixels and
// saved as a JPEG (quality 85), a few hundred KB instead of ~30 MB for a 4K window, so the zip stays under Discord's
// 10 MB limit (the first test report was 29.6 MB, 2026-11-22). Windows Imaging Component; Linux / macOS: the core's
// PNG writer with the picture scaled down (likely; no JPEG there yet). Empty when the picture couldn't be read.
std::string shrink_to_jpeg(const std::wstring& png) {
    std::string out;
    IWICImagingFactory* wic = nullptr;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* fe = nullptr;
    IPropertyBag2* props = nullptr;
    IStream* st = SHCreateMemStream(nullptr, 0);
    UINT w = 0, h = 0;
    if (st && SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
        SUCCEEDED(wic->CreateDecoderFromFilename(png.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(frame->GetSize(&w, &h)) && w && h) {
        const double k = std::min(1.0, 1920.0 / (double)std::max(w, h));
        const UINT sw = std::max(1u, (UINT)std::lround(w * k)), sh = std::max(1u, (UINT)std::lround(h * k));
        if (SUCCEEDED(wic->CreateBitmapScaler(&scaler)) &&
            SUCCEEDED(scaler->Initialize(frame, sw, sh, WICBitmapInterpolationModeFant)) &&  // (Fant: a smooth reduction)
            SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &enc)) &&
            SUCCEEDED(enc->Initialize(st, WICBitmapEncoderNoCache)) && SUCCEEDED(enc->CreateNewFrame(&fe, &props))) {
            PROPBAG2 opt{};
            opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT q;
            VariantInit(&q);
            q.vt = VT_R4;
            q.fltVal = 0.85f;
            props->Write(1, &opt, &q);
            WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
            if (SUCCEEDED(fe->Initialize(props)) && SUCCEEDED(fe->SetSize(sw, sh)) && SUCCEEDED(fe->SetPixelFormat(&fmt))) {
                IWICFormatConverter* conv = nullptr;
                if (SUCCEEDED(wic->CreateFormatConverter(&conv)) &&
                    SUCCEEDED(conv->Initialize(scaler, fmt, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) &&
                    SUCCEEDED(fe->WriteSource(conv, nullptr)) && SUCCEEDED(fe->Commit()) && SUCCEEDED(enc->Commit())) {
                    STATSTG s{};
                    if (SUCCEEDED(st->Stat(&s, STATFLAG_NONAME))) {
                        out.resize((size_t)s.cbSize.QuadPart);
                        LARGE_INTEGER zero{};
                        ULONG got = 0;
                        st->Seek(zero, STREAM_SEEK_SET, nullptr);
                        st->Read(out.data(), (ULONG)out.size(), &got);
                        out.resize(got);
                    }
                }
                if (conv) conv->Release();
            }
        }
    }
    for (IUnknown* u : std::initializer_list<IUnknown*>{props, fe, enc, scaler, frame, dec, wic, st})
        if (u) u->Release();
    return out;
}
std::string about_text() {
    std::ostringstream o;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char when[64];
    std::snprintf(when, sizeof when, "%04d-%02d-%02d %02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
    o << "OrrePort Studio report\n" << diag_build_stamp() << "\nsaved: " << when << "\n";
    char buf[4096];
    diag_sysinfo(buf, sizeof buf);  // (the processor, memory, Windows, every graphics adapter and driver)
    o << buf;
    diag_displays(buf, sizeof buf);  // (every screen)
    o << buf;
    for (const std::string& l : g.disc_lines) o << l << "\n";  // (which discs: game id, release, main.dol, OrrePort's check)
    o << "layout: " << (g.root_dir.empty() ? "development" : "portable") << "\nlanguage: " << g.lang_code
      << "\ntheme: " << (g.pal.dark ? "dark" : "light") << "\nmonitors: " << GetSystemMetrics(SM_CMONITORS)
      << "\nscale: " << (int)std::lround(g.scale * 100) << "%\nwindows open: " << g_windows.size() << "\n";
    for (const std::wstring& l : watch_lines()) o << "memory: " << narrow(l) << "\n";
    return o.str();
}
void save_report() {
    struct File { std::string name, data; };
    std::vector<File> files;
    std::wstring note_w;
    if (g.report_note) {
        const int n = GetWindowTextLengthW(g.report_note);
        note_w.resize((size_t)n + 1);
        GetWindowTextW(g.report_note, note_w.data(), n + 1);
        note_w.resize((size_t)n);
    }
    files.push_back({"note.txt", narrow(note_w) + "\n"});
    files.push_back({"about.txt", about_text()});
    log_flush();
    logf(Level::Info, "report", "saving: screenshots %s, what's open %s", g.report_tick[0] ? "yes" : "no", g.report_tick[1] ? "yes" : "no");
    log_flush();
    const std::wstring lp = log_file_path(), pp = perf_file_path();
    auto base = [](const std::wstring& p) { return narrow(p.substr(p.find_last_of(L"\\/") + 1)); };
    if (!lp.empty()) files.push_back({"log/" + base(lp), read_all(lp)});
    if (!pp.empty()) files.push_back({"log/" + base(pp), read_all(pp)});
    // made from the bar after an unexpected close: the run that ended badly is the one that matters most
    const std::wstring prev = previous_run_unclean();
    // (in log/previous/, as OrrePort's report places it, 8ef34d4b8; never the run's .dmp: the project plan 5.1)
    if (g.report_prev && !prev.empty()) {
        files.push_back({"log/previous/" + base(prev), read_all(prev)});
        const std::wstring prev_perf = prev.substr(0, prev.size() - 4) + L"_perf.csv";
        if (GetFileAttributesW(prev_perf.c_str()) != INVALID_FILE_ATTRIBUTES) files.push_back({"log/previous/" + base(prev_perf), read_all(prev_perf)});
    }
    if (GetFileAttributesW(studio_ini().c_str()) != INVALID_FILE_ATTRIBUTES) files.push_back({"settings/studio.ini", read_all(studio_ini())});
    const std::wstring tmp = g.logs_dir + L"\\report-part.tmp";
    if (g.report_tick[1]) {
        save_session(tmp);
        files.push_back({"open/studio-session.ini", read_all(tmp)});
        DeleteFileW(tmp.c_str());
    }
    for (File& f : files)
        if (f.name.size() > 4 && f.name.compare(f.name.size() - 4, 4, ".png") != 0) scrub_user(f.data);
    if (g.report_tick[0]) {
        // each window as it looks, this one without the panel over it (hidden for the moment of the picture)
        const HWND here = g.hwnd;
        const std::vector<HWND> wins = [] { std::vector<HWND> v; for (const auto& w : g_windows) v.push_back(w.hwnd); return v; }();
        int k = 0;
        for (HWND w : wins) {
            activate(w);
            const bool panel = g.report_open;
            if (panel) { g.report_open = false; if (g.report_note) ShowWindow(g.report_note, SW_HIDE); }
            InvalidateRect(w, nullptr, FALSE);
            UpdateWindow(w);
            const std::wstring shot = g.logs_dir + L"\\report-shot.tmp.png";
            capture(shot);
            std::string jpg = shrink_to_jpeg(shot);
            ++k;
            if (!jpg.empty()) files.push_back({"screenshots/window-" + std::to_string(k) + ".jpg", std::move(jpg)});
            DeleteFileW(shot.c_str());
            if (panel) g.report_open = true;
        }
        activate(here);
        place_children();
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf(name, 64, L"Studio-%04d%02d%02d-%02d%02d%02d.zip", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    const std::wstring dir = reports_dir();
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    std::vector<PcZipFile> zf;
    for (const File& f : files) zf.push_back({f.name.c_str(), f.data.data(), f.data.size()});
    const std::wstring path = dir + L"\\" + name;
    errno = 0;
    SetLastError(0);
    if (!pc_zip_write(narrow(path).c_str(), zf.data(), (int)zf.size())) {
        const int e = errno;
        const DWORD last = GetLastError();
        g.report_error = tx("studio.report.failed");
        // (the C runtime's and Windows' own reasons: a full drive, a read-only folder, a name in use)
        logf(Level::Error, "report", "the zip %s couldn't be written (%zu files, into the reports folder): %s; Windows: %s", narrow(name).c_str(),
             files.size(), e ? std::strerror(e) : "no C runtime error", hr_text(HRESULT_FROM_WIN32(last)).c_str());
        InvalidateRect(g.hwnd, nullptr, FALSE);
        return;
    }
    // onto the clipboard as a file (OrrePort's shared pc_clip_file.c); never in a test run, which would wipe the PC
    // user's clipboard (the project's rule for that file)
    const bool test_run = g.select_given || !g.screenshot.empty();
    const bool copied = !test_run && pc_clipboard_set_file(narrow(path).c_str()) == 1;
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    const double mb = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa)
                          ? (((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) / 1048576.0 : 0.0;
    logf(Level::Info, "report", "saved %s: %zu files, %.1f MB, %s", narrow(name).c_str(), files.size(), mb,
         copied ? "copied to the clipboard" : test_run ? "not copied (a test run)" : "not copied (the clipboard refused it)");
    trail("report", "saved");
    g.report_prev = false;  // (the crashed run's log went with this report; a later F8 report doesn't repeat it, as OrrePort's)
    watch_forget_io();  // (the screenshots' temporary files aren't Studio misbehaving: left out of the disk check)
    g.report_copied = copied;
    g.report_mb = mb;
    g.report_file = name;
    g.report_stage = 1;
    if (g.report_note) ShowWindow(g.report_note, SW_HIDE);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
// The window's place and size from the session (run(), before it shows): only when that rectangle is still on a
// screen (a monitor unplugged since would leave the window out of sight); false = the default place
bool session_window(HWND hwnd, bool& maximized) {
    const auto& s = g.session;
    if (s.empty() || !s.count("window") || !s.count("tab0")) return false;
    RECT r{};
    if (std::sscanf(s.at("window").c_str(), "%ld,%ld,%ld,%ld", &r.left, &r.top, &r.right, &r.bottom) != 4) return false;
    if (r.right - r.left < 200 || r.bottom - r.top < 150 || !MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) return false;
    WINDOWPLACEMENT wp{sizeof wp};
    GetWindowPlacement(hwnd, &wp);
    wp.rcNormalPosition = r;
    wp.showCmd = SW_HIDE;  // (placed now, shown by run())
    SetWindowPlacement(hwnd, &wp);
    maximized = s.count("maximized") && s.at("maximized") == "yes";
    return true;
}
// The tabs and the rest, once the disc's list is read (WM_LIST_LOADED); false = nothing to restore (the usual first page)
bool restore_session() {
    auto s = std::move(g.session);  // (read once)
    g.session.clear();
    if (s.empty()) return false;
    auto num = [&](const char* k, int def) { return s.count(k) ? std::atoi(s[k].c_str()) : def; };
    std::vector<int> tabs, pos;
    std::vector<std::vector<int>> hist;
    for (int t = 0; t < 200; ++t) {
        const std::string k = "tab" + std::to_string(t);
        if (!s.count(k)) break;
        const std::string v = s[k];
        const size_t a = v.find('|'), b = a == std::string::npos ? a : v.find('|', a + 1);
        const int page = page_from_number(std::atoi(v.c_str()));
        if (page == INT_MIN) continue;  // (a Pokémon this list doesn't have)
        std::vector<int> h;
        int at = 0;
        if (a != std::string::npos && b != std::string::npos) {
            std::stringstream hs(v.substr(a + 1, b - a - 1));
            std::string item;
            while (std::getline(hs, item, ',')) {
                const int p = page_from_number(std::atoi(item.c_str()));
                if (p != INT_MIN) h.push_back(p);
            }
            at = std::atoi(v.c_str() + b + 1);
        }
        if (h.empty() || at < 0 || at >= (int)h.size() || h[(size_t)at] != page) { h = {page}; at = 0; }
        tabs.push_back(page);
        hist.push_back(std::move(h));
        pos.push_back(at);
    }
    if (tabs.empty()) return false;
    // the choices first, so the page opens with them (the learnset keeps learn_move when this Pokémon learns it)
    static const char* pages[] = {"overview", "moves", "looks", "sound", "animations", "status"};
    const std::string page = s.count("page") ? s["page"] : "";
    for (int i = 0; i < kTabCount; ++i) if (page == pages[i]) g.tab = i;
    // (0-2: Reactions, 3, moved to the Animations tab on 2026-11-22, so an older session's 3 opens Level-up)
    g.learn_group = std::clamp(num("learn_group", 0), 0, 2);
    g.anim_sel = num("anim_sel", -1);
    g.anim_for = -2;  // (kept for the Pokémon the session comes back on)
    g.an_stage = std::clamp(num("animations_stage", 0), 0, 2);
    g.learn_move = num("learn_move", 0);
    g.turn = num("turn", 2) == 1 ? 1 : 2;
    g.parts = std::clamp(num("parts", 0), 0, 2);
    const std::string stage = s.count("stage") ? s["stage"] : "studio";
    g.arena = stage == "arena";
    g.plain = g.arena || stage == "plain";
    // the Overview's own stage (2026-11-22; an older session has none: Studio view)
    const std::string ov = s.count("overview_stage") ? s["overview_stage"] : "studio";
    g.ov_stage = ov == "arena" ? kStageArena : ov == "plain" ? kStagePlain : kStageStudio;
    g.shiny = s.count("shiny") && s["shiny"] == "yes";
    g.settings_pane = std::max(0, num("settings_pane", 0));
    // the styles viewed ("styles = 124:1"), before the page opens so its model loads in that style (style_now checks
    // that the style still exists: without the Japanese disc, the game's own)
    if (s.count("styles")) {
        std::stringstream ss(s["styles"]);
        std::string item;
        while (std::getline(ss, item, ',')) {
            int sp = 0, st = 0;
            if (std::sscanf(item.c_str(), "%d:%d", &sp, &st) == 2 && st > 0) g.style_of[sp] = st;
        }
    }
    g.page_tabs = tabs;
    g.tab_hist = hist;
    g.tab_pos = pos;
    g.tab_sel.assign(tabs.size(), 0);
    g.cur_tab = std::clamp(num("current_tab", 0), 0, (int)tabs.size() - 1);
    g.navigating = true;  // (showing it isn't a new visit)
    open_page(g.page_tabs[(size_t)g.cur_tab]);
    g.navigating = false;
    const int form = num("form", 0);
    if (form > 0 && form < (int)g.forms.size() && g.forms[(size_t)form].available) g.form_at_start = form;
    g.pscroll = (float)std::max(0, num("page_scroll", 0));
    // each editor's last place (open_editor): what no longer exists, or belongs to another editor, is skipped
    for (int ed = 0; ed < kEditorCount; ++ed) {
        const std::string k = std::string("place_") + kEditors[ed];
        if (!s.count(k)) continue;
        int n = 0;
        float sc = 0;
        if (std::sscanf(s[k].c_str(), "%d,%f", &n, &sc) < 1) continue;
        const int page = page_from_number(n);
        if (page == INT_MIN || page_owner(page) != ed) continue;
        g.places[ed] = {page, std::max(0.0f, sc)};
        if (ed == 0) g.last_mon = page;
    }
    logf(Level::Info, "session", "restored: %zu tabs, the shown one %d", tabs.size(), g.cur_tab);
    return true;
}

bool key(WPARAM vk) {
    const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
    // the trail: shortcuts and navigation keys only (letters typed into the search boxes go to those boxes, not here).
    // A modifier on its own isn't logged: Ctrl held for Ctrl+clicks repeats about 30 times a second and filled the log
    // with "Ctrl+Ctrl" lines (snatcherLeo's stress test, 2026-11-22: 58 of them in two minutes). Windows key codes; Linux /
    // macOS: their modifier keys likewise
    const bool modifier = vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN ||
                          (vk >= VK_LSHIFT && vk <= VK_RMENU);
    if (!modifier && (ctrl || (vk >= VK_F1 && vk <= VK_F12) || vk == VK_ESCAPE || vk == VK_SPACE || vk == VK_RETURN ||
                      vk == VK_TAB || (vk >= VK_PRIOR && vk <= VK_DOWN) || vk == VK_DELETE)) {
        wchar_t kn[32] = L"";
        GetKeyNameTextW((LONG)(MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC) << 16), kn, 32);  // (Windows' key names)
        trail("key", std::string(ctrl ? "Ctrl+" : "") + (shift ? "Shift+" : "") + (kn[0] ? narrow(kn) : std::to_string(vk)));
    }
    // Report a problem: F8 anywhere opens it (OrrePort's key; snatcherLeo approved the flow 2026-11-22); while it's open,
    // Escape closes it and no other shortcut acts behind it. Windows' F8; Linux / macOS: the same key (Fn+F8 on Mac
    // laptops, likely)
    if (vk == VK_F8) { open_report(); return true; }
    if (g.report_open) {
        if (vk == VK_ESCAPE) close_report();
        return true;
    }
    // the "Go back" pop-up likewise: Escape is Cancel, and no other shortcut acts behind it
    if (g.goback_open) {
        if (vk == VK_ESCAPE) click(HIT_UPD_GOBACK_NO);
        return true;
    }
    if (ctrl && shift && vk == 'D') { g.dev_view = !g.dev_view; InvalidateRect(g.hwnd, nullptr, FALSE); return true; }
    if (ctrl && vk == 'F') { SetFocus(g.search); SendMessageW(g.search, EM_SETSEL, 0, -1); return true; }
    // Ctrl+Shift+N: a new, empty window (multi-window plan step 2; macOS: Cmd+Shift+N)
    if (ctrl && shift && vk == 'N') { open_window({}); return true; }
    // Ctrl+comma: Settings, as in Chrome, Edge and VS Code (macOS: Cmd+comma, the system's own convention): the
    // window's Settings tab if it has one, else a new one. VK_OEM_COMMA is the comma key on every Windows layout.
    if (ctrl && vk == VK_OEM_COMMA) {
        for (size_t t = 0; t < g.page_tabs.size(); ++t)
            if (g.page_tabs[t] == kSettings) { click(HIT_PTAB + (int)t); return true; }
        open_page(kSettings, true);
        return true;
    }
    // Ctrl+K: the global search's palette (snatcherLeo, 2026-11-18; macOS: Cmd+K)
    if (ctrl && vk == 'K' && g.disc) { open_palette(); return true; }
    if (vk == VK_ESCAPE && (g.palette_open || g.drop_open)) { close_search(); return true; }
    // Escape during a tab drag cancels it (the tabs stay where they were)
    if (vk == VK_ESCAPE && g_drag.pressed) {
        const HWND src = g_drag.src;
        drag_end();
        ReleaseCapture();
        InvalidateRect(src, nullptr, FALSE);
        return true;
    }
    // a browser's tab keys (snatcherLeo, 2026-11-18: "Basic tab navigation that you'd find on Chrome"): Ctrl+Tab / Ctrl+Shift+Tab
    // the next / previous tab (round the ends), Ctrl+1..8 that tab, Ctrl+9 the last, Ctrl+W close the shown tab.
    // Windows key codes; Linux / macOS: the same keys (Cmd instead of Ctrl on macOS, by convention)
    {
        const int n = (int)g.page_tabs.size();
        if (ctrl && vk == VK_TAB && n) {
            const int t = g.cur_tab < 0 ? (shift ? n - 1 : 0) : ((g.cur_tab + (shift ? -1 : 1)) % n + n) % n;
            click(HIT_PTAB + t);
            return true;
        }
        // Ctrl+T: a new tab, as "+" makes one (a Home tab at the end, shown). snatcherLeo, 2026-11-22: "can "CTRL+T" open a new
        // tab?" (Chrome's key; Ctrl+Shift+T, reopening a closed tab, is not taken: left free for that later)
        if (ctrl && !shift && vk == 'T') {
            click(HIT_PTAB_ADD);
            return true;
        }
        if (ctrl && vk == 'W') {  // the shown tab, or every selected tab
            const std::vector<int> s = sel_list();
            for (size_t k = s.size(); k-- > 0;) click(HIT_PTAB_CLOSE + s[k]);
            return true;
        }
        if (ctrl && vk >= '1' && vk <= '9') {
            const int t = vk == '9' ? n - 1 : (int)(vk - '1');
            if (t >= 0 && t < n) click(HIT_PTAB + t);
            return true;
        }
    }
    if (g.editor != 0 || !g.disc || on_app_page()) return false;  // (Home and Settings have no list to step through)
    // the Moves tab's preview, as video players do it: Space plays / pauses, comma and period step one frame back /
    // forward (VK_OEM_COMMA / VK_OEM_PERIOD are those keys on every Windows layout; Linux / macOS: the same keys)
    if (preview_tab(g.tab) && g.selected >= 0 && !ctrl) {
        if (vk == VK_SPACE) { click(HIT_PLAY); return true; }
        if (vk == VK_OEM_COMMA || vk == VK_OEM_PERIOD) { click(vk == VK_OEM_COMMA ? HIT_STEP_BACK : HIT_STEP_FWD); return true; }
    }
    const int pos = shown_pos();
    const int page = std::max(1, (int)((layout().h - kListTop) / kRow) - 1);
    switch (vk) {
        case VK_DOWN: select_shown(pos + 1); return true;
        case VK_UP: select_shown(pos < 0 ? 0 : pos - 1); return true;
        case VK_NEXT: select_shown(pos + page); return true;
        case VK_PRIOR: select_shown(pos - page); return true;
        case VK_HOME: if (GetFocus() != g.search) { select_shown(0); return true; } break;
        case VK_END: if (GetFocus() != g.search) { select_shown((int)g.shown.size() - 1); return true; } break;
        case VK_SPACE: if (GetFocus() != g.search) { g.viewer.set_spin(!g.viewer.spin()); InvalidateRect(g.hwnd, nullptr, FALSE); return true; } break;
    }
    return false;
}

// the search box forwards the list keys (up / down step through the list while typing) and Escape clears it
LRESULT CALLBACK search_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    activate(GetParent(h));  // its window's state (the box's messages don't pass through the window procedure)
    if (m == WM_KEYDOWN) {
        if (w == VK_ESCAPE) { SetWindowTextW(h, L""); return 0; }
        const bool tabkey = w == VK_F8 ||  // (Report a problem while typing too)
                            (GetKeyState(VK_CONTROL) < 0 && (w == VK_TAB || w == 'W' || w == 'T' || w == 'K' || w == VK_OEM_COMMA ||
                                                             (w >= '1' && w <= '9')));  // the tab keys, Ctrl+K and Ctrl+comma work while typing too
        if (tabkey || w == VK_UP || w == VK_DOWN || w == VK_NEXT || w == VK_PRIOR ||
            (w == 'D' && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0))
            if (key(w)) return 0;
        if (w == VK_RETURN) { SetFocus(g.hwnd); return 0; }
    }
    if (m == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE)) return 0;
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS) InvalidateRect(g.hwnd, nullptr, FALSE);
    return DefSubclassProc(h, m, w, l);
}

// ---- several tabs: selecting, taking out, putting in (multi-window plan steps 3-5) ----
// the selected tabs in order: every flagged one, and the shown one
std::vector<int> sel_list() {
    std::vector<int> s;
    for (int k = 0; k < (int)g.page_tabs.size(); ++k)
        if (k == g.cur_tab || (k < (int)g.tab_sel.size() && g.tab_sel[(size_t)k])) s.push_back(k);
    return s;
}
// one tab with its history, as it travels between places or windows
struct TabRec { int idx; std::vector<int> hist; int pos; };
// takes tabs ts (ascending) out of this window, keeping a sensible tab shown; the window may end up with none
// (show = false: the tabs go straight back into this window, so nothing in between is shown or loaded: a reorder)
std::vector<TabRec> take_tabs(const std::vector<int>& ts, bool show = true) {
    std::vector<TabRec> out;
    for (int t : ts) out.push_back({g.page_tabs[(size_t)t], g.tab_hist[(size_t)t], g.tab_pos[(size_t)t]});
    const int shown = g.cur_tab, first = ts.empty() ? 0 : ts.front();
    bool shown_taken = false;
    int before = 0;  // tabs taken from before the shown one
    for (int t : ts) { if (t == shown) shown_taken = true; else if (t < shown) ++before; }
    for (size_t k = ts.size(); k-- > 0;) {
        const size_t t = (size_t)ts[k];
        g.page_tabs.erase(g.page_tabs.begin() + (long)t);
        g.tab_hist.erase(g.tab_hist.begin() + (long)t);
        g.tab_pos.erase(g.tab_pos.begin() + (long)t);
        if (t < g.tab_sel.size()) g.tab_sel.erase(g.tab_sel.begin() + (long)t);
    }
    g.tab_sel.assign(g.page_tabs.size(), 0);
    g.tab_anchor = -1;
    if (g.page_tabs.empty()) {
        g.cur_tab = -1;
    } else {
        g.cur_tab = shown_taken ? std::min(first, (int)g.page_tabs.size() - 1) : shown - before;
        const int idx = g.page_tabs[(size_t)g.cur_tab];
        if (show && idx != g.selected) { g.selected = -1; g.navigating = true; open_page(idx); g.navigating = false; }
    }
    return out;
}
// puts tabs into this window at position `at`, the first shown and all of them selected
void put_tabs(const std::vector<TabRec>& recs, int at) {
    at = std::clamp(at, 0, (int)g.page_tabs.size());
    for (size_t k = 0; k < recs.size(); ++k) {
        const long p = at + (long)k;
        g.page_tabs.insert(g.page_tabs.begin() + p, recs[k].idx);
        g.tab_hist.insert(g.tab_hist.begin() + p, recs[k].hist);
        g.tab_pos.insert(g.tab_pos.begin() + p, recs[k].pos);
    }
    g.tab_sel.assign(g.page_tabs.size(), 0);
    if (recs.size() > 1)
        for (size_t k = 0; k < recs.size(); ++k) g.tab_sel[(size_t)at + k] = 1;
    g.cur_tab = at;
    g.tab_anchor = at;
    g.editor = 0;
    // (the Pokémon already shown is kept as it is: forcing a reload here flashed the 3D view black after a reorder,
    // snatcherLeo, 2026-11-19, "there can be a black flicker that happens on the model")
    g.navigating = true;
    open_page(g.page_tabs[(size_t)at]);
    g.navigating = false;
    place_children();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
// a press on tab t (multi-window plan step 3): Ctrl toggles it in the selection, Shift selects the range from the
// anchor, a plain press shows it (and clears the selection, unless it presses an already selected tab: then the
// selection stays for a drag and clears on release without one, as in Chrome)
void tab_press(int t, bool ctrl, bool shift) {
    const int n = (int)g.page_tabs.size();
    if (t < 0 || t >= n) return;
    g.tab_sel.resize((size_t)n, 0);
    auto show = [&](int k) {
        g.cur_tab = k;
        g.editor = 0;
        const int idx = g.page_tabs[(size_t)k];
        if (idx != g.selected) { g.selected = -1; g.navigating = true; open_page(idx); g.navigating = false; }
        place_children();
    };
    if (shift && g.tab_anchor >= 0 && g.tab_anchor < n) {
        std::fill(g.tab_sel.begin(), g.tab_sel.end(), 0);
        for (int k = std::min(g.tab_anchor, t); k <= std::max(g.tab_anchor, t); ++k) g.tab_sel[(size_t)k] = 1;
        show(t);
    } else if (ctrl) {
        const std::vector<int> s = sel_list();
        const bool in = std::find(s.begin(), s.end(), t) != s.end();
        if (in && s.size() > 1) {
            // out of the selection; if it was the shown one, the nearest other selected tab is shown
            g.tab_sel[(size_t)t] = 0;
            if (t == g.cur_tab) {
                for (int k : s) if (k != t) { show(k); break; }
            }
        } else if (!in) {
            if (g.cur_tab >= 0) g.tab_sel[(size_t)g.cur_tab] = 1;
            g.tab_sel[(size_t)t] = 1;
            show(t);
        }
        g.tab_anchor = t;
    } else {
        const std::vector<int> s = sel_list();
        g_drag.clear_on_up = s.size() > 1 && std::find(s.begin(), s.end(), t) != s.end();
        if (!g_drag.clear_on_up) std::fill(g.tab_sel.begin(), g.tab_sel.end(), 0);
        g.tab_anchor = t;
        show(t);
    }
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// The torn-off tabs under the pointer: the first tab drawn as the strip draws a tab (its rounded shape, mug shot or
// symbol, name in the tab's font), lifted on a soft shadow, with a pill counting the tabs when several move together.
// *Updated 2026-11-22 (snatcherLeo, 4.3 part 2, with screenshots of the chip next to a real tab: "When I drag the tab, I want
// it to look... idk, better?"):* it was drawn with GDI (square corners showing round the rounded box, no icon, another
// font); now Direct2D draws it with the strip's own helpers into an offscreen bitmap on the window's device context,
// which is read back and given to a layered window with per-pixel alpha (smooth corners, see-through shadow). Drawn
// once per drag (its content doesn't change), then only moved. Windows-only (UpdateLayeredWindow); Linux / macOS: the
// toolkit's drag image, drawn by the same code.
constexpr float kChipMargin = 10;  // DIPs round the chip for its shadow
// a normal tab's width (the strip's kTabW), never the squeezed width of a crowded strip (snatcherLeo's stress test,
// 2026-11-22: the dragged tab read "D... +5")
float chip_w() { return std::max(180.0f, g.strip_step - 2); }
constexpr float kDragChipH = kTabBar - 8;  // (the Used by card has its own kChipH)
void render_chip() {
    if (!g.rt || g_drag.tabs.empty()) return;
    const float s = g.scale, m = kChipMargin, W = chip_w(), H = kDragChipH;
    const UINT pw = (UINT)std::ceil((W + 2 * m) * s), ph = (UINT)std::ceil((H + 2 * m) * s);
    const D2D1_PIXEL_FORMAT pf = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    ID2D1Bitmap1 *target = nullptr, *cpu = nullptr;
    if (FAILED(g.rt->CreateBitmap(D2D1::SizeU(pw, ph), nullptr, 0,
                                  D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, pf, 96 * s, 96 * s), &target)))
        return;
    ID2D1Image* old = nullptr;
    g.rt->GetTarget(&old);
    D2D1_MATRIX_3X2_F old_tf;
    g.rt->GetTransform(&old_tf);
    g.rt->SetTarget(target);
    g.rt->BeginDraw();
    g.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    g.rt->Clear(D2D1::ColorF(0, 0, 0, 0));
    const Palette& P = g.pal;
    for (int k = 6; k >= 1; --k)  // the shadow: a few soft rings, darker toward the chip, a little lower than it
        round_fill(m - k, m - k + 3, W + 2 * k, H + 2 * k, 8 + k, D2D1::ColorF(0, 0, 0, 0.045f));
    round_fill(m, m, W, H, 8, P.bg);
    round_line(m, m, W, H, 8, P.accent_line);
    // the tab that was grabbed, not the selection's first (snatcherLeo, 2026-11-22: "I did click drag from the Moves tab, so
    // technically the tab that is shown should be the "Moves" with the +1")
    const int grabbed = g_drag.press_tab >= 0 && g_drag.press_tab < (int)g.page_tabs.size() ? g_drag.press_tab : g_drag.tabs[0];
    const int page = g.page_tabs[(size_t)grabbed];
    float cx = m + 12;
    if (page >= 0) { draw_mug(tab_bitmap(page), cx, m + (H - 20) / 2, 20); cx += 28; }
    else {
        const float iy = m + (H - 20) / 2;
        if (page == kSettings) draw_gear(cx, iy, 20, gear_colour());
        else if (page_editor(page)) draw_symbol(page_editor(page), cx, iy, 20, P.text);
        else draw_o(cx, iy, 20);
        cx += 28;
    }
    float right = m + W - 12;
    if (g_drag.tabs.size() > 1) {  // how many tabs travel together, all of them counted (snatcherLeo, 2026-11-22: "get rid
        // of the "+" symbol and just have the number of things I am dragging"; it read "+1" for two)
        const std::wstring more = std::to_wstring(g_drag.tabs.size());
        const float mw = text_width(more, 12, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 14;
        round_fill(right - mw, m + (H - 20) / 2, mw, 20, 10, P.accent_bg);
        text(more, right - mw, m + (H - 20) / 2, mw, 20, 12, P.accent, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        right -= mw + 8;
    }
    text(tab_label(page), cx, m, std::max(4.0f, right - cx), H, 13, P.text, DWRITE_FONT_WEIGHT_MEDIUM);
    g.rt->EndDraw();
    g.rt->SetTransform(old_tf);
    g.rt->SetTarget(old);
    if (old) old->Release();
    // read the pixels back (a CPU-readable copy) into a DIB for UpdateLayeredWindow (both premultiplied BGRA)
    if (SUCCEEDED(g.rt->CreateBitmap(D2D1::SizeU(pw, ph), nullptr, 0,
                                     D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, pf, 96 * s, 96 * s),
                                     &cpu)) &&
        SUCCEEDED(cpu->CopyFromBitmap(nullptr, target, nullptr))) {
        D2D1_MAPPED_RECT mr{};
        if (SUCCEEDED(cpu->Map(D2D1_MAP_OPTIONS_READ, &mr))) {
            if (!g_drag.preview) {
                static bool reg = false;
                if (!reg) {
                    WNDCLASSW wc{};
                    wc.lpfnWndProc = DefWindowProcW;
                    wc.hInstance = g.inst;
                    wc.lpszClassName = L"OrrePortStudioDragChip";
                    RegisterClassW(&wc);
                    reg = true;
                }
                g_drag.preview = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                                                 L"OrrePortStudioDragChip", L"", WS_POPUP, 0, 0, (int)pw, (int)ph, nullptr, nullptr, g.inst, nullptr);
            }
            HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
            BITMAPINFO bi{};
            bi.bmiHeader = {sizeof(BITMAPINFOHEADER), (LONG)pw, -(LONG)ph, 1, 32, BI_RGB};
            void* bits = nullptr;
            HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (bmp && bits) {
                for (UINT y = 0; y < ph; ++y) std::memcpy((uint8_t*)bits + (size_t)y * pw * 4, mr.bits + (size_t)y * mr.pitch, (size_t)pw * 4);
                HGDIOBJ prev = SelectObject(dc, bmp);
                POINT zero{0, 0};
                SIZE sz{(LONG)pw, (LONG)ph};
                BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
                UpdateLayeredWindow(g_drag.preview, screen, nullptr, &sz, dc, &zero, 0, &bf, ULW_ALPHA);
                SelectObject(dc, prev);
            }
            if (bmp) DeleteObject(bmp);
            DeleteDC(dc);
            ReleaseDC(nullptr, screen);
            cpu->Unmap();
        }
    }
    if (cpu) cpu->Release();
    target->Release();
}
// Where the pointer sits in the window the dragged tabs end up in, so the drop places it under the tab: a new window
// has its first tab under the pointer, where the tab was grabbed; a window moved by all its tabs keeps the place it was
// grabbed at (screen pixels, from src's frame and strip; Windows' frame metrics, the same idea on any OS)
void set_new_off() {
    RECT wr;
    GetWindowRect(g_drag.src, &wr);
    POINT org{0, 0};
    ClientToScreen(g_drag.src, &org);
    const bool whole = g_drag.tabs.size() == g.page_tabs.size();
    const float x0 = g.strip_x0 + g.tab_scroll;
    g_drag.new_off = whole ? g_drag.win_off
                           : POINT{(org.x - wr.left) + (LONG)std::lround((x0 + g_drag.grab_dx) * g.scale),
                                   (org.y - wr.top) + (LONG)std::lround(18 * g.scale)};
}
void show_preview(POINT at) {
    if (!g_drag.preview) render_chip();
    if (!g_drag.preview) return;
    // held where the tab was grabbed (2026-11-22: it was a list box beside the pointer); over another window's strip it
    // hangs below and right of the pointer instead, so it never covers that strip's vertical line (the first test
    // pictures showed the chip hiding it). (The window is the chip plus its shadow margin all round.)
    const int h = (int)std::lround(kDragChipH * g.scale), mp = (int)std::lround(kChipMargin * g.scale);
    const int gx = (int)std::lround(std::clamp(g_drag.grab_dx, 8.0f, chip_w() - 10) * g.scale);
    const int x = g_drag.dock ? at.x + (int)std::lround(10 * g.scale) : at.x - gx;
    const int y = g_drag.dock ? at.y + (int)std::lround(16 * g.scale) : at.y - h / 2;
    SetWindowPos(g_drag.preview, HWND_TOPMOST, x - mp, y - mp, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
void hide_preview() {
    if (g_drag.preview) ShowWindow(g_drag.preview, SW_HIDE);
}
// The Studio window under a screen point, by z-order, skipping the window being moved and the drag's own see-through
// windows (WindowFromPoint can't be used once the dragged window itself sits under the pointer). Another program's
// window on top means none. Windows-only (the z-order walk); Linux / macOS: the toolkit's stacking order
HWND studio_window_at(POINT scr) {
    for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT)) {
        if (h == g_drag.src || h == g_drag.preview || !IsWindowVisible(h)) continue;
        RECT r;
        GetWindowRect(h, &r);
        if (!PtInRect(&r, scr)) continue;
        for (auto& w : g_windows)
            if (w.hwnd == h) return h;
        const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
        if (ex & WS_EX_TRANSPARENT) continue;  // (click-through overlays: not in the way)
        return nullptr;
    }
    return nullptr;
}
// the dock target under the pointer: another Studio window whose tab strip is under it (and where in it), or none
bool find_dock(POINT scr, HWND& target, int& at) {
    HWND under = studio_window_at(scr);
    if (!under || under == g_drag.src) return false;
    for (auto& w : g_windows) {
        if (w.hwnd != under) continue;
        const WinState& s = w.active ? static_cast<const WinState&>(g) : w.parked;
        POINT c = scr;
        ScreenToClient(under, &c);
        const float x = c.x / s.scale, y = c.y / s.scale;
        if (y < -8 || y > kTabBar + 16) return false;
        target = under;
        at = std::clamp((int)std::lround((x - s.strip_x0) / s.strip_step), 0, (int)s.page_tabs.size());
        return true;
    }
    return false;
}
// the drag is over: everything resets, the preview window hides (it is kept for the next drag)
// every window's 3D view holds still while tabs are dragged, and carries on from the same frame afterwards (snatcherLeo,
// 2026-11-19); the parked windows' views are reached through their slots
void pause_views(bool on) {
    g.viewer.set_paused(on);
    for (auto& w : g_windows)
        if (!w.active) w.parked.viewer.set_paused(on);
}
void drag_end() {
    if (g_drag.active) pause_views(false);
    if (g_drag.preview) DestroyWindow(g_drag.preview);  // (drawn for one drag's tabs: made again for the next)
    const HWND old_dock = g_drag.dock;
    g_drag = TabDrag{};
    if (old_dock) InvalidateRect(old_dock, nullptr, FALSE);
}
// the pointer moved while a tab is pressed (screen point; DIP point in src)
void drag_move(POINT scr, float cx, float cy) {
    if (!g_drag.active) {
        const int dx = scr.x - g_drag.press_pt.x, dy = scr.y - g_drag.press_pt.y;
        if (dx * dx + dy * dy < (int)std::lround(36 * g.scale * g.scale)) return;  // 6 DIPs before a drag starts
        g_drag.active = true;
        pause_views(true);
        const std::vector<int> s = sel_list();
        g_drag.tabs = std::find(s.begin(), s.end(), g_drag.press_tab) != s.end() ? s : std::vector<int>{g_drag.press_tab};
        g_drag.names.clear();
        for (int t : g_drag.tabs) g_drag.names.push_back(tab_label(g.page_tabs[(size_t)t]));
        g_drag.grab_dx = cx - (g.strip_x0 + (float)g_drag.press_tab * g.strip_step);
        RECT wr;
        GetWindowRect(g.hwnd, &wr);
        g_drag.win_off = {g_drag.press_pt.x - wr.left, g_drag.press_pt.y - wr.top};
        set_new_off();
    }
    g_drag.mouse_x = cx;
    const HWND old_dock = g_drag.dock;
    g_drag.dock = nullptr;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    const bool in_strip = cy > -30 && cy < kTabBar + 30 && cx > -30 && cx < rc.right / g.scale + 30;
    if (in_strip) {
        // reordering in this window: where the dragged tabs land among the others
        g_drag.torn = false;
        hide_preview();
        const int others = (int)g.page_tabs.size() - (int)g_drag.tabs.size();
        g_drag.insert = std::clamp((int)std::lround((cx - g_drag.grab_dx - g.strip_x0) / g.strip_step), 0, others);
    } else {
        g_drag.torn = true;
        HWND target = nullptr;
        int at = 0;
        if (find_dock(scr, target, at)) { g_drag.dock = target; g_drag.dock_at = at; }
        // the dragged tab rides under the pointer; over another window's strip, that strip's vertical line shows where
        // they land; anywhere else, letting go opens a new window there (or moves the window: all its tabs)
        show_preview(scr);
    }
    if (old_dock && old_dock != g_drag.dock) InvalidateRect(old_dock, nullptr, FALSE);
    if (g_drag.dock) InvalidateRect(g_drag.dock, nullptr, FALSE);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}
// the button released: reorder, dock into another window, or tear off into a new one
void drag_drop(POINT scr) {
    const HWND src = g.hwnd;
    if (!g_drag.active) {
        // a plain click on one of several selected tabs: now the selection clears (no drag happened)
        if (g_drag.clear_on_up) {
            std::fill(g.tab_sel.begin(), g.tab_sel.end(), 0);
            InvalidateRect(src, nullptr, FALSE);
        }
        drag_end();
        return;
    }
    const std::vector<int> tabs = g_drag.tabs;
    if (!g_drag.torn) {
        const int at = g_drag.insert;
        drag_end();
        put_tabs(take_tabs(tabs, false), at);
        logf(Level::Info, "tabs", "%zu tab(s) moved to place %d", tabs.size(), at);
        return;
    }
    const HWND dock = g_drag.dock;
    const int dock_at = g_drag.dock_at;
    const POINT new_off = g_drag.new_off;
    drag_end();
    if (dock) {
        // into another window's strip at the marked place
        std::vector<TabRec> recs = take_tabs(tabs);
        const bool emptied = g.page_tabs.empty();
        place_children();
        InvalidateRect(src, nullptr, FALSE);
        activate(dock);
        put_tabs(recs, dock_at);
        SetForegroundWindow(dock);
        if (emptied) PostMessageW(src, WM_CLOSE, 0, 0);
        logf(Level::Info, "tabs", "%zu tab(s) moved to another window", recs.size());
        return;
    }
    // torn off: all of a window's tabs move the window itself (as in Chrome), onto the outline; otherwise a new window
    // opens on the outline
    const int off = (int)std::lround(40 * g.scale);
    if (tabs.size() == g.page_tabs.size()) {
        const POINT at = new_off.x || new_off.y ? POINT{scr.x - new_off.x, scr.y - new_off.y} : POINT{scr.x - off * 3, scr.y - off / 2};
        SetWindowPos(src, nullptr, at.x, at.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        logf(Level::Info, "tabs", "window moved by its %zu tab(s)", tabs.size());
        return;
    }
    std::vector<TabRec> recs = take_tabs(tabs);
    place_children();
    InvalidateRect(src, nullptr, FALSE);
    NewWindowTabs nw;
    for (const TabRec& r : recs) { nw.tabs.push_back(r.idx); nw.hist.push_back(r.hist); nw.pos.push_back(r.pos); }
    nw.has_at = true;
    // (exactly where the live window was: the pointer keeps its place over the first tab)
    nw.at = new_off.x || new_off.y ? POINT{scr.x - new_off.x, scr.y - new_off.y} : POINT{scr.x - off * 3, scr.y - off / 2};
    open_window(std::move(nw));
    if (recs.size() > 1) {  // they arrive selected together, as they left
        g.tab_sel.assign(g.page_tabs.size(), 1);
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }
}

// --drag-test T (2026-11-22, snatcherLeo's 4.3 part 2: the live window and the line): a tab drag played out with made-up
// pointer moves (the real mouse is never touched), a picture of the screen around the windows after each step, written
// to the logs folder as drag_1_torn.png ... drag_4_back.png. Tab T of the first window (with --tab-select, the
// selection) is torn off into a new window; then the new window's tabs (all of them: the window itself moves) are
// dragged back onto the first window's strip and dropped there. Windows-only (screen capture with GDI); a test switch
int g_drag_test = -1, g_drag_stage = 0;
// --menu-test ID (2026-11-22, the dark menus): click hit ID (a button that opens a native menu: 7204 the tab list, the
// rate or target buttons...), photograph the screen round the window while the menu is open (menu_test.png in the logs
// folder), close the menu and Studio. The menu's modal loop still delivers the window's timers, which is what takes the
// picture and ends the menu (EndMenu). Windows-only; a test switch
int g_menu_test = 0;
// --window-cycle N (2026-11-22, after snatcherLeo's multi-window stress test: memory stayed up after windows closed): opens a
// second window on a Pokémon and closes it again, N times, 1.5 s apart, with the memory watch after every close. Flat
// numbers mean a closed window's memory is reused; numbers climbing each cycle mean a window leaks. A test switch
int g_window_cycle = 0, g_window_cycles_done = 0;
HWND g_cycle_main = nullptr, g_cycle_win = nullptr;
HWND g_drag_main = nullptr, g_drag_new = nullptr;
void drag_test_shot(const char* name) {
    RECT u{};
    for (HWND h : {g_drag_main, g_drag_new, g_drag.preview}) {
        if (!h || !IsWindow(h) || !IsWindowVisible(h)) continue;
        RECT r;
        GetWindowRect(h, &r);
        if (IsRectEmpty(&u)) u = r;
        else UnionRect(&u, &u, &r);
    }
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    u.left = std::max(u.left - 40, (LONG)vx);
    u.top = std::max(u.top - 40, (LONG)vy);
    u.right = std::min(u.right + 40, (LONG)(vx + GetSystemMetrics(SM_CXVIRTUALSCREEN)));
    u.bottom = std::min(u.bottom + 40, (LONG)(vy + GetSystemMetrics(SM_CYVIRTUALSCREEN)));
    const int w = u.right - u.left, h = u.bottom - u.top;
    if (w <= 0 || h <= 0) return;
    HDC sdc = GetDC(nullptr), mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi{};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mdc, bmp);
    BitBlt(mdc, 0, 0, w, h, sdc, u.left, u.top, SRCCOPY | CAPTUREBLT);  // (CAPTUREBLT: layered windows too)
    GdiFlush();
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    const uint8_t* s = static_cast<const uint8_t*>(bits);
    for (size_t i = 0; i < (size_t)w * h; ++i) { rgba[i * 4] = s[i * 4 + 2]; rgba[i * 4 + 1] = s[i * 4 + 1]; rgba[i * 4 + 2] = s[i * 4]; rgba[i * 4 + 3] = 255; }
    OrreImage im{w, h, rgba.data()};
    const std::string path = narrow(g.logs_dir) + "\\" + name;
    logf(Level::Info, "test", "drag test: %s (%dx%d) %s", name, w, h, orre_png_write(path.c_str(), &im) ? "written" : "FAILED");
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(nullptr, sdc);
}
// a made-up pointer move to a screen point, handled as the pressed window's WM_MOUSEMOVE would. (The real mouse still
// sends the pressed window its own moves between the test's steps; each drop first repeats the last made-up point.)
POINT g_drag_last{};
void drag_test_move(POINT scr) {
    g_drag_last = scr;
    activate(g_drag.src);
    POINT c = scr;
    ScreenToClient(g.hwnd, &c);
    drag_move(scr, c.x / g.scale, c.y / g.scale);
    UpdateWindow(g.hwnd);
    if (g_drag.dock) UpdateWindow(g_drag.dock);
    if (g_drag.preview) UpdateWindow(g_drag.preview);
}
// pressing tab t of window `w` (its strip's place for that tab, on screen)
POINT drag_test_press(HWND w, int t) {
    activate(w);
    POINT p{(LONG)px(g.strip_x0 + (float)t * g.strip_step + 40), (LONG)px(kTabBar / 2)};
    ClientToScreen(w, &p);
    g_drag.pressed = true;
    g_drag.src = w;
    g_drag.press_tab = t;
    g_drag.press_pt = p;
    tab_press(t, false, false);
    return p;
}
void drag_test_step(HWND hwnd) {
    switch (g_drag_stage++) {
        case 0: {  // tear tab T off: down and to the right, past the strip
            g_drag_main = hwnd;
            const bool keep_sel = g.tab_sel.size() > (size_t)g_drag_test && g.tab_sel[(size_t)g_drag_test];
            const std::vector<char> sel = g.tab_sel;
            const POINT p = drag_test_press(hwnd, g_drag_test);
            if (keep_sel) g.tab_sel.assign(sel.begin(), sel.end());  // (the press keeps a multi-selection, as a real one does)
            drag_test_move({p.x + 2, p.y + (LONG)px(8)});
            for (int k = 1; k <= 6; ++k) drag_test_move({p.x + (LONG)px(30.0f * k), p.y + (LONG)px(45.0f * k)});
            drag_test_shot("drag_1_torn.png");
            break;
        }
        case 1: {  // let go: the new window opens where the live window was
            const POINT p = g_drag_last;
            drag_test_move(p);
            activate(g_drag.src);
            drag_drop(p);
            g_drag_new = g.hwnd != g_drag_main ? g.hwnd : nullptr;
            if (!g_drag_new)
                for (auto& w : g_windows)
                    if (w.hwnd != g_drag_main) g_drag_new = w.hwnd;
            logf(Level::Info, "test", "drag test: %zu windows after the drop", g_windows.size());
            UpdateWindow(g_drag_main);
            if (g_drag_new) UpdateWindow(g_drag_new);
            drag_test_shot("drag_2_dropped.png");
            break;
        }
        case 2: {  // the new window's tabs (all of them): the window moves, then over the first window's strip
            if (!g_drag_new) { g_drag_stage = 4; break; }
            activate(g_drag_new);
            g.tab_sel.assign(g.page_tabs.size(), 1);
            const POINT p = drag_test_press(g_drag_new, 0);
            g.tab_sel.assign(g.page_tabs.size(), 1);
            drag_test_move({p.x, p.y + (LONG)px(10)});
            drag_test_move({p.x + (LONG)px(20), p.y + (LONG)px(80)});  // torn: the window follows
            activate(g_drag_main);
            POINT q{(LONG)px(g.strip_x0 + (float)g.page_tabs.size() * g.strip_step - 4), (LONG)px(kTabBar / 2)};
            ClientToScreen(g_drag_main, &q);
            for (int k = 1; k <= 8; ++k) drag_test_move({p.x + (q.x - p.x) * k / 8, p.y + (LONG)px(80) + (q.y - p.y - (LONG)px(80)) * k / 8});
            logf(Level::Info, "test", "drag test: over the first window's strip: dock %s at %d", g_drag.dock ? "yes" : "no", g_drag.dock_at);
            drag_test_shot("drag_3_dock.png");
            break;
        }
        case 3: {  // let go over the strip: the tabs go back, the emptied window closes
            const POINT q = g_drag_last;
            drag_test_move(q);
            activate(g_drag.src);
            drag_drop(q);
            break;
        }
        case 4: {
            activate(g_drag_main);
            logf(Level::Info, "test", "drag test: %zu window(s) at the end, %zu tabs in the first", g_windows.size(), g.page_tabs.size());
            UpdateWindow(g_drag_main);
            g_drag_new = nullptr;
            drag_test_shot("drag_4_back.png");
            break;
        }
        default:
            KillTimer(hwnd, TIMER_FLASH + 8);
            for (const WindowSlot& w : g_windows) PostMessageW(w.hwnd, WM_CLOSE, 0, 0);
            break;
    }
}

// A tab's right-click menu (multi-window plan step 2): Move to new window, Duplicate, Close, Close other tabs, Close tabs
// to the right. A native menu at the pointer (Windows' TrackPopupMenu; Linux / macOS: the toolkit's popup menu).
void tab_menu(int t) {
    if (t < 0 || t >= (int)g.page_tabs.size()) return;
    enum { M_MOVE = 1, M_DUP, M_CLOSE, M_OTHERS, M_RIGHT };
    const int n = (int)g.page_tabs.size();
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, M_MOVE, tx("studio.tabmenu.move_new").c_str());
    AppendMenuW(m, MF_STRING, M_DUP, tx("studio.tabmenu.duplicate").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, M_CLOSE, tx("studio.tabmenu.close").c_str());
    AppendMenuW(m, MF_STRING | (n > 1 ? 0 : MF_GRAYED), M_OTHERS, tx("studio.tabmenu.close_others").c_str());
    AppendMenuW(m, MF_STRING | (t < n - 1 ? 0 : MF_GRAYED), M_RIGHT, tx("studio.tabmenu.close_right").c_str());
    POINT p;
    GetCursorPos(&p);
    const int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, g.hwnd, nullptr);
    DestroyMenu(m);
    // Move and Close act on every selected tab when the clicked one is among them (step 3), else on it alone
    const std::vector<int> s = sel_list();
    const std::vector<int> targets = s.size() > 1 && std::find(s.begin(), s.end(), t) != s.end() ? s : std::vector<int>{t};
    switch (pick) {
        case M_MOVE: {
            // the tabs, with their history, go to a new window; they leave this one first (a window left with no
            // tabs closes, unless it is the only one)
            const HWND src = g.hwnd;
            std::vector<TabRec> recs = take_tabs(targets);
            const bool emptied = g.page_tabs.empty();
            place_children();
            NewWindowTabs mv;
            for (const TabRec& r : recs) { mv.tabs.push_back(r.idx); mv.hist.push_back(r.hist); mv.pos.push_back(r.pos); }
            open_window(std::move(mv));
            if (emptied && g_windows.size() > 1) PostMessageW(src, WM_CLOSE, 0, 0);
            break;
        }
        case M_DUP: {
            // a copy with the same history, opened at the end of the strip and shown
            const std::vector<int> hist = g.tab_hist[(size_t)t];
            const int pos = g.tab_pos[(size_t)t];
            open_page(g.page_tabs[(size_t)t], true);
            g.tab_hist.back() = hist;
            g.tab_pos.back() = pos;
            break;
        }
        case M_CLOSE:
            for (size_t k = targets.size(); k-- > 0;) click(HIT_PTAB_CLOSE + targets[k]);
            break;
        case M_OTHERS:
            for (int k = n - 1; k >= 0; --k)
                if (k != t) click(HIT_PTAB_CLOSE + k);
            break;
        case M_RIGHT:
            for (int k = n - 1; k > t; --k) click(HIT_PTAB_CLOSE + k);
            break;
    }
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// The global search's two fields (the top bar's and the palette's): Up / Down move through the results, Enter opens
// the selected one (Ctrl+Enter in a new tab), Escape closes (and clears the top field), Ctrl+K opens the palette.
// Windows' edit control underneath (typing, the IME, selection); Linux / macOS: the toolkit's line edit.
LRESULT CALLBACK gsearch_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    activate(GetParent(h));  // its window's state (as for the list's search box)
    if (m == WM_KEYDOWN) {
        const bool ctrl = GetKeyState(VK_CONTROL) < 0;
        const int n = (int)g.sres.size();
        if (w == VK_DOWN || w == VK_UP) {
            if (n) g.ssel = std::clamp(g.ssel + (w == VK_DOWN ? 1 : -1), 0, n - 1);
            InvalidateRect(g.hwnd, nullptr, FALSE);
            return 0;
        }
        if (w == VK_RETURN) { choose_result(g.ssel, ctrl); return 0; }
        if (w == VK_ESCAPE) {
            if (h == g.gsearch) SetWindowTextW(h, L"");
            close_search();
            SetFocus(g.hwnd);
            return 0;
        }
        if (ctrl && w == 'K') { open_palette(); return 0; }
        if ((w == VK_F8 || (ctrl && (w == VK_TAB || w == 'W' || w == 'T' || (w >= '1' && w <= '9')))) && key(w)) return 0;
    }
    // the control characters those keys would type (Enter, Escape, Ctrl+K) are swallowed, so the edit doesn't beep
    if (m == WM_CHAR && w < 0x20 && w != VK_BACK) return 0;
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS) InvalidateRect(g.hwnd, nullptr, FALSE);
    return DefSubclassProc(h, m, w, l);
}

// ---- a smooth start: the first window appears once, complete (snatcherLeo, 2026-11-19: "on startup, will it always have a
// flicker ... what ways can we mitigate opening the app so it's more pleasant to the eyes", then "I'd do 1, yes build
// it") ----
// Measured before (2026-11-19, a warm start): the window showed at about 0.27 s, painted "Reading the disc…" at 0.33 s,
// the Pokémon page (no model yet) at 0.34 s and the model at 0.47 s: three changes of picture in half a second. Now the
// first window is created cloaked (DWMWA_CLOAK: it exists, lays out and draws, but isn't on screen), and is uncloaked
// once its first full page, model included, has been drawn (WM_VIEW_FRAMED), or straight away when there is nothing to
// wait for (no disc chosen, the disc couldn't be read, no 3D view, a Pokémon without a model). A 1 s limit
// (TIMER_REVEAL) uncloaks it whatever it shows, so it can never stay invisible. Screenshot runs don't cloak. Windows
// only (DWM cloaking); Linux / macOS: show the window after its first frame (the toolkit's "ready to show" signal).
constexpr DWORD kDwmCloak = 13;  // DWMWA_CLOAK (dwmapi.h, Windows 8+)

// ---- --flash-test N: measuring the 3D view's white flash on the real screen (2026-11-19) ----
// snatcherLeo saw a white flash when going from the Overview to another page tab and back. Studio's own screenshots can't
// show it (they capture Studio's drawing, not what Windows puts on screen), so this reads the screen itself: a thread
// copies the 3D view's rectangle from the desktop as fast as it can (BitBlt from the screen; a few ms a read) and
// records how much of it is near-white, while the window flips Overview / Moves every 200 ms. The log gets the
// brightest reads and a count of "white" frames. It reads pixels only, sends no input, and closes Studio at the end.
// Windows only (screen DC); Linux / macOS: the compositor's screenshot APIs.
struct FlashSample { double ms; double white; int tab; double mean = 0; };
std::vector<FlashSample> g_flash;
// --flash-watch S (2026-11-19): the same screen reads for S seconds while snatcherLeo clicks around himself (no clicks sent,
// Studio stays open), for a flash the automated flips don't produce ("white flashes in the model viewer where it would
// load the model of the pokemon in the overview tab"). The whitest read is kept and written to logs\flash_whitest.png
std::vector<uint8_t> g_flash_worst;  // BGRA of the whitest read
int g_flash_w = 0, g_flash_h = 0;
double g_flash_worst_share = -1, g_flash_worst_ms = 0;
// the biggest one-read brightening (a read brighter than both its neighbours) and its picture: flash_spike.png
double g_flash_spike = 0, g_flash_spike_ms = 0;
std::vector<uint8_t> g_flash_spike_px;
int g_flash_spike_w = 0, g_flash_spike_h = 0;
std::atomic<bool> g_flash_stop{false};
std::thread g_flash_thread;
int g_flash_ticks = 0;
void flash_test_start(HWND hwnd) {
    RECT r;
    // the whole window by default (a flash could be anywhere on the page); ORRE_FLASH_STAGE=1: the 3D view's rectangle
    // only (it keeps its place even while cut)
    GetWindowRect(std::getenv("ORRE_FLASH_STAGE") ? g.viewer.hwnd() : hwnd, &r);
    const int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    logf(Level::Info, "flash", "test: %d flips, reading %dx%d screen pixels at %ld,%ld", g.flash_test, w, h, r.left, r.top);
    const auto t0 = std::chrono::steady_clock::now();
    // *Updated 2026-11-22* (snatcherLeo's flash on Zapdos, after starting on the Moves tab): with ORRE_FLASH_STAGE the reads
    // follow the 3D view wherever it is now (the Overview's card and the Moves preview sit in different places), and
    // every sudden one-read brightening is noted (a flash needn't be near-white: a yellow model's flash was missed by the
    // white count), the biggest kept as flash_spike.png. Two pictures alternate, so the read before the spike's next
    // one is still there to save.
    const HWND follow = std::getenv("ORRE_FLASH_STAGE") ? g.viewer.hwnd() : nullptr;
    g_flash_thread = std::thread([r, w, h, t0, follow] {
        HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen);
        RECT at = r;
        int cw = 0, ch = 0;
        void* bits[2] = {nullptr, nullptr};
        HBITMAP bmp[2] = {nullptr, nullptr};
        HGDIOBJ old = nullptr;
        auto make = [&](int nw, int nh) {
            if (old) SelectObject(mem, old);
            for (int k = 0; k < 2; ++k) if (bmp[k]) DeleteObject(bmp[k]);
            BITMAPINFO bi{};
            bi.bmiHeader = {sizeof(BITMAPINFOHEADER), nw, -nh, 1, 32, BI_RGB};
            for (int k = 0; k < 2; ++k) bmp[k] = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits[k], nullptr, 0);
            old = SelectObject(mem, bmp[0]);
            cw = nw;
            ch = nh;
        };
        make(w, h);
        double m2 = -1, m1 = -1;  // the two reads before this one: their means
        int cur = 0;
        while (!g_flash_stop) {
            if (follow) {
                RECT fr;
                if (GetWindowRect(follow, &fr) && fr.right > fr.left && fr.bottom > fr.top) {
                    at = fr;
                    if (fr.right - fr.left != cw || fr.bottom - fr.top != ch) { make(fr.right - fr.left, fr.bottom - fr.top); m2 = m1 = -1; }
                }
            }
            SelectObject(mem, bmp[cur]);
            BitBlt(mem, 0, 0, cw, ch, screen, at.left, at.top, SRCCOPY);
            const uint8_t* p = static_cast<const uint8_t*>(bits[cur]);
            size_t white = 0;
            double sum = 0;
            for (size_t i = 0; i < (size_t)cw * ch; ++i, p += 4) {
                if (p[0] > 225 && p[1] > 225 && p[2] > 225) ++white;
                sum += 0.0722 * p[0] + 0.7152 * p[1] + 0.2126 * p[2];  // (BGRA: luminance by the sRGB weights)
            }
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            const double share = (double)white / ((double)cw * ch), mean = sum / ((double)cw * ch);
            g_flash.push_back({ms, share, g.tab, mean});  // (g.tab: a label only)
            if (share > g_flash_worst_share) {  // (the whitest read, kept for the picture)
                g_flash_worst_share = share;
                g_flash_worst_ms = ms;
                g_flash_w = cw;
                g_flash_h = ch;
                g_flash_worst.assign(static_cast<const uint8_t*>(bits[cur]), static_cast<const uint8_t*>(bits[cur]) + (size_t)cw * ch * 4);
            }
            // the previous read, now that both its neighbours are known: a spike if it is brighter than both by 10+
            if (m2 >= 0 && m1 >= 0) {
                const double jump = m1 - std::max(m2, mean);
                if (jump >= 10 && jump > g_flash_spike) {
                    g_flash_spike = jump;
                    g_flash_spike_ms = g_flash.size() >= 2 ? g_flash[g_flash.size() - 2].ms : ms;
                    g_flash_spike_px.assign(static_cast<const uint8_t*>(bits[1 - cur]),
                                            static_cast<const uint8_t*>(bits[1 - cur]) + (size_t)cw * ch * 4);
                    g_flash_spike_w = cw;
                    g_flash_spike_h = ch;
                }
            }
            m2 = m1;
            m1 = mean;
            cur = 1 - cur;
        }
        if (old) SelectObject(mem, old);
        for (int k = 0; k < 2; ++k) if (bmp[k]) DeleteObject(bmp[k]);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    });
    g_flash_ticks = 0;
    SetTimer(hwnd, TIMER_FLASH, 200, nullptr);
}
// ORRE_FLASH_MOUSE=1: the flips are real mouse clicks (the pointer moved onto the page tab, pressed and released), as
// snatcherLeo clicks (2026-11-19, he allowed the test to take his pointer for a few seconds); the pointer goes back after
POINT g_flash_cursor{};
void flash_test_finish(HWND hwnd);
void flash_test_tick(HWND hwnd) {
    if (g.flash_watch > 0 && ++g_flash_ticks * 200 < g.flash_watch * 1000) return;  // (watching only: no clicks)
    if (g.flash_watch <= 0 && ++g_flash_ticks <= g.flash_test * 2) {
        int id = HIT_TAB + (g.tab == kTabOverview ? 1 : kTabOverview);  // Overview <-> Moves
        // ORRE_FLASH_KIND=species: down the list instead, one Pokémon after another (snatcherLeo, 2026-11-19: the flash "if I
        // go from Overview of one Pokemon, to another Pokemon down the list"); the next row below the selected one
        // ORRE_FLASH_KIND=jump: far jumps around the list, as snatcherLeo clicks ("click from charmander to something like
        // butterfree, random stuff, down the list"); a fixed sequence, so runs compare
        // ORRE_FLASH_KIND=mix (2026-11-22, snatcherLeo's flash on Zapdos: picks on Moves, then Overview, then picks there): a
        // cycle of the Moves tab, two jumps, the Overview, two jumps
        const char* kind = std::getenv("ORRE_FLASH_KIND");
        const bool mix = kind && std::strcmp(kind, "mix") == 0;
        if (mix && (g_flash_ticks % 6 == 1 || g_flash_ticks % 6 == 4)) {
            id = HIT_TAB + (g_flash_ticks % 6 == 1 ? kTabMoves : kTabOverview);
        } else if (mix) {
            int pos = 0;
            for (size_t q = 0; q < g.shown.size(); ++q)
                if (g.shown[q] == g.selected) pos = (int)q;
            id = HIT_ROW + (pos * 7 + 37 + g_flash_ticks * 13) % std::max(1, (int)g.shown.size());
        } else if (const char* k = kind; k && (std::strcmp(k, "species") == 0 || std::strcmp(k, "jump") == 0)) {
            int pos = 0;
            for (size_t q = 0; q < g.shown.size(); ++q)
                if (g.shown[q] == g.selected) pos = (int)q;
            const int n = (int)g.shown.size();
            id = HIT_ROW + (std::strcmp(k, "jump") == 0 ? (pos * 7 + 37 + g_flash_ticks * 13) % std::max(1, n)
                                                        : std::min(pos + 1, n - 1));
        }
        if (!std::getenv("ORRE_FLASH_MOUSE")) { click(id); return; }
        if (g_flash_ticks == 1) GetCursorPos(&g_flash_cursor);
        for (const auto& [r, hid] : g.hits)
            if (hid == id) {
                POINT p{(LONG)px(r.x + r.w / 2), (LONG)px(r.y + r.h / 2)};
                ClientToScreen(hwnd, &p);
                SetCursorPos(p.x, p.y);
                // pressed now, released 120 ms later (a person holds the button that long; TIMER_FLASH + 1)
                INPUT in{};
                in.type = INPUT_MOUSE;
                in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &in, sizeof(INPUT));
                SetTimer(hwnd, TIMER_FLASH + 1, 120, nullptr);
                break;
            }
        return;
    }
    if (std::getenv("ORRE_FLASH_MOUSE")) SetCursorPos(g_flash_cursor.x, g_flash_cursor.y);
    flash_test_finish(hwnd);
    if (g.flash_watch <= 0) PostMessageW(hwnd, WM_CLOSE, 0, 0);  // (a watch leaves Studio open)
}
// The results, at the end of the test or watch, and also when Studio closes while a watch still runs (WM_DESTROY;
// *added 2026-11-22*: snatcherLeo closed Studio at 174 s of a 300 s watch and nothing was written)
void flash_test_finish(HWND hwnd) {
    if (!g_flash_thread.joinable()) return;
    KillTimer(hwnd, TIMER_FLASH);
    g_flash_stop = true;
    g_flash_thread.join();
    // the share of near-white pixels per read: the model itself has a few, a flash covers the stage
    double worst = 0, base = 1;
    int flashes = 0;
    for (const auto& s : g_flash) {
        worst = std::max(worst, s.white);
        if (s.tab == kTabOverview) base = std::min(base, s.white);
        if (s.white > 0.5) ++flashes;
    }
    logf(Level::Info, "flash", "result: %zu screen reads, near-white share lowest %.3f, highest %.3f; reads over half white: %d",
         g_flash.size(), base, worst, flashes);
    for (const auto& s : g_flash)
        if (s.white > 0.5) logf(Level::Info, "flash", "  white read at %.0f ms (%.2f, page tab %d)", s.ms, s.white, s.tab);
    // every read's average brightness (0-255), in order, so a flash of any colour shows as a spike between two levels
    std::string line;
    for (const auto& s : g_flash) line += std::to_string((int)std::lround(s.mean)) + (s.tab == kTabOverview ? "o " : "m ");
    logf(Level::Info, "flash", "brightness per read (o Overview, m Moves): %s", line.c_str());
    // the reads with any real white (over 3% of the area: a flash of the 3D view alone is about a tenth of the window)
    for (const auto& s : g_flash)
        if (s.white > 0.03) logf(Level::Info, "flash", "  whitish read at %.0f ms after the start: %.3f of the area", s.ms, s.white);
    // every one-read brightening of 10+ (any colour): its time after the start (the "test:" line's time + this = the log's
    // clock, to set beside the trail's clicks), its jump and the page tab
    int spikes = 0;
    for (size_t i = 1; i + 1 < g_flash.size(); ++i) {
        const double jump = g_flash[i].mean - std::max(g_flash[i - 1].mean, g_flash[i + 1].mean);
        if (jump < 10) continue;
        if (++spikes <= 40)
            logf(Level::Info, "flash", "  brightening at %.0f ms after the start: +%.0f (%.0f -> %.0f -> %.0f), page tab %d", g_flash[i].ms,
                 jump, g_flash[i - 1].mean, g_flash[i].mean, g_flash[i + 1].mean, g_flash[i].tab);
    }
    logf(Level::Info, "flash", "one-read brightenings of 10 or more: %d", spikes);
    if (!g_flash_spike_px.empty()) {
        OrreImage im{g_flash_spike_w, g_flash_spike_h, static_cast<uint8_t*>(std::malloc(g_flash_spike_px.size()))};
        for (size_t i = 0; i + 3 < g_flash_spike_px.size(); i += 4) {
            im.rgba[i] = g_flash_spike_px[i + 2]; im.rgba[i + 1] = g_flash_spike_px[i + 1]; im.rgba[i + 2] = g_flash_spike_px[i];
            im.rgba[i + 3] = 255;
        }
        const std::string path = narrow(g.logs_dir) + "\\flash_spike.png";
        orre_png_write(path.c_str(), &im);
        orre_image_free(&im);
        logf(Level::Info, "flash", "the biggest brightening (+%.0f at %.0f ms) written to %s", g_flash_spike, g_flash_spike_ms, path.c_str());
    }
    if (!g_flash_worst.empty()) {
        // the whitest read as a picture (BGRA -> RGBA), next to the logs
        OrreImage im{g_flash_w, g_flash_h, static_cast<uint8_t*>(std::malloc(g_flash_worst.size()))};
        for (size_t i = 0; i + 3 < g_flash_worst.size(); i += 4) {
            im.rgba[i] = g_flash_worst[i + 2]; im.rgba[i + 1] = g_flash_worst[i + 1]; im.rgba[i + 2] = g_flash_worst[i]; im.rgba[i + 3] = 255;
        }
        const std::string path = narrow(g.logs_dir) + "\\flash_whitest.png";
        orre_png_write(path.c_str(), &im);
        orre_image_free(&im);
        logf(Level::Info, "flash", "the whitest read (%.3f, at %.0f ms) written to %s", g_flash_worst_share, g_flash_worst_ms, path.c_str());
    }
}

void reveal(const char* why) {
    if (!g.cloaked) return;
    g.cloaked = false;
    const HWND first = g_windows.empty() ? g.hwnd : g_windows.front().hwnd;
    KillTimer(first, TIMER_REVEAL);
    if (first == g.hwnd) paint();  // the page as it is now, before it comes on screen
    BOOL off = FALSE;
    DwmSetWindowAttribute(first, kDwmCloak, &off, sizeof off);
    logf(Level::Info, "app", "window shown after %.0f ms (%s)",
         std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g.started).count(), why);
    startup_done();  // (the startup timeline, item 7)
    if (first == g.hwnd) log_window_place("opened");
}

// --click-after-model's steps (tests): each click in order, a pause ("w1900", kept as -1900) waits on a timer before the
// rest, so a test can do what a user does at a user's pace (snatcherLeo's Bite report, 2026-11-22: the tab switched while the
// move still played). Win32 timer; another system's port would use its own (only tests use it)
void next_test_click(HWND hwnd) {
    while (!g.test_clicks_model.empty()) {
        const int id = g.test_clicks_model.front();
        if (id < 0) {  // (left in the list until it ends, so the screenshot waits for a closing pause too)
            SetTimer(hwnd, TIMER_FLASH + 7, (UINT)-id, nullptr);
            return;
        }
        g.test_clicks_model.erase(g.test_clicks_model.begin());
        click(id);
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // every message is handled with its own window's state in g (a window not yet created isn't known: WM_NCCREATE
    // and the like arrive before WM_CREATE and go straight to DefWindowProc's handling below)
    // (a window being created gets messages before WM_CREATE: those go to Windows' default handling, untouched)
    if (msg != WM_CREATE && !activate(hwnd)) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_CREATE: {
            // a second or later window (multi-window plan step 2): the window that was active keeps its state in its
            // slot, and g starts this one fresh
            for (auto& w : g_windows)
                if (w.active) { std::swap(static_cast<WinState&>(g), w.parked); w.active = false; }
            g.hwnd = hwnd;
            g_windows.push_back({hwnd, WinState{}, true});  // its state lives in g while it is the active window
            g.win_no = ++g_windows_opened;
            log_set_window(g.win_no);
            log_note_open((int)g_windows.size(), 0);
            g.scale = GetDpiForWindow(hwnd) / 96.0f;
            g.search = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 10, 10, hwnd, (HMENU)1, g.inst, nullptr);
            SetWindowSubclass(g.search, search_proc, 1, 0);
            const std::wstring cue = tx("studio.list.search");
            SendMessageW(g.search, EM_SETCUEBANNER, TRUE, (LPARAM)cue.c_str());
            // the global search's fields (ids 2: the top bar, 3: the palette), with their own key handling
            g.gsearch = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 10, 10, hwnd, (HMENU)2, g.inst, nullptr);
            g.psearch = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd, (HMENU)3, g.inst, nullptr);
            for (HWND sh : {g.gsearch, g.psearch}) SetWindowSubclass(sh, gsearch_proc, 2, 0);
            const std::wstring gcue = tx("studio.search.field"), pcue = tx("studio.search.palette");
            SendMessageW(g.gsearch, EM_SETCUEBANNER, TRUE, (LPARAM)gcue.c_str());
            SendMessageW(g.psearch, EM_SETCUEBANNER, TRUE, (LPARAM)pcue.c_str());
            if (!g.search_font) make_search_font();
            else apply_search_fonts();
            if (!g.no_3d) g.viewer.create(hwnd, g.inst);
            g.viewer.hold_frame(g.hold_frame);
            // (a held frame shows the particles as they'd be after two seconds of the animation, not just starting)
            if (g.hold_frame >= 0) g.viewer.set_particle_warmup(60);
            g.viewer.set_particles(g.particles_on);
            g.viewer.set_game_lighting(g.game_light);
            g.viewer.set_battle_camera(g.battle_cam);
            if (!g.shot_camera.empty()) {
                float v[7] = {0, 0, 0, 0, 0, 0, 45};
                if (std::swscanf(g.shot_camera.c_str(), L"%f,%f,%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) == 7)
                    g.viewer.set_battle_camera_shot(v, v + 3, v[6]);
            }
            g.viewer.set_rate(g.preview_rate);
            if (!g.viewer.ok()) logf(Level::Error, "viewer", "3D view unavailable: %s", g.no_3d ? "turned off (--no-3d)" : g.viewer.error().c_str());
            watch_check("3D view start");  // (the spin starts off and the idle loop on: snatcherLeo, 2026-11-18)
            startup_mark("window and 3D view made");  // (the first window only: the timeline ends when it shows)
            create_target();
            apply_theme();
            // (the 30 s memory check per window is now the program's one "[state]" line every 30 s: perf_timer, item 14)
            // a later window: the list (if the disc is already read) and the tabs it was opened with (open_window)
            if (g_windows.size() > 1) {
                if (g.disc) filter();
                if (g_new_window && !g_new_window->tabs.empty()) {
                    g.page_tabs = g_new_window->tabs;
                    g.tab_hist = g_new_window->hist;
                    g.tab_pos = g_new_window->pos;
                    g.tab_sel.assign(g.page_tabs.size(), 0);
                    g.cur_tab = std::clamp(g_new_window->cur, 0, (int)g.page_tabs.size() - 1);
                    g.navigating = true;  // showing it isn't a new visit
                    open_page(g.page_tabs[(size_t)g.cur_tab]);
                    g.navigating = false;
                } else {
                    open_page(kHome);  // a new, empty window (Ctrl+Shift+N) starts with a Home tab, as a browser's does
                }
                place_children();
            }
            return 0;
        }
        case WM_SIZE:
            resize_target(LOWORD(lp), HIWORD(lp));
            place_children();
            InvalidateRect(hwnd, nullptr, FALSE);
            // maximise, minimise and restore change the place at once; a drag waits for its end (WM_EXITSIZEMOVE)
            if (!g_modal && (wp == SIZE_MAXIMIZED || wp == SIZE_MINIMIZED || wp == SIZE_RESTORED) && g.place_seen.length)
                log_window_place(wp == SIZE_MAXIMIZED ? "maximised" : wp == SIZE_MINIMIZED ? "minimised" : "restored");
            return 0;
        // Windows' own loops (the hang watch's flag, item 6) and a drag's end (the window's place, item 5)
        case WM_ENTERSIZEMOVE: g_modal = "a window being moved or resized"; return 0;
        case WM_EXITSIZEMOVE:
            g_modal = nullptr;
            log_window_place("moved or resized");
            return 0;
        case WM_ENTERMENULOOP: g_modal = "a menu open"; return 0;
        case WM_EXITMENULOOP: g_modal = nullptr; return 0;
        case WM_DPICHANGED: {
            g.scale = HIWORD(wp) / 96.0f;
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (g.rt) {  // the back buffer at the new size (SetWindowPos above sent WM_SIZE) and the new DPI
                RECT cr;
                GetClientRect(hwnd, &cr);
                resize_target((UINT)cr.right, (UINT)cr.bottom);
            }
            make_search_font();
            apply_window_icons(hwnd, g.ui_dir + L"\\icons", g.pal.dark);
            place_children();
            logf(Level::Info, "dpi", "now %.0f%%", g.scale * 100);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (!g.rt) create_target();
            paint();
            EndPaint(hwnd, &ps);
            // the list's search box, held back by place_children until the page behind it was drawn: shown now, and
            // painted at once, so it never shows as an empty white box
            if (g.search_deferred) {
                g.search_deferred = false;
                if (g.editor == 0 && g.disc && !on_app_page() && !g.palette_open && !g.drop_open) {
                    ShowWindow(g.search, SW_SHOWNA);
                    UpdateWindow(g.search);
                }
            }
            static bool first_paint = true;  // Direct2D's first frame on its own (it starts its graphics device then)
            if (first_paint) { first_paint = false; watch_check("first window paint"); }
            static bool first_list_paint = true;  // the watch's measure of the first full page's drawing
            if (first_list_paint && g.selected >= 0) { first_list_paint = false; watch_check("first page drawn"); }
            return 0;
        }
        case WM_CTLCOLOREDIT: {
            const Palette& P = g.pal;
            SetTextColor((HDC)wp, RGB((int)(P.text.r * 255), (int)(P.text.g * 255), (int)(P.text.b * 255)));
            if ((HWND)lp == g.gsearch || (HWND)lp == g.psearch) {  // these two sit on card-coloured frames
                SetBkColor((HDC)wp, RGB((int)(P.card.r * 255), (int)(P.card.g * 255), (int)(P.card.b * 255)));
                return (LRESULT)g.card_brush;
            }
            SetBkColor((HDC)wp, RGB((int)(P.bg.r * 255), (int)(P.bg.g * 255), (int)(P.bg.b * 255)));
            return (LRESULT)g.search_brush;
        }
        case WM_COMMAND:
            if ((LOWORD(wp) == 2 || LOWORD(wp) == 3) && (HIWORD(wp) == EN_CHANGE || HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS)) {
                // the global search: every change runs the search; the top field's drop-down shows while it has the
                // focus and some text (it closes when the focus leaves, after a click on a result has been handled)
                wchar_t q[128] = L"";
                GetWindowTextW((HWND)lp, q, 128);
                if (HIWORD(wp) == EN_CHANGE) run_search(q);
                if (LOWORD(wp) == 2) {
                    const bool open = HIWORD(wp) != EN_KILLFOCUS && q[0] != 0 && GetFocus() == g.gsearch;
                    if (HIWORD(wp) == EN_SETFOCUS && q[0]) run_search(q);
                    if (open != g.drop_open) { g.drop_open = open; place_children(); }
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (LOWORD(wp) == 1 && HIWORD(wp) == EN_CHANGE) {
                // the clear button comes or goes with the first letter or the last: the edit's width changes with it
                // (placed again on every change: per window, and MoveWindow to the same place costs nothing)
                place_children();
                filter();
                if (!g.shown.empty() && std::find(g.shown.begin(), g.shown.end(), g.selected) == g.shown.end()) select_shown(0);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEMOVE: {
            g.mouse_x = GET_X_LPARAM(lp) / g.scale;
            g.mouse_y = GET_Y_LPARAM(lp) / g.scale;
            if (g_drag.pressed && g_drag.src == hwnd) {  // a tab is pressed: maybe a drag, which follows the pointer
                POINT scr;
                GetCursorPos(&scr);
                drag_move(scr, g.mouse_x, g.mouse_y);
                return 0;
            }
            if (g.scrubbing) {  // the timeline's playhead follows the pointer while the button is down
                scrub_to(g.mouse_x);
                return 0;
            }
            const int h = hit_at(g.mouse_x, g.mouse_y);
            std::wstring title, body;
            hover_card_text(h, title, body);
            // redraw when the spot changes, or while a hover card follows the pointer
            if (h != g.hover || !title.empty()) { g.hover = h; InvalidateRect(hwnd, nullptr, FALSE); }
            TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&t);
            return 0;
        }
        case WM_SETCURSOR:
            // Windows asks for the pointer before each mouse move reaches us, so the spot is looked up here rather
            // than taken from g.hover (one move behind). Windows only; Linux and macOS set a pointer shape per
            // window region in their own toolkit, with the same does_something() rule (likely).
            if ((HWND)wp == hwnd && LOWORD(lp) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                SetCursor(LoadCursor(nullptr, does_something(hit_at(pt.x / g.scale, pt.y / g.scale)) ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_MOUSELEAVE:
            if (g.hover != -1) { g.hover = -1; InvalidateRect(hwnd, nullptr, FALSE); }
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            if (view_mouse(hwnd, msg, wp, lp)) return 0;  // (over the 3D view in page mode: the model's own drag)
            const int h = hit_at(GET_X_LPARAM(lp) / g.scale, GET_Y_LPARAM(lp) / g.scale);
            if (h >= HIT_PTAB && h < HIT_PTAB + 100) {
                // a press on a tab: select / show it now (Ctrl, Shift as in Chrome), and maybe a drag follows; the
                // window keeps the mouse until the button is released, even outside it (SetCapture)
                g_drag.pressed = true;
                g_drag.src = hwnd;
                g_drag.press_tab = h - HIT_PTAB;
                GetCursorPos(&g_drag.press_pt);
                trail("click", hit_words(h));
                tab_press(h - HIT_PTAB, (wp & MK_CONTROL) != 0, (wp & MK_SHIFT) != 0);
                SetCapture(hwnd);
                return 0;
            }
            if (h == HIT_TIMELINE) {
                // a press on the timeline: hold the frame under it, and keep following the pointer until release
                // (SetCapture: even when it leaves the window). Windows mouse capture; other OSes: the toolkit's grab
                trail("drag", "timeline pressed");
                g.scrubbing = true;
                SetCapture(hwnd);
                scrub_to(GET_X_LPARAM(lp) / g.scale);
                return 0;
            }
            if (h >= 0) click(h);
            return 0;
        }
        case WM_LBUTTONUP:
            if (g.scrubbing) {
                g.scrubbing = false;
                trail("drag", "timeline released at frame " + std::to_string(g.viewer.once_frame()));
                ReleaseCapture();
                return 0;
            }
            if (g_drag.pressed && g_drag.src == hwnd) {
                POINT scr;
                GetCursorPos(&scr);
                ReleaseCapture();  // (WM_CAPTURECHANGED below sees the drag still pressed: it leaves it to this)
                drag_drop(scr);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if (g.scrubbing && (HWND)lp != hwnd) g.scrubbing = false;  // (the mouse was taken away mid-drag)
            // the mouse was taken away mid-drag (another window, a system dialog): the drag is cancelled
            if (g_drag.pressed && g_drag.src == hwnd && (HWND)lp != hwnd && GetKeyState(VK_LBUTTON) < 0) {
                drag_end();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        // a right click on a tab: its menu (multi-window plan step 2)
        case WM_RBUTTONUP: {
            const int h = hit_at(GET_X_LPARAM(lp) / g.scale, GET_Y_LPARAM(lp) / g.scale);
            if (h >= HIT_PTAB && h < HIT_PTAB + 100) tab_menu(h - HIT_PTAB);
            return 0;
        }
        // a middle click on a tab closes it, as in a browser
        case WM_MBUTTONDOWN: {
            const int h = hit_at(GET_X_LPARAM(lp) / g.scale, GET_Y_LPARAM(lp) / g.scale);
            if (h >= HIT_PTAB && h < HIT_PTAB + 100) click(HIT_PTAB_CLOSE + (h - HIT_PTAB));
            return 0;
        }
        // the wheel tilted left / right (WM_MOUSEHWHEEL; positive = right) scrolls the tab strip, wherever the pointer
        // is: nothing else in Studio scrolls sideways (snatcherLeo: "press in the left direction on their mouse scroll
        // wheel"). Windows Vista+; Linux / macOS: horizontal scroll events
        case WM_MOUSEHWHEEL:
            g.tab_scroll += GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * g.strip_step;  // (a notch: one tab; snapped when drawn)
            InvalidateRect(hwnd, nullptr, FALSE);
            return TRUE;
        case WM_MOUSEWHEEL: {
            if (view_mouse(hwnd, msg, wp, lp)) return 0;  // (over the 3D view in page mode: the model's zoom)
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &p);
            const Layout L = layout();
            if (p.y / g.scale < kTabBar && p.x / g.scale >= L.page.x) {
                // the wheel over the tab strip scrolls it sideways (only when the tabs don't all fit)
                g.tab_scroll -= GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * g.strip_step;  // (a notch: one tab; snapped when drawn)
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (L.list.in(p.x / g.scale, p.y / g.scale)) scroll_list(-GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * kRow * 3);
            else if (g.tab == kTabAnimations && L.mv.used.in(p.x / g.scale, p.y / g.scale) && [&] {
                         // the Used by card scrolls its own chips when they're more than three rows (else the page)
                         int rows = 0;
                         used_chips(L.mv.used.w - 32, &rows);
                         return rows > (int)kUsedRows;
                     }()) {
                g.used_scroll -= GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * (kChipH + kChipGap);  // (clamped when drawn)
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (L.page.in(p.x / g.scale, p.y / g.scale)) {
                // the page scrolls as a whole (the 3D view keeps its own wheel: zoom)
                g.pscroll = std::clamp(g.pscroll - GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * 60.0f, 0.0f, g.pscroll_max);
                place_children();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (key(wp)) return 0;
            break;
        // Alt+Left / Alt+Right come as system keys (Windows sends Alt combinations this way; Linux / macOS: the
        // toolkit's own shortcut events, Cmd+[ / Cmd+] on macOS by convention)
        case WM_SYSKEYDOWN:
            if (wp == VK_LEFT || wp == VK_RIGHT) { go(wp == VK_LEFT ? -1 : 1); return 0; }
            break;
        // The mouse's side buttons: DefWindowProc turns X button presses into WM_APPCOMMAND and passes it up from
        // child windows (the 3D view too), so this one case covers the whole window (Windows only; Linux: button
        // 8 / 9, macOS: NSEvent buttons 3 / 4)
        case WM_APPCOMMAND: {
            const int cmd = GET_APPCOMMAND_LPARAM(lp);
            if (cmd == APPCOMMAND_BROWSER_BACKWARD || cmd == APPCOMMAND_BROWSER_FORWARD) {
                go(cmd == APPCOMMAND_BROWSER_BACKWARD ? -1 : 1);
                return TRUE;
            }
            break;
        }
        case WM_SPIN_STOPPED:
            if (g.viewer.spin()) { g.viewer.set_spin(false); InvalidateRect(hwnd, nullptr, FALSE); }
            return 0;
        case WM_LIST_LOADED: {
            std::unique_ptr<ListResult> r(reinterpret_cast<ListResult*>(lp));
            g.loading = false;
            const bool stay = g.load_stays;  // (discs_scan: the first disc added while open; no jump to a Pokémon)
            g.load_stays = false;
            // a disc switched while Studio runs (Settings › Discs or the ROM folder's watch, 4.4): the old one stays open,
            // not closed, since a background read may still use it (closed at exit); its list's pictures are let go
            if (g.disc && wp && g.disc != reinterpret_cast<OrreDisc*>(wp)) {
                g_retired_discs.push_back(g.disc);
                for (auto& e : g.all) if (e.bmp) { e.bmp->Release(); e.bmp = nullptr; }
            }
            if (!wp && g.disc) {  // (a failed switch keeps the disc in use, and its list)
                logf(Level::Warn, "disc", "the new US disc couldn't be read (%s): Studio keeps the one in use", r->error.c_str());
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            g.disc = reinterpret_cast<OrreDisc*>(wp);
            g.load_error = r->error;
            if (!r->identity.empty()) {  // (also when it's the wrong disc: the line says which one it was)
                logf(Level::Info, "disc", "main disc: %s", r->identity.c_str());
                g.disc_lines.push_back("main disc: " + r->identity);
            }
            if (!g.disc) {
                logf(Level::Error, "load", "the disc couldn't be read: %s", r->error.c_str());
                reveal("the disc couldn't be read");  // its message page is all there is to show
            } else {
                g.all = std::move(r->entries);
                g.by_index.clear();
                for (int i = 0; i < (int)g.all.size(); ++i) g.by_index[g.all[i].index] = i;
                logf(Level::Info, "load", "%zu Pokémon with mug shots in %.0f ms", g.all.size(), r->ms);
                startup_mark("Pokémon list read");
                log_note_step("reading the Pokémon list", r->ms);
                filter();
                // (a disc added while open: the page shown stays, Home, Settings or an editor; the Pokémon editor opens
                // on its first Pokémon when its icon is clicked)
                const bool restored = !stay && restore_session();
                if (!restored && !stay) {  // (where you left off, else Bulbasaur or --select's Pokémon)
                    int start = 0;
                    for (int i = 0; i < (int)g.all.size(); ++i) if (g.all[i].index == g.select_at_start) start = i;
                    select(start);
                }
                if (!g.list_query.empty()) SetWindowTextW(g.search, g.list_query.c_str());  // --list-query (before --click)
                for (int id : g.test_clicks) { paint(); click(id); }  // --click (tests; painted between, as a user sees it)
                if (restored && cur_page() < 0) {
                    // the session's shown tab is Home, Settings or an editor: no model will arrive to show the window
                    // or take a screenshot, so both happen now (it waited for the 1 s limit before, 2026-11-22)
                    reveal("a page without a Pokémon");
                    if (!g.screenshot.empty() && !g.shot_pending) {
                        g.shot_pending = true;
                        SetTimer(hwnd, TIMER_SHOT, 700, nullptr);
                    }
                }
                if (g.form_at_start > 0 && g.form_at_start < (int)g.forms.size() && g.forms[(size_t)g.form_at_start].available)
                    click(HIT_FORM + g.form_at_start);  // --form N (screenshot tests)
                watch_check("first load");
            }
            place_children();
            InvalidateRect(hwnd, nullptr, FALSE);
            if (!g.viewer.ok()) reveal("no 3D view");  // (--no-3d, or a graphics card the 3D view can't use)
            // --tabs / --tab-select / --tab-move (screenshot tests of the multi-window plan's steps 3-5)
            if (g.disc && !g.test_tabs.empty()) {
                auto ints = [](const std::wstring& s) {
                    std::vector<int> v;
                    for (const wchar_t* p = s.c_str(); *p;) {
                        wchar_t* end;
                        const long n = std::wcstol(p, &end, 10);
                        if (end == p) { ++p; continue; }
                        v.push_back((int)n);
                        p = end;
                    }
                    return v;
                };
                for (int sp : ints(g.test_tabs)) {
                    if (sp == 0) { open_page(kHome, true); continue; }  // 0 (the disc's empty first slot): a Home tab
                    if (sp == kSettings) { open_page(kSettings, true); continue; }  // -2: a Settings tab
                    if (page_editor(sp)) { open_page(sp, true); continue; }        // -11 .. -17: an editor's tab
                    auto it = g.by_index.find(sp);
                    if (it != g.by_index.end()) select(it->second, true);
                }
                if (!g.test_tab_select.empty()) {
                    g.tab_sel.assign(g.page_tabs.size(), 0);
                    for (int t : ints(g.test_tab_select))
                        if (t >= 0 && t < (int)g.page_tabs.size()) g.tab_sel[(size_t)t] = 1;
                }
                const size_t gt = g.test_tab_move.find(L'>');
                if (gt != std::wstring::npos) {
                    std::vector<int> ts = ints(g.test_tab_move.substr(0, gt));
                    std::sort(ts.begin(), ts.end());
                    put_tabs(take_tabs(ts), _wtoi(g.test_tab_move.substr(gt + 1).c_str()));
                }
                place_children();
                if (g_drag_test >= 0) SetTimer(hwnd, TIMER_FLASH + 8, 900, nullptr);  // (--drag-test: its steps)
            }
            if (g_menu_test > 0 && g.disc) SetTimer(hwnd, TIMER_FLASH + 10, 1500, nullptr);  // (--menu-test)
            if (g_window_cycle > 0 && g.disc) SetTimer(hwnd, TIMER_FLASH + 12, 1500, nullptr);  // (--window-cycle)
            // --second-window N (screenshot tests of the multi-window plan): a second window with that species' tab
            if (g.second_window > 0 && g.disc && g_windows.size() == 1) {
                auto it = g.by_index.find(g.second_window);
                if (it != g.by_index.end()) {
                    NewWindowTabs nw;
                    nw.tabs = {it->second};
                    nw.hist = {{it->second}};
                    nw.pos = {0};
                    open_window(std::move(nw));
                    activate(hwnd);
                }
            }
            // windows opened while the disc was being read get their list now
            for (size_t k = 0; k < g_windows.size(); ++k) {
                const HWND other = g_windows[k].hwnd;
                if (other == hwnd || !g.disc) continue;
                activate(other);
                filter();
                place_children();
                InvalidateRect(other, nullptr, FALSE);
            }
            activate(hwnd);
            return 0;
        }
        case WM_MOVE_ENDED:
            playhead_timer();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_VIEW_MOVED:  // the model turned, zoomed or reset: the header's ↺ follows (lit or dimmed)
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_VIEW_SHARED:  // a new 3D frame in the shared texture: stacked on the saved page and shown (compose)
            if (GetUpdateRect(hwnd, nullptr, FALSE)) return 0;  // (a whole paint is coming anyway, with this frame)
            compose();
            return 0;
        case WM_VIEW_FRAMED:
            place_children();  // reveals the 3D view now that it has a real picture
            reveal("the first full page");  // at launch: the page and its model are both drawn now
            if (g.flash_test > 0 && !g_flash_thread.joinable()) {
                // ORRE_FLASH_MAX=1: maximized first (snatcherLeo's own way of using Studio, likely), measured 600 ms later
                if (std::getenv("ORRE_FLASH_MAX") && !IsZoomed(hwnd)) { ShowWindow(hwnd, SW_MAXIMIZE); SetTimer(hwnd, TIMER_FLASH + 2, 600, nullptr); }
                else flash_test_start(hwnd);
            }
            return 0;
        case WM_DISC_OP_DONE:
            discs_done(reinterpret_cast<DiscOpResult*>(lp));
            return 0;
        case WM_DISCS_READ:
            discs_read_done(reinterpret_cast<DiscsRead*>(lp));
            return 0;
        case WM_MODEL_LOADED: {
            std::unique_ptr<ModelResult> r(reinterpret_cast<ModelResult*>(lp));
            if (r->generation != g.model_generation) { orre_model_free(&r->model); return 0; }  // an older request
            g.model_loading = false;
            if (r->ok) {
                int tris = 0, parts = 0;
                for (int i = 0; i < r->model.mesh_count; ++i)
                    if (!r->model.meshes[i].hidden) { tris += r->model.meshes[i].vertex_count / 3; ++parts; }
                g.model_info = tx("studio.model.info", {{"triangles", std::to_wstring(tris)}, {"parts", std::to_wstring(parts)},
                                                        {"textures", std::to_wstring(r->model.texture_count)}});
                logf(Level::Info, "model", "%s: %d triangles, %d parts, %d textures, read in %.1f ms", mon_name().c_str(), tris, parts,
                     r->model.texture_count, r->ms);
                log_note_step((mon_name() + "'s model").c_str(), r->ms);
                startup_mark("first model read");  // (only until the first page shows)
                g.viewer.set_model(&r->model);
                if (g.has_yaw) g.viewer.set_yaw(g.yaw);
                if (!g.game_camera.empty()) apply_game_camera(g.game_camera);
                sync_preview();  // the Moves tab: the new model's animation for the chosen move, Plain stage
                if (g.test_play && g.tab == kTabMoves) click(HIT_PLAY);  // --play (screenshot tests)
                // --click-after-model N,M (tests): clicks that need the model, once its first model has arrived (the
                // Animations tab's link reads which animation the move plays, 2026-11-22)
                if (!g.test_clicks_model.empty()) next_test_click(hwnd);
                if (g.flip_turns > 0 && g.flips_done == 0) SetTimer(hwnd, TIMER_FLASH + 6, (UINT)g.flip_turns, nullptr);
            } else {
                g.model_info = tx("studio.model.missing");
                reveal("a Pokémon without a model");  // no 3D frame will come
                logf(Level::Warn, "model", "no model for %s (species %d, form %d, model id 0x%X)", mon_name().c_str(),
                     g.selected >= 0 ? g.all[g.selected].index : -1, g.form, g.selected >= 0 ? (unsigned)g.all[g.selected].s.model_id : 0u);
            }
            watch_check((mon_name() + "'s model").c_str());
            place_children();
            InvalidateRect(hwnd, nullptr, FALSE);
            // (with --second-window, the shot waits for the second window's model: that window is the one captured)
            const bool shot_here = g.second_window <= 0 || (g_windows.size() > 1 && hwnd != g_windows.front().hwnd);
            if (!g.screenshot.empty() && !g.shot_pending && shot_here) {
                // --search-query / --palette (screenshot tests of the global search): type the query into the top
                // field (its drop-down) or the palette's
                if (!g.test_query.empty()) {
                    if (g.test_palette) open_palette(); else SetFocus(g.gsearch);
                    SetWindowTextW(g.test_palette ? g.psearch : g.gsearch, g.test_query.c_str());
                }
                g.shot_pending = true;
                SetTimer(hwnd, TIMER_SHOT, 700, nullptr);
            }
            return 0;
        }
        case WM_TIMER:
            if (wp == TIMER_FLASH) {
                flash_test_tick(hwnd);
            } else if (wp == TIMER_FLASH + 2) {  // the flash test, maximized: start once the layout settled
                KillTimer(hwnd, TIMER_FLASH + 2);
                flash_test_start(hwnd);
            } else if (wp == TIMER_FLASH + 8) {  // --drag-test: the next step
                drag_test_step(hwnd);
            } else if (wp == TIMER_FLASH + 12) {  // --window-cycle: open or close the second window
                if (g_cycle_win && IsWindow(g_cycle_win)) {
                    PostMessageW(g_cycle_win, WM_CLOSE, 0, 0);
                    g_cycle_win = nullptr;
                    ++g_window_cycles_done;
                } else if (g_window_cycles_done >= g_window_cycle) {
                    KillTimer(hwnd, TIMER_FLASH + 12);
                    watch_check("window cycles done");
                    logf(Level::Info, "test", "window cycle: %d windows opened and closed", g_window_cycles_done);
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                } else {
                    if (g_window_cycles_done > 0) watch_check("a window closed (cycle)");
                    g_cycle_main = hwnd;
                    NewWindowTabs nw;
                    const int sp = g.selected >= 0 ? g.selected : 0;
                    nw.tabs = {sp};
                    nw.hist = {{sp}};
                    nw.pos = {0};
                    g_cycle_win = open_window(std::move(nw));
                    activate(hwnd);
                }
            } else if (wp == TIMER_FLASH + 10) {  // --menu-test: open the menu (the picture's timer runs inside it)
                KillTimer(hwnd, TIMER_FLASH + 10);
                SetTimer(hwnd, TIMER_FLASH + 11, 700, nullptr);
                click(g_menu_test);
            } else if (wp == TIMER_FLASH + 11) {  // --menu-test: the picture, then close the menu and Studio
                KillTimer(hwnd, TIMER_FLASH + 11);
                g_drag_main = hwnd;
                drag_test_shot("menu_test.png");
                EndMenu();
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            } else if (wp == TIMER_FLASH + 1) {  // the flash test's button release
                KillTimer(hwnd, TIMER_FLASH + 1);
                INPUT in{};
                in.type = INPUT_MOUSE;
                in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &in, sizeof(INPUT));
            } else if (wp == TIMER_FLASH + 6) {  // --flip-turns: the next Turn 1 / Turn 2 click, then close after 20
                if (++g.flips_done > 20) {
                    KillTimer(hwnd, TIMER_FLASH + 6);
                    logf(Level::Info, "test", "flip-turns: 20 switches done");
                    DestroyWindow(hwnd);
                } else {
                    click(g.turn == 1 ? HIT_TURN2 : HIT_TURN1);
                }
            } else if (wp == TIMER_REVEAL) {
                reveal("the 1 s limit");  // whatever page there is (a slow first read after a reboot, a slow drive)
            } else if (wp == TIMER_TOAST) {
                KillTimer(hwnd, TIMER_TOAST);
                g.toast.clear();
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (wp == TIMER_DISCOP) {  // Settings › Discs' copy or move: its percentage
                repaint_all();
            } else if (wp == TIMER_TABGROW) {  // a new tab growing in: redraw until it's full width
                InvalidateRect(hwnd, nullptr, FALSE);
                if (g.tab_born < 0 || GetTickCount64() - g.tab_born_at > kTabGrowMs + 30) KillTimer(hwnd, TIMER_TABGROW);
            } else if (wp == TIMER_PLAYHEAD) {
                InvalidateRect(hwnd, nullptr, FALSE);  // (the timeline's playhead follows the playing animation)
                playhead_timer();
            } else if (wp == TIMER_FLASH + 7) {  // --click-after-model's pause ended: the clicks after it
                KillTimer(hwnd, TIMER_FLASH + 7);
                if (!g.test_clicks_model.empty()) g.test_clicks_model.erase(g.test_clicks_model.begin());
                next_test_click(hwnd);
            } else if (wp == TIMER_SHOT) {
                KillTimer(hwnd, TIMER_SHOT);
                if (!g.test_clicks_model.empty()) {  // (a test's pauses still running: the shot comes 700 ms after them)
                    SetTimer(hwnd, TIMER_SHOT, 200, nullptr);
                    return 0;
                }
                if (g_test_hover >= 0) g.hover = g_test_hover;  // --hover <id> (tests: a spot drawn as hovered)
                paint();
                g.viewer.walk_into_hold(30);  // (a screen blur's trails need the frames before: Viewer::walk_into_hold)
                g.viewer.render();
                // page mode: the held frame is stacked on the page now, and on screen before the capture (PrintWindow
                // takes the composed picture: DwmFlush waits for Windows to compose it)
                if (page_view() && g.viewer.shared_frames() > 0) { compose(); DwmFlush(); }
                capture(g.screenshot);
                // a screenshot test ends Studio, every window
                for (size_t k = g_windows.size(); k-- > 1;) DestroyWindow(g_windows[k].hwnd);
                DestroyWindow(g_windows.empty() ? hwnd : g_windows.front().hwnd);
            }
            return 0;
        case WM_SETTINGCHANGE:
            if (lp && std::wstring(reinterpret_cast<const wchar_t*>(lp)) == L"ImmersiveColorSet") apply_theme();
            return 0;
        case WM_ACTIVATEAPP:
            if (wp) {  // back in front: OrrePort's hub may have changed the shared Theme setting meanwhile (it matters
                       // only while no theme is picked in Studio's own Settings)
                const int before = g.theme_setting;
                const bool check_before = g.update_check, auto_before = g.update_auto;
                read_shared_settings();
                if (before != g.theme_setting && g.studio_theme < 0) apply_theme_all();
                // (the update switches may have changed in the hub too: one setting for both programs)
                if (check_before != g.update_check || auto_before != g.update_auto) { updates_settings_changed(); repaint_all(); }
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize = {(LONG)px(900), (LONG)px(600)};
            return 0;
        }
        case WM_DESTROY: {
            // one window closes: its own resources go; Studio quits with the last window (as a browser does)
            if (g_windows.size() > 1) {
                release_target();
                release_pics();
                for (auto& f : g.forms) if (f.bmp) f.bmp->Release();
                g.forms.clear();
                g.viewer.set_model(nullptr);  // its model's GPU buffers (the shared device stays)
                // and the view itself, and the textures it shared frames with the page through (2026-11-22: neither
                // was freed, so every closed window kept about 12 MB; the --window-cycle test measures it)
                release_shared();
                g.viewer.destroy();
                g_windows_closing_no = g.win_no;  // (named in the closing line below, said by the window that remains)
                size_t at = 0;
                while (at < g_windows.size() && g_windows[at].hwnd != hwnd) ++at;
                if (at < g_windows.size()) g_windows.erase(g_windows.begin() + (long)at);
                // another window becomes the one g describes; this window's state is dropped
                auto& next = g_windows.front();
                std::swap(static_cast<WinState&>(g), next.parked);
                next.parked = WinState{};
                next.active = true;
                const int closed_no = g_windows_closing_no;
                log_set_window(g.win_no);  // (the lines from here on are the remaining window's)
                // (the hub finds Studio through its published window: the remaining one takes over)
                if (!g_apps_root.empty() && !(g.select_given || !g.screenshot.empty()))
                    pc_apps_set_window(PC_APP_STUDIO, g_apps_root.c_str(), g_windows.front().hwnd);
                logf(Level::Info, "window", "w%d closed (%zu open)", closed_no, g_windows.size());
                return 0;
            }
            flash_test_finish(hwnd);  // (a flash watch still running writes its results now)
            watch_check("closing");
            save_session();  // (where you were, for the next start)
            logf(Level::Info, "app", "closed");
            log_exit(1, g.page_tabs.size());  // (the clean-exit line: its absence marks a crash at the next start)
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

std::wstring exe_folder() {
    // grown until the whole path fits (GetModuleFileNameW truncates silently at the buffer's size): the portable
    // folder is about 20 characters deeper than the development one, and long paths pass 260 (Windows only; Linux:
    // readlink("/proc/self/exe"), macOS: _NSGetExecutablePath)
    std::wstring p(512, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, p.data(), (DWORD)p.size());
        if (n < p.size()) { p.resize(n); break; }
        p.resize(p.size() * 2);
    }
    return p.substr(0, p.find_last_of(L"\\/"));
}

// R\ when the exe's folder is named "app" (any capitals; the same rule as OrrePort's pc_paths.c), else empty.
// *Changed 2026-11-22* (layout A, snatcherLeo: "We will build A"): it was "bin"; no fallback to it, as no tester folder has one
// and snatcherLeo's folder is renamed in one step (the project's call)
std::wstring portable_root(const std::wstring& exe_dir) {
    const size_t slash = exe_dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos || slash == 0) return {};
    if (_wcsicmp(exe_dir.c_str() + slash + 1, L"app") != 0) return {};
    return exe_dir.substr(0, slash);
}

bool exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

// The disc in the game's ROM folder (R\Pokemon Colosseum\ROM\ portable, <OrrePort>\Colosseum\ROM\ in development):
// the first file whose game id is GC6E01, as OrrePort's pc_platform_disc_scan checks (a European or Japanese disc may
// sit there too, as a language disc; files ending ".part" are the hub's copy in progress). Each file is opened
// read-only through the core (OrrePort may have it open at the same time).
std::wstring find_rom_disc_id(const std::wstring& rom, const char* want);
std::wstring find_rom_disc(const std::wstring& rom) { return find_rom_disc_id(rom, "GC6E01"); }
// the disc of game id `want` in a folder (GC6E01 in ROM\; GC6P01 / GC6J01 in ROM Other Regions\), by the shared
// "which disc" rule (orre_disc_find, 2026-11-23: the newest when there are two; before, the first by name)
std::wstring find_rom_disc_id(const std::wstring& rom, const char* want) {
    char out[2048];
    const int n = orre_disc_find(narrow(rom).c_str(), want, out, sizeof out);
    if (n > 1)
        logf(Level::Info, "disc", "%s folder: %d discs are %s; the newest is used (%s)", narrow(rom.substr(rom.find_last_of(L"\\/") + 1)).c_str(),
             n, want, std::strrchr(out, '\\') ? std::strrchr(out, '\\') + 1 : out);
    return n ? widen(out) : std::wstring();
}

// the first folder in the list that holds `probe`
std::wstring find_folder(std::initializer_list<std::wstring> candidates, const wchar_t* probe) {
    for (const auto& c : candidates)
        if (!c.empty() && exists(c + L"\\" + probe)) return c;
    return {};
}

}  // namespace

void page_frame_stats(int& frames, double& avg_ms, double& worst_ms) { page_stats_take(frames, avg_ms, worst_ms); }

int run(HINSTANCE inst, int argc, wchar_t** argv) {
    pc_crash_install();  // (OrrePort's crash handler: the call stack to stderr on a crash; see CMakeLists.txt)
    g.inst = inst;
    g.exe_dir = exe_folder();
    std::wstring size_arg, fonts_arg;
    bool detail = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::wstring(argv[++i]) : std::wstring(); };
        if (a == L"--disc") g.disc_path = next();
        else if (a == L"--orreport") g.orreport_dir = next();
        else if (a == L"--fonts") fonts_arg = next();
        else if (a == L"--theme") { const auto t = next(); g.theme_forced = t == L"light" ? 1 : t == L"dark" ? 2 : 0; }
        else if (a == L"--lang") g.lang_code = narrow(next());
        else if (a == L"--select") { g.select_at_start = _wtoi(next().c_str()); g.select_given = true; }
        else if (a == L"--no-session") g.select_given = true;  // (start fresh: the last session is neither restored nor read)
        else if (a == L"--restore-session") g.test_session = true;  // (a screenshot run that restores it, to test it)
        else if (a == L"--region-disc") g.region_disc_arg = next();  // (the Japanese disc for its styles: tests)
        else if (a == L"--style") g.style_of[124] = _wtoi(next().c_str());  // (Jynx's style viewed: screenshot tests)
        else if (a == L"--click-after-model") {  // (HIT_ ids clicked once the first model has arrived)
            std::wstringstream ss(next());
            std::wstring item;
            // "w1900": a pause of 1,900 ms before the next click (kept as -1900), so a test can wait as a user does
            // (a move still playing when the tab is switched, 2026-11-22)
            while (std::getline(ss, item, L','))
                if (!item.empty() && item[0] == L'w') g.test_clicks_model.push_back(-std::max(1, _wtoi(item.c_str() + 1)));
                else if (_wtoi(item.c_str()) > 0) g.test_clicks_model.push_back(_wtoi(item.c_str()));
        }
        else if (a == L"--click") {  // (HIT_ ids clicked in order once the list is read: "7200,100")
            std::wstringstream ss(next());
            std::wstring item;
            while (std::getline(ss, item, L',')) if (_wtoi(item.c_str()) > 0) g.test_clicks.push_back(_wtoi(item.c_str()));
        }
        else if (a == L"--form") g.form_at_start = _wtoi(next().c_str());
        else if (a == L"--screenshot") g.screenshot = next();
        else if (a == L"--size") size_arg = next();
        else if (a == L"--log-detail") detail = true;
        else if (a == L"--no-3d") g.no_3d = true;
        else if (a == L"--shot-model") g.shot_model = true;
        else if (a == L"--hold-frame") g.hold_frame = _wtoi(next().c_str());
        else if (a == L"--yaw") { g.yaw = (float)_wtof(next().c_str()); g.has_yaw = true; }
        else if (a == L"--game-camera") g.game_camera = next();
        else if (a == L"--shot-camera") g.shot_camera = next();
        else if (a == L"--turn") g.turn = std::wstring(next()) == L"1" ? 1 : 2;  // (a two-turn move's turn, for tests)
        else if (a == L"--parts") g.parts = std::clamp(_wtoi(next().c_str()), 0, 2);  // (the parts picker, for tests)
        else if (a == L"--flash-test") g.flash_test = _wtoi(next().c_str());  // (measures the 3D view's flash; see above)
        else if (a == L"--flash-watch") { g.flash_watch = _wtoi(next().c_str()); g.flash_test = 1; }  // (snatcherLeo clicks)
        else if (a == L"--search-query") g.test_query = next();
        else if (a == L"--list-query") g.list_query = next();  // (the list's own search box: its clear button's tests)
        else if (a == L"--palette") g.test_palette = true;
        else if (a == L"--second-window") g.second_window = _wtoi(next().c_str());
        else if (a == L"--tabs") g.test_tabs = next();
        else if (a == L"--tab-select") g.test_tab_select = next();
        else if (a == L"--tab-move") g.test_tab_move = next();
        else if (a == L"--tab") g.tab = std::clamp(_wtoi(next().c_str()), 0, kTabCount - 1);  // screenshot tests
        // the Moves tab's screenshot tests: the learnset group (0 level-up, 1 TM / HM, 2 egg), the chosen move (its id),
        // Plain stage, and Play once the model is in (with --hold-frame N: the move held on frame N)
        else if (a == L"--learn-group") g.learn_group = std::clamp(_wtoi(next().c_str()), 0, 3);
        else if (a == L"--learn-move") g.learn_move = _wtoi(next().c_str());
        else if (a == L"--plain") g.plain = true;
        else if (a == L"--arena") g.plain = g.arena = true;  // (the Arena stage: screenshot tests, the move sheets)
        else if (a == L"--ov-stage") g.ov_stage = std::clamp(_wtoi(next().c_str()), 0, 2);  // (the Overview's stage: tests)
        else if (a == L"--quiet-window") g.quiet_window = true;
        else if (a == L"--play") g.test_play = true;
        else if (a == L"--target") g.target_species = _wtoi(next().c_str());
        else if (a == L"--any-move") g.any_move = _wtoi(next().c_str());
        else if (a == L"--no-particles") g.particles_on = false;  // the Particles switch off (screenshot tests)
        else if (a == L"--no-battle-camera") g.battle_cam = false;  // Battle camera off: the stage's wide view (tests)
        else if (a == L"--hang-test") g_hang_test = true;
        else if (a == L"--pick-disc") g_test_picks.push_back(next());
        else if (a == L"--hover") g_test_hover = _wtoi(next().c_str());  // (Settings › Discs' tests: the file "picked")  // (the hang watch's test: a 12 s stall, 3 s in)
        else if (a == L"--window-cycle") g_window_cycle = std::max(1, _wtoi(next().c_str()));  // (see g_window_cycle)
        else if (a == L"--menu-test") g_menu_test = _wtoi(next().c_str());  // (a native menu photographed: see g_menu_test)
        else if (a == L"--drag-test") g_drag_test = std::max(0, _wtoi(next().c_str()));  // (a tab drag played out: see drag_test_step)
        else if (a == L"--flip-turns") g.flip_turns = std::max(1, _wtoi(next().c_str()));  // (see Globals::flip_turns)
        // --crash-test: crash on purpose after the log is open, to check the crash report reaches the log (log_open)
        else if (a == L"--crash-test") g.crash_test = true;
        else if (a == L"--shiny") g.shiny = true;                 // Shiny picked (screenshot tests)
        else if (a == L"--rate") g.preview_rate = std::max(0, _wtoi(next().c_str()));  // the preview rate (tests)
        else if (a == L"--game-lighting") g.game_light = true;    // Game lighting on (screenshot tests)
        else if (a == L"--studio-lighting") g.game_light = false; // Game lighting off: Studio's even light (screenshot tests)
        else if (a == L"--era") g.era = std::clamp(_wtoi(next().c_str()), 0, kEraGens);  // how a picked generation looks
    }
    // Where things are (step 4.1, docs/port/portable-layout.md part 1d; the project's six calls, the project plan section 8):
    // - portable layout (R\app\orrestudio.exe, started by R\OrrePort Studio.exe with R\ as the working folder):
    //   pictures app\UI\Studio\, words app\UI\lang\Studio\ (app\UI\lang\ holds OrrePort's own en.txt), fonts app\Fonts\,
    //   shared settings R\data\settings\settings.ini, Studio's R\data\settings\studio.ini (and studio-session.ini), logs
    //   R\data\system\logs\ (shared with OrrePort's run logs: each program prunes only its own names), the disc
    //   R\Pokemon Colosseum\ROM\. R\Studio\ is the player's Studio work (Source games\ in 0.1) and is never written by
    //   updates; the search index and caches will go in R\data\system\ when they exist.
    //   *Changed 2026-11-22, twice* (snatcherLeo, through Main): first "Yes, lowercase them" (Settings\ and System\ became
    //   settings\ and system\), then layout A, "We will build A": bin\ became app\, and settings\ and system\ moved
    //   into one data\ folder. The rule: capitalized top folders are the player's (Pokemon Colosseum, later Mods,
    //   OrreBank, Translations in progress), lowercase ones are OrrePort's own (app, data). Linux / macOS are
    //   case-sensitive, so their ports use exactly these names (likely).
    // - development layout (build-pc\studio): as before; run_studio.ps1 passes --orreport and --fonts.
    g.root_dir = portable_root(g.exe_dir);
    g.settings_dir = g.root_dir.empty() ? g.exe_dir + L"\\user" : g.root_dir + L"\\data\\settings";
    g.logs_dir = g.root_dir.empty() ? g.exe_dir + L"\\logs" : g.root_dir + L"\\data\\system\\logs";
    if (!g.root_dir.empty()) {
        // (one level at a time: CreateDirectoryW makes only the last folder of a path)
        CreateDirectoryW((g.root_dir + L"\\data").c_str(), nullptr);
        CreateDirectoryW((g.root_dir + L"\\data\\system").c_str(), nullptr);
    }
    log_open(g.logs_dir, detail);
    startup_mark("log open");
    // a crash's minidump beside this run's log, studio-<stamp>.dmp (4.3 part 3, the logging list's item 10; the project's
    // pc_crash, 29bfd26e2: the threads, their stacks and the memory they point at, opened in WinDbg or Visual Studio
    // with this build's symbols). Removed with its log by the rotation. Windows only (dbghelp); a no-op elsewhere
    if (const std::wstring lp = log_file_path(); lp.size() > 4) pc_crash_set_dump_path(narrow(lp.substr(0, lp.size() - 4) + L".dmp").c_str());
    // the previous run ended unexpectedly: say so in this log and offer a report (not in test runs, which start and stop
    // Studio by the dozen)
    if (const std::wstring prev = previous_run_unclean(); !prev.empty()) {
        logf(Level::Warn, "app", "the previous run did not end cleanly (%s)", narrow(prev.substr(prev.find_last_of(L"\\/") + 1)).c_str());
        // *Off 2026-11-22 (snatcherLeo: "we can comment that code out for now, maybe later we'll need it"); back on 2026-11-23
        // (snatcherLeo: "Yes, turn it on", once OrrePort got the same bar, 8ef34d4b8, in the same shared words):* the bar under
        // the tab bar, Make a report (whose zip carries the crashed run's log as log/previous/) or Not now. Never in test
        // runs (STUDIO_TEST_CRASH_BAR shows it for a screenshot test)
        constexpr bool kCrashBar = true;
        g.crash_bar = kCrashBar && ((!g.select_given && g.screenshot.empty()) || std::getenv("STUDIO_TEST_CRASH_BAR"));
    }
    {   // the machine and its screens, line by line (4.3 part 3, items 2 and 5; diag/sysinfo.h)
        char buf[4096];
        for (int part = 0; part < 2; ++part) {
            if (part == 0) diag_sysinfo(buf, sizeof buf);
            else diag_displays(buf, sizeof buf);
            for (char *p = buf, *nl; *p; p = nl + 1) {
                nl = std::strchr(p, '\n');
                if (!nl) { logf(Level::Info, "system", "%s", p); break; }
                *nl = 0;
                logf(Level::Info, "system", "%s", p);
            }
        }
    }
    trail_set_sink(log_trail_line);       // (each action as a "[do]" line in the log)
    pc_crash_report_hook = crash_trail;   // (a crash report ends with the last 50 actions)
    SetTimer(nullptr, 0, 1000, perf_timer);  // (the performance log, one row a second)
    if (g.crash_test) {
        trail("test", "about to crash on purpose");
        logf(Level::Info, "test", "crash test: reading a null pointer now");
        volatile int* volatile p = nullptr;
        g.flips_done = *p;  // (deliberate)
    }
    logf(Level::Info, "app", "OrrePort Studio starting (%s layout)", g.root_dir.empty() ? "development" : "portable");
    if (g.orreport_dir.empty()) g.orreport_dir = g.root_dir.empty() ? g.exe_dir + L"\\.." : g.root_dir;
    g.orreport_root = !g.root_dir.empty() ? g.root_dir
                    : exists(g.orreport_dir + L"\\app") || exists(g.orreport_dir + L"\\data\\settings\\settings.ini") ? g.orreport_dir
                    : std::wstring();
    g.ui_dir = find_folder({g.exe_dir + L"\\UI\\Studio", g.exe_dir + L"\\ui"}, L"icons");
    g.lang_dir = find_folder({g.exe_dir + L"\\UI\\lang\\Studio", g.exe_dir + L"\\lang"}, L"en.txt");
    g.fonts_dir = find_folder({fonts_arg, g.exe_dir + L"\\Fonts", g.orreport_dir + L"\\Fonts", g.orreport_dir + L"\\app\\Fonts"}, L"Inter-Variable.ttf");
    read_shared_settings();
    lang_load(g.lang_dir, g.lang_code);
    lang_load_shared(g.exe_dir + L"\\UI\\lang", g.lang_code);  // (OrrePort's disc messages, shared word for word)
    {
        // Studio's own preferences (studio.ini): the theme picked in Settings. Screenshot runs leave it out, so
        // the tests look the same whatever the player picked.
        auto m = read_ini(studio_ini());
        g.disc_pref = m.count("disc") ? widen(m["disc"]) : L"";  // (kept as written: relative or full)
        if (g.screenshot.empty() && m.count("theme")) {
            const std::string t = m["theme"];
            g.studio_theme = t == "windows" ? 0 : t == "light" ? 1 : t == "dark" ? 2 : -1;
        }
        if (g.screenshot.empty() && m.count("particles")) g.particles_on = m["particles"] != "off";
        if (g.screenshot.empty() && m.count("lighting")) g.game_light = m["lighting"] != "studio";
        if (g.screenshot.empty() && m.count("battle_camera")) g.battle_cam = m["battle_camera"] != "off";
        if (g.screenshot.empty() && m.count("target")) g.target_species = std::atoi(m["target"].c_str());
        if (g.screenshot.empty() && m.count("target_recent")) {
            std::stringstream ss(m["target_recent"]);
            std::string item;
            while (std::getline(ss, item, ',') && g.target_recent.size() < 5)
                if (!item.empty()) g.target_recent.push_back(std::atoi(item.c_str()));
        }
        if (g.screenshot.empty() && m.count("rate")) {
            const int r = std::atoi(m["rate"].c_str());
            for (int k : kRates) if (k == r) g.preview_rate = r;  // (only the listed rates)
        }
    }
    if (g.disc_path.empty()) {
        wchar_t env[1024];
        if (GetEnvironmentVariableW(L"ORRE_DISC", env, 1024)) g.disc_path = env;
    }
    // *Changed 2026-11-23 (4.4, decided by snatcherLeo: "I agree with the project's suggestion"):* ROM\ is the one place for both
    // apps' discs, so the disc studio.ini remembers is used only without an OrrePort folder (the older development
    // layout); --disc and ORRE_DISC stay for tests. A disc given those ways isn't switched by the ROM folder's watch.
    g.disc_forced = !g.disc_path.empty();
    if (g.disc_path.empty() && g.orreport_root.empty() && !g.disc_pref.empty()) {
        const std::wstring d = disc_from_ini(g.disc_pref);
        if (exists(d)) { g.disc_path = d; g.disc_forced = true; }
    }
    // Settings › Discs' rows and the watch's starting point (4.4): the folders as they are now, nothing acted on; the
    // US disc OrrePort itself uses comes from the same reading (the US game id and the original program file)
    g_discs.sig = discs_signature();
    discs_scan(false);
    if (g.disc_path.empty()) g.disc_path = g_discs.path[0];
    {
        const size_t slash = g.disc_path.find_last_of(L"\\/");
        logf(Level::Info, "app", "OrrePort's folder: %s", g.orreport_root.empty() ? "the older layout (user\\settings.ini, Colosseum\\ROM)" : "portable (data\\settings\\settings.ini, Pokemon Colosseum\\ROM)");
        logf(Level::Info, "app", "disc: %s; fonts: %s; language: %s", g.disc_path.empty() ? "(none yet)" : narrow(g.disc_path.substr(slash + 1)).c_str(),
             g.fonts_dir.empty() ? "not found" : "found", g.lang_code.c_str());
    }
    prefs_log(true);  // (every preference in effect; later changes log as old -> new)
    startup_mark("settings read");
    updates_start();  // (4.4: the shared updater; its check runs on its own thread)
    // One Studio per OrrePort folder (4.4, snatcherLeo's "apps connecting" design: starting it again brings the open one
    // forward): claimed here, before any window; a second copy shows the first and ends. Test runs don't claim (they
    // start and stop by the dozen, often while snatcherLeo has Studio open) but still see the hub. The folder is the one
    // holding OrrePort.exe and "OrrePort Studio.exe" (the portable root; --orreport's folder in development), the same
    // one the hub passes, so both programs' names match (pc_apps hashes it)
    if (!g.orreport_root.empty()) {
        // (the full path, so "..", "." or a different slash in --orreport still names the same folder the hub names;
        // pc_apps lower-cases it and drops trailing slashes itself. Windows: GetFullPathNameW; Linux / macOS: realpath)
        wchar_t full[2048];
        const DWORD n = GetFullPathNameW(g.orreport_root.c_str(), 2048, full, nullptr);
        g_apps_root = narrow(n > 0 && n < 2048 ? std::wstring(full) : g.orreport_root);
        const bool test_run = g.select_given || !g.screenshot.empty();
        if (!test_run && !pc_apps_claim(PC_APP_STUDIO, g_apps_root.c_str())) {
            logf(Level::Info, "apps", "Studio is already open from this OrrePort folder: brought it forward, this copy ends");
            pc_apps_show(PC_APP_STUDIO, g_apps_root.c_str());
            if (g_upd_on) pc_update_shutdown();
            log_exit(0, 0);  // (its short log ends cleanly too, or the next start would take it for a crash)
            return 0;
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr, reinterpret_cast<void**>(&g.d2d));
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), reinterpret_cast<IUnknown**>(&g.dw));
    if (!g.d2d || !g.dw) {
        logf(Level::Error, "app", "Direct2D / DirectWrite unavailable (Windows 10 1703 or later is needed)");
        return 1;
    }
    load_fonts();
    startup_mark("fonts loaded");
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"OrrePortStudio";
    RegisterClassExW(&wc);
    // the window: 1360 x 840 DIPs on the monitor under the cursor, or --size W x H (pixels at 100%)
    POINT cur;
    GetCursorPos(&cur);
    HMONITOR mon = MonitorFromPoint(cur, MONITOR_DEFAULTTOPRIMARY);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(mon, &mi);
    int w = MulDiv(1360, dx, 96), h = MulDiv(840, dx, 96);
    if (!size_arg.empty()) {
        int sw = 0, sh = 0;
        if (swscanf(size_arg.c_str(), L"%dx%d", &sw, &sh) == 2) { w = MulDiv(sw, dx, 96); h = MulDiv(sh, dx, 96); }
    }
    const int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2, y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
    const std::wstring title = tx("studio.app.title");
    // the last session (save_session): read now, so the window opens where it was; its tabs follow once the list is read
    if ((g.screenshot.empty() || g.test_session) && !g.select_given && !g.disc_path.empty()) g.session = read_ini(session_ini());
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;
    // (the window the hub finds to show or close Studio: this first one; when it closes and others remain, the next one)
    if (!g_apps_root.empty() && !(g.select_given || !g.screenshot.empty())) pc_apps_set_window(PC_APP_STUDIO, g_apps_root.c_str(), hwnd);
    bool maximized = false;
    if (size_arg.empty()) session_window(hwnd, maximized);
    // cloaked until its first full page (reveal(); not for screenshot runs, nor when no disc is known: the choose-a-
    // disc page has nothing to wait for)
    if (g.screenshot.empty() && !g.disc_path.empty()) {
        BOOL on = TRUE;
        if (SUCCEEDED(DwmSetWindowAttribute(hwnd, kDwmCloak, &on, sizeof on))) {
            g.cloaked = true;
            SetTimer(hwnd, TIMER_REVEAL, 1000, nullptr);
        }
    }
    // --quiet-window (screenshot runs; snatcherLeo, 2026-11-19, seeing a test window pop up: "Is this just showing because
    // you're testing something specific?"): the window is all but transparent and kept off the taskbar, so the move
    // sweeps' hundreds of short runs don't flash across his screen; PrintWindow still captures its drawn content.
    // Windows only (layered windows); Linux / macOS: an offscreen or hidden test window
    // *Corrected the same night:* an all but transparent window still left faint ghost traces on snatcherLeo's monitor
    // ("It loks weird"); the window now sits just past the right edge of all his screens instead (off the taskbar),
    // where nothing shows, and PrintWindow still captures it
    if (g.quiet_window && !g.screenshot.empty()) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
        const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        SetWindowPos(hwnd, nullptr, vx + 64, GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ShowWindow(hwnd, !g.screenshot.empty() ? SW_SHOWNOACTIVATE : maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    start_loading();
    if (g.disc_path.empty()) InvalidateRect(hwnd, nullptr, FALSE);
    // a screenshot test with no disc (2026-11-24, Home's no-disc strip): no list or model will arrive, so its
    // --click-after-model steps (pauses included) run now and the shot follows them; a disc added by those steps is read
    // as a user's would be
    if (g.disc_path.empty() && !g.screenshot.empty()) {
        paint();
        next_test_click(hwnd);
        g.shot_pending = true;
        SetTimer(hwnd, TIMER_SHOT, 700, nullptr);
    }
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (g.disc) orre_disc_close(g.disc);
    for (OrreDisc* d : g_retired_discs) orre_disc_close(d);  // (discs switched away from while Studio ran)
    if (g.jp_disc) orre_disc_close(g.jp_disc);
    if (g_upd_on) pc_update_shutdown();  // (a download in progress keeps its .part and resumes next time)
    return g_exit_code;  // (PC_UPDATE_EXIT_RESTART after "Restart now" or "Go back": the launcher swaps app\ and restarts)
}

}  // namespace studio

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // sharp on every screen: per-monitor DPI v2 before any window exists (the manifest says the same)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const int rc = studio::run(inst, argc, argv);
    LocalFree(argv);
    return rc;
}
