#ifndef GAME_WORLD_FIELD_PARTICLE_H
#define GAME_WORLD_FIELD_PARTICLE_H

/* Field (overworld) particle system: banks of particle generators attached to models in the room.
 * Moved here from src/game/field_range_80117E58.c (2026-10-15) so the startup path's structs live in a
 * header (Milestone 1, boot structs). Layouts are the GameCube ones (32-bit pointers): the port build
 * must not assume these offsets (docs/port/platform-layer-plan.md, data model).
 *
 * FieldParticleBank (0x108 bytes): +0x00 active, +0x01 texture type, +0x04 particle file,
 *   +0x08 64 node slots.
 * FieldParticleNode (0x74 bytes): +0x00 active, +0x02 selector, +0x04 sub id, +0x08 timestamp
 *   (fn_800D3094), +0x0C owning bank, +0x10 generator, +0x14 / +0x20 / +0x2C local position /
 *   rotation / scale, +0x38 position, +0x44 transform state (attach mode), +0x48 / +0x4C attach words
 *   (GSpart entry), +0x50 / +0x5C / +0x68 transformed position / rotation / scale.
 */

#include "dolphin/types.h"

typedef struct FieldParticleInstance {
    u8 active;
    u8 pad_01[7];
    u32 age;
} FieldParticleInstance;

typedef union FieldParticleInstancePtr {
    u8* raw;
    FieldParticleInstance* instance;
} FieldParticleInstancePtr;

typedef struct FieldParticleInstanceList { /* a FieldParticleBank seen through its slot array */
#ifdef TARGET_PC
    u8 pad_00[4];
    void* file;      /* the PC's pointers are 8 bytes: the slots start at 0x10 */
#else
    u8 pad_00[8];
#endif
    FieldParticleInstancePtr entries[0x40];
} FieldParticleInstanceList;

#ifdef TARGET_PC
/* PC: the game's view of HSD_Generator (sysdolphin/baselib/psstructs.h) with HSD_Generator's member types in its
 * order, so the offsets match it on the PC too (checked in tests/pc_hsd/main_gfx.c, which sees both types; this
 * header does not include psstructs.h, whose matrix prototypes clash with older declarations in the field units);
 * the GameCube view pads with GameCube byte counts. */
#define FIELD_PS_APPSRT_SIZE 136 /* sizeof(HSD_psAppSRT) on the PC (checked in tests/pc_hsd/main_gfx.c); 0x74 on the GameCube */
typedef struct FieldParticleGenerator {
    struct HSD_Generator* next;
    u32 kind;
    f32 random;
    f32 count;
    u16 field_10;     /* genLife */
    u16 type;
    u8 bank_index;    /* bank */
    u8 link_no;       /* linkNo */
    u8 field_16;      /* texGroup */
    u8 enabled;       /* x17 */
    u16 family_id;    /* idnum */
    u16 life;
    u8* cmdList;
    f32 position[3];  /* pos */
    f32 vel[3];
    f32 grav;
    f32 fric;
    f32 size;
    f32 radius;
    f32 angle;
    u32 numChild;
    void* appsrt;     /* HSD_psAppSRT* */
    f32 aux[12];      /* the shape union (largest: rect, 12 floats and a u16 flag) */
    u16 auxFlag;
    u16 auxPad;
    u16 posFlags;
    u16 gfxIdx;
    f32 rotation[3];  /* rot */
    f32 scale[3];
} FieldParticleGenerator;
#else
typedef struct FieldParticleGenerator {
    u8 pad_00[0x10];
    u16 field_10;
    u8 pad_12[2];
    u8 bank_index;
    u8 link_no;
    u8 field_16;
    u8 enabled;
    u16 family_id;
    u8 pad_1A[6];
    f32 position[3];
    u8 pad_2C[0x60];
    f32 rotation[3];
    f32 scale[3];
} FieldParticleGenerator;
#endif

typedef struct FieldParticleBank FieldParticleBank;

typedef struct FieldParticleNode {
    u8 active;
    u8 field_01;
    u16 selector;
    u8 field_04;
    u8 field_05;
    u8 field_06;
    u8 pad_07;
    void* field_08;
    FieldParticleBank* bank;
    FieldParticleGenerator* generator;
    f32 local_position[3];
    f32 local_rotation[3];
    f32 local_scale[3];
    f32 position[3];
    s32 transform_state;
    u32 field_48;
    u32 field_4C;
    f32 transformed_position[3];
    f32 transformed_rotation[3];
    f32 transformed_scale[3];
} FieldParticleNode;

struct FieldParticleBank {
    u8 active;
    u8 texture_type;
    u8 pad_02[2];
    void* file;
    FieldParticleNode* slots[64];
};

typedef union FieldParticleNodeRaw {
    u8 bytes[0x74];
    FieldParticleNode typed;
} FieldParticleNodeRaw;

typedef union FieldParticleNodePtr {
    u8* raw;
    FieldParticleNode* typed;
} FieldParticleNodePtr;

typedef union FieldParticleBankCursor {
    FieldParticleBank* bank;
    FieldParticleNode** slot_window;
} FieldParticleBankCursor;

#endif /* GAME_WORLD_FIELD_PARTICLE_H */
