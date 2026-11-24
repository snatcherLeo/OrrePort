/*
 * pc_vi.c: the video-interface functions Aurora declares but does not define (docs/port/sdk-gaps.md): the frame
 * boundary.
 *
 * On the GameCube the VI hardware raises a retrace interrupt every field (59.94 per second for NTSC); the game ends
 * each frame by copying it to a frame buffer (GXCopyDisp), handing that buffer to VI (VISetNextFrameBuffer) and
 * waiting for the retrace (VIWaitForRetrace). It counts retraces in a post-retrace callback (GSgfx's fn_800D3F5C,
 * through HSD's callbacks) to run at 30 or 60 frames per second (fn_800D361C waits for minFrameRetraces).
 *
 * On the PC the frame boundary is the game's display copy (pc_vi_display_copy, below; corrected 2026-10-17, A's
 * finding): Aurora's frame is ended (shown) there and the next begun. VIWaitForRetrace paces:
 *   1. before the first display copy (early boot), or when the game hands VI frames without a copy, it ends Aurora's
 *      frame as before (if the game finished a frame since the last call, VISetNextFrameBuffer); a second wait in the
 *      same frame (30 fps) only paces, it shows no empty frame;
 *   2. poll window events (closing the window ends the program);
 *   3. as on the GameCube, the calling thread sleeps until the retrace count changes. Meanwhile other SDK threads
 *      run (pc_os_thread.c: loaders, GSgapp's idle loop), or the processor idles until the next field;
 *   4. the "retrace interrupt" (pc_vi_retrace_interrupt), due every 1/59.94 s on SDL's clock, runs with interrupts
 *      disabled: the pre-retrace callback, due alarms (pc_os_alarm.c), the post-retrace callback (the GameCube's
 *      order), then wakes the sleepers. The scheduler delivers it when interrupts are enabled or nothing can run.
 *      After a long stall (loading) the clock resyncs instead of racing through missed fields.
 * melee-pc paces at the same place (studied, not copied).
 */
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <dolphin/types.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_time.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*VIRetraceCallback)(u32 retraceCount);
typedef s64 OSTime; /* dolphin/os/OSTime.h */

extern BOOL OSDisableInterrupts(void);
extern BOOL OSRestoreInterrupts(BOOL level);
extern void pc_os_run_alarms(void);
extern void pc_os_thread_sleep_on(const void* key);
extern void pc_os_thread_wakeup(const void* key);
extern void pc_frame_dump_end_of_frame(u32 frame);
extern void pc_pad_script_frame(unsigned frame);
#include "pc_debug.h"
#include "pc_cheats.h"
#include "pc_speed.h"
#include "pc_health.h" /* the performance log's frame times */
#include "pc_smooth.h" /* frame smoothing (test switches only so far): step stamps, pauses, the rate */
#include "pc_paths.h" /* the one game-folder check: live play's clock (4.1) */

void pc_menu_bar_draw(void); /* pc_menu_bar.cpp */

#define FIELD_NS 16683333ull /* NTSC: 1 / 59.94 s */

static VIRetraceCallback s_pre, s_post;
static u32 s_retraces;
static void* s_current_fb;
static void* s_next_fb;
static int s_frame_pending;
static int s_in_frame;
static BOOL s_black;
static u64 s_next_field_ns;
static int s_exit_requested;
static int s_started;      /* pc_vi_begin was called: Aurora draws (tests without a window only pace) */
static char s_retrace_key; /* the address VIWaitForRetrace's sleepers wait on */
static void (*s_port_hook)(void); /* the port's own retrace work (pc_audio.c: start the device after sndInit) */
static void (*s_pad_sampling)(void); /* PADSetSamplingCallback: the game reads its pads there (input.c fn_800F8268) */
static int s_frame_log = -1;      /* ORRE_FRAME_LOG set: a line every 60 game frames (first-run diagnostics) */
static u32 s_presented;           /* frames handed to Aurora */
static int s_copy_ends_frame;     /* the game's display copy (pc_vi_display_copy) ends frames, not the retrace wait */
static int s_frames_without_copy; /* frames handed to VI since the last display copy (the fallback below) */
static int s_copies_this_frame;   /* ORRE_FRAME_TIMES: the game's display copies since the last retrace wait */
volatile int pc_vi_movie_drew;    /* pc_thp_draw.c: a movie frame was drawn since the last retrace wait */
extern volatile u32 pc_vi_heartbeat; /* defined below */

void pc_camera_log_frame(u32 frame) __attribute__((weak)); /* ORRE_CAMERA_LOG (pc_debug_game.c) */
void pc_party_portraits_frame(u32 frame) __attribute__((weak)); /* smooth party screen (pc_debug_game.c) */
void pc_event_log_frame(u32 frame) __attribute__((weak));       /* ORRE_EVENT_LOG (pc_debug_game.c) */
void pc_hero_watch_frame(u32 frame) __attribute__((weak));      /* pad script player anchors (pc_debug_game.c) */
#ifdef ORRE_SETTINGS_OVERLAY
/* the RmlUi trial's Settings window (pc_settings_overlay.cpp; RmlUi builds only, port/CMakeLists.txt). A plain call,
 * not a weak one: a weak reference would not pull the object out of this static library */
void pc_settings_overlay_frame(void);
#endif

/* `why`: how the step ended (PC_SMOOTH_STEP_*: the display copy, the retrace without one, a movie frame, the Settings
 * overlay's pause), for frame smoothing (pc_smooth.c): a step it can't draw again gets its picture repeated */
