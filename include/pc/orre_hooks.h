/*
 * pc/orre_hooks.h: hook points in the game code (PC only; docs/port/hook-points.md lists every one).
 *
 * Why: snatcherLeo's rule (2026-10-16), "hook points, not features, in game code". A TARGET_PC change inside src/game that
 * changes behaviour is a small named hook whose default is the console's behaviour; the feature lives in port/ and
 * sets the hook. Every hook here is a function pointer: NULL = nothing registered = exactly what the GameCube does.
 * The pointers are defined in port/src/pc_cheats.c (the port library, always linked into the game).
 *
 * Older hooks are still declared with a local extern where they are used (the hook-point page names each); new hooks
 * go here, so this header becomes the one list the page describes.
 *
 * Platforms: the same on Windows, Linux and macOS (plain C function pointers).
 */
#ifndef PC_ORRE_HOOKS_H
#define PC_ORRE_HOOKS_H

#ifdef TARGET_PC

#include "dolphin/types.h"

/* ---- Quick cheats (snatcherLeo, 2026-11-13, mockup 63 v2; port/src/pc_cheats.c sets these while a cheat is on) ---- */

/* fight_range_exact_80226134.c, WS_DAMAGE_LOSS: a damaging move's final damage, after the random roll, Focus Band and
 * Endure, before the health bar and the HP change. Given whether the attacker and the target are on side 0 (the
 * player's side), the damage and the target's HP; returns the damage to deal. Instant win. */
extern s32 (*OrreHook_MoveDamage)(int attackerOnSide0, int targetOnSide0, s32 damage, s32 targetHp);

/* fight_range_80211A00.c, wsCmdEF (the PC copy): nonzero makes a thrown ball certain to catch, as the Master Ball is
 * (four shakes, no random draws). Instant catch. */
extern int (*OrreHook_CatchCertain)(void);

/* hero_savedata_exact_80129280.c, heroDecPokedoru (every place money is taken: shops, a lost battle, scripts):
 * returns how much to take. Infinite money. */
extern u32 (*OrreHook_MoneyTaken)(u32 amount);

/* The places an item is used up: a battle item (fight_range_80211A00.c, the PC copy of the item action) and an item
 * used from the bag on a Pokémon (menuPokemon.c). Nonzero keeps the item. Selling, tossing, giving and story item
 * swaps don't ask. Items never run out. */
extern int (*OrreHook_ItemUseKeeps)(u16 item);

/* fight_range_80211A00.c, wsCmd23 (the PC copy): one Pokémon's EXP after every bonus (Exp. Share split, Lucky Egg,
 * trainer battle, traded), before it is added or banked by a Shadow Pokémon. Returns the EXP to give. EXP
 * multiplier. */
extern u32 (*OrreHook_ExpGained)(u32 exp);

/* ---- The font renderer (step 2.2, part A; 2026-11-13; docs/port/font-renderer.md) ---- */

/* gs_msg.c, where a glyph's 4-bit bitmap is about to be copied into the text atlas (msgSetChar and the ruby-text
 * draw, both before _msgMakeTexture). Given the font id the text is in (the game's own per-message font, the "kind of
 * text" a later per-kind choice keys on), the character, the glyph's width (kept: it is the pen's advance and the
 * quad's width, so the layout never changes), the font's cell height, its baseline (the bottom of 'H' from the
 * cell's top) and its capital height (the rows of 'H' with ink; *added 2026-11-13, part C:* the renderer sizes each
 * font so its capitals are as tall as the game's, after the first battle pictures showed the small fonts 1-2 rows
 * short), and the original bitmap with its height and vertical offset (in: the original's; the game draws the
 * picture at the line's top + yOffset): return nonzero and point *pixels (and, if the box changes, *height and
 * *yOffset) at a replacement bitmap (same width, rows of ceil(width / 2) bytes, high nibble first) to draw that
 * instead. The renderer keeps the original's box, so letters sit where the originals did. NULL or 0 = the disc's own
 * bitmap. */
extern int (*OrreHook_GlyphFetch)(u16 fontId, u16 code, s16 width, s16 cellHeight, s16 baseline, s16 capHeight,
                                  const u8** pixels, s16* height, s16* yOffset);

/* gs_msg.c, _msgGetCodeInfo, when the current font has no glyph for a character (part D, 2026-11-13): return a
 * glyph record laid out as the disc's (8 bytes, big-endian: u16 code, u8 width, u8 height, u32 offset = vertical offset
 * in the top byte, the picture's place after the bank's data offset in the low 24 bits) and set *outBank to its bank
 * (a 16-byte header: u16 count, 2 bytes, u32 data offset, two links), so the game draws, measures and wraps the
 * character as a letter. Given the font id, the character, the cell height, the font's baseline and capital height
 * (from its own 'H'). NULL: the game's white box, as on the GameCube. */
extern void* (*OrreHook_GlyphMissing)(u16 fontId, u16 code, s16 cellHeight, s16 baseline, s16 capHeight,
                                      void** outBank);

/* gs_msg.c, orre_glyph_fetch (msgSetChar and the ruby draw), just before a letter's picture is copied into the text
 * atlas: the character it is. Part B (pc_text_atlas.cpp) picks its tracer by it: Japanese characters get the kanji
 * tracer, the rest the one checked on the Latin letters (2026-11-15). Nothing: as on the GameCube. */
extern void (*OrreHook_GlyphCode)(u16 code);

/* gs_msg.c, GSmsgFindMessage (the game's message lookup, inlined wherever a message is shown, measured or inserted),
 * when a key is found (the language switch, 2.2 step 4, 2026-11-13): return the chosen language's text for the
 * key (bank key >> 20, entry key & 0xFFFFF) in the disc's own encoding (big-endian UTF-16, 0xFFFF + a code byte +
 * that code's argument bytes for control codes, 0x0000 at the end), kept alive for the rest of the run and below
 * 4 GB (the game stores text pointers in 32-bit slots). Given the disc's text for the key. NULL: the disc's text, as
 * on the GameCube. */
extern void* (*OrreHook_MessageText)(u32 key, const void* original);

/* gs_msg.c, GSmsgGetGSchar: above 0 while the game fetches a message as a plain string it may copy or compare (a
 * species name written into a Pokémon's nickname at a snag or an evolution; "is this nickname its species name?").
 * OrreHook_MessageText keeps species names the disc's while it is set, so the save keeps the US names whatever
 * language is shown (2026-11-15). Not a behaviour change by itself: a counter the hook reads. */
extern int OrreHook_MessageForCopy;

/* win_msg.c, winMsgDraw, every frame a message window (not the battle one, 0x50) draws its message, just before it
 * does (dialogue re-wrapping, 2026-11-14; port/src/pc_msg_rewrap.cpp): the message's key and the box's text item
 * size. The hook may point the running message at a re-laid-out copy of its text before its first letter shows.
 * NULL: as on the GameCube. */
extern void (*OrreHook_MessageBox)(u32 key, s32 width, s32 height);

/* gs_msg.c, GSmsgDispatchControl, when a control code inserts a string (its mode 1: the player's and partner's names,
 * nicknames, numbers; drawing and measuring alike) (2026-11-14): return other text to insert instead (the disc's
 * encoding, below 4 GB, kept alive while shown), or NULL for the game's own. First users: a test switch that shows a
 * long player name without touching the save (pc_msg_rewrap.cpp); later the names longer than the save's fields
 * (naming keyboard stage 3). NULL hook: as on the GameCube. */
extern void* (*OrreHook_MessageInsert)(u32 control, void* text);

/* The name screen's field, for OrrePort's 12-slot field (naming keyboard stage 3, 2026-11-14; mockup 103):
 * - OrreHook_NameEntryPiece: the field's piece callbacks (menuNameEntry_exact_800265C0.c / _80026640.c / _80026740.c),
 *   which show a piece only when the screen's layout type (7, 8 or 10 letters) matches. piece: 0 a frame piece, 1 a
 *   white underscore, 2 the green next-slot bar; type: the callback's own layout. Return nonzero after setting the
 *   piece's alpha (draw[0x67]) to skip the game's test. NULL / 0: the game's own.
 * - OrreHook_NameEntryDrawName: the name text's draw (menuNameEntryDrawName, in the two text-draw files) for layout
 *   type 7, 8 or 10. Return nonzero after drawing (or hiding) it. NULL / 0: the game's own.
 * - OrreHook_NameEntryCommit: after the screen stored the name by mode (menuNameEntry_exact_80028FBC.c: mode 0 the
 *   player, held in the buffer for the title's savedataCreate; 1 Rui; 2 a nickname, record = the Pokémon's record;
 *   3 a box, index = the box). The stored name is the game's buffer. Observation only. */
