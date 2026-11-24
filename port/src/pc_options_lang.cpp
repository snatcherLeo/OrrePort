/*
 * pc_options_lang.cpp: the Language row on the game's own Options screen (2.2 step 4, 2026-11-13; mockup 78;
 * docs/port/translation.md "The game's own text in other languages").
 *
 * What it does: the title's Options screen gets four rows, Language, Rumble, Sound and Done (snatcherLeo: "We can put
 * Language on top then, and Sound below Rumble ... It's officially apart of the game"), the cursor starting on
 * Language. Left / right on Language switches the game's text at once (pc_game_text.cpp). The choice is saved to
 * OrrePort's settings ("language"), never to the memory card (the project's rules, the project plan "The details, decided by the project").
 *
 * How (the screen's tables: docs/engine/new-game-flow.md, "How the Options screen is built"):
 *   - Layout: the game's rows are moved through the item table's own setter, so the screen keeps drawing itself.
 *     Rows 58 apart instead of 76: Language y 104, Rumble 162, Sound 220, Done 278 (clear of the help box at 349).
 *   - The Language row: items OrrePort owns, past the item table's end (OrreHook_MenuExtra, ids 0x7000+), copied from
 *     the game's own row items (panel, highlight, stripes, label), linked onto the ends of the screen's item chains. Its
 *     value is a game text item showing OrrePort's own message (bank 0xFFF, pc_game_text.cpp), circled with the
 *     game's own circle sprite (0x192), between two arrow characters.
 *   - The label is a picture OrrePort draws (the game has no "Language" label; the European game's is the model, and
 *     nothing from that disc is used). snatcherLeo, 2026-11-13: "the UI you make has to look like it's part of the game" and
 *     "To look more lower resolution, later in OrrePort Studio UI Editor, users can take this out": so it is made at the
 *     game's own scale (drawn 4x larger, then shrunk), in the game labels' colours (a pale lavender fill, a thick navy
 *     outline, a soft dark glow), in the shipped Archivo at its heaviest (a rounded font would be closer; adding one is a
 *     download for snatcherLeo to approve). Its text comes from ui/lang ("game.options.language"), so it is translatable.
 *     *Updated 2026-11-15 (language piece 5):* in German, French, Italian and Spanish, with the player's European disc
 *     added, the label is the European game's own picture from that disc (disc_label: Sprache, Langue, LINGUA, Idioma,
 *     in the game's own face); English and Japanese keep the drawn one. The label is made again at the next Options
 *     visit after a language change (the texture's label slot has a fixed size, so it is rewritten in place).
 *   - The game's row numbers stay (0 Sound, 1 Rumble, 2 Done); Language is 3. The screen's "A on Done leaves" loop and
 *     its callbacks work unchanged; OrreHook_OptionsKeys only changes the up / down order and handles Language.
 *   - The label's texture is a game texture (GStextureCreate, RGBA8) registered under its own id, so the game's own
 *     sprite code draws it. It is made at the first visit to Options and kept for the run: freeing it as the screen
 *     closes would leave the closing animation drawing a freed texture. Cost: one slot of the game's texture pool and
 *     about 37 KB of its heap after the first visit (208 x 44 RGBA8) (likely harmless; the game's pool and heap are not near full on the
 *     title screen, to be measured).
 *
 * Accurate mode: shown there too (snatcherLeo: "Yes, show it"); it draws no random numbers (likely: the title and Options
 * screens draw none, measured 2026-11-13; the determinism test checks this row).
 *
 * Platforms: all the same (game code and FreeType). For XD: XD's Options screen is a different menu; the approach (move
 * the game's own items, add OrrePort's past the table's end, a texture through the game's own sprite path) carries
 * over (likely), the item numbers do not.
 */
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "pc_lowmem.h"
#include "pc_options.h" /* pc_option_gamecube_accurate */
#include "pc_settings.h"
#include "pc_text.h"
#include "pc_word_pictures.h" /* the European disc's Options sheet (the label, piece 5) */

extern "C" {
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
extern void* (*OrreHook_MenuExtra)(int kind, u32 index);
extern void (*OrreHook_OptionsScreen)(int event);
extern int (*OrreHook_OptionsCancelKeeps)(void);
extern int (*OrreHook_OptionsKeys)(s32* selection, u16 keys);

/* OrrePort */
unsigned char* pc_font_render_line(const char* utf8, float size, float wdth, float wght, int* w, int* h, int* baseline);
const char* pc_game_text_language(void);
int pc_game_text_has(const char* code);
void pc_game_text_set_language(const char* code);
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
void pc_names_options_screen(int open); /* pc_names.cpp: the screen's value fits in accurate mode too (2026-11-21) */
}
#include "pc_game_ui.h" /* the game's functions and data, through the table the game side fills */

extern "C" struct PcGameUi pc_game_ui = {};

