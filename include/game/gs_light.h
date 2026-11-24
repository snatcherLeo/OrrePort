/**
 * @file gs_light.h
 * @brief GSlight: the game's light objects around HSD's LObj (one pool of 0x74-byte entries), and the light
 *        descriptions a room archive carries (scene_data +8).
 *
 * One definition for every GSlight unit (gs_light*.c, gs_model_shadow*.c used private copies). The descriptions are
 * disc layouts: GSlight builds its default position / interest / light descriptions at run time and HSD loads them
 * like archive data, so they carry the disc marks (pc/disc_types.h: big-endian with 32-bit pointer slots on the PC,
 * the plain layout on the GameCube).
 */
#ifndef GAME_GS_LIGHT_H
#define GAME_GS_LIGHT_H

#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#include "hsd/hsd_forward.h"

/* Same layout as HSD_WObjDesc. */
typedef struct DISC_STRUCT GSlightWObjDesc {
    /* 0x00 */ DISC_PTR(char) className;
    /* 0x04 */ f32 pos[3];
    /* 0x10 */ DISC_PTR(void) robjDesc;
} GSlightWObjDesc; /* size 0x14 */

/* Same layout as HSD_LightDesc. */
typedef struct DISC_STRUCT GSlightLObjDesc {
    /* 0x00 */ DISC_PTR(char) className;
    /* 0x04 */ DISC_PTR(struct GSlightLObjDesc) next;
    /* 0x08 */ u16 flags;
    /* 0x0A */ u16 attnFlags;
    /* 0x0C */ GXColor color;
    /* 0x10 */ DISC_PTR(GSlightWObjDesc) position;
    /* 0x14 */ DISC_PTR(GSlightWObjDesc) interest;
    /* 0x18 */ DISC_PTR(void) param;
} GSlightLObjDesc; /* size 0x1C */

/* One entry of a room's light list (scene_data +8): the light description and its null-terminated animation list. */
typedef DISC_PTR(HSD_LightAnim) GSlightAnimSlot;
typedef struct DISC_STRUCT GSlightDesc {
    /* 0x00 */ DISC_PTR(HSD_LightDesc) lobjDesc;
    /* 0x04 */ DISC_PTR(GSlightAnimSlot) animations;
} GSlightDesc;

/* One GSlight pool entry. */
typedef struct GSlight {
    /* 0x00 */ u8 allocated;
    /* 0x01 */ u8 active;
    /* 0x02 */ u8 hasAnimation;
    /* 0x03 */ u8 isAnimating;
    /* 0x04 */ s32 type;
    /* 0x08 */ GSlightDesc* desc;
    /* 0x0C */ HSD_LObj* lobj;
    /* 0x10 */ GSlightWObjDesc positionDesc; /* default light descriptions */
    /* 0x24 */ GSlightWObjDesc interestDesc;
    /* 0x38 */ GSlightLObjDesc lobjDesc;
    /* 0x54 */ f32 shininess;
    /* 0x58 */ u32 animCount;
    /* 0x5C */ s32 animMode;
    /* 0x60 */ u32 animIndex;
    /* 0x64 */ f32 animRate;
    /* 0x68 */ f32 animFrame;
    /* 0x6C */ f32 animEndFrame;
    /* 0x70 */ s8 animEnded;
    /* 0x71 */ s8 animDirection; /* -1 / +1 while ping-ponging (written by the animation step) */
    /* 0x72 */ u8 pad_72[2];
} GSlight; /* size 0x74 on the GameCube */

#ifndef TARGET_PC
typedef char GSlightWObjDesc_check_size[sizeof(GSlightWObjDesc) == 0x14 ? 1 : -1];
typedef char GSlightLObjDesc_check_size[sizeof(GSlightLObjDesc) == 0x1C ? 1 : -1];
typedef char GSlight_check_size[sizeof(GSlight) == 0x74 ? 1 : -1];
#endif

#endif /* GAME_GS_LIGHT_H */
