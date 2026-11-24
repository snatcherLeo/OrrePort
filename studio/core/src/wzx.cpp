// wzx.cpp: move-effect files (WZX), read the way the battle reads them (2026-11-19, release step 4: the move's own
// effects in Studio's Moves preview).
// Why: a move's look is a timeline of entries (particles, models, screen effects, sounds, control steps) placed against
// the user's own animation time points, so one file fits every Pokémon (docs/engine/wzx.md, models-animation.md
// "Running a move"). Studio plays the timeline itself; this file gives it the entries, their banks and their start
// times. The walk follows the game's loader (src/game/wazaSequence.c: wazaSequenceLoadData, wazaSequenceLoadEntryParams,
// wazaSequenceLoadHeader, _wazaSequenceParticleEntryLoad, _wazaSequenceEffectEntryLoad / fn_801364A8; verified from the
// code) and the extractor's reader (tools/local/colo/wzx.py), which reads all 1,098 files on the disc to their last byte.
// Platforms: plain C++20, every OS (no platform code here).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <vector>

#include "internal.hpp"

using namespace orre;

namespace {
inline size_t a32(size_t x) { return (x + 0x1F) & ~(size_t)0x1F; }
inline int s32(const Bytes& b, size_t o) { return (int)be32(b, o); }
}  // namespace

struct OrreWzx {
    Bytes data;
    OrreWzxHeader header{};
    std::vector<OrreWzxEntry> entries;
    std::vector<OrreParticleBank*> banks;  // per entry: its own bank (owned), or nullptr
    std::vector<int> bank_of;              // per entry: the entry whose bank it uses (-1 none)
    std::map<int, OrreWzxFade> fades;      // colour fades (screen effect kind 0), by entry
    std::map<int, size_t> model_at;        // a model entry's archive offset, by entry
    struct Bolt { OrreWzxBolt p{}; std::vector<uint8_t> rgba; };
    std::map<int, Bolt> bolts;             // bolts (screen effect kind 1), by entry
    std::map<int, OrreWzxBlur> blurs;      // blurs (screen effect kind 9), by entry
    std::map<int, OrreWzxBillboard> billboards;  // billboards (screen effect kind 11) with a model, by entry
    std::map<int, OrreWzxLeaves> leaves;   // leaves (screen effect kind 5), by entry
    std::map<int, OrreWzxEnvMap> envmaps;  // env-map shines (screen effect kind 8), by entry
    struct Warp { OrreWzxWarp p{}; std::vector<uint8_t> rgba; };
    std::map<int, Warp> warps;             // screen warps (screen effect kind 12), by entry
    ~OrreWzx() {
        for (OrreParticleBank* b : banks)
            if (b) orre_particle_bank_free(b);
    }
};

namespace {
// The screen-effect descriptor's size (fn_801364A8; the same walk as wzx.py's effect()): u32 kind, s32 frames, u32,
// then the kind's parameters and any embedded texture or model archive (32-byte aligned). Returns the next offset, or
// 0 for the two layouts the game's parser leaves its cursor unset on (no file on the disc uses them).
size_t effect_end(const Bytes& b, size_t p, int& kind, int& frames) {
    kind = s32(b, p);
    frames = s32(b, p + 4);
    const size_t q = p + 0x0C;
    switch (kind) {
        case 0: {
            const int count = s32(b, q + 4), mode = s32(b, q + 8);
            return q + (mode == 1 || mode == 2 ? 0x0C : 0x10) + 16 * (size_t)std::max(0, count);
        }
        case 1: return a32(q + 0x54) + a32((size_t)s32(b, q + 0x4C));
        case 2: return q + 0x1C;
        case 3: return a32(q + 0x54) + a32(be32(b, q + 0x4C));
        case 4: return a32(q + 0x18) + a32((size_t)s32(b, q + 0x10));
        case 5: return a32(q + 0x3C) + a32((size_t)s32(b, q + 0x34));
        case 6: return a32(q + (be32(b, q + 0x3C) == 1 ? 0x40 : 0x44)) + a32(be32(b, q + 0x38));
        case 7: return a32(q + 0x34) + a32((size_t)s32(b, q + 0x2C));
        case 8: {
            const int count = s32(b, q + 8), mode = s32(b, q + 0x0C);
            return mode == 2 ? q + 0x10 + 16 * (size_t)std::max(0, count) : 0;
        }
        case 9: {
            const int count = s32(b, q), mode = s32(b, q + 4);
            const size_t d = q + (mode == 3 ? 0x0C : 8);
            return mode == 2 || mode == 3 ? d + 16 * (size_t)std::max(0, count) : 0;
        }
        case 10: return q + 0x28;
        case 11: {
            const int size = s32(b, q);
            return size ? a32(q + 0x18) + a32((size_t)size) : a32(q + 0x18);
        }
        case 12: {
            const int mode = s32(b, q + 0x14);
            return a32(q + (mode == 1 ? 0x18 : 0x1C)) + a32((size_t)s32(b, q + 0x10));
        }
        default: return q;  // an unknown kind: the game's switch does nothing
    }
}
}  // namespace