extern s32 (*OrreHook_NameEntryPiece)(s32 piece, s32 type, void* window, u8* draw);
extern s32 (*OrreHook_NameEntryDrawName)(void* window, u8* draw, s32 type);
extern void (*OrreHook_NameEntryCommit)(s32 mode, s32 index, const void* stored, void* record);
#define ORRE_NAME_PIECE(piece, type, window, draw)                                                                   \
    do {                                                                                                              \
        if (OrreHook_NameEntryPiece != NULL && OrreHook_NameEntryPiece((piece), (type), (window), (draw)) != 0)       \
            return 0;                                                                                                 \
    } while (0)

/* B on the name menu (the list of NEW NAME and the presets that modes 0 and 1 show first; snatcherLeo, 2026-11-15:
 * "If a user hits B once more in [that name menu], they can get back to main menu ... users cannot back out of naming
 * RUI"; docs/port/naming-screen.md "B on Wes's name menu"):
 * - OrreHook_NameEntryListBack: menuNameEntry (menuNameEntry_exact_80028FBC.c), asked once as the name menu opens:
 *   given the mode, the index and OrreHook_NameEntryCaller, nonzero means B may leave the screen. The list's button
 *   callback ignores B (the game's own: B does nothing there), so the screen then sets OrreHook_NameMenuCanLeave while
 *   the list is open; B closes the list with -1, and the screen ends without a name (nothing stored, no commit hook;
 *   the request block's name is cleared and its result is ORRE_NAME_ENTRY_LEFT, which the game never writes: it writes
 *   0 or 1). NULL or 0: B does nothing on the name menu, as on the GameCube.
 * - OrreHook_NameEntryCaller: set by the code that opens the screen, around its menuNameEntryOpen, to say it can take a
 *   back-out (ORRE_NAME_CALLER_NEW_GAME: the title's New Game, gs_title.c, which then returns to the main menu); 0
 *   everywhere else (Rui's script, nicknames, boxes, a mod's call), so a screen whose caller can't handle a back-out is
 *   never left by B. Nothing else reads it; 0 or unread: as on the GameCube.
 * The rule per kind of name (player, partner, Pokémon, box, a mod's) lives in port/src/pc_name_keyboard.cpp. */
extern s32 (*OrreHook_NameEntryListBack)(s32 mode, s32 index, u32 caller);
extern u32 OrreHook_NameEntryCaller;
/* menuNameEntrySelectButton (menuNameEntry_exact_80028444.c), the name menu's button callback, which closes the list on
 * A and ignores B: nonzero lets B close it with "back" (+0x99; menuOpenCustom then returns -1). Set only by
 * menuNameEntry, around its list's menuOpenCustom, from OrreHook_NameEntryListBack's answer; 0, the default: B ignored,
 * as on the GameCube. */
extern s32 OrreHook_NameMenuCanLeave;
/* menuNameEntrySelectButton, when B closes a name menu that may be left (OrreHook_NameMenuCanLeave): OrrePort shortens
 * the fades on the way back to the main menu (snatcherLeo, 2026-11-15: about 2.8 s before). NULL: nothing, the game's own
 * fades (and the GameCube never gets here: B is ignored there). */
extern void (*OrreHook_NameMenuLeft)(void);
#define ORRE_NAME_CALLER_NEW_GAME 1u
#define ORRE_NAME_ENTRY_LEFT (-1)

/* gs_msg.c, GSmsgFindMessage, when no loaded bank has the key (OrrePort's own messages in the game's screens, e.g.
 * the Options screen's Language row; 2026-11-13): return the text in the disc's encoding (below 4 GB, kept for
 * the run) and, when outGroup isn't NULL, set *outGroup to a 16-byte group header whose font id (byte 3) the text uses.
 * NULL: not found, as on the GameCube. */
extern void* (*OrreHook_MessageExtra)(u32 key, void** outGroup);

/* menu_bios_range_8005D7F8.c, menuItemBiosGetPtr (kind 0) and menuSpriteBiosGetPtr (kind 1), for an index past the
 * table's end: an item (0x1C bytes) or sprite (0x18 bytes) record laid out as the table's (big-endian), owned by
 * OrrePort and kept below 4 GB (OrrePort's own parts of the game's screens; 2026-11-13). NULL: as on the
 * GameCube (the getters return NULL). */
extern void* (*OrreHook_MenuExtra)(int kind, u32 index);

/* menu_bios_range_8005D7F8.c, menuDataBiosGetPtr / GetXY / SetXY / GetType, for a menu number past the menu table's end:
 * a whole menu record (0x1C bytes, laid out as the table's, big-endian; its callbacks OrrePort's own C functions) owned
 * by OrrePort and kept below 4 GB, or NULL. NULL: as on the GameCube (such a number is treated as menu 1). (
 * 2026-11-14: the main menu's SAVE FILES list.) */
extern void* (*OrreHook_MenuDataExtra)(u32 index);

/* ---- Word pictures per language (language piece 4, 2026-11-15; port/src/pc_word_pictures.cpp sets these;
 * docs/port/word-pictures.md). Why three hooks: a menu picture with words is a sprite record (which piece of which
 * texture) drawn into a window item's box, and the texture is found by its id alone. The European disc gives each
 * language its own textures and its own piece rectangles, so another language needs the record swapped, the texture
 * found, and the item's box sized to the new piece. All three are look only (no random draws, no timing). ---- */

/* menu_bios_range_8005D7F8.c, menuSpriteBiosGetPtr, for an index inside the sprite table: given the game's record,
 * return another record (0x18 bytes, laid out as the table's, big-endian, owned by OrrePort, below 4 GB and kept for
 * the run) to use instead: the same piece cut from the chosen language's picture. NULL: the game's record, as on the
 * GameCube. */
extern void* (*OrreHook_MenuSprite)(u32 index, const void* record);

/* window_r50_80104A94_o2.c, _windowCreateItemSprite, after a window item's picture is set up (a sprite not sized to the
 * window): given the item's number and its sprite's, the item's drawn place and size (x, y, width, height; in/out).
 * OrrePort places a language's piece where that language's game places it: the language's own item record (its box,
 * and the European game's alignment flag in the record's low 4 bits: stretched, centred, or left / right on the bottom)
 * read from the player's disc. Without a matching record: the box scaled to the piece around its centre. *Changed
 * 2026-11-15* (the item's number added, for the language's item record). NULL: the item's own place and size, as on
 * the GameCube. */
extern void (*OrreHook_ItemSpriteBox)(u32 itemIndex, u32 spriteIndex, s16* x, s16* y, s16* width, s16* height);

/* window_r50_80104CA0_suffix.c, windowCreateCursorSprite, before the cursor's pieces are placed: given the item the
 * cursor sits on (its record in the item table, big-endian), the place the cursor is drawn from (x, y; in/out; the
 * cursor sprite's own offsets are added after). OrrePort moves it as the chosen language's game does (the European
 * and Japanese main menus put the glove a few pixels further left). Look only: moving between items still reads the
 * items' own places. NULL: the item's place, as on the GameCube. */
extern void (*OrreHook_CursorSpriteAt)(const void* item, s16* x, s16* y);

/* menuCB_BattleStart.c, fn_80060EF4: the Battle Mode stadium name header's item callback (the VS screen's items
 * 0xBF1-0xBFE and the results screen's 0x1097-0x10A4; run every frame before the item draws), after it decided the
 * item is shown: given the item's number and the stadium number the item stands for (-1: the Mt. Battle header), the
 * item's drawn x, y, width and height (in / out). Why: the European game lays this header out in code, not from its
 * item table (its 0x80064078: the name centred on the screen, a two-piece name with the "stadium" word first in German,
 * French, Italian and Spanish), so the item-table placement (OrreHook_ItemSpriteBox) can't place it. OrrePort places
 * the language's pieces the same way (language piece 4, port/src/pc_word_pictures.cpp; added 2026-11-15). Look only.
 * NULL: the item's own place and size, as on the GameCube. */
extern void (*OrreHook_StadiumHeader)(u32 itemIndex, s32 stadium, s16* x, s16* y, s16* width, s16* height);

/* gs_res.c, GSresFindById (the lookup the menu and window drawing use for textures), when no loaded resource has the
 * id: return the data to use for it (a texture already run through GStextureLoad, below 4 GB, kept alive while the
 * game may draw it), or NULL. OrrePort answers only for its own language texture ids, and only while the US texture
 * they replace is loaded, so the game's "not loaded in this room, not drawn" rule holds. NULL: not found, as on the
 * GameCube. */
extern void* (*OrreHook_ResourceMissing)(u32 id);

/* gs_title.c, titleNewContinueSelect (the title's Options screen): event 0 when it opens (before its menus open), 1
 * when it is cancelled with B, 2 when it is left with Done (2026-11-13, the Language row). Nothing: as on the
 * GameCube. */
