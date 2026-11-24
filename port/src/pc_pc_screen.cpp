/*
 * pc_pc_screen.cpp: OrrePort's rows on the game's PC screen; first, "Main Menu" (2026-11-15; plan and as built:
 * docs/port/pc-menu-anywhere.md, "Main Menu on the PC screen"; the look: docs/design/mockups/pc-menu-main-menu.png).
 *
 * Why it exists: snatcherLeo approved a way back to the title's main menu from the PC screen (2026-11-15): a fifth row "Main
 * Menu" between Save and Exit, on every PC screen (PC MENU's and a PC walked up to), which asks "Return to the main
 * menu? Unsaved progress will be lost." (NO first, B = NO) and on YES goes to the title's main menu with the slot
 * re-read from disk, so CONTINUE goes on from the last save and nothing unsaved leaks. OrreBank is to come later as
 * one more row (snatcherLeo may put it at the top), so the rows are data (ui/game/pc_menu.ini), not code.
 *
 * How the PC screen is built (pc-menu-anywhere.md, "the plan"; read from main.dol's tables): its list (menu 0x1D) is a
 * chain of item records: four button pieces (0x350-0x353), the lit piece (0x354) and four text labels (0x355-0x358,
 * the selectable ones; their chain order is the choice number). The game's code fixes each row's meaning by number (the
 * label colour tests, the per-row slide callbacks, the driver's switch), and two tables have exactly four entries (the
 * lit piece's places, the wheel's target angles). So:
 *   - OrrePort's rows are records served past the item table's end (OrreHook_MenuExtra), linked AFTER the game's own:
 *     Main Menu's label after Exit's, so the game's choices keep 0-3 and Main Menu is choice 4 while it shows above
 *     Exit; its button piece after the game's four (so the lit piece still draws over it), its wheel piece after the
 *     Pokémon icon (window 0x23, so it opens, fades and closes with it).
 *   - The game's own records, and this screen's PC data copies (the slide tables' resting x, the lit piece's places,
 *     the wheel's targets and base angles), are laid out for the screen order each time the screen opens
 *     (OrreHook_PcMenuOpen), from copies taken the first time, or put back exactly when the rows are off.
 *   - Small hooks answer for what the four-entry tables can't: the cursor along the screen order, wrapping round
 *     (OrreHook_PcMenuCursor), the lit piece's place and the wheel's target for choice 4 (OrreHook_PcMenuRowPlace,
 *     OrreHook_PcMenuWheel), the choice itself (OrreHook_PcMenuChoice).
 *   - The new row's pieces move with the game's: its button and word mirror Exit's row's live slide (read only, so no
 *     row is integrated twice), its wheel piece is placed on the shared angle and never steps it (the four game icons
 *     step it; their step is scaled so each row still takes as long to turn to: 72 degrees in the time of 90), and
 *     every icon is drawn spread out by N / 4 from the front slot, so neighbours sit the game's 90 degrees apart.
 *   - The list wraps round, tapped or held (snatcherLeo, 2026-11-15), and the turn's slots are re-centred on the chosen row
 *     at every move, so a wrap turns one slot the short way (slot_of, below).
 *
 * Back to the main menu (YES; the plan's section 5, "pop, then change"): the PC screen is left exactly as Exit leaves
 * it, the field's fade-in held black (OrreHook_FadeHold); PC MENU's pause menu closes the game's way
 * (OrreHook_FieldMenuReopen); once the field has settled at the bottom of the room stack with the pause menu closed,
 * the room manager changes room to the top menu 0x3A1 (OrreHook_RoomLeave; the game code resets the live return points
 * and the play-time clock first); at the main menu (OrreHook_TopMenuOpen, chained before LOAD's) the card looks at the
 * slot's folder again and the title's own read reloads the save image whole, the glove goes to Continue. A stall
 * check gives the screen back if the change hasn't happened 5 s after YES; a failed read falls back to the title
 * screen (the game's Quit path), never leaving the unsaved image in memory with CONTINUE offered.
 *
 * Pictures: the wheel piece is the icon (drawn by tools/local/pc_menu_pictures.py, no disc data) and the language's
 * label (traced by the same tool from the discs' wheel sheets into OrrePort's own outlines), shipped PNGs at 4x, drawn
 * down to the game's size into OrrePort's texture 'ORMM' when the screen opens (again when the language changes).
 * Japanese uses the English label (the Japanese disc's own wheel words are English).
 *
 * The switch: built in, off in GameCube-accurate mode and in automated runs (recorded presses were made on the
 * four-row screen: Down x 3 from POKéMON Storage would land on Main Menu instead of Exit); ORRE_PC_MAIN_MENU=1 turns it
 * on in a test run. Random numbers: none drawn (menu records, text, the YES / NO box, a card read, a room change;
 * likely, to be confirmed by the determinism test). Stores nothing in the save: no GameCube-copy rule.
 *
 * Test log: ORRE_SAVE_IMAGE_LOG=1 prints the save image's SHA-1 at every main-menu open, so a fresh boot's and a
 * re-read's can be compared (the re-read's is always printed).
 *
 * Platforms: plain C++ over the game's own code and SDL's file and picture calls: Windows, Linux and macOS the same
 * (likely; built and tested on Windows). For XD: XD's PC screen has its own records and room (likely different ids and
 * places); the approach (records past the table, chained after the game's own so its choice numbers stay, the PC data
 * copies laid out per screen order, a pop-then-change room path, the title's own card read) carries over (likely).
 */
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_lowmem.h"
#include "pc_options.h"
#include "pc_saves.h"
#include "pc_text.h"

extern "C" {
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/orre_hooks.h"
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
int pc_platform_automated_run(void);
const char* pc_game_text_language(void);
u32 VIGetRetraceCount(void);
void pc_fast_fades(int fadeOuts);           /* pc_field_menu.cpp: the PC exit's short fades disarmed */
int pc_card_remount_current(void);          /* pc_card.c */
void pc_saves_sha1_hex(const void* data, size_t len, char hex[41]); /* pc_saves.c */
}
#include "pc_game_ui.h"
#include "pc_paths.h" /* the UI folder (4.1) */

