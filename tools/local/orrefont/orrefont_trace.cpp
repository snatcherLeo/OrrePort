// orrefont_trace.cpp: traces the game's letters with the game's own tracer and turns each trace into smooth vector
// outlines, for the OrreFont Colosseum font file (the project's build helper, 2026-11-16; tools/local/orrefont/README.md).
//
// Why a C++ program that includes the game's own file: OrreFont Colosseum must hold the shapes the game draws, so the
// tracing is the exact code the game runs (pc_text_atlas.cpp: `trace` for Latin letters and symbols, `trace_cjk` for
// Japanese characters, the same routing as the game's `is_japanese`), not a Python copy of it. This program #includes
// that file (as tools/local/kanji_review/grade_harness.cpp does), stubs the Aurora calls it makes, answers its
// Japanese-disc font lookup from the glyph dump, traces every letter at its working detail (8x for the Latin tracer,
// 16x for the kanji tracer), and vectorises the traced shape (below). Python (orrefont.py) then writes the font file
// with fontTools. Nothing here writes into the repo: the dump and these outputs hold or describe the disc's letters and
// live in a scratch folder; only the finished font goes into the traced-shapes folder.
//
// The vectorising, per traced shape (a 0/1 picture at 8x or 16x the game's pixels):
//   1. a light Gaussian blur of the 0/1 shape (0.8 working pixel) and the contour at half (marching squares): smooth
//      sub-pixel outlines that follow the traced shape, without its working-pixel stair-steps;
//   2. corners found where the outline turns more than 40 degrees within 3 working pixels (a local maximum), so a
//      stroke's corner stays sharp;
//   3. between corners, a straight line when every point lies within 0.5 working pixel of it, else cubic Bezier
//      curves fitted by least squares (Schneider's method, "An Algorithm for Automatically Fitting Digitized Curves",
//      Graphics Gems, 1990) to within 0.6 working pixel, split where they miss, with one tangent at every split, so
//      the curves join smoothly (no lumpy edges);
//   4. a line within a third of a working pixel of upright or level is made exactly upright or level.
// orrefont.py turns the cubics into TrueType's quadratic curves (fontTools' cu2qu, within one font unit).
//
// Build (MSYS2 MinGW on Windows; any C++20 compiler elsewhere; SDL3 for the declarations the file includes; FreeType,
// with libpng and zlib behind it, for the kanji tracer's rare-kanji rule, which only decides here which letters are
// left out):
//   g++ -O2 -std=c++20 -I<repo> -I<repo>/extern/aurora/include -I<repo>/build-pc/_deps/sdl3_prebuilt-src/include
//       -I<repo>/build-pc/_deps/freetype-build/include -I<repo>/build-pc/_deps/freetype-src/include
//       tools/local/orrefont/orrefont_trace.cpp -o <scratch>/orrefont_trace.exe
//       -L<repo>/build-pc/_deps/sdl3_prebuilt-src/lib -lSDL3 <repo>/build-pc/_deps/freetype-build/libfreetype.a
//       <repo>/build-pc/_deps/png-build/libpng16.dll.a <repo>/build-pc/_deps/zlib-build/libzlib.dll.a
//   (orrefont.py builds it this way; run it with build-pc's SDL3, libpng16 and zlib DLLs on PATH)
// Run: orrefont_trace <glyphs.bin> <jobs.txt> <out folder> [threads] [--used=<kanji_freq.tsv>] [--fonts=<folder with
//      NotoSansCJK-Bold.ttc>] [--crop]
//   glyphs.bin (orrefont.py): u32 count, then per glyph u8 disc (0 US, 1 Japanese), u16 font id, u8 cell height,
//   u16 character, u16 width, u16 height, s8 vertical offset, then width x height levels (0-15), one byte each.
//   jobs.txt: one line per letter to trace, "<glyph index> <L or J>" (L: the Latin tracer, J: the kanji tracer).
//   --crop keeps only what lies inside the glyph's own box (what the game's quad shows); the default keeps the traced
//   shape whole (measured: the difference is a sliver at a few letters' edges; orrefont.py reports it).
// Writes <out>/outlines.txt (per job: "G <glyph> <tracer> <noto> <borrowed> <kept> <spill>", then each contour as
// "M x y", "L x y", "B x1 y1 x2 y2 x y", "Z", in game pixels from the picture's top-left, y down; "E" ends the letter)
// and <out>/trace8.bin (per job: u32 glyph, u16 w, u16 h, then the game's own trace at 8x, w 8 x h 8 bytes: the
// picture part B draws at a drawing scale of 8, cropped to the glyph's box as the game's quad is; for the specimen's
// comparison with the font).
// Platforms: plain C++ and threads, the same on every OS (on Linux / macOS drop the .exe, link SDL3 and FreeType from
// their install). XD: the same program traces XD's letters from an XD glyph dump (OrreFont XD; xd-insights.md 3b).
#ifndef TRACER_FILE
#define TRACER_FILE "port/src/pc_text_atlas.cpp"
#endif
#include TRACER_FILE

