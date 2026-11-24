#include "musyx/pc.h" /* salPCHostMalloc */
#include "musyx/platform.h"

#include "hw_pc_decode.h"
#include "hw_pc_internal.h"
#include <math.h>
#include <stdio.h>  /* ORRE_SOUND_LOG */
#include <stdlib.h> /* getenv (OrrePort test switch) */
/* OrrePort: the switch for the per-voice sound log. The sound split prototype (2026-11-16; docs/port/sound-split.md)
 * builds this file a second time as the audible MusyX copy with ORRE_SOUND_LOG_ENV="ORRE_SOUND_LOG_AUDIBLE", so the
 * two copies' lines never mix in one log unless both are asked for. */
#ifndef ORRE_SOUND_LOG_ENV
#define ORRE_SOUND_LOG_ENV "ORRE_SOUND_LOG"
#endif

#include <string.h>

#include "musyx/adsr.h"
#include "musyx/assert.h"
#include "musyx/hardware.h"
#include "musyx/musyx.h"
#include "musyx/sal.h"
#include "musyx/synth.h"

// Audio parameters
#define SAL_SAMPLES_PER_FRAME SAL_PC_MAX_FRAMES
#define SAL_SUBFRAMES 5
static u32 cycleFrames;
static u32 segmentOffset[SAL_SUBFRAMES + 1];
static u64 cycleTick;
static s32 effectHistory[8][2][2][3][32];
static s32 studioHistory[8][7][5][2];
static s32 rearMain[8][2][2][SAL_PC_MAX_FRAMES];
static s32 legacyWork[3][160];
static s32 adaptedWork[5][SAL_PC_MAX_FRAMES];

/* Detached, bounded stop tails: no decoder/sample pointer survives hwOff, and
 * reusing a voice cannot truncate the preceding generation's ramp. */
#define DEPOP_RING 1024
static s32 depop[8][11][DEPOP_RING];
static s32 lastContribution[SYNTH_MAX_VOICES][11];
static u32 depopCursor, renderOffset;

/* OrrePort: the mixer time in seconds at the current cycle (the audio dump's start, ORRE_SOUND_LOG's times) */
double salPCCurrentSeconds(void) { return (double)salPCBoundary(cycleTick, salPCMixRate()) / salPCMixRate(); }

void salPCBeginCycle(u64 tick) {
  cycleTick = tick;
  u32 rate = salPCMixRate();
  u64 begin = salPCBoundary(tick, rate);
  for (u32 i = 0; i <= 5; ++i)
    segmentOffset[i] = (u32)(salPCBoundary(tick + i, rate) - begin);
  cycleFrames = segmentOffset[5];
}

static void resetStudioHistory(u8 studio) {
  memset(effectHistory[studio], 0, sizeof(effectHistory[studio]));
  memset(studioHistory[studio], 0, sizeof(studioHistory[studio]));
}
#define POLYPHASE_PHASES 128
#define POLYPHASE_TAPS 32
#define POLYPHASE_BANDS 64
#define POLYPHASE_SCALE (1 << 23)
typedef struct VoiceResamplerState {
  MusyPCMReader reader;
  u32 phase;
  s16 history[POLYPHASE_TAPS];
  u32 historyIndex;
  u8 tail;
  s16 itdHistory[128];
  u32 itdIndex;
  u32 itdDelay[2];
} VoiceResamplerState;

static VoiceResamplerState voiceResampler[SYNTH_MAX_VOICES];
static s32 polyphaseTable[POLYPHASE_BANDS][POLYPHASE_PHASES][POLYPHASE_TAPS];
static u8 resampleTablesInitialized = 0;

// Mix accumulation buffers
static s32 mixBufferL[SAL_SAMPLES_PER_FRAME];
static s32 mixBufferR[SAL_SAMPLES_PER_FRAME];
static s32 mixBufferRear[2][SAL_SAMPLES_PER_FRAME];
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
static s32 filterState[SYNTH_MAX_VOICES];
#endif

/* Removed 2026-11-01 (snatcherLeo's OK; the project notes): ORRE_AUDIO_DSP_DELAY, a starting voice held N
 * mix blocks before it played (P01's old Folly resync; no final replay uses it) */

/* ORRE_SOUND_LOG (2026-11-01, port-gaps 28): why the mixer ended a voice, printed on its "[sndvoice] ... end"
 * line: "break" (the synth's hwBreak), "sample" (a one-shot sample ran out), "adsr" (the envelope finished its
 * release); anything else (hwOff: the synth's kill or a voice steal; a voice that failed to start) prints "off" */
static const char *voiceEndWhy;

#define SURROUND_DOWNMIX_GAIN 23170

static inline s16 clamp16(s32 v) {
  if (v > 32767)
    return 32767;
  if (v < -32768)
    return -32768;
  return (s16)v;
}

static double sincUnit(double x) {
  if (fabs(x) < 1e-12)
    return 1.0;
  const double pix = x * M_PI;
  return sin(pix) / pix;
}

static void initResampleTables(void) {
  if (resampleTablesInitialized)
    return;
  /* Generated Blackman-windowed sinc filters, independently authored for the
   * native renderer. Each phase has exact unity DC gain in Q23. Filter banks
   * lower the cutoff as source pitch rises, rather than aliasing downsampling. */
  for (u32 band = 0; band < POLYPHASE_BANDS; ++band) {
    double cutoff = (double)(band + 1) / POLYPHASE_BANDS;
    for (u32 phase = 0; phase < POLYPHASE_PHASES; ++phase) {
      double coefficients[POLYPHASE_TAPS], sum = 0;
      double center = POLYPHASE_TAPS / 2 - 1 + (double)phase / POLYPHASE_PHASES;
      for (u32 tap = 0; tap < POLYPHASE_TAPS; ++tap) {
        double offset = tap - center;
        double angle = 2 * M_PI * offset / POLYPHASE_TAPS;
        double window = 0.42 + 0.5 * cos(angle) + 0.08 * cos(2 * angle);
        coefficients[tap] = cutoff * sincUnit(cutoff * offset) * window;
        sum += coefficients[tap];
      }
      s32 total = 0;
      for (u32 tap = 0; tap < POLYPHASE_TAPS; ++tap) {
        s32 value = (s32)lround(coefficients[tap] / sum * POLYPHASE_SCALE);
        polyphaseTable[band][phase][tap] = value;
        total += value;
      }
      polyphaseTable[band][phase][POLYPHASE_TAPS / 2] += POLYPHASE_SCALE - total;
    }
  }
  resampleTablesInitialized = 1;
}

const s32 *salPCRateCoefficients(u32 inputRate, u32 outputRate, u32 fraction) {
  /* Leave room for the finite filter's transition band below the destination
   * Nyquist frequency. This also limits phase-dependent alias energy. */
  u32 band = inputRate > outputRate ? (u64)outputRate * POLYPHASE_BANDS * 9 / (inputRate * 10)
                                    : POLYPHASE_BANDS;
  if (!band)
    band = 1;
  return polyphaseTable[band - 1][fraction >> 9];
}

static s32 mixRampChannel(s32 *dest, const s32 *src, int nSamples, s16 startVol, s16 endVol,
                          u16 envStart, u16 envEnd, int frameOffset) {
  if (!dest || (!startVol && !endVol))
    return 0;
  s32 contribution = 0;
  for (int i = 0; i < nSamples; ++i) {
    s32 env = envStart + ((s32)envEnd - envStart) * i / nSamples;
    s32 vol = startVol + ((s32)endVol - startVol) * (frameOffset + i) / (s32)cycleFrames;
    s32 mixVol = (vol * env) >> 15;
    contribution = (src[i] * mixVol) >> 15;
    dest[i] += contribution;
  }
  return contribution;
}

