/*
 * pc_text_atlas.cpp: the font renderer's part B (step 2.2, 2026-11-13; docs/port/font-renderer.md "Part B").
 *
 * What it does: the game's text is drawn from a 512 x 512 4-bit atlas (two of them, double buffered) that
 * _msgMakeTexture fills letter by letter from the CPU (gs_msg.c). This keeps a copy of each atlas at the drawing
 * resolution, S times larger (S = the drawing height / 480, rounded up), and Aurora draws the copy instead of the
 * game's atlas (aurora_orre_hires_texture_register, include/aurora/gfx.h). Every letter the game copies into its atlas
 * is copied into ours at S times the place, as the game's own picture **traced sharp**: so the letters keep the game's
 * exact shapes, widths and spacing, and are crisp at 1440p or 4K. The game's quads use normalised UVs, so they sample
 * the same place in the larger copy: no game code changes, and the layout is the game's.
 *
 * Why this design (snatcherLeo, 2026-11-13: "Yes, traced", "it can't hamper game performance"):
 *   - Pictures are recognised by their pixels (a hash), not by font and character, so dialogue, the small fonts, ruby
 *     text and any later replacement picture (part D's open fonts) all go through one path.
 *   - Tracing never runs on the game's thread: a picture seen for the first time is queued for two low-priority worker
 *     threads and drawn enlarged without smoothing until its trace is ready (the game rewrites its text every frame,
 *     so the sharp version appears a frame or two later); traces are kept for the rest of the run.
 *   - Only the rectangle the letters changed is uploaded, when Aurora would have read the game's atlas, so the order
 *     of writes and draws stays the game's (the game double-buffers its atlas per frame; so do we).
 *
 * The tracer, per picture (the rule checked with snatcherLeo on every glyph of all five fonts; tools/local/font_trace_mockup.py
 * is its prototype): the 16-level picture enlarged 8x (Catmull-Rom), then for each setting, smoothest first, a light
 * Gaussian blur and a hard cut; the settings that keep the game picture's structure (its separate pieces and its holes)
 * win (the closest when none keeps all), and of those the smoothest whose round trip (shrunk back to the game's size)
 * stays within 2x of the most faithful. The kept shape is shrunk by area to S x: crisp anti-aliased edges.
 *
 * Switches: ORRE_TEXT_HIRES=0 turns it off for one run (the game's atlas, enlarged by the GPU, as before). Not a
 * setting: always on, look only (draws no random numbers, changes no layout), so it also runs in GameCube-accurate
 * mode. snatcherLeo's planned "Nostalgia" .orremod would turn it off.
 *
 * Platforms: all the same (standard C++, Aurora's WebGPU upload). The worker threads' lower priority is asked for
 * through SDL (SDL_SetCurrentThreadPriority), which each OS maps to its own scheduler.
 */
#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <aurora/gfx.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" {
void pc_text_atlas_init(const void* image0, const void* image1);
void pc_text_atlas_glyph(const void* image, int atlasX, int atlasY, int width, int height, const uint8_t* pixels);
int pc_text_atlas_scale(void);
void pc_text_atlas_provide(int width, int height, const uint8_t* pixels4, const uint8_t* hi, int scale);
/* the character copied next (gs_msg.c orre_glyph_fetch; include/pc/orre_hooks.h): picks the kanji tracer */
extern void (*OrreHook_GlyphCode)(uint16_t code);
/* pc_region_text.cpp: a Japanese disc font's own picture of a character (the kanji tracer's proposal 2 looks for the
 * same character in a larger font) */
const uint8_t* pc_region_font_picture(uint16_t fontId, uint16_t code, int* cellH, int* w, int* h);
/* pc_region_text.cpp: whether the Japanese game's own text uses a character (1 / 0; -1 not known yet), computed from
 * the Japanese disc's messages at run time (the rare-kanji rule below) */
int pc_region_text_uses(uint16_t code);
/* pc_font_files.cpp: the shipped fonts' folder (Noto Sans CJK for the rare-kanji rule) */
const char* pc_font_dir(char* out, size_t size);
/* round 12: the warm-up's entry (pc_font_render.cpp, at each font's opening), and the game frame (pc_vi.c) for the
 * first-sighting log */
void pc_text_atlas_warm(int width, int height, const uint8_t* pixels, uint16_t code);
extern volatile uint32_t pc_vi_heartbeat;
}

/* FreeType, for the rare-kanji rule's Noto shapes (built with Aurora; the renderer's own, pc_font_render.cpp) */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

namespace {

constexpr int UP = 8; /* the tracer's working detail per game pixel */
/* (blur in game pixels, cut level out of 255), smoothest first: as the checked prototype */
constexpr float kBlur[] = {0.7f, 0.55f, 0.45f, 0.35f, 0.25f, 0.25f, 0.15f, 0.f, 0.f, 0.f, 0.f};
constexpr int kCut[] = {128, 128, 128, 128, 128, 144, 144, 128, 144, 160, 176};
constexpr int kSettings = 11;
constexpr double kRoundTripSlack = 2.0;
constexpr int kPad = 2; /* empty pixels around a picture, so the interpolation sees its edges */

int s_scale = 0; /* S; 0 = off */

/* One high-resolution atlas copy */
struct Atlas {
    const void* gameImage = nullptr;
    std::vector<uint8_t> pixels; /* 512 S x 512 S, one byte of intensity each */
    int size = 0;
    uint32_t dirty[4] = {0, 0, 0, 0};
};
Atlas s_atlas[2];
std::mutex s_atlasLock; /* the dirty rectangles (the pixels are written by the game thread, read by Aurora's) */

/* Traced pictures, by their pixels */
struct Trace {
    std::atomic<bool> ready{false};
    std::atomic<bool> warmQueued{false}; /* round 12: still waiting in the warm-up queue (s_warmJobs) */
    bool provided = false;       /* drawn by part D from an open font (pc_text_atlas_provide), not traced */
    std::vector<uint8_t> pixels; /* (w S) x (h S) */
};
std::unordered_map<uint64_t, std::shared_ptr<Trace>> s_traces;
std::mutex s_traceLock;

struct Job {
    uint64_t key;
    int w, h;
    bool cjk = false;         /* a Japanese character: the kanji tracer (trace_cjk) */
    uint16_t code = 0;        /* its character (the kanji tracer's larger-font check) */
    std::vector<uint8_t> src; /* the 4-bit picture, as given */
    std::shared_ptr<Trace> out;
};
std::deque<Job> s_jobs;
/* *Added 2026-11-17 (round 12, snatcherLeo: names "flicker" the first time the PC box's cursor passes them, never again):*
 * a letter drawn before its trace is ready shows the game's picture enlarged without smoothing for a frame or two,
 * then the sharp one: that switch is the flicker, and only the first sighting of a letter does it. So every letter of
 * each font the game opens is traced ahead of time (pc_text_atlas_warm, from the font's opening in pc_font_render.cpp),
 * on these same low-priority threads, but after any letter already on screen (s_jobs always goes first; a warm job a
 * screen needs before its turn is moved up). ORRE_TEXT_WARM=0 turns the warm-up off for one run. All OSes the same. */
std::deque<Job> s_warmJobs;
std::atomic<uint64_t> s_warmQueued{0}, s_warmDone{0}, s_untraced{0};
std::atomic<uint64_t> s_warmStartMs{0};
std::mutex s_jobLock;
std::condition_variable s_jobWake;

uint64_t hash_picture(int w, int h, const uint8_t* px) {
    uint64_t x = 1469598103934665603ull ^ (uint64_t)(w * 131 + h);
    const size_t n = (size_t)((w + 1) / 2) * (size_t)h;
    for (size_t i = 0; i < n; ++i) {
        x ^= px[i];
        x *= 1099511628211ull;
    }
    return x;
}

/* --- the tracer ---------------------------------------------------------------------------------------------- */

struct Img {
    int w = 0, h = 0;
    std::vector<float> v;
    float& at(int x, int y) { return v[(size_t)y * w + x]; }
    float at(int x, int y) const { return v[(size_t)y * w + x]; }
};

float cubic(float t) { /* Catmull-Rom (a = -0.5), the kernel Pillow's BICUBIC uses */
    t = std::fabs(t);
    if (t < 1.f) return (1.5f * t - 2.5f) * t * t + 1.f;
    if (t < 2.f) return ((-0.5f * t + 2.5f) * t - 4.f) * t + 2.f;
    return 0.f;
}

Img upsample(const Img& s, int up) {
    Img mid;
    mid.w = s.w * up, mid.h = s.h;
    mid.v.assign((size_t)mid.w * mid.h, 0.f);
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < mid.w; ++x) {
            const float sx = (x + 0.5f) / up - 0.5f;
            const int x0 = (int)std::floor(sx);
            float acc = 0, wsum = 0;
            for (int k = -1; k <= 2; ++k) {
                const int xx = std::clamp(x0 + k, 0, s.w - 1);
                const float wgt = cubic(sx - (x0 + k));
                acc += wgt * s.at(xx, y);
                wsum += wgt;
            }
            mid.at(x, y) = acc / wsum;
        }
    Img out;
    out.w = mid.w, out.h = s.h * up;
    out.v.assign((size_t)out.w * out.h, 0.f);
    for (int y = 0; y < out.h; ++y) {
        const float sy = (y + 0.5f) / up - 0.5f;
        const int y0 = (int)std::floor(sy);
        for (int x = 0; x < out.w; ++x) {
            float acc = 0, wsum = 0;
            for (int k = -1; k <= 2; ++k) {
                const int yy = std::clamp(y0 + k, 0, s.h - 1);
                const float wgt = cubic(sy - (y0 + k));
                acc += wgt * mid.at(x, yy);
                wsum += wgt;
            }
            out.at(x, y) = acc / wsum;
        }
    }
    return out;
}

Img blur(const Img& s, float sigma) {
    if (sigma <= 0.f) return s;
    const int r = (int)std::ceil(sigma * 3.f);
    std::vector<float> k(2 * r + 1);
    float ks = 0;
    for (int i = -r; i <= r; ++i) ks += (k[i + r] = std::exp(-(float)(i * i) / (2.f * sigma * sigma)));
    for (float& e : k) e /= ks;
    Img a = s, b = s;
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x) {
            float acc = 0;
            for (int i = -r; i <= r; ++i) acc += k[i + r] * s.at(std::clamp(x + i, 0, s.w - 1), y);
            a.at(x, y) = acc;
        }
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x) {
            float acc = 0;
            for (int i = -r; i <= r; ++i) acc += k[i + r] * a.at(x, std::clamp(y + i, 0, s.h - 1));
            b.at(x, y) = acc;
        }
    return b;
}

/* a binary shape (w x h, 0 / 1) shrunk by area to (w / f) x (h / f), f a whole factor: 0..1 coverage */
std::vector<float> shrink(const std::vector<uint8_t>& bin, int w, int h, int f, int& ow, int& oh) {
    ow = w / f, oh = h / f;
    std::vector<float> out((size_t)ow * oh, 0.f);
    for (int y = 0; y < oh; ++y)
        for (int x = 0; x < ow; ++x) {
            int s = 0;
            for (int yy = 0; yy < f; ++yy)
                for (int xx = 0; xx < f; ++xx) s += bin[(size_t)(y * f + yy) * w + x * f + xx];
            out[(size_t)y * ow + x] = (float)s / (float)(f * f);
        }
    return out;
}