#include <fstream>
#include <map>
#include <sstream>
#include <string>

extern "C" {
void (*OrreHook_GlyphCode)(uint16_t code);
}
AuroraWindowSize aurora_get_window_size() { return AuroraWindowSize{}; }
void aurora_orre_hires_texture_register(const void*, const uint8_t*, uint32_t, uint32_t, AuroraTakeDirtyRect) {}

namespace ofont {

struct Glyph {
    uint8_t disc = 0;
    uint16_t font = 0, ch = 0, w = 0, h = 0;
    uint8_t cell = 0;
    int8_t yoff = 0;
    std::vector<uint8_t> px4; /* the game's 4-bit layout: rows of ceil(w / 2) bytes, high nibble first */
};
std::vector<Glyph> s_glyphs;

/* the game's Japanese-disc font lookup (pc_region_text.cpp), answered from the dump's Japanese glyphs: the first of a
 * font id and character, as the game's store keeps the first */
std::map<uint32_t, const Glyph*> s_jp;

std::vector<uint8_t> s_usedBits;
std::string s_fontDir = "../_external/fonts/noto-cjk/";

} // namespace ofont

extern "C" const uint8_t* pc_region_font_picture(uint16_t fontId, uint16_t code, int* cellH, int* w, int* h) {
    const auto it = ofont::s_jp.find((uint32_t)fontId << 16 | code);
    if (it == ofont::s_jp.end()) return nullptr;
    *cellH = it->second->cell, *w = it->second->w, *h = it->second->h;
    return it->second->px4.data();
}
/* "does the Japanese game's text use this letter" (the rare-kanji rule), from freq.py's kanji_freq.tsv; -1 without it,
 * as the game answers before the Japanese disc is read */
extern "C" int pc_region_text_uses(uint16_t code) {
    if (ofont::s_usedBits.empty()) return -1;
    return (ofont::s_usedBits[code >> 3] >> (code & 7)) & 1;
}
extern "C" const char* pc_font_dir(char* out, size_t size) {
    std::snprintf(out, size, "%s", ofont::s_fontDir.c_str());
    return out;
}

