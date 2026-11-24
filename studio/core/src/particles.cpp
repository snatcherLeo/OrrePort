// particles.cpp: particle banks (GPT1) and, later, the particle simulation (2026-11-19).
//
// Why: the Moves tab's version 2 and the Overview show the effects the game plays (snatcherLeo, 2026-11-19: "Sure we can do
// your plan"; step 1 is the Pokémon's own effects: Charizard's tail flame, Vaporeon's sparkle). The bank layout is the
// loader's (gsParticleBankLoad in src/game/field_range_80117E58.c; psInitDataBank in src/game/particle.c) as written up
// in docs/engine/particles.md, and matches tools/local/colo/gpt1.py, which the golden tests compare against.
// Platforms: plain C++20, every OS (the bank stays big-endian; values are read as they are needed).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>

#include "internal.hpp"

using namespace orre;

struct OrreParticleBank {
    Bytes data;  // the whole bank, big-endian as on the disc
    uint32_t desc = 0, tex = 0, ref = 0;  // the command block, the texture block, the id table (offsets in data)
    uint16_t version = 0;
    uint32_t first_id = 0, count = 0, table = 0;  // the emitter offset table (offset in data)
    std::vector<uint32_t> emitter_off;             // per emitter: its header's offset in data (0 = none)
    std::vector<uint32_t> emitter_end;             // per emitter: where its byte-code ends
    struct Group {
        uint32_t fmt = 0, tlut = 0, w = 0, h = 0, num = 0;
        std::vector<OrreImage> images;  // decoded (rgba NULL when a format isn't decodable)
        std::vector<uint8_t> present;   // the image's offset is set (the game's texTable[i] != NULL)
    };
    std::vector<Group> groups;
    ~OrreParticleBank() {
        for (auto& g : groups)
            for (auto& im : g.images) std::free(im.rgba);
    }
};

extern "C" OrreParticleBank* orre_particle_bank_read(const uint8_t* p, size_t size) {
    if (!p || size < 0x14 || std::memcmp(p, "GPT1", 4) != 0) return nullptr;
    auto b = std::make_unique<OrreParticleBank>();
    b->data.assign(p, p + size);
    const Bytes& d = b->data;
    b->desc = be32(d, 4);
    b->tex = be32(d, 8);
    b->ref = be32(d, 0x10);
    if (!b->desc || b->desc >= size || b->tex > size || b->desc >= b->tex) return nullptr;
    // the command block: u16 version (0x40-0x43; version 0 has no first id), u16, [u32 first id], u32 count, offsets
    b->version = be16(d, b->desc);
    if (b->version == 0) {
        b->count = be32(d, b->desc + 4);
        b->table = b->desc + 8;
    } else {
        b->first_id = be32(d, b->desc + 4);
        b->count = be32(d, b->desc + 8);
        b->table = b->desc + 12;
    }
    if (b->count > 4096 || b->table + 4 * (size_t)b->count > size) return nullptr;
    // each emitter's byte-code runs to the next emitter's start, or to the texture block (as gpt1.py reads it)
    std::vector<uint32_t> starts;
    for (uint32_t i = 0; i < b->count; ++i) {
        const uint32_t o = be32(d, b->table + 4 * i);
        b->emitter_off.push_back(o ? b->desc + o : 0);
        if (o) starts.push_back(o);
    }
    std::sort(starts.begin(), starts.end());
    starts.push_back(b->tex - b->desc);
    for (uint32_t i = 0; i < b->count; ++i) {
        const uint32_t o = be32(d, b->table + 4 * i);
        uint32_t end = 0;
        if (o) end = b->desc + *std::upper_bound(starts.begin(), starts.end(), o);
        b->emitter_end.push_back(end);
    }
    // texture groups: u32 count, offsets from the block start; a group is u32 count, GX format, palette format,
    // width, height, u16 palette count, u16 palette flags, then image (and palette) offsets from the block start
    const uint32_t ngroups = be32(d, b->tex);
    if (ngroups > 1024) return nullptr;
    for (uint32_t k = 0; k < ngroups; ++k) {
        OrreParticleBank::Group g;
        const uint32_t go = be32(d, b->tex + 4 + 4 * k);
        if (go) {
            const size_t gp = b->tex + go;
            g.num = be32(d, gp);
            g.fmt = be32(d, gp + 4);
            g.tlut = be32(d, gp + 8);
            g.w = be32(d, gp + 12);
            g.h = be32(d, gp + 16);
            const uint16_t palnum = be16(d, gp + 20), palflag = be16(d, gp + 22);
            const bool paletted = g.fmt == 8 || g.fmt == 9 || g.fmt == 10;
            const uint32_t n_entries = !paletted ? g.num : (palflag & 1) ? g.num + 1 : palnum ? g.num + palnum : g.num * 2;
            for (uint32_t i = 0; i < g.num && i < 256; ++i) {
                OrreImage im{};
                const uint32_t io = be32(d, gp + 0x18 + 4 * i);
                std::vector<uint32_t> pal;
                if (paletted) {
                    // one shared palette (flag bit 0), or one per image (no paletted groups on the US disc: particles.md)
                    const uint32_t pi = (palflag & 1) ? g.num : g.num + i;
                    const uint32_t po = pi < n_entries ? be32(d, gp + 0x18 + 4 * pi) : 0;
                    const int n = g.fmt == 8 ? 16 : g.fmt == 9 ? 256 : 16384;
                    if (po && b->tex + po < size)
                        pal = decode_tlut(d.data() + b->tex + po, size - (b->tex + po), (int)std::min<uint32_t>(g.tlut, 2), n);
                }
                std::vector<uint8_t> rgba;
                if (io && g.w && g.h && g.w <= 1024 && g.h <= 1024 && b->tex + io < size &&
                    decode_gx(d.data() + b->tex + io, size - (b->tex + io), (int)g.fmt, (int)g.w, (int)g.h, paletted ? &pal : nullptr,
                              rgba)) {
                    im.width = (int)g.w;
                    im.height = (int)g.h;
                    im.rgba = static_cast<uint8_t*>(std::malloc(rgba.size()));
                    std::memcpy(im.rgba, rgba.data(), rgba.size());
                }
                g.images.push_back(im);
                g.present.push_back(io != 0);
            }
        }
        b->groups.push_back(std::move(g));
    }
    return b.release();
}

extern "C" void orre_particle_bank_free(OrreParticleBank* b) { delete b; }

extern "C" int orre_particle_bank_count(const OrreParticleBank* b, int* first_id) {
    if (!b) return 0;
    if (first_id) *first_id = (int)b->first_id;
    return (int)b->count;
}

extern "C" int orre_particle_emitter(const OrreParticleBank* b, int i, OrreParticleEmitter* out) {
    if (!b || !out || i < 0 || i >= (int)b->count) return 0;
    *out = OrreParticleEmitter{};
    out->index = i;
    out->id = (int)b->first_id + i;
    const uint32_t o = b->emitter_off[(size_t)i];
    if (!o) return 1;
    const Bytes& d = b->data;
    out->present = 1;
    out->type = be16(d, o);
    out->tex_group = be16(d, o + 2);
    out->gen_life = be16(d, o + 4);
    out->life = be16(d, o + 6);
    out->kind = be32(d, o + 8);
    float f[12];
    for (int k = 0; k < 12; ++k) f[k] = bef32(d, o + 0x0C + 4 * k);
    out->gravity = f[0];
    out->friction = f[1];
    out->vel[0] = f[2];
    out->vel[1] = f[3];
    out->vel[2] = f[4];
    out->radius = f[5];
    out->angle = f[6];
    out->random = f[7];
    out->size = f[8];
    out->param[0] = f[9];
    out->param[1] = f[10];
    out->param[2] = f[11];
    const uint32_t end = b->emitter_end[(size_t)i];
    out->code_size = end > o + 0x3C ? (int)(end - (o + 0x3C)) : 0;
    return 1;
}

extern "C" const uint8_t* orre_particle_emitter_code(const OrreParticleBank* b, int i) {
    if (!b || i < 0 || i >= (int)b->count || !b->emitter_off[(size_t)i]) return nullptr;
    return b->data.data() + b->emitter_off[(size_t)i] + 0x3C;
}

extern "C" int orre_particle_group_count(const OrreParticleBank* b) { return b ? (int)b->groups.size() : 0; }

extern "C" int orre_particle_group(const OrreParticleBank* b, int g, int* fmt, int* w, int* h, int* images) {
    if (!b || g < 0 || g >= (int)b->groups.size()) return 0;
    const auto& gr = b->groups[(size_t)g];
    if (fmt) *fmt = (int)gr.fmt;
    if (w) *w = (int)gr.w;
    if (h) *h = (int)gr.h;
    if (images) *images = (int)gr.images.size();
    return 1;
}

extern "C" const OrreImage* orre_particle_group_image(const OrreParticleBank* b, int g, int i) {
    if (!b || g < 0 || g >= (int)b->groups.size()) return nullptr;
    const auto& gr = b->groups[(size_t)g];
    if (i < 0 || i >= (int)gr.images.size() || !gr.images[(size_t)i].rgba) return nullptr;
    return &gr.images[(size_t)i];
}

extern "C" uint32_t orre_particle_bank_ref(const OrreParticleBank* b, int i) {
    if (!b || i < 0 || i >= (int)b->count) return 0;
    return be32(b->data, b->ref + 4 * (size_t)i);
}

// ==== The simulation (2026-11-19): HSD's particle system, ported line by line ====
// Each function below names the game's function it follows; the order of operations, the float types and every
// random draw are kept as the game has them, so a seed gives the same run (the PC port's ORRE_PTCL_LOG / ORRE_RNG_TRACE
// are the reference). Pointers and free lists as the game's (a particle keeps its place in its list; the list order is
// the drawing order). Left out for now, as the Pokémon's own effects never reach them (Studio's port map, 2026-11-19):
// application SRTs (kind 0x20000, the 0xF5 / 0xF6 / 0xB2 follow modes), the camera-basis emission (kind 0x10000), texture
// forms, palettes and points. Platforms: plain C++20; sinf / cosf / atan2f / sqrtf from the C library, which can differ
// from the console's MSL in the last bit (a bit-exact replay would need the console's routines; likely invisible).