/* (ink pieces 8-connected, holes = 4-connected background regions not touching the border), regions under minSize
 * pixels ignored */
std::pair<int, int> topology(const std::vector<uint8_t>& ink, int w, int h, int minSize) {
    std::vector<uint8_t> seen((size_t)w * h, 0);
    std::vector<int> stack;
    int pieces = 0, holes = 0;
    for (int i = 0; i < w * h; ++i) {
        if (seen[i]) continue;
        const uint8_t val = ink[i];
        int n = 0;
        bool border = false;
        stack.assign(1, i);
        seen[i] = 1;
        while (!stack.empty()) {
            const int p = stack.back();
            stack.pop_back();
            ++n;
            const int x = p % w, y = p / w;
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) border = true;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    if (!val && dx && dy) continue; /* background: 4-connected */
                    const int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    const int q = yy * w + xx;
                    if (!seen[q] && ink[q] == val) {
                        seen[q] = 1;
                        stack.push_back(q);
                    }
                }
        }
        if (n < minSize) continue;
        if (val) ++pieces;
        else if (!border) ++holes;
    }
    return {pieces, holes};
}

/* the trace of one picture (w x h, 4-bit, rows of ceil(w / 2) bytes) at scale S: (w S) x (h S) bytes */
std::vector<uint8_t> trace(int w, int h, const std::vector<uint8_t>& px4, int S) {
    Img src;
    src.w = w + 2 * kPad, src.h = h + 2 * kPad;
    src.v.assign((size_t)src.w * src.h, 0.f);
    const int stride = (w + 1) / 2;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint8_t b = px4[(size_t)y * stride + (x >> 1)];
            src.at(x + kPad, y + kPad) = (float)(((x & 1) ? (b & 0x0F) : (b >> 4)) * 17);
        }
    /* the structure to keep: the game picture's half-dark pixels */
    std::vector<uint8_t> want((size_t)src.w * src.h);
    for (size_t i = 0; i < want.size(); ++i) want[i] = src.v[i] >= 128.f;
    const auto wantTopo = topology(want, src.w, src.h, 1);

    const Img up = upsample(src, UP);
    struct Cand {
        int dist;
        double err;
        std::vector<uint8_t> bin;
    };
    std::vector<Cand> cands;
    Img blurred;
    float lastBlur = -1.f;
    for (int s = 0; s < kSettings; ++s) {
        if (kBlur[s] != lastBlur) {
            blurred = blur(up, kBlur[s] * UP);
            lastBlur = kBlur[s];
        }
        Cand c;
        c.bin.resize(blurred.v.size());
        for (size_t i = 0; i < c.bin.size(); ++i) c.bin[i] = blurred.v[i] >= (float)kCut[s];
        /* structure, on a 4x shrink cut at half */
        int w4, h4;
        const auto cov4 = shrink(c.bin, up.w, up.h, UP / 4, w4, h4);
        std::vector<uint8_t> ink4(cov4.size());
        for (size_t i = 0; i < ink4.size(); ++i) ink4[i] = cov4[i] >= 0.5f;
        const auto topo = topology(ink4, w4, h4, 4);
        c.dist = std::abs(topo.first - wantTopo.first) + std::abs(topo.second - wantTopo.second);
        /* the round trip: shrunk back to the game's size */
        int w1, h1;
        const auto cov1 = shrink(c.bin, up.w, up.h, UP, w1, h1);
        c.err = 0;
        for (size_t i = 0; i < cov1.size(); ++i) c.err += std::fabs(cov1[i] * 255.f - src.v[i]);
        cands.push_back(std::move(c));
    }
    int closest = 1 << 30;
    for (const auto& c : cands) closest = std::min(closest, c.dist);
    double least = 1e30;
    for (const auto& c : cands)
        if (c.dist == closest) least = std::min(least, c.err);
    const Cand* pick = nullptr;
    for (const auto& c : cands)
        if (c.dist == closest && c.err <= least * kRoundTripSlack + 1.0) {
            pick = &c;
            break;
        }
    /* shrink the kept shape to S x by area (fractional factors handled by sampling each output pixel's area at the
     * working detail), keeping only the glyph's own box (the pad is outside the quad) */
    std::vector<uint8_t> out((size_t)w * S * h * S, 0);
    const float f = (float)UP / (float)S;
    for (int y = 0; y < h * S; ++y) {
        const float y0 = (kPad * S + y) * f, y1 = y0 + f;
        for (int x = 0; x < w * S; ++x) {
            const float x0 = (kPad * S + x) * f, x1 = x0 + f;
            float acc = 0, area = 0;
            for (int yy = (int)y0; yy < (int)std::ceil(y1) && yy < up.h; ++yy) {
                const float wy = std::min(y1, (float)yy + 1) - std::max(y0, (float)yy);
                for (int xx = (int)x0; xx < (int)std::ceil(x1) && xx < up.w; ++xx) {
                    const float wx = std::min(x1, (float)xx + 1) - std::max(x0, (float)xx);
                    acc += wx * wy * pick->bin[(size_t)yy * up.w + xx];
                    area += wx * wy;
                }
            }
            out[(size_t)y * w * S + x] = (uint8_t)std::lround(area > 0 ? 255.f * acc / area : 0.f);
        }
    }
    return out;
}

/* --- the kanji tracer (region work piece 3, 2026-11-15; docs/port/font-renderer.md "The kanji") ---------
 * Why a second tracer: the one above was tuned on the Latin letters (bold, spaced). On the Japanese disc's 22-pixel
 * kanji, strokes often sit one pixel apart and the gap pixels between them are 30-45% grey, so its blur ran strokes
 * together: 810 of the 5,095 Japanese letters lost a stroke or a gap (tools/local/kanji_trace_check.py). snatcherLeo: "as
 * exact as we can ... go slow and line up as best you can", "we're going for the best possible result". This is the
 * "hybrid" chosen after four trials (the prototype, step for step: the scratch kanji_hybrid.py described in that doc):
 *   1. Decisions, after Kopf and Lischinski's depixelizing: ink = level 8 of 15 or more; where a 2 x 2 block is a
 *      checkerboard, one diagonal goes, by votes (the longer line kept whole; the sparser colour in the 8 x 8 window
 *      kept; a pixel's only link kept, weight 5); a tie keeps the gap.
 *   2. The field: the 16 levels enlarged 16x (Catmull-Rom), lightly blurred.
 *   3. Anchors from the decisions, their widths following each pixel's level: ink cores 0.5 x v/15 of a pixel wide,
 *      gap cores 0.8 x (1 - v/15), along every kept link (a link takes the narrower end); gap cores only where a gap
 *      can close (inside an enclosed space, or with ink within two pixels on opposite sides).
 *   4. The cut at the level covering as much ink as the game's picture; a final pass: softened, cut at half, the
 *      anchors applied again.
 *   5. Settings tried smoothest first; the first whose shape keeps the structure the decisions mean (strokes joined
 *      only by kept links, gaps only by kept gap links) wins. In the prototype all 5,095 letters kept it, 5,055 with
 *      the first settings (field blur 0.30, final pass 0.35).
 * *Updated 2026-11-16 (the kanji review; snatcherLeo approved both changes through Main):* before step 1, valley gaps
 * (valley_gaps: a grey pixel darker than the strokes on both sides is a gap thinner than a pixel, kept open); and,
 * for a small font's kanji still unreadable after that, the same character's picture from the disc's larger font,
 * traced and fitted into the small box (mush_points, fit_larger). Graded on all 5,095 letters with
 * tools/local/kanji_review/: D 135 -> 11, C 424 -> 264, A 3,220 -> 3,475, structure kept on all.
 * Platforms: plain C++ (the same everywhere). XD: the same tracer would serve XD's Japanese letters (likely: the
 * same engine's font format; a guess until XD's fonts are read). */
