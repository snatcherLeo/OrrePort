// hsd_model.cpp: HSD models to triangles in their bind pose (port of tools/local/colo/hsdmodel.py parse_model; format:
// docs/engine/hsd-models.md, layouts include/hsd/hsd_jobj.h, hsd_dobj.h, hsd_pobj.h, hsd_mobj.h, hsd_tobj.h).
// Why: Studio's 3D view shows each Pokémon's battle model; this walks the scene from scene_data through joints, display
// objects, materials, textures and the GX display lists, exactly as the Python reader does, so the golden test can
// compare the two vertex for vertex. Animations, the texture-animation frames and extra texture stages are not read
// yet (the bind pose is the first milestone). Platforms: plain C++, every OS.
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>

#include "internal.hpp"

namespace orre {

namespace {
using M4 = std::array<std::array<double, 4>, 4>;

M4 ident() { return {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}}; }

M4 mul(const M4& a, const M4& b) {
    M4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += a[i][k] * b[k][j];
            r[i][j] = s;
        }
    return r;
}

// HSD joint matrix: translate * rotate (Z * Y * X) * scale, as HSD_MtxSRT (src/hsd/mtx.c) builds it. `ps` is the
// parent's accumulated scale (jobj->scl, null for a root or under a classical-scale chain without one): row i's scale
// for column j is multiplied by ps[j] / ps[i], which cancels the parent's uneven scale where it would shear or stretch
// the child (HSD's default, non-classical scaling). *Added 2026-11-18* (snatcherLeo asked about Crawdaunt's long claws):
// before, Studio composed plain parent x local, so a child under an unevenly scaled parent came out stretched.
M4 srt(const double s[3], const double r[3], const double t[3], const double* ps = nullptr) {
    const double cx = std::cos(r[0]), sx = std::sin(r[0]), cy = std::cos(r[1]), sy = std::sin(r[1]), cz = std::cos(r[2]), sz = std::sin(r[2]);
    const double R[3][3] = {{cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz},
                            {cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz},
                            {-sy, sx * cy, cx * cy}};
    M4 m{};
    for (int i = 0; i < 3; ++i) {
        double k[3] = {s[0], s[1], s[2]};
        if (ps) {
            // HSD_MtxReciprocal: 1 / x (the game's reciprocal estimate; exact enough here)
            const double inv = ps[i] != 0 ? 1.0 / ps[i] : 0.0;
            for (int j = 0; j < 3; ++j) if (j != i) k[j] *= ps[j] * inv;
        }
        m[i] = {R[i][0] * k[0], R[i][1] * k[1], R[i][2] * k[2], t[i]};
    }
    m[3] = {0, 0, 0, 1};
    return m;
}
// jobj->scl after HSD_JObjMakeMatrix (src/hsd/jobj.c): a classical-scale joint (JOBJ_CLASSICAL_SCALE, bit 3) takes
// its parent's, or has none; any other joint has its own scale times its parent's (its own alone at a root).
// Returns false when the joint has no scl.
bool accumulate_scl(uint32_t flags, const double s[3], const double* parent_scl, double out[3]) {
    if (flags & (1u << 3)) {
        if (!parent_scl) return false;
        for (int k = 0; k < 3; ++k) out[k] = parent_scl[k];
        return true;
    }
    for (int k = 0; k < 3; ++k) out[k] = parent_scl ? s[k] * parent_scl[k] : s[k];
    return true;
}

M4 inv(const M4& m) {
    const double a[3][3] = {{m[0][0], m[0][1], m[0][2]}, {m[1][0], m[1][1], m[1][2]}, {m[2][0], m[2][1], m[2][2]}};
    const double t[3] = {m[0][3], m[1][3], m[2][3]};
    const double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                       a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (std::fabs(det) < 1e-12) return ident();
    double c[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            c[i][j] = (a[(j + 1) % 3][(i + 1) % 3] * a[(j + 2) % 3][(i + 2) % 3] - a[(j + 1) % 3][(i + 2) % 3] * a[(j + 2) % 3][(i + 1) % 3]) / det;
    M4 r{};
    for (int i = 0; i < 3; ++i) r[i] = {c[i][0], c[i][1], c[i][2], -(c[i][0] * t[0] + c[i][1] * t[1] + c[i][2] * t[2])};
    r[3] = {0, 0, 0, 1};
    return r;
}

void apply(const M4& m, const double v[3], double w, double out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] + m[i][3] * w;
}

// where the archive's data block starts in Archive.data (parse_hsd_model: the relocation base; offsets handed out of
// this file, such as the texture sources, have it taken off again so they stay offsets into the data block)
constexpr uint32_t kRelocBase = 0x20;

struct Archive {
    Bytes data;
    std::map<std::string, uint32_t> roots;
    std::vector<std::string> root_order;
    uint32_t u32(size_t o) const { return be32(data, o); }
    uint16_t u16(size_t o) const { return be16(data, o); }
    uint8_t b(size_t o) const { return u8(data, o); }
    double f32(size_t o) const { return bef32(data, o); }
    uint32_t ptr(size_t o) const {
        const uint32_t v = u32(o);
        return v > 0 && v < data.size() ? v : 0;
    }
    bool in(size_t o, size_t n) const { return o + n <= data.size(); }
};

// One reference object of a joint (HSD_RObjDesc, include/hsd/hsd_robj.h; loaded as HSD_RObjLoadDesc does): a joint
// reference (IK target, pole, aim), a limit (rotation limits 1-6 stored in degrees, turned to radians), or an IK hint
// (bone length, twist). Only Ralts, Sableye and Exploud carry them (their legs are IK chains; survey 2026-11-18).
struct RObj {
    uint32_t flags = 0;     // bit 31 active, bits 28-30 type (REFTYPE_*), the rest the subtype
    uint32_t target = 0;    // REFTYPE_JOBJ: the referenced joint's descriptor offset, then its index in `joint`
    int joint = -1;
    double value = 0;       // REFTYPE_LIMIT
    double bone = 0, rotate_x = 0;  // REFTYPE_IKHINT
};

// A joint's path (HSD_Spline, include/hsd/hsd_spline.h; a joint with JOBJ_SPLINE keeps it where others keep their
// display objects): type 0 linear, 1 Bezier, 2 B-spline, 3 cardinal; the control points; the total length; each
// segment's start as a fraction of the length; each segment's arc-length polynomial (5 coefficients). An animation's
// PATH track places a joint on it by arc length (splArcLengthPoint, src/hsd/spline.c, ported below). *Added
// 2026-11-21:* Trick's sparkle carrier flies along one (the project's PC-port run)
struct Spline {
    int type = 0, numcv = 0;
    double tension = 0, total = 0;
    std::vector<std::array<double, 3>> cv;
    std::vector<double> seg;
    std::vector<std::array<double, 5>> poly;
};

struct Joint {
    uint32_t offset, flags;
    std::shared_ptr<Spline> spline;  // JOBJ_SPLINE (null: none)
    std::vector<RObj> robj;
    double r[3], s[3], t[3];  // the joint's own rotation, scale, translation (animations replace channels of these)
    M4 world;
    bool has_scl;     // jobj->scl at bind (accumulate_scl)
    double scl[3];
    int parent;
    bool has_inv_bind;
    M4 inv_bind;
    uint32_t dobj;
    bool has_right;
    M4 right;
};

// How a vertex follows the skeleton (filled where the bind pose is computed, so posing reuses the same decision):
// up to 4 joints with weights (HSD envelopes can hold more; the 4 heaviest are kept, renormalised)
struct Influence {
    int joint[4] = {-1, -1, -1, -1};
    float weight[4] = {0, 0, 0, 0};
};

struct Vert {
    double pos[3] = {0, 0, 0};
    bool has_nrm = false;
    double nrm[3] = {0, 0, 0};
    bool has_uv = false;
    double uv[2] = {0, 0};
    double uv1[2] = {0, 0}, uv2[2] = {0, 0}, uv3[2] = {0, 0}, uv4[2] = {0, 0};  // GX_VA_TEX1..TEX4 (attributes 14..17)
    bool has_clr = false;
    float clr[4] = {1, 1, 1, 1};
    Influence infl;
    int spos = -1, snrm = -1;  // a shape-animated polygon object's indices into its shapes (ShapeData)
    float raw[6] = {0, 0, 0, 0, 0, 0};  // position and normal before placing (MeshSkin.raw)
};

// A shape-animated polygon object's shapes (POBJ_SHAPEANIM, HSD_ShapeSet; src/hsd/pobj.c drawShapeAnim, verified from
// the code): nb_shape position sets (and normal sets) over the same index space, kept raw (in the joint's space: the
// additive kind adds shapes as offsets, so they're blended before they're placed), the owning joint's bind placement,
// and every drawn vertex's index into them. The pose blends the shapes: with SHAPESET_AVERAGE (1) one blend value b
// picks shapes floor(b) and floor(b) + 1 and mixes them by the fraction; otherwise (SHAPESET_ADDITIVE) shape 0 plus
// each further shape times its own weight (track type - 2 picks which)
struct ShapeData {
    int flags = 0, nb_shape = 0, npos = 0, nnrm = 0;
    std::vector<double> pos, nrm;  // [shape][index][3], raw
    std::vector<int> vpos, vnrm;   // per drawn vertex (-1: none)
    M4 place{};                    // the owning joint's world matrix at bind (how the plain vertices are placed)
};

// How a mesh's vertices are posed (see pose_model below): RIGID follows its joint (world now x inverse world at
// bind); ENVELOPE blends its joints' skin matrices (world now x inverse-bind); ENVELOPE_NODE does that after the
// owner's node matrix change (right now x inverse right at bind); STATIC doesn't move.
enum SkinKind { SKIN_STATIC = 0, SKIN_RIGID, SKIN_ENVELOPE, SKIN_ENVELOPE_NODE };

// A material as the viewer needs it: HSD_MObjDesc (+0x04 render mode, +0x08 texture list, +0x0C colours, +0x14
// pixel-engine settings) and its texture stages (HSD_TObjDesc), include/hsd/hsd_mobj.h and hsd_tobj.h.
struct Material {
    uint8_t diffuse[4] = {204, 204, 204, 255};
    uint8_t ambient[4] = {0, 0, 0, 255};
    uint8_t specular[4] = {0, 0, 0, 255};
    double alpha = 1.0, shininess = 0.0;
    uint32_t render_mode = 0;
    bool has_pe = false;
    uint8_t pe[12] = {};
    int stage_count = 0;
    OrreTexStage stages[ORRE_MAX_STAGES] = {};
    int dropped_stages = 0;  // decodable stages past ORRE_MAX_STAGES (logged by the caller's stats; none seen yet)
};

constexpr uint32_t JOBJ_SPLINE = 1u << 14, JOBJ_PTCL = 1u << 5, JOBJ_HIDDEN = 0x10;

// The node matrix for any set of joint world matrices (the bind pose, or a posed frame), as
// _HSD_mkEnvelopeModelNodeMtx builds it (src/hsd/displayfunc.c): none for a skeleton root; else, with X the nearest
// skeleton (the joint or an ancestor flagged SKELETON 0x1 / SKELETON_ROOT 0x2): X == owner -> inverse of its
// inverse-bind matrix; X a skeleton root -> inverse(X world) x owner world; otherwise inverse(X world x X inverse-bind)
// x owner world. False when there is none.
bool node_matrix(const std::vector<Joint>& joints, const std::vector<M4>& worlds, int ji, M4& out) {
    const Joint& jt = joints[ji];
    if (jt.flags & 2) return false;
    int xi = ji;
    while (xi >= 0 && !(joints[xi].flags & 3)) xi = joints[xi].parent;
    if (xi < 0) return false;
    const Joint& x = joints[xi];
    if (xi == ji) { out = jt.has_inv_bind ? inv(jt.inv_bind) : worlds[ji]; return true; }
    if (x.flags & 2) { out = mul(inv(worlds[xi]), worlds[ji]); return true; }
    const M4 xb = x.has_inv_bind ? mul(worlds[xi], x.inv_bind) : ident();
    out = mul(inv(xb), worlds[ji]);
    return true;
}

struct Parser {
    Archive a;
    std::vector<Joint> joints;
    std::map<uint32_t, int> by_off;
    std::map<uint32_t, int> tex_index;  // image data offset -> OrreModel texture index
    std::vector<OrreImage> textures;
    std::vector<OrreTextureSource> tex_src;  // per texture: its image and palette in the archive's data block
    std::vector<OrreMesh> meshes;

    // parent_m by value: the joints vector grows during the recursion, so a reference into it would dangle
    void walk(uint32_t j, M4 parent_m, int depth, int parent) {
        while (j && depth < 200 && !by_off.count(j)) {
            const uint32_t flags = a.u32(j + 4);
            const double r[3] = {a.f32(j + 0x14), a.f32(j + 0x18), a.f32(j + 0x1C)};
            const double s[3] = {a.f32(j + 0x20), a.f32(j + 0x24), a.f32(j + 0x28)};
            const double t[3] = {a.f32(j + 0x2C), a.f32(j + 0x30), a.f32(j + 0x34)};
            Joint jt{};
            jt.offset = j;
            jt.flags = flags;
            for (int k = 0; k < 3; ++k) { jt.r[k] = r[k]; jt.s[k] = s[k]; jt.t[k] = t[k]; }
            const double* pscl = parent >= 0 && joints[(size_t)parent].has_scl ? joints[(size_t)parent].scl : nullptr;
            jt.world = mul(parent_m, srt(s, r, t, pscl));
            jt.has_scl = accumulate_scl(flags, s, pscl, jt.scl);
            jt.parent = parent;
            const uint32_t ib = a.ptr(j + 0x38);
            jt.has_inv_bind = ib != 0;
            if (ib) {
                for (int row = 0; row < 3; ++row)
                    for (int c = 0; c < 4; ++c) jt.inv_bind[row][c] = a.f32(ib + 16 * row + 4 * c);
                jt.inv_bind[3] = {0, 0, 0, 1};
            }
            jt.dobj = (flags & (JOBJ_SPLINE | JOBJ_PTCL)) ? 0 : a.ptr(j + 0x10);
            // its path (HSD_Spline: u8 type, s16 numcv +2, f32 tension +4, cv +8, f32 totalLength +0x0C, segLength
            // +0x10 (numcv floats), segPoly +0x14 (5 floats per segment))
            if (flags & JOBJ_SPLINE) {
                const uint32_t sp = a.ptr(j + 0x10);
                if (sp && a.in(sp, 0x18)) {
                    auto s = std::make_shared<Spline>();
                    s->type = a.b(sp);
                    s->numcv = std::clamp((int)(int16_t)a.u16(sp + 2), 0, 4096);
                    s->tension = a.f32(sp + 4);
                    s->total = a.f32(sp + 0x0C);
                    const uint32_t cv = a.ptr(sp + 8), sl = a.ptr(sp + 0x10), sq = a.ptr(sp + 0x14);
                    // (Bezier keeps 3 points per segment plus the last; the rest numcv, cardinal one either side)
                    const int ncv = s->type == 1 ? 3 * s->numcv + 1 : s->numcv + 2;
                    for (int i = 0; cv && i < ncv && a.in(cv + 12 * (uint32_t)i, 12); ++i)
                        s->cv.push_back({a.f32(cv + 12 * i), a.f32(cv + 12 * i + 4), a.f32(cv + 12 * i + 8)});
                    for (int i = 0; sl && i < s->numcv + 1 && a.in(sl + 4 * (uint32_t)i, 4); ++i) s->seg.push_back(a.f32(sl + 4 * i));
                    for (int i = 0; sq && i < s->numcv && a.in(sq + 20 * (uint32_t)i, 20); ++i)
                        s->poly.push_back({a.f32(sq + 20 * i), a.f32(sq + 20 * i + 4), a.f32(sq + 20 * i + 8), a.f32(sq + 20 * i + 12),
                                           a.f32(sq + 20 * i + 16)});
                    if (s->numcv >= 2 && !s->cv.empty()) jt.spline = s;
                }
            }
            // the joint's reference objects (HSD_Joint +0x3C), joint references resolved after the walk
            for (uint32_t rd = a.ptr(j + 0x3C), guard = 0; rd && a.in(rd, 12) && guard < 64; rd = a.ptr(rd), ++guard) {
                RObj r;
                r.flags = a.u32(rd + 4);
                const uint32_t type = r.flags & 0x70000000, sub = r.flags & 0x0FFFFFFF;
                if (type == 0x10000000) r.target = a.ptr(rd + 8);
                else if (type == 0x20000000) r.value = a.f32(rd + 8) * (sub >= 1 && sub <= 6 ? 0.017453292 : 1.0);
                else if (type == 0x40000000) {
                    const uint32_t ik = a.ptr(rd + 8);
                    if (ik && a.in(ik, 8)) { r.bone = a.f32(ik); r.rotate_x = a.f32(ik + 4); }
                }
                jt.robj.push_back(r);
            }
            const int me = (int)joints.size();
            by_off[j] = me;
            joints.push_back(jt);
            walk(a.ptr(j + 8), joints[me].world, depth + 1, me);
            j = a.ptr(j + 0x0C);
        }
    }

