/* pc_save_region.c: a save's region from its own bytes, and the import's game-code change (pc_save_region.h says why
 * and what each byte means). Plain C, every OS. */
#include "pc_save_region.h"
#include "pc_save_cipher.h"

#include <string.h>

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

int pc_save_gci_newest_slot(const uint8_t* gci, size_t len, uint8_t* plain) {
    int order[3] = {0, 1, 2};
    uint32_t counter[3];
    if (gci == NULL || len < PC_GCI_HEADER + PC_GCI_CARD_HEADER + 3 * (size_t)PC_SAVE_SLOT_SIZE) return -1;
    for (int n = 0; n < 3; n++)
        counter[n] = be32(gci + PC_GCI_HEADER + PC_GCI_CARD_HEADER + (size_t)n * PC_SAVE_SLOT_SIZE + 4);
    /* highest counter first; equal counters keep their slot order (the first wins, as the game's own pick and
     * pc_saves.c's earlier loop did) */
    for (int i = 1; i < 3; i++)
        for (int j = i; j > 0 && counter[order[j]] > counter[order[j - 1]]; j--) {
            const int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    for (int i = 0; i < 3; i++) {
        const uint8_t* enc = gci + PC_GCI_HEADER + PC_GCI_CARD_HEADER + (size_t)order[i] * PC_SAVE_SLOT_SIZE;
        if (pc_save_slot_decrypt(enc, plain)) return order[i]; /* the digest checks out: this is the save */
    }
    return -1;
}

void pc_save_stamp(const uint8_t* plain, int* area, int* language) {
    const int a = plain[PC_SAVE_STAMP_AREA];
    const int known = a == PC_SAVE_AREA_JAPAN || a == PC_SAVE_AREA_US || a == PC_SAVE_AREA_PAL;
    if (area != NULL) *area = known ? a : PC_SAVE_AREA_UNKNOWN;
    if (language != NULL) *language = known ? plain[PC_SAVE_STAMP_LANGUAGE] : 0;
}

char pc_save_gci_region_letter(const uint8_t* gci, size_t len) {
    if (gci == NULL || len < PC_GCI_HEADER || memcmp(gci, "GC6", 3) != 0 || memcmp(gci + 4, "01", 2) != 0) return 0;
    return gci[3] == 'E' || gci[3] == 'P' || gci[3] == 'J' ? (char)gci[3] : 0;
}

int pc_save_gci_make_us(uint8_t* gci, size_t len) {
    if (gci == NULL || len < 4 || memcmp(gci, "GC6E", 4) == 0) return 0;
    memcpy(gci, "GC6E", 4);
    return 1;
}
