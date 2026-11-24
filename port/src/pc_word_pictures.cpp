/*
 * pc_word_pictures.cpp: the game's word pictures in the chosen language (language piece 4, 2026-11-15;
 * docs/port/word-pictures.md).
 *
 * What it does: many of the game's menu words are pictures, not text (PRESS START, the main menu's cards, the Options
 * labels, the bag's and the PC's headings, Battle Mode's screens). When the game's language (pc_game_text_language())
 * is German, French, Italian or Spanish and the player has added a European disc, or Japanese with a Japanese disc,
 * those pictures show that disc's own versions, read at run time from the player's disc. English, or no such disc:
 * the US pictures. Only the pictures snatcherLeo approved (batch by batch, 2026-11-15) are swapped (kApproved below). Anything
 * that can't be matched safely keeps the US picture: never a blank, never the wrong piece of a picture.
 *
 * How the discs store them (verified 2026-11-15 by read-only studies of the three discs; docs/engine/regions.md):
 *   - A menu picture is a texture (resource type 9; its resource id is the archive entry's first word, 0xNNNN1200),
 *     drawn in pieces. Each piece is a sprite record in main.dol's sprite table (the US table: 986 records of 0x18
 *     bytes; +0x08 x, y, w, h as big-endian s16, the piece's rectangle inside the texture; +0x10 the texture id).
 *     Menus find the texture by id alone (GSresFindById), so a texture that isn't loaded in the current room is not
 *     drawn.
 *   - The European disc keeps each language's pictures in its own copies of 13 menu archives (<archive>_ge / _fr /
 *     _it / _sp). A translated picture has its own id (0x15xx1200; the US disc never uses 0x15xx), and its program
 *     holds a per-language sprite table: 967 records of 0x48 bytes at file offset 0x326570 of its main.dol (an 8-byte
 *     header, then five {x, y, w, h, id} slots of 12 bytes in the order English, German, French, Italian, Spanish,
 *     then 4 bytes ending in the "next" field). The English slot equals the US record for almost every piece; its
 *     order is the US table's with 19 records fewer and a few UK changes.
 *   - The Japanese disc uses the US ids and archive layout; its pictures simply differ. Its sprite table is the US
 *     table's shape at file offset 0x2D1CB8 of its main.dol (986 records, the same order and ids; 152 rectangles
 *     differ, all on word pictures).
 *
 * So switching a picture's language switches the piece's record (where it's cut from, and how big) and the texture it
 * looks up. Three hook points in the game code (orre_hooks.h), each vanilla when nothing is registered:
 *   1. OrreHook_MenuSprite (menuSpriteBiosGetPtr, the only getter of the sprite table): a port-owned copy of the US
 *      record with the language's rectangle and texture id. Japanese ids equal the US ones, so the Japanese pieces get
 *      synthetic ids (0x7A000000 | usId >> 8) that no disc uses, so the lookup below can tell them apart.
 *   2. OrreHook_ResourceMissing (GSresFindById, when no loaded resource has the id): a language id is answered with the
 *      texture read from the language disc (once per id: seek, read, LZSS-unpack, then GStextureLoad, the game's own
 *      step after loading a texture), but only while the US texture it replaces is loaded, so "not loaded in this
 *      room, not drawn" still holds.
 *   3. OrreHook_ItemSpriteBox (_windowCreateItemSprite): a window item draws its picture into the item's own box
 *      (the US size). A language's piece is placed where that language's game places it, read from its own item
 *      table on the player's disc (place_side; *changed 2026-11-15*, before: the box scaled around its centre, which
 *      stays the fallback for an item with no matching record).
 *   4. OrreHook_CursorSpriteAt (windowCreateCursorSprite): the cursor (the main menu's glove) drawn where that
 *      language's game draws it (the same item tables; added 2026-11-15).
 *
 * Where the language's game places a piece (worked out 2026-11-15 from the three programs, read-only; verified against
 * Dolphin pictures of the European and Japanese games):
 *   - Every program has a menu item table (records of 0x1C bytes: +0 flags, +2 x, +4 y, +6 w, +8 h, +0x0A sprite,
 *     +0x0C cursor sprite, +0x18 next), in nearly the same order (US 4,818 records, European 4,829, Japanese 4,799).
 *   - The US and Japanese programs draw a piece stretched into its item's box; the Japanese items simply have boxes
 *     sized for the Japanese words.
 *   - The European program copies the record's low 4 flag bits into the window's sprite node (its
 *     _windowCreateItemSprite, 0x80107FB4, stores them at node +0x75; the US one has no such store) and its sprite
 *     drawing (0x8010BAB0) reads them: 0 = stretched into the box as in the US game; 1 = the piece at its own size,
 *     centred in the box; 2 = its own size, on the box's left and bottom edges; 3 = its own size, on the right and
 *     bottom edges. 98 European items use 1, 15 use 2, none 3. Their boxes are often larger than the US ones (the
 *     bag's 76-pixel label is a 210-pixel box there), or moved.
 *   - So OrrePort lines the language's item table up with the US one (align_items), finds the language's item for each
 *     US item that draws a swapped piece (or carries a cursor), and moves the box by the difference between the
 *     language's and the US records (so OrrePort's own moves of an item, the main menu's and the Options screen's,
 *     are kept), then aligns the piece by the flag. An axis where the language's box is in another place altogether
 *     (the European Options screen has its rows in another order) keeps OrrePort's place on that axis (same_slot).
 *
 *   5. OrreHook_StadiumHeader (fn_80060EF4, the Battle Mode stadium name header's item callback; added 2026-11-15):
 *      not from the item tables: the European game draws that header in code (0x80064078: centred on the screen,
 *      two-piece names with an 8 px gap and the "stadium" word first in German, French, Italian and Spanish), so
 *      OrrePort places the shown US items' pieces the same way (stadium_header below).
 *
 * The rule for every piece (check_built): either the US record untouched, or the language's rectangle AND texture
 * together, never one without the other. Every language's pieces are matched and checked once, at the game's first
 * sprite lookup (build_all_ready), and any piece of a word picture left unmatched is logged then, with its picture.
 *
 * Matching the European records to the US ones (worked out 2026-11-15 with a read-only script, the scratch
 * wp_align.py; 566 US records are pieces of the 26 approved pictures):
 *   - By content: a US record matches every European record whose English slot has the same rectangle and texture;
 *     when all such records agree on the language's slot, that slot is used (528 records).
 *   - kEuPairs: 36 records by hand (our own analysis, from the tables' order; each checked at run time: the European
 *     record's English slot must use the US record's texture). 20 are duplicates in the US table (menu_032 pieces)
 *     whose European copies disagree (one keeps English, one is translated); the tables' order says which is which
 *     (the sequence alignment of both tables lines them up 1:1, likely). 16 are pieces whose UK English rectangle
 *     changed by a pixel or a few (no content match), placed by their neighbours, which match exactly; three of those
 *     (US 0x011-0x013) are the duplicates of 0x07D-0x07B, whose European record the European table keeps once (likely).
 *   - Left as US (logged at run time): 0x048 (menu_009, a piece the European game no longer has) and 0x2DC
 *     (menu_047; the European table drops it and draws that screen with other pieces).
 *
 * Kept for the run: the record copies (0x18 bytes each) and every language texture once made (about 2-4 MB a
 * language, only the pictures actually shown). Not freed when the language changes: a screen still open may be drawing
 * one, and freeing it under the drawing would crash; a language is only switched on the Options screen, so at most
 * each language's few pictures are held (decided 2026-11-15).
 *
 * Accurate mode and recordings: the pictures follow the language in every mode, like the text (look only: no random
 * numbers, no change in timing; likely, the determinism test checks it). ORRE_NO_WORD_PICTURES=1 turns it off (a
 * test switch, for side-by-side pictures).
 *
 * The Japanese release look (3.5 steps 2 and 3, 2026-11-19; pc_region_look.cpp, ui/regions/jp_look.txt): in
 * Japanese with the Japanese disc, the table's "sprite" rows (pictures that aren't words: the blue Nintendo logo, Rui's
 * title picture, three sheets that differ only in colour depth) take this same path as the Japanese word pictures: their
 * entries are noted from the disc read, their records get the synthetic Japanese id, and the picture is read when first
 * drawn. They need no new hook: the boot logo is sprite record 0x1B9 using texture 0x06F71200, which menuInit registers
 * from the program's own picture, so "only while the US texture is loaded" holds for it too (verified,
 * docs/engine/regions.md). Every item that draws one of their pieces has the same box in both programs' item tables
 * (verified 2026-11-19 with a read-only script: 18 of 18 items), so their placement is unchanged. ORRE_NO_REGION_LOOK=1
 * leaves them out; ORRE_NO_WORD_PICTURES=1 leaves the word pictures out but keeps them (each switch turns off its own).
 *
 * Platforms: plain C++ (the same on Windows, Linux and macOS; verified on Windows, likely elsewhere). XD: its menus
 * have their own tables, but the engine's sprite getter and texture lookup are the same shape, so the approach (swap
 * the record, answer the missing id) likely carries over, with XD's own tables and offsets (guess until XD's European
 * disc is studied; docs/xd-insights.md).
 */
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pc_game_ui.h"
#include "pc_lowmem.h"
#include "pc_region_look.h"
#include "pc_word_pictures.h"

