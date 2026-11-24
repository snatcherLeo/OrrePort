/*
 * musyx_pc_groups.c: bridge from our MusyX runtime's sndPushGroup to AxioDL's PC group decoder (
 * docs/port/musyx-evaluation.md, "Step 2").
 *
 * The disc's MusyX sections (project, pool with the sound macros / curves / key maps / layers, sample directory) are
 * big-endian. AxioDL's salPCDecodeGroup (hw_pc_assets.c, part of the PC hardware layer we link) validates them and
 * makes native-byte-order copies in the same layout, the sample directory in the layout of AxioDL's PC sample code
 * (which our build uses for the sample-memory functions, snatcherLeo's rule). Our sndPushGroup calls this under TARGET_PC and
 * then works on the copies exactly as it works on the disc data on the GameCube.
 *
 * Compiled with AxioDL's headers (it needs MusyPCGroupData); our runtime only sees the function below. The copies are
 * allocated with the PC layer's host allocator (sndPCSetHostAllocator; pc_audio.c sets the port's low-memory
 * allocator, so pointers stay below 4 GB). *Corrected 2026-10-16:* they used to go through MusyX's memory hooks
 * (salMalloc), which in the game are its own sndSetHooks allocator on the GS heap; the converted copies and the
 * converter's bookkeeping ran that heap out and later archive loads failed (the background music's data never
 * arrived).
 */
#include "hw_pc_assets.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> /* getenv: ORRE_SOUND_LOG */

/* Returns 1 and replaces *project, *pool and *sdir with the decoded copies; 0 if the data is invalid. */
u32 pc_musyx_decode_group(void** project, void** pool, void** sdir, void* samples) {
    SND_PC_GROUP_ASSETS raw = {{*project, SIZE_MAX}, {*pool, SIZE_MAX}, {*sdir, SIZE_MAX}, {samples, SIZE_MAX}};
    MusyPCGroupData decoded;
    SND_PC_ASSET_ERROR error = {0};

    if (!salPCDecodeGroup(&raw, &decoded, &error)) {
        /* the game only logs "can't open WAVE File"; say which section the PC decoder rejected and why */
        fprintf(stderr, "[musyx] group decode failed: %s at offset 0x%zx: %s\n", error.section ? error.section : "?",
                error.offset, error.reason ? error.reason : "?");
        return 0;
    }
    *project = decoded.project;
    *pool = decoded.pool;
    *sdir = decoded.directory;
    return 1;
}

/* Sound diagnostics (B, 2026-10-17, for the on-and-off cries in Willie's battle): which groups the game pushes, and
 * why an effect does not start. The game itself only logs "GSsndGetStatus:Forced termination SE=<id>" later, when it
 * finds the effect's voice gone. One line per push; one line per effect id that fails (the first time only). */
/* Set by the game exe (pc_game_main.c): told about every group pushed, e.g. so a checkpoint load can wait for the
 * title screen's sound groups. NULL in the other programs that link this library. */
void (*pc_musyx_push_listener)(unsigned gid);

/* The sound split prototype (2026-11-16; docs/port/sound-split.md) compiles this file a second time into the audible
 * MusyX copy (ORRE_MUSYX_AUDIBLE_COPY). That copy is given every command the game gives the game instance, so its
 * diagnostics would repeat the game instance's lines one for one and double what the log tools count; it keeps only a
 * failed group decode, tagged. Platforms: the same everywhere. */
#ifdef ORRE_MUSYX_AUDIBLE_COPY
#define PC_MUSYX_NOTES 0
#else
#define PC_MUSYX_NOTES 1
#endif

void pc_musyx_note_push(u16 gid, u32 ok, int stack, int fxGroups) {
    if (!PC_MUSYX_NOTES) {
        if (!ok) {
            fprintf(stderr, "[sound_split] the audible copy could not push group %u\n", gid);
        }
        return;
    }
    fprintf(stderr, "[musyx] push group %u -> %s (stack %d, effect groups %d)\n", gid, ok ? "ok" : "FAILED", stack,
            fxGroups);
    if (ok && pc_musyx_push_listener != NULL) {
        pc_musyx_push_listener(gid);
    }
}