namespace {

void put16(u8* p, int off, u16 v) { p[off] = (u8)(v >> 8), p[off + 1] = (u8)v; }
void put32(u8* p, int off, u32 v) { p[off] = (u8)(v >> 24), p[off + 1] = (u8)(v >> 16), p[off + 2] = (u8)(v >> 8), p[off + 3] = (u8)v; }
u32 fn32(const void* f) { return (u32)(uintptr_t)f; } /* below 4 GB: the exe's image base (pc/ptr32.h) */

/* ---- the game's records and tables (main.dol, decoded 2026-11-15: pc-menu-anywhere.md section 1) ---- */
constexpr int kGameRows = 4;
constexpr int kButton0 = 0x350; /* 0x350-0x353: the button pieces, one per choice */
constexpr int kLit = 0x354;     /* the lit piece (the chain goes 0x353 -> 0x354) */
constexpr int kLabel0 = 0x355;  /* 0x355-0x358: the words; 0x358 ends the chain */
constexpr int kIconPokemon = 0x35D; /* window 0x23's only item (the Pokémon icon) */
constexpr u32 kSpritePokemonIcon = 0x9F;
constexpr int kExitChoice = 3;
constexpr u32 kGroupOR = 0x4F52; /* 'OR': OrrePort's resource group */
constexpr u32 kTopMenu = 0x3A1;
constexpr int kStallFrames = 300; /* 5 s at 60 a second: the black screen is never held longer waiting for the change */
const char* kGameRowIds[kGameRows] = {"pokemon_storage", "item_storage", "save", "exit"};
constexpr int kMaxExtra = 2;      /* Main Menu, and OrreBank later */
constexpr int kPicW = 150, kPicH = 170, kTexW = 152, kTexH = 172, kUp = 4;

/* ---- the data (ui/game/pc_menu.ini), with the values built in as the fallback ---- */
struct Layout {
    int firstX = 42, firstY = 154, stepY = 55, stepX = 10, labelDx = 16, labelDy = 7, labelW = 200;
    int cx = 544, cy = 215, radius = 130, picDx = -12, picDy = -10; /* picDy -6 until 2026-11-16 (pc_menu.ini says why) */
    int ynX = 440, ynY = 150;
};
struct Row {
    std::string id, word, question, icon, label;
    int choice = 4;
    u32 itemButton = 0x7070, itemLabel = 0x7071, itemIcon = 0x7072, sprite = 0x7070, texture = 0x4F524D4D /* 'ORMM' */;
    u32 keyWord = 0xFFF00031, keyQuestion = 0xFFF00032;
    std::string picLang; /* the language its picture was made for ("" = not made) */
    bool picOk = false;
};
Layout s_lay;
std::vector<std::string> s_order; /* ids, top to bottom */
std::vector<Row> s_rows;          /* OrrePort's rows, choices 4, 5 in this order */
bool s_dataRead = false;

/* the live arrangement (made at each open) */
int s_count = kGameRows;                  /* rows on screen */
int s_choiceAt[kGameRows + kMaxExtra];    /* screen position -> choice */
int s_posOf[kGameRows + kMaxExtra];       /* choice -> screen position */
bool s_applied = false;

/* the game's own, copied the first time */
bool s_saved = false;
u8 s_origItems[10][0x1C]; /* 0x350-0x358, 0x35D */
float s_origSlideB[24], s_origSlideL[24], s_origLit[8], s_origTarget[4], s_origBase[4];
u8* s_items = nullptr;   /* OrrePort's records, 3 per row (below 4 GB: the game reads them as its own) */
u8* s_sprites = nullptr; /* one per row */
void* (*s_prevMenuExtra)(int, u32) = nullptr;
void (*s_prevTopMenuOpen)(void) = nullptr;
/* the card-message hook registered before this file's (LOAD's, pc_title_loadsave.cpp: the "loaded" message left out
 * after a pick in SAVE FILES, 2026-11-16), chained so both apply. All platforms. */
s32 (*s_prevCardMessageSkip)(s32, s32) = nullptr;

bool enabled() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_PC_MAIN_MENU");
    return force != nullptr && force[0] == '1';
}

u8* item(int id) { return static_cast<u8*>(pc_game_ui.menu_item(id)); }
int orig_index(int id) { return id == kIconPokemon ? 9 : id - kButton0; }

