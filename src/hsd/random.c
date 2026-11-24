/**
 * @file random.c
 * @brief HAL sysdolphin random.c: the library's linear congruential
 *        generator, 0x801ADC08-0x801ADD0C.
 *
 * Adapted from the Melee decompilation (doldecomp/melee,
 * src/sysdolphin/baselib/random.c). Colosseum's newer sysdolphin forgets the
 * seed pointer through the memory module's released-memory check instead of
 * a [low, high) range. The library is built with deferred inlining, so the
 * functions are listed in HAL's order and MWCC emits them in reverse (the
 * retail address order); HSD_Randi inlines HSD_Rand.
 *
 * Symbols keep their dtk names (fn_/lbl_) because game code already links
 * against them: HSD_Rand is HSD_Rand, HSD_Randf fn_801ADC7C, HSD_Randi
 * HSD_Randi, the seed lbl_80478C90 and HSD_RandSeedPtr lbl_80478C94 (the
 * GS math code repoints it).
 */
#include "dolphin/types.h"

/* HSD_IsMemoryReleased: nonzero when the pointer lies in freed memory. */
int _HSD_MemCheckOwn(void* ptr);

/* seed */
u32 lbl_80478C90 = 1;
/* HSD_RandSeedPtr */
u32* lbl_80478C94 = &lbl_80478C90;

#ifdef TARGET_PC
/* ORRE_RNG_TRACE=1, or =FROM-TO (game frames, 2026-10-19): tracing every draw from power-on slows a replay below
 * real time, and the replay's disc and frame timing follow the wall clock, so a sitting-2 replay traced in full fell
 * out of step before Phenac's first conversation. A range traces only the stretch being compared (a battle). Shared
 * with the wrappers' caller lines (gs_math_random_exact_800E0BA0.c / _800E0C54.c). */
int pc_rng_trace_active(void)
{
    static int mode = -1;
    static unsigned from, to;
    extern char* getenv(const char*);
    extern int sscanf(const char*, const char*, ...);
    extern volatile u32 pc_vi_heartbeat __attribute__((weak));
    if (mode < 0) {
        const char* env = getenv("ORRE_RNG_TRACE");
        mode = env == 0 ? 0 : (sscanf(env, "%u-%u", &from, &to) == 2 ? 2 : 1);
    }
    if (mode == 2) {
        return &pc_vi_heartbeat != 0 && pc_vi_heartbeat >= from && pc_vi_heartbeat <= to;
    }
    return mode;
}

/* PC test switch ORRE_RNG_TRACE (off by default; golden-slice M19): one line per draw with its caller and the new
 * state, for diffing two runs to find where the PC's RNG sequence stops repeating. Observes only. */
static void pc_rand_trace(void* caller)
{
    static unsigned n;
    static unsigned cap; /* ORRE_RNG_TRACE_MAX=N (2026-10-25): more than 200,000 draws (a battle with particles
                          * makes ~500 a frame) */
    extern void OSReport(const char*, ...);
    if (cap == 0) {
        extern char* getenv(const char*);
        extern int sscanf(const char*, const char*, ...);
        const char* env = getenv("ORRE_RNG_TRACE_MAX");
        if (env == 0 || sscanf(env, "%u", &cap) != 1 || cap == 0) {
            cap = 200000;
        }
    }
    if (pc_rng_trace_active() && n < cap) {
        /* the game frame (port/src/pc_vi.c's VIWaitForRetrace count; weak, so tools without the port still link): a
         * "[rngtrace] frame N" line whenever it changes, so traces line up per frame with Dolphin's per-frame RNG log */
        extern volatile u32 pc_vi_heartbeat __attribute__((weak));
        extern u32 VIGetRetraceCount(void) __attribute__((weak));
        static u32 lastFrame = 0xFFFFFFFFu;
        if (&pc_vi_heartbeat != 0 && pc_vi_heartbeat != lastFrame) {
            lastFrame = pc_vi_heartbeat;
            /* and the retrace count (2026-10-21): Dolphin's probes and the disc clock count retraces, the game
             * frame count falls behind them by every retrace the game spent waiting (23 by P01's first battle) */
            OSReport("[rngtrace] frame %u retrace %u\n", (unsigned)lastFrame,
                     VIGetRetraceCount != 0 ? (unsigned)VIGetRetraceCount() : 0u);
        }
        n++;
        {
            /* and the game task being dispatched (2026-10-26; index:priority, 0:0 outside a dispatch) */
            extern u32 pc_gapp_current_task(void) __attribute__((weak));
            u32 t = pc_gapp_current_task != 0 ? pc_gapp_current_task() : 0;
            OSReport("[rngtrace] %u %p %08X task %u:%u\n", n, caller, *lbl_80478C94, (unsigned)(t >> 8),
                     (unsigned)(t & 0xFF));
        }
    }
}
#endif

#ifdef TARGET_PC
/* OrreHook_RngFrameState (decision hook, docs/port/hook-points.md; defined in port/src/pc_frame_times.c): replay mode's
 * per-frame resync (ORRE_REPLAY_RNG) posts the console's state for the frame now starting; the generator takes it
 * before its next step. Default vanilla: nothing is ever posted */
static void pc_rng_take_posted(void)
{
    extern u32 OrreHook_RngFrameState;
    extern int OrreHook_RngFrameStatePending;
    if (OrreHook_RngFrameStatePending) {
        OrreHook_RngFrameStatePending = 0;
        *lbl_80478C94 = OrreHook_RngFrameState;
    }
    {
        /* OrreHook_RngBeforeDraw (decision hook, docs/port/hook-points.md; defined in src/game/pc_debug_game.c, weak
         * so programs without the game still link): the replay's per-draw battle-roll resync (ORRE_ROLL_SEEDS) may replace
         * the state before this step. Default vanilla: unset */
        extern void (*OrreHook_RngBeforeDraw)(u32* state) __attribute__((weak));
        if (&OrreHook_RngBeforeDraw != 0 && OrreHook_RngBeforeDraw != 0) {
            OrreHook_RngBeforeDraw(lbl_80478C94);
        }
    }
}
#endif

/* HSD_Rand */
s32 HSD_Rand(void)
{
#ifdef TARGET_PC
    pc_rng_take_posted();
#endif
    *lbl_80478C94 = *lbl_80478C94 * 214013 + 2531011;
#ifdef TARGET_PC
    pc_rand_trace(__builtin_return_address(0));
#endif
    return *lbl_80478C94 >> 0x10;
}

/* HSD_Randf */
f32 HSD_Randf(void)
{
#ifdef TARGET_PC
    pc_rng_take_posted();
#endif
    *lbl_80478C94 = *lbl_80478C94 * 214013 + 2531011;
#ifdef TARGET_PC
    pc_rand_trace(__builtin_return_address(0));
#endif
    return (f32) (*lbl_80478C94 >> 0x10) / (1 << 16);
}

/* HSD_Randi */
s32 HSD_Randi(s32 max_val)
{
    return max_val * HSD_Rand() / (1 << 16);
}

void _HSD_RandForgetMemory(void)
{
    if (_HSD_MemCheckOwn(lbl_80478C94)) {
        lbl_80478C94 = &lbl_80478C90;
    }
}
