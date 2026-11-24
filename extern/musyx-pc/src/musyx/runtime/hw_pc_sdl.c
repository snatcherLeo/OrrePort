#include "hw_pc_internal.h"
#include "musyx/hardware.h"
#include "musyx/dspvoice.h"
#include "musyx/sal.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_Mutex *mutex;
static SDL_AudioStream *stream;
static SDL_Thread *thread;
static SDL_AtomicInt running;
static SDL_AtomicInt paused;

/* OrrePort: the player's master volume (Settings > Audio), applied by SDL as the stream is played, so it never
 * changes the mix itself (ORRE_DUMP_AUDIO's file stays at full level). 1 = as mixed. */
static float masterGain = 1.f;

void sndPCSetMasterGain(float gain) {
  masterGain = gain < 0.f ? 0.f : gain;
  if (stream)
    SDL_SetAudioStreamGain(stream, masterGain);
}

/* OrrePort: Settings > Enhancements > Music and sound > "Faster with the game" (snatcherLeo, 2026-11-13, mockup 66). The
 * render thread mixes only when the device wants more (at most 40 ms queued), so playing the stream faster makes the
 * mixer, and with it every voice and every "has this sound ended?" the game asks, run at the same speed as the game:
 * music and effects play faster and higher, like fast-forward. 1 = as mixed. SDL resamples; any thread. Platforms:
 * SDL's own call, the same on Windows, Linux and macOS. */
static float playbackSpeed = 1.f;

void sndPCSetPlaybackSpeed(float speed) {
  /* up to 5x (OrrePort's Speed settings go to 5x, 2026-11-16; 4x before). SDL 3 takes far higher ratios. */
  playbackSpeed = speed < 0.25f ? 0.25f : speed > 5.f ? 5.f : speed;
  if (stream)
    SDL_SetAudioStreamFrequencyRatio(stream, playbackSpeed);
}

/* OrrePort: the rate the mix is played at. The GameCube's "32 kHz" audio runs from the 54 MHz clock (54 MHz / 1686,
 * about 32,028.5 Hz; Dolphin's dumps say 32,028), and its DSP mixes 160-sample frames at that clock, so pitch and
 * music tempo are 0.09% above a true 32,000 Hz. The mixer keeps its 160-sample cycles; telling SDL the stream plays at
 * 32,028 Hz makes it consume them at the console's speed (SDL resamples to the device). Other rates play as mixed. */
static int playbackRate(u32 mixRate) { return mixRate == 32000 ? 32028 : (int)mixRate; }

/* ORRE_DUMP_AUDIO=<file.wav> (OrrePort, for comparisons with Dolphin's audio dump): everything the mixer renders, as
 * 16-bit PCM at the mix rate. The header's sizes are rewritten about once a second, so a run that is killed still
 * leaves a playable file. Opened on the render thread's first block; written only there. */
static FILE *dumpFile;
static Uint32 dumpBytes;

static void dumpPut32(FILE *f, Uint32 v) {
  const unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16),
                              (unsigned char)(v >> 24)};
  fwrite(b, 1, 4, f);
}

