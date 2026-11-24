/*
 * pc_title_loadsave.cpp: the LOAD button on the title's main menu, the SAVE FILES list it opens, and the Continue
 * question's YES-first tweak (2.2 step 6, 2026-11-14; the design, the build steps and how each was checked:
 * docs/port/title-load-save.md; mockups 79-81).
 *
 * Why it exists: OrrePort keeps many save files in its Saves folder (the hub picks one). snatcherLeo wanted a second, in-game
 * place to switch saves without leaving for the hub: "Just to switch ... Very simple". Picking a save will run the
 * game's own Continue (plan step 7), so nothing about loading a save is reimplemented here.
 *
 * What it does: the main menu's bottom row becomes LOAD · OPTIONS · Quit (snatcherLeo: LOAD is Quit's twin, in Story Mode's
 * indigo; first designed as "LOAD SAVE", so the code's names still say LOAD SAVE). Its help line is OrrePort's own
 * message ("Load a different save file and continue the Story Mode."). A on LOAD opens SAVE FILES, a compact copy of
 * the Items bag (five rows: each save's name and play time; the ring cursor, the highlight bar, the arrows); A or B
 * closes it (picking and the help box's details are plan steps 6-7, still to build).
 *
 * The parts, top to bottom in this file:
 *   - the button's pictures (read_rgba8 .. put_word, build): made from the game's own Quit picture;
 *   - the bag's pictures for the list (load_bag_pieces): read from the player's disc;
 *   - the row's layout (apply_layout): the game's own items moved, two of OrrePort's added;
 *   - the list (list_* callbacks, build_list): copies of the bag's records with OrrePort's callbacks;
 *   - the hooks the game calls (menu_extra, menu_data_extra, top_menu_*, continue_initial) and their registration.
 *
 * Why records live in low memory (pc_lowmem_alloc): the game's menu records hold 32-bit pointers and ids, read
 * big-endian by its own menu code; OrrePort's copies must sit below 4 GB so a callback's address fits (fn32), the
 * same rule as the rest of the 64-bit port (docs/port/low-memory.md).
 *
 * How (the menu's tables: docs/engine/new-game-flow.md, "How the top menu's entries are defined"; the bag's:
 * docs/engine/bag-and-pc.md, "How the bag screen is put together"; record fields: docs/engine/menus.md):
 *   - Pictures: both made at start-up from the game's own Quit picture (texture 0x0C5D1200, sprite 0x264's crop
 *     (1, 60, 143 x 50)), read from the loaded texture, so nothing of the game's art ships:
 *       Quit at 107 px: plain blue columns removed either side of its word (x 26-42 and 97-115), so every pixel of
 *         "Quit" is the game's own (cutting the word out of the blue and putting it back lost its soft edge pixels);
 *       LOAD SAVE: the word painted out (each row blended between two plain columns), the same 107 px shape, and the
 *         body recoloured to indigo inside the grey rim (a flood fill from the centre). The word "LOAD" is laid on by
 *         put_word (pc_title_word.cpp makes it from the Battle Mode word sheet on the player's disc).
 *     Both go into one RGBA8 texture registered under OrrePort's id ('ORLS'), so the game's own sprite code draws them.
 *     Prototype of the exact pixel work: the scratch pellet_proto2.py (2026-11-14).
 *   - *Per language (2026-11-15; snatcherLeo approved the words CHARGER, LADEN, CARICA, CARGAR, ロード):* both pellets follow the
 *     game's language (pc_game_text_language). The word comes from that language's own word sheet (pc_title_word.cpp)
 *     and the shorter Quit from that language's own Quit pill (Retour, Zurück, INDIETRO, Volver, もどる: the piece the
 *     game itself draws in that language, read from the language disc by pc_word_pictures_piece). Those words fill most
 *     of the pill, so instead of taking plain columns out beside them (too few), the empty pill is shortened as in
 *     English and the language's word pixels are laid back on it, squeezed sideways when wider than 74 px
 *     (quit_shorter_lang; the LOAD words likewise). The LOAD pellet itself is the same in every language. Fail safe:
 *     without the language's disc, or when anything isn't as measured, both stay English (LOAD and Quit), logged.
 *     When the language changes (Options), the pellets are made again the next time the main menu opens (make_pictures;
 *     the hook runs at every open since 2026-11-15).
 *   - Layout: the row symmetric around OPTIONS: LOAD SAVE at x 53, OPTIONS 182 -> 215, Quit 448 -> 479 (all item x,
 *     which sit 4 px left of the visible pellet); the glove's places moved with them (Options 125 -> 158, Quit
 *     395 -> 426, LOAD SAVE 0: fully on screen; mockup 79 had the twins 10 px wider and the glove cut at the edge).
 *   - Entries: two items OrrePort owns past the table's end (OrreHook_MenuExtra, ids 0x7010-0x7011): LOAD SAVE's
 *     picture (a copy of Quit's item 0x911) and its glove place (a copy of Quit's 0x917), linked after 0x917, so it is
 *     the menu's seventh selectable entry: choice 6. The menu system's own cursor rule reaches it (nearest entry in the
 *     pressed direction), so no key handling is needed.
 *   - Replays: whenever a recording drives the pad (any automated run: ORRE_PAD_SCRIPT or a test window), none of this
 *     is applied and the menu is the game's own, so recorded presses land where they did on the console. Test runs of
 *     LOAD SAVE itself set ORRE_LOAD_SAVE=1. Not a pc_options entry: the accurate preset turns every entry off, and
 *     LOAD SAVE stays in accurate mode (there it lists only .gci saves; the project plan).
 *   - The Continue tweak: OrreHook_ContinueInitialAnswer returns YES (0) when pc_option("continue_yes") is on; the
 *     options system keeps it off in the accurate preset and in automated runs (snatcherLeo, 2026-11-14).
 *   - *Added 2026-11-16 (snatcherLeo, 2026-11-15 / 16):* one question after a pick. Picking a save showed two windows: the card
 *     read's "Your save file has been loaded!" (message 0x44E5, waiting for A), then the Continue screen's "Is it okay
 *     to continue from here?" (0x3C22, verified in a PC run's frames). Now the card's message is left out on that read
 *     (OrreHook_CardMessageSkip, error 12 only) and the Continue screen's window asks OrrePort's own two lines instead
 *     (OrreHook_ContinueQuestion, once): the game's own "Your save file has been loaded!" words (game.card.load_done,
 *     what 0x44E5 shows on the PC) and "Continue your story now?" (game.main_menu.continue_now), YES first. YES and
 *     NO / B go on exactly as before (the game's state 3). Stable ids for the UI Editor: title.load_question (the
 *     question's text; the window, its place and the YES / NO box are the game's own).
 *   - *Added 2026-11-16:* CONTINUE's help line names the loaded save: "Continue the Story Mode from / "<name>"."
 *     (game.main_menu.continue_help, the name in {name}), for whichever save is loaded (the remembered slot,
 *     pc_saves_current, when it holds a story), made each time the main menu opens and when the language changes; a
 *     name too wide for the help box (its text item 0xA8A, 440 px) is cut a letter at a time and ends with the
 *     language's mark (game.save_files.name_cut: "..." in English, "…" in Japanese; see cut_mark). Stable id:
 *     title.continue_help.
 *   - *Changed 2026-11-16 (snatcherLeo, on "WWWWWWWWWWWWW" running into its play time):* the SAVE FILES rows cut a name by its
 *     measured width in the list's font (was: 16 letters), so it always ends a clear gap before the play time
 *     (kNameGap); the same helper (fit_name) cuts CONTINUE's help line.
 *   - Both are off in GameCube-accurate mode (the game's own question, its "loaded" message and its CONTINUE line) and
 *     whenever a recording drives the pad (the whole of this file is: enabled()).
 *
 *   - The list: a menu record OrrePort owns (id 0x7000, served by OrreHook_MenuDataExtra past the game's menu table)
 *     whose items are copies of the bag's (0x1F9-0x214) moved into a compact layout, drawing the bag's sprites (copies
 *     of 0xC0-0xDC pointing at 'ORBG'). The bag's own callbacks read the bag's globals, so each copy gets OrrePort's
 *     callback instead (frame colour, title, rows, cursor row, arrows); A / B use the game's own menuButtonNormal.
 *     The game opens it over the main menu and waits for the answer (menuOpenCustom), as it does for its own menus.
 *   - Text: the title and rows are OrrePort's own messages (keys 0xFFF000xx, words from port/ui/lang, so translatable),
 *     printed in the bag's font 1 asked for directly (fontRef 0xF0000001, pc_game_text.cpp), as ITEMS and the item
 *     names are (snatcherLeo: "Make Save Files look like ITEMS").
 *
 * Random numbers: none drawn here (pictures, table edits and the game's menu code; likely, to be confirmed with the
 * caller log, plan step 8). The list only runs when the player presses LOAD, which a console route never does.
 *
 * GameCube-accurate mode: the button and the list stay (they only add a way to switch saves; in accurate mode the list
 * will show only .gci saves, the project plan); the Continue tweak is off. Its row in the accurate-mode list:
 * the project notes section 5.
 *
 * Platforms: all the same (plain C++ over the game's own texture, menu and text code; no OS calls here; the save list
 * comes from pc_saves.c, which keeps the platform parts). Linux and macOS need nothing different (likely).
 * For XD: XD's main menu is a different menu with its own item numbers, and its bag has its own records; the approach
 * (pictures made from the game's own texture, records past the tables' ends, copies of a game menu's items with
 * OrrePort's callbacks, the game's cursor rule and button handling) carries over (likely); the ids and positions
 * differ (likely different).
 */
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <string>

#include "pc_lowmem.h"
#include "pc_options.h"
#include "pc_saves.h"
#include "pc_text.h"
#include "pc_word_pictures.h" /* the language's own Quit pill (pc_word_pictures_piece) */
/* *Changed 2026-11-21 (snatcherLeo: "In-game the load thing, no, we will not have that, you can only load an autosave from
 * hub"):* SAVE FILES no longer lists a save's autosave (it had a row under each save since 2026-11-20). An autosave is
 * played from the hub's "Play the autosave" button, or offered by the hang watch's "Go back to your autosave?"
 * (pc_save_windows.cpp), the safety net; so this file no longer needs pc_autosave.h. docs/port/autosave.md section 3 */

extern "C" {
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
extern void* (*OrreHook_MenuExtra)(int kind, u32 index);
extern void (*OrreHook_TopMenuOpen)(void);
extern int (*OrreHook_TopMenuChoice)(s32 choice, s32* nextState);
extern u32 (*OrreHook_TopMenuHelp)(s32 choice);
extern void (*OrreHook_TopMenuHelpDrawn)(s32 choice, u32 key, u32 alpha, void* window); /* the accurate badge (2026-11-21) */
extern s32 (*OrreHook_ContinueInitialAnswer)(s32 initial);
extern u32 (*OrreHook_ContinueQuestion)(u32 key);
extern s32 (*OrreHook_CardMessageSkip)(s32 error, s32 task_kind);
extern s32 (*OrreHook_NewGameInitialAnswer)(s32 initial, s32 hasStory);
extern void* (*OrreHook_MenuDataExtra)(u32 index);
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
int pc_platform_automated_run(void);
int pc_loadsave_word(const char* lang, int maxW, uint8_t** rgbaOut, int* wOut, int* hOut); /* pc_title_word.cpp */
int pc_title_battle_texture(const char* entry, uint8_t** texels, int* w, int* h, int* format, int* bytes,
                            uint32_t sampling[5]);
void pc_title_disc_done(void);
int pc_card_switch(int slot); /* pc_card.c: the card on another save's folder (plan step 7a) */
const char* pc_game_text_language(void);
}
#include "pc_game_ui.h"

/* OrrePort's words on the title's screens follow the game's language, not the hub's (2026-11-15; pc_text_lang):
 * a European language from a language disc gets these lines from ui/lang/<code>.txt too. Lines made once per run (the
 * SAVE FILES list's title, its tags) keep the language they were first made in until the next start. */
namespace {
const char* gt(const char* key) { return pc_text_lang(pc_game_text_language(), key); }
std::string gtx(const char* key, std::initializer_list<std::pair<const char*, std::string>> values = {}) {
    return tx_lang(pc_game_text_language(), key, values);
}
} // namespace

