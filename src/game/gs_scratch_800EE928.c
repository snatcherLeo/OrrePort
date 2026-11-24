/**
 * GSscratch, first part (0x800EE928-0x800EEC38).
 *
 * GSscratch manages a scratch area in the Gekko locked cache at 0xE0000000:
 * 32 blocks of 0x200 bytes, tracked by a bitmap (bit 31 = block 0) and 32
 * allocation records. The retail TU is 0x800EE928-0x800EEF48 (-O4,p); this
 * object covers its first six functions in binary order. GSscratchAlloc
 * (0x800EEC38) and GSscratchInit (0x800EEDF8, retail inline asm) follow in
 * separate objects, so all state stays extern.
 *
 * GSscratchFindAllocation is the record search the target expands in both
 * GSscratchFree (by first block) and GSscratchAlloc (free record, 0xFF); the
 * loop-exhausted `li rX,0` feeding the NULL test is the inlined return.
 */
#include "dolphin/types.h"

typedef struct GSscratchAllocation {
    u8 firstBlock; /* 0xFF: record unused */
    u8 blockCount;
    u8 _pad[2];
    void (*callback)(BOOL valid, void *ptr, u8 blockCount);
} GSscratchAllocation;

extern GSscratchAllocation _blockTable[32]; /* allocation records */
extern u32 _entriesQueued;                     /* locked-cache DMA transactions queued */
extern u8 *_scratchPadBase;                     /* scratch base (0xE0000000) */
extern u8 _scratchInvalid;                      /* 1: scratch contents invalid */
extern u32 _blockMap;                     /* used-block bitmap */

extern void LCQueueWait(u32 len);
extern u32 LCQueueLength(void);
extern u32 LCStoreData(void *dest, void *src, u32 len);

void GSscratchSetValid(void)
{
    s32 count;
    GSscratchAllocation *allocation;

    if (_scratchInvalid != 0) {
        allocation = _blockTable;
        count = 32;
        while (count-- != 0) {
            if (allocation->firstBlock != 0xFF &&
                allocation->callback != NULL) {
                allocation->callback(
                    TRUE,
                    _scratchPadBase + allocation->firstBlock * 0x200,
                    allocation->blockCount);
            }
            allocation++;
        }
        _scratchInvalid = 0;
    }
}

void GSscratchSetInvalid(void)
{
    s32 count;
    GSscratchAllocation *allocation;

    if (_scratchInvalid != 1) {
        allocation = _blockTable;
        count = 32;
        while (count-- != 0) {
            if (allocation->firstBlock != 0xFF &&
                allocation->callback != NULL) {
                allocation->callback(
                    FALSE,
                    _scratchPadBase + allocation->firstBlock * 0x200,
                    allocation->blockCount);
            }
            allocation++;
        }
        _scratchInvalid = 1;
    }
}

u8 GSscratchIsPtr(void *ptr)
{
    return (PC_PTR32(ptr) & 0xF0000000) == (PC_PTR32(_scratchPadBase) & 0xF0000000);
}

void GSscratchWaitForCompletion(void)
{
    LCQueueWait(_entriesQueued);
    _entriesQueued = 0;
}

u32 GSscratchStore(void *dest, void *src, u32 len)
{
    u32 queued;

    if (LCQueueLength() >= 15) {
        return 1;
    }
    if ((PC_PTR32(src) & 0x1F) != 0 || (PC_PTR32(dest) & 0x1F) != 0) {
        return 2;
    }
    if ((len & 0x1F) != 0) {
        return 2;
    }

    queued = LCStoreData(dest, src, len);
    _entriesQueued += queued;
    return 0;
}

static inline GSscratchAllocation *GSscratchFindAllocation(u8 firstBlock)
{
    GSscratchAllocation *allocation;
    u32 i;

    allocation = _blockTable;
    for (i = 0; i < 32; allocation++, i++) {
        if (allocation->firstBlock == firstBlock) {
            return allocation;
        }
    }
    return NULL;
}

void GSscratchFree(void *ptr)
{
    GSscratchAllocation *allocation;
    u32 blockMask;
    u8 firstBlock;
    u8 blockCount;
    u32 usedBlocks;

    allocation = GSscratchFindAllocation(
        (PC_PTR32(ptr) - PC_PTR32(_scratchPadBase)) >> 9);
    if (allocation == NULL) {
        return;
    }

    blockCount = allocation->blockCount;
    firstBlock = allocation->firstBlock;
    blockMask = 0x80000000;
    while (firstBlock-- != 0) {
        blockMask >>= 1;
    }

    usedBlocks = _blockMap;
    while (blockCount-- != 0) {
        usedBlocks &= ~blockMask;
        blockMask >>= 1;
    }
    _blockMap = usedBlocks;
    allocation->firstBlock = 0xFF;
}
