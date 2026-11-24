/*
 * pc_gba_stub.c: the SDK's GBA link driver on the PC: "no Game Boy Advance connected" (decision 8: the GBA link is
 * skipped for the prototype; docs/port/sdk-gaps.md).
 *
 * The GameCube driver (src/game/gba/GBA.c, GBARead.c, GBAWrite.c, GBAXfer.c) talks to the serial-interface hardware,
 * so it is hardware code and the PC build replaces it with this file (snatcherLeo's rule: hardware -> the PC version). The
 * game's own GBA features (gbaCommunication*.c, the e-Reader / Pokemon Box transfers) stay ours and see every port
 * report "not ready", as on a console with nothing plugged in.
 *
 * Asynchronous calls report "not ready" without calling the callback: the SDK only calls it for a transfer that
 * started (likely; the game checks the return value first in the callers read so far).
 */
#include <dolphin/types.h>
#include <stdio.h>

/* From Aurora's dolphin/gba.h, which cannot be included as is (it includes "types.h" without its directory) */
#define GBA_NOT_READY 1
typedef void (*GBACallback)(s32 chan, s32 ret);

/* *Added 2026-11-11 (snatcherLeo's GBA link look):* a log of which game code asks the GBA driver what. Each new
 * (call, port, caller) is printed once ("[gba] GBAGetStatus port 1 from 0x... (first)"), then every 600th repeat of
 * it with its count, so a per-frame poll shows up without flooding the log. The caller is the return address in the
 * exe: `addr2line -f -e orrecolo.exe <address>` names the game function. Log only: the answers stay "not ready" on
 * every port. No frame number: this file is also linked into the SDK test programs, which have no game frame counter
 * (pc_vi_heartbeat); the log's neighbouring [flag] / [text] lines carry frames. */
#define GBA_LOG_KEYS 64
static struct { const char* name; s32 chan; void* caller; unsigned count; } s_gba_seen[GBA_LOG_KEYS];
static int s_gba_seen_n;

static void gba_log(const char* name, s32 chan, void* caller) {
    int i;
    for (i = 0; i < s_gba_seen_n; i++) {
        if (s_gba_seen[i].name == name && s_gba_seen[i].chan == chan && s_gba_seen[i].caller == caller) {
            if (++s_gba_seen[i].count % 600 == 0) {
                printf("[gba] %s port %d from %p: %u calls so far\n", name, (int)chan, caller,
                       s_gba_seen[i].count);
            }
            return;
        }
    }
    if (s_gba_seen_n < GBA_LOG_KEYS) {
        s_gba_seen[s_gba_seen_n].name = name;
        s_gba_seen[s_gba_seen_n].chan = chan;
        s_gba_seen[s_gba_seen_n].caller = caller;
        s_gba_seen[s_gba_seen_n].count = 1;
        s_gba_seen_n++;
    }
    printf("[gba] %s port %d from %p (first)\n", name, (int)chan, caller);
    fflush(stdout);
}
#define GBA_LOG(name, chan) gba_log(name, chan, __builtin_return_address(0))

void GBAInit(void) { GBA_LOG("GBAInit", -1); }

static s32 gba_not_ready(u8* status) {
    if (status != NULL) {
        *status = 0;
    }
    return GBA_NOT_READY;
}

s32 GBAGetStatus(s32 chan, u8* status) {
    GBA_LOG("GBAGetStatus", chan);
    return gba_not_ready(status);
}

s32 GBAGetStatusAsync(s32 chan, u8* status, GBACallback callback) {
    (void)callback;
    GBA_LOG("GBAGetStatusAsync", chan);
    return gba_not_ready(status);
}

s32 GBAReset(s32 chan, u8* status) {
    GBA_LOG("GBAReset", chan);
    return gba_not_ready(status);
}

s32 GBAResetAsync(s32 chan, u8* status, GBACallback callback) {
    (void)callback;
    GBA_LOG("GBAResetAsync", chan);
    return gba_not_ready(status);
}

s32 GBAGetProcessStatus(s32 chan, u8* percentp) {
    GBA_LOG("GBAGetProcessStatus", chan);
    if (percentp != NULL) {
        *percentp = 0;
    }
    return GBA_NOT_READY;
}

s32 GBARead(s32 chan, u8* dst, u8* status) {
    (void)dst;
    GBA_LOG("GBARead", chan);
    return gba_not_ready(status);
}

s32 GBAReadAsync(s32 chan, u8* dst, u8* status, GBACallback callback) {
    (void)dst;
    (void)callback;
    GBA_LOG("GBAReadAsync", chan);
    return gba_not_ready(status);
}

s32 GBAWrite(s32 chan, u8* src, u8* status) {
    (void)src;
    GBA_LOG("GBAWrite", chan);
    return gba_not_ready(status);
}

s32 GBAWriteAsync(s32 chan, u8* src, u8* status, GBACallback callback) {
    (void)src;
    (void)callback;
    GBA_LOG("GBAWriteAsync", chan);
    return gba_not_ready(status);
}

s32 GBAJoyBoot(s32 chan, s32 palette_color, s32 palette_speed, u8* programp, s32 length, u8* status) {
    (void)palette_color;
    (void)palette_speed;
    (void)programp;
    (void)length;
    GBA_LOG("GBAJoyBoot", chan);
    return gba_not_ready(status);
}

s32 GBAJoyBootAsync(s32 chan, s32 palette_color, s32 palette_speed, u8* programp, s32 length, u8* status,
                    GBACallback callback) {
    (void)palette_color;
    (void)palette_speed;
    (void)programp;
    (void)length;
    (void)callback;
    GBA_LOG("GBAJoyBootAsync", chan);
    return gba_not_ready(status);
}
