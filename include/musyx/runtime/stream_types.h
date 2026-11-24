/**
 * @file stream_types.h
 * @brief MusyX stream types (SNDADPCMinfo, SND_ADPCMSTREAM_INFO, SAMPLE_INFO, STREAM_INFO).
 *
 * Moved here unchanged from src/musyx/runtime/stream.c (2026-10-15), so that port/src/musyx_pc_data.c can define the
 * MusyX globals with their real types on the PC. Offsets in the comments are the GameCube layout; on a 64-bit
 * PC the pointer fields make the structs larger.
 */
#ifndef MUSYX_RUNTIME_STREAM_TYPES_H
#define MUSYX_RUNTIME_STREAM_TYPES_H

#include "dolphin/types.h"

typedef struct SNDADPCMinfo {
  u16 numCoef;       // 0x0
  u8 initialPS;      // 0x2
  u8 loopPS;         // 0x3
  s16 loopY0;        // 0x4
  s16 loopY1;         // 0x6
  s16 coefTab[8][2]; // 0x8, size 0x20
} SNDADPCMinfo; // size 0x28

typedef struct SND_ADPCMSTREAM_INFO {
  s16 coefTab[8][2]; // 0x0, public-facing struct passed by callers - NOT the
                     // same layout as the internal SNDADPCMinfo embedded in
                     // STREAM_INFO (which has numCoef/initialPS/loopPS/loopY0/
                     // loopY1 before coefTab).
} SND_ADPCMSTREAM_INFO;

typedef struct SAMPLE_INFO {
  u32 info;        // 0x0
  void* addr;      // 0x4
  void* extraData; // 0x8
  u32 offset;      // 0xc
  u32 length;      // 0x10
  u32 loop;        // 0x14
  u32 loopLength;  // 0x18
  u8 compType;     // 0x1c
} SAMPLE_INFO; // size 0x20

typedef struct STREAM_INFO {
  u32 nextStreamHandle; // 0x0
  u32 stid;             // 0x4
  u32 flags;            // 0x8
  u8 state;             // 0xc
  u8 type;               // 0xd
  u8 hwStreamHandle;      // 0xe
  u8 lastPSFromBuffer;    // 0xf
  u32 (*updateFunction)(void* buffer1, u32 len1, void* buffer2, u32 len2, u32 user); // 0x10
  s16* buffer;          // 0x14
  u32 size;             // 0x18
  u32 bytes;            // 0x1c
  u32 last;             // 0x20
  SNDADPCMinfo adpcmInfo; // 0x24, size 0x28
  volatile u32 voice;   // 0x4c
  u32 user;             // 0x50
  u32 frq;              // 0x54
  u8 prio;              // 0x58
  u8 vol;               // 0x59
  u8 pan;               // 0x5a
  u8 span;              // 0x5b
  u8 auxa;              // 0x5c
  u8 auxb;              // 0x5d
  u8 orgPan;            // 0x5e
  u8 orgSPan;           // 0x5f
  u8 studio;            // 0x60
} STREAM_INFO; // size 0x64 (rounded up from 0x61)

#endif /* MUSYX_RUNTIME_STREAM_TYPES_H */
