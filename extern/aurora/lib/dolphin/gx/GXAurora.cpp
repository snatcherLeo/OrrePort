#include "dolphin/gx/GXAurora.h"

#include <limits>

#include "__gx.h"
#include "gx.hpp"
#include "../../window.hpp"

#include "../../gfx/readback/readback.hpp"
#include "../../gx/fifo.hpp"
#include "../../gx/readback.hpp"

static void GXWriteString(const char* label) {
  auto length = strlen(label);

  if (length > std::numeric_limits<u16>::max()) {
    Log.warn("Debug marker size over u16 max, truncating");
    length = std::numeric_limits<u16>::max();
  }

  GX_WRITE_U16(length);
  GX_WRITE_DATA(label, length);
}

void GXPushDebugGroup(const char* label) {
  GX_WRITE_AURORA(GX_AURORA_DEBUG_GROUP_PUSH);
  GXWriteString(label);
}

void GXPopDebugGroup() { GX_WRITE_AURORA(GX_AURORA_DEBUG_GROUP_POP); }

void GXInsertDebugMarker(const char* label) {
  GX_WRITE_AURORA(GX_AURORA_DEBUG_MARKER_INSERT);
  GXWriteString(label);
}

void AuroraSetViewportPolicy(AuroraViewportPolicy policy) { aurora::gx::set_viewport_policy(policy); }

void AuroraGetRenderSize(u32* width, u32* height) {
  const auto windowSize = aurora::window::get_window_size();
  if (width != nullptr) {
    *width = windowSize.fb_width;
  }
  if (height != nullptr) {
    *height = windowSize.fb_height;
  }
}

void AuroraGXSync() {
  GXFlush();
  aurora::gx::fifo::drain();
}

void AuroraSetDrawsBlocking(bool blocking) {
  GX_WRITE_AURORA(GX_AURORA_SET_DRAWS_BLOCKING);
  GX_WRITE_U8(blocking);
}

void GXSetViewportRender(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
  GX_WRITE_AURORA(GX_AURORA_LOAD_VIEWPORT_RENDER);
  GX_WRITE_F32(left);
  GX_WRITE_F32(top);
  GX_WRITE_F32(wd);
  GX_WRITE_F32(ht);
  GX_WRITE_F32(nearz);
  GX_WRITE_F32(farz);
}

void GXSetScissorRender(u32 left, u32 top, u32 wd, u32 ht) {
  GX_WRITE_AURORA(GX_AURORA_LOAD_SCISSOR_RENDER);
  GX_WRITE_U32(left);
  GX_WRITE_U32(top);
  GX_WRITE_U32(wd);
  GX_WRITE_U32(ht);
}

void GXSetProjectionFull(const void* mtx) {
  const f32* values = reinterpret_cast<const f32*>(mtx);
  GX_WRITE_AURORA(GX_AURORA_LOAD_PROJECTION_FULL);
  for (int i = 0; i < 16; ++i) {
    GX_WRITE_F32(values[i]);
  }
}

void GX2SetPolygonOffset(f32 mFrontOffset, f32 mFrontScale, f32 mBackOffset, f32 mBackScale, f32 mClamp) {
  GX_WRITE_AURORA(GX2_SET_POLYGON_OFFSET);
  GX_WRITE_F32(mFrontOffset);
  GX_WRITE_F32(mFrontScale);
  GX_WRITE_F32(mBackOffset);
  GX_WRITE_F32(mBackScale);
  GX_WRITE_F32(mClamp);
}

