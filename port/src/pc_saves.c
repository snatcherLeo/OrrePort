/*
 * pc_saves.c: OrrePort's save slots, previous versions and importing (pc_saves.h has the overview; step 2.1, snatcherLeo,
 * 2026-11-13: "Import a save", "Restore from previous version", the save's file named after the save; the project's reviews).
 *
 * Why each part works this way:
 * - One folder per save (Saves\Slot N\): the game keeps exactly one save per memory card, and Dolphin refuses two
 *   saves of the same game in one GCI folder (verified from Dolphin's GCMemcardDirectory.cpp), so a folder is a card.
 *   The numbers are fixed because setup files, recordings and reports refer to them; the player sees names.
 * - slot.ini holds the name exactly as typed; the file on disk gets a cleaned copy of it (clean_name below), so the
 *   folder is easy to browse and a file copied into Dolphin just works (Dolphin reads any .gci by its header).
 * - Previous versions are whole-file copies kept only when the save changed (its SHA-1 differs from the newest kept
 *   copy): one before each in-game save (the card's before-write hook) and one at launch, so "previous version" means
 *   the previous in-game save, and identical copies don't pile up. The newest KEEP_VERSIONS (10) are kept per
 *   save, the oldest removed past that (snatcherLeo, 2026-11-13, asking "is there a limit?", then "Build it": 384 KB each,
 *   so at most about 4 MB per save); the copy a pending Undo needs is never removed. Nothing else is ever deleted.
 * - Undo after going back (mockup 74; snatcherLeo, 2026-11-13): a restore first keeps the save as it is (before_restore) and
 *   records that copy in slot.ini (undo=); Undo copies it back. It is offered until that save is played (the card
 *   opens it) or restored again, so leaving the Saves page doesn't lose it.
 * - Reading a save never writes it: the info comes from a decrypted copy in memory (docs/engine/save-data.md).
 *
 * Save layout used here (verified 2026-11-13 by decrypting a the reference playthrough save with these rules):
 *   .gci = 0x40-byte header (game code at 0, maker at 4, block count at 0x38) + the card file (blocks of 0x2000);
 *   three save slots of 0x1E000 bytes from file offset 0x6000 (after the header); in a slot, +0x04 the save counter
 *   (highest valid one wins, as the game does), +0x18..+0x1DFD8 encrypted with a SHA-1 key stream starting from the
 *   inverted digest stored at +0x1DFEC; the digest covers the plain slot 0..+0x1DFD8 with the word at +0x0C zeroed.
 *   In the decrypted save block (slot +0x08): +0x00 the region stamp (0x0B 0x03 0x02 0x02 = Colosseum, Gen 3, USA,
 *   English), +0x20 the play time in seconds (big-endian float), +0x70 the trainer's name (big-endian UTF-16).
 * - A whole memory card (.raw): block 0 header, blocks 1 / 2 the directory and its spare, 3 / 4 the block map and its
 *   spare (the copy with the higher update counter is current); a directory entry is the same 0x40 bytes as a .gci
 *   header, with the first block at +0x36 and the block count at +0x38; the map's entry for block b (b >= 5) is at
 *   0x0A + (b - 5) * 2 and names the next block. Colosseum's save is copied out as a .gci (entry + blocks).
 *
 * Platforms: SDL's file calls everywhere; the Recycle Bin is Windows' shell (SHFileOperationW); Linux and macOS move a
 * deleted slot to Saves\Deleted\ for now (likely later: the freedesktop trash spec / NSFileManager trashItem).
 */
#include "pc_saves.h"
#include "pc_save_cipher.h" /* SHA-1 and the slot cipher (3.5: shared with the GameCube copy's re-encrypt) */
#include "pc_save_region.h" /* the newest slot, the region stamp and the import's game-code change (3.5 steps 6-7) */
#include "pc_names.h" /* a save's long names (2.4): the trainer's full name for the hub */
#include "pc_zip.h"   /* the one-file .orresave (2.4 part 5) */
#include "pc_settings.h"
#include "pc_options.h" /* the new-saves default for GameCube-accurate mode (2.12) */

#include <SDL3/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

int pc_platform_automated_run(void); /* pc_platform.c */

/* the card's before-write hook (extern/aurora/lib/card/CardGciFolder.hpp) */
extern void (*aurora_card_before_write)(const char* path);

enum {
    GCI_HEADER = 0x40,
    BLOCK = 0x2000,
    SAVE_SLOT0 = 0x6000,
    SAVE_SLOT = 0x1E000,
    ENC_START = 0x18,
    ENC_END = 0x1DFD8,
    DIGEST_AT = 0x1DFEC,
    MAX_SLOTS = 4096,
    KEEP_VERSIONS = 10, /* previous versions kept per save (snatcherLeo, 2026-11-13) */
    NAME_CAP = 64 /* codepoints in a file name: the portable folder can sit deep, Windows' paths stop at 260 */
};

static char s_saves[1100]; /* ...\Colosseum\Saves\ ; "" outside the portable layout */

/* SHA-1 and the slot cipher live in pc_save_cipher.c (moved there 2026-11-19 so the GameCube copy can re-encrypt with
 * the same code it decrypts with) */
static void sha1(const uint8_t* data, size_t len, uint8_t out[20]) { pc_save_sha1(data, len, out); }

static uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* ---------------------------------------------------------------- reading a save (never writes) */
typedef struct {
    int code;              /* PC_SAVE_IMPORTED (0) or a PC_SAVE_ERR_* */
    char trainer[48];
    unsigned play_seconds;
    uint8_t digest[20];    /* the newest valid slot's own SHA-1: the same save has the same digest */
    /* for the SAVE FILES list's help box (pc_saves_read_details); only filled when story_flags_out is set */
    unsigned floor_id;     /* block +0x1C: the save header (+0x08) field +0x14, gamedatasaveBiosGetFloorid */
    unsigned* story_flags_out; /* PC_SAVE_STORY_FLAG_WORDS words, or NULL: section 6 at block +0x1BE5C */
    /* 3.5 (2026-11-19): where the save comes from. region_letter: the .gci's game code letter ('E', 'P', 'J'); area and
     * language: the newest slot's stamp (pc_save_region.h), which stays when a European or Japanese save is imported
     * and played here, so the hub and the SAVE FILES list read the region from it, not from the game code */
    char region_letter;
    int area;
    int language;
} SaveFacts;

/* the decrypted save block's offsets read here (docs/engine/save-data.md; the header layout from gamedatasaveBios.c:
 * header at block +0x08, floor id at its +0x14, play time at +0x18) */
enum { BLOCK_FLOOR_ID = 0x1C, BLOCK_STORY_FLAGS = 0x1BE5C };

/* UTF-16BE (the game's text for US saves) to UTF-8, stopping at 0 */
static void utf16be_to_utf8(const uint8_t* p, size_t chars, char* out, size_t size) {
    size_t o = 0;
    for (size_t i = 0; i < chars; i++) {
        unsigned c = be16(p + i * 2);
        if (c == 0) break;
        char tmp[4];
        size_t n;
        if (c < 0x80) { tmp[0] = (char)c; n = 1; }
        else if (c < 0x800) { tmp[0] = (char)(0xC0 | c >> 6); tmp[1] = (char)(0x80 | (c & 0x3F)); n = 2; }
        else { tmp[0] = (char)(0xE0 | c >> 12); tmp[1] = (char)(0x80 | ((c >> 6) & 0x3F)); tmp[2] = (char)(0x80 | (c & 0x3F)); n = 3; }
        if (o + n >= size) break;
        memcpy(out + o, tmp, n);
        o += n;
    }
    out[o] = '\0';
}

/* flags_out: PC_SAVE_STORY_FLAG_WORDS words for the story flags, or NULL (every caller but pc_saves_read_details) */
static void read_gci_facts_ex(const uint8_t* gci, size_t len, SaveFacts* f, unsigned* flags_out) {
    memset(f, 0, sizeof(*f));
    f->story_flags_out = flags_out;
    if (len < GCI_HEADER || (len - GCI_HEADER) % BLOCK != 0 || be16(gci + 0x38) == 0 ||
        len != GCI_HEADER + (size_t)be16(gci + 0x38) * BLOCK) {
        f->code = PC_SAVE_ERR_NOT_SAVE;
        return;
    }
    if (memcmp(gci, "GXX", 3) == 0) { f->code = PC_SAVE_ERR_XD; return; }
    if (memcmp(gci, "GC6", 3) != 0) { f->code = PC_SAVE_ERR_OTHER_GAME; return; }
    /* *Changed 2026-11-19 (3.5 step 7, snatcherLeo's import decision of 2026-11-17):* European (GC6P) and Japanese (GC6J) saves
     * are read too: the save layout and cipher are the same in every region (regions.md, verified on real saves). Only
     * an unknown region letter or another maker code is still refused (saves.err.region) */
    f->region_letter = pc_save_gci_region_letter(gci, len);
    if (f->region_letter == 0) { f->code = PC_SAVE_ERR_REGION; return; }
    if (len < GCI_HEADER + SAVE_SLOT0 + 3 * (size_t)SAVE_SLOT) { f->code = PC_SAVE_ERR_DAMAGED; return; }
    uint8_t* slot = malloc(SAVE_SLOT);
    int found = 0;
    if (slot == NULL) { f->code = PC_SAVE_ERR_READ; return; }
    /* the newest valid slot, as the game loads it (pc_save_region.c; *changed 2026-11-19:* moved there so the region
     * test reads the stamp with the same code; the same pick as the loop that was here: highest counter, first on a
     * tie, digest checked) */
    if (pc_save_gci_newest_slot(gci, len, slot) >= 0) {
        found = 1;
        const uint8_t* block = slot + 8;
        uint32_t bits = be32(block + 0x20);
        float seconds;
        memcpy(&seconds, &bits, 4);
        f->play_seconds = seconds > 0 && seconds < 4.0e9f ? (unsigned)seconds : 0;
        utf16be_to_utf8(block + 0x70, 0x2C / 2, f->trainer, sizeof(f->trainer));
        memcpy(f->digest, slot + DIGEST_AT, 20);
        f->floor_id = be32(block + BLOCK_FLOOR_ID);
        if (f->story_flags_out != NULL) /* big-endian on the card; the game swaps them on load (pc_save_fields.c) */
            for (int i = 0; i < PC_SAVE_STORY_FLAG_WORDS; i++) f->story_flags_out[i] = be32(block + BLOCK_STORY_FLAGS + i * 4);
        pc_save_stamp(slot, &f->area, &f->language); /* 3.5 step 6: block +0x02 / +0x03 of this newest slot */
    }
    free(slot);
    f->code = found ? PC_SAVE_IMPORTED : PC_SAVE_ERR_DAMAGED;
}
static void read_gci_facts(const uint8_t* gci, size_t len, SaveFacts* f) { read_gci_facts_ex(gci, len, f, NULL); }

/* ---------------------------------------------------------------- what a save file says, remembered
 * Why (snatcherLeo, 2026-11-15: pressing LOAD "feels slower", the sound crackled): reading a save means decrypting its three
 * copies (a SHA-1 per 20 bytes, about 5 ms a file), and the SAVE FILES list read every slot's file in one game frame
 * (85 ms with 15 saves, measured from his log's [slowframe] lines). So each file's facts are kept, by path, and used
 * again while the file's size and change time are the same; and pc_saves_warm reads them all in the background at
 * start-up, so the list opens without reading any. A file the game has just saved is read once more (one file, a few
 * ms). Platforms: SDL threads and file times; the same on every OS. */
