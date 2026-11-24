/*
 * musyx_pc_voiceclass.c: what kind of sound a MusyX voice is playing, for the sound split prototype (2026-11-16;
 * docs/port/sound-split.md). Observe only: reads the synth's own voice table, changes nothing.
 *
 * Why: with the sound split on, the game's own MusyX instance keeps the console's 1x schedule and is heard only in
 * part (the movies' sound always; the sound effects in the "fast" mode), while a second copy plays the music at its
 * normal tempo. The PC mixer (AxioDL's hw_pc.c) asks, per voice and per 5 ms block, how loud to make it
 * (sndPCSetVoiceGain); this answers the question it needs: music, sound effect or stream.
 *   - a stream (the THP movies' sound) holds its voice with voiceBlock, which sets block = 1 (synthvoice.c);
 *   - a sound effect (sndFXStartEx, an emitter) runs with fxFlag set, the same flag the game's own effects / music
 *     volume faders (groups 22 / 21) use in synth.c;
 *   - everything else is music (the songs' voices, including the jingles and fanfares the game plays as songs).
 *
 * Compiled with our runtime's headers (the voice layout is ours, not AxioDL's), into both MusyX copies; the audible
 * copy's is renamed aud_pc_musyx_voice_class like the rest of that copy (port/tools/musyx_prefix.py).
 * Platforms: plain C over the synth's data; the same on Windows, Linux and macOS.
 */
#include "dolphin/types.h"
#include "musyx/runtime/synth_voice.h"

extern SYNTH_VOICE* synthVoice; /* synth.c: synthInfo.voiceNum voices */
extern u8 salNumVoices;         /* the PC layer: the voices its mixer has, the same count */

/* 0 = music, 1 = sound effect, 2 = stream (a movie's sound). Out-of-range or before sndInit: music. */
int pc_musyx_voice_class(u32 voice) {
    if (synthVoice == NULL || voice >= salNumVoices) {
        return 0;
    }
    if (synthVoice[voice].block != 0) {
        return 2;
    }
    return synthVoice[voice].fxFlag != 0 ? 1 : 0;
}