namespace {

void put16(u8* p, int off, u16 v) { p[off] = (u8)(v >> 8), p[off + 1] = (u8)v; }
void put32(u8* p, int off, u32 v) { p[off] = (u8)(v >> 24), p[off + 1] = (u8)(v >> 16), p[off + 2] = (u8)(v >> 8), p[off + 3] = (u8)v; }

/* the game's records (new-game-flow.md, verified from main.dol) */
constexpr int kItemOptions = 0x910, kItemQuit = 0x911, kGloveOptions = 0x916, kGloveQuit = 0x917;
constexpr u32 kSpriteQuit = 0x264;
constexpr u32 kTexTopMenu = 0x0C5D1200; /* topmenu menu_032: Quit's pellet, the glove, the help box */
constexpr int kQuitCropX = 1, kQuitCropY = 60, kQuitW = 143, kQuitH = 50;

/* OrrePort's records and texture */
constexpr int kItemBase = 0x7010, kItemCount = 2; /* 0x7010 LOAD SAVE's picture, 0x7011 its glove place */
constexpr u32 kSpriteBase = 0x7010;               /* 0x7010 LOAD SAVE's pellet, 0x7011 the shorter Quit */
constexpr u32 kTexId = 0x4F524C53;                /* 'ORLS' */
constexpr u32 kResGroup = 0x4F52;                 /* 'OR': OrrePort's resource group */
constexpr u32 kKeyHelp = 0xFFF00010;              /* LOAD SAVE's help line, OrrePort's own message */
constexpr s32 kChoiceLoadSave = 6;

/* the new row: item x (4 px left of the visible pellet), the glove places, the pellets' width. *Changed 2026-11-14*
 * (snatcherLeo, on the glove cut off at the screen's edge: "Will the cursor hand have to get cut-off, no other way?"): both
 * twins 10 px narrower (117 -> 107) and LOAD SAVE 10 px further right, so its glove (53 px left of the button) starts
 * at x 0, fully on screen; the row stays symmetric around OPTIONS (visible 219-420): LOAD SAVE 57-156, Quit 483-582,
 * 63 px of glove room on each side */
constexpr int kNewW = 107;
/* how wide a word may be on the 107 px pellets (2026-11-15, the words per language): as near the pellet's ends as the
 * game's own longest Quit word comes (Italian INDIETRO: 18 px from the left end and 15 from the right of its 143 px
 * pill, measured on the European disc's picture). A longer word is squeezed sideways to this, its height kept */
constexpr int kRoomL = 18, kRoomR = 15, kMaxWord = kNewW - kRoomL - kRoomR; /* 74 */
constexpr s16 kLoadX = 53, kOptionsX = 215, kQuitX = 479;
constexpr s16 kGloveLoadX = 0, kGloveOptionsX = 158, kGloveQuitX = 426;

void* (*s_prevMenuExtra)(int, u32) = nullptr; /* the Options Language row's (pc_options_lang.cpp), chained */
u8* s_items = nullptr;
u8* s_sprites = nullptr;
bool s_built = false, s_failed = false;

/* whether to change the main menu at all: never when a recording drives the pad (the recorded presses must land on the
 * game's own row, as on the console), except a test of LOAD itself (ORRE_LOAD_SAVE=1) */
bool enabled() {
    if (!pc_game_ui.ready) return false; /* a test program without the game */
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_LOAD_SAVE");
    return force != nullptr && force[0] == '1';
}

/* ---- pictures (straight RGBA bytes) ---- */
struct Pic {
    int w = 0, h = 0;
    std::vector<u8> px;
    u8* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
};

/* GX RGBA8: 4 x 4 tiles of 64 bytes, the 16 alpha / red pairs, then the 16 green / blue pairs. Why read it back: the
 * Quit picture is already loaded for the title (texture 0x0C5D1200), so OrrePort reuses the player's own copy instead
 * of shipping any game art; the texel bytes stay in the GameCube's tiled layout on the PC (the renderer untiles them) */
Pic read_rgba8(const u8* texels, int texW, int x0, int y0, int w, int h) {
    Pic p;
    p.w = w, p.h = h, p.px.assign((size_t)w * h * 4, 0);
    const int tilesPerRow = (texW + 3) / 4;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int tx = x0 + x, ty = y0 + y;
            const u8* tile = texels + ((size_t)(ty / 4) * tilesPerRow + tx / 4) * 64;
            const int i = (ty % 4) * 4 + tx % 4;
            u8* o = p.at(x, y);
            o[3] = tile[i * 2], o[0] = tile[i * 2 + 1], o[1] = tile[32 + i * 2], o[2] = tile[32 + i * 2 + 1];
        }
    return p;
}

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

/* hue / saturation / value both ways: the indigo recolour keeps each pixel's shading (its saturation and brightness
 * relative to Quit's blue) and swaps only the hue, so the pellet's highlights and gradient stay the game's */
void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) {
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx, s = mx > 0 ? d / mx : 0;
    if (d <= 0) { h = 0; return; }
    if (mx == r) h = std::fmod((g - b) / d, 6.f);
    else if (mx == g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h /= 6.f;
    if (h < 0) h += 1;
}

void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) {
    const float c = v * s, hp = h * 6.f, x = c * (1 - std::fabs(std::fmod(hp, 2.f) - 1)), m = v - c;
    float rr = 0, gg = 0, bb = 0;
    if (hp < 1) rr = c, gg = x;
    else if (hp < 2) rr = x, gg = c;
    else if (hp < 3) gg = c, bb = x;
    else if (hp < 4) gg = x, bb = c;
    else if (hp < 5) rr = x, bb = c;
    else rr = c, bb = x;
    r = rr + m, g = gg + m, b = bb + m;
}

/* Quit at the new width: plain body columns removed either side of the word, the word itself untouched. The plain
 * columns (identical, top to bottom, to column 34) are x 26-42 and 96-118 (measured); 36 go: 26-42 and 97-115, so the
 * word ends up 1.5 px left of the centre (2 px in the original) */
Pic quit_shorter(const Pic& src) {
    Pic out;
    std::vector<int> keep;
    for (int x = 0; x < src.w; ++x)
        if (!((x >= 26 && x <= 42) || (x >= 97 && x <= 115))) keep.push_back(x);
    out.w = (int)keep.size(), out.h = src.h, out.px.assign((size_t)out.w * out.h * 4, 0);
    for (int y = 0; y < src.h; ++y)
        for (int i = 0; i < out.w; ++i) std::memcpy(out.at(i, y), &src.px[((size_t)y * src.w + keep[i]) * 4], 4);
    return out;
}

/* Quit's pill with its word painted out (each row blended between plain columns 34 and 104): the empty pill. Its body
 * is the same in every language (verified 2026-11-15: the European and Japanese Quit pills differ from the US one only
 * inside their word) */
Pic paint_out(const Pic& src) {
    Pic e = src;
    constexpr int kL = 34, kR = 104;
    for (int y = 0; y < e.h; ++y) {
        const u8* a = &src.px[((size_t)y * src.w + kL) * 4];
        const u8* b = &src.px[((size_t)y * src.w + kR) * 4];
        for (int x = kL + 1; x < kR; ++x) {
            const float k = (float)(x - kL) / (kR - kL);
            for (int c = 0; c < 4; ++c) e.at(x, y)[c] = (u8)std::lround(a[c] * (1 - k) + b[c] * k);
        }
    }
    return e;
}

/* Pillow's LANCZOS (3 lobes) along x, the kernel widened when shrinking (as pc_title_word.cpp's), over premultiplied
 * RGBA floats: used to squeeze a long Quit word sideways */
float lanczos3(float x) {
    if (x == 0) return 1;
    if (std::fabs(x) >= 3) return 0;
    const float px = 3.14159265f * x;
    return 3 * std::sin(px) * std::sin(px / 3) / (px * px);
}
std::vector<float> squeeze_rows(const std::vector<float>& src, int sw, int h, int dw) {
    std::vector<float> out((size_t)dw * h * 4, 0.f);
    const float scale = (float)sw / dw, fs = std::max(1.f, scale), sup = 3.f * fs;
    for (int o = 0; o < dw; ++o) {
        const float centre = (o + 0.5f) * scale;
        const int lo = std::max(0, (int)std::floor(centre - sup)), hi = std::min(sw, (int)std::ceil(centre + sup));
        std::vector<float> wts;
        float sum = 0;
        for (int s = lo; s < hi; ++s) wts.push_back(lanczos3((s - centre + 0.5f) / fs)), sum += wts.back();
        if (sum == 0) sum = 1;
        for (int y = 0; y < h; ++y)
            for (int c = 0; c < 4; ++c) {
                float v = 0;
                for (int s = lo; s < hi; ++s) v += wts[s - lo] * src[((size_t)y * sw + s) * 4 + c];
                out[((size_t)y * dw + o) * 4 + c] = v / sum;
            }
    }
    return out;
}

/* Quit at the new width in another language (2026-11-15): the language's own Quit pill (its "Retour", "Zurück",
 * "INDIETRO", "Volver", "もどる") at 107 px. Their words fill most of the pill (80-110 px against Quit's 53), so there are
 * too few plain columns to take out either side as in English (French has 9, Italian none). So the same idea is done
 * the other way round: the empty pill is shortened as in English (quit_shorter), and the word's own pixels (every pixel
 * of the language's pill that differs from the empty pill: the letters, their outline and soft edges, their shadow)
 * are laid back on it at the same rows, squeezed sideways when wider than the room (kMaxWord), as the game narrows its
 * own long menu words. A word that fits keeps every pixel as the game drew it. Returns false if the language's pill
 * doesn't look like Quit's (then the US Quit is used). */
bool quit_shorter_lang(const Pic& usQuit, const Pic& lang, Pic& out) {
    const Pic plain = paint_out(usQuit);
    int wl = plain.w, wr = -1, wt = plain.h, wb = -1;
    auto differs = [&](int x, int y) { return std::memcmp(&lang.px[((size_t)y * lang.w + x) * 4], &plain.px[((size_t)y * plain.w + x) * 4], 4) != 0; };
    for (int y = 0; y < plain.h; ++y)
        for (int x = 0; x < plain.w; ++x)
            if (differs(x, y)) wl = std::min(wl, x), wr = std::max(wr, x), wt = std::min(wt, y), wb = std::max(wb, y);
    /* the word must sit inside the pill's body, the pill's ends and rim the US one's (measured: words span x 18-127,
     * rows 6-32) */
    if (wr < 0 || wl < 10 || wr > kQuitW - 11 || wt < 1 || wb > kQuitH - 2) {
        std::printf("[load_save] the language's Quit pill differs outside its word (x %d-%d, rows %d-%d): the US Quit\n",
                    wl, wr, wt, wb);
        return false;
    }
    out = quit_shorter(plain);
    const int ww = wr - wl + 1, wh = wb - wt + 1, tw = std::min(ww, kMaxWord);
    /* the word's place: its offset from the pill's centre kept (the 36 columns go 18 a side, as in English), inside the
     * room */
    int x0 = (int)std::lround((wl + wr + 1) / 2.0 - 18 - tw / 2.0);
    x0 = std::clamp(x0, kRoomL, kNewW - kRoomR - tw);
    /* the word layer: the language's pixels where they differ, premultiplied; see-through elsewhere */
    std::vector<float> layer((size_t)ww * wh * 4, 0.f);
    for (int y = 0; y < wh; ++y)
        for (int x = 0; x < ww; ++x) {
            if (!differs(wl + x, wt + y)) continue;
            const u8* p = &lang.px[((size_t)(wt + y) * lang.w + wl + x) * 4];
            const float a = p[3] / 255.f;
            float* l = &layer[((size_t)y * ww + x) * 4];
            l[0] = p[0] * a, l[1] = p[1] * a, l[2] = p[2] * a, l[3] = a;
        }
    if (tw < ww) layer = squeeze_rows(layer, ww, wh, tw);
    for (int y = 0; y < wh; ++y)
        for (int x = 0; x < tw; ++x) {
            const float* l = &layer[((size_t)y * tw + x) * 4];
            const float a = std::clamp(l[3], 0.f, 1.f);
            if (a <= 0) continue;
            u8* d = out.at(x0 + x, wt + y);
            const float da = d[3] / 255.f, oa = a + da * (1 - a);
            for (int c = 0; c < 3; ++c)
                d[c] = (u8)std::lround(oa > 0 ? std::clamp((std::max(l[c], 0.f) + d[c] * da * (1 - a)) / oa, 0.f, 255.f) : 0.f);
            d[3] = (u8)std::lround(oa * 255);
        }
    std::printf("[load_save] the language's Quit word (%d px wide%s) on the 107 px pellet at x %d\n", ww,
                tw < ww ? (", squeezed to " + std::to_string(tw)).c_str() : "", x0);
    return true;
}

/* LOAD SAVE's pellet: the word painted out (paint_out), the ends kept and a plain middle column repeated to the new
 * width, then the body recoloured indigo inside the grey rim */
Pic load_pellet(const Pic& src) {
    Pic e = paint_out(src);
    constexpr int kEnd = 30;
    Pic out;
    out.w = kNewW, out.h = e.h, out.px.assign((size_t)out.w * out.h * 4, 0);
    for (int y = 0; y < e.h; ++y)
        for (int x = 0; x < kNewW; ++x) {
            const int sx = x < kEnd ? x : (x >= kNewW - kEnd ? e.w - (kNewW - x) : e.w / 2);
            std::memcpy(out.at(x, y), e.at(sx, y), 4);
        }
    /* indigo inside the rim: the rim is grey (low saturation, not bright) or see-through */
    auto rim = [&](int x, int y) {
        const u8* p = out.at(x, y);
        float h, s, v;
        rgb_to_hsv(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, h, s, v);
        return p[3] < 200 || (s < 0.12f && v < 0.82f);
    };
    std::vector<u8> inside((size_t)out.w * out.h, 0);
    std::vector<std::pair<int, int>> stack = {{out.w / 2, 20}};
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        if (x < 0 || y < 0 || x >= out.w || y >= out.h || inside[(size_t)y * out.w + x] || rim(x, y)) continue;
        inside[(size_t)y * out.w + x] = 1;
        stack.push_back({x + 1, y}), stack.push_back({x - 1, y}), stack.push_back({x, y + 1}), stack.push_back({x, y - 1});
    }
    float bh, bs, bv, th, ts, tv;
    rgb_to_hsv(90 / 255.f, 149 / 255.f, 197 / 255.f, bh, bs, bv);  /* Quit's blue */
    rgb_to_hsv(98 / 255.f, 52 / 255.f, 197 / 255.f, th, ts, tv);   /* Story Mode's indigo */
    for (int y = 0; y < out.h; ++y)
        for (int x = 0; x < out.w; ++x) {
            if (!inside[(size_t)y * out.w + x]) continue;
            u8* p = out.at(x, y);
            float h, s, v;
            rgb_to_hsv(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, h, s, v);
            if (!(h > 0.45f && h < 0.75f && s > 0.05f)) continue;
            float r, g, b;
            hsv_to_rgb(th, std::min(1.f, s * ts / bs), std::min(1.f, v * tv / bv), r, g, b);
            p[0] = (u8)(r * 255), p[1] = (u8)(g * 255), p[2] = (u8)(b * 255);
        }
    return out;
}

/* The SAVE FILES list's pictures (plan step 5a): the Items bag draws every piece (panel, frame, ring cursor, highlight
 * bar, arrows) from one texture, menu_010 (360 x 322 RGBA8, the game's id 0x02591200), loaded only in the bag's own
 * room. It is read from the player's disc (the Battle Mode archive, as the word) and registered under OrrePort's own id
 * 'ORBG', not the game's: the real bag later loads its own copy under 0x02591200, and two textures with one id could
 * meet in the resource list. The texel data is already in the GameCube's tiled layout, so it is copied unchanged. */
