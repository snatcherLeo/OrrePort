/*
 * pc_font_render.cpp: OrrePort's font renderer, parts A and C (step 2.2, 2026-11-13; docs/port/font-renderer.md).
 *
 * What it does: through the hook OrreHook_GlyphFetch (src/game/gs_msg.c, orre_hooks.h) it draws the game's letters with
 * real fonts instead of the disc's bitmaps, into the same slot, at the same size: the glyph's own width is kept (it is
 * the pen's advance and the quad's width, so every line, box and menu keeps its layout), the letter sits on the game's
 * baseline, and the result is the same 4-bit greyscale the GameCube's texture holds. It changes how letters are formed,
 * not their size; part B (sharp text at the resolution chosen in Settings) builds on it.
 *
 * Part A, font 0 (the 22-px main font; snatcherLeo's lock, 2026-10-18, docs/engine/fonts.md "Locked"): Latin letters in MS
 * PGothic at 22 px (verified pixel by pixel), kana in BIZ UDGothic Regular; both +0.25 px bolder on each side
 * (FreeType's outline emboldening by 0.5 px in total); pen at the slot's left edge, as the approved mock-ups drew it.
 *
 * Part C, fonts 1-4 (18 and 13 px; snatcherLeo, 2026-11-13, option A, after the side-by-side and the full character sheets):
 * the open fonts the project's scoring found closest (fonts.md, "Open fonts scored against fonts 1-4"): Archivo for fonts 1-3,
 * Roboto for font 4, each at its scored width, weight and size (font 3 a touch bolder than its score: it looked lighter
 * than the original); their kana, CJK punctuation and full-width forms in BIZ UDGothic Bold, +0.25 px bolder on each
 * side (the game's small kana are bolder than the font's Bold at this size). Each letter placed in the game's slot with
 * the slot's spare room shared out by the letter's own side bearings; a letter wider than its slot is squeezed to fit
 * (at most 0.75x) and drawn heavier first, so its strokes stay as thick as its neighbours'. Tweaks read from the full
 * sheets: the small fonts draw '|' as a broken bar and '~' as a raised tilde, so those are drawn as U+00A6 / U+02DC; a
 * character the game leaves blank (font 4's '_') stays blank.
 *
 * The Pokedollar (0x0024 and the full-width 0xFF04), all fonts (snatcherLeo, 2026-11-13: "for pokedollars, we could draw our
 * own custom thing for that, for the font"): built from the font's own 'P', proportions read from the game's own ₽ in
 * fonts 0, 1 and 4: the bowl as drawn, two bars across the stem at about 0.64 and 0.86 of the P's height, each about
 * 0.14 of it thick, reaching 1.2 stem widths left of the stem and 1.4 right, the stem carried down to 1.12 of the
 * height. The other 11 special symbols (the speech bubble, the e-Reader marks...) keep the game's bitmaps.
 *
 * Fallback, per character: the kind's first font if it has the character, then the other one (BIZ UDGothic has the
 * arrows and the gender signs Archivo and Roboto lack), else the game's own picture. Not a setting (snatcherLeo, 2026-11-11:
 * one in-game font): on whenever the fonts are found. ORRE_ORIGINAL_LETTERS=1 turns it off for one run (the developer's
 * side-by-side check). The original bitmaps stay in the game: snatcherLeo's planned "Nostalgia" .orremod turns the renderer
 * off per font id to bring them back (decisions log, 2026-11-13).
 *
 * Platforms: the renderer is platform-neutral (FreeType, built with Aurora, with variable-font support and, since
 * 2026-11-16, its PNG reader for colour emoji). Where the fonts come from is not:
 *   - Archivo and Roboto (SIL Open Font License) are shipped next to the exe in Fonts/ (app\Fonts\ in the portable
 *     layout; port/tools/firstlink.py, make_portable.py), on every OS. *Moved 2026-11-16* from ui/fonts/ (the decision
 *     "Fonts in the portable layout").
 *   - Google's Noto set (SIL OFL), shipped in the same Fonts/ on every OS: part D's last resort (pc_font_files.cpp has
 *     the chain and why). *Replaced 2026-11-16* the player's own Windows fonts (Yu Gothic, Meiryo, Segoe UI...), so
 *     every OS draws the same fallback letters.
 *   - Windows only: MS PGothic (msgothic.ttc, face 2; never shipped, it is the player's own) and BIZ UDGothic Regular /
 *     Bold from the Windows font folder (the user's own folder too, for optional fonts installed per user): option A's
 *     recipe (ORRE_OPEN_FONTS=1) and part D's Latin letters for font 0. Part D's CJK letters no longer use them (Noto
 *     CJK on every OS, 2026-11-16).
 *   - Linux / Steam Deck and macOS (likely, not built): the same shipped files; MS PGothic is missing there, so font
 *     0's missing Latin letters come from Noto Sans instead (the lock's Arimo, "variant D", is still to ship).
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_MULTIPLE_MASTERS_H

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h> /* SHGetKnownFolderPath: where Windows keeps its fonts (Windows only; see find_font) */
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "dolphin/types.h"
extern int (*OrreHook_GlyphFetch)(u16 fontId, u16 code, s16 width, s16 cellHeight, s16 baseline, s16 capHeight,
                                  const u8** pixels, s16* height, s16* yOffset);
extern void* (*OrreHook_GlyphMissing)(u16 fontId, u16 code, s16 cellHeight, s16 baseline, s16 capHeight,
                                      void** outBank);
void pc_font_render_start(void);
/* part B's sharp atlas (pc_text_atlas.cpp): its scale (0 or 1 = off) and a ready picture for a 4-bit glyph picture */
int pc_text_atlas_scale(void);
void pc_text_atlas_provide(int width, int height, const u8* pixels4, const u8* hi, int scale);
const char* pc_game_text_language(void); /* pc_game_text.cpp: the game's language (the CJK region rule) */
}
#include "pc_font_files.h"  /* the shipped fonts' folder, the Noto chain, the CJK region rule, the stand-ins */
#include "pc_region_text.h" /* pc_region_font_glyph: the Japanese disc's own letters (part D asks first) */
#include "pc_shape.h"       /* shaped clusters' glyph records (draw_cluster; 2026-11-16) */

namespace {

FT_Library s_ft = nullptr;

/* How one game font is drawn. Sizes in pixels; weights and widths on the variable fonts' own axis scales. */
struct Recipe {
    FT_Face latin = nullptr;  /* Latin letters, digits, punctuation */
    float latinSize = 0;
    bool latinVar = false;    /* a variable font: set wdth / wght before each glyph */
    float wdth = 100, wght = 400;
    FT_Pos latinBold = 0;     /* outline emboldening, 26.6 units in total (32 = +0.25 px each side) */
    FT_Face kana = nullptr;   /* kana, CJK punctuation, full-width forms */
    float kanaSize = 0;       /* the font's cell height: the game's kana fill the cell (fixed-width) */
    FT_Pos kanaBold = 32;
    bool fitSlot = false;     /* part C: share the slot's room by the side bearings, squeeze if too wide */
    bool cjkWide = false;     /* full-width forms and CJK punctuation go to the kana font first (part C) */
    bool smallTweaks = false; /* '|' -> broken bar, '~' -> raised tilde (fonts 1-4 draw them so) */
    bool sized = false;       /* part C: latinSize already matched to the game's capital height (size_to_cap) */
    int hint = 0;             /* FreeType hinting: 0 normal, 1 light, 2 none (font lab tuning) */
    int capAdjust = 0;        /* rows added to the game's capital height when sizing (font lab tuning) */
};
Recipe s_recipe[5];

/* rendered glyphs, kept: the game copies every letter into its atlas again each frame */
struct Cached {
    std::vector<u8> bits;
    s16 height = 0, yOffset = 0;
};
std::unordered_map<u64, Cached> s_cache;

/* the game's own symbols at standard code points (fonts.md "Special symbols"): its bitmaps, never a font's letter;
 * the Pokedollar (0x0024, 0xFF04) is drawn by pokedollar() instead */
bool special(u16 c) {
    switch (c) {
    case 0x2031: case 0x03B2: case 0x337D: case 0x03B1: case 0x337C: case 0x337E:
    case 0x2030: case 0x005C: case 0x25A1: case 0xFF3E:
        return true;
    default:
        return false;
    }
}
bool pokedollar_code(u16 c) { return c == 0x0024 || c == 0xFF04; }
bool kana(u16 c) { return c >= 0x3000 && c <= 0x30FF; }
bool cjk_wide(u16 c) { return kana(c) || (c >= 0xFF00 && c <= 0xFFEF) || (c >= 0x4E00 && c <= 0x9FFF); }

/* The font file read whole into memory (SDL_LoadFile: UTF-8 paths, so a user folder with non-English letters works;
 * FreeType's own file opening uses the C library's narrow-character fopen on Windows) and kept for the face's life. */
/* *Updated 2026-11-16 (the Noto wiring):* each file is read once and shared by every face opened from it, so the
 * 20 MB Noto CJK collection costs its size once however many region faces (JP, KR, SC, TC, HK) are in use. */
struct FontFile {
    void* data;
    size_t len;
};
std::unordered_map<std::string, FontFile> s_font_files;
FT_Face open_face(const std::string& path, long index) {
    auto it = s_font_files.find(path);
    if (it == s_font_files.end()) {
        size_t len = 0;
        void* data = SDL_LoadFile(path.c_str(), &len);
        if (data == nullptr) return nullptr;
        it = s_font_files.emplace(path, FontFile{data, len}).first;
    }
    FT_Face f = nullptr;
    if (FT_New_Memory_Face(s_ft, (const FT_Byte*)it->second.data, (FT_Long)it->second.len, index, &f) != 0) return nullptr;
    return f;
}

/* Windows: the system font folder, then the user's own (fonts installed "for me only" land there). Asked from Windows
 * itself (Known Folders), not from environment variables. *Corrected 2026-11-13:* the first version read
 * WINDIR / LOCALAPPDATA with SDL_getenv, whose lookup is case-sensitive: Git Bash has WINDIR, PowerShell has "windir",
 * so in snatcherLeo's PowerShell launch no font was found and the original letters showed. */
FT_Face find_font(const char* file, long index) {
#ifdef _WIN32
    std::vector<std::string> dirs;
    for (const KNOWNFOLDERID* id : {&FOLDERID_Fonts, &FOLDERID_LocalAppData}) {
        PWSTR w = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, 0, nullptr, &w)) && w != nullptr) {
            char utf8[1024];
            if (WideCharToMultiByte(CP_UTF8, 0, w, -1, utf8, sizeof(utf8), nullptr, nullptr) > 0) {
                std::string d = utf8;
                d += (id == &FOLDERID_Fonts) ? "\\" : "\\Microsoft\\Windows\\Fonts\\";
                dirs.push_back(d);
            }
        }
        CoTaskMemFree(w);
    }
    for (const std::string& d : dirs) {
        if (FT_Face f = open_face(d + file, index)) return f;
    }
