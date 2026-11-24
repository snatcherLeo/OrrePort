/*
 * pc_audio_test: the game's sound start-up order on the PC (port/src/pc_audio.c), headless: pc_audio_init at port
 * startup (lock + paused device), then the game's own sndInit (same arguments as gs_range_801653CC.c), then frames.
 * Checks that the device starts only after sndInit, at a retrace, and that audio is delivered to the device while
 * the game waits for retraces. Exit code 0 = passed; 77 = no audio device on this machine (skipped).
 */
#include <musyx/musyx.h>
#include <musyx/pc.h>

#include "pc_crash.h"
#include "pc_lowmem.h"

#include <stdio.h>

int pc_audio_init(void);
int pc_audio_started(void);
void pc_audio_stop(void);
void VIWaitForRetrace(void);

static void* low_alloc(size_t n) { return pc_lowmem_alloc(n); }
static void low_free(void* p) { pc_lowmem_free(p); }

static int check(const char* what, int ok) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

int main(void) {
    static SND_HOOKS hooks = {low_alloc, low_free};
    SND_PC_RENDER_INFO before, after;
    int ok = 1, i, started_before_init;

    pc_crash_install();
    if (!pc_lowmem_init()) {
        return 1;
    }
    if (!pc_audio_init()) {
        printf("no audio device: skipped\n");
        return 77;
    }
    for (i = 0; i < 3; i++) {
        VIWaitForRetrace();
    }
    started_before_init = pc_audio_started();
    ok &= check("the device stays paused until the game's sndInit", !started_before_init);

    sndSetHooks(&hooks);
    ok &= check("the game's sndInit (64 voices, 48 music, 16 effects, 1 studio)",
                sndInit(0x40, 0x30, 0x10, 1, 0, 0x9FC000) == 0);
    VIWaitForRetrace();
    ok &= check("the device starts at the next retrace after sndInit", pc_audio_started());

    before = sndPCGetRenderInfo();
    for (i = 0; i < 30; i++) { /* half a second of frames */
        VIWaitForRetrace();
    }
    after = sndPCGetRenderInfo();
    printf("  device output frames: %llu -> %llu in 30 fields (%u Hz)\n", (unsigned long long)before.outputFrames,
           (unsigned long long)after.outputFrames, (unsigned)after.mixRate);
    ok &= check("audio flows to the device while the game waits for retraces (about 0.5 s worth)",
                after.outputFrames > before.outputFrames &&
                    after.outputFrames - before.outputFrames > (u64)after.mixRate / 4);

    pc_audio_stop();
    sndQuit();
    printf("result: %s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
