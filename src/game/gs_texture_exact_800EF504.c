/**
 * @file gs_texture_exact_800EF504.c
 * @brief Exact texture image unlock, 0x800EF504 - 0x800EF548.
 */

#include "dolphin/types.h"
#include "dolphin/os/OSCache.h"
#include "game/gs_texture.h"

extern void GXInvalidateTexAll(void);

#ifdef TARGET_PC
void GXInitTexObjData(GXTexObj* obj, const void* data);
void* GXGetTexObjData(const GXTexObj* obj);
#endif

void GStextureUnlockImage(GStextureHandle* tex)
{
    DCFlushRange(P32(tex->mipData[0]), tex->totalSize);
    GXInvalidateTexAll();
#ifdef TARGET_PC
    /* The texels were written by the CPU between lock and unlock. Aurora's GXInvalidateTexAll does nothing and its
     * texture cache hits on (texture object, data version), so give the object a new version (same pointer) and the
     * next load re-reads it. Without this the message atlas kept the first message's letters (2026-10-16). */
    {
        GXTexObj* obj = (GXTexObj*)GS_TEX_OBJ(tex);
        GXInitTexObjData(obj, GXGetTexObjData(obj));
    }
#endif

    tex->refCount--;
}
