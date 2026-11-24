/* pc_gcopy_region.c: the byte work of a GameCube copy for another region (pc_gcopy_region.h says why and what changes).
 * Plain C99 over memory, no platform calls: the same on Windows, Linux and macOS. */
#include "pc_gcopy_region.h"
#include "pc_save_cipher.h"
#include "pc_save_region.h"

#include <stdlib.h>
#include <string.h>

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* the zlib CRC-32 (Python's zlib.crc32), as pc_disc_check.c checks main.dol's data */
static uint32_t crc32_of(const uint8_t* p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* main.dol's bytes at GameCube address addr (n of them, all in one section), or NULL. The DOL header: file offsets of
 * the 18 sections at 0x00, their load addresses at 0x48, their sizes at 0x90 (big-endian). */
static const uint8_t* dol_at(const uint8_t* dol, size_t size, uint32_t addr, uint32_t n) {
    if (dol == NULL || size < 0x100) return NULL;
    for (int i = 0; i < 18; i++) {
        const uint32_t off = be32(dol + 4 * i), at = be32(dol + 0x48 + 4 * i), len = be32(dol + 0x90 + 4 * i);
        if (len != 0 && addr >= at && addr - at <= len && n <= len - (addr - at) && (size_t)off + len <= size)
            return dol + off + (addr - at);
    }
    return NULL;
}

/* A text of the card header (title or comment) at addr: the bytes up to its 0 end, at most 0x1F, zero-filled to 0x20.
 * Returns 1 when there was a readable text (possibly empty). A pointer that lands on a 0 right before a word is taken as
 * pointing at that word: the European game's German comment entry points one byte early, at the end of the Spanish
 * word before it (verified in its main.dol, regions.md; the project's call 2026-11-19: a German copy writes the intended word,
 * "Datei", since the comment is only shown in the memory card screen). */
static int dol_text(const uint8_t* dol, size_t size, uint32_t addr, uint8_t out[PC_CARD_TEXT_SIZE], int fix_early) {
    const uint8_t* p = dol_at(dol, size, addr, 2);
    memset(out, 0, PC_CARD_TEXT_SIZE);
    if (p == NULL) return 0;
    if (fix_early && p[0] == 0 && p[1] != 0) addr++;
    for (int i = 0; i < PC_CARD_TEXT_SIZE - 1; i++) {
        const uint8_t* c = dol_at(dol, size, addr + (uint32_t)i, 1);
        if (c == NULL) return 0;
        if (*c == 0) break;
        out[i] = *c;
    }
    return 1;
}

/* Where each region's executable keeps its card header's pieces (verified 2026-11-19 on the three discs' main.dol,
 * regions.md "The save file's header per region"): the banner and the icon right after it (the icon is the same in all
 * three, the US and European banners the same), the title, and the comment (US / Japan: one text; Europe: a table of
 * five {language number, text address} pairs, read by the Options language). The CRC-32s guard against another
 * revision or a modified disc: anything else and the caller falls back (pc_gcopy.c). */
typedef struct {
    char region;
    uint32_t banner, banner_crc, icon, icon_crc, title, comment, comment_table;
} DolCardPlaces;
static const DolCardPlaces kPlaces[] = {
    {'E', 0x802758E8u, 0x52F2E1A7u, 0x802770E8u, 0x6B3B8AB4u, 0x802792FCu, 0x80279310u, 0},
    {'P', 0x8027A898u, 0x52F2E1A7u, 0x8027C098u, 0x6B3B8AB4u, 0x8027E2D8u, 0, 0x803BAF30u},
    {'J', 0x80270E90u, 0x6886B065u, 0x80272690u, 0x6B3B8AB4u, 0x802748A4u, 0x802748B8u, 0},
};

int pc_card_parts_from_dol(char region, const uint8_t* dol, size_t size, PcCardParts* out) {
    const DolCardPlaces* pl = NULL;
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < sizeof(kPlaces) / sizeof(kPlaces[0]); i++)
        if (kPlaces[i].region == region) pl = &kPlaces[i];
    if (pl == NULL) return 0;
    const uint8_t* banner = dol_at(dol, size, pl->banner, PC_CARD_BANNER_SIZE);
    const uint8_t* icon = dol_at(dol, size, pl->icon, PC_CARD_ICON_SIZE);
    if (banner == NULL || icon == NULL || crc32_of(banner, PC_CARD_BANNER_SIZE) != pl->banner_crc ||
        crc32_of(icon, PC_CARD_ICON_SIZE) != pl->icon_crc)
        return 0;
    memcpy(out->banner, banner, PC_CARD_BANNER_SIZE);
    memcpy(out->icon, icon, PC_CARD_ICON_SIZE);
    if (!dol_text(dol, size, pl->title, out->title, 0) || out->title[0] == 0) return 0;
    if (pl->comment_table != 0) {
        const uint8_t* t = dol_at(dol, size, pl->comment_table, 5 * 8);
        if (t == NULL) return 0;
        for (int k = 0; k < 5; k++) {
            const uint32_t lang = be32(t + 8 * k), addr = be32(t + 8 * k + 4);
            if (lang < 2 || lang > 6) return 0;
            out->has_comment[lang] = dol_text(dol, size, addr, out->comment[lang], 1);
        }
        if (!out->has_comment[2]) return 0; /* English at least */
    } else {
        if (!dol_text(dol, size, pl->comment, out->comment[0], 0)) return 0;
        out->has_comment[0] = 1;
    }
    out->region = region;
    out->ok = 1;
    return 1;
}

uint32_t pc_card_header_sum(const uint8_t* header) {
    uint32_t sum = 0;
    for (size_t i = 0; i < PC_CARD_SUM_AT; i += 4) sum += be32(header + i);
    return sum;
}

void pc_card_header_seal(uint8_t* header) { put_be32(header + PC_CARD_SUM_AT, pc_card_header_sum(header)); }

int pc_card_header_build(const PcCardParts* parts, int language, uint8_t* header) {
    int ok = 1;
    memset(header, 0, PC_CARD_HEADER_SIZE);
    memcpy(header + PC_CARD_TITLE, parts->title, PC_CARD_TEXT_SIZE);
    if (parts->region == 'P') {
        if (language >= 2 && language <= 6 && parts->has_comment[language])
            memcpy(header + PC_CARD_COMMENT, parts->comment[language], PC_CARD_TEXT_SIZE);
        else ok = 0;
    } else {
        memcpy(header + PC_CARD_COMMENT, parts->comment[0], PC_CARD_TEXT_SIZE);
    }
    memcpy(header + PC_CARD_BANNER, parts->banner, PC_CARD_BANNER_SIZE);
    memcpy(header + PC_CARD_ICON, parts->icon, PC_CARD_ICON_SIZE);
    pc_card_header_seal(header);
    return ok;
}

void pc_card_header_set_comment(uint8_t* header, const uint8_t* comment) {
    memset(header + PC_CARD_COMMENT, 0, PC_CARD_TEXT_SIZE);
    for (int i = 0; i < PC_CARD_TEXT_SIZE - 1 && comment[i] != 0; i++) header[PC_CARD_COMMENT + i] = comment[i];
    pc_card_header_seal(header);
}

int pc_gcopy_pick_header(char target, int language, const uint8_t* original, char original_region, int original_language,
                         const PcCardParts* target_parts, const PcCardParts* us_parts, uint8_t* out, int* exact) {
    int source, right = 1;
    if (target_parts != NULL && (!target_parts->ok || target_parts->region != target)) target_parts = NULL;
    if (us_parts != NULL && (!us_parts->ok || us_parts->region != 'E')) us_parts = NULL;
    if (target != 'P') language = 0;
    if (original != NULL && original_region == target) {
        /* the region it came from: its own header, exactly as that game wrote it (call 1) */
        memcpy(out, original, PC_CARD_HEADER_SIZE);
        source = PC_GCOPY_HEADER_OWN;
        if (target == 'P' && language != original_language) {
            /* another European language: that language's comment, from the European disc when it's there */
            if (target_parts != NULL && target_parts->has_comment[language])
                pc_card_header_set_comment(out, target_parts->comment[language]);
            else if (language == 2 && us_parts != NULL)
                pc_card_header_set_comment(out, us_parts->comment[0]); /* English: the US comment, verified the same */
            else right = 0;
        }
    } else if (target_parts != NULL) {
        source = PC_GCOPY_HEADER_DISC;
        right = pc_card_header_build(target_parts, language, out);
    } else if (us_parts != NULL) {
        /* no disc of that region: the US header (verified the same as PAL English's, banner, icon, title and comment) */
        source = PC_GCOPY_HEADER_US;
        pc_card_header_build(us_parts, 0, out);
        right = target == 'E' || (target == 'P' && language == 2);
    } else {
        source = PC_GCOPY_HEADER_KEPT;
        if (original != NULL) memcpy(out, original, PC_CARD_HEADER_SIZE);
        else memset(out, 0, PC_CARD_HEADER_SIZE);
        right = original != NULL && original_region == target;
    }
    if (exact != NULL) *exact = right;
    return source;
}

int pc_gcopy_species_keys(const uint8_t* rel, size_t size, uint32_t* keys, int max) {
    /* a REL module: section count at +0x0C, the section table at +0x10 (8 bytes a section: offset with the "code" bit
     * 0, size); the species table in section 4 at +0x121A04, 0x11C bytes a species, its name key at +0x18, and the
     * count at section 4 + 0x14A6E0 (tools/local/colo/tables.py; docs/engine/game-data-tables.md) */
    if (rel == NULL || size < 0x40 || keys == NULL) return 0;
    const uint32_t sections = be32(rel + 0x0C), table = be32(rel + 0x10);
    if (sections < 5 || (size_t)table + 8u * sections > size) return 0;
    const uint32_t sec4 = be32(rel + table + 4 * 8) & ~1u, sec4_size = be32(rel + table + 4 * 8 + 4);
    if (sec4_size < 0x14A6E4u || (size_t)sec4 + sec4_size > size) return 0;
    const uint32_t count = be32(rel + sec4 + 0x14A6E0u);
    if (count == 0 || count > PC_GCOPY_SPECIES_MAX || 0x121A04u + count * 0x11Cu > sec4_size) return 0;
    int n = 0;
    for (uint32_t id = 0; id < count && (int)id < max; id++, n++) keys[id] = be32(rel + sec4 + 0x121A04u + id * 0x11Cu + 0x18);
    return n;
}

/* a message bank (docs/engine/messages.md; the same reading as pc_region_text.cpp add_bank): u16 group, u8, u8 font,
 * u16 count, 2 bytes of language tag, 8 zero bytes, then count entries {u32 id ascending, u32 offset of the text from
 * the bank's start}; the game's key is group << 20 | id */
int pc_gcopy_blob_names(const uint8_t* blob, size_t size, const char tag[2], const uint32_t* keys, int count,
                        PcGcopyNames* names) {
    int found = 0;
    if (blob == NULL || keys == NULL || names == NULL) return 0;
    memset(names, 0, sizeof(*names));
    if (count > PC_GCOPY_SPECIES_MAX) count = PC_GCOPY_SPECIES_MAX;
    for (size_t at = 0; at + 0x10 <= size; at += 4) {
        const uint8_t* h = blob + at;
        if (h[6] != (uint8_t)tag[0] || h[7] != (uint8_t)tag[1]) continue;
        int zero = 1;
        for (int k = 8; k < 16; k++) zero &= h[k] == 0;
        const uint32_t n = be16(h + 4), group = be16(h);
        if (!zero || n == 0 || at + 0x10 + 8 * (size_t)n > size) continue;
        /* a real bank: ids ascending, every text inside the blob after the table (as add_bank checks; a stray "US" in
         * other data is skipped) */
        int bank = 1;
        for (uint32_t k = 0; bank && k < n; k++) {
            const uint32_t off = be32(h + 0x14 + 8 * k);
            bank = off >= 0x10 + 8 * n && at + off + 2 <= size && (k == 0 || be32(h + 0x10 + 8 * k) > be32(h + 0x08 + 8 * k));
        }
        if (!bank) continue;
        for (uint32_t k = 0; k < n; k++) {
            const uint32_t key = group << 20 | be32(h + 0x10 + 8 * k), off = be32(h + 0x14 + 8 * k);
            if (at + off + 2 > size) continue;
            for (int id = 0; id < count; id++)
                if (keys[id] != 0 && keys[id] == key && names->name[id] == NULL) {
                    names->name[id] = h + off;
                    found++;
                }
        }
        at += 8 * (size_t)n; /* past this bank's table */
    }
    return found;
}

void pc_gcopy_target_stamp(char region, int language, int* area, int* stamp_language, int* language_bytes) {
    int a, l, b;
    if (region == 'J') a = PC_SAVE_AREA_JAPAN, l = 1, b = 0;
    else if (region == 'P') {
        l = language >= 2 && language <= 6 ? language : 2;
        a = PC_SAVE_AREA_PAL, b = l;
    } else a = PC_SAVE_AREA_US, l = 2, b = 0;
    if (area != NULL) *area = a;
    if (stamp_language != NULL) *stamp_language = l;
    if (language_bytes != NULL) *language_bytes = b;
}

/* the two language bytes, slot offsets (block +0x2B and +0xB48; regions.md's table) */
enum { SLOT_LANG_A = PC_SAVE_BLOCK_AT + 0x2B, SLOT_LANG_B = PC_SAVE_BLOCK_AT + 0xB48 };

int pc_gcopy_slot_set_region(uint8_t* plain, char region, int language) {
    int area, lang, bytes, changed = 0;
    pc_gcopy_target_stamp(region, language, &area, &lang, &bytes);
    const struct { size_t at; int value; } set[] = {
        {PC_SAVE_STAMP_AREA, area}, {PC_SAVE_STAMP_LANGUAGE, lang}, {SLOT_LANG_A, bytes}, {SLOT_LANG_B, bytes}};
    for (size_t i = 0; i < sizeof(set) / sizeof(set[0]); i++) {
        if (plain[set[i].at] != (uint8_t)set[i].value) changed = 1;
        plain[set[i].at] = (uint8_t)set[i].value;
    }
    return changed;
}

/* ---- species names (snatcherLeo's call 3) ----
 * Where Pokémon records sit in the slot (docs/engine/save-data.md, verified on real saves): the save block at slot +0x08;
 * the party at block +0x70 (the player) + 0x30, six records; the PC's three boxes at block +0xB88, each a 0x14-byte
 * header (the box name) then 30 records, 0x24A4 bytes a box; the day care at block +0x8168 (a state byte, then the
 * record at +8). A record is 0x138 bytes: species (u16) at +0x00, the nickname (0x16 bytes, big-endian UTF-16, at most
 * 10 letters and the 0 end) at +0x2E. */
enum {
    REC_SIZE = 0x138,
    REC_NICK = 0x2E,
    NICK_SIZE = 0x16,
    NICK_LETTERS = 10,
    PARTY_AT = PC_SAVE_BLOCK_AT + 0x70 + 0x30,
    BOXES_AT = PC_SAVE_BLOCK_AT + 0xB88,
    BOX_SIZE = 0x24A4,
    BOX_HEAD = 0x14,
    DAYCARE_AT = PC_SAVE_BLOCK_AT + 0x8168
};

/* does the 0x16-byte field hold exactly this name (the game's "no nickname" test: the nickname equals the species'
 * name, pokemon_range_8011F5FC.c, which an evolution uses to rename)? */
static int field_is(const uint8_t* field, const uint8_t* name) {
    int n = 0;
    if (name == NULL || be16(name) == 0) return 0;
    for (; n < NICK_LETTERS + 1; n++) {
        const uint16_t want = be16(name + 2 * n), have = be16(field + 2 * n);
        if (want != have) return 0;
        if (want == 0) return 1;
    }
    return 0; /* a table name longer than the field can hold: never equal */
}

static void field_put(uint8_t* field, const uint8_t* name) {
    memset(field, 0, NICK_SIZE);
    for (int n = 0; n < NICK_LETTERS && be16(name + 2 * n) != 0; n++) {
        field[2 * n] = name[2 * n];
        field[2 * n + 1] = name[2 * n + 1];
    }
}

static int rename_one(uint8_t* rec, int where, const PcGcopyNames* const* from, int from_count, const PcGcopyNames* to,
                      PcGcopyRenamed* out, int max, int got) {
    const int species = be16(rec);
    if (species <= 0 || species >= PC_GCOPY_SPECIES_MAX || to->name[species] == NULL) return 0;
    uint8_t* field = rec + REC_NICK;
    if (field_is(field, to->name[species])) return 0; /* already the target's name */
    for (int i = 0; i < from_count; i++) {
        if (from[i] == NULL || !field_is(field, from[i]->name[species])) continue;
        if (out != NULL && got < max) {
            out[got].species = species;
            out[got].where = where;
            memcpy(out[got].from, field, NICK_SIZE);
        }
        field_put(field, to->name[species]);
        if (out != NULL && got < max) memcpy(out[got].to, field, NICK_SIZE);
        return 1;
    }
    return 0;
}

int pc_gcopy_slot_rename_species(uint8_t* plain, const PcGcopyNames* const* from, int from_count, const PcGcopyNames* to,
                                 PcGcopyRenamed* out, int max) {
    int got = 0;
    if (plain == NULL || from == NULL || to == NULL) return 0;
    for (int p = 0; p < 6; p++) got += rename_one(plain + PARTY_AT + p * REC_SIZE, p, from, from_count, to, out, max, got);
    for (int b = 0; b < 3; b++)
        for (int k = 0; k < 30; k++)
            got += rename_one(plain + BOXES_AT + b * BOX_SIZE + BOX_HEAD + k * REC_SIZE, 100 + b * 30 + k, from, from_count,
                              to, out, max, got);
    if (plain[DAYCARE_AT] != 0) got += rename_one(plain + DAYCARE_AT + 8, 200, from, from_count, to, out, max, got);
    return got;
}

static void add_name(PcGcopySlotName* out, int max, int* n, int kind, int where, int species, const uint8_t* text, int units) {
    if (*n >= max || be16(text) == 0) return; /* an empty name has no letters to check */
    out[*n].kind = kind;
    out[*n].where = where;
    out[*n].species = species;
    out[*n].text = text;
    out[*n].units = units;
    (*n)++;
}

int pc_gcopy_slot_names(const uint8_t* plain, PcGcopySlotName* out, int max) {
    int n = 0;
    if (plain == NULL || out == NULL) return 0;
    /* the trainer: block +0x70, a 0x2C-byte field; Rui: block +0xB32 (save-data.md "Section 2"); the game stores at
     * most 10 letters in either (its setters copy 11 units with the end), so 11 units are read */
    add_name(out, max, &n, PC_GCOPY_NAME_TRAINER, 0, 0, plain + PC_SAVE_BLOCK_AT + 0x70, 11);
    add_name(out, max, &n, PC_GCOPY_NAME_PARTNER, 0, 0, plain + PC_SAVE_BLOCK_AT + 0xB32, 11);
    for (int b = 0; b < 3; b++) add_name(out, max, &n, PC_GCOPY_NAME_BOX, b + 1, 0, plain + BOXES_AT + b * BOX_SIZE, BOX_HEAD / 2);
    for (int p = 0; p < 6; p++) {
        const uint8_t* rec = plain + PARTY_AT + p * REC_SIZE;
        if (be16(rec) != 0) add_name(out, max, &n, PC_GCOPY_NAME_POKEMON, p, be16(rec), rec + REC_NICK, NICK_SIZE / 2);
    }
    for (int b = 0; b < 3; b++)
        for (int k = 0; k < 30; k++) {
            const uint8_t* rec = plain + BOXES_AT + b * BOX_SIZE + BOX_HEAD + k * REC_SIZE;
            if (be16(rec) != 0)
                add_name(out, max, &n, PC_GCOPY_NAME_POKEMON, 100 + b * 30 + k, be16(rec), rec + REC_NICK, NICK_SIZE / 2);
        }
    if (plain[DAYCARE_AT] != 0 && be16(plain + DAYCARE_AT + 8) != 0)
        add_name(out, max, &n, PC_GCOPY_NAME_POKEMON, 200, be16(plain + DAYCARE_AT + 8), plain + DAYCARE_AT + 8 + REC_NICK,
                 NICK_SIZE / 2);
    return n;
}

int pc_gcopy_region_apply(uint8_t* gci, size_t len, const PcGcopyRegionJob* job, int* renamed, PcGcopyRenamed* out,
                          int max) {
    const size_t slots_at = PC_GCI_HEADER + PC_GCI_CARD_HEADER;
    int valid = 0, newest = -1, newest_renamed = 0;
    uint32_t newest_counter = 0;
    if (renamed != NULL) *renamed = 0;
    if (gci == NULL || job == NULL || len < slots_at + 3 * (size_t)PC_SAVE_SLOT_SIZE || pc_save_gci_region_letter(gci, len) == 0 ||
        (job->region != 'E' && job->region != 'P' && job->region != 'J'))
        return -1;
    uint8_t* plain = (uint8_t*)malloc(PC_SAVE_SLOT_SIZE);
    uint8_t* before = (uint8_t*)malloc(PC_SAVE_SLOT_SIZE);
    PcGcopyRenamed* list = max > 0 && out != NULL ? (PcGcopyRenamed*)malloc(sizeof(PcGcopyRenamed) * (size_t)max) : NULL;
    if (plain == NULL || before == NULL) {
        free(plain);
        free(before);
        free(list);
        return -1;
    }
    gci[3] = (uint8_t)job->region; /* the game code: GC6E / GC6P / GC6J (maker 01 in every region) */
    if (job->card_header != NULL) memcpy(gci + PC_GCI_HEADER, job->card_header, PC_CARD_HEADER_SIZE);
    for (int s = 0; s < 3; s++) {
        uint8_t* enc = gci + slots_at + (size_t)s * PC_SAVE_SLOT_SIZE;
        if (!pc_save_slot_decrypt(enc, plain)) continue; /* an empty or damaged slot stays exactly as it is */
        valid++;
        memcpy(before, plain, PC_SAVE_SLOT_SIZE);
        pc_gcopy_slot_set_region(plain, job->region, job->language);
        int got = 0;
        if (job->names_from != NULL && job->names_to != NULL)
            got = pc_gcopy_slot_rename_species(plain, job->names_from, job->names_from_count, job->names_to, list, max);
        /* only a slot whose plain bytes changed is sealed again; the rest keep their stored bytes exactly (sealing the
         * same bytes gives the same result anyway, test/save_cipher_test.c, but untouched is simpler to trust) */
        if (memcmp(before, plain, PC_SAVE_SLOT_SIZE) != 0) pc_save_slot_encrypt(plain, enc);
        const uint32_t counter = be32(plain + 4);
        if (newest < 0 || counter > newest_counter) {
            newest = s;
            newest_counter = counter;
            newest_renamed = got;
            if (list != NULL) memcpy(out, list, sizeof(PcGcopyRenamed) * (size_t)(got < max ? got : max));
        }
    }
    free(plain);
    free(before);
    free(list);
    if (renamed != NULL) *renamed = newest_renamed;
    return valid > 0 ? valid : -1;
}