extern void (*OrreHook_OptionsScreen)(int event);

/* gs_title.c, titleNewContinueSelect: the Options screen cancelled with B. Return 1 to keep the choices made on the
 * screen (Sound and Rumble are then not put back; snatcherLeo, 2026-11-15: "The "B" button would keep whatever selection you
 * made"), 0 to put them back. Nothing: as on the GameCube (B puts them back). */
extern int (*OrreHook_OptionsCancelKeeps)(void);

/* gs_title.c, fn_80020618 (the Options screen's cursor callback), with the keys pressed this frame and the screen's
 * selection (0 Sound, 1 Rumble, 2 Done: the game's numbers): return 1 when it handled the keys itself (the row order
 * and any row the game doesn't have), 0 to let the game handle them. NULL: as on the GameCube. */
extern int (*OrreHook_OptionsKeys)(s32* selection, u16 keys);

/* gs_title.c, titleMenuMain (the title's main menu, menu 0xAA), for a choice the game's switch doesn't know (its
 * default case, which the game uses for Quit, choice 5): return 1 when OrrePort handled the choice (an entry it added,
 * such as LOAD SAVE, choice 6) and set *nextState to the menu loop's next state (1 = back to the main menu); return 0
 * to let the game go on (Quit: back to the title). NULL: as on the GameCube (2026-11-14, LOAD SAVE). */
extern int (*OrreHook_TopMenuChoice)(s32 choice, s32* nextState);

/* gs_title.c, titleMenuMain (the PC copy), each time the main menu (menu 0xAA) opens (its state 1, also after Options
 * and the Continue question): OrrePort adds its entries to the menu's item chain and moves the game's own there (the
 * menu counts its entries when it opens), and remakes LOAD's pictures if the game's language changed. NULL: the menu as
 * on the GameCube (2026-11-14, LOAD SAVE; *moved 2026-11-15* from the function's start to every open). */
extern void (*OrreHook_TopMenuOpen)(void);

/* gs_title.c, titleDrawOptionHelp, every frame the main menu's help line is drawn, with the glove's choice:
 * - a choice past the game's six (its help table has exactly six keys; the Options screen's table follows it in
 *   memory, so reading past it would show an Options line): return the message key to show, or 0 to show nothing
 *   (2026-11-14, LOAD SAVE's help line);
 * - one of the game's own six (0-5; *added 2026-11-16*, CONTINUE's line naming the loaded save, snatcherLeo): return a key to
 *   show instead of the game's own line, or 0 for the game's own line.
 * NULL: as on the GameCube. */
extern u32 (*OrreHook_TopMenuHelp)(s32 choice);
/* gs_title.c, titleDrawOptionHelp, right after the game's own help line for one of its six choices is printed (key: the
 * line's message; alpha: the help's fade, the low byte of the print colour; window: the drawing callback's own first
 * argument, for drawing a menu sprite there), in the help box's own drawing (its clip and origin). Why (2026-11-21, snatcherLeo:
 * a yellow "GameCube-accurate" badge in accurate sessions): OrrePort draws a small badge beside the game's line without
 * touching its words (pc_title_loadsave.cpp). NULL: nothing, as on the GameCube. All platforms. */
extern void (*OrreHook_TopMenuHelpDrawn)(s32 choice, u32 key, u32 alpha, void* window);

/* gs_title.c, titleMenuMain, when the Continue question ("Is it okay to continue from here?") opens: given the game's
 * initial answer (1 = the cursor starts on NO, 0 = on YES), return the one to use. OrrePort's tweak returns 0 (YES)
 * in the normal mode and leaves 1 in GameCube-accurate mode and while a recording drives the pad (snatcherLeo, 2026-11-14).
 * NULL: as on the GameCube (NO). */
extern s32 (*OrreHook_ContinueInitialAnswer)(s32 initial);

/* gs_title.c, titleMenuMain (the PC copy), state 3, when the Continue question's message window opens: given the
 * game's message key (0x3C22, "Is it okay to continue from here?"), return the key to show. OrrePort returns its own
 * two-line question ("Your save file has been loaded!" / "Continue your story now?") once, right after a save was
 * picked in LOAD's SAVE FILES list, outside GameCube-accurate mode (snatcherLeo, 2026-11-15 / 16); every other time the
 * game's key. NULL: as on the GameCube. Call site helper: ORRE_CONTINUE_QUESTION. */
extern u32 (*OrreHook_ContinueQuestion)(u32 key);

/* gs_title.c, titleMenuMain (the PC copy), when the New Game question opens: given the game's initial answer (1 = NO)
 * and whether a story save exists (the game's own save status 4, which also picks the question's wording), return
 * the one to use. OrrePort's tweak returns 0 (YES) only when there is no story to lose, in the normal mode; with a
 * story saved, in GameCube-accurate mode and while a recording drives the pad it leaves 1 (snatcherLeo, 2026-11-14: "if a
 * user DOESN'T have a story mode to continue from ... then the YES will be the default ... if a user DOES have a
 * story mode save ... it still stays on "NO" ... just to protect users"). NULL: as on the GameCube (NO). */
extern s32 (*OrreHook_NewGameInitialAnswer)(s32 initial, s32 hasStory);

/* memcard.c, memcardStartTask, before every memory-card task starts (task_kind: 1 the title's load, 4 a save, 7 the
 * options save, ...): OrrePort may prepare the card first. Used for rule R4 (2026-11-14, snatcherLeo approved): a save
 * whose file was deleted from the Saves folder while the game ran is put back from its newest kept version, so the
 * game's save finds its file instead of failing. NULL: as on the GameCube. */
extern void (*OrreHook_CardTaskStart)(s32 task_kind);
/* memcard.c, memcardEndTask, after every card task with its kind and result (2.4, 2026-11-14; pc_names.cpp):
 * OrrePort writes the extras file beside the save when a save lands (kind 4) and reads it when one loads (kind 1).
 * Observation only. NULL: as on the GameCube. */
extern void (*OrreHook_CardTaskEnd)(s32 task_kind, s32 result);
/* memcard.c, _memcardMsgOpen, before a card task's message opens (error: the game's MEMCARD_MSGTYPE, 12 = "Your save
 * file has been loaded!", message 0x44E5; task_kind as above): nonzero = show nothing, as the game does for error 0 (no
 * window, no wait for A; the task goes on). 2026-11-16: the quiet re-read after the PC screen's Main Menu (snatcherLeo:
 * "it should just skip straight to loading the main menu"); every other load keeps its message. NULL or 0: the message,
 * as on the GameCube. */
extern s32 (*OrreHook_CardMessageSkip)(s32 error, s32 task_kind);
/* pokemon_range_8011F77C.c, pokemonToMenuPokemonStatus, after it copies a Pokémon's nickname into a menu record (the
 * party menu prints that copy): the copy's address and the Pokémon's record, so OrrePort can show the full name for the
 * copy (2.4, 2026-11-14; pc_names.cpp). Observation only. NULL: nothing. *Corrected 2026-11-16:* the battle HP
 * boxes print a later copy of a copy, never this address; pc_names.cpp finds those through the battle field. */
extern void (*OrreHook_MenuNameCopy)(void* copy, const void* record);

/* gs_msg.c: long names in fixed menu spots (2.4, 2026-11-14; decided with snatcherLeo, mockups 102-102d; pc_names.cpp):
 * - OrreHook_MessageFit: msgPrintRect / GSmsgPrintRect, after the print's task is set up and before it draws: the
 *   hook may set the task's scale (+0x60 x, +0x64 y) and start (+0x04 / +0x08) so a name too wide for its place fits
 *   (the Continue screen's and the P★DA card's name resized, the P★DA menu's e-mail row narrowed).
 * - OrreHook_MessageFitRect: GSmsgGetRect's result for a key (width in the high half, height in the low): the size
 *   the print will really have, so what the game places after it follows.
 * NULL: as on the GameCube. */
extern void (*OrreHook_MessageFit)(u32 key, u8* task);
extern s32 (*OrreHook_MessageFitRect)(u32 key, s32 rect);
/* The place a generic print is drawn for (2026-11-15): set by game code around a print whose message key is a
 * generic "insert a value" line used on many screens, so the fit above can tell that one place apart; 0 everywhere
 * else. Places: 0x155 the summary page's OT name (gbaCommunication.c, menuStatusDrawPokemonInfo; snatcherLeo: a 12-letter
 * name ran into "ID No."). Nothing reads it but the fit; 0 or unread: as on the GameCube. */
extern u32 OrreHook_PrintPlace;