static void dumpPut16(FILE *f, Uint16 v) {
  const unsigned char b[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
  fwrite(b, 1, 2, f);
}

static void dumpHeader(FILE *f, Uint32 rate, Uint16 channels, Uint32 dataBytes) {
  fseek(f, 0, SEEK_SET);
  fwrite("RIFF", 1, 4, f);
  dumpPut32(f, 36 + dataBytes);
  fwrite("WAVEfmt ", 1, 8, f);
  dumpPut32(f, 16);
  dumpPut16(f, 1); /* PCM */
  dumpPut16(f, channels);
  dumpPut32(f, rate);
  dumpPut32(f, rate * channels * 2);
  dumpPut16(f, (Uint16)(channels * 2));
  dumpPut16(f, 16);
  fwrite("data", 1, 4, f);
  dumpPut32(f, dataBytes);
  fseek(f, 0, SEEK_END);
}

static void dumpAudio(const s16 *buffer, size_t frames, const SND_PC_RENDER_INFO *info) {
  static int state = -1, blocks;
  if (state < 0) {
#ifdef ORRE_MUSYX_AUDIBLE_COPY
    /* OrrePort, the sound split prototype (2026-11-16; docs/port/sound-split.md): this file is compiled twice, and the
     * second (audible) copy dumps to its own file, so the two outputs can be compared and mixed apart */
    const char *path = SDL_getenv("ORRE_DUMP_AUDIO_AUDIBLE");
#else
    const char *path = SDL_getenv("ORRE_DUMP_AUDIO");
#endif
    state = 0;
    if (path != NULL && (dumpFile = fopen(path, "wb")) != NULL) {
      dumpHeader(dumpFile, (Uint32)playbackRate(info->mixRate), (Uint16)info->channels, 0);
      fprintf(stderr, "[audio] dumping the mix to %s (%u Hz, %u channels, 16-bit)\n", path,
              (unsigned)playbackRate(info->mixRate),
              (unsigned)info->channels);
      { /* ORRE_SOUND_LOG's times are mixer seconds; the dump's first sample is about this mixer time (the block just
         * rendered ends at the current cycle's end; within a cycle, 5 ms) */
        extern double salPCCurrentSeconds(void);
        fprintf(stderr, "[audio] dump starts at mixer t=%.4f\n",
                salPCCurrentSeconds() + 0.005 - (double)frames / info->mixRate);
      }
      state = 1;
    }
  }
  if (state != 1)
    return;
  /* little-endian 16-bit samples, as the WAV format wants (x86 / arm64 hosts are little-endian) */
  dumpBytes += (Uint32)fwrite(buffer, 1, frames * info->channels * sizeof(s16), dumpFile);
  if (++blocks >= 200) { /* 200 blocks of 5 ms */
    blocks = 0;
    dumpHeader(dumpFile, (Uint32)playbackRate(info->mixRate), (Uint16)info->channels, dumpBytes);
    fflush(dumpFile);
  }
}

static int audioThread(void *unused) {
  (void)unused;
  s16 buffer[SAL_PC_MAX_FRAMES * SAL_PC_MAX_CHANNELS];
  SND_PC_RENDER_INFO info = sndPCGetRenderInfo();
  size_t frames = (info.mixRate + 199) / 200;
  int bytes = (int)(frames * info.channels * sizeof(s16));
  SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
  while (SDL_GetAtomicInt(&running)) {
    /* At most 40 ms queued, in the stream's input format. */
    int queued = SDL_GetAudioStreamQueued(stream);
    if (queued < 0)
      break;
    if (SDL_GetAtomicInt(&paused) || queued >= bytes * 8) {
      SDL_Delay(1);
      continue;
    }
    hwIRQEnterCritical();
    bool ok = true;
    if (!SDL_GetAtomicInt(&paused) && SDL_GetAtomicInt(&running))
      ok = sndPCRender(buffer, frames) && SDL_PutAudioStreamData(stream, buffer, bytes);
    hwIRQLeaveCritical();
    if (ok)
      dumpAudio(buffer, frames, &info);
    { /* ORRE_AUDIO_METER=1 (OrrePort diagnostics): the loudest sample once a second, to tell silence in the mix from
       * silence at the device */
      static int meter = -1, steps;
      static s16 peak;
      if (meter < 0)
        meter = SDL_getenv("ORRE_AUDIO_METER") != NULL;
      if (meter) {
        for (size_t i = 0; i < frames * info.channels; ++i) {
          const s16 v = buffer[i] < 0 ? (s16)-buffer[i] : buffer[i];
          if (v > peak)
            peak = v;
        }
        if (++steps >= 200) {
          int active = 0, loudest = 0;
          for (u32 v = 0; dspVoice != NULL && v < salNumVoices; ++v) {
            if (dspVoice[v].state != DSP_VOICE_STATE_INACTIVE) {
              ++active;
              if (dspVoice[v].volL > loudest)
                loudest = dspVoice[v].volL;
              if (dspVoice[v].volR > loudest)
                loudest = dspVoice[v].volR;
            }
          }
          fprintf(stderr, "[audio_meter] peak %d (of 32767), queued %d bytes, voices active %d, loudest voice volume %d\n",
                  (int)peak, queued, active, loudest);
          steps = 0;
          peak = 0;
        }
      }
    }
    if (!ok)
      break;
  }
  fprintf(stderr, "[audio] render thread stopped\n"); /* OrrePort: silence from here on */
  if (dumpFile != NULL) { /* ORRE_DUMP_AUDIO: final sizes */
    dumpHeader(dumpFile, (Uint32)playbackRate(info.mixRate), (Uint16)info.channels, dumpBytes);
    fclose(dumpFile);
    dumpFile = NULL;
  }
  SDL_SetAtomicInt(&running, 0);
  return 0;
}

bool sndPCOpenAudio(const SND_PC_CONFIG *preferred, SND_PC_CONFIG *obtained) {
  SND_PC_CONFIG selected = preferred ? *preferred : (SND_PC_CONFIG){0, 0};
  if (sndIsInstalled() || stream || thread ||
      (selected.mixRate &&
       (selected.mixRate < SAL_PC_MIN_RATE || selected.mixRate > SAL_PC_MAX_RATE)) ||
      (selected.channels && selected.channels != 2 && selected.channels != 4 &&
       selected.channels != 6 && selected.channels != 8))
    return false;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    return false;
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL, NULL, NULL);
  if (!stream) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return false;
  }
  SDL_AudioSpec device;
  if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &device, NULL)) {
    sndPCStopAudio();
    return false;
  }
  if (!selected.mixRate)
    selected.mixRate = CLAMP(device.freq, SAL_PC_MIN_RATE, SAL_PC_MAX_RATE);
  if (!selected.channels)
    selected.channels =
        device.channels == 4 || device.channels == 6 || device.channels == 8 ? device.channels : 2;
  SDL_AudioSpec input = {SDL_AUDIO_S16, (int)selected.channels, playbackRate(selected.mixRate)};
  if (!SDL_SetAudioStreamFormat(stream, &input, NULL) || !sndPCConfigure(&selected)) {
    sndPCStopAudio();
    return false;
  }
  if (obtained)
    *obtained = selected;
  return true;
}

