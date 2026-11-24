// battle_camera.cpp: the battle's procedural move camera (2026-11-19, release step 4).
// Why: in battle every move shot is either authored (133 move files carry an HSD camera) or built at random around the
// Pokémon filmed: a motion (dolly, slide, orbit, or a held push / wide / close-up shot), a distance, height and angle
// inside ranges fitted to its size, and a zoom picked from weighted timing patterns (docs/engine/battle-camera.md).
// Studio's Moves preview films moves the same way. Ported from the matched decomp (src/game/wazaSequenceCamera.c:
// battleCameraStartWaza, _wazaSequenceCameraCalculateParams / SelectMotion / DoPosition / DoDollyPosition / DoFOV /
// SelectDuration, wazaSequenceCameraGetPattern, battleCameraUpdateWaza; src/game/camera.c: cameraUpdate's move
// channels and mode 7; the pattern tables from src/game/data/data_8036E150.c); constants and formulas verified from
// the code (a spec written from it, kept with Studio's notes). Random numbers: HSD's generator (seed * 214013 +
// 2531011), Studio's own seed: the battle draws from its one shared sequence, so the exact shots can't match a given
// battle, but every shot is one the game can make. Platforms: plain C++20, every OS.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "orre_core.h"
// the PowerPC frsqrte estimate, bit for bit (Aurora's header, MIT, Luke Street; the PC port's PSVECMag uses it): the
// camera's size comes from PSVECMag, an estimate plus one Newton step, not an exact square root, and the exact check
// against the game's log (orrestudio-cli bcamtest) needs the same bits
#include "dolphin/ppc_math.h"