static void end_frame(unsigned why) {
    pc_frame_dump_end_of_frame(pc_vi_heartbeat); /* ORRE_DUMP_FRAMES (pc_frame_dump.c) */
    if (pc_camera_log_frame) {
        pc_camera_log_frame(pc_vi_heartbeat);
    }
    if (pc_party_portraits_frame) {
        pc_party_portraits_frame(pc_vi_heartbeat);
    }
    pc_menu_bar_draw(); /* the window's menu bar, shown with the mouse at the top edge (pc_menu_bar.cpp) */
    pc_debug_ui_draw(); /* debug toolkit menu, F1 (pc_debug_ui.cpp; ORRE_DEBUG=1 only) */
#ifdef ORRE_SETTINGS_OVERLAY
    pc_settings_overlay_frame(); /* F2 / Back: the trial's Settings window (pc_settings_overlay.cpp) */
#endif
    pc_smooth_step(why, s_retraces); /* frame smoothing: the step's stamp (nothing while it is off) */
    aurora_end_frame();
    pc_smooth_after_end_frame(); /* frame smoothing's test mode waits here for the step's in-between pictures */
    s_in_frame = 0;
    ++s_presented;
}

/* The game's display copy (A, 2026-10-17: HSD_VICopyXFBAsync, fn_801BF8A0, after HSD_VICopyEFB2XFB, calls this; weak
 * on A's side). On the GameCube the copy of the finished picture (GXCopyDisp with clear) happens at the start of the
 * next frame's work, before code that renders into textures immediately during the game logic (menuModelRender for
 * name entry / PDA / TV; the battle distortion fn_8013F410). Ending Aurora's frame at the retrace wait instead showed
 * those immediate renders and their clearing copies on top of the previous picture: name entry's black block,
 * likely Confusion's black screen. So the display copy is the frame boundary; the retrace wait only paces. */
/* Draws wait for their GPU pipelines (2026-11-03; fixes port-gaps 50, 51 and 52, verified on the reference playthrough's P01
 * replay): Aurora builds each new pipeline (a draw's shaders and state) on a worker thread and by default skips every
 * draw whose pipeline isn't built yet (pipeline_cache.cpp find_pipeline_impl, encoding.cpp bind_pipeline). So the first
 * pictures of anything new lost whole models: the ground in Outskirt Stand's bike shot, the battle-start swirl, the
 * stage after a battle camera cut (longer on Direct3D 12, whose shader compiler is slower). The GameCube never skips a
 * draw, so the port waits instead: a short stall the first time a pipeline is needed (P01: about 3 s over 279 builds
 * in a 56,750-frame replay), none once the saved pipeline cache holds it. ORRE_DRAWS_BLOCKING=0 restores Aurora's
 * skipping (for comparisons). Set at every frame start (the state lives in Aurora's GX state; a no-op once set). */
extern void AuroraSetDrawsBlocking(bool blocking);
static void apply_draws_blocking(void) {
    static int on = -1;
    if (on < 0) {
        const char* env = getenv("ORRE_DRAWS_BLOCKING");
        on = !(env != NULL && env[0] == '0' && env[1] == '\0');
        if (!on) {
            fprintf(stderr, "[vi] draws whose pipeline isn't built yet are skipped (ORRE_DRAWS_BLOCKING=0)\n");
        }
    }
    if (on && s_in_frame) {
        AuroraSetDrawsBlocking(true);
    }
}

#ifdef ORRE_SETTINGS_OVERLAY
int pc_settings_overlay_wants_pause(void); /* pc_settings_overlay.cpp: open, with "Pause the game" on */
static void hold_for_overlay(void);
#endif

void pc_vi_display_copy(void) {
    if (!s_started) {
        return;
    }
    s_copy_ends_frame = 1;
    s_frames_without_copy = 0;
    ++s_copies_this_frame; /* ORRE_FRAME_TIMES */
    if (s_in_frame) {
        end_frame(PC_SMOOTH_STEP_COPY);
#ifdef ORRE_SETTINGS_OVERLAY
        if (pc_settings_overlay_wants_pause()) {
            hold_for_overlay(); /* a finished frame: the game waits here while the Settings overlay pauses it */
        }
#endif
    }
    s_in_frame = aurora_begin_frame();
    apply_draws_blocking();
}
void (*pc_vi_frame_hook)(unsigned frame); /* set by the game side (pc_game_main.c); NULL in the tests */
volatile u32 pc_vi_heartbeat;     /* VIWaitForRetrace calls: the game frame count (pad scripts, logs, dumps) */
volatile u32 pc_vi_alive;         /* the pc_crash watchdog's counter: each VIWaitForRetrace, and also while the debug
                                   * toolkit's Pause holds the game (a pause is not a hang; before, the watchdog
                                   * reported one 15 s into a pause, 2026-10-17) */

/* On the GameCube the serial interface samples the pads around each retrace and then calls this callback; the PC
 * calls it once per field from the retrace interrupt, right after the pre-retrace callback (likely the same order). */
void* PADSetSamplingCallback(void (*callback)(void)) {
    void (*old)(void) = s_pad_sampling;
    s_pad_sampling = callback;
    return (void*)old;
}

void pc_vi_set_retrace_hook(void (*hook)(void)) { s_port_hook = hook; }

/* Port startup, after aurora_initialize and before the game draws: begin the first frame */
void pc_vi_begin(void) {
    s_started = 1;
    if (!s_in_frame) {
        s_in_frame = aurora_begin_frame();
        apply_draws_blocking();
    }
}

/* The hub (pc_hub.cpp) runs its own frames between the platform's start and the game: it closes the first frame
 * pc_vi_begin opened (an empty picture) and calls pc_vi_begin again before the game draws. Without this the game's
 * first GX commands found no open frame (an empty render-pass list: a crash in map_logical_viewport, 2026-11-09). */
void pc_vi_close_first_frame(void) {
    if (s_in_frame) {
        aurora_end_frame();
        s_in_frame = 0;
    }
}

int pc_vi_exit_requested(void) { return s_exit_requested; }

void pc_platform_window_event(const AuroraEvent* event); /* pc_platform.c: fullscreen keys, window place */
void pc_platform_window_frame(void);