    // _HSD_mkEnvelopeModelNodeMtx (src/hsd/displayfunc.c) at the bind pose (hsdmodel.py _envelope_node_mtx)
    void envelope_right(Joint& jt) {
        const int ji = (int)(&jt - joints.data());
        std::vector<M4> worlds;
        for (const auto& j : joints) worlds.push_back(j.world);
        jt.has_right = node_matrix(joints, worlds, ji, jt.right);
    }

    // JOBJ_HIDDEN hides only the joint's OWN display objects: the game's drawing walk skips that joint's DObjs and still
    // descends into its children (fn_80197784 / the walk in src/hsd/hsd_jobj.c; verified 2026-11-18).
    // *Corrected 2026-11-18:* this used to inherit the flag from every ancestor (as colo/hsdmodel.py _hidden does),
    // which hid all of Blaziken (its root joint is hidden; the game draws its children). Animations switch the flag
    // per frame (HSD_A_J_NODE / BRANCH tracks: orre_model_visibility).
    bool hidden(int ji) const { return ji >= 0 && (joints[ji].flags & JOBJ_HIDDEN) != 0; }

    // one TObj's image (HSD_ImageDesc at +0x4C: u32 data, u16 w, u16 h, u32 GX format; TLUT desc at +0x50)
    int decode(uint32_t td) { return decode_image(a.ptr(td + 0x4C), a.ptr(td + 0x50)); }

    // an HSD_ImageDesc (u32 data, u16 w, u16 h, u32 GX format) with its palette description (or 0), decoded once per
    // image (keyed by its pixel data): used by materials' own stages and by texture animations' image tables
    int decode_image(uint32_t img, uint32_t tl) {
        if (!img) return -1;
        const uint32_t data = a.ptr(img);
        const int w = a.u16(img + 4), h = a.u16(img + 6);
        const uint32_t fmt = a.u32(img + 8);
        static const uint32_t kOk[] = {8, 9, 10, 0, 2, 1, 3, 4, 6, 5, 14};
        if (!(data && std::find(std::begin(kOk), std::end(kOk), fmt) != std::end(kOk) && w > 0 && w <= 1024 && h > 0 && h <= 1024)) return -1;
        auto it = tex_index.find(data);
        if (it != tex_index.end()) return it->second;
        std::vector<uint32_t> pal;
        if (fmt == 8 || fmt == 9 || fmt == 10) {
            if (tl) {
                const uint32_t pd = a.ptr(tl), pf = a.u32(tl + 4);
                const int n = a.u16(tl + 12);
                if (pd) pal = decode_tlut(a.data.data() + pd, a.data.size() - pd, (int)(pf <= 2 ? pf : 2), n);
            }
        }
        std::vector<uint8_t> rgba;
        if (!decode_gx(a.data.data() + data, a.data.size() - data, (int)fmt, w, h, pal.empty() ? nullptr : &pal, rgba)) return -1;
        OrreImage im{w, h, static_cast<uint8_t*>(std::malloc(rgba.size()))};
        std::memcpy(im.rgba, rgba.data(), rgba.size());
        const int idx = (int)textures.size();
        textures.push_back(im);
        tex_index[data] = idx;
        // where it came from (orre_model_texture_source; the all-models check compares it with the game's binding)
        // (offsets into the archive's data block: the relocation base taken off)
        tex_src.push_back({data - kRelocBase, fmt, tl && a.ptr(tl) ? a.ptr(tl) - kRelocBase : 0, tl ? a.u32(tl + 4) : 0, w, h});
        return idx;
    }

    // One texture stage (HSD_TObjDesc): its transform as MakeTextureMtx builds it (src/hsd/tobj.c; hsdmodel.py
    // _tex_transform): uv' = (uv + t) * s with s = repeat / scale, t = -translate, and on a mirrored t wrap a shift of
    // 1 / (repeat_t / scale_t). Rotation (+0x10) is not applied: no Pokémon or map texture rotates (hsd-models.md).
    void tex_stage(uint32_t td, int texture, OrreTexStage& st) {
        const double sx = a.f32(td + 0x1C), sy = a.f32(td + 0x20), tx = a.f32(td + 0x28), ty = a.f32(td + 0x2C);
        const double rs = a.b(td + 0x3C) ? a.b(td + 0x3C) : 1, rt = a.b(td + 0x3D) ? a.b(td + 0x3D) : 1;
        st.texture = texture;
        st.wrap_s = (uint8_t)std::min<uint32_t>(a.u32(td + 0x34), 2);
        st.wrap_t = (uint8_t)std::min<uint32_t>(a.u32(td + 0x38), 2);
        st.su = (float)(std::fabs(sx) < 1e-7 ? 0.0 : rs / sx);
        st.sv = (float)(std::fabs(sy) < 1e-7 ? 0.0 : rt / sy);
        const double mirror = st.wrap_t == 2 && st.sv != 0 ? 1.0 / st.sv : 0.0;
        st.tu = (float)-tx;
        st.tv = (float)-(ty + mirror);
        st.rotated = std::fabs(a.f32(td + 0x10)) > 1e-6 || std::fabs(a.f32(td + 0x14)) > 1e-6 || std::fabs(a.f32(td + 0x18)) > 1e-6;
        st.flags = a.u32(td + 0x40);
        st.blending = (float)a.f32(td + 0x44);
        const uint32_t tev = a.ptr(td + 0x58);
        st.has_tev = tev && a.in(tev, 32) ? 1 : 0;
        if (st.has_tev)
            for (int k = 0; k < 32; ++k) st.tev[k] = a.b(tev + k);
        st.map_id = a.u32(td + 0x08);
        st.translate[0] = (float)tx; st.translate[1] = (float)ty;
        st.scale[0] = (float)sx; st.scale[1] = (float)sy;
        st.repeat[0] = a.b(td + 0x3C); st.repeat[1] = a.b(td + 0x3D);
        // the coordinate set (GXTexGenSrc at +0x0C: GX_TG_TEX0 = 4). *Added 2026-11-18:* Studio used to give every
        // stage the first set, which scattered Nuzleaf's, Kirlia's and Gardevoir's face layers (they use TEX1 / TEX2).
        const uint32_t src = a.u32(td + 0x0C);
        st.uv_set = src >= 4 && src <= 11 ? (int)(src - 4) : -1;
    }

    void material(uint32_t mobj, Material& m) {
        m.render_mode = a.u32(mobj + 4);
        const uint32_t md = a.ptr(mobj + 0x0C);
        if (md) {
            for (int k = 0; k < 4; ++k) {
                m.ambient[k] = a.b(md + k);
                m.diffuse[k] = a.b(md + 4 + k);
                m.specular[k] = a.b(md + 8 + k);
            }
            m.alpha = a.f32(md + 0x0C);
            m.shininess = a.in(md, 0x14) ? a.f32(md + 0x10) : 0.0;
        }
        // pixel-engine settings (HSD_PEDesc, 12 bytes); without them HSD_SetupPEMode (src/hsd/hsd_texp.c) uses the
        // render mode's defaults (the viewer applies those)
        const uint32_t pe = a.ptr(mobj + 0x14);
        if (pe && a.in(pe, 12)) {
            m.has_pe = true;
            for (int k = 0; k < 12; ++k) m.pe[k] = a.b(pe + k);
        }
        uint32_t td = a.ptr(mobj + 8);
        for (int guard = 0; td && guard < 16; ++guard) {
            const int t = decode(td);
            if (t >= 0) {
                if (m.stage_count < ORRE_MAX_STAGES) tex_stage(td, t, m.stages[m.stage_count++]);
                else ++m.dropped_stages;
            }
            td = a.ptr(td + 4);
        }
    }

    // one attribute value (hsdmodel.py _read_attr); false when it runs past the data
    bool read_attr(size_t o, int cnt, int comp, int frac, int kind, double* v, int& nv, float* clr) {
        if (kind == 3) {  // colour (GXCompType for colours: 0 RGB565, 1 RGB8, 2 RGBX8, 3 RGBA4, 4 RGBA6, 5 RGBA8),
                          // decoded as hsdmodel.py _read_attr does (RGBX8 keeps its 4th byte as alpha there too)
            static const int sz[] = {2, 3, 4, 2, 3, 4};
            if (comp < 0 || comp > 5 || !a.in(o, sz[comp])) return false;
            if (clr) {
                int c[4] = {255, 255, 255, 255};
                if (comp == 0) {
                    const unsigned x = a.u16(o);
                    c[0] = (x >> 11 & 31) * 255 / 31; c[1] = (x >> 5 & 63) * 255 / 63; c[2] = (x & 31) * 255 / 31;
                } else if (comp == 3) {
                    const unsigned x = a.u16(o);
                    for (int k = 0; k < 4; ++k) c[k] = (x >> (12 - 4 * k) & 15) * 17;
                } else if (comp == 4) {
                    const unsigned x = (unsigned)a.b(o) << 16 | (unsigned)a.b(o + 1) << 8 | a.b(o + 2);
                    for (int k = 0; k < 4; ++k) c[k] = (x >> (18 - 6 * k) & 63) * 255 / 63;
                } else {
                    for (int k = 0; k < (comp == 1 ? 3 : 4); ++k) c[k] = a.b(o + k);
                }
                for (int k = 0; k < 4; ++k) clr[k] = c[k] / 255.0f;
            }
            return true;
        }
        const int n = kind == 0 ? 2 + cnt : kind == 1 ? (cnt ? 3 * (1 + 2 * cnt) : 3) : 1 + cnt;
        static const int csz[] = {1, 1, 2, 2, 4};
        if (comp < 0 || comp > 4 || n <= 0 || n > 9 || !a.in(o, (size_t)n * csz[comp])) return false;
        if (kind == 1) frac = comp == 1 ? 6 : comp == 3 ? 14 : frac;
        for (int i = 0; i < n; ++i) {
            const size_t p = o + (size_t)i * csz[comp];
            double x;
            switch (comp) {
                case 0: x = a.b(p); break;
                case 1: x = (int8_t)a.b(p); break;
                case 2: x = a.u16(p); break;
                case 3: x = (int16_t)a.u16(p); break;
                default: x = a.f32(p); break;
            }
            v[i] = comp == 4 ? x : x / (double)(1u << (frac & 31));
        }
        nv = n;
        (void)clr;
        return true;
    }

    struct Attr { int at, typ, cnt, comp, frac, stride; uint32_t base; };

