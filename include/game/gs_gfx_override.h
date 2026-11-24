#ifndef GS_GFX_OVERRIDE_H
#define GS_GFX_OVERRIDE_H

#include "dolphin/types.h"

#include "game/gs_gfx_layer.h"

/*
 * Corrected 2026-10-15: the "graphics override" pool is the vertex-descriptor pool (lbl_8047AAAC, count
 * lbl_8047AAB0, 0x190 bytes each, allocated by gfxVtxDescPoolInit): gfxVtxDescAlloc takes a free descriptor and gives it the
 * next of eight GX vertex-format slots, gfxVtxDescSetAttr fills attribute idx, gfxLoadVtxDesc hands it to GX. The record
 * is GSVtxDesc / GSVtxAttr (gs_gfx_layer.h); its data member is a pointer (GXSetArray), so it does grow on a 64-bit
 * host (the earlier note said it didn't). These names stay as aliases.
 */
typedef GSVtxAttr GSgfxOverrideEntry;
typedef GSVtxDesc GSgfxOverride;

#endif /* GS_GFX_OVERRIDE_H */