static void retireContribution(DSPvoice *voice) {
  s32 *last = lastContribution[voice - dspVoice];
  const u32 frames = (salPCMixRate() + 199) / 200;
  for (u32 channel = 0; channel < 11; ++channel) {
    for (u32 frame = 0; frame < frames; ++frame)
      depop[voice->studio][channel][(depopCursor + renderOffset + frame) & (DEPOP_RING - 1)] +=
          (s32)((s64)last[channel] * (frames - frame - 1) / frames);
    last[channel] = 0;
  }
}

static void mixStudioOutput(const s32 *left, const s32 *right, const s32 *surround) {
  /* OrrePort test switch ORRE_NO_SURROUND_FOLD=1 (2026-10-18): drop the surround bus in stereo output instead of
   * folding it into left and right at -3 dB. Dolphin's AX output takes only left and right (B's reading of its code,
   * gamecube-accuracy.md entry 10), so sounds with a surround share (3D-placed or rear-panned SFX and cries) may be
   * louder on the PC; snatcherLeo hears "certain sfx / pokemon cries" at the wrong volume. For the comparison with Dolphin */
  static int noFold = -1;
  if (noFold < 0)
    noFold = getenv("ORRE_NO_SURROUND_FOLD") != NULL;
  if (noFold && salPCChannels() == 2)
    surround = NULL;
  for (int i = 0; i < cycleFrames; ++i) {
    mixBufferL[i] += left[i];
    mixBufferR[i] += right[i];
    if (surround != NULL) {
      s32 surroundMix = (s32)((s64)surround[i] * SURROUND_DOWNMIX_GAIN >> 15);
      if (salPCChannels() == 2) {
        mixBufferL[i] += surroundMix;
        mixBufferR[i] += surroundMix;
      } else {
        mixBufferRear[0][i] += surroundMix;
        mixBufferRear[1][i] += surroundMix;
      }
    }
  }
}

static void foldStereoToOutput(const s32 *left, const s32 *right) {
  for (int i = 0; i < cycleFrames; ++i) {
    mixBufferL[i] += left[i];
    mixBufferR[i] += right[i];
  }
}

/* Causal generated polyphase interpolation. Decoder history and sample position
 * advance only as the renderer consumes source samples; there is no speculative
 * decode across a stream refill boundary. */
static int resampleVoice(DSPvoice *voice, u32 voiceIdx, s32 *out, int numSamples, u32 pitch) {
  VoiceResamplerState *state = &voiceResampler[voiceIdx];
  const u32 mode = voice->srcTypeSelect;
  if (mode == SAL_SRC_NONE)
    pitch = 65536; /* SDK SRC_NONE always runs at the mixing rate. */
  u32 filter = voice->srcCoefSelect;
  u32 cutoff = filter == 0 ? 32768 : filter == 1 ? 52428 : 65536;
  if (pitch > 65536)
    cutoff = (u32)(((u64)cutoff * 65536) / pitch);
  u32 band = cutoff * POLYPHASE_BANDS / 65536;
  if (!band)
    band = 1;
  if (band > POLYPHASE_BANDS)
    band = POLYPHASE_BANDS;
  for (int i = 0; i < numSamples; ++i) {
    state->phase += pitch;
    while (state->phase >= 65536) {
      /* A virtual sample first consumes its uploaded prefix, then continues
       * in the callback-owned ring without resetting ADPCM history. */
      SAMPLE_INFO *source = &voice->smp_info;
      if (source->compType == SAMPLE_TYPE_ADPCM_VIRTUAL && !voice->vSampleInfo.inLoopBuffer &&
          source->loopLength && state->reader.position >= source->loop + source->loopLength) {
        source->addr = voice->vSampleInfo.loopBufferAddr;
        source->length = source->loopLength = voice->vSampleInfo.loopBufferLength;
        source->loop = 0;
        voice->vSampleInfo.inLoopBuffer = 1;
        state->reader.position = 0;
        state->reader.predictorScale = voice->streamLoopPS;
        state->reader.useInitialPS = true;
      }
      s16 sample = salPCReadSample(&state->reader, &voice->smp_info, voice->streamLoopPS);
      state->history[state->historyIndex++ & (POLYPHASE_TAPS - 1)] = sample;
      if (state->reader.ended && state->tail < POLYPHASE_TAPS)
        ++state->tail;
      state->phase -= 65536;
    }
    if (mode == SAL_SRC_NONE) {
      out[i] = state->history[(state->historyIndex - 1) & (POLYPHASE_TAPS - 1)];
    } else if (mode == SAL_SRC_LINEAR) {
      s32 older = state->history[(state->historyIndex - 2) & (POLYPHASE_TAPS - 1)];
      s32 newer = state->history[(state->historyIndex - 1) & (POLYPHASE_TAPS - 1)];
      out[i] = older + (s32)((s64)(newer - older) * state->phase / 65536);
    } else {
      const s32 *coefficients = polyphaseTable[band - 1][state->phase >> 9];
      s64 result = 0;
      for (u32 tap = 0; tap < POLYPHASE_TAPS; ++tap)
        result += (s64)state->history[(state->historyIndex + tap) & (POLYPHASE_TAPS - 1)] *
                  coefficients[tap];
      out[i] = clamp16((s32)(result >> 23));
    }
  }
  voice->playInfo.posHi = state->reader.position;
  voice->playInfo.posLo = state->phase;
  return numSamples;
}

/*
 * Software voice rendering for one frame (SAL_SAMPLES_PER_FRAME samples).
 * Decodes samples, then mixes into main and AUX buffers with per-channel volumes.
 */
static s32 voiceDecodeBuf[SAL_SAMPLES_PER_FRAME];
static s32 itdOutput[2][SAL_SAMPLES_PER_FRAME];

/* OrrePort, the sound split prototype (2026-11-16): the per-voice output gain renderVoiceSegment applies (NULL: none).
 * Set from the game thread before the mixer starts; read by the mixer under its lock. */
static float (*voiceGain)(u32 voice);
void sndPCSetVoiceGain(float (*gain)(u32 voice)) {
  hwIRQEnterCritical();
  voiceGain = gain;
  hwIRQLeaveCritical();
}

static u32 itdTarget(const DSPvoice *voice, u32 channel) {
  if (!(voice->flags & 0x80000000))
    return 0;
  u32 shift = channel ? voice->itdShiftR : voice->itdShiftL;
  if (shift > 32)
    shift = 32;
  return (u32)(((u64)shift * salPCMixRate() * 65536) / 32000);
}

static void applyITD(const DSPvoice *voice, VoiceResamplerState *state, int frames, int offset) {
  u32 targets[2] = {itdTarget(voice, 0), itdTarget(voice, 1)};
  for (int i = 0; i < frames; ++i) {
    state->itdHistory[state->itdIndex & 127] = voiceDecodeBuf[i];
    for (u32 channel = 0; channel < 2; ++channel) {
      s64 delay = state->itdDelay[channel] +
                  ((s64)targets[channel] - state->itdDelay[channel]) * (offset + i) / cycleFrames;
      u32 index = state->itdIndex - (u32)(delay >> 16);
      s32 newer = state->itdHistory[index & 127];
      s32 older = state->itdHistory[(index - 1) & 127];
      itdOutput[channel][i] = newer + (s32)((s64)(older - newer) * (delay & 0xffff) / 65536);
    }
    ++state->itdIndex;
  }
}

