/*
 * pc_gcopy.c: "Make a GameCube copy…", the foundation (pc_gcopy.h says what it is for).
 *
 * How it works, and why:
 * - The checklist is the kRules table below, one function per thing a GameCube save can't hold. Each reports notes
 *   (pc_gcopy.h). Today a slot is always a plain .gci, so the only live rule is "save" (there must be a readable US
 *   Colosseum save to copy); "names" is written but has nothing to read until the font renderer stores OrrePort's
 *   longer names in the slot's setup file (step 2.2; then its keys are filled and the rule starts reporting).
 * - The rule for adding rules (snatcherLeo, 2026-11-13, through C; docs/port/saves.md "The GameCube copy's checklist"):
 *   every feature that stores something a GameCube can't hold (a long name, a 4th box, an item past 99, a modded
 *   Pokémon, a position saved anywhere) adds its rule here in the same change.
 * - Making the copy: the slot's .gci is the GameCube save itself (the plan's rule: nothing extra goes into the .gci),
 *   so the copy is that file with the rules' changes applied (*updated 2026-11-19:* the first changes are the region
 *   copy's, below; for the save's own region with nothing to rename it is still the file as it is). It is written next to nothing of OrrePort's:
 *   wherever the player picks, under the save's name, never replacing a file. The slot is never changed.
 * - Where a GameCube copy puts the player (verified 2026-11-13 from the code): a save stores the room and entrance the
 *   scripts last set as the return point (std235_menuShowSaveMenu -> fn_8011418C), and loading puts the player there
 *   (floor.c). So a copy of a save made anywhere (Access PC menu anywhere) loads at an original room and entrance on a
 *   console; resuming where the player stood would be an OrrePort-only extra, and a rule here when it's built.
 * - The plan's final check, "load it once in accurate mode", is the hand test after the font renderer (snatcherLeo: "we should
 *   revisit and test it after we get the font renderer stuff working").
 *
 * - *Added 2026-11-19 (3.5 step 9; snatcherLeo's four calls of 2026-11-19, the decision log):* a copy for another region.
 *   The window picks the target (PcGcopyTarget: US, PAL with its language, JPN); every check first makes the copy in
 *   memory (plan_copy, the same code the file is written from), so each rule reports exactly what the file will hold.
 *   The byte work is pc_gcopy_region.c (plain C, proven on real saves by test/gcopy_region_test.c); this file finds
 *   its inputs: the slot's .gci, the original card header kept at import (region_header.bin), each region's card
 *   header pieces from its disc (the US disc Aurora has open; the European and Japanese ones handed over by the
 *   language-disc reader, pc_gcopy_note_dol), and the species names (the US common_rel through pc_region_text.cpp, the
 *   other languages from the language discs' text). New rules: "region", "pal_language", "species_names",
 *   "region_letters" (each says why below). docs/port/saves.md "A GameCube copy for another region".
 *
 * Platforms: plain C over SDL's file calls; Windows, Linux and macOS alike (Aurora's disc layer for the US main.dol is
 * the same on every OS).
 */
#include "pc_gcopy.h"
#include "pc_gcopy_region.h"
#include "pc_names.h"
#include "pc_region_text.h"
#include "pc_save_cipher.h"
#include "pc_save_region.h"
#include "pc_saves.h"
#include "pc_text.h"

#include <SDL3/SDL.h>

#ifdef PC_HAVE_AURORA_DVD
#include <aurora/dvd.h> /* the US disc's main.dol: the US card header's pieces */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* pc_game_text_language(void); /* pc_game_text.cpp: the play language ("en", "de", ...) */

/* ---- the card header pieces of each region (snatcherLeo's call 1) ----
 * The European and Japanese ones arrive from the language-disc reader's thread (pc_region_text.cpp read_one): the
 * pieces are written while the flag is 0 and the flag set after, so the hub (which reads them on its own thread) sees
 * either nothing or a whole set. The US ones are read once from the disc the game plays. */
static PcCardParts s_dol_parts[2]; /* 0 Europe, 1 Japan */
static SDL_AtomicInt s_dol_ready[2];

void pc_gcopy_note_dol(char region, const uint8_t* dol, size_t size) {
    if (region != 'P' && region != 'J') return;
    const int i = region == 'J';
    SDL_SetAtomicInt(&s_dol_ready[i], 0);
    if (dol == NULL) return;
    if (pc_card_parts_from_dol(region, dol, size, &s_dol_parts[i])) {
        SDL_SetAtomicInt(&s_dol_ready[i], 1);
        printf("[gcopy] the %s disc's card header found (a GameCube copy for that region gets its own)\n",
               region == 'J' ? "Japanese" : "European");
    } else {
        printf("[gcopy] the %s disc's card header wasn't where expected (another revision?): copies for that region get "
               "the US one\n",
               region == 'J' ? "Japanese" : "European");
    }
}

static const PcCardParts* us_parts(void) {
    static PcCardParts parts;
    static int tried = 0;
    if (!tried) {
        tried = 1;
#ifdef PC_HAVE_AURORA_DVD
        const u8* dol = NULL;
        size_t size = 0;
        if (aurora_dvd_main_dol(&dol, &size)) pc_card_parts_from_dol('E', dol, size, &parts);
#endif
        if (!parts.ok) printf("[gcopy] the US disc's card header couldn't be read\n");
    }
    return parts.ok ? &parts : NULL;
}