#else
    (void)file;
    (void)index;
#endif
    return nullptr;
}

/* A font shipped with OrrePort: Fonts/ next to the exe (app\Fonts\ in the portable layout; the same folder as the hub's
 * Inter and Saira and the Noto set; *moved 2026-11-16* from ui/fonts/). All OSes. */
FT_Face shipped_font(const char* file, long index = 0) {
    char dir[1024];
    if (pc_font_dir(dir, sizeof dir)[0] == '\0') return nullptr;
    return open_face(std::string(dir) + file, index);
}

/* Set a variable font's width and weight axes (by tag; other axes stay at their defaults). */
void set_axes(FT_Face face, float wdth, float wght) {
    FT_MM_Var* mm = nullptr;
    if (FT_Get_MM_Var(face, &mm) != 0 || mm == nullptr) return;
    std::vector<FT_Fixed> coords(mm->num_axis);
    for (FT_UInt i = 0; i < mm->num_axis; ++i) {
        const FT_Var_Axis& ax = mm->axis[i];
        float v = (float)ax.def / 65536.0f;
        if (ax.tag == FT_MAKE_TAG('w', 'd', 't', 'h')) v = wdth;
        if (ax.tag == FT_MAKE_TAG('w', 'g', 'h', 't')) v = wght;
        v = std::max((float)ax.minimum / 65536.0f, std::min((float)ax.maximum / 65536.0f, v));
        coords[i] = (FT_Fixed)std::lround(v * 65536.0f);
    }
    FT_Set_Var_Design_Coordinates(face, mm->num_axis, coords.data());
    FT_Done_MM_Var(s_ft, mm);
}

/* Load one character's outline at a size (and, for a variable font, a width and weight). */
/* FreeType's hinting for the glyph being drawn: 0 normal (the default), 1 light, 2 none. Set from the recipe at the top
 * of each draw (font lab tuning, ORRE_FONT_TUNE); one renderer thread, so a file-level value is enough */
int s_hint = 0;

bool load_outline(FT_Face face, u32 cp, float size, bool var, float wdth, float wght) {
    if (var) set_axes(face, wdth, wght);
    if (FT_Set_Char_Size(face, 0, (FT_F26Dot6)std::lround(size * 64.0f), 72, 72) != 0) return false;
    const FT_UInt gi = FT_Get_Char_Index(face, cp);
    if (gi == 0) return false;
    const FT_Int32 hint = s_hint == 1 ? FT_LOAD_TARGET_LIGHT : s_hint == 2 ? FT_LOAD_NO_HINTING : FT_LOAD_TARGET_NORMAL;
    if (FT_Load_Glyph(face, gi, FT_LOAD_NO_BITMAP | hint) != 0) return false;
    return face->glyph->format == FT_GLYPH_FORMAT_OUTLINE;
}

/* An 8-bit coverage picture of one glyph, with its origin: `left` pixels from the pen, `top` pixels above the
 * baseline (FreeType's bitmap_left / bitmap_top). */
struct Pic {
    int w = 0, h = 0, left = 0, top = 0;
    std::vector<u8> px;
};
Pic grab(const FT_GlyphSlot slot) {
    Pic p;
    const FT_Bitmap& bm = slot->bitmap;
    p.w = (int)bm.width;
    p.h = (int)bm.rows;
    p.left = slot->bitmap_left;
    p.top = slot->bitmap_top;
    p.px.resize((size_t)p.w * (size_t)p.h);
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) p.px[(size_t)y * p.w + x] = bm.buffer[y * bm.pitch + x];
    return p;
}

/* The Pokedollar from the face's own 'P' (proportions in the file comment). Returns the picture and the P's advance
 * (26.6) through `adv`. */
bool pokedollar(const Recipe& r, Pic& out, FT_Pos& adv) {
    s_hint = r.hint;
    if (r.latin == nullptr || !load_outline(r.latin, 'P', r.latinSize, r.latinVar, r.wdth, r.wght)) return false;
    FT_GlyphSlot slot = r.latin->glyph;
    if (r.latinBold) FT_Outline_Embolden(&slot->outline, r.latinBold);
    adv = slot->advance.x;
    if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) return false;
    Pic p = grab(slot);
    if (p.w <= 0 || p.h <= 2) return false;
    auto ink = [&](int x, int y) { return p.px[(size_t)y * p.w + x] >= 128; };
    /* the stem: the run of ink from the leftmost inked pixel in the P's bottom row */
    int s0 = -1;
    for (int x = 0; x < p.w; ++x)
        if (ink(x, p.h - 1)) { s0 = x; break; }
    if (s0 < 0) return false;
    int s1 = s0;
    while (s1 + 1 < p.w && ink(s1 + 1, p.h - 1)) ++s1;
    const int stem = s1 - s0 + 1;
    const int C = p.h; /* the P's height (cap height: no descender) */
    const int t = std::max(1, (int)std::lround(0.14 * C));
    const int y1 = (int)std::lround(0.64 * C), y2 = (int)std::lround(0.86 * C);
    const int bottom = std::max(y2 + t, (int)std::lround(1.12 * C));
    const int padL = (int)std::lround(1.2 * stem) + 1;
    const int right = s1 + (int)std::lround(1.4 * stem);
    Pic d;
    d.w = std::max(p.w, right + 1) + padL;
    d.h = bottom;
    d.left = p.left - padL;
    d.top = p.top;
    d.px.assign((size_t)d.w * (size_t)d.h, 0);
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) d.px[(size_t)y * d.w + x + padL] = p.px[(size_t)y * p.w + x];
    auto fill = [&](int x0, int y0, int x1, int y1e) {
        for (int y = std::max(0, y0); y < std::min(d.h, y1e); ++y)
            for (int x = std::max(0, x0); x < std::min(d.w, x1); ++x) d.px[(size_t)y * d.w + x] = 255;
    };
    const int bl = s0 + padL - (int)std::lround(1.2 * stem), br = right + padL + 1;
    fill(bl, y1, br, y1 + t);
    fill(bl, y2, br, y2 + t);
    fill(s0 + padL, p.h - 1, s1 + padL + 1, bottom); /* the stem carried below the baseline */
    out = std::move(d);
    return true;
}

/* Draw `cp` with the recipe into a picture placed for the slot: font 0 at the pen (part A); fonts 1-4 with the slot's
 * room shared by the side bearings, squeezed (and drawn heavier) if too wide (part C). */
bool draw(const Recipe& r, u16 code, s16 slotW, int inkL, int inkR, Pic& out) {
    s_hint = r.hint;
    if (pokedollar_code(code)) {
        FT_Pos adv = 0;
        if (!pokedollar(r, out, adv)) return false;
        if (r.fitSlot) { /* place by bearings, whole pixels (built from a picture, not an outline) */
            const int lsb = out.left, rsb = std::max(0, (int)((adv + 32) >> 6) - (out.left + out.w));
            const int room = slotW - out.w;
            out.left = (lsb + rsb > 0) ? (int)std::lround((double)room * std::max(0, lsb) / (std::max(0, lsb) + rsb))
                                       : room / 2;
        }
        return true;
    }
    u32 cp = code;
    if (r.smallTweaks && code == 0x7C) cp = 0x00A6; /* the small fonts draw '|' as a broken bar */
    if (r.smallTweaks && code == 0x7E) cp = 0x02DC; /* ... and '~' as a raised tilde */
    /* which font: the kind's own first, then the other */
    const bool wide = r.cjkWide ? cjk_wide(code) : kana(code);
    FT_Face order[2] = {wide ? r.kana : r.latin, wide ? r.latin : r.kana};
    for (FT_Face face : order) {
        if (face == nullptr || FT_Get_Char_Index(face, cp) == 0) continue;
        const bool isLatin = face == r.latin;
        const float em = isLatin ? r.latinSize : r.kanaSize;
        if (!load_outline(face, cp, em, isLatin && r.latinVar, r.wdth, r.wght)) continue;
        FT_GlyphSlot slot = face->glyph;
        if (r.fitSlot) {
            FT_BBox cb;
            FT_Outline_Get_CBox(&slot->outline, &cb);
            FT_Pos adv = slot->advance.x;
            const FT_Pos slot64 = (FT_Pos)slotW * 64;
            const FT_Pos bold = isLatin ? r.latinBold : r.kanaBold;
            /* *Updated 2026-11-13:* every letter goes into the box the game's own letter occupies (its left and
             * right ink edges), so every gap between letters is the game's, pixel for pixel. Why: the summary page's
             * "SNEASEL♂" had a 1-px gap in the game and none in ours (Archivo's L reaches further right than the game's
             * L); placing by the font's own side bearings can't reproduce the game's spacing, placing by the game's
             * ink box does. The emboldening's growth is taken off the box first. A letter with no ink in the game's
             * picture keeps the side-bearing rule. */
            const bool gameBox = inkL >= 0 && inkR >= inkL;
            const FT_Pos boxL = gameBox ? (FT_Pos)inkL * 64 : 0;
            const FT_Pos boxW = gameBox ? std::max<FT_Pos>(64, (FT_Pos)(inkR - inkL + 1) * 64 - bold) : slot64;
            const FT_Pos fit = gameBox ? boxW : slot64 - 32;
            FT_Pos inkW = cb.xMax - cb.xMin;
            if (inkW > fit && isLatin) {
                /* too wide for its box (Roboto's M and W in font 4): heavier first, then squeezed to fit */
                const double sx0 = std::max(0.75, (double)fit / (double)inkW);
                if (r.latinVar) {
                    const float heavier = std::min(900.0f, r.wght + (float)((1.0 - sx0) / sx0 * r.wght));
                    if (!load_outline(face, cp, em, true, r.wdth, heavier)) continue;
                    slot = face->glyph;
                    FT_Outline_Get_CBox(&slot->outline, &cb);
                    adv = slot->advance.x;
                    inkW = cb.xMax - cb.xMin;
                }
                const double sx = std::max(0.75, (double)fit / (double)std::max<FT_Pos>(inkW, 1));
                if (sx < 1.0) {
                    FT_Matrix m = {(FT_Fixed)std::lround(sx * 65536.0), 0, 0, 0x10000};
                    FT_Outline_Transform(&slot->outline, &m);
                    FT_Outline_Get_CBox(&slot->outline, &cb);
                    adv = (FT_Pos)std::lround(adv * sx);
                    inkW = cb.xMax - cb.xMin;
                }
            }
            if (gameBox) {
                /* a non-Latin letter or symbol wider than the box (the fallback font's gender signs and arrows, about
                 * 15 px where the game's narrow font draws its ♂ 9 px: snatcherLeo, "gender icons are too close") is
                 * squeezed to it too, without the weight change (BIZ UDGothic isn't a variable font) */
                if (inkW > boxW) {
                    const double sx = (double)boxW / (double)inkW;
                    FT_Matrix m = {(FT_Fixed)std::lround(sx * 65536.0), 0, 0, 0x10000};
                    FT_Outline_Transform(&slot->outline, &m);
                    FT_Outline_Get_CBox(&slot->outline, &cb);
                    inkW = cb.xMax - cb.xMin;
                }
                const FT_Pos x = boxL + (boxW - inkW) / 2 + bold / 2; /* centred in the game's box */
                FT_Outline_Translate(&slot->outline, x - cb.xMin, 0);
            } else {
                /* the slot's spare room shared out by the letter's own side bearings */
                const FT_Pos lsb = std::max<FT_Pos>(0, cb.xMin), rsb = std::max<FT_Pos>(0, adv - cb.xMax);
                const FT_Pos room = slot64 - inkW;
                const FT_Pos x = (lsb + rsb > 0) ? (FT_Pos)((double)room * lsb / (double)(lsb + rsb)) : room / 2;
                FT_Outline_Translate(&slot->outline, x - cb.xMin, 0);
            }
        }
        const FT_Pos boldNow = isLatin ? r.latinBold : r.kanaBold;
        if (boldNow) FT_Outline_Embolden(&slot->outline, boldNow);
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) continue;
        out = grab(slot);
        return true;
    }
    return false;
}

