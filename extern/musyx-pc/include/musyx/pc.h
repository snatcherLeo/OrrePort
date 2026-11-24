#ifndef MUSYX_PC_H
#define MUSYX_PC_H

#include "musyx/musyx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single-instance PC renderer. Configure before sndInit; configuration persists
 * across sndQuit. sndInit never opens a device or creates a thread.
 * Supported rates: 8000 through 96000 Hz inclusive (integer Hz).
 * Interleaved signed 16-bit PCM. Channels select the speaker layout:
 * 2: FL FR; 4: FL FR BL BR; 6: FL FR FC LFE BL BR;
 * 8: FL FR FC LFE BL BR SL SR. Center/LFE are silent; in 7.1 the rear
 * directions split equally between back/side at -3 dB each.
 * Legacy effects process three channels at 32000 Hz. */
typedef struct SND_PC_CONFIG {
  u32 mixRate;
  u32 channels;
} SND_PC_CONFIG;

typedef struct SND_PC_RENDER_INFO {
  u64 outputFrames;
  u64 renderedFrames;
  u64 controlTicks;
  u32 mixRate;
  u32 channels;
} SND_PC_RENDER_INFO;

bool sndPCConfigure(const SND_PC_CONFIG* config);
/* Optional host-wide recursive lock, installed before sndInit. Both callbacks
 * must be supplied, or both NULL to use the adapter's default. They must remain
 * valid until replaced after sndQuit. This allows an application's I/O and
 * stream callbacks to share the engine's critical section without lock-order
 * inversion. The setting persists across sndQuit. */
bool sndPCSetSynchronization(void (*enter)(void), void (*leave)(void));
/* Where the PC layer keeps its own buffers (converted sample directories, decoded songs, the converter's field
 * bookkeeping): data that exists only on the PC and is larger than the GameCube's packed form. Default malloc /
 * free. They do not go through the game's sndSetHooks allocator, whose heap is sized for the GameCube data
 * (OrrePort: converting a group there ran the game's heap out, 2026-10-16). The host may pass an allocator that
 * keeps pointers below 4 GB. */
void sndPCSetHostAllocator(void* (*alloc)(size_t size), void (*release)(void* ptr));
void* salPCHostMalloc(size_t size);
void salPCHostFree(void* ptr);
/* Fills exactly frames * channels samples. No allocation, device access, or
 * wall-clock dependence. Calls must be serialized with all other snd* calls.
 * The core retains one 5 ms cycle: control/lifetime queries may lead delivered
 * PCM by up to that cycle. A zero-frame request does not advance the engine.
 * Returns false without touching output if uninitialized or output is NULL. */
bool sndPCRender(s16* output, size_t frames);
SND_PC_RENDER_INFO sndPCGetRenderInfo(void);

/* Optional application mixer, called once per completed 5 ms cycle with the
 * interleaved output before it is queued. Runs under the render lock; it must
 * not allocate, block or reenter sndPCRender. Passing NULL detaches it.
 * Registration persists through sndInit and is cleared by sndQuit. */
typedef void (*SND_PC_OUTPUT_CALLBACK)(s16* output, size_t frames, u32 rate, u32 channels);
void sndPCSetOutputCallback(SND_PC_OUTPUT_CALLBACK callback);

typedef struct SND_PC_SPAN {
  const void* data;
  size_t size;
} SND_PC_SPAN;

typedef struct SND_PC_GROUP_ASSETS {
  SND_PC_SPAN project, pool, directory, samples;
} SND_PC_GROUP_ASSETS;

typedef struct SND_PC_ASSET_ERROR {
  const char* section;
  size_t offset;
  const char* reason;
} SND_PC_ASSET_ERROR;

/* Bounded validation of raw GC sections. Immutable, alignment-independent and
 * usable without sndInit or an output device. This checks file structure;
 * references to groups already on the stack are resolved when pushing. */
bool sndPCValidateGroup(const SND_PC_GROUP_ASSETS* assets, SND_PC_ASSET_ERROR* error);
bool sndPCValidateArrangement(SND_PC_SPAN input, SND_PC_ASSET_ERROR* error);

/* Optional SDL adapter, built with MUSYX_ENABLE_SDL. It uses the same renderer,
 * performs device-format conversion, and serializes calls via hwDisableIrq /
 * hwEnableIrq (as the original public APIs do). Start after sndInit; stop before
 * teardown. Pause preserves voices, the control clock, and queued audio. */
/* Open paused before sndInit. NULL/zero fields select the opened device's
 * actual rate/layout; automatic rates are clamped to 8..96 kHz, and unsupported
 * device layouts use stereo. Explicit values follow sndPCConfigure's limits.
 * The selected configuration is returned through obtained when non-NULL.
 * sndPCStopAudio closes a prepared device even if sndInit has not been called. */
bool sndPCOpenAudio(const SND_PC_CONFIG* preferred, SND_PC_CONFIG* obtained);
bool sndPCStartAudio(void);
void sndPCStopAudio(void);
void sndPCPauseAudio(bool paused);
/* OrrePort: the output's volume, 0 (silent) to 1 (as mixed); applied as the device plays, any time, from any thread */
void sndPCSetMasterGain(float gain);
/* OrrePort: how fast the output plays (1 = as mixed; 2 = twice as fast and an octave higher), so the mixer's clock
 * follows a sped-up game; any time, from any thread (hw_pc_sdl.c says why) */
void sndPCSetPlaybackSpeed(float speed);
/* OrrePort: manual rendering (set before sndPCStartAudio): no render thread; the host calls sndPCRenderManual with
 * the number of 5 ms blocks due, on its own thread, and they are queued to the device while there is room. For a
 * deterministic replay clock, where the sound must follow the game's clock. */
void sndPCSetManualRender(bool manual);
bool sndPCRenderManual(u32 blocks);
/* OrrePort, the sound split prototype (2026-11-16; docs/port/sound-split.md): a factor on each voice's share of the
 * output (0 = silent, 1 = as mixed), asked per voice and per 5 ms block; the voice still plays on underneath, so
 * nothing the synth reports changes. NULL (the default): every voice as mixed. */
void sndPCSetVoiceGain(float (*gain)(u32 voice));

#ifdef __cplusplus
}
#endif
#endif