static const PcCardParts* region_parts(char region) {
    if (region == 'E') return us_parts();
    const int i = region == 'J';
    return SDL_GetAtomicInt(&s_dol_ready[i]) ? &s_dol_parts[i] : NULL;
}

/* ---- species names (snatcherLeo's call 3) ---- */
static const char* lang_code(int language) {
    static const char* const kCodes[7] = {NULL, "ja", "en", "de", "fr", "it", "es"};
    return language >= 1 && language <= 6 ? kCodes[language] : NULL;
}

/* One language's species names into out: English from the US common_rel (read once, the game's own banks), the
 * others from the language discs' text (pc_region_text_get: NULL while that disc is missing or still being read).
 * 1 when the language has names. The pointers stay valid while the discs aren't rescanned (the copy is made at once). */
static int names_for(int language, PcGcopyNames* out) {
    static uint32_t keys[PC_GCOPY_SPECIES_MAX];
    static int key_count = -1;
    static PcGcopyNames english;
    static int english_count = 0;
    static const uint8_t* rel = NULL;
    static size_t rel_size = 0;
    memset(out, 0, sizeof(*out));
    if (key_count < 0) {
        rel = pc_region_us_common_rel(&rel_size);
        key_count = rel != NULL ? pc_gcopy_species_keys(rel, rel_size, keys, PC_GCOPY_SPECIES_MAX) : 0;
        english_count = key_count > 0 ? pc_gcopy_blob_names(rel, rel_size, "US", keys, key_count, &english) : 0;
        printf("[gcopy] species names: %d species, %d English names\n", key_count, english_count);
    }
    if (key_count <= 0) return 0;
    if (language == 2) {
        *out = english;
        return english_count > 0;
    }
    const char* code = lang_code(language);
    int got = 0;
    if (code == NULL) return 0;
    for (int id = 0; id < key_count; id++) {
        if (keys[id] == 0) continue;
        out->name[id] = pc_region_text_get(code, keys[id], NULL);
        got += out->name[id] != NULL;
    }
    return got > 0;
}

/* ---- the copy, made in memory (every rule reads it; the makers write it) ---- */
typedef struct GcopyPlan {
    int ok;                /* the copy was made (gci holds it) */
    PcGcopyTarget target;
    char own_region;       /* the region of the save's original file: 'E', 'P', 'J' */
    int own_language;      /* the newest slot's stamp language (1 Japanese, 2 English, 3-6) */
    int header_source;     /* PC_GCOPY_HEADER_* */
    int header_exact;      /* the card header is the one that region's game writes */
    int names_to_missing;  /* the target language's species names weren't available: no renaming (its number) */
    int names_from_missing; /* the save's own language's names weren't available (its number): only English ones renamed */
    int renamed;
    PcGcopyRenamed list[PC_GCOPY_MAX_NOTES];
    uint8_t* gci;          /* the copy's bytes */
    size_t len;
    uint8_t* plain;        /* the copy's newest valid slot, decrypted (the names the target game will show) */
} GcopyPlan;

static char region_of_save(const PcSaveInfo* save) {
    const char r = save->imported_region[3];
    return r == 'P' || r == 'J' ? r : 'E';
}

void pc_gcopy_default_target(int slot, PcGcopyTarget* out) {
    PcSaveInfo save;
    out->region = 'E';
    out->language = 2;
    if (pc_saves_get(slot, &save)) out->region = region_of_save(&save);
    /* snatcherLeo's call 2: the play language when it is a European one, else English */
    const char* play = pc_game_text_language();
    for (int l = 3; l <= 6 && play != NULL; l++)
        if (strcmp(play, lang_code(l)) == 0) out->language = l;
}

static void plan_free(GcopyPlan* plan) {
    SDL_free(plan->gci);
    free(plan->plain);
    plan->gci = NULL;
    plan->plain = NULL;
}