namespace cjk {

constexpr int KU = 16;
/* The kanji review's two changes on (valley gaps, the larger font's shape). A test switch only:
 * ORRE_KANJI_V1=1 turns both off for a run, so a test can show the tracer as it was before the review (2026-11-16)
 * beside it; read once in pc_text_atlas_init. Look only: it changes no layout and no random numbers. All OSes. */
bool g_kanjiReview = true;
/* The valley gaps (valley_gaps below; the kanji review, 2026-11-16), measured on all 5,095 letters: a gap pixel is 8-12
 * (out of 15) and at least 4 levels darker than both sides; a gap line grows through pixels 3 levels darker up to
 * level 11; at least 2 pixels in a line; held open at least 0.45 of a pixel (0.3 broke the 4x structure check on
 * about 100 letters: a narrower gap vanishes when shrunk) */
constexpr int kGapTop = 12, kGapDepth = 4, kGrowDepth = 3, kGrowTop = 11, kGrowSteps = 2, kGapRun = 2;
constexpr float kGapMin = 0.45f;

struct Glyph {
    int W = 0, H = 0;
    std::vector<int> lvl;     /* 0..15, padded */
    std::vector<uint8_t> B;   /* ink: level 8 or more */
    std::vector<uint64_t> cut; /* removed checkerboard diagonals (pair keys), sorted */
    std::vector<int> region;  /* background regions by the decisions' links */
    std::vector<uint8_t> outsideRegion;
    /* the valley gaps (valley_gaps below): each gap pixel's core radius at 16x (0 = not one) and its centre's offset
     * from the pixel centre (twin gap pixels put theirs against their shared edge); empty when the glyph has none or
     * after the fallback cleared them */
    std::vector<float> gapR, offX, offY;
    int at(int x, int y) const { return (x >= 0 && y >= 0 && x < W && y < H) ? B[y * W + x] : 0; }
    static uint64_t pair(int a, int b) { return a < b ? ((uint64_t)a << 32 | (uint32_t)b) : ((uint64_t)b << 32 | (uint32_t)a); }
    bool isCut(int a, int b) const { return std::binary_search(cut.begin(), cut.end(), pair(a, b)); }
};

const int kN8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

/* step 1: the decisions, in the prototype's order (each sees the cuts made before it) */
void decide(Glyph& g) {
    const int W = g.W, H = g.H;
    std::vector<uint64_t> cutList; /* kept unsorted while deciding (searched linearly: a glyph has few) */
    auto cutHas = [&](int a, int b) {
        const uint64_t k = Glyph::pair(a, b);
        return std::find(cutList.begin(), cutList.end(), k) != cutList.end();
    };
    auto links = [&](int x, int y, int* out) {
        int n = 0;
        for (const auto& d : kN8) {
            const int xx = x + d[0], yy = y + d[1];
            if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
            if (g.B[yy * W + xx] != g.B[y * W + x] || cutHas(y * W + x, yy * W + xx)) continue;
            out[n++] = yy * W + xx;
        }
        return n;
    };
    auto curve = [&](int p, int q) {
        int n = 1;
        const int starts[2][2] = {{p, q}, {q, p}};
        for (const auto& sp : starts) {
            int cur = sp[0], last = sp[1];
            std::vector<int> seen = {p, q};
            for (;;) {
                int ls[8];
                if (links(cur % W, cur / W, ls) != 2) break;
                const int nxt = ls[1] == last ? ls[0] : ls[1];
                if (std::find(seen.begin(), seen.end(), nxt) != seen.end()) break;
                seen.push_back(nxt);
                last = cur, cur = nxt;
                ++n;
            }
        }
        return n;
    };
    for (int y = 0; y + 1 < H; ++y)
        for (int x = 0; x + 1 < W; ++x) {
            const int a = g.B[y * W + x], b = g.B[y * W + x + 1], c = g.B[(y + 1) * W + x], d = g.B[(y + 1) * W + x + 1];
            if (!(a == d && b == c && a != b)) continue;
            const int i00 = y * W + x, i10 = y * W + x + 1, i01 = (y + 1) * W + x, i11 = (y + 1) * W + x + 1;
            const int inkP = a ? i00 : i10, inkQ = a ? i11 : i01, bgP = a ? i10 : i00, bgQ = a ? i01 : i11;
            double vote = curve(inkP, inkQ) - curve(bgP, bgQ);
            int ink = 0, bg = 0;
            for (int yy = y - 3; yy < y + 5; ++yy)
                for (int xx = x - 3; xx < x + 5; ++xx) (g.at(xx, yy) ? ink : bg)++;
            vote += (bg - ink) / 8.0;
            int tmp[8];
            if (links(inkP % W, inkP / W, tmp) == 1 || links(inkQ % W, inkQ / W, tmp) == 1) vote += 5;
            if (links(bgP % W, bgP / W, tmp) == 1 || links(bgQ % W, bgQ / W, tmp) == 1) vote -= 5;
            cutList.push_back(vote > 0 ? Glyph::pair(bgP, bgQ) : Glyph::pair(inkP, inkQ));
        }
    g.cut = cutList;
    std::sort(g.cut.begin(), g.cut.end());
}

/* joined in the decisions' sense: orthogonal neighbours of a colour, or a checkerboard diagonal not cut (a diagonal
 * with a shared orthogonal neighbour of the colour is reached through it) */
bool joined(const Glyph& g, int x, int y, int dx, int dy) {
    const int W = g.W, xx = x + dx, yy = y + dy;
    if (xx < 0 || yy < 0 || xx >= g.W || yy >= g.H) return false;
    const int col = g.B[y * W + x];
    if (g.B[yy * W + xx] != col) return false;
    if (dx && dy) {
        if (g.B[y * W + xx] == col || g.B[yy * W + x] == col) return false;
        if (g.isCut(y * W + x, yy * W + xx)) return false;
    }
    return true;
}

/* the structure the decisions mean: (ink pieces, enclosed background regions). Also keeps each pixel's region and
 * whether that region touches the border (g.region / g.outsideRegion), which the gap rule below reads: the prototype
 * found the background regions with the same links, so one search serves both. */
std::pair<int, int> intended(Glyph& g) {
    const int W = g.W, H = g.H;
    int pieces = 0, holes = 0, id = 0;
    g.region.assign((size_t)W * H, -1);
    g.outsideRegion.clear();
    std::vector<int> stack;
    for (int s = 0; s < W * H; ++s) {
        if (g.region[s] >= 0) continue;
        bool border = false;
        stack.assign(1, s);
        g.region[s] = id;
        while (!stack.empty()) {
            const int p = stack.back();
            stack.pop_back();
            const int x = p % W, y = p / W;
            if (x == 0 || y == 0 || x == W - 1 || y == H - 1) border = true;
            for (const auto& d : kN8) {
                if (!joined(g, x, y, d[0], d[1])) continue;
                const int q = (y + d[1]) * W + x + d[0];
                if (g.region[q] < 0) {
                    g.region[q] = id;
                    stack.push_back(q);
                }
            }
        }
        if (g.B[s]) ++pieces;
        else if (!border) ++holes;
        g.outsideRegion.push_back(border ? 1 : 0);
        ++id;
    }
    return {pieces, holes};
}

/* a background pixel where a gap can close: inside an enclosed space, or ink within two pixels on opposite sides */
bool between(const Glyph& g, int x, int y) {
    if (!g.outsideRegion[g.region[y * g.W + x]]) return true;
    const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (const auto& d : dirs) {
        const bool a = g.at(x + d[0], y + d[1]) || g.at(x + 2 * d[0], y + 2 * d[1]);
        const bool b = g.at(x - d[0], y - d[1]) || g.at(x - 2 * d[0], y - 2 * d[1]);
        if (a && b) return true;
    }
    return false;
}

/* Fill a disc / a thick segment into a 0/1 mask (pixels whose centre is within reach). A disc reaches half a pixel
 * past r: the prototype drew its cores with Pillow, whose ellipse counts the far edge of its box as inside, so they
 * came out about a pixel wider at 16x; drawn exactly r wide, 43 letters lost a one-pixel enclosed space at the 4x
 * check (verified 2026-11-15 on all 5,095: with the half pixel, all keep their structure, 5,091 pick the prototype's
 * setting, mean difference 0.77 of 255 a pixel; growing the links too, or moving the pixel centres, matched worse). */
void disc(std::vector<uint8_t>& m, int mw, int mh, float cx, float cy, float r) {
    r += 0.5f;
    const int x0 = std::max(0, (int)std::floor(cx - r)), x1 = std::min(mw - 1, (int)std::ceil(cx + r));
    const int y0 = std::max(0, (int)std::floor(cy - r)), y1 = std::min(mh - 1, (int)std::ceil(cy + r));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) m[(size_t)y * mw + x] = 1;
        }
}
void segment(std::vector<uint8_t>& m, int mw, int mh, float ax, float ay, float bx, float by, float half) {
    const int x0 = std::max(0, (int)std::floor(std::min(ax, bx) - half)), x1 = std::min(mw - 1, (int)std::ceil(std::max(ax, bx) + half));
    const int y0 = std::max(0, (int)std::floor(std::min(ay, by) - half)), y1 = std::min(mh - 1, (int)std::ceil(std::max(ay, by) + half));
    const float dx = bx - ax, dy = by - ay, L = dx * dx + dy * dy;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            float t = L > 0 ? ((px - ax) * dx + (py - ay) * dy) / L : 0.f;
            t = std::clamp(t, 0.f, 1.f);
            const float ex = px - (ax + t * dx), ey = py - (ay + t * dy);
            if (ex * ex + ey * ey <= half * half) m[(size_t)y * mw + x] = 1;
        }
}

/* Pillow's GaussianBlur, as the prototype used it (2026-11-15): three passes of an "extended" box blur (a box
 * of fractional radius, the edge pixels weighted by the fraction) along the rows, then three along the columns,
 * edges clamped; the box radius from the sigma by Pillow's own formula (_gaussian_blur_radius). Why: about 6
 * operations a pixel whatever the sigma, where the true Gaussian above takes 31-35 taps at these sizes, so a screen
 * of new Japanese characters turns sharp sooner (snatcherLeo saw the enlarged pictures for a moment); and it is the very blur
 * the prototype was checked with. */
float pil_box_radius(float sigma, int passes) {
    const float s2 = sigma * sigma / passes;
    const float L = std::sqrt(12.f * s2 + 1.f);
    const float l = std::floor((L - 1.f) / 2.f);
    float a = (2.f * l + 1.f) * (l * (l + 1.f) - 3.f * s2);
    a /= 6.f * (s2 - (l + 1.f) * (l + 1.f));
    return l + a;
}
void box_line(const float* in, float* out, int n, int stride, float r) {
    const int ri = (int)r;
    const float edge = r - (float)ri, ww = 1.f / (2.f * r + 1.f);
    auto at = [&](int i) { return in[(size_t)std::clamp(i, 0, n - 1) * stride]; };
    float acc = 0.f;
    for (int i = -ri; i <= ri; ++i) acc += at(i);
    for (int x = 0; x < n; ++x) {
        out[(size_t)x * stride] = (acc + edge * (at(x - ri - 1) + at(x + ri + 1))) * ww;
        acc += at(x + ri + 1) - at(x - ri);
    }
}
Img pil_blur(const Img& s, float sigma) {
    if (sigma <= 0.f) return s;
    const float r = pil_box_radius(sigma, 3);
    Img a = s, b = s;
    for (int p = 0; p < 3; ++p) {
        for (int y = 0; y < a.h; ++y) box_line(&a.v[(size_t)y * a.w], &b.v[(size_t)y * a.w], a.w, 1, r);
        std::swap(a, b);
    }
    for (int p = 0; p < 3; ++p) {
        for (int x = 0; x < a.w; ++x) box_line(&a.v[x], &b.v[x], a.h, a.w, r);
        std::swap(a, b);
    }
    return a;
}

/* The valley gaps (proposal 1 of the kanji review, 2026-11-16; snatcherLeo approved it through Main: "Yes, this is much better
 * idea"; docs/port/font-renderer.md "The kanji").
 * Why: the disc's dense kanji draw a gap thinner than a pixel as a grey pixel between two strokes, often at level 8-11.
 * The decisions call level 8 or more ink, so such a gap was never a gap and its strokes merged (着 in the 18-px font:
 * its 目's bars ran into one block; 署 and 雲 came out almost solid). A grey pixel darker than the strokes on both sides
 * of it is that gap: it becomes background, so the decisions keep it and a gap core holds it open.
 * The rule (each part measured on all 5,095 letters with the review's grades):
 *   - a pixel at level 8-12 at least 4 levels darker than its neighbours on both sides, left and right or above and
 *     below. A side looks one pixel further only past a twin of the same grey (within 2 levels: a gap two pixels
 *     wide), never across a darker pixel (that is background, and the stroke beyond belongs to something else: looking
 *     across it marked real strokes as gaps);
 *   - each gap line grows twice along itself through pixels 3 levels darker, up to level 11 (so a gap row isn't left
 *     as dashes where a few of its pixels are a little brighter);
 *   - kept only where 2 or more such pixels line up along it (a lone one is mostly the stair step of a slanted stroke);
 *   - each held open as wide as its own grey says (1 - level/15 of a pixel), at least kGapMin; two side by side across
 *     the gap make one gap at their shared edge, each giving its part (at least half of kGapMin).
 * Platforms: plain C++, the same everywhere. XD: the same rule should suit XD's kanji font (likely: the same kind of
 * anti-aliased 4-bit pictures); its constants would want the review's grading run on XD's disc first. */
