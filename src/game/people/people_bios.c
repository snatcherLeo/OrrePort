/**
 * @file people_bios.c
 * @brief The people core unit (.text 0x8018F470-0x8018FE30): the people
 *        array, entry accessors, info-bios getters and the floor
 *        save/restore hooks.
 *
 * A separate unit from people.c: it owns .sdata2 0x8047D8A8-0x8047D8B8
 * (0.0f, the degree factor and 1.0f, all of which people.c's pool also
 * holds), .sbss 0x8047B1F8-0x8047B208 and fn_8018F4C8's switch table at
 * .data 0x8036C540, and people.c calls even its eight-byte getters.
 *
 * Most of its functions are linked from their own carved units; this file
 * holds the save/restore hooks the candidate unit people_candidate_8018F730
 * scores (research source carried over from the old combined people.c).
 */
#include "dolphin/types.h"
#include "game/people/people.h"
#ifdef TARGET_PC
#include "game/gs_model_struct.h" /* GSmodel's attachment vectors by name in peopleBiosPushData */
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif
extern void* GSmodelGetPositionPtr(void*);
extern void* GSmodelGetRotationPtr(void*);
extern void GSmodelPushState(void* model, void* state);
extern void GSmodelPopState(void* model, void* state);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetVisibility(void*, u8);
#endif
extern void GScolsys2HumanEnable(s32, u8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetPosition(void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetRotation(void*, void*);
#endif
extern int peopleOpenSub(void* entry, u32 groupId, u32 indexId, s32 objectId);
extern void peopleSetMotionSlot(void* entry, u32 motionIndex);
extern void fn_801848D0(void* model, s32 group, s32 id, s32 part);
extern void std067_characterStopLookAt(u32 groupId, u32 index);
extern void std181_characterTurnHeadTowardsCharacter(u32 groupId, u32 index, u32 targetGroupId, u32 targetIndex);
extern void std066_characterLookAtPosition(u32 groupId, u32 index, u32 x, u32 y, u32 z);
extern void fn_801812C4(void* entry);

extern s32 lbl_8047B1F8;          /* maximum people count */
extern PeopleEntry* lbl_8047B200; /* people array (heap-allocated) */
extern const f32 lbl_8047D8B0;    /* default moveSpeed (1.0f) */

#define gPeopleMaxCount lbl_8047B1F8
#define gPeopleArray    lbl_8047B200

/* A PeopleEntry field read at its GameCube offset: the PC entry has native pointers, so the PC names the field
 * (the save / restore around a battle read the thread handle as the group id: Willie's battle end, 2026-10-17). */
#ifdef TARGET_PC
/* hero_move.c's HeroChatTarget reads a talk target's group / index at these PC offsets */
_Static_assert(offsetof(PeopleEntry, groupId) == 0x40 && offsetof(PeopleEntry, index) == 0x44,
               "PeopleEntry moved: update HeroChatTarget in hero_move.c");
#define PEOPLE_AT(e, T, gcOffset, field) (((PeopleEntry*)(e))->field)
#else
#define PEOPLE_AT(e, T, gcOffset, field) (*(T*)((u8*)(e) + (gcOffset)))
#endif

u32 peopleBiosGetPushDataSize(void) {
    PeopleEntry* current;
    PeopleEntry* entry = lbl_8047B200;
    s32 count = lbl_8047B1F8;
    u32 total;
    s32 i;

    total = 0;
    for (i = 0; i < count; i++) {
        if (i < 0 || count <= i) {
            current = NULL;
        } else {
            current = entry;
        }
        if (current->active != 0) {
            total += PEOPLE_SPAWN_DATA_SIZE;
        }
        entry = (PeopleEntry*)((u8*)entry + PEOPLE_ENTRY_SIZE);
    }

    return total;
}
void peopleBiosPushData(u8* dst, u32 size) {
    u32 offset;
    u8* end;
    u8* current;
    void* model;
    s32 i;

    i = 0;
    current = dst;
    offset = 0;
    end = dst + size;
    while (i < gPeopleMaxCount) {
        PeopleEntry* entry;

        if (i < 0 || gPeopleMaxCount <= i) {
            entry = NULL;
        } else {
            entry = (PeopleEntry*)((u8*)gPeopleArray + offset);
        }
        if (entry->active) {
            model = entry->modelHandle;
            memcpy(current, (u8*)entry + PEOPLE_SAVE_TAIL_OFFSET, PEOPLE_SAVE_TAIL_SIZE);
            memcpy(current + PEOPLE_SAVE_TAIL_SIZE, GSmodelGetPositionPtr(model), 0xC);
            memcpy(current + PEOPLE_SAVE_TAIL_SIZE + 0x0C, GSmodelGetRotationPtr(model), 0xC);
#ifdef TARGET_PC /* the GSmodel attachment vectors by name (GameCube +0x120 / +0x12C) */
            memcpy(current + PEOPLE_SAVE_TAIL_SIZE + 0x18, &((GSmodel*)model)->overridePosition, 0xC);
            memcpy(current + PEOPLE_SAVE_TAIL_SIZE + 0x24, &((GSmodel*)model)->overrideRotation, 0xC);
#else
            memcpy(current + PEOPLE_SAVE_TAIL_SIZE + 0x18, (u8*)model + 0x120, 0xC);
            memcpy(current + PEOPLE_SAVE_TAIL_SIZE + 0x24, (u8*)model + 0x12C, 0xC);
#endif
            GSmodelPushState(model, current + PEOPLE_SAVE_TAIL_SIZE + 0x30);
            current += PEOPLE_SPAWN_DATA_SIZE;
            if (current > end) {
                break;
            }
        }
        offset += PEOPLE_ENTRY_SIZE;
        i++;
    }
}
/* Take the first free slot of the people array and reset it. */
/* RULE-EXCEPTION(user-approved): single-use inline helper — see docs/RULE_EXCEPTIONS.md */
static inline PeopleEntry* peopleBiosAllocEntry(void) {
    s32 count = gPeopleMaxCount;
    PeopleEntry* current = gPeopleArray;
    PeopleEntry* entry;
    s32 i;

    for (i = 0; i < count; i++) {
        if (i < 0 || count <= i) {
            entry = NULL;
        } else {
            entry = current;
        }
        if (entry->active == 0) {
            memset(entry, 0, PEOPLE_ENTRY_SIZE);
            entry->active = 1;
            entry->selfPtr = entry;
            entry->shadowId = -1;
            entry->moveSpeed = lbl_8047D8B0;
            return entry;
        }
        current = (PeopleEntry*)((u8*)current + PEOPLE_ENTRY_SIZE);
    }
    return NULL;
}

/* fn_8018FB60's body: show the model with the given animation. */
/* RULE-EXCEPTION(user-approved): inline copy of the real fn_8018FB60 — see docs/RULE_EXCEPTIONS.md */
static inline void peopleBiosSetAnim(PeopleEntry* entry, u8 animationId) {
    void* model = entry->modelHandle;

    if (model != NULL) {
        entry->animId = animationId;
        GSmodelSetVisibility(model, animationId);
    }
}

void peopleBiosPopData(u8* src, u32 size) {
    PeopleEntry* entry;
    u8* saved;
    u32 n;
    void* model;
    u32 count;
    PeopleEntry* restored[64];
    u32 k;
    u32 j;

    memset(gPeopleArray, 0, gPeopleMaxCount * PEOPLE_ENTRY_SIZE);
    count = size / PEOPLE_SPAWN_DATA_SIZE;
    if (size % PEOPLE_SPAWN_DATA_SIZE != 0) {
        return;
    }
    if (count > 64) {
        return;
    }

    saved = src;
    n = count;
    k = 0;
    while (n--) {
        entry = peopleBiosAllocEntry();
        /* +8 / +0x0C / +0x10 in the saved tail: groupId, index, scriptRef (the same tail offsets on the PC; the
         * 8-byte scriptRef's low half is the object id) */
        peopleOpenSub(entry, *(u32*)(saved + 8), *(u32*)(saved + 0x0C),
                      *(u32*)(saved + 0x10));
        memcpy((u8*)entry + PEOPLE_SAVE_TAIL_OFFSET, saved, PEOPLE_SAVE_TAIL_SIZE);
#ifdef TARGET_PC
        {
            /* PC (observe only, 2026-11-03, port-gaps 55): with ORRE_PAUSE_LOG set, each person as restored
             * after a menu room pops (sub-state and the wander pause, +0x84) */
            extern char* getenv(const char*);
            extern void OSReport(const char*, ...);
            extern volatile u32 pc_vi_heartbeat __attribute__((weak));
            if (getenv("ORRE_PAUSE_LOG") != NULL) {
                OSReport("[pause] restored frame %u group %d index %d moveType %u subState %u left %.4f\n",
                         &pc_vi_heartbeat != NULL ? (unsigned)pc_vi_heartbeat : 0u, (int)entry->groupId,
                         (int)entry->index, (unsigned)entry->moveType, (unsigned)entry->subState,
                         (double)entry->animBlendFactor);
            }
        }
#endif
        restored[k++] = entry;
        saved += PEOPLE_SPAWN_DATA_SIZE;
    }

    saved = src;
    k = 0;
    while (count--) {
        entry = restored[k++];
        model = entry->modelHandle;
        peopleBiosSetAnim(entry, entry->animId);
        {
            u8 shadowAnimId = entry->shadowAnimId;
            s32 shadowId = entry->shadowId;

            if (shadowId >= 0) {
                GScolsys2HumanEnable(shadowId, shadowAnimId);
            }
        }
        peopleSetMotionSlot(entry, PEOPLE_AT(entry, u32, 0x90, motionIndex));
        GSmodelPopState(model, saved + PEOPLE_SAVE_TAIL_SIZE + 0x30);

        if (PEOPLE_AT(entry, s32, 0xC8, walkNodeA) != -1 && PEOPLE_AT(entry, s32, 0xCC, walkNodeB) != -1) {
            fn_801848D0(model, PEOPLE_AT(entry, s32, 0xC8, walkNodeA), PEOPLE_AT(entry, s32, 0xCC, walkNodeB),
                        PEOPLE_AT(entry, s32, 0xD0, walkNodeC));
            GSvecCopy(GSmodelGetPositionPtr(model), saved + PEOPLE_SAVE_TAIL_SIZE);
            GSvecCopy(GSmodelGetRotationPtr(model), saved + PEOPLE_SAVE_TAIL_SIZE + 0x0C);
            GSmodelSetPosition(model, saved + PEOPLE_SAVE_TAIL_SIZE + 0x18);
            GSmodelSetRotation(model, saved + PEOPLE_SAVE_TAIL_SIZE + 0x24);
        }

        switch (PEOPLE_AT(entry, u8, 0x96, moveType)) {
        case 0:
            std067_characterStopLookAt(entry->groupId, entry->index);
            break;
        case 1:
            std181_characterTurnHeadTowardsCharacter(entry->groupId, entry->index, PEOPLE_AT(entry, u32, 0xC0, walkPathId),
                        PEOPLE_AT(entry, u32, 0xC4, walkPathParam));
            break;
        case 2:
            std066_characterLookAtPosition(entry->groupId, entry->index, (s32)PEOPLE_AT(entry, f32, 0xA8, targetX),
                        (s32)PEOPLE_AT(entry, f32, 0xAC, targetY), (s32)PEOPLE_AT(entry, f32, 0xB0, targetZ));
            break;
        }
        if (PEOPLE_AT(entry, u8, 0x96, moveType) != 0) {
            for (j = 0; j < 60; j++) {
                fn_801812C4(entry);
            }
        }
        saved += PEOPLE_SPAWN_DATA_SIZE;
    }
}
