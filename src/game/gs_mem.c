/**
 * @file gs_mem.c
 * @brief GSmem -- Genius Sonority's handle-based heap allocator.
 *
 * Address range: 0x800E202C - 0x800E3604.  The allocator's state below is
 * shared with the neighbouring GSmem units (0x800E0DDC - 0x800E202C), so it
 * stays extern here.
 *
 * Guard mode (lbl_8047AB28 != 0; retail main() passes 0): every allocation
 * is 8 bytes larger, the caller's pointer is data + 4, the first and last
 * four bytes must stay zero, and an unlocked block carries a checksum that
 * the next lock or free verifies.
 */
#include "dolphin/types.h"
#include "game/gs_mem.h"

extern u8 lbl_8047AB28;          /* guard mode                             */
extern s32 lbl_8047AB2C;         /* fit strategy (GSMEM_FIT_*)             */
extern GSmemBlock* lbl_8047AB30; /* free list head (lowest address)        */
extern GSmemEntry* lbl_8047AB34; /* handle table top: entry of handle 1    */
extern GSmemEntry* lbl_8047AB38; /* handle table bottom (lowest entry)     */
extern u32 lbl_8047AB3C;         /* last error code, 0 on success          */
extern u32 lbl_8047AB48;         /* outstanding locks                      */
extern u32 lbl_8047AB4C;         /* live allocations                       */
extern u32 lbl_8047AB50;         /* handle table growths                   */
extern u32 lbl_8047AB54;         /* frees                                  */
extern u32 lbl_8047AB58;         /* unlocks                                */
extern u32 lbl_8047AB5C;         /* locks                                  */
extern u32 lbl_8047AB60;         /* allocations                            */
extern void* lbl_8047AB64;       /* heap end (32-byte aligned)             */
extern void* lbl_8047AB68;       /* heap start (32-byte aligned)           */
extern char lbl_80270658[];
extern char lbl_80270D78[];
extern char lbl_80270DD0[];
extern char lbl_80270DFC[];

/* Nonzero: new allocations are zeroed and flushed from the data cache. */
u8 lbl_80478AF0 = 1;

extern void GSlogWrite(const char* format, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dest, int value, u32 length);
#endif
extern void DCFlushRange(void* address, u32 length);

u16 GSmemShrink(u8* allocation, u32 size);

#ifdef TARGET_PC
/* the project's build helper, 2026-11-18 (port-gaps 83), observe-only: with ORRE_HEAP_GS_CHECK=1, every free, shrink and
 * allocation search checks the free list (each block inside the heap, its end inside it, the links in address order)
 * and reports the first bad block once, with the operation and the host stack that found it. The move sweep's
 * free-list head turned up holding game data; this names the first operation that sees it. All platforms (the stack
 * is Windows-only in pc_heap.c). */
static void GSmemPcCheck(const char* where)
{
    extern int pc_heap_gsmem_check_on(const char* where);
    extern void pc_heap_gsmem_check_fail(const char* where, const void* block, unsigned size, const void* next,
                                         int index);
    GSmemBlock* block;
    int index = 0;
    if (!pc_heap_gsmem_check_on(where)) {
        return;
    }
    for (block = lbl_8047AB30; block != NULL && index < 100000; block = block->next, index++) {
        const u8* start = (const u8*)lbl_8047AB68;
        const u8* end = (const u8*)lbl_8047AB64;
        if ((const u8*)block < start || (const u8*)block >= end) { /* outside: its fields can't be read safely */
            pc_heap_gsmem_check_fail(where, block, 0, NULL, index);
            return;
        }
        if ((const u8*)block + block->size > end || (block->next != NULL && block->next <= block)) {
            pc_heap_gsmem_check_fail(where, block, block->size, block->next, index);
            return;
        }
    }
}
#define GS_PC_CHECK(where) GSmemPcCheck(where)
#else
#define GS_PC_CHECK(where)
#endif

