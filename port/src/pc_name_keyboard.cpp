/*
 * pc_name_keyboard.cpp: OrrePort's naming keyboard, version B, on the game's own name screen (2026-11-14).
 *
 * Why: snatcherLeo asked for the name screen's keyboard to be redone "to make it more functional" in the same style, and
 * chose version B (2026-11-14: "option B for th ekeyboard"; mockups 91-94, docs/port/naming-screen.md). The game's
 * grid is 15 letters x 4 lines in alphabetical order, with two pages on X and the case of the last letter on R; B is
 * a QWERTY grid with a number row on every page, three pages on L / R (Aa, ÀÉ, #?!), X = Shift (capitals; on #?! a
 * second symbol set), Y = space, B = delete, START = done, and the first letter a capital on its own.
 *
 * How (stage 1 of the plan in naming-screen.md: the pad, the console's lengths):
 *   - Only the grid and its input change. The game's panel, buttons, name field, title, model and background particles
 *     stay as they are, so the screen looks like the game's own.
 *   - The game calls five hooks (include/pc/orre_hooks.h): its input function (OrreHook_NameEntryInput), the grid's
 *     draw item (DrawGrid), the pulsing cursor box (CursorBox), the X label and the R hint (DrawLabel: what X will do,
 *     lower / UPPER / MORE, and the page tabs), and the name field's preview letter (Preview). The SPACE key carries
 *     a Y badge (a picture OrrePort draws, served as a menu sprite through OrreHook_MenuExtra).
 *   - Letters are printed with the screen's own one-letter template (message 0xCE through pc_game_ui.print_gs), the
 *     way the game prints its grid, so the font, size and fade are the game's.
 *   - The cursor uses the game's own coordinates: `letter` is the column (0xF = the button column, whose highlights
 *     the game draws itself), `column` is the line (0-3). Positions 0-9 are keys, and on the bottom line position 7 is
 *     the SPACE key.
 *   - Names are the game's text: big-endian UTF-16. A letter is written as its two bytes, as the game writes it.
 *   - Lengths: the game's per-mode limits (7 / 7 / 10 / 8 on the US disc) for now. 12 letters comes with the
 *     .orresave (stage 3), because the game's name buffer holds 11 (name-entry.md).
 *
 * Stage 2 (the PC keyboard and the mouse; the section "stage 2" below): while the screen is open the PC keyboard types,
 * and the mouse (or a touch) hovers and clicks keys, buttons and page tabs. The last device used decides the look.
 *
 * Accurate mode and recordings: the keyboard is built in, not a tweak; each hook asks enabled(), which is false in the
 * GameCube-accurate preset and in automated runs. Then every hook returns 0 and the game's own screen runs, exactly as on the console
 * (snatcherLeo: "if a user is in Gamecube Accurate mode, we just fallback to the original keyboard"). The screen's background
 * draws the shared random numbers every frame in both keyboards (name-entry.md), so the keyboard changes no roll;
 * only the time spent naming does, as on the console.
 *
 * Typing with the operating system's input method (IME; 2026-11-15, snatcherLeo's approved four-state design): the
 * composition in the name's slots, the IME's choices in the game's window style, the commit, and "too long"; see the
 * section "the input method" below and naming-screen.md "Typing with an input method".
 *
 * Platforms: plain C++ with no OS calls of its own, except the IME's hint: Windows hands the switch to
 * pc_ime_win32.cpp (Windows only), Linux sets SDL's hint around typing (likely), macOS needs nothing (likely).
 * XD: XD's name screen is likely the same engine screen with its own art (xd-insights.md section 10); the pages, the
 * rules and this file carry over once its hook spots are found in XD's code (guess until checked).
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <atomic>
#include <cstdio>
#include <deque>
#include <map>
#include <mutex>
#include <string>

#include <SDL3/SDL.h>
#include <aurora/input.hpp>

#include "pc_font_files.h" /* the stand-in codes for letters past U+FFFF (emoji; 2026-11-16) */
#include "pc_grapheme.h"   /* the 12 letters are visible letters (grapheme clusters; 2026-11-16) */
#include "pc_lowmem.h"
#include "pc_options.h"
#include "pc_rtl.h" /* right-to-left names drawn as read in the field (draw_name; 2026-11-16) */
#include "pc_shape.h" /* the field's name shaped by HarfBuzz in SheenBidi's order (draw_name; 2026-11-16) */
#include "pc_text.h"

extern "C" {
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
extern void* (*OrreHook_MenuExtra)(int kind, u32 index);
const char* pc_game_text_language(void); /* pc_game_text.cpp: the game's language (the keyboard's words follow it) */
int pc_platform_automated_run(void); /* pc_platform.c */
int pc_platform_stretched(void);     /* pc_platform.c: the picture fills the window instead of 4:3 with bars */
extern s32 (*OrreHook_NameEntryInput)(void* window);
extern s32 (*OrreHook_NameEntryDrawGrid)(void* window);
extern s32 (*OrreHook_NameEntryCursorBox)(void* window, s32* x, s32* y, f32* width, f32* height);
extern s32 (*OrreHook_NameEntryDrawLabel)(s32 which, void* window, u8* draw);
extern s32 (*OrreHook_NameEntryPreview)(void* window, u16* letter);
extern s32 (*OrreHook_NameEntryPiece)(s32 piece, s32 type, void* window, u8* draw);
extern s32 (*OrreHook_NameEntryDrawName)(void* window, u8* draw, s32 type);
extern void (*OrreHook_NameEntryCommit)(s32 mode, s32 index, const void* stored, void* record);
/* B on the name menu (orre_hooks.h; the naming rules per kind, below) */
extern s32 (*OrreHook_NameEntryListBack)(s32 mode, s32 index, u32 caller);
extern void (*OrreHook_NameMenuLeft)(void);
void pc_fast_fades_backout(void); /* pc_field_menu.cpp */
#define ORRE_NAME_CALLER_NEW_GAME 1u /* as orre_hooks.h (this file declares the hooks it sets itself) */
/* pc_names.cpp: the full names beside the save's GameCube names (2.4) */
void pc_names_commit(s32 mode, s32 index, void* record, const uint16_t* full, const uint16_t* stored);
/* the input-method (IME) parts (2026-11-15): whether the font renderer can draw a character the game's fonts lack
 * (pc_font_render.cpp), a line of OrrePort's own small text as a coverage picture (the same file), and the game frame */
int pc_font_render_can_draw(uint16_t code);
int pc_font_render_can_draw_cp(uint32_t cp); /* the same past U+FFFF (emoji; 2026-11-16) */
unsigned char* pc_font_render_line(const char* utf8, float size, float wdth, float wght, int* w, int* h, int* baseline);
extern volatile u32 pc_vi_heartbeat; /* pc_vi.c: the game frame (ORRE_IME_TEST's log) */
#ifdef _WIN32
void pc_ime_win32_own_ui(int on);      /* pc_ime_win32.cpp (Windows only) */
int pc_ime_win32_status(unsigned* lang);
#endif
}
#include "pc_game_ui.h"

namespace {

/* ---- the pages (snatcherLeo's version B; mockup 94). Index [set][line]: set 1 is Shift's (lower case on the letter pages,
 * the second symbols on #?!). Every character is one the game's US font draws (checked against its glyph list,
 * naming-screen.md "Is B complete?"); $ is drawn as the game's Poké Dollar sign. ---- */
enum { PAGE_LETTERS, PAGE_ACCENTS, PAGE_SYMBOLS, PAGE_COUNT };
const char32_t* const kPages[PAGE_COUNT][2][4] = {
    {{U"1234567890", U"QWERTYUIOP", U"ASDFGHJKL", U"ZXCVBNM"},
     {U"1234567890", U"qwertyuiop", U"asdfghjkl", U"zxcvbnm"}},
    {{U"1234567890", U"ÀÁÂÄÇÈÉÊËÑ", U"ÌÍÎÏÒÓÔÖŒ", U"ÙÚÛÜß¿¡"},
     {U"1234567890", U"àáâäçèéêëñ", U"ìíîïòóôöœ", U"ùúûüß¿¡"}},
    {{U"1234567890", U"!?♂♀&@#%+=", U"‥“”‘’()/-", U".,:;*~_"},
     {U"1234567890", U"[]<>\"'`^|$", U"«»„‟×\\", U"←↑→↓"}},
};
/* the page tabs' labels: samples of each page's characters, not words, so they aren't in the language file */
const char32_t* const kTabs[PAGE_COUNT] = {U"Aa", U"ÀÉ", U"#?!"};

/* ---- the layout, in the screen's 640 x 480 coordinates (mockups 91-94, measured on the game's frame) ----
 * The grid's draw item sits at (36, 283); its letters print relative to that. Lines are 35 px apart, as the game's;
 * keys 42 px apart, each line set in like a typewriter's. */
const int kItemX = 36;
const int kLineY[4] = {0, 35, 70, 105};
const int kLineX0[4] = {27, 37, 48, 57}; /* the first key's centre on each line, relative to the item */
const int kPitch = 42;
const int kSpacePos = 7;                                   /* the bottom line's SPACE key, at positions 7-9 */
const int kSpaceCentre = 57 + kPitch * 8;                  /* its centre (relative): the middle of positions 7-9 */
const int kSpaceWidth = kPitch * 3 - 8;                    /* its cursor box */
const int kButtons = 0xF;                                  /* the game's button column */

struct State {
    void* window = nullptr; /* the keyboard window this state belongs to (a new window starts fresh) */
    int page = PAGE_LETTERS;
    bool shift = false; /* one press: the next character only (as a phone's Shift) */
    bool keys_look = false; /* the PC keyboard was used last: the keyboard look (stage 2), else the pad's */
    /* where the R hint's strip printed L, the three tabs and R this frame (screen x, left and right edge), for the
     * mouse: the widths come from the game's font, so they are measured as they are drawn (draw_label) */
    int strip_x0[PAGE_COUNT + 2] = {};
    int strip_x1[PAGE_COUNT + 2] = {};
    /* the name as typed, up to 12 letters (stage 3; the game's own buffer holds 11 and is given the GameCube name, a
     * cut of this one: see sync_game). Big-endian code units, as the game's text, 0-ended.
     * *Changed 2026-11-16 (snatcherLeo: the 12 letters are visible letters, pc_grapheme.h):* one visible letter can be several
     * characters (an emoji family is five, a Hindi conjunct with its vowel sign four), so the buffer holds 12 letters of
     * at most 11 characters each (PC_NAME_MAX_CPS) and `len` counts characters, never letters (letters() does) */
    uint16_t full[PC_NAME_MAX_CPS + 1] = {};
    int len = 0;
    const uint16_t* game_buf = nullptr; /* the game's name buffer (the name-entry block's +0x18), set by sync_game */
    /* the GameCube name last given to the game (its closest fit, see gc_name) and the game's own fallback name (its
     * place and its text), for "Is <name> OK?" and the commit when no letter had a GameCube form */
    uint16_t gc[13] = {};
    int gc_len = 0;
    const uint16_t* fallback = nullptr;
    uint16_t fb[13] = {};
} s;

/* ---- the input method (IME) while it composes (2026-11-15; snatcherLeo approved the design: the mockup with the four
 * states). Applied on the game's frame from the queued events, like everything typed ---- */
struct ImeState {
    std::vector<uint16_t> comp; /* the composition, drawable letters only, as the game's text (big-endian units) */
    int caret = -1;             /* SDL's cursor in the composition (code points), -1 unknown */
    std::vector<std::vector<uint16_t>> cands; /* this page's choices as the game's text */
    std::vector<std::string> labels;          /* the IME's own number for each (the key that picks it) */
    int selected = -1;  /* SDL's index (Windows: in the whole list; mapped to the page in draw_choices) */
    int page_size = 0;  /* the longest page seen while this list is open: Windows' page size (likely) */
    uint64_t flash_at = 0; /* SDL_GetTicks of the last "too long" (the field flashes, the line shows for 2 s) */
    /* the flash was for letters past the 12 (the "Only 12 letters fit" line shows), not only for marks dropped by the
     * hidden-part cap (the field flashes and the sound plays, but that line would give the wrong reason) */
    bool flash_full = true;
} ime;

/* OrrePort's limit for every kind of name (snatcherLeo, 2026-11-10 and 2026-11-14: Wes, Rui, nicknames, boxes), counted in
 * visible letters (snatcherLeo, 2026-11-16, pc_grapheme.h), and the safeguard: at most 10 hidden parts on one letter */
const int kNameMax = PC_NAME_MAX_LETTERS;
const int kHiddenMax = PC_NAME_MAX_HIDDEN;
const int kNameUnits = PC_NAME_MAX_CPS; /* the most characters the 12 letters can hold */

/* OrrePort's keyboard is built in, not a tweak (snatcherLeo, 2026-11-14: "it's a built-in default for OrrePort, and the only
 * way to toggle it off is ... Gamecube Accurate Mode"). Off in the accurate preset (read once per run, so a change
 * needs a restart, as the preset always does) and in automated runs, whose recorded presses were made on the game's
 * own grid; ORRE_NAME_KEYBOARD=1 turns it on in a test run */
bool enabled() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_NAME_KEYBOARD");
    return force != nullptr && force[0] == '1';
}

PcNameEntryView view_of(void* window) {
    PcNameEntryView v;
    std::memset(&v, 0, sizeof v);
    pc_game_ui.name_entry_view(window, &v);
    return v;
}

int game_len(const uint16_t* t) {
    int n = 0;
    while (t[n] != 0) n++;
    return n;
}

/* a unit of the game's text (big-endian in memory) as a code, and back */
uint32_t code_of(uint16_t unit) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&unit);
    return (uint32_t)b[0] << 8 | b[1];
}
uint16_t unit_of(uint32_t code) {
    uint16_t u;
    uint8_t* b = reinterpret_cast<uint8_t*>(&u);
    b[0] = (uint8_t)(code >> 8), b[1] = (uint8_t)code;
    return u;
}
uint32_t game_space(); /* below: the game's space letter */

/* ---- visible letters (*added 2026-11-16*, snatcherLeo's decision: "the 12-letter limit counts visible letters";
 * pc_grapheme.h) ----
 * The name is stored as typed, one code unit per character; these count and cut it by visible letters (Unicode's
 * extended grapheme clusters). An emoji's stand-in code (pc_font_files.h) is turned back into its real character
 * first, since the rules depend on what the character is. Platforms: all. */
std::vector<uint32_t> real_cps(const uint16_t* units, int n) {
    std::vector<uint32_t> out((size_t)std::max(n, 0));
    for (int i = 0; i < n; ++i) out[(size_t)i] = pc_text_real((uint16_t)code_of(units[i]));
    return out;
}
/* the typed name's visible letters */
int letters() {
    const std::vector<uint32_t> cps = real_cps(s.full, s.len);
    return pc_grapheme_count(cps.data(), s.len);
}
/* one character (a unit of the game's text) added at the name's end if the rules allow it: PC_GRAPHEME_ADDED, FULL (it
 * would be a 13th letter) or CAPPED (it would be an 11th hidden part on the last letter: dropped, the safeguard) */
int add_unit(uint16_t unit) {
    if (s.len >= kNameUnits) return PC_GRAPHEME_FULL; /* can't happen within the rules; the buffer's own guard */
    const std::vector<uint32_t> cps = real_cps(s.full, s.len);
    const int r = pc_grapheme_append(cps.data(), s.len, pc_text_real((uint16_t)code_of(unit)), kNameMax, kHiddenMax);
    if (r == PC_GRAPHEME_ADDED) {
        s.full[s.len++] = unit;
        s.full[s.len] = 0;
    }
    return r;
}

/* every character on the pages (all of them in the US font: naming-screen.md "Is B complete?") */
bool on_pages(char32_t c) {
    for (int p = 0; p < PAGE_COUNT; ++p)
        for (int set = 0; set < 2; ++set)
            for (int line = 0; line < 4; ++line)
                for (const char32_t* t = kPages[p][set][line]; *t != 0; ++t)
                    if (*t == c) return true;
    return false;
}
/* the game's own symbols at standard code points (fonts.md "Special symbols"; the font renderer's special()): a typed
 * α, β, ‰ or □ would draw the game's speech bubble or e-Reader mark, so they are never letters of a name (the
 * backslash, also on that list, is on the #?! page as the game's own glyph) */
