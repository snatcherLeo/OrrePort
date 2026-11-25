/*
 * pc_game_ui.c (PC only, not in configure.py): the game side of port/src/pc_game_ui.h. Fills the table of game
 * functions and data that OrrePort's own parts of the game's screens use (the Options screen's Language row first).
 * Why: port/ libraries are also linked into test programs with no game code, so they reach the game through this
 * table, filled at start-up (the same way pc_debug_game.c fills the debug toolkit's). Compiled with the game units by
 * port/tools/firstlink.py (PORT_GAME_SOURCES). 2026-11-13. Platforms: all the same.
 */
#include "dolphin/types.h"
#include "../../port/src/pc_game_ui.h"
#include "game/name_entry_arg.h" /* NAME_ENTRY_ARG (the naming keyboard's view of the name-entry block) */
#include "game/constants/pokemon_status.h" /* POKEMON_STATUS_SPECIES (Smooth Party UI's party_face_key) */

/* the game's own (signatures in pc/prototypes.h where it has them) */
extern void* menuItemBiosGetPtr(s32 idx);
extern void* menuItemBiosSetXY(s32 idx, s16 x, s16 y);
extern void* menuSpriteBiosGetPtr(u32 idx);
extern void* GSresFindById(u32 id);
/* declared here with their real types: without a prototype C would read the returned pointer as an int and sign-extend
 * it (the first test crashed writing to 0xffffffff80c656c0, 2026-11-13) */
struct GStextureHandle;
extern void* GStextureLockImage(struct GStextureHandle* tex, u8 level);
extern void GStextureUnlockImage(struct GStextureHandle* tex);
extern void windowDrawSprite(s16 x, s16 y, void* context, u32 spriteId, u32 flags);
extern u8 lbl_803A1FC8[];
extern u8 lbl_802E4EF0[];
extern void* pc_msg_group_of(u32 key);

static void* item_get(int32_t index) { return menuItemBiosGetPtr(index); }
static void item_set_xy(int32_t index, int16_t x, int16_t y) { menuItemBiosSetXY(index, x, y); }
/* the game's own sprite record, never a language's (OrreHook_MenuSprite, language piece 4, 2026-11-15): OrrePort's
 * screens copy a record and point it at their own US-based texture (LOAD SAVE, the SAVE FILES list, the Language row),
 * where a language's rectangle would cut the wrong piece; the word pictures themselves need the US records to build
 * their copies. Past the table's end: the getter as before (OrrePort's own records). All platforms */
extern u8 _menu_sprite[];
extern u32 _menu_sprite_number;
static void* sprite_get(uint32_t index)
{
    return index < _menu_sprite_number ? (void*)(_menu_sprite + index * 0x18) : menuSpriteBiosGetPtr(index);
}
static uint32_t sprite_count(void) { return _menu_sprite_number; }
/* the item table's record count (menu_bios_range_8005D7F8.c checks indices against it; the word pictures copy the
 * table at the first menu sprite lookup, before OrrePort's screens move items). All platforms */
extern u32 lbl_80478968;
static uint32_t item_count(void) { return lbl_80478968; }
/* a texture file read from a language disc made ready to draw, exactly as the game does after loading one from an
 * archive (floorReadTexPostFunc: GStextureLoad relocates its picture offsets and builds its GX objects) */
extern struct GStextureHandle* GStextureLoad(struct GStextureHandle* tex);
static void* texture_load(void* file) { return GStextureLoad((struct GStextureHandle*)file); }
static void* texture_create(int32_t width, int32_t height, int32_t format)
{
    return GStextureCreate(width, height, format, 0, 0);
}
static void* texture_lock(void* texture) { return GStextureLockImage(texture, 0); }
static void texture_unlock(void* texture) { GStextureUnlockImage(texture); }
static void* resource_find(uint32_t id) { return GSresFindById(id); }
static void resource_register(void* data, uint32_t group, uint32_t id)
{
    GSresRegisterResource(data, group, id, (void*)0); /* no release callback: OrrePort's texture lives for the run */
}
static void draw_sprite(int16_t x, int16_t y, void* window, uint32_t sprite) { windowDrawSprite(x, y, window, sprite, 0); }
static void* msg_group_of(uint32_t key) { return pc_msg_group_of(key); }

/* for OrrePort's own menus (the SAVE FILES list, 2026-11-14). Prototypes as in pc/prototypes.h, declared here
 * because this file doesn't include it */
extern s32 menuOpenCustom(u32 menu_id, u32 parent_id, s32* cursor_out, u32 close_flags, u8 check_cursor, s32 open_param, ...);
extern u32 windowGetActiveID(void);
extern void* windowGetKeyInfo();
extern void menuButtonNormal();
extern u32 soundStart(u32 id, u32 arg1, u32 volume, u32 arg3);
extern s32 GSmsgPrintColor(s32 x, s32 y, s32 color, u32 key);
extern s32 GSmsgGetRect(u32 key);
extern void fn_800FE38C(s32 x, s32 y, s32 w, s32 h);
extern void fn_800FE35C(void);
static int32_t menu_open_wait(uint32_t menuId, uint32_t parentId) { return menuOpenCustom(menuId, parentId, (s32*)0, 0, 1, 0); }
extern s32 menuCloseCustom(u32 id, u32 mode, u8 wait);
static void menu_close(uint32_t menuId) { menuCloseCustom(menuId, 0, 1); } /* as titleMenuMain closes its own */
static uint32_t window_active_id(void) { return windowGetActiveID(); }
static const uint8_t* window_keys(void) { return (const uint8_t*)windowGetKeyInfo(); }
static void menu_button_normal(void* window) { menuButtonNormal(window); }
static void sound(uint32_t id) { soundStart(id, 0, 0xFF, 0); } /* as the bag's own sounds */
static int32_t msg_print(int32_t x, int32_t y, int32_t color, uint32_t key) { return GSmsgPrintColor(x, y, color, key); }
static int32_t msg_rect(uint32_t key) { return GSmsgGetRect(key); }
static void clip_set(int32_t x, int32_t y, int32_t w, int32_t h) { fn_800FE38C(x, y, w, h); }
static void clip_reset(void) { fn_800FE35C(); }

/* a window's record by id (window_exact_80104704.c; pc/prototypes.h declares it unprototyped, so typed here) */
extern void* windowSearchID(s32 id);
static uint8_t* window_find(uint32_t windowId) { return (uint8_t*)windowSearchID((s32)windowId); }

/* the title's own card load, as its state 0xC8 does it (gs_title_8001EF78.c, std021_newGameIntroSequence): memory-card
 * task 1 (-1 = aborted: nothing changed; 3 = a save was loaded), any other result an empty save (savedataCreate(0,
 * 0): a new game's starting state), then the main menu's default glove (fn_800216E0: Continue when a save exists).
 * For the SAVE FILES list (plan step 7b): after OrrePort points the card at the picked save's folder, the game reads
 * it the same way it did at the title. Must run in a game task that may wait (the main menu's: memcardExecTask
 * yields until the card task finishes). Returns -1 aborted, 1 a save loaded, 0 empty. */
extern s32 memcardExecTask(s32 task_kind, s32 card_work_size, s32 card_channel);
extern void savedataCreate(s32 save, s32 name); /* as gs_title_8001EF78.c declares it: (0, 0) */
extern void fn_800216E0(u32 value);
struct GameDataSave;
extern u32 gamedatasaveGetStatus(struct GameDataSave* ptr, u16 kind); /* as pc/prototypes.h (reached via types.h) */
static int32_t card_load_title(void)
{
    const s32 status = memcardExecTask(1, 2, 0);
    u32 hasSave;
    if (status == -1) return -1;
    if (status != 3) savedataCreate(0, 0);
    hasSave = gamedatasaveGetStatus((struct GameDataSave*)0, 4) != 0; /* the save count (kind 4): 0 for an empty save */
    fn_800216E0(hasSave ? 1 : 0);
    return hasSave ? 1 : 0;
}

/* ---- for the SAVE FILES list's help box (2026-11-14; docs/port/title-load-save.md step 6) ---- */

/* the game's own place name for a room, as plain text: the floor record's place-name message (+0x24,
 * floorGetPlaceName: the same name a Pokémon's summary shows for where it was met), read through the game's message
 * lookup (so the chosen language, and later a mod's renamed place, come back). "" when the room has none. */
extern s32 floorGetPlaceName(s32 floorId);
extern void pc_msg_text_utf8(u32 key, char* out, size_t cap); /* port/src/pc_event_log.c */
static void place_text(uint32_t floorId, char* out, size_t cap)
{
    const s32 key = floorId != 0 ? floorGetPlaceName((s32)floorId) : 0;
    if (cap == 0) return;
    out[0] = '\0';
    if (key != 0) pc_msg_text_utf8((u32)key, out, cap);
}

/* the Continue screen's two Shadow Pokémon counts (gs_title.c: darkPokemonCountFlag12 = snagged, the Shadow record's
 * +0x12 flag; darkPokemonCountFlag14 = purified, its +0x14 flag; shadow-system.md) for a save that isn't loaded: the
 * same walk over the game's own Shadow table (lbl_80478F6C, 0x38-byte big-endian records, ids 1 .. count - 1, as
 * battle_main.c), each flag tested in the given story-flag words (class 3) at the bit the game packed it to
 * (lbl_80478F9C, 8-byte definitions: byte 0 class and width, +4 the bit position; gs_flag_test_exact_801902E0.c).
 * Why not swap the game's own buffer and call darkPokemonCountFlag12 / 14: that would touch live game state; this reads
 * only. A flag of another class (none expected) isn't counted. */