/*
 * Inline helpers.  Each is recovered from the target, not invented:
 * - GSmemEntryFromHandle: expanded in GSmemFree, fn_800E24B0,
 *   GSmemLock, fn_800E2B00 and GSmemAlloc; the free/lock/unlock copies
 *   re-test the handle on a stale condition register ("bne; bne").
 * - GSmemEntryHandle: GSmemShrink re-tests the entry for NULL after it has
 *   already returned on NULL (the helper's own guard).
 * - GSmemGuardsAreValid (3 sites), GSmemClearGuards (4 sites) and
 *   GSmemChecksum (5 sites) are repeated expansions; the guard test returns
 *   a byte (clrlwi. on the result).
 * - GSmemFindEntryByData: GSmemGetHandleByPtr and GSmemShrink, both with the
 *   return-in-loop shape ("bne next; b found") and a NULL result that is
 *   tested again by the caller.
 * - GSmemGetFreeEntry and GSmemFindPreviousData (GSmemShrink only): the
 *   same return-in-loop shape, a dead branch after the returned NULL, and
 *   the found->data result routed through a register and re-tested.
 */
static inline GSmemEntry* GSmemEntryFromHandle(u16 handle)
{
    if (handle == 0) {
        return NULL;
    }
    return lbl_8047AB34 - (handle - 1);
}

static inline u16 GSmemEntryHandle(GSmemEntry* entry)
{
    if (entry == NULL) {
        return 0;
    }
    return (PC_PTR32(lbl_8047AB34) - PC_PTR32(entry)) / sizeof(GSmemEntry) + 1;
}

static inline u8 GSmemGuardsAreValid(GSmemEntry* entry)
{
    u8* guard = entry->data;

    if (guard[0] != 0) {
        return FALSE;
    }
    if (guard[1] != 0) {
        return FALSE;
    }
    if (guard[2] != 0) {
        return FALSE;
    }
    if (guard[3] != 0) {
        return FALSE;
    }
    guard = guard + entry->size - 4;
    if (guard[0] != 0) {
        return FALSE;
    }
    if (guard[1] != 0) {
        return FALSE;
    }
    if (guard[2] != 0) {
        return FALSE;
    }
    if (guard[3] != 0) {
        return FALSE;
    }
    return TRUE;
}

static inline void GSmemClearGuards(GSmemEntry* entry)
{
    u8* guard = entry->data;

    guard[0] = 0;
    guard[1] = 0;
    guard[2] = 0;
    guard[3] = 0;
    guard = (u8*)entry->data + entry->size - 4;
    guard[0] = 0;
    guard[1] = 0;
    guard[2] = 0;
    guard[3] = 0;
}

static inline u16 GSmemChecksum(GSmemEntry* entry)
{
    u32 size = entry->size;
    u32 checksum = 0x3D94;
    u16* halves = entry->data;
    u32 halfCount = size >> 1;
    u32 byteCount = size & 1;
    u8* bytes;

    for (; halfCount != 0; halfCount--) {
        checksum += *halves++;
    }
    bytes = (u8*)halves;
    for (; byteCount != 0; byteCount--) {
        checksum += *bytes++;
    }
    return checksum;
}

static inline GSmemEntry* GSmemGetFreeEntry(void)
{
    GSmemBlock* block;
    GSmemEntry* entry;
    GSmemEntry* clear;
    u32 bytes;

    for (entry = lbl_8047AB34; entry >= lbl_8047AB38; entry--) {
        if (entry->handle == 0) {
            return entry;
        }
    }

    block = lbl_8047AB30;
    if (block == NULL) {
        return NULL;
    }
    while (block->next != NULL) {
        block = block->next;
    }
    if ((u8*)block + block->size != (u8*)lbl_8047AB38) {
        return NULL;
    }

    bytes = 0x4000;
    if (block->size <= 0x4000) {
        bytes = block->size & ~0xF;
        if (bytes == 0) {
            return NULL;
        }
        if (block->prev != NULL) {
            block->prev->next = NULL;
        } else {
            lbl_8047AB30 = NULL;
        }
    } else {
        block->size -= 0x4000;
    }

    entry = lbl_8047AB38 - 1;
    lbl_8047AB38 -= bytes / sizeof(GSmemEntry);
    for (clear = entry; clear >= lbl_8047AB38; clear--) {
        clear->handle = 0;
    }
    lbl_8047AB50++;
    return entry;
}