extern "C" {
#include "dolphin/types.h"
extern void* (*OrreHook_MenuSprite)(u32 index, const void* record);
extern void (*OrreHook_ItemSpriteBox)(u32 itemIndex, u32 spriteIndex, s16* x, s16* y, s16* width, s16* height);
extern void (*OrreHook_CursorSpriteAt)(const void* item, s16* x, s16* y);
extern void (*OrreHook_StadiumHeader)(u32 itemIndex, s32 stadium, s16* x, s16* y, s16* width, s16* height);
extern void* (*OrreHook_ResourceMissing)(u32 id);
const char* pc_game_text_language(void); /* pc_game_text.cpp: the game text's language now */
/* pc_region_text.cpp: one archive entry of a language disc, read and unpacked (std::free it); NULL on a failed read */
uint8_t* pc_region_read_entry(int disc, uint32_t fileIndex, uint32_t dataOff, uint32_t stored, int packed,
                              size_t* outSize);
}

namespace {

/* OrrePort's languages with word pictures, in this file's order; the European ones' archive suffixes and their slot in
 * the European sprite record (0 is English) */
enum { kDe, kFr, kIt, kEs, kJa, kLangCount };
const char* const kCodes[kLangCount] = {"de", "fr", "it", "es", "ja"};
const char* const kSuffix[4] = {"_ge", "_fr", "_it", "_sp"};
const int kEuSlot[4] = {1, 2, 3, 4}; /* German, French, Italian, Spanish */

/* The approved word pictures, by their archive entry names (the same name on all three discs: verified, each name is
 * one picture with one id on the US and Japanese discs). snatcherLeo approved them batch by batch (2026-11-15). Japanese: the
 * same list minus three whose Japanese pictures are the US ones (menu_055, menu_065_1, menu_071: verified identical).
 * The Japanese pictures that differ but aren't words (the Nintendo logo, Rui's outfit, a marbled background, buttons,
 * ribbons: logo_nintendo, t_vs_c4, chara_bigl074, menu004, menu_024, menu_025, menu_040) aren't here: since 2026-11-19
 * they come from the Japanese release look's table (ui/regions/jp_look.txt, pc_region_look.cpp), its "sprite" rows on
 * this file's path and its "entry" rows on the language files' (pc_lang_files.cpp). */
struct Approved {
    const char* name;
    bool japanese;
    const char* where; /* for the log and the doc: which archive(s) and screen */
};
const Approved kApproved[] = {
    {"menu_018", true, "title: PRESS START"},
    {"menu_033", true, "topmenu: the main menu's cards"},
    {"menu_031", true, "topmenu"},
    {"menu003", true, "common / field_common / fight_common"},
    {"menu005", true, "common / field_common / fight_common"},
    {"menu_009", true, "common / field_common / fight_common"},
    {"menu_012", true, "common / field_common / fight_common"},
    {"menu_048", true, "fight_common"},
    {"menu_013", true, "pcbox_pocket_menu: the bag"},
    {"menu008", true, "pcbox_pocket_menu"},
    {"menu_019", true, "pcbox_menu: the PC"},
    {"menu_037", true, "pcbox_poke_box"},
    {"menu_023", true, "pda_menu: the P*DA"},
    {"menu_029", true, "pda_menu"},
    {"menu_032", true, "cb_menu (US colosseumbattle_menu): Battle Mode"},
    {"menu_046", true, "cb_menu"},
    {"menu_051", true, "cb_menu"},
    {"menu_055", false, "cb_menu"},
    {"menu_056", true, "cb_menu"},
    {"menu_058", true, "cb_menu"},
    {"menu_065_1", false, "cb_menu"},
    {"menu_070", true, "cb_menu"},
    {"menu_047", true, "toolbattle_menu"},
    {"menu_050", true, "toolbattle_menu"},
    {"menu_071", false, "toolbattle_menu"},
    {"menu_057", true, "pokechg_menu (US pokemonchange_menu)"},
};

const Approved* approved(const char* name) {
    for (const Approved& a : kApproved)
        if (std::strcmp(a.name, name) == 0) return &a;
    return nullptr;
}

/* Where the sprite tables are in each disc's main.dol (file offsets; found 2026-11-15 by the read-only study: the
 * European one by its five-slot shape, the Japanese one as the US table's twin). Positions, not data: the tables
 * themselves are read from the player's disc each run, and checked before use (build_eu / build_jp) */
constexpr size_t kEuTableAt = 0x326570, kEuTableCount = 967, kEuRecord = 0x48;
constexpr size_t kJpTableAt = 0x2D1CB8, kJpTableCount = 986;
constexpr size_t kUsRecord = 0x18;

/* Where the menu item tables are in each language disc's main.dol (file offsets and record counts; found 2026-11-15
 * from each program's menuItemBiosGetPtr: the European table at 0x8033C028, 4,829 records; the Japanese at 0x802DC058,
 * 4,799; the US one has 4,818). Positions, not data: the tables are read from the player's disc each run and checked
 * before use (align_items) */
constexpr size_t kItemRecord = 0x1C;
constexpr size_t kEuItemsAt = 0x339028, kEuItemCount = 4829;
constexpr size_t kJpItemsAt = 0x2D9058, kJpItemCount = 4799;

/* US record -> European record, for the pieces the content match can't place (see the file comment). Our own
 * analysis of the two tables' order (2026-11-15), not disc data; each pair is checked at run time */
struct Pair {
    uint16_t us, eu;
};
const Pair kEuPairs[] = {
    /* UK English rectangles 2 px shorter (122 -> 120); 0x011-0x013 are the US table's duplicates of 0x07D-0x07B */
    {0x011, 0x06A}, {0x012, 0x069}, {0x013, 0x068}, {0x07B, 0x068}, {0x07C, 0x069}, {0x07D, 0x06A},
    /* UK English rectangles changed slightly, placed between neighbours that match exactly */
    {0x11D, 0x10A}, {0x145, 0x132}, {0x146, 0x133}, {0x279, 0x266}, {0x27A, 0x267}, {0x2BD, 0x2AA}, {0x2BE, 0x2AB},
    {0x2C6, 0x2B3}, {0x2C7, 0x2B4}, {0x354, 0x341},
    /* menu_032's duplicated pieces: which European copy is translated follows the tables' order */
    {0x19A, 0x187}, {0x19B, 0x188}, {0x19C, 0x189}, {0x25E, 0x24B}, {0x264, 0x251}, {0x35F, 0x34C}, {0x360, 0x34D},
    {0x361, 0x34E}, {0x362, 0x34F}, {0x363, 0x350}, {0x364, 0x351}, {0x365, 0x352}, {0x366, 0x353}, {0x367, 0x354},
    {0x368, 0x355}, {0x377, 0x364}, {0x378, 0x365}, {0x379, 0x366}, {0x37A, 0x367}, {0x37B, 0x368},
};

/* Japanese pieces that are pictures, not words, on an approved word picture: they keep the US piece. 0x09A is the
 * title's copyright lines (menu_018), which the Japanese disc dates 2003 (decided 2026-11-15: the Japanese copyright
 * year is a picture difference and stays US; the Japanese PRESS START itself is the same English as the US one,
 * verified on a piece-by-piece sheet). Our own analysis, by sprite index. */
const uint16_t kJpKeepUs[] = {0x09A};

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
int16_t be16s(const uint8_t* p) { return (int16_t)(p[0] << 8 | p[1]); }
void put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v; }

/* the synthetic id of a Japanese picture: the US ids all end in 0x1200 (verified on every texture of the three discs),
 * so the top 24 bits are unique, and no disc's resource id starts with 0x7A (the highest US texture id starts with
 * 0x12, the European ones with 0x15, OrrePort's own textures with 0x4F 'O') */
uint32_t jp_id(uint32_t usId) { return 0x7A000000u | (usId >> 8); }

/* one language texture OrrePort can supply: where it is on its disc, what it replaces, and once made, the texture */
struct Tex {
    int disc = 0;
    int lang = 0;
    uint32_t fileIndex = 0, dataOff = 0, stored = 0;
    bool packed = false;
    std::string archive, name;
    uint32_t usId = 0;
    void* made = nullptr; /* below 4 GB, after GStextureLoad; kept for the run */
    bool failed = false;
    bool look = false; /* a row of the Japanese release look's table, not a word picture (counted apart in the log) */
};

