// bolt.cpp: the bolt screen effect (WZX screen effect kind 1, boltEffectCreate), Thunder's lightning (2026-11-19,
// release step 4: the move sheets showed the game's bolts and Studio's none).
// Why: Studio's Moves preview should show every move as the game does (snatcherLeo: "this needs to be a faithful and very
// reliable tool"). The game builds each bolt once as a random forked line, then makes it wander a little every frame,
// reveals it from the top down over the first half of its time, fades it over the second half, and draws every segment
// as a camera-facing strip. This file follows the game's functions one by one (src/game/effect/effect_visual.c,
// verified from the code): boltStartEffect (the timing and the build), _boltSubdivide (the forked line), _boltUpdate
// (the wander and the fade), _boltRender / _boltDrawStrip (the reveal and the strips). The random numbers are Studio's
// own: the game draws from its one shared sequence (GSrandf; fn_800E0BA0 = two of them added, minus 1), so the shapes
// are the game's kind of shape, not its exact ones.
// Platforms: plain C++20, every OS (no platform code here).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "internal.hpp"

namespace {
struct V3 { float x, y, z; };
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float mag(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
V3 norm(V3 a) {
    const float m = mag(a);
    return m > 0 ? a * (1.0f / m) : a;
}
constexpr int kPoints = 200;  // (0xC8: a bolt's point array)
// a point of a bolt (ElectronPoint): its place, the point before it (-1: the bolt's start), its width at the start
// and the end of its segment
struct Point { V3 p; int prev; float w0, w1; };
struct Strike {
    Point pts[kPoints];
    int count = 0;
    V3 start{}, end{};        // +0x12C0 the start (0, height, 0); +0x12CC the trunk's last point
    uint8_t rgba[4] = {};     // +0x12D8, faded in its second half
    int from = 0, to = 0;     // +0x12DC / +0x12DE: its frames (ticks)
    bool on = false;          // +0x12E0
};
}  // namespace

struct OrreBolt {
    OrreWzxBolt p{};
    std::vector<Strike> strikes;
    int frame = 0;  // +0x48, ticks since the start
    uint32_t seed = 1;
    // Studio's random numbers (the game: GSrandf, its shared sequence): uniform in [0, 1)
    float randf() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (float)(seed >> 8) / 16777216.0f;
    }
    float randpm() { return randf() + randf() - 1.0f; }  // fn_800E0BA0: -1..1, peaked at 0
    // the point index of `at` in a strike (-1: its start)
    V3 point(const Strike& s, int at) const { return at < 0 ? s.start : s.pts[at].p; }
    // _boltSubdivide: from `origin`, points by a random step plus the step until one falls below the stop height (a
    // fork: a quarter of its origin's height, or by chance, more likely the deeper), then forks along the new stretch
    void subdivide(Strike& s, int origin, int depth, float weight) {
        int previous = origin;
        const int first = s.count;
        const float stop = depth ? 0.25f * point(s, origin).y : 0.0f;
        const float spawn = 1.0f / (float)(depth + 1);
        for (int i = first; i < kPoints; ++i) {
            Point& c = s.pts[i];
            c.prev = previous;
            const V3 from = point(s, previous);
            previous = i;
            c.p = V3{p.spread[0] * randpm() + p.step[0], p.spread[1] * randpm() + p.step[1], p.spread[2] * randpm() + p.step[2]} + from;
            ++s.count;
            if (c.p.y < stop || spawn < randf()) {
                if (depth == 0) s.end = c.p;
                break;
            }
        }
        if (depth >= p.depth) return;
        const float chance = p.branch * (float)(p.depth - depth) / (float)std::max(1, p.depth);
        const int count = s.count;
        const float decay = depth ? weight / (float)std::max(1, count - first) : 0.0f;
        for (int j = first; j < count; ++j) {
            s.pts[j].w0 = weight;
            weight = s.pts[j].w1 = weight - decay;
            if (chance > randf()) subdivide(s, s.pts[j].prev, depth + 1, 0.5f * weight);
        }
    }
};

extern "C" OrreBolt* orre_bolt_create(const OrreWzxBolt* params, uint32_t seed) {
    if (!params || params->count <= 0) return nullptr;
    auto* b = new OrreBolt;
    b->p = *params;
    b->seed = seed ? seed : 1;
    // boltStartEffect: the strikes' times (u16 arithmetic, as the game's), then each one built
    const int n = params->count, dur = params->frames & 0xFFFF;
    int span, step;
    if (n > 1) {
        span = (uint16_t)(dur / (n - 1));
        step = (uint16_t)((dur - span) / n);
    } else {
        step = 0;
        span = dur;
    }
    b->strikes.resize((size_t)n);
    for (int i = 0; i < n; ++i) {
        Strike& s = b->strikes[(size_t)i];
        s.count = 0;
        s.start = {0, params->height, 0};
        b->subdivide(s, -1, 0, params->width);
        std::copy(params->rgba, params->rgba + 4, s.rgba);
        s.from = (uint16_t)(i * step);
        s.to = (uint16_t)(s.from + span);
    }
    b->frame = 0;
    return b;
}

