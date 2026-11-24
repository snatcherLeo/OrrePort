/* gcopy_region_test: "Make a GameCube copy…" for another region, proven on real saves (Phase 3 step 3.5, step 9,
 * 2026-11-19).
 *
 * Why: the hub's copy window (pc_gcopy.c) now writes a copy for the US, European or Japanese game: another game code,
 * that region's card header (banner, title, comment) with its word sum, the region stamp and language bytes in every
 * valid slot, un-nicknamed Pokémon renamed into the target's language, every changed slot sealed again. All of that byte
 * work is pc_gcopy_region.c, which the hub calls; this program calls the same functions on the player's own saves and
 * discs' files (never in the repo) and checks the result:
 *   - every valid slot of each copy still decrypts with a good digest, and empty slots stay as they were;
 *   - the stamp (+0x02 / +0x03) and the language bytes (+0x2B / +0xB48) are the target's in every valid slot;
 *   - the card header's word sum checks out and the game code is the target's;
 *   - a US save copied for the US is byte for byte the original;
 *   - a PAL English save copied for the US gets the US save's header bytes exactly;
 *   - with each region's disc, the copy's header equals the real save of that region and language byte for byte;
 *   - round trips (US -> JPN -> US, JPN -> US -> JPN, PAL English -> JPN -> PAL English) give the original file back;
 *   - un-nicknamed Pokémon carry the target language's species name.
 *
 *   gcopy_region_test --out <folder> --rel <US common_rel> --dol E <US main.dol> --dol P <EU main.dol>
 *                     --dol J <JP main.dol> --names <de|fr|it|es|ja> <message list> ...
 *                     --save <file.gci> ... [--us-ref <US save>] [--pal-en-ref <PAL English save>]
 *                     [--pal-es-ref <PAL Spanish save>] [--jp-ref <Japanese save>]
 *
 * The US species names come from the US common_rel the same way the hub reads them (pc_gcopy_blob_names); the other
 * languages, which the hub takes from its language-disc reader (pc_region_text.cpp, nod), are read here from message
 * lists ("0x003e9<TAB>NAME" lines, the extractor's text output) since this test has no disc reader. Copies are written
 * into --out so other tools (tools/local/colo/savefile.py) can read them. Ends with "ALL PASSED" or "FAILED".
 * Plain C, no SDL: every OS. */
#include "../src/pc_gcopy_region.h"
#include "../src/pc_save_cipher.h"
#include "../src/pc_save_region.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_failed = 0, s_checks = 0;

static void check(int ok, const char* what) {
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("    FAIL: %s\n", what);
    }
}

static uint8_t* load(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    long n;
    uint8_t* data;
    *len = 0;
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
    if (data != NULL) *len = (size_t)n;
    return data;
}

static int save_file(const char* path, const uint8_t* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

/* ---- the species name tables ---- */
static const char* const kCodes[7] = {"", "ja", "en", "de", "fr", "it", "es"}; /* by the game's language number */
static PcGcopyNames s_names[7];
static int s_have[7];
static uint32_t s_keys[PC_GCOPY_SPECIES_MAX];
static int s_key_count;

static int lang_number(const char* code) {
    for (int i = 1; i < 7; i++)
        if (strcmp(kCodes[i], code) == 0) return i;
    return 0;
}

/* a message list line "0x<key><TAB><text>": the text (UTF-8, no control codes for species names) as the game's text */
static int read_names(int lang, const char* path) {
    FILE* f = fopen(path, "rb");
    char line[1024];
    int got = 0;
    if (f == NULL) return 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        char* tab = strchr(line, '\t');
        if (tab == NULL || line[0] != '0' || line[1] != 'x') continue;
        const uint32_t key = (uint32_t)strtoul(line, NULL, 16);
        int id = -1;
        for (int i = 0; i < s_key_count; i++)
            if (s_keys[i] == key && key != 0) id = i;
        if (id < 0 || s_names[lang].name[id] != NULL) continue;
        uint8_t* out = (uint8_t*)calloc(64, 1);
        int n = 0;
        for (const unsigned char* p = (const unsigned char*)tab + 1; *p != 0 && *p != '\r' && *p != '\n' && n < 30;) {
            uint32_t c;
            if (*p < 0x80) c = *p++;
            else if (*p >= 0xE0) { c = (uint32_t)(p[0] & 0x0F) << 12 | (uint32_t)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); p += 3; }
            else { c = (uint32_t)(p[0] & 0x1F) << 6 | (p[1] & 0x3F); p += 2; }
            out[2 * n] = (uint8_t)(c >> 8);
            out[2 * n + 1] = (uint8_t)c;
            n++;
        }
        s_names[lang].name[id] = out;
        got++;
    }
    fclose(f);
    if (got > 0) s_have[lang] = 1;
    return got;
}