// Returns 1 if voice finished playing (end of non-looping sample), 0 otherwise.
static int renderVoiceSegment(DSPvoice *vp, s32 *mainL, s32 *mainR, s32 *mainS, s32 *auxAL,
                              s32 *auxAR, s32 *auxAS, s32 *auxBL, s32 *auxBR, s32 *auxBS,
                              u32 voiceIdx, int frameOffset, int frameSamples, u16 adsrStart,
                              u16 adsrEnd) {
  if (vp->state == DSP_VOICE_STATE_INACTIVE)
    return 0;

  const SAMPLE_INFO *smp = &vp->smp_info;
  if (smp->addr == NULL)
    return 0;

  u32 pitch = vp->playInfo.pitch;
  if (pitch == 0)
    pitch = vp->pitch[0];
  if (pitch == 0 && vp->srcTypeSelect != SAL_SRC_NONE)
    return 0;

  /* Decode samples into temp buffer */
  VoiceResamplerState *state = &voiceResampler[voiceIdx];
  int nSamples = resampleVoice(vp, voiceIdx, voiceDecodeBuf, frameSamples, pitch);
  u32 tail = vp->srcTypeSelect == SAL_SRC_NONE     ? 1
             : vp->srcTypeSelect == SAL_SRC_LINEAR ? 2
                                                   : POLYPHASE_TAPS;
  int voiceDone = state->tail >= tail;

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  if (vp->filter.on) {
    s32 yn = filterState[voiceIdx];
    for (int i = 0; i < nSamples; ++i) {
      yn = ((s32)vp->filter.coefA * voiceDecodeBuf[i] + (s32)vp->filter.coefB * yn) >> 15;
      voiceDecodeBuf[i] = yn;
    }
    filterState[voiceIdx] = yn;
  }
#endif

  applyITD(vp, state, nSamples, frameOffset);
  { /* OrrePort, the sound split prototype (2026-11-16; docs/port/sound-split.md): a per-voice output gain, applied to
     * what this voice adds to the mix and nothing else. The voice is still decoded and stepped exactly as before (its
     * position, its end, the ITD history and the DSP budget pass above all see the unscaled voice), so the synth's
     * state, and every answer the game gets from it, is the same whatever the gain. The split uses it to silence the
     * game instance's music (the audible copy plays it at normal tempo) while that instance keeps running for the
     * game. NULL (the default): untouched. Platforms: plain C, the same everywhere. */
    if (voiceGain != NULL) {
      const float g = voiceGain(voiceIdx);
      if (g != 1.f) {
        for (int i = 0; i < nSamples; ++i) {
          voiceDecodeBuf[i] = (s32)((float)voiceDecodeBuf[i] * g);
          itdOutput[0][i] = (s32)((float)itdOutput[0][i] * g);
          itdOutput[1][i] = (s32)((float)itdOutput[1][i] * g);
        }
      }
    }
  }
  lastContribution[voiceIdx][0] =
      mixRampChannel(mainL ? mainL + frameOffset : NULL, itdOutput[0], nSamples, (s16)vp->lastVolL,
                     (s16)vp->volL, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][1] =
      mixRampChannel(mainR ? mainR + frameOffset : NULL, itdOutput[1], nSamples, (s16)vp->lastVolR,
                     (s16)vp->volR, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][2] =
      mixRampChannel(mainS ? mainS + frameOffset : NULL, voiceDecodeBuf, nSamples,
                     (s16)vp->lastVolS, (s16)vp->volS, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][3] =
      mixRampChannel(auxAL ? auxAL + frameOffset : NULL, itdOutput[0], nSamples, (s16)vp->lastVolLa,
                     (s16)vp->volLa, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][4] =
      mixRampChannel(auxAR ? auxAR + frameOffset : NULL, itdOutput[1], nSamples, (s16)vp->lastVolRa,
                     (s16)vp->volRa, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][5] =
      mixRampChannel(auxAS ? auxAS + frameOffset : NULL, voiceDecodeBuf, nSamples,
                     (s16)vp->lastVolSa, (s16)vp->volSa, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][6] =
      mixRampChannel(auxBL ? auxBL + frameOffset : NULL, itdOutput[0], nSamples, (s16)vp->lastVolLb,
                     (s16)vp->volLb, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][7] =
      mixRampChannel(auxBR ? auxBR + frameOffset : NULL, itdOutput[1], nSamples, (s16)vp->lastVolRb,
                     (s16)vp->volRb, adsrStart, adsrEnd, frameOffset);
  lastContribution[voiceIdx][8] =
      mixRampChannel(auxBS ? auxBS + frameOffset : NULL, voiceDecodeBuf, nSamples,
                     (s16)vp->lastVolSb, (s16)vp->volSb, adsrStart, adsrEnd, frameOffset);

  for (u32 channel = 0; channel < 2; ++channel)
    lastContribution[voiceIdx][9 + channel] = mixRampChannel(
        rearMain[vp->studio][salFrame][channel] + frameOffset, itdOutput[channel], nSamples,
        vp->lastVolRear[channel], vp->volRear[channel], adsrStart, adsrEnd, frameOffset);
  return voiceDone;
}

/* OrrePort (2026-11-01, port-gaps 28; for B to review): the console's DSP time budget. On the GameCube,
 * salBuildCommandList (hw_dspctrl, matched code) walks each studio's voices by priority, highest first, and adds up
 * what each costs the DSP this 5 ms frame: its mixer mode (the parameter block's mixerCtrl bits, which follow the
 * volume ramps), its sample-rate converter and its update commands, plus each studio's and input's overhead. When the
 * sum passes one DSP frame (__OSBusClock / 400 cycles), that voice and every later one are cut at once and the synth
 * gets a kill message. The native mixer renders every voice, so without this pass nothing is ever cut and released
 * sounds ring on (the voices pairing on P08). This pass repeats the console's bookkeeping with a shadow of each
 * voice's parameter block (the volumes the DSP ends the frame on, mixerCtrl, the converter ratio and type) and a copy
 * of its envelope, so the PC's own rendering is untouched; it only decides which voices the console would cut.
 * Taken from the console's code as-is (the tables are verified against main.dol); likely-exact assumptions: the
 * start delay is under 200 us (the base 10,430 cycles), and a virtual-sample voice's loop buffer is ready at start.
 * ORRE_DSP_BUDGET=0 turns it off (comparisons only). */
#define BUDGET_VOICES 64
#define BUDGET_CYCLES (162000000u / 400) /* __OSBusClock / 400: the GameCube's bus clock, 162 MHz */

typedef struct BudgetShadow {
  u16 last[9]; /* L R S, aux A L R S, aux B L R S: the console's lastVol*, = the DSP's volumes at frame end */
  u16 mixerCtrl;
  u16 ratioHi; /* the converter ratio's integer part the DSP is using */
  u16 srcSelect;
} BudgetShadow;
static BudgetShadow budgetShadow[BUDGET_VOICES];

static const u16 budgetMixerCycles[32] = { /* dspMixerCycles, lbl_802732E0 */
    1470, 2940, 2940, 4410, 2230, 4460, 4460, 6690, 2470, 4940, 4940, 7410, 3735, 7470, 7470, 11205,
    2940, 3386, 2940, 3386, 2940, 3386, 2940, 3386, 4940, 5687, 4940, 5687, 4940, 5687, 4940, 5687,
};
static const u16 budgetSRCCycles[3][3] = { /* dspSRCCycles, lbl_80369A50 */
    {2990, 2990, 1115},
    {3300, 3300, 1115},
    {3700, 3700, 1115},
};

static void budgetSetupVol(u16 *delta, u16 *last, u16 vol) { /* sal_setup_dspvol */
  *delta = ((s16)vol - (s16)*last) / 160;
  *last += (s16)*delta * 160;
}