static inline void* GSmemFindPreviousData(void* ptr)
{
    GSmemEntry* entry;
    GSmemEntry* found = NULL;

    for (entry = lbl_8047AB34; entry >= lbl_8047AB38; entry--) {
        if (entry->handle != 0 && entry->data < ptr) {
            if (found == NULL) {
                found = entry;
            } else if (PC_PTR32(ptr) - PC_PTR32(entry->data) < PC_PTR32(ptr) - PC_PTR32(found->data)) {
                found = entry;
            }
        }
    }
    if (found == NULL) {
        return NULL;
    }
    return found->data;
}

static inline GSmemEntry* GSmemFindEntryByData(void* ptr)
{
    GSmemEntry* entry;

    for (entry = lbl_8047AB34; entry >= lbl_8047AB38; entry--) {
        if (entry->handle != 0 && entry->data == ptr) {
            return entry;
        }
    }
    return NULL;
}

u16 GSmemGetHandleByPtr(void* ptr)
{
    GSmemEntry* entry;

    if (lbl_8047AB28 == 1) {
        ptr = (u8*)ptr - 4;
    }
    entry = GSmemFindEntryByData(ptr);
    if (entry == NULL) {
        return 0;
    }
    return entry->handle;
}

s32 GSmemFree(u16 handle)
{
    s32 result = 0;
    GSmemBlock* after;
    const char* messages = lbl_80270658;
    GSmemEntry* entry;
    GSmemBlock* last;
    GSmemBlock* before;
    GSmemBlock* released;
    GSmemBlock* next;
    u32 size;

    if (handle == 0) {
        GSlogWrite(messages + 0x5A4, handle);
        lbl_8047AB3C = 1;
        return 1;
    }

    entry = GSmemEntryFromHandle(handle);
    if (entry->handle != handle) {
        GSlogWrite(messages + 0x5A4, handle);
        lbl_8047AB3C = 1;
        return 1;
    }
    if (entry->lockCount != 0) {
        result = 8;
        GSlogWrite(messages + 0x5C8, handle);
    }

    if (lbl_8047AB28 == 1) {
        if (!GSmemGuardsAreValid(entry)) {
            GSlogWrite(messages + 0x5FC, handle);
            lbl_8047AB3C = 7;
            result = 7;
        }
        if (entry->checksum != GSmemChecksum(entry)) {
            GSlogWrite(messages + 0x62C, handle);
            lbl_8047AB3C = 6;
        }
    }

    entry->handle = 0;
    size = entry->size;
#ifdef TARGET_PC
    { /* port-gaps 83, observe-only: ORRE_HEAP_GS_WATCH names who frees the watched address */
        extern void pc_heap_gsmem_watch(const void* block, unsigned size, int kind);
        pc_heap_gsmem_watch(entry->data, size, 'F');
    }
#endif
    /* Insert the block into the address-ordered free list.  When the list
     * is empty, retail stores `after` without ever setting it (r28 is
     * stored at 0x800E23E8 on a path that never writes it); the source
     * keeps that read. */
    if (lbl_8047AB30 == NULL) {
        lbl_8047AB30 = entry->data;
        released = lbl_8047AB30;
        before = NULL;
    } else {
        for (after = lbl_8047AB30; (GSmemBlock*)entry->data > after;) {
            last = after;
            after = after->next;
            if (after == NULL) {
                break;
            }
        }
        if (after != NULL) {
            before = after->prev;
            next = after;
        } else {
            before = last;
            next = NULL;
        }
        if (before != NULL) {
            before->next = entry->data;
        } else {
            lbl_8047AB30 = entry->data;
        }
        if (next != NULL) {
            next->prev = entry->data;
        }
        released = entry->data;
    }
    released->prev = before;
    released->next = after;
    released->size = size;

    if (released->next != NULL &&
        released->next == (GSmemBlock*)((u8*)released + released->size)) {
        released->size += released->next->size;
        if (released->next->next != NULL) {
            released->next->next->prev = released;
        }
        released->next = released->next->next;
    }
    if (released->prev != NULL &&
        released == (GSmemBlock*)((u8*)released->prev + released->prev->size)) {
        released->prev->size += released->size;
        if (released->next != NULL) {
            released->next->prev = released->prev;
        }
        released->prev->next = released->next;
    }

    lbl_8047AB54++;
    lbl_8047AB4C--;
    GS_PC_CHECK("after GSmemFree");
    return result;
}