/* Part C: size the font so its 'H' is exactly as many rows tall as the game's (capHeight), trying quarter-pixel steps
 * around the scored size. Why (2026-11-13, the first battle pictures, measured): at the scored sizes the small fonts'
 * capitals came out 1-2 rows shorter than the game's (font 3's "SNEASEL" 13 rows against 15), so the letters looked
 * small and sat low; matching the capital height fixes both. Done once per font id. */
void size_to_cap(Recipe& r, s16 capHeight) {
    r.sized = true;
    s_hint = r.hint;
    capHeight = (s16)(capHeight + r.capAdjust);
    if (r.latin == nullptr || capHeight <= 0) return;
    auto rows_of_H = [&](float size) -> int {
        if (!load_outline(r.latin, 'H', size, r.latinVar, r.wdth, r.wght)) return -1;
        if (r.latinBold) FT_Outline_Embolden(&r.latin->glyph->outline, r.latinBold);
        if (FT_Render_Glyph(r.latin->glyph, FT_RENDER_MODE_NORMAL) != 0) return -1;
        const FT_Bitmap& bm = r.latin->glyph->bitmap;
        int first = -1, last = -1;
        for (unsigned y = 0; y < bm.rows; ++y)
            for (unsigned x = 0; x < bm.width; ++x)
                if (bm.buffer[y * bm.pitch + x] >= 128) {
                    if (first < 0) first = (int)y;
                    last = (int)y;
                }
        return first < 0 ? -1 : last - first + 1;
    };
    const float start = r.latinSize;
    float best = start;
    int bestErr = 1000;
    for (int step = -8; step <= 16; ++step) { /* -2 px .. +4 px around the scored size */
        const float s = start + 0.25f * (float)step;
        const int rows = rows_of_H(s);
        if (rows < 0) continue;
        const int err = std::abs(rows - capHeight);
        if (err < bestErr || (err == bestErr && std::fabs(s - start) < std::fabs(best - start))) {
            bestErr = err;
            best = s;
        }
    }
    std::printf("[font_render] a small font sized %.2f px (scored %.2f) so its H is %d rows, as the game's (off by %d)\n",
                best, start, capHeight, bestErr);
    r.latinSize = best;
}

int glyph_fetch(u16 fontId, u16 code, s16 width, s16 cellHeight, s16 baseline, s16 capHeight, const u8** pixels,
                s16* height, s16* yOffset) {
    if (fontId > 4 || width <= 0 || special(code) || *height <= 0 || *pixels == nullptr) return 0;
    Recipe& r = s_recipe[fontId];
    if (r.latin == nullptr && r.kana == nullptr) return 0;
    if (r.fitSlot && !r.sized) size_to_cap(r, capHeight);
    const s16 rows0 = *height, yo0 = *yOffset;
    const int stride = (width + 1) / 2;
    const u64 key = ((u64)fontId << 56) | ((u64)(u8)rows0 << 48) | ((u64)(u8)(s8)yo0 << 40) | ((u64)(u16)width << 24) |
                    ((u64)(u8)baseline << 16) | code;
    auto it = s_cache.find(key);
    if (it == s_cache.end()) {
        Cached c;
        /* a character the game leaves blank (font 4's '_') stays blank */
        bool blank = true;
        for (int i = 0; i < stride * rows0 && blank; ++i) blank = (*pixels)[i] == 0;
        /* the game's own ink, left and right columns (for fitting fallback symbols into the same box) */
        int inkL = -1, inkR = -1;
        for (int y = 0; y < rows0; ++y)
            for (int x = 0; x < width; ++x) {
                const u8 b = (*pixels)[y * stride + (x >> 1)];
                if (((x & 1) ? (b & 0x0F) : (b >> 4)) >= 4) {
                    if (inkL < 0 || x < inkL) inkL = x;
                    if (x > inkR) inkR = x;
                }
            }
        Pic p;
        if (!blank && draw(r, code, width, inkL, inkR, p)) {
            /* the box: the original glyph's own (its rows and vertical offset: the game places each picture at its
             * own offset), grown to hold the new letter's ink, within the line's cell. *Corrected 2026-11-13:*
             * part A's first version drew every letter from the line's top in a 22-row box and cut descenders */
            /* *Updated 2026-11-13:* the box may also rise above the line's top (a negative offset: accents on
             * tall letters), and holds at most cell + 1 rows: _msgMakeTexture packs glyphs in atlas rows cell + 2
             * apart and clears one row above and below each picture, so a taller one would be wiped by the next row */
            const int inkTop = baseline - p.top, inkBottom = inkTop + p.h;
            int top = std::min<int>(yo0, inkTop);
            int bottom = std::max<int>(yo0 + rows0, inkBottom);
            const int limit = cellHeight + 1;
            if (bottom - top > limit) {
                if (yo0 + rows0 <= top + limit) bottom = top + limit; /* trim below, keeping the original box */
                else top = bottom - limit;
            }
            c.yOffset = (s16)top;
            c.height = (s16)(bottom - top);
            c.bits.assign((size_t)stride * (size_t)c.height, 0);
            for (int y = 0; y < p.h; ++y) {
                const int dy = inkTop + y - top;
                if (dy < 0 || dy >= c.height) continue;
                for (int x = 0; x < p.w; ++x) {
                    const int dx = p.left + x;
                    if (dx < 0 || dx >= width) continue;
                    const unsigned v = p.px[(size_t)y * p.w + x];
                    const u8 n = (u8)((v * 15u + 127u) / 255u); /* 8-bit coverage to the texture's 16 levels */
                    u8& b = c.bits[(size_t)dy * (size_t)stride + (size_t)(dx >> 1)];
                    if (dx & 1) b = (u8)((b & 0xF0) | n);
                    else b = (u8)((b & 0x0F) | (n << 4));
                }
            }
        }
        it = s_cache.emplace(key, std::move(c)).first;
    }
    if (it->second.bits.empty()) return 0; /* no font has it, or the game's is blank: the original */
    *pixels = it->second.bits.data();
    *height = it->second.height;
    *yOffset = it->second.yOffset;
    return 1;
}

/* --- Part D: characters the game's fonts lack (2026-11-13; snatcherLeo: "Trace what we can, and then, for characters
 * missing (modern stuff), we can just add close alternatives, based on the styling we have") ------------------------
 * When the game's lookup finds no glyph (gs_msg.c, _msgGetCodeInfo; OrreHook_GlyphMissing), the character is drawn
 * once with the open-font recipe of that game font (font 0: MS PGothic + BIZ UDGothic; fonts 1-4: the lab-tuned
 * Archivo / Roboto + BIZ UDGothic Bold, sized to the game's 'H'), on the game font's baseline, its width the font's own
 * advance, and given to the game as a glyph record in a bank of OrrePort's own laid out as the disc's: so the game
 * draws, measures and wraps it as a letter. Part B gets the same character drawn straight from the font at the
 * drawing scale (pc_text_atlas_provide), sharp, not traced from the small picture. All platforms the same. */
struct MissingBank {
    std::vector<u8> arena;                     /* header + pictures; never reallocated (the game keeps pointers) */
    size_t used = 0;
    std::unordered_map<u16, u8*> entries;      /* nullptr: no font has it (the game's white box) */
    std::vector<std::unique_ptr<u8[]>> records; /* 8-byte glyph records, each its own allocation (stable) */
};
MissingBank s_missing[5];
constexpr size_t kMissingArena = 4u << 20;