/* sndOutputMode's argument (0 mono, 1 stereo, 2 surround, 3 Dolby Pro Logic II): the game's soundSetOutputMode calls it with
 * the mode left in r3 on the GameCube; logged to see what the PC passes */
void pc_musyx_note_output_mode(u32 mode) {
    static const char* const names[] = {"mono", "stereo", "surround", "Dolby Pro Logic II"};
    if (!PC_MUSYX_NOTES) {
        return;
    }
    fprintf(stderr, "[musyx] output mode %u (%s)\n", (unsigned)mode, mode < 4 ? names[mode] : "not a mode");
}

/* ReverbHICreate's settings (the reverb is refused, and stays silent, if any is out of its range) */
void pc_musyx_note_reverb(const char* what, double coloration, double time, double mix, double damping,
                          double preDelay, double crosstalk) {
    /* the same single-precision tests as ReverbHICreate (0.1f is accepted as a pre-delay: in double it is above 0.1) */
    const float c = (float)coloration, t = (float)time, m = (float)mix, d = (float)damping, p = (float)preDelay,
                x = (float)crosstalk;
    const int ok = c >= 0.f && c <= 1.f && t >= 0.01f && t <= 10.f && m >= 0.f && m <= 1.f && x >= 0.f && x <= 1.f &&
                   d >= 0.f && d <= 1.f && p >= 0.f && p <= 0.1f;
    if (!PC_MUSYX_NOTES) {
        return;
    }
    fprintf(stderr, "[musyx] reverb %s: coloration %g time %g mix %g damping %g preDelay %g crosstalk %g -> %s\n",
            what, coloration, time, mix, damping, preDelay, crosstalk, ok ? "accepted" : "REFUSED (out of range)");
}

/* ORRE_SOUND_LOG=1 (2026-10-18, per-sound level comparisons with Dolphin): each effect the game starts and each
 * volume change, as the game asked for it, with the mixer time (the same clock as hw_pc.c's "[sndvoice]" lines and
 * the audio dump's "dump starts at mixer t="). vol / pan are 0..127 (64 = centre); vid 0xFFFFFFFF = did not start */
static int sound_log(void) {
    static int on = -1;
    if (on < 0) {
        on = PC_MUSYX_NOTES && getenv("ORRE_SOUND_LOG") != NULL;
    }
    return on;
}
extern double salPCCurrentSeconds(void); /* hw_pc.c */

void pc_musyx_note_fx_start(u16 fid, u8 vol, u8 pan, u8 studio, u32 vid) {
    if (sound_log()) {
        fprintf(stderr, "[sndfx] t=%.4f fx=%u vol=%u pan=%u studio=%u vid=0x%X\n", salPCCurrentSeconds(), fid, vol, pan,
                studio, vid);
    }
}

void pc_musyx_note_volume(const char* what, u8 volume, u16 time, u8 a, u8 b) {
    if (sound_log()) {
        fprintf(stderr, "[sndvol] t=%.4f %s volume=%u time=%u args=%u/%u\n", salPCCurrentSeconds(), what, volume, time,
                a, b);
    }
}

void pc_musyx_note_fx_fail(u16 fid, int known) {
    static uint8_t seen[65536 / 8];
    if (!PC_MUSYX_NOTES || (seen[fid >> 3] & (1u << (fid & 7)))) {
        return;
    }
    seen[fid >> 3] |= (uint8_t)(1u << (fid & 7));
    fprintf(stderr, "[musyx] effect %u did not start: %s\n", fid,
            known ? "no voice (all voices busy at its priority, or its macro is missing)"
                  : "not in any pushed group (its sound group is not loaded)");
}