static u32 budgetCheckVol(u16 *vol, u16 *delta, u16 *last, u16 target, u16 *reset, u16 mask) {
  /* salCheckVolErrorAndResetDelta */
  s16 d, x;
  if (target != *last) {
    d = (s16)target - (s16)*last;
    if (d >= 32 && d < 160) {
      x = d >> 5;
      if (x < 5)
        reset[x] |= mask;
      *delta = 1;
      *last += x << 5;
      return 1;
    }
    if (-32 >= d && -160 < d) {
      x = -d >> 5;
      if (x < 5)
        reset[x] |= mask;
      *delta = 0xFFFF;
      *last -= x << 5;
      return 1;
    }
    if (target == 0 && d > -32)
      *vol = *last = 0;
  }
  *delta = 0;
  return 0;
}

static void budgetSortVoices(DSPvoice **voices, long l, long r) { /* SortVoices: the same quicksort, so ties order alike */
  long i, last;
  DSPvoice *tmp;
  if (l >= r)
    return;
  tmp = voices[l];
  voices[l] = voices[(l + r) / 2];
  voices[(l + r) / 2] = tmp;
  last = l;
  for (i = l + 1; i <= r; ++i) {
    if (voices[i]->prio < voices[l]->prio) {
      ++last;
      tmp = voices[last];
      voices[last] = voices[i];
      voices[i] = tmp;
    }
  }
  tmp = voices[l];
  voices[l] = voices[last];
  voices[last] = tmp;
  budgetSortVoices(voices, l, last - 1);
  budgetSortVoices(voices, last + 1, r);
}

/* One voice's cost this frame, as salBuildCommandList adds it; 0 = the console ends the voice before costing it. */
static int budgetVoice(const DSPstudioinfo *stp, const DSPvoice *vp, BudgetShadow *sh, u32 *cycles) {
  const u16 target[9] = {vp->volL, vp->volR, vp->volS, vp->volLa, vp->volRa, vp->volSa, vp->volLb, vp->volRb, vp->volSb};
  u16 v[9], d[9] = {0}, reset[5] = {0};
  ADSR_VARS adsr = vp->adsr;
  u8 mixStart, postBreak = vp->postBreak;
  u32 done, upd = 0, ratioUpdate = 0, needsDelta;
  u16 mc, start, delta, oldDelta, nextRatio = 0;
  s32 curDelta;
  int i, s;

  if (vp->state == DSP_VOICE_STATE_STARTING) {
    if (adsrSetup(&adsr) != 0)
      return 0;
    for (i = 0; i < 9; ++i)
      sh->last[i] = v[i] = target[i];
    mc = (v[5] | (v[3] | v[4])) != 0 ? 1 : 0;
    if (stp->type == 0) {
      if ((v[8] | (v[6] | v[7])) != 0)
        mc |= 2;
      if ((v[8] | (v[2] | v[5])) != 0)
        mc |= 4;
    } else if ((v[5] | (v[6] | v[7])) != 0) {
      mc |= 0x10;
    }
    sh->srcSelect = vp->srcTypeSelect;
    mixStart = vp->singleOffset;
  } else {
    if (vp->smp_info.loopLength == 0 && vp->playInfo.posHi >= vp->smp_info.length)
      return 0;
    if ((vp->changed[0] & 0x10) != 0 && adsrSetup(&adsr) != 0)
      return 0;
    for (i = 0; i < 9; ++i)
      v[i] = sh->last[i];
    mc = sh->mixerCtrl;
    if ((vp->changed[0] & 1) != 0) {
      for (i = 0; i < 3; ++i)
        budgetSetupVol(&d[i], &sh->last[i], target[i]);
      needsDelta = 1;
    } else {
      needsDelta = budgetCheckVol(&v[0], &d[0], &sh->last[0], target[0], reset, 1);
      needsDelta |= budgetCheckVol(&v[1], &d[1], &sh->last[1], target[1], reset, 2);
      needsDelta |= budgetCheckVol(&v[2], &d[2], &sh->last[2], target[2], reset, 4);
    }
    if ((vp->changed[0] & 2) != 0) {
      for (i = 3; i < 6; ++i)
        budgetSetupVol(&d[i], &sh->last[i], target[i]);
      if ((d[5] | (d[3] | d[4])) != 0) {
        mc |= 1;
        needsDelta = 1;
      } else if ((v[5] | (v[3] | v[4])) != 0) {
        mc |= 1;
      } else {
        mc &= ~1;
      }
    } else if ((mc & 1) != 0) {
      u32 local = budgetCheckVol(&v[3], &d[3], &sh->last[3], target[3], reset, 8);
      local |= budgetCheckVol(&v[4], &d[4], &sh->last[4], target[4], reset, 0x10);
      local |= budgetCheckVol(&v[5], &d[5], &sh->last[5], target[5], reset, 0x20);
      if ((local | (v[5] | (v[3] | v[4]))) == 0)
        mc &= ~1;
      else
        needsDelta = 1;
    } else {
      d[3] = d[4] = d[5] = 0;
    }
    if ((vp->changed[0] & 4) != 0) {
      if (stp->type == 0) {
        for (i = 6; i < 9; ++i)
          budgetSetupVol(&d[i], &sh->last[i], target[i]);
        if ((d[8] | (d[6] | d[7])) != 0) {
          mc |= 2;
          needsDelta = 1;
        } else if ((v[8] | (v[6] | v[7])) != 0) {
          mc |= 2;
        } else {
          mc &= ~2;
        }
      } else {
        budgetSetupVol(&d[6], &sh->last[6], target[6]);
        budgetSetupVol(&d[7], &sh->last[7], target[7]);
        if ((d[6] | d[7]) != 0) {
          mc |= 0x10;
          needsDelta = 1;
        } else if ((d[5] | (v[5] | (v[6] | v[7]))) != 0) {
          mc |= 0x10;
        } else {
          mc &= ~0x10;
        }
      }
    } else if (stp->type == 0) {
      if ((mc & 2) != 0) {
        u32 local = budgetCheckVol(&v[6], &d[6], &sh->last[6], target[6], reset, 0x40);
        local |= budgetCheckVol(&v[7], &d[7], &sh->last[7], target[7], reset, 0x80);
        local |= budgetCheckVol(&v[8], &d[8], &sh->last[8], target[8], reset, 0x100);
        if ((local | (v[8] | (v[6] | v[7]))) == 0)
          mc &= ~2;
        else
          needsDelta = 1;
      } else {
        d[6] = d[7] = d[8] = 0;
      }
    } else if ((mc & 0x10) != 0) {
      u32 local = budgetCheckVol(&v[6], &d[6], &sh->last[6], target[6], reset, 0x40);
      local |= budgetCheckVol(&v[7], &d[7], &sh->last[7], target[7], reset, 0x80);
      if ((local | (v[6] | v[7])) == 0) {
        if ((v[5] | d[5]) == 0)
          mc &= ~0x10;
      } else {
        needsDelta = 1;
      }
    } else {
      d[6] = d[7] = 0;
      if ((v[5] | d[5]) != 0)
        mc |= 0x10;
    }
    if (needsDelta != 0)
      mc |= 8;
    else
      mc &= ~8;
    if (stp->type == 0) {
      if (v[2] != 0 || d[2] != 0 || v[5] != 0 || d[5] != 0 || v[8] != 0 || d[8] != 0)
        mc |= 4;
      else
        mc &= ~4;
    }
    if ((vp->changed[0] & 0x100) != 0)
      sh->srcSelect = vp->srcTypeSelect;
    mixStart = 0;
  }

  /* block_186 and the sub-frame loop: the update commands this voice sends the DSP this frame */
  if ((vp->changed[mixStart] & 0x40) != 0)
    adsrRelease(&adsr);
  if ((vp->changed[mixStart] & 8) != 0)
    sh->ratioHi = (u16)(vp->pitch[mixStart] >> 16);
  done = adsrHandle(&adsr, &start, &oldDelta);
  if (mixStart != 0)
    ++upd;
  for (s = mixStart + 1; s < 5; ++s) {
    if (done != 0) {
      ++upd;
      break;
    }
    for (i = 0; i < 9; ++i)
      if ((reset[s] & (1 << i)) != 0)
        ++upd;
    if ((vp->changed[s] & 0x20) != 0) {
      adsrStartRelease(&adsr, 10);
      postBreak = 1;
    } else if (postBreak == 0) {
      if ((vp->changed[s] & 0x40) != 0)
        adsrRelease(&adsr);
      if ((vp->changed[s] & 8) != 0) {
        upd += 2;
        nextRatio = (u16)(vp->pitch[s] >> 16);
        ratioUpdate = 1;
      }
    }
    curDelta = adsr.currentDelta;
    done = adsrHandle(&adsr, &start, &delta);
    if (oldDelta == delta) {
      if (curDelta != 0)
        ++upd;
    } else {
      upd += 2;
      oldDelta = delta;
    }
  }
  *cycles += budgetMixerCycles[mc & 31] + 0x4FE;
  *cycles += budgetSRCCycles[sh->ratioHi < 2 ? sh->ratioHi : 2][sh->srcSelect < 3 ? sh->srcSelect : 2];
  *cycles += upd * 4;
  sh->mixerCtrl = mc;
  if (ratioUpdate)
    sh->ratioHi = nextRatio; /* the DSP applies the frame's pitch updates as it runs */
  return 1;
}