/* two names in the game's text equal (up to 11 code units, the 0 end included)? */
static int same_name(const uint8_t* a, const uint8_t* b) {
    if (a == NULL || b == NULL) return 0;
    for (int n = 0; n < 11; n++) {
        if (a[2 * n] != b[2 * n] || a[2 * n + 1] != b[2 * n + 1]) return 0;
        if ((a[2 * n] | a[2 * n + 1]) == 0) return 1;
    }
    return 0;
}

static void utf8_of(const uint8_t* text, int max_units, char* out, size_t size) {
    size_t o = 0;
    for (int i = 0; i < max_units; i++) {
        const uint32_t c = (uint32_t)text[2 * i] << 8 | text[2 * i + 1];
        if (c == 0 || o + 4 >= size) break;
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) out[o++] = (char)(0xC0 | c >> 6), out[o++] = (char)(0x80 | (c & 0x3F));
        else out[o++] = (char)(0xE0 | c >> 12), out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)), out[o++] = (char)(0x80 | (c & 0x3F));
    }
    out[o] = 0;
}

/* ---- the saves ---- */
typedef struct {
    const char* path;
    char base[200];
    uint8_t* gci;
    size_t len;
    char region;       /* its game code's letter */
    int language;      /* the newest slot's stamp language */
    int options_lang;  /* the newest slot's block +0x2B (a European save's Options language) */
} Save;

static PcCardParts s_parts[3]; /* E, P, J */
static int s_has_parts[3];
static int part_index(char r) { return r == 'E' ? 0 : r == 'P' ? 1 : 2; }

static uint8_t s_plain[PC_SAVE_SLOT_SIZE], s_plain2[PC_SAVE_SLOT_SIZE];

/* zero the bytes after each Pokémon nickname's 0 end (party, PC boxes, day care; pc_gcopy_region.c's offsets) */
static void clear_name_tails(uint8_t* plain) {
    size_t recs[6 + 90 + 1];
    int n = 0;
    for (int p = 0; p < 6; p++) recs[n++] = PC_SAVE_BLOCK_AT + 0x70 + 0x30 + (size_t)p * 0x138;
    for (int b = 0; b < 3; b++)
        for (int k = 0; k < 30; k++) recs[n++] = PC_SAVE_BLOCK_AT + 0xB88 + (size_t)b * 0x24A4 + 0x14 + (size_t)k * 0x138;
    recs[n++] = PC_SAVE_BLOCK_AT + 0x8168 + 8;
    for (int i = 0; i < n; i++) {
        uint8_t* f = plain + recs[i] + 0x2E;
        int end = 0;
        for (int u = 0; u < 11; u++) {
            if (end) f[2 * u] = f[2 * u + 1] = 0;
            else if ((f[2 * u] | f[2 * u + 1]) == 0) end = 1;
        }
    }
    memset(plain + 0x0C, 0, 4); /* the sum word follows from the rest */
}

/* two .gci files the same save, apart from leftover bytes after a nickname's end: the first 0x6040 bytes exactly, and
 * every slot's validity and decrypted contents */
static int same_but_name_tails(const uint8_t* a, const uint8_t* b, size_t len) {
    if (memcmp(a, b, PC_GCI_HEADER + PC_GCI_CARD_HEADER) != 0) return 0;
    for (int k = 0; k < 3; k++) {
        const size_t at = PC_GCI_HEADER + PC_GCI_CARD_HEADER + (size_t)k * PC_SAVE_SLOT_SIZE;
        if (at + PC_SAVE_SLOT_SIZE > len) return 0;
        const int va = pc_save_slot_decrypt(a + at, s_plain), vb = pc_save_slot_decrypt(b + at, s_plain2);
        if (va != vb) return 0;
        if (!va) {
            if (memcmp(a + at, b + at, PC_SAVE_SLOT_SIZE) != 0) return 0;
            continue;
        }
        clear_name_tails(s_plain);
        clear_name_tails(s_plain2);
        if (memcmp(s_plain, s_plain2, PC_SAVE_DIGEST_AT) != 0) return 0; /* the digests differ with the tails */
    }
    return 1;
}