extern "C" OrreWzx* orre_wzx_read(const uint8_t* p, size_t n) {
    if (!p || n < 0x88) return nullptr;
    auto w = std::make_unique<OrreWzx>();
    w->data.assign(p, p + n);
    const Bytes& b = w->data;
    // the first node header (no meaning of its own; size mode +0x68: 1 = 0x6C bytes), then the sequence header
    size_t o = s32(b, 0x68) == 1 ? 0x6C : 0x70;
    const int slot = s32(b, o), count = s32(b, o + 4) - 1, flags = s32(b, o + 8), value = s32(b, o + 0x0C),
              mode = s32(b, o + 0x10), res = s32(b, o + 0x14);
    if (count < 0 || count > 4096) return nullptr;
    OrreWzxHeader& h = w->header;
    h.slot = slot;
    h.value = value;
    h.mode = mode;
    h.count = count;
    // wazaSequenceLoadHeader's fixes for the older layouts: modes up to 3 allow every procedural camera motion (0x78),
    // modes up to 5 drop flag bits 15-20
    uint32_t f = (uint32_t)flags;
    if (mode <= 3) f |= 0x78;
    if (mode <= 5) f &= ~0x001F8000u;
    h.flags = f;
    if (mode == 1) o += 0x10;
    else if (mode == 2) o += 0x14;
    else {
        const size_t at = a32(o + 0x18);
        h.camera_size = res > 0 ? res : 0;
        o = at + (res > 0 ? a32((size_t)res) : 0);
    }
    std::map<int, int> by_key;  // first entry with each key, as the walk over the loaded list finds it
    for (int i = 0; i < count; ++i) {
        if (o + 0x6C > b.size()) return nullptr;
        OrreWzxEntry e{};
        const int smode = s32(b, o + 0x68);
        e.key = s32(b, o);
        e.type = s32(b, o + 4);
        e.attach = s32(b, o + 8);
        e.follow = s32(b, o + 0x0C);
        e.link_key = s32(b, o + 0x10);
        e.own_index = s32(b, o + 0x14);
        e.base_index = s32(b, o + 0x18);
        e.length_index = s32(b, o + 0x1C);
        for (int k = 0; k < 16; ++k) e.timing[k] = s32(b, o + 0x20 + 4 * (size_t)k);
        e.flags = be32(b, o + 0x60);
        e.part = s32(b, o + 0x64);
        e.bank_link = smode == 2 ? s32(b, o + 0x6C) : 0;
        const size_t pl = o + (smode == 1 ? 0x6C : 0x70);
        OrreParticleBank* bank = nullptr;
        int bank_of = -1;
        switch (e.type) {
            case 1:
                e.value_kind = s32(b, pl);
                e.value = s32(b, pl + 4);
                e.value_f = bef32(b, pl + 4);
                o = pl + 0x0C + (e.value_kind == 3 ? 8 * (size_t)std::max(0, e.value) : 0);
                break;
            case 2:
                // the model entry (_wazaSequenceModelEntryLoad; the entries spec B1, verified from the code): +0x00 joint
                // animation (-1 none), +0x04 its type (0 once), +0x08 texture animation, +0x0C its type, +0x10 carry
                // the owner, +0x14 the effect model's joint the owner rides, +0x18 model flags, +0x1C the archive's size;
                // the HSD scene archive at the next 32-byte boundary after +0x24 (its scene_data's model 0 is made)
                e.model_anim = s32(b, pl);
                e.model_tex_anim = s32(b, pl + 8);
                e.model_carry = s32(b, pl + 0x10);
                e.model_carry_joint = s32(b, pl + 0x14);
                e.model_flags = be32(b, pl + 0x18);
                e.model_size = s32(b, pl + 0x1C);
                w->model_at[i] = a32(pl + 0x24);
                o = a32(pl + 0x24) + a32((size_t)std::max(0, e.model_size));
                break;
            case 3: {
                e.emitter = s32(b, pl);
                e.mode = be32(b, pl + 4);
                const size_t size = be32(b, pl + 8);
                const size_t d = pl + 0x10 + (s32(b, pl + 0x0C) == 3 ? 4 : 0);
                if (e.bank_link) {
                    auto it = by_key.find(e.bank_link);
                    if (it != by_key.end()) bank_of = w->bank_of[(size_t)it->second];
                    o = d;
                } else {
                    if (d + size > b.size()) return nullptr;
                    bank = orre_particle_bank_read(b.data() + d, size);
                    e.has_bank = bank != nullptr;
                    if (bank) bank_of = i;
                    o = d + a32(size);
                }
                break;
            }
            case 4: {
                const size_t end = effect_end(b, pl, e.effect_kind, e.effect_frames);
                if (!end) return nullptr;
                if (e.effect_kind == 0) {
                    // the colour fade (fn_80137780 / filterAddKey, src/game/effect/tracefx.c and effect_visual.c;
                    // verified from the code): +0x0C target (low byte: 0 a full-screen quad, 1 the stage's models,
                    // 2 the owner), +0x10 key count, +0x14 mode (1 / 2: no flags word), +0x18 flags (1 use the keys'
                    // alpha, 4 ignore their RGB), then keys {u32 from, u32 to, s32 frames, pad}; a colour word read
                    // big-endian has red in its low byte (the disc's bytes run A, B, G, R); frames < 0 = hold, both
                    // colours then the midpoint
                    OrreWzxFade fd{};
                    const size_t q = pl + 0x0C;
                    fd.target = s32(b, q) & 0xFF;
                    const int count = s32(b, q + 4), mode = s32(b, q + 8);
                    const bool flagged = !(mode == 1 || mode == 2);
                    const uint32_t flags = flagged ? be32(b, q + 0x0C) : 0;
                    fd.use_alpha = fd.target == 0 || (flags & 1);
                    fd.ignore_rgb = fd.target != 0 && (flags & 4);
                    size_t d = q + (flagged ? 0x10 : 0x0C);
                    fd.count = std::clamp(count, 0, 16);
                    for (int k = 0; k < fd.count; ++k, d += 16) {
                        const uint32_t from = be32(b, d), to = be32(b, d + 4);
                        const int frames = s32(b, d + 8);
                        for (int c = 0; c < 4; ++c) {
                            fd.key[k].from[c] = (uint8_t)(from >> (8 * c));
                            fd.key[k].to[c] = (uint8_t)(to >> (8 * c));
                        }
                        if (frames < 0) {
                            for (int c = 0; c < 4; ++c)
                                fd.key[k].from[c] = fd.key[k].to[c] =
                                    (uint8_t)(fd.key[k].from[c] + 0.5f * ((float)fd.key[k].to[c] - (float)fd.key[k].from[c]));
                            fd.key[k].len = -1;
                        } else {
                            fd.key[k].len = frames;  // (60 Hz frames; the US disc's rate leaves them as they are)
                        }
                    }
                    w->fades[i] = fd;
                } else if (e.effect_kind == 1) {
                    // the bolt (fn_801364A8 case 1, src/game/effect/tracefx.c; verified from the code): from the
                    // parameters at +0x0C: vectors jitter +0x00, spread +0x0C, scale +0x18, step +0x24; floats height
                    // +0x30, width +0x34, branch chance +0x38; count +0x3C, depth +0x40 (u16); colour +0x44 (red in
                    // the word's low byte, as the fades'); the owner's part +0x48; the GStexture's size +0x4C, the
                    // file itself at the next 32-byte boundary after +0x54; frames scaled by the video rate / 60 (1)
                    OrreWzx::Bolt bt;
                    const size_t q = pl + 0x0C;
                    for (int c = 0; c < 3; ++c) {
                        bt.p.jitter[c] = bef32(b, q + 4 * c);
                        bt.p.spread[c] = bef32(b, q + 0x0C + 4 * c);
                        bt.p.scale[c] = bef32(b, q + 0x18 + 4 * c);
                        bt.p.step[c] = bef32(b, q + 0x24 + 4 * c);
                    }
                    bt.p.height = bef32(b, q + 0x30);
                    bt.p.width = bef32(b, q + 0x34);
                    bt.p.branch = bef32(b, q + 0x38);
                    bt.p.count = s32(b, q + 0x3C) & 0xFFFF;
                    bt.p.depth = s32(b, q + 0x40) & 0xFFFF;
                    const uint32_t col = be32(b, q + 0x44);
                    for (int c = 0; c < 4; ++c) bt.p.rgba[c] = (uint8_t)(col >> (8 * c));
                    bt.p.part = s32(b, q + 0x48);
                    bt.p.frames = e.effect_frames & 0xFFFF;
                    const size_t ts = (size_t)std::max(0, s32(b, q + 0x4C)), to = a32(q + 0x54);
                    if (ts && to + ts <= b.size()) {
                        const Bytes tex(b.begin() + (std::ptrdiff_t)to, b.begin() + (std::ptrdiff_t)(to + ts));
                        int tw = 0, th = 0;
                        if (decode_texture_file(tex, tw, th, bt.rgba)) { bt.p.tex_width = tw; bt.p.tex_height = th; }
                    }
                    w->bolts[i] = std::move(bt);
                } else if (e.effect_kind == 9) {
                    // the blur (fn_80137114, src/game/effect/tracefx.c; verified from the code): from +0x0C a count and
                    // a mode. Mode 3: the screen switch at +8, keys from +0x0C; mode 2: the screen blur, keys from +8;
                    // each key {f32 from, f32 to, s32 frames, pad} (frames x the video rate / 60). Mode 1: the screen
                    // blur with one value v at +0 (the count's word) over the entry's frames in thirds: 0 -> v, v, 0
                    OrreWzxBlur bl{};
                    const size_t q = pl + 0x0C;
                    const int count = s32(b, q), mode = s32(b, q + 4);
                    auto add = [&](float a0, float a1, int t) {
                        if (bl.count < 16) bl.key[bl.count++] = {a0, a1, std::max(0, t)};
                    };
                    if (mode == 3 || mode == 2) {
                        bl.screen = mode == 3 ? (int)be32(b, q + 8) != 0 : 1;
                        size_t d = q + (mode == 3 ? 0x0C : 8);
                        for (int k = 0; k < count; ++k, d += 16) add(bef32(b, d), bef32(b, d + 4), s32(b, d + 8));
                    } else if (mode == 1) {
                        bl.screen = 1;
                        const float v = bef32(b, q);
                        const int third = e.effect_frames / 3;
                        add(0, v, third);
                        add(v, v, third);
                        add(0, 0, third);
                    }
                    w->blurs[i] = bl;
                } else if (e.effect_kind == 5) {
                    // the leaves (fn_801364A8 case 5, verified from the code): from +0x0C ten floats +0x00..+0x24, the
                    // count +0x28, the fade's start +0x2C, the attach point +0x30, the model archive's size +0x34 and
                    // the archive at the next 32-byte boundary after +0x3C (model 0 of its scene, as a model entry)
                    const size_t q = pl + 0x0C;
                    OrreWzxLeaves lv{};
                    float* f[10] = {&lv.scale_base, &lv.scale_range, &lv.near_base, &lv.near_range, &lv.far_base,
                                    &lv.far_range, &lv.lift, &lv.speed, &lv.first_base, &lv.first_range};
                    for (int k = 0; k < 10; ++k) *f[k] = bef32(b, q + 4 * (size_t)k);
                    lv.count = std::min(32, s32(b, q + 0x28) & 0xFFFF);
                    lv.fade_start = s32(b, q + 0x2C) & 0xFFFF;
                    lv.attach = s32(b, q + 0x30);
                    lv.frames = e.effect_frames & 0xFFFF;
                    const int size = s32(b, q + 0x34);
                    if (size > 0) {
                        w->model_at[i] = a32(q + 0x3C);
                        e.model_size = size;
                    }
                    w->leaves[i] = lv;
                } else if (e.effect_kind == 8) {
                    // the env-map shine (tracefx.c case 8, verified from the code): +0x00 mode, +0x04 type, +0x08 count,
                    // +0x0C key mode: 2 = count keys {f32 from, f32 to, s32 frames, pad} from +0x10; 1 = one value (the
                    // +0x08 word read as a float) over the entry's frames in thirds: 0 -> v, v, v -> 0 (the third key
                    // is written 0 -> 0, as the blur's mode 1); frames x the video rate / 60 (1)
                    const size_t q = pl + 0x0C;
                    OrreWzxEnvMap em{};
                    em.mode = s32(b, q);
                    em.type = s32(b, q + 4);
                    const int count = s32(b, q + 8), kmode = s32(b, q + 0x0C);
                    auto add = [&](float a0, float a1, int t) {
                        if (em.count < 16) { em.key[em.count].from = a0; em.key[em.count].to = a1; em.key[em.count].ticks = t; ++em.count; }
                    };
                    if (kmode == 2) {
                        size_t d = q + 0x10;
                        for (int k = 0; k < count; ++k, d += 16) add(bef32(b, d), bef32(b, d + 4), s32(b, d + 8));
                    } else if (kmode == 1) {
                        const float v = bef32(b, q + 8);
                        const int third = e.effect_frames / 3;
                        add(0, v, third);
                        add(v, v, third);
                        add(0, 0, third);
                    }
                    w->envmaps[i] = em;
                } else if (e.effect_kind == 12) {
                    // the screen warp (tracefx.c case 12, verified from the code): +0x00 particles switch, +0x04 follow
                    // switch, +0x08 size, +0x0C strength, +0x10 the warp picture's size, +0x14 mode (1: peak 1.0, the
                    // picture after +0x18; 2: the peak at +0x18, the picture after +0x1C), at the next 32-byte boundary
                    const size_t q = pl + 0x0C;
                    OrreWzx::Warp wp;
                    wp.p.particles = (s32(b, q) & 0xFFFF) != 0;
                    wp.p.follow = (s32(b, q + 4) & 0xFFFF) != 0;
                    wp.p.size = bef32(b, q + 8);
                    wp.p.strength = bef32(b, q + 0x0C);
                    const int mode = s32(b, q + 0x14);
                    wp.p.peak = mode == 2 ? bef32(b, q + 0x18) : 1.0f;
                    wp.p.frames = e.effect_frames & 0xFFFF;
                    const size_t ts = (size_t)std::max(0, s32(b, q + 0x10)), to = a32(q + (mode == 1 ? 0x18 : 0x1C));
                    if (ts && to + ts <= b.size()) {
                        const Bytes tex(b.begin() + (std::ptrdiff_t)to, b.begin() + (std::ptrdiff_t)(to + ts));
                        int tw = 0, th = 0;
                        if (decode_texture_file(tex, tw, th, wp.rgba)) { wp.p.tex_width = tw; wp.p.tex_height = th; }
                    }
                    w->warps[i] = std::move(wp);
                } else if (e.effect_kind == 11) {
                    // the billboard (fn_801364A8 case 11, src/game/effect/tracefx.c; verified from the code): from +0x0C
                    // the model archive's size +0x00 (0: none), the animation +0x08, a particle subid +0x0C, the loop
                    // switch +0x10 (0 / 1); the archive at the next 32-byte boundary after +0x18, read as a model
                    // entry's is (model 0 of its scene)
                    const size_t q = pl + 0x0C;
                    const int size = s32(b, q);
                    if (size > 0) {
                        OrreWzxBillboard bb{};
                        bb.anim = s32(b, q + 8);
                        bb.subid = s32(b, q + 0x0C) & 0xFFFF;
                        bb.loop = s32(b, q + 0x10) == 1;
                        w->billboards[i] = bb;
                        w->model_at[i] = a32(q + 0x18);
                        e.model_size = size;
                    }
                }
                // Why: an entry whose last time point (timing[length_index - 1]) is 0 takes the effect's own length
                // there (_wazaSequenceEffectEntryLoad, src/game/wazaSequence.c; verified from the code), before it is
                // linked, so "start at my end" links work. Howl 336's stage fade links its time point 1 to the mouth
                // glow's and is 210 long: it starts 210 ticks before its anchor, near the move's start, as the game's
                // floor shows (the darkening from about 10 frames in; it was 109 in Studio before). The length per kind
                // (fn_801364A8's record): a fade, the blur and the env-map shine, the sum of their keys' lengths (a
                // held fade key counts -1, as the game's u32 sum wraps); the billboard 0; every other kind its frames
                // as a u16. Applies to every platform (data rule, no OS code).
                // (ORRE_WZX_NOFILL=1 turns the rule off, to list what it changes)
                if (e.length_index >= 1 && e.length_index <= 16 && e.timing[e.length_index - 1] == 0 &&
                    !std::getenv("ORRE_WZX_NOFILL")) {
                    int len = e.effect_frames & 0xFFFF;
                    if (e.effect_kind == 0) {
                        len = 0;
                        auto f = w->fades.find(i);
                        if (f != w->fades.end())
                            for (int k = 0; k < f->second.count; ++k) len += f->second.key[k].len;
                    } else if (e.effect_kind == 8) {
                        len = 0;
                        auto m = w->envmaps.find(i);
                        if (m != w->envmaps.end())
                            for (int k = 0; k < m->second.count; ++k) len += m->second.key[k].ticks;
                    } else if (e.effect_kind == 9) {
                        len = 0;
                        auto m = w->blurs.find(i);
                        if (m != w->blurs.end())
                            for (int k = 0; k < m->second.count; ++k) len += m->second.key[k].ticks;
                    } else if (e.effect_kind == 11 || e.effect_kind < 0 || e.effect_kind > 12) {
                        len = 0;
                    }
                    e.timing[e.length_index - 1] = len;
                    if (std::getenv("ORRE_DEBUG_WZX_FILL") && len)
                        std::fprintf(stderr, "[wzx fill] entry %d effect kind %d: time point %d = %d\n", i,
                                     e.effect_kind, e.length_index - 1, len);
                }
                o = end;
                break;
            }
            case 5:
                e.sound = s32(b, pl);
                e.sound_kind = s32(b, pl + 4);
                o = pl + (e.sound_kind == 1 || e.sound_kind == 2 ? 8 : 0x0C);
                break;
            case 6:
                e.control = s32(b, pl);
                o = pl + 8;
                break;
            default:
                return nullptr;  // the game logs an error and stops loading
        }
        if (e.key && !by_key.count(e.key)) by_key[e.key] = i;
        w->entries.push_back(e);
        w->banks.push_back(bank);
        w->bank_of.push_back(bank_of);
    }
    // (the last entry may end a few bytes short of a 32-byte boundary: a bank's size is rounded up as it is skipped)
    if (o > a32(b.size())) return nullptr;
    // Why: the model entry's version of the rule above (_wazaSequenceModelEntryLoad, verified from the code): a 0 last
    // time point takes the model's animation length (GSmodelGetFrameCount's larger of start and end; 0 with no
    // animation). Which animation the game's motion measures is likely the entry's joint animation (or the first);
    // done after the walk since the model is parsed from the finished reader, which is the same result because the
    // fill only touches the entry itself before it is linked. Every platform.
    for (size_t i = 0; i < w->entries.size(); ++i) {
        OrreWzxEntry& e = w->entries[i];
        if (e.type != 2 || e.length_index < 1 || e.length_index > 16 || e.timing[e.length_index - 1] != 0 ||
            std::getenv("ORRE_WZX_NOFILL"))
            continue;
        int len = 0;
        OrreModel m{};
        if (orre_wzx_model(w.get(), (int)i, &m)) {
            if (m.animation_count > 0)
                len = (int)orre_model_animation_length(&m, e.model_anim >= 0 && e.model_anim < m.animation_count ? e.model_anim : 0);
            orre_model_free(&m);
        }
        e.timing[e.length_index - 1] = len;
        if (std::getenv("ORRE_DEBUG_WZX_FILL") && len)
            std::fprintf(stderr, "[wzx fill] entry %zu model: time point %d = %d\n", i, e.length_index - 1, len);
    }
    return w.release();
}