typedef struct {
    char path[1300];
    Uint64 size;
    SDL_Time mtime;
    SaveFacts facts;
    unsigned flags[PC_SAVE_STORY_FLAG_WORDS];
    uint32_t card_time;
} FactsCache;
enum { FACTS_CACHE = 64 };
static FactsCache s_facts[FACTS_CACHE];
static int s_facts_next;
static SDL_Mutex* s_facts_mutex;

/* the facts of the save file at path (and its story flags and card time when asked), read or remembered; 0 when the
 * file can't be read (f->code is then PC_SAVE_ERR_READ) */
static int facts_of(const char* path, SaveFacts* f, unsigned* flags_out, uint32_t* card_time) {
    SDL_PathInfo pi;
    memset(f, 0, sizeof(*f));
    f->code = PC_SAVE_ERR_READ;
    if (path == NULL || !SDL_GetPathInfo(path, &pi)) return 0;
    if (s_facts_mutex != NULL) SDL_LockMutex(s_facts_mutex);
    for (int i = 0; i < FACTS_CACHE; i++) {
        const FactsCache* c = &s_facts[i];
        if (c->path[0] != '\0' && c->size == pi.size && c->mtime == pi.modify_time && strcmp(c->path, path) == 0) {
            *f = c->facts;
            if (flags_out != NULL) memcpy(flags_out, c->flags, sizeof(c->flags));
            if (card_time != NULL) *card_time = c->card_time;
            if (s_facts_mutex != NULL) SDL_UnlockMutex(s_facts_mutex);
            return 1;
        }
    }
    if (s_facts_mutex != NULL) SDL_UnlockMutex(s_facts_mutex);
    size_t len = 0;
    uint8_t* data = SDL_LoadFile(path, &len);
    if (data == NULL) return 0;
    static SDL_TLSID tls; /* a scratch flags buffer per thread (the list and the warm-up may read at once) */
    unsigned* flags = SDL_GetTLS(&tls);
    if (flags == NULL) {
        flags = SDL_malloc(sizeof(unsigned) * PC_SAVE_STORY_FLAG_WORDS);
        if (flags == NULL) { SDL_free(data); return 0; }
        SDL_SetTLS(&tls, flags, SDL_free);
    }
    memset(flags, 0, sizeof(unsigned) * PC_SAVE_STORY_FLAG_WORDS);
    read_gci_facts_ex(data, len, f, flags);
    f->story_flags_out = NULL;
    const uint32_t ct = len >= 0x2C ? be32(data + 0x28) : 0;
    SDL_free(data);
    if (flags_out != NULL) memcpy(flags_out, flags, sizeof(unsigned) * PC_SAVE_STORY_FLAG_WORDS);
    if (card_time != NULL) *card_time = ct;
    if (s_facts_mutex != NULL) SDL_LockMutex(s_facts_mutex);
    int at = -1;
    for (int i = 0; i < FACTS_CACHE && at < 0; i++)
        if (strcmp(s_facts[i].path, path) == 0) at = i; /* the same file changed: its entry is replaced */
    if (at < 0) at = s_facts_next, s_facts_next = (s_facts_next + 1) % FACTS_CACHE;
    FactsCache* c = &s_facts[at];
    SDL_snprintf(c->path, sizeof(c->path), "%s", path);
    c->size = pi.size, c->mtime = pi.modify_time, c->facts = *f, c->card_time = ct;
    memcpy(c->flags, flags, sizeof(c->flags));
    if (s_facts_mutex != NULL) SDL_UnlockMutex(s_facts_mutex);
    return 1;
}

int pc_saves_read_info(const char* path, char* trainer, size_t trainer_size, unsigned* play_seconds) {
    SaveFacts f;
    if (!facts_of(path, &f, NULL, NULL)) return 0; /* remembered while the file is unchanged */
    if (f.code != PC_SAVE_IMPORTED) return 0;
    if (trainer != NULL && trainer_size > 0) SDL_snprintf(trainer, trainer_size, "%s", f.trainer);
    if (play_seconds != NULL) *play_seconds = f.play_seconds;
    return 1;
}

/* pc_saves.h says what each field is for. The card time is the directory entry's "last modified" word (+0x28 of the
 * .gci's 0x40-byte header, the same entry a memory card's directory holds), written by the console's card library
 * from its own clock; 0 or all ones means the file carries none. Platforms: all the same. */
int pc_saves_read_details(const char* path, PcSaveDetails* out) {
    SaveFacts f;
    uint32_t t = 0;
    if (out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->card_time = -1;
    if (!facts_of(path, &f, out->story_flags, &t)) return 0; /* remembered while the file is unchanged */
    if (f.code == PC_SAVE_IMPORTED) {
        out->floor_id = f.floor_id;
        out->card_time = (t != 0 && t != 0xFFFFFFFFu) ? (long long)t : -1;
    }
    return f.code == PC_SAVE_IMPORTED;
}

/* Colosseum's save copied out of a whole memory card as .gci bytes (malloc'd), or NULL; *why on failure */
static uint8_t* extract_from_raw(const uint8_t* raw, size_t len, size_t* out_len, int* why) {
    *why = PC_SAVE_ERR_NOT_SAVE;
    if (len < 5 * BLOCK || len % BLOCK != 0) return NULL;
    const uint8_t* dir = raw + BLOCK;
    if (be16(raw + 2 * BLOCK + 0x1FFA) > be16(raw + BLOCK + 0x1FFA)) dir = raw + 2 * BLOCK;
    const uint8_t* bat = raw + 3 * BLOCK;
    if (be16(raw + 4 * BLOCK + 4) > be16(raw + 3 * BLOCK + 4)) bat = raw + 4 * BLOCK;
    int other_region = 0, xd = 0;
    /* *Changed 2026-11-19 (3.5 step 7):* a European or Japanese card's Colosseum save is taken too (the importer then
     * changes its game code, pc_saves_import); only an unknown region letter or maker is skipped. A card normally holds
     * one console's saves, so the first Colosseum entry is the one */
    for (int e = 0; e < 127; e++) {
        const uint8_t* d = dir + e * 0x40;
        if (d[0] == 0xFF) continue;
        if (memcmp(d, "GXX", 3) == 0) { xd = 1; continue; }
        if (memcmp(d, "GC6", 3) != 0) continue;
        if (pc_save_gci_region_letter(d, 0x40) == 0) { other_region = 1; continue; }
        const unsigned count = be16(d + 0x38);
        unsigned block = be16(d + 0x36);
        uint8_t* gci = malloc(GCI_HEADER + (size_t)count * BLOCK);
        if (gci == NULL || count == 0) { free(gci); *why = PC_SAVE_ERR_READ; return NULL; }
        memcpy(gci, d, GCI_HEADER);
        for (unsigned i = 0; i < count; i++) {
            if (block < 5 || (size_t)(block + 1) * BLOCK > len) { free(gci); *why = PC_SAVE_ERR_DAMAGED; return NULL; }
            memcpy(gci + GCI_HEADER + (size_t)i * BLOCK, raw + (size_t)block * BLOCK, BLOCK);
            block = be16(bat + 0x0A + (block - 5) * 2);
        }
        *out_len = GCI_HEADER + (size_t)count * BLOCK;
        return gci;
    }
    *why = other_region ? PC_SAVE_ERR_REGION : xd ? PC_SAVE_ERR_XD : PC_SAVE_ERR_NO_COLOSSEUM;
    return NULL;
}

/* ---------------------------------------------------------------- file names */
/* *Corrected 2026-11-14 (checking snatcherLeo's "all saves can be read on any OS"):* the ASCII test is limited to
 * c < 0x80. strchr compares only the low byte of its argument, so letters such as U+0100 (low byte 00: the string's
 * end matches), U+013A (3A ':') or U+015C (5C '\') were dropped from file names before. */
static int forbidden(unsigned c) {
    return c < 0x20 || c == 0x7F || (c < 0x80 && strchr("<>:\"/\\|?*", (int)c) != NULL) ||
           /* emoji and their joiners stay in the shown name only (the plan's open emoji item: removed from files) */
           (c >= 0x1F000 && c <= 0x1FAFF) || (c >= 0x2600 && c <= 0x27BF) || c == 0xFE0F || c == 0x200D;
}

/* The display name made safe as a file name on Windows, macOS and Linux (snatcherLeo: "users name the save file anything
 * they want to, but we sanitize the actual filename"; the project's list): drops < > : " / \ | ? *, control characters and
 * emoji, trims spaces and dots at both ends, caps the length at NAME_CAP codepoints; empty becomes "Save N". Windows'
 * reserved names (CON, NUL, COM1...) can't occur: " - Colosseum" always follows. Unicode normalisation (macOS NFC) is
 * left for the macOS build (likely needed there; Windows keeps what was typed). */
static void clean_name(const char* in, int slot, char* out, size_t size) {
    size_t o = 0;
    int cps = 0;
    const unsigned char* p = (const unsigned char*)in;
    while (*p != '\0' && cps < NAME_CAP) {
        unsigned c = *p;
        int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        if (n == 2) c = (c & 0x1F) << 6 | (p[1] & 0x3F);
        else if (n == 3) c = (c & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F);
        else if (n == 4) c = (c & 0x07) << 18 | (p[1] & 0x3F) << 12 | (p[2] & 0x3F) << 6 | (p[3] & 0x3F);
        for (int i = 1; i < n; i++) if (p[i] == '\0') { n = i; c = '?'; break; }
        if (!forbidden(c) && o + (size_t)n < size) {
            memcpy(out + o, p, (size_t)n);
            o += (size_t)n;
            cps++;
        }
        p += n;
    }
    out[o] = '\0';
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '.')) out[--o] = '\0';
    size_t start = 0;
    while (out[start] == ' ' || out[start] == '.') start++;
    if (start > 0) memmove(out, out + start, strlen(out + start) + 1);
    if (out[0] == '\0') SDL_snprintf(out, size, "Save %d", slot);
}

void pc_saves_file_name(const char* display, int slot, char* out, size_t size) {
    char base[400];
    clean_name(display, slot, base, sizeof(base));
    SDL_snprintf(out, size, "%s - Colosseum.gci", base);
}

/* ---------------------------------------------------------------- slots and their setup files */
const char* pc_saves_slot_dir(int slot, char* out, size_t size) {
    SDL_snprintf(out, size, "%sSlot %d/", s_saves, slot);
    return out;
}

typedef struct {
    char name[160];
    char came_from[64];
    int is_new;
    int order;
    int version;
    char undo[64];    /* the version folder Undo puts back ("" = no Undo offered) */
    char undo_to[64]; /* the version folder the save went back to (the notice's date) */
    char spot[200];   /* the exact spot a PC MENU save was made at ("" = none; pc_saves_spot_get) */
    /* GameCube-accurate mode for this save (2.12): 1 / 0, or -1 when the file has no line yet (a slot.ini from before
     * 2.12: pc_saves_init gives it a value once, the migration; a new setup made in memory gets one before it's written) */
    int accurate;
    /* 3.5 step 7 (2026-11-19; the project plan's importer rules and snatcherLeo's call 1 of 2026-11-19): a European or Japanese save
     * brought in keeps a record of where it came from, for the GameCube copy back to its region (step 9). region: the
     * original game code ("GC6P", "GC6J"; "" = a US save or one made here); region_header: the file in the slot folder
     * that holds the original .gci's first 0x6040 bytes (its 0x40-byte directory entry and the 0x6000-byte card header:
     * the comment, banner and icon in that region's language), "" when none */
    char region[8];
    char region_header[64];
} SlotSetup;

static int slot_has_extras(int slot); /* below, with accurate mode's calls */

