#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/mtx.h); docs/port/stage2-plan.md. */
#include_next <dolphin/mtx.h>
#ifndef ORRE_PC_MTX_EXTRA
#define ORRE_PC_MTX_EXTRA
typedef Vec Vec3; /* our reconstructed header's second name for Vec */
#endif
#else /* GameCube build: unchanged */
/**
 * @file mtx.h
 * @brief Dolphin SDK matrix/vector types.
 *
 * The SDK's Mtx library types, as used by the HSD (sysdolphin) middleware.
 * Having a real Vec type lets HSD code assign vectors by value
 * (`wobj->pos = *position;`) the way the original sources did, instead of
 * copying through `u32*` casts.
 *
 * Type layout cross-checked against doldecomp/melee
 * extern/dolphin/include/dolphin/mtx.h at commit
 * 81153faa93ea558e08d102007f9b663ddb526884.
 */
#ifndef DOLPHIN_MTX_H
#define DOLPHIN_MTX_H

#include "dolphin/types.h"

typedef struct Vec {
    f32 x;
    f32 y;
    f32 z;
} Vec, Vec3;

typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];

void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst);

#endif /* DOLPHIN_MTX_H */
#endif /* TARGET_PC */