/* one archive entry noted during the disc read, put together when that disc is done */
struct Noted {
    std::string archive, name;
    uint32_t fileIndex, id, dataOff, stored;
    bool packed;
    bool look; /* as Tex::look */
};

/* the language swapped records, built on the game thread the first time a language is used */
struct Built {
    bool done = false;
    uint8_t* block = nullptr;        /* the copies, below 4 GB (0x18 bytes each) */
    std::vector<uint8_t*> rec;       /* by sprite index: the copy, or NULL (the US record) */
    std::vector<int32_t> recOf;      /* by sprite index: the language table's record it was copied from, or -1 */
    std::vector<int32_t> itemOf;     /* by US item: the language's matching item (build_items), or -1 */
};

std::recursive_mutex s_mutex; /* recursive: the texture lookup may come back here through the game's own lookup */
std::vector<Noted> s_noted[2];
std::vector<uint8_t> s_table[2]; /* the European and Japanese sprite tables, copied out of their main.dol */
std::vector<uint8_t> s_items[2]; /* their menu item tables (0x1C bytes a record, big-endian) */
std::vector<int32_t> s_align[2]; /* by US item: the language table's item at the same place (align_items), or -1 */
bool s_aligned[2] = {false, false}; /* align_items tried for that disc */
/* the US item table at the first menu sprite lookup (take_us_items), before OrrePort's screens move items (the main
 * menu's LOAD SAVE row, the Options screen's Language row): a language's place is the US one plus the difference
 * between the two discs' records, so OrrePort's own moves are kept */
std::vector<uint8_t> s_usItems;
std::atomic<bool> s_usItemsTaken{false}; /* take_us_items done (or nothing to take) */
std::unordered_map<uint32_t, Tex> s_tex; /* by the id the swapped records carry */
std::atomic<bool> s_ready[2] = {false, false};
Built s_built[kLangCount];
size_t s_bytes = 0; /* language textures made so far */
bool s_off = false;
bool s_scaleOnly = false; /* ORRE_WORD_PICTURES_SCALE: the old placement (pc_word_pictures_start) */

int lang_now() {
    const char* code = pc_game_text_language();
    if (code == nullptr) return -1;
    for (int i = 0; i < kLangCount; ++i)
        if (std::strcmp(code, kCodes[i]) == 0) return i;
    return -1; /* English, or the pseudo test language: the US pictures */
}

int disc_of(int lang) { return lang == kJa ? PC_WP_DISC_JP : PC_WP_DISC_EU; }

/* the US records' type: bits 12-13 of the first big-endian u16 (window_r50_80104A94_o2.c MenuSprite.type); 1 is a
 * texture piece (2 is another kind that carries no texture id), so only type 1 is ever swapped */
bool is_texture_piece(const uint8_t* rec) { return ((rec[0] >> 4) & 3) == 1; }

/* the US textures of the approved pictures that have a language version on that disc (for the language) */
std::unordered_map<uint32_t, uint32_t> lang_ids(int lang) {
    std::unordered_map<uint32_t, uint32_t> out; /* usId -> the id the swapped record carries */
    for (const auto& [id, t] : s_tex)
        if (t.lang == lang) out[t.usId] = id;
    return out;
}

/* a picture's entry name by its US texture id, for the log */
const char* picture_name(uint32_t usId) {
    for (const auto& [id, t] : s_tex)
        if (t.usId == usId) return t.name.c_str();
    return "?";
}

/* The rule every swapped piece must keep (2026-11-15, after the German title's misplaced glow, whose cause was the
 * box, not the records: verified, PRESS START's three records each take German's rectangle and texture together):
 * a piece is either the US record untouched, or a copy whose rectangle AND texture are both the language's, the texture
 * one OrrePort can supply in place of that record's own US texture, in this language. Never one without the other: a
 * language rectangle on the US texture, or the reverse, cuts the wrong part of a picture. Checked after each build;
 * a piece that breaks it goes back to US and is logged. Returns how many broke it (0 expected). */
unsigned check_built(int lang) {
    Built& b = s_built[lang];
    unsigned broken = 0;
    for (uint32_t k = 0; k < b.rec.size(); ++k) {
        uint8_t* r = b.rec[k];
        if (r == nullptr) continue;
        const uint8_t* us = static_cast<const uint8_t*>(pc_game_ui.menu_sprite(k));
        const auto t = s_tex.find(be32(r + 0x10));
        const bool ok = t != s_tex.end() && t->second.lang == lang && t->second.usId == be32(us + 0x10) &&
                        std::memcmp(r, us, 0x08) == 0 && std::memcmp(r + 0x14, us + 0x14, kUsRecord - 0x14) == 0;
        if (ok) continue;
        std::printf("[word_pictures] %s: sprite record 0x%03X (%s) broke the rule (rectangle and texture together): "
                    "it stays US\n", kCodes[lang], k, picture_name(be32(us + 0x10)));
        b.rec[k] = nullptr;
        ++broken;
    }
    return broken;
}

/* a copy of the US record k with another rectangle (12 bytes: x, y, w, h, id, big-endian) */
uint8_t* make_copy(Built& b, uint32_t k, const uint8_t* us, const uint8_t* rect8, uint32_t id) {
    uint8_t* r = b.block + (size_t)k * kUsRecord;
    std::memcpy(r, us, kUsRecord);
    std::memcpy(r + 0x08, rect8, 8);
    put32(r + 0x10, id);
    return r;
}

bool begin_build(Built& b, uint32_t n) {
    b.done = true; /* tried once; a failure leaves the US pictures for the run */
    b.rec.assign(n, nullptr);
    b.recOf.assign(n, -1);
    b.block = static_cast<uint8_t*>(pc_lowmem_alloc((size_t)n * kUsRecord));
    if (b.block == nullptr) {
        std::printf("[word_pictures] no low memory for the sprite copies: the US pictures stay\n");
        return false;
    }
    return true;
}

/* ---- the item tables: where the language's game places a piece (file comment, "Where the language's game places a
 * piece") ---- */

int16_t item_s16(const uint8_t* rec, size_t at) { return be16s(rec + at); }

/* an item's shape, the same on all three discs for the same item (verified 2026-11-15: lined up this way, 4,783 of the
 * US table's 4,818 records agree with the European table's and 4,795 with the Japanese table's): the flag byte's top 4
 * bits (selectable, last, the two drawing bits; the low 4 are the European alignment), whether it has a picture and a
 * cursor, and how far its "next" link reaches. Sprite numbers differ (the European sprite table is numbered
 * differently), and the boxes are what differs, so neither is part of it */
uint32_t item_key(const uint8_t* rec, size_t k) {
    const bool last = (rec[0] & 0x40) != 0;
    const int32_t link = last ? 0 : (int32_t)item_s16(rec, 0x18) - (int32_t)k;
    return (uint32_t)(rec[0] & 0xF0) << 24 | (uint32_t)(item_s16(rec, 0x0A) != 0) << 23 |
           (uint32_t)(item_s16(rec, 0x0C) != 0) << 22 | ((uint32_t)link & 0x3FFFFF);
}

/* Lines the language's item table up with the US one, once per disc: both are walked together; where the shapes
 * differ, the smallest skip (up to 48 records) on either side or both after which 4 shapes agree again resumes the walk
 * (the tables differ in a handful of places: the European one has one record more near 1,299 and 11 at its end, the
 * Japanese one one fewer near 1,032 and none of the US table's last 19; worked out 2026-11-15 with a read-only script,
 * whose result this reproduces). The check that these are the tables OrrePort knows: at least 90 % of the US records
 * find an item of the same shape, else nothing is placed from them (logged). */