/* a setup made from nothing (memset 0 would read as "accurate off" already decided): no accurate value yet */
static void blank_setup(SlotSetup* s) {
    memset(s, 0, sizeof(*s));
    s->accurate = -1;
}

static int read_setup(int slot, SlotSetup* s) {
    char path[1300], dir[1200];
    size_t len = 0;
    blank_setup(s);
    SDL_snprintf(s->name, sizeof(s->name), "Save %d", slot);
    s->order = slot;
    SDL_snprintf(path, sizeof(path), "%sslot.ini", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    char* text = SDL_LoadFile(path, &len);
    if (text == NULL) return 0;
    for (char* line = text; line != NULL && *line != '\0';) {
        char* end = strchr(line, '\n');
        if (end != NULL) *end = '\0';
        char* cr = strchr(line, '\r');
        if (cr != NULL) *cr = '\0';
        char* eq = strchr(line, '=');
        if (eq != NULL) {
            *eq = '\0';
            const char* v = eq + 1;
            if (strcmp(line, "name") == 0) SDL_snprintf(s->name, sizeof(s->name), "%s", v);
            else if (strcmp(line, "came_from") == 0) SDL_snprintf(s->came_from, sizeof(s->came_from), "%s", v);
            else if (strcmp(line, "new") == 0) s->is_new = atoi(v);
            else if (strcmp(line, "order") == 0) s->order = atoi(v);
            else if (strcmp(line, "version") == 0) s->version = atoi(v);
            else if (strcmp(line, "undo") == 0) SDL_snprintf(s->undo, sizeof(s->undo), "%s", v);
            else if (strcmp(line, "undo_to") == 0) SDL_snprintf(s->undo_to, sizeof(s->undo_to), "%s", v);
            else if (strcmp(line, "spot") == 0) SDL_snprintf(s->spot, sizeof(s->spot), "%s", v);
            else if (strcmp(line, "gamecube_accurate") == 0) s->accurate = atoi(v) != 0;
            else if (strcmp(line, "region") == 0) SDL_snprintf(s->region, sizeof(s->region), "%s", v);
            else if (strcmp(line, "region_header") == 0) SDL_snprintf(s->region_header, sizeof(s->region_header), "%s", v);
        }
        line = end != NULL ? end + 1 : NULL;
    }
    SDL_free(text);
    return 1;
}

static int write_setup(int slot, const SlotSetup* s) {
    char path[1300], dir[1200], text[1024];
    SDL_snprintf(path, sizeof(path), "%sslot.ini", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    int n = SDL_snprintf(text, sizeof(text),
                         "# OrrePort's setup file for this save (written by OrrePort; marks the folder as a registered slot)\n"
                         "version=%d\nname=%s\ncame_from=%s\nnew=%d\norder=%d\n",
                         PC_SAVES_FORMAT_VERSION, s->name, s->came_from, s->is_new, s->order);
    if (s->undo[0] != '\0' && n > 0 && (size_t)n < sizeof(text)) { /* only while an Undo is offered */
        n += SDL_snprintf(text + n, sizeof(text) - (size_t)n, "undo=%s\nundo_to=%s\n", s->undo, s->undo_to);
    }
    if (s->spot[0] != '\0' && n > 0 && (size_t)n < sizeof(text)) { /* only while a PC MENU save's spot is kept */
        n += SDL_snprintf(text + n, sizeof(text) - (size_t)n, "spot=%s\n", s->spot);
    }
    if (s->accurate >= 0 && n > 0 && (size_t)n < sizeof(text)) { /* once decided (2.12); never dropped once written */
        n += SDL_snprintf(text + n, sizeof(text) - (size_t)n, "gamecube_accurate=%d\n", s->accurate);
    }
    /* only for a save imported from another region (3.5 step 7); never dropped once written: the GameCube copy needs it */
    if (s->region[0] != '\0' && n > 0 && (size_t)n < sizeof(text)) {
        n += SDL_snprintf(text + n, sizeof(text) - (size_t)n, "region=%s\n", s->region);
    }
    if (s->region_header[0] != '\0' && n > 0 && (size_t)n < sizeof(text)) {
        n += SDL_snprintf(text + n, sizeof(text) - (size_t)n, "region_header=%s\n", s->region_header);
    }
    if (n < 0 || (size_t)n >= sizeof(text)) n = (int)strlen(text);
    return SDL_SaveFile(path, text, (size_t)n);
}

/* The exact spot a save made through the pause menu's PC MENU was made at (Access PC menu anywhere; snatcherLeo, 2026-11-15:
 * "A save from that PC MENU you lcick should load yo in teh exact spot you saved"; pc_field_menu.cpp). Kept in slot.ini,
 * not in the extras file: a slot with an extras file is an .orresave, and the save must stay a plain .gci (snatcherLeo,
 * 2026-11-13: "the save stays a .gci", 2026-11-15). The value starts with the .gci's SHA-1, so any other save in
 * the slot (a real PC, Dolphin, Restore, Undo) makes it stale and it is ignored. It never travels: an exported or
 * copied save loads at the last Pokémon Center, as on a console. Platforms: plain SDL file calls, the same everywhere */
int pc_saves_spot_get(int slot, char* out, size_t size) {
    SlotSetup s;
    out[0] = '\0';
    if (!read_setup(slot, &s)) return 0;
    SDL_snprintf(out, size, "%s", s.spot);
    return out[0] != '\0';
}

int pc_saves_spot_set(int slot, const char* value) {
    SlotSetup s;
    if (!read_setup(slot, &s)) return 0;
    if (strcmp(s.spot, value != NULL ? value : "") == 0) return 1;
    SDL_snprintf(s.spot, sizeof(s.spot), "%s", value != NULL ? value : "");
    return write_setup(slot, &s);
}

/* the slot's save file (the newest .gci in its folder; one per folder), or "" */
static int slot_save_file(int slot, char* out, size_t size) {
    char dir[1200];
    int count = 0;
    pc_saves_slot_dir(slot, dir, sizeof(dir));
    out[0] = '\0';
    char** files = SDL_GlobDirectory(dir, "*.gci", SDL_GLOB_CASEINSENSITIVE, &count);
    Sint64 newest = -1;
    for (int i = 0; files != NULL && i < count; i++) {
        char path[1500];
        SDL_PathInfo info;
        SDL_snprintf(path, sizeof(path), "%s%s", dir, files[i]);
        if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE && (Sint64)info.modify_time > newest) {
            newest = (Sint64)info.modify_time;
            SDL_snprintf(out, size, "%s", path);
        }
    }
    SDL_free(files);
    return out[0] != '\0';
}

/* the slot's save file as a SHA-1 in hex (40 characters and a 0), for the extras file (2.4: pc_names.cpp records which
 * .gci its names belong to, so a save changed elsewhere, in Dolphin say, is noticed instead of mixed). 0 when the slot
 * has no save file */
/* any bytes' SHA-1 as hex (2026-11-15: the test log that proves Main Menu's re-read leaves nothing unsaved, by the
 * save image's hash; pc_pc_screen.cpp). All platforms */
void pc_saves_sha1_hex(const void* data, size_t len, char hex[41]) {
    uint8_t d[20];
    sha1((const uint8_t*)data, len, d);
    for (int i = 0; i < 20; i++) SDL_snprintf(hex + i * 2, 3, "%02x", d[i]);
}

int pc_saves_save_digest(int slot, char hex[41]) {
    char path[1500];
    size_t len = 0;
    hex[0] = '\0';
    if (!slot_save_file(slot, path, sizeof(path))) return 0;
    uint8_t* data = SDL_LoadFile(path, &len);
    if (data == NULL) return 0;
    uint8_t d[20];
    sha1(data, len, d);
    SDL_free(data);
    for (int i = 0; i < 20; i++) SDL_snprintf(hex + i * 2, 3, "%02x", d[i]);
    return 1;
}

/* every "Slot N" folder's number */
static int slot_numbers(int* out, int max) {
    int count = 0, n = 0;
    char** dirs = SDL_GlobDirectory(s_saves, "Slot *", 0, &count);
    for (int i = 0; dirs != NULL && i < count && n < max; i++) {
        char path[1300];
        SDL_PathInfo info;
        const int slot = atoi(dirs[i] + 5);
        SDL_snprintf(path, sizeof(path), "%s%s", s_saves, dirs[i]);
        if (slot > 0 && slot < MAX_SLOTS && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
            out[n++] = slot;
        }
    }
    SDL_free(dirs);
    return n;
}

static int highest_slot(void) {
    int slots[MAX_SLOTS], best = 0;
    const int n = slot_numbers(slots, MAX_SLOTS);
    for (int i = 0; i < n; i++) if (slots[i] > best) best = slots[i];
    return best;
}

static int highest_order(void) {
    int slots[MAX_SLOTS], best = 0;
    const int n = slot_numbers(slots, MAX_SLOTS);
    for (int i = 0; i < n; i++) {
        SlotSetup s;
        read_setup(slots[i], &s);
        if (s.order > best) best = s.order;
    }
    return best;
}

static int new_slot(void) {
    char dir[1200];
    const int slot = highest_slot() + 1;
    SDL_CreateDirectory(pc_saves_slot_dir(slot, dir, sizeof(dir)));
    return slot;
}

/* ---------------------------------------------------------------- previous versions */
static void now_stamp(char* out, size_t size) {
    SDL_Time now;
    SDL_DateTime dt;
    SDL_zero(dt);
    if (SDL_GetCurrentTime(&now)) SDL_TimeToDateTime(now, &dt, true);
    SDL_snprintf(out, size, "%04d%02d%02d_%02d%02d%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

static int file_hash(const char* path, uint8_t out[20]) {
    size_t len = 0;
    uint8_t* data = SDL_LoadFile(path, &len);
    if (data == NULL) return 0;
    sha1(data, len, out);
    SDL_free(data);
    return 1;
}

static int version_cmp(const void* a, const void* b) { return strcmp(*(char* const*)b, *(char* const*)a); }

/* the newest version folder of a slot ("" if none) */
static int newest_version(int slot, char* out, size_t size) {
    char dir[1200], bk[1300];
    int count = 0;
    out[0] = '\0';
    SDL_snprintf(bk, sizeof(bk), "%sBackups/", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    char** names = SDL_GlobDirectory(bk, "*", 0, &count);
    if (names != NULL && count > 0) {
        qsort(names, (size_t)count, sizeof(char*), version_cmp);
        SDL_snprintf(out, size, "%s%s/", bk, names[0]);
    }
    SDL_free(names);
    return out[0] != '\0';
}

/* the last folder name of "...\Backups\<name>\" */
static void base_of(const char* folder, char* out, size_t size) {
    char tmp[1500];
    SDL_snprintf(tmp, sizeof(tmp), "%s", folder);
    size_t n = strlen(tmp);
    while (n > 0 && (tmp[n - 1] == '/' || tmp[n - 1] == '\\')) tmp[--n] = '\0';
    const char* a = strrchr(tmp, '/');
    const char* b = strrchr(tmp, '\\');
    if (b != NULL && (a == NULL || b > a)) a = b;
    SDL_snprintf(out, size, "%s", a != NULL ? a + 1 : tmp);
}

/* removes one version folder: its files (the .gci and why.txt; nothing else is ever put there), then the folder */
static void remove_version(const char* folder) {
    int count = 0;
    char** files = SDL_GlobDirectory(folder, "*", 0, &count);
    for (int i = 0; files != NULL && i < count; i++) {
        char path[1700];
        SDL_snprintf(path, sizeof(path), "%s%s", folder, files[i]);
        SDL_RemovePath(path);
    }
    SDL_free(files);
    SDL_RemovePath(folder);
}

/* Keeps the newest KEEP_VERSIONS versions of a slot, removing the oldest past that, never the one a pending Undo
 * needs (snatcherLeo, 2026-11-13). Folder names are date stamps, so name order is age order. */
static void prune_versions(int slot) {
    char dir[1200], bk[1300];
    int count = 0;
    SlotSetup s;
    read_setup(slot, &s);
    SDL_snprintf(bk, sizeof(bk), "%sBackups/", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    char** names = SDL_GlobDirectory(bk, "*", 0, &count);
    if (names != NULL && count > KEEP_VERSIONS) {
        qsort(names, (size_t)count, sizeof(char*), version_cmp); /* newest first */
        for (int i = KEEP_VERSIONS; i < count; i++) {
            if (s.undo[0] != '\0' && strcmp(names[i], s.undo) == 0) continue;
            char folder[1500];
            SDL_snprintf(folder, sizeof(folder), "%s%s/", bk, names[i]);
            remove_version(folder);
            printf("[saves] slot %d: oldest version removed (%s; %d kept)\n", slot, names[i], KEEP_VERSIONS);
        }
    }
    SDL_free(names);
}

/* Keeps the slot's save as a version when it differs from the newest kept one (or always, force). The kept (or the
 * identical newest) folder in *folder. why: launch / before_save / before_restore / before_import. */
/* 2.4 (2026-11-14): a save's extras file (`extras.ini`: what a GameCube save can't hold, written by
 * pc_names.cpp) belongs to its .gci, so it travels with it: kept with each version, brought back by going back and
 * Undo, copied by Duplicate (decided: "backups and Restore keep the .gci and the extras together"). from / to are
 * folders ending in '/'. When `from` has none, `to`'s is removed if `remove_missing` (the save gone back to had no
 * long names, so neither does the slot now). */
static void carry_extras(const char* from, const char* to, int remove_missing) {
    char src[1500], dst[1500];
    SDL_snprintf(src, sizeof(src), "%sextras.ini", from);
    SDL_snprintf(dst, sizeof(dst), "%sextras.ini", to);
    if (SDL_GetPathInfo(src, NULL)) {
        if (!SDL_CopyFile(src, dst)) fprintf(stderr, "[saves] extras.ini not copied to %s: %s\n", to, SDL_GetError());
    } else if (remove_missing && SDL_GetPathInfo(dst, NULL)) {
        SDL_RemovePath(dst);
    }
}

static int keep_version(int slot, const char* why, int force, char* folder, size_t folder_size) {
    char save[1300], last[1300], stamp[32], dir[1200], dest[1400], file[1500];
    uint8_t now_hash[20], old_hash[20];
    if (folder != NULL && folder_size > 0) folder[0] = '\0';
    if (!slot_save_file(slot, save, sizeof(save)) || !file_hash(save, now_hash)) return 0;
    if (!force && newest_version(slot, last, sizeof(last))) {
        int count = 0;
        char** files = SDL_GlobDirectory(last, "*.gci", SDL_GLOB_CASEINSENSITIVE, &count);
        if (files != NULL && count > 0) {
            char lastFile[1500];
            SDL_snprintf(lastFile, sizeof(lastFile), "%s%s", last, files[0]);
            /* unchanged means the save and its extras file (2.4): a version without the slot's long names isn't the
             * save as it is now (snatcherLeo's hand check, 2026-11-14: going back reused a version made before versions
             * carried extras.ini, and Undo then lost the full nickname) */
            char ex_now[1300], ex_old[1300], sdir[1200];
            uint8_t hx_now[20], hx_old[20];
            SDL_snprintf(ex_now, sizeof(ex_now), "%sextras.ini", pc_saves_slot_dir(slot, sdir, sizeof(sdir)));
            SDL_snprintf(ex_old, sizeof(ex_old), "%sextras.ini", last);
            const int has_now = file_hash(ex_now, hx_now), has_old = file_hash(ex_old, hx_old);
            const int same_extras = has_now == has_old && (!has_now || memcmp(hx_now, hx_old, 20) == 0);
            if (same_extras && file_hash(lastFile, old_hash) && memcmp(now_hash, old_hash, 20) == 0) {
                SDL_free(files);
                if (folder != NULL) SDL_snprintf(folder, folder_size, "%s", last);
                return 1; /* unchanged since the newest version: nothing new to keep */
            }
        }
        SDL_free(files);
    }
    now_stamp(stamp, sizeof(stamp));
    SDL_snprintf(dest, sizeof(dest), "%sBackups/%s/", pc_saves_slot_dir(slot, dir, sizeof(dir)), stamp);
    for (int k = 2; SDL_GetPathInfo(dest, NULL) && k < 100; k++) {
        SDL_snprintf(dest, sizeof(dest), "%sBackups/%s_%d/", dir, stamp, k);
    }
    SDL_CreateDirectory(dest);
    const char* base = strrchr(save, '/');
    SDL_snprintf(file, sizeof(file), "%s%s", dest, base != NULL ? base + 1 : save);
    if (!SDL_CopyFile(save, file)) {
        fprintf(stderr, "[saves] could not keep a version of slot %d: %s\n", slot, SDL_GetError());
        return 0;
    }
    carry_extras(dir, dest, 0); /* the save's extras with it (2.4) */
    {
        char why_path[1500];
        SDL_snprintf(why_path, sizeof(why_path), "%swhy.txt", dest);
        SDL_SaveFile(why_path, why, strlen(why));
    }
    printf("[saves] slot %d: version kept (%s) in %s\n", slot, why, dest);
    if (folder != NULL) SDL_snprintf(folder, folder_size, "%s", dest);
    prune_versions(slot);
    return 1;
}

/* the card's before-write hook: the save as it was before this in-game save ("previous version" = the previous
 * in-game save). Only a readable save is kept (a brand-new empty file has nothing to go back to). */
static void before_write(const char* path) {
    char slotdir[1200];
    const int slot = pc_saves_current();
    pc_saves_slot_dir(slot, slotdir, sizeof(slotdir));
    if (strncmp(path, slotdir, strlen(slotdir) - 1) != 0 && strstr(path, "Slot ") == NULL) return;
    if (!pc_saves_read_info(path, NULL, 0, NULL)) return;
    keep_version(slot, "before_save", 0, NULL, 0);
}

static int version_list_cmp(const void* a, const void* b) {
    return strcmp(((const PcSaveVersion*)b)->when, ((const PcSaveVersion*)a)->when);
}

int pc_saves_versions(int slot, PcSaveVersion* out, int max) {
    char dir[1200], bk[1300], now_file[1300];
    int count = 0, n = 0;
    uint8_t now_hash[20];
    SlotSetup s;
    /* the save as it is now, to mark versions identical to it (going back to one would change nothing) */
    const int have_now = slot_save_file(slot, now_file, sizeof(now_file)) && file_hash(now_file, now_hash);
    read_setup(slot, &s);
    SDL_snprintf(bk, sizeof(bk), "%sBackups/", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    char** names = SDL_GlobDirectory(bk, "*", 0, &count);
    for (int i = 0; names != NULL && i < count && n < max; i++) {
        PcSaveVersion* v = &out[n];
        char whyPath[1500], save[1500];
        int files = 0;
        memset(v, 0, sizeof(*v));
        SDL_snprintf(v->folder, sizeof(v->folder), "%s%s/", bk, names[i]);
        SDL_snprintf(v->when, sizeof(v->when), "%s", names[i]);
        SDL_snprintf(whyPath, sizeof(whyPath), "%swhy.txt", v->folder);
        size_t len = 0;
        char* why = SDL_LoadFile(whyPath, &len);
        SDL_snprintf(v->why, sizeof(v->why), "%s", why != NULL ? why : "launch");
        SDL_free(why);
        char** gci = SDL_GlobDirectory(v->folder, "*.gci", SDL_GLOB_CASEINSENSITIVE, &files);
        if (gci != NULL && files > 0) {
            uint8_t h[20];
            SDL_snprintf(save, sizeof(save), "%s%s", v->folder, gci[0]);
            pc_saves_read_info(save, v->trainer, sizeof(v->trainer), &v->play_seconds);
            v->same_as_now = have_now && file_hash(save, h) && memcmp(h, now_hash, 20) == 0;
            v->is_undo = s.undo[0] != '\0' && strcmp(names[i], s.undo) == 0;
            n++;
        }
        SDL_free(gci);
    }
    SDL_free(names);
    qsort(out, (size_t)n, sizeof(PcSaveVersion), version_list_cmp);
    return n;
}

int pc_saves_restore(int slot, const char* version_folder) {
    char save[1300], from[1500];
    int files = 0;
    char** gci = SDL_GlobDirectory(version_folder, "*.gci", SDL_GLOB_CASEINSENSITIVE, &files);
    if (gci == NULL || files == 0) { SDL_free(gci); return 0; }
    SDL_snprintf(from, sizeof(from), "%s%s", version_folder, gci[0]);
    SDL_free(gci);
    /* the current save first, so the restore can be undone (the kept copy, or the identical newest one) */
    char kept[1400];
    if (!keep_version(slot, "before_restore", 0, kept, sizeof(kept))) kept[0] = '\0';
    SlotSetup s;
    read_setup(slot, &s);
    if (!slot_save_file(slot, save, sizeof(save))) {
        char dir[1200], name[400];
        pc_saves_file_name(s.name, slot, name, sizeof(name));
        SDL_snprintf(save, sizeof(save), "%s%s", pc_saves_slot_dir(slot, dir, sizeof(dir)), name);
    }
    const int ok = SDL_CopyFile(from, save); /* the slot keeps its current file name */
    printf("[saves] slot %d restored from %s: %s\n", slot, version_folder, ok ? "ok" : SDL_GetError());
    if (ok) {
        char sdir[1200];
        carry_extras(version_folder, pc_saves_slot_dir(slot, sdir, sizeof(sdir)), 1); /* that version's names (2.4) */
    }
    if (ok) { /* Undo is offered until the save is played or restored again (mockup 74) */
        if (kept[0] != '\0') base_of(kept, s.undo, sizeof(s.undo));
        else s.undo[0] = '\0';
        base_of(version_folder, s.undo_to, sizeof(s.undo_to));
        write_setup(slot, &s);
    }
    return ok;
}

/* Rules R4 / R7 (snatcherLeo approved, 2026-11-14): the played save's file (R4) or its whole slot folder (R7) was deleted
 * while the game ran; just before the game saves, put a file back so the game's save finds it and lands on top (the
 * game itself would say "Save failed!": verified in a the reference playthrough replay). The source: `bytes` (the file as OrrePort opened
 * it this session, kept in memory by pc_card.c; the only source left when the whole folder, versions included, went),
 * else the slot's newest kept version. Either is an older copy of this save; the game's save then writes the current
 * progress into it (the newest of its three save slots wins on load). A quiet put-back, not a Restore: no Undo is
 * armed. Returns 1 when a file was put back. Platforms: SDL's file calls, all. */
int pc_saves_put_back(int slot, const void* bytes, size_t len) {
    char save[1300], from[1500], dir[1200], name[400];
    PcSaveVersion v[1];
    int files = 0, ok = 0;
    if (slot_save_file(slot, save, sizeof(save))) return 0; /* the file is there: nothing to do */
    SlotSetup s;
    const int had_setup = read_setup(slot, &s);
    SDL_CreateDirectory(pc_saves_slot_dir(slot, dir, sizeof(dir))); /* R7: the folder may be gone */
    if (!had_setup) write_setup(slot, &s);                         /* and its setup file with it */
    pc_saves_file_name(s.name, slot, name, sizeof(name));
    SDL_snprintf(save, sizeof(save), "%s%s", dir, name);
    if (bytes != NULL && len > 0) {
        ok = SDL_SaveFile(save, bytes, len);
        printf("[saves] slot %d: its save file was gone; put back from the copy opened this session: %s\n", slot,
               ok ? "ok" : SDL_GetError());
        if (ok) return 1;
    }
    if (pc_saves_versions(slot, v, 1) <= 0) return 0; /* nothing kept: a new slot (nothing was lost) */
    char** gci = SDL_GlobDirectory(v[0].folder, "*.gci", SDL_GLOB_CASEINSENSITIVE, &files);
    if (gci == NULL || files == 0) { SDL_free(gci); return 0; }
    SDL_snprintf(from, sizeof(from), "%s%s", v[0].folder, gci[0]);
    SDL_free(gci);
    ok = SDL_CopyFile(from, save);
    printf("[saves] slot %d: its save file was gone; put back from its version %s: %s\n", slot, v[0].when,
           ok ? "ok" : SDL_GetError());
    return ok;
}

int pc_saves_undo_info(int slot, char* gone_back_to, size_t size, unsigned* play_seconds) {
    SlotSetup s;
    char dir[1200], folder[1500];
    if (gone_back_to != NULL && size > 0) gone_back_to[0] = '\0';
    if (!read_setup(slot, &s) || s.undo[0] == '\0') return 0;
    SDL_snprintf(folder, sizeof(folder), "%sBackups/%s/", pc_saves_slot_dir(slot, dir, sizeof(dir)), s.undo);
    if (!SDL_GetPathInfo(folder, NULL)) return 0; /* its copy is gone (removed by hand): nothing to undo to */
    if (gone_back_to != NULL && size > 0) SDL_snprintf(gone_back_to, size, "%s", s.undo_to);
    if (play_seconds != NULL) {
        char save[1300];
        *play_seconds = 0;
        if (slot_save_file(slot, save, sizeof(save))) pc_saves_read_info(save, NULL, 0, play_seconds);
    }
    return 1;
}

/* no Undo any more (the save was played, or the Undo was used) */
static void clear_undo(int slot) {
    SlotSetup s;
    if (read_setup(slot, &s) && s.undo[0] != '\0') {
        s.undo[0] = '\0';
        s.undo_to[0] = '\0';
        write_setup(slot, &s);
    }
}

int pc_saves_undo(int slot) {
    SlotSetup s;
    char dir[1200], folder[1500], from[1700], save[1300];
    int files = 0;
    if (!read_setup(slot, &s) || s.undo[0] == '\0') return 0;
    SDL_snprintf(folder, sizeof(folder), "%sBackups/%s/", pc_saves_slot_dir(slot, dir, sizeof(dir)), s.undo);
    char** gci = SDL_GlobDirectory(folder, "*.gci", SDL_GLOB_CASEINSENSITIVE, &files);
    if (gci == NULL || files == 0) { SDL_free(gci); return 0; }
    SDL_snprintf(from, sizeof(from), "%s%s", folder, gci[0]);
    SDL_free(gci);
    /* no copy of the restored save is kept first: it is still a version itself (the one gone back to) */
    if (!slot_save_file(slot, save, sizeof(save))) {
        char name[400];
        pc_saves_file_name(s.name, slot, name, sizeof(name));
        SDL_snprintf(save, sizeof(save), "%s%s", dir, name);
    }
    const int ok = SDL_CopyFile(from, save);
    printf("[saves] slot %d: restore undone from %s: %s\n", slot, folder, ok ? "ok" : SDL_GetError());
    if (ok) carry_extras(folder, dir, 1); /* the names that save had (2.4) */
    if (ok) clear_undo(slot);
    return ok;
}

/* ---------------------------------------------------------------- the current slot */
int pc_saves_current(void) {
    const int slot = pc_settings_int("save_slot", 1);
    return slot > 0 && slot < MAX_SLOTS ? slot : 1;
}

void pc_saves_set_current(int slot) {
    pc_settings_set_int("save_slot", slot);
    pc_settings_save();
}

int pc_saves_ready(void) { return s_saves[0] != '\0'; }

/* ---------------------------------------------------------------- listing */
/* A game file the game created but never saved into (its "create a new save file" step, at start-up or a pick in the
 * main menu's LOAD on an empty slot): all three save slots blank, their check digests all zero (verified 2026-11-14 on
 * such a file: each slot zero but a 4-byte marker at its start, 01 01 .. FE FF). It reads as no save, but it isn't
 * damaged: the SAVE FILES list shows it as empty (rule R2). Platforms: all. */
static int gci_never_saved(const char* path) {
    size_t len = 0;
    uint8_t* data = SDL_LoadFile(path, &len);
    int blank = data != NULL && len >= GCI_HEADER + SAVE_SLOT0 + 3 * (size_t)SAVE_SLOT;
    for (int n = 0; blank && n < 3; n++) {
        const uint8_t* digest = data + GCI_HEADER + SAVE_SLOT0 + (size_t)n * SAVE_SLOT + DIGEST_AT;
        for (int i = 0; i < 20; i++) blank = blank && digest[i] == 0;
    }
    SDL_free(data);
    return blank;
}

static void fill_info(int slot, PcSaveInfo* info) {
    SlotSetup s;
    SDL_PathInfo pi;
    memset(info, 0, sizeof(*info));
    read_setup(slot, &s);
    info->slot = slot;
    SDL_snprintf(info->name, sizeof(info->name), "%s", s.name);
    SDL_snprintf(info->came_from, sizeof(info->came_from), "%s", s.came_from);
    info->is_new = s.is_new;
    info->order = s.order;
    info->accurate = s.accurate >= 0 ? s.accurate : pc_saves_accurate_get(slot); /* 2.12 */
    SDL_snprintf(info->imported_region, sizeof(info->imported_region), "%s", s.region); /* 3.5 step 7 */
    if (slot_save_file(slot, info->file, sizeof(info->file))) {
        SaveFacts f;
        /* the same remembered read as pc_saves_read_info, also giving the region stamp (3.5 step 6) */
        info->has_save = facts_of(info->file, &f, NULL, NULL) && f.code == PC_SAVE_IMPORTED;
        if (info->has_save) {
            SDL_snprintf(info->trainer, sizeof(info->trainer), "%s", f.trainer);
            info->play_seconds = f.play_seconds;
            info->region = f.area;
            info->language = f.language;
        }
        info->damaged = !info->has_save && !gci_never_saved(info->file); /* rule R2 */
        if (SDL_GetPathInfo(info->file, &pi)) info->saved_time = (long long)(pi.modify_time / 1000000000LL);
    }
    /* 2.4 (decided): a slot is an .orresave exactly when it has an extras file (pc_names.cpp writes it only while the
     * save holds something a GameCube can't: in 0.1, a name longer than its GameCube version). The save's typed name
     * never decides it, only its contents */
    {
        char dir[1200], extras[1300];
        SDL_snprintf(extras, sizeof(extras), "%sextras.ini", pc_saves_slot_dir(slot, dir, sizeof(dir)));
        info->is_orresave = SDL_GetPathInfo(extras, NULL) ? 1 : 0;
    }
    /* the trainer as the player named them (snatcherLeo, 2026-11-14: the hub should show the long name, not the GameCube
     * one): the extras file's full name when the save keeps one; the .gci holds only its GameCube version */
    /* *Changed 2026-11-16 (visible letters, pc_grapheme.h):* the list is taken from the heap: a PcLongName now holds a
     * whole 12-letter name (up to 529 bytes), so 48 of them are about 34 KB, too much for the stack of a game callback
     * (the title's LOAD SAVE screen reaches here, pc_title_loadsave.cpp). All OSes the same. */
    if (info->is_orresave) {
        PcLongName* names = (PcLongName*)SDL_malloc(sizeof(PcLongName) * 48);
        const int n = names != NULL ? pc_names_read_slot(slot, names, 48) : 0;
        for (int i = 0; i < n; i++) {
            if (strcmp(names[i].id, "hero") == 0) SDL_snprintf(info->trainer, sizeof(info->trainer), "%s", names[i].full);
        }
        SDL_free(names);
    }
}

static int info_order_cmp(const void* a, const void* b) {
    const PcSaveInfo* x = a;
    const PcSaveInfo* y = b;
    return x->order != y->order ? (x->order < y->order ? -1 : 1) : (x->slot < y->slot ? -1 : 1);
}

int pc_saves_list(PcSaveInfo* out, int max) {
    int slots[MAX_SLOTS];
    const int n = slot_numbers(slots, MAX_SLOTS);
    int m = 0;
    for (int i = 0; i < n && m < max; i++) fill_info(slots[i], &out[m++]);
    qsort(out, (size_t)m, sizeof(PcSaveInfo), info_order_cmp);
    return m;
}

int pc_saves_get(int slot, PcSaveInfo* out) {
    char dir[1200];
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(pc_saves_slot_dir(slot, dir, sizeof(dir)), &info) || info.type != SDL_PATHTYPE_DIRECTORY) return 0;
    fill_info(slot, out);
    return 1;
}

void pc_saves_mark_seen(int slot) {
    SlotSetup s;
    if (read_setup(slot, &s) && s.is_new) {
        s.is_new = 0;
        write_setup(slot, &s);
    }
}

/* ---------------------------------------------------------------- import */
/* ---------------------------------------------------------------- the one-file .orresave (2.4 part 5)
 * Decided (snatcherLeo approved, 2026-11-15, the project's notes): an .orresave travels as one zip with fixed names inside:
 *   manifest.ini  version=1, game=Colosseum (one extension for every game: the file says which), the OrrePort that
 *                 wrote it, and gci=<the save's SHA-1>;
 *   save.gci      the save as the slot holds it (always with GameCube-safe names);
 *   extras.ini    its long names (pc_names.cpp), whose own gci= fingerprint must match save.gci;
 *   settings.ini  the save's display name, and mods= (empty in 0.1, so the format is ready for them).
 * Not the slot's local history (where it came from, Undo, the page's order). The save's region and language stamps
 * are inside the .gci and never touched. Platforms: plain C; the same everywhere. XD: the same file, game=XD. */
static const char* ini_value(const char* text, size_t len, const char* key, char* out, size_t size) {
    const size_t kl = strlen(key);
    out[0] = '\0';
    for (size_t at = 0; at < len;) {
        size_t end = at;
        while (end < len && text[end] != '\n') end++;
        if (end - at > kl && strncmp(text + at, key, kl) == 0 && text[at + kl] == '=') {
            size_t n = end - at - kl - 1;
            if (n > 0 && text[at + kl + 1 + n - 1] == '\r') n--;
            if (n >= size) n = size - 1;
            memcpy(out, text + at + kl + 1, n);
            out[n] = '\0';
            return out;
        }
        at = end + 1;
    }
    return NULL;
}

/* the save, long names and display name inside an .orresave. A file whose long names don't belong to the save inside
 * (tampered or badly made) is refused (PC_SAVE_ERR_TAMPERED) unless save_only: "Import just the save", as a plain .gci
 * save, so the progress isn't lost (the project's recommendation (d), approved) */
static int read_orresave(const uint8_t* zip, size_t len, int save_only, uint8_t** gci, size_t* gci_len,
                         const char** extras, size_t* extras_len, char* name, size_t name_size) {
    size_t ml = 0, sl = 0, el = 0, tl = 0;
    const char* manifest = (const char*)pc_zip_find(zip, len, "manifest.ini", &ml);
    const uint8_t* save = (const uint8_t*)pc_zip_find(zip, len, "save.gci", &sl);
    const char* ex = (const char*)pc_zip_find(zip, len, "extras.ini", &el);
    const char* settings = (const char*)pc_zip_find(zip, len, "settings.ini", &tl);
    char v[80];
    if (manifest == NULL || save == NULL) return PC_SAVE_ERR_NOT_SAVE;
    if (ini_value(manifest, ml, "game", v, sizeof(v)) != NULL && strcmp(v, "Colosseum") != 0) {
        return strcmp(v, "XD") == 0 ? PC_SAVE_ERR_XD : PC_SAVE_ERR_OTHER_GAME;
    }
    *gci = (uint8_t*)save;
    *gci_len = sl;
    if (ex != NULL && !save_only) {
        uint8_t d[20];
        char hex[41];
        sha1(save, sl, d);
        for (int i = 0; i < 20; i++) SDL_snprintf(hex + i * 2, 3, "%02x", d[i]);
        if (ini_value(ex, el, "gci", v, sizeof(v)) == NULL || strcmp(v, hex) != 0) return PC_SAVE_ERR_TAMPERED;
        *extras = ex;
        *extras_len = el;
    }
    if (settings != NULL && !save_only) ini_value(settings, tl, "name", name, name_size);
    return PC_SAVE_IMPORTED;
}

int pc_saves_export_orresave(int slot, const char* file_path, char* out, size_t out_size) {
    char save[1300], dir[1200], ex_path[1300], path[1600], digest[41];
    SlotSetup s;
    if (out != NULL && out_size > 0) out[0] = '\0';
    if (file_path == NULL || file_path[0] == '\0' || !slot_save_file(slot, save, sizeof(save))) return 0;
    read_setup(slot, &s);
    size_t gl = 0, el = 0;
    uint8_t* gci = SDL_LoadFile(save, &gl);
    SDL_snprintf(ex_path, sizeof(ex_path), "%sextras.ini", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    char* ex = (char*)SDL_LoadFile(ex_path, &el);
    if (gci == NULL || ex == NULL) { /* only a save with long names travels as an .orresave (the project's (e), approved) */
        SDL_free(gci);
        SDL_free(ex);
        return 0;
    }
    pc_saves_save_digest(slot, digest);
    char manifest[400], settings[400];
    SDL_snprintf(manifest, sizeof(manifest),
                 "# An OrrePort save (one file for sharing and backups): docs/port/orresave.md\n"
                 "version=1\ngame=Colosseum\nwritten_by=OrrePort 0.1\ngci=%s\n", digest);
    SDL_snprintf(settings, sizeof(settings), "# The save's own settings\nname=%s\nmods=\n", s.name);
    const PcZipFile files[4] = {
        {"manifest.ini", manifest, strlen(manifest)},
        {"save.gci", gci, gl},
        {"extras.ini", ex, el},
        {"settings.ini", settings, strlen(settings)},
    };
    const size_t len = strlen(file_path);
    const int has_ext = len > 9 && SDL_strcasecmp(file_path + len - 9, ".orresave") == 0;
    SDL_snprintf(path, sizeof(path), "%s%s", file_path, has_ext ? "" : ".orresave");
    const int ok = pc_zip_write(path, files, 4);
    printf("[saves] slot %d exported as an .orresave to %s: %s\n", slot, path, ok ? "ok" : SDL_GetError());
    if (ok && out != NULL) SDL_snprintf(out, out_size, "%s", path);
    SDL_free(gci);
    SDL_free(ex);
    return ok;
}

int pc_saves_import(const char* path, int move, int force, int* new_slot_out, int* already_slot) {
    size_t len = 0, gci_len = 0;
    uint8_t* data = SDL_LoadFile(path, &len);
    uint8_t* gci = NULL;
    int code, is_raw = 0, gci_owned = 0;
    SaveFacts f;
    /* a one-file .orresave (2.4 part 5): its save, long names and display name, all from inside the zip */
    const char* extras = NULL;
    size_t extras_len = 0;
    char orre_name[160] = "";
    if (new_slot_out != NULL) *new_slot_out = 0;
    if (already_slot != NULL) *already_slot = 0;
    if (data == NULL) return PC_SAVE_ERR_READ;
    if (pc_zip_is_zip(data, len)) {
        const int why = read_orresave(data, len, force == 2, &gci, &gci_len, &extras, &extras_len, orre_name,
                                      sizeof(orre_name));
        if (why != PC_SAVE_IMPORTED) { SDL_free(data); return why; }
        is_raw = 1; /* like a memory card: the save is written out, and a loose file goes to Saves\Imported\ */
    } else
    /* a .gci starts with its header; a memory card image is a whole number of 8 KB blocks with a directory */
    if (len >= GCI_HEADER && (len - GCI_HEADER) % BLOCK == 0 && len == GCI_HEADER + (size_t)be16(data + 0x38) * BLOCK) {
        gci = data;
        gci_len = len;
    } else {
        int why = 0;
        gci = extract_from_raw(data, len, &gci_len, &why);
        if (gci == NULL) { SDL_free(data); return why; }
        is_raw = 1;
        gci_owned = 1;
    }
    read_gci_facts(gci, gci_len, &f);
    code = f.code;
    int match = 0; /* a slot already holding this very save (by its digest) */
    if (code == PC_SAVE_IMPORTED) { /* the same save already in a slot (the save's own digest) */
        int slots[MAX_SLOTS];
        const int n = slot_numbers(slots, MAX_SLOTS);
        for (int i = 0; i < n && match == 0; i++) {
            char save[1300];
            size_t l2 = 0;
            if (!slot_save_file(slots[i], save, sizeof(save))) continue;
            uint8_t* other = SDL_LoadFile(save, &l2);
            SaveFacts g;
            if (other == NULL) continue;
            read_gci_facts(other, l2, &g);
            SDL_free(other);
            /* an .orresave is the same save only with the same long names too (the project's draft, approved with part 5) */
            int same_extras = 1;
            if (extras != NULL) {
                char ex[1300], sd[1200];
                size_t el = 0;
                SDL_snprintf(ex, sizeof(ex), "%sextras.ini", pc_saves_slot_dir(slots[i], sd, sizeof(sd)));
                char* mine = (char*)SDL_LoadFile(ex, &el);
                same_extras = mine != NULL && el == extras_len && memcmp(mine, extras, el) == 0;
                SDL_free(mine);
            }
            if (same_extras && g.code == PC_SAVE_IMPORTED && memcmp(g.digest, f.digest, 20) == 0) {
                match = slots[i];
                if (already_slot != NULL) *already_slot = slots[i];
                if (!force) code = PC_SAVE_ERR_ALREADY;
            }
        }
    }
    if (code == PC_SAVE_IMPORTED) {
        const int order = highest_order() + 1; /* before the new folder exists, so it doesn't count itself */
        const int slot = new_slot();
        char dir[1200], name[400], dest[1700];
        SlotSetup s;
        blank_setup(&s);
        /* every import starts as "Imported save", renamed later (snatcherLeo, 2026-11-13: "we could just have a temporary
         * default name ... which users can change later on") */
        SDL_snprintf(s.name, sizeof(s.name), "%s", orre_name[0] != '\0' ? orre_name : "Imported save");
        SDL_snprintf(s.came_from, sizeof(s.came_from), "%s", "imported");
        s.is_new = 1;
        s.order = order;
        pc_saves_file_name(s.name, slot, name, sizeof(name));
        SDL_snprintf(dest, sizeof(dest), "%s%s", pc_saves_slot_dir(slot, dir, sizeof(dir)), name);
        /* A European or Japanese save (3.5 step 7; snatcherLeo, 2026-11-17, the importer's rules): the slot's copy gets the US
         * game code (bytes 0-3 only, pc_save_gci_make_us) so OrrePort's memory card (CARDInit "GC6E", "01") opens it;
         * its stamp, language bytes and Pokémon stay as they are. The original region goes into slot.ini (region=) and
         * the original first 0x6040 bytes (directory entry and card header: that region's comment, banner and icon)
         * into region_header.bin beside it, for the GameCube copy back to that region (step 9; snatcherLeo's call 1 of
         * 2026-11-19). The player's own file is never changed: the changed bytes are a copy. The slot digests and the
         * "already imported" match don't cover bytes 0-3, so matching keeps working. Every OS the same (SDL files). */
        uint8_t* stored = gci;
        const int foreign = f.region_letter != 'E';
        if (foreign) {
            stored = malloc(gci_len);
            if (stored != NULL) {
                memcpy(stored, gci, gci_len);
                pc_save_gci_make_us(stored, gci_len);
                SDL_snprintf(s.region, sizeof(s.region), "%.4s", (const char*)gci);
                SDL_snprintf(s.region_header, sizeof(s.region_header), "%s", "region_header.bin");
            }
        }
        int ok = stored != NULL;
        if (ok && foreign) {
            char hdr[1300];
            SDL_snprintf(hdr, sizeof(hdr), "%s%s", dir, s.region_header);
            ok = SDL_SaveFile(hdr, gci, GCI_HEADER + SAVE_SLOT0); /* the length was checked by read_gci_facts */
            if (!ok) fprintf(stderr, "[saves] the original card header could not be kept in %s: %s\n", hdr, SDL_GetError());
        }
        if (ok) {
            if (move && !is_raw && !foreign) ok = SDL_RenamePath(path, dest) || SDL_SaveFile(dest, gci, gci_len);
            else ok = SDL_SaveFile(dest, stored, gci_len);
        }
        if (stored != gci) free(stored);
        if (ok && write_setup(slot, &s)) {
            if (new_slot_out != NULL) *new_slot_out = slot;
            /* a GameCube copy of an .orresave brought back in (2.4; decided: "Never lost: a copy brought back into
             * OrrePort reads the full name again"): the save is that slot's own .gci byte for byte, so its long
             * names (extras.ini) apply as they are; the new save gets them */
            if (extras != NULL) { /* an .orresave: its own long names (2.4 part 5) */
                char ex[1300];
                SDL_snprintf(ex, sizeof(ex), "%sextras.ini", dir);
                SDL_SaveFile(ex, extras, extras_len);
            } else if (match != 0) {
                char from[1200];
                carry_extras(pc_saves_slot_dir(match, from, sizeof(from)), dir, 0);
            }
            /* a new save takes the new-saves default (2.12), once its extras are known: an .orresave stays normal, as
             * accurate mode plays .gci saves only (turning it on later offers a GameCube copy) */
            s.accurate = slot_has_extras(slot) ? 0 : pc_option_accurate_new_saves();
            write_setup(slot, &s);
            printf("[saves] imported %s into slot %d (%s, %u s played)%s\n", path, slot, f.trainer, f.play_seconds,
                   move ? " (moved: it was put into Saves\\)" : " (copied: the original is unchanged)");
            if (foreign)
                printf("[saves] slot %d: a %s save (stamp area %d, language %d): stored with the US game code; %s and "
                       "region_header.bin keep the original\n", slot, s.region, f.area, f.language, "slot.ini region=");
        } else {
            code = PC_SAVE_ERR_WRITE;
        }
        /* a card, or a European / Japanese .gci (its slot copy is a changed copy, not the file itself), put into Saves\
         * goes to Saves\Imported\ so it isn't read again */
        if (ok && move && (is_raw || foreign)) {
            char imported[1300], target[1700];
            const char* base = strrchr(path, '/');
            const char* base2 = strrchr(path, '\\');
            if (base2 != NULL && (base == NULL || base2 > base)) base = base2;
            SDL_snprintf(imported, sizeof(imported), "%sImported/", s_saves);
            SDL_CreateDirectory(imported);
            SDL_snprintf(target, sizeof(target), "%s%s", imported, base != NULL ? base + 1 : path);
            SDL_RenamePath(path, target);
        }
    }
    if (gci_owned) free(gci);
    SDL_free(data);
    return code;
}

/* ---------------------------------------------------------------- rename, duplicate, delete */
int pc_saves_rename(int slot, const char* name) {
    SlotSetup s;
    char save[1300], dir[1200], fname[400], dest[1700];
    if (name == NULL || name[0] == '\0') return 0;
    read_setup(slot, &s);
    SDL_snprintf(s.name, sizeof(s.name), "%s", name);
    if (!write_setup(slot, &s)) return 0;
    if (slot_save_file(slot, save, sizeof(save))) {
        pc_saves_file_name(name, slot, fname, sizeof(fname));
        SDL_snprintf(dest, sizeof(dest), "%s%s", pc_saves_slot_dir(slot, dir, sizeof(dir)), fname);
        if (strcmp(save, dest) != 0) {
            /* a change of capitals only needs a stop on the way: Windows and macOS see "a" and "A" as one name */
            if (SDL_strcasecmp(save, dest) == 0) {
                char tmp[1800];
                SDL_snprintf(tmp, sizeof(tmp), "%s.renaming", dest);
                if (!SDL_RenamePath(save, tmp) || !SDL_RenamePath(tmp, dest)) return 0;
            } else if (!SDL_RenamePath(save, dest)) {
                fprintf(stderr, "[saves] slot %d: could not rename the file to %s: %s\n", slot, dest, SDL_GetError());
                return 0;
            }
        }
    }
    printf("[saves] slot %d renamed\n", slot);
    return 1;
}

int pc_saves_duplicate(int slot) {
    SlotSetup s, t;
    char save[1300], dir[1200], fname[400], dest[1700];
    int slots[MAX_SLOTS];
    read_setup(slot, &s);
    const int n = slot_numbers(slots, MAX_SLOTS);
    /* "Nuzlocke (1)", "(2)"...: the first number no save uses yet (snatcherLeo: "it can say a (1) at the end") */
    blank_setup(&t);
    /* a duplicate plays as its original does (2.12): the same save, so the same promise */
    t.accurate = s.accurate >= 0 ? s.accurate : pc_saves_accurate_get(slot);
    for (int k = 1; k < 1000; k++) {
        int used = 0;
        SDL_snprintf(t.name, sizeof(t.name), "%s (%d)", s.name, k);
        for (int i = 0; i < n && !used; i++) {
            SlotSetup o;
            read_setup(slots[i], &o);
            used = strcmp(o.name, t.name) == 0;
        }
        if (!used) break;
    }
    const int copy = new_slot();
    SDL_snprintf(t.came_from, sizeof(t.came_from), "%s", "duplicated");
    t.order = s.order + 1;
    /* right under the original: everything after it moves down one (display order only; folders keep numbers) */
    for (int i = 0; i < n; i++) {
        SlotSetup o;
        if (slots[i] == slot || !read_setup(slots[i], &o) || o.order <= s.order) continue;
        o.order++;
        write_setup(slots[i], &o);
    }
    if (slot_save_file(slot, save, sizeof(save))) {
        pc_saves_file_name(t.name, copy, fname, sizeof(fname));
        SDL_snprintf(dest, sizeof(dest), "%s%s", pc_saves_slot_dir(copy, dir, sizeof(dir)), fname);
        if (!SDL_CopyFile(save, dest)) return 0;
        char from_dir[1200];
        carry_extras(pc_saves_slot_dir(slot, from_dir, sizeof(from_dir)), dir, 0); /* its long names too (2.4) */
        /* a save imported from another region keeps its record of that region in the copy too (3.5 step 7): the same
         * save, so its GameCube copy goes back the same way */
        if (s.region[0] != '\0') {
            SDL_snprintf(t.region, sizeof(t.region), "%s", s.region);
            if (s.region_header[0] != '\0') {
                char hsrc[1300], hdst[1300];
                SDL_snprintf(hsrc, sizeof(hsrc), "%s%s", from_dir, s.region_header);
                SDL_snprintf(hdst, sizeof(hdst), "%s%s", dir, s.region_header);
                if (SDL_CopyFile(hsrc, hdst)) SDL_snprintf(t.region_header, sizeof(t.region_header), "%s", s.region_header);
            }
        }
    }
    write_setup(copy, &t);
    printf("[saves] slot %d duplicated into slot %d\n", slot, copy);
    return copy;
}

/* A new, empty save (snatcherLeo, 2026-11-14: "we should create the option for users to load in with... no save file ... from
 * the "Saves" screen, let's make it simple to start out with no save file"). Why: every save on the page held a game,
 * so there was no simple way to start fresh; a slot with no file is what a blank memory card is to the game (the title
 * finds no save and offers NEW GAME), and the Saves page already shows such a slot as "not started". The slot goes at
 * the top of the page's order, named `name` (the hub's word, in the player's language), numbered "(1)", "(2)"... when
 * that name is taken, as duplicates are. Its file appears the first time the game saves. Returns the new slot, 0 on
 * failure. All platforms (SDL file calls). */
int pc_saves_new_empty(const char* name) {
    SlotSetup t;
    int slots[MAX_SLOTS];
    const int n = slot_numbers(slots, MAX_SLOTS);
    int top = 0;
    blank_setup(&t);
    t.accurate = pc_option_accurate_new_saves(); /* "A new save: takes the game's default" (the project plan, section 5) */
    SDL_snprintf(t.name, sizeof(t.name), "%s", (name != NULL && name[0] != '\0') ? name : "New save");
    for (int k = 0; k < 1000; k++) {
        int used = 0;
        if (k > 0) SDL_snprintf(t.name, sizeof(t.name), "%s (%d)", (name != NULL && name[0] != '\0') ? name : "New save", k);
        for (int i = 0; i < n && !used; i++) {
            SlotSetup o;
            read_setup(slots[i], &o);
            used = strcmp(o.name, t.name) == 0;
        }
        if (!used) break;
    }
    for (int i = 0; i < n; i++) {
        SlotSetup o;
        if (read_setup(slots[i], &o) && (i == 0 || o.order < top)) top = o.order;
    }
    const int slot = new_slot();
    if (slot <= 0) return 0;
    SDL_snprintf(t.came_from, sizeof(t.came_from), "%s", "made");
    t.order = top - 1; /* above every other save */
    write_setup(slot, &t);
    printf("[saves] slot %d made: a new, empty save%s\n", slot, t.accurate ? " (GameCube-accurate mode)" : "");
    return slot;
}

/* ---------------------------------------------------------------- GameCube-accurate mode, per save (2.12) */
/* an .orresave: the slot has an extras file (2.4, the rule fill_info uses) */
static int slot_has_extras(int slot) {
    char dir[1200], extras[1300];
    SDL_snprintf(extras, sizeof(extras), "%sextras.ini", pc_saves_slot_dir(slot, dir, sizeof(dir)));
    return SDL_GetPathInfo(extras, NULL) ? 1 : 0;
}

int pc_saves_accurate_get(int slot) {
    SlotSetup s;
    read_setup(slot, &s);
    if (s.accurate >= 0) return s.accurate;
    /* no line yet (pc_saves_init gives every slot one at start-up; this covers a folder made while running) */
    return slot_has_extras(slot) ? 0 : pc_option_accurate_new_saves();
}

int pc_saves_accurate_set(int slot, int on) {
    SlotSetup s;
    char dir[1200];
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(pc_saves_slot_dir(slot, dir, sizeof(dir)), &info) || info.type != SDL_PATHTYPE_DIRECTORY) return 0;
    if (on && slot_has_extras(slot)) {
        printf("[saves] slot %d: accurate mode not turned on: it is an .orresave (a GameCube copy is offered)\n", slot);
        return PC_SAVES_ACCURATE_NEEDS_COPY;
    }
    read_setup(slot, &s);
    s.accurate = on != 0;
    if (!write_setup(slot, &s)) return 0;
    printf("[saves] slot %d: GameCube-accurate mode %s (the Saves page)\n", slot, on ? "on" : "off");
    return 1;
}

int pc_saves_add_gcopy(int from_slot, const char* gci_path, const char* name, int accurate) {
    SlotSetup s, t;
    int slots[MAX_SLOTS];
    char dir[1200], fname[400], dest[1700];
    if (gci_path == NULL || !SDL_GetPathInfo(gci_path, NULL)) return 0;
    read_setup(from_slot, &s);
    blank_setup(&t);
    SDL_snprintf(t.name, sizeof(t.name), "%s", (name != NULL && name[0] != '\0') ? name : s.name);
    SDL_snprintf(t.came_from, sizeof(t.came_from), "%s", "gcopy");
    t.accurate = accurate != 0;
    t.order = s.order + 1;
    const int n = slot_numbers(slots, MAX_SLOTS);
    for (int i = 0; i < n; i++) { /* right under the original, as Duplicate puts its copy */
        SlotSetup o;
        if (slots[i] == from_slot || !read_setup(slots[i], &o) || o.order <= s.order) continue;
        o.order++;
        write_setup(slots[i], &o);
    }
    const int slot = new_slot();
    if (slot <= 0) return 0;
    pc_saves_file_name(t.name, slot, fname, sizeof(fname));
    SDL_snprintf(dest, sizeof(dest), "%s%s", pc_saves_slot_dir(slot, dir, sizeof(dir)), fname);
    /* a copy made for the European or Japanese version (3.5 step 9: the copy window's region choice) is added the way
     * the importer adds such a save (step 7): the slot's file gets the US game code so OrrePort's card opens it, and the
     * copy's own first 0x6040 bytes are kept in region_header.bin with region= in slot.ini, so a later GameCube copy
     * back to that region gets them again. A US copy is added as it is. Every OS the same (SDL files). */
    size_t len = 0;
    uint8_t* bytes = (uint8_t*)SDL_LoadFile(gci_path, &len);
    const char letter = bytes != NULL ? pc_save_gci_region_letter(bytes, len) : 0;
    int copied;
    if (letter == 'P' || letter == 'J') {
        char hdr[1300];
        SDL_snprintf(t.region, sizeof(t.region), "%.4s", (const char*)bytes);
        SDL_snprintf(t.region_header, sizeof(t.region_header), "%s", "region_header.bin");
        SDL_snprintf(hdr, sizeof(hdr), "%s%s", dir, t.region_header);
        copied = len >= GCI_HEADER + SAVE_SLOT0 && SDL_SaveFile(hdr, bytes, GCI_HEADER + SAVE_SLOT0);
        pc_save_gci_make_us(bytes, len);
        copied = copied && SDL_SaveFile(dest, bytes, len);
    } else {
        copied = SDL_CopyFile(gci_path, dest);
    }
    SDL_free(bytes);
    if (!copied || !write_setup(slot, &t)) {
        fprintf(stderr, "[saves] the GameCube copy could not be added as slot %d: %s\n", slot, SDL_GetError());
        return 0;
    }
    printf("[saves] the GameCube copy of slot %d added as slot %d%s\n", from_slot, slot,
           t.accurate ? " (GameCube-accurate mode)" : "");
    return slot;
}

/* one file to the Recycle Bin (the hub's Language discs panel, Remove; 2026-11-15): recoverable, never destroyed
 * outright (the plan's rule). Windows: the shell's Recycle Bin (SHFileOperationW, shell32). Linux and macOS for now:
 * moved into a "Removed" folder beside it (likely later: the freedesktop trash spec / NSFileManager trashItem). 1 when
 * the file is gone from where it was */
int pc_recycle_file(const char* path) {
    int ok = 0;
    if (path == NULL || path[0] == '\0') return 0;
#ifdef _WIN32
    {
        wchar_t wide[1300];
        int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1290);
        if (n > 1) {
            for (int i = 0; i < n; i++) if (wide[i] == L'/') wide[i] = L'\\';
            wide[n] = 0; /* double terminator */
            SHFILEOPSTRUCTW op;
            memset(&op, 0, sizeof(op));
            op.wFunc = FO_DELETE;
            op.pFrom = wide;
            op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            ok = SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
        }
    }
#else
    {
        char dir[1300], target[1500];
        const char* slash = strrchr(path, '/');
        const size_t len = slash != NULL ? (size_t)(slash - path) : 0;
        SDL_snprintf(dir, sizeof(dir), "%.*s/Removed/", (int)len, path);
        SDL_CreateDirectory(dir);
        SDL_snprintf(target, sizeof(target), "%s%s", dir, slash != NULL ? slash + 1 : path);
        ok = SDL_RenamePath(path, target);
    }
#endif
    printf("[saves] %s to the Recycle Bin: %s\n", path, ok ? "ok" : "failed");
    return ok;
}

int pc_saves_delete(int slot) {
    char dir[1200];
    pc_saves_slot_dir(slot, dir, sizeof(dir));
    int ok = 0;
#ifdef _WIN32
    /* the Recycle Bin (the plan: never destroy data silently); the whole folder goes, its versions with it, so it
     * comes back whole if restored. Windows only (shell32) */
    {
        wchar_t wide[1300];
        int n = MultiByteToWideChar(CP_UTF8, 0, dir, -1, wide, 1290);
        if (n > 1) {
            for (int i = 0; i < n; i++) if (wide[i] == L'/') wide[i] = L'\\';
            if (wide[n - 2] == L'\\') { wide[n - 2] = 0; n--; }
            wide[n] = 0; /* double terminator */
            SHFILEOPSTRUCTW op;
            memset(&op, 0, sizeof(op));
            op.wFunc = FO_DELETE;
            op.pFrom = wide;
            op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            ok = SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
        }
    }
#else
    {
        /* Linux / macOS for now: Saves\Deleted\ (likely later: the system trash) */
        char deleted[1300], target[1400], stamp[32];
        now_stamp(stamp, sizeof(stamp));
        SDL_snprintf(deleted, sizeof(deleted), "%sDeleted/", s_saves);
        SDL_CreateDirectory(deleted);
        SDL_snprintf(target, sizeof(target), "%sSlot %d %s", deleted, slot, stamp);
        dir[strlen(dir) - 1] = '\0';
        ok = SDL_RenamePath(dir, target);
    }
#endif
    printf("[saves] slot %d deleted (to the Recycle Bin): %s\n", slot, ok ? "ok" : "failed");
    if (ok && pc_saves_current() == slot) {
        PcSaveInfo first;
        int slots[MAX_SLOTS];
        const int n = slot_numbers(slots, MAX_SLOTS);
        pc_saves_set_current(n > 0 ? slots[0] : 1);
        (void)first;
    }
    return ok;
}

/* ---------------------------------------------------------------- start-up */
/* the background read of every slot's save file (pc_saves_init; see facts_of): a few ms per save, off the game's and
 * the hub's frames. Logs how long it took. */
static int SDLCALL warm_facts(void* unused) {
    (void)unused;
    const Uint64 start = SDL_GetTicksNS();
    int slots[MAX_SLOTS], read = 0;
    const int n = slot_numbers(slots, MAX_SLOTS);
    for (int i = 0; i < n; i++) {
        char file[1300];
        SaveFacts f;
        if (slot_save_file(slots[i], file, sizeof(file)) && facts_of(file, &f, NULL, NULL)) read++;
    }
    printf("[saves] %d save file(s) read in the background in %.1f ms\n", read,
           (double)(SDL_GetTicksNS() - start) / 1e6);
    return 0;
}

void pc_saves_init(const char* saves_dir) {
    int slots[MAX_SLOTS], n;
    int count = 0;
    SDL_snprintf(s_saves, sizeof(s_saves), "%s", saves_dir);
    SDL_CreateDirectory(s_saves);

    /* the old shared backups (Saves\Backups\<date>\Slot 1\, before 2026-11-13) move into Slot 1\Backups\<date>\:
     * moved, never deleted ("never destroy data silently") */
    {
        char old[1300];
        SDL_snprintf(old, sizeof(old), "%sBackups/", s_saves);
        char** stamps = SDL_GlobDirectory(old, "*", 0, &count);
        for (int i = 0; stamps != NULL && i < count; i++) {
            char from[1500], to[1500], dir[1200], todir[1300];
            SDL_snprintf(from, sizeof(from), "%s%s/Slot 1", old, stamps[i]);
            SDL_snprintf(todir, sizeof(todir), "%sBackups/", pc_saves_slot_dir(1, dir, sizeof(dir)));
            SDL_CreateDirectory(todir);
            SDL_snprintf(to, sizeof(to), "%s%s", todir, stamps[i]);
            if (SDL_GetPathInfo(from, NULL) && !SDL_GetPathInfo(to, NULL) && SDL_RenamePath(from, to)) {
                char empty[1500];
                SDL_snprintf(empty, sizeof(empty), "%s%s", old, stamps[i]);
                SDL_RemovePath(empty); /* the now-empty date folder (removes only an empty folder) */
            }
        }
        SDL_free(stamps);
        SDL_RemovePath(old); /* only succeeds when empty */
    }

    /* a slot folder without a setup file is adopted (the plan's "registered" rule) */
    n = slot_numbers(slots, MAX_SLOTS);
    for (int i = 0; i < n; i++) {
        SlotSetup s;
        if (!read_setup(slots[i], &s)) {
            SDL_snprintf(s.came_from, sizeof(s.came_from), "%s", slots[i] == 1 ? "made" : "found");
            write_setup(slots[i], &s);
            printf("[saves] slot %d registered (\"%s\")\n", slots[i], s.name);
        }
    }
    if (n == 0) { /* a first start: Slot 1, empty (New Game begins there) */
        SlotSetup s;
        char dir[1200];
        SDL_CreateDirectory(pc_saves_slot_dir(1, dir, sizeof(dir)));
        read_setup(1, &s);
        SDL_snprintf(s.came_from, sizeof(s.came_from), "%s", "made");
        write_setup(1, &s);
    }

    /* GameCube-accurate mode, the migration (2.12, 2026-11-16; the project's suggestion, taken): a save from before this build
     * has no setting of its own; it takes the program-wide value it played with until now once (settings.ini "preset",
     * through the new-saves default, which starts from it), then keeps it. An .orresave takes "off": accurate mode plays
     * .gci saves only, and it couldn't play in accurate mode before either (the hub offered a copy). Written at once,
     * so a later change of the default never moves an existing save. */
    n = slot_numbers(slots, MAX_SLOTS);
    for (int i = 0; i < n; i++) {
        SlotSetup s;
        if (read_setup(slots[i], &s) && s.accurate < 0) {
            s.accurate = slot_has_extras(slots[i]) ? 0 : pc_option_accurate_new_saves();
            write_setup(slots[i], &s);
            printf("[saves] slot %d: GameCube-accurate mode %s (its first setting: %s)\n", slots[i], s.accurate ? "on" : "off",
                   slot_has_extras(slots[i]) ? "an .orresave plays in the normal mode" : "the default for new saves");
        }
    }

    /* a .gci or .raw the player put into Saves\ is imported by moving it in (the plan's third way in) */
    pc_saves_import_loose();

    /* the chosen slot must exist */
    {
        char dir[1200];
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(pc_saves_slot_dir(pc_saves_current(), dir, sizeof(dir)), &info)) {
            n = slot_numbers(slots, MAX_SLOTS);
            pc_saves_set_current(n > 0 ? slots[0] : 1);
        }
    }
    /* previous versions before each in-game save: off in automated runs (replays stay untouched), except a test of them
     * (ORRE_TEST_SAVE_VERSIONS=1, 2026-11-14, rule R5's test) */
    if (!pc_platform_automated_run() || SDL_getenv("ORRE_TEST_SAVE_VERSIONS") != NULL) aurora_card_before_write = before_write;
    printf("[saves] %d save slot(s) in %s; Play starts slot %d\n", slot_numbers(slots, MAX_SLOTS), s_saves,
           pc_saves_current());
    /* every save's facts read in the background, so the hub and the SAVE FILES list find them remembered (facts_of) */
    if (s_facts_mutex == NULL) s_facts_mutex = SDL_CreateMutex();
    if (s_facts_mutex != NULL) {
        SDL_Thread* t = SDL_CreateThread(warm_facts, "save-facts", NULL);
        if (t != NULL) SDL_DetachThread(t);
    }
}

/* A .gci or .raw the player put straight into Saves\ (the plan's third way in), imported by moving it into a new slot.
 * Run at start-up and, rule R6 (snatcherLeo approved, 2026-11-14), each time the main menu's LOAD opens its list, so a file
 * dropped in while the game runs shows at once. A file that isn't imported (still being copied, so cut short or locked;
 * not a save; the same save already in a slot) stays where it is and is tried again on the next pass: never refused
 * for good (the project plan's edge case). Returns how many were imported. Platforms: SDL's file calls, all. */
/* the slots imported loose and not yet handed to the hub's import question (pc_saves_take_loose_imported); a fixed
 * list, since a start-up rarely finds more than a few files (any past 64 simply keep the new-saves default) */
static int s_loose_made[64];
static int s_loose_made_n = 0;

int pc_saves_take_loose_imported(int* out, int max) {
    int n = 0;
    for (int i = 0; i < s_loose_made_n && n < max; i++) out[n++] = s_loose_made[i];
    s_loose_made_n = 0;
    return n;
}

int pc_saves_import_loose(void) {
    const char* patterns[] = {"*.gci", "*.raw", "*.orresave"}; /* .orresave: 2.4 part 5 */
    int imported = 0, count = 0;
    if (s_saves[0] == '\0') return 0;
    for (int p = 0; p < 3; p++) {
        char** files = SDL_GlobDirectory(s_saves, patterns[p], SDL_GLOB_CASEINSENSITIVE, &count);
        for (int i = 0; files != NULL && i < count; i++) {
            char path[1500];
            int made = 0, already = 0;
            SDL_snprintf(path, sizeof(path), "%s%s", s_saves, files[i]);
            const int code = pc_saves_import(path, 1, 0, &made, &already);
            if (code == PC_SAVE_IMPORTED) {
                imported++;
                if (s_loose_made_n < (int)(sizeof(s_loose_made) / sizeof(s_loose_made[0]))) s_loose_made[s_loose_made_n++] = made;
                printf("[saves] %s in Saves\\ imported into slot %d\n", files[i], made);
            } else {
                printf("[saves] %s in Saves\\ not imported (code %d%s); tried again next time\n", files[i], code,
                       code == PC_SAVE_ERR_ALREADY ? ", the same save is already in a slot" : "");
            }
        }
        SDL_free(files);
    }
    return imported;
}

const char* pc_saves_open_current(char* launch_version, size_t size) {
    static char dir[1200];
    const int slot = pc_saves_current();
    pc_saves_slot_dir(slot, dir, sizeof(dir));
    SDL_CreateDirectory(dir);
    clear_undo(slot); /* playing the save ends its Undo (mockup 74: "until you play that save or go back again") */
    /* the save this sitting starts from (its recording replays from it; Report a problem names it), kept only when it
     * changed since the newest version, else the newest version is named */
    if (!keep_version(slot, "launch", 0, launch_version, size) && launch_version != NULL && size > 0) {
        launch_version[0] = '\0';
    }
    return dir;
}
