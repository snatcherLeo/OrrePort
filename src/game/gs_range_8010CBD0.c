/**
 * @file gs_range_8010CBD0.c
 * @brief GScolsys2 floor/CCD management, 0x8010CBD0 - 0x8010D170.
 *
 * A function-boundary carve of the GScolsys2 core TU. The core owns the
 * collision state GScolsys2 and runs from GScolsys2GetObjEnable
 * (0x8010C7BC) through the debug draw (fn_8010D8D4), the last function
 * that addresses the state directly; everything after it goes through
 * the GScolsys2GetCCDFileHead / GScolsys2GetCurFloor accessors. Built with the core
 * TU's flags (GC/1.3, project defaults), with which every core function
 * matched so far is exact; the state stays extern here.
 */
#include "dolphin/types.h"
#include "game/gs_colsys.h"

extern GSColSysState GScolsys2;

extern void GSgfxDLFree(void* displayList);

/* One CCD object record, 0x40 bytes; the offsets in it are relative to
 * the file head until _offsetCCD resolves them. */
typedef struct CcdOffsetBlock {
    PTR32_FIELD(void) field_00; /* 32-bit slot on the PC too (pc/ptr32.h): the collision file keeps its layout */
    u32 field_04;
    PTR32_FIELD(void) field_08;
    PTR32_FIELD(void) field_0C;
} CcdOffsetBlock;

typedef struct CcdOffsetRecord {
    Vec3f trans;
    Vec3f rot;
    Vec3f scale;
    PTR32_FIELD(CcdOffsetBlock) field_24;
    PTR32_FIELD(CcdOffsetBlock) field_28;
    PTR32_FIELD(CcdOffsetBlock) field_2C;
    PTR32_FIELD(CcdOffsetBlock) field_30;
    PTR32_FIELD(CcdOffsetBlock) field_34;
    PTR32_FIELD(CcdOffsetBlock) field_38;
    u16 flags;
    u8 pad_3E[2];
} CcdOffsetRecord;

typedef struct CCD_FILEHEAD {
    PTR32_FIELD(CcdOffsetRecord) records; /* 32-bit slot on the PC too (pc/ptr32.h): the collision file keeps its layout */
    u32 count;
} CCD_FILEHEAD;

/* 0x8010CBD0 | 0x34 */
GSColFloor* GScolsys2GetCurFloor(void)
{
    s32 layer;

    layer = GScolsys2.activeLayer;
    if (layer < 0 || layer >= GSCOLSYS_MAX_LAYERS) {
        return NULL;
    }
    return &GScolsys2.floors[layer];
}

/* 0x8010CC04 | 0x50 */
s32 GScolsys2UnloadCCD(void)
{
    P32_SET(GScolsys2.wzxDataPtr, NULL);
    if (P32(GScolsys2.displayList) != NULL) {
        GSgfxDLFree(P32(GScolsys2.displayList));
        P32_SET(GScolsys2.displayList, NULL);
    }
    return 1;
}

/* 0x8010CC54 | 0x118 */
void fn_8010CC54(void)
{
    GSColFloor* floor;
    u32 i;

    floor = GScolsys2GetCurFloor();
    if (floor != NULL) {
        for (i = 0; i < 48; i++) {
            floor->events[i].flags &= 0xFFFE;
        }
    }
    P32_SET(GScolsys2.wzxDataPtr, NULL);
}

/* 0x8010CD6C | 0x98 */
void fn_8010CD6C(void)
{
    CCD_FILEHEAD* head;
    u32 i;
    CcdOffsetRecord* source;
    GSColFloorObj* destination;

    head = P32(GScolsys2.wzxDataPtr);
    if (head == NULL) {
        return;
    }

    i = 0;
    source = P32(head->records);
    destination = GScolsys2.floors[GScolsys2.activeLayer].objs;
    while (i < head->count) {
        destination->trans = source->trans;
        destination->rot = source->rot;
        destination->scale = source->scale;
        destination->flags = 0;
        i++;
        source++;
        destination++;
    }
}

/* 0x8010CE04 | 0x1E0 */
#ifdef TARGET_PC
/* PC: the collision file (docs/engine/collision.md) is converted to native byte order once, here, then its offsets are
 * turned into pointers kept in the same 32-bit slots (the views use PTR32_FIELD, so the file keeps its layout). Field
 * sizes from the decoded format: record 0x40 (9 f32, 6 group offsets, u16 flags, pad); group 0x24 (triangle offset,
 * u32 count, cell offset, index-list offset, u16 x2 cell counts, 4 f32), only 8 bytes (offset, count) for groups 3 and
 * 5; triangle 0x34 (12 f32, then u8 attribute A,
 * u8 attribute B, u16 event id; the examine-plane group keeps a u16 id at +0x30 instead) or 0x30 in the +0x38 group;
 * grid cells {u32 first, u32 count}; index lists u32. Group 3 (+0x30) and group 5 (+0x38) have no grid. Each block is
 * converted once even if several records point at it. */
static u32 ccdSwap32(u8* p)
{
    u32 v = __builtin_bswap32(*(u32*)p);
    *(u32*)p = v;
    return v;
}