extern "C" void orre_wzx_free(OrreWzx* w) { delete w; }

extern "C" int orre_wzx_header(const OrreWzx* w, OrreWzxHeader* out) {
    if (!w || !out) return 0;
    *out = w->header;
    return 1;
}

extern "C" int orre_wzx_entry(const OrreWzx* w, int i, OrreWzxEntry* out) {
    if (!w || !out || i < 0 || i >= (int)w->entries.size()) return 0;
    *out = w->entries[(size_t)i];
    return 1;
}

extern "C" int orre_wzx_model(const OrreWzx* w, int i, OrreModel* out) {
    if (!w || !out || i < 0 || i >= (int)w->entries.size()) return 0;
    auto it = w->model_at.find(i);
    const int size = w->entries[(size_t)i].model_size;
    if (it == w->model_at.end() || size <= 0 || it->second + (size_t)size > w->data.size()) return 0;
    const Bytes dat(w->data.begin() + (std::ptrdiff_t)it->second, w->data.begin() + (std::ptrdiff_t)(it->second + (size_t)size));
    *out = OrreModel{};
    return parse_hsd_model(dat, *out) ? 1 : 0;
}

extern "C" int orre_wzx_fade(const OrreWzx* w, int i, OrreWzxFade* out) {
    if (!w || !out) return 0;
    auto it = w->fades.find(i);
    if (it == w->fades.end()) return 0;
    *out = it->second;
    return 1;
}