/* Closing the window ends the game (snatcherLeo, 2026-10-19: the close button did nothing while the game's threads were
 * stuck; verified from the code that the game never read the request at all, only the test programs' loops did).
 * Each memory-card operation is written to its .gci file when the call finishes (Aurora commits per operation), and
 * this runs between game calls, so no file is cut in half; closing during the game's save sequence is like switching
 * a GameCube off then. Ends without the exit handlers: tearing Aurora down under its render worker ended in a "Device
 * lost" fatal before (pc_pad_script.c's stop_run). The test programs (no game side) keep their own loop exit. */
void pc_log_drain(void); /* pc_log.c */
void pc_log_exit_clean(const char* how); /* pc_log.c: the clean-exit line, then the drain (5.1, 2026-11-23) */
static int s_close_ends_program; /* set by the game exe (pc_game_main.c); the test programs end their own loops */
void pc_vi_set_close_ends_program(int on) { s_close_ends_program = on; }
static void close_requested(const char* how) {
    s_exit_requested = 1;
    if (!s_close_ends_program) {
        return; /* a test program: its loop ends on pc_vi_exit_requested */
    }
    fprintf(stderr, "[vi] the window was closed%s (game frame %u): ending the program\n", how, pc_vi_heartbeat);
    fflush(stderr);
    pc_log_exit_clean("the game's window was closed");
    _Exit(0);
}

/* While no game frame ends (a stuck game, a long load), the window's events are still looked at: at each retrace,
 * once STUCK_FIELDS fields have passed without a frame, the OS's window messages are fetched (they stay queued for the
 * game's next frame) and a close request ends the program. Also keeps Windows from marking the window "not
 * responding". The retrace runs on the game's one OS thread, the window's thread, whenever the game reaches an
 * interrupt point (idle waits included); a thread that spins with no interrupt point is the one case it can't see. */
enum { STUCK_FIELDS = 30 };
static void check_close_while_stuck(void) {
    static u32 last_heartbeat, fields_stuck;
    if (!s_started || !s_close_ends_program) {
        return;
    }
    if (pc_vi_heartbeat != last_heartbeat) {
        last_heartbeat = pc_vi_heartbeat;
        fields_stuck = 0;
        return;
    }
    if (++fields_stuck < STUCK_FIELDS) {
        return;
    }
    SDL_PumpEvents();
    if (SDL_HasEvent(SDL_EVENT_QUIT) || SDL_HasEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED)) {
        close_requested(" while no game frame had ended for a while");
    }
}

static void poll_events(void) {
    const AuroraEvent* event = aurora_update();
    while (event != NULL && event->type != AURORA_NONE) {
        if (event->type == AURORA_EXIT) {
            close_requested("");
        }
        pc_platform_window_event(event);
        ++event;
    }
    pc_platform_window_frame();
}

#ifdef ORRE_SETTINGS_OVERLAY
/* The Settings overlay's pause (the RmlUi trial, pc_settings_overlay.cpp): called right after a finished frame, at the
 * game's display copy, so nothing of the next frame is recorded yet. Until the overlay lets go, Aurora presents empty
 * frames that keep the last picture (aurora_keep_efb_next_frame, as a frame with no display copy does), with the
 * overlay drawn over it in end_frame; the window and input keep running; the audio is held as the debug pause does. */
extern void (*pc_debug_audio_pause)(int on);
static void hold_for_overlay(void) {
    if (pc_debug_audio_pause != NULL) {
        pc_debug_audio_pause(1);
    }
    pc_smooth_hold(1, "the Settings overlay's pause"); /* frame smoothing: these frames show at once */
    while (pc_settings_overlay_wants_pause() && !s_exit_requested) {
        ++pc_vi_alive; /* a pause is not a hang (the watchdog) */
        pc_health_frame(); /* the menu's held frames count as frames in the performance log */
        poll_events();
        aurora_keep_efb_next_frame();
        if (aurora_begin_frame()) {
            s_in_frame = 1;
            end_frame(PC_SMOOTH_STEP_PAUSE);
        }
        SDL_Delay(16);
    }
    pc_smooth_hold(0, "the Settings overlay's pause");
    if (pc_debug_audio_pause != NULL) {
        pc_debug_audio_pause(0);
    }
}
#endif

/* While the debug toolkit's Pause holds the game (pc_debug_pause_point): Aurora re-presents the last finished frame at
 * full resolution (replaced textures included) with the F1 menu over it, and the game's open frame stays untouched,
 * to continue on resume (aurora_overlay_frame_*: the game's drawing is recorded and only runs at end_frame, so the
 * frame buffer still holds the last picture). Corrected 2026-10-17: before, a 640x480 RGB565 copy was drawn instead,
 * which made an HD texture pack look like the original textures while paused (snatcherLeo). The open frame's ImGui frame is
 * used by the first paused present; each later one starts its own; after the pause one more is started for the
 * open frame (present_paused_done). */
static unsigned s_paused_presents;
static void present_paused(void) {
    if (!s_started) {
        return;
    }
    /* frame smoothing: the presenter stops while this one presents from the game thread (two presenters on one
     * window would alternate the paused picture with in-between pictures); aurora_overlay_frame_present waits for the
     * render worker first, so no in-between picture is half made when it presents */
    pc_smooth_hold(1, "the debug toolkit's pause");
    if (s_paused_presents++ > 0 || !s_in_frame) {
        aurora_overlay_frame_begin();
    }
    pc_menu_bar_draw();
    pc_debug_ui_draw();
    aurora_overlay_frame_present();
}
static void present_paused_done(void) {
    if (s_paused_presents > 0 && s_in_frame) {
        aurora_overlay_frame_begin(); /* the open frame's ImGui frame was used up by the first paused present */
    }
    if (s_paused_presents > 0) {
        pc_smooth_hold(0, "the debug toolkit's pause");
    }
    s_paused_presents = 0;
}

