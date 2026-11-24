// OrrePort, frame smoothing step (c): the camera blend. smooth_camera.hpp says what it does and why; this file is the
// maths, the per-step plan and the blend buffer. Render worker only (plan_step from end_frame's job, prepare and the
// override from the presenter's in-between pictures), so no locks; the counters are atomics because the game thread
// reads them for the log. Platform-neutral (WebGPU through Dawn; plain C++ maths).
#include "smooth_camera.hpp"

#include "resources.hpp"
#include "../gx/gx.hpp"
#include "../gx/shader_info.hpp"
#include "../webgpu/gpu.hpp"
#include "dolphin/gx/GXAurora.h" // OrrePort, round 11: GX_AURORA_ORRE_2D_VERTICES

#include <algorithm>
#include <array>
#include <atomic>
#include <bit> // OrrePort, item (b): std::popcount (texture matrices counted)
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string> // OrrePort, round 12: the 2D log's piece list
#include <tuple>
#include <utility>

namespace aurora::gfx::smooth_camera {
using detail::FramePacket;
using detail::OrreSmoothBlock;
using detail::OrreSmoothCamera;
using Mat4f = Mat4x4<float>;

namespace {
// The blend buffer: the re-aimed copies of one picture's blocks. 16 MiB holds about 8,000 blocks of 2 KB; a step past
// that leaves the rest unblended (counted as overflow). Created on first use.
constexpr uint64_t BlendBufferSize = 16ull * 1024 * 1024;
// the cut thresholds (smooth_camera.hpp); Dan's XD port snaps at about 35 degrees or 25 % (our earlier study notes)
constexpr double CutDegrees = 35.0;
constexpr double CutJumpOfDistance = 0.25;
constexpr double CutFovRatio = 0.25;
constexpr double Pi = 3.14159265358979323846;

struct Vec3d {
  double x = 0, y = 0, z = 0;
};
Vec3d operator+(Vec3d a, Vec3d b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3d operator-(Vec3d a, Vec3d b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3d operator*(Vec3d a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(Vec3d a, Vec3d b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3d cross(Vec3d a, Vec3d b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double length(Vec3d a) { return std::sqrt(dot(a, a)); }
Vec3d lerp(Vec3d a, Vec3d b, double f) { return a + (b - a) * f; }

using Mat4d = std::array<std::array<double, 4>, 4>; // rows

Mat4d mul(const Mat4d& a, const Mat4d& b) {
  Mat4d out{};
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      double sum = 0;
      for (int k = 0; k < 4; ++k) {
        sum += a[r][k] * b[k][c];
      }
      out[r][c] = sum;
    }
  }
  return out;
}

// A camera taken apart: eye, the point it looks at, its up direction (all world space), and its rotation rows
struct Pose {
  Vec3d eye, target, up;
  Vec3d rows[3];
  double distance = 0;
  bool rigid = false;
  bool hasTarget = false;
};

Pose decompose(const OrreSmoothCamera& camera) {
  Pose pose;
  Vec3d t{camera.view[0][3], camera.view[1][3], camera.view[2][3]};
  for (int i = 0; i < 3; ++i) {
    pose.rows[i] = {camera.view[i][0], camera.view[i][1], camera.view[i][2]};
  }
  // rigid: orthonormal rows (C_MTXLookAt builds such; a view matrix set directly might carry scale)
  pose.rigid = true;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const double expected = i == j ? 1.0 : 0.0;
      if (std::abs(dot(pose.rows[i], pose.rows[j]) - expected) > 2e-3) {
        pose.rigid = false;
      }
    }
  }
  // eye = -R^T t
  pose.eye = (pose.rows[0] * t.x + pose.rows[1] * t.y + pose.rows[2] * t.z) * -1.0;
  pose.up = pose.rows[1];
  const Vec3d forward = pose.rows[2] * -1.0; // the camera looks down its -z
  if (camera.hasInterest) {
    const Vec3d interest{camera.interest[0], camera.interest[1], camera.interest[2]};
    const Vec3d toInterest = interest - pose.eye;
    const double d = length(toInterest);
    // the interest must lie in front of the camera (within 60 degrees of its view); otherwise the matrix came from
    // somewhere else and the distance means nothing. Only its distance is used (the pivot sits on the view axis), so a
    // camera that frames its interest off-centre still blends around the right depth. *Widened 2026-11-16* from
    // within 8 degrees: P01's battle cameras failed the narrow test in about half their steps (likely framed
    // off-centre; unchecked)
    if (d > 1e-4 && d < 1e7 && dot(toInterest, forward) > 0.5 * d) {
      pose.distance = d;
      pose.hasTarget = true;
    }
  }
  pose.target = pose.eye + forward * (pose.hasTarget ? pose.distance : 1.0);
  return pose;
}

// C_MTXLookAt's convention: rows right, up, look (look points from the target to the eye), translation -R x eye
bool look_at(Vec3d eye, Vec3d up, Vec3d target, Mat4d& out) {
  Vec3d look = eye - target;
  const double lookLen = length(look);
  if (lookLen < 1e-9) {
    return false;
  }
  look = look * (1.0 / lookLen);
  Vec3d right = cross(up, look);
  const double rightLen = length(right);
  if (rightLen < 1e-9) {
    return false;
  }
  right = right * (1.0 / rightLen);
  const Vec3d upOut = cross(look, right);
  const Vec3d rows[3] = {right, upOut, look};
  for (int i = 0; i < 3; ++i) {
    out[i] = {rows[i].x, rows[i].y, rows[i].z, -dot(rows[i], eye)};
  }
  out[3] = {0, 0, 0, 1};
  return true;
}

// a plain perspective projection's last row is (0, 0, -1, 0): GX's (C_MTXPerspective, C_MTXFrustum)
bool plain_perspective(const float proj[4][4]) {
  return proj[3][0] == 0.f && proj[3][1] == 0.f && proj[3][2] == -1.f && proj[3][3] == 0.f && proj[0][0] != 0.f &&
         proj[1][1] != 0.f;
}

double turn_degrees(const Pose& a, const Pose& b) {
  // the angle of the rotation between the two: trace(Rb Ra^T) = 1 + 2 cos(angle)
  double trace = 0;
  for (int i = 0; i < 3; ++i) {
    trace += dot(b.rows[i], a.rows[i]);
  }
  const double c = std::clamp((trace - 1.0) / 2.0, -1.0, 1.0);
  return std::acos(c) * 180.0 / Pi;
}

// ---- step (d) helpers: 3x4 affine matrices as GX keeps them (rows; column 3 the translation) ----
using Aff = std::array<std::array<double, 4>, 3>;

Aff aff_from_floats(const float* m) {
  Aff out{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 4; ++c) {
      out[r][c] = m[r * 4 + c];
    }
  }
  return out;
}

// a x b, both affine (a's rotation part applied to b's translation, plus a's translation)
Aff aff_mul(const Aff& a, const Aff& b) {
  Aff out{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 4; ++c) {
      double sum = 0;
      for (int k = 0; k < 3; ++k) {
        sum += a[r][k] * b[k][c];
      }
      out[r][c] = sum + (c == 3 ? a[r][3] : 0.0);
    }
  }
  return out;
}

// a x b for b's 3x3 part only (normal matrices: no translation)
Aff aff_mul3(const Aff& a, const Aff& b) {
  Aff out{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      double sum = 0;
      for (int k = 0; k < 3; ++k) {
        sum += a[r][k] * b[k][c];
      }
      out[r][c] = sum;
    }
    out[r][3] = b[r][3];
  }
  return out;
}

// round 5: a x b's 3x3 parts, keeping a's translation (a texture matrix fed with normals, times a normal-space change)
Aff aff_mul_rot(const Aff& a, const Aff& b) {
  Aff out{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      double sum = 0;
      for (int k = 0; k < 3; ++k) {
        sum += a[r][k] * b[k][c];
      }
      out[r][c] = sum;
    }
    out[r][3] = a[r][3];
  }
  return out;
}

// round 5: the inverse of any invertible affine 3x4 (3x3 by cofactors, then the translation); false when singular
bool aff_inverse(const Aff& m, Aff& out) {
  const double a = m[0][0], b = m[0][1], c = m[0][2], d = m[1][0], e = m[1][1], f = m[1][2], g = m[2][0], h = m[2][1],
               i = m[2][2];
  const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (!std::isfinite(det) || std::abs(det) < 1e-12) {
    return false;
  }
  const double k = 1.0 / det;
  out[0] = {(e * i - f * h) * k, (c * h - b * i) * k, (b * f - c * e) * k, 0};
  out[1] = {(f * g - d * i) * k, (a * i - c * g) * k, (c * d - a * f) * k, 0};
  out[2] = {(d * h - e * g) * k, (b * g - a * h) * k, (a * e - b * d) * k, 0};
  for (int r = 0; r < 3; ++r) {
    out[r][3] = -(out[r][0] * m[0][3] + out[r][1] * m[1][3] + out[r][2] * m[2][3]);
  }
  return true;
}

// the inverse of a rigid view matrix (rows orthonormal): R^T, -R^T t
Aff rigid_inverse(const Aff& v) {
  Aff out{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out[r][c] = v[c][r];
    }
  }
  for (int r = 0; r < 3; ++r) {
    out[r][3] = -(out[r][0] * v[0][3] + out[r][1] * v[1][3] + out[r][2] * v[2][3]);
  }
  return out;
}

double column_length(const float* m, int c) {
  return std::sqrt(static_cast<double>(m[c]) * m[c] + static_cast<double>(m[4 + c]) * m[4 + c] +
                   static_cast<double>(m[8 + c]) * m[8 + c]);
}

// `out` = from + (to - from) x f, number by number, then each of the three axis columns rescaled to the blended length
// of the two (blending rotated axes number by number shortens them, which would shrink a turning model mid-turn)
void blend_matrix(const float* from, const float* to, double f, float* out) {
  for (int i = 0; i < 12; ++i) {
    out[i] = static_cast<float>(from[i] + (static_cast<double>(to[i]) - from[i]) * f);
  }
  for (int c = 0; c < 3; ++c) {
    const double want = column_length(from, c) + (column_length(to, c) - column_length(from, c)) * f;
    const double have = column_length(out, c);
    if (have > 1e-12 && want > 0) {
      const double scale = want / have;
      for (int r = 0; r < 3; ++r) {
        out[r * 4 + c] = static_cast<float>(out[r * 4 + c] * scale);
      }
    }
  }
}

// Whether a position matrix jumped between the two steps, so its model snaps to the newest instead of blending: a
// non-number, a move over 3/4 of the model's distance from the camera in one step, or an axis turned over 90 degrees
// (number-by-number blending folds such an axis through zero). *Guess, tuned by eye:* a Pokemon's lunge moves well
// under half its distance a step; a model moved to a new place (a warp, a reused piece) usually more.
// *Changed 2026-11-17 (round 4):* the move is measured against the model's depth (`depth`: the median depth of all its
// used matrices this step), not the matrix's own. An envelope matrix (a bone's matrix times its bind-pose inverse, or a
// weighted sum of such) carries the model-space origin, which can sit near the camera's plane while the vertices are
// far away: in P15, during Xatu's faint, the player's Aerodactyl or Gengar had slots "moving" 1-11 times their own
// depth every step with an axis turned only 15 degrees (P13, Nascour's battle: 0.8-4.6 times). `why` (the log) gets 1
// for a move, 2 for a turn, 3 for a non-number.
bool jumped(const float* from, const float* to, double depth, int* why = nullptr) {
  for (int i = 0; i < 12; ++i) {
    if (!std::isfinite(from[i]) || !std::isfinite(to[i])) {
      if (why != nullptr) {
        *why = 3;
      }
      return true;
    }
  }
  const double dx = static_cast<double>(to[3]) - from[3];
  const double dy = static_cast<double>(to[7]) - from[7];
  const double dz = static_cast<double>(to[11]) - from[11];
  if (std::sqrt(dx * dx + dy * dy + dz * dz) > 0.75 * std::max(depth, 1e-3)) {
    if (why != nullptr) {
      *why = 1;
    }
    return true;
  }
  for (int c = 0; c < 3; ++c) {
    double d = 0;
    for (int r = 0; r < 3; ++r) {
      d += static_cast<double>(from[r * 4 + c]) * to[r * 4 + c];
    }
    if (d < 0) {
      if (why != nullptr) {
        *why = 2;
      }
      return true;
    }
  }
  return false;
}

// one camera of the newest step, with the previous step's to blend from
struct Planned {
  uint64_t id = 0;
  uint32_t occurrence = 0;
  // how far it moved and turned from the step before (0 when unmatched): the next step's cut test compares with it
  double eyeMove = 0, targetMove = 0, turn = 0;
  Cut cut = Cut::Unmatched;
  Pose from, to;
  float fromProj[4][4]{};
  float toProj[4][4]{};
  float toView[3][4]{};
};

std::vector<Planned> g_plan; // indexed like the kept step's cameras

// Step (d): one matched model block of the newest step. `fromPos` / `fromNrm` are the step before's ten position and
// ten normal matrices (3x4, rows, as the uniform block holds them), already carried into the newest step's camera space;
// `mask` has a bit per matrix slot that moved and blends (the others keep the newest).
constexpr uint32_t PnSlots = 10; // gx::MaxPnMtx
struct ModelBlend {
  uint32_t mask = 0;
  float fromPos[PnSlots][12]{};
  float fromNrm[PnSlots][12]{};
  // item (b), scrolling textures (2026-11-17): the post-transform texture matrices fed by texture coordinates that
  // changed and blend (bit k: GX_PTTEXMTX0 + 3k), and the step before's, offsets already shifted across a wrap
  uint32_t texMask = 0;
  float fromTex[20][12]{}; // gx::MaxPTTexMtx
  // item (c), the two-step history (2026-11-17): the blend's own fraction is bias + scale x the picture's (1 and 0:
  // the picture's own). A menu model posed on every other step blends over two steps: half way in the step it changed,
  // the rest in the step it held (plan_models)
  double fScale = 1.0;
  double fBias = 0.0;
  // the colour-data blend (2026-11-18): the step before's colour floats of the block's colour run (OrreSmoothBlock::
  // colorAt), where the parts that must not blend (the light masks, the lights' positions, directions and attenuation)
  // already hold the newest step's values; 0 floats: the colours keep the newest
  uint32_t colorFloats = 0;
  float fromColor[256]{};
  // the capture prototype's warp blend (2026-11-18; ORRE_SMOOTH_CAPTURE=1): the texture matrices fed by the vertex's
  // own texture coordinates (bit k: GX_TEXMTX0 + 3k) and the indirect matrices (three, 8 floats each as the block
  // holds them), the step before's, for a warp draw (Psychic's) whose picture moves through them
  uint32_t uvMtxMask = 0;
  float fromUvMtx[10][12]{};
  bool indBlend = false;
  float fromInd[24]{};
};
constexpr uint32_t ColorFloatsMax = 256;
constexpr uint32_t PtSlots = 20; // gx::MaxPTTexMtx: the post-transform texture matrices
std::vector<ModelBlend> g_models;
std::vector<int32_t> g_blockModel; // per newest-step block: its index in g_models, or -1
// round 10: per newest-step block, 1 when its model moves with its camera (plan_models' lock): drawn exactly as the
// step drew it in every in-between (no camera re-aim, no blend), so it keeps its place on the screen
std::vector<uint8_t> g_blockLocked;
// round 11, 2D at 120: a named 2D piece whose value changed since the step before (a bar's drawn width), and how an
// in-between draws it: its matrix slot scaled across x about the piece's left end by the blended value over the newest
struct Piece2DBlend {
  float from = 0.f, to = 0.f, x0 = 0.f;
  uint32_t slot = 0;
};
std::vector<Piece2DBlend> g_2d;
std::vector<int32_t> g_block2d; // per newest-step block: its index in g_2d, or -1
// round 11, 2D at 120, vertex mode (the pause menu): a piece's block whose position matrix moved since the step before
// (a window sliding: the game moves a window by its sprite camera, gs_thread_hi_range_800FE35C.c), with the step
// before's matrix; an in-between draws the matrix blended element by element (2D: translations and scales only)
struct Matrix2DBlend {
  uint32_t slot = 0;
  float from[12]{};
};
std::vector<Matrix2DBlend> g_2dMtx;
std::vector<int32_t> g_block2dMtx; // per newest-step block: its index in g_2dMtx, or -1
// a vertex-mode piece of the newest step: its draws' vertices span [spanStart, spanEnd) in the step's vertex buffer
// (the piece's first draw never merges with a draw before it, the marker's DirtyUniform), and per draw the step before's
// matching draw (-1: none, drawn as the step drew it). An in-between writes the whole span, blended where it can, past
// the step's own vertices in the same buffer, and its draws read it there (override_vertices)
struct VertexPiece2D {
  uint32_t spanStart = 0, spanEnd = 0;
  std::vector<std::pair<uint32_t, int32_t>> chunks; // newest chunk index, step-before chunk index
};
std::vector<VertexPiece2D> g_2dVtx;
// the step before's vertex-mode draws and their bytes (kept here: the plan sees each step once, in order), and the
// frame number they came from (a gap in the numbers means a step went unplanned: nothing is blended across it)
std::vector<detail::OrreSmooth2DChunk> g_prev2dChunks;
std::vector<uint8_t> g_prev2dChunkBytes;
uint32_t g_prev2dFrame = 0;
bool g_prev2dValid = false;
std::vector<detail::OrreSmooth2DChunk> g_from2dChunks; // the planned step's step before (what its in-betweens blend from)
std::vector<uint8_t> g_from2dChunkBytes;

// one big-endian GX component <-> double (the position types a vertex format allows; `frac`: fixed-point bits)
double read_component(const uint8_t* p, uint8_t type, uint8_t frac) {
  const double scale = 1.0 / static_cast<double>(1u << frac);
  switch (type) {
  case GX_F32: {
    const uint32_t bits = (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | uint32_t{p[3]};
    float v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
  }
  case GX_S16:
    return static_cast<int16_t>((p[0] << 8) | p[1]) * scale;
  case GX_U16:
    return static_cast<uint16_t>((p[0] << 8) | p[1]) * scale;
  case GX_S8:
    return static_cast<int8_t>(p[0]) * scale;
  default:
    return p[0] * scale;
  }
}

void write_component(uint8_t* p, uint8_t type, uint8_t frac, double v) {
  const double scaled = v * static_cast<double>(1u << frac);
  switch (type) {
  case GX_F32: {
    const float fv = static_cast<float>(v);
    uint32_t bits;
    std::memcpy(&bits, &fv, sizeof(bits));
    p[0] = static_cast<uint8_t>(bits >> 24);
    p[1] = static_cast<uint8_t>(bits >> 16);
    p[2] = static_cast<uint8_t>(bits >> 8);
    p[3] = static_cast<uint8_t>(bits);
    break;
  }
  case GX_S16:
  case GX_U16: {
    const auto i = static_cast<int32_t>(std::lround(scaled));
    const auto u = static_cast<uint16_t>(type == GX_S16 ? std::clamp(i, -32768, 32767) : std::clamp(i, 0, 65535));
    p[0] = static_cast<uint8_t>(u >> 8);
    p[1] = static_cast<uint8_t>(u);
    break;
  }
  case GX_S8:
    p[0] = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(static_cast<int32_t>(std::lround(scaled)), -128, 127)));
    break;
  default:
    p[0] = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(std::lround(scaled)), 0, 255));
    break;
  }
}

