/*
 * musyx_play: play one Colosseum sound effect through MusyX's PC renderer and write it to a WAV file
 * (OrrePort audio path, first step; docs/port/musyx-evaluation.md).
 *
 * Step 1 (this test): AxioDL MusyX's own runtime + its PC backend (extern/musyx-pc, CC0), rendering without an audio
 * device (sndPCRender), fed with Colosseum's real sound data from the player's disc. Step 2 (next): the same with our
 * matched runtime (src/musyx) in place of AxioDL's.
 *
 *   musyx_play <proj> <pool> <sdir> <samp> <out.wav> [fx id] [seconds]
 *
 * The four inputs are one MusyX group set as extracted from common.fsys (for example extracted/audio/musyx_data/
 * original/snd_se_proj.bin, snd_se_pool.bin, snd_se_sdir.bin and extracted/audio/se/original/snd_se.samp); they are
 * the player's own data and are never committed. Without an fx id it plays the first effect of the first
 * sound-effect group. Prints the peak and RMS level, so a silent result is visible without listening.
 */
#include "musyx/musyx.h"
#include "musyx/pc.h"

#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MUSYX_PLAY_OURS: built on our matched runtime (target musyx_play_ours). It keeps pointers in 32-bit values, so
 * everything it is handed must be below 4 GB (docs/port/low-memory.md): files and MusyX's own allocations come from
 * pc_lowmem there, as they will in the port. */
#ifdef MUSYX_PLAY_OURS
#include "pc_crash.h"
#include "pc_lowmem.h"
static void* play_alloc(size_t n) { return pc_lowmem_alloc(n); }
static void play_free(void* p) { pc_lowmem_free(p); }
#else
static void* play_alloc(size_t n) { return malloc(n); }
static void play_free(void* p) { free(p); }
#endif

#define RATE 32000
#define CHANNELS 2