static int plan_copy(int slot, const PcSaveInfo* save, const PcGcopyTarget* target, GcopyPlan* plan) {
    char dir[1200], path[1300];
    size_t hlen = 0;
    memset(plan, 0, sizeof(*plan));
    if (target != NULL) plan->target = *target;
    else pc_gcopy_default_target(slot, &plan->target);
    if (plan->target.region != 'P' && plan->target.region != 'J') plan->target.region = 'E';
    if (plan->target.language < 2 || plan->target.language > 6) plan->target.language = 2;
    plan->own_region = region_of_save(save);
    if (!save->has_save || save->file[0] == '\0') return 0;
    plan->gci = (uint8_t*)SDL_LoadFile(save->file, &plan->len);
    plan->plain = (uint8_t*)malloc(PC_SAVE_SLOT_SIZE);
    uint8_t* header = (uint8_t*)malloc(PC_CARD_HEADER_SIZE);
    uint8_t* kept = NULL;
    if (plan->gci == NULL || plan->plain == NULL || header == NULL ||
        pc_save_gci_newest_slot(plan->gci, plan->len, plan->plain) < 0) {
        free(header);
        plan_free(plan);
        return 0;
    }
    pc_save_stamp(plan->plain, NULL, &plan->own_language);
    const int own_options = plan->plain[PC_SAVE_BLOCK_AT + 0x2B]; /* a European save's Options language */
    /* the original card header: kept at import for a European or Japanese save (step 7), else the slot's own */
    const uint8_t* original = plan->gci + PC_GCI_HEADER;
    if (plan->own_region != 'E') {
        SDL_snprintf(path, sizeof(path), "%sregion_header.bin", pc_saves_slot_dir(slot, dir, sizeof(dir)));
        kept = (uint8_t*)SDL_LoadFile(path, &hlen);
        if (kept != NULL && hlen >= PC_GCI_HEADER + PC_CARD_HEADER_SIZE) original = kept + PC_GCI_HEADER;
        else printf("[gcopy] slot %d: its original card header (%s) is missing; the slot's own is used\n", slot, path);
    }
    const char t = plan->target.region;
    plan->header_source = pc_gcopy_pick_header(t, plan->target.language, original, plan->own_region, own_options,
                                               region_parts(t), us_parts(), header, &plan->header_exact);
    /* names: recognise English (the US program names every Pokémon it makes in English) and the save's own language;
     * write the target's */
    static PcGcopyNames from_en, from_own, to;
    const PcGcopyNames* from[2];
    int nfrom = 0;
    const int to_lang = t == 'J' ? 1 : t == 'P' ? plan->target.language : 2;
    if (names_for(2, &from_en)) from[nfrom++] = &from_en;
    if (plan->own_language != 2 && lang_code(plan->own_language) != NULL) {
        if (names_for(plan->own_language, &from_own)) from[nfrom++] = &from_own;
        else plan->names_from_missing = plan->own_language;
    }
    const int have_to = names_for(to_lang, &to);
    if (!have_to) plan->names_to_missing = to_lang;
    PcGcopyRegionJob job;
    memset(&job, 0, sizeof(job));
    job.region = t;
    job.language = plan->target.language;
    job.card_header = header;
    if (have_to && nfrom > 0) {
        job.names_from = from;
        job.names_from_count = nfrom;
        job.names_to = &to;
    }
    const int valid = pc_gcopy_region_apply(plan->gci, plan->len, &job, &plan->renamed, plan->list, PC_GCOPY_MAX_NOTES);
    free(header);
    SDL_free(kept);
    if (valid < 0 || pc_save_gci_newest_slot(plan->gci, plan->len, plan->plain) < 0) {
        plan_free(plan);
        return 0;
    }
    plan->ok = 1;
    return 1;
}

/* the region's word as the hub shows it ("US", "JPN", "PAL · German"; saves.region.*, translatable) */
static void region_word(char region, int language, char* out, size_t size) {
    const char* word = pc_text(region == 'J' ? "saves.region.jpn" : region == 'P' ? "saves.region.pal" : "saves.region.us");
    if (region == 'P' && language >= 2 && language <= 6) {
        char key[32];
        SDL_snprintf(key, sizeof(key), "saves.language.%d", language);
        pc_text_format(out, size, "saves.region.with_language", "region", word, "language", pc_text(key), NULL);
    } else {
        SDL_snprintf(out, size, "%s", word);
    }
}

/* a name in the game's text (big-endian UTF-16, at most units) as UTF-8 */
static void name_utf8(const uint8_t* text, int units, char* out, size_t size) {
    size_t o = 0;
    for (int i = 0; i < units; i++) {
        const unsigned c = (unsigned)text[2 * i] << 8 | text[2 * i + 1];
        if (c == 0 || o + 4 >= size) break;
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) out[o++] = (char)(0xC0 | c >> 6), out[o++] = (char)(0x80 | (c & 0x3F));
        else out[o++] = (char)(0xE0 | c >> 12), out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)), out[o++] = (char)(0x80 | (c & 0x3F));
    }
    out[o] = 0;
}

static PcGcopyNote* new_note(PcGcopyNote* notes, int* got, int kind, const char* key) {
    PcGcopyNote* o = &notes[(*got)++];
    memset(o, 0, sizeof(*o));
    o->kind = kind;
    SDL_snprintf(o->text_key, sizeof(o->text_key), "%s", key);
    return o;
}

typedef int (*GcopyRule)(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max);

/* "save": there must be a readable Colosseum save in the slot, or nothing can be copied */
static int rule_save(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    (void)slot;
    if ((save->has_save && plan->ok) || max < 1) return 0;
    memset(&notes[0], 0, sizeof(notes[0]));
    notes[0].rule = "save";
    notes[0].blocking = 1;
    notes[0].kind = PC_GCOPY_BLOCKS;
    /* no game in it yet, or a save file that couldn't be read or made into a copy (*added 2026-11-19*) */
    SDL_snprintf(notes[0].text_key, sizeof(notes[0].text_key), "%s", save->has_save ? "gcopy.note.unreadable" : "gcopy.note.no_save");
    SDL_snprintf(notes[0].what, sizeof(notes[0].what), "%s", save->name);
    return 1;
}

/* "names": OrrePort's longer names (the hero, Rui, nicknames: 12 letters, any letters the font renderer can draw)
 * against the GameCube's (7 for names, 10 for nicknames, 8 for box names; the US font's letters). Nothing to read in
 * 0.1 yet: the slot's setup file has no long names until step 2.2 writes them (keys "full_<what>" = the full name,
 * "gc_<what>" = the GameCube name in the save). When it does, each difference becomes a note: kept = the full name,
 * copy = the GameCube name.
 * *Decided 2026-11-13 (snatcherLeo, through C; the project plan step 2.2 step 5, "How names and the GameCube fit together"):*
 * - "GameCube-safe" letters = what fonts 0 AND 4 can both draw, computed at start-up from the player's own disc fonts
 *   (nothing hard-coded; a PAL or Japanese disc gives its own set).
 * - The save itself always holds a GameCube-safe name ("closest fit": safe letters kept, cut to 7 / 7 / 10 / 8; a
 *   missing letter becomes its plain form where one exists (ł -> l), otherwise dropped; nothing left -> WES / RUI, or
 *   no nickname = the species name). The full name is kept beside it in slot.ini (the project's call: the "full_" keys above,
 *   not a sidecar, until 2.4 brings one) and shown wherever the game draws a name, through a hook. So the .gci inside is
 *   always Dolphin-ready, and a differing name is what makes the save an .orresave.
 * - The GameCube copy carries the in-save names; its window lists each differing name, editable before writing.
 * - Nothing in the game itself marks a name (snatcherLeo: in-game marks "would ruin the experience"): the Saves page and the
 *   save's earlier versions carry that information.
 * Platform-neutral. */
