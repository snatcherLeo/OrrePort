/*
 * pc_smooth.c: frame smoothing's port side, steps (a) and (b) of docs/port/frame-smoothing-plan.md (2026-11-16).
 *
 * What frame smoothing is: the game steps once and draws once (60 or 30 steps a second); a screen faster than that
 * can show pictures in between. Aurora keeps each step's recording and draws it again for the in-between pictures, on
 * its render worker, without running any game code (extern/aurora/lib/aurora.cpp, "Frame smoothing"; gfx/frame.cpp,
 * the kept step; gfx/encoding.cpp, encode_replay). Steps (a) and (b) build the clock, the presenter and the replay of
 * the identical picture: nothing is blended yet, so with smoothing on the screen shows the same pictures as without
 * it, more often. Step (c) blends the camera (the project's build helper, 2026-11-16; extern/aurora/lib/gfx/smooth_camera.hpp):
 * OrreHook_CameraSet (src/hsd/cobj.c) reports each camera the game sets, camera_set below writes it into the GX stream
 * as a marker, and Aurora re-aims the step's 3D draws at a camera blended between the previous step's and the newest
 * one's. Pictures then run one step behind (Dusklight's clock): each step is stamped with its anchor, the time it is
 * due on screen (the next retrace), and a picture at time t shows the camera (t - anchor) / (step length) of the way.
 * Room changes and speed changes reset the history (the camera snaps), as do Aurora's own checks (gaps, held steps,
 * jumps). Step (d) (2026-11-17) blends HSD models too: OrreHook_ModelDraw (src/hsd/pobj.c) reports each model piece,
 * model_draw below writes it into the GX stream, and Aurora blends the piece's matrices with the same piece's in the
 * step before, at the camera's fraction, so a model the camera follows no longer runs a step ahead of it (snatcherLeo's first
 * hand check: "dizzying"). Particles, 2D, text and fades keep the step's own.
 * Round 4 (2026-11-17; the plan's section 16): a model blends whole or snaps whole (the marker also says which model a
 * piece belongs to and which matrix slots it uses: pc_smooth_model_info, game side), the game's back-framebuffer
 * captures are saved before each kept step and put back before each replay (Aurora, gfx/frame.cpp), and a speed change
 * no longer resets the history (pc_smooth_frame). Observe-only switches added with it: ORRE_SMOOTH_MODELLOG,
 * ORRE_SMOOTH_PICLOG (Aurora) and ORRE_SMOOTH_SPEED_RESET (below).
 *
 * This file is the port's side: the switches, the picture rate, the step stamps, the pauses, the tripwire's check and
 * the "[smooth]" log lines. *Changed 2026-11-18 (step 3.4's Settings row):* the player's choice is Settings › Graphics
 * › Frame rate (pc_settings_overlay.cpp), saved as settings.ini "frame_smoothing" in ORRE_SMOOTH's own words (off,
 * match, 60 ... 240), "off" by default since 2026-11-23 (snatcherLeo: "the FPS should be on original"; "match" from
 * 2026-11-16, the decision log "Frame smoothing's dropdown keeps every entry"), changed live (pc_smooth_setting_changed; applied at the next step). The test switches below win over it,
 * and automated runs and games in GameCube-accurate mode don't read it (setting_choice). Test switches:
 *   ORRE_SMOOTH=match | <pictures a second> | off
 *       the live presenter: pictures paced to the screen ("match": the monitor's refresh rate, read from SDL) or to a
 *       cap (60, 120, 144, 165, 240, ...). With V-Sync on (strict Fifo) a cap above the screen's rate is lowered to it,
 *       since Fifo can't show more and a presenter waiting on it would hold back the steps' own work.
 *   ORRE_TEST_SMOOTH=<n> (0-7)
 *       recorded runs (the the reference playthrough gate): every step is followed at once by n in-between pictures and the game waits for
 *       them. Such runs use the replay clock, as fast as the CPU allows, in a minimized window, so the screen can't
 *       pace them; n = 1 stands for 120 and n = 3 for 240 in 60-a-second scenes, n = 0 runs the path with none.
 *   ORRE_SMOOTH_DUMP=<game frames, comma-separated>
 *       at those frames, the frame buffer after the step and after each of its in-betweens, as PPM files in
 *       smooth_dumps/ under the working folder (f<frame>_step.ppm, f<frame>_ib<k>.ppm): identical files prove the
 *       replay draws the identical picture. Works with smoothing off too (the step files only). *Since 2026-11-19
 *       (frame-smoothing-plan.md section 33):* while a UI frame with the game picture inside it is up (the Settings
 *       overlay's backdrop blur), the UI picture the window shows is dumped too (f<frame>_stepui.ppm,
 *       f<frame>_ib<k>ui.ppm): each in-between redraws the UI over its own blended picture.
 *   ORRE_SMOOTH_HOLD_UI=1 (Aurora, read once; 2026-11-19, section 33)
 *       the old rule for before / after measurements: in-betweens hold while such a UI frame is up ("held (UI)").
 *   ORRE_SMOOTH_TEST_WIPE=1
 *       the negative control for those dumps: the frame buffer is painted magenta before each replay, so an identical
 *       in-between file can only come from a replay that redrew the whole picture (without it, a replay that drew
 *       nothing would leave the step's own picture in the frame buffer and "match" too). ORRE_SMOOTH_TEST_WIPE=2
 *       paints and skips the replay: its dumps must come out magenta, which shows the control itself works.
 *
 * When steps come faster than pictures (the Enhancements' game speed, the debug speed): smoothing pauses itself; at
 * each picture slot the newest step is shown and the steps in between are drawn but not shown (Aurora counts them).
 * The step rate is the retrace rate (pc_vi.c: the GameCube's 59.94 a second times the speeds) over the step's length
 * in retraces (1, or 2 in 30-a-second scenes). Recorded runs at ORRE_REPLAY_SPEED=max step far faster than that
 * formula says; the live presenter then simply always has a new step at each slot.
 *
 * The log ("[smooth]", in the normal log, so it reaches every report): a line at start; each event at once (the rate
 * or the screen changing, pausing and resuming, game-speed resets; at most 500 a run, like [slowframe]); every 10 s a
 * summary: steps, pictures by kind, held pictures by reason, steps not shown, slips (a picture more than half a period
 * late), the worst gap between two pictures, and the render worker's average time per in-between and per picture.
 *
 * GameCube-accurate mode: smoothing changes pictures only, never game code, so the test switches work in accurate mode
 * (the the reference playthrough gate runs there). The player setting, when it comes, stays off in accurate mode until the gate passes
 * for its rate (the plan, section 7; the project notes's accurate-mode list).
 *
 * Platforms: plain C over Aurora's C API and SDL3; the same on Windows, Linux and macOS (the refresh rate from SDL's
 * display mode on all three; variable-refresh screens report their maximum, unknown whether that is right: the plan's
 * open question). The dump folder uses _mkdir on Windows and mkdir elsewhere.
 */
#include "pc_smooth.h"
#include "pc_options.h"  /* the Settings row: off in GameCube-accurate mode (pc_option_gamecube_accurate) */
#include "pc_settings.h" /* the Settings row: settings.ini "frame_smoothing" */

#include <aurora/aurora.h>
#include <dolphin/types.h>

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define smooth_make_dir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define smooth_make_dir(path) mkdir(path, 0755)
#endif