namespace {

/* the game's item and sprite tables keep the GameCube's big-endian layout on the PC */
void put16(u8* p, int off, u16 v) { p[off] = (u8)(v >> 8), p[off + 1] = (u8)v; }
void put32(u8* p, int off, u32 v) { p[off] = (u8)(v >> 24), p[off + 1] = (u8)(v >> 16), p[off + 2] = (u8)(v >> 8), p[off + 3] = (u8)v; }
s16 get16(const u8* p, int off) { return (s16)((p[off] << 8) | p[off + 1]); }

constexpr int kRowLanguage = 3; /* the game's rows: 0 Sound, 1 Rumble, 2 Done */
constexpr s16 kRowY[4] = {220, 162, 278, 104}; /* by row number: Sound, Rumble, Done, Language */
constexpr s16 kOldRowY[3] = {116, 192, 267};
/* the game's items of each row (new-game-flow.md): panel, highlight, stripes, label, choices, circles, pictures */
const std::vector<int> kRowItems[3] = {
    {0x50E, 0x511, 0x514, 0x519, 0x520, 0x521, 0x524, 0x525, 0x51B, 0x51C, 0x51D}, /* Sound */
    {0x50F, 0x512, 0x515, 0x518, 0x51E, 0x51F, 0x522, 0x523, 0x51A},               /* Rumble */
    {0x510, 0x513, 0x516, 0x517},                                                    /* Done */
};
constexpr int kItemBase = 0x7000, kItemCount = 7; /* OrrePort's items: past the table's 0x12D2 */
constexpr u32 kSpriteLabel = 0x7000;              /* OrrePort's sprite: past the sprite table's end */
constexpr u32 kTexLabel = 0x4F524C31;             /* 'ORL1': the label texture's resource id */
constexpr u32 kResGroup = 0x4F52;                 /* 'OR': OrrePort's resource group */
constexpr u32 kKeyHelp = 0xFFF00001, kKeyValue = 0xFFF00002;
/* the value's circle: the game's choice column (190) moved 22 pixels right, with every row's choices (apply_layout), so
 * the wider "Language" label fits before the left arrow (snatcherLeo, 2026-11-13: the first, wider label's look was the right
 * one) */
constexpr int kCircleX = 212, kCircleY = 104 + 6, kValueShift = kCircleX - 190;

u8* s_items = nullptr;  /* kItemCount item records (0x1C bytes each), below 4 GB */
u8* s_sprite = nullptr; /* one sprite record (0x18 bytes) */
std::vector<std::pair<int, s16>> s_origY; /* the game's items' original y, saved once (the moves are idempotent) */
std::vector<s16> s_origX;                 /* and their original x, in the same order */
bool s_ready = false;
std::string s_before; /* the language when the screen opened (B restores it) */
bool s_cancelled = false;

/* the languages the row offers: those with the game's text (the project's rule). English; the pseudo test language only in test
 * runs (ORRE_LANG=pseudo or ORRE_TEST_LANGUAGES=1), never for players. The rule for the region discs (snatcherLeo,
 * 2026-11-13: "only show those options for the other langauges IF the ROM from that other region is poitned to it"):
 * German, French, Italian and Spanish join only when the player has added a European disc, Japanese only with a
 * Japanese disc (pc_region_text.cpp reads them; 2026-11-15, the European ones). Order: English, then the European
 * disc's in the order the European game lists them (English, Deutsch, Français, Español, Italiano: likely, from its
 * language row's text; guess until seen in the European game) */
std::vector<std::string> languages() {
    std::vector<std::string> out = {"en"};
    for (const char* code : {"de", "fr", "es", "it", "ja"}) /* Japanese from the Japanese disc (piece 3) */
        if (pc_game_text_has(code)) out.push_back(code);
    const char* lang = SDL_getenv("ORRE_LANG");
    const char* test = SDL_getenv("ORRE_TEST_LANGUAGES");
    if ((lang != nullptr && std::string(lang) == "pseudo") || (test != nullptr && test[0] == '1')) out.push_back("pseudo");
    return out;
}

/* the language's own name in capitals, as the game writes its choices (STEREO, ON) */
std::string display_name(const std::string& code) {
    /* a language from a language disc: its own name, from ui/lang (game.options.lang.<code>; already in capitals, since
     * the capitals step below only knows a-z and these names have Ç and Ñ) */
    if (code == "de" || code == "fr" || code == "it" || code == "es" || code == "ja")
        return pc_text(("game.options.lang." + code).c_str());
    std::string name = code == "pseudo" ? "Pseudo" : code;
    for (const auto& [c, n] : pc_text_languages()) {
        if (c == code) name = n;
    }
    for (char& ch : name) {
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    }
    return name;
}

/* a premultiplied RGBA picture drawn K times larger, shrunk to the game's scale by averaging K x K blocks (this is what
 * gives OrrePort's pictures the game's own softness) */
std::vector<float> shrink(const std::vector<float>& big, int W, int H, int K, int& outW, int& outH) {
    outW = (W + K - 1) / K, outH = (H + K - 1) / K;
    std::vector<float> out((size_t)outW * outH * 4, 0.f);
    for (int y = 0; y < outH; ++y)
        for (int x = 0; x < outW; ++x)
            for (int j = 0; j < K; ++j)
                for (int i = 0; i < K; ++i) {
                    const int bx = x * K + i, by = y * K + j;
                    if (bx >= W || by >= H) continue;
                    for (int c = 0; c < 4; ++c) out[((size_t)y * outW + x) * 4 + c] += big[((size_t)by * W + bx) * 4 + c] / (K * K);
                }
    return out;
}

/* a grey picture grown (max) or shrunk (min) by a disc of radius r: the outline, and the corner rounding */
std::vector<float> morph(const std::vector<float>& src, int W, int H, int r, bool grow) {
    std::vector<std::pair<int, int>> disc;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx * dx + dy * dy <= r * r) disc.emplace_back(dx, dy);
    std::vector<float> out(src.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float m = grow ? 0.f : 1.f;
            for (const auto& [dx, dy] : disc) {
                const int sx = x + dx, sy = y + dy;
                const float v = (sx < 0 || sy < 0 || sx >= W || sy >= H) ? 0.f : src[(size_t)sy * W + sx];
                m = grow ? std::max(m, v) : std::min(m, v);
            }
            out[(size_t)y * W + x] = m;
        }
    return out;
}

