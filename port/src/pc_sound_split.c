/*
 * pc_sound_split.c: the sound split (snatcherLeo, 2026-11-16: "Is there a way we could get option 2? ... Can we just see
 * (experiment) if it is possible?"; then, after playing it, "this is it! ... We will always use "ring""). Built as an
 * experiment behind ORRE_SOUND_SPLIT=1; *the normal path since 2026-11-16 (the Speed settings' build)*: it runs in every
 * live game on the replay clock, idle at 1x, and Settings › Enhancements › Speed › "Music when faster" picks what is
 * heard (pc_sound_split_set_speed). docs/port/sound-split.md has the whole story, the switches and what was measured.
 *
 * The problem: at 2x / 3x game speed with "Music and sound: Normal speed", the mixer renders fewer 5 ms blocks per
 * game frame (pc_audio.c, s_block_div), so every game wait on "has this sound ended?" (a move's sound entries, the
 * fanfares, the message box's sound wait, the cries...) lasts more game frames: moves and status effects linger, and
 * the rolls after them differ from the console's. With the sound following the game's speed nothing lingers, but the
 * music plays fast.
 *
 * The idea: two MusyX instances.
 *   - The game's own one (liborre_musyx.a, unchanged; here "the game instance") keeps the console's exact 1x schedule
 *     of blocks per game frame, so every answer the game gets (sndFXCheck, sndSeqGetValid, the voice-ended messages,
 *     the movies' stream callbacks, the voice budget's cuts) is the one it gets at 1x. It is heard only in part (the
 *     per-voice output gain, hw_pc.c sndPCSetVoiceGain): the movies' sound always, and the sound effects in the "fast"
 *     mode, played at the game's speed so they stay with the picture.
 *   - A second copy (liborre_musyx_aud.a: the same sources with every global renamed aud_*, port/tools/
 *     musyx_prefix.py; "the audible copy") gets the same commands and renders on the real-time schedule (the blocks
 *     per frame divided by the game's speed): the music at its normal tempo. The game never asks it anything.
 *
 * How the commands reach both: the game exe is linked with --wrap for each MusyX call the game makes that changes
 * the sound (pc_sound_split.wrap, read by port/tools/firstlink.py), so the game's call reaches __wrap_<name> here,
 * which calls the game instance (__real_<name>) exactly as before and then, with the split on, the audible copy.
 * Calls that only ask (sndFXCheck, sndSeqGetValid, sndCheckEmitter) and the movies' streams go to the game instance
 * only. Voice and song ids differ between the two (the copies allocate at different times), so each id the game gets
 * is mapped to the audible copy's. Structures the game hands MusyX to keep (3D emitters and listeners, the reverb's
 * work area) get a copy of their own for the audible copy, since MusyX writes into them.
 *
 * When it runs (pc_sound_split_open): every live game on the replay clock (the portable layout); ORRE_SOUND_SPLIT=0 / 1
 * overrides it for a test; an automated run (recordings, replays) has it only when ORRE_TEST_SPEED asks for a speed
 * above 1x, so 1x replays and their audio dumps stay exactly as before. Without the replay clock (a development build
 * outside the portable layout: the device's render thread mixes) it can't run, and "Normal tempo" falls back to the
 * old way (the game's waits on sounds linger at speed).
 * Off: every wrapper only calls the game instance with the same arguments and returns its answer, so the game behaves
 * exactly as without the wrappers (verified: see sound-split.md, the 1x replay).
 *
 * At 1x it is idle, not off: the audible copy keeps rendering the same blocks per frame as the game instance (the
 * same schedule), so the music heard is the console's mix (verified to 0.1 dB per window on P01 to frame 14,000), and
 * a speed change, including the fast-forward key's, needs no restart and makes no jump in the music. Why always
 * running rather than started on demand: the copy must have heard every command since the game's sndInit (the songs
 * playing, the groups pushed, the reverb), so it can't join a game half-way.
 *
 * Platforms: plain C over the two MusyX copies; the same on Windows, Linux and macOS. The --wrap link option is GNU
 * ld's (MinGW on Windows, Linux); macOS's linker has no --wrap (guess: its -alias or a renamed object would do it).
 */
#include "dolphin/types.h"
#include "musyx/runtime/reverb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_lowmem.h"

/* ---- the two MusyX instances' entry points (our runtime's definitions' exact signatures, so the wrappers pass on
 * whatever the game's call put in the registers, as the real function would read it) ---- */
#define DECL2(ret, name, params)  \
    ret __real_##name params;     \
    ret aud_##name params;
DECL2(s32, sndInit, (u8 voices, u8 music, u8 sfx, u8 studios, u32 flags, u32 aramSize))
DECL2(void, sndQuit, (void))
DECL2(void, sndSetHooks, (void* hooks))
DECL2(u32, sndPushGroup, (void* prj, u16 gid, void* samples, void* sdir, void* pool))
DECL2(void, sndSetSampleDataUploadCallback, (void* (*callback)(u32, u32), u32 chunkSize))
DECL2(void, sndSetAuxProcessingCallbacks, (u8 studio, void* auxA, void* userA, u8 midiA, u32 seqA, void* auxB,
                                           void* userB, u8 midiB, u32 seqB))
