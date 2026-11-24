// OrrePort trial (2026-11-10; docs/port/pipeline-hitches.md): the "uber-fragment" path.
//
// A draw whose setup is eligible keeps Aurora's generated vertex stage, but its fragment stage (the TEV stages, swap
// tables, constant colors, fog and alpha compare) is interpreted from a small program the draw appends to its uniform
// data. Its pipeline key is the config with the fragment fields zeroed and ShaderConfig.pad2 = UberShaderMarker, so
// every TEV setup sharing a vertex setup and fixed-function state shares one pipeline. The rules below mirror
// shader.cpp's generator line for line (tev_op, color_arg_reg / alpha_arg_reg, the overflow wrapping of unclamped
// registers, fog, alpha compare); a difference between the two is a bug in this file.
//
// Stage 2 (same day): the color channels (material / ambient sources, per-vertex lighting) and the texture coordinate
// generation are interpreted too, in the vertex stage (uber_vertex_source mirrors lighting_func and the texgen loop of
// build_shader_source); the pipeline key then keeps only the vertex format and the fixed-function state.
//
// Not covered (those draws keep the generated shader): indirect texturing, alpha-bump channels, emboss texgens,
// binormal / tangent texgen sources, lines / points, range-adjusted fog.

#include "gx.hpp"
#include "pipeline.hpp"

#include <cstdlib>
#include <fmt/format.h>