/* OrrePort, 2026-10-19 (the deterministic replay clock): in manual mode no render thread runs; the host renders the
 * mix itself with sndPCRenderManual, on its game thread, as many 5 ms blocks as are due, so the sound's progress (and
 * the game callbacks the mixer calls) follow the game's own clock instead of the sound device's. The blocks are still
 * played: queued to the device while there is room (up to about 200 ms; beyond that, when the host runs faster than
 * real time, they are dropped, which the game never sees). */
static bool manualRender;
void sndPCSetManualRender(bool manual) { manualRender = manual; }

bool sndPCRenderManual(u32 blocks) {
  static s16 buffer[SAL_PC_MAX_FRAMES * SAL_PC_MAX_CHANNELS];
  SND_PC_RENDER_INFO info = sndPCGetRenderInfo();
  size_t frames = (info.mixRate + 199) / 200;
  int bytes = (int)(frames * info.channels * sizeof(s16));
  if (!manualRender || !SDL_GetAtomicInt(&running))
    return false;
  for (u32 i = 0; i < blocks; ++i) {
    hwIRQEnterCritical();
    bool ok = sndPCRender(buffer, frames);
    hwIRQLeaveCritical();
    if (!ok)
      return false;
    dumpAudio(buffer, frames, &info);
    if (stream && !SDL_GetAtomicInt(&paused) && SDL_GetAudioStreamQueued(stream) < bytes * 40)
      SDL_PutAudioStreamData(stream, buffer, bytes);
  }
  return true;
}

bool sndPCStartAudio(void) {
  if (!sndIsInstalled() || thread || (manualRender && SDL_GetAtomicInt(&running)))
    return false;
  SND_PC_RENDER_INFO info = sndPCGetRenderInfo();
  SDL_AudioSpec input = {SDL_AUDIO_S16, (int)info.channels, playbackRate(info.mixRate)};
  if (!stream) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
      return false;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &input, NULL, NULL);
    if (!stream) {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return false;
    }
  } else if (!SDL_SetAudioStreamFormat(stream, &input, NULL)) {
    sndPCStopAudio();
    return false;
  }
  SDL_SetAudioStreamGain(stream, masterGain);
  SDL_SetAudioStreamFrequencyRatio(stream, playbackSpeed);
  SDL_SetAtomicInt(&paused, 0);
  SDL_SetAtomicInt(&running, 1);
  if (manualRender) { /* the host renders (sndPCRenderManual) */
    if (!SDL_ResumeAudioStreamDevice(stream)) {
      sndPCStopAudio();
      return false;
    }
    return true;
  }
  thread = SDL_CreateThread(audioThread, "MusyX Audio", NULL);
  if (!thread || !SDL_ResumeAudioStreamDevice(stream)) {
    sndPCStopAudio();
    return false;
  }
  return true;
}

void sndPCStopAudio(void) {
  SDL_SetAtomicInt(&running, 0);
  if (thread) {
    SDL_WaitThread(thread, NULL);
    thread = NULL;
  }
  if (stream) {
    SDL_DestroyAudioStream(stream);
    stream = NULL;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

void sndPCPauseAudio(bool value) {
  SDL_SetAtomicInt(&paused, value);
  /* Barrier with a batch already being rendered when pause was requested. */
  hwIRQEnterCritical();
  hwIRQLeaveCritical();
  if (stream) {
    if (value)
      SDL_PauseAudioStreamDevice(stream);
    else
      SDL_ResumeAudioStreamDevice(stream);
  }
}

void hwInitIrq(void) {
  mutex = SDL_CreateMutex();
  hwDisableIrq();
}
void hwExitIrq(void) {
  SDL_DestroyMutex(mutex);
  mutex = NULL;
}
void hwEnableIrq(void) {
  if (!salPCExternalLeave() && mutex)
    SDL_UnlockMutex(mutex);
}
void hwDisableIrq(void) {
  if (!salPCExternalEnter() && mutex)
    SDL_LockMutex(mutex);
}
void hwIRQEnterCritical(void) { hwDisableIrq(); }
void hwIRQLeaveCritical(void) { hwEnableIrq(); }