    // cull: POBJ_CULLFRONT (1 << 14) and POBJ_CULLBACK (1 << 15) of the PObj flags (include/hsd/hsd_forward.h), as
    // HSD_PObjDisp switches on them (src/hsd/pobj.c): 1 front, 2 back, 3 both (the PObj isn't drawn at all)
    void pobj(uint32_t p, int ji, const Material& mat, std::vector<OrreVertex>& out, int& cull, bool& has_colours,
              std::vector<Influence>& infl_out, int& skin, ShapeData* shape_out = nullptr, std::vector<float>* raw_out = nullptr) {
        const uint16_t flags = a.u16(p + 0x0C), ndisp = a.u16(p + 0x0E);
        cull = flags >> 14 & 3;
        has_colours = false;
        const uint32_t disp = a.ptr(p + 0x10);
        const int kind = flags & 0x3000;
        skin = kind == 0x2000 ? (joints[ji].has_right ? SKIN_ENVELOPE_NODE : SKIN_ENVELOPE) : SKIN_RIGID;
        // a shape-animated polygon object (POBJ_SHAPEANIM, 0x1000; its +0x14 is an HSD_ShapeSetDesc: flags, nb_shape,
        // nb_vertex_index, vertex_desc, vertex_idx_list[nb_shape], nb_normal_index, normal_desc, normal_idx_list;
        // get_shape_vertex_xyz / get_shape_normal_xyz in src/hsd/pobj.c): every shape's positions and normals read
        // through their index lists; the drawn vertices' position and normal indices point into them
        std::vector<double> shape0_pos;  // shape 0's raw positions (the rest pose), unplaced
        std::vector<double> shape0_nrm;
        ShapeData* shp = nullptr;
        if (kind == 0x1000 && shape_out) {
            const uint32_t ss = a.ptr(p + 0x14);
            if (ss && a.in(ss, 0x1C)) {
                shp = shape_out;
                shp->flags = a.u16(ss);
                shp->nb_shape = std::min<int>(a.u16(ss + 2), 64);
                shp->npos = std::clamp((int)a.u32(ss + 4), 0, 65536);
                shp->nnrm = std::clamp((int)a.u32(ss + 0x10), 0, 65536);
                // one shape list's values (kind 0 positions, 1 normals: as read_attr takes them), raw
                auto read_set = [&](uint32_t desc, uint32_t idx_list, int count, int kind_, std::vector<double>& raw) {
                    raw.assign((size_t)shp->nb_shape * (size_t)count * 3, 0.0);
                    if (!desc || !idx_list || !a.in(desc, 0x18)) return;
                    const int typ = (int)a.u32(desc + 4), cnt = (int)a.u32(desc + 8), comp = (int)a.u32(desc + 12);
                    const int frac = a.b(desc + 0x10), stride = a.u16(desc + 0x12);
                    const uint32_t base = a.ptr(desc + 0x14);
                    for (int s = 0; s < shp->nb_shape; ++s) {
                        const uint32_t list = a.ptr(idx_list + 4 * (uint32_t)s);
                        if (!list) continue;
                        for (int i = 0; i < count; ++i) {
                            const int idx = typ == 3 ? a.u16(list + 2 * (uint32_t)i) : a.b(list + (uint32_t)i);
                            double v[9];
                            int nv = 0;
                            if (!read_attr((size_t)base + (size_t)idx * (size_t)stride, cnt, comp, frac, kind_, v, nv, nullptr)) continue;
                            for (int c = 0; c < 3; ++c) raw[((size_t)s * count + i) * 3 + c] = c < nv ? v[c] : 0.0;
                        }
                    }
                };
                std::vector<double> rp, rn;
                read_set(a.ptr(ss + 8), a.ptr(ss + 0x0C), shp->npos, 0, rp);
                read_set(a.ptr(ss + 0x14), a.ptr(ss + 0x18), shp->nnrm, 1, rn);
                shape0_pos.assign(rp.begin(), rp.begin() + (ptrdiff_t)std::min(rp.size(), (size_t)shp->npos * 3));
                shape0_nrm.assign(rn.begin(), rn.begin() + (ptrdiff_t)std::min(rn.size(), (size_t)shp->nnrm * 3));
                // kept raw; the pose blends them and then places them by the owning joint at bind, as the drawn
                // vertices are placed (below)
                shp->pos = std::move(rp);
                shp->nrm = std::move(rn);
                shp->place = joints[ji].world;
            }
        }
        std::vector<std::vector<std::pair<int, double>>> envs;
        if (kind == 0x2000) {
            const uint32_t el = a.ptr(p + 0x14);
            for (int i = 0; el && a.ptr(el + 4 * i); ++i) {
                uint32_t e = a.ptr(el + 4 * i);
                std::vector<std::pair<int, double>> parts;
                while (a.ptr(e)) {
                    auto it = by_off.find(a.ptr(e));
                    parts.emplace_back(it == by_off.end() ? -1 : it->second, a.f32(e + 4));
                    e += 8;
                }
                envs.push_back(std::move(parts));
            }
        }
        std::vector<Attr> attrs;
        for (uint32_t v = a.ptr(p + 8); v && a.in(v, 0x18); v += 0x18) {
            const int at = (int)a.u32(v);
            if (at == 0xFF) break;
            attrs.push_back({at, (int)a.u32(v + 4), (int)a.u32(v + 8), (int)a.u32(v + 12), a.b(v + 0x10), a.u16(v + 0x12), a.ptr(v + 0x14)});
        }
        if (!disp || attrs.empty()) return;
        if (std::getenv("ORRE_DEBUG_SKIN")) {  // each polygon object's vertex attributes (GX_VA_*, type, count, format, frac)
            std::string s;
            for (const Attr& at : attrs)
                s += " [va " + std::to_string(at.at) + " typ " + std::to_string(at.typ) + " cnt " + std::to_string(at.cnt) + " comp " +
                     std::to_string(at.comp) + " frac " + std::to_string(at.frac) + " stride " + std::to_string(at.stride) + "]";
            std::fprintf(stderr, "pobj joint %d kind %04X envelopes %zu:%s\n", ji, kind, envs.size(), s.c_str());
        }
        const size_t end = std::min<size_t>(disp + (size_t)ndisp * 32, a.data.size());
        std::map<int, int> slot_use;
        struct SlotReport {  // printed when the part is done (ORRE_DEBUG_SKIN)
            std::map<int, int>& u; int ji;
            ~SlotReport() {
                if (!std::getenv("ORRE_DEBUG_SKIN") || u.empty()) return;
                std::string s;
                for (auto& [k, c] : u) s += " " + std::to_string(k) + "x" + std::to_string(c);
                std::fprintf(stderr, "  matrix slots (PNMTXIDX) used by joint %d's part:%s\n", ji, s.c_str());
            }
        } slot_report{slot_use, ji};
        const Joint& jt = joints[ji];
        // the vertex's joints for posing: the envelope its matrix index picks (index / 3), the 4 heaviest kept
        auto envelope_of = [&](Vert& vx, int mi) {
            if (envs.empty() || mi < 0 || mi / 3 >= (int)envs.size()) return;
            auto parts = envs[mi / 3];
            std::stable_sort(parts.begin(), parts.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
            double sum = 0;
            for (size_t k = 0; k < parts.size() && k < 4; ++k) if (parts[k].first >= 0) sum += parts[k].second;
            for (size_t k = 0, n = 0; k < parts.size() && n < 4; ++k) {
                if (parts[k].first < 0) continue;
                vx.infl.joint[n] = parts[k].first;
                vx.infl.weight[n] = (float)(sum > 0 ? parts[k].second / sum : 0);
                ++n;
            }
        };
        auto place = [&](Vert& vx, int mi) {
            for (int c = 0; c < 3; ++c) { vx.raw[c] = (float)vx.pos[c]; vx.raw[3 + c] = (float)vx.nrm[c]; }
            const M4* m = nullptr;
            if (kind == 0x2000) {
                envelope_of(vx, mi);
                if (jt.has_right) m = &jt.right;
                else if (!envs.empty() && mi >= 0 && mi / 3 < (int)envs.size()) {
                    const auto& parts = envs[mi / 3];
                    if (parts.size() == 1 && parts[0].second >= 1.0 && parts[0].first >= 0) m = &joints[parts[0].first].world;
                }
            } else {
                m = &jt.world;
                vx.infl.joint[0] = ji;
                vx.infl.weight[0] = 1;
            }
            if (!m) return;
            double o[3];
            apply(*m, vx.pos, 1.0, o);
            std::memcpy(vx.pos, o, sizeof o);
            if (vx.has_nrm) {
                // the normal through the inverse transpose (cofactors, the determinant's sign kept), then normalised:
                // as the game's normal matrix treats it. *Corrected 2026-11-19:* through the matrix itself, which bent
                // normals on unevenly scaled joints (Silcoon, Cascoon, Wurmple came out greyer than the game's)
                const M4& M = *m;
                const double c[3][3] = {{M[1][1] * M[2][2] - M[1][2] * M[2][1], -(M[1][0] * M[2][2] - M[1][2] * M[2][0]), M[1][0] * M[2][1] - M[1][1] * M[2][0]},
                                        {-(M[0][1] * M[2][2] - M[0][2] * M[2][1]), M[0][0] * M[2][2] - M[0][2] * M[2][0], -(M[0][0] * M[2][1] - M[0][1] * M[2][0])},
                                        {M[0][1] * M[1][2] - M[0][2] * M[1][1], -(M[0][0] * M[1][2] - M[0][2] * M[1][0]), M[0][0] * M[1][1] - M[0][1] * M[1][0]}};
                const double det = M[0][0] * c[0][0] + M[0][1] * c[0][1] + M[0][2] * c[0][2], s = det < 0 ? -1.0 : 1.0;
                for (int r = 0; r < 3; ++r) o[r] = s * (c[r][0] * vx.nrm[0] + c[r][1] * vx.nrm[1] + c[r][2] * vx.nrm[2]);
                const double len = std::sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
                if (len > 1e-12) for (double& q : o) q /= len;
                std::memcpy(vx.nrm, o, sizeof o);
            }
        };
        // the raw coordinate goes out; each texture stage applies its own transform (OrreTexStage)
        auto emit = [&](const Vert& v) {
            OrreVertex o{};
            for (int k = 0; k < 3; ++k) { o.pos[k] = (float)v.pos[k]; o.nrm[k] = (float)v.nrm[k]; }
            if (v.has_uv) { o.uv[0] = (float)v.uv[0]; o.uv[1] = (float)v.uv[1]; }
            for (int k = 0; k < 2; ++k) { o.uv1[k] = (float)v.uv1[k]; o.uv2[k] = (float)v.uv2[k]; o.uv3[k] = (float)v.uv3[k]; o.uv4[k] = (float)v.uv4[k]; }
            for (int k = 0; k < 4; ++k) o.clr[k] = v.clr[k];
            if (v.has_clr) has_colours = true;
            out.push_back(o);
            infl_out.push_back(v.infl);
            if (shp) {
                shp->vpos.push_back(v.spos);
                shp->vnrm.push_back(v.snrm);
            }
            if (raw_out) raw_out->insert(raw_out->end(), v.raw, v.raw + 6);
        };
        (void)mat;
        size_t q = disp;
        while (q + 3 <= end) {
            const uint8_t op = a.b(q);
            if (op == 0) {
                if (std::getenv("ORRE_DEBUG_PRIMS")) {  // what follows the zero byte, up to the list's end
                    size_t nz = 0, first = 0;
                    for (size_t r = q; r < end; ++r)
                        if (a.b(r)) { if (!nz) first = r - disp; ++nz; }
                    std::fprintf(stderr, "  joint %d list stops at +0x%zX of 0x%zX; %zu non-zero bytes after (first at +0x%zX)\n", ji,
                                 q - disp, end - disp, nz, first);
                }
                break;
            }
            const int prim = op & 0xF8, n = a.u16(q + 1);
            q += 3;
            std::vector<Vert> verts;
            verts.reserve(n);
            bool truncated = false;
            for (int vi = 0; vi < n && !truncated; ++vi) {
                Vert vx;
                int pnmtx = -1;
                for (const Attr& at : attrs) {
                    if (at.at <= 8) {  // GX_VA_PNMTXIDX and TEX0..7MTXIDX: one direct byte each
                        if (at.at == 0) pnmtx = a.b(q);
                        q += 1;
                        continue;
                    }
                    const int k = (at.at == 11 || at.at == 12) ? 3 : at.at == 9 ? 0 : (at.at == 10 || at.at == 25) ? 1 : 2;
                    double val[9];
                    int nv = 0;
                    bool ok;
                    if (at.typ == 1) {  // direct
                        if (k == 3) {
                            static const int sz[] = {2, 3, 4, 2, 3, 4};
                            ok = read_attr(q, at.cnt, at.comp, at.frac, 3, val, nv, at.at == 11 ? vx.clr : nullptr);
                            if (!ok) { truncated = true; break; }
                            if (at.at == 11) vx.has_clr = true;
                            q += sz[at.comp];
                            continue;
                        }
                        static const int csz[] = {1, 1, 2, 2, 4};
                        const int n_ = k == 0 ? 2 + at.cnt : k == 1 ? 3 : 1 + at.cnt;
                        ok = read_attr(q, at.cnt, at.comp, at.frac, k, val, nv, nullptr);
                        if (!ok) { truncated = true; break; }
                        q += (size_t)n_ * csz[std::clamp(at.comp, 0, 4)];
                    } else {
                        int idx;
                        if (at.typ == 2) { idx = a.b(q); q += 1; }
                        else { idx = a.u16(q); q += 2; }
                        if (at.at == 0) { pnmtx = idx; continue; }
                        // (a shape-animated part: its position and normal indices point into the shapes; the rest
                        // pose is shape 0, placed below like any vertex)
                        if (shp && at.at == 9 && idx >= 0 && idx < shp->npos && (size_t)idx * 3 + 2 < shape0_pos.size()) {
                            vx.spos = idx;
                            for (int c = 0; c < 3; ++c) vx.pos[c] = shape0_pos[(size_t)idx * 3 + c];
                            continue;
                        }
                        if (shp && (at.at == 10 || at.at == 25) && idx >= 0 && idx < shp->nnrm && (size_t)idx * 3 + 2 < shape0_nrm.size()) {
                            vx.snrm = idx;
                            vx.has_nrm = true;
                            for (int c = 0; c < 3; ++c) vx.nrm[c] = shape0_nrm[(size_t)idx * 3 + c];
                            continue;
                        }
                        ok = read_attr((size_t)at.base + (size_t)idx * at.stride, at.cnt, at.comp, at.frac, k, val, nv,
                                       at.at == 11 ? vx.clr : nullptr);
                        if (!ok) continue;
                        if (at.at == 11) vx.has_clr = true;
                    }
                    if (at.at == 9) {
                        vx.pos[0] = val[0]; vx.pos[1] = nv > 1 ? val[1] : 0; vx.pos[2] = nv > 2 ? val[2] : 0.0;
                    } else if (at.at == 10 || at.at == 25) {
                        // 25 is GX_VA_NBT: normal, binormal, tangent in one record, on bump-mapped models; its first
                        // three values are the normal. *Corrected 2026-11-19:* only 10 was kept, so those models had zero
                        // normals and went near black under Game lighting (Regirock, Groudon; snatcherLeo). The binormal and
                        // tangent are not kept yet (the emboss bump stage would need them)
                        vx.has_nrm = true;
                        for (int c = 0; c < 3; ++c) vx.nrm[c] = val[c];
                    } else if (at.at == 13) {
                        vx.has_uv = true;
                        vx.uv[0] = val[0];
                        vx.uv[1] = nv > 1 ? val[1] : 0;
                    } else if (at.at >= 14 && at.at <= 17) {
                        double* d = at.at == 14 ? vx.uv1 : at.at == 15 ? vx.uv2 : at.at == 16 ? vx.uv3 : vx.uv4;
                        d[0] = val[0];
                        d[1] = nv > 1 ? val[1] : 0;
                    }
                }
                if (truncated) break;
                if (pnmtx >= 0) ++slot_use[pnmtx];  // (ORRE_DEBUG_SKIN: which matrix slots this part's vertices name)
                place(vx, pnmtx);
                verts.push_back(vx);
            }
            if (truncated) {  // hsdmodel.py: a list that runs past the data ends the whole PObj
                if (std::getenv("ORRE_DEBUG_SKIN"))
                    std::fprintf(stderr, "truncated display list: joint %d, primitive 0x%02X of %d vertices at 0x%zX (list end 0x%zX)\n", ji, prim,
                                 n, q, end);
                break;
            }
            // ORRE_DEBUG_PRIMS=1 (developer switch, any OS): each display-list command (primitive and vertex count), to
            // see how a part's triangles are built (Lunatone's body, 2026-11-19)
            if (std::getenv("ORRE_DEBUG_PRIMS"))
                std::fprintf(stderr, "  joint %d prim 0x%02X x%d at +0x%zX\n", ji, prim, n, q - disp);
            if (const char* skip = std::getenv("ORRE_DEBUG_SKIP_PRIM"))  // e.g. 0x80: leave quads out (same purpose)
                if ((int)std::strtol(skip, nullptr, 0) == prim) continue;
            switch (prim) {
                case 0x90:
                    for (int i = 0; i + 2 < n; i += 3) { emit(verts[i]); emit(verts[i + 1]); emit(verts[i + 2]); }
                    break;
                case 0x98:
                    for (int i = 0; i + 2 < n; ++i) {
                        if (i % 2 == 0) { emit(verts[i]); emit(verts[i + 1]); }
                        else { emit(verts[i + 1]); emit(verts[i]); }
                        emit(verts[i + 2]);
                    }
                    break;
                case 0xA0:
                    for (int i = 1; i + 1 < n; ++i) { emit(verts[0]); emit(verts[i]); emit(verts[i + 1]); }
                    break;
                case 0x80: case 0x88:
                    // (checked 2026-11-19 on Lunatone's rim: the other diagonal and strip order both tear the surface;
                    // this split closes it)
                    for (int i = 0; i + 3 < n; i += 4) {
                        emit(verts[i]); emit(verts[i + 1]); emit(verts[i + 2]);
                        emit(verts[i]); emit(verts[i + 2]); emit(verts[i + 3]);
                    }
                    break;
                default: break;  // lines and points: not drawn
            }
        }
    }
};

// ---- animations ----

// One keyframe track (HSD_FObjDesc, include/hsd/hsd_fobj.h): which value it drives (HSD_A_J_* type), its encoding
// and where its byte stream sits in the archive's data block.
struct Track {
    uint8_t type = 0, frac_value = 0, frac_slope = 0;
    float startframe = 0;
    uint32_t off = 0, length = 0;
    int path_joint = -1;  // HSD_A_J_PATH (4): the joint whose spline the track follows (its AObjDesc's obj_id)
};

// One texture animation (HSD_TexAnim): it drives the texture stage whose GXTexMapID is map_id, in the material of the
// joint's dobj-th display object; images = its image table decoded (OrreModel texture indices, -1 if not decodable)
struct TexAnimEntry {
    int joint = -1, dobj = 0;
    uint32_t map_id = 0;
    std::vector<Track> tracks;
    std::vector<int> images;
};

// one material's colour animation (HSD_MatAnim +0x04: an AObjDesc of HSD_A_M_* tracks) for the display object `dobj`
// of joint `joint`
struct MatAnimEntry {
    int joint = -1, dobj = 0;
    std::vector<Track> tracks;
};

// one polygon object's shape animation (HSD_ShapeAnim +0x04: an AObjDesc whose tracks set the shape blend): the
// pobj-th polygon object of the joint's dobj-th display object
struct ShapeAnimEntry {
    int joint = -1, dobj = 0, pobj = 0;
    std::vector<Track> tracks;
};

struct Animation {
    std::vector<ShapeAnimEntry> shape;        // shape (morph) animations (*added 2026-11-21*: Surf's water waves)
    std::vector<MatAnimEntry> mat;            // material colour animations
    float end_frame = 0;                      // the largest AObjDesc end frame over its joints
    std::vector<std::vector<Track>> joints;   // tracks per joint index
    std::vector<TexAnimEntry> tex;            // the matching material animation's texture animations
};

// HSD's FObj player, ported line by line from src/hsd/fobj.c (HSD_FObjReqAnim, FObjLoadData and its op handlers,
// FObjLoadWait, FObjUpdateAnim, HSD_FObjInterpretAnim) so a pose matches the game's exactly, quirks included (e.g.
// a float-typed value with a non-zero shift reads as 0 without advancing, as parseFloat does).
// States (FOBJ_*): 0 stopped, 1 load first data, 2 load data, 3 load wait, 4 running, 5 running (second pass),
// 6 end of the stream.
struct FObj {
    const uint8_t* ad_head = nullptr;
    size_t pos = 0, length = 0;
    uint8_t op = 0, op_intrp = 0, frac_value = 0, frac_slope = 0;
    uint32_t flags = 0, state = 0, nb_pack = 0;
    float time = 0, fterm = 0, p0 = 0, p1 = 0, d0 = 0, d1 = 0;
    bool has_value = false;
    float value = 0;
    // every value update() produces (the particle tracks: HSD calls the track's callback on each, so two keys on one
    // frame start two emitters; Moltres and Articuno have such pairs: the project's ORRE_WAZA_VIEWER_PTCLHOOK log, verified
    // 2026-11-19). Null: only the last value is kept, as posing needs
    std::vector<float>* sink = nullptr;