constexpr u32 kTexBag = 0x4F524247; /* 'ORBG' */
bool s_bagReady = false;
void load_bag_pieces() {
    const PcGameUi& g = pc_game_ui;
    if (g.resource_find(kTexBag) != nullptr) { s_bagReady = true; return; }
    uint8_t* texels = nullptr;
    int w = 0, h = 0, fmt = 0, bytes = 0;
    uint32_t sampling[5] = {};
    if (!pc_title_battle_texture("menu_010", &texels, &w, &h, &fmt, &bytes, sampling)) {
        std::printf("[load_save] the bag's pictures (menu_010) weren't found: no save list\n");
        return;
    }
    const int need = ((w + 3) / 4) * ((h + 3) / 4) * 64;
    if (fmt != 0x45 || w != 360 || h != 322 || bytes < need) {
        std::printf("[load_save] the bag's pictures are %d x %d format %02x (%d bytes): not as expected, no save list\n", w, h,
                    fmt, bytes);
        delete[] texels;
        return;
    }
    void* t = g.texture_create(w, h, 0x45 /* RGBA8 */);
    if (t == nullptr) {
        std::printf("[load_save] no room for the bag's pictures: no save list\n");
        delete[] texels;
        return;
    }
    std::memcpy(g.texture_lock(t), texels, (size_t)need);
    g.texture_unlock(t);
    /* the disc texture's own wrap and filter settings into the handle (gs_texture.h: +0x10..+0x23, native on the PC as
     * the size fields are), so the copy samples as the bag's own does: with the defaults the mirrored halves showed a
     * seam (one darker column where they meet) */
    std::memcpy(static_cast<u8*>(t) + 0x10, sampling, sizeof(uint32_t) * 5);
    g.resource_register(t, kResGroup, kTexBag);
    delete[] texels;
    s_bagReady = true;
    std::printf("[load_save] the bag's pictures are ready for the save list (%d x %d; wrap %u/%u, filter %u/%u/%u)\n", w, h,
                sampling[0], sampling[1], sampling[2], sampling[3], sampling[4]);
}

/* the word (pc_title_word.cpp, made from a disc's word sheet in the language lang: LOAD, CHARGER, LADEN, CARICA, CARGAR,
 * ロード) laid over the pellet: centred on the body horizontally (its first and last body column on the middle row) and
 * on row 19, where the game's own "Quit" is centred in its pellet (its word spans rows 7-31); at most kMaxWord wide.
 * Returns false when that language's word can't be made (the pellet is then unchanged). */
bool put_word(Pic& pellet, const char* lang) {
    uint8_t* rgba = nullptr;
    int ww = 0, wh = 0;
    if (!pc_loadsave_word(lang, kMaxWord, &rgba, &ww, &wh)) return false;
    int l = pellet.w, r = -1;
    for (int x = 0; x < pellet.w; ++x)
        if (pellet.at(x, 19)[3] > 200) l = std::min(l, x), r = std::max(r, x);
    const int ox = (int)std::lround((l + r + 1) / 2.0 - ww / 2.0), oy = (int)std::lround(19.5 - wh / 2.0);
    for (int y = 0; y < wh; ++y)
        for (int x = 0; x < ww; ++x) {
            const int px = ox + x, py = oy + y;
            if (px < 0 || py < 0 || px >= pellet.w || py >= pellet.h) continue;
            const uint8_t* s = &rgba[((size_t)y * ww + x) * 4];
            u8* d = pellet.at(px, py);
            const float a = s[3] / 255.f, da = d[3] / 255.f, oa = a + da * (1 - a);
            for (int c = 0; c < 3; ++c)
                d[c] = (u8)std::lround(oa > 0 ? (s[c] * a + d[c] * da * (1 - a)) / oa : 0);
            d[3] = (u8)std::lround(oa * 255);
        }
    delete[] rgba;
    return true;
}

/* the language's own Quit pill, as the game draws it in that language: the piece of US sprite record 0x264 (Quit's)
 * in that language, cut from the language disc's own picture (pc_word_pictures_piece: the rectangle and texture
 * together, the European cb_menu_<l> / Japanese colosseumbattle_menu menu_032, 774 x 139 RGBA8). False when the language
 * has no disc or the piece isn't as measured (143 x 50 RGBA8) */
bool language_quit(const char* lang, Pic& out) {
    int16_t rect[4] = {};
    size_t size = 0;
    uint8_t* file = pc_word_pictures_piece(lang, kSpriteQuit, rect, &size);
    if (file == nullptr) {
        std::printf("[load_save] %s: the language's Quit picture isn't there (no disc, not read yet, or the word pictures "
                    "are off)\n", lang);
        return false;
    }
    const std::vector<u8> d(file, file + size);
    std::free(file);
    auto be16 = [&](size_t o) { return o + 2 <= d.size() ? (int)((d[o] << 8) | d[o + 1]) : 0; };
    auto be32 = [&](size_t o) { return o + 4 <= d.size() ? (u32)((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) : 0u; };
    const int texW = be16(0), texH = be16(2);
    const size_t off = be32(0x28) != 0 ? be32(0x28) : 0x80;
    const size_t need = off + (size_t)((texW + 3) / 4) * ((texH + 3) / 4) * 64;
    if (be32(8) != 0x45 /* RGBA8 */ || rect[2] != kQuitW || rect[3] != kQuitH || rect[0] < 0 || rect[1] < 0 ||
        rect[0] + kQuitW > texW || rect[1] + kQuitH > texH || d.size() < need) {
        std::printf("[load_save] %s: the language's Quit picture isn't as measured (%d x %d format %02x, piece %d,%d %dx%d): "
                    "the US Quit\n", lang, texW, texH, be32(8), rect[0], rect[1], rect[2], rect[3]);
        return false;
    }
    out = read_rgba8(d.data() + off, texW, rect[0], rect[1], kQuitW, kQuitH);
    return true;
}

/* the texture handle's size (gs_texture.h: width and height first). Read both ways and keep the plausible one: the
 * handle may hold the disc's big-endian values or native ones once loaded (checked in the log) */
bool texture_size(const void* tex, int& w, int& h) {
    const u8* b = static_cast<const u8*>(tex);
    const int nw = (int)*(const u16*)b, nh = (int)*(const u16*)(b + 2);
    const int bw = (b[0] << 8) | b[1], bh = (b[2] << 8) | b[3];
    if (nw >= kQuitCropX + kQuitW && nw <= 1024 && nh >= kQuitCropY + kQuitH && nh <= 1024) w = nw, h = nh;
    else if (bw >= kQuitCropX + kQuitW && bw <= 1024 && bh >= kQuitCropY + kQuitH && bh <= 1024) w = bw, h = bh;
    else {
        std::printf("[load_save] the title's texture has an unexpected size (%d x %d / %d x %d): no LOAD SAVE\n", nw, nh, bw, bh);
        return false;
    }
    std::printf("[load_save] title texture %d x %d (format word %08x)\n", w, h, *(const u32*)(b + 8));
    return true;
}

/* one texture: LOAD SAVE's pellet at x 0, the shorter Quit at x 120 (whole GX tiles: 240 x 52) */
constexpr int kAtlasW = 240, kAtlasH = 52, kQuit2X = 120;
std::string s_picLang; /* the game's language the pellets were last made for */

/* the languages with their own LOAD word and Quit pill (the word pictures' languages, pc_word_pictures.cpp) */
bool own_word_language(const char* lang) {
    for (const char* l : {"de", "fr", "it", "es", "ja"})
        if (std::strcmp(lang, l) == 0) return true;
    return false;
}

/* The two pellets in the game's language lang, written into OrrePort's texture 'ORLS' (made the first time, written
 * again in place when the language changes: the game's unlock gives the texture a new version, so the renderer reads it
 * again). English, or a language without its disc: LOAD and the US Quit. *Per language (2026-11-15):* the word from the
 * language's own word sheet and the shorter Quit from the language's own Quit pill, both or neither (fail safe: never
 * a French LOAD beside an English Quit; the log says why). */
bool make_pictures(const char* lang) {
    const PcGameUi& g = pc_game_ui;
    void* tex = g.resource_find(kTexTopMenu);
    if (tex == nullptr) {
        std::printf("[load_save] the title's texture 0x%08X isn't loaded: no LOAD SAVE\n", kTexTopMenu);
        return false;
    }
    int tw = 0, th = 0;
    if (!texture_size(tex, tw, th)) return false;
    const u8* texels = static_cast<const u8*>(g.texture_lock(tex));
    const Pic quit = read_rgba8(texels, tw, kQuitCropX, kQuitCropY, kQuitW, kQuitH);
    g.texture_unlock(tex);
    Pic load = load_pellet(quit);
    Pic quit2;
    bool own = false;
    if (own_word_language(lang)) {
        Pic withWord = load, langQuit;
        if (put_word(withWord, lang) && language_quit(lang, langQuit) && quit_shorter_lang(quit, langQuit, quit2)) {
            load = withWord;
            own = true;
        } else {
            std::printf("[load_save] %s: LOAD and Quit stay English (the reason is above)\n", lang);
        }
    }
    if (!own) {
        quit2 = quit_shorter(quit);
        put_word(load, "en");
    }
    std::vector<u8> rgba((size_t)kAtlasW * kAtlasH * 4, 0);
    auto blit = [&](const Pic& p, int ox) {
        for (int y = 0; y < p.h; ++y) std::memcpy(&rgba[((size_t)y * kAtlasW + ox) * 4], &p.px[(size_t)y * p.w * 4], (size_t)p.w * 4);
    };
    blit(load, 0), blit(quit2, kQuit2X);
    void* t = g.resource_find(kTexId);
    const bool fresh = t == nullptr;
    if (fresh && (t = g.texture_create(kAtlasW, kAtlasH, 0x45 /* RGBA8 */)) == nullptr) {
        std::printf("[load_save] no room for LOAD SAVE's texture: no LOAD SAVE\n");
        return false;
    }
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(t)), rgba, kAtlasW, kAtlasH);
    g.texture_unlock(t);
    if (fresh) g.resource_register(t, kResGroup, kTexId);
    s_picLang = lang;
    std::printf("[load_save] the pellets made for the language \"%s\" (%s)\n", lang, own ? "its own word and Quit" : "LOAD and Quit");
    return true;
}

/* everything for the button, once, on the title's first main menu (the Quit texture is loaded by then); also starts the
 * list's pictures and the word, which read the disc, so a failure leaves the game's own menu */
bool build() {
    if (s_built) return true;
    if (s_failed) return false;
    s_failed = true; /* until it succeeds: no retry every visit */
    const PcGameUi& g = pc_game_ui;
    if (!make_pictures(pc_game_text_language())) return false;
    s_items = static_cast<u8*>(pc_lowmem_alloc(0x1C * kItemCount));
    s_sprites = static_cast<u8*>(pc_lowmem_alloc(0x18 * 2));
    if (s_items == nullptr || s_sprites == nullptr) return false;
    /* the sprites: copies of Quit's (its offsets and type kept), each pointing at its picture in OrrePort's texture */
    for (int i = 0; i < 2; ++i) {
        u8* s = s_sprites + i * 0x18;
        std::memcpy(s, g.menu_sprite(kSpriteQuit), 0x18);
        put16(s, 0x08, (u16)(i == 0 ? 0 : kQuit2X)), put16(s, 0x0A, 0), put16(s, 0x0C, (u16)kNewW), put16(s, 0x0E, (u16)kQuitH);
        put32(s, 0x10, kTexId);
    }
    /* the items: LOAD SAVE's picture (a copy of Quit's) and its glove place (a copy of Quit's), linked after 0x917 */
    u8* pic = s_items, *glove = s_items + 0x1C;
    std::memcpy(pic, g.menu_item(kItemQuit), 0x1C);
    put16(pic, 2, (u16)kLoadX), put16(pic, 6, (u16)kNewW), put16(pic, 0x0A, (u16)kSpriteBase);
    pic[0] &= (u8)~0x40, put16(pic, 0x18, (u16)(kItemBase + 1));
    std::memcpy(glove, g.menu_item(kGloveQuit), 0x1C);
    put16(glove, 2, (u16)kGloveLoadX);
    glove[0] |= 0x40, put16(glove, 0x18, 0); /* the chain's new end */
    pc_game_text_set_own(kKeyHelp, gt("game.main_menu.load_save_help"), 0x3C15); /* the main menu help's font */
    load_bag_pieces();
    pc_title_disc_done();
    s_built = true, s_failed = false;
    std::printf("[load_save] LOAD SAVE is ready (pellets %d x %d)\n", kNewW, kQuitH);
    return true;
}

/* the game's own items moved and linked (idempotent: every visit) */
void apply_layout() {
    const PcGameUi& g = pc_game_ui;
    auto set_x = [&](int id, s16 x) {
        const u8* it = static_cast<const u8*>(g.menu_item(id));
        g.menu_item_set_xy(id, x, (s16)((it[4] << 8) | it[5]));
    };
    set_x(kItemOptions, kOptionsX), set_x(kItemQuit, kQuitX);
    set_x(kGloveOptions, kGloveOptionsX), set_x(kGloveQuit, kGloveQuitX);
    u8* quit = static_cast<u8*>(g.menu_item(kItemQuit));
    put16(quit, 6, (u16)kNewW), put16(quit, 0x0A, (u16)(kSpriteBase + 1)); /* the shorter Quit */
    u8* last = static_cast<u8*>(g.menu_item(kGloveQuit));
    last[0] &= (u8)~0x40, put16(last, 0x18, (u16)kItemBase); /* 0x917 -> LOAD SAVE's picture -> its glove place */
}

/* ---- the SAVE FILES list (plan step 5): copies of the Items bag's own pieces, laid out compact ----
 * The bag's records (decoded from main.dol, 2026-11-14; bag-and-pc.md): panel 0x1F9 (sprite 0xC0, a 29 x 29 patch
 * stretched, alpha 0xCC), sides 0x1FF / 0x200 (sprite 0xC2), bottom strip 0x201 / 0x202 (0xC1, 182 x 64, mirrored
 * halves), title halves 0x209 / 0x20A (0xCC, 182 x 120: the title bar and the panel's top with the pocket tabs' space
 * between), title text 0x214, a tab selector 0x20B (selectable). Changes: no tabs, no description box, no money box;
 * the title bar cut to the bar alone (rows 0-35 of 0xCC) and joined to the panel's top (the game's own 0xCB: rows
 * 64-119 of the same picture), five rows instead of eight (3 x 31 px shorter), centred on the screen above the bottom
 * row, the panel 95 % solid (alpha 0xF2), the frame in the Items page's colour (C0 C0 C0, the bag's page table). */