double pc_vi_fields_per_second(void); /* pc_vi.c: retraces a second at the current speeds */
u64 pc_vi_retrace_due_ns(void);       /* pc_vi.c: when the next retrace is due (SDL_GetTicksNS time) */
double pc_vi_retrace_clock(void);     /* pc_vi.c: the retrace count (with the field's fraction on the wall clock) */
extern volatile u32 pc_vi_heartbeat;  /* pc_vi.c: the game frame count */
void pc_log_drain(void);              /* pc_log.c */
int pc_platform_automated_run(void);  /* pc_platform.c: a test run (a pad script or a minimized window) */
/* step (c): the camera marker into the GX stream (extern/aurora's GXAurora.h, GX_AURORA_ORRE_CAMERA) */
void GXOrreSmoothCamera(u64 id, s32 renderPass, const f32* view, const f32* interest);
/* step (c): the game-code hook (pc/orre_hooks.h; defined in pc_cheats.c) */
extern void (*OrreHook_CameraSet)(const void* camera, s32 renderPass, const f32* view, const f32* interest);
/* step (d): the model marker and its game-code hook (GXAurora.h, GX_AURORA_ORRE_MODEL; pc/orre_hooks.h) */
void GXOrreSmoothModel(u64 jobj, u64 pobj, u64 root, u32 slots);
/* round 4 (2026-11-17): the piece's model root and used matrix slots, read on the game side (src/game/pc_debug_game.c,
 * compiled with the game's HSD struct layout). Weak, like HSD_CObjGetProjectionType below: the platform library's own
 * test programs link without the game code */
extern u32 pc_smooth_model_info(const void* jobj, const void* pobj, u64* root) __attribute__((weak));
/* src/hsd/cobj.c: 1 perspective, 2 frustum, 3 ortho (a read). Weak: the platform library's own test programs link
 * without the game code */
extern int HSD_CObjGetProjectionType(void* cobj) __attribute__((weak));
extern void (*OrreHook_ModelDraw)(const void* jobj, const void* pobj);
/* item (a), the particle blend (2026-11-17): the particle marker (GXAurora.h, GX_AURORA_ORRE_PARTICLE) and its
 * game-code hook (pc/orre_hooks.h; src/game/psdisp.c) */
void GXOrreSmoothParticle(u64 particle, u32 guard, u32 life);
extern void (*OrreHook_ParticleDraw)(const void* particle, u32 guard, u32 life);
/* round 11, 2D at 120 (2026-11-17): the 2D-piece marker (GXAurora.h, GX_AURORA_ORRE_2D) and its game-code hook
 * (pc/orre_hooks.h; played back from GSgfx's request buffer, src/game/gs_gfx_range_800D3FA4.c command 0x7E) */
void GXOrreSmooth2D(u32 kind, u32 variant, u32 id, u32 valueBits);
extern void (*OrreHook_2DPiece)(u32 kind, u32 variant, u32 id, u32 valueBits);
#ifndef GX_AURORA_ORRE_2D_VERTICES
#define GX_AURORA_ORRE_2D_VERTICES 0x80000000u /* GXAurora.h: the piece blends by vertices and matrices (menus) */
#endif

enum { MAX_EVENT_LINES = 500, MAX_DUMPS = 32768 }; /* dumps: 1024 since round 4 (a faint's whole length, several);
                                                     * 32,768 since round 10 (whole bike rides for the seam sweep) */
#define SUMMARY_NS 10000000000ull
#define RECHECK_NS 1000000000ull

static int s_parsed;
static int s_mode;         /* AURORA_SMOOTH_OFF / LIVE / FORCED: what the switches asked for */
static int s_match;        /* live: follow the screen's rate */
static unsigned s_cap;     /* live: the cap, pictures a second */
static unsigned s_forced;  /* test mode: in-betweens per step */
static int s_on;           /* Aurora configured (from the first game step: the hub's own frames stay as they were) */
static unsigned s_target;  /* pictures a second now (live) */
static double s_display;   /* the screen's refresh rate (0: unknown) */
static int s_outpaced = -1;
static double s_fields;    /* retraces a second at the last check */
static u32 s_last_retrace; /* the last display-copy step's retrace count */
static unsigned s_step_len = 1;
static int s_held;
static unsigned s_events;
static Uint64 s_summary_at, s_recheck_at;
static AuroraSmoothStats s_prev;
static unsigned long long s_rng_checked; /* random draws the tripwire looked at */
static u32 s_dumps[MAX_DUMPS];
static int s_dump_count;
/* step (c): a reset asked for since the last step (room change, speed change), sent with the next step's stamp */
static int s_reset_pending;
static unsigned long long s_last_cut_seq;

/* an event line, at once, at most MAX_EVENT_LINES a run */
static void smooth_event(const char* fmt, ...) {
    va_list args;
    if (s_events >= MAX_EVENT_LINES) {
        return;
    }
    ++s_events;
    fputs("[smooth] ", stderr);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (s_events == MAX_EVENT_LINES) {
        fputs(" (the last [smooth] event line this run; the 10 s summaries go on)", stderr);
    }
    fputc('\n', stderr);
}

/* The Settings row (2026-11-18): what the live presenter follows when no test switch is set.
 *   - Automated runs (a pad script, a minimized test window) never read it, as they ignore the Graphics rows
 *     (pc_platform.c), so a recorded check can't change with whatever a player's settings.ini holds.
 *     ORRE_SMOOTH_SETTING_TEST=1 (a test switch) reads it anyway, for checking the row itself in a minimized run.
 *   - GameCube-accurate mode follows the row too (*changed 2026-11-20*). Until then an accurate-mode game played with it
 *     off, until the the reference playthrough gate passed for the rates (the plan, section 7). It has now passed: every the reference playthrough sitting at
 *     0, 1, 3 and 7 in-betweens, which bound every offered rate, gives identical rolls and rooms (plan section 34). So
 *     smoothing is a "still works" row in pc_options.c's accurate-mode list (same_display).
 * Platform-neutral. */
static int s_from_setting;             /* the live choice came from the Settings row, not a test switch */
static volatile int s_setting_changed; /* pc_smooth_setting_changed: read the row again at the next step */

static const char* setting_choice(void) {
    const char* test = getenv("ORRE_SMOOTH_SETTING_TEST");
    if (pc_platform_automated_run() && !(test != NULL && test[0] == '1')) {
        return "off";
    }
    return pc_settings_string("frame_smoothing", PC_SMOOTH_SETTING_DEFAULT);
}

/* "off" | "match" | <pictures a second> (ORRE_SMOOTH's words, the Settings row's too) into s_mode, s_match, s_cap */
static void read_live(const char* live) {
    s_match = 0;
    s_cap = 0;
    if (live == NULL || *live == '\0' || strcmp(live, "off") == 0 || strcmp(live, "0") == 0) {
        s_mode = AURORA_SMOOTH_OFF;
        return;
    }
    s_mode = AURORA_SMOOTH_LIVE;
    if (strcmp(live, "match") == 0) {
        s_match = 1;
    } else {
        const int hz = atoi(live);
        s_cap = hz < 30 ? 30u : hz > 1000 ? 1000u : (unsigned)hz;
    }
}