namespace {

constexpr float kFltEpsilon = 1.1920929e-07f, kFltMin = 1.17549435e-38f;
constexpr double kPi = 3.14159265358979323846;

struct V3 { float x = 0, y = 0, z = 0; };
struct Col { uint8_t r = 0, g = 0, b = 0, a = 0; };
using Mtx = float[3][4];

struct Gen;
// HSD_psAppSRT (psstructs.h; src/game/ps_exact_8016A9B4.c, psinterpret.c, psdisp.c; release step 4, verified from
// the code): a frame a generator and its particles share. Created for an emitter whose kind has 0x20000 (on the US
// disc only the emitters a particle starts, never the ones a move's entry starts: orrestudio-cli wzxscan), with
// translate 0, no rotation and scale 1; nothing in the game sets its rotation or scale, only its translate (where the
// starting particle was). While `screen` (the struct's byte 0x72, set for a generator's own frame and cleared by
// command 0xF5) the particles' positions are offsets in the camera's axes around translate (psDispSubAppSRT:
// rotation of the inverse view, then + translate): a burst that always faces the camera. Otherwise positions are
// translate + position. Shared by reference (usedCount); a generator whose frame others still use lives on.
struct AppSRT {
    V3 translate;
    bool screen = false;
    Gen* gp = nullptr;
};
// HSD_Particle (include/sysdolphin/baselib/psstructs.h)
struct Part {
    Part* next = nullptr;
    uint32_t kind = 0;
    uint8_t bank = 0, texGroup = 0, poseNum = 0, palNum = 0;
    uint16_t sizeCount = 0, primColCount = 0, envColCount = 0;
    Col primCol, envCol;
    uint16_t cmdWait = 0;
    uint8_t loopCount = 0, linkNo = 0;
    uint16_t idnum = 0;
    const uint8_t* cmdList = nullptr;
    uint16_t cmdPtr = 0, cmdMarkPtr = 0, cmdLoopPtr = 0, life = 0;
    V3 vel;
    float grav = 0, fric = 0;
    V3 pos;
    float size = 0, rotate = 0;
    uint16_t aCmpCount = 0;
    uint8_t aCmpMode = 0, aCmpParam1 = 0, aCmpParam2 = 0, pJObjOfs = 0;
    uint16_t matColCount = 0, ambColCount = 0, rotateCount = 0;
    float sizeTarget = 0, rotateTarget = 0, x68 = 0;
    uint16_t primColRemain = 0, envColRemain = 0;
    Col primColTarget, envColTarget;
    uint16_t matColRemain = 0, ambColRemain = 0, aCmpRemain = 0;
    uint8_t aCmpParam1Target = 0, aCmpParam2Target = 0;
    uint8_t matRGB = 0, matA = 0, ambRGB = 0, ambA = 0, matRGBTarget = 0, matATarget = 0, ambRGBTarget = 0, ambATarget = 0;
    float trail = 0;
    Gen* gen = nullptr;
    std::shared_ptr<AppSRT> app;  // its application SRT (psAttachParticleAppSRT), or none
};
// HSD_Generator (psstructs.h; generator.c). The aux union is kept as separate fields; the tornado's speed shares
// line.x2 as in the game (generateParticle writes aux.line.x2, the tornado motion reads aux.tornado.vel: one place).
struct Gen {
    Gen* next = nullptr;
    uint32_t kind = 0;
    float random = 0, count = 0;
    uint16_t genLife = 0, type = 0;
    uint8_t bank = 0, linkNo = 0, texGroup = 0, x17 = 0;
    uint16_t idnum = 0, life = 0;
    const uint8_t* cmdList = nullptr;
    V3 pos, vel;
    float grav = 0, fric = 0, size = 0, radius = 0, angle = 0;
    uint32_t numChild = 0;
    struct { float minAngle = 0, maxAngle = 0, height = 0; } cone;
    struct { float x2 = 0, y2 = 0, z2 = 0; } line;
    struct { float x = 0, y = 0, z = 0, xx = 0, xy = 0, xz = 0, yx = 0, yy = 0, yz = 0, zx = 0, zy = 0, zz = 0; uint16_t flag = 0; } rect;
    struct { float speed = 0, latMid = 0, latRange = 0, lonMid = 0, lonRange = 0; } sphere;
    uint16_t posFlags = 0, gfxIdx = 0;
    V3 rot, scale, scale2;
    // the joint psLinkChildGensToJObj links (posFlags 1): which node's joint, its world position kept in the system's
    // jobj table (the game reads the joint's matrix each time; here the follow step writes it). 0 = none
    int jobj = 0;
    std::shared_ptr<AppSRT> app;  // its application SRT, or none
};
// one emitter as psInitDataBankLocate leaves it (kind bits 25-27 forced to 4, the sort group: particle.c 154-159)
struct Emitter {
    bool present = false;
    uint16_t type = 0, texGroup = 0, genLife = 0, life = 0;
    uint32_t kind = 0;
    float grav = 0, fric = 0, vx = 0, vy = 0, vz = 0, radius = 0, angle = 0, random = 0, size = 0, p1 = 0, p2 = 0, p3 = 0;
    const uint8_t* cmd = nullptr;
};
struct BankSlot {
    const OrreParticleBank* bank = nullptr;
    int first = 0;
    std::vector<Emitter> by_id;  // indexed by absolute id (psCmdList[bank] = table - firstId)
};
// a gsParticleCreate node (field_particle.h): the generator, whether it never finishes, where it is placed
// (2026-11-19, release step 4: the move effects' transform, src/game/field_range_80117E58.c gsParticleUpdate,
// gsParticleLinkToJObj and the setters fn_80118DE0 / fn_80118E8C / gsParticleSetVecB; verified from the code):
// unlinked (state 0), position / rotation / scale go straight to the generator; linked to a joint (state 1-7), they
// become an offset ("transformed": rotated by the joint and added, rotation added, scale multiplied) and the state
// says which of the joint's position / rotation / scale the generator follows
struct Node {
    Gen* gen = nullptr;
    bool infinite = false;
    int handle = 0;
    V3 position;                                // +0x08: the generator's position when created (fn_80118F7C reads it)
    V3 local_pos, local_rot, local_scale{1, 1, 1};
    V3 tr_pos, tr_rot, tr_scale{1, 1, 1};       // the offset while linked
    int state = 0;                              // transform_state: 0 unlinked, 1-7 what follows the joint
    bool once = false, pending = false;         // field_05 (place once) and field_06 (a follow is due)
};

// PSMTXRotRad / PSMTXConcat / PSMTXMultVec / PSMTXRotAxisRad (row-major 3 x 4, as the SDK's)
void mtx_identity(Mtx m) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c) m[r][c] = r == c ? 1.0f : 0.0f;
}
void mtx_rot(Mtx m, char axis, float a) {
    const float s = std::sin(a), c = std::cos(a);
    mtx_identity(m);
    if (axis == 'X') { m[1][1] = c; m[1][2] = -s; m[2][1] = s; m[2][2] = c; }
    else if (axis == 'Y') { m[0][0] = c; m[0][2] = s; m[2][0] = -s; m[2][2] = c; }
    else { m[0][0] = c; m[0][1] = -s; m[1][0] = s; m[1][1] = c; }
}
void mtx_concat(const Mtx a, const Mtx b, Mtx out) {
    Mtx t;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) t[r][c] = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c];
        t[r][3] = a[r][0] * b[0][3] + a[r][1] * b[1][3] + a[r][2] * b[2][3] + a[r][3];
    }
    std::memcpy(out, t, sizeof t);
}
V3 mtx_mul(const Mtx m, const V3& v) {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3], m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3],
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]};
}
V3 normalize(V3 v) {
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l > 0) { v.x /= l; v.y /= l; v.z /= l; }
    return v;
}
void mtx_rot_axis(Mtx m, V3 axis, float a) {
    axis = normalize(axis);
    const float s = std::sin(a), c = std::cos(a), t = 1.0f - c;
    const float x = axis.x, y = axis.y, z = axis.z;
    m[0][0] = t * x * x + c; m[0][1] = t * x * y - s * z; m[0][2] = t * x * z + s * y; m[0][3] = 0;
    m[1][0] = t * x * y + s * z; m[1][1] = t * y * y + c; m[1][2] = t * y * z - s * x; m[1][3] = 0;
    m[2][0] = t * x * z - s * y; m[2][1] = t * y * z + s * x; m[2][2] = t * z * z + c; m[2][3] = 0;
}

}  // namespace

struct OrreParticleSystem {
    BankSlot banks[64];
    Part* active[16] = {};        // activeParticle[link] (newest first)
    Gen* activeGen = nullptr;     // activeGenerator
    Gen* lastGP = nullptr;        // lastGP: kept between calls, as the game's global (where a new generator is linked)
    uint16_t idNum = 0;           // psIDNum
    uint32_t rng = 1;             // HSD_Randf's state
    bool point_set[8] = {};
    std::map<int, V3> jobj_pos;  // a linked joint's world position, by node handle (Gen::jobj)
    V3 point[8];                  // psPointJObj[8]: their world positions
    std::vector<Node> nodes;
    int next_handle = 1;
    // every particle and generator ever made, so the free lists can hand them back (the game's HSD heap)
    std::vector<std::unique_ptr<Part>> part_store;
    std::vector<std::unique_ptr<Gen>> gen_store;
    Part* freePart = nullptr;
    Gen* freeGen = nullptr;
    int live = 0, live_gens = 0;

    // HSD_Randf (src/hsd/random.c)
    float randf() {
        rng = rng * 214013u + 2531011u;
        return (float)(rng >> 16) / 65536.0f;
    }
    uint16_t new_id() {  // psGetNewIDNum / psGeneratorAlloc's numbering
        idNum++;
        if (idNum < 0x100) idNum = 0x100;
        return idNum;
    }
    const Emitter* emitter(int bank, int id) const {
        if (bank < 0 || bank >= 64) return nullptr;
        const BankSlot& b = banks[bank];
        if (id < 0 || id >= (int)b.by_id.size() || !b.by_id[(size_t)id].present) return nullptr;
        return &b.by_id[(size_t)id];
    }
    // texTable[poseNum] != NULL for the particle's texture group
    bool has_image(const Part* pp) const {
        const OrreParticleBank* b = banks[pp->bank].bank;
        if (!b || pp->texGroup >= b->groups.size()) return false;
        const auto& g = b->groups[pp->texGroup];
        return pp->poseNum < g.present.size() && g.present[pp->poseNum];
    }
    int bank_ref(int bank, int id) const {  // psBankRef[bank][id]
        const OrreParticleBank* b = banks[bank].bank;
        return b ? (int)orre_particle_bank_ref(b, id) : id;
    }

    // ---- the particle list (pslist.c) ----
    Part* list_new(Part* parent, int link) {  // _psListNew: at the head, or right after the parent
        if (!freePart) {
            part_store.push_back(std::make_unique<Part>());
            freePart = part_store.back().get();
        }
        Part* pp = freePart;
        freePart = pp->next;
        ++live;
        if (!parent) {
            pp->next = active[link];
            active[link] = pp;
        } else {
            pp->next = parent->next;
            parent->next = pp;
        }
        return pp;
    }
    void list_delete(Part* pp, Part* parent) {  // _psListDelete
        if (!parent) active[pp->linkNo] = pp->next;
        else parent->next = pp->next;
        pp->app.reset();  // (psRemoveParticleAppSRT, as the game's deaths do)
        pp->next = freePart;
        freePart = pp;
        --live;
    }
    void delete_point(Part* pp) {  // psDeletePntJObjwithParticle
        if (pp->kind & 0x8000) point_set[(pp->kind >> 12) & 7] = false;
    }

    // ---- particles (particle.c) ----
    Part* generate(Part* parent, int link, int bank, uint32_t kind, uint16_t texGroup, const uint8_t* list, int life, float x,
                   float y, float z, float vx, float vy, float vz, float size, float grav, float fric, int palflag, Gen* gp,
                   bool interpret) {  // psGenerateParticle0
        Part* pp = list_new(parent, link);
        pp->idnum = gp ? gp->idnum : new_id();
        pp->bank = (uint8_t)bank;
        pp->linkNo = (uint8_t)link;
        pp->kind = kind;
        pp->texGroup = (uint8_t)texGroup;
        pp->pos = {x, y, z};
        pp->vel = {vx, vy, vz};
        pp->size = size;
        pp->grav = grav;
        pp->fric = fric;
        pp->life = (uint16_t)(life + 1);
        pp->cmdList = list;
        pp->cmdMarkPtr = 0;
        pp->cmdPtr = 0;
        if (palflag) pp->kind |= 0x10;
        pp->cmdWait = list != nullptr;
        pp->poseNum = 0;
        pp->palNum = 0xFF;
        pp->primCol = {0xFF, 0xFF, 0xFF, 0xFF};
        pp->envCol = {0, 0, 0, 0};
        pp->envColCount = pp->primColCount = pp->sizeCount = 0;
        pp->envColRemain = pp->primColRemain = 0;
        pp->aCmpMode = 0x33;
        pp->aCmpParam1 = ((pp->kind >> 22) & 3) >= 2 ? 0 : 1;
        pp->aCmpParam2 = 0xFF;
        pp->aCmpRemain = pp->aCmpCount = 0;
        pp->rotateCount = 0;
        pp->x68 = pp->rotateTarget = pp->rotate = 0.0f;
        pp->gen = gp;
        if (gp) gp->numChild++;
        pp->pJObjOfs = 0;
        pp->matColRemain = pp->matColCount = 0;
        pp->matRGB = pp->matA = 0xFF;
        pp->ambColRemain = pp->ambColCount = 0;
        pp->ambRGB = pp->ambA = 0xFF;
        pp->trail = 1.0f;
        pp->app.reset();
        if (gp && gp->app) pp->app = gp->app;  // psAttachParticleAppSRT
        if (interpret) interpret_one(pp, nullptr);
        return pp;
    }
    Part* generate_id(Part* parent, int link, int bank, int id) {  // psGenerateParticleID0 (never interprets here)
        if (link >= 8 || bank >= 64) return nullptr;
        const Emitter* e = emitter(bank, id);
        if (!e) return nullptr;
        return generate(parent, link, bank, e->kind, e->texGroup, e->cmd, e->life, 0, 0, 0, e->vx, e->vy, e->vz, e->size, e->grav,
                        e->fric, 0, nullptr, false);
    }

    // ---- generators (generator.c) ----
    Gen* gen_alloc() {  // psGeneratorAlloc
        if (!freeGen) {
            gen_store.push_back(std::make_unique<Gen>());
            freeGen = gen_store.back().get();
        }
        Gen* gen = freeGen;
        freeGen = gen->next;
        ++live_gens;
        if (!lastGP || !lastGP->next) {
            if (!activeGen) {
                gen->next = nullptr;
                activeGen = gen;
            } else {
                gen->next = activeGen->next;
                activeGen->next = gen;
            }
        } else {
            gen->next = lastGP->next->next;
            lastGP->next->next = gen;
        }
        gen->idnum = new_id();
        gen->x17 = 0;
        return gen;
    }
    // psSetGeneratorAngleRadiusScale (src/game/particle.c; verified from the code): the emitter's shape parameters and
    // radius times the scale (the average for angles), gravity and friction too with motion, and the scale kept (scale2)
    void angle_radius_scale(Gen* gp, const float s[3], bool motion) {
        const Emitter* e = emitter(gp->bank, gp->gfxIdx);
        if (!e) return;
        const float avg = (s[0] + s[1] + s[2]) / 3.0f;
        switch (gp->type & 0xF) {
            case 0: case 3: case 4:
                gp->cone.minAngle = avg * e->p1;
                gp->cone.maxAngle = avg * e->p2;
                break;
            case 1:
                gp->line.x2 = s[0] * e->p1;
                gp->line.y2 = s[1] * e->p2;
                gp->line.z2 = s[2] * e->p3;
                break;
            case 6: case 7:
                gp->cone.minAngle = avg * e->p1;
                gp->cone.maxAngle = avg * e->p2;
                gp->cone.height = avg * e->p3;
                break;
            case 5:
                gp->rect.xx = gp->rect.x = s[0] * e->p1;
                gp->rect.yy = gp->rect.y = s[1] * e->p2;
                gp->rect.zz = gp->rect.z = s[2] * e->p3;
                break;
            case 8:
                gp->sphere.latRange = avg * e->p1;
                gp->sphere.lonRange = avg * e->p2;
                break;
            default: break;
        }
        gp->radius = avg * e->radius;
        if (motion) {
            gp->grav *= avg;
            gp->fric *= avg;
            gp->posFlags |= 0x1000;
        }
        gp->scale2 = {s[0], s[1], s[2]};
    }