/* The label picture, in the game labels' style, at the game's own scale (see the file comment). Colours and outline
 * from the game's own labels (fill 201-205 / 205 / 255, outline 9 / 5 / 56, a soft dark shadow; the European game's
 * "Language" label, studied for its look only). Archivo isn't rounded like the game's face, so the letters' corners
 * are rounded (shrunk, then grown back by a small disc). Returns premultiplied RGBA floats at the game's scale. */
std::vector<float> draw_label(const char* text, int& outW, int& outH) {
    constexpr int K = 4;                    /* drawn 4x larger, then shrunk to the game's scale */
    /* *Tuned 2026-11-13 with snatcherLeo:* the first picture (30 px, width 112, weight 900) had the right look but ran under the
     * left arrow; a narrower 25 px at width 100 "looked off" (snatcherLeo), its corner rounding closing the "e". So: the first
     * one's heavy, wide face at 27 px, lighter rounding (1 game pixel), and the row's value column moved right
     * (kLangCircleX) so the label fits ("This was closer ... go for that more") */
    constexpr float kSize = 27.0f * K;
    constexpr int kRound = K, kOutline = 3 * K + K / 2, kGlow = 2 * K, kPad = kOutline + kGlow * 2 + K;
    int w = 0, h = 0, base = 0;
    unsigned char* cov = pc_font_render_line(text, kSize, 112.0f, 900.0f, &w, &h, &base);
    if (cov == nullptr) return {};
    const int W = w + kPad * 2, H = h + kPad * 2;
    std::vector<float> letters((size_t)W * H, 0.f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) letters[(size_t)(y + kPad) * W + x + kPad] = cov[(size_t)y * w + x] / 255.f;
    std::free(cov);
    letters = morph(morph(letters, W, H, kRound, false), W, H, kRound, true); /* rounded corners */
    auto at = [&](int x, int y) -> float { return letters[(size_t)y * W + x]; };
    /* the outline: the letters grown by a disc */
    std::vector<float> outline = morph(letters, W, H, kOutline, true), glow;
    /* the glow: the outline blurred (two box passes each way), darker below */
    glow = outline;
    std::vector<float> tmp(glow.size());
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                float s = 0;
                for (int d = -kGlow; d <= kGlow; ++d) s += glow[(size_t)y * W + std::clamp(x + d, 0, W - 1)];
                tmp[(size_t)y * W + x] = s / (2 * kGlow + 1);
            }
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                float s = 0;
                for (int d = -kGlow; d <= kGlow; ++d) s += tmp[(size_t)std::clamp(y + d, 0, H - 1) * W + x];
                glow[(size_t)y * W + x] = s / (2 * kGlow + 1);
            }
    }
    /* compose at K x (premultiplied): the shadow (black, 60 %, a little down and right as the game's), the outline
     * (navy), the fill (pale lavender) */
    const float fill[3] = {203 / 255.f, 205 / 255.f, 255 / 255.f}, line[3] = {9 / 255.f, 5 / 255.f, 56 / 255.f};
    constexpr int kShadow = K; /* 1 game pixel */
    std::vector<float> big((size_t)W * H * 4, 0.f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float* o = &big[((size_t)y * W + x) * 4];
            auto over = [&](const float* rgb, float a) {
                for (int c = 0; c < 3; ++c) o[c] = rgb[c] * a + o[c] * (1 - a);
                o[3] = a + o[3] * (1 - a);
            };
            const float black[3] = {0, 0, 0};
            const int gx = std::max(0, x - kShadow), gy = std::max(0, y - kShadow);
            over(black, std::min(1.f, glow[(size_t)gy * W + gx] * 0.6f));
            over(line, outline[(size_t)y * W + x]);
            over(fill, at(x, y));
        }
    return shrink(big, W, H, K, outW, outH);
}