/* *Built 2026-11-15 (the IME work):* the closest fit itself (fonts 0 and 4 read from the disc, plain forms for
 * marked Latin letters and full-width ASCII, other letters dropped, nothing left -> the game's own fallback name) is
 * pc_name_keyboard.cpp's gc_name, so a name typed with an input method (kanji, hangul...) still saves a name a console
 * draws; before, every typed letter was in the US font and the fit was only a cut. */
/* *Built 2026-11-14 (2.4):* the full names live in the slot's extras file (pc_names.cpp; the slot.ini "full_"
 * keys above were never needed). The .gci already holds every GameCube name, so the copy carries them as they are;
 * each long name is one "Changes in the copy" note: the name kept in OrrePort -> the GameCube name the copy has,
 * editable (the hub edits the GameCube name before the copy is written). */
/* *Changed 2026-11-16 (visible letters, pc_grapheme.h):* the long names are read into the heap: a PcLongName holds a
 * whole 12-letter name now (up to 529 bytes), so 48 are about 34 KB, on top of the caller's 64 notes. All OSes the
 * same. Nothing else changes for the copy: names are stored as typed, and the GameCube name (gc) is the closest fit
 * made by the keyboard (gc_name), cut to the game's 7 / 7 / 10 / 8 by GameCube letters as before. */
static int rule_names(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    PcLongName* names = (PcLongName*)SDL_malloc(sizeof(PcLongName) * 48);
    (void)save, (void)plan;
    const int n = names != NULL ? pc_names_read_slot(slot, names, 48) : 0;
    int got = 0;
    for (int i = 0; i < n && got < max; i++) {
        PcGcopyNote* o = &notes[got++];
        memset(o, 0, sizeof(*o));
        o->kind = PC_GCOPY_CHANGES;
        o->editable = 1;
        SDL_snprintf(o->edit_id, sizeof(o->edit_id), "%s", names[i].id);
        SDL_snprintf(o->text_key, sizeof(o->text_key), "%s", "gcopy.note.name");
        SDL_snprintf(o->what, sizeof(o->what), "%s", names[i].species); /* "" for the trainer, Rui, a box: the hub labels them */
        SDL_snprintf(o->kept, sizeof(o->kept), "%s", names[i].full);
        SDL_snprintf(o->copy, sizeof(o->copy), "%s", names[i].gc);
    }
    SDL_free(names);
    return got;
}

/* "emoji": letters past U+FFFF in a long name (emoji, rare CJK; *added 2026-11-16*, the Noto wiring, with the same
 * change that lets them be typed, per the project rules "The GameCube copy's checklist").
 * Why a rule of its own: the game's text is one 16-bit code per letter and no GameCube font draws these, so OrrePort
 * carries them in the game's text as private stand-in codes for one run (pc_font_files.h) and keeps the real letters
 * only in the extras file. The save itself never holds them: the closest fit (pc_name_keyboard.cpp gc_name) drops every
 * letter the disc's fonts 0 and 4 lack, and a stand-in is never one of them. So the copy needs no change for them (the
 * "names" rule already lists the name's GameCube form); this note tells the player plainly that the emoji stay in
 * OrrePort and come back if the copy is brought back (the extras travel with the save, orresave.md). One note per
 * name that has any. Platform-neutral (UTF-8 bytes: a lead byte of 0xF0 or more starts a letter past U+FFFF). */
static int rule_emoji(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    PcLongName* names = (PcLongName*)SDL_malloc(sizeof(PcLongName) * 48); /* on the heap: rule_names says why */
    (void)save, (void)plan;
    const int n = names != NULL ? pc_names_read_slot(slot, names, 48) : 0;
    int got = 0;
    for (int i = 0; i < n && got < max; i++) {
        int past = 0;
        for (const unsigned char* p = (const unsigned char*)names[i].full; *p != 0; p++) past |= *p >= 0xF0;
        if (!past) continue;
        PcGcopyNote* o = &notes[got++];
        memset(o, 0, sizeof(*o));
        o->kind = PC_GCOPY_STAYS;
        SDL_snprintf(o->text_key, sizeof(o->text_key), "%s", "gcopy.note.emoji");
        SDL_snprintf(o->what, sizeof(o->what), "%s", names[i].species);
        SDL_snprintf(o->kept, sizeof(o->kept), "%s", names[i].full);
        SDL_snprintf(o->copy, sizeof(o->copy), "%s", names[i].gc);
    }
    SDL_free(names);
    return got;
}