    Gen* create_gen(int link, int bank, int idx) {  // psCreateGeneratorID
        if (bank >= 64 || link >= 8) return nullptr;
        const Emitter* e = emitter(bank, idx);
        if (!e) return nullptr;
        Gen* gen = gen_alloc();
        gen->type = e->type;
        gen->bank = (uint8_t)bank;
        gen->linkNo = (uint8_t)link;
        gen->gfxIdx = (uint16_t)idx;
        gen->kind = e->kind;
        gen->texGroup = (uint8_t)e->texGroup;
        gen->life = e->life;
        gen->genLife = e->genLife;
        gen->pos = {0, 0, 0};
        gen->vel = {e->vx, e->vy, e->vz};
        gen->grav = e->grav;
        gen->fric = e->fric;
        gen->size = e->size;
        gen->cmdList = e->cmd;
        gen->radius = e->radius;
        gen->angle = e->angle;
        gen->random = e->random;
        if (gen->kind & 0x100) {
            if (gen->random < 0.0f) gen->count = (1.0f + gen->random > kFltEpsilon) ? 1.0f : 0.0f;
            else gen->count = 1.0f - kFltEpsilon;
        } else if (gen->random < 0.0f) {
            gen->count = 0.0f;
        } else {
            gen->count = randf();
        }
        // (palette flag -> kind 0x10: no paletted groups on the US disc, particles.md)
        gen->numChild = 0;
        switch (gen->type & 0xF) {
            case 0: case 3: case 4: case 6: case 7:
                if (0.0f == e->p1 && 0.0f == e->p2) {
                    gen->cone.minAngle = 0.0f;
                    gen->cone.maxAngle = 6.2831855f;
                } else {
                    gen->cone.minAngle = e->p1;
                    gen->cone.maxAngle = e->p2;
                }
                if ((gen->type & 0xF) >= 6) gen->cone.height = e->p3;
                break;
            case 1:
                gen->line.x2 = e->p1;
                gen->line.y2 = e->p2;
                gen->line.z2 = e->p3;
                break;
            case 2:
                break;
            case 5:
                gen->rect.xx = gen->rect.x = e->p1;
                gen->rect.yy = gen->rect.y = e->p2;
                gen->rect.zz = gen->rect.z = e->p3;
                gen->rect.zy = gen->rect.zx = gen->rect.yz = gen->rect.yx = gen->rect.xz = gen->rect.xy = 0.0f;
                gen->rect.flag = 0;
                if (e->p1 < 0.0f) gen->rect.flag |= 1;
                if (e->p2 < 0.0f) gen->rect.flag |= 2;
                if (e->p3 < 0.0f) gen->rect.flag |= 4;
                break;
            case 8: {
                gen->sphere.speed = std::sqrt(gen->vel.z * gen->vel.z + (gen->vel.x * gen->vel.x + gen->vel.y * gen->vel.y));
                const float mag = std::sqrt(gen->vel.x * gen->vel.x + gen->vel.z * gen->vel.z);
                gen->sphere.latMid = mag < kFltMin ? (gen->vel.y >= 0.0f ? 1.5707964f : -1.5707964f) : std::atan2(gen->vel.y, mag);
                gen->sphere.lonMid = std::fabs(gen->vel.x) < kFltMin ? (gen->vel.z >= 0.0f ? 1.5707964f : -1.5707964f)
                                                                       : std::atan2(gen->vel.z, gen->vel.x);
                gen->sphere.latRange = e->p1;
                if (gen->sphere.latRange < 0.0f) {
                    gen->sphere.latRange = -gen->sphere.latRange;
                    gen->sphere.speed = -gen->sphere.speed;
                }
                gen->sphere.lonRange = e->p2;
                break;
            }
            default:
                break;  // psExtCreateGenFunc: none in this game
        }
        // kind 0x20000: a frame of its own (psAddGeneratorAppSRT; its byte 0x72 set: the camera-facing kind)
        gen->app.reset();
        if (gen->kind & 0x20000) {
            gen->type |= 0x800;
            gen->app = std::make_shared<AppSRT>();
            gen->app->screen = true;
            gen->app->gp = gen;
        }
        gen->posFlags = 2;
        gen->jobj = 0;
        gen->rot = {0, 0, 0};
        gen->scale = {1, 1, 1};
        gen->scale2 = {1, 1, 1};
        return gen;
    }
    void kill_gen_child(Gen* gp) {  // psKillGeneratorChild
        Part* prev = nullptr;
        for (Part* pp = active[gp->linkNo]; pp;) {
            Part* next = pp->next;
            if (pp->idnum == gp->idnum && pp->gen == gp) {
                pp->gen->numChild--;
                delete_point(pp);
                list_delete(pp, prev);
            } else {
                prev = pp;
            }
            pp = next;
        }
    }
    Gen* delete_gen(Gen* gen, Gen* prev) {  // psDeleteGenerator
        if (gen->type & 0x80) kill_gen_child(gen);
        if (gen->numChild != 0) {
            gen->random = 0.0f;
            gen->genLife = 1;
            return gen;
        }
        // a generator whose own frame others still use lives on (its usedCount, here the shared pointer's count)
        if ((gen->type & 0x3800) && gen->app && gen->app->gp == gen && gen->app.use_count() != 1) {
            gen->random = 0.0f;
            gen->genLife = 1;
            return gen;
        }
        if (!prev) activeGen = gen->next;
        else prev->next = gen->next;
        if (gen->app) {  // psRemoveGeneratorAppSRT
            if (gen->app->gp == gen) gen->app->gp = nullptr;
            gen->app.reset();
        }
        gen->next = freeGen;
        freeGen = gen;
        --live_gens;
        return prev;
    }
    void kill_gen(Gen* gen) {  // psKillGenerator
        lastGP = nullptr;
        for (Gen* cur = activeGen; cur; cur = cur->next) {
            if (cur == gen) {
                lastGP = delete_gen(gen, lastGP);
                if (lastGP) {
                    while (lastGP->next) lastGP = lastGP->next;
                } else if (activeGen) {
                    lastGP = activeGen;
                    while (lastGP->next) lastGP = lastGP->next;
                }
                return;
            }
            lastGP = cur;
        }
    }
    void kill_gen_id(uint16_t id) {  // psKillGeneratorID
        lastGP = nullptr;
        for (Gen* gen = activeGen; gen;) {
            Gen* next = gen->next;
            if (gen->idnum == id) lastGP = delete_gen(gen, lastGP);
            else lastGP = gen;
            gen = next;
        }
    }
    void kill_family(uint16_t id, int link) {  // psKillFamily
        Part* prev = nullptr;
        for (Part* pp = active[link]; pp;) {
            Part* next = pp->next;
            if (pp->idnum == id) {
                if (pp->gen) pp->gen->numChild--;
                delete_point(pp);
                list_delete(pp, prev);
            } else {
                prev = pp;
            }
            pp = next;
        }
        kill_gen_id(id);
    }
    uint32_t child_max_life(Gen* gp) {  // psGetGeneratorChildMaxLife
        uint32_t m = gp->genLife;
        for (Gen* g = activeGen; g; g = g->next)
            if (g->idnum == gp->idnum && g->genLife > m) m = g->genLife;
        return m;
    }
    uint32_t child_count(Gen* gp) {  // psGetParticleChildCount
        uint32_t n = 0;
        for (Gen* g = activeGen; g; g = g->next)
            if (g->idnum == gp->idnum) n += g->numChild;
        return n;
    }
    // genPosUpdate (generator.c): a generator linked to a joint (posFlags 1) but not placed by its node (posFlags 2)
    // takes the joint's world position
    void gen_pos_update(Gen* gen) {
        if (!gen || (gen->posFlags & 2) || !(gen->posFlags & 1) || !gen->jobj) return;
        auto it = jobj_pos.find(gen->jobj);
        if (it != jobj_pos.end()) gen->pos = it->second;
    }
    // a generator a particle starts (0xA5 / 0xEF / 0xF0; src/game/psinterpret.c, verified from the code): the parent's
    // generator data copied, then its application SRT: the parent's frame is shared (replacing the child's own when the
    // parent's generator has type 0x2000, else only when the child has none); with a frame the position becomes an
    // offset from the frame's translate, a frame of the child's own starting at the particle
    void child_gen(Part* pp, int id, int kind_bits) {
        Gen* gp = create_gen(pp->linkNo, pp->bank, id);
        if (!gp) return;
        gp->idnum = pp->idnum;
        if (pp->gen) {  // psCopyGeneratorData (src/game/ps_range_80168C64.c)
            if (pp->gen->kind & 0x20000000) gp->kind |= 0x20000000;
            gp->rot = pp->gen->rot;
            gp->scale = pp->gen->scale;
            gp->posFlags = (uint16_t)(pp->gen->posFlags & ~2);
            gp->jobj = pp->gen->jobj;
            // psSetGeneratorAngleRadiusScale with the parent's scale2, motion when the parent had it (identity for the
            // Pokémon's own effects; the move effects scale theirs)
            const float s2[3] = {pp->gen->scale2.x, pp->gen->scale2.y, pp->gen->scale2.z};
            angle_radius_scale(gp, s2, (pp->gen->posFlags & 0x1000) != 0);
        }
        if (pp->app) {
            if (pp->gen && (pp->gen->type & 0x2000)) gp->app = pp->app;  // psChangeGeneratorAppSRT
            else if (!gp->app) gp->app = pp->app;                       // psAttachGeneratorAppSRT (none yet only)
        }
        if (kind_bits >= 0) {
            gp->kind &= ~0x0E000000u;
            gp->kind |= (uint32_t)(kind_bits & 7) << 25;
        }
        if (pp->app) {
            if (gp->app) {
                gp->pos = pp->pos;
                if (gp->app != pp->app) gp->app->translate = pp->app->translate;
            }
        } else if (gp->app) {
            gp->app->translate = pp->pos;
            gp->pos = pp->pos;
        } else {
            gp->pos = pp->pos;
        }
        if (gp->app != pp->app) {
            if (!pp->app) {
                if (gp->app) {
                    gen_pos_update(gp);
                    gp->pos.x -= gp->app->translate.x;
                    gp->pos.y -= gp->app->translate.y;
                    gp->pos.z -= gp->app->translate.z;
                }
            } else {
                gen_pos_update(pp->app->gp);
                if (gp->app) {
                    gen_pos_update(gp);
                    gp->pos.x += pp->app->translate.x - gp->app->translate.x;
                    gp->pos.y += pp->app->translate.y - gp->app->translate.y;
                    gp->pos.z += pp->app->translate.z - gp->app->translate.z;
                } else {
                    gp->pos.x += pp->app->translate.x;
                    gp->pos.y += pp->app->translate.y;
                    gp->pos.z += pp->app->translate.z;
                }
            }
        }
    }
    // a particle a particle spawns (0xA4 / 0xF1 / 0xAA / 0xB9 / 0xF2)
    void child_part(Part* pp, int id, bool copy_vel) {
        Part* child = generate_id(pp, pp->linkNo, pp->bank, id);
        if (!child) return;
        child->pos = pp->pos;
        if (copy_vel) child->vel = pp->vel;
        child->idnum = pp->idnum;
        child->gen = pp->gen;
        if (pp->gen) {
            pp->gen->numChild++;
            if (pp->gen->kind & 0x20000000) child->kind |= 0x20000000;
        }
        apply_vel_local(child);
        // the parent's application SRT (psChangeParticleAppSRT / psAttachParticleAppSRT: the child has none of its own
        // here, so both give it the parent's)
        if (pp->app) child->app = pp->app;
        interpret_one(child, pp);
    }
    void apply_vel_local(Part* pp) {  // psApplyVelocityLocalRotation
        if (!pp->gen || !(pp->gen->posFlags & 4)) return;
        Mtx rx, ry, rz;
        mtx_rot(rx, 'X', pp->gen->rot.x);
        mtx_rot(ry, 'Y', pp->gen->rot.y);
        mtx_rot(rz, 'Z', pp->gen->rot.z);
        mtx_concat(ry, rx, rx);
        mtx_concat(rz, rx, rx);
        pp->vel = mtx_mul(rx, pp->vel);
    }
    void apply_offset_local(Part* pp, V3& ofs) {  // psApplyOffsetLocalRotation
        if (!pp->gen || !(pp->gen->posFlags & 8)) return;
        Mtx s, rx, ry, rz;
        mtx_identity(s);
        if (!(pp->gen->posFlags & 0x10)) { s[0][0] = pp->gen->scale.x; s[1][1] = pp->gen->scale.y; s[2][2] = pp->gen->scale.z; }
        mtx_rot(rx, 'X', pp->gen->rot.x);
        mtx_rot(ry, 'Y', pp->gen->rot.y);
        mtx_rot(rz, 'Z', pp->gen->rot.z);
        mtx_concat(ry, rx, rx);
        mtx_concat(rz, rx, rx);
        mtx_concat(s, rx, rx);
        ofs = mtx_mul(rx, ofs);
    }
    void modify_dir(Part* pp, float angle, const V3* base) {  // modifyDir; modifyDirGenBase (base = generator vel + offset)
        float vx = base ? base->x : pp->vel.x, vy = base ? base->y : pp->vel.y, vz = base ? base->z : pp->vel.z;
        const float rx = std::fabs(vz) < kFltMin ? (vy >= 0.0f ? (float)(kPi / 2) : (float)-(kPi / 2)) : std::atan2(vy, vz);
        const float sx = std::sin(rx), cx = std::cos(rx);
        const float w = vy * sx + vz * cx;
        const float ry = std::fabs(w) < kFltMin ? (vx >= 0.0f ? (float)(kPi / 2) : (float)-(kPi / 2)) : std::atan2(vx, w);
        const float sy = std::sin(ry), cy = std::cos(ry);
        const float len = base ? std::sqrt(pp->vel.x * pp->vel.x + pp->vel.y * pp->vel.y + pp->vel.z * pp->vel.z)
                               : std::sqrt(vx * vx + vy * vy + vz * vz);
        const float rnd = (float)(2.0 * (kPi * randf()));
        const float r = len * std::sin(angle);
        vx = r * std::cos(rnd);
        vy = r * std::sin(rnd);
        vz = len * std::cos(angle);
        pp->vel.x = vx * cy + vz * sy;
        pp->vel.y = sy * (-vx * sx) + vy * cx + cy * (vz * sx);
        pp->vel.z = sy * (-vx * cx) - vy * sx + cy * (vz * cx);
    }