/* The last resort behind every game font's two open fonts (snatcherLeo, 2026-11-15: "make sure we never have white boxes,
 * that's important"): fonts that cover the scripts and symbols the recipes' fonts lack, tried in order, the character
 * drawn at the cell height like a kana.
 * *Replaced 2026-11-16 (the Noto wiring; decided 2026-11-15, snatcherLeo: "Yes with emoji"):* the chain was the player's own
 * Windows fonts (Yu Gothic, Meiryo, MS Gothic, Segoe UI, Microsoft YaHei, Malgun Gothic, Nirmala UI, Segoe UI Symbol,
 * Segoe UI Historic, Segoe UI Emoji), so Linux and macOS had none and two Windows PCs could differ. It is now Google's
 * Noto set shipped in Fonts/ on every OS (pc_font_files.cpp: the files, the order and why). Each file is opened the
 * first time a character of its script reaches it (pc_noto_covers; the whole set of 82 read at once would be about 53 MB
 * for characters most players never
 * see); only the game thread draws part D's letters, so no lock is needed. */
struct Fallback {
    const PcNotoFont* font = nullptr;
    FT_Face face[5] = {};  /* the CJK collection: one per region face (pc_font_files.h); the others use [0] */
    bool tried[5] = {};
};
std::vector<Fallback> s_fallback;
std::unordered_map<u32, bool> s_fallback_logged; /* characters no font has, reported once each */
void fallback_init() {
    if (!s_fallback.empty()) return;
    for (int i = 0; i < pc_noto_count(); ++i) {
        Fallback f;
        f.font = pc_noto_font(i);
        s_fallback.push_back(f);
    }
}
/* the face that draws cp from this chain entry (the CJK one picks its region face by pc_noto_cjk_face) */
FT_Face fallback_face(Fallback& f, u32 cp) {
    if (!pc_noto_covers(f.font, cp)) return nullptr; /* not this font's script: never opened for it */
    const int slot = f.font->index < 0 ? std::clamp(pc_noto_cjk_face(cp, pc_game_text_language()), 0, 4) : 0;
    if (!f.tried[slot]) {
        f.tried[slot] = true;
        f.face[slot] = shipped_font(f.font->file, f.font->index < 0 ? slot : f.font->index);
        std::printf("[font_render] part D's last resort: %s%s %s\n", f.font->what,
                    f.font->index < 0 ? (slot == 0 ? " JP" : slot == 1 ? " KR" : slot == 2 ? " SC" : slot == 3 ? " TC" : " HK") : "",
                    f.face[slot] ? "opened" : "not found (Fonts/ next to the exe: run port/tools/firstlink.py)");
    }
    return f.face[slot];
}

/* Noto Color Emoji (the project's helper, 2026-11-16): its letters are PNG pictures at one size (a 109-ppem strike; FreeType
 * reads them since Aurora's PNG switch), in colour. The game's text texture holds one grey level per pixel (4 bits; part
 * B's sharp copy one byte), and the game tints it with the text's colour, so an emoji can't show its own colours in the
 * game's text (verified from the texture format, gs_msg.c's GStextureCreate 0x40 = I4; colour would need part B's copy
 * kept in four channels: a guess at the work, not tried). It is drawn as a grey picture instead: each pixel's ink is
 * its opacity times (0.35 + 0.65 x its brightness), so the face, its outline and its eyes stay apart in any text colour
 * (light text on a dark box: the bright parts lit; dark text on a light box: the reverse) rather than a flat
 * silhouette. Sized so the picture is 0.88 of the em (a kanji's ink height), on the font's own baseline. The same on
 * every OS. */
/* *Split 2026-11-16 (shaping):* by glyph index, so a shaped emoji sequence (a skin tone, a family: one glyph HarfBuzz
 * picked from the font's ligatures, pc_shape.h) draws the same way as a single emoji */
bool draw_color_gid(FT_Face face, FT_UInt gi, float em, Pic& out, int& advance) {
    if (gi == 0 || face->num_fixed_sizes < 1 || FT_Select_Size(face, 0) != 0) return false;
    if (FT_Load_Glyph(face, gi, FT_LOAD_COLOR) != 0) return false;
    const FT_GlyphSlot slot = face->glyph;
    const FT_Bitmap& bm = slot->bitmap;
    if (bm.pixel_mode != FT_PIXEL_MODE_BGRA || bm.width == 0 || bm.rows == 0) return false;
    const double f = std::max(0.01, 0.88 * em / (double)bm.rows);
    Pic p;
    p.w = std::max(1, (int)std::lround(bm.width * f));
    p.h = std::max(1, (int)std::lround(bm.rows * f));
    p.left = (int)std::lround(slot->bitmap_left * f);
    p.top = (int)std::lround(slot->bitmap_top * f);
    p.px.assign((size_t)p.w * p.h, 0);
    /* area average of the big picture into the small one (premultiplied BGRA: brightness x opacity needs no divide) */
    for (int y = 0; y < p.h; ++y) {
        const double y0 = y / f, y1 = (y + 1) / f;
        for (int x = 0; x < p.w; ++x) {
            const double x0 = x / f, x1 = (x + 1) / f;
            double acc = 0, area = 0;
            for (int sy = (int)y0; sy < (int)std::ceil(y1) && sy < (int)bm.rows; ++sy) {
                const double wy = std::min(y1, (double)sy + 1) - std::max(y0, (double)sy);
                const unsigned char* row = bm.buffer + (ptrdiff_t)sy * bm.pitch;
                for (int sx = (int)x0; sx < (int)std::ceil(x1) && sx < (int)bm.width; ++sx) {
                    const double wx = std::min(x1, (double)sx + 1) - std::max(x0, (double)sx);
                    const unsigned char* q = row + sx * 4; /* B G R A, premultiplied */
                    const double lum = 0.0722 * q[0] + 0.7152 * q[1] + 0.2126 * q[2];
                    acc += wx * wy * (0.35 * q[3] + 0.65 * lum);
                    area += wx * wy;
                }
            }
            p.px[(size_t)y * p.w + x] = (u8)std::clamp((int)std::lround(area > 0 ? acc / area : 0), 0, 255);
        }
    }
    advance = std::max(p.left + p.w + 1, (int)std::lround(slot->advance.x / 64.0 * f));
    out = std::move(p);
    return true;
}
bool draw_color(FT_Face face, u32 cp, float em, Pic& out, int& advance) {
    return draw_color_gid(face, FT_Get_Char_Index(face, cp), em, out, advance);
}

/* one face of the chain at the game font's size: a variable font at the entry's weight, emboldened as the entry says
 * (scaled with the drawing), CJK a touch heavier in the small fonts (below), colour emoji through draw_color */
bool draw_chain_face(const Recipe& r, const Fallback& f, FT_Face face, u32 cp, float scale, Pic& out, int& advance) {
    if (face == nullptr || FT_Get_Char_Index(face, cp) == 0) return false;
    const float em = r.kanaSize * scale;
    if (f.font->color) return draw_color(face, cp, em, out, advance);
    const bool var = FT_HAS_MULTIPLE_MASTERS(face) && f.font->wght > 0;
    if (!load_outline(face, cp, em, var, 100.f, f.font->wght)) return false;
    FT_GlyphSlot slot = face->glyph;
    /* Noto Sans CJK Bold's ink against the game's kanji (the kanji review, measured on 250 kanji each, 2026-11-16): 1.06x
     * the 22-px font's, 0.90x the 18-px font's; so none at 22 px, and about 0.11 px a side at 18 and 13 px to make up
     * the 10% (likely; the 13-px fonts have no kanji to measure against, a guess) */
    int bold = f.font->bold;
    if (f.font->index < 0 && r.kanaSize < 20) bold += 14;
    const FT_Pos b = (FT_Pos)std::lround(bold * scale);
    if (b) FT_Outline_Embolden(&slot->outline, b);
    advance = (int)((slot->advance.x + 32) >> 6);
    if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) return false;
    out = grab(slot);
    return true;
}

bool draw_face(const Recipe& r, FT_Face face, bool isLatin, u32 code, float scale, Pic& out, int& advance) {
    if (face == nullptr || FT_Get_Char_Index(face, code) == 0) return false;
    const float em = (isLatin ? r.latinSize : r.kanaSize) * scale;
    if (!load_outline(face, code, em, isLatin && r.latinVar, r.wdth, r.wght)) return false;
    FT_GlyphSlot slot = face->glyph;
    const FT_Pos bold = (FT_Pos)std::lround((isLatin ? r.latinBold : r.kanaBold) * scale);
    if (bold) FT_Outline_Embolden(&slot->outline, bold);
    advance = (int)((slot->advance.x + 32) >> 6);
    if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) return false;
    out = grab(slot);
    return true;
}

/* one character for part D at `scale` (1 = the game's size): its picture with its origin and its advance in pixels.
 * cp is the real character (a stand-in already turned back, pc_font_files.h). Returns false when no font has it.
 * *Changed 2026-11-16 (the Noto wiring):* CJK characters (kana, kanji, hangul, CJK signs, full-width forms) go to the
 * shipped Noto CJK first, before the game font's recipe, whose kana font (BIZ UDGothic) is an optional Windows font:
 * so a kanji or hangul in a name draws the same on every OS. Latin keeps the recipe's face first (MS PGothic for font 0
 * is the game's own design, verified pixel by pixel; Archivo / Roboto for fonts 1-4 are shipped). */
bool draw_missing(const Recipe& r, u32 cp, float scale, Pic& out, int& advance) {
    s_hint = r.hint;
    fallback_init();
    const bool cjk = pc_noto_is_cjk(cp) != 0;
    if (cjk) {
        for (Fallback& f : s_fallback)
            if (f.font->index < 0 && draw_chain_face(r, f, fallback_face(f, cp), cp, scale, out, advance)) return true;
    }
    const bool wide = cp <= 0xFFFF && cjk_wide((u16)cp);
    if (draw_face(r, wide ? r.kana : r.latin, !wide, cp, scale, out, advance)) return true;
    if (draw_face(r, wide ? r.latin : r.kana, wide, cp, scale, out, advance)) return true;
    for (Fallback& f : s_fallback)
        if (draw_chain_face(r, f, fallback_face(f, cp), cp, scale, out, advance)) return true;
    return false;
}