/* The European game's own "Language" label, from the player's European disc (language piece 5, 2026-11-15;
 * docs/port/sequences-per-language.md): its Options sheet (topmenu menu_031, 640 x 256 RGBA8) per language carries the
 * label in the game's own face (Sprache, Langue, LINGUA, Idioma), which the US sheet lacks. The European program draws
 * it without a sprite record in its table (none of its menu_031 records covers it, checked 2026-11-15), so the place is
 * measured here: the box of the label's solid ink (alpha > 60) on each language's sheet. At run time the sheet must
 * show exactly that box (the solid ink's bounds inside it, margin included), or the drawn label is used. The cut keeps
 * only the label's ink and its soft glow (pixels within 3 of the solid ink): the boxes padded by 3 would also catch the
 * oval under "Langue" and the blue band under "Sprache". English has no sheet from this path (the European disc's
 * English sheet, with "Language", is not one of the word pictures), so English and Japanese keep the drawn label.
 * Returns premultiplied RGBA floats at the game's scale (the sheet's labels draw 1:1), or empty. */
std::vector<float> disc_label(const std::string& lang, int& outW, int& outH) {
    struct Box {
        const char* lang;
        int x0, y0, x1, y1; /* inclusive, the solid ink */
    };
    static const Box kBoxes[] = {
        {"fr", 413, 3, 538, 43}, {"de", 131, 47, 273, 87}, {"it", 433, 6, 566, 42}, {"es", 146, 48, 259, 84},
    };
    const Box* b = nullptr;
    for (const Box& k : kBoxes)
        if (lang == k.lang) b = &k;
    if (b == nullptr) return {};
    size_t size = 0;
    uint8_t* file = pc_word_pictures_read(lang.c_str(), "menu_031", &size);
    if (file == nullptr) return {};
    const std::vector<u8> d(file, file + size);
    std::free(file);
    auto be16 = [&](size_t o) { return o + 2 <= d.size() ? (int)((d[o] << 8) | d[o + 1]) : 0; };
    auto be32 = [&](size_t o) { return o + 4 <= d.size() ? (u32)((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) : 0u; };
    const int texW = be16(0), texH = be16(2);
    const size_t off = be32(0x28) != 0 ? be32(0x28) : 0x80;
    if (be32(8) != 0x45 || texW != 640 || texH != 256 || d.size() < off + (size_t)texW * texH * 4) {
        std::printf("[options_lang] %s: the Options sheet isn't as measured (%d x %d, format %02x): the drawn label\n",
                    lang.c_str(), texW, texH, be32(8));
        return {};
    }
    /* one texel of the RGBA8 tiles (4 x 4, alpha / red pairs then green / blue pairs) */
    auto texel = [&](int x, int y, u8 out[4]) {
        const size_t tile = off + ((size_t)(y / 4) * (texW / 4) + x / 4) * 64;
        const int i = (y % 4) * 4 + x % 4;
        out[3] = d[tile + i * 2], out[0] = d[tile + i * 2 + 1], out[1] = d[tile + 32 + i * 2], out[2] = d[tile + 32 + i * 2 + 1];
    };
    constexpr int kPad = 3, kSolid = 60;
    const int x0 = std::max(0, b->x0 - kPad), y0 = std::max(0, b->y0 - kPad);
    const int x1 = std::min(texW - 1, b->x1 + kPad), y1 = std::min(texH - 1, b->y1 + kPad);
    const int W = x1 - x0 + 1, H = y1 - y0 + 1;
    std::vector<u8> px((size_t)W * H * 4);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) texel(x0 + x, y0 + y, &px[((size_t)y * W + x) * 4]);
    /* the check: the solid ink inside the box spans the box exactly */
    int sx0 = W, sy0 = H, sx1 = -1, sy1 = -1;
    for (int y = b->y0 - y0; y <= b->y1 - y0; ++y)
        for (int x = b->x0 - x0; x <= b->x1 - x0; ++x)
            if (px[((size_t)y * W + x) * 4 + 3] > kSolid)
                sx0 = std::min(sx0, x), sy0 = std::min(sy0, y), sx1 = std::max(sx1, x), sy1 = std::max(sy1, y);
    if (sx0 != b->x0 - x0 || sy0 != b->y0 - y0 || sx1 != b->x1 - x0 || sy1 != b->y1 - y0) {
        std::printf("[options_lang] %s: the Language label isn't where measured: the drawn label\n", lang.c_str());
        return {};
    }
    /* keep the solid ink inside the box and what lies within kPad of it (the glow), nothing else */
    std::vector<float> out((size_t)W * H * 4, 0.f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            bool near = false;
            for (int dy = -kPad; dy <= kPad && !near; ++dy)
                for (int dx = -kPad; dx <= kPad && !near; ++dx) {
                    const int sx = x + dx, sy = y + dy;
                    if (sx < b->x0 - x0 || sx > b->x1 - x0 || sy < b->y0 - y0 || sy > b->y1 - y0) continue;
                    near = px[((size_t)sy * W + sx) * 4 + 3] > kSolid && dx * dx + dy * dy <= kPad * kPad;
                }
            if (!near) continue;
            const u8* p = &px[((size_t)y * W + x) * 4];
            const float a = p[3] / 255.f;
            float* o = &out[((size_t)y * W + x) * 4];
            o[0] = p[0] / 255.f * a, o[1] = p[1] / 255.f * a, o[2] = p[2] / 255.f * a, o[3] = a;
        }
    outW = W, outH = H;
    std::printf("[options_lang] %s: the European disc's own Language label (%d x %d)\n", lang.c_str(), W, H);
    return out;
}

