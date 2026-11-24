/*
 * pc_title_word.cpp: the word on the main menu's LOAD button ("LOAD" in English; first designed as "LOAD SAVE"; snatcherLeo
 * chose "LOAD" on 2026-11-14 once the pellet was narrowed for the glove), made at start-up from the player's own disc
 * (2026-11-14; the design: decisions log 2026-11-13, "LOAD SAVE's lettering is final"; the method: playbook
 * section 6, "New menu words from the game's own lettering"; the prototype it follows step by step: the scratch
 * smooth_word.py, load_save(..., seat_round="O", lo_gap=1.0, l_src=(0, (211, 220)))).
 *
 * *Per language (2026-11-15; snatcherLeo: "However way we made "LOAD" so perfect, we gotta make sure CHARGER, LADEN,
 * CARICA, CARGAR, look in style"):* the word follows the game's language: French CHARGER, German LADEN, Italian CARICA,
 * Spanish CARGAR, Japanese ロード (approved by snatcherLeo 2026-11-15). Each is made the same way as LOAD, its letters traced
 * from THAT language's own Battle Mode word sheet on the player's European / Japanese disc (read by pc_word_pictures.cpp
 * from the disc pc_region_text.cpp found). Where each letter comes from: kWords below and docs/port/title-load-save.md,
 * "The word in each language". Without that disc, or if its sheet isn't the one measured, the word is English LOAD and
 * the log says why (the caller, pc_title_loadsave.cpp, falls back).
 *
 * Why it is made here: the word is drawn in the menu words' style (navy fill, white outline, dark edge) from the
 * game's own Battle Mode word sheet, which isn't loaded on the title screen and must not ship with OrrePort. So the
 * sheet is read from the player's disc (US: colosseumbattle_menu.fsys, entry "menu_070", a 372 x 216 RGB5A3 texture),
 * each needed letter traced smooth, the word set with the game's spacing, the style drawn once around it, and the
 * result shrunk once to the height of Quit's word (25 game pixels, about 62 wide for LOAD).
 *
 * Steps (each as the prototype):
 *   1. Read the archive with DVDOpen / DVDReadPrio: the plain read under DVDRead, so the drive-timing model (accurate
 *      mode) and the read counters never see it. (Another language's sheet: nod's own handle on the language disc,
 *      never the US disc's DVD layer: pc_word_pictures_read.)
 *   2. Unpack the entry (the game's LZSS), decode the texture (RGB5A3 4 x 4 tiles).
 *   3. Each letter's navy coverage from its columns on the sheet (the US sheet's lines and columns, measured: the L and
 *      the O from COLOSSEUM, the game's own L-then-O; D from DOUBLE; A from GBA; the other languages' in kWords).
 *   4. Trace at 8x: a cubic enlargement, the lightest blur that keeps the letter's holes (the topology check), cut at
 *      half.
 *   5. Set the word: letters on one baseline, 2 sheet px apart, 1 between L and O (the game's own COLOSSEUM); a round
 *      letter (O, G) overshoots by half above and below (centred on the cap height); a 6 px space. Japanese kana
 *      instead keep their height on their own line (ー sits mid-height) and each is centred in a slot of the sheet's
 *      kana pitch (20 px).
 *   6. The style once around the word: a white outline 2.6 px, a dark edge 0.8 px, a soft shadow; then a Lanczos shrink
 *      to 25 px tall (Quit's word), squeezed sideways to the pellet's room when the word is longer (the game narrows its
 *      own long menu words the same way; CHARGER and CARGAR need it).
 *
 * Region: the letters' places are each disc's own (US GC6E01, European GC6P01, Japanese GC6J01), checked by the
 * sheet's size before use.
 *
 * Platforms: all the same (plain C++ over the game's own DVD layer and nod; nothing OS-specific; verified on Windows,
 * likely the same on Linux and macOS). For XD: the same method works on XD's word sheets (likely); the places differ.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_word_pictures.h" /* pc_word_pictures_read: another language's copy of the sheet, from its disc */

extern "C" {
#include <dolphin/dvd.h>
}

