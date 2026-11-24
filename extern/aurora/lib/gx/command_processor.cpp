#include "command_processor.hpp"

#include "../gfx/frame.hpp"
#include "../gfx/pipeline_cache.hpp"
#include "../gfx/recording.hpp"
#include "../internal.hpp"
#include "dolphin/gd/GDGeometry.h"
#include "dolphin/gx/GXAurora.h"
#include "gx.hpp"
#include "pipeline.hpp"
#include "readback.hpp"
#include "regs.hpp"
#include "shader_info.hpp"
#include "texture.hpp"

#include <fmt/format.h>
#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring> // OrrePort, round 11: std::memcpy (a 2D piece's float positions)
#include <span>
#include <string> // OrrePort, fix (3)'s draw log
#include <vector>

namespace aurora::gx::fifo {
namespace {
constexpr Module Log{"aurora::gx::fifo"};

u16 prepare_idx_buffer(ByteBuffer& buf, GXPrimitive prim, u16 vtxStart, u16 vtxCount) noexcept {
  u16 numIndices = 0;
  if (prim == GX_QUADS) {
    buf.reserve_extra((vtxCount / 4) * 6 * sizeof(u16));

    for (u16 v = 0; v < vtxCount; v += 4) {
      u16 idx0 = vtxStart + v;
      u16 idx1 = vtxStart + v + 1;
      u16 idx2 = vtxStart + v + 2;
      u16 idx3 = vtxStart + v + 3;

      buf.append(idx0);
      buf.append(idx1);
      buf.append(idx2);
      numIndices += 3;

      buf.append(idx2);
      buf.append(idx3);
      buf.append(idx0);
      numIndices += 3;
    }
  } else if (prim == GX_TRIANGLES) {
    buf.reserve_extra(vtxCount * sizeof(u16));
    for (u16 v = 0; v < vtxCount; ++v) {
      const u16 idx = vtxStart + v;
      buf.append(idx);
      ++numIndices;
    }
  } else if (prim == GX_TRIANGLEFAN) {
    buf.reserve_extra(((u32(vtxCount) - 3) * 3 + 3) * sizeof(u16));
    for (u16 v = 0; v < vtxCount; ++v) {
      const u16 idx = vtxStart + v;
      if (v < 3) {
        buf.append(idx);
        ++numIndices;
        continue;
      }
      buf.append(std::array{vtxStart, static_cast<u16>(idx - 1), idx});
      numIndices += 3;
    }
  } else if (prim == GX_TRIANGLESTRIP) {
    buf.reserve_extra(((static_cast<u32>(vtxCount) - 3) * 3 + 3) * sizeof(u16));
    for (u16 v = 0; v < vtxCount; ++v) {
      const u16 idx = vtxStart + v;
      if (v < 3) {
        buf.append(idx);
        ++numIndices;
        continue;
      }
      if ((v & 1) == 0) {
        buf.append(std::array{static_cast<u16>(idx - 2), static_cast<u16>(idx - 1), idx});
      } else {
        buf.append(std::array{static_cast<u16>(idx - 1), static_cast<u16>(idx - 2), idx});
      }
      numIndices += 3;
    }
  } else if (prim == GX_LINES || prim == GX_LINESTRIP || prim == GX_POINTS) {
    buf.reserve_extra(6 * sizeof(u16));
    buf.append<u16>(0);
    buf.append<u16>(1);
    buf.append<u16>(3);
    buf.append<u16>(3);
    buf.append<u16>(2);
    buf.append<u16>(0);
    numIndices = 6;
  } else
    UNLIKELY FATAL("unsupported primitive type {}", static_cast<u32>(prim));
  return numIndices;
}

// GX FIFO opcodes - use CP_ prefix to avoid clashing with GXCommandList.h macros
constexpr u8 CP_CMD_NOP = GX_NOP;
constexpr u8 CP_CMD_LOAD_CP_REG = GX_LOAD_CP_REG;
constexpr u8 CP_CMD_LOAD_XF_REG = GX_LOAD_XF_REG;
constexpr u8 CP_CMD_LOAD_INDX_A = GX_LOAD_INDX_A;
constexpr u8 CP_CMD_LOAD_INDX_B = GX_LOAD_INDX_B;
constexpr u8 CP_CMD_LOAD_INDX_C = GX_LOAD_INDX_C;
constexpr u8 CP_CMD_LOAD_INDX_D = GX_LOAD_INDX_D;
constexpr u8 CP_CMD_CALL_DL = GX_CMD_CALL_DL;
constexpr u8 CP_CMD_INVAL_VTX = GX_CMD_INVL_VC;
constexpr u8 CP_CMD_LOAD_BP_REG = GX_LOAD_BP_REG & GX_OPCODE_MASK;

// Primitive type mask
constexpr u8 CP_OPCODE_MASK = GX_OPCODE_MASK;
constexpr u8 CP_VAT_MASK = GX_VAT_MASK;

struct FogRangeLutKey {
  std::array<u16, 10> rangeK;
  f32 rangeCenter;
  f32 renderWidth;
  u32 targetWidth;