    // ---- the byte-code (psinterpret.c psInterpretParticle0) ----
    static const uint8_t* get_time(const uint8_t* c, uint16_t& v) {
        v = *c++;
        if (v & 0x80) v = (uint16_t)(((v & 0x7F) << 8) + *c++);
        return c;
    }
    static const uint8_t* get_float(const uint8_t* c, float& v) {
        const uint32_t w = (uint32_t)c[0] << 24 | (uint32_t)c[1] << 16 | (uint32_t)c[2] << 8 | c[3];
        std::memcpy(&v, &w, 4);
        return c + 4;
    }
    static uint8_t clamp_add(uint8_t val, float add) {  // U8ClampAdd
        float f = val + add;
        if (f < 0.0f) f = 0.0f;
        if (f > 255.0f) f = 255.0f;
        return (uint8_t)f;
    }
    static void freeze(Col& col, const Col& target, uint16_t remain, uint16_t count) {  // PS_FREEZE_COLOR
        if (count == 0) return;
        const int32_t t = (int32_t)(remain << 16) / count;
        col.r = (uint8_t)(((target.r << 16) + t * (col.r - target.r)) >> 16);
        col.g = (uint8_t)(((target.g << 16) + t * (col.g - target.g)) >> 16);
        col.b = (uint8_t)(((target.b << 16) + t * (col.b - target.b)) >> 16);
        col.a = (uint8_t)(((target.a << 16) + t * (col.a - target.a)) >> 16);
    }
    float avg_scale(const Gen* g) const { return (g->scale.x + g->scale.y + g->scale.z) / 3.0f; }