s32 fn_800E24B0(u16 handle)
{
    const char* messages = lbl_80270658;
    GSmemEntry* entry;
    s32 result;

    if (handle == 0) {
        GSlogWrite(messages + 0x660, handle);
        lbl_8047AB3C = 1;
        return 1;
    }
    entry = GSmemEntryFromHandle(handle);
    if (entry->handle != handle) {
        GSlogWrite(messages + 0x660, handle);
        lbl_8047AB3C = 1;
        return 1;
    }
    if (entry->lockCount == 0) {
        GSlogWrite(messages + 0x684, handle);
        lbl_8047AB3C = 5;
        return 5;
    }

    lbl_8047AB3C = 0;
    result = 0;
    if (lbl_8047AB28 == 1) {
        if (!GSmemGuardsAreValid(entry)) {
            GSlogWrite(messages + 0x5FC, handle);
            lbl_8047AB3C = 7;
            result = 7;
            GSmemClearGuards(entry);
        }
        if (entry->lockCount == 1) {
            entry->checksum = GSmemChecksum(entry);
        }
    }

    entry->lockCount--;
    lbl_8047AB58++;
    lbl_8047AB48--;
    return result;
}

void* GSmemLock(u16 handle)
{
    const char* messages = lbl_80270658;
    GSmemEntry* entry;

    if (handle == 0) {
        GSlogWrite(messages + 0x6D0, handle);
        lbl_8047AB3C = 1;
        return NULL;
    }
    entry = GSmemEntryFromHandle(handle);
    if (entry->handle != handle) {
        GSlogWrite(messages + 0x6D0, handle);
        lbl_8047AB3C = 1;
        return NULL;
    }
    if (entry->lockCount == 0xFFFF) {
        GSlogWrite(messages + 0x6F4, handle);
        lbl_8047AB3C = 4;
        return NULL;
    }

    lbl_8047AB3C = 0;
    if (lbl_8047AB28 == 1) {
        if (!GSmemGuardsAreValid(entry)) {
            GSlogWrite(messages + 0x5FC, handle);
            lbl_8047AB3C = 7;
            GSmemClearGuards(entry);
        }
        if (entry->lockCount == 0 && entry->checksum != GSmemChecksum(entry)) {
            GSlogWrite(messages + 0x62C, handle);
            lbl_8047AB3C = 6;
        }
    }

    entry->lockCount++;
    lbl_8047AB5C++;
    lbl_8047AB48++;
    if (lbl_8047AB28 == 1) {
        return (u8*)entry->data + 4;
    }
    return entry->data;
}

s32 fn_800E2AF8(u16 handle)
{
    return 1;
}