void valley_gaps(Glyph& g) {
    std::vector<uint8_t> thin(g.B.size(), 0); /* 0, or 1 + the direction across the gap (od) */
    auto L = [&](int x, int y) { return (x < 0 || y < 0 || x >= g.W || y >= g.H) ? 0 : g.lvl[(size_t)y * g.W + x]; };
    const int od[2][2] = {{1, 0}, {0, 1}}; /* across a gap: left-right (a vertical gap), up-down (a horizontal one) */
    auto side = [&](int v, int x1, int y1, int x2, int y2) {
        const int l1 = L(x1, y1);
        if (l1 >= 8 && std::abs(l1 - v) <= 2) return L(x2, y2);
        return l1;
    };
    for (int y = 0; y < g.H; ++y)
        for (int x = 0; x < g.W; ++x) {
            const int v = L(x, y);
            if (v < 8 || v > kGapTop) continue;
            for (int k = 0; k < 2; ++k) {
                const int* d = od[k];
                const int a = side(v, x + d[0], y + d[1], x + 2 * d[0], y + 2 * d[1]);
                const int b = side(v, x - d[0], y - d[1], x - 2 * d[0], y - 2 * d[1]);
                if (a >= v + kGapDepth && b >= v + kGapDepth) thin[(size_t)y * g.W + x] = (uint8_t)(1 + k);
            }
        }
    for (int it = 0; it < kGrowSteps; ++it) {
        std::vector<uint8_t> add = thin;
        for (int y = 0; y < g.H; ++y)
            for (int x = 0; x < g.W; ++x) {
                const uint8_t t = thin[(size_t)y * g.W + x];
                if (!t) continue;
                const int* d = od[t - 1];
                const int ax = -d[1], ay = d[0]; /* along the gap line */
                for (int s = -1; s <= 1; s += 2) {
                    const int xx = x + s * ax, yy = y + s * ay;
                    const int v = L(xx, yy);
                    if (v < 8 || v > kGrowTop || thin[(size_t)yy * g.W + xx]) continue;
                    const int a = side(v, xx + d[0], yy + d[1], xx + 2 * d[0], yy + 2 * d[1]);
                    const int b = side(v, xx - d[0], yy - d[1], xx - 2 * d[0], yy - 2 * d[1]);
                    if (a >= v + kGrowDepth && b >= v + kGrowDepth) add[(size_t)yy * g.W + xx] = t;
                }
            }
        thin.swap(add);
    }
    {
        std::vector<uint8_t> keep(thin.size(), 0);
        for (int y = 0; y < g.H; ++y)
            for (int x = 0; x < g.W; ++x) {
                const uint8_t t = thin[(size_t)y * g.W + x];
                if (!t) continue;
                const int* d = od[t - 1];
                const int ax = -d[1], ay = d[0];
                int run = 1;
                for (int s = -1; s <= 1; s += 2)
                    for (int k = 1;; ++k) {
                        const int xx = x + s * k * ax, yy = y + s * k * ay;
                        if (xx < 0 || yy < 0 || xx >= g.W || yy >= g.H || thin[(size_t)yy * g.W + xx] != t) break;
                        ++run;
                    }
                keep[(size_t)y * g.W + x] = run >= kGapRun;
            }
        for (size_t i = 0; i < thin.size(); ++i) thin[i] = keep[i] ? thin[i] : 0;
    }
    for (size_t i = 0; i < thin.size(); ++i)
        if (thin[i]) g.B[i] = 0;
    g.gapR.assign(thin.size(), 0.f);
    g.offX.assign(thin.size(), 0.f);
    g.offY.assign(thin.size(), 0.f);
    for (int y = 0; y < g.H; ++y)
        for (int x = 0; x < g.W; ++x) {
            const size_t i = (size_t)y * g.W + x;
            const uint8_t t = thin[i];
            if (!t) continue;
            const int* d = od[t - 1];
            const float own = 1.f - g.lvl[i] / 15.f;
            int twin = 0;
            for (int s = -1; s <= 1; s += 2) {
                const int xx = x + s * d[0], yy = y + s * d[1];
                if (xx >= 0 && yy >= 0 && xx < g.W && yy < g.H && thin[(size_t)yy * g.W + xx] == t) twin = s;
            }
            if (!twin) {
                g.gapR[i] = 0.5f * KU * std::max(own, kGapMin);
            } else {
                const float r = 0.5f * KU * std::max(own, 0.5f * kGapMin);
                g.gapR[i] = r;
                g.offX[i] = twin * d[0] * (0.5f * KU - r);
                g.offY[i] = twin * d[1] * (0.5f * KU - r);
            }
        }
}

/* one setting: the shape at 16x (0/1) */
std::vector<uint8_t> shape(const Glyph& g, const Img& field0, float blurPx, float postPx, bool betweenOnly) {
    const int W = g.W, H = g.H, mw = W * KU, mh = H * KU;
    const Img field = blurPx > 0 ? pil_blur(field0, blurPx * KU) : field0;
    std::vector<uint8_t> inkc((size_t)mw * mh, 0), bgc((size_t)mw * mh, 0);
    const bool hasGaps = !g.gapR.empty();
    auto rad = [&](int x, int y) {
        const float v = g.lvl[y * W + x] / 15.f;
        if (hasGaps && g.gapR[y * W + x] > 0.f) return g.gapR[y * W + x]; /* a valley gap: its own measured width */
        if (!g.B[y * W + x] && g.lvl[y * W + x] >= 8) /* a valley gap after the fallback: the full minimum width */
            return 0.5f * KU * std::max(0.8f * (1.f - v), kGapMin);
        return 0.5f * KU * (g.B[y * W + x] ? 0.5f * v : 0.8f * (1.f - v));
    };
    /* a core's centre: the pixel's centre, moved for twin gap pixels (valley_gaps) */
    auto centre = [&](int x, int y, float& cx, float& cy) {
        cx = (x + 0.5f) * KU, cy = (y + 0.5f) * KU;
        if (hasGaps) cx += g.offX[y * W + x], cy += g.offY[y * W + x];
    };
    const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {-1, 1}};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const bool ink = g.B[y * W + x];
            if (!ink && betweenOnly && !between(g, x, y)) continue;
            std::vector<uint8_t>& m = ink ? inkc : bgc;
            float cx, cy;
            centre(x, y, cx, cy);
            const float r = rad(x, y);
            if (r >= 0.5f) disc(m, mw, mh, cx, cy, r);
            for (const auto& d : dirs) {
                const int xx = x + d[0], yy = y + d[1];
                if (xx < 0 || yy < 0 || xx >= W || yy >= H || g.B[yy * W + xx] != g.B[y * W + x]) continue;
                if (d[0] && d[1] && g.isCut(y * W + x, yy * W + xx)) continue;
                if (!ink && betweenOnly && !between(g, xx, yy)) continue;
                const int lw = (int)std::lround(2.f * std::min(r, rad(xx, yy)));
                float ex, ey;
                centre(xx, yy, ex, ey);
                if (lw >= 1) segment(m, mw, mh, cx, cy, ex, ey, lw * 0.5f);
            }
        }
    /* anchors, then the cut covering as much ink as the game's picture. A valley gap's pixels take no part in the
     * count (their ink sits beside their gap already): counted, the narrower ink there made the cut drop and the whole
     * letter thicken, a faint haze around every stroke (found in the 2026-11-16 review) */
    std::vector<int> q((size_t)mw * mh);
    int hist[256] = {0};
    for (size_t i = 0; i < q.size(); ++i) {
        float v = field.v[i];
        if (inkc[i]) v = 255.f;
        if (bgc[i]) v = 0.f;
        q[i] = std::clamp((int)std::lround(v), 0, 255);
        if (hasGaps && g.gapR[(i / mw) / KU * W + (i % mw) / KU] > 0.f) continue;
        ++hist[q[i]];
    }
    double ink = 0;
    for (size_t i = 0; i < g.lvl.size(); ++i)
        if (!(hasGaps && g.gapR[i] > 0.f)) ink += g.lvl[i] * 17;
    ink = ink / 255.0 * KU * KU;
    int th = 128;
    double total = 0;
    for (int v = 255; v > 0; --v) {
        if (total + hist[v] >= ink) {
            th = v;
            break;
        }
        total += hist[v];
    }
    th = std::max(1, th);
    std::vector<uint8_t> bin((size_t)mw * mh);
    for (size_t i = 0; i < bin.size(); ++i) bin[i] = q[i] >= th;
    if (postPx > 0) {
        Img s;
        s.w = mw, s.h = mh;
        s.v.resize(bin.size());
        for (size_t i = 0; i < bin.size(); ++i) s.v[i] = bin[i] ? 255.f : 0.f;
        const Img sb = pil_blur(s, postPx * KU);
        for (size_t i = 0; i < bin.size(); ++i) {
            float v = sb.v[i];
            if (inkc[i]) v = 255.f;
            if (bgc[i]) v = 0.f;
            bin[i] = v >= 128.f;
        }
    }
    return bin;
}

/* One picture's shape at 16x (0/1, the glyph and its kPad border: mw x mh), the setting that won and whether the
 * structure was kept */
struct Shape16 {
    std::vector<uint8_t> bin;
    int mw = 0, mh = 0, step = 0;
    bool kept = false;
};

/* the levels of a 4-bit picture (rows of ceil(w / 2) bytes, high nibble first) */
inline int level4(const uint8_t* px4, int w, int x, int y) {
    const uint8_t b = px4[(size_t)y * ((w + 1) / 2) + (x >> 1)];
    return (x & 1) ? (b & 0x0F) : (b >> 4);
}

Shape16 trace16(int w, int h, const uint8_t* px4) {
    Glyph g;
    g.W = w + 2 * kPad, g.H = h + 2 * kPad;
    g.lvl.assign((size_t)g.W * g.H, 0);
    g.B.assign((size_t)g.W * g.H, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int v = level4(px4, w, x, y);
            g.lvl[(size_t)(y + kPad) * g.W + x + kPad] = v;
            g.B[(size_t)(y + kPad) * g.W + x + kPad] = v >= 8;
        }
    if (g_kanjiReview) valley_gaps(g);
    decide(g);
    const auto want = intended(g);
    Img src;
    src.w = g.W, src.h = g.H;
    src.v.resize((size_t)g.W * g.H);
    for (size_t i = 0; i < src.v.size(); ++i) src.v[i] = (float)(g.lvl[i] * 17);
    const Img field = upsample(src, KU);
    struct Step {
        float blur, post;
        bool betweenOnly;
    };
    const Step steps[] = {{0.30f, 0.35f, true}, {0.30f, 0.20f, true}, {0.30f, 0.f, true}, {0.15f, 0.f, true},
                          {0.15f, 0.f, false}};
    const int mw = g.W * KU, mh = g.H * KU;
    std::vector<uint8_t> bin;
    int used = 0;
    bool ok = false;
    for (const Step& st : steps) {
        bin = shape(g, field, st.blur, st.post, st.betweenOnly);
        int w4, h4;
        const auto cov4 = shrink(bin, mw, mh, KU / 4, w4, h4);
        std::vector<uint8_t> ink4(cov4.size());
        for (size_t i = 0; i < ink4.size(); ++i) ink4[i] = cov4[i] >= 0.5f;
        if (topology(ink4, w4, h4, 4) == want) {
            ok = true;
            break;
        }
        if (used + 1 < (int)(sizeof(steps) / sizeof(steps[0]))) ++used;
    }
    /* No setting kept the structure with every valley gap at its measured width: the chain once more with each at
     * the full minimum (kGapMin; one letter of the 5,095 needed it in the review) */
    if (!ok && !g.gapR.empty()) {
        g.gapR.clear(), g.offX.clear(), g.offY.clear();
        used = 0;
        for (const Step& st : steps) {
            bin = shape(g, field, st.blur, st.post, st.betweenOnly);
            int w4, h4;
            const auto cov4 = shrink(bin, mw, mh, KU / 4, w4, h4);
            std::vector<uint8_t> ink4(cov4.size());
            for (size_t i = 0; i < ink4.size(); ++i) ink4[i] = cov4[i] >= 0.5f;
            if (topology(ink4, w4, h4, 4) == want) {
                ok = true;
                break;
            }
            if (used + 1 < (int)(sizeof(steps) / sizeof(steps[0]))) ++used;
        }
    }
    Shape16 s;
    s.bin = std::move(bin), s.mw = mw, s.mh = mh, s.step = used, s.kept = ok;
    return s;
}

