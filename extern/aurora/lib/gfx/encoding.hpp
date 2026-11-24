#pragma once

#include "frame_packet.hpp"

namespace aurora::gfx {

bool bind_pipeline(PipelineRef ref, const wgpu::RenderPassEncoder& pass);
void bind_pipeline(const wgpu::RenderPipeline& pipeline, const wgpu::RenderPassEncoder& pass);

namespace detail {
void encode_op(wgpu::CommandEncoder& encoder, FramePacket& frame, const FrameOp& op);
// OrrePort, frame smoothing step (b): draws a finished step's passes again (encoding.cpp says what is skipped)
uint32_t encode_replay(wgpu::CommandEncoder& encoder, FramePacket& frame);
}

} // namespace aurora::gfx