// round 11, vertex mode: one draw's vertices at fraction f between the step before's (`was`) and the newest (`now`),
// into `dst` (already the newest's copy): positions component by component (integer positions round to the nearest
// whole unit: the game's 2D draws whole pixels), the first colour byte by byte. The rest stays the newest's (the plan
// checked it is the same in both).
void blend_vertices(uint8_t* dst, const uint8_t* was, const uint8_t* now, uint32_t length,
                    const detail::OrreSmooth2DLayout& l, double f) {
  const uint32_t comp = l.posCount != 0 ? l.posBytes / l.posCount : 0;
  for (uint32_t v = 0; v + l.vtxSize <= length; v += l.vtxSize) {
    for (uint32_t k = 0; k < l.posCount; ++k) {
      const uint32_t at = v + k * comp;
      const double a = read_component(was + at, l.posType, l.posFrac);
      const double b = read_component(now + at, l.posType, l.posFrac);
      if (a != b) {
        write_component(dst + at, l.posType, l.posFrac, a + (b - a) * f);
      }
    }
    for (uint32_t k = 0; k < l.clrBytes; ++k) {
      const uint32_t at = v + l.posBytes + k;
      dst[at] = static_cast<uint8_t>(std::lround(was[at] + (static_cast<double>(now[at]) - was[at]) * f));
    }
  }
}
std::vector<uint8_t> g_vpatch;                      // the in-between's blended vertices (one write per picture)
std::vector<std::pair<uint32_t, uint32_t>> g_vmap; // a draw's vtxStart in the step -> where its blended copy starts
uint32_t g_lastModelWrites = 0;    // model-blended blocks in the last prepare (the coverage counters, aurora.cpp)
uint32_t g_lastTexFollows = 0;     // round 5: blocks whose position / normal texture matrices followed their piece
// item (c): the newest step has a menu model in the first half of its two-step blend, so even its picture at
// fraction 1 (normally the step's own) must be drawn again with that model half way (two_step_at_one)
bool g_twoStepAtOne = false;

std::vector<uint8_t> g_patch;
std::vector<std::pair<uint32_t, uint32_t>> g_map; // a block's offset in the step -> its copy's offset in the blend buffer
bool g_overrideActive = false;
wgpu::Buffer g_blendBuffer;
wgpu::BindGroup g_blendBindGroup;

struct AtomicStats {
  std::atomic<uint64_t> cameras{0}, blocks{0}, looseBlocks{0}, overflow{0}, bytes{0};
  std::array<std::atomic<uint64_t>, static_cast<int>(Cut::Count)> cuts{};
  // step (d)
  std::atomic<uint64_t> modelTagged{0}, modelUntagged{0}, modelMatched{0}, modelMoved{0}, modelSnapped{0},
      modelHeld{0}, modelBlockWrites{0}, looseSameFrustum{0}, looseTagged{0};
  std::atomic<uint64_t> modelsSnapped{0}, modelBorrowed{0}; // round 4
  std::atomic<uint64_t> texFollows{0};                      // round 5
  std::atomic<uint64_t> particleTagged{0}, particleBlended{0}, particleNew{0}, particleReused{0}, particleJumped{0},
      particleHeld{0}; // item (a)
  std::atomic<uint64_t> scrollBlended{0}, scrollWrapped{0}, scrollSnapped{0}; // item (b)
  std::atomic<uint64_t> scrollUnsteady{0};                                     // round 9
  std::atomic<uint64_t> lockedBlocks{0};                                       // round 10
  std::atomic<uint64_t> p2Tagged{0}, p2Blended{0}, p2New{0}, p2Swapped{0}, p2Written{0}, p2NoSpan{0}; // round 11
  std::atomic<uint64_t> p2VtxDraws{0}, p2VtxSnapped{0}, p2MtxBlocks{0}, p2VtxWritten{0};             // vertex mode
  std::atomic<uint64_t> twoStepBlocks{0}, twoStepModels{0};                   // item (c)
  std::atomic<uint64_t> colorBlended{0}, colorHeld{0};                         // the colour-data blend
  std::atomic<uint64_t> cornerParticles{0}; // (3b): particles blended by their corners
};
AtomicStats g_stats;
std::mutex g_cutLock;
CutInfo g_lastCut;

void note_cut(Cut cut, uint32_t retrace, const Planned& p, double degrees, double fovRatio) {
  // only real jumps are events worth a log line; unmatched / not rigid / no target are steady states, counted only
  if (cut != Cut::Rotation && cut != Cut::Eye && cut != Cut::Target && cut != Cut::Fov) {
    return;
  }
  std::lock_guard lock{g_cutLock};
  ++g_lastCut.sequence;
  g_lastCut.reason = cut;
  g_lastCut.retrace = retrace;
  g_lastCut.degrees = static_cast<float>(degrees);
  g_lastCut.eyeJump = static_cast<float>(length(p.to.eye - p.from.eye));
  g_lastCut.targetJump = static_cast<float>(length(p.to.target - p.from.target));
  g_lastCut.distance = static_cast<float>(std::max(p.from.distance, p.to.distance));
  g_lastCut.fovRatio = static_cast<float>(fovRatio);
}

bool ensure_buffer() {
  if (g_blendBuffer) {
    return true;
  }
  const wgpu::BufferDescriptor descriptor{
      .label = "Frame smoothing blend uniforms",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = BlendBufferSize,
  };
  g_blendBuffer = webgpu::g_device.CreateBuffer(&descriptor);
  if (!g_blendBuffer) {
    return false;
  }
  // the same layout and binding size as Aurora's own uniform bind group (frame.cpp), so the GX pipelines take it
  const std::array entries{
      wgpu::BindGroupEntry{
          .binding = 0,
          .buffer = g_blendBuffer,
          .size = gx::MaxUniformSize,
      },
  };
  const wgpu::BindGroupDescriptor bindGroupDescriptor{
      .label = "Frame smoothing blend uniform bind group",
      .layout = detail::resources().uniformBindGroupLayout,
      .entryCount = entries.size(),
      .entries = entries.data(),
  };
  g_blendBindGroup = webgpu::g_device.CreateBindGroup(&bindGroupDescriptor);
  return static_cast<bool>(g_blendBindGroup);
}
} // namespace

namespace {
// Step (d): match the newest step's model blocks with the step before's and work out what each blends from
// a view matrix's rotation rows orthonormal (as decompose's test)
bool rigid_view(const float view[3][4]) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      double d = 0;
      for (int k = 0; k < 3; ++k) {
        d += static_cast<double>(view[i][k]) * view[j][k];
      }
      if (std::abs(d - (i == j ? 1.0 : 0.0)) > 2e-3) {
        return false;
      }
    }
  }
  return true;
}

// ORRE_SMOOTH_MODELLOG=<from>-<to> (a test switch, read once; retrace counts; round 4, 2026-11-17): per step in that
// range, what the model plan found: pieces new this step or gone since the step before (with whether their model was
// there on the other side), pieces drawn a different number of times, and every matched piece with a matrix that
// jumped (which used slots blended, which jumped, and why). Observation only; at most 120 lines a step.
struct ModelLog {
  bool on = false;
  uint32_t lines = 0;
  void line(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    if (lines >= 120) {
      return;
    }
    ++lines;
    va_list args;
    va_start(args, fmt);
    std::fputs("[smooth] modellog ", stderr);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
    va_end(args);
  }
};
bool model_log_wanted(uint32_t retrace) {
  // several ranges, comma-separated: "<from>-<to>,<from>-<to>"
  static const std::vector<std::pair<uint32_t, uint32_t>> ranges = [] {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    const char* v = std::getenv("ORRE_SMOOTH_MODELLOG");
    while (v != nullptr && *v != '\0') {
      unsigned from = 0, to = 0;
      int used = 0;
      if (std::sscanf(v, "%u-%u%n", &from, &to, &used) != 2) {
        break;
      }
      out.emplace_back(from, to);
      v += used;
      v += *v == ',' ? 1 : 0;
    }
    return out;
  }();
  for (const auto& [from, to] : ranges) {
    if (retrace >= from && retrace <= to) {
      return true;
    }
  }
  return false;
}