void align_items(int disc) {
    if (s_aligned[disc]) return;
    s_aligned[disc] = true;
    s_align[disc].clear();
    const std::vector<uint8_t>& lang = s_items[disc];
    const size_t nU = s_usItems.size() / kItemRecord, nL = lang.size() / kItemRecord;
    if (nU == 0 || nL == 0) {
        if (nL != 0)
            std::printf("[word_pictures] the US item table wasn't copied at start-up: pieces keep the US boxes "
                        "(scaled)\n");
        return;
    }
    std::vector<uint32_t> ku(nU), kl(nL);
    for (size_t k = 0; k < nU; ++k) ku[k] = item_key(s_usItems.data() + k * kItemRecord, k);
    for (size_t k = 0; k < nL; ++k) kl[k] = item_key(lang.data() + k * kItemRecord, k);
    auto agree = [&](size_t a, size_t b) {
        for (size_t t = 0; t < 4; ++t) {
            if (a + t >= nU || b + t >= nL) return a + t >= nU && b + t >= nL && t > 0;
            if (ku[a + t] != kl[b + t]) return false;
        }
        return true;
    };
    std::vector<int32_t> map(nU, -1);
    size_t i = 0, j = 0;
    while (i < nU && j < nL) {
        if (ku[i] == kl[j]) {
            map[i++] = (int32_t)j++;
            continue;
        }
        bool found = false;
        for (size_t d = 1; d < 48 && !found; ++d) {
            if (agree(i + d, j + d)) { /* the same place, a changed record: paired by place (the sprite check decides) */
                for (size_t t = 0; t < d; ++t) map[i + t] = (int32_t)(j + t);
                i += d, j += d, found = true;
            } else if (agree(i + d, j)) {
                i += d, found = true;
            } else if (agree(i, j + d)) {
                j += d, found = true;
            }
        }
        if (!found) map[i++] = (int32_t)j++;
    }
    size_t same = 0;
    for (size_t k = 0; k < nU; ++k) same += map[k] >= 0 && ku[k] == kl[(size_t)map[k]];
    const char* name = disc == PC_WP_DISC_JP ? "Japanese" : "European";
    if (same < nU * 9 / 10) {
        std::printf("[word_pictures] the %s disc's item table doesn't line up with the US one (%zu of %zu records "
                    "alike): pieces keep the US boxes (scaled)\n", name, same, nU);
        return;
    }
    s_align[disc] = std::move(map);
    std::printf("[word_pictures] %s disc: item table lined up with the US one (%zu of %zu records alike)\n", name, same,
                nU);
}

/* is language item record `rec` the one drawing US sprite k's piece in this language? Japanese: the same sprite number.
 * European: the record build_eu copied (recOf), or another European record holding the same piece (the same English
 * and language slots; the European table keeps a few pieces twice) */
bool item_draws(int lang, const Built& b, uint32_t k, int32_t langSprite) {
    if (k >= b.recOf.size() || b.recOf[k] < 0 || langSprite <= 0) return false;
    if (lang == kJa) return langSprite == b.recOf[k];
    const std::vector<uint8_t>& eu = s_table[PC_WP_DISC_EU];
    const size_t m = (size_t)b.recOf[k], e = (size_t)langSprite;
    if (e == m) return true;
    if (e >= kEuTableCount) return false;
    const uint8_t* a = eu.data() + m * kEuRecord + 8;
    const uint8_t* c = eu.data() + e * kEuRecord + 8;
    const size_t slot = 12 * (size_t)kEuSlot[lang];
    return std::memcmp(a, c, 12) == 0 && std::memcmp(a + slot, c + slot, 12) == 0;
}

/* is language item record `rec` carrying US cursor sprite c (the glove)? Japanese: the same number; European: a record
 * whose English slot is the US record's piece */
bool item_cursor(int lang, uint32_t c, int32_t langCursor) {
    if (c == 0 || langCursor <= 0) return false;
    if (lang == kJa) return (uint32_t)langCursor == c;
    if ((size_t)langCursor >= kEuTableCount || pc_game_ui.menu_sprite_count == nullptr ||
        c >= pc_game_ui.menu_sprite_count())
        return false;
    const uint8_t* us = static_cast<const uint8_t*>(pc_game_ui.menu_sprite(c));
    return std::memcmp(s_table[PC_WP_DISC_EU].data() + (size_t)langCursor * kEuRecord + 8, us + 0x08, 12) == 0;
}

/* For each US item that draws a swapped piece in this language, or carries a cursor: the language's item for it. The
 * lined-up item first; if it draws something else (a few European screens list their items in another order), the one
 * item within 8 places that draws the same piece, if there is exactly one. Anything else: no item, and the piece keeps
 * the US box (scaled, the fallback). */
void build_items(int lang) {
    Built& b = s_built[lang];
    const int disc = disc_of(lang);
    align_items(disc);
    const std::vector<int32_t>& al = s_align[disc];
    const std::vector<uint8_t>& L = s_items[disc];
    const size_t nU = s_usItems.size() / kItemRecord, nL = L.size() / kItemRecord;
    b.itemOf.clear();
    if (al.size() != nU || nU == 0) return; /* logged by align_items */
    b.itemOf.assign(nU, -1);
    unsigned pieces = 0, placed = 0, cursors = 0, cursorsFound = 0;
    std::string unmatched;
    for (size_t k = 0; k < nU; ++k) {
        const uint8_t* u = s_usItems.data() + k * kItemRecord;
        const int32_t spr = item_s16(u, 0x0A), cur = item_s16(u, 0x0C);
        const bool piece = spr > 0 && (size_t)spr < b.rec.size() && b.rec[(size_t)spr] != nullptr;
        if (!piece && cur == 0) continue;
        const int32_t a = al[k];
        auto langRec = [&](int32_t t) { return L.data() + (size_t)t * kItemRecord; };
        if (piece) {
            ++pieces;
            int32_t found = -1;
            if (a >= 0 && item_draws(lang, b, (uint32_t)spr, item_s16(langRec(a), 0x0A))) {
                found = a;
            } else if (a >= 0) {
                int32_t only = -1, count = 0;
                for (int32_t t = std::max(0, a - 8); t <= std::min((int32_t)nL - 1, a + 8); ++t)
                    if (item_draws(lang, b, (uint32_t)spr, item_s16(langRec(t), 0x0A))) only = t, ++count;
                if (count == 1) found = only;
            }
            if (found >= 0) {
                b.itemOf[k] = found;
                ++placed;
            } else if (unmatched.size() < 400) {
                char buf[24];
                std::snprintf(buf, sizeof(buf), "%s%zu", unmatched.empty() ? "" : ", ", k);
                unmatched += buf;
            }
        } else {
            ++cursors;
            if (a >= 0 && item_cursor(lang, (uint32_t)cur, item_s16(langRec(a), 0x0C))) {
                b.itemOf[k] = a;
                ++cursorsFound;
            }
        }
    }
    std::printf("[word_pictures] %s: %u of %u items with a swapped piece placed as the %s game places them%s%s%s; "
                "%u of %u cursor places found\n", kCodes[lang], placed, pieces,
                lang == kJa ? "Japanese" : "European", unmatched.empty() ? "" : " (the rest keep the US box: items ",
                unmatched.c_str(), unmatched.empty() ? "" : ")", cursorsFound, cursors);
}

/* A European language: each US record on an approved picture matched to its European record (by content, or by
 * kEuPairs), then copied with that language's slot. See the file comment for the rules. */
void build_eu(int lang) {
    Built& b = s_built[lang];
    const PcGameUi& g = pc_game_ui;
    const uint32_t n = g.menu_sprite_count != nullptr ? g.menu_sprite_count() : 0;
    const std::vector<uint8_t>& eu = s_table[PC_WP_DISC_EU];
    if (n == 0 || eu.size() != kEuTableCount * kEuRecord || !begin_build(b, n)) return;
    const int slot = kEuSlot[lang];
    auto euRec = [&](size_t m) { return eu.data() + m * kEuRecord; };
    auto slotOf = [&](size_t m, int s) { return euRec(m) + 8 + 12 * (size_t)s; };
    auto us = [&](uint32_t k) { return static_cast<const uint8_t*>(g.menu_sprite(k)); };

    /* the content index: the English slot's 12 bytes -> the European records with it */
    std::unordered_map<std::string, std::vector<uint32_t>> byEnglish;
    for (size_t m = 0; m < kEuTableCount; ++m)
        byEnglish[std::string(reinterpret_cast<const char*>(slotOf(m, 0)), 12)].push_back((uint32_t)m);
    /* the check that this is the table we studied: nearly every US record finds its English slot (965 of 986 on the
     * studied disc, verified); far fewer means another table or another disc revision, and nothing is swapped */
    uint32_t found = 0;
    for (uint32_t k = 0; k < n; ++k)
        found += byEnglish.count(std::string(reinterpret_cast<const char*>(us(k) + 0x08), 12)) != 0;
    if (found < n * 9 / 10) {
        std::printf("[word_pictures] the European disc's sprite table isn't the one OrrePort knows (%u of %u records "
                    "found): the US pictures stay\n", found, n);
        return;
    }
    std::unordered_map<uint32_t, uint32_t> pairs;
    for (const Pair& p : kEuPairs) pairs[p.us] = p.eu;
    const std::unordered_map<uint32_t, uint32_t> ids = lang_ids(lang);

    uint32_t pieces = 0, swapped = 0, english = 0;
    std::string leftUs;
    for (uint32_t k = 0; k < n; ++k) {
        const uint8_t* r = us(k);
        const uint32_t usId = be32(r + 0x10);
        if (!is_texture_piece(r) || ids.count(usId) == 0) continue;
        ++pieces;
        int64_t m = -1;
        if (const auto p = pairs.find(k); p != pairs.end()) {
            if (p->second < kEuTableCount && be32(slotOf(p->second, 0) + 8) == usId) m = p->second;
        } else if (const auto c = byEnglish.find(std::string(reinterpret_cast<const char*>(r + 0x08), 12));
                   c != byEnglish.end()) {
            /* every European record with this English piece must say the same for the language */
            bool agree = true;
            for (uint32_t other : c->second) agree &= std::memcmp(slotOf(other, slot), slotOf(c->second[0], slot), 12) == 0;
            if (agree) m = c->second[0];
        }
        if (m < 0) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s0x%03X (%s)", leftUs.empty() ? "" : ", ", k, picture_name(usId));
            leftUs += buf;
            continue;
        }
        const uint8_t* s = slotOf((size_t)m, slot);
        const uint32_t id = be32(s + 8);
        if (id == usId) { /* the European game keeps the English piece here */
            ++english;
            continue;
        }
        if (s_tex.count(id) == 0) { /* a texture the disc read didn't find: keep the US piece */
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s0x%03X (%s: no 0x%08X)", leftUs.empty() ? "" : ", ", k,
                          picture_name(usId), id);
            leftUs += buf;
            continue;
        }
        b.rec[k] = make_copy(b, k, r, s, id);
        b.recOf[k] = (int32_t)m;
        ++swapped;
    }
    const unsigned broken = check_built(lang);
    std::printf("[word_pictures] %s: %u pieces of the approved pictures: %u swapped, %u where the European game keeps "
                "English, %u stay US%s%s%s\n", kCodes[lang], pieces, swapped - broken, english,
                pieces - (swapped - broken) - english, leftUs.empty() ? "" : " (left unmatched: sprite records ",
                leftUs.c_str(), leftUs.empty() ? "" : ")");
    build_items(lang);
}

