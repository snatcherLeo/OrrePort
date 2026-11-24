/**
 * @file gs_math_random_exact_800E0BA0.c
 * @brief Exact floating-point random helpers.
 *
 * Address range: 0x800E0BA0 - 0x800E0C04.
 */

#include "dolphin/types.h"

extern f32 HSD_Randf(void);
extern f32 lbl_8047CB10;

#ifdef TARGET_PC
/* with ORRE_RNG_TRACE=1: the wrappers' own callers (the HSD trace names only the wrapper), so particle, camera and
 * effect draws can be told apart (the request, 2026-10-18; observe only) */
static void pc_randf_trace(const char* which, void* caller)
{
    extern int pc_rng_trace_active(void); /* src/hsd/random.c: ORRE_RNG_TRACE=1 or a frame range */
    extern void OSReport(const char*, ...);
    if (pc_rng_trace_active()) {
        OSReport("[rngtrace] %s from %p\n", which, caller);
    }
}
#endif

f32 fn_800E0BA0(void)
{
#ifdef TARGET_PC
    pc_randf_trace("randpm", __builtin_return_address(0));
#endif
    f32 a = HSD_Randf();
    f32 b = HSD_Randf();

    return b + a - lbl_8047CB10;
}

f32 GSrandf(void)
{
#ifdef TARGET_PC
    pc_randf_trace("randf", __builtin_return_address(0));
#endif
    return HSD_Randf();
}
