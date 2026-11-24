/**
 * @file gs_npc_event_candidate_8003037C_r40_80030C14.c
 * @brief fn_80030C14 carve, 0x80030C14 - 0x80030D34.
 *
 * NPC event callback: draw trainer model A (event 0x10CC) or B (0x10CD)
 * over the sprite, like fn_800301B0 does for events 0x10CE/0x10CF.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(s32 layer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(s32 layer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(void* resource);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxLoadTexture(s32 slot, void* model);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(s32 blendMode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos2s16(s32 x, s32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(s32 slot, s32 r, s32 g, s32 b, s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxTexCoord(s32 slot, f32 scaleX, f32 scaleY);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif
extern void* menuModelRender(void* data);

extern u8 lbl_803A3230[];
extern u8 lbl_803A31E8[];
extern u8 lbl_80314F98[];
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
extern f32 lbl_8047B9D4;
#endif
extern f32 lbl_8047B9F0;

void fn_80030C14(void* r3, u8* r4)
{
    s32 evtype = *(s16*)(r4 + 0x6);
    void* model = NULL;

    switch (evtype) {
    case 0x10CC:
        model = menuModelRender(lbl_803A3230);
        break;
    case 0x10CD:
        model = menuModelRender(lbl_803A31E8);
        break;
    }
    if (model != NULL) {
        gfxSetFlags(3);
        gfxClearFlags(4);
        gfxSetPrimitive(7);
        gfxSetVtxDesc(lbl_80314F98);
        gfxLoadTexture(0, model);
        gfxBegin(2);
        gfxVtxPos2s16(0, 0);
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxTexCoord(0, lbl_8047B9D4, lbl_8047B9D4);
        gfxVtxPos2s16(*(s16*)(r4 + 0x54), *(s16*)(r4 + 0x56));
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxTexCoord(0, lbl_8047B9F0, lbl_8047B9F0);
        gfxEnd();
    }
}