extern u8* lbl_80478F6C;
extern u8* lbl_80478F9C;
extern u32 lbl_80478F68;
static void shadow_counts(const uint32_t* storyFlags, uint32_t words, uint32_t* snagged, uint32_t* purified)
{
    const u32 count = *(u32*)((u8*)PC_PTR(lbl_80478F68) + 0x0); /* as darkPokemonCountFlag12 / 14 read it */
    u32 i;
    *snagged = 0;
    *purified = 0;
    for (i = 1; i < count && i <= 0x60; i++) { /* darkPokemonCheckFlag12 / 14 treat ids past 0x60 as none */
        const u8* rec = lbl_80478F6C + i * 0x38;
        const u16 flags[2] = {(u16)(rec[0x12] << 8 | rec[0x13]), (u16)(rec[0x14] << 8 | rec[0x15])};
        int k;
        for (k = 0; k < 2; k++) {
            const u8* def;
            u32 bit;
            if (flags[k] == 0) continue;
            def = lbl_80478F9C + (u32)flags[k] * 8;
            if ((def[0] & 0xC0) >> 6 != 3) continue;
            bit = (u32)(def[4] << 8 | def[5]);
            if ((bit >> 5) < words && ((storyFlags[bit >> 5] >> (bit & 31)) & 1) != 0) {
                if (k == 0) ++*snagged;
                else ++*purified;
            }
        }
    }
}

/* for the naming keyboard (2026-11-14; port/src/pc_name_keyboard.cpp). Text printed and measured through the
 * name screen's own one-letter template (message 0xCE, which prints insert 0x37), the way the game's grid prints each
 * letter (menuNameEntryDraw50Text), so OrrePort's grid gets the screen's font, size and fade; `text` is big-endian
 * UTF-16 like every game string in memory, ended by 0. The screen's maximum length for a mode, read exactly as the
 * game's typing code reads the per-mode table (lbl_80266DD8, native on the PC). A game message's raw text (the space
 * letter is message 0x2EF9's first). All platforms. */
extern u8 lbl_80266DD8[];
static int32_t print_gs(int32_t x, int32_t y, uint32_t color, const void* text)
{
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(text));
    return GSmsgPrintColor(x, y, (s32)color, 0xCE);
}
static int32_t measure_gs(const void* text)
{
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(text));
    return (int32_t)GSmsgGetRect(0xCE); /* width in the high half, height in the low, as the game reads it */
}
/* the name-entry block of the keyboard window (+0x60; game/name_entry_arg.h), its 32-bit slots resolved to real
 * pointers so OrrePort needn't know the PC's pointer packing; and the screen's own sound effects (soundStartSE:
 * 0x23 move, 0x24 type, 0x25 delete, 0x27 page, as menuNameEntryCursor plays them) */
static void name_entry_view(void* window, struct PcNameEntryView* v)
{
    NAME_ENTRY_ARG* arg = (NAME_ENTRY_ARG*)PTR32_AT(window, 0x60);
    v->name = (uint16_t*)P32(arg->name);
    v->kind = (int32_t)arg->kind;
    v->row = (int32_t*)P32(arg->row);
    v->letter = (int32_t*)P32(arg->letter);
    v->column = (int32_t*)P32(arg->column);
    v->state = (int32_t*)P32(arg->state);
    v->fallback = arg->buffer; /* the name the game uses when the typed one is empty or all blanks (inputName) */
}
static void sound_se(uint32_t id) { soundStartSE(id); }
/* an open window's pieces of one size: their draw alpha set (2026-11-14, the naming keyboard: the X / B / START
 * icon pieces hidden while the PC keyboard types). Why the live pieces: a window turns its items into pieces ("sprite
 * nodes", menus.md) when it opens, so changing the item or sprite tables afterwards never reaches the screen (tried
 * and seen, 2026-11-14). The window keeps them in a list at +0x1C, each linked by its first word (32-bit slots on the
 * PC); a piece's size is at +0x54 / +0x56 and its alpha at +0x67, the byte the game's own piece callbacks set every
 * frame. Returns how many pieces matched. All platforms */
static int32_t window_piece_alpha(void* window, int16_t width, int16_t height, uint8_t alpha)
{
    int32_t n = 0, guard = 0;
    u8* node;
    if (window == NULL) return 0;
    for (node = (u8*)PTR32_AT(window, 0x1C); node != NULL && guard < 0x400; node = (u8*)PTR32_AT(node, 0x00), ++guard) {
        if (*(s16*)(node + 0x54) == width && *(s16*)(node + 0x56) == height) {
            node[0x67] = alpha;
            ++n;
        }
    }
    return n;
}
static int32_t name_max(int32_t kind)
{
    if (kind < 0 || kind > 3) return 0;
    return ((s32*)(lbl_80266DD8 + 4))[kind * 4];
}
static const void* msg_gschar(uint32_t key) { return GSmsgGetGSchar(key); }

/* the naming keyboard's 12-slot field (stage 3, 2026-11-14):
 * - a menu sprite drawn at a size and colour (windowDrawSprite2: 0xRRGGBBAA; the window's own fade still applies, as
 *   windowDrawSprite's -1 does), so the game's own underscore pictures (0x83 white, 0x82 green) can be drawn narrower;
 * - the green next-slot bar's pulse, exactly as the game's callbacks compute it (fn_80026740: 1 - phase x scale, from
 *   the name-entry block's phase at +0x30);
 * - whether a text equals a species name (a nickname's GameCube name must not: evolution renames a Pokémon whose
 *   nickname equals its old species name, pokemon_range_8011F5FC.c) */
extern void windowDrawSprite2(s16 x, s16 y, s16 width, s16 height, u32 color, void* context, s32 spriteId, u32 flags);
extern f32 lbl_8047B934;
extern f32 lbl_8047B938;
static void draw_sprite_sized(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t rgba, void* window, uint32_t sprite)
{
    windowDrawSprite2(x, y, w, h, rgba, window, (s32)sprite, 0);
}
static uint8_t name_pulse(void* window)
{
    u8* ctx = PTR32_AT_T(u8, (u8*)window, 0x60);
    const f32 phase = *PTR32_AT_T(f32, ctx, 0x30);
    return (uint8_t)(lbl_8047B934 - lbl_8047B938 * phase);
}
static int32_t is_species_name(const uint16_t* text)
{
    u16 id;
    for (id = 1; id < 0x19C; id++) {
        void* data = pokemonDataBiosGetPtr(id);
        const u16* name;
        if (data == NULL) continue;
        name = (const u16*)GSmsgGetGSchar(pokemonDataBiosGetName((u8*)data));
        if (name != NULL && GScharCmp(name, (const u16*)text) == 0) return 1;
    }
    return 0;
}

/* the full names beside the save's GameCube names (2.4, 2026-11-14; port/src/pc_names.cpp; the map of the game's
 * name code is in docs/port/orresave.md): the stored player's and Rui's names (heroGetStatus selectors 1 and 0x17, as
 * the message controls 0x2B / 0x2C read them), every Pokémon record in the save (the party's six, the three PC boxes of
 * 30 at box base + b x 0x24A4 + 0x14 + e x 0x138, and the daycare's one at section 11 + 8), so names of Pokémon no
 * longer in the save are dropped from the extras, and a box's name (box base + b x 0x24A4, 0x14 bytes) */
static uint16_t* hero_name(void) { return (uint16_t*)PC_PTR(heroGetStatus(NULL, 1, 0)); }
static uint16_t* partner_name(void) { return (uint16_t*)PC_PTR(heroGetStatus(NULL, 0x17, 0)); }
static uint16_t* box_name(int32_t box)
{
    u8* base = (u8*)PC_PTR(savedataGetStatus(NULL, 3));
    return (base == NULL || box < 0 || box > 2) ? NULL : (uint16_t*)(base + box * 0x24A4);
}
static int32_t pokemon_records(void** out, int32_t max)
{
    int32_t n = 0;
    int32_t i;
    int32_t b;
    u8* base;
    u8* daycare;
    for (i = 0; i < 6 && n < max; i++) {
        u8* r = (u8*)PC_PTR(heroGetStatus(NULL, 3, (u32)i));
        if (r != NULL && pokemonCheckValid(r) != 0) out[n++] = r;
    }
    base = (u8*)PC_PTR(savedataGetStatus(NULL, 3));
    for (b = 0; base != NULL && b < 3; b++) {
        for (i = 0; i < 30 && n < max; i++) {
            u8* r = base + b * 0x24A4 + 0x14 + i * 0x138;
            if (pokemonCheckValid(r) != 0) out[n++] = r;
        }
    }
    daycare = (u8*)PC_PTR(savedataGetStatus(NULL, 11));
    if (daycare != NULL && n < max && pokemonCheckValid(daycare + 8) != 0) out[n++] = daycare + 8;
    return n;
}

/* a Pokémon record's species name (the game's text), for the extras file: the hub labels a long nickname with its
 * species ("RAIKOU: Moonlightsky") without the game running (2.4 part 4) */
static const uint16_t* species_name(const void* record)
{
    const u16 id = (u16)pokemonGetStatus((u8*)record, 0, 0x6E /* POKEMON_STATUS_SPECIES */, 0);
    void* data = pokemonDataBiosGetPtr(id);
    return data != NULL ? (const uint16_t*)GSmsgGetGSchar(pokemonDataBiosGetName((u8*)data)) : NULL;
}