constexpr u32 kMenuSaveList = 0x7000;          /* past the menu table's end (OrreHook_MenuDataExtra) */
constexpr int kListItemBase = 0x7020;          /* the list's items */
constexpr u32 kListSpriteBase = 0x7020;        /* copies of the bag's sprites 0xC0-0xDC, on OrrePort's 'ORBG' */
constexpr u32 kListSpriteTitle = 0x7020 + 0x1D; /* the title bar alone (0xCC's rows 0-35) */
constexpr int kBagDX = 320 - 448;              /* the bag's centre (x 448) to the screen's */
constexpr int kTitleY = 6;                     /* the title bar's top */
constexpr int kBagDY = kTitleY + 38 - 108;     /* the bag's panel top (y 108) to just under the title bar */
constexpr int kShorter = 3 * 31;               /* five rows instead of eight */
constexpr u32 kKeyTitle = 0xFFF00011;          /* "SAVE FILES", OrrePort's own message */
constexpr u32 kFontBag = 0xF0000001;           /* font 1, asked for directly (pc_game_text.cpp): the bag's own font */
enum { kL_Panel, kL_Bar, kL_CapR, kL_CapL, kL_Rows, kL_SideL, kL_SideR, kL_BotR, kL_BotL, kL_TitleL, kL_TitleR,
       kL_TopL, kL_TopR, kL_TitleText, kL_Ring, kL_ArrowDown, kL_ArrowUp, kL_TagGci, kL_TagOrre, kL_TagWord,
       kL_Select, kListItemCount };
/* the save-type tag on the help box's top-right corner (design: snatcherLeo, 2026-11-13, "I like these tags, perfect";
 * mockup 80): "GCI" on the GameCube indigo or "ORRESAVE" on OrrePort's rust, the hub's own tag colours (hub.rcss
 * .ftag), white letters, a lighter rim, 15 px tall, each as wide as its word plus 6 px a side, its right edge at x 555
 * and its top at y 345, sitting on the box's frame. The shapes are drawn into OrrePort's texture 'ORTG' at start-up;
 * the word is printed by the game's text in the bag's font 1 (the game's 13 px fonts, 2 and 4, belong to the battle
 * screens and aren't loaded on the title) */
constexpr u32 kTexTag = 0x4F525447;                  /* 'ORTG' */
constexpr u32 kListSpriteTags = kListSpriteBase + 0x1E; /* + 0 GCI, + 1 ORRESAVE */
constexpr int kListSpriteCount = 0x20;               /* the bag's 0x1D, the title bar, the two tags */
/* *Changed 2026-11-14 (snatcherLeo, on the first in-game tag: "Make the GCI fit a little better"):* 15 -> 18 px tall and
 * 6 -> 8 px a side, still centred on the frame line (top 345 -> 343): font 1's capitals are about 13 px, so the
 * mockup's 15 px tag (drawn with 11 px letters) left them touching its rims */
constexpr int kTagH = 18, kTagPad = 8, kTagRight = 555, kTagTop = 343;
constexpr u32 kKeyTag = 0xFFF000E0;                  /* + 0 "GCI", + 1 "ORRESAVE" */
int s_tagW[2] = {0, 0};
constexpr int kRows = 5, kRowPitch = 31;   /* the bag's row spacing; five rows in view */
constexpr int kRingY = 149 + kBagDY;        /* the ring cursor's and the bar's y on the first row */
/* the texts, OrrePort's own messages, one key per save (index in the list) for its name, its play time and its help
 * box. *Changed 2026-11-14 (step 6):* 5d had one key per visible row, re-set on every scroll; each change of an own
 * message's text makes a new encoded copy in low memory (pc_game_text.cpp keeps the old one, which the game may still
 * be showing), so scrolling back and forth grew memory a little each time. With a key per save the text of a key
 * never changes while the list is open, and the same text set again on the next visit is not encoded again. */
constexpr int kMaxSaves = 64;               /* the list's cap (the hub has no fixed limit; more are not listed) */
constexpr u32 kKeyName = 0xFFF00100, kKeyTime = 0xFFF00180, kKeyHelpSave = 0xFFF00200; /* + the save's index */
constexpr u32 kFontHelp = 0x3C15;           /* the main menu help's font (a key of its bank: LOAD's help line too) */
constexpr int kItemHelpText = 0xA8A;        /* the help box's text item (menu 0x7F): its width bounds a line */
constexpr u32 kKeyMeasure = 0xFFF000F0;     /* a scratch message for measuring a line before it is used (never shown) */
std::vector<PcSaveInfo> s_saves;            /* the save files, in the hub's order */
std::vector<char> s_helpReady;              /* per save: its help text is made (made the first time it is shown) */
int s_first = 0, s_row = 0;                 /* the first save in view, the cursor's row in the view */
unsigned s_tick = 0;                        /* frames while the list is open (the arrows' blink) */
bool s_listOpen = false;                    /* the help box shows the highlighted save's details meanwhile */
/* a one-off line in the help box after a refused pick (rules R2 / R3), until the cursor moves; 0 = none */
constexpr u32 kKeyNoticeGone = 0xFFF00012, kKeyNoticeDamaged = 0xFFF00013;
u32 s_notice = 0;

/* ---- cutting a save's name to a measured width (the project's helper, 2026-11-16), shared by the SAVE FILES rows and
 * CONTINUE's help line so both cut the same way ----
 * Why measured: the game's letters have their own widths (a glyph's advance is its width, fonts.md), so a count of
 * letters can't say whether a name fits: "WWWWWWWWWWWWW" (13 letters, under the old 16-letter limit) ran into the play
 * time (snatcherLeo, 2026-11-16: "we need to truncate names in this save files place too, to ensure we show playtime for all
 * saves"). Platforms: all the same. */

/* a text's width in the game's pixels, measured by the game itself (GSmsgGetRect on a scratch own message in the font
 * the text will be printed in; the widest line of a two-line text) */
int measured_width(const std::string& text, u32 fontRef) {
    pc_game_text_set_own(kKeyMeasure, text.c_str(), fontRef);
    return (s16)(pc_game_ui.msg_rect(kKeyMeasure) >> 16);
}
/* the mark a cut name ends with, per language (game.save_files.name_cut): "..." in English and the European languages,
 * "…" in Japanese. Why not "…" everywhere: the US disc's font 0 has a U+2026 glyph (its extra-glyph file), but it is the
 * Japanese-style centred three dots, which read as "wi···" in an English line (seen in a PC frame dump, 2026-11-16),
 * and the list's font 1 has no U+2026 at all (verified in the extracted character maps); the game's own English text
 * writes three periods. A translator picks the mark that suits the language */
std::string cut_mark() { return gt("game.save_files.name_cut"); }
/* the longest start of name (whole characters, not UTF-8 bytes; spaces before the mark dropped) plus the mark for which
 * fits() holds; the name itself when it fits; the mark alone when nothing does. *cut says whether it was cut */
template <class Fits>
std::string fit_name(const std::string& name, Fits fits, const std::string& mark, bool* cut) {
    *cut = false;
    if (fits(name)) return name;
    *cut = true;
    std::vector<size_t> starts;
    for (size_t i = 0; i < name.size(); ++i)
        if ((name[i] & 0xC0) != 0x80) starts.push_back(i);
    for (size_t keep = starts.size(); keep-- > 1;) {
        std::string part = name.substr(0, starts[keep]);
        while (!part.empty() && part.back() == ' ') part.pop_back();
        if (!part.empty() && fits(part + mark)) return part + mark;
    }
    return mark;
}

/* play time as the game shows it (the Continue screen's "09:34": hours, two-digit minutes) */
std::string list_time(const PcSaveInfo& s) {
    if (!s.has_save) return "";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02u:%02u", s.play_seconds / 3600, (s.play_seconds / 60) % 60);
    return buf;
}
/* the clear room between a name and its play time, in the game's measure: the gap the first build's cut name left
 * ("Phenac save e..." ends at x 364, its play time "00:19" starts at x 410: 45 clear pixels in a PC frame dump,
 * 2026-11-16, the coordinator's reference for the new rule). By the game's own measure (GSmsgGetRect, which counts
 * each letter's whole advance) that is 42: the game measures "Phenac save e..." at 177 px (logged in a PC run), and the
 * rows piece (282 px) less 4, the play time (59) and 42 leaves exactly 177, so that cut stays the longest that fits */
constexpr int kNameGap = 42;
constexpr int kBagRowsItem = 0x1FE; /* the bag's rows piece (the list's copy keeps its width; list_rows prints in it) */

/* ---- The region word on every row (Phase 3 step 3.5 step 6, 2026-11-19; snatcherLeo's call 4 of 2026-11-19: "the region
 * word in the game's own SAVE FILES list too ... the same coloured word (US blue, PAL green, JPN red), in all six
 * languages") ----
 * Where: on each row, just before its play time (the row's facts, as the hub's line leads its facts with the word),
 * right-aligned kRegionGap before the time; the name's room shrinks by the word and the gap, so a name still ends
 * kNameGap clear of what follows it. Why on the rows and not in the help box: the rows show every save at once (snatcherLeo,
 * 2026-11-17: "color code it next to every save ... viewable in our LOAD menu as well"), and the help box is drawn by
 * the game's own help line in one colour (a Color control code there would also fix the text's alpha and stop it
 * fading with the box). Why the region alone, no European language: a row is 282 px, and "PAL · ES" (about 90 px in
 * font 1) would cut most names to five letters; the hub's Saves page writes the language out.
 * How: one own message per save (kKeyRegion + index, the game's language: game.save_files.region.*), printed with the
 * game's own GSmsgPrintColor in the region's colour (RGBA; the game's text colour multiplies the font's white letters,
 * as the help line's alpha does: likely, to see in a hand check). The colours are the hub's dark-look ones, the lighter
 * shades, since the list's panel is dark (guess until snatcherLeo's look). Stable id for the UI Editor: title.save_files.region.
 * Look only: nothing here draws a random number or changes timing (likely); the accurate-mode row is the project's to add.
 * Platforms: all the same (the game's own text calls). */
constexpr u32 kKeyRegion = 0xFFF00280; /* + the save's index (kMaxSaves of them: 0xFFF00280-0xFFF002BF) */
constexpr int kRegionGap = 12;         /* between the region word and the play time */
/* the word's key and colour by the save's stamp (PcSaveInfo.region: 1 Japan, 2 US, 3 Europe); nullptr / 0 = none */
const char* region_key(int region) {
    return region == 1 ? "game.save_files.region.jpn" : region == 2 ? "game.save_files.region.us"
         : region == 3 ? "game.save_files.region.pal" : nullptr;
}
u32 region_colour(int region) { /* hub.rcss .rword's dark-look colours, alpha opaque */
    return region == 1 ? 0xE0607EFFu : region == 2 ? 0x5B9BE0FFu : 0x4FAE72FFu;
}
/* every save's row texts, once as the list opens (font 1: the Items bag's font for its item names and counts, its
 * bank's group header, verified from the disc). *Changed 2026-11-16 (snatcherLeo: "we just truncate it and we good"):* a
 * name is cut to the room its row leaves: the rows piece's width, less the play time drawn at its right (list_rows: 4
 * px from the edge) and kNameGap, measured in font 1 (was: at most 16 letters, a longer one its first 13 and "...") */
void set_row_texts() {
    const u8* rows = static_cast<const u8*>(pc_game_ui.menu_item(kBagRowsItem));
    const int rowW = rows != nullptr ? (s16)((rows[6] << 8) | rows[7]) : 0;
    for (int idx = 0; idx < (int)s_saves.size(); ++idx) {
        const std::string time = list_time(s_saves[idx]);
        pc_game_text_set_own(kKeyTime + idx, time.c_str(), kFontBag);
        const int timeW = time.empty() ? 0 : (s16)(pc_game_ui.msg_rect(kKeyTime + idx) >> 16);
        /* the region word before the play time (3.5 step 6, above): only on a row with a save and a known region */
        const char* rkey = s_saves[idx].has_save ? region_key(s_saves[idx].region) : nullptr;
        pc_game_text_set_own(kKeyRegion + idx, rkey != nullptr ? gt(rkey) : "", kFontBag);
        const int regionW = rkey != nullptr ? (s16)(pc_game_ui.msg_rect(kKeyRegion + idx) >> 16) + kRegionGap : 0;
        const int room = rowW - 4 - timeW - regionW - kNameGap;
        bool cut = false;
        const std::string name = fit_name(
            s_saves[idx].name, [&](const std::string& t) { return rowW <= 0 || measured_width(t, kFontBag) <= room; },
            cut_mark(), &cut);
        pc_game_text_set_own(kKeyName + idx, name.c_str(), kFontBag);
        if (cut)
            std::printf("[load_save] row of slot %d: the name cut to \"%s\" (%d px of %d; row %d px, play time %d px)\n",
                        s_saves[idx].slot, name.c_str(), measured_width(name, kFontBag), room, rowW, timeW);
    }
}

/* ---- the help box while the list is open (plan step 6): the highlighted save's place and date, then its play time
 * and the Continue screen's Shadow Pokémon counts (design: title-load-save.md, "The help box under the list") ---- */

/* the date as the design shows it ("Nov 13, 9:41 PM"; the order and clock are the translator's, game.save_files.date).
 * The card's time is the console's own wall clock (seconds since 2000-01-01, no time zone), so it is broken down as
 * UTC to give back exactly the stored clock; the file's time (seconds since 1970) is shown in this PC's local time */
std::string help_date(const PcSaveDetails& d, const PcSaveInfo& s) {
    SDL_DateTime dt{};
    const bool fromCard = d.card_time >= 0;
    const long long secs = fromCard ? 946684800LL + d.card_time : s.saved_time; /* 946684800: 2000-01-01 since 1970 */
    if (secs <= 0 || !SDL_TimeToDateTime((SDL_Time)secs * 1000000000LL, &dt, !fromCard)) return "";
    char h[8], hh[8], mm[8];
    std::snprintf(h, sizeof h, "%d", dt.hour % 12 == 0 ? 12 : dt.hour % 12);
    std::snprintf(hh, sizeof hh, "%02d", dt.hour);
    std::snprintf(mm, sizeof mm, "%02d", dt.minute);
    const std::string month = "month." + std::to_string(dt.month < 1 || dt.month > 12 ? 1 : dt.month);
    return gtx("game.save_files.date", {{"month", gt(month.c_str())}, {"day", std::to_string(dt.day)}, {"h", h},
                                        {"hh", hh}, {"mm", mm},
                                        {"ampm", gt(dt.hour < 12 ? "game.save_files.am" : "game.save_files.pm")}});
}

/* the widest line of a two-line own message, in the game's pixels (GSmsgGetRect: the width in the high half) */
int help_width(u32 key) { return (s16)(pc_game_ui.msg_rect(key) >> 16); }