static u16 ccdSwap16(u8* p)
{
    u16 v = __builtin_bswap16(*(u16*)p);
    *(u16*)p = v;
    return v;
}

#define CCD_SEEN_MAX 2048
static u8* ccdSeen[CCD_SEEN_MAX];
static u32 ccdSeenCount;

static int ccdFirstVisit(u8* p)
{
    u32 i;
    for (i = 0; i < ccdSeenCount; i++) {
        if (ccdSeen[i] == p) {
            return 0;
        }
    }
    if (ccdSeenCount < CCD_SEEN_MAX) {
        ccdSeen[ccdSeenCount++] = p;
    }
    return 1;
}

static void ccdConvertGroup(u8* base, u8* group, u32 kind)
{
    u32 triOffset = ccdSwap32(group + 0x00);
    u32 triCount = ccdSwap32(group + 0x04);
    u32 cellOffset = 0;
    u32 indexOffset = 0;
    u32 cellsX = 0;
    u32 cellsZ = 0;
    u32 triSize = (kind == 5) ? 0x30 : 0x34;
    u32 i;
    u32 k;

    /* Groups 3 and 5 have only the 8-byte {triangle offset, count} head (GSFieldWzxTriangleList), and their triangles
     * follow it directly. Corrected 2026-10-21 (verified): they were swapped as 0x24-byte grid heads, which swapped
     * the first triangle's first 0x1C bytes a second time, so the first event triangle of every room read as garbage
     * (P02: the second half of Phenac's counter quad, the talk Wes started there in Dolphin never opened on the PC) */
    if (kind != 3 && kind != 5) {
        cellOffset = ccdSwap32(group + 0x08);
        indexOffset = ccdSwap32(group + 0x0C);
        cellsX = ccdSwap16(group + 0x10);
        cellsZ = ccdSwap16(group + 0x12);
        for (k = 0x14; k < 0x24; k += 4) {
            ccdSwap32(group + k);
        }
    }
    if (triOffset != 0 && ccdFirstVisit(base + triOffset)) {
        for (i = 0; i < triCount; i++) {
            u8* tri = base + triOffset + i * triSize;
            for (k = 0; k < 0x30; k += 4) {
                ccdSwap32(tri + k);
            }
            if (triSize == 0x34) {
                if (kind == 3) {
                    ccdSwap16(tri + 0x30); /* examine planes: u16 id at +0x30 */
                }
                ccdSwap16(tri + 0x32);
            }
        }
    }
    if (triOffset != 0) {
        *(u32*)(group + 0x00) = PC_PTR32(base + triOffset);
    }
    if (kind == 3 || kind == 5) {
        return; /* no grid: the GameCube fixes only the triangle offset here */
    }
    if (cellOffset != 0 && ccdFirstVisit(base + cellOffset)) {
        u32 maxIndex = 0;
        for (i = 0; i < cellsX * cellsZ; i++) {
            u8* cell = base + cellOffset + i * 8;
            u32 first = ccdSwap32(cell);
            u32 count = ccdSwap32(cell + 4);
            if (count != 0 && first + count > maxIndex) {
                maxIndex = first + count;
            }
        }
        if (indexOffset != 0 && ccdFirstVisit(base + indexOffset)) {
            for (i = 0; i < maxIndex; i++) {
                ccdSwap32(base + indexOffset + i * 4);
            }
        }
    }
    if (cellOffset != 0) {
        *(u32*)(group + 0x08) = PC_PTR32(base + cellOffset);
    }
    if (indexOffset != 0) {
        *(u32*)(group + 0x0C) = PC_PTR32(base + indexOffset);
    }
}

