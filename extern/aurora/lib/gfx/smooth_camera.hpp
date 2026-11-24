#pragma once

// OrrePort, frame smoothing step (c): the camera blend (docs/port/frame-smoothing-plan.md section 13; 2026-11-16).
//
// What it does: an in-between picture draws the newest kept step again (step b) with its 3D draws re-aimed at a camera
// part of the way from the previous step's camera to the newest one. Models are not blended (step d): every draw keeps
// the step's own model-to-camera matrix, so only the camera moves smoothly.
//
// How: the game's camera is known from OrreHook_CameraSet (HSD_CObjSetCurrent), written into the GX stream as a marker
// (GX_AURORA_ORRE_CAMERA) and kept with the step (frame_packet.hpp, OrreSmoothCamera). Each uniform block built under
// a perspective screen camera was copied on the CPU (recording.cpp, orre_smooth_uniform). A draw's position matrix is
// M = V_new x W (the step's view times the model's world matrix), and the shader computes P x M x v. With a blended
// view V_blend, the re-aimed position is V_blend x W = A x M, A = V_blend x V_new^-1, so only the projection needs
// replacing: P' = P_blend x A. That leaves the position and normal matrices, and the lights (in the step's view space),
// exactly as the step had them, so the lighting is the step's own. The changed blocks are written into a uniform
// buffer of their own (one write per picture) and the replayed draws are pointed there (pipeline.cpp, render).
// V_blend is rebuilt from blended eye, target and up (C_MTXLookAt's convention), as Dusklight does, rather than by
// blending matrix numbers, which shrinks the picture on fast turns.
//
// Cuts snap: a camera not in the previous step, not rigid, or that jumped, and every camera after a reset (room change,
// speed change, a gap over 250 ms, a step that couldn't be drawn again), keeps the step's own camera. A jump is big and
// sudden: rotation over 35 degrees, eye or target moved more than a quarter of the eye-to-target distance (a fixed 4
// units for a camera without a target), field of view over 25 %; and over three times the same camera's move in the
// step before, so a camera tracking something fast blends (measured: P01's opening moves 30 % of its distance a step).
// A camera without a usable target blends around its eye. Render worker only. Platform-neutral (WebGPU).
//
// Step (d), the model blend (2026-11-17; the plan's section 14): without it the camera blends but every model keeps
// the newest step's pose, so anything the camera follows (Wes walking, a Pokemon's idle sway) is drawn one step ahead
// of the camera at the start of each step and catches up by its end: a back-and-forth at the step rate, which is what
// made snatcherLeo's first hand check "dizzying". Now each HSD model piece is tagged (OrreHook_ModelDraw, HSD_PObjDisp, as a
// GX_AURORA_ORRE_MODEL marker) and its uniform block matched with the same piece's block in the step before. The
// step before's matrices are carried into the newest step's camera space (B = V_new x V_old^-1, so the blend is a
// blend of world poses), then each of the ten position matrices and normal matrices is blended number by number with
// the same fraction as the camera, its axes' lengths kept (no shrink on turns). The camera re-aim (the projection) goes
// on top as before. A matrix snaps to the newest when it jumped (over 3/4 of its distance from the camera in one step,
// or an axis turned over 90 degrees), and every piece snaps when its camera was cut or reset. Draws outside HSD's model
// path (particles, the game's own GX drawing) still get only the camera re-aim. *Since 2026-11-17 (item a):* particles
// are moved too: each particle is tagged (OrreHook_ParticleDraw, psDispMain, as a GX_AURORA_ORRE_PARTICLE marker), the
// middle of its drawn corners is kept with its blocks, and an in-between moves its draws part of the way back to where
// that middle was in the step before (plan_models says how).
//
// Round 4 (2026-11-17; the plan's section 16): a model blends whole or snaps whole. Round 3 snapped single matrices
// inside a piece, matched blocks by a per-block count that shifted when a piece's draw needed one more block or a
// shadow caster started or stopped, and showed a new piece of an existing model at its newest pose: each put parts of
// one model at different poses in the same in-between, which tore it at the seams (Nuzleaf's white line and Xatu's
// clipping in faints and send-outs). Now blocks match by (JObj, PObj, render pass, draw number), only the matrix slots
// a piece uses count (the marker says which), a new rigid piece borrows its joint's matrix of the step before, the
// jump test measures moves against the model's depth, and anything that jumps snaps the whole model (its root JObj,
// every pass) for that step; a new piece with nothing to blend from snaps the model in its own pass only (HSD's shadow
// pass switches single caster pieces on and off), and a model held for a screen-camera cut has its casters held too.