static void budgetCut(DSPvoice *vp) { /* the console: salDeactivateVoice, then the kill message */
  voiceEndWhy = "budget";
  salDeactivateVoice(vp);
  voiceEndWhy = NULL;
  salSynthSendMessage(vp, HW_MESSAGE_VOICE_KILL);
}

static void budgetPass(void) {
  static int on = -1;
  DSPvoice *list[BUDGET_VOICES];
  u32 cycles, cut = 0;
  u8 st;
  if (on < 0) {
    const char *env = getenv("ORRE_DSP_BUDGET");
    on = !(env != NULL && strcmp(env, "0") == 0);
  }
  if (!on)
    return;
  cycles = 10430 + (dspHRTFOn ? 45000 : 0);
  for (st = 0; st < salMaxStudioNum; ++st) {
    DSPstudioinfo *stp = &dspStudio[st];
    DSPvoice *vp, *next;
    long n = 0, v;
    if (stp->state != DSP_STUDIO_STATE_ACTIVE)
      continue;
    if (cut != 0) { /* a later studio: every voice goes */
      for (vp = stp->voiceRoot; vp != NULL; vp = next) {
        next = vp->next;
        budgetCut(vp);
        ++cut;
      }
      continue;
    }
    cycles += 0x2C62 + (u32)stp->numInputs * 0x294D;
    for (vp = stp->voiceRoot; vp != NULL && n < BUDGET_VOICES; vp = vp->next) {
      /* the console ends these breaks before sorting (the render loop below does the same) */
      if ((vp->postBreak || (vp->changed[0] & 0x20)) &&
          (vp->state != DSP_VOICE_STATE_STARTING || vp->startupBreak))
        continue;
      list[n++] = vp;
    }
    budgetSortVoices(list, 0, n - 1);
    for (v = n; v > 0; --v) {
      DSPvoice *bv = list[v - 1];
      const u32 idx = (u32)(bv - dspVoice);
      if (bv->state == DSP_VOICE_STATE_INACTIVE || idx >= BUDGET_VOICES)
        continue;
      if (!budgetVoice(stp, bv, &budgetShadow[idx], &cycles))
        continue;
      if (cycles > BUDGET_CYCLES) {
        static int soundLog = -1;
        if (soundLog < 0)
          soundLog = getenv(ORRE_SOUND_LOG_ENV) != NULL;
        if (soundLog)
          fprintf(stderr, "[sndvoice] t=%.4f budget: %u cycles of %u at voice %u (prio %u); %ld voice(s) cut\n",
                  salPCCurrentSeconds(), (unsigned)cycles, BUDGET_CYCLES, idx, (unsigned)bv->prio, v);
        for (; v > 0; --v) {
          budgetCut(list[v - 1]);
          ++cut;
        }
        break;
      }
    }
  }
}