bool game_symbol(uint32_t c) {
    switch (c) {
    case 0x2031: case 0x03B2: case 0x337D: case 0x03B1: case 0x337C: case 0x337E: case 0x2030: case 0x25A1: case 0xFF3E:
        return true;
    default:
        return false;
    }
}

/* "GameCube-safe" (decided 2026-11-13, pc_gcopy.c "names"): a letter fonts 0 and 4 of the player's own disc both have,
 * so a console shows it wherever the name appears. Read from the disc's fonts (pc_game_ui.font_has; never the font
 * renderer's drawn stand-ins). When a font isn't open the keyboard's own known set counts (printable ASCII and the
 * pages, all in the US font). Since the input method (2026-11-15) a name can hold kanji and other letters the console
 * lacks, so this matters now: the US font has the kana, full-width letters and a few kanji (fonts.md), not the rest. */
bool gc_safe(uint32_t c) {
    if (c == game_space() || game_symbol(c)) return c == game_space();
    const bool known = (c >= 0x21 && c <= 0x7E) || on_pages((char32_t)c);
    if (pc_game_ui.font_has == nullptr) return known;
    const int a = pc_game_ui.font_has(0, (uint16_t)c), b = pc_game_ui.font_has(4, (uint16_t)c);
    if (a < 0 || b < 0) return known;
    return a == 1 && b == 1;
}

/* a letter's plain form when the console lacks it (the decided rule: "a missing letter becomes its plain form where
 * one exists (ł -> l), otherwise dropped"): Latin letters with marks the US font lacks (Latin-1 and Latin Extended-A;
 * Polish, Czech, Nordic, Baltic...; the table was made from Unicode's own decompositions plus the barred and stroked
 * letters), and full-width ASCII as ASCII. 0: none (kana the font lacks, kanji, hangul, Cyrillic...: dropped). */
uint32_t plain_form(uint32_t c) {
    static const char kLatin[] = /* U+00C0 to U+017F, '.' = none */
        "AAAAAA.CEEEEIIIIDNOOOOO.OUUUUY..aaaaaa.ceeeeiiiidnooooo.ouuuuy.y"
        "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIi..JjKkkLlLlLlL"
        "lLlNnNnNnnNnOoOoOo..RrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
    if (c >= 0xC0 && c < 0x180) return kLatin[c - 0xC0] != '.' ? (uint32_t)(unsigned char)kLatin[c - 0xC0] : 0;
    if (c >= 0xFF01 && c <= 0xFF5E) return c - 0xFEE0;
    return 0;
}

/* the GameCube name: the typed name's closest fit, cut to the game's own limit for this kind of name (7 Wes, 7 Rui, 10
 * a nickname, 8 a box: the screen's per-mode table). Each letter a console shows is kept, one it lacks becomes its
 * plain form or is dropped (decided 2026-11-13; built 2026-11-15 with the input method, before which every typed
 * letter was in the US font). Nothing left: the game's buffer stays empty and the game itself uses its fallback name
 * (inputName: the first preset WES / RUI in modes 0 and 1, the Pokémon's current name or the box's in 2 and 3), as it
 * does for an empty name on the console. A nickname's GameCube name may not equal a species name unless the typed name
 * does (evolution renames a Pokémon whose nickname equals its old species name), so such a fit loses more letters. The
 * game only ever sees this name: what it stores, compares and saves stays a name a GameCube save can hold. */
int gc_name(const PcNameEntryView& v, uint16_t* out) {
    const int limit = pc_game_ui.name_max(v.kind);
    int n = 0;
    bool changed = false;
    for (int i = 0; i < s.len && n < limit; i++) {
        const uint32_t c = code_of(s.full[i]);
        if (gc_safe(c)) {
            out[n++] = s.full[i];
        } else if (const uint32_t p = plain_form(c)) {
            out[n++] = unit_of(p);
            changed = true;
        } else {
            changed = true;
        }
    }
    out[n] = 0;
    if (n < s.len) changed = true;
    while (v.kind == 2 && n > 0 && changed && pc_game_ui.is_species_name(out)) out[--n] = 0;
    /* only spaces left: the game treats an all-blank name as empty (inputName), so it is one */
    bool blank = true;
    for (int i = 0; i < n && blank; i++) blank = code_of(out[i]) == game_space();
    if (blank) n = 0, out[0] = 0;
    return n;
}

/* the GameCube name into the game's buffer, with the length the game keeps (+0x34) */
void sync_game(const PcNameEntryView& v) {
    uint16_t gc[13];
    const int n = gc_name(v, gc);
    for (int i = 0; i <= n; i++) v.name[i] = gc[i];
    *v.state = n;
    s.game_buf = v.name;
    for (int i = 0; i <= n; i++) s.gc[i] = gc[i];
    s.gc_len = n;
    s.fallback = v.fallback;
    int f = 0;
    for (; v.fallback != nullptr && v.fallback[f] != 0 && f < 12; f++) s.fb[f] = v.fallback[f];
    s.fb[f] = 0;
}

/* a new keyboard window: page 1, Shift off, the cursor on Q, no composition. The typed name is kept when the game's
 * buffer still holds its GameCube name (the game reopens the same screen after "Is <name> OK?" -> NO); otherwise it
 * starts from the buffer (empty, or a name the game put there) */
bool keeps_name_after_no(s32 kind); /* the naming rules per kind (below) */
void fresh(void* window, const PcNameEntryView& v) {
    uint16_t gc[13];
    const int n = gc_name(v, gc);
    bool same = game_len(v.name) == n;
    for (int i = 0; same && i < n; i++) same = v.name[i] == gc[i];
    /* *Fixed 2026-11-15 (found by the naming-rule work; snatcherLeo's rule: "if users hit NEW NAME again, they would NOT see
     * the name they typed beforehand, it would be blank"):* the same window record alone isn't "nothing changed". The
     * keyboard re-attaches to it while "Is <name> OK?" shows, and the game reuses it after NO → the name menu → NEW
     * NAME, having emptied its buffer; the early return then kept the old name on screen while the game's copy was
     * empty (START gave WES, typing added to the old letters). So the window counts as the same only while the game's
     * buffer still holds our name: true for a Pokémon's nickname after NO (the game keeps it, so do we), false for the
     * player's NEW NAME (blank, as the game intends). Platforms: all. */
    if (window == s.window && (s.len == 0 || same)) return;
    s.window = window;
    s.page = PAGE_LETTERS;
    s.shift = false;
    *v.letter = 0;
    *v.column = 1;
    ime = ImeState{};
    /* *Fixed again 2026-11-15 (snatcherLeo's hand check: a Japanese name, NO, NEW NAME still showed "武田¥"):* the buffer test
     * alone can't tell "the game kept the name" from "the game emptied it" when the name has no letters a GameCube can
     * store (its GameCube form is empty both ways). So it is decided by who is named, as snatcherLeo's rule says: people (Wes,
     * Rui, a mod's ally: the kinds with a name menu) start blank at every new opening; Pokémon and boxes keep the typed
     * name after NO, as the game does. Platforms: all. */
    if (!keeps_name_after_no(v.kind) || !same || s.len == 0) {
        s.len = 0;
        for (int i = 0; v.name[i] != 0 && i < kNameUnits; i++) s.full[s.len++] = v.name[i];
        s.full[s.len] = 0;
        s.gc_len = 0; /* the buffer's name is its own GameCube name until something is typed (sync_game) */
        for (; s.gc_len < s.len; s.gc_len++) s.gc[s.gc_len] = s.full[s.gc_len];
        s.gc[s.gc_len] = 0;
        s.game_buf = v.name;
    }
}

/* which set the page shows now: on the letter pages, capitals for the first letter (the name is empty) unless Shift,
 * and small letters after it unless Shift; on #?!, Shift's second set */
int set_now(const PcNameEntryView& v) {
    (void)v;
    if (s.page == PAGE_SYMBOLS) return s.shift ? 1 : 0;
    const bool capitals = (s.len == 0) != s.shift;
    return capitals ? 0 : 1;
}
const char32_t* line_text(const PcNameEntryView& v, int line) { return kPages[s.page][set_now(v)][line]; }
int text_len(const char32_t* t) {
    int n = 0;
    while (t[n] != 0) n++;
    return n;
}

/* what a grid position holds: 1 a character (*c), 2 the SPACE key, 0 nothing */
int key_at(const PcNameEntryView& v, int line, int pos, char32_t* c) {
    if (line == 3 && pos == kSpacePos) return 2;
    const char32_t* t = line_text(v, line);
    if (pos >= 0 && pos < text_len(t) && !(line == 3 && pos >= kSpacePos)) {
        *c = t[pos];
        return 1;
    }
    return 0;
}
int last_pos(const PcNameEntryView& v, int line) { return line == 3 ? kSpacePos : text_len(line_text(v, line)) - 1; }

/* ---- the game's text: big-endian UTF-16 ---- */
void put_be(uint16_t* at, uint32_t code) {
    uint8_t* b = reinterpret_cast<uint8_t*>(at);
    b[0] = (uint8_t)(code >> 8);
    b[1] = (uint8_t)(code & 0xFF);
}
/* UTF-8 (the language file) to the game's text, for SHIFT and SPACE; characters past U+FFFF are skipped (the game's
 * text has none) */
void utf8_to_game(const char* in, uint16_t* out, int cap) {
    int n = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(in);
    while (*p != 0 && n < cap - 1) {
        uint32_t c = *p++;
        if (c >= 0xF0) {
            p += 3;
            continue;
        }
        if (c >= 0xE0) {
            c = ((c & 0x0F) << 12) | ((uint32_t)(p[0] & 0x3F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if (c >= 0xC0) {
            c = ((c & 0x1F) << 6) | (p[0] & 0x3F);
            p += 1;
        }
        put_be(&out[n++], c);
    }
    out[n] = 0;
}
/* the game's space letter: the first character of message 0x2EF9, as the game's own grid types it */
uint32_t game_space() {
    const uint8_t* p = static_cast<const uint8_t*>(pc_game_ui.msg_gschar(0x2EF9));
    return p != nullptr ? ((uint32_t)p[0] << 8 | p[1]) : 0x20;
}

/* text printed centred on x (relative to the draw item), in the screen's one-letter template */
void print_centred(const uint16_t* text, int x, int y, uint32_t color) {
    const int32_t rect = pc_game_ui.measure_gs(text);
    const int w = (int16_t)((uint32_t)rect >> 16);
    pc_game_ui.print_gs(x - w / 2, y, color, text);
}

/* ---- the Y badge on the SPACE key (snatcherLeo, 2026-11-14, mockups 98-98d: option 1, "Y" types a space) ----
 * Why a drawn picture: the game has no Y-button picture (it hardly uses Y), so OrrePort draws one once, in the colours
 * of the game's own X icon (a light grey button, a darker rim, a dark letter), into its own RGBA8 texture 'ORYB'. It is
 * served as menu sprite 0x7040 through OrreHook_MenuExtra (chained after the LOAD SAVE button's and the Language row's
 * sprites) and drawn with the game's own sprite drawing, so it fades with the window like the game's icons. With the
 * PC keyboard in use (stage 2) a small blank space-bar cap takes its place (mockup 98d). All platforms. */
/* The fixed IDs of this file's pictures for Studio's UI editor and HD texture packs (snatcherLeo approved, 2026-11-14:
 * "Yeah that's fine"; the project plan): name_keyboard.y_badge ('ORYB'), name_keyboard.space_cap ('ORSB'),
 * name_keyboard.shift_cap ('ORK1'), name_keyboard.backspace_cap ('ORK2'), name_keyboard.enter_cap ('ORK3'). A pack's
 * picture under one of these replaces the drawn one (to build with the pack loader). */
constexpr u32 kTexY = 0x4F525942; /* 'ORYB' */
constexpr u32 kResGroup = 0x4F52; /* 'OR': OrrePort's resource group (as pc_title_loadsave.cpp's) */
constexpr u32 kSpriteY = 0x7040;  /* past the menu sprite table; 0x7000-0x703F are the Language row's and LOAD's */
constexpr int kBadgeW = 20, kBadgeH = 24; /* whole GX tiles (4 x 4) */
u8* s_badgeSprite = nullptr;
bool s_badgeTried = false;
void* (*s_prevMenuExtra)(int, u32) = nullptr;

void put16be(u8* p, uint16_t v) { p[0] = (u8)(v >> 8), p[1] = (u8)v; }
void put32be(u8* p, uint32_t v) { put16be(p, (uint16_t)(v >> 16)), put16be(p + 2, (uint16_t)v); }

float seg_dist(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    float t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
    t = std::max(0.f, std::min(1.f, t));
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

/* RGBA8 in GX's tiled layout (4 x 4 tiles, AR then GB), as the game's textures are stored */
void write_rgba8_tiles(u8* dst, const std::vector<u8>& rgba, int w, int h) {
    for (int ty = 0; ty < h / 4; ++ty)
        for (int tx = 0; tx < w / 4; ++tx) {
            u8* tile = dst + ((size_t)ty * (w / 4) + tx) * 64;
            for (int i = 0; i < 16; ++i) {
                const u8* p = &rgba[((size_t)(ty * 4 + i / 4) * w + tx * 4 + i % 4) * 4];
                tile[i * 2] = p[3], tile[i * 2 + 1] = p[0];
                tile[32 + i * 2] = p[1], tile[32 + i * 2 + 1] = p[2];
            }
        }
}

/* the badge drawn once (an oval with a rim and a Y, smoothed by 4 x 4 samples per pixel) and its sprite record (a copy
 * of a menu sprite record of the same kind, pointed at 'ORYB'); false when the texture couldn't be made */
bool build_badge() {
    if (s_badgeSprite != nullptr) return true;
    if (s_badgeTried) return false;
    s_badgeTried = true;
    const PcGameUi& g = pc_game_ui;
    if (g.texture_create == nullptr || g.menu_sprite == nullptr) return false;
    /* the letter in the X icon's style (snatcherLeo, 2026-11-14: "the text for Y needs to match the "X" text"): white with a
     * dark outline, on the light grey button */
    /* Sizes and the outline's shade settled with snatcherLeo on mockups 99-99c ("make it slightly more subtle"): */
    /* the button's outer outline dark, like the game's B button's (snatcherLeo, 2026-11-14, mockup 100b: "make the outline of
     * the overall button the same dark outline that looks just like the Back button") */
    const u8 rim[3] = {44, 44, 50}, fill[3] = {196, 198, 204}, ink[3] = {40, 40, 45}, white[3] = {248, 248, 248};
    const float cx = kBadgeW / 2.f, cy = kBadgeH / 2.f, rx = kBadgeW / 2.f - 0.5f, ry = kBadgeH / 2.f - 0.5f;
    std::vector<u8> rgba((size_t)kBadgeW * kBadgeH * 4, 0);
    for (int y = 0; y < kBadgeH; ++y)
        for (int x = 0; x < kBadgeW; ++x) {
            float sum[3] = {0, 0, 0};
            int inside = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    const float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
                    const float e = std::pow((px - cx) / rx, 2.f) + std::pow((py - cy) / ry, 2.f);
                    if (e > 1.f) continue;
                    ++inside;
                    const float inner = std::pow((px - cx) / (rx - 1.25f), 2.f) + std::pow((py - cy) / (ry - 1.25f), 2.f);
                    const float letter = std::min({seg_dist(px, py, cx - 3.f, cy - 4.2f, cx, cy + 0.3f),
                                                    seg_dist(px, py, cx + 3.f, cy - 4.2f, cx, cy + 0.3f),
                                                    seg_dist(px, py, cx, cy + 0.3f, cx, cy + 4.6f)});
                    const u8* c = letter < 0.85f ? white : letter < 1.85f ? ink : (inner > 1.f ? rim : fill);
                    for (int k = 0; k < 3; ++k) sum[k] += c[k];
                }
            if (inside == 0) continue;
            u8* p = &rgba[((size_t)y * kBadgeW + x) * 4];
            for (int k = 0; k < 3; ++k) p[k] = (u8)std::lround(sum[k] / inside);
            p[3] = (u8)std::lround(255.f * inside / 16);
        }
    void* tex = g.texture_create(kBadgeW, kBadgeH, 0x45 /* RGBA8 */);
    if (tex == nullptr) return false;
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(tex)), rgba, kBadgeW, kBadgeH);
    g.texture_unlock(tex);
    g.resource_register(tex, kResGroup, kTexY);
    u8* sp = static_cast<u8*>(pc_lowmem_alloc(0x18)); /* the game keeps 32-bit pointers to sprite records */
    if (sp == nullptr) return false;
    std::memcpy(sp, g.menu_sprite(0xC0), 0x18); /* an RGBA8 menu sprite's record (the bag's panel, as the GCI tag) */
    sp[0x07] = 0xFF;
    put16be(sp + 0x08, 0), put16be(sp + 0x0A, 0), put16be(sp + 0x0C, (uint16_t)kBadgeW);
    put16be(sp + 0x0E, (uint16_t)kBadgeH), put32be(sp + 0x10, kTexY);
    s_badgeSprite = sp;
    return true;
}

/* the space-bar cap that replaces the Y badge while the PC keyboard is used (snatcherLeo, 2026-11-14, mockup 98d: "No text
 * works perfectly, people would know"): a short, wide light-grey key with a darker rim, no word on it. Its own texture
 * 'ORSB' and sprite 0x7041, made the same way as the badge */
constexpr u32 kTexSpace = 0x4F525342; /* 'ORSB' */
constexpr u32 kSpriteSpace = 0x7041;
constexpr int kCapW = 20, kCapH = 12;
u8* s_capSprite = nullptr;
bool s_capTried = false;

bool build_space_cap() {
    if (s_capSprite != nullptr) return true;
    if (s_capTried) return false;
    s_capTried = true;
    const PcGameUi& g = pc_game_ui;
    if (g.texture_create == nullptr || g.menu_sprite == nullptr) return false;
    /* the dark outline of the game's icons, as the key caps (snatcherLeo, 2026-11-14: "Space icon we can change slightly the
     * outline") */
    const u8 rim[3] = {44, 44, 50}, fill[3] = {214, 216, 220};
    const float w = (float)kCapW, h = (float)kCapH, r = 3.f;
    auto dist = [&](float x, float y) { /* signed distance to the rounded rectangle's edge (negative inside) */
        const float qx = std::fabs(x - w / 2) - (w / 2 - r), qy = std::fabs(y - h / 2) - (h / 2 - r);
        return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - r;
    };
    std::vector<u8> rgba((size_t)kCapW * kCapH * 4, 0);
    for (int y = 0; y < kCapH; ++y)
        for (int x = 0; x < kCapW; ++x) {
            float sum[3] = {0, 0, 0};
            int inside = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    const float d = dist(x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4);
                    if (d >= 0) continue;
                    ++inside;
                    const u8* c = d > -1.2f ? rim : fill;
                    for (int k = 0; k < 3; ++k) sum[k] += c[k];
                }
            if (inside == 0) continue;
            u8* p = &rgba[((size_t)y * kCapW + x) * 4];
            for (int k = 0; k < 3; ++k) p[k] = (u8)std::lround(sum[k] / inside);
            p[3] = (u8)std::lround(255.f * inside / 16);
        }
    void* tex = g.texture_create(kCapW, kCapH, 0x45 /* RGBA8 */);
    if (tex == nullptr) return false;
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(tex)), rgba, kCapW, kCapH);
    g.texture_unlock(tex);
    g.resource_register(tex, kResGroup, kTexSpace);
    u8* sp = static_cast<u8*>(pc_lowmem_alloc(0x18));
    if (sp == nullptr) return false;
    std::memcpy(sp, g.menu_sprite(0xC0), 0x18);
    sp[0x07] = 0xFF;
    put16be(sp + 0x08, 0), put16be(sp + 0x0A, 0), put16be(sp + 0x0C, (uint16_t)kCapW);
    put16be(sp + 0x0E, (uint16_t)kCapH), put32be(sp + 0x10, kTexSpace);
    s_capSprite = sp;
    return true;
}