    uint8_t byte() { return pos < length ? ad_head[pos++] : (pos++, 0); }
    float parse_float(uint8_t frac) {
        if (frac == 0) {
            uint32_t d = byte();
            d |= (uint32_t)byte() << 8;
            d |= (uint32_t)byte() << 16;
            d |= (uint32_t)byte() << 24;
            float f;
            std::memcpy(&f, &d, 4);
            return f;
        }
        const float denom = (float)(1u << (frac & 0x1F));
        float numer;
        switch (frac & 0xE0) {
            case 0x60: numer = (float)(int8_t)byte(); break;                                   // S8
            case 0x80: numer = (float)byte(); break;                                           // U8
            case 0x20: { const uint8_t lo = byte(), hi = byte(); numer = (float)(int16_t)((int8_t)hi << 8 | lo); break; }  // S16
            case 0x40: { const uint8_t lo = byte(), hi = byte(); numer = (float)(hi << 8 | lo); break; }                   // U16
            default: return 0.0f;
        }
        return numer / denom;
    }
    uint32_t pack_info() {
        uint8_t d = byte();
        uint32_t n = ((d >> 4) & 7) + 1;
        int shift = 3;
        while (d & 0x80) {
            d = byte();
            n += (uint32_t)(d & 0x7F) << shift;
            shift += 7;
        }
        return n;
    }
    int32_t wait() {
        int32_t w = 0, shift = 0;
        uint8_t d;
        do {
            d = byte();
            w |= (int32_t)(d & 0x7F) << shift;
            shift += 7;
        } while ((d & 0x80) && shift < 35);
        return w;
    }
    void launch_key() {
        if (flags & 0x40) {
            op_intrp = op;
            flags &= ~0x40u;
            flags |= 0x80;
            p0 = p1;
        }
    }
    uint32_t next_state(uint32_t st) { return state = (st == 1 ? 3 : 4); }
    uint32_t load_data() {
        if (pos >= length) return 6;
        op_intrp = op;
        if (nb_pack == 0) {
            op = ad_head[pos] & 0xF;  // parseOpCode reads without advancing; parsePackInfo then consumes the byte
            nb_pack = pack_info();
        }
        nb_pack -= 1;
        const uint32_t st = state;
        switch (op) {
            case 1:  // CON
            case 2:  // LIN
                p0 = p1;
                p1 = parse_float(frac_value);
                if (op_intrp != 5) { d0 = d1; d1 = 0.0f; }
                return next_state(st);
            case 3:  // SPL0
                p0 = p1;
                d0 = d1;
                p1 = parse_float(frac_value);
                d1 = 0.0f;
                return next_state(st);
            case 4:  // SPL
                p0 = p1;
                p1 = parse_float(frac_value);
                d0 = d1;
                d1 = parse_float(frac_slope);
                return next_state(st);
            case 5:  // SLP
                d0 = d1;
                d1 = parse_float(frac_slope);
                return state;
            case 6:  // KEY
                launch_key();
                p1 = parse_float(frac_value);
                flags |= 0x40;
                return next_state(st);
            default:
                return state = 0;
        }
    }
    uint32_t load_wait() {
        if (pos >= length) return 6;
        fterm = (float)wait();
        flags |= 0x20;
        return state = 2;
    }
    // splGetHelmite (src/hsd/hsd_texp.c; constants 2.0 and 3.0 read from the US disc): the cubic Hermite curve
    // through p0 -> p1 over fterm frames with slopes d0, d1 (per frame)
    static float hermite(float a, float t, float p0, float p1, float d0, float d1) {
        const float t2 = t * t, a2 = a * a, a2t3 = a2 * t2 * t, at2 = a * t2;
        const float h00 = 2.0f * a2t3 * a - 3.0f * a2 * t2 + 1.0f, h01 = -2.0f * a2t3 * a + 3.0f * a2 * t2;
        const float h11 = a2t3 - at2, h10 = t + a2t3 - 2.0f * at2;
        return p0 * h00 + p1 * h01 + d0 * h10 + d1 * h11;
    }
    void update() {
        switch (op_intrp) {
            case 6:  // KEY: only once, when launched
                if (!(flags & 0x80)) return;
                value = p0;
                flags &= ~0x80u;
                break;
            case 1:
                value = time >= fterm ? p1 : p0;
                break;
            case 2:
                if (flags & 0x20) {
                    flags &= ~0x20u;
                    if (fterm != 0) d0 = (p1 - p0) / fterm;
                    else { d0 = 0; p0 = p1; }
                }
                value = d0 * time + p0;
                break;
            case 3: case 4: case 5:
                value = fterm != 0 ? hermite(1.0f / fterm, time, p0, p1, d0, d1) : p1;
                break;
            default:
                value = 0.0f;  // as the port does for op 0 (src/hsd/fobj.c, TARGET_PC note)
                break;
        }
        has_value = true;
        if (sink) sink->push_back(value);
    }
    void req_anim(float startframe) {
        pos = 0;
        time = startframe;
        op = op_intrp = 0;
        flags &= ~0x40u;
        nb_pack = 0;
        fterm = 0;
        p0 = p1 = d0 = d1 = 0;
        state = 1;
    }
    void interpret(float rate) {
        float ft = 0.0f;
        uint32_t st = state;
        if (st == 0) return;
        time += rate;
        if (time < 0.0f) return;
        for (int guard = 0; guard < 100000; ++guard) {
            switch (st) {
                case 6:
                    time += ft;
                    launch_key();
                    update();
                    return;
                case 1: case 2:
                    st = load_data();
                    break;
                case 3:
                    if (flags & 0x80) update();
                    st = load_wait();
                    break;
                case 4:
                    if (fterm <= time) {
                        st = 3;
                        ft = fterm;
                        time -= fterm;
                        state = st;
                        break;
                    }
                    update();
                    state = 5;
                    return;
                case 5:
                    st = 4;
                    state = st;
                    break;
                default:
                    return;
            }
        }
    }
};

// what posing needs, kept with the model (OrreModel.internal)
struct MeshSkin {
    int kind = SKIN_STATIC;
    int owner = -1;  // the joint the polygon object hangs from
    int dobj = 0;    // which of that joint's display objects (material animations are matched by it)
    int pobj = 0;    // which polygon object of that display object (shape animations are matched by it)
    std::vector<Influence> infl;
    std::shared_ptr<ShapeData> shape;  // a shape-animated polygon object's shapes (null: none)
    // a rigid part whose joint is scaled to nothing at bind (its world matrix has no inverse): its vertices in the
    // joint's own space (position and normal, 6 floats each), posed by the joint's matrix now directly. Placing them
    // at bind collapses them to a point, and "now x inverse(bind)" can't bring them back. *Added 2026-11-21:* Mind
    // Reader's lock-on rings (joints 4-6 rest at scale 0, grown by the animation) never showed. The game itself always
    // draws a rigid part with its joint's current matrix (HSD_JObjSetupMatrix), so this is its own way, used only
    // where Studio's bind placement fails
    std::vector<float> raw;
};

struct ModelInternal {
    Bytes data;  // the archive's data block (the tracks' byte streams point into it)
    std::vector<Joint> joints;
    std::vector<MeshSkin> skins;  // per OrreMesh
    std::vector<Animation> anims;
    int colour_anims = 0, render_anims = 0;  // counted by read_tex_animations (orre_model_survey)
    int dropped_stages = 0;                  // stages past ORRE_MAX_STAGES, summed over the meshes
    std::vector<OrreTextureSource> tex_src;  // per OrreModel texture (orre_model_texture_source)
};

// the joint-animation trees of model set 0 (scene_data -> set +0x04: a null-terminated list of HSD_AnimJoint roots),
// matched to the joint tree in the same child / next order HSD_JObjAddAnimAll walks them
void read_animations(const Parser& P, uint32_t set, uint32_t root_joint, ModelInternal& mi) {
    const uint32_t list = P.a.ptr(set + 4);
    for (int k = 0; list && P.a.ptr(list + 4 * k) && k < 256; ++k) {
        Animation an;
        an.joints.resize(P.joints.size());
        std::vector<std::pair<uint32_t, uint32_t>> stack = {{root_joint, P.a.ptr(list + 4 * k)}};
        int guard = 0;
        while (!stack.empty() && ++guard < 100000) {
            auto [j, aj] = stack.back();
            stack.pop_back();
            if (!j || !aj) continue;
            auto it = P.by_off.find(j);
            const uint32_t aobj = P.a.ptr(aj + 8);
            if (it != P.by_off.end() && aobj) {
                an.end_frame = std::max(an.end_frame, (float)P.a.f32(aobj + 4));
                // the AObjDesc's obj_id (+0x0C): the joint its PATH track follows (HSD_AObjSetHsdObj; jobj.c's
                // HSD_A_J_PATH case reads jobj->aobj->hsd_obj's spline)
                const auto pj = P.by_off.find(P.a.ptr(aobj + 0x0C));
                const int path_joint = pj != P.by_off.end() ? pj->second : -1;
                for (uint32_t f = P.a.ptr(aobj + 8), g2 = 0; f && g2 < 64; f = P.a.ptr(f), ++g2) {
                    Track t;
                    t.length = P.a.u32(f + 4);
                    t.startframe = (float)P.a.f32(f + 8);
                    t.type = P.a.b(f + 0x0C);
                    t.frac_value = P.a.b(f + 0x0D);
                    t.frac_slope = P.a.b(f + 0x0E);
                    t.off = P.a.ptr(f + 0x10);
                    if (t.type == 4) t.path_joint = path_joint;
                    if (t.off && t.off + (size_t)t.length <= P.a.data.size()) an.joints[it->second].push_back(t);
                }
            }
            stack.push_back({P.a.ptr(j + 0x0C), P.a.ptr(aj + 4)});  // next sibling
            stack.push_back({P.a.ptr(j + 0x08), P.a.ptr(aj + 0)});  // child (walked first)
        }
        // ORRE_DEBUG_HSD_ROOTS=1 also lists each joint's animation tracks (types), for the carrier models (2026-11-21)
        if (std::getenv("ORRE_DEBUG_HSD_ROOTS")) {
            std::fprintf(stderr, "[hsd] animation %d: end %.1f\n", k, an.end_frame);
            for (size_t ji = 0; ji < an.joints.size(); ++ji) {
                if (an.joints[ji].empty()) continue;
                std::fprintf(stderr, "[hsd]   joint %zu:", ji);
                for (const Track& t : an.joints[ji]) std::fprintf(stderr, " type %d (%u bytes)", t.type, t.length);
                std::fprintf(stderr, "\n");
            }
        }
        mi.anims.push_back(std::move(an));
    }
}
// an AObjDesc's chain of FObjDesc tracks
std::vector<Track> read_tracks(const Parser& P, uint32_t aobj) {
    std::vector<Track> out;
    for (uint32_t f = aobj ? P.a.ptr(aobj + 8) : 0, g = 0; f && g < 64; f = P.a.ptr(f), ++g) {
        Track t;
        t.length = P.a.u32(f + 4);
        t.startframe = (float)P.a.f32(f + 8);
        t.type = P.a.b(f + 0x0C);
        t.frac_value = P.a.b(f + 0x0D);
        t.frac_slope = P.a.b(f + 0x0E);
        t.off = P.a.ptr(f + 0x10);
        if (t.off && t.off + (size_t)t.length <= P.a.data.size()) out.push_back(t);
    }
    return out;
}

// The material animations of model set 0 (set +0x08: a null-terminated list of HSD_MatAnimJoint trees, the k-th
// belonging to the k-th joint animation). Each tree is walked alongside the joint tree (child +0x00, next +0x04);
// at each joint its HSD_MatAnim list (+0x08; next +0x00) pairs with the joint's display objects in order, and each
// MatAnim's HSD_TexAnim list (+0x08: next +0x00, GXTexMapID +0x04, AObjDesc +0x08, image table +0x0C, palette table
// +0x10, u16 counts +0x14 / +0x16) drives the stage with that map id (include/hsd/hsd_mobj.h, hsd_tobj.h). The image
// tables are decoded now, each image with the palette of the same index or the last one (as colo/hsdmodel.py does).
// the shape-animation trees of model set 0 (scene_data -> set +0x0C: a null-terminated list of HSD_ShapeAnimJoint roots,
// one per animation), walked in step with the joint tree (child +0x00 / next +0x04 against the joint's +0x08 / +0x0C):
// each joint's HSD_ShapeAnimDObj list (+0x08; next +0x00, shape animations +0x04) runs beside its display objects, and
// each HSD_ShapeAnim list (next +0x00, AObjDesc +0x04) beside that display object's polygon objects
// (HSD_JObjAddAnimAll / HSD_DObjAddAnimAll / HSD_PObjAddAnimAll, verified from the code). *Added 2026-11-21:* Surf's
// water is a shape-animated wave mesh (the project's RenderDoc capture: interpretShapeAnimDisplayList)
void read_shape_animations(Parser& P, uint32_t set, uint32_t root_joint, ModelInternal& mi) {
    const uint32_t list = P.a.ptr(set + 0x0C);
    for (int k = 0; list && P.a.ptr(list + 4 * k) && k < 256; ++k) {
        if (k >= (int)mi.anims.size()) {
            Animation an;
            an.joints.resize(P.joints.size());
            mi.anims.push_back(std::move(an));
        }
        Animation& an = mi.anims[k];
        std::vector<std::pair<uint32_t, uint32_t>> stack = {{root_joint, P.a.ptr(list + 4 * k)}};
        int guard = 0;
        while (!stack.empty() && ++guard < 100000) {
            auto [j, sj] = stack.back();
            stack.pop_back();
            if (!j || !sj) continue;
            auto it = P.by_off.find(j);
            if (it != P.by_off.end()) {
                int dobj = 0;
                for (uint32_t sd = P.a.ptr(sj + 8); sd && dobj < 512; sd = P.a.ptr(sd), ++dobj) {
                    int pobj = 0;
                    for (uint32_t sa = P.a.ptr(sd + 4); sa && pobj < 512; sa = P.a.ptr(sa), ++pobj) {
                        const uint32_t ao = P.a.ptr(sa + 4);
                        if (!ao) continue;
                        ShapeAnimEntry e;
                        e.joint = it->second;
                        e.dobj = dobj;
                        e.pobj = pobj;
                        e.tracks = read_tracks(P, ao);
                        an.end_frame = std::max(an.end_frame, (float)P.a.f32(ao + 4));
                        if (!e.tracks.empty()) an.shape.push_back(std::move(e));
                    }
                }
            }
            stack.push_back({P.a.ptr(j + 0x0C), P.a.ptr(sj + 4)});
            stack.push_back({P.a.ptr(j + 0x08), P.a.ptr(sj + 0)});
        }
    }
}

void read_tex_animations(Parser& P, uint32_t set, uint32_t root_joint, ModelInternal& mi) {
    const uint32_t list = P.a.ptr(set + 8);
    for (int k = 0; list && P.a.ptr(list + 4 * k) && k < 256; ++k) {
        if (k >= (int)mi.anims.size()) {
            Animation an;
            an.joints.resize(P.joints.size());
            mi.anims.push_back(std::move(an));
        }
        Animation& an = mi.anims[k];
        std::vector<std::pair<uint32_t, uint32_t>> stack = {{root_joint, P.a.ptr(list + 4 * k)}};
        int guard = 0;
        while (!stack.empty() && ++guard < 100000) {
            auto [j, mj] = stack.back();
            stack.pop_back();
            if (!j || !mj) continue;
            auto it = P.by_off.find(j);
            int dobj = 0;
            for (uint32_t ma = P.a.ptr(mj + 8); ma && it != P.by_off.end() && dobj < 512; ma = P.a.ptr(ma), ++dobj) {
                if (const uint32_t ao = P.a.ptr(ma + 4)) {
                    ++mi.colour_anims;
                    MatAnimEntry me;
                    me.joint = it->second;
                    me.dobj = dobj;
                    me.tracks = read_tracks(P, ao);
                    an.end_frame = std::max(an.end_frame, (float)P.a.f32(ao + 4));
                    an.mat.push_back(std::move(me));
                }
                if (P.a.ptr(ma + 0x0C)) ++mi.render_anims;
                for (uint32_t ta = P.a.ptr(ma + 8), g = 0; ta && g < 64; ta = P.a.ptr(ta), ++g) {
                    TexAnimEntry e;
                    e.joint = it->second;
                    e.dobj = dobj;
                    e.map_id = P.a.u32(ta + 4);
                    const uint32_t ao = P.a.ptr(ta + 8);
                    if (ao) an.end_frame = std::max(an.end_frame, (float)P.a.f32(ao + 4));
                    e.tracks = read_tracks(P, ao);
                    const uint32_t itb = P.a.ptr(ta + 0x0C), ttb = P.a.ptr(ta + 0x10);
                    const int n_img = P.a.u16(ta + 0x14), n_tl = P.a.u16(ta + 0x16);
                    for (int i = 0; i < n_img && i < 256; ++i) {
                        const uint32_t img = itb ? P.a.ptr(itb + 4 * i) : 0;
                        const uint32_t tl = ttb && n_tl ? P.a.ptr(ttb + 4 * std::min(i, n_tl - 1)) : 0;
                        e.images.push_back(P.decode_image(img, tl));
                    }
                    an.tex.push_back(std::move(e));
                }
            }
            stack.push_back({P.a.ptr(j + 0x0C), P.a.ptr(mj + 4)});
            stack.push_back({P.a.ptr(j + 0x08), P.a.ptr(mj + 0)});
        }
    }
}
}  // namespace