/* *Added 2026-11-16 (shaping; pc_shape.h):* a shaped cluster (an Indic syllable, a Thai letter with its marks, an Arabic
 * letter in its joined form, a whole emoji sequence) as one picture. HarfBuzz chose the glyphs and their places in font
 * units from the same Noto file, weight and width draw_chain_face uses; here each glyph is loaded by its index at the game
 * font's size (the kana size, as every chain letter), emboldened as that chain entry says, and laid at its place; the
 * pictures are merged (the darker pixel wins where marks overlap their base). The width is the cluster's advance, so the
 * next cluster joins where HarfBuzz put it. Colour emoji go through draw_color_gid, the same grey rule as a single emoji,
 * each glyph after the last (a sequence the font has as one picture is one glyph). Same on every OS. */
bool draw_cluster(const Recipe& r, const PcShapedRun& run, float scale, Pic& out, int& advance) {
    s_hint = r.hint;
    fallback_init();
    if (run.chain < 0 || run.chain >= (int)s_fallback.size() || run.upem <= 0 || run.count <= 0) return false;
    Fallback& f = s_fallback[(size_t)run.chain];
    FT_Face face = fallback_face(f, run.first_cp);
    if (face == nullptr) return false;
    const float em = r.kanaSize * scale;
    const double k = (double)em / run.upem; /* pixels per font unit */
    struct Part {
        Pic p;
        int dx, dy; /* the glyph's origin from the cluster's pen, whole pixels, y up */
    };
    std::vector<Part> parts;
    if (f.font->color) {
        int pen = 0;
        for (int i = 0; i < run.count; ++i) {
            Part part;
            int adv = 0;
            if (!draw_color_gid(face, run.glyph[i].gid, em, part.p, adv)) return false;
            part.dx = pen, part.dy = 0;
            pen += adv;
            parts.push_back(std::move(part));
        }
        advance = pen;
    } else {
        const bool var = FT_HAS_MULTIPLE_MASTERS(face) && f.font->wght > 0;
        if (var) set_axes(face, 100.f, f.font->wght);
        if (FT_Set_Char_Size(face, 0, (FT_F26Dot6)std::lround(em * 64.0f), 72, 72) != 0) return false;
        const FT_Int32 hint = s_hint == 1 ? FT_LOAD_TARGET_LIGHT : s_hint == 2 ? FT_LOAD_NO_HINTING : FT_LOAD_TARGET_NORMAL;
        const FT_Pos b = (FT_Pos)std::lround(f.font->bold * scale);
        for (int i = 0; i < run.count; ++i) {
            if (FT_Load_Glyph(face, run.glyph[i].gid, FT_LOAD_NO_BITMAP | hint) != 0) return false;
            FT_GlyphSlot slot = face->glyph;
            if (slot->format != FT_GLYPH_FORMAT_OUTLINE) return false;
            if (b) FT_Outline_Embolden(&slot->outline, b);
            if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) return false;
            parts.push_back({grab(slot), (int)std::lround(run.glyph[i].x * k), (int)std::lround(run.glyph[i].y * k)});
        }
        advance = (int)std::lround(run.advance * k);
    }
    /* merge: the union of every part's box */
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0; /* x from the pen, y as rows above the baseline (top exclusive at y1) */
    bool any = false;
    for (const Part& part : parts) {
        if (part.p.w <= 0 || part.p.h <= 0) continue;
        const int l = part.p.left + part.dx, t = part.p.top + part.dy;
        if (!any) x0 = l, x1 = l + part.p.w, y1 = t, y0 = t - part.p.h, any = true;
        x0 = std::min(x0, l), x1 = std::max(x1, l + part.p.w), y1 = std::max(y1, t), y0 = std::min(y0, t - part.p.h);
    }
    Pic m;
    if (any) {
        m.left = x0, m.top = y1, m.w = x1 - x0, m.h = y1 - y0;
        m.px.assign((size_t)m.w * m.h, 0);
        for (const Part& part : parts)
            for (int y = 0; y < part.p.h; ++y)
                for (int x = 0; x < part.p.w; ++x) {
                    const int dx = part.p.left + part.dx + x - x0, dy = y1 - (part.p.top + part.dy) + y;
                    u8& d = m.px[(size_t)dy * m.w + dx];
                    d = std::max(d, part.p.px[(size_t)y * part.p.w + x]);
                }
    }
    advance = std::max(advance, 1);
    out = std::move(m);
    return true;
}

/* *Changed 2026-11-16 (point 4 of "Start here", snatcherLeo, 2026-11-15: "make sure we never have white boxes"):* a character
 * no font has is left out (a blank one pixel wide) and logged once, where it used to be drawn as a '?'. The naming
 * keyboard never lets such a letter in (pc_font_render_can_draw), so this is only for text from elsewhere. */
void log_left_out(u32 cp) {
    if (s_fallback_logged[cp]) return;
    s_fallback_logged[cp] = true;
    std::printf("[font_render] no font has U+%04X: left out (point 4: never a box; add a font that covers it)\n",
                (unsigned)cp);
}

/* ORRE_PARTD_DUMP=<folder> (a test switch, never a setting; the project's build helper, 2026-11-16): each letter part D draws
 * is written as a greyscale PGM, its 1x picture (the record's whole box, cell + 1 rows) and part B's sharp copy at the
 * run's scale, named font<id>_U+<code>_<scale>x.pgm. Why: the frame dumps hold only the game's 640 x 480 copy, so part
 * B's pictures can't be seen there; this shows the box fit (nothing cut at the bottom) at every scale. The pictures are
 * drawn from OrrePort's shipped fonts, not the disc: no game data. C stdio: all OSes. */
void partd_dump(u16 fontId, u32 cp, int w, int h, const u8* grey, int scale) {
    static const char* s_dir = SDL_getenv("ORRE_PARTD_DUMP");
    if (s_dir == nullptr || s_dir[0] == '\0' || w <= 0 || h <= 0) return;
    char path[1200];
    std::snprintf(path, sizeof path, "%s/font%u_U+%04X_%dx.pgm", s_dir, (unsigned)fontId, (unsigned)cp, scale);
    if (FILE* f = std::fopen(path, "wb")) {
        std::fprintf(f, "P5\n%d %d\n255\n", w, h);
        std::fwrite(grey, 1, (size_t)w * h, f);
        std::fclose(f);
    }
}