static void parse(void) {
    const char* forced = getenv("ORRE_TEST_SMOOTH");
    const char* live = getenv("ORRE_SMOOTH");
    const char* dumps = getenv("ORRE_SMOOTH_DUMP");
    s_parsed = 1;
    /* frames, comma-separated; *since 2026-11-17 (round 10, the seam sweep over whole bike rides)* also ranges
     * "<from>-<to>", so a sweep's list stays short (a Windows environment value is capped at 32,767 characters) */
    while (dumps != NULL && *dumps != '\0' && s_dump_count < MAX_DUMPS) {
        char* end;
        unsigned long v = strtoul(dumps, &end, 10);
        unsigned long to;
        if (end == dumps) {
            break;
        }
        to = v;
        if (*end == '-') {
            char* end2;
            to = strtoul(end + 1, &end2, 10);
            end = end2;
        }
        for (; v <= to && s_dump_count < MAX_DUMPS; v++) {
            s_dumps[s_dump_count++] = (u32)v;
        }
        dumps = *end == ',' ? end + 1 : end;
    }
    if (forced != NULL && *forced != '\0') {
        const int n = atoi(forced);
        s_mode = AURORA_SMOOTH_FORCED;
        s_forced = n < 0 ? 0u : n > 7 ? 7u : (unsigned)n;
        return;
    }
    if (live != NULL && *live != '\0') {
        read_live(live); /* the test switch wins (ORRE_SMOOTH=off too) */
        return;
    }
    s_from_setting = 1;
    read_live(setting_choice());
}

static double display_rate(void) {
    SDL_Window* window = aurora_get_window();
    const SDL_DisplayMode* mode;
    if (window == NULL) {
        return 0.0;
    }
    mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    return mode != NULL ? (double)mode->refresh_rate : 0.0;
}

/* the live presenter's rate: the screen's (match) or the cap, never above the screen's with V-Sync's strict Fifo.
 * *Since 2026-11-18:* a cap chosen in Settings is never above the screen's either, V-Sync or not (snatcherLeo, 2026-11-16:
 * a cap above the monitor's rate behaves like Match, and the row says so): pictures the screen can't show only cost
 * the graphics card, and without V-Sync they would tear. ORRE_SMOOTH=<cap> keeps the old rule, for tests */
static unsigned live_target(void) {
    unsigned target = s_match ? (s_display > 0.0 ? (unsigned)(s_display + 0.5) : 60u) : s_cap;
    if (s_display > 0.0 && (s_from_setting || strncmp(aurora_smooth_present_mode(), "Fifo", 4) == 0) &&
        target > (unsigned)(s_display + 0.5)) {
        target = (unsigned)(s_display + 0.5);
    }
    return target;
}

/* Step (c), OrreHook_CameraSet (src/hsd/cobj.c, HSD_CObjSetCurrent): the camera the next draws are built with goes
 * into the GX stream as a marker, so Aurora sees it in order with those draws (its FIFO thread decodes the stream
 * later, so a direct call into Aurora from here would land at the wrong place). Game thread; reads only, writes no
 * game state, draws no random numbers: the GX stream is Aurora's, and the marker changes no GX state (Aurora reads it
 * and drops it). Set only while smoothing is on, so with smoothing off the GX stream is byte for byte what it was. */
static void camera_set(const void* camera, s32 renderPass, const f32* view, const f32* interest) {
    /* ORRE_SMOOTH_CAMLOG=<from>-<to> (a test switch, observe only; 2026-11-17): every camera set in those game frames,
     * with HSD's render pass and its projection type, to see which cameras draw the shadows and the menu models */
    static int log_on = -1;
    static unsigned log_from, log_to;
    if (log_on < 0) {
        const char* v = getenv("ORRE_SMOOTH_CAMLOG");
        log_on = v != NULL && sscanf(v, "%u-%u", &log_from, &log_to) == 2;
    }
    if (log_on && pc_vi_heartbeat >= log_from && pc_vi_heartbeat <= log_to) {
        fprintf(stderr, "[smooth] camlog frame %u camera %p pass %d projection %d interest %d\n",
                (unsigned)pc_vi_heartbeat, camera, (int)renderPass,
                HSD_CObjGetProjectionType != NULL ? HSD_CObjGetProjectionType((void*)camera) : -1,
                interest != NULL);
    }
    GXOrreSmoothCamera((u64)(uintptr_t)camera, renderPass, view, interest);
}

/* Step (d), OrreHook_ModelDraw (src/hsd/pobj.c, HSD_PObjDisp): the model piece the next matrix loads and draws belong
 * to, as a marker in the GX stream for the same reason as the camera's (2026-11-17). Aurora tags the uniform blocks
 * built in between with the pair and blends their matrices with the same piece's in the step before. Reads two
 * pointers, writes no game state, draws no random numbers; set only while smoothing is on.
 * *Extended 2026-11-17 (round 4, the plan's section 16):* the marker also carries the piece's model (its root JObj) and
 * the matrix slots it uses, so Aurora tests and blends only the slots the piece draws with and can snap a whole model
 * at once. Both are read by walking the game's own structs (pc_smooth_model_info, game side); still no writes. */
static void model_draw(const void* jobj, const void* pobj) {
    u64 root = 0;
    u32 slots = 0;
    if (pobj != NULL) {
        /* without the game side (a test program) every slot counts as used: the round-3 behaviour */
        slots = pc_smooth_model_info != NULL ? pc_smooth_model_info(jobj, pobj, &root) : 0x3FFu;
    }
    GXOrreSmoothModel((u64)(uintptr_t)jobj, (u64)(uintptr_t)pobj, root, slots);
}

/* Item (a), OrreHook_ParticleDraw (src/game/psdisp.c, psDispMain): the particle the next draws belong to, as a marker
 * in the GX stream for the same reason as the camera's and the model's (2026-11-17; the plan's section 18). A particle's
 * corners are worked out on the CPU each step (they face the camera), so there is no matrix of its own to blend: Aurora
 * takes the middle of the particle's drawn corners in each step and, in an in-between, moves its draws part of the way
 * back to where it was drawn in the step before. The guard (bank, id number) and the life (it only counts down) tell a
 * reused particle slot from the same particle. Writes no game state, draws no random numbers; set only while smoothing
 * is on. Platform-neutral. */
static void particle_draw(const void* particle, u32 guard, u32 life) {
    GXOrreSmoothParticle((u64)(uintptr_t)particle, guard, life);
}

/* Round 11, OrreHook_2DPiece (2D at 120; the plan's section 24): a named 2D piece the next draws belong to (kind 0:
 * done), as a marker in the GX stream like the others. Aurora gives the piece's draws their own uniform block, finds
 * the same piece (kind and id) in the step before, and draws the in-between with the piece's value blended (a bar's
 * width; a panel's offset or fade). ORRE_SMOOTH_2D=0 turns it off (the hook stays unset: nothing tagged, nothing
 * recorded in the game's request buffer); on by default for snatcherLeo's trial (round 11: "we can always revert").
 * Writes no game state, draws no random numbers. Platform-neutral. */
static unsigned long long s_2dCalls; /* named pieces the game's playback reached (the 10 s line: a stage check) */