/* a 16x shape reduced to S x by area over the glyph's own box (the pad is outside the quad), as trace() */
std::vector<uint8_t> reduce(const Shape16& s, int w, int h, int S) {
    std::vector<uint8_t> out((size_t)w * S * h * S, 0);
    const float f = (float)KU / (float)S;
    for (int y = 0; y < h * S; ++y) {
        const float y0 = (kPad * S + y) * f, y1 = y0 + f;
        for (int x = 0; x < w * S; ++x) {
            const float x0 = (kPad * S + x) * f, x1 = x0 + f;
            float acc = 0, area = 0;
            for (int yy = (int)y0; yy < (int)std::ceil(y1) && yy < s.mh; ++yy) {
                const float wy = std::min(y1, (float)yy + 1) - std::max(y0, (float)yy);
                for (int xx = (int)x0; xx < (int)std::ceil(x1) && xx < s.mw; ++xx) {
                    const float wx = std::min(x1, (float)xx + 1) - std::max(x0, (float)xx);
                    acc += wx * wy * s.bin[(size_t)yy * s.mw + xx];
                    area += wx * wy;
                }
            }
            out[(size_t)y * w * S + x] = (uint8_t)std::lround(area > 0 ? 255.f * acc / area : 0.f);
        }
    }
    return out;
}

/* --- proposal 2 of the kanji review (2026-11-16; snatcherLeo approved it through Main): the larger font's shape ----------
 * Why: the 18-px font draws some dense kanji as grey blocks that no rule can read (違, 襲, 霧, 麗: the strokes are
 * closer than a pixel everywhere, so the picture holds no gaps to find). The same disc draws the same character in its
 * 22-px font with room for every stroke. For such a letter the 22-px picture is traced instead and fitted into the
 * smaller picture's ink box: the letter reads like itself, at the cost of no longer following the small picture's
 * pixels (snatcherLeo's call: legibility over faithfulness, for these letters only).
 * Which letters (computed per picture, never a list, so any disc works the same): a kanji whose picture is a Japanese
 * disc font's own picture of that character, where another font of the disc has the character at a larger cell, and
 * whose proposal-1 trace still scores as a C or D by the review's disc-side measures (mush_points >= 3). The
 * review's full grade also compares with a reference font, which the game doesn't have; this rule, checked against it
 * on the 914 18-px kanji, picks 38 of its 56 C / D letters and none of its A or B ones (2026-11-16, the scratch
 * harness). Platforms: plain C++, all the same. XD: its own Japanese disc would be read the same way; the typical
 * values below are Colosseum's 18-px font's and would be measured again (guess: similar). */
constexpr float kTypRoundTrip = 9.34f; /* the 18-px kanji's median round trip (0-255 a pixel) with today's tracer */
constexpr float kTypWidth = 2.13f;     /* their median stroke width in game pixels (2 x area / outline at 4x) */
constexpr int kMushPoints = 3;

/* The review's disc-side grade points for a trace (tools/local/kanji_review/grade.py, parts b, c and d): the round
 * trip against the picture, the grey gaps painted solid, the stroke width; each against the typical 18-px kanji */
int mush_points(const uint8_t* px4, int w, int h, const Shape16& s) {
    const std::vector<uint8_t> t4 = reduce(s, w, h, 4);
    std::vector<float> cov((size_t)w * h);
    double rt = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int acc = 0;
            for (int yy = 0; yy < 4; ++yy)
                for (int xx = 0; xx < 4; ++xx) acc += t4[(size_t)(y * 4 + yy) * w * 4 + x * 4 + xx];
            cov[(size_t)y * w + x] = acc / 16.f / 255.f;
            rt += std::fabs(cov[(size_t)y * w + x] * 255.f - level4(px4, w, x, y) * 17);
        }
    rt /= std::max(1, w * h);
    /* grey valleys painted solid: a pixel at 3-12 with a pixel of 10 or more, and 4 levels above it, within two on
     * both sides (in any of four directions), that the trace covers 80% or more */
    auto L = [&](int x, int y) { return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : level4(px4, w, x, y); };
    const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    int filled = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int v = L(x, y);
            if (v < 3 || v > 12 || cov[(size_t)y * w + x] < 0.8f) continue;
            for (const auto& d : dirs) {
                const int a = std::max(L(x + d[0], y + d[1]), L(x + 2 * d[0], y + 2 * d[1]));
                const int b = std::max(L(x - d[0], y - d[1]), L(x - 2 * d[0], y - 2 * d[1]));
                if (a >= v + 4 && b >= v + 4 && a >= 10 && b >= 10) {
                    ++filled;
                    break;
                }
            }
        }
    /* stroke width: 2 x area / outline length of the 4x trace cut at half (the border counts as outside) */
    long area = 0, edge = 0;
    for (int y = 0; y < h * 4; ++y)
        for (int x = 0; x < w * 4; ++x) {
            if (t4[(size_t)y * w * 4 + x] < 128) continue;
            ++area;
            const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : d4) {
                const int xx = x + d[0], yy = y + d[1];
                if (xx < 0 || yy < 0 || xx >= w * 4 || yy >= h * 4 || t4[(size_t)yy * w * 4 + xx] < 128) ++edge;
            }
        }
    const float width = edge ? 2.f * area / edge / 4.f : 0.f;
    const float rtr = (float)rt / kTypRoundTrip, wr = width / kTypWidth;
    const int pb = rtr >= 1.7f ? 2 : rtr >= 1.35f ? 1 : 0;
    const int pc = filled >= 35 ? 3 : filled >= 22 ? 2 : filled >= 12 ? 1 : 0;
    const int pd = wr >= 1.4f ? 2 : wr >= 1.2f ? 1 : 0;
    return pb + pc + pd;
}

/* the larger font's 16x shape fitted into this picture's ink box (levels 4 and up, a quarter pixel in from each side:
 * those edge pixels are only partly inked), by area, cut at half */
Shape16 fit_larger(const uint8_t* px4, int w, int h, const Shape16& big) {
    Shape16 out;
    out.mw = (w + 2 * kPad) * KU, out.mh = (h + 2 * kPad) * KU;
    out.bin.assign((size_t)out.mw * out.mh, 0);
    out.step = big.step, out.kept = big.kept;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (level4(px4, w, x, y) >= 4) x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
    int bx0 = big.mw, by0 = big.mh, bx1 = -1, by1 = -1;
    for (int y = 0; y < big.mh; ++y)
        for (int x = 0; x < big.mw; ++x)
            if (big.bin[(size_t)y * big.mw + x]) bx0 = std::min(bx0, x), by0 = std::min(by0, y), bx1 = std::max(bx1, x), by1 = std::max(by1, y);
    if (x1 < 0 || bx1 < 0) return out;
    const float tx0 = (x0 + kPad + 0.25f) * KU, ty0 = (y0 + kPad + 0.25f) * KU;
    const float tx1 = (x1 + 1 + kPad - 0.25f) * KU, ty1 = (y1 + 1 + kPad - 0.25f) * KU;
    const float sx = (bx1 + 1 - bx0) / (tx1 - tx0), sy = (by1 + 1 - by0) / (ty1 - ty0);
    for (int Y = (int)std::floor(ty0); Y < (int)std::ceil(ty1); ++Y)
        for (int X = (int)std::floor(tx0); X < (int)std::ceil(tx1); ++X) {
            /* this pixel's footprint in the larger shape, clipped to the target box */
            const float u0 = bx0 + (std::max((float)X, tx0) - tx0) * sx, u1 = bx0 + (std::min((float)X + 1, tx1) - tx0) * sx;
            const float v0 = by0 + (std::max((float)Y, ty0) - ty0) * sy, v1 = by0 + (std::min((float)Y + 1, ty1) - ty0) * sy;
            float acc = 0, area = 0;
            for (int yy = (int)std::floor(v0); yy < (int)std::ceil(v1); ++yy) {
                const float wy = std::min(v1, (float)yy + 1) - std::max(v0, (float)yy);
                for (int xx = (int)std::floor(u0); xx < (int)std::ceil(u1); ++xx) {
                    const float wx = std::min(u1, (float)xx + 1) - std::max(u0, (float)xx);
                    if (xx >= 0 && yy >= 0 && xx < big.mw && yy < big.mh) acc += wx * wy * big.bin[(size_t)yy * big.mw + xx];
                    area += wx * wy;
                }
            }
            /* the part of the pixel inside the target box is the shape's; the rest stays empty */
            const float inside = (std::min((float)X + 1, tx1) - std::max((float)X, tx0)) *
                                 (std::min((float)Y + 1, ty1) - std::max((float)Y, ty0));
            if (area > 0 && X >= 0 && Y >= 0 && X < out.mw && Y < out.mh)
                out.bin[(size_t)Y * out.mw + X] = acc / area * inside >= 0.5f;
        }
    return out;
}

/* --- the rare kanji drawn from Noto (2026-11-16; snatcherLeo: "We could mix in other sources, I wouldn't mind, for those
 * rare ones"; the project's rule, which snatcherLeo accepted) --------------------------------------------------------------------
 * Why: after both review changes, 11 letters still graded D and about 260 C, nearly all dense 22-px kanji whose disc
 * picture is a grey block: strokes closer than a pixel everywhere, so no rule can read them, and (unlike the 18-px
 * ones) no larger font on the disc to borrow from. For the ones a player can only meet by typing them in a name, the
 * letter is drawn from Noto Sans CJK JP instead, fitted to the game's own ink box and matched to its ink, so it reads
 * like itself at the game's size and place. Letters the Japanese game's own text uses (最 事 機 撃 爆 愛 熱 暴...) stay on
 * the game's picture: a Noto letter mid-sentence would stand out (the project's line), and those are the ones the review
 * looked at hardest.
 * Which letters (computed per picture, never a list of letters; the same on any disc):
 *   - a kanji whose picture is a Japanese disc font's own picture of that character (pc_region_font_picture, matched
 *     pixel for pixel), not already drawn from the disc's larger font (proposal 2);
 *   - its valley-gap trace scores the review's disc-side points (mush_points, against that font's own typical kanji:
 *     the 22-px and 18-px fonts each have theirs, kTyp22 / kTyp18 below) of 3 or more AND the Japanese game's text
 *     never uses it (pc_region_text_uses, read from the Japanese disc's 9,414 messages when the game starts: no list);
 *     or 5 or more (a D-level picture) whether used or not;
 *   - while the disc's text is still loading, "used" is assumed, so the game's picture is kept (rare: the read takes a
 *     few seconds at start-up).
 * Measured with the review's harness on all 5,095 letters (tools/local/kanji_review/, 2026-11-16): docs/port/
 * font-renderer.md "The kanji", "Rare kanji from Noto". Platforms: plain C++ and FreeType, the same everywhere (Noto
 * Sans CJK is shipped in Fonts/ on every OS). XD: the same rule would serve XD's Japanese font; its typical values are
 * Colosseum's and would be measured again (guess: similar). */