/* the Pokémon on the battle field (the project's build helper, 2026-11-16; port/src/pc_names.cpp, the battle nickname fix).
 * Why: the battle's menus (the HP boxes, the command menu's name line) print a Pokémon's name from copies of a copy
 * (fightOutPokemonToMenuPokemonStatus fills a stack record, the status window and the battle menus copy that again),
 * so neither the record around the text nor the first copy's address names the Pokémon; for a nickname whose
 * GameCube form is its species name, only "which Pokémon on the field has this name" can. How, as the game does it:
 * the field Pokémon pointers at the battle floor's +0xA4C4 (fightFloorBiosGetFightOutPokemonPtrAry, 8 slots, 32-bit),
 * each one checked out on the field (fightOutPokemonCheckFightOut), then its battle record (status 0xD6) and that
 * record's copy of the party record (status 0xCC), the same chain fightOutPokemonToMenuPokemonStatus reads. Outside a
 * battle (fightIsBattleActive) the slots may be stale, so nothing is returned. Read only. Platforms: all. */
extern u8 fightIsBattleActive(void);
extern u32 fightFloorBiosGetFightOutPokemonPtrAry(u8* ptr, u16 idx);
static int32_t battle_field_records(void** out, int32_t max)
{
    u8* floor;
    int32_t n = 0;
    u16 i;
    if ((u8)fightIsBattleActive() != 1) return 0;
    floor = fightFloorBiosGetFightFloorPtr();
    for (i = 0; i < 8 && n < max; i++) {
        u8* fightOut = (u8*)PC_PTR(fightFloorBiosGetFightOutPokemonPtrAry(floor, i));
        u8* battle;
        u8* bios;
        if (fightOut == NULL || fightOutPokemonCheckFightOut(fightOut) == 0) continue;
        battle = (u8*)PC_PTR(pokemonGetStatus(fightOut, 0, 0xD6 /* POKEMON_STATUS_BATTLE_POKEMON_PTR */, 0));
        if (battle == NULL) continue;
        bios = (u8*)PC_PTR(pokemonGetStatus(battle, 0, 0xCC /* POKEMON_STATUS_RECORD_COPY_PTR */, 0));
        if (bios != NULL) out[n++] = bios;
    }
    return n;
}

/* un-nicknamed Pokémon in the chosen language (2026-11-15; port/src/pc_names.cpp): a species' name key, and a
 * message as the plain string the game copies (GSmsgGetGSchar: species names stay the disc's) */
static uint32_t species_key(uint16_t id)
{
    void* data = pokemonDataBiosGetPtr(id);
    return data != NULL ? pokemonDataBiosGetName((u8*)data) : 0;
}
static const uint16_t* msg_string(uint32_t key) { return (const uint16_t*)GSmsgGetGSchar(key); }

/* dialogue re-wrapping (2026-11-14): the measuring walk lives in gs_msg.c, next to the game's own walker it uses */
extern u8* pc_msg_task_of(u32 key);
extern s32 pc_msg_measure_units(const u8* task, struct PcMsgUnit* out, s32 cap);
static uint8_t* msg_task(uint32_t key) { return pc_msg_task_of(key); }
static int32_t msg_measure(const uint8_t* task, struct PcMsgUnit* out, int32_t cap)
{
    return pc_msg_measure_units(task, out, cap);
}

/* for the naming keyboard's input-method parts (the project's IME work, 2026-11-15; port/src/pc_name_keyboard.cpp):
 * - whether a game font has its own glyph for a character (gs_msg.c, orre_font_has_glyph: the disc's glyphs only,
 *   never the font renderer's part D), for the GameCube name's "GameCube-safe" letters;
 * - a flat or two-colour box and a 1-pixel line, drawn with the game's own 2D drawing exactly as the game's window
 *   callbacks draw them (fn_80063AD4, Battle Mode's "O.K.?" box: a quad with a colour per vertex, then lines), at x / y
 *   relative to the current draw item; colours 0xRRGGBBAA by value. Why the game's drawing: the choice list and the
 *   field's red flash then sit in the game's picture like its own windows (same blending, same fade);
 * - that box's own two colours (lbl_8047BFC8 top, lbl_8047BFCC bottom: the dark see-through slate), read by value.
 * All platforms (the colours are u32 values: the byte order note in fn_80063AD4 applies; these read them by value). */
extern s32 orre_font_has_glyph(u16 fontId, u16 code);
extern void gfxVtxColor1u32(s32, u32);
extern u8 lbl_80314E08[];
extern u32 lbl_8047BFC8;
extern u32 lbl_8047BFCC;
static int32_t font_has(uint16_t fontId, uint16_t code) { return orre_font_has_glyph(fontId, code); }
static void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t top, uint32_t bottom)
{
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetPrimitive(6);
    gfxSetVtxDesc(lbl_80314E08);
    gfxBegin(4);
    gfxVtxPos2s16(x, y);
    gfxVtxColor1u32(0, top);
    gfxVtxPos2s16((s16)(x + w), y);
    gfxVtxColor1u32(0, top);
    gfxVtxPos2s16((s16)(x + w), (s16)(y + h));
    gfxVtxColor1u32(0, bottom);
    gfxVtxPos2s16(x, (s16)(y + h));
    gfxVtxColor1u32(0, bottom);
    gfxEnd();
}
static void draw_hline(int16_t x, int16_t y, int16_t w, uint32_t rgba)
{
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetLineWidth(1.0f);
    gfxSetPrimitive(1);
    gfxSetVtxDesc(lbl_80314E08);
    gfxBegin(2);
    gfxVtxPos2s16(x, y);
    gfxVtxColor1u32(0, rgba);
    gfxVtxPos2s16((s16)(x + w), y);
    gfxVtxColor1u32(0, rgba);
    gfxEnd();
}
static uint32_t slate_color(int32_t which) { return which == 0 ? lbl_8047BFC8 : lbl_8047BFCC; }

/* ---- Main Menu on the PC screen (2026-11-15; port/src/pc_pc_screen.cpp) ----
 * The question asks the way the title's New Game question does (gs_title.c, titleMenuMain state 5: winMsgOpen(2, key,
 * 1, 1), menuSubOpenYesNo, winMsgClose(1)); the PC's own Save asks with the same YES / NO box (pcMenuSave). Both wait,
 * so only from the PC screen's driver task (OrreHook_PcMenuChoice). The tables are the PC build's data copies (pcdata,
 * native floats), which OrrePort lays out for its rows and puts back; the GameCube build is never touched. All
 * platforms */
extern s8 menuSubOpenYesNo(u8 menuType, s16 x, s16 y, s32 initialValue);
extern s32 winMsgOpen(s8 type, u32 message, u32 wait, u8 pause);
extern void winMsgClose(u32 wait);
static int32_t yes_no(int32_t initial, int16_t x, int16_t y) { return menuSubOpenYesNo(0, x, y, initial); }
static void msg_window_open(uint32_t key) { winMsgOpen(2, key, 1, 1); }
static void msg_window_close(void) { winMsgClose(1); }
extern f32 lbl_802E51C8[], lbl_802E5228[], lbl_802E5288[], lbl_802E52A8[], lbl_802E52B8[];
extern u8 lbl_803A654C[];
extern f32 lbl_8047A484, lbl_8047A488;
/* the main menu's glove: titleIntroStart copies the default (lbl_8047A358, set by the title's card read through
 * fn_800216E0) into the menu's cursor (lbl_8047A360) before OrrePort's re-read runs, so the cursor is set again here */
extern u32 lbl_8047A358, lbl_8047A360;
static void top_menu_glove_default(void) { lbl_8047A360 = lbl_8047A358; }
extern u32 savedataGetStatus(u8* arg1, u16 arg2);
static const uint8_t* save_image(uint32_t* size)
{
    *size = 0x1DFD0; /* the save image the PC's Save copies whole (pcMenuSave's PdaSaveImage) */
    return (const uint8_t*)PC_PTR(savedataGetStatus(0, 0));
}
/* the main menu's Quit (titleMenuMain's default case): the title screen, whose own card read runs at Press Start */
extern void fn_800205B8(u32 val);
extern void floorRequestChange(u32 floorId);
static void title_restart(void)
{
    fn_800205B8(1);
    floorRequestChange(900); /* ROOM_TITLE */
}

/* where Wes stands, for OrrePort's test logs (the project's helper, 2026-11-16: the PC MENU spot check): the same reads as
 * the PC MENU spot (gs_range_8000D290.c, orre_field_menu_spot), only in a field room (kind 1) where the hero's model
 * exists. Read only; all platforms */
extern void* pc_lowmem_alloc(size_t size);
extern void heroMoveGetHeroPos(u32 param);
extern void heroMoveGetHeroRot(u32 param);
extern void* floorDataBiosGetPtr(u32 key);
extern u8 floorDataBiosGetFloorKind(u8* ptr);
extern u32 std092_getCurrentRoomID(void);
static int32_t hero_spot(uint32_t* room, float* xyz, int32_t* facingDegrees)
{
    static f32* vec = NULL; /* below 4 GB: the hero getters take a 32-bit address */
    const u32 r = std092_getCurrentRoomID();
    *room = r;
    if (vec == NULL) vec = (f32*)pc_lowmem_alloc(sizeof(f32) * 6);
    if (vec == NULL || r == 0 || floorDataBiosGetFloorKind((u8*)floorDataBiosGetPtr(r)) != 1) return 0;
    heroMoveGetHeroPos((u32)(uintptr_t)vec);
    heroMoveGetHeroRot((u32)(uintptr_t)(vec + 3));
    xyz[0] = vec[0], xyz[1] = vec[1], xyz[2] = vec[2];
    *facingDegrees = (s32)(vec[4] * 57.29578f + (vec[4] < 0.0f ? -0.5f : 0.5f));
    return 1;
}