/* ---- The naming keyboard (2026-11-14, snatcherLeo's keyboard B; docs/port/naming-screen.md "Building keyboard B") ----
 * Why: OrrePort's redone name screen (QWERTY pages Aa / ÀÉ / #?!, Shift, typing and the mouse) replaces only the
 * letter grid and its input; the game's own panel, buttons, name field, model and background stay. port/src/
 * pc_name_keyboard.cpp sets these in the normal mode; in GameCube-accurate mode and while a recording drives the pad
 * nothing is registered, so the game's own screen runs unchanged (snatcherLeo, 2026-11-14: "if a user is in Gamecube
 * Accurate mode, we just fallback to the original keyboard"). `window` is the keyboard window (menu 0x6E; its first
 * argument at +0x60 is the NAME_ENTRY_ARG block, game/name_entry_arg.h). All platforms. */

/* menuNameEntryCursor (menuNameEntry_exact_80027D58.c), once a frame while the keyboard is open: nonzero means OrrePort
 * handled this frame's input itself (it wrote the name and its length, moved its cursor, and set the window's "done"
 * byte +0x98 to finish); 0 lets the game's own input run. */
extern s32 (*OrreHook_NameEntryInput)(void* window);

/* menuNameEntryDraw50Text (menuNameEntry_candidate_800275F4_gc20.c), the letter grid's draw item: nonzero means
 * OrrePort drew the grid itself (its pages, the SPACE key); 0 draws the game's own grid. */
extern s32 (*OrreHook_NameEntryDrawGrid)(void* window);

/* menuNameEntryDraw50Cursor (menuNameEntry_exact_8002749C.c), the pulsing cursor box: given the game's box for the
 * current cursor (top-left x / y and the box's width and height before its pulse), OrrePort may move or size it for
 * its own grid. Returns 1 to show the box as given back, 2 to hide it (the button column, or the keyboard-and-mouse
 * look), 0 to keep the game's own placement. */
extern s32 (*OrreHook_NameEntryCursorBox)(void* window, s32* x, s32* y, f32* width, f32* height);

/* The button labels the keyboard changes (menuNameEntry.c): which 0 = the X button's label
 * (menuNameEntryDrawXButtonText: the next page's name; OrrePort shows what X will do, lower / UPPER / MORE), which 1 =
 * the R hint under the grid (fn_80027740; OrrePort shows its page tabs), which 2 = BACK's label (fn_800277B8) and
 * which 3 = DONE's (fn_8002777C), where OrrePort draws the Backspace / Enter key caps over the pad icons while the PC
 * keyboard is used and returns 0 so the game's own label prints. `draw` is the item's draw record (NULL for 1-3).
 * Nonzero: OrrePort drew the label itself. */
extern s32 (*OrreHook_NameEntryDrawLabel)(s32 which, void* window, u8* draw);

/* The name field's blinking preview of the letter under the cursor (the inline name draw in
 * menuNameEntry_r56_800268F0_gc20p1_o4s.c and menuNameEntry_candidate_80026B44_gc20.c): the game looks the letter up
 * in its own grid, which would be the wrong letter under OrrePort's grid. Nonzero: OrrePort set *letter (the game's
 * big-endian character, 0 for no preview: a button, SPACE). */
extern s32 (*OrreHook_NameEntryPreview)(void* window, u16* letter);

/* menuDaemon (menu_r50_80102014_prefix.c; the older menu.c copy kept in step), the standard A / B handling of a menu
 * window: buttons that close the window as B does, for this menu number (window id). Returns a MENU_KEY_ mask, 0 for
 * none. NULL: B alone, as on the GameCube (2026-11-15: Start closes the field pause menu, 0x41, and its Mt. Battle
 * version, 0x109; snatcherLeo: "just for this specific PAUSE MENU"). */
extern u32 (*OrreHook_MenuBackKeys)(u32 menuId);

/* menuCursorNormal (menu_r50_80102F38_o3.c; the older menu.c copy kept in step): nonzero = this menu (window id)
 * wraps round, as the game's own cursor mode 2 menus do (Up on the top entry goes to the bottom one, Down on the
 * bottom to the top, held or tapped). NULL or 0: only the game's own wrapping menus wrap (2026-11-15: the field
 * pause menu, 0x41 and 0x109; snatcherLeo: "this makes navigation far better"). */
extern s32 (*OrreHook_MenuWrap)(u32 menuId);

/* heroMoveRunEvent (gs_range_8000D290.c, the field pause menu), each time the menu is about to open (menuId 0x41, or
 * 0x109 during a Battle Mode run): OrrePort adds its entries to the menu's item chain, or puts the game's own back
 * when it doesn't (the menu counts its entries when it opens). NULL: the menu as on the GameCube (2026-11-15,
 * PC MENU). */
extern void (*OrreHook_FieldMenuOpen)(s32 menuId);

/* fn_8000D3AC (the pause menu's per-frame callback), for a cursor position past the game's own four on menu 0x41: the
 * item that slides out (and back) for it, or 0 for none. Not called for the game's own positions 0-3. NULL: as on the
 * GameCube (2026-11-15, PC MENU, position 4). */
extern s32 (*OrreHook_FieldMenuItem)(s32 selection);

/* heroMoveRunEvent, menu 0x41, for a choice the game's chain doesn't know (its else branch, which leaves the menu):
 * return 1 when OrrePort handled it (PC MENU, choice 4: the PC menu room was opened and has closed) and the pause menu
 * should open again, 0 to leave as the game does. NULL: as on the GameCube (2026-11-15). */
extern s32 (*OrreHook_FieldMenuChoice)(s32 choice);

/* Where the player stands, for a save made through PC MENU (Access PC menu anywhere; 2026-11-15): the field room,
 * the room before it and the entry used to come in (as the game's room-change record keeps them), the hero's position
 * and facing (degrees, as floorChangePos takes it). */
typedef struct OrreFieldSpot {
    u32 room;
    u32 prevRoom;
    u32 entry;
    f32 x, y, z;
    s32 facingDegrees;
} OrreFieldSpot;

/* gs_range_8000D290.c, OrrePort's PC MENU choice: as the PC menu room opens, where the player stands (room 0 when the
 * spot can't be resumed: not a field room, or a story area with its own return point), and NULL when the PC menu room
 * has closed. Observation only; OrrePort keeps the spot when a save lands in between. NULL: nothing. */
extern void (*OrreHook_FieldMenuSpot)(const OrreFieldSpot* spot);

/* gs_title.c, titleMenuMain (the PC copy), when Continue is answered YES, in place of floorReturnToSavedRoom: return 1
 * with *spot filled to go to that exact spot instead of the save's return point (the last Pokémon Center). Only for
 * Continue (and the main menu's LOAD, which ends there); whiteouts use floorReturnToSavedRoom elsewhere and are never
 * changed. NULL or 0: as on the GameCube. */
extern s32 (*OrreHook_ContinueSpot)(OrreFieldSpot* spot);

/* std024_dialogDisplayPCBoxMenu (menu/pda_range_80037158.c), as the PC screen is left, before its room's fade-out and
 * the field's fade-in are set: observation only. NULL: nothing (2026-11-15). */
extern void (*OrreHook_PcMenuExit)(void);

/* fn_800388C4 (the PC room's leave job, menu/pda_candidate_8003842C.cpp; pda_range_80037158.c kept in step): nonzero =
 * close the PC screen's eleven windows all at once and wait once, instead of one after another (each waiting for its
 * closing animation: 66 frames behind the black screen, measured). NULL or 0: one after another, as on the GameCube
 * (2026-11-15). */
extern s32 (*OrreHook_PcMenuCloseTogether)(void);

/* menuDaemon (menu_r50_80102014_prefix.c; the older menu.c copy kept in step), a closing window waiting for its closing
 * sequence: nonzero = skip the sequence and close the window now. NULL or 0: the window closes when its sequence ends,
 * as on the GameCube (2026-11-15: the PC screen's window 0x1A slides away for 60 frames behind the black screen
 * when PC MENU's PC is left). */
extern s32 (*OrreHook_WindowCloseNow)(u32 windowId);

/* fadeSet (effect/fade.c), every timed fade: the length to use, in seconds (given the game's). NULL: the game's own
 * (2026-11-15: the PC screen opened from PC MENU fades out and back in at half length; snatcherLeo: "slightly"). */
extern f32 (*OrreHook_FadeDuration)(u16 type, f32 duration);

/* ---- Main Menu on the PC screen (2026-11-15; port/src/pc_pc_screen.cpp sets these; docs/port/pc-menu-anywhere.md,
 * "Main Menu on the PC screen: as built"). Why: snatcherLeo asked for a way back to the title's main menu from the PC screen
 * ("Main Menu", approved mockup 2026-11-15). The PC screen's list (menu 0x1D) has four rows whose meaning is fixed by
 * row number in the game's code (label colours, slide rows, the driver's switch) and two 4-entry tables (the lit
 * piece's place, the icon wheel's target angle), so a fifth row needs these small hooks. All look or flow only: no
 * random draws. NULL: the game's own four rows, exactly as on the GameCube. All platforms. ---- */

