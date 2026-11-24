// leaves.cpp: the leaves screen effect (WZX screen effect kind 5, leaffxEffectCreate): Razor Leaf's, Petal Dance's,
// Magical Leaf's, Leaf Blade's and Feather Dance's flying copies of one small model (2026-11-20, release step 4).
// Why: Studio's Moves preview should show every move as the game does (snatcherLeo: "this needs to be a faithful and very
// reliable tool"). The game clones the effect's model up to 32 times; each copy flies a chain of cubic Bezier curves
// from the owner's attach point, turning from one random orientation to the next along each curve. Each new curve
// starts where the last ended and leaves along the same line (its first control point mirrors the last one), so the
// path is smooth. The copies race at first (twice the speed at the start, slowing to the base speed by the fade's
// start), then all fade together until the effect's time is up; a copy that dips below the ground is dropped.
// This file follows the game's functions one by one (src/game/effect/effect_visual.c, all matched, verified from the
// code): leaffxStartEffect (the count, at most 32), _leaffxGenerateLeafData (a copy's first curve), _leaffxUpdate (the
// speed, the fade, the drop) and _leaffxMoveLeaf (the next curve); GSbezierCalculateVector (gs_math_bezier.c) for the
// curve, the SDK's quaternion slerp and quaternion-to-matrix for the turn. The random numbers are Studio's own: the
// game draws from its one shared sequence (GSrandf; fn_800E0BA0 = two of them added, minus 1), so the flights are the
// game's kind of flight, not its exact ones.
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
V3 norm(V3 a) {
    const float m = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return m > 0 ? a * (1.0f / m) : a;
}
struct Q { float x, y, z, w; };
constexpr float kTwoPi = 6.2831855f, kPi = 3.1415927f, kHalfPi = 1.5707964f;  // lbl_8047D180 / 184 / 188
// a copy (the game's 0x5C-byte record): the curve's points P0 +0x00, P3 +0x0C, P1 +0x18, P2 +0x24; the turn from q0
// (+0x30) to q1 (+0x40); its size (+0x50); its place on the curve (+0x54); its model (+0x58: here, alive)
struct Leaf {
    V3 p0{}, p3{}, p1{}, p2{};
    Q q0{}, q1{};
    float scale = 1, t = 0;
    bool alive = false;
};
// fn_800E0718: the turn by `angle` about the unit `axis`
Q axis_angle(V3 a, float angle) {
    const float s = std::sin(0.5f * angle);
    return {a.x * s, a.y * s, a.z * s, std::cos(0.5f * angle)};
}
// GSquatSlerp (the SDK's C_QUATSlerp: the shorter way round, straight blending when nearly equal)
Q slerp(Q p, Q q, float t) {
    float c = p.x * q.x + p.y * q.y + p.z * q.z + p.w * q.w, sign = 1;
    if (c < 0) { c = -c; sign = -1; }
    float a, b;
    if (c <= 1.0f - 1e-5f) {
        const float th = std::acos(c), s = std::sin(th);
        a = std::sin((1 - t) * th) / s;
        b = std::sin(t * th) / s * sign;
    } else {
        a = 1 - t;
        b = t * sign;
    }
    return {a * p.x + b * q.x, a * p.y + b * q.y, a * p.z + b * q.z, a * p.w + b * q.w};
}
// GSbezierCalculateVector: the cubic curve at t (clamped to 0..1)
V3 bezier(const Leaf& l, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1 - t;
    return l.p0 * (u * u * u) + l.p1 * (3 * t * u * u) + l.p2 * (3 * t * t * u) + l.p3 * (t * t * t);
}
}  // namespace