namespace {
constexpr float kVsync = 60.0f;

// PSVECMag as the port computes it (extern/aurora/lib/dolphin/mtx/vec.c): the square sum with fused multiply-adds,
// times the refined reciprocal square-root estimate
template <class V>
float ps_vec_mag(const V& v) {
    const float xx = v.x * v.x, yy = v.y * v.y;
    const float xx_zz = std::fma(v.z, v.z, xx);
    const float sq = std::fma(xx_zz, 1.0f, yy);
    if (sq == 0.0f) return 0.0f;
    return sq * ppc_rsqrte(sq);
}  // fn_800D37CC: the NTSC video rate (moves are timed in seconds of it)

// The game's sinf / cosf (MSL, src/crt: (f32)sin((double)x); verified from the decomp), which PSMTXRotRad uses for
// _cameraDynamicUpdate's turn, and its atan2 on floats promoted to double. The C library's own float versions can round
// the last bit differently (the battle-camera log test's 1-ULP eye differences, 2026-11-19)
inline float gc_sinf(float x) { return (float)std::sin((double)x); }
inline float gc_cosf(float x) { return (float)std::cos((double)x); }
inline float gc_atan2f(float y, float x) { return (float)std::atan2((double)y, (double)x); }

struct Block { int mode, dur; float th[4]; int init, flags, timing; };
struct Pattern { float w; Block b[2]; };
// lbl_80371F60 (slots other than 8 / 9) and lbl_803721C0 (slots 8 / 9): weight, then two blocks
// {mode, duration mode, thresholds, opening FOV choice, FOV choice, timing}; a "hold" block is all 0 but its init
const Pattern kShort[8] = {
    {0.20f, {{3, 13, {.2f, 0, .7f, .1f}, 6, 1, 1}, {4, 5, {.8f, 0, .2f, 0}, 0, 2, 1}}},
    {0.25f, {{3, 13, {.3f, 0, .4f, .3f}, 4, 2, 2}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.20f, {{1, 0, {0, 0, 0, 0}, 1, 0, 0}, {4, 5, {.8f, 0, .2f, 0}, 0, 2, 1}}},
    {0.10f, {{4, 13, {.1f, 0, .2f, .7f}, 1, 2, 2}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 1, 0, 0}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 2, 0, 0}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 4, 0, 0}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.10f, {{4, 2, {0, 1, 0, 0}, 1, 2, 2}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
};
const Pattern kLong[13] = {
    {0.15f, {{4, 13, {.7f, 0, .15f, .15f}, 1, 2, 1}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.20f, {{4, 5, {.8f, 0, .2f, 0}, 1, 2, 1}, {3, 12, {.5f, 0, 0, .5f}, 0, 1, 1}}},
    {0.05f, {{3, 5, {.7f, 0, .3f, 0}, 6, 1, 1}, {4, 13, {.2f, 0, .5f, .3f}, 0, 6, 1}}},
    {0.05f, {{3, 5, {.8f, 0, .2f, 0}, 4, 3, 1}, {4, 13, {.2f, 0, .5f, .3f}, 0, 4, 1}}},
    {0.05f, {{3, 5, {.7f, 0, .3f, 0}, 6, 1, 1}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.15f, {{3, 5, {.7f, 0, .3f, 0}, 4, 2, 1}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.10f, {{1, 0, {0, 0, 0, 0}, 2, 0, 0}, {3, 12, {.7f, 0, 0, .3f}, 0, 1, 1}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 1, 0, 0}, {4, 13, {.1f, 0, .7f, .2f}, 0, 6, 1}}},
    {0.10f, {{1, 0, {0, 0, 0, 0}, 2, 0, 0}, {4, 13, {.1f, 0, .7f, .2f}, 0, 4, 1}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 1, 0, 0}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 2, 0, 0}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.05f, {{1, 0, {0, 0, 0, 0}, 4, 0, 0}, {0, 0, {0, 0, 0, 0}, 0, 0, 0}}},
    {0.00f, {{2, 0, {0, 0, 0, 0}, 0, 0, 0}, {3, 12, {0, 0, 0, 0}, 4, 3, 1}}},
};

struct V3 { float x = 0, y = 0, z = 0; };
inline V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 lerp(V3 a, V3 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }
struct Channel { bool on = false; float t = 0, dur = 0; V3 a, b; };  // camera.c's move channels: linear in seconds
struct Key { float a = 0, b = 0, f0 = 0, f1 = 0; };
}  // namespace

struct OrreBattleCamera {
    uint32_t seed = 1;
    int last_motion = -1;  // gLastMotionType (shared by every shot)
    // the shot
    OrreCamOwner own{};
    int motion = 0, shift = 0;
    bool reverse = false;
    float size = 0, height = 0, rot_min = 0, rot_max = 0, rot_base = 0, h_min = 0, h_max = 0, d_min = 0, d_max = 0;
    float near_ = 0, mid = 0, far_ = 0;
    // CameraPadState
    V3 position, view, eye;
    float cam_height = 0, distance = 0, rot_y = 0, fov = 45;
    Channel pos_ch, rot_ch;  // rot_ch uses .a.y / .b.y
    bool active = false;
    // FOV
    Key key[2];
    float clock = 0;
    float s0 = 0, s1 = 1;  // FovMix's kept range
    int start_step = 2;    // the frame step the start's own cameraUpdate runs with (orre_bcam_set_start_step)

    float randf() {  // HSD_Randf
        seed = seed * 214013u + 2531011u;
        return (float)(seed >> 16) / 65536.0f;
    }
    uint32_t rand16() {  // HSD_Rand
        seed = seed * 214013u + 2531011u;
        return seed >> 16;
    }
    uint32_t rand_n(uint32_t n) {  // _fadeEffectGetRandom: two draws
        const uint32_t a = rand16(), b = rand16();
        return n ? ((b << 16) | a) % n : ((b << 16) | a);
    }
    float rnd(float a, float b) { return a + (b - a) * randf(); }

    // the slot's shot length: tp[N-2], or tp[N-1] when that is 0
    int shot_len() const {
        const int n = std::clamp(own.tp_count, 2, 16);
        const int d = own.tp[n - 2];
        return d ? d : own.tp[n - 1];
    }

    void place(float d, float h, float a) {
        distance = d;
        cam_height = h;
        rot_y = a;
        position = {own.pos[0], own.pos[1], own.pos[2]};
        eye = {position.x + d * gc_sinf(a), position.y + h, position.z + d * gc_cosf(a)};
    }

    // _wazaSequenceCameraCalculateParams
    void params(int pf) {
        float ss, ds;
        switch (own.size_class) {
            case -2: case -1: ss = 1.1f; ds = 0.875f; break;
            case 1: ss = 1.25f; ds = 1.4f; break;
            case 2: ss = 1.3f; ds = 1.8f; break;
            case 3: ss = 1.5f; ds = 3.0f; break;
            default: ss = 1.2f; ds = 1.0f; break;
        }
        const bool big = own.size_class > 0;
        if (pf & 1) { rot_min = 0; rot_max = 0.5237035f; }
        else if (pf & 2) { rot_min = 0.31415927f; rot_max = 0.62831855f; }
        else if (pf & 4) { rot_min = big ? 0.31415927f : 0.62831855f; rot_max = big ? 0.62831855f : 0.9424779f; }
        else if (pf & 8) { rot_min = big ? 0.62831855f : 0.9424779f; rot_max = big ? 0.9424779f : 1.2566371f; }
        else if (pf & 0x10) { rot_min = 1.2566371f; rot_max = 1.5707964f; }
        else { rot_min = 0.31415927f; rot_max = big ? 0.9424779f : 1.0995574f; }
        float upper = 20;
        if (pf & 0x20) { d_min = 25; d_max = 35; upper = 8; }
        else if (pf & 0x40) { d_min = 35; d_max = 50; upper = 11; }
        else if (pf & 0x80) { d_min = 50; d_max = 60; upper = 20; }
        else { d_min = 20; d_max = 60; upper = 20; }
        const float lower = 6;
        d_min *= ds;
        d_max *= ds;
        const V3 sz{own.box_max[0] - own.box_min[0], own.box_max[1] - own.box_min[1], own.box_max[2] - own.box_min[2]};
        size = 0.57735026f * ps_vec_mag(sz);
        height = sz.y;
        rot_base = own.rot_y;
        h_min = own.box_min[1];
        h_max = own.box_max[1];
        if (h_min < lower) {
            h_min = lower;
            if (h_max < lower) h_max = 1.5f * lower;
        }
        if (h_max > upper) {
            h_max = upper;
            if (h_min > upper) h_min = 0.8f * upper;
        }
        size *= ss;
        height *= ss;
        if (d_min < 20) d_min = 20;
        if (d_min > 48) d_min = 48;
        if (d_max < 40) d_max = 40;
        if (d_max > 60) d_max = 60;
    }

    // _wazaSequenceCameraSelectMotion
    void select_motion(bool has_seq, uint32_t flags) {
        if (own.species == 0x13A) { motion = 4; return; }
        if (has_seq && (flags & 1)) { motion = 5; return; }
        int opts[4], n = 0;
        if (has_seq) {
            if (flags & 0x08) opts[n++] = 3;
            if (flags & 0x10) opts[n++] = 0;
            if (flags & 0x20) opts[n++] = 1;
            if (flags & 0x40) opts[n++] = 2;
        }
        if (n == 0) { opts[0] = 3; opts[1] = 0; opts[2] = 1; opts[3] = 2; n = 4; }
        if (n == 1) { motion = last_motion = opts[0]; return; }
        const float iv = 1.0f / (float)n;
        for (int guard = 0; guard < 64; ++guard) {
            const float r = randf();
            float lim = iv;
            bool done = false;
            for (int k = 0; k < n; ++k) {
                if (lim > r) {
                    motion = opts[k];
                    if (motion != last_motion) { done = true; break; }
                }
                if (k < n - 1) lim += iv;
            }
            if (done) break;
        }
        last_motion = motion;
    }

    float angle(float r) const {
        const float off = rot_min + (rot_max - rot_min) * r;
        return reverse ? rot_base - off : rot_base + off;
    }
    void move_pos(V3 a, V3 b, float secs) { pos_ch = {true, 0, secs, a, b}; }

    // _wazaSequenceCameraDoPosition / DoDollyPosition
    void position_shot() {
        int D = shot_len();
        if (motion != 4) D <<= shift;
        const float T = (float)D / kVsync;
        rot_ch.on = false;
        switch (motion) {
            case 0: {  // dolly: push in or pull out, possibly shortened
                const bool alt = randf() <= 0.7f;
                const float t0 = alt ? 0.3f : 0.5f, t1 = alt ? 0.65f : 0.75f;
                const float c = randf();
                int cut = 0;
                if (c < t0) {
                } else if (c < t1) {
                    const int full = D;
                    D = (int)rand_n((uint32_t)std::max(1, D));
                    if (D < (full >> 1)) D = full >> 1;
                } else {
                    cut = (int)rand_n((uint32_t)std::max(1, D));
                    if (cut > (D >> 1)) cut = D >> 1;
                }
                const float d0 = rnd(d_min, d_max), d1 = rnd(d_min, d_max);
                const float ds = alt ? std::max(d0, d1) : std::min(d0, d1), de = alt ? std::min(d0, d1) : std::max(d0, d1);
                const float h = rnd(h_min, h_max);
                const float a = angle(randf());
                place(ds, h, a);
                V3 v = sub(eye, position);
                v.y = 0;
                const float k = ds != 0 ? de / ds : 1.0f;
                v = {v.x * k, h, v.z * k};
                move_pos(eye, add(v, position), (float)(D - cut) / kVsync);
                near_ = std::sqrt(ds * ds + h * h);
                mid = std::sqrt(((ds + de) * 0.5f) * ((ds + de) * 0.5f) + h * h);
                far_ = std::sqrt(de * de + h * h);
                break;
            }
            case 1: {  // slide past
                const float off = 10.0f * randf() + 25.0f, h = 9.0f * randf() + 1.0f, d = 30.0f * randf() + 20.0f;
                place(d, h, 0.0f);
                eye.x += reverse ? 10.0f : -10.0f;
                V3 e1 = eye;
                e1.x += reverse ? off : -off;
                move_pos(eye, e1, T);
                const float sq = off * off + h * h;  // (the game's order: far = sqrt(d^2 + (off^2 + h^2)))
                near_ = std::sqrt(sq);
                far_ = std::sqrt(d * d + sq);
                mid = 0.5f * (near_ + far_);
                break;
            }
            case 2: {  // orbit from the smaller angle to the larger
                const float d = rnd(d_min, d_max), h = rnd(h_min, h_max);
                float a1 = angle(randf()), a2 = angle(randf());
                if (a1 > a2) std::swap(a1, a2);
                place(d, h, a1);
                rot_ch = {true, 0, T, {0, a1, 0}, {0, a2, 0}};
                near_ = mid = far_ = std::sqrt(d * d + h * h);
                break;
            }
            case 3: {  // a held shot (the "push" moves nowhere)
                const float d = rnd(d_min, d_max), h = rnd(h_min, h_max);
                place(d, h, angle(randf()));
                move_pos(eye, eye, T);
                near_ = mid = far_ = std::sqrt(d * d + h * h);
                break;
            }
            case 4: {  // the fixed wide shot
                place(110.0f, 25.0f, rot_base + (reverse ? -0.47123894f : 0.47123894f));
                move_pos(eye, eye, T);
                near_ = mid = far_ = std::sqrt(110.0f * 110.0f + 25.0f * 25.0f);
                break;
            }
            default: {  // 5: the close-up
                float k;
                switch (own.size_class) {
                    case -2: case -1: k = 0.875f; break;
                    case 1: k = 1.4f; break;
                    case 2: k = 1.8f; break;
                    case 3: k = 3.0f; break;
                    default: k = 1.0f; break;
                }
                const float h = rnd(h_min, h_max);
                place(50.0f * k, h, rot_base + (reverse ? -0.7853982f : 0.7853982f));
                move_pos(eye, eye, T);
                near_ = mid = far_ = std::sqrt(2500.0f * k * k + h * h);
                break;
            }
        }
    }

    // the FOV helpers (_wazaSequenceCameraFovRange, FovMix)
    float pick_fov(float R, int choice) {
        const float span = std::max(0.75f * size, height);
        float lo = 57.29578f * 2.0f * (float)std::atan2(0.5 * span, (double)R);
        float hi = 57.29578f * 2.0f * (float)std::atan2(2.0 * span, (double)R);
        lo = std::clamp(lo, 15.0f, 85.0f);
        hi = std::clamp(hi, 15.0f, 85.0f);
        switch (choice) {
            case 1: s0 = 0; s1 = 0.2f; break;
            case 2: s0 = 0.35f; s1 = 0.6f; break;
            case 4: case 5: s0 = 0.75f; s1 = 1.0f; break;
            case 3: s0 = 0; s1 = 0.6f; break;
            case 6: s0 = 0.35f; s1 = 1.0f; break;
            case 7: s0 = 0; s1 = 1.0f; break;
            default: break;  // 0: the range kept
        }
        return lo + (s0 + (s1 - s0) * randf()) * (hi - lo);
    }
    int select_duration(int mode, const float th[4], int len) {
        auto m = [&](int n) { return std::min(len, n); };
        switch (mode) {
            case 1: return m(12);
            case 2: return m(20);
            case 4: return m(45);
            case 8: return len;
            default: break;
        }
        const float r = randf();
        switch (mode) {
            case 3: return r < th[0] ? m(12) : m(20);
            case 5: return r < th[0] ? m(12) : m(45);
            case 9: return r < th[0] ? m(12) : len;
            case 6: return r < th[1] ? m(20) : m(45);
            case 10: return r < th[1] ? m(20) : len;
            case 12: return r < th[2] ? m(45) : len;
            case 7: return r < th[0] ? m(12) : (r < th[1] ? m(20) : m(45));
            case 11: return r < th[0] ? m(12) : (r < th[1] ? m(20) : len);
            case 13: return r < th[0] ? m(12) : (r < th[2] ? m(45) : len);
            case 14: return r < th[1] ? m(12) : (r < th[2] ? m(20) : len);
            case 15: return r < th[0] ? m(12) : (r < th[1] ? m(12) : (r < th[2] ? m(20) : len));
            default: return len;
        }
    }
    const Pattern& get_pattern(bool short_table, int pf) {
        const float r = randf();
        const Pattern* t = short_table ? kShort : kLong;
        const int n = short_table ? 8 : 13;
        if (pf & 0x20) return t[short_table ? 4 : 9];
        if (pf & 0x40) return t[short_table ? 5 : 10];
        if (pf & 0x80) return t[short_table ? 6 : 11];
        float sum = 0;
        for (int i = 0; i < n; ++i) {
            sum += t[i].w;
            if (r < sum) return t[i];
        }
        return t[rand_n((uint32_t)n)];
    }
    // _wazaSequenceCameraDoFOV
    void fov_shot(int pf) {
        const int N = std::clamp(own.tp_count, 0, 16);
        const float tp0 = (float)(own.tp[0] << shift);
        if (motion == 0 || motion == 4 || motion == 5) {
            const int choice = (pf & 0x20) ? 1 : (pf & 0x80) ? 4 : 2;
            fov = pick_fov(near_, choice);
            key[0] = key[1] = {fov, fov, tp0, tp0};
            clock = tp0;
            return;
        }
        const bool short_table = own.slot != 8 && own.slot != 9;
        const Pattern& pat = get_pattern(short_table, pf);
        float cur = pick_fov(near_, pat.b[0].init);
        fov = cur;
        key[0] = key[1] = {cur, cur, tp0, tp0};
        clock = tp0;
        if (N <= 2) return;
        for (int i = 0; i < 2; ++i) {
            const int a = own.tp[i], b = own.tp[i + 1];
            const Block& bl = pat.b[i];
            if (a == b) {
                key[i] = {cur, cur, (float)(a << shift), (float)(a << shift)};
                continue;
            }
            if (bl.mode == 3 || bl.mode == 4) {
                const int len = select_duration(bl.dur, bl.th, b - a);
                const float nf = pick_fov(i == 0 ? mid : far_, bl.flags);
                const int f0 = bl.timing == 2 ? b - len : a, f1 = bl.timing == 2 ? b : a + len;
                key[i] = {cur, nf, (float)(f0 << shift), (float)(f1 << shift)};
                cur = nf;
            } else {
                key[i] = {cur, cur, (float)(a << shift), (float)(b << shift)};
            }
        }
    }

    // the aim's channel (cameraMoveTargetOfs, re-armed by battleCameraUpdateWaza every frame with 0.2 s): cameraUpdate
    // runs first in a battle frame, so the view moves toward the target the previous frame chose (verified against the
    // game's ORRE_BCAM_LOG: the interest lags one frame)
    bool view_on = false;
    V3 view_from, view_to;
    float view_t = 0;
    bool battle_order = true;  // the battle's frame order (orre_bcam_set_order: 0 = the move viewer's)
    // cameraUpdate: the move channels (linear in seconds), then mode 7 rebuilds the eye from height / distance / yaw
    void camera_update(int step) {
        const float dt = (float)step / kVsync;
        if (view_on) {
            view_t += dt;
            if (view_t >= 0.2f) { view = view_to; view_on = false; }
            else view = lerp(view_from, view_to, view_t / 0.2f);
        }
        const bool pos_was = pos_ch.on, rot_was = rot_ch.on;
        if (pos_ch.on) {
            pos_ch.t += dt;
            if (pos_ch.t >= pos_ch.dur) { eye = pos_ch.b; pos_ch.on = false; }
            else eye = lerp(pos_ch.a, pos_ch.b, pos_ch.t / pos_ch.dur);
        }
        if (rot_ch.on) {
            rot_ch.t += dt;
            if (rot_ch.t >= rot_ch.dur) { rot_y = rot_ch.b.y; rot_ch.on = false; }
            else rot_y = rot_ch.a.y + (rot_ch.b.y - rot_ch.a.y) * (rot_ch.t / rot_ch.dur);
        }
        const V3 m = sub(eye, position);
        cam_height = m.y;
        if (!rot_ch.on) distance = std::sqrt(m.x * m.x + m.z * m.z);
        if (!rot_was && pos_was) rot_y = gc_atan2f(m.x, m.z);
        eye = {position.x + distance * gc_sinf(rot_y), position.y + cam_height, position.z + distance * gc_cosf(rot_y)};
    }
    // battleCameraUpdateWaza: the aim eases toward the joint (the view channel re-armed every frame with 0.2 s), the
    // FOV follows its keys on the slot's clock
    void update_waza(int step, const float* aim) {
        if (aim) {
            // the channel re-armed from the view now toward this frame's target. In a battle the next frame's
            // cameraUpdate takes its step (battle_main.c runs cameraUpdate before the sequences: the aim lags a frame,
            // verified on Willie's battles' log); in the move viewer the sequences run first, so the step comes at once
            // (verified on the move sweep's Tackle log)
            const V3 tgt{aim[0] - own.pos[0], aim[1] - own.pos[1], aim[2] - own.pos[2]};
            if (battle_order) {
                view_from = view;
                view_to = tgt;
                view_t = 0;
                view_on = true;
            } else {
                view = lerp(view, tgt, std::min(1.0f, ((float)step / kVsync) / 0.2f));
            }
        }
        clock += (float)step;
        float f = key[1].b;
        for (int i = 0; i < 2; ++i) {
            if (clock <= key[i].f0) { f = key[i].a; break; }
            if (clock <= key[i].f1) {
                const float span = key[i].f1 - key[i].f0;
                f = span > 0 ? key[i].a + (key[i].b - key[i].a) * ((clock - key[i].f0) / span) : key[i].b;
                break;
            }
            f = key[i].b;
        }
        fov = std::clamp(f, 3.0f, 120.0f);
    }
};

extern "C" OrreBattleCamera* orre_bcam_create(uint32_t seed) {
    auto* c = new OrreBattleCamera();
    c->seed = seed;
    return c;
}
extern "C" void orre_bcam_free(OrreBattleCamera* c) { delete c; }
extern "C" void orre_bcam_reset(OrreBattleCamera* c, uint32_t seed) {
    if (!c) return;
    const bool order = c->battle_order;
    *c = OrreBattleCamera{};
    c->seed = seed;
    c->battle_order = order;
}

// battleCameraStartWaza (the procedural path)
extern "C" int orre_bcam_start(OrreBattleCamera* c, const OrreCamOwner* o, int has_seq, uint32_t flags) {
    if (!c || !o) return -1;
    c->own = *o;
    c->reverse = o->facing < 0 && !(has_seq && (flags & 0x80));
    int pf = 0;
    if (!has_seq) {
        pf = 4;
    } else {
        if (flags & 0x200000) pf |= 2;
        else if (flags & 0x200) pf |= 4;
        else if (flags & 0x400000) pf |= o->pos[2] < 0.0f ? 4 : 8;
        if (flags & 0x400) pf |= 0x20;
        else if (flags & 0x800) pf |= 0x40;
        else if (flags & 0x1000) pf |= 0x80;
    }
    // a trainer's shot: the angle range by how many Pokémon it has out (battleGridGetNumPokemonsForTrainer: one 0x10,
    // more 1), and every duration doubled
    if (!o->is_pokemon) {
        if (o->partners == 1) pf = (pf & ~0x1F) | 0x10;
        else if (o->partners > 1) pf = (pf & ~0x1F) | 1;
    }
    c->shift = o->is_pokemon ? 0 : 1;
    c->pos_ch.on = c->rot_ch.on = false;
    c->view_on = false;
    // the aim: the joint's offset from the model's place (cameraRefreshTargetPos)
    c->position = {o->pos[0], o->pos[1], o->pos[2]};
    c->view = {o->aim[0] - o->pos[0], o->aim[1] - o->pos[1], o->aim[2] - o->pos[2]};
    c->params(pf);
    c->select_motion(has_seq != 0, flags);
    c->position_shot();
    c->fov_shot(pf);
    c->active = true;
    c->camera_update(c->start_step);  // (the start's own cameraUpdate advances the moves one frame step)
    return c->motion;
}

extern "C" void orre_bcam_update(OrreBattleCamera* c, int step, const float* aim) {
    if (!c || !c->active) return;
    c->camera_update(step);
    c->update_waza(step, aim);
}

extern "C" void orre_bcam_set_last_motion(OrreBattleCamera* c, int motion) {
    if (c) c->last_motion = motion;
}
extern "C" void orre_bcam_set_start_step(OrreBattleCamera* c, int step) {
    if (c) c->start_step = step;
}
extern "C" void orre_bcam_set_order(OrreBattleCamera* c, int battle) {
    if (c) c->battle_order = battle != 0;
}
extern "C" void orre_bcam_update_camera_only(OrreBattleCamera* c, int step) {
    if (c && c->active) c->camera_update(step);
}
extern "C" void orre_bcam_update_aim_only(OrreBattleCamera* c, int step, const float* aim) {
    if (c && c->active) c->update_waza(step, aim);
}
extern "C" int orre_bcam_debug(const OrreBattleCamera* c, float p[12], float k[8], uint32_t* seed) {
    if (!c) return -1;
    if (p) {
        const float v[12] = {c->size, c->height, c->rot_min, c->rot_max, c->rot_base, c->h_min, c->h_max, c->d_min, c->d_max,
                             c->near_, c->mid, c->far_};
        std::copy(v, v + 12, p);
    }
    if (k) {
        const float v[8] = {c->key[0].a, c->key[0].b, c->key[0].f0, c->key[0].f1, c->key[1].a, c->key[1].b, c->key[1].f0, c->key[1].f1};
        std::copy(v, v + 8, k);
    }
    if (seed) *seed = c->seed;
    return c->motion;
}

extern "C" int orre_bcam_get(const OrreBattleCamera* c, float eye[3], float interest[3], float* fov_deg) {
    if (!c || !c->active) return 0;
    if (eye) { eye[0] = c->eye.x; eye[1] = c->eye.y; eye[2] = c->eye.z; }
    if (interest) {
        interest[0] = c->position.x + c->view.x;
        interest[1] = c->position.y + c->view.y;
        interest[2] = c->position.z + c->view.z;
    }
    if (fov_deg) *fov_deg = c->fov;
    return 1;
}
