#include "dolphin/types.h"
#include "musyx/runtime/reverb.h"

extern u32 ReverbHICreate(_SND_REVHI_WORK* obj, f32 f1, f32 f2, f32 f3, f32 f4,
                          f32 f5, f32 f6);

#ifdef TARGET_PC
/* PORT: typed for the PC: the fixed offsets below (0x1C4 tempDisableFX, 0x1C8 coloration ... 0x1DC crosstalk) are the
 * GameCube layout; SND_AUX_REVERBHI holds pointer arrays, so the fields sit further on in a 64-bit build. */
void sndAuxCallbackPrepareReverbHI(u8* ptr)
{
    SND_AUX_REVERBHI* rev = (SND_AUX_REVERBHI*)ptr;

    rev->tempDisableFX = 0;
    ReverbHICreate(&rev->rv, rev->coloration, rev->time, rev->mix, rev->damping, rev->preDelay, rev->crosstalk);
}
#else
void sndAuxCallbackPrepareReverbHI(u8* ptr)
{
    ptr[0x1C4] = 0;
    ReverbHICreate(
        (_SND_REVHI_WORK*)ptr,
        *(f32*)(ptr + 0x1C8),
        *(f32*)(ptr + 0x1D0),
        *(f32*)(ptr + 0x1CC),
        *(f32*)(ptr + 0x1D4),
        *(f32*)(ptr + 0x1D8),
        *(f32*)(ptr + 0x1DC)
    );
}
#endif