namespace {

using u8 = uint8_t;

uint32_t be32(const std::vector<u8>& d, size_t o) {
    return o + 4 <= d.size() ? ((uint32_t)d[o] << 24) | ((uint32_t)d[o + 1] << 16) | ((uint32_t)d[o + 2] << 8) | d[o + 3] : 0;
}
uint16_t be16(const std::vector<u8>& d, size_t o) { return o + 2 <= d.size() ? (uint16_t)((d[o] << 8) | d[o + 1]) : 0; }

/* ---- 1. the disc: the whole archive through the plain read (no drive-timing model, no read counters) ---- */
bool read_disc_file(const char* name, std::vector<u8>& out) {
    DVDFileInfo info;
    std::string path = name;
    if (!DVDOpen(path.c_str(), &info)) {
        path = "/" + path;
        if (!DVDOpen(path.c_str(), &info)) return false;
    }
    const uint32_t len = (uint32_t)info.length;
    out.assign(((size_t)len + 31) & ~(size_t)31, 0);
    const s32 got = DVDReadPrio(&info, out.data(), (s32)out.size(), 0, 2);
    DVDClose(&info);
    if (got < (s32)len) return false;
    out.resize(len);
    return true;
}

/* ---- 2. the archive entry and the texture ---- */
std::vector<u8> lzss(const std::vector<u8>& src) {
    std::vector<u8> out;
    u8 win[0x1000] = {};
    int w = 0xFEE;
    size_t i = 0x10;
    unsigned flags = 0;
    const size_t n = src.size();
    while (i < n) {
        flags >>= 1;
        if (!(flags & 0x100)) {
            flags = src[i++] | 0xFF00;
            if (i >= n) break;
        }
        if (flags & 1) {
            const u8 b = src[i++];
            out.push_back(b), win[w] = b, w = (w + 1) & 0xFFF;
        } else {
            if (i + 1 >= n) break;
            const u8 a = src[i], b = src[i + 1];
            i += 2;
            const int off = a | ((b << 4) & 0xF00);
            for (int k = 0; k < (b & 0xF) + 3; ++k) {
                const u8 c = win[(off + k) & 0xFFF];
                out.push_back(c), win[w] = c, w = (w + 1) & 0xFFF;
            }
        }
    }
    return out;
}

bool fsys_entry(const std::vector<u8>& arc, const char* want, std::vector<u8>& out) {
    if (arc.size() < 0x20 || std::memcmp(arc.data(), "FSYS", 4) != 0) return false;
    const uint32_t count = be32(arc, 0x0C), blk = be32(arc, 0x18), tab = be32(arc, blk);
    for (uint32_t k = 0; k < count; ++k) {
        const uint32_t eo = be32(arc, tab + 4 * k);
        if ((size_t)eo + 0x28 > arc.size()) return false;
        const uint32_t doff = be32(arc, eo + 4), flags = be32(arc, eo + 0x0C), stored = be32(arc, eo + 0x14);
        const uint32_t noff = be32(arc, eo + 0x24);
        if (noff >= arc.size()) continue;
        const char* name = reinterpret_cast<const char*>(&arc[noff]);
        if (std::strncmp(name, want, std::strlen(want) + 1) != 0) continue;
        if ((size_t)doff + stored > arc.size()) return false;
        std::vector<u8> raw(arc.begin() + doff, arc.begin() + doff + stored);
        out = (flags & 0x80000000u) ? lzss(raw) : raw;
        return true;
    }
    return false;
}

struct Img { /* straight RGBA, or one channel (c = 1) */
    int w = 0, h = 0, c = 4;
    std::vector<float> p;
    float& at(int x, int y, int k = 0) { return p[((size_t)y * w + x) * c + k]; }
    float get(int x, int y, int k = 0) const { return p[((size_t)y * w + x) * c + k]; }
};

Img make(int w, int h, int c) { Img i; i.w = w, i.h = h, i.c = c, i.p.assign((size_t)w * h * c, 0.f); return i; }

/* a GS texture file: the 0x80-byte header (u16 width, u16 height, s32 GS format at +8, pixel data offset at +0x28,
 * 0x80 when 0), RGB5A3 tiles (GS format 0x90) */
bool decode_rgb5a3_texture(const std::vector<u8>& d, Img& out) {
    const int w = be16(d, 0), h = be16(d, 2);
    const uint32_t fmt = be32(d, 8), off0 = be32(d, 0x28);
    const size_t off = off0 ? off0 : 0x80;
    if (fmt != 0x90 || w <= 0 || h <= 0) return false;
    out = make(w, h, 4);
    const int bx = (w + 3) / 4, by = (h + 3) / 4;
    size_t p = off;
    for (int ty = 0; ty < by; ++ty)
        for (int tx = 0; tx < bx; ++tx)
            for (int i = 0; i < 16; ++i, p += 2) {
                const int x = tx * 4 + i % 4, y = ty * 4 + i / 4;
                const uint16_t v = be16(d, p);
                if (x >= w || y >= h) continue;
                float r, g, b, a;
                if (v & 0x8000) r = ((v >> 10) & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, b = (v & 31) * 255 / 31, a = 255;
                else r = ((v >> 8) & 15) * 17, g = ((v >> 4) & 15) * 17, b = (v & 15) * 17, a = ((v >> 12) & 7) * 255 / 7;
                out.at(x, y, 0) = r, out.at(x, y, 1) = g, out.at(x, y, 2) = b, out.at(x, y, 3) = a;
            }
    return true;
}

/* ---- image operations (as the prototype's Pillow calls) ---- */
constexpr int K = 8;

/* Pillow's BICUBIC (a = -0.5) and LANCZOS (3 lobes) resampling, separable, the kernel widened when shrinking */
float cubic(float x) {
    constexpr float a = -0.5f;
    x = std::fabs(x);
    if (x < 1) return ((a + 2) * x - (a + 3)) * x * x + 1;
    if (x < 2) return (((x - 5) * x + 8) * x - 4) * a;
    return 0;
}
float lanczos(float x) {
    if (x == 0) return 1;
    if (std::fabs(x) >= 3) return 0;
    const float px = 3.14159265f * x;
    return 3 * std::sin(px) * std::sin(px / 3) / (px * px);
}
Img resample_1d(const Img& src, int nw, bool horizontal, float (*kernel)(float), float support) {
    const int sn = horizontal ? src.w : src.h;
    const int dn = nw;
    const float scale = (float)sn / dn, fs = std::max(1.f, scale), sup = support * fs;
    Img out = horizontal ? make(dn, src.h, src.c) : make(src.w, dn, src.c);
    for (int o = 0; o < dn; ++o) {
        const float centre = (o + 0.5f) * scale;
        const int lo = std::max(0, (int)std::floor(centre - sup)), hi = std::min(sn, (int)std::ceil(centre + sup));
        std::vector<float> wts;
        float sum = 0;
        for (int s = lo; s < hi; ++s) wts.push_back(kernel((s - centre + 0.5f) / fs)), sum += wts.back();
        if (sum == 0) sum = 1;
        const int other = horizontal ? src.h : src.w;
        for (int t = 0; t < other; ++t)
            for (int k = 0; k < src.c; ++k) {
                float v = 0;
                for (int s = lo; s < hi; ++s) v += wts[s - lo] * (horizontal ? src.get(s, t, k) : src.get(t, s, k));
                (horizontal ? out.at(o, t, k) : out.at(t, o, k)) = v / sum;
            }
    }
    return out;
}
Img resize(const Img& src, int nw, int nh, float (*kernel)(float), float support) {
    return resample_1d(resample_1d(src, nw, true, kernel, support), nh, false, kernel, support);
}
Img box_shrink(const Img& src, int f) { /* an exact K x K average (Pillow's BOX for a whole-number factor) */
    Img out = make(src.w / f, src.h / f, src.c);
    for (int y = 0; y < out.h; ++y)
        for (int x = 0; x < out.w; ++x)
            for (int k = 0; k < src.c; ++k) {
                float s = 0;
                for (int j = 0; j < f; ++j)
                    for (int i = 0; i < f; ++i) s += src.get(x * f + i, y * f + j, k);
                out.at(x, y, k) = s / (f * f);
            }
    return out;
}
Img gauss(const Img& src, float sigma) { /* one channel */
    if (sigma <= 0) return src;
    const int r = (int)std::ceil(sigma * 3);
    std::vector<float> k(2 * r + 1);
    float sum = 0;
    for (int i = -r; i <= r; ++i) k[i + r] = std::exp(-(float)(i * i) / (2 * sigma * sigma)), sum += k[i + r];
    for (float& v : k) v /= sum;
    Img tmp = make(src.w, src.h, 1), out = make(src.w, src.h, 1);
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            float v = 0;
            for (int i = -r; i <= r; ++i) v += k[i + r] * src.get(std::clamp(x + i, 0, src.w - 1), y);
            tmp.at(x, y) = v;
        }
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            float v = 0;
            for (int i = -r; i <= r; ++i) v += k[i + r] * tmp.get(x, std::clamp(y + i, 0, src.h - 1));
            out.at(x, y) = v;
        }
    return out;
}
Img threshold(const Img& src, float t) {
    Img out = src;
    for (float& v : out.p) v = v > t ? 255.f : 0.f;
    return out;
}
Img max_filter(const Img& src, int r) { /* a (2r+1) square, separable */
    Img tmp = make(src.w, src.h, 1), out = make(src.w, src.h, 1);
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            float m = 0;
            for (int i = std::max(0, x - r); i <= std::min(src.w - 1, x + r); ++i) m = std::max(m, src.get(i, y));
            tmp.at(x, y) = m;
        }
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            float m = 0;
            for (int j = std::max(0, y - r); j <= std::min(src.h - 1, y + r); ++j) m = std::max(m, tmp.get(x, j));
            out.at(x, y) = m;
        }
    return out;
}
/* the enclosed background regions (an O has 1, an L 0): the shape's topology */
int holes(const Img& m) {
    std::vector<u8> seen((size_t)m.w * m.h, 0);
    int count = 0;
    for (int sy = 0; sy < m.h; ++sy)
        for (int sx = 0; sx < m.w; ++sx) {
            if (m.get(sx, sy) > 0 || seen[(size_t)sy * m.w + sx]) continue;
            std::vector<std::pair<int, int>> st = {{sx, sy}};
            seen[(size_t)sy * m.w + sx] = 1;
            bool touches = false;
            while (!st.empty()) {
                const auto [x, y] = st.back();
                st.pop_back();
                if (x == 0 || y == 0 || x == m.w - 1 || y == m.h - 1) touches = true;
                const int nx[4] = {x + 1, x - 1, x, x}, ny[4] = {y, y, y + 1, y - 1};
                for (int d = 0; d < 4; ++d) {
                    if (nx[d] < 0 || ny[d] < 0 || nx[d] >= m.w || ny[d] >= m.h) continue;
                    u8& s = seen[(size_t)ny[d] * m.w + nx[d]];
                    if (s || m.get(nx[d], ny[d]) > 0) continue;
                    s = 1, st.push_back({nx[d], ny[d]});
                }
            }
            count += !touches;
        }
    return count;
}
/* the shape cut to its ink; *top gets the ink's first row in the uncut picture (kana keep their place on the line) */
Img crop_ink(const Img& m, int* top = nullptr) {
    int x0 = m.w, y0 = m.h, x1 = -1, y1 = -1;
    for (int y = 0; y < m.h; ++y)
        for (int x = 0; x < m.w; ++x)
            if (m.get(x, y) > 0) x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
    if (top != nullptr) *top = x1 < 0 ? 0 : y0;
    if (x1 < 0) return make(1, 1, 1);
    Img out = make(x1 - x0 + 1, y1 - y0 + 1, 1);
    for (int y = 0; y < out.h; ++y)
        for (int x = 0; x < out.w; ++x) out.at(x, y) = m.get(x0 + x, y0 + y);
    return out;
}