/* The arrow, as the European game's (a solid cyan triangle, 83 / 255 / 239, about 21 x 23 game pixels): pointing
 * right, or left when `left`. Premultiplied RGBA floats at the game's scale. */
std::vector<float> draw_arrow(bool left, int& outW, int& outH) {
    constexpr int K = 4, aw = 15 * K, ah = 19 * K, pad = K;
    const int W = aw + pad * 2, H = ah + pad * 2;
    const float cyan[3] = {83 / 255.f, 255 / 255.f, 239 / 255.f};
    std::vector<float> big((size_t)W * H * 4, 0.f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            /* inside the triangle: the tip at the middle of one side, the base on the other */
            const float u = (float)(x - pad) / aw, v = (float)(y - pad) / ah;
            const float t = left ? 1.f - u : u;           /* 0 at the base, 1 at the tip */
            const float half = 0.5f * (1.f - t);          /* the half-height at that point */
            const bool in = t >= 0 && t <= 1 && std::fabs(v - 0.5f) <= half;
            if (!in) continue;
            float* o = &big[((size_t)y * W + x) * 4];
            for (int c = 0; c < 3; ++c) o[c] = cyan[c];
            o[3] = 1.f;
        }
    return shrink(big, W, H, K, outW, outH);
}

/* GX RGBA8: 4 x 4 tiles of 64 bytes, the 16 alpha / red pairs, then the 16 green / blue pairs */
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

/* the highlight bar's callback for the Language row: as the game's own (fn_8002092C and its two siblings), for row 3 */
void lang_highlight(void* /*window*/, u8* node) {
    u8* state = pc_game_ui.options_state;
    const float f = 0.1f * (float)std::sin(6.2831855f * *(float*)(state + 0x24)) + 1.1f;
    *(float*)(node + 0x6C) = f;
    *(float*)(node + 0x68) = f;
    if (*(s32*)state == kRowLanguage) node[4] |= 2;
    else node[4] &= (u8)~2;
}

/* the value's callback: the game's circle around the language in use, as menuOptionDrawSoundMode draws its own */
void lang_value_draw(void* window, u8* node) {
    pc_game_ui.draw_sprite((s16)(kCircleX - *(s16*)(node + 0x50)), (s16)(kCircleY - *(s16*)(node + 0x52)), window, 0x192);
}

u32 fn32(void (*f)(void*, u8*)) { return (u32)(uintptr_t)f; } /* below 4 GB: the exe's image base (pc/ptr32.h) */

void* menu_extra(int kind, u32 index) {
    if (kind == 0 && s_items != nullptr && index >= (u32)kItemBase && index < (u32)(kItemBase + kItemCount)) {
        return s_items + (size_t)(index - kItemBase) * 0x1C;
    }
    if (kind == 1 && s_sprite != nullptr && index >= kSpriteLabel && index < kSpriteLabel + 3) {
        return s_sprite + (size_t)(index - kSpriteLabel) * 0x18;
    }
    return nullptr;
}

/* the pictures side by side in one texture: the label, the left arrow, the right arrow; premultiplied floats to
 * straight RGBA bytes, the size rounded up to whole GX tiles (4 x 4) */
struct Atlas {
    std::vector<u8> rgba;
    int w = 0, h = 0;
    int x[3] = {0, 0, 0}, pw[3] = {0, 0, 0}, ph[3] = {0, 0, 0};
};
/* *2026-11-15 (piece 5):* the label's slot in the texture has a fixed size, so the texture keeps its size when the label
 * is made again for another language (the arrows sit after the slot) */