/* Which named pieces blend, and how (the game names more than this blends: the policy lives here, not in the game
 * code). The kind's top 16 bits are the group the game code gave it (src/game/pc_debug_game.c, PC_ORRE_2D_*):
 *   0  a battle gauge's fill (the HP and EXP bars, fn_80011EA4): blended by its value (its width);
 *   1  a menu sprite (menuDaemon's draw loop, src/game/menu_r50_80102014_prefix.c: low 16 bits the menu id), 2 a menu's own draw callback: every menu since round 12
 *      (round 11: only the pause menus 0x41 / 0x109), blended by vertices and matrices;
 *   3  the menu backdrop's darkening (menu_offscreen.c, id 1), the screen fade (effect/fade.c, id 2; round 12) and the
 *      Poké Ball wipe's balls (effect/fade.c, id 3; fix 3, 2026-11-18), by their vertices;
 *   4  a window sprite (windowDrawSprite2, window_exact_80104160.c; round 12): low 16 bits the sprite's id.
 * Any other named piece is sent as "no piece" (kind 0), so it can't run on into the draws after it. A menu that must
 * stay at the step rate would be listed in case 1-2 below. */
static u32 piece_2d_kind(u32 kind) {
    const u32 group = kind >> 16, menu = kind & 0xFFFFu;
    switch (group) {
    case 0:
        return kind;
    case 1:
    case 2: {
        /* *Widened 2026-11-17 (round 12, snatcherLeo: the Items menu, the P★DA, the PC menu and box...):* every menu,
         * except the ones listed here (none yet); ORRE_SMOOTH_2D_MENUS=pause keeps round 11's pause menus only
         * (a before / after switch for tests). What can't blend in a menu (a picture or glyph swapped, a list
         * scrolled back a row) shows the newest in Aurora's own rules (smooth_camera.cpp) */
        static int s_pauseOnly = -1;
        if (s_pauseOnly < 0) {
            const char* v = getenv("ORRE_SMOOTH_2D_MENUS");
            s_pauseOnly = v != NULL && strcmp(v, "pause") == 0;
        }
        if (s_pauseOnly) {
            return menu == 0x41 || menu == 0x109 ? kind | GX_AURORA_ORRE_2D_VERTICES : 0;
        }
        return kind | GX_AURORA_ORRE_2D_VERTICES;
    }
    case 3:
        return kind | GX_AURORA_ORRE_2D_VERTICES;
    case 4: /* round 12: a window sprite drawn outside the menu daemon's list (windowDrawSprite2: the PC screen, the
             * P*DA...), by vertices; ORRE_SMOOTH_2D_MENUS=pause leaves it out as round 11 did */
        return getenv("ORRE_SMOOTH_2D_MENUS") != NULL && strcmp(getenv("ORRE_SMOOTH_2D_MENUS"), "pause") == 0
                   ? 0
                   : kind | GX_AURORA_ORRE_2D_VERTICES;
    default:
        return 0;
    }
}

/* Round 12 (2026-11-17): named pieces nest (a gauge inside a window sprite inside a menu sprite's callback), and Aurora
 * knows one piece at a time. So the open pieces are kept as a stack: a new piece takes over the draws, and when it ends
 * the one around it is named again (Aurora counts that as its next occurrence, the same every step). A plain window
 * sprite (group 4) inside an already named piece doesn't take over: the outer name is the more specific one (a gauge's
 * value, a menu sprite's identity). Cleared at every step (pc_smooth_step), so an unbalanced pair can't leak. */
typedef struct Piece2D {
    u32 kind, variant, id, valueBits;
    int transparent; /* a group-4 sprite inside a named piece: left to the outer piece */
} Piece2D;
static Piece2D s_2dStack[16];
static int s_2dDepth;

static void piece_2d(u32 kind, u32 variant, u32 id, u32 valueBits) {
    if (kind != 0) {
        Piece2D p;
        ++s_2dCalls;
        p.kind = piece_2d_kind(kind);
        p.variant = variant, p.id = id, p.valueBits = valueBits;
        p.transparent = (kind >> 16) == 4 && s_2dDepth > 0 && s_2dDepth <= 16 && s_2dStack[s_2dDepth - 1].kind != 0;
        if (s_2dDepth < 16) {
            s_2dStack[s_2dDepth] = p;
        }
        ++s_2dDepth;
        if (!p.transparent) {
            GXOrreSmooth2D(p.kind, variant, id, valueBits);
        }
        return;
    }
    if (s_2dDepth > 0) {
        const int was = s_2dDepth - 1;
        const int transparent = was < 16 && s_2dStack[was].transparent;
        s_2dDepth = was;
        if (transparent) {
            return; /* the outer piece never stopped */
        }
        if (s_2dDepth > 0 && s_2dDepth <= 16) {
            const Piece2D* outer = &s_2dStack[s_2dDepth - 1];
            GXOrreSmooth2D(outer->kind, outer->variant, outer->id, outer->valueBits);
            return;
        }
    }
    GXOrreSmooth2D(0, 0, 0, 0);
}

static int smooth_2d_wanted(void) {
    const char* v = getenv("ORRE_SMOOTH_2D");
    return v == NULL || *v == '\0' || strcmp(v, "0") != 0;
}

/* step (c): history resets at the next step (the camera snaps there), with the reason logged */
static void request_reset(const char* why) {
    if (!s_on) {
        return;
    }
    s_reset_pending = 1;
    smooth_event("reset at game frame %u: %s (the camera snaps at the next step)", (unsigned)pc_vi_heartbeat, why);
}

void pc_smooth_room_entered(unsigned room) {
    char why[48];
    snprintf(why, sizeof(why), "room change to 0x%X", room);
    request_reset(why);
}

static void start(void) {
    static int starts;
    s_on = 1;
    if (starts++ > 0) {
        /* turned on again from Settings (2026-11-18): the first step snaps, with no history from before it was off */
        s_reset_pending = 1;
    }
    if (s_held) {
        aurora_smooth_hold(true); /* turned on during a pause (the Settings overlay's): held until it ends */
    }
    OrreHook_CameraSet = camera_set; /* step (c): the camera blend's markers */
    OrreHook_ModelDraw = model_draw; /* step (d): the model blend's markers */
    OrreHook_ParticleDraw = particle_draw; /* item (a): the particle blend's markers (2026-11-17) */
    if (smooth_2d_wanted()) {
        OrreHook_2DPiece = piece_2d; /* round 11: 2D at 120's markers (ORRE_SMOOTH_2D=0: off) */
        smooth_event("2D at 120 on (the HP / EXP bars, every menu, window sprites, the menu backdrop and the screen "
                     "fade); ORRE_SMOOTH_2D=0 turns it off");
    } else {
        smooth_event("2D at 120 off (ORRE_SMOOTH_2D=0): 2D stays at the step rate");
    }
    s_display = display_rate();
    s_fields = pc_vi_fields_per_second();
    if (s_fields > 0.0) {
        aurora_smooth_field_ns((uint64_t)(1e9 / s_fields), 0); /* round 5: the retrace length the steps run at */
    }
    s_summary_at = SDL_GetTicksNS() + SUMMARY_NS;
    s_recheck_at = SDL_GetTicksNS() + RECHECK_NS;
    aurora_smooth_get_stats(&s_prev);
    {
        const char* wipe = getenv("ORRE_SMOOTH_TEST_WIPE");
        const int mode = wipe != NULL ? atoi(wipe) : 0;
        if (mode == 1 || mode == 2) {
            aurora_smooth_test_wipe(mode);
            smooth_event("ORRE_SMOOTH_TEST_WIPE=%d: the frame buffer is painted magenta before each in-between's "
                         "replay%s", mode, mode == 2 ? ", and the replay is skipped (the dumps must not match)" : "");
        }
    }
    if (s_mode == AURORA_SMOOTH_FORCED) {
        aurora_smooth_configure(AURORA_SMOOTH_FORCED, 0, s_forced);
        smooth_event("on, test mode (ORRE_TEST_SMOOTH=%u): every step is followed at once by %u in-between picture%s "
                     "and the game waits for them (about %u pictures a second in 60-a-second scenes), evenly between "
                     "the previous step and this one (camera and HSD models blended, steps c and d); screen "
                     "%.2f Hz, present mode %s",
                     s_forced, s_forced, s_forced == 1 ? "" : "s", 60u * (s_forced + 1u), s_display,
                     aurora_smooth_present_mode());
        return;
    }
    s_target = live_target();
    aurora_smooth_configure(AURORA_SMOOTH_LIVE, s_target, 0);
    smooth_event("on (%s=%s): %u pictures a second (%s; screen %.2f Hz), present mode %s; one step behind, "
                 "the camera and HSD models blended between steps (steps c and d; particles, 2D, text and fades as "
                 "the step drew them)",
                 s_from_setting ? "Settings frame_smoothing" : "ORRE_SMOOTH", s_match ? "match" : "cap", s_target,
                 s_match ? "Match your display" : "the cap", s_display, aurora_smooth_present_mode());
}

