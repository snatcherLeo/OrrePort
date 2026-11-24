/*
 * pc_audio.c: the game's sound on the PC device (docs/port/musyx-evaluation.md, "Audio path").
 *
 * The game sets up MusyX itself (gs_range_801653CC.c soundInit: sndInit, reverb, volumes); on the PC the MusyX PC
 * layer (AxioDL's, hw_pc_sdl.c) also needs, before sndInit, the interrupt lock shared with its audio thread and an
 * opened (paused) output device, and after sndInit the device started. pc_audio_init does the first part at port
 * startup; the device is started from the retrace interrupt at the first field after the game's sndInit has
 * finished (sndIsInstalled), so no game code changes: at most one field (16.7 ms) later than right after sndInit,
 * long before the game plays its first sound. pc_audio_stop before shutdown.
 */
#include <musyx/musyx.h>
#include <musyx/pc.h>

#include <stdio.h>
#include <stdlib.h>
#include "pc_lowmem.h"

extern void pc_os_audio_enter(void);
extern void pc_os_audio_leave(void);
extern void pc_vi_set_retrace_hook(void (*hook)(void));
extern int pc_platform_automated_run(void);
extern int pc_settings_int(const char* key, int fallback);
void pc_audio_set_volumes(int master, int music, int effects);

static int s_open, s_started;
static SND_PC_CONFIG s_config;

/* The sound split (2026-11-16; pc_sound_split.c, docs/port/sound-split.md): a second, audible MusyX copy plays the
 * music at its normal tempo while this (the game's) instance keeps the console's 1x schedule. *The normal path since
 * the Speed settings' build (2026-11-16):* on in every live game on the replay clock, idle at 1x; pc_sound_split_open
 * says when it runs (ORRE_SOUND_SPLIT=0 / 1 only as a test override). Weak: pc_sound_split.o is linked only into the
 * game exe (it holds the --wrap wrappers); other programs that link this library run without it. s_split: the split
 * runs this session. Platforms: plain C over MusyX's PC layer and SDL, the same on Windows, Linux and macOS (the
 * --wrap link step is GNU ld's: pc_sound_split.c says what macOS would need). */
int pc_sound_split_open(void) __attribute__((weak));
void pc_sound_split_retrace(void) __attribute__((weak));
void pc_sound_split_set_speed(unsigned game_percent, int music_follows) __attribute__((weak));
void pc_sound_split_set_volumes(int master, f32 (*scale)(int is_effect)) __attribute__((weak));
void pc_sound_split_stop(void) __attribute__((weak));
static int s_split;
/* set while this instance mixes: MusyX's own calls to its API from inside the mix are not the game's, so the split's
 * wrappers don't give them to the audible copy (which makes its own) */
int pc_audio_rendering;

/* Settings > Audio (snatcherLeo, 2026-11-11: "actually make those things real"): the master volume is the output stream's
 * gain (sndPCSetMasterGain: everything heard, the mix itself unchanged); music and sound effects scale each voice
 * through OrreHook_SoundVolumeScale in the mixer (src/musyx/runtime/synth.c), after the game's own volume faders,
 * which the game resets itself. All three are percentages, 100 = as the game mixes it. Automated runs never call
 * this, so their sound (and its dumps) stays vanilla. */
static float s_music = 1.f, s_effects = 1.f;
static f32 sound_volume_scale(int is_effect) { return is_effect ? s_effects : s_music; }
extern f32 (*OrreHook_SoundVolumeScale)(int isEffect); /* src/musyx/runtime/synth.c */

void pc_audio_set_volumes(int master, int music, int effects) {
    sndPCSetMasterGain(master / 100.f);
    s_music = music / 100.f;
    s_effects = effects / 100.f;
    /* the hook only while a volume differs from 100, so the default path is the game's own, unchanged */
    OrreHook_SoundVolumeScale = music == 100 && effects == 100 ? NULL : sound_volume_scale;
    if (s_split) { /* the split's audible copy: the same volumes */
        pc_sound_split_set_volumes(master, OrreHook_SoundVolumeScale);
    }
}