u16 fn_800E2B00(u32 size, u16 alignment)
{
    GSmemBlock* block;
    void* address;
    u32 blockSize;
    u32 allocationSize;
    u16 handle;

    if (size == 0) {
        return 0;
    }
    GS_PC_CHECK("before fn_800E2B00's search");
    alignment = (alignment + 3) & ~3;
    if (alignment == 0 || (alignment & 0x1F) != 0) {
        return 0;
    }

    size = (size + 0x1F) & ~0x1F;
    blockSize = (size + 3) & ~3;
    if (lbl_8047AB28 != 0) {
        blockSize += 8;
    }
    allocationSize = blockSize;
    if (allocationSize < sizeof(GSmemBlock)) {
        allocationSize = sizeof(GSmemBlock);
    }
    address = NULL;

    for (block = lbl_8047AB30; block != NULL; block = block->next) {
        if (block->size >= allocationSize) {
            u8* candidate = (u8*)block + block->size - allocationSize;

#ifdef TARGET_PC
            /* the project's build helper, 2026-11-18 (port-gaps 83), observe-only: a free block that reaches past 4 GB can
             * only have an overwritten size; print it and the bytes around its header before the pointer check
             * stops the game, so the overwrite can be traced. */
            if ((uintptr_t)candidate > 0xFFFFFFFFu) {
                extern void pc_heap_gsmem_bad(const void* block, unsigned size, unsigned allocationSize);
                extern void pc_heap_gsmem_chain(int index, const void* block, unsigned size, const void* next);
                GSmemBlock* walk = lbl_8047AB30;
                int index = 0;
                pc_heap_gsmem_bad(block, block->size, allocationSize);
                for (; walk != NULL && index < 8; walk = walk->next, index++) { /* the list from its head */
                    pc_heap_gsmem_chain(index, walk, walk->size, walk->next);
                    if (walk == block) {
                        break;
                    }
                }
            }
#endif
            candidate = (u8*)PC_PTR((PC_PTR32(candidate) & ~(alignment - 1)));
            if (candidate >= (u8*)block) {
                address = candidate;
            }
        }
    }
    if (address == NULL) {
        lbl_8047AB3C = 2;
        return 0;
    }

    handle = GSmemShrink(address, size);
    GSmemEntryFromHandle(handle)->align = alignment;
    return handle;
}

u16 GSmemAlloc(u32 size, u16 alignment)
{
    u32 blockSize;
    u8* address;
    GSmemBlock* selected;
    u8* candidate;
    u16 handle;
    GSmemBlock* block = lbl_8047AB30;
    u32 allocationSize;
    u32 mask;

    if (size == 0) {
        return 0;
    }
    GS_PC_CHECK("before GSmemAlloc's search");
#ifdef TARGET_PC
    { /* 2026-11-07 (port-gaps 74): with ORRE_HEAP_LOG, the GS heap's free space every 18,000 frames, observe-only,
       * so a long replay shows whether this heap stays flat like the SDK heap */
        extern int pc_heap_gsmem_due(void);
        extern void pc_heap_gsmem_report(long freeTotal, long largest, unsigned blocks, u32 failedSize,
                                         unsigned long long lowest, unsigned long long reachEnd);
        if (pc_heap_gsmem_due()) {
            long freeTotal = 0, largest = 0;
            unsigned blocks = 0;
            /* the project's build helper, 2026-11-18 (port-gaps 83): the free list's lowest block and furthest end, so a
             * block past the arena (an overwritten size or link) shows in the log; observe-only. */
            unsigned long long lowest = ~0ull, reachEnd = 0;
            for (block = lbl_8047AB30; block != NULL; block = block->next) {
                freeTotal += (long)block->size;
                if ((long)block->size > largest) {
                    largest = (long)block->size;
                }
                if ((unsigned long long)(uintptr_t)block < lowest) {
                    lowest = (unsigned long long)(uintptr_t)block;
                }
                if ((unsigned long long)(uintptr_t)block + block->size > reachEnd) {
                    reachEnd = (unsigned long long)(uintptr_t)block + block->size;
                }
                blocks++;
                if (blocks > 100000u) { /* a list that loops (an overwritten link) would never end the walk */
                    break;
                }
            }
            pc_heap_gsmem_report(freeTotal, largest, blocks, 0, blocks != 0 ? lowest : 0, reachEnd);
            block = lbl_8047AB30;
        }
    }
#endif
    alignment = (alignment + 3) & ~3;
    if (alignment == 0 || (alignment & 0x1F) != 0) {
        return 0;
    }

    size = (size + 0x1F) & ~0x1F;
    blockSize = (size + 3) & ~3;
    if (lbl_8047AB28 != 0) {
        blockSize += 8;
    }
    mask = alignment - 1;
    allocationSize = blockSize + mask;
    if (allocationSize < sizeof(GSmemBlock)) {
        allocationSize = sizeof(GSmemBlock);
    }

    for (; block != NULL; block = block->next) {
        candidate = (u8*)PC_PTR(((PC_PTR32(block) + alignment - 1) & ~mask));
        if (candidate + allocationSize < (u8*)block + block->size) {
            break;
        }
    }
    /* With an empty free list `candidate` is copied unset (retail moves
     * r10 unconditionally at 0x800E2CB4); `selected` is NULL then and the
     * copy is never used. */
    selected = block;
    address = candidate;

    switch (lbl_8047AB2C) {
    case GSMEM_FIT_BEST:
        for (; block != NULL; block = block->next) {
            candidate = (u8*)PC_PTR(((PC_PTR32(block) + alignment - 1) & ~mask));
            if (candidate + allocationSize < (u8*)block + block->size &&
                block->size < selected->size) {
                selected = block;
                address = candidate;
            }
        }
        break;
    case GSMEM_FIT_WORST:
        for (; block != NULL; block = block->next) {
            candidate = (u8*)PC_PTR(((PC_PTR32(block) + alignment - 1) & ~mask));
            if (candidate + allocationSize < (u8*)block + block->size &&
                block->size > selected->size) {
                selected = block;
                address = candidate;
            }
        }
        break;
    }

    if (selected == NULL) {
        lbl_8047AB3C = 2;
#ifdef TARGET_PC
        { /* port-gaps 74, observe-only: a failed GS allocation is logged with ORRE_HEAP_LOG */
            extern void pc_heap_gsmem_report(long freeTotal, long largest, unsigned blocks, u32 failedSize,
                                             unsigned long long lowest, unsigned long long reachEnd);
            pc_heap_gsmem_report(-1, -1, 0, size, 0, 0);
        }
#endif
        return 0;
    }
    handle = GSmemShrink(address, size);
    GSmemEntryFromHandle(handle)->align = alignment;
    return handle;
}