/* std024_dialogDisplayPCBoxMenu (menu/pda_candidate_8003842C.cpp; pda_range_80037158.c kept in step), as the PC screen
 * opens (before its windows open): OrrePort lays out its rows (or puts the game's own records and tables back). */
extern void (*OrreHook_PcMenuOpen)(void);
/* the same driver, for a choice its switch doesn't know (vanilla: none; the list then simply reopens): return 1 to leave
 * the PC screen as Exit does, 0 to show the list again. NULL: the list reopens, as on the GameCube. */
extern s32 (*OrreHook_PcMenuChoice)(s32 choice);
/* fn_80038380 (the list's cursor callback, menu/pda_exact_80038250.c; pda_range copy kept in step), with the window's
 * cursor row (+0x95) and the key record's repeat word: return 1 when the hook moved the row itself (the screen order
 * differs from the choice order once rows are added), 0 for the game's own +1 / -1. NULL: the game's own. */
extern s32 (*OrreHook_PcMenuCursor)(s8* row, u16 keys);
/* fn_80038170 (the lit piece's callback, menu/pda_candidate_80038170.c; pda_range copy kept in step), for a row past
 * the game's four only (its place table has exactly four entries): the lit piece's place (x, y). Never asked for rows
 * 0-3. NULL: (0, 0); the GameCube never has such a row. */
extern void (*OrreHook_PcMenuRowPlace)(s32 row, f32* x, f32* y);
/* pdaUpdateOrbitSprite (the icon wheel, menu/pda_candidate_800379E8.c; pda_range copy kept in step), every icon
 * callback: given the cursor's choice, the target angle (the game's table entry for choices 0-3, the current target
 * past them, which the table must never be read for), the turn's speed factor (1) and the spread the icons are drawn
 * with around the front slot (1): the hook may change all three (a fifth row's target; the speed so each row still
 * takes as long to turn to as with four; *added 2026-11-15, snatcherLeo: "we use that same spacing the original game had"*
 * the spread, so neighbours are drawn the game's 90 degrees apart although five rows sit 72 apart on the turn).
 * NULL: the game's own. */
extern void (*OrreHook_PcMenuWheel)(s32 choice, f32* target, f32* stepScale, f32* angleScale);
/* fadeSet (effect/fade.c), before OrreHook_FadeDuration: nonzero turns this fade into "stay black" (a fade-out of 0
 * seconds, as movieGsLogoExit does), used while the game is on its way back to the main menu so the field never shows.
 * NULL or 0: the fade as asked. */
extern s32 (*OrreHook_FadeHold)(u16 type);
/* _floorManagerThread (gs_floor.c), every frame of its RUNNING state with no room change pending: the room on screen,
 * the room stack's depth (0 = a field or story room, 1 = a menu room pushed over it), whether the room has settled
 * (its resources running, isActive 4) and whether the field pause menu is open (lbl_8047A2A0). Return a room number to
 * change to it the way Battle Mode's end enters the top menu (floorLink(room, 0)), after the game code resets the state
 * a fresh boot has outside the save (the live return points in _floorChangeInfo +0x3C-+0x51, the play-time clock);
 * -1 to fade the screen back in (OrrePort gave up waiting); -2 the same and also bring the music back up (500 ms) when
 * OrreHook_FieldMenuUnduckMusic left it ducked (added 2026-11-16); 0 for nothing. NULL: nothing, as on the GameCube. */
extern s32 (*OrreHook_RoomLeave)(u32 room, s32 depth, s32 settled, s32 pauseOpen);
/* gs_floor.c, fn_80100B24 (the room manager's per-frame phases), the moment the room's entry-phase scripts have all
 * finished (its on-entry setup has placed its people from the story flags) and before its main loop starts: room is
 * the room id, depth the room stack's depth (0: the field room at the bottom). Why (2026-11-21, snatcherLeo: "yes, put Vander
 * back on the floor"): a Continue from a PC MENU spot or an autosave can land in a room whose setup has no branch for
 * the story state, so a person a scene left lying or kneeling stands at the default spot; OrrePort's pose rows
 * (port/ui/game/pose_rows.ini, pc_pose_rows.cpp) place that person here, the way the room's own setup places them in
 * the states it does handle. NULL: nothing, as on the GameCube. All platforms (plain C function pointer). */
extern void (*OrreHook_RoomEntryDone)(u32 room, s32 depth);
/* gs_range_8000D290.c, after PC MENU's PC screen has closed: 0 = close the pause menu the game's own way (as after B)
 * instead of opening it again (the game is on its way back to the main menu). NULL or 1: it opens again. */
extern s32 (*OrreHook_FieldMenuReopen)(void);
/* heroMoveRunEvent (gs_range_8000D290.c), as the field pause menu closes (its soundUnduck(500 ms, music, effects)): 0 =
 * bring back only the effects' volume and leave the music ducked, because the field music is about to be replaced by
 * the title's song (the project's helper 2026-11-16: on the way from the PC screen's Main Menu to the main menu the unduck
 * raised the field music behind the black screen just before it was cut; snatcherLeo heard a swell). NULL or nonzero: music
 * and effects, as on the GameCube. Sound only: no random draws. */
extern s32 (*OrreHook_FieldMenuUnduckMusic)(void);

/* ---- PC MENU saves made in a scene's open window (the softlock fix; 2026-11-20, the project's helper; port/src/pc_save_windows.cpp
 * sets these; docs/port/pc-menu-anywhere.md, "Saves made in the middle of a scene"; the project notes).
 * Why: a PC MENU save made between two parts of a story scene (Rui's sack in Phenac: flag 993 set, 996 clear) holds a
 * state the room's own setup can't rebuild, and Continue then hangs. OrrePort writes such a save with the window's
 * story flags rolled back (a state the game itself was in), says so before saving, and watches for a hang after a
 * Continue. Both hooks only report where the game is; the feature lives in port/. No random draws. NULL: nothing, as
 * on the GameCube. All platforms. ---- */

/* pcMenuSave (menu/pda_candidate_8003842C.cpp; pda_range_80037158.c kept in step), the PC's Save: step 0 just before
 * its "Would you like to save your progress?" YES / NO opens, 1 right after it is answered, 2 just before the card
 * write (memory-card task 4, after the return point is put into the save image), 3 right after the write returns. On
 * the driver's task, which may wait (OrrePort's notice uses the game's message window there). NULL: nothing. */
extern void (*OrreHook_PcSaveStep)(s32 step);
/* heroMoveMain (hero_move.c), the hero step of a room's main loop, around the scripts it runs synchronously for the
 * player: an auto event, the touch / check events found this frame (heroDispatchEvent), and a talk (updateChat): 1
 * just before, 0 just after. A script that never returns leaves it at 1: OrrePort's hang watch counts how long. NULL:
 * nothing. */
extern void (*OrreHook_HeroEventScript)(s32 running);

/* ---- Frame smoothing, step (c): the camera blend (the project's build helper 2026-11-16; port/src/pc_smooth.c sets it while
 * smoothing is on; docs/port/frame-smoothing-plan.md section 13) ---- */
/* HSD_CObjSetCurrent (src/hsd/cobj.c), after the view matrix is set up: the camera the following draws are built with
 * (the camera's address, HSD's render pass, its 3x4 view matrix, the interest WObj's stored position or NULL).
 * Observation only. cobj.c declares it weak itself (HSD is built into several units). NULL: nothing. */
extern void (*OrreHook_CameraSet)(const void* camera, s32 renderPass, const f32* view, const f32* interest);
/* Step (d), the model blend (2026-11-17; docs/port/frame-smoothing-plan.md section 14): HSD_PObjDisp (src/hsd/pobj.c)
 * reports the model piece it is about to set up and draw (the current JObj and the PObj), then (NULL, NULL) once the
 * piece is drawn. Observation only. pobj.c declares it weak itself. NULL: nothing. */
extern void (*OrreHook_ModelDraw)(const void* jobj, const void* pobj);
/* Item (a), the particle blend (2026-11-17; docs/port/frame-smoothing-plan.md section 18): psDispMain
 * (src/game/psdisp.c) reports each particle it is about to draw (its address; a guard, bank << 16 | id number; its
 * remaining life), then (NULL, 0, 0) once it is drawn. Observation only. psdisp.c declares it weak itself. NULL: nothing. */
