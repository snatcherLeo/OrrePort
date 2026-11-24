/**
 * @file effect_mesh_views.h
 * @brief The surf and sea (blur) effects' views (effect_visual.c, _surfSetupMaterial / _seaSetupMaterial / _seaResetMaterial): the HSD
 *        display and polygon objects of the effect model, its vertex descriptors, and the effect's own work record.
 *
 * The effect takes a model's first mesh, points its vertex descriptors at the effect's own buffers (16-bit indices)
 * and gives it a display list it builds itself (triangle strips); on the way out it restores the saved values.
 * On the GameCube these are the original padded views. On the PC:
 *   - the HSD objects start with an 8-byte class pointer and an 8-byte next pointer (16 bytes, not 8);
 *   - the vertex descriptors are disc data: big-endian (DISC_STRUCT), the array slot a 32-bit disc pointer;
 *   - the work record keeps its GameCube layout (it is allocated in GameCube bytes): pointers in 32-bit slots;
 *   - the display list the effect builds is GX command bytes: 16-bit values big-endian (EFFECT_DL16).
 */
#ifndef GAME_EFFECT_EFFECT_MESH_VIEWS_H
#define GAME_EFFECT_EFFECT_MESH_VIEWS_H

#include "dolphin/types.h"
#include "pc/disc_types.h"
#include "pc/ptr32.h"

#ifdef TARGET_PC
#define EFFECT_DL16(v) __builtin_bswap16((u16)(v))
#define EFFECT_HSD_HEADER void* class_info; void* next_object;
#else
#define EFFECT_DL16(v) (v)
#define EFFECT_HSD_HEADER u8 pad_00[8];
#endif

typedef struct DISC_STRUCT SurfRenderStage {
    s32 kind;
    u32 field_04;
    s32 field_08;
    s32 field_0C;
    u16 field_10;
    u16 field_12;
    u32 field_14;
} SurfRenderStage;
typedef struct SurfRenderPObj {
    EFFECT_HSD_HEADER
    SurfRenderStage* stages;
    u16 flags;
    u16 display_count;
    u8* display;
} SurfRenderPObj;
typedef struct SurfRenderDObj {
    EFFECT_HSD_HEADER
    void* mobj;
    SurfRenderPObj* pobj;
} SurfRenderDObj;
typedef struct SurfRenderState {
    PTR32_FIELD(void) model;
    PTR32_FIELD(u8) positions;
    PTR32_FIELD(u8) normals;
    PTR32_FIELD(u8) texcoords;
    PTR32_FIELD(u8) colors;
    PTR32_FIELD(u8) display;
    s32 display_size;
    u16 rows;
    u16 cols;
} SurfRenderState;

typedef struct DISC_STRUCT BlurRenderStage {
    s32 kind;
    u32 field_04;
    s32 field_08;
    s32 field_0C;
    u16 field_10;
    u16 field_12;
    DISC_PTR(void) field_14;
} BlurRenderStage;
typedef struct BlurRenderPObj {
    EFFECT_HSD_HEADER
    BlurRenderStage* stages;
    u16 flags;
    u16 display_count;
    u8* display;
} BlurRenderPObj;
typedef struct BlurRenderDObj {
    EFFECT_HSD_HEADER
    void* mobj;
    BlurRenderPObj* pobj;
} BlurRenderDObj;
typedef struct BlurRenderState {
    PTR32_FIELD(void) model;
    PTR32_FIELD(u8) positions;
    PTR32_FIELD(u8) texcoords;
    PTR32_FIELD(u8) colors;
    PTR32_FIELD(u8) display;
    s32 display_size;
    u16 rows;
    u16 cols;
    u32 pad_1C;
    u32 saved_field_04[4];
    u32 saved_field_14[4];
    PTR32_FIELD(u8) old_display;
    u16 old_display_count;
    u8 applied;
} BlurRenderState;

#endif /* GAME_EFFECT_EFFECT_MESH_VIEWS_H */