bool parse_hsd_model(const Bytes& dat, OrreModel& out) {
    out = OrreModel{};
    if (dat.size() < 0x20) return false;
    Parser P;
    const uint32_t dsize = be32(dat, 4), nrel = be32(dat, 8), nroot = be32(dat, 12), nref = be32(dat, 16);
    if (0x20 + (size_t)dsize > dat.size()) return false;
    // The data block, relocated as HSD_ArchiveParse does: every field the relocation table lists is a pointer, whatever
    // it holds, 0 included (a stored 0 then points at data offset 0). Studio reads 0 as "none" everywhere, so the data
    // is placed kRelocBase bytes in and every listed field gets kRelocBase added, the way the game adds the archive's
    // load address: a listed 0 becomes a real pointer to the start, an unlisted 0 stays null. *Added 2026-11-21:* 15
    // moves' effect models (Trick, Splash, Encore...) keep their root joint at data offset 0 behind such a field;
    // Studio read them as having no joints (the project's PC-port run: renderJObj is a real joint; verified)
    P.a.data.assign(kRelocBase, 0);
    P.a.data.insert(P.a.data.end(), dat.begin() + 0x20, dat.begin() + 0x20 + dsize);
    for (uint32_t i = 0; i < nrel && 0x20 + (size_t)dsize + 4 * (i + 1) <= dat.size(); ++i) {
        const size_t f = kRelocBase + be32(dat, 0x20 + (size_t)dsize + 4 * i);
        if (f + 4 > P.a.data.size()) continue;
        const uint32_t v = be32(P.a.data, f) + kRelocBase;
        for (int k = 0; k < 4; ++k) P.a.data[f + (size_t)k] = (uint8_t)(v >> (24 - 8 * k));
    }
    const size_t rt = 0x20 + (size_t)dsize + 4 * (size_t)nrel, st = rt + 8 * ((size_t)nroot + nref);
    for (uint32_t i = 0; i < nroot && rt + 8 * (i + 1) <= dat.size(); ++i) {
        const uint32_t off = be32(dat, rt + 8 * i) + kRelocBase, so = be32(dat, rt + 8 * i + 4);
        if (st + so >= dat.size()) continue;
        const char* s = reinterpret_cast<const char*>(dat.data() + st + so);
        std::string name(s, strnlen(s, dat.size() - st - so));
        if (!P.a.roots.count(name)) P.a.root_order.push_back(name);
        P.a.roots[name] = off;
    }
    // ORRE_DEBUG_HSD_ROOTS=1 (developer switch, any OS): print each archive's root names (how the move models with no
    // meshes, Splash's mover, were looked into, 2026-11-21)
    if (std::getenv("ORRE_DEBUG_HSD_ROOTS")) {
        for (const auto& n : P.a.root_order) std::fprintf(stderr, "[hsd] root %s at 0x%X (data %u bytes)\n", n.c_str(), P.a.roots[n], dsize);
        if (P.a.roots.count("scene_data")) {
            const uint32_t sd = P.a.roots["scene_data"], sets = P.a.ptr(sd);
            std::fprintf(stderr, "[hsd] scene_data words %08X %08X %08X %08X; sets 0x%X -> set 0x%X -> joint 0x%X, anim 0x%X\n",
                         P.a.u32(sd), P.a.u32(sd + 4), P.a.u32(sd + 8), P.a.u32(sd + 12), sets, sets ? P.a.ptr(sets) : 0,
                         sets && P.a.ptr(sets) ? P.a.ptr(P.a.ptr(sets)) : 0, sets && P.a.ptr(sets) ? P.a.ptr(P.a.ptr(sets) + 4) : 0);
        }
    }
    std::vector<uint32_t> roots;
    if (P.a.roots.count("scene_data")) {
        const uint32_t sets = P.a.ptr(P.a.roots["scene_data"]);
        for (int k = 0; sets && P.a.ptr(sets + 4 * k); ++k) {
            const uint32_t js = P.a.ptr(P.a.ptr(sets + 4 * k));
            if (js) roots.push_back(js);
        }
    } else {
        for (const auto& n : P.a.root_order)
            if (n.size() >= 6 && n.compare(n.size() - 6, 6, "_joint") == 0) roots.push_back(P.a.roots[n]);
    }
    for (uint32_t r : roots) P.walk(r, ident(), 0, -1);
    for (auto& jt : P.joints)
        for (auto& r : jt.robj)
            if (r.target) {
                auto it = P.by_off.find(r.target);
                r.joint = it == P.by_off.end() ? -1 : it->second;
            }
    for (auto& jt : P.joints) P.envelope_right(jt);
    auto* mi = new ModelInternal();
    if (P.a.roots.count("scene_data") && !roots.empty()) {
        const uint32_t sets = P.a.ptr(P.a.roots["scene_data"]);
        if (sets && P.a.ptr(sets)) {
            read_animations(P, P.a.ptr(sets), roots[0], *mi);
            read_tex_animations(P, P.a.ptr(sets), roots[0], *mi);
            read_shape_animations(P, P.a.ptr(sets), roots[0], *mi);
        }
    }

    for (int ji = 0; ji < (int)P.joints.size(); ++ji) {
        uint32_t d = P.joints[ji].dobj;
        for (int guard = 0; d && guard < 512; ++guard) {
            Material mat;
            const uint32_t mobj = P.a.ptr(d + 8);
            if (mobj) P.material(mobj, mat);
            mi->dropped_stages += mat.dropped_stages;
            int pobj_index = -1;
            for (uint32_t p = P.a.ptr(d + 0x0C); p; p = P.a.ptr(p + 4)) {
                ++pobj_index;  // (its place in the display object's list: shape animations are matched by it)
                std::vector<OrreVertex> v;
                std::vector<Influence> infl;
                int cull = 0, skin = SKIN_STATIC;
                bool colours = false;
                auto shape = std::make_shared<ShapeData>();
                std::vector<float> raw;
                P.pobj(p, ji, mat, v, cull, colours, infl, skin, shape.get(), &raw);
                if (v.empty()) continue;
                MeshSkin ms;
                ms.kind = skin;
                // (a rigid part whose joint has no inverse at bind keeps its unplaced vertices: MeshSkin.raw)
                if (skin == SKIN_RIGID && raw.size() == v.size() * 6) {
                    const M4& W = P.joints[ji].world;
                    const double det = W[0][0] * (W[1][1] * W[2][2] - W[1][2] * W[2][1]) - W[0][1] * (W[1][0] * W[2][2] - W[1][2] * W[2][0]) +
                                       W[0][2] * (W[1][0] * W[2][1] - W[1][1] * W[2][0]);
                    if (std::fabs(det) < 1e-12) ms.raw = std::move(raw);
                    // ORRE_DEBUG_ZEROBIND=1 (developer switch, any OS): name every such part (how the moves with
                    // rings grown from nothing were listed, 2026-11-21)
                    if (!ms.raw.empty() && std::getenv("ORRE_DEBUG_ZEROBIND"))
                        std::fprintf(stderr, "zero-scale bind: joint %d, %zu vertices\n", ji, v.size());
                }
                ms.owner = ji;
                ms.dobj = guard;  // the loop counts the joint's display objects in list order
                ms.pobj = pobj_index;
                if (shape->nb_shape > 0 && shape->vpos.size() == v.size()) ms.shape = shape;
                // ORRE_DEBUG_SHAPES=1 (developer switch, any OS): name every shape-animated part as it loads (how the
                // moves with morphing effect models were listed, 2026-11-21)
                if (shape->nb_shape > 0 && std::getenv("ORRE_DEBUG_SHAPES"))
                    std::fprintf(stderr, "shape set: joint %d dobj %d pobj %d, %d shapes, flags %d, %d positions, %d drawn vertices%s\n",
                                 ji, guard, pobj_index, shape->nb_shape, shape->flags, shape->npos, (int)v.size(),
                                 ms.shape ? "" : " (index mismatch: not used)");
                ms.infl = std::move(infl);
                mi->skins.push_back(std::move(ms));
                OrreMesh m{};
                m.texture = mat.stage_count ? mat.stages[0].texture : -1;
                std::memcpy(m.diffuse, mat.diffuse, 4);
                std::memcpy(m.ambient, mat.ambient, 4);
                m.alpha = (float)mat.alpha;
                std::memcpy(m.specular, mat.specular, 4);
                m.shininess = (float)mat.shininess;
                m.render_mode = mat.render_mode;
                m.has_pe = mat.has_pe ? 1 : 0;
                std::memcpy(m.pe, mat.pe, 12);
                m.cull = cull;
                m.has_colours = colours ? 1 : 0;
                m.stage_count = mat.stage_count;
                std::memcpy(m.stages, mat.stages, sizeof m.stages);
                m.hidden = P.hidden(ji) ? 1 : 0;
                // a glow card: a billboard joint's part drawn blended without depth writes (OrreMesh.glow)
                m.glow = (P.joints[ji].flags & (0xE00u | 0x2000u)) && (mat.render_mode & 0x60000000u) == 0x60000000u ? 1 : 0;
                m.vertex_count = (int)v.size();
                m.vertices = static_cast<OrreVertex*>(std::malloc(v.size() * sizeof(OrreVertex)));
                std::memcpy(m.vertices, v.data(), v.size() * sizeof(OrreVertex));
                P.meshes.push_back(m);
            }
            d = P.a.ptr(d + 4);
        }
    }
    out.joint_count = (int)P.joints.size();
    mi->data = P.a.data;
    mi->joints = P.joints;
    mi->tex_src = P.tex_src;
    out.animation_count = (int)mi->anims.size();
    out.idle_animation = mi->anims.empty() ? -1 : 0;  // the PKX's slot table refines this (orre_species_model)
    out.internal = mi;
    out.mesh_count = (int)P.meshes.size();
    out.meshes = static_cast<OrreMesh*>(std::malloc(std::max<size_t>(1, P.meshes.size()) * sizeof(OrreMesh)));
    if (!P.meshes.empty()) std::memcpy(out.meshes, P.meshes.data(), P.meshes.size() * sizeof(OrreMesh));
    out.texture_count = (int)P.textures.size();
    out.textures = static_cast<OrreImage*>(std::malloc(std::max<size_t>(1, P.textures.size()) * sizeof(OrreImage)));
    if (!P.textures.empty()) std::memcpy(out.textures, P.textures.data(), P.textures.size() * sizeof(OrreImage));
    // the bounds of the parts shown in the standing pose; if none are shown, of every part
    for (int pass = 0; pass < 2; ++pass) {
        for (int k = 0; k < 3; ++k) { out.bounds_min[k] = 1e30f; out.bounds_max[k] = -1e30f; }
        bool any = false;
        for (const auto& m : P.meshes) {
            if (m.hidden && pass == 0) continue;
            for (int i = 0; i < m.vertex_count; ++i, any = true)
                for (int k = 0; k < 3; ++k) {
                    out.bounds_min[k] = std::min(out.bounds_min[k], m.vertices[i].pos[k]);
                    out.bounds_max[k] = std::max(out.bounds_max[k], m.vertices[i].pos[k]);
                }
        }
        if (any) break;
    }
    // (a model with no meshes but joints and an animation is a real one too: a move's carrier, whose joints carry
    // linked particles (Trick) or the Pokémon itself (Splash); the project's PC-port run, 2026-11-21, verified)
    return out.mesh_count > 0 || (!mi->joints.empty() && out.animation_count > 0);
}

}  // namespace orre

using namespace orre;

// splGetSplinePoint (src/hsd/spline.c, verified from the code): the point at parameter u (0..1) of a spline
static void spline_point(const Spline& s, double u, double p[3]) {
    if (u < 0.0 || u > 1.0) return;
    auto cv = [&](int i) -> const std::array<double, 3>& {
        static const std::array<double, 3> zero{0, 0, 0};
        return i >= 0 && i < (int)s.cv.size() ? s.cv[(size_t)i] : zero;
    };
    auto mix4 = [&](int i0, const double w[4]) {
        for (int c = 0; c < 3; ++c) p[c] = cv(i0)[c] * w[0] + cv(i0 + 1)[c] * w[1] + cv(i0 + 2)[c] * w[2] + cv(i0 + 3)[c] * w[3];
    };
    auto bspline = [&](int i0, double t) {
        const double t2 = t * t, t3 = t2 * t, m = 1.0 - t, k = 1.0 / 6.0;
        const double w[4] = {k * m * m * m, k * (4.0 + (3.0 * t3 - 6.0 * t2)), k * (3.0 * (-t3 + t2 + t) + 1.0), k * t3};
        mix4(i0, w);
    };
    if (u < 1.0) {
        u *= s.numcv - 1;
        const int idx = (int)u;
        u -= idx;
        const double u2 = u * u, u3 = u2 * u, m = 1.0 - u;
        switch (s.type) {
            case 0:
                for (int c = 0; c < 3; ++c) p[c] = u * (cv(idx + 1)[c] - cv(idx)[c]) + cv(idx)[c];
                return;
            case 1: {
                const double w[4] = {m * m * m, 3.0 * u * m * m, 3.0 * u2 * m, u3};
                mix4(idx * 3, w);
                return;
            }
            case 2: bspline(idx, u); return;
            case 3: {
                const double t = s.tension;
                const double w[4] = {t * (-u3 + 2.0 * u2 - u), (2.0 - t) * u3 + (t - 3.0) * u2 + 1.0,
                                     (t - 2.0) * u3 + (3.0 - 2.0 * t) * u2 + t * u, t * (u3 - u2)};
                mix4(idx, w);
                return;
            }
        }
    } else {
        const int idx = s.numcv - 1;
        switch (s.type) {
            case 0: for (int c = 0; c < 3; ++c) p[c] = cv(idx)[c]; return;
            case 1: for (int c = 0; c < 3; ++c) p[c] = cv(idx * 3)[c]; return;
            case 2: bspline(idx - 1, 1.0); return;
            case 3: for (int c = 0; c < 3; ++c) p[c] = cv(idx + 1)[c]; return;
        }
    }
}

// splArcLengthGetParameter + splArcLengthPoint (src/hsd/spline.c, verified from the code): the point a fraction u of
// the way along the spline by length (each segment's length by Simpson's rule on its arc-length polynomial, bisected)
static void spline_arc_point(const Spline& s, double u, double p[3]) {
    auto seg = [&](int i) { return i >= 0 && i < (int)s.seg.size() ? s.seg[(size_t)i] : 1.0; };
    double par;
    if (u <= 0.0) {
        par = 0.0;
    } else if (u >= 1.0) {
        par = 1.0;
    } else {
        int idx = 0;
        while (idx + 1 < (int)s.seg.size() && seg(idx + 1) < u) ++idx;
        double result = 0;
        if (s.type == 0) {
            const double d = seg(idx + 1) - seg(idx);
            result = d != 0 ? (u - seg(idx)) / d : 0.0;
        } else if (idx < (int)s.poly.size()) {
            const auto& c = s.poly[(size_t)idx];
            auto poly = [&](double t) {
                const double t2 = t * t, t3 = t2 * t, t4 = t3 * t;
                double r = c[0] * t4 + c[1] * t3 + c[2] * t2 + c[3] * t + c[4];
                if (r < 0.0 && r > -0.001) r = 0.0;
                return std::sqrt(std::max(0.0, r));
            };
            auto simpson = [&](double a0, double a1) {  // fn_801B1AD0
                const double dx = (a1 - a0) / 8.0;
                double t = a0 + dx, mid = 0;
                for (int i = 2; i <= 8; ++i, t += dx) mid += ((i & 1) ? 2.0 : 4.0) * poly(t);
                return dx * (mid + poly(a0) + poly(a1)) / 3.0;
            };
            double want = s.total * (u - seg(idx)), lo = 0, hi = 1;
            for (int guard = 0; std::fabs(lo - hi) >= 0.00001 && guard < 64; ++guard) {
                result = (lo + hi) / 2.0;
                const double part = simpson(lo, result);
                if (want < 0.00001 + part) hi = result;
                else { lo = result; want -= part; }
            }
        }
        par = (result + idx) / (s.numcv - 1.0);
    }
    spline_point(s, par, p);
}

// one joint's local rotation / scale / translation at a frame: its own values, with each track that produced a value
// replacing its channel (HSD_A_J_ROTX..ROTZ 1-3, TRAX..TRAZ 5-7, SCAX..SCAZ 8-10, as the JObj's animation update
// writes them; PATH (4) sets the translation to the point that far along its path joint's spline, clamped to 0..1,
// as jobj.c's HSD_A_J_PATH case does; the others are not applied)
static void joint_local(const ModelInternal& mi, const Animation* an, size_t i, float frame, double* r, double* s, double* t) {
    const Joint& J = mi.joints[i];
    for (int k = 0; k < 3; ++k) { r[k] = J.r[k]; s[k] = J.s[k]; t[k] = J.t[k]; }
    if (!an) return;
    for (const Track& tr : an->joints[i]) {
        FObj f;
        f.ad_head = mi.data.data() + tr.off;
        f.length = tr.length;
        f.frac_value = tr.frac_value;
        f.frac_slope = tr.frac_slope;
        f.req_anim(tr.startframe);
        f.interpret(frame);
        if (!f.has_value) continue;
        switch (tr.type) {
            case 1: case 2: case 3: r[tr.type - 1] = f.value; break;
            case 4:
                if (tr.path_joint >= 0 && tr.path_joint < (int)mi.joints.size() && mi.joints[(size_t)tr.path_joint].spline) {
                    double p[3] = {t[0], t[1], t[2]};
                    spline_arc_point(*mi.joints[(size_t)tr.path_joint].spline, std::clamp((double)f.value, 0.0, 1.0), p);
                    for (int k = 0; k < 3; ++k) t[k] = p[k];
                }
                break;
            case 5: case 6: case 7: t[tr.type - 5] = f.value; break;
            // a scale smaller than 0.001 becomes 0.001, as the JObj's animation update does (src/hsd/hsd_jobj.c: the
            // 0x3A83126F test), so a "shrink to nothing" key never makes a matrix that can't be inverted
            case 8: case 9: case 10: s[tr.type - 8] = std::fabs(f.value) < 0.001f ? 0.001f : f.value; break;
            default: break;
        }
    }
}

// Which mesh parts are drawn at a frame: a joint's own parts are hidden by its JOBJ_HIDDEN flag, and an animation's
// HSD_A_J_NODE track (type 11) sets or clears it for that joint, HSD_A_J_BRANCH (12) for the joint and everything
// below it (value > 0.5 = shown; HSD_JObjAnim's update in src/hsd/jobj.c). Tracks apply parent before child (the
// joints' walk order), so a child's own track overrides a branch above it.
static void joint_hidden(const ModelInternal& mi, const Animation* an, float frame, std::vector<char>& hidden) {
    const auto& J = mi.joints;
    hidden.assign(J.size(), 0);
    for (size_t i = 0; i < J.size(); ++i) hidden[i] = (J[i].flags & JOBJ_HIDDEN) ? 1 : 0;
    if (!an) return;
    for (size_t i = 0; i < J.size(); ++i)
        for (const Track& tr : an->joints[i]) {
            if (tr.type != 11 && tr.type != 12) continue;
            FObj f;
            f.ad_head = mi.data.data() + tr.off;
            f.length = tr.length;
            f.frac_value = tr.frac_value;
            f.frac_slope = tr.frac_slope;
            f.req_anim(tr.startframe);
            f.interpret(frame);
            if (!f.has_value) continue;
            const char h = f.value > 0.5f ? 0 : 1;
            if (tr.type == 11) { hidden[i] = h; continue; }
            for (size_t k = i; k < J.size(); ++k) {  // the joint and its descendants (they follow it in walk order)
                if (k != i) {
                    int p = J[k].parent;
                    while (p >= 0 && p != (int)i) p = J[p].parent;
                    if (p != (int)i) continue;
                }
                hidden[k] = h;
            }
        }
}

