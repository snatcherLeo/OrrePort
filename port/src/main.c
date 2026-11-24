/*
 * OrrePort: open a window through Aurora and clear it. No game code yet.
 *
 * Startup order matters (platform-layer plan, decision 5; docs/port/low-memory.md): GameCube main memory is
 * reserved at 0x80000000, the locked cache at 0xE0000000 and the port's low-memory region at 0x40000000 before
 * anything else; Aurora is told to use that MEM1 (AuroraConfig.mem1Base) so the game's heaps land there; then
 * Aurora starts SDL, Dawn and the GPU driver, and the memory checks run with all of them loaded.
 *
 *   orreport.exe [--disc <image.iso|.rvz|.ciso>] [--check] [--frames N]
 *     --disc    open the disc image through Aurora's DVD layer (nod) and read the banner, to prove disc access
 *     --check   run the startup checks and exit without opening the main loop (exit code 0 = all passed)
 *     --frames  run N frames, then report the frame rate, the retrace callbacks and a periodic alarm, and exit
 *               (exit code 0 = about 59.94 fields per second, one frame per field (1 % slack for desktop
 *               hitches), every callback and alarm on time)
 *
 * The loop is shaped like the game's: draw, hand the frame buffer to VI, wait for the retrace (port/src/pc_vi.c,
 * where Aurora's frame is shown and the next begun).
 */
#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include <dolphin/dvd.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>

#include "pc_fibre.h"
#include "pc_lowmem.h"
#include "pc_mem.h"
#include "pc_os.h"

#include <SDL3/SDL_thread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* snatcherLeo's 32-bit pointer decision (option (a)): everything the game can see must sit below 4 GB */
static void fibre_probe(void* arg) {
    volatile int local = 0x47433645;
    *(uintptr_t*)arg = (uintptr_t)&local;
    for (;;) {
        pc_fibre_switch(pc_fibre_init_main());
    }
}