DECL2(void, sndAuxCallbackPrepareReverbHI, (SND_AUX_REVERBHI * rev))
DECL2(u32, sndAuxCallbackUpdateSettingsReverbHI, (SND_AUX_REVERBHI * rev))
DECL2(void, sndOutputMode, (u32 mode))
DECL2(void, sndMasterVolume, (u8 volume, u16 time, u8 music, u8 fx))
DECL2(void, sndVolume, (u8 volume, u16 time, u8 group))
DECL2(u32, sndFXStartEx, (u16 fid, u8 vol, u8 pan, u8 studio))
DECL2(u32, sndFXCtrl, (u32 vid, u8 ctrl, u8 value))
DECL2(u32, sndFXKeyOff, (u32 vid))
DECL2(u32, sndSeqPlayEx, (u16 sgid, u16 sid, void* arrfile, void* para, u8 studio))
DECL2(void, sndSeqStop, (u32 seqId))
DECL2(void, sndSeqContinue, (u32 seqId))
DECL2(void, sndSeqVolume, (u8 volume, u16 time, u32 seqId, u8 mode))
DECL2(u32, sndAddEmitter, (void* em, void* pos, void* dir, f32 maxDis, f32 comp, u32 flags, u16 fxid, u8 maxVol,
                           u8 minVol, void* room))
DECL2(u32, s3dSetListener, (void* li, void* pos, void* dir, void* heading, void* up, f32 frontSur, f32 backSur,
                            f32 soundSpeed, u32 flags, u8 vol, void* room))
DECL2(u32, s3dUpdateListener, (void* li, void* pos, void* dir, void* heading, void* up, u8 vol, void* room))
#undef DECL2

/* asked of the game instance only (never wrapped) */
extern u32 sndFXCheck(u32 vid);
extern u32 sndSeqGetValid(u32 seqId);
extern void sndAuxCallbackReverbHI(u8 type, u32* data, u8* work);
/* the audible copy only */
extern void aud_sndAuxCallbackReverbHI(u8 type, u32* data, u8* work);
extern u32 aud_sndCheckEmitter(void* em);
extern u32 aud_sndSeqGetValid(u32 seqId);
extern u8 aud_sndIsInstalled(void);

/* the PC layer of each copy (AxioDL's hw_pc*.c; the game instance's through pc_audio.c) */
typedef struct {
    u32 mixRate, channels;
} SplitPcConfig; /* = SND_PC_CONFIG (musyx/pc.h) */
extern _Bool aud_sndPCSetSynchronization(void (*enter)(void), void (*leave)(void));
extern void aud_sndPCSetHostAllocator(void* (*alloc)(size_t), void (*release)(void*));
extern _Bool aud_sndPCOpenAudio(const SplitPcConfig* preferred, SplitPcConfig* obtained);
extern _Bool aud_sndPCStartAudio(void);
extern void aud_sndPCStopAudio(void);
extern void aud_sndPCSetManualRender(_Bool manual);
extern _Bool aud_sndPCRenderManual(u32 blocks);
extern void aud_sndPCSetPlaybackSpeed(float speed);
extern void aud_sndPCSetMasterGain(float gain);
extern f32 (*aud_OrreHook_SoundVolumeScale)(int isEffect);
extern void sndPCSetVoiceGain(float (*gain)(u32 voice)); /* the game instance's mixer */
extern int pc_musyx_voice_class(u32 voice);              /* the game instance's voices (musyx_pc_voiceclass.c) */

extern void pc_os_audio_enter(void);
extern void pc_os_audio_leave(void);
extern int pc_vi_virtual_clock(void);
extern u32 VIGetRetraceCount(void);
extern int pc_audio_rendering; /* pc_audio.c: set while the game instance mixes (its own calls are not the game's) */

/* ---- state ---- */
enum { FX_RING, FX_CUT, FX_FAST };
static const char* const kFxNames[] = {"ring", "cut", "fast"};
static int s_wanted;    /* ORRE_SOUND_SPLIT=1 and the audible copy's device opened (pc_audio_init) */
static int s_active;    /* the audible copy initialised (sndInit worked on both) */
static int s_started;   /* the audible copy's mixer started */
/* "ring" always (snatcherLeo, 2026-11-16, after playing it: "We will always use "ring""); "cut" and "fast" stay as test-only
 * values of ORRE_SOUND_SPLIT_FX, nothing in Settings */
static int s_fx = FX_RING;
static int s_depth;     /* inside a call to the game instance: MusyX's own calls to its API are not the game's */
static SplitPcConfig s_cfg;
/* "Music when faster: Faster too" (pc_speed.c): the game instance is heard whole, at the game's speed (the behaviour
 * before the split), and the audible copy is muted but keeps rendering in step with the game instance, so switching
 * back to "Normal tempo" picks the music up where the game has it. s_master: the player's master volume, kept for the
 * copy while it is muted */
static int s_muted;
static int s_master = 100;

/* ORRE_SOUND_SPLIT_LOG=1 (observe only): one "[sound_split]" line per song and volume command given to both copies,
 * with the frame-independent retrace count, to find a command one copy missed */