/* turned off from Settings while the game runs (2026-11-18): Aurora back to one picture per step, the game-code hooks
 * unset again, so the GX stream is byte for byte what it is with smoothing never on (the hooks' rule above) */
static void stop(void) {
    s_on = 0;
    OrreHook_CameraSet = NULL;
    OrreHook_ModelDraw = NULL;
    OrreHook_ParticleDraw = NULL;
    OrreHook_2DPiece = NULL;
    s_2dDepth = 0;
    if (s_held) {
        aurora_smooth_hold(false); /* s_held stays: a pause still running is held again if smoothing comes back */
    }
    if (s_outpaced > 0) {
        aurora_smooth_set_outpaced(false);
    }
    s_outpaced = -1;
    s_reset_pending = 0;
    aurora_smooth_configure(AURORA_SMOOTH_OFF, 0, 0);
    smooth_event("off at game frame %u (Settings frame_smoothing=off): one picture per step, as without smoothing",
                 (unsigned)pc_vi_heartbeat);
}

/* Settings › Graphics › Frame rate changed (pc_settings_overlay.cpp; settings.ini already holds the new choice). Read
 * at the next step, on the game thread, where smoothing starts and changes. Platform-neutral. */
void pc_smooth_setting_changed(void) { s_setting_changed = 1; }

/* the row's new choice, at a step: nothing when a test switch decides; otherwise on, off, or a new rate */
static void apply_setting_change(void) {
    const int was_on = s_on;
    s_setting_changed = 0;
    if (!s_from_setting) {
        smooth_event("Settings frame_smoothing changed to \"%s\": not applied, a test switch decides (ORRE_SMOOTH / "
                     "ORRE_TEST_SMOOTH)", pc_settings_string("frame_smoothing", PC_SMOOTH_SETTING_DEFAULT));
        return;
    }
    read_live(setting_choice());
    if (s_mode == AURORA_SMOOTH_OFF) {
        if (was_on) {
            stop();
        }
        return;
    }
    if (!was_on) {
        return; /* pc_smooth_step starts it right after this, with the new choice */
    }
    {
        const unsigned target = live_target();
        smooth_event("Settings frame_smoothing=%s at game frame %u: %u pictures a second (was %u; screen %.2f Hz, "
                     "present mode %s)", s_match ? "match" : "cap", (unsigned)pc_vi_heartbeat, target, s_target,
                     s_display, aurora_smooth_present_mode());
        s_target = target;
        aurora_smooth_configure(AURORA_SMOOTH_LIVE, target, 0);
    }
}

int pc_smooth_on(void) { return s_on; }

void pc_smooth_step(unsigned why, unsigned retrace) {
    int fullDump = 0; /* round 12: a full ORRE_SMOOTH_DUMP requested for this step (the thumbnails then skip it) */
    s_2dDepth = 0;    /* round 12: the named 2D pieces' stack starts empty every step (piece_2d) */
    if (!s_parsed) {
        parse();
    }
    if (s_setting_changed) {
        apply_setting_change(); /* the Settings row, changed since the last step (2026-11-18) */
    }
    if (s_dump_count > 0) {
        /* ORRE_SMOOTH_DUMP: this step's picture, and its in-betweens' (works with smoothing off: the step only) */
        int i;
        for (i = 0; i < s_dump_count; i++) {
            if (s_dumps[i] == pc_vi_heartbeat) {
                char path[64];
                smooth_make_dir("smooth_dumps");
                snprintf(path, sizeof(path), "smooth_dumps/f%u", (unsigned)pc_vi_heartbeat);
                aurora_smooth_request_dump(path);
                s_dumps[i] = 0xFFFFFFFFu; /* once */
                fullDump = 1;
                break;
            }
        }
    }
    if (!fullDump) {
        /* ORRE_SMOOTH_THUMBS=<from>-<to> | all (a test switch; round 12, 2026-11-17, the census of what still runs at
         * the step rate over every the reference playthrough sitting): every step in the range and its in-betweens as small pictures in one
         * file, smooth_thumbs.bin (Aurora's dump writer, aurora.cpp: 160 x 120 grey, every 32nd frame's step also
         * 320 x 240 colour). Look only. A frame with a full ORRE_SMOOTH_DUMP above keeps that one. All OSes. */
        static int s_thumbsParsed;
        static unsigned long s_thumbFrom, s_thumbTo;
        if (!s_thumbsParsed) {
            const char* v = getenv("ORRE_SMOOTH_THUMBS");
            s_thumbsParsed = 1;
            if (v != NULL && strcmp(v, "all") == 0) {
                s_thumbFrom = 0;
                s_thumbTo = 0xFFFFFFFFul;
            } else if (v != NULL && *v != '\0') {
                char* end;
                s_thumbFrom = strtoul(v, &end, 10);
                s_thumbTo = *end == '-' ? strtoul(end + 1, NULL, 10) : s_thumbFrom;
            }
        }
        if (s_thumbTo != 0 && pc_vi_heartbeat >= s_thumbFrom && pc_vi_heartbeat <= s_thumbTo) {
            char path[32];
            snprintf(path, sizeof(path), "thumb:%u", (unsigned)pc_vi_heartbeat);
            aurora_smooth_request_dump(path);
        }
    }
    if (s_mode == AURORA_SMOOTH_OFF) {
        return;
    }
    if (!s_on) {
        start();
    }
    if (why == PC_SMOOTH_STEP_COPY) {
        /* the step's length in retraces, for the step rate (30-a-second scenes take 2) */
        const u32 len = (u32)retrace - s_last_retrace;
        if (s_last_retrace != 0 && len >= 1 && len <= 8) {
            s_step_len = (unsigned)len;
        }
        s_last_retrace = (u32)retrace;
    }
    {
        /* step (c): the anchor (when this step is due on screen: the next retrace, the moment the console would show
         * it) and a pending reset. Reading the due time changes nothing: it is set at the first retrace. */
        uint32_t flags = (uint32_t)why;
        if (s_reset_pending) {
            flags |= AURORA_SMOOTH_STEP_RESET;
            s_reset_pending = 0;
        }
        aurora_smooth_step(retrace, flags, (uint64_t)pc_vi_retrace_due_ns());
    }
}