// _filterUpdate (effect_visual.c; verified from the code), evaluated from the effect's age: the k-th update since the
// start runs with 2k ticks elapsed (the battle steps 2 per frame), shows the current key's from + (elapsed / len) x
// (to - from), truncated; a hold key shows its colour for good; past the last key the effect has stopped (and the
// tint is removed): returns 0
extern "C" int orre_wzx_fade_colour(const OrreWzxFade* f, int ticks, uint8_t rgba[4]) {
    if (!f || f->count <= 0 || ticks < 0) return 0;
    int k = 0;
    long long e = ticks;
    for (;;) {
        if (k >= f->count) return 0;
        const int len = f->key[k].len;
        if (len < 0) { e = 0; break; }
        if (e < len) break;
        e -= len;
        ++k;
        if (k >= f->count) return 0;  // (the last key ran out: the game writes its "to" and stops this frame)
    }
    const auto& key = f->key[k];
    const float t = key.len > 0 ? std::clamp((float)e / (float)key.len, 0.0f, 1.0f) : 0.0f;
    for (int c = 0; c < 4; ++c) rgba[c] = (uint8_t)((float)key.from[c] + t * ((float)key.to[c] - (float)key.from[c]));
    if (!f->use_alpha) rgba[3] = 0xFF;
    if (f->ignore_rgb) rgba[0] = rgba[1] = rgba[2] = 0x7F;
    return 1;
}