/* Japanese: the Japanese table is the US table's twin (checked: the same length and every texture id equal), so record
 * k gets the Japanese rectangle and the synthetic id of its picture, for the approved pictures */
void build_jp() {
    Built& b = s_built[kJa];
    const PcGameUi& g = pc_game_ui;
    const uint32_t n = g.menu_sprite_count != nullptr ? g.menu_sprite_count() : 0;
    const std::vector<uint8_t>& jp = s_table[PC_WP_DISC_JP];
    if (n == 0 || n != kJpTableCount || jp.size() != kJpTableCount * kUsRecord) {
        if (n != 0) std::printf("[word_pictures] the Japanese disc's sprite table doesn't line up with the US one: the US "
                                "pictures stay\n");
        b.done = true;
        return;
    }
    for (uint32_t k = 0; k < n; ++k) {
        if (be32(jp.data() + k * kUsRecord + 0x10) != be32(static_cast<const uint8_t*>(g.menu_sprite(k)) + 0x10)) {
            std::printf("[word_pictures] the Japanese disc's sprite table differs from the US one at record 0x%03X: the "
                        "US pictures stay\n", k);
            b.done = true;
            return;
        }
    }
    if (!begin_build(b, n)) return;
    const std::unordered_map<uint32_t, uint32_t> ids = lang_ids(kJa);
    uint32_t pieces = 0, resized = 0, look = 0;
    std::string lookNames; /* the Japanese release look's pictures swapped, for the log */
    for (uint32_t k = 0; k < n; ++k) {
        const uint8_t* r = static_cast<const uint8_t*>(g.menu_sprite(k));
        const auto it = ids.find(be32(r + 0x10));
        if (!is_texture_piece(r) || it == ids.end()) continue;
        if (s_tex[it->second].look) {
            /* a row of the Japanese release look (not words): no kJpKeepUs piece is one of these. Counted apart */
            const uint8_t* j = jp.data() + k * kUsRecord;
            b.rec[k] = make_copy(b, k, r, j + 0x08, it->second);
            b.recOf[k] = (int32_t)k;
            ++look;
            const std::string& nm = s_tex[it->second].name;
            if (lookNames.find(nm) == std::string::npos) lookNames += (lookNames.empty() ? "" : ", ") + nm;
            continue;
        }
        bool keepUs = false;
        for (uint16_t s : kJpKeepUs) keepUs |= s == k;
        if (keepUs) {
            std::printf("[word_pictures] ja: sprite record 0x%03X (%s) is a picture, not words: it stays US\n", k,
                        picture_name(be32(r + 0x10)));
            continue;
        }
        const uint8_t* j = jp.data() + k * kUsRecord;
        b.rec[k] = make_copy(b, k, r, j + 0x08, it->second);
        b.recOf[k] = (int32_t)k; /* the Japanese table is the US one's twin */
        ++pieces;
        resized += std::memcmp(j + 0x0C, r + 0x0C, 4) != 0;
    }
    const unsigned broken = check_built(kJa);
    std::printf("[word_pictures] ja: %u pieces of the approved pictures swapped (%u of another size)%s\n",
                pieces - broken, resized, broken != 0 ? "; some stay US (above)" : "");
    if (look != 0 || !lookNames.empty())
        /* built with every language of the disc at the first sprite lookup (build_all_ready), whatever the language
         * now: drawn only while the language is Japanese */
        std::printf("[region_look] ja: %u sprite pieces of the Japanese release look ready, drawn while the language is "
                    "Japanese (%s)\n", look, lookNames.c_str());
    build_items(kJa);
}

void build(int lang) {
    /* not before the US item table is copied (take_us_items): the pieces' places are worked out with the build */
    if (!s_usItemsTaken.load(std::memory_order_acquire)) return;
    if (lang == kJa) build_jp();
    else build_eu(lang);
}

/* Every language of every disc read so far, matched and checked once, at the game's first sprite lookup in any
 * language (English included), so any piece of a word picture left unmatched is in the log before the first menu
 * draws (2026-11-15, asked after the German title's glow). Here and not when the disc is read: the US sprite table
 * is the game's data, loaded at start-up, and its record count was still 0 on the loader thread (seen in a test run).
 * A few milliseconds and 24 KB of low memory per language, once. */
std::atomic<bool> s_allBuilt[2] = {false, false};

/* The US item table as the game has it at its first menu sprite lookup (the title screen's PRESS START), before
 * OrrePort's screens move items: the main menu's LOAD SAVE row, the Options screen's Language row and the field menu
 * all move theirs when their screen first opens, later (s_usItems says why it's needed). Not at start-up
 * (pc_word_pictures_start): the table's record count is still 0 there, like the sprite table's (seen in a test run,
 * 2026-11-15). 135 KB, once. Without it the pieces keep the US boxes (scaled), as before 2026-11-15. */
void take_us_items() {
    if (s_usItemsTaken.load(std::memory_order_acquire)) return;
    const PcGameUi& g = pc_game_ui;
    if (g.menu_item_count == nullptr || g.menu_item == nullptr) { /* a test program: nothing to take */
        s_usItemsTaken.store(true, std::memory_order_release);
        return;
    }
    const uint32_t n = g.menu_item_count();
    if (n == 0) return; /* not loaded yet: the next lookup tries again, and nothing is built before */
    const auto* base = static_cast<const uint8_t*>(g.menu_item(0));
    const std::lock_guard lock(s_mutex);
    if (base != nullptr && s_usItems.empty()) s_usItems.assign(base, base + (size_t)n * kItemRecord);
    s_usItemsTaken.store(true, std::memory_order_release);
}

void build_all_ready() {
    take_us_items();
    if (!s_usItemsTaken.load(std::memory_order_acquire)) return; /* the languages are built after the copy */
    for (int disc = 0; disc < 2; ++disc) {
        if (s_allBuilt[disc].load(std::memory_order_acquire) || !s_ready[disc].load(std::memory_order_acquire)) continue;
        const std::lock_guard lock(s_mutex);
        if (s_allBuilt[disc].load() || pc_game_ui.menu_sprite_count == nullptr || pc_game_ui.menu_sprite_count() == 0)
            continue;
        if (disc == PC_WP_DISC_JP) {
            if (!s_built[kJa].done) build(kJa);
        } else {
            for (int l = kDe; l <= kEs; ++l)
                if (!s_built[l].done) build(l);
        }
        s_allBuilt[disc].store(true, std::memory_order_release);
    }
}

/* the swapped record for sprite index k in the language now, or NULL */
const uint8_t* swapped_record(uint32_t k) {
    if (!pc_game_ui.ready) return nullptr;
    build_all_ready();
    const int lang = lang_now();
    if (lang < 0 || !s_ready[disc_of(lang)].load(std::memory_order_acquire)) return nullptr;
    const std::lock_guard lock(s_mutex);
    Built& b = s_built[lang];
    if (!b.done) build(lang); /* normally done by build_all_ready already */
    return k < b.rec.size() ? b.rec[k] : nullptr;
}

/* ---- the hooks ---- */

void* menu_sprite(u32 index, const void* /*record*/) {
    return const_cast<uint8_t*>(swapped_record(index));
}

