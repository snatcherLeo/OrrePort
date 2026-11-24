/*
 * pc_card_test: a save round trip through Aurora's CARD library under MinGW (docs/port/saves.md), in a scratch
 * folder: init the way the port does it (game "GC6E", maker "01", Dolphin-style GCI folder), mount slot A, create a
 * file, write a block, read it back, check the bytes, check that the game's own later CARDInit() call changes
 * nothing, delete the file. Exit code 0 = all passed.
 *
 *   pc_card_test <scratch folder>
 */
#include <dolphin/card.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

static int check(const char* what, int ok) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

static u8 s_work[CARD_WORKAREA_SIZE] __attribute__((aligned(32)));
static u8 s_out[8192] __attribute__((aligned(32)));
static u8 s_in[8192] __attribute__((aligned(32)));

int main(int argc, char** argv) {
    CARDFileInfo file;
    u32 sector = 0;
    s32 r;
    int ok = 1, i;

    if (argc < 2 || chdir(argv[1]) != 0) {
        fprintf(stderr, "usage: pc_card_test <existing scratch folder>\n");
        return 2;
    }
    for (i = 0; i < (int)sizeof(s_out); i++) {
        s_out[i] = (u8)(i * 7 + 3);
    }

    CARDSetLoadType(CARD_GCIFOLDER);
    CARDInit("GC6E", "01"); /* the port's startup does this before the game runs */
    r = CARDMount(0, s_work, NULL);
    ok &= check("mount slot A (a GCI folder under the scratch folder)", r == CARD_RESULT_READY || r == CARD_RESULT_BROKEN);
    r = CARDGetSectorSize(0, &sector);
    ok &= check("sector size is the GameCube card's 8 KB", r == CARD_RESULT_READY && sector == 8192);

    CARDDelete(0, "orreport_test"); /* left over from an earlier run */
    r = CARDCreate(0, "orreport_test", sector, &file);
    ok &= check("create a one-block file", r == CARD_RESULT_READY);
    r = CARDWrite(&file, s_out, (s32)sizeof(s_out), 0);
    ok &= check("write 8 KB", r == CARD_RESULT_READY);
    CARDClose(&file);

    /* the game's own CARDInit() reaches Aurora's CARDInit(game, maker) with whatever is in the argument registers;
     * once initialised it returns before reading them (here: obviously wrong arguments) */
    CARDInit("XXXX", "ZZ");

    r = CARDOpen(0, "orreport_test", &file);
    ok &= check("open it again", r == CARD_RESULT_READY);
    memset(s_in, 0, sizeof(s_in));
    r = CARDRead(&file, s_in, (s32)sizeof(s_in), 0);
    ok &= check("read 8 KB back, same bytes", r == CARD_RESULT_READY && memcmp(s_in, s_out, sizeof(s_in)) == 0);
    CARDClose(&file);
    r = CARDDelete(0, "orreport_test");
    ok &= check("delete it", r == CARD_RESULT_READY);
    r = CARDOpen(0, "orreport_test", &file);
    ok &= check("... and it is gone", r == CARD_RESULT_NOFILE);
    CARDUnmount(0);

    printf("result: %s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