/* the copy, made as the hub makes it (pc_gcopy.c make_copy): the slot's file (an imported foreign save has GC6E since
 * the import, pc_save_gci_make_us), the original header (region_header.bin's, or the slot's own for a US save), the
 * header picked, the names tables (English always, the save's own language, the target's language), then the job */
static uint8_t* make_copy(const uint8_t* original_gci, size_t len, char original_region, int original_options_lang,
                          int save_language, char target, int language, int with_discs, int* renamed, int* exact,
                          PcGcopyRenamed* list, int max) {
    uint8_t* gci = (uint8_t*)malloc(len);
    uint8_t* header = (uint8_t*)malloc(PC_CARD_HEADER_SIZE);
    memcpy(gci, original_gci, len);
    pc_save_gci_make_us(gci, len); /* the slot's copy since the import */
    const PcCardParts* tp = with_discs && s_has_parts[part_index(target)] ? &s_parts[part_index(target)] : NULL;
    if (target == 'E' && s_has_parts[0]) tp = &s_parts[0]; /* the US disc is always there */
    pc_gcopy_pick_header(target, language, original_gci + PC_GCI_HEADER, original_region, original_options_lang, tp,
                         s_has_parts[0] ? &s_parts[0] : NULL, header, exact);
    const int to_lang = target == 'J' ? 1 : target == 'P' ? language : 2;
    const PcGcopyNames* from[2];
    int nfrom = 0;
    if (s_have[2]) from[nfrom++] = &s_names[2];
    if (save_language >= 1 && save_language <= 6 && save_language != 2 && s_have[save_language]) from[nfrom++] = &s_names[save_language];
    PcGcopyRegionJob job;
    memset(&job, 0, sizeof(job));
    job.region = target;
    job.language = language;
    job.card_header = header;
    if (s_have[to_lang] && nfrom > 0) {
        job.names_from = from;
        job.names_from_count = nfrom;
        job.names_to = &s_names[to_lang];
    }
    const int valid = pc_gcopy_region_apply(gci, len, &job, renamed, list, max);
    free(header);
    if (valid < 0) {
        free(gci);
        return NULL;
    }
    return gci;
}

static void check_copy(const Save* s, const uint8_t* copy, char target, int language) {
    char what[300];
    int area, lang, bytes;
    pc_gcopy_target_stamp(target, language, &area, &lang, &bytes);
    snprintf(what, sizeof(what), "game code GC6%c", target);
    check(memcmp(copy, "GC6", 3) == 0 && copy[3] == (uint8_t)target && memcmp(copy + 4, "01", 2) == 0, what);
    const uint8_t* header = copy + PC_GCI_HEADER;
    check(pc_card_header_sum(header) == ((uint32_t)header[PC_CARD_SUM_AT] << 24 | (uint32_t)header[PC_CARD_SUM_AT + 1] << 16 |
                                         (uint32_t)header[PC_CARD_SUM_AT + 2] << 8 | header[PC_CARD_SUM_AT + 3]),
          "the card header's word sum");
    for (int k = 0; k < 3; k++) {
        const size_t at = PC_GCI_HEADER + PC_GCI_CARD_HEADER + (size_t)k * PC_SAVE_SLOT_SIZE;
        const int was = pc_save_slot_decrypt(s->gci + at, s_plain2);
        const int now = pc_save_slot_decrypt(copy + at, s_plain);
        snprintf(what, sizeof(what), "slot %d: valid before %d, after %d", k, was, now);
        check(was == now, what);
        if (!was) {
            check(memcmp(s->gci + at, copy + at, PC_SAVE_SLOT_SIZE) == 0, "an invalid slot stays byte for byte");
            continue;
        }
        snprintf(what, sizeof(what), "slot %d: stamp %d/%d, +0x2B %d, +0xB48 %d, want %d/%d, %d", k,
                 s_plain[PC_SAVE_STAMP_AREA], s_plain[PC_SAVE_STAMP_LANGUAGE], s_plain[PC_SAVE_BLOCK_AT + 0x2B],
                 s_plain[PC_SAVE_BLOCK_AT + 0xB48], area, lang, bytes);
        check(s_plain[PC_SAVE_STAMP_AREA] == area && s_plain[PC_SAVE_STAMP_LANGUAGE] == lang &&
                  s_plain[PC_SAVE_BLOCK_AT + 0x2B] == bytes && s_plain[PC_SAVE_BLOCK_AT + 0xB48] == bytes,
              what);
        /* each Pokémon's origin stamp is never touched (decided): the party's +0x08 bytes stay the original's */
        int same = 1;
        for (int p = 0; p < 6; p++) {
            const size_t rec = PC_SAVE_BLOCK_AT + 0x70 + 0x30 + (size_t)p * 0x138;
            same &= memcmp(s_plain + rec + 0x08, s_plain2 + rec + 0x08, 4) == 0;
            same &= memcmp(s_plain + rec + 0x44, s_plain2 + rec + 0x44, 0x18) == 0; /* the original-nickname field too */
        }
        check(same, "the party's origin stamps and original-nickname fields unchanged");
        /* un-nicknamed Pokémon: their name is the target language's species name, when that language is known */
        const int to_lang = target == 'J' ? 1 : target == 'P' ? language : 2;
        if (s_have[to_lang]) {
            int right = 1;
            for (int p = 0; p < 6; p++) {
                const uint8_t* rec = s_plain + PC_SAVE_BLOCK_AT + 0x70 + 0x30 + (size_t)p * 0x138;
                const uint8_t* old = s_plain2 + PC_SAVE_BLOCK_AT + 0x70 + 0x30 + (size_t)p * 0x138;
                const int sp = rec[0] << 8 | rec[1];
                if (sp == 0 || sp >= PC_GCOPY_SPECIES_MAX) continue;
                /* was it un-nicknamed in a language we know? */
                int plain_name = 0;
                for (int l = 1; l < 7; l++)
                    if (s_have[l] && same_name(s_names[l].name[sp], old + 0x2E)) plain_name = 1;
                if (!plain_name) continue;
                if (!same_name(s_names[to_lang].name[sp], rec + 0x2E)) right = 0;
            }
            check(right, "un-nicknamed party Pokémon carry the target language's species name");
        }
    }
}

