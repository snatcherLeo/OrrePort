/**
 * @file battle_range_exact_801ED640.c
 * @brief Exact battle resource state and texture-loading helpers.
 *
 * Address range: 0x801ED640 - 0x801ED780.
 */

#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT: the lens-flare resource is disc data */

typedef struct BattleRangeVec {
    f32 x;
    f32 y;
    f32 z;
} BattleRangeVec;

/* The lens-flare resource is a disc file read in place: big-endian views on the PC (pc/disc_types.h). */
typedef struct DISC_STRUCT BattleRangeTextureEntry {
    u16 id;
    u16 pad02;
    DISC_PTR(void) data; /* offset from the resource, relocated by flareInit */
} BattleRangeTextureEntry;

typedef struct DISC_STRUCT BattleRangeResource {
    u32 magic;
    u16 textureCount;
    u16 textureOffset;
    u8 pad08[0x14];
    u8 data[];
} BattleRangeResource;

extern u8 lbl_8047B5C0;
extern u8 lbl_8047B5C1;
extern BattleRangeResource* lbl_8047B5C4;
extern u8* lbl_8047B5C8;
extern BattleRangeVec lbl_80375230;
extern void* lbl_8046D630[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(BattleRangeVec* dst, BattleRangeVec* src);
#endif
extern void* GStextureLoad(void* data);
extern u32 GSgappCreate(s32 state, u8 priority, u32 param,
                       void (*callback)(void));

void flareDraw(void);

void flareSetEnable(u8 value)
{
    lbl_8047B5C1 = value;
}

void flareSetSunPos(BattleRangeVec* value)
{
    GSvecCopy(&lbl_80375230, value);
}

void flareRelease(void)
{
    lbl_8047B5C0 = 0;
}

void flareInit(BattleRangeResource* resource)
{
    s32 i;
    BattleRangeTextureEntry* entry;
    void* texture;
    u8* data;

    lbl_8047B5C4 = resource;
    if (resource->magic == 0x7B1EE3F0) {
        data = resource->data;
        lbl_8047B5C8 = data;
        entry = (BattleRangeTextureEntry*)(data + resource->textureOffset * 12);

        for (i = 0; i < lbl_8047B5C4->textureCount; i++) {
#ifdef TARGET_PC
            DP_SET(entry->data, (u8*)resource + entry->data.raw); /* .raw reads the slot big-endian */
#else
            entry->data = (void*)PC_PTR((PC_PTR32(entry->data) + PC_PTR32(resource)));
#endif
            texture = GStextureLoad(DP(entry->data));
            if (texture == NULL) {
                break;
            }
            lbl_8046D630[entry->id] = texture;
            entry++;
        }
    }
    lbl_8047B5C0 = 1;
}

void flareCreateTask(void)
{
    lbl_8047B5C0 = 0;
    lbl_8047B5C1 = 0;
    GSgappCreate(1, 0xF0, 0xA, flareDraw);
}