/* Forget the game's ARAM copy of one archive (the project's helper, 2026-11-16, snatcherLeo: "if I say, switched to Japanese in the
 * language settings, it would ... switch all the UI at that point in time"). Why: the archive loader keeps every
 * archive entry it read in its ARAM cache (fsysInitTOC / fn_8017F928) and, the next time the archive is loaded, copies
 * the entries from there instead of the disc (fsysLoadEntry: fn_8017F794 finds them by the archive's id and the
 * entry's group and name). OrrePort's language files (port/src/pc_lang_files.cpp) answer for title.fsys and the Battle
 * Mode sequence archives with the language disc's bytes under the US ids, so after a language change those archives
 * would come back from the cache in the old language (verified 2026-11-16: the title's archive was reloaded with slot
 * status 0xA1, from the cache, and the logo stayed English). This drops the archive's cached entries and its place in
 * the cache's archive list, the way the game itself makes room (fsysCacheMakeRoom in gs_range_80179F4C.c:
 * fn_8017F800, then the list entry removed), so its next load reads the (overlaid) disc file and caches that. Skipped
 * while any slot of that archive is loading (a cache copy may be in flight); a loaded archive is not touched (its
 * resources are in main memory). Returns 1 when forgotten, 0 when skipped. Read and called on the game's thread (the
 * Options screen). All platforms. */
#include "game/fsys/fsys.h"
extern FSYSManager lbl_80453FEC;
extern FSYSSlot* lbl_8047B1B4;
extern FSYSFileHandle* lbl_8047B1B8;
extern s32 lbl_8047B1BC;
extern void fn_8017F800(u32 fileHandle);
static int32_t fsys_cache_forget(uint32_t archiveId)
{
    s32 i, at = -1;
    if (lbl_8047B1B4 != NULL && lbl_80453FEC.maxSlots <= 256) {
        for (i = 0; i < (s32)lbl_80453FEC.maxSlots; i++) {
            const FSYSSlot* s = &lbl_8047B1B4[i];
            if (s->fileHandle == archiveId && s->status != 0 && s->status != 1000) return 0; /* loading now */
        }
    }
    fn_8017F800(archiveId);
    if (lbl_8047B1B8 == NULL) return 1;
    for (i = 0; i < lbl_8047B1BC; i++) {
        if (lbl_8047B1B8[i].handleID == (s32)archiveId) {
            at = i;
            break;
        }
    }
    if (at >= 0) { /* as fsysCacheRemoveHandle: the later entries move up one, the last one is cleared */
        for (i = at; i < lbl_8047B1BC - 1; i++) lbl_8047B1B8[i] = lbl_8047B1B8[i + 1];
        lbl_8047B1BC--;
        lbl_8047B1B8[lbl_8047B1BC].handleID = -1;
    }
    return 1;
}

/* the field camera's state for test logs (the project's helper, 2026-11-16; field_camera.c, fn_801171C8, read only) */
extern u8 lbl_8047AD70, lbl_8047AD71;
extern u32 lbl_8047AD68;
extern f32 lbl_8047AD74, lbl_8047AD78, lbl_8047AD7C;
static int32_t field_camera_state(float hdy[3])
{
    hdy[0] = lbl_8047AD74, hdy[1] = lbl_8047AD78, hdy[2] = lbl_8047AD7C;
    return (int32_t)lbl_8047AD71 | (int32_t)(lbl_8047AD68 & 0xFF) << 8 | (int32_t)lbl_8047AD70 << 16;
}

/* Test log (the project's helper, 2026-11-16; ORRE_DUCK_LOG=1): the music's volume every game frame, for snatcherLeo's report that
 * the field music swells after PC screen → Main Menu → YES. Why here: it reads the game's sound table (the duck stack
 * kept per sound) and MusyX's volume faders, and this file is the PC's game-side home for such reads. One line per
 * frame: the room, the mixer's clock (to line up with an ORRE_DUMP_AUDIO dump), the current music id (lbl_8047B0AC),
 * every playing music entry's game volume (GSsndWork +1, 0-127), duck depth and saved volumes (+4..+7), then MusyX's
 * music master fader (21) and the sequence faders 23-30 (volume x pause volume, 0-1). Read only; all platforms. */
#include <stdlib.h>
#include "pc/disc_types.h"
extern u8* snd_data;      /* the sound table: 12-byte big-endian records, the work slot at +8 (GSsndEntry) */
extern u32 lbl_8047B0E8;  /* its record count */
extern u32 lbl_8047B0AC;  /* the current music id (soundPlay, kind 1) */
extern f32 lbl_80434E64[]; /* MusyX's 32 volume faders, 0x30 bytes each: +0 volume, +0x14 pause volume (synth.c) */
extern double salPCCurrentSeconds(void);
static void (*s_duck_prev_frame)(unsigned frame);
static void duck_log_frame(unsigned frame)
{
    char line[512];
    int n = 0;
    u32 i;
    extern void* lbl_8047ACC8; /* the room manager's context: none before the first room (the room id reads it) */
    if (s_duck_prev_frame != NULL) s_duck_prev_frame(frame);
    if (lbl_8047ACC8 == NULL) return;
    n += snprintf(line + n, sizeof line - n, "[duck_log] f=%u room=0x%X t=%.3f bgm=%u |", frame,
                  (unsigned)std092_getCurrentRoomID(), salPCCurrentSeconds(), (unsigned)lbl_8047B0AC);
    for (i = 0; snd_data != NULL && i < lbl_8047B0E8 && n < 300; i++) {
        const u8* e = snd_data + i * 12;
        const u8* w;
        if ((e[0] & 0x20) == 0) continue; /* active (bit 5) */
        w = (const u8*)DISC_SLOT(e + 8, 0);
        if (w == NULL) continue;
        if ((e[0] & 0x80) == 0) { /* music and streamed sounds (bit 7 clear): always */
            n += snprintf(line + n, sizeof line - n, " %u:v%u d%u [%u %u %u]", (unsigned)i, w[1], w[4], w[5], w[6], w[7]);
        } else if (*(const s32*)(w + 8) != -1) { /* a resident effect (bit 7) with a voice: "se" */
            n += snprintf(line + n, sizeof line - n, " se%u:v%u d%u", (unsigned)i, w[1], w[4]);
        }
    }
    n += snprintf(line + n, sizeof line - n, " | m21=%.3f", lbl_80434E64[21 * 12]);
    for (i = 23; i <= 30 && n < (int)sizeof line - 16; i++) {
        const f32* f = &lbl_80434E64[i * 12];
        n += snprintf(line + n, sizeof line - n, " %.3f", f[0] * f[5]);
    }
    printf("%s\n", line);
}

static f32 duck_mute_music(int isEffect) { return isEffect ? 1.0f : 0.0f; }

/* The PC MENU softlock fix (2026-11-20, the project's helper; port/src/pc_save_windows.cpp, docs/port/pc-menu-anywhere.md
 * "Saves made in the middle of a scene"). Why each:
 * - flag_get / flag_set: the unsafe-window table's conditions and rollbacks are story flags; the game's own getter and
 *   setter (std082_flagGetValue, _flagSet: every class, the save image's own storage, docs/engine/flags.md) so the
 *   port never needs the flags' bit layout. _flagSet also reports through OrreHook_FlagChanged (the event log's [flag]
 *   lines), so a rollback shows in the log like any other flag write.
 * - hero_event_locked: Wes's people flag 0x80000008, which heroMoveInitEvent sets and heroMoveTermEvent clears
 *   (hero_move.c; group 0, index 100 = the hero, getResID): a scene holds him.
 * - player_prompt_up: something on screen the player answers with a button (a message window: winMsgCheck gives -1 when
 *   none; any window reading buttons: the window system's active id, 0 when none, window_candidate_80104828.c), so a
 *   scene waiting for the player is never taken for a hang.
 * - start_task: the hang watch's question needs a task that may wait (the message window and YES / NO box wait); a GS
 *   thread on the current room's context, the way the developers' debug battle start makes one (gs_task.c
 *   fn_8000804C; pc_debug_game.c's `fight`). A room change ends it with the room's other threads.
 * Read / call on the game's thread only. All platforms. */
extern void _flagSet(s32 flagId, u32 value);
extern u8 peopleTestFlagsByID(u32 groupId, u32 index, u32 mask);
extern s32 winMsgCheck(void);
static uint32_t flag_get(int32_t id) { return std082_flagGetValue((s32)id); }
static void flag_set(int32_t id, uint32_t value) { _flagSet((s32)id, value); }
static int32_t hero_event_locked(void) { return peopleTestFlagsByID(0, 100, 0x80000008) != 0; }
static int32_t player_prompt_up(void) { return (winMsgCheck() != -1 ? 1 : 0) | (windowGetActiveID() != 0 ? 2 : 0); }
static int32_t start_task(void (*fn)(void))
{
    return GSthreadCreate(1, floorGetCurrentContext(), 0x4000, 1, 1, (void*)fn) != NULL;
}