/* ---- the keyboard look's symbol key caps on the three buttons (snatcherLeo, 2026-11-14, mockup 100: "symbols (B)") ----
 * While the PC keyboard is used, the X, B and START icons give way to the keys that do the same: ⇧ (Shift), ⌫
 * (Backspace) and ↵ (Enter). Symbols, not words, so they read the same in every language and match the blank
 * space-bar cap. The game's three icons are menu items of their own (read from the item table in main.dol): X's item
 * 0x3FD at (505, 268) 33 x 42 shows sprite 0x87, B's 0x3FC at (504, 328) 36 x 35 sprite 0x88, START's 0x3FB at
 * (502, 386) 38 x 33 sprite 0x86. The buttons under them are one plain picture with no icon in it (sprite 0x7E,
 * 123 x 47, items 0x3F0-0x3F2 at x 498, y 266 / 322 / 375), tinted by their callbacks: BACK and DONE (0, 0x35, 0x3C),
 * the X button the next page's colour (lbl_80266E18).
 *
 * How (snatcherLeo, 2026-11-14: "we just have to figure out how to replace it perfectly"): while the PC keyboard is used, the
 * three icon pieces of the open keyboard window are not drawn (their draw alpha set to 0 each frame through
 * pc_game_ui.window_piece_alpha, found by their exact sizes), so the buttons show exactly as the game draws them, and
 * the key caps are drawn on top in the three label items (OrreHook_NameEntryDrawLabel, which 0, 2, 3). With the
 * gamepad the alpha goes back to 255 and the game's icons show.
 * *Corrected four times the same day (snatcherLeo's checks):* a navy box painted over each icon showed its edges; a draw
 * callback on the icon items and a swap of their sprite records both changed nothing on screen (a window turns its
 * items into pieces when it opens, so later table changes never reach it: verified by those two builds); redrawing
 * the plain button picture over the icons showed a lighter block and left icon edges (its tint and scaling aren't
 * reproduced exactly; mockup 101's build). Hiding the live pieces needs no matching at all.
 * Textures 'ORK1'-'ORK3' (fixed IDs name_keyboard.shift_cap / backspace_cap / enter_cap). Replaceable in Studio's UI
 * editor and HD packs, like all OrrePort-made UI (the project plan) */
constexpr int kKeyW = 32, kKeyH = 28;
constexpr u32 kSpriteKeyShift = 0x7044, kSpriteKeyEnter = 0x7046;
/* the icon pieces' sizes (their items in the table: X 0x3FD 33 x 42, B 0x3FC 36 x 35, START 0x3FB 38 x 33; no other
 * piece of the keyboard window has these sizes) */
const int16_t kIconSize[3][2] = {{33, 42}, {36, 35}, {38, 33}};
u8* s_keySprites[3] = {nullptr, nullptr, nullptr}; /* 0x7044 + i */
bool s_keysTried = false;

/* one RGBA8 picture registered as an OrrePort texture and served as a menu sprite (the sprite shows the top-left
 * regionW x regionH of the texture; 0: all of it); nullptr on failure */
u8* make_sprite(u32 texId, int w, int h, const std::vector<u8>& rgba, int regionW = 0, int regionH = 0) {
    const PcGameUi& g = pc_game_ui;
    void* tex = g.texture_create(w, h, 0x45 /* RGBA8 */);
    if (tex == nullptr) return nullptr;
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(tex)), rgba, w, h);
    g.texture_unlock(tex);
    g.resource_register(tex, kResGroup, texId);
    u8* sp = static_cast<u8*>(pc_lowmem_alloc(0x18));
    if (sp == nullptr) return nullptr;
    std::memcpy(sp, g.menu_sprite(0xC0), 0x18);
    sp[0x07] = 0xFF;
    put16be(sp + 0x08, 0), put16be(sp + 0x0A, 0), put16be(sp + 0x0C, (uint16_t)(regionW > 0 ? regionW : w));
    put16be(sp + 0x0E, (uint16_t)(regionH > 0 ? regionH : h)), put32be(sp + 0x10, texId);
    return sp;
}

/* distance from a point to a closed polygon's outline, and whether it is inside (even-odd rule) */
float poly_dist(float px, float py, const float* xy, int n, bool* inside) {
    float best = 1e9f;
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
        best = std::min(best, seg_dist(px, py, xj, yj, xi, yi));
        if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) in = !in;
    }
    if (inside != nullptr) *inside = in;
    return best;
}

/* a key cap with its symbol: which 0 ⇧, 1 ⌫, 2 ↵. A light grey rounded key with a dark outline like the game's X / B /
 * START icons' (snatcherLeo, 2026-11-14: "make sure the square-shaped outlines match the same dark outline"), the symbol in
 * dark grey; 4 x 4 samples per pixel */
std::vector<u8> key_cap_rgba(int which) {
    const u8 rim[3] = {44, 44, 50}, fill[3] = {214, 216, 220}, ink[3] = {30, 30, 34};
    const float w = (float)kKeyW, h = (float)kKeyH, r = 4.f, cx = w / 2, cy = h / 2;
    auto edge = [&](float x, float y) {
        const float qx = std::fabs(x - cx) - (cx - r), qy = std::fabs(y - cy) - (cy - r);
        return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - r;
    };
    const float shift[] = {0, -9, 8, 0, 4, 0, 4, 8, -4, 8, -4, 0, -8, 0};
    const float back[] = {-11, 0, -5, -6, 10, -6, 10, 6, -5, 6};
    std::vector<u8> rgba((size_t)kKeyW * kKeyH * 4, 0);
    for (int y = 0; y < kKeyH; ++y)
        for (int x = 0; x < kKeyW; ++x) {
            float sum[3] = {0, 0, 0};
            int inside = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    const float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
                    const float d = edge(px, py);
                    if (d >= 0) continue;
                    ++inside;
                    const float lx = px - cx, ly = py - cy;
                    bool symbol = false;
                    if (which == 0) symbol = poly_dist(lx, ly, shift, 7, nullptr) < 0.8f;
                    if (which == 1)
                        symbol = poly_dist(lx, ly, back, 5, nullptr) < 0.8f || seg_dist(lx, ly, -1, -3, 5, 3) < 0.8f ||
                                 seg_dist(lx, ly, -1, 3, 5, -3) < 0.8f;
                    if (which == 2) {
                        bool head = false;
                        const float tri[] = {-10, 3, -5, -1.5f, -5, 7.5f};
                        poly_dist(lx, ly, tri, 3, &head);
                        symbol = head || seg_dist(lx, ly, 8, -7, 8, 3) < 0.9f || seg_dist(lx, ly, 8, 3, -6, 3) < 0.9f;
                    }
                    const u8* c = symbol ? ink : (d > -1.3f ? rim : fill);
                    for (int k = 0; k < 3; ++k) sum[k] += c[k];
                }
            if (inside == 0) continue;
            u8* p = &rgba[((size_t)y * kKeyW + x) * 4];
            for (int k = 0; k < 3; ++k) p[k] = (u8)std::lround(sum[k] / inside);
            p[3] = (u8)std::lround(255.f * inside / 16);
        }
    return rgba;
}

bool build_key_caps() {
    if (s_keySprites[2] != nullptr) return true;
    if (s_keysTried) return false;
    s_keysTried = true;
    if (pc_game_ui.texture_create == nullptr || pc_game_ui.menu_sprite == nullptr) return false;
    for (int i = 0; i < 3; ++i) s_keySprites[i] = make_sprite(0x4F524B31 + (u32)i /* 'ORK1'.. */, kKeyW, kKeyH, key_cap_rgba(i));
    return s_keySprites[0] != nullptr && s_keySprites[1] != nullptr && s_keySprites[2] != nullptr;
}

/* the game's X / B / START icon pieces in the open keyboard window: hidden (keyboard look) or shown (the pad's) */
void show_icons(void* window, bool show) {
    if (pc_game_ui.window_piece_alpha == nullptr) return;
    for (const auto& sz : kIconSize) pc_game_ui.window_piece_alpha(window, sz[0], sz[1], show ? 0xFF : 0);
}

/* in a label item (x 552, y 278 / 334 / 389): while the PC keyboard is used, the key cap centred where the icon is
 * (the icon items' centres: X (521, 289), B (522, 345), START (521, 402)). which: 0 X, 1 B, 2 START */
void draw_key_cap(void* window, int which) {
    if (!s.keys_look || !build_key_caps()) return;
    static const int kLabelY[3] = {278, 334, 389};
    static const int kCentreX[3] = {521, 522, 521}, kCentreY[3] = {289, 345, 402};
    pc_game_ui.draw_sprite((int16_t)(kCentreX[which] - kKeyW / 2 - 552),
                           (int16_t)(kCentreY[which] - kKeyH / 2 - kLabelY[which]), window, kSpriteKeyShift + (u32)which);
}

/* ---- OrrePort's own small words on the name screen (the IME work, 2026-11-15): the help line, the "too long" line,
 * the "あ IME" tag and the choices' numbers ----
 * Why pictures: the game's one-letter template prints only at the name's own size (22 px), too big for a help line
 * under the field (the approved mockup draws them small). The texts are drawn by the font renderer
 * (pc_font_render_line: Archivo, with kana and other scripts from the fallback fonts) at twice their size, white with
 * their coverage as alpha, packed into ONE OrrePort texture ('OIME') and served as menu sprites 0x7050 + n, each a
 * crop of it; drawn at half size with the game's sprite drawing, which tints them and fades them with the window.
 * *Changed 2026-11-15 (the first test run):* one texture per text failed from the 8th on (texts missing in the frame
 * dumps): the game hands out texture records from a fixed pool (GStextureCreate -> textureAlloc), so OrrePort uses
 * as few as it can. The atlas is built once per language, when the keyboard opens (never while drawing), with every
 * text the IME parts can show. All platforms (the fonts each OS has decide the scripts). */
struct TextSprite {
    u8* record = nullptr;
    int w = 0, h = 0; /* the drawn size (half the picture's) */
};
constexpr u32 kSpriteText0 = 0x7050, kTextSprites = 0x20;
std::map<std::string, int> s_textIndex; /* "text@size" -> its sprite (index in s_texts) */
std::vector<TextSprite> s_texts;
std::string s_atlasLang; /* the language the atlas was built for */
int s_atlasBuilds = 0;   /* each build takes a texture from the game's pool: at most a few per run */

std::string text_key(const std::string& utf8, float px) { return utf8 + "@" + std::to_string((int)(px * 10)); }

/* every text with its size, packed in rows into one texture (shelf packing, 512 px wide) */
void build_atlas(const std::vector<std::pair<std::string, float>>& texts, const std::string& lang) {
    if (s_atlasLang == lang || s_atlasBuilds >= 3 || pc_game_ui.texture_create == nullptr) return;
    s_atlasBuilds++;
    s_atlasLang = lang;
    struct Piece {
        std::string key;
        int w, h, x = 0, y = 0;
        unsigned char* cov;
    };
    std::vector<Piece> pieces;
    for (const auto& [utf8, px] : texts) {
        int w = 0, h = 0, base = 0;
        unsigned char* cov = pc_font_render_line(utf8.c_str(), px * 2, 100.f, 650.f, &w, &h, &base);
        if (cov != nullptr && w > 0 && h > 0 && w <= 508) pieces.push_back({text_key(utf8, px), w, h, 0, 0, cov});
        else if (cov != nullptr) std::free(cov);
    }
    const int W = 512;
    int x = 0, y = 0, rowH = 0;
    for (Piece& p : pieces) { /* a pixel of room around each, places on even pixels (drawn at half size) */
        const int pw = (p.w + 3) & ~1, ph = (p.h + 3) & ~1;
        if (x + pw > W) x = 0, y += rowH, rowH = 0;
        p.x = x + 1, p.y = y + 1;
        x += pw;
        rowH = std::max(rowH, ph);
    }
    const int H = std::min(1024, ((y + rowH) + 3) & ~3);
    std::vector<u8> rgba((size_t)W * H * 4, 0);
    for (const Piece& p : pieces)
        for (int py = 0; py < p.h; ++py)
            for (int px = 0; px < p.w; ++px) {
                if (p.y + py >= H) continue;
                u8* o = &rgba[((size_t)(p.y + py) * W + p.x + px) * 4];
                o[0] = o[1] = o[2] = 0xFF;
                o[3] = p.cov[(size_t)py * p.w + px];
            }
    const u32 texId = 0x4F494D45u + (u32)(s_atlasBuilds - 1); /* 'OIME', then the next ids for a rebuild */
    u8* first = make_sprite(texId, W, H, rgba);
    s_textIndex.clear();
    s_texts.clear();
    for (const Piece& p : pieces) {
        if (first != nullptr && s_texts.size() < kTextSprites) {
            u8* rec = static_cast<u8*>(pc_lowmem_alloc(0x18)); /* the game keeps 32-bit pointers to sprite records */
            if (rec != nullptr) {
                std::memcpy(rec, first, 0x18);
                const int x0 = p.x - 1, y0 = p.y - 1, w = (p.w + 3) & ~1, h = (p.h + 3) & ~1;
                put16be(rec + 0x08, (uint16_t)x0), put16be(rec + 0x0A, (uint16_t)y0);
                put16be(rec + 0x0C, (uint16_t)w), put16be(rec + 0x0E, (uint16_t)h);
                s_textIndex[p.key] = (int)s_texts.size();
                s_texts.push_back({rec, w / 2, h / 2});
            }
        }
        std::free(p.cov);
    }
    std::printf("[name_keyboard] the IME words: %zu texts in one %dx%d texture (%s)%s\n", s_texts.size(), W, H,
                lang.c_str(), first == nullptr ? ": the texture couldn't be made, the words are left out" : "");
}