/* ---- 3. a letter's navy coverage: blue clearly dominant (the outline's black edge and grey shading don't count) ---- */
float navyness(const Img& s, int x, int y) {
    if (s.get(x, y, 3) < 200) return 0;
    const float r = s.get(x, y, 0), g = s.get(x, y, 1), b = s.get(x, y, 2);
    return std::clamp(std::trunc((b - (r + g) / 2 - 10) * 4), 0.f, 255.f);
}
Img letter_shape(const Img& sheet, int y0, int y1, int a, int b) {
    Img out = make(b - a + 2, y1 - y0 + 2, 1);
    for (int y = y0 - 1; y < y1 + 1; ++y)
        for (int x = a - 1; x < b + 1; ++x)
            if (x >= a && x < b && y >= 0 && y < sheet.h) out.at(x - a + 1, y - y0 + 1) = navyness(sheet, x, y);
    return out;
}

/* ---- 4. the trace: smoothest first, keeping the letter's holes ---- */
Img trace(const Img& shape) {
    const Img big = resize(shape, shape.w * K, shape.h * K, cubic, 2.f);
    const int want = holes(threshold(shape, 110));
    for (const float blur : {2.2f, 1.6f, 1.0f, 0.5f, 0.0f}) {
        const Img m = threshold(blur > 0 ? gauss(big, blur * K / 4) : big, 118);
        if (holes(threshold(box_shrink(m, K), 110)) == want) return m;
    }
    return threshold(big, 118);
}