extern "C" int orre_wzx_warp(const OrreWzx* w, int i, OrreWzxWarp* out) {
    if (!w || !out) return 0;
    auto it = w->warps.find(i);
    if (it == w->warps.end()) return 0;
    *out = it->second.p;
    out->tex_rgba = it->second.rgba.empty() ? nullptr : it->second.rgba.data();
    return 1;
}

extern "C" int orre_wzx_envmap(const OrreWzx* w, int i, OrreWzxEnvMap* out) {
    if (!w || !out) return 0;
    auto it = w->envmaps.find(i);
    if (it == w->envmaps.end()) return 0;
    *out = it->second;
    return 1;
}

// _envMapUpdate (effect_visual.c; verified from the code): the elapsed ticks run through the keys (a key of -1 ticks
// holds its "from" for good); the value is from + (elapsed / ticks) x (to - from); past the last key it stops
extern "C" int orre_wzx_envmap_value(const OrreWzxEnvMap* e, int ticks, float* value) {
    if (!e || e->count <= 0 || ticks < 0) return 0;
    int k = 0;
    long long t = ticks;
    for (;;) {
        if (k >= e->count) return 0;
        if (e->key[k].ticks == -1) { t = 0; break; }
        if (t < e->key[k].ticks) break;
        t -= e->key[k].ticks;
        ++k;
    }
    const auto& key = e->key[k];
    const float f = key.ticks > 0 ? (float)t / (float)key.ticks : 0.0f;
    if (value) *value = key.from + f * (key.to - key.from);
    return 1;
}