/* the IME parts' words in the game's language (the keyboard's words follow it, as SPACE does), with their sizes: the
 * help line, the "too long" line, the tag for each input language's mark, and the IME's numbers 0-9 */
constexpr float kWordPx = 11.f, kNumberPx = 9.f;
const char* const kImeMarks[] = {"\xE3\x81\x82" /* あ */, "\xEA\xB0\x80" /* 가 */, "\xE4\xB8\xAD" /* 中 */};
std::vector<std::pair<std::string, float>> ime_texts(const char* lang) {
    std::vector<std::pair<std::string, float>> t;
    t.push_back({tx_lang(lang, "game.name_entry.ime_help"), kWordPx});
    t.push_back({tx_lang(lang, "game.name_entry.ime_too_long", {{"n", std::to_string(kNameMax)}}), kWordPx});
    for (const char* mark : kImeMarks) t.push_back({tx_lang(lang, "game.name_entry.ime_tag", {{"mark", mark}}), kWordPx});
    for (int d = 0; d <= 9; d++) t.push_back({std::to_string(d), kNumberPx});
    return t;
}

const TextSprite* text_sprite(const std::string& utf8, float px) {
    const auto it = s_textIndex.find(text_key(utf8, px));
    return it != s_textIndex.end() ? &s_texts[(size_t)it->second] : nullptr;
}
/* a text drawn with its top-left at x / y (relative to the draw item), colour 0xRRGGBB00 | alpha */
void draw_text(void* window, const std::string& utf8, float px, int x, int y, uint32_t rgba, bool centred = false) {
    const TextSprite* t = text_sprite(utf8, px);
    if (t == nullptr) return;
    const u32 index = kSpriteText0 + (u32)(t - s_texts.data());
    pc_game_ui.draw_sprite_sized((int16_t)(centred ? x - t->w / 2 : x), (int16_t)y, (int16_t)t->w, (int16_t)t->h, rgba,
                                 window, index);
}
int text_width(const std::string& utf8, float px) {
    const TextSprite* t = text_sprite(utf8, px);
    return t != nullptr ? t->w : 0;
}

/* the game asks for a sprite past its table (OrreHook_MenuExtra): the badge's, the space-bar cap's, a key's or the
 * button crop, else the hook registered before */
void* menu_extra(int kind, u32 index) {
    if (kind == 1 && index >= kSpriteText0 && index < kSpriteText0 + s_texts.size()) return s_texts[index - kSpriteText0].record;
    if (kind == 1 && index == kSpriteY && s_badgeSprite != nullptr) return s_badgeSprite;
    if (kind == 1 && index == kSpriteSpace && s_capSprite != nullptr) return s_capSprite;
    if (kind == 1 && index >= kSpriteKeyShift && index <= kSpriteKeyEnter && s_keySprites[index - kSpriteKeyShift] != nullptr)
        return s_keySprites[index - kSpriteKeyShift];
    return s_prevMenuExtra != nullptr ? s_prevMenuExtra(kind, index) : nullptr;
}

/* ---- stage 2: the PC keyboard types while a name keyboard is open (snatcherLeo, 2026-11-14: "when a keyboard is being used,
 * for the naming screens ... it needs to automatically realize "Oh, they're using a keyboard to type something in."")
 *
 * Why: the PC keyboard is Player 1's controller everywhere (pc_controls.c: WASD, J K L I...), so typing a name moved
 * the cursor and pressed buttons instead. The rule: while OrrePort's name keyboard is open, the PC keyboard types; the
 * moment it closes, the keyboard is a controller again. The gamepad keeps working the whole time.
 *
 * How: an Aurora input layer above the game's pad layer captures only the keyboard source while typing is on, so the
 * keyboard's controller bindings see nothing (and stay saved untouched), and every key and typed text comes here
 * instead. Esc still reaches OrrePort's menu, whose layer is higher. Text arrives through SDL's text input (started
 * while typing is on), so the player's keyboard layout, dead keys and AltGr accents, and the operating system's input
 * method (IME, for Japanese) all work. The events are queued here and applied by the game's input hook, so the name
 * changes only on the game's own frame.
 *
 * Platforms: SDL's text input and Aurora's router are the same on Windows, Linux and macOS (Windows verified by hand;
 * the others likely). On a Steam Deck in Game Mode, SDL_StartTextInput is also what brings up Steam's on-screen
 * keyboard (likely, per SDL's docs; to check on a Deck). */
namespace input = aurora::input;

enum class Cmd { Text, Delete, Done, PageNext, PagePrev, Left, Right, Up, Down, Hover, Click, Compose, Choices };
struct KbCmd {
    Cmd cmd;
    std::string text;
    int x = 0, y = 0; /* Hover / Click: the pointer on the game's 640 x 480 screen; Compose: the IME's cursor, length;
                       * Choices: the selected index */
    int button = 0;   /* Click: SDL_BUTTON_LEFT / RIGHT (a touch counts as left) */
    std::vector<std::string> list; /* Choices: the IME's candidates, as SDL gives them */
};
std::mutex s_queueLock;
std::deque<KbCmd> s_queue;
std::atomic<bool> s_typing{false};      /* the name keyboard is open in OrrePort's version */
std::atomic<uint64_t> s_typingTick{0};  /* when the game's input hook last ran (SDL_GetTicks) */
/* the IME is composing (its text isn't empty), and when it last stopped (SDL_GetTicks). While it composes, Enter,
 * Backspace, Tab and the arrows belong to the IME (Enter keeps the composition, Backspace edits it, the arrows move
 * between its parts), so they mustn't also press DONE or delete a letter of the name. Windows sends the Enter that
 * keeps a composition before the kept text (likely: the key arrives first, the IME's result after it), so the guard
 * also covers 150 ms after a composition ends. Set where the events arrive (layer_event) */
std::atomic<bool> s_composing{false};
std::atomic<uint64_t> s_composeEnd{0};
input::LayerId s_layer = input::kInvalidLayerId;
SDL_Window* s_textWindow = nullptr;

/* typing counts as on only while the hook keeps running: if the screen went away some other way, the keyboard is a
 * controller again a quarter of a second later */
bool typing_now() { return s_typing.load() && SDL_GetTicks() - s_typingTick.load() < 250; }

/* the mouse and touch (stage 2, the mouse: snatcherLeo's plan, "The mouse hovers and clicks keys"). While the keyboard is open
 * they belong to it: hovering a key moves the cursor there, a left click (or a tap) presses what is under it (a key, a
 * button on the right, a page tab or L / R under the grid), and a right click deletes, as B does */
bool pointer_source(const input::InputSource& source) {
    return source.id == input::mouse_source().id || source.id == input::touch_source().id;
}

bool layer_captures(const input::InputSource& source, void*) {
    return (source.id == input::keyboard_source().id || pointer_source(source)) && typing_now();
}

/* the pointer stays visible while the keyboard is open (the game hides it after a moment of no mouse use) */
input::PointerMode layer_pointer(void*) {
    return typing_now() ? input::PointerMode::Visible : input::PointerMode::None;
}

/* a point in the window (SDL window coordinates) on the game's 640 x 480 screen. The picture is either fitted 4:3 and
 * centred with bars (the default) or stretched to the window (settings.ini "stretch", pc_platform.c), as Aurora
 * presents it (window.cpp get_window_size). Likely right for both; the hand check confirms the fitted one */
bool window_to_game(SDL_Window* w, float wx, float wy, float* gx, float* gy) {
    int ww = 0, wh = 0;
    if (w == nullptr || !SDL_GetWindowSize(w, &ww, &wh) || ww <= 0 || wh <= 0) return false;
    if (pc_platform_stretched()) {
        *gx = wx * 640.f / ww;
        *gy = wy * 480.f / wh;
        return true;
    }
    const float k = std::min(ww / 640.f, wh / 480.f), ox = (ww - 640.f * k) / 2, oy = (wh - 480.f * k) / 2;
    *gx = (wx - ox) / k;
    *gy = (wy - oy) / k;
    return true;
}

input::EventResult pointer_event(const input::InputEvent& event) {
    using E = input::InputEvent;
    const auto* p = event.payload.get_if<E::PointerChanged>();
    if (p == nullptr) return input::EventResult::Consume; /* scrolls and the rest: nothing on this screen */
    using Phase = E::PointerChanged::Phase;
    if (p->phase != Phase::Move && p->phase != Phase::Down) return input::EventResult::Consume;
    const bool touch = event.source.id == input::touch_source().id;
    if (touch && p->phase == Phase::Move) return input::EventResult::Consume; /* a finger has no hover */
    float gx, gy;
    if (!window_to_game(SDL_GetMouseFocus() != nullptr ? SDL_GetMouseFocus() : s_textWindow, p->position.x,
                        p->position.y, &gx, &gy))
        return input::EventResult::Consume;
    KbCmd c{p->phase == Phase::Move ? Cmd::Hover : Cmd::Click, {}};
    c.x = (int)std::floor(gx);
    c.y = (int)std::floor(gy);
    c.button = touch ? SDL_BUTTON_LEFT : p->button;
    std::lock_guard<std::mutex> lock(s_queueLock);
    if (c.cmd == Cmd::Hover && !s_queue.empty() && s_queue.back().cmd == Cmd::Hover) s_queue.back() = c; /* the latest */
    else s_queue.push_back(c);
    return input::EventResult::Consume;
}

input::EventResult layer_event(const input::InputEvent& event, void*) {
    using E = input::InputEvent;
    if (pointer_source(event.source)) return typing_now() ? pointer_event(event) : input::EventResult::Pass;
    if (event.source.id != input::keyboard_source().id || !typing_now()) return input::EventResult::Pass;
    std::lock_guard<std::mutex> lock(s_queueLock);
    if (const auto* text = event.payload.get_if<E::TextInput>()) {
        if (!text->text.empty()) s_queue.push_back({Cmd::Text, text->text});
        return input::EventResult::Consume;
    }
    /* the IME's composition (SDL_EVENT_TEXT_EDITING: SDL_HINT_IME_IMPLEMENTED_UI has "composition" while this keyboard
     * types, pc_ime_win32.cpp / start_typing) and its choices (SDL_EVENT_TEXT_EDITING_CANDIDATES, Windows only in SDL
     * 3.4; routed by an aurora: addition, input.hpp). Only the latest of each matters, so a newer one replaces a
     * queued one */
    if (const auto* edit = event.payload.get_if<E::TextEditing>()) {
        const bool now = !edit->text.empty();
        if (s_composing.exchange(now) && !now) s_composeEnd = SDL_GetTicks();
        KbCmd c{Cmd::Compose, edit->text};
        c.x = edit->start, c.y = edit->length;
        if (!s_queue.empty() && s_queue.back().cmd == Cmd::Compose) s_queue.back() = c;
        else s_queue.push_back(c);
        return input::EventResult::Consume;
    }
    if (const auto* list = event.payload.get_if<E::TextEditingCandidates>()) {
        KbCmd c{Cmd::Choices, {}};
        c.x = list->selected;
        c.list = list->candidates;
        if (!s_queue.empty() && s_queue.back().cmd == Cmd::Choices) s_queue.back() = c;
        else s_queue.push_back(c);
        return input::EventResult::Consume;
    }
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        if (!key->pressed) return input::EventResult::Consume;
        /* the IME's own keys while it composes (s_composing above) */
        if (s_composing.load() || SDL_GetTicks() - s_composeEnd.load() < 150) {
            switch (key->keycode) {
            case SDLK_BACKSPACE: case SDLK_DELETE: case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_TAB:
            case SDLK_PAGEDOWN: case SDLK_PAGEUP: case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN:
                return input::EventResult::Consume;
            default: break;
            }
        }
        const bool shiftHeld = (key->modifiers & SDL_KMOD_SHIFT) != 0;
        switch (key->keycode) {
        case SDLK_BACKSPACE:
        case SDLK_DELETE: s_queue.push_back({Cmd::Delete, {}}); break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (!key->repeat) s_queue.push_back({Cmd::Done, {}});
            break;
        case SDLK_TAB:
            if (!key->repeat) s_queue.push_back({shiftHeld ? Cmd::PagePrev : Cmd::PageNext, {}});
            break;
        case SDLK_PAGEDOWN: s_queue.push_back({Cmd::PageNext, {}}); break;
        case SDLK_PAGEUP: s_queue.push_back({Cmd::PagePrev, {}}); break;
        case SDLK_LEFT: s_queue.push_back({Cmd::Left, {}}); break;
        case SDLK_RIGHT: s_queue.push_back({Cmd::Right, {}}); break;
        case SDLK_UP: s_queue.push_back({Cmd::Up, {}}); break;
        case SDLK_DOWN: s_queue.push_back({Cmd::Down, {}}); break;
        default: break; /* letters come as text; other keys do nothing while typing */
        }
        return input::EventResult::Consume;
    }
    return input::EventResult::Consume;
}

/* the name field's place on the window, for the IME's composition window: the game's 640 x 480 screen scaled into the
 * window as the game is drawn (fitted and centred; likely right for the usual 4:3 view, close enough for an IME box) */
void set_text_area(SDL_Window* w) {
    int ww = 0, wh = 0;
    if (!SDL_GetWindowSize(w, &ww, &wh) || ww <= 0 || wh <= 0) return;
    const float k = std::min(ww / 640.f, wh / 480.f), ox = (ww - 640.f * k) / 2, oy = (wh - 480.f * k) / 2;
    SDL_Rect r{(int)(ox + 366 * k), (int)(oy + 150 * k), (int)(190 * k), (int)(44 * k)};
    SDL_SetTextInputArea(w, &r, 0);
}

void start_typing() {
    s_typingTick = SDL_GetTicks();
    if (s_typing.exchange(true)) return;
    if (s_layer == input::kInvalidLayerId) {
        s_layer = input::register_layer({
            .label = "OrrePort name keyboard",
            .priority = input::kGameLayerPriority + 50, /* above the game's pad, below OrrePort's menu (RmlUi) */
            .onEvent = layer_event,
            .capturesSource = layer_captures,
            .pointerMode = layer_pointer,
        });
    }
    s_textWindow = SDL_GetKeyboardFocus();
    /* this keyboard draws the IME's composition and choices itself (the IME work, 2026-11-15), so the system's own
     * strip and list are hidden while it types and come back for the hub's and overlay's text fields after it:
     *   - Windows: SDL reads the hint once per run, so it is set at start-up and pc_ime_win32.cpp decides per moment
     *     who gets the IME's messages (verified in SDL 3.4's source);
     *   - Linux (IBus, fcitx): SDL follows the hint live, so it is set here and cleared in stop_typing (likely; IBus
     *     then hands its preedit to SDL; not run on Linux yet). Candidates stay the IME's own window there (SDL 3.4
     *     sends no candidate events on Linux: a FIXME in its IBus / fcitx code);
     *   - macOS: Cocoa always hands the composition to the app and draws its own list; the hint changes nothing
     *     (likely). The list is placed by set_text_area on every OS */
#ifdef _WIN32
    pc_ime_win32_own_ui(1);
#else
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
#endif
    if (s_textWindow != nullptr) {
        set_text_area(s_textWindow);
        SDL_StartTextInput(s_textWindow);
    }
    input::reconcile(); /* keys held as the screen opened are let go for the pad, as the router promises */
}