/* The debug toolkit's speed setting (pc_debug.h): retraces come speed% as often; 100 = the GameCube's 59.94 Hz.
 * Only the debug toolkit changes it. */
static unsigned s_speed_percent = 100;
unsigned pc_vi_speed_percent(void) { return s_speed_percent; }
void pc_vi_set_speed_percent(unsigned percent) { s_speed_percent = percent < 10 ? 10 : percent > 1000 ? 1000 : percent; }
/* Settings > Enhancements > Game speed / Battles (pc_speed.c), on top of the debug menu's speed: both are "more of
 * the game's own steps per second", so they multiply */
static unsigned s_enh_speed_percent = 100;
/* up to 5x (snatcherLeo, 2026-11-16, the Speed settings: "maybe up to 5x"; 400 before) */
void pc_vi_set_enhancement_speed(unsigned percent) { s_enh_speed_percent = percent < 100 ? 100 : percent > 500 ? 500 : percent; }
static u64 field_ns(void) { return FIELD_NS * 10000u / ((u64)s_speed_percent * s_enh_speed_percent); }
/* frame smoothing (pc_smooth.c): retraces a second at the current speeds, for the step rate it compares with the
 * picture rate */
double pc_vi_fields_per_second(void) { return 1e9 / (double)field_ns(); }

/* When the next retrace is due (SDL_GetTicksNS time) */
u64 pc_vi_retrace_due_ns(void) {
    if (s_next_field_ns == 0) {
        s_next_field_ns = SDL_GetTicksNS() + field_ns();
    }
    return s_next_field_ns;
}

/* The deterministic replay clock (the project's S02 task, 2026-10-19: identical replays differed, Phenac entered at frames
 * 3553 to 3584 with the same inputs and exe, because retraces came from the host's clock). In replay mode a retrace
 * is an event of the game's own progress, never a timer: the thread layer asks for it (pc_vi_request_retrace) when
 * the game would wait for one (only the idle loop can run, or nothing can, pc_os_thread.c), and the retrace runs
 * the same callbacks, pad sampling and alarms in the same order as on the wall clock. OSGetTime counts from the
 * retraces (virtual_time), disc completions and draw-done handlers are delivered at fixed points (pc_dvd.c,
 * pc_gx_hooks.c), and the sound is mixed on the game thread, so many 5 ms blocks per retrace (pc_audio.c).
 * On in replay mode: ORRE_DVD_TRACE, ORRE_REPLAY_FRAMES or ORRE_PAD_SCRIPT set; ORRE_VIRTUAL_CLOCK=1 / 0 forces it.
 * Paced to real time (so a person can watch and hear it); ORRE_REPLAY_SPEED=max runs as fast as the CPU allows.
 * Normal play keeps the wall clock. */
#define TICKS_PER_FIELD 675675ull /* the 40.5 MHz timer: 40,500,000 * 1001 / 60,000 ticks per NTSC field */
/* OSGetTime moves on 8 ticks (0.2 us) per call inside a field, so a wait on the timer sees time pass. Corrected
 * 2026-10-19: at 50 us a call, frames that read the timer a few hundred times were cut by a timer retrace (two
 * retraces per game frame). A wait that outlasts a whole field asks for the retrace; time never runs backwards. */
#define TICKS_PER_CALL 8ull
static int s_virtual = -1;
static int s_virtual_max_speed;
static volatile int s_virtual_request;
static OSTime s_vtime_epoch, s_vtime_last;
static u32 s_vtime_field = 0xFFFFFFFFu;
static u64 s_vtime_offset;
static u32 s_vcause[5]; /* retraces by cause since the last report: idle loop, nothing runnable, spin, timer, other */
extern int pc_os_on_game_thread(void);   /* pc_os_thread.c */
extern void AuroraSetTimeSource(OSTime (*source)(void));

static OSTime virtual_time(void) {
    OSTime t;
    if (pc_os_on_game_thread()) { /* other host threads read the time without moving it on */
        if (s_vtime_field != s_retraces) {
            s_vtime_field = s_retraces;
            s_vtime_offset = 0;
        } else {
            s_vtime_offset += TICKS_PER_CALL;
            if (s_vtime_offset >= TICKS_PER_FIELD && !s_virtual_request) {
                static int s_logged;
                s_virtual_request = 4; /* a wait on the timer that outlasts the field: the retrace comes */
                if (s_logged < 2) { /* who waits on the timer (the stack; decode with addr2line -e orrecolo.exe) */
                    extern void pc_crash_print_stack(const char* reason);
                    s_logged++;
                    fprintf(stderr, "[vi] replay clock: a field's worth of timer reads at game frame %u\n",
                            pc_vi_heartbeat);
                    pc_crash_print_stack("replay clock: timer reads");
                }
            }
        }
    }
    t = s_vtime_epoch + (OSTime)s_retraces * (OSTime)TICKS_PER_FIELD + (OSTime)s_vtime_offset;
    if (t <= s_vtime_last) {
        t = s_vtime_last + 1;
    }
    s_vtime_last = t;
    return t;
}

/* Live play in the portable layout (a "Colosseum" folder next to the exe) also runs on the replay clock, paced to real
 * time (2026-11-01, the reference playthrough Part 1 item 3; for B to review): its timing then depends only on the game's progress
 * and its inputs, so the sitting's input recording (pc_pad_record.c) replays it exactly (docs/port/pad-recording.md:
 * the round trip). Its clock starts at the real local time; the recording keeps that start (pc_vi_clock_epoch). */