int main(int argc, char** argv) {
    const char* out_dir = NULL;
    const char* refs[4] = {NULL, NULL, NULL, NULL}; /* us, pal-en, pal-es, jp */
    Save saves[8];
    int nsaves = 0;
    uint8_t* rel = NULL;
    size_t rel_len = 0;
    const char* name_args[16][2];
    int nnames = 0;
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "--out") == 0 && a + 1 < argc) out_dir = argv[++a];
        else if (strcmp(argv[a], "--rel") == 0 && a + 1 < argc) rel = load(argv[++a], &rel_len);
        else if (strcmp(argv[a], "--dol") == 0 && a + 2 < argc) {
            const char r = argv[a + 1][0];
            size_t n;
            uint8_t* dol = load(argv[a + 2], &n);
            a += 2;
            const int ok = dol != NULL && pc_card_parts_from_dol(r, dol, n, &s_parts[part_index(r)]);
            s_has_parts[part_index(r)] = ok;
            printf("main.dol %c (%s): card header pieces %s\n", r, argv[a], ok ? "found" : "NOT found");
            check(ok, "the card header's pieces in main.dol");
            free(dol);
        } else if (strcmp(argv[a], "--names") == 0 && a + 2 < argc && nnames < 16) {
            name_args[nnames][0] = argv[a + 1];
            name_args[nnames][1] = argv[a + 2];
            nnames++;
            a += 2;
        } else if (strcmp(argv[a], "--save") == 0 && a + 1 < argc && nsaves < 8) {
            saves[nsaves++].path = argv[++a];
        } else if (strcmp(argv[a], "--us-ref") == 0 && a + 1 < argc) refs[0] = argv[++a];
        else if (strcmp(argv[a], "--pal-en-ref") == 0 && a + 1 < argc) refs[1] = argv[++a];
        else if (strcmp(argv[a], "--pal-es-ref") == 0 && a + 1 < argc) refs[2] = argv[++a];
        else if (strcmp(argv[a], "--jp-ref") == 0 && a + 1 < argc) refs[3] = argv[++a];
        else {
            fprintf(stderr, "unknown argument %s (see the file's comment for the usage)\n", argv[a]);
            return 2;
        }
    }
    if (out_dir == NULL || nsaves == 0) {
        fprintf(stderr, "usage: gcopy_region_test --out <folder> --rel <US common_rel> --dol E|P|J <main.dol> ... "
                        "--names <lang> <list> ... --save <file.gci> ...\n");
        return 2;
    }
    /* the species table and the US names, the hub's way */
    s_key_count = rel != NULL ? pc_gcopy_species_keys(rel, rel_len, s_keys, PC_GCOPY_SPECIES_MAX) : 0;
    const int en = s_key_count > 0 ? pc_gcopy_blob_names(rel, rel_len, "US", s_keys, s_key_count, &s_names[2]) : 0;
    s_have[2] = en > 0;
    printf("US common_rel: %d species, %d English names\n", s_key_count, en);
    check(s_key_count == 413 && en == 386, "413 species and 386 English names");
    for (int i = 0; i < nnames; i++) {
        const int l = lang_number(name_args[i][0]);
        const int got = l > 0 ? read_names(l, name_args[i][1]) : 0;
        printf("names %s: %d from %s\n", name_args[i][0], got, name_args[i][1]);
    }
    /* the reference headers (real saves of each region and language) */
    uint8_t* ref_gci[4] = {NULL, NULL, NULL, NULL};
    for (int i = 0; i < 4; i++) {
        size_t n;
        ref_gci[i] = refs[i] != NULL ? load(refs[i], &n) : NULL;
    }
    /* the US and PAL English headers are byte for byte the same (regions.md): checked, since the fallback relies on it */
    if (ref_gci[0] != NULL && ref_gci[1] != NULL)
        check(memcmp(ref_gci[0] + PC_GCI_HEADER, ref_gci[1] + PC_GCI_HEADER, PC_CARD_HEADER_SIZE) == 0,
              "the US and PAL English card headers are identical");
    /* every disc's pieces build its real save's header exactly */
    {
        uint8_t h[PC_CARD_HEADER_SIZE];
        const struct { int ref; char r; int lang; } built[] = {{0, 'E', 0}, {1, 'P', 2}, {2, 'P', 6}, {3, 'J', 0}};
        for (size_t i = 0; i < sizeof(built) / sizeof(built[0]); i++) {
            if (ref_gci[built[i].ref] == NULL || !s_has_parts[part_index(built[i].r)]) continue;
            pc_card_header_build(&s_parts[part_index(built[i].r)], built[i].lang, h);
            const int same = memcmp(h, ref_gci[built[i].ref] + PC_GCI_HEADER, PC_CARD_HEADER_SIZE) == 0;
            printf("header built from the %c disc (language %d) = the real save's: %s\n", built[i].r, built[i].lang,
                   same ? "PASS" : "FAIL");
            check(same, "a disc-built header equals the real save's");
        }
    }

    for (int i = 0; i < nsaves; i++) {
        Save* s = &saves[i];
        const char* slash = strrchr(s->path, '/');
        const char* bslash = strrchr(s->path, '\\');
        const char* b = slash > bslash ? slash : bslash;
        snprintf(s->base, sizeof(s->base), "%s", b != NULL ? b + 1 : s->path);
        char* dot = strrchr(s->base, '.');
        if (dot != NULL) *dot = 0;
        for (char* c = s->base; *c; c++) if (*c == ' ') *c = '_';
        s->gci = load(s->path, &s->len);
        s->region = s->gci != NULL ? pc_save_gci_region_letter(s->gci, s->len) : 0;
        const int newest = s->region != 0 ? pc_save_gci_newest_slot(s->gci, s->len, s_plain) : -1;
        if (newest < 0) {
            printf("%s: not a readable Colosseum save\n", s->path);
            check(0, "readable save");
            continue;
        }
        pc_save_stamp(s_plain, NULL, &s->language);
        s->options_lang = s_plain[PC_SAVE_BLOCK_AT + 0x2B];
        printf("\n%s: GC6%c, stamp language %d, Options language %d\n", s->path, s->region, s->language, s->options_lang);
        static const struct { char r; int lang; } kTargets[] = {{'E', 0}, {'P', 2}, {'P', 3}, {'P', 4}, {'P', 5}, {'P', 6}, {'J', 0}};
        for (int discs = 1; discs >= 0; discs--) {
            for (size_t t = 0; t < sizeof(kTargets) / sizeof(kTargets[0]); t++) {
                const char target = kTargets[t].r;
                const int lang = kTargets[t].lang;
                int renamed = 0, exact = 0;
                PcGcopyRenamed list[4];
                uint8_t* copy = make_copy(s->gci, s->len, s->region, s->options_lang, s->language, target, lang, discs,
                                          &renamed, &exact, list, 4);
                const int before = s_failed;
                printf("  -> %c%s%s (%s): ", target, lang ? " language " : "", lang ? kCodes[lang] : "",
                       discs ? "with every region's disc" : "US disc only");
                if (copy == NULL) {
                    printf("no copy\n");
                    check(0, "copy made");
                    continue;
                }
                printf("%d renamed, header %s", renamed, exact ? "that region's own" : "not that region's own (US fallback)");
                if (renamed > 0) {
                    char a[64], z[64];
                    utf8_of(list[0].from, 11, a, sizeof(a));
                    utf8_of(list[0].to, 11, z, sizeof(z));
                    printf(", e.g. %s -> %s", a, z);
                }
                printf("\n");
                check_copy(s, copy, target, lang);
                /* expectations */
                if (s->region == 'E' && target == 'E') check(memcmp(copy, s->gci, s->len) == 0, "US -> US is byte-identical");
                if (target == 'E' && ref_gci[0] != NULL)
                    check(memcmp(copy + PC_GCI_HEADER, ref_gci[0] + PC_GCI_HEADER, PC_CARD_HEADER_SIZE) == 0,
                          "a US copy has the US save's header bytes");
                if (discs || target == 'E' || (target == 'P' && lang == 2)) check(exact, "the header is the region's own");
                const int ref = target == 'P' && lang == 2 ? 1 : target == 'P' && lang == 6 ? 2 : target == 'J' ? 3 : -1;
                if (ref >= 0 && ref_gci[ref] != NULL && exact)
                    check(memcmp(copy + PC_GCI_HEADER, ref_gci[ref] + PC_GCI_HEADER, PC_CARD_HEADER_SIZE) == 0,
                          "the copy's header equals that region's real save");
                /* round trip back to the save's own region (with discs): the original file again, when its slots carry
                 * no mixed creation slot (the PAL Spanish file's first slot is stamped English: regions.md) */
                if (discs && target != s->region && !(s->region == 'P' && s->options_lang != 2)) {
                    int r2 = 0, e2 = 0;
                    uint8_t* back = make_copy(copy, s->len, target, target == 'P' ? lang : 0,
                                              target == 'J' ? 1 : target == 'P' ? lang : 2, s->region,
                                              s->region == 'P' ? s->options_lang : 0, 1, &r2, &e2, NULL, 0);
                    const int same = back != NULL && memcmp(back, s->gci, s->len) == 0;
                    /* a renamed name is written zero-filled after its end; the Japanese game leaves old bytes there
                     * (never read: likely). So a round trip through a rename is the same save up to those bytes */
                    const int tails = !same && back != NULL && same_but_name_tails(back, s->gci, s->len);
                    check(same || tails, "the round trip gives the original file back");
                    if (tails) printf("    round trip back to GC6%c: the original save, apart from leftover bytes after a name's end\n",
                                      s->region);
                    if (!same && !tails && back != NULL) {
                        /* where it differs: in a slot, compare the decrypted bytes (the cipher spreads one change) */
                        size_t first = 0;
                        while (first < s->len && back[first] == s->gci[first]) first++;
                        printf("    round trip differs from file byte 0x%zX", first);
                        if (first >= PC_GCI_HEADER + PC_GCI_CARD_HEADER) {
                            const size_t k = (first - PC_GCI_HEADER - PC_GCI_CARD_HEADER) / PC_SAVE_SLOT_SIZE;
                            const size_t at = PC_GCI_HEADER + PC_GCI_CARD_HEADER + k * PC_SAVE_SLOT_SIZE;
                            pc_save_slot_decrypt(back + at, s_plain);
                            pc_save_slot_decrypt(s->gci + at, s_plain2);
                            size_t d = 0;
                            /* the sum word (+0x0C) follows from the rest: look past it */
                            while (d < PC_SAVE_SLOT_SIZE && (s_plain[d] == s_plain2[d] || (d >= 0x0C && d < 0x10))) d++;
                            printf(" (slot %zu, plain +0x%zX: %02X, was %02X)", k, d, d < PC_SAVE_SLOT_SIZE ? s_plain[d] : 0,
                                   d < PC_SAVE_SLOT_SIZE ? s_plain2[d] : 0);
                        }
                        printf("\n");
                    }
                    if (same) printf("    round trip back to GC6%c: byte for byte the original\n", s->region);
                    free(back);
                }
                if (discs) {
                    char path[1400];
                    snprintf(path, sizeof(path), "%s/%s__to_%c%s.gci", out_dir, s->base, target, lang ? kCodes[lang] : "");
                    check(save_file(path, copy, s->len), "the copy written");
                }
                if (s_failed != before) printf("    (%d check(s) failed above)\n", s_failed - before);
                free(copy);
            }
        }
    }
    printf("\n%d checks: %s\n", s_checks, s_failed == 0 && s_checks > 0 ? "ALL PASSED" : "FAILED");
    return s_failed == 0 && s_checks > 0 ? 0 : 1;
}