/* *Checked 2026-11-16 (the project's build helper; right-to-left names, pc_rtl.h), no new rule needed:* Arabic and Hebrew
 * names are stored as typed (their own letters, in typed order) in the extras file, like any letters no GameCube font
 * has; the joined forms and the reversed order exist only in the copy handed to a drawing call and never reach the
 * save, the extras file or the copy. The closest fit (pc_name_keyboard.cpp gc_name) drops every Arabic and Hebrew
 * letter (fonts 0 and 4 of the player's disc have none, and they have no plain Latin form), keeping only digits, Latin
 * and spaces typed with them, or the game's own fallback name when nothing is left. So "names" above already lists such
 * a name (kept: the full name, drawn joined and from the right by the hub; copy: its GameCube form), and the copy
 * needs nothing more. */
/* *Checked 2026-11-16 (the project's build helper; HarfBuzz shaping, pc_shape.h), no new rule needed:* shaped names (Indic,
 * Thai, Khmer, Arabic, Hebrew) are stored as typed, like the right-to-left ones above; the shaped clusters' stand-in
 * codes live only in the drawn copy of a run and never reach the save, the extras file or the copy. With shaping on,
 * the naming keyboard now also keeps an emoji's modifiers (a skin tone, past U+FFFF; the zero-width joiner U+200D and
 * variation selector U+FE0F, within 16 bits): the closest fit drops all of them (no GameCube font has them and they
 * have no plain form), and the "emoji" rule above notes any name with a letter past U+FFFF (every emoji with a skin
 * tone or a joiner has one). A selector after a symbol within 16 bits (♥ + U+FE0F) is dropped like any letter the
 * console lacks, and the "names" rule lists that name's GameCube form. */
/* *Checked 2026-11-16 (2.12, GameCube-accurate mode per save), no new rule needed:* accurate mode is a setting of the
 * save (slot.ini "gamecube_accurate"), not something the save holds: the .gci never carries it, and a GameCube or
 * Dolphin has no other mode to play in, so the copy itself needs nothing. What a copy gets in OrrePort is decided where
 * it comes back in: a copy made through accurate mode's offer (PLAY on an .orresave set accurate, or the Saves page's
 * switch turned on for an .orresave; pc_hub_saves.cpp gcopy_write) is also added at once as a new save with accurate
 * mode on (pc_saves_add_gcopy: the copy's bytes, no extras file, so it stays a .gci); any other copy imported later
 * takes the new-saves default like every import (an import matching an .orresave's .gci gets that save's long names
 * back and stays normal, pc_saves_import). Accurate mode itself never stores anything a GameCube can't hold (its list:
 * everything that would make an .orresave is off), so it adds nothing to this checklist. */
/* The checklist. Add a rule with every feature that stores what a GameCube save can't hold (pc_gcopy.h).
 * *Settled 2026-11-15 (built, docs/port/pc-menu-anywhere.md):* "Continue where you saved" needs no rule
 * here. The exact spot lives in slot.ini, bound to the .gci's SHA-1, never in the save or the extras file, so the copy
 * (the .gci itself) already starts at the game's own return point, the last PC used, as a console does. */
/* *Settled 2026-11-20 (the autosave; docs/port/autosave.md):* the autosave needs no rule either. It never changes the
 * player's .gci or slot.ini (the copy's source); it is a separate .gci in the game's own format (Slot N\Autosave\), whose
 * spot lives beside it in autosave.ini, bound to its SHA-1 the same way, so on a GameCube or in Dolphin it starts at its
 * own return point, the last PC used. Nothing it keeps is something a GameCube save can't hold. */

/* "region" (*added 2026-11-19*, 3.5 step 9): the copy is for another version of the game than the save came from.
 * What the copy gets instead: the target's game code, its card header (call 1: the save's own original header when it
 * returns to its region, else that region's disc, else the US one), its region stamp and language bytes in every valid
 * slot (pc_gcopy_region.h). What stays in OrrePort: nothing (the save here never changes). Each Pokémon's origin stamp
 * stays as it is, since it records where that Pokémon came from (decided 2026-11-17). Two notes:
 * - "Region: <from> → <to>" whenever the region changes;
 * - "the memory card screen won't show that version's own banner and words" when the header isn't the one that game
 *   writes (no disc of that region added; or a European copy in another language than the save's, without the European
 *   disc for that language's comment). Only the memory card screen shows these (the game checks just their word sum,
 *   which is redone; save-data.md), so it never stops the copy. */
static int rule_region(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    int got = 0;
    (void)slot, (void)save;
    if (!plan->ok) return 0;
    char from[96], to[96];
    region_word(plan->own_region, plan->own_region == 'P' ? plan->own_language : 0, from, sizeof(from));
    region_word(plan->target.region, plan->target.language, to, sizeof(to));
    if (plan->target.region != plan->own_region && got < max) {
        PcGcopyNote* o = new_note(notes, &got, PC_GCOPY_CHANGES, "gcopy.note.region");
        SDL_snprintf(o->what, sizeof(o->what), "%s", pc_text("saves.region"));
        SDL_snprintf(o->kept, sizeof(o->kept), "%s", from);
        SDL_snprintf(o->copy, sizeof(o->copy), "%s", to);
    }
    if (!plan->header_exact && got < max) {
        PcGcopyNote* o = new_note(notes, &got, PC_GCOPY_CHANGES, "gcopy.note.region_card");
        SDL_snprintf(o->what, sizeof(o->what), "%s", pc_text("gcopy.what.card"));
        region_word(plan->target.region, 0, o->copy, sizeof(o->copy));
    }
    return got;
}

