#include "game/gs_texture.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif

void GStextureFree(GStextureHandle* texture)
{
    if (texture->inUse == 0) {
        return;
    }
    if (texture->memHandle == 0) {
        return;
    }
    texture->inUse = 0;
#ifdef TARGET_PC
    {
        /* PC (2026-11-05, port-gaps 67, the P09 out-of-GPU-memory crash): on the GameCube an EFB copy
         * (GStextureConvertFromHW) writes into this texture's own memory, so freeing the memory ends the copy too. The
         * port keeps each copy as a GPU texture keyed by its destination address and never learnt the memory was
         * freed: every texture ever copied into stayed on the card (1.1 GB over P09), and a later texture loaded at
         * the same address could be drawn from the stale copy. Tell the renderer the copy is gone. */
        extern void GXDestroyCopyTex(void* dest);
        GXDestroyCopyTex(P32(texture->mipData[0]));
    }
#endif
    fn_800E24B0(texture->memHandle);
    GSmemFree(texture->memHandle);
}