void* glyph_missing(u16 fontId, u16 code, s16 cellHeight, s16 baseline, s16 capHeight, void** outBank) {
    if (fontId > 4 || cellHeight <= 0) return nullptr;
    /* *Removed 2026-11-15 (snatcherLeo's Japanese hand check: a white box for 最, U+6700; "make sure we never have white
     * boxes"):* a guard here held back every code ending in 00 with a printable ASCII high byte, because the reference playthrough's P02
     * (the 2026-11-02 build) showed font-1 text arriving byte-swapped ('2' as 0x3200; port gap 79). That bug is gone:
     * P02 replayed on the current build through the same scene (rolls identical to the live sitting there) reports no
     * missing glyph at all, against 3,944 then; and no other the reference playthrough sitting ever showed it. The guard only hid real
     * kanji (一 U+4E00, 最 U+6700 and 80 more ending in 00). */
    /* first the Japanese disc's own picture of the letter, when the player added that disc (region work piece 3,
     * 2026-11-15; pc_region_text.cpp): the Japanese game's real kanji, which part B then traces like any of the game's
     * letters. The open fonts below only for what no disc has */
    if (const void* jp = pc_region_font_glyph(fontId, code, cellHeight, outBank)) return const_cast<void*>(jp);
    Recipe& r = s_recipe[fontId];
    if (r.fitSlot && !r.sized) size_to_cap(r, capHeight);
    MissingBank& b = s_missing[fontId];
    if (b.arena.empty()) {
        b.arena.assign(kMissingArena, 0);
        b.arena[7] = 16; /* the data offset (big-endian u32 at 4): pictures follow the 16-byte header */
        b.used = 16;
    }
    const auto it = b.entries.find(code);
    if (it != b.entries.end()) {
        if (it->second != nullptr) *outBank = b.arena.data();
        return it->second;
    }
    /* a stand-in code (a character past U+FFFF in a name: an emoji, a rare kanji; pc_font_files.h) is drawn as the
     * character it stands for */
    const u32 cp = pc_text_real(code);
    /* *Added 2026-11-16 (shaping):* a shaped cluster's stand-in (pc_shape.h) is drawn from its glyph record; everything
     * below (the box, the shrink, part B's copy) treats it as one letter */
    PcShapedRun run;
    const bool isCluster = pc_shape_cluster(code, &run) != 0;
    auto draw_it = [&](float scale, Pic& out, int& adv) {
        return isCluster ? draw_cluster(r, run, scale, out, adv) : draw_missing(r, cp, scale, out, adv);
    };
    Pic p;
    int advance = 0;
    /* *Changed 2026-11-16 (the project's build helper; snatcherLeo's hand check: emoji in names "slightly cut off at the bottom",
     * clearest in "Is <name> OK?"):* the letter's box. Every game font's baseline is its cell's bottom row (measured
     * from each font's own 'H': fonts 0-4 baselines 22 / 18 / 13 / 18 / 13 in cells of 22 / 18 / 13 / 18 / 13, the
     * font lab's dumps), and the game draws descenders and its kana below the cell through each glyph's vertical offset
     * (font 0's kana: rows 3-24; its 'g': from row 11 down past the cell). Part D's record had offset 0 and the cell's
     * rows, so everything below the baseline was cut: the emoji's lower fifth (Noto Color Emoji sits 27 of its 128
     * rows below the baseline), Arabic's dots and tails below the line, a Polish ogonek. Now each letter gets its own
     * box, as each of the game's glyphs has (its own vertical offset and rows): the letter's ink, on the baseline, one
     * spare row below it (part B's sharp copy may round a pixel taller). The room it may use: from the old box's top
     * (the cell's top row, so tall accented capitals keep the room they had) down to the bottom of the game's own kana
     * box (descent = 0.14 of the cell below the baseline: font 0's kana end on row 24, measured), and at most cell + 1
     * rows (the most _msgMakeTexture holds: its atlas rows are cell + 2 apart and it clears one row around each
     * picture). A letter taller than that is drawn again smaller until it fits (as part C squeezes a letter wider than
     * its slot); one that fits but reaches past the room is moved in, the least it can (the emoji: one row up); every
     * other letter stays exactly on the baseline. Same on every OS. */
    const int desc = (int)std::lround(0.14 * cellHeight);
    const int roomTop = baseline - cellHeight, roomBottom = baseline + desc; /* line rows, bottom exclusive */
    const int maxRows = std::min(cellHeight + 1, roomBottom - roomTop);
    float k = 1.f; /* the drawing's shrink, kept for part B's sharp copy */
    bool drew = draw_it(1.f, p, advance);
    for (int tries = 0; drew && p.h > maxRows && tries < 4; ++tries) {
        k *= (float)maxRows / (float)p.h * 0.98f;
        drew = draw_it(k, p, advance);
    }
    if (!drew || p.h > maxRows) {
        if (!drew) log_left_out(cp); /* no font has it: left out, a blank one pixel wide (point 4: never a box) */
        p = Pic{};
        advance = std::max(advance, 1);
    }
    /* the ink's place: on the baseline, moved into the room only as far as it must; the record's box starts there */
    const int inkTop = baseline - p.top;
    const int boxTop = p.h > 0 ? std::clamp(inkTop, roomTop, roomBottom - p.h) : baseline - 1;
    const int shiftY = p.h > 0 ? boxTop - inkTop : 0;
    const int rows = std::clamp(p.h + 1, 1, maxRows);
    const int top = 0; /* the ink's first row in the box */
    /* *Added 2026-11-16 (right-to-left names):* a zero-width mark (an Arabic or Hebrew vowel point with no spacing
     * form, a Thai or Indic mark) has its ink left of the pen, where the letter before it would be: the game's text
     * can't draw over the last letter, so the mark gets its own box, its ink moved right to start at the pen. A letter
     * with a width keeps its place (a joining stroke reaching a pixel left of the pen is simply cut, as before) */
    const int shiftX = (advance <= 1 && p.left < 0) ? -p.left : 0;
    const int w = std::clamp(std::max(advance, p.left + shiftX + p.w), 1, 255);
    const int stride = (w + 1) / 2;
    if (b.used + (size_t)stride * rows > kMissingArena) return nullptr;
    u8* px = &b.arena[b.used];
    for (int y = 0; y < p.h; ++y) {
        const int dy = top + y;
        if (dy < 0 || dy >= rows) continue;
        for (int x = 0; x < p.w; ++x) {
            const int dx = p.left + shiftX + x;
            if (dx < 0 || dx >= w) continue;
            const u8 n = (u8)((p.px[(size_t)y * p.w + x] * 15u + 127u) / 255u);
            u8& cell = px[(size_t)dy * stride + (dx >> 1)];
            cell = (dx & 1) ? (u8)((cell & 0xF0) | n) : (u8)((cell & 0x0F) | (n << 4));
        }
    }
    /* the record: its vertical offset (the top byte, signed) is the box's top row; its picture follows the header */
    const u32 offset = ((u32)(u8)(s8)boxTop << 24) | (u32)(b.used - 16);
    auto rec = std::make_unique<u8[]>(8);
    rec[0] = (u8)(code >> 8), rec[1] = (u8)code, rec[2] = (u8)w, rec[3] = (u8)rows;
    rec[4] = (u8)(offset >> 24), rec[5] = (u8)(offset >> 16), rec[6] = (u8)(offset >> 8), rec[7] = (u8)offset;
    u8* entry = rec.get();
    b.records.push_back(std::move(rec));
    b.entries.emplace(code, entry);
    b.used += (size_t)stride * rows;
    /* once per font and character: which letters part D supplied (tests read this; a short list in a normal game) */
    if (isCluster)
        std::printf("[font_render] part D: font %u U+%04X (shaped cluster from U+%04X, %d glyph%s, %s) drawn\n",
                    (unsigned)fontId, (unsigned)code, (unsigned)run.first_cp, run.count, run.count == 1 ? "" : "s",
                    pc_noto_font(run.chain) != nullptr ? pc_noto_font(run.chain)->what : "?");
    else if (cp != code)
        std::printf("[font_render] part D: font %u U+%04X (stand-in for U+%05X) drawn from the open fonts\n",
                    (unsigned)fontId, (unsigned)code, (unsigned)cp);
    else
        std::printf("[font_render] part D: font %u U+%04X drawn from the open fonts\n", (unsigned)fontId, (unsigned)code);

    /* part B: the same character drawn straight from the font at the drawing scale, for the sharp atlas */
    const int S = pc_text_atlas_scale();
    if (S > 1 && !p.px.empty()) {
        Pic h;
        int adv2 = 0;
        if (draw_it((float)S * k, h, adv2)) {
            std::vector<u8> hi((size_t)w * S * rows * S, 0);
            /* placed as the 1x picture: the same box, the same shrink (k), the same moves (shiftY, shiftX) at S times,
             * and kept inside the box (the sharp drawing's rounding may differ from 1x by a pixel) */
            int topS = (baseline - boxTop + shiftY) * S - h.top;
            if (h.h <= rows * S) topS = std::clamp(topS, 0, rows * S - h.h);
            for (int y = 0; y < h.h; ++y) {
                const int dy = topS + y;
                if (dy < 0 || dy >= rows * S) continue;
                for (int x = 0; x < h.w; ++x) {
                    const int dx = h.left + shiftX * S + x;
                    if (dx < 0 || dx >= w * S) continue;
                    hi[(size_t)dy * w * S + dx] = h.px[(size_t)y * h.w + x];
                }
            }
            pc_text_atlas_provide(w, rows, px, hi.data(), S);
            partd_dump(fontId, isCluster ? code : cp, w * S, rows * S, hi.data(), S);
        }
    }
    {
        std::vector<u8> one((size_t)w * rows); /* the 1x picture's 16 levels as bytes, for the dump */
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < w; ++x) {
                const u8 c2 = px[(size_t)y * stride + (x >> 1)];
                one[(size_t)y * w + x] = (u8)(((x & 1) ? (c2 & 0x0F) : (c2 >> 4)) * 17);
            }
        partd_dump(fontId, isCluster ? code : cp, w, rows, one.data(), 1);
    }
    *outBank = b.arena.data();
    return entry;
}

} // namespace

/* Whether part D can draw a character the game's fonts lack, from a real font (never the '?' stand-in), or the
 * Japanese disc has it (the project's IME work, 2026-11-15). Why: the naming keyboard takes whatever the operating system's
 * input method types and must never let in a letter that would show as a white box or a '?' (snatcherLeo: "make sure we
 * never have white boxes"). Asks font 0's recipe (the name screen's letters are the main font's; likely, from their
 * size) and the last-resort list, the same order draw_missing tries. The game's own symbols at standard code points
 * (the speech bubble at U+2031, the e-Reader marks...) answer no: typed, they would draw the game's symbol, not the
 * letter. Only from the game thread (part D's faces are opened there without a lock). All platforms; what it finds
 * depends on the fonts each OS has (pc_font_render_start's notes: Windows' own fonts verified, Linux / macOS none yet). */
extern "C" int pc_font_render_can_draw_cp(uint32_t cp);
extern "C" int pc_font_render_can_draw(u16 code) { return pc_font_render_can_draw_cp(pc_text_real(code)); }
/* The same for any code point, past U+FFFF too (*added 2026-11-16*, the Noto wiring: an emoji typed in a name is let in
 * when Noto Color Emoji has it, then carried in the game's text by a stand-in code, pc_font_files.h). */
extern "C" int pc_font_render_can_draw_cp(uint32_t cp) {
    if (cp <= 0xFFFF && special((u16)cp)) return 0;
    if (cp <= 0xFFFF && pc_region_font_has(0, (u16)cp)) return 1;
    if (s_ft == nullptr) return 0;
    const Recipe& r = s_recipe[0];
    if (r.latin != nullptr && FT_Get_Char_Index(r.latin, cp) != 0) return 1;
    if (r.kana != nullptr && FT_Get_Char_Index(r.kana, cp) != 0) return 1;
    fallback_init();
    for (Fallback& f : s_fallback) {
        FT_Face face = fallback_face(f, cp);
        if (face != nullptr && FT_Get_Char_Index(face, cp) != 0) return 1;
    }
    return 0;
}

/* The font lab's dump (2026-11-13; docs/port/font-renderer.md "The font lab"). With ORRE_FONT_LAB=<folder>, each
 * font bank the game opens (GSmsgFontOpen, gs_msg.c) is written to <folder>/font<id>_<n>.lab: every glyph's original
 * 4-bit picture and the renderer's own for it, so tools/local/font_lab.py can score every letter against the game's,
 * worst first. Why here: the renderer's own code draws the "ours" half, so the lab scores exactly what players see.
 * The bank is the disc's own bytes (big-endian): a 16-byte header {u16 count, u16, u32 data offset, two links}, then
 * 8-byte records {u16 code, u8 width, u8 height, u32 offset: top byte the vertical offset, low 24 bits the picture's
 * place after the data offset}. The baseline and capital height come from the bank's own 'H', by the same half-dark
 * rule as gs_msg.c's orre_glyph_fetch. A test switch on the developer's PC; the files hold game pictures and never
 * enter git. All OSes the same (C stdio). File layout: "ORRELAB1", u16 font id, u8 cell width, u8 cell height,
 * u32 glyph count, then per glyph: u16 code, u8 width, u8 cell height, s8 baseline, s8 cap height, s8 y offset, u8 rows,
 * the original (rows x ceil(width / 2) bytes), u8 replaced, and if replaced: s8 y offset, u8 rows, the replacement. */
extern "C" void pc_text_atlas_warm(int width, int height, const uint8_t* pixels, uint16_t code);

/* Round 12 (2026-11-17; pc_text_atlas.cpp, s_warmJobs): every letter of a font bank the game just opened, as the game
 * will copy it into its text atlas (OrreHook_GlyphFetch's picture with this bank's own metrics, the same rule as
 * gs_msg.c's orre_glyph_fetch and the lab below; the disc's picture where the renderer leaves it), handed to part B to
 * trace ahead, so no letter shows its blocky first frame (snatcherLeo's PC-box name "flicker"). Runs on the game thread at the
 * bank's opening: the renderer draws each letter once here, which it would do at its first sighting anyway. All OSes
 * the same. */