constexpr int kLabelSlotW = 256, kLabelSlotH = 48;
Atlas make_atlas(const std::string& lang) {
    Atlas a;
    std::vector<float> pics[3];
    /* in the game's language when the screen opens (made again at the next visit after a language change: build()).
     * The European disc's own label where it has one (piece 5), else OrrePort's drawn one */
    pics[0] = disc_label(lang, a.pw[0], a.ph[0]);
    if (pics[0].empty()) pics[0] = draw_label(pc_text_lang(lang.c_str(), "game.options.language"), a.pw[0], a.ph[0]);
    if (pics[0].empty() || a.pw[0] > kLabelSlotW || a.ph[0] > kLabelSlotH) {
        if (!pics[0].empty())
            std::printf("[options_lang] the %s label is %d x %d, more than its %d x %d slot: no Language row\n",
                        lang.c_str(), a.pw[0], a.ph[0], kLabelSlotW, kLabelSlotH);
        a.rgba.clear();
        return a;
    }
    pics[1] = draw_arrow(true, a.pw[1], a.ph[1]);
    pics[2] = draw_arrow(false, a.pw[2], a.ph[2]);
    a.x[0] = 0, a.x[1] = kLabelSlotW, a.x[2] = kLabelSlotW + a.pw[1];
    a.w = (a.x[2] + a.pw[2] + 3) & ~3, a.h = (std::max({kLabelSlotH, a.ph[1], a.ph[2]}) + 3) & ~3;
    a.rgba.assign((size_t)a.w * a.h * 4, 0);
    for (int i = 0; i < 3; ++i)
        for (int y = 0; y < a.ph[i]; ++y)
            for (int px = 0; px < a.pw[i]; ++px) {
                const float* s = &pics[i][((size_t)y * a.pw[i] + px) * 4];
                u8* o = &a.rgba[((size_t)y * a.w + a.x[i] + px) * 4];
                for (int c = 0; c < 3; ++c) o[c] = (u8)std::lround(s[3] > 0 ? std::clamp(s[c] / s[3], 0.f, 1.f) * 255 : 0);
                o[3] = (u8)std::lround(std::clamp(s[3], 0.f, 1.f) * 255);
            }
    return a;
}

/* the label's sprite size and its item's place, from the label picture's size (the first build, and each remake) */
void place_label(const Atlas& a) {
    u8* s = s_sprite;
    put16(s, 0x0C, (u16)a.pw[0]), put16(s, 0x0E, (u16)a.ph[0]);
    u8* it = s_items + 3 * 0x1C;
    const s16 rowY = kRowY[kRowLanguage];
    /* the label: centred on the game labels' column (x 93) and their height in the row (30 below the panel's top) */
    /* (a long label, as in other languages, keeps clear of the left arrow: its right edge stops 2 pixels before it) */
    const int arrowL = kCircleX - 3 - a.pw[1];
    const int labelX = std::min(93 - a.pw[0] / 2, arrowL - 2 - a.pw[0]);
    put16(it, 2, (u16)std::max(0, labelX)), put16(it, 4, (u16)(rowY + 30 - a.ph[0] / 2));
    put16(it, 6, (u16)a.pw[0]), put16(it, 8, (u16)a.ph[0]);
}

std::string s_labelLang; /* the language the label picture was last made in */

/* the texture and the sprite and item records, made once (see the file comment); the label picture made again when
 * the game's language changed since (piece 5, 2026-11-15: the texture rewritten in place, its size fixed) */
bool build() {
    const std::string lang = pc_game_text_language();
    if (s_ready) {
        if (lang == s_labelLang) return true;
        const Atlas a = make_atlas(lang);
        void* tex = pc_game_ui.resource_find(kTexLabel);
        if (a.rgba.empty() || tex == nullptr) return true; /* keep the last label */
        write_rgba8_tiles(static_cast<u8*>(pc_game_ui.texture_lock(tex)), a.rgba, a.w, a.h);
        pc_game_ui.texture_unlock(tex);
        place_label(a);
        s_labelLang = lang;
        std::printf("[options_lang] the Language label made again for %s (%d x %d)\n", lang.c_str(), a.pw[0], a.ph[0]);
        return true;
    }
    const Atlas a = make_atlas(lang);
    if (a.rgba.empty()) {
        std::printf("[options_lang] the label's font (Fonts/Archivo) wasn't found: no Language row\n");
        return false;
    }
    s_labelLang = lang;
    const PcGameUi& g = pc_game_ui;
    if (g.resource_find(kTexLabel) == nullptr) {
        void* tex = g.texture_create(a.w, a.h, 0x45 /* RGBA8 */);
        if (tex == nullptr) {
            std::printf("[options_lang] no room for the label's texture: no Language row\n");
            return false;
        }
        u8* texels = static_cast<u8*>(g.texture_lock(tex));
        write_rgba8_tiles(texels, a.rgba, a.w, a.h);
        g.texture_unlock(tex);
        g.resource_register(tex, kResGroup, kTexLabel);
    }
    s_items = static_cast<u8*>(pc_lowmem_alloc(0x1C * kItemCount));
    s_sprite = static_cast<u8*>(pc_lowmem_alloc(0x18 * 3));
    if (s_items == nullptr || s_sprite == nullptr) return false;
    /* the sprites: the game's "Sound" label sprite with OrrePort's texture and each picture's place in it */
    for (int i = 0; i < 3; ++i) {
        u8* s = s_sprite + i * 0x18;
        std::memcpy(s, g.menu_sprite(0x191), 0x18);
        s[5] = 0, s[6] = 0; /* no offset */
        put16(s, 0x08, (u16)a.x[i]), put16(s, 0x0A, 0), put16(s, 0x0C, (u16)a.pw[i]), put16(s, 0x0E, (u16)a.ph[i]);
        put32(s, 0x10, kTexLabel);
    }
    /* the items: copies of the game's own, then placed. Menu 0x80 (the rows) gets 0x7000-0x7005 after its last item
     * (panel, highlight, stripes, label, two arrows); menu 0x7B (the choices) gets 0x7006, the value */
    const int src[kItemCount] = {0x50F, 0x512, 0x515, 0x519, 0x519, 0x519, 0x521};
    for (int i = 0; i < kItemCount; ++i) std::memcpy(s_items + i * 0x1C, g.menu_item(src[i]), 0x1C);
    auto item = [&](int i) { return s_items + i * 0x1C; };
    auto picture = [&](int i, int sprite, int x, int y) {
        put16(item(i), 2, (u16)x), put16(item(i), 4, (u16)y);
        put16(item(i), 6, (u16)a.pw[sprite]), put16(item(i), 8, (u16)a.ph[sprite]);
        put16(item(i), 0x0A, (u16)(kSpriteLabel + sprite));
    };
    const s16 rowY = kRowY[kRowLanguage];
    put16(item(0), 4, (u16)rowY);       /* the panel */
    put16(item(1), 4, (u16)(rowY - 1)); /* the highlight bar, with OrrePort's callback for row 3 */
    put32(item(1), 0x14, fn32(lang_highlight));
    put16(item(2), 4, (u16)rowY); /* the stripes */
    picture(3, 0, 0, 0);
    place_label(a); /* the label's place: place_label */
    const int arrowL = kCircleX - 3 - a.pw[1];
    /* the arrows: either side of the value's circle (x 190-317), on the row's middle (as the European game's row) */
    picture(4, 1, arrowL, rowY + 27 - a.ph[1] / 2);
    picture(5, 2, kCircleX + 127 + 3, rowY + 27 - a.ph[2] / 2);
    /* the value: a text item like the game's STEREO / ON (14 below the panel's top), circled by its callback */
    put32(item(6), 0x10, kKeyValue), put16(item(6), 4, (u16)(rowY + 14)), put32(item(6), 0x14, fn32(lang_value_draw));
    put16(item(6), 2, (u16)(get16(item(6), 2) + kValueShift)); /* centred in the moved circle */
    /* the chains: each copy's "last" flag (0x40) off except the chains' new ends; next ids at +0x18 */
    for (int i = 0; i < kItemCount; ++i) {
        item(i)[0] &= (u8)~0x40;
        put16(item(i), 0x18, (u16)(kItemBase + i + 1));
    }
    item(5)[0] |= 0x40, put16(item(5), 0x18, 0);
    item(6)[0] |= 0x40, put16(item(6), 0x18, 0);
    s_ready = true;
    std::printf("[options_lang] the Language row is ready (label %d x %d, texture %d x %d)\n", a.pw[0], a.ph[0], a.w, a.h);
    return true;
}