bool g_notoRare = true; /* ORRE_KANJI_NOTO=0 turns it off for a run (a test switch; look only) */
struct Typical {
    int cell;
    float roundTrip, width;
};
/* each font's typical kanji with the tracer before the review (the review's baseline, measured on 2,969 22-px and 914
 * 18-px kanji: the median round trip, 0-255 a pixel, and the median stroke width in game pixels) */
constexpr Typical kTyp[] = {{22, 7.603f, 1.886f}, {18, 9.34f, 2.13f}};
constexpr int kNotoPointsUnused = 3, kNotoPointsAlways = 5;

/* the review's disc-side points (mush_points' measures) against a given font's typical kanji */
int disc_points(const uint8_t* px4, int w, int h, const Shape16& s, int cell, float* rtOut = nullptr) {
    const Typical* t = nullptr;
    for (const Typical& k : kTyp)
        if (k.cell == cell) t = &k;
    if (t == nullptr) return -1;
    const std::vector<uint8_t> t4 = reduce(s, w, h, 4);
    std::vector<float> cov((size_t)w * h);
    double rt = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int acc = 0;
            for (int yy = 0; yy < 4; ++yy)
                for (int xx = 0; xx < 4; ++xx) acc += t4[(size_t)(y * 4 + yy) * w * 4 + x * 4 + xx];
            cov[(size_t)y * w + x] = acc / 16.f / 255.f;
            rt += std::fabs(cov[(size_t)y * w + x] * 255.f - level4(px4, w, x, y) * 17);
        }
    rt /= std::max(1, w * h);
    if (rtOut != nullptr) *rtOut = (float)rt;
    auto L = [&](int x, int y) { return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : level4(px4, w, x, y); };
    const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    int filled = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int v = L(x, y);
            if (v < 3 || v > 12 || cov[(size_t)y * w + x] < 0.8f) continue;
            for (const auto& d : dirs) {
                const int a = std::max(L(x + d[0], y + d[1]), L(x + 2 * d[0], y + 2 * d[1]));
                const int b = std::max(L(x - d[0], y - d[1]), L(x - 2 * d[0], y - 2 * d[1]));
                if (a >= v + 4 && b >= v + 4 && a >= 10 && b >= 10) {
                    ++filled;
                    break;
                }
            }
        }
    long area = 0, edge = 0;
    for (int y = 0; y < h * 4; ++y)
        for (int x = 0; x < w * 4; ++x) {
            if (t4[(size_t)y * w * 4 + x] < 128) continue;
            ++area;
            const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : d4) {
                const int xx = x + d[0], yy = y + d[1];
                if (xx < 0 || yy < 0 || xx >= w * 4 || yy >= h * 4 || t4[(size_t)yy * w * 4 + xx] < 128) ++edge;
            }
        }
    const float width = edge ? 2.f * area / edge / 4.f : 0.f;
    const float rtr = (float)rt / t->roundTrip, wr = width / t->width;
    const int pb = rtr >= 1.7f ? 2 : rtr >= 1.35f ? 1 : 0;
    const int pc = filled >= 35 ? 3 : filled >= 22 ? 2 : filled >= 12 ? 1 : 0;
    const int pd = wr >= 1.4f ? 2 : wr >= 1.2f ? 1 : 0;
    return pb + pc + pd;
}

/* Noto Sans CJK JP Bold, opened once for the tracer's worker threads (their own FreeType library: FreeType faces aren't
 * shared across threads, and part D's live on the game thread), behind a lock (a rare letter at a time) */
std::mutex s_notoLock;
FT_Library s_notoLib = nullptr;
FT_Face s_notoFace = nullptr;
std::vector<uint8_t> s_notoFile;
bool s_notoTried = false;
FT_Face noto_face_locked() {
    if (s_notoTried) return s_notoFace;
    s_notoTried = true;
    char dir[1024];
    const std::string path = std::string(pc_font_dir(dir, sizeof dir)) + "NotoSansCJK-Bold.ttc";
    size_t len = 0;
    void* data = SDL_LoadFile(path.c_str(), &len);
    if (data == nullptr || FT_Init_FreeType(&s_notoLib) != 0) {
        if (data != nullptr) SDL_free(data);
        std::printf("[kanji] Noto Sans CJK not found (%s): rare kanji keep the game's picture\n", path.c_str());
        return nullptr;
    }
    s_notoFile.assign((const uint8_t*)data, (const uint8_t*)data + len);
    SDL_free(data);
    if (FT_New_Memory_Face(s_notoLib, s_notoFile.data(), (FT_Long)s_notoFile.size(), 0 /* JP */, &s_notoFace) != 0)
        s_notoFace = nullptr;
    return s_notoFace;
}

/* The character from Noto Sans CJK JP Bold as a 16x shape fitted into the picture's ink box (as fit_larger: levels 4
 * and up, a quarter pixel in from each side), its weight matched to the picture's own ink: FreeType's emboldening (a
 * negative one thins) chosen by halving so the shape's area equals the picture's ink (the sum of its levels, a level 15
 * pixel counting whole). Why the picture's own ink: the disc's grey levels are the original font's coverage, so their
 * sum is that letter's real amount of ink even where the strokes blur together; matching it keeps the letter as dark
 * as its neighbours. (Noto Bold unchanged measured 1.06x the 22-px font's ink and 0.90x the 18-px's on 250 kanji each:
 * close, but per letter it varies.) The emboldening is kept within -0.45 and +0.6 of a game pixel, so a picture with
 * odd ink can't turn the letter into a blob or a hairline. bold: the emboldening used, in game pixels (for the review). */
bool noto_shape(uint16_t code, const uint8_t* px4, int w, int h, Shape16& out, float* boldOut, float* inkOut) {
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    double ink = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int v = level4(px4, w, x, y);
            ink += v / 15.0;
            if (v >= 4) x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
        }
    if (x1 < 0) return false;
    out = Shape16{};
    out.mw = (w + 2 * kPad) * KU, out.mh = (h + 2 * kPad) * KU;
    const double tx0 = (x0 + kPad + 0.25) * KU, ty0 = (y0 + kPad + 0.25) * KU;
    const double tx1 = (x1 + 1 + kPad - 0.25) * KU, ty1 = (y1 + 1 + kPad - 0.25) * KU;
    const double targetArea = ink * KU * KU;
    std::lock_guard<std::mutex> lock(s_notoLock);
    FT_Face face = noto_face_locked();
    if (face == nullptr) return false;
    const FT_UInt gi = FT_Get_Char_Index(face, code);
    if (gi == 0 || FT_Set_Pixel_Sizes(face, 0, (FT_UInt)std::lround(ty1 - ty0)) != 0 ||
        FT_Load_Glyph(face, gi, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0 || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
        return false;
    const FT_Outline& src = face->glyph->outline;
    /* one try: the outline emboldened by e (26.6, at this size), fitted to the box, rendered and cut at half */
    std::vector<uint8_t> gray((size_t)out.mw * out.mh);
    auto render = [&](double e, std::vector<uint8_t>& bin) -> long {
        FT_Outline o;
        if (FT_Outline_New(s_notoLib, src.n_points, src.n_contours, &o) != 0) return -1;
        FT_Outline_Copy(&src, &o);
        if (std::fabs(e) > 0.01) FT_Outline_Embolden(&o, (FT_Pos)std::lround(e));
        FT_BBox cb;
        FT_Outline_Get_CBox(&o, &cb);
        if (cb.xMax <= cb.xMin || cb.yMax <= cb.yMin) {
            FT_Outline_Done(s_notoLib, &o);
            return -1;
        }
        /* fit: the outline's box onto the target box (y up in the outline, rows down in the bitmap) */
        const double sx = (tx1 - tx0) * 64.0 / (double)(cb.xMax - cb.xMin);
        const double sy = (ty1 - ty0) * 64.0 / (double)(cb.yMax - cb.yMin);
        FT_Outline_Translate(&o, -cb.xMin, -cb.yMin);
        FT_Matrix m = {(FT_Fixed)std::lround(sx * 65536.0), 0, 0, (FT_Fixed)std::lround(sy * 65536.0)};
        FT_Outline_Transform(&o, &m);
        FT_Outline_Translate(&o, (FT_Pos)std::lround(tx0 * 64.0), (FT_Pos)std::lround((out.mh - ty1) * 64.0));
        std::fill(gray.begin(), gray.end(), 0);
        FT_Bitmap bm{};
        bm.rows = (unsigned)out.mh, bm.width = (unsigned)out.mw, bm.pitch = out.mw;
        bm.buffer = gray.data(), bm.num_grays = 256, bm.pixel_mode = FT_PIXEL_MODE_GRAY;
        const FT_Error err = FT_Outline_Get_Bitmap(s_notoLib, &o, &bm);
        FT_Outline_Done(s_notoLib, &o);
        if (err != 0) return -1;
        /* FreeType fills the bitmap from its bottom row up (row 0 of the buffer is the top: a positive pitch) */
        bin.assign(gray.size(), 0);
        long area = 0;
        for (size_t i = 0; i < gray.size(); ++i) area += bin[i] = gray[i] >= 128;
        return area;
    };
    const double px26 = 64.0 * KU; /* one game pixel in 26.6 units at the 16x size */
    double lo = -0.45 * px26, hi = 0.6 * px26;
    std::vector<uint8_t> bin;
    const long natural = render(0, bin);
    if (natural <= 0) return false;
    if (inkOut != nullptr) *inkOut = (float)(natural / std::max(1.0, targetArea));
    double e = 0;
    for (int it = 0; it < 12; ++it) {
        e = 0.5 * (lo + hi);
        const long a = render(e, bin);
        if (a < 0) return false;
        if ((double)a < targetArea) lo = e;
        else hi = e;
    }
    e = 0.5 * (lo + hi);
    if (render(e, bin) <= 0) return false;
    out.bin = std::move(bin);
    out.step = 0, out.kept = true;
    if (boldOut != nullptr) *boldOut = (float)(e / px26);
    return true;
}

} // namespace cjk