/* one side of an item's box for a piece of another size: the same scale as the US piece in its box (most items draw
 * 1:1, so the box takes the language piece's size), and the box's centre kept.
 * *Fixed 2026-11-15 (snatcherLeo's first look at the German title: "DRÜCKE START" showed its letters with the white glow
 * offset behind them):* the first version kept the box's top-left corner. PRESS START is two items, the glow (sprite
 * 0x32D, 261 x 33 at 190, 287) and the letters on it (0x09B, 247 x 18 at 197, 295), centred on each other; German's
 * glow is 5 px narrower and its letters 5 px taller, so with the corner kept they drifted 2-3 px apart (and as much in
 * French, Italian and Spanish; checked on a composite of the disc's pieces). The US layout centres such pieces on each
 * other, so the centre is what to keep (verified on that composite; likely right for buttons and labels in general,
 * the hand check confirms). A negative size (a mirrored piece) keeps its sign. */
void scale_side(s16* at, s16* size, int16_t usPiece, int16_t langPiece) {
    if (usPiece <= 0 || langPiece <= 0 || langPiece == usPiece) return;
    const int old = *size;
    const int now = (int)std::lround((double)old * langPiece / usPiece);
    *size = (s16)now;
    *at = (s16)(*at + (std::abs(old) - std::abs(now)) / 2);
}

/* do two boxes on one axis (start, size; a negative size is a mirrored piece, the start still its left / top edge) stand
 * for the same place? Each one's centre inside the other. Yes for a box widened or nudged for the language's words
 * (CONFERMA's 193-pixel box around the US 87-pixel OK: centres 106 and 92); no where the language's screen is laid out
 * differently (the European Options screen's rows in another order: 18 to 45 px apart vertically, each centre outside
 * the other box). Centres doubled to stay in whole numbers */
bool same_slot(int a0, int aSize, int b0, int bSize) {
    if ((aSize < 0) != (bSize < 0)) return false;
    const int aw = std::abs(aSize), bw = std::abs(bSize);
    const int ac2 = 2 * a0 + aw, bc2 = 2 * b0 + bw;
    return ac2 >= 2 * b0 && ac2 <= 2 * (b0 + bw) && bc2 >= 2 * a0 && bc2 <= 2 * (a0 + aw);
}

/* One axis of a piece's place, as the language's game places it (file comment). `at` / `size`: the item's box as the
 * game has it now (OrrePort may have moved it); us0 / usSize and lang0 / langSize: the US record as the game started
 * and the language's record; piece / usPiece: the language's and the US piece on this axis; mode: the European
 * alignment flag (0 for Japanese). The box moves by the records' difference where they stand for the same place
 * (same_slot), then:
 *   mode 0, stretched into the box, as the US, Japanese and European games all do for it (where the language's box
 *     isn't the same place, the old fallback: the box scaled to the piece around its centre);
 *   modes 1-3, the piece at its own size: centred (1), on the left (2 across) or right (3 across) edge, on the bottom
 *     edge (2 and 3 down); the European drawing's arithmetic (0x8010BE48-0x8010BF1C), halves rounded toward zero. */
void place_side(s16* at, s16* size, int us0, int usSize, int lang0, int langSize, int piece, int usPiece, int mode,
                bool vertical, bool sameRow) {
    const bool same = same_slot(us0, usSize, lang0, langSize) || sameRow;
    if (same) {
        *at = (s16)(*at + lang0 - us0);
        *size = (s16)(*size + langSize - usSize);
    }
    if (mode == 0) {
        if (!same) scale_side(at, size, (int16_t)usPiece, (int16_t)piece);
        return;
    }
    if (piece <= 0) return;
    const int box = std::abs((int)*size);
    const int off = mode == 1 ? (box - piece) / 2 : (mode == 2 && !vertical) ? 0 : box - piece;
    *at = (s16)(*at + off);
    *size = (s16)(*size < 0 ? -piece : piece);
}

void item_sprite_box(u32 item, u32 index, s16* x, s16* y, s16* width, s16* height) {
    const uint8_t* r = swapped_record(index);
    if (r == nullptr) return;
    const uint8_t* us = static_cast<const uint8_t*>(pc_game_ui.menu_sprite(index));
    uint8_t usItem[kItemRecord], langItem[kItemRecord];
    int mode = -1; /* -1: no language item found */
    bool european = false;
    if (!s_scaleOnly) {
        const std::lock_guard lock(s_mutex);
        const int lang = lang_now();
        if (lang >= 0) {
            const Built& b = s_built[lang];
            const int disc = disc_of(lang);
            if (item < b.itemOf.size() && b.itemOf[item] >= 0) {
                std::memcpy(usItem, s_usItems.data() + (size_t)item * kItemRecord, kItemRecord);
                std::memcpy(langItem, s_items[disc].data() + (size_t)b.itemOf[item] * kItemRecord, kItemRecord);
                /* the European flag (the low 4 bits); its drawing treats anything above 3 as 0. Japanese: 0 */
                mode = disc == PC_WP_DISC_EU ? (langItem[0] & 0x0F) : 0;
                european = disc == PC_WP_DISC_EU;
                if (mode > 3) mode = 0;
            }
        }
    }
    if (mode < 0) { /* the fallback (before 2026-11-15, every piece): the box scaled to the piece around its centre */
        scale_side(x, width, be16s(us + 0x0C), be16s(r + 0x0C));
        scale_side(y, height, be16s(us + 0x0E), be16s(r + 0x0E));
        return;
    }
    /* Across, a piece that stays on its row (the language's item has the US item's y and height) takes the language's
     * place even where same_slot refuses it (*added 2026-11-15, night*): the European summary screen's label pills
     * grow along their row for longer words (PROFIL's pill body 83 -> 201 px, its rounded end 118 px further right,
     * the line after it starting there; verified against Dolphin), boxes too far apart for the centre test but plainly
     * the same row. Down, same_slot alone decides: the European Options screen orders its rows differently. European
     * only: the Japanese placement was checked without it, and its screens are laid out differently more often */
    const bool sameRow = european && item_s16(usItem, 4) == item_s16(langItem, 4) &&
                         item_s16(usItem, 8) == item_s16(langItem, 8);
    place_side(x, width, item_s16(usItem, 2), item_s16(usItem, 6), item_s16(langItem, 2), item_s16(langItem, 6),
               be16s(r + 0x0C), be16s(us + 0x0C), mode, false, sameRow);
    place_side(y, height, item_s16(usItem, 4), item_s16(usItem, 8), item_s16(langItem, 4), item_s16(langItem, 8),
               be16s(r + 0x0E), be16s(us + 0x0E), mode, true, false);
}

/* the cursor's place for the language: moved by the difference between the language's item and the US one, on each
 * axis where they stand for the same place (same_slot). Of all cursor items only the main menu's story and battle
 * gloves differ on the European disc (verified 2026-11-15 by comparing the tables: New Game 5 px left, Continue 7,
 * Colosseum Battle 17 right, Battle Now 14 left); the Japanese disc moves the same four and a few others (whose boxes
 * are elsewhere altogether, so same_slot leaves them). The item is found by its place in the table; an OrrePort item
 * past the table's end isn't the game's and stays */
void cursor_at(const void* item, s16* x, s16* y) {
    if (!pc_game_ui.ready || item == nullptr || pc_game_ui.menu_item == nullptr) return;
    build_all_ready();
    const int lang = lang_now();
    if (lang < 0 || !s_ready[disc_of(lang)].load(std::memory_order_acquire)) return;
    const std::lock_guard lock(s_mutex);
    Built& b = s_built[lang];
    if (!b.done) build(lang);
    const auto* base = static_cast<const uint8_t*>(pc_game_ui.menu_item(0));
    if (base == nullptr) return;
    const auto* p = static_cast<const uint8_t*>(item);
    if (p < base || (size_t)(p - base) % kItemRecord != 0) return;
    const size_t k = (size_t)(p - base) / kItemRecord;
    if (k >= b.itemOf.size() || b.itemOf[k] < 0) return;
    const uint8_t* u = s_usItems.data() + k * kItemRecord;
    const uint8_t* l = s_items[disc_of(lang)].data() + (size_t)b.itemOf[k] * kItemRecord;
    if (same_slot(item_s16(u, 2), item_s16(u, 6), item_s16(l, 2), item_s16(l, 6)))
        *x = (s16)(*x + item_s16(l, 2) - item_s16(u, 2));
    if (same_slot(item_s16(u, 4), item_s16(u, 8), item_s16(l, 4), item_s16(l, 8)))
        *y = (s16)(*y + item_s16(l, 4) - item_s16(u, 4));
}