namespace ofont {

struct V {
    double x = 0, y = 0;
};
inline V operator+(V a, V b) { return {a.x + b.x, a.y + b.y}; }
inline V operator-(V a, V b) { return {a.x - b.x, a.y - b.y}; }
inline V operator*(V a, double s) { return {a.x * s, a.y * s}; }
inline double dot(V a, V b) { return a.x * b.x + a.y * b.y; }
inline double len(V a) { return std::sqrt(dot(a, a)); }
inline V unit(V a) {
    const double l = len(a);
    return l > 1e-12 ? a * (1.0 / l) : V{0, 0};
}

/* --- 1. the smooth contour of a 0/1 shape ----------------------------------------------------------------------- */

/* Closed loops (in working pixels, the shape's pixel (x, y) covering [x, x + 1) x [y, y + 1)) along the half level of
 * the shape blurred by sigma. Every loop has the ink on the same side, so outer outlines and holes come out in
 * opposite directions (orrefont.py turns them to the font's convention). */
std::vector<std::vector<V>> contours(const std::vector<uint8_t>& bin, int W, int H, double sigma) {
    const int pad = (int)std::ceil(sigma * 3.0) + 2;
    const int FW = W + 2 * pad, FH = H + 2 * pad;
    std::vector<float> f((size_t)FW * FH, 0.f), t((size_t)FW * FH, 0.f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) f[(size_t)(y + pad) * FW + x + pad] = bin[(size_t)y * W + x] ? 1.f : 0.f;
    if (sigma > 0) {
        const int r = (int)std::ceil(sigma * 3.0);
        std::vector<float> k(2 * r + 1);
        float ks = 0;
        for (int i = -r; i <= r; ++i) ks += (k[i + r] = (float)std::exp(-(double)(i * i) / (2.0 * sigma * sigma)));
        for (float& e : k) e /= ks;
        for (int y = 0; y < FH; ++y)
            for (int x = 0; x < FW; ++x) {
                float acc = 0;
                for (int i = -r; i <= r; ++i) {
                    const int xx = x + i;
                    if (xx >= 0 && xx < FW) acc += k[i + r] * f[(size_t)y * FW + xx];
                }
                t[(size_t)y * FW + x] = acc;
            }
        for (int y = 0; y < FH; ++y)
            for (int x = 0; x < FW; ++x) {
                float acc = 0;
                for (int i = -r; i <= r; ++i) {
                    const int yy = y + i;
                    if (yy >= 0 && yy < FH) acc += k[i + r] * t[(size_t)yy * FW + x];
                }
                f[(size_t)y * FW + x] = acc;
            }
    }
    auto val = [&](int i, int j) { return f[(size_t)j * FW + i]; };
    /* an edge between two neighbouring samples: (j * FW + i) * 2, + 1 when it goes down from (i, j) */
    auto crossing = [&](uint64_t key) {
        const int idx = (int)(key >> 1), i = idx % FW, j = idx / FW;
        const int i1 = (key & 1) ? i : i + 1, j1 = (key & 1) ? j + 1 : j;
        const float v0 = val(i, j), v1 = val(i1, j1);
        const double tt = std::fabs(v1 - v0) > 1e-9 ? (0.5 - v0) / (double)(v1 - v0) : 0.5;
        /* sample (i, j) sits at the centre of the shape's pixel (i - pad, j - pad) */
        return V{i - pad + 0.5 + tt * (i1 - i), j - pad + 0.5 + tt * (j1 - j)};
    };
    std::unordered_map<uint64_t, uint64_t> next;
    for (int j = 0; j + 1 < FH; ++j)
        for (int i = 0; i + 1 < FW; ++i) {
            /* the cell's corners a, b, c, d clockwise on screen, and its edges a-b, b-c, c-d, d-a */
            const float c4[4] = {val(i, j), val(i + 1, j), val(i + 1, j + 1), val(i, j + 1)};
            const bool in[4] = {c4[0] >= 0.5f, c4[1] >= 0.5f, c4[2] >= 0.5f, c4[3] >= 0.5f};
            if (in[0] == in[1] && in[1] == in[2] && in[2] == in[3]) continue;
            const uint64_t ek[4] = {(uint64_t)(j * FW + i) * 2, (uint64_t)(j * FW + i + 1) * 2 + 1,
                                    (uint64_t)((j + 1) * FW + i) * 2, (uint64_t)(j * FW + i) * 2 + 1};
            /* crossings in clockwise order: entering the ink (P) or leaving it (Q) */
            int pk[2], qk[2], np = 0, nq = 0, order[4], kinds[4], n = 0;
            for (int e = 0; e < 4; ++e) {
                const bool a = in[e], b = in[(e + 1) & 3];
                if (a == b) continue;
                order[n] = e, kinds[n] = b ? 0 : 1; /* 0: P (out -> in), 1: Q (in -> out) */
                ++n;
                if (b) pk[np++] = e;
                else qk[nq++] = e;
            }
            if (n == 2) {
                next[ek[pk[0]]] = ek[qk[0]];
            } else { /* a saddle: the centre decides whether the two inked corners join */
                const bool centre = (c4[0] + c4[1] + c4[2] + c4[3]) * 0.25f >= 0.5f;
                for (int m = 0; m < 4; ++m) {
                    if (kinds[m] != 0) continue;
                    const int q = centre ? order[(m + 3) & 3] : order[(m + 1) & 3];
                    next[ek[order[m]]] = ek[q];
                }
            }
        }
    std::vector<std::vector<V>> loops;
    std::unordered_map<uint64_t, bool> seen;
    for (const auto& kv : next) {
        if (seen.count(kv.first)) continue;
        std::vector<V> loop;
        uint64_t k = kv.first;
        while (!seen.count(k)) {
            seen[k] = true;
            const V p = crossing(k);
            if (loop.empty() || len(p - loop.back()) > 1e-6) loop.push_back(p);
            const auto it = next.find(k);
            if (it == next.end()) break;
            k = it->second;
        }
        while (loop.size() > 1 && len(loop.front() - loop.back()) < 1e-6) loop.pop_back();
        if (loop.size() >= 3) loops.push_back(std::move(loop));
    }
    /* a fixed order (the map's is not), so a rebuild writes the same file */
    std::sort(loops.begin(), loops.end(), [](const std::vector<V>& a, const std::vector<V>& b) {
        V ma = a[0], mb = b[0];
        for (const V& p : a)
            if (p.y < ma.y || (p.y == ma.y && p.x < ma.x)) ma = p;
        for (const V& p : b)
            if (p.y < mb.y || (p.y == mb.y && p.x < mb.x)) mb = p;
        return ma.y < mb.y || (ma.y == mb.y && ma.x < mb.x);
    });
    /* and each loop starting at its top-left point */
    for (auto& l : loops) {
        size_t best = 0;
        for (size_t i = 1; i < l.size(); ++i)
            if (l[i].y < l[best].y || (l[i].y == l[best].y && l[i].x < l[best].x)) best = i;
        std::rotate(l.begin(), l.begin() + (long)best, l.end());
    }
    return loops;
}

/* --- 2-4. corners, lines and curves ---------------------------------------------------------------------------- */

struct Seg {
    bool line = true;
    V p0, c1, c2, p3;
};

constexpr double kCornerArc = 3.0;    /* working pixels each side of a point when measuring its turn */
constexpr double kCornerDeg = 40.0;   /* a turn sharper than this is a corner */
constexpr double kLineTol = 0.5;      /* a run between corners within this of its chord is a straight line */
constexpr double kCurveTol = 0.6;     /* a fitted curve stays within this of the traced outline */
constexpr double kAxisSnap = 1.0 / 3; /* a line this close to upright or level is made exactly so */

V bez(const Seg& s, double t) {
    const double u = 1 - t;
    return s.p0 * (u * u * u) + s.c1 * (3 * u * u * t) + s.c2 * (3 * u * t * t) + s.p3 * (t * t * t);
}
V bez1(const Seg& s, double t) {
    const double u = 1 - t;
    return (s.c1 - s.p0) * (3 * u * u) + (s.c2 - s.c1) * (6 * u * t) + (s.p3 - s.c2) * (3 * t * t);
}
V bez2(const Seg& s, double t) {
    return (s.c2 - s.c1 * 2 + s.p0) * (6 * (1 - t)) + (s.p3 - s.c2 * 2 + s.c1) * (6 * t);
}

struct Fitter {
    const std::vector<V>& d;
    std::vector<Seg>& out;