void salCtrlDsp(s16 *dest) {
  u8 st;
  DSPstudioinfo *stp;

  renderOffset = 0;
  memset(mixBufferL, 0, sizeof(mixBufferL));
  memset(mixBufferR, 0, sizeof(mixBufferR));
  memset(mixBufferRear, 0, sizeof(mixBufferRear));
  budgetPass();

  for (st = 0, stp = dspStudio; st < salMaxStudioNum; ++st, ++stp) {
    if (stp->state != DSP_STUDIO_STATE_ACTIVE)
      continue;

    memset(rearMain[st][salFrame], 0, sizeof(rearMain[st][salFrame]));

    /* Clear the current frame buffers only. Previous-frame data feeds studio inputs. */
    if (stp->main[salFrame])
      memset(stp->main[salFrame], 0, SAL_SAMPLES_PER_FRAME * 3 * sizeof(s32));

    /* Clear AUX buffers for the current aux frame */
    if (stp->auxA[salAuxFrame])
      memset(stp->auxA[salAuxFrame], 0, SAL_SAMPLES_PER_FRAME * 3 * sizeof(s32));
    if (stp->auxB[salAuxFrame])
      memset(stp->auxB[salAuxFrame], 0, SAL_SAMPLES_PER_FRAME * 3 * sizeof(s32));

    /* Render all voices in this studio */
    DSPvoice *vp = stp->voiceRoot;
    while (vp != NULL) {
      renderOffset = 0;
      DSPvoice *nextVp = vp->next; /* save in case voice is deactivated */
      if (vp->state != DSP_VOICE_STATE_INACTIVE) {
        u32 voiceIdx = (u32)(vp - dspVoice);
        u8 mixStart = 0;

        /* Breaks end the previous generation. Reusing an active voice carries
         * the break bit into a new startup, which must not release the new note. */
        if (vp->postBreak || (vp->changed[0] & 0x20)) {
          if (vp->state != DSP_VOICE_STATE_STARTING || vp->startupBreak) {
            voiceEndWhy = "break";
            salDeactivateVoice(vp);
            vp->startupBreak = 0;
            vp = nextVp;
            continue;
          }
          vp->changed[0] &= ~0x20u;
        }

        /* New voice initialization (mirrors salBuildCommandList startup path) */
        if (vp->state == DSP_VOICE_STATE_STARTING) {
          if (adsrSetup(&vp->adsr) != 0) {
            salSynthSendMessage(vp, HW_MESSAGE_SAMPLE_END);
            salDeactivateVoice(vp);
            vp = nextVp;
            continue;
          }

          vp->virtualSampleID = UINT32_MAX;
          if (vp->smp_info.compType == SAMPLE_TYPE_ADPCM_VIRTUAL) {
            vp->vSampleInfo.loopBufferAddr = NULL;
            vp->vSampleInfo.loopBufferLength = 0;
            vp->vSampleInfo.inLoopBuffer = 0;
            vp->virtualSampleID = salSynthSendMessage(vp, HW_MESSAGE_VIRTUAL_SAMPLE_START);
            if (!vp->vSampleInfo.loopBufferAddr || !vp->vSampleInfo.loopBufferLength) {
              salSynthSendMessage(vp, HW_MESSAGE_VOICE_KILL);
              salDeactivateVoice(vp);
              vp = nextVp;
              continue;
            }
          }

          vp->lastVolL = vp->volL;
          vp->lastVolR = vp->volR;
          vp->lastVolS = vp->volS;
          vp->lastVolLa = vp->volLa;
          vp->lastVolRa = vp->volRa;
          vp->lastVolSa = vp->volSa;
          vp->lastVolLb = vp->volLb;
          vp->lastVolRb = vp->volRb;
          vp->lastVolSb = vp->volSb;
          vp->lastVolRear[0] = vp->volRear[0];
          vp->lastVolRear[1] = vp->volRear[1];

          VoiceResamplerState *native = &voiceResampler[voiceIdx];
          memset(native, 0, sizeof(*native));
          native->itdDelay[0] = itdTarget(vp, 0);
          native->itdDelay[1] = itdTarget(vp, 1);
          if (!salPCInitReader(&native->reader, &vp->smp_info)) {
            salSynthSendMessage(vp, HW_MESSAGE_SAMPLE_END);
            salDeactivateVoice(vp);
            vp = nextVp;
            continue;
          }
          vp->playInfo.posHi = native->reader.position;
          vp->playInfo.posLo = 0;
          vp->playInfo.pitch = 0;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
          filterState[voiceIdx] = 0;
#endif
          mixStart = vp->singleOffset;
          vp->state = DSP_VOICE_STATE_PLAYING;
          renderOffset = segmentOffset[mixStart];
          { /* OrrePort ORRE_SOUND_LOG=1 (2026-10-18): one line per voice start, for per-sound level comparisons with
             * Dolphin (snatcherLeo: cries and the Poke Ball sound sound quiet). t = mixer time in seconds (the audio dump
             * starts at "[audio] dump starts at mixer t=..."); volumes as the game set them (0x7FFF = full) */
            static int soundLog = -1;
            if (soundLog < 0)
              soundLog = getenv(ORRE_SOUND_LOG_ENV) != NULL;
            if (soundLog) {
              const u32 rate = salPCMixRate();
              /* pslot / pnow (2026-10-28, the voices probe): hwSetPitch writes pitch[salTimeOffset] and keeps that
               * slot in lastUpdate.pitch (0xFF = none yet), so pitch[0] is 0 for most voices; pnow is the pitch in the
               * slot last written (the console probe's columns of the same names) */
              const int pslot = vp->lastUpdate.pitch < 5 ? (int)vp->lastUpdate.pitch : -1;
              const u32 pnow = pslot >= 0 ? vp->pitch[pslot] : vp->pitch[0];
              fprintf(stderr,
                      "[sndvoice] t=%.4f voice=%u smp=%u studio=%u comp=%u len=%u loop=%u+%u pitch=0x%X itd=%u "
                      "vol=%d/%d/%d auxA=%d/%d/%d auxB=%d/%d/%d pslot=%d pnow=0x%X\n",
                      (double)(salPCBoundary(cycleTick, rate) + renderOffset) / rate, voiceIdx, vp->smp_id, vp->studio,
                      vp->smp_info.compType, vp->smp_info.length, vp->smp_info.loop, vp->smp_info.loopLength,
                      vp->pitch[0], native->itdDelay[0] || native->itdDelay[1],
                      (s16)vp->volL, (s16)vp->volR, (s16)vp->volS, (s16)vp->volLa, (s16)vp->volRa, (s16)vp->volSa,
                      (s16)vp->volLb, (s16)vp->volRb, (s16)vp->volSb, pslot, (unsigned)pnow);
            }
          }
        }

        /* Get studio main buffer pointers */
        s32 *studioMain = stp->main[salFrame];
        s32 *studioL = studioMain;
        s32 *studioR = studioMain ? studioMain + SAL_SAMPLES_PER_FRAME : NULL;
        s32 *studioS = studioMain ? studioMain + SAL_SAMPLES_PER_FRAME * 2 : NULL;

        /* Get AUX buffer pointers */
        s32 *auxACur = stp->auxA[salAuxFrame];
        s32 *auxAL = auxACur;
        s32 *auxAR = auxACur ? auxACur + SAL_SAMPLES_PER_FRAME : NULL;
        s32 *auxAS = auxACur ? auxACur + SAL_SAMPLES_PER_FRAME * 2 : NULL;
        s32 *auxBCur = stp->auxB[salAuxFrame];
        s32 *auxBL = auxBCur;
        s32 *auxBR = auxBCur ? auxBCur + SAL_SAMPLES_PER_FRAME : NULL;
        s32 *auxBS = auxBCur ? auxBCur + SAL_SAMPLES_PER_FRAME * 2 : NULL;

        if (studioMain != NULL) {
          int finished = 0;

          for (u8 subframe = mixStart; subframe < SAL_SUBFRAMES; ++subframe) {
            renderOffset = segmentOffset[subframe];
            if ((vp->changed[subframe] & 0x20) != 0) {
              vp->postBreak = 1;
              voiceEndWhy = "break";
              salDeactivateVoice(vp);
              finished = 1;
              break;
            } else if (vp->postBreak == 0) {
              if ((vp->changed[subframe] & 0x40) != 0) {
                adsrRelease(&vp->adsr);
                { /* OrrePort ORRE_SOUND_LOG (2026-10-29, the voices pairing): the key-off, when the envelope enters its
                   * release, "[sndvoice] t=... release voice=N smp=S env=0x...", to set against the console probe's adsr
                   * rows and tell a late release from a release that runs at another speed */
                  static int soundLog = -1;
                  if (soundLog < 0)
                    soundLog = getenv(ORRE_SOUND_LOG_ENV) != NULL;
                  if (soundLog) {
                    const u32 rate = salPCMixRate();
                    fprintf(stderr, "[sndvoice] t=%.4f release voice=%u smp=%u env=0x%X\n",
                            (double)(salPCBoundary(cycleTick, rate) + renderOffset) / rate, voiceIdx, vp->smp_id,
                            (unsigned)vp->adsr.currentVolume);
                  }
                }
              }
              if ((vp->changed[subframe] & 8) != 0)
                vp->playInfo.pitch = vp->pitch[subframe];
            }

            if ((vp->changed[subframe] & 0x10) != 0) {
              if (adsrSetup(&vp->adsr) != 0) {
                salSynthSendMessage(vp, HW_MESSAGE_SAMPLE_END);
                salDeactivateVoice(vp);
                finished = 1;
                break;
              }
            }

            u16 adsrStart = 0;
            u16 adsrDelta = 0;
            u32 adsrDone = adsrHandle(&vp->adsr, &adsrStart, &adsrDelta);
            int sampleOffset = segmentOffset[subframe];
            int sampleCount = segmentOffset[subframe + 1] - sampleOffset;
            u16 adsrEnd = CLAMP(vp->adsr.currentVolume >> 16, 0, 0x7fff);

            int sampleDone =
                renderVoiceSegment(vp, studioL, studioR, studioS, auxAL, auxAR, auxAS, auxBL, auxBR,
                                   auxBS, voiceIdx, sampleOffset, sampleCount, adsrStart, adsrEnd);
            renderOffset = segmentOffset[subframe + 1];
            if (sampleDone) {
              salSynthSendMessage(vp, HW_MESSAGE_SAMPLE_END);
              voiceEndWhy = "sample";
              salDeactivateVoice(vp);
              finished = 1;
              break;
            }

            if (adsrDone) {
              salSynthSendMessage(vp, HW_MESSAGE_SAMPLE_END);
              voiceEndWhy = "adsr";
              salDeactivateVoice(vp);
              finished = 1;
              break;
            }
          }

          vp->lastVolL = vp->volL;
          vp->lastVolR = vp->volR;
          vp->lastVolS = vp->volS;
          vp->lastVolLa = vp->volLa;
          vp->lastVolRa = vp->volRa;
          vp->lastVolSa = vp->volSa;
          vp->lastVolLb = vp->volLb;
          vp->lastVolRb = vp->volRb;
          vp->lastVolSb = vp->volSb;
          vp->lastVolRear[0] = vp->volRear[0];
          vp->lastVolRear[1] = vp->volRear[1];

          voiceResampler[voiceIdx].itdDelay[0] = itdTarget(vp, 0);
          voiceResampler[voiceIdx].itdDelay[1] = itdTarget(vp, 1);

          if (finished) {
            vp = nextVp;
            continue;
          }
        }
      }
      vp = nextVp;
    }

    for (u32 channel = 0; channel < 11; ++channel) {
      s32 *buffer = channel < 3   ? stp->main[salFrame]
                    : channel < 6 ? stp->auxA[salAuxFrame]
                                  : stp->auxB[salAuxFrame];
      if (channel >= 9)
        buffer = rearMain[st][salFrame][channel - 9];
      else
        buffer += (channel % 3) * SAL_PC_MAX_FRAMES;
      for (u32 frame = 0; frame < cycleFrames; ++frame) {
        u32 index = (depopCursor + frame) & (DEPOP_RING - 1);
        buffer[frame] += depop[st][channel][index];
        depop[st][channel][index] = 0;
      }
    }

    if (stp->main[salFrame] != NULL) {
      s32 *studioMain = stp->main[salFrame];
      s32 *auxACur = stp->auxA[salAuxFrame];
      s32 *auxBCur = stp->auxB[salAuxFrame];
      for (u8 inputIdx = 0; inputIdx < stp->numInputs; ++inputIdx) {
        DSPinput *input = &stp->in[inputIdx];
        DSPstudioinfo *srcStudio = &dspStudio[input->studio];
        s32 *srcMain = srcStudio->main[salFrame ^ 1];
        if (srcMain == NULL)
          continue;

        u32 rate = salPCMixRate();
        u64 previousTick = cycleTick >= 5 ? cycleTick - 5 : 0;
        u32 previousFrames =
            cycleTick ? (u32)(salPCBoundary(cycleTick, rate) - salPCBoundary(previousTick, rate))
                      : cycleFrames;
        for (u32 channel = 0; channel < 5; ++channel) {
          const s32 *source = channel < 3 ? srcMain + channel * SAL_SAMPLES_PER_FRAME
                                          : rearMain[input->studio][salFrame ^ 1][channel - 3];
          if (previousFrames == cycleFrames && rate % 200 == 0) {
            memcpy(adaptedWork[channel], source, cycleFrames * sizeof(s32));
          } else {
            salPCConvertStudioCycle(adaptedWork[channel], cycleFrames, rate, cycleTick, source,
                                    previousFrames, rate, previousTick,
                                    studioHistory[st][inputIdx][channel]);
          }
          u32 base = channel * SAL_SAMPLES_PER_FRAME;
          for (u32 frame = 0; frame < cycleFrames; ++frame) {
            s32 value = adaptedWork[channel][frame];
            if (channel < 3) {
              studioMain[base + frame] += (s32)((s64)value * input->vol >> 15);
            } else {
              rearMain[st][salFrame][channel - 3][frame] += (s32)((s64)value * input->vol >> 15);
              value = (s32)((s64)value * SURROUND_DOWNMIX_GAIN >> 15);
              base = 2 * SAL_PC_MAX_FRAMES;
            }
            auxACur[base + frame] += (s32)((s64)value * input->volA >> 15);
            auxBCur[base + frame] += (s32)((s64)value * input->volB >> 15);
          }
        }
      }
    }

    /* Legacy callbacks always receive three 160-sample, 32 kHz buffers. The
     * previous cycle is processed, preserving the existing triple rotation. */
    for (u32 bus = 0; bus < 2; ++bus) {
      SND_AUX_CALLBACK callback = bus ? stp->auxBHandler : stp->auxAHandler;
      { /* OrrePort test switch ORRE_NO_REVERB=1: skip the aux effects (reverb), to measure their share of the mix */
        static int noReverb = -1;
        if (noReverb < 0)
          noReverb = getenv("ORRE_NO_REVERB") != NULL;
        if (noReverb)
          continue;
      }
      if (!callback || (bus && stp->type == SND_STUDIO_TYPE_DPL2))
        continue;
      s32 *work = (bus ? stp->auxB : stp->auxA)[(salAuxFrame + 2) % 3];
      u32 rate = salPCMixRate();
      u64 previousTick = cycleTick >= 5 ? cycleTick - 5 : 0;
      u32 previousFrames =
          cycleTick ? (u32)(salPCBoundary(cycleTick, rate) - salPCBoundary(previousTick, rate))
                    : cycleFrames;
      for (u32 channel = 0; channel < 3; ++channel) {
        const s32 *source = work + channel * SAL_SAMPLES_PER_FRAME;
        if (rate == 32000)
          memcpy(legacyWork[channel], source, sizeof(legacyWork[channel]));
        else
          salPCConvertCycle(legacyWork[channel], 160, 32000, previousTick, source, previousFrames,
                            rate, previousTick, effectHistory[st][bus][0][channel]);
      }
      SND_AUX_INFO info;
      info.data.bufferUpdate.left = legacyWork[0];
      info.data.bufferUpdate.right = legacyWork[1];
      info.data.bufferUpdate.surround = legacyWork[2];
      callback(SND_AUX_REASON_BUFFERUPDATE, &info, bus ? stp->auxBUser : stp->auxAUser);
      for (u32 channel = 0; channel < 3; ++channel) {
        if (rate == 32000)
          memcpy(adaptedWork[channel], legacyWork[channel], sizeof(legacyWork[channel]));
        else
          salPCConvertCycle(adaptedWork[channel], cycleFrames, rate, cycleTick, legacyWork[channel],
                            160, 32000, cycleTick, effectHistory[st][bus][1][channel]);
        s32 *main = stp->main[salFrame] + channel * SAL_SAMPLES_PER_FRAME;
        for (u32 frame = 0; frame < cycleFrames; ++frame)
          main[frame] += adaptedWork[channel][frame];
      }
    }

    /* Accumulate master studio into final mix */
    if (stp->isMaster) {
      s32 *studioMain = stp->main[salFrame];
      if (studioMain != NULL) {
        s32 *studioL = studioMain;
        s32 *studioR = studioMain + SAL_SAMPLES_PER_FRAME;
        s32 *studioS = studioMain + SAL_SAMPLES_PER_FRAME * 2;
        mixStudioOutput(studioL, studioR,
                        stp->type == SND_STUDIO_TYPE_DPL2 && salPCChannels() == 2 ? NULL : studioS);
        for (u32 channel = 0; channel < 2; ++channel)
          for (u32 frame = 0; frame < cycleFrames; ++frame)
            mixBufferRear[channel][frame] += rearMain[st][salFrame][channel][frame];
      }

      if (stp->type == SND_STUDIO_TYPE_DPL2 && salPCChannels() == 2) {
        s32 *dpl2Rear = stp->auxB[salAuxFrame];
        if (dpl2Rear != NULL)
          foldStereoToOutput(dpl2Rear, dpl2Rear + SAL_SAMPLES_PER_FRAME);
      }
    }
  }

  depopCursor = (depopCursor + cycleFrames) & (DEPOP_RING - 1);
  renderOffset = 0;

  if (dest) {
    u32 channels = salPCChannels();
    for (u32 i = 0; i < cycleFrames; ++i) {
      s16 *frame = dest + i * channels;
      frame[0] = clamp16(mixBufferL[i]);
      frame[1] = clamp16(mixBufferR[i]);
      if (channels > 2) {
        u32 rear = channels == 4 ? 2 : 4;
        s32 left = mixBufferRear[0][i], right = mixBufferRear[1][i];
        if (channels >= 6)
          frame[2] = frame[3] = 0;
        if (channels == 8) {
          left = (s32)((s64)left * SURROUND_DOWNMIX_GAIN >> 15);
          right = (s32)((s64)right * SURROUND_DOWNMIX_GAIN >> 15);
          frame[6] = clamp16(left);
          frame[7] = clamp16(right);
        }
        frame[rear] = clamp16(left);
        frame[rear + 1] = clamp16(right);
      }
    }
  }
}