/* the help text of save idx, made once: read the save (pc_saves_read_details), the place from the game's own text, the
 * counts with the game's own rule; then the lines fit the help box: the gaps narrowed 3 -> 2 -> 1 spaces until both
 * lines fit (letter spacing is not narrowed yet: the design's last step, still to do) */
void ensure_help(int idx) {
    if (idx < 0 || idx >= (int)s_saves.size() || s_helpReady[idx]) return;
    s_helpReady[idx] = 1;
    const PcSaveInfo& s = s_saves[idx];
    const u32 key = kKeyHelpSave + (u32)idx;
    static PcSaveDetails d; /* 1.5 KB: kept off the stack of a game callback */
    if (s.damaged) { /* rule R2: the approved damaged-save line (docs/port/save-messages.md) */
        pc_game_text_set_own(key, gt("game.card.damaged"), kFontHelp);
        return;
    }
    if (!s.has_save || !pc_saves_read_details(s.file, &d)) {
        pc_game_text_set_own(key, gt(s.has_save ? "game.main_menu.load_save_help" : "game.save_files.empty"),
                             kFontHelp);
        return;
    }
    char place[160];
    pc_game_ui.place_text(d.floor_id, place, sizeof place);
    /* a test switch for the cut below (the US game's place names all fit): ORRE_TEST_PLACE replaces the place */
    if (const char* test = SDL_getenv("ORRE_TEST_PLACE")) std::snprintf(place, sizeof place, "%s", test);
    uint32_t snagged = 0, purified = 0;
    pc_game_ui.shadow_counts(d.story_flags, PC_SAVE_STORY_FLAG_WORDS, &snagged, &purified);
    const std::string date = help_date(d, s);
    const u8* item = static_cast<const u8*>(pc_game_ui.menu_item(kItemHelpText));
    const int boxW = item != nullptr ? (s16)((item[6] << 8) | item[7]) : 0;
    /* each line fits on its own (a long place mustn't squeeze the counts' line): measured on a scratch key, so the
     * save's own key gets its text once */
    auto fits = [&](const std::string& line) {
        pc_game_text_set_own(kKeyMeasure, line.c_str(), kFontHelp);
        return boxW <= 0 || help_width(kKeyMeasure) <= boxW;
    };
    auto where_line = [&](int gap, const std::string& where) {
        return gtx("game.save_files.where", {{"place", where}, {"gap", where.empty() ? "" : std::string(gap, ' ')},
                                             {"date", date}});
    };
    /* a test switch for the fitting at the largest values (the story has 48 Shadow Pokémon, but mods may add more):
     * ORRE_TEST_PROGRESS="999:59,9999,9999" replaces the play time and both counts */
    std::string timeText = list_time(s), snagText = std::to_string(snagged), purText = std::to_string(purified);
    if (const char* test = SDL_getenv("ORRE_TEST_PROGRESS")) {
        char t[16] = "", a[16] = "", b[16] = "";
        if (std::sscanf(test, "%15[^,],%15[^,],%15s", t, a, b) == 3) timeText = t, snagText = a, purText = b;
    }
    auto progress_line = [&](const char* form, int gap) {
        return gtx(form, {{"time", timeText}, {"gap", std::string(gap, ' ')}, {"snagged", snagText}, {"purified", purText}});
    };
    /* "Play Time" (the Continue screen's own label, snatcherLeo) with gaps 3 -> 2 -> 1; past that, the short form ("Time"):
     * the game has no letter spacing to narrow (a glyph's advance is its width, fonts.md), and the counts are never
     * cut. Only mods reach it: at "999:59" and four-digit counts the full line is 481 px against 440 */
    std::string line1, line2;
    bool progressFits = false;
    for (const char* form : {"game.save_files.progress", "game.save_files.progress_short"})
        for (int gap = 3; gap >= 1 && !progressFits; --gap) progressFits = fits(line2 = progress_line(form, gap));
    bool placeFits = false;
    for (int gap = 3; gap >= 1 && !placeFits; --gap) placeFits = fits(line1 = where_line(gap, place));
    /* still too wide with a 1-space gap: the place is cut, a character at a time, and ends with "..." (snatcherLeo,
     * 2026-11-14: "for the location the user is in, we should plan "what if it's really long", and have truncation
     * (3 dots)"; a modded or translated place name can be long). Spaces before the dots are dropped ("CRYSTAL...",
     * not "CRYSTAL ..."). The date and the counts are never cut. Counted in characters, not bytes (UTF-8) */
    if (!placeFits) {
        std::vector<size_t> starts;
        for (size_t i = 0; place[i] != '\0'; ++i)
            if ((place[i] & 0xC0) != 0x80) starts.push_back(i);
        for (size_t keep = starts.size(); keep-- > 1 && !placeFits;) {
            std::string cut(place, starts[keep]);
            while (!cut.empty() && cut.back() == ' ') cut.pop_back();
            if (!cut.empty()) placeFits = fits(line1 = where_line(1, cut + cut_mark())); /* the language's mark (2026-11-16) */
        }
    }
    pc_game_text_set_own(key, (line1 + "\n" + line2).c_str(), kFontHelp);
    std::printf("[load_save] help for slot %d: room %u, \"%s\", snagged %u, purified %u, %s (box %d px, text %d px)\n",
                s.slot, d.floor_id, place, snagged, purified, d.card_time >= 0 ? "card time" : "file time", boxW,
                help_width(key));
}

/* ---- CONTINUE's help line naming the loaded save (2026-11-16; snatcherLeo: "the helper line for CONTINUE will show
 * ... what save file is loaded", "truncate based on character length and all") ----
 * Why here: the main menu's help line is the game's (titleDrawOptionHelp, the help box menu 0x7F); for CONTINUE
 * (choice 1) OrreHook_TopMenuHelp now hands it this own message instead (gs_title.c). The text is one whole sentence per
 * language with the name in {name} (game.main_menu.continue_help), set when the main menu opens (the loaded save can
 * only change there: a pick in SAVE FILES, the PC screen's Main Menu re-read, a start) and when the language changes,
 * so drawing it each frame costs nothing. Stable id for the UI Editor: title.continue_help (its place and box are the
 * game's own). Platforms: all the same. */
constexpr s32 kChoiceContinue = 1;           /* the game's choice number for CONTINUE (gs_title.c's switch) */
constexpr u32 kKeyContinueHelp = 0xFFF00014; /* CONTINUE's line with the save's name, OrrePort's own message */
/* ---- the "GameCube-accurate" badge (deep gold, white letters; 2026-11-21, the project's build helper; snatcherLeo: "We could have a yellow
 * 'Gamecube-accurate' tag, and things that don't have it, and are enhanced, we just don't need that badge") ----
 * Why: in an accurate session the title's main menu shows the game's own CONTINUE line (OrrePort's naming line is off
 * there) and SAVE FILES lists only accurate saves, but nothing said the session was accurate. The hub's rule is the
 * same: only accurate saves get the yellow Accurate badge; Enhanced shows none.
 * How: one badge, the GCI tag's shape in deep gold with white letters (ensure_acc_badge / draw_acc_badge, by the tags'
 * code below), with OrrePort's own word (game.main_menu.accurate_tag) printed by the game's text code: on the help
 * box's top frame line, right-aligned, while the glove is on CONTINUE (OrreHook_TopMenuHelpDrawn, after the game printed
 * its line, with the help's own fade), and in the SAVE FILES list beside the GCI tag on the same frame line
 * (list_tag_word). The game's words are never
 * changed. Only in GameCube-accurate sessions and with the title's other OrrePort parts (enabled()). Stable id for the
 * UI Editor: title.accurate_tag. Look only: no random draws, no timing. Platforms: all the same. For XD: XD's title has
 * its own menu and help box; the same badge fits (likely).
 * *Changed the same night (snatcherLeo, on the first version's bare yellow letters: "we need a consistent look, but also on
 * the right, it needs to be an actual badge, like GCI"):* bare amber text became the badge in both places */
constexpr u32 kKeyAccTag = 0xFFF00016;      /* "GameCube-accurate", OrrePort's own message */
constexpr int kAccTagGap = 6;               /* pixels between the accurate badge and the GCI tag */
/* CONTINUE's badge on the help box (2026-11-21, top_menu_help_drawn): the box's bottom frame line's y and the box's
 * left edge for the badge (640 x 480; measured on a frame dump; checked on screen), and the default place */
constexpr int kHelpBottomLine = 446;
constexpr int kHelpBoxLeft = 95; /* the help text's own left edge (the text item's x) */
constexpr int kAccSpot = 1; /* bottom right: under the box's right end, nothing near it */
/* (*Removed 2026-11-21:* kAccTagDY, kHelpLineH and kAccBadgeLower placed the badge on CONTINUE's second text row and
 * lowered it twice at snatcherLeo's hand checks; since option A it sits on the box's frame line, top_menu_help_drawn) */
void refresh_language(); /* below: sets the own messages, the tag's included, for the game's current language */
constexpr u32 kKeyQuestion = 0xFFF00015;     /* the question after a pick in SAVE FILES, OrrePort's own message */
constexpr u32 kFontContinueHelp = 0x3C14;    /* the game's own CONTINUE line: its bank's font (font 0, the help font) */
constexpr u32 kFontQuestion = 0x3C22;        /* the game's own Continue question: its bank's font */
bool s_continueHelp = false;                 /* kKeyContinueHelp holds the loaded save's line */

/* make CONTINUE's line for the loaded save, or switch it off (the game's own line) when there is no named save with a
 * story: the portable layout off (no slots), an empty or damaged slot, a nameless one */
void update_continue_help() {
    s_continueHelp = false;
    if (!pc_saves_ready()) return;
    static PcSaveInfo s; /* about 1 KB: kept off the stack of a game task */
    if (!pc_saves_get(pc_saves_current(), &s) || !s.has_save || s.name[0] == '\0') return;
    const u8* item = static_cast<const u8*>(pc_game_ui.menu_item(kItemHelpText));
    const int boxW = item != nullptr ? (s16)((item[6] << 8) | item[7]) : 0;
    auto line = [](const std::string& name) { return gtx("game.main_menu.continue_help", {{"name", name}}); };
    /* too wide: the name is cut to the box with the shared helper (fit_name: whole characters, spaces before the mark
     * dropped), the whole sentence measured in the help line's own font, ending with the language's mark (cut_mark) */
    bool cut = false;
    const std::string name = fit_name(
        s.name, [&](const std::string& n) { return boxW <= 0 || measured_width(line(n), kFontContinueHelp) <= boxW; },
        cut_mark(), &cut);
    const std::string text = line(name);
    pc_game_text_set_own(kKeyContinueHelp, text.c_str(), kFontContinueHelp);
    s_continueHelp = true;
    std::printf("[load_save] CONTINUE's help line names slot %d (\"%s\")%s (box %d px, text %d px, language %s)\n", s.slot,
                s.name, cut ? ", cut to fit" : "", boxW, help_width(kKeyContinueHelp), pc_game_text_language());
}
u8* s_listItems = nullptr;
u8* s_listSprites = nullptr;
u8* s_menuRecord = nullptr;
bool s_listBuilt = false;

/* the frame pieces' colour, as the bag's page callback sets it for the Items page (the draw node's colour at +0x64) */
s32 list_frame_colour(void* /*ctx*/, u8* node) {
    node[0x64] = node[0x65] = node[0x66] = 0xC0;
    return 0;
}
/* the title, centred in its piece, as the bag's title callback draws its own (clip, print, clip off) */
s32 list_title(void* /*ctx*/, u8* node) {
    const PcGameUi& g = pc_game_ui;
    const int w = *(s16*)(node + 0x54), h = *(s16*)(node + 0x56);
    g.clip_set(0, 0, w, h);
    const int tw = (s16)(g.msg_rect(kKeyTitle) >> 16);
    g.msg_print(w / 2 - tw / 2, 0, -1, kKeyTitle);
    g.clip_reset();
    return 0;
}
/* the rows: each save's name, and its play time right-aligned, 31 px apart, clipped to the rows' piece (the bag's row
 * callback does the same with item names and counts) */
s32 list_rows(void* /*ctx*/, u8* node) {
    const PcGameUi& g = pc_game_ui;
    const int w = *(s16*)(node + 0x54), h = *(s16*)(node + 0x56);
    g.clip_set(0, 0, w, h);
    for (int i = 0; i < kRows && s_first + i < (int)s_saves.size(); ++i) {
        const u32 idx = (u32)(s_first + i);
        g.msg_print(0, i * kRowPitch, -1, kKeyName + idx);
        const int tw = (s16)(g.msg_rect(kKeyTime + idx) >> 16);
        g.msg_print(w - tw - 4, i * kRowPitch, -1, kKeyTime + idx);
        /* the region word, coloured, right-aligned kRegionGap before the play time (3.5 step 6) */
        if (s_saves[idx].has_save && region_key(s_saves[idx].region) != nullptr) {
            const int rw = (s16)(g.msg_rect(kKeyRegion + idx) >> 16);
            g.msg_print(w - tw - 4 - kRegionGap - rw, i * kRowPitch, (s32)region_colour(s_saves[idx].region),
                        kKeyRegion + idx);
        }
    }
    g.clip_reset();
    return 0;
}
/* the ring cursor and the highlight bar follow the cursor's row (the draw node's y at +0x52); the bar faint while
 * browsing (alpha 0x72 at +0x67, as the bag's) */
s32 list_on_row(void* /*ctx*/, u8* node) {
    *(s16*)(node + 0x52) = (s16)(kRingY + s_row * kRowPitch);
    return 0;
}
s32 list_bar(void* ctx, u8* node) {
    list_on_row(ctx, node);
    node[0x67] = 0x72;
    return 0;
}
/* the arrows: shown only when more saves lie that way, blinking as the bag's */
s32 list_arrow_down(void* /*ctx*/, u8* node) {
    const bool more = s_first + kRows < (int)s_saves.size();
    node[0x67] = more ? ((s_tick / 16) % 2 ? 0xFF : 0x60) : 0;
    return 0;
}
s32 list_arrow_up(void* /*ctx*/, u8* node) {
    node[0x67] = s_first > 0 ? ((s_tick / 16) % 2 ? 0xFF : 0x60) : 0;
    return 0;
}
/* the keys, every frame while the list is open: up / down move the cursor and scroll past five rows, with the bag's
 * cursor sound (0x23); A and B are the game's own button handling (menuButtonNormal, the record's +0x10).
 * Which key word: the repeat word (+6), as the game's own cursor code reads it (menuCursorNormal, menu.c: repeatKeys):
 * set on the press, then every 5 frames after the first 15 while the key stays down (the key update, menu.c).
 * *Corrected 2026-11-14 (the documentation pass):* 5d read "pressed (+4) | held (+8)"; +8 is set on every frame
 * once the first repeat has fired, so holding a direction would have moved one row per frame. Verified from menu.c's
 * key update (the held word keeps its bit from frame to frame after the first repeat). */
