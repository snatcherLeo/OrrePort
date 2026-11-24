/**
 * @file hooks_types.h
 * @brief MusyX memory hooks (SND_HOOKS, MusyX 2.0.0: malloc and free).
 *
 * Moved here unchanged from src/musyx/runtime/hardware.c (2026-10-15), so the PC build can check this layout against
 * AxioDL MusyX's (port/test/musyx_layout_check) and define the shared global with one type.
 */
#ifndef MUSYX_RUNTIME_HOOKS_TYPES_H
#define MUSYX_RUNTIME_HOOKS_TYPES_H

#include "dolphin/types.h"

typedef struct SND_HOOKS {
    void* (*malloc)(u32 len);
    void (*free)(void* addr);
} SND_HOOKS;

#endif /* MUSYX_RUNTIME_HOOKS_TYPES_H */