bool salInitAi(SND_SOME_CALLBACK callback, u32 flags, u32 *outFreq) {
  (void)flags;
  salPCResetOutput(callback);
  memset(voiceResampler, 0, sizeof(voiceResampler));
  memset(lastContribution, 0, sizeof(lastContribution));
  memset(depop, 0, sizeof(depop));
  depopCursor = renderOffset = 0;
  initResampleTables();
  synthInfo.numSamples = 32;
  *outFreq = salPCMixRate();
  return true;
}

bool salInitDsp(u32 flags) {
  (void)flags;
  return true;
}
bool salExitDsp(void) { return true; }
void salStartDsp(u16 *cmdList) { (void)cmdList; }

bool salInitDspCtrl(u8 voices, u8 studios, u32 dpl2) {
  salNumVoices = voices;
  salMaxStudioNum = studios;
  memset(dspStudio, 0, sizeof(dspStudio));
  dspVoice = salPCHostMalloc(voices * sizeof(*dspVoice));
  if (!dspVoice)
    return false;
  memset(dspVoice, 0, voices * sizeof(*dspVoice));
  for (u32 i = 0; i < voices; ++i) {
    dspVoice[i].virtualSampleID = 0xffffffff;
    memset(&dspVoice[i].lastUpdate, 0xff, sizeof(dspVoice[i].lastUpdate));
  }
  for (u32 i = 0; i < studios; ++i) {
    DSPstudioinfo *studio = &dspStudio[i];
    studio->main[0] = salPCHostMalloc(SAL_PC_STUDIO_BYTES);
    if (!studio->main[0]) {
      salExitDspCtrl();
      return false;
    }
    memset(studio->main[0], 0, SAL_PC_STUDIO_BYTES);
    studio->main[1] = studio->main[0] + 3 * SAL_SAMPLES_PER_FRAME;
    for (u32 j = 0; j < 3; ++j) {
      studio->auxA[j] = studio->main[0] + (2 + j) * 3 * SAL_SAMPLES_PER_FRAME;
      studio->auxB[j] = studio->main[0] + (5 + j) * 3 * SAL_SAMPLES_PER_FRAME;
    }
  }
  salActivateStudio(0, 1, dpl2 ? SND_STUDIO_TYPE_DPL2 : SND_STUDIO_TYPE_STD);
  return true;
}