s32 list_keys(void* /*window*/) {
    ++s_tick;
    const uint8_t* in = pc_game_ui.window_keys();
    const u16 keys = *(const u16*)(in + 6); /* the window system's own record, native byte order on the PC */
    const int n = (int)s_saves.size();
    bool moved = false, scrolled = false;
    if ((keys & MENU_KEY_DOWN) && s_first + s_row + 1 < n) {
        if (s_row < kRows - 1) ++s_row;
        else ++s_first, scrolled = true;
        moved = true;
    } else if ((keys & MENU_KEY_UP) && s_first + s_row > 0) {
        if (s_row > 0) --s_row;
        else --s_first, scrolled = true;
        moved = true;
    }
    (void)scrolled; /* the rows print each save's own keys: nothing to re-set on a scroll */
    if (moved) pc_game_ui.sound(0x23), s_notice = 0; /* a refused pick's line goes when the cursor moves */
    return 0;
}

/* the two tag shapes in one RGBA8 texture ('ORTG'), side by side, each s_tagW[i] wide: a rounded rectangle (radius 4)
 * in the tag's colour with a 1 px rim 40 % lighter, its edges smoothed by 4 x 4 samples per pixel. The widths come
 * from the words as the game measures them in font 1, so a translated word gets a tag that fits it */
/* one tag's shape into an RGBA picture at x0: a rounded rectangle (radius 4) kTagH tall and w wide in the colour, with a
 * 1 px rim 40 % lighter, its edges smoothed by 4 x 4 samples per pixel (the GCI / ORRESAVE tags' and, since 2026-11-21,
 * the accurate badge's shape: one look for every badge) */
void draw_pill(std::vector<u8>& rgba, int texW, int x0, int width, const u8 colour[3]) {
    const float w = (float)width, h = (float)kTagH, r = 4.f;
    /* signed distance to the rounded rectangle's edge (negative inside) */
    auto dist = [&](float x, float y) {
        const float qx = std::fabs(x - w / 2) - (w / 2 - r), qy = std::fabs(y - h / 2) - (h / 2 - r);
        return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - r;
    };
    for (int y = 0; y < kTagH; ++y)
        for (int x = 0; x < width; ++x) {
            int inside = 0, rim = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    const float d = dist(x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4);
                    if (d < 0) ++inside, rim += d > -1.f;
                }
            if (inside == 0) continue;
            u8* p = &rgba[((size_t)y * texW + x0 + x) * 4];
            const float k = 0.4f * rim / inside; /* the rim's share of this pixel, lightened 40 % */
            for (int c = 0; c < 3; ++c) p[c] = (u8)std::lround(colour[c] + (255 - colour[c]) * k);
            p[3] = (u8)std::lround(255.f * inside / 16);
        }
}

bool build_tag_texture() {
    const PcGameUi& g = pc_game_ui;
    if (g.resource_find(kTexTag) != nullptr) return true;
    const char* words[2] = {"game.save_files.tag_gci", "game.save_files.tag_orresave"};
    for (int i = 0; i < 2; ++i) {
        pc_game_text_set_own(kKeyTag + i, gt(words[i]), kFontBag);
        s_tagW[i] = (s16)(g.msg_rect(kKeyTag + i) >> 16) + 2 * kTagPad;
    }
    const int texW = ((s_tagW[0] + s_tagW[1] + 3) / 4) * 4, texH = ((kTagH + 3) / 4) * 4; /* whole GX tiles */
    std::vector<u8> rgba((size_t)texW * texH * 4, 0);
    const u8 colours[2][3] = {{0x6A, 0x5F, 0xCB}, {0xC8, 0x45, 0x2E}}; /* the hub's .ftag gci / orre */
    for (int t = 0, x0 = 0; t < 2; x0 += s_tagW[t++]) draw_pill(rgba, texW, x0, s_tagW[t], colours[t]);
    void* tex = g.texture_create(texW, texH, 0x45 /* RGBA8 */);
    if (tex == nullptr) return false;
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(tex)), rgba, texW, texH);
    g.texture_unlock(tex);
    g.resource_register(tex, kResGroup, kTexTag);
    return true;
}

/* ---- the accurate badge (2026-11-21, reworked the same night; snatcherLeo, on the first version's bare yellow text: "on the
 * right, it needs to be an actual badge, like GCI" and "we need a consistent look") ----
 * The same shape as the GCI tag (draw_pill: kTagH tall, kTagPad a side, radius 4, the lighter rim), deep gold with white
 * letters (kAccFill, kAccInk: the colour's story there), the word in the bag's font 1 like the GCI tag's. Its
 * own texture ('ORTA', one pill as wide as the word in the game's language, redrawn when the language changes) and its
 * own sprite record (kSpriteAcc, served past the sprite table, menu_extra), so the main menu's help box can draw it
 * (draw_acc_badge, from OrreHook_TopMenuHelpDrawn) before the SAVE FILES list exists, and the list draws the same one.
 * UI Editor id: title.accurate_tag (its colours and place are the constants here). All platforms */
constexpr u32 kTexAcc = 0x4F525441;           /* 'ORTA' */
constexpr u32 kSpriteAcc = 0x7012;            /* past LOAD's two sprites (0x7010-0x7011) */
constexpr int kAccTexW = 256;                 /* room for the longest language's word (GameCube-accurate: 231 px) */
/* the pill: a deep, fully saturated gold, #AD8000, with white letters: 3.6:1 contrast (WCAG's formula), hue 44°, still
 * yellow, not orange or brown; the hub's Accurate badge uses the same since the same night (hub.rcss .ftag.acc), so
 * the badge matches everywhere. How it was chosen (snatcherLeo, 2026-11-21): white letters on the hub's old amber (#E0B45C)
 * gave only 1.9:1, #B98A2F 3.1:1, both read poorly; candidates gold #AD8000, orange #CC6600 (3.8:1) and green #3B8A3E
 * (4.3:1) were pictured; green was picked ("Green is cool"), then dropped ("Okay, let's not use green": the hub
 * already uses green for positive things), and gold kept */
constexpr u8 kAccFill[3] = {0xAD, 0x80, 0x00};
/* the letters: white, as the game's own text and the GCI tag's (snatcherLeo, 2026-11-21: "make it white colored text too for
 * the badge"; first dark #20160A as the hub's .ftag.acc). Font 1's white letters carry the game's own dark edge, which
 * keeps them readable on the amber. The low byte is the alpha */
constexpr s32 kAccInk = (s32)0xFFFFFF00;
u8* s_accSprite = nullptr;
int s_accW = 0;
std::string s_accLang;

/* the badge's word, texture and sprite for the game's current language (once per language); false: none to draw */
bool ensure_acc_badge() {
    const PcGameUi& g = pc_game_ui;
    const std::string lang = pc_game_text_language();
    if (s_accSprite != nullptr && lang == s_accLang) return true;
    pc_game_text_set_own(kKeyAccTag, gt("game.main_menu.accurate_tag"), kFontBag);
    const int tw = (s16)(g.msg_rect(kKeyAccTag) >> 16);
    s_accW = std::min(tw + 2 * kTagPad, kAccTexW);
    const int texH = ((kTagH + 3) / 4) * 4; /* whole GX tiles */
    std::vector<u8> rgba((size_t)kAccTexW * texH * 4, 0);
    /* ORRE_ACC_BADGE_RGB=RRGGBB (a test switch, never a setting; 2026-11-21): another fill, for pictures of candidate
     * colours side by side */
    u8 fill[3] = {kAccFill[0], kAccFill[1], kAccFill[2]};
    if (const char* v = SDL_getenv("ORRE_ACC_BADGE_RGB")) {
        const unsigned long c = std::strtoul(v, nullptr, 16);
        fill[0] = (u8)(c >> 16), fill[1] = (u8)(c >> 8), fill[2] = (u8)c;
    }
    draw_pill(rgba, kAccTexW, 0, s_accW, fill);
    void* tex = g.resource_find(kTexAcc);
    const bool fresh = tex == nullptr;
    if (fresh) tex = g.texture_create(kAccTexW, texH, 0x45 /* RGBA8 */);
    if (tex == nullptr) return false;
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(tex)), rgba, kAccTexW, texH);
    g.texture_unlock(tex);
    if (fresh) g.resource_register(tex, kResGroup, kTexAcc);
    if (s_accSprite == nullptr) s_accSprite = static_cast<u8*>(pc_lowmem_alloc(0x18));
    if (s_accSprite == nullptr) return false;
    std::memcpy(s_accSprite, g.menu_sprite(kSpriteQuit), 0x18); /* a plain picture record: its type and offsets kept */
    s_accSprite[0x07] = 0xFF;
    put16(s_accSprite, 0x08, 0), put16(s_accSprite, 0x0A, 0), put16(s_accSprite, 0x0C, (u16)s_accW);
    put16(s_accSprite, 0x0E, (u16)kTagH), put32(s_accSprite, 0x10, kTexAcc);
    s_accLang = lang;
    std::printf("[load_save] the accurate badge for \"%s\": %d px (word %d px)\n", lang.c_str(), s_accW, tw);
    return true;
}

/* the badge with its top-left at (x, y) in the calling callback's own coordinates, at the window's fade (alpha): the
 * pill, then the word centred on it as the GCI tag's (font 1's capitals start 5 px below the print y: up 3) */
void draw_acc_badge(void* window, int x, int y, u32 alpha) {
    if (!ensure_acc_badge()) return;
    const PcGameUi& g = pc_game_ui;
    g.draw_sprite_sized((int16_t)x, (int16_t)y, (int16_t)s_accW, (int16_t)kTagH, 0xFFFFFF00u | (alpha & 0xFF), window,
                        kSpriteAcc);
    const int tw = (s16)(g.msg_rect(kKeyAccTag) >> 16);
    g.msg_print(x + s_accW / 2 - tw / 2, y - 3, kAccInk | (s32)(alpha & 0xFF), kKeyAccTag);
}

/* which tag the highlighted save shows: 0 GCI, 1 ORRESAVE, -1 none (no save there) */
int tag_now() {
    const int sel = s_first + s_row;
    if (sel >= (int)s_saves.size() || !s_saves[sel].has_save) return -1;
    return s_saves[sel].is_orresave != 0 ? 1 : 0;
}
/* a tag's shape: shown only for the highlighted save's format (node +0x67, the alpha) */
s32 list_tag_gci(void* /*ctx*/, u8* node) { node[0x67] = tag_now() == 0 ? 0xFF : 0; return 0; }
s32 list_tag_orre(void* /*ctx*/, u8* node) { node[0x67] = tag_now() == 1 ? 0xFF : 0; return 0; }
/* the tag's word, from its own item drawn after the shapes: an item's callback runs before its own picture is drawn
 * (verified 2026-11-14: a word printed from the shape's callback was covered by the shape), so the word needs an item
 * without a picture, as the game's own title text (item 0x214) has. White, font 1, centred on the shown tag (the
 * item spans the wider tag; both tags share the right edge) */
s32 list_tag_word(void* ctx, u8* node) {
    const int t = tag_now();
    const PcGameUi& g = pc_game_ui;
    const int w = *(s16*)(node + 0x54), h = *(s16*)(node + 0x56);
    /* the accurate badge (draw_acc_badge above), in an accurate session: on the same frame line, at the format tag's
     * height, kAccTagGap left of it, as the hub's details show the GCI tag and the Accurate badge side by side. Drawn
     * before the clip below, as it lies left of this item. *Changed 2026-11-21:* first bare yellow letters beside the
     * SAVE FILES heading (its bar too narrow: cut off), then bare letters here; now the badge itself (snatcherLeo: "an actual
     * badge, like GCI") */
    if (pc_option_gamecube_accurate()) {
        ensure_acc_badge();
        draw_acc_badge(ctx, w - s_tagW[t == 1 ? 1 : 0] - kAccTagGap - s_accW, 0, 0xFF);
    }
    if (t < 0) return 0;
    const int tw = (s16)(g.msg_rect(kKeyTag + t) >> 16);
    const int left = w - s_tagW[t]; /* the shown tag's left edge inside this item */
    g.clip_set(0, 0, w, h);
    /* font 1's capitals start 5 px below the print y (measured on the first tag); up 3 centres their 13 px in the
     * 18 px tag, the letters' dark edge taking the extra pixel below */
    g.msg_print(left + s_tagW[t] / 2 - tw / 2, -3, -1, kKeyTag + t);
    g.clip_reset();
    return 0;
}

u32 fn32(const void* f) { return (u32)(uintptr_t)f; } /* below 4 GB: the exe's image base (pc/ptr32.h) */