    std::vector<double> chord_params(int first, int last) const {
        std::vector<double> u(last - first + 1, 0.0);
        for (int i = first + 1; i <= last; ++i) u[i - first] = u[i - first - 1] + len(d[i] - d[i - 1]);
        const double tot = u.back();
        for (double& e : u) e = tot > 0 ? e / tot : 0;
        return u;
    }
    Seg generate(int first, int last, const std::vector<double>& u, V t1, V t2) const {
        double C[2][2] = {{0, 0}, {0, 0}}, X[2] = {0, 0};
        const V p0 = d[first], p3 = d[last];
        for (int i = first; i <= last; ++i) {
            const double t = u[i - first], s = 1 - t;
            const double b0 = s * s * s, b1 = 3 * t * s * s, b2 = 3 * t * t * s, b3 = t * t * t;
            const V a1 = t1 * b1, a2 = t2 * b2;
            C[0][0] += dot(a1, a1), C[0][1] += dot(a1, a2), C[1][1] += dot(a2, a2);
            const V tmp = d[i] - (p0 * (b0 + b1) + p3 * (b2 + b3));
            X[0] += dot(a1, tmp), X[1] += dot(a2, tmp);
        }
        C[1][0] = C[0][1];
        const double det = C[0][0] * C[1][1] - C[1][0] * C[0][1];
        double al = 0, ar = 0;
        if (std::fabs(det) > 1e-12) {
            al = (X[0] * C[1][1] - X[1] * C[0][1]) / det;
            ar = (C[0][0] * X[1] - C[1][0] * X[0]) / det;
        }
        const double seg = len(p3 - p0), eps = 1e-6 * seg;
        Seg s;
        s.line = false, s.p0 = p0, s.p3 = p3;
        if (al < eps || ar < eps || al > 2 * seg || ar > 2 * seg) al = ar = seg / 3.0; /* the usual fallback */
        s.c1 = p0 + t1 * al, s.c2 = p3 + t2 * ar;
        return s;
    }
    double max_error(int first, int last, const Seg& s, const std::vector<double>& u, int& split) const {
        double mx = 0;
        split = (first + last) / 2;
        for (int i = first + 1; i < last; ++i) {
            const V p = bez(s, u[i - first]);
            const double e = dot(p - d[i], p - d[i]);
            if (e >= mx) mx = e, split = i;
        }
        return mx;
    }
    void reparam(int first, int last, const Seg& s, std::vector<double>& u) const {
        for (int i = first; i <= last; ++i) {
            double t = u[i - first];
            const V q = bez(s, t) - d[i], q1 = bez1(s, t), q2 = bez2(s, t);
            const double num = dot(q, q1), den = dot(q1, q1) + dot(q, q2);
            if (std::fabs(den) > 1e-12) t -= num / den;
            u[i - first] = std::clamp(t, 0.0, 1.0);
        }
    }
    V tangent_at(int i, int first, int last) const { /* pointing back along the outline, from a few points away */
        const int k = std::min(2, std::min(i - first, last - i));
        return unit(d[i - k] - d[i + k]);
    }
    void fit(int first, int last, V t1, V t2, int depth = 0) {
        const double tol2 = kCurveTol * kCurveTol;
        if (last - first == 1 || depth > 40) {
            Seg s;
            s.p0 = d[first], s.p3 = d[last];
            if (last - first == 1) {
                s.line = true;
            } else {
                const double dist = len(s.p3 - s.p0) / 3.0;
                s.line = false, s.c1 = s.p0 + t1 * dist, s.c2 = s.p3 + t2 * dist;
            }
            out.push_back(s);
            return;
        }
        std::vector<double> u = chord_params(first, last);
        Seg s = generate(first, last, u, t1, t2);
        int split = 0;
        double err = max_error(first, last, s, u, split);
        if (err < tol2) {
            out.push_back(s);
            return;
        }
        if (err < tol2 * 16) {
            for (int it = 0; it < 20; ++it) {
                reparam(first, last, s, u);
                s = generate(first, last, u, t1, t2);
                err = max_error(first, last, s, u, split);
                if (err < tol2) {
                    out.push_back(s);
                    return;
                }
            }
        }
        split = std::clamp(split, first + 1, last - 1);
        const V tc = tangent_at(split, first, last);
        fit(first, split, t1, tc, depth + 1);
        fit(split, last, tc * -1.0, t2, depth + 1);
    }
};

/* the turn at each point of a closed loop (degrees), measured between the chords kCornerArc behind and ahead */
std::vector<double> turns(const std::vector<V>& p) {
    const int n = (int)p.size();
    std::vector<double> out(n, 0.0);
    for (int i = 0; i < n; ++i) {
        int b = i, a = i;
        double lb = 0, la = 0;
        for (int s = 0; s < n && lb < kCornerArc; ++s) {
            const int nb = (b - 1 + n) % n;
            lb += len(p[b] - p[nb]);
            b = nb;
        }
        for (int s = 0; s < n && la < kCornerArc; ++s) {
            const int na = (a + 1) % n;
            la += len(p[na] - p[a]);
            a = na;
        }
        const V u = unit(p[i] - p[b]), v = unit(p[a] - p[i]);
        out[i] = std::acos(std::clamp(dot(u, v), -1.0, 1.0)) * 57.29577951308232; /* degrees */
    }
    return out;
}

/* one closed loop as lines and curves */
std::vector<Seg> fit_loop(const std::vector<V>& p) {
    const int n = (int)p.size();
    const auto tr = turns(p);
    std::vector<int> corners;
    for (int i = 0; i < n; ++i) {
        if (tr[i] < kCornerDeg) continue;
        /* a local maximum within the arc on each side */
        bool best = true;
        double l = 0;
        for (int s = 1; s < n && l < kCornerArc && best; ++s) {
            const int j = (i + s) % n;
            l += len(p[j] - p[(j - 1 + n) % n]);
            if (tr[j] > tr[i]) best = false;
        }
        l = 0;
        for (int s = 1; s < n && l < kCornerArc && best; ++s) {
            const int j = (i - s + n) % n;
            l += len(p[(j + 1) % n] - p[j]);
            if (tr[j] >= tr[i]) best = false;
        }
        if (best) corners.push_back(i);
    }
    std::vector<Seg> segs;
    /* the run from corner to corner (or, with none, the whole loop from its first point), as an open polyline */
    auto run = [&](int from, int count, bool smoothEnds) {
        std::vector<V> d;
        for (int s = 0; s <= count; ++s) d.push_back(p[(from + s) % n]);
        const int last = (int)d.size() - 1;
        /* a straight line? */
        const V a = d.front(), b = d.back(), ab = b - a;
        const double L = len(ab);
        double mx = 0;
        if (L > 1e-9)
            for (const V& q : d) mx = std::max(mx, std::fabs((q.x - a.x) * ab.y - (q.y - a.y) * ab.x) / L);
        if (L > 1e-9 && mx <= kLineTol) {
            Seg s;
            s.p0 = a, s.p3 = b;
            segs.push_back(s);
            return;
        }
        /* end tangents: along the outline from a couple of working pixels in, or across the start for a loop with no
         * corner (so the curve closes smoothly) */
        auto along = [&](int i, int dir) {
            double l = 0;
            int j = i;
            while (j + dir >= 0 && j + dir <= last && l < 2.0) {
                l += len(d[j + dir] - d[j]);
                j += dir;
            }
            return unit(d[j] - d[i]);
        };
        V t1 = along(0, 1), t2 = along(last, -1);
        if (smoothEnds) {
            const V tc = unit(p[(from + 2) % n] - p[(from - 2 + n) % n]);
            t1 = tc, t2 = tc * -1.0;
        }
        Fitter f{d, segs};
        f.fit(0, last, t1, t2);
    };
    if (corners.empty()) {
        run(0, n, true);
    } else {
        for (size_t c = 0; c < corners.size(); ++c) {
            const int from = corners[c], to = corners[(c + 1) % corners.size()];
            const int count = corners.size() == 1 ? n : (to - from + n) % n;
            run(from, count, false);
        }
    }
    /* a curve whose handles lie on its chord is a line */
    for (Seg& s : segs) {
        if (s.line) continue;
        const V ab = s.p3 - s.p0;
        const double L = len(ab);
        if (L < 1e-9) continue;
        auto off = [&](V q) { return std::fabs((q.x - s.p0.x) * ab.y - (q.y - s.p0.y) * ab.x) / L; };
        if (off(s.c1) < 0.05 && off(s.c2) < 0.05) s.line = true;
    }
    /* upright and level lines made exact (the shared end points move with them) */
    const int m = (int)segs.size();
    for (int i = 0; i < m; ++i) {
        Seg& s = segs[i];
        if (!s.line) continue;
        Seg& prev = segs[(i - 1 + m) % m];
        Seg& next = segs[(i + 1) % m];
        const V dv = s.p3 - s.p0;
        if (std::fabs(dv.x) < kAxisSnap && std::fabs(dv.y) > 2 * kAxisSnap) {
            const double x = (s.p0.x + s.p3.x) * 0.5;
            const double dx0 = x - s.p0.x, dx3 = x - s.p3.x;
            s.p0.x = x, s.p3.x = x;
            prev.p3.x = x, next.p0.x = x;
            if (!prev.line) prev.c2.x += dx0;
            if (!next.line) next.c1.x += dx3;
        } else if (std::fabs(dv.y) < kAxisSnap && std::fabs(dv.x) > 2 * kAxisSnap) {
            const double y = (s.p0.y + s.p3.y) * 0.5;
            const double dy0 = y - s.p0.y, dy3 = y - s.p3.y;
            s.p0.y = y, s.p3.y = y;
            prev.p3.y = y, next.p0.y = y;
            if (!prev.line) prev.c2.y += dy0;
            if (!next.line) next.c1.y += dy3;
        }
    }
    return segs;
}

/* --- the job: trace, vectorise ------------------------------------------------------------------------------------ */

struct Result {
    int glyph = 0;
    char tracer = 'L';
    bool noto = false, borrowed = false, kept = true;
    double spill = 0;         /* the share of the traced ink outside the glyph's own box */
    std::string outline;      /* the contours, as text */
    std::vector<uint8_t> t8;  /* the game's own trace at 8x (w 8 x h 8) */
};

bool g_crop = false;
constexpr int kMargin = 2; /* blank game pixels added around a Latin picture so its trace is kept whole */

void run_job(Result& r) {
    const Glyph& g = s_glyphs[r.glyph];
    const int w = g.w, h = g.h;
    std::vector<uint8_t> bin;
    int BW = 0, BH = 0, U = 0;
    double ox = 0, oy = 0; /* the shape's top-left, in game pixels from the picture's top-left */
    long inkAll = 0, inkBox = 0;
    if (r.tracer == 'L') {
        /* the Latin tracer at 8x, its working detail (UP), so each output pixel is one of its working pixels. With a
         * blank margin around the picture the trace comes out whole (the tracer crops to the picture's box, the quad
         * the game draws); the picture's own box is checked to be the same either way below */
        r.t8 = trace(w, h, g.px4, 8);
        const int pw = w + 2 * kMargin, ph = h + 2 * kMargin, ps = (pw + 1) / 2, s0 = (w + 1) / 2;
        std::vector<uint8_t> padded((size_t)ps * ph, 0);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const uint8_t b = g.px4[(size_t)y * s0 + (x >> 1)], v = (x & 1) ? (b & 0x0F) : (b >> 4);
                const int X = x + kMargin, Y = y + kMargin;
                padded[(size_t)Y * ps + (X >> 1)] |= (X & 1) ? v : (uint8_t)(v << 4);
            }
        const auto whole = trace(pw, ph, padded, 8);
        BW = pw * 8, BH = ph * 8, U = 8, ox = -kMargin, oy = -kMargin;
        bin.resize(whole.size());
        for (size_t i = 0; i < whole.size(); ++i) bin[i] = whole[i] >= 128;
        for (int y = 0; y < BH; ++y)
            for (int x = 0; x < BW; ++x) {
                if (!bin[(size_t)y * BW + x]) continue;
                ++inkAll;
                const int gx = x - kMargin * 8, gy = y - kMargin * 8;
                const bool inside = gx >= 0 && gy >= 0 && gx < w * 8 && gy < h * 8;
                if (inside) {
                    ++inkBox;
                    if ((r.t8[(size_t)gy * w * 8 + gx] >= 128) != 1) r.kept = false; /* the box must match the game's */
                } else if (g_crop) {
                    bin[(size_t)y * BW + x] = 0;
                }
            }
        for (int y = 0; y < h * 8; ++y) /* and the game's ink inside the box must all be there */
            for (int x = 0; x < w * 8; ++x)
                if (r.t8[(size_t)y * w * 8 + x] >= 128 && !bin[(size_t)(y + kMargin * 8) * BW + x + kMargin * 8])
                    r.kept = false;
    } else {
        /* the kanji tracer as the game calls it (its character passed, as OrreHook_GlyphCode does), at 16x, its working
         * detail; the whole shape (with its kPad border) is the same Shape16 trace_cjk shrinks, rebuilt here the same
         * way and checked to give the game's picture byte for byte */
        CjkReport rep;
        const auto t16 = trace_cjk(w, h, g.px4, 16, g.ch, &rep);
        r.t8 = trace_cjk(w, h, g.px4, 8, g.ch);
        r.noto = rep.noto, r.borrowed = rep.borrowed;
        cjk::Shape16 s;
        if (rep.noto) {
            /* drawn from Noto by the rare-kanji rule: left out of the font by default (orrefont.py), so only the game's
             * own picture traced (as with ORRE_KANJI_NOTO=0) is kept here, for --keep-rare */
            s = cjk::trace16(w, h, g.px4.data());
        } else if (rep.borrowed) {
            int bigCell = 0, bw = 0, bh = 0;
            const uint8_t* bigPx = nullptr;
            for (uint16_t id = 0; id <= 4; ++id) {
                int ch = 0, gw = 0, gh = 0;
                const uint8_t* p = pc_region_font_picture(id, g.ch, &ch, &gw, &gh);
                if (p != nullptr && ch > bigCell) bigCell = ch, bigPx = p, bw = gw, bh = gh;
            }
            s = cjk::fit_larger(g.px4.data(), w, h, cjk::trace16(bw, bh, bigPx));
        } else {
            s = cjk::trace16(w, h, g.px4.data());
        }
        if (!rep.noto && cjk::reduce(s, w, h, 16) != t16) r.kept = false; /* not the game's shape: reported */
        BW = s.mw, BH = s.mh, U = cjk::KU, ox = -kPad, oy = -kPad;
        bin = s.bin;
        for (int y = 0; y < BH; ++y)
            for (int x = 0; x < BW; ++x) {
                if (!bin[(size_t)y * BW + x]) continue;
                ++inkAll;
                const int gx = x - kPad * U, gy = y - kPad * U;
                if (gx >= 0 && gy >= 0 && gx < w * U && gy < h * U) ++inkBox;
                else if (g_crop) bin[(size_t)y * BW + x] = 0;
            }
    }
    r.spill = inkAll > 0 ? (double)(inkAll - inkBox) / (double)inkAll : 0.0;
    const auto loops = contours(bin, BW, BH, 0.8);
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(4);
    auto P = [&](V p) { os << ' ' << (p.x / U + ox) << ' ' << (p.y / U + oy); };
    for (const auto& l : loops) {
        const auto segs = fit_loop(l);
        if (segs.empty()) continue;
        os << 'M';
        P(segs[0].p0);
        os << '\n';
        for (const Seg& s : segs) {
            if (s.line) {
                os << 'L';
                P(s.p3);
            } else {
                os << 'B';
                P(s.c1);
                P(s.c2);
                P(s.p3);
            }
            os << '\n';
        }
        os << "Z\n";
    }
    r.outline = os.str();
}

} // namespace ofont