/* Autosave (2026-11-20, the project's helper; port/src/pc_autosave.cpp, docs/port/autosave.md "How it's built"). Why each:
 * - save_capture: the save image copied the way the PC's Save would write it at this moment, without touching the
 *   game's own image. The PC's Save (pcMenuSave, state 4, pda_candidate_8003842C.cpp) first stores in the live image
 *   whether Rui is with the player (hero status 0x18) and the return point the last PC's script set (_floorChangeInfo
 *   +0x3C / +0x40 / +0x44, fn_8011418C); the card task then adds 1 to the save count (status 1, kind 4) in its own copy
 *   (memcardSaveSetup). Here all three go into the copy only, with the game's own setters pointed at the copy, as
 *   memcardSaveSetup points them at its work buffer. When no PC has been used since the save was loaded (+0x3C is 0, its
 *   boot value), the image's own return point stays: what PC MENU fills in from the loaded save in the same case
 *   (gs_range_8000D290.c). `out` must lie below 4 GB (pc_lowmem_alloc): the game's section getter returns 32-bit
 *   addresses. Reads the game, writes only `out`, draws no random number (the card task's 20 random bytes,
 *   memcardFillRandom, are exactly why the autosave doesn't run the card task: likely, docs/port/autosave.md). Returns
 *   the copied size (0x1DFD0), 0 when there is no image.
 * - field_spot: where the player stands, with the room before and the entry, exactly as PC MENU records its spot
 *   (orre_field_menu_spot): 0 outside a field room (kind 1) or inside a story area with its own pending return point
 *   (+0x51, script command 161), where a spot can't be resumed.
 * - battle_active: fightIsBattleActive, a second guard beside the room stack (a battle is a pushed room).
 * Game thread only. All platforms. */
#include <string.h> /* memcpy */
extern void heroSetStatus(u8* ptr, u32 selector, u32 value);
extern void gamedatasaveSetStatus(struct GameDataSave* ptr, u16 kind, u32 value);
extern void fn_8011418C(u32* roomId, u32* secondRoomId, u8* entryIndex);
extern u32 floorGetNextPosIndex(void);
extern u8 _floorChangeInfo[];
static uint32_t save_capture(uint8_t* out)
{
    const u8* image = (const u8*)PC_PTR(savedataGetStatus(0, 0));
    struct GameDataSave* status;
    u8* hero;
    u32 room = 0, prev = 0;
    u8 entry = 0;
    if (image == NULL || out == NULL) return 0;
    memcpy(out, image, 0x1DFD0);
    status = (struct GameDataSave*)PC_PTR(savedataGetStatus(out, 1));
    hero = (u8*)PC_PTR(savedataGetStatus(out, 2));
    if (status == NULL || hero == NULL) return 0;
    heroSetStatus(hero, 0x18, heroMoveIsMember(1) ? 1 : 0);
    fn_8011418C(&room, &prev, &entry);
    if (room != 0) {
        gamedatasaveSetStatus(status, 5, room);
        gamedatasaveSetStatus(status, 7, prev);
        gamedatasaveSetStatus(status, 8, entry);
    }
    gamedatasaveSetStatus(status, 4, gamedatasaveGetStatus(status, 4) + 1);
    return 0x1DFD0;
}
static int32_t field_spot(uint32_t ids[3], float xyz[3], int32_t* facingDegrees)
{
    uint32_t room = 0;
    if (*(u8*)(_floorChangeInfo + 0x51) != 0 || !hero_spot(&room, xyz, facingDegrees)) return 0;
    ids[0] = room;
    ids[1] = floorGetPrevFloorID();
    ids[2] = floorGetNextPosIndex();
    return 1;
}
static int32_t battle_active(void) { return (u8)fightIsBattleActive() == 1; }

/* The Battle Mode guard (2026-11-20, the project's helper; docs/engine/battle-mode.md "Telling Battle Mode from the story",
 * docs/port/autosave.md, docs/port/pc-menu-anywhere.md). Why: Battle Mode runs on the story save's image (the title's
 * card read loaded it, and Battle Mode's own saves write that whole image with its record in section 14), and its
 * Mt. Battle course is walked in the story's own field rooms with the player in control. So "a settled field room, free
 * to move" doesn't tell Battle Mode from the story, and the autosave and PC MENU need a direct signal never to write
 * Battle Mode's image as a story save (snatcherLeo, 2026-11-20: no autosave in Battle Mode at all).
 * - battle_mode: two of the game's own values, read only (no random draws, nothing written):
 *   bit 0, the top menu's choice (lbl_8047A360, titleMenuMain in gs_title.c): 2 and 3 are its two Battle Mode entries
 *   (menuColosseumBattle(0) / (3): Colosseum Battle and Battle Now, both into the Battle Mode menu, room 0x395). It
 *   keeps that value for the whole of Battle Mode, Battle Now and Gang battles included, because nothing else writes
 *   it until the top menu room is set up again (titleIntroStart puts the default back: Continue or New Game), and that
 *   room is exactly where Battle Mode ends (floorLink(0x3A1, 0), menuColosseumBattle.c). Continue leaves 1 in it, New
 *   Game 0, so story play never reads 2 or 3. While the top menu itself is up it follows the cursor (the menu's draw
 *   callback, case 0x90E): nothing saves there;
 *   bit 1, the game mode variable 0x8AE (FLAG_system_gamemode, its name from an assert string): 1 / 2 (single /
 *   double) for the length of a Colosseum or Mt. Battle run, set when a run starts or is resumed and cleared by the
 *   Battle Mode menu's setup and exit (menuColosseumBattle.c). The game itself goes by it: the run's short pause menu
 *   (0x109, gs_range_8000D290.c), Mt. Battle's trainer battles through Battle Mode (fieldStartEncount). A story save
 *   never holds it set: every Battle Mode save clears it in the image it writes (gbaConvRunLinkMode, gba_conv.c).
 *   Either alone would do for the field (a Mt. Battle course always has both); the two together cover each other.
 * - battle_record_span: section 14 (savedataGetStatus 14) up to section 15, the next in memory: +0xE3E8, 0xCC2C bytes
 *   on the US disc (save-data.md), taken from the game's own section getters rather than written down a second time.
 * Game thread only. All platforms. */
static int32_t battle_mode(void)
{
    const s32 choice = (s32)lbl_8047A360;
    int32_t bits = choice == 2 || choice == 3 ? 1 : 0;
    if (PC_PTR(savedataGetStatus(0, 0)) != NULL && std082_flagGetValue(0x8AE) != 0) bits |= 2; /* only with an image */
    return bits;
}
static uint32_t battle_record_span(uint32_t* offset)
{
    const u8* image = (const u8*)PC_PTR(savedataGetStatus(0, 0));
    const u8* s14 = (const u8*)PC_PTR(savedataGetStatus(0, 14));
    const u8* s15 = (const u8*)PC_PTR(savedataGetStatus(0, 15));
    if (image == NULL || s14 == NULL || s15 == NULL || s14 < image || s15 <= s14) return 0;
    *offset = (uint32_t)(s14 - image);
    return (uint32_t)(s15 - s14);
}

/* The save-check sweep (2026-11-20; port/src/pc_savecheck.cpp, the project notes "The save-check
 * sweep"): the people in the room as text, so a test can compare the room a reloaded save rebuilds with the room the
 * game itself had at that moment (who is shown, where, standing or wandering). Why these columns: the Phenac softlock
 * (that page, section 2) was exactly a person hidden (the sack) and a person wandering (state 4) where the scene had
 * left her standing (state 0). "shown" is the model's visibility as peopleSetVisible sets it (fn_8018FB60 writes it to
 * PeopleEntry +0x21, the header's `animId`: likely misnamed; GSmodelSetVisibility takes the same byte). Same people
 * table walk as the debug toolkit's ORRE_NPC_LOG (pc_debug_game.c). Reads only; game thread; all platforms. */
#include "game/people/people.h"
#include <stdio.h> /* snprintf */
static int32_t people_rows(char* out, int32_t cap)
{
    s32 i, n = peopleGetMaxCount(), lines = 0, at = 0;
    if (out == NULL || cap <= 0) return 0;
    out[0] = '\0';
    for (i = 0; i < n && i < 512; i++) {
        PeopleEntry* e = peopleGetEntry(i);
        const f32* p;
        char line[160];
        s32 len;
        if (e == NULL || !e->active) continue;
        p = e->modelHandle != NULL ? (const f32*)peopleGetPositionPtr(e) : NULL;
        if (p != NULL)
            len = snprintf(line, sizeof line, "%d %u %u %u %u %u %.1f %.1f %.1f\n", (int)i, (unsigned)e->index,
                           (unsigned)e->groupId, (unsigned)e->animId, (unsigned)e->state, (unsigned)e->subState,
                           (double)p[0], (double)p[1], (double)p[2]);
        else
            len = snprintf(line, sizeof line, "%d %u %u %u %u %u - - -\n", (int)i, (unsigned)e->index,
                           (unsigned)e->groupId, (unsigned)e->animId, (unsigned)e->state, (unsigned)e->subState);
        if (len <= 0 || at + len >= cap) break;
        memcpy(out + at, line, (size_t)len + 1);
        at += len;
        lines++;
    }
    return lines;
}

/* Step 3.7's item tweaks (2026-11-20; port/src/pc_bag_tweaks.cpp; docs/port/tweaks-37-research.md). Why here: the
 * tweaks' rules live in port/, which reaches the bag and the item table only through this table. How each reads:
 * - the bag's rows: the same walk as the bag's own actions (bagItemUse / bagItemGive in gs_event_exec.c): a page's data
 *   source (+0x04 of its 0x4C-byte page record: a pocket kind, or -1 for the Hizuki list), the pocket's item array, and
 *   only its valid entries (fn_801429E8) counted as rows. The count of an entry is its item record's field 0x1C, as
 *   the decrement (fn_80140ACC) reads it. Keeping the walk identical matters: the loop hands the row back to the same
 *   action, which looks the item up by that row number;
 * - the option lists: each page record holds five option lists at +0x24 (one per bag mode: 8 bytes each, a 4-byte
 *   pointer cell and a count), each option 0x0C bytes (label key, callback cell, flags), as fn_80017CB8 reads them;
 * - item data: the item table's own getters (people_item_getters_exact_80143C50.c).
 * All read only; plain C, all platforms. */