void plan_models(const FramePacket& step, const std::vector<OrreSmoothCamera>& previous,
                 const std::vector<OrreSmoothBlock>& previousBlocks, const std::vector<uint8_t>& previousBytes,
                 bool history, uint32_t retrace, const TwoBack& twoBack, bool twoBackOk) {
  g_models.clear();
  g_blockModel.assign(step.orreBlocks.size(), -1);
  ModelLog log;
  log.on = model_log_wanted(retrace);
  // ---- Round 4 (2026-11-17; docs/port/frame-smoothing-plan.md section 16): a model blends whole or snaps whole. ----
  // snatcherLeo's hand check of round 3: a white line by Nuzleaf's mouth as it was sent out and as it fainted, and Nascour's
  // Xatu clipping as it fainted (Match only). The model log on P13 and P15 (verified) showed three ways one model's
  // pieces were treated differently within one in-between, each tearing the model at the seams between them:
  //  1. single matrices snapped inside a piece: an envelope piece's ten slots are bones (or weighted bone mixes); round
  //     3 snapped any slot that "jumped" and blended the rest, so vertices on the snapped slots sat at the newest pose
  //     while their neighbours sat half a step back. In hits and faints that happened every step, mostly from a false
  //     "move" (an envelope matrix's own origin can sit near the camera's plane: jumped's comment);
  //  2. blocks matched to the wrong partner: blocks were numbered per piece across all passes, and some pieces are
  //     drawn in HSD's shadow pass only in some steps (Nuzleaf's mouth, Nascour's Xatu's wing panel: the cause of both
  //     of snatcherLeo's cases, verified in the pictures). When the caster draw came, the screen draw had no partner (drawn at
  //     its newest pose alone); when it went, the screen draw was blended from the caster's block (another camera's
  //     draw) and landed elsewhere;
  //  3. new pieces of a model that was already there (a part switched on, e.g. pieces appearing one step after the rest
  //     when a Pokemon is sent out) shown at the newest pose next to blended neighbours.
  // The rules now: blocks are matched by (JObj, PObj, render pass, draw number), every block of one draw shares the
  // draw's plan; only the slots the piece uses are tested and blended (the rest hold matrices an earlier piece left);
  // a new rigid piece borrows the step before's matrix of the same joint (a part swapped on the same bone blends like
  // the part it replaced); and when anything of a model jumps, or a new piece of it has nothing to blend from, the whole
  // model (every piece with its root, in every pass) shows its newest pose in this step's in-betweens. Snapping a model
  // whole costs one step of step-rate motion for that model; snapping a part of it tore it.
  const auto passOf = [](const std::vector<OrreSmoothCamera>& cameras, const OrreSmoothBlock& b) {
    return b.camera < cameras.size() ? cameras[b.camera].renderPass : -1;
  };
  // the slots a block's piece uses (all ten when the marker didn't say: a test program without the game side)
  const auto used = [](const OrreSmoothBlock& b) { return b.modelSlots != 0 ? b.modelSlots & 0x3FFu : 0x3FFu; };
  struct Key {
    uint64_t jobj, pobj;
    int32_t pass;
    uint32_t occurrence;
    bool operator<(const Key& o) const {
      return std::tie(jobj, pobj, pass, occurrence) < std::tie(o.jobj, o.pobj, o.pass, o.occurrence);
    }
  };
  // the step before's model blocks (only those drawn with their camera's own projection): the first block of each
  // draw, a rigid piece's block by (joint, pass) for the swap rule, and the models drawn in each pass
  std::map<Key, uint32_t> before;
  std::map<std::pair<uint64_t, int32_t>, uint32_t> beforeRigid;
  std::set<std::pair<uint64_t, int32_t>> modelsBefore;
  for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
    const auto& b = previousBlocks[i];
    if (b.modelPObj != 0 && b.projIsCamera) {
      const int32_t pass = passOf(previous, b);
      before.emplace(Key{b.modelJObj, b.modelPObj, pass, b.modelOccurrence}, i);
      if (used(b) == 1u) {
        beforeRigid.emplace(std::pair{b.modelJObj, pass}, i);
      }
      modelsBefore.emplace(b.modelRoot, pass);
    }
  }
  // the same two steps back (item c's two-step history; round 9's scroll rule): empty without two steps of history
  std::map<Key, uint32_t> before2;
  for (uint32_t i = 0; twoBackOk && i < twoBack.blocks.size(); ++i) {
    const auto& b = twoBack.blocks[i];
    if (b.modelPObj != 0 && b.projIsCamera) {
      before2.emplace(Key{b.modelJObj, b.modelPObj, passOf(twoBack.cameras, b), b.modelOccurrence}, i);
    }
  }
  // each model's depth in each pass this step: the median distance in front of its camera of the used matrices'
  // origins (the jump test's scale; jumped says why not each matrix's own)
  // *Changed 2026-11-17 (round 10; the plan's section 23):* from the model's rigid pieces when it has any. A rigid
  // piece's matrix is its joint's placement (its origin is where the piece is); an envelope piece's matrices are a
  // bone's matrix times its bind-pose inverse, whose origin can sit anywhere, often near the camera's plane. Seen in
  // P17b's battle (the model log): a model's median over all matrices gave a depth of 0.36 while its first piece was
  // 233 units away at frame 6,900. (*Corrected the same day:* at Thunderbolt's frames that model's rigid pieces are at
  // the camera itself, depth 0.3, so it still snaps there; it is mostly clipped. The rule stays: a rigid piece is the
  // better measure of where a model is.) Models with no rigid piece keep the median of all.
  std::map<std::pair<uint64_t, int32_t>, std::vector<float>> depthSamples, rigidSamples;
  for (const auto& b : step.orreBlocks) {
    if (b.modelPObj == 0 || !b.projIsCamera || b.bytes < b.projOffset + 64 + PnSlots * 48) {
      continue;
    }
    auto& samples = depthSamples[{b.modelRoot, passOf(step.orreCameras, b)}];
    for (uint32_t s = 0; s < PnSlots; ++s) {
      if ((used(b) & (1u << s)) != 0) {
        const float* m = reinterpret_cast<const float*>(step.orreBlockBytes.data() + b.bytesOffset + b.projOffset + 64 + s * 48);
        samples.push_back(std::abs(m[11]));
        if (used(b) == 1u && b.modelSlots != 0) {
          rigidSamples[{b.modelRoot, passOf(step.orreCameras, b)}].push_back(std::abs(m[11]));
        }
      }
    }
  }
  std::map<std::pair<uint64_t, int32_t>, double> modelDepth;
  for (auto& [model, all] : depthSamples) {
    const auto rigid = rigidSamples.find(model);
    auto& samples = rigid != rigidSamples.end() && !rigid->second.empty() ? rigid->second : all;
    if (!samples.empty()) {
      std::nth_element(samples.begin(), samples.begin() + static_cast<long>(samples.size() / 2), samples.end());
      modelDepth[model] = samples[samples.size() / 2];
    }
  }
  std::set<uint64_t> snapModels;                            // models shown whole at their newest pose this step
  std::set<std::pair<uint64_t, int32_t>> snapModelPass;     // the same, in one render pass only (a new piece there)
  std::set<uint64_t> heldModels; // models whose screen pieces were held for a camera cut: their casters are held too
  std::map<uint64_t, int> lockState; // round 10: per model, 1 = every moved piece moved with its camera, -1 = not
  uint32_t heldCasterLines = 0;      // round 10: the model log's lines for held shadow casters
  std::vector<int32_t> planned(step.orreBlocks.size(), -1); // a block's plan in g_models before the model rule
  // the log's view of both steps: which models each had, and how many draws each piece had per pass
  std::map<uint64_t, uint32_t> rootsBefore, rootsNow;
  std::map<std::tuple<uint64_t, uint64_t, int32_t>, std::pair<uint32_t, uint32_t>> counts;
  std::vector<bool> beforeMatched(previousBlocks.size(), false);
  if (log.on) {
    for (const auto& b : previousBlocks) {
      if (b.modelPObj != 0 && b.projIsCamera) {
        ++rootsBefore[b.modelRoot];
        auto& n = counts[{b.modelJObj, b.modelPObj, passOf(previous, b)}].first;
        n = std::max(n, b.modelOccurrence + 1);
      }
    }
    for (const auto& b : step.orreBlocks) {
      if (b.modelPObj != 0 && b.projIsCamera) {
        ++rootsNow[b.modelRoot];
        auto& n = counts[{b.modelJObj, b.modelPObj, passOf(step.orreCameras, b)}].second;
        n = std::max(n, b.modelOccurrence + 1);
      }
    }
  }
  uint64_t tagged = 0, untagged = 0, matched = 0, moved = 0, snapped = 0, held = 0, borrowed = 0;
  uint64_t scrollBlended = 0, scrollWrapped = 0, scrollSnapped = 0; // item (b): texture matrices
  uint64_t scrollSeen = 0, scrollChanged = 0; // the model log: matched texture-coordinate matrices, and those changed
  uint64_t scrollUnsteady = 0; // round 9: changed, but not by about the step before's change (an atlas cell): newest
  uint32_t scrollLogLines = 0;
  uint64_t colorBlended = 0, colorHeld = 0; // the colour-data blend (2026-11-18): blocks whose colours blend / not
  for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
    const auto& block = step.orreBlocks[i];
    if (!block.projIsCamera) {
      continue; // drawn with another projection (the battle HUD): held, as in step (c)
    }
    if (block.modelPObj == 0) {
      untagged += block.particle == 0 ? 1 : 0; // particles: item (a), below
      continue;
    }
    ++tagged;
    const int32_t pass = passOf(step.orreCameras, block);
    if (block.camera >= g_plan.size() || block.camera >= step.orreCameras.size()) {
      continue;
    }
    const auto found = before.find(Key{block.modelJObj, block.modelPObj, pass, block.modelOccurrence});
    const OrreSmoothBlock* oldBlock = nullptr;
    bool borrow = false;
    if (found != before.end()) {
      oldBlock = &previousBlocks[found->second];
      beforeMatched[found->second] = true;
    } else if (used(block) == 1u) {
      // the swap rule: a rigid piece new this step (a part switched on) takes the matrix the same joint had in the
      // step before, from another rigid piece on it (the part switched off, often)
      const auto rigid = beforeRigid.find(std::pair{block.modelJObj, pass});
      if (rigid != beforeRigid.end()) {
        oldBlock = &previousBlocks[rigid->second];
        borrow = true;
      }
    }
    if (oldBlock == nullptr) {
      // nothing to blend from. A new model shows its newest pose anyway (all its pieces are new); a new piece of a
      // model that was there must not sit at the newest pose next to blended neighbours: the model snaps whole in
      // that pass. *Per pass (2026-11-17, round 4, after P15's survey):* HSD's shadow pass switches single caster
      // pieces on and off every few steps (P15: Xatu's PObj 814b4b00 during its faint, Nuzleaf's mouth through the
      // whole battle); snapping the visible model for a caster piece would hitch it for nothing, and a caster piece at
      // its newest pose only shifts its part of a soft shadow for one in-between. A jump (below) still snaps every
      // pass, since the same matrices drive the model and its caster.
      const bool modelWasThere = modelsBefore.count({block.modelRoot, pass}) != 0;
      if (history && modelWasThere) {
        snapModelPass.insert({block.modelRoot, pass});
      }
      if (log.on && history) {
        log.line("retrace %u new piece J %llx P %llx draw %u root %llx slots %03x pass %d (its model %s the step "
                 "before%s)",
                 retrace, (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj,
                 block.modelOccurrence, (unsigned long long)block.modelRoot, block.modelSlots, pass,
                 rootsBefore.count(block.modelRoot) ? "was there" : "was not there",
                 modelWasThere ? ": it snaps whole" : "");
      }
      continue; // the newest pose
    }
    const auto& old = *oldBlock;
    if (old.camera >= previous.size()) {
      continue;
    }
    ++matched;
    borrowed += borrow ? 1 : 0;
    // the piece blends only when its camera does (or stood still): across a cut or a reset the newest step is shown.
    // *Added 2026-11-17:* a camera of HSD's offscreen pass (the shadows' cameras, never re-aimed: recording.cpp) has
    // no cut test of its own; its pieces blend when the step has history and both its views are rigid (the carry
    // below needs that), and the jump test still snaps a caster that moved too far.
    const auto& camNew = step.orreCameras[block.camera];
    const auto& camOld = previous[old.camera];
    // ORRE_SMOOTH_TEST_NOCASTERS=1 (a test switch, round 5): offscreen-pass pieces (the shadow casters) never blend
    static const bool noCasters = [] {
      const char* v = std::getenv("ORRE_SMOOTH_TEST_NOCASTERS");
      return v != nullptr && v[0] == '1';
    }();
    if (camNew.renderPass != 0) {
      // *Changed 2026-11-17 (round 10):* a shadow camera whose view is the same in both steps needs no carry (it is
      // the identity), so its casters blend whether or not the view is rigid. (A guess while looking at Raikou's
      // Thunderbolt in P17b, where a patch of ground under it stays at the step rate; the log line below showed no
      // caster held there, so that patch is something else. Kept: it is right, and costs nothing.)
      const bool sameView = std::memcmp(camNew.view, camOld.view, sizeof(camNew.view)) == 0;
      if (noCasters || !history || (!sameView && (!rigid_view(camNew.view) || !rigid_view(camOld.view)))) {
        ++held;
        if (log.on && heldCasterLines++ < 3) {
          log.line("retrace %u caster held J %llx P %llx root %llx pass %d: shadow camera's view %s", retrace,
                   (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj,
                   (unsigned long long)block.modelRoot, pass,
                   !history ? "(no history)" : noCasters ? "(test switch)" : "not rigid and changed");
        }
        continue;
      }
    } else {
      const Cut cut = g_plan[block.camera].cut;
      if (cut != Cut::None && cut != Cut::Unchanged) {
        ++held;
        // *Added 2026-11-17 (round 4):* the model's other passes too. Its shadow casters are drawn by the shadow
        // cameras, which have no cut test, so round 3 blended them while the model itself showed its newest pose:
        // the shadow sat apart from the feet for the in-between after every camera cut (verified, P15 frame 84,900,
        // the trainer's legs right after a shot change).
        heldModels.insert(block.modelRoot);
        continue;
      }
    }
    const uint32_t posAt = block.projOffset + 64;                    // after the projection (4x4)
    const uint32_t nrmAt = posAt + 2 * PnSlots * 48;                 // after the 10 position and 10 texture matrices
    const uint32_t need = nrmAt + PnSlots * 48;
    if (block.bytes < need || old.bytes < need || old.projOffset != block.projOffset) {
      continue;
    }
    const Aff vNew = aff_from_floats(&camNew.view[0][0]);
    const Aff vOld = aff_from_floats(&camOld.view[0][0]);
    // B carries the step before's camera space into the newest's: B x M_old = V_new x W_old (rigid views only; a view
    // with scale was already refused by the camera plan, Cut::NotRigid)
    // round 10: the same view in both steps carries nothing (exactly the identity, rigid or not)
    const Aff carry = std::memcmp(camNew.view, camOld.view, sizeof(camNew.view)) == 0
                          ? Aff{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}}
                          : aff_mul(vNew, rigid_inverse(vOld));
    const auto depthIt = modelDepth.find({block.modelRoot, pass});
    // *Round 5:* the move test only in the screen pass. A shadow camera's distance says nothing about how far a step's
    // move looks (the ride's orthographic shadow cameras sit about 20 units from the bike, which moves 19 a step: it
    // "jumped" there every step and snapped the whole bike); the same model in the screen pass decides, and a jump
    // there snaps the casters too. Turns and non-numbers are tested in every pass (they don't depend on distance).
    const double depth = pass != 0 ? 1e30 : depthIt != modelDepth.end() ? depthIt->second : 1e-3;
    const uint32_t slots = used(block) & (borrow ? 1u : used(old));
    ModelBlend model;
    uint32_t jumpMask = 0;
    int jumpWhy = 0;
    uint32_t r3Jump = 0, r3Blend = 0; // the log: what round 3's rules did with these used slots (the survey)
    double worstMove = 0, worstDot = 1; // the log: the largest move (of the model's depth) and the smallest axis cosine
    double worldMoveMax = 0, relMoveMax = 0; // round 10: the lock of models that move with their camera
    for (uint32_t s = 0; s < PnSlots; ++s) {
      if ((slots & (1u << s)) == 0) {
        continue; // a slot the piece doesn't draw with (round 4)
      }
      const float* posNew = reinterpret_cast<const float*>(step.orreBlockBytes.data() + block.bytesOffset + posAt + s * 48);
      const float* posOld = reinterpret_cast<const float*>(previousBytes.data() + old.bytesOffset + posAt + s * 48);
      const float* nrmOld = reinterpret_cast<const float*>(previousBytes.data() + old.bytesOffset + nrmAt + s * 48);
      const Aff p = aff_mul(carry, aff_from_floats(posOld));
      const Aff n = aff_mul3(carry, aff_from_floats(nrmOld));
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
          model.fromPos[s][r * 4 + c] = static_cast<float>(p[r][c]);
          model.fromNrm[s][r * 4 + c] = static_cast<float>(n[r][c]);
        }
      }
      if (std::memcmp(model.fromPos[s], posNew, 48) == 0) {
        continue; // did not move in the world (often exactly: a still model under a still camera)
      }
      {
        // round 10: how far this matrix moved in the world (carried) and as its camera sees it (raw), for the lock
        // below: a model that moves with its camera
        const double wx = posNew[3] - model.fromPos[s][3], wy = posNew[7] - model.fromPos[s][7],
                     wz = posNew[11] - model.fromPos[s][11];
        const double rx = posNew[3] - posOld[3], ry = posNew[7] - posOld[7], rz = posNew[11] - posOld[11];
        worldMoveMax = std::max(worldMoveMax, std::sqrt(wx * wx + wy * wy + wz * wz));
        relMoveMax = std::max(relMoveMax, std::sqrt(rx * rx + ry * ry + rz * rz));
      }
      if (log.on) {
        // round 3's test: the matrix's own depth, slot by slot
        const double own = std::max(std::abs(static_cast<double>(posNew[11])), std::abs(static_cast<double>(model.fromPos[s][11])));
        (jumped(model.fromPos[s], posNew, own) ? r3Jump : r3Blend) |= 1u << s;
      }
      int why = 0;
      // *Changed 2026-11-17 (round 10; the plan's section 23):* a jump is a jump in the world AND as the camera sees
      // it. On the bike rides the camera follows the bike closely (about 5 units behind, both moving 12-13 units a
      // step): in the world the bike "jumped" over 3/4 of its depth every step, so it snapped whole at its newest
      // place while the camera blended, and every in-between drew it half a step ahead of the camera: a second, bigger
      // bike over the picture (snatcherLeo's "model duplicating" in the bike transitions; the seam sweep flagged 642 of
      // 2,847 in-betweens in his 19:09 sitting, verified). Seen from its camera such a model barely moves (its
      // model-view matrix of the step before, posOld, against the newest), so it blends with the camera, consistent.
      // A model that really teleports moves in both, and still snaps.
      if (jumped(model.fromPos[s], posNew, depth, &why) && jumped(posOld, posNew, depth)) {
        jumpMask |= 1u << s;
        jumpWhy = jumpWhy != 0 ? jumpWhy : why;
        if (log.on) {
          const float* a = model.fromPos[s];
          const double dx = posNew[3] - a[3], dy = posNew[7] - a[7], dz = posNew[11] - a[11];
          worstMove = std::max(worstMove, std::sqrt(dx * dx + dy * dy + dz * dz) / std::max(depth, 1e-3));
          for (int c = 0; c < 3; ++c) {
            const double la = column_length(a, c), lb = column_length(posNew, c);
            double d = 0;
            for (int rr = 0; rr < 3; ++rr) {
              d += static_cast<double>(a[rr * 4 + c]) * posNew[rr * 4 + c];
            }
            worstDot = std::min(worstDot, la > 1e-12 && lb > 1e-12 ? d / (la * lb) : 0.0);
          }
        }
        continue;
      }
      // tiny differences (the carry's rounding under a moving camera) are not worth a write
      bool differs = false;
      for (int k = 0; k < 12 && !differs; ++k) {
        differs = std::abs(model.fromPos[s][k] - posNew[k]) > 1e-5f * (1.f + std::abs(posNew[k]));
      }
      if (differs) {
        model.mask |= 1u << s;
      }
    }
    if (pass == 0 && worldMoveMax > 0) {
      // round 10: the evidence for the lock (plan section 23): in the world the piece moved far for its distance (over
      // half the model's depth in one step: the bike, 12-13 units at about 5; Wes walking with a following field
      // camera moves 1-2 units at 5-10 and keeps blending), but as its camera sees it under a quarter of that
      auto& st = lockState[block.modelRoot];
      const bool withCamera = worldMoveMax > 0.5 * depth && relMoveMax < 0.25 * worldMoveMax;
      st = !withCamera ? -1 : st == 0 ? 1 : st;
    }
    if (log.on && r3Jump != 0 && r3Blend != 0 && camNew.renderPass == 0) {
      // the survey: a piece round 3 tore (some used slots snapped alone, the others blended)
      log.line("retrace %u r3tear J %llx P %llx root %llx: round 3 snapped %03x and blended %03x; round 4 %s",
               retrace, (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj,
               (unsigned long long)block.modelRoot, r3Jump, r3Blend,
               jumpMask != 0 ? "snaps the model whole" : "blends the whole piece");
    }
    if (jumpMask != 0) {
      snapModels.insert(block.modelRoot); // round 4: never a part of a model, always the whole of it
      if (log.on) {
        // "moving" = the slots that moved without jumping: round 3 blended those and snapped the jumped ones alone
        // (a tear whenever both are non-empty); now the model snaps whole
        log.line("retrace %u jump J %llx P %llx draw %u root %llx slots used %03x: jumped %03x, moving %03x (%s; move "
                 "%.2f of the model's depth %.2f, axis cosine %.2f), pass %d%s: the model snaps whole",
                 retrace, (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj,
                 block.modelOccurrence, (unsigned long long)block.modelRoot, slots, jumpMask, model.mask,
                 jumpWhy == 1 ? "moved" : jumpWhy == 2 ? "turned over 90 degrees" : "not a number", worstMove, depth,
                 worstDot, pass, borrow ? " (borrowed from the same joint)" : "");
      }
      continue;
    }
    if (log.on && borrow) {
      log.line("retrace %u borrow J %llx P %llx draw %u root %llx pass %d: a rigid piece new this step blends from its "
               "joint's matrix of the step before",
               retrace, (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj, block.modelOccurrence,
               (unsigned long long)block.modelRoot, pass);
    }
    // ---- Item (b), scrolling textures (2026-11-17; docs/port/frame-smoothing-plan.md section 19). ----
    // Water, clouds, waterfalls, conveyor-like surfaces and glows scroll their texture with an animated HSD TObj
    // matrix (translate, rotate, scale; TObjSetupMtx loads it per draw). Texture matrices were held at the newest step,
    // so a scrolling surface moved in steps while the camera and the models around it blended. Now a matched piece's
    // texture matrices fed by texture coordinates blend from the step before's, number by number (a plain blend: a
    // texture matrix has no axes to keep the length of). Across a wrap (a scroll animation running from 1 back to 0)
    // the step before's offset is first shifted by whole texture repeats, which shows the same picture when the
    // textures repeat along that axis (whole pairs of repeats when they mirror), so the blend goes the short way; a
    // matrix that still changed a lot in one step (an offset over half a texture, or a scale or turn number by over
    // half: a swap, not a scroll; a guess at the threshold) shows the newest step's, as before.
    {
      // both blocks hold all twenty post matrices (fill_uniform writes them whole) at their recorded places
      const bool both = block.postMtxOffset != 0 && old.postMtxOffset != 0 &&
                        block.bytes >= block.postMtxOffset + PtSlots * 48 && old.bytes >= old.postMtxOffset + PtSlots * 48;
      const uint32_t uv = both ? block.texUvMask & old.texUvMask : 0u;
      for (uint32_t k = 0; k < PtSlots && uv != 0; ++k) {
        const uint32_t bit = 1u << k;
        if ((uv & bit) == 0) {
          continue;
        }
        const float* tNew =
            reinterpret_cast<const float*>(step.orreBlockBytes.data() + block.bytesOffset + block.postMtxOffset + k * 48);
        const float* tOld =
            reinterpret_cast<const float*>(previousBytes.data() + old.bytesOffset + old.postMtxOffset + k * 48);
        ++scrollSeen;
        if (std::memcmp(tNew, tOld, 48) == 0) {
          continue;
        }
        ++scrollChanged;
        float from[12];
        std::memcpy(from, tOld, sizeof(from));
        bool ok = true, wrapped = false;
        for (int r = 0; r < 2 && ok; ++r) {
          // row 0 makes s, row 1 makes t; column 3 is the offset
          const bool repeat = ((r == 0 ? block.texRepeatS & old.texRepeatS : block.texRepeatT & old.texRepeatT) & bit) != 0;
          const bool pairs =
              ((r == 0 ? block.texWholePairsS & old.texWholePairsS : block.texWholePairsT & old.texWholePairsT) & bit) != 0;
          const double delta = static_cast<double>(tNew[r * 4 + 3]) - from[r * 4 + 3];
          double shift = 0;
          if (repeat) {
            shift = std::round(delta);
          } else if (pairs) {
            shift = 2.0 * std::round(delta / 2.0);
          }
          if (shift != 0) {
            from[r * 4 + 3] = static_cast<float>(from[r * 4 + 3] + shift);
            wrapped = true;
          }
          ok = std::abs(static_cast<double>(tNew[r * 4 + 3]) - from[r * 4 + 3]) <= (pairs && !repeat ? 1.0 : 0.5);
        }
        for (int n = 0; n < 12 && ok; ++n) {
          ok = std::isfinite(tNew[n]) && std::isfinite(from[n]);
          if (ok && n % 4 != 3) {
            const double a = std::abs(static_cast<double>(tNew[n])), b = std::abs(static_cast<double>(from[n]));
            ok = std::abs(static_cast<double>(tNew[n]) - from[n]) <= 0.5 * std::max(a, b) + 1e-6;
          }
        }
        // ---- Round 9 (2026-11-17): only steady motion blends; a jump to another picture cell snaps. ----
        // snatcherLeo's hand check of items (a)-(d): the audience in Realgam Tower's stands flickered, rows of half figures (legs
        // of one sprite under the top of another). The crowd is a picture atlas: its texture matrix jumps from one cell
        // to the next now and then, and item (b) blended those jumps, so in-betweens showed the seam between two cells.
        // A scroll is steady: its matrix moves by about the same amount every step. So a change blends only when the
        // step before changed it by about the same (each number's change within a quarter of the larger of the two,
        // after the same whole-repeat shift), which needs two steps of history; an atlas cell change (still, then a
        // jump) and the first step of any motion show the newest, as before item (b).
        double nowMove = 0, prevMove = 0; // the log: the offsets' change now and the step before
        bool steady = false;
        if (ok) {
          const auto found2 = before2.find(Key{block.modelJObj, block.modelPObj, pass, block.modelOccurrence});
          const OrreSmoothBlock* old2 = found2 != before2.end() ? &twoBack.blocks[found2->second] : nullptr;
          if (old2 != nullptr && old2->postMtxOffset != 0 && (old2->texUvMask & bit) != 0 &&
              old2->bytes >= old2->postMtxOffset + PtSlots * 48) {
            const float* tOld2 =
                reinterpret_cast<const float*>(twoBack.bytes.data() + old2->bytesOffset + old2->postMtxOffset + k * 48);
            float from2[12];
            std::memcpy(from2, tOld2, sizeof(from2));
            for (int r = 0; r < 2; ++r) {
              const bool repeat = ((r == 0 ? old.texRepeatS & old2->texRepeatS : old.texRepeatT & old2->texRepeatT) & bit) != 0;
              const bool pairs =
                  ((r == 0 ? old.texWholePairsS & old2->texWholePairsS : old.texWholePairsT & old2->texWholePairsT) & bit) != 0;
              const double delta = static_cast<double>(tOld[r * 4 + 3]) - from2[r * 4 + 3];
              const double shift = repeat ? std::round(delta) : pairs ? 2.0 * std::round(delta / 2.0) : 0.0;
              from2[r * 4 + 3] = static_cast<float>(from2[r * 4 + 3] + shift);
            }
            steady = true;
            for (int n = 0; n < 12 && steady; ++n) {
              const double dNow = static_cast<double>(tNew[n]) - from[n];
              const double dPrev = static_cast<double>(tOld[n]) - from2[n];
              steady = std::isfinite(from2[n]) &&
                       std::abs(dNow - dPrev) <= 0.25 * std::max(std::abs(dNow), std::abs(dPrev)) + 1e-5;
            }
            for (int r = 0; r < 2; ++r) {
              nowMove = std::max(nowMove, std::abs(static_cast<double>(tNew[r * 4 + 3]) - from[r * 4 + 3]));
              prevMove = std::max(prevMove, std::abs(static_cast<double>(tOld[r * 4 + 3]) - from2[r * 4 + 3]));
            }
            // and small: an atlas stepping through its cells at one cell a step would look steady too. Measured scrolls
            // move 0.003-0.013 of a texture a step (P16's waterfall, room 0x24); an eighth a step (four textures a
            // second at 30 steps) is far past any scroll seen, while atlas cells are a quarter or a half (the crowd: 0.5)
            steady = steady && nowMove < 0.125;
          }
        }
        if (log.on && scrollLogLines < 8) {
          ++scrollLogLines;
          log.line("retrace %u scroll J %llx P %llx post %u: offsets s %.4f t %.4f (the step before %.4f %.4f), "
                   "change now %.4f, the step before %.4f: %s",
                   retrace, (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj, k, tNew[3],
                   tNew[7], tOld[3], tOld[7], nowMove, prevMove,
                   !ok ? "changed too much" : steady ? "steady: blends" : "not steady: newest");
        }
        if (ok && !steady) {
          ++scrollUnsteady;
          continue;
        }
        if (!ok) {
          ++scrollSnapped;
          continue;
        }
        std::memcpy(model.fromTex[k], from, sizeof(from));
        model.texMask |= bit;
        scrollWrapped += wrapped ? 1 : 0;
      }
    }
    // ---- The colour-data blend (2026-11-18; docs/port/frame-smoothing-plan.md section 26). ----
    // The census of all 25 the reference playthrough sittings (section 25) found battle moves' flashes and fades held at the step rate on
    // models whose matrices blended: the game changes a piece's colours (HSD's material colour animations, the battle's
    // flash and tint colours, the lights' colours) through the TEV colour registers, the channel ambient / material
    // colours, the konstant colours and the light colours, all in the same uniform block as the matrices, and those
    // were held at the newest step. Now a matched piece's colour floats blend from the step before's too, number by
    // number, at the piece's own fraction. Only between blocks of the same colour layout (the same registers, channels
    // and konstant colours written, at the same places); of the lights only their colours (a light's position and
    // direction are in its step's camera space, and moving lights are rare: guess), never the light masks (integers).
    if (block.colorEnd != 0 && old.colorEnd != 0 && block.colorAt == old.colorAt && block.colorEnd == old.colorEnd &&
        block.lightAt == old.lightAt && block.colorLayout == old.colorLayout && block.colorEnd <= block.bytes &&
        old.colorEnd <= old.bytes && (block.colorEnd - block.colorAt) % 4 == 0 &&
        (block.colorEnd - block.colorAt) / 4 <= ColorFloatsMax) {
      const uint32_t n = (block.colorEnd - block.colorAt) / 4;
      const auto* cNew = step.orreBlockBytes.data() + block.bytesOffset + block.colorAt;
      const auto* cOld = previousBytes.data() + old.bytesOffset + old.colorAt;
      if (std::memcmp(cNew, cOld, n * 4) != 0) {
        bool any = false, bad = false;
        for (uint32_t k = 0; k < n; ++k) {
          const uint32_t at = block.colorAt + k * 4;
          float vNew, vOld;
          std::memcpy(&vNew, cNew + k * 4, 4);
          std::memcpy(&vOld, cOld + k * 4, 4);
          bool blendable = true;
          if (block.lightAt != 0 && at >= block.lightAt) {
            const uint32_t inLights = at - block.lightAt;
            // eight lights of 80 bytes (position, direction, colour, cosine and distance attenuation, 16 each), then
            // the four masks: only bytes 32-47 of a light (its colour)
            blendable = inLights < 640 && inLights % 80 >= 32 && inLights % 80 < 48;
          }
          if (!blendable || std::memcmp(&vNew, &vOld, 4) == 0) {
            model.fromColor[k] = vNew;
            continue;
          }
          if (!std::isfinite(vNew) || !std::isfinite(vOld)) {
            bad = true;
            break;
          }
          model.fromColor[k] = vOld;
          any = true;
        }
        if (bad) {
          ++colorHeld;
        } else if (any) {
          model.colorFloats = n; // counted where the plan is kept (a snapped or held model drops it)
        }
      }
    } else if (block.colorEnd != 0 && old.colorEnd != 0 && block.colorEnd - block.colorAt == old.colorEnd - old.colorAt &&
               block.colorEnd <= block.bytes && old.colorEnd <= old.bytes &&
               std::memcmp(step.orreBlockBytes.data() + block.bytesOffset + block.colorAt,
                           previousBytes.data() + old.bytesOffset + old.colorAt, block.colorEnd - block.colorAt) != 0) {
      ++colorHeld; // changed, but another layout: the newest (counted for the census's question)
    }
    if (model.mask != 0 || model.texMask != 0 || model.colorFloats != 0) {
      planned[i] = static_cast<int32_t>(g_models.size());
      g_models.push_back(model);
      if (log.on && (block.texPosMask | block.texNrmMask) != 0) {
        log.line("retrace %u blend J %llx P %llx root %llx slots %03x moved %03x pass %d: texture matrices from "
                 "position %03x, from normal %03x",
                 retrace, (unsigned long long)block.modelJObj, (unsigned long long)block.modelPObj,
                 (unsigned long long)block.modelRoot, block.modelSlots, model.mask, pass, block.texPosMask,
                 block.texNrmMask);
      }
    }
  }
  // the model rule: a model with anything that jumped, or a new piece with nothing to blend from, shows every piece at
  // its newest pose in this step's in-betweens
  // ---- Round 10 (2026-11-17; the plan's section 23): a model that moves with its camera keeps its place on screen. ----
  // On the bike rides the camera follows the bike a few units behind; bike, riders and exhaust pipes move with it
  // (12-13 units a step in the world, almost nothing as the camera sees them). Blending such a model's world pose and,
  // separately, the camera's eye and target does not give back the same screen place: the two blends are not the same
  // curve, and close to the camera the gap is large on screen (the seam sweep, snatcherLeo's 19:09 sitting: an exhaust pipe a
  // whole pipe-width off in the in-between while both steps had it in the same place, frame 6,327; verified). Such a
  // model is drawn exactly as the step drew it (no camera re-aim, no blend): it stays where it is on screen, and its
  // own motion relative to the camera (a rider's animation) shows at the step rate. Its world moved over half its
  // depth in one step and every moved matrix moved under a quarter of that as the camera sees it; screen pass only.
  // Held this way it stays consistent with the world: the camera follows it, so the ground under its screen place in
  // an in-between is the ground it was over half a step before, where its blended world pose would be (its shadow,
  // cast by its blended casters in pass 3, stays under it). *Tried the same day and dropped:* blending it in its
  // camera's space (the step before's model-view matrices blended with the newest, then put under the re-aim's
  // inverse) left the frame 6,327 pipe exactly as far off as before (verified, the sweep's exe34), so the hold stays.
  g_blockLocked.assign(step.orreBlocks.size(), 0);
  uint64_t locked = 0;
  for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
    const auto& block = step.orreBlocks[i];
    if (block.modelPObj == 0 || !block.projIsCamera || passOf(step.orreCameras, block) != 0) {
      continue;
    }
    const auto it = lockState.find(block.modelRoot);
    if (it != lockState.end() && it->second == 1 && heldModels.count(block.modelRoot) == 0) {
      g_blockLocked[i] = 1;
      ++locked;
    }
  }
  if (log.on && locked != 0) {
    log.line("retrace %u locked to the camera: %llu blocks of %zu models", retrace, (unsigned long long)locked,
             static_cast<size_t>(std::count_if(lockState.begin(), lockState.end(),
                                               [](const auto& kv) { return kv.second == 1; })));
  }
  g_stats.lockedBlocks.fetch_add(locked, std::memory_order_relaxed);
  for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
    const auto& block = step.orreBlocks[i];
    if (block.modelPObj == 0 || !block.projIsCamera || g_blockLocked[i] != 0) {
      continue;
    }
    if (snapModels.count(block.modelRoot) != 0 ||
        snapModelPass.count({block.modelRoot, passOf(step.orreCameras, block)}) != 0) {
      ++snapped;
      continue;
    }
    if (planned[i] >= 0 && heldModels.count(block.modelRoot) != 0) {
      ++held; // a caster (or another piece) of a model held for a camera cut: held with it
      continue;
    }
    if (planned[i] >= 0) {
      const auto& m = g_models[static_cast<size_t>(planned[i])];
      moved += m.mask != 0 ? 1 : 0;
      scrollBlended += static_cast<uint64_t>(std::popcount(m.texMask)); // item (b)
      colorBlended += m.colorFloats != 0 ? 1u : 0u;                     // the colour-data blend
      g_blockModel[i] = planned[i];
    }
  }
  // ---- The capture prototype: Psychic's warp (2026-11-18; docs/port/frame-smoothing-plan.md section 29). ----
  // On by default since 2026-11-18 (first behind ORRE_SMOOTH_CAPTURE=1; below). Psychic's wavy background is not a picture
  // from an earlier step: each step copies its own picture (EFB copies inside the step, which the replay already redoes
  // from the in-between's scene) and draws it back through a warp, one draw with an indirect texture stage under the
  // battle camera (verified, the draw log at P17b 8,008-8,010: a quad whose position matrix shrinks, whose texture
  // matrix offset and indirect matrix change every step). That draw is no model piece, so its matrices stayed at the
  // newest step and the warp's phase moved at the step rate while everything around it blended. Now such a draw (no
  // model, particle or 2D piece; an indirect stage; its camera's own projection) is matched with the step before's by its
  // camera and its number among those draws, and its position matrix (carried into this step's camera space, as the
  // models'), the texture matrices on its texture coordinates and its indirect matrices blend. The warp's shape is the
  // game's own in every step; only the in-betweens get the poses and phases between.
  // *Changed 2026-11-18 (snatcherLeo's read of the demo, the project's decision):* on by default as part of frame smoothing (no
  // Settings entry; off whenever smoothing is off, as this whole plan is). ORRE_SMOOTH_TEST_NOWARP=1 or
  // ORRE_SMOOTH_CAPTURE=0 (test switches) turn it off, for before / after pictures from one build.
  static const bool captureProto = [] {
    const char* noWarp = std::getenv("ORRE_SMOOTH_TEST_NOWARP");
    const char* capture = std::getenv("ORRE_SMOOTH_CAPTURE");
    return !(noWarp != nullptr && noWarp[0] == '1') && !(capture != nullptr && capture[0] == '0');
  }();
  uint64_t warpSeen = 0, warpBlended = 0;
  if (captureProto && history) {
    const auto warp = [](const OrreSmoothBlock& b) {
      return b.modelPObj == 0 && b.particle == 0 && b.piece2dKind == 0 && b.projIsCamera && b.indStages != 0 &&
             b.indAt != 0 && b.piece2dSlot < PnSlots && b.bytes >= b.projOffset + 64 + 3 * PnSlots * 48;
    };
    using WarpKey = std::tuple<uint64_t, uint32_t, uint32_t>; // camera id, its occurrence, the draw's number under it
    std::map<WarpKey, uint32_t> warpBefore;
    std::map<std::pair<uint64_t, uint32_t>, uint32_t> counter;
    for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
      const auto& b = previousBlocks[i];
      if (warp(b) && b.camera < previous.size()) {
        const auto& cam = previous[b.camera];
        warpBefore.emplace(WarpKey{cam.id, cam.occurrence, counter[{cam.id, cam.occurrence}]++}, i);
      }
    }
    counter.clear();
    for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
      const auto& block = step.orreBlocks[i];
      if (!warp(block) || block.camera >= step.orreCameras.size() || block.camera >= g_plan.size()) {
        continue;
      }
      ++warpSeen;
      const auto& camNew = step.orreCameras[block.camera];
      const auto found = warpBefore.find(WarpKey{camNew.id, camNew.occurrence, counter[{camNew.id, camNew.occurrence}]++});
      if (found == warpBefore.end() || g_blockModel[i] >= 0) {
        continue;
      }
      const auto& old = previousBlocks[found->second];
      const Cut cut = g_plan[block.camera].cut;
      if ((cut != Cut::None && cut != Cut::Unchanged) || old.camera >= previous.size() ||
          old.projOffset != block.projOffset || old.indAt != block.indAt || old.piece2dSlot != block.piece2dSlot ||
          old.texUvMtxMask != block.texUvMtxMask || !old.projIsCamera) {
        continue;
      }
      const auto& camOld = previous[old.camera];
      const bool sameView = std::memcmp(camNew.view, camOld.view, sizeof(camNew.view)) == 0;
      if (!sameView && (!rigid_view(camNew.view) || !rigid_view(camOld.view))) {
        continue;
      }
      const Aff carry = sameView ? Aff{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}}
                                 : aff_mul(aff_from_floats(&camNew.view[0][0]),
                                           rigid_inverse(aff_from_floats(&camOld.view[0][0])));
      const auto* now = step.orreBlockBytes.data() + block.bytesOffset;
      const auto* was = previousBytes.data() + old.bytesOffset;
      ModelBlend m;
      const uint32_t s = block.piece2dSlot;
      const uint32_t posAt = block.projOffset + 64, texAt = posAt + PnSlots * 48, nrmAt = posAt + 2 * PnSlots * 48;
      {
        const float* posNew = reinterpret_cast<const float*>(now + posAt + s * 48);
        const Aff p = aff_mul(carry, aff_from_floats(reinterpret_cast<const float*>(was + posAt + s * 48)));
        const Aff n = aff_mul3(carry, aff_from_floats(reinterpret_cast<const float*>(was + nrmAt + s * 48)));
        for (int r = 0; r < 3; ++r) {
          for (int c = 0; c < 4; ++c) {
            m.fromPos[s][r * 4 + c] = static_cast<float>(p[r][c]);
            m.fromNrm[s][r * 4 + c] = static_cast<float>(n[r][c]);
          }
        }
        if (jumped(m.fromPos[s], posNew, 1e30)) {
          continue; // turned over 90 degrees, or not a number: the newest, as before
        }
        if (std::memcmp(m.fromPos[s], posNew, 48) != 0) {
          m.mask |= 1u << s;
        }
      }
      // the texture matrices on its texture coordinates: a plain number-by-number blend unless one moved by over half
      // a texture in one step (a jump, not a phase moving)
      for (uint32_t k = 0; k < PnSlots; ++k) {
        if ((block.texUvMtxMask & (1u << k)) == 0) {
          continue;
        }
        const float* tNew = reinterpret_cast<const float*>(now + texAt + k * 48);
        const float* tOld = reinterpret_cast<const float*>(was + texAt + k * 48);
        if (std::memcmp(tNew, tOld, 48) == 0) {
          continue;
        }
        bool ok = true;
        for (int n = 0; n < 12 && ok; ++n) {
          ok = std::isfinite(tNew[n]) && std::isfinite(tOld[n]) &&
               (n % 4 != 3 || std::abs(static_cast<double>(tNew[n]) - tOld[n]) <= 0.5);
        }
        if (ok) {
          std::memcpy(m.fromUvMtx[k], tOld, 48);
          m.uvMtxMask |= 1u << k;
        }
      }
      // the indirect matrices (the warp's own strength and direction)
      if (std::memcmp(now + block.indAt, was + old.indAt, 96) != 0) {
        bool ok = true;
        std::memcpy(m.fromInd, was + old.indAt, 96);
        for (int n = 0; n < 24 && ok; ++n) {
          ok = std::isfinite(m.fromInd[n]);
        }
        m.indBlend = ok;
      }
      if (m.mask != 0 || m.uvMtxMask != 0 || m.indBlend) {
        g_blockModel[i] = static_cast<int32_t>(g_models.size());
        g_models.push_back(m);
        ++warpBlended;
      }
    }
    if (log.on && warpSeen != 0) {
      log.line("retrace %u the capture prototype: warp draws %llu, blended %llu", retrace,
               (unsigned long long)warpSeen, (unsigned long long)warpBlended);
    }
  }
  // ---- Item (c), menu models: the two-step history (2026-11-17; docs/port/frame-smoothing-plan.md section 20). ----
  // The summary page's Pokemon (and the other menu models drawn by a camera of HSD's render pass 3, not the screen's)
  // is posed on every other step: the page runs 60 steps a second and the menu model holds its pose on half frames
  // (port gap 78; verified, P01 frames 8,840-8,860: its pieces change on even steps only). Blending step to step then
  // moves it over one step and holds it over the next: motion at half the picture rate, in jerks. A model like that
  // blends over two steps instead, one step later: in the step it changed, from its pose of the step before to half way
  // to the new pose; in the step it held, from its pose two steps back the rest of the way. The motion is even, at the
  // cost of one more step of delay for that model only (a menu doesn't feel it). Only models with no piece in the
  // screen pass this step (a field model's shadow caster follows its model's own blend), only with two steps of history
  // and the same pieces in all three steps; the whole model takes the same case.
  uint64_t twoStepBlocks = 0, twoStepModels = 0;
  g_twoStepAtOne = false;
  if (twoBackOk && !twoBack.blocks.empty()) {
    std::set<uint64_t> onScreen;
    std::map<uint64_t, std::vector<uint32_t>> menuBlocks; // root -> its blocks this step
    for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
      const auto& b = step.orreBlocks[i];
      if (b.modelPObj == 0 || !b.projIsCamera) {
        continue;
      }
      if (passOf(step.orreCameras, b) == 0) {
        onScreen.insert(b.modelRoot);
      } else {
        menuBlocks[b.modelRoot].push_back(i);
      }
    }
    const uint32_t posOff = 64, nrmOff = 64 + 2 * PnSlots * 48;
    // a block's used position matrices carried into `to`'s camera space; false when not comparable
    const auto carried = [&](const OrreSmoothBlock& from, const std::vector<OrreSmoothCamera>& fromCams,
                             const std::vector<uint8_t>& fromBytes, const OrreSmoothCamera& to, uint32_t s,
                             float* pos, float* nrm) {
      if (from.camera >= fromCams.size() || from.bytes < from.projOffset + nrmOff + PnSlots * 48) {
        return false;
      }
      const auto& cam = fromCams[from.camera];
      if (!rigid_view(cam.view) || !rigid_view(to.view)) {
        return false;
      }
      const Aff carry = aff_mul(aff_from_floats(&to.view[0][0]), rigid_inverse(aff_from_floats(&cam.view[0][0])));
      const auto* base = fromBytes.data() + from.bytesOffset + from.projOffset;
      const Aff p = aff_mul(carry, aff_from_floats(reinterpret_cast<const float*>(base + posOff + s * 48)));
      const Aff n = aff_mul3(carry, aff_from_floats(reinterpret_cast<const float*>(base + nrmOff + s * 48)));
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
          pos[r * 4 + c] = static_cast<float>(p[r][c]);
          nrm[r * 4 + c] = static_cast<float>(n[r][c]);
        }
      }
      return true;
    };
    const auto differs = [](const float* a, const float* b) {
      for (int k = 0; k < 12; ++k) {
        if (std::abs(a[k] - b[k]) > 1e-5f * (1.f + std::abs(b[k]))) {
          return true;
        }
      }
      return false;
    };
    for (const auto& [root, blocks] : menuBlocks) {
      if (onScreen.count(root) != 0 || snapModels.count(root) != 0 || heldModels.count(root) != 0) {
        continue;
      }
      // per block: its partners one and two steps back; the model's cadence from all its used slots
      bool complete = true, changedNow = false, changedBefore = false;
      std::vector<std::pair<uint32_t, uint32_t>> partners; // (index one back, index two back)
      for (uint32_t i : blocks) {
        const auto& b = step.orreBlocks[i];
        const int32_t pass = passOf(step.orreCameras, b);
        const Key key{b.modelJObj, b.modelPObj, pass, b.modelOccurrence};
        const auto one = before.find(key);
        const auto two = before2.find(key);
        if (one == before.end() || two == before2.end() || b.camera >= step.orreCameras.size() ||
            b.bytes < b.projOffset + nrmOff + PnSlots * 48) {
          complete = false;
          break;
        }
        partners.emplace_back(one->second, two->second);
        const auto& cam = step.orreCameras[b.camera];
        const auto* now = step.orreBlockBytes.data() + b.bytesOffset + b.projOffset;
        for (uint32_t s = 0; s < PnSlots && complete; ++s) {
          if ((used(b) & (1u << s)) == 0) {
            continue;
          }
          float p1[12], n1[12], p2[12], n2[12];
          if (!carried(previousBlocks[one->second], previous, previousBytes, cam, s, p1, n1) ||
              !carried(twoBack.blocks[two->second], twoBack.cameras, twoBack.bytes, cam, s, p2, n2)) {
            complete = false;
            break;
          }
          changedNow = changedNow || differs(p1, reinterpret_cast<const float*>(now + posOff + s * 48));
          changedBefore = changedBefore || differs(p2, p1);
        }
        if (!complete) {
          break;
        }
      }
      if (!complete || changedNow == changedBefore) {
        continue; // moving every step (the ordinary blend), still, or not comparable
      }
      // the case: changed now after a hold (half way, from the step before), or held now after a change (from two
      // steps back, the second half)
      bool jump = false;
      std::vector<ModelBlend> plans(blocks.size());
      for (size_t k = 0; k < blocks.size() && !jump; ++k) {
        const auto& b = step.orreBlocks[blocks[k]];
        const auto& cam = step.orreCameras[b.camera];
        const auto* now = step.orreBlockBytes.data() + b.bytesOffset + b.projOffset;
        auto& m = plans[k];
        m.fScale = 0.5;
        m.fBias = changedNow ? 0.0 : 0.5;
        for (uint32_t s = 0; s < PnSlots; ++s) {
          if ((used(b) & (1u << s)) == 0) {
            continue;
          }
          const bool fromOne = changedNow;
          const auto& src = fromOne ? previousBlocks[partners[k].first] : twoBack.blocks[partners[k].second];
          carried(src, fromOne ? previous : twoBack.cameras, fromOne ? previousBytes : twoBack.bytes, cam, s,
                  m.fromPos[s], m.fromNrm[s]);
          const float* posNew = reinterpret_cast<const float*>(now + posOff + s * 48);
          if (jumped(m.fromPos[s], posNew, 1e30)) { // turns and non-numbers (a menu camera's depth says nothing)
            jump = true;
            break;
          }
          if (differs(m.fromPos[s], posNew)) {
            m.mask |= 1u << s;
          }
        }
      }
      if (jump) {
        continue;
      }
      for (size_t k = 0; k < blocks.size(); ++k) {
        if (plans[k].mask == 0) {
          // nothing of this piece moved over the two steps: no blend; and drop a step-to-step plan it may have had
          g_blockModel[blocks[k]] = -1;
          continue;
        }
        g_blockModel[blocks[k]] = static_cast<int32_t>(g_models.size());
        g_models.push_back(plans[k]);
        ++twoStepBlocks;
        g_twoStepAtOne = g_twoStepAtOne || changedNow; // the first half ends half way, not at the step's own pose
      }
      ++twoStepModels;
      if (log.on) {
        log.line("retrace %u two-step root %llx: %s, %zu blocks", retrace, (unsigned long long)root,
                 changedNow ? "changed after a hold: the first half" : "held after a change: the second half",
                 blocks.size());
      }
    }
  }
  g_stats.twoStepBlocks.fetch_add(twoStepBlocks, std::memory_order_relaxed);
  g_stats.colorBlended.fetch_add(colorBlended, std::memory_order_relaxed); // the colour-data blend
  g_stats.colorHeld.fetch_add(colorHeld, std::memory_order_relaxed);
  g_stats.twoStepModels.fetch_add(twoStepModels, std::memory_order_relaxed);
  // ---- Round 11, 2D at 120 (2026-11-17; docs/port/frame-smoothing-plan.md section 24). ----
  // The game names its HP and EXP bars' fills (OrreHook_2DPiece, played back from GSgfx's request buffer): kind (the
  // window sprite's kind), variant (the picture: green, yellow, red), id (the window) and value (the width the step
  // drew, whole pixels). A piece is matched with the step before's by kind, id and draw number, never by draw order
  // (Count-Sheep's port matched 2D by order and its EXP bar drifted). When its value changed and its picture didn't,
  // the in-between draws it scaled across x about its left end by (the blended width / the drawn width), so its right
  // end moves smoothly from the step before's width to this step's; at the step itself it is exactly the game's
  // width (no drift). New or gone pieces, a changed picture (a colour change), a moved left end or an empty bar show
  // the newest step. ORRE_SMOOTH_2D=0 (pc_smooth.c) leaves the hook unset: no tags, nothing of this runs.
  g_2d.clear();
  g_block2d.assign(step.orreBlocks.size(), -1);
  uint64_t p2Tagged = 0, p2Blended = 0, p2New = 0, p2Swapped = 0, p2NoSpan = 0;
  {
    struct K2 {
      uint32_t kind, id, occ;
      bool operator<(const K2& o) const { return std::tie(kind, id, occ) < std::tie(o.kind, o.id, o.occ); }
    };
    std::map<K2, uint32_t> before2d;
    for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
      const auto& b = previousBlocks[i];
      if (b.piece2dKind != 0 && (b.piece2dKind & GX_AURORA_ORRE_2D_VERTICES) == 0 && b.piece2dXValid) {
        before2d.emplace(K2{b.piece2dKind, b.piece2dId, b.piece2dOccurrence}, i);
      }
    }
    for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
      const auto& b = step.orreBlocks[i];
      if (b.piece2dKind == 0 || (b.piece2dKind & GX_AURORA_ORRE_2D_VERTICES) != 0) {
        continue; // vertex-mode pieces (the pause menu) blend below, by their vertices and matrices
      }
      if (!b.piece2dXValid) {
        ++p2NoSpan; // named, but its left end is unknown: shown as the step drew it
        continue;
      }
      ++p2Tagged;
      const auto it = before2d.find(K2{b.piece2dKind, b.piece2dId, b.piece2dOccurrence});
      if (!history || it == before2d.end()) {
        ++p2New;
        continue;
      }
      const auto& o = previousBlocks[it->second];
      // the EXP bar (kinds 0x535 / 0x53D, 0x537 / 0x53F) only ever fills: a shorter bar is the level-up's restart from
      // empty, never a drain, so it shows the newest (blending it would sweep the bar backwards across one picture)
      const uint32_t kindLow = b.piece2dKind & 0xFFFFu;
      const bool expBar = kindLow == 0x535 || kindLow == 0x53D || kindLow == 0x537 || kindLow == 0x53F;
      if (o.piece2dVariant != b.piece2dVariant || o.piece2dMinX != b.piece2dMinX ||
          (expBar && b.piece2dValue < o.piece2dValue)) {
        ++p2Swapped;
        continue;
      }
      if (o.piece2dValue == b.piece2dValue || !(b.piece2dValue > 0.f) || !(o.piece2dValue >= 0.f) ||
          b.piece2dSlot >= PnSlots || b.bytes < b.projOffset + 64 + PnSlots * 48) {
        continue;
      }
      g_block2d[i] = static_cast<int32_t>(g_2d.size());
      g_2d.push_back({o.piece2dValue, b.piece2dValue, b.piece2dMinX, b.piece2dSlot});
      ++p2Blended;
      if (log.on) {
        log.line("retrace %u 2D piece kind %x id %08x draw %u: %.0f -> %.0f (left end %.1f, slot %u)", retrace,
                 b.piece2dKind, b.piece2dId, b.piece2dOccurrence, o.piece2dValue, b.piece2dValue, b.piece2dMinX,
                 b.piece2dSlot);
      }
    }
  }
  {
    constexpr auto r = std::memory_order_relaxed;
    g_stats.p2Tagged.fetch_add(p2Tagged, r);
    g_stats.p2Blended.fetch_add(p2Blended, r);
    g_stats.p2New.fetch_add(p2New, r);
    g_stats.p2Swapped.fetch_add(p2Swapped, r);
    g_stats.p2NoSpan.fetch_add(p2NoSpan, r);
  }
  // ---- Round 11, 2D at 120, vertex mode (2026-11-17; frame-smoothing-plan.md section 24): the pause menu. ----
  // The game names each pause-menu sprite (menuDaemon's draw loop, menu_r50_80102014_prefix.c on the PC: the rail
  // pieces and buttons, with their labels) and the
  // menu backdrop's darkening (menu_offscreen.c). They move in two ways: a sequence moves or fades a sprite (its
  // vertices' positions and colours, recomputed every step: win_sprite.c), and a window slides by its sprite camera (the
  // position matrix). Each piece is matched with the step before's by kind, id and draw number (never by draw order),
  // and each of its draws with the same-numbered draw: when the draw's vertex layout and size are the same and nothing
  // but the positions and the first colour changed (texture coordinates differ: another picture or another glyph, shown
  // as the newest), an in-between draws positions and colours blended; a block whose position matrix changed draws the
  // matrix blended. Everything else in the piece is drawn as the step drew it.
  g_2dMtx.clear();
  g_block2dMtx.assign(step.orreBlocks.size(), -1);
  g_2dVtx.clear();
  uint64_t p2VtxDraws = 0, p2VtxSnapped = 0, p2MtxBlocks = 0;
  bool chunksAdjacent = false; // (3b): the step before's chunks are the step just before (the corner pass below)
  {
    struct KV {
      uint32_t kind, id, occ, ordinal;
      bool operator<(const KV& o) const {
        return std::tie(kind, id, occ, ordinal) < std::tie(o.kind, o.id, o.occ, o.ordinal);
      }
    };
    const bool adjacent = history && g_prev2dValid && step.frameIndex == g_prev2dFrame + 1;
    chunksAdjacent = adjacent;
    uint32_t blockDiff[5] = {0, 0, 0, 0, 0}; // round 12, the model log (below)
    // matrices: blocks of vertex-mode pieces, matched by piece and block number
    std::map<KV, uint32_t> beforeBlocks;
    if (adjacent) {
      for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
        const auto& b = previousBlocks[i];
        if ((b.piece2dKind & GX_AURORA_ORRE_2D_VERTICES) != 0) {
          beforeBlocks.emplace(KV{b.piece2dKind, b.piece2dId, b.piece2dOccurrence, b.piece2dBlockIndex}, i);
        }
      }
    }
    for (uint32_t i = 0; i < step.orreBlocks.size() && !beforeBlocks.empty(); ++i) {
      const auto& b = step.orreBlocks[i];
      if ((b.piece2dKind & GX_AURORA_ORRE_2D_VERTICES) == 0) {
        continue;
      }
      const auto it = beforeBlocks.find(KV{b.piece2dKind, b.piece2dId, b.piece2dOccurrence, b.piece2dBlockIndex});
      if (it == beforeBlocks.end()) {
        continue;
      }
      const auto& o = previousBlocks[it->second];
      const size_t at = static_cast<size_t>(b.projOffset) + 64 + static_cast<size_t>(b.piece2dSlot) * 48;
      if (b.piece2dSlot >= PnSlots || o.piece2dSlot != b.piece2dSlot || b.bytes < at + 48 || o.bytes < at + 48 ||
          o.projOffset != b.projOffset) {
        continue;
      }
      const uint8_t* now = step.orreBlockBytes.data() + b.bytesOffset + at;
      const uint8_t* was = previousBytes.data() + o.bytesOffset + at;
      if (log.on && b.bytes == o.bytes) {
        // round 12, the model log: what else changed in the piece's block (what the blend doesn't follow yet): the
        // projection, another matrix slot, the texture matrices, the normal matrices, the colours and the rest
        const uint8_t* bn = step.orreBlockBytes.data() + b.bytesOffset;
        const uint8_t* bw = previousBytes.data() + o.bytesOffset;
        const size_t base = b.projOffset;
        const size_t edges[] = {0, base + 64, base + 64 + 480, base + 64 + 960, base + 64 + 1440, b.bytes};
        for (int r = 0; r < 5; ++r) {
          for (size_t x = edges[r]; x < edges[r + 1] && x < b.bytes; x += 4) {
            if ((x < at || x >= at + 48) && std::memcmp(bn + x, bw + x, 4) != 0) {
              ++blockDiff[r];
              break;
            }
          }
        }
      }
      if (std::memcmp(now, was, 48) == 0) {
        continue;
      }
      // round 12: a window put somewhere else in one step (farther than a slide moves: 64 game units across or down)
      // shows the newest, as a vertex that jumps does
      float mNow[12], mWas[12];
      std::memcpy(mNow, now, sizeof(mNow));
      std::memcpy(mWas, was, sizeof(mWas));
      if (std::fabs(mNow[3] - mWas[3]) > 64.f || std::fabs(mNow[7] - mWas[7]) > 64.f) {
        continue;
      }
      Matrix2DBlend m{.slot = b.piece2dSlot};
      std::memcpy(m.from, was, sizeof(m.from));
      g_block2dMtx[i] = static_cast<int32_t>(g_2dMtx.size());
      g_2dMtx.push_back(m);
      ++p2MtxBlocks;
    }
    // vertices: each piece's draws, matched by piece and draw number
    std::map<KV, uint32_t> beforeChunks;
    // OrrePort, port-gaps 86 (2026-11-21, the project's flicker helper): how many draws each piece had in the step before.
    // Draws pair up by their number inside the piece, so a piece whose draw count changed between the two steps has
    // its numbers shifted from the first missing or extra draw on, and every later draw would blend with its
    // neighbour's place. Seen (verified, full-size dumps at snatcherLeo's 2812 x 2109): the naming keyboard's next-slot bar
    // pulses to alpha 0 twice a second, winSpriteDraw then skips it, and the 11 dashes after it blended halfway to the
    // next dash in that in-between ("they flicker and jump"). A list that gains or loses its extra row as a scroll
    // starts or stops does the same to its rows. Such a piece now shows the newest step (KV's ordinal unused here).
    std::map<KV, uint32_t> beforeCounts;
    if (adjacent) {
      for (uint32_t i = 0; i < g_prev2dChunks.size(); ++i) {
        const auto& c = g_prev2dChunks[i];
        beforeChunks.emplace(KV{c.kind, c.id, c.occurrence, c.ordinal}, i);
        ++beforeCounts[KV{c.kind, c.id, c.occurrence, 0}];
      }
    }
    const auto& chunks = step.orre2dChunks;
    uint32_t snapWhy[4] = {0, 0, 0, 0};     // round 12: the model log's reasons a draw showed the newest
    uint32_t p2VtxCountSnapped = 0;         // port-gaps 86: pieces shown newest because their draw count changed
    std::map<uint32_t, uint32_t> snapKinds; // and which pieces (kind without the vertex-mode bit)
    for (size_t i = 0; i < chunks.size();) {
      // one piece: its draws are consecutive in the list (a piece ends before the next starts)
      size_t j = i + 1;
      while (j < chunks.size() && chunks[j].kind == chunks[i].kind && chunks[j].id == chunks[i].id &&
             chunks[j].occurrence == chunks[i].occurrence) {
        ++j;
      }
      if (chunks[i].kind == detail::OrreParticleChunkKind) {
        i = j; // (3b): a particle's draws, planned by the corner pass after item (a)
        continue;
      }
      VertexPiece2D piece{.spanStart = chunks[i].vtxOffset, .spanEnd = chunks[i].vtxOffset};
      bool moved = false, contiguous = true;
      // port-gaps 86 (above): the same number of draws in both steps, or the piece shows the newest step
      const auto countIt = beforeCounts.find(KV{chunks[i].kind, chunks[i].id, chunks[i].occurrence, 0});
      const bool sameCount = countIt != beforeCounts.end() && countIt->second == static_cast<uint32_t>(j - i);
      if (countIt != beforeCounts.end() && !sameCount) {
        ++p2VtxCountSnapped;
        snapKinds[chunks[i].kind & 0xFFFFFFu] += static_cast<uint32_t>(j - i);
      }
      for (size_t k = i; k < j; ++k) {
        const auto& c = chunks[k];
        contiguous = contiguous && c.vtxOffset >= piece.spanEnd && c.vtxOffset - piece.spanEnd < 4;
        piece.spanEnd = c.vtxOffset + c.length;
        int32_t partner = -1;
        const auto it = sameCount ? beforeChunks.find(KV{c.kind, c.id, c.occurrence, c.ordinal}) : beforeChunks.end();
        if (it != beforeChunks.end()) {
          const auto& o = g_prev2dChunks[it->second];
          const uint8_t* now = step.orre2dChunkBytes.data() + c.bytesOffset;
          const uint8_t* was = g_prev2dChunkBytes.data() + o.bytesOffset;
          const auto& l = c.layout;
          bool same = o.length == c.length && o.layout == l && o.variant == c.variant && l.posCount != 0 &&
                      l.vtxSize != 0 && c.length % l.vtxSize == 0;
          int why = same ? 0 : 1; // round 12, the log's reasons: 1 another size or layout, 2 another picture, 3 a jump
          bool differs = false;
          // the rest of each vertex (texture coordinates, a second colour) must be the same; else the newest
          const uint32_t blendBytes = l.posBytes + l.clrBytes;
          for (uint32_t v = 0; same && v < c.length; v += l.vtxSize) {
            differs = differs || std::memcmp(now + v, was + v, blendBytes) != 0;
            same = std::memcmp(now + v + blendBytes, was + v + blendBytes, l.vtxSize - blendBytes) == 0;
            why = same ? 0 : 2;
            // round 12 (the other menus): a vertex that moved farther than a slide does in one step (a list scrolled
            // back by a row, a sprite put back at its start, a sequence's instant jump) shows the newest; blending it
            // would sweep it across the screen. 64 game units: the fastest slides measured move 12-15 a step
            const uint32_t comp = l.posBytes / l.posCount;
            for (uint32_t axis = 0; same && axis < l.posCount && axis < 2; ++axis) {
              const double a = read_component(was + v + axis * comp, l.posType, l.posFrac);
              const double b = read_component(now + v + axis * comp, l.posType, l.posFrac);
              same = std::fabs(b - a) <= 64.0;
              why = same ? 0 : 3;
            }
          }
          if (same && differs) {
            partner = static_cast<int32_t>(it->second);
            moved = true;
            ++p2VtxDraws;
          } else if (!same) {
            ++p2VtxSnapped;
            ++snapWhy[why];
            snapKinds[c.kind & 0xFFFFFFu] += 1;
          }
        }
        piece.chunks.emplace_back(static_cast<uint32_t>(k), partner);
      }
      // the span must hold only this piece's vertices, in order (alignment padding at most), else drawn as the step
      if (moved && contiguous) {
        g_2dVtx.push_back(std::move(piece));
      }
      i = j;
    }
    // the step before's draws stay for this step's in-betweens (the partners above index them), and the newest step
    // becomes the step before for the next plan
    g_from2dChunks = std::move(g_prev2dChunks);
    g_from2dChunkBytes = std::move(g_prev2dChunkBytes);
    g_prev2dChunks = step.orre2dChunks;
    g_prev2dChunkBytes = step.orre2dChunkBytes;
    g_prev2dFrame = step.frameIndex;
    g_prev2dValid = true;
    if (log.on && (!g_2dVtx.empty() || !g_2dMtx.empty() || p2VtxSnapped != 0 || p2VtxCountSnapped != 0 ||
                   blockDiff[0] + blockDiff[1] + blockDiff[2] + blockDiff[3] + blockDiff[4] != 0)) {
      std::string kinds;
      for (const auto& [kind, n] : snapKinds) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), " %x:%u", kind, n);
        kinds += buf;
      }
      log.line("retrace %u 2D vertex pieces %zu (draws blended %llu, shown newest %llu: size or layout %u, picture %u, "
               "jump %u, pieces whose draw count changed %u; by piece%s), matrices blended %zu; blocks changed "
               "elsewhere: projection %u, other slots %u, texture matrices %u, normal matrices %u, colours and the rest %u",
               retrace, g_2dVtx.size(), static_cast<unsigned long long>(p2VtxDraws),
               static_cast<unsigned long long>(p2VtxSnapped), snapWhy[1], snapWhy[2], snapWhy[3], p2VtxCountSnapped,
               kinds.c_str(),
               g_2dMtx.size(), blockDiff[0], blockDiff[1], blockDiff[2], blockDiff[3], blockDiff[4]);
    }
  }
  {
    constexpr auto r = std::memory_order_relaxed;
    g_stats.p2VtxDraws.fetch_add(p2VtxDraws, r);
    g_stats.p2VtxSnapped.fetch_add(p2VtxSnapped, r);
    g_stats.p2MtxBlocks.fetch_add(p2MtxBlocks, r);
  }
  // ---- Item (a), the particle blend (2026-11-17; docs/port/frame-smoothing-plan.md section 18). ----
  // Particles (sparks of a send-out, a move's effects, dust, smoke) were drawn at the newest step in every in-between,
  // while the models and the camera blended: half a step ahead of everything around them on average, and with the
  // camera blending, sliding against the world at the step rate. HSD's particle code works each particle's corners out
  // on the CPU every step (they face the camera) and draws them under GX_PNMTX0 = the camera's view, so there is no
  // matrix of the particle's own to blend. What is known per particle (recording.cpp): the middle of its drawn corners,
  // in world space. In an in-between its draws are moved, as a whole, part of the way back to where that middle was in
  // the step before: the view matrix it is drawn with gets the move, M' = M x T(d), d = (1 - f)(at_before - at_now),
  // expressed as a "pose of the step before" (M x T(at_before - at_now)) that the model blend's number-by-number blend
  // turns into exactly that (the 3x3 is the same on both sides). Its shape, size, colour and turn stay the newest
  // step's (they change little in a step; a guess, by eye). A particle new this step, a reused slot (another bank or
  // id number, or a longer life: a particle's life only counts down), one whose camera was cut or reset, or one that
  // moved over 3/4 of its distance from the camera in one step (the model blend's jump rule) shows the newest step.
  // ORRE_SMOOTH_TEST_NOPARTICLES=1 (a test switch): particles are never moved (the round 5 behaviour), for before /
  // after pictures from one build.
  static const bool noParticles = [] {
    const char* v = std::getenv("ORRE_SMOOTH_TEST_NOPARTICLES");
    return v != nullptr && v[0] == '1';
  }();
  uint64_t ptTagged = 0, ptBlended = 0, ptNew = 0, ptReused = 0, ptJumped = 0, ptHeld = 0;
  uint64_t ptLocked = 0; // round 10: particles moving with their camera, held as drawn
  // (3b): the particles item (a) accepted (matched, same guard, life counting down, not jumped, not locked), with their
  // blocks: the corner pass below blends only these
  std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> ptAccepted; // (particle, draw count) -> its blocks
  {
    std::map<std::pair<uint64_t, uint32_t>, uint32_t> particlesBefore;
    for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
      const auto& b = previousBlocks[i];
      if (b.particle != 0 && b.modelPObj == 0 && b.projIsCamera && b.particleAtValid) {
        particlesBefore.emplace(std::pair{b.particle, b.particleOccurrence}, i);
      }
    }
    constexpr uint32_t posAt = 64; // the position matrices follow the projection (4x4 floats); + projOffset below
    for (uint32_t i = 0; i < step.orreBlocks.size(); ++i) {
      const auto& block = step.orreBlocks[i];
      if (block.particle == 0 || block.modelPObj != 0 || !block.projIsCamera || !block.particleAtValid ||
          block.camera >= g_plan.size() || block.camera >= step.orreCameras.size() ||
          step.orreCameras[block.camera].renderPass != 0) {
        continue;
      }
      ++ptTagged;
      if (noParticles || !history) {
        continue;
      }
      const Cut cut = g_plan[block.camera].cut;
      if (cut != Cut::None && cut != Cut::Unchanged) {
        ++ptHeld;
        continue;
      }
      const auto found = particlesBefore.find({block.particle, block.particleOccurrence});
      if (found == particlesBefore.end()) {
        ++ptNew;
        continue;
      }
      const auto& old = previousBlocks[found->second];
      if (old.particleGuard != block.particleGuard || old.particleLife < block.particleLife) {
        if (log.on && ptReused < 4) {
          log.line("retrace %u particle %llx draw %u: guard %08x -> %08x, life %u -> %u: a reused slot", retrace,
                   (unsigned long long)block.particle, block.particleOccurrence, old.particleGuard,
                   block.particleGuard, old.particleLife, block.particleLife);
        }
        ++ptReused;
        continue;
      }
      const double d[3] = {static_cast<double>(old.particleAt[0]) - block.particleAt[0],
                           static_cast<double>(old.particleAt[1]) - block.particleAt[1],
                           static_cast<double>(old.particleAt[2]) - block.particleAt[2]};
      if (d[0] == 0 && d[1] == 0 && d[2] == 0) {
        // did not move as a whole; its corners may still have changed (a star growing in place): (3b)
        ptAccepted[{static_cast<uint32_t>(block.particle), block.particleOccurrence}].push_back(i);
        continue;
      }
      const uint32_t at = block.projOffset + posAt;
      const uint32_t nrmAt = at + 2 * PnSlots * 48;
      if (block.bytes < nrmAt + 48) {
        continue;
      }
      const float* posNew = reinterpret_cast<const float*>(step.orreBlockBytes.data() + block.bytesOffset + at);
      const float* nrmNew = reinterpret_cast<const float*>(step.orreBlockBytes.data() + block.bytesOffset + nrmAt);
      ModelBlend model;
      model.mask = 1u; // slot 0: GX_PNMTX0, the only matrix HSD's particle code draws with (recording checks it)
      std::memcpy(model.fromPos[0], posNew, 48);
      std::memcpy(model.fromNrm[0], nrmNew, 48);
      for (int r = 0; r < 3; ++r) {
        model.fromPos[0][r * 4 + 3] = static_cast<float>(posNew[r * 4 + 3] + posNew[r * 4 + 0] * d[0] +
                                                         posNew[r * 4 + 1] * d[1] + posNew[r * 4 + 2] * d[2]);
      }
      // the jump test's scale: the particle's distance in front of the camera (its middle, newest step)
      const float* v = posNew;
      const double depth = std::abs(v[8] * block.particleAt[0] + v[9] * block.particleAt[1] +
                                    v[10] * block.particleAt[2] + v[11]);
      // round 10: as for models, a jump in the world and as the camera sees it: the particle's middle in view space,
      // the step before's (its own camera) against the newest; dust or sparks that travel with a following camera
      // (the bike's) barely move on screen and blend with it
      bool relJump = true;
      double relMove = 1e30;
      if (old.bytes >= old.projOffset + posAt + 48) {
        const float* vo = reinterpret_cast<const float*>(previousBytes.data() + old.bytesOffset + old.projOffset + posAt);
        double move = 0;
        for (int r = 0; r < 3; ++r) {
          const double nv = v[r * 4] * block.particleAt[0] + v[r * 4 + 1] * block.particleAt[1] +
                            v[r * 4 + 2] * block.particleAt[2] + v[r * 4 + 3];
          const double ov = vo[r * 4] * old.particleAt[0] + vo[r * 4 + 1] * old.particleAt[1] +
                            vo[r * 4 + 2] * old.particleAt[2] + vo[r * 4 + 3];
          move += (nv - ov) * (nv - ov);
        }
        relMove = std::sqrt(move);
        relJump = relMove > 0.75 * std::max(depth, 1e-3);
      }
      // round 10: a particle that moves with its camera (dust riding with the bike) is held as the step drew it, like
      // the models it rides with (plan section 23). Measured: no change in P01's ride, whose flagged exhaust flame is a
      // new particle each step; kept as the consistent rule
      const double worldMove = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      if (g_blockLocked.size() == step.orreBlocks.size() && worldMove > 0.5 * std::max(depth, 1e-3) &&
          relMove < 0.25 * worldMove) {
        g_blockLocked[i] = 1;
        ++ptLocked;
        continue;
      }
      if (jumped(model.fromPos[0], posNew, depth) && relJump) {
        ++ptJumped;
        continue;
      }
      g_blockModel[i] = static_cast<int32_t>(g_models.size());
      g_models.push_back(model);
      ++ptBlended;
      ptAccepted[{static_cast<uint32_t>(block.particle), block.particleOccurrence}].push_back(i); // (3b)
    }
  }
  // ---- (3b), the particle corners (2026-11-18; docs/port/frame-smoothing-plan.md section 30). ----
  // HSD's particle code works each particle's corners out on the CPU every step (they face the camera, and they grow,
  // shrink, turn and fade as the particle ages). Item (a) moves a particle as a whole, so a star that grows or turns in
  // place (the ice glints of "frozen solid", the sandstorm's and send-outs' sparkles; verified in section 26) showed the
  // newest step's shape in every in-between. Now each draw of a particle item (a) accepted is matched with the same draw
  // of the step before (same particle, guard, draw count and draw number; the same layout, size and texture
  // coordinates), and its corners' positions (world space, three floats) and first colour are drawn part way, written
  // past the step's own vertices as round 11's vertex mode does for the menus. The corners carry the particle's motion
  // too, so item (a)'s matrix move is dropped for that particle (both would move it twice). Anything that doesn't match
  // (a texture frame swapped, another vertex count, a corner that moved over 64 units) keeps item (a) as before.
  // ORRE_SMOOTH_TEST_NOPARTICLECORNERS=1 (a test switch) turns it off, for before / after pictures from one build.
  static const bool noCorners = [] {
    const char* v = std::getenv("ORRE_SMOOTH_TEST_NOPARTICLECORNERS");
    return v != nullptr && v[0] == '1';
  }();
  uint64_t cornerParticles = 0, cornerDraws = 0, cornerKept = 0;
  if (!noCorners && chunksAdjacent && !ptAccepted.empty()) {
    struct PK {
      uint32_t id, occ, ordinal;
      bool operator<(const PK& o) const { return std::tie(id, occ, ordinal) < std::tie(o.id, o.occ, o.ordinal); }
    };
    std::map<PK, uint32_t> before;
    for (uint32_t i = 0; i < g_from2dChunks.size(); ++i) {
      const auto& c = g_from2dChunks[i];
      if (c.kind == detail::OrreParticleChunkKind) {
        before.emplace(PK{c.id, c.occurrence, c.ordinal}, i);
      }
    }
    const auto& chunks = step.orre2dChunks;
    for (size_t i = 0; i < chunks.size() && !before.empty();) {
      size_t j = i + 1;
      while (j < chunks.size() && chunks[j].kind == chunks[i].kind && chunks[j].id == chunks[i].id &&
             chunks[j].occurrence == chunks[i].occurrence) {
        ++j;
      }
      if (chunks[i].kind != detail::OrreParticleChunkKind) {
        i = j;
        continue;
      }
      const auto accepted = ptAccepted.find({chunks[i].id, chunks[i].occurrence});
      if (accepted == ptAccepted.end()) {
        i = j;
        continue;
      }
      VertexPiece2D piece{.spanStart = chunks[i].vtxOffset, .spanEnd = chunks[i].vtxOffset};
      bool all = true, moved = false, contiguous = true;
      for (size_t k = i; k < j && all; ++k) {
        const auto& c = chunks[k];
        contiguous = contiguous && c.vtxOffset >= piece.spanEnd && c.vtxOffset - piece.spanEnd < 4;
        piece.spanEnd = c.vtxOffset + c.length;
        const auto it = before.find(PK{c.id, c.occurrence, c.ordinal});
        if (it == before.end()) {
          all = false;
          break;
        }
        const auto& o = g_from2dChunks[it->second];
        const auto& l = c.layout;
        bool same = o.length == c.length && o.layout == l && o.variant == c.variant && l.posCount == 3 &&
                    l.posType == GX_F32 && l.vtxSize != 0 && c.length % l.vtxSize == 0;
        const uint8_t* now = step.orre2dChunkBytes.data() + c.bytesOffset;
        const uint8_t* was = g_from2dChunkBytes.data() + o.bytesOffset;
        const uint32_t blendBytes = l.posBytes + l.clrBytes;
        bool differs = false;
        for (uint32_t v = 0; same && v < c.length; v += l.vtxSize) {
          differs = differs || std::memcmp(now + v, was + v, blendBytes) != 0;
          same = std::memcmp(now + v + blendBytes, was + v + blendBytes, l.vtxSize - blendBytes) == 0;
          for (uint32_t axis = 0; same && axis < 3; ++axis) {
            const double a = read_component(was + v + axis * 4, l.posType, l.posFrac);
            const double b = read_component(now + v + axis * 4, l.posType, l.posFrac);
            same = std::isfinite(a) && std::isfinite(b) && std::fabs(b - a) <= 64.0;
          }
        }
        if (!same) {
          all = false;
          break;
        }
        moved = moved || differs;
        piece.chunks.emplace_back(static_cast<uint32_t>(k), static_cast<int32_t>(it->second));
      }
      if (all && moved && contiguous) {
        // the corners carry the move: item (a)'s matrix move off for this particle's blocks
        for (const uint32_t b : accepted->second) {
          if (g_blockModel.size() > b) {
            g_blockModel[b] = -1;
          }
        }
        cornerDraws += piece.chunks.size();
        ++cornerParticles;
        g_2dVtx.push_back(std::move(piece));
      } else if (moved || !all) {
        ++cornerKept;
      }
      i = j;
    }
  }
  if (log.on && (cornerParticles != 0 || cornerKept != 0)) {
    log.line("retrace %u particle corners: %llu particles (%llu draws) blended by their corners, %llu kept to item (a)",
             retrace, (unsigned long long)cornerParticles, (unsigned long long)cornerDraws,
             (unsigned long long)cornerKept);
  }
  g_stats.cornerParticles.fetch_add(cornerParticles, std::memory_order_relaxed);
  if (log.on) {
    log.line("retrace %u particles: blocks %llu, blended %llu, new %llu, reused slot %llu, jumped %llu, held %llu, "
             "locked to the camera %llu",
             retrace, (unsigned long long)ptTagged, (unsigned long long)ptBlended, (unsigned long long)ptNew,
             (unsigned long long)ptReused, (unsigned long long)ptJumped, (unsigned long long)ptHeld,
             (unsigned long long)ptLocked);
    {
      // round 10 (2026-11-17, a measure only: the HP / EXP bar question): blocks drawn with another projection than
      // their camera's (the battle's 2D: HP boxes, bars, menus) that belong to an HSD model piece, matched with the
      // step before's by piece and draw number; which of them changed, and how (the first used matrix's x scale and
      // its offsets), to tell whether a bar's fill is a model piece moved by its matrix (blendable like a model) or
      // something else (CPU-written corners, a texture change). Never changes a picture.
      std::map<Key, uint32_t> looseBefore;
      for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
        const auto& b = previousBlocks[i];
        if (b.modelPObj != 0 && !b.projIsCamera) {
          looseBefore.emplace(Key{b.modelJObj, b.modelPObj, passOf(previous, b), b.modelOccurrence}, i);
        }
      }
      uint32_t loose = 0, looseMatched = 0, looseChanged = 0, lines = 0;
      for (const auto& b : step.orreBlocks) {
        if (b.modelPObj == 0 || b.projIsCamera) {
          continue;
        }
        ++loose;
        const auto it = looseBefore.find(Key{b.modelJObj, b.modelPObj, passOf(step.orreCameras, b), b.modelOccurrence});
        if (it == looseBefore.end()) {
          continue;
        }
        ++looseMatched;
        const auto& o = previousBlocks[it->second];
        const uint32_t at = b.projOffset + 64;
        if (b.bytes < at + 48 || o.bytes < o.projOffset + 64 + 48) {
          continue;
        }
        uint32_t s = 0;
        while (s + 1 < PnSlots && (used(b) & (1u << s)) == 0) {
          ++s;
        }
        const float* mn = reinterpret_cast<const float*>(step.orreBlockBytes.data() + b.bytesOffset + at + s * 48);
        const float* mo = reinterpret_cast<const float*>(previousBytes.data() + o.bytesOffset + o.projOffset + 64 + s * 48);
        if (std::memcmp(mn, mo, 48) != 0) {
          ++looseChanged;
          if (lines++ < 6) {
            log.line("retrace %u 2D piece J %llx P %llx draw %u slot %u: x scale %.4f -> %.4f, offsets (%.2f, %.2f, "
                     "%.2f) -> (%.2f, %.2f, %.2f)",
                     retrace, (unsigned long long)b.modelJObj, (unsigned long long)b.modelPObj, b.modelOccurrence, s,
                     column_length(mo, 0), column_length(mn, 0), mo[3], mo[7], mo[11], mn[3], mn[7], mn[11]);
          }
        }
      }
      log.line("retrace %u 2D model pieces (another projection): blocks %u, matched %u, matrix changed %u", retrace,
               loose, looseMatched, looseChanged);
    }
    uint32_t uvBlocks = 0; // blocks with a texture matrix fed by texture coordinates, all of this step
    for (const auto& b : step.orreBlocks) {
      uvBlocks += b.texUvMask != 0 ? 1u : 0u;
    }
    log.line("retrace %u scrolling textures: blocks with one %u; in matched pieces %llu, changed %llu: blended %llu "
             "(across a wrap %llu), changed too much %llu, not steady %llu",
             retrace, uvBlocks, (unsigned long long)scrollSeen, (unsigned long long)scrollChanged,
             (unsigned long long)scrollBlended, (unsigned long long)scrollWrapped, (unsigned long long)scrollSnapped,
             (unsigned long long)scrollUnsteady);
  }
  {
    constexpr auto r = std::memory_order_relaxed;
    g_stats.scrollUnsteady.fetch_add(scrollUnsteady, r); // round 9
    g_stats.lockedBlocks.fetch_add(ptLocked, r);         // round 10: particles locked to the camera
    g_stats.scrollBlended.fetch_add(scrollBlended, r); // item (b)
    g_stats.scrollWrapped.fetch_add(scrollWrapped, r);
    g_stats.scrollSnapped.fetch_add(scrollSnapped, r);
    g_stats.particleTagged.fetch_add(ptTagged, r);
    g_stats.particleBlended.fetch_add(ptBlended, r);
    g_stats.particleNew.fetch_add(ptNew, r);
    g_stats.particleReused.fetch_add(ptReused, r);
    g_stats.particleJumped.fetch_add(ptJumped, r);
    g_stats.particleHeld.fetch_add(ptHeld, r);
  }
  if (log.on) {
    if (history) {
      for (uint32_t i = 0; i < previousBlocks.size(); ++i) {
        const auto& b = previousBlocks[i];
        if (b.modelPObj != 0 && b.projIsCamera && !beforeMatched[i]) {
          log.line("retrace %u gone piece J %llx P %llx draw %u root %llx slots %03x pass %d (its model %s now)",
                   retrace, (unsigned long long)b.modelJObj, (unsigned long long)b.modelPObj, b.modelOccurrence,
                   (unsigned long long)b.modelRoot, b.modelSlots, passOf(previous, b),
                   rootsNow.count(b.modelRoot) ? "is still there" : "is gone");
        }
      }
      for (const auto& [piece, n] : counts) {
        if (n.first != n.second && n.first != 0 && n.second != 0) {
          log.line("retrace %u count J %llx P %llx pass %d: drawn %u times the step before, %u now", retrace,
                   (unsigned long long)std::get<0>(piece), (unsigned long long)std::get<1>(piece), std::get<2>(piece),
                   n.first, n.second);
        }
      }
    }
    // each screen-pass model once: where on screen its first piece's first used matrix sits (percent across and down:
    // which model is which Pokemon in a battle, and where to crop the pictures), and whether it snapped whole
    std::set<uint64_t> placed;
    for (const auto& b : step.orreBlocks) {
      if (b.modelPObj == 0 || !b.projIsCamera || passOf(step.orreCameras, b) != 0 || !placed.insert(b.modelRoot).second ||
          b.bytes < b.projOffset + 64 + PnSlots * 48) {
        continue;
      }
      uint32_t s = 0;
      while (s + 1 < PnSlots && (used(b) & (1u << s)) == 0) {
        ++s;
      }
      const float* m = reinterpret_cast<const float*>(step.orreBlockBytes.data() + b.bytesOffset + b.projOffset + 64 + s * 48);
      const auto& proj = step.orreCameras[b.camera].proj;
      const double v[4] = {m[3], m[7], m[11], 1.0};
      double clip[4] = {0, 0, 0, 0};
      for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
          clip[r] += proj[r][c] * v[c];
        }
      }
      const double w = std::abs(clip[3]) > 1e-9 ? clip[3] : 1e-9;
      log.line("retrace %u model root %llx at screen (%.0f %%, %.0f %%), depth %.1f%s", retrace,
               (unsigned long long)b.modelRoot, (clip[0] / w * 0.5 + 0.5) * 100.0, (0.5 - clip[1] / w * 0.5) * 100.0,
               -m[11],
               snapModels.count(b.modelRoot)                 ? ", snapped whole"
               : snapModelPass.count({b.modelRoot, 0}) != 0 ? ", snapped whole in the screen pass"
                                                             : "");
    }
    {
      // round 5: every block with a texture matrix fed by positions or normals, by kind (where projected textures are)
      uint32_t kinds[2][2][2]{}; // [tagged][projIsCamera][pass 0]
      for (const auto& b : step.orreBlocks) {
        if ((b.texPosMask | b.texNrmMask) != 0) {
          ++kinds[b.modelPObj != 0][b.projIsCamera][passOf(step.orreCameras, b) == 0];
        }
      }
      log.line("retrace %u texture-matrix blocks: model pieces %u (camera's projection, screen pass %u / other pass "
               "%u; another projection %u), other draws %u (camera's projection %u)",
               retrace, kinds[1][0][0] + kinds[1][0][1] + kinds[1][1][0] + kinds[1][1][1], kinds[1][1][1],
               kinds[1][1][0], kinds[1][0][0] + kinds[1][0][1], kinds[0][0][0] + kinds[0][0][1] + kinds[0][1][0] + kinds[0][1][1],
               kinds[0][1][0] + kinds[0][1][1]);
    }
    log.line("retrace %u summary: history %d, model blocks %llu, matched %llu (borrowed %llu), blended %llu, in a model "
             "snapped whole %llu (models %zu, in one pass %zu), held %llu; models before %zu, now %zu",
             retrace, history ? 1 : 0, (unsigned long long)tagged, (unsigned long long)matched,
             (unsigned long long)borrowed, (unsigned long long)moved, (unsigned long long)snapped, snapModels.size(),
             snapModelPass.size(), (unsigned long long)held, rootsBefore.size(), rootsNow.size());
  }
  g_stats.modelBorrowed.fetch_add(borrowed, std::memory_order_relaxed);
  g_stats.modelsSnapped.fetch_add(snapModels.size() + snapModelPass.size(), std::memory_order_relaxed);
  constexpr auto r = std::memory_order_relaxed;
  g_stats.modelTagged.fetch_add(tagged, r);
  g_stats.modelUntagged.fetch_add(untagged, r);
  g_stats.modelMatched.fetch_add(matched, r);
  g_stats.modelMoved.fetch_add(moved, r);
  g_stats.modelSnapped.fetch_add(snapped, r);
  g_stats.modelHeld.fetch_add(held, r);
}
} // namespace