/* ---- The Battle Mode stadium name header (VS and results screens) ----
 * The US game shows the header as one or two items per stadium, each with its own piece (the name, and for most
 * stadiums the "COLOSSEUM" word after it), placed by the items' boxes; the item callback (fn_80060EF4) shows the items
 * of the chosen stadium. The European game lays it out in code instead (0x80064078, called from the callback of one
 * item, 0xBF5 / 0x109B, at x 0, y 41, height 33 in its table; worked out 2026-11-15 from its disassembly, read-only):
 *   - the pieces are taken in the language's own sizes (its per-language sprite slots);
 *   - in German, French, Italian and Spanish (its language numbers 3-6), a two-piece name is drawn with the "stadium"
 *     word first (the French word is "COLOSSEUM DE", the German one-piece "STADION PHENAC");
 *   - the whole name is centred on the 640-pixel screen, the two pieces 8 pixels apart (halves rounded toward zero);
 *   - each piece's bottom on the anchor item's bottom (y 41 + 33); the Mt. Battle header (shown alone in Mt. Battle
 *     mode) at the anchor's top instead.
 * Verified against the disassembly; the pixel places checked on Dolphin pictures of the European game (word-pictures.md,
 * "Verified"). OrrePort places each shown US item's piece there (OrreHook_StadiumHeader). Japanese draws the header as
 * the US game does (its own item boxes already fit, verified), so only the European languages are placed here.
 *
 * Which pieces make each stadium's name: our own analysis of the two programs (the US callback's stadium numbers and
 * the European code's table, by US sprite number); not disc data. Stadium 5 shows stadium 3's items (both programs).
 * Stadium 6 isn't here: the US game shows two names on top of each other there (MT. BATTLE and DEEP, each with
 * COLOSSEUM) where the European one shows the Mt. Battle header; no screen was seen to use it, so it stays vanilla. */
struct HeaderName {
    int32_t stadium;
    uint16_t name, word; /* US sprite numbers; word 0: a one-piece name */
};
const HeaderName kHeaderNames[] = {
    {-1, 735, 0},   /* MT. BATTLE - VS 100 (Mt. Battle mode) */
    {0, 736, 0},    /* PHENAC STADIUM */
    {1, 742, 738},  /* PYRITE + COLOSSEUM */
    {2, 739, 738},  /* UNDER + COLOSSEUM */
    {3, 740, 738},  /* ORRE + COLOSSEUM (stadiums 3 and 5) */
    {4, 750, 738},  /* TOWER + COLOSSEUM */
};
/* the European layout's numbers (its code and its anchor item record; see above) */
constexpr int kHeaderScreen = 640, kHeaderGap = 8, kHeaderTop = 41, kHeaderBottom = 41 + 33;

void stadium_header(u32 item, s32 stadium, s16* x, s16* y, s16* width, s16* height) {
    const int lang = lang_now();
    if (lang < 0 || lang == kJa || pc_game_ui.menu_item == nullptr) return;
    const auto* rec = static_cast<const uint8_t*>(pc_game_ui.menu_item((int32_t)item));
    if (rec == nullptr) return;
    const int32_t sprite = item_s16(rec, 0x0A);
    const HeaderName* h = nullptr;
    for (const HeaderName& n : kHeaderNames)
        if (n.stadium == stadium && (sprite == n.name || (n.word != 0 && sprite == n.word))) h = &n;
    if (h == nullptr) return; /* not a header piece OrrePort knows: the item's own place */
    /* both pieces in the language (the rule: rectangle and texture together); else the US header stays as it is */
    const uint8_t* name = swapped_record(h->name);
    const uint8_t* word = h->word != 0 ? swapped_record(h->word) : nullptr;
    if (name == nullptr || (h->word != 0 && word == nullptr)) return;
    const uint8_t* first = word != nullptr ? word : name; /* de / fr / it / es: the "stadium" word first */
    const uint8_t* second = word != nullptr ? name : nullptr;
    const int w1 = be16s(first + 0x0C), w2 = second != nullptr ? be16s(second + 0x0C) : 0;
    const int total = second != nullptr ? w1 + kHeaderGap + w2 : w1;
    const int x1 = (kHeaderScreen - total) / 2; /* C's division rounds toward zero, as the European code does */
    const bool isFirst = (sprite == h->name) == (second == nullptr);
    const uint8_t* piece = isFirst ? first : second;
    const int pw = be16s(piece + 0x0C), ph = be16s(piece + 0x0E);
    *x = (s16)(isFirst ? x1 : x1 + w1 + kHeaderGap);
    *y = (s16)(stadium < 0 ? kHeaderTop : kHeaderBottom - ph);
    *width = (s16)pw;
    *height = (s16)ph;
    /* once per language, stadium and piece, for the test runs' check */
    static std::unordered_set<uint32_t> logged;
    const uint32_t key = (uint32_t)lang << 24 | (uint32_t)(stadium & 0xFF) << 16 | (uint32_t)sprite;
    const std::lock_guard lock(s_mutex);
    if (logged.insert(key).second)
        std::printf("[word_pictures] %s: stadium header %d, sprite %d (item %u) at x %d, y %d, %d x %d (the European "
                    "layout)\n", kCodes[lang], (int)stadium, (int)sprite, (unsigned)item, *x, *y, pw, ph);
}

void* resource_missing(u32 id) {
    const uint32_t top = id >> 24;
    if (top != 0x15 && top != 0x7A) return nullptr; /* not a language texture: "not found", at once */
    const std::lock_guard lock(s_mutex);
    const auto it = s_tex.find(id);
    if (it == s_tex.end()) return nullptr;
    Tex& t = it->second;
    if (!s_ready[t.disc].load(std::memory_order_acquire)) return nullptr;
    /* only while the US texture it replaces is loaded (the game's own lookup; an id of ours never comes back here) */
    if (pc_game_ui.resource_find == nullptr || pc_game_ui.resource_find(t.usId) == nullptr) return nullptr;
    if (t.made != nullptr || t.failed) return t.made;
    t.failed = true; /* until it works: one try per texture, a failed read isn't retried every frame */
    size_t size = 0;
    uint8_t* file = pc_region_read_entry(t.disc, t.fileIndex, t.dataOff, t.stored, t.packed ? 1 : 0, &size);
    if (file == nullptr || size < 0x40) {
        std::printf("[word_pictures] %s %s: couldn't be read from the %s disc: the US picture stays\n", kCodes[t.lang],
                    t.name.c_str(), t.disc == PC_WP_DISC_JP ? "Japanese" : "European");
        std::free(file);
        return nullptr;
    }
    /* below 4 GB (the game keeps the picture's address in 32-bit fields) and 32-byte aligned, as the game's own
     * texture loads are (floorReadTexPreFunc) */
    const size_t alloc = (size + 0x1F) & ~(size_t)0x1F;
    auto* mem = static_cast<uint8_t*>(pc_lowmem_alloc(alloc));
    if (mem == nullptr) {
        std::printf("[word_pictures] no low memory for %s %s: the US picture stays\n", kCodes[t.lang], t.name.c_str());
        std::free(file);
        return nullptr;
    }
    std::memcpy(mem, file, size);
    std::free(file);
    const unsigned texW = (unsigned)(mem[0] << 8 | mem[1]), texH = (unsigned)(mem[2] << 8 | mem[3]);
    if (pc_game_ui.texture_load == nullptr || pc_game_ui.texture_load(mem) == nullptr) {
        pc_lowmem_free(mem);
        return nullptr;
    }
    t.made = mem;
    t.failed = false;
    s_bytes += alloc;
    std::printf("[word_pictures] %s %s: texture 0x%08X (%ux%u) from %s, in place of 0x%08X (%zu KB of language "
                "pictures held)\n", kCodes[t.lang], t.name.c_str(), id, texW, texH, t.archive.c_str(), t.usId,
                s_bytes / 1024);
    return t.made;
}

} // namespace

extern "C" void pc_word_pictures_start(void) {
    /* the Japanese release look's table (3.5 steps 2 and 3): read here, before the language discs are, since the disc
     * read asks it which entries to note */
    pc_region_look_start();
    if (SDL_getenv("ORRE_NO_WORD_PICTURES") != nullptr) {
        s_off = true;
        std::printf("[word_pictures] off (ORRE_NO_WORD_PICTURES): the US word pictures in every language\n");
        /* the Japanese release look's sprite rows still take this path (its own switch is ORRE_NO_REGION_LOOK): only
         * the record and texture hooks, so nothing else moves (the cursor and the stadium header stay as the US game
         * places them) */
        if (!pc_region_look_off()) {
            OrreHook_MenuSprite = menu_sprite;
            OrreHook_ItemSpriteBox = item_sprite_box;
            OrreHook_ResourceMissing = resource_missing;
        }
        return;
    }
    /* ORRE_WORD_PICTURES_SCALE=1 (a test switch, for before / after pictures): the language's pieces in the US boxes
     * scaled around their centres and the cursor at the US place, as before 2026-11-15, instead of where the
     * language's game places them */
    s_scaleOnly = SDL_getenv("ORRE_WORD_PICTURES_SCALE") != nullptr;
    if (s_scaleOnly) std::printf("[word_pictures] ORRE_WORD_PICTURES_SCALE: pieces in the US boxes (scaled)\n");
    OrreHook_MenuSprite = menu_sprite;
    OrreHook_ItemSpriteBox = item_sprite_box;
    if (!s_scaleOnly) OrreHook_CursorSpriteAt = cursor_at;
    if (!s_scaleOnly) OrreHook_StadiumHeader = stadium_header;
    OrreHook_ResourceMissing = resource_missing;
}

