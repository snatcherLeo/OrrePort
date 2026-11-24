#ifndef DOLPHIN_GXAURORA_H
#define DOLPHIN_GXAURORA_H

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>

#if __cplusplus
extern "C" {
#endif

//
// Subcommands for GX_AURORA.
//

/**
 * Sets the actual render viewport in native framebuffer coordinates.
 * Must be followed by six f32 values: left, top, width, height, nearz, farz.
 */
#define GX_AURORA_LOAD_VIEWPORT_RENDER 0x0001

/**
 * Sets the actual render scissor in native framebuffer coordinates.
 * Must be followed by four u32 values: left, top, width, height.
 */
#define GX_AURORA_LOAD_SCISSOR_RENDER 0x0002

/**
 * Loads a full 4x4 projection matrix, bypassing GXSetProjection's 6-parameter
 * hardware encoding. Must be followed by sixteen f32 values in row-major order.
 */
#define GX_AURORA_LOAD_PROJECTION_FULL 0x0003

/**
 * Aurora equivalent of CP_REG_ARRAYBASE_ID: sets the base address and size of a vertex array.
 * This command must be followed by a 64-bit memory address, 32-bit size, and 1-byte little-endian flag.
 * The index of the vertex array is given by the lowest 4 bits of the command ID,
 * e.g. writing GX_AURORA_LOAD_ARRAYBASE + 5 will set the vertex array for the sixth vertex attribute.
 * To set strides, use the normal CP_REG_ARRAYSTRIDE_ID register.
 */
#define GX_AURORA_LOAD_ARRAYBASE 0x0010

/**
 * Pushes a debug group to the backend graphics API. These may show in debugging tools such as RenderDoc.
 * Must be followed by a u16 string length and that many UTF-8 characters (no null terminator required).
 * It is considered an error to have unpopped debug groups at the end of the frame. They will be automatically cleared.
 */
#define GX_AURORA_DEBUG_GROUP_PUSH 0x0020

/**
 * Pops a previously pushed debug group.
 * Followed by nothing.
 */
#define GX_AURORA_DEBUG_GROUP_POP 0x0021

/**
 * Sends a debug marker to the backend graphics API.
 * Must be followed by a u16 string length and that many UTF-8 characters (no null terminator required).
 */
#define GX_AURORA_DEBUG_MARKER_INSERT 0x0022

#define GX_AURORA_LOAD_TEXOBJ 0x0030

#define GX_AURORA_LOAD_TLUT 0x0031

#define GX_AURORA_DESTROY_TEXOBJ 0x0032

#define GX_AURORA_DESTROY_TLUT 0x0033

#define GX_AURORA_DESTROY_COPY_TEX 0x0034

#define GX_AURORA_LOAD_COPY_SRC 0x0035

#define GX_AURORA_LOAD_COPY_DST 0x0036

#define GX_AURORA_LOAD_COPY_DEST 0x0037

#define GX_AURORA_BEGIN_OFFSCREEN 0x0039

#define GX_AURORA_END_OFFSCREEN 0x003A

/**
 * Draw primitives with the vertex count derived from a byte length, as written by
 * GXBegin(prim, fmt, GX_AUTO). Must be followed by a u8 draw opcode (vtxfmt|prim),
 * a u32 vertex data byte length, then that many bytes of vertex data. The byte length
 * must be a whole multiple of the current vertex size or zero (no draw).
 */
#define GX_AURORA_DRAW_SIZED 0x0040

/**
 * Draw pre-merged triangles with a prebuilt index buffer, as written by the display
 * list optimizer (aurora::gx::dl::optimize). Must be followed by a u8 draw opcode
 * (vtxfmt | GX_TRIANGLES), a u16 vertex count, a u32 index count, that many u16
 * indices, then vertex count * vertex size bytes of packed vertex data. Index data
 * is always host-endian regardless of stream endianness.
 */
#define GX_AURORA_DRAW_INDEXED 0x0041

/** Sets whether subsequent draws wait for pipeline compilation. Followed by a u8 boolean. */
#define GX_AURORA_SET_DRAWS_BLOCKING 0x0042

/**
 * Asynchronous readback of EFB points.
 *
 * Command format:
 *   u64 stream
 *   u64 sequence
 *   u32 count
 *   u8  alphaReadMode            GXAlphaReadMode
 *   count times:
 *     u64 key
 *     u8  kind                   AuroraReadbackPeekKind
 *     u16 x
 *     u16 y
 */
#define GX_AURORA_READBACK_CAPTURE_PEEKS 0x0050

/**
 * Asynchronous readback of texture copies.
 *
 * Command format:
 *   u64 stream
 *   u64 sequence
 *   u32 count
 *   count times:
 *     u64 key
 *     u64 src                    GXCopyTex dest address
 */
#define GX_AURORA_READBACK_CAPTURE_COPIES 0x0051

/**
 * OrrePort, frame smoothing step (c) (docs/port/frame-smoothing-plan.md; 2026-11-16): the game set a camera. Followed
 * by a u64 camera id, an s32 render pass (HSD's: 0 = the screen), twelve f32 (the 3x4 view matrix, rows), a u8 that
 * is 1 when three f32 follow (the point the camera looks at). Observation only: it changes no GX state; Aurora tags the
 * uniform blocks built after it so in-between pictures can re-aim their draws at a blended camera. Written only while
 * frame smoothing is on (GXOrreSmoothCamera).
 */
#define GX_AURORA_ORRE_CAMERA 0x0060
/**
 * OrrePort, frame smoothing step (d) (docs/port/frame-smoothing-plan.md section 14; 2026-11-17): the game is about to
 * draw one HSD model piece. Followed by a u64 JObj and a u64 PObj (both 0: the piece is done). Observation only, like
 * GX_AURORA_ORRE_CAMERA: Aurora tags the uniform blocks built in between, so an in-between picture can find the same
 * piece in the step before and blend its matrices. Written only while frame smoothing is on (GXOrreSmoothModel).
 * *Extended 2026-11-17 (round 4, the faint and send-out tears):* then a u64 model root (the piece's top JObj: every
 * piece of one model shares it, so a model can snap whole) and a u32 mask of the position-matrix slots the piece's
 * display list uses (bit n = GX_PNMTX0 + 3n; the other slots hold whatever an earlier piece left there).
 */
#define GX_AURORA_ORRE_MODEL 0x0061
/**
 * OrrePort, frame smoothing item (a), the particle blend (docs/port/frame-smoothing-plan.md section 18; 2026-11-17):
 * the game is about to draw one particle. Followed by a u64 particle id (0: the particle is done), a u32 guard and a
 * u32 remaining life (a reused particle slot has another guard or a longer life). Observation only for the game; on
 * Aurora's side it makes the next draw start its own uniform block (no merging across particles), so an in-between can
 * move each particle on its own. Written only while frame smoothing is on (GXOrreSmoothParticle).
 */
#define GX_AURORA_ORRE_PARTICLE 0x0062
/**
 * OrrePort, frame smoothing round 11, 2D at 120 (docs/port/frame-smoothing-plan.md section 24; 2026-11-17): a named 2D
 * piece the following draws belong to. Followed by a u32 kind (0: the piece is done), a u32 variant (its picture), a
 * u32 id (its window) and an f32 value (a bar's drawn width, a panel's offset or fade). Observation only for the game;
 * on Aurora's side the next draw starts its own uniform block (as GX_AURORA_ORRE_PARTICLE). Written only while frame
 * smoothing's 2D is on (GXOrreSmooth2D).
 */
#define GX_AURORA_ORRE_2D 0x0063
/**
 * Set in a GX_AURORA_ORRE_2D kind (round 11, 2026-11-17): the piece blends by its vertices and matrices (each draw's
 * positions and colours, and its position matrix, between the step before and this one), not by a value: the pause
 * menu's buttons (slide and fade) and its darkening. Without it the piece is a bar, blended by its value (its width).
 */
#define GX_AURORA_ORRE_2D_VERTICES 0x80000000u

#define GX2_SET_POLYGON_OFFSET 0x1000

/*
 * Debug marker stuff
 */

/**
 * Pushes a debug group to the backend graphics API. These may show in debugging tools such as RenderDoc.
 * It is considered an error to have unpopped debug groups at the end of the frame. They will be automatically cleared.
 */
void GXPushDebugGroup(const char* label);

/**
 * Pop a debug group previously pushed via GXPushDebugGroup().
 */
void GXPopDebugGroup();

/**
 * Sends a debug marker to the backend graphics API. These may show in debugging tools such as RenderDoc.
 */
void GXInsertDebugMarker(const char* label);

typedef enum _AuroraViewportPolicy {
  AURORA_VIEWPORT_FIT = 0,     // Preserve logical aspect in the content framebuffer
  AURORA_VIEWPORT_STRETCH = 1, // Match content framebuffer aspect to the native surface
  AURORA_VIEWPORT_NATIVE = 2,  // Use active framebuffer pixels directly
} AuroraViewportPolicy;

/**
 * Configures content framebuffer sizing and how GXSetViewport/GXSetScissor parameters are applied to rendering.
 * When AURORA_VIEWPORT_NATIVE is used, GXSetTexCopySrc/GXSetTexCopyDst will use native framebuffer resolution.
 */
void AuroraSetViewportPolicy(AuroraViewportPolicy policy);

/**
 * Retrieves the current content framebuffer size.
 */
void AuroraGetRenderSize(u32* width, u32* height);

/**
 * Flush pending GX state and wait for FIFO processing without signaling a draw-done callback.
 */
void AuroraGXSync(void);
/* OrrePort (2026-10-19): called on the caller's thread each time GXSetDrawDone issues a draw-done token, so a host can
 * hand the draw-done callback to the game at a fixed point (deterministic replays). NULL: no hook. */
void AuroraSetDrawDoneIssuedHook(void (*hook)(void));

/**
 * When true, GX draws block and wait for their pipeline compilation to complete, if not already done.
 * When false, GX draws are skipped if their pipeline is not yet compiled.
 * Defaults to false ("async pipelines"). Applies to all future draws.
 */
void AuroraSetDrawsBlocking(bool blocking);

/**
 * Sets the actual render viewport in native framebuffer coordinates.
 * Overrides the automatically scaled values set by the logical GXSetViewport.
 */
void GXSetViewportRender(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);

/**
 * Sets the actual render scissor in native framebuffer coordinates.
 * Overrides the automatically scaled values set by the logical GXSetScissor.
 */
void GXSetScissorRender(u32 left, u32 top, u32 wd, u32 ht);

void GX2SetPolygonOffset(f32 mFrontOffset, f32 mFrontScale, f32 mBackOffset, f32 mBackScale, f32 mClamp);

/**
 * OrrePort, frame smoothing step (c): writes a GX_AURORA_ORRE_CAMERA marker (above) at this point of the GX stream.
 * `view` is a 3x4 view matrix (12 floats, rows); `interest` 3 floats or NULL. Does nothing inside a display list.
 */
void GXOrreSmoothCamera(u64 id, s32 renderPass, const f32* view, const f32* interest);
/**
 * OrrePort, frame smoothing step (d): writes a GX_AURORA_ORRE_MODEL marker (above): the HSD model piece the following
 * draws belong to (0, 0: none), its model's root JObj and the mask of matrix slots it uses (2026-11-17). Does nothing
 * inside a display list.
 */
void GXOrreSmoothModel(u64 jobj, u64 pobj, u64 root, u32 slots);
/**
 * OrrePort, frame smoothing item (a): writes a GX_AURORA_ORRE_PARTICLE marker (above): the particle the following draws
 * belong to (0: none), its guard and its remaining life (2026-11-17). Does nothing inside a display list.
 */
void GXOrreSmoothParticle(u64 particle, u32 guard, u32 life);
/**
 * OrrePort, frame smoothing round 11: writes a GX_AURORA_ORRE_2D marker (above): the 2D piece the following draws belong
 * to (kind 0: none), its variant, id and value (float bits). Does nothing inside a display list.
 */
void GXOrreSmooth2D(u32 kind, u32 variant, u32 id, u32 valueBits);

/**
 * Load an arbitrary 4x4 projection matrix, avoiding the 6-parameter hardware encoding.
 */
void GXSetProjectionFull(const void* mtx);

/**
 * Create an offscreen framebuffer and switch rendering to it.
 * All subsequent GX rendering will target this framebuffer until GXRestoreFrameBuffer() is called.
 * Use GXCopyTex to resolve the offscreen content into a texture.
 */
void GXCreateFrameBuffer(u32 width, u32 height);

/**
 * Restore rendering to the main EFB framebuffer.
 * Must be called after GXCreateFrameBuffer() to resume normal rendering.
 */
void GXRestoreFrameBuffer(void);

/*
 * Asynchronous readback APIs
 *
 * On original hardware, GXPeekZ and GXPeekARGB read the EFB contents directly, and GXCopyTex writes directly to
 * CPU-visible memory. On modern hardware, GPU work is broadly asynchronous and attempting to read data back right away
 * would stall our main thread until the GPU finishes all of its current work and writes the results, leaving the GPU
 * idle while we finish our current frame.
 *
 * Instead of trying to replicate the original behavior, Aurora's readback APIs request data and then allow it to be
 * read a few frames later, whenever the GPU has finished the work. For most cases, the delay is imperceptible: a
 * GXPeekZ that determines whether a lens flare is visible will simply have the visibility change delayed by a frame or
 * two.
 *
 * For simplicity's sake, the API is designed for multiple "peeks" or copies to be requested at once, where some games
 * may call GXPeekZ or GXCopyTex back-to-back for different points or formats/regions. Aurora will dispatch and deliver
 * the results in one go.
 *
 * To use it:
 * 1. Create a stream for each logical consumer, such as a lens flare probe.
 * 2. Where the game reads the data, first collect the results of an earlier capture with Get, then
 *    request a new one with Capture. A capture uses the EFB's contents at that point in the frame.
 * 3. Expect no results at first. Keep the previous value or use a default.
 *
 * Each item has a key that you choose and keep stable across frames, such as an actor ID. Results are
 * looked up by key. A stream only holds the results (and therefore keys) of its most recent completed capture.
 *
 *   // Once:
 *   AuroraReadbackStream stream = AuroraReadbackCreateStream();
 *
 *   // Each frame, where the game used to call GXPeekZ:
 *   u32 depth[2];
 *   AuroraReadbackDest results[] = {
 *       {0, &depth[0], sizeof(u32)},
 *       {1, &depth[1], sizeof(u32)},
 *   };
 *   if (AuroraReadbackGet(stream, results, 2)) {
 *     for (int i = 0; i < 2; ++i) {
 *       if (results[i].found) {
 *         visible[i] = depth[i] == 0xFFFFFF;
 *       }
 *     }
 *   }
 *   const AuroraReadbackPeekRequest points[] = {
 *       {0, AURORA_READBACK_PEEK_Z, x[0], y[0]},
 *       {1, AURORA_READBACK_PEEK_Z, x[1], y[1]},
 *   };
 *   AuroraReadbackCapturePeeks(stream, points, 2);
 */

/** Identifies a readback stream. 0 is never a valid stream. */
typedef u64 AuroraReadbackStream;

typedef enum _AuroraReadbackPeekKind {
  AURORA_READBACK_PEEK_Z,    // Depth (GXPeekZ)
  AURORA_READBACK_PEEK_ARGB, // Color (GXPeekARGB w/ alpha from GXPokeAlphaRead)
} AuroraReadbackPeekKind;

typedef struct {
  u64 key;
  AuroraReadbackPeekKind kind;
  u16 x;
  u16 y;
} AuroraReadbackPeekRequest;

typedef struct {
  u64 key;
  const void* src; // dest passed to GXCopyTex (unique identifier; not a real pointer)
} AuroraReadbackCopySrc;

typedef struct {
  u64 key;
  void* dest;   // Written when key found in the latest completed capture
  u32 size;     // Exact size of the result: sizeof(u32) for a peek, or the copy's size (see GXGetTexBufferSize)
  GXBool found; // Set to GX_TRUE when found & written
} AuroraReadbackDest;

/** Creates a stream for one consumer of readback results. */
AuroraReadbackStream AuroraReadbackCreateStream();
void AuroraReadbackDestroyStream(AuroraReadbackStream stream);

/**
 * Requests EFB values at this point in the frame's rendering.
 * Values are read back asynchronously and can be retrieved later with AuroraReadbackGet.
 *
 * Returns GX_TRUE if the capture was accepted. If GX_FALSE, either readback is unavailable (on this GPU, or with MSAA
 * enabled), or the request was invalid (and logged).
 */
GXBool AuroraReadbackCapturePeeks(AuroraReadbackStream stream, const AuroraReadbackPeekRequest* requests, u32 count);

/**
 * Requests the data of earlier GXCopyTex calls, as the hardware would have written it to main memory.
 * Texture data is read back asynchronously and can be retrieved later with AuroraReadbackGet.
 *
 * Returns GX_TRUE if the capture was accepted. If GX_FALSE, either readback is unavailable (on this GPU, or with MSAA
 * enabled), or the request was invalid (and logged).
 */
GXBool AuroraReadbackCaptureCopies(AuroraReadbackStream stream, const AuroraReadbackCopySrc* srcs, u32 count);

/**
 * Retrieves results from the stream's most recent completed capture. A peek result is a u32, as GXPeekZ or GXPeekARGB
 * returns it; a copy result is the texture data as GXCopyTex would have written it to main memory.
 *
 * Returns GX_TRUE if any were found. Each destination's `found` allows you to check individual results. A destination
 * whose size doesn't match its result is not found (and logged).
 */
GXBool AuroraReadbackGet(AuroraReadbackStream stream, AuroraReadbackDest* dests, u32 count);

#if __cplusplus
}
#endif

#endif