void pc_smooth_after_end_frame(void) {
    if (s_on && s_mode == AURORA_SMOOTH_FORCED) {
        aurora_smooth_wait(); /* the test mode: the in-betweens are done before the game goes on */
    }
}

void pc_smooth_hold(int on, const char* why) {
    /* *Changed 2026-11-18 (the Settings row):* the pause is remembered while smoothing is off too, since the row can
     * turn smoothing on or off in the middle of the Settings overlay's own pause (start and stop above apply it) */
    if (on == s_held) {
        return;
    }
    s_held = on;
    if (!s_on) {
        return;
    }
    aurora_smooth_hold(on != 0);
    if (on) {
        smooth_event("held at game frame %u (%s): each step shows at once, no in-between pictures",
                     (unsigned)pc_vi_heartbeat, why);
    } else {
        smooth_event("resumed at game frame %u after %s: the picture grid restarts", (unsigned)pc_vi_heartbeat, why);
    }
}

void pc_smooth_check_rng_draw(void) {
    if (!s_on) {
        return;
    }
    ++s_rng_checked;
    if (aurora_smooth_in_between_now()) {
        fprintf(stderr, "[smooth] TRIPWIRE: the game drew a random number during an in-between picture (game frame "
                        "%u): in-between pictures must run no game code. Stopping.\n",
                (unsigned)pc_vi_heartbeat);
        fflush(stderr);
        pc_log_drain();
        abort();
    }
}

