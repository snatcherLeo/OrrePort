/**
 * @file field_range_801140DC.c
 * @brief field code, 0x801140DC - 0x8011432C (8 fns).
 *
 * Range unit assigned from the propagated subsystem map
 * (tools/subsystem_propagation.py, >=80% single-label dominance;
 * campaign 2026-07-01). All functions asm-only until matched; the
 * range name stays honest until internal TU structure is proven.
 *
 * Boundary bug fix: the 8 functions below were physically sitting in
 * game/gs_field_colquery.c -- past that unit's own splits.txt range
 * (0x8010F6A0-0x801140DC) -- while this unit's declared range already
 * covers them exactly (8 fns, matching the count in this file's own
 * header above). Relocated here during the gs_field_colquery.c 6-way
 * split so this unit scores real progress instead of 0%.
 */
#include "dolphin/types.h"

extern u8 _floorChangeInfo[];

/* 0x801140DC | 0x90 */
void std246_saveSetStatus578(u32 material, u32 palette, u8 slot) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 savedataBiosGetNowSavedataPtr(void);
#endif
    extern void* savedataBiosGetGamedataPtr(void* ptr); /* returns ptr: the save object savedataBiosGetNowSavedataPtr returned, in r3 (assembly) */
    extern void gamedatasaveSetStatus(void*, u32, u32);
    void* obj;
    u32 save;

    if ((save = savedataBiosGetNowSavedataPtr()) != 0) {
        obj = savedataBiosGetGamedataPtr((void*)PC_PTR(save));
        if (obj != 0) {
            gamedatasaveSetStatus(obj, 5, material);
            gamedatasaveSetStatus(obj, 7, palette);
            gamedatasaveSetStatus(obj, 8, slot);
        }
    }
}

/* 0x8011416C | 0x20 */
/* Script command 161: set the return point used after a blackout (field-steps.md, 2026-10-16) */
void std161_fieldSetPending(u32 roomId, u32 secondRoomId, u8 entryIndex) {
    u8* state = _floorChangeInfo;

    *(u32*)(state + 0x48) = roomId;
    *(u32*)(state + 0x4C) = secondRoomId;
    *(u8*)(state + 0x50) = entryIndex;
    *(u8*)(state + 0x51) = 1;
}

/* 0x8011418C | 0x4C */
void fn_8011418C(u32* roomId, u32* secondRoomId, u8* entryIndex) { /* reads the return map (likely) */
    if (roomId != NULL) {
        *roomId = *(u32*)(_floorChangeInfo + 0x3C);
    }
    if (secondRoomId != NULL) {
        *secondRoomId = *(u32*)(_floorChangeInfo + 0x40);
    }
    if (entryIndex != NULL) {
        *entryIndex = *(u8*)(_floorChangeInfo + 0x44);
    }
}

/* 0x801141D8 | 0x20 */
void std128_mapSetReturnMap(u32 roomId, u32 secondRoomId, u8 entryIndex) {
    u8* state = _floorChangeInfo;

    *(u32*)(state + 0x3C) = roomId;
    *(u32*)(state + 0x40) = secondRoomId;
    *(u8*)(state + 0x44) = entryIndex;
    *(u8*)(state + 0x51) = 0;
}

/* 0x801141F8 | 0x5C */
void EvlogSet__FScUl(s8 event, u32 value) {
    typedef struct EvlogEntry {
        s8 event;
        u8 padding[3];
        u32 value;
    } EvlogEntry;
    extern EvlogEntry lbl_8035B818[];
    extern s8 lbl_8047AD60;
    s8 index = lbl_8047AD60;

    lbl_8035B818[index].event = event;
    lbl_8035B818[index].value = value;
    lbl_8047AD60++;
    lbl_8047AD60 %= 10;
}

/* 0x80114254 | 0x60 */
s32 floorReadRelModulePostFunc(s32 param1, s32 param2, s32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GSresGetResource(s32, s32);
#endif
    extern void fn_8017F484(s32, s32, s32);
    s32 result = PC_PTR32_IMPLICIT(GSresGetResource(param1, param2)); /* r4 = param2 at the call (assembly) */

    fn_8017F484(param1, param2, param3);
    return result;
}

/* 0x801142B4 | 0x44 */
void* floorReadRelModulePreFunc(void* group, void* model, u32 size) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresAllocResourceAlign(u32 size, u32 alignment, void* group, void* model, u32 flags);
#endif
    void* result;

    result = GSresAllocResourceAlign((size + 0x1F) & ~0x1F, 0x20, group, model, 0);
    if (result == NULL) {
        result = NULL;
    }
    return result;
}

/* 0x801142F8 | 0x34 */
void* floorReadGFLPostFunc(u32 group, u32 modelId) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32, u32);
#endif
    extern void flareInit(void*);
    void* model;

    model = GSresGetResource(group, modelId);
    flareInit(model);
    return model;
}