void stop_typing() {
    if (!s_typing.exchange(false)) return;
    if (s_textWindow != nullptr) SDL_StopTextInput(s_textWindow);
    s_textWindow = nullptr;
#ifdef _WIN32
    pc_ime_win32_own_ui(0);
#else
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, nullptr); /* the default: the system draws it (RmlUi's fields) */
#endif
    s_composing = false;
    ime = ImeState{};
    std::lock_guard<std::mutex> lock(s_queueLock);
    s_queue.clear();
    input::reconcile();
}

/* every character OrrePort can draw in a name (*widened 2026-11-15*: before, only printable ASCII and the pages
 * passed, so every input-method commit (kana, kanji, full-width letters) was silently dropped; found from the
 * code after snatcherLeo's typing test): the game font's own glyphs (the US main font has ASCII, Latin-1, kana, full-width
 * letters and a few kanji), then whatever the font renderer draws for letters the game lacks: the Japanese disc's own
 * kanji when the player added it, the open fonts and the system's last-resort fonts (part D). A letter no font has
 * would show as a white box or a '?', so it is never let in (snatcherLeo: "make sure we never have white boxes"). The game's
 * text is 16-bit with no pairs, so characters past U+FFFF (emoji) were left out; so are the game's own symbols at
 * standard code points (game_symbol) and private-use codes.
 * *Changed 2026-11-16 (the Noto wiring; decided 2026-11-15, snatcherLeo: "Yes with emoji"):* characters past U+FFFF are let in
 * when a shipped font draws them (Noto Color Emoji, Noto CJK), carried in the game's text by a stand-in code
 * (pc_font_files.h; name_code below). Typed private-use codes stay out: the stand-ins live there. */
bool drawable(char32_t c) {
    if (c >= 0x21 && c <= 0x7E) return true;
    if (on_pages(c)) return true;
    /* *Added 2026-11-16 (shaping, pc_shape.h):* with HarfBuzz on, an emoji's modifiers (a skin tone, a variation
     * selector, the zero-width joiner, tags, the keycap) are kept: the whole sequence draws as one picture (pc_names.cpp's
     * shown). ~~Each still takes a letter of the 12 (the name is stored as typed, one code per character)~~
     * *Corrected 2026-11-16 (snatcherLeo: the 12 letters are visible letters):* the name is still stored one code per
     * character, but a modifier rides on its emoji's letter and takes none of the 12 (add_unit, pc_grapheme.h) */
    if (pc_text_is_modifier((uint32_t)c) && pc_shape_enabled()) return true;
    if (c > 0xFFFF) return c <= 0x10FFFF && !pc_text_is_modifier((uint32_t)c) && pc_font_render_can_draw_cp((uint32_t)c) != 0;
    if (c < 0xA0 || (c >= 0xD800 && c <= 0xF8FF) || game_symbol((uint32_t)c) || pc_text_is_modifier((uint32_t)c)) return false;
    if (pc_game_ui.font_has != nullptr && pc_game_ui.font_has(0, (uint16_t)c) == 1) return true;
    return pc_font_render_can_draw((uint16_t)c) != 0;
}

/* UTF-8 to code points (typed text).
 * *Updated 2026-11-16:* an emoji sequence keeps only its first emoji (pc_font_files.cpp, pc_text_is_modifier, says
 * why): variation selectors, skin tones and the like are dropped, and a zero-width joiner drops itself and the
 * character after it (👨‍👩‍👧 types as 👨, one letter) */
void decode_utf8(const std::string& in, std::vector<char32_t>& out) {
    /* *Changed 2026-11-16 (shaping):* with HarfBuzz on, every character is kept as typed (drawable lets the modifiers
     * in) and the sequence is joined at drawing time; the rule below is the fallback with ORRE_SHAPING=0 */
    const bool keepAll = pc_shape_enabled() != 0;
    bool joined = false;
    for (size_t i = 0; i < in.size();) {
        const unsigned char b = (unsigned char)in[i];
        char32_t c;
        int n;
        if (b < 0x80) c = b, n = 1;
        else if (b >= 0xF0) c = b & 0x07, n = 4;
        else if (b >= 0xE0) c = b & 0x0F, n = 3;
        else c = b & 0x1F, n = 2;
        for (int k = 1; k < n && i + k < in.size(); ++k) c = (c << 6) | ((unsigned char)in[i + k] & 0x3F);
        i += n;
        if (keepAll) {
            out.push_back(c);
            continue;
        }
        if (c == 0x200D) {
            joined = true;
            continue;
        }
        if (pc_text_is_modifier((uint32_t)c)) continue;
        if (joined) {
            joined = false;
            continue;
        }
        out.push_back(c);
    }
}

/* the code a drawable character takes in the game's text: itself, or a stand-in past U+FFFF (0: the stand-in table is
 * full, so it is left out) */
uint32_t name_code(char32_t c) { return c > 0xFFFF ? pc_text_standin((uint32_t)c) : (uint32_t)c; }

/* ---- editing the name (the same rules as the game's selectLetter and its delete) ---- */
/* stage 3 (2026-11-14): letters go into OrrePort's own 12-letter name; the game's buffer gets its GameCube name */
void type_code(const PcNameEntryView& v, uint32_t code) {
    /* full: the game would overwrite the last letter; B keeps it (the cursor is on DONE). *2026-11-16:* full means 12
     * visible letters (add_unit) */
    if (add_unit(unit_of(code)) != PC_GRAPHEME_ADDED) return;
    sync_game(v);
    s.shift = false;
    pc_game_ui.sound_se(0x24);
    if (letters() >= kNameMax) { /* full now: the cursor jumps to DONE, as the game's does */
        *v.letter = kButtons;
        *v.column = 3;
    }
}
/* typed text from the PC keyboard or the IME's commit (SDL's text input): every drawable letter in order, the game's
 * letter sound once, the cursor (the green next-slot bar) after them. What doesn't fit the 12 letters (snatcherLeo's design,
 * state 4 "Too long"): the letters that fit are kept, the field flashes red, the game's error sound plays and a line
 * under the field says how many fit, for about 2 seconds; nothing is dropped silently.
 * *Changed 2026-11-16 (snatcherLeo: the 12 letters are visible letters, with at most 10 hidden parts each; pc_grapheme.h):*
 *   - a character that rides on the last letter (a mark, a skin tone, the joiner and the emoji after it) takes none of
 *     the 12, so "👨‍👩‍👧" is one letter and twelve families fit;
 *   - once a character would start a 13th letter, it and everything after it in this text are refused: the rest of
 *     that letter (a family's joiners and emoji) must not ride on the 12th letter instead;
 *   - a part past the 10th on one letter is dropped (the safeguard against "Zalgo" text); the field flashes and the
 *     sound plays, but without the "Only 12 letters fit" line, which would give the wrong reason (the helper's choice;
 *     to show snatcherLeo). */
void type_text(const PcNameEntryView& v, const std::string& utf8) {
    std::vector<char32_t> cps;
    decode_utf8(utf8, cps);
    int typed = 0, lost = 0, capped = 0;
    for (char32_t cp : cps) {
        uint32_t code;
        if (cp == 0x20 || cp == 0x3000) code = game_space();
        else if (drawable(cp)) code = name_code(cp);
        else continue; /* a letter no font has: never a white box (drawable says why) */
        if (code == 0) continue;
        if (lost > 0) { /* past the 12th letter: the rest of the text too */
            lost++;
            continue;
        }
        const int r = add_unit(unit_of(code));
        if (r == PC_GRAPHEME_ADDED) typed++;
        else if (r == PC_GRAPHEME_FULL) lost++;
        else capped++;
    }
    if (typed > 0) {
        sync_game(v);
        s.shift = false;
        pc_game_ui.sound_se(0x24);
        if (letters() >= kNameMax) { /* full now: the cursor jumps to DONE, as the game's does */
            *v.letter = kButtons;
            *v.column = 3;
        }
    }
    if (lost > 0 || capped > 0) {
        ime.flash_at = SDL_GetTicks();
        ime.flash_full = lost > 0;
        if (capped > 0) std::printf("[name_keyboard] %d hidden parts past %d on one letter dropped\n", capped, kHiddenMax);
        /* 0x26: likely the game's "can't do that" sound (it plays when a trade is refused, gbaCommunication.c, and on
         * other refusals; the screen's own sounds are 0x23-0x27, naming-screen.md); snatcherLeo's ear decides */
        pc_game_ui.sound_se(0x26);
    }
}

/* the IME's composition as the name field shows it: drawable letters only (the commit keeps only those too) */
void set_composition(const std::string& utf8, int caret) {
    std::vector<char32_t> cps;
    decode_utf8(utf8, cps);
    ime.comp.clear();
    for (char32_t cp : cps) {
        if (cp == 0x20 || cp == 0x3000) ime.comp.push_back(unit_of(game_space()));
        else if (drawable(cp) && name_code(cp) != 0) ime.comp.push_back(unit_of(name_code(cp)));
    }
    ime.caret = caret;
}

/* the IME's choices: SDL's Windows strings are "<number> <choice>" (its IME_AddCandidate); the number is kept as the
 * row's label, the choice as the game's text. An empty list closes it */
void set_choices(const std::vector<std::string>& list, int selected) {
    ime.cands.clear();
    ime.labels.clear();
    if (list.empty()) {
        ime.selected = -1;
        ime.page_size = 0;
        return;
    }
    for (const std::string& item : list) {
        size_t cut = 0;
        while (cut < item.size() && item[cut] >= '0' && item[cut] <= '9') cut++;
        std::string label, text = item;
        if (cut > 0 && cut < item.size() && item[cut] == ' ') label = item.substr(0, cut), text = item.substr(cut + 1);
        std::vector<char32_t> cps;
        decode_utf8(text, cps);
        std::vector<uint16_t> units;
        for (char32_t cp : cps)
            if (drawable(cp) && name_code(cp) != 0) units.push_back(unit_of(name_code(cp)));
        ime.cands.push_back(units);
        ime.labels.push_back(label);
    }
    ime.selected = selected;
    ime.page_size = std::max(ime.page_size, (int)list.size());
}

/* *Changed 2026-11-16 (snatcherLeo: what you see is what goes):* Backspace (and B, a right click, the BACK button) removes the
 * last visible letter whole: the family emoji, the flag, the letter with all its marks, the Hindi conjunct with its
 * vowel sign. Before, it removed one character, so a family took five presses through half-built sequences (👨‍👩‍,
 * 👨‍👩, ...) and a pointed Hebrew letter lost its points one by one. Inside an input method's composition Backspace
 * still belongs to the IME (its own editing, unchanged). Platforms: all. */
void delete_last(const PcNameEntryView& v) {
    if (s.len <= 0) return;
    const std::vector<uint32_t> cps = real_cps(s.full, s.len);
    s.len = pc_grapheme_last(cps.data(), s.len);
    s.full[s.len] = 0;
    sync_game(v);
    pc_game_ui.sound_se(0x25);
}
/* the game's "done": inputName takes over. The keyboard closes only this way (B never cancels), so forgetting the
 * window here makes the next opening start fresh: the game reopens the same window record after "Is <name> OK?" -> NO,
 * and snatcherLeo asked for the Aa page then, as a new save shows it (2026-11-14: "should this just take us back to "Aa"
 * keyboard?") */
void finish(void* window) {
    static_cast<uint8_t*>(window)[0x98] = 1;
    s.window = nullptr;
    stop_typing(); /* the keyboard is a controller again for "Is <name> OK?" and everything after */
}

/* ---- the cursor (left / right wrap round through the button column, up / down stop at the edges, as snatcherLeo's
 * research note asked; the button column keeps the game's own three stops: SHIFT 0, BACK 1, DONE 3) ---- */
void move(const PcNameEntryView& v, int dx, int dy) {
    int x = *v.letter, y = *v.column;
    char32_t c;
    if (x == kButtons) {
        if (dy > 0) y = (y == 0) ? 1 : 3;
        if (dy < 0) y = (y == 3) ? 1 : 0;
        if (dx > 0) x = 0;
        if (dx < 0) x = last_pos(v, y == 0 ? 0 : y);
    } else {
        if (dx > 0) {
            do x++;
            while (x <= last_pos(v, y) && key_at(v, y, x, &c) == 0);
            if (x > last_pos(v, y)) x = kButtons;
        }
        if (dx < 0) {
            do x--;
            while (x >= 0 && key_at(v, y, x, &c) == 0);
            if (x < 0) x = kButtons;
        }
        if (dy != 0) {
            const int ny = y + dy;
            if (ny >= 0 && ny <= 3) {
                y = ny;
                if (x > last_pos(v, y)) x = last_pos(v, y);
                if (key_at(v, y, x, &c) == 0) x = last_pos(v, y); /* a gap on the bottom line: SPACE */
            }
        }
    }
    if (x == kButtons && y == 2) y = 1; /* the game's BACK has two stops (1 and 2); one is enough */
    if (x != *v.letter || y != *v.column) pc_game_ui.sound_se(0x23);
    *v.letter = x;
    *v.column = y;
}

/* a page by number, as L / R and the tabs pick it; the cursor stays on its key, or moves to the line's last key if the
 * new page's line is shorter */
void set_page(const PcNameEntryView& v, int page) {
    s.page = (page + PAGE_COUNT) % PAGE_COUNT;
    s.shift = false;
    pc_game_ui.sound_se(0x27);
    char32_t c;
    if (*v.letter != kButtons && key_at(v, *v.column, *v.letter, &c) == 0) *v.letter = last_pos(v, *v.column);
}

/* A on what the cursor is on (the gamepad's A, a click, a tap); true when that finished the name */
bool press(void* window, const PcNameEntryView& v) {
    const int x = *v.letter, y = *v.column;
    if (x == kButtons) {
        if (y == 0) {
            s.shift = !s.shift;
            pc_game_ui.sound_se(0x27);
        } else if (y == 3) {
            finish(window);
            return true;
        } else {
            delete_last(v);
        }
        return false;
    }
    char32_t c = 0;
    const int what = key_at(v, y, x, &c);
    if (what == 1) type_code(v, (uint32_t)c);
    if (what == 2) type_code(v, game_space());
    return false;
}

/* what is under a point of the 640 x 480 screen (the mouse). Keys: their line's 35 px band from just above the letters,
 * and half a key's pitch either side of the centre (the bottom line's SPACE: its cursor box); the buttons: the game's
 * three panels (sprite 0x7E, 123 x 47 at x 498, y 266 / 322 / 375); the strip under the grid: where draw_label printed
 * L, the tabs and R this frame. 1 a key or a button (*px, *line in the game's cursor coordinates), 2 the strip (*px:
 * 0 L, 1-3 the tabs, 4 R), 0 nothing */
const int kGridY = 283; /* the grid item's y (its x is kItemX) */
const int kStripY = 420; /* the R hint item's place (219, 420) */
const int kStripX = 219;
int hit(const PcNameEntryView& v, int gx, int gy, int* px, int* line) {
    static const int kPanelY[3] = {266, 322, 375}, kPanelStop[3] = {0, 1, 3};
    if (gx >= 498 && gx < 498 + 123)
        for (int i = 0; i < 3; i++)
            if (gy >= kPanelY[i] && gy < kPanelY[i] + 47) {
                *px = kButtons;
                *line = kPanelStop[i];
                return 1;
            }
    if (gy >= kStripY - 6 && gy < kStripY + 28)
        for (int i = 0; i < PAGE_COUNT + 2; i++)
            if (s.strip_x1[i] > s.strip_x0[i] && gx >= s.strip_x0[i] - 8 && gx < s.strip_x1[i] + 8) {
                *px = i;
                return 2;
            }
    const int top = kGridY - 4;
    if (gy < top || gy >= top + 35 * 4) return 0;
    const int l = (gy - top) / 35;
    if (l == 3 && std::abs(gx - (kItemX + kSpaceCentre)) <= kSpaceWidth / 2 + 4) {
        *px = kSpacePos;
        *line = 3;
        return 1;
    }
    const int rel = gx - kItemX - kLineX0[l];
    const int p = (int)std::lround(rel / (double)kPitch);
    char32_t c;
    if (p < 0 || std::abs(rel - kPitch * p) > kPitch / 2 || key_at(v, l, p, &c) != 1) return 0;
    *px = p;
    *line = l;
    return 1;
}