/* the rate checks (once a second) and the 10 s summary */
static void summary(double seconds) {
    static const char* const why[AURORA_SMOOTH_HELD_COUNT] = {"kept EFB", "no copy", "movie", "pause",
                                                             "UI",       "resized", "stale", "nothing kept"};
    AuroraSmoothStats now;
    unsigned long long held = 0, d[AURORA_SMOOTH_HELD_COUNT];
    unsigned long long steps, shown, ib, pictures, replays, presents;
    char reasons[256];
    int len = 0, i;
    aurora_smooth_get_stats(&now);
    reasons[0] = '\0';
    for (i = 0; i < AURORA_SMOOTH_HELD_COUNT; i++) {
        d[i] = now.held[i] - s_prev.held[i];
        held += d[i];
        if (d[i] > 0 && len < (int)sizeof(reasons) - 32) {
            len += snprintf(reasons + len, sizeof(reasons) - (size_t)len, "%s%s %llu", len ? ", " : "", why[i], d[i]);
        }
    }
    steps = now.steps - s_prev.steps;
    shown = now.stepPictures - s_prev.stepPictures;
    ib = now.inBetweens - s_prev.inBetweens;
    /* step (c): a later picture of a step where nothing moved is shown without a redraw ("unchanged"); it is a picture
     * too (*corrected 2026-11-16*: the first step (c) build left these out of the count, so 120 a second read as 60) */
    pictures = shown + ib + held + (now.repeats - s_prev.repeats);
    replays = now.replays - s_prev.replays;
    presents = now.presents - s_prev.presents;
    fprintf(stderr,
            "[smooth] %.1f s to game frame %u: %llu steps, %llu pictures (%.1f a second): %llu steps shown, %llu "
            "in-between, %llu unchanged, %llu held%s%s%s; steps not shown %llu, not presented %llu, slips %llu, worst "
            "gap %.1f ms; render worker: %.2f ms per in-between redraw (%.0f draws each), %.2f ms per picture; "
            "tripwire checked %llu random draws, silent%s\n",
            seconds, (unsigned)pc_vi_heartbeat, steps, pictures, seconds > 0.0 ? (double)pictures / seconds : 0.0,
            shown, ib, now.repeats - s_prev.repeats, held, held > 0 ? " (" : "", reasons, held > 0 ? ")" : "",
            now.stepsNotShown - s_prev.stepsNotShown, now.notPresented - s_prev.notPresented, now.slips - s_prev.slips,
            (double)now.worstGapNs / 1e6, replays > 0 ? (double)(now.replayNs - s_prev.replayNs) / 1e6 / (double)replays : 0.0,
            replays > 0 ? (double)(now.replayDraws - s_prev.replayDraws) / (double)replays : 0.0,
            presents > 0 ? (double)(now.presentNs - s_prev.presentNs) / 1e6 / (double)presents : 0.0, s_rng_checked,
            s_outpaced > 0 ? "; smoothing paused (steps outpace pictures)" : "");
    {
        /* step (c): the camera blend, and where the render worker's time went (the pacing work) */
        unsigned long long c[AURORA_SMOOTH_CUT_COUNT];
        for (i = 0; i < AURORA_SMOOTH_CUT_COUNT; i++) {
            c[i] = now.cuts[i] - s_prev.cuts[i];
        }
        fprintf(stderr,
                "[smooth] camera: %llu pictures blended, %llu repeated unchanged, %llu restores; cameras %llu: blended "
                "%llu (of which around the eye, no target %llu), unchanged %llu, reset %llu, unmatched %llu, not rigid "
                "%llu, cut (turn %llu, eye "
                "%llu, target %llu, projection %llu); blocks re-aimed %llu (left as drawn: %llu another projection, %llu "
                "overflow), "
                "%.1f MB written\n",
                now.blended - s_prev.blended, now.repeats - s_prev.repeats, now.restores - s_prev.restores,
                now.cameras - s_prev.cameras, c[AURORA_SMOOTH_CUT_NONE], c[AURORA_SMOOTH_CUT_NO_TARGET],
                c[AURORA_SMOOTH_CUT_UNCHANGED], c[AURORA_SMOOTH_CUT_RESET], c[AURORA_SMOOTH_CUT_UNMATCHED],
                c[AURORA_SMOOTH_CUT_NOT_RIGID], c[AURORA_SMOOTH_CUT_ROTATION], c[AURORA_SMOOTH_CUT_EYE],
                c[AURORA_SMOOTH_CUT_TARGET], c[AURORA_SMOOTH_CUT_FOV], now.reaimedBlocks - s_prev.reaimedBlocks,
                now.looseBlocks - s_prev.looseBlocks, now.overflowBlocks - s_prev.overflowBlocks,
                (double)(now.blendBytes - s_prev.blendBytes) / 1e6);
        /* step (d), the model blend (2026-11-17), and the coverage measure snatcherLeo's first hand check asked for: of the
         * in-between pictures (not a step's own picture), how many moved something (blended: camera or models), how
         * many with a model piece blended, how many were drawn again unchanged (nothing moved: test mode redraws them
         * anyway), repeated without a redraw (live), or held (by the reasons above) */
        {
            const unsigned long long ibAll = ib + (now.repeats - s_prev.repeats) + held;
            const unsigned long long ibBlend = now.ibBlended - s_prev.ibBlended;
            fprintf(stderr,
                    "[smooth] models: blocks under a screen camera's own projection %llu (HSD model pieces %llu, other "
                    "draws %llu); pieces found in the step before %llu (%llu of them borrowed from their joint): "
                    "blended %llu, in a model snapped whole %llu (%llu models), held for a camera cut %llu; model "
                    "blocks written %llu; left-as-drawn blocks: same frustum as the camera %llu, of a model piece "
                    "%llu; capture copies %llu; texture matrices following their piece %llu\n",
                    (now.modelTagged - s_prev.modelTagged) + (now.modelUntagged - s_prev.modelUntagged),
                    now.modelTagged - s_prev.modelTagged, now.modelUntagged - s_prev.modelUntagged,
                    now.modelMatched - s_prev.modelMatched, now.modelBorrowed - s_prev.modelBorrowed,
                    now.modelMoved - s_prev.modelMoved, now.modelSnapped - s_prev.modelSnapped,
                    now.modelsSnapped - s_prev.modelsSnapped, now.modelHeld - s_prev.modelHeld,
                    now.modelBlockWrites - s_prev.modelBlockWrites, now.looseSameFrustum - s_prev.looseSameFrustum,
                    now.looseTagged - s_prev.looseTagged, now.captureCopies - s_prev.captureCopies,
                    now.texFollows - s_prev.texFollows);
            /* item (a), the particle blend (2026-11-17): particles under a screen camera, per step */
            fprintf(stderr,
                    "[smooth] particles: %llu drawn under a screen camera (per step): blended %llu, new %llu, a reused "
                    "slot %llu, moved too far %llu, held for a camera cut %llu\n",
                    now.particleTagged - s_prev.particleTagged, now.particleBlended - s_prev.particleBlended,
                    now.particleNew - s_prev.particleNew, now.particleReused - s_prev.particleReused,
                    now.particleJumped - s_prev.particleJumped, now.particleHeld - s_prev.particleHeld);
            /* item (b), scrolling textures (2026-11-17): model pieces' texture matrices, per step */
            fprintf(stderr,
                    "[smooth] scrolling textures: %llu texture matrices blended (%llu across a wrap), %llu changed too "
                    "much in one step, %llu not steady (an atlas cell change; round 9) (both: the newest shown)\n",
                    now.scrollBlended - s_prev.scrollBlended, now.scrollWrapped - s_prev.scrollWrapped,
                    now.scrollSnapped - s_prev.scrollSnapped, now.scrollUnsteady - s_prev.scrollUnsteady);
            /* round 10 (2026-11-17): models moving with their camera, kept in place on screen (per step) */
            fprintf(stderr, "[smooth] locked to the camera: %llu model blocks (moving with their camera: drawn as "
                            "the step drew them)\n",
                    now.lockedBlocks - s_prev.lockedBlocks);
            /* round 11 (2026-11-17): 2D at 120, the named 2D pieces (per step) */
            fprintf(stderr, "[smooth] 2D pieces: %llu named (per step): blended %llu, new %llu, picture changed %llu; "
                            "blocks drawn blended %llu; %llu with no readable x (held); the game named %llu draws so far\n",
                    now.p2Tagged - s_prev.p2Tagged, now.p2Blended - s_prev.p2Blended, now.p2New - s_prev.p2New,
                    now.p2Swapped - s_prev.p2Swapped, now.p2Written - s_prev.p2Written,
                    now.p2NoSpan - s_prev.p2NoSpan, s_2dCalls);
            /* round 11, vertex mode (the pause menu: its buttons, rail and darkening) */
            fprintf(stderr, "[smooth] 2D menu pieces (per step): %llu draws blended, %llu shown newest (another "
                            "picture or layout), %llu matrices blended; pieces drawn blended %llu\n",
                    now.p2VtxDraws - s_prev.p2VtxDraws, now.p2VtxSnapped - s_prev.p2VtxSnapped,
                    now.p2MtxBlocks - s_prev.p2MtxBlocks, now.p2VtxWritten - s_prev.p2VtxWritten);
            /* item (c), menu models (2026-11-17): the two-step history, per step */
            fprintf(stderr,
                    "[smooth] menu models: %llu models (%llu blocks) blended over two steps (posed on every other "
                    "step)\n",
                    now.twoStepModels - s_prev.twoStepModels, now.twoStepBlocks - s_prev.twoStepBlocks);
            /* the colour-data blend (2026-11-18): model pieces whose colours changed (a flash, a fade, a light) */
            fprintf(stderr, "[smooth] model colours (per step): %llu blocks blended, %llu changed but shown newest\n",
                    now.colorBlended - s_prev.colorBlended, now.colorHeld - s_prev.colorHeld);
            /* (3b) (2026-11-18): particles drawn with their corners blended (a sparkle growing or turning in place) */
            fprintf(stderr, "[smooth] particle corners (per step): %llu particles blended by their corners\n",
                    now.cornerParticles - s_prev.cornerParticles);
            fprintf(stderr,
                    "[smooth] coverage: in-between pictures %llu: blended %llu (%.0f %%; with models %llu), drawn again "
                    "unchanged %llu, repeated %llu, held %llu\n",
                    ibAll, ibBlend, ibAll > 0 ? 100.0 * (double)ibBlend / (double)ibAll : 0.0,
                    now.ibModels - s_prev.ibModels, ib >= ibBlend ? ib - ibBlend : 0ull,
                    now.repeats - s_prev.repeats, held);
        }
        fprintf(stderr,
                "[smooth] pacing: slots over 2 ms late %llu (%llu with a queue item running across the slot), latest "
                "%.2f ms; longest picture %.2f ms, acquire %.2f ms, submit %.2f ms, present %.2f ms; worker items "
                "(longest / average ms, count): begin frame %.2f / %.3f, %llu; encode pass %.2f / %.3f, %llu; end "
                "frame %.2f / %.3f, %llu; sync %.2f / %.3f, %llu\n",
                now.lateStarts, now.lateByItem, (double)now.worstLateNs / 1e6, (double)now.worstPictureNs / 1e6,
                (double)now.worstAcquireNs / 1e6, (double)now.worstSubmitNs / 1e6, (double)now.worstPresentNs / 1e6,
                (double)now.itemMaxNs[0] / 1e6,
                now.itemCount[0] ? (double)now.itemTotalNs[0] / 1e6 / (double)now.itemCount[0] : 0.0,
                now.itemCount[0], (double)now.itemMaxNs[1] / 1e6,
                now.itemCount[1] ? (double)now.itemTotalNs[1] / 1e6 / (double)now.itemCount[1] : 0.0,
                now.itemCount[1], (double)now.itemMaxNs[2] / 1e6,
                now.itemCount[2] ? (double)now.itemTotalNs[2] / 1e6 / (double)now.itemCount[2] : 0.0,
                now.itemCount[2], (double)now.itemMaxNs[3] / 1e6,
                now.itemCount[3] ? (double)now.itemTotalNs[3] / 1e6 / (double)now.itemCount[3] : 0.0,
                now.itemCount[3]);
        /* the step-to-screen delay (2026-11-17, live mode): from a step's arrival to the moment it is shown whole,
         * now and with step (c)'s retrace anchor; the anchor lead that took the difference off; steps that arrived
         * after their moved anchor (each a small jump forward at its first picture) */
        if (now.delayCount > 0) {
            fprintf(stderr,
                    "[smooth] delay: a step is shown whole %.2f ms after it is drawn (largest %.2f ms; with the "
                    "retrace anchor of step c: %.2f ms); anchor %.2f ms before that retrace; steps after their "
                    "anchor %llu of %llu, anchors started again %llu; pictures at the older step %llu, in between "
                    "%llu, at the newest %llu; pairs re-timed for a speed change %llu\n",
                    (double)now.delaySumNs / 1e6 / (double)now.delayCount, (double)now.delayMaxNs / 1e6,
                    (double)now.delayRetSumNs / 1e6 / (double)now.delayCount, (double)now.leadNs / 1e6,
                    now.lateArrivals, now.delayCount, now.reanchors, now.fracZero, now.fracMid, now.fracOne,
                    now.retimes);
        }
    }
    s_rng_checked = 0;
    s_prev = now;
}