    // returns the particle after pp (pp may have died)
    Part* interpret_one(Part* pp, Part* prev) {
        if (pp->kind & 0x800) return pp->next;
        if (pp->sizeCount != 0) {
            pp->size += (pp->sizeTarget - pp->size) / pp->sizeCount;
            pp->sizeCount--;
        }
        if (pp->primColCount != 0 && --pp->primColRemain == 0) { pp->primColCount = 0; pp->primCol = pp->primColTarget; }
        if (pp->envColCount != 0 && --pp->envColRemain == 0) { pp->envColCount = 0; pp->envCol = pp->envColTarget; }
        if (pp->matColCount != 0 && --pp->matColRemain == 0) { pp->matColCount = 0; pp->matRGB = pp->matRGBTarget; pp->matA = pp->matATarget; }
        if (pp->ambColCount != 0 && --pp->ambColRemain == 0) { pp->ambColCount = 0; pp->ambRGB = pp->ambRGBTarget; pp->ambA = pp->ambATarget; }
        if (pp->aCmpCount != 0 && --pp->aCmpRemain == 0) {
            pp->aCmpCount = 0;
            pp->aCmpParam1 = pp->aCmpParam1Target;
            pp->aCmpParam2 = pp->aCmpParam2Target;
        }
        if (pp->rotateCount != 0) {
            if (pp->x68 != 0.0f) {
                pp->rotate += pp->rotateTarget;
                if (pp->rotateTarget >= 0.0f) pp->rotateTarget += pp->x68;
                else pp->rotateTarget -= pp->x68;
                if (--pp->rotateCount == 0) { pp->x68 = 0.0f; pp->rotateTarget = 0.0f; }
            } else {
                pp->rotate += (pp->rotateTarget - pp->rotate) / pp->rotateCount;
                pp->rotateCount--;
            }
        }
        if (pp->cmdWait != 0 && --pp->cmdWait == 0) {
            const uint8_t* c = pp->cmdList + pp->cmdPtr;
            uint16_t time = 0;
            float val = 0, val2 = 0;
            do {
                const uint8_t cmd = *c++;
                if (cmd < 0x80) {
                    time = cmd & 0x1F;
                    if (cmd & 0x20) time = (uint16_t)((time << 8) + *c++);
                    if ((cmd & 0xC0) == 0x40) {
                        pp->poseNum = *c++;
                        if (has_image(pp)) pp->kind |= 0x400;
                    }
                    continue;
                }
                time = 0;
                uint8_t op;
                if ((op = cmd & 0xF8) <= 0x98) {
                } else if ((op = cmd & 0xF0) == 0xC0 || op == 0xD0) {
                } else {
                    op = cmd;
                }
                switch (op) {
                    case 0x80: case 0x88: case 0x90: case 0x98: {
                        // the components the command doesn't name: 0.0, as the PC port gives them and the console had
                        // for 0x88 (measured; particles.md "the position and velocity commands")
                        V3 v{0, 0, 0};
                        if (cmd & 1) { c = get_float(c, val); v.x = val; }
                        if (cmd & 2) { c = get_float(c, val); v.y = val; }
                        if (cmd & 4) { c = get_float(c, val); v.z = val; }
                        if (op == 0x98) {
                            if (!(pp->kind & 4)) apply_offset_local(pp, v);
                            else if (pp->gen && (pp->gen->posFlags & 0x40)) {
                                const float s = avg_scale(pp->gen);
                                v.x *= s; v.y *= s; v.z *= s;
                            }
                        } else {
                            apply_offset_local(pp, v);
                        }
                        if (op == 0x80) pp->pos = v;
                        else if (op == 0x88) { pp->pos.x += v.x; pp->pos.y += v.y; pp->pos.z += v.z; }
                        else if (op == 0x90) pp->vel = v;
                        else { pp->vel.x += v.x; pp->vel.y += v.y; pp->vel.z += v.z; }
                        break;
                    }
                    case 0xA0:
                        c = get_time(c, pp->sizeCount);
                        c = get_float(c, pp->sizeTarget);
                        if (pp->sizeCount == 0) pp->size = pp->sizeTarget;
                        break;
                    case 0xA1: pp->kind &= ~0x400u; break;
                    case 0xA2:
                        c = get_float(c, pp->grav);
                        if (0.0f == pp->grav) pp->kind &= ~1u; else pp->kind |= 1;
                        if (pp->gen && (pp->gen->posFlags & 0x1000)) pp->grav *= avg_scale(pp->gen);
                        break;
                    case 0xA3:
                        c = get_float(c, pp->fric);
                        if (1.0f == pp->fric) pp->kind &= ~2u; else pp->kind |= 2;
                        if (pp->gen && (pp->gen->posFlags & 0x1000)) pp->fric *= avg_scale(pp->gen);
                        break;
                    case 0xA4: case 0xF1: {
                        int id = c[0] << 8 | c[1];
                        c += 2;
                        if (op == 0xF1) id = bank_ref(pp->bank, id);
                        child_part(pp, id, false);
                        break;
                    }
                    case 0xA5: {
                        const int id = c[0] << 8 | c[1];
                        c += 2;
                        child_gen(pp, id, -1);
                        break;
                    }
                    case 0xEF: case 0xF0: {
                        int id = c[0] << 8 | c[1];
                        const int k = c[2];
                        c += 3;
                        if (op == 0xF0) id = bank_ref(pp->bank, id);
                        child_gen(pp, id, k);
                        break;
                    }
                    case 0xA6: {
                        const int base = c[0] << 8 | c[1], range = c[2] << 8 | c[3];
                        c += 4;
                        pp->life = (uint16_t)(base + (int32_t)(range * randf()));
                        break;
                    }
                    case 0xA7: {
                        const int rate = *c++;
                        if (rate >= (int32_t)(100.0f * randf())) { pp->life = 1; goto exit; }
                        break;
                    }
                    case 0xA8: {
                        V3 o;
                        c = get_float(c, val); o.x = 2.0f * val * randf() - val;
                        c = get_float(c, val); o.y = 2.0f * val * randf() - val;
                        c = get_float(c, val); o.z = 2.0f * val * randf() - val;
                        apply_offset_local(pp, o);
                        pp->pos.x += o.x; pp->pos.y += o.y; pp->pos.z += o.z;
                        break;
                    }
                    case 0xA9:
                        c = get_float(c, val);
                        modify_dir(pp, val, nullptr);
                        break;
                    case 0xF4: {
                        float y, z;
                        c = get_float(c, val2);
                        c = get_float(c, y);
                        c = get_float(c, z);
                        c = get_float(c, val);
                        if (pp->gen) {
                            const V3 b{pp->gen->vel.x + val2, pp->gen->vel.y + y, pp->gen->vel.z + z};
                            modify_dir(pp, val, &b);
                        }
                        break;
                    }
                    // the application SRT's modes (psinterpret.c): 0xF5 its children share the frame (type 0x2000)
                    // and it stops facing the camera; 0xF6 type 0x1000 (kept alive while used)
                    case 0xF5:
                        if (pp->gen && pp->gen->app) {
                            const bool done = (pp->gen->type & 0x2000) != 0;
                            pp->gen->type |= 0x2000;
                            pp->gen->app->screen = false;
                            if (!done) gen_pos_update(pp->gen);
                        }
                        break;
                    case 0xF6:
                        if (pp->gen && pp->gen->app) {
                            const bool done = (pp->gen->type & 0x1000) != 0;
                            pp->gen->type |= 0x1000;
                            if (!done) gen_pos_update(pp->gen);
                        }
                        break;
                    case 0xF7: pp->kind |= 0x10000000; break;
                    case 0xAA: {
                        int id = c[0] << 8 | c[1];
                        const int range = c[2] << 8 | c[3];
                        c += 4;
                        id += (int32_t)(range * randf());
                        id = bank_ref(pp->bank, id);
                        child_part(pp, id, false);
                        break;
                    }
                    case 0xAB:
                        c = get_float(c, val);
                        pp->vel.x *= val; pp->vel.y *= val; pp->vel.z *= val;
                        break;
                    case 0xAC:
                        c = get_time(c, pp->sizeCount);
                        c = get_float(c, pp->sizeTarget);
                        c = get_float(c, val);
                        pp->sizeTarget += val * randf();
                        if (pp->sizeCount == 0) pp->size = pp->sizeTarget;
                        break;
                    case 0xAD: pp->kind |= 0x80; break;
                    case 0xAE: pp->kind &= ~0x60u; break;
                    case 0xAF: pp->kind &= ~0x40u; pp->kind |= 0x20; break;
                    case 0xB0: pp->kind &= ~0x20u; pp->kind |= 0x40; break;
                    case 0xB1: pp->kind |= 0x60; break;
                    // 0xB2: bake the frame into the position and leave it (only a frame that isn't camera-facing; its
                    // matrix is a plain translation, since nothing sets its rotation or scale)
                    case 0xB2:
                        if (pp->app && !pp->app->screen) {
                            gen_pos_update(pp->app->gp);
                            pp->pos.x += pp->app->translate.x;
                            pp->pos.y += pp->app->translate.y;
                            pp->pos.z += pp->app->translate.z;
                            pp->app.reset();
                        }
                        break;
                    case 0xB3:
                        if (pp->aCmpCount != 0) {
                            const int32_t t = (int32_t)(pp->aCmpRemain << 16) / pp->aCmpCount;
                            pp->aCmpParam1 = (uint8_t)(((pp->aCmpParam1Target << 16) + t * (pp->aCmpParam1 - pp->aCmpParam1Target)) >> 16);
                            pp->aCmpParam2 = (uint8_t)(((pp->aCmpParam2Target << 16) + t * (pp->aCmpParam2 - pp->aCmpParam2Target)) >> 16);
                        }
                        c = get_time(c, pp->aCmpCount);
                        pp->aCmpMode = c[0];
                        pp->aCmpParam1Target = c[1];
                        pp->aCmpParam2Target = c[2];
                        c += 3;
                        if (pp->aCmpCount == 0) {
                            pp->aCmpParam1 = pp->aCmpParam1Target;
                            pp->aCmpParam2 = pp->aCmpParam2Target;
                            pp->aCmpRemain = 0;
                            pp->aCmpCount = 0;
                        } else {
                            pp->aCmpRemain = pp->aCmpCount;
                        }
                        break;
                    case 0xB4: pp->kind |= 0x200; break;
                    case 0xB5: pp->kind &= ~0x200u; break;
                    case 0xB6:
                        c = get_time(c, pp->rotateCount);
                        c = get_float(c, val);
                        pp->rotateTarget += val;
                        if (pp->rotateCount == 0) pp->rotate = pp->rotateTarget;
                        break;
                    case 0xB7: {  // setVelToJObj
                        const int no = *c++ + pp->pJObjOfs;
                        if (no >= 0 && no < 8 && point_set[no]) {
                            const float dx = point[no].x - pp->pos.x, dy = point[no].y - pp->pos.y, dz = point[no].z - pp->pos.z;
                            float speed = std::sqrt(pp->vel.x * pp->vel.x + pp->vel.y * pp->vel.y + pp->vel.z * pp->vel.z);
                            const float len = dx * dx + dy * dy + dz * dz;
                            if (len != 0.0f) {
                                speed = speed / std::sqrt(len);
                                pp->vel = {dx * speed, dy * speed, dz * speed};
                            }
                        }
                        break;
                    }
                    case 0xB8: {  // applyForceJObj
                        const int no = *c++ + pp->pJObjOfs;
                        c = get_float(c, val);
                        c = get_float(c, val2);
                        if (pp->gen && (pp->gen->posFlags & 0x100)) {
                            const float s = avg_scale(pp->gen);
                            val *= s;
                            val2 *= s;
                        }
                        if (no >= 0 && no < 8 && point_set[no] && val2 >= 0.0f) {
                            const float dx = point[no].x - pp->pos.x, dy = point[no].y - pp->pos.y, dz = point[no].z - pp->pos.z;
                            float len = dx * dx + dy * dy + dz * dz;
                            if (len <= val2 * val2) { pp->life = 1; goto exit; }
                            if (0.0f != len) {
                                len = val / len;
                                pp->vel.x += len * dx; pp->vel.y += len * dy; pp->vel.z += len * dz;
                            }
                        }
                        break;
                    }
                    case 0xB9: case 0xF2: {
                        int id = c[0] << 8 | c[1];
                        c += 2;
                        if (op == 0xF2) id = bank_ref(pp->bank, id);
                        child_part(pp, id, true);
                        break;
                    }
                    case 0xBA: case 0xBB: {
                        Col& col = op == 0xBA ? pp->primCol : pp->envCol;
                        Col& tgt = op == 0xBA ? pp->primColTarget : pp->envColTarget;
                        uint16_t& rem = op == 0xBA ? pp->primColRemain : pp->envColRemain;
                        const uint16_t cnt = op == 0xBA ? pp->primColCount : pp->envColCount;
                        freeze(col, tgt, rem, cnt);
                        val = (float)((int8_t)*c++ * 2) * randf(); tgt.r = clamp_add(tgt.r, val);
                        val = (float)((int8_t)*c++ * 2) * randf(); tgt.g = clamp_add(tgt.g, val);
                        val = (float)((int8_t)*c++ * 2) * randf(); tgt.b = clamp_add(tgt.b, val);
                        val = (float)((int8_t)*c++ * 2) * randf(); tgt.a = clamp_add(tgt.a, val);
                        if (cnt == 0) col = tgt;
                        else rem = cnt;
                        break;
                    }
                    case 0xBC:
                        pp->poseNum = c[0];
                        val = c[1];
                        c += 2;
                        pp->poseNum = (uint8_t)(pp->poseNum + val * randf());
                        if (has_image(pp)) pp->kind |= 0x400;
                        break;
                    case 0xBD:
                        c = get_float(c, val);
                        c = get_float(c, val2);
                        if (pp->gen && (pp->gen->posFlags & 0x200)) {
                            const float s = avg_scale(pp->gen);
                            val *= s;
                            val2 *= s;
                        }
                        val = val + val2 * randf();
                        val2 = std::sqrt(pp->vel.x * pp->vel.x + pp->vel.y * pp->vel.y + pp->vel.z * pp->vel.z);
                        if (val2 > 1e-10f) {
                            val /= val2;
                            pp->vel.x *= val; pp->vel.y *= val; pp->vel.z *= val;
                        }
                        break;
                    case 0xBE:
                        c = get_float(c, val); pp->vel.x *= val;
                        c = get_float(c, val); pp->vel.y *= val;
                        c = get_float(c, val); pp->vel.z *= val;
                        break;
                    case 0xBF: {
                        const int no = *c++;
                        pp->kind |= ((uint32_t)((no + pp->pJObjOfs) & 7) << 12) | 0x8000;
                        break;
                    }
                    case 0xC0: case 0xD0: {
                        Col& col = op == 0xC0 ? pp->primCol : pp->envCol;
                        Col& tgt = op == 0xC0 ? pp->primColTarget : pp->envColTarget;
                        uint16_t& rem = op == 0xC0 ? pp->primColRemain : pp->envColRemain;
                        uint16_t& cnt = op == 0xC0 ? pp->primColCount : pp->envColCount;
                        freeze(col, tgt, rem, cnt);
                        c = get_time(c, cnt);
                        tgt = col;
                        if (cmd & 1) tgt.r = *c++;
                        if (cmd & 2) tgt.g = *c++;
                        if (cmd & 4) tgt.b = *c++;
                        if (cmd & 8) tgt.a = *c++;
                        if (cnt == 0) { col = tgt; rem = 0; }
                        else rem = cnt;
                        break;
                    }
                    case 0xE0:
                        freeze(pp->primCol, pp->primColTarget, pp->primColRemain, pp->primColCount);
                        freeze(pp->envCol, pp->envColTarget, pp->envColRemain, pp->envColCount);
                        val = (float)((int8_t)*c++ * 2) * randf();
                        pp->primColTarget.r = clamp_add(pp->primColTarget.r, val); pp->envColTarget.r = clamp_add(pp->envColTarget.r, val);
                        val = (float)((int8_t)*c++ * 2) * randf();
                        pp->primColTarget.g = clamp_add(pp->primColTarget.g, val); pp->envColTarget.g = clamp_add(pp->envColTarget.g, val);
                        val = (float)((int8_t)*c++ * 2) * randf();
                        pp->primColTarget.b = clamp_add(pp->primColTarget.b, val); pp->envColTarget.b = clamp_add(pp->envColTarget.b, val);
                        val = (float)((int8_t)*c++ * 2) * randf();
                        pp->primColTarget.a = clamp_add(pp->primColTarget.a, val); pp->envColTarget.a = clamp_add(pp->envColTarget.a, val);
                        if (pp->primColCount == 0) pp->primCol = pp->primColTarget;
                        pp->primColRemain = pp->primColCount;
                        if (pp->envColCount == 0) pp->envCol = pp->envColTarget;
                        pp->envColRemain = pp->envColCount;
                        break;
                    case 0xE9: {
                        freeze(pp->primCol, pp->primColTarget, pp->primColRemain, pp->primColCount);
                        freeze(pp->envCol, pp->envColTarget, pp->envColRemain, pp->envColCount);
                        const int steps = c[1];
                        const uint8_t flags = c[0];
                        c += 2;
                        if (steps != 0) val = (float)(int32_t)((steps + 1) * randf()) / (float)steps;
                        else val = randf();
                        auto chan = [&](uint8_t bit, uint8_t Col::*m) {
                            if (!(flags & bit)) return;
                            val2 = val * (float)((int8_t)*c++ * 2);
                            if (flags & 0x10) pp->primColTarget.*m = clamp_add(pp->primColTarget.*m, val2);
                            if (flags & 0x20) pp->envColTarget.*m = clamp_add(pp->envColTarget.*m, val2);
                        };
                        chan(1, &Col::r);
                        chan(2, &Col::g);
                        chan(4, &Col::b);
                        if (flags & 8) {
                            if (steps != 0) val = (float)((int8_t)*c++ * 2) * (float)(int32_t)((steps + 1) * randf()) / (float)steps;
                            else val = (float)((int8_t)*c++ * 2) * randf();
                            if (flags & 0x10) pp->primColTarget.a = clamp_add(pp->primColTarget.a, val);
                            if (flags & 0x20) pp->envColTarget.a = clamp_add(pp->envColTarget.a, val);
                        }
                        if (pp->primColCount == 0) pp->primCol = pp->primColTarget;
                        pp->primColRemain = pp->primColCount;
                        if (pp->envColCount == 0) pp->envCol = pp->envColTarget;
                        pp->envColRemain = pp->envColCount;
                        break;
                    }
                    case 0xE2: pp->kind |= 8; break;
                    case 0xE3: pp->palNum = *c++; break;
                    case 0xE4: case 0xE5: {
                        const uint32_t bit = op == 0xE4 ? 0x40000u : 0x80000u;
                        switch (*c++ & 3) {
                            case 0: pp->kind &= ~bit; break;
                            case 1: pp->kind |= bit; break;
                            case 2: pp->kind ^= bit; break;
                            case 3:
                                if (randf() < 0.5f) pp->kind &= ~bit;
                                else pp->kind |= bit;
                                break;
                        }
                        break;
                    }
                    case 0xE6: pp->kind |= 0x200000; break;
                    case 0xE7: pp->kind &= ~0x200000u; break;
                    case 0xE8:
                        c = get_float(c, val);
                        if (val < 0.0f) pp->kind &= ~0x100000u;
                        else { pp->kind |= 0x100000; pp->trail = val; }
                        break;
                    case 0xEA: case 0xEB: {
                        uint8_t& rgb = op == 0xEA ? pp->matRGB : pp->ambRGB;
                        uint8_t& a = op == 0xEA ? pp->matA : pp->ambA;
                        uint8_t& rgbT = op == 0xEA ? pp->matRGBTarget : pp->ambRGBTarget;
                        uint8_t& aT = op == 0xEA ? pp->matATarget : pp->ambATarget;
                        uint16_t& cnt = op == 0xEA ? pp->matColCount : pp->ambColCount;
                        uint16_t& rem = op == 0xEA ? pp->matColRemain : pp->ambColRemain;
                        if (cnt != 0) {
                            const int32_t t = (int32_t)(rem << 16) / cnt;
                            rgb = (uint8_t)(((rgbT << 16) + t * (rgb - rgbT)) >> 16);
                            a = (uint8_t)(((aT << 16) + t * (a - aT)) >> 16);
                        }
                        c = get_time(c, cnt);
                        const uint8_t flags = *c++;
                        rgbT = rgb;
                        if (flags & 1) rgbT = *c++;
                        if (flags & 8) aT = *c++;
                        if (cnt == 0) { rgb = rgbT; rem = 0; }
                        else rem = cnt;
                        break;
                    }
                    case 0xED: {
                        c = get_float(c, val);
                        c = get_float(c, val2);
                        const int steps = *c++;
                        if (steps != 0) val = val + val2 * (float)(int32_t)((steps + 1) * randf()) / (float)steps;
                        else val = val + val2 * randf();
                        pp->rotateTarget += val;
                        pp->rotate += val;
                        break;
                    }
                    case 0xF3: {
                        const int dir = *c++;
                        c = get_float(c, pp->rotateTarget);
                        c = get_float(c, pp->x68);
                        c = get_time(c, pp->rotateCount);
                        if (pp->rotateCount != 0) {
                            if (dir == 0) pp->rotateTarget = (float)(pp->rotateTarget + pp->x68 / 2.0);
                            else {
                                pp->rotateTarget *= -1.0f;
                                pp->rotateTarget = (float)(pp->rotateTarget - pp->x68 / 2.0);
                            }
                        } else {
                            pp->rotateTarget = 0.0f;
                            pp->x68 = 0.0f;
                        }
                        break;
                    }
                    case 0xFA:
                        pp->loopCount = *c++;
                        pp->cmdLoopPtr = (uint16_t)(c - pp->cmdList);
                        break;
                    case 0xFB:
                        if (--pp->loopCount != 0) c = pp->cmdList + pp->cmdLoopPtr;
                        break;
                    case 0xFC: pp->cmdMarkPtr = (uint16_t)(c - pp->cmdList); break;
                    case 0xFD: c = pp->cmdList + pp->cmdMarkPtr; break;
                    case 0xFE: case 0xFF:
                        pp->life = 1;
                        goto exit;
                    default:
                        break;
                }
            } while (time == 0);
        exit:
            pp->cmdPtr = (uint16_t)(c - pp->cmdList);
            pp->cmdWait = time;
        }
        if (--pp->life == 0) {
            if (pp->gen) pp->gen->numChild--;
            Part* next = pp->next;
            delete_point(pp);
            list_delete(pp, prev);
            return next;
        }
        if (pp->kind & 4) {  // the tornado's motion
            const Gen* gen = pp->gen;
            const float sa = std::sin(pp->grav), sb = std::sin(pp->fric), ca = std::cos(pp->grav), cb = std::cos(pp->fric);
            pp->vel.z += gen->line.x2;
            float r = std::fabs(gen->radius);
            r += pp->vel.z * std::tan(std::fabs(gen->angle));
            r *= pp->vel.y;
            pp->vel.x += gen->grav;
            const float x = r * std::cos(pp->vel.x), y = r * std::sin(pp->vel.x), z = pp->vel.z;
            pp->pos.x = gen->pos.x + (x * cb + z * sb);
            pp->pos.y = gen->pos.y + (sb * (-x * sa) + y * ca + cb * (z * sa));
            pp->pos.z = gen->pos.z + (sb * (-x * ca) - y * sa + cb * (z * ca));
            // (ORRE_DEBUG_PS_TORNADO=1: each tornado particle's height above its generator and its climb, every step;
            // a debug switch for comparing with the port's ORRE_WAZA_SWEEP_PARTICLES log, Grudge 2026-11-21)
            static const bool dbg = std::getenv("ORRE_DEBUG_PS_TORNADO") != nullptr;
            if (dbg)
                std::fprintf(stderr, "[ps tornado] part %p life %d: y %.3f above its generator %.3f, climb %.4f, gen scale %.3f, flags 0x%X\n",
                             (void*)pp, pp->life, pp->pos.y, pp->pos.y - gen->pos.y, gen->line.x2, gen->scale.y, gen->posFlags);
        } else {
            if (pp->kind & 1) pp->vel.y -= pp->grav;
            if (pp->kind & 2) { pp->vel.x *= pp->fric; pp->vel.y *= pp->fric; pp->vel.z *= pp->fric; }
            pp->pos.x += pp->vel.x;
            pp->pos.y += pp->vel.y;
            pp->pos.z += pp->vel.z;
        }
        if (pp->kind & 0x8000) {  // the particle drives a point joint
            const uint32_t no = (pp->kind >> 12) & 7;
            point_set[no] = true;
            point[no] = pp->pos;
        }
        return pp->next;
    }

