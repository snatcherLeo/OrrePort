/**
 * @file gs_range_8017F3F8_middle.c
 * @brief gs-engine code, 0x8017F3F8 - 0x8017FA5C (7 fns): the REL floor
 *        resource callbacks and the file-resource cache list.
 *
 * Exact island carved out of the 0x8017F2C4 - 0x80180C78 range. The
 * neighbouring LZSS decoder (gs_range_8017F2C4.c) and heap/ARQ code
 * (gs_range_8017FA5C_suffix.c) stay CodeCandidate residuals.
 *
 * The whole range is optimisation-level-0 code (peephole and scheduling
 * still on): leaf functions keep dead induction counters in r30/r31, and
 * single-use parameters and locals get stack homes. configure.py builds
 * this unit with a unit-wide `-opt level=0`; every function here matches
 * with that one setting and no local pragmas.
 *
 * fn_8017F3F8 / fn_8017F484 are the pre/post read callbacks of floor
 * resource type 0x0E in the floor read table (lbl_8036C2A0); the post
 * callback relocates and links the loaded REL (common_rel) and runs its
 * prolog. fn_8017F6B4 is the release callback that runs the epilog and
 * unlinks it.
 */
#include "dolphin/types.h"
#include "dolphin/os/OSModule.h"

/* The REL header as loaded from the disc: big-endian GameCube layout. On the PC, OSModule.h is Aurora's, whose
 * OSModuleInfo holds native link pointers (every later field 8 bytes further on), so the header is read by its
 * GameCube offsets (docs/port/rel-loading.md; pc_rel.c runs the prolog inside Relocate and clears the prolog / epilog
 * words, so the game's own calls below find 0). GameCube: the field reads themselves. */