void pc_smooth_frame(void) {
    Uint64 now;
    if (!s_on) {
        return;
    }
    now = SDL_GetTicksNS();
    /* *Changed 2026-11-17 (snatcherLeo's second hand check: "briefly gets the dizzying thing again" after letting go of
     * fast-forward):* the game speed is read every frame, not once a second. pc_speed_frame sets it just before this
     * call (pc_vi.c's per-frame bookkeeping), so the change is seen on the very frame it happens: the history resets
     * there (the next step snaps, then blends from fresh history) and the paused / resumed decision below is made at
     * once. The once-a-second check left up to a second (about 30 game frames at 1x, 300 at 5x) in which the presenter
     * still thought steps outpaced pictures after a release (each step shown at once, unblended, against the picture
     * grid), or blended 300 steps a second after a press. Reading the speed is one division; the screen's rate stays on
     * the once-a-second check (SDL's display query is slower). Platform-neutral. */
    int speed_changed = 0;
    {
        const double fields = pc_vi_fields_per_second();
        if (fields < s_fields - 0.01 || fields > s_fields + 0.01) {
            /* *Changed 2026-11-17 (round 4; the plan's section 16):* a speed change no longer resets smoothing's
             * history. The reset made the step after the change snap, and Aurora's history rule then refused the step
             * after that too (fixed there), so letting go of fast-forward showed two steps whole at the step rate,
             * about 67 ms of 30-a-second motion with no in-betweens, after a picture held at the last fast step
             * (measured, P01 live at Match 120, ORRE_TEST_FF released in the field and in a battle: pictures 3-10
             * after the release all at fraction 1). That was snatcherLeo's "brief dizzy moment". Nothing about the steps
             * themselves changes with the speed (a step is the same game time at any speed); only their spacing on
             * screen does, and the step clock already follows that (each step's length is its retrace gap at the
             * speed it ran, and an anchor that drifts more than 4 ms from the arrivals restarts there).
             * ORRE_SMOOTH_SPEED_RESET=1 (a test switch) keeps the old reset, for before / after with one exe.
             * Platform-neutral. */
            static int keep_reset = -1;
            char why[112];
            if (keep_reset < 0) {
                const char* v = getenv("ORRE_SMOOTH_SPEED_RESET");
                keep_reset = v != NULL && v[0] == '1';
            }
            snprintf(why, sizeof(why), "game speed changed (%.2f -> %.2f retraces a second)", s_fields, fields);
            if (keep_reset) {
                request_reset(why);
            } else {
                smooth_event("%s at game frame %u: the step clock follows, no reset", why, (unsigned)pc_vi_heartbeat);
            }
            s_fields = fields;
            speed_changed = 1;
            /* round 5 (2026-11-17; the plan's section 17): the new retrace length goes to Aurora at once, which re-times
             * the pair on screen to end as the next step arrives (no picture held when fast-forward is let go) and
             * stamps the next steps with it (their pair lengths from the known speed, not measured across the change).
             * When the next step will be there: its last retrace is the last step's plus the step's length in
             * retraces; this runs right after a retrace, the next one is due at pc_vi_retrace_due_ns() (scheduled at
             * the old speed), and every later one a new retrace length after it. A late step (its last retrace
             * already past) is due now. */
            if (fields > 0.0) {
                const u64 field = (u64)(1e9 / fields);
                const u32 now_retrace = (u32)pc_vi_retrace_clock();
                const u32 last = s_last_retrace != 0 ? s_last_retrace + s_step_len : 0;
                u64 due = 0;
                if (last != 0) {
                    due = last <= now_retrace ? SDL_GetTicksNS()
                                              : pc_vi_retrace_due_ns() + (u64)(last - now_retrace - 1) * field;
                }
                aurora_smooth_field_ns(field, due);
            }
        }
    }
    if (now >= s_recheck_at || speed_changed) {
        const double fields = s_fields;
        if (now >= s_recheck_at) {
            const double display = display_rate();
            s_recheck_at = now + RECHECK_NS;
            if (display != s_display) {
                smooth_event("the screen's rate changed: %.2f Hz -> %.2f Hz (game frame %u)", s_display, display,
                             (unsigned)pc_vi_heartbeat);
                s_display = display;
            }
        }
        if (s_mode == AURORA_SMOOTH_LIVE) {
            const unsigned target = live_target();
            const double steps = fields / (double)s_step_len;
            const int outpaced = steps > (double)target + 0.5;
            if (target != s_target) {
                smooth_event("%u pictures a second (was %u; screen %.2f Hz, present mode %s)", target, s_target,
                             s_display, aurora_smooth_present_mode());
                s_target = target;
                aurora_smooth_configure(AURORA_SMOOTH_LIVE, target, 0);
            }
            if (outpaced != s_outpaced) {
                if (outpaced) {
                    smooth_event("smoothing paused at game frame %u: %.1f steps a second, %u pictures (the newest "
                                 "step is shown at each picture)", (unsigned)pc_vi_heartbeat, steps, target);
                } else if (s_outpaced > 0) {
                    smooth_event("smoothing resumed at game frame %u: %.1f steps a second, %u pictures",
                                 (unsigned)pc_vi_heartbeat, steps, target);
                }
                s_outpaced = outpaced;
                aurora_smooth_set_outpaced(outpaced != 0);
            }
        }
    }
    {
        /* step (c): a camera cut Aurora found (a jump the blend refused: the picture snapped there), once each */
        static const char* const reason[AURORA_SMOOTH_CUT_COUNT] = {
            "blended", "unchanged", "reset", "unmatched", "not rigid", "no target", "turned", "eye jumped",
            "target jumped", "field of view changed"};
        AuroraSmoothCut cut;
        aurora_smooth_last_cut(&cut);
        if (cut.sequence != s_last_cut_seq) {
            s_last_cut_seq = cut.sequence;
            smooth_event("camera cut near game frame %u (retrace %u): %s (turn %.1f degrees, eye moved %.2f, target "
                         "moved %.2f, eye-to-target %.2f, projection changed %.0f %%): snapped, no blend",
                         (unsigned)pc_vi_heartbeat, cut.retrace,
                         cut.reason >= 0 && cut.reason < AURORA_SMOOTH_CUT_COUNT ? reason[cut.reason] : "?",
                         cut.degrees, cut.eyeJump, cut.targetJump, cut.distance, cut.fovRatio * 100.0);
        }
    }
    if (now >= s_summary_at) {
        summary((double)(now - (s_summary_at - SUMMARY_NS)) / 1e9);
        s_summary_at = now + SUMMARY_NS;
    }
}