void plan_step(const FramePacket& step, const std::vector<OrreSmoothCamera>& previous,
               const std::vector<OrreSmoothBlock>& previousBlocks, const std::vector<uint8_t>& previousBytes,
               bool history, uint32_t retrace, const TwoBack& twoBack) {
  // the previous plan keeps each camera's last move, for the cut test's "sudden" half below
  static std::vector<Planned> lastPlan;
  lastPlan.swap(g_plan);
  g_plan.assign(step.orreCameras.size(), Planned{});
  for (size_t i = 0; i < step.orreCameras.size(); ++i) {
    const auto& camera = step.orreCameras[i];
    auto& p = g_plan[i];
    p.id = camera.id;
    p.occurrence = camera.occurrence;
    if (!camera.perspective || camera.renderPass != 0) {
      // never re-aimed. Blocks drawn under an offscreen-pass camera (the shadows') are tagged with it since
      // 2026-11-17, but only their model pieces blend (plan_models), never the camera
      p.cut = Cut::Unmatched;
      continue;
    }
    g_stats.cameras.fetch_add(1, std::memory_order_relaxed);
    std::memcpy(p.toProj, camera.proj, sizeof(p.toProj));
    std::memcpy(p.toView, camera.view, sizeof(p.toView));
    p.to = decompose(camera);
    const OrreSmoothCamera* before = nullptr;
    for (const auto& candidate : previous) {
      if (candidate.id == camera.id && candidate.occurrence == camera.occurrence && candidate.perspective &&
          candidate.renderPass == 0) {
        before = &candidate;
        break;
      }
    }
    double degrees = 0, fovRatio = 1;
    if (!history) {
      p.cut = Cut::Reset;
    } else if (before == nullptr) {
      p.cut = Cut::Unmatched;
    } else {
      std::memcpy(p.fromProj, before->proj, sizeof(p.fromProj));
      p.from = decompose(*before);
      // The jump test's scale: the eye-to-target distance. A camera without a usable target on both sides (P01's
      // battle cameras, often) blends around its eye instead (the pivot one unit ahead, decompose), and its jump test
      // uses a fixed 4 world units (a quarter of 16; P01's battle cameras sit 20-60 units from their subjects, the
      // field cameras about 5: a guess, tuned by eye). Its target test is skipped (the pivot moves with the eye).
      const bool targets = p.from.hasTarget && p.to.hasTarget;
      if (!targets) {
        p.from.target = p.from.eye + (p.from.rows[2] * -1.0);
        p.to.target = p.to.eye + (p.to.rows[2] * -1.0);
        p.from.distance = p.to.distance = 0;
      }
      const double distance = targets ? std::max(p.from.distance, p.to.distance) : 16.0;
      if (std::memcmp(before->view, camera.view, sizeof(camera.view)) == 0 &&
          std::memcmp(before->proj, camera.proj, sizeof(camera.proj)) == 0) {
        p.cut = Cut::Unchanged;
      } else if (!p.from.rigid || !p.to.rigid) {
        p.cut = Cut::NotRigid;
      } else if (!plain_perspective(before->proj) || !plain_perspective(camera.proj)) {
        p.cut = Cut::Fov;
      } else {
        degrees = turn_degrees(p.from, p.to);
        fovRatio = std::max(std::abs(static_cast<double>(camera.proj[0][0]) / before->proj[0][0] - 1.0),
                            std::abs(static_cast<double>(camera.proj[1][1]) / before->proj[1][1] - 1.0));
        p.eyeMove = length(p.to.eye - p.from.eye);
        p.targetMove = length(p.to.target - p.from.target);
        p.turn = degrees;
        // A cut is a jump that is both big and sudden. Big: over the thresholds above. Sudden: over three times the
        // same camera's move in the step before (plus a little), so a camera tracking something fast is not a cut.
        // Measured 2026-11-16 (P01, room 0x1, the opening's tracking shot): the eye moved 1.47-1.54 units every step
        // at an eye-to-target distance of 4.94 (30 %), steadily; the size test alone snapped every step there.
        double lastEye = 0, lastTarget = 0, lastTurn = 0;
        for (const auto& old : lastPlan) {
          if (old.id == camera.id && old.occurrence == camera.occurrence) {
            lastEye = old.eyeMove, lastTarget = old.targetMove, lastTurn = old.turn;
            break;
          }
        }
        const auto sudden = [](double now, double last, double floor) { return now > 3.0 * last + floor; };
        if (degrees > CutDegrees && sudden(degrees, lastTurn, 2.0)) {
          p.cut = Cut::Rotation;
        } else if (p.eyeMove > CutJumpOfDistance * distance && sudden(p.eyeMove, lastEye, 0.05 * distance)) {
          p.cut = Cut::Eye;
        } else if (targets && p.targetMove > CutJumpOfDistance * distance &&
                   sudden(p.targetMove, lastTarget, 0.05 * distance)) {
          p.cut = Cut::Target;
        } else if (fovRatio > CutFovRatio) {
          p.cut = Cut::Fov;
        } else {
          p.cut = Cut::None;
        }
        // The moves are kept even when this step is a cut: the next step compares with them. *Corrected 2026-11-16:*
        // zeroing them after a cut made a fast steady camera snap on every step (each cut made the next step's move
        // look sudden again; seen in P01's opening, frames 6812-6922).
        if (p.cut == Cut::None && !targets) {
          // counted apart for the log: blended, but around the eye (Cut::None is counted below as well)
          g_stats.cuts[static_cast<int>(Cut::NoTarget)].fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
    g_stats.cuts[static_cast<int>(p.cut)].fetch_add(1, std::memory_order_relaxed);
    note_cut(p.cut, retrace, p, degrees, fovRatio);
    if (model_log_wanted(retrace)) {
      // round 10 (2026-11-17, a log only): each screen camera's plan, for the seam sweep's camera questions
      std::fprintf(stderr,
                   "[smooth] modellog retrace %u camera %llx #%u: cut %d, eye (%.2f %.2f %.2f) -> (%.2f %.2f %.2f), "
                   "target (%.2f %.2f %.2f) -> (%.2f %.2f %.2f), turn %.1f, target %d\n",
                   retrace, (unsigned long long)camera.id, camera.occurrence, static_cast<int>(p.cut), p.from.eye.x,
                   p.from.eye.y, p.from.eye.z, p.to.eye.x, p.to.eye.y, p.to.eye.z, p.from.target.x, p.from.target.y,
                   p.from.target.z, p.to.target.x, p.to.target.y, p.to.target.z, degrees,
                   p.from.hasTarget && p.to.hasTarget ? 1 : 0);
    }
  }
  // item (c): two steps back is usable only when the step before had history too (no reset between them)
  static bool lastHistory = false;
  const bool twoBackOk = history && lastHistory;
  lastHistory = history;
  plan_models(step, previous, previousBlocks, previousBytes, history, retrace, twoBack, twoBackOk); // step (d)
}

uint32_t last_model_writes() noexcept { return g_lastModelWrites; }
bool two_step_at_one() noexcept { return g_twoStepAtOne; } // item (c)

uint32_t prepare(const FramePacket& step, float fraction) {
  g_patch.clear();
  g_map.clear();
  g_vpatch.clear(); // round 11, vertex mode
  g_vmap.clear();
  g_lastModelWrites = 0;
  g_lastTexFollows = 0;
  // item (c): at fraction 1 the picture is the step's own, except for a menu model in the first half of its two-step
  // blend (it must still be half way there: two_step_at_one); only those blocks are written then
  const bool atOne = fraction >= 1.f;
  if ((atOne && !g_twoStepAtOne) || step.orreBlocks.empty() || g_plan.size() != step.orreCameras.size()) {
    return 0;
  }
  const double f = std::clamp(static_cast<double>(fraction), 0.0, 1.0);
  // per camera: the re-aim A = V_blend x V_new^-1 and the blended projection P_blend (rows)
  struct Aim {
    bool on = false;
    Mat4d a{};
    Mat4f camProj{}; // uniform_projection(P_blend x A): for blocks with the camera's own projection
  };
  std::vector<Aim> aims(g_plan.size());
  bool any = false;
  for (size_t i = 0; i < g_plan.size(); ++i) {
    const auto& p = g_plan[i];
    if (p.cut != Cut::None) {
      continue;
    }
    const Vec3d eye = lerp(p.from.eye, p.to.eye, f);
    const Vec3d target = lerp(p.from.target, p.to.target, f);
    Vec3d up = lerp(p.from.up, p.to.up, f);
    const double upLen = length(up);
    if (upLen < 1e-9) {
      continue;
    }
    up = up * (1.0 / upLen);
    Mat4d blendView;
    if (!look_at(eye, up, target, blendView)) {
      continue;
    }
    // the newest view's inverse (rigid): R^T, -R^T t
    Mat4d inverse{};
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        inverse[r][c] = p.toView[c][r];
      }
    }
    for (int r = 0; r < 3; ++r) {
      inverse[r][3] = -(inverse[r][0] * p.toView[0][3] + inverse[r][1] * p.toView[1][3] +
                        inverse[r][2] * p.toView[2][3]);
    }
    inverse[3] = {0, 0, 0, 1};
    auto& aim = aims[i];
    aim.a = mul(blendView, inverse);
    Mat4d projBlend{};
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        projBlend[r][c] = p.fromProj[r][c] + (static_cast<double>(p.toProj[r][c]) - p.fromProj[r][c]) * f;
      }
    }
    const Mat4d aimed = mul(projBlend, aim.a);
    Mat4f raw{};
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        raw[r][c] = static_cast<float>(aimed[r][c]);
      }
    }
    aim.camProj = gx::uniform_projection(raw);
    aim.on = true;
    any = true;
  }
  // step (d): model pieces that moved blend too, also under a camera that stood still (no re-aim then)
  // ORRE_SMOOTH_TEST_ONLY=camera|models (a test switch, round 5): blend only the camera or only the models, to tell
  // which part of a blended picture goes wrong
  static const int testOnly = [] {
    const char* v = std::getenv("ORRE_SMOOTH_TEST_ONLY");
    return v == nullptr ? 0 : std::strcmp(v, "camera") == 0 ? 1 : std::strcmp(v, "models") == 0 ? 2 : 0;
  }();
  if (testOnly == 2) {
    for (auto& aim : aims) {
      aim.on = false;
    }
    any = false;
  }
  const bool models = testOnly != 1 && !g_models.empty() && g_blockModel.size() == step.orreBlocks.size();
  // round 11: named 2D pieces to blend (never at fraction 1: the step's own picture has the game's own widths)
  const bool pieces = !atOne && !g_2d.empty() && g_block2d.size() == step.orreBlocks.size();
  // round 11, vertex mode (the pause menu): matrices and vertices to blend (likewise never at fraction 1)
  const bool mtx2d = !atOne && !g_2dMtx.empty() && g_block2dMtx.size() == step.orreBlocks.size();
  const bool vtx2d = !atOne && !g_2dVtx.empty();
  if ((!any && !models && !pieces && !mtx2d && !vtx2d) || !ensure_buffer()) {
    return 0;
  }
  const uint32_t align = static_cast<uint32_t>(detail::resources().limits.minUniformBufferOffsetAlignment);
  const uint64_t limit = BlendBufferSize - gx::MaxUniformSize;
  uint32_t loose = 0, overflow = 0, modelWrites = 0, looseSame = 0, looseTagged = 0;
  uint32_t written2d = 0; // round 11: named 2D pieces drawn blended
  for (size_t i = 0; i < step.orreBlocks.size(); ++i) {
    const auto& block = step.orreBlocks[i];
    if (g_blockLocked.size() == step.orreBlocks.size() && g_blockLocked[i] != 0) {
      continue; // round 10: a model moving with its camera, drawn as the step drew it (plan_models' lock)
    }
    if (pieces && g_block2d[i] >= 0) {
      // round 11: a named 2D piece (a bar's fill): its matrix slot scaled across x about the piece's left end x0 by
      // k = blended value / drawn value, M' = M x T(x0) x S(k, 1, 1) x T(-x0): column 0 times k, and the offset column
      // moved by (1 - k) x0 times the old column 0. Never re-aimed (2D), nothing else changed.
      const auto& p2 = g_2d[static_cast<size_t>(g_block2d[i])];
      const size_t offset = (g_patch.size() + align - 1) / align * align;
      const size_t slotSize = (static_cast<size_t>(block.size) + align - 1) / align * align;
      if (offset + slotSize > limit) {
        ++overflow;
        continue;
      }
      g_patch.resize(offset + slotSize, 0);
      std::memcpy(g_patch.data() + offset, step.orreBlockBytes.data() + block.bytesOffset, block.bytes);
      auto* m = reinterpret_cast<float*>(g_patch.data() + offset + block.projOffset + 64 + p2.slot * 48);
      const double k = (p2.from + (static_cast<double>(p2.to) - p2.from) * f) / p2.to;
      for (int r = 0; r < 3; ++r) {
        const double c0 = m[r * 4 + 0];
        m[r * 4 + 0] = static_cast<float>(c0 * k);
        m[r * 4 + 3] = static_cast<float>(m[r * 4 + 3] + (1.0 - k) * p2.x0 * c0);
      }
      g_map.emplace_back(block.uniformOffset, static_cast<uint32_t>(offset));
      ++written2d;
      continue;
    }
    if (mtx2d && g_block2dMtx[i] >= 0) {
      // round 11, vertex mode: a pause-menu block whose position matrix moved (its window sliding): the matrix drawn
      // blended element by element from the step before's to the newest (2D sprite cameras: translations only, so an
      // element blend is the blended position)
      const auto& mb = g_2dMtx[static_cast<size_t>(g_block2dMtx[i])];
      const size_t offset = (g_patch.size() + align - 1) / align * align;
      const size_t slotSize = (static_cast<size_t>(block.size) + align - 1) / align * align;
      if (offset + slotSize > limit) {
        ++overflow;
        continue;
      }
      g_patch.resize(offset + slotSize, 0);
      std::memcpy(g_patch.data() + offset, step.orreBlockBytes.data() + block.bytesOffset, block.bytes);
      auto* m = reinterpret_cast<float*>(g_patch.data() + offset + block.projOffset + 64 + mb.slot * 48);
      for (int e = 0; e < 12; ++e) {
        m[e] = static_cast<float>(mb.from[e] + (static_cast<double>(m[e]) - mb.from[e]) * f);
      }
      g_map.emplace_back(block.uniformOffset, static_cast<uint32_t>(offset));
      ++written2d;
      continue;
    }
    int32_t model = models ? g_blockModel[i] : -1;
    if (atOne && (model < 0 || g_models[static_cast<size_t>(model)].fBias + g_models[static_cast<size_t>(model)].fScale >= 1.0)) {
      continue; // item (c): at fraction 1 only a two-step model still on its way is written
    }
    const bool aimed = !atOne && block.camera < aims.size() && aims[block.camera].on;
    if (!aimed && model < 0) {
      continue;
    }
    if (!block.projIsCamera) {
      // Another perspective projection while this camera is current: left as the step drew it. *Changed 2026-11-16:*
      // the first build re-aimed these too (stored x A, below), meant for a sky drawn with other near and far planes;
      // P01's battle HUD (HP boxes, command window, portraits: 2D drawn through a perspective projection of the game's
      // own, after the battle camera was set) is such a block, and it swam with the camera (frame 25002's dumps).
      // Field scenes have few (436 a 10 s at most in P01), battles about 30 % of the tagged blocks.
      // (Never a model block: plan_models skips these.) Counted for the diagnosis of 2026-11-17: same frustum as the
      // camera with other depth planes (rows 0, 1 and 3 of the stored projection equal the camera's; the depth row is
      // the only one uniform_projection changes), and how many belong to an HSD model piece.
      ++loose;
      if (block.camera < g_plan.size() && block.bytes >= block.projOffset + 64) {
        float stored[16];
        std::memcpy(stored, step.orreBlockBytes.data() + block.bytesOffset + block.projOffset, sizeof(stored));
        const auto& cam = g_plan[block.camera].toProj;
        bool same = true;
        for (int c = 0; c < 4 && same; ++c) {
          same = stored[0 * 4 + c] == cam[0][c] && stored[1 * 4 + c] == cam[1][c] && stored[3 * 4 + c] == cam[3][c];
        }
        looseSame += same ? 1u : 0u;
      }
      looseTagged += block.modelPObj != 0 ? 1u : 0u;
      continue;
    }
    const size_t offset = (g_patch.size() + align - 1) / align * align;
    const size_t slot = (static_cast<size_t>(block.size) + align - 1) / align * align;
    if (offset + slot > limit) {
      ++overflow;
      continue;
    }
    g_patch.resize(offset + slot, 0);
    std::memcpy(g_patch.data() + offset, step.orreBlockBytes.data() + block.bytesOffset, block.bytes);
    if (model >= 0) {
      // step (d): the moved matrices, `f` of the way from the step before's pose (carried into this camera space) to
      // the newest; position and normal matrices of a slot together
      const auto& m = g_models[static_cast<size_t>(model)];
      const double f = std::clamp(m.fBias + m.fScale * fraction, 0.0, 1.0); // item (c): the two-step history
      const uint32_t posAt = block.projOffset + 64;
      const uint32_t texAt = posAt + PnSlots * 48;
      const uint32_t nrmAt = posAt + 2 * PnSlots * 48;
      // the newest slot 0 matrices, kept for the texture matrices below
      float pos0New[12], nrm0New[12];
      std::memcpy(pos0New, g_patch.data() + offset + posAt, sizeof(pos0New));
      std::memcpy(nrm0New, g_patch.data() + offset + nrmAt, sizeof(nrm0New));
      for (uint32_t s = 0; s < PnSlots; ++s) {
        if ((m.mask & (1u << s)) == 0) {
          continue;
        }
        auto* pos = reinterpret_cast<float*>(g_patch.data() + offset + posAt + s * 48);
        auto* nrm = reinterpret_cast<float*>(g_patch.data() + offset + nrmAt + s * 48);
        float to[12];
        std::memcpy(to, pos, sizeof(to));
        blend_matrix(m.fromPos[s], to, f, pos);
        std::memcpy(to, nrm, sizeof(to));
        blend_matrix(m.fromNrm[s], to, f, nrm);
      }
      // item (b): post-transform texture matrices fed by texture coordinates (a scroll, turn or scale), a plain blend
      // (plan_models)
      for (uint32_t k = 0; k < PtSlots && m.texMask != 0; ++k) {
        if ((m.texMask & (1u << k)) == 0) {
          continue;
        }
        auto* tex = reinterpret_cast<float*>(g_patch.data() + offset + block.postMtxOffset + k * 48);
        for (int n = 0; n < 12; ++n) {
          tex[n] = static_cast<float>(m.fromTex[k][n] + (static_cast<double>(tex[n]) - m.fromTex[k][n]) * f);
        }
      }
      // the colour-data blend (2026-11-18; plan_models): each colour float that differs from the step before's, `f` of
      // the way; the parts that must not blend hold the newest in fromColor, so they compare equal and stay as written
      if (m.colorFloats != 0 && block.colorAt + m.colorFloats * 4 <= block.bytes) {
        auto* col = g_patch.data() + offset + block.colorAt;
        for (uint32_t k = 0; k < m.colorFloats; ++k) {
          float to;
          std::memcpy(&to, col + k * 4, 4);
          if (std::memcmp(&to, &m.fromColor[k], 4) == 0) {
            continue;
          }
          const float v = static_cast<float>(m.fromColor[k] + (static_cast<double>(to) - m.fromColor[k]) * f);
          std::memcpy(col + k * 4, &v, 4);
        }
      }
      // ---- Round 5 (2026-11-17): texture matrices that carry the piece's world matrix follow it. ----
      // A texture coordinate made from the vertex position (GX_TG_POS) or normal goes through a texture matrix the
      // game built with the piece's world matrix in it: a projected shadow on the ground under it (the shadow's
      // projection times the receiver's world matrix: HSD's shadow code), a projected light, a reflection. Texture
      // matrices are held at the newest step (plan section 4), so when such a piece blended, its shadow was mapped as
      // if the piece stood at its newest place: on the bike rides the ground model moves under the bike every step,
      // and the in-betweens showed the bike's shadow as a wrong, blurred shape (verified, snatcherLeo's 2026-11-17 sitting,
      // ride 1 frame 16,442: unblended replays were exact, blended ones lost the shadow). Now such a texture matrix
      // gets the piece's own move: T' = T_new x M_new^-1 x M_blend (for normals the 3x3 parts), so the texture is
      // mapped where the piece is drawn. Only for a piece drawing with one matrix (a rigid piece: slot 0); an
      // envelope piece's texture matrix per vertex isn't known here (none found on a receiver). Plain UV scrolls
      // (texture coordinates from UVs) are not touched.
      if ((block.texPosMask | block.texNrmMask) != 0 && (m.mask & 1u) != 0 &&
          (block.modelSlots == 1u || block.modelSlots == 0u)) {
        const Aff mNew = aff_from_floats(pos0New);
        const Aff mBlend = aff_from_floats(reinterpret_cast<const float*>(g_patch.data() + offset + posAt));
        const Aff nNew = aff_from_floats(nrm0New);
        const Aff nBlend = aff_from_floats(reinterpret_cast<const float*>(g_patch.data() + offset + nrmAt));
        Aff dPos{}, dNrm{};
        if (aff_inverse(mNew, dPos) && aff_inverse(nNew, dNrm)) {
          dPos = aff_mul(dPos, mBlend);
          dNrm = aff_mul_rot(dNrm, nBlend);
          for (uint32_t k = 0; k < PnSlots; ++k) {
            const bool byPos = (block.texPosMask & (1u << k)) != 0;
            const bool byNrm = (block.texNrmMask & (1u << k)) != 0;
            if (!byPos && !byNrm) {
              continue;
            }
            auto* tex = reinterpret_cast<float*>(g_patch.data() + offset + texAt + k * 48);
            const Aff t = byPos ? aff_mul(aff_from_floats(tex), dPos) : aff_mul_rot(aff_from_floats(tex), dNrm);
            for (int r = 0; r < 3; ++r) {
              for (int c = 0; c < 4; ++c) {
                tex[r * 4 + c] = static_cast<float>(t[r][c]);
              }
            }
          }
          ++g_lastTexFollows;
        }
      }
      // the capture prototype's warp blend (2026-11-18; plan_models): the texture matrices on its texture coordinates
      // and its indirect matrices, `f` of the way from the step before's
      for (uint32_t k = 0; k < PnSlots && m.uvMtxMask != 0; ++k) {
        if ((m.uvMtxMask & (1u << k)) == 0) {
          continue;
        }
        auto* tex = reinterpret_cast<float*>(g_patch.data() + offset + texAt + k * 48);
        for (int n = 0; n < 12; ++n) {
          tex[n] = static_cast<float>(m.fromUvMtx[k][n] + (static_cast<double>(tex[n]) - m.fromUvMtx[k][n]) * f);
        }
      }
      if (m.indBlend && block.indAt != 0 && block.indAt + 96 <= block.bytes) {
        auto* ind = reinterpret_cast<float*>(g_patch.data() + offset + block.indAt);
        for (int n = 0; n < 24; ++n) {
          ind[n] = static_cast<float>(m.fromInd[n] + (static_cast<double>(ind[n]) - m.fromInd[n]) * f);
        }
      }
      ++modelWrites;
    }
    if (aimed) {
      auto* projBytes = g_patch.data() + offset + block.projOffset;
      // (Were another projection's block ever re-aimed again: the stored rows are uniform_projection(P), a
      // left-multiply of P, so uniform_projection(P x A) = stored x A.)
      const Mat4f proj = aims[block.camera].camProj;
      std::memcpy(projBytes, &proj, sizeof(proj));
    }
    g_map.emplace_back(block.uniformOffset, static_cast<uint32_t>(offset));
  }
  // ---- round 11, vertex mode (the pause menu): each moving piece's vertex span, blended, written past the step's own
  // vertices in the same vertex buffer (bound whole), so its draws need only another start (override_vertices). The
  // step's vertices stay as they were (its own picture, the restore and later in-betweens read them unchanged); the
  // buffer's later steps overwrite the area only after this step's in-betweens (the kept step's ordering, frame.cpp).
  uint32_t writtenVtx = 0;
  if (vtx2d) {
    const uint64_t base = (static_cast<uint64_t>(step.stats.lastVertSize) + 255) / 256 * 256;
    for (const auto& piece : g_2dVtx) {
      const uint64_t at = (g_vpatch.size() + 3) / 4 * 4;
      const uint64_t span = piece.spanEnd - piece.spanStart;
      if (base + at + span > VertexBufferSize) {
        ++overflow;
        continue;
      }
      g_vpatch.resize(at + span, 0);
      for (const auto& [now, before] : piece.chunks) {
        const auto& c = step.orre2dChunks[now];
        uint8_t* dst = g_vpatch.data() + at + (c.vtxOffset - piece.spanStart);
        const uint8_t* src = step.orre2dChunkBytes.data() + c.bytesOffset;
        std::memcpy(dst, src, c.length);
        if (before >= 0) {
          const auto& o = g_from2dChunks[static_cast<size_t>(before)];
          blend_vertices(dst, g_from2dChunkBytes.data() + o.bytesOffset, src, c.length, c.layout, f);
        }
        g_vmap.emplace_back(c.vtxOffset, static_cast<uint32_t>(base + at + (c.vtxOffset - piece.spanStart)));
      }
      ++writtenVtx;
    }
    if (!g_vpatch.empty()) {
      webgpu::g_queue.WriteBuffer(detail::resources().vertexBuffer, base, g_vpatch.data(), g_vpatch.size());
      std::sort(g_vmap.begin(), g_vmap.end());
    }
  }
  // round 11: a picture with a blended 2D piece counts as "moved"
  g_lastModelWrites = modelWrites + written2d + writtenVtx;
  g_stats.modelBlockWrites.fetch_add(modelWrites, std::memory_order_relaxed);
  g_stats.p2Written.fetch_add(written2d, std::memory_order_relaxed);
  g_stats.p2VtxWritten.fetch_add(writtenVtx, std::memory_order_relaxed);
  g_stats.texFollows.fetch_add(g_lastTexFollows, std::memory_order_relaxed); // round 5
  g_stats.looseSameFrustum.fetch_add(looseSame, std::memory_order_relaxed);
  g_stats.looseTagged.fetch_add(looseTagged, std::memory_order_relaxed);
  if (g_map.empty() && g_vmap.empty()) {
    return 0;
  }
  // blocks are pushed in rising offsets within a step, so the map is sorted already; sort anyway (cheap) to be sure
  if (!std::is_sorted(g_map.begin(), g_map.end())) {
    std::sort(g_map.begin(), g_map.end());
  }
  // queue.WriteBuffer is ordered with the submits on this queue: the earlier in-between pictures read their own copies
  if (!g_patch.empty()) {
    webgpu::g_queue.WriteBuffer(g_blendBuffer, 0, g_patch.data(), g_patch.size());
  }
  g_stats.blocks.fetch_add(g_map.size(), std::memory_order_relaxed);
  g_stats.looseBlocks.fetch_add(loose, std::memory_order_relaxed);
  g_stats.overflow.fetch_add(overflow, std::memory_order_relaxed);
  g_stats.bytes.fetch_add(g_patch.size() + g_vpatch.size(), std::memory_order_relaxed);
  return static_cast<uint32_t>(g_map.size() + g_vmap.size());
}

