/**
 * @file vsamples_types.h
 * @brief MusyX virtual-sample streaming types (SND_VIRTUALSAMPLE_INFO, VS_BUFFER, VS).
 *
 * Moved here unchanged from src/musyx/runtime/synth_vsamples.c (2026-10-15), so the PC build can check this layout against
 * AxioDL MusyX's (port/test/musyx_layout_check) and define the shared global with one type.
 */
#ifndef MUSYX_RUNTIME_VSAMPLES_TYPES_H
#define MUSYX_RUNTIME_VSAMPLES_TYPES_H

#include "dolphin/types.h"

typedef struct SND_VIRTUALSAMPLE_INFO {
    u16 smpID;
    u16 instID;
    union {
        struct {
            u32 off1;
            u32 len1;
            u32 off2;
            u32 len2;
        } update;
    } data;
} SND_VIRTUALSAMPLE_INFO;

typedef struct VS_BUFFER {
    u8 state;
    u8 hwId;
    u8 smpType;
    u8 voice;
    u32 last;
    u32 finalGoodSamples;
    u32 finalLast;
    SND_VIRTUALSAMPLE_INFO info;
} VS_BUFFER;

typedef struct VS {
    u8 numBuffers;
    u32 bufferLength;
    VS_BUFFER streamBuffer[64];
    u8 voices[64];
    u16 nextInstID;
    u32 (*callback)(u8 reason, const SND_VIRTUALSAMPLE_INFO* info);
} VS;

#endif /* MUSYX_RUNTIME_VSAMPLES_TYPES_H */
