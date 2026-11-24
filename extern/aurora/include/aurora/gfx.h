#ifndef AURORA_GFX_H
#define AURORA_GFX_H

#ifdef __cplusplus
#include <cstdint>

extern "C" {
#else
#include "stdint.h"
#endif

#if !defined(NDEBUG) && !defined(AURORA_GFX_DEBUG_GROUPS)
#define AURORA_GFX_DEBUG_GROUPS
#endif

void push_debug_group(const char* label);
void pop_debug_group();

typedef struct {
  uint32_t queuedPipelines;
  uint32_t createdPipelines;
  uint32_t drawCallCount;
  uint32_t mergedDrawCallCount;
  uint32_t lastVertSize;
  uint32_t lastUniformSize;
  uint32_t lastIndexSize;
  uint32_t lastStorageSize;
  uint32_t lastTextureUploadSize;
} AuroraStats;

const AuroraStats* aurora_get_stats();
float aurora_get_fps();

void aurora_enable_vsync(bool enabled);

/* OrrePort (the font renderer's part B, 2026-11-13; docs/port/font-renderer.md): a sharper copy of a texture the
 * game writes from the CPU (the text atlas). When a texture whose data is `gameData` is drawn, Aurora draws `pixels`
 * instead: an 8-bit intensity image of `width` x `height` (any size: the game's UVs are normalised). Before drawing,
 * Aurora calls `takeDirty(gameData, rect)` to fetch and clear the rectangle the port changed since the last call
 * (rect = x0, y0, x1, y1, exclusive; x1 <= x0 for nothing) and uploads only that, at the moment it would have read the
 * game's own texture, so the order of writes and draws stays the game's. Why: re-uploading a 4K-sized atlas every
 * frame would cost performance (snatcherLeo: "it can't hamper game performance"). All platforms the same (WebGPU). */
typedef void (*AuroraTakeDirtyRect)(const void* gameData, uint32_t rect[4]);
void aurora_orre_hires_texture_register(const void* gameData, const uint8_t* pixels, uint32_t width, uint32_t height,
                                        AuroraTakeDirtyRect takeDirty);
void aurora_orre_hires_texture_unregister(const void* gameData);

#ifdef __cplusplus
}
#endif

#endif