// a stage's coordinate transform from its raw values, as MakeTextureMtx builds it (tex_stage above)
static void stage_xform(float tx, float ty, float sx, float sy, uint8_t rs_, uint8_t rt_, uint8_t wrap_t, float* out) {
    const float rs = rs_ ? rs_ : 1, rt = rt_ ? rt_ : 1;
    const float su = std::fabs(sx) < 1e-7f ? 0.0f : rs / sx, sv = std::fabs(sy) < 1e-7f ? 0.0f : rt / sy;
    const float mirror = wrap_t == 2 && sv != 0 ? 1.0f / sv : 0.0f;
    out[0] = su;
    out[1] = sv;
    out[2] = -tx;
    out[3] = -(ty + mirror);
}

extern "C" int orre_model_texture_frame2(const OrreModel* m, int a, float frame, int* texture, float* blend, float* xform) {
    if (!m || !m->internal || !texture || !blend) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    for (int i = 0; i < m->mesh_count; ++i)
        for (int s = 0; s < ORRE_MAX_STAGES; ++s) {
            const bool has = s < m->meshes[i].stage_count;
            const OrreTexStage& st = m->meshes[i].stages[s];
            texture[i * ORRE_MAX_STAGES + s] = has ? st.texture : -1;
            blend[i * ORRE_MAX_STAGES + s] = has ? st.blending : 0;
            if (xform) {
                float* x = xform + (size_t)(i * ORRE_MAX_STAGES + s) * 4;
                if (has) { x[0] = st.su; x[1] = st.sv; x[2] = st.tu; x[3] = st.tv; }
                else { x[0] = x[1] = 1; x[2] = x[3] = 0; }
            }
        }
    if (!an) return 1;
    for (const TexAnimEntry& e : an->tex) {
        // the values this texture animation gives at the frame (TObjUpdateFunc, src/hsd/tobj.c): TIMG picks image
        // (int)value when the table has it; BLEND sets the factor clamped to 0..1; TRAU / TRAV / SCAU / SCAV replace
        // the stage's translation / scale (types 2-5). TCLT (palettes), rotation and the TEV colours aren't applied.
        int img = -1;
        float bl = -1;
        bool has_tr[2] = {false, false}, has_sc[2] = {false, false};
        float tr_v[2] = {0, 0}, sc_v[2] = {1, 1};
        for (const Track& tr : e.tracks) {
            if (tr.type != 1 && tr.type != 9 && (tr.type < 2 || tr.type > 5)) continue;
            FObj f;
            f.ad_head = mi->data.data() + tr.off;
            f.length = tr.length;
            f.frac_value = tr.frac_value;
            f.frac_slope = tr.frac_slope;
            f.req_anim(tr.startframe);
            f.interpret(frame);
            if (!f.has_value) continue;
            switch (tr.type) {
                case 1: {
                    const int n = (int)f.value;
                    if (n >= 0 && n < (int)e.images.size() && e.images[n] >= 0) img = e.images[n];
                    break;
                }
                case 9: bl = std::clamp(f.value, 0.0f, 1.0f); break;
                case 2: case 3: has_tr[tr.type - 2] = true; tr_v[tr.type - 2] = f.value; break;
                case 4: case 5: has_sc[tr.type - 4] = true; sc_v[tr.type - 4] = f.value; break;
            }
        }
        const bool coords = has_tr[0] || has_tr[1] || has_sc[0] || has_sc[1];
        if (img < 0 && bl < 0 && !coords) continue;
        for (int i = 0; i < m->mesh_count && i < (int)mi->skins.size(); ++i) {
            if (mi->skins[i].owner != e.joint || mi->skins[i].dobj != e.dobj) continue;
            for (int s = 0; s < m->meshes[i].stage_count; ++s) {
                const OrreTexStage& st = m->meshes[i].stages[s];
                if (st.map_id != e.map_id) continue;
                if (img >= 0) texture[i * ORRE_MAX_STAGES + s] = img;
                if (bl >= 0) blend[i * ORRE_MAX_STAGES + s] = bl;
                if (coords && xform)
                    stage_xform(has_tr[0] ? tr_v[0] : st.translate[0], has_tr[1] ? tr_v[1] : st.translate[1],
                                has_sc[0] ? sc_v[0] : st.scale[0], has_sc[1] ? sc_v[1] : st.scale[1], st.repeat[0],
                                st.repeat[1], st.wrap_t, xform + (size_t)(i * ORRE_MAX_STAGES + s) * 4);
            }
        }
    }
    return 1;
}

// The colours at a frame (orre_core.h ORRE_MATERIAL_FLOATS): each mesh's material colours with its colour animation
// applied (MObjUpdateFunc, src/hsd/mobj.c: AMBIENT_R..B 1-3 and DIFFUSE_R..B 4-6 set 255 x value clamped; ALPHA 10
// sets 1 - value), and each stage's TEV override colours with the texture animation's KONST / TEV0 / TEV1 tracks
// (12-23, TObjUpdateFunc, src/hsd/tobj.c: 255 x value clamped). Values are 0..1, quantised to 8 bits as the game
// stores them. Specular and the pixel-engine references are not used by the viewer.
extern "C" int orre_model_material_frame(const OrreModel* m, int a, float frame, float* out) {
    if (!m || !m->internal || !out) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    auto q8 = [](float v) { return (float)(int)(255.0f * std::clamp(v, 0.0f, 1.0f)) / 255.0f; };
    for (int i = 0; i < m->mesh_count; ++i) {
        const OrreMesh& me = m->meshes[i];
        float* o = out + (size_t)i * ORRE_MATERIAL_FLOATS;
        for (int k = 0; k < 4; ++k) { o[k] = me.diffuse[k] / 255.0f; o[4 + k] = me.ambient[k] / 255.0f; }
        o[3] = std::clamp(me.alpha, 0.0f, 1.0f);
        for (int s = 0; s < ORRE_MAX_STAGES; ++s)
            for (int k = 0; k < 12; ++k) o[8 + 12 * s + k] = s < me.stage_count ? me.stages[s].tev[0x10 + k] / 255.0f : 0.0f;
    }
    if (!an) return 1;
    auto eval = [&](const Track& tr, float& v) {
        FObj f;
        f.ad_head = mi->data.data() + tr.off;
        f.length = tr.length;
        f.frac_value = tr.frac_value;
        f.frac_slope = tr.frac_slope;
        f.req_anim(tr.startframe);
        f.interpret(frame);
        v = f.value;
        return f.has_value;
    };
    for (const MatAnimEntry& e : an->mat) {
        for (int i = 0; i < m->mesh_count && i < (int)mi->skins.size(); ++i) {
            if (mi->skins[i].owner != e.joint || mi->skins[i].dobj != e.dobj) continue;
            float* o = out + (size_t)i * ORRE_MATERIAL_FLOATS;
            for (const Track& tr : e.tracks) {
                float v;
                if (!eval(tr, v)) continue;
                if (tr.type >= 1 && tr.type <= 3) o[4 + tr.type - 1] = q8(v);
                else if (tr.type >= 4 && tr.type <= 6) o[tr.type - 4] = q8(v);
                else if (tr.type == 10) o[3] = std::clamp(1.0f - v, 0.0f, 1.0f);
            }
        }
    }
    for (const TexAnimEntry& e : an->tex) {
        for (const Track& tr : e.tracks) {
            if (tr.type < 12 || tr.type > 23) continue;
            float v;
            if (!eval(tr, v)) continue;
            for (int i = 0; i < m->mesh_count && i < (int)mi->skins.size(); ++i) {
                if (mi->skins[i].owner != e.joint || mi->skins[i].dobj != e.dobj) continue;
                for (int s = 0; s < m->meshes[i].stage_count; ++s)
                    if (m->meshes[i].stages[s].map_id == e.map_id)
                        out[(size_t)i * ORRE_MATERIAL_FLOATS + 8 + 12 * s + (tr.type - 12)] = q8(v);
            }
        }
    }
    return 1;
}

extern "C" int orre_model_texture_frame(const OrreModel* m, int a, float frame, int* texture, float* blend) {
    return orre_model_texture_frame2(m, a, frame, texture, blend, nullptr);
}

extern "C" int orre_model_visibility(const OrreModel* m, int a, float frame, uint8_t* shown) {
    if (!m || !m->internal || !shown) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    std::vector<char> hidden;
    joint_hidden(*mi, an, frame, hidden);
    for (int i = 0; i < m->mesh_count && i < (int)mi->skins.size(); ++i) {
        const int o = mi->skins[i].owner;
        shown[i] = o >= 0 && o < (int)hidden.size() && !hidden[o] ? 1 : 0;
    }
    return 1;
}

extern "C" int orre_model_joint_values(const OrreModel* m, int a, float frame, float* out) {
    if (!m || !m->internal || !out) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    for (size_t i = 0; i < mi->joints.size(); ++i) {
        double r[3], s[3], t[3];
        joint_local(*mi, an, i, frame, r, s, t);
        for (int k = 0; k < 3; ++k) { out[9 * i + k] = (float)r[k]; out[9 * i + 3 + k] = (float)s[k]; out[9 * i + 6 + k] = (float)t[k]; }
    }
    return 1;
}

// GSpartGetTransform's rotation and scale (src/game/gs_part.c, verified from the code; release step 4): the Euler
// rotations of the joint and every parent up to the root, added, and their scales multiplied (the game's own shortcut
// for "the joint's rotation": a move effect following a joint takes these). At most 16 joints up, as the game's walk.
extern "C" int orre_model_part_chain(const OrreModel* m, int a, float frame, int joint, float rot[3], float scl[3]) {
    if (!m || !m->internal || !rot || !scl) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    if (joint < 0 || joint >= (int)mi->joints.size()) return 0;
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    double R[3] = {0, 0, 0}, S[3] = {1, 1, 1};
    int count = 0;
    for (int j = joint; j >= 0 && count < 16; j = mi->joints[(size_t)j].parent, ++count) {
        double r[3], s[3], t[3];
        joint_local(*mi, an, (size_t)j, frame, r, s, t);
        for (int k = 0; k < 3; ++k) {
            R[k] += r[k];
            S[k] *= s[k];
        }
    }
    for (int k = 0; k < 3; ++k) {
        rot[k] = (float)R[k];
        scl[k] = (float)S[k];
    }
    return 1;
}

// Particle keys (track type 0x28, HSD_JObjAnimCallback in src/hsd/hsd_jobj.c): plays each such track from its start
// one frame at a time, as HSD_FObjInterpretAnim does, and records every key that fires. The key's value is read as raw
// bits (val->iv: the FObj stores it as a float, the callback reads the same word as an integer), low 6 bits the bank,
// the rest the emitter; gsParticlePtclHook then starts that emitter at the joint. Why: the 12 species' ambient effects
// (Charizard's tail flame, Vaporeon's sparkle) start only from these keys (no battle model has a JOBJ_PTCL joint), so
// this is what Studio's particle playback is driven by. Plain C++, every platform.
extern "C" int orre_model_particle_keys(const OrreModel* m, int a, OrreParticleKey* out, int max) {
    if (!m || !m->internal) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    if (a < 0 || a >= (int)mi->anims.size()) return 0;
    const Animation& an = mi->anims[(size_t)a];
    const int frames = (int)std::ceil(an.end_frame) + 1;
    int n = 0;
    for (size_t j = 0; j < an.joints.size(); ++j)
        for (const Track& tr : an.joints[j]) {
            if (tr.type != 0x28) continue;
            FObj f;
            f.ad_head = mi->data.data() + tr.off;
            f.length = tr.length;
            f.frac_value = tr.frac_value;
            f.frac_slope = tr.frac_slope;
            f.req_anim(tr.startframe);
            std::vector<float> fired;
            f.sink = &fired;
            for (int fr = 0; fr < frames; ++fr) {
                fired.clear();
                f.interpret(fr == 0 ? 0.0f : 1.0f);
                // every key the frame reached (two on one frame start two emitters)
                for (float v : fired) {
                    uint32_t bits;
                    std::memcpy(&bits, &v, 4);
                    if (n < max && out) out[n] = {fr, (int)j, (int)(bits & 0x3F), (int)(bits >> 6)};
                    ++n;
                }
            }
        }
    return n;
}

extern "C" float orre_model_animation_length(const OrreModel* m, int a) {
    if (!m || !m->internal) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    return a >= 0 && a < (int)mi->anims.size() ? mi->anims[a].end_frame : 0;
}