extern u32 lbl_8047A2F8; /* the hero record the bag works on (a 32-bit game address) */
extern const u8 lbl_80266918[]; /* the bag's page table: six pockets, 0x4C bytes each */
extern u32 itemDataBiosGetFieldUseFunc(u8* p);
extern u32 itemDataBiosGetBattleUseFunc(u8* p);
extern u32 itemDataBiosGetDoc(u8* p);
extern s32 bagItemUse(s32 entry_idx, s32 target_n, s32* out);
extern s32 bagItemGive(s32 entry_idx, s32 target_n, s32* out);
extern s32 bagItemToss(s32 entry_idx, s32 target_n, s32* out);
extern s32 itemFieldUseNotNow();
extern s32 itemFieldUseForbidden();
extern s32 itemFieldUseMeaningless();
extern s32 itemFieldUseReadEinFile();
extern s32 menuUseItemEvolution(); /* the evolution stones' bag use (gs_title.c): the stone label's test, below */
enum { BAG_PAGES = 6, BAG_PAGE_SIZE = 0x4C, BAG_MODES = 5, BAG_OPTION_SIZE = 0x0C };
static u8* bag_list(int32_t page, u16* count)
{
    s32 kind;
    *count = 0;
    /* lbl_8047A2F8 may be 0 (the field bag): the getters then use the current hero, as the bag's own actions do */
    if (page < 0 || page >= BAG_PAGES) return NULL;
    kind = *(const s32*)(lbl_80266918 + page * BAG_PAGE_SIZE + 4);
    if (kind >= 0)
        return (u8*)heroItemGetItemKindToItemAryPtr((struct HeroSaveData*)PC_PTR(lbl_8047A2F8), (u8)kind, count, NULL,
                                                    NULL, NULL);
    return (u8*)PC_PTR(heroHizukiItemGetItemAryPtr((u8*)PC_PTR(lbl_8047A2F8), count, NULL, NULL, NULL));
}
static int32_t bag_row_of(int32_t page, uint16_t item, uint32_t* total)
{
    u16 count = 0;
    u8* list = bag_list(page, &count);
    s32 i, row = -1, first = -1;
    uint32_t sum = 0;
    for (i = 0; list != NULL && i < count; i++, list += 4) {
        if (fn_801429E8(PC_PTR32(list)) == 0) continue;
        row++;
        if ((u16)itemBiosGetItemDataId(list) != item) continue;
        if (first < 0) first = row;
        sum += (u16)itemGetStatus(list, 0, 0x1C, 0);
    }
    if (total != NULL) *total = sum;
    return first;
}
static uint16_t bag_item_at(int32_t page, int32_t row)
{
    u16 count = 0;
    u8* list = bag_list(page, &count);
    s32 i, n = -1;
    for (i = 0; list != NULL && i < count; i++, list += 4) {
        if (fn_801429E8(PC_PTR32(list)) == 0) continue;
        if (++n >= row) return (uint16_t)itemBiosGetItemDataId(list);
    }
    return 0;
}
static uint32_t bag_option_label(uint32_t callback)
{
    s32 page, mode, k;
    for (page = 0; page < BAG_PAGES; page++) {
        for (mode = 0; mode < BAG_MODES; mode++) {
            const u8* cell = lbl_80266918 + page * BAG_PAGE_SIZE + 0x24 + mode * 8;
            const u8* options = (const u8*)PC_PTR(*(const u32*)cell);
            const s32 n = *(const s32*)(cell + 4);
            if (options == NULL) continue;
            for (k = 0; k < n && k < 16; k++)
                if (*(const u32*)(options + k * BAG_OPTION_SIZE + 4) == callback)
                    return *(const u32*)(options + k * BAG_OPTION_SIZE);
        }
    }
    return 0;
}
static uint32_t item_field_use(uint16_t item) { return itemDataBiosGetFieldUseFunc(itemDataBiosGetPtr(item)); }
static uint32_t item_battle_use(uint16_t item) { return itemDataBiosGetBattleUseFunc(itemDataBiosGetPtr(item)); }
static uint8_t item_kind(uint16_t item) { return itemDataBiosGetKind(itemDataBiosGetPtr(item)); }
static uint32_t item_tm_number(uint16_t item) { return itemDataBiosGetWazaMachineNo(itemDataBiosGetPtr(item)) & 0xFF; }
static uint32_t item_hm_number(uint16_t item) { return itemDataBiosGetHidenMachineNo(itemDataBiosGetPtr(item)) & 0xFF; }
static uint32_t item_price(uint16_t item) { return itemDataBiosGetPrice(itemDataBiosGetPtr(item)); }
static uint32_t item_doc(uint16_t item) { return itemDataBiosGetDoc(itemDataBiosGetPtr(item)); }
/* as itemFieldUseTM's teach check (gs_title.c, gsTitleTeachWazaMachine): the move of the item's machine number against
 * the Pokémon's four moves */
static int32_t pokemon_knows_move(void* pokemon, uint16_t item)
{
    const u16 move = (u16)itemDataBiosGetWazaIDByWazaMachineNo(item_tm_number(item));
    s32 i;
    if (pokemon == NULL || move == 0) return 0;
    for (i = 0; i < 4; i++)
        if ((u16)pokemonBiosGetPokemonWazaDataId(pokemon, i) == move) return 1;
    return 0;
}
/* *Added 2026-11-24 (0.1.2's stone label, snatcherLeo: "USABLE only shows up for that specific Pokemon if it can use that to
 * evolve"; port/src/pc_bag_tweaks.cpp; the project notes):* whether using an evolution item on
 * this Pokémon would evolve it, asked the way the stone's own use asks (menuUseItemEvolution, then itemUse2Pokemon):
 * first a Shadow Pokémon is refused (pokemonIsDarkPokemon, the "can't be used on a SHADOW POKéMON" check, which comes
 * before anything else), then the game's evolution check in its item mode (pokemonEvolutionCheck mode 1: the species'
 * kind-7 evolution slots against this item, and none at all while an Everstone is held; 0xFFFF = no valid Pokémon).
 * So the label can never disagree with what pressing A does, and a mod that changes a species' evolution item or adds
 * its own stone is followed with no list in OrrePort. Read only: the check only reads the record and the species data
 * (verified from the code, pokemon_range_80128524.c) and draws no random numbers. Plain C, all platforms. */
static int32_t stone_able(void* pokemon, uint16_t item, uint16_t* species)
{
    u16 extra = 0;
    u8 evolution[4] = {0, 0, 0, 0};
    u16 result;
    if (species != NULL) *species = 0;
    if (pokemon == NULL) return 0;
    if (species != NULL) *species = (uint16_t)pokemonBiosGetPokemonDataId((u8*)pokemon);
    if (pokemonIsDarkPokemon((u8*)pokemon) != 0) return -1;
    result = pokemonEvolutionCheck((u8*)pokemon, 1, item, &extra, evolution);
    return result != 0 && result != 0xFFFF ? 1 : 0;
}
static void bag_message(uint32_t key)
{
    winMsgOpen(2, key, 1, 0);
    winMsgClose(1);
}

/* Step 3.7's Relic Forest shortcuts (2026-11-20; port/src/pc_relic_forest.cpp; docs/port/tweaks-37-research.md sections
 * 2-3). Why here: the shortcuts' rules live in port/, which reaches the room, the pause menu's "usable now" list and the
 * party's Shadow state only through this table.
 * - current_room: the game's own getter (inside the party screen's menu room it answers 0x38F, so the shortcuts note
 *   the field room when the pause menu opens);
 * - menu_queue_add: script command 17, std017_menuQueueAdd (menuPokemon.c), the call the Relic Stone's function 11
 *   makes for the Time Flute (eight entries at most; it ignores a ninth, as the game does);
 * - party_heart: the Relic Stone's own test, as its party picker (menuPokemonSub kind 7) and the stone's count
 *   (scriptGetDarkPointZeroPokemonNum) make it: a valid Pokémon, pokemonIsDarkPokemon, and its gauge (pokemonGetDp)
 *   exactly 0.0 (lbl_8047B7C4) for an open heart.
 * Plain C, all platforms. */
extern void std017_menuQueueAdd(u32 item, u32 data);
extern f32 pokemonGetDp(u8* ptr);
extern const f32 lbl_8047B7C4; /* 0.0f, the picker's "heart open" gauge value */
static uint32_t current_room(void) { return std092_getCurrentRoomID(); }
static void menu_queue_add(uint16_t item, uint32_t eventId) { std017_menuQueueAdd(item, eventId); }
static int32_t party_heart(int32_t slot)
{
    u8* party;
    u8* pokemon;
    if (slot < 0 || slot >= 6) return -1;
    party = (u8*)PC_PTR(savedataGetStatus(0, 2));
    pokemon = party != NULL ? (u8*)heroBiosGetPokemonPtr(party, (u16)slot) : NULL;
    if (pokemon == NULL || pokemonCheckValid(pokemon) == 0) return -1;
    if (pokemonIsDarkPokemon(pokemon) == 0) return 0;
    return pokemonGetDp(pokemon) == lbl_8047B7C4 ? 2 : 1;
}