// OrrePort, frame smoothing step (c) (GXAurora.h, GX_AURORA_ORRE_CAMERA): the marker goes into the GX stream so it is
// decoded in order with the draws on Aurora's FIFO thread. Never into a display list: a list is replayed later (and
// may be replayed many times), so a marker inside it would tag the wrong draws. Platform-neutral.
void GXOrreSmoothCamera(u64 id, s32 renderPass, const f32* view, const f32* interest) {
  if (view == nullptr || aurora::gx::fifo::in_display_list()) {
    return;
  }
  GX_WRITE_AURORA(GX_AURORA_ORRE_CAMERA);
  GX_WRITE_U64(id);
  GX_WRITE_U32(static_cast<u32>(renderPass));
  for (int i = 0; i < 12; ++i) {
    GX_WRITE_F32(view[i]);
  }
  GX_WRITE_U8(interest != nullptr ? 1 : 0);
  if (interest != nullptr) {
    GX_WRITE_F32(interest[0]);
    GX_WRITE_F32(interest[1]);
    GX_WRITE_F32(interest[2]);
  }
}

// OrrePort, frame smoothing step (d) (GXAurora.h, GX_AURORA_ORRE_MODEL): the same rule as the camera marker above, for
// the same reason (a display list is replayed later, so a marker inside one would tag the wrong draws)
void GXOrreSmoothModel(u64 jobj, u64 pobj, u64 root, u32 slots) {
  if (aurora::gx::fifo::in_display_list()) {
    return;
  }
  GX_WRITE_AURORA(GX_AURORA_ORRE_MODEL);
  GX_WRITE_U64(jobj);
  GX_WRITE_U64(pobj);
  GX_WRITE_U64(root);  // 2026-11-17: the model the piece belongs to (GXAurora.h)
  GX_WRITE_U32(slots); // the matrix slots its display list uses
}

// OrrePort, frame smoothing item (a) (GXAurora.h, GX_AURORA_ORRE_PARTICLE; 2026-11-17): the same rule as the markers
// above (never inside a display list, which is replayed later)
void GXOrreSmoothParticle(u64 particle, u32 guard, u32 life) {
  if (aurora::gx::fifo::in_display_list()) {
    return;
  }
  GX_WRITE_AURORA(GX_AURORA_ORRE_PARTICLE);
  GX_WRITE_U64(particle);
  GX_WRITE_U32(guard);
  GX_WRITE_U32(life);
}

// OrrePort, frame smoothing round 11 (GXAurora.h, GX_AURORA_ORRE_2D; 2026-11-17): the same rule as the markers above
void GXOrreSmooth2D(u32 kind, u32 variant, u32 id, u32 valueBits) {
  if (aurora::gx::fifo::in_display_list()) {
    return;
  }
  GX_WRITE_AURORA(GX_AURORA_ORRE_2D);
  GX_WRITE_U32(kind);
  GX_WRITE_U32(variant);
  GX_WRITE_U32(id);
  GX_WRITE_U32(valueBits);
}

void GXCreateFrameBuffer(u32 width, u32 height) {
  GX_WRITE_AURORA(GX_AURORA_BEGIN_OFFSCREEN);
  GX_WRITE_U32(width);
  GX_WRITE_U32(height);
  aurora::gx::fifo::publish();
}

void GXRestoreFrameBuffer() {
  GX_WRITE_AURORA(GX_AURORA_END_OFFSCREEN);
  aurora::gx::fifo::publish();
}

AuroraReadbackStream AuroraReadbackCreateStream() { return aurora::gfx::readback::create_stream(); }

void AuroraReadbackDestroyStream(AuroraReadbackStream stream) { aurora::gfx::readback::destroy_stream(stream); }

GXBool AuroraReadbackCapturePeeks(AuroraReadbackStream stream, const AuroraReadbackPeekRequest* requests, u32 count) {
  return aurora::gx::readback::capture_peeks(stream, requests, count, __gx->alphaRead);
}

GXBool AuroraReadbackCaptureCopies(AuroraReadbackStream stream, const AuroraReadbackCopySrc* srcs, u32 count) {
  return aurora::gx::readback::capture_copies(stream, srcs, count);
}

GXBool AuroraReadbackGet(AuroraReadbackStream stream, AuroraReadbackDest* dests, u32 count) {
  return aurora::gx::readback::get(stream, dests, count);
}