/* ---- reading the data file ---- */
std::string ui_path(const std::string& rel) {
    return pc_paths_str(PC_PLACE_UI, rel.c_str()); /* app\UI\ (flat layout: ui\), 4.1 */
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

u32 id4(const std::string& v) { /* "ORMM" -> 'ORMM', or a number */
    if (v.size() == 4 && !(v[0] >= '0' && v[0] <= '9')) return (u32)(u8)v[0] << 24 | (u32)(u8)v[1] << 16 | (u32)(u8)v[2] << 8 | (u8)v[3];
    return (u32)std::strtoul(v.c_str(), nullptr, 0);
}

void read_data() {
    s_dataRead = true;
    s_order = {"pokemon_storage", "item_storage", "save", "main_menu", "exit"};
    s_rows.clear();
    Row mm;
    mm.id = "main_menu", mm.word = "game.pc_menu.main_menu", mm.question = "game.pc_menu.main_menu_question";
    mm.icon = "game/pc_menu/main_menu_icon.png", mm.label = "traced/pc_menu/main_menu_label_{lang}.png";
    const std::string path = ui_path("game/pc_menu.ini");
    size_t len = 0;
    char* text = static_cast<char*>(SDL_LoadFile(path.c_str(), &len));
    if (text == nullptr) {
        std::printf("[pc_screen] %s can't be read (%s): the built-in layout and the Main Menu row\n", path.c_str(), SDL_GetError());
        s_rows.push_back(mm);
        return;
    }
    std::string section;
    std::vector<Row> rows;
    std::vector<std::string> order;
    Row* cur = nullptr;
    const std::string all(text, len);
    SDL_free(text);
    size_t at = 0;
    while (at < all.size()) {
        size_t nl = all.find('\n', at);
        if (nl == std::string::npos) nl = all.size();
        const std::string line = trim(all.substr(at, nl - at));
        at = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            section = trim(line.substr(1, line.find(']') - 1));
            cur = nullptr;
            if (section.compare(0, 4, "row ") == 0) {
                rows.push_back(Row{});
                cur = &rows.back();
                cur->id = trim(section.substr(4));
            }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        /* through 64 bits: strtol's long is 32-bit on Windows and saturates 0xFFF00031 to 0x7FFFFFFF (the first test run
         * showed the label and question as message 0x7FFFFFFF); u32 ids and signed offsets both come out right */
        const int n = (int)(uint32_t)std::strtoll(v.c_str(), nullptr, 0);
        if (section == "list") {
            if (k == "first_x") s_lay.firstX = n; else if (k == "first_y") s_lay.firstY = n;
            else if (k == "step_y") s_lay.stepY = n; else if (k == "step_x") s_lay.stepX = n;
            else if (k == "label_dx") s_lay.labelDx = n; else if (k == "label_dy") s_lay.labelDy = n;
            else if (k == "label_width") s_lay.labelW = n;
        } else if (section == "wheel") {
            if (k == "centre_x") s_lay.cx = n; else if (k == "centre_y") s_lay.cy = n;
            else if (k == "radius") s_lay.radius = n; else if (k == "picture_dx") s_lay.picDx = n;
            else if (k == "picture_dy") s_lay.picDy = n;
        } else if (section == "question") {
            if (k == "yes_no_x") s_lay.ynX = n; else if (k == "yes_no_y") s_lay.ynY = n;
        } else if (section == "order" && k == "rows") {
            size_t p = 0;
            while (p <= v.size()) {
                size_t c = v.find(',', p);
                if (c == std::string::npos) c = v.size();
                const std::string id = trim(v.substr(p, c - p));
                if (!id.empty()) order.push_back(id);
                p = c + 1;
            }
        } else if (cur != nullptr) {
            if (k == "choice") cur->choice = n; else if (k == "word") cur->word = v;
            else if (k == "question") cur->question = v; else if (k == "icon") cur->icon = v;
            else if (k == "label") cur->label = v; else if (k == "item_button") cur->itemButton = (u32)n;
            else if (k == "item_label") cur->itemLabel = (u32)n; else if (k == "item_icon") cur->itemIcon = (u32)n;
            else if (k == "sprite") cur->sprite = (u32)n; else if (k == "texture") cur->texture = id4(v);
            else if (k == "key_word") cur->keyWord = (u32)n; else if (k == "key_question") cur->keyQuestion = (u32)n;
        }
    }
    /* checks: the game's four rows all listed once, OrrePort's rows known and at most two, each id once */
    std::vector<Row> used;
    std::vector<std::string> ok;
    int game = 0;
    for (const std::string& id : order) {
        bool dup = false;
        for (const std::string& o : ok) dup |= o == id;
        if (dup) continue;
        int g = -1;
        for (int i = 0; i < kGameRows; ++i)
            if (id == kGameRowIds[i]) g = i;
        if (g >= 0) { ok.push_back(id), ++game; continue; }
        const Row* r = nullptr;
        for (const Row& x : rows)
            if (x.id == id) r = &x;
        if (r == nullptr || (int)used.size() >= kMaxExtra) {
            std::printf("[pc_screen] pc_menu.ini: row \"%s\" %s: left out\n", id.c_str(), r == nullptr ? "has no [row] section" : "is past the two OrrePort rows the code has room for");
            continue;
        }
        used.push_back(*r);
        ok.push_back(id);
    }
    if (game != kGameRows) {
        std::printf("[pc_screen] pc_menu.ini: the order must list the game's four rows: the built-in order is used\n");
        s_order = {"pokemon_storage", "item_storage", "save", "main_menu", "exit"};
        s_rows.push_back(mm);
        return;
    }
    for (size_t i = 0; i < used.size(); ++i) {
        if (used[i].choice != 4 + (int)i)
            std::printf("[pc_screen] pc_menu.ini: row \"%s\" says choice %d; it is choice %d (OrrePort's rows are numbered from 4 in their [row] order)\n",
                        used[i].id.c_str(), used[i].choice, 4 + (int)i);
        used[i].choice = 4 + (int)i;
    }
    s_order = ok;
    s_rows = used;
    std::printf("[pc_screen] pc_menu.ini read: %d rows (%d of OrrePort's)\n", (int)s_order.size(), (int)s_rows.size());
}

/* ---- the wheel piece's picture: icon + the language's label, drawn down from 4x into the row's texture ---- */
bool load_rgba(const std::string& rel, std::vector<u8>& px, int& w, int& h) {
    const std::string path = ui_path(rel);
    SDL_Surface* s = SDL_LoadSurface(path.c_str()); /* SDL's own PNG reader (all platforms) */
    if (s == nullptr) return false;
    SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (c == nullptr) return false;
    w = c->w, h = c->h;
    px.resize((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) std::memcpy(&px[(size_t)y * w * 4], static_cast<const u8*>(c->pixels) + (size_t)y * c->pitch, (size_t)w * 4);
    SDL_DestroySurface(c);
    return true;
}

void write_rgba8_tiles(u8* dst, const std::vector<u8>& rgba, int w, int h) { /* GX RGBA8: 4 x 4 tiles, AR then GB */
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

std::string label_for(const Row& r, const std::string& lang) {
    std::string rel = r.label;
    const size_t at = rel.find("{lang}");
    if (at == std::string::npos) return rel;
    std::string want = rel;
    want.replace(at, 6, lang);
    SDL_PathInfo info;
    if (SDL_GetPathInfo(ui_path(want).c_str(), &info)) return want;
    rel.replace(at, 6, "en"); /* Japanese (on purpose), pseudo and any language without its own label: English */
    return rel;
}

bool make_picture(Row& r, const std::string& lang) {
    const PcGameUi& g = pc_game_ui;
    std::vector<u8> icon, lab;
    int iw = 0, ih = 0, lw = 0, lh = 0;
    const std::string labelRel = label_for(r, lang);
    if (!load_rgba(r.icon, icon, iw, ih) || iw != kPicW * kUp || ih != kPicH * kUp) {
        std::printf("[pc_screen] %s: the icon %s can't be read or isn't %d x %d: no wheel piece\n", r.id.c_str(), r.icon.c_str(), kPicW * kUp, kPicH * kUp);
        return false;
    }
    if (!load_rgba(labelRel, lab, lw, lh) || lw != iw || lh != ih) {
        std::printf("[pc_screen] %s: the label %s can't be read or isn't the icon's size: the icon alone\n", r.id.c_str(), labelRel.c_str());
        lab.assign(icon.size(), 0);
    }
    /* label over icon ("over" with straight alpha), then a 4 x 4 box average with premultiplied colour */
    std::vector<u8> out((size_t)kTexW * kTexH * 4, 0);
    for (int y = 0; y < kPicH; ++y)
        for (int x = 0; x < kPicW; ++x) {
            float acc[4] = {0, 0, 0, 0};
            for (int sy = 0; sy < kUp; ++sy)
                for (int sx = 0; sx < kUp; ++sx) {
                    const size_t o = ((size_t)(y * kUp + sy) * iw + x * kUp + sx) * 4;
                    const float la = lab[o + 3] / 255.f, ia = icon[o + 3] / 255.f;
                    const float a = la + ia * (1 - la);
                    for (int c = 0; c < 3; ++c) acc[c] += lab[o + c] * la + icon[o + c] * ia * (1 - la); /* premultiplied */
                    acc[3] += a;
                }
            u8* p = &out[((size_t)y * kTexW + x) * 4];
            const float a = acc[3] / (kUp * kUp);
            for (int c = 0; c < 3; ++c) p[c] = acc[3] > 0 ? (u8)SDL_min(255.f, acc[c] / acc[3] + 0.5f) : 0;
            p[3] = (u8)(a * 255.f + 0.5f);
        }
    void* t = g.resource_find(r.texture);
    const bool fresh = t == nullptr;
    if (fresh && (t = g.texture_create(kTexW, kTexH, 0x45 /* RGBA8 */)) == nullptr) {
        std::printf("[pc_screen] %s: no room for the wheel piece's texture\n", r.id.c_str());
        return false;
    }
    write_rgba8_tiles(static_cast<u8*>(g.texture_lock(t)), out, kTexW, kTexH);
    g.texture_unlock(t); /* a new version: the renderer reads it again */
    if (fresh) g.resource_register(t, kGroupOR, r.texture);
    r.picLang = lang;
    std::printf("[pc_screen] %s: wheel piece made (%s + %s)\n", r.id.c_str(), r.icon.c_str(), labelRel.c_str());
    return true;
}

/* ---- the arrangement ---- */
float X(int pos) { return (float)(s_lay.firstX + pos * s_lay.stepX); }
float Y(int pos) { return (float)(s_lay.firstY + pos * s_lay.stepY); }
constexpr float kTwoPi = 6.28318548f;
float wrap(float a) {
    while (a < 0) a += kTwoPi;
    while (a >= kTwoPi) a -= kTwoPi;
    return a;
}
/* *Added 2026-11-15 (the list wraps round, snatcherLeo: "Similarly, for how we figured out the pause menu ... we can do the same
 * in this PC Menu"):* the turn's slots are the screen positions rotated by s_slotShift, re-chosen at every cursor move so
 * the newly chosen row's slot is the middle one (2 pi / N x (N / 2)). The turn's angle is kept in [0, 2 pi) and moves
 * toward its target the direct way, so with the target always mid-circle a step (a wrap from Exit to POKéMON Storage
 * too) is one slot the short way, never a spin back through every icon. Shifting every target, base and the current
 * angle by the same amount changes nothing on screen */
int s_slotShift = 0;
int slot_of(int choice) { return (s_posOf[choice] + s_slotShift) % s_count; }
float target_of(int choice) { return (float)slot_of(choice) * kTwoPi / (float)s_count; }
float base_of(int choice) { return wrap(target_of(choice) - 4.71238898f /* 3 pi / 2: the front slot, at the left */); }
/* *Changed 2026-11-15 (snatcherLeo, on the first pictures: "They are too bunched together currently ... we use that same
 * spacing the original game had"):* the turn keeps one slot per row (2 pi / N apart, so the shared angle never wraps
 * between targets), but every icon is DRAWN spread out from the front slot by N / 4: neighbours 90 degrees away as in
 * the game, icons two rows away at the right edge, off the screen (the game's own opposite slot) */
float spread() { return (float)s_count / (float)kGameRows; }
/* the drawn angle for an icon at turn angle a (the game code does the same for its four, pdaUpdateOrbitSprite) */
float drawn(float a) {
    float rel = a - 4.71238898f;
    if (rel > 3.14159274f) rel -= kTwoPi;
    if (rel <= -3.14159274f) rel += kTwoPi;
    return 4.71238898f + rel * spread();
}

void put_back() {
    if (!s_saved || !s_applied) return;
    const PcGameUi& g = pc_game_ui;
    for (int i = 0; i < 10; ++i) std::memcpy(item(i == 9 ? kIconPokemon : kButton0 + i), s_origItems[i], 0x1C);
    std::memcpy(g.pc_slide_buttons, s_origSlideB, sizeof(s_origSlideB));
    std::memcpy(g.pc_slide_labels, s_origSlideL, sizeof(s_origSlideL));
    std::memcpy(g.pc_lit_places, s_origLit, sizeof(s_origLit));
    std::memcpy(g.pc_wheel_targets, s_origTarget, sizeof(s_origTarget));
    std::memcpy(g.pc_wheel_bases, s_origBase, sizeof(s_origBase));
    s_applied = false;
    std::printf("[pc_screen] the PC screen is the game's own again\n");
}

void row_button(void* window, u8* node, int k);
void row_label(void* window, u8* node, int k);
void row_icon(void* window, u8* node, int k);
void button0(void* w, u8* n) { row_button(w, n, 0); }
void button1(void* w, u8* n) { row_button(w, n, 1); }
void label0(void* w, u8* n) { row_label(w, n, 0); }
void label1(void* w, u8* n) { row_label(w, n, 1); }
void icon0(void* w, u8* n) { row_icon(w, n, 0); }
void icon1(void* w, u8* n) { row_icon(w, n, 1); }
void (*const kButtonCb[kMaxExtra])(void*, u8*) = {button0, button1};
void (*const kLabelCb[kMaxExtra])(void*, u8*) = {label0, label1};
void (*const kIconCb[kMaxExtra])(void*, u8*) = {icon0, icon1};

bool apply() {
    const PcGameUi& g = pc_game_ui;
    if (g.pc_slide_buttons == nullptr || g.yes_no == nullptr) return false;
    if (!s_saved) {
        for (int i = 0; i < 10; ++i) {
            const u8* r = item(i == 9 ? kIconPokemon : kButton0 + i);
            if (r == nullptr) return false;
            std::memcpy(s_origItems[i], r, 0x1C);
        }
        std::memcpy(s_origSlideB, g.pc_slide_buttons, sizeof(s_origSlideB));
        std::memcpy(s_origSlideL, g.pc_slide_labels, sizeof(s_origSlideL));
        std::memcpy(s_origLit, g.pc_lit_places, sizeof(s_origLit));
        std::memcpy(s_origTarget, g.pc_wheel_targets, sizeof(s_origTarget));
        std::memcpy(s_origBase, g.pc_wheel_bases, sizeof(s_origBase));
        s_saved = true;
    }
    if (!s_dataRead) read_data();
    if (s_items == nullptr) {
        s_items = static_cast<u8*>(pc_lowmem_alloc(0x1C * 3 * kMaxExtra));
        s_sprites = static_cast<u8*>(pc_lowmem_alloc(0x18 * kMaxExtra));
        if (s_items == nullptr || s_sprites == nullptr) return false;
    }
    /* the pictures first: a row whose picture can't be made is left out (never a row without its wheel piece) */
    const std::string lang = pc_game_text_language();
    std::vector<int> live; /* indices into s_rows shown this time */
    for (size_t i = 0; i < s_rows.size(); ++i) {
        Row& r = s_rows[i];
        if (r.picLang != lang || g.resource_find(r.texture) == nullptr) r.picOk = make_picture(r, lang);
        if (r.picOk) live.push_back((int)i);
    }
    /* the list numbers its choices in chain order: OrrePort's live rows are 4, 5 in their [row] order */
    for (size_t k = 0; k < live.size(); ++k) s_rows[live[k]].choice = kGameRows + (int)k;
    /* the screen order */
    s_count = 0;
    for (const std::string& id : s_order) {
        int c = -1;
        for (int i = 0; i < kGameRows; ++i)
            if (id == kGameRowIds[i]) c = i;
        for (int i : live)
            if (id == s_rows[i].id) c = s_rows[i].choice;
        if (c < 0) continue;
        s_choiceAt[s_count] = c, s_posOf[c] = s_count;
        ++s_count;
    }
    /* the game's own back first (idempotent), then laid out */
    for (int i = 0; i < 10; ++i) std::memcpy(item(i == 9 ? kIconPokemon : kButton0 + i), s_origItems[i], 0x1C);
    std::memcpy(g.pc_slide_buttons, s_origSlideB, sizeof(s_origSlideB));
    std::memcpy(g.pc_slide_labels, s_origSlideL, sizeof(s_origSlideL));
    std::memcpy(g.pc_lit_places, s_origLit, sizeof(s_origLit));
    std::memcpy(g.pc_wheel_targets, s_origTarget, sizeof(s_origTarget));
    std::memcpy(g.pc_wheel_bases, s_origBase, sizeof(s_origBase));
    /* the screen opens on POKéMON Storage (the driver's reset): its slot is the middle one (slot_of, above) */
    s_slotShift = ((s_count / 2 - s_posOf[0]) % s_count + s_count) % s_count;
    for (int c = 0; c < kGameRows; ++c) {
        const int p = s_posOf[c];
        put16(item(kButton0 + c), 2, (u16)(s16)X(p)), put16(item(kButton0 + c), 4, (u16)(s16)Y(p));
        put16(item(kLabel0 + c), 2, (u16)(s16)(X(p) + s_lay.labelDx)), put16(item(kLabel0 + c), 4, (u16)(s16)(Y(p) + s_lay.labelDy));
        g.pc_slide_buttons[c * 6 + 3] = X(p);
        g.pc_slide_labels[c * 6 + 3] = X(p) + (float)s_lay.labelDx;
        g.pc_lit_places[c * 2] = X(p), g.pc_lit_places[c * 2 + 1] = Y(p);
        g.pc_wheel_targets[c] = target_of(c);
        g.pc_wheel_bases[c] = base_of(c);
    }
    /* OrrePort's records, chained after the game's own */
    const u8* oButton = s_origItems[3]; /* 0x353 */
    const u8* oLabel = s_origItems[8];  /* 0x358 */
    const u8* oIcon = s_origItems[9];   /* 0x35D */
    u8* prevButton = item(kButton0 + 3);
    u8* prevLabel = item(kLabel0 + 3);
    u8* prevIcon = item(kIconPokemon);
    for (size_t k = 0; k < live.size(); ++k) {
        Row& r = s_rows[live[k]];
        const int p = s_posOf[r.choice];
        u8* b = s_items + (k * 3 + 0) * 0x1C;
        u8* l = s_items + (k * 3 + 1) * 0x1C;
        u8* ic = s_items + (k * 3 + 2) * 0x1C;
        std::memcpy(b, oButton, 0x1C);
        put16(b, 2, (u16)(s16)X(p)), put16(b, 4, (u16)(s16)Y(p)), put32(b, 0x14, fn32((void*)kButtonCb[k])), put16(b, 0x18, (u16)kLit);
        put16(prevButton, 0x18, (u16)r.itemButton), prevButton = b;
        std::memcpy(l, oLabel, 0x1C); /* selectable, the chain's end */
        put16(l, 2, (u16)(s16)(X(p) + s_lay.labelDx)), put16(l, 4, (u16)(s16)(Y(p) + s_lay.labelDy)), put16(l, 6, (u16)s_lay.labelW);
        put32(l, 0x10, r.keyWord), put32(l, 0x14, fn32((void*)kLabelCb[k])), put16(l, 0x18, 0);
        prevLabel[0] = (u8)(prevLabel[0] & ~0x40), put16(prevLabel, 0x18, (u16)r.itemLabel), prevLabel = l;
        std::memcpy(ic, oIcon, 0x1C); /* the chain's end */
        put16(ic, 6, (u16)kPicW), put16(ic, 8, (u16)kPicH), put16(ic, 0x0A, (u16)r.sprite);
        put32(ic, 0x14, fn32((void*)kIconCb[k])), put16(ic, 0x18, 0);
        prevIcon[0] = (u8)(prevIcon[0] & ~0x40), put16(prevIcon, 0x18, (u16)r.itemIcon), prevIcon = ic;
        /* the sprite: a copy of the Pokémon icon's (its type kept), the whole picture of OrrePort's texture */
        u8* s = s_sprites + k * 0x18;
        std::memcpy(s, g.menu_sprite(kSpritePokemonIcon), 0x18);
        put16(s, 0x08, 0), put16(s, 0x0A, 0), put16(s, 0x0C, (u16)kPicW), put16(s, 0x0E, (u16)kPicH), put32(s, 0x10, r.texture);
        /* the words, in the game's language: the button's in font 1 (the approved mockup), the question in font 0 (the
         * game's message window) */
        pc_game_text_set_own(r.keyWord, pc_text_lang(lang.c_str(), r.word.c_str()), 0xF0000001u);
        pc_game_text_set_own(r.keyQuestion, pc_text_lang(lang.c_str(), r.question.c_str()), 0xF0000000u);
    }
    if (!s_applied)
        std::printf("[pc_screen] the PC screen with %d rows (%s): %s\n", s_count, lang.c_str(), live.empty() ? "only the game's own" : "OrrePort's added");
    s_applied = !live.empty();
    if (!s_applied) put_back();
    return s_applied;
}

/* which of OrrePort's live rows a choice is (the k-th), or -1 */
const Row* row_of_choice(int choice) {
    for (const Row& r : s_rows)
        if (r.choice == choice && r.picOk) return &r;
    return nullptr;
}
const Row* live_row(int k) {
    int n = 0;
    for (const Row& r : s_rows)
        if (r.picOk && n++ == k) return &r;
    return nullptr;
}

/* ---- the items' callbacks (the game calls them once a frame per item, with the window and the item's draw node:
 * +0x50 / +0x52 x and y, +0x64-+0x66 the colour, +0x67 the alpha; the window's cursor row at +0x95) ---- */
float exit_slide() { return pc_game_ui.pc_slide_live[kExitChoice * 6 + 4]; } /* Exit's row: its offset (-300 .. 0) */

void row_button(void* /*window*/, u8* node, int k) {
    const Row* r = live_row(k);
    if (r == nullptr) return;
    const float v = exit_slide(); /* the game's own row integrates it; this row only follows */
    const float rest = X(s_posOf[r->choice]);
    if (v < 0.0f) {
        node[0x67] = (u8)((v < -255.0f ? -255.0f : v) + 255.0f);
        *(s16*)(node + 0x50) = (s16)(rest + v);
    } else {
        node[0x67] = 0xFF;
        *(s16*)(node + 0x50) = (s16)rest;
    }
}

void row_label(void* window, u8* node, int k) {
    const Row* r = live_row(k);
    if (r == nullptr) return;
    const float v = exit_slide();
    node[0x67] = (u8)((v < -255.0f ? -255.0f : v) + 255.0f);
    *(s16*)(node + 0x50) = (s16)(X(s_posOf[r->choice]) + (float)s_lay.labelDx + v);
    const bool on = (s8)static_cast<u8*>(window)[0x95] == r->choice; /* the game's white / blue, as its own labels */
    node[0x64] = on ? 0xFF : 0x46, node[0x65] = on ? 0xFF : 0x8F, node[0x66] = on ? 0xFF : 0xB4;
}

void row_icon(void* /*window*/, u8* node, int k) {
    const Row* r = live_row(k);
    if (r == nullptr) return;
    /* placed on the shared angle like the game's icons (pdaUpdateOrbitSprite), never stepping it */
    const float a = drawn(wrap(*pc_game_ui.pc_wheel_angle - base_of(r->choice)));
    *(s16*)(node + 0x50) = (s16)((float)s_lay.radius * std::sin(a) + (float)s_lay.cx + (float)s_lay.picDx);
    *(s16*)(node + 0x52) = (s16)((float)s_lay.radius * std::cos(a) + (float)s_lay.cy + (float)s_lay.picDy);
}

/* ---- the hooks ---- */
void* menu_extra(int kind, u32 index) {
    if (s_items != nullptr) {
        int k = 0;
        for (const Row& r : s_rows) {
            if (!r.picOk) continue;
            if (kind == 0 && index == r.itemButton) return s_items + (k * 3 + 0) * 0x1C;
            if (kind == 0 && index == r.itemLabel) return s_items + (k * 3 + 1) * 0x1C;
            if (kind == 0 && index == r.itemIcon) return s_items + (k * 3 + 2) * 0x1C;
            if (kind == 1 && index == r.sprite) return s_sprites + k * 0x18;
            ++k;
        }
    }
    return s_prevMenuExtra != nullptr ? s_prevMenuExtra(kind, index) : nullptr;
}

void pc_menu_open() {
    if (enabled()) {
        if (!apply()) std::printf("[pc_screen] OrrePort's rows couldn't be added: the PC screen is the game's own\n");
    } else {
        put_back();
    }
}

/* *Changed 2026-11-15 (snatcherLeo: "Similarly, for how we figured out the pause menu, where you can keep holding down on
 * control stick or dpad to have the menu loop endlessly (or opposite direction), we can do the same in this PC Menu ...
 * just for this main screen"):* the list wraps round (the game clamps): Up on the top row goes to the bottom one, Down
 * on the bottom to the top, tapped or held (the key record's repeat word: the press, then every 5 frames after 15). The
 * ends are the data file's first and last rows, so a sixth row later wraps with no change here. The window plays the
 * move sound itself whenever the row changes. Then the turn's slots are re-centred on the new row (slot_of) */
s32 pc_menu_cursor(s8* row, u16 keys) {
    if (!s_applied) return 0;
    const int was = (u8)*row < (u8)(kGameRows + kMaxExtra) ? s_posOf[(u8)*row] : 0;
    int pos = was;
    if (keys & MENU_KEY_DOWN) pos = (pos + 1) % s_count;
    if (keys & MENU_KEY_UP) pos = (pos + s_count - 1) % s_count;
    *row = (s8)s_choiceAt[pos];
    if (pos != was) {
        const int shift = ((s_count / 2 - pos) % s_count + s_count) % s_count;
        const int delta = shift - s_slotShift;
        s_slotShift = shift;
        const PcGameUi& g = pc_game_ui;
        *g.pc_wheel_angle = wrap(*g.pc_wheel_angle + (float)delta * kTwoPi / (float)s_count); /* nothing moves on screen */
        *g.pc_wheel_target = target_of(s_choiceAt[pos]); /* at once: no icon steps toward the old target this frame */
        for (int c = 0; c < kGameRows; ++c) g.pc_wheel_targets[c] = target_of(c), g.pc_wheel_bases[c] = base_of(c);
    }
    return 1;
}

void pc_menu_row_place(s32 row, f32* x, f32* y) {
    if (!s_applied || row_of_choice(row) == nullptr) return;
    *x = X(s_posOf[row]), *y = Y(s_posOf[row]);
}

void pc_menu_wheel(s32 choice, f32* target, f32* stepScale, f32* angleScale) {
    if (!s_applied) return;
    *stepScale = (float)kGameRows / (float)s_count; /* each row still takes as long to turn to as with four */
    *angleScale = spread();                          /* and is drawn the game's 90 degrees from its neighbours */
    if ((u8)choice >= kGameRows && row_of_choice(choice) != nullptr) *target = target_of(choice);
}

/* ---- Main Menu: the question and the way back ---- */
enum Leave { kNone, kArmed, kIssued };
Leave s_leave = kNone;
u32 s_yesAt = 0;

s32 pc_menu_choice(s32 choice) {
    const Row* r = s_applied ? row_of_choice(choice) : nullptr;
    if (r == nullptr) return 0;
    if (r->id != "main_menu") return 0; /* a row whose action isn't built yet: the list again */
    const PcGameUi& g = pc_game_ui;
    std::printf("[pc_screen] Main Menu: asking\n");
    g.msg_window_open(r->keyQuestion);
    const int answer = g.yes_no(1 /* NO first */, (s16)s_lay.ynX, (s16)s_lay.ynY);
    g.msg_window_close();
    if (answer != 0) {
        std::printf("[pc_screen] Main Menu: %s, back to the list\n", answer == 1 ? "NO" : "B");
        return 0;
    }
    s_leave = kArmed;
    s_yesAt = VIGetRetraceCount();
    std::printf("[pc_screen] Main Menu: YES, leaving the PC for the main menu (the screen stays black)\n");
    return 1;
}

/* Black from the PC's close until the main menu (snatcherLeo, 2026-11-16: the field "shows for a moment" after YES).
 * - kArmed: the field's fade back in (type 2) stays black (as built 2026-11-15).
 * - kIssued: *added 2026-11-16* the room change's own fade to black (type 3, 0.5 s) also stays black. The cause of the
 *   flash, found in the test log and fade.c: a new fade-out starts from fully clear (fadeSet resets its elapsed time, and
 *   fadeDaemon draws alpha = elapsed / duration), so the held-black field reappeared and darkened again over 30 frames.
 *   Held, it is set as a 0 s fade-out, which draws fully black at once. The top menu's own fade in (type 2) is the
 *   game's: that is the main menu appearing. Platforms: all (game code and its fade). */
s32 fade_hold(u16 type) {
    if (s_leave == kArmed) return type == 2 ? 1 : 0;
    if (s_leave == kIssued) return type == 3 ? 1 : 0;
    return 0;
}
s32 field_menu_reopen() { return s_leave == kArmed ? 0 : 1; }

/* *Added 2026-11-16 (the project's helper; snatcherLeo: after YES the field music "gets slightly louder for a moment" before the main
 * menu's music; "Ideally, no swelling of any kind"):* the closing pause menu brings back only the effects' volume; the
 * music stays at the level PC MENU's pause menu ducked it to (85 of 127). The cause, measured per frame (ORRE_DUCK_LOG,
 * docs/engine/music.md "Ducking"): the pause menu's own unduck (500 ms, 85 -> 127) ran about 70 frames after YES,
 * behind the black screen, and the title's song replaced the field music about 50 frames later (+5 dB in the mix);
 * the PC screen's own unduck only pops back to the pause menu's 85. The title's song then starts on a fresh slot at
 * full volume (the field music's slot is released, which clears its duck). If the room change never comes (the stall
 * check), room_leave asks for the music back (-2). Platforms: all (game code and MusyX). */
bool s_musicHeld = false;
s32 field_menu_unduck_music() {
    if (s_leave != kArmed) return 1;
    s_musicHeld = true;
    std::printf("[pc_screen] Main Menu: the pause menu closes with the music left ducked (no swell before the title's song)\n");
    return 0;
}

/* Test log (the project's helper, 2026-11-16, the PC MENU spot check; ORRE_SPOT_LOG=1): where Wes stands after a field room
 * settles, every frame for the first 30 frames, then every 30 frames up to 20 s, so a restored spot can be compared
 * with the saved one and anything that moves him afterwards (the room's start, an NPC push, a script) shows. Read only;
 * all platforms */
u32 s_spotLogRoom = 0, s_spotLogFrames = 0;
void spot_log(u32 room, s32 depth, s32 settled) {
    static int on = -1;
    if (on < 0) {
        const char* v = SDL_getenv("ORRE_SPOT_LOG");
        on = v != nullptr && v[0] == '1';
    }
    if (!on || pc_game_ui.hero_spot == nullptr) return;
    if (depth != 0 || !settled) { s_spotLogRoom = 0; return; }
    if (room != s_spotLogRoom) s_spotLogRoom = room, s_spotLogFrames = 0;
    const u32 n = s_spotLogFrames++;
    if (n >= 1200 || (n >= 30 && n % 30 != 0)) return;
    u32 r = 0;
    float p[3];
    s32 facing = 0;
    /* and the field camera's state (field_camera.c, fn_801171C8): enabled, its point count, "blended once", and the
     * smoothed height / distance / yaw it hands the camera every frame (docs/engine/field-camera.md) */
    float cam[3] = {0, 0, 0};
    const s32 st = pc_game_ui.field_camera_state != nullptr ? pc_game_ui.field_camera_state(cam) : -1;
    if (pc_game_ui.hero_spot(&r, p, &facing))
        std::printf("[spot_log] room 0x%X settled +%u frames: Wes at (%.3f, %.3f, %.3f), facing %d; field camera on %d, "
                    "points %d, blended %d, height %.2f distance %.2f yaw %.4f\n", (unsigned)r, (unsigned)n, p[0], p[1], p[2],
                    (int)facing, (int)(st & 0xFF), (int)((st >> 8) & 0xFF), (int)((st >> 16) & 0xFF), cam[0], cam[1], cam[2]);
}

s32 room_leave(u32 room, s32 depth, s32 settled, s32 pauseOpen) {
    spot_log(room, depth, settled);
    if (s_leave != kArmed) return 0;
    if (VIGetRetraceCount() - s_yesAt > (u32)kStallFrames) {
        s_leave = kNone;
        std::printf("[pc_screen] Main Menu: the room change didn't come within 5 s (room 0x%X, depth %d, settled %d, pause menu %d): "
                    "the screen fades back in, nothing changed\n", (unsigned)room, (int)depth, (int)settled, (int)pauseOpen);
        const bool held = s_musicHeld;
        s_musicHeld = false;
        return held ? -2 : -1; /* -2: the music, left ducked by the pause menu, comes back up too */
    }
    if (depth != 0 || !settled || pauseOpen) return 0;
    s_musicHeld = false; /* the title's song replaces the held music (its slot's release clears the duck) */
    s_leave = kIssued;
    pc_fast_fades(0); /* the PC exit's short fades end here: the title's own fades are the game's */
    std::printf("[pc_screen] Main Menu: room 0x%X settled after %u frames: to the main menu (0x%X)\n", (unsigned)room,
                (unsigned)(VIGetRetraceCount() - s_yesAt), (unsigned)kTopMenu);
    return (s32)kTopMenu;
}

void log_save_image(const char* why) {
    if (pc_game_ui.save_image == nullptr) return;
    u32 size = 0;
    const u8* img = pc_game_ui.save_image(&size);
    char hex[41] = {0};
    if (img != nullptr) pc_saves_sha1_hex(img, size, hex);
    std::printf("[pc_screen] save image (%s): slot %d, sha1 %s\n", why, pc_saves_ready() ? pc_saves_current() : 0, hex);
}

/* *Added 2026-11-16 (snatcherLeo: "We don't need 'Your save file has been loaded!'. Not needed. Instead, it should just skip
 * straight to loading the main menu"):* set only around the re-read below, so the card task's "loaded" message (error
 * 12, message 0x44E5) is left out on this path alone; every other load (boot, SAVE FILES, LOAD) still shows it, and a
 * failed read still shows its own message. Platforms: all. */
bool s_quietRead = false;
s32 card_message_skip(s32 error, s32 taskKind) {
    if (!s_quietRead || error != 12) return s_prevCardMessageSkip != nullptr ? s_prevCardMessageSkip(error, taskKind) : 0;
    std::printf("[pc_screen] Main Menu: the re-read's \"loaded\" message left out\n");
    return 1;
}

void top_menu_open() {
    if (s_leave == kIssued) {
        s_leave = kNone;
        const PcGameUi& g = pc_game_ui;
        int got = -1;
        s_quietRead = true;
        for (int attempt = 0; attempt < 2 && got < 0; ++attempt) {
            pc_card_remount_current(); /* the slot's folder as it is on disk now */
            got = g.card_load_title();
        }
        s_quietRead = false;
        if (got < 0) {
            /* never the unsaved image with CONTINUE offered: the title screen, whose Press Start reads the card again */
            std::printf("[pc_screen] Main Menu: the slot couldn't be read: to the title screen\n");
            g.title_restart();
        } else {
            g.top_menu_glove_default();
            std::printf("[pc_screen] Main Menu: back at the main menu, the slot read again (%s)\n", got ? "a save: CONTINUE" : "no save: NEW GAME");
            log_save_image("re-read after Main Menu");
        }
    } else {
        const char* v = SDL_getenv("ORRE_SAVE_IMAGE_LOG");
        if (v != nullptr && v[0] == '1') log_save_image("main menu");
    }
    if (s_prevTopMenuOpen != nullptr) s_prevTopMenuOpen();
}

} // namespace

/* Back to the title's main menu from the field, the same way as Main Menu's YES (2026-11-20, the softlock fix's hang
 * watch, pc_save_windows.cpp): once the field room is settled with no pause menu open, room_leave changes room to the
 * top menu, and top_menu_open re-reads the slot from disk (the hang watch has just put the previous version back). The
 * stall check applies as for YES. Call on the game's thread. Platforms: all. */
extern "C" void pc_pc_screen_leave_for_main_menu(void) {
    s_leave = kArmed;
    s_yesAt = VIGetRetraceCount();
    std::printf("[pc_screen] back to the main menu from the field (asked by the hang watch)\n");
}

/* called once at start-up (pc_platform.c), after pc_field_menu_start: it chains the item hook and the main menu's open
 * hook (LOAD SAVE's) */
extern "C" void pc_pc_screen_start(void) {
    s_prevMenuExtra = OrreHook_MenuExtra;
    OrreHook_MenuExtra = menu_extra;
    s_prevTopMenuOpen = OrreHook_TopMenuOpen;
    OrreHook_TopMenuOpen = top_menu_open;
    OrreHook_PcMenuOpen = pc_menu_open;
    OrreHook_PcMenuChoice = pc_menu_choice;
    OrreHook_PcMenuCursor = pc_menu_cursor;
    OrreHook_PcMenuRowPlace = pc_menu_row_place;
    OrreHook_PcMenuWheel = pc_menu_wheel;
    OrreHook_FadeHold = fade_hold;
    OrreHook_RoomLeave = room_leave;
    OrreHook_FieldMenuReopen = field_menu_reopen;
    OrreHook_FieldMenuUnduckMusic = field_menu_unduck_music;
    s_prevCardMessageSkip = OrreHook_CardMessageSkip;
    OrreHook_CardMessageSkip = card_message_skip;
}