/* ---- 5 and 6. the word, styled, shrunk ---- */
Img grow(const Img& img, int r) {
    Img out = img;
    const int step = std::max(1, r / 3);
    for (int done = 0; done < r;) {
        const int s = std::min(step, r - done);
        out = threshold(gauss(max_filter(out, s), s * 0.35f), 100);
        done += s;
    }
    return out;
}

/* ---- the words, per language: where each letter is on that disc's word sheet (menu_070) ----
 * Our own measurements on each sheet (2026-11-15, read-only scratch scripts segs.py / comps.py / check.py over the
 * sheets: the navy pieces' columns and rows), not disc data: the sheets themselves are read from the player's disc.
 * A letter's place is as the US one's: columns [a, b), rows y0 - 1 .. y1 (one row of margin around the ink), with no
 * other letter's ink inside (checked: each box holds one navy piece). Letters come, where the sheet has them, from a
 * word where they have the same neighbour as in the new word (the US L and O from COLOSSEUM's L-then-O); otherwise
 * from a word where they stand clear of their neighbours, so no neighbour's edge is traced with them. */
struct Letter {
    const char* glyph;  /* the letter (UTF-8) */
    int y0, y1, a, b;   /* its place on the sheet */
    bool round;         /* O, G: overshoots the cap height, half of it below the line */
    const char* from;   /* the word on the sheet it is traced from (for the log; the doc lists them) */
    int lineTop = 0;    /* kana only: the top row of its line's full-height kana (its height on the line is kept) */
    /* a second piece traced together with it, moved by markDx (the dakuten that makes ト into ド); markB 0 = none */
    int markY0 = 0, markY1 = 0, markA = 0, markB = 0, markDx = 0;
};
struct Kern { /* a pair set closer (or wider) than the usual 2 px, as the game's own word sets it */
    const char* first;
    const char* second;
    int gap;
};
struct Word {
    const char* lang;     /* OrrePort's language code */
    const char* text;     /* the word, UTF-8 */
    int sheetW, sheetH;   /* the sheet measured on that disc (any other size: not this word) */
    const char* cap;      /* a flat letter: its height is the cap height round letters are centred on */
    const Letter* letters;
    int count;
    const Kern* kerns;
    int kernCount;
    int pitch;            /* kana: each centred in a slot this wide (sheet px); 0 = letters set 2 px apart */
    bool darkHoles;       /* a hole the outline doesn't fill is the dark edge's colour, as on the game's own ロ */
};

/* English (US disc, 372 x 216): the original LOAD (2026-11-14), unchanged */
const Letter kUs[] = {
    {"L", 5, 21, 211, 220, false, "COLOSSEUM (top line: the L before its first O)"},
    {"O", 5, 21, 221, 235, true, "COLOSSEUM (top line)"},
    {"A", 84, 99, 236, 249, false, "GBA (line 4: GBA POKéMON)"},
    {"D", 188, 208, 144, 157, false, "DOUBLE (DOUBLE BATTLE)"},
};
const Kern kUsKerns[] = {{"L", "O", 1}}; /* COLOSSEUM sets its L and O 1 px apart */

/* French (European disc, cb_menu_fr, 430 x 225): CHARGER. C, H, A from ECHANGE (its C-H-A, as CHARGER's), G and E from
 * ECHANGE (its G-E, as CHARGER's); R from CONTINUER (its last letter, after E, as CHARGER's last R). ECHANGE's letters
 * touch their neighbours by a pale pixel at most (cut between them on the empty column). Flat capitals 14 px, as US */
const Letter kFr[] = {
    {"C", 175, 191, 22, 34, true, "ECHANGE (ECHANGE DE POKéMON)"},
    {"H", 175, 191, 34, 46, false, "ECHANGE"},
    {"A", 175, 191, 46, 59, false, "ECHANGE"},
    {"R", 64, 78, 333, 346, false, "CONTINUER"},
    {"G", 175, 191, 72, 85, true, "ECHANGE"},
    {"E", 175, 191, 86, 96, false, "ECHANGE"},
};
/* German (cb_menu_ge, 321 x 308): LADEN. L and D from DUELL (DUELL/TURNIER), A from GBA (GBA-POKéMON: the one A that
 * stands clear; KAMPF's touch their K), E and N from FORTSETZEN (its E-N, as LADEN's) */
const Letter kDe[] = {
    {"L", 182, 196, 50, 59, false, "DUELL (DUELL/TURNIER)"},
    {"A", 212, 226, 37, 51, false, "GBA (GBA-POKéMON)"},
    {"D", 182, 196, 8, 21, false, "DUELL (DUELL/TURNIER)"},
    {"E", 243, 257, 275, 286, false, "FORTSETZEN"},
    {"N", 243, 257, 287, 298, false, "FORTSETZEN"},
};
/* Italian (cb_menu_it, 231 x 411): CARICA. A, R, I from SCENARIO (its A-R-I, as CARICA's), C from CONTINUA (clear of
 * its neighbours; SCAMBIO's and GIOCATORI's C touch their A) */