/* Settings > Enhancements > Speed (pc_speed.c; snatcherLeo, 2026-11-13, mockup 67; *renamed 2026-11-16:* "Music when faster":
 * "Normal tempo" / "Faster too"): the game runs at game_percent, and the music either follows it ("Faster too") or
 * keeps its normal tempo ("Normal tempo"). With the sound split running (the normal path) this instance always keeps
 * the console's blocks per frame and the split decides what is heard (below). Without it, two ways the mix is made,
 * and both are handled:
 *   - the render thread (no replay clock): it mixes whenever the device wants more, so the output's playback speed
 *     alone decides: game_percent when the music follows, else 1 (MusyX's hw_pc_sdl.c says why);
 *   - the replay clock (live play in the portable layout, recorded sittings, replays): the mix is rendered on the game
 *     thread, a fixed number of blocks per game frame, so a faster game made faster music whatever the setting
 *     (*Corrected 2026-11-13:* snatcherLeo heard the music speed up on "Normal speed"; verified from this file). Now the blocks
 *     per frame are divided by the game's speed when the music keeps its tempo (s_block_div), and the device plays
 *     them at the game's speed when it follows. At 1x, or with the music following, the blocks per frame are exactly
 *     the console's, as before (the same integer formula), so recordings replay unchanged.
 * Platforms: plain C over MusyX's PC layer and SDL; the same on Windows, Linux and macOS. */
static u32 s_block_div = 100;  /* the blocks per frame are multiplied by 100 / s_block_div (replay clock only) */
static u64 s_block_base;       /* blocks rendered before the last change of s_block_div */
static void rebase_manual(void);

void pc_audio_set_speed(unsigned game_percent, int music_follows) {
    u32 div = music_follows ? 100u : (game_percent < 100u ? 100u : game_percent);
    float play = music_follows ? (float)game_percent / 100.0f : 1.0f;
    if (s_split) {
        /* the sound split: this (the game's) instance always keeps the console's blocks per frame, whatever the music
         * does, so the game's sound waits keep their 1x frame counts; what is heard of it (the movies; everything with
         * "Faster too") plays at the game's speed, with the picture. The split takes the music setting. */
        div = 100u;
        play = game_percent < 100u ? 1.0f : (float)game_percent / 100.0f;
        pc_sound_split_set_speed(game_percent, music_follows);
    }
    sndPCSetPlaybackSpeed(play);
    if (div != s_block_div) {
        rebase_manual(); /* the blocks so far stay as rendered; the new rate counts from this frame */
        s_block_div = div;
    }
}

/* The replay clock (pc_vi.c, 2026-10-19): the mix is rendered here, on the game thread, as many 5 ms blocks as the
 * console would have played by this retrace (its 32,028 Hz output: about 3.34 blocks a field), so sound progress and
 * the callbacks the mixer runs follow the game's clock, never the sound device's. */
extern int pc_vi_virtual_clock(void);
extern u32 VIGetRetraceCount(void);
static u32 s_manual_from;   /* the retrace the device started at */
static u64 s_manual_blocks; /* blocks rendered since then */

/* Removed 2026-10-26 (snatcherLeo: PC time; the project notes): ORRE_AUDIO_LAG_BLOCKS, N blocks fewer than
 * due by the retrace (a constant lag cancels: no change to Uproar's end on P01). Removed 2026-11-01 (snatcherLeo's OK, the
 * same page): ORRE_AUDIO_CATCHUP / ORRE_AUDIO_START_DELAY and the OrreHook_AudioCatchUp hook (mixing up to the
 * field's current point at effect starts and checks: P01's old Folly resync; no final replay uses it) */
static void rebase_manual(void) {
    s_block_base = s_manual_blocks;
    s_manual_from = VIGetRetraceCount();
}

