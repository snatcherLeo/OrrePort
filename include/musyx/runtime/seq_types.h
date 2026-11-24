/**
 * @file seq_types.h
 * @brief MusyX sequencer types (NOTE ... SEQ_INSTANCE), shared by the seq units and the PC data file.
 *
 * Moved here unchanged from the identical copies in src/musyx/runtime/seq.c, seq_exact_80146E88.c and
 * seq_residual_801485FC.c (2026-10-15), so that port/src/musyx_pc_data.c can define the sequencer
 * globals with their real types on the PC. Layout notes (sizes, field offsets, the pre-2.0.1 PRG_STATE) are in the
 * comment block above the include in seq.c. Offsets in the comments are the GameCube layout; on a 64-bit PC the
 * pointer fields make the structs larger.
 */
#ifndef MUSYX_RUNTIME_SEQ_TYPES_H
#define MUSYX_RUNTIME_SEQ_TYPES_H

#include "dolphin/types.h"

typedef struct NOTE {
    struct NOTE* next; // 0x0
    struct NOTE* prev; // 0x4
    u32 id;            // 0x8
    s32 endTime;        // 0xC
    u8 section;          // 0x10
    u8 timeIndex;         // 0x11
    u8 reserved[2];        // 0x12
} NOTE; // size 0x14

typedef struct {
    u32 seqId1;         // 0x0
    u16 time1;           // 0x4
    u16 pad_6;            // 0x6
    u32 seqId2;            // 0x8
    u16 time2;              // 0xC
    u16 pad_E;               // 0xE
    void* arr2;               // 0x10
    u16 gid2;                  // 0x14
    u16 sid2;                    // 0x16
    u8 vol2;                      // 0x18
    u8 studio2;                     // 0x19
    u16 pad_1A;                      // 0x1A
    u32 trackMute2[2];                // 0x1C
    u16 speed2;                        // 0x24
    u8 flags;                            // 0x26
    u8 pad_27;                            // 0x27
} SND_CROSSFADE; // size 0x28

typedef struct {
    volatile u32 time; // 0x0
    u32 bpm;            // 0x4
} MTRACK_DATA;           // size 0x8

typedef struct {
    MTRACK_DATA* base; // 0x0
    MTRACK_DATA* addr; // 0x4
} MTRACK;               // size 0x8

typedef struct {
    u32 low;  // 0x0
    s32 high; // 0x4
} TICKS;      // size 0x8

typedef struct {
    u32 time;       // 0x0
    u8 prgChange;    // 0x4
    u8 velocity;      // 0x5
    u8 res[2];         // 0x6
    u16 pattern;        // 0x8
    s8 transpose;        // 0xA
    s8 velocityAdd;       // 0xB
} TENTRY; // size 0xC

typedef struct {
    TENTRY* base; // 0x0
    TENTRY* addr; // 0x4
} TRACK;          // size 0x8

typedef struct {
    u16 time;     // 0x0
    u8 key;        // 0x2
    u8 velocity;    // 0x3
    u16 length;      // 0x4
} NOTE_DATA;          // size 0x6

typedef struct {
    u8* nextAddr;    // 0x0
    u16 value;        // 0x4
    s16 nextDelta;     // 0x6
    u32 nextTime;       // 0x8
} SEQ_STREAM;            // size 0xC

typedef struct {
    u32 lTime;                // 0x0
    u32 baseTime;              // 0x4
    NOTE_DATA* addr;            // 0x8
    TENTRY* patternInfo;         // 0xC
    SEQ_STREAM pitchBend;          // 0x10
    SEQ_STREAM modulation;           // 0x1C
    u8 midi;                          // 0x28
    u8 pad_29[3];                      // 0x29
} CPAT;                                 // size 0x2C

typedef struct SEQ_EVENT {
    struct SEQ_EVENT* next; // 0x0
    struct SEQ_EVENT* prev;  // 0x4
    u32 time;                 // 0x8
    union {
        TENTRY* trackAddr;
        struct {
            NOTE_DATA* addr; // 0x0
            CPAT* base;       // 0x4
        } pattern;
    } info;               // 0xC
    u8 type;               // 0x14
    u8 trackId;              // 0x15
    u8 pad_16[2];             // 0x16
} SEQ_EVENT;                   // size 0x18

typedef struct {
    u16 macId;     // 0x0
    u8 priority;    // 0x2
    u8 maxVoices;    // 0x3
} PRG_STATE;          // size 0x4 (pre-2.0.1 variant, no `program` field --
                       // proven by prgState[16] gap == 0x40 == 16*4)

typedef struct {
    u16 macro;    // 0x0
    u8 prio;       // 0x2
    u8 maxVoices;   // 0x3
    u8 index;        // 0x4
    u8 reserved;      // 0x5
} PAGE;                // size 0x6

typedef struct {
    u32 tTab;          // 0x0
    u32 pTab;           // 0x4
    u32 tmTab;           // 0x8
    u32 mTrack;           // 0xC
    u32 info;              // 0x10
    u32 loopPoint[16];      // 0x14
    u32 tsTab;               // 0x54
} ARR;                        // size 0x58

#define ARR_GET(arr, offset) ((void*)PC_PTR((offset) + PC_PTR32(arr)))
#define ARR_GET_TYPE(arr, offset, ty) ((ty)ARR_GET(arr, offset))

typedef struct {
    MTRACK mTrack;              // 0x0
    u32 bpm;                     // 0x8
    TICKS tickDelta[2];           // 0xC
    SEQ_EVENT* globalEventRoot;     // 0x1C
    TICKS time[2];                    // 0x20
    u8 timeIndex;                       // 0x30
    u8 pad_31;                            // 0x31
    u16 speed;                              // 0x32
    u16 loopCnt;                              // 0x34
    u8 loopDisable;                             // 0x36
    u8 pad_37;                                    // 0x37
} SEQ_SECTION; // size 0x38

typedef struct SEQ_INSTANCE {
    struct SEQ_INSTANCE* next; // 0x0
    struct SEQ_INSTANCE* prev; // 0x4
    u8 state;                   // 0x8
    u8 index;                    // 0x9
    u16 groupID;                  // 0xA
    u32 publicId;                   // 0xC
    PAGE* normtab;                    // 0x10
    u8 normTrans[128];                  // 0x14
    PAGE* drumtab;                        // 0x94
    u8 drumTrans[128];                     // 0x98
    ARR* arrbase;                            // 0x118
    u32 trackMute[2];                          // 0x11C
    TRACK track[64];                             // 0x124
    u8 trackVolGroup[64];                          // 0x324
    CPAT pattern[64];                                // 0x364
    NOTE* noteUsed[2];                                 // 0xE64
    NOTE* noteKeyOff;                                    // 0xE6C
    PRG_STATE prgState[16];                                // 0xE70
    u8 defVGroup;                                            // 0xEB0
    u8 pad_EB1[3];                                             // 0xEB1
    SND_CROSSFADE syncCrossInfo;                                 // 0xEB4
    u32* syncSeqIdPtr;                                             // 0xEDC
    u8 syncActive;                                                   // 0xEE0
    u8 defStudio;                                                     // 0xEE1
    u8 keyOffCheck;                                                    // 0xEE2
    u8 pad_EE3;                                                         // 0xEE3
    SEQ_EVENT event[64];                                                 // 0xEE4
    u8* trackSectionTab;                                                   // 0x14E4
    SEQ_SECTION section[16];                                                // 0x14E8
} SEQ_INSTANCE; // size 0x1868

#endif /* MUSYX_RUNTIME_SEQ_TYPES_H */
