#include "pipeline.hpp"

#include "../gfx/encoding.hpp"
#include "../gfx/resources.hpp"
#include "../gfx/pipeline_cache.hpp"
#include "../gfx/resource_cache.hpp"
#include "../gfx/smooth_camera.hpp" // OrrePort, frame smoothing step (c): the blended camera's uniform copies

#include "gx_fmt.hpp"
#include "shader_info.hpp"

#include <tracy/Tracy.hpp>

#include <chrono>

namespace aurora::gx {

gfx::CompiledPipeline create_pipeline(const PipelineConfig& config, const gfx::RenderTargetLayout& layout) {
  ZoneScoped;
  const auto hash = xxh3_hash(layout.key, xxh3_hash(config, static_cast<HashType>(gfx::ShaderType::GX)));
  const auto create = [&](const PipelineOptions& options, const char* passName) {
    // OrrePort trial (ubershader.cpp): how long each ubershader pipeline takes to build, split into the shader module
    // (WGSL parsing) and the pipeline (translation to the backend and the driver's compile), for the first-launch
    // build time
    const bool uber = config.shaderConfig.pad2 == UberShaderMarker;
    const auto t0 = std::chrono::steady_clock::now();
    const auto shader = build_shader(config.shaderConfig, layout, options.dstAlphaMode);
    const auto t1 = std::chrono::steady_clock::now();
    const auto label = fmt::format("GX Pipeline {:x} {}", hash, passName);
    auto built = build_pipeline(config, layout, options, {}, shader, label.c_str());
    if (uber) {
      const auto t2 = std::chrono::steady_clock::now();
      static constexpr Module UberTimeLog{"aurora::gx::uber"};
      UberTimeLog.info("uber pipeline {:x} {}: shader module {:.1f} ms, pipeline {:.1f} ms", hash, passName,
                       std::chrono::duration<double, std::milli>(t1 - t0).count(),
                       std::chrono::duration<double, std::milli>(t2 - t1).count());
    }
    return built;
  };

  PipelineOptions options{
      .colorUpdate = config.colorUpdate,
      .alphaUpdate = config.alphaUpdate,
      .depthUpdate = config.depthUpdate,
  };
  gfx::CompiledPipeline pipeline;
  if (config.alphaUpdate && config.dstAlpha != UINT32_MAX && config.dstAlpha != 0) {
    const auto usesSourceAlpha = [](GXBlendFactor factor) {
      return factor == GX_BL_SRCALPHA || factor == GX_BL_INVSRCALPHA;
    };
    const bool needsSourceAlpha = config.colorUpdate && config.blendMode == GX_BM_BLEND &&
                                  (usesSourceAlpha(config.blendFacSrc) || usesSourceAlpha(config.blendFacDst));
    if (!needsSourceAlpha) {
      options.dstAlphaMode = DstAlphaMode::Replace;
    } else if (webgpu::g_dualSourceBlendingSupported && layout.colorAttachmentCount == 1) {
      options.dstAlphaMode = DstAlphaMode::DualSource;
    } else {
      // Write alpha before RGB, no depth write
      auto alpha = options;
      alpha.dstAlphaMode = DstAlphaMode::Replace;
      alpha.colorUpdate = false;
      alpha.depthUpdate = false;
      pipeline.prepass = create(alpha, "alpha");
      if (!pipeline.prepass) {
        return {};
      }
      options.alphaUpdate = false;
    }
  }
  pipeline.main = create(options, "main");
  if (!pipeline.main) {
    return {};
  }
  return pipeline;
}

void render(const DrawData& data, const wgpu::RenderPassEncoder& pass) {
  gfx::CompiledPipeline pipeline;
  if (!gfx::get_pipeline(data.pipeline, pipeline)) {
    return;
  }
  const auto& resources = gfx::detail::resources();
  // OrrePort, frame smoothing round 11 (2D at 120, the pause menu): a draw whose vertices were blended for this
  // in-between reads them from their copy (another start in the same vertex buffer). A plain bool check otherwise.
  DrawImmediateData immediates = data.immediateData;
  gfx::smooth_camera::override_vertices(immediates.vtxStart);
  pass.SetImmediates(0, &immediates, sizeof(immediates));
  // OrrePort, frame smoothing step (c) (gfx/smooth_camera.hpp): while an in-between picture with a blended camera is
  // recorded, a draw whose uniform block was re-aimed reads its copy from the blend buffer instead. A plain bool check
  // otherwise.
  uint32_t uniformOffset = data.uniformRange.offset;
  wgpu::BindGroup uniformGroup;
  const bool blended = gfx::smooth_camera::override_uniform(uniformOffset, uniformGroup);
  const std::array offsets{uniformOffset};
  pass.SetBindGroup(1, blended ? uniformGroup : resources.uniformBindGroup, offsets.size(), offsets.data());
  if (data.bindGroups.textureBindGroup) {
    pass.SetBindGroup(2, gfx::find_bind_group(data.bindGroups.textureBindGroup));
  }
  pass.SetIndexBuffer(resources.indexBuffer, wgpu::IndexFormat::Uint16, data.idxRange.offset, data.idxRange.size);
  if (data.dstAlpha != UINT32_MAX) {
    const wgpu::Color color{0.f, 0.f, 0.f, data.dstAlpha / 255.f};
    pass.SetBlendConstant(&color);
  }
  const auto draw = [&] {
    if (data.indexCount == 0) {
      pass.Draw(data.vtxCount, data.instanceCount);
    } else {
      pass.DrawIndexed(data.indexCount, data.instanceCount);
    }
  };
  if (pipeline.prepass) {
    gfx::bind_pipeline(pipeline.prepass, pass);
    draw();
  }
  gfx::bind_pipeline(pipeline.main, pass);
  draw();
}

} // namespace aurora::gx