static void render_due_blocks(void) {
    const u64 fields = (u64)(VIGetRetraceCount() - s_manual_from);
    const u64 rate = s_config.mixRate == 32000 ? 32028 : s_config.mixRate; /* the console's 54 MHz-derived rate */
    const u64 block = (s_config.mixRate + 199) / 200;
    /* blocks played by this retrace (59.94 fields a second); with the music at normal tempo while the game runs faster,
     * fewer per frame (pc_audio_set_speed). At s_block_div 100 and no change since the start this is the original
     * formula exactly (fields * rate * 1001 / 60000 / block) */
    const u64 due = s_block_base + fields * rate * 1001 * 100 / ((u64)60000 * block * s_block_div);
    if (due > s_manual_blocks) {
        ++pc_audio_rendering;
        sndPCRenderManual((u32)(due - s_manual_blocks));
        --pc_audio_rendering;
        s_manual_blocks = due;
    }
    if (s_split) {
        pc_sound_split_retrace(); /* the audible copy's blocks, and what the game instance has finished */
    }
}

static void start_when_installed(void) { /* retrace interrupt, game thread */
    if (s_open && !s_started && sndIsInstalled()) {
        const int manual = pc_vi_virtual_clock();
        sndPCSetManualRender(manual != 0);
        s_started = sndPCStartAudio();
        if (!s_started) {
            fprintf(stderr, "[pc_audio] cannot start the audio device\n");
            s_open = 0;
        } else if (manual) {
            s_manual_from = VIGetRetraceCount();
            /* (the retrace it counts from: where an ORRE_DUMP_AUDIO file's first sample sits on the game's clock) */
            fprintf(stderr, "[pc_audio] replay clock: the mix is rendered on the game thread, per retrace (from retrace %u)\n",
                    (unsigned)s_manual_from);
        }
        if (s_split) {
            pc_sound_split_retrace(); /* starts the audible copy at the same retrace */
        }
        return;
    }
    if (s_started && pc_vi_virtual_clock()) {
        render_due_blocks();
    }
}

/* Port startup, before the game's sndInit. Returns whether an output device is open (no device: the game runs
 * silently). */
int pc_audio_init(void) {
    SND_PC_CONFIG preferred = {32000, 2}; /* MusyX's native rate on the GameCube, stereo */
    SND_PC_CONFIG obtained;
    sndPCSetSynchronization(pc_os_audio_enter, pc_os_audio_leave);
    /* the PC sound layer's own buffers (converted groups, decoded songs) from the port's low memory, not the game's heap */
    sndPCSetHostAllocator(pc_lowmem_alloc, pc_lowmem_free);
    s_open = sndPCOpenAudio(&preferred, &obtained);
    if (s_open) {
        s_config = obtained;
        printf("[pc_audio] audio device: %u Hz, %u channels\n", (unsigned)obtained.mixRate, (unsigned)obtained.channels);
        /* the sound split (the normal path; pc_sound_split_open decides): its audible copy's output, opened before the
         * game's sndInit as this one is */
        s_split = pc_sound_split_open != NULL && pc_sound_split_open();
        if (!pc_platform_automated_run()) { /* the player's volumes (automated runs: the game's own) */
            const int master = pc_settings_int("volume_master", 100), music = pc_settings_int("volume_music", 100),
                      effects = pc_settings_int("volume_effects", 100);
            pc_audio_set_volumes(master, music, effects);
            printf("[pc_audio] volumes: master %d, music %d, sound effects %d (settings.ini)\n", master, music, effects);
        }
        pc_vi_set_retrace_hook(start_when_installed);
    } else {
        fprintf(stderr, "[pc_audio] no audio device: the game runs silently\n");
    }
    return s_open;
}

int pc_audio_started(void) { return s_started; }

void pc_audio_stop(void) {
    if (s_split) {
        pc_sound_split_stop();
        s_split = 0;
    }
    if (s_open) {
        sndPCStopAudio();
    }
    s_open = s_started = 0;
}