void _offsetCCD__FP12CCD_FILEHEAD(CCD_FILEHEAD* head)
{
    u8* base = (u8*)head;
    u32 recordOffset;
    u32 count;
    u32 i;
    u32 g;
    u32 k;

    if (head == NULL) {
        return;
    }
    ccdSeenCount = 0;
    recordOffset = ccdSwap32(base + 0);
    count = ccdSwap32(base + 4);
    *(u32*)(base + 0) = PC_PTR32(base + recordOffset);
    for (i = 0; i < count; i++) {
        u8* record = base + recordOffset + i * 0x40;
        for (k = 0; k < 0x24; k += 4) {
            ccdSwap32(record + k); /* translation, rotation, scale */
        }
        ccdSwap16(record + 0x3C); /* flags */
        ccdSwap16(record + 0x3E);
        for (g = 0; g < 6; g++) {
            u32 groupOffset = ccdSwap32(record + 0x24 + g * 4);
            if (groupOffset != 0) {
                if (ccdFirstVisit(base + groupOffset)) {
                    ccdConvertGroup(base, base + groupOffset, g);
                }
                *(u32*)(record + 0x24 + g * 4) = PC_PTR32(base + groupOffset);
            }
        }
    }
}
#else
void _offsetCCD__FP12CCD_FILEHEAD(CCD_FILEHEAD* head)
{
    CcdOffsetRecord* record;
    CcdOffsetBlock* block;
    u32 i;

    if (head == NULL) {
        return;
    }
    head->records = (CcdOffsetRecord*)PC_PTR((PC_PTR32(head->records) + PC_PTR32(head)));
    record = head->records;

    for (i = 0; i < head->count; i++, record++) {
        if (record->field_24 != NULL) {
            record->field_24 =
                (CcdOffsetBlock*)PC_PTR((PC_PTR32(record->field_24) + PC_PTR32(head)));
            block = record->field_24;
            if (block->field_00 != NULL) {
                block->field_00 = (void*)PC_PTR((PC_PTR32(block->field_00) + PC_PTR32(head)));
            }
            if (block->field_08 != NULL) {
                block->field_08 = (void*)PC_PTR((PC_PTR32(block->field_08) + PC_PTR32(head)));
            }
            if (block->field_0C != NULL) {
                block->field_0C = (void*)PC_PTR((PC_PTR32(block->field_0C) + PC_PTR32(head)));
            }
        }
        if (record->field_28 != NULL) {
            record->field_28 =
                (CcdOffsetBlock*)PC_PTR((PC_PTR32(record->field_28) + PC_PTR32(head)));
            block = record->field_28;
            if (block->field_00 != NULL) {
                block->field_00 = (void*)PC_PTR((PC_PTR32(block->field_00) + PC_PTR32(head)));
            }
            if (block->field_08 != NULL) {
                block->field_08 = (void*)PC_PTR((PC_PTR32(block->field_08) + PC_PTR32(head)));
            }
            if (block->field_0C != NULL) {
                block->field_0C = (void*)PC_PTR((PC_PTR32(block->field_0C) + PC_PTR32(head)));
            }
        }
        if (record->field_2C != NULL) {
            record->field_2C =
                (CcdOffsetBlock*)PC_PTR((PC_PTR32(record->field_2C) + PC_PTR32(head)));
            block = record->field_2C;
            if (block->field_00 != NULL) {
                block->field_00 = (void*)PC_PTR((PC_PTR32(block->field_00) + PC_PTR32(head)));
            }
            if (block->field_08 != NULL) {
                block->field_08 = (void*)PC_PTR((PC_PTR32(block->field_08) + PC_PTR32(head)));
            }
            if (block->field_0C != NULL) {
                block->field_0C = (void*)PC_PTR((PC_PTR32(block->field_0C) + PC_PTR32(head)));
            }
        }
        if (record->field_30 != NULL) {
            record->field_30 =
                (CcdOffsetBlock*)PC_PTR((PC_PTR32(record->field_30) + PC_PTR32(head)));
            block = record->field_30;
            if (block->field_00 != NULL) {
                block->field_00 = (void*)PC_PTR((PC_PTR32(block->field_00) + PC_PTR32(head)));
            }
        }
        if (record->field_34 != NULL) {
            record->field_34 =
                (CcdOffsetBlock*)PC_PTR((PC_PTR32(record->field_34) + PC_PTR32(head)));
            block = record->field_34;
            if (block->field_00 != NULL) {
                block->field_00 = (void*)PC_PTR((PC_PTR32(block->field_00) + PC_PTR32(head)));
            }
            if (block->field_08 != NULL) {
                block->field_08 = (void*)PC_PTR((PC_PTR32(block->field_08) + PC_PTR32(head)));
            }
            if (block->field_0C != NULL) {
                block->field_0C = (void*)PC_PTR((PC_PTR32(block->field_0C) + PC_PTR32(head)));
            }
        }
        if (record->field_38 != NULL) {
            record->field_38 =
                (CcdOffsetBlock*)PC_PTR((PC_PTR32(record->field_38) + PC_PTR32(head)));
            block = record->field_38;
            if (block->field_00 != NULL) {
                block->field_00 = (void*)PC_PTR((PC_PTR32(block->field_00) + PC_PTR32(head)));
            }
        }
    }
}
#endif

/* 0x8010CFE4 | 0x54 */
s32 fn_8010CFE4(CCD_FILEHEAD* fileHead)
{
    if (GScolsys2.activeLayer < 0) {
        return 0;
    }
    _offsetCCD__FP12CCD_FILEHEAD(fileHead);
    P32_SET(GScolsys2.wzxDataPtr, fileHead);
    return 1;
}

/* 0x8010D038 | 0x2C */
s32 fn_8010D038(void)
{
    s32 layer;

    layer = GScolsys2.activeLayer;
    if (layer < 0) {
        return 0;
    }
    GScolsys2.activeLayer = layer - 1;
    return 1;
}

/* 0x8010D064 | 0x10C */
s32 fn_8010D064(void)
{
    s32 layer;
    u32 i;

    layer = GScolsys2.activeLayer + 1;
    if (layer >= GSCOLSYS_MAX_LAYERS) {
        return 0;
    }

    P32_SET(GScolsys2.wzxDataPtr, NULL);
    for (i = 0; i < 48; i++) {
        GScolsys2.floors[layer].events[i].flags &= 0xFFFE;
    }
    GScolsys2.activeLayer = layer;
    return 1;
}