static int portable_live(void) {
    const char* window = getenv("ORRE_WINDOW");
    if (getenv("ORRE_PAD_SCRIPT") != NULL || (window != NULL && strcmp(window, "minimized") == 0)) {
        return 0;
    }
    /* *Updated 2026-11-21 (4.1):* the one game-folder check (pc_paths.h), shared with the recordings and the save
     * slots. Flat layout: a Colosseum folder next to the exe, exactly as before; portable layout (exe in app\):
     * always. This answer decides which clock live play runs on, so it must never change by accident: a sitting
     * played on the other clock would not replay (docs/port/portable-layout.md part 1c). */
    return pc_paths_game_folder_exists();
}

/* The replay clock's start (OSTime ticks since 2000-01-01, the GameCube's epoch), for the input recording's header */
OSTime pc_vi_clock_epoch(void) { return s_vtime_epoch; }

/* pc_pad_script.c, at a hand-over: a person plays from here, so a replay run at ORRE_REPLAY_SPEED=max is paced to real
 * time from now on (crash recovery: the sitting replays fast up to the hand-over, 2026-11-01).
 * *Fixed 2026-11-15 (snatcherLeo saw the test window go black and freeze at a hand-over):* the comment here said the next
 * retrace's long-stall check would resync the pacing, but that check only catches a clock that fell behind. A run at
 * max speed counts its retraces faster than real time, so the next retrace's due time had run ahead (18,433 fields,
 * about five minutes, against about a minute of real time) and the paced wait slept until then: the game drew nothing
 * and the hang watch fired. The pacing now restarts from the moment of the hand-over. All platforms the same. */
void pc_vi_pace_real_time(void) {
    if (s_virtual_max_speed) {
        s_virtual_max_speed = 0;
        s_next_field_ns = SDL_GetTicksNS() + field_ns();
        fprintf(stderr, "[vi] replay clock: paced to real time from game frame %u (the hand-over)\n", pc_vi_heartbeat);
    }
}

int pc_vi_virtual_clock(void) {
    if (s_virtual < 0) {
        const char* force = getenv("ORRE_VIRTUAL_CLOCK");
        const char* speed = getenv("ORRE_REPLAY_SPEED");
        const char* epoch = getenv("ORRE_VIRTUAL_EPOCH");
        int live = 0;
        if (force != NULL && *force != '\0') {
            s_virtual = atoi(force) != 0;
        } else {
            s_virtual = getenv("ORRE_DVD_TRACE") != NULL || getenv("ORRE_REPLAY_FRAMES") != NULL ||
                        getenv("ORRE_PAD_SCRIPT") != NULL || (live = portable_live());
        }
        s_virtual_max_speed = speed != NULL && strcmp(speed, "max") == 0;
        if (s_virtual) {
            if (epoch != NULL && *epoch != '\0') {
                /* ORRE_VIRTUAL_EPOCH=<ticks>: a recording's start, so its replay reads the same clock */
                s_vtime_epoch = (OSTime)strtoll(epoch, NULL, 0);
            } else if (live) {
                /* the real local time, as a GameCube's clock reads it (ticks since 2000-01-01 local, 40.5 MHz) */
                SDL_Time now;
                SDL_DateTime dt;
                long long seconds = 0;
                if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &dt, true)) {
                    seconds = (long long)(now / 1000000000ll) + dt.utc_offset - 946684800ll;
                }
                s_vtime_epoch = (OSTime)seconds * (OSTime)40500000ll;
            } else {
                /* a fixed start date (2026-01-01 00:00 UTC, in ticks since 2000-01-01), so the game's clock reads
                 * the same in every run */
                s_vtime_epoch = (OSTime)820540800ll * (OSTime)40500000ll;
            }
            AuroraSetTimeSource(virtual_time);
            fprintf(stderr, "[vi] replay clock: retraces follow the game's own progress, not the host's clock "
                            "(%s%s; ORRE_VIRTUAL_CLOCK=0 turns it off), start %lld\n",
                    s_virtual_max_speed ? "as fast as the CPU allows" : "paced to real time; ORRE_REPLAY_SPEED=max",
                    live ? ", live play in the portable layout" : "", (long long)s_vtime_epoch);
        }
    }
    return s_virtual;
}

/* pc_os_thread.c: the game would wait for the retrace now (replay clock only); cause 1 = only the idle loop could
 * run, 2 = nothing could, 3 = a thread spun a field's worth of interrupt points */
void pc_vi_request_retrace_for(int cause) {
    if (!s_virtual_request) {
        s_virtual_request = cause;
    }
}
void pc_vi_request_retrace(void) { pc_vi_request_retrace_for(5); }

/* pc_os_thread.c, at each interrupt point: is a retrace due? */
int pc_vi_retrace_due_now(void) {
    if (pc_vi_virtual_clock()) {
        return s_virtual_request;
    }
    return SDL_GetTicksNS() >= pc_vi_retrace_due_ns();
}

/* For pc_dvd.c's disc model (2026-10-19): the retrace count with the fraction of the current field already passed,
 * so a read can start part-way through a field (the console's drive does not wait for a retrace). With the replay
 * clock: the retrace count (the fraction would come from the host's clock). */
double pc_vi_retrace_clock(void) {
    if (pc_vi_virtual_clock()) {
        return (double)s_retraces;
    }
    const u64 now = SDL_GetTicksNS();
    const u64 due = pc_vi_retrace_due_ns();
    const double left = due > now ? (double)(due - now) / (double)field_ns() : 0.0;
    double phase = 1.0 - left;
    phase = phase < 0.0 ? 0.0 : phase > 0.999 ? 0.999 : phase;
    return (double)s_retraces + phase;
}

/* ORRE_VI_MIDFRAME=FROM-TO (2026-10-24; observes only): in those game frames, every retrace (replay clock) that
 * comes while the frame thread is not in VIWaitForRetrace, i.e. a game frame that spans two or more retraces, with the
 * cause and the call stack, and every block of the frame thread outside that wait (pc_os_thread.c). P02's battle 20 and
 * P01's Willie battle part at such a frame (waza_sequence_global's TOC read, Growl's effect load), the console has none
 * there: which wait holds the PC's frame */