/* the game's choice column's items (the words, their circles, the speaker and controller pictures): moved right with
 * the Language row's value, so ON, STEREO and the language line up in one column (snatcherLeo, 2026-11-13, on the first
 * aligned-left version: "the selection highlight has to be off-cenetered from the rest... hmmm") */
bool in_choice_column(int id) {
    switch (id) {
    case 0x520: case 0x521: case 0x524: case 0x525: case 0x51B: case 0x51C: case 0x51D: /* Sound */
    case 0x51E: case 0x51F: case 0x522: case 0x523: case 0x51A:                         /* Rumble */
        return true;
    default:
        return false;
    }
}

/* the game's own tables: the rows moved, the new items linked on (idempotent: every visit) */
void apply_layout() {
    const PcGameUi& g = pc_game_ui;
    if (s_origY.empty()) {
        /* the originals, saved once: y in .second, x kept alongside (s_origX) */
        for (int row = 0; row < 3; ++row)
            for (const int id : kRowItems[row]) {
                const u8* it = (const u8*)g.menu_item(id);
                s_origY.emplace_back(id, get16(it, 4));
                s_origX.push_back(get16(it, 2));
            }
    }
    size_t k = 0;
    for (int row = 0; row < 3; ++row)
        for (const int id : kRowItems[row]) {
            const s16 x = (s16)(s_origX[k] + (in_choice_column(id) ? kValueShift : 0));
            g.menu_item_set_xy(id, x, (s16)(s_origY[k].second + kRowY[row] - kOldRowY[row]));
            ++k;
        }
    for (const int last : {0x51A, 0x525}) {
        u8* it = (u8*)g.menu_item(last);
        it[0] &= (u8)~0x40;
        put16(it, 0x18, (u16)(last == 0x51A ? kItemBase : kItemBase + 6));
    }
    g.options_help[kRowLanguage] = kKeyHelp;
}

void set_texts() {
    /* the help line in the language the screen opened in, so it changes on the next visit, not while the player flips
     * through the languages (snatcherLeo, 2026-11-15: "if I pick Spanish, it would remain "Choose the langauge for the game's
     * text and menus." You leave that option menu by hitting B, but maybe you go back and want to change langauge, now
     * it'll say that message in Spanish") */
    const std::string helpLang = s_before.empty() ? pc_game_text_language() : s_before;
    pc_game_text_set_own(kKeyHelp, pc_text_lang(helpLang.c_str(), "game.options.language_help"), 0x43FB); /* the font of the game's help lines */
    pc_game_text_set_own(kKeyValue, display_name(pc_game_text_language()).c_str(), 0x1900); /* the font of STEREO */
}

