/**
 * @file gs_gfx_backfb.h
 * @brief GSgfx back-framebuffer captures (GSgfxBeginBackFBCapture / GSgfxEndBackFBCapture, GSgfxBackFBDoFrame).
 *
 * Up to four textures can ask for a copy of the rendered frame: each frame-end (GSgfxBackFBDoFrame) converts the
 * copied frame into every active capture's texture and calls its callback with the texture, the number of frames
 * captured so far and the caller's data; a callback returning 0 ends its capture. Users: screen fades and
 * transitions, move effects, the title screen.
 *
 * The table holds pointers, so it is larger on a 64-bit PC: it must be declared with this type everywhere (the
 * GameCube layout is checked below).
 */
#ifndef GS_GFX_BACKFB_H
#define GS_GFX_BACKFB_H

#include "dolphin/types.h"

typedef u8 (*GSbackFBCallback)(void* texture, u32 frame, void* userData);

typedef struct GSbackFBCapture {
    u8 active;                   /* 0x00 */
    void* texture;               /* 0x04 */
    GSbackFBCallback callback;   /* 0x08: returns 0 to end the capture */
    void* userData;              /* 0x0C */
    u32 frame;                   /* 0x10: frames captured so far */
} GSbackFBCapture;               /* 0x14 on the GameCube */

extern GSbackFBCapture lbl_80400EE0[4]; /* the capture slots */
extern u8 lbl_8047AAE0;                 /* any capture active */

#ifndef TARGET_PC
typedef char GSbackFBCapture_check_size[sizeof(GSbackFBCapture) == 0x14 ? 1 : -1];
#endif

#endif /* GS_GFX_BACKFB_H */