    void interpret_all() {  // psInterpretParticles(0)
        for (int i = 0; i < 16; ++i) {
            Part* lastPP = nullptr;
            for (Part* pp = active[i]; pp;) {
                Part* next = pp->next;
                Part* ret = interpret_one(pp, lastPP);
                if (next) {
                    if (pp->next == next) lastPP = pp;
                    else if (ret != next) {
                        lastPP = ret;
                        while (lastPP && lastPP->next != next) lastPP = lastPP->next;
                    }
                }
                pp = next;
            }
        }
    }

    // generateParticle_8017424C
    float spawn(Gen* gen) {
        float cur_angle = 0, angle_step = 0, dist = 0, radius = 0, cone_angle = 0, angle1 = 0, angle3 = 0;
        if (gen->count < 1.0f) return gen->count;
        V3 vel_copy = gen->vel;
        const float scale_x = gen->scale.x, scale_y = gen->scale.y, scale_z = gen->scale.z;
        if ((gen->type & 0xF) == 2 && (gen->posFlags & 0x80)) { vel_copy.x *= scale_x; vel_copy.y *= scale_y; vel_copy.z *= scale_z; }
        const float vel_mag = std::sqrt(vel_copy.z * vel_copy.z + (vel_copy.x * vel_copy.x + vel_copy.y * vel_copy.y));
        Mtx rot_mtx;
        mtx_identity(rot_mtx);
        if (!(gen->kind & 0x30000)) {
            Mtx mx, my, mz;
            mtx_rot(mx, 'X', gen->rot.x);
            mtx_rot(my, 'Y', gen->rot.y);
            mtx_rot(mz, 'Z', gen->rot.z);
            mtx_concat(my, mx, mx);
            mtx_concat(mz, mx, mx);
            for (int col = 0; col < 3; ++col) {
                const V3 v = normalize({mx[0][col], mx[1][col], mx[2][col]});
                rot_mtx[0][col] = v.x;
                rot_mtx[1][col] = v.y;
                rot_mtx[2][col] = v.z;
            }
            rot_mtx[0][3] = rot_mtx[1][3] = rot_mtx[2][3] = 0.0f;
        }
        // (kind 0x10000: the camera's basis; none of the Pokémon's own effects use it)
        if ((gen->type & 0xF) != 1 && vel_mag > kFltEpsilon) {
            const V3 vn = normalize(gen->vel);
            const float yaw = std::fabs(vn.z) < kFltMin ? (vn.y >= 0.0f ? 1.5707964f : -1.5707964f) : std::atan2(vn.y, vn.z);
            const float sin_yaw = std::sin(yaw), cos_yaw = std::cos(yaw);
            const float pz = vn.y * sin_yaw + vn.z * cos_yaw;
            const float pitch = std::fabs(pz) < kFltMin ? (vn.x >= 0.0f ? 1.5707964f : -1.5707964f) : std::atan2(vn.x, pz);
            const float sp = std::sin(pitch), cp = std::cos(pitch);
            Mtx t = {{cp, 0.0f, sp, 0.0f}, {-sin_yaw * sp, cos_yaw, sin_yaw * cp, 0.0f}, {-cos_yaw * sp, -sin_yaw, cos_yaw * cp, 0.0f}};
            mtx_concat(rot_mtx, t, rot_mtx);
        }
        const float elevation = vel_mag;
        if ((gen->type & 0xF) == 2) {
            angle1 = std::fabs(rot_mtx[2][2]) < kFltMin ? (rot_mtx[1][2] >= 0.0f ? 1.5707964f : -1.5707964f)
                                                         : std::atan2(rot_mtx[1][2], rot_mtx[2][2]);
            const float comb = rot_mtx[1][2] * std::sin(angle1) + rot_mtx[2][2] * std::cos(angle1);
            angle3 = std::fabs(comb) < kFltMin ? (rot_mtx[0][2] >= 0.0f ? 1.5707964f : -1.5707964f) : std::atan2(rot_mtx[0][2], comb);
        }
        if (gen->angle < 0.0f) {
            switch (gen->type & 0xF) {
                case 0: case 3: case 4: case 6: case 7: {
                    const float min_a = gen->cone.minAngle;
                    const float rnd = randf();
                    const float step = (gen->cone.maxAngle - min_a) / (float)(int32_t)gen->count;
                    angle_step = step;
                    cur_angle = std::fma(step, rnd, min_a);  // (fmadds on the console: one rounding)
                    break;
                }
                default:
                    cur_angle = (float)(2.0 * (kPi * randf()));
                    angle_step = (float)((2.0 * kPi) / (int32_t)gen->count);
                    break;
            }
        }
        while (gen->count >= 1.0f) {
            const int shape = gen->type & 0xF;
            switch (shape) {
                case 0: case 3: case 4: case 6: case 7: {
                    if (gen->radius < 0.0f) {
                        dist = -gen->radius;
                        radius = 1.0f;
                    } else {
                        radius = randf();
                        if (shape == 3 || shape == 4) radius = std::sqrt(radius);
                        dist = radius * gen->radius;
                    }
                    if (shape == 6) {
                        if (gen->angle < 0.0f) {
                            cur_angle += angle_step;
                            if (std::fabs(dist) < kFltMin) cone_angle = gen->cone.height >= 0.0f ? -gen->angle : (float)(kPi - gen->angle);
                            else cone_angle = (float)(kPi / 2 - std::atan2(gen->cone.height, dist) - gen->angle);
                        } else {
                            const float min_angle = gen->cone.minAngle;
                            cur_angle = (gen->cone.maxAngle - min_angle) * randf() + min_angle;
                            if (std::fabs(dist) < kFltMin) cone_angle = gen->cone.height >= 0.0f ? gen->angle : (float)(kPi + gen->angle);
                            else cone_angle = (float)(gen->angle + (kPi / 2 - std::atan2(gen->cone.height, dist)));
                        }
                    } else if (shape == 7) {
                        if (gen->angle < 0.0f) {
                            cone_angle = (float)(kPi / 2 - gen->angle);
                            cur_angle += angle_step;
                        } else {
                            cur_angle = gen->cone.minAngle;
                            cur_angle = (gen->cone.maxAngle - cur_angle) * randf() + cur_angle;
                            cone_angle = (float)(kPi / 2 + gen->angle);
                        }
                    } else {
                        if (gen->angle < 0.0f) {
                            cone_angle = -gen->angle;
                            cur_angle += angle_step;
                            cone_angle = radius * cone_angle;
                        } else {
                            cur_angle = gen->cone.minAngle;
                            cur_angle = (gen->cone.maxAngle - cur_angle) * randf() + cur_angle;
                            cone_angle = radius * gen->angle;
                        }
                    }
                    V3 emit{dist * std::cos(cur_angle), dist * std::sin(cur_angle), 0.0f};
                    if (shape == 6 || shape == 7) {
                        emit.z = randf();
                        if (shape == 6) { emit.x *= 1.0f - emit.z; emit.y *= 1.0f - emit.z; }
                        emit.z *= gen->cone.height;
                    }
                    const float sin_ca = elevation * std::sin(cone_angle);
                    V3 v{sin_ca * std::cos(cur_angle), sin_ca * std::sin(cur_angle), elevation * std::cos(cone_angle)};
                    if (shape == 3) { v.x *= radius; v.y *= radius; v.z *= radius; }
                    emit = mtx_mul(rot_mtx, emit);
                    emit.x += gen->pos.x; emit.y += gen->pos.y; emit.z += gen->pos.z;
                    v = mtx_mul(rot_mtx, v);
                    generate(nullptr, gen->linkNo, gen->bank, gen->kind, gen->texGroup, gen->cmdList, gen->life, emit.x, emit.y, emit.z,
                             v.x * scale_x, v.y * scale_y, v.z * scale_z, gen->size, gen->grav, gen->fric, 0, gen, true);
                    break;
                }
                case 1: {
                    const float rnd = randf();
                    V3 emit{rnd * gen->line.x2, rnd * gen->line.y2, rnd * gen->line.z2};
                    emit = mtx_mul(rot_mtx, emit);
                    emit.x += gen->pos.x; emit.y += gen->pos.y; emit.z += gen->pos.z;
                    const V3 v = mtx_mul(rot_mtx, vel_copy);
                    generate(nullptr, gen->linkNo, gen->bank, gen->kind, gen->texGroup, gen->cmdList, gen->life, emit.x, emit.y, emit.z,
                             v.x * scale_x, v.y * scale_y, v.z * scale_z, gen->size, gen->grav, gen->fric, 0, gen, true);
                    break;
                }
                case 2:
                    dist = gen->radius < 0.0f ? 1.0f : randf();
                    if (gen->angle < 0.0f) cur_angle += angle_step;
                    else cur_angle = (float)(2.0 * (kPi * randf()));
                    gen->line.x2 = elevation;
                    generate(nullptr, gen->linkNo, gen->bank, gen->kind | 4, gen->texGroup, gen->cmdList, gen->life, 0, 0, 0, cur_angle, dist,
                             0.0f, gen->size, angle1, angle3, 0, gen, true);
                    break;
                case 5: {
                    V3 e;
                    e.x = randf();
                    e.y = randf();
                    e.z = randf();
                    auto snap = [](float v) { return v > 0.5f ? 1.0f : 0.0f; };
                    switch (gen->rect.flag) {
                        case 1: e.x = snap(e.x); break;
                        case 2: e.y = snap(e.y); break;
                        case 3: {
                            const float rnd = randf(), a2 = gen->rect.x;
                            if (rnd > a2 / (a2 + gen->rect.y)) e.y = snap(e.y); else e.x = snap(e.x);
                            break;
                        }
                        case 4: e.z = snap(e.z); break;
                        case 5: {
                            const float rnd = randf(), a2 = gen->rect.x;
                            if (rnd > a2 / (a2 + gen->rect.z)) e.z = snap(e.z); else e.x = snap(e.x);
                            break;
                        }
                        case 6: {
                            const float rnd = randf(), a2 = gen->rect.y;
                            if (rnd > a2 / (a2 + gen->rect.z)) e.z = snap(e.z); else e.y = snap(e.y);
                            break;
                        }
                        case 7: {
                            const float rnd = randf(), a2 = gen->rect.z, b2 = gen->rect.y, c2 = gen->rect.x;
                            const float r0 = 1.0f / (c2 * (b2 + a2) + b2 * a2);
                            if (rnd < r0 * (c2 * b2)) e.z = snap(e.z);
                            else if (rnd > 1.0f - r0 * (c2 * a2)) e.y = snap(e.y);
                            else e.x = snap(e.x);
                            break;
                        }
                        default: break;
                    }
                    e.x -= 0.5f; e.y -= 0.5f; e.z -= 0.5f;
                    const auto& R = gen->rect;
                    const V3 t{R.xx * e.x + R.yx * e.y + R.zx * e.z, R.xy * e.x + R.yy * e.y + R.zy * e.z, R.xz * e.x + R.yz * e.y + R.zz * e.z};
                    V3 emit = mtx_mul(rot_mtx, t);
                    emit.x += gen->pos.x; emit.y += gen->pos.y; emit.z += gen->pos.z;
                    const float mag = std::sqrt(R.zx * R.zx + R.zy * R.zy + R.zz * R.zz);
                    const float sc = elevation / mag;
                    const V3 v = mtx_mul(rot_mtx, V3{R.zx * sc, R.zy * sc, R.zz * sc});
                    generate(nullptr, gen->linkNo, gen->bank, gen->kind, gen->texGroup, gen->cmdList, gen->life, emit.x, emit.y, emit.z,
                             v.x * scale_x, v.y * scale_y, v.z * scale_z, gen->size, gen->grav, gen->fric, 0, gen, true);
                    break;
                }
                case 8: {
                    const float lat = 0.0f == gen->sphere.latRange ? (float)(kPi * randf()) : gen->sphere.latRange * randf();
                    const float lon = (float)(2.0 * (kPi * randf()));
                    const float r2 = gen->radius;
                    dist = r2 < 0.0f ? -r2 : r2 * randf();
                    const V3 dir = mtx_mul(rot_mtx, V3{std::sin(lat) * std::cos(lon), std::sin(lat) * std::sin(lon), std::cos(lat)});
                    V3 v{dir.x * gen->sphere.speed, dir.y * gen->sphere.speed, dir.z * gen->sphere.speed};
                    if (gen->radius >= 0.0f && gen->sphere.speed < 0.0f) {
                        const float sc = dist / gen->radius;
                        v.x *= sc; v.y *= sc; v.z *= sc;
                    }
                    const V3 emit{dist * dir.x + gen->pos.x, dist * dir.y + gen->pos.y, dist * dir.z + gen->pos.z};
                    generate(nullptr, gen->linkNo, gen->bank, gen->kind, gen->texGroup, gen->cmdList, gen->life, emit.x, emit.y, emit.z,
                             v.x * scale_x, v.y * scale_y, v.z * scale_z, gen->size, gen->grav, gen->fric, 0, gen, true);
                    break;
                }
                default:
                    break;  // psExtExecGenFunc: none in this game
            }
            gen->count -= 1.0f;
        }
        return gen->count;
    }

