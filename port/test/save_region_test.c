/* save_region_test: a save's region read with OrrePort's own code, and the import's change proven harmless (Phase 3
 * step 3.5, steps 6 and 7, 2026-11-19).
 *
 * Why: the hub and the game's SAVE FILES list colour every save by the region stamp in its newest valid slot
 * (pc_saves.c calls pc_save_region.c), and importing a European or Japanese save changes its .gci's game code. Before
 * either is trusted, real saves from each region must give the stamps they really hold, and the imported copy must
 * differ from the player's file in bytes 0-3 only, still decrypting to the same save. The saves are the player's own
 * (never in the repo): run it with snatcherLeo's four region saves and the stamps expected of each:
 *
 *   build-pc/save_region_test.exe "US English.gci" 2 2 PAL_english.gci 3 2 PAL_spanish.gci 3 6 "JPN save.gci" 1 1
 *
 * For each save it prints the game code, the newest slot, its stamp (area / language), then the import check: the
 * copy made the importer's way (pc_save_gci_make_us), which bytes differ, and the copy's own stamp and slot digest read
 * again. Ends with "ALL PASSED" or "FAILED". Plain C, no SDL, every OS. */
#include "../src/pc_save_cipher.h"
#include "../src/pc_save_region.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t* load(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    long n;
    uint8_t* data;
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = n > 0 ? (uint8_t*)malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *len = data != NULL ? (size_t)n : 0;
    return data;
}

int main(int argc, char** argv) {
    static uint8_t plain[PC_SAVE_SLOT_SIZE], again[PC_SAVE_SLOT_SIZE];
    int failed = 0, checked = 0;
    if (argc < 4 || (argc - 1) % 3 != 0) {
        fprintf(stderr, "usage: save_region_test <save.gci> <area> <language> [...]\n");
        return 2;
    }
    for (int a = 1; a + 2 < argc; a += 3) {
        const char* path = argv[a];
        const int want_area = atoi(argv[a + 1]), want_lang = atoi(argv[a + 2]);
        size_t len = 0;
        uint8_t* gci = load(path, &len);
        int area = 0, lang = 0, area2 = 0, lang2 = 0;
        checked++;
        if (gci == NULL) { printf("%s: can't read\n", path); failed++; continue; }
        const char letter = pc_save_gci_region_letter(gci, len);
        const int slot = pc_save_gci_newest_slot(gci, len, plain);
        if (letter == 0 || slot < 0) {
            printf("%s: not a readable Colosseum save (game code %.4s, newest slot %d)\n", path, (const char*)gci, slot);
            failed++;
            free(gci);
            continue;
        }
        pc_save_stamp(plain, &area, &lang);
        const int stamp_ok = area == want_area && lang == want_lang;
        printf("%s: game code %.4s, newest slot %d (counter %u), stamp area %d language %d, expected %d / %d: %s\n", path,
               (const char*)gci, slot, (unsigned)plain[4] << 24 | (unsigned)plain[5] << 16 | (unsigned)plain[6] << 8 | plain[7],
               area, lang, want_area, want_lang, stamp_ok ? "PASS" : "FAIL");
        if (!stamp_ok) failed++;

        /* the import's copy, made the importer's way (pc_saves.c pc_saves_import: a copy, then pc_save_gci_make_us) */
        uint8_t* copy = (uint8_t*)malloc(len);
        if (copy == NULL) { printf("  out of memory\n"); failed++; free(gci); continue; }
        memcpy(copy, gci, len);
        const int changed = pc_save_gci_make_us(copy, len);
        size_t first = len, last = 0, count = 0;
        for (size_t i = 0; i < len; i++)
            if (copy[i] != gci[i]) {
                if (first == len) first = i;
                last = i;
                count++;
            }
        /* a European / Japanese save: only bytes 0-3 may differ, and they must read GC6E; a US one: no change at all */
        const int bytes_ok = letter == 'E' ? (changed == 0 && count == 0)
                                           : (changed == 1 && count > 0 && first <= 3 && last <= 3 && memcmp(copy, "GC6E", 4) == 0);
        /* the copy still reads as the same save: the same newest slot, the same stamp, the same slot digest */
        const int slot2 = pc_save_gci_newest_slot(copy, len, again);
        pc_save_stamp(again, &area2, &lang2);
        const int same_ok = slot2 == slot && area2 == area && lang2 == lang &&
                            memcmp(again + PC_SAVE_DIGEST_AT, plain + PC_SAVE_DIGEST_AT, 20) == 0;
        if (count == 0)
            printf("  import copy: unchanged (a US save keeps its file as it is): %s\n", bytes_ok ? "PASS" : "FAIL");
        else
            printf("  import copy: %zu byte(s) differ, at +%zu..+%zu, game code now %.4s: %s\n", count, first, last,
                   (const char*)copy, bytes_ok ? "PASS" : "FAIL");
        printf("  import copy read again: newest slot %d, stamp %d / %d, same digest: %s\n", slot2, area2, lang2,
               same_ok ? "PASS" : "FAIL");
        if (!bytes_ok) failed++;
        if (!same_ok) failed++;
        free(copy);
        free(gci);
    }
    printf("%d save(s) checked: %s\n", checked, failed == 0 && checked > 0 ? "ALL PASSED" : "FAILED");
    return failed == 0 && checked > 0 ? 0 : 1;
}
