/**
 * @file window_exact_801040F0.c
 * @brief windowDrawSprite (0x801040F0 - 0x80104160).
 *
 * Draws a menu-sprite BIOS entry at its own size through windowDrawSprite2.
 * Window TU flags (GC/2.5, -O4,p, "-opt nopeephole"; see
 * window_exact_80104318.c). Carved so it can link while windowDrawSprite2
 * stays a candidate.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* menuSpriteBiosGetPtr(s32 id);
#endif
#ifdef TARGET_PC
/* PC: the real parameter types (window_exact_80104160.c): the 6th argument (the sprite context) is a pointer passed
 * on the stack on x86-64; declared s32 it was stored as 4 bytes into an 8-byte slot (upper half garbage). */
extern void windowDrawSprite2(s16 x, s16 y, s16 width, s16 height,
                              u32 color, void* context, s32 spriteId, u32 flags);
#else
extern void windowDrawSprite2(s16 x, s16 y, s16 width, s16 height,
                              s32 color, s32 context, s32 spriteId, u32 flags);
#endif

/* 0x801040F0 | 0x70 */
void windowDrawSprite(s16 x, s16 y, void* context, u32 spriteId, u32 flags) /* x, y: screen coordinates (callers convert with fctiwz) */
{
    if ((u16)spriteId != 0) {
        u8* sprite = menuSpriteBiosGetPtr((u16)spriteId);

        /* the sprite table (_menu_sprite) is disc data: big-endian sizes on the PC too */
        windowDrawSprite2(x, y, DISC_S16(sprite + 0x0C), DISC_S16(sprite + 0x0E),
#ifdef TARGET_PC
                          -1, context, spriteId, flags);
#else
                          -1, (s32)PC_PTR32(context), spriteId, flags);
#endif
    }
}