/* ---- the hooks ---- */
void draw_choices(void* window, int ox, int oy); /* the IME's choice box (below, with the name field) */
void ime_test_frame();                           /* ORRE_IME_TEST (below) */

s32 input_hook(void* window) {
    if (!enabled()) return 0;
    const PcNameEntryView v = view_of(window);
    fresh(window, v);
    start_typing(); /* the PC keyboard types from now until DONE (stage 2) */
    if (s_atlasLang != pc_game_text_language()) /* the IME parts' words, once per language */
        build_atlas(ime_texts(pc_game_text_language()), pc_game_text_language());
    ime_test_frame(); /* ORRE_IME_TEST: fake IME events through the same path (a test switch) */
    const uint8_t* keys = pc_game_ui.window_keys();
    uint16_t pressed, repeat;
    std::memcpy(&pressed, keys + 4, 2); /* the key record (menu.c MenuKeyInfo): +4 newly pressed, +6 repeat */
    std::memcpy(&repeat, keys + 6, 2);

    /* what the PC keyboard did since the last frame (typed text, Backspace, Enter, Tab, the arrows) */
    std::deque<KbCmd> cmds;
    {
        std::lock_guard<std::mutex> lock(s_queueLock);
        cmds.swap(s_queue);
    }
    for (const KbCmd& c : cmds) {
        s.keys_look = true;
        switch (c.cmd) {
        case Cmd::Text: type_text(v, c.text); break;
        case Cmd::Compose: set_composition(c.text, c.x); break;
        case Cmd::Choices: set_choices(c.list, c.x); break;
        case Cmd::Delete: delete_last(v); break;
        case Cmd::Done: finish(window); return 1;
        case Cmd::PageNext:
        case Cmd::PagePrev: set_page(v, s.page + (c.cmd == Cmd::PageNext ? 1 : -1)); break;
        case Cmd::Left: move(v, -1, 0); break;
        case Cmd::Right: move(v, 1, 0); break;
        case Cmd::Up: move(v, 0, -1); break;
        case Cmd::Down: move(v, 0, 1); break;
        case Cmd::Hover:
        case Cmd::Click: {
            if (c.cmd == Cmd::Click && c.button == SDL_BUTTON_RIGHT) {
                delete_last(v);
                break;
            }
            int px = 0, line = 0;
            const int what = hit(v, c.x, c.y, &px, &line);
            if (what == 1 && (px != *v.letter || line != *v.column)) { /* the cursor follows the pointer */
                *v.letter = px;
                *v.column = line;
                if (c.cmd == Cmd::Hover) pc_game_ui.sound_se(0x23);
            }
            if (c.cmd != Cmd::Click || c.button != SDL_BUTTON_LEFT) break;
            if (what == 1 && press(window, v)) return 1;
            if (what == 2) set_page(v, px == 0 ? s.page - 1 : px == PAGE_COUNT + 1 ? s.page + 1 : px - 1);
            break;
        }
        }
    }
    if (pressed != 0 || repeat != 0) s.keys_look = false; /* the gamepad again: its look comes back */
    show_icons(window, !s.keys_look);                     /* the game's X / B / START icons only with the pad */

    if (pressed & MENU_KEY_START) {
        finish(window);
    } else if (pressed & MENU_KEY_A) {
        press(window, v);
    } else if (pressed & MENU_KEY_B) {
        delete_last(v);
    } else if (pressed & MENU_KEY_X) {
        s.shift = !s.shift;
        pc_game_ui.sound_se(0x27);
    } else if (pressed & MENU_KEY_Y) {
        type_code(v, game_space());
    } else if (pressed & (MENU_KEY_L | MENU_KEY_R)) {
        set_page(v, s.page + ((pressed & MENU_KEY_R) ? 1 : -1));
    } else {
        if (repeat & MENU_KEY_RIGHT) move(v, 1, 0);
        if (repeat & MENU_KEY_LEFT) move(v, -1, 0);
        if (repeat & MENU_KEY_DOWN) move(v, 0, 1);
        if (repeat & MENU_KEY_UP) move(v, 0, -1);
    }
    return 1;
}

s32 draw_grid(void* window) {
    if (!enabled()) return 0;
    const PcNameEntryView v = view_of(window);
    const uint32_t alpha = static_cast<uint8_t*>(window)[0x8b];
    const uint32_t white = 0xFFFFFF00u | alpha;
    for (int line = 0; line < 4; line++) {
        const char32_t* t = line_text(v, line);
        for (int pos = 0; t[pos] != 0 && !(line == 3 && pos >= kSpacePos); pos++) {
            uint16_t one[2];
            put_be(&one[0], (uint32_t)t[pos]);
            one[1] = 0;
            print_centred(one, kLineX0[line] + kPitch * pos, kLineY[line], white);
        }
    }
    /* the SPACE key: the Y badge, then the word, centred together on the key. The badge sits on the letters' middle
     * (the template prints a capital about 7 px below its y, 15 px tall) */
    uint16_t word[16];
    utf8_to_game(pc_text_lang(pc_game_text_language(), "game.name_entry.space"), word, 16); /* the game's language */
    const int w = (int16_t)((uint32_t)pc_game_ui.measure_gs(word) >> 16);
    /* with the gamepad the Y badge, with the PC keyboard the blank space-bar cap (stage 2; mockup 98d) */
    const bool cap = s.keys_look;
    const bool badge = cap ? build_space_cap() : build_badge();
    const int iconW = cap ? kCapW : kBadgeW, iconY = cap ? kLineY[3] + 8 : kLineY[3] + 2;
    const int gap = 5;
    int x = kSpaceCentre - (w + (badge ? iconW + gap : 0)) / 2;
    if (badge) {
        pc_game_ui.draw_sprite((int16_t)x, (int16_t)iconY, window, cap ? kSpriteSpace : kSpriteY);
        x += iconW + gap;
    }
    pc_game_ui.print_gs(x, kLineY[3], white, word);
    return 1;
}

s32 cursor_box(void* window, s32* x, s32* y, f32* width, f32* height) {
    (void)height;
    if (!enabled()) return 0;
    const PcNameEntryView v = view_of(window);
    const int px = *v.letter, line = *v.column;
    if (px == kButtons || line < 0 || line > 3) return 2; /* the game highlights its own buttons */
    int centre = kItemX + kLineX0[line] + kPitch * px;
    if (line == 3 && px == kSpacePos) {
        centre = kItemX + kSpaceCentre;
        *width = (f32)kSpaceWidth;
    }
    *x = (s32)(centre - (int)(*width) / 2);
    return 1; /* y stays the game's: its lines are 35 px apart, as ours */
}

s32 draw_label(s32 which, void* window, u8* draw) {
    if (!enabled()) return 0;
    const uint32_t alpha = static_cast<uint8_t*>(window)[0x8b];
    if (which == 2 || which == 3) { /* BACK and DONE: the icon cleaned off and the key cap; the game prints its label */
        draw_key_cap(window, which - 1);
        return 0;
    }
    if (which == 0) draw_key_cap(window, 0);
    if (which == 0) {
        /* the X button says what X will do (snatcherLeo, 2026-11-14, after "SHIFT on a capital keyboard, gets lowercase...?
         * Normally this wouldn't make sense"; mockup 98): on the letter pages the game's own words for its X label,
         * "lower" (message 0x2EF1) while capitals show and "UPPER" (0x2EF0) while small letters show, already in
         * every language the game has; on #?! "MORE" (OrrePort's word: the second set and back). Bottom-aligned as
         * the game's label; green while Shift is on. With the PC keyboard (stage 2) a SHIFT key cap sits beside it */
        if (draw == nullptr) return 0;
        const PcNameEntryView v = view_of(window);
        int16_t bottom;
        std::memcpy(&bottom, draw + 0x56, 2);
        const uint32_t color = (s.shift ? 0x46E64600u : 0xFFFFFF00u) | alpha;
        if (s.page == PAGE_SYMBOLS) {
            uint16_t word[16];
            utf8_to_game(pc_text_lang(pc_game_text_language(), "game.name_entry.more"), word, 16);
            const int32_t rect = pc_game_ui.measure_gs(word);
            pc_game_ui.print_gs(0, bottom - (int16_t)(rect & 0xFFFF), color, word);
        } else {
            const uint32_t key = set_now(v) == 0 ? 0x2EF1u : 0x2EF0u;
            pc_game_ui.msg_print(0, bottom - (int16_t)(pc_game_ui.msg_rect(key) & 0xFFFF), (int32_t)color, key);
        }
        return 1;
    }
    /* the R hint's place under the grid: L, the three page tabs (the open one in the game's own strip colour), R. L and
     * R are drawn in dark grey so they read as the buttons that switch pages, not as more tabs (snatcherLeo, 2026-11-14: "have
     * "L" and "R" be differently colored, so they're distinct") */
    const uint32_t on = 0x00509100u | alpha, off = 0x8CA0B400u | alpha, button = 0x3C424A00u | alpha;
    int x = 8; /* a little right of the hint's own place, centred better on the strip (snatcherLeo, 2026-11-14) */
    const char32_t* parts[PAGE_COUNT + 2] = {U"L", kTabs[0], kTabs[1], kTabs[2], U"R"};
    for (int i = 0; i < PAGE_COUNT + 2; i++) {
        uint16_t text[8];
        int n = 0;
        for (const char32_t* c = parts[i]; *c != 0 && n < 7; c++) put_be(&text[n++], (uint32_t)*c);
        text[n] = 0;
        const bool tab = i >= 1 && i <= PAGE_COUNT;
        const uint32_t color = !tab ? button : (s.page == i - 1 ? on : off);
        pc_game_ui.print_gs(x, 0, color, text);
        const int w = (int16_t)((uint32_t)pc_game_ui.measure_gs(text) >> 16);
        s.strip_x0[i] = kStripX + x; /* for the mouse (hit) */
        s.strip_x1[i] = kStripX + x + w;
        x += w + 22;
    }
    draw_choices(window, kStripX, kStripY); /* the IME's choices, over the keyboard panel (state 2) */
    return 1;
}

s32 preview(void* window, u16* letter) {
    if (!enabled()) return 0;
    const PcNameEntryView v = view_of(window);
    char32_t c = 0;
    *letter = 0;
    /* no preview while typing on the PC keyboard: the letter under the grid cursor isn't what the next key types */
    if (!s.keys_look && *v.letter != kButtons && key_at(v, *v.column, *v.letter, &c) == 1) {
        uint16_t one;
        put_be(&one, (uint32_t)c); /* the game holds the letter as its two bytes, read in memory order */
        *letter = one;
    }
    return 1;
}

/* ---- stage 3: the 12-slot name field (snatcherLeo, 2026-11-14: mockup 103, "Yes, build it") ----
 * Every name screen shows the game's widest field, the nickname screen's frame (layout 10: frame pieces 0x12C1-0x12C5),
 * with 12 slots 21.5 px apart instead of the game's 26, ending before the Pokémon's gender sign (item 0x40D at x 551).
 * The game's 24 x 3 underscore pieces and its green next-slot bar are hidden, and the same pictures (menu sprites 0x83
 * white, 0x82 green) are drawn here 19 px wide, the green one with the game's own pulse. The letters print with the
 * screen's one-letter template, centred on their bar. Positions are relative to the layout-10 text item (0x12D1, at
 * 290, 157): its bars sit at y 183, the first at x 289. */
const int kSlotPitch2 = 43; /* twice the pitch: 21.5 px */
const int kBarW = 19, kBarH = 3, kBarY = 183 - 157, kBarX0 = 289 - 290;

int slot_x(int i) { return kBarX0 + (i * kSlotPitch2) / 2; }

/* ---- the input method's parts around the name field (the IME work, 2026-11-15; snatcherLeo approved the design: the
 * mockup's four states, Typing / Choosing / Confirmed / Too long). Places in the screen's 640 x 480 coordinates,
 * measured on frame dumps of the field (layout 10's frame) ---- */
const int kItemNameX = 290, kItemNameY = 157;                 /* the name item's place (layout 10's text item) */
const int kFieldX = 276, kFieldY = 146, kFieldW = 296, kFieldH = 50; /* the field's dark inside */
const uint32_t kComposeBar = 0xFFD64000u;                     /* composing letters' bars: yellow (255, 214, 64) */
const int kChoiceRows = 3, kChoiceRowH = 26;                  /* the choice box shows 3 rows at a time (the mockup) */
const int kChoiceX = 288, kChoiceY = 199;                     /* its top-left, under the field's left end */

/* the "IME on" tag's mark: the input language's first letter, as the IMEs' own indicators show it (あ Japanese, 가
 * Korean, 中 Chinese). Windows says whether the IME is on (pc_ime_win32_status); elsewhere the tag shows while a
 * composition exists, its mark from the composition's script. "" when no tag */
bool s_testImeOn = false; /* ORRE_IME_TEST's "ime_on" step (a test run has no system IME to ask) */
std::string ime_mark() {
    unsigned lang = 0;
    bool on = !ime.comp.empty() || s_testImeOn;
#ifdef _WIN32
    on = pc_ime_win32_status(&lang) != 0 || on;
#endif
    if (!on) return "";
    if (lang == 0x12) return "\xEA\xB0\x80"; /* 가 */
    if (lang == 0x04) return "\xE4\xB8\xAD"; /* 中 */
    if (lang == 0x11) return "\xE3\x81\x82"; /* あ */
    const uint32_t c = ime.comp.empty() ? 0 : code_of(ime.comp[0]);
    if (c >= 0xAC00 && c <= 0xD7A3) return "\xEA\xB0\x80";
    if (c >= 0x4E00 && c <= 0x9FFF) return "\xE4\xB8\xAD";
    return "\xE3\x81\x82";
}

/* the tag above the field's right end and the one line under the field (relative to the name item) */
void draw_ime_words(void* window, uint32_t alpha, bool done) {
    const char* lang = pc_game_text_language(); /* the keyboard's words follow the game's language, as SPACE does */
    const std::string mark = done ? "" : ime_mark();
    if (!mark.empty()) {
        const std::string tag = tx_lang(lang, "game.name_entry.ime_tag", {{"mark", mark}});
        const int w = text_width(tag, kWordPx) + 8, h = 14;
        const int x = kFieldX + kFieldW - 4 - w - kItemNameX, y = kFieldY - h - 4 - kItemNameY;
        const uint32_t pill = 0xDCE0E600u | (alpha * 230 / 255);
        pc_game_ui.fill_rect((int16_t)(x + 2), (int16_t)y, (int16_t)(w - 4), (int16_t)h, pill, pill);
        pc_game_ui.fill_rect((int16_t)x, (int16_t)(y + 2), (int16_t)w, (int16_t)(h - 4), pill, pill);
        pc_game_ui.fill_rect((int16_t)(x + 1), (int16_t)(y + 1), (int16_t)(w - 2), (int16_t)(h - 2), pill, pill);
        draw_text(window, tag, kWordPx, x + w / 2, y + 1, 0x28303800u | alpha, true);
    }
    if (done) return;
    const int lineY = kFieldY + kFieldH + 6 - kItemNameY, centre = kFieldX + kFieldW / 2 - kItemNameX;
    const uint64_t since = SDL_GetTicks() - ime.flash_at;
    if (ime.flash_at != 0 && since < 2000 && ime.flash_full) { /* state 4: how many fit, for about 2 seconds */
        const std::string why = tx_lang(lang, "game.name_entry.ime_too_long", {{"n", std::to_string(kNameMax)}});
        draw_text(window, why, kWordPx, centre, lineY, 0xFFD0D000u | alpha, true);
    } else if (!ime.comp.empty() && ime.cands.empty()) { /* state 1: the IME's two keys */
        draw_text(window, tx_lang(lang, "game.name_entry.ime_help"), kWordPx, centre, lineY, 0xFFFFFF00u | alpha, true);
    }
}