volatile int pc_vi_in_retrace_wait;
void* volatile pc_vi_frame_thread;
struct OSThread;
extern struct OSThread* OSGetCurrentThread(void); /* pc_os_thread.c */
static int s_mid_on = -1;
static unsigned s_mid_from, s_mid_to;
int pc_vi_midframe_log_active(void) {
    if (s_mid_on < 0) {
        const char* env = getenv("ORRE_VI_MIDFRAME");
        s_mid_on = env != NULL && sscanf(env, "%u-%u", &s_mid_from, &s_mid_to) == 2;
    }
    return s_mid_on > 0 && pc_vi_heartbeat >= s_mid_from && pc_vi_heartbeat <= s_mid_to;
}

/* The retrace interrupt; the caller (pc_os_thread.c) has disabled interrupts */
void pc_vi_retrace_interrupt(void) {
    u64 now = SDL_GetTicksNS();
    if (pc_vi_virtual_clock()) {
        const int cause = s_virtual_request;
        if (!pc_vi_in_retrace_wait && pc_vi_midframe_log_active()) {
            extern void pc_crash_print_stack(const char* reason);
            static const char* const why[] = {"?", "idle loop", "nothing runnable", "spin", "timer", "other"};
            fprintf(stderr, "[vi] mid-frame retrace %u in game frame %u: %s\n", s_retraces + 1, pc_vi_heartbeat,
                    why[cause >= 1 && cause <= 5 ? cause : 0]);
            pc_crash_print_stack("ORRE_VI_MIDFRAME: the thread running at the retrace");
        }
        s_vcause[cause >= 1 && cause <= 5 ? cause - 1 : 4]++;
        if ((s_retraces + 1) % 3600 == 0) { /* once a minute of game time: why the retraces came */
            fprintf(stderr, "[vi] replay clock: last 3600 retraces (to game frame %u): idle loop %u, nothing runnable "
                            "%u, spin %u, timer %u, other %u\n",
                    pc_vi_heartbeat, s_vcause[0], s_vcause[1], s_vcause[2], s_vcause[3], s_vcause[4]);
            memset(s_vcause, 0, sizeof(s_vcause));
        }
        s_virtual_request = 0;
        if (!s_virtual_max_speed) { /* paced to real time: the game's logic does not see this wait */
            const u64 due = pc_vi_retrace_due_ns();
            while (now < due) {
                SDL_DelayPrecise(due - now);
                now = SDL_GetTicksNS();
            }
        }
    }
    if (now > pc_vi_retrace_due_ns() + 4 * field_ns()) {
        s_next_field_ns = now + field_ns(); /* a long stall: resync rather than race to catch up */
    } else {
        s_next_field_ns += field_ns();
    }
    ++s_retraces;
    if (s_virtual > 0) {
        extern void pc_gx_virtual_retrace(void); /* pc_gx_hooks.c: draw-done handlers at this fixed point */
        pc_gx_virtual_retrace();
    }
    if (s_pre != NULL) {
        s_pre(s_retraces);
    }
    if (s_pad_sampling != NULL) {
        s_pad_sampling();
    }
    pc_os_run_alarms();
    if (s_post != NULL) {
        s_post(s_retraces);
    }
    if (s_port_hook != NULL) {
        s_port_hook();
    }
    check_close_while_stuck();
    pc_os_thread_wakeup(&s_retrace_key);
}

/* ORRE_FRAME_TIMES=FROM-TO (2026-10-18, the project's T16 question: the picture stalls right after a battle's first send-out on
 * the console; does the PC show it, what takes the time, are game frames delayed?): one line per game frame in that
 * range: the game's own work since the last retrace wait returned (logic + recording draws), the time spent waiting,
 * how many retraces passed since the last return (1 = on time; 2+ = the game frame took that many fields, i.e. it was
 * delayed, as on the console when a frame's work overruns), and the disc reads started / completed in that frame. */
extern volatile u32 pc_dvd_reads_started, pc_dvd_bytes_started, pc_dvd_completions; /* pc_dvd.c */
static int s_ft_on = -1;
static unsigned s_ft_from, s_ft_to;
/* pc_dvd.c (and game-side diagnostics): is the current game frame inside ORRE_FRAME_TIMES's range? */
int pc_vi_frame_times_active(void) {
    return s_ft_on > 0 && pc_vi_heartbeat >= s_ft_from && pc_vi_heartbeat <= s_ft_to;
}
static void frame_times(int phase) {
    int on = s_ft_on;
    unsigned from = s_ft_from, to = s_ft_to;
    static Uint64 last_return;
    static u32 retraces_at_return, reads0, bytes0, done0;
    static Uint64 entry;
    if (on < 0) {
        const char* env = getenv("ORRE_FRAME_TIMES");
        on = env != NULL && sscanf(env, "%u-%u", &from, &to) == 2;
        s_ft_on = on;
        s_ft_from = from;
        s_ft_to = to;
        if (on) {
            fprintf(stderr, "[frametime] frames %u-%u: frame, work ms, wait ms, retraces since the last frame, display "
                            "copies (0 = no new picture), disc reads started (bytes) / completed\n", from, to);
        }
    }
    if (phase == 0) { /* entry: the game's work since the last return is done */
        /* 2026-11-07 (P12 note 9): ORRE_MAPOBJ_FRAMES, observe-only; weak, since the test programs that link
         * this file have no game code */
        extern void pc_mapobj_tick(unsigned frame) __attribute__((weak));
        entry = SDL_GetTicksNS();
        if (pc_mapobj_tick != NULL) {
            pc_mapobj_tick(pc_vi_heartbeat);
        }
        return;
    }
    {
        const Uint64 now = SDL_GetTicksNS();
        const unsigned frame = pc_vi_heartbeat;
        /* always on (2026-11-04, the reference playthrough P05 note 1: a live "lag" the replay couldn't show): any game frame whose
         * work took over 25 ms gets one line, so a player's own log shows where and how long; reads the clock only,
         * at most 500 lines a run */
        static unsigned slow_lines;
        if (last_return != 0 && entry > last_return && entry - last_return > 25000000u && slow_lines < 500) {
            ++slow_lines;
            fprintf(stderr, "[slowframe] game frame %u: work %.1f ms, disc reads started %u (%u B), completed %u%s\n",
                    frame, (double)(entry - last_return) / 1e6, pc_dvd_reads_started - reads0,
                    pc_dvd_bytes_started - bytes0, pc_dvd_completions - done0,
                    slow_lines == 500 ? " (the last [slowframe] line this run)" : "");
        }
        if (on && last_return != 0 && frame >= from && frame <= to) {
            fprintf(stderr, "[frametime] %u work %.2f wait %.2f retraces %u copies %d disc %u (%u B) / %u\n", frame,
                    (double)(entry - last_return) / 1e6, (double)(now - entry) / 1e6, s_retraces - retraces_at_return,
                    s_copies_this_frame, pc_dvd_reads_started - reads0, pc_dvd_bytes_started - bytes0,
                    pc_dvd_completions - done0);
        }
        s_copies_this_frame = 0;
        last_return = now;
        retraces_at_return = s_retraces;
        reads0 = pc_dvd_reads_started;
        bytes0 = pc_dvd_bytes_started;
        done0 = pc_dvd_completions;
    }
}