static void* load(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    void* data;
    long n;
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = play_alloc((size_t)n + 32);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(2);
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

/* Diagnostics: AxioDL's debugger hook prints the first sound-macro steps and voice events (not in musyx_play_ours:
 * our runtime has no debugger hooks) */
#ifndef MUSYX_PLAY_OURS
#include "musyx/debugger.h"
static int s_events;
static void on_event(const MUSYX_DEBUG_EVENT* e, void* user) {
    (void)user;
    if (e->kind != MUSYX_DEBUG_EVENT_FRAME && s_events++ < 40) {
        printf("  event %u: voice %u macro %u opcode 0x%02X pc %u args 0x%X 0x%X message %d\n", (unsigned)e->kind,
               (unsigned)e->voiceIndex, (unsigned)e->macroId, (unsigned)e->opcode, (unsigned)e->pc,
               (unsigned)e->arg0, (unsigned)e->arg1, (int)e->message);
    }
}
#endif

/* sndIsIdle is not in Colosseum's MusyX (the game never calls it): -1 when testing our runtime */
#ifdef MUSYX_PLAY_OURS
#define MUSYX_PLAY_IDLE() (-1)
#else
#define MUSYX_PLAY_IDLE() ((int)sndIsIdle())
#endif

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

static void put32(FILE* f, uint32_t v) { fputc(v & 0xFF, f); fputc(v >> 8 & 0xFF, f); fputc(v >> 16 & 0xFF, f); fputc(v >> 24, f); }
static void put16(FILE* f, uint16_t v) { fputc(v & 0xFF, f); fputc(v >> 8, f); }

int main(int argc, char* argv[]) {
    size_t projSize, poolSize, sdirSize, sampSize, frames, i;
    uint8_t *proj, *pool, *sdir, *samp;
    uint32_t offset = 0, seconds;
    int groupId = -1, fxId = -1, fxCount = 0, songGroup = -1;
    /* song mode: ... <out.wav> song <song file> <song id> [seconds] (the song group of the project) */
    int songMode = argc > 8 && strcmp(argv[6], "song") == 0;
    SND_PC_CONFIG config = {RATE, CHANNELS};
    SND_PC_GROUP_ASSETS assets;
    SND_PC_ASSET_ERROR error;
    SND_VOICEID voice;
    s16* pcm;
    double sumSquares = 0;
    int peak = 0;
    FILE* out;

    setvbuf(stdout, NULL, _IONBF, 0); /* nothing lost if the renderer crashes */
#ifdef MUSYX_PLAY_OURS
    pc_crash_install();
#endif
    if (argc < 6) {
        fprintf(stderr, "usage: musyx_play <proj> <pool> <sdir> <samp> <out.wav> [fx id] [seconds]\n"
                        "       musyx_play <proj> <pool> <sdir> <samp> <out.wav> song <song file> <song id> [seconds]\n");
        return 2;
    }
    proj = load(argv[1], &projSize);
    pool = load(argv[2], &poolSize);
    sdir = load(argv[3], &sdirSize);
    samp = load(argv[4], &sampSize);
    seconds = songMode ? (argc > 9 ? (uint32_t)atoi(argv[9]) : 10) : (argc > 7 ? (uint32_t)atoi(argv[7]) : 3);

    /* Groups: +0 offset of the next group (0xFFFFFFFF ends the list), +4 group id, +6 type (1 = sound effects),
     * +28 the effect table: count, then 10-byte entries starting with the effect id */
    while (offset + 32 <= projSize && be32(proj + offset) != 0xFFFFFFFFu) {
        uint16_t id = be16(proj + offset + 4), type = be16(proj + offset + 6);
        if (type == 1) {
            uint32_t table = be32(proj + offset + 28);
            uint16_t count = be16(proj + table);
            printf("group %u: %u sound effects (ids %u..%u)\n", id, count, be16(proj + table + 4),
                   be16(proj + table + 4 + (count - 1) * 10));
            if (groupId < 0 && count > 0) {
                groupId = id;
                fxCount = count;
                fxId = be16(proj + table + 4);
            }
        } else {
            printf("group %u: songs\n", id);
            if (songGroup < 0) {
                songGroup = id;
            }
        }
        offset = be32(proj + offset);
    }
    if (argc > 6 && !songMode) {
        fxId = atoi(argv[6]);
    }
    if (songMode) {
        groupId = songGroup;
    }
    if (groupId < 0) {
        fprintf(stderr, "no sound-effect group in %s\n", argv[1]);
        return 1;
    }

    assets.project = (SND_PC_SPAN){proj, projSize};
    assets.pool = (SND_PC_SPAN){pool, poolSize};
    assets.directory = (SND_PC_SPAN){sdir, sdirSize};
    assets.samples = (SND_PC_SPAN){samp, sampSize};
    printf("step: validate\n");
    if (!sndPCValidateGroup(&assets, &error)) {
        fprintf(stderr, "invalid data: %s at 0x%zX: %s\n", error.section, error.offset, error.reason);
        return 1;
    }
    {
        /* MusyX allocates through these (AxioDL's own test does the same) */
        static SND_HOOKS hooks = {play_alloc, play_free};
        printf("step: hooks\n");
        sndSetHooks(&hooks);
    }
    printf("step: configure\n");
    /* MUSYX_PLAY_LIVE=1 (our runtime build only): play through the default audio device with AxioDL's SDL3 adapter
     * instead of rendering into memory; opened before sndInit, as the adapter requires */
    int live = 0;
#ifdef MUSYX_PLAY_OURS
    live = getenv("MUSYX_PLAY_LIVE") != NULL;
    if (live) {
        SND_PC_CONFIG obtained;
        /* the audio thread takes the game's interrupt lock (port/src/pc_os_sdk.c), as it will in the port */
        extern void pc_os_audio_enter(void);
        extern void pc_os_audio_leave(void);
        sndPCSetSynchronization(pc_os_audio_enter, pc_os_audio_leave);
        if (!sndPCOpenAudio(&config, &obtained)) {
            fprintf(stderr, "sndPCOpenAudio failed\n");
            return 1;
        }
        printf("audio device open: %u Hz, %u channels\n", (unsigned)obtained.mixRate, (unsigned)obtained.channels);
    }
#endif
    if (!live && !sndPCConfigure(&config)) {
        fprintf(stderr, "sndPCConfigure failed\n");
        return 1;
    }
    /* As the game's sound init does (src/game/gs_range_801653CC.c): 64 voices, 48 music, 16 effects, 1 studio,
     * flags 0, ARAM 0x9FC000 */
    printf("step: sndInit\n");
    if (sndInit(0x40, 0x30, 0x10, 1, 0, 0x9FC000) != 0) {
        fprintf(stderr, "sndInit failed\n");
        return 1;
    }
    /* The PC backend takes the disc's raw sections (its own decoder reads them, salPCDecodeGroup); the older
     * sndConvert32BitSDIRTo64BitSDIR step from AxioDL's test makes sndPushGroup fail here */
    printf("step: sndPushGroup\n");
    if (!sndPushGroup(proj, (SND_GROUPID)groupId, samp, sdir, pool)) {
        fprintf(stderr, "sndPushGroup(%d) failed\n", groupId);
        return 1;
    }
#ifndef MUSYX_PLAY_OURS
    if (getenv("MUSYX_PLAY_TRACE") != NULL) {
        static const MUSYX_DEBUG_HOOKS dbg = {on_event, NULL};
        musyxDebuggerSetHooks(&dbg);
    }
#endif
    /* Volume groups: every group's fader starts at 0 after sndInit, and Colosseum's effects play in group 31, so
     * nothing is heard until the volumes are set. The game's sound init sets them right after sndInit
     * (gs_range_801653CC.c: soundMasterVolume(0x7F, 0, music) and soundMasterVolume(0x64, 0, effects), which end in
     * sndVolume(127, 0, SND_MUSIC_VOLGROUPS) and sndVolume(100, 0, SND_FX_VOLGROUPS)); do the same. */
    sndVolume(0x7F, 0, SND_MUSIC_VOLGROUPS);
    sndVolume(0x64, 0, SND_FX_VOLGROUPS);
    /* Reverb, as the game's sound init sets it up (gs_range_801653CC.c: sndAuxCallbackPrepareReverbHI, then
     * sndSetAuxProcessingCallbacks on studio 0 aux A). MUSYX_PLAY_REVERB=1: fixed test parameters (cross-talk non-zero
     * so DoCrossTalk runs too); MUSYX_PLAY_REVERB=c,m,t,d,p,x: coloration, mix, time, damping, pre-delay, cross-talk,
     * e.g. one of the game's presets from common_rel (port/tools/musyx_game_songs.py passes the song's). */
    if (getenv("MUSYX_PLAY_REVERB") != NULL) {
        SND_AUX_REVERBHI* rev = (SND_AUX_REVERBHI*)play_alloc(sizeof(SND_AUX_REVERBHI));
        float v[6] = {0.5f, 0.5f, 2.0f, 0.5f, 0.05f, 0.3f};
        const char* spec = getenv("MUSYX_PLAY_REVERB");
        if (strchr(spec, ',') != NULL &&
            sscanf(spec, "%f,%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
            fprintf(stderr, "MUSYX_PLAY_REVERB: expected 6 comma-separated numbers\n");
            return 1;
        }
        rev->coloration = v[0];
        rev->mix = v[1];
        rev->time = v[2];
        rev->damping = v[3];
        rev->preDelay = v[4];
        rev->crosstalk = v[5];
        sndAuxCallbackPrepareReverbHI(rev);
        sndSetAuxProcessingCallbacks(0, sndAuxCallbackReverbHI, rev, 0xFF, 0, NULL, NULL, 0xFF, 0);
        printf("reverb on (studio 0, aux A): %g %g %g %g %g %g\n", v[0], v[1], v[2], v[3], v[4], v[5]);
    }
    if (songMode) {
        size_t songSize;
        void* song = load(argv[7], &songSize);
        int songId = atoi(argv[8]);
        voice = sndSeqPlayEx((SND_GROUPID)groupId, (SND_SONGID)songId, song, NULL, 0);
        printf("song group %d: sndSeqPlayEx(song %d, %s) -> sequence 0x%08X\n", groupId, songId, argv[7],
               (unsigned)voice);
        if (voice == SND_ID_ERROR) {
            return 1;
        }
    } else {
    voice = sndFXStart((SND_FXID)fxId, 127, 64);
    printf("group %d (%d effects): sndFXStart(%d) -> voice 0x%08X\n", groupId, fxCount, fxId, (unsigned)voice);
    /* As the game does (src/game/gs_dvd.c: sndFXStartEx, then sndFXCtrl(handle, 7, volume)): the effect macros read
     * their volume from MIDI controller 7 (macro opcode 0x40, VolumeSelect). Same result without it for full volume
     * (MUSYX_PLAY_NO_CTRL7 skips it). */
    if (voice != SND_ID_ERROR && getenv("MUSYX_PLAY_NO_CTRL7") == NULL) {
        printf("sndFXCtrl(voice, 7, 127) -> %d\n", (int)sndFXCtrl(voice, 7, 127));
    }
    }

    printf("after start: sndFXCheck = 0x%08X, idle = %d\n", (unsigned)sndFXCheck(voice), MUSYX_PLAY_IDLE());
#ifdef MUSYX_PLAY_OURS
    if (live) {
        clock_t until;
        SND_PC_RENDER_INFO info;
        if (!sndPCStartAudio()) {
            fprintf(stderr, "sndPCStartAudio failed\n");
            return 1;
        }
        until = clock() + (clock_t)seconds * CLOCKS_PER_SEC;
        while (clock() < until) {
        }
        info = sndPCGetRenderInfo();
        sndPCStopAudio();
        printf("played live for %u s: %llu frames delivered to the device (%.2f s of audio)\n", seconds,
               (unsigned long long)info.outputFrames, (double)info.outputFrames / info.mixRate);
        printf("step: sndQuit\n");
        sndQuit();
        return info.outputFrames > 0 ? 0 : 1;
    }
#endif
    frames = (size_t)RATE * seconds;
    pcm = (s16*)calloc(frames * CHANNELS, sizeof(s16));
    if (pcm == NULL || !sndPCRender(pcm, frames)) {
        fprintf(stderr, "sndPCRender failed\n");
        return 1;
    }
    {
        SND_PC_RENDER_INFO info = sndPCGetRenderInfo();
        printf("after render: sndFXCheck = 0x%08X, idle = %d, control ticks %llu, rendered frames %llu\n",
               (unsigned)sndFXCheck(voice), MUSYX_PLAY_IDLE(), (unsigned long long)info.controlTicks,
               (unsigned long long)info.renderedFrames);
    }
    for (i = 0; i < frames * CHANNELS; i++) {
        int v = abs(pcm[i]);
        peak = v > peak ? v : peak;
        sumSquares += (double)pcm[i] * pcm[i];
    }
    printf("rendered %u s at %u Hz stereo: peak %d, RMS %.1f (%s)\n", seconds, RATE, peak,
           sqrt(sumSquares / (double)(frames * CHANNELS)), peak > 0 ? "SOUND" : "SILENT");

    out = fopen(argv[5], "wb");
    if (out == NULL) {
        fprintf(stderr, "cannot write %s\n", argv[5]);
        return 1;
    }
    fwrite("RIFF", 1, 4, out);
    put32(out, (uint32_t)(36 + frames * CHANNELS * 2));
    fwrite("WAVEfmt ", 1, 8, out);
    put32(out, 16);
    put16(out, 1);
    put16(out, CHANNELS);
    put32(out, RATE);
    put32(out, RATE * CHANNELS * 2);
    put16(out, CHANNELS * 2);
    put16(out, 16);
    fwrite("data", 1, 4, out);
    put32(out, (uint32_t)(frames * CHANNELS * 2));
    for (i = 0; i < frames * CHANNELS; i++) {
        put16(out, (uint16_t)pcm[i]);
    }
    fclose(out);
    printf("step: sndQuit\n");
    sndQuit();
    return peak > 0 ? 0 : 1;
}