const Letter kIt[] = {
    {"C", 152, 166, 7, 20, true, "CONTINUA"},
    {"A", 99, 113, 124, 137, false, "SCENARIO (SCEGLI SCENARIO)"},
    {"R", 99, 113, 138, 150, false, "SCENARIO"},
    {"I", 99, 113, 151, 154, false, "SCENARIO"},
};
/* Spanish (cb_menu_sp, 240 x 400): CARGAR. C, A, R from CONTINUAR (its A-R, as CARGAR's), G from JUGADOR (UN SOLO
 * JUGADOR: its G-A, as CARGAR's) */
const Letter kEs[] = {
    {"C", 5, 19, 10, 22, true, "CONTINUAR"},
    {"A", 5, 19, 94, 107, false, "CONTINUAR"},
    {"R", 5, 19, 108, 120, false, "CONTINUAR"},
    {"G", 325, 339, 124, 137, true, "JUGADOR (UN SOLO JUGADOR)"},
};
/* Japanese (Japanese disc, colosseumbattle_menu, 380 x 313): ロード. ロ from コロシアム (ポケモンコロシアムの), ー from
 * ポケクーポン (クーポン), ド built: the sheet has no ド, so ト from バトル (ともだちとバトル) with the dakuten of だ in the same
 * line (ともだち), placed as the game's voiced kana place it: in the top-right corner of the kana's slot, its top on the
 * line's top and its right edge 4 px inside the slot's right end (the line's kana stand in 20 px slots from x 193: だ's
 * slot is 233-252 and its dakuten ends at 248; バ's, 293-312, ends at 309). ト's slot is 313-332, so the dakuten ends at
 * 329, just right of and above the end of ト's stroke. markDx 81: だ's dakuten's right ink column 248 to 329.
 * *Changed 2026-11-15:* first placed on ト's own right edge (327, markDx 79): the dakuten then ran into the stroke's end
 * and read as a blot, not two marks (likely; the em-corner rule is how fonts place a dakuten) */
const Letter kJa[] = {
    {"ロ", 156, 171, 107, 121, false, "コロシアム (ポケモンコロシアムの)", 155},
    {"ー", 238, 242, 108, 122, false, "クーポン (ポケクーポン)", 232},
    {"ド", 124, 140, 316, 330, false, "ト of バトル (ともだちとバトル) + the dakuten of だ (ともだち)", 124, 124, 128, 244, 249, 81},
};

const Word kWords[] = {
    {"en", "LOAD", 372, 216, "L", kUs, 4, kUsKerns, 1, 0, false},
    {"fr", "CHARGER", 430, 225, "H", kFr, 6, nullptr, 0, 0, false},
    {"de", "LADEN", 321, 308, "L", kDe, 5, nullptr, 0, 0, false},
    /* Italian and Spanish: the sheets' round letters are the flat ones' height (14 px), so A is the cap reference */
    {"it", "CARICA", 231, 411, "A", kIt, 4, nullptr, 0, 0, false},
    {"es", "CARGAR", 240, 400, "A", kEs, 4, nullptr, 0, 0, false},
    /* the kana pitch: ポケクーポン's kana start 20 px apart (47, 67, 87, 108, 127, 147) */
    {"ja", "ロード", 380, 313, nullptr, kJa, 3, nullptr, 0, 20, true},
};

const Word* word_for(const char* lang) {
    for (const Word& w : kWords)
        if (lang != nullptr && std::strcmp(w.lang, lang) == 0) return &w;
    return nullptr;
}

/* a UTF-8 word's letters, one string each */
std::vector<std::string> letters_of(const char* s) {
    std::vector<std::string> out;
    for (size_t i = 0; s[i] != '\0';) {
        size_t n = 1;
        while ((s[i + n] & 0xC0) == 0x80) ++n;
        out.emplace_back(s + i, n);
        i += n;
    }
    return out;
}

/* the Battle Mode archive, read once while the title's pictures are made (the word and the save list's bag pieces
 * both come from it) and dropped afterwards (pc_title_disc_done) */
std::vector<u8> s_battleArc;
bool battle_archive() {
    if (!s_battleArc.empty()) return true;
    if (read_disc_file("colosseumbattle_menu.fsys", s_battleArc)) return true;
    std::printf("[load_save] colosseumbattle_menu.fsys not found on the disc\n");
    return false;
}

/* the word's sheet: the US disc's from the Battle Mode archive, another language's from its own disc
 * (pc_word_pictures_read: the European disc's cb_menu_<l> copy, the Japanese disc's colosseumbattle_menu one) */
bool read_sheet(const Word& wd, Img& sheet) {
    std::vector<u8> tex;
    if (std::strcmp(wd.lang, "en") == 0) {
        if (!battle_archive() || !fsys_entry(s_battleArc, "menu_070", tex)) {
            std::printf("[load_save] the word sheet (menu_070) isn't on the disc: no word\n");
            return false;
        }
    } else {
        size_t size = 0;
        u8* file = pc_word_pictures_read(wd.lang, "menu_070", &size);
        if (file == nullptr) {
            std::printf("[load_save] %s: the language disc's word sheet (menu_070) isn't there (no disc, not read yet, or "
                        "the word pictures are off)\n", wd.lang);
            return false;
        }
        tex.assign(file, file + size);
        std::free(file);
    }
    if (!decode_rgb5a3_texture(tex, sheet)) {
        std::printf("[load_save] %s: the word sheet (menu_070) isn't as expected: no word\n", wd.lang);
        return false;
    }
    if (sheet.w != wd.sheetW || sheet.h != wd.sheetH) {
        std::printf("[load_save] %s: the word sheet is %d x %d, not the measured %d x %d: no word\n", wd.lang, sheet.w,
                    sheet.h, wd.sheetW, wd.sheetH);
        return false;
    }
    return true;
}

} // namespace

