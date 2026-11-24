/*
 * pc_disc_check.c: is the open disc the one the port is built for? (the reference playthrough Part 1 item 2c, step 1, 2026-11-01,
 * for B to review; docs/port/disc-data-loading.md.)
 *
 * The port will take the game's data from the player's own main.dol, so main.dol must be exactly the USA one the
 * build is verified against: its SHA-1 is checked against config/GC6E01/build.sha1's main.dol line. A different
 * main.dol (another revision or region, a modified disc) stops the start with a message. The SHA-1 below is the
 * standard algorithm (FIPS 180-1), written out here so the port needs no crypto library.
 */
#include <aurora/dvd.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_timer.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_text.h" /* the messages in the player's language */

/* what every "this disc can't be used" message ends with: how a player checks a dump themselves */
/* *Updated 2026-11-13 (every word translatable):* the messages are text keys in ui/lang/<code>.txt ("check.*";
 * pc_text.h); the hint about Dolphin's Verify Integrity is "check.verify_hint" */

/* config/GC6E01/build.sha1, build/GC6E01/main.dol: Pokemon Colosseum (USA), GC6E01 */
static const char USA_MAIN_DOL_SHA1[] = "870e8b9693ca780782d80f22a6a4572d8ba9458f";

typedef struct {
    uint32_t h[5];
    uint64_t length;
    unsigned char block[64];
    size_t used;
} Sha1;

static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(Sha1* s, const unsigned char* p) {
    uint32_t w[80], a, b, c, d, e;
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k, t;
        if (i < 20) {
            f = (b & c) | (~b & d), k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d, k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d, k = 0xCA62C1D6u;
        }
        t = rol(a, 5) + f + e + k + w[i];
        e = d, d = c, c = rol(b, 30), b = a, a = t;
    }
    s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d, s->h[4] += e;
}

/* streaming form (2026-11-02, the whole-disc check reads files in chunks) */
static void sha1_init(Sha1* s) {
    static const uint32_t h0[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    memcpy(s->h, h0, sizeof(h0));
    s->length = 0;
    s->used = 0;
}

static void sha1_update(Sha1* s, const unsigned char* p, size_t n) {
    s->length += (uint64_t)n * 8;
    if (s->used != 0) {
        size_t take = 64 - s->used < n ? 64 - s->used : n;
        memcpy(s->block + s->used, p, take);
        s->used += take, p += take, n -= take;
        if (s->used < 64) {
            return;
        }
        sha1_block(s, s->block);
        s->used = 0;
    }
    for (; n >= 64; p += 64, n -= 64) {
        sha1_block(s, p);
    }
    memcpy(s->block, p, n);
    s->used = n;
}

static void sha1_final(Sha1* s, unsigned char out[20]) {
    unsigned char tail[128] = {0};
    size_t pad = s->used < 56 ? 64 : 128;
    memcpy(tail, s->block, s->used);
    tail[s->used] = 0x80;
    for (int i = 0; i < 8; i++) {
        tail[pad - 1 - i] = (unsigned char)(s->length >> (8 * i));
    }
    sha1_block(s, tail);
    if (pad == 128) {
        sha1_block(s, tail + 64);
    }
    for (int i = 0; i < 20; i++) {
        out[i] = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));
    }
}

static void sha1(const unsigned char* data, size_t size, char hex[41]) {
    Sha1 s;
    unsigned char out[20];
    sha1_init(&s);
    sha1_update(&s, data, size);
    sha1_final(&s, out);
    for (int i = 0; i < 20; i++) {
        snprintf(hex + 2 * i, 3, "%02x", out[i]);
    }
}

/* ---- main.dol's bytes by GameCube address (Part 1 item 2c step 2: the data the exe no longer carries) ----
 * The DOL header (big-endian): file offsets of 7 text and 11 data sections at 0x00 / 0x1C, their load addresses at
 * 0x48 / 0x64, their sizes at 0x90 / 0xAC. */