u16 GSmemShrink(u8* allocation, u32 size)
{
    GSmemBlock* block = lbl_8047AB30;
    GSmemBlock* suffix;
    GSmemEntry* entry;
    GSmemEntry* previousEntry;
    void* previousData;
    u32 allocationSize;
    u32 prefixSize;
    u32 suffixSize;

    if (size == 0) {
        return 0;
    }
#ifdef TARGET_PC
    { /* port-gaps 83, observe-only: ORRE_HEAP_GS_WATCH names who allocates the watched address */
        extern void pc_heap_gsmem_watch(const void* block, unsigned size, int kind);
        pc_heap_gsmem_watch(allocation, size, 'A');
    }
#endif

    size = (size + 0x1F) & ~0x1F;
    if (lbl_8047AB28 != 0) {
        allocation -= 4;
        allocationSize = ((size + 3) & ~3) + 8;
    } else {
        allocationSize = (size + 3) & ~3;
    }
    if (allocationSize < sizeof(GSmemBlock)) {
        allocationSize = sizeof(GSmemBlock);
    }

    for (; block != NULL; block = block->next) {
        if (allocation >= (u8*)block && allocation <= (u8*)block + block->size) {
            break;
        }
    }
    if (block == NULL) {
        lbl_8047AB3C = 2;
        return 0;
    }
    if (allocation + allocationSize > (u8*)block + block->size) {
        lbl_8047AB3C = 2;
        return 0;
    }

    entry = GSmemGetFreeEntry();
    if (entry == NULL) {
        lbl_8047AB3C = 3;
        return 0;
    }

    prefixSize = allocation - (u8*)block;
    suffix = (GSmemBlock*)((u8*)block + prefixSize + allocationSize);
    suffixSize = block->size - (prefixSize + allocationSize);
    if (suffixSize < sizeof(GSmemBlock)) {
        suffix = NULL;
    }

    if (prefixSize < sizeof(GSmemBlock)) {
        previousData = GSmemFindPreviousData(block);
        if (previousData == NULL) {
            if (prefixSize != 0) {
                GSlogWrite(lbl_80270D78, prefixSize, allocation);
            }
            block = NULL;
        } else {
            if (block->prev != NULL) {
                if (block->next == NULL) {
                    block->prev->next = NULL;
                } else {
                    block->prev->next = block->next;
                }
                block = block->prev;
            } else {
                if (suffix != NULL) {
                    suffix->prev = NULL;
                    suffix->next = block->next;
                    lbl_8047AB30 = suffix;
                } else {
                    lbl_8047AB30 = block->next;
                }
                if (block->next != NULL) {
                    block->next->prev = suffix;
                }
                block = NULL;
            }
            if (prefixSize != 0) {
                previousEntry = GSmemFindEntryByData(previousData);
                if (previousEntry == NULL) {
                    GSlogWrite(lbl_80270DD0);
                    return 0;
                }
                previousEntry->size += prefixSize;
                if (lbl_8047AB28 != 0) {
                    GSmemClearGuards(previousEntry);
                    if (previousEntry->lockCount == 0) {
                        previousEntry->checksum = GSmemChecksum(previousEntry);
                    }
                }
            }
        }
    } else {
        block->size = prefixSize;
    }

    entry->handle = GSmemEntryHandle(entry);
    entry->lockCount = 0;
    entry->data = allocation;
    entry->size = allocationSize;
    entry->align = 0xFFFF;
    entry->checksum = 0;
    if (block != NULL) {
        if (suffix != NULL) {
            suffix->prev = block;
            suffix->next = block->next;
            suffix->size = suffixSize;
            if (block->next != NULL) {
                block->next->prev = suffix;
            }
            block->next = suffix;
        } else {
            entry->size += suffixSize;
            if (block->next != NULL) {
                block->next->prev = block;
            }
        }
    } else if (suffix != NULL) {
        suffix->prev = NULL;
        suffix->next = lbl_8047AB30->next;
        suffix->size = suffixSize;
        lbl_8047AB30 = suffix;
    } else {
        entry->size += suffixSize;
    }

    if (lbl_80478AF0 == 1) {
        memset(entry->data, 0, entry->size);
        DCFlushRange(entry->data, entry->size);
    }
    if (lbl_8047AB28 != 0) {
        GSmemClearGuards(entry);
        entry->checksum = GSmemChecksum(entry);
    }

    lbl_8047AB3C = 0;
    lbl_8047AB60++;
    lbl_8047AB4C++;
    GS_PC_CHECK("after GSmemShrink");
    return entry->handle;
}

