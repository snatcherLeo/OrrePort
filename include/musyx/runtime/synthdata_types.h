/**
 * @file synthdata_types.h
 * @brief MusyX data-manager table types (key maps, layers, sample directories, FX groups, macro tables).
 *
 * Moved here unchanged from src/musyx/runtime/synthdata.c (2026-10-15), so that port/src/musyx_pc_data.c can define the
 * MusyX globals with their real types on the PC. Offsets in the comments are the GameCube layout; on a 64-bit
 * PC the pointer fields make the structs larger.
 * SdirDataT holds a pointer (`addr`), so on the PC it is larger than the disc's 0x20-byte entry; on the PC the
 * disc's sample directories are read by AxioDL's PC code instead (docs/port/musyx-evaluation.md, "Decisions").
 */
#ifndef MUSYX_RUNTIME_SYNTHDATA_TYPES_H
#define MUSYX_RUNTIME_SYNTHDATA_TYPES_H

#include "dolphin/types.h"

typedef struct { void* data; u16 id; u16 refCount; } DataTabT;               /* keymap/curve entry, 8 bytes */
typedef struct { void* data; u16 id; u16 num; u16 refCount; } LayerTabT;     /* layer entry, 12 bytes */
typedef struct { void* data; void* base; u16 numSmp; } SdirTabT;             /* sample-dir directory entry, 12 bytes */
typedef struct {
    u32 info;
    u32 length;
    u32 loopOffset;
    u32 loopLength;
    u32 extraData;
} SdirHeaderT;
typedef struct {
    u16 id;
    u16 refCount;
    u32 offset;
    void* addr;
    SdirHeaderT header;
} SdirDataT; /* sample-dir data entry, 0x20 bytes */
typedef struct {
    u32 info;
    void* addr;
    void* extraData;
    u32 offset;
    u32 length;
    u32 loop;
    u32 loopLength;
    u8 compType;
} SampleInfoT;
typedef struct { u8 pad[9]; u8 vGroup; } FxEntryT;                            /* individual FX_TAB entry, 0xA bytes (only vGroup@9 used here) */
typedef struct { u16 gid; u16 fxNum; void* fxTab; } FxGroupT;                 /* FX group cluster entry, 8 bytes */
typedef struct { u16 num; u16 subTabIndex; } MacMainEntryT;                   /* macro main-table entry, 4 bytes */
typedef struct { void* data; u16 id; u16 refCount; } MacSubEntryT;            /* macro sub-table entry, 8 bytes */

#endif /* MUSYX_RUNTIME_SYNTHDATA_TYPES_H */