extern void (*OrreHook_ParticleDraw)(const void* particle, u32 guard, u32 life);
/* Round 11, 2D at 120 (2026-11-17; docs/port/frame-smoothing-plan.md section 24): a named 2D piece the following
 * draws belong to (kind 0: the piece is done): its kind (the window sprite's kind, or a pause-menu piece), a variant
 * (its picture: a colour change shows the newest), an id (its window) and a value (a bar's drawn width, a panel's
 * offset or alpha) as float bits. Reported when the gfx request buffer plays the piece back (command 0x7E, recorded by
 * pc_orre_2d_tag), so it lands in order with its draws. Observation only. NULL: nothing (and nothing is recorded). */
extern void (*OrreHook_2DPiece)(u32 kind, u32 variant, u32 id, u32 valueBits);

/* ---- Widescreen groundwork: the 2D inventory (2026-11-19; docs/port/widescreen-plan.md section 6, "The 2D inventory
 * switch"; port/src/pc_ui_inventory.c sets it only with the test switch ORRE_UI_INVENTORY=1) ----
 * Why a hook of its own and not OrreHook_2DPiece: 2DPiece names only the pieces frame smoothing blends (the gauges,
 * the menu daemon's sprites, windowDrawSprite2, the fades), carries no rectangle, and when set makes the game record
 * extra commands into its request buffer. The inventory needs every 2D piece with its place, and must leave the game's
 * buffers exactly as they are. So this one is called at the moment the game decides to draw (record time), with plain
 * values read at the call site, and records nothing. Observation only: plain reads, no state change, no random draws.
 * Platforms: the same everywhere (plain C); the caller address uses GCC / Clang's __builtin_return_address (the PC
 * build's compilers on Windows, Linux and macOS; MSVC would need _ReturnAddress()). */
enum {
    ORRE_UI2D_SPRITE = 1, /* winSpriteDraw (win_sprite.c): a window sprite actually drawn (visible, alpha above 0) */
    ORRE_UI2D_ORTHO = 2,  /* gfxSetOrtho (gs_gfx_range_800D9AF0.c): a code-drawn flat 2D projection is set */
    ORRE_UI2D_ENV = 3,    /* spriteSetEnv (gs_thread_hi_range_800FE35C.c): the window 2D camera is set (any caller) */
    ORRE_UI2D_WINDOW = 4  /* menuDaemon's draw loop (menu_r50_80102014_prefix.c): a window is drawn at its origin; flags
                           * bit 0: it has a draw callback (code-drawn content inside the window: lines, text, gauges) */
};
typedef struct OrreUi2DPiece {
    u32 source;          /* ORRE_UI2D_* */
    u32 window;          /* the window's menu id (MenuWindow +0x04); 0: none (a sprite drawn with no window) */
    u32 activeWindow;    /* the window that takes input now (windowGetActiveID; 0: none) */
    s32 item;            /* the sprite's item id (its node +0x06; 0 for windowDrawSprite2's throw-away node) */
    u32 texture;         /* the sprite's texture resource id (when it draws a texture), else 0 */
    u32 primitive;       /* the sprite's code-drawn primitive (0x10000-0x10004, when it draws one), else 0 */
    s16 cropX, cropY;    /* where in its texture the sprite's picture starts (tells atlas parts apart) */
    s16 x, y, w, h;      /* logical 640 x 480 pixels: a sprite's drawn box (window origin + its place, scaled about its
                          * centre); an ortho's projection box (left, top, width, height); env: the 2D origin, 0 x 0 */
    s32 recordMode;      /* GSgfx's recordMode at the call (1: recording; playback replays ortho calls too) */
    u32 flags;           /* per source (ORRE_UI2D_WINDOW: bit 0 = has a draw callback) */
    const void* caller;  /* the calling code's address (ortho and env; NULL for sprites and windows) */
} OrreUi2DPiece;
extern void (*OrreHook_Ui2DDraw)(const OrreUi2DPiece* piece);

/* ---- The developers' move viewer as a reference camera for Studio (the project's helper 2026-11-18; requested by the Studio
 * session; port side src/game/pc_waza_viewer.c, set only when ORRE_WAZA_VIEWER is given; docs/port/setup.md, "Booting
 * into the move viewer") ---- */
/* floor.c, fn_801128A0 (the room system's start-up), after the build kind picked the first room (retail: 0x399, the
 * Pokémon logo): returns the room to open first. NULL: the build kind's room, as on the GameCube. */
extern u32 (*OrreHook_FirstRoom)(u32 room);
/* wazaViewer.c, wazaViewerThread (the move viewer's own thread), each pass after the mode's step and before
 * cameraUpdate: hands over the viewer's work area (lbl_804673F8, 0x884 bytes) so the port can fill the command fields
 * the developers' debug menu wrote (+0x628 reload, +0x630 grid slot, +0x634 model id, +0x63C one-model camera,
 * +0x640 shiny) and adjust the camera and the model once spawned. NULL: nothing (the viewer idles, empty). */
extern void (*OrreHook_WazaViewerThread)(u8* work);
/* wazaViewer.c, wazaViewerUpdate (the viewer's per-frame draw callback, retail mode 1), after the attachment-point
 * dots: the model shown (an owner record), so the port can draw more of the viewer's own overlays there (the flat
 * backdrop, which the GameCube draws only in the developer build's mode 0). NULL: nothing. */
extern void (*OrreHook_WazaViewerDraw)(void* current);

/* ---- The archive cache's room for language files (2026-11-19; the Japanese release look's determinism fix;
 * port/src/pc_lang_files.cpp sets both while a language's archives are overlaid; docs/port/word-pictures.md) ---- */
/* gs_range_8017F3F8_middle.c, fn_8017F928 (the FSYS loader's ARAM cache reserving room for an archive piece): given
 * the piece's size and its cache key (the archive id; the entry's group id and resource id, or 0 / 0 and 0 / 1 for
 * the archive's header and table), returns the size the console's game would reserve for it (the US disc's piece in
 * its place). The allocator then reserves that, so its choices match the US archive's; the cache node keeps the real
 * size. NULL: the size itself, as on the GameCube. */
extern u32 (*OrreHook_FsysCacheRoom)(u32 size, u32 fileHandle, u32 key1, u32 key2);
/* the same place, right after a reservation smaller than the piece (aligned): its ARAM address, the room reserved and
 * the piece's real size, so the port keeps the bytes past the room aside (port/src/pc_arq.c) instead of letting the
 * copy run into the next block. NULL: not called. */
extern void (*OrreHook_FsysCacheReserved)(u32 aram, u32 room, u32 size);
/* the loader's eviction checks (gs_range_80179F4C.c and fsys_file_candidate_8017CED8.c, the LRU loops that free ARAM
 * until an archive fits) ask the same hook for the archive's total (key 0xFFFFFFFF / 0xFFFFFFFF; the slot's
 * totalDecompSize, which for an archive on the disc is its header's +0x20, the archive's length), so they evict as for
 * the US archive. The
 * GameCube build defines the macro as the field itself (in those two files). */
#define ORRE_FSYS_CACHE_TOTAL(slot)                                                                                    \
    (OrreHook_FsysCacheRoom != NULL                                                                                   \
         ? OrreHook_FsysCacheRoom((slot)->totalDecompSize, (slot)->fileHandle, 0xFFFFFFFFu, 0xFFFFFFFFu)              \
         : (slot)->totalDecompSize)

/* ---- Step 3.7's item tweaks (2026-11-20, the project's build helper; port/src/pc_bag_tweaks.cpp sets these; decided in
 * docs/port/tweaks-37-research.md sections 1-1i; how the bag and the party list work: docs/engine/bag-and-pc.md).
 * Why hooks: keep using an item from the party list, the TM design (no repeated "Booted up a TM", LEARNED), infinite
 * TMs, the remembered scent cursors and the USE / READ / CHECK / TOSS rule all change the game's own bag code in a few
 * places; each place gets one small hook whose NULL default is exactly the GameCube's, and the rules live in port/.
 * None draws random numbers (the bag, the party list and the item use functions draw none: verified by search,
 * tweaks-37-research.md section 1). All platforms (plain C function pointers). ---- */

/* menuPokemon.c, bagUseItemOnPokemon (the field bag only, lbl_8047A2E0 == 0), right after a bag action that may open the
 * party list returned "done" or "nothing used" and the bag took its count (kind 1: Use, bagItemUse; kind 2: Give,
 * bagItemGive): the item, the bag page (pocket), the party slot the player picked (-1 when none was picked: B, a refusal
 * before the list, an item that never opens it), the action's result (0 done: used, given or a TM taught; 3 nothing
 * used: "no effect", a refusal, a TM not learned) and the bag row the action used (in / out). Return nonzero to run the
 * same action again on *row (the party list comes straight back with the item still chosen); 0 to go back to the bag
 * as the game does. NULL: back to the bag, as on the GameCube.
 * *Changed 2026-11-20 (snatcherLeo's hand check, research 1k):* the result was added, so a taught TM can go back to the bag
 * while a TM not learned stays on the list. */
