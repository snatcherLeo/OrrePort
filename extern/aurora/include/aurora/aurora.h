#ifndef AURORA_AURORA_H
#define AURORA_AURORA_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>

extern "C" {
#else
#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#endif

typedef enum {
  SAMPLER_BILINEAR,
  SAMPLER_AREA,
} AuroraSampler;

typedef enum {
  BACKEND_AUTO,
  BACKEND_D3D11,
  BACKEND_D3D12,
  BACKEND_METAL,
  BACKEND_VULKAN,
  BACKEND_OPENGL,
  BACKEND_OPENGLES,
  BACKEND_WEBGPU,
  BACKEND_NULL,
} AuroraBackend;

typedef enum {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_FATAL,
} AuroraLogLevel;

typedef struct {
  int32_t x;
  int32_t y;
} AuroraWindowPos;

typedef struct {
  uint32_t width;
  uint32_t height;

  /**
   * Width of the main GX framebuffer.
   */
  uint32_t fb_width;

  /**
   * Height of the main GX framebuffer.
   */
  uint32_t fb_height;

  /**
   * The size of the framebuffer used to present to the operating system.
   * May differ from fb_width if Aurora is instructed to force an aspect ratio or resolution configuration.
   */
  uint32_t native_fb_width;

  /**
   * The size of the framebuffer used to present to the operating system.
   * May differ from fb_height if Aurora is instructed to force an aspect ratio or resolution configuration.
   */
  uint32_t native_fb_height;
  float scale;
} AuroraWindowSize;

typedef struct SDL_Window SDL_Window;
typedef struct AuroraEvent AuroraEvent;

typedef void (*AuroraLogCallback)(AuroraLogLevel level, const char* module, const char* message, unsigned int len);
typedef void (*AuroraImGuiInitCallback)(const AuroraWindowSize* size);

#define MEM1_DEFAULT_SIZE (24 * 1024 * 1024)
#define ARAM_DEFAULT_SIZE (16 * 1024 * 1024)

typedef struct {
  const char* appName;
  const char* userPath;
  const char* cachePath;
  const char* resourcesPath;
  AuroraBackend desiredBackend;
  uint32_t msaa;
  uint16_t maxTextureAnisotropy;
  bool vsync;
  bool startFullscreen;
  bool allowJoystickBackgroundEvents;
  bool pauseOnFocusLost;
  bool allowTextureDumps;
  bool allowCpuAdapter;
  int32_t windowPosX;
  int32_t windowPosY;
  uint32_t windowWidth;
  uint32_t windowHeight;
  void* iconRGBA8;
  uint32_t iconWidth;
  uint32_t iconHeight;
  AuroraLogCallback logCallback;
  AuroraLogLevel logLevel;
  AuroraImGuiInitCallback imGuiInitCallback;

  /*
   * The size of the GameCube's main memory, or MEM1 on the Wii.
   * Note that it will not be allocated at the exact 0x80000000 address, as that cannot be guaranteed.
   * This can be set to 0 to disable allocating this region.
   */
  uint32_t mem1Size;

  /*
   * The size of the GameCube's ARAM, or MEM2 on the Wii.
   * This can be set to 0 to disable allocating this region.
   */
  uint32_t mem2Size;

  /*
   * OrrePort: optional memory to use as MEM1 instead of letting Aurora allocate it. It must be at least mem1Size
   * bytes, committed, read / write and zeroed, and stay valid until shutdown. A game that keeps pointers in 32-bit
   * slots passes memory it mapped below 4 GB (OrrePort maps it at the real 0x80000000). NULL: Aurora allocates.
   */
  void* mem1Base;

  /*
   * OrrePort: optional arena bounds, as byte offsets from the start of MEM1, used by OSInit instead of the defaults
   * (low = MEM1 + 0x4000, high = the end of MEM1). A game whose data must sit at its console addresses inside MEM1
   * passes the console's own values (OrrePort: the retail __ArenaLo / __ArenaHi, so main.dol's data fits below the
   * arena). 0: the default.
   */
  uint32_t arenaLoOffset;
  uint32_t arenaHiOffset;

  /*
   * OrrePort: which GPU to use: NULL or "high-performance" (the discrete GPU when one can present to the window) or
   * "low-power" (the integrated GPU; the low-end test path).
   */
  const char* gpuPreference;

  /*
   * OrrePort: optional, called once with the new window while it is still hidden, just before Aurora shows it, so the
   * application can set its icon, title and title-bar style first and the window never appears with the defaults.
   */
  void (*windowCreatedCallback)(SDL_Window* window);
} AuroraConfig;

typedef struct {
  AuroraBackend backend;
  const char* userPath;
  const char* cachePath;
  SDL_Window* window;
  AuroraWindowSize windowSize;
} AuroraInfo;

AuroraInfo aurora_initialize(int argc, char* argv[], const AuroraConfig* config);
void aurora_shutdown();
const AuroraEvent* aurora_update();
bool aurora_begin_frame();
void aurora_end_frame();
/* OrrePort: while the game is held mid-frame, re-present the last finished frame with ImGui over it, leaving the
 * game's open frame untouched (begin starts an ImGui frame; present draws it over the last picture). */
// OrrePort: the next aurora_begin_frame keeps the EFB (colour and depth) instead of clearing it, for a frame that does
// not start at a display copy with clear (GameCube semantics: only a copy with clear empties the EFB)
void aurora_keep_efb_next_frame();
void aurora_overlay_frame_begin();
bool aurora_overlay_frame_present();

void aurora_set_log_level(AuroraLogLevel level);
void aurora_set_pause_on_focus_lost(bool value);
void aurora_set_background_input(bool value);
void aurora_set_resampler(AuroraSampler sampler);
/** Sets the clock timescale. Default 1.0f. 0.0f is paused. Range 0.0f-16.0f. */
void aurora_set_timescale(float scale);
/** OrrePort (2026-11-13): keeps the top `pixels` of the window free of the game picture, which is fitted into the
 * space below (for a menu bar that is always shown). Only the game picture moves; overlays still get the whole
 * window. 0 (the default) is the whole window. */
void aurora_set_present_inset_top(uint32_t pixels);
/** OrrePort (2026-11-16, a test switch, never a setting): at the end of the current frame, writes the game picture at
 * full resolution as two binary PPM files: `<pathPrefix>_render.ppm` (the frame buffer at the render size, before it
 * is scaled to the window) and `<pathPrefix>_window.ppm` (the same after the present resampler, at the size it takes
 * in the window, bars not included). The GX frame dumps are 640x480 copies, which hide every resolution effect. */
void aurora_orre_request_frame_dump(const char* pathPrefix);

/* OrrePort, frame smoothing steps (a) and (b) (docs/port/frame-smoothing-plan.md; 2026-11-16). The game steps once and
 * draws once; Aurora keeps that step's recording, and in-between pictures draw it again on Aurora's render worker,
 * without running any game code. Steps (a) and (b): the clock, the presenter and the identical picture; step (c)
 * (2026-11-16): in-between pictures re-aim the step's 3D draws at a camera blended between the previous step's and
 * the newest's (gfx/smooth_camera.hpp); models, 2D, text and fades keep the step's own. The port's side (port/src/pc_smooth.c) decides the mode and the rate, stamps each step and writes the log.
 * Off by default: nothing is kept and every step presents at once, as before. Platform-neutral (WebGPU, SDL's clock). */
enum {
  AURORA_SMOOTH_OFF = 0,
  AURORA_SMOOTH_LIVE = 1,   /* a presenter shows pictures at `pictureHz`: the newest step, or an in-between */
  AURORA_SMOOTH_FORCED = 2, /* test: every step is followed by `inBetweens` in-between pictures at once, and
                             * aurora_smooth_wait makes the game wait for them (recorded runs, whose clock is not the
                             * screen's) */
};
/* the step's own flags (aurora_smooth_step): why the step can't be drawn again, so its in-betweens repeat the picture */
enum {
  AURORA_SMOOTH_STEP_NO_COPY = 1, /* ended at the retrace without the game's display copy */
  AURORA_SMOOTH_STEP_MOVIE = 2,   /* a movie frame */
  AURORA_SMOOTH_STEP_PAUSE = 4,   /* a frame of the Settings overlay's pause */
  AURORA_SMOOTH_STEP_RESET = 8,   /* step (c): history resets here (room change, speed change): the camera snaps */
};
/* step (c): why a camera of the newest step was not blended (AuroraSmoothStats.cuts; gfx/smooth_camera.hpp, Cut) */
enum {
  AURORA_SMOOTH_CUT_NONE = 0, /* blended */
  AURORA_SMOOTH_CUT_UNCHANGED,
  AURORA_SMOOTH_CUT_RESET,
  AURORA_SMOOTH_CUT_UNMATCHED,
  AURORA_SMOOTH_CUT_NOT_RIGID,
  AURORA_SMOOTH_CUT_NO_TARGET,
  AURORA_SMOOTH_CUT_ROTATION,
  AURORA_SMOOTH_CUT_EYE,
  AURORA_SMOOTH_CUT_TARGET,
  AURORA_SMOOTH_CUT_FOV,
  AURORA_SMOOTH_CUT_COUNT,
};
/* why an in-between picture repeated the last picture instead of drawing the step again (AuroraSmoothStats.held) */
enum {
  AURORA_SMOOTH_HELD_KEPT_EFB = 0, /* the step began from the previous step's picture (aurora_keep_efb_next_frame) */
  AURORA_SMOOTH_HELD_NO_COPY,
  AURORA_SMOOTH_HELD_MOVIE,
  AURORA_SMOOTH_HELD_PAUSE,
  AURORA_SMOOTH_HELD_UI,      /* a UI frame with a backdrop was on screen: only with the test switch ORRE_SMOOTH_HOLD_UI=1
                                 since 2026-11-19 (frame-smoothing-plan.md section 33: in-betweens redraw the UI) */
  AURORA_SMOOTH_HELD_RESIZED, /* the frame buffers were re-created since the step */
  AURORA_SMOOTH_HELD_STALE,   /* the step is too many frames old to draw again */
  AURORA_SMOOTH_HELD_NONE,    /* no step kept yet */
  AURORA_SMOOTH_HELD_COUNT,
};
typedef struct AuroraSmoothStats {
  uint64_t steps;         /* steps ended while smoothing was on */
  uint64_t stepPictures;  /* steps shown */
  uint64_t stepsNotShown; /* steps replaced by a newer one before their picture slot (steps outpace pictures) */
  uint64_t inBetweens;    /* in-between pictures that drew the newest step again */
  uint64_t held[AURORA_SMOOTH_HELD_COUNT]; /* in-between pictures that repeated the last picture, by reason */
  uint64_t notPresented;  /* pictures the window could not take (minimized, surface lost) */
  uint64_t slips;         /* pictures shown more than half a picture period after their slot */
  uint64_t replayNs;      /* render-worker time drawing the in-betweens again (recording the GPU work, CPU side) */
  uint64_t replays;       /* in-betweens counted in replayNs */
  uint64_t replayDraws;   /* draws recorded again by those in-betweens */
  uint64_t presentNs;     /* render-worker time for the window picture, submit and present, every picture */
  uint64_t presents;      /* pictures counted in presentNs */
  uint64_t worstGapNs;    /* the longest time between two pictures since the last aurora_smooth_get_stats call */
  /* step (c), the camera blend (cumulative) */
  uint64_t blended;       /* in-betweens drawn with at least one camera re-aimed */
  uint64_t repeats;       /* later pictures of a step shown without drawing it again (nothing moved) */
  uint64_t restores;      /* unblended redraws before the next step (the frame buffer back to the step's own picture) */
  uint64_t cameras;       /* screen cameras planned, one per newest step's camera */
  uint64_t cuts[AURORA_SMOOTH_CUT_COUNT]; /* those cameras by outcome (AURORA_SMOOTH_CUT_NONE: blended) */
  uint64_t reaimedBlocks; /* uniform blocks re-aimed, all pictures */
  uint64_t looseBlocks;   /* blocks left as drawn: another perspective projection under a moving camera (battle HUD) */
  uint64_t overflowBlocks; /* left unblended: the blend buffer was full */
  uint64_t blendBytes;    /* bytes written into the blend buffer */
  /* step (d), the model blend (cumulative; 2026-11-17). Per step, the newest step's blocks drawn with their camera's
   * own projection: */
  uint64_t modelTagged;   /* of an HSD model piece (OrreHook_ModelDraw) */
  uint64_t modelUntagged; /* of other draws (particles, the game's own GX drawing): camera re-aim only */
  uint64_t modelMatched;  /* tagged and found in the step before */
  uint64_t modelMoved;    /* matched and moved: blended */
  uint64_t modelSnapped;  /* tagged, in a model snapped whole (*changed 2026-11-17, round 4*: was matched blocks with
                           * a matrix that jumped, which snapped alone) */
  uint64_t modelHeld;     /* matched, but the camera was cut or reset (the newest pose) */
  uint64_t modelsSnapped; /* round 4: models snapped whole (something jumped, or a new piece had nothing to blend from) */
  uint64_t modelBorrowed; /* round 4: rigid pieces new this step that blended from their joint's matrix before */
  uint64_t captureCopies; /* round 4: capture-target copies (saved before a step, put back before each replay) */
  uint64_t texFollows;    /* round 5: blended blocks whose position / normal texture matrices followed the piece */
  uint64_t modelBlockWrites; /* model-blended blocks written, all pictures */
  uint64_t looseSameFrustum; /* blocks left as drawn whose projection is the camera's frustum with other depth planes */
  uint64_t looseTagged;   /* blocks left as drawn that belong to an HSD model piece */
  /* item (a), the particle blend (cumulative; 2026-11-17), per step: particle blocks under a screen camera's own
   * projection with a known middle, and of those: blended, new this step (nothing to blend from), a reused particle
   * slot (another guard or a longer life), moved too far (snapped), held for a camera cut or reset */
  uint64_t particleTagged;
  uint64_t particleBlended;
  uint64_t particleNew;
  uint64_t particleReused;
  uint64_t particleJumped;
  uint64_t particleHeld;
  /* item (b), scrolling textures (cumulative; 2026-11-17), per step: matched model pieces' texture matrices fed by
   * texture coordinates that changed and blend, of those how many across a wrap (shifted by whole repeats), and how
   * many changed too much in one step (the newest is shown) */
  uint64_t scrollBlended;
  uint64_t scrollWrapped;
  uint64_t scrollSnapped;
  uint64_t scrollUnsteady; /* round 9 (2026-11-17): changed, but not by about the step before's change (a picture
                            * atlas switching cells, the first step of a motion): the newest shown */
  uint64_t lockedBlocks;   /* round 10 (2026-11-17): blocks of models moving with their camera (the bike rides), drawn
                            * as the step drew them so they keep their place on screen */
  /* round 11, 2D at 120 (cumulative; 2026-11-17), per step: named 2D pieces seen (OrreHook_2DPiece), blended (their
   * value changed), new, with a changed picture (the newest shown); blocks written blended, all pictures */
  uint64_t p2Tagged;
  uint64_t p2Blended;
  uint64_t p2New;
  uint64_t p2Swapped;
  uint64_t p2Written;
  /* round 11, vertex mode (the pause menu), per step: draws whose positions / colours blend, draws shown newest (their
   * layout or pictures changed), blocks whose position matrix blends; per in-between: pieces written blended */
  uint64_t p2VtxDraws;
  uint64_t p2VtxSnapped;
  uint64_t p2MtxBlocks;
  uint64_t p2VtxWritten;
  uint64_t p2NoSpan; /* round 11: named pieces whose draws' x couldn't be read (an index or unknown vertex layout):
                      * never blended, so a bar that never blends shows here instead of silently */
  /* item (c), menu models (cumulative; 2026-11-17), per step: blocks and models (drawn only by a camera of HSD's
   * offscreen pass, posed on every other step) blended over two steps with the two-step history */
  uint64_t twoStepBlocks;
  uint64_t twoStepModels;
  /* the colour-data blend (cumulative; 2026-11-18), per step: matched model blocks whose colour data (TEV colour
   * registers, light colours, channel and konstant colours) changed and blends, and those changed but shown newest
   * (another colour layout, or a value that is not a number) */
  uint64_t colorBlended;
  uint64_t colorHeld;
  /* (3b), the particle corners (cumulative; 2026-11-18), per step: particles blended by their corners */
  uint64_t cornerParticles;
  /* the coverage measure: in-between pictures (not a step's own) that moved something, and those with a model */
  uint64_t ibBlended;
  uint64_t ibModels;
  /* pacing (each reset by aurora_smooth_get_stats, like worstGapNs) */
  uint64_t lateStarts;    /* picture slots handled more than 2 ms late */
  uint64_t lateByItem;    /* of those, with a render-worker queue item running across the slot */
  uint64_t worstLateNs;   /* the latest slot */
  uint64_t worstPictureNs; /* the longest picture (redraw, window picture, submit, present) */
  uint64_t worstAcquireNs; /* the longest wait for the window's next texture */
  uint64_t worstPresentNs; /* the longest present call */
  uint64_t worstSubmitNs; /* the longest queue submit */
  /* the step-to-screen delay (2026-11-17, live mode; reset by each call): from a step's arrival to its whole picture */
  uint64_t delaySumNs, delayMaxNs, delayRetSumNs, delayCount;
  uint64_t lateArrivals;  /* steps that arrived after their (moved) anchor */
  int64_t leadNs;         /* how much earlier the anchor is than the retrace after the step (step c's anchor) */
  uint64_t fracZero, fracMid, fracOne; /* live pictures by where they fall between the two steps (reset by each call) */
  uint64_t reanchors;     /* anchors started again at the step's arrival (they had drifted over 4 ms past it) */
  uint64_t retimes;       /* round 5: pairs on screen re-timed for a game-speed change (reset by get_stats) */
  int64_t itemMaxNs[5];   /* render-worker queue items by kind: begin frame, encode pass, end frame, sync, shutdown */
  int64_t itemTotalNs[5];
  uint64_t itemCount[5];
} AuroraSmoothStats;
/* step (c): the last camera cut found (a jump: rotation, eye, target or field of view), for the port's event line */
typedef struct AuroraSmoothCut {
  uint64_t sequence; /* counts up with each cut; unchanged = no new cut */
  int reason;        /* AURORA_SMOOTH_CUT_* */
  uint32_t retrace;  /* the step's stamp */
  float degrees, eyeJump, targetJump, distance, fovRatio;
} AuroraSmoothCut;
void aurora_smooth_last_cut(AuroraSmoothCut* out);
/* any thread; pictureHz: the live presenter's rate (0 = no pacing); inBetweens: the forced mode's count */
void aurora_smooth_configure(int mode, uint32_t pictureHz, uint32_t inBetweens);
/* steps come faster than pictures: no in-betweens; the newest step is shown at each picture slot */
void aurora_smooth_set_outpaced(bool outpaced);
/* a pause (the debug toolkit's, the Settings overlay's): each step presents at once and no in-betweens are drawn */
void aurora_smooth_hold(bool held);
/* game thread, just before aurora_end_frame: the step's retrace count, AURORA_SMOOTH_STEP_* flags, and (step c) its
 * anchor: when the step is due on screen, SDL_GetTicksNS time (the next retrace's due time). Pictures run one step
 * behind: a picture at time t shows the camera (t - anchor) / (anchor - previous anchor) of the way from the previous
 * step's to this one's, clamped to 0-1 (Dusklight's clock; the plan, section 2) */
void aurora_smooth_step(uint32_t retrace, uint32_t flags, uint64_t anchorNs);
/* round 5 (2026-11-17), game thread: the length of one retrace at the game's current speed, in ns (16.68 ms at 1x),
 * set whenever the speed changes (and at start). A step is stamped with it (a pair's length = its retrace gap times
 * it), and a change re-times the pair on screen at once, so letting go of fast-forward slows the motion without a
 * held picture. 0: not known (the pair lengths are then measured from the due times, as in round 4). nextStepDueNs:
 * at a change, when the next step's last retrace is due at the new speed (SDL_GetTicksNS time; 0 = not known): the
 * re-timed pair ends as that step arrives */
void aurora_smooth_field_ns(uint64_t fieldNs, uint64_t nextStepDueNs);
/* game thread, after aurora_end_frame: in the forced mode, waits until the step's in-betweens are done */
void aurora_smooth_wait(void);
/* the tripwire (docs/port/frame-smoothing-plan.md section 5): true on the render worker, and in the forced mode while
 * an in-between picture is being made; the game's random-number hook calls it and stops the program when true */
bool aurora_smooth_in_between_now(void);
/* test switch: at the end of the current step, writes the frame buffer as `<prefix>_step.ppm`, and after each of its
 * in-betweens as `<prefix>_ib<n>.ppm`, so identical pictures can be compared byte for byte */
void aurora_smooth_request_dump(const char* prefix);
/* test switch: 1 paints the frame buffer magenta before each in-between's replay, so a matching dump proves the replay
 * redrew the whole picture; 2 paints it and skips the replay (the control's own control: dumps must not match);
 * 0 off */
void aurora_smooth_test_wipe(int mode);
/* cumulative counters (worstGapNs is reset by each call) */
void aurora_smooth_get_stats(AuroraSmoothStats* out);
/* the window's present mode ("Fifo", "Mailbox", "Immediate", ...) for the log */
const char* aurora_smooth_present_mode(void);

SDL_Window* aurora_get_window();
AuroraWindowSize aurora_get_window_size();

AuroraBackend aurora_get_backend();
const AuroraBackend* aurora_get_available_backends(size_t* count);
float aurora_get_timescale();

#ifdef __cplusplus
}
#endif

#endif