extern "C" int orre_wzx_leaves(const OrreWzx* w, int i, OrreWzxLeaves* out) {
    if (!w || !out) return 0;
    auto it = w->leaves.find(i);
    if (it == w->leaves.end()) return 0;
    *out = it->second;
    return 1;
}

extern "C" int orre_wzx_billboard(const OrreWzx* w, int i, OrreWzxBillboard* out) {
    if (!w || !out) return 0;
    auto it = w->billboards.find(i);
    if (it == w->billboards.end()) return 0;
    *out = it->second;
    return 1;
}

extern "C" int orre_wzx_blur(const OrreWzx* w, int i, OrreWzxBlur* out) {
    if (!w || !out) return 0;
    auto it = w->blurs.find(i);
    if (it == w->blurs.end()) return 0;
    *out = it->second;
    return 1;
}

extern "C" int orre_wzx_bolt(const OrreWzx* w, int i, OrreWzxBolt* out) {
    if (!w || !out) return 0;
    auto it = w->bolts.find(i);
    if (it == w->bolts.end()) return 0;
    *out = it->second.p;
    out->tex_rgba = it->second.rgba.empty() ? nullptr : it->second.rgba.data();
    return 1;
}

extern "C" const OrreParticleBank* orre_wzx_bank(const OrreWzx* w, int i) {
    if (!w || i < 0 || i >= (int)w->entries.size()) return nullptr;
    const int b = w->bank_of[(size_t)i];
    return b >= 0 ? w->banks[(size_t)b] : nullptr;
}