struct OrreLeaves {
    OrreWzxLeaves p{};
    V3 origin{};
    float effect_scale = 1;
    std::vector<Leaf> leaves;
    int frame = 0;      // +0x3C, ticks since the start
    float alpha = 1;    // the shared materials' alpha (every copy shares its model's)
    uint32_t seed = 1;
    float randf() {     // Studio's random numbers (the game: GSrandf): uniform in [0, 1)
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (float)(seed >> 8) / 16777216.0f;
    }
    float randpm() { return randf() + randf() - 1.0f; }  // fn_800E0BA0: -1..1, peaked at 0
    // a random direction: yaw 2 pi x randf, pitch (from straight up) `pitch` x randf
    V3 dir(float pitch) {
        const float yaw = kTwoPi * randf(), pt = pitch * randf();
        return {std::sin(pt) * std::cos(yaw), std::cos(pt), std::sin(pt) * std::sin(yaw)};
    }
    Q turn() {
        const V3 a = dir(kPi);
        return axis_angle(a, kTwoPi * randf());
    }
    // _leaffxGenerateLeafData (the draws in its order)
    void generate(Leaf& e) {
        e.alive = true;
        e.scale = (p.scale_range * randpm() + p.scale_base) * effect_scale;
        e.t = 0;
        e.q0 = turn();
        e.q1 = turn();
        e.p0 = origin;
        const float r2 = (p.far_range * randpm() + p.far_base) * e.scale;
        e.p1 = dir(kPi) * r2 + origin;
        const float yaw3 = kTwoPi * randf(), pitch3 = kHalfPi * randf();  // (the end: the upper half only)
        float r3 = p.near_range * randpm() + p.near_base;
        r3 *= p.first_range * randpm() + p.first_base;
        r3 *= e.scale;
        e.p3 = V3{std::sin(pitch3) * std::cos(yaw3), std::cos(pitch3), std::sin(pitch3) * std::sin(yaw3)} * r3 + origin;
        const float yaw4 = kTwoPi * randf(), pitch4 = kPi * randf();
        const float r4 = (p.far_range * randpm() + p.far_base) * e.scale;
        e.p2 = V3{std::sin(pitch4) * std::cos(yaw4), std::cos(pitch4), std::sin(pitch4) * std::sin(yaw4)} * r4 + e.p3;
    }
    // _leaffxMoveLeaf: the next curve, from the last one's end and along its line
    void move(Leaf& e) {
        e.t -= 1.0f;
        e.q0 = e.q1;
        e.q1 = turn();
        e.p0 = e.p3;
        const float d0 = (p.far_range * randpm() + p.far_base) * e.scale;
        e.p1 = norm(e.p2 - e.p3) * -d0 + e.p0;
        const float yaw1 = kTwoPi * randf(), pitch1 = kPi * randf();
        const float d1 = (p.near_range * randpm() + p.near_base) * e.scale;
        e.p3 = V3{std::sin(pitch1) * std::cos(yaw1), std::cos(pitch1), std::sin(pitch1) * std::sin(yaw1)} * d1;
        e.p3.y += p.lift;
        e.p3 = e.p3 + e.p0;
        const float yaw2 = kTwoPi * randf(), pitch2 = kPi * randf();
        const float d2 = (p.far_range * randpm() + p.far_base) * e.scale;
        e.p2 = V3{std::sin(pitch2) * std::cos(yaw2), std::cos(pitch2), std::sin(pitch2) * std::sin(yaw2)} * d2 + e.p3;
    }
};

extern "C" OrreLeaves* orre_leaves_create(const OrreWzxLeaves* params, const float origin[3], float effect_scale, uint32_t seed) {
    if (!params || params->count <= 0) return nullptr;
    auto* l = new OrreLeaves;
    l->p = *params;
    l->p.count = std::min(32, l->p.count);
    l->origin = {origin[0], origin[1], origin[2]};
    l->effect_scale = effect_scale;
    l->seed = seed ? seed : 1;
    l->leaves.resize((size_t)l->p.count);
    for (Leaf& e : l->leaves) l->generate(e);
    return l;
}

extern "C" void orre_leaves_free(OrreLeaves* l) { delete l; }

// _leaffxUpdate: once the effect's time is up it stops (the copies are freed with it)
extern "C" int orre_leaves_update(OrreLeaves* l, int delta) {
    if (!l || l->frame >= l->p.frames) return 0;
    float step = l->p.speed * (float)delta;
    const int next = l->frame + delta;
    l->frame = next;
    if (next < l->p.fade_start) {
        step *= 2.0f - (float)next / (float)l->p.fade_start;
    } else {
        const int span = l->p.frames - l->p.fade_start;
        l->alpha = span > 0 ? 1.0f - (float)(next - l->p.fade_start) / (float)span : 0.0f;
    }
    for (Leaf& e : l->leaves) {
        if (!e.alive) continue;
        e.t += step;
        if (e.t >= 1.0f) l->move(e);
        if (bezier(e, e.t).y < 0.0f) e.alive = false;  // (below the ground: hidden and freed)
    }
    return 1;
}

extern "C" int orre_leaves_get(const OrreLeaves* l, float* xf, float* alpha) {
    if (!l) return 0;
    if (alpha) *alpha = l->alpha;
    int n = 0;
    for (const Leaf& e : l->leaves) {
        if (!e.alive) continue;
        if (xf) {
            // PSMTXQuat of the turn, scaled (fn_800E02C4), placed (fn_800E03B4)
            const Q q = slerp(e.q0, e.q1, e.t);
            const float s = e.scale;
            float* m = xf + 12 * n;
            m[0] = (1 - 2 * (q.y * q.y + q.z * q.z)) * s;
            m[1] = (2 * (q.x * q.y - q.w * q.z)) * s;
            m[2] = (2 * (q.x * q.z + q.w * q.y)) * s;
            m[3] = (2 * (q.x * q.y + q.w * q.z)) * s;
            m[4] = (1 - 2 * (q.x * q.x + q.z * q.z)) * s;
            m[5] = (2 * (q.y * q.z - q.w * q.x)) * s;
            m[6] = (2 * (q.x * q.z - q.w * q.y)) * s;
            m[7] = (2 * (q.y * q.z + q.w * q.x)) * s;
            m[8] = (1 - 2 * (q.x * q.x + q.y * q.y)) * s;
            const V3 pos = bezier(e, e.t);
            m[9] = pos.x;
            m[10] = pos.y;
            m[11] = pos.z;
        }
        ++n;
    }
    return n;
}
