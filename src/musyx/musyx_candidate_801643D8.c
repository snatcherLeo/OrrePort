/** Exact standalone owner for 0x801643D8 - 0x80164488 (hw_dolphin.c ends at
 * 0x801643D8). */
#include "dolphin/types.h"
#ifdef TARGET_PC
/* PORT: typed versions for the PC (docs/port/musyx-evaluation.md, "Step 2"). On a 64-bit PC SND_HOOKS holds two
 * 8-byte pointers and the aux callback receives real buffer pointers, so the GameCube code's u32-array reads pick up
 * pointer halves: salFree called callbacks[1], the upper half of the malloc pointer, and jumped to address 0. */
#include "musyx/runtime/hooks_types.h"
#include "musyx/runtime/reverb.h"

extern void ReverbHICallback(s32* left, s32* right, s32* surround, SND_AUX_REVERBHI* rev);
extern SND_HOOKS lbl_8047B054; /* salHooks */
typedef struct PcAuxBuffers {  /* SND_AUX_INFO.data.bufferUpdate */
    s32* left;
    s32* right;
    s32* surround;
} PcAuxBuffers;

u32 salMalloc(u32 size) { /* salMalloc */
    return PC_PTR32(lbl_8047B054.malloc(size));
}

void salFree(u32 allocation) { /* salFree */
    lbl_8047B054.free(PC_PTR(allocation));
}

void sndAuxCallbackReverbHI(u8 type, u32* data, u8* work) {
    PcAuxBuffers* buffers = (PcAuxBuffers*)data;
    SND_AUX_REVERBHI* rev = (SND_AUX_REVERBHI*)work;

    switch (type) {
    case 0:
        if (rev->tempDisableFX != 0) { /* work[0x1C4] on the GameCube */
            break;
        }
        ReverbHICallback(buffers->left, buffers->right, buffers->surround, rev);
        break;
    case 1:
        break;
    }
}
#else /* GameCube: unchanged */

extern void ReverbHICallback(u32 left, u32 right, u32 surround, void* work);
extern u32 lbl_8047B054;

u32 salMalloc(u32 size) {
    return ((u32 (*)(u32))PC_PTR(lbl_8047B054))(size);
}

void salFree(u32 allocation) {
    u32* callbacks = &lbl_8047B054;

    ((void (*)(u32))PC_PTR(callbacks[1]))(allocation);
}

void sndAuxCallbackReverbHI(u8 type, u32* data, u8* work) {
    switch (type) {
    case 0:
        if (work[0x1C4] != 0) {
            break;
        }
        ReverbHICallback(data[0], data[1], data[2], work);
        break;
    case 1:
        break;
    }
}
#endif /* TARGET_PC */