  bool operator==(const FogRangeLutKey&) const = default;
};

struct FogRangeLutEntry {
  FogRangeLutKey key;
  std::vector<f32> factors;
};

constexpr size_t MaxFogRangeLuts = 32;
std::vector<FogRangeLutEntry> sFogRangeLuts;

struct DrawCache {
  uint64_t targetLayoutKey = 0;
  PipelineConfig config{};
  ShaderInfo shaderInfo{};
  gfx::PipelineRef pipelineRef{};
  GXBindGroups bindGroups{};
  uint64_t bindGeneration = 0;
  GXVtxFmt fmt = GX_MAX_VTXFMT;
  u8 lineMode = 0;
  bool hasPipeline = false;
  gfx::Range uniformRange{};
  gfx::Range fogRange{};
  FogRangeLutKey fogRangeKey{};
  bool hasFogRange = false;
  GXVtxFmt lastDrawFmt = GX_MAX_VTXFMT;
  // OrrePort (the hybrid ubershader, 2026-11-11): this setup is drawn by the ubershader only while its specialized
  // pipeline compiles; checked again at the next frame
  bool uberFallback = false;
  uint32_t uberFallbackFrame = 0;
};
DrawCache sDrawCache;
u32 sUberTexMask = 0; // OrrePort trial (ubershader.cpp): the maps the bound uber program samples
static aurora::Module UberLog("aurora::gx::uber");

FogRangeLutKey fog_range_lut_key() noexcept {
  const auto& state = g_gxState.fog;
  const f32 logicalWidth = std::max(g_gxState.logicalViewport.width, 1.f);
  const f32 renderWidth = std::max(g_gxState.renderViewport.width, 1.f);
  return {
      .rangeK = state.rangeK,
      .rangeCenter = ((static_cast<f32>(state.rangeCenter) - g_gxState.logicalViewport.left) / logicalWidth) * 2.f -
                     1.f + (g_gxState.renderViewport.left / renderWidth) * 2.f,
      .renderWidth = renderWidth,
      .targetWidth = gfx::get_render_target_size().x,
  };
}

std::vector<f32> build_fog_range_lut(const FogRangeLutKey& key) {
  std::array<f32, 10> rangeK;
  for (u32 i = 0; i < rangeK.size(); ++i) {
    const u32 source = (i & ~1u) | (1u - (i & 1u));
    rangeK[i] = static_cast<f32>(key.rangeK[source]) / 64.f;
  }

  std::vector<f32> lut(key.targetWidth);
  for (u32 x = 0; x < key.targetWidth; ++x) {
    const f32 screenX = ((static_cast<f32>(x) + 0.5f) / key.renderWidth) * 2.f - 1.f;
    const f32 offset = screenX - key.rangeCenter;
    const f32 rangeIndex = std::clamp(9.f - std::abs(offset) * 9.f, 0.f, 9.f);
    const u32 lower = static_cast<u32>(rangeIndex);
    const u32 upper = std::min(lower + 1, 9u);
    const f32 fraction = rangeIndex - static_cast<f32>(lower);
    const f32 k = std::max(rangeK[lower] * (1.f - fraction) + rangeK[upper] * fraction, 0.000001f);
    lut[x] = std::sqrt(offset * offset + k * k) / k;
  }
  return lut;
}

const std::vector<f32>& resolve_fog_range_lut(const FogRangeLutKey& key) {
  for (const auto& entry : sFogRangeLuts) {
    if (entry.key == key) {
      return entry.factors;
    }
  }
  if (sFogRangeLuts.size() == MaxFogRangeLuts) {
    sFogRangeLuts.erase(sFogRangeLuts.begin());
  }
  sFogRangeLuts.emplace_back(FogRangeLutEntry{key, build_fog_range_lut(key)});
  return sFogRangeLuts.back().factors;
}

gfx::Range push_fog_range_lut(const FogRangeLutKey& key) {
  const auto& lut = resolve_fog_range_lut(key);
  return gfx::push_storage(reinterpret_cast<const u8*>(lut.data()), lut.size() * sizeof(f32));
}

u8 line_mode_for_prim(GXPrimitive prim) noexcept {
  switch (prim) {
  case GX_LINES:
    return 1;
  case GX_LINESTRIP:
    return 2;
  case GX_POINTS:
    return 3;
  default:
    return 0;
  }
}
} // namespace

static void handle_draw(u8 cmd, ByteReader& reader) noexcept;
static void handle_aurora(ByteReader& reader) noexcept;

ProcessResult process(const u8* data, u32 size) noexcept {
  ZoneScoped;
  ByteReader reader{{data, size}};

  while (!reader.empty()) {
    const u8 cmd = reader.read<u8>();
    u8 opcode = cmd & CP_OPCODE_MASK;

    switch (opcode) {
    case CP_CMD_NOP:
      continue;

    case CP_CMD_LOAD_BP_REG: {
      const u32 value = reader.read<u32>();
      handle_bp(value);
      if (reg_get(value, 8, 24) == GX_BP_REG_DRAWDONE) {
        return {static_cast<u32>(reader.offset()), true};
      }
      break;
    }

    case CP_CMD_LOAD_CP_REG: {
      const u8 addr = reader.read<u8>();
      handle_cp(addr, reader.read<u32>());
      break;
    }

    case CP_CMD_LOAD_XF_REG: {
      const u32 header = reader.read<u32>();
      const u32 count = ((header >> 16) & 0xFFFF) + 1;
      const u16 addr = header & 0xFFFF;
      handle_xf(addr, reader.take(count * sizeof(u32)));
      break;
    }

    case CP_CMD_LOAD_INDX_A:
    case CP_CMD_LOAD_INDX_B:
    case CP_CMD_LOAD_INDX_C:
    case CP_CMD_LOAD_INDX_D: {
      ZoneScopedN("LOAD_INDX");
      const u32 arrayType = GX_POS_MTX_ARRAY + (opcode - CP_CMD_LOAD_INDX_A) / 0x08;
      const u16 srcArrayIdx = reader.read<u16>();
      const u16 addrLen = reader.read<u16>();

      const u16 len = (addrLen >> 12) + 1;
      const u16 dstAddr = addrLen & 0x0FFF;
      auto const& array = g_gxState.arrays[arrayType];
      const u32 srcOffset = static_cast<u32>(srcArrayIdx) * array.stride;
      const u32 srcSize = static_cast<u32>(len) * sizeof(u32);
      AURORA_ASSERT(array.data != nullptr, "indexed XF load from unmapped array {}", arrayType);
      AURORA_ASSERT(srcOffset <= array.size && srcSize <= array.size - srcOffset,
                    "indexed XF load outside array {}: offset={}, size={}, array size={}", arrayType, srcOffset,
                    srcSize, array.size);
      auto const* srcData = static_cast<const u8*>(array.data) + srcOffset;
      if (!copy_xf_data(dstAddr, srcData, len, array.le ? std::endian::little : std::endian::big)) {
#ifndef NDEBUG
        Log.debug("Unimplemented indexed XF load (opcode 0x{:02X}, dstAddr=%04x)", opcode, dstAddr);
#endif
      }
      break;
    }

    case CP_CMD_CALL_DL: {
      // Call display list: 8 bytes (address + size)
      Log.warn("Ignoring nested GX_CMD_CALL_DL");
      reader.skip(8);
      break;
    }

    case CP_CMD_INVAL_VTX: {
      for (auto& array : g_gxState.arrays) {
        array.cachedRange = {};
      }
      g_gxState.dirty |= DirtyImmediates;
      break;
    }

    case GX_AURORA: {
      handle_aurora(reader);
      break;
    }

    // Draw commands: 0x80-0xBF
    case GX_DRAW_QUADS:
    case GX_DRAW_TRIANGLES:
    case GX_DRAW_TRIANGLE_STRIP:
    case GX_DRAW_TRIANGLE_FAN:
    case GX_DRAW_LINES:
    case GX_DRAW_LINE_STRIP:
    case GX_DRAW_POINTS: {
      handle_draw(cmd, reader);
      break;
    }

    default:
      // Check if it's a draw command (0x80-0xBF range)
      if (cmd >= 0x80) {
        handle_draw(cmd, reader);
      } else {
        // Hex dump surrounding bytes for debugging
        {
          const size_t pos = reader.offset();
          size_t dumpStart = (pos > 17) ? pos - 17 : 0;
          size_t dumpEnd = (pos + 16 < size) ? pos + 16 : size;
          std::string hex;
          for (size_t i = dumpStart; i < dumpEnd; i++) {
            if (i == pos - 1)
              hex += fmt::format("[{:02x}]", data[i]);
            else
              hex += fmt::format(" {:02x}", data[i]);
          }
          Log.error("  hex dump (pos {}-{}):{}", dumpStart, dumpEnd - 1, hex);
        }
        FATAL("command_processor: unknown opcode 0x{:02X} at pos {}", cmd, reader.offset() - 1);
      }
      break;
    }
  }
  return {size, false};
}

[[noreturn]] static void handle_draw_overrun(size_t totalVtxBytes, const ByteReader& reader) noexcept {
  // Hex dump around the draw command for debugging
  const size_t pos = reader.offset();
  const size_t size = reader.size();
  const u8* data = reader.data();
  size_t cmdPos = pos - 2 - 1; // opcode byte position (before vtxCount and pos++)
  size_t dumpStart = (cmdPos > 16) ? cmdPos - 16 : 0;
  size_t dumpEnd = (cmdPos + 32 < size) ? cmdPos + 32 : size;
  std::string hex;
  for (size_t i = dumpStart; i < dumpEnd; i++) {
    if (i == cmdPos)
      hex += fmt::format("[{:02x}]", data[i]);
    else
      hex += fmt::format(" {:02x}", data[i]);
  }
  Log.error("  hex dump around draw cmd (pos {}-{}):{}", dumpStart, dumpEnd - 1, hex);
  FATAL("draw vertex data overrun: need {} bytes at pos {}, have {}", totalVtxBytes, pos, reader.remaining());
}

static u32 calc_vtx_size(GXVtxFmt fmt) noexcept {
  u32 vtxSize = 0;
  const auto& vtxFmt = g_gxState.vtxFmts[fmt];
  for (int i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7; ++i) {
    const auto& attrFmt = vtxFmt.attrs[i];
    switch (g_gxState.vtxDesc[i]) {
    case GX_NONE:
      break;
    case GX_DIRECT: {
      const auto attr = static_cast<GXAttr>(i);
      vtxSize += comp_type_size(attr, attrFmt.type) * comp_cnt_count(attr, attrFmt.cnt);
      break;
    }
    case GX_INDEX8:
      vtxSize += i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3 ? 3 : 1;
      break;
    case GX_INDEX16:
      vtxSize += i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3 ? 6 : 2;
      break;
    }
  }
  g_gxState.lastVtxFmt = fmt;
  g_gxState.lastVtxSize = vtxSize;
  return vtxSize;
}

static void push_gx_draw(GXPrimitive prim, GXVtxFmt fmt, u16 vtxCount, gfx::Range vertRange, gfx::Range idxRange,
                         u32 numIndices) noexcept {
  auto& state = g_gxState;
  auto& cache = sDrawCache;

  DrawImmediateData immediates{.vtxStart = vertRange.offset, .currentPnMtx = state.currentPnMtx};
  for (int i = GX_VA_POS; i <= GX_VA_TEX7; ++i) {
    if (state.vtxDesc[i] != GX_INDEX8 && state.vtxDesc[i] != GX_INDEX16) {
      continue;
    }
    auto& array = state.arrays[i];
    if (array.cachedRange.size == 0) {
      array.cachedRange = gfx::push_storage(static_cast<const uint8_t*>(array.data), array.size);
    }
    immediates.arrayStart[i - GX_VA_POS] = array.cachedRange.offset;
  }

  const u8 lineMode = line_mode_for_prim(prim);
  const auto targetLayoutKey = gfx::get_render_target_layout().key;
  // the hybrid ubershader: a setup drawn by the ubershader while its specialized pipeline compiles is looked at again
  // once per frame, so it switches as soon as that pipeline is ready
  const bool fallbackStale = cache.uberFallback && cache.uberFallbackFrame != gfx::current_frame();
  const bool pipelineValid = cache.hasPipeline && (state.dirty & DirtyPipeline) == 0 && cache.fmt == fmt &&
                             cache.lineMode == lineMode && cache.targetLayoutKey == targetLayoutKey && !fallbackStale;
  if (!pipelineValid) {
    const bool hadPipeline = cache.hasPipeline;
    const bool hadUber = cache.shaderInfo.uber;
    const auto prevSampledTextures = cache.shaderInfo.sampledTextures;
    const auto prevSampledIndTextures = cache.shaderInfo.sampledIndTextures;
    populate_pipeline_config(cache.config, prim, fmt);
    bool specializedReady = false;
    gfx::PipelineRef specializedRef{};
    cache.uberFallback = false;
    if (uber_enabled()) {
      // OrrePort trial (ubershader.cpp): this draw's TEV setup becomes data; the pipeline key loses it. A count of
      // setups taken / left to the generated shader goes to the log every 5,000 setups (hybrid: and how many of the
      // eligible ones already had their specialized pipeline)
      static u32 sUberTaken = 0, sUberLeft = 0, sSpecialized = 0;
      if (uber_eligible(cache.config.shaderConfig)) {
        if (uber_hybrid() &&
            gfx::find_pipeline_if_ready(cache.config, gfx::get_render_target_layout(), specializedRef)) {
          specializedReady = true;
          ++sSpecialized;
        } else {
          uber_encode_program(cache.config.shaderConfig);
          uber_normalize(cache.config.shaderConfig);
          uber_canonicalize_state(cache.config);
          ++sUberTaken;
          if (uber_hybrid()) {
            cache.uberFallback = true;
            cache.uberFallbackFrame = gfx::current_frame();
          }
        }
      } else {
        ++sUberLeft;
      }
      if ((sUberTaken + sUberLeft + sSpecialized) % 5000 == 0) {
        UberLog.info("uber-fragment: {} draw setups through the ubershader, {} through generated shaders{}", sUberTaken,
                     sUberLeft + sSpecialized,
                     uber_hybrid() ? fmt::format(" ({} of them specialized pipelines already compiled)", sSpecialized)
                                   : std::string());
      }
    }
    cache.shaderInfo = build_shader_info(cache.config.shaderConfig);
    cache.pipelineRef = specializedReady ? specializedRef : gfx::pipeline_ref(cache.config);
    if (hadUber != cache.shaderInfo.uber) {
      cache.bindGeneration = 0; // the ubershader binds all its maps, a specialized pipeline only its own: rebind
    }
    cache.targetLayoutKey = targetLayoutKey;
    cache.fmt = fmt;
    cache.lineMode = lineMode;
    cache.hasPipeline = true;
    state.dirty = (state.dirty & ~DirtyPipeline) | DirtyUniform;
    if (!hadPipeline || prevSampledTextures != cache.shaderInfo.sampledTextures ||
        prevSampledIndTextures != cache.shaderInfo.sampledIndTextures) {
      cache.bindGeneration = 0;
    }
    if (cache.shaderInfo.uber && uber_texture_mask() != sUberTexMask) {
      sUberTexMask = uber_texture_mask(); // OrrePort trial: the uber program samples other maps: rebind
      cache.bindGeneration = 0;
    }
  }

  const bool bindGroupsValid =
      (state.dirty & DirtyTextures) == 0 && cache.bindGeneration == texture::current_bind_generation();
  if (!bindGroupsValid) {
    const auto prevBindGroup = cache.bindGroups.textureBindGroup;
    // OrrePort trial: the uber-fragment shader declares all eight maps; resolve and bind only the ones its program
    // samples (a stale slot left by an earlier draw is never touched), the rest get the empty texture
    ShaderInfo bindInfo = cache.shaderInfo;
    if (bindInfo.uber) {
      bindInfo.sampledTextures = std::bitset<MaxTextures>(sUberTexMask);
    }
    resolve_sampled_textures(bindInfo);
    cache.bindGroups = build_bind_groups(bindInfo);
    cache.bindGeneration = texture::current_bind_generation();
    state.dirty &= ~DirtyTextures;
    // For texture_size_bias uniform
    if (cache.bindGroups.textureBindGroup != prevBindGroup) {
      state.dirty |= DirtyUniform;
    }
  }

  const bool uniformValid = (state.dirty & DirtyUniform) == 0 && cache.uniformRange.size != 0;
  if (!uniformValid) {
    cache.uniformRange = build_uniform(cache.shaderInfo);
    state.dirty &= ~DirtyUniform;
  }
  if (cache.config.shaderConfig.fogRangeEnabled) {
    const auto key = fog_range_lut_key();
    if (!cache.hasFogRange || cache.fogRangeKey != key) {
      cache.fogRange = push_fog_range_lut(key);
      cache.fogRangeKey = key;
      cache.hasFogRange = true;
    }
  }
  immediates.fogRangeBase = cache.fogRange.offset / sizeof(u32);

  state.dirty &= ~DirtyImmediates;

  uint32_t instanceCount = 1;
  if (prim == GX_LINES) {
    instanceCount = vtxCount / 2;
  } else if (prim == GX_LINESTRIP) {
    instanceCount = vtxCount - 1;
  } else if (prim == GX_POINTS) {
    instanceCount = vtxCount;
  }
  cache.lastDrawFmt = fmt;
  gfx::push_draw_command(DrawData{
      .pipeline = cache.pipelineRef,
      .bindGroups = cache.bindGroups,
      .vertRange = vertRange,
      .idxRange = idxRange,
      .uniformRange = cache.uniformRange,
      .immediateData = immediates,
      .vtxCount = vtxCount,
      .indexCount = numIndices,
      .instanceCount = instanceCount,
      .dstAlpha = state.dstAlpha,
  });
}

static void handle_draw_unmerged(GXPrimitive prim, GXVtxFmt fmt, u16 vtxCount, gfx::Range vertRange) noexcept {
  ZoneScoped;
  u32 numIndices = 0;
  gfx::Range idxRange;

  if (prim != GX_TRIANGLES) {
    ZoneScopedN("build idx buffer");
    static ByteBuffer idxBuf;
    numIndices = prepare_idx_buffer(idxBuf, prim, 0, vtxCount);
    idxRange = gfx::push_indices(idxBuf.data(), idxBuf.size(), 4);
    idxBuf.clear();
  }

  push_gx_draw(prim, fmt, vtxCount, vertRange, idxRange, numIndices);
}

static void draw_prim(GXPrimitive prim, GXVtxFmt fmt, u16 vtxCount, ByteReader& reader) noexcept {
  ZoneScoped;
  u32 vtxSize;
  if (g_gxState.lastVtxFmt == fmt)
    LIKELY { vtxSize = g_gxState.lastVtxSize; }
  else
    UNLIKELY { vtxSize = calc_vtx_size(fmt); }

  u32 totalVtxBytes = vtxCount * vtxSize;
  if (totalVtxBytes > reader.remaining())
    UNLIKELY { handle_draw_overrun(totalVtxBytes, reader); }

  const bool cleanState = g_gxState.dirty == 0 && fmt == sDrawCache.lastDrawFmt && sDrawCache.lineMode == 0 &&
                          prim != GX_LINES && prim != GX_LINESTRIP && prim != GX_POINTS;
  auto* lastDraw = cleanState ? gfx::get_last_draw_command<DrawData>() : nullptr;
  const bool canMerge = lastDraw != nullptr && lastDraw->instanceCount == 1;

  // Push raw vertex data to buffer. Merged draws must remain contiguous with the previous range.
  const auto vertexData = reader.take(totalVtxBytes);
  // OrrePort, fix (3)'s draw log (2026-11-18; ORRE_SMOOTH_DRAWLOG, recording.hpp): one line per draw with the GX state
  // that tells draws apart (primitive, vertex count and size, the first vertex's position, the current position
  // matrix's scale and offset, the projection, texture map 0), to find which game code draws a piece that changes
  // between steps without blending. Test switch only; reads state, changes nothing. Platform-neutral.
  if (gfx::orre_draw_log_on()) {
    const auto& posFmt = g_gxState.vtxFmts[fmt].attrs[GX_VA_POS];
    float p[3]{};
    const u32 n = posFmt.cnt == GX_POS_XYZ ? 3u : 2u;
    const u32 comp = posFmt.type == GX_F32 ? 4u : posFmt.type == GX_S16 || posFmt.type == GX_U16 ? 2u : 1u;
    const bool direct = g_gxState.vtxDesc[GX_VA_POS] == GX_DIRECT && vtxCount > 0;
    u32 skip = 0; // matrix-index bytes before the position (one each)
    for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7MTXIDX; ++a) {
      skip += g_gxState.vtxDesc[a] != GX_NONE ? 1u : 0u;
    }
    for (u32 c = 0; c < n && direct && skip + (c + 1) * comp <= vtxSize; ++c) {
      const u8* q = vertexData.data() + skip + c * comp;
      const float scale = posFmt.type == GX_F32 ? 1.f : 1.f / static_cast<float>(1u << posFmt.frac);
      switch (posFmt.type) {
      case GX_F32: {
        const u32 bits = (u32{q[0]} << 24) | (u32{q[1]} << 16) | (u32{q[2]} << 8) | u32{q[3]};
        std::memcpy(&p[c], &bits, 4);
        break;
      }
      case GX_S16:
        p[c] = static_cast<float>(static_cast<s16>((q[0] << 8) | q[1])) * scale;
        break;
      case GX_U16:
        p[c] = static_cast<float>(static_cast<u16>((q[0] << 8) | q[1])) * scale;
        break;
      case GX_S8:
        p[c] = static_cast<float>(static_cast<s8>(q[0])) * scale;
        break;
      default:
        p[c] = static_cast<float>(q[0]) * scale;
        break;
      }
    }
    const auto& m = g_gxState.pnMtx[g_gxState.currentPnMtx < MaxPnMtx ? g_gxState.currentPnMtx : 0].pos;
    const auto& tex = g_gxState.textures[0].texObj;
    const std::string line = fmt::format(
        "prim {:x} verts {} size {} pos{} {}{} ({:.2f} {:.2f} {:.2f}) mtx {} scale ({:.3f} {:.3f}) at ({:.2f} {:.2f} "
        "{:.2f}) proj {} ({:.4f} {:.4f} {:.4f} {:.4f}) tex0 {}x{} fmt {:x} data {} id {} stages {} gens {}",
        static_cast<u32>(prim), vtxCount, vtxSize, n, static_cast<int>(posFmt.type), direct ? "" : " indexed", p[0], p[1], p[2],
        g_gxState.currentPnMtx, m.m0.x(), m.m1.y(), m.m0.w(), m.m1.w(), m.m2.w(),
        g_gxState.projType == GX_PERSPECTIVE ? "persp" : "ortho", g_gxState.proj.m0.x(), g_gxState.proj.m1.y(),
        g_gxState.proj.m0.w(), g_gxState.proj.m1.w(), tex.width(), tex.height(), tex.format(), fmt::ptr(tex.data),
        tex.texObjId, g_gxState.numTevStages, g_gxState.numTexGens);
    // the capture prototype (2026-11-18): what moves a picture without moving its vertices (Psychic's warp): the first
    // indirect matrix and the first texture coordinate generator's matrices (their offsets)
    const auto& ind = g_gxState.indTexMtxs[0];
    const auto& tcg = g_gxState.tcgs[0];
    const bool texMtx = tcg.mtx >= GX_TEXMTX0 && tcg.mtx <= GX_TEXMTX9 && (tcg.mtx - GX_TEXMTX0) % 3 == 0;
    const bool ptMtx = tcg.postMtx >= GX_PTTEXMTX0 && tcg.postMtx <= GX_PTTEXMTX19 && (tcg.postMtx - GX_PTTEXMTX0) % 3 == 0;
    const auto& tm = g_gxState.texMtxs[texMtx ? (tcg.mtx - GX_TEXMTX0) / 3 : 0];
    const auto& pm = g_gxState.ptTexMtxs[ptMtx ? (tcg.postMtx - GX_PTTEXMTX0) / 3 : 0];
    const std::string more = fmt::format(
        "   ...ind {} ({:.4f} {:.4f} / {:.4f} {:.4f} / {:.4f} {:.4f}) exp {}; tg0 src {} mtx {} ({:.4f} {:.4f}) post {} "
        "({:.4f} {:.4f})",
        g_gxState.numIndStages, ind.mtx.m0.x, ind.mtx.m0.y, ind.mtx.m1.x, ind.mtx.m1.y, ind.mtx.m2.x, ind.mtx.m2.y,
        static_cast<int>(ind.scaleExp), static_cast<int>(tcg.src), static_cast<int>(tcg.mtx),
        texMtx ? tm.m0.w() : 0.f, texMtx ? tm.m1.w() : 0.f, static_cast<int>(tcg.postMtx), ptMtx ? pm.m0.w() : 0.f,
        ptMtx ? pm.m1.w() : 0.f);
    gfx::orre_draw_log((line + more).c_str());
  }
  // OrrePort, frame smoothing item (a) (2026-11-17): while a particle is being drawn, the middle of its corners (the
  // particle blend's "where it was drawn"). Only when the position is the vertex's first attribute and three plain
  // floats (no matrix-index attributes before it), as all of HSD's particle formats are; anything else marks the
  // particle unusable (it is then drawn as the step drew it). One relaxed load when smoothing is off.
  if (gfx::keep_steps()) {
    bool posFirst = g_gxState.vtxDesc[GX_VA_POS] == GX_DIRECT;
    for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7MTXIDX && posFirst; ++a) {
      posFirst = g_gxState.vtxDesc[a] == GX_NONE;
    }
    // posFirst: the position is each vertex's first attribute (direct, no matrix indexes before it), whatever matrix
    // slot is current. *Corrected 2026-11-17 (round 11):* it also required slot 0, as the particles do, and the game's
    // 2D loads its matrix into another slot, so no bar's x was ever read (verified: P15's HP drain, 2,655 named blocks
    // "no readable x", none blended)
    const auto& posFmt = g_gxState.vtxFmts[fmt].attrs[GX_VA_POS];
    bool plain = posFirst && g_gxState.currentPnMtx == GX_PNMTX0;
    plain = plain && posFmt.cnt == GX_POS_XYZ && posFmt.type == GX_F32 && vtxSize >= 12;
    gfx::orre_smooth_particle_vertices(vertexData.data(), vtxCount, vtxSize, plain);
    // round 11, 2D at 120: while a named 2D piece is drawn, the left and right ends of its vertices' x (the piece's own
    // space, before the matrix), for the bar's width blend about its left end. Any position type the GX vertex format
    // allows (the game's 2D draws its corners as 16-bit integers, `gfxVtxPos2s16`), big-endian in the stream.
    if (gfx::orre_smooth_2d_active()) {
      bool ok = posFirst;
      float minX = 0.f, maxX = 0.f;
      const u32 size = posFmt.type == GX_F32 ? 4u : posFmt.type == GX_S16 || posFmt.type == GX_U16 ? 2u : 1u;
      const float scale = posFmt.type == GX_F32 ? 1.f : 1.f / static_cast<float>(1u << posFmt.frac);
      for (u32 v = 0; v < vtxCount && ok && vtxSize >= size; ++v) {
        const u8* p = vertexData.data() + static_cast<size_t>(v) * vtxSize;
        float x = 0.f;
        switch (posFmt.type) {
        case GX_F32: {
          const u32 bits = (u32{p[0]} << 24) | (u32{p[1]} << 16) | (u32{p[2]} << 8) | u32{p[3]};
          std::memcpy(&x, &bits, sizeof(x));
          break;
        }
        case GX_S16:
          x = static_cast<float>(static_cast<s16>((p[0] << 8) | p[1])) * scale;
          break;
        case GX_U16:
          x = static_cast<float>(static_cast<u16>((p[0] << 8) | p[1])) * scale;
          break;
        case GX_S8:
          x = static_cast<float>(static_cast<s8>(p[0])) * scale;
          break;
        default:
          x = static_cast<float>(p[0]) * scale;
          break;
        }
        ok = std::isfinite(x);
        minX = v == 0 ? x : std::min(minX, x);
        maxX = v == 0 ? x : std::max(maxX, x);
      }
      gfx::orre_smooth_2d_vertices(minX, maxX, ok && vtxCount > 0);
    }
  }
  gfx::Range vertRange = gfx::push_verts(vertexData.data(), vertexData.size(), canMerge ? 0 : 4);
  // round 11, 2D at 120, vertex mode (GXAurora.h, GX_AURORA_ORRE_2D_VERTICES: the pause menu): this draw's vertices as
  // pushed, with where the position and the first colour sit in each vertex, for the in-between's vertex blend
  // (smooth_camera.cpp). The position must be first (direct, no matrix indexes before it); the colour is blended only
  // when it directly follows the position with 8 bits a channel (no normal between). One relaxed load when it's off.
  // (3b), 2026-11-18: and while a particle is drawn, the same copy for the particle-corner blend (a star that grows or
  // turns in place: HSD works its corners out on the CPU every step)
  const bool pieceVertices = gfx::keep_steps() && gfx::orre_smooth_2d_vertex_mode();
  const bool particleVertices = gfx::keep_steps() && !pieceVertices && gfx::orre_smooth_particle_active();
  if (pieceVertices || particleVertices) {
    gfx::detail::OrreSmooth2DLayout layout{.vtxSize = vtxSize};
    bool posFirst = g_gxState.vtxDesc[GX_VA_POS] == GX_DIRECT;
    for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7MTXIDX && posFirst; ++a) {
      posFirst = g_gxState.vtxDesc[a] == GX_NONE;
    }
    const auto& posFmt = g_gxState.vtxFmts[fmt].attrs[GX_VA_POS];
    const u32 compSize = posFmt.type == GX_F32 ? 4u : posFmt.type == GX_S16 || posFmt.type == GX_U16 ? 2u : 1u;
    if (posFirst && posFmt.type <= GX_F32) {
      layout.posType = static_cast<u8>(posFmt.type);
      layout.posCount = posFmt.cnt == GX_POS_XYZ ? 3 : 2;
      layout.posFrac = static_cast<u8>(posFmt.frac);
      layout.posBytes = static_cast<u16>(compSize * layout.posCount);
      if (g_gxState.vtxDesc[GX_VA_NRM] == GX_NONE && g_gxState.vtxDesc[GX_VA_NBT] == GX_NONE &&
          g_gxState.vtxDesc[GX_VA_CLR0] == GX_DIRECT) {
        const auto clrType = g_gxState.vtxFmts[fmt].attrs[GX_VA_CLR0].type;
        layout.clrBytes = clrType == GX_RGB8 ? 3 : clrType == GX_RGBX8 || clrType == GX_RGBA8 ? 4 : 0;
      }
      if (layout.posBytes + layout.clrBytes > vtxSize) {
        layout.posCount = 0;
        layout.clrBytes = 0;
      }
    }
    if (pieceVertices) {
      gfx::orre_smooth_2d_chunk(vertexData.data(), vertRange.offset, vertRange.size, layout);
    } else {
      gfx::orre_smooth_particle_chunk(vertexData.data(), vertRange.offset, vertRange.size, layout);
    }
  }

  // Try to merge with previous draw call
  if (canMerge) {
    u32 numIndices = 0;
    gfx::Range idxRange;
    static ByteBuffer idxBuf;
    const bool hadIndexRange = lastDraw->idxRange.size != 0;
    if (lastDraw->indexCount == 0 && prim != GX_TRIANGLES) {
      // Generate triangle index buffer for previous draw
      lastDraw->indexCount = prepare_idx_buffer(idxBuf, GX_TRIANGLES, 0, lastDraw->vtxCount);
    }
    if (lastDraw->indexCount != 0) {
      numIndices += prepare_idx_buffer(idxBuf, prim, lastDraw->vtxCount, vtxCount);
      idxRange = gfx::push_indices(idxBuf.data(), idxBuf.size(), hadIndexRange ? 0 : 4);
      idxBuf.clear();
    }
    CHECK(lastDraw->vertRange.offset + lastDraw->vertRange.size == vertRange.offset,
          "Non-consecutive vertex ranges ({} < {})", lastDraw->vertRange.offset + lastDraw->vertRange.size,
          vertRange.offset);
    if (hadIndexRange) {
      CHECK(lastDraw->idxRange.offset + lastDraw->idxRange.size == idxRange.offset,
            "Non-consecutive index ranges ({} < {})", lastDraw->idxRange.offset + lastDraw->idxRange.size,
            idxRange.offset);
    }
    lastDraw->vertRange.size += vertRange.size;
    if (lastDraw->idxRange.size == 0) {
      lastDraw->idxRange = idxRange;
    } else {
      lastDraw->idxRange.size += idxRange.size;
    }
    lastDraw->vtxCount += vtxCount;
    lastDraw->indexCount += numIndices;
    gfx::detail::increment_merged_draw_count();
    return;
  }

  handle_draw_unmerged(prim, fmt, vtxCount, vertRange);
}