/* the game's own list pointer (the white ball with the red arrow of its YES / NO box) on the selected choice: menu
 * sprite 0xBC, the cursor sprite of 35 menu items (found with ORRE_IME_TEST's "pointer" steps: 0x34, 0x6F, 0x148 and
 * 0x243, the other most used cursor sprites, are other shapes; verified on frame dumps). At its own size (its record's
 * +0x0C / +0x0E, big-endian). Drawn as a cursor it lands 43 px left of the place given (measured on a dump; likely
 * the sprite's own cursor offset, which puts the game's pointer left of its item), so x is the pointer's left edge */
u32 s_pointerSprite = 0xBC; /* ORRE_IME_TEST's "pointer" step may change it (a test aid) */
constexpr int kPointerDX = 43;
void draw_pointer(void* window, int x, int y, uint32_t alpha) {
    const u8* rec = static_cast<const u8*>(pc_game_ui.menu_sprite(s_pointerSprite));
    if (rec == nullptr) return;
    const int w = rec[0x0C] << 8 | rec[0x0D], h = rec[0x0E] << 8 | rec[0x0F];
    pc_game_ui.draw_sprite_sized((int16_t)(x + kPointerDX), (int16_t)(y + (18 - h) / 2), (int16_t)w, (int16_t)h,
                                 0xFFFFFF00u | alpha, window, s_pointerSprite);
}

/* state 2: the IME's choices under the field, in the game's window style: the "O.K.?" box of Battle Mode
 * (fn_80063AD4: its two slate colours as a gradient, faint white lines every 4 pixels) inside a grey rounded frame,
 * one numbered row per choice in the game's letters, a pointer on the IME's selection and dots for where the 3 shown
 * rows sit in the page. Only shows what the IME chose: the IME itself handles the keys that move and pick (Space,
 * the arrows, the numbers). Drawn from the item drawn last on the screen (the R hint's, at ox / oy), so the box sits
 * over the keyboard panel's top edge, never over the name */
void draw_choices(void* window, int ox, int oy) {
    if (ime.cands.empty() || !enabled()) return;
    const uint32_t alpha = static_cast<uint8_t*>(window)[0x8b];
    const int n = (int)ime.cands.size();
    const int P = std::max(ime.page_size, n);
    /* SDL 3.4 on Windows gives the selection's index in the whole list, the strings one page (verified in its
     * source): the row is that index within its page (the page size is the longest page seen; likely right) */
    int sel = ime.selected;
    if (sel >= n && P > 0) sel %= P;
    if (sel >= n) sel = -1;
    int first = std::max(0, std::min(sel - 1, n - kChoiceRows));
    if (sel < 0) first = 0;
    const int rows = std::min(n, kChoiceRows);
    int textW = 0;
    for (int i = 0; i < rows; i++) {
        std::vector<uint16_t> t = ime.cands[(size_t)(first + i)];
        t.push_back(0);
        textW = std::max(textW, (int)(int16_t)((uint32_t)pc_game_ui.measure_gs(t.data()) >> 16));
    }
    const int w = std::max(190, 56 + textW + 18), h = rows * kChoiceRowH + 8; /* pointer, number, choice, dots */
    const int x = kChoiceX - ox, y = kChoiceY - oy;
    /* the frame: light grey, its corners cut (two crossed boxes), a darker line inside it */
    const uint32_t frame = 0xC4C8D000u | alpha, edge = 0x6A707A00u | alpha;
    pc_game_ui.fill_rect((int16_t)(x - 3), (int16_t)(y - 1), (int16_t)(w + 6), (int16_t)(h + 2), frame, frame);
    pc_game_ui.fill_rect((int16_t)(x - 1), (int16_t)(y - 3), (int16_t)(w + 2), (int16_t)(h + 6), frame, frame);
    pc_game_ui.fill_rect((int16_t)(x - 1), (int16_t)(y - 1), (int16_t)(w + 2), (int16_t)(h + 2), edge, edge);
    /* the slate: the game's own colours with their own alpha scaled by the window's fade, by value (fn_80063AD4) */
    auto faded = [&](uint32_t c) { return (c & 0xFFFFFF00u) | ((c & 0xFF) * alpha / 255); };
    pc_game_ui.fill_rect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, faded(pc_game_ui.slate_color(0)),
                         faded(pc_game_ui.slate_color(1)));
    for (int ly = 0; ly < h; ly += 4) pc_game_ui.draw_hline((int16_t)x, (int16_t)(y + ly), (int16_t)w, 0xFFFFFF00u | (56 * alpha / 255));
    for (int i = 0; i < rows; i++) {
        const int k = first + i, ry = y + 4 + i * kChoiceRowH;
        if (k == sel) draw_pointer(window, x + 3, ry + 4, alpha);
        if (!ime.labels[(size_t)k].empty())
            draw_text(window, ime.labels[(size_t)k], kNumberPx, x + 41, ry + 9, 0xB4BEC800u | alpha, true);
        std::vector<uint16_t> t = ime.cands[(size_t)k];
        t.push_back(0);
        pc_game_ui.print_gs(x + 50, ry - 4, (k == sel ? 0xFFFFFF00u : 0xD2DCE600u) | alpha, t.data());
    }
    /* dots on the right: one per group of 3 rows in this page, the shown group bright */
    const int groups = (n + kChoiceRows - 1) / kChoiceRows;
    if (groups > 1) {
        const int shown = first / kChoiceRows + (first % kChoiceRows != 0 ? 1 : 0);
        for (int g = 0; g < groups; g++) {
            const uint32_t dot = (g == std::min(shown, groups - 1) ? 0xFFFFFF00u | alpha : 0x8C96A000u | (alpha * 160 / 255));
            pc_game_ui.fill_rect((int16_t)(x + w - 8), (int16_t)(y + 6 + g * 7), 3, 3, dot, dot);
        }
    }
}

s32 name_piece(s32 piece, s32 type, void* window, u8* draw) {
    (void)window;
    if (!enabled()) return 0;
    draw[0x67] = (piece == 0 && type == 10) ? 0xFF : 0; /* the widest frame only; no game underscores */
    return 1;
}

s32 draw_name(void* window, u8* draw, s32 type) {
    if (!enabled()) return 0;
    if (type != 10) {
        draw[0x67] = 0;
        return 1;
    }
    draw[0x67] = 0xFF;
    const PcNameEntryView v = view_of(window);
    const bool done = static_cast<uint8_t*>(window)[0x98] != 0;
    /* *2026-11-15:* not while the window is done ("Is <name> OK?" draws the name field too): re-attaching then made
     * the next opening after NO skip its reset when nothing had been typed (the cursor stayed on "1" instead of Q, the
     * page not back to Aa). The keyboard attaches again when the game reopens it (input_hook). Platforms: all. */
    if (s.window != window && !done) fresh(window, v);
    const uint32_t alpha = static_cast<uint8_t*>(window)[0x8b];
    const uint32_t pulse = pc_game_ui.name_pulse(window);
    /* the IME's composition (state 1 of the approved design): its letters in the next slots, in the game's letters,
     * their bars yellow until kept; the green next-slot bar after them. Letters past the 12th slot aren't shown (the
     * commit keeps only what fits and says so, type_text) */
    /* *Changed 2026-11-16 (visible letters, pc_grapheme.h):* what of the composition shows is decided by the same rules
     * as its commit: its characters follow the name one by one until one would start a 13th letter; parts past the
     * 10th on one letter are left out */
    std::vector<uint16_t> typedUnits(s.full, s.full + s.len);
    std::vector<uint32_t> typedCps = real_cps(s.full, s.len); /* an emoji's stand-in: its real letter */
    if (!done) {
        for (uint16_t u : ime.comp) {
            const uint32_t c = pc_text_real((uint16_t)code_of(u));
            const int r = pc_grapheme_append(typedCps.data(), (int)typedCps.size(), c, kNameMax, kHiddenMax);
            if (r == PC_GRAPHEME_FULL) break;
            if (r == PC_GRAPHEME_CAPPED) continue;
            typedUnits.push_back(u);
            typedCps.push_back(c);
        }
    }
    const int total = (int)typedCps.size();
    const int comp = total - s.len;
    /* *Changed 2026-11-16 (the project's build helper; right-to-left names, pc_rtl.h):* the letters are placed in their drawn
     * order. The typed name and the composition, in typed order, go through pc_rtl_display (Arabic's joined forms, the
     * right-to-left runs reversed); a name that reads from the right (its first strong letter is Arabic or Hebrew) is
     * placed against the field's right end, the slots filling leftwards, and its next-slot bar sits left of it, where
     * the next right-to-left letter lands; any other name fills from the left as before (for it nothing changes: the
     * drawn order is the typed order). Each drawn letter keeps where it came from, so the composition's letters keep
     * their yellow bars and the kept ones the field's flash colour. Typing still adds at the name's end and Backspace
     * still removes the last letter typed (for a right-to-left name, the leftmost drawn one), as in any text field. */
    /* *Changed 2026-11-16 (the project's build helper; HarfBuzz and SheenBidi, pc_shape.h):* a name that needs shaping is
     * shaped as one run (pc_shape_name): each drawn item is a shaped cluster (an Indic syllable, a Thai letter with its
     * marks, an Arabic letter in its joined form, a whole emoji sequence) or a plain letter, in SheenBidi's drawn order,
     * and a cluster made of several typed letters is drawn centred over their slots (the slots stay one per typed letter:
     * Backspace and the 12-letter count are per typed letter). Without shaping (it failed, or ORRE_SHAPING=0): the
     * pc_rtl pass as before, one slot per drawn letter. */
    /* *Changed again 2026-11-16 (visible letters, pc_grapheme.h; snatcherLeo: "the 12-letter limit counts visible letters"):*
     * the slots are one per visible letter, no longer one per typed character: "👨‍👩‍👧" takes one slot, "नमस्ते" three
     * (न, म, स्ते). Drawn items are grouped into pieces by the letters they cover: a shaped run over several letters is
     * one piece centred over their slots (as before), and items that share a letter (the fallback pass draws a mark or a
     * modifier as its own character) are printed together, as one string, centred on that letter's slot. */
    std::vector<uint8_t> starts((size_t)total + 1, 0);
    const int nLetters = pc_grapheme_starts(typedCps.data(), total, starts.data());
    std::vector<int> letterOf((size_t)total + 1, 0), letterStart;
    for (int i = 0, L = -1; i < total; i++) {
        if (starts[(size_t)i]) L++, letterStart.push_back(i);
        letterOf[(size_t)i] = std::max(L, 0);
    }
    std::vector<PcShapedItem> items((size_t)total + 1);
    int drawn = total > 0 ? -1 : 0;
    bool shaped = false;
    if (total > 0 && pc_shape_needed(typedCps.data(), total)) drawn = pc_shape_name(typedCps.data(), total, items.data(), total);
    if (drawn >= 0 && total > 0) {
        shaped = true;
    } else if (total > 0) {
        std::vector<uint32_t> drawnCps((size_t)total);
        std::vector<int> from((size_t)total);
        drawn = pc_rtl_display(typedCps.data(), total, drawnCps.data(), from.data(), 0);
        for (int j = 0; j < drawn; j++) items[(size_t)j] = {drawnCps[(size_t)j], 0, from[(size_t)j], from[(size_t)j]};
    }
    const bool fromRight =
        total > 0 && (shaped ? pc_shape_base_rtl(typedCps.data(), total) : pc_rtl_base_rtl(typedCps.data(), total)) != 0;
    struct Piece {
        int lo, hi;            /* the visible letters it covers (logical numbers) */
        std::vector<int> item; /* its drawn items, in drawn order */
    };
    std::vector<Piece> pieces;
    for (int j = 0; j < drawn; j++) {
        int a = letterOf[(size_t)items[(size_t)j].first], b = letterOf[(size_t)items[(size_t)j].last];
        if (a > b) std::swap(a, b);
        if (!pieces.empty() && a <= pieces.back().hi && b >= pieces.back().lo) {
            Piece& p = pieces.back();
            p.lo = std::min(p.lo, a), p.hi = std::max(p.hi, b);
            p.item.push_back(j);
            continue;
        }
        pieces.push_back({a, b, {j}});
    }
    int used = 0; /* slots the pieces cover */
    for (const Piece& p : pieces) used += p.hi - p.lo + 1;
    used = std::min(used, kNameMax);
    const int firstSlot = fromRight ? kNameMax - used : 0;
    const int next_slot = (done || nLetters >= kNameMax) ? -1 : fromRight ? firstSlot - 1 : used;
    bool composing[kNameMax] = {};
    std::vector<int> pieceSlot(pieces.size());
    for (size_t k = 0, at = (size_t)firstSlot; k < pieces.size(); at += (size_t)(pieces[k].hi - pieces[k].lo + 1), k++) {
        pieceSlot[k] = (int)at;
        /* the composition is the name's end in typed order: a piece's composing letters are its last ones, drawn on its
         * right in a left-to-right name and on its left in a right-to-left one (likely right for a piece of one
         * direction, which every shaped run is) */
        const int span = pieces[k].hi - pieces[k].lo + 1;
        int composingLetters = 0;
        for (int L = pieces[k].lo; L <= pieces[k].hi; L++) composingLetters += letterStart[(size_t)L] >= s.len ? 1 : 0;
        for (int q = 0; q < span && (int)at + q < kNameMax; q++)
            composing[(int)at + q] = fromRight ? q < composingLetters : q >= span - composingLetters;
    }
    const uint64_t since = SDL_GetTicks() - ime.flash_at;
    const bool flashing = ime.flash_at != 0 && since < 600;
    if (flashing) { /* state 4: the field flashes red for a moment, fading out */
        const uint32_t a = (uint32_t)(150 * (600 - since) / 600) * alpha / 255;
        pc_game_ui.fill_rect(kFieldX - kItemNameX, kFieldY - kItemNameY, kFieldW, kFieldH, 0xD0283000u | a, 0xD0283000u | a);
    }
    for (int i = 0; i < kNameMax; i++) {
        const bool next = i == next_slot && !done;
        const uint32_t a = next ? pulse * alpha / 255 : alpha;
        const uint32_t rgb = composing[i] ? kComposeBar : 0xFFFFFF00u;
        pc_game_ui.draw_sprite_sized((int16_t)slot_x(i), (int16_t)kBarY, (int16_t)kBarW, (int16_t)kBarH, rgb | a, window,
                                     next ? 0x82u : 0x83u);
    }
    const uint32_t ink = flashing ? 0xFFC8C800u : 0xFFFFFF00u;
    for (size_t p = 0; p < pieces.size(); p++) {
        if (pieceSlot[p] >= kNameMax) break;
        std::vector<uint16_t> str;
        int k = total; /* the piece's first typed character: kept letters take the flash colour */
        for (int j : pieces[p].item) {
            const PcShapedItem& it = items[(size_t)j];
            k = std::min(k, it.first);
            /* a shaped cluster: its stand-in; unchanged: the typed unit itself (a stand-in stays one); a joined form or
             * mirrored bracket: its own code */
            str.push_back(it.cluster != 0 ? unit_of(it.cluster)
                          : it.cp == typedCps[(size_t)it.first] ? typedUnits[(size_t)it.first] : unit_of(it.cp));
        }
        str.push_back(0);
        const int last = std::min(pieceSlot[p] + (pieces[p].hi - pieces[p].lo), kNameMax - 1);
        print_centred(str.data(), (slot_x(pieceSlot[p]) + slot_x(last)) / 2 + kBarW / 2 + 1, 0,
                      (k < s.len ? ink : 0xFFFFFF00u) | alpha);
    }
    draw_ime_words(window, alpha, done);
    if (comp > 0 || !ime.cands.empty()) return 1; /* no grid preview while the IME composes */
    /* the letter under the grid cursor, faint in the next slot (the game's preview; not while typing on the PC
     * keyboard, where the grid cursor isn't what the next key types) */
    char32_t c = 0;
    if (!done && !s.keys_look && next_slot >= 0 && *v.letter != kButtons && key_at(v, *v.column, *v.letter, &c) == 1) {
        uint16_t one[2];
        put_be(&one[0], (uint32_t)c);
        one[1] = 0;
        print_centred(one, slot_x(next_slot) + kBarW / 2 + 1, 0, 0x00FF0000u | (pulse * alpha / 255));
    }
    return 1;
}