extern s32 (*OrreHook_ItemUseAgain)(s32 kind, u16 item, s32 page, s32 slot, s32 result, u32* row);
/* The party list held open between two uses of the same bag item (2026-11-20, snatcherLeo's hand check: "it just remains on
 * the party menu", research 1k). Each use function opens the party list (itemUseOpenPartyMenu) and closes it itself
 * (itemUseClosePartyMenu; bagItemGive closes it with menuPokemonClose), so a repeat closed the list (it slid apart) and
 * opened it again. The hook lets the close be skipped while the bag's loop is running, so the next use finds the list
 * still open and only resumes its cursor (the game's own path: the PP items' handlers call itemUseOpenPartyMenu again on
 * an open list when B leaves the move list). Called with one step at a time:
 *   ORRE_HOLD_BEGIN  menuPokemon.c, bagUseItemOnPokemon, before a field Use / Give (slot -1; return ignored);
 *   ORRE_HOLD_PICKED gs_event_exec.c, itemUseOpenPartyMenu, after the list returned (the slot picked, -1 for B; return
 *                    ignored: OrrePort remembers the last Pokémon chosen for a bag item);
 *   ORRE_HOLD_CLOSE  gs_event_exec_exact_80014198.c, itemUseClosePartyMenu, and gs_event_exec.c, bagItemGive, just
 *                    before the list would close (the slot picked): nonzero skips the close (the list stays open);
 *   ORRE_HOLD_LEAVE  gs_title.c, menuUseItemEvolution and menuUseItemListLVUp, just before the evolution scene: nonzero
 *                    means the list is still held, and the caller closes it now (the scene is another room);
 *   ORRE_HOLD_END    menuPokemon.c, bagUseItemOnPokemon, after its loop (slot -1): nonzero means the list is still held,
 *                    and the caller closes it now (back to the bag, as the game does after the last use).
 * NULL or 0: every close happens where the game does it, as on the GameCube. */
extern s32 (*OrreHook_PartyListHold)(s32 step, s32 slot);
#define ORRE_HOLD_BEGIN 0
#define ORRE_HOLD_PICKED 1
#define ORRE_HOLD_CLOSE 2
#define ORRE_HOLD_LEAVE 3
#define ORRE_HOLD_END 4
#define ORRE_PARTY_LIST_HOLD(step, slot) \
    (OrreHook_PartyListHold != NULL && OrreHook_PartyListHold((step), (s32)(slot)) != 0)
/* menuPokemon.c, menuPokemonSub, as the party list opens with a cursor (not the "show only" list of Sacred Ash): given
 * the list's mode (3 item use in the field, 4 in battle, 5 give, ...) and the item it was opened for, the slot the
 * cursor starts on, or -1 for the game's own (the first Pokémon). A slot without a Pokémon is ignored. NULL: the first
 * Pokémon, as on the GameCube. */
extern s32 (*OrreHook_PartyCursorStart)(s32 mode, u16 item);
/* gs_title.c, itemFieldUseTM, before "Booted up a TM." and its "Teach it to a Pokémon?" YES / NO: nonzero skips both
 * (and the boot-up sound) and opens the party list at once. NULL or 0: the game's own text and question, as on the
 * GameCube. *Changed 2026-11-20 (snatcherLeo's hand check, research 1k):* only for staying on the party list after a TM was
 * not learned (a refusal, or "did not learn"), which is the same use going on; every new use from the bag asks as the
 * game does, and a taught TM goes back to the bag. */
extern s32 (*OrreHook_TmSkipAsk)(u16 item);
/* gs_title.c, itemFieldUseTM, after a TM was taught: given the count the game uses up (1 for a TM; HMs never ask, they
 * use none), the count to use up. Infinite TMs returns 0, the path an HM takes. NULL: the game's count. */
extern u32 (*OrreHook_TmUsedCount)(u16 item, u32 count);
/* menuPokemon.c, menuPokemonDrawItem, the party list's ABLE / NOT ABLE label with a TM or HM: given the game's label key
 * (0x2B64 ABLE, 0x2B65 NOT ABLE), the Pokémon's record and the item, the key to print (OrrePort's LEARNED for a Pokémon
 * that already knows the move). Look only. NULL: the game's label. */
extern u32 (*OrreHook_TmLabel)(u32 key, void* pokemon, u16 item);
/* *Added 2026-11-24 (0.1.2's stone label; port/src/pc_bag_tweaks.cpp sets it; the project notes):*
 * menuPokemon.c, menuPokemonDrawItem, for an item that is NOT a TM or HM in the party list's item modes (3 the field,
 * 4 battle), at the two panes where the TM label goes: given the item, the row's Pokémon record and the mode, a label
 * key to print in the TM label's place (the game's own 0x2B64 ABLE / 0x2B65 NOT ABLE, drawn and placed exactly as the
 * TM's), or 0 for none. Look only. NULL or 0: the row as on the GameCube (no label for such items). Why a hook: the
 * game shows ABLE / NOT ABLE only for TMs; snatcherLeo (2026-11-24) wanted evolution stones to show it too, built in. All
 * platforms (plain C function pointer). */
extern u32 (*OrreHook_ItemLabel)(u16 item, void* pokemon, s32 mode);
/* menuPokemon.c, fn_80019754 (the Cologne Case's scent list, menu 0x5F), before it opens: the scents the player owns
 * (ids, count of them; the list's CANCEL after them is not passed). The hook may reorder them and returns the row the
 * cursor starts on, or -1 for no start (the game's own: the first). NULL: the game's order and start. */
extern s32 (*OrreHook_ScentListOpen)(u16* ids, s32 count);
/* menuPokemon.c, bagUseItemOnPokemon's Cologne Case branch, after a scent was used on a Pokémon (queued for the field,
 * one taken from the bag): the scent and the party slot. Observation only. NULL: nothing. */
extern void (*OrreHook_ScentUsed)(u16 item, s32 slot);
/* gs_pokemon_summary.c, fn_80017CB8, after it built a bag item's option list (USE / GIVE / TOSS / CANCEL, menu 0x5A)
 * from its pocket's data: the list (out, 0x0C-byte records: label key, callback address, flags; room for max), how many
 * it holds, the page (pocket), the mode (0 field, 1 battle) and the item. The hook may rename, replace, remove or add
 * records and returns the new count (at most max). NULL: the pocket's own list, as on the GameCube. */
extern s32 (*OrreHook_BagOptions)(void* out, s32 count, s32 max, s32 page, s32 mode, u16 item);

/* ---- Step 3.7's Relic Forest shortcuts (2026-11-20, the project's build helper; port/src/pc_relic_forest.cpp sets these;
 * decided in docs/port/tweaks-37-research.md sections 2-3 and snatcherLeo's answers 5-6; how the stone and the flute work:
 * docs/engine/shadow-system.md "Purification").
 * Why hooks: PURIFY in the party menu needs an extra entry in the party's per-Pokémon menu (0x6B), a way out of the
 * party screen and the pause menu, and the stone's own ceremony started for the chosen Pokémon without asking again;
 * the Time Flute anywhere in the forest needs one more entry on the pause menu's "usable now" list. Each place gets one
 * small hook whose NULL default is exactly the GameCube's; the rules live in port/. None draws random numbers (the
 * menus, the stone's checks and the flute's draw none: verified by search, research sections 2-3). All platforms
 * (plain C function pointers). ---- */

/* gs_range_8000D290.c, heroMoveRunEvent (the field pause menu), at each step of its life: step 0 as it opens (right
 * after the kind-4 examine events ran, the moment the room script's function 11 adds the Time Flute when Wes faces the
 * Relic Stone); step 1 just before its POKéMON entry opens the party screen; step 2 when that party screen has closed:
 * return nonzero with *eventId / *value set to leave the pause menu and queue that script event for the hero step
 * (heroMoveAddAutoEvent, as the ITEMS entry queues an item's event); step 3 as the pause menu closes. Steps 1 and 2
 * come only from the story's pause menu (0x41), never from a Battle Mode run's (0x109). eventId / value are NULL at
 * steps 0, 1 and 3, where the return value is ignored. NULL: the pause menu as on the GameCube. */
extern s32 (*OrreHook_PauseMenuStep)(s32 step, u32* eventId, u32* value);
/* menuPokemon.c, fn_8001C7B8 (the party list's per-Pokémon menu, from the pause menu's party screen), just before the
 * menu opens: its id (0x6B, or 0x10F during a Mt. Battle run) and the party slot. OrrePort links its extra entries
 * into the menu's item chain here, or puts the game's own back. NULL: the menu as on the GameCube. */