/* What the kanji tracer did with a picture (for the review's harness; the game ignores it) */
struct CjkReport {
    int step = 0;          /* the setting that won (0 = the smoothest; 4 = the last) */
    bool kept = false;     /* the structure the decisions mean was kept */
    bool borrowed = false; /* proposal 2: the larger font's shape was used */
    int mush = -1;         /* the disc-side grade points of the proposal-1 trace (-1: not measured: no larger font) */
    int cell = 0;          /* the cell height of the Japanese disc font the picture is from (0: none matched) */
    /* the rare-kanji rule (2026-11-16): the disc-side points against the picture's own font's typical (-1: not
     * measured), whether the Japanese text uses the letter (1 / 0, -1 unknown), whether Noto drew it, the emboldening
     * it took (game pixels) and Noto's own ink against the picture's before the matching */
    int points = -1;
    int used = -1;
    bool noto = false;
    float notoBold = 0.f, notoInk = 0.f;
};

/* the kanji tracer's trace of one picture at scale S: (w S) x (h S) bytes, as trace(). code: the character (0:
 * unknown, no larger-font check). Used by the game's workers and by the review's harness
 * (tools/local/kanji_review/grade_harness.cpp, which includes this file). */
std::vector<uint8_t> trace_cjk(int w, int h, const std::vector<uint8_t>& px4, int S, uint16_t code = 0,
                               CjkReport* rep = nullptr) {
    using namespace cjk;
    Shape16 s = trace16(w, h, px4.data());
    CjkReport r;
    r.step = s.step, r.kept = s.kept;
    /* proposal 2: only for a kanji that is a Japanese disc font's own picture, with a larger font of that disc
     * holding the same character */
    if (g_kanjiReview && code >= 0x3400 && code <= 0x9FFF) {
        int srcCell = 0, bigCell = 0, bw = 0, bh = 0;
        const uint8_t* bigPx = nullptr;
        for (uint16_t id = 0; id <= 4; ++id) {
            int ch = 0, gw = 0, gh = 0;
            const uint8_t* p = pc_region_font_picture(id, code, &ch, &gw, &gh);
            if (p == nullptr) continue;
            if (gw == w && gh == h && std::memcmp(p, px4.data(), (size_t)((w + 1) / 2) * h) == 0) srcCell = ch;
            if (ch > bigCell) bigCell = ch, bigPx = p, bw = gw, bh = gh;
        }
        r.cell = srcCell;
        if (srcCell > 0 && bigCell > srcCell && bigPx != nullptr) {
            r.mush = mush_points(px4.data(), w, h, s);
            if (r.mush >= kMushPoints) {
                const Shape16 big = trace16(bw, bh, bigPx);
                s = fit_larger(px4.data(), w, h, big);
                r.borrowed = true, r.step = s.step, r.kept = s.kept;
            }
        }
        /* the rare kanji from Noto (above, at noto_shape): a disc picture still unreadable, and a letter the Japanese
         * game's text never uses (or a D-level picture) */
        if (g_notoRare && srcCell > 0 && !r.borrowed) {
            r.points = disc_points(px4.data(), w, h, s, srcCell);
            if (r.points >= kNotoPointsUnused) {
                r.used = pc_region_text_uses(code);
                if (r.points >= kNotoPointsAlways || r.used == 0) {
                    Shape16 n;
                    if (noto_shape(code, px4.data(), w, h, n, &r.notoBold, &r.notoInk)) {
                        s = std::move(n);
                        r.noto = true, r.step = 0, r.kept = true;
                    }
                }
            }
        }
    }
    if (rep != nullptr) *rep = r;
    return reduce(s, w, h, S);
}

/* The character whose picture the game copies next (OrreHook_GlyphCode, set just before _msgMakeTexture on the game
 * thread; read and cleared by pc_text_atlas_glyph on the same thread, so a picture copied without the hook never
 * inherits an old code). Japanese characters (CJK signs and kana U+3000-30FF, kanji U+3400-9FFF, full-width forms
 * U+FF00-FFEF) take the kanji tracer; everything else keeps the Latin tracer snatcherLeo signed off (2026-11-13). The codes
 * are the game's own (UCS-2: the US and JP fonts both index by Unicode; docs/engine/fonts.md). */
uint16_t s_nextCode = 0;
bool is_japanese(uint16_t c) {
    return (c >= 0x3000 && c <= 0x30FF) || (c >= 0x3400 && c <= 0x9FFF) || (c >= 0xFF00 && c <= 0xFFEF);
}
void on_glyph_code(uint16_t code) { s_nextCode = code; }
/* a Japanese picture is kept apart from the same pixels traced the Latin way (a plain dash, say) */
constexpr uint64_t kCjkSalt = 0x9E3779B97F4A7C15ull;

/* ORRE_KANJI_DUMP=<folder> (a test switch, the kanji review's in-game check, 2026-11-16): every Japanese trace the
 * workers finish is written there as two 8-bit PGM pictures, the game's picture (levels x 17) and the trace at the
 * run's scale, and logged with what the tracer did; so a run shows exactly what the game drew and can be compared,
 * picture for picture, with the review's harness. Off by default; look only. PGM is written by hand (no image library
 * needed); fopen on a UTF-8 path works on every OS for the plain names used. */
std::string s_kanjiDump;
std::mutex s_kanjiDumpLock;

void dump_pgm(const std::string& path, int w, int h, const uint8_t* px) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return;
    std::fprintf(f, "P5\n%d %d\n255\n", w, h);
    std::fwrite(px, 1, (size_t)w * h, f);
    std::fclose(f);
}

void worker() {
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    for (;;) {
        Job job;
        bool warm = false;
        {
            std::unique_lock<std::mutex> lock(s_jobLock);
            s_jobWake.wait(lock, [] { return !s_jobs.empty() || !s_warmJobs.empty(); });
            if (!s_jobs.empty()) { /* letters on screen first; the warm-up only when none waits (round 12) */
                job = std::move(s_jobs.front());
                s_jobs.pop_front();
            } else {
                job = std::move(s_warmJobs.front());
                s_warmJobs.pop_front();
                warm = true;
            }
            job.out->warmQueued.store(false, std::memory_order_relaxed);
        }
        CjkReport rep;
        job.out->pixels = job.cjk ? trace_cjk(job.w, job.h, job.src, s_scale, job.code, &rep)
                                  : trace(job.w, job.h, job.src, s_scale);
        if (job.cjk && !s_kanjiDump.empty()) {
            char name[96];
            std::snprintf(name, sizeof name, "U%04X_%dx%d_%016llx", job.code, job.w, job.h,
                          (unsigned long long)job.key);
            std::vector<uint8_t> src((size_t)job.w * job.h);
            for (int y = 0; y < job.h; ++y)
                for (int x = 0; x < job.w; ++x) src[(size_t)y * job.w + x] = (uint8_t)(cjk::level4(job.src.data(), job.w, x, y) * 17);
            std::lock_guard<std::mutex> lock(s_kanjiDumpLock);
            dump_pgm(s_kanjiDump + "/" + name + "_disc.pgm", job.w, job.h, src.data());
            dump_pgm(s_kanjiDump + "/" + name + "_trace" + std::to_string(s_scale) + "x.pgm", job.w * s_scale,
                     job.h * s_scale, job.out->pixels.data());
            std::printf("[kanji] U+%04X %dx%d at %dx: setting %d, structure %s%s%s, Japanese font cell %d, mush %d, "
                        "points %d, used by the text %d (%s)\n",
                        job.code, job.w, job.h, s_scale, rep.step + 1, rep.kept ? "kept" : "NOT kept",
                        rep.borrowed ? ", the larger font's shape" : "", rep.noto ? ", drawn from Noto (rare kanji)" : "",
                        rep.cell, rep.mush, rep.points, rep.used, name);
        }
        job.out->ready.store(true, std::memory_order_release);
        if (warm) {
            const uint64_t done = s_warmDone.fetch_add(1, std::memory_order_relaxed) + 1;
            bool empty;
            {
                std::lock_guard<std::mutex> lock(s_jobLock);
                empty = s_warmJobs.empty();
            }
            /* the warm-up's end, once per batch (a later font opening starts another): how long it took */
            if (empty && done == s_warmQueued.load(std::memory_order_relaxed)) {
                std::printf("[text_atlas] warm-up: %llu letters traced ahead (%.1f s since the first was queued)\n",
                            (unsigned long long)done,
                            (double)(SDL_GetTicks() - s_warmStartMs.load(std::memory_order_relaxed)) / 1000.0);
            }
        }
    }
}

Atlas* atlas_for(const void* image) {
    for (Atlas& a : s_atlas)
        if (a.gameImage == image) return &a;
    return nullptr;
}

void take_dirty(const void* gameData, uint32_t rect[4]) {
    std::lock_guard<std::mutex> lock(s_atlasLock);
    Atlas* a = atlas_for(gameData);
    if (a == nullptr) {
        rect[0] = rect[1] = rect[2] = rect[3] = 0;
        return;
    }
    std::memcpy(rect, a->dirty, sizeof(a->dirty));
    a->dirty[0] = a->dirty[1] = a->dirty[2] = a->dirty[3] = 0;
}

} // namespace

/* After the game creates its two atlas textures (gs_msg.c): pick S from the drawing size, make the copies and hand
 * them to Aurora. S is fixed for the run (a later resolution change keeps it; noted for a later version). */