/* B on the Options screen keeps every choice made there (Sound, Rumble, Language), as Done does. Why: snatcherLeo,
 * 2026-11-15, on the question whether B should cancel as on the GameCube: "If we had that "B keeps it", that would hold
 * true for all the other optoins ... The "B" button would keep whatever selection you made, which is what users may do
 * and not get confused. We could have it in our code as a switch, make no mention of it in the settings menu, and I'd
 * say, yes, B keeps it." So: settings.ini "options_b_keeps" (1 by default; 0 = the GameCube's way), shown nowhere. In
 * GameCube-accurate mode B cancels, as on the console (it changes what a button press does; accurate-mode list).
 * Platforms: all. */
bool b_keeps() {
    return !pc_option_gamecube_accurate() && pc_settings_int("options_b_keeps", 1) != 0;
}
int options_cancel_keeps() { return b_keeps() ? 1 : 0; }

void options_screen(int event) {
    if (!pc_game_ui.ready) return; /* a test program without the game */
    /* *2026-11-21 (snatcherLeo: the Options screen in an accurate save must look as in an Enhanced one, every language):* the
     * screen's own value fits (Rumble's ON / OFF, pc_names.cpp) apply in accurate mode too while it is open; opened at
     * event 0, left at event 2 (B's cancel, event 1, still leaves through 2) */
    if (event == 0) pc_names_options_screen(1);
    if (event == 2) pc_names_options_screen(0);
    if (event == 0) {
        s_before = pc_game_text_language();
        s_cancelled = false;
        if (!build()) return;
        apply_layout();
        set_texts();
        *(s32*)pc_game_ui.options_state = kRowLanguage; /* the cursor starts on Language (mockup 78) */
    } else if (event == 1) {
        if (b_keeps()) return; /* B keeps the pick: left like Done, so it is saved below (event 2) */
        s_cancelled = true;
        pc_game_text_set_language(s_before.c_str());
        set_texts();
    } else if (event == 2 && !s_cancelled && s_before != pc_game_text_language()) {
        /* saved only when the player picked another language (the project's rule: a hub-only language stays in the setting) */
        pc_settings_set_string("language", pc_game_text_language());
        pc_settings_save();
        std::printf("[options_lang] language %s saved\n", pc_game_text_language());
    }
}

int options_keys(s32* selection, u16 keys) {
    if (!s_ready) return 0;
    static const int order[4] = {kRowLanguage, 1, 0, 2}; /* Language, Rumble, Sound, Done */
    int pos = 0;
    while (pos < 4 && order[pos] != *selection) ++pos;
    if (pos == 4) pos = 0;
    if (keys & MENU_KEY_DOWN) {
        if (pos < 3) *selection = order[pos + 1];
        return 1;
    }
    if (keys & MENU_KEY_UP) {
        if (pos > 0) *selection = order[pos - 1];
        return 1;
    }
    if (*selection == kRowLanguage && (keys & (MENU_KEY_LEFT | MENU_KEY_RIGHT))) {
        const std::vector<std::string> langs = languages();
        int i = 0;
        while (i < (int)langs.size() && langs[i] != pc_game_text_language()) ++i;
        if (i == (int)langs.size()) i = 0;
        const int n = (int)langs.size();
        i = (keys & MENU_KEY_RIGHT) ? (i + 1) % n : (i + n - 1) % n;
        pc_game_text_set_language(langs[i].c_str());
        set_texts();
        return 1;
    }
    return 0;
}

} // namespace

/* called once at start-up (pc_platform.c, after pc_game_text_start) */
extern "C" void pc_names_add_fit_own(uint32_t key, float place, int resize, float dx, int baseline);
extern "C" void pc_options_lang_start(void) {
    /* the language's name fits its circle (snatcherLeo, 2026-11-15, on FRANÇAIS and ESPAÑOL touching the circle's ends: "we
     * want all languages in the "Language" option menu, to be resized properly, such that, they look good in that
     * picker"): a name wider than 96 of the circle's 127 px is drawn smaller both ways, centred on its line; ENGLISH
     * (about 93 px) keeps the game's size. The same fitting as the long names (pc_names.cpp).
     * *Changed 2026-11-21 (snatcherLeo, on DEUTSCH and FRANÇAIS spilling past the circle in Slot 16, an accurate save: "Yes,
     * keep it on"):* fitted in GameCube-accurate mode too (pc_names_add_fit_own): this row is OrrePort's own, a
     * GameCube has none, so fitting its value can't make the game differ from a console's. The game's own prints'
     * fits stay off in accurate mode */
    pc_names_add_fit_own(kKeyValue, 96.f, 1, 0.f, 0);
    OrreHook_MenuExtra = menu_extra;
    OrreHook_OptionsScreen = options_screen;
    OrreHook_OptionsCancelKeeps = options_cancel_keeps;
    OrreHook_OptionsKeys = options_keys;
}