extern "C" void orre_bolt_free(OrreBolt* b) { delete b; }

// _boltUpdate: while the effect runs, each strike in its time wanders (every point by jitter x randpm, which adds up)
// and, past its middle, fades linearly; the others are off. Then the clock moves on
extern "C" int orre_bolt_update(OrreBolt* b, int ticks) {
    if (!b || b->frame >= (b->p.frames & 0xFFFF)) return 0;
    for (Strike& s : b->strikes) {
        if (b->frame >= s.from && b->frame < s.to) {
            for (int k = 0; k < s.count; ++k)
                s.pts[k].p = s.pts[k].p + V3{b->p.jitter[0] * b->randpm(), b->p.jitter[1] * b->randpm(), b->p.jitter[2] * b->randpm()};
            if (b->frame > ((s.to + s.from) >> 1)) {
                const float fade = (float)(s.to - b->frame) / (float)std::max(1, (s.to - s.from) >> 1);
                for (int c = 0; c < 4; ++c) s.rgba[c] = (uint8_t)((float)b->p.rgba[c] * fade);
            }
            s.on = true;
        } else {
            s.on = false;
        }
    }
    b->frame = (uint16_t)(b->frame + ticks);
    return 1;
}

// _boltRender / _boltDrawStrip: for each strike that is on, the segments whose start is above the reveal's line (from
// the top down over the first half of its time) as strips facing the camera: the start's corners at half the segment's
// start width, the end's at half its end width, texture across (u) and along (v). The strike is placed so the trunk's
// last point lands on the owner: world = owner - end x scale + local x scale. The camera's direction is taken from the
// segment's local middle to the camera's world place, as the game's code does (its quirk, kept)
extern "C" int orre_bolt_quads(const OrreBolt* b, const float camera[3], const float owner[3], float* xyz, float* uv, uint8_t* rgba,
                               int max) {
    if (!b || !camera || !owner) return 0;
    const V3 cam{camera[0], camera[1], camera[2]}, sc{b->p.scale[0], b->p.scale[1], b->p.scale[2]};
    int n = 0;
    for (const Strike& s : b->strikes) {
        if (!s.on) continue;
        const int current = (uint16_t)(b->frame - s.from);
        const int half = ((uint16_t)(s.to - s.from)) >> 1;
        const float line = current < half ? b->p.height * (1.0f - (float)current / (float)std::max(1, half)) : 0.0f;
        const V3 at{owner[0] - s.end.x * sc.x, owner[1] - s.end.y * sc.y, owner[2] - s.end.z * sc.z};
        for (int k = 0; k < s.count && n < max; ++k) {
            const Point& pt = s.pts[k];
            const V3 prev = b->point(s, pt.prev);
            if (!(prev.y > line)) continue;
            const V3 dir = norm(pt.p - prev);
            const V3 to_cam = norm(cam - (pt.p + prev) * 0.5f);
            V3 nrm = cross(to_cam, dir);
            if (mag(nrm) > 0.001f) nrm = norm(nrm);
            const V3 d0 = nrm * (0.5f * pt.w0), d1 = nrm * (0.5f * pt.w1);
            const V3 corner[4] = {prev + d0, prev - d0, pt.p - d1, pt.p + d1};
            static const float kUV[8] = {0, 0, 1, 0, 1, 1, 0, 1};
            for (int c = 0; c < 4; ++c) {
                if (xyz) {
                    xyz[(n * 4 + c) * 3 + 0] = at.x + corner[c].x * sc.x;
                    xyz[(n * 4 + c) * 3 + 1] = at.y + corner[c].y * sc.y;
                    xyz[(n * 4 + c) * 3 + 2] = at.z + corner[c].z * sc.z;
                }
                if (uv) { uv[(n * 4 + c) * 2] = kUV[2 * c]; uv[(n * 4 + c) * 2 + 1] = kUV[2 * c + 1]; }
                if (rgba) std::copy(s.rgba, s.rgba + 4, rgba + (n * 4 + c) * 4);
            }
            ++n;
        }
    }
    return n;
}