static void warm_bank(u16 fontId, u8 cellHeight, const u8* bank) {
    auto be16 = [](const u8* p) { return (unsigned)((p[0] << 8) | p[1]); };
    auto be32 = [](const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; };
    const unsigned count = be16(bank);
    const u32 dataOffset = be32(bank + 4);
    s16 baseline = cellHeight, cap = 0;
    for (unsigned i = 0; i < count; ++i) { /* the bank's own 'H' (as the lab below) */
        const u8* g = bank + 0x10 + i * 8;
        if (be16(g) != 'H') continue;
        const int w = g[2], h = g[3], stride = (w + 1) / 2;
        const u32 off = be32(g + 4);
        const u8* px = bank + dataOffset + (off & 0xFFFFFF);
        int first = -1, last = -1;
        for (int y = 0; y < h; ++y)
            for (int b = 0; b < stride; ++b)
                if ((px[y * stride + b] >> 4) >= 8 || (px[y * stride + b] & 0x0F) >= 8) {
                    if (first < 0) first = y;
                    last = y;
                }
        if (last >= 0) {
            baseline = (s16)((s8)(off >> 24) + last + 1);
            cap = (s16)(last - first + 1);
        }
        break;
    }
    /* the game measures a font's metrics once per font id, from the first 'H' it finds (gs_msg.c, orre_font_metrics):
     * the first bank with an 'H' sets them, and a later bank without one (a screen's few extra letters) uses those */
    static s16 s_base[8], s_capH[8];
    static bool s_known[8];
    if (fontId < 8) {
        if (!s_known[fontId] && cap > 0) {
            s_known[fontId] = true;
            s_base[fontId] = baseline;
            s_capH[fontId] = cap;
        }
        if (s_known[fontId]) {
            baseline = s_base[fontId];
            cap = s_capH[fontId];
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        const u8* g = bank + 0x10 + i * 8;
        const u16 code = (u16)be16(g);
        const int w = g[2], h = g[3];
        if (w <= 0 || h <= 0) continue;
        const u8* px = bank + dataOffset + (be32(g + 4) & 0xFFFFFF);
        const u8* ours = px;
        s16 oh = (s16)h, oy = (s16)(s8)(be32(g + 4) >> 24);
        if (OrreHook_GlyphFetch != nullptr && OrreHook_GlyphFetch(fontId, code, (s16)w, cellHeight, baseline, cap, &ours, &oh, &oy)) {
            pc_text_atlas_warm(w, oh, ours, code);
        } else {
            pc_text_atlas_warm(w, h, px, code);
        }
    }
}

extern "C" void pc_font_lab_dump(u16 fontId, u8 cellWidth, u8 cellHeight, const void* bankPtr) {
    static int s_on = -1, s_files = 0;
    static std::string s_dir;
    /* round 12: the warm-up runs at every bank's opening, whether or not the lab is on (this is the one place the port
     * hears of a bank opening) */
    if (bankPtr != nullptr) warm_bank(fontId, cellHeight, (const u8*)bankPtr);
    if (s_on < 0) {
        const char* d = SDL_getenv("ORRE_FONT_LAB");
        s_on = d != nullptr && d[0] != '\0';
        if (s_on) s_dir = d;
    }
    /* *Updated 2026-11-13:* runs whether or not the open fonts draw over the game's letters (ORRE_OPEN_FONTS):
     * without them each glyph is written as "not replaced", and part D's check below still runs */
    if (!s_on || bankPtr == nullptr) return;
    const u8* bank = (const u8*)bankPtr;
    auto be16 = [](const u8* p) { return (unsigned)((p[0] << 8) | p[1]); };
    auto be32 = [](const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; };
    const unsigned count = be16(bank);
    const u32 dataOffset = be32(bank + 4);
    /* the bank's own 'H': baseline under its last half-dark row, capital height = its half-dark rows */
    s16 baseline = cellHeight, cap = 0;
    for (unsigned i = 0; i < count; ++i) {
        const u8* g = bank + 0x10 + i * 8;
        if (be16(g) != 'H') continue;
        const int w = g[2], h = g[3], stride = (w + 1) / 2;
        const u32 off = be32(g + 4);
        const u8* px = bank + dataOffset + (off & 0xFFFFFF);
        int first = -1, last = -1;
        for (int y = 0; y < h; ++y)
            for (int b = 0; b < stride; ++b)
                if ((px[y * stride + b] >> 4) >= 8 || (px[y * stride + b] & 0x0F) >= 8) {
                    if (first < 0) first = y;
                    last = y;
                }
        if (last >= 0) {
            baseline = (s16)((s8)(off >> 24) + last + 1);
            cap = (s16)(last - first + 1);
        }
        break;
    }
    char name[64];
    std::snprintf(name, sizeof(name), "/font%u_%d.lab", (unsigned)fontId, s_files++);
    FILE* f = std::fopen((s_dir + name).c_str(), "wb");
    if (f == nullptr) return;
    std::fwrite("ORRELAB1", 1, 8, f);
    const u8 head[4] = {(u8)(fontId >> 8), (u8)fontId, cellWidth, cellHeight};
    std::fwrite(head, 1, 4, f);
    const u8 cnt[4] = {(u8)(count >> 24), (u8)(count >> 16), (u8)(count >> 8), (u8)count};
    std::fwrite(cnt, 1, 4, f);
    for (unsigned i = 0; i < count; ++i) {
        const u8* g = bank + 0x10 + i * 8;
        const u16 code = (u16)be16(g);
        const int w = g[2], h = g[3], stride = (w + 1) / 2;
        const u32 off = be32(g + 4);
        const u8* px = bank + dataOffset + (off & 0xFFFFFF);
        const u8 rec[8] = {(u8)(code >> 8), (u8)code, (u8)w, cellHeight, (u8)(s8)baseline, (u8)(s8)cap,
                           (u8)(s8)(off >> 24), (u8)h};
        std::fwrite(rec, 1, 8, f);
        std::fwrite(px, 1, (size_t)stride * (size_t)h, f);
        const u8* ours = px;
        s16 oh = (s16)h, oy = (s16)(s8)(off >> 24);
        const int replaced = w > 0 && h > 0 && OrreHook_GlyphFetch != nullptr &&
                             OrreHook_GlyphFetch(fontId, code, (s16)w, cellHeight, baseline, cap, &ours, &oh, &oy);
        const u8 flag = replaced ? 1 : 0;
        std::fwrite(&flag, 1, 1, f);
        if (replaced) {
            const u8 r2[2] = {(u8)(s8)oy, (u8)oh};
            std::fwrite(r2, 1, 2, f);
            std::fwrite(ours, 1, (size_t)stride * (size_t)oh, f);
        }
    }
    std::fclose(f);
    std::printf("[font_lab] font %u: %u glyphs written to %s%s\n", (unsigned)fontId, count, s_dir.c_str(), name);
    /* Part D's check (2026-11-13): the US game never shows a character its fonts lack, so for each main bank the
     * lab also asks part D for a set of such characters (with this bank's own metrics) and writes them, in the same
     * record layout, to font<id>_missing.lab; tools/local/font_lab_missing.py lays them out beside the game's letters.
     * The characters: €, ø, ♪, ✓, ★, 漢, 字, 語, 本, and the kanji in ポケモン図鑑 */
    if (count > 300 && OrreHook_GlyphMissing != nullptr) {
        static const u16 kTest[] = {0x20AC, 0x00F8, 0x266A, 0x2713, 0x2605, 0x6F22, 0x5B57, 0x8A9E, 0x672C, 0x56F3, 0x9451};
        std::snprintf(name, sizeof(name), "/font%u_missing.lab", (unsigned)fontId);
        FILE* m = std::fopen((s_dir + name).c_str(), "wb");
        if (m != nullptr) {
            std::fwrite("ORRELAB1", 1, 8, m);
            std::fwrite(head, 1, 4, m);
            unsigned n = 0;
            std::vector<std::vector<u8>> recs;
            for (u16 code : kTest) {
                void* bank2 = nullptr;
                const u8* e = (const u8*)OrreHook_GlyphMissing(fontId, code, cellHeight, baseline, cap, &bank2);
                if (e == nullptr || bank2 == nullptr) continue;
                const int w = e[2], h = e[3], stride = (w + 1) / 2;
                const u32 off = be32(e + 4);
                const u8* px = (const u8*)bank2 + be32((const u8*)bank2 + 4) + (off & 0xFFFFFF);
                std::vector<u8> r = {(u8)(code >> 8), (u8)code, (u8)w, cellHeight, (u8)(s8)baseline, (u8)(s8)cap,
                                     (u8)(s8)(off >> 24), (u8)h};
                r.insert(r.end(), px, px + (size_t)stride * h);
                r.push_back(0); /* not "replaced": the picture above is part D's own */
                recs.push_back(std::move(r));
                ++n;
            }
            const u8 c2[4] = {(u8)(n >> 24), (u8)(n >> 16), (u8)(n >> 8), (u8)n};
            std::fwrite(c2, 1, 4, m);
            for (const auto& r : recs) std::fwrite(r.data(), 1, r.size(), m);
            std::fclose(m);
            std::printf("[font_lab] font %u: %u of %u part-D test characters drawn\n", (unsigned)fontId, n,
                        (unsigned)(sizeof(kTest) / sizeof(kTest[0])));
        }
    }
}

/* Start-up (pc_platform.c): find the fonts and register the hook, unless ORRE_ORIGINAL_LETTERS=1 asks for the game's
 * own letters this run. With no font found nothing is registered and the game draws its own letters. */
extern "C" void pc_font_render_start(void) {
    const char* orig = SDL_getenv("ORRE_ORIGINAL_LETTERS");
    if (orig != nullptr && orig[0] == '1') {
        std::printf("[font_render] ORRE_ORIGINAL_LETTERS=1: the game's own letters this run\n");
        return;
    }
    if (FT_Init_FreeType(&s_ft) != 0) {
        std::printf("[font_render] FreeType didn't start: the game's own letters\n");
        return;
    }
    FT_Face pgothic = find_font("msgothic.ttc", 2);      /* MS PGothic: font 0's Latin letters (fonts.md, locked) */
    FT_Face bizR = find_font("BIZ-UDGothicR.ttc", 0);    /* BIZ UDGothic Regular: font 0's kana */
    FT_Face bizB = find_font("BIZ-UDGothicB.ttc", 0);    /* BIZ UDGothic Bold: fonts 1-4's kana */
    /* each variable font opened once per game font that uses it, so each keeps its own axis settings */
    FT_Face archivo[3] = {shipped_font("Archivo[wdth,wght].ttf"), shipped_font("Archivo[wdth,wght].ttf"),
                          shipped_font("Archivo[wdth,wght].ttf")};
    FT_Face roboto = shipped_font("Roboto[wdth,wght].ttf");

    Recipe& f0 = s_recipe[0];
    /* *Tuned 2026-11-13 (the font lab: three rounds of ORRE_FONT_TUNE runs scored by tools/local/font_lab.py,
     * English letters and digits against the game's own pictures, 1.0 = identical):* font 0's Latin emboldening 32 ->
     * 12 (0.814 -> 0.900: the lock's +0.25 px per side made 22 letters a row taller and 16 a column wider than the
     * game's); fonts 1-4 without emboldening and with the weights / widths below (font 1 0.789 -> 0.839, font 3 0.773 ->
     * 0.848, font 2 0.699 -> 0.789, font 4 0.759 -> 0.854). Lighter or no hinting and taller sizes scored lower. */
    f0.latin = pgothic, f0.latinSize = 22, f0.latinBold = 12, f0.kana = bizR, f0.kanaSize = 22, f0.kanaBold = 32;
    /* fonts 1-4: face, width, weight, size (sized again to the game's 'H' at first use), cell */
    struct { int id; FT_Face face; float wdth, wght, size, cell; } small[] = {
        {1, archivo[0], 110, 700, 19.0f, 18}, {3, archivo[1], 62, 650, 18.5f, 18}, {2, archivo[2], 105, 650, 15.0f, 13},
        {4, roboto, 85, 700, 15.5f, 13}};
    for (const auto& s : small) {
        Recipe& r = s_recipe[s.id];
        r.latin = s.face, r.latinVar = true, r.wdth = s.wdth, r.wght = s.wght, r.latinSize = s.size;
        r.kanaSize = s.cell;
        /* no emboldening: the font lab scored the heavier weights above closer to the game than +0.25 px (the first
         * battle pictures' 75-83 % ink came from the letters being too small, fixed by sizing to the game's 'H') */
        r.latinBold = 0;
        r.kana = bizB, r.kanaBold = 32, r.fitSlot = true, r.cjkWide = true, r.smallTweaks = true;
    }
    /* ORRE_FONT_TUNE (the font lab's test switch, 2026-11-13; never a setting): per-font overrides for one run,
     * "id:key=value,key=value;id:..." with keys bold / kbold (Latin / kana emboldening, 26.6 units in total: 32 =
     * +0.25 px each side), wght, wdth, size (font 0; fonts 1-4 size themselves to the game's 'H'), hint (0 normal,
     * 1 light, 2 none) and cap (rows added to the game's capital height when sizing). So tools/local/font_lab.py's
     * tuning runs try settings without a rebuild. Platform-neutral. */
    if (const char* tune = SDL_getenv("ORRE_FONT_TUNE")) {
        std::string t = tune;
        size_t pos = 0;
        while (pos < t.size()) {
            size_t end = t.find(';', pos);
            if (end == std::string::npos) end = t.size();
            const std::string part = t.substr(pos, end - pos);
            pos = end + 1;
            const size_t colon = part.find(':');
            if (colon == std::string::npos) continue;
            const int id = std::atoi(part.substr(0, colon).c_str());
            if (id < 0 || id > 4) continue;
            Recipe& r = s_recipe[id];
            size_t p = colon + 1;
            while (p < part.size()) {
                size_t e = part.find(',', p);
                if (e == std::string::npos) e = part.size();
                const std::string kv = part.substr(p, e - p);
                p = e + 1;
                const size_t eq = kv.find('=');
                if (eq == std::string::npos) continue;
                const std::string k = kv.substr(0, eq);
                const float v = (float)std::atof(kv.substr(eq + 1).c_str());
                if (k == "bold") r.latinBold = (FT_Pos)v;
                else if (k == "kbold") r.kanaBold = (FT_Pos)v;
                else if (k == "wght") r.wght = v;
                else if (k == "wdth") r.wdth = v;
                else if (k == "size") r.latinSize = v;
                else if (k == "hint") r.hint = (int)v;
                else if (k == "cap") r.capAdjust = (int)v;
            }
        }
        std::printf("[font_render] ORRE_FONT_TUNE=%s (a font lab test run)\n", tune);
    }
    std::printf("[font_render] font 0: Latin %s, kana %s; fonts 1-3: %s, font 4: %s, their kana: %s\n",
                pgothic ? "MS PGothic" : "the game's own (MS PGothic not found)",
                bizR ? "BIZ UDGothic" : "the game's own (BIZ UDGothic not found)",
                archivo[0] ? "Archivo" : "the game's own (Fonts/Archivo not found)",
                roboto ? "Roboto" : "the game's own (Fonts/Roboto not found)",
                bizB ? "BIZ UDGothic Bold" : "the game's own (BIZ UDGothic Bold not found)");
    /* *Changed 2026-11-13 (snatcherLeo: "Yes, traced", after a side by side of the game's letters, the same traced sharp, and
     * option A):* every character the game's fonts have is drawn from the game's own picture. At the game's own size
     * that is the original picture itself, so the open fonts no longer draw over the game's letters; part B will draw
     * them traced sharp at higher resolutions, and the open fonts stay for characters the game's fonts lack (part D).
     * ORRE_OPEN_FONTS=1 (a test switch, never a setting) still draws option A over every glyph: the font lab and part
     * the development use it. */
    /* part D: characters the game's fonts lack, always (they would otherwise be the game's white box) */
    OrreHook_GlyphMissing = glyph_missing;
    const char* open = SDL_getenv("ORRE_OPEN_FONTS");
    if (open != nullptr && open[0] == '1') {
        OrreHook_GlyphFetch = glyph_fetch;
        std::printf("[font_render] ORRE_OPEN_FONTS=1: option A's open fonts draw over the game's letters this run\n");
    } else {
        std::printf("[font_render] the game's own letters (traced sharp at higher resolutions comes with part B)\n");
    }
}

/* A line of text as an 8-bit coverage picture, for OrrePort's own labels inside the game's screens (the Options
 * screen's Language label, pc_options_lang.cpp; 2026-11-13). Why here: FreeType and the shipped fonts are already
 * set up in this file. Drawn in the shipped Archivo at `size` pixels and the given width / weight axes, from its own
 * face (opened once), so the game fonts' axis settings are never touched. Returns a malloc'd picture of *w x *h bytes
 * (the caller frees it) with the baseline *baseline rows from the top, or NULL when no font or no ink. Platform-neutral
 * (the font is shipped with OrrePort on every OS). */
extern "C" unsigned char* pc_font_render_line(const char* utf8, float size, float wdth, float wght, int* w, int* h,
                                               int* baseline) {
    if (s_ft == nullptr && FT_Init_FreeType(&s_ft) != 0) return nullptr;
    static FT_Face face = nullptr;
    if (face == nullptr) face = shipped_font("Archivo[wdth,wght].ttf");
    if (face == nullptr || utf8 == nullptr) return nullptr;
    /* UTF-8 to code points (the language files are UTF-8) */
    std::vector<u32> cps;
    for (const unsigned char* p = (const unsigned char*)utf8; *p;) {
        u32 c = *p++;
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        while (extra-- > 0 && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
        cps.push_back(c);
    }
    /* each glyph rendered once, placed by its advance; then the pictures are laid on one canvas */
    struct Placed {
        Pic p;
        int x;
    };
    std::vector<Placed> glyphs;
    int pen = 0, top = 0, bottom = 0, right = 0, left = 0;
    s_hint = 1; /* light hinting: a large label, smooth shapes over pixel snapping */
    for (const u32 c : cps) {
        /* a character Archivo lacks (kana, kanji: the naming keyboard's "あ IME" tag, a translated label) from font
         * 0's kana face or the last-resort list, as part D draws it (*added 2026-11-15*, the project's IME work; before, such
         * characters were left out of the line) */
        /* *Changed 2026-11-16 (the Noto wiring):* the last resort is the shipped Noto chain, as part D's (pc_font_files.cpp);
         * its colour emoji are skipped here (a label is one grey picture; part D draws emoji) */
        FT_Face use = face;
        bool var = true;
        if (FT_Get_Char_Index(face, c) == 0) {
            use = nullptr, var = false;
            if (s_recipe[0].kana != nullptr && FT_Get_Char_Index(s_recipe[0].kana, c) != 0) use = s_recipe[0].kana;
            fallback_init();
            for (size_t i = 0; use == nullptr && i < s_fallback.size(); ++i) {
                if (s_fallback[i].font->color) continue;
                FT_Face f = fallback_face(s_fallback[i], c);
                if (f != nullptr && FT_Get_Char_Index(f, c) != 0) use = f;
            }
            if (use == nullptr) continue;
        }
        if (!load_outline(use, c, size, var, wdth, wght)) continue;
        if (FT_Render_Glyph(use->glyph, FT_RENDER_MODE_NORMAL) != 0) continue;
        Placed g{grab(use->glyph), pen};
        left = std::min(left, pen + g.p.left);
        right = std::max(right, pen + g.p.left + g.p.w);
        top = std::max(top, g.p.top);
        bottom = std::max(bottom, g.p.h - g.p.top);
        pen += (int)((use->glyph->advance.x + 32) >> 6);
        glyphs.push_back(std::move(g));
    }
    if (glyphs.empty() || right <= left || top + bottom <= 0) return nullptr;
    *w = right - left;
    *h = top + bottom;
    *baseline = top;
    unsigned char* out = (unsigned char*)std::calloc((size_t)*w * (size_t)*h, 1);
    if (out == nullptr) return nullptr;
    for (const Placed& g : glyphs) {
        for (int y = 0; y < g.p.h; ++y) {
            const int dy = top - g.p.top + y;
            for (int x = 0; x < g.p.w; ++x) {
                const int dx = g.x + g.p.left + x - left;
                if (dx < 0 || dx >= *w || dy < 0 || dy >= *h) continue;
                unsigned char& o = out[(size_t)dy * *w + dx];
                o = std::max(o, g.p.px[(size_t)y * g.p.w + x]);
            }
        }
    }
    return out;
}