extern void (*OrreHook_PartySubmenuOpen)(s32 menuId, s32 slot);
/* menuPokemon.c, fn_8001C7B8, for a chosen entry the game's switch doesn't know (it has no default case: the menu would
 * simply open again): the entry's item id and the party slot. Return nonzero to close the party screen (the function
 * then returns -1, which ends menuPokemonSub's loop as B does), 0 to open the menu again as the game does. NULL: as
 * on the GameCube. */
extern s32 (*OrreHook_PartySubmenuChoice)(s32 itemId, s32 slot);
/* pokemon_relive.c, preReliveMain, on the Relic Stone's path (action 4) just before "Which POKéMON..." (0x3B0F) and
 * the party picker (menuPokemonOpen kind 7): a party slot to use instead of asking, or -1 to ask as the game does.
 * The slot must hold a Shadow Pokémon whose heart is open (the picker's own test); the hook checks that. NULL: the
 * message and the picker, as on the GameCube. */
extern s32 (*OrreHook_RelivePickSlot)(void);

/* ---- Step 3.7's right-stick walking (2026-11-20, the project's build helper; port/src/pc_cstick_walk.cpp sets it; decided in
 * docs/port/tweaks-37-research.md section 4 and snatcherLeo's answer 7; how walking reads the stick:
 * docs/engine/player-movement.md). Why a hook in the walking code, not in the pad layer: merging the sticks in the
 * controller state would also move menu cursors, double the battle menu's direct pick and hide which stick a recording
 * used; here only the two readers that turn the stick into Wes's steps see it. No random numbers (verified by search,
 * research section 4). All platforms (plain C function pointer). ---- */

/* hero_move.c, heroReadStick, and people.c, fn_80186B5C (the step prediction script command std170 checks against a
 * line), right after each reads the main stick: its smoothed x / y (speed and direction) and its unsmoothed x / y
 * (whether the facing changes), all four in the game's clamped units (radius 56), before the d-pad is tried. The hook
 * may replace all four (OrrePort: the C-stick's values while the main stick is centred). NULL: the main stick, then the
 * d-pad, as on the GameCube. */
extern void (*OrreHook_WalkStick)(s8* x, s8* y, s8* rawX, s8* rawY);
/* input/input.c, GSinputRead, right after the frame's pad state is latched and before the game smooths and clamps its
 * sticks: id is the game's pad id (1 = the player's controller), the main stick by pointer (in / out), the C-stick as
 * read. Why (2026-11-21, snatcherLeo: "We ensure right stick copies left one exactly, except, in battle mode ... for PDA menu,
 * we could turn this off. Everything else, we copy over"): one place where a value put in the main stick reaches every
 * reader of it (walking and every menu), as a main-stick push would. NULL: the sticks as read, as on the GameCube.
 * *Changed 2026-11-21 (snatcherLeo: "in a battle, (in story mode) right stick should work just like left stick"):* the C-stick
 * by pointer too (in / out), so the hook can also centre it where the copy replaces the game's own C-stick use (story
 * battles: the battle menus' C-stick direct pick would otherwise act on the same push as the copied cursor move).
 * All platforms (plain C function pointer). */
extern void (*OrreHook_PadSticks)(s32 id, s8* stickX, s8* stickY, s8* substickX, s8* substickY);
/* main.c, the main loop, right after each GSgappUpdate pass: the frame's update tasks have all run and no task list walk
 * is in progress, so a task may be run or ended here safely. Why (2026-11-21, the quicker way back from the PC screen):
 * the field room's archive, restored after the PC screen, waits about two frames per entry in the loader's job chain;
 * OrrePort runs those jobs here sooner while that PC screen is being left (pc_field_menu.cpp, pc_game_ui.fsys_hurry).
 * NULL: nothing, as on the GameCube. All platforms (plain C function pointer). */
extern void (*OrreHook_AfterGappUpdate)(void);
#define ORRE_WALK_STICK(x, y, rawX, rawY)                                                                              \
    do {                                                                                                               \
        if (OrreHook_WalkStick != NULL) OrreHook_WalkStick((x), (y), (rawX), (rawY));                                 \
    } while (0)

/* ---- Step 3.7's Smooth Party UI (2026-11-20, the project's build helper; port/src/pc_party_ui.cpp sets these; decided in
 * docs/port/tweaks-37-research.md section 6b and snatcherLeo's answer 8; how the portrait cache works: docs/engine/menus.md
 * "Party portraits"). Why hooks: the party screen asks for its six portraits as one chain and opens at once, so the
 * missing ones pop in one by one as their disc reads finish. Making them appear together needs two small places: the
 * moment after the screen asked (to let the loads finish before its first frame) and the portrait draw (to hold a
 * portrait until all of the screen's are ready). The portrait code draws no random numbers (verified by search,
 * research 6b). All platforms (plain C function pointers). ---- */

/* menuPokemon.c, menuPokemonSub, right after it asked for the party's portraits (menuFaceRequestPokemonFace with the
 * party chain) and before the party list opens: called with waited = 0, 1, 2, ... once per frame for as long as it
 * returns nonzero (the game thread switches a frame between calls); fromBattle is the list's Pokémon source (the
 * work block's +0x08: 0 the field party, 1 a battle's). NULL or 0: the list opens at once, as on the GameCube.
 * *Changed 2026-11-21:* OrrePort always answers 0 now (snatcherLeo: "it shouldn't pause at all"); the call only tells it the
 * screen opened (pc_party_ui.cpp arms its stand-ins, OrreHook_FaceStandIn below). */
extern s32 (*OrreHook_PartyPortraitWait)(s32 fromBattle, s32 waited);
/* gs_range_80109C88.c, faceDrawPokemon (menuFaceDrawPokemon) and faceDraw (fn_8010B9E8), with the portrait's key, when
 * its cache slot is ready and it would be drawn: nonzero skips this frame's draw (the portrait isn't drawn, exactly as
 * when it isn't loaded yet). Look only. NULL: drawn as soon as it is ready, as on the GameCube. */
extern s32 (*OrreHook_FaceDrawHold)(u16 key);
#define ORRE_FACE_DRAW_HOLD(key) (OrreHook_FaceDrawHold != NULL && OrreHook_FaceDrawHold((u16)(key)) != 0)
/* *Added 2026-11-21 (snatcherLeo on the list's wait: "it shouldn't pause at all"; docs/port/tweaks-37-research.md 6d):*
 * gs_range_80109C88.c, faceDraw (fn_8010B9E8: the party list's portraits, by key) and faceDrawPokemon
 * (menuFaceDrawPokemon), when the portrait's cache slot is NOT ready yet and the
 * game would skip drawing it: the hook may return a stand-in texture for this frame's draw (a copy of the same portrait
 * that OrrePort read from the disc itself, already run through GStextureLoad, below 4 GB, kept alive for the run), and
 * the portrait is drawn from it exactly as the game would draw the loaded one. Look only: the game's portrait cache,
 * its loads, their order and timing are untouched (the stand-in never enters the cache or GSres), so it can't change
 * any roll. resourceId is the portrait's texture id (its poke_face.fsys entry id, _menuFaceBiosGetPtr). NULL hook or a
 * NULL answer: not drawn, as on the GameCube. All platforms (plain C function pointer). */
extern void* (*OrreHook_FaceStandIn)(u16 key, u32 resourceId);

/* the helpers the game code uses at the call sites, so each site is one short line */
#define ORRE_ITEM_USE_KEEPS(item) (OrreHook_ItemUseKeeps != NULL && OrreHook_ItemUseKeeps((u16)(item)) != 0)
#define ORRE_CONTINUE_INITIAL(initial) \
    (OrreHook_ContinueInitialAnswer != NULL ? OrreHook_ContinueInitialAnswer(initial) : (initial))
#define ORRE_CONTINUE_QUESTION(key) \
    (OrreHook_ContinueQuestion != NULL ? OrreHook_ContinueQuestion(key) : (key))
#define ORRE_NEW_GAME_INITIAL(initial, hasStory) \
    (OrreHook_NewGameInitialAnswer != NULL ? OrreHook_NewGameInitialAnswer((initial), (hasStory)) : (initial))
#define ORRE_TM_SKIP_ASK(item) (OrreHook_TmSkipAsk != NULL && OrreHook_TmSkipAsk((u16)(item)) != 0)
#define ORRE_TM_USED_COUNT(item, count) \
    (OrreHook_TmUsedCount != NULL ? OrreHook_TmUsedCount((u16)(item), (count)) : (count))
#define ORRE_TM_LABEL(key, pokemon, item) \
    (OrreHook_TmLabel != NULL ? OrreHook_TmLabel((key), (pokemon), (u16)(item)) : (key))
#define ORRE_ITEM_LABEL(item, pokemon, mode) \
    (OrreHook_ItemLabel != NULL ? OrreHook_ItemLabel((u16)(item), (pokemon), (s32)(mode)) : 0u)

#endif /* TARGET_PC */

#endif