#include "frame_packet.hpp"

#include <cstdint>
#include <vector>

namespace aurora::gfx::smooth_camera {

// why a camera of the newest step is not blended (AURORA_SMOOTH_CUT_* in aurora.h use the same order)
enum class Cut : uint8_t {
  None = 0,  // blended
  Unchanged, // the same camera as the previous step: nothing to re-aim
  Reset,     // history was reset (room change, speed change, gap, held step)
  Unmatched, // not set in the previous step
  NotRigid,  // its view matrix has scale or shear
  NoTarget,  // not a cut: blended around its eye, having no usable point it looks at (counted apart, and as None too)
  Rotation,  // turned more than 35 degrees in one step, and over 3x its turn in the step before
  Eye,       // the eye jumped (over a quarter of the eye-to-target distance, and over 3x its move the step before)
  Target,    // the target jumped (the same)
  Fov,       // the projection changed by more than 25 %, or isn't a plain perspective one
  Count,
};

struct Stats {
  uint64_t cameras = 0;                          // screen cameras planned (one per newest step's camera)
  uint64_t cuts[static_cast<int>(Cut::Count)]{}; // by reason (Cut::None = blended)
  uint64_t blocks = 0;                           // uniform blocks re-aimed, all pictures
  uint64_t looseBlocks = 0;                      // left as drawn: another perspective projection (battle HUD)
  uint64_t overflow = 0;                         // blocks left unblended because the blend buffer was full
  uint64_t bytes = 0;                            // bytes written into the blend buffer
  // step (d), the model blend (per step: the newest step's blocks drawn under a usable camera's own projection)
  uint64_t modelTagged = 0;    // blocks of an HSD model piece (OrreHook_ModelDraw's markers)
  uint64_t modelUntagged = 0;  // blocks of other draws (particles, the game's own GX drawing): camera re-aim only
  uint64_t modelMatched = 0;   // tagged blocks found in the step before (same piece, same occurrence)
  uint64_t modelMoved = 0;     // matched blocks with at least one matrix that moved and blends
  uint64_t modelSnapped = 0;   // tagged blocks in a model snapped whole (round 4; was: blocks with a matrix that jumped)
  uint64_t modelHeld = 0;      // matched, but their camera was cut or reset: the newest pose (no blend)
  uint64_t modelsSnapped = 0;  // round 4: models snapped whole
  uint64_t modelBorrowed = 0;  // round 4: new rigid pieces that blended from their joint's matrix of the step before
  uint64_t texFollows = 0;     // round 5: blended blocks whose position / normal texture matrices followed the piece
  uint64_t modelBlockWrites = 0; // model-blended blocks written, all pictures
  // diagnostics of the blocks left as drawn (looseBlocks), per picture: same frustum as the camera but other depth
  // planes (likely a world draw: a sky or a backdrop), and how many of the loose ones belong to an HSD model piece
  uint64_t looseSameFrustum = 0;
  uint64_t looseTagged = 0;
  // item (a), the particle blend (2026-11-17; aurora.h says what each counts)
  uint64_t particleTagged = 0, particleBlended = 0, particleNew = 0, particleReused = 0, particleJumped = 0,
           particleHeld = 0;
  // item (b), scrolling textures (2026-11-17), per step: texture matrices fed by texture coordinates that blend, of
  // those how many across a wrap (shifted by whole repeats), and how many changed too much and show the newest
  uint64_t scrollBlended = 0, scrollWrapped = 0, scrollSnapped = 0;
  uint64_t scrollUnsteady = 0; // round 9: changed but not steadily (an atlas cell change): the newest shown
  uint64_t lockedBlocks = 0;   // round 10: blocks of models moving with their camera, drawn as the step drew them
  // round 11, 2D at 120, per step: named 2D pieces seen, blended (value changed), new, picture changed; blocks written
  uint64_t p2Tagged = 0, p2Blended = 0, p2New = 0, p2Swapped = 0, p2Written = 0, p2NoSpan = 0;
  uint64_t p2VtxDraws = 0, p2VtxSnapped = 0, p2MtxBlocks = 0, p2VtxWritten = 0; // round 11, vertex mode
  // item (c), menu models (2026-11-17), per step: blocks and models blended with the two-step history
  uint64_t twoStepBlocks = 0, twoStepModels = 0;
  // the colour-data blend (2026-11-18), per step: matched model blocks whose colours changed and blend / show newest
  uint64_t colorBlended = 0, colorHeld = 0;
  uint64_t cornerParticles = 0; // (3b), 2026-11-18, per step: particles blended by their corners
};

// The last cut found (for the port's event line)
struct CutInfo {
  uint64_t sequence = 0; // counts up with each cut
  Cut reason = Cut::None;
  uint32_t retrace = 0;  // the step's stamp
  float degrees = 0.f;   // the turn
  float eyeJump = 0.f;   // eye and target moves and the eye-to-target distance, world units
  float targetJump = 0.f;
  float distance = 0.f;
  float fovRatio = 1.f;
};

// After a step is kept (end_frame's job): match its cameras with `previous` (the step before's) and decide which blend.
// `history`: false after a reset (all snap).
// Step (d): `previousBlocks` / `previousBytes` are the step before's tagged blocks and their CPU copies; each model
// piece's block is matched with the same piece's there, and the matrices it will blend from are worked out once here.
// Item (c) (2026-11-17): the step two back (cameras, tagged blocks and their copies), for the two-step history of
// menu models posed on every other step (plan_models)
struct TwoBack {
  const std::vector<detail::OrreSmoothCamera>& cameras;
  const std::vector<detail::OrreSmoothBlock>& blocks;
  const std::vector<uint8_t>& bytes;
};
void plan_step(const detail::FramePacket& step, const std::vector<detail::OrreSmoothCamera>& previous,
               const std::vector<detail::OrreSmoothBlock>& previousBlocks, const std::vector<uint8_t>& previousBytes,
               bool history, uint32_t retrace, const TwoBack& twoBack);
// Builds the re-aimed uniform blocks for `fraction` (0 = the previous step's camera, 1 = the newest's) and writes them
// into the blend buffer. Returns the number of blocks re-aimed; 0 means the replay would be the step's own picture.
uint32_t prepare(const detail::FramePacket& step, float fraction);
// step (d): how many of the blocks the last prepare wrote had a model piece blended (0 after a picture that only
// re-aimed the camera); for the coverage counters
uint32_t last_model_writes() noexcept;
// item (c): true when the newest step's picture at fraction 1 must be drawn again too (a menu model in the first half
// of its two-step blend sits half way, not at the step's own pose); prepare(step, 1) then writes only those blocks
bool two_step_at_one() noexcept;
// Between these, replayed draws whose uniform block was re-aimed use the blend buffer (pipeline.cpp)
void begin_override() noexcept;
void end_override() noexcept;
// pipeline.cpp, every GX draw: true when the draw's block has a re-aimed copy (`offset` and `group` replaced)
bool override_uniform(uint32_t& offset, wgpu::BindGroup& group) noexcept;
// pipeline.cpp, every GX draw (round 11, 2D at 120's vertex mode: the pause menu): true when the draw's vertices have a
// blended copy (`vtxStart` replaced: the copy sits in the same vertex buffer, past the step's own vertices)
bool override_vertices(uint32_t& vtxStart) noexcept;
Stats stats() noexcept;
CutInfo last_cut() noexcept;
void shutdown() noexcept;

} // namespace aurora::gfx::smooth_camera