/* For pc_dvd.c's console-like sync reads (2026-10-19): the calling game thread sleeps until retrace `count`, as a
 * GameCube thread blocks while it waits for the drive, so other (lower-priority) threads run meanwhile. Not a game
 * frame: no heartbeat, no frame end. (Spinning instead starved the main thread: a higher-priority sound loader
 * thread held the processor through its sync reads and the game hung at start-up.) */
void pc_vi_sleep_until_retrace(u32 count) {
    BOOL enabled = OSDisableInterrupts();
    while ((s32)(count - s_retraces) > 0) {
        pc_os_thread_sleep_on(&s_retrace_key);
    }
    OSRestoreInterrupts(enabled);
}

void VIWaitForRetrace(void) {
    BOOL enabled;
    u32 count;

    frame_times(0);
    ++pc_vi_heartbeat;
    ++pc_vi_alive;
    pc_health_frame(); /* the performance log's frame times (pc_health.c) */
    if (pc_hero_watch_frame) {
        pc_hero_watch_frame(pc_vi_heartbeat); /* the player appearing: the pad script's "@0x1<room>" anchors */
    }
    pc_pad_script_frame(pc_vi_heartbeat); /* ORRE_PAD_SCRIPT (pc_pad_script.c) */
#if PC_HAVE_AURORA_CARD
    {
        extern void pc_card_timing_frame(unsigned frame); /* pc_card_timing.c: the card task's states, logged */
        pc_card_timing_frame(pc_vi_heartbeat);
    }
#endif
    if (pc_vi_frame_hook != NULL) {
        pc_vi_frame_hook(pc_vi_heartbeat); /* game-side diagnostics (ORRE_DUMP_FLAGS, pc_game_main.c) */
    }
    if (pc_event_log_frame) {
        pc_event_log_frame(pc_vi_heartbeat); /* ORRE_EVENT_LOG: message windows and flag bits (pc_debug_game.c) */
    }
    pc_debug_frame(pc_vi_heartbeat); /* debug toolkit: queued commands (pc_debug.c; ORRE_DEBUG=1 only) */
    pc_cheats_frame(); /* the Cheats menu's "Put in the PC" and money fill, on the game thread (pc_cheats.c) */
    pc_speed_frame();  /* Settings > Enhancements > Speed: this frame's pacing and sound speed (pc_speed.c) */
    pc_smooth_frame(); /* frame smoothing: the picture rate against the step rate, the [smooth] log (pc_smooth.c) */
    {
        /* ORRE_REPLAY_REAL_FROM=<game frame> (a test switch, 2026-11-17): a replay at ORRE_REPLAY_SPEED=max is paced to
         * real time from that frame on, through the hand-over's own path (pc_vi_pace_real_time), so a live-presenter
         * measurement of a late scene doesn't keep a visible test window up for minutes before it. Replay clock only;
         * all platforms the same. */
        static long real_from = -1;
        if (real_from < 0) {
            const char* v = getenv("ORRE_REPLAY_REAL_FROM");
            real_from = v != NULL ? atol(v) : 0;
        }
        if (real_from > 0 && (long)pc_vi_heartbeat == real_from) {
            pc_vi_pace_real_time();
        }
    }
    pc_debug_hotkeys();              /* F5 / F9 / F12 / Pause / '.' (ORRE_DEBUG=1 only) */
    pc_debug_pause_point(poll_events, present_paused, &s_exit_requested); /* the Pause key holds the game here */
    present_paused_done();
    {
        /* ORRE_TEST_STUCK_AT=FRAME (test switch, 2026-10-19): the game's main thread waits forever at that frame, as
         * in a hang, to test that closing the window still works (check_close_while_stuck) */
        static long stuck_at = -1;
        static char never;
        if (stuck_at < 0) {
            const char* env = getenv("ORRE_TEST_STUCK_AT");
            stuck_at = env != NULL ? atol(env) : 0;
        }
        if (stuck_at > 0 && pc_vi_heartbeat == (u32)stuck_at) {
            BOOL was = OSDisableInterrupts();
            fprintf(stderr, "[vi] ORRE_TEST_STUCK_AT: the main thread waits forever from game frame %u\n",
                    pc_vi_heartbeat);
            for (;;) {
                pc_os_thread_sleep_on(&never);
            }
            OSRestoreInterrupts(was);
        }
    }
    if (s_frame_log < 0) {
        s_frame_log = getenv("ORRE_FRAME_LOG") != NULL;
    }
    if (s_frame_log && pc_vi_heartbeat % 60 == 0) {
        fprintf(stderr, "[vi] game frame %u, retraces %u, presented %u, black %d\n", pc_vi_heartbeat, s_retraces,
                s_presented, (int)s_black);
        fflush(stderr);
    }
    if (s_frame_pending) {
        /* Before the first display copy (early boot), the retrace wait ends frames. Afterwards the copy does; if the
         * game hands VI two frames with no copy in between (a screen that never goes through HSD's copy: to watch,
         * e.g. movies), the retrace wait ends them again until the next copy, so nothing ever stops presenting. */
        if (s_copy_ends_frame && ++s_frames_without_copy >= 2) {
            /* No display copy: on the GameCube the TV keeps showing the previous XFB. So the PC keeps the window's
             * last picture and leaves Aurora's frame open (the game's drawing keeps recording into it), instead of
             * presenting the EFB mid-work: Confusion / Psychic's distortion had just copied the EFB into its texture
             * and cleared it, and that frame showed black (A17, A, 2026-10-18). Two exceptions end the frame here:
             * a movie frame drawn without HSD's copy (the THP draw, pc_thp_draw.c: it must be shown), and a hold of
             * more than HOLD_MAX frames (a screen that never copies; and so one Aurora frame never records for long) */
            enum { HOLD_MAX = 30 };
            const int movie = pc_vi_movie_drew;
            pc_vi_movie_drew = 0;
            if (s_frames_without_copy == 2) {
                fprintf(stderr, "[vi] no display copy for 2 frames (game frame %u): %s\n", pc_vi_heartbeat,
                        movie ? "a movie frame: frames end at the retrace until the next copy"
                              : "the last picture stays (as on the TV) until the next copy");
            }
            if (s_in_frame && (movie || s_frames_without_copy >= 2 + HOLD_MAX)) {
                if (!movie && s_frames_without_copy == 2 + HOLD_MAX) {
                    fprintf(stderr, "[vi] no display copy for %d frames (game frame %u): frames end at the retrace "
                            "until the next copy\n", s_frames_without_copy, pc_vi_heartbeat);
                }
                end_frame(movie ? PC_SMOOTH_STEP_MOVIE : PC_SMOOTH_STEP_NO_COPY);
                /* no display copy, so on the GameCube nothing emptied the EFB: the next frame keeps its picture
                 * (Aurora would clear it). Willie's battle-start swirl showed black frames during the battle room's
                 * load and blended its ring over a black capture (A, 2026-10-18) */
                aurora_keep_efb_next_frame();
            }
        } else if (!s_copy_ends_frame && s_in_frame) {
            end_frame(PC_SMOOTH_STEP_NO_COPY); /* early boot, before the game's first display copy */
        }
        s_current_fb = s_next_fb;
        s_frame_pending = 0;
    }
    if (s_started) {
        poll_events();
    }

    enabled = OSDisableInterrupts();
    count = s_retraces;
    pc_vi_frame_thread = OSGetCurrentThread(); /* ORRE_VI_MIDFRAME */
    pc_vi_in_retrace_wait = 1;
    while (count == s_retraces) {
        pc_os_thread_sleep_on(&s_retrace_key);
    }
    pc_vi_in_retrace_wait = 0;
    OSRestoreInterrupts(enabled);

    if (s_started && !s_in_frame) {
        s_in_frame = aurora_begin_frame(); /* false while the window can't present (minimised): try next field */
        apply_draws_blocking();
    }
    {
        extern void pc_renderdoc_frame(unsigned frame); /* pc_renderdoc.c: ORRE_RDOC_CAPTURE, a test aid */
        pc_renderdoc_frame(pc_vi_heartbeat);
    }
    {
        /* Aurora's observe-only GX depth log (ORRE_GX_ZLOG, extern/aurora/lib/dolphin/gx/GXPixel.cpp) reads the game
         * frame from here, so its window is in the same frames as ORRE_DUMP_FRAMES and ORRE_RDOC_CAPTURE. Any OS */
        extern unsigned orre_gx_zlog_frame;
        orre_gx_zlog_frame = pc_vi_heartbeat;
    }
    frame_times(1);
}

