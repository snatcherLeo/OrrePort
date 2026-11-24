/**
 * @file gs_msg_exact_800FBF10.c
 * @brief GSmsgDaemon, 0x800FBF10 - 0x800FBF74.
 *
 * Function-boundary carve of the GSmsg TU (see gs_msg.c): flush the glyph
 * atlas being drawn and flip to the other one. No jump table, no pooled
 * constant; its only data is the .sdata message-system pointer
 * (_pMw), kept extern. GC/1.3 -O4,p with the TU's unit-wide
 * -opt nopeephole, no pragmas.
 */
#include "dolphin/types.h"
#include "game/gs_texture.h"

/* The message-system record (layout as in gs_msg.c; only the fields used
 * here are named). */
struct MessageSystem { /* GameCube layout on the PC too: pointer fields are 32-bit slots (pc/ptr32.h) */
    u16 taskCount;                   /* 0x00 */
    u16 taskHandle;                  /* 0x02 */
    u16 fontCount;                   /* 0x04 */
    u16 fontHandle;                  /* 0x06 */
    PTR32_FIELD(void) groups;                    /* 0x08 */
    PTR32_FIELD(GStextureHandle) textures[2];    /* 0x0C: double-buffered glyph atlas */
    PTR32_FIELD(void) image;                     /* 0x14: locked atlas image */
    s16 atlasX;                      /* 0x18: glyph atlas cursor */
    s16 atlasY;                      /* 0x1A */
    u8 unk1C;                        /* 0x1C */
    s8 textureIndex;                 /* 0x1D */
    u8 reserved_1E[2];               /* 0x1E */
    PTR32_FIELD(u8) tasks;                       /* 0x20 */
    PTR32_FIELD(void) fonts;                     /* 0x24 */
    PTR32_FIELD(void) controls;                  /* 0x28 */
};

extern struct MessageSystem* _pMw;

void GSmsgDaemon(void) {
    GStextureUnlockImage(_pMw->textures[_pMw->textureIndex]);
    _pMw->atlasX = 2;
    _pMw->atlasY = 1;
    _pMw->textureIndex ^= 1;
}