// wazaSequenceEntryLink, entry by entry in file order: the start is the linked entry's start + its timing[base] (the
// link found by walking the list loaded so far, which is kept sorted by start), else the owner's time point [base];
// minus the entry's own timing[own]. Then it is inserted in start order (a control step goes before others with the
// same start). Finally every start is shifted so the earliest is 0 (wazaSequenceLoadData). Timings are 60 Hz values
// scaled by the video rate / 60 (fn_800D37CC / 60.0: 1 on the 60 Hz US disc).
namespace {
int schedule_impl(const OrreWzx* w, const int tp[16], int* starts, int* order, int* lead);
}
extern "C" int orre_wzx_schedule(const OrreWzx* w, const int tp[16], int* starts, int* order) {
    return schedule_impl(w, tp, starts, order, nullptr);
}
// The shift wazaSequenceLoadData applies (src/game/wazaSequence.c; verified from the code): when entries start before
// the owner's time point 0, every start moves later by that much and the sequence keeps it (seq + 0x10), which
// wazaSequenceOwnerSetAnim hands to the owner's slot record (+0x90). wazaSequencePokemonMotionStart
// (src/game/wazaSequenceEntry.c; verified) then starts the slot's animation at frame (length - shift / 2) instead of 0,
// so it plays its last shift / 2 frames first (a hit animation's settled ending: standing) and its real first frame
// lands exactly on the shift, in step with its time points. Verified against the move sweep, 2026-11-19: Flamethrower's
// target half has a 30-tick shift and the game's Umbreon is knocked back 15-18 frames into the half
extern "C" int orre_wzx_lead_in(const OrreWzx* w, const int tp[16]) {
    int lead = 0;
    schedule_impl(w, tp, nullptr, nullptr, &lead);
    return lead;
}
namespace {
int schedule_impl(const OrreWzx* w, const int tp[16], int* starts, int* order, int* lead) {
    if (!w) return 0;
    const int n = (int)w->entries.size();
    std::vector<int> st((size_t)n, 0);
    std::list<int> run;  // the loaded list, by index
    auto clamp16 = [](int k) { return k < 0 ? 0 : (k > 15 ? 15 : k); };  // (a bad index reads a neighbour in the game)
    for (int i = 0; i < n; ++i) {
        const OrreWzxEntry& e = w->entries[(size_t)i];
        int s = 0;
        bool linked = false;
        if (e.link_key) {
            for (int j : run)
                if (w->entries[(size_t)j].key == e.link_key) {
                    s = st[(size_t)j] + w->entries[(size_t)j].timing[clamp16(e.base_index)];
                    linked = true;
                    break;
                }
        }
        if (!linked) s = tp ? tp[clamp16(e.base_index)] : 0;
        s -= e.timing[clamp16(e.own_index)];
        st[(size_t)i] = s;
        if (run.empty()) {
            run.push_back(i);
            continue;
        }
        const int head = run.front();
        if (st[(size_t)head] > s || (e.type == 6 && st[(size_t)head] == s)) {
            run.push_front(i);
            continue;
        }
        auto prev = run.begin();
        for (;;) {
            auto next = std::next(prev);
            if (next == run.end() || st[(size_t)*next] > s) break;
            if (e.type == 6 && st[(size_t)*prev] == s) break;
            prev = next;
        }
        run.insert(std::next(prev), i);
    }
    int earliest = 0;
    for (int v : st) earliest = std::min(earliest, v);
    if (lead) *lead = -earliest;
    if (starts)
        for (int i = 0; i < n; ++i) starts[i] = st[(size_t)i] - earliest;
    if (order) {
        int k = 0;
        for (int j : run) order[k++] = j;
    }
    return n;
}
}  // namespace

// A dry run of the sequence, for how long its effects last (the Moves preview's timeline for the target's half, which
// the game runs until every entry has ended: the move sweep's battle-camera logs show the half ending well after the
// target's own animation when its particles linger, Flamethrower 326 ticks against the animation's 296). Each entry's
// end follows wazaSequenceEntryUpdate (src/game/wazaSequenceEntry.c; verified from the code): a particle entry runs
// while its node has generators or particles (fn_80118DA8 / fn_80118D84); a screen effect while it runs (a colour fade:
// its keys, a held key counting as ended here since the hold only marks the sequence); a model while its animation
// plays (one frame per battle frame: 2 ticks). Sounds are left out (no lengths in Studio yet), so a half whose sound
// outlasts everything else ends a little earlier than in the game. The particles step once a tick, as Studio's preview
// steps them (two steps per 30 fps battle frame).
extern "C" int orre_wzx_run_ticks(const OrreWzx* w, const int tp[16], int max_ticks) {
    if (!w || max_ticks <= 0) return 0;
    const int n = (int)w->entries.size();
    if (n == 0) return 0;
    std::vector<int> st((size_t)n), ord((size_t)n);
    orre_wzx_schedule(w, tp, st.data(), ord.data());
    OrreParticleSystem* ps = orre_particles_create();
    if (!ps) return 0;
    orre_particles_reset(ps, 1);
    std::vector<const OrreParticleBank*> slots;  // bank -> slot (1-based)
    std::vector<int> nodes;
    int end = 0, last_start = 0;
    for (int i = 0; i < n; ++i) last_start = std::max(last_start, st[(size_t)i]);
    std::vector<char> started((size_t)n, 0);
    int t = 0;
    for (; t < max_ticks; ++t) {
        for (int i : ord) {
            if (started[(size_t)i] || st[(size_t)i] > t) continue;
            started[(size_t)i] = 1;
            const OrreWzxEntry& e = w->entries[(size_t)i];
            if (e.type == 3) {
                const OrreParticleBank* b = orre_wzx_bank(w, i);
                if (!b) continue;
                auto it = std::find(slots.begin(), slots.end(), b);
                int slot = (int)(it - slots.begin()) + 1;
                if (it == slots.end()) {
                    if (slots.size() >= 63) continue;
                    slots.push_back(b);
                    slot = (int)slots.size();
                    orre_particles_set_bank(ps, slot, b);
                }
                const int h = orre_particles_start_link(ps, slot, e.emitter, (int)(e.mode & 1));
                if (h) {
                    orre_particles_node_flags(ps, h, e.mode);
                    nodes.push_back(h);
                }
            } else if (e.type == 4) {
                OrreWzxFade f{};
                int len = 0;
                if (orre_wzx_fade(w, i, &f)) {
                    for (int k = 0; k < f.count; ++k) len += std::max(0, f.key[k].len);
                } else {
                    len = std::max(0, e.effect_frames);
                }
                end = std::max(end, t + len);
            } else if (e.type == 2) {
                OrreModel m{};
                if (e.model_anim >= 0 && orre_wzx_model(w, i, &m)) {
                    if (e.model_anim < m.animation_count)
                        end = std::max(end, t + 2 * (int)std::floor(orre_model_animation_length(&m, e.model_anim)));
                    orre_model_free(&m);
                }
            }
        }
        if ((t & 1) == 0) orre_particles_reclaim(ps);  // (once per 30 fps frame, as gsParticleUpdateAll runs)
        orre_particles_step(ps);
        bool alive = false;
        for (int h : nodes)
            if (orre_particles_node_alive(ps, h)) { alive = true; break; }
        if (!alive && t >= last_start && t >= end) break;
    }
    orre_particles_free(ps);
    return t;
}