/* the screen stored its name: OrrePort keeps the full name beside it (pc_names.cpp). The keyboard's name counts only
 * when the stored name is its GameCube name (a preset picked from the list, or the game's own screen, stores its own) */
void name_commit(s32 mode, s32 index, const void* stored, void* record) {
    const uint16_t* got = static_cast<const uint16_t*>(stored);
    uint16_t cut[13] = {};
    /* *Changed 2026-11-15 (the IME work):* the GameCube name is the typed name's closest fit, no longer always its
     * first letters, so the stored name is compared with the fit last given to the game (s.gc); when no letter had a
     * GameCube form the game stored its own fallback name (WES, RUI, the current nickname or box name), s.fb */
    auto equal = [](const uint16_t* a, const uint16_t* b) {
        int i = 0;
        for (; a[i] != 0 && b[i] != 0; i++)
            if (a[i] != b[i]) return false;
        return a[i] == b[i];
    };
    bool blank = true; /* a name of only spaces is the game's empty name: its fallback, nothing kept beside it */
    for (int i = 0; i < s.len && blank; i++) blank = code_of(s.full[i]) == game_space();
    const bool mine = !blank && (s.gc_len > 0 ? equal(got, s.gc) : equal(got, s.fb));
    for (int i = 0; got[i] != 0 && i < 12; i++) cut[i] = got[i];
    pc_names_commit(mode, index, record, mine ? s.full : cut, got);
    s.len = 0; /* stored: the full name now lives in pc_names.cpp */
    s.full[0] = 0;
    s.gc_len = 0;
    s.gc[0] = 0;
}

/* ---- ORRE_IME_TEST: fake input-method events for an automated run (a test switch, never a setting; the project's IME
 * work, 2026-11-15) ----
 * Why: a real IME can't be typed into an automated run, yet the composition, the choices, the commit and the "too
 * long" path need frame dumps. The steps are turned into the same Aurora input events SDL's IME produces
 * (TextEditing, TextEditingCandidates, TextInput, KeyChanged) and handed to this keyboard's own input layer
 * (layer_event), so everything after SDL is the real path. Needs ORRE_NAME_KEYBOARD=1 (automated runs keep the game's
 * own keyboard otherwise).
 *   ORRE_IME_TEST=@file:<path> or the steps themselves; steps separated by new lines or ';', '#' starts a comment;
 *   each step "N|what|..." fires N game frames after the keyboard opened:
 *     N|compose|<text>[|<cursor>]       the composition (empty text ends it)
 *     N|choices|<selected>|<c1>|<c2>... the choice list as SDL's Windows code gives it ("1 ウエス"); no items closes it
 *     N|commit|<text>                   the kept text (SDL_EVENT_TEXT_INPUT)
 *     N|key|enter / backspace           a key press
 *     N|ime_on|1 / 0                    the "IME on" tag as if the system's IME were on (test runs have none)
 *     N|pointer|<sprite>                which menu sprite draws the choice pointer (used to find the game's own)
 * All platforms (plain C++ over the events). */
struct ImeStep {
    unsigned frame;
    std::vector<std::string> parts;
};
std::vector<ImeStep> s_imeSteps;
int s_imeTest = -1;        /* -1 not read, 0 off, 1 on */
void* s_imeWindow = nullptr;
unsigned s_imeFrame = 0;   /* game frames since this keyboard window opened */

void ime_test_load() {
    s_imeTest = 0;
    const char* env = SDL_getenv("ORRE_IME_TEST");
    if (env == nullptr || env[0] == 0) return;
    std::string text = env;
    if (text.rfind("@file:", 0) == 0) {
        size_t size = 0;
        void* data = SDL_LoadFile(text.substr(6).c_str(), &size);
        if (data == nullptr) {
            std::printf("[ime_test] can't read %s\n", text.substr(6).c_str());
            return;
        }
        text.assign(static_cast<const char*>(data), size);
        SDL_free(data);
    }
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find_first_of(";\n", pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (const size_t hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        ImeStep st{};
        size_t p = 0;
        while (true) {
            const size_t bar = line.find('|', p);
            st.parts.push_back(line.substr(p, bar == std::string::npos ? std::string::npos : bar - p));
            if (bar == std::string::npos) break;
            p = bar + 1;
        }
        if (st.parts.size() < 2) continue;
        st.frame = (unsigned)std::strtoul(st.parts[0].c_str(), nullptr, 10);
        s_imeSteps.push_back(st);
    }
    s_imeTest = s_imeSteps.empty() ? 0 : 1;
    std::printf("[ime_test] %zu steps\n", s_imeSteps.size());
    /* which sprite the game's lists use for their cursor: the item table's cursor sprites (+0x0C), most used first,
     * for picking draw_pointer's sprite (logged once) */
    if (pc_game_ui.menu_item_count != nullptr && pc_game_ui.menu_item != nullptr) {
        std::map<int, int> use;
        for (uint32_t i = 0; i < pc_game_ui.menu_item_count(); i++) {
            const u8* r = static_cast<const u8*>(pc_game_ui.menu_item((int32_t)i));
            if (r != nullptr && (r[0x0C] | r[0x0D]) != 0) use[r[0x0C] << 8 | r[0x0D]]++;
        }
        std::vector<std::pair<int, int>> top(use.begin(), use.end());
        std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < top.size() && i < 10; i++) {
            const u8* sp = static_cast<const u8*>(pc_game_ui.menu_sprite((uint32_t)top[i].first));
            std::printf("[ime_test] cursor sprite 0x%X used by %d items (%dx%d)\n", top[i].first, top[i].second,
                        sp ? (sp[0x0C] << 8 | sp[0x0D]) : -1, sp ? (sp[0x0E] << 8 | sp[0x0F]) : -1);
        }
    }
}

void ime_test_frame() {
    if (s_imeTest < 0) ime_test_load();
    if (s_imeTest != 1) return;
    if (s.window != s_imeWindow) s_imeWindow = s.window, s_imeFrame = 0;
    ++s_imeFrame;
    using E = input::InputEvent;
    for (const ImeStep& st : s_imeSteps) {
        if (st.frame != s_imeFrame) continue;
        E ev{};
        ev.source = input::keyboard_source();
        const std::string& what = st.parts[1];
        if (what == "compose") {
            ev.payload = E::TextEditing{.text = st.parts.size() > 2 ? st.parts[2] : "",
                                        .start = st.parts.size() > 3 ? std::atoi(st.parts[3].c_str()) : -1,
                                        .length = 0};
        } else if (what == "choices") {
            E::TextEditingCandidates list;
            list.selected = st.parts.size() > 2 ? std::atoi(st.parts[2].c_str()) : -1;
            for (size_t i = 3; i < st.parts.size(); i++) list.candidates.push_back(st.parts[i]);
            ev.payload = std::move(list);
        } else if (what == "commit") {
            ev.payload = E::TextInput{.text = st.parts.size() > 2 ? st.parts[2] : ""};
        } else if (what == "pointer") { /* which menu sprite draws the choice pointer (finding the game's own) */
            s_pointerSprite = (u32)std::strtoul(st.parts.size() > 2 ? st.parts[2].c_str() : "0", nullptr, 0);
            std::printf("[ime_test] keyboard frame %u: pointer sprite 0x%X\n", s_imeFrame, (unsigned)s_pointerSprite);
            continue;
        } else if (what == "ime_on") { /* the "IME on" tag as if the system's IME were on (no IME in a test run) */
            s_testImeOn = st.parts.size() > 2 && st.parts[2] == "1";
            continue;
        } else if (what == "key") {
            const bool enter = st.parts.size() > 2 && st.parts[2] == "enter";
            ev.payload = E::KeyChanged{.keycode = enter ? SDLK_RETURN : SDLK_BACKSPACE, .pressed = true};
        } else {
            continue;
        }
        std::printf("[ime_test] keyboard frame %u (game frame %u): %s\n", s_imeFrame, (unsigned)pc_vi_heartbeat,
                    what.c_str());
        layer_event(ev, nullptr);
    }
}

/* ---- naming rules per kind (2026-11-15; docs/port/naming-screen.md "B on Wes's name menu") ----
 * Why: snatcherLeo's rule (2026-11-15): "For just WES, if user picks a name, but after hitting start, doesn't like it and hits
 * NO, it would boot them back to the screen with the defaults ... If a user hits B once more in [that name menu], they
 * can get back to main menu. For naming RUI, or any other ally character ... users cannot back out of naming RUI, they
 * have to name her something. We would have the special handling for WES, since this is something we clicked on the
 * main menu to get here", and "have this be turned off (this normal flow) when gamecube-accurate mode is turned on".
 * Pokémon and box names stay as the game has them.
 *
 * The rule for each kind of name sits in this one table, so a mod that adds a kind (an ally of its own, through a new
 * name-entry mode) gets the people's default, "must name", without code: */
enum NameMenuB {
    kMenuBLeaves,   /* B on the name menu leaves the screen, back to the caller, when the caller can take it */
    kMenuBMustName, /* B on the name menu does nothing (the game's own: the list's flags turn B off); a name must be
                       chosen */
    kMenuBNoMenu,   /* no name menu: the keyboard opens at once, and after "Is <name> OK?" -> NO it comes back with
                       the typed name (the game's own way, untouched; snatcherLeo's Pokémon run, 2026-11-15) */
};
struct NameKindRule {
    s32 mode;         /* the game's name-entry mode (name-entry.md's table) */
    const char* kind; /* for the log */
    NameMenuB menu_b;
};
const NameKindRule kNameKindRules[] = {
    {0, "player (Wes)", kMenuBLeaves},    /* only New Game opens it; New Game sets OrreHook_NameEntryCaller */
    {1, "partner (Rui)", kMenuBMustName}, /* Phenac City's script: the story needs her name */
    {2, "Pokémon nickname", kMenuBNoMenu},
    {3, "box name", kMenuBNoMenu},
};
const NameKindRule kOtherPerson = {-1, "another person (a mod's)", kMenuBMustName};

const NameKindRule& name_kind_rule(s32 mode) {
    for (const NameKindRule& r : kNameKindRules)
        if (r.mode == mode) return r;
    return kOtherPerson;
}

/* the keyboard keeps the typed name when it reopens only for the kinds without a name menu (Pokémon, boxes); people
 * start blank (fresh() above) */
bool keeps_name_after_no(s32 kind) { return name_kind_rule(kind).menu_b == kMenuBNoMenu; }

/* Like the keyboard (enabled() above), the back-out is OrrePort's own and off in the GameCube-accurate preset (read
 * once per run), where B on the name menu does nothing, as on the console; and off in automated runs, whose recorded
 * presses were made on the console's flow, unless ORRE_NAME_BACK=1 (a test switch, never a setting). It doesn't depend
 * on the keyboard: the name menu is the game's own list in both keyboards. */
bool back_out_enabled() {
    if (pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_NAME_BACK");
    return force != nullptr && force[0] == '1';
}

/* OrreHook_NameEntryListBack, asked once as the name menu opens: may B leave it? Only for a kind whose rule says so
 * AND a caller that said it can take it (OrreHook_NameEntryCaller), so a name menu opened from anywhere else never
 * closes without a name. Anything a previous screen left in the keyboard's state (a name typed before a NO, then B) is
 * forgotten here, so this screen starts as the first one did (the field blank, the Aa page) */
s32 name_list_back(s32 mode, s32 index, u32 caller) {
    const NameKindRule& rule = name_kind_rule(mode);
    const bool on = back_out_enabled();
    const bool leave = on && rule.menu_b == kMenuBLeaves && caller == ORRE_NAME_CALLER_NEW_GAME;
    std::printf("[names] name menu opens: mode %d (%s), index %d, caller %u: %s\n", (int)mode, rule.kind, (int)index,
                (unsigned)caller,
                leave ? "B leaves it (back to the caller)"
                      : (!on ? "B does nothing (off: GameCube-accurate mode or an automated run)"
                             : "B does nothing (this name must be chosen)"));
    if (!leave) return 0;
    s.window = nullptr;
    s.len = 0;
    s.full[0] = 0;
    s.gc_len = 0;
    s.gc[0] = 0;
    s.fallback = nullptr;
    s.fb[0] = 0;
    s.game_buf = nullptr;
    ime = ImeState{};
    return 1;
}

} // namespace

/* "Is <name> OK?" after DONE (pc_names.cpp's insert hook): the game inserts its buffer, the GameCube name; while the
 * typed name is longer and not yet stored, the question shows the full name. 1 and the name in out13 when text is the
 * GameCube cut of the typed name.
 * *Changed 2026-11-16 (visible letters):* `out` holds the whole typed name, up to PC_NAME_MAX_CPS units and the
 * ending 0 (a 12-letter name can be 132 characters now: twelve emoji families are 60); `cap` is its size in units */
extern "C" int pc_name_keyboard_full(const void* text, uint16_t* out, int cap) {
    /* only the game's own name buffer, by its address: the screen prints every letter (the field's, the grid's) as a
     * one-letter insert, and matching by content swapped each letter equal to the name's first for the whole name
     * (snatcherLeo's hand check, 2026-11-14, screenshots 254-256) */
    /* *Changed 2026-11-15 (the IME work):* the GameCube name is the closest fit (not always the name's first letters),
     * and a name with no GameCube letters leaves the game's buffer empty: then the question shows the game's fallback
     * name (inputName's default buffer, its own address), which also stands for the typed name */
    if (s.len == 0 || !enabled() || text == nullptr) return 0;
    const bool buf = s.gc_len > 0 && text == static_cast<const void*>(s.game_buf);
    const bool fb = s.gc_len == 0 && s.fallback != nullptr && text == static_cast<const void*>(s.fallback);
    if (!buf && !fb) return 0;
    bool blank = true; /* only spaces: the game's empty name, its fallback shows as it is */
    for (int i = 0; i < s.len && blank; i++) blank = code_of(s.full[i]) == game_space();
    if (blank) return 0;
    const uint16_t* t = static_cast<const uint16_t*>(text);
    const int n = game_len(t);
    if (n == 0) return 0;
    if (buf) {
        if (n != s.gc_len) return 0;
        for (int i = 0; i < n; i++)
            if (t[i] != s.gc[i]) return 0;
    }
    bool same = n == s.len;
    for (int i = 0; same && i < n; i++) same = t[i] == s.full[i];
    if (same) return 0; /* the name is its own GameCube name: nothing to swap */
    if (out == nullptr || cap < s.len + 1) return 0;
    for (int i = 0; i <= s.len; i++) out[i] = s.full[i];
    return 1;
}

/* called once at start-up (pc_platform.c). The hooks stay registered; each asks the option every time, so the
 * Tweaks switch takes effect at the next name screen and accurate mode always gets the game's own screen */
extern "C" void pc_name_keyboard_start(void) {
    s_prevMenuExtra = OrreHook_MenuExtra; /* chained: the Language row's and LOAD SAVE's sprites keep working */
    OrreHook_MenuExtra = menu_extra;
    OrreHook_NameEntryInput = input_hook;
    OrreHook_NameEntryDrawGrid = draw_grid;
    OrreHook_NameEntryCursorBox = cursor_box;
    OrreHook_NameEntryDrawLabel = draw_label;
    OrreHook_NameEntryPreview = preview;
    OrreHook_NameEntryPiece = name_piece;
    OrreHook_NameEntryDrawName = draw_name;
    OrreHook_NameEntryCommit = name_commit;
    OrreHook_NameEntryListBack = name_list_back; /* asks back_out_enabled() each time, like the hooks above */
    /* B left the name menu (only possible when the back-out is on): the way back fades out twice and in once; make
     * them short, as leaving a PC screen (pc_field_menu.cpp; snatcherLeo, 2026-11-15) */
    OrreHook_NameMenuLeft = [] { pc_fast_fades_backout(); }; /* its own timing: pc_field_menu.cpp, fade_duration */
}
