/*
 * pc_update_test: the shared updater (pc_update.c) on its own, against a local release folder (2026-11-23; the
 * dummy-update test of docs/port/updater-plan.md, before the screens exist).
 *
 * Why: the check, the signature, the download, the hash and the unpacking can be proven without a network, a public
 * repo or either program's screens: a test key signs a test release in a scratch folder, and this finds, downloads and
 * unpacks it into a scratch OrrePort folder. The launcher's swap is tested after it (orre_launcher.c).
 *
 * Usage (both test switches set, never a player's setup):
 *   ORRE_UPDATE_URL=<release folder>/ ORRE_UPDATE_TEST_PUBKEY=<64 hex> pc_update_test <OrrePort folder> <version> [auto]
 * Prints each state change and exits 0 when the update ends READY, 1 otherwise. "auto": Download updates automatically.
 * Platforms: Windows (the module's only built path so far).
 */
#include "pc_update.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const char* name(PcUpdateState s) {
    static const char* const n[] = {"IDLE", "OFF", "CHECKING", "UP_TO_DATE", "AVAILABLE", "DOWNLOADING", "READY", "FAILED"};
    return (unsigned)s < 8 ? n[s] : "?";
}

int main(int argc, char** argv) {
    PcUpdateStatus st;
    int asked = 0;
    if (argc < 3) {
        fprintf(stderr, "usage: pc_update_test <OrrePort folder> <version> [auto]\n");
        return 2;
    }
    if (!pc_update_init(argv[1], argv[2], "en")) { printf("init failed\n"); return 1; }
    pc_update_set_auto_download(argc > 3 && strcmp(argv[3], "auto") == 0);
    pc_update_check(1);
    for (int i = 0; i < 600; i++) { /* at most a minute */
#ifdef _WIN32
        Sleep(100);
#endif
        if (pc_update_poll(&st)) {
            printf("state %s: current %s, latest %s, %llu of %llu bytes%s%s\n", name(st.state), st.current, st.latest,
                   (unsigned long long)st.done, (unsigned long long)st.size, st.error_key[0] ? ", error " : "",
                   st.error_key);
            if (st.notes[0]) printf("  notes: %s\n", st.notes);
            /* in brackets, so a stray line break (the 2026-11-23 bug: version.txt's own) shows */
            if (st.previous[0]) printf("  previous (Go back): [%s]\n", st.previous);
        }
        if (st.state == PC_UPDATE_AVAILABLE && !asked && !(argc > 3 && strcmp(argv[3], "auto") == 0)) {
            asked = 1;
            printf("pressing Download\n");
            pc_update_download();
        }
        if (st.state == PC_UPDATE_READY || st.state == PC_UPDATE_FAILED || st.state == PC_UPDATE_UP_TO_DATE) break;
    }
    pc_update_shutdown();
    printf("%s\n", st.state == PC_UPDATE_READY ? "READY" : "not ready");
    return st.state == PC_UPDATE_READY ? 0 : 1;
}