// wazaSequenceGetEffectScale (src/game/wazaSequenceEntry.c, constants from battle_waza_sdata2_8047E290.c; verified
// from the code): the size class's scale, then a few moves' own factors on one half (part 1 the user's, 2 the target's)
extern "C" float orre_wzx_effect_scale(int size_class, int move, int part) {
    float s = 1.0f;
    switch (size_class) {
        case -2: s = 0.5f; break;
        case -1: s = 0.75f; break;
        case 1: s = 1.33329999f; break;
        case 2: s = 2.0f; break;
        case 3: s = 3.25f; break;
        default: break;
    }
    switch (move) {
        case 0x8F: if (part == 2) s *= 0.600000024f; break;
        case 0xB1: s *= 0.5f; break;
        case 0xC4: if (part == 1) s *= 0.400000006f; else if (part == 2) s *= 0.5f; break;
        case 0xFA: if (part == 1) s *= 0.625f; else if (part == 2) s *= 0.61500001f; break;
        case 0x119: s *= 0.666999996f; break;
        case 0x133: case 0x13B: if (part == 2) s *= 0.666999996f; break;
        case 0x143: if (part == 2) s *= 0.555000007f; break;
        case 0x149: case 0x162: if (part == 2) s *= 0.713999987f; break;
        default: break;
    }
    return s;
}

// A sequence file by its archive's name (the type-4 sequences that belong to no move: the balls' send-out, return and
// drop-in, status and weather effects; lbl_803727C8), e.g. "wzx_monsterball_downin": the archive's first entry.
extern "C" OrreWzx* orre_wzx_named(OrreDisc* od, const char* name) {
    if (!od || !name || !*name) return nullptr;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    od->d.load_toc();
    const std::string want = name;
    for (const auto& [id, arc] : od->d.toc) {
        const size_t slash = arc.find_last_of('/');
        const std::string base = slash == std::string::npos ? arc : arc.substr(slash + 1);
        if (base != want) continue;
        Bytes a;
        if (!od->d.read_archive(arc, a)) return nullptr;
        const auto entries = fsys_entries(a);
        if (entries.empty()) return nullptr;
        const Bytes w = fsys_load(a, entries.front());
        return orre_wzx_read(w.data(), w.size());
    }
    return nullptr;
}

// The file for a move's part on a Pokémon: its own version first (main.dol lbl_80373750, variant 0), else the move's
// record (lbl_8036E150: part n's {archive, entry} at +4 + 8 (n - 1)); as orre_move_effect (species.cpp) and
// wazaSequenceGetWazaResource. The own-version table holds parts by range (models-animation.md): checked by its
// part field below when present, else part 1 only.
extern "C" OrreWzx* orre_move_wzx(OrreDisc* od, int move, uint32_t model_id, int part) {
    if (!od || move <= 0 || part < 1 || part > 3) return nullptr;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    constexpr uint32_t kMoveEffects = 0x8036E150, kMoveEffectsSize = 0x26F0, kMoveEffectSize = 0x1C;
    constexpr uint32_t kOwnEffects = 0x80373750, kOwnEffectsSize = 0x16F0, kOwnEffectSize = 0x10;
    uint32_t archive_id = 0, entry_id = 0;
    Bytes t;
    // the own versions: each part searches its own range of records, the end excluded (src/game/sequence.c
    // wazaSequenceGetWazaResource, verified from the code: part 1 records 1..0x11B, part 2 0x11E..0x129, part 3
    // 0x12C..0x15D); the first record for this model and move ends the search, used only when it names a file
    static const uint32_t kFirst[3] = {1, 0x11E, 0x12C}, kLast[3] = {0x11C, 0x12A, 0x15E};
    for (uint32_t i = kFirst[part - 1]; i < kLast[part - 1] && i < kOwnEffectsSize / kOwnEffectSize; ++i) {
        if (!od->d.dol_read(kOwnEffects + i * kOwnEffectSize, kOwnEffectSize, t)) break;
        if (be16(t, 2) == model_id && be16(t, 4) == (uint32_t)move && t[0] == 0) {
            archive_id = be32(t, 8);
            entry_id = be32(t, 12);
            if (!archive_id || !entry_id) archive_id = entry_id = 0;
            break;
        }
    }
    if (!archive_id && (uint32_t)move < kMoveEffectsSize / kMoveEffectSize &&
        od->d.dol_read(kMoveEffects + (uint32_t)move * kMoveEffectSize, kMoveEffectSize, t)) {
        archive_id = be32(t, 4 + 8 * (size_t)(part - 1));
        entry_id = be32(t, 8 + 8 * (size_t)(part - 1));
    }
    if (!archive_id) return nullptr;
    Bytes wzx;
    if (!find_entry(od->d, archive_id, entry_id, wzx)) return nullptr;
    return orre_wzx_read(wzx.data(), wzx.size());
}