bool salExitDspCtrl(void) {
  for (u32 i = 0; i < salMaxStudioNum; ++i) {
    if (dspStudio[i].main[0])
      salPCHostFree(dspStudio[i].main[0]);
  }
  memset(dspStudio, 0, sizeof(dspStudio));
  if (dspVoice)
    salPCHostFree(dspVoice);
  dspVoice = NULL;
  return true;
}

/* The native renderer owns studio storage and rate-adapter history. Keep the
 * shared sal* boundary, with each backend defining its own implementation. */
void salActivateStudio(u8 studio, u32 isMaster, SND_STUDIO_TYPE type) {
  DSPstudioinfo *state = &dspStudio[studio];
  memset(state->main[0], 0, SAL_PC_STUDIO_BYTES);
  resetStudioHistory(studio);
  memset(rearMain[studio], 0, sizeof(rearMain[studio]));
  memset(depop[studio], 0, sizeof(depop[studio]));
  state->voiceRoot = NULL;
  state->alienVoiceRoot = NULL;
  state->state = DSP_STUDIO_STATE_ACTIVE;
  state->isMaster = isMaster;
  state->numInputs = 0;
  state->type = type;
  state->auxAHandler = state->auxBHandler = NULL;
  state->auxAUser = state->auxBUser = NULL;
}

/* Hardware HRTF storage is not used by the native panner. */
void salInitHRTFBuffer(void) {}

/* Native teardown also retires virtual instances, including voice stealing and
 * explicit hwOff. Unlink first so a STOP callback can safely reenter APIs. */
void salDeactivateVoice(DSPvoice *voice) {
  if (voice->state == DSP_VOICE_STATE_INACTIVE)
    return;
  { /* ORRE_SOUND_LOG: when a voice stops (the start line has the same voice number), for sounds that end too early */
    static int soundLog = -1;
    if (soundLog < 0)
      soundLog = getenv(ORRE_SOUND_LOG_ENV) != NULL;
    if (soundLog)
      fprintf(stderr, "[sndvoice] t=%.4f end voice=%u smp=%u why=%s env=0x%X\n", salPCCurrentSeconds(),
              (u32)(voice - dspVoice), voice->smp_id, voiceEndWhy ? voiceEndWhy : "off",
              (unsigned)voice->adsr.currentVolume);
    voiceEndWhy = NULL;
  }
  retireContribution(voice);
  if (voice->prev)
    voice->prev->next = voice->next;
  else
    dspStudio[voice->studio].voiceRoot = voice->next;
  if (voice->next)
    voice->next->prev = voice->prev;
  voice->state = DSP_VOICE_STATE_INACTIVE;
  if (voice->virtualSampleID != UINT32_MAX) {
    salSynthSendMessage(voice, HW_MESSAGE_VIRTUAL_SAMPLE_END);
    voice->virtualSampleID = UINT32_MAX;
  }
  voice->vSampleInfo.inLoopBuffer = 0;
}

void salReconnectVoice(DSPvoice *voice, u8 studio) {
  if (voice->studio == studio)
    return;
  if (voice->state != DSP_VOICE_STATE_INACTIVE) {
    if (voice->state == DSP_VOICE_STATE_PLAYING) {
      /* The old studio owns its detached tail. The live decoder, envelope and
       * virtual instance continue in the new studio with a 5 ms gain ramp. */
      retireContribution(voice);
      voice->lastVolL = voice->lastVolR = voice->lastVolS = 0;
      voice->lastVolLa = voice->lastVolRa = voice->lastVolSa = 0;
      voice->lastVolLb = voice->lastVolRb = voice->lastVolSb = 0;
      voice->lastVolRear[0] = voice->lastVolRear[1] = 0;
    }
    if (voice->prev)
      voice->prev->next = voice->next;
    else
      dspStudio[voice->studio].voiceRoot = voice->next;
    if (voice->next)
      voice->next->prev = voice->prev;
    voice->next = dspStudio[studio].voiceRoot;
    if (voice->next)
      voice->next->prev = voice;
    voice->prev = NULL;
    dspStudio[studio].voiceRoot = voice;
  }
  voice->studio = studio;
}