/* "pal_language" (*added 2026-11-19*; snatcherLeo's call 2): a European copy carries one language. The European game picks
 * its language in Options and continues a story only when the save's stamp matches it (regions.md, verified from its
 * code), so the copy is stamped with the language chosen in the window (the stamp, +0x2B and +0xB48, and its card
 * comment). The note tells the player which language to set on the console. Nothing stays behind. */
static int rule_pal_language(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    int got = 0;
    char key[32];
    (void)slot, (void)save;
    if (!plan->ok || plan->target.region != 'P' || max < 1) return 0;
    PcGcopyNote* o = new_note(notes, &got, PC_GCOPY_CHANGES, "gcopy.note.pal_language");
    SDL_snprintf(o->what, sizeof(o->what), "%s", pc_text("gcopy.what.language"));
    SDL_snprintf(key, sizeof(key), "saves.language.%d", plan->target.language);
    SDL_snprintf(o->copy, sizeof(o->copy), "%s", pc_text(key));
    return got;
}

/* "species_names" (*added 2026-11-19*; snatcherLeo's call 3): a Pokémon with no nickname holds its species' name as text, in
 * the language it was named in (regions.md: the Japanese save holds kana, PAL Spanish and Italian English names). The
 * copy gives each such Pokémon its species' name in the target's language (party, PC boxes, day care), so it reads as
 * that version's own; a nickname the player gave stays. "No nickname" is the game's own test: the name equals the
 * species' name, here in English (the US program names what it makes in English) or the save's own language. Each
 * rename is one "Changes in the copy" row. Kept in OrrePort: nothing (the save here never changes). When a language's
 * names aren't available (its disc isn't added, or still being read), a "Stays" row says which names keep their form. */
static int rule_species_names(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    int got = 0;
    char key[32];
    (void)slot, (void)save;
    if (!plan->ok) return 0;
    for (int i = 0; i < plan->renamed && i < PC_GCOPY_MAX_NOTES && got < max; i++) {
        PcGcopyNote* o = new_note(notes, &got, PC_GCOPY_CHANGES, "gcopy.note.species");
        SDL_snprintf(o->what, sizeof(o->what), "%s", pc_text("gcopy.what.no_nickname"));
        name_utf8(plan->list[i].from, 11, o->kept, sizeof(o->kept));
        name_utf8(plan->list[i].to, 11, o->copy, sizeof(o->copy));
    }
    /* the language the names would come from (to) or be recognised in (from) wasn't there. Italian and Spanish use the
     * English names (verified in the European disc's text), so a missing Italian or Spanish source changes nothing */
    const int missing = plan->names_to_missing ? plan->names_to_missing
                        : (plan->names_from_missing == 1 || plan->names_from_missing == 3 || plan->names_from_missing == 4)
                            ? plan->names_from_missing
                            : 0;
    if (missing != 0 && got < max) {
        PcGcopyNote* o = new_note(notes, &got, PC_GCOPY_STAYS,
                                  plan->names_to_missing ? "gcopy.note.species_no_target" : "gcopy.note.species_no_source");
        SDL_snprintf(o->what, sizeof(o->what), "%s", pc_text("gcopy.what.no_nickname"));
        SDL_snprintf(key, sizeof(key), "saves.language.%d", missing);
        SDL_snprintf(o->kept, sizeof(o->kept), "%s", pc_text(key));
        region_word(missing == 1 ? 'J' : 'P', 0, o->copy, sizeof(o->copy));
    }
    return got;
}

/* "region_letters" (*added 2026-11-19*): a name (the trainer's, Rui's, a box's, a nickname) holding a letter the
 * target version's font can't draw. Why a note and not a block: the save itself loads (the game checks no letters), only
 * that letter shows as a gap on that console (guess: what the game draws for a missing letter isn't traced), and the
 * player can rename in OrrePort first. A "Changes" row, since the copy reads differently there. What's known:
 * - a Japanese copy: the Japanese disc's own fonts, when it's added (pc_region_font_has, font 0: the one names are
 *   drawn in; verified 2026-11-19 that its main font 0 has ASCII, the European accented letters and kana, so this is
 *   rare); without that disc nothing is checked;
 * - a US copy: the signs only the European fonts have (° γ ○ ★ ♪ ， ～ „, regions.md, verified by comparing the fonts);
 * - a European copy: nothing (its fonts are the US ones plus those signs; only the US e-Reader marks are missing, which
 *   no name can hold: likely).
 * Kept in OrrePort: the name as it is (the save here never changes). */
static int letter_missing(char region, unsigned c) {
    static const unsigned kPalOnly[] = {0x00B0, 0x03B3, 0x25CB, 0x2605, 0x266A, 0xFF0C, 0xFF5E, 0x201E};
    if (region == 'E') {
        for (size_t i = 0; i < sizeof(kPalOnly) / sizeof(kPalOnly[0]); i++)
            if (c == kPalOnly[i]) return 1;
        return 0;
    }
    if (region == 'J') return pc_region_text_has("ja") && c != ' ' && !pc_region_font_has(0, (uint16_t)c);
    return 0;
}