u16 _toolentryAlloc__FUl(u32 size)
{
    size = (size + 0x1F) & ~0x1F;
    return GSmemAlloc(size, 0x20);
}

void fn_800E3560(s32 strategy)
{
    lbl_8047AB2C = strategy;
}

void GSmemInit(u32 guardMode, void* start, void* end)
{
    GSmemBlock* block;

    lbl_8047AB28 = guardMode;
    lbl_8047AB68 = (void*)PC_PTR(((PC_PTR32(start) + 0x1F) & ~0x1F));
    lbl_8047AB64 = (void*)PC_PTR((PC_PTR32(end) & ~0x1F));
    lbl_8047AB60 = 0;
    lbl_8047AB5C = 0;
    lbl_8047AB58 = 0;
    lbl_8047AB54 = 0;
    lbl_8047AB50 = 0;
    lbl_8047AB4C = 0;
    lbl_8047AB48 = 0;

    lbl_8047AB38 = lbl_8047AB34 = (GSmemEntry*)lbl_8047AB64 - 1;
    lbl_8047AB34->handle = 0;

    block = lbl_8047AB68;
    block->prev = NULL;
    block->next = NULL;
    block->size = (u8*)lbl_8047AB38 - (u8*)lbl_8047AB68;
    lbl_8047AB30 = block;
    GSlogWrite(lbl_80270DFC, lbl_8047AB68, lbl_8047AB64);
}