static int low_memory_check(void) {
    uintptr_t image = pc_os_image_base(), local_addr = 0;
    void* samples[3];
    PcFibre* f;
    int ok = 1, i;
    u8* lo = (u8*)OSGetArenaLo();
    u8* hi = (u8*)OSGetArenaHi();

    /* the game's arena (its heaps are carved from it) is our MEM1 */
    if ((uintptr_t)lo < PC_MEM1_BASE || (uintptr_t)hi > PC_MEM1_BASE + pc_mem_committed() || lo >= hi) {
        printf("[orreport] low memory: game arena %p-%p is not inside MEM1 at 0x%08X\n", (void*)lo, (void*)hi,
               PC_MEM1_BASE);
        ok = 0;
    }
    /* the program image (code, static data, string constants) */
    if (!pc_is_low((void*)image, 1)) {
        printf("[orreport] low memory: exe image at %p is above 4 GB\n", (void*)image);
        ok = 0;
    }
    /* a fibre stack, as used by every GS thread: the address of a local inside it */
    f = pc_fibre_create(256 * 1024, fibre_probe, &local_addr);
    if (f == NULL) {
        ok = 0;
    } else {
        pc_fibre_switch(f);
        if (local_addr == 0 || !pc_is_low((void*)local_addr, 4)) {
            printf("[orreport] low memory: fibre stack local at %p is above 4 GB\n", (void*)local_addr);
            ok = 0;
        }
        pc_fibre_destroy(f);
    }
    /* port allocations handed to the game */
    for (i = 0; i < 3; i++) {
        samples[i] = pc_lowmem_alloc((size_t)1 << (10 + 6 * i)); /* 1 KB, 64 KB, 4 MB */
        if (samples[i] == NULL || !pc_is_low(samples[i], (size_t)1 << (10 + 6 * i))) {
            printf("[orreport] low memory: port allocation %d at %p is not below 4 GB\n", i, samples[i]);
            ok = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        pc_lowmem_free(samples[i]);
    }
    printf("[orreport] low memory: game arena %p-%p, exe image %p, fibre stack local %p, port allocations "
           "%p / %p / %p: %s\n", (void*)lo, (void*)hi, (void*)image, (void*)local_addr, samples[0], samples[1],
           samples[2], ok ? "OK" : "FAILED");
    return ok;
}


/* The arena is the GameCube's (port-gaps 22, option 1): OSInit leaves exactly the console's bounds (pc_mem.h), also
 * after the game's own (second) OSInit, and main.dol's range below the arena stays untouched (all zero here, where
 * nothing has loaded yet; in the game, the project's copy of main.dol's data lives there). */
static int arena_check(void) {
    const u8* below;
    u32 i, nonzero = 0;
    uintptr_t lo, hi;
    int ok = 1;

    OSInit(); /* the game calls it again; the arena must not move */
    lo = (uintptr_t)OSGetArenaLo();
    hi = (uintptr_t)OSGetArenaHi();
    if (lo != (uintptr_t)pc_mem_from_gc(PC_ARENA_LO) || hi != (uintptr_t)pc_mem_from_gc(PC_ARENA_HI)) {
        printf("[orreport] arena: %p-%p after OSInit, expected 0x%08X-0x%08X (the console's, pc_mem.h)\n",
               (void*)lo, (void*)hi, PC_ARENA_LO, PC_ARENA_HI);
        ok = 0;
    }
    below = (const u8*)pc_mem_from_gc(PC_DOL_DATA_START);
    for (i = 0; i < PC_ARENA_LO - PC_DOL_DATA_START; i++) {
        nonzero += below[i] != 0;
    }
    if (nonzero != 0) {
        printf("[orreport] arena: %u bytes written in main.dol's range 0x%08X-0x%08X\n", nonzero, PC_DOL_DATA_START,
               PC_ARENA_LO);
        ok = 0;
    }
    printf("[orreport] arena: 0x%08X-0x%08X (%u KB), main.dol's range 0x%08X-0x%08X below it untouched: %s\n",
           (u32)(lo - (uintptr_t)pc_mem_from_gc(PC_MEM1_BASE)) + PC_MEM1_BASE,
           (u32)(hi - (uintptr_t)pc_mem_from_gc(PC_MEM1_BASE)) + PC_MEM1_BASE, (u32)((hi - lo) >> 10),
           PC_DOL_DATA_START, PC_ARENA_LO, ok ? "OK" : "FAILED");
    return ok;
}

int pc_platform_start(int argc, char** argv, const char* disc);
void pc_platform_stop(void);
#ifdef PC_HAVE_AURORA_DVD
void VIWaitForRetrace(void);
static SDL_ThreadID s_dvd_main_thread;
static volatile int s_dvd_async_done, s_dvd_async_off_thread;
static volatile s32 s_dvd_async_result;
static void dvd_async_done(s32 result, DVDFileInfo* file) {
    (void)file;
    s_dvd_async_result = result;
    s_dvd_async_off_thread = SDL_GetCurrentThreadID() != s_dvd_main_thread;
    s_dvd_async_done = 1;
}
#endif

/* Open the disc image and read the start of opening.bnr into MEM1 (the game will read everything this way) */
static int disc_check(const char* path) {
    DVDFileInfo file;
    /* any 32-byte aligned spot inside the arena (below it is main.dol's data on the PC too); nothing else runs yet */
    u8* buf = (u8*)pc_mem_from_gc(PC_ARENA_LO + 0x10000 - (PC_ARENA_LO & 0x1F));
    s32 read;
    int ok;

    DVDInit(); /* the image was opened by pc_platform_start */
    if (!DVDOpen("/opening.bnr", &file)) {
        printf("[orreport] disc: opened, but /opening.bnr is missing\n");
        return 0;
    }
    read = DVDReadPrio(&file, buf, 32, 0, 2);
    ok = read == 32 && memcmp(buf, "BNR", 3) == 0;
#ifdef PC_HAVE_AURORA_DVD
    /* an asynchronous read, as gs_dvd.c does: the completion callback must run on the game thread */
    {
        static DVDFileInfo async_file; /* must outlive the read */
        int waited;
        s_dvd_main_thread = SDL_GetCurrentThreadID();
        if (DVDOpen("/opening.bnr", &async_file) && DVDReadAsyncPrio(&async_file, buf + 64, 32, 0, dvd_async_done, 2)) {
            for (waited = 0; waited < 120 && !s_dvd_async_done; waited++) {
                VIWaitForRetrace(); /* the game waits by running frames; the callback arrives at an interrupt point */
            }
            ok &= s_dvd_async_done && s_dvd_async_result == 32 && !s_dvd_async_off_thread &&
                  memcmp(buf + 64, "BNR", 3) == 0;
            printf("[orreport] disc: async read %s after %d fields, callback on the %s thread\n",
                   s_dvd_async_done ? "done" : "NOT done", waited, s_dvd_async_off_thread ? "WRONG" : "game");
        } else {
            ok = 0;
        }
    }
#endif
    printf("[orreport] disc: %s, %d FST entries, opening.bnr is %u bytes, magic \"%.4s\": %s\n", path,
           (int)aurora_dvd_base_entry_count(), (unsigned)file.length, (const char*)buf, ok ? "OK" : "FAILED");
    DVDClose(&file);
    return ok;
}

/* Frame boundary (pc_vi.c) */
typedef void (*RetraceCallback)(u32 retraceCount);
void pc_vi_begin(void);
int pc_vi_exit_requested(void);
void VIWaitForRetrace(void);
void VISetNextFrameBuffer(void* fb);
RetraceCallback VISetPostRetraceCallback(RetraceCallback cb);
RetraceCallback VISetPreRetraceCallback(RetraceCallback cb);

static u8 s_fake_xfb[16]; /* the game hands VI its external frame buffer; Aurora presents on its own */

static void draw_frame(void) {
    /* Colosseum-ish dusk orange, so it is clearly our window */
    GXSetCopyClear((GXColor){.r = 0xC8, .g = 0x6E, .b = 0x32, .a = 0xFF}, GX_MAX_Z24);
    VISetNextFrameBuffer(s_fake_xfb);
}

static u32 s_pre_count, s_post_count, s_last_retrace, s_retrace_gaps, s_alarm_count;
static void pre_retrace(u32 count) { ++s_pre_count; (void)count; }
static void post_retrace(u32 count) {
    if (s_last_retrace != 0 && count != s_last_retrace + 1) {
        ++s_retrace_gaps;
    }
    s_last_retrace = count;
    ++s_post_count;
}
static void alarm_tick(OSAlarm* alarm, OSContext* context) { ++s_alarm_count; (void)alarm; (void)context; }

/* GX draw done through HSD's setter name (pc_gx_hooks.c): must arrive on the game thread */
typedef void (*DrawDoneCallback)(void);
DrawDoneCallback fn_800B90A4(DrawDoneCallback cb);
static SDL_ThreadID s_game_thread;
static u32 s_draw_done_count, s_draw_done_off_thread;
static void draw_done(void) {
    ++s_draw_done_count;
    if (SDL_GetCurrentThreadID() != s_game_thread) {
        ++s_draw_done_off_thread;
    }
}

/* --frames N: N game-style frames; checks the field rate, the callbacks and a 100 ms periodic alarm */
static int frame_check(int frames) {
    OSAlarm alarm;
    OSTime start, end;
    double seconds, fps;
    u32 expected_alarms;
    int i, ok = 1;

    VISetPreRetraceCallback(pre_retrace);
    VISetPostRetraceCallback(post_retrace);
    s_game_thread = SDL_GetCurrentThreadID();
    fn_800B90A4(draw_done);
    OSCreateAlarm(&alarm);
    pc_vi_begin();
    VIWaitForRetrace(); /* the first frame is slow (swap chain setup); start measuring two fields in */
    VIWaitForRetrace();
    start = OSGetTime();
    OSSetPeriodicAlarm(&alarm, start + OSMillisecondsToTicks(100), OSMillisecondsToTicks(100), alarm_tick);
    s_pre_count = s_post_count = s_alarm_count = 0;
    s_draw_done_count = 0;
    for (i = 0; i < frames && !pc_vi_exit_requested(); i++) {
        draw_frame();
        GXSetDrawDone(); /* as GSgfx does before it waits for the draw-done flag */
        VIWaitForRetrace();
    }
    end = OSGetTime();
    fn_800B90A4(NULL);
    OSCancelAlarm(&alarm);
    seconds = (double)(end - start) / OS_TIMER_CLOCK;
    fps = s_post_count / seconds; /* fields counted by the retrace interrupt, as the game counts them */
    expected_alarms = (u32)(seconds * 10.0);
    printf("[orreport] %d frames, %u fields in %.3f s = %.2f fields/s (target 59.94); frames that took two fields: %u\n",
           i, s_post_count, seconds, fps, s_post_count - (u32)i);
    printf("[orreport] retrace callbacks: pre %u, post %u, gaps in the count %u\n", s_pre_count, s_post_count,
           s_retrace_gaps);
    printf("[orreport] GX draw done: %u callbacks for %d frames, %u off the game thread\n", s_draw_done_count, i,
           s_draw_done_off_thread);
    printf("[orreport] 100 ms periodic alarm: fired %u times (expected %u +- 1)\n", s_alarm_count, expected_alarms);
    /* a frame the desktop delays past a field boundary counts two fields, as on the GameCube; allow 1 % of those */
    ok &= fps > 59.3 && fps < 60.6;
    ok &= s_pre_count == s_post_count && s_post_count >= (u32)i && s_post_count - (u32)i <= (u32)i / 100 &&
          s_retrace_gaps == 0;
    ok &= s_alarm_count + 1 >= expected_alarms && s_alarm_count <= expected_alarms + 1;
    ok &= s_draw_done_count + 2 >= (u32)i && s_draw_done_count <= (u32)i && s_draw_done_off_thread == 0;
    printf("[orreport] frame check: %s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok;
}

int main(int argc, char* argv[]) {
    const char* disc = NULL;
    int check_only = 0, ok = 1, i, frames = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--disc") == 0 && i + 1 < argc) {
            disc = argv[++i];
        } else if (strcmp(argv[i], "--check") == 0) {
            check_only = 1;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        }
    }

    /* memory below 4 GB first, then Aurora, DVD callbacks, the disc, the memory card, OSInit, the first frame */
    if (!pc_platform_start(argc, argv, disc)) {
        return 1;
    }

    /* Repeat the memory checks now that SDL, Dawn and the GPU driver are loaded */
    ok &= pc_mem_check("orreport");
    ok &= low_memory_check();
    ok &= arena_check(); /* before the disc check writes into MEM1 */
    if (disc != NULL) {
        ok &= disc_check(disc);
    }
    if (check_only) {
        printf("[orreport] startup checks: %s\n", ok ? "ALL OK" : "SOME FAILED");
        pc_platform_stop();
        return ok ? 0 : 1;
    }

    if (frames > 0) {
        ok &= frame_check(frames);
        pc_platform_stop();
        return ok ? 0 : 1;
    }

    pc_vi_begin();
    while (!pc_vi_exit_requested()) {
        draw_frame();
        VIWaitForRetrace();
    }
    pc_platform_stop();
    return 0;
}