namespace aurora::gx {
namespace {
std::array<u32, UberProgramVec4s * 4> s_program{};
u32 s_texMask = 0; // the texture maps the current program samples

u32 pack_swap(const TevSwap& s) noexcept {
  return (u32(s.red) & 3u) | (u32(s.green) & 3u) << 2 | (u32(s.blue) & 3u) << 4 | (u32(s.alpha) & 3u) << 6;
}
u32 pack_op(const TevOp& op) noexcept {
  return (u32(op.op) & 0xFu) | (u32(op.bias) & 3u) << 4 | (u32(op.scale) & 3u) << 6 | (op.clamp ? 1u : 0u) << 8 |
         (u32(op.outReg) & 3u) << 9;
}
} // namespace

// The mode, from ORRE_UBERSHADER: 0 = generated shaders only (the reference), 1 = the ubershader for every eligible draw
// (pure), unset or anything else = hybrid (the default since 2026-11-11, snatcherLeo: "Yes, hybrid first"): a draw uses its
// specialized pipeline once that is compiled, the ubershader until then (pipeline-hitches.md "Hybrid")
static int uber_mode() noexcept {
  static int mode = -1;
  if (mode < 0) {
    const char* env = std::getenv("ORRE_UBERSHADER");
    mode = env != nullptr && env[0] == '0' ? 0 : env != nullptr && env[0] == '1' ? 1 : 2;
  }
  return mode;
}

bool uber_enabled() noexcept { return uber_mode() != 0; }

bool uber_hybrid() noexcept { return uber_mode() == 2; }

bool uber_eligible(const ShaderConfig& config) noexcept {
  // (lines and points are interpreted since the 0.2 % step: the line mode stays in the pipeline key; so is indirect
  // texturing, except the alpha-bump channels, which Colosseum never uses)
  if (config.fogRangeEnabled || config.numIndStages > MaxIndStages || config.tevStageCount == 0 ||
      config.tevStageCount > MaxTevStages) {
    return false;
  }
  for (u32 k = 0; k < config.numIndStages; ++k) {
    const auto& is = config.indStages[k];
    if (is.texCoordId > GX_TEXCOORD7 || is.texMapId > GX_TEXMAP7 || is.scaleS > GX_ITS_256 ||
        is.scaleT > GX_ITS_256) {
      return false;
    }
  }
  for (u32 i = 0; i < config.tevStageCount; ++i) {
    const auto& st = config.tevStages[i];
    if (st.channelId == GX_ALPHA_BUMP || st.channelId == GX_ALPHA_BUMPN || st.indTexFormat > GX_ITF_3 ||
        st.indTexWrapS > GX_ITW_0 || st.indTexWrapT > GX_ITW_0 || st.indTexStage > GX_INDTEXSTAGE3 ||
        (st.indTexMtxId != GX_ITM_OFF && !(st.indTexMtxId >= GX_ITM_0 && st.indTexMtxId <= GX_ITM_2) &&
         !(st.indTexMtxId >= GX_ITM_S0 && st.indTexMtxId <= GX_ITM_S2) &&
         !(st.indTexMtxId >= GX_ITM_T0 && st.indTexMtxId <= GX_ITM_T2))) {
      return false;
    }
    if (st.texMapId != GX_TEXMAP_NULL && st.texMapId > GX_TEXMAP7) {
      return false;
    }
  }
  for (const auto& tcg : config.tcgs) {
    if (tcg.src == GX_MAX_TEXGENSRC) {
      continue;
    }
    if (tcg.type >= GX_TG_BUMP0 && tcg.type <= GX_TG_BUMP7) {
      // emboss: reads the binormal and tangent of an NBT normal (the generator's attr_load_nbt_slice needs one too)
      const auto& nrm = config.attrs[GX_VA_NRM];
      if (nrm.attrType == GX_NONE || nrm.cnt != 9 || tcg.embossSrc >= MaxTexCoord) {
        return false;
      }
      continue;
    }
    if (tcg.type != GX_TG_MTX3x4 && tcg.type != GX_TG_MTX2x4 && tcg.type != GX_TG_SRTG) {
      return false;
    }
    if (tcg.src == GX_TG_BINRM || tcg.src == GX_TG_TANGENT || tcg.src > GX_TG_COLOR1) {
      return false;
    }
    if (tcg.mtx != GX_IDENTITY && tcg.mtx / 3u >= MaxPnMtx + MaxTexMtx) {
      return false;
    }
    if (tcg.postMtx != GX_PTIDENTITY && (tcg.postMtx < GX_PTTEXMTX0 || (tcg.postMtx - GX_PTTEXMTX0) / 3u >= MaxPTTexMtx)) {
      return false;
    }
  }
  // stage 3: the matrix-index attributes are direct bytes (an indexed one would change the generated texgen's choice)
  for (u32 a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7MTXIDX; ++a) {
    if (config.attrs[a].attrType != GX_NONE && config.attrs[a].attrType != GX_DIRECT) {
      return false;
    }
  }
  if (config.attrs[GX_VA_POS].attrType == GX_NONE) {
    return false; // the generated shader can't draw it either
  }
  for (u32 a = GX_VA_POS; a <= GX_VA_TEX7; ++a) {
    const auto& m = config.attrs[a];
    if (m.attrType == GX_NONE) {
      continue;
    }
    const bool color = a == GX_VA_CLR0 || a == GX_VA_CLR1;
    if ((color && m.compType > GX_RGBA8) || (!color && m.compType > GX_RGBA8) || m.frac > 31) {
      return false;
    }
  }
  return true;
}

u32 uber_texture_mask() noexcept { return s_texMask; }

void uber_encode_program(const ShaderConfig& c) noexcept {
  s_program.fill(0);
  s_texMask = 0;
  for (u32 i = 0; i < c.tevStageCount; ++i) {
    if (c.tevStages[i].texMapId <= GX_TEXMAP7) {
      s_texMask |= 1u << u32(c.tevStages[i].texMapId);
    }
  }
  const auto& ac = c.alphaCompare;
  s_program[0] = c.tevStageCount;
  s_program[1] = (u32(ac.comp0) & 7u) | u32(ac.ref0) << 8 | (u32(ac.comp1) & 7u) << 16 | u32(ac.ref1) << 24;
  // header z: alpha-compare op (0-1), fog type (8-15), vertex stride (16-23), the normal carries binormal and tangent
  // (NBT, 24) and each with its own index (NBT3, 25)
  const auto& nrmAttr = c.attrs[GX_VA_NRM];
  const bool nbt = nrmAttr.attrType != GX_NONE && nrmAttr.cnt == 9;
  s_program[2] = (u32(ac.op) & 3u) | (u32(c.fogType) & 0xFFu) << 8 | u32(c.vtxStride) << 16 | (nbt ? 1u : 0u) << 24 |
                 (nbt && nrmAttr.nbt3 ? 1u : 0u) << 25;
  // vec4u 4-9 (stage 3): one word per vertex attribute, GX_VA_PNMTXIDX .. GX_VA_TEX7. Bits 0-1 attrType (none, direct,
  // index8, index16), 2-5 component count (the normal's NBT counts read as 3, as attr_load does), 6-9 compType, 10-17
  // offset in the vertex, 18-22 frac, 23 little-endian array, 24-31 array stride
  for (u32 a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7; ++a) {
    const auto& m = c.attrs[a];
    if (m.attrType == GX_NONE) {
      continue;
    }
    u32 cnt = m.cnt;
    if (a == GX_VA_NRM && cnt > 3) {
      cnt = 3;
    }
    s_program[4 * UberAttrBase + a] = (u32(m.attrType) & 3u) | (cnt & 0xFu) << 2 | (u32(m.compType) & 0xFu) << 6 |
                                      u32(m.offset) << 10 | (u32(m.frac) & 0x1Fu) << 18 | (m.le ? 1u : 0u) << 23 |
                                      u32(m.stride) << 24;
  }
  s_program[3] = pack_swap(c.tevSwapTable[0]) | pack_swap(c.tevSwapTable[1]) << 8 | pack_swap(c.tevSwapTable[2]) << 16 |
                 pack_swap(c.tevSwapTable[3]) << 24;
  // vec4u 1: the color channels in GXChannelID order (COLOR0, COLOR1, ALPHA0, ALPHA1). Bit 0 lighting on, 1 material
  // from the vertex, 2 ambient from the vertex, 4-5 diffuse (0 none, 1 sign, 2 clamp), 8-9 attenuation (0 none, 1 spot,
  // 2 specular)
  for (u32 i = 0; i < MaxColorChannels; ++i) {
    const auto& cc = c.colorChannels[i];
    const u32 diff = cc.diffFn == GX_DF_SIGN ? 1u : cc.diffFn == GX_DF_CLAMP ? 2u : 0u;
    const u32 attn = cc.attnFn == GX_AF_SPOT ? 1u : cc.attnFn == GX_AF_SPEC ? 2u : 0u;
    s_program[4 + i] = (cc.lightingEnabled ? 1u : 0u) | (cc.matSrc == GX_SRC_VTX ? 2u : 0u) |
                       (cc.ambSrc == GX_SRC_VTX ? 4u : 0u) | diff << 4 | attn << 8;
  }
  // vec4u 2-3: the texgens. Bit 0 on, 1-2 type (0 MTX3x4, 1 MTX2x4, 2 SRTG, 3 emboss), 3-7 source (GXTexGenSrc), 8
  // identity matrix, 9-13 matrix slot (mtx / 3), 14 normalize, 15 identity post-matrix, 16-20 post-matrix slot;
  // emboss: 21-23 the light (BUMP0..7), 24-26 the source texcoord
  for (u32 i = 0; i < MaxTexCoord; ++i) {
    const auto& tcg = c.tcgs[i];
    if (tcg.src == GX_MAX_TEXGENSRC) {
      continue;
    }
    if (tcg.type >= GX_TG_BUMP0 && tcg.type <= GX_TG_BUMP7) {
      s_program[8 + i] = 1u | 3u << 1 | (u32(tcg.type - GX_TG_BUMP0) & 7u) << 21 | (u32(tcg.embossSrc) & 7u) << 24;
      continue;
    }
    const u32 type = tcg.type == GX_TG_MTX3x4 ? 0u : tcg.type == GX_TG_MTX2x4 ? 1u : 2u;
    u32 w = 1u | type << 1 | (u32(tcg.src) & 0x1Fu) << 3;
    if (tcg.mtx == GX_IDENTITY) {
      w |= 1u << 8;
    } else {
      w |= (u32(tcg.mtx) / 3u & 0x1Fu) << 9;
    }
    if (tcg.normalize) {
      w |= 1u << 14;
    }
    if (tcg.postMtx == GX_PTIDENTITY) {
      w |= 1u << 15;
    } else {
      w |= ((u32(tcg.postMtx) - GX_PTTEXMTX0) / 3u & 0x1Fu) << 16;
    }
    s_program[8 + i] = w;
  }
  for (u32 i = 0; i < c.tevStageCount; ++i) {
    const auto& st = c.tevStages[i];
    u32* w = &s_program[4 * UberStageBase + 4 * i];
    w[0] = (u32(st.colorPass.a) & 0xFu) | (u32(st.colorPass.b) & 0xFu) << 4 | (u32(st.colorPass.c) & 0xFu) << 8 |
           (u32(st.colorPass.d) & 0xFu) << 12 | (u32(st.alphaPass.a) & 7u) << 16 | (u32(st.alphaPass.b) & 7u) << 19 |
           (u32(st.alphaPass.c) & 7u) << 22 | (u32(st.alphaPass.d) & 7u) << 25;
    w[1] = pack_op(st.colorOp) | pack_op(st.alphaOp) << 16;
    // indirect (the 0.2 % step): w2 bits 20-21 indirect stage, 22-23 format, 24-26 bias select (S, T, U bits),
    // 27-30 matrix (GXIndTexMtxID), 31 add previous; w3 bits 24-26 / 27-29 wrap S / T (GXIndTexWrap)
    w[2] = (u32(st.kcSel) & 0x1Fu) | (u32(st.kaSel) & 0x1Fu) << 8 | (u32(st.tevSwapRas) & 3u) << 16 |
           (u32(st.tevSwapTex) & 3u) << 18 | (u32(st.indTexStage) & 3u) << 20 | (u32(st.indTexFormat) & 3u) << 22 |
           (u32(st.indTexBiasSel) & 7u) << 24 | (u32(st.indTexMtxId) & 0xFu) << 27 | (st.indTexAddPrev ? 1u : 0u) << 31;
    w[3] = (u32(st.texMapId) & 0xFFu) | (u32(st.texCoordId) & 0xFFu) << 8 | (u32(st.channelId) & 0xFFu) << 16 |
           (u32(st.indTexWrapS) & 7u) << 24 | (u32(st.indTexWrapT) & 7u) << 27;
  }
  // the indirect stages, in the three words after the 21 attribute words (vec4u UberAttrBase + 5, .y .z .w): two
  // stages per word (16 bits each: texcoord 0-2, texture 3-5, scale S 6-9, scale T 10-13), then the stage count (0-2)
  // and whether any TEV stage uses an indirect stage (3; the generator's usedIndStages.any())
  bool anyInd = false;
  for (u32 i = 0; i < c.tevStageCount; ++i) {
    if (c.tevStages[i].indTexMtxId != GX_ITM_OFF && c.tevStages[i].indTexStage < c.numIndStages) {
      anyInd = true;
    }
  }
  const u32 indBase = 4 * UberAttrBase + 21;
  for (u32 k = 0; k < c.numIndStages; ++k) {
    const auto& is = c.indStages[k];
    const u32 half = (u32(is.texCoordId) & 7u) | (u32(is.texMapId) & 7u) << 3 | (u32(is.scaleS) & 0xFu) << 6 |
                     (u32(is.scaleT) & 0xFu) << 10;
    s_program[indBase + k / 2] |= half << (16 * (k % 2));
    s_texMask |= 1u << (u32(is.texMapId) & 7u);
  }
  s_program[indBase + 2] = (c.numIndStages & 7u) | (anyInd ? 8u : 0u);
}

void uber_normalize(ShaderConfig& c) noexcept {
  c.tevSwapTable = {};
  c.tevStages = {};
  c.tevStageCount = 0;
  c.alphaCompare = {};
  c.indStages = {};
  c.numIndStages = 0;
  c.fogType = GX_FOG_NONE;
  c.fogRangeEnabled = false;
  c.colorChannels = {}; // stage 2: interpreted in the vertex stage
  c.tcgs = {};
  c.attrs = {}; // stage 3: the vertex format is read from the program
  c.vtxStride = 0;
  c.pad2 = UberShaderMarker;
}

// What build_pipeline / to_blend_state actually read (gx.cpp, verified 2026-11-11): GX_BM_NONE and GX_BM_SUBTRACT use
// neither the factors nor the logic op, GX_BM_BLEND uses the factors but not the op, GX_BM_LOGIC the op but not the
// factors; the depth function only when depthCompare is on. create_pipeline's source-alpha check reads the factors
// only for GX_BM_BLEND. the reference playthrough's 53 pipelines become 44 with this.
void uber_canonicalize_state(PipelineConfig& c) noexcept {
  switch (c.blendMode) {
  case GX_BM_NONE:
  case GX_BM_SUBTRACT:
    c.blendFacSrc = GX_BL_ZERO;
    c.blendFacDst = GX_BL_ZERO;
    c.blendOp = GX_LO_CLEAR;
    break;
  case GX_BM_BLEND:
    c.blendOp = GX_LO_CLEAR;
    break;
  case GX_BM_LOGIC:
    c.blendFacSrc = GX_BL_ZERO;
    c.blendFacDst = GX_BL_ZERO;
    break;
  default:
    break;
  }
  if (!c.depthCompare) {
    c.depthFunc = GX_NEVER;
  }
}

const std::array<u32, UberProgramVec4s * 4>& uber_program() noexcept { return s_program; }

// The interpreter, appended to the fragment function after the generator's prologue (which declares prev, tevreg0-2,
// rast0-1 and tex<n>_uv for the superset ShaderInfo that UberShaderMarker selects in build_shader_info).
std::string uber_fragment_source(const ShaderInfo& info) noexcept {
  std::string uvCases;
  for (u32 i = 0; i < MaxTexCoord; ++i) {
    if (info.sampledTexCoords.test(i)) {
      uvCases += fmt::format("\n            case {0}u: {{ uv = tex{0}_uv; }}", i);
    }
  }
  std::string texCases;
  for (u32 i = 0; i < MaxTextures; ++i) {
    texCases += fmt::format(
        "\n            case {0}u: {{ smp = textureSampleBias(tex{0}, tex{0}_samp, uv, ubuf.tex{0}_size_bias.z); }}", i);
  }
  const std::string_view fogDepth = UseReversedZ ? "(1.0 - in.pos.z)" : "in.pos.z";
  return fmt::format(R"""(
    // --- uber-fragment interpreter (OrrePort trial) ---
    var regs = array<vec4f, 4>(prev, tevreg0, tevreg1, tevreg2);
    var nc = array<bool, 4>(false, false, false, false);
    var na = array<bool, 4>(false, false, false, false);
    var kc = array<vec4f, 4>(ubuf.kcolor0, ubuf.kcolor1, ubuf.kcolor2, ubuf.kcolor3);
    let hdr = ubuf.uber_prog[0];
    let stageCount = hdr.x;
    // indirect texturing (the 0.2 % step): each indirect stage's texture sampled once, as the generator's
    // "Indirect stage" lines do (t_IndTexCoord)
    var uber_sb = array<vec4f, 8>(ubuf.tex0_size_bias, ubuf.tex1_size_bias, ubuf.tex2_size_bias, ubuf.tex3_size_bias,
                                  ubuf.tex4_size_bias, ubuf.tex5_size_bias, ubuf.tex6_size_bias, ubuf.tex7_size_bias);
    let uber_iw = ubuf.uber_prog[{4}];
    let uber_numInd = uber_iw.w & 7u;
    let uber_anyInd = (uber_iw.w & 8u) != 0u;
    var uber_indc: array<vec3f, 4>;
    var uber_tcoord = vec2f(0.0);
    for (var k = 0u; k < uber_numInd; k = k + 1u) {{
        let iw = (uber_iw[1u + k / 2u] >> (16u * (k % 2u))) & 0xFFFFu;
        let tcI = iw & 7u;
        let tmI = (iw >> 3u) & 7u;
        var uv = vec2f(0.0);
        switch (tcI) {{{0}
            default: {{}}
        }}
        uv = uv * ubuf.texcoord_scale[tcI].xy * vec2f(uber_pow2neg((iw >> 6u) & 0xFu), uber_pow2neg((iw >> 10u) & 0xFu))
             / uber_sb[tmI].xy;
        var smp = vec4f(0.0);
        switch (tmI) {{{1}
            default: {{}}
        }}
        uber_indc[k] = 255.0 * smp.abg;
    }}
    var lastColorReg = 0u;
    var lastAlphaReg = 0u;
    for (var s = 0u; s < stageCount; s = s + 1u) {{
        let w0 = ubuf.uber_prog[{3}u + s];
        let cargs = w0.x;
        let cop = w0.y & 0xFFFFu;
        let aop = w0.y >> 16u;
        let kcSel = w0.z & 0x1Fu;
        let kaSel = (w0.z >> 8u) & 0x1Fu;
        let swapRas = (hdr.w >> (8u * ((w0.z >> 16u) & 3u))) & 0xFFu;
        let swapTex = (hdr.w >> (8u * ((w0.z >> 18u) & 3u))) & 0xFFu;
        let texMap = w0.w & 0xFFu;
        let texCoord = (w0.w >> 8u) & 0xFFu;
        let chan = (w0.w >> 16u) & 0xFFu;

        var uv = vec2f(0.0);
        switch (texCoord) {{{0}
            default: {{}}
        }}
        // this stage's indirect coordinate (the generator's "TEV stage N indirect" block)
        let iMtx = (w0.z >> 27u) & 0xFu;
        let iAdd = (w0.z >> 31u) == 1u;
        let iWrapS = (w0.w >> 24u) & 7u;
        let iWrapT = (w0.w >> 27u) & 7u;
        if (iMtx != 0u || iWrapS != 0u || iWrapT != 0u || iAdd) {{
            let hasBase = texCoord != 0xFFu && texMap != 0xFFu;
            let iStage = (w0.z >> 20u) & 3u;
            let iFmt = (w0.z >> 22u) & 3u;
            var off = vec2f(0.0);
            var hasOff = false;
            if (iMtx != 0u && iStage < uber_numInd) {{
                var c = uber_indc[iStage];
                switch (iFmt) {{
                    case 1u: {{ c = floor(c / 8.0); }}
                    case 2u: {{ c = floor(c / 16.0); }}
                    case 3u: {{ c = floor(c / 32.0); }}
                    default: {{}}
                }}
                let bsel = (w0.z >> 24u) & 7u;
                if (bsel != 0u) {{
                    c = c + vec3f(f32(bsel & 1u), f32((bsel >> 1u) & 1u), f32((bsel >> 2u) & 1u)) *
                            select(1.0, -128.0, iFmt == 0u);
                }}
                if (iMtx >= 1u && iMtx <= 3u) {{
                    let c0 = ubuf.ind_mtx[iMtx - 1u][0];
                    let c1 = ubuf.ind_mtx[iMtx - 1u][1];
                    off = vec2f(dot(vec3f(c0.xz, c1.x), c), dot(vec3f(c0.yw, c1.y), c)) * c1.z;
                    hasOff = true;
                }} else if (iMtx >= 5u && iMtx <= 7u && hasBase) {{
                    off = uv * ubuf.texcoord_scale[texCoord].xy * c.x * ubuf.ind_mtx[iMtx - 5u][1][2] / 256.0;
                    hasOff = true;
                }} else if (iMtx >= 9u && iMtx <= 11u && hasBase) {{
                    off = uv * ubuf.texcoord_scale[texCoord].xy * c.y * ubuf.ind_mtx[iMtx - 9u][1][2] / 256.0;
                    hasOff = true;
                }}
            }}
            let simple = iMtx == 0u && !iAdd;
            var fin = vec2f(0.0);
            var hasFin = false;
            if (hasBase) {{
                var base = uv;
                if (!simple) {{ base = uv * ubuf.texcoord_scale[texCoord].xy; }}
                if (iWrapS != 0u || iWrapT != 0u) {{
                    base = vec2f(uber_wrap(base.x, iWrapS), uber_wrap(base.y, iWrapT));
                }}
                fin = base;
                if (hasOff) {{ fin = base + (off); }}
                hasFin = true;
            }} else if (hasOff) {{
                fin = off;
                hasFin = true;
            }}
            if (uber_anyInd && hasFin) {{
                if (iAdd) {{ uber_tcoord += fin; }} else {{ uber_tcoord = fin; }}
                if (texMap != 0xFFu) {{
                    if (simple) {{ uv = uber_tcoord; }} else {{ uv = uber_tcoord / uber_sb[texMap].xy; }}
                }}
            }}
        }}
        var smp = vec4f(1.0);
        if (texMap != 0xFFu) {{
            switch (texMap) {{{1}
                default: {{}}
            }}
            smp = uber_swz(smp, swapTex);
        }}
        var ras = vec4f(0.0);
        if (chan == 0u || chan == 2u || chan == 4u) {{
            ras = uber_swz(rast0, swapRas);
        }} else if (chan == 1u || chan == 3u || chan == 5u) {{
            ras = uber_swz(rast1, swapRas);
        }}
        let kcv = uber_kc(kcSel, &kc);
        let kav = uber_ka(kaSel, &kc);

        let ca = uber_carg(cargs & 0xFu, &regs, &nc, &na, smp, ras, kcv, true);
        let cb = uber_carg((cargs >> 4u) & 0xFu, &regs, &nc, &na, smp, ras, kcv, true);
        let cc = uber_carg((cargs >> 8u) & 0xFu, &regs, &nc, &na, smp, ras, kcv, true);
        let cd = uber_carg((cargs >> 12u) & 0xFu, &regs, &nc, &na, smp, ras, kcv, false);
        let cOut = (cop >> 9u) & 3u;
        let cRes = uber_op3(cop & 0xFu, (cop >> 4u) & 3u, (cop >> 6u) & 3u, ((cop >> 8u) & 1u) == 1u, ca, cb, cc, cd);
        regs[cOut] = vec4f(cRes, regs[cOut].a);
        nc[cOut] = ((cop >> 8u) & 1u) == 1u;

        let aa = uber_aarg((cargs >> 16u) & 7u, &regs, &na, smp, ras, kav, true);
        let ab = uber_aarg((cargs >> 19u) & 7u, &regs, &na, smp, ras, kav, true);
        let ac = uber_aarg((cargs >> 22u) & 7u, &regs, &na, smp, ras, kav, true);
        let ad = uber_aarg((cargs >> 25u) & 7u, &regs, &na, smp, ras, kav, false);
        let aOut = (aop >> 9u) & 3u;
        let aRes = uber_op1(aop & 0xFu, (aop >> 4u) & 3u, (aop >> 6u) & 3u, ((aop >> 8u) & 1u) == 1u, aa, ab, ac, ad, ca, cb);
        regs[aOut].a = aRes;
        na[aOut] = ((aop >> 8u) & 1u) == 1u;
        lastColorReg = cOut;
        lastAlphaReg = aOut;
    }}
    prev = regs[0];
    if (lastColorReg != 0u) {{ prev = vec4f(regs[lastColorReg].rgb, prev.a); }}
    if (lastAlphaReg != 0u) {{ prev.a = regs[lastAlphaReg].a; }}
    let prevColorNormalized = nc[lastColorReg];
    let prevAlphaNormalized = na[lastAlphaReg];

    let fogType = (ubuf.uber_prog[0].z >> 8u) & 0xFFu;
    if (fogType != 0u) {{
        let fogDepth = {2};
        var fogBase: f32;
        if ((fogType & 8u) != 0u) {{
            fogBase = ubuf.fog.a * fogDepth;
        }} else {{
            fogBase = ubuf.fog.a / (ubuf.fog.b - fogDepth);
        }}
        var fogF = clamp(fogBase - ubuf.fog.c, 0.0, 1.0);
        var fogZ: f32;
        switch (fogType & 7u) {{
            case 4u: {{ fogZ = 1.0 - exp2(-8.0 * fogF); }}
            case 5u: {{ fogZ = 1.0 - exp2(-8.0 * fogF * fogF); }}
            case 6u: {{ fogZ = exp2(-8.0 * (1.0 - fogF)); }}
            case 7u: {{ fogF = 1.0 - fogF; fogZ = exp2(-8.0 * fogF * fogF); }}
            default: {{ fogZ = fogF; }}
        }}
        prev = vec4f(mix(prev.rgb, ubuf.fog.color.rgb, clamp(fogZ, 0.0, 1.0)), prev.a);
    }}
    if (!prevColorNormalized && !prevAlphaNormalized) {{
        prev = tev_overflow_vec4f(prev);
    }} else if (!prevColorNormalized) {{
        prev = vec4f(tev_overflow_vec3f(prev.rgb), prev.a);
    }} else if (!prevAlphaNormalized) {{
        prev.a = tev_overflow_f32(prev.a);
    }}
    let acw = ubuf.uber_prog[0].y;
    let comp0 = acw & 7u;
    let comp1 = (acw >> 16u) & 7u;
    if (comp0 != 7u || comp1 != 7u) {{
        let alphaCompare = u32(round(clamp(prev.a, 0.0, 1.0) * 255.0));
        let p0 = uber_acmp(comp0, (acw >> 8u) & 0xFFu, alphaCompare);
        let p1 = uber_acmp(comp1, (acw >> 24u) & 0xFFu, alphaCompare);
        var acPass: bool;
        switch (ubuf.uber_prog[0].z & 3u) {{
            case 0u: {{ acPass = p0 && p1; }}
            case 1u: {{ acPass = p0 || p1; }}
            case 2u: {{ acPass = p0 != p1; }}
            default: {{ acPass = p0 == p1; }}
        }}
        if (!acPass) {{ discard; }}
    }}
    // --- end of the interpreter ---)""",
                     uvCases, texCases, fogDepth, UberStageBase, UberAttrBase + 5);
}

// Stage 2, the vertex side. Placed where build_shader_source writes its color channels and texgens (after mv_pos and
// mv_nrm). Channels: lighting_func's rules, run for the color and the alpha half of each channel (the color result's
// alpha is replaced by the alpha half's, as the generated pair of assignments does). Texgens: the texgen loop's rules;
// every texgen writes tex<n>_uvw, and the fragment divides by w only for MTX3x4 (uber_texcoord_source).
// The four channel halves and the eight texgens run as loops, so each helper (uber_light, the texgen body) appears
// once in the shader instead of once per channel or texgen (U7, 2026-11-10: the unrolled form took 7.4 s to build cold).
// Reads the stage 3 fetch's uber_av (POS, NRM, CLR0, CLR1, TEX0..7) and uber_mtxidx arrays.
std::string uber_vertex_source(u32 lineMode) noexcept {
  // line / point texcoord offsets, as the generator writes them after each texgen's projection
  std::string lineOffset;
  if (lineMode == 3) {
    lineOffset = "\n        if ((ubuf.line_texcoord_mask & (1u << i)) != 0u) {"
                 "\n            if ((vidx & 1u) != 0u) { proj.x += ubuf.line_tex_offset; }"
                 "\n            if (vidx >= 2u) { proj.y += ubuf.line_tex_offset; }"
                 "\n        }";
  } else if (lineMode != 0) {
    lineOffset = "\n        if ((ubuf.line_texcoord_mask & (1u << i)) != 0u && (vidx & 1u) != 0u) {"
                 "\n            proj.y += ubuf.line_tex_offset;"
                 "\n        }";
  }
  std::string out = fmt::format(R"""(
    // --- uber color channels and texgens (OrrePort trial, stages 2-3) ---
    let uber_ch = ubuf.uber_prog[1];
    var uber_ls = array<u32, 4>(ubuf.lightState0, ubuf.lightState1, ubuf.lightState0a, ubuf.lightState1a);
    var uber_amb = array<vec4f, 4>(ubuf.cc0_amb, ubuf.cc1_amb, ubuf.cc0a_amb, ubuf.cc1a_amb);
    var uber_mat = array<vec4f, 4>(ubuf.cc0_mat, ubuf.cc1_mat, ubuf.cc0a_mat, ubuf.cc1a_mat);
    var uber_cc: array<vec4f, 4>;
    for (var c = 0u; c < 4u; c = c + 1u) {{
        let cw = uber_ch[c];
        let vc = uber_av[2u + (c & 1u)];
        uber_cc[c] = uber_light(cw, uber_ls[c], select(uber_amb[c], vc, (cw & 4u) != 0u),
                                select(uber_mat[c], vc, (cw & 2u) != 0u), mv_pos, mv_nrm);
    }}
    out.cc0 = vec4f(uber_cc[0].rgb, uber_cc[2].a);
    out.cc1 = vec4f(uber_cc[1].rgb, uber_cc[3].a);
    var uber_tcs: array<vec4f, 8>;
    var uber_uvw: array<vec3f, 8>;
    for (var i = 0u; i < 8u; i = i + 1u) {{
        let tg = ubuf.uber_prog[2u + i / 4u][i % 4u];
        if ((tg & 1u) == 0u) {{ continue; }}
        if (((tg >> 1u) & 3u) == 3u) {{
            // emboss (the generator's is_emboss_texgen branch): the source texcoord's projection offset by the
            // light direction projected onto the tangent and binormal
            let bump_ldir = normalize(ubuf.lights[(tg >> 21u) & 7u].pos - mv_pos);
            let bump_tan = vec4f(uber_tangent, 0.0) * ubuf.nrm_mtx[in_pnmtxidx];
            let bump_bin = vec4f(uber_binrm, 0.0) * ubuf.nrm_mtx[in_pnmtxidx];
            uber_uvw[i] = vec3f(uber_uvw[(tg >> 24u) & 7u].xy +
                                vec2f(dot(bump_ldir, bump_tan), dot(bump_ldir, bump_bin)), 1.0);
            continue;
        }}
        let src = (tg >> 3u) & 0x1Fu;
        var tc = vec4f(0.0);
        if (src == {0}u) {{
            tc = vec4f(in_pos, 1.0);
        }} else if (src == {1}u) {{
            tc = vec4f(in_nrm, 1.0);
        }} else if (src >= {2}u && src <= {3}u) {{
            tc = vec4f(uber_av[4u + src - {2}u].xy, 1.0, 1.0);
        }} else if (src >= {4}u && src <= {5}u) {{
            tc = uber_tcs[src - {4}u];
        }} else if (src == {6}u) {{
            tc = out.cc0;
        }} else if (src == {7}u) {{
            tc = out.cc1;
        }}
        uber_tcs[i] = tc;
        let ty = (tg >> 1u) & 3u;
        var tmp: vec3f;
        if (ty == 2u) {{
            tmp = vec3f(tc.xy, 1.0f);
        }} else {{
            // a vertex carrying TEXnMTXIDX (a direct byte) picks the matrix per vertex, as the generated shader
            // does when indexAttr has it
            if ((uber_attr_word({8}u + i) & 3u) == 1u) {{
                tmp = tc * ubuf.postex_mtx[uber_mtxidx[i] / 3u];
            }} else if ((tg & 0x100u) != 0u) {{
                tmp = tc.xyz;
            }} else {{
                tmp = tc * ubuf.postex_mtx[(tg >> 9u) & 0x1Fu];
            }}
            if (ty == 1u) {{ tmp.z = 1.0f; }}
        }}
        if ((tg & 0x4000u) != 0u) {{ tmp = normalize(tmp); }}
        var proj = tmp;
        if ((tg & 0x8000u) == 0u) {{ proj = vec4f(tmp.xyz, 1.0) * ubuf.postmtx[(tg >> 16u) & 0x1Fu]; }}{9}
        uber_uvw[i] = proj;
    }})""",
                                u32(GX_TG_POS), u32(GX_TG_NRM), u32(GX_TG_TEX0), u32(GX_TG_TEX7),
                                u32(GX_TG_TEXCOORD0), u32(GX_TG_TEXCOORD6), u32(GX_TG_COLOR0), u32(GX_TG_COLOR1),
                                u32(GX_VA_TEX0MTXIDX), lineOffset);
  for (u32 i = 0; i < MaxTexCoord; ++i) {
    out += fmt::format("\n    out.tex{0}_uvw = uber_uvw[{0}];", i);
  }
  return out;
}

std::string uber_texcoord_source(u32 i) noexcept {
  return fmt::format("\n    var tex{0}_uv = select(in.tex{0}_uvw.xy, in.tex{0}_uvw.xy / in.tex{0}_uvw.z, "
                     "((ubuf.uber_prog[{1}][{2}] >> 1u) & 3u) == 0u);",
                     i, 2 + i / 4, i % 4);
}

// Helper functions, placed at module scope (after the overflow helpers the generator always emits).
std::string uber_helper_source() noexcept {
  return fmt::format(R"""(
// Stage 3: the vertex attributes, read as shader.cpp's attr_address / attr_load / fetch_attr / fetch_color_attr write
// them, the format coming from the attribute's program word (uber_encode_program)
fn uber_attr_word(a: u32) -> u32 {{
  return ubuf.uber_prog[{0}u + a / 4u][a % 4u];
}}
fn uber_attr_off(w: u32, vtxBase: u32, arrayStart: u32) -> u32 {{
  let dl = vtxBase + ((w >> 10u) & 0xFFu);
  let t = w & 3u;
  if (t == 2u) {{ return arrayStart + raw_fetch_u8_1(&vbuf, dl) * (w >> 24u); }}
  if (t == 3u) {{ return arrayStart + raw_fetch_u16_1(&vbuf, dl, false) * (w >> 24u); }}
  return dl;
}}
fn uber_fetch_n(p: ptr<storage, array<u32>>, off: u32, w: u32, le: bool) -> vec4f {{
  let cnt = (w >> 2u) & 0xFu;
  let frac = (w >> 18u) & 0x1Fu;
  switch ((w >> 6u) & 0xFu) {{
    case 0u: {{
      switch (cnt) {{
        case 1u: {{ return vec4f(fetch_u8_1(p, off, frac, le), 0.0, 0.0, 0.0); }}
        case 2u: {{ return vec4f(fetch_u8_2(p, off, frac, le), 0.0, 0.0); }}
        case 3u: {{ return vec4f(fetch_u8_3(p, off, frac, le), 0.0); }}
        default: {{ return fetch_u8_4(p, off, frac, le); }}
      }}
    }}
    case 1u: {{
      switch (cnt) {{
        case 1u: {{ return vec4f(fetch_s8_1(p, off, frac, le), 0.0, 0.0, 0.0); }}
        case 2u: {{ return vec4f(fetch_s8_2(p, off, frac, le), 0.0, 0.0); }}
        case 3u: {{ return vec4f(fetch_s8_3(p, off, frac, le), 0.0); }}
        default: {{ return fetch_s8_4(p, off, frac, le); }}
      }}
    }}
    case 2u: {{
      switch (cnt) {{
        case 1u: {{ return vec4f(fetch_u16_1(p, off, frac, le), 0.0, 0.0, 0.0); }}
        case 2u: {{ return vec4f(fetch_u16_1(p, off + 0u, frac, le), fetch_u16_1(p, off + 2u, frac, le), 0.0, 0.0); }}
        case 3u: {{ return vec4f(fetch_u16_1(p, off + 0u, frac, le), fetch_u16_1(p, off + 2u, frac, le),
                                 fetch_u16_1(p, off + 4u, frac, le), 0.0); }}
        default: {{ return fetch_u16_4(p, off, frac, le); }}
      }}
    }}
    case 3u: {{
      switch (cnt) {{
        case 1u: {{ return vec4f(fetch_s16_1(p, off, frac, le), 0.0, 0.0, 0.0); }}
        case 2u: {{ return vec4f(fetch_s16_1(p, off + 0u, frac, le), fetch_s16_1(p, off + 2u, frac, le), 0.0, 0.0); }}
        case 3u: {{ return vec4f(fetch_s16_1(p, off + 0u, frac, le), fetch_s16_1(p, off + 2u, frac, le),
                                 fetch_s16_1(p, off + 4u, frac, le), 0.0); }}
        default: {{ return fetch_s16_4(p, off, frac, le); }}
      }}
    }}
    case 4u: {{
      switch (cnt) {{
        case 1u: {{ return vec4f(fetch_f32_1(p, off, le), 0.0, 0.0, 0.0); }}
        case 2u: {{ return vec4f(fetch_f32_2(p, off, le), 0.0, 0.0); }}
        case 3u: {{ return vec4f(fetch_f32_3(p, off, le), 0.0); }}
        default: {{ return fetch_f32_4(p, off, le); }}
      }}
    }}
    default: {{ return unpack4x8unorm(load_u32_raw(p, off)); }}
  }}
}}
fn uber_fetch_clr(p: ptr<storage, array<u32>>, off: u32, w: u32, le: bool) -> vec4f {{
  switch ((w >> 6u) & 0xFu) {{
    case 0u: {{ return fetch_rgb565(p, off, le); }}
    case 1u: {{ return fetch_rgb8(p, off, le); }}
    case 2u: {{ return fetch_rgbx8(p, off, le); }}
    case 3u: {{ return fetch_rgba4(p, off, le); }}
    case 4u: {{ return fetch_rgba6(p, off, le); }}
    default: {{ return fetch_rgba8(p, off, le); }}
  }}
}}
// the binormal (slice 1) or tangent (slice 2) of an NBT normal, as attr_load_nbt_slice reads it: an NBT3 normal has
// one index per slice in the vertex (dlExtra), and each slice sits 3 components further on (within)
fn uber_load_nbt(slice: u32, w: u32, nbt3: bool, vtxBase: u32, arrayStart: u32) -> vec3f {{
  let comp = (w >> 6u) & 0xFu;
  var compsize = 4u;
  if (comp <= 1u) {{ compsize = 1u; }} else if (comp <= 3u) {{ compsize = 2u; }}
  let t = w & 3u;
  var dlExtra = 0u;
  if (nbt3) {{
    if (t == 2u) {{ dlExtra = slice; }} else if (t == 3u) {{ dlExtra = slice * 2u; }}
  }}
  let within = slice * 3u * compsize;
  let dl = vtxBase + ((w >> 10u) & 0xFFu) + dlExtra;
  if (t == 2u) {{
    return uber_fetch_n(&abuf, arrayStart + raw_fetch_u8_1(&vbuf, dl) * (w >> 24u) + within, w,
                        ((w >> 23u) & 1u) == 1u).xyz;
  }}
  if (t == 3u) {{
    return uber_fetch_n(&abuf, arrayStart + raw_fetch_u16_1(&vbuf, dl, false) * (w >> 24u) + within, w,
                        ((w >> 23u) & 1u) == 1u).xyz;
  }}
  return uber_fetch_n(&vbuf, dl + within, w, false).xyz;
}}
// line / point mode: one vertex's position-matrix index (attr_load's GX_VA_PNMTXIDX, or the current one)
fn uber_pnmtx(vtxBase: u32, current: u32) -> u32 {{
  let w = uber_attr_word(0u);
  if ((w & 3u) == 0u) {{ return current; }}
  return raw_fetch_u8_1(&vbuf, vtxBase + ((w >> 10u) & 0xFFu)) / 3u;
}}
// a direct attribute reads vbuf (big-endian, as attr_address passes le = false); an indexed one reads abuf with the
// array's own byte order
fn uber_load(w: u32, vtxBase: u32, arrayStart: u32, color: bool) -> vec4f {{
  let off = uber_attr_off(w, vtxBase, arrayStart);
  let indexed = (w & 3u) >= 2u;
  let le = indexed && ((w >> 23u) & 1u) == 1u;
  if (color) {{
    if (indexed) {{ return uber_fetch_clr(&abuf, off, w, le); }}
    return uber_fetch_clr(&vbuf, off, w, le);
  }}
  if (indexed) {{ return uber_fetch_n(&abuf, off, w, le); }}
  return uber_fetch_n(&vbuf, off, w, le);
}}
)""",
                     UberAttrBase) +
         R"""(
fn uber_swz(v: vec4f, s: u32) -> vec4f {
  return vec4f(v[s & 3u], v[(s >> 2u) & 3u], v[(s >> 4u) & 3u], v[(s >> 6u) & 3u]);
}
fn uber_kc(sel: u32, k: ptr<function, array<vec4f, 4>>) -> vec3f {
  if (sel < 8u) { return vec3f(f32(8u - sel) / 8.0); }
  if (sel < 0x10u) { return (*k)[sel - 12u].rgb; }
  return vec3f((*k)[(sel - 0x10u) % 4u][(sel - 0x10u) / 4u]);
}
fn uber_ka(sel: u32, k: ptr<function, array<vec4f, 4>>) -> f32 {
  if (sel < 8u) { return f32(8u - sel) / 8.0; }
  return (*k)[(sel - 0x10u) % 4u][(sel - 0x10u) / 4u];
}
fn uber_carg(arg: u32, regs: ptr<function, array<vec4f, 4>>, nc: ptr<function, array<bool, 4>>,
             na: ptr<function, array<bool, 4>>, smp: vec4f, ras: vec4f, kc: vec3f, wrap: bool) -> vec3f {
  var v: vec3f;
  var norm = true;
  if (arg < 8u) {
    let r = arg / 2u;
    if ((arg & 1u) == 0u) { v = (*regs)[r].rgb; norm = (*nc)[r]; }
    else { v = vec3f((*regs)[r].a); norm = (*na)[r]; }
  } else {
    switch (arg) {
      case 8u: { v = smp.rgb; }
      case 9u: { v = vec3f(smp.a); }
      case 10u: { v = ras.rgb; }
      case 11u: { v = vec3f(ras.a); }
      case 12u: { v = vec3f(1.0); }
      case 13u: { v = vec3f(0.5); }
      case 14u: { v = kc; }
      default: { v = vec3f(0.0); }
    }
  }
  if (wrap && !norm) { return tev_overflow_vec3f(v); }
  return v;
}
fn uber_aarg(arg: u32, regs: ptr<function, array<vec4f, 4>>, na: ptr<function, array<bool, 4>>, smp: vec4f,
             ras: vec4f, ka: f32, wrap: bool) -> f32 {
  var v: f32;
  var norm = true;
  switch (arg) {
    case 0u, 1u, 2u, 3u: { v = (*regs)[arg].a; norm = (*na)[arg]; }
    case 4u: { v = smp.a; }
    case 5u: { v = ras.a; }
    case 6u: { v = ka; }
    default: { v = 0.0; }
  }
  if (wrap && !norm) { return tev_overflow_f32(v); }
  return v;
}
fn uber_bias(b: u32) -> f32 {
  if (b == 1u) { return 0.5; }
  if (b == 2u) { return -0.5; }
  return 0.0;
}
fn uber_scale3(v: vec3f, s: u32) -> vec3f {
  if (s == 1u) { return v * 2.0; }
  if (s == 2u) { return v * 4.0; }
  if (s == 3u) { return v / 2.0; }
  return v;
}
fn uber_scale1(v: f32, s: u32) -> f32 {
  if (s == 1u) { return v * 2.0; }
  if (s == 2u) { return v * 4.0; }
  if (s == 3u) { return v / 2.0; }
  return v;
}
fn uber_op3(op: u32, bias: u32, scale: u32, clmp: bool, a: vec3f, b: vec3f, c: vec3f, d: vec3f) -> vec3f {
  var r: vec3f;
  switch (op) {
    case 0u: { r = uber_scale3((mix(a, b, c) + d) + uber_bias(bias), scale); }
    case 1u: { r = uber_scale3((-mix(a, b, c) + d) + uber_bias(bias), scale); }
    case 8u: { r = select(vec3(0.0), c, round(a.r * 255.0) > round(b.r * 255.0)) + d; }
    case 9u: { r = select(vec3(0.0), c, round(a.r * 255.0) == round(b.r * 255.0)) + d; }
    case 10u: { r = select(vec3(0.0), c, round(dot(a.rg * 255.0, vec2(1.0, 256.0))) > round(dot(b.rg * 255.0, vec2(1.0, 256.0)))) + d; }
    case 11u: { r = select(vec3(0.0), c, round(dot(a.rg * 255.0, vec2(1.0, 256.0))) == round(dot(b.rg * 255.0, vec2(1.0, 256.0)))) + d; }
    case 12u: { r = select(vec3(0.0), c, round(dot(a.rgb * 255.0, vec3(1.0, 256.0, 65536.0))) > round(dot(b.rgb * 255.0, vec3(1.0, 256.0, 65536.0)))) + d; }
    case 13u: { r = select(vec3(0.0), c, round(dot(a.rgb * 255.0, vec3(1.0, 256.0, 65536.0))) == round(dot(b.rgb * 255.0, vec3(1.0, 256.0, 65536.0)))) + d; }
    case 14u: { r = select(vec3(0.0), c, round(a * 255.0) > round(b * 255.0)) + d; }
    default: { r = select(vec3(0.0), c, round(a * 255.0) == round(b * 255.0)) + d; }
  }
  if (clmp) { return clamp(r, vec3f(0.0), vec3f(1.0)); }
  return clamp(r, vec3f(-4.0), vec3f(4.0));
}
fn uber_op1(op: u32, bias: u32, scale: u32, clmp: bool, a: f32, b: f32, c: f32, d: f32, ca: vec3f, cb: vec3f) -> f32 {
  var r: f32;
  switch (op) {
    case 0u: { r = uber_scale1((mix(a, b, c) + d) + uber_bias(bias), scale); }
    case 1u: { r = uber_scale1((-mix(a, b, c) + d) + uber_bias(bias), scale); }
    case 8u: { r = select(0.0, c, round(ca.r * 255.0) > round(cb.r * 255.0)) + d; }
    case 9u: { r = select(0.0, c, round(ca.r * 255.0) == round(cb.r * 255.0)) + d; }
    case 10u: { r = select(0.0, c, round(dot(ca.rg * 255.0, vec2(1.0, 256.0))) > round(dot(cb.rg * 255.0, vec2(1.0, 256.0)))) + d; }
    case 11u: { r = select(0.0, c, round(dot(ca.rg * 255.0, vec2(1.0, 256.0))) == round(dot(cb.rg * 255.0, vec2(1.0, 256.0)))) + d; }
    case 12u: { r = select(0.0, c, round(dot(ca.rgb * 255.0, vec3(1.0, 256.0, 65536.0))) > round(dot(cb.rgb * 255.0, vec3(1.0, 256.0, 65536.0)))) + d; }
    case 13u: { r = select(0.0, c, round(dot(ca.rgb * 255.0, vec3(1.0, 256.0, 65536.0))) == round(dot(cb.rgb * 255.0, vec3(1.0, 256.0, 65536.0)))) + d; }
    case 14u: { r = select(0.0, c, round(a * 255.0) > round(b * 255.0)) + d; }
    default: { r = select(0.0, c, round(a * 255.0) == round(b * 255.0)) + d; }
  }
  if (clmp) { return clamp(r, 0.0, 1.0); }
  return clamp(r, -4.0, 4.0);
}
// indirect: 2^-s exactly (the generator writes the scale as the constant 1.0 / 2^s)
fn uber_pow2neg(s: u32) -> f32 {
  return bitcast<f32>((127u - s) << 23u);
}
// indirect wrap (GXIndTexWrap: 1 = 256 ... 5 = 16, 6 = 0), as the generator's wrap_comp
fn uber_wrap(v: f32, w: u32) -> f32 {
  switch (w) {
    case 1u: { return v % 256.0; }
    case 2u: { return v % 128.0; }
    case 3u: { return v % 64.0; }
    case 4u: { return v % 32.0; }
    case 5u: { return v % 16.0; }
    case 6u: { return 0.0; }
    default: { return v; }
  }
}
// Stage 2: one color channel half (lighting_func's rules). cw: the channel word (see uber_encode_program)
fn uber_light(cw: u32, mask: u32, amb: vec4f, mat: vec4f, mvPos: vec3f, mvNrm: vec3f) -> vec4f {
  if ((cw & 1u) == 0u) { return mat; }
  let diffFn = (cw >> 4u) & 3u;
  let attnFn = (cw >> 8u) & 3u;
  var lighting = amb;
  for (var i = 0u; i < 8u; i++) {
    if ((mask & (1u << i)) == 0u) { continue; }
    var light = ubuf.lights[i];
    var ldir = light.pos - mvPos;
    var dist2 = dot(ldir, ldir);
    var dist = sqrt(dist2);
    ldir = ldir / dist;
    var attn: f32;
    if (attnFn == 0u) {
      attn = 1.0;
    } else if (attnFn == 1u) {
      var cosine = max(0.0, dot(ldir, light.dir));
      var cos_attn = dot(light.cos_att, vec3f(1.0, cosine, cosine * cosine));
      var dist_attn = dot(light.dist_att, vec3f(1.0, dist, dist2));
      attn = max(0.0, cos_attn / dist_attn);
    } else {
      attn = select(0.0, max(0.0, dot(mvNrm, light.dir)), dot(mvNrm, ldir) >= 0.0);
      var cos_attn = dot(light.cos_att, vec3f(1.0, attn, attn * attn));
      var dist_attn: f32;
      if (diffFn != 0u) {
        dist_attn = max(0.0, dot(normalize(light.dist_att), vec3f(1.0, attn, attn * attn)));
      } else {
        dist_attn = max(0.0, dot(light.dist_att, vec3f(1.0, attn, attn * attn)));
      }
      attn = max(0.0, cos_attn / dist_attn);
    }
    var diff: f32;
    if (diffFn == 0u) {
      diff = 1.0;
    } else if (diffFn == 1u) {
      diff = dot(ldir, mvNrm);
    } else {
      diff = max(0.0, dot(ldir, mvNrm));
    }
    lighting = lighting + (attn * diff * light.color);
  }
  return mat * clamp(lighting, vec4f(0.0), vec4f(1.0));
}
fn uber_acmp(comp: u32, ref_: u32, a: u32) -> bool {
  switch (comp) {
    case 0u: { return false; }
    case 1u: { return a < ref_; }
    case 2u: { return a == ref_; }
    case 3u: { return a <= ref_; }
    case 4u: { return a > ref_; }
    case 5u: { return a != ref_; }
    case 6u: { return a >= ref_; }
    default: { return true; }
  }
}
)""";
}

} // namespace aurora::gx