/* a GS texture from the Battle Mode archive, by entry name: its size, its GS format and its tiled texel data as on the
 * disc (the caller frees it with delete[]). Used for the save list's bag pieces (menu_010, RGBA8). */
extern "C" int pc_title_battle_texture(const char* entry, uint8_t** texels, int* w, int* h, int* format, int* bytes,
                                       uint32_t sampling[5]) {
    std::vector<u8> tex;
    if (!battle_archive() || !fsys_entry(s_battleArc, entry, tex) || tex.size() < 0x80) return 0;
    const uint32_t off0 = be32(tex, 0x28);
    const size_t off = off0 ? off0 : 0x80;
    if (off >= tex.size()) return 0;
    *w = be16(tex, 0), *h = be16(tex, 2), *format = (int)be32(tex, 8);
    /* the texture's own wrap and filter settings (the header's +0x10..+0x23, as GStextureHandle: wrap S / T, min / mag
     * filter, mip filter): a copy drawn with the defaults instead showed a seam where the bag's mirrored halves meet */
    for (int i = 0; i < 5; ++i) sampling[i] = be32(tex, 0x10 + 4 * i);
    *bytes = (int)(tex.size() - off);
    *texels = new uint8_t[(size_t)*bytes];
    std::memcpy(*texels, &tex[off], (size_t)*bytes);
    return 1;
}

/* the archive is no longer needed once the title's pictures are made */
extern "C" void pc_title_disc_done(void) { std::vector<u8>().swap(s_battleArc); }

/* The word in the game's language (lang: OrrePort's code; "en", or any language without a word here, makes LOAD) as
 * straight RGBA bytes (w x h, delete[] by the caller), at most maxW wide (squeezed sideways beyond it; 0: no limit).
 * Returns 0 when that language's sheet can't be read or isn't the one measured (the log says why; the caller then asks
 * for "en"). */
