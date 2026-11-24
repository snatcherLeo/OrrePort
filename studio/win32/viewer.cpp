// viewer.cpp: Studio's small 3D view: a species' battle model in its bind pose, turning in the centre of the page.
//
// Why WebGPU through Dawn: it is the GPU layer Aurora (the port's renderer) uses, so this view can later move onto the
// port's own renderer and show models exactly as the game draws them (the plan's "3D view through the game's own
// graphics path"). Until then it reproduces the game's materials itself, following the game's own code:
//   - texture stages combined as the TEV setup does (TObjMakeTExp, src/hsd/tobj.c; the formulas are in
//     docs/engine/hsd-models.md "Texture coordinates and texture stages"): each stage's colour and alpha operation,
//     applied to the lighting channel it maps (diffuse before lighting, ext after it);
//   - each stage's own coordinates: its transform (MakeTextureMtx), its wrap modes (clamp / repeat / mirror), and
//     reflection-style coordinates from the normal for stages that use them;
//   - the material's pixel-engine settings (HSD_SetupPEMode, src/hsd/hsd_texp.c): blending, alpha test, depth test and
//     depth writes, with the render mode's defaults when a material has none (translucent: blend by alpha, keep
//     pixels with alpha above 0; opaque: no blending and NO alpha test);
//   - culling per polygon object (HSD_PObjDisp, src/hsd/pobj.c);
//   - vertex colours where the render mode asks for them (RENDER_DIFFUSE_VTX / ALPHA_VTX).
// Not yet (each is a known gap): the game's lights (one fixed light here),
// specular and ambient-channel stages, TEV overrides (TObjDesc +0x58), stage rotation, toon / shadow / gradation /
// backlight coordinates, shiny recolouring, animations.
//
// Idle means idle: frames are drawn only while the model spins (a 60 Hz timer that stops when spinning is turned off,
// the window is hidden or minimised) or when the user drags, zooms or the window needs repainting.
//
// Platforms: Windows-only parts are the child window and the HWND surface (wgpu::SurfaceSourceWindowsHWND). Linux
// (Qt): a QWindow with SurfaceSourceXlibWindow / SurfaceSourceWaylandSurface; macOS (AppKit): an NSView's CAMetalLayer
// with SurfaceSourceMetalLayer. The WebGPU code itself is the same everywhere.
#include <webgpu/webgpu_cpp.h>
#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <map>
#include <random>
#include <thread>

#include "app.hpp"

namespace studio {

namespace {
const char* kShader = R"(
// The game's lighting (in-game lighting, 2026-11-19; the port map: GX's per-vertex channels as HSD sets them up,
// src/hsd/state.c HSD_SetupChannelMode, src/hsd/lobj.c): arena places the model in the arena's world (the move viewer's
// x -40, turned +90 degrees, or the game camera's logged placement), eye is the camera there (w: 1 = in-game lighting
// on), amb the ambient light's colour, and per light (8: GX's limit) its position (w: 1 infinite, 2 point, 0 none), its
// colour (a: 1 diffuse, 2 specular slot, 3 both) and its distance attenuation k0, k1, k2
struct U { mvp: mat4x4f, model: mat4x4f, hi: vec4f, lo: vec4f, size: vec4f, arena: mat4x4f, eye: vec4f, amb: vec4f,
           lpos: array<vec4f, 8>, lcol: array<vec4f, 8>, lk: array<vec4f, 8>, hl: vec4f, tint: vec4f, ghost: vec4f, }
// (ghost.x: the alpha a blur's after-image is drawn with, 1 otherwise; _blurRender's GSmaterialSetAlpha)
// (tint: a move's colour fade on this Pokémon, release step 4: the final colour x 2 x tint (0.5 = none), as GS's
// modulation stage after the material's whole result; _matGSmatObjMakeTExp, the fade spec)
// (hl: the highlight texture coordinates' half vector in view space, from the first infinite light, w 1 = set; Game
// lighting only)
@group(0) @binding(0) var<uniform> u: U;

// One material (filled by material_uniform below; every value from the disc's material and its texture stages).
struct M {
  diffuse: vec4f,            // material diffuse RGB, material alpha
  ambient: vec4f,
  mode: vec4f,               // x diffuse source, y alpha source (0/1 material, 2 vertex, 3 both), z lit (1/0), w stages
  at: vec4f,                 // alpha test: comp0, ref0, op, comp1 (GXCompare, refs 0..1, GXAlphaOp)
  at2: vec4f,                // x ref1
  spec: vec4f,               // the specular highlight's colour and shininess (zero: none; RENDER_SPECULAR)
  // per stage (8: GX's limit; the most a Pokémon uses is 5, Spinda and Deoxys):
  xf: array<vec4f, 8>,       // su, sv, tu, tv
  op: array<vec4f, 8>,       // coordinate source, lighting channels, colour op, alpha op
  k: array<vec4f, 8>,        // x blending factor, y coordinate set (0..3: GX_TG_TEX0..TEX3)
  // the stage's TEV override (OrreTexStage.tev; MakeColorGenTExp, src/hsd/tobj.c): input codes, settings, colours
  tc: array<vec4f, 8>,       // colour inputs a, b, c, d (GXTevColorArg or TOBJ_TEV_CC_*)
  ta: array<vec4f, 8>,       // alpha inputs a, b, c, d (GXTevAlphaArg or TOBJ_TEV_CA_*)
  tp: array<vec4f, 8>,       // colour op, bias, scale, flags (1 colour on, 2 alpha on, 4 colour clamp, 8 alpha clamp)
  tq: array<vec4f, 8>,       // alpha op, bias, scale
  kc: array<vec4f, 8>,       // konst colour
  r0: array<vec4f, 8>,       // register tev0
  r1: array<vec4f, 8>,       // register tev1
  // the shiny recolour (OrreModel.shiny_*; GS's material extensions, _matGSmatObjMakeTExp): the channel swap's
  // source per R, G, B, A, and the modulation colour (0x7F / 255 = x1 after the doubling); flags in shon
  shsw: vec4f,
  shmod: vec4f,
  shon: vec4f,               // x swap on, y modulation on
}
@group(1) @binding(0) var<uniform> m: M;
@group(1) @binding(1) var s0: sampler;
@group(1) @binding(2) var s1: sampler;
@group(1) @binding(3) var s2: sampler;
@group(1) @binding(4) var s3: sampler;
@group(1) @binding(5) var s4: sampler;
@group(1) @binding(6) var s5: sampler;
@group(1) @binding(7) var s6: sampler;
@group(1) @binding(8) var s7: sampler;
@group(1) @binding(9) var t0: texture_2d<f32>;
@group(1) @binding(10) var t1: texture_2d<f32>;
@group(1) @binding(11) var t2: texture_2d<f32>;
@group(1) @binding(12) var t3: texture_2d<f32>;
@group(1) @binding(13) var t4: texture_2d<f32>;
@group(1) @binding(14) var t5: texture_2d<f32>;
@group(1) @binding(15) var t6: texture_2d<f32>;
@group(1) @binding(16) var t7: texture_2d<f32>;

struct VOut { @builtin(position) pos: vec4f, @location(0) uv: vec2f, @location(1) n: vec3f, @location(2) c: vec4f,
              @location(3) uv1: vec2f, @location(4) uv2: vec2f, @location(5) uv3: vec2f, @location(6) uv4: vec2f,
              @location(7) c0: vec4f, @location(8) c1: vec4f, }

// The shiny channel swap (GS's extension bit 2: every TEV stage's swap table, so every texture and raster colour read;
// src/hsd/texp.c): each output channel takes the source channel the model's colour block names. Identity when off.
fn swz(c: vec4f) -> vec4f {
  if (m.shon.x < 0.5) { return c; }
  return vec4f(c[u32(m.shsw.x)], c[u32(m.shsw.y)], c[u32(m.shsw.z)], c[u32(m.shsw.w)]);
}
// The shiny modulation (extension bit 1), the material's last stage: colour = 2 x modulation x colour, clamped
// (scale 2; 0x7F = unchanged). Alpha is left alone: the alpha half of that stage is not settled (guess; the lighting
// map's open question 1)
fn shiny_mod(rgb: vec3f) -> vec3f {
  var c = rgb;
  if (m.shon.y >= 0.5) { c = clamp(c * 2.0 * m.shmod.rgb, vec3f(0.0), vec3f(1.0)); }
  // a move's colour fade on the Pokémon, last
  return clamp(c * 2.0 * u.tint.rgb, vec3f(0.0), vec3f(1.0));
}

// GX's lit channels for one vertex, as HSD sets them up (the port map, sections 1-3; formula as Dolphin's
// LightingShaderGen, likely exact apart from the 8-bit rounding): COLOR0 = clamp(material ambient x ambient light +
// sum of the diffuse lights' colour x attenuation x max(0, N.L)) x (white, or the vertex colour for a vertex diffuse
// source); COLOR1 = clamp(sum over the specular slots of colour x x^2 / (s/2 + (1 - s/2) x^2)), x = N.H (0 facing away),
// s the material's shininess. Per vertex, then interpolated (Gouraud), as the GameCube does. Approximation (likely
// small): the half vector H uses each vertex's direction to the eye, where the game takes it once per joint
fn gx_light(P: vec3f, N: vec3f, c: vec4f) -> array<vec4f, 2> {
  var lacc = m.ambient.rgb * u.amb.rgb;
  var sacc = vec3f(0.0);
  let s = m.spec.w;
  let toEye = normalize(u.eye.xyz - P);
  for (var i = 0u; i < 8u; i++) {
    let kind = u32(u.lpos[i].w);
    if (kind == 0u) { continue; }
    let flags = u32(u.lcol[i].a);
    var L = normalize(u.lpos[i].xyz);  // infinite: from the direction of its position, seen from the origin
    var attn = 1.0;
    if (kind == 2u) {
      let v = u.lpos[i].xyz - P;
      let d = length(v);
      L = v / max(d, 0.0001);
      attn = 1.0 / max(u.lk[i].x + u.lk[i].y * d + u.lk[i].z * d * d, 0.0001);
    }
    let nl = dot(N, L);
    if ((flags & 1u) != 0u) { lacc += u.lcol[i].rgb * attn * max(nl, 0.0); }
    if ((flags & 2u) != 0u && s > 0.0 && nl >= 0.0) {
      let H = normalize(L + toEye);
      let x = max(dot(N, H), 0.0);
      sacc += u.lcol[i].rgb * (x * x) / max(s * 0.5 + (1.0 - s * 0.5) * x * x, 0.0001);
    }
  }
  var mat = vec3f(1.0);
  if (u32(m.mode.x) >= 2u) { mat = c.rgb; }  // (RENDER_DIFFUSE_VTX / BOTH: the vertex colour is the channel's material)
  return array<vec4f, 2>(vec4f(clamp(lacc, vec3f(0.0), vec3f(1.0)) * mat, 1.0), vec4f(clamp(sacc, vec3f(0.0), vec3f(1.0)), 1.0));
}

@vertex fn vs(@location(0) p: vec3f, @location(1) n: vec3f, @location(2) uv: vec2f, @location(3) c: vec4f,
              @location(4) uv1: vec2f, @location(5) uv2: vec2f, @location(6) uv3: vec2f, @location(7) uv4: vec2f) -> VOut {
  var o: VOut;
  o.pos = u.mvp * vec4f(p, 1.0);
  o.uv = uv;
  o.n = (u.model * vec4f(n, 0.0)).xyz;
  o.c = c;
  o.uv1 = uv1;
  o.uv2 = uv2;
  o.uv3 = uv3;
  o.uv4 = uv4;
  o.c0 = vec4f(1.0);
  o.c1 = vec4f(0.0);
  if (u.eye.w > 0.5) {
    let P = (u.arena * vec4f(p, 1.0)).xyz;
    var N = (u.arena * vec4f(n, 0.0)).xyz;
    let nl = length(N);
    if (nl > 0.0001) { N = N / nl; }
    let ch = gx_light(P, N, swz(c));
    o.c0 = ch[0];
    o.c1 = ch[1];
  }
  return o;
}

// a stage's coordinates: 0 UV (from the coordinate set the stage names) through its transform; 1 reflection and
// 2 highlight from the view-space normal (an approximation of the game's normal-based texgen); anything else: the
// plain UV (that stage is skipped below)
fn stage_uv(i: u32, v: VOut, n: vec3f) -> vec2f {
  let coord = u32(m.op[i].x);
  var uv = v.uv;
  let uv_set = u32(m.k[i].y);  // ("set" is a reserved word in WGSL)
  if (uv_set == 1u) { uv = v.uv1; }
  if (uv_set == 2u) { uv = v.uv2; }
  if (uv_set == 3u) { uv = v.uv3; }
  if (uv_set == 4u) { uv = v.uv4; }
  if (coord == 0u) { return (uv + m.xf[i].zw) * m.xf[i].xy; }
  // highlight (2) with Game lighting: HSD's own texgen (tobj.c; the lighting map section 4): the view-space normal
  // against the half vector of the first infinite light and the view direction, x = 0.5 + 0.5 N.H, in both coordinates,
  // then the stage's own transform
  if (coord == 2u && u.hl.w > 0.5) {
    let x = 0.5 + 0.5 * dot(n, u.hl.xyz);
    return (vec2f(x, x) + m.xf[i].zw) * m.xf[i].xy;
  }
  if (coord == 1u || coord == 2u) { return vec2f(n.x * 0.5 + 0.5, -n.y * 0.5 + 0.5); }
  return uv;
}
fn stage_used(i: u32) -> bool {
  let coord = u32(m.op[i].x);
  return i < u32(m.mode.w) && coord <= 2u;
}
// The TEV override's inputs (MakeColorGenTExp): GX's own colour args (8 TEXC, 9 TEXA, 12 ONE, 13 HALF, 15 ZERO) and
// HSD's extra ones (0x80 | n: konst RGB / RRR / GGG / BBB / AAA, tev0 RGB / AAA, tev1 RGB / AAA)
fn tev_c(i: u32, code: u32, t: vec4f) -> vec3f {
  switch code {
    case 8u: { return t.rgb; }
    case 9u: { return vec3f(t.a); }
    case 12u: { return vec3f(1.0); }
    case 13u: { return vec3f(0.5); }
    case 128u: { return m.kc[i].rgb; }
    case 129u: { return vec3f(m.kc[i].r); }
    case 130u: { return vec3f(m.kc[i].g); }
    case 131u: { return vec3f(m.kc[i].b); }
    case 132u: { return vec3f(m.kc[i].a); }
    case 133u: { return m.r0[i].rgb; }
    case 134u: { return vec3f(m.r0[i].a); }
    case 135u: { return m.r1[i].rgb; }
    case 136u: { return vec3f(m.r1[i].a); }
    default: { return vec3f(0.0); }
  }
}
// alpha args: 4 TEXA, 7 ZERO; HSD's 0x40 | n: konst R / G / B / A, tev0 A, tev1 A
fn tev_a(i: u32, code: u32, t: vec4f) -> f32 {
  switch code {
    case 4u: { return t.a; }
    case 64u: { return m.kc[i].r; }
    case 65u: { return m.kc[i].g; }
    case 66u: { return m.kc[i].b; }
    case 67u: { return m.kc[i].a; }
    case 68u: { return m.r0[i].a; }
    case 69u: { return m.r1[i].a; }
    default: { return 0.0; }
  }
}
// GX's TEV stage: (d +/- ((1 - c) a + c b) + bias) x scale; op 0 add, 1 subtract; bias 0 none, 1 +1/2, 2 -1/2;
// scale 0 x1, 1 x2, 2 x4, 3 x1/2
fn tev_bias(b: u32) -> f32 { if (b == 1u) { return 0.5; } if (b == 2u) { return -0.5; } return 0.0; }
fn tev_scale(s: u32) -> f32 { if (s == 1u) { return 2.0; } if (s == 2u) { return 4.0; } if (s == 3u) { return 0.5; } return 1.0; }
// a stage's texture value after its TEV override, where it has one
fn tev_apply(i: u32, t: vec4f) -> vec4f {
  let flags = u32(m.tp[i].w);
  var r = t;
  if ((flags & 1u) != 0u) {
    let a = tev_c(i, u32(m.tc[i].x), t);
    let b = tev_c(i, u32(m.tc[i].y), t);
    let c = tev_c(i, u32(m.tc[i].z), t);
    let d = tev_c(i, u32(m.tc[i].w), t);
    var v = mix(a, b, c);
    if (u32(m.tp[i].x) == 1u) { v = d - v; } else { v = d + v; }
    v = (v + tev_bias(u32(m.tp[i].y))) * tev_scale(u32(m.tp[i].z));
    if ((flags & 4u) != 0u) { v = clamp(v, vec3f(0.0), vec3f(1.0)); }
    r = vec4f(v, r.a);
  }
  if ((flags & 2u) != 0u) {
    let a = tev_a(i, u32(m.ta[i].x), t);
    let b = tev_a(i, u32(m.ta[i].y), t);
    let c = tev_a(i, u32(m.ta[i].z), t);
    let d = tev_a(i, u32(m.ta[i].w), t);
    var v = mix(a, b, c);
    if (u32(m.tq[i].x) == 1u) { v = d - v; } else { v = d + v; }
    v = (v + tev_bias(u32(m.tq[i].y))) * tev_scale(u32(m.tq[i].z));
    if ((flags & 8u) != 0u) { v = clamp(v, 0.0, 1.0); }
    r.a = v;
  }
  return r;
}

// TObjMakeTExp's colour operations (TEX_COLORMAP_*: 1 alpha mask, 2 rgb mask, 3 blend, 4 modulate, 5 replace, 6 pass,
// 7 add, 8 sub); T = the texture, k = the stage's blending factor
fn colour_op(prev: vec3f, t: vec4f, op: u32, k: f32) -> vec3f {
  switch op {
    case 1u: { return prev * (1.0 - t.a) + t.rgb * t.a; }
    case 2u: { return prev * (1.0 - t.rgb) + t.rgb * t.rgb; }
    case 3u: { return prev * (1.0 - k) + t.rgb * k; }
    case 4u: { return prev * t.rgb; }
    case 5u: { return t.rgb; }
    case 7u: { return clamp(prev + t.rgb, vec3f(0.0), vec3f(1.0)); }
    case 8u: { return clamp(prev - t.rgb, vec3f(0.0), vec3f(1.0)); }
    default: { return prev; }
  }
}
// the alpha operations (TEX_ALPHAMAP_*: 1 alpha mask, 2 blend, 3 modulate, 4 replace, 5 pass, 6 add, 7 sub)
fn alpha_op(prev: f32, t: vec4f, op: u32, k: f32) -> f32 {
  switch op {
    case 1u: { return prev * (1.0 - t.a) + t.a * t.a; }
    case 2u: { return prev * (1.0 - k) + t.a * k; }
    case 3u: { return prev * t.a; }
    case 4u: { return t.a; }
    case 6u: { return clamp(prev + t.a, 0.0, 1.0); }
    case 7u: { return clamp(prev - t.a, 0.0, 1.0); }
    default: { return prev; }
  }
}
// GXCompare: 0 never, 1 less, 2 equal, 3 lequal, 4 greater, 5 nequal, 6 gequal, 7 always (on 8-bit values)
fn compare(a: f32, comp: u32, reference: f32) -> bool {
  let x = round(a * 255.0);
  let r = round(reference * 255.0);
  switch comp {
    case 0u: { return false; }
    case 1u: { return x < r; }
    case 2u: { return x == r; }
    case 3u: { return x <= r; }
    case 4u: { return x > r; }
    case 5u: { return x != r; }
    case 6u: { return x >= r; }
    default: { return true; }
  }
}

@fragment fn fs(i: VOut) -> @location(0) vec4f {
  let len = length(i.n);
  var nv = vec3f(0.0, 0.0, 1.0);
  if (len > 0.0001) { nv = i.n / len; }
  // every stage sampled up front (WGSL wants texture sampling outside non-uniform branches)
  var tex: array<vec4f, 8>;
  tex[0] = textureSample(t0, s0, stage_uv(0u, i, nv));
  tex[1] = textureSample(t1, s1, stage_uv(1u, i, nv));
  tex[2] = textureSample(t2, s2, stage_uv(2u, i, nv));
  tex[3] = textureSample(t3, s3, stage_uv(3u, i, nv));
  tex[4] = textureSample(t4, s4, stage_uv(4u, i, nv));
  tex[5] = textureSample(t5, s5, stage_uv(5u, i, nv));
  tex[6] = textureSample(t6, s6, stage_uv(6u, i, nv));
  tex[7] = textureSample(t7, s7, stage_uv(7u, i, nv));
  for (var s = 0u; s < 8u; s++) { tex[s] = tev_apply(s, swz(tex[s])); }
  let vc = swz(i.c);  // the vertex colour, through the shiny swap too
  // the diffuse channel's starting colour and alpha (RENDER_DIFFUSE_* / RENDER_ALPHA_*)
  let ds = u32(m.mode.x);
  let as_ = u32(m.mode.y);
  // In-game lighting on a lit material: the game's TEV order (MObjMakeTExp, src/hsd/mobj.c; the port map section 4):
  // base = the material diffuse (1 for a vertex diffuse source, whose colour is in COLOR0); the diffuse- and
  // ambient-group stages; x COLOR0; + (the material specular through the specular-group stages) x COLOR1; then the
  // ext-group stages
  if (u.eye.w > 0.5 && m.mode.z > 0.5) {
    var gdif = m.diffuse.rgb;
    if (ds == 2u) { gdif = vec3f(1.0); }
    var galpha = m.diffuse.a;
    if (as_ == 2u) { galpha = vc.a; }
    if (as_ == 3u) { galpha = m.diffuse.a * vc.a; }
    for (var s = 0u; s < 8u; s++) {
      if (!stage_used(s)) { continue; }
      let lm = u32(m.op[s].y);
      if ((lm & 5u) != 0u) {
        gdif = colour_op(gdif, tex[s], u32(m.op[s].z), m.k[s].x);
        galpha = alpha_op(galpha, tex[s], u32(m.op[s].w), m.k[s].x);
      }
    }
    var grgb = gdif * i.c0.rgb;
    if (m.spec.w > 0.0) {
      var sp = m.spec.rgb;
      for (var s = 0u; s < 8u; s++) {
        if (!stage_used(s)) { continue; }
        if ((u32(m.op[s].y) & 2u) != 0u) { sp = colour_op(sp, tex[s], u32(m.op[s].z), m.k[s].x); }
      }
      grgb = clamp(grgb + sp * i.c1.rgb, vec3f(0.0), vec3f(1.0));
    }
    for (var s = 0u; s < 8u; s++) {
      if (!stage_used(s)) { continue; }
      let lm = u32(m.op[s].y);
      if ((lm & 8u) != 0u || lm == 0u) {
        grgb = colour_op(grgb, tex[s], u32(m.op[s].z), m.k[s].x);
        galpha = alpha_op(galpha, tex[s], u32(m.op[s].w), m.k[s].x);
      }
    }
    let g0 = compare(galpha, u32(m.at.x), m.at.y);
    let g1 = compare(galpha, u32(m.at.w), m.at2.x);
    var gpass = g0 && g1;
    if (u32(m.at.z) == 1u) { gpass = g0 || g1; }
    if (u32(m.at.z) == 2u) { gpass = g0 != g1; }
    if (u32(m.at.z) == 3u) { gpass = g0 == g1; }
    if (!gpass) { discard; }
    return vec4f(shiny_mod(grgb), clamp(galpha, 0.0, 1.0) * u.ghost.x);
  }
  var dif = m.diffuse.rgb;
  if (ds == 2u) { dif = vc.rgb; }
  if (ds == 3u) { dif = m.diffuse.rgb * vc.rgb; }
  var alpha = m.diffuse.a;
  if (as_ == 2u) { alpha = vc.a; }
  if (as_ == 3u) { alpha = m.diffuse.a * vc.a; }
  // stages on the diffuse channel (lighting bit 0) shape the colour before lighting
  for (var s = 0u; s < 8u; s++) {
    if (!stage_used(s)) { continue; }
    let lm = u32(m.op[s].y);
    if ((lm & 1u) != 0u) {
      dif = colour_op(dif, tex[s], u32(m.op[s].z), m.k[s].x);
      alpha = alpha_op(alpha, tex[s], u32(m.op[s].w), m.k[s].x);
    }
  }
  // lighting: one fixed light from the upper front (guess: the battle's own lights aren't read yet)
  var rgb = dif;
  if (m.mode.z > 0.5) {
    let L = normalize(vec3f(0.4, 0.8, 0.6));
    var d = 0.6;
    if (len > 0.0001) { d = max(dot(nv, L), 0.0); }
    rgb = dif * (0.45 + 0.65 * d);
    // the specular highlight (RENDER_SPECULAR): the material's specular colour, tightened by its shininess, from the
    // same light toward the camera (Blinn-Phong on the interpolated normal: an approximation of GX's per-vertex
    // specular channel; Grumpig's black pearls get their shine from it)
    if (m.spec.w > 0.0 && len > 0.0001) {
      let H = normalize(L + vec3f(0.0, 0.0, 1.0));
      rgb = rgb + m.spec.rgb * pow(max(dot(nv, H), 0.0), m.spec.w);
    }
  }
  // stages on the ext channel (bit 3), or on none, apply to the lit colour; specular (bit 1), ambient (bit 2) and
  // shadow (bit 4) stages are not applied yet
  for (var s = 0u; s < 8u; s++) {
    if (!stage_used(s)) { continue; }
    let lm = u32(m.op[s].y);
    if ((lm & 8u) != 0u || lm == 0u) {
      rgb = colour_op(rgb, tex[s], u32(m.op[s].z), m.k[s].x);
      alpha = alpha_op(alpha, tex[s], u32(m.op[s].w), m.k[s].x);
    }
  }
  // the alpha test (TExpStateSetAlphaCompare): comp0 against ref0, combined by op (0 and, 1 or, 2 xor, 3 xnor)
  let a0 = compare(alpha, u32(m.at.x), m.at.y);
  let a1 = compare(alpha, u32(m.at.w), m.at2.x);
  let op = u32(m.at.z);
  var pass_ = a0 && a1;
  if (op == 1u) { pass_ = a0 || a1; }
  if (op == 2u) { pass_ = a0 != a1; }
  if (op == 3u) { pass_ = a0 == a1; }
  if (!pass_) { discard; }
  return vec4f(shiny_mod(rgb), clamp(alpha, 0.0, 1.0) * u.ghost.x);
}

struct BOut { @builtin(position) pos: vec4f, }
@vertex fn bvs(@builtin(vertex_index) i: u32) -> BOut {
  var p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o: BOut;
  o.pos = vec4f(p[i], 0.0, 1.0);
  return o;
}
@fragment fn bfs(i: BOut) -> @location(0) vec4f {
  let q = i.pos.xy / u.size.xy - vec2f(0.5, 0.7);
  let d = clamp(length(q * vec2f(1.0, u.size.y / u.size.x * 1.4)) / 0.7, 0.0, 1.0);
  return mix(u.hi, u.lo, d);
}
)";

// The particles' shader (2026-11-19, particles step 1): one camera-facing quad per particle, built on the CPU by the
// core (orre_particles_quads, as psDispSub does), coloured as psSetupTev combines it (src/game/psdisptev.c; Studio's
// port map section 7, key = kind & (Lit 0x80000000 | Trail 0x100000 | Texture 0x400 | PrimEnv 0x80)), then the
// particle's own alpha test (GXSetAlphaCompare from its mode and refs). Lighting: Studio has no lights yet, so the lit
// colour channel is its material colour (prim, white with PrimEnv) at full light (an approximation; the four lit
// banks are Gastly's, Koffing's, Weezing's and Torkoal's). Its own module: the material group's bindings differ.
const char* kParticleShader = R"(
struct U { mvp: mat4x4f, model: mat4x4f, hi: vec4f, lo: vec4f, size: vec4f, }
@group(0) @binding(0) var<uniform> u: U;
@group(1) @binding(0) var ps: sampler;
@group(1) @binding(1) var pt: texture_2d<f32>;
struct PO { @builtin(position) pos: vec4f, @location(0) uv: vec2f, @location(1) ras: vec4f, @location(2) prim: vec4f,
            @location(3) env: vec4f, @location(4) mat: vec4f, @location(5) @interpolate(flat) flags: vec2u, }
@vertex fn pvs(@location(0) p: vec3f, @location(1) uv: vec2f, @location(2) ras: vec4f, @location(3) prim: vec4f,
               @location(4) env: vec4f, @location(5) mat: vec4f, @location(6) flags: vec2u) -> PO {
  var o: PO;
  o.pos = u.mvp * vec4f(p, 1.0);
  o.uv = uv; o.ras = ras; o.prim = prim; o.env = env; o.mat = mat; o.flags = flags;
  return o;
}
fn compare(a: f32, comp: u32, r: u32) -> bool {
  let x = u32(round(a * 255.0));
  switch comp {
    case 0u: { return false; }
    case 1u: { return x < r; }
    case 2u: { return x == r; }
    case 3u: { return x <= r; }
    case 4u: { return x > r; }
    case 5u: { return x != r; }
    case 6u: { return x >= r; }
    default: { return true; }
  }
}
@fragment fn pfs(i: PO) -> @location(0) vec4f {
  let kind = i.flags.x;
  let textured = (kind & 0x400u) != 0u;
  let primenv = (kind & 0x80u) != 0u;
  let trail = (kind & 0x100000u) != 0u;
  let lit = (kind & 0x80000000u) != 0u;
  // (sampled before any branch: WGSL allows textureSample only in uniform control flow; an untextured particle is
  // bound to a white picture anyway)
  // mirrored particles (kind 0x20 across, 0x40 down): the coordinates folded as GX_MIRROR wraps them (every other
  // period mirrored: m = x mod 2, then m below 1 as it is, else 2 - m)
  let ms = (kind & 0x20u) != 0u;
  let mt = (kind & 0x40u) != 0u;
  let fm = vec2f(1.0) - abs(fract(i.uv * 0.5) * 2.0 - vec2f(1.0));
  let uvm = vec2f(select(i.uv.x, fm.x, ms), select(i.uv.y, fm.y, mt));
  let ts = textureSample(pt, ps, uvm);
  let t = select(vec4f(1.0), ts, textured);
  var c = i.prim;
  if (!lit) {
    if (!trail) {
      // C1 + (C0 - C1) x tex (C1 = 0 without PrimEnv: prim x tex); no texture: C0
      if (textured) { c = mix(i.env, i.prim, t); }
    } else {
      if (!textured) { c = i.ras; }
      else if (!primenv) { c = t * i.ras; }
      else { c = vec4f(mix(i.env.rgb, i.prim.rgb, t.rgb), mix(i.env.a, i.prim.a, t.a) * i.ras.a); }
    }
  } else {
    // C2 x ras x (lerp(C1, C0, tex) | tex | C0); alpha A2 x (...) (x the trail's ras alpha)
    var b = i.prim;
    if (textured) { if (primenv) { b = mix(i.env, i.prim, t); } else { b = t; } }
    c = vec4f(i.mat.rgb * i.ras.rgb * b.rgb, i.mat.a * b.a);
    if (trail) { c.a = c.a * i.ras.a; }
  }
  c = clamp(c, vec4f(0.0), vec4f(1.0));
  let mode = i.flags.y & 0xFFu;
  let r0 = (i.flags.y >> 8u) & 0xFFu;
  let r1 = (i.flags.y >> 16u) & 0xFFu;
  let a0 = compare(c.a, (mode >> 3u) & 7u, r0);
  let a1 = compare(c.a, mode & 7u, r1);
  let op = (mode >> 6u) & 3u;
  var pass_ = a0 && a1;
  if (op == 1u) { pass_ = a0 || a1; }
  if (op == 2u) { pass_ = a0 != a1; }
  if (op == 3u) { pass_ = a0 == a1; }
  if (!pass_) { discard; }
  return c;
}
)";
// The screen warp's quad (screen effect kind 12, _distortionRender; verified from the code except where marked): a
// camera-facing square that redraws the scene's picture (wscene: here the picture after the first pass) at each of its
// pixels' own screen place, moved by the warp picture (wpic) through GX's indirect texturing, with no blending and no
// depth test. The offset, from the indirect matrix _distortionEffectUpdateMatrices builds (rows {0.5, 0, (s - 1) / 2}
// and {0, 0.5, (s - 1) / 2}, scale exponent 1) on the warp picture's components (A, B, G; the disc's warp picture is
// one intensity I in every channel, 0 at its edges): s x I texels on both axes, I in 0..255, of the game's 640 x 480
// picture (likely: the matrix is verified, the texel scale and the cancelling of the texture matrix's 128 s / width
// term are read from the move sweep's pictures, which show no seam at the square's edges); k.x the strength now (s),
// k.y the scale (255: I x 255 texels), k.zw the target's size in pixels. On every OS (WGSL through Dawn)
const char* kWarpShader = R"(
struct U { mvp: mat4x4f, model: mat4x4f, hi: vec4f, lo: vec4f, size: vec4f, }
@group(0) @binding(0) var<uniform> u: U;
struct W { k: vec4f, }
@group(1) @binding(0) var<uniform> wk: W;
@group(1) @binding(1) var ws: sampler;
@group(1) @binding(2) var wscene: texture_2d<f32>;
@group(1) @binding(3) var wpic: texture_2d<f32>;
struct WO { @builtin(position) pos: vec4f, @location(0) uv: vec2f, }
@vertex fn wvs(@location(0) p: vec3f, @location(1) uv: vec2f) -> WO {
  var o: WO;
  o.pos = u.mvp * vec4f(p, 1.0);
  o.uv = uv;
  return o;
}
@fragment fn wfs(i: WO) -> @location(0) vec4f {
  let d = textureSample(wpic, ws, i.uv);
  let at = i.pos.xy / wk.k.zw;
  let off = vec2f(wk.k.x * d.r * wk.k.y) / vec2f(640.0, 480.0);
  return vec4f(textureSample(wscene, ws, at + off).rgb, 1.0);
}
)";
// the vertex the particle pipeline takes (one per quad corner, two triangles per quad)
struct ParticleVertex {
    float pos[3], uv[2];
    uint8_t ras[4], prim[4], env[4], mat[4];
    uint32_t kind, alpha;  // the particle's kind bits; its alpha compare: mode | ref0 << 8 | ref1 << 16
};

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // column-major (WGSL's order)
};
Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
Mat4 translate(float x, float y, float z) { Mat4 r; r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
Mat4 rot_y(float a) { Mat4 r; r.m[0] = std::cos(a); r.m[2] = -std::sin(a); r.m[8] = std::sin(a); r.m[10] = std::cos(a); return r; }
Mat4 rot_x(float a) { Mat4 r; r.m[5] = std::cos(a); r.m[6] = std::sin(a); r.m[9] = -std::sin(a); r.m[10] = std::cos(a); return r; }
Mat4 perspective(float fovy, float aspect, float n, float f) {  // depth 0..1 (WebGPU)
    Mat4 r;
    const float t = 1.0f / std::tan(fovy / 2);
    std::fill(r.m, r.m + 16, 0.0f);
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = f / (n - f); r.m[11] = -1; r.m[14] = n * f / (n - f);
    return r;
}

// GX blend factors (GXBlendFactor) as WebGPU's; for the source factor 2 / 3 mean the destination colour, for the
// destination factor the source colour (GX names both SRCCLR / DSTCLR at the same numbers)
wgpu::BlendFactor gx_factor(int f, bool src) {
    switch (f) {
        case 0: return wgpu::BlendFactor::Zero;
        case 1: return wgpu::BlendFactor::One;
        case 2: return src ? wgpu::BlendFactor::Dst : wgpu::BlendFactor::Src;
        case 3: return src ? wgpu::BlendFactor::OneMinusDst : wgpu::BlendFactor::OneMinusSrc;
        case 4: return wgpu::BlendFactor::SrcAlpha;
        case 5: return wgpu::BlendFactor::OneMinusSrcAlpha;
        case 6: return wgpu::BlendFactor::DstAlpha;
        default: return wgpu::BlendFactor::OneMinusDstAlpha;
    }
}
wgpu::CompareFunction gx_compare(int c) {
    static const wgpu::CompareFunction k[] = {wgpu::CompareFunction::Never, wgpu::CompareFunction::Less,
                                              wgpu::CompareFunction::Equal, wgpu::CompareFunction::LessEqual,
                                              wgpu::CompareFunction::Greater, wgpu::CompareFunction::NotEqual,
                                              wgpu::CompareFunction::GreaterEqual, wgpu::CompareFunction::Always};
    return k[c & 7];
}

// How a mesh draws: the pixel-engine settings, as HSD_SetupPEMode (src/hsd/hsd_texp.c) derives them.
struct DrawState {
    int blend_type = 0;  // GXBlendMode: 0 none, 1 blend, 2 logic (drawn as none), 3 subtract
    int src = 1, dst = 0;
    bool z_test = true, z_write = true;
    int z_comp = 3;  // GX_LEQUAL
    int a_comp0 = 7, a_ref0 = 0, a_op = 0, a_comp1 = 7, a_ref1 = 0;
    int cull = 0;
    uint32_t key() const {
        return (uint32_t)blend_type | (uint32_t)src << 2 | (uint32_t)dst << 5 | (uint32_t)z_test << 8 | (uint32_t)z_write << 9 |
               (uint32_t)z_comp << 10 | (uint32_t)cull << 13;
    }
};

constexpr uint32_t RENDER_ZMODE_ALWAYS = 1u << 27, RENDER_NO_ZUPDATE = 1u << 29, RENDER_XLU = 1u << 30;

// The material uniform's layout (struct M in the shader): 24 floats of header (diffuse, ambient, mode, alpha test,
// alpha test 2, specular), then ten tables of one vec4 per stage
static_assert(ORRE_MAX_STAGES == 8, "the shader's arrays, samplers and textures are written for 8 stages");
// The camera uniform (struct U): mvp, model, hi, lo, size (44 floats), then the in-game lighting's arena matrix, eye,
// ambient and 8 lights x (position, colour, attenuation): 164 floats
constexpr int kCameraFloats = 44 + 16 + 4 + 4 + 3 * 8 * 4 + 4 + 4 + 4;  // (+ the fade tint, then the after-images' alpha)
constexpr int kTintAt = kCameraFloats - 8;
constexpr int kGhostAt = kCameraFloats - 4;
constexpr uint64_t kCameraBytes = kCameraFloats * sizeof(float);

// The lights in-game lighting uses: the move viewer room's arena, M1_water_colo.dat (the Phenac Colosseum stage) in
// waza_viewer.fsys, decoded from its scene_data light list and equal to the game's own ORRE_WAZA_VIEWER_LIGHTS log
// (verified 2026-11-19); static (no light animation). The game's battles light the same way from their arena's map
// (likely); other arenas would need their own lists (a later reader of scene_data +0x08). *Note 2026-11-22:* the
// Arena stage is now Mt. Battle's valley, still lit with this Phenac list (it looked right to snatcherLeo; the valley's own
// list is not read yet, so the game would light it somewhat differently: likely). Positions in the arena's
// world; the infinite lights shine from the direction of their position seen from the world origin (the game puts
// them 2^20 times further out). Flags: 1 diffuse, 2 a specular slot (GX has 8 slots: only the first two infinite
// lights get one; the point lights are flagged specular too but no slot is left, verified from the log's masks).
// Point lights: k = GXInitLightDistAttn(ref distance, 0.99, steep) = (1, 0, 0.0101 / (0.99 d^2)).
struct ArenaLight { int kind; float pos[3]; float rgb[3]; int flags; float k[3]; };
constexpr float kArenaAmbient[3] = {76, 76, 76};
const ArenaLight kArenaLights[] = {
    {1, {68.5441f, 171.2361f, -91.5254f}, {128, 128, 128}, 3, {1, 0, 0}},
    {1, {-88.5074f, 171.2361f, 98.6520f}, {128, 128, 128}, 3, {1, 0, 0}},
    {1, {266.6534f, 171.2361f, 302.9531f}, {51, 51, 51}, 1, {1, 0, 0}},
    {1, {-268.9288f, 171.2361f, -315.4289f}, {51, 51, 51}, 1, {1, 0, 0}},
    {2, {-15.1620f, 291.7086f, 13.6458f}, {0, 0, 0}, 1, {1, 0, 0.0101f / (0.99f * 10000.0f * 10000.0f)}},
    {2, {2.8264f, -62.5905f, -3.0874f}, {87, 92, 92}, 1, {1, 0, 0.0101f / (0.99f * 60.0f * 60.0f)}},
};

constexpr int kSpec = 20, kXF = 24, kOP = kXF + 32, kK = kOP + 32, kTC = kK + 32, kTA = kTC + 32, kTP = kTA + 32, kTQ = kTP + 32,
              kKC = kTQ + 32, kR0 = kKC + 32, kR1 = kR0 + 32, kShSw = kR1 + 32, kShMod = kShSw + 4, kShOn = kShMod + 4,
              kUniformFloats = kShOn + 4;

DrawState draw_state(const OrreMesh& m) {
    DrawState s;
    s.cull = m.cull;
    if (m.has_pe) {
        // HSD_PEDesc: flags (1 colour update, 2 alpha update, 4 dst alpha, 8 z before texturing, 0x10 z test,
        // 0x20 z update, 0x40 dither), ref0, ref1, dst_alpha, type, src, dst, logic op, z_comp, alpha comp0, op, comp1
        const uint8_t* pe = m.pe;
        s.blend_type = pe[4];
        s.src = pe[5] & 7;
        s.dst = pe[6] & 7;
        s.z_test = (pe[0] & 0x10) != 0;
        s.z_write = (pe[0] & 0x20) != 0;
        s.z_comp = pe[8] & 7;
        s.a_comp0 = pe[9] & 7; s.a_ref0 = pe[1]; s.a_op = pe[10] & 3; s.a_comp1 = pe[11] & 7; s.a_ref1 = pe[2];
        return s;
    }
    // the defaults: blending only for translucent materials (RENDER_XLU), source alpha over the rest; depth writes
    // unless RENDER_NO_ZUPDATE; depth test LEQUAL, or ALWAYS with RENDER_ZMODE_ALWAYS; translucent materials that
    // still write depth keep only pixels with alpha above 0, everything else has no alpha test
    const bool xlu = (m.render_mode & RENDER_XLU) != 0;
    s.blend_type = xlu ? 1 : 0;
    s.src = 4;
    s.dst = 5;
    s.z_write = (m.render_mode & RENDER_NO_ZUPDATE) == 0;
    s.z_comp = (m.render_mode & RENDER_ZMODE_ALWAYS) ? 7 : 3;
    if (s.z_write && xlu) { s.a_comp0 = 4; s.a_comp1 = 4; }
    return s;
}

const wchar_t* kClass = L"OrreStudioViewer";
}  // namespace

struct Viewer::Impl {
    Viewer* owner = nullptr;  // the Viewer that holds this Impl now (its child window's messages go there)
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;
    wgpu::TextureFormat format = wgpu::TextureFormat::BGRA8Unorm;
    wgpu::Texture depth;
    wgpu::ShaderModule shader;
    wgpu::PipelineLayout playout;
    wgpu::RenderPipeline background;
    std::map<uint32_t, wgpu::RenderPipeline> pipelines;  // by DrawState::key
    wgpu::Buffer uniforms;
    wgpu::BindGroupLayout layout0, layout1;
    wgpu::BindGroup camera_group, default_group;
    wgpu::Buffer default_buf;
    std::map<int, wgpu::Sampler> samplers;  // by wrap_s * 3 + wrap_t
    wgpu::Texture white;
    int w = 0, h = 0;
    struct Part {
        wgpu::Buffer vbuf, mbuf;
        wgpu::BindGroup group;
        wgpu::RenderPipeline pipeline;
        wgpu::RenderPipeline ghost_pipe;  // the after-images' blended copy of it (built when first needed)
        uint32_t count;
        bool blended;
        int mesh;  // index into model.meshes (for re-posing)
        int tex[ORRE_MAX_STAGES] = {-2, -2, -2, -2};  // the pictures its material group was built with
        float blend[ORRE_MAX_STAGES] = {-1, -1, -1, -1};
        float xf[ORRE_MAX_STAGES * 4] = {-1};          // and the stages' coordinate transforms
        float mat[ORRE_MATERIAL_FLOATS] = {-1};        // and its colours (orre_model_material_frame)
    };
    std::vector<float> fmat;    // per mesh x ORRE_MATERIAL_FLOATS: the colours at the current frame
    std::vector<int> ftex;      // per mesh x stage: the picture at the current frame (orre_model_texture_frame2)
    std::vector<float> fblend;  // per mesh x stage: the blend factor at the current frame
    std::vector<float> fxf;     // per mesh x stage x 4: the coordinate transform at the current frame
    // the animation clock: wall time since the loop started, turned into whole animation frames (the battle draws
    // whole frames only: models-animation.md "Whole frames and half frames")
    LARGE_INTEGER anim_start{}, freq{};
    int anim_frame = -1;  // the frame last uploaded (-1: bind pose uploaded)
    double anim_t = -1;   // the exact frame last posed (with its fraction when smoothing)
    double anim_tl = -1;  // the timeline's frame last shown (runs on past the animation in a held ending)
    // the idle blink (advance_idle): its state, counted in battle frames on the animation clock
    bool blinking = false;
    int blink_timer = 0;
    long long blink_start = 0, blink_last = -1;
    float blink_shown = -1;
    std::mt19937 blink_rng{std::random_device{}()};
    // paused while a tab is dragged (set_paused): the loop holds its frame and, when let go, carries on from it
    bool paused = false;
    LARGE_INTEGER paused_at{};
    // Restarts the animation clock from now. While paused, the pause starts now too: resuming moves the clock on by the
    // time paused (set_paused), so a restart made during a pause would otherwise be pushed into the future by the whole
    // pause and the model sat in its first pose, or unposed, that long. *Fixed 2026-11-22* (snatcherLeo: after picking a style
    // on the Styles tab, where the view is cut and so paused, the model on Overview "shouldn't take a while to load in";
    // the new model was read in 2 ms, but its idle started only after the seconds spent on that tab).
    // Windows clock: QueryPerformanceCounter; Linux / macOS: std::chrono::steady_clock.
    void restart_clock() {
        QueryPerformanceCounter(&anim_start);
        if (paused) paused_at = anim_start;
    }
    // cut: the page has cut the view to nothing (set_cut); framed: a picture has been presented, so revealing the
    // view shows a real frame, never a white, not-yet-drawn surface (snatcherLeo, 2026-11-19: "there's a "white flash" from
    // the 3D model ... Same white flash occurs when I first launch the app")
    bool cut = false, framed = false;
    // a fixed camera copied from the game (set_game_camera; comparisons with the move viewer's pictures)
    bool game_cam = false;
    // the battle camera's eye, interest and fov fixed to one frame of the game's own (set_battle_camera_shot; the
    // move sheets' side-by-sides, from the game's battle-camera log), in the battle's space
    bool shot_cam = false;
    float shot_eye[3] = {}, shot_at[3] = {}, shot_fov = 45;
    bool game_light = false;  // in-game lighting (set_game_lighting)
    int rate = 0;             // the preview rate (set_rate): 0 the game's, else Hz with smoothing
    // the pacer for rates above 64 Hz (update_timer): a thread that wakes at the rate on a high-resolution waitable
    // timer and asks for one repaint (InvalidateRect) when the last has been drawn; the frame is posed in WM_PAINT,
    // which Windows hands out only when no input waits, so clicks always get through. *Corrected 2026-11-19:* it posted
    // WM_APP + 30 first, and posted messages go before input: at 165 Hz the window stopped taking clicks (snatcherLeo).
    // Windows waitable timers; Linux / macOS: the toolkit's frame clock
    std::thread pacer;
    std::atomic<bool> pacer_run{false}, pacer_pending{false};
    std::atomic<HWND> pacer_hwnd{nullptr};
    std::atomic<int> pacer_hz{0};
    void stop_pacer() {
        pacer_run = false;
        if (pacer.joinable()) pacer.join();
        pacer_pending = false;
    }
    void start_pacer(HWND hwnd, int hz) {
        pacer_hwnd = hwnd;
        pacer_hz = hz;
        if (pacer_run) return;
        stop_pacer();
        pacer_run = true;
        pacer = std::thread([this] {
            HANDLE t = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if (!t) t = CreateWaitableTimerW(nullptr, FALSE, nullptr);
            while (pacer_run) {
                LARGE_INTEGER due;
                due.QuadPart = -(LONGLONG)(10000000.0 / std::max(1, pacer_hz.load()));
                if (t) { SetWaitableTimer(t, &due, 0, nullptr, nullptr, FALSE); WaitForSingleObject(t, 200); }
                else Sleep(8);
                // (a repaint request, not a posted message: paints wait for input, so clicks always get through)
                // (page mode: the view's window shows nothing and gets no paints, so the frame is a posted message,
                // skipped by the view while input waits: WM_APP + 32 in Viewer::proc)
                if (pacer_run && !pacer_pending.exchange(true)) {
                    if (page) PostMessageW(pacer_hwnd, WM_APP + 32, 0, 0);
                    else InvalidateRect(pacer_hwnd, nullptr, FALSE);
                }
            }
            if (t) CloseHandle(t);
        });
    }
    Mat4 game_view, game_model;
    float game_fov = 1.0f, game_aspect = 1.0f;
    // the Moves tab: an animation played once (once >= 0), held by the user on once_at; the animation the camera
    // also frames; Plain stage (the move viewer's camera and grey) and the colours to go back to
    int once = -1, once_at = 0, frame_anim = -1, anim_last = -2;
    bool held = false, plain = false;
    float plain_yaw = 0, plain_pitch = 0;  // Plain stage: the drag's turn from the game's angle
    // Studio's own camera on the Plain stage and the Arena too (set_stage_camera): the Overview's stages change only
    // what is around the model, never the angle (snatcherLeo, 2026-11-22: "if I'm loading different views/backgrounds for the
    // model, it shouldn't change rotation/the viewing angle"); the Moves tab keeps the game's move-viewer angle
    bool studio_cam = false;
    float user_hi[4] = {0.15f, 0.17f, 0.19f, 1}, user_lo[4] = {0.09f, 0.1f, 0.11f, 1};
    std::vector<std::vector<OrreVertex>> posed;  // per mesh, reused every frame
    bool has_billboards = false;  // the model has camera-facing parts (re-posed when the view turns)
    float view[12] = {};          // the view they were last posed for
    std::vector<uint8_t> shown;                   // per mesh: drawn at the current frame (orre_model_visibility)
    std::vector<Part> parts;
    std::vector<wgpu::Texture> textures;
    float centre[3] = {0, 0, 0}, radius = 1;
    bool has_model = false;

    // ---- extra models beside the Pokémon (release step 4): the target Pokémon a comparison with the game's move sweep
    // needs (Espeon on Umbreon), and later the moves' own models (the balls, rocks). Each keeps its model, pictures and
    // parts; it is posed on the CPU and its vertices placed in the Pokémon's own space (where the shaders draw), so the
    // same camera, lights and pipelines serve it
    struct MoveSeq;  // (defined with the move's sequence state below)
    struct Inst {
        OrreModel model{};
        std::vector<wgpu::Texture> textures;
        std::vector<Part> parts;
        std::vector<std::vector<OrreVertex>> posed;
        std::vector<uint8_t> shown;
        bool visible = false;
        float alpha_set = -1;  // the material alpha last given to its parts (the leaves' fade; -1: the model's own)
        ~Inst() { orre_model_free(&model); }
    };
    std::unique_ptr<Inst> target;  // the target Pokémon (set_target_model; the box when none)
    // The arena (snatcherLeo, 2026-11-19: "something really basic - but with a visible platform and area ... that Phenac City
    // arena"): the move viewer's own room, M1_water_colo.dat, drawn around the Pokémon with Plain stage's camera
    // (set_arena_model / set_arena). Its vertices sit in the battle's space, placed into the Pokémon's like the target;
    // the moves' stage fades (target 1) tint it through its own copy of the camera uniforms (stage_group), as the
    // game's filter tints the room's models. Posed once (its standing pose); its water animation is for later
    std::unique_ptr<Inst> arena;
    bool arena_on = false;
    float arena_placed = 0;  // the -kUserX it was placed with (re-placed when the grid's spacing changes)
    wgpu::Buffer uniforms_stage;
    wgpu::BindGroup stage_group;
    // the target Pokémon's own copy of the camera uniforms: its half's colour fade (target 2 in the target's half,
    // box_fade) as its tint, as the fade tints its half's owner (*added 2026-11-21:* Attract's pink on Umbreon; the
    // target was drawn with the user's uniforms, so it took the user's tint and never its own)
    wgpu::Buffer uniforms_target;
    wgpu::BindGroup target_group;
    bool has_target() const { return target && target->model.mesh_count > 0; }
    // the target's animation and frame at move frame f: its idle loop, and from the moment its half starts that half's
    // slot (the WZX header's: 8 for most hits) played once, then the idle again
    // Whether a half's owner draws in the second pass at move frame f (GSgfxDrawScene draws the models with flag 0x200
    // and the link-1 particles after a depth reset; wazaSequenceStart sets an owner's flag from its sequence header's
    // flags bit 1 through wazaSequencePokemonMotionStart, and clears it when the sequence stops: verified from the code
    // and the project's capture of Fire Punch, 2026-11-20)
    bool owner_overlay(const MoveSeq& S, int f, int from) const {
        if (once < 0 || !S.wzx || !S.running || S.anim != once || f < from) return false;
        OrreWzxHeader h{};
        orre_wzx_header(S.wzx, &h);
        return ((h.flags >> 1) & 1) != 0;
    }
    // A half's lead-in in battle frames (orre_wzx_lead_in: entries timed before the owner's time point 0 move every
    // start later, and the owner's animation reaches its first frame that late), against its owner's time points
    int half_lead(const MoveSeq& S) const {
        if (!S.wzx) return 0;
        OrreWzxHeader h{};
        orre_wzx_header(S.wzx, &h);
        const OrreModel* om = !S.box ? &model : (has_target() ? &target->model : nullptr);
        int slot = h.slot;
        if (om && slot >= om->slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
        slot = std::clamp(slot, 0, 16);
        return orre_wzx_lead_in(S.wzx, om ? om->slot_times[slot] : kBoxTimes) / 2;
    }
    // the frame of an owner's slot animation (len frames) shown `f` frames into its half with a lead-in of `lead`
    // frames: wazaSequencePokemonMotionStart (verified from the code) starts it at frame len - lead (mod len), so it
    // plays its ending first and its frame 0 falls on the lead; then on to its last frame, held
    static int lead_frame(int f, int lead, int len) {
        len = std::max(1, len);
        if (lead <= 0 || f >= lead) return std::min(std::max(0, f - std::max(0, lead)), len - 1);
        return (len - lead % len + f) % len;
    }
    // The idle's frame `f` frames into a half's lead-in (f < lead). *Corrected 2026-11-22* (snatcherLeo: "when a Pokemon uses a
    // move, it does its move, and the target doesn't move from idle, UNTIL the hit happens on it, and then it does the
    // hit animation"): wazaSequencePokemonMotionStart runs the lead-in on the animation wazaSequenceOwnerFindAnim
    // returns, and in a battle that is the idle (verified by the project's animation log, ORRE_ANIM_LOG, the reference playthrough sitting P17c:
    // Lanturn hit by Bite, shift 22, stayed on its idle, which jumped from frame 43 to 50 = 61 - 11 and played on to
    // its last frame, 60, held; the hit started at its frame 1 as the jaws struck). Before, Studio played the hit
    // animation's last frames there, as the game's debug move viewer does (its FindAnim returned Umbreon's hit; why the
    // two differ is still open: FindAnim reads the owner's first animation list, which the viewer likely fills
    // differently). The idle skips to `lead` frames before its end, plays on and holds its last frame
    static int battle_lead_frame(int f, int lead, const OrreModel& m) {
        // (as the game: shift / 2 mod the frame count, a whole count when that is 0; a longer lead-in loops it). The
        // count is GSmodelGetFrameCount's: 1 + the animation's end frame (the end frame included), so Lanturn's
        // 60-frame idle counts 61 and the jump lands on 50, as the project's log shows, then plays to its end frame, 60
        // (*fixed 2026-11-22*: it counted 60 and landed on 49)
        const int end = std::max(0, (int)std::floor(orre_model_animation_length(&m, m.idle_animation)));
        const int count = end + 1;
        int h = std::max(0, lead) % count;
        if (h == 0) h = count;
        return (count - h + std::max(0, f)) % count;
    }
    // the user's pose frame for animation anim at move frame f (its half's lead-in when it is the move's animation)
    int user_pose(int anim, int f) const {
        const int len = std::max(1, (int)std::floor(orre_model_animation_length(&model, anim)));
        const bool move = seq.wzx && anim == seq.anim;
        // (a move's pose starts with its half, kUserDelay after Play: the first frame holds the pose's start)
        if (move && seq.running) f = std::max(0, f - seq.first_frame);
        return lead_frame(f, move ? half_lead(seq) : 0, len);
    }
    // the user's animation and pose frame at move frame f: the move's animation (anim), or, once its third part runs
    // (aseq), that part's slot animation from its own start, with its lead-in (wazaSequencePokemonMotionStart starts the
    // owner's slot animation with each sequence, verified from the code)
    // A half's lead-in plays the idle in a battle (battle_lead_frame, *corrected 2026-11-22*), for the user's halves too
    // (verified by the project's animation log, the reference playthrough sitting P17b: Ninetales' Imprison, shift 26, its idle 39-42, then from
    // frame 98 = 111 - 13 to 110, held, then the move's animation from its start)
    void user_anim(int anim, int f, int& a, int& pf) const {
        const bool idle = model.idle_animation >= 0;
        if (aseq.wzx && aseq.running && anim == seq.anim && f >= aseq.first_frame) {
            const int a3 = orre_model_slot_animation(&model, std::clamp(aseq.slot, 0, 16));
            if (a3 >= 0) {
                const int len3 = std::max(1, (int)std::floor(orre_model_animation_length(&model, a3)));
                const int F3 = f - aseq.first_frame, lead3 = half_lead(aseq);
                a = idle && F3 < lead3 ? model.idle_animation : a3;
                pf = idle && F3 < lead3 ? battle_lead_frame(F3, lead3, model) : lead_frame(F3, lead3, len3);
                return;
            }
        }
        a = anim;
        if (idle && seq.wzx && anim == seq.anim && seq.running) {
            const int F = f - seq.first_frame, lead = half_lead(seq);
            if (lead > 0 && F < lead) {  // (before the half's first frame too: the idle where the lead-in starts it)
                a = model.idle_animation;
                pf = battle_lead_frame(F, lead, model);
                return;
            }
        }
        pf = user_pose(anim, f);
    }
    void target_anim_at(int f, int& anim, float& frame) const {
        const OrreModel& m = target->model;
        anim = m.idle_animation;
        int from = 0;
        if (tseq.wzx && f >= tseq.first_frame) {
            OrreWzxHeader h{};
            orre_wzx_header(tseq.wzx, &h);
            int slot = h.slot;
            if (slot >= m.slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
            const int a = orre_model_slot_animation(&m, std::clamp(slot, 0, 16));
            const int len = a >= 0 ? (int)std::floor(orre_model_animation_length(&m, a)) : 0;
            const int lead = half_lead(tseq);
            // (its first pose holds for one update: in the camera log of Thunder on Umbreon, joint 42 reaches the hit
            // animation's frame 1 one update after the frame the lead-in alone gives; likely the start sets the frame
            // and the first advance comes with the next update)
            const int F = std::max(0, f - tseq.first_frame - 1);
            if (a >= 0 && F < lead + len) {
                // the lead-in plays the idle, not the hit (battle_lead_frame); then the hit from its first frame
                if (F < lead && m.idle_animation >= 0) {
                    anim = m.idle_animation;
                    frame = (float)battle_lead_frame(F, lead, m);
                    return;
                }
                anim = a;
                frame = (float)lead_frame(F, lead, len);
                return;
            }
            from = tseq.first_frame + 1 + lead + len;
        }
        // its idle carries on from where it was when the move started (tgt_phase), until the hit restarts it from its
        // first frame (the game starts the idle motion again after a reaction)
        const float il = anim >= 0 ? std::max(1.0f, std::floor(orre_model_animation_length(&m, anim))) : 1.0f;
        const double base = from == 0 ? tgt_phase : 0.0;
        frame = anim >= 0 ? (float)std::fmod((double)std::max(0, f - from) + base, (double)il) : 0.0f;
    }
    // The target's idle frame at this moment (timeline frame tl while a move plays, else the idle clock), so its idle
    // can carry on across a clock restart (play_once, stop_once, a move's end, set_idle): 0 while it shows its reaction.
    // *Added 2026-11-22* (snatcherLeo: "if I play a move once, then I do another move, there are some small animation oddities,
    // in both the target Pokemon and the Pokemon I'm currently viewing"): the target's idle ran on the move's timeline
    // and the Pokémon's idle clock, both restarted at each Play and each move's end, so it jumped back to its first
    // frame then (verified from the pose log: Espeon's idle frame 40 became 0 when Bite started, and frame 43 became 0
    // when Tackle, which gives the target no reaction, ended). In battle the target keeps idling through the user's half.
    double target_phase_at(int tl) const {
        if (!has_target() || target->model.idle_animation < 0) return 0.0;
        const OrreModel& m = target->model;
        const double il = std::max(1.0f, std::floor(orre_model_animation_length(&m, m.idle_animation)));
        if (once >= 0) {
            int a;
            float fr;
            target_anim_at(tl, a, fr);
            return a == m.idle_animation ? (double)fr : 0.0;
        }
        LARGE_INTEGER now;
        if (paused) now = paused_at;
        else QueryPerformanceCounter(&now);  // (Windows clock; std::chrono::steady_clock on Linux / macOS)
        const double secs = freq.QuadPart ? (double)(now.QuadPart - anim_start.QuadPart) / (double)freq.QuadPart : 0.0;
        return std::fmod(std::floor(std::max(0.0, secs) * ORRE_BATTLE_ANIM_FPS) + tgt_phase, il);
    }
    // the target's place: its model space into the Pokémon's (where the shaders draw): turned 180 degrees about y and
    // set kTargetX - kUserX ahead (battle (kTargetX - z, y, x), then to_battle's inverse)
    void target_xf(float M[12]) const {
        const float D = kTargetX - kUserX;
        const float m[12] = {-1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, D};
        std::copy(m, m + 12, M);
    }
    // a target joint's place in the battle's space at move frame f, and its chain's rotation / scale
    bool target_joint(int joint, int f, float p[3], float r[3], float s[3]) {
        int anim;
        float fr;
        target_anim_at(f, anim, fr);
        const OrreModel& m = target->model;
        if (joint < 0 || joint >= m.joint_count) return false;
        tjw.resize((size_t)m.joint_count * 12);
        tjs.resize((size_t)m.joint_count * 3);
        if (!orre_model_joint_world(&m, anim, fr, tjw.data(), tjs.data())) return false;
        const float x = tjw[(size_t)joint * 12 + 3], y = tjw[(size_t)joint * 12 + 7], z = tjw[(size_t)joint * 12 + 11];
        p[0] = kTargetX - z;
        p[1] = y;
        p[2] = x;
        r[0] = r[1] = r[2] = 0;
        s[0] = s[1] = s[2] = 1;
        orre_model_part_chain(&m, anim, fr, joint, r, s);
        r[1] -= kTurn;  // (its root turned -90 degrees in the battle)
        return true;
    }
    std::vector<float> tjw, tjs;
    // any owner's joint in the battle's space at move frame f: the user's (joint >= 0, or -2 its bounding box's
    // centre), the target Pokémon's, or the box's places; with the chain's rotation and scale (GSpartGetTransform)
    bool owner_joint(bool box, int joint, int f, float p[3], float r[3], float s[3]) {
        r[0] = r[1] = r[2] = 0;
        s[0] = s[1] = s[2] = 1;
        float cd[3];  // (a carried owner's displacement: carry_offset)
        if (box && has_target()) {
            carry_offset(true, f, cd);
            if (joint == -2) {
                const OrreModel& tm = target->model;
                const float c[3] = {(tm.bounds_min[0] + tm.bounds_max[0]) * 0.5f, (tm.bounds_min[1] + tm.bounds_max[1]) * 0.5f,
                                    (tm.bounds_min[2] + tm.bounds_max[2]) * 0.5f};
                p[0] = kTargetX - c[2] + cd[0]; p[1] = c[1] + cd[1]; p[2] = c[0] + cd[2];
                r[1] = -kTurn;
                return true;
            }
            if (!target_joint(joint, f, p, r, s)) return false;
            for (int c = 0; c < 3; ++c) p[c] += cd[c];
            return true;
        }
        if (box) {
            box_joint(joint, box_pose(f), p, r);
            return true;
        }
        float sp[3];
        if (joint == -2) {
            for (int c = 0; c < 3; ++c) sp[c] = (model.bounds_min[c] + model.bounds_max[c]) * 0.5f;
        } else {
            if (joint < 0 || joint >= model.joint_count || seq.anim < 0) return false;
            int ua, pf;
            user_anim(seq.anim, f, ua, pf);  // (the third part's own animation once it runs)
            if (!orre_model_joint_world(&model, ua, (float)pf, jw.data(), js.data())) return false;
            sp[0] = jw[(size_t)joint * 12 + 3];
            sp[1] = jw[(size_t)joint * 12 + 7];
            sp[2] = jw[(size_t)joint * 12 + 11];
            orre_model_part_chain(&model, ua, (float)pf, joint, r, s);
        }
        // (carried: the joint moved with the user, carry_user_xf, in its own space; its turn and size changes are left
        // out of r and s for now)
        float CM[12];
        if (carry_user_xf(f, CM)) {
            const float q[3] = {sp[0], sp[1], sp[2]};
            for (int i = 0; i < 3; ++i) sp[i] = CM[3 * i] * q[0] + CM[3 * i + 1] * q[1] + CM[3 * i + 2] * q[2] + CM[9 + i];
        }
        (void)cd;
        to_battle(sp, p);
        r[1] += kTurn;
        return true;
    }

    // ---- the moves' own models (type 2 entries, 545 on the disc; the entries spec B, verified from the code:
    // _wazaSequenceModelEntryStart / Update / Stop): model 0 of the entry's embedded HSD archive, its joint animation
    // played once from frame 0 at half rate (one frame per 30 fps battle frame), placed by the entry's attach point and
    // follow mode as GSmodelAttachToGSpart and modelUpdateAttachments do (the offsets set from the owner's place,
    // turn and effect scale per mode, then the followed parts replaced by the joint's every frame, or once), or at the
    // field's centre (world-placed, node flag 4). It is drawn while it runs and ends when its animation does, unless
    // it is held (model flags 2): then it stays on its last frame for the rest of the move (*corrected 2026-11-20*:
    // this said a held model isn't drawn; _wazaSequenceModelEntryUpdate puts it in state 3, which is never stopped,
    // and the sequence's end then leaves it held, sequence +0x15 = -1: Dig's hole stays, verified on the move sweep).
    // Carry mode (the Pokémon riding the effect model: Dig, Dive, Waterfall, Earthquake's
    // target): carry_offset
    struct FxModel {
        std::unique_ptr<Inst> inst;
        bool box = false;           // the target's half
        int start = 0, anim = -1, len = 0, sel = 0, joint = -1;
        bool world = false, once = false, done = false, snapped = false;
        bool overlay = false;  // drawn in the second pass (its entry's flags bit 0: fn_800E3CC8, wazaSequenceEntry.c)
        // carry (model_carry): the owner rides this model's joint carry_joint; held (model flags 2): the entry stays on
        // its last frame for the rest of the move (state 3), the owner with it
        bool carry = false, held = false;
        int carry_joint = -1, carry_sel = 0;
        // its texture animation when it is not the joint animation's own (+0x08; started only then), and its length:
        // the entry runs until both animations have ended (_wazaSequenceModelEntryUpdate: done = joint done && texture
        // done; verified from the code); Waterfall's target half is a model with only a texture animation
        int tex_anim = -1, tex_len = 0;
        int key = 0;  // its entry's key (particle entries link to it by key: entry flags 1)
        // its animation frames per battle frame: 1 for every model. *Corrected 2026-11-21:* this was 2 with model flag 4
        // (GSmodelSet60fpsAnimFlag; Mist's and Haze's carriers, the only two on the disc), a guess from pictures whose
        // puffs were also 6.7 too high (the root joint, root_replaced). The game's model log on Mist 54 shows the
        // carrier at animation frame 15 at battle frame 15 (rate 0.5 per tick, as every move model), and Studio's puffs
        // match the game's particle log frame by frame at rate 1 (mean height within 0.3 at every 9th frame, the
        // ring's x / z spread too), far off at 2 (verified)
        int rate = 1;
        // how many battle frames its animation runs behind its entry's start in Studio's timeline (negative: ahead).
        // *Updated 2026-11-21* from the project's PC-port log of the models' animFrame (the move sweep, Earthquake 89 and
        // Block 335 on Espeon vs Umbreon; verified readings): the game's model shows animFrame = (its sequence's clock -
        // the entry's start) / 2, no lag of its own (Earthquake's ring at animFrame 0 the frame its entry starts,
        // Block's X at 1 when its sequence reads tick 2). So 0 for the user's and the target's halves (side by side:
        // at the old 2 Earthquake's ring ran about 3 frames and Block's X 4-5 frames behind; a trial at -1 / -3, from
        // reading the log's game frames as the sweep's, overshot both by a row: that log is a separate run, so only
        // its relative readings carry over). The user's own third part (self-aimed moves: Swords Dance's swords,
        // Double Team's carrier) keeps kModelLag = 2, which their close-ups showed (asked of Main to log too)
        int lag = kModelLag;
        // the first frame its half has ended (half_end_frame): the game stops every entry still running then, so a
        // model whose animation outlasts its half is hidden at that moment unless it is held (INT_MAX: no end entry)
        int stop = INT_MAX;
        float off_pos[3] = {0, 0, 0}, off_rot[3] = {0, 0, 0}, off_scl[3] = {1, 1, 1};
        float snap_p[3] = {0, 0, 0}, snap_r[3] = {0, 0, 0}, snap_s[3] = {1, 1, 1};
    };
    std::vector<FxModel> fx_models;
    // A move model's animation runs kModelLag battle frames behind its entry's start (shown from the start, holding its
    // first pose): likely, from the move sweep (2026-11-20): the Swords Dance close-up, every 2 frames, shows the game's
    // swords one row (2 frames) behind Studio's while their linked sparkles keep time; Withdraw's shrinking fits it
    // (~1.6). Where in the game's update order the 2 frames come from is not read yet (open)
    static constexpr int kModelLag = 2;
    void seq_model(MoveSeq& S, int i, int f) {
        OrreWzxEntry e{};
        orre_wzx_entry(S.wzx, i, &e);
        FxModel fm;
        fm.inst = std::make_unique<Inst>();
        if (!orre_wzx_model(S.wzx, i, &fm.inst->model)) { S.state[(size_t)i] = 2; return; }
        inst_build(*fm.inst);
        fm.box = S.box;
        fm.key = e.key;
        fm.start = f;
        fm.stop = half_end_frame(S);
        fm.anim = e.model_anim >= 0 && e.model_anim < fm.inst->model.animation_count ? e.model_anim : -1;
        fm.len = fm.anim >= 0 ? (int)std::floor(orre_model_animation_length(&fm.inst->model, fm.anim)) : 0;
        if (e.model_tex_anim >= 0 && e.model_tex_anim != e.model_anim && e.model_tex_anim < fm.inst->model.animation_count) {
            fm.tex_anim = e.model_tex_anim;
            fm.tex_len = (int)std::floor(orre_model_animation_length(&fm.inst->model, fm.tex_anim));
        }
        fm.sel = e.follow >= 1 && e.follow <= 7 ? e.follow : 0;
        fm.world = (e.flags & 4) != 0;
        fm.once = (e.flags & 2) != 0;
        fm.overlay = (e.model_flags & 1) != 0;
        // (ORRE_FLAG4_RATE=2: flag 4 at double rate, the earlier guess, a test switch for comparing; *corrected
        // 2026-11-21:* the normal rate is the game's, see FxModel::rate)
        static const int flag4_rate = std::getenv("ORRE_FLAG4_RATE") ? std::max(1, std::atoi(std::getenv("ORRE_FLAG4_RATE"))) : 1;
        fm.rate = (e.model_flags & 4) ? flag4_rate : 1;
        // (the target's half: -1. Block's close-ups every 4 frames, the second X's fast growth (its frames 19-24)
        // showing it best: at 0 Studio's X ran about 2 frames behind, at -2 (-kTargetDelay) 1-2 ahead, at -3 a row
        // ahead; -1 sits between. Likely: the target's content starts kTargetDelay after the cut for its particles and
        // animation, and its models a frame sooner; not read in the code)
        // (*updated 2026-11-21:* a self-aimed move's own third part, played as the user's half, now starts kUserDelay
        // after Play, so its models' lag is kModelLag less that: the same place on screen as before)
        fm.lag = S.box ? -1 : (S.part == 3 ? kModelLag - (&S == &seq ? kUserDelay : 0) : 0);
        const float own_pos[3] = {S.box ? kTargetX : kUserX, 0, 0}, own_rot[3] = {0, S.box ? -kTurn : kTurn, 0};
        const float sc = S.scale;
        if (!fm.world && fm.sel != 0 && e.model_carry == 0) {
            // the attach point, as for particles (the box's places, the target's or the user's joints; 16 = the centre)
            if (S.box && !has_target()) {
                fm.joint = e.attach == 0 ? kBoxGround : (e.attach == 1 || e.attach == 4 || e.attach == 5 ? kBoxFront : kBoxCentre);
            } else if (e.attach == 16) {
                fm.joint = -2;
            } else if (e.attach >= 0 && e.attach < 16) {
                const OrreModel& om = S.box ? target->model : model;
                const int j = om.slot_points[S.slot][e.attach];
                if (j >= 0 && j < om.joint_count) fm.joint = j;
            }
            // the parts not followed are set as offsets from the owner (scale S, rotation R, position P):
            // 1 S R, 2 S P, 3 P R, 4 S, 5 P, 6 R, 7 nothing
            const bool setS = fm.sel == 1 || fm.sel == 2 || fm.sel == 4, setP = fm.sel == 2 || fm.sel == 3 || fm.sel == 5,
                       setR = fm.sel == 1 || fm.sel == 3 || fm.sel == 6;
            for (int k = 0; k < 3; ++k) {
                if (setS) fm.off_scl[k] = sc;
                if (setP) fm.off_pos[k] = own_pos[k];
                if (setR) fm.off_rot[k] = own_rot[k];
            }
        } else if (!fm.world) {
            // carry mode or no follow: at the owner's place, turned as it, at the effect scale (the model's own root);
            // carry with follow mode 3, 5, 6 or 7 leaves the scale at 1 (_wazaSequenceModelEntryStart, verified)
            const bool unscaled = e.model_carry != 0 && (fm.sel == 3 || fm.sel == 5 || fm.sel == 6 || fm.sel == 7);
            for (int k = 0; k < 3; ++k) { fm.off_pos[k] = own_pos[k]; fm.off_rot[k] = own_rot[k]; fm.off_scl[k] = unscaled ? 1.0f : sc; }
            if (e.model_carry != 0 && fm.sel != 0) {  // (follow 0: no attaching, the game only re-places the owner)
                fm.carry = true;
                fm.carry_joint = e.model_carry_joint;
                fm.carry_sel = fm.sel;
                fm.held = (e.model_flags & 2) != 0;
            }
            fm.sel = 0;
        }
        S.state[(size_t)i] = 1;
        fx_models.push_back(std::move(fm));
    }
    // poses and places the moves' models for move frame f
    void fx_models_at(int f) {
        for (FxModel& fm : fx_models) {
            const int since = f - fm.start;
            int t = std::max(0, since - fm.lag) * fm.rate;  // (its animation's frame: fm.lag behind)
            // ended: its joint animation (none: at once) and its own texture animation (if any) both done
            const bool joint_done = fm.anim < 0 ? (fm.tex_anim >= 0 || since > fm.lag) : (since - fm.lag) * fm.rate >= fm.len;
            const bool tex_done = fm.tex_anim < 0 || (since - fm.lag) * fm.rate >= fm.tex_len;
            fm.done = since < 0 || (!fm.held && ((joint_done && tex_done) || f >= fm.stop));
            if (fm.done) continue;
            const int tt = std::max(0, std::min(t, fm.tex_len > 0 ? fm.tex_len - 1 : 0));  // (the texture's frame)
            t = fm.anim < 0 ? 0 : std::min(t, std::max(0, fm.len - 1));  // (an ended joint animation holds its last)
            float pos[3], rot[3], scl[3];
            fx_place(fm, f, pos, rot, scl);
            // the model's place in the battle (T R S, R = Rz Ry Rx), then into the Pokémon's space (to_battle's inverse:
            // (-z, y, x - kUserX))
            const float cx = std::cos(rot[0]), sx = std::sin(rot[0]), cy = std::cos(rot[1]), sy = std::sin(rot[1]),
                        cz = std::cos(rot[2]), sz = std::sin(rot[2]);
            const float R[3][3] = {{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
                                   {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
                                   {-sy, cy * sx, cy * cx}};
            float B[3][3];
            for (int r2 = 0; r2 < 3; ++r2)
                for (int c = 0; c < 3; ++c) B[r2][c] = R[r2][c] * scl[c];
            float M[12];
            for (int c = 0; c < 3; ++c) {
                M[0 + c] = -B[2][c];  // studio x = -battle z
                M[3 + c] = B[1][c];   // studio y = battle y
                M[6 + c] = B[0][c];   // studio z = battle x
            }
            M[9] = -pos[2];
            M[10] = pos[1];
            M[11] = pos[0] - kUserX;
            if (fm.sel != 0 && fm.joint != -1) root_replaced(fm, t, M);
            inst_pose(*fm.inst, fm.anim, (float)std::max(0, t), M, true, fm.tex_anim >= 0 ? fm.tex_anim : -2, (float)tt);
        }
    }
    // A move model that follows a part has its root joint's own transform replaced by its placement: M becomes
    // M x (the root joint's matrix at frame t)^-1, so the root's own place, turn and size drop out. Why: the game's
    // GSmodelSetPosition / SetRotation / SetScale write the model's root joint (renderJObj->translate etc.), and an
    // attached model (transformOverride, modelUpdateAttachments) takes its whole root transform from the part each
    // frame. Verified 2026-11-21 with the game's model log on Mist 54: its carrier's root joint stands 9 up in the
    // file; the game's emitter (the carrier's joint 1) was at y 14.87 with the carrier at Espeon's part 62 (y 3.64,
    // scale 0.75), which fits 3.64 + 0.75 x (joint 1 - joint 0) and not Studio's 3.64 + 0.75 x joint 1 (21.6: every
    // puff 6.7 too high). A scan of the disc found 6 such roots, all on following models: Mist's and Haze's carriers
    // (9 up) and Bite 44, Growl 45, Crunch 242 (root scale 0.5 / 0.83; likely the same rule, checked by the pictures).
    // ORRE_KEEP_FX_ROOT=1 keeps the root's own transform (a test switch only)
    // the inverse of a following model's root joint matrix at frame t, from its joints' world matrices w (rows of 4,
    // the root first): inv (3 x 3) and it (translation); false when the root's own transform is kept
    static bool root_inverse(const FxModel& fm, const float* J, float inv[9], float it[3]) {
        static const bool keep = std::getenv("ORRE_KEEP_FX_ROOT") != nullptr;
        if (keep || fm.sel == 0 || fm.joint == -1) return false;
        const float a = J[0], b = J[1], c = J[2], d = J[4], e = J[5], g = J[6], h = J[8], k = J[9], l = J[10];
        const float det = a * (e * l - g * k) - b * (d * l - g * h) + c * (d * k - e * h);
        if (std::fabs(det) < 1e-12f) return false;
        const float m[9] = {(e * l - g * k) / det, (c * k - b * l) / det, (b * g - c * e) / det,
                            (g * h - d * l) / det, (a * l - c * h) / det, (c * d - a * g) / det,
                            (d * k - e * h) / det, (b * h - a * k) / det, (a * e - b * d) / det};
        std::copy(m, m + 9, inv);
        for (int r = 0; r < 3; ++r) it[r] = -(m[3 * r] * J[3] + m[3 * r + 1] * J[7] + m[3 * r + 2] * J[11]);
        return true;
    }
    void root_replaced(const FxModel& fm, int t, float M[12]) {
        const int nj = fm.inst->model.joint_count;
        if (nj <= 0) return;
        std::vector<float> w((size_t)nj * 12);
        if (!orre_model_joint_world(&fm.inst->model, fm.anim, (float)std::max(0, t), w.data(), nullptr)) return;
        float inv[9], it[3];
        if (!root_inverse(fm, w.data(), inv, it)) return;
        float R[9];
        for (int r = 0; r < 3; ++r)
            for (int col = 0; col < 3; ++col)
                R[3 * r + col] = M[3 * r] * inv[col] + M[3 * r + 1] * inv[3 + col] + M[3 * r + 2] * inv[6 + col];
        for (int r = 0; r < 3; ++r) M[9 + r] += M[3 * r] * it[0] + M[3 * r + 1] * it[1] + M[3 * r + 2] * it[2];
        std::copy(R, R + 9, M);
    }
    // A move model's place, turn and size in the battle at move frame f (its offsets, with the followed parts replaced
    // by its owner's joint's, as modelUpdateAttachments does; used for posing it and for particles linked to it)
    void fx_place(FxModel& fm, int f, float pos[3], float rot[3], float scl[3]) {
        {
            std::copy(fm.off_pos, fm.off_pos + 3, pos);
            std::copy(fm.off_rot, fm.off_rot + 3, rot);
            std::copy(fm.off_scl, fm.off_scl + 3, scl);
            if (fm.sel != 0 && fm.joint != -1) {
                float jp[3], jr[3], js3[3];
                bool have = false;
                if (fm.once && fm.snapped) {
                    std::copy(fm.snap_p, fm.snap_p + 3, jp);
                    std::copy(fm.snap_r, fm.snap_r + 3, jr);
                    std::copy(fm.snap_s, fm.snap_s + 3, js3);
                    have = true;
                } else if (owner_joint(fm.box, fm.joint, std::max(fm.start, f), jp, jr, js3)) {
                    have = true;
                    if (fm.once) {
                        std::copy(jp, jp + 3, fm.snap_p);
                        std::copy(jr, jr + 3, fm.snap_r);
                        std::copy(js3, js3 + 3, fm.snap_s);
                        fm.snapped = true;
                    }
                }
                if (have) {
                    const int k = fm.sel;
                    const bool fp = k == 1 || k == 4 || k == 6 || k == 7, fr = k == 2 || k == 4 || k == 5 || k == 7,
                               fs = k == 3 || k == 5 || k == 6 || k == 7;
                    if (fp) {
                        // the joint's place + its turn applied to the offset (Rz Ry Rx, X first)
                        const float cx = std::cos(jr[0]), sx = std::sin(jr[0]), cy = std::cos(jr[1]), sy = std::sin(jr[1]),
                                    cz = std::cos(jr[2]), sz = std::sin(jr[2]);
                        float v[3] = {fm.off_pos[0], fm.off_pos[1], fm.off_pos[2]};
                        float a = v[1] * cx - v[2] * sx, b = v[1] * sx + v[2] * cx;  // Rx
                        v[1] = a; v[2] = b;
                        a = v[0] * cy + v[2] * sy; b = -v[0] * sy + v[2] * cy;     // Ry
                        v[0] = a; v[2] = b;
                        a = v[0] * cz - v[1] * sz; b = v[0] * sz + v[1] * cz;     // Rz
                        v[0] = a; v[1] = b;
                        for (int c = 0; c < 3; ++c) pos[c] = jp[c] + v[c];
                    }
                    if (fr) for (int c = 0; c < 3; ++c) rot[c] = jr[c] + fm.off_rot[c];
                    if (fs) for (int c = 0; c < 3; ++c) scl[c] = js3[c] * fm.off_scl[c];
                }
            }
        }
    }
    // A joint of a move model in the battle's space at move frame f (its place, its chain's turn and scale with the
    // model's own added, as GSpartGetTransform gives them), for particle entries linked to that model (entry flags 1)
    bool fx_joint(FxModel& fm, int joint, int f, float p[3], float r[3], float s[3]) {
        if (joint < 0 || joint >= fm.inst->model.joint_count) return false;
        // In the target's half a linked emitter reads its model's pose 2 frames earlier than the one drawn. Likely, from
        // the game's model log on Frenzy Plant 338 (2026-11-21): its seven streak emitters ("once", follow 4) took the
        // vine's joints at the pose of their start ticks (the first: vine frame 22 at tick 45, Studio's joint 79 there
        // to 0.00), where Studio read frame 24 (its target-half model lag -1 plus the tick rounding); with the shift
        // they start on the ground as in the game, not high on the vines. A/B on 14 matching moves: unchanged overall
        // (Surf closer, Muddy Water a touch further). The user's half keeps 0 (Mist's emitters matched the game's
        // particle log there). ORRE_FXJOINT_SHIFT=<n> sets it for both halves (a test switch only)
        static const char* sh = std::getenv("ORRE_FXJOINT_SHIFT");
        const int shift = sh ? std::atoi(sh) : (fm.box ? 2 : 0);
        int t = std::max(0, f - fm.start - fm.lag - shift) * fm.rate;  // (the model's pose: fm.lag behind)
        if (fm.anim >= 0) t = std::min(t, std::max(0, fm.len - 1));
        else t = 0;
        const int nj = fm.inst->model.joint_count;
        std::vector<float> w((size_t)nj * 12), ws((size_t)nj * 3);
        if (!orre_model_joint_world(&fm.inst->model, fm.anim, (float)t, w.data(), ws.data())) return false;
        float pos[3], rot[3], scl[3];
        fx_place(fm, f, pos, rot, scl);
        // (a following model's root transform is replaced by its placement: the joint's place taken from the root's
        // frame, root_replaced)
        float jw[3] = {w[(size_t)joint * 12 + 3], w[(size_t)joint * 12 + 7], w[(size_t)joint * 12 + 11]};
        float inv[9], it[3];
        const bool attached = root_inverse(fm, w.data(), inv, it);
        if (attached) {
            const float q[3] = {jw[0], jw[1], jw[2]};
            for (int r = 0; r < 3; ++r) jw[r] = inv[3 * r] * q[0] + inv[3 * r + 1] * q[1] + inv[3 * r + 2] * q[2] + it[r];
        }
        const float J[3] = {jw[0] * scl[0], jw[1] * scl[1], jw[2] * scl[2]};
        const float cx = std::cos(rot[0]), sx = std::sin(rot[0]), cy = std::cos(rot[1]), sy = std::sin(rot[1]),
                    cz = std::cos(rot[2]), sz = std::sin(rot[2]);
        const float R[3][3] = {{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
                               {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
                               {-sy, cy * sx, cy * cx}};
        for (int i = 0; i < 3; ++i) p[i] = pos[i] + R[i][0] * J[0] + R[i][1] * J[1] + R[i][2] * J[2];
        float cr[3] = {0, 0, 0}, cs[3] = {1, 1, 1};
        orre_model_part_chain(&fm.inst->model, fm.anim, (float)t, joint, cr, cs);
        // The turn and size: GSpartGetTransform sums the rotate fields and multiplies the scale fields of every joint
        // from the part up to the root (gs_part.c, verified from the code). A following model's placement goes into
        // its override matrix (transformOverride: GSmodelSetRotation / SetScale store overrideRotation, not the root
        // joint's fields), so its chain is the file's own values only, the root's included; a model placed without a
        // part has its placement written into the root joint, so the chain carries it. Verified 2026-11-21 on Frenzy
        // Plant 338's streak emitters (follow 4): the game's generator turn was (-1.427, 1.592, -1.563), Studio's
        // chain alone, where adding the vine's -90 degree placement gave y 0.021
        for (int i = 0; i < 3; ++i) {
            r[i] = attached ? cr[i] : cr[i] + rot[i];
            s[i] = attached ? cs[i] : cs[i] * scl[i];
        }
        return true;
    }
    // The carry (Dig, Dive, Waterfall, Earthquake's target half; _wazaSequenceModelEntryStart / Stop, verified from the
    // code): a model entry with carry set attaches its owner to the model's joint carry_joint (GSmodelAttachToGSpart with
    // the entry's follow mode), the model placed at the owner. When the entry stops, the owner is put back at the model's
    // place (its own). Every carry entry on the disc that has a model uses follow mode 1 (verified, a scan of all 355
    // moves, 2026-11-20): the owner's place follows the joint, its turn and size stay its own. So here: the owner's
    // displacement in the battle's space at move frame f (the joint's place minus the owner's home), 0 when none. A held
    // model (flags 2) stays on its last frame for the rest of the move, and the owner there with it (Dig: underground
    // until its second turn). The pivot
    // and keep-the-displacement flags (0x10, 8) appear on no carry entry with a model, so they are not built.
    // (kCarryLag: the carried owner follows its carrier's pose, which runs kModelLag behind the entry: no extra lag of
    // its own. *Updated 2026-11-20:* this was one frame of the carry's own; the Swords Dance close-up showed the move
    // models' animations as a whole run 2 frames behind, which also fits Withdraw's ~1.6)
    // *Updated 2026-11-21:* one more frame (kModelLag + 1). Double Team's carrier flips Espeon to the other side every
    // frame (its joint's x alternates sign frame by frame), so the frame's parity shows which pose the owner takes: at
    // kModelLag Studio's Espeon stood exactly where the game's after-image was, in every frame of a close-up every 3
    // frames (verified side by side); one frame more puts it where the game's is. Likely why: the owner is updated
    // before the effect model that carries it (the Pokémon's model is made first, and GSmodelAdvanceAll walks models in
    // order), so the attachment reads the carrier's pose from the frame before (not read in the code yet)
    static constexpr int kCarryLag = kModelLag + 1;
    bool carry_offset(bool box, int f, float d[3]) {
        d[0] = d[1] = d[2] = 0;
        if (once < 0) return false;
        for (FxModel& fm : fx_models) {
            if (!fm.carry || fm.box != box || fm.carry_sel != 1 || fm.inst->model.joint_count <= fm.carry_joint || fm.carry_joint < 0)
                continue;
            const int tr = f - fm.start;
            if (tr < 0) continue;
            if (fm.anim >= 0 && (tr - fm.lag) * fm.rate >= fm.len && !fm.held) continue;  // (held: for the rest of the move)
            int t = std::max(0, tr - (fm.lag + kCarryLag - kModelLag)) * fm.rate;  // (the carrier's pose a frame behind)
            if (fm.anim >= 0) t = std::min(t, std::max(0, fm.len - 1));
            std::vector<float> w((size_t)fm.inst->model.joint_count * 12), ws((size_t)fm.inst->model.joint_count * 3);
            if (!orre_model_joint_world(&fm.inst->model, fm.anim, (float)t, w.data(), ws.data())) continue;
            const float J[3] = {w[(size_t)fm.carry_joint * 12 + 3] * fm.off_scl[0], w[(size_t)fm.carry_joint * 12 + 7] * fm.off_scl[1],
                                w[(size_t)fm.carry_joint * 12 + 11] * fm.off_scl[2]};
            // the model's turn (Rz Ry Rx of its offsets, as fx_models_at)
            const float* r = fm.off_rot;
            const float cx = std::cos(r[0]), sx = std::sin(r[0]), cy = std::cos(r[1]), sy = std::sin(r[1]), cz = std::cos(r[2]),
                        sz = std::sin(r[2]);
            const float R[3][3] = {{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
                                   {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
                                   {-sy, cy * sx, cy * cx}};
            // the joint's place is pos + R J; the owner's home is the model's place (pos): the displacement is R J
            for (int c = 0; c < 3; ++c) d[c] = R[c][0] * J[0] + R[c][1] * J[1] + R[c][2] * J[2];
            return true;
        }
        return false;
    }
    // The user's whole carry transform in its own space (M: rotation-scale rows M[0..8], translation M[9..11]) at move
    // frame f, for every follow mode (*added 2026-11-20*: the self-aimed third parts use 4, position and turn: Dig's
    // second turn, and 6, position and size: Withdraw): the followed parts are the joint's (its world turn and scale
    // in the battle), the others the owner's own (GSmodelAttachToGSpart's selector; the offsets left unset, so the turn
    // stays the owner's and the size 1). In the battle the owner's turn is replaced by R Rj (R the model's placement
    // turn, the owner's own; Rj the joint's), so in the Pokémon's own space, where its own turn is the identity, it
    // becomes A Rj A^T with A = (battle -> Pokémon space) R
    bool carry_user_xf(int f, float M[12]) {
        for (int k = 0; k < 12; ++k) M[k] = (k == 0 || k == 4 || k == 8) ? 1.0f : 0.0f;
        if (once < 0) return false;
        for (FxModel& fm : fx_models) {
            if (!fm.carry || fm.box || fm.carry_sel <= 0 || fm.carry_sel > 7 || fm.carry_joint < 0 ||
                fm.carry_joint >= fm.inst->model.joint_count)
                continue;
            const int tr = f - fm.start;
            if (tr < 0) continue;
            if (fm.anim >= 0 && (tr - fm.lag) * fm.rate >= fm.len && !fm.held) continue;
            int t = std::max(0, tr - (fm.lag + kCarryLag - kModelLag)) * fm.rate;  // (the carrier's pose a frame behind)
            if (fm.anim >= 0) t = std::min(t, std::max(0, fm.len - 1));
            const int nj = fm.inst->model.joint_count;
            std::vector<float> w((size_t)nj * 12), ws((size_t)nj * 3);
            if (!orre_model_joint_world(&fm.inst->model, fm.anim, (float)t, w.data(), ws.data())) continue;
            const float* J = &w[(size_t)fm.carry_joint * 12];
            const int k = fm.carry_sel;
            const bool fp = k == 1 || k == 4 || k == 6 || k == 7, fr = k == 2 || k == 4 || k == 5 || k == 7,
                       fs = k == 3 || k == 5 || k == 6 || k == 7;
            // the joint's turn (its columns normalised) and its chain's scale (their lengths) x the model's own scale
            float Rj[3][3], sj[3];
            for (int c = 0; c < 3; ++c) {
                const float len = std::sqrt(J[c] * J[c] + J[4 + c] * J[4 + c] + J[8 + c] * J[8 + c]);
                sj[c] = len * fm.off_scl[c];
                for (int r = 0; r < 3; ++r) Rj[r][c] = len > 0 ? J[4 * r + c] / len : (r == c ? 1.0f : 0.0f);
            }
            // A: battle (Rz Ry Rx of the placement turn) then into the Pokémon's space ((x, y, z) -> (-z, y, x))
            const float* r = fm.off_rot;
            const float cx = std::cos(r[0]), sx = std::sin(r[0]), cy = std::cos(r[1]), sy = std::sin(r[1]), cz = std::cos(r[2]),
                        sz = std::sin(r[2]);
            const float R[3][3] = {{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
                                   {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
                                   {-sy, cy * sx, cy * cx}};
            float A[3][3];
            for (int c = 0; c < 3; ++c) {
                A[0][c] = -R[2][c];
                A[1][c] = R[1][c];
                A[2][c] = R[0][c];
            }
            // L = A (fr ? Rj : I) diag(fs ? sj : 1) A^T
            float B[3][3], L[3][3];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    float v = 0;
                    for (int q = 0; q < 3; ++q) v += A[i][q] * (fr ? Rj[q][j] : (q == j ? 1.0f : 0.0f));
                    B[i][j] = v * (fs ? sj[j] : 1.0f);
                }
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    float v = 0;
                    for (int q = 0; q < 3; ++q) v += B[i][q] * A[j][q];
                    L[i][j] = v;
                }
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) M[3 * i + j] = L[i][j];
            // the place: the joint's, as a displacement A (scale x joint position) from the owner's home
            if (fp) {
                const float Jt[3] = {J[3] * fm.off_scl[0], J[7] * fm.off_scl[1], J[11] * fm.off_scl[2]};
                for (int i = 0; i < 3; ++i) M[9 + i] = A[i][0] * Jt[0] + A[i][1] * Jt[1] + A[i][2] * Jt[2];
            }
            return true;
        }
        return false;
    }
    // the same displacement in the Pokémon's space (battle (x, y, z) -> (-z, y, x))
    bool carry_offset_studio(bool box, int f, float s[3]) {
        float d[3];
        if (!carry_offset(box, f, d)) { s[0] = s[1] = s[2] = 0; return false; }
        s[0] = -d[2];
        s[1] = d[1];
        s[2] = d[0];
        return true;
    }
    void inst_build(Inst& in) {
        for (int t = 0; t < in.model.texture_count; ++t)
            in.textures.push_back(make_texture(in.model.textures[t].width, in.model.textures[t].height, in.model.textures[t].rgba));
        for (int i = 0; i < in.model.mesh_count; ++i) {
            const OrreMesh& m = in.model.meshes[i];
            if (m.vertex_count == 0 || m.cull == 3) continue;
            Part p;
            wgpu::BufferDescriptor vb;
            vb.size = (uint64_t)m.vertex_count * sizeof(OrreVertex);
            vb.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
            p.vbuf = device.CreateBuffer(&vb);
            p.count = (uint32_t)m.vertex_count;
            p.group = material_group(&m, p.mbuf, nullptr, nullptr, nullptr, nullptr, &in.model, &in.textures);
            const DrawState ds = draw_state(m);
            p.pipeline = pipeline(ds);
            p.blended = ds.blend_type != 0;
            p.mesh = i;
            in.parts.push_back(std::move(p));
        }
        std::stable_partition(in.parts.begin(), in.parts.end(), [](const Part& p) { return !p.blended; });
        in.posed.assign(in.model.mesh_count, {});
        for (int i = 0; i < in.model.mesh_count; ++i) in.posed[i].resize(in.model.meshes[i].vertex_count);
        in.shown.assign(in.model.mesh_count, 1);
    }
    // poses it at (anim, frame) (-1: its standing pose) and places it: p' = R p + t (R row-major 3x3 in M[0..8], t in
    // M[9..11]), normals turned by R; then its pictures for that frame
    float last_view[12] = {};  // the Pokémon's model-view as last drawn (3 rows of 4), for the extra models' billboards
    bool have_view = false;
    // (face_camera false: its camera-facing cards stay as modelled, for a model the caller already lines up with the
    // camera: the billboards)
    // (tex_anim: the texture animation to show, when not the joint animation's own: a model entry's +0x08; -2 = anim's)
    void inst_pose(Inst& in, int anim, float frame, const float M[12], bool face_camera = true, int tex_anim = -2,
                   float tex_frame = 0) {
        std::vector<OrreVertex*> out(in.model.mesh_count);
        for (int i = 0; i < in.model.mesh_count; ++i) out[i] = in.posed[i].data();
        // its billboards (glow cards: Ice Beam's frost glow) face the camera: the view in its own space is the
        // Pokémon's model-view after its placement (V [R | t])
        float v[12];
        if (have_view)
            for (int r = 0; r < 3; ++r) {
                const float* L = last_view + 4 * r;
                for (int c = 0; c < 3; ++c) v[4 * r + c] = L[0] * M[0 + c] + L[1] * M[3 + c] + L[2] * M[6 + c];
                v[4 * r + 3] = L[0] * M[9] + L[1] * M[10] + L[2] * M[11] + L[3];
            }
        if (!orre_model_pose_view(&in.model, anim, frame, have_view && face_camera ? v : nullptr, out.data())) return;
        orre_model_visibility(&in.model, anim, std::floor(frame), in.shown.data());
        for (auto& part : in.parts) {
            for (OrreVertex& v : in.posed[part.mesh]) {
                const float x = v.pos[0], y = v.pos[1], z = v.pos[2];
                v.pos[0] = M[0] * x + M[1] * y + M[2] * z + M[9];
                v.pos[1] = M[3] * x + M[4] * y + M[5] * z + M[10];
                v.pos[2] = M[6] * x + M[7] * y + M[8] * z + M[11];
                const float a = v.nrm[0], b = v.nrm[1], c = v.nrm[2];
                v.nrm[0] = M[0] * a + M[1] * b + M[2] * c;
                v.nrm[1] = M[3] * a + M[4] * b + M[5] * c;
                v.nrm[2] = M[6] * a + M[7] * b + M[8] * c;
            }
            queue.WriteBuffer(part.vbuf, 0, in.posed[part.mesh].data(), (uint64_t)part.count * sizeof(OrreVertex));
        }
        // its texture animation's pictures and colours at the frame (as apply_textures does for the Pokémon)
        const size_t n = (size_t)in.model.mesh_count * ORRE_MAX_STAGES;
        std::vector<int> ft(n, -1);
        std::vector<float> fb(n, 0), fx(n * 4, 0), fm((size_t)in.model.mesh_count * ORRE_MATERIAL_FLOATS, 0);
        const int ta = tex_anim == -2 ? anim : tex_anim;
        const float tf = tex_anim == -2 ? frame : tex_frame;
        if (ta >= 0 && orre_model_texture_frame2(&in.model, ta, std::floor(tf), ft.data(), fb.data(), fx.data()) &&
            orre_model_material_frame(&in.model, ta, std::floor(tf), fm.data()))
            for (auto& p : in.parts) {
                const int* t = &ft[(size_t)p.mesh * ORRE_MAX_STAGES];
                const float* b = &fb[(size_t)p.mesh * ORRE_MAX_STAGES];
                const float* x = &fx[(size_t)p.mesh * ORRE_MAX_STAGES * 4];
                const float* c = &fm[(size_t)p.mesh * ORRE_MATERIAL_FLOATS];
                if (std::equal(t, t + ORRE_MAX_STAGES, p.tex) && std::equal(b, b + ORRE_MAX_STAGES, p.blend) &&
                    std::equal(x, x + ORRE_MAX_STAGES * 4, p.xf) && std::equal(c, c + ORRE_MATERIAL_FLOATS, p.mat))
                    continue;
                std::copy(t, t + ORRE_MAX_STAGES, p.tex);
                std::copy(b, b + ORRE_MAX_STAGES, p.blend);
                std::copy(x, x + ORRE_MAX_STAGES * 4, p.xf);
                std::copy(c, c + ORRE_MATERIAL_FLOATS, p.mat);
                p.group = material_group(&in.model.meshes[p.mesh], p.mbuf, p.tex, p.blend, p.xf, p.mat, &in.model, &in.textures);
            }
    }
    float yaw = 0.6f, pitch = 0.15f, dist = 1.0f;
    float hi[4] = {0.15f, 0.17f, 0.19f, 1}, lo[4] = {0.09f, 0.1f, 0.11f, 1};
    bool dragging = false;
    int last_x = 0, last_y = 0;
    OrreModel model{};

    // ---- particles (2026-11-19, particles step 1: the model's own effects) ----
    // The core simulates them (orre_particles_*, the game's particle system ported); the view feeds it the animation's
    // whole frames as the battle does: the frame's particle keys start emitters, finished ones are reclaimed, each
    // emitter moves to its joint, then two 60 Hz steps (gsParticleUpdate). A new model starts them afresh (seed 1).
    OrreParticleSystem* ps = nullptr;
    bool particles_on = true;
    struct LiveGen { int handle, joint; };
    std::vector<LiveGen> live;
    std::map<int, std::vector<OrreParticleKey>> keys;  // per animation
    int sim_anim = -1, sim_frame = -1, warm = 0;
    bool first_keys = true;  // the model's first frame since loading (its frame-0 keys fire twice)
    std::vector<float> jw, js;
    wgpu::ShaderModule pshader;
    wgpu::BindGroupLayout player1;
    wgpu::PipelineLayout pplayout;
    std::map<uint32_t, wgpu::RenderPipeline> ppipes;
    std::map<uint32_t, wgpu::BindGroup> ptex;  // (bank slot << 16 | group << 8 | image) -> texture bind group
    // ---- the move's own effects (2026-11-19, release step 4) ----
    // A move's effect file (WZX; orre_move_wzx) plays beside its body animation, as the battle's wazaSequence does
    // (src/game/wazaSequence.c, wazaSequenceEntry.c; verified from the code): its entries start at their ticks (60 Hz;
    // the battle adds 2 per 30 fps frame) against this Pokémon's time points for the file's slot. So far the particle
    // entries play (_wazaSequenceParticleEntryStart ported: the attach point, the follow mode, the effect scale);
    // models, screen effects, sounds and the camera come next. Its banks take particle slots 1 and up (0 is the
    // Pokémon's own). Particles outlive the move, as in the game: they finish on the idle that follows.
    struct MoveSeq {
        OrreWzx* wzx = nullptr;
        int move = 0, part = 1, anim = -1, slot = 0;
        float scale = 1.0f;
        std::vector<int> start, order, state, handle, joint;  // per entry (state 0 waiting, 1 started, 2 failed)
        bool armed = false;    // play_once starts it with the next move animation
        bool running = false;  // its entries are being started
        bool box = false;      // played on the target box (the target's half), not the Pokémon
        int first_frame = 0;   // the move's frame its tick 0 falls on (the target's half: when the user's half ends)
    } seq, tseq, aseq;  // (aseq: the user's third part after the target's half, set_move_after: the drains' return)
    // a started particle entry: its node, the joint it follows (-2: the bounding box's centre; on the box, its own
    // points kBoxGround..kBoxCentre) and whose it is
    // (fx: the index in fx_models of the move model it follows, -1 none: entry flags 1, joint then that model's part)
    struct MoveNode { int handle, joint; bool box; int fx = -1; };
    std::vector<MoveNode> move_nodes;
    int move_tail = 0;  // frames the move's timeline runs past the user's animation, for the target's half
    // The target's half: its camera shot starts when the user's half ends (tshot; verified against the move sweep's
    // camera logs), its content (entries, animation, fades, bolts, the box's hit) kTargetDelay frames later, at
    // tseq.first_frame. Measured 2026-11-19 on the move sweep: Umbreon's knock-back (its joint 42 in the camera log
    // starts moving 2 frames after Studio's), Fire Punch's burst (Studio's at frame 73 = the game's at 75) and Thunder's
    // first bolt all run 2 frames later than the shot's start (likely: the sequence's first update and the first
    // particle / animation step each wait a frame). The user's half showed 1 frame on Flamethrower's flames; *applied
    // 2026-11-21* as kUserDelay (below), without moving the target's start, which the camera logs confirm
    static constexpr int kTargetDelay = 2;
    // The user's half: its content (entries, fades, models, particles) and the user's pose start kUserDelay frames
    // after Play, while the target's shot still starts at user_end_frame (the camera logs confirm that). *Added
    // 2026-11-21:* measured a frame at a time against the move sweep (Studio rendered at frames around each game
    // picture, the whole picture's difference and the floor's brightness): Earthquake 89's ring, Espeon's pose and the
    // stage darkening all matched Studio's picture of one frame earlier (three game frames), Growl 45's darkening and
    // its lifting the same, as Flamethrower's flames had shown (verified readings). Likely the same first update the
    // target's half waits (kTargetDelay), here one frame
    // (ORRE_USER_DELAY=0 restores the earlier timing, for comparing; a test switch only)
    const int kUserDelay = std::getenv("ORRE_USER_DELAY") ? std::atoi(std::getenv("ORRE_USER_DELAY")) : 1;
    int tshot = 0;

    // ---- the target box (snatcherLeo, 2026-11-19: "a random 3D box, very simple, and will be fine in any background, will
    // have a "hit animation", something very clear and simple. And an idle pose. That's all we'd need."; design in
    // studio/moves-tab-plan.md 8b). Studio's own, nothing from the disc: a light warm-grey cube standing where the
    // game's target stands (x = +40, facing the user), still while idle; when the target's half starts it takes the hit:
    // a quick shove away and a tilt back, a short white flash, then settling over most of a second. Its attach points
    // are three places of its own (ground, the upper front, the centre), so every point a move's target half asks for
    // lands somewhere sensible; its "hit slot" time points are Studio's (kBoxTimes, 60 Hz like a PKX's).
    static constexpr float kBoxSize = 14.0f;
    static constexpr int kBoxHitFrames = 24, kBoxGround = -10, kBoxFront = -11, kBoxCentre = -12;
    static constexpr int kBoxTimes[16] = {0, 16, 40};
    bool box_on = false;     // the move has a target half: the box stands at the target's place
    bool user_hidden = false, box_hidden = false;  // a control step hid the user / the box (while the move plays)
    double tgt_phase = 0;  // the target's idle frame when the clock last restarted (target_phase_at), so it never jumps
    // ---- the battle camera (release step 4; core battle_camera.cpp): while a move with its effects plays, the shot the
    // battle builds films it, the user's half then the target's on the box. Run frame by frame along the timeline from
    // the move's start (a step back replays it), a new seed for each Play
    OrreBattleCamera* bcam = nullptr;
    int cam_frame = -1;
    uint32_t cam_seed = 1, plays = 0;
    bool cam_live = false;  // a shot is running for the frame shown
    bool cam_on = true;     // the Battle camera switch (off: Studio's own camera stays during moves)
    OrreCamOwner user_owner(int f) {
        OrreCamOwner o{};
        o.pos[0] = kUserX;
        o.rot_y = kTurn;
        for (int k = 0; k < 3; ++k) { o.box_min[k] = model.bounds_min[k]; o.box_max[k] = model.bounds_max[k]; }
        o.size_class = model.size_class;
        o.facing = -1;  // (the grid's mirrored side: x < 0, turned +90 degrees; corrected 2026-11-19, was +1)
        o.is_pokemon = 1;
        o.species = model.model_id;  // (owner +0x70: model 0x13A, Wailord, always gets the wide shot; verified 314 = WAILORD)
        o.slot = seq.slot;
        o.tp_count = model.slot_time_count[seq.slot];
        std::copy(model.slot_times[seq.slot], model.slot_times[seq.slot] + 16, o.tp);
        user_aim(f, o.aim);
        return o;
    }
    // the joint the move's camera aims at: the slot's attach point 2 (the body's middle), or the one the WZX header
    // names (mode 5: header +0x0C), at frame f, in the battle's space
    void user_aim(int f, float out[3]) {
        OrreWzxHeader h{};
        orre_wzx_header(seq.wzx, &h);
        const int idx = h.mode == 5 && h.value >= 0 && h.value < 16 ? h.value : 2;
        const int j = model.slot_points[seq.slot][idx];
        float s[3] = {(model.bounds_min[0] + model.bounds_max[0]) * 0.5f, (model.bounds_min[1] + model.bounds_max[1]) * 0.5f,
                      (model.bounds_min[2] + model.bounds_max[2]) * 0.5f};
        int ua, pf;
        user_anim(seq.anim, f, ua, pf);  // (the idle during a lead-in)
        if (j >= 0 && j < model.joint_count && orre_model_joint_world(&model, ua, (float)pf, jw.data(), js.data())) {
            s[0] = jw[(size_t)j * 12 + 3];
            s[1] = jw[(size_t)j * 12 + 7];
            s[2] = jw[(size_t)j * 12 + 11];
        }
        to_battle(s, out);
    }
    OrreCamOwner box_owner(int f) {
        OrreCamOwner o{};
        o.pos[0] = kTargetX;
        o.rot_y = -kTurn;
        const float w = kBoxSize * 0.5f;
        o.box_min[0] = o.box_min[2] = -w;
        o.box_max[0] = o.box_max[2] = w;
        o.box_max[1] = kBoxSize;
        o.size_class = 0;
        o.facing = 1;  // (the grid's unmirrored side: x > 0, turned -90 degrees; corrected 2026-11-19, was -1)
        o.is_pokemon = 1;
        o.species = -1;
        OrreWzxHeader h{};
        orre_wzx_header(tseq.wzx, &h);
        o.slot = h.slot;
        o.tp_count = 3;
        std::copy(kBoxTimes, kBoxTimes + 16, o.tp);
        float r[3];
        box_joint(kBoxCentre, box_pose(f), o.aim, r);
        if (has_target()) {
            // the target Pokémon: its own bounding box, size class, model id, slot and time points
            const OrreModel& tm = target->model;
            for (int k = 0; k < 3; ++k) { o.box_min[k] = tm.bounds_min[k]; o.box_max[k] = tm.bounds_max[k]; }
            o.size_class = tm.size_class;
            o.species = tm.model_id;
            o.slot = std::clamp(tseq.slot, 0, 16);
            o.tp_count = tm.slot_time_count[o.slot];
            std::copy(tm.slot_times[o.slot], tm.slot_times[o.slot] + 16, o.tp);
            target_aim(f, o.aim);
        }
        return o;
    }
    // the target Pokémon's aimed joint (the slot's point 2, or the one a mode-5 header names) in the battle's space
    void target_aim(int f, float out[3]) {
        OrreWzxHeader h{};
        orre_wzx_header(tseq.wzx, &h);
        const OrreModel& tm = target->model;
        const int idx = h.mode == 5 && h.value >= 0 && h.value < 16 ? h.value : 2;
        const int j = tm.slot_points[std::clamp(tseq.slot, 0, 16)][idx];
        float r[3], s[3];
        if (!target_joint(j, f, out, r, s)) {
            const float c[3] = {(tm.bounds_min[0] + tm.bounds_max[0]) * 0.5f, (tm.bounds_min[1] + tm.bounds_max[1]) * 0.5f,
                                (tm.bounds_min[2] + tm.bounds_max[2]) * 0.5f};
            out[0] = kTargetX - c[2]; out[1] = c[1]; out[2] = c[0];
        }
    }
    // ---- colour fades (screen effect kind 0; the entries spec, verified from the code): each runs on its own clock
    // from its entry's start; the latest started wins per target. The user's half: target 2 tints the Pokémon; the
    // target's half: target 2 tints the box. Target 1 (the stage's models) tints Studio's stage backdrop (Studio has no
    // battle room: likely the nearest look), target 0 is a colour over the whole picture. A hold keeps its colour until
    // the move ends
    uint8_t box_fade[4] = {128, 128, 128, 255}, screen_fade[4] = {};
    bool box_fade_on = false, screen_fade_on = false;
    void fades_at(int f, uint8_t out[3][4], bool on[3], uint8_t boxc[4], bool* box_on_out) {
        on[0] = on[1] = on[2] = false;
        *box_on_out = false;
        if (f < 0 || once < 0) return;
        int best[4] = {-1, -1, -1, -1};  // the latest start per target (3 = the box)
        for (MoveSeq* S : {&seq, &tseq, &aseq}) {
            if (!S->wzx || !S->running || S->anim != once || f < S->first_frame) continue;
            // (the user's half ends when the target's starts, and its held fades with it: the game's Thunder has the
            // stage back to its own colour for the first 15 ticks of the target's half, before that half's own
            // darkening begins; likely, from the move sweep's pictures, 2026-11-19). *Updated 2026-11-20:* they end when
            // the target's half starts (tseq.first_frame, kTargetDelay after the cut), not at the cut: the game's
            // Earthquake still has the user's held darkening just after the cut, and Thunder's 15 bright ticks fit both
            const bool ended = S == &seq && tseq.wzx && box_on && f >= tseq.first_frame;
            const int tick = 2 * (f - S->first_frame);
            for (size_t i = 0; i < S->start.size(); ++i) {
                OrreWzxFade fd{};
                if (!orre_wzx_fade(S->wzx, (int)i, &fd)) continue;
                const int s = (S->start[i] + 1) / 2 * 2;  // (the first frame whose tick reaches its start)
                if (tick < s) continue;
                // (the latest started per target wins even once it has run out: a newer fade replaces a held one, and
                // when it ends the tint is gone. The game's Thunderbolt: the user's half darkens the stage and holds,
                // the target's half brings it back, and the stage stays bright to the end; move sweep, 2026-11-19)
                const int t = fd.target == 2 && S->box ? 3 : std::min(fd.target, 2);
                const int abs_start = 2 * S->first_frame + S->start[i];
                if (abs_start < best[t]) continue;
                best[t] = abs_start;
                uint8_t c[4];
                const bool live = !ended && orre_wzx_fade_colour(&fd, tick - s, c) != 0;
                if (t == 3) { if (live) std::copy(c, c + 4, boxc); *box_on_out = live; }
                else { if (live) std::copy(c, c + 4, out[t]); on[t] = live; }
            }
        }
    }
    // brings the camera to the move's frame f
    void cam_to(int f) {
        cam_live = false;
        if (!seq.wzx || !seq.running || once < 0 || once != seq.anim) return;
        if (!bcam) bcam = orre_bcam_create(cam_seed);
        if (f < cam_frame || cam_frame < 0) {
            orre_bcam_reset(bcam, cam_seed);
            OrreWzxHeader h{};
            orre_wzx_header(seq.wzx, &h);
            const OrreCamOwner o = user_owner(0);
            orre_bcam_start(bcam, &o, 1, h.flags);
            cam_frame = 0;
        }
        while (cam_frame < f) {
            ++cam_frame;
            if (tseq.wzx && box_on && cam_frame == tshot) {
                // the target's half takes the camera (battleCameraStartWaza on the target)
                OrreWzxHeader h{};
                orre_wzx_header(tseq.wzx, &h);
                const OrreCamOwner o = box_owner(cam_frame);
                orre_bcam_start(bcam, &o, 1, h.flags);
                continue;
            }
            float aim[3];
            if (tseq.wzx && box_on && cam_frame > tshot) {
                float r[3];
                if (has_target()) target_aim(cam_frame, aim);
                else box_joint(kBoxCentre, box_pose(cam_frame), aim, r);
            } else {
                user_aim(cam_frame, aim);
            }
            orre_bcam_update(bcam, 2, aim);
        }
        cam_live = true;
    }
    int box_frame = -1;      // the move frame the box's pose is for (-1: idle)
    struct BoxPose { float push = 0, tilt = 0, flash = 0; };
    BoxPose box_pose(int f) const {
        BoxPose b;
        if (!box_on || !tseq.wzx || f < 0 || f < tseq.first_frame) return b;
        // (the hit lands on its time point 0, after the half's lead-in, as a Pokémon's hit animation does)
        const float t = (float)(f - tseq.first_frame - half_lead(tseq));
        if (t < 0 || t >= kBoxHitFrames) return b;
        // in 3 frames to the full shove, then easing back (quadratic) over the rest
        const float k = t < 3 ? t / 3.0f : (1.0f - (t - 3) / (kBoxHitFrames - 3)) * (1.0f - (t - 3) / (kBoxHitFrames - 3));
        b.push = 4.0f * k;
        b.tilt = 0.24f * k;
        b.flash = t < 4 ? 1.0f - t / 4.0f : 0.0f;
        return b;
    }
    // a point of the box (its own space: origin under its centre on the ground, +z its front) in the battle's space
    void box_point(const BoxPose& b, const float l[3], float g[3]) const {
        // tilt back about its x axis (Rx(-tilt)), pushed back along -z, then turned to face the user (Ry(-90 degrees):
        // its +z becomes the battle's -x) and set at the target's place
        const float c = std::cos(-b.tilt), s = std::sin(-b.tilt);
        const float x = l[0], y = c * l[1] - s * l[2], z = s * l[1] + c * l[2] - b.push;
        g[0] = kTargetX - z;
        g[1] = y;
        g[2] = x;
    }
    void box_joint(int joint, const BoxPose& b, float p[3], float r[3]) const {
        const float h = kBoxSize;
        const float l[3] = {0.0f, joint == kBoxGround ? 0.0f : (joint == kBoxFront ? 0.8f * h : 0.5f * h),
                            joint == kBoxFront ? 0.5f * h : 0.0f};
        box_point(b, l, p);
        r[0] = -b.tilt;
        r[1] = -kTurn;
        r[2] = 0;
    }
    int tail_anim = -1, tail = 0;  // a held ending (set_play_tail: the faint's time on the ground)
    // The battle's space (release step 4): the particles live where the game puts them, the user standing at
    // x = -40 turned +90 degrees about y, the target at x = +40 facing it (the move viewer's layout,
    // battleGridGetDistance and wazaViewerSpawnTarget, src/game/wazaViewer.c; verified from the project's camera logs). Studio
    // draws its Pokémon at the origin facing +z, so a point of the model goes into the battle's space as (z - 40, y, -x)
    // and back as (-z, y, x + 40); the drawing turns the particles' view to match. So an effect flying toward the
    // target flies toward where the target box will stand (studio (0, 0, 80)).
    // *Corrected 2026-11-19 (snatcherLeo: the camera "is close" but not there):* the two sides stand at x = -/+ 40 x the grid's
    // spacing factor for the largest size class on the field (battleGridUpdate and battleGridGetBaseDistance,
    // src/game/battle/battle_grid.c, constants read from main.dol: 0.875 for classes -2 / -1, 1 for 0, 1.4, 1.8 and 2.75
    // for 1-3; the box counts as class 0), so a big Pokémon stands further off; set by set_grid. The side at -x, turned
    // +90 degrees, is the grid's mirrored side (facing -1), the user's here
    float kUserX = -40.0f, kTargetX = 40.0f;
    static constexpr float kTurn = 1.5707964f;
    void set_grid(int size_class) {
        float k = 1.0f;
        switch (std::max(size_class, 0)) {
            case 1: k = 1.4f; break;
            case 2: k = 1.8f; break;
            case 3: k = 2.75f; break;
            default: k = 1.0f; break;  // (the box's class 0 is the field's largest when the Pokémon is smaller)
        }
        kUserX = -40.0f * k;
        kTargetX = 40.0f * k;
    }
    void to_battle(const float s[3], float g[3]) const {
        const float x = s[0], y = s[1], z = s[2];
        g[0] = z + kUserX;
        g[1] = y;
        g[2] = -x;
    }
    std::map<const OrreParticleBank*, int> move_banks;  // bank -> particle slot
    std::map<std::pair<const void*, int>, wgpu::BindGroup> bolt_tex;  // a bolt entry's picture, by (file, entry)
    // ---- billboards (screen effect kind 11: the sandstorm; _billboardUpdate, verified from the code): the entry's
    // model held at 1.1 x the near plane in front of the camera, scaled so its bounds fill the view (2 x d x
    // tan(fov / 2) high, times the aspect wide, flattened in depth), drawn in the second pass while its animation runs
    // (from frame 0, one frame a battle frame; to its end unless it loops). Built when first shown, by (file, entry)
    std::map<std::pair<const void*, int>, std::unique_ptr<Inst>> billboard_inst;
    std::vector<Inst*> billboard_now;  // posed for this frame
    // The leaves (screen effect kind 5; core leaves.cpp): up to 32 copies of an entry's model per (file, entry), and
    // the copies flying this frame (drawn with the models in the first pass: the leaf code sets no overlay flag on them,
    // verified; that nothing else does, likely)
    std::map<std::pair<const void*, int>, std::vector<std::unique_ptr<Inst>>> leaf_inst;
    std::vector<Inst*> leaves_now;
    // The env-map shine (screen effect kind 8: Steel Wing, Iron Tail, Metal Claw, Mirror Coat, Magic Coat; core
    // orre_wzx_envmap; verified from the code): _envMapUpdate draws the other visible models (the room, the target,
    // the move's models; not the owner, no particles) from the battle camera into a screen-sized texture
    // (GSgfxRenderLayer), and GSmodelEnableEnvMap gives every material of the owner one extra stage with it
    // (_matGSmatEnableEnvMapExt): light map ext (drawn over the lit colour), coordinates by mode, blend or modulate by
    // type, at the key's value. Here: env_tex drawn the same way each frame it runs, and each of the Pokémon's parts
    // drawn with a copy of its material that has the extra stage (env_groups, rebuilt when the value changes).
    // On every OS (WebGPU through Dawn; no platform code)
    wgpu::Texture env_tex, env_depth;
    int env_w = 0, env_h = 0;
    bool env_now = false;     // a shine runs this frame
    OrreWzxEnvMap env_em{};   // its settings
    float env_value = 0;      // and its value now
    float env_built = -1;     // the value env_groups were built with (-1: none built)
    std::vector<wgpu::BindGroup> env_groups;  // per entry of parts
    std::vector<wgpu::Buffer> env_bufs;
    // The screen warp (screen effect kind 12: Psychic, Confusion, Hypnosis, Thunder Wave and 12 more; kWarpShader):
    // the warps running this frame (the place's centre in the Pokémon's space, the half-size, the strength now and the
    // picture), the first pass's picture copied into warp_scene for them (the game re-renders the models into its own
    // screen-sized texture: likely the same picture, without the link-0 particles' small share), drawn at the start of
    // the second pass (guess: where the effect draws among the passes)
    struct WarpNow { float c[3], half, strength; wgpu::Texture pic; };
    std::vector<WarpNow> warps_now;
    std::map<std::pair<const void*, int>, wgpu::Texture> warp_pics;  // a warp entry's picture, by (file, entry)
    wgpu::Texture warp_scene;
    int warp_w = 0, warp_h = 0;
    wgpu::ShaderModule wshader;
    wgpu::BindGroupLayout wlayout1;
    wgpu::RenderPipeline wpipe;
    wgpu::Buffer wvbuf;
    // the warp pipeline, made when first needed
    void warp_pipeline() {
        if (wpipe) return;
        wgpu::ShaderSourceWGSL sw;
        sw.code = kWarpShader;
        wgpu::ShaderModuleDescriptor smd;
        smd.nextInChain = &sw;
        wshader = device.CreateShaderModule(&smd);
        wgpu::BindGroupLayoutEntry le[4];
        le[0].binding = 0;
        le[0].visibility = wgpu::ShaderStage::Fragment;
        le[0].buffer.type = wgpu::BufferBindingType::Uniform;
        le[1].binding = 1;
        le[1].visibility = wgpu::ShaderStage::Fragment;
        le[1].sampler.type = wgpu::SamplerBindingType::Filtering;
        for (int k = 2; k < 4; ++k) {
            le[k].binding = (uint32_t)k;
            le[k].visibility = wgpu::ShaderStage::Fragment;
            le[k].texture.sampleType = wgpu::TextureSampleType::Float;
            le[k].texture.viewDimension = wgpu::TextureViewDimension::e2D;
        }
        wgpu::BindGroupLayoutDescriptor bld;
        bld.entryCount = 4;
        bld.entries = le;
        wlayout1 = device.CreateBindGroupLayout(&bld);
        wgpu::BindGroupLayout both[2] = {layout0, wlayout1};
        wgpu::PipelineLayoutDescriptor pld;
        pld.bindGroupLayoutCount = 2;
        pld.bindGroupLayouts = both;
        wgpu::VertexAttribute a[2];
        a[0] = {nullptr, wgpu::VertexFormat::Float32x3, 0, 0};
        a[1] = {nullptr, wgpu::VertexFormat::Float32x2, 12, 1};
        wgpu::VertexBufferLayout vbl;
        vbl.arrayStride = 20;
        vbl.attributeCount = 2;
        vbl.attributes = a;
        wgpu::ColorTargetState ct;
        ct.format = format;  // (no blending: gfxSetBlendMode(0, ...))
        wgpu::FragmentState fs;
        fs.module = wshader;
        fs.entryPoint = "wfs";
        fs.targetCount = 1;
        fs.targets = &ct;
        wgpu::DepthStencilState ds;
        ds.format = wgpu::TextureFormat::Depth24Plus;
        ds.depthWriteEnabled = wgpu::OptionalBool::False;  // (gfxSetZMode(0, ...): no depth test)
        ds.depthCompare = wgpu::CompareFunction::Always;
        wgpu::RenderPipelineDescriptor pd;
        pd.layout = device.CreatePipelineLayout(&pld);
        pd.vertex.module = wshader;
        pd.vertex.entryPoint = "wvs";
        pd.vertex.bufferCount = 1;
        pd.vertex.buffers = &vbl;
        pd.fragment = &fs;
        pd.depthStencil = &ds;
        pd.primitive.cullMode = wgpu::CullMode::None;  // (gfxSetCullMode(0))
        wpipe = device.CreateRenderPipeline(&pd);
    }
    // ---- after-images (screen effect kind 9, mode 0: _blurRender, src/game/effect/effect_visual.c; verified from the
    // code): while the blur runs, the user is drawn again at earlier moments of its animation before it is drawn
    // itself. Its keys run one after another on the effect's clock (_blurUpdate: 2 ticks a battle frame, a key
    // passed when its ticks are used up; the effect ends after the last); the current key's value v gives the copies,
    // round(30 v) at the battle's rate; they sit at the animation frame now - 0.5 n, then every half frame (the model's
    // rate of 0.5 per tick) up to now, the j-th with alpha 0.5 (j / n)^2 (GSmaterialSetAlpha), alpha-blended
    // ---- the screen blur (kind 9, screen: GSgfxBeginBackFBCapture / _blurDrawCapture, verified from the code): each
    // frame the last finished picture is laid over the new one, alpha 255 x v, at the end of the second pass; so a
    // moving scene leaves fading trails. Studio keeps its last finished picture (prev, copied from the surface after
    // each frame) and draws it the same way while the move plays on (a held frame or a screenshot has no last frame,
    // so it shows none)
    wgpu::Texture prev;
    wgpu::BindGroup prev_group;
    bool can_copy = false;
    // ORRE_FRAME_WATCH=1 (2026-11-22, snatcherLeo's rare white flash on picking a Pokémon, never caught by reading the screen):
    // the frames Studio itself draws, read back from the GPU for the 15 frames after each model change, their
    // brightness logged, so a bright frame shows whether it is Studio's drawing or Windows' compositing (render)
    int watch_frames = 0, watch_seq = 0;
    double watch_prev = -1;
    // The page's copy of each frame (the 3D view drawn into the page; Viewer::share_into): a texture the page made and
    // shares by its DXGI handle, the frame copied into it after it's drawn, a keyed mutex taking turns with the page
    // page mode (the view drawn into the page): frames go into `offscreen`, never to the window's surface, and are asked
    // for by a posted message (frame_posted: one at a time) instead of a repaint, as the window shows nothing
    std::atomic<bool> page{false};  // (read by the pacer thread too)
    wgpu::Texture offscreen;
    std::atomic<bool> frame_posted{false};
    wgpu::SharedTextureMemory share_mem;
    wgpu::Texture share_tex;
    int share_w = 0, share_h = 0;
    // the size of the frame last copied into the shared texture (its top-left part: the texture may be larger, as the
    // page sizes it in steps, 2026-11-22), read by the page to draw just that part (shared_frame_size)
    int share_fw = 0, share_fh = 0;
    bool share_init = false;
    int share_frames = 0;
    int prev_frame = -1;  // the move frame prev holds (-1: none)
    // a blur's value `k` frames after its start (_blurUpdate then _blurRender: 2 ticks a frame, a key passed when its
    // ticks are used up), -1 once it has ended
    static float blur_value(const OrreWzxBlur& bl, int k) {
        int key = 0, c = 0;
        for (int i = 0; i <= k; ++i) {
            while (c >= bl.key[key].ticks) {
                c -= bl.key[key].ticks;
                if (++key >= bl.count) return -1;
            }
            c += 2;
        }
        const auto& kk = bl.key[key];
        return kk.from + (kk.ticks > 0 ? (float)c / (float)kk.ticks : 0.0f) * (kk.to - kk.from);
    }
    // the screen blur's strength at move frame f (the largest of the running ones), 0 for none
    float screen_blur(int f) const {
        float a = 0;
        if (f < 0 || once < 0) return 0;
        for (const MoveSeq* S : {&seq, &tseq, &aseq}) {
            if (!S->wzx || !S->running || S->anim != once || f < S->first_frame) continue;
            for (size_t i = 0; i < S->start.size(); ++i) {
                OrreWzxBlur bl{};
                if (!orre_wzx_blur(S->wzx, (int)i, &bl) || !bl.screen || bl.count <= 0) continue;
                const int sf = S->first_frame + (S->start[i] + 1) / 2;
                if (f >= sf) a = std::max(a, blur_value(bl, f - sf));
            }
        }
        return std::clamp(a, 0.0f, 1.0f);
    }
    // The user's own colour fade with alpha (fade target 2 with its alpha flag: Teleport fades Espeon out to nothing,
    // 80808080 -> 00000000; GSmodelSetModulationColor. *Corrected 2026-11-21:* the alpha is NOT doubled as the colour
    // is: the modulation stage (gs_material_800DF498.c) runs the colour with TEV scale x2 (HSD_TExpColorOp scale 1)
    // and the alpha with scale x1 (HSD_TExpAlphaOp scale 0), and the default modulation is (7F, 7F, 7F, FF), so alpha
    // FF is whole (verified from the code; Camouflage's FF -> 00 fade showed Espeon see-through at once); that the
    // model then draws blended, likely: the game's Espeon goes see-through, move sweep 2026-11-20): user_alpha < 1 draws
    // the Pokémon with the after-images' blended pipelines through fade_cg, a copy of the camera uniforms with that alpha
    float user_alpha = 1;
    wgpu::Buffer fade_ub;
    wgpu::BindGroup fade_cg;
    struct Ghost { std::vector<wgpu::Buffer> vb; wgpu::Buffer ub; wgpu::BindGroup cg; };
    std::vector<Ghost> ghosts;
    std::vector<std::pair<float, float>> ghost_now;  // this frame's copies: {animation frame, alpha}
    // the after-images due at move frame f with the user's pose at animation frame t: fills ghost_now
    void after_images(int f, double t) {
        ghost_now.clear();
        if (f < 0 || once < 0 || !seq.wzx || !seq.running || seq.anim != once) return;
        for (size_t i = 0; i < seq.start.size(); ++i) {
            OrreWzxBlur bl{};
            if (!orre_wzx_blur(seq.wzx, (int)i, &bl) || bl.screen || bl.count <= 0) continue;
            const int sf = seq.first_frame + (seq.start[i] + 1) / 2;
            if (f < sf) continue;
            int key = 0, c = 0;
            bool live = true;
            for (int k = 0; k <= f - sf && live; ++k) {
                while (c >= bl.key[key].ticks) {
                    c -= bl.key[key].ticks;
                    if (++key >= bl.count) { live = false; break; }
                }
                c += 2;
            }
            if (!live) continue;
            const auto& kk = bl.key[key];
            const float v = kk.from + (kk.ticks > 0 ? (float)c / (float)kk.ticks : 0.0f) * (kk.to - kk.from);
            int n = (int)(30.0f * v + 0.5f);
            if (n <= 0) continue;
            const float rate = 0.5f, now = (float)t;
            float start = now - rate * (float)n;
            if (start < 0) { n = (int)(now / rate); start = 0; }
            if (n <= 0 || start >= now) continue;
            const float step = 1.0f / (float)n;
            for (int j = 1; start + 0.5f * rate < now; ++j, start += rate) {
                const float a = (float)j * step;
                ghost_now.push_back({start, 0.5f * a * a});
            }
        }
    }
    std::vector<wgpu::Texture> ptextures;
    wgpu::Sampler psampler;
    wgpu::BindGroup pwhite;
    wgpu::Buffer pvbuf;
    size_t pvcap = 0;
    std::vector<OrreParticleQuad> quads;
    std::vector<ParticleVertex> pverts;
    ~Impl() {
        stop_pacer();
        orre_particles_free(ps);
        orre_wzx_free(seq.wzx);
        orre_wzx_free(tseq.wzx);
        orre_bcam_free(bcam);
    }

    wgpu::BindGroup particle_group(const wgpu::Texture& t) {
        wgpu::BindGroupEntry e[2];
        e[0].binding = 0;
        e[0].sampler = psampler;
        e[1].binding = 1;
        e[1].textureView = t.CreateView();
        wgpu::BindGroupDescriptor bg;
        bg.layout = player1;
        bg.entryCount = 2;
        bg.entries = e;
        return device.CreateBindGroup(&bg);
    }
    // the particle pipelines: one per blend mode (kind bits 22-23, psDispMain) and depth test (NoZComp 0x10000000);
    // the depth is tested against the model but never written (no TexEdge particles on the Pokémon), no culling
    // solid: the target box's faces through the same shader (opaque, written to the depth buffer so the particles
    // behind it are hidden)
    wgpu::RenderPipeline particle_pipe(uint32_t blend_mode, bool noz, bool solid = false) {
        const uint32_t key = blend_mode | (noz ? 4u : 0u) | (solid ? 8u : 0u);
        auto it = ppipes.find(key);
        if (it != ppipes.end()) return it->second;
        wgpu::VertexAttribute a[7];
        a[0] = {nullptr, wgpu::VertexFormat::Float32x3, offsetof(ParticleVertex, pos), 0};
        a[1] = {nullptr, wgpu::VertexFormat::Float32x2, offsetof(ParticleVertex, uv), 1};
        a[2] = {nullptr, wgpu::VertexFormat::Unorm8x4, offsetof(ParticleVertex, ras), 2};
        a[3] = {nullptr, wgpu::VertexFormat::Unorm8x4, offsetof(ParticleVertex, prim), 3};
        a[4] = {nullptr, wgpu::VertexFormat::Unorm8x4, offsetof(ParticleVertex, env), 4};
        a[5] = {nullptr, wgpu::VertexFormat::Unorm8x4, offsetof(ParticleVertex, mat), 5};
        a[6] = {nullptr, wgpu::VertexFormat::Uint32x2, offsetof(ParticleVertex, kind), 6};
        wgpu::VertexBufferLayout vbl;
        vbl.arrayStride = sizeof(ParticleVertex);
        vbl.attributeCount = 7;
        vbl.attributes = a;
        // GX's modes (psDispMain): 0 SRCALPHA / INVSRCALPHA, 1 SRCALPHA / ONE (additive: most flames), 2 subtract
        // (destination minus source, factors unused), 3 DSTCLR / ZERO (multiply)
        wgpu::BlendState blend;
        blend.color.operation = blend_mode == 2 ? wgpu::BlendOperation::ReverseSubtract : wgpu::BlendOperation::Add;
        blend.color.srcFactor = blend_mode == 2 ? wgpu::BlendFactor::One : blend_mode == 3 ? wgpu::BlendFactor::Dst : wgpu::BlendFactor::SrcAlpha;
        blend.color.dstFactor = blend_mode == 0 ? wgpu::BlendFactor::OneMinusSrcAlpha : blend_mode == 3 ? wgpu::BlendFactor::Zero
                                                                                      : wgpu::BlendFactor::One;
        blend.alpha = blend.color;
        wgpu::ColorTargetState ct;
        ct.format = format;
        ct.blend = solid ? nullptr : &blend;
        wgpu::FragmentState fs;
        fs.module = pshader;
        fs.entryPoint = "pfs";
        fs.targetCount = 1;
        fs.targets = &ct;
        wgpu::DepthStencilState ds;
        ds.format = wgpu::TextureFormat::Depth24Plus;
        ds.depthWriteEnabled = wgpu::OptionalBool::False;  // (the box too: particles always draw over it)
        ds.depthCompare = noz ? wgpu::CompareFunction::Always : wgpu::CompareFunction::LessEqual;
        wgpu::RenderPipelineDescriptor pd;
        pd.layout = pplayout;
        pd.vertex.module = pshader;
        pd.vertex.entryPoint = "pvs";
        pd.vertex.bufferCount = 1;
        pd.vertex.buffers = &vbl;
        pd.fragment = &fs;
        pd.depthStencil = &ds;
        // (the box's back faces culled; particles are two-sided)
        pd.primitive.frontFace = wgpu::FrontFace::CCW;
        pd.primitive.cullMode = solid ? wgpu::CullMode::Back : wgpu::CullMode::None;
        return ppipes[key] = device.CreateRenderPipeline(&pd);
    }
    // a new model: its bank into the simulation (slot 0) and its pictures onto the GPU; the run starts afresh
    // a bank's pictures, keyed by its particle slot
    void bank_textures(const OrreParticleBank* bank, int slot) {
        for (int g = 0; g < orre_particle_group_count(bank); ++g) {
            int n = 0;
            orre_particle_group(bank, g, nullptr, nullptr, nullptr, &n);
            for (int i = 0; i < n && i < 256; ++i)
                if (const OrreImage* im = orre_particle_group_image(bank, g, i)) {
                    ptextures.push_back(make_texture(im->width, im->height, im->rgba));
                    ptex[(uint32_t)slot << 16 | (uint32_t)g << 8 | (uint32_t)i] = particle_group(ptextures.back());
                }
        }
    }
    void particles_model() {
        live.clear();
        keys.clear();
        move_nodes.clear();
        move_banks.clear();
        sim_anim = sim_frame = -1;
        first_keys = true;
        ptex.clear();
        ptextures.clear();
        if (!ps) ps = orre_particles_create();
        orre_particles_reset(ps, 1);
        for (int s = 0; s < 64; ++s) orre_particles_set_bank(ps, s, nullptr);
        orre_particles_set_bank(ps, 0, model.particles);
        jw.assign((size_t)model.joint_count * 12, 0.0f);
        js.assign((size_t)model.joint_count * 3, 0.0f);
        if (model.particles) bank_textures(model.particles, 0);
    }
    // The move's sequence armed for its animation (play_once): every entry's start against this Pokémon's time points
    // for the file's slot, the banks given slots, the particles afresh (wazaSequenceLoadData / wazaSequenceEntryLink)
    // one half's schedule and banks (wazaSequenceLoadData / wazaSequenceEntryLink): the user's against its own time
    // points for the file's slot, the box's against kBoxTimes
    void seq_load(MoveSeq& S, int anim, int& next_bank) {
        OrreWzxHeader h{};
        orre_wzx_header(S.wzx, &h);
        // the slot the file names, falling back as wazaSequenceLoadHeader does on a short slot table
        // the half's owner: the Pokémon, the target Pokémon, or the box (its own time points, size class 0)
        const OrreModel* om = !S.box ? &model : (has_target() ? &target->model : nullptr);
        int slot = h.slot;
        if (om && slot >= om->slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
        S.slot = std::clamp(slot, 0, 16);
        const size_t n = (size_t)h.count;
        S.start.assign(n, 0);
        S.order.assign(n, 0);
        S.state.assign(n, 0);
        S.handle.assign(n, 0);
        S.joint.assign(n, -1);
        orre_wzx_schedule(S.wzx, om ? om->slot_times[S.slot] : kBoxTimes, S.start.data(), S.order.data());
        S.scale = orre_wzx_effect_scale(om ? om->size_class : 0, S.move, S.part);
        for (size_t i = 0; i < n; ++i) {
            const OrreParticleBank* b = orre_wzx_bank(S.wzx, (int)i);
            if (!b || move_banks.count(b) || next_bank >= 64) continue;
            move_banks[b] = next_bank;
            orre_particles_set_bank(ps, next_bank, b);
            bank_textures(b, next_bank);
            ++next_bank;
        }
        S.anim = anim;
        S.running = true;
    }
    // when the user's half ends, so the target's begins (the battle's move functions wait for the attack sequence to
    // end, wazaSequenceSysIsAllEnd(6), before most damage halves): its first "frames" value entry ends it that many
    // ticks after it starts (wazaSequenceEntryUpdate, kind 1 value 0; verified from the code), else the animation's end
    int user_end_frame(int anim) const {
        const int len = std::max(1, (int)std::floor(orre_model_animation_length(&model, anim)));
        const int et = end_tick(seq);
        return et >= 0 ? (et + 1) / 2 : len;
    }
    // a half's end tick: its first "frames" value entry's start plus its value (-1: none; the half then ends when all
    // its entries have)
    static int end_tick(const MoveSeq& S) {
        if (!S.wzx) return -1;
        for (int i : S.order) {
            OrreWzxEntry e{};
            orre_wzx_entry(S.wzx, i, &e);
            if (e.type == 1 && e.value_kind == 0) return S.start[(size_t)i] + e.value;
        }
        return -1;
    }
    // The first frame a half has ended, for its models (INT_MAX: no end entry). The game's sequence update returns -1
    // once its clock passes the end tick (wazaSequenceUpdate: wazaSequenceSysGetWazaTimeById(..., 1) < clock), and
    // wazaSequenceApplyStop then stops every entry (wazaSequenceEntryStop, immediate: a model's animation stopped and
    // the model hidden, GSmodelSetVisibility 0). Verified 2026-11-21 with the game's model log
    // (ORRE_WAZA_SWEEP_MODELLOG) on Hydro Cannon 308: the beam (key 5, a 59-frame animation from tick 140) was hidden at
    // its frame 42, the sequence clock stopping at 222 with the end entry at 220, where Studio drew it on to the end
    // of its animation over the target's half. Held models stay (state 3 is not stopped: Dig's hole). The frame whose
    // tick 2 x (f - first_frame) first exceeds the end tick. ORRE_NO_HALF_END=1 turns it off (a test switch only)
    int half_end_frame(const MoveSeq& S) const {
        static const bool off = std::getenv("ORRE_NO_HALF_END") != nullptr;
        const int et = end_tick(S);
        return off || et < 0 ? INT_MAX : S.first_frame + et / 2 + 1;
    }
    // How long the target's half runs, in battle frames: the game ends a half when every entry has ended
    // (wazaSequenceEntryUpdate), so until its effects are gone (orre_wzx_run_ticks' dry run) and the target's own
    // animation for the file's slot has played (verified against the move sweep's battle-camera logs, 2026-11-19: on
    // 12 moves on Umbreon the half lasted its 298-tick hit animation, less one frame, or longer). Longer in the game
    // when a sound outlasts both (Flamethrower: 326 ticks), which Studio can't time yet. The box: its own hit's frames
    int target_half_frames() const {
        if (!tseq.wzx) return 0;
        OrreWzxHeader h{};
        orre_wzx_header(tseq.wzx, &h);
        const OrreModel* om = has_target() ? &target->model : nullptr;
        int slot = h.slot;
        if (om && slot >= om->slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
        slot = std::clamp(slot, 0, 16);
        int frames = (orre_wzx_run_ticks(tseq.wzx, om ? om->slot_times[slot] : kBoxTimes, 1200) + 1) / 2;
        const int lead = half_lead(tseq);
        if (!om) return std::max(frames, lead + kBoxHitFrames);
        const int an = orre_model_slot_animation(om, slot);
        if (an >= 0) frames = std::max(frames, 1 + lead + (int)std::floor(orre_model_animation_length(om, an)));  // (+1: its held first pose)
        return frames;
    }
    // the user's third part (aseq) loaded to start at move frame `first`; the timeline runs on to its end (its end value
    // entry, else its effects' run, and at least its own slot animation)
    void aseq_load(int anim, int& next, int first) {
        aseq.box = false;
        seq_load(aseq, anim, next);
        aseq.first_frame = first;
        bool has_end = false;
        int end = 0;
        for (int i : aseq.order) {
            OrreWzxEntry e{};
            orre_wzx_entry(aseq.wzx, i, &e);
            if (e.type == 1 && e.value_kind == 0 && !has_end) {
                has_end = true;
                end = (aseq.start[(size_t)i] + e.value + 1) / 2;
            }
        }
        const int run = has_end ? end : (orre_wzx_run_ticks(aseq.wzx, model.slot_times[aseq.slot], 600) + 1) / 2 + half_lead(aseq);
        const int a3 = orre_model_slot_animation(&model, aseq.slot);
        const int alen3 = a3 >= 0 ? (int)std::floor(orre_model_animation_length(&model, a3)) : 0;
        const int len = std::max(1, (int)std::floor(orre_model_animation_length(&model, anim)));
        move_tail = std::max(move_tail, first + std::max(run, half_lead(aseq) + alen3) - len);
        logf(Level::Detail, "move", "move %d part 3 after the earlier half: %zu entries, from frame %d", aseq.move, aseq.start.size(),
             aseq.first_frame);
    }
    void seq_begin(int anim) {
        seq.running = tseq.running = aseq.running = false;
        box_on = false;
        user_hidden = box_hidden = false;
        fx_models.clear();
        move_tail = 0;
        if (!seq.wzx || !seq.armed) return;
        seq.armed = false;
        particles_model();  // (a clean run: the Pokémon's own effects restart with it, seed 1)
        int next = 1;
        seq_load(seq, anim, next);
        seq.first_frame = kUserDelay;
        logf(Level::Detail, "move", "move %d part %d: %zu entries, %d banks, animation %d, slot %d, lead-in %d frames", seq.move,
             seq.part, seq.start.size(), next - 1, anim, seq.slot, half_lead(seq));
        if (tseq.wzx) {
            tseq.box = true;
            seq_load(tseq, anim, next);
            tshot = user_end_frame(anim);
            // *Added 2026-11-21:* the "Att" class (fightWazaWzxTypeFuncAtt, the 4th function of the move's row in
            // waza_func_data, verified from the code) plays part 1 alone on the move's turn; its part 2 comes from
            // another event later (Future Sight's and Doom Desire's hit two turns on). Of its moves, these six have a
            // part 2: Scary Face 184, Sandstorm 201, Sleep Talk 214, Future Sight 248, Hail 258, Doom Desire 353. So the
            // preview plays the target's half once the user's has fully run (its effects' run, not its first value
            // entry), as the game's own move viewer does (the move sweep: Future Sight's hit lands at about frame 110,
            // after part 1's 170-tick fade, where its value entry would have cut at frame 22)
            static const int kAttLater[] = {184, 201, 214, 248, 258, 353};
            if (std::find(std::begin(kAttLater), std::end(kAttLater), seq.move) != std::end(kAttLater)) {
                const int run = (orre_wzx_run_ticks(seq.wzx, model.slot_times[seq.slot], 600) + 1) / 2;
                tshot = std::max(tshot, run);
            }
            tseq.first_frame = tshot + kTargetDelay;
            box_on = true;
            // the timeline runs on until the target's half has ended
            const int len = std::max(1, (int)std::floor(orre_model_animation_length(&model, anim)));
            move_tail = std::max(0, tseq.first_frame + target_half_frames() - len);
            // the user's third part (aseq: the drains' return), when the target's half has ended, as the move sweep plays
            // it (the next owner starts once the last one stops playing; kTargetDelay as the target's half: likely the
            // same first update); the timeline runs on to its end (its end value entry, else its effects' run)
            if (aseq.wzx) {
                // (the target's sequence ends at its first "frames" value entry when it has one, as the user's does:
                // user_end_frame; Mega Drain's part 2 ends 80 ticks in, and the game's part 3 starts then, not after
                // the target's whole hit animation; else when its effects and animation have run)
                // (*updated:* and not before the target's effects have all ended (its particles gone). There is no game
                // timing to copy exactly: a battle plays part 3 in the move class's second call, after the damage and
                // HP steps (fightWazaWzxTypeFuncVampire / AttDefSpc mode 1, verified from the code), and the move sweep
                // plays it as an item of its own after reloading the models (pc_waza_viewer.c layout 1). This choice
                // lands within half a sheet row of the sweep's pictures of Mega Drain)
                int tend = target_half_frames();
                for (int i : tseq.order) {
                    OrreWzxEntry e{};
                    orre_wzx_entry(tseq.wzx, i, &e);
                    if (e.type == 1 && e.value_kind == 0) {
                        tend = (tseq.start[(size_t)i] + e.value + 1) / 2;
                        const OrreModel* om = has_target() ? &target->model : nullptr;
                        const int run = (orre_wzx_run_ticks(tseq.wzx, om ? om->slot_times[tseq.slot] : kBoxTimes, 1200) + 1) / 2;
                        tend = std::max(tend, run);
                        break;
                    }
                }
                aseq_load(anim, next, tseq.first_frame + tend + kTargetDelay);
            }
        } else {
            // *Added 2026-11-20:* with no target half (the self-aimed moves' third parts: Harden), the timeline runs
            // on until the user's own half has ended: at its end value entry when it has one (user_end_frame), else
            // when its effects have (orre_wzx_run_ticks, capped at 5 s: a held fade never ends on its own): the game's
            // Harden shines 195 ticks, past its
            // animation, and the move sweep shows the shine run out
            const int len = std::max(1, (int)std::floor(orre_model_animation_length(&model, anim)));
            bool has_end = false;
            for (int i : seq.order) {
                OrreWzxEntry e{};
                orre_wzx_entry(seq.wzx, i, &e);
                has_end = has_end || (e.type == 1 && e.value_kind == 0);
            }
            const int slot = std::clamp(seq.slot, 0, 16);
            const int run = kUserDelay + (has_end ? user_end_frame(anim)
                                    : (orre_wzx_run_ticks(seq.wzx, model.slot_times[slot], 600) + 1) / 2 + half_lead(seq));
            move_tail = std::max(0, run - len);
            // a third part with no target half before it (the parts picker's "Both" on Substitute, Helping Hand,
            // Wish): when the user's own half has ended
            if (aseq.wzx) aseq_load(anim, next, run + kTargetDelay);
        }
        // (the user's pose reaches its last frame kUserDelay later: the timeline runs that much past the animation)
        move_tail = std::max(move_tail, kUserDelay + half_lead(seq));
        box_frame = -1;
        // the camera starts afresh with every Play (a new seed) and every replay from a step back (the same one)
        cam_frame = -1;
    }
    // _wazaSequenceParticleEntryStart (src/game/wazaSequenceEntry.c; verified from the code): the emitter starts on its
    // bank (link 1 with mode bit 0), takes the mode's generator switches, then is placed by the attach point and follow
    // mode. Studio's Pokémon stands at the origin unturned, so the owner's position and rotation are zero. An entry
    // attached to a model entry (flags 1) follows that model's part (*added 2026-11-20*: fx_joint);
    // battleGridGetNormalisedScale (flags 8, world-placed entries) is taken as 1.
    void seq_particle(MoveSeq& S, int i, int anim, float frame) {
        OrreWzxEntry e{};
        orre_wzx_entry(S.wzx, i, &e);
        const OrreParticleBank* b = orre_wzx_bank(S.wzx, i);
        auto bs = b ? move_banks.find(b) : move_banks.end();
        if (bs == move_banks.end()) { S.state[(size_t)i] = 2; return; }
        const int h = orre_particles_start_link(ps, bs->second, e.emitter, (int)(e.mode & 1));
        if (!h) {
            logf(Level::Detail, "move", "%s entry %d: emitter %d didn't start", S.box ? "target" : "user", i, e.emitter);
            S.state[(size_t)i] = 2;
            return;
        }
        S.state[(size_t)i] = 1;
        S.handle[(size_t)i] = h;
        orre_particles_node_flags(ps, h, e.mode);
        const int apply = (int)((e.mode >> 5) & 1), motion = (int)((e.mode >> 9) & 1);
        const int sel = e.follow >= 1 && e.follow <= 7 ? e.follow : 0;
        const float sc[3] = {S.scale, S.scale, S.scale};
        const float zero[3] = {0, 0, 0};
        // the owner's place in the battle's space: GSmodelGetPosition / GSmodelGetRotation of the user, or the box's
        float own_pos[3] = {S.box ? kTargetX : kUserX, 0, 0}, own_rot[3] = {0, S.box ? -kTurn : kTurn, 0};
        // linked to an earlier model entry (entry flags 1 with a link key; wazaSequenceEntryGetAttachPart, verified from
        // the code): its part +0x64 of that model is the attach point, and that model's place and turn stand in for the
        // owner's (as _wazaSequenceModelEntryStart's sourceModel; likely the same for particles): Swords Dance's sparkles
        // on the swords
        int fx = -1;
        if ((e.flags & 1) && e.link_key > 0)
            for (size_t m = 0; m < fx_models.size(); ++m)
                if (fx_models[m].key == e.link_key && fx_models[m].box == S.box) fx = (int)m;
        if (fx >= 0) {
            float sc3[3];
            fx_place(fx_models[(size_t)fx], (int)frame, own_pos, own_rot, sc3);
        }
        float off[3];
        orre_particles_node_position(ps, h, off);
        if (e.flags & 4) {  // placed in the world, unattached
            const float one[3] = {1, 1, 1};
            orre_particles_node_set(ps, h, 0, zero, 0, 0);
            orre_particles_node_set(ps, h, 1, zero, 0, 0);
            orre_particles_node_set(ps, h, 2, one, apply, motion);
            return;
        }
        // the attach point: the slot's joint for points 0-15, the bounding box's centre for 16 (likely: the game adds a
        // centre null joint there)
        int joint = -1;
        if (fx >= 0) {
            if (e.part >= 0 && e.part < fx_models[(size_t)fx].inst->model.joint_count) joint = e.part;
        } else if (S.box && has_target()) {
            // the target Pokémon: its own slot's attach points, as the user's (the bounding box's centre for 16)
            if (e.attach == 16) joint = -2;
            else if (e.attach >= 0 && e.attach < 16) {
                const int j = target->model.slot_points[S.slot][e.attach];
                if (j >= 0 && j < target->model.joint_count) joint = j;
            }
        } else if (S.box) {
            // the box's three places: the ground for point 0, the upper front for the mouth and eyes, else the centre
            joint = e.attach == 0 ? kBoxGround : (e.attach == 1 || e.attach == 4 || e.attach == 5 ? kBoxFront : kBoxCentre);
        } else if (e.attach == 16) {
            joint = -2;
        } else if (e.attach >= 0 && e.attach < 16) {
            const int j = model.slot_points[S.slot][e.attach];
            if (j >= 0 && j < model.joint_count) joint = j;
        }
        if (joint == -1) return;  // no part: the generator stays where it was created (the game: GSmodelGetPart NULL)
        if (sel != 0) {
            orre_particles_node_link(ps, h, sel, (int)((e.flags >> 1) & 1), 1, (int)((e.mode >> 1) & 1));
            float r[3] = {0, 0, 0}, ps3[3] = {1, 1, 1};
            if (fx >= 0) {
                float tp[3], tr[3];
                fx_joint(fx_models[(size_t)fx], joint, (int)frame, tp, tr, ps3);
            } else if (joint >= 0 && S.box && has_target()) {
                float tp[3], tr[3];
                target_joint(joint, (int)frame, tp, tr, ps3);
            } else if (joint >= 0) {
                orre_model_part_chain(&model, anim, frame, joint, r, ps3);
            }
            const float temp[3] = {ps3[0] * sc[0], ps3[1] * sc[1], ps3[2] * sc[2]};
            const bool by_temp = sel == 7 || sel == 5 || sel == 6 || sel == 3;
            for (int k = 0; k < 3; ++k) off[k] *= (by_temp ? temp[k] : sc[k]) - 1.0f;
            if (sel == 5 || sel == 2 || sel == 3)
                for (int k = 0; k < 3; ++k) off[k] += own_pos[k];  // (the game's own quirk: an offset plus the place)
            orre_particles_node_set(ps, h, 0, off, 0, 0);
            if (sel == 4 || sel == 1 || sel == 2) orre_particles_node_set(ps, h, 2, sc, apply, motion);
            if (sel == 1 || sel == 6 || sel == 3) orre_particles_node_set(ps, h, 1, own_rot, 0, 0);
        } else {
            // not following: placed once at the owner's place (its feet), turned as the owner
            for (int k = 0; k < 3; ++k) off[k] = off[k] * (sc[k] - 1.0f) + own_pos[k];
            orre_particles_node_set(ps, h, 0, off, 0, 0);
            orre_particles_node_set(ps, h, 1, own_rot, 0, 0);
            orre_particles_node_set(ps, h, 2, sc, apply, motion);
        }
        S.joint[(size_t)i] = joint;
        move_nodes.push_back({h, joint, S.box, fx});
    }
    // a half's entries due at battle frame f of the move (its tick: 2 per frame from the half's first frame): the
    // particle entries start; the other kinds wait for the next steps of release step 4
    void seq_frame(MoveSeq& S, int anim, int f) {
        if (!S.running || anim != S.anim || !S.wzx || f < S.first_frame) return;
        const int tick = 2 * (f - S.first_frame);
        for (int i : S.order) {
            if (S.state[(size_t)i] != 0 || S.start[(size_t)i] > tick) continue;
            OrreWzxEntry e{};
            orre_wzx_entry(S.wzx, i, &e);
            if (e.type == 3) {
                seq_particle(S, i, anim, (float)f);
            } else if (e.type == 2) {
                seq_model(S, i, f);
            } else if (e.type == 6) {
                // control steps (wazaSequenceEntryStart kind 6; verified from the code): 2 hides the owner, 3 shows it
                // (wazaSequenceOwnerSetVisibility). The others (the aura 0 / 1, the null joint 4-6, the stop 7 / 9,
                // releasing the owner 8) change nothing Studio draws yet
                if (e.control == 2 || e.control == 3) (S.box ? box_hidden : user_hidden) = e.control == 2;
                S.state[(size_t)i] = 1;
            } else {
                S.state[(size_t)i] = 2;
            }
        }
    }
    // gsParticleUpdate's follow for the move's nodes: each due node takes its joint's world position and its chain's
    // rotation and scale at the frame shown (pf: the user's pose frame; f: the move's frame, for the box)
    void seq_follow(int anim, int pf, int f, bool have_world) {
        const BoxPose bp = box_pose(f);
        for (size_t k = 0; k < move_nodes.size();) {
            const MoveNode& n = move_nodes[k];
            if (!orre_particles_node_alive(ps, n.handle)) {
                move_nodes.erase(move_nodes.begin() + (std::ptrdiff_t)k);
                continue;
            }
            if (orre_particles_node_due(ps, n.handle)) {
                float sp[3], p[3], r[3] = {0, 0, 0}, s[3] = {1, 1, 1};
                if (n.fx >= 0 && n.fx < (int)fx_models.size()) {
                    // a move model's joint (linked entries: fx_joint)
                    if (fx_joint(fx_models[(size_t)n.fx], n.joint, f, p, r, s)) {
                        orre_particles_node_follow(ps, n.handle, p, r, s);
                        // ORRE_DEBUG_FXNODE=1 (a debug switch, 2026-11-21): each linked emitter's follow, to compare
                        // with the game's particle log ("gen ... at"): the move frame, node, part and battle place
                        static const bool dbg = std::getenv("ORRE_DEBUG_FXNODE") != nullptr;
                        if (dbg)
                            logf(Level::Info, "fxnode", "frame %d node %d part %d at %.3f,%.3f,%.3f rot %.3f,%.3f,%.3f scale %.3f",
                                 f, n.handle, n.joint, p[0], p[1], p[2], r[0], r[1], r[2], s[0]);
                    }
                    ++k;
                    continue;
                }
                if (n.box && has_target()) {
                    // the target Pokémon's joint (its bounding box's centre for -2)
                    bool ok = true;
                    if (n.joint == -2) {
                        const OrreModel& tm = target->model;
                        const float c[3] = {(tm.bounds_min[0] + tm.bounds_max[0]) * 0.5f, (tm.bounds_min[1] + tm.bounds_max[1]) * 0.5f,
                                            (tm.bounds_min[2] + tm.bounds_max[2]) * 0.5f};
                        p[0] = kTargetX - c[2]; p[1] = c[1]; p[2] = c[0];
                        r[1] = -kTurn;
                    } else {
                        ok = target_joint(n.joint, f, p, r, s);
                    }
                    if (ok) orre_particles_node_follow(ps, n.handle, p, r, s);
                    ++k;
                    continue;
                }
                if (n.box) {
                    box_joint(n.joint, bp, p, r);
                    orre_particles_node_follow(ps, n.handle, p, r, s);
                    ++k;
                    continue;
                }
                if (n.joint == -2) {
                    for (int c = 0; c < 3; ++c) sp[c] = (model.bounds_min[c] + model.bounds_max[c]) * 0.5f;
                } else if (have_world) {
                    sp[0] = jw[(size_t)n.joint * 12 + 3];
                    sp[1] = jw[(size_t)n.joint * 12 + 7];
                    sp[2] = jw[(size_t)n.joint * 12 + 11];
                    orre_model_part_chain(&model, anim, (float)pf, n.joint, r, s);
                } else {
                    ++k;
                    continue;
                }
                to_battle(sp, p);
                r[1] += kTurn;  // (the chain reaches the model's root, which the battle turns +90 degrees)
                orre_particles_node_follow(ps, n.handle, p, r, s);
            }
            ++k;
        }
    }
    // one battle frame of animation `anim` at whole frame f
    // One half of a battle frame's particle work (two steps per 30 fps frame): the first half starts the frame's keys,
    // places the live emitters on their joints and steps once; the second half steps once more. At the game's rate
    // both halves run together on each whole frame, exactly as before; with smoothing on (a preview rate above the
    // game's) each runs at its own time, so the particles move 60 times a second (2026-11-19, release step 3)
    void particles_half(int anim, int f, int half) {
        if (half == 0) particles_frame(anim, f, false);
        else orre_particles_step(ps);
    }
    void particles_frame(int anim, int f, bool both = true) {
        auto it = keys.find(anim);
        if (it == keys.end()) {
            std::vector<OrreParticleKey> k(1024);
            const int n = std::min(1024, orre_model_particle_keys(&model, anim, k.data(), 1024));
            k.resize((size_t)std::max(0, n));
            it = keys.emplace(anim, std::move(k)).first;
        }
        // The first frame after a model is loaded fires its frame-0 keys twice: the game's log shows 8 starts there
        // instead of 4 (the project's ORRE_WAZA_VIEWER_PTCLHOOK, Moltres and Articuno, verified 2026-11-19), likely because
        // GSmodelLinkToGSparticleBank re-applies the current frame after the animation already fired it
        // (models-animation.md "Code that loads and plays them")
        const int times = first_keys && f == 0 ? 2 : 1;
        first_keys = false;
        if (particles_on && model.particles)
            for (int t = 0; t < times; ++t)
                for (const OrreParticleKey& k : it->second)
                    if (k.frame == f) {
                        const int h = orre_particles_start(ps, 0, k.emitter);
                        if (h) live.push_back({h, k.joint});
                    }
        // the move's entries due this frame, the user's half then the target's (the battle runs its sequences before the
        // particles' update)
        seq_frame(seq, anim, f);
        seq_frame(tseq, anim, f);
        seq_frame(aseq, anim, f);  // (the user's third part, after the target's half)
        orre_particles_reclaim(ps);
        // past the animation's end (the timeline's extra frames) the user holds its last pose
        int ua, pf;
        user_anim(anim, f, ua, pf);  // (the third part's own animation once it runs)
        const bool have = (!live.empty() || !move_nodes.empty()) &&
                          orre_model_joint_world(&model, ua, (float)pf, jw.data(), js.data());
        if (have) {
            for (const LiveGen& l : live) {
                if (l.joint < 0 || l.joint >= model.joint_count) continue;
                const float s[3] = {jw[(size_t)l.joint * 12 + 3], jw[(size_t)l.joint * 12 + 7], jw[(size_t)l.joint * 12 + 11]};
                float p[3];
                to_battle(s, p);
                orre_particles_set_position(ps, l.handle, p);
            }
        }
        seq_follow(ua, pf, f, have);
        orre_particles_step(ps);
        if (both) orre_particles_step(ps);
    }
    // brings the simulation to (anim, frame): each whole frame passed since the last call, in order (a late timer
    // or a loop's wrap included, at most 30 at once); a different animation carries on from its frame. back: a held
    // move stepped backward (particles don't run backward: they stay as they are)
    // half: the position in half frames (whole frame x 2, + 1 once its second half has run). The game's rate passes
    // frame x 2 + 1 (both halves of each whole frame); smoothing passes floor(frame x 2)
    void particles_to(int anim, int half, bool back) {
        if (!ps) return;
        const bool own = particles_on && model.particles;
        if (!own && !seq.running && move_nodes.empty()) return;
        // (a move with its effects: the whole timeline, its extra frames for the target's half included)
        const int alen = std::max(1, (int)std::floor(orre_model_animation_length(&model, anim)));
        const int len2 = 2 * (seq.running && anim == seq.anim ? alen + std::max(move_tail, half_lead(seq)) : alen);
        int steps;
        if (anim != sim_anim) {
            // a new animation: from its frame (warm: screenshot tests start this many frames earlier, round the loop,
            // so a held frame shows the effect running). A move with its effects runs from its first frame instead, so
            // a held frame shows everything its entries started up to there
            sim_anim = anim;
            steps = seq.running && anim == seq.anim ? std::min(half + 1, len2) : 2 * warm + 2;
        } else {
            if (half == sim_frame) return;
            if (back && half < sim_frame) {
                // stepped or dragged backward: a move with its effects runs again from its first frame to there (the
                // particles can't run backward, and the timeline should show the move's state at that frame);
                // otherwise the particles stay as they are
                if (seq.wzx && seq.running && anim == seq.anim) {
                    seq.armed = true;
                    seq_begin(anim);
                    sim_anim = anim;
                    steps = std::min(half + 1, len2);
                } else {
                    sim_frame = half;
                    return;
                }
            } else {
                // forward; a move with effects never wraps round (its entries run once), so it goes straight there
                steps = seq.running && anim == seq.anim ? std::max(0, half - sim_frame)
                                                        : std::min(60, ((half - sim_frame) % len2 + len2) % len2);
            }
        }
        int h = (((half - steps) % len2) + len2) % len2;
        for (int n = 0; n < steps; ++n) {
            h = h + 1 >= len2 ? 0 : h + 1;
            particles_half(anim, h / 2, h % 2);
        }
        sim_frame = half;
    }

    wgpu::Texture make_texture(int tw, int th, const uint8_t* rgba) {
        wgpu::TextureDescriptor td;
        td.size = {(uint32_t)tw, (uint32_t)th, 1};
        td.format = wgpu::TextureFormat::RGBA8Unorm;
        td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        wgpu::Texture t = device.CreateTexture(&td);
        wgpu::TexelCopyTextureInfo dst;
        dst.texture = t;
        wgpu::TexelCopyBufferLayout layout;
        layout.bytesPerRow = (uint32_t)tw * 4;
        layout.rowsPerImage = (uint32_t)th;
        wgpu::Extent3D ext{(uint32_t)tw, (uint32_t)th, 1};
        queue.WriteTexture(&dst, rgba, (size_t)tw * th * 4, &layout, &ext);
        return t;
    }

    // GXTexWrapMode: 0 clamp, 1 repeat, 2 mirror
    wgpu::Sampler sampler(int ws, int wt) {
        const int key = ws * 3 + wt;
        auto it = samplers.find(key);
        if (it != samplers.end()) return it->second;
        auto mode = [](int wm) {
            return wm == 1 ? wgpu::AddressMode::Repeat : wm == 2 ? wgpu::AddressMode::MirrorRepeat : wgpu::AddressMode::ClampToEdge;
        };
        wgpu::SamplerDescriptor sd;
        sd.addressModeU = mode(ws);
        sd.addressModeV = mode(wt);
        sd.magFilter = sd.minFilter = wgpu::FilterMode::Linear;
        return samplers[key] = device.CreateSampler(&sd);
    }

    // Texture animations: each part's pictures and blend factors at an animation frame; a part's material group is
    // rebuilt only when one of them changed
    void apply_textures(int anim, float frame) {
        const size_t n = (size_t)model.mesh_count * ORRE_MAX_STAGES;
        ftex.assign(n, -1);
        fblend.assign(n, 0);
        fxf.assign(n * 4, 0);
        if (!orre_model_texture_frame2(&model, anim, frame, ftex.data(), fblend.data(), fxf.data())) return;
        fmat.assign((size_t)model.mesh_count * ORRE_MATERIAL_FLOATS, 0);
        if (!orre_model_material_frame(&model, anim, frame, fmat.data())) return;
        for (auto& p : parts) {
            const int* t = &ftex[(size_t)p.mesh * ORRE_MAX_STAGES];
            const float* b = &fblend[(size_t)p.mesh * ORRE_MAX_STAGES];
            const float* x = &fxf[(size_t)p.mesh * ORRE_MAX_STAGES * 4];
            const float* c = &fmat[(size_t)p.mesh * ORRE_MATERIAL_FLOATS];
            if (std::equal(t, t + ORRE_MAX_STAGES, p.tex) && std::equal(b, b + ORRE_MAX_STAGES, p.blend) &&
                std::equal(x, x + ORRE_MAX_STAGES * 4, p.xf) && std::equal(c, c + ORRE_MATERIAL_FLOATS, p.mat))
                continue;
            std::copy(t, t + ORRE_MAX_STAGES, p.tex);
            std::copy(b, b + ORRE_MAX_STAGES, p.blend);
            std::copy(x, x + ORRE_MAX_STAGES * 4, p.xf);
            std::copy(c, c + ORRE_MATERIAL_FLOATS, p.mat);
            p.group = material_group(&model.meshes[p.mesh], p.mbuf, p.tex, p.blend, p.xf, p.mat);
        }
    }

    // the Pokémon's parts with the shine's extra stage (env_tex as one more picture, after the material's own stages;
    // a material already at the stage limit keeps its own look), for the current value and animation state
    void env_build() {
        std::vector<wgpu::Texture> texs = textures;
        const int ei = (int)texs.size();
        texs.push_back(env_tex);
        env_groups.assign(parts.size(), nullptr);
        env_bufs.assign(parts.size(), nullptr);
        const uint32_t coord = env_em.mode == 0 ? 1u : env_em.mode == 1 ? 5u : 6u;  // reflection, gradation, backlight
        for (size_t i = 0; i < parts.size(); ++i) {
            Part& p = parts[i];
            OrreMesh m = model.meshes[p.mesh];
            const int n = m.stage_count;
            if (n >= ORRE_MAX_STAGES) { env_groups[i] = p.group; continue; }
            OrreTexStage& st = m.stages[n];
            st = OrreTexStage{};
            st.texture = ei;
            st.flags = 0x80u | coord | (env_em.type == 0 ? 0x40000u : 0x30000u);
            st.blending = env_value;
            st.su = st.sv = 1;
            m.stage_count = n + 1;
            if (p.mat[0] < 0) {  // (no texture animation applied yet: the material's own values)
                env_groups[i] = material_group(&m, env_bufs[i], nullptr, nullptr, nullptr, nullptr, &model, &texs);
            } else {
                int tx[ORRE_MAX_STAGES];
                float bl[ORRE_MAX_STAGES], xf[ORRE_MAX_STAGES * 4];
                std::copy(p.tex, p.tex + ORRE_MAX_STAGES, tx);
                std::copy(p.blend, p.blend + ORRE_MAX_STAGES, bl);
                std::copy(p.xf, p.xf + ORRE_MAX_STAGES * 4, xf);
                tx[n] = ei;
                bl[n] = env_value;
                xf[4 * n] = xf[4 * n + 1] = 1;
                xf[4 * n + 2] = xf[4 * n + 3] = 0;
                env_groups[i] = material_group(&m, env_bufs[i], tx, bl, xf, p.mat, &model, &texs);
            }
        }
        env_built = env_value;
    }

    // The material uniform (struct M in the shader) and its bind group: the stages' samplers and textures
    // (tex / blend: a texture animation's current pictures and factors; mat: the colours at the frame; null = the
    // material's own)
    // (mdl / texs: another model's, for the extra models a move shows and the target Pokémon; default the Pokémon's)
    wgpu::BindGroup material_group(const OrreMesh* mesh, wgpu::Buffer& buf, const int* tex = nullptr, const float* blend = nullptr,
                                   const float* xf = nullptr, const float* mat = nullptr, const OrreModel* mdl = nullptr,
                                   const std::vector<wgpu::Texture>* texs = nullptr) {
        const OrreModel& model = mdl ? *mdl : this->model;
        const std::vector<wgpu::Texture>& textures = texs ? *texs : this->textures;
        float u[kUniformFloats] = {};
        int stages = 0;
        if (mesh) {
            for (int k = 0; k < 3; ++k) {
                u[k] = mat ? mat[k] : mesh->diffuse[k] / 255.0f;
                u[4 + k] = mat ? mat[4 + k] : mesh->ambient[k] / 255.0f;
            }
            u[3] = mat ? mat[3] : std::clamp(mesh->alpha, 0.0f, 1.0f);
            u[7] = 1;
            u[8] = (float)(mesh->render_mode & 3);             // RENDER_DIFFUSE_BITS
            u[9] = (float)(mesh->render_mode >> 13 & 3);       // RENDER_ALPHA_BITS
            u[10] = (mesh->render_mode & 4) ? 1.0f : 0.0f;     // RENDER_DIFFUSE: lit
            stages = mesh->stage_count;
            u[11] = (float)stages;
            const DrawState ds = draw_state(*mesh);
            u[12] = (float)ds.a_comp0; u[13] = ds.a_ref0 / 255.0f; u[14] = (float)ds.a_op; u[15] = (float)ds.a_comp1;
            u[16] = ds.a_ref1 / 255.0f;
            // the shiny recolour, on every material of a model read as shiny (the game walks all the model's own
            // materials, GSmodelAcquireMaterials; its particles keep their colours)
            for (int k = 0; k < 4; ++k) {
                u[kShSw + k] = model.shiny_swap_on ? (float)model.shiny_swap[k] : (float)k;
                u[kShMod + k] = model.shiny_mod_on ? model.shiny_mod[k] / 255.0f : 0.5f;
            }
            // The swap is lost on a material with an emboss bump stage, as in the game (2026-11-19, the all-models
            // check's shinies: Lunatone's shiny looks exactly like its normal one in the game): GX has only four swap
            // tables, shared by all stages, each stage rewriting table 0 when it is set up (HSD_SetupTevStage,
            // src/hsd/tev.c), and the bump stages (SetupEmbossBumpTev, src/hsd/tobj.c), set up after the material's own,
            // write it back to "no swap". Verified from the code; the setup order, likely
            bool bump = false;
            for (int s = 0; s < mesh->stage_count; ++s) bump = bump || ((mesh->stages[s].flags >> 24) & 1);
            u[kShOn] = model.shiny_swap_on && !bump ? 1.0f : 0.0f;
            u[kShOn + 1] = model.shiny_mod_on ? 1.0f : 0.0f;
            if ((mesh->render_mode & 8) && mesh->shininess > 0) {  // RENDER_SPECULAR
                for (int k = 0; k < 3; ++k) u[kSpec + k] = mesh->specular[k] / 255.0f;
                u[kSpec + 3] = mesh->shininess;
            }
            for (int s = 0; s < stages; ++s) {
                const OrreTexStage& st = mesh->stages[s];
                float* x = u + kXF + 4 * s;
                if (xf) { x[0] = xf[4 * s]; x[1] = xf[4 * s + 1]; x[2] = xf[4 * s + 2]; x[3] = xf[4 * s + 3]; }
                else { x[0] = st.su; x[1] = st.sv; x[2] = st.tu; x[3] = st.tv; }
                u[kOP + 4 * s] = (float)(st.flags & 15);
                u[kOP + 1 + 4 * s] = (float)(st.flags >> 4 & 0x1F);
                u[kOP + 2 + 4 * s] = (float)(st.flags >> 16 & 15);
                u[kOP + 3 + 4 * s] = (float)(st.flags >> 20 & 15);
                // the TEV takes the factor as an 8-bit constant: 255 x value, clamped to 0..255 (src/hsd/texp.c), so
                // Mankey's 5.0 acts as 1.0 (verified from the code, 2026-11-18)
                const float k = blend ? blend[s] : st.blending;
                u[kK + 4 * s] = (float)std::clamp((int)(255.0f * k), 0, 255) / 255.0f;
                u[kK + 1 + 4 * s] = (float)std::max(st.uv_set, 0);
                // the TEV override: inputs, settings and colours, 4 floats per stage in each table
                if (st.has_tev) {
                    const uint8_t* t = st.tev;
                    const uint32_t active = (uint32_t)t[0x1C] << 24 | (uint32_t)t[0x1D] << 16 | (uint32_t)t[0x1E] << 8 | t[0x1F];
                    for (int q = 0; q < 4; ++q) { u[kTC + 4 * s + q] = t[8 + q]; u[kTA + 4 * s + q] = t[0x0C + q]; }
                    u[kTP + 4 * s] = t[0]; u[kTP + 1 + 4 * s] = t[2]; u[kTP + 2 + 4 * s] = t[4];
                    u[kTP + 3 + 4 * s] = (float)(((active >> 30) & 1) | ((active >> 31) & 1) << 1 | (t[6] ? 4 : 0) | (t[7] ? 8 : 0));
                    u[kTQ + 4 * s] = t[1]; u[kTQ + 1 + 4 * s] = t[3]; u[kTQ + 2 + 4 * s] = t[5];
                    // konst / tev0 / tev1: the frame's values (texture animations can drive them) or the stored ones
                    for (int q = 0; q < 4; ++q) {
                        u[kKC + 4 * s + q] = mat ? mat[8 + 12 * s + q] : t[0x10 + q] / 255.0f;
                        u[kR0 + 4 * s + q] = mat ? mat[8 + 12 * s + 4 + q] : t[0x14 + q] / 255.0f;
                        u[kR1 + 4 * s + q] = mat ? mat[8 + 12 * s + 8 + q] : t[0x18 + q] / 255.0f;
                    }
                }
            }
        } else {
            for (int k = 0; k < 4; ++k) u[k] = 1;
            u[12] = u[15] = 7;  // no alpha test
        }
        wgpu::BufferDescriptor mb;
        mb.size = sizeof u;
        mb.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        buf = device.CreateBuffer(&mb);
        queue.WriteBuffer(buf, 0, u, sizeof u);
        constexpr int S = ORRE_MAX_STAGES;
        wgpu::BindGroupEntry e[1 + 2 * S];
        e[0].binding = 0;
        e[0].buffer = buf;
        e[0].size = sizeof u;
        for (int s = 0; s < S; ++s) {
            const int t = mesh && s < stages ? (tex && tex[s] >= 0 ? tex[s] : mesh->stages[s].texture) : -1;
            const bool used = t >= 0 && t < (int)textures.size();
            e[1 + s].binding = 1 + s;
            e[1 + s].sampler = used ? sampler(mesh->stages[s].wrap_s, mesh->stages[s].wrap_t) : sampler(0, 0);
            e[1 + S + s].binding = 1 + S + s;
            e[1 + S + s].textureView = (used ? textures[t] : white).CreateView();
        }
        wgpu::BindGroupDescriptor bg;
        bg.layout = layout1;
        bg.entryCount = 1 + 2 * S;
        bg.entries = e;
        return device.CreateBindGroup(&bg);
    }

    wgpu::RenderPipeline pipeline(const DrawState& s) {
        const uint32_t key = s.key();
        auto it = pipelines.find(key);
        if (it != pipelines.end()) return it->second;
        // OrreVertex: position, normal, uv, colour, uv1, uv2, uv3, uv4 (80 bytes)
        wgpu::VertexAttribute attrs[8];
        attrs[0] = {nullptr, wgpu::VertexFormat::Float32x3, offsetof(OrreVertex, pos), 0};
        attrs[1] = {nullptr, wgpu::VertexFormat::Float32x3, offsetof(OrreVertex, nrm), 1};
        attrs[2] = {nullptr, wgpu::VertexFormat::Float32x2, offsetof(OrreVertex, uv), 2};
        attrs[3] = {nullptr, wgpu::VertexFormat::Float32x4, offsetof(OrreVertex, clr), 3};
        attrs[4] = {nullptr, wgpu::VertexFormat::Float32x2, offsetof(OrreVertex, uv1), 4};
        attrs[5] = {nullptr, wgpu::VertexFormat::Float32x2, offsetof(OrreVertex, uv2), 5};
        attrs[6] = {nullptr, wgpu::VertexFormat::Float32x2, offsetof(OrreVertex, uv3), 6};
        attrs[7] = {nullptr, wgpu::VertexFormat::Float32x2, offsetof(OrreVertex, uv4), 7};
        wgpu::VertexBufferLayout vbl;
        vbl.arrayStride = sizeof(OrreVertex);
        vbl.attributeCount = 8;
        vbl.attributes = attrs;
        wgpu::BlendState blend;
        wgpu::ColorTargetState ct;
        ct.format = format;
        if (s.blend_type == 1 || s.blend_type == 3) {
            blend.color.operation = s.blend_type == 3 ? wgpu::BlendOperation::ReverseSubtract : wgpu::BlendOperation::Add;
            blend.color.srcFactor = s.blend_type == 3 ? wgpu::BlendFactor::One : gx_factor(s.src, true);
            blend.color.dstFactor = s.blend_type == 3 ? wgpu::BlendFactor::One : gx_factor(s.dst, false);
            blend.alpha = blend.color;
            ct.blend = &blend;
        }
        wgpu::FragmentState fs;
        fs.module = shader;
        fs.entryPoint = "fs";
        fs.targetCount = 1;
        fs.targets = &ct;
        wgpu::DepthStencilState ds;
        ds.format = wgpu::TextureFormat::Depth24Plus;
        ds.depthWriteEnabled = s.z_write ? wgpu::OptionalBool::True : wgpu::OptionalBool::False;
        ds.depthCompare = s.z_test ? gx_compare(s.z_comp) : wgpu::CompareFunction::Always;
        wgpu::RenderPipelineDescriptor pd;
        pd.layout = playout;
        pd.vertex.module = shader;
        pd.vertex.entryPoint = "vs";
        pd.vertex.bufferCount = 1;
        pd.vertex.buffers = &vbl;
        pd.fragment = &fs;
        pd.depthStencil = &ds;
        // Front faces are the clockwise-wound ones: with the meshes triangulated in the display lists' order
        // (hsd_model.cpp), front = CW plus the game's own "cull back" shows the models' outsides (verified by eye on
        // Bulbasaur and Venusaur, 2026-11-18). Before culling was applied, Venusaur showed an "extra eye" above its
        // real one: the inside of its head seen through the front, not an animation part as first guessed.
        pd.primitive.frontFace = wgpu::FrontFace::CW;
        pd.primitive.cullMode = s.cull == 1 ? wgpu::CullMode::Front : s.cull == 2 ? wgpu::CullMode::Back : wgpu::CullMode::None;
        return pipelines[key] = device.CreateRenderPipeline(&pd);
    }

    void configure(int nw, int nh) {
        w = std::max(1, nw);
        h = std::max(1, nh);
        if (page) {
            // page mode: a texture of our own at the view's size (copied into the page's shared texture each frame)
            wgpu::TextureDescriptor od;
            od.size = {(uint32_t)w, (uint32_t)h, 1};
            od.format = format;
            od.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding;
            offscreen = device.CreateTexture(&od);
            can_copy = true;
            wgpu::TextureDescriptor pd;
            pd.size = {(uint32_t)w, (uint32_t)h, 1};
            pd.format = wgpu::TextureFormat::Depth24Plus;
            pd.usage = wgpu::TextureUsage::RenderAttachment;
            depth = device.CreateTexture(&pd);
            prev = nullptr;
            prev_frame = -1;
            return;
        }
        wgpu::SurfaceConfiguration cfg;
        cfg.device = device;
        cfg.format = format;
        cfg.usage = wgpu::TextureUsage::RenderAttachment;
        // (the screen blur keeps each finished frame: the surface must allow copying out of it, where the driver lets
        // it; without, the blur isn't drawn)
        wgpu::SurfaceCapabilities caps;
        can_copy = adapter && surface.GetCapabilities(adapter, &caps) == wgpu::Status::Success &&
                   (caps.usages & wgpu::TextureUsage::CopySrc) != wgpu::TextureUsage::None;
        if (can_copy) cfg.usage = cfg.usage | wgpu::TextureUsage::CopySrc;
        cfg.width = (uint32_t)w;
        cfg.height = (uint32_t)h;
        cfg.presentMode = wgpu::PresentMode::Fifo;
        cfg.alphaMode = wgpu::CompositeAlphaMode::Opaque;
        surface.Configure(&cfg);
        wgpu::TextureDescriptor dd;
        dd.size = {(uint32_t)w, (uint32_t)h, 1};
        dd.format = wgpu::TextureFormat::Depth24Plus;
        dd.usage = wgpu::TextureUsage::RenderAttachment;
        depth = device.CreateTexture(&dd);
        prev = nullptr;  // (made at the new size when first needed, render)
        prev_frame = -1;
    }
};

void Viewer::take(Viewer& o) {
    impl_ = o.impl_;
    hwnd_ = o.hwnd_;
    ok_ = o.ok_;
    spin_ = o.spin_;
    idle_ = o.idle_;
    hold_ = o.hold_;
    hold_walk_ = o.hold_walk_;
    error_ = std::move(o.error_);
    o.impl_ = nullptr;
    o.hwnd_ = nullptr;
    o.ok_ = false;
    if (impl_) impl_->owner = this;
}

void Viewer::destroy() {
    // the child window first: its procedure reaches the Impl through the window's data, so it must be gone before the
    // Impl is (DestroyWindow sends its last messages now, while the Impl is still there)
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    delete impl_;  // (~Impl stops the pacer and frees the particles, move files and camera; the GPU objects release
                   // themselves as their handles go)
    impl_ = nullptr;
    ok_ = false;
}

bool Viewer::create(HWND parent, HINSTANCE inst) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_DBLCLKS;
        // the child window keeps its Impl (which never moves) and reaches the Viewer that owns it now
        wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
            if (m == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            auto* im = reinterpret_cast<Impl*>(GetWindowLongPtrW(h, GWLP_USERDATA));
            Viewer* v = im ? im->owner : nullptr;
            return v && v->hwnd_ ? v->proc(m, w, l) : DefWindowProcW(h, m, w, l);
        };
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.lpszClassName = kClass;
        RegisterClassW(&wc);
        registered = true;
    }
    impl_ = new Impl();
    impl_->owner = this;
    hwnd_ = CreateWindowExW(0, kClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 16, 16, parent, nullptr, inst, impl_);
    if (!hwnd_) { error_ = "the 3D view's window couldn't be created"; return false; }
    Impl& I = *impl_;
    // born cut to nothing: nothing of an undrawn surface shows before the first frame (the page reveals it then)
    SetWindowRgn(hwnd_, CreateRectRgn(0, 0, 0, 0), FALSE);
    I.cut = true;
    // The WebGPU instance, adapter, device and queue are shared by every 3D view in every window (multi-window plan
    // step 1d): made by the first view, reused by the rest, so a second window costs a surface and its own buffers,
    // not a second device. Each view keeps its own surface (its child window), shaders, pipelines and model data.
    // Windows here (SurfaceSourceWindowsHWND); Linux / macOS: the same sharing with their own surface sources.
    static struct Shared { wgpu::Instance instance; wgpu::Adapter adapter; wgpu::Device device; wgpu::Queue queue; } gpu;
    if (!gpu.instance) {
        const wgpu::InstanceFeatureName features[] = {wgpu::InstanceFeatureName::TimedWaitAny};
        wgpu::InstanceDescriptor idesc;
        idesc.requiredFeatureCount = 1;
        idesc.requiredFeatures = features;
        gpu.instance = wgpu::CreateInstance(&idesc);
        if (!gpu.instance) { error_ = "WebGPU couldn't start"; return false; }
    }
    I.instance = gpu.instance;
    wgpu::SurfaceSourceWindowsHWND src;
    src.hinstance = inst;
    src.hwnd = hwnd_;
    wgpu::SurfaceDescriptor sd;
    sd.nextInChain = &src;
    I.surface = I.instance.CreateSurface(&sd);
    if (!gpu.adapter) {
        wgpu::RequestAdapterOptions ao;
        ao.powerPreference = wgpu::PowerPreference::HighPerformance;
        ao.compatibleSurface = I.surface;
        auto f1 = I.instance.RequestAdapter(&ao, wgpu::CallbackMode::WaitAnyOnly,
                                            [&](wgpu::RequestAdapterStatus s, wgpu::Adapter a, wgpu::StringView msg) {
                                                if (s == wgpu::RequestAdapterStatus::Success) gpu.adapter = std::move(a);
                                                else error_ = "no graphics adapter: " + std::string(std::string_view(msg));
                                            });
        I.instance.WaitAny(f1, 5000000000ull);
        if (!gpu.adapter) { if (error_.empty()) error_ = "no graphics adapter"; return false; }
        wgpu::AdapterInfo info;
        gpu.adapter.GetInfo(&info);
        // the backend by name (4.3 part 3, item 2: "backend 4" wasn't readable); WebGPU's own numbering of
        // wgpu::BackendType, the same on every system (Metal on macOS, Vulkan on Linux)
        static const char* kBackends[] = {"undefined", "null", "WebGPU", "Direct3D 11", "Direct3D 12", "Metal", "Vulkan", "OpenGL", "OpenGL ES"};
        const int bt = (int)info.backendType;
        logf(Level::Info, "viewer", "adapter: %.*s, %s (driver: %.*s)", (int)info.device.length, info.device.data ? info.device.data : "?",
             bt >= 0 && bt < (int)std::size(kBackends) ? kBackends[bt] : "unknown backend", (int)info.description.length,
             info.description.data ? info.description.data : "?");
    }
    I.adapter = gpu.adapter;
    if (!gpu.device) {
        wgpu::DeviceDescriptor dd;
        // (the error's kind and the loss's reason by name, 4.3 part 3's item 8: webgpu.h's WGPUErrorType_ and
        // WGPUDeviceLostReason_ values; Dawn's own message follows, and on Direct3D 12 it carries the driver's
        // device-removed reason when there is one)
        dd.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView msg) {
            static const char* kinds[] = {"?", "no error", "validation", "out of memory", "internal", "unknown"};
            const int t = (int)type;
            logf(Level::Error, "viewer", "WebGPU %s error: %.*s", t > 0 && t < (int)std::size(kinds) ? kinds[t] : "unlisted", (int)msg.length,
                 msg.data ? msg.data : "");
        });
        dd.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
                                 [](const wgpu::Device&, wgpu::DeviceLostReason r, wgpu::StringView msg) {
                                     static const char* why[] = {"?", "unknown reason (a driver reset or crash, likely)", "destroyed",
                                                                 "callback cancelled", "creation failed"};
                                     const int k = (int)r;
                                     if (r != wgpu::DeviceLostReason::Destroyed)
                                         logf(Level::Error, "viewer", "graphics device lost: %s: %.*s", k > 0 && k < (int)std::size(why) ? why[k] : "unlisted",
                                              (int)msg.length, msg.data ? msg.data : "");
                                 });
        // the features that let the page show the view's frames (the 3D view drawn into the page, 2026-11-22: a texture
        // made by the page's Direct3D 11 device, imported here by its DXGI shared handle, a keyed mutex taking turns),
        // asked for only where the adapter has them; without them the view keeps its own window. Windows (D3D12)
        std::vector<wgpu::FeatureName> req;
        for (wgpu::FeatureName f : {wgpu::FeatureName::SharedTextureMemoryDXGISharedHandle, wgpu::FeatureName::SharedFenceDXGISharedHandle})
            if (gpu.adapter.HasFeature(f)) req.push_back(f);
        dd.requiredFeatureCount = req.size();
        dd.requiredFeatures = req.data();
        logf(Level::Info, "viewer", "texture sharing with the page: %s", req.empty() ? "not available" : "available");
        auto f2 = gpu.adapter.RequestDevice(&dd, wgpu::CallbackMode::WaitAnyOnly,
                                            [&](wgpu::RequestDeviceStatus s, wgpu::Device d, wgpu::StringView msg) {
                                                if (s == wgpu::RequestDeviceStatus::Success) gpu.device = std::move(d);
                                                else error_ = "no graphics device: " + std::string(std::string_view(msg));
                                            });
        I.instance.WaitAny(f2, 5000000000ull);
        if (!gpu.device) { if (error_.empty()) error_ = "no graphics device"; return false; }
        gpu.queue = gpu.device.GetQueue();
    }
    I.device = gpu.device;
    I.queue = gpu.queue;
    wgpu::SurfaceCapabilities caps;
    if (I.surface.GetCapabilities(I.adapter, &caps) == wgpu::Status::Success && caps.formatCount > 0) {
        I.format = caps.formats[0];
        for (size_t i = 0; i < caps.formatCount; ++i)
            if (caps.formats[i] == wgpu::TextureFormat::BGRA8Unorm) I.format = caps.formats[i];
    }

    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = kShader;
    wgpu::ShaderModuleDescriptor smd;
    smd.nextInChain = &wgsl;
    I.shader = I.device.CreateShaderModule(&smd);

    // One explicit layout for every pipeline (automatic layouts can't share bind groups): group 0 the camera / stage
    // uniforms, group 1 a material (its uniform, four samplers, four textures).
    wgpu::BindGroupLayoutEntry l0;
    l0.binding = 0;
    l0.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    l0.buffer.type = wgpu::BufferBindingType::Uniform;
    wgpu::BindGroupLayoutDescriptor bl0;
    bl0.entryCount = 1;
    bl0.entries = &l0;
    I.layout0 = I.device.CreateBindGroupLayout(&bl0);
    // the material group: its uniform, then one sampler and one texture per stage (8 of each: GX's own limit; WebGPU
    // guarantees 16 of each per shader stage)
    constexpr int S = ORRE_MAX_STAGES;
    wgpu::BindGroupLayoutEntry l1[1 + 2 * S];
    l1[0].binding = 0;
    l1[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;  // (the vertex stage lights per vertex)
    l1[0].buffer.type = wgpu::BufferBindingType::Uniform;
    for (int s = 0; s < S; ++s) {
        l1[1 + s].binding = 1 + s;
        l1[1 + s].visibility = wgpu::ShaderStage::Fragment;
        l1[1 + s].sampler.type = wgpu::SamplerBindingType::Filtering;
        l1[1 + S + s].binding = 1 + S + s;
        l1[1 + S + s].visibility = wgpu::ShaderStage::Fragment;
        l1[1 + S + s].texture.sampleType = wgpu::TextureSampleType::Float;
        l1[1 + S + s].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    }
    wgpu::BindGroupLayoutDescriptor bl1;
    bl1.entryCount = 1 + 2 * S;
    bl1.entries = l1;
    I.layout1 = I.device.CreateBindGroupLayout(&bl1);
    wgpu::BindGroupLayout both[2] = {I.layout0, I.layout1};
    wgpu::PipelineLayoutDescriptor pld;
    pld.bindGroupLayoutCount = 2;
    pld.bindGroupLayouts = both;
    I.playout = I.device.CreatePipelineLayout(&pld);

    // the background: one triangle covering the view, the stage's soft gradient
    wgpu::ColorTargetState ct;
    ct.format = I.format;
    wgpu::FragmentState bfs;
    bfs.module = I.shader;
    bfs.entryPoint = "bfs";
    bfs.targetCount = 1;
    bfs.targets = &ct;
    wgpu::DepthStencilState bds;
    bds.format = wgpu::TextureFormat::Depth24Plus;
    bds.depthWriteEnabled = wgpu::OptionalBool::False;
    bds.depthCompare = wgpu::CompareFunction::Always;
    wgpu::RenderPipelineDescriptor bpd;
    bpd.layout = I.playout;
    bpd.vertex.module = I.shader;
    bpd.vertex.entryPoint = "bvs";
    bpd.fragment = &bfs;
    bpd.depthStencil = &bds;
    I.background = I.device.CreateRenderPipeline(&bpd);

    // the particles: their own shader module and layout (group 0 the same camera uniforms, group 1 a sampler and one
    // picture), linear filtering with clamped edges (GX_CLAMP; psDispMain uses mirroring only for kind 0x20 / 0x40)
    {
        wgpu::ShaderSourceWGSL pw;
        pw.code = kParticleShader;
        wgpu::ShaderModuleDescriptor psd;
        psd.nextInChain = &pw;
        I.pshader = I.device.CreateShaderModule(&psd);
        wgpu::BindGroupLayoutEntry pl[2];
        pl[0].binding = 0;
        pl[0].visibility = wgpu::ShaderStage::Fragment;
        pl[0].sampler.type = wgpu::SamplerBindingType::Filtering;
        pl[1].binding = 1;
        pl[1].visibility = wgpu::ShaderStage::Fragment;
        pl[1].texture.sampleType = wgpu::TextureSampleType::Float;
        pl[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
        wgpu::BindGroupLayoutDescriptor pbl;
        pbl.entryCount = 2;
        pbl.entries = pl;
        I.player1 = I.device.CreateBindGroupLayout(&pbl);
        wgpu::BindGroupLayout pboth[2] = {I.layout0, I.player1};
        wgpu::PipelineLayoutDescriptor ppld;
        ppld.bindGroupLayoutCount = 2;
        ppld.bindGroupLayouts = pboth;
        I.pplayout = I.device.CreatePipelineLayout(&ppld);
        wgpu::SamplerDescriptor sd;
        sd.magFilter = wgpu::FilterMode::Linear;
        sd.minFilter = wgpu::FilterMode::Linear;
        sd.addressModeU = sd.addressModeV = wgpu::AddressMode::ClampToEdge;
        I.psampler = I.device.CreateSampler(&sd);
    }

    wgpu::BufferDescriptor ub;
    ub.size = kCameraBytes;
    ub.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    I.uniforms = I.device.CreateBuffer(&ub);
    wgpu::BindGroupEntry ce;
    ce.binding = 0;
    ce.buffer = I.uniforms;
    ce.size = kCameraBytes;
    wgpu::BindGroupDescriptor cg;
    cg.layout = I.layout0;
    cg.entryCount = 1;
    cg.entries = &ce;
    I.camera_group = I.device.CreateBindGroup(&cg);
    // (the arena's copy: the same camera with the stage's fade as its tint)
    I.uniforms_stage = I.device.CreateBuffer(&ub);
    ce.buffer = I.uniforms_stage;
    I.stage_group = I.device.CreateBindGroup(&cg);
    // (the target Pokémon's copy: the same camera with its half's fade as its tint)
    I.uniforms_target = I.device.CreateBuffer(&ub);
    ce.buffer = I.uniforms_target;
    I.target_group = I.device.CreateBindGroup(&cg);
    const uint8_t white[4] = {255, 255, 255, 255};
    I.white = I.make_texture(1, 1, white);
    I.default_group = I.material_group(nullptr, I.default_buf);
    I.pwhite = I.particle_group(I.white);
    RECT r;
    GetClientRect(hwnd_, &r);
    I.configure(r.right, r.bottom);
    ok_ = true;
    return true;
}

void Viewer::set_colours(const D2D1_COLOR_F& hi, const D2D1_COLOR_F& lo) {
    if (!impl_) return;
    const float a[4] = {hi.r, hi.g, hi.b, 1}, b[4] = {lo.r, lo.g, lo.b, 1};
    std::copy(a, a + 4, impl_->user_hi);
    std::copy(b, b + 4, impl_->user_lo);
    if (!impl_->plain) {  // (Plain stage keeps its grey until it is turned off)
        std::copy(a, a + 4, impl_->hi);
        std::copy(b, b + 4, impl_->lo);
    }
    request_frame();
}

void Viewer::set_model(OrreModel* model) {
    if (!impl_) { if (model) orre_model_free(model); return; }
    Impl& I = *impl_;
    I.parts.clear();
    I.ghosts.clear();  // (the after-images' buffers are sized to the model's meshes)
    I.textures.clear();
    orre_model_free(&I.model);
    I.has_model = false;
    if (model) {
        I.model = *model;
        *model = OrreModel{};
        for (int t = 0; t < I.model.texture_count; ++t)
            I.textures.push_back(I.make_texture(I.model.textures[t].width, I.model.textures[t].height, I.model.textures[t].rgba));
        int blended = 0;
        for (int i = 0; i < I.model.mesh_count; ++i) {
            const OrreMesh& m = I.model.meshes[i];
            // every part gets its buffers, hidden ones too: animations show and hide parts per frame
            if (m.vertex_count == 0 || m.cull == 3) continue;
            Impl::Part p;
            wgpu::BufferDescriptor vb;
            vb.size = (uint64_t)m.vertex_count * sizeof(OrreVertex);
            vb.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
            p.vbuf = I.device.CreateBuffer(&vb);
            I.queue.WriteBuffer(p.vbuf, 0, m.vertices, vb.size);
            p.count = (uint32_t)m.vertex_count;
            p.group = I.material_group(&m, p.mbuf);
            const DrawState ds = draw_state(m);
            p.pipeline = I.pipeline(ds);
            p.blended = ds.blend_type != 0;
            p.mesh = i;
            blended += p.blended;
            I.parts.push_back(std::move(p));
        }
        // opaque parts first, then the blended ones in the model's own order (blending needs what's behind it)
        std::stable_partition(I.parts.begin(), I.parts.end(), [](const Impl::Part& p) { return !p.blended; });
        I.has_model = !I.parts.empty();
        I.posed.assign(I.model.mesh_count, {});
        for (int i = 0; i < I.model.mesh_count; ++i) I.posed[i].resize(I.model.meshes[i].vertex_count);
        OrreModelSurvey sv{};
        I.has_billboards = orre_model_survey(&I.model, &sv) && sv.billboard_joints > 0;
        std::fill(I.view, I.view + 12, 0.0f);
        I.shown.assign(I.model.mesh_count, 1);
        orre_model_visibility(&I.model, -1, 0, I.shown.data());
        I.apply_textures(I.model.idle_animation, 0);  // the idle's first-frame pictures (see set_idle)
        logf(Level::Detail, "viewer", "%zu parts (%d blended), %zu pipelines, %d animations (idle %d, %.0f frames)",
             I.parts.size(), blended, I.pipelines.size(), I.model.animation_count, I.model.idle_animation,
             orre_model_animation_length(&I.model, I.model.idle_animation));
    }
    I.anim_frame = -1;
    I.anim_last = -2;
    static const bool frame_watch = std::getenv("ORRE_FRAME_WATCH") != nullptr;
    if (frame_watch) { I.watch_frames = 15; I.watch_seq = 0; }  // (the next frames are read back: render)
    I.blinking = false;  // (a new model starts its blink count afresh)
    I.blink_last = -1;
    // a new model: no move playing, and the page names the move to frame again for this model's animations
    I.once = -1;
    I.held = false;
    I.frame_anim = -1;
    // (and no move effect: its timing belongs to the previous model's time points)
    orre_wzx_free(I.seq.wzx);
    orre_wzx_free(I.tseq.wzx);
    orre_wzx_free(I.aseq.wzx);
    I.seq = Impl::MoveSeq{};
    I.tseq = Impl::MoveSeq{};
    I.aseq = Impl::MoveSeq{};
    I.box_on = false;
    // the battle grid's spacing for this Pokémon and the box (the larger size class sets it)
    I.set_grid(std::max(I.model.size_class, I.has_target() ? I.target->model.size_class : 0));
    I.particles_model();
    QueryPerformanceFrequency(&I.freq);
    I.restart_clock();
    frame_model();
    update_timer();
    request_frame();
}

// Aims the camera at what is actually drawn: the visible parts in the standing pose, or across the idle loop (8
// sampled frames) when it plays, so a Pokémon that rises or crouches in its idle stays in view (Weezing floats above
// its standing pose; Blaziken stands up). Computed once per model and mode, so the camera never wobbles.
void Viewer::frame_model() {
    if (!impl_ || !impl_->has_model) return;
    Impl& I = *impl_;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    const bool anim = idle_ && has_idle();
    const int a = anim ? I.model.idle_animation : -1;
    const float len = anim ? orre_model_animation_length(&I.model, a) : 0;
    std::vector<std::vector<OrreVertex>> buf(I.model.mesh_count);
    std::vector<OrreVertex*> out(I.model.mesh_count);
    for (int i = 0; i < I.model.mesh_count; ++i) { buf[i].resize(I.model.meshes[i].vertex_count); out[i] = buf[i].data(); }
    std::vector<uint8_t> shown(I.model.mesh_count, 1);
    auto take = [&](int an, float f) {
        if (!orre_model_pose(&I.model, an, f, out.data())) return false;
        orre_model_visibility(&I.model, an, f, shown.data());
        for (int i = 0; i < I.model.mesh_count; ++i) {
            if (!shown[i]) continue;
            for (const OrreVertex& v : buf[i])
                for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], v.pos[k]); hi[k] = std::max(hi[k], v.pos[k]); }
        }
        return true;
    };
    for (int s = 0; s < (anim ? 8 : 1); ++s)
        if (!take(a, anim ? std::floor(len * s / 8) : 0)) return;
    // the Moves tab's move too (12 samples: a lunge or a jump stays in view), so playing it never moves the camera
    if (I.frame_anim >= 0) {
        const float ml = orre_model_animation_length(&I.model, I.frame_anim);
        for (int s = 0; s < 12 && ml > 0; ++s) take(I.frame_anim, std::floor(ml * s / 12));
    }
    if (lo[0] > hi[0]) for (int k = 0; k < 3; ++k) { lo[k] = I.model.bounds_min[k]; hi[k] = I.model.bounds_max[k]; }
    // the target box too, while a move with a target half is chosen (it stands at the model's (0, 0, 80), a box's
    // width each side), so the shot shows both ends of the move until the battle camera takes over
    if (I.box_on && I.frame_anim >= 0) {
        const float hs = Impl::kBoxSize * 0.5f + 4.0f;  // (+ the hit's shove)
        const float bz = I.kTargetX - I.kUserX;
        lo[0] = std::min(lo[0], -hs); hi[0] = std::max(hi[0], hs);
        lo[1] = std::min(lo[1], 0.0f); hi[1] = std::max(hi[1], Impl::kBoxSize);
        lo[2] = std::min(lo[2], bz - hs); hi[2] = std::max(hi[2], bz + hs);
    }
    for (int k = 0; k < 3; ++k) I.centre[k] = (lo[k] + hi[k]) / 2;
    const float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    I.radius = std::max(0.001f, std::sqrt(dx * dx + dy * dy + dz * dz) / 2);
}

bool Viewer::has_model() const { return impl_ && impl_->has_model; }
bool Viewer::has_idle() const {
    return impl_ && impl_->has_model && impl_->model.idle_animation >= 0 &&
           orre_model_animation_length(&impl_->model, impl_->model.idle_animation) > 0;
}

void Viewer::set_idle(bool on) {
    idle_ = on;
    if (impl_) {
        if (impl_->once < 0) {
            impl_->tgt_phase = impl_->target_phase_at(0);  // (the target's idle carries on)
            impl_->restart_clock();  // the loop restarts from its first frame
        }
        if (!on && impl_->once < 0) show_standing();
        frame_model();
    }
    update_timer();
    request_frame();
    // a view cut to nothing gets no WM_PAINT, so the new model is drawn now: when the page reveals the view, its
    // picture is already there
    if (impl_ && impl_->cut) render();
}

// The page cuts the view to nothing instead of hiding its window (an empty window region), when the Overview isn't
// shown or before its first frame. Hiding the window let its surface go blank, and showing it again flashed white
// until the next frame; a cut window keeps its last picture, and comes back with it at once. While cut, the idle loop
// and the spin stop (the timer skips it), so the model is frozen where it was. Windows only (window regions); Linux /
// macOS: the toolkit keeps a hidden view's layer, so plain hiding would do there (likely).
void Viewer::set_cut(bool on) {
    if (!impl_ || !hwnd_) return;
    if (on && !impl_->cut) SetWindowRgn(hwnd_, CreateRectRgn(0, 0, 0, 0), FALSE);
    if (on == impl_->cut) return;
    logf(Level::Info, "surface", "view %s", on ? "cut (hidden, keeps its picture)" : "shown again");  // (flash suspects)
    if (on) { set_paused(true); impl_->cut = true; }
    else {
        impl_->cut = false;
        set_paused(false);
        // (the animation clock must never be ahead of now after a resume: that is the "slow to load in" of
        // restart_clock; logged so a hand check's log shows it)
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (impl_->freq.QuadPart && impl_->anim_start.QuadPart > now.QuadPart)
            logf(Level::Warn, "viewer", "animation clock %.2f s ahead after the view was shown again",
                 (double)(impl_->anim_start.QuadPart - now.QuadPart) / (double)impl_->freq.QuadPart);
    }
}
bool Viewer::framed() const { return impl_ && impl_->framed; }
int Viewer::shared_frames() const { return impl_ ? impl_->share_frames : 0; }
// A new frame, asked for: a repaint of the view's window, or in page mode (where that window shows nothing and gets no
// paints) a posted message, one at a time (Viewer::proc, WM_APP + 31)
void Viewer::request_frame() {
    if (!hwnd_) return;
    if (impl_ && impl_->page) {
        if (!impl_->frame_posted.exchange(true)) PostMessageW(hwnd_, WM_APP + 31, 0, 0);
    } else {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}
// Page mode on or off (the page calls it once sharing works, ensure_shared): the view draws into its own texture, the
// page shows it; off, back to its window's surface
void Viewer::set_page_mode(bool on) {
    if (!impl_ || impl_->page == on) return;
    Impl& I = *impl_;
    I.page = on;
    I.offscreen = nullptr;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    I.configure(rc.right, rc.bottom);
    logf(Level::Info, "viewer", "page mode %s", on ? "on: frames go to the page" : "off: frames go to the view's window");
    request_frame();
}
bool Viewer::share_into(void* nt_handle, int w, int h) {
    if (!impl_) return false;
    Impl& I = *impl_;
    I.share_tex = nullptr;
    I.share_mem = nullptr;
    I.share_init = false;
    I.share_w = I.share_h = 0;
    I.share_fw = I.share_fh = 0;
    if (!nt_handle || w <= 0 || h <= 0 || !I.device.HasFeature(wgpu::FeatureName::SharedTextureMemoryDXGISharedHandle)) return false;
    wgpu::SharedTextureMemoryDXGISharedHandleDescriptor hd;
    hd.handle = nt_handle;
    hd.useKeyedMutex = true;
    wgpu::SharedTextureMemoryDescriptor md;
    md.nextInChain = &hd;
    I.share_mem = I.device.ImportSharedTextureMemory(&md);
    if (!I.share_mem) { logf(Level::Error, "viewer", "the page's texture couldn't be imported"); return false; }
    wgpu::TextureDescriptor td;
    td.size = {(uint32_t)w, (uint32_t)h, 1};
    td.format = wgpu::TextureFormat::BGRA8Unorm;
    td.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment;
    I.share_tex = I.share_mem.CreateTexture(&td);
    if (!I.share_tex) { I.share_mem = nullptr; logf(Level::Error, "viewer", "no texture from the page's shared memory"); return false; }
    I.share_w = w;
    I.share_h = h;
    logf(Level::Info, "viewer", "sharing frames with the page at %dx%d", w, h);
    return true;
}
bool Viewer::cut() const { return impl_ && impl_->cut; }
void Viewer::shared_frame_size(int& w, int& h) const {
    w = impl_ ? impl_->share_fw : 0;
    h = impl_ ? impl_->share_fh : 0;
}

// back to the standing pose: the bind vertices again
void Viewer::show_standing() {
    if (!impl_ || !impl_->has_model) return;
    Impl& I = *impl_;
    for (auto& p : I.parts)
        I.queue.WriteBuffer(p.vbuf, 0, I.model.meshes[p.mesh].vertices, (uint64_t)p.count * sizeof(OrreVertex));
    orre_model_visibility(&I.model, -1, 0, I.shown.data());
    // the standing pose wears the idle's first-frame pictures: a material's own picture can be a non-default
    // expression (Shellder's own eye picture is its spiral "confused" eyes)
    I.apply_textures(I.model.idle_animation, 0);
    I.anim_frame = -1;
    I.anim_last = -2;
}

bool Viewer::animating() const {
    const bool once = impl_ && impl_->once >= 0 && !impl_->held;
    return spin_ || once || (idle_ && has_idle());
}
// The tick runs only while something moves: the spin, a move playing once, or the idle loop (idle means idle). At the
// game's rate (and up to 64 Hz) a Windows timer (16 ms for 60; Windows timers can't run faster than about 64 a
// second). Above 64 Hz the pacer thread asks for each frame as a repaint (Impl::start_pacer); presenting waits
// for the screen's refresh (vsync), which paces it, and a rate below the screen's waits out the gap. Windows message
// loop and timers; Linux / macOS: the toolkit's frame callback (CVDisplayLink / a frame clock) at the same rate
void Viewer::update_timer() {
    if (!hwnd_) return;
    if (!animating()) {
        KillTimer(hwnd_, 1);
        if (impl_) impl_->stop_pacer();
        return;
    }
    const int r = impl_ ? impl_->rate : 0;
    if (r > 64) {
        KillTimer(hwnd_, 1);
        impl_->start_pacer(hwnd_, r);
    } else {
        if (impl_) impl_->stop_pacer();
        SetTimer(hwnd_, 1, r > 0 ? (UINT)std::max(10, 1000 / r) : 16, nullptr);
    }
}
void Viewer::set_rate(int hz) {
    if (!impl_ || impl_->rate == hz) return;
    impl_->rate = std::max(0, hz);
    impl_->anim_frame = -1;  // (re-posed at the new rate's next tick)
    update_timer();
}
int Viewer::rate() const { return impl_ ? impl_->rate : 0; }

// Advances what plays: a move's animation played once (the Moves tab), else the idle loop. The pose is recomputed and
// uploaded only when the whole animation frame changes (30 a second, ORRE_BATTLE_ANIM_FPS). True when something
// changed.
bool Viewer::advance_idle() {
    if (!impl_ || impl_->paused) return false;
    Impl& I = *impl_;
    int anim = I.model.idle_animation, frame = 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double secs = I.freq.QuadPart ? (double)(now.QuadPart - I.anim_start.QuadPart) / (double)I.freq.QuadPart : 0.0;
    // Smoothing (a preview rate above the game's, set_rate; 2026-11-19, release step 3; snatcherLeo: "if people want to, they
    // can see that effect at 120fps or higher frame rate and it would just be... smoother"): the pose is taken at the
    // clock's exact frame, fraction included, so it moves between the game's frames; at every whole frame it is exactly
    // the game's pose. Pictures (texture animations, the eyes and mouths) and shown / hidden parts still change on
    // whole frames only, as the game draws them. At the game's rate (rate 0) nothing changes: whole frames, 30 a second.
    const bool smooth = I.rate > 0;
    double t = 0;  // the frame posed: whole at the game's rate or while held, with its fraction when smoothing
    double idle_clock = -1;  // while idling: battle frames since the idle started, unwrapped (the target's own loop)
    int tex_anim = -1;     // the texture animation shown (-1: the posed animation's own; the blink's while blinking)
    float tex_frame = 0;
    float blink_key = -1;   // the blink's frame while blinking (a change redraws), else -1
    if (I.once >= 0) {
        I.blinking = false;  // (no blink while a move plays, as wazaSequenceOwnerUpdateBlink returns then)
        anim = I.once;
        // (the timeline's length: the animation, plus a held ending on its last pose for the faint, set_play_tail)
        const int len = std::max(1, timeline_frames(anim));
        if (hold_ >= 0) frame = std::min(hold_walk_ >= 0 ? hold_walk_ : hold_, len - 1);  // screenshot tests: that frame, held
        else if (I.held) frame = I.once_at;
        else {
            const double ft = secs * ORRE_BATTLE_ANIM_FPS;
            frame = (int)std::floor(ft);
            if (smooth && frame < len) t = std::min(ft, (double)(len - 1));
            if (frame >= len) {
                // the move has played: back to the idle loop from its first frame (or the standing pose), and the page
                // is told (its Play button); the target's idle carries on from its last frame
                I.tgt_phase = I.target_phase_at(len - 1);
                I.once = -1;
                I.anim_start = now;
                I.anim_last = -2;
                if (hwnd_) PostMessageW(GetParent(hwnd_), WM_APP + 22, 0, 0);
                update_timer();
                if (!(idle_ && has_idle())) { show_standing(); return true; }
                return advance_idle();
            }
            I.once_at = frame;
        }
    } else {
        if (!idle_ || !has_idle()) return false;
        const float len = orre_model_animation_length(&I.model, anim);
        // looping wraps by the end frame, as GSmodelAdvanceAnimation does (models-animation.md)
        const double ft = std::fmod(secs * ORRE_BATTLE_ANIM_FPS, (double)std::max(1.0f, std::floor(len)));
        frame = (int)std::floor(ft);
        if (smooth) t = ft;
        idle_clock = smooth ? secs * ORRE_BATTLE_ANIM_FPS : std::floor(secs * ORRE_BATTLE_ANIM_FPS);
        if (hold_ >= 0) { frame = hold_ % std::max(1, (int)len); t = 0; idle_clock = hold_; }
        // The idle blink (2026-11-19, release step 3; studio/blink-plan.md): the game's own rule,
        // wazaSequenceOwnerUpdateBlink (verified from the matched code): counting battle frames since the last blink,
        // never before 10, then a chance each frame of t(2 - t)/60 with t = (frames - 10) / 50 up to 60, 1 in 60 up to
        // 180, and certain from 180; a blink plays the model's blink texture animation (PKX header 0x20) once from its
        // start at rate 0.5, then the idle's pictures return. Studio rolls its own random numbers (the game's come from
        // the shared battle sequence, so the exact moments can't match anyway). Held frames and screenshots never
        // blink; a model without a blink animation doesn't. Whether the game's frame step counts 30 or 60 a second is
        // not settled (likely 30, the battle's rate)
        const int bi = I.model.eye_texanim[2];
        if (bi >= 0 && hold_ < 0) {
            const long long absf = (long long)std::floor(secs * ORRE_BATTLE_ANIM_FPS);
            if (I.blink_last < 0 || absf < I.blink_last) { I.blink_last = absf; I.blink_timer = 0; I.blinking = false; }
            const float blen = std::max(1.0f, orre_model_animation_length(&I.model, bi));
            for (long long a = I.blink_last + 1; a <= absf && a - I.blink_last <= 300; ++a) {
                if (I.blinking) {
                    if ((float)(a - I.blink_start) * 0.5f >= blen) { I.blinking = false; I.blink_timer = 0; }
                    continue;
                }
                if (++I.blink_timer < 10) continue;
                double p = 1.0;
                if (I.blink_timer < 60) {
                    const double tt = (I.blink_timer - 10) / 50.0;
                    p = tt * (2.0 - tt) / 60.0;
                } else if (I.blink_timer < 180) {
                    p = 1.0 / 60.0;
                }
                if (std::uniform_real_distribution<double>(0.0, 1.0)(I.blink_rng) < p) {
                    I.blinking = true;
                    I.blink_start = a;
                    I.blink_timer = 0;
                }
            }
            I.blink_last = absf;
            if (I.blinking) { tex_anim = bi; tex_frame = (float)(absf - I.blink_start) * 0.5f; blink_key = tex_frame; }
        }
        // ORRE_DEBUG_BLINK=<frame> (developer switch, any OS): show the blink animation's pictures at that frame, held
        // (a blink comes at random moments, so tests look at it this way)
        if (const char* db = std::getenv("ORRE_DEBUG_BLINK"); db && bi >= 0) {
            tex_anim = bi;
            tex_frame = (float)std::atof(db);
            blink_key = tex_frame;
        }
    }
    if (!smooth || t < frame) t = frame;  // (held frames, hold_, and the game's rate: the whole frame)
    // the timeline's own frame (the particles, the box and the target's half run on it), before the pose is held
    const int tl_frame = frame;
    const double tl_t = t;
    if (I.once >= 0) {
        // in a held ending the pose stays on the animation's last frame (the playhead and counter carry on); a half
        // with a lead-in shows its animation that many frames late, its ending first (Impl::lead_frame)
        const int last = std::max(0, animation_frames(anim) - 1);
        const int pf = I.user_pose(anim, frame);
        t = std::clamp(pf + (t - frame), (double)pf, (double)std::max(pf, last));  // (the smoothing's fraction kept)
        frame = pf;
    }
    // the third part's own animation once it runs (Impl::user_anim: the drains' return)
    int pose_anim = anim;
    if (I.once >= 0) {
        int ua, upf;
        I.user_anim(anim, tl_frame, ua, upf);
        if (ua != anim) {
            pose_anim = ua;
            frame = upf;
            t = upf;
        }
    }
    if (frame == I.anim_frame && t == I.anim_t && anim == I.anim_last && blink_key == I.blink_shown &&
        tl_t == I.anim_tl)
        return false;
    I.blink_shown = blink_key;
    I.anim_tl = tl_t;
    I.box_frame = I.once >= 0 ? tl_frame : -1;
    // the particles: both halves of each whole frame at the game's rate, each half at its own time when smoothing
    const bool back = I.once >= 0 && I.held;
    I.particles_to(anim, smooth && !back && hold_ < 0 ? (int)std::floor(tl_t * 2) : tl_frame * 2 + 1, back);
    // the battle camera for this frame of the move (render reads it), and the move's own models posed for it
    I.cam_to(tl_frame);
    if (I.once >= 0) I.fx_models_at(tl_frame);
    // the target Pokémon in the box's place (comparisons with the game's move sweep): posed for this frame of the
    // move, or its idle loop alongside the Pokémon's
    if (I.has_target() && I.box_on) {
        int ta;
        float tf;
        I.target_anim_at(I.once >= 0 ? tl_frame : -1, ta, tf);
        if (I.once < 0) {
            // its own idle on its own loop, from the clock. *Fixed 2026-11-22* (snatcherLeo: "for Espeon, the target pokemon, I
            // noticed that its idle loops awkwardly, but the idle for Wartortle's animation loops just fine"): it took
            // the Pokémon's idle frame, which wraps at the Pokémon's idle length, so a target whose idle is longer (or
            // not a whole multiple of it) jumped back to its start partway through each loop
            const float il = ta >= 0 ? std::max(1.0f, std::floor(orre_model_animation_length(&I.target->model, ta))) : 1.0f;
            const double clk = (idle_clock >= 0 ? idle_clock : (double)frame) + I.tgt_phase;
            tf = ta >= 0 ? (float)std::fmod(clk, (double)il) : 0.0f;
        }
        float M[12], cs[3];
        I.target_xf(M);
        if (I.once >= 0 && I.carry_offset_studio(true, tl_frame, cs))  // (carried: Earthquake's, Waterfall's target)
            for (int c = 0; c < 3; ++c) M[9 + c] += cs[c];
        I.inst_pose(*I.target, ta, tf, M);
        // (--log-detail: each frame's poses, so a jump between one move and the next can be read from the log)
        logf(Level::Detail, "pose", "once %d tl %d | user anim %d frame %.2f | target anim %d frame %.2f", I.once, tl_frame,
             pose_anim, t, ta, tf);
    } else {
        logf(Level::Detail, "pose", "once %d tl %d | user anim %d frame %.2f", I.once, tl_frame, pose_anim, t);
    }
    I.anim_frame = frame;
    I.anim_t = t;
    I.anim_last = anim;
    std::vector<OrreVertex*> out(I.model.mesh_count);
    for (int i = 0; i < I.model.mesh_count; ++i) out[i] = I.posed[i].data();
    if (!orre_model_pose_view(&I.model, pose_anim, (float)t, I.has_billboards ? I.view : nullptr, out.data())) return false;
    // carried by a move's model (Dig, Dive, Waterfall, Withdraw: carry_user_xf): placed, turned and sized with the
    // model's joint as its follow mode says
    float CM[12];
    if (I.once >= 0 && I.carry_user_xf(tl_frame, CM))
        for (auto& pm : I.posed)
            for (OrreVertex& v : pm) {
                const float x = v.pos[0], y = v.pos[1], z = v.pos[2], a = v.nrm[0], b = v.nrm[1], c = v.nrm[2];
                for (int r = 0; r < 3; ++r) {
                    v.pos[r] = CM[3 * r] * x + CM[3 * r + 1] * y + CM[3 * r + 2] * z + CM[9 + r];
                    v.nrm[r] = CM[3 * r] * a + CM[3 * r + 1] * b + CM[3 * r + 2] * c;
                }
            }
    orre_model_visibility(&I.model, pose_anim, (float)frame, I.shown.data());
    // pictures on whole frames (the game's), the blink's while it blinks
    if (tex_anim >= 0) I.apply_textures(tex_anim, tex_frame);
    else I.apply_textures(pose_anim, (float)frame);
    for (auto& p : I.parts) I.queue.WriteBuffer(p.vbuf, 0, I.posed[p.mesh].data(), (uint64_t)p.count * sizeof(OrreVertex));
    return true;
}

// Pauses the idle loop and the spin while a tab is dragged (snatcherLeo, 2026-11-19: "we could freeze the movement of the idle
// pose ... as we drag the tab elsewhere, and then it resumes the loop when we let go"). The loop runs on the clock, so
// on resuming its start moves forward by the time paused: it carries on from the frame it held instead of jumping.
// (A drag starves this window's timer anyway, since Windows sends WM_TIMER only when nothing else waits; pausing makes
// that freeze deliberate. Windows clock: QueryPerformanceCounter; Linux / macOS: std::chrono::steady_clock.)
void Viewer::set_paused(bool on) {
    if (!impl_ || impl_->paused == on) return;
    if (!on && impl_->cut) return;  // a cut view stays frozen until the page reveals it (set_cut), whatever a drag does
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (on) impl_->paused_at = now;
    else impl_->anim_start.QuadPart += now.QuadPart - impl_->paused_at.QuadPart;
    impl_->paused = on;
}
bool Viewer::paused() const { return impl_ && impl_->paused; }

// ---- the Moves tab's preview (2026-11-19) ----
// A move's body animation is one of the model's animations (the move's WZX names a PKX slot; the slot names the
// animation: orre_move_effect, orre_model_slot_animation). It plays once, at the battle's 30 frames a second, then the
// idle loop carries on from its first frame, as the game returns a Pokémon to its idle after a move (likely; the
// battle's wazaSequence owner code, models-animation.md). Same clock as the idle loop (QueryPerformanceCounter on
// Windows; std::chrono::steady_clock on Linux / macOS).
int Viewer::animation_frames(int anim) const {
    if (!impl_ || !impl_->has_model || anim < 0 || anim >= impl_->model.animation_count) return 0;
    return (int)std::floor(orre_model_animation_length(&impl_->model, anim));
}
void Viewer::set_play_tail(int anim, int frames) {
    if (!impl_) return;
    impl_->tail_anim = anim;
    impl_->tail = std::max(0, frames);
}
// A held ending on every playback (the Moves and Animations tabs): 1.5 seconds at the game's 30 frames a second after
// everything the animation or move does, its last pose and fading effects still on screen, before it goes back to the
// idle. snatcherLeo, 2026-11-22: "Can we also extend the timeline, for all Pokemon, just a little more (for like a second or
// 2)? Just to have extra time to see the final ending of the animation." A Studio viewing aid, not the game's timing
// (in battle the next step follows at once); the ruler shades it after the animation's own end.
constexpr int kEndHoldFrames = 45;
int Viewer::timeline_frames(int anim) const {
    const int n = animation_frames(anim);
    if (n <= 0 || !impl_) return n;
    int extra = anim == impl_->tail_anim ? impl_->tail : 0;
    // a move whose target half runs past the user's animation (the box's hit and its effects)
    if (impl_->seq.wzx && anim == impl_->seq.anim)
        extra = std::max({extra, impl_->move_tail, impl_->half_lead(impl_->seq)});  // (a lead-in plays the animation late)
    return n + extra + kEndHoldFrames;
}
int Viewer::slot_animation(int slot) const {
    if (!impl_ || !impl_->has_model || slot < 0) return -1;
    return orre_model_slot_animation(&impl_->model, slot);
}
// whether the chosen target can react to the move (2026-11-19: "When a target can't react ... say so in the
// preview"): -3 no target Pokémon or no target half; else its own animation for the target half's slot (slot_own's
// values: -1 empty, -2 not in its table), the slot in *slot
int Viewer::target_reaction(int* slot) const {
    if (!impl_ || !impl_->has_target() || !impl_->tseq.wzx) return -3;
    OrreWzxHeader h{};
    orre_wzx_header(impl_->tseq.wzx, &h);
    if (slot) *slot = h.slot;
    const OrreModel& m = impl_->target->model;
    if (h.slot < 0 || h.slot >= 17 || h.slot >= m.slot_count) return -2;
    return m.slot_animation[h.slot];
}
int Viewer::slot_own(int slot) const {
    if (!impl_ || !impl_->has_model || slot < 0 || slot >= 17) return -2;
    if (slot >= impl_->model.slot_count) return -2;     // the model's slot table is shorter
    return impl_->model.slot_animation[slot];           // -1: the slot is empty (the game falls back)
}
int Viewer::animation_count() const { return impl_ && impl_->has_model ? impl_->model.animation_count : 0; }
bool Viewer::eye_animation(int anim) const {
    if (!impl_ || !impl_->has_model || anim < 0) return false;
    for (int k = 0; k < 3; ++k)
        if (impl_->model.eye_texanim[k] == anim) return true;
    return false;
}
// The move's own effects (2026-11-19, release step 4): the effect file the next play of `anim` runs beside it (owned
// by the viewer from here; NULL clears). move and part pick the game's per-move effect scale.
// The target Pokémon in the box's place (release step 4; the comparisons with the game's move sweep, Espeon on Umbreon):
// owned by the viewer; NULL brings the box back. The battle grid's spacing follows the larger of the two size classes
void Viewer::set_target_model(OrreModel* m) {
    if (!impl_) { if (m) orre_model_free(m); return; }
    Impl& I = *impl_;
    I.target.reset();
    if (m) {
        I.target = std::make_unique<Impl::Inst>();
        I.target->model = *m;
        *m = OrreModel{};
        I.inst_build(*I.target);
    }
    I.set_grid(std::max(I.model.size_class, I.has_target() ? I.target->model.size_class : 0));
    request_frame();
}
void Viewer::set_move_effect(OrreWzx* wzx, OrreWzx* target, int move, int part, int anim) {
    if (!impl_) {
        orre_wzx_free(wzx);
        orre_wzx_free(target);
        return;
    }
    Impl& I = *impl_;
    // the particle system lets go of the old files' banks first. *Fixed 2026-11-22* (snatcherLeo's crash switching Solarbeam's
    // Turn 1 / Turn 2 while it played; reproduced with --flip-turns, the stack in OrreParticleSystem::interpret_one under
    // advance_idle): the banks live inside the WZX files freed below, and the live particles and generators kept
    // reading them on the next step
    if (I.ps) I.particles_model();
    if (I.seq.wzx != wzx) orre_wzx_free(I.seq.wzx);
    if (I.tseq.wzx != target) orre_wzx_free(I.tseq.wzx);
    orre_wzx_free(I.aseq.wzx);  // (set_move_after gives the new one, if any)
    I.seq = Impl::MoveSeq{};
    I.tseq = Impl::MoveSeq{};
    I.aseq = Impl::MoveSeq{};
    I.bolt_tex.clear();  // (keyed by the files, which change now)
    I.billboard_inst.clear();
    I.billboard_now.clear();
    I.leaf_inst.clear();
    I.leaves_now.clear();
    I.warp_pics.clear();
    I.warps_now.clear();
    // the move's own models in the scene (Bite's jaws, for example) and what it hid go with it. *Fixed 2026-11-22*
    // (snatcherLeo: Bite played on the Moves tab, then Body attack on the Animations tab "replays the particle of Bite"):
    // only seq_begin emptied them, so models a move had put in the scene stayed drawn, at the shown animation's frame,
    // after its effects were dropped (and they were built from the files freed above)
    I.fx_models.clear();
    I.user_hidden = I.box_hidden = false;
    I.seq.wzx = wzx;
    I.seq.move = move;
    I.seq.part = part;
    I.seq.anim = anim;
    I.move_tail = 0;
    I.box_on = false;
    if (wzx && target) {
        // the target's half on the box (part 2), from when the user's half ends
        I.tseq.wzx = target;
        I.tseq.move = move;
        I.tseq.part = 2;
        I.tseq.box = true;
        I.tseq.anim = anim;
        OrreWzxHeader h{};
        orre_wzx_header(wzx, &h);
        int slot = h.slot;
        if (slot >= I.model.slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
        slot = std::clamp(slot, 0, 16);
        I.seq.start.assign((size_t)h.count, 0);
        I.seq.order.assign((size_t)h.count, 0);
        orre_wzx_schedule(wzx, I.model.slot_times[slot], I.seq.start.data(), I.seq.order.data());
        const int first = I.user_end_frame(anim);
        const int len = std::max(1, animation_frames(anim));
        I.move_tail = std::max(0, first + Impl::kTargetDelay + I.target_half_frames() - len);
        I.tshot = first;
        I.tseq.first_frame = first + Impl::kTargetDelay;
        I.box_on = true;  // (standing idle until the move plays)
    } else {
        orre_wzx_free(target);
    }
    frame_model();  // (the box joins the framing when there is a target half)
}
void Viewer::set_move_after(OrreWzx* after) {
    if (!impl_) {
        orre_wzx_free(after);
        return;
    }
    Impl& I = *impl_;
    if (I.ps && I.aseq.wzx && I.aseq.wzx != after) I.particles_model();  // (its banks too: see set_move_effect)
    if (I.aseq.wzx != after) orre_wzx_free(I.aseq.wzx);
    I.aseq = Impl::MoveSeq{};
    if (!after || !I.seq.wzx) {  // (a target half is not needed: Substitute, Helping Hand, Wish have none)
        if (after && after != I.aseq.wzx) orre_wzx_free(after);
        return;
    }
    I.aseq.wzx = after;
    I.aseq.move = I.seq.move;
    I.aseq.part = 3;
    I.aseq.anim = I.seq.anim;
    // the timeline's length before the first play (seq_begin sets it again then): the target's half, then this part's
    // slot animation with its lead-in or its effects' run, whichever is longer
    OrreWzxHeader h{};
    orre_wzx_header(after, &h);
    int slot = h.slot;
    if (slot >= I.model.slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
    slot = std::clamp(slot, 0, 16);
    I.aseq.slot = slot;
    const int first = I.tseq.wzx ? I.tseq.first_frame + I.target_half_frames() + Impl::kTargetDelay
                                 : std::max(1, animation_frames(I.seq.anim)) + Impl::kTargetDelay;
    const int run = (orre_wzx_run_ticks(after, I.model.slot_times[slot], 600) + 1) / 2 + I.half_lead(I.aseq);
    const int a3 = orre_model_slot_animation(&I.model, slot);
    const int alen3 = a3 >= 0 ? (int)std::floor(orre_model_animation_length(&I.model, a3)) : 0;
    const int len = std::max(1, animation_frames(I.seq.anim));
    I.move_tail = std::max(I.move_tail, first + std::max(run, I.half_lead(I.aseq) + alen3) - len);
}
bool Viewer::play_once(int anim) {
    if (animation_frames(anim) <= 0) return false;
    Impl& I = *impl_;
    // the target's idle carries on from where it is now (before the new sequence changes its schedule)
    // (a screenshot test's held frame starts it from its first frame, so the move sweep's pictures stay the same run to run)
    I.tgt_phase = hold_ >= 0 ? 0.0 : I.target_phase_at(I.once_at);
    // a move with its effect file: its sequence starts with the animation, afresh each time it is played (and its
    // battle camera with a new seed, as the game's shots differ from one use of a move to the next)
    if (I.seq.wzx && anim == I.seq.anim) {
        logf(Level::Info, "viewer", "play: animation %d with the move's effects (move %d)", anim, I.seq.move);
        I.cam_seed = 0x9E3779B9u * ++I.plays;
        I.seq.armed = true;
        I.seq_begin(anim);
    } else {
        I.seq.running = false;
        I.aseq.running = false;
    }
    I.once = anim;
    I.once_at = 0;
    I.held = false;
    I.anim_last = -2;
    I.restart_clock();
    update_timer();
    request_frame();
    return true;
}
void Viewer::stop_once() {
    if (!impl_ || impl_->once < 0) return;
    Impl& I = *impl_;
    I.tgt_phase = I.target_phase_at(I.once_at);  // (the target's idle carries on)
    I.once = -1;
    I.held = false;
    I.anim_last = -2;
    I.restart_clock();
    if (!(idle_ && has_idle())) show_standing();
    update_timer();
    request_frame();
    if (I.cut) render();
}
int Viewer::playing() const { return impl_ ? impl_->once : -1; }
bool Viewer::held() const { return impl_ && impl_->once >= 0 && impl_->held; }
int Viewer::once_frame() const { return impl_ && impl_->once >= 0 ? impl_->once_at : 0; }
void Viewer::set_held(bool on) {
    if (!impl_ || impl_->once < 0 || impl_->held == on) return;
    Impl& I = *impl_;
    I.held = on;
    if (!on) {
        // carries on from the held frame: the clock starts that many frames back
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        I.anim_start.QuadPart = now.QuadPart - (LONGLONG)((double)I.once_at / ORRE_BATTLE_ANIM_FPS * (double)I.freq.QuadPart);
    }
    update_timer();
    request_frame();
}
void Viewer::step(int anim, int delta) {
    if (!impl_) return;
    Impl& I = *impl_;
    if (I.once < 0 || I.once != anim) {
        if (!play_once(anim)) return;
        I.held = true;
        I.once_at = 0;
        delta = 0;  // the first press shows the move's first frame
    }
    I.held = true;
    I.once_at = std::clamp(I.once_at + delta, 0, std::max(0, timeline_frames(I.once) - 1));
    update_timer();
    render();  // (render poses the held frame first)
}
// The Moves tab's timeline (snatcherLeo, 2026-11-19: a bar "like in Blender ... users can drag back and forth"): holds the
// animation on the frame the bar's playhead was dragged to (starting it first if another one, or none, was playing)
void Viewer::seek(int anim, int frame) {
    if (!impl_) return;
    Impl& I = *impl_;
    if (I.once != anim && !play_once(anim)) return;
    I.held = true;
    I.once_at = std::clamp(frame, 0, std::max(0, timeline_frames(I.once) - 1));
    update_timer();
    render();
}
void Viewer::set_frame_animation(int anim) {
    if (!impl_ || impl_->frame_anim == anim) return;
    impl_->frame_anim = anim;
    frame_model();
    request_frame();
    if (impl_->cut) render();
}
void Viewer::set_plain_stage(bool on) {
    if (!impl_ || impl_->plain == on) return;
    Impl& I = *impl_;
    I.plain = on;
    // the move viewer's backdrop: flat 0x7F7F7F (the project's _BACKDROP=1 shots); the surface is a plain (not sRGB) format,
    // so the value goes through as is
    const float grey[4] = {127 / 255.0f, 127 / 255.0f, 127 / 255.0f, 1};
    std::copy(on ? grey : I.user_hi, (on ? grey : I.user_hi) + 4, I.hi);
    std::copy(on ? grey : I.user_lo, (on ? grey : I.user_lo) + 4, I.lo);
    request_frame();
    if (I.cut) render();
}
bool Viewer::plain_stage() const { return impl_ && impl_->plain; }
// Studio's camera on any stage (on: the Overview) or the game's move-viewer angle on the Plain stage and the Arena (off:
// the Moves tab); see Impl::studio_cam
void Viewer::set_stage_camera(bool studio) {
    if (!impl_ || impl_->studio_cam == studio) return;
    impl_->studio_cam = studio;
    request_frame();
    if (impl_->cut) render();
}
// The arena's model (the app reads it from the disc once: Mt. Battle's valley, D2_valley_1_bf entry 2, since
// 2026-11-22; before that waza_viewer entry 3, M1_water_colo.dat; ORRE_ARENA picks another) and whether it is
// drawn (the Moves tab's Arena stage, with Plain stage's camera)
void Viewer::set_arena_model(OrreModel* m) {
    if (!impl_) { if (m) orre_model_free(m); return; }
    Impl& I = *impl_;
    I.arena.reset();
    if (m) {
        I.arena = std::make_unique<Impl::Inst>();
        I.arena->model = *m;
        *m = OrreModel{};
        I.inst_build(*I.arena);
        I.arena_placed = 0;
    }
    request_frame();
}
bool Viewer::has_arena_model() const { return impl_ && impl_->arena; }
void Viewer::set_arena(bool on) {
    if (!impl_ || impl_->arena_on == on) return;
    impl_->arena_on = on;
    impl_->arena_placed = 0;
    request_frame();
    if (impl_->cut) render();
}

// The Pokémon's own particle effects (2026-11-19, particles step 1). Off: none are drawn and the run stops; back on:
// it starts afresh with the next frame (snatcherLeo, 2026-11-19: "we can toggle those off in the overview tab ... anytime,
// and that change gets reflected when we see them in action in the moves").
void Viewer::set_particles(bool on) {
    if (!impl_ || impl_->particles_on == on) return;
    impl_->particles_on = on;
    // afresh either way: on, the bank, the pictures, seed 1; off, the Pokémon's own effects are cleared (a move's
    // effects don't follow this switch: they start again with the next play)
    impl_->particles_model();
    impl_->seq.running = false;
    impl_->aseq.running = false;
    request_frame();
    if (impl_->cut) render();
}
bool Viewer::particles() const { return impl_ && impl_->particles_on; }

// In-game lighting (2026-11-19, release step 2; snatcherLeo: an "in-game lighting" toggle): the game's own per-vertex lights
// (kArenaLights) and TEV order in place of Studio's single studio light. Off: Studio's look, unchanged.
void Viewer::set_battle_camera(bool on) {
    if (!impl_ || impl_->cam_on == on) return;
    impl_->cam_on = on;
    request_frame();
    if (impl_->cut) render();
}
void Viewer::set_game_lighting(bool on) {
    if (!impl_ || impl_->game_light == on) return;
    impl_->game_light = on;
    request_frame();
    if (impl_->cut) render();
}
bool Viewer::game_lighting() const { return impl_ && impl_->game_light; }
bool Viewer::has_particles() const {
    if (!impl_ || !impl_->has_model) return false;
    if (impl_->model.particles) return true;
    // (a glow card counts: the switch hides it too)
    for (int i = 0; i < impl_->model.mesh_count; ++i)
        if (impl_->model.meshes[i].glow) return true;
    return false;
}
void Viewer::set_particle_warmup(int frames) {
    if (impl_) impl_->warm = std::max(0, frames);
}

// A camera copied from the game: its view matrix (3 rows of 4, as the [waza_viewer] log prints it), its vertical field
// of view (degrees) and aspect, and the model's position and rotation (radians, applied X then Y then Z as HSD's joints
// do; only Y is used in the move viewer's shots). For comparing Studio's drawing with the game's pictures (the project's
// move-viewer shots, 2026-11-19); the mouse no longer turns the view while it is set.
void Viewer::set_game_camera(const float view_rows[12], float fov_deg, float aspect, const float pos[3], const float rot[3]) {
    if (!impl_) return;
    Impl& I = *impl_;
    Mat4 v;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c) v.m[c * 4 + r] = view_rows[4 * r + c];
    Mat4 rz;
    rz.m[0] = std::cos(rot[2]); rz.m[1] = std::sin(rot[2]); rz.m[4] = -std::sin(rot[2]); rz.m[5] = std::cos(rot[2]);
    I.game_view = v;
    I.game_model = mul(translate(pos[0], pos[1], pos[2]), mul(rz, mul(rot_y(rot[1]), rot_x(rot[0]))));
    I.game_fov = fov_deg * 3.14159265f / 180.0f;
    I.game_aspect = aspect;
    I.game_cam = true;
    request_frame();
}
// One frame of the game's battle camera (its eye, interest and fov in degrees, in the battle's space, as the game's
// ORRE_BCAM_LOG prints them), held while a move plays: the move sheets (studio/tests/movebatch.py) draw each moment
// from the game's own camera, so the effects can be compared shape for shape. Screenshot tests only.
void Viewer::set_battle_camera_shot(const float eye[3], const float interest[3], float fov_deg) {
    if (!impl_) return;
    Impl& I = *impl_;
    for (int c = 0; c < 3; ++c) { I.shot_eye[c] = eye[c]; I.shot_at[c] = interest[c]; }
    I.shot_fov = fov_deg;
    I.shot_cam = true;
    request_frame();
}

// A resize (the Overview's card and the Moves tab's preview have different sizes) leaves the window without a picture
// of the new size until the next frame, which showed as a white flash on every switch between the two tabs (snatcherLeo,
// 2026-11-19: "it's happening again for the Moves tab"). So the view is cut to nothing while it is resized, a frame
// is drawn at the new size at once, and the page's window region (place_children) shows it again with that picture.
// Windows only (window regions); Linux / macOS: resize the toolkit's view and render before the next compositor frame.
void Viewer::move(int x, int y, int w, int h) {
    w = std::max(1, w);
    h = std::max(1, h);
    // already there: nothing to do (*added 2026-11-22*, with place_children's region: a pick of another Pokémon no
    // longer moves and repaints the view's window for nothing; see the white-flash note there)
    RECT wr{};
    GetWindowRect(hwnd_, &wr);
    MapWindowPoints(nullptr, GetParent(hwnd_), reinterpret_cast<POINT*>(&wr), 2);
    if (wr.left == x && wr.top == y && wr.right - wr.left == w && wr.bottom - wr.top == h) return;
    RECT r{};
    GetClientRect(hwnd_, &r);
    const bool resized = r.right != w || r.bottom != h;
    if (resized && impl_ && !impl_->cut && impl_->framed) SetWindowRgn(hwnd_, CreateRectRgn(0, 0, 0, 0), FALSE);
    MoveWindow(hwnd_, x, y, w, h, resized ? FALSE : TRUE);
    // (*Tried and taken out, 2026-11-22:* a DwmFlush here, waiting for Windows' next composition before the view is shown
    // again, did not stop snatcherLeo's rare white flash on resizes: Pikachu, Dewgong and Swablu flashed with it in. Every flash
    // so far came on a pick that shrank the Overview's model card, so the view's surface was set up again; the likely
    // cause is the GPU's overlay (MPO) path on the NVIDIA driver, outside what Studio draws or a screen read sees.)
    if (resized && impl_ && impl_->framed) render();
}

void Viewer::set_spin(bool on) {
    spin_ = on;
    update_timer();
}

void Viewer::set_yaw(float yaw) {
    if (impl_) impl_->yaw = yaw;
    request_frame();
}

void Viewer::reset_view() {
    if (!impl_) return;
    impl_->yaw = 0.6f;
    impl_->pitch = 0.15f;
    impl_->dist = 1.0f;
    impl_->plain_yaw = impl_->plain_pitch = 0;
    request_frame();
}
// turned or zoomed away from where reset_view puts it (the Overview's ↺ button is dimmed while not; 2026-11-22). The
// spin doesn't count: it turns the model by itself
bool Viewer::view_moved() const {
    if (!impl_) return false;
    const Impl& I = *impl_;
    if (spin_) return false;
    return std::fabs(I.yaw - 0.6f) > 1e-3f || std::fabs(I.pitch - 0.15f) > 1e-3f || std::fabs(I.dist - 1.0f) > 1e-3f ||
           std::fabs(I.plain_yaw) > 1e-3f || std::fabs(I.plain_pitch) > 1e-3f;
}

void Viewer::walk_into_hold(int frames) {
    if (!ok_ || !impl_ || hold_ < 0) return;
    if (impl_->screen_blur(hold_) <= 0) return;  // (no screen blur there: the shot stays a plain held frame)
    for (int k = std::min(frames, hold_); k >= 1; --k) {
        hold_walk_ = hold_ - k;
        render();
    }
    hold_walk_ = hold_;  // (the next render: the held frame, its last picture the one just before)
}

// The frame times for the performance log (perf_tick in log.cpp): counted across every window's view, on the UI thread
// that draws them. QueryPerformanceCounter is Windows'; Linux / macOS: std::chrono::steady_clock does the same
namespace {
int s_frames = 0;
double s_frame_ms_sum = 0, s_frame_ms_worst = 0;
struct FrameTimer {
    LARGE_INTEGER t0;
    FrameTimer() { QueryPerformanceCounter(&t0); }
    ~FrameTimer() {
        LARGE_INTEGER t1, f;
        QueryPerformanceCounter(&t1);
        QueryPerformanceFrequency(&f);
        const double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart;
        ++s_frames;
        s_frame_ms_sum += ms;
        s_frame_ms_worst = std::max(s_frame_ms_worst, ms);
    }
};
}  // namespace
void viewer_frame_stats(int& frames, double& avg_ms, double& worst_ms) {
    frames = s_frames;
    avg_ms = s_frames ? s_frame_ms_sum / s_frames : 0;
    worst_ms = s_frame_ms_worst;
    s_frames = 0;
    s_frame_ms_sum = s_frame_ms_worst = 0;
}

void Viewer::render() {
    if (!ok_ || !impl_) return;
    Impl& I = *impl_;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    const FrameTimer frame_timer;  // (this frame's time, for the performance log)
    if (rc.right != I.w || rc.bottom != I.h) {
        logf(Level::Info, "surface", "set up at %dx%d (was %dx%d)", (int)rc.right, (int)rc.bottom, I.w, I.h);  // (see below)
        I.configure(rc.right, rc.bottom);
    }
    // the pose, the particles and the battle camera for this moment first (a repaint before the first timer tick already
    // shows the idle pose), so the camera below is the frame's own
    advance_idle();
    // camera: the model centred, turned by yaw / pitch, far enough that its bounding sphere fits the view's narrower
    // side (a tall, narrow card would otherwise cut its sides), times the zoom
    const float r = I.radius;
    const float aspect = (float)I.w / I.h, half = std::tan(0.7f / 2);
    const float fit = r / (half * std::min(1.0f, aspect)) * 0.92f;
    const Mat4 model = mul(rot_x(I.pitch), rot_y(I.yaw));
    Mat4 mv = mul(translate(0, 0, -fit * I.dist), mul(model, translate(-I.centre[0], -I.centre[1], -I.centre[2])));
    // The far plane while the arena shows: 32768, the game's own for its map cameras (the game-camera path below).
    // *Changed 2026-11-22* (snatcherLeo, on the valley: "Isn't there a skybox? Can we add the sky from whatever it is
    // in-game"): the valley's map has its sky built in, two model parts out to about 10,000 units (D2_valley_1_bf
    // entry 2, meshes 19-20: a dome from 2,737 to 7,243 units high and its lower band; fsysmodel in the CLI shows
    // them), as rooms do (floor-resources.md, "the sky is a model set"). The far planes here (4,000 for the battle
    // camera, 6,000 for the arena's) cut it off, so the frame's grey clear colour showed instead. Depth precision at
    // the Pokémon's distances stays well under a tenth of a unit (Depth24Plus)
    const float arena_far = I.arena_on && I.arena ? 32768.0f : 0.0f;
    Mat4 mvp = mul(perspective(0.7f, (float)I.w / I.h, r * 0.05f, std::max(r * 20.0f, arena_far)), mv);
    float cam_fov = 0.7f, cam_aspect = aspect;  // (the particles' trails need the projection's scale)
    float cam_near = r * 0.05f;                 // (the billboards sit just past it)
    if (I.game_cam) {
        // the game's own camera and model placement; its projection's aspect is kept even when this window's shape
        // differs, so the picture is the game's frame stretched to the window (scaled back to 640x480 to compare).
        // Near is at least 1 (the game's 0.1 with a far of 32768 leaves the depth test little precision)
        mv = mul(I.game_view, I.game_model);
        mvp = mul(perspective(I.game_fov, I.game_aspect, 1.0f, 32768.0f), mv);
        cam_fov = I.game_fov;
        cam_near = 1.0f;
        cam_aspect = I.game_aspect;
    } else if ((I.shot_cam && I.once >= 0) || (I.cam_on && I.once >= 0 && I.cam_live && orre_bcam_get(I.bcam, nullptr, nullptr, nullptr))) {
        // the battle camera (release step 4): its eye and interest are in the battle's space; the model's space the
        // shaders draw is (-z, y, x + 40) of it (to_battle's inverse), and up stays +y (the layout only turns about y)
        float be[3], bi[3], fovd = 45.0f;
        if (I.shot_cam) {
            for (int c = 0; c < 3; ++c) { be[c] = I.shot_eye[c]; bi[c] = I.shot_at[c]; }
            fovd = I.shot_fov;
        } else {
            orre_bcam_get(I.bcam, be, bi, &fovd);
        }
        const float eye[3] = {-be[2], be[1], be[0] - I.kUserX}, at[3] = {-bi[2], bi[1], bi[0] - I.kUserX};
        float f[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
        const float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        if (fl > 1e-4f) {
            for (float& c : f) c /= fl;
            float s[3] = {-f[2], 0, f[0]};  // f x (0, 1, 0)
            const float sl = std::max(1e-6f, std::sqrt(s[0] * s[0] + s[2] * s[2]));
            s[0] /= sl; s[2] /= sl;
            const float u2[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
            Mat4 v;
            for (int c = 0; c < 3; ++c) { v.m[c * 4 + 0] = s[c]; v.m[c * 4 + 1] = u2[c]; v.m[c * 4 + 2] = -f[c]; }
            v.m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
            v.m[13] = -(u2[0] * eye[0] + u2[1] * eye[1] + u2[2] * eye[2]);
            v.m[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
            mv = v;
            cam_fov = fovd * 3.14159265f / 180.0f;
            // near 0.1, as the game's battle camera (the project's capture of Charm: P23 0.1, verified). *Corrected
            // 2026-11-21:* it was 1.0, which cut Hydro Cannon's beam where it passes the camera (a hard edge with the
            // tube's dark inside showing). The far plane stays 4000: at the arena's distances (under a few hundred
            // units) the depth buffer still resolves well under a tenth of a unit
            cam_near = 0.1f;
            mvp = mul(perspective(cam_fov, aspect, 0.1f, std::max(4000.0f, arena_far)), mv);  // (the arena's sky: above)
        }
    } else if (I.plain && !I.studio_cam) {
        // Plain stage: the move viewer's camera angle, a look-at along its eye -> interest direction with y up (as
        // C3D's camera builds its view: a right-handed look-at), and the model placed and turned as the viewer does.
        // The game's own distance (eye (-40, 44, 70), interest (-40, 14, 0)) shows every Pokémon tiny and low in the
        // frame (the project's shots: Espeon about a tenth of the picture's height), so the camera keeps that angle but aims at
        // the model's middle from the distance that fits it, as Studio view does; the wheel zooms (2026-11-19)
        // Dragging turns around the Pokémon from that angle (snatcherLeo, 2026-11-19: "are we able to navigate the
        // preview/rotate so we can better see how the Pokemon uses the move?"): the game's view direction (looking
        // down about 23 degrees, from +z) plus the drag's turn and tilt; a double-click goes back to the game's angle
        const float e0 = std::asin(30.0f / std::sqrt(30.0f * 30.0f + 70.0f * 70.0f));
        const float el = std::clamp(e0 + I.plain_pitch, -1.2f, 1.45f), az = 3.14159265f + I.plain_yaw;
        float f[3] = {std::cos(el) * std::sin(az), -std::sin(el), std::cos(el) * std::cos(az)};
        // the model's middle in the viewer's world: turned +90 degrees about y ((x, z) -> (z, -x)), then moved to x -40
        const float at[3] = {I.centre[2] - 40, I.centre[1], -I.centre[0]};
        const float back = r / (std::tan(30.0f * 3.14159265f / 180.0f) * std::min(1.0f, aspect)) * 0.92f * I.dist;
        const float eye[3] = {at[0] - f[0] * back, at[1] - f[1] * back, at[2] - f[2] * back};
        float s[3] = {-f[2], 0, f[0]};  // f x (0, 1, 0)
        const float sl = std::sqrt(s[0] * s[0] + s[2] * s[2]);
        s[0] /= sl; s[2] /= sl;
        const float u2[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
        Mat4 v;
        for (int c = 0; c < 3; ++c) { v.m[c * 4 + 0] = s[c]; v.m[c * 4 + 1] = u2[c]; v.m[c * 4 + 2] = -f[c]; }
        v.m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
        v.m[13] = -(u2[0] * eye[0] + u2[1] * eye[1] + u2[2] * eye[2]);
        v.m[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
        mv = mul(v, mul(translate(-40, 0, 0), rot_y(3.14159265f / 2)));
        // (with the arena, far enough for the whole room and its sky: arena_far, above; it was 6,000, enough for the
        // Phenac room's 2,400 units but not the valley's sky)
        mvp = mul(perspective(60.0f * 3.14159265f / 180.0f, aspect, r * 0.05f, I.arena_on && I.arena ? arena_far : r * 20.0f + back), mv);
        cam_fov = 60.0f * 3.14159265f / 180.0f;
    }
    // (the extra models' billboards use this view on the next frame)
    for (int r2 = 0; r2 < 3; ++r2)
        for (int c = 0; c < 4; ++c) I.last_view[4 * r2 + c] = mv.m[c * 4 + r2];
    I.have_view = true;
    // billboard parts face the camera, so a model with any is posed again whenever the view turns (as the game
    // rebuilds their matrices every frame); the view goes to the core as 3 rows of 4 (it looks down -Z, as GX's does)
    if (I.has_billboards) {
        float v[12];
        for (int r2 = 0; r2 < 3; ++r2)
            for (int c = 0; c < 4; ++c) v[4 * r2 + c] = mv.m[c * 4 + r2];
        if (!std::equal(v, v + 12, I.view)) {
            std::copy(v, v + 12, I.view);
            I.anim_frame = -1;  // the idle loop re-poses on its next step
            if (!(idle_ && has_idle()) && I.once < 0) {
                std::vector<OrreVertex*> out(I.model.mesh_count);
                for (int i = 0; i < I.model.mesh_count; ++i) out[i] = I.posed[i].data();
                if (orre_model_pose_view(&I.model, -1, 0, I.view, out.data()))
                    for (auto& p : I.parts) I.queue.WriteBuffer(p.vbuf, 0, I.posed[p.mesh].data(), (uint64_t)p.count * sizeof(OrreVertex));
            }
        }
    }
    wgpu::SurfaceTexture st;
    if (I.page) {
        // page mode: our own texture, shown by the page (share_tex), never the window's surface
        if (!I.offscreen) I.configure(rc.right, rc.bottom);
        st.texture = I.offscreen;
        st.status = wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal;
    } else {
        I.surface.GetCurrentTexture(&st);
    }
    if (st.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        st.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
        // (always logged, 2026-11-22: a rebuilt surface can show one blank frame; a suspect for snatcherLeo's rare white flash)
        logf(Level::Info, "surface", "no frame to draw into (status %d): the surface is set up again at %dx%d", (int)st.status,
             (int)rc.right, (int)rc.bottom);
        I.configure(rc.right, rc.bottom);
        return;
    }
    if (st.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
        static int suboptimal = 0;
        if (++suboptimal <= 20) logf(Level::Info, "surface", "frame %dx%d is suboptimal (the surface wants setting up again)", I.w, I.h);
    }
    float u[kCameraFloats] = {};
    std::copy(mvp.m, mvp.m + 16, u);
    // the normals' matrix: the model-view rotation (its move dropped), so normals are in view space for every camera.
    // *Corrected 2026-11-19:* this was Studio's turntable rotation alone, right for Studio's own camera but not for the
    // game's camera or Plain stage, which turned the reflection textures wrongly there (the all-models check: Silcoon,
    // Steelix, Skarmory and the other shiny-metal Pokémon among the furthest from the game's pictures)
    Mat4 nrm = mv;
    nrm.m[12] = nrm.m[13] = nrm.m[14] = 0;
    std::copy(nrm.m, nrm.m + 16, u + 16);
    // a move's colour fades this frame (release step 4): the stage's (target 1) on the backdrop, the user's own
    // (target 2) as the model's tint, the box's on its faces, the full-screen one (target 0) as a quad after all
    uint8_t fade[3][4] = {};
    bool fade_on[3] = {};
    I.fades_at(I.box_frame, fade, fade_on, I.box_fade, &I.box_fade_on);
    float hi[4], lo[4];
    std::copy(I.hi, I.hi + 4, hi);
    std::copy(I.lo, I.lo + 4, lo);
    if (fade_on[1])
        for (int k = 0; k < 3; ++k) {
            hi[k] = std::min(1.0f, hi[k] * 2.0f * fade[1][k] / 255.0f);
            lo[k] = std::min(1.0f, lo[k] * 2.0f * fade[1][k] / 255.0f);
        }
    std::copy(hi, hi + 4, u + 32);
    std::copy(lo, lo + 4, u + 36);
    u[40] = (float)I.w; u[41] = (float)I.h; u[42] = u[43] = 0;
    for (int k = 0; k < 4; ++k) u[kTintAt + k] = fade_on[2] ? fade[2][k] / 255.0f : 0.5f;
    for (int k = 0; k < 4; ++k) u[kGhostAt + k] = 1.0f;  // (drawn whole; the after-images' copies set their own)
    I.screen_fade_on = fade_on[0];
    std::copy(fade[0], fade[0] + 4, I.screen_fade);
    if (I.game_light) {
        // the model in the arena's world: the game camera's logged placement, or the move viewer's own (x -40, turned
        // +90 degrees about y), whichever camera Studio shows it with; the lights stay put while Studio's own camera
        // turns around the Pokémon, as the game's orbit camera does
        const Mat4 arena = I.game_cam ? I.game_model : mul(translate(-40, 0, 0), rot_y(3.14159265f / 2));
        std::copy(arena.m, arena.m + 16, u + 44);
        // the eye in the model's space (the model-view matrix is a rotation and a move: its inverse is the transposed
        // rotation applied to the negated move), then in the arena's world
        float em[3];
        for (int i = 0; i < 3; ++i) em[i] = -(mv.m[i * 4] * mv.m[12] + mv.m[i * 4 + 1] * mv.m[13] + mv.m[i * 4 + 2] * mv.m[14]);
        for (int r = 0; r < 3; ++r) u[60 + r] = arena.m[r] * em[0] + arena.m[4 + r] * em[1] + arena.m[8 + r] * em[2] + arena.m[12 + r];
        u[63] = 1;
        for (int k = 0; k < 3; ++k) u[64 + k] = kArenaAmbient[k] / 255.0f;
        int n = 0;
        for (const ArenaLight& L : kArenaLights) {
            if (n >= 8) break;
            float* p = u + 68 + 4 * n;
            float* c = u + 68 + 32 + 4 * n;
            float* k = u + 68 + 64 + 4 * n;
            p[0] = L.pos[0]; p[1] = L.pos[1]; p[2] = L.pos[2]; p[3] = (float)L.kind;
            c[0] = L.rgb[0] / 255.0f; c[1] = L.rgb[1] / 255.0f; c[2] = L.rgb[2] / 255.0f; c[3] = (float)L.flags;
            k[0] = L.k[0]; k[1] = L.k[1]; k[2] = L.k[2];
            ++n;
        }
        // the highlight texgen's half vector (HSD: the first infinite light in the list; with no interest its direction
        // is from its position toward the origin): the direction to the light, arena -> model (the arena rotation's
        // transpose) -> view (the model-view rotation), plus the view's +Z (toward the eye), normalised
        for (const ArenaLight& L : kArenaLights) {
            if (L.kind != 1) continue;
            const float len = std::sqrt(L.pos[0] * L.pos[0] + L.pos[1] * L.pos[1] + L.pos[2] * L.pos[2]);
            float tl[3] = {L.pos[0] / len, L.pos[1] / len, L.pos[2] / len}, tm[3], tv[3];
            for (int c = 0; c < 3; ++c) tm[c] = arena.m[c * 4] * tl[0] + arena.m[c * 4 + 1] * tl[1] + arena.m[c * 4 + 2] * tl[2];
            for (int r = 0; r < 3; ++r) tv[r] = mv.m[r] * tm[0] + mv.m[4 + r] * tm[1] + mv.m[8 + r] * tm[2];
            tv[2] += 1.0f;
            const float hl = std::sqrt(tv[0] * tv[0] + tv[1] * tv[1] + tv[2] * tv[2]);
            if (hl > 1e-6f) { u[164] = tv[0] / hl; u[165] = tv[1] / hl; u[166] = tv[2] / hl; u[167] = 1; }
            break;
        }
    }
    I.queue.WriteBuffer(I.uniforms, 0, u, sizeof u);
    // the after-images this frame: each copy posed at its animation frame into its own vertex buffers, with its own
    // copy of the camera uniforms carrying its alpha
    I.after_images(I.box_frame, I.anim_t);
    if (!I.ghost_now.empty() && I.has_model && I.anim_last >= 0 && !(I.user_hidden && I.once >= 0)) {
        const size_t n = std::min<size_t>(I.ghost_now.size(), 64);
        while (I.ghosts.size() < n) {
            Impl::Ghost gh;
            for (int i = 0; i < I.model.mesh_count; ++i) {
                wgpu::BufferDescriptor vb;
                vb.size = std::max<uint64_t>(16, (uint64_t)I.model.meshes[i].vertex_count * sizeof(OrreVertex));
                vb.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
                gh.vb.push_back(I.device.CreateBuffer(&vb));
            }
            wgpu::BufferDescriptor ub;
            ub.size = kCameraBytes;
            ub.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            gh.ub = I.device.CreateBuffer(&ub);
            wgpu::BindGroupEntry ce;
            ce.binding = 0;
            ce.buffer = gh.ub;
            ce.size = kCameraBytes;
            wgpu::BindGroupDescriptor cg;
            cg.layout = I.layout0;
            cg.entryCount = 1;
            cg.entries = &ce;
            gh.cg = I.device.CreateBindGroup(&cg);
            I.ghosts.push_back(std::move(gh));
        }
        std::vector<std::vector<OrreVertex>> buf((size_t)I.model.mesh_count);
        std::vector<OrreVertex*> out((size_t)I.model.mesh_count);
        for (int i = 0; i < I.model.mesh_count; ++i) { buf[(size_t)i].resize(I.model.meshes[i].vertex_count); out[(size_t)i] = buf[(size_t)i].data(); }
        for (size_t g = 0; g < n; ++g) {
            if (!orre_model_pose_view(&I.model, I.anim_last, I.ghost_now[g].first, I.has_billboards ? I.view : nullptr, out.data())) continue;
            for (int i = 0; i < I.model.mesh_count; ++i)
                if (I.model.meshes[i].vertex_count)
                    I.queue.WriteBuffer(I.ghosts[g].vb[(size_t)i], 0, buf[(size_t)i].data(), (uint64_t)I.model.meshes[i].vertex_count * sizeof(OrreVertex));
            u[kGhostAt] = I.ghost_now[g].second;
            I.queue.WriteBuffer(I.ghosts[g].ub, 0, u, sizeof u);
        }
        u[kGhostAt] = 1.0f;
        I.ghost_now.resize(n);
    } else {
        I.ghost_now.clear();
    }
    // the user's fade alpha (user_alpha): the fade colour's alpha byte / 255 (0xFF = whole, as the modulation stage
    // scales alpha by 1; 0xFF also when the fade has no alpha flag). *Corrected 2026-11-21:* was the byte x 2
    I.user_alpha = fade_on[2] ? std::clamp(fade[2][3] / 255.0f, 0.0f, 1.0f) : 1.0f;
    if (I.user_alpha < 0.999f) {
        if (!I.fade_ub) {
            wgpu::BufferDescriptor ub;
            ub.size = kCameraBytes;
            ub.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            I.fade_ub = I.device.CreateBuffer(&ub);
            wgpu::BindGroupEntry ce;
            ce.binding = 0;
            ce.buffer = I.fade_ub;
            ce.size = kCameraBytes;
            wgpu::BindGroupDescriptor cg;
            cg.layout = I.layout0;
            cg.entryCount = 1;
            cg.entries = &ce;
            I.fade_cg = I.device.CreateBindGroup(&cg);
        }
        u[kGhostAt] = I.user_alpha;
        I.queue.WriteBuffer(I.fade_ub, 0, u, sizeof u);
        u[kGhostAt] = 1.0f;
    }
    // the billboards running this frame, posed in front of the camera (drawn in the second pass)
    I.billboard_now.clear();
    if (I.once >= 0 && I.box_frame >= 0) {
        const float right[3] = {mv.m[0], mv.m[4], mv.m[8]}, up[3] = {mv.m[1], mv.m[5], mv.m[9]}, back[3] = {mv.m[2], mv.m[6], mv.m[10]};
        float eye[3];
        for (int c = 0; c < 3; ++c) eye[c] = -(mv.m[c * 4] * mv.m[12] + mv.m[c * 4 + 1] * mv.m[13] + mv.m[c * 4 + 2] * mv.m[14]);
        for (Impl::MoveSeq* S : {&I.seq, &I.tseq, &I.aseq}) {
            if (!S->wzx || !S->running || S->anim != I.once || I.box_frame < S->first_frame) continue;
            // (the user's half ends when the target's starts, its billboard with it: the game's Sandstorm shows only
            // the target half's own, fading in, after the cut; move sweep, 2026-11-20)
            // (*updated 2026-11-20:* from the target half's start, as the fades: fades_at)
            if (S == &I.seq && I.tseq.wzx && I.box_on && I.box_frame >= I.tseq.first_frame) continue;
            for (size_t e = 0; e < S->start.size(); ++e) {
                OrreWzxBillboard bb{};
                if (!orre_wzx_billboard(S->wzx, (int)e, &bb)) continue;
                const int sf = S->first_frame + (S->start[e] + 1) / 2;
                if (I.box_frame < sf) continue;
                const auto key = std::make_pair((const void*)S->wzx, (int)e);
                auto it = I.billboard_inst.find(key);
                if (it == I.billboard_inst.end()) {
                    auto in = std::make_unique<Impl::Inst>();
                    if (orre_wzx_model(S->wzx, (int)e, &in->model)) I.inst_build(*in);
                    it = I.billboard_inst.emplace(key, std::move(in)).first;
                }
                Impl::Inst& in = *it->second;
                if (in.model.mesh_count == 0) continue;
                const int anim = bb.anim >= 0 && bb.anim < in.model.animation_count ? bb.anim : -1;
                const float len = anim >= 0 ? std::max(1.0f, std::floor(orre_model_animation_length(&in.model, anim))) : 1.0f;
                float k = (float)(I.box_frame - sf);
                if (anim >= 0 && !bb.loop && k >= len) continue;  // (its animation has ended: the effect stops)
                if (anim >= 0 && bb.loop) k = std::fmod(k, len);
                const float d = 1.1f * cam_near;
                const float bx = std::max(1e-4f, in.model.bounds_max[0] - in.model.bounds_min[0]);
                const float by = std::max(1e-4f, in.model.bounds_max[1] - in.model.bounds_min[1]);
                const float sy = 2.0f * d * std::tan(cam_fov * 0.5f) / by, sx = 2.0f * d * std::tan(cam_fov * 0.5f) * cam_aspect / bx;
                float M[12];
                for (int c = 0; c < 3; ++c) {
                    M[3 * c + 0] = right[c] * sx;
                    M[3 * c + 1] = up[c] * sy;
                    M[3 * c + 2] = 0;
                    M[9 + c] = eye[c] - back[c] * d;
                }
                I.inst_pose(in, anim, anim >= 0 ? k : 0.0f, M, false);
                I.billboard_now.push_back(&in);
            }
        }
    }
    // the leaves flying this frame: each started leaves entry run from its start to this frame (Studio's random
    // numbers from the Play's seed, so a held frame shows the same flight), each copy posed by its turn, size and place
    I.leaves_now.clear();
    // The game's model pool: GSmodelInit(0x40) gives 64 models in all, and each leaf copy is a clone
    // (_leaffxGenerateLeafData: GSmodelClone, "Could not create leaf clone model" when none is left; the failed copy
    // draws no random numbers and is never drawn). Verified by the project's port log, 2026-11-21: Feather Dance's target
    // half has three leaf entries of 32, and they got 32, 22 and 0 copies (42 failures), so 54 were free there. Studio
    // gives the live leaf entries of a frame that many copies in all, in entry order (likely the same in the user's
    // half, where the other models in use are the same; not measured there)
    constexpr int kLeafPool = 54;
    int leaf_pool = kLeafPool;
    if (I.once >= 0 && I.box_frame >= 0) {
        for (Impl::MoveSeq* S : {&I.seq, &I.tseq, &I.aseq}) {
            if (!S->wzx || !S->running || S->anim != I.once || I.box_frame < S->first_frame) continue;
            if (S == &I.seq && I.tseq.wzx && I.box_on && I.box_frame >= I.tseq.first_frame) continue;  // (its half has ended)
            for (size_t e = 0; e < S->start.size(); ++e) {
                OrreWzxLeaves lp{};
                if (!orre_wzx_leaves(S->wzx, (int)e, &lp)) continue;
                const int sf = S->first_frame + (S->start[e] + 1) / 2;  // (the first frame whose tick reaches its start)
                if (I.box_frame < sf) continue;
                // (the copies the pool still has: the effect flies only those, as the game's failed clones are skipped)
                const int got = std::min(std::min(32, lp.count), leaf_pool);
                leaf_pool -= got;
                if (got <= 0) continue;
                lp.count = got;
                // where they start: the owner's attach point lp.attach through its slot's joints (wazaSequenceEntry's
                // case 5: table->field_4C[...]), read once when the effect starts (_leaffxGenerateLeafData); the box:
                // its centre; no joint: the owner's place
                float origin[3] = {S->box ? I.kTargetX : I.kUserX, 0, 0}, r3[3], s3[3];
                if (S->box && !I.has_target()) {
                    I.box_joint(Impl::kBoxCentre, I.box_pose(sf), origin, r3);
                } else if (lp.attach >= 0 && lp.attach < 16) {
                    const OrreModel& om = S->box ? I.target->model : I.model;
                    const int j = om.slot_points[S->slot][lp.attach];
                    if (j >= 0 && j < om.joint_count) I.owner_joint(S->box, j, sf, origin, r3, s3);
                }
                OrreLeaves* lv = orre_leaves_create(&lp, origin, S->scale, I.cam_seed ^ (uint32_t)(e * 2246822519u) ^ (S->box ? 0x77u : 0u));
                bool live = lv != nullptr;
                for (int k = 0; live && k <= I.box_frame - sf; ++k) live = orre_leaves_update(lv, 2) != 0;
                float xf[32 * 12], alpha = 1;
                const int n = live ? orre_leaves_get(lv, xf, &alpha) : 0;
                orre_leaves_free(lv);
                if (n <= 0) continue;
                // the copies, made once per file and entry (all 32, as leaffxStartEffect clones them)
                const auto key = std::make_pair((const void*)S->wzx, (int)e);
                auto it = I.leaf_inst.find(key);
                if (it == I.leaf_inst.end()) {
                    std::vector<std::unique_ptr<Impl::Inst>> v;
                    for (int c = 0; c < lp.count; ++c) {
                        auto in = std::make_unique<Impl::Inst>();
                        if (!orre_wzx_model(S->wzx, (int)e, &in->model)) break;
                        I.inst_build(*in);
                        v.push_back(std::move(in));
                    }
                    it = I.leaf_inst.emplace(key, std::move(v)).first;
                }
                for (int c = 0; c < n && c < (int)it->second.size(); ++c) {
                    Impl::Inst& in = *it->second[(size_t)c];
                    if (in.model.mesh_count == 0) continue;
                    // the fade: GSmaterialSetAlpha on every material (HSD_MObjSetAlpha: the material's alpha replaced)
                    if (in.alpha_set != alpha) {
                        for (auto& p : in.parts) {
                            in.model.meshes[p.mesh].alpha = alpha;
                            p.group = I.material_group(&in.model.meshes[p.mesh], p.mbuf, nullptr, nullptr, nullptr, nullptr, &in.model, &in.textures);
                        }
                        in.alpha_set = alpha;
                    }
                    // battle space (R s | p) into the Pokémon's: (x, y, z) -> (-z, y, x - kUserX)
                    const float* b = xf + 12 * c;
                    float M[12];
                    for (int q = 0; q < 3; ++q) {
                        M[0 + q] = -b[6 + q];
                        M[3 + q] = b[3 + q];
                        M[6 + q] = b[0 + q];
                    }
                    M[9] = -b[11];
                    M[10] = b[10];
                    M[11] = b[9] - I.kUserX;
                    I.inst_pose(in, -1, 0.0f, M);
                    I.leaves_now.push_back(&in);
                }
            }
        }
    }
    // the env-map shine running this frame on the Pokémon (the user's half; no target half on the disc has one:
    // verified, a scan of all 355 moves), the latest started winning; its value from the keys, 2 ticks a frame from
    // its start (envMapEffectStart shows the first key's "from")
    I.env_now = false;
    if (I.once >= 0 && I.box_frame >= 0 && I.seq.wzx && I.seq.running && I.seq.anim == I.once && I.has_model) {
        const Impl::MoveSeq& S = I.seq;
        int best = -1;
        for (size_t e = 0; e < S.start.size(); ++e) {
            OrreWzxEnvMap em{};
            if (!orre_wzx_envmap(S.wzx, (int)e, &em)) continue;
            const int sf = S.first_frame + (S.start[e] + 1) / 2;
            if (I.box_frame < sf || S.start[e] < best) continue;
            float v = 0;
            if (!orre_wzx_envmap_value(&em, 2 * (I.box_frame - sf), &v)) continue;
            best = S.start[e];
            I.env_em = em;
            I.env_value = v;
            I.env_now = true;
        }
        if (I.tseq.wzx && I.box_on && I.box_frame >= I.tseq.first_frame) I.env_now = false;  // (its half has ended)
    }
    if (!I.env_now) I.env_built = -1;
    // the screen warps running this frame (_distortionUpdate, verified from the code): at progress p = elapsed / its
    // length, the half-size is size x length x p / peak up to the peak, x (1 - p) / (1 - peak) after it (peak 0:
    // shrinking from the start); the strength is strength x (1 - p); the place is the owner's centre (its model
    // record's +0x54 part; here its bounding box's centre: likely close), read at the start, or every frame when it
    // follows. Needs the picture copied mid-frame (can_copy)
    I.warps_now.clear();
    if (I.once >= 0 && I.box_frame >= 0 && I.can_copy) {
        for (Impl::MoveSeq* S : {&I.seq, &I.tseq, &I.aseq}) {
            if (!S->wzx || !S->running || S->anim != I.once || I.box_frame < S->first_frame) continue;
            if (S == &I.seq && I.tseq.wzx && I.box_on && I.box_frame >= I.tseq.first_frame) continue;  // (its half has ended)
            for (size_t e = 0; e < S->start.size(); ++e) {
                OrreWzxWarp wp{};
                if (!orre_wzx_warp(S->wzx, (int)e, &wp) || !wp.tex_rgba || wp.frames <= 0) continue;
                const int sf = S->first_frame + (S->start[e] + 1) / 2;
                if (I.box_frame < sf) continue;
                const int tk = 2 * (I.box_frame - sf);
                if (tk >= wp.frames) continue;
                const float pr = (float)tk / (float)wp.frames;
                float half = wp.size * (float)wp.frames;
                if (wp.peak != 0.0f && pr <= wp.peak) half *= pr / wp.peak;
                else half *= (1.0f - pr) / (1.0f - wp.peak);
                Impl::WarpNow wn{};
                wn.half = half;
                wn.strength = wp.strength * (1.0f - pr);
                const int at = wp.follow ? I.box_frame : sf;
                float p[3] = {S->box ? I.kTargetX : I.kUserX, 0, 0}, r3[3], s3[3];
                if (S->box && !I.has_target()) I.box_joint(Impl::kBoxCentre, I.box_pose(at), p, r3);
                else I.owner_joint(S->box, -2, at, p, r3, s3);
                wn.c[0] = -p[2];
                wn.c[1] = p[1];
                wn.c[2] = p[0] - I.kUserX;
                const auto key = std::make_pair((const void*)S->wzx, (int)e);
                auto it = I.warp_pics.find(key);
                if (it == I.warp_pics.end()) it = I.warp_pics.emplace(key, I.make_texture(wp.tex_width, wp.tex_height, wp.tex_rgba)).first;
                wn.pic = it->second;
                I.warps_now.push_back(wn);
            }
        }
    }
    if (I.arena_on && I.arena) {
        // the arena's copy: the stage's fade (target 1) as its tint (0.5 = none), as the game's filter tints the room
        for (int k = 0; k < 4; ++k) u[kTintAt + k] = fade_on[1] ? fade[1][k] / 255.0f : 0.5f;
        I.queue.WriteBuffer(I.uniforms_stage, 0, u, sizeof u);
        // placed in the Pokémon's space: battle (x, y, z) -> (-z, y, x - kUserX), as the target is
        if (I.arena_placed != -I.kUserX) {
            const float M[12] = {0, 0, -1, 0, 1, 0, 1, 0, 0, 0, 0, -I.kUserX};
            I.inst_pose(*I.arena, -1, 0, M);
            I.arena_placed = -I.kUserX;
        }
    }
    if (I.has_target()) {
        // the target's copy: its half's own colour fade (box_fade) as its tint (0.5 = none). Its alpha isn't used
        // here (no target-half fade with alpha seen yet; the user's goes through user_alpha)
        for (int k = 0; k < 4; ++k) u[kTintAt + k] = I.box_fade_on ? I.box_fade[k] / 255.0f : 0.5f;
        I.queue.WriteBuffer(I.uniforms_target, 0, u, sizeof u);
    }

    wgpu::RenderPassColorAttachment ca;
    ca.view = st.texture.CreateView();
    ca.loadOp = wgpu::LoadOp::Clear;
    ca.storeOp = wgpu::StoreOp::Store;
    ca.clearValue = {I.lo[0], I.lo[1], I.lo[2], 1};
    wgpu::RenderPassDepthStencilAttachment da;
    da.view = I.depth.CreateView();
    da.depthLoadOp = wgpu::LoadOp::Clear;
    da.depthStoreOp = wgpu::StoreOp::Store;
    da.depthClearValue = 1.0f;
    wgpu::RenderPassDescriptor rp;
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &ca;
    rp.depthStencilAttachment = &da;
    // the particles' quads for this camera (in the model's space, like the meshes: the view given to the core is the
    // model-view matrix), two triangles each, uploaded before the pass; runs of one blend mode, depth test and picture
    // become one draw each, in the game's drawing order
    // (pass: 0 drawn with the models, 1 after the depth reset: link-1 particles, the overlay box, the screen fade)
    struct PRun { uint32_t first, count, blend; bool noz; wgpu::BindGroup tex; bool solid = false; int pass = 0; };
    // the two passes' owners this frame (GSgfxDrawScene, the project's capture of Fire Punch, 2026-11-20)
    const bool user_overlay = I.owner_overlay(I.seq, I.box_frame, 0) && !(I.tseq.wzx && I.box_on && I.box_frame >= I.tshot);
    const bool target_overlay = I.owner_overlay(I.tseq, I.box_frame, I.tshot);
    std::vector<PRun> pruns;
    int blur_now = 0;  // a screen blur runs this frame: the finished picture is kept for the next one
    if ((I.ps && orre_particles_count(I.ps) > 0) || I.box_on || I.screen_fade_on || I.once >= 0) {
        I.pverts.clear();
        // the target box first (opaque, depth written): its six faces, flat-shaded by which way each faces in its own
        // space (top brightest, its front next, the sides, the back, the bottom darkest), whitened by the hit's flash
        if (I.box_on && !I.has_target() && !(I.box_hidden && I.once >= 0)) {
            const Impl::BoxPose bp = I.box_pose(I.box_frame);
            const float h = Impl::kBoxSize, w = h * 0.5f;
            struct Face { float n[3]; float shade; };
            static const Face kFaces[6] = {{{0, 1, 0}, 1.0f},   {{0, 0, 1}, 0.86f},  {{1, 0, 0}, 0.74f},
                                           {{-1, 0, 0}, 0.74f}, {{0, 0, -1}, 0.62f}, {{0, -1, 0}, 0.5f}};
            const float base[3] = {0.80f, 0.77f, 0.72f};  // a light warm grey: reads on the grey stage and the dark one
            const uint32_t first = (uint32_t)I.pverts.size();
            for (const Face& fc : kFaces) {
                // the face's four corners in its own space: centre +- the two axes across it
                float u[3] = {0, 0, 0}, v[3] = {0, 0, 0}, c[3] = {fc.n[0] * w, w + fc.n[1] * w, fc.n[2] * w};
                if (fc.n[1] != 0) { u[0] = w; v[2] = w; }
                else if (fc.n[0] != 0) { u[2] = w; v[1] = w; }
                else { u[0] = w; v[1] = w; }
                // wound counter-clockwise seen from outside (u x v along the face's normal), so the box's hidden faces
                // are culled (it isn't written to the depth buffer)
                const float cx = u[1] * v[2] - u[2] * v[1], cy = u[2] * v[0] - u[0] * v[2], cz = u[0] * v[1] - u[1] * v[0];
                if (cx * fc.n[0] + cy * fc.n[1] + cz * fc.n[2] < 0) std::swap(u, v);
                const float sgn[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                float pos[4][3];
                for (int k = 0; k < 4; ++k) {
                    const float l[3] = {c[0] + sgn[k][0] * u[0] + sgn[k][1] * v[0], c[1] + sgn[k][0] * u[1] + sgn[k][1] * v[1],
                                        c[2] + sgn[k][0] * u[2] + sgn[k][1] * v[2]};
                    float g[3];
                    I.box_point(bp, l, g);
                    pos[k][0] = -g[2];  // (to the model's space the shader draws: (-z, y, x + 40))
                    pos[k][1] = g[1];
                    pos[k][2] = g[0] - I.kUserX;
                }
                ParticleVertex pv{};
                for (int k = 0; k < 3; ++k) {
                    float col = base[k] * fc.shade + (1.0f - base[k] * fc.shade) * bp.flash * 0.85f;
                    if (I.box_fade_on) col *= 2.0f * I.box_fade[k] / 255.0f;  // (the target half's own colour fade)
                    pv.prim[k] = (uint8_t)std::lround(std::clamp(col, 0.0f, 1.0f) * 255.0f);
                }
                pv.prim[3] = 255;
                pv.kind = 0;
                pv.alpha = 0x3F;  // (alpha test: always)
                static const int kTri[6] = {0, 1, 2, 0, 2, 3};
                for (int k : kTri) {
                    std::copy(pos[k], pos[k] + 3, pv.pos);
                    I.pverts.push_back(pv);
                }
            }
            // (opaque but not written to the depth buffer: every particle of the hit draws over the box, as snatcherLeo asked,
            // 2026-11-19: "it cannot obstruct/block the particles from view, we need to see every hitting particle".
            // The target half's effects start at the box's centre, inside the cube, where a solid box hid them; its
            // faces stay correct because a convex box drawn far faces first needs no depth buffer)
            pruns.push_back({first, (uint32_t)I.pverts.size() - first, 0, false, I.pwhite, true, target_overlay ? 1 : 0});
        }
        // the model-view for the battle's space: a battle point g is the model's point (-g.z, g.y, g.x + 40) (to_battle)
        float m12[12], v12[12];
        for (int r2 = 0; r2 < 3; ++r2)
            for (int c = 0; c < 4; ++c) m12[4 * r2 + c] = mv.m[c * 4 + r2];
        for (int r2 = 0; r2 < 3; ++r2) {
            const float* m = m12 + 4 * r2;
            v12[4 * r2 + 0] = m[2];
            v12[4 * r2 + 1] = m[1];
            v12[4 * r2 + 2] = -m[0];
            v12[4 * r2 + 3] = m[2] * -I.kUserX + m[3];
        }
        const float f = 1.0f / std::tan(cam_fov / 2);
        const float proj4[4] = {f / cam_aspect, 0.0f, f, 0.0f};
        const int n = I.ps ? orre_particles_quads(I.ps, v12, proj4, nullptr, 0) : 0;
        I.quads.resize((size_t)n);
        if (n) orre_particles_quads(I.ps, v12, proj4, I.quads.data(), n);
        // ORRE_DEBUG_PTCL_KIND=<hex kind> (a debug switch, 2026-11-21): per move frame, the quads of that particle kind
        // in the battle's space (count, centre x / y / z range and mean height, edge length range), the same space as
        // the game's particle log (ORRE_WAZA_SWEEP_PARTICLES, its "centre"), so a move's particles can be compared
        // with the game's place by place (Mist's puffs first). Plain C++, every OS
        if (static const char* dk = std::getenv("ORRE_DEBUG_PTCL_KIND"); dk && I.box_frame >= 0) {
            const uint32_t want = (uint32_t)std::strtoul(dk, nullptr, 16);
            float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f}, ys = 0, elo = 1e30f, ehi = 0;
            int c = 0, alo = 255, ahi = 0, asum = 0;
            for (const OrreParticleQuad& q : I.quads) {
                if (q.kind != want) continue;
                ++c;
                alo = std::min(alo, (int)q.prim[3]);
                ahi = std::max(ahi, (int)q.prim[3]);
                asum += q.prim[3];
                for (int k = 0; k < 3; ++k) {
                    const float m = (q.pos[0][k] + q.pos[2][k]) * 0.5f;
                    lo[k] = std::min(lo[k], m);
                    hi[k] = std::max(hi[k], m);
                }
                ys += (q.pos[0][1] + q.pos[2][1]) * 0.5f;
                const float ex = q.pos[1][0] - q.pos[0][0], ey = q.pos[1][1] - q.pos[0][1], ez = q.pos[1][2] - q.pos[0][2];
                const float e = std::sqrt(ex * ex + ey * ey + ez * ez);
                elo = std::min(elo, e);
                ehi = std::max(ehi, e);
            }
            if (c)
                logf(Level::Info, "ptcl", "frame %d kind %08X: %d  x %.1f..%.1f  y %.1f..%.1f (mean %.1f)  z %.1f..%.1f  edge %.2f..%.2f  prim a %d..%d (mean %d)",
                     I.box_frame, want, c, lo[0], hi[0], lo[1], hi[1], ys / c, lo[2], hi[2], elo, ehi, alo, ahi, asum / c);
        }
        static const int kTri[6] = {0, 1, 2, 0, 2, 3};
        for (const OrreParticleQuad& q : I.quads) {
            ParticleVertex pv{};
            const uint32_t alpha = q.alpha_mode | (uint32_t)q.alpha_ref0 << 8 | (uint32_t)q.alpha_ref1 << 16;
            wgpu::BindGroup tex = I.pwhite;
            if (q.textured) {
                auto it = I.ptex.find((uint32_t)q.bank << 16 | (uint32_t)q.group << 8 | (uint32_t)q.image);
                if (it != I.ptex.end()) tex = it->second;
            }
            const uint32_t blend = (q.kind >> 22) & 3;
            const bool noz = (q.kind & 0x10000000u) != 0;
            const int qpass = q.link == 1 ? 1 : 0;  // (link 1: the second pass, after the depth reset)
            if (pruns.empty() || pruns.back().solid || pruns.back().blend != blend || pruns.back().noz != noz ||
                pruns.back().tex.Get() != tex.Get() || pruns.back().pass != qpass)
                pruns.push_back({(uint32_t)I.pverts.size(), 0, blend, noz, tex, false, qpass});
            for (int k : kTri) {
                // (back from the battle's space to the model's, which the shader's camera draws)
                pv.pos[0] = -q.pos[k][2];
                pv.pos[1] = q.pos[k][1];
                pv.pos[2] = q.pos[k][0] - I.kUserX;
                std::copy(q.uv[k], q.uv[k] + 2, pv.uv);
                std::copy(q.colour[k], q.colour[k] + 4, pv.ras);
                std::copy(q.prim, q.prim + 4, pv.prim);
                std::copy(q.env, q.env + 4, pv.env);
                std::copy(q.mat, q.mat + 4, pv.mat);
                pv.kind = q.kind;
                pv.alpha = alpha;
                I.pverts.push_back(pv);
            }
            pruns.back().count += 6;
        }
        // the bolts (screen effect kind 1: Thunder's lightning; core bolt.cpp), after the particles as the game's
        // effects render after the models: each started bolt entry run from its start to this frame (its random
        // numbers from the Play's seed, so a held frame shows the same strikes), its strips added to the picture
        if (I.once >= 0 && I.box_frame >= 0) {
            float em[3], eye_b[3];
            for (int i = 0; i < 3; ++i) em[i] = -(mv.m[i * 4] * mv.m[12] + mv.m[i * 4 + 1] * mv.m[13] + mv.m[i * 4 + 2] * mv.m[14]);
            eye_b[0] = em[2] + I.kUserX; eye_b[1] = em[1]; eye_b[2] = -em[0];
            for (Impl::MoveSeq* S : {&I.seq, &I.tseq, &I.aseq}) {
                if (!S->wzx || !S->running || S->anim != I.once || I.box_frame < S->first_frame) continue;
                for (size_t e = 0; e < S->start.size(); ++e) {
                    OrreWzxBolt bp{};
                    if (!orre_wzx_bolt(S->wzx, (int)e, &bp)) continue;
                    const int sf = S->first_frame + (S->start[e] + 1) / 2;  // (the first frame whose tick reaches its start)
                    if (I.box_frame < sf) continue;
                    OrreBolt* bo = orre_bolt_create(&bp, I.cam_seed ^ (uint32_t)(e * 2654435761u) ^ (S->box ? 0x55u : 0u));
                    bool live = bo != nullptr;
                    for (int k = 0; live && k <= I.box_frame - sf; ++k) live = orre_bolt_update(bo, 2) != 0;
                    // where the trunk lands: the owner's joint `part`, or its place (the box: its ground, or its centre)
                    float own[3] = {S->box ? I.kTargetX : I.kUserX, 0, 0}, r3[3], s3[3];
                    if (bp.part > 0) {
                        if (S->box && !I.has_target()) I.box_joint(Impl::kBoxCentre, I.box_pose(I.box_frame), own, r3);
                        else I.owner_joint(S->box, bp.part, I.box_frame, own, r3, s3);
                    }
                    const int nq = live ? orre_bolt_quads(bo, eye_b, own, nullptr, nullptr, nullptr, 1 << 20) : 0;
                    if (nq > 0) {
                        std::vector<float> xyz((size_t)nq * 12), uv((size_t)nq * 8);
                        std::vector<uint8_t> col((size_t)nq * 16);
                        orre_bolt_quads(bo, eye_b, own, xyz.data(), uv.data(), col.data(), nq);
                        // its picture, made once per file and entry
                        const auto key = std::make_pair((const void*)S->wzx, (int)e);
                        auto it = I.bolt_tex.find(key);
                        if (it == I.bolt_tex.end()) {
                            wgpu::BindGroup g = I.pwhite;
                            if (bp.tex_rgba) g = I.particle_group(I.make_texture(bp.tex_width, bp.tex_height, bp.tex_rgba));
                            it = I.bolt_tex.emplace(key, g).first;
                        }
                        // textured, texture x vertex colour (the trail combine), added (blend 1: SRCALPHA / ONE, as
                        // _boltRender's gfxSetBlendMode(1, 6, 1)); its alpha test always passes
                        const uint32_t kind = 0x400u | 0x100000u | (1u << 22);
                        pruns.push_back({(uint32_t)I.pverts.size(), 0, 1, false, it->second});
                        for (int qd = 0; qd < nq; ++qd) {
                            for (int k : kTri) {
                                ParticleVertex pv{};
                                const float* g = &xyz[((size_t)qd * 4 + k) * 3];
                                pv.pos[0] = -g[2];
                                pv.pos[1] = g[1];
                                pv.pos[2] = g[0] - I.kUserX;
                                pv.uv[0] = uv[((size_t)qd * 4 + k) * 2];
                                pv.uv[1] = uv[((size_t)qd * 4 + k) * 2 + 1];
                                std::copy(&col[((size_t)qd * 4 + k) * 4], &col[((size_t)qd * 4 + k) * 4] + 4, pv.ras);
                                pv.kind = kind;
                                pv.alpha = 0x3F;
                                I.pverts.push_back(pv);
                            }
                            pruns.back().count += 6;
                        }
                    }
                    orre_bolt_free(bo);
                }
            }
        }
        // the screen blur: the last finished picture over this one (only while the move plays on from that frame)
        blur_now = 0;
        // (a held screenshot shows it after walk_into_hold, which gives it the frames before)
        const float sb = I.can_copy && I.once >= 0 && !I.held && (hold_ < 0 || hold_walk_ >= 0) ? I.screen_blur(I.box_frame) : 0.0f;
        if (sb > 0) {
            if (!I.prev) {
                wgpu::TextureDescriptor pd;
                pd.size = {(uint32_t)I.w, (uint32_t)I.h, 1};
                pd.format = I.format;
                pd.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
                I.prev = I.device.CreateTexture(&pd);
                I.prev_group = I.particle_group(I.prev);
                I.prev_frame = -1;
            }
            if (I.prev_frame >= 0 && (I.prev_frame == I.box_frame || I.prev_frame == I.box_frame - 1)) {
                const float right[3] = {mv.m[0], mv.m[4], mv.m[8]}, up[3] = {mv.m[1], mv.m[5], mv.m[9]},
                            back[3] = {mv.m[2], mv.m[6], mv.m[10]};
                float eye[3];
                for (int c = 0; c < 3; ++c) eye[c] = -(mv.m[c * 4] * mv.m[12] + mv.m[c * 4 + 1] * mv.m[13] + mv.m[c * 4 + 2] * mv.m[14]);
                const float d = std::max(2.0f, I.radius * 0.1f);
                const float hh = d * std::tan(cam_fov * 0.5f), hw = hh * cam_aspect;
                const float sg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                const uint32_t first = (uint32_t)I.pverts.size();
                const uint8_t a8 = (uint8_t)std::clamp((int)(255.0f * sb), 0, 255);
                static const int kTri[6] = {0, 1, 2, 0, 2, 3};
                for (int k : kTri) {
                    ParticleVertex pv{};
                    for (int c = 0; c < 3; ++c) pv.pos[c] = eye[c] - back[c] * d + right[c] * hw * sg[k][0] + up[c] * hh * sg[k][1];
                    pv.uv[0] = (sg[k][0] + 1) * 0.5f;
                    pv.uv[1] = (1 - sg[k][1]) * 0.5f;
                    // textured, PrimEnv: colour = the picture, alpha = the blur's (prim and env alpha both a)
                    const uint8_t prim[4] = {255, 255, 255, a8}, env[4] = {0, 0, 0, a8};
                    std::copy(prim, prim + 4, pv.prim);
                    std::copy(env, env + 4, pv.env);
                    std::copy(prim, prim + 4, pv.ras);
                    pv.kind = 0x400u | 0x80u;
                    pv.alpha = 0x3F;
                    I.pverts.push_back(pv);
                }
                pruns.push_back({first, 6, 0, true, I.prev_group, false, 1});
            }
            blur_now = 1;
        }
        // a full-screen colour fade (target 0; GSfilter: a screen quad of the colour, alpha-blended over the 3D
        // scene, drawn while its alpha isn't 0): a quad just in front of the camera that fills the view
        if (I.screen_fade_on && I.screen_fade[3] != 0) {
            const float right[3] = {mv.m[0], mv.m[4], mv.m[8]}, up[3] = {mv.m[1], mv.m[5], mv.m[9]},
                        back[3] = {mv.m[2], mv.m[6], mv.m[10]};
            float eye[3];
            for (int c = 0; c < 3; ++c) eye[c] = -(mv.m[c * 4] * mv.m[12] + mv.m[c * 4 + 1] * mv.m[13] + mv.m[c * 4 + 2] * mv.m[14]);
            const float d = std::max(2.0f, I.radius * 0.1f);
            const float hh = d * std::tan(cam_fov * 0.5f) * 1.2f, hw = hh * aspect * 1.2f;
            const float sg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            const uint32_t first = (uint32_t)I.pverts.size();
            static const int kTri[6] = {0, 1, 2, 0, 2, 3};
            for (int k : kTri) {
                ParticleVertex pv{};
                for (int c = 0; c < 3; ++c) pv.pos[c] = eye[c] - back[c] * d + right[c] * hw * sg[k][0] + up[c] * hh * sg[k][1];
                std::copy(I.screen_fade, I.screen_fade + 4, pv.prim);
                pv.kind = 0;
                pv.alpha = 0x3F;
                I.pverts.push_back(pv);
            }
            pruns.push_back({first, 6, 0, true, I.pwhite, false, 1});  // (last: over both passes)
        }
        const size_t bytes = I.pverts.size() * sizeof(ParticleVertex);
        if (bytes > I.pvcap) {
            wgpu::BufferDescriptor bd;
            bd.size = std::max<size_t>(bytes, 64 * 1024);
            bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
            I.pvbuf = I.device.CreateBuffer(&bd);
            I.pvcap = bd.size;
        }
        if (bytes) I.queue.WriteBuffer(I.pvbuf, 0, I.pverts.data(), bytes);
    }
    wgpu::CommandEncoder enc = I.device.CreateCommandEncoder();
    // the env-map shine's picture: the other models from this camera into env_tex (_envMapUpdate: GSgfxRenderLayer,
    // then GSmodelDrawModel for every visible model but the owner; the clear colour black is a guess), then the
    // Pokémon's materials with the extra stage
    if (I.env_now) {
        if (!I.env_tex || I.env_w != I.w || I.env_h != I.h) {
            wgpu::TextureDescriptor td;
            td.size = {(uint32_t)I.w, (uint32_t)I.h, 1};
            td.format = I.format;
            td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
            I.env_tex = I.device.CreateTexture(&td);
            td.format = wgpu::TextureFormat::Depth24Plus;
            td.usage = wgpu::TextureUsage::RenderAttachment;
            I.env_depth = I.device.CreateTexture(&td);
            I.env_w = I.w;
            I.env_h = I.h;
            I.env_built = -1;
        }
        wgpu::RenderPassColorAttachment eca;
        eca.view = I.env_tex.CreateView();
        eca.loadOp = wgpu::LoadOp::Clear;
        eca.storeOp = wgpu::StoreOp::Store;
        eca.clearValue = {0, 0, 0, 1};
        wgpu::RenderPassDepthStencilAttachment eda;
        eda.view = I.env_depth.CreateView();
        eda.depthLoadOp = wgpu::LoadOp::Clear;
        eda.depthStoreOp = wgpu::StoreOp::Discard;
        eda.depthClearValue = 1.0f;
        wgpu::RenderPassDescriptor erp;
        erp.colorAttachmentCount = 1;
        erp.colorAttachments = &eca;
        erp.depthStencilAttachment = &eda;
        wgpu::RenderPassEncoder ep = enc.BeginRenderPass(&erp);
        auto draw_inst = [&](const Impl::Inst& in) {
            for (const auto& p : in.parts) {
                if (p.mesh < (int)in.shown.size() && !in.shown[p.mesh]) continue;
                ep.SetPipeline(p.pipeline);
                ep.SetBindGroup(1, p.group);
                ep.SetVertexBuffer(0, p.vbuf);
                ep.Draw(p.count);
            }
        };
        if (I.arena_on && I.arena) {
            ep.SetBindGroup(0, I.stage_group);
            draw_inst(*I.arena);
        }
        ep.SetBindGroup(0, I.camera_group);
        if (I.has_target() && I.box_on && !I.box_hidden) draw_inst(*I.target);
        for (const auto& fm : I.fx_models)
            if (!fm.done) draw_inst(*fm.inst);
        for (Impl::Inst* in : I.leaves_now) draw_inst(*in);
        ep.End();
        I.env_build();  // (every frame it runs: the value and the Pokémon's own texture animation both change)
    }
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.SetPipeline(I.background);
    pass.SetBindGroup(0, I.camera_group);
    pass.SetBindGroup(1, I.default_group);
    pass.Draw(3);
    // the arena first, as the game draws its room before the Pokémon (with the stage-tinted uniforms)
    if (I.arena_on && I.arena) {
        pass.SetBindGroup(0, I.stage_group);
        for (const auto& p : I.arena->parts) {
            if (p.mesh < (int)I.arena->shown.size() && !I.arena->shown[p.mesh]) continue;
            pass.SetPipeline(p.pipeline);
            pass.SetBindGroup(1, p.group);
            pass.SetVertexBuffer(0, p.vbuf);
            pass.Draw(p.count);
        }
        pass.SetBindGroup(0, I.camera_group);
    }
    // (a move's control step can hide the Pokémon: wazaSequenceOwnerSetVisibility, Dig, Fly, the faint's end)
    // the after-images first, oldest first, alpha-blended without depth writes (guess: the game's own PE settings for
    // them, lbl_80363CC8, aren't read yet), then the Pokémon itself
    for (size_t g = 0; g < I.ghost_now.size(); ++g) {
        pass.SetBindGroup(0, I.ghosts[g].cg);
        for (auto& p : I.parts) {
            if (p.mesh < (int)I.shown.size() && !I.shown[p.mesh]) continue;
            if (!p.ghost_pipe) {
                DrawState ds = draw_state(I.model.meshes[p.mesh]);
                ds.blend_type = 1;
                ds.src = 4;
                ds.dst = 5;
                ds.z_write = false;
                p.ghost_pipe = I.pipeline(ds);
            }
            pass.SetPipeline(p.ghost_pipe);
            pass.SetBindGroup(1, p.group);
            pass.SetVertexBuffer(0, I.ghosts[g].vb[(size_t)p.mesh]);
            pass.Draw(p.count);
        }
    }
    if (!I.ghost_now.empty()) pass.SetBindGroup(0, I.camera_group);
    // The game's two passes (GSgfxDrawScene, src/game/gs_gfx_range_800D3FA4.c, 100% matched; the project's RenderDoc capture
    // of Fire Punch, 2026-11-20, verified): pass 0 draws the room, the models without the overlay flag (0x200) and the
    // link-0 particles; then a full-screen quad writes the far depth everywhere (colour untouched); pass 1 draws the
    // overlay models and the link-1 particles, which a move's particle entries are. So the floor can't hide a hit's
    // burst, and a Pokémon in pass 0 can't hide the move's flames. Here the second pass is a second render pass that
    // keeps the picture and clears depth to far.
    auto draw_models = [&](wgpu::RenderPassEncoder& rpass, int which) {
    if (I.has_model && !(I.user_hidden && I.once >= 0) && (user_overlay ? 1 : 0) == which) {
        const bool env = I.env_now && I.env_groups.size() == I.parts.size();
        const bool faded = I.user_alpha < 0.999f && I.fade_cg;  // (the user's fade alpha: user_alpha)
        if (faded) rpass.SetBindGroup(0, I.fade_cg);
        for (size_t pi = 0; pi < I.parts.size(); ++pi) {
            auto& p = I.parts[pi];
            if (faded && !p.ghost_pipe) {
                DrawState ds = draw_state(I.model.meshes[p.mesh]);
                ds.blend_type = 1;
                ds.src = 4;
                ds.dst = 5;
                ds.z_write = false;
                p.ghost_pipe = I.pipeline(ds);
            }
            if (p.mesh < (int)I.shown.size() && !I.shown[p.mesh]) continue;
            // glow cards (Espeon's gem sparkle; OrreMesh.glow) go with the particles when the switch is off (snatcherLeo,
            // 2026-11-19: "can we make it so that, the sparkle effect can be considered a particle")
            if (!I.particles_on && I.model.meshes[p.mesh].glow) continue;
            // ORRE_DEBUG_ONLY_MESHES=0,4,6 (developer switch, any OS): draw only these parts, to see which part covers
            // which (how Lunatone's mouth was looked into, 2026-11-19)
            if (const char* only = std::getenv("ORRE_DEBUG_ONLY_MESHES")) {
                const std::string o = std::string(",") + only + ",";
                if (o.find("," + std::to_string(p.mesh) + ",") == std::string::npos) continue;
            }
            rpass.SetPipeline(faded ? p.ghost_pipe : p.pipeline);
            rpass.SetBindGroup(1, env && I.env_groups[pi] ? I.env_groups[pi] : p.group);  // (the shine's stage: env_build)
            rpass.SetVertexBuffer(0, p.vbuf);
            rpass.Draw(p.count);
        }
        if (faded) rpass.SetBindGroup(0, I.camera_group);
    }
    // the target Pokémon (in the box's place), with the same camera and pipelines, before the move's models.
    // *Moved 2026-11-21:* it was drawn after them, so a translucent effect model (no depth write) lay under it:
    // Muddy Water's mud and Block's X showed Umbreon on top, where the game draws them over her. The game draws the
    // translucent models after the solid ones (the project's RenderDoc capture of Surf: the water in HSD's translucent
    // sub-pass, after Espeon; verified)
    if (I.has_target() && I.box_on && !(I.box_hidden && I.once >= 0) && (target_overlay ? 1 : 0) == which) {
        rpass.SetBindGroup(0, I.target_group);  // (its half's own tint: uniforms_target)
        for (const auto& p : I.target->parts) {
            if (p.mesh < (int)I.target->shown.size() && !I.target->shown[p.mesh]) continue;
            rpass.SetPipeline(p.pipeline);
            rpass.SetBindGroup(1, p.group);
            rpass.SetVertexBuffer(0, p.vbuf);
            rpass.Draw(p.count);
        }
        rpass.SetBindGroup(0, I.camera_group);
    }
    // the move's own models, while they run (the game draws them with their owner, after it; in the second pass when
    // their entry says so)
    if (I.once >= 0)
        for (const auto& fm : I.fx_models) {
            if (fm.done || (fm.overlay ? 1 : 0) != which) continue;
            for (const auto& p : fm.inst->parts) {
                if (p.mesh < (int)fm.inst->shown.size() && !fm.inst->shown[p.mesh]) continue;
                rpass.SetPipeline(p.pipeline);
                rpass.SetBindGroup(1, p.group);
                rpass.SetVertexBuffer(0, p.vbuf);
                rpass.Draw(p.count);
            }
        }
    // the leaves (the first pass)
    if (which == 0)
        for (Impl::Inst* in : I.leaves_now)
            for (const auto& p : in->parts) {
                if (p.mesh < (int)in->shown.size() && !in->shown[p.mesh]) continue;
                rpass.SetPipeline(p.pipeline);
                rpass.SetBindGroup(1, p.group);
                rpass.SetVertexBuffer(0, p.vbuf);
                rpass.Draw(p.count);
            }
    // the billboards (the second pass: billboardEffectStart puts their model there)
    if (which == 1)
        for (Impl::Inst* in : I.billboard_now)
            for (const auto& p : in->parts) {
                if (p.mesh < (int)in->shown.size() && !in->shown[p.mesh]) continue;
                rpass.SetPipeline(p.pipeline);
                rpass.SetBindGroup(1, p.group);
                rpass.SetVertexBuffer(0, p.vbuf);
                rpass.Draw(p.count);
            }
    // the particles after the models, as gsParticleDraw follows GSmodelDrawAll in each pass
    for (const PRun& r2 : pruns) {
        if (r2.pass != which) continue;
        rpass.SetPipeline(I.particle_pipe(r2.blend, r2.noz, r2.solid));
        rpass.SetBindGroup(0, I.camera_group);
        rpass.SetBindGroup(1, r2.tex);
        rpass.SetVertexBuffer(0, I.pvbuf);
        rpass.Draw(r2.count, 1, r2.first, 0);
    }
    };
    draw_models(pass, 0);
    pass.End();
    {
        // the depth reset, then the second pass over the same picture
        wgpu::RenderPassColorAttachment ca2 = ca;
        ca2.loadOp = wgpu::LoadOp::Load;
        wgpu::RenderPassDepthStencilAttachment da2 = da;
        da2.depthLoadOp = wgpu::LoadOp::Clear;
        wgpu::RenderPassDescriptor rp2 = rp;
        rp2.colorAttachments = &ca2;
        rp2.depthStencilAttachment = &da2;
        // the screen warps: the first pass's picture copied, then each warp's square drawn over it (kWarpShader)
        std::vector<wgpu::BindGroup> wgroups;
        if (!I.warps_now.empty()) {
            I.warp_pipeline();
            if (!I.warp_scene || I.warp_w != I.w || I.warp_h != I.h) {
                wgpu::TextureDescriptor td;
                td.size = {(uint32_t)I.w, (uint32_t)I.h, 1};
                td.format = I.format;
                td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
                I.warp_scene = I.device.CreateTexture(&td);
                I.warp_w = I.w;
                I.warp_h = I.h;
            }
            wgpu::TexelCopyTextureInfo src, dst;
            src.texture = st.texture;
            dst.texture = I.warp_scene;
            const wgpu::Extent3D size{(uint32_t)I.w, (uint32_t)I.h, 1};
            enc.CopyTextureToTexture(&src, &dst, &size);
            // the squares: facing the camera (its right and up in the Pokémon's space), corners (-1, -1)..(1, 1) with
            // the picture's (0, 0)..(1, 1), as _distortionRender's quad
            const float right[3] = {mv.m[0], mv.m[4], mv.m[8]}, up[3] = {mv.m[1], mv.m[5], mv.m[9]};
            std::vector<float> vtx;
            static const float kc[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            float k = 255.0f;  // (the offsets' scale: intensity 1.0 = 255 texels, kWarpShader)
            if (const char* kv = std::getenv("ORRE_DEBUG_WARP_K")) k = (float)std::atof(kv);  // (developer switch, any OS)
            for (const auto& wn : I.warps_now) {
                for (const auto& c : kc) {
                    for (int q = 0; q < 3; ++q) vtx.push_back(wn.c[q] + (c[0] * right[q] + c[1] * up[q]) * wn.half);
                    vtx.push_back(c[0] * 0.5f + 0.5f);
                    vtx.push_back(c[1] * 0.5f + 0.5f);
                }
                wgpu::BufferDescriptor ubd;
                ubd.size = 16;
                ubd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
                wgpu::Buffer ub = I.device.CreateBuffer(&ubd);
                const float kk[4] = {wn.strength, k, (float)I.w, (float)I.h};
                I.queue.WriteBuffer(ub, 0, kk, sizeof kk);
                wgpu::BindGroupEntry be[4];
                be[0].binding = 0;
                be[0].buffer = ub;
                be[0].size = 16;
                be[1].binding = 1;
                be[1].sampler = I.psampler;
                be[2].binding = 2;
                be[2].textureView = I.warp_scene.CreateView();
                be[3].binding = 3;
                be[3].textureView = wn.pic.CreateView();
                wgpu::BindGroupDescriptor bgd;
                bgd.layout = I.wlayout1;
                bgd.entryCount = 4;
                bgd.entries = be;
                wgroups.push_back(I.device.CreateBindGroup(&bgd));
            }
            const uint64_t bytes = vtx.size() * sizeof(float);
            if (!I.wvbuf || I.wvbuf.GetSize() < bytes) {
                wgpu::BufferDescriptor vbd;
                vbd.size = std::max<uint64_t>(bytes, 4096);
                vbd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
                I.wvbuf = I.device.CreateBuffer(&vbd);
            }
            I.queue.WriteBuffer(I.wvbuf, 0, vtx.data(), bytes);
        }
        wgpu::RenderPassEncoder pass1 = enc.BeginRenderPass(&rp2);
        pass1.SetBindGroup(0, I.camera_group);
        for (size_t wi = 0; wi < wgroups.size(); ++wi) {
            pass1.SetPipeline(I.wpipe);
            pass1.SetBindGroup(1, wgroups[wi]);
            pass1.SetVertexBuffer(0, I.wvbuf);
            pass1.Draw(6, 1, (uint32_t)(wi * 6), 0);
        }
        draw_models(pass1, 1);
        pass1.End();
    }
    // the screen blur's capture: this finished picture, for the next frame's blur (GSgfxBeginBackFBCapture)
    if (blur_now && I.prev) {
        wgpu::TexelCopyTextureInfo src, dst;
        src.texture = st.texture;
        dst.texture = I.prev;
        const wgpu::Extent3D size{(uint32_t)I.w, (uint32_t)I.h, 1};
        enc.CopyTextureToTexture(&src, &dst, &size);
        I.prev_frame = I.box_frame;
    } else {
        I.prev_frame = -1;
    }
    // ORRE_FRAME_WATCH: this frame copied out for reading back (see Impl::watch_frames). Windows and every platform:
    // WebGPU's own copy and map calls
    wgpu::Buffer rb;
    uint32_t rb_row = 0;
    if (I.watch_frames > 0 && I.can_copy) {
        rb_row = ((uint32_t)I.w * 4 + 255) & ~255u;
        wgpu::BufferDescriptor bd;
        bd.size = (uint64_t)rb_row * (uint64_t)I.h;
        bd.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        rb = I.device.CreateBuffer(&bd);
        wgpu::TexelCopyTextureInfo src;
        src.texture = st.texture;
        wgpu::TexelCopyBufferInfo dst;
        dst.buffer = rb;
        dst.layout.bytesPerRow = rb_row;
        dst.layout.rowsPerImage = (uint32_t)I.h;
        const wgpu::Extent3D size{(uint32_t)I.w, (uint32_t)I.h, 1};
        enc.CopyTextureToBuffer(&src, &dst, &size);
    }
    wgpu::CommandBuffer cb = enc.Finish();
    I.queue.Submit(1, &cb);
    // the page's copy (share_into): the finished frame copied into the shared texture, inside its access (Dawn takes the
    // keyed mutex at BeginAccess and gives it back at EndAccess), then the page is told a frame is there
    if (I.share_tex && I.can_copy) {
        wgpu::SharedTextureMemoryBeginAccessDescriptor bd;
        bd.concurrentRead = false;
        bd.initialized = I.share_init;
        bd.fenceCount = 0;
        if (I.share_mem.BeginAccess(I.share_tex, &bd)) {
            wgpu::CommandEncoder e2 = I.device.CreateCommandEncoder();
            wgpu::TexelCopyTextureInfo src, dst;
            src.texture = st.texture;
            dst.texture = I.share_tex;
            const wgpu::Extent3D size{(uint32_t)std::min(I.w, I.share_w), (uint32_t)std::min(I.h, I.share_h), 1};
            e2.CopyTextureToTexture(&src, &dst, &size);
            wgpu::CommandBuffer c2 = e2.Finish();
            I.queue.Submit(1, &c2);
            wgpu::SharedTextureMemoryEndAccessState es;
            I.share_mem.EndAccess(I.share_tex, &es);
            I.share_init = true;
            I.share_fw = (int)size.width;
            I.share_fh = (int)size.height;
            if (++I.share_frames == 1) logf(Level::Info, "viewer", "first frame shared with the page (%dx%d)", I.share_w, I.share_h);
            PostMessageW(GetParent(hwnd_), WM_APP + 30, 0, 0);  // (the page draws it: WM_VIEW_SHARED)
        } else if (I.share_frames == 0) {
            logf(Level::Error, "viewer", "the shared texture couldn't be accessed");
            I.share_tex = nullptr;
        }
    }
    if (rb) {
        bool ok = false;
        auto f = rb.MapAsync(wgpu::MapMode::Read, 0, (size_t)rb.GetSize(), wgpu::CallbackMode::WaitAnyOnly,
                             [&](wgpu::MapAsyncStatus s, wgpu::StringView) { ok = s == wgpu::MapAsyncStatus::Success; });
        I.instance.WaitAny(f, 1000000000ull);
        if (ok) {
            const auto* p = static_cast<const uint8_t*>(rb.GetConstMappedRange(0, (size_t)rb.GetSize()));
            double sum = 0;
            size_t white = 0;
            for (int y = 0; y < I.h; ++y) {
                const uint8_t* row = p + (size_t)y * rb_row;
                for (int x = 0; x < I.w; ++x, row += 4) {
                    sum += 0.0722 * row[0] + 0.7152 * row[1] + 0.2126 * row[2];  // (BGRA)
                    if (row[0] > 225 && row[1] > 225 && row[2] > 225) ++white;
                }
            }
            rb.Unmap();
            const double n = (double)I.w * I.h, mean = sum / n, share = (double)white / n;
            const bool bright = (I.watch_prev >= 0 && mean > I.watch_prev + 15) || share > 0.3;
            logf(bright ? Level::Warn : Level::Info, "frames", "after the model change, frame %d: brightness %.0f, near-white %.3f%s",
                 I.watch_seq, mean, share, bright ? " (a bright frame)" : "");
            I.watch_prev = mean;
        }
        ++I.watch_seq;
        --I.watch_frames;
    }
    if (!I.page) I.surface.Present();  // (page mode: the page shows the frame, WM_VIEW_SHARED)
    static bool first = true;  // the watch measures the first frame on its own (shader compilation, driver caches)
    if (first) { first = false; watch_check("first 3D frame"); }
    if (!I.framed && I.model.mesh_count > 0) {
        // the first frame with a model in it: the page may reveal the view now (WM_APP + 21 to the parent window)
        I.framed = true;
        PostMessageW(GetParent(hwnd_), WM_APP + 21, 0, 0);
    }
}

LRESULT Viewer::proc(UINT msg, WPARAM wp, LPARAM lp) {
    Impl* I = impl_;
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            EndPaint(hwnd_, &ps);
            // the pacer's next frame (rates above 64 Hz): it asks by invalidating the view, so the frame arrives as a
            // paint, which Windows hands out only when no input waits (a posted message is handed out before input,
            // and at 165 Hz that starved every click: snatcherLeo, 2026-11-19, "my mouse is frozen")
            if (I && I->pacer_pending.exchange(false) && I->rate > 64 && IsWindowVisible(hwnd_) &&
                !IsIconic(GetAncestor(hwnd_, GA_ROOT))) {
                advance_idle();
                if (spin_ && !I->dragging && !I->paused) I->yaw += 0.006f * 60.0f / (float)I->rate;
            }
            render();
            return 0;
        }
        case WM_APP + 31:  // page mode: a frame asked for (request_frame)
            if (I) I->frame_posted = false;
            render();
            return 0;
        case WM_APP + 32:  // page mode: the pacer's next frame (rates above 64 Hz), left out while input waits
            if (!I) return 0;
            if (HIWORD(GetQueueStatus(QS_INPUT)) != 0) { I->pacer_pending = false; return 0; }
            if (I->pacer_pending.exchange(false) && I->rate > 64 && !IsIconic(GetAncestor(hwnd_, GA_ROOT))) {
                advance_idle();
                if (spin_ && !I->dragging && !I->paused) I->yaw += 0.006f * 60.0f / (float)I->rate;
                render();
            }
            return 0;
        case WM_TIMER:
            if (I && IsWindowVisible(hwnd_) && !IsIconic(GetAncestor(hwnd_, GA_ROOT))) {
                bool changed = advance_idle();
                if (spin_ && !I->dragging && !I->paused) { I->yaw += 0.006f; changed = true; }
                if (changed) render();
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (I) {
                I->dragging = true;
                I->last_x = GET_X_LPARAM(lp);
                I->last_y = GET_Y_LPARAM(lp);
                SetCapture(hwnd_);
                SendMessageW(GetParent(hwnd_), WM_APP + 20, 0, 0);  // tells the page the spin stopped (its button)
            }
            return 0;
        case WM_MOUSEMOVE:
            if (I && I->dragging) {
                const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                if (I->plain && !I->studio_cam) {  // (Plain stage turns its own camera, from the game's angle)
                    I->plain_yaw -= (x - I->last_x) * 0.01f;
                    I->plain_pitch += (y - I->last_y) * 0.01f;
                } else {
                    I->yaw += (x - I->last_x) * 0.01f;
                    I->pitch = std::clamp(I->pitch + (y - I->last_y) * 0.01f, -1.2f, 1.2f);
                }
                I->last_x = x;
                I->last_y = y;
                render();
            }
            return 0;
        case WM_LBUTTONUP:
            if (I) I->dragging = false;
            ReleaseCapture();
            PostMessageW(GetParent(hwnd_), WM_APP + 33, 0, 0);  // (the page redraws: its ↺ lights up after a turn)
            return 0;
        case WM_LBUTTONDBLCLK:
            reset_view();
            PostMessageW(GetParent(hwnd_), WM_APP + 33, 0, 0);  // (the page redraws: its ↺ dims again)
            return 0;
        case WM_MOUSEWHEEL:
            if (I) {
                I->dist = std::clamp(I->dist * (GET_WHEEL_DELTA_WPARAM(wp) < 0 ? 1.1f : 0.9f), 0.4f, 3.0f);
                render();
                PostMessageW(GetParent(hwnd_), WM_APP + 33, 0, 0);  // (the page redraws: its ↺ lights up after a zoom)
            }
            return 0;
        case WM_SIZE:
            request_frame();
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace studio