/* the list's records, once, the first time LOAD is pressed (not at start-up: nothing is spent unless it is used) */
bool build_list() {
    if (s_listBuilt) return true;
    if (!s_bagReady) return false;
    const PcGameUi& g = pc_game_ui;
    s_listSprites = static_cast<u8*>(pc_lowmem_alloc(0x18 * kListSpriteCount));
    s_listItems = static_cast<u8*>(pc_lowmem_alloc(0x1C * kListItemCount));
    s_menuRecord = static_cast<u8*>(pc_lowmem_alloc(0x1C));
    if (s_listSprites == nullptr || s_listItems == nullptr || s_menuRecord == nullptr) return false;
    for (int i = 0; i < 0x1D; ++i) { /* the bag's sprites, on OrrePort's copy of the texture */
        u8* s = s_listSprites + i * 0x18;
        std::memcpy(s, g.menu_sprite(0xC0 + i), 0x18);
        put32(s, 0x10, kTexBag);
    }
    s_listSprites[0x07] = 0xF2; /* the panel (0xC0's copy) 95 % solid (the bag's: 0xCC, 80 %) */
    u8* title = s_listSprites + 0x1D * 0x18; /* the title bar alone */
    std::memcpy(title, g.menu_sprite(0xCC), 0x18);
    put32(title, 0x10, kTexBag), put16(title, 0x0E, 36);
    auto spr = [](u32 bagSprite) { return (u16)(kListSpriteBase + (bagSprite - 0xC0)); };
    auto item = [&](int i) { return s_listItems + i * 0x1C; };
    auto from = [&](int i, int bagItem) {
        std::memcpy(item(i), g.menu_item(bagItem), 0x1C);
        u8* it = item(i);
        const s16 x = (s16)((it[2] << 8) | it[3]), y = (s16)((it[4] << 8) | it[5]);
        put16(it, 2, (u16)(x + kBagDX)), put16(it, 4, (u16)(y + kBagDY));
        put32(it, 0x14, 0);
        return it;
    };
    u8* it;
    it = from(kL_Panel, 0x1F9), put16(it, 8, (u16)(283 - kShorter)), put16(it, 0x0A, spr(0xC0));
    it = from(kL_Bar, 0x1FA), put16(it, 0x0A, spr(0xD0)), put32(it, 0x14, fn32((void*)list_bar));
    it = from(kL_CapR, 0x1FB), put16(it, 0x0A, spr(0xD2)), put32(it, 0x14, fn32((void*)list_bar));
    it = from(kL_CapL, 0x1FC), put16(it, 0x0A, spr(0xD2)), put32(it, 0x14, fn32((void*)list_bar));
    it = from(kL_Rows, 0x1FE), put16(it, 8, (u16)(kRows * kRowPitch)), put32(it, 0x14, fn32((void*)list_rows));
    it = from(kL_Ring, 0x213), put16(it, 0x0A, spr(0xCF)), put32(it, 0x14, fn32((void*)list_on_row));
    it = from(kL_ArrowDown, 0x211), put16(it, 4, (u16)(413 + kBagDY - kShorter)), put16(it, 0x0A, spr(0xDB));
    put32(it, 0x14, fn32((void*)list_arrow_down));
    it = from(kL_ArrowUp, 0x212), put16(it, 0x0A, spr(0xDC)), put32(it, 0x14, fn32((void*)list_arrow_up));
    for (const auto [i, bag] : {std::pair{kL_SideL, 0x1FF}, std::pair{kL_SideR, 0x200}}) {
        it = from(i, bag), put16(it, 8, (u16)(219 - kShorter)), put16(it, 0x0A, spr(0xC2)), put32(it, 0x14, fn32((void*)list_frame_colour));
    }
    for (const auto [i, bag] : {std::pair{kL_BotR, 0x201}, std::pair{kL_BotL, 0x202}}) {
        it = from(i, bag), put16(it, 4, (u16)(383 + kBagDY - kShorter)), put16(it, 0x0A, spr(0xC1));
        put32(it, 0x14, fn32((void*)list_frame_colour));
    }
    for (const auto [i, bag] : {std::pair{kL_TitleL, 0x20A}, std::pair{kL_TitleR, 0x209}}) {
        it = from(i, bag), put16(it, 4, (u16)kTitleY), put16(it, 8, 36), put16(it, 0x0A, (u16)kListSpriteTitle);
        put32(it, 0x14, fn32((void*)list_frame_colour));
    }
    for (const auto [i, bag] : {std::pair{kL_TopL, 0x20A}, std::pair{kL_TopR, 0x209}}) {
        it = from(i, bag), put16(it, 4, (u16)(kTitleY + 38)), put16(it, 8, 56), put16(it, 0x0A, spr(0xCB));
        put32(it, 0x14, fn32((void*)list_frame_colour));
    }
    it = from(kL_TitleText, 0x214), put16(it, 4, (u16)(kTitleY + 6)), put32(it, 0x14, fn32((void*)list_title));
    /* the tags: sprites cropped from 'ORTG' (copies of the panel's sprite record for its type and offsets), items copied
     * from the panel's and placed on the help box's corner; without the tag texture they stay hidden */
    const bool tags = build_tag_texture();
    for (int t = 0; t < 2; ++t) {
        u8* sp = s_listSprites + (0x1E + t) * 0x18;
        std::memcpy(sp, g.menu_sprite(0xC0), 0x18);
        sp[0x07] = 0xFF;
        put16(sp, 0x08, (u16)(t == 0 ? 0 : s_tagW[0])), put16(sp, 0x0A, 0), put16(sp, 0x0C, (u16)s_tagW[t]);
        put16(sp, 0x0E, (u16)kTagH), put32(sp, 0x10, kTexTag);
        it = from(t == 0 ? kL_TagGci : kL_TagOrre, 0x1F9);
        put16(it, 2, (u16)(kTagRight - s_tagW[t])), put16(it, 4, (u16)kTagTop);
        put16(it, 6, (u16)s_tagW[t]), put16(it, 8, (u16)kTagH), put16(it, 0x0A, (u16)(kListSpriteTags + t));
        put32(it, 0x14, tags ? fn32(t == 0 ? (void*)list_tag_gci : (void*)list_tag_orre) : 0);
        if (!tags) it[1] = 0; /* the item's own alpha: hidden */
    }
    {   /* the word over whichever tag shows: a copy of the bag's title text item (no picture), spanning the wider tag */
        const int wide = std::max(s_tagW[0], s_tagW[1]);
        it = from(kL_TagWord, 0x214);
        put16(it, 2, (u16)(kTagRight - wide)), put16(it, 4, (u16)kTagTop), put16(it, 6, (u16)wide);
        put16(it, 8, (u16)kTagH), put32(it, 0x14, tags ? fn32((void*)list_tag_word) : 0);
    }
    it = from(kL_Select, 0x20B), put16(it, 0x0A, 0), put32(it, 0x14, 0); /* selectable, no picture: A / B close */
    for (int i = 0; i < kListItemCount; ++i) { /* the chain, in drawing order; the last item ends it */
        item(i)[0] &= (u8)~0x40;
        put16(item(i), 0x18, (u16)(kListItemBase + i + 1));
    }
    item(kL_Select)[0] |= 0xC0, put16(item(kL_Select), 0x18, 0);
    /* the menu record: as the bag's (sound group 1, C callbacks), no remembered cursor (slot 0, as the main menu's), the
     * first item, the keys callback and the game's own button handling; no open / close animation yet */
    std::memset(s_menuRecord, 0, 0x1C);
    s_menuRecord[0] = 0x61, s_menuRecord[1] = 0x90;
    put16(s_menuRecord, 4, (u16)kListItemBase);
    put32(s_menuRecord, 0x0C, fn32((void*)list_keys));
    put32(s_menuRecord, 0x10, g.menu_button_normal_addr);
    /* the title in the bag's own font, as ITEMS (snatcherLeo: "Make Save Files look like ITEMS and make it seated better"):
     * borrowed from the main menu's help it was font 0, lighter and larger than the bag's font 1 */
    pc_game_text_set_own(kKeyTitle, gt("game.main_menu.save_files_title"), kFontBag);
    /* the refused-pick lines (rules R2 / R3), in the help box's font */
    pc_game_text_set_own(kKeyNoticeGone, gt("game.save_files.gone"), kFontHelp);
    pc_game_text_set_own(kKeyNoticeDamaged, gt("game.card.damaged"), kFontHelp);
    s_listBuilt = true;
    std::printf("[load_save] the SAVE FILES list is ready\n");
    return true;
}

/* the game asks for a menu record past its table (OrreHook_MenuDataExtra): the list's, once built */
void* menu_data_extra(u32 index) { return index == kMenuSaveList && s_listBuilt ? s_menuRecord : nullptr; }

/* the game asks for an item (kind 0) or sprite (kind 1) past its tables (OrreHook_MenuExtra): the button's, then the
 * list's; anything else goes to the hook registered before this one (the Options Language row's), so both coexist */
void* menu_extra(int kind, u32 index) {
    if (kind == 0 && s_items != nullptr && index >= (u32)kItemBase && index < (u32)(kItemBase + kItemCount))
        return s_items + (size_t)(index - kItemBase) * 0x1C;
    if (kind == 1 && s_sprites != nullptr && index >= kSpriteBase && index < kSpriteBase + 2)
        return s_sprites + (size_t)(index - kSpriteBase) * 0x18;
    if (kind == 1 && index == kSpriteAcc && s_accSprite != nullptr) return s_accSprite; /* the accurate badge */
    if (kind == 0 && s_listItems != nullptr && index >= (u32)kListItemBase && index < (u32)(kListItemBase + kListItemCount))
        return s_listItems + (size_t)(index - kListItemBase) * 0x1C;
    if (kind == 1 && s_listSprites != nullptr && index >= kListSpriteBase && index < kListSpriteBase + kListSpriteCount)
        return s_listSprites + (size_t)(index - kListSpriteBase) * 0x18;
    return s_prevMenuExtra != nullptr ? s_prevMenuExtra(kind, index) : nullptr;
}

/* each time the main menu opens (OrreHook_TopMenuOpen): the button made once, the row laid out every time (the game
 * may rebuild its item table between visits, so the layout is re-applied; it is idempotent).
 * *2026-11-15:* the pellets follow the game's language: made again when it differs from the one they were made for
 * (the Options screen's Language row changes it; the game comes back to the main menu through here, since the hook
 * now runs at every open, gs_title.c). A rebuild reads the language's disc and traces the word: about 0.1 s, once. */
bool s_askPicked = false;   /* a save was just picked in SAVE FILES: the next Continue question is OrrePort's */
bool s_askingPicked = false; /* OrrePort's question is open: its cursor starts on YES */
bool s_quietLoad = false;    /* the picked save's card read: its "loaded" message is left out (the question says it) */

void top_menu_open() {
    /* the main menu opening again means no question followed a pick (a refused or aborted one): nothing stays armed */
    s_askPicked = s_askingPicked = false;
    if (!enabled() || !build()) return;
    if (s_picLang != pc_game_text_language()) {
        make_pictures(pc_game_text_language()); /* a failure keeps the last pellets and tries again next open */
        pc_title_disc_done();                   /* the US Battle Mode archive, if English needed it, is dropped again */
    }
    apply_layout();
    update_continue_help(); /* the loaded save may have changed (a pick, the PC screen's Main Menu re-read) */
}

/* A on a main-menu choice the game doesn't know (OrreHook_TopMenuChoice; the game's own choices 0-5 never reach here):
 * choice 6 is LOAD. Returns 1 = handled, with *nextState the title's state to go on with (1: the main menu again) */
int open_list_once(int keepSlot, int* picked);
int pick_save(const PcSaveInfo& s, s32* nextState);

int top_menu_choice(s32 choice, s32* nextState) {
    if (!s_built || choice != kChoiceLoadSave) return 0;
    *nextState = 1; /* back to the main menu afterwards */
    if (!build_list()) {
        std::printf("[load_save] LOAD chosen, but the save list couldn't be made: staying on the menu\n");
        return 1;
    }
    /* the cursor starts on the first row (see open_list_once); after a refused pick (rules R2 / R3) it stays on that row */
    int keepSlot = -1;
    s_notice = 0;
    for (;;) {
        int picked = 0;
        const int sel = open_list_once(keepSlot, &picked);
        if (picked < 0 || sel < 0) {
            std::printf("[load_save] the save list closed (choice %d)\n", picked);
            return 1; /* B: back to the main menu, nothing changed */
        }
        const PcSaveInfo s = s_saves[sel];
        /* R3: the file went away (or was replaced by an unreadable one) since the list was read: say so, read the
         * folder again (its row drops out) and open the list again; the loaded save stays loaded */
        SDL_PathInfo pi;
        const bool gone = (s.has_save || s.damaged) && !SDL_GetPathInfo(s.file, &pi);
        if (gone) {
            std::printf("[load_save] picked slot %d (%s), but its save file is gone: the list again\n", s.slot, s.name);
            s_notice = kKeyNoticeGone;
            keepSlot = s.slot;
            continue;
        }
        /* R2: a damaged save isn't loaded: the approved line says why and where to go */
        if (s.damaged) {
            std::printf("[load_save] picked slot %d (%s), but its save file is damaged: the list again\n", s.slot, s.name);
            s_notice = kKeyNoticeDamaged;
            keepSlot = s.slot;
            continue;
        }
        return pick_save(s, nextState);
    }
}

/* the list read and opened once, the cursor on keepSlot (or the first row); returns the picked row, -1 when none, and
 * the menu's answer in *picked (-1 for B) */