    void exec_generators() {  // psExecGenerator(0)
        Gen* gen = activeGen;
        lastGP = nullptr;
        while (gen) {
            if (gen->kind & 0x800) {
                lastGP = gen;
                gen = gen->next;
                continue;
            }
            // genPosUpdate (generator.c): a generator a particle started (posFlags 2 cleared) on a linked joint takes the
            // joint's position; a node's own generator keeps what the follow step set
            if (!(gen->posFlags & 2) && (gen->posFlags & 1) && gen->jobj) {
                auto it = jobj_pos.find(gen->jobj);
                if (it != jobj_pos.end()) gen->pos = it->second;
            }
            if (gen->random < 0.0f) gen->count -= gen->random;
            else gen->count = std::fma(gen->random, randf(), gen->count);  // one fused multiply-add, as the console's fmadds
            if (gen->count >= 1.0f) gen->count = spawn(gen);
            if (gen->genLife != 0 && --gen->genLife == 0) {
                if (gen->x17 == 0) lastGP = delete_gen(gen, lastGP);
                else gen->kind |= 0x800;
                gen = lastGP ? lastGP->next : activeGen;
                continue;
            }
            lastGP = gen;
            gen = gen->next;
        }
    }

    // gsParticleUpdateAll: a node whose generator had a life is reclaimed once it and its particles are all done
    void reclaim() {
        for (size_t i = 0; i < nodes.size();) {
            Node& n = nodes[i];
            if (!n.infinite && child_max_life(n.gen) == 0 && child_count(n.gen) == 0) {
                kill_family(n.gen->idnum, n.gen->linkNo);
                kill_gen(n.gen);  // (already gone with its family: a no-op that leaves lastGP at the list's end, as the game's)
                nodes.erase(nodes.begin() + (std::ptrdiff_t)i);
                continue;
            }
            ++i;
        }
    }

    void kill_all() {
        for (int i = 0; i < 16; ++i)
            while (active[i]) list_delete(active[i], nullptr);
        while (activeGen) {
            Gen* g = activeGen;
            activeGen = g->next;
            g->app.reset();
            g->next = freeGen;
            freeGen = g;
        }
        live = live_gens = 0;
        lastGP = nullptr;
        nodes.clear();
        jobj_pos.clear();
        for (bool& p : point_set) p = false;
    }
};

extern "C" OrreParticleSystem* orre_particles_create(void) { return new OrreParticleSystem(); }
extern "C" void orre_particles_free(OrreParticleSystem* s) { delete s; }

extern "C" void orre_particles_reset(OrreParticleSystem* s, uint32_t seed) {
    if (!s) return;
    s->kill_all();
    s->rng = seed;
    s->idNum = 0;
}

extern "C" void orre_particles_set_bank(OrreParticleSystem* s, int slot, const OrreParticleBank* b) {
    if (!s || slot < 0 || slot >= 64) return;
    BankSlot& bs = s->banks[slot];
    bs = BankSlot{};
    if (!b) return;
    bs.bank = b;
    bs.first = (int)b->first_id;
    bs.by_id.resize((size_t)b->first_id + b->count);
    for (uint32_t i = 0; i < b->count; ++i) {
        OrreParticleEmitter e{};
        if (!orre_particle_emitter(b, (int)i, &e) || !e.present) continue;
        Emitter& m = bs.by_id[(size_t)b->first_id + i];
        m.present = true;
        m.type = e.type;
        m.texGroup = e.tex_group;
        m.genLife = e.gen_life;
        m.life = e.life;
        m.kind = (e.kind & 0xF1FFFFFFu) | 0x08000000u;  // psInitDataBankLocate: the sort group forced to 4
        m.grav = e.gravity;
        m.fric = e.friction;
        m.vx = e.vel[0];
        m.vy = e.vel[1];
        m.vz = e.vel[2];
        m.radius = e.radius;
        m.angle = e.angle;
        m.random = e.random;
        m.size = e.size;
        m.p1 = e.param[0];
        m.p2 = e.param[1];
        m.p3 = e.param[2];
        m.cmd = e.code_size > 0 ? orre_particle_emitter_code(b, (int)i) : nullptr;
    }
}

extern "C" int orre_particles_start(OrreParticleSystem* s, int slot, int emitter_id) {
    return orre_particles_start_link(s, slot, emitter_id, 0);
}

extern "C" int orre_particles_start_link(OrreParticleSystem* s, int slot, int emitter_id, int link) {
    if (!s || s->nodes.size() >= 128) return 0;  // gsParticleCreate: 128 nodes
    Gen* gen = s->create_gen(link == 1 ? 1 : 0, slot, emitter_id);
    if (!gen) return 0;
    gen->x17 = 1;  // keep it (frozen) after its life, until its particles are gone
    Node n;
    n.gen = gen;
    n.infinite = gen->genLife == 0;
    n.handle = s->next_handle++;
    n.position = gen->pos;
    s->nodes.push_back(n);
    return n.handle;
}

// ---- the move effects' node calls (release step 4; field_range_80117E58.c and particle.c, verified from the code) ----
namespace {
Node* find_node(OrreParticleSystem* s, int handle) {
    if (!s) return nullptr;
    for (Node& n : s->nodes)
        if (n.handle == handle) return &n;
    return nullptr;
}
}  // namespace

extern "C" void orre_particles_node_flags(OrreParticleSystem* s, int handle, uint32_t mode) {
    Node* n = find_node(s, handle);
    if (!n) return;
    uint16_t& f = n->gen->posFlags;
    // _wazaSequenceParticleEntryStart: bit 2 (with 4) psSetOffsetRotationInLocal, 3 psSetVelocityRotationInLocal,
    // 7 psSetParticleTexScaling, 11 psSetNodeScaling, 12 psSetRandomVelocityScaling, then psSetTornadoScaling(bit 8,
    // bit 10) always (each setter sets its bit when on; the tornado one also clears)
    if (mode & 0x4) {
        f |= 8;
        if ((mode >> 4) & 1) f |= 0x10; else f &= ~0x10;
    }
    if (mode & 0x8) f |= 4;
    if (mode & 0x80) f |= 0x20;
    if (mode & 0x800) f |= 0x100;
    if (mode & 0x1000) f |= 0x200;
    if ((mode >> 8) & 1) f |= 0x40; else f &= ~0x40;
    if ((mode >> 10) & 1) f |= 0x80; else f &= ~0x80;
}

extern "C" void orre_particles_node_set(OrreParticleSystem* s, int handle, int what, const float v[3], int apply, int motion) {
    Node* n = find_node(s, handle);
    if (!n || !v) return;
    const V3 x{v[0], v[1], v[2]};
    if (what == 0) {  // gsParticleSetVecB
        if (n->state == 0) n->local_pos = n->gen->pos = x;
        else n->tr_pos = x;
    } else if (what == 1) {  // fn_80118E8C
        if (n->state == 0) n->local_rot = n->gen->rot = x;
        else n->tr_rot = x;
    } else {  // fn_80118DE0
        if (n->state == 0) n->local_scale = n->gen->scale = x;
        else n->tr_scale = x;
        if (apply == 1) s->angle_radius_scale(n->gen, v, motion != 0);
    }
}

extern "C" void orre_particles_node_position(OrreParticleSystem* s, int handle, float out[3]) {
    Node* n = find_node(s, handle);
    if (!n || !out) return;
    out[0] = n->position.x;
    out[1] = n->position.y;
    out[2] = n->position.z;
}

extern "C" void orre_particles_node_link(OrreParticleSystem* s, int handle, int state, int once, int from_zero, int link_jobj) {
    Node* n = find_node(s, handle);
    if (!n || n->state != 0 || state == 0) return;  // gsParticleLinkToJObj
    if (from_zero) {
        n->tr_pos = {0, 0, 0};
        n->tr_rot = {0, 0, 0};
        n->tr_scale = {1, 1, 1};
    } else {
        n->tr_pos = n->local_pos;
        n->tr_rot = n->local_rot;
        n->tr_scale = n->local_scale;
    }
    n->local_pos = {0, 0, 0};
    n->local_rot = {0, 0, 0};
    n->local_scale = {1, 1, 1};
    if (link_jobj) {  // psLinkChildGensToJObj
        n->gen->posFlags |= 1;
        n->gen->jobj = handle;
    }
    n->state = state;
    n->once = once != 0;
    n->pending = true;
}

extern "C" int orre_particles_node_due(const OrreParticleSystem* s, int handle) {
    if (!s) return 0;
    for (const Node& n : s->nodes)
        if (n.handle == handle) return n.state != 0 && n.pending;
    return 0;
}

// gsParticleUpdate's per-node part: the joint's world position, its rotation (the sum of the chain's Euler angles, as
// GSpartGetTransform adds them) and its scale (the chain's product); the offset turned by the joint's rotation
// (GSquatFromEuler, then GSvecTransformQuat) and added; then the state picks what the generator takes
extern "C" void orre_particles_node_follow(OrreParticleSystem* s, int handle, const float p[3], const float r[3], const float sc[3]) {
    Node* n = find_node(s, handle);
    if (!n || n->state == 0 || !n->pending || !p || !r || !sc) return;
    s->jobj_pos[handle] = {p[0], p[1], p[2]};
    // the quaternion from Euler angles, applied as R = Rz Ry Rx (HSD's convention: X first)
    Mtx rx, ry, rz, m;
    mtx_rot(rx, 'X', r[0]);
    mtx_rot(ry, 'Y', r[1]);
    mtx_rot(rz, 'Z', r[2]);
    mtx_concat(ry, rx, m);
    mtx_concat(rz, m, m);
    const V3 off = mtx_mul(m, n->tr_pos);
    const V3 pos{p[0] + off.x, p[1] + off.y, p[2] + off.z};
    const V3 rot{r[0] + n->tr_rot.x, r[1] + n->tr_rot.y, r[2] + n->tr_rot.z};
    const V3 scl{sc[0] * n->tr_scale.x, sc[1] * n->tr_scale.y, sc[2] * n->tr_scale.z};
    // (the game first writes the offsets into the local values and the generator, then the state's choice over them)
    n->local_pos = n->gen->pos = n->tr_pos;
    n->local_rot = n->gen->rot = n->tr_rot;
    n->local_scale = n->gen->scale = n->tr_scale;
    const bool fp = n->state == 1 || n->state == 4 || n->state == 6 || n->state == 7;
    const bool fr = n->state == 2 || n->state == 4 || n->state == 5 || n->state == 7;
    const bool fs = n->state == 3 || n->state == 5 || n->state == 6 || n->state == 7;
    if (fp) n->local_pos = n->gen->pos = pos;
    if (fr) n->local_rot = n->gen->rot = rot;
    if (fs) n->local_scale = n->gen->scale = scl;
    n->pending = !n->once;
}

// a node is done when its generator's family has no life left and no particles (wazaSequenceEntryUpdate, kind 3)
extern "C" int orre_particles_node_alive(OrreParticleSystem* s, int handle) {
    Node* n = find_node(s, handle);
    if (!n) return 0;
    return n->infinite || s->child_max_life(n->gen) != 0 || s->child_count(n->gen) != 0;
}

extern "C" void orre_particles_set_position(OrreParticleSystem* s, int handle, const float p[3]) {
    if (!s || !p) return;
    for (Node& n : s->nodes)
        if (n.handle == handle) n.gen->pos = {p[0], p[1], p[2]};
}

extern "C" void orre_particles_set_point(OrreParticleSystem* s, int no, const float* p) {
    if (!s || no < 1 || no > 8) return;
    s->point_set[no - 1] = p != nullptr;
    if (p) s->point[no - 1] = {p[0], p[1], p[2]};
}

extern "C" void orre_particles_step(OrreParticleSystem* s) {
    if (!s) return;
    s->interpret_all();
    s->exec_generators();
}

extern "C" void orre_particles_reclaim(OrreParticleSystem* s) {
    if (s) s->reclaim();
}

extern "C" int orre_particles_count(const OrreParticleSystem* s) { return s ? s->live : 0; }
extern "C" int orre_particles_generators(const OrreParticleSystem* s) { return s ? s->live_gens : 0; }

// ---- drawing: what psDispMain / psDispSub / psDispSubMakePolygon give the GPU for each particle (src/game/psdisp.c) ----
namespace {
// the four corners' texture coordinates (lbl_8036BFC0): none, flip S, flip T, both; corners in drawing order
const uint8_t kQuadUV[32] = {0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0};
// getColorPrimEnv (psdisp_color.h): a ramp's value drawn now, in 16.16 fixed point
Col ramp(const Col& start, const Col& target, uint16_t remain, uint16_t count) {
    if (!count) return start;
    const int scale = 65536 * remain / count;
    return {(uint8_t)(((target.r << 16) + (start.r - target.r) * scale) >> 16), (uint8_t)(((target.g << 16) + (start.g - target.g) * scale) >> 16),
            (uint8_t)(((target.b << 16) + (start.b - target.b) * scale) >> 16), (uint8_t)(((target.a << 16) + (start.a - target.a) * scale) >> 16)};
}
uint8_t ramp1(uint8_t start, uint8_t target, uint16_t remain, uint16_t count) {
    if (!count) return start;
    const int scale = 65536 * remain / count;
    return (uint8_t)(((target << 16) + (start - target) * scale) >> 16);
}
}  // namespace