static void handle_draw(u8 cmd, ByteReader& reader) noexcept {
  const auto fmt = static_cast<GXVtxFmt>(cmd & CP_VAT_MASK);
  const auto prim = static_cast<GXPrimitive>(cmd & CP_OPCODE_MASK);
  draw_prim(prim, fmt, reader.read<u16>(), reader);
}

void handle_aurora(ByteReader& reader) noexcept {
  ZoneScoped;
  const u16 subCmd = reader.read<u16>();

  if (subCmd == GX_AURORA_LOAD_VIEWPORT_RENDER) {
    const f32 left = reader.read<f32>();
    const f32 top = reader.read<f32>();
    const f32 width = reader.read<f32>();
    const f32 height = reader.read<f32>();
    const f32 nearZ = reader.read<f32>();
    const f32 farZ = reader.read<f32>();
    set_render_viewport({
        .left = left,
        .top = top,
        .width = width,
        .height = height,
        .znear = nearZ,
        .zfar = farZ,
    });
  } else if (subCmd == GX_AURORA_LOAD_SCISSOR_RENDER) {
    const s32 left = reader.read<s32>();
    const s32 top = reader.read<s32>();
    const s32 width = reader.read<s32>();
    const s32 height = reader.read<s32>();
    set_render_scissor({left, top, width, height});
  } else if (subCmd == GX_AURORA_LOAD_PROJECTION_FULL) {
    auto& proj = g_gxState.proj;
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        proj[r][c] = reader.read<f32>();
      }
    }
    // Invalidate projection XF regs
    for (u32 reg = 0x20; reg <= 0x26; ++reg) {
      g_gxState.xfRegValid.reset(reg);
    }
    g_gxState.dirty |= DirtyUniform;
  } else if (subCmd >= GX_AURORA_LOAD_ARRAYBASE && subCmd <= (GX_AURORA_LOAD_ARRAYBASE | 0x0f)) {
    const u32 attrIdx = subCmd - GX_AURORA_LOAD_ARRAYBASE + GX_VA_POS;
    const u64 arrayAddr = reader.read<u64>();
    const u32 arraySize = reader.read<u32>();
    const bool le = reader.read<u8>() == 1;

    auto& array = g_gxState.arrays[attrIdx];
    const auto newData = reinterpret_cast<void*>(arrayAddr);
    if (array.data != newData || array.size != arraySize || array.le != le) {
      if (array.le != le) {
        // Endianness is baked into the shader
        g_gxState.dirty |= DirtyPipeline;
      }
      array.data = newData;
      array.size = arraySize;
      array.le = le;
      array.cachedRange = {};
      g_gxState.dirty |= DirtyImmediates;
    }
  } else if (subCmd == GX_AURORA_LOAD_TEXOBJ) {
    const auto texMapId = reader.read<u8>();
    CHECK(texMapId < MaxTextures, "invalid texture map id {}", texMapId);
    auto& slot = g_gxState.loadedTextures[texMapId];
    const auto newData = reinterpret_cast<const void*>(reader.read<u64>());
    const u32 newWidth = reader.read<u32>();
    const u32 newHeight = reader.read<u32>();
    const auto newFormat = static_cast<GXTexFmt>(reader.read<u32>());
    const auto newTlut = static_cast<GXTlut>(reader.read<u32>());
    u8 newFlags = slot.flags & ~0x80u; // Reset no-cache flag
    if (reader.read<u8>() != 0) {
      newFlags |= 1u;
    } else {
      newFlags &= ~1u;
    }
    const u32 newTexObjId = reader.read<u32>();
    const u32 newTexDataVersion = reader.read<u32>();
    if (slot.data != newData || slot.mWidth != newWidth || slot.mHeight != newHeight ||
        slot.mFormat != static_cast<u32>(newFormat) || slot.tlut != newTlut || slot.flags != newFlags ||
        slot.texObjId != newTexObjId || slot.texDataVersion != newTexDataVersion) {
      slot.data = newData;
      slot.mWidth = newWidth;
      slot.mHeight = newHeight;
      slot.mFormat = newFormat;
      slot.tlut = newTlut;
      slot.flags = newFlags;
      slot.texObjId = newTexObjId;
      slot.texDataVersion = newTexDataVersion;
      g_gxState.dirty |= DirtyTextures;
    }
  } else if (subCmd == GX_AURORA_LOAD_TLUT) {
    const auto idx = reader.read<u8>();
    CHECK(idx < MaxTluts, "invalid tlut slot {}", idx);
    auto& slot = g_gxState.loadedTluts[idx];
    const auto newData = reinterpret_cast<const void*>(reader.read<u64>());
    const auto newFormat = static_cast<GXTlutFmt>(reader.read<u32>());
    const u16 newNumEntries = reader.read<u16>();
    const u32 newTlutObjId = reader.read<u32>();
    const u32 newTlutDataVersion = reader.read<u32>();
    const u8 newFlags = slot.flags & ~0x80u; // Reset no-cache flag
    if (slot.data != newData || slot.format != newFormat || slot.numEntries != newNumEntries ||
        slot.tlutObjId != newTlutObjId || slot.tlutDataVersion != newTlutDataVersion || slot.flags != newFlags) {
      if (slot.tlutObjId != newTlutObjId || slot.tlutDataVersion != newTlutDataVersion) {
        texture::invalidate_bindings();
      }
      slot.data = newData;
      slot.format = newFormat;
      slot.numEntries = newNumEntries;
      slot.tlutObjId = newTlutObjId;
      slot.tlutDataVersion = newTlutDataVersion;
      slot.flags = newFlags;
      g_gxState.dirty |= DirtyTextures;
    }
  } else if (subCmd == GX2_SET_POLYGON_OFFSET) {
    const f32 frontOffset = reader.read<f32>();
    const f32 frontScale = reader.read<f32>();
    const f32 backOffset = reader.read<f32>();
    const f32 backScale = reader.read<f32>();
    const f32 clamp = reader.read<f32>();
    if (g_gxState.frontOffset != frontOffset || g_gxState.frontScale != frontScale ||
        g_gxState.backOffset != backOffset || g_gxState.backScale != backScale || g_gxState.clamp != clamp) {
      g_gxState.frontOffset = frontOffset;
      g_gxState.frontScale = frontScale;
      g_gxState.backOffset = backOffset;
      g_gxState.backScale = backScale;
      g_gxState.clamp = clamp;
      g_gxState.dirty |= DirtyPipeline;
    }
  } else if (subCmd == GX_AURORA_SET_DRAWS_BLOCKING) {
    const bool blocking = reader.read<u8>() != 0;
    if (g_gxState.drawsBlocking != blocking) {
      g_gxState.drawsBlocking = blocking;
      g_gxState.dirty |= DirtyPipeline;
    }
  } else if (subCmd == GX_AURORA_LOAD_COPY_SRC) {
    const s32 left = reader.read<s32>();
    const s32 top = reader.read<s32>();
    const s32 width = reader.read<s32>();
    const s32 height = reader.read<s32>();
    g_gxState.texCopySrc = {left, top, width, height};
  } else if (subCmd == GX_AURORA_LOAD_COPY_DST) {
    g_gxState.texCopyDstWidth = reader.read<u32>();
    g_gxState.texCopyDstHeight = reader.read<u32>();
    g_gxState.texCopyFmt = static_cast<GXTexFmt>(reader.read<u32>());
    reader.skip(1); // mipmap is not implemented, but remains part of the command payload
    g_gxState.texCopyDstWide = true;
  } else if (subCmd == GX_AURORA_LOAD_COPY_DEST) {
    g_gxState.texCopyDest = reinterpret_cast<const void*>(reader.read<u64>());
  } else if (subCmd == GX_AURORA_READBACK_CAPTURE_PEEKS) {
    readback::handle_capture_peeks(reader);
  } else if (subCmd == GX_AURORA_READBACK_CAPTURE_COPIES) {
    readback::handle_capture_copies(reader);
  } else if (subCmd == GX_AURORA_BEGIN_OFFSCREEN) {
    const u32 width = reader.read<u32>();
    const u32 height = reader.read<u32>();
    gfx::begin_offscreen(width, height);
  } else if (subCmd == GX_AURORA_END_OFFSCREEN) {
    gfx::end_offscreen();
  } else if (subCmd == GX_AURORA_ORRE_CAMERA) {
    // OrrePort, frame smoothing step (c) (GXAurora.h): observation only, no GX state changes. Kept in the step's
    // packet while steps are kept (recording.cpp, orre_smooth_camera); read and dropped otherwise.
    const u64 id = reader.read<u64>();
    const auto renderPass = static_cast<s32>(reader.read<u32>());
    float view[12];
    for (auto& value : view) {
      value = reader.read<f32>();
    }
    float interest[3]{};
    const bool hasInterest = reader.read<u8>() != 0;
    if (hasInterest) {
      for (auto& value : interest) {
        value = reader.read<f32>();
      }
    }
    if (gfx::keep_steps()) {
      gfx::orre_smooth_camera(id, renderPass, view, hasInterest ? interest : nullptr);
    }
  } else if (subCmd == GX_AURORA_ORRE_MODEL) {
    // OrrePort, frame smoothing step (d) (GXAurora.h): observation only, like the camera marker above
    const u64 jobj = reader.read<u64>();
    const u64 pobj = reader.read<u64>();
    const u64 root = reader.read<u64>();  // 2026-11-17 (GXAurora.h): the piece's model
    const u32 slots = reader.read<u32>(); // and the matrix slots it uses
    if (gfx::keep_steps()) {
      gfx::orre_smooth_model(jobj, pobj, root, slots);
    }
  } else if (subCmd == GX_AURORA_ORRE_PARTICLE) {
    // OrrePort, frame smoothing item (a) (GXAurora.h; 2026-11-17): the particle the next draws belong to. The draw
    // after it gets a uniform block of its own and is not merged into the draw before (DirtyUniform: draw_prim merges
    // only with a clean state), so each particle can be moved on its own in an in-between; the same at the particle's
    // end, so a later draw never shares a particle's block. Changes no picture: the same block contents, one more
    // block and draw per particle, only while steps are kept.
    const u64 particle = reader.read<u64>();
    const u32 guard = reader.read<u32>();
    const u32 life = reader.read<u32>();
    if (gfx::keep_steps()) {
      gfx::orre_smooth_particle(particle, guard, life);
      g_gxState.dirty |= DirtyUniform;
    }
  } else if (subCmd == GX_AURORA_ORRE_2D) {
    // OrrePort, frame smoothing round 11 (GXAurora.h; 2026-11-17): a named 2D piece; its first draw gets a uniform
    // block of its own (not merged with the draw before), and so does the first draw after it, as for particles
    const u32 kind = reader.read<u32>();
    const u32 variant = reader.read<u32>();
    const u32 id = reader.read<u32>();
    const f32 value = reader.read<f32>();
    if (gfx::keep_steps()) {
      gfx::orre_smooth_2d(kind, variant, id, value);
      g_gxState.dirty |= DirtyUniform;
    }
  } else if (subCmd == GX_AURORA_DESTROY_TEXOBJ) {
    evict_texture_object(reader.read<u32>());
  } else if (subCmd == GX_AURORA_DESTROY_TLUT) {
    evict_tlut_object(reader.read<u32>());
  } else if (subCmd == GX_AURORA_DESTROY_COPY_TEX) {
    evict_copy_texture(reinterpret_cast<const void*>(reader.read<u64>()));
  } else if (subCmd == GX_AURORA_DRAW_SIZED) {
    const u8 cmd = reader.read<u8>();
    const u32 byteLen = reader.read<u32>();
    const GXVtxFmt fmt = static_cast<GXVtxFmt>(cmd & CP_VAT_MASK);
    const GXPrimitive prim = static_cast<GXPrimitive>(cmd & CP_OPCODE_MASK);
    if (byteLen != 0) {
      u32 vtxSize;
      if (g_gxState.lastVtxFmt == fmt) {
        vtxSize = g_gxState.lastVtxSize;
      } else {
        vtxSize = calc_vtx_size(fmt);
      }
      AURORA_ASSERT(vtxSize != 0 && byteLen % vtxSize == 0,
                    "GX_AURORA_DRAW_SIZED: {} bytes is not a whole number of size-{} vertices", byteLen, vtxSize);
      u32 vtxCount = byteLen / vtxSize;
      AURORA_ASSERT(vtxCount <= 0xFFFF, "GX_AURORA_DRAW_SIZED: too many vertices ({})", vtxCount);
      draw_prim(prim, fmt, static_cast<u16>(vtxCount), reader);
    }
  } else if (subCmd == GX_AURORA_DRAW_INDEXED) {
    ZoneScopedN("DRAW_INDEXED");
    const u8 cmd = reader.read<u8>();
    const u16 vtxCount = reader.read<u16>();
    const u32 indexCount = reader.read<u32>();
    const GXVtxFmt fmt = static_cast<GXVtxFmt>(cmd & CP_VAT_MASK);
    const GXPrimitive prim = static_cast<GXPrimitive>(cmd & CP_OPCODE_MASK);
    AURORA_ASSERT(prim == GX_TRIANGLES, "GX_AURORA_DRAW_INDEXED: primitive must be GX_TRIANGLES, got {}",
                  static_cast<u32>(prim));
    const size_t idxBytes = static_cast<size_t>(indexCount) * sizeof(u16);
    // Index data is always host-endian; push it to the GPU buffer as-is
    const auto indexData = reader.take(idxBytes);
    const gfx::Range idxRange = gfx::push_indices(indexData.data(), indexData.size(), 4);
    u32 vtxSize;
    if (g_gxState.lastVtxFmt == fmt) {
      vtxSize = g_gxState.lastVtxSize;
    } else {
      vtxSize = calc_vtx_size(fmt);
    }
    const u32 totalVtxBytes = vtxCount * vtxSize;
    const auto vertexData = reader.take(totalVtxBytes);
    const gfx::Range vertRange = gfx::push_verts(vertexData.data(), vertexData.size(), 4);
    if (indexCount != 0) {
      push_gx_draw(prim, fmt, vtxCount, vertRange, idxRange, indexCount);
    }
  } else if (subCmd == GX_AURORA_DEBUG_GROUP_PUSH) {
    auto label = reader.read_string();
    gfx::push_debug_group(std::move(label));
  } else if (subCmd == GX_AURORA_DEBUG_GROUP_POP) {
    pop_debug_group();
  } else if (subCmd == GX_AURORA_DEBUG_MARKER_INSERT) {
    auto label = reader.read_string();
    gfx::insert_debug_marker(std::move(label));
  }

  else {
    Log.error("Unknown Aurora subcommand: {:04X}", subCmd);
  }
}

void clear_draw_cache() noexcept {
  sDrawCache.bindGeneration = 0;
  sDrawCache.uniformRange = {};
  sDrawCache.fogRange = {};
  sDrawCache.hasFogRange = false;
}

} // namespace aurora::gx::fifo