int main(int argc, char** argv) {
    using namespace ofont;
    if (argc < 4) {
        std::fprintf(stderr, "usage: orrefont_trace <glyphs.bin> <jobs.txt> <out folder> [threads] [--used=<tsv>] "
                             "[--fonts=<folder>] [--crop]\n");
        return 2;
    }
    const std::string out = argv[3];
    int threads = 8;
    for (int i = 4; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--crop") g_crop = true;
        else if (a.rfind("--used=", 0) == 0) {
            std::ifstream f(a.substr(7), std::ios::binary);
            if (!f) {
                std::fprintf(stderr, "can't read %s\n", a.c_str() + 7);
                return 2;
            }
            s_usedBits.assign(0x10000 / 8, 0);
            std::string line;
            while (std::getline(f, line)) { /* "<char>\t<code point>\t<count>" (kanji_review/freq.py) */
                const size_t t = line.find('\t');
                if (t == std::string::npos) continue;
                const unsigned long cp = std::strtoul(line.c_str() + t + 1, nullptr, 10);
                if (cp > 0 && cp < 0x10000) s_usedBits[cp >> 3] |= (uint8_t)(1u << (cp & 7));
            }
        } else if (a.rfind("--fonts=", 0) == 0) {
            s_fontDir = a.substr(8);
            if (!s_fontDir.empty() && s_fontDir.back() != '/' && s_fontDir.back() != '\\') s_fontDir += '/';
        } else {
            threads = std::max(1, std::atoi(a.c_str()));
        }
    }
    std::ifstream gf(argv[1], std::ios::binary);
    uint32_t n = 0;
    gf.read((char*)&n, 4);
    if (!gf) {
        std::fprintf(stderr, "can't read %s\n", argv[1]);
        return 1;
    }
    s_glyphs.resize(n);
    for (auto& g : s_glyphs) {
        gf.read((char*)&g.disc, 1);
        gf.read((char*)&g.font, 2);
        gf.read((char*)&g.cell, 1);
        gf.read((char*)&g.ch, 2);
        gf.read((char*)&g.w, 2);
        gf.read((char*)&g.h, 2);
        gf.read((char*)&g.yoff, 1);
        std::vector<uint8_t> lv((size_t)g.w * g.h);
        gf.read((char*)lv.data(), (std::streamsize)lv.size());
        const int stride = (g.w + 1) / 2;
        g.px4.assign((size_t)stride * g.h, 0);
        for (int y = 0; y < g.h; ++y)
            for (int x = 0; x < g.w; ++x)
                g.px4[(size_t)y * stride + x / 2] |= (x & 1) ? lv[(size_t)y * g.w + x] : (uint8_t)(lv[(size_t)y * g.w + x] << 4);
    }
    if (!gf) {
        std::fprintf(stderr, "%s ends early\n", argv[1]);
        return 1;
    }
    for (const auto& g : s_glyphs)
        if (g.disc == 1) s_jp.emplace((uint32_t)g.font << 16 | g.ch, &g);
    std::vector<Result> jobs;
    {
        std::ifstream jf(argv[2]);
        std::string line;
        while (std::getline(jf, line)) {
            std::istringstream is(line);
            Result r;
            std::string kind;
            if (!(is >> r.glyph >> kind) || r.glyph < 0 || r.glyph >= (int)n) continue;
            r.tracer = kind == "J" ? 'J' : 'L';
            jobs.push_back(std::move(r));
        }
    }
    std::atomic<size_t> nextJob{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; ++t)
        th.emplace_back([&] {
            for (size_t i; (i = nextJob++) < jobs.size();) run_job(jobs[i]);
        });
    for (auto& t : th) t.join();
    std::ofstream of(out + "/outlines.txt", std::ios::binary), tf(out + "/trace8.bin", std::ios::binary);
    int noto = 0, borrowed = 0, notKept = 0;
    for (const auto& r : jobs) {
        char head[160];
        std::snprintf(head, sizeof head, "G %d %c %d %d %d %.5f\n", r.glyph, r.tracer, (int)r.noto, (int)r.borrowed,
                      (int)r.kept, r.spill);
        of << head << r.outline << "E\n";
        const Glyph& g = s_glyphs[r.glyph];
        const uint32_t gi = (uint32_t)r.glyph;
        tf.write((const char*)&gi, 4);
        tf.write((const char*)&g.w, 2);
        tf.write((const char*)&g.h, 2);
        tf.write((const char*)r.t8.data(), (std::streamsize)r.t8.size());
        noto += r.noto, borrowed += r.borrowed, notKept += !r.kept;
    }
    std::printf("%zu letters traced and outlined; drawn from Noto by the rare-kanji rule %d; the larger font's shape %d; "
                "whole shape differing from the game's own picture %d\n",
                jobs.size(), noto, borrowed, notKept);
    return 0;
}