extern "C" int orre_particles_quads(const OrreParticleSystem* s, const float view[12], const float proj[4], OrreParticleQuad* out,
                                    int max) {
    if (!s || !view) return 0;
    // vmtx and its inverse rvmtx (a camera's view is a rotation and a translation: the inverse is the transpose)
    float vm[3][4], rv[3][4];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c) vm[r][c] = view[4 * r + c];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) rv[r][c] = vm[c][r];
        rv[r][3] = -(rv[r][0] * vm[0][3] + rv[r][1] * vm[1][3] + rv[r][2] * vm[2][3]);
    }
    // the camera's rotation for the camera-facing application SRTs (psDispMain: HSD_MtxGetRotationMtx(rvmtx, 'z', 'x'):
    // z from the inverse view's third column, x from its first, y = z x x, then x = y x z; columns of rc)
    V3 rz = normalize({rv[0][2], rv[1][2], rv[2][2]}), rx = normalize({rv[0][0], rv[1][0], rv[2][0]});
    const V3 ry{rz.y * rx.z - rz.z * rx.y, rz.z * rx.x - rz.x * rx.z, rz.x * rx.y - rz.y * rx.x};
    rx = {ry.y * rz.z - ry.z * rz.y, ry.z * rz.x - ry.x * rz.z, ry.x * rz.y - ry.y * rz.x};
    auto cam_rot = [&](const V3& v) {
        return V3{rx.x * v.x + ry.x * v.y + rz.x * v.z, rx.y * v.x + ry.y * v.y + rz.y * v.z, rx.z * v.x + ry.z * v.y + rz.z * v.z};
    };
    // the billboard axes (psDispMain: lbl_8047B160..14C): A = camera right + up, B = right - up
    const V3 A0{rv[0][0] + rv[0][1], rv[1][0] + rv[1][1], rv[2][0] + rv[2][1]};
    const V3 B0{rv[0][0] - rv[0][1], rv[1][0] - rv[1][1], rv[2][0] - rv[2][1]};
    // pvmtx: the projection's first two rows times the view (a perspective camera's, prj[0] == 0)
    float pv[2][4] = {};
    const float pxx = proj ? proj[0] : 1, pxz = proj ? proj[1] : 0, pyy = proj ? proj[2] : 1, pyz = proj ? proj[3] : 0;
    for (int c = 0; c < 4; ++c) {
        pv[0][c] = pxx * vm[0][c] + pxz * vm[2][c];
        pv[1][c] = pyy * vm[1][c] + pyz * vm[2][c];
    }
    int n = 0;
    // particleSort's order: the edge-sorted particles (kind 8) first, then the rest; within each, sort groups 0-7 (kind bits
    // 25-27), each in its list's order (newest first). Links in order.
    for (int pass = 0; pass < 2; ++pass)
        for (int link = 0; link < 16; ++link)
            for (int group = 0; group < 8; ++group)
                for (const Part* pp = s->active[link]; pp; pp = pp->next) {
                    const bool edge = (pp->kind & 8) != 0;
                    if (edge != (pass == 0) || (int)((pp->kind >> 25) & 7) != group) continue;
                    if (pp->size < kFltEpsilon || (pp->kind & 0x20000000)) continue;
                    // DispPoint (kind bit 30; psDispMain, src/game/psdisp.c, verified from the code): a GX point, a square
                    // on screen GXSetPointSize(6 x size) sixths of a pixel wide (255 at most: size 42.5), the texture
                    // stretched across it (GX_TO_ONE). Drawn here as a camera-facing square of that many pixels of the
                    // game's 480-line picture at the particle's depth. Helping Hand's gold dust is points. With Trail
                    // (psDispSubPointTrail): a GX line of the same width from pos - vel (its alpha x the trail factor)
                    // to pos, drawn as a screen-facing strip. Not drawn yet: points with an application frame
                    // (psDispSubAPPSRTPoint) and tornado trails (calcTornadoLastPos)
                    const bool point = (pp->kind & 0x40000000) != 0;
                    float point_half = 0, point_px = 0;
                    auto depth_of = [&](const V3& p) { return -(vm[2][3] + vm[2][0] * p.x + vm[2][1] * p.y + vm[2][2] * p.z); };
                    const float pyy_safe = pyy != 0.0f ? pyy : 1.0f;
                    if (point) {
                        if (pp->app || ((pp->kind & 0x100000) && (pp->kind & 0x4))) continue;
                        point_px = std::floor(pp->size > 42.5f ? 255.0f : 6.0f * pp->size) / 6.0f;  // (u8 w: whole sixths)
                        const float depth = depth_of(pp->pos);
                        if (depth <= 0.0f || point_px <= 0.0f) continue;
                        point_half = (point_px / 480.0f) * depth / pyy_safe;
                    }
                    if (n >= max || !out) { ++n; continue; }
                    OrreParticleQuad& q = out[n++];
                    q = OrreParticleQuad{};
                    q.link = pp->linkNo;
                    // psDispSub: the axes at the particle's size (times the generator's scale with posFlags 0x20)
                    const float hs = point ? point_half : pp->size;
                    V3 a{A0.x * hs, A0.y * hs, A0.z * hs}, b{B0.x * hs, B0.y * hs, B0.z * hs};
                    if (!point && pp->gen && (pp->gen->posFlags & 0x20)) {
                        a.x *= pp->gen->scale.x; b.x *= pp->gen->scale.x;
                        a.y *= pp->gen->scale.y; b.y *= pp->gen->scale.y;
                        a.z *= pp->gen->scale.z; b.z *= pp->gen->scale.z;
                    }
                    // the position and motion drawn: in an application SRT's frame (psDispSubAppSRT; release step 4) a
                    // camera-facing frame turns them by the camera's rotation (the motion only when not a tornado), then
                    // adds the frame's translate; otherwise translate + position
                    V3 pos = pp->pos, vel = pp->vel;
                    if (pp->app) {
                        if (pp->app->screen) {
                            pos = cam_rot(pos);
                            if (!(pp->kind & 0x4)) vel = cam_rot(vel);  // (Tornado = kind bit 2, psdisp.c)
                        }
                        pos.x += pp->app->translate.x;
                        pos.y += pp->app->translate.y;
                        pos.z += pp->app->translate.z;
                    }
                    const V3 prev{pos.x - vel.x, pos.y - vel.y, pos.z - vel.z};
                    float angle = point ? 0.0f : pp->rotate;  // (a point is never turned)
                    if (!point && (pp->kind & (0x100000 | 0x200000))) {  // Trail / DirVec: turned to the screen-space motion
                        const float w0 = vm[2][3] + (vm[2][2] * pos.z + (vm[2][0] * pos.x + vm[2][1] * pos.y));
                        const float w1 = vm[2][3] + (vm[2][2] * prev.z + (vm[2][0] * prev.x + vm[2][1] * prev.y));
                        if (0.0f != w0 && 0.0f != w1) {
                            const float i0 = -1.0f / w0, i1 = -1.0f / w1;
                            auto row = [&](int r, const V3& p) { return pv[r][3] + (pv[r][2] * p.z + (pv[r][0] * p.x + pv[r][1] * p.y)); };
                            const float dx = i0 * row(0, pos) - i1 * row(0, prev), dy = i0 * row(1, pos) - i1 * row(1, prev);
                            angle = std::fabs(dy) < kFltMin ? (dx >= 0.0f ? 1.5707964f : -1.5707964f) : std::atan2(dx, dy);
                            if (pp->kind & 0x200000) angle += pp->rotate;
                        }
                    }
                    if (std::fabs(angle) > 0.01) {
                        Mtx m;
                        mtx_rot_axis(m, V3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}, angle);
                        a = mtx_mul(m, a);
                        b = mtx_mul(m, b);
                    }
                    const Col prim = ramp(pp->primCol, pp->primColTarget, pp->primColRemain, pp->primColCount);
                    const Col env = ramp(pp->envCol, pp->envColTarget, pp->envColRemain, pp->envColCount);
                    const bool primenv = (pp->kind & 0x80) != 0, lit = (pp->kind & 0x80000000u) != 0, trail = (pp->kind & 0x100000) != 0;
                    // psDispSubMakePolygon: pos - A, pos - B, pos + A, pos + B; a trail's tail corners at pos - vel
                    const V3 base[4] = {trail ? prev : pos, pos, pos, trail ? prev : pos};
                    V3 off[4] = {{-a.x, -a.y, -a.z}, {-b.x, -b.y, -b.z}, a, b};
                    if (point && trail) {
                        // the line's strip: sideways on screen (the motion crossed with the camera's axis), half its
                        // width at each end's own depth
                        const V3 d{pos.x - prev.x, pos.y - prev.y, pos.z - prev.z}, cz{vm[2][0], vm[2][1], vm[2][2]};
                        const V3 sd{d.y * cz.z - d.z * cz.y, d.z * cz.x - d.x * cz.z, d.x * cz.y - d.y * cz.x};
                        const float sl = std::sqrt(sd.x * sd.x + sd.y * sd.y + sd.z * sd.z);
                        const float dp = depth_of(prev);
                        if (sl > 1e-6f && dp > 0.0f) {
                            const V3 s{sd.x / sl, sd.y / sl, sd.z / sl};
                            const float hp = (point_px / 480.0f) * dp / pyy_safe, hc = point_half;
                            off[0] = {-s.x * hp, -s.y * hp, -s.z * hp};
                            off[1] = {-s.x * hc, -s.y * hc, -s.z * hc};
                            off[2] = {s.x * hc, s.y * hc, s.z * hc};
                            off[3] = {s.x * hp, s.y * hp, s.z * hp};
                        }
                    }
                    // the rasterised colour: a trail's vertex colours (getClrTrail: prim, or white with PrimEnv; the tail
                    // corners' alpha times the trail factor); otherwise the lit channel's material colour, prim (white with
                    // PrimEnv) as setupChanReg gives it
                    const Col ras = primenv ? Col{255, 255, 255, 255} : prim;
                    const int uv0 = point ? 0 : (int)((pp->kind >> 16) & 0xC);  // (a point: the whole texture, GX_TO_ONE)
                    for (int k = 0; k < 4; ++k) {
                        q.pos[k][0] = base[k].x + off[k].x;
                        q.pos[k][1] = base[k].y + off[k].y;
                        q.pos[k][2] = base[k].z + off[k].z;
                        // MirrorS / MirrorT (kind bits 0x20 / 0x40; psDispMain: the texture matrix scales the
                        // coordinates by 2 and the texture wraps mirrored, so a picture stored as one half shows whole:
                        // Ice Beam's snowflakes; verified from the code, 2026-11-19, after the move sweep's pictures
                        // showed them halved). The shader folds 0..2 back, as GX_MIRROR does
                        // (and MirrorT also moves t by 1, temp_mtx[1][3] = 1: t runs 1..3, so the picture's top row
                        // lands in the quad's middle; verified from psdisp.c after Ice Beam's snowflakes came out as
                        // "anchors" with t 0..2)
                        q.uv[k][0] = kQuadUV[2 * (uv0 + k)] * ((pp->kind & 0x20) ? 2.0f : 1.0f);
                        q.uv[k][1] = (pp->kind & 0x40) ? kQuadUV[2 * (uv0 + k) + 1] * 2.0f + 1.0f : kQuadUV[2 * (uv0 + k) + 1];
                        q.colour[k][0] = ras.r;
                        q.colour[k][1] = ras.g;
                        q.colour[k][2] = ras.b;
                        q.colour[k][3] = trail && (k == 0 || k == 3) ? (uint8_t)((float)ras.a * pp->trail) : ras.a;
                    }
                    // setupTevReg: C0 = prim, C1 = env with PrimEnv or 0 (unlit, untrailed); C2 = the grey material colour
                    q.prim[0] = prim.r; q.prim[1] = prim.g; q.prim[2] = prim.b; q.prim[3] = prim.a;
                    if (primenv) { q.env[0] = env.r; q.env[1] = env.g; q.env[2] = env.b; q.env[3] = env.a; }
                    const uint8_t mrgb = ramp1(pp->matRGB, pp->matRGBTarget, pp->matColRemain, pp->matColCount);
                    uint8_t ma = ramp1(pp->matA, pp->matATarget, pp->matColRemain, pp->matColCount);
                    if (lit && !primenv) ma = (uint8_t)(ma * prim.a / 255);
                    q.mat[0] = q.mat[1] = q.mat[2] = mrgb;
                    q.mat[3] = ma;
                    q.kind = pp->kind;
                    q.textured = (pp->kind & 0x400) != 0;
                    q.bank = pp->bank;
                    q.group = pp->texGroup;
                    q.image = pp->poseNum;
                    q.alpha_mode = pp->aCmpMode;
                    q.alpha_ref0 = ramp1(pp->aCmpParam1, pp->aCmpParam1Target, pp->aCmpRemain, pp->aCmpCount);
                    q.alpha_ref1 = ramp1(pp->aCmpParam2, pp->aCmpParam2Target, pp->aCmpRemain, pp->aCmpCount);
                    (void)trail;
                }
    return n;
}