// Posing (2026-11-18). Each joint's local transform is its own rotation / scale / translation with the animation's
// tracks replacing channels (HSD_A_J_ROTX..SCAZ = 1-3, 5-10, as the JObj's animation update writes them); world
// matrices as at bind (parent x T x Rz x Ry x Rx x S). Then each vertex follows its mesh's skin kind (MeshSkin):
// rigid -> world now x inverse(world at bind); envelope -> sum of weight x world now x inverse-bind (a joint without
// one uses inverse(world at bind)); envelope with a node matrix -> the same, after the node matrix's change since the
// bind pose. The bind pose itself comes out unchanged (every such product is the identity there).
// Approximations (likely small): envelopes keep their 4 heaviest joints; path animations and quaternion joints are not
// applied. Scale inheritance follows HSD_JObjMakeMatrix (srt's parent-scale compensation, classical scale; added
// 2026-11-18); billboards are applied below.
// ---- billboards (HSD_JObjMakePositionMtx and mkBillBoardMtx / mkVBillBoardMtx / mkHBillBoardMtx / mkRBillBoardMtx,
// src/hsd/displayfunc.c): a billboard joint's matrix in view space keeps its position and its axes' lengths, and its
// axes are rebuilt to face the camera (all of them; V keeps its own up axis; H keeps its own side axis; R turns only
// about the view axis by the joint's own Z rotation). PBILLBOARD faces the eye point instead of the view direction. ----
namespace {
struct V3 { double x, y, z; };
V3 col(const M4& m, int c) { return {m[0][c], m[1][c], m[2][c]}; }
void set_col(M4& m, int c, V3 v) { m[0][c] = v.x; m[1][c] = v.y; m[2][c] = v.z; }
double mag(V3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
V3 scale(V3 v, double s) { return {v.x * s, v.y * s, v.z * s}; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
constexpr double kEps = 1.1920929e-7;  // FLT_EPSILON, as the game adds it

M4 billboard(uint32_t flags, double rot_z, const M4& src) {
    constexpr uint32_t PB = 0x2000;
    M4 dst = src;
    const V3 pos = col(src, 3);
    switch (flags & 0xE00) {
        case 0x200: {  // JOBJ_BILLBOARD
            double sx = mag(col(src, 0)), sz = mag(col(src, 2));
            V3 ay = col(src, 1);
            double sy = mag(ay);
            V3 az = (flags & PB) ? scale(pos, -1.0 / (kEps + mag(pos))) : V3{0, 0, 1};
            ay = scale(ay, 1.0 / (sy + kEps));
            V3 ax = cross(ay, az);
            const double mg = mag(ax);
            if (mg >= kEps) {
                sx /= mg;
                ay = cross(az, ax);
                sy /= kEps + mag(ay);
            } else {
                ay.y = kEps + std::sqrt(az.x * az.x + az.z * az.z);
                ay.x = az.x * (-az.y / ay.y);
                ay.z = az.z * (-az.y / ay.y);
                ax = cross(ay, az);
                sx /= kEps + mag(ax);
            }
            set_col(dst, 0, scale(ax, sx));
            set_col(dst, 1, scale(ay, sy));
            set_col(dst, 2, scale(az, sz));
            break;
        }
        case 0x400: {  // JOBJ_VBILLBOARD
            const V3 ay = col(src, 1);
            const V3 uy = scale(ay, 1.0 / (kEps + mag(ay)));
            double sx = mag(col(src, 0)), sz = mag(col(src, 2));
            V3 ax = (flags & PB) ? cross(uy, scale(pos, -1.0 / (kEps + mag(pos)))) : cross(uy, V3{0, 0, 1});
            const double mg = mag(ax);
            if (mg < kEps) return src;
            sx /= mg;
            const V3 az = cross(ax, uy);
            sz /= kEps + mag(az);
            set_col(dst, 0, scale(ax, sx));
            set_col(dst, 1, ay);
            set_col(dst, 2, scale(az, sz));
            break;
        }
        case 0x600: {  // JOBJ_HBILLBOARD
            const V3 ax = col(src, 0);
            const V3 ux = scale(ax, 1.0 / (kEps + mag(ax)));
            double sy = mag(col(src, 1)), sz = mag(col(src, 2));
            V3 ay{0, 1, 0};
            if (flags & PB) {
                ay.y = kEps + std::sqrt(pos.x * pos.x + pos.z * pos.z);
                ay.x = pos.x * (-pos.y / ay.y);
                ay.z = pos.z * (-pos.y / ay.y);
                ay = scale(ay, 1.0 / (kEps + mag(ay)));
            }
            V3 az = cross(ux, ay);
            const double mg = mag(az);
            if (mg < kEps) return src;
            sz /= mg;
            ay = cross(az, ux);
            sy /= kEps + mag(ay);
            set_col(dst, 0, ax);
            set_col(dst, 1, scale(ay, sy));
            set_col(dst, 2, scale(az, sz));
            break;
        }
        case 0x800: {  // JOBJ_RBILLBOARD: scale, then the joint's own Z rotation, at the joint's position
            const double sx = mag(col(src, 0)), sy = mag(col(src, 1)), sz = mag(col(src, 2));
            const double c = std::cos(rot_z), s = std::sin(rot_z);
            dst = ident();
            dst[0][0] = c * sx; dst[0][1] = -s * sy;
            dst[1][0] = s * sx; dst[1][1] = c * sy;
            dst[2][2] = sz;
            set_col(dst, 3, pos);
            break;
        }
        default: break;
    }
    return dst;
}
}  // namespace

// ---- IK and constraints (HSD_JObjSetupMatrixSub, resolveIKJoint1 / resolveIKJoint2 in src/hsd/jobj.c, and
// HSD_RObjUpdateAll / HSD_RObjGetGlobalPosition / resolveLimits in src/hsd/robj.c; ported line by line, 2026-11-18).
// Only Ralts, Sableye and Exploud use them: their legs are two-bone IK chains (JOBJ_JOINT1 -> JOINT2 -> EFFECTOR) that
// reach for a target joint, so without this Ralts's idle crouched with its legs folded wrong (snatcherLeo's review).
namespace {
constexpr uint32_t ROBJ_ACTIVE = 0x80000000u, REF_JOBJ = 0x10000000u, REF_LIMIT = 0x20000000u, REF_IKHINT = 0x40000000u;
constexpr uint32_t JOINT1 = 1u << 21, JOINT2 = 2u << 21, EFFECTOR = 3u << 21;

const RObj* robj_by_type(const Joint& j, uint32_t type, uint32_t sub) {
    for (const RObj& r : j.robj)
        if ((r.flags & ROBJ_ACTIVE) && (r.flags & 0x70000000) == type && (!sub || sub == (r.flags & 0x0FFFFFFF))) return &r;
    return nullptr;
}
V3 v3sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 v3add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 unit10(V3 v) { return scale(v, std::sqrt(1.0 / (1e-10 + dot(v, v)))); }  // the game's 1e-10 guard
// rotation by `ang` about axis `ax` (PSMTXRotAxisRad), applied to v
V3 rot_axis(V3 ax, double ang, V3 v) {
    const V3 k = unit10(ax);
    const double c = std::cos(ang), s = std::sin(ang);
    const V3 kxv = cross(k, v);
    const double kd = dot(k, v) * (1 - c);
    return {v.x * c + kxv.x * s + k.x * kd, v.y * c + kxv.y * s + k.y * kd, v.z * c + kxv.z * s + k.z * kd};
}

struct Poser {
    const ModelInternal& mi;
    const Animation* an;
    float frame;
    std::vector<M4> world, plain;  // plain: without IK (targets later in the walk read it, as SetupMatrix would)
    std::vector<char> done;
    std::vector<V3> eff_target;    // an effector's target position, set by its chain's JOINT1 (jobj->translate)
    std::vector<V3> scl;           // each joint's accumulated scale (jobj->scl; 1,1,1 where it has none)
    std::vector<char> has_scl;     // whether the joint has a jobj->scl (a classical-scale chain may not)
    std::vector<double> local_sx;  // each joint's own (local) x scale at this frame: jobj->scale.x

    V3 pos(int j) const { return col(done[(size_t)j] ? world[(size_t)j] : plain[(size_t)j], 3); }
    // HSD_RObjGetGlobalPosition: the average position of the active joint references of this subtype
    int global_position(const Joint& jt, uint32_t sub, V3& p) const {
        V3 v{0, 0, 0};
        int n = 0;
        for (const RObj& r : jt.robj)
            if ((r.flags & 0x70000000) == REF_JOBJ && (r.flags & ROBJ_ACTIVE) && (r.flags & 0x0FFFFFFF) == sub && r.joint >= 0) {
                v = v3add(v, pos(r.joint));
                ++n;
            }
        if (n) p = scale(v, 1.0 / n);
        return n;
    }
    int child_with(int ji, uint32_t kind) const {
        for (size_t c = (size_t)ji + 1; c < mi.joints.size(); ++c)
            if (mi.joints[c].parent == ji && (mi.joints[c].flags & EFFECTOR) == kind) return (int)c;
        return -1;
    }
    // jobj_get_effector_checked: the child effector, only if it has a target (REFTYPE_JOBJ subtype 1)
    int effector_of(int ji) const {
        const int e = child_with(ji, EFFECTOR);
        return e >= 0 && robj_by_type(mi.joints[(size_t)e], REF_JOBJ, 1) ? e : -1;
    }

    void joint1(int i) {
        const Joint& jt = mi.joints[(size_t)i];
        const V3 sc = scl[(size_t)i];
        V3 base{0, 0, 0};
        const int j2 = child_with(i, JOINT2);
        const RObj* hint = robj_by_type(jt, REF_IKHINT, 0);
        if (!hint) return;
        const double rot_x = hint->rotate_x, len1 = hint->bone * sc.x;
        double len2 = 0;
        bool flip = false;
        int eff;
        if (j2 >= 0) {
            const RObj* h2 = robj_by_type(mi.joints[(size_t)j2], REF_IKHINT, 0);
            if (!h2) return;
            // the second bone: its hint's length x the second joint's OWN scale x the first joint's accumulated scale,
            // as resolveIKJoint1 has it (src/hsd/jobj.c: bone_length * var_r31->scale.x * spBC.x). *Corrected
            // 2026-11-19:* this used the second joint's accumulated scale (scl), which already holds the first
            // joint's, so that scale counted twice; the second bone came out short (about 0.29 x on Sableye), the
            // target looked out of reach and the leg was stretched straight (Sableye's front leg; verified against
            // the game's own joint matrices from the move viewer, the project's log)
            len2 = h2->bone * local_sx[(size_t)j2] * sc.x;
            flip = (h2->flags & 4) != 0;
            eff = effector_of(j2);
        } else {
            eff = effector_of(i);
        }
        if (eff < 0) return;
        if (jt.parent >= 0) base = col(world[(size_t)jt.parent], 3);
        V3 target;
        if (!global_position(mi.joints[(size_t)eff], 1, target)) return;
        eff_target[(size_t)eff] = target;
        const V3 d = v3sub(target, base);
        const double dd = dot(d, d);
        double along, side, l1sq = 0, l2sq = 0, h = 0;
        V3 up, ax_y;
        if (dd > 1e-8) {
            V3 pole, n;
            if (global_position(jt, 3, pole)) {
                pole = v3sub(pole, base);
                if (rot_x != 0) pole = rot_axis(d, rot_x, pole);
                n = cross(d, pole);
                pole = cross(n, d);
            } else {
                n = col(world[(size_t)i], 2);
                pole = cross(n, d);
                n = cross(d, pole);
            }
            up = unit10(n);
            ax_y = unit10(pole);
            l1sq = len1 * len1;
            l2sq = len2 * len2;
            h = 0.25 * ((2.0 * (l1sq + l2sq) - dd) - ((l1sq - l2sq) * (l1sq - l2sq)) / dd);
            if (h < 0) h = 0;
            const double t = (l1sq - h) / dd;
            along = t * std::sqrt(1.0 / (1e-10 + t));
            side = h * std::sqrt(1.0 / (1e-10 + h));
        } else {
            along = 0;
            side = len1;
            up = col(world[(size_t)i], 2);
            ax_y = col(world[(size_t)i], 1);
        }
        if (flip) side = -side;
        V3 x = (l2sq - h) < dd ? scale(d, along) : scale(d, -along);
        x = unit10(v3add(x, scale(ax_y, side)));
        const V3 y = cross(up, x);
        set_col(world[(size_t)i], 0, scale(x, sc.x));
        set_col(world[(size_t)i], 1, scale(y, sc.y));
        set_col(world[(size_t)i], 2, scale(up, sc.z));
        set_col(world[(size_t)i], 3, base);
    }

    void joint2(int i) {
        const Joint& jt = mi.joints[(size_t)i];
        const int eff = effector_of(i);
        if (eff < 0 || jt.parent < 0) return;
        const V3 sc = scl[(size_t)i];
        const M4& pm = world[(size_t)jt.parent];
        const RObj* ph = robj_by_type(mi.joints[(size_t)jt.parent], REF_IKHINT, 0);
        if (!ph) return;
        const V3 start = v3add(col(pm, 3), scale(unit10(col(pm, 0)), ph->bone * scl[(size_t)jt.parent].x));
        V3 x = unit10(v3sub(eff_target[(size_t)eff], start));
        const RObj* lo = robj_by_type(jt, REF_LIMIT, 5);
        const RObj* hi = robj_by_type(jt, REF_LIMIT, 6);
        if (lo || hi) {
            const RObj* h = robj_by_type(jt, REF_IKHINT, 0);
            const bool flip = h && (h->flags & 4);
            const V3 px = unit10(col(pm, 0));
            const double c = dot(px, x);
            double ang = c >= 1.0 ? 0.0 : c <= -1.0 ? 3.14159265358979 : std::acos(c);
            if (!flip) ang = -ang;
            bool clamp = false;
            if (lo && ang < lo->value) { ang = lo->value; clamp = true; }
            else if (hi && hi->value < ang) { ang = hi->value; clamp = true; }
            if (clamp) x = rot_axis(col(pm, 2), ang, px);
        }
        const V3 y = unit10(cross(col(pm, 2), x));
        const V3 z = cross(x, y);
        M4& w = world[(size_t)i];
        set_col(w, 0, scale(x, sc.x));
        set_col(w, 1, scale(y, sc.y));
        set_col(w, 2, scale(z, sc.z));
        set_col(w, 3, start);
    }

    void effector(int i) {
        const Joint& jt = mi.joints[(size_t)i];
        if (jt.parent < 0) return;
        const RObj* ph = robj_by_type(mi.joints[(size_t)jt.parent], REF_IKHINT, 0);
        if (!ph) return;
        const M4& pm = world[(size_t)jt.parent];
        set_col(world[(size_t)i], 3, v3add(col(pm, 3), scale(unit10(col(pm, 0)), ph->bone * scl[(size_t)jt.parent].x)));
    }

    // HSD_RObjUpdateAll for an ordinary joint: a position constraint (subtype 1) moves the joint onto its targets
    // (update type 0x35); an aim constraint (2, with up 3) and an orientation copy (4) set its axes
    void constraints(int i) {
        const Joint& jt = mi.joints[(size_t)i];
        M4& w = world[(size_t)i];
        V3 p;
        if (global_position(jt, 1, p)) set_col(w, 3, p);
        V3 aim;
        if (global_position(jt, 2, aim)) {
            const V3 here = col(w, 3);
            V3 dir = v3sub(aim, here), up{0, 1, 0};
            if (global_position(jt, 3, up)) up = v3sub(up, here);
            else if (std::fabs(1.0 - dot(dir, up)) < 1e-10) up = {0, 0, 1};
            const V3 s = scl[(size_t)i];
            V3 z = cross(dir, up);
            dir = unit10(dir);
            z = unit10(z);
            up = cross(z, dir);
            set_col(w, 0, scale(dir, s.x));
            set_col(w, 1, scale(up, s.y));
            set_col(w, 2, scale(z, s.z));
        }
        if (const RObj* o = robj_by_type(jt, REF_JOBJ, 4); o && o->joint >= 0) {
            const M4& src = done[(size_t)o->joint] ? world[(size_t)o->joint] : plain[(size_t)o->joint];
            for (int c = 0; c < 3; ++c) {
                V3 v = col(src, c);
                double sv = mag(v);
                if (sv > 1e-10) sv = 1.0 / sv;
                sv *= mag(col(w, c));
                set_col(w, c, scale(v, sv));
            }
        }
    }

    void run() {
        const size_t n = mi.joints.size();
        world.assign(n, ident());
        plain.assign(n, ident());
        done.assign(n, 0);
        eff_target.assign(n, V3{0, 0, 0});
        scl.assign(n, V3{1, 1, 1});
        has_scl.assign(n, 0);
        local_sx.assign(n, 1.0);
        std::vector<M4> local(n);
        for (size_t i = 0; i < n; ++i) {
            double r[3], s[3], t[3];
            joint_local(mi, an, i, frame, r, s, t);
            const Joint& J = mi.joints[i];
            // resolveLimits (rotation / translation clamps, REFTYPE_LIMIT 1-12) for ordinary joints, before the matrix
            if (!(J.flags & EFFECTOR))
                for (const RObj& lr : J.robj) {
                    if ((lr.flags & 0x70000000) != REF_LIMIT) continue;
                    const uint32_t sub = lr.flags & 0x0FFFFFFF;
                    double* v = sub <= 6 ? r : t;
                    const int axis = sub <= 6 ? (int)(sub - 1) / 2 : sub <= 8 ? 0 : 1;  // 9-12 all limit y, as the game does
                    if (sub < 1 || sub > 12) continue;
                    if (sub % 2 == 1) v[axis] = std::max(v[axis], lr.value);
                    else v[axis] = std::min(v[axis], lr.value);
                }
            // jobj->scl: the joint's accumulated scale, as HSD_JObjMakeMatrix keeps it (accumulate_scl); the parent's
            // goes into the matrix (HSD_MtxSRT's uneven-scale compensation). The IK hints' bone lengths are in local
            // units, so they are scaled by it too (Ralts's model is about 0.42 of its authored size: its 1.348 thigh
            // is 0.57 long in the bind pose; with the joint's own scale only, the chain overshot and folded)
            const bool p_has = J.parent >= 0 && has_scl[(size_t)J.parent];
            local_sx[i] = s[0];
            double sv[3];
            has_scl[i] = accumulate_scl(J.flags, s, p_has ? &scl[(size_t)J.parent].x : nullptr, sv) ? 1 : 0;
            if (has_scl[i]) scl[i] = {sv[0], sv[1], sv[2]};
            local[i] = srt(s, r, t, p_has ? &scl[(size_t)J.parent].x : nullptr);
            plain[i] = J.parent >= 0 ? mul(plain[(size_t)J.parent], local[i]) : local[i];
        }
        for (size_t i = 0; i < n; ++i) {
            const Joint& J = mi.joints[i];
            world[i] = J.parent >= 0 ? mul(world[(size_t)J.parent], local[i]) : local[i];
            switch (J.flags & EFFECTOR) {
                case JOINT1: joint1((int)i); break;
                case JOINT2: joint2((int)i); break;
                case EFFECTOR: effector((int)i); break;
                default: if (!J.robj.empty()) constraints((int)i); break;
            }
            done[i] = 1;
        }
    }
};
}  // namespace

// Each joint's world matrix at a frame (3 rows of 4, row-major, 12 floats per joint, in the model's walk order) and
// its accumulated scale (jobj->scl, 3 floats; 0 0 0 where the joint has none). Written to compare Studio's posing
// with the game's own matrices, logged from the move viewer (Lunatone's and Sharpedo's jaws, 2026-11-19).
extern "C" int orre_model_joint_world(const OrreModel* m, int a, float frame, float* out12, float* scl3) {
    if (!m || !m->internal || !out12) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    Poser P{*mi, an, frame, {}, {}, {}, {}, {}};
    P.run();
    for (size_t i = 0; i < P.world.size(); ++i) {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c) out12[i * 12 + (size_t)(r * 4 + c)] = (float)P.world[i][r][c];
        if (scl3) {
            const bool has = i < P.has_scl.size() && P.has_scl[i];
            scl3[i * 3] = has ? (float)P.scl[i].x : 0.0f;
            scl3[i * 3 + 1] = has ? (float)P.scl[i].y : 0.0f;
            scl3[i * 3 + 2] = has ? (float)P.scl[i].z : 0.0f;
        }
    }
    return 1;
}

extern "C" int orre_model_pose(const OrreModel* m, int a, float frame, OrreVertex* const* out) {
    return orre_model_pose_view(m, a, frame, nullptr, out);
}

extern "C" int orre_model_pose_view(const OrreModel* m, int a, float frame, const float* view, OrreVertex* const* out) {
    if (!m || !m->internal || !out) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    const auto& J = mi->joints;
    const size_t n = J.size();
    std::vector<M4> skin(n), rigid(n);
    const Animation* an = a >= 0 && a < (int)mi->anims.size() ? &mi->anims[a] : nullptr;
    Poser P{*mi, an, frame, {}, {}, {}, {}, {}};
    P.run();
    const std::vector<M4>& world = P.world;
    // ORRE_DEBUG_POSE=1 (developer switch, any OS): print how far IK and constraints move each joint from the plain
    // chain, and every joint's reference objects (how the IK port was checked: Exploud's chains solve exactly to its
    // bind pose, 2026-11-18)
    if (std::getenv("ORRE_DEBUG_POSE")) {
        double dp = 0, db = 0;
        for (size_t i = 0; i < n; ++i)
            for (int r2 = 0; r2 < 3; ++r2)
                for (int c = 0; c < 4; ++c) {
                    dp = std::max(dp, std::fabs(world[i][r2][c] - P.plain[i][r2][c]));
                    db = std::max(db, std::fabs(world[i][r2][c] - J[i].world[r2][c]));
                }
        std::fprintf(stderr, "pose: vs plain %.5f, vs bind %.5f\n", dp, db);
        for (size_t i = 0; i < n; ++i) {
            double d = 0;
            for (int r2 = 0; r2 < 3; ++r2)
                for (int c = 0; c < 4; ++c) d = std::max(d, std::fabs(world[i][r2][c] - P.plain[i][r2][c]));
            if (d > 0.01 || (J[i].flags & 0x600000))
                std::fprintf(stderr, "  joint %zu parent %d flags %08X robj %zu dev %.3f pos %.2f %.2f %.2f plain %.2f %.2f %.2f\n", i,
                             J[i].parent, J[i].flags, J[i].robj.size(), d, world[i][0][3], world[i][1][3], world[i][2][3],
                             P.plain[i][0][3], P.plain[i][1][3], P.plain[i][2][3]);
        }
        for (size_t i = 0; i < n; ++i)
            for (const RObj& r : J[i].robj)
                std::fprintf(stderr, "  robj j%zu flags %08X target %d value %.3f bone %.3f rotx %.3f\n", i, r.flags, r.joint, r.value,
                             r.bone, r.rotate_x);
    }
    for (size_t i = 0; i < n; ++i) {
        const M4 bind_inv = inv(J[i].world);
        rigid[i] = mul(world[i], bind_inv);
        skin[i] = mul(world[i], J[i].has_inv_bind ? J[i].inv_bind : bind_inv);
    }
    // each joint's matrix now (the billboard below replaces it), for the parts posed straight from their joint's own
    // space (MeshSkin.raw)
    std::vector<M4> now = world;
    // billboard joints, when the caller gives the view: their parts are placed by inverse(view) x billboard(view x
    // world) instead of world (only the joint's own parts: children keep the plain world matrix, as in the game)
    if (view) {
        M4 V = ident();
        for (int r2 = 0; r2 < 3; ++r2)
            for (int c = 0; c < 4; ++c) V[r2][c] = view[4 * r2 + c];
        const M4 Vi = inv(V);
        for (size_t i = 0; i < n; ++i) {
            if (!(J[i].flags & 0xE00)) continue;
            double r[3], s[3], t[3];
            joint_local(*mi, an, i, frame, r, s, t);
            const M4 bb = mul(Vi, billboard(J[i].flags, r[2], mul(V, world[i])));
            rigid[i] = mul(bb, inv(J[i].world));
            now[i] = bb;
        }
    }
    auto xf = [](const M4& M, const float* v, double w, double* o) {
        for (int r2 = 0; r2 < 3; ++r2) o[r2] = M[r2][0] * v[0] + M[r2][1] * v[1] + M[r2][2] * v[2] + M[r2][3] * w;
    };
    // a normal through the inverse transpose of M's 3 x 3 part: the cofactor matrix (inverse transpose x determinant),
    // its sign kept so a mirrored joint (negative determinant) doesn't turn the normal inside out, then normalised
    auto normal_xf = [](const M4& M, const float* n, double* o) {
        const double c00 = M[1][1] * M[2][2] - M[1][2] * M[2][1], c01 = -(M[1][0] * M[2][2] - M[1][2] * M[2][0]),
                     c02 = M[1][0] * M[2][1] - M[1][1] * M[2][0], c10 = -(M[0][1] * M[2][2] - M[0][2] * M[2][1]),
                     c11 = M[0][0] * M[2][2] - M[0][2] * M[2][0], c12 = -(M[0][0] * M[2][1] - M[0][1] * M[2][0]),
                     c20 = M[0][1] * M[1][2] - M[0][2] * M[1][1], c21 = -(M[0][0] * M[1][2] - M[0][2] * M[1][0]),
                     c22 = M[0][0] * M[1][1] - M[0][1] * M[1][0];
        const double det = M[0][0] * c00 + M[0][1] * c01 + M[0][2] * c02, s = det < 0 ? -1.0 : 1.0;
        o[0] = s * (c00 * n[0] + c01 * n[1] + c02 * n[2]);
        o[1] = s * (c10 * n[0] + c11 * n[1] + c12 * n[2]);
        o[2] = s * (c20 * n[0] + c21 * n[1] + c22 * n[2]);
        const double len = std::sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
        if (len > 1e-12) { o[0] /= len; o[1] /= len; o[2] /= len; }
    };
    // ORRE_DEBUG_SKIN=1 (developer switch, any OS): each part's skinning kind and the joints its vertices follow (how
    // Lunatone's jaws were looked into against the game's own joint matrices, 2026-11-19)
    if (std::getenv("ORRE_DEBUG_SKIN")) {
        static const char* kKind[] = {"static", "rigid", "envelope", "envelope+node"};
        for (int mesh = 0; mesh < m->mesh_count && mesh < (int)mi->skins.size(); ++mesh) {
            const MeshSkin& sk = mi->skins[mesh];
            std::map<int, int> used;
            for (const Influence& in : sk.infl)
                for (int k = 0; k < 4 && in.joint[k] >= 0; ++k) ++used[in.joint[k]];
            std::string js;
            for (auto& [j, c] : used) js += " j" + std::to_string(j) + "x" + std::to_string(c);
            std::fprintf(stderr, "skin mesh %d: %s owner %d inv_bind %d;%s\n", mesh, kKind[std::clamp(sk.kind, 0, 3)], sk.owner,
                         sk.owner >= 0 && sk.owner < (int)J.size() ? (int)J[(size_t)sk.owner].has_inv_bind : -1, js.c_str());
        }
        for (size_t i = 0; i < J.size(); ++i)
            std::fprintf(stderr, "joint %zu parent %d flags %08X inv_bind %d right %d\n", i, J[i].parent, J[i].flags, (int)J[i].has_inv_bind,
                         (int)J[i].has_right);
        // the node matrix ("right") each part uses now, as _HSD_mkEnvelopeModelNodeMtx makes it
        for (size_t i = 0; i < J.size(); ++i) {
            M4 now;
            if (!J[i].has_right || !node_matrix(J, world, (int)i, now)) continue;
            std::fprintf(stderr, "right %zu now: %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f\n", i, now[0][0], now[0][1],
                         now[0][2], now[0][3], now[1][0], now[1][1], now[1][2], now[1][3], now[2][0], now[2][1], now[2][2], now[2][3]);
        }
    }
    for (int mesh = 0; mesh < m->mesh_count && mesh < (int)mi->skins.size(); ++mesh) {
        const OrreMesh& me = m->meshes[mesh];
        const MeshSkin& sk = mi->skins[mesh];
        OrreVertex* dst = out[mesh];
        if (!dst) continue;
        M4 node = ident();
        if (sk.kind == SKIN_ENVELOPE_NODE) {
            M4 now, bind;
            if (node_matrix(J, world, sk.owner, now) && J[sk.owner].has_right) node = mul(now, inv(J[sk.owner].right));
        }
        // a shape-animated part (Surf's waves): its shapes blended for this frame as drawShapeAnim does
        // (src/hsd/pobj.c), then placed by the owning joint at bind like a plain vertex, before the pose below
        std::vector<double> bpos, bnrm;
        if (sk.shape && an) {
            const ShapeData& sd = *sk.shape;
            const ShapeAnimEntry* se = nullptr;
            for (const ShapeAnimEntry& e : an->shape)
                if (e.joint == sk.owner && e.dobj == sk.dobj && e.pobj == sk.pobj) { se = &e; break; }
            if (se && sd.nb_shape > 0) {
                const int nb = sd.nb_shape;
                float bl = 0.0f;
                std::vector<float> bp((size_t)nb, 0.0f);
                for (const Track& tr : se->tracks) {
                    FObj f;
                    f.ad_head = mi->data.data() + tr.off;
                    f.length = tr.length;
                    f.frac_value = tr.frac_value;
                    f.frac_slope = tr.frac_slope;
                    f.req_anim(tr.startframe);
                    f.interpret(frame);
                    if (!f.has_value) continue;
                    // PObjUpdateFunc: the additive kind keeps one weight per track type (HSD_A_S_W0 = 2 is the
                    // first), the averaging kind one blend value
                    if (sd.flags & 2) {
                        const int j = tr.type - 2;
                        if (j >= 0 && j < nb) bp[(size_t)j] = f.value;
                    } else {
                        bl = f.value;
                    }
                }
                auto blend_set = [&](const std::vector<double>& S, int count, std::vector<double>& o) {
                    o.assign((size_t)count * 3, 0.0);
                    auto at = [&](int s, int i, int c) {
                        const size_t k = ((size_t)s * count + i) * 3 + c;
                        return k < S.size() ? S[k] : 0.0;
                    };
                    if (sd.flags & 1) {
                        const int id = std::min(std::max((int)bl, 0), nb - 1), id2 = std::min(id + 1, nb - 1);
                        const double t = std::clamp((double)bl - id, 0.0, 1.0);
                        for (int i = 0; i < count; ++i)
                            for (int c = 0; c < 3; ++c) o[(size_t)i * 3 + c] = (at(id2, i, c) - at(id, i, c)) * t + at(id, i, c);
                    } else {
                        // shape 0 plus each further shape times its weight (the game's loop reads shape j + 1 for
                        // every j < nb_shape; past the last shape there is nothing to add here)
                        for (int i = 0; i < count; ++i)
                            for (int c = 0; c < 3; ++c) {
                                double s = at(0, i, c);
                                for (int j = 0; j + 1 < nb; ++j) s += std::max(0.0f, bp[(size_t)j]) * at(j + 1, i, c);
                                o[(size_t)i * 3 + c] = s;
                            }
                    }
                };
                blend_set(sd.pos, sd.npos, bpos);
                blend_set(sd.nrm, sd.nnrm, bnrm);
                // (a part posed from its joint's own space keeps them unplaced: MeshSkin.raw)
                for (size_t i = 0; sk.raw.empty() && i + 2 < bpos.size(); i += 3) {
                    double q[3];
                    apply(sd.place, &bpos[i], 1.0, q);
                    for (int c = 0; c < 3; ++c) bpos[i + c] = q[c];
                }
                for (size_t i = 0; sk.raw.empty() && i + 2 < bnrm.size(); i += 3) {
                    const float nf[3] = {(float)bnrm[i], (float)bnrm[i + 1], (float)bnrm[i + 2]};
                    double q[3];
                    normal_xf(sd.place, nf, q);
                    for (int c = 0; c < 3; ++c) bnrm[i + c] = q[c];
                }
            }
        }
        const bool shaped = !bpos.empty() && sk.shape && sk.shape->vpos.size() == (size_t)me.vertex_count;
        for (int v = 0; v < me.vertex_count; ++v) {
            OrreVertex srcv = me.vertices[v];
            const bool own_space = sk.kind == SKIN_RIGID && sk.raw.size() == (size_t)me.vertex_count * 6;
            if (own_space)
                for (int c = 0; c < 3; ++c) { srcv.pos[c] = sk.raw[(size_t)v * 6 + c]; srcv.nrm[c] = sk.raw[(size_t)v * 6 + 3 + c]; }
            if (shaped) {
                const int ip = sk.shape->vpos[(size_t)v], in_ = sk.shape->vnrm[(size_t)v];
                if (ip >= 0 && (size_t)ip * 3 + 2 < bpos.size())
                    for (int c = 0; c < 3; ++c) srcv.pos[c] = (float)bpos[(size_t)ip * 3 + c];
                if (in_ >= 0 && (size_t)in_ * 3 + 2 < bnrm.size())
                    for (int c = 0; c < 3; ++c) srcv.nrm[c] = (float)bnrm[(size_t)in_ * 3 + c];
            }
            const OrreVertex& src = srcv;
            OrreVertex o = src;
            double p[3] = {0, 0, 0}, nn[3] = {0, 0, 0};
            const Influence& in = v < (int)sk.infl.size() ? sk.infl[v] : Influence{};
            // Normals go through the inverse transpose of the (blended) matrix, as the game loads them
            // (HSD_MtxInverseTranspose in src/hsd/pobj.c; the lighting map section 3). *Corrected 2026-11-19:* they were
            // turned by the matrix itself (and blended after), which bends them wherever a joint is scaled unevenly
            // (found by the all-models check under Game lighting). Normalised afterwards (the shader normalises too)
            if (sk.kind == SKIN_RIGID && in.joint[0] >= 0) {
                const M4& R = own_space ? now[in.joint[0]] : rigid[in.joint[0]];
                xf(R, src.pos, 1, p);
                normal_xf(R, src.nrm, nn);
            } else if ((sk.kind == SKIN_ENVELOPE || sk.kind == SKIN_ENVELOPE_NODE) && in.joint[0] >= 0) {
                float base[3] = {src.pos[0], src.pos[1], src.pos[2]}, bn[3] = {src.nrm[0], src.nrm[1], src.nrm[2]};
                if (sk.kind == SKIN_ENVELOPE_NODE) {
                    double q[3], qn[3];
                    xf(node, src.pos, 1, q);
                    normal_xf(node, src.nrm, qn);
                    for (int k = 0; k < 3; ++k) { base[k] = (float)q[k]; bn[k] = (float)qn[k]; }
                }
                // a single full-weight joint without a node matrix was placed by its world matrix at bind: rigid
                const bool single = sk.kind == SKIN_ENVELOPE && in.joint[1] < 0 && in.weight[0] >= 0.999f;
                M4 blend{};
                for (int k = 0; k < 4 && in.joint[k] >= 0; ++k) {
                    double pp[3];
                    const M4& M = single ? rigid[in.joint[k]] : skin[in.joint[k]];
                    xf(M, base, 1, pp);
                    for (int c = 0; c < 3; ++c) p[c] += in.weight[k] * pp[c];
                    for (int r = 0; r < 3; ++r)
                        for (int c = 0; c < 3; ++c) blend[r][c] += in.weight[k] * M[r][c];
                }
                normal_xf(blend, bn, nn);
            } else {
                if (shaped) dst[v] = src;  // static but shape-animated: the blended shape stands
                continue;                  // static: the bind position stands
            }
            for (int c = 0; c < 3; ++c) { o.pos[c] = (float)p[c]; o.nrm[c] = (float)nn[c]; }
            dst[v] = o;
        }
        if (sk.kind == SKIN_STATIC && !shaped) std::memcpy(dst, me.vertices, sizeof(OrreVertex) * me.vertex_count);
    }
    return 1;
}

// The survey (orre_core.h OrreModelSurvey): counts read straight from the joint flags, the material animations and
// the stages, so the CLI can list which species use which feature the viewer doesn't draw yet
extern "C" int orre_model_survey(const OrreModel* m, OrreModelSurvey* out) {
    if (!m || !out || !m->internal) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    *out = OrreModelSurvey{};
    constexpr uint32_t BILLBOARD = 0xE00, PBILLBOARD = 0x2000, XLU = 1u << 19, TEXEDGE = 1u << 20;
    for (const auto& j : mi->joints) {
        if (j.flags & (BILLBOARD | PBILLBOARD)) ++out->billboard_joints;
        if (j.flags & JOBJ_PTCL) ++out->particle_joints;
        if (j.flags & JOBJ_SPLINE) ++out->spline_joints;
        if (j.flags & XLU) ++out->xlu_joints;
        if (j.flags & TEXEDGE) ++out->texedge_joints;
    }
    out->colour_anims = mi->colour_anims;
    out->render_anims = mi->render_anims;
    out->dropped_stages = mi->dropped_stages;
    for (int i = 0; i < m->mesh_count; ++i)
        for (int s = 0; s < m->meshes[i].stage_count; ++s) {
            out->tev_stages += m->meshes[i].stages[s].has_tev ? 1 : 0;
            out->max_uv_set = std::max(out->max_uv_set, m->meshes[i].stages[s].uv_set);
        }
    return 1;
}

extern "C" int orre_model_texture_source(const OrreModel* m, int texture, OrreTextureSource* out) {
    if (!m || !out || !m->internal) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    if (texture < 0 || texture >= (int)mi->tex_src.size()) return 0;
    *out = mi->tex_src[(size_t)texture];
    return 1;
}

extern "C" int orre_model_mesh_owner(const OrreModel* m, int mesh, int* joint, int* dobj) {
    if (!m || !m->internal) return 0;
    const auto* mi = static_cast<const ModelInternal*>(m->internal);
    if (mesh < 0 || mesh >= (int)mi->skins.size()) return 0;
    if (joint) *joint = mi->skins[(size_t)mesh].owner;
    if (dobj) *dobj = mi->skins[(size_t)mesh].dobj;
    return 1;
}

// A model straight from an archive entry (the Moves preview's arena, 2026-11-19; snatcherLeo: "have something really basic -
// but with a visible platform and area ... that Phenac City arena"): a map DAT is an HSD archive whose scene_data lists
// its model sets, which parse_hsd_model already walks. Platforms: every OS.
extern "C" int orre_fsys_model(OrreDisc* d, const char* archive, int entry, OrreModel* out) {
    if (!d || !archive || !out) return 0;
    size_t n = 0;
    uint8_t* p = orre_fsys_entry(d, archive, entry, &n);
    if (!p) return 0;
    const Bytes dat(p, p + n);
    orre_free(p);
    *out = OrreModel{};
    return parse_hsd_model(dat, *out) ? 1 : 0;
}

extern "C" void orre_model_free(OrreModel* m) {
    if (!m) return;
    delete static_cast<ModelInternal*>(m->internal);
    orre_particle_bank_free(m->particles);
    for (int i = 0; i < m->mesh_count; ++i) std::free(m->meshes[i].vertices);
    for (int i = 0; i < m->texture_count; ++i) std::free(m->textures[i].rgba);
    std::free(m->meshes);
    std::free(m->textures);
    *m = OrreModel{};
}