#ifdef TARGET_PC
#include "pc/disc_types.h"
#define REL_VERSION(m) DISC_U32((u8*)(m) + 0x1C)
#define REL_BSS_SIZE(m) DISC_U32((u8*)(m) + 0x20)
#define REL_PROLOG(m) DISC_U32((u8*)(m) + 0x34)
#define REL_EPILOG(m) DISC_U32((u8*)(m) + 0x38)
#define REL_FIX_SIZE(m) DISC_U32((u8*)(m) + 0x48)
#else
#define REL_VERSION(m) ((m)->info.version)
#define REL_BSS_SIZE(m) ((m)->bssSize)
#define REL_PROLOG(m) ((m)->prolog)
#define REL_EPILOG(m) ((m)->epilog)
#define REL_FIX_SIZE(m) ((m)->fixSize)
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
extern void DCFlushRange(void* addr, u32 nBytes);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource(u32 group, u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresAllocResourceAlign(u32 size, u32 alignment, u32 group,
                                      u32 handle, void* releaseCallback);
#endif
extern void fn_800F9210(u32 group, u32 handle);
extern u16 fn_800E2B00(u32 size, u32 alignment);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSmemGetHandleByPtr(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif
extern BOOL OSLink(OSModuleInfo* module, void* bss);
extern BOOL fn_8009EFE4(OSModuleInfo* module);
extern void fn_8017FB08(void* allocation);
extern void* fn_8017FDB0(u32 size);

s32 fn_8017F6B4(void* unused, u32 group, u32 handle);

/* Pre-read callback: reserve 32-byte aligned room for the REL image. */
void* fn_8017F3F8(u32 group, u32 handle, u32 size)
{
    u32 alignedSize = (size + 0x1F) & ~0x1F;
    /* Fetched and never used; retail keeps the store (stw r0,0xc(r1)). */
    void* existing = GSresGetResource(group, handle);
    void* buffer = GSresAllocResourceAlign(alignedSize, 0x20, group, handle, NULL);

    if (!buffer) {
        return NULL;
    }
    return buffer;
}

/*
 * Temporary block from the tail of the GS heap (handle alloc + pointer).
 * Recovered helper: fn_8017F484 expands it twice with the same call
 * sequence, each time routing the result through its own return temp
 * (r22 -> r29, r20 -> r29).
 */
static inline void* memAllocTail(u32 size)
{
    u32 alignedSize = (size + 0x1F) & ~0x1F;
    u16 handle = fn_800E2B00(alignedSize, 0x20);

    if (handle) {
        return GSmemLock(handle);
    }
    return NULL;
}

#ifdef TARGET_PC
/* common_rel's table counts (46: lbl_80478F98's word heads the 8-byte flag config {count, head}, which stays
 * big-endian with its DISC_STRUCT views) (the u32 words its prolog publishes through these globals: lbl_80478F90 -> species
 * count, ...) are read natively all over the game, so on the PC they are converted to native order once, right
 * after the prolog (the project notes "Decided while snatcherLeo was away", 2026-10-15 night). Only words inside the module just
 * linked are touched; the tables themselves stay big-endian and are read through DISC_STRUCT views. */
extern u32 lbl_80478E88;
extern u32 lbl_80478EA8;
extern u32 lbl_80478E70;
extern u32 char_name_number;
extern u32 lbl_80478E08;
extern u32 lbl_80478EF0;
extern u32 lbl_80478EE8;
extern u32 lbl_80478FB8;
extern u32 lbl_80478ED0;
extern u32 lbl_80478EE0;
extern u32 lbl_80478ED8;
extern u32 lbl_80478EF8;
extern u32 fight_trainer_kind_data_number;
extern u32 fight_side_data_number;
extern u32 fight_floor_data_number;
extern u32 lbl_80478EC8;
extern u32 fight_kind_data_number;
extern u32 lbl_80478E28;
extern u32 lbl_80478E10;
extern u32 lbl_80478E20;
extern u32 snd_test_se_number;
extern u32 fight_type_data_number;
extern u32 lbl_80478F20;
extern u32 lbl_80478F30;
extern u32 lbl_80478F10;
extern u32 fight_encount_data_number;
extern u32 lbl_80478E30;
extern u32 lbl_80478F98;
extern u32 fight_trainer_ai_value_addsub_data_number;
extern u32 fight_trainer_pokemon_part_data_number;
extern u32 floor_tresure_list_number;
extern u32 waza_data_number;
extern u32 pokemon_seikaku_data_number;
extern u32 pokemon_dp_filter_data_number;
extern u32 lbl_80478F90;
extern u32 pokemon_seikaku_rate_data_number;
extern u32 peopleInfoData_number;
extern u32 snd_data_number;
extern u32 lbl_80478FA0;
extern u32 lbl_80478FB0;
extern u32 lbl_80478F68;
extern u32 lbl_80478F60;
extern u32 lbl_80478F58;
extern u32 lbl_80478EC0;
extern u32 lbl_80478F78;
extern u32 lbl_80478F70;
extern u32 menu_se_number;
/* The mail module (common.fsys member 1, src/rel/mail) publishes two counts the mail code reads natively too
 * (added 2026-10-16: read big-endian, 32 rows became 0x20000000 and the hero's mail check ran past the table). */
extern u32 mailData_number;
extern u32 lbl_80478EA0;
static u32* const pc_common_rel_count_slots[48] = {
    &lbl_80478E88, &lbl_80478EA8, &lbl_80478E70, &char_name_number, &lbl_80478E08, &lbl_80478EF0,
    &lbl_80478EE8, &lbl_80478FB8, &lbl_80478ED0, &lbl_80478EE0, &lbl_80478ED8, &lbl_80478EF8,
    &fight_trainer_kind_data_number, &fight_side_data_number, &fight_floor_data_number, &lbl_80478EC8, &fight_kind_data_number, &lbl_80478E28,
    &lbl_80478E10, &lbl_80478E20, &snd_test_se_number, &fight_type_data_number, &lbl_80478F20, &lbl_80478F30,
    &lbl_80478F10, &fight_encount_data_number, &lbl_80478E30, &fight_trainer_ai_value_addsub_data_number, &fight_trainer_pokemon_part_data_number,
    &floor_tresure_list_number, &waza_data_number, &pokemon_seikaku_data_number, &pokemon_dp_filter_data_number, &lbl_80478F90, &pokemon_seikaku_rate_data_number,
    &peopleInfoData_number, &snd_data_number, &lbl_80478FA0, &lbl_80478FB0, &lbl_80478F68, &lbl_80478F60,
    &lbl_80478F58, &lbl_80478EC0, &lbl_80478F78, &lbl_80478F70, &menu_se_number,
    &mailData_number, &lbl_80478EA0
};

static void pc_common_rel_counts_to_native(void* module, u32 size)
{
    u32 i;
    u8* lo = (u8*)module;
    u8* hi = lo + size;
    for (i = 0; i < sizeof(pc_common_rel_count_slots) / sizeof(pc_common_rel_count_slots[0]); i++) {
        u8* word = (u8*)PC_PTR(*pc_common_rel_count_slots[i]);
        if (word >= lo && word + 4 <= hi) {
            *(u32*)word = __builtin_bswap32(*(u32*)word);
        }
    }
}
#endif

/*
 * Post-read callback: move the REL image into a resource block with room
 * for its bss (or its fixed-link tail), link it and run its prolog.
 */
void* fn_8017F484(u32 group, u32 handle, u32 size)
{
    OSModuleHeader* resource;
    OSModuleHeader* module;
    void* bss;
    void* image;
    u32 alignedSize;
    u32 bssSize;
    /*
     * Retail stores to both of these and never reads them: `unused` is
     * zeroed on entry (stw r0,0xc(r1)) and `copy` receives the temporary
     * image in the bss path (stw r29,0x10(r1)).
     */
    void* copy;
    void* unused = NULL;
    u16 imageHandle;

    bss = NULL;
    image = NULL;
    resource = GSresGetResource(group, handle);
    module = resource;
    if (REL_VERSION(module) >= 3) {
        if (REL_BSS_SIZE(module) != 0) {
            alignedSize = (size + 0x1F) & ~0x1F;
            bssSize = (REL_BSS_SIZE(module) + 0x1F) & ~0x1F;
            image = memAllocTail(alignedSize);
            memcpy(image, resource, size);
            copy = image;
            fn_800F9210(group, handle);
            resource = GSresAllocResourceAlign(alignedSize + bssSize, 0x20, group,
                                               handle, fn_8017F6B4);
            memcpy(resource, image, alignedSize);
            module = resource;
            bss = (u8*)module + alignedSize;
            bss = (void*)PC_PTR(((PC_PTR32(bss) + 0x1F) & ~0x1F));
            OSLink(&module->info, bss);
        } else {
            alignedSize = (size + 0x1F) & ~0x1F;
            image = memAllocTail(alignedSize);
            memcpy(image, resource, size);
            fn_800F9210(group, handle);
            resource = GSresAllocResourceAlign(alignedSize, 0x20, group, handle,
                                               fn_8017F6B4);
            DCFlushRange(image, alignedSize);
            memcpy(resource, image, alignedSize);
            module = resource;
            bss = (u8*)module + REL_FIX_SIZE(module);
            bss = (void*)PC_PTR(((PC_PTR32(bss) + 0x1F) & ~0x1F));
            OSLink(&module->info, bss);
        }
    } else {
        OSLink(&module->info, bss);
    }
    if (REL_PROLOG(module)) {
        ((void (*)(void))PC_PTR(REL_PROLOG(module)))();
    }
#ifdef TARGET_PC
    pc_common_rel_counts_to_native(module, size + 0x20);
#endif
    if (image) {
        imageHandle = GSmemGetHandleByPtr(image);
        if (imageHandle) {
            fn_800E24B0(imageHandle);
            GSmemFree(imageHandle);
        }
    }
    return resource;
}

/* Release callback: run the REL epilog and unlink it. */
s32 fn_8017F6B4(void* unused, u32 group, u32 handle)
{
    OSModuleHeader* resource;
    OSModuleHeader* module;
    /* Zeroed on entry and never read (stw r0,0x10(r1)). */
    void* result = NULL;

    resource = GSresGetResource(group, handle);
    module = resource;
    if (REL_EPILOG(module)) {
        ((void (*)(void))PC_PTR(REL_EPILOG(module)))();
    }
    fn_8009EFE4(&module->info);
    return 1;
}

typedef struct GsRangeCacheNode {
    void* data;
    struct GsRangeCacheNode* prev;
    struct GsRangeCacheNode* next;
    s32 size;
    u32 fileHandle;
    u32 key1;
    u32 key2;
    s32 active;
} GsRangeCacheNode;

typedef struct GsRangeCache {
    GsRangeCacheNode* nodes;
    GsRangeCacheNode* last;
    u32 capacity;
    s32 count;
} GsRangeCache;

extern GsRangeCache lbl_80454038;

s32 fn_8017F728(u32 fileHandle, u32 key1, u32 key2)
{
    GsRangeCacheNode* node;
    s32 i;

    node = lbl_80454038.nodes;
    i = 0;
    while (node) {
        if (node->fileHandle == fileHandle && node->key1 == key1 &&
            node->key2 == key2)
        {
            return node->size;
        }
        node = node->next;
        i++;
    }
    return 0;
}

void* fn_8017F794(u32 fileHandle, u32 key1, u32 key2)
{
    GsRangeCacheNode* node;
    s32 i;

    node = lbl_80454038.nodes;
    i = 0;
    while (node) {
        if (node->fileHandle == fileHandle && node->key1 == key1 &&
            node->key2 == key2)
        {
            return node->data;
        }
        node = node->next;
        i++;
    }
    return NULL;
}

void fn_8017F800(u32 fileHandle)
{
    GsRangeCacheNode* node;
    GsRangeCacheNode* prev;
    GsRangeCacheNode* next;
    s32 i;

    node = lbl_80454038.nodes;
    i = 0;
    while (node) {
        if (node->active != 0 && node->fileHandle == fileHandle) {
            next = node->next;
            prev = node->prev;
            if (next) {
                next->prev = node->prev;
            }
            if (prev) {
                prev->next = node->next;
            }
            lbl_80454038.count--;
            node->size = 0;
            node->fileHandle = 0;
            node->key1 = 0;
            node->key2 = 0;
            if (node->data) {
                fn_8017FB08(node->data);
                node->data = NULL;
            }
            node->active = 0;
            if (lbl_80454038.last == node) {
                node->next = NULL;
                lbl_80454038.last = node->prev;
            }
        }
        node = node->next;
        i++;
    }
}

#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_FsysCacheRoom (fn_8017F928) */
#endif

void* fn_8017F928(s32 size, u32 fileHandle, u32 key1, u32 key2)
{
    GsRangeCacheNode* node;
    u32 i;
    u32 alignedSize;
#ifdef TARGET_PC
    u32 room;
#endif

    node = &lbl_80454038.nodes[1];
    for (i = 1; i < lbl_80454038.capacity; node++, i++) {
        if (node->active == 0) {
            node->next = NULL;
#ifdef TARGET_PC
            /* OrreHook_FsysCacheRoom (orre_hooks.h; 2026-11-19, the Japanese release look's determinism fix): the
             * room the console's game would reserve for this archive piece, which OrrePort's language files can serve
             * at another size (a swapped entry from the Japanese disc). The ARAM cache's allocator then makes the
             * console's choices (which pieces fit, where), so later loads come from ARAM or the disc as they would.
             * node->size stays the real size, which the loader copies back. NULL: the size itself, as on the GameCube */
            room = OrreHook_FsysCacheRoom != NULL ? OrreHook_FsysCacheRoom((u32)size, fileHandle, key1, key2) : (u32)size;
            alignedSize = (room + 0x1F) & ~0x1F;
#else
            alignedSize = (size + 0x1F) & ~0x1F;
#endif
            node->prev = lbl_80454038.last;
            node->data = fn_8017FDB0(alignedSize);
#ifdef TARGET_PC
            /* a piece bigger than its room: the port keeps the bytes past the room aside (pc_arq.c), so they never
             * overwrite the next block of ARAM */
            if (node->data && OrreHook_FsysCacheReserved != NULL && alignedSize < (((u32)size + 0x1F) & ~0x1Fu)) {
                OrreHook_FsysCacheReserved(PC_PTR32(node->data), alignedSize, (u32)size);
            }
#endif
            if (node->data) {
                node->size = size;
                node->fileHandle = fileHandle;
                node->key1 = key1;
                node->key2 = key2;
                if (lbl_80454038.last) {
                    lbl_80454038.last->next = node;
                }
                lbl_80454038.last = node;
                lbl_80454038.count++;
                node->active = 1;
            }
            return node->data;
        }
    }
    return NULL;
}