static int rule_region_letters(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    PcGcopySlotName names[6 + 90 + 1 + 5];
    int got = 0;
    (void)slot, (void)save;
    if (!plan->ok || plan->target.region == plan->own_region) return 0;
    const int n = pc_gcopy_slot_names(plan->plain, names, (int)(sizeof(names) / sizeof(names[0])));
    for (int i = 0; i < n && got < max; i++) {
        int bad = 0;
        for (int u = 0; u < names[i].units; u++) {
            const unsigned c = (unsigned)names[i].text[2 * u] << 8 | names[i].text[2 * u + 1];
            if (c == 0) break;
            bad |= letter_missing(plan->target.region, c);
        }
        if (!bad) continue;
        PcGcopyNote* o = new_note(notes, &got, PC_GCOPY_CHANGES, "gcopy.note.region_letters");
        /* labelled the way the "names" rule's rows are (the hub turns these ids into "Trainer", "Rui", "Box 2"...) */
        if (names[i].kind == PC_GCOPY_NAME_TRAINER) SDL_snprintf(o->edit_id, sizeof(o->edit_id), "%s", "hero");
        else if (names[i].kind == PC_GCOPY_NAME_PARTNER) SDL_snprintf(o->edit_id, sizeof(o->edit_id), "%s", "partner");
        else if (names[i].kind == PC_GCOPY_NAME_BOX) SDL_snprintf(o->edit_id, sizeof(o->edit_id), "box%d", names[i].where);
        else SDL_snprintf(o->edit_id, sizeof(o->edit_id), "%s", "pokemon");
        name_utf8(names[i].text, names[i].units, o->kept, sizeof(o->kept));
        region_word(plan->target.region, 0, o->copy, sizeof(o->copy));
    }
    return got;
}

/* Features checked against this list that need no rule (each says why, so the list stays complete; saves.md "The
 * GameCube copy's checklist"):
 * - Infinite TMs (step 3.7, 2026-11-20; pc_bag_tweaks.cpp): no rule needed: a count that never drops is a normal
 *   GameCube save (the TM's bag count stays where the player's last purchase or pick-up left it, within the game's own
 *   99; nothing is stored outside the save).
 * - The other step 3.7 item tweaks (the party list after a use or a give, the TM design, the remembered scent cursors,
 *   READ / CHECK / TOSS in the bag): no rule needed: they change only which menu comes back; what they remember (the
 *   last scent and slot) lives in memory for the run, never in the save. */
static const struct {
    const char* id;
    GcopyRule check;
} kRules[] = {
    {"save", rule_save},
    {"names", rule_names},
    {"emoji", rule_emoji},
    {"region", rule_region},
    {"pal_language", rule_pal_language},
    {"species_names", rule_species_names},
    {"region_letters", rule_region_letters},
};

/* the rules on a plan already made */
static int check_plan(int slot, const PcSaveInfo* save, const GcopyPlan* plan, PcGcopyNote* notes, int max) {
    int n = 0;
    for (size_t i = 0; i < sizeof(kRules) / sizeof(kRules[0]) && n < max; i++) {
        const int got = kRules[i].check(slot, save, plan, notes + n, max - n);
        for (int k = 0; k < got; k++) notes[n + k].rule = kRules[i].id;
        n += got;
    }
    return n;
}

int pc_gcopy_check(int slot, const PcGcopyTarget* target, PcGcopyNote* notes, int max) {
    PcSaveInfo save;
    if (!pc_saves_get(slot, &save)) return 0;
    GcopyPlan* plan = (GcopyPlan*)calloc(1, sizeof(GcopyPlan));
    if (plan == NULL) return 0;
    plan_copy(slot, &save, target, plan);
    const int n = check_plan(slot, &save, plan, notes, max);
    plan_free(plan);
    free(plan);
    return n;
}

void pc_gcopy_file_name(int slot, const PcGcopyTarget* target, char* out, size_t size) {
    PcSaveInfo save;
    PcGcopyTarget t;
    char name[400];
    if (target != NULL) t = *target;
    else pc_gcopy_default_target(slot, &t);
    pc_saves_file_name(pc_saves_get(slot, &save) ? save.name : "", slot, name, sizeof(name));
    if (t.region != 'P' && t.region != 'J') {
        SDL_snprintf(out, size, "%s", name);
        return;
    }
    /* "<name> - Colosseum (PAL).gci": the region word before the extension (the word is translatable; the usual
     * abbreviations have no characters a file name can't hold) */
    const size_t len = strlen(name);
    const int ext = len > 4 && SDL_strcasecmp(name + len - 4, ".gci") == 0;
    SDL_snprintf(out, size, "%.*s (%s).gci", (int)(ext ? len - 4 : len), name,
                 pc_text(t.region == 'J' ? "saves.region.jpn" : "saves.region.pal"));
}

/* the copy made, the rules on it; 1 when nothing blocks (then *plan holds the bytes to write) */
static int make_plan(int slot, const PcGcopyTarget* target, PcSaveInfo* save, GcopyPlan* plan, int* note_count) {
    PcGcopyNote* notes = (PcGcopyNote*)calloc(PC_GCOPY_MAX_NOTES, sizeof(PcGcopyNote));
    if (notes == NULL || !pc_saves_get(slot, save)) {
        free(notes);
        return 0;
    }
    plan_copy(slot, save, target, plan);
    const int n = check_plan(slot, save, plan, notes, PC_GCOPY_MAX_NOTES);
    int blocked = !plan->ok;
    for (int i = 0; i < n; i++) blocked |= notes[i].blocking;
    free(notes);
    if (note_count != NULL) *note_count = n;
    return !blocked;
}

/* Is a file of this name (ignoring capitals) already in dir? Why not just SDL_GetPathInfo: Windows and macOS file
 * systems see "Nuzlocke" and "NUZLOCKE" as one name, Linux's as two; a GameCube copy written next to another on Linux
 * would then clash when that folder is copied to Windows or a Mac (the project plan's file-name rule: names that differ
 * only by capitals are numbered). Comparing case-insensitively on every OS gives the same numbering everywhere.
 * SDL_strcasecmp folds ASCII only (likely enough: accented capitals differ on Windows too, NTFS folds them, so a
 * "É" vs "é" pair could still clash there; guess, not checked). 2026-11-14. */