void VISetNextFrameBuffer(void* fb) {
    s_next_fb = fb;
    s_frame_pending = 1;
}

void* VIGetNextFrameBuffer(void) { return s_next_fb; }
void* VIGetCurrentFrameBuffer(void) { return s_current_fb; }
u32 VIGetRetraceCount(void) { return s_retraces; }
u32 VIGetNextField(void) { return s_retraces & 1; }
void VISetBlack(BOOL black) { s_black = black; } /* PORT: kept, not drawn yet (the game uses it around mode changes) */
u32 VIGetDTVStatus(void) { return 0; }           /* PORT: "no component cable": 480i; progressive is a later setting */

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) {
    BOOL enabled = OSDisableInterrupts();
    VIRetraceCallback old = s_pre;
    s_pre = cb;
    OSRestoreInterrupts(enabled);
    return old;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) {
    BOOL enabled = OSDisableInterrupts();
    VIRetraceCallback old = s_post;
    s_post = cb;
    OSRestoreInterrupts(enabled);
    return old;
}

/* HSD calls these two by their GameCube address names (src/hsd/video.c: fn_800A880C "VISetPreRetraceCallback",
 * fn_800A8850 "VISetPostRetraceCallback"); provided under both names until they are renamed. */
VIRetraceCallback fn_800A880C(VIRetraceCallback cb) { return VISetPreRetraceCallback(cb); }
VIRetraceCallback fn_800A8850(VIRetraceCallback cb) { return VISetPostRetraceCallback(cb); }
