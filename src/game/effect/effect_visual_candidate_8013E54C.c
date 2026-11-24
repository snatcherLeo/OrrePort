#include "crt/string.h"

extern void GXDrawDone(void);
extern void GXInvalidateVtxCache(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GStextureFree(void* texture);
#endif

u32 _auraDestroy(void* arg)
{
    void* ptr;

    if (arg != 0) {
        ptr = arg;
#ifdef TARGET_PC
        /* PC: the texture is a 32-bit slot of the aura work record (_auraInit stores it so, effect_visual.c) */
        if (PTR32_AT_T(void, (u8*)ptr, 4) != 0) {
            GXDrawDone();
            GXInvalidateVtxCache();
            GStextureFree(PTR32_AT_T(void, (u8*)ptr, 4));
        }
#else
        if (*(void**)((u8*)ptr + 4) != 0) {
            GXDrawDone();
            GXInvalidateVtxCache();
            GStextureFree(*(void**)((u8*)ptr + 4));
        }
#endif
        memset(ptr, 0, 0x34);
    }
    return 1;
}
