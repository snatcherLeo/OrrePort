/* save_cipher_test: proves pc_save_cipher.c's encrypt is the exact inverse of its decrypt on real saves.
 *
 * Why (2026-11-19, Phase 3 step 3.5): "Make a GameCube copy..." for another region will change bytes inside a
 * slot and re-encrypt it. Before any copy is trusted, the re-encrypt must give back, byte for byte, the slot the game
 * itself wrote: for every valid slot of every save given, decrypt -> encrypt must equal the stored slot (digest, key
 * stream and sum word all included). Saves come from the player's own files (never in the repo): run it with the
 * paths of .gci files, e.g. snatcherLeo's four region saves and a few OrrePort saves.
 *
 *   build-pc/save_cipher_test.exe <a.gci> [b.gci ...]
 *
 * Prints one line per slot and "ALL PASSED" or "FAILED". Plain C, every OS. */
#include "../src/pc_save_cipher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GCI_HEADER = 0x40, SAVE_SLOT0 = 0x6000, SLOTS = 3 };

int main(int argc, char** argv) {
    int checked = 0, failed = 0;
    static uint8_t plain[PC_SAVE_SLOT_SIZE], again[PC_SAVE_SLOT_SIZE];
    if (argc < 2) {
        fprintf(stderr, "usage: save_cipher_test <save.gci> [...]\n");
        return 2;
    }
    for (int a = 1; a < argc; a++) {
        FILE* f = fopen(argv[a], "rb");
        long len;
        uint8_t* data;
        if (f == NULL) { printf("%s: can't open\n", argv[a]); failed++; continue; }
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = (uint8_t*)malloc((size_t)len);
        if (data == NULL || fread(data, 1, (size_t)len, f) != (size_t)len) {
            printf("%s: can't read\n", argv[a]);
            failed++;
            fclose(f);
            free(data);
            continue;
        }
        fclose(f);
        if (len < GCI_HEADER + SAVE_SLOT0 + SLOTS * (long)PC_SAVE_SLOT_SIZE) {
            printf("%s: too short for a Colosseum save (%ld bytes)\n", argv[a], len);
            failed++;
            free(data);
            continue;
        }
        for (int n = 0; n < SLOTS; n++) {
            const uint8_t* enc = data + GCI_HEADER + SAVE_SLOT0 + (size_t)n * PC_SAVE_SLOT_SIZE;
            if (!pc_save_slot_decrypt(enc, plain)) {
                printf("%s slot %d: not a valid slot (skipped)\n", argv[a], n);
                continue;
            }
            pc_save_slot_encrypt(plain, again);
            checked++;
            if (memcmp(again, enc, PC_SAVE_SLOT_SIZE) == 0) {
                printf("%s slot %d: PASS (stamp %02X %02X %02X %02X)\n", argv[a], n, plain[8], plain[9], plain[10],
                       plain[11]);
            } else {
                size_t first = 0;
                while (first < PC_SAVE_SLOT_SIZE && again[first] == enc[first]) first++;
                printf("%s slot %d: FAIL, first differing byte at +0x%zX\n", argv[a], n, first);
                failed++;
            }
        }
        free(data);
    }
    printf("%d slot(s) checked: %s\n", checked, failed == 0 && checked > 0 ? "ALL PASSED" : "FAILED");
    return failed == 0 && checked > 0 ? 0 : 1;
}
