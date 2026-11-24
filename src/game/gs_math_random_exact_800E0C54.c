/**
 * @file gs_math_random_exact_800E0C54.c
 * @brief Exact integer RNG and seed helpers.
 *
 * Address range: 0x800E0C54 - 0x800E0CA0.
 */

#include "dolphin/types.h"

extern u64 OSGetTime(void);
extern u32 HSD_Rand(void);
extern u32* lbl_80478C94;

u16 GSrand(void)
{
#ifdef TARGET_PC
    {
        /* with ORRE_RNG_TRACE=1: GSrand's own caller (the HSD_Rand trace only names GSrand); golden slice, the New
         * Game draw gap */
        extern int pc_rng_trace_active(void); /* src/hsd/random.c: ORRE_RNG_TRACE=1 or a frame range */
        extern void OSReport(const char*, ...);
        if (pc_rng_trace_active()) {
            OSReport("[rngtrace] GSrand from %p\n", __builtin_return_address(0));
        }
    }
#endif
    return HSD_Rand();
}

void fn_800E0C78(void)
{
    u64 time = OSGetTime();

#ifdef TARGET_PC
    {
        /* OrreHook_FixedRngSeed (decision hook, default vanilla = the clock; docs/port/hook-points.md): the boot seed
         * for comparison runs against Dolphin, set outside the game code (ORRE_RNG_SEED, src/game/pc_debug_game.c) */
        extern int OrreHook_FixedRngSeedSet;
        extern u32 OrreHook_FixedRngSeed;
        extern void OSReport(const char*, ...);
        if (OrreHook_FixedRngSeedSet) {
            *lbl_80478C94 = OrreHook_FixedRngSeed;
            OSReport("[rng] boot seed 0x%08X (fixed, OrreHook_FixedRngSeed)\n", OrreHook_FixedRngSeed);
            return;
        }
        OSReport("[rng] boot seed 0x%08X (clock)\n", (u32)time);
    }
#endif
    *lbl_80478C94 = (u32)time;
}