/* Step 3.7's right-stick walking (2026-11-20; port/src/pc_cstick_walk.cpp; research section 4): the C-stick through
 * the game's own getters (input.c), so OrrePort sees exactly the smoothed and clamped values the game's other C-stick
 * readers see, and the scene's camera mode (the debug orbit camera, mode 5, keeps the C-stick). Plain C, all platforms */
static void right_stick(int32_t filtered, int8_t* x, int8_t* y)
{
    *x = GSinputGetRightStickXData(1, filtered);
    *y = GSinputGetRightStickYData(1, filtered);
}
static uint32_t scene_mode(void) { return GSscene_GetMode(); }

/* Step 3.7's Smooth Party UI (2026-11-20; port/src/pc_party_ui.cpp; research 6b): the field party's portrait keys the
 * party screen asks for (its chain, fn_8001BEBC, reads the same party records through menuPokemonGetPokemon in the
 * field mode), and the portrait cache's own state (gs_range_80109C88.c). Read only; plain C, all platforms */
extern u16 pokemonGetFaceSpriteId(void* pokemon);
extern s8 menuFaceIsReady(u16 key);
extern u8 fn_8010B560(void);
static uint16_t party_face_key(int32_t slot)
{
    u8* party;
    void* pokemon;
    if (slot < 0 || slot >= 6) return 0;
    party = (u8*)PC_PTR(savedataGetStatus(0, 2));
    pokemon = party != NULL ? heroBiosGetPokemonPtr(party, (u16)slot) : NULL;
    if (pokemon == NULL || (u16)pokemonGetStatus((u8*)pokemon, 0, POKEMON_STATUS_SPECIES, 0) == 0) return 0;
    return pokemonGetFaceSpriteId(pokemon);
}
static int32_t face_ready(uint16_t key) { return menuFaceIsReady(key) == 1 ? 1 : 0; }
static int32_t face_loading(void) { return fn_8010B560() != 0 ? 1 : 0; }
/* the key's texture id = its poke_face.fsys entry id (2026-11-21, the stand-ins: pc_party_ui.cpp). The same record read
 * menuFaceRequestPokemon passes to the file system as the entry to load (gs_range_80109C88.c); a key past the table's
 * end is refused first, so the getter's out-of-range report never fires from here */
extern void* _menuFaceBiosGetPtr__FUs(u16 key);
extern void* lbl_80478E70;
static uint32_t face_resource_id(uint16_t key)
{
    if (lbl_80478E70 == NULL || key >= *(u32*)lbl_80478E70) return 0;
    return PC_PTR32(_menuFaceBiosGetPtr__FUs(key));
}

/* the right stick copying the left one (2026-11-21; port/src/pc_cstick_walk.cpp): the P★DA's Pokémon model page is open,
 * where the C-stick spins the model (pda_range_80037158.c, its per-frame update: page state 4 of the P★DA's work area,
 * lbl_803A6818 +0x1C, reads GSinputGetRightStick*). Only while the P★DA's menu room (0x392) is the current room, since
 * the work area keeps its last state after the P★DA closes. Read only.
 *
 * *Changed 2026-11-21 (the 3.7 closing sweep's gap, docs/port/tweaks-37-research.md):* every page of the P★DA's
 * Pokémon section and the fades between them, not only the model page. Why: in that section the P★DA's own pad reader
 * (pdaReadPadInput, the left stick and the D-pad only) turns a sideways push into a page key (its bits 4 / 8:
 * pdaModelViewerControl steps the page level +0x28 down / up: level 0 the first page = state 2, 1 the model page =
 * state 4, 2 the third page = state 5, each change through state 0xD, a fade of a few frames that then sets the state to
 * its target +0x24) and an up / down push into the next Pokémon. the reference playthrough's P02 and P07 (normal mode) pushed the C-stick
 * on the third page (state 5), where the GameCube's C-stick does nothing; copied, its first frame turned the page back
 * to the model page, and the model page's own guard only took over once it was up (the sweep's 3-4 copied frames;
 * verified from the fix's own P02 replay: the windows show level 2 before the push and level 1 after it). So in the
 * Pokémon section the right stick now does exactly what it did on the GameCube: it spins the model on the model page and
 * does nothing elsewhere, instead of turning pages under a player who means to spin the model. The states (verified
 * from pda_range_80037158.c's state loop, fn_80044630):
 *   2, 4, 5   the section's three pages (4, the model page, is the only P★DA state that reads the C-stick);
 *   6         the way in from the top menu; 8, 9, 0xA the ways out from pages 2, 4 and 5;
 *   0xD       every fade between the pages (whatever its target).
 * Still copied (the left stick's job there, as the feature intends): the top menu's tabs (state 1, its menu 0x3A), the
 * other tab's page (3, menu 0xCE, and its ways in and out 7 / 0xC), the opening (0) and closing (0xB, 0x64). (0xE, a
 * second fade, is never set by the game: left out.) */
extern u8 lbl_803A6818[];
static int32_t pda_model_page(void)
{
    s32 state;
    if (std092_getCurrentRoomID() != 0x392) return 0;
    state = *(s32*)(lbl_803A6818 + 0x1C);
    switch (state) {
    case 2: case 4: case 5: case 6: case 8: case 9: case 0xA: case 0xD:
        return state; /* non-zero: the state itself, so the caller's log can say which */
    default:
        return 0;
    }
}

/* the pose rows (2026-11-21; port/src/pc_pose_rows.cpp): a person posed with the same three calls the room scripts
 * use (D2_valley_1's own setup lays Vander down with std051 animation 7, loop on, then std059 to his place: verified
 * from the decoded script), the facing set at once rather than turned over frames (std068 turns) */
extern u8 std051_characterSetAnimation(u32 groupId, u32 index, s32 animIndex, s32 frame, u8 loop);
extern void std059_characterSetPosition(u32 groupId, u32 index, f32 x, f32 y, f32 z);
extern void peopleSetRotationDeg(u32 groupId, u32 index, f32 x, f32 y, f32 z);
#include "game/people/people.h" /* PeopleEntry, peopleGetEntry: is the person in the room (as pc_debug_game.c's npc) */
static int32_t person_pose(uint32_t group, uint32_t index, int32_t anim, int32_t loop, float x, float y, float z,
                           int32_t hasFacing, float facingDegrees)
{
    s32 i, found = 0;
    for (i = 0; i < peopleGetMaxCount() && !found; i++) {
        const PeopleEntry* e = peopleGetEntry(i);
        found = e != NULL && e->modelHandle != NULL && e->groupId == group && e->index == index;
    }
    if (!found) return 0;
    if (anim >= 0) std051_characterSetAnimation(group, index, anim, 0, (u8)(loop != 0));
    std059_characterSetPosition(group, index, x, y, z);
    if (hasFacing) peopleSetRotationDeg(group, index, 0.0f, facingDegrees, 0.0f);
    return 1;
}

/* the pose rows' "look" field (2026-11-21, snatcherLeo: Vander's "head follows me when I don't think it should"): the head
 * switched the way the room scripts switch it. Vander's placement has the look-at bit (byte 0 bit 5; likely, from his
 * head following Wes outside any talk), so the room's entry (floor.c) starts std065_characterLookAtPlayer on him and his
 * head follows Wes; Mt. Battle's func73 stops it with
 * std067_characterStopLookAt(group, 9) before Dakim knocks him down, so in the scene he lies still (verified from the
 * decoded script: func73 calls it twice on him, so calling it on a person already not looking is the game's own use).
 * No random draws (likely: std067 resets the head part and clears the look target; std065 sets a look target) */
extern void std065_characterLookAtPlayer(u32 groupId, u32 index);
extern void std067_characterStopLookAt(u32 groupId, u32 index);
static int32_t person_look(uint32_t group, uint32_t index, int32_t lookPlayer)
{
    s32 i;
    const PeopleEntry* found = NULL;
    int32_t wasLooking;
    for (i = 0; i < peopleGetMaxCount() && found == NULL; i++) {
        const PeopleEntry* e = peopleGetEntry(i);
        if (e != NULL && e->modelHandle != NULL && e->groupId == group && e->index == index) found = e;
    }
    if (found == NULL) return 0;
    /* for the log: a look target set (+0x18, the head's target; std067 clears it), so the caller can say what changed */
    wasLooking = found->threadHandle != NULL;
    if (lookPlayer) std065_characterLookAtPlayer(group, index);
    else std067_characterStopLookAt(group, index);
    return wasLooking ? 2 : 1;
}

/* The quicker way back from the PC screen (2026-11-21, the project's build helper; snatcherLeo: "the fade isn't the problem ... I just
 * want to get back to main menu a little faster"; measured: docs/port/pc-menu-anywhere.md "Leaving the PC screen
 * sooner"). Why: when a menu room pops, the field room's archive comes back from the loader's ARAM cache (every entry in
 * one frame with fast loading), but each compressed entry then goes through the loader's job chain, one job at a time,
 * each job its own GSgapp update task: an LZSS decode job, then the entry's group callback (the resources registered),
 * and the job manager (fn_8018094C, at the start of fsysUpdateTask) finishes one and starts the next on the following
 * frame. About two frames per entry: 39 entries in Mt. Battle's valley = 78 frames of black, while the room manager
 * waits for the archive (floorLoadData). On the GameCube that pacing kept each frame's work small; on the PC a decode
 * takes well under a millisecond.
 * What this does: called after the frame's update pass (OrreHook_AfterGappUpdate), for at most budgetUs microseconds,
 * it runs the current job's task at once (pc_gapp_run_task: the same task, the same order, only sooner) and the
 * loader's own frame step (fsysUpdateTask: the job manager, then every slot's status step), repeating while that makes
 * progress. Nothing is skipped or reordered: the jobs run in their chain's order and every slot goes through its own
 * statuses; only the frames between steps go. Returns how many jobs it ran. No random draws (likely: decoding, the
 * archive's group callbacks and the loader's bookkeeping; checked with ORRE_RNG_COUNT). All platforms (SDL's clock) */
