#pragma once

#include "gx.hpp"

namespace aurora::gx {
ShaderInfo build_shader_info(const ShaderConfig& config) noexcept;
gfx::Range build_uniform(const ShaderInfo& info) noexcept;
u8 color_channel(GXChannelID id) noexcept;
// OrrePort, frame smoothing step (c): the projection as the uniform block holds it (Aurora's depth-range change applied
// to the GX projection's rows), so a re-aimed projection is written the way fill_uniform writes the step's own
Mat4x4<float> uniform_projection(const Mat4x4<float>& proj) noexcept;
}; // namespace aurora::gx
