/**
 * @file s_data_types.h
 * @brief MusyX group-stack types (GROUP_DATA, GSTACK).
 *
 * Moved here unchanged from src/musyx/runtime/s_data.c (2026-10-15), so that port/src/musyx_pc_data.c can define the
 * MusyX globals with their real types on the PC. Offsets in the comments are the GameCube layout; on a 64-bit
 * PC the pointer fields make the structs larger.
 */
#ifndef MUSYX_RUNTIME_S_DATA_TYPES_H
#define MUSYX_RUNTIME_S_DATA_TYPES_H

#include "dolphin/types.h"

typedef struct GROUP_DATA {
    u32 nextOff;
    u16 id;
    u16 type;
    u32 macroOff;
    u32 sampleOff;
    u32 curveOff;
    u32 keymapOff;
    u32 layerOff;
    union {
        struct {
            u32 normpageOff;
            u32 drumpageOff;
            u32 midiSetupOff;
        } song;
        struct {
            u32 tableOff;
        } fx;
    } data;
} GROUP_DATA;

typedef struct GSTACK {
    GROUP_DATA* gAddr;
    void* sdirAddr;
    void* prjAddr;
} GSTACK;

#endif /* MUSYX_RUNTIME_S_DATA_TYPES_H */