extern "C" int pc_loadsave_word(const char* lang, int maxW, uint8_t** rgbaOut, int* wOut, int* hOut) {
    const Word* found = word_for(lang);
    const Word& wd = found != nullptr ? *found : kWords[0];
    Img sheet;
    if (!read_sheet(wd, sheet)) return 0;
    /* *Changed 2026-11-14 (snatcherLeo):* the English word is "LOAD" alone ("Seems kinda scrunched" in the narrower pellet),
     * its height Quit's word's (25 px, "25 px (Quit's height)"); "save file" stays in the help line and the list's title */
    /* *Changed 2026-11-14 (snatcherLeo: "I just think the O is slightly off ... the bottom of the O specifically ... Just make
     * sure its round"):* the O is COLOSSEUM's (line 0, the O right after this L), not DOUBLE's: DOUBLE's sits off the
     * pixel grid (15 px wide, smeared over an extra column, like its L) and shrunk to 25 px its bottom came to a slight
     * point; COLOSSEUM's is 14 px, crisp and symmetric top to bottom (the sheet's four Os compared) */
    const int count = wd.count;
    std::vector<Img> traced((size_t)count);
    std::vector<int> inkTop((size_t)count, 0);
    for (int i = 0; i < count; ++i) {
        const Letter& L = wd.letters[i];
        Img shape = letter_shape(sheet, L.y0, L.y1, L.a, L.b);
        /* a second piece traced with it (ド's dakuten): its navy laid into the letter's box, markDx columns along */
        if (L.markB > L.markA)
            for (int y = L.markY0 - 1; y <= L.markY1; ++y)
                for (int x = L.markA; x < L.markB; ++x) {
                    const int dx = x + L.markDx - L.a + 1, dy = y - L.y0 + 1;
                    if (dx < 0 || dy < 0 || dx >= shape.w || dy >= shape.h || y < 0 || y >= sheet.h) continue;
                    shape.at(dx, dy) = std::max(shape.get(dx, dy), navyness(sheet, x, y));
                }
        traced[i] = crop_ink(trace(shape), &inkTop[i]);
    }
    /* *The O made round (snatcherLeo, 2026-11-14: "Make sure the bottom of the "O" in LOAD has no slight smudge"):* its bottom
     * came to a 1-pixel point after the shrink. The traced O is made symmetric (the union of it and its mirror images,
     * top to bottom and side to side), and its dip below the baseline halved (below); together its bottom curve matches
     * its top (four versions compared in the prototype, this the roundest) */
    for (int i = 0; i < count; ++i) {
        if (std::strcmp(wd.letters[i].glyph, "O") != 0) continue;
        Img& o = traced[i];
        Img sym = o;
        for (int y = 0; y < o.h; ++y)
            for (int x = 0; x < o.w; ++x)
                sym.at(x, y) = std::max({o.get(x, y), o.get(o.w - 1 - x, y), o.get(x, o.h - 1 - y), o.get(o.w - 1 - x, o.h - 1 - y)});
        o = sym;
    }
    auto index_of = [&](const std::string& g) {
        for (int i = 0; i < count; ++i)
            if (g == wd.letters[i].glyph) return i;
        return -1;
    };
    const std::vector<std::string> text = letters_of(wd.text);
    const int n = (int)text.size();
    for (const std::string& g : text)
        if (g != " " && index_of(g) < 0) {
            std::printf("[load_save] %s: the letter %s of %s has no place on the sheet: no word\n", wd.lang, g.c_str(), wd.text);
            return 0;
        }
    const int capIndex = wd.cap != nullptr ? index_of(wd.cap) : -1;
    const int cap = capIndex >= 0 ? traced[capIndex].h : 0;
    /* places: x along the line, y from the baseline up (negative); a round letter centred on the cap height. Kana: y
     * from the line's top (the letter's own height on its line kept), x centred in its slot */
    struct Place { const Img* img; int x, y; bool isO; };
    std::vector<Place> placed;
    int x = 0;
    for (int i = 0; i < n; ++i) {
        const int li = text[i] == " " ? -1 : index_of(text[i]);
        if (li < 0) x += 6 * K;
        else if (wd.pitch > 0) {
            const Img* p = &traced[li];
            const Letter& L = wd.letters[li];
            placed.push_back({p, i * wd.pitch * K + (wd.pitch * K - p->w) / 2, (L.y0 - 1 - L.lineTop) * K + inkTop[li], false});
        } else {
            const Img* p = &traced[li];
            int lift = 0;
            /* the round letter's overshoot, half of it below the baseline as the game's; at this size halved again, so
             * its bottom lands on a clean pixel row (see "The O made round" above) */
            if (wd.letters[li].round) lift -= (int)std::lround((p->h - cap) / 4.0);
            placed.push_back({p, x, -p->h - lift, text[i] == "O"});
            x += p->w;
            if (i < n - 1) { /* 2 sheet px to the next letter, or the game's own gap for a kerned pair (L-O: 1) */
                int gap = 2;
                for (int k = 0; k < wd.kernCount; ++k)
                    if (text[i] == wd.kerns[k].first && text[i + 1] == wd.kerns[k].second) gap = wd.kerns[k].gap;
                x += gap * K;
            }
        }
    }
    int left = 0, right = 0, top = 0, bottom = 0;
    for (size_t i = 0; i < placed.size(); ++i) {
        const Place& pl = placed[i];
        left = i == 0 ? pl.x : std::min(left, pl.x), right = std::max(right, pl.x + pl.img->w);
        top = i == 0 ? pl.y : std::min(top, pl.y), bottom = i == 0 ? pl.y + pl.img->h : std::max(bottom, pl.y + pl.img->h);
    }
    if (wd.pitch == 0) top = std::min(top, 0), bottom = std::max(bottom, 0), left = std::min(left, 0); /* as LOAD: from 0 */
    constexpr int kOutline = (int)(2.6 * K), kEdge = (int)(0.8 * K), kPad = kOutline + kEdge + 3 * K;
    Img m = make(right - left + 2 * kPad, bottom - top + 2 * kPad, 1);
    for (const Place& pl : placed)
        for (int yy = 0; yy < pl.img->h; ++yy)
            for (int xx = 0; xx < pl.img->w; ++xx)
                if (pl.img->get(xx, yy) > 0) m.at(kPad + pl.x - left + xx, kPad + pl.y - top + yy) = 255;
    const Img white = grow(m, kOutline);
    Img edge = grow(white, kEdge);
    /* the background reachable from the picture's border (outside every letter and its outline) */
    auto outside_of = [&](const Img& img) {
        std::vector<u8> outside((size_t)img.w * img.h, 0);
        std::vector<std::pair<int, int>> st;
        for (int px = 0; px < img.w; ++px) st.push_back({px, 0}), st.push_back({px, img.h - 1});
        for (int py = 0; py < img.h; ++py) st.push_back({0, py}), st.push_back({img.w - 1, py});
        while (!st.empty()) {
            const auto [px, py] = st.back();
            st.pop_back();
            if (px < 0 || py < 0 || px >= img.w || py >= img.h) continue;
            u8& o = outside[(size_t)py * img.w + px];
            if (o || img.get(px, py) > 0) continue;
            o = 1;
            st.push_back({px + 1, py}), st.push_back({px - 1, py}), st.push_back({px, py + 1}), st.push_back({px, py - 1});
        }
        return outside;
    };
    /* *Japanese (2026-11-15):* ロ's hole is wider than the outline, and on the game's sheet what the white ring leaves
     * inside is dark (the edge and shadow colour), not see-through: so the inside the outline and edge don't reach is
     * filled with the edge colour (only for the words that ask for it: LOAD's O keeps its own centre, below) */
    if (wd.darkHoles) {
        const std::vector<u8> outside = outside_of(edge);
        for (int py = 0; py < edge.h; ++py)
            for (int px = 0; px < edge.w; ++px)
                if (!outside[(size_t)py * edge.w + px]) edge.at(px, py) = 255;
    }
    const Img shadow = gauss(edge, K * 0.9f);
    /* *The O's centre, as CONTINUE's (snatcherLeo, 2026-11-14: "Match the O in "LOAD" center to look like the CONTINUE "O"";
     * an open centre like OPTIONS' was tried first and set aside):* the outline fills the O's hole with white, as the
     * game's menu words do, and the dark edge closing up left only a faint grey speck in the middle. CONTINUE's O has a
     * crisp navy dot there instead, a dark core about 1.5 px wide and 2 px tall (game pixels, from a PC-build frame), in the
     * letters' own navy. So the dot is drawn at the hole's centre at that size. The hole: the background inside the
     * fill that doesn't reach the picture's border, within the O's own columns. */
    std::vector<u8> oHole((size_t)m.w * m.h, 0);
    {
        const std::vector<u8> outside = outside_of(m);
        for (const Place& pl : placed) {
            if (!pl.isO) continue;
            for (int py = 0; py < m.h; ++py)
                for (int px = kPad + pl.x - left; px < kPad + pl.x - left + pl.img->w; ++px)
                    if (m.get(px, py) == 0 && !outside[(size_t)py * m.w + px]) oHole[(size_t)py * m.w + px] = 1;
        }
    }
    double holeX = 0, holeY = 0, holeN = 0;
    for (int py = 0; py < m.h; ++py)
        for (int px = 0; px < m.w; ++px)
            if (oHole[(size_t)py * m.w + px]) holeX += px, holeY += py, ++holeN;
    /* compose (premultiplied): the shadow (offset 0.9 / 1.1 px), the dark edge, the white outline, the navy fill */
    Img out = make(m.w, m.h, 4);
    auto over = [&](int px, int py, const float rgb[3], float a) {
        if (a <= 0) return;
        for (int k = 0; k < 3; ++k) out.at(px, py, k) = rgb[k] * a + out.at(px, py, k) * (1 - a);
        out.at(px, py, 3) = a + out.at(px, py, 3) * (1 - a);
    };
    const float shc[3] = {10, 6, 28}, edc[3] = {26, 20, 50}, whc[3] = {244, 242, 246}, fic[3] = {10, 20, 92};
    const int sx = (int)(0.9 * K), sy = (int)(1.1 * K);
    for (int py = 0; py < m.h; ++py)
        for (int px = 0; px < m.w; ++px) {
            const int qx = px - sx, qy = py - sy;
            if (qx >= 0 && qy >= 0) over(px, py, shc, shadow.get(qx, qy) / 255.f * 0.45f);
            over(px, py, edc, edge.get(px, py) / 255.f);
            over(px, py, whc, white.get(px, py) / 255.f);
            over(px, py, fic, m.get(px, py) / 255.f);
        }
    /* crop to the drawn area, shrink to Quit's word height (25 px, outline included: its word spans rows 7-31) */
    int x0 = out.w, y0 = out.h, x1 = -1, y1 = -1;
    for (int py = 0; py < out.h; ++py)
        for (int px = 0; px < out.w; ++px)
            if (out.get(px, py, 3) > 0) x0 = std::min(x0, px), y0 = std::min(y0, py), x1 = std::max(x1, px), y1 = std::max(y1, py);
    /* the O's navy dot (see above): 3 x 2 game pixels, i.e. that many times the drawing's size per game pixel (the word
     * is shrunk to 25 px from its drawn height), an ellipse with soft edges (4 x 4 samples per pixel) */
    if (holeN > 0) {
        const double perGamePx = (double)(y1 - y0 + 1) / 25.0;
        /* the dot's dark core is about 1.5 x 2 game pixels (the 3 x 2 first measured counted its soft halo) */
        const double cx = holeX / holeN + 0.5, cy = holeY / holeN + 0.5, rx = 0.75 * perGamePx, ry = 1.0 * perGamePx;
        for (int py = (int)(cy - ry) - 1; py <= (int)(cy + ry) + 1; ++py)
            for (int px = (int)(cx - rx) - 1; px <= (int)(cx + rx) + 1; ++px) {
                if (px < 0 || py < 0 || px >= out.w || py >= out.h) continue;
                int in = 0;
                for (int j = 0; j < 4; ++j)
                    for (int i = 0; i < 4; ++i) {
                        const double dx = (px + (i + 0.5) / 4 - cx) / rx, dy = (py + (j + 0.5) / 4 - cy) / ry;
                        in += dx * dx + dy * dy <= 1.0;
                    }
                over(px, py, fic, in / 16.f);
            }
    }
    Img cropped = make(x1 - x0 + 1, y1 - y0 + 1, 4);
    for (int py = 0; py < cropped.h; ++py)
        for (int px = 0; px < cropped.w; ++px)
            for (int k = 0; k < 4; ++k) cropped.at(px, py, k) = out.get(x0 + px, y0 + py, k);
    constexpr int kHeight = 25;
    const int fh = kHeight, natural = (int)std::lround((double)cropped.w * kHeight / cropped.h);
    /* *A long word (2026-11-15):* wider than the pellet's room it is squeezed sideways in the same shrink, its height
     * kept, as the game narrows its own long menu words (the European word sheets' long labels are narrowed letters) */
    const int fw = maxW > 0 ? std::min(natural, maxW) : natural;
    const Img small = resize(cropped, fw, fh, lanczos, 3.f); /* premultiplied, as Pillow resizes RGBA */
    uint8_t* rgba = new uint8_t[(size_t)fw * fh * 4];
    for (int py = 0; py < fh; ++py)
        for (int px = 0; px < fw; ++px) {
            const float a = std::clamp(small.get(px, py, 3), 0.f, 1.f);
            u8* o = &rgba[((size_t)py * fw + px) * 4];
            for (int k = 0; k < 3; ++k) o[k] = (u8)std::lround(a > 0 ? std::clamp(small.get(px, py, k) / a, 0.f, 255.f) : 0.f);
            o[3] = (u8)std::lround(a * 255);
        }
    *rgbaOut = rgba, *wOut = fw, *hOut = fh;
    std::string sources;
    for (int i = 0; i < count; ++i) sources += std::string(i ? "; " : "") + wd.letters[i].glyph + " from " + wd.letters[i].from;
    std::printf("[load_save] %s: the word %s made from the %s disc's word sheet (%d x %d%s; %s)\n", wd.lang, wd.text,
                std::strcmp(wd.lang, "en") == 0 ? "US" : (std::strcmp(wd.lang, "ja") == 0 ? "Japanese" : "European"),
                fw, fh, fw < natural ? (", squeezed from " + std::to_string(natural)).c_str() : "", sources.c_str());
    return 1;
}