#include "game/gs_range_8017FA5C_shared.h" /* the loader's job pool (GsRangePoolElem, lbl_8047B1E4) */
extern void fsysUpdateTask(void);
extern s32 pc_gapp_run_task(u32 taskId); /* gs_gapp.c */
extern unsigned long long SDL_GetTicksNS(void);
static int32_t fsys_hurry(int32_t budgetUs)
{
    const unsigned long long start = SDL_GetTicksNS();
    int32_t jobs = 0, idle = 0;
    while (idle < 3 && SDL_GetTicksNS() - start < (unsigned long long)budgetUs * 1000ull) {
        GsRangePoolElem* job = lbl_8047B1E4;
        int ran = 0;
        /* the current job, created as an update task and not yet run (state 1): run it now, as GSgappUpdate would */
        if (job != NULL && job->active == 1 && job->callback == NULL && job->app != NULL && job->state == 1) {
            ran = pc_gapp_run_task((u32)(uintptr_t)job->app);
        }
        fsysUpdateTask(); /* the loader's own frame step: finish / start jobs, every slot's status step */
        jobs += ran;
        idle = ran ? 0 : idle + 1;
    }
    return jobs;
}

__attribute__((constructor)) static void pc_game_ui_register(void)
{
    {
        extern void (*pc_vi_frame_hook)(unsigned frame);
        const char* v = getenv("ORRE_DUCK_LOG");
        if (v != NULL && v[0] == '1') {
            s_duck_prev_frame = pc_vi_frame_hook;
            pc_vi_frame_hook = duck_log_frame;
        }
        /* ORRE_DUCK_MUTE_MUSIC=1 (the same test log): every music voice silent in the mix, so a dump holds only the
         * effects (the field's ambient sounds measured on their own). Through the mixer's volume hook, as Settings'
         * music volume would; test runs only */
        v = getenv("ORRE_DUCK_MUTE_MUSIC");
        if (v != NULL && v[0] == '1') {
            extern f32 (*OrreHook_SoundVolumeScale)(int isEffect);
            OrreHook_SoundVolumeScale = duck_mute_music;
        }
    }
    pc_game_ui.menu_item = item_get;
    pc_game_ui.menu_item_set_xy = item_set_xy;
    pc_game_ui.menu_sprite = sprite_get;
    pc_game_ui.menu_sprite_count = sprite_count;
    pc_game_ui.menu_item_count = item_count;
    pc_game_ui.texture_load = texture_load;
    pc_game_ui.texture_create = texture_create;
    pc_game_ui.texture_lock = texture_lock;
    pc_game_ui.texture_unlock = texture_unlock;
    pc_game_ui.resource_find = resource_find;
    pc_game_ui.resource_register = resource_register;
    pc_game_ui.draw_sprite = draw_sprite;
    pc_game_ui.options_state = lbl_803A1FC8;
    pc_game_ui.options_help = (uint32_t*)lbl_802E4EF0;
    pc_game_ui.msg_group_of = msg_group_of;
    pc_game_ui.menu_open_wait = menu_open_wait;
    pc_game_ui.menu_close = menu_close;
    pc_game_ui.window_active_id = window_active_id;
    pc_game_ui.window_keys = window_keys;
    pc_game_ui.menu_button_normal = menu_button_normal;
    pc_game_ui.menu_button_normal_addr = (uint32_t)(uintptr_t)&menuButtonNormal; /* below 4 GB: the exe's image */
    pc_game_ui.sound = sound;
    pc_game_ui.msg_print = msg_print;
    pc_game_ui.msg_rect = msg_rect;
    pc_game_ui.clip_set = clip_set;
    pc_game_ui.clip_reset = clip_reset;
    pc_game_ui.place_text = place_text;
    pc_game_ui.shadow_counts = shadow_counts;
    pc_game_ui.window_find = window_find;
    pc_game_ui.card_load_title = card_load_title;
    pc_game_ui.print_gs = print_gs;
    pc_game_ui.measure_gs = measure_gs;
    pc_game_ui.name_max = name_max;
    pc_game_ui.msg_gschar = msg_gschar;
    pc_game_ui.name_entry_view = name_entry_view;
    pc_game_ui.sound_se = sound_se;
    pc_game_ui.window_piece_alpha = window_piece_alpha;
    pc_game_ui.msg_task = msg_task;
    pc_game_ui.msg_measure = msg_measure;
    pc_game_ui.draw_sprite_sized = draw_sprite_sized;
    pc_game_ui.name_pulse = name_pulse;
    pc_game_ui.is_species_name = is_species_name;
    pc_game_ui.hero_name = hero_name;
    pc_game_ui.partner_name = partner_name;
    pc_game_ui.box_name = box_name;
    pc_game_ui.pokemon_records = pokemon_records;
    pc_game_ui.species_name = species_name;
    pc_game_ui.species_key = species_key;
    pc_game_ui.msg_string = msg_string;
    pc_game_ui.font_has = font_has;
    pc_game_ui.fill_rect = fill_rect;
    pc_game_ui.draw_hline = draw_hline;
    pc_game_ui.slate_color = slate_color;
    pc_game_ui.yes_no = yes_no;
    pc_game_ui.msg_window_open = msg_window_open;
    pc_game_ui.msg_window_close = msg_window_close;
    pc_game_ui.pc_slide_buttons = lbl_802E51C8;
    pc_game_ui.pc_slide_labels = lbl_802E5228;
    pc_game_ui.pc_lit_places = lbl_802E5288;
    pc_game_ui.pc_wheel_targets = lbl_802E52A8;
    pc_game_ui.pc_wheel_bases = lbl_802E52B8;
    pc_game_ui.pc_slide_live = (float*)lbl_803A654C;
    pc_game_ui.hero_spot = hero_spot;
    pc_game_ui.fsys_cache_forget = fsys_cache_forget;
    pc_game_ui.field_camera_state = field_camera_state;
    pc_game_ui.battle_field_records = battle_field_records;
    pc_game_ui.pc_wheel_angle = &lbl_8047A484;
    pc_game_ui.pc_wheel_target = &lbl_8047A488;
    pc_game_ui.top_menu_glove_default = top_menu_glove_default;
    pc_game_ui.save_image = save_image;
    pc_game_ui.title_restart = title_restart;
    pc_game_ui.flag_get = flag_get;
    pc_game_ui.flag_set = flag_set;
    pc_game_ui.hero_event_locked = hero_event_locked;
    pc_game_ui.player_prompt_up = player_prompt_up;
    pc_game_ui.start_task = start_task;
    pc_game_ui.save_capture = save_capture;
    pc_game_ui.field_spot = field_spot;
    pc_game_ui.battle_active = battle_active;
    pc_game_ui.people_rows = people_rows;
    pc_game_ui.battle_mode = battle_mode;
    pc_game_ui.battle_record_span = battle_record_span;
    pc_game_ui.bag_row_of = bag_row_of;
    pc_game_ui.bag_item_at = bag_item_at;
    pc_game_ui.bag_use_addr = (uint32_t)(uintptr_t)&bagItemUse; /* below 4 GB: the exe's image */
    pc_game_ui.bag_give_addr = (uint32_t)(uintptr_t)&bagItemGive;
    pc_game_ui.bag_toss_addr = (uint32_t)(uintptr_t)&bagItemToss;
    pc_game_ui.bag_option_label = bag_option_label;
    pc_game_ui.item_field_use = item_field_use;
    pc_game_ui.item_battle_use = item_battle_use;
    pc_game_ui.item_use_not_now_addr = (uint32_t)(uintptr_t)&itemFieldUseNotNow;
    pc_game_ui.item_use_forbidden_addr = (uint32_t)(uintptr_t)&itemFieldUseForbidden;
    pc_game_ui.item_use_meaningless_addr = (uint32_t)(uintptr_t)&itemFieldUseMeaningless;
    pc_game_ui.item_use_reader_addr = (uint32_t)(uintptr_t)&itemFieldUseReadEinFile;
    pc_game_ui.item_kind = item_kind;
    pc_game_ui.item_tm_number = item_tm_number;
    pc_game_ui.item_hm_number = item_hm_number;
    pc_game_ui.item_price = item_price;
    pc_game_ui.item_doc = item_doc;
    pc_game_ui.pokemon_knows_move = pokemon_knows_move;
    pc_game_ui.item_use_evolution_addr = (uint32_t)(uintptr_t)&menuUseItemEvolution;
    pc_game_ui.stone_able = stone_able;
    pc_game_ui.bag_message = bag_message;
    pc_game_ui.current_room = current_room;
    pc_game_ui.menu_queue_add = menu_queue_add;
    pc_game_ui.party_heart = party_heart;
    pc_game_ui.right_stick = right_stick;
    pc_game_ui.scene_mode = scene_mode;
    pc_game_ui.party_face_key = party_face_key;
    pc_game_ui.face_ready = face_ready;
    pc_game_ui.face_loading = face_loading;
    pc_game_ui.face_resource_id = face_resource_id;
    pc_game_ui.person_pose = person_pose;
    pc_game_ui.person_look = person_look;
    pc_game_ui.fsys_hurry = fsys_hurry;
    pc_game_ui.pda_model_page = pda_model_page;
    pc_game_ui.ready = 1;
}