static uint32_t be32(const u8* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* A pointer to size bytes of main.dol loaded at GameCube address addr, or NULL (not all in one section, or no disc) */
static const u8* dol_at(uint32_t addr, uint32_t size) {
    const u8* dol;
    size_t dol_size;
    if (!aurora_dvd_main_dol(&dol, &dol_size) || dol_size < 0x100) {
        return NULL;
    }
    for (int i = 0; i < 18; i++) {
        const uint32_t off = be32(dol + 4 * i), at = be32(dol + 0x48 + 4 * i), len = be32(dol + 0x90 + 4 * i);
        if (len != 0 && addr >= at && addr - at <= len && size <= len - (addr - at) && off + len <= dol_size) {
            return dol + off + (addr - at);
        }
    }
    return NULL;
}

static uint32_t crc32_of(const u8* p, size_t n) { /* the zlib CRC-32 (Python's zlib.crc32) */
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

static const u8* dol_checked(uint32_t addr, uint32_t size, uint32_t crc) {
    const u8* src = dol_at(addr, size);
    if (src == NULL || crc32_of(src, size) != crc) {
        fprintf(stderr, "[disc] main.dol has no matching data at 0x%08X (+0x%X)%s: stopping\n", (unsigned)addr,
                (unsigned)size, src == NULL ? "" : " (checksum differs)");
        abort();
    }
    return src;
}

/* pc_data_main.c (pcdata.py): copy main.dol's bytes for [addr, addr + size) to that GameCube address, unchanged */
void pc_disc_dol_copy(uint32_t addr, uint32_t size, uint32_t crc) {
    memcpy((void*)(uintptr_t)addr, dol_checked(addr, size, crc), size);
}

/* main.dol's bytes for [addr, addr + size) into a PC buffer, unchanged (game art the PC build doesn't carry, e.g. the
 * save banner and icon in memcard.c; Part 1 item 2c step 5) */
void pc_disc_dol_read(void* dst, uint32_t addr, uint32_t size, uint32_t crc) {
    memcpy(dst, dol_checked(addr, size, crc), size);
}

/* the same, but 1 / 0 instead of stopping when the bytes are missing or differ: for extras the game itself doesn't need
 * (the window icon's Espeon and Umbreon, pc_window_icon.cpp) */
int pc_disc_dol_try_read(void* dst, uint32_t addr, uint32_t size, uint32_t crc) {
    const u8* src = dol_at(addr, size);
    if (src == NULL || crc32_of(src, size) != crc) {
        return 0;
    }
    memcpy(dst, src, size);
    return 1;
}

/* the same into a PC buffer, each esz-byte element turned from big-endian to the PC's order (pcdata.py's typed tables) */
void pc_disc_dol_read_swapped(void* dst, uint32_t addr, uint32_t size, uint32_t esz, uint32_t crc) {
    const u8* src = dol_checked(addr, size, crc);
    u8* out = (u8*)dst;
    for (uint32_t i = 0; i + esz <= size; i += esz) {
        for (uint32_t k = 0; k < esz; k++) {
            out[i + k] = src[i + esz - 1 - k];
        }
    }
}

/* pcdata.py's record arrays (pcdata_types.txt `{...}[]`): the PC records have the native layout, so each number field is
 * read from its GameCube offset in the record and stored, in the PC's order, at its PC offset (the compiler's offsetof);
 * pointer fields are left zero here and filled by the pointer-slot table afterwards */
typedef struct PcDiscField {
    uint32_t pc_offset, gc_offset, size;
} PcDiscField;
void pc_disc_fill_records(void* dst, uint32_t pc_record_size, const PcDiscField* fields, uint32_t field_count,
                          uint32_t addr, uint32_t gc_record_size, uint32_t record_count, uint32_t crc) {
    const u8* src = dol_checked(addr, gc_record_size * record_count, crc);
    for (uint32_t r = 0; r < record_count; r++) {
        u8* out = (u8*)dst + (size_t)r * pc_record_size;
        const u8* in = src + (size_t)r * gc_record_size;
        for (uint32_t f = 0; f < field_count; f++) {
            for (uint32_t k = 0; k < fields[f].size; k++) {
                out[fields[f].pc_offset + k] = in[fields[f].gc_offset + fields[f].size - 1 - k];
            }
        }
    }
}

/* the same, each esz-byte element turned from big-endian to the PC's order (pcfixed.txt "var" entries) */
void pc_disc_dol_copy_swapped(uint32_t addr, uint32_t size, uint32_t esz, uint32_t crc) {
    const u8* src = dol_checked(addr, size, crc);
    u8* dst = (u8*)(uintptr_t)addr;
    for (uint32_t i = 0; i < size; i += esz) {
        for (uint32_t k = 0; k < esz; k++) {
            dst[i + k] = src[i + esz - 1 - k];
        }
    }
}

/* After the disc is open. Returns 1 when its main.dol is the USA one; else tells the player (show_box) and returns 0 */
int pc_disc_check_main_dol(int show_box) {
    const u8* dol;
    size_t size;
    char hex[41];
    if (!aurora_dvd_main_dol(&dol, &size)) {
        fprintf(stderr, "[disc] the disc image has no readable main.dol\n");
        return 0;
    }
    sha1(dol, size, hex);
    if (strcmp(hex, USA_MAIN_DOL_SHA1) == 0) {
        printf("[disc] main.dol is Pokemon Colosseum (USA): SHA-1 %s, %u bytes\n", hex, (unsigned)size);
        return 1;
    }
    fprintf(stderr, "[disc] main.dol's SHA-1 is %s, not the USA disc's %s: another revision, region or a modified "
                    "disc\n",
            hex, USA_MAIN_DOL_SHA1);
    if (show_box) {
        /* in the player's language (ui/lang "check.*", pc_text.h) */
        char message[1600];
        snprintf(message, sizeof(message), "%s\n\n%s", pc_text("check.dol.text"), pc_text("check.verify_hint"));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pc_text("check.dol.title"), message, NULL);
    }
    return 0;
}

/* ---- the whole-disc check (2026-11-02, snatcherLeo: "checks the SHA ... of the ROM, to verify it is a copy that
 * works, and if it isn't, it warns people and tells them why"; for B to review; docs/port/disc-data-loading.md "The
 * disc check") ----
 * Every file on the disc against the reference list the build made from a verified USA disc
 * (tools/local/port/discmanifest.py -> build-pc/generated/pc_disc_manifest.c: names, sizes, SHA-1s). The result is
 * remembered per image (its size and change time) in the cache file, so a later start skips the reading. Missing files
 * stop the start (the game can't run without them); files that differ (a modified image: a hack, a translation, a
 * randomizer; or a damaged dump) are named, and the player chooses to play anyway or quit. Automated runs log only. */
typedef struct { const char* path; uint32_t size; uint8_t sha1[20]; } PcDiscFile;
/* the generated list, or the empty one in pc_disc_manifest_none.c (a link without the generated list: the platform's
 * tests, firstlink's nodata step), and then the check is skipped */
extern const uint32_t pc_disc_fst_entries, pc_disc_file_count;
extern const PcDiscFile pc_disc_files[];
extern const char pc_disc_manifest_id[];
s32 DVDConvertPathToEntrynum(const char* path);

static void sha1_consume(void* user, const u8* data, size_t size) { sha1_update((Sha1*)user, data, size); }

/* 1: go on; 0: stop (the player was told why, or the log says) */
int pc_disc_verify_files(const char* image, const char* cache, int interactive) {
    char key[200] = "", line[200] = "", names[700] = "";
    SDL_PathInfo info;
    int missing = 0, differ = 0, listed = 0;
    s32 entries;
    Uint64 t0;
    if (pc_disc_file_count == 0) {
        printf("[disc] this build has no reference list (no USA disc image at build time): whole-disc check skipped\n");
        return 1;
    }
    if (image != NULL && SDL_GetPathInfo(image, &info)) {
        snprintf(key, sizeof(key), "%s %llu %lld", pc_disc_manifest_id, (unsigned long long)info.size,
                 (long long)info.modify_time);
    }
    if (key[0] != 0 && cache != NULL) {
        FILE* f = fopen(cache, "r");
        if (f != NULL) {
            char state[40] = "";
            if (fgets(line, sizeof(line), f) != NULL && fgets(state, sizeof(state), f) != NULL) {
                line[strcspn(line, "\r\n")] = 0;
                state[strcspn(state, "\r\n")] = 0;
                if (strcmp(line, key) == 0 && (strcmp(state, "ok") == 0 || strcmp(state, "accepted") == 0)) {
                    printf("[disc] whole-disc check: %s earlier for this image (%s)\n",
                           strcmp(state, "ok") == 0 ? "passed" : "differences accepted by the player", cache);
                    fclose(f);
                    return 1;
                }
            }
            fclose(f);
        }
    }
    t0 = SDL_GetTicks();
    for (uint32_t i = 0; i < pc_disc_file_count; i++) {
        const PcDiscFile* want = &pc_disc_files[i];
        char path[300];
        unsigned char got[20];
        Sha1 s;
        s32 entry;
        s64 size;
        snprintf(path, sizeof(path), "/%s", want->path);
        entry = DVDConvertPathToEntrynum(path);
        sha1_init(&s);
        size = entry < 0 ? -1 : aurora_dvd_read_entry(entry, 1 << 20, sha1_consume, &s);
        if (size < 0) {
            missing++;
        } else {
            sha1_final(&s, got);
            if ((uint64_t)size == want->size && memcmp(got, want->sha1, 20) == 0) {
                continue;
            }
            differ++;
        }
        fprintf(stderr, "[disc] %s: %s\n", want->path, size < 0 ? "missing" : "differs from the USA disc");
        if (listed < 8) {
            size_t used = strlen(names);
            snprintf(names + used, sizeof(names) - used, "\n  %s%s%s", want->path, size < 0 ? " " : "",
                     size < 0 ? pc_text("check.files.missing_tag") : "");
            listed++;
        }
    }
    entries = aurora_dvd_base_entry_count();
    printf("[disc] whole-disc check: %u files, %d missing, %d differ, %d file table entries (the USA disc: %u); %llu ms\n",
           (unsigned)pc_disc_file_count, missing, differ, (int)entries, (unsigned)pc_disc_fst_entries,
           (unsigned long long)(SDL_GetTicks() - t0));
    if (missing == 0 && differ == 0 && (uint32_t)entries == pc_disc_fst_entries) {
        if (key[0] != 0 && cache != NULL) {
            FILE* f = fopen(cache, "w");
            if (f != NULL) {
                fprintf(f, "%s\nok\n", key);
                fclose(f);
            }
        }
        return 1;
    }
    {
        char message[2400], count_line[400], n_text[16], all_text[16], extra[200];
        const int total = missing + differ;
        /* the player's message in their language (ui/lang "check.*", pc_text.h): whole sentences, put in order; the
         * file names stay as on the disc ("(missing)" is a word, so it is a key too) */
        if (total == 0) {
            snprintf(n_text, sizeof(n_text), "%d", (int)entries);
            snprintf(all_text, sizeof(all_text), "%u", (unsigned)pc_disc_fst_entries);
            pc_text_format(extra, sizeof(extra), "check.files.table", "n", n_text, "usa", all_text, NULL);
            snprintf(names, sizeof(names), "\n  (%s)", extra);
        } else if (total > listed) {
            size_t used = strlen(names);
            snprintf(n_text, sizeof(n_text), "%d", total - listed);
            pc_text_format(extra, sizeof(extra), "check.files.more", "n", n_text, NULL);
            snprintf(names + used, sizeof(names) - used, "\n  %s", extra);
        }
        snprintf(n_text, sizeof(n_text), "%d", total);
        snprintf(all_text, sizeof(all_text), "%u", (unsigned)pc_disc_file_count);
        pc_text_format(count_line, sizeof(count_line), missing > 0 ? "check.files.count_missing" : "check.files.count_differ",
                       "n", n_text, "all", all_text, NULL);
        snprintf(message, sizeof(message), "%s\n\n%s%s\n\n%s\n\n%s",
                 pc_text(missing > 0 ? "check.files.missing" : "check.files.not_exact"), count_line, names,
                 pc_text(missing > 0 ? "check.files.use_complete" : total > 50 ? "check.files.damaged" : "check.files.modified"),
                 pc_text("check.verify_hint"));
        if (!interactive) {
            fprintf(stderr, "[disc] %s\n", missing > 0 ? "missing files: stopping" : "differences: going on (an automated run)");
            return missing == 0;
        }
        if (missing > 0) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pc_text("check.files.title_missing"), message, NULL);
            return 0;
        }
        {
            const SDL_MessageBoxButtonData buttons[] = {
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, pc_text("check.files.quit")},
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, pc_text("check.files.play_anyway")},
            };
            const SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING, NULL, pc_text("check.files.title_differ"),
                                            message, 2, buttons, NULL};
            int choice = 0;
            if (!SDL_ShowMessageBox(&box, &choice) || choice != 1) {
                fprintf(stderr, "[disc] the player chose to quit\n");
                return 0;
            }
            fprintf(stderr, "[disc] the player chose to play anyway (remembered for this image)\n");
            if (key[0] != 0 && cache != NULL) {
                FILE* f = fopen(cache, "w");
                if (f != NULL) {
                    fprintf(f, "%s\naccepted\n", key);
                    fclose(f);
                }
            }
            return 1;
        }
    }
}