extern "C" void pc_word_pictures_reset(void) {
    const std::lock_guard lock(s_mutex);
    s_ready[0].store(false);
    s_ready[1].store(false);
    s_allBuilt[0].store(false);
    s_allBuilt[1].store(false);
    for (auto& v : s_noted) v.clear();
    for (auto& v : s_table) v.clear();
    for (auto& v : s_items) v.clear();
    for (auto& v : s_align) v.clear();
    s_aligned[0] = s_aligned[1] = false; /* s_usItems stays: the game's own table as it started */
    /* the textures made so far stay allocated (nothing made before the game starts, when rescans happen; and a made
     * texture may still be drawn), only forgotten */
    s_tex.clear();
    for (Built& b : s_built) {
        pc_lowmem_free(b.block); /* the record copies: nothing points into them before the game starts */
        b = Built{};
    }
}

extern "C" void pc_word_pictures_note_entry(int disc, const char* archive, uint32_t fileIndex, const char* name,
                                            uint32_t id, uint32_t dataOff, uint32_t stored, int packed) {
    if (disc < 0 || disc > 1 || archive == nullptr || name == nullptr) return;
    const Approved* a = s_off ? nullptr : approved(name);
    const bool word = a != nullptr && (disc != PC_WP_DISC_JP || a->japanese);
    /* a "sprite" row of the Japanese release look's table (pc_region_look_how says no when ORRE_NO_REGION_LOOK) */
    const bool look = !word && disc == PC_WP_DISC_JP && pc_region_look_how('J', name) == PC_RL_SPRITE;
    if (!word && !look) return;
    const std::lock_guard lock(s_mutex);
    s_noted[disc].push_back(Noted{archive, name, fileIndex, id, dataOff, stored, packed != 0, look});
}

extern "C" void pc_word_pictures_note_dol(int disc, const uint8_t* dol, size_t size) {
    if (disc < 0 || disc > 1 || dol == nullptr) return;
    const size_t at = disc == PC_WP_DISC_EU ? kEuTableAt : kJpTableAt;
    const size_t bytes = disc == PC_WP_DISC_EU ? kEuTableCount * kEuRecord : kJpTableCount * kUsRecord;
    if (at + bytes > size) return;
    const std::lock_guard lock(s_mutex);
    s_table[disc].assign(dol + at, dol + at + bytes);
    /* the menu item table too (where that game places each piece and cursor; checked by align_items) */
    const size_t itemsAt = disc == PC_WP_DISC_EU ? kEuItemsAt : kJpItemsAt;
    const size_t itemBytes = (disc == PC_WP_DISC_EU ? kEuItemCount : kJpItemCount) * kItemRecord;
    if (itemsAt + itemBytes <= size) s_items[disc].assign(dol + itemsAt, dol + itemsAt + itemBytes);
}

/* ---- OrrePort's own pictures made from the game's (the LOAD button, pc_title_word.cpp / pc_title_loadsave.cpp) ----
 * They follow the language like the game's pictures do, so they read the same index: the language's texture entry, read
 * from its disc with nod's own handle (pc_region_read_entry) on the caller's thread (the main menu's open; a read of
 * 0.1-0.2 MB). Nothing is cached here: the caller keeps what it makes from it. */
namespace {
int lang_index(const char* code) {
    if (code == nullptr) return -1;
    for (int i = 0; i < kLangCount; ++i)
        if (std::strcmp(code, kCodes[i]) == 0) return i;
    return -1;
}
uint8_t* read_tex(const Tex& t, size_t* size) {
    if (size != nullptr) *size = 0;
    return pc_region_read_entry(t.disc, t.fileIndex, t.dataOff, t.stored, t.packed ? 1 : 0, size);
}
} // namespace

extern "C" uint8_t* pc_word_pictures_read(const char* lang, const char* name, size_t* size) {
    const int l = lang_index(lang);
    if (s_off || l < 0 || name == nullptr || !s_ready[disc_of(l)].load(std::memory_order_acquire)) return nullptr;
    Tex found;
    {
        const std::lock_guard lock(s_mutex);
        const Tex* t = nullptr;
        for (const auto& [id, tex] : s_tex)
            if (tex.lang == l && tex.name == name) t = &tex;
        if (t == nullptr) return nullptr;
        found = *t;
    }
    return read_tex(found, size);
}

extern "C" uint8_t* pc_word_pictures_piece(const char* lang, uint32_t k, int16_t rect[4], size_t* size) {
    const int l = lang_index(lang);
    if (s_off || l < 0 || !pc_game_ui.ready || !s_ready[disc_of(l)].load(std::memory_order_acquire)) return nullptr;
    build_all_ready();
    Tex found;
    {
        const std::lock_guard lock(s_mutex);
        Built& b = s_built[l];
        if (!b.done) build(l);
        const uint8_t* r = k < b.rec.size() ? b.rec[k] : nullptr;
        if (r == nullptr) return nullptr; /* the US piece in this language */
        const auto t = s_tex.find(be32(r + 0x10));
        if (t == s_tex.end()) return nullptr;
        for (int i = 0; i < 4; ++i) rect[i] = be16s(r + 0x08 + 2 * i);
        found = t->second;
    }
    return read_tex(found, size);
}

extern "C" void pc_word_pictures_disc_done(int disc) {
    if (disc < 0 || disc > 1) return;
    const std::lock_guard lock(s_mutex);
    unsigned per[kLangCount] = {};
    if (disc == PC_WP_DISC_EU) {
        /* each approved picture's US id: its entry whose id isn't a translated one (the names are unique per picture) */
        std::unordered_map<std::string, uint32_t> usIds;
        for (const Noted& e : s_noted[disc])
            if ((e.id >> 24) != 0x15 && usIds.count(e.name) == 0) usIds[e.name] = e.id;
        for (const Noted& e : s_noted[disc]) {
            if ((e.id >> 24) != 0x15 || e.archive.size() < 3) continue;
            int lang = -1;
            for (int l = 0; l < 4; ++l)
                if (e.archive.compare(e.archive.size() - 3, 3, kSuffix[l]) == 0) lang = l;
            const auto us = usIds.find(e.name);
            if (lang < 0 || us == usIds.end() || s_tex.count(e.id) != 0) continue; /* one copy per id (the copies of one id
                                                                                       in several archives are identical,
                                                                                       verified) */
            Tex t;
            t.disc = disc, t.lang = lang, t.fileIndex = e.fileIndex, t.dataOff = e.dataOff, t.stored = e.stored;
            t.packed = e.packed, t.archive = e.archive, t.name = e.name, t.usId = us->second;
            s_tex[e.id] = t;
            ++per[lang];
        }
        std::printf("[word_pictures] European disc: approved pictures found: de %u, fr %u, it %u, es %u (of %zu); sprite "
                    "table %s\n", per[kDe], per[kFr], per[kIt], per[kEs], sizeof(kApproved) / sizeof(kApproved[0]),
                    s_table[disc].empty() ? "not found (no pictures swapped)" : "read");
    } else {
        unsigned look = 0;
        for (const Noted& e : s_noted[disc]) {
            const uint32_t id = jp_id(e.id);
            if (s_tex.count(id) != 0) continue; /* one copy per id (the Japanese copies of one entry in several archives
                                                   are identical: verified 2026-11-19 for the look's menu_024) */
            Tex t;
            t.disc = disc, t.lang = kJa, t.fileIndex = e.fileIndex, t.dataOff = e.dataOff, t.stored = e.stored;
            t.packed = e.packed, t.archive = e.archive, t.name = e.name, t.usId = e.id, t.look = e.look;
            s_tex[id] = t;
            ++(e.look ? look : per[kJa]);
        }
        size_t want = 0;
        for (const Approved& a : kApproved) want += a.japanese;
        std::printf("[word_pictures] Japanese disc: approved pictures found: %u (of %zu); Japanese release look "
                    "pictures (sprite rows) found: %u; sprite table %s\n", per[kJa], s_off ? (size_t)0 : want, look,
                    s_table[disc].empty() ? "not found (no pictures swapped)" : "read");
    }
    s_noted[disc].clear();
    s_noted[disc].shrink_to_fit();
    /* every language of this disc is matched and checked at the game's first sprite lookup (build_all_ready), not here:
     * the US sprite table is the game's data, loaded from the US disc at start-up, and may not be there yet while this
     * loader thread runs (seen in a test run: its record count was still 0 here) */
    if (!s_table[disc].empty()) s_ready[disc].store(true, std::memory_order_release);
}