static int split_log(void) {
    static int on = -1;
    if (on < 0) {
        on = getenv("ORRE_SOUND_SPLIT_LOG") != NULL;
    }
    return on;
}
#define SPLIT_LOG(...)                                                                     \
    do {                                                                                   \
        if (split_log()) {                                                                 \
            printf("[sound_split] r%u ", (unsigned)VIGetRetraceCount());                  \
            printf(__VA_ARGS__);                                                           \
        }                                                                                  \
    } while (0)

/* fan out this call? Only the game's own calls, only once the copy runs */
static int fan_out(void) { return s_active && s_depth == 0 && !pc_audio_rendering; }

/* ---- id maps: the game instance's voice / song id -> the audible copy's ---- */
#define FX_MAP 256
#define SEQ_MAP 64
typedef struct {
    u32 game, aud;
    int used;
} IdPair;
static IdPair s_fx_map[FX_MAP], s_seq_map[SEQ_MAP];

static void map_put(IdPair* map, int n, u32 game, u32 aud) {
    int i, free_slot = -1;
    if (game == 0xFFFFFFFFu) {
        return;
    }
    for (i = 0; i < n; ++i) {
        if (map[i].used && map[i].game == game) { /* an id reused: the newest pairing wins */
            map[i].aud = aud;
            return;
        }
        if (!map[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) {
        static int warned;
        if (!warned++) {
            fprintf(stderr, "[sound_split] id map full (%d): the oldest pairing is dropped\n", n);
        }
        free_slot = (int)(game % (u32)n);
    }
    map[free_slot].game = game;
    map[free_slot].aud = aud;
    map[free_slot].used = 1;
}

/* the audible copy's id for a game-instance id: 0xFFFFFFFF passes through (the API's "no id"); an id the copy has no
 * pairing for (*found = 0): skip the call there (a song MusyX started by itself, an effect "fast" left to the game) */
static u32 map_get(const IdPair* map, int n, u32 game, int* found) {
    int i;
    *found = 1;
    if (game == 0xFFFFFFFFu) {
        return game;
    }
    for (i = 0; i < n; ++i) {
        if (map[i].used && map[i].game == game) {
            return map[i].aud;
        }
    }
    *found = 0;
    return 0xFFFFFFFFu;
}

/* ---- the game instance's per-voice output gain (hw_pc.c): what of it is heard ---- */
static float game_voice_gain(u32 voice) {
    switch (pc_musyx_voice_class(voice)) {
    case 2: /* a stream (the movies): paced by this instance, so heard from it, with the picture */
        return 1.f;
    case 1: /* a sound effect: heard from here in the "fast" mode (sped up with the game), else from the copy */
        return s_fx == FX_FAST ? 1.f : 0.f;
    default: /* music: the audible copy plays it at its normal tempo */
        return 0.f;
    }
}

/* ---- the audible copy's memory hooks: the PC's low memory, never the game's sound heap (doubling the game's
 * allocations there would run that heap out, as converted groups once did: musyx_pc_groups.c) ---- */
static void* aud_alloc(u32 len) { return pc_lowmem_alloc(len); }
static void aud_free(void* p) { pc_lowmem_free(p); }
static struct {
    void* (*malloc)(u32 len);
    void (*free)(void* addr);
} s_aud_hooks = {aud_alloc, aud_free}; /* = SND_HOOKS */

/* ---- the switches ---- */
extern int pc_platform_automated_run(void); /* pc_platform.c */

/* whether this run has the split, and why (for the log): see the file's head */
static int split_wanted(const char** why) {
    const char* on = getenv("ORRE_SOUND_SPLIT");
    if (on != NULL && *on != '\0') {
        *why = "ORRE_SOUND_SPLIT (a test override)";
        return strcmp(on, "0") != 0;
    }
    if (pc_platform_automated_run()) {
        /* recordings and replays: only for a sped-up test (pc_speed.c's ORRE_TEST_SPEED), so a 1x replay and its
         * ORRE_DUMP_AUDIO file stay the game instance's whole mix, as every earlier comparison was made */
        const char* speed = getenv("ORRE_TEST_SPEED");
        *why = "an automated run: only with ORRE_TEST_SPEED above 100 or ORRE_TEST_FF";
        return (speed != NULL && atoi(speed) > 100) || getenv("ORRE_TEST_FF") != NULL;
    }
    *why = "live play: the normal path (Settings › Enhancements › Speed)";
    return 1;
}

int pc_sound_split_open(void) {
    const char* fx = getenv("ORRE_SOUND_SPLIT_FX");
    const char* why = "";
    SplitPcConfig preferred = {32000, 2}, obtained;
    if (!split_wanted(&why)) {
        printf("[sound_split] off (%s)\n", why);
        return 0;
    }
    if (fx != NULL) { /* test only: "cut" and "fast" (sound-split.md); anything else is "ring" */
        s_fx = strcmp(fx, "cut") == 0 ? FX_CUT : strcmp(fx, "fast") == 0 ? FX_FAST : FX_RING;
    }
    /* the game instance must render per game frame (the replay clock: the portable layout, recordings, replays);
     * with the device's render thread its progress follows the sound card, not the game, and there is nothing to keep */
    if (!pc_vi_virtual_clock()) {
        fprintf(stderr, "[sound_split] needs the replay clock (the portable layout or a replay): off; \"Music when "
                        "faster: Normal tempo\" then lets the game's waits on sounds linger at speed\n");
        return 0;
    }
    aud_sndPCSetSynchronization(pc_os_audio_enter, pc_os_audio_leave); /* one lock for both copies */
    aud_sndPCSetHostAllocator(pc_lowmem_alloc, pc_lowmem_free);
    if (!aud_sndPCOpenAudio(&preferred, &obtained)) {
        fprintf(stderr, "[sound_split] the audible copy cannot open an output: off\n");
        return 0;
    }
    s_cfg = obtained;
    sndPCSetVoiceGain(game_voice_gain);
    s_wanted = 1;
    printf("[sound_split] on (%s): the game instance keeps the 1x schedule, the audible copy plays the music at normal "
           "tempo; sound effects: %s\n", why, kFxNames[s_fx]);
    return 1;
}

/* the audible copy's output gain: the player's master volume, or 0 while "Faster too" mutes it */
static void apply_copy_gain(void) { aud_sndPCSetMasterGain(s_muted ? 0.f : s_master / 100.f); }

/* ---- the audible copy's own schedule: the same formula as pc_audio.c's render_due_blocks, with its own divider ---- */
static u32 s_div = 100, s_from;
static u64 s_base, s_blocks;

static void rebase(void) {
    s_base = s_blocks;
    s_from = VIGetRetraceCount();
}

/* the audible copy failed to set up or start: the game instance is heard whole for the rest of the run */
static int s_failed;

/* pc_audio_set_speed, whenever the game's speed or "Music when faster" changes (pc_speed.c; live, no restart):
 *   - "Normal tempo" (music_follows 0): the copy renders on the real-time schedule (the blocks per frame divided by
 *     the game's speed) and is what is heard, the game instance only for the movies;
 *   - "Faster too" (music_follows 1): the game instance is heard whole at the game's speed, as before the split; the
 *     copy is muted and renders the game instance's own blocks per frame, so it stays in step with it.
 * The game instance's schedule never changes (pc_audio.c keeps it at the console's 1x blocks per frame either way), so
 * neither choice changes what the game is told: the rolls are the console's in both (Normal tempo verified by the
 * x3ring pass; Faster too follows from the same construction, likely until its own pass). */
void pc_sound_split_set_speed(unsigned game_percent, int music_follows) {
    const u32 div = music_follows ? 100u : (game_percent < 100u ? 100u : game_percent);
    if (!s_wanted) {
        return;
    }
    aud_sndPCSetPlaybackSpeed(music_follows ? (float)game_percent / 100.0f : 1.0f);
    if (div != s_div) {
        if (s_started) {
            rebase();
        }
        s_div = div;
    }
    if (music_follows != s_muted) {
        s_muted = music_follows;
        apply_copy_gain();
        /* the game instance: heard whole while the copy is muted; else only its movies (game_voice_gain) */
        sndPCSetVoiceGain(s_muted || s_failed ? NULL : game_voice_gain);
        printf("[sound_split] heard now: %s\n", s_muted ? "the game instance, at the game's speed (Faster too)"
                                                        : "the audible copy at normal tempo (Normal tempo)");
    }
}

void pc_sound_split_set_volumes(int master, f32 (*scale)(int is_effect)) {
    if (!s_wanted) {
        return;
    }
    s_master = master;
    apply_copy_gain();
    aud_OrreHook_SoundVolumeScale = scale;
}

/* the retrace interrupt (pc_audio.c start_when_installed), after the game instance's blocks */
static void split_retrace_checks(void);
void pc_sound_split_retrace(void) {
    if (!s_wanted) {
        return;
    }
    if (!s_started) {
        if (s_active && aud_sndIsInstalled()) {
            aud_sndPCSetManualRender(1);
            s_started = aud_sndPCStartAudio();
            s_from = VIGetRetraceCount();
            s_base = s_blocks = 0;
            if (!s_started) {
                fprintf(stderr, "[sound_split] the audible copy cannot start its output: off\n");
                s_active = 0;
                s_failed = 1;
                sndPCSetVoiceGain(NULL);
            } else {
                printf("[sound_split] the audible copy's mixer started at retrace %u\n", (unsigned)s_from);
            }
        }
        return;
    }
    {
        const u64 fields = (u64)(VIGetRetraceCount() - s_from);
        const u64 rate = s_cfg.mixRate == 32000 ? 32028 : s_cfg.mixRate;
        const u64 block = (s_cfg.mixRate + 199) / 200;
        const u64 due = s_base + fields * rate * 1001 * 100 / ((u64)60000 * block * s_div);
        if (due > s_blocks) {
            aud_sndPCRenderManual((u32)(due - s_blocks));
            s_blocks = due;
        }
    }
    split_retrace_checks();
}

void pc_sound_split_stop(void) {
    if (s_wanted) {
        aud_sndPCStopAudio();
    }
    s_wanted = s_active = s_started = 0;
}

/* ---- copies of what the game hands MusyX to keep ---- */
/* 3D emitters and listeners: MusyX links them into its own lists and writes into them, so the audible copy gets its own
 * zeroed block (its add function fills every field it uses). 256 bytes: more than either structure on a 64-bit PC
 * (snd3d.c: SND_LISTENER about 160, SND_EMITTER about 100). */
#define COPY_BYTES 256
#define EMITTERS 64
#define LISTENERS 16
typedef struct {
    void* game;
    void* copy;
} Copy;
static Copy s_emitters[EMITTERS], s_listeners[LISTENERS];

static void* listener_copy(void* game, int create) {
    int i, free_slot = -1;
    for (i = 0; i < LISTENERS; ++i) {
        if (s_listeners[i].copy != NULL && s_listeners[i].game == game) {
            return s_listeners[i].copy;
        }
        if (s_listeners[i].copy == NULL && free_slot < 0) {
            free_slot = i;
        }
    }
    if (!create || free_slot < 0) {
        return NULL;
    }
    s_listeners[free_slot].game = game;
    s_listeners[free_slot].copy = pc_lowmem_alloc(COPY_BYTES);
    return s_listeners[free_slot].copy;
}

/* a free emitter block: never one the audible copy still has in its list (in the "ring" mode an emitter can still
 * play there after the game instance's ended and the game reused its own block) */
static void* emitter_copy(void* game) {
    int i;
    for (i = 0; i < EMITTERS; ++i) {
        if (s_emitters[i].copy == NULL) {
            s_emitters[i].copy = pc_lowmem_alloc(COPY_BYTES);
        }
        if (s_emitters[i].copy != NULL && (s_emitters[i].game == NULL || !aud_sndCheckEmitter(s_emitters[i].copy))) {
            s_emitters[i].game = game;
            memset(s_emitters[i].copy, 0, COPY_BYTES);
            return s_emitters[i].copy;
        }
    }
    return NULL;
}

/* the reverb: the game's work area holds the delay lines the game instance allocated; the copy gets its own, with the
 * game's settings (the fields after the work area) copied over before each prepare / update */
#define REVERBS 4
typedef struct {
    SND_AUX_REVERBHI* game;
    SND_AUX_REVERBHI* copy;
} ReverbCopy;
static ReverbCopy s_reverbs[REVERBS];

static SND_AUX_REVERBHI* reverb_copy(SND_AUX_REVERBHI* game, int create) {
    int i;
    for (i = 0; i < REVERBS; ++i) {
        if (s_reverbs[i].game == game && s_reverbs[i].copy != NULL) {
            return s_reverbs[i].copy;
        }
    }
    if (!create) {
        return NULL;
    }
    for (i = 0; i < REVERBS; ++i) {
        if (s_reverbs[i].copy == NULL) {
            s_reverbs[i].game = game;
            s_reverbs[i].copy = (SND_AUX_REVERBHI*)pc_lowmem_alloc(sizeof(SND_AUX_REVERBHI));
            return s_reverbs[i].copy;
        }
    }
    return NULL;
}

static void reverb_settings(SND_AUX_REVERBHI* to, const SND_AUX_REVERBHI* from) {
    to->tempDisableFX = from->tempDisableFX;
    to->coloration = from->coloration;
    to->mix = from->mix;
    to->time = from->time;
    to->damping = from->damping;
    to->preDelay = from->preDelay;
    to->crosstalk = from->crosstalk;
}

/* sample uploads from the disc (soundPushWave's streamed groups): the game instance's push reads each chunk from the
 * disc through the game's callback; the chunks are kept and handed to the audible copy's push, so the disc is read
 * once, as on the console (a second read could change the disc model's timing, and with it the game's frames) */
typedef struct {
    u32 offset, length;
    void* data;
} Chunk;
static void* (*s_game_upload)(u32 offset, u32 length);
static Chunk* s_chunks;
static int s_nchunks, s_capchunks;

static void* upload_record(u32 offset, u32 length) {
    void* data = s_game_upload(offset, length);
    if (data != NULL && s_wanted) {
        if (s_nchunks == s_capchunks) {
            const int cap = s_capchunks ? s_capchunks * 2 : 64;
            Chunk* grown = (Chunk*)realloc(s_chunks, (size_t)cap * sizeof(Chunk));
            if (grown == NULL) {
                return data;
            }
            s_chunks = grown;
            s_capchunks = cap;
        }
        s_chunks[s_nchunks].offset = offset;
        s_chunks[s_nchunks].length = length;
        s_chunks[s_nchunks].data = pc_lowmem_alloc(length); /* below 4 GB, as the game's own buffer is */
        if (s_chunks[s_nchunks].data != NULL) {
            memcpy(s_chunks[s_nchunks].data, data, length);
            ++s_nchunks;
        }
    }
    return data;
}

static void* upload_replay(u32 offset, u32 length) {
    int i;
    for (i = 0; i < s_nchunks; ++i) {
        if (s_chunks[i].offset == offset && s_chunks[i].length == length) {
            return s_chunks[i].data;
        }
    }
    {
        static int warned;
        if (!warned++) {
            fprintf(stderr, "[sound_split] the audible copy asked for a sample chunk the game instance did not read "
                            "(offset 0x%X, %u bytes): read from the disc\n", (unsigned)offset, (unsigned)length);
        }
    }
    return s_game_upload != NULL ? s_game_upload(offset, length) : NULL;
}

static void chunks_free(void) {
    int i;
    for (i = 0; i < s_nchunks; ++i) {
        pc_lowmem_free(s_chunks[i].data);
    }
    s_nchunks = 0;
}

/* ---- per retrace: what the game instance has finished ---- */
static void split_retrace_checks(void) {
    int i;
    for (i = 0; i < FX_MAP; ++i) {
        IdPair* p = &s_fx_map[i];
        if (p->used && sndFXCheck(p->game) == 0xFFFFFFFFu) {
            /* the game instance's effect has ended: the game asks no more about it. "cut": end the audible copy's too
             * (volume to 0, then key off: a looping or held effect also stops); "ring": let it play out */
            if (s_fx == FX_CUT && p->aud != 0xFFFFFFFFu) {
                aud_sndFXCtrl(p->aud, 7, 0);
                aud_sndFXKeyOff(p->aud);
            }
            p->used = 0;
        }
    }
    for (i = 0; i < SEQ_MAP; ++i) {
        IdPair* p = &s_seq_map[i];
        /* A song (a jingle, a fanfare) that ended in the game instance still plays on in the copy at its tempo; the
         * pairing is kept until the copy's has ended too, so the game's later stop or fade for that id still reaches
         * the copy and cuts it when the game moves on. *Corrected 2026-11-16 (first 3x run):* the pairing used to be
         * dropped when the game instance's song ended; the game's stop then found no pairing, and on P01 a jingle
         * (song 64) went on under the next two field songs, 8-12 dB louder than the console's mix for about 20 s. */
        if (p->used && sndSeqGetValid(p->game) == 0 && (p->aud == 0xFFFFFFFFu || aud_sndSeqGetValid(p->aud) == 0)) {
            SPLIT_LOG("song 0x%X ended in both (copy 0x%X)\n", (unsigned)p->game, (unsigned)p->aud);
            p->used = 0;
        }
    }
}

/* ---- the wrappers (pc_sound_split.wrap lists them; port/tools/firstlink.py links the game with --wrap). Each calls
 * the game instance first, with s_depth raised so MusyX's calls to its own API inside it are not fanned out again (the
 * copy makes those itself), then the audible copy. ---- */

s32 __wrap_sndInit(u8 voices, u8 music, u8 sfx, u8 studios, u32 flags, u32 aramSize) {
    const int fan = s_wanted && s_depth == 0;
    s32 r;
    ++s_depth;
    r = __real_sndInit(voices, music, sfx, studios, flags, aramSize);
    --s_depth;
    if (fan && r == 0) {
        if (aud_sndInit(voices, music, sfx, studios, flags, aramSize) == 0) {
            s_active = 1;
            printf("[sound_split] the audible copy is set up (%u voices)\n", (unsigned)voices);
        } else {
            fprintf(stderr, "[sound_split] the audible copy's sndInit failed: off\n");
            s_failed = 1;
            sndPCSetVoiceGain(NULL); /* the game instance is heard whole again (its music then follows the game) */
        }
    }
    return r;
}

void __wrap_sndQuit(void) {
    const int fan = fan_out();
    ++s_depth;
    __real_sndQuit();
    --s_depth;
    if (fan) {
        aud_sndQuit();
        s_active = 0;
        memset(s_fx_map, 0, sizeof(s_fx_map));
        memset(s_seq_map, 0, sizeof(s_seq_map));
    }
}

void __wrap_sndSetHooks(void* hooks) {
    ++s_depth;
    __real_sndSetHooks(hooks);
    --s_depth;
    if (s_wanted) { /* before sndInit, so not fan_out(): the copy's own allocator, whatever the game's */
        aud_sndSetHooks(&s_aud_hooks);
    }
}

void __wrap_sndSetSampleDataUploadCallback(void* (*callback)(u32, u32), u32 chunkSize) {
    if (!s_wanted) {
        ++s_depth;
        __real_sndSetSampleDataUploadCallback(callback, chunkSize);
        --s_depth;
        return;
    }
    s_game_upload = callback;
    ++s_depth;
    __real_sndSetSampleDataUploadCallback(callback != NULL ? upload_record : NULL, chunkSize);
    --s_depth;
    if (s_active) {
        aud_sndSetSampleDataUploadCallback(callback != NULL ? upload_replay : NULL, chunkSize);
    }
}

u32 __wrap_sndPushGroup(void* prj, u16 gid, void* samples, void* sdir, void* pool) {
    const int fan = fan_out();
    u32 r;
    ++s_depth;
    r = __real_sndPushGroup(prj, gid, samples, sdir, pool);
    --s_depth;
    if (fan && r) {
        /* the copy decodes its own native copies of the group (musyx_pc_groups.c), so nothing it writes is shared */
        if (!aud_sndPushGroup(prj, gid, samples, sdir, pool)) {
            fprintf(stderr, "[sound_split] the audible copy could not push group %u\n", (unsigned)gid);
        }
    }
    chunks_free();
    return r;
}

void __wrap_sndSetAuxProcessingCallbacks(u8 studio, void* auxA, void* userA, u8 midiA, u32 seqA, void* auxB,
                                         void* userB, u8 midiB, u32 seqB) {
    const int fan = fan_out();
    ++s_depth;
    __real_sndSetAuxProcessingCallbacks(studio, auxA, userA, midiA, seqA, auxB, userB, midiB, seqB);
    --s_depth;
    if (fan) {
        /* the game's callbacks are the game instance's functions with its work areas: the copy's own instead (the
         * game only ever uses the high-quality reverb; anything else stays off in the copy, with a note) */
        void* a = NULL;
        void* ua = NULL;
        void* b = NULL;
        void* ub = NULL;
        int found;
        if (auxA == (void*)sndAuxCallbackReverbHI) {
            a = (void*)aud_sndAuxCallbackReverbHI;
            ua = reverb_copy((SND_AUX_REVERBHI*)userA, 0);
        } else if (auxA != NULL) {
            fprintf(stderr, "[sound_split] aux A callback %p not known: off in the audible copy\n", auxA);
        }
        if (auxB == (void*)sndAuxCallbackReverbHI) {
            b = (void*)aud_sndAuxCallbackReverbHI;
            ub = reverb_copy((SND_AUX_REVERBHI*)userB, 0);
        } else if (auxB != NULL) {
            fprintf(stderr, "[sound_split] aux B callback %p not known: off in the audible copy\n", auxB);
        }
        if (a != NULL && ua == NULL) {
            a = NULL;
        }
        if (b != NULL && ub == NULL) {
            b = NULL;
        }
        aud_sndSetAuxProcessingCallbacks(studio, a, ua, midiA, map_get(s_seq_map, SEQ_MAP, seqA, &found), b, ub, midiB,
                                         map_get(s_seq_map, SEQ_MAP, seqB, &found));
    }
}

void __wrap_sndAuxCallbackPrepareReverbHI(SND_AUX_REVERBHI* rev) {
    const int fan = fan_out();
    SND_AUX_REVERBHI* copy;
    ++s_depth;
    __real_sndAuxCallbackPrepareReverbHI(rev);
    --s_depth;
    if (fan && (copy = reverb_copy(rev, 1)) != NULL) {
        reverb_settings(copy, rev);
        aud_sndAuxCallbackPrepareReverbHI(copy);
    }
}

u32 __wrap_sndAuxCallbackUpdateSettingsReverbHI(SND_AUX_REVERBHI* rev) {
    const int fan = fan_out();
    SND_AUX_REVERBHI* copy;
    u32 r;
    ++s_depth;
    r = __real_sndAuxCallbackUpdateSettingsReverbHI(rev);
    --s_depth;
    if (fan && (copy = reverb_copy(rev, 0)) != NULL) {
        reverb_settings(copy, rev);
        aud_sndAuxCallbackUpdateSettingsReverbHI(copy);
    }
    return r;
}

void __wrap_sndOutputMode(u32 mode) {
    const int fan = fan_out();
    ++s_depth;
    __real_sndOutputMode(mode);
    --s_depth;
    if (fan) {
        aud_sndOutputMode(mode);
    }
}

void __wrap_sndMasterVolume(u8 volume, u16 time, u8 music, u8 fx) {
    const int fan = fan_out();
    ++s_depth;
    __real_sndMasterVolume(volume, time, music, fx);
    --s_depth;
    SPLIT_LOG("master volume %u time %u music %u fx %u%s\n", volume, time, music, fx, fan ? "" : " (game only)");
    if (fan) {
        aud_sndMasterVolume(volume, time, music, fx);
    }
}

void __wrap_sndVolume(u8 volume, u16 time, u8 group) {
    const int fan = fan_out();
    ++s_depth;
    __real_sndVolume(volume, time, group);
    --s_depth;
    SPLIT_LOG("group volume %u time %u group %u%s\n", volume, time, group, fan ? "" : " (game only)");
    if (fan) {
        aud_sndVolume(volume, time, group);
    }
}

u32 __wrap_sndFXStartEx(u16 fid, u8 vol, u8 pan, u8 studio) {
    const int fan = fan_out();
    u32 r;
    ++s_depth;
    r = __real_sndFXStartEx(fid, vol, pan, studio);
    --s_depth;
    /* "fast": the effects are heard from the game instance (with the picture), so the copy never starts them and its
     * voices are all the music's */
    if (fan && s_fx != FX_FAST && r != 0xFFFFFFFFu) {
        map_put(s_fx_map, FX_MAP, r, aud_sndFXStartEx(fid, vol, pan, studio));
    }
    return r;
}

u32 __wrap_sndFXCtrl(u32 vid, u8 ctrl, u8 value) {
    const int fan = fan_out();
    u32 r;
    int found;
    ++s_depth;
    r = __real_sndFXCtrl(vid, ctrl, value);
    --s_depth;
    if (fan) {
        const u32 a = map_get(s_fx_map, FX_MAP, vid, &found);
        if (found && a != 0xFFFFFFFFu) {
            aud_sndFXCtrl(a, ctrl, value);
        }
    }
    return r;
}

u32 __wrap_sndFXKeyOff(u32 vid) {
    const int fan = fan_out();
    u32 r;
    int found;
    ++s_depth;
    r = __real_sndFXKeyOff(vid);
    --s_depth;
    if (fan) {
        const u32 a = map_get(s_fx_map, FX_MAP, vid, &found);
        if (found && a != 0xFFFFFFFFu) {
            aud_sndFXKeyOff(a);
        }
    }
    return r;
}

u32 __wrap_sndSeqPlayEx(u16 sgid, u16 sid, void* arrfile, void* para, u8 studio) {
    const int fan = fan_out();
    u32 r;
    ++s_depth;
    r = __real_sndSeqPlayEx(sgid, sid, arrfile, para, studio);
    --s_depth;
    if (fan && r != 0xFFFFFFFFu) {
        const u32 a = aud_sndSeqPlayEx(sgid, sid, arrfile, para, studio);
        SPLIT_LOG("song play group %u song %u -> game 0x%X, copy 0x%X\n", (unsigned)sgid, (unsigned)sid, (unsigned)r,
                  (unsigned)a);
        map_put(s_seq_map, SEQ_MAP, r, a);
    }
    return r;
}

void __wrap_sndSeqStop(u32 seqId) {
    const int fan = fan_out();
    int found;
    ++s_depth;
    __real_sndSeqStop(seqId);
    --s_depth;
    if (fan) {
        const u32 a = map_get(s_seq_map, SEQ_MAP, seqId, &found);
        SPLIT_LOG("song stop game 0x%X -> copy %s0x%X\n", (unsigned)seqId, found ? "" : "NONE ", (unsigned)a);
        if (found) {
            aud_sndSeqStop(a);
        }
    }
}

void __wrap_sndSeqContinue(u32 seqId) {
    const int fan = fan_out();
    int found;
    ++s_depth;
    __real_sndSeqContinue(seqId);
    --s_depth;
    if (fan) {
        const u32 a = map_get(s_seq_map, SEQ_MAP, seqId, &found);
        SPLIT_LOG("song continue game 0x%X -> copy %s0x%X\n", (unsigned)seqId, found ? "" : "NONE ", (unsigned)a);
        if (found) {
            aud_sndSeqContinue(a);
        }
    }
}

void __wrap_sndSeqVolume(u8 volume, u16 time, u32 seqId, u8 mode) {
    const int fan = fan_out();
    int found;
    ++s_depth;
    __real_sndSeqVolume(volume, time, seqId, mode);
    --s_depth;
    if (fan) {
        const u32 a = map_get(s_seq_map, SEQ_MAP, seqId, &found);
        SPLIT_LOG("song volume %u time %u mode %u game 0x%X -> copy %s0x%X\n", volume, time, mode, (unsigned)seqId,
                  found ? "" : "NONE ", (unsigned)a);
        if (found) {
            aud_sndSeqVolume(volume, time, a, mode);
        }
    }
}

u32 __wrap_sndAddEmitter(void* em, void* pos, void* dir, f32 maxDis, f32 comp, u32 flags, u16 fxid, u8 maxVol,
                         u8 minVol, void* room) {
    const int fan = fan_out();
    u32 r;
    ++s_depth;
    r = __real_sndAddEmitter(em, pos, dir, maxDis, comp, flags, fxid, maxVol, minVol, room);
    --s_depth;
    if (fan && s_fx != FX_FAST && em != NULL) { /* an emitter is a sound effect ("fast": heard from the game instance) */
        void* copy = emitter_copy(em);
        if (copy != NULL && room == NULL) { /* the game passes no room; a room would be shared state */
            aud_sndAddEmitter(copy, pos, dir, maxDis, comp, flags, fxid, maxVol, minVol, NULL);
        }
    }
    return r;
}

u32 __wrap_s3dSetListener(void* li, void* pos, void* dir, void* heading, void* up, f32 frontSur, f32 backSur,
                          f32 soundSpeed, u32 flags, u8 vol, void* room) {
    const int fan = fan_out();
    u32 r;
    ++s_depth;
    r = __real_s3dSetListener(li, pos, dir, heading, up, frontSur, backSur, soundSpeed, flags, vol, room);
    --s_depth;
    if (fan && room == NULL) {
        void* copy = listener_copy(li, 1);
        if (copy != NULL) {
            aud_s3dSetListener(copy, pos, dir, heading, up, frontSur, backSur, soundSpeed, flags, vol, NULL);
        }
    }
    return r;
}

u32 __wrap_s3dUpdateListener(void* li, void* pos, void* dir, void* heading, void* up, u8 vol, void* room) {
    const int fan = fan_out();
    u32 r;
    ++s_depth;
    r = __real_s3dUpdateListener(li, pos, dir, heading, up, vol, room);
    --s_depth;
    if (fan && room == NULL) {
        void* copy = listener_copy(li, 0);
        if (copy != NULL) {
            aud_s3dUpdateListener(copy, pos, dir, heading, up, vol, NULL);
        }
    }
    return r;
}