extern "C" void pc_text_atlas_init(const void* image0, const void* image1) {
    const char* off = SDL_getenv("ORRE_TEXT_HIRES");
    if (off != nullptr && off[0] == '0') {
        std::printf("[text_atlas] ORRE_TEXT_HIRES=0: the game's own atlas this run\n");
        return;
    }
    const AuroraWindowSize size = aurora_get_window_size();
    /* *Changed 2026-11-16 (the project's build helper, the text-softness check):* S covers the larger of the two on-screen
     * scales, the width's (render width / 640) and the height's (render height / 480). With the 4:3 picture (bars, the
     * default) they are equal. With settings.ini stretch = 1 the game's 640 x 480 is stretched to the window, so a
     * 16:9 window shows each game pixel 4/3 times wider than tall; S from the height alone left the letters magnified
     * that much across by the GPU's linear filter (measured on a 2560 x 1440 render: no hard edge across any letter,
     * text edges 1.4-2.2 px wide against 0.8 px for the box frame). Now the copy is never magnified, only reduced
     * (the gentler of the two). All OSes. */
    const double scaleX = (double)size.fb_width / 640.0, scaleY = (double)size.fb_height / 480.0;
    int S = (int)std::ceil(std::max(scaleX, scaleY) - 1e-6);
    S = std::clamp(S, 1, 8);
    if (S <= 1) {
        std::printf("[text_atlas] drawing at %u lines: the game's own atlas (no sharper copy needed)\n",
                    (unsigned)size.fb_height);
        return;
    }
    s_scale = S;
    if (const char* v1 = SDL_getenv("ORRE_KANJI_V1"); v1 != nullptr && v1[0] == '1') {
        cjk::g_kanjiReview = false;
        std::printf("[text_atlas] ORRE_KANJI_V1=1: the kanji tracer as before the review (no valley gaps, no larger "
                    "font's shape)\n");
    }
    if (const char* noto = SDL_getenv("ORRE_KANJI_NOTO"); noto != nullptr && noto[0] == '0') {
        /* a test switch (2026-11-16): the rare kanji keep the game's picture, for a before / after beside the default */
        cjk::g_notoRare = false;
        std::printf("[text_atlas] ORRE_KANJI_NOTO=0: rare kanji keep the game's picture (not drawn from Noto)\n");
    }
    if (const char* dump = SDL_getenv("ORRE_KANJI_DUMP"); dump != nullptr && dump[0] != '\0') {
        s_kanjiDump = dump;
        std::printf("[text_atlas] ORRE_KANJI_DUMP: each Japanese trace written to %s\n", dump);
    }
    const void* images[2] = {image0, image1};
    for (int i = 0; i < 2; ++i) {
        Atlas& a = s_atlas[i];
        a.gameImage = images[i];
        a.size = 512 * S;
        a.pixels.assign((size_t)a.size * a.size, 0);
        aurora_orre_hires_texture_register(a.gameImage, a.pixels.data(), (uint32_t)a.size, (uint32_t)a.size,
                                           take_dirty);
    }
    /* *Updated 2026-11-15 (snatcherLeo saw Japanese text enlarged for a moment before turning sharp):* a quarter of the
     * CPU's threads (2 to 6), still at low priority, so a screen of new Japanese characters (many at once, where
     * English reuses the same letters) is shared out; the game's own threads keep priority. All OSes (SDL). */
    const int workers = std::clamp(SDL_GetNumLogicalCPUCores() / 4, 2, 6);
    for (int i = 0; i < workers; ++i) std::thread(worker).detach();
    OrreHook_GlyphCode = on_glyph_code;
    std::printf("[text_atlas] the game's letters traced sharp at %dx (drawing at %u lines; a game pixel %.2f x %.2f "
                "screen pixels; two %dx%d atlas copies; %d tracing threads)\n",
                S, (unsigned)size.fb_height, scaleX, scaleY, 512 * S, 512 * S, workers);
}

/* Part D (pc_font_render.cpp): the drawing scale, and a ready sharp picture for a character the renderer drew from an
 * open font (drawn straight from the font at the scale, so it is never traced from its small picture) */
extern "C" int pc_text_atlas_scale(void) { return s_scale; }

extern "C" void pc_text_atlas_provide(int width, int height, const uint8_t* pixels4, const uint8_t* hi, int scale) {
    if (scale != s_scale || scale <= 1 || width <= 0 || height <= 0) return;
    auto t = std::make_shared<Trace>();
    t->pixels.assign(hi, hi + (size_t)width * scale * height * scale);
    t->provided = true;
    t->ready.store(true, std::memory_order_release);
    std::lock_guard<std::mutex> lock(s_traceLock);
    s_traces[hash_picture(width, height, pixels4)] = std::move(t);
}

/* _msgMakeTexture (gs_msg.c) copied a picture (w x h, 4-bit) into the game atlas `image` at (atlasX, atlasY): copy
 * its trace (or, until ready, the picture enlarged) into our copy at S times the place. */
extern "C" void pc_text_atlas_glyph(const void* image, int atlasX, int atlasY, int width, int height,
                                   const uint8_t* pixels) {
    if (s_scale <= 1 || width <= 0 || height <= 0 || pixels == nullptr) return;
    Atlas* a = atlas_for(image);
    if (a == nullptr) return;
    const int S = s_scale;
    const bool cjk = is_japanese(s_nextCode);
    const uint16_t code = s_nextCode;
    s_nextCode = 0;
    const uint64_t plain = hash_picture(width, height, pixels);
    const uint64_t key = cjk ? plain ^ kCjkSalt : plain;
    std::shared_ptr<Trace> t;
    {
        std::lock_guard<std::mutex> lock(s_traceLock);
        auto it = s_traces.find(key);
        if (it == s_traces.end() && cjk) { /* part D's open-font picture of a Japanese character is filed plain */
            auto p = s_traces.find(plain);
            if (p != s_traces.end() && p->second->provided) it = p;
        }
        if (it == s_traces.end()) {
            t = std::make_shared<Trace>();
            s_traces.emplace(key, t);
            Job job;
            job.key = key, job.w = width, job.h = height, job.cjk = cjk, job.code = code, job.out = t;
            job.src.assign(pixels, pixels + (size_t)((width + 1) / 2) * height);
            {
                std::lock_guard<std::mutex> jl(s_jobLock);
                s_jobs.push_back(std::move(job));
            }
            s_jobWake.notify_one();
        } else {
            t = it->second;
            /* round 12: a letter still waiting in the warm-up queue is needed now: move it up front */
            if (t->warmQueued.load(std::memory_order_relaxed)) {
                std::lock_guard<std::mutex> jl(s_jobLock);
                for (auto w = s_warmJobs.begin(); w != s_warmJobs.end(); ++w) {
                    if (w->out == t) {
                        s_jobs.push_back(std::move(*w));
                        s_warmJobs.erase(w);
                        s_warmQueued.fetch_sub(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        }
    }
    if (!t->ready.load(std::memory_order_acquire)) {
        /* round 12: drawn before its trace (the first sighting's blocky frame); counted, and listed with
         * ORRE_TEXT_TRACE_LOG=1 (test runs: which frames still show it) */
        s_untraced.fetch_add(1, std::memory_order_relaxed);
        static int s_logOn = -1;
        if (s_logOn < 0) {
            const char* v = SDL_getenv("ORRE_TEXT_TRACE_LOG");
            s_logOn = v != nullptr && v[0] == '1';
        }
        if (s_logOn) {
            std::printf("[text_atlas] letter U+%04X (%dx%d) drawn before its trace at game frame %u\n", code, width,
                        height, (unsigned)pc_vi_heartbeat);
        }
    }
    const int x0 = atlasX * S, y0 = atlasY * S, ww = width * S, hh = height * S;
    if (x0 < 0 || y0 < 0 || x0 + ww > a->size || y0 + hh > a->size) return;
    /* *Fixed 2026-11-14 (snatcherLeo's hand check: thin vertical slivers beside letters in every help line and on the party
     * screen, at high resolution only):* the game clears a border around each letter before writing it (one row above
     * and below, two columns left and right: _msgMakeTexture's first loop, gs_msg.c), so the edge of the letter's quad,
     * which the texture filter samples one texel past, always meets empty texture. This copy wrote only the letter's
     * own rectangle, so that border kept slivers of letters written there earlier, and they showed at the letters'
     * edges once enough text had cycled through the atlas (a fresh atlas has nothing old in it, which is why short test
     * runs looked clean; at 640 x 480 the copy is off). The same border is cleared here, at S times its size. */
    const int bx0 = std::max(0, (atlasX - 2) * S), by0 = std::max(0, (atlasY - 1) * S);
    const int bx1 = std::min(a->size, (atlasX + width + 2) * S), by1 = std::min(a->size, (atlasY + height + 1) * S);
    for (int y = by0; y < by1; ++y) std::memset(&a->pixels[(size_t)y * a->size + bx0], 0, (size_t)(bx1 - bx0));
    const bool ready = t->ready.load(std::memory_order_acquire);
    const int stride4 = (width + 1) / 2;
    for (int y = 0; y < hh; ++y) {
        uint8_t* row = &a->pixels[(size_t)(y0 + y) * a->size + x0];
        if (ready) {
            std::memcpy(row, &t->pixels[(size_t)y * ww], (size_t)ww);
        } else { /* until the trace is ready: the game's picture enlarged without smoothing */
            const int sy = y / S;
            for (int x = 0; x < ww; ++x) {
                const int sx = x / S;
                const uint8_t b = pixels[(size_t)sy * stride4 + (sx >> 1)];
                row[x] = (uint8_t)(((sx & 1) ? (b & 0x0F) : (b >> 4)) * 17);
            }
        }
    }
    std::lock_guard<std::mutex> lock(s_atlasLock);
    uint32_t* d = a->dirty;
    /* the changed area: the letter and its cleared border */
    if (d[2] <= d[0] || d[3] <= d[1]) {
        d[0] = (uint32_t)bx0, d[1] = (uint32_t)by0, d[2] = (uint32_t)bx1, d[3] = (uint32_t)by1;
    } else {
        d[0] = std::min(d[0], (uint32_t)bx0);
        d[1] = std::min(d[1], (uint32_t)by0);
        d[2] = std::max(d[2], (uint32_t)bx1);
        d[3] = std::max(d[3], (uint32_t)by1);
    }
}

/* Round 12 (2026-11-17; the s_warmJobs note above): one letter's picture, exactly as the game will copy it into its
 * atlas (the renderer's own picture, or the disc's when not replaced: pc_font_render.cpp), traced ahead of its first
 * sighting. Queued behind every letter already on screen; nothing is drawn or uploaded here. Called on the game thread
 * at a font's opening; the copy of the picture is the only cost there. All OSes the same. */
extern "C" void pc_text_atlas_warm(int width, int height, const uint8_t* pixels, uint16_t code) {
    if (s_scale <= 1 || width <= 0 || height <= 0 || pixels == nullptr) return;
    static int s_on = -1;
    if (s_on < 0) {
        const char* v = SDL_getenv("ORRE_TEXT_WARM");
        s_on = !(v != nullptr && v[0] == '0');
        if (!s_on) std::printf("[text_atlas] ORRE_TEXT_WARM=0: letters are traced at their first sighting only\n");
    }
    if (!s_on) return;
    const bool cjk = is_japanese(code);
    const uint64_t plain = hash_picture(width, height, pixels);
    const uint64_t key = cjk ? plain ^ kCjkSalt : plain;
    std::lock_guard<std::mutex> lock(s_traceLock);
    if (s_traces.find(key) != s_traces.end() || (cjk && s_traces.find(plain) != s_traces.end())) return;
    auto t = std::make_shared<Trace>();
    t->warmQueued.store(true, std::memory_order_relaxed);
    s_traces.emplace(key, t);
    Job job;
    job.key = key, job.w = width, job.h = height, job.cjk = cjk, job.code = code, job.out = t;
    job.src.assign(pixels, pixels + (size_t)((width + 1) / 2) * height);
    {
        std::lock_guard<std::mutex> jl(s_jobLock);
        if (s_warmJobs.empty() && s_warmQueued.load(std::memory_order_relaxed) == s_warmDone.load(std::memory_order_relaxed))
            s_warmStartMs.store(SDL_GetTicks(), std::memory_order_relaxed);
        s_warmJobs.push_back(std::move(job));
        s_warmQueued.fetch_add(1, std::memory_order_relaxed);
    }
    s_jobWake.notify_one();
}

/* round 12: letters drawn before their trace so far (the first-sighting frames; pc_smooth.c's 10 s line could show it) */
extern "C" unsigned long long pc_text_atlas_untraced(void) { return s_untraced.load(std::memory_order_relaxed); }