typedef struct {
    const char* name;
    int found;
} NameProbe;
static SDL_EnumerationResult SDLCALL probe_name(void* user, const char* dir, const char* entry) {
    NameProbe* p = (NameProbe*)user;
    (void)dir;
    if (SDL_strcasecmp(entry, p->name) == 0) {
        p->found = 1;
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}
static int name_taken(const char* dir, const char* name) {
    NameProbe p = {name, 0};
    SDL_EnumerateDirectory(dir, probe_name, &p);
    return p.found;
}

/* the planned copy's bytes into path (*changed 2026-11-19*: was SDL_CopyFile of the slot's file; now the copy made for
 * the target, which for the save's own region and nothing to rename is still the slot's file byte for byte: verified by
 * test/gcopy_region_test.c, a US save copied for the US) */
static int write_plan(int slot, const GcopyPlan* plan, const char* path, int notes) {
    if (!SDL_SaveFile(path, plan->gci, plan->len)) {
        fprintf(stderr, "[gcopy] slot %d: could not write %s: %s\n", slot, path, SDL_GetError());
        return 0;
    }
    printf("[gcopy] slot %d: GameCube copy for GC6%c%s written to %s (%d note(s); header %s; %d renamed)\n", slot,
           plan->target.region, plan->target.region == 'P' ? (plan->target.language == 3   ? " German"
                                                              : plan->target.language == 4 ? " French"
                                                              : plan->target.language == 5 ? " Italian"
                                                              : plan->target.language == 6 ? " Spanish"
                                                                                           : " English")
                                                           : "",
           path, notes,
           plan->header_source == PC_GCOPY_HEADER_OWN    ? "the save's own"
           : plan->header_source == PC_GCOPY_HEADER_DISC ? "from that region's disc"
           : plan->header_source == PC_GCOPY_HEADER_US   ? "the US one"
                                                         : "kept",
           plan->renamed);
    return 1;
}

int pc_gcopy_make(int slot, const PcGcopyTarget* target, const char* dest_dir, char* out, size_t out_size) {
    PcSaveInfo save;
    char name[400], path[1600];
    int n = 0;
    if (out != NULL && out_size > 0) out[0] = '\0';
    GcopyPlan* plan = (GcopyPlan*)calloc(1, sizeof(GcopyPlan));
    if (plan == NULL) return 0;
    if (!make_plan(slot, target, &save, plan, &n)) {
        plan_free(plan);
        free(plan);
        return 0;
    }
    pc_gcopy_file_name(slot, &plan->target, name, sizeof(name));
    const size_t dl = strlen(dest_dir);
    const char* sep = dl > 0 && (dest_dir[dl - 1] == '/' || dest_dir[dl - 1] == '\\') ? "" : "/";
    SDL_snprintf(path, sizeof(path), "%s%s%s", dest_dir, sep, name);
    /* never over an existing file, nor beside one differing only by capitals: "Nuzlocke - Colosseum (2).gci" */
    {
        char base[400], candidate[440];
        SDL_snprintf(base, sizeof(base), "%s", name);
        char* dot = strrchr(base, '.');
        if (dot != NULL) *dot = '\0';
        SDL_snprintf(candidate, sizeof(candidate), "%s", name);
        for (int k = 2; (SDL_GetPathInfo(path, NULL) || name_taken(dest_dir, candidate)) && k < 1000; k++) {
            SDL_snprintf(candidate, sizeof(candidate), "%s (%d).gci", base, k);
            SDL_snprintf(path, sizeof(path), "%s%s%s", dest_dir, sep, candidate);
        }
    }
    const int ok = write_plan(slot, plan, path, n);
    plan_free(plan);
    free(plan);
    if (ok && out != NULL) SDL_snprintf(out, out_size, "%s", path);
    return ok;
}

/* The same copy, written to the exact file the player chose in a Save As window (snatcherLeo, 2026-11-15: the folder picker
 * didn't let him name the copy). The window has already asked before replacing a file, so this writes there; ".gci" is
 * added when the typed name lacks it, since Dolphin and GameCube tools look for it. The slot's own save never changes. */
int pc_gcopy_make_at(int slot, const PcGcopyTarget* target, const char* file_path, char* out, size_t out_size) {
    PcSaveInfo save;
    char path[1600];
    int n = 0;
    if (out != NULL && out_size > 0) out[0] = '\0';
    if (file_path == NULL || file_path[0] == '\0') return 0;
    GcopyPlan* plan = (GcopyPlan*)calloc(1, sizeof(GcopyPlan));
    if (plan == NULL) return 0;
    if (!make_plan(slot, target, &save, plan, &n)) {
        plan_free(plan);
        free(plan);
        return 0;
    }
    const size_t len = strlen(file_path);
    const int has_ext = len > 4 && SDL_strcasecmp(file_path + len - 4, ".gci") == 0;
    SDL_snprintf(path, sizeof(path), "%s%s", file_path, has_ext ? "" : ".gci");
    const int ok = write_plan(slot, plan, path, n);
    plan_free(plan);
    free(plan);
    if (ok && out != NULL) SDL_snprintf(out, out_size, "%s", path);
    return ok;
}