int open_list_once(int keepSlot, int* picked) {
    const Uint64 started = SDL_GetTicksNS(); /* how long opening takes, in the log (it was 85 ms; one frame is 16.7) */
    /* a .gci dropped into Saves\ while the game runs becomes a slot now (rule R6), then the save files, in the hub's
     * order (read afresh every time: rule R1; the files' facts are remembered while unchanged, pc_saves.c) */
    pc_saves_import_loose();
    static PcSaveInfo buf[kMaxSaves]; /* about 85 KB (since 2026-11-16 the trainer holds a whole 12-letter name, up to 529
                                         bytes): kept off the stack of a game callback */
    const int n = pc_saves_list(buf, kMaxSaves);
    s_saves.assign(buf, buf + std::max(0, n));
    /* a test switch for the ORRESAVE tag until .orresave files exist (a later build): ORRE_TEST_ORRESAVE=<slot>
     * marks that slot's save as .orresave in this list only (before the accurate-mode filter, so it tests that too) */
    if (const char* test = SDL_getenv("ORRE_TEST_ORRESAVE"))
        for (PcSaveInfo& s : s_saves)
            if (s.slot == std::atoi(test)) s.is_orresave = 1;
    /* rule R3: a slot whose save file is gone but which has earlier versions kept is "save missing" (the hub offers to
     * restore it; the project plan's 2.1 edge cases), not empty: it isn't listed, so a pick can't start a new game over
     * a save waiting to be restored. A slot with no file and no versions is new and empty, and is listed */
    s_saves.erase(std::remove_if(s_saves.begin(), s_saves.end(),
                                 [](const PcSaveInfo& s) {
                                     static PcSaveVersion v[1];
                                     return !s.has_save && !s.damaged && s.file[0] == '\0' && pc_saves_versions(s.slot, v, 1) > 0;
                                 }),
                  s_saves.end());
    /* GameCube-accurate mode lists only the saves a GameCube could play: .gci, and empty slots (a new game there is
     * one); an .orresave holds things a console save can't (the project plan, the title-saves section) */
    /* *Changed 2026-11-16 (2.12, accurate mode per save):* the list shows only the saves whose own setting matches the
     * running game's mode, in both modes. Why: the mode is fixed while the game runs (snatcherLeo, 2026-11-10: never switched
     * in game), and a save always opens in its own mode (an accurate save always opens accurate), so a save of the
     * other mode can't be loaded here; it is played from the hub, which starts the game in that save's mode. An
     * .orresave never has accurate mode on (pc_saves.c), so the accurate list still holds .gci saves only. */
    {
        const int accurate = pc_option_gamecube_accurate() != 0;
        s_saves.erase(std::remove_if(s_saves.begin(), s_saves.end(),
                                     [accurate](const PcSaveInfo& s) {
                                         return (s.accurate != 0) != accurate || (accurate && s.is_orresave != 0);
                                     }),
                      s_saves.end());
    }
    /* *Removed 2026-11-21 (snatcherLeo: "you can only load an autosave from hub"):* the autosave row under each save (added
     * 2026-11-20). The list holds the saves themselves only; the autosave stays in the hub and the hang watch's offer */
    s_helpReady.assign(s_saves.size(), 0); /* re-read each time the list opens: the saves may have changed */
    /* the list opens at its top, the cursor on the first save. *Changed 2026-11-15* (snatcherLeo: "it always starts me at the
     * bottom of the SAVE FILES menu"): it opened on the loaded save, which sat on the last row whenever that save was
     * further down than five. keepSlot (-1 at first) keeps the row when the list is opened again after a refused pick */
    int at = 0;
    for (int i = 0; i < (int)s_saves.size(); ++i)
        if (s_saves[i].slot == keepSlot) at = i;
    s_first = std::max(0, at - (kRows - 1));
    s_row = at - s_first, s_tick = 0;
    set_row_texts();
    ensure_help(s_first + s_row); /* the first help line, made now so it is in the time below */
    std::printf("[load_save] the save list opened in %.1f ms (%d saves)\n", (double)(SDL_GetTicksNS() - started) / 1e6,
                (int)s_saves.size());
    /* the list opens over the main menu and the game waits for its answer, as the main menu waits for its own */
    s_listOpen = true; /* the help box shows the highlighted save meanwhile (top_menu_help) */
    /* the main menu dimmed behind the list (design, mockup 80): its window's colour (+0x88 R, G, B) at 50 % grey, which
     * the game's sprite drawing multiplies into every piece that window draws (the panels, the buttons, the glove);
     * the help box and the list are windows of their own and stay bright. The same grey the game itself uses to darken
     * a fainted Pokémon's portrait (menus.md). The window's own colour is put back as the list closes. */
    const uint32_t mainWindow = pc_game_ui.window_active_id();
    u8* win = pc_game_ui.window_find(mainWindow);
    u8 saved[3] = {0xFF, 0xFF, 0xFF};
    if (win != nullptr) std::memcpy(saved, win + 0x88, 3), std::memset(win + 0x88, 0x80, 3);
    *picked = pc_game_ui.menu_open_wait(kMenuSaveList, mainWindow);
    pc_game_ui.menu_close(kMenuSaveList); /* the game opens and closes its menus separately (titleMenuMain does too) */
    if ((win = pc_game_ui.window_find(mainWindow)) != nullptr) std::memcpy(win + 0x88, saved, 3);
    s_listOpen = false;
    const int sel = s_first + s_row;
    return *picked >= 0 && sel < (int)s_saves.size() ? sel : -1;
}

/* A picked a save (plan step 7): switch the card to it unless it's the one loaded now, let the game read it as the
 * title does, then go on with the title's own states: 2 (Continue: the save's summary and "Is it okay to continue
 * from here?") for a save, 5 (New Game's question) for an empty one */
int pick_save(const PcSaveInfo& s, s32* nextState) {
    const int was = pc_saves_current();
    int hasSave = s.has_save;
    /* one question instead of two windows (snatcherLeo, 2026-11-15 / 16): for a save with a story, outside GameCube-accurate
     * mode (there the game's own "loaded" message and its own question stay). An empty save keeps the game's card flow
     * and New Game's question unchanged */
    const bool ask = s.has_save && !pc_option_gamecube_accurate();
    if (s.slot != was) {
        if (!pc_card_switch(s.slot)) {
            std::printf("[load_save] picked slot %d (%s), but the card couldn't switch to it: staying on slot %d\n",
                        s.slot, s.name, was);
            return 1;
        }
        s_quietLoad = ask;
        hasSave = pc_game_ui.card_load_title();
        s_quietLoad = false;
        if (hasSave < 0) { /* the card task aborted: back to the save that was loaded, read again */
            std::printf("[load_save] slot %d couldn't be read: back to slot %d\n", s.slot, was);
            if (pc_card_switch(was)) pc_game_ui.card_load_title();
            return 1;
        }
    }
    *nextState = hasSave ? 2 : 5;
    s_askPicked = ask && hasSave; /* state 2 goes straight on to state 3, the Continue question (gs_title.c) */
    std::printf("[load_save] picked slot %d (%s): %s\n", s.slot, s.name,
                !hasSave ? "New Game's question" : s_askPicked ? "Continue, OrrePort's question" : "Continue");
    return 1;
}

/* the card read's own "Your save file has been loaded!" (error 12, message 0x44E5) while the picked save is read
 * (OrreHook_CardMessageSkip): left out, because the question right after says it in its first line. A failed read
 * keeps its own message (other errors), and every other load (the start, the PC screen's Main Menu, which has its own
 * rule in pc_pc_screen.cpp, chained) keeps it too. All platforms */
s32 (*s_prevCardMessageSkip)(s32, s32) = nullptr;
s32 card_message_skip(s32 error, s32 taskKind) {
    if (s_quietLoad && error == 12) {
        std::printf("[load_save] the picked save's \"loaded\" message left out (the question says it)\n");
        return 1;
    }
    return s_prevCardMessageSkip != nullptr ? s_prevCardMessageSkip(error, taskKind) : 0;
}

/* the Continue question's message (OrreHook_ContinueQuestion), once after a pick: the game's own "loaded" words
 * (game.card.load_done: what message 0x44E5 shows on the PC, in the game's language) and "Continue your story now?"
 * (game.main_menu.continue_now), two whole sentences, one per line, in the same window the game opens for its own
 * question (window 1, over the Continue screen). Every other time the game's 0x3C22 */
u32 continue_question(u32 key) {
    if (!s_askPicked) return key;
    s_askPicked = false;
    if (pc_option_gamecube_accurate()) return key; /* switched on between the pick and here: the game's own */
    const std::string text = std::string(gt("game.card.load_done")) + "\n" + gt("game.main_menu.continue_now");
    pc_game_text_set_own(kKeyQuestion, text.c_str(), kFontQuestion);
    s_askingPicked = true;
    std::printf("[load_save] the question after the pick: OrrePort's (language %s), YES first\n", pc_game_text_language());
    return kKeyQuestion;
}

/* OrrePort's own lines here, set again when the game's language has changed since they were set (snatcherLeo, 2026-11-15:
 * after German, then English picked in Options, LOAD's help line stayed German: it was set once, when LOAD was built;
 * the game's own help lines are looked up each time). The SAVE FILES list's title and notices too; its tags are a
 * picture and keep their first language until the next start. */
void refresh_language() {
    static std::string setFor;
    const std::string lang = pc_game_text_language();
    if (lang == setFor) return;
    setFor = lang;
    pc_game_text_set_own(kKeyHelp, gt("game.main_menu.load_save_help"), 0x3C15);
    pc_game_text_set_own(kKeyAccTag, gt("game.main_menu.accurate_tag"), kFontBag); /* the accurate tag (below) */
    if (s_listBuilt) {
        pc_game_text_set_own(kKeyTitle, gt("game.main_menu.save_files_title"), kFontBag);
        pc_game_text_set_own(kKeyNoticeGone, gt("game.save_files.gone"), kFontHelp);
        pc_game_text_set_own(kKeyNoticeDamaged, gt("game.card.damaged"), kFontHelp);
    }
    std::fill(s_helpReady.begin(), s_helpReady.end(), 0); /* each save's line is made again in the new language */
    update_continue_help();                               /* CONTINUE's line too */
}

/* the help line under the main menu (OrreHook_TopMenuHelp). For CONTINUE (choice 1): the loaded save's line, or 0 for
 * the game's own (GameCube-accurate mode, no named save with a story). For LOAD (choice 6, past the game's six; 0 =
 * print nothing): LOAD's line; the help box keeps drawing while the list is open (the glove stays on LOAD), so then it
 * shows the highlighted save. Every other choice: 0, the game's own line */
/* the game's own help line was just printed (OrreHook_TopMenuHelpDrawn): in an accurate session, the accurate tag
 * beside CONTINUE's line, in the help's own fade (kKeyAccTag above) */
void top_menu_help_drawn(s32 choice, u32 key, u32 alpha, void* window) {
    if (!s_built || choice != kChoiceContinue || !pc_option_gamecube_accurate()) return;
    if (!ensure_acc_badge()) return;
    (void)key;
    /* *Changed 2026-11-21 (snatcherLeo chose A):* on the help box's top frame line, right-aligned, at the same height and edge
     * as the SAVE FILES list's tags on the same box (kTagTop, kTagRight: the GCI tag's place), so it never touches the
     * text in any language. Before, it sat on the text's second row: in Japanese it covered the end of the sentence
     * ("シナリオモードを つづけ…" under "ゲームキューブ再現"). The help callback draws from its text item's place
     * (menu item kItemHelpText: x at +2, y at +4, the 640 x 480 screen), so the screen place is made relative to it */
    const u8* item = static_cast<const u8*>(pc_game_ui.menu_item(kItemHelpText));
    if (item == nullptr) return;
    const int ix = (s16)((item[2] << 8) | item[3]), iy = (s16)((item[4] << 8) | item[5]);
    /* *Changed 2026-11-21 (snatcherLeo: "it does get covered by OPTIONS. Hmm.... maybe we move it more to the left? ... so it
     * isn't covered by anything"):* on the top frame line its right part sat under the OPTIONS button (x 222-423 at
     * 640 x 480, its bottom at about y 341; the button is drawn over the help box), and no stretch of the top line is
     * free of the three buttons and wide enough for the longest word (LOAD 62-160, OPTIONS 222-423, Quit 490-585; the
     * badge is 178-231 px). The help box's bottom frame line (about y 446) has nothing above or below it, whichever item
     * the glove is on. kAccSpot picks the place; ORRE_ACC_BADGE_SPOT=top | bottom-right | bottom-left | bottom-centre (a
     * test switch, for the candidate pictures) overrides it */
    static int spot = -1; /* 0 top (the old place), 1 bottom right, 2 bottom left, 3 bottom centre */
    if (spot < 0) {
        spot = kAccSpot;
        if (const char* v = SDL_getenv("ORRE_ACC_BADGE_SPOT")) {
            spot = SDL_strcmp(v, "top") == 0 ? 0 : SDL_strcmp(v, "bottom-left") == 0 ? 2
                 : SDL_strcmp(v, "bottom-centre") == 0 ? 3 : 1;
        }
    }
    const int top = spot == 0 ? kTagTop : kHelpBottomLine - kTagH / 2;
    const int left = spot == 2 ? kHelpBoxLeft : spot == 3 ? (kHelpBoxLeft + kTagRight - s_accW) / 2 : kTagRight - s_accW;
    static bool logged = false;
    if (!logged) {
        logged = true;
        std::printf("[load_save] the accurate badge on the help box's frame (spot %d; text item at %d,%d; badge %d px at "
                    "%d,%d)\n", spot, ix, iy, s_accW, left, top);
    }
    draw_acc_badge(window, left - ix, top - iy, alpha);
}

u32 top_menu_help(s32 choice) {
    if (!s_built) return 0;
    if (choice == kChoiceContinue) {
        if (pc_option_gamecube_accurate()) return 0;
        refresh_language();
        return s_continueHelp ? kKeyContinueHelp : 0;
    }
    if (choice != kChoiceLoadSave) return 0;
    refresh_language();
    if (s_listOpen && s_notice != 0) return s_notice; /* a refused pick's reason (R2 / R3) */
    if (s_listOpen && s_first + s_row < (int)s_saves.size()) {
        ensure_help(s_first + s_row);
        return kKeyHelpSave + (u32)(s_first + s_row);
    }
    return kKeyHelp;
}

/* the Continue question's first answer (OrreHook_ContinueInitialAnswer): YES (0) when the enhancement is on (snatcherLeo,
 * 2026-11-14: "YES everywhere by default"); pc_options keeps it off in GameCube-accurate mode and automated runs, where
 * the game's own NO stays so recorded presses mean what they did on the console */
s32 continue_initial(s32 initial) {
    /* OrrePort's own question after a pick in SAVE FILES starts on YES always (snatcherLeo, 2026-11-15: "YES first"): the
     * player has just chosen that save */
    if (s_askingPicked) {
        s_askingPicked = false;
        return 0;
    }
    return pc_option("continue_yes") ? 0 : initial;
}

/* the New Game question's first answer (OrreHook_NewGameInitialAnswer): YES (0) only when the save holds no story, so
 * nothing can be lost (snatcherLeo, 2026-11-14: "if a user DOESN'T have a story mode to continue from ... YES will be the
 * default ... if a user DOES have a story mode save ... it still stays on "NO" ... just to protect users"). hasStory is
 * the game's own test (save status 4). Part of the Continue YES switch (snatcherLeo: "that has to be apart of the same tweak
 * option"). pc_options keeps it off in GameCube-accurate mode and automated runs */
s32 new_game_initial(s32 initial, s32 hasStory) { return (!hasStory && pc_option("continue_yes")) ? 0 : initial; }

} // namespace

/* called once at start-up (pc_platform.c), after pc_options_lang_start (its item hook is chained) */
extern "C" void pc_title_loadsave_start(void) {
    s_prevMenuExtra = OrreHook_MenuExtra;
    OrreHook_MenuExtra = menu_extra;
    OrreHook_TopMenuOpen = top_menu_open;
    OrreHook_TopMenuChoice = top_menu_choice;
    OrreHook_TopMenuHelp = top_menu_help;
    OrreHook_TopMenuHelpDrawn = top_menu_help_drawn;
    OrreHook_ContinueInitialAnswer = continue_initial;
    OrreHook_ContinueQuestion = continue_question;
    s_prevCardMessageSkip = OrreHook_CardMessageSkip; /* pc_pc_screen.cpp registers later and chains this one */
    OrreHook_CardMessageSkip = card_message_skip;
    OrreHook_NewGameInitialAnswer = new_game_initial;
    OrreHook_MenuDataExtra = menu_data_extra;
}