void begin_override() noexcept { g_overrideActive = !g_map.empty() || !g_vmap.empty(); }
void end_override() noexcept { g_overrideActive = false; }

// round 11, vertex mode (smooth_camera.hpp): a draw of a blended pause-menu piece reads its vertices from the copy
bool override_vertices(uint32_t& vtxStart) noexcept {
  if (!g_overrideActive || g_vmap.empty()) {
    return false;
  }
  const auto it = std::lower_bound(g_vmap.begin(), g_vmap.end(), std::pair<uint32_t, uint32_t>{vtxStart, 0});
  if (it == g_vmap.end() || it->first != vtxStart) {
    return false;
  }
  vtxStart = it->second;
  return true;
}

bool override_uniform(uint32_t& offset, wgpu::BindGroup& group) noexcept {
  if (!g_overrideActive) {
    return false;
  }
  const auto it = std::lower_bound(g_map.begin(), g_map.end(), std::pair<uint32_t, uint32_t>{offset, 0});
  if (it == g_map.end() || it->first != offset) {
    return false;
  }
  offset = it->second;
  group = g_blendBindGroup;
  return true;
}

Stats stats() noexcept {
  Stats out;
  constexpr auto r = std::memory_order_relaxed;
  out.cameras = g_stats.cameras.load(r);
  for (int i = 0; i < static_cast<int>(Cut::Count); ++i) {
    out.cuts[i] = g_stats.cuts[i].load(r);
  }
  out.blocks = g_stats.blocks.load(r);
  out.looseBlocks = g_stats.looseBlocks.load(r);
  out.overflow = g_stats.overflow.load(r);
  out.bytes = g_stats.bytes.load(r);
  out.modelTagged = g_stats.modelTagged.load(r);
  out.modelUntagged = g_stats.modelUntagged.load(r);
  out.modelMatched = g_stats.modelMatched.load(r);
  out.modelMoved = g_stats.modelMoved.load(r);
  out.modelSnapped = g_stats.modelSnapped.load(r);
  out.modelHeld = g_stats.modelHeld.load(r);
  out.modelsSnapped = g_stats.modelsSnapped.load(r);
  out.modelBorrowed = g_stats.modelBorrowed.load(r);
  out.texFollows = g_stats.texFollows.load(r);
  out.modelBlockWrites = g_stats.modelBlockWrites.load(r);
  out.looseSameFrustum = g_stats.looseSameFrustum.load(r);
  out.looseTagged = g_stats.looseTagged.load(r);
  out.particleTagged = g_stats.particleTagged.load(r); // item (a)
  out.particleBlended = g_stats.particleBlended.load(r);
  out.particleNew = g_stats.particleNew.load(r);
  out.particleReused = g_stats.particleReused.load(r);
  out.particleJumped = g_stats.particleJumped.load(r);
  out.particleHeld = g_stats.particleHeld.load(r);
  out.scrollBlended = g_stats.scrollBlended.load(r); // item (b)
  out.scrollWrapped = g_stats.scrollWrapped.load(r);
  out.scrollSnapped = g_stats.scrollSnapped.load(r);
  out.scrollUnsteady = g_stats.scrollUnsteady.load(r); // round 9
  out.lockedBlocks = g_stats.lockedBlocks.load(r);     // round 10
  out.p2Tagged = g_stats.p2Tagged.load(r);             // round 11
  out.p2Blended = g_stats.p2Blended.load(r);
  out.p2New = g_stats.p2New.load(r);
  out.p2Swapped = g_stats.p2Swapped.load(r);
  out.p2Written = g_stats.p2Written.load(r);
  out.p2NoSpan = g_stats.p2NoSpan.load(r);
  out.p2VtxDraws = g_stats.p2VtxDraws.load(r);
  out.p2VtxSnapped = g_stats.p2VtxSnapped.load(r);
  out.p2MtxBlocks = g_stats.p2MtxBlocks.load(r);
  out.p2VtxWritten = g_stats.p2VtxWritten.load(r);
  out.twoStepBlocks = g_stats.twoStepBlocks.load(r); // item (c)
  out.twoStepModels = g_stats.twoStepModels.load(r);
  out.colorBlended = g_stats.colorBlended.load(r); // the colour-data blend
  out.colorHeld = g_stats.colorHeld.load(r);
  out.cornerParticles = g_stats.cornerParticles.load(r); // (3b)
  return out;
}

CutInfo last_cut() noexcept {
  std::lock_guard lock{g_cutLock};
  return g_lastCut;
}

void shutdown() noexcept {
  g_overrideActive = false;
  g_map.clear();
  g_patch.clear();
  g_plan.clear();
  g_models.clear();
  g_blockModel.clear();
  g_blendBindGroup = {};
  g_blendBuffer = {};
}

} // namespace aurora::gfx::smooth_camera
