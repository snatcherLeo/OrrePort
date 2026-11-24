/*
 * pc_game_ui.h: the game functions and data OrrePort's own parts of the game's screens use (2026-11-13; first
 * user: the Options screen's Language row, pc_options_lang.cpp).
 *
 * Why a table of pointers: port/ libraries are also linked into OrrePort's small test programs (orreport.exe,
 * pc_thread_test.exe), which have no game code, so port/ code can't call game functions by name. The game side
 * (src/game/pc_game_ui.c, compiled with the game by port/tools/firstlink.py) fills this table at start-up, the same
 * way pc_debug_game.c fills the debug toolkit's. Every pointer is NULL in the test programs; users check `ready`.
 *
 * Platforms: all the same (plain C).
 */
#ifndef PC_GAME_UI_H
#define PC_GAME_UI_H

#include <stddef.h> /* size_t (place_text) */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the name-entry block as OrrePort's naming keyboard sees it (filled by name_entry_view; 2026-11-14). The name is
 * the game's text: big-endian UTF-16, ended by 0. letter = the cursor's column (0xF = the button column), column =
 * its line (0-3): the game's own field names, which read the other way round. row = the game's page (0 / 1: it tints
 * the panel and picks the X label). state = the name's length. */
struct PcNameEntryView {
    uint16_t* name;
    int32_t kind; /* 0 player, 1 partner, 2 nickname, 3 box */
    int32_t* row;
    int32_t* letter;
    int32_t* column;
    int32_t* state;
    /* the name the game falls back to when the typed one is empty or all blanks (inputName's default: the first preset
     * in modes 0 and 1, the current nickname or box name in 2 and 3); the IME work, 2026-11-15: a name whose letters
     * all have no GameCube form leaves the game's buffer empty, so the game itself picks this one */
    uint16_t* fallback;
};

/* one piece of a message's own text as the game measures it (dialogue re-wrapping; 2026-11-14;
 * pc_msg_rewrap.cpp): where it starts in the text (bytes from the message's start), what it is, and the pen's x once
 * it is laid out (for an inserted name or message, the x after the whole insert, measured inside it as the game does) */
enum PcMsgUnitKind {
    PC_MSG_LETTER = 0,  /* a character other than a space */
    PC_MSG_SPACE = 1,   /* a space (0x0020): where a line may be broken */
    PC_MSG_CR = 2,      /* control 0x00 / 0x01, a line break (3 bytes) */
    PC_MSG_PAGE = 3,    /* control 0x03 (msgctrlKeyWait): wait for A, then a fresh box (3 bytes) */
    PC_MSG_WAIT = 4,    /* control 0x02 (msgctrlKeyEnd): wait for A at the message's end */
    PC_MSG_CONTROL = 5, /* any other control code with its argument bytes (inserted names, colours, sounds) */
    PC_MSG_END = 6      /* the 0x0000 that ends the text */
};
struct PcMsgUnit {
    uint32_t offset;
    uint32_t kind; /* enum PcMsgUnitKind */
    float x_after;
};

struct PcGameUi {
    int ready; /* 1 once the game side filled the table */
    /* the menu tables (menu_bios_range_8005D7F8.c): an item's record (0x1C bytes, big-endian), its x / y, a sprite */
    void* (*menu_item)(int32_t index);
    void (*menu_item_set_xy)(int32_t index, int16_t x, int16_t y);
    void* (*menu_sprite)(uint32_t index);
    /* textures (gs_texture.c) and the resource registry (gs_res.c) */
    void* (*texture_create)(int32_t width, int32_t height, int32_t format);
    void* (*texture_lock)(void* texture);
    void (*texture_unlock)(void* texture);
    void* (*resource_find)(uint32_t id);
    void (*resource_register)(void* data, uint32_t group, uint32_t id);
    /* a menu sprite drawn at a place, as menu callbacks do (window_exact_801040F0.c) */
    void (*draw_sprite)(int16_t x, int16_t y, void* window, uint32_t sprite);
    /* the Options screen's state record (lbl_803A1FC8: the selection first, the pulse phase at +0x24) and its help
     * keys, one per row number (lbl_802E4EF0, native u32 on the PC) */
    uint8_t* options_state;
    uint32_t* options_help;
    /* the group header of the loaded bank holding a message key (gs_msg.c), or NULL */
    void* (*msg_group_of)(uint32_t key);
    /* for OrrePort's own menus (the main menu's SAVE FILES list, 2026-11-14): open a menu and wait for its choice
     * (menuOpenCustom with a cursor check; the choice, or -1 for B), the active window, the keys this frame (the
     * window system's key record, menu.c's MenuKeyInfo: +0 down now, +2 last frame, +4 newly pressed, +6 repeat (the
     * press, then every 5 frames after 15: what cursors read), +8 held past the first repeat), the standard A / B handling of a menu's
     * button callback (menuButtonNormal) and its address for a menu record, a sound (soundStart), text in a menu piece
     * (GSmsgPrintColor at piece-relative x / y, GSmsgGetRect's width in the high half), and the clip around a piece
     * (fn_800FE38C / fn_800FE35C, as the game's own text callbacks do) */
    int32_t (*menu_open_wait)(uint32_t menuId, uint32_t parentId);
    void (*menu_close)(uint32_t menuId); /* menuCloseCustom(id, 0, 1): the game opens and closes its menus separately */
    uint32_t (*window_active_id)(void);
    const uint8_t* (*window_keys)(void);
    void (*menu_button_normal)(void* window);
    uint32_t menu_button_normal_addr;
    void (*sound)(uint32_t id);
    int32_t (*msg_print)(int32_t x, int32_t y, int32_t color, uint32_t key);
    int32_t (*msg_rect)(uint32_t key);
    void (*clip_set)(int32_t x, int32_t y, int32_t w, int32_t h);
    void (*clip_reset)(void);
    /* for the SAVE FILES list's help box (2026-11-14): the game's own place name for a room (its floor record's
     * place-name message, as plain UTF-8, "" when none), and the Continue screen's snagged / purified counts tested
     * against another save's story-flag words (class 3, native order) with the game's own Shadow table and flag
     * positions; src/game/pc_game_ui.c says how */
    void (*place_text)(uint32_t floorId, char* out, size_t cap);
    void (*shadow_counts)(const uint32_t* storyFlags, uint32_t words, uint32_t* snagged, uint32_t* purified);
    /* a window's record by its id (windowSearchID; NULL when none): OrrePort reads and sets its colour at +0x88 (red,
     * green, blue, alpha), which the game's sprite drawing multiplies into every piece the window draws (win_sprite.c)
     * (2026-11-14: the main menu dimmed behind the SAVE FILES list) */
    uint8_t* (*window_find)(uint32_t windowId);
    /* the title's own card load (memory-card task 1, an empty save when none; src/game/pc_game_ui.c): -1 aborted,
     * 1 a save loaded, 0 empty. Only from a game task that may wait (the main menu's hooks) */
    int32_t (*card_load_title)(void);
    /* for the naming keyboard (2026-11-14; pc_name_keyboard.cpp): big-endian UTF-16 text (ended by 0) printed and
     * measured with the name screen's own one-letter template (message 0xCE: its font, size and fade), at x / y
     * relative to the draw item, as the game's grid prints; the screen's maximum length for a mode (0 player, 1
     * partner, 2 nickname, 3 box: the per-mode table, 7 / 7 / 10 / 8 on the US disc); a game message's raw text */
    int32_t (*print_gs)(int32_t x, int32_t y, uint32_t color, const void* text);
    int32_t (*measure_gs)(const void* text); /* the template's rect: width in the high 16 bits, height in the low */
    int32_t (*name_max)(int32_t kind);
    const void* (*msg_gschar)(uint32_t key);
    /* the keyboard window's name-entry block with real pointers (game/name_entry_arg.h), and the screen's sounds */
    void (*name_entry_view)(void* window, struct PcNameEntryView* view);
    void (*sound_se)(uint32_t id);
    /* an open window's live pieces of one width x height: their draw alpha set (0 hides, 0xFF shows); how many
     * matched (src/game/pc_game_ui.c says why the live pieces) */
    int32_t (*window_piece_alpha)(void* window, int16_t width, int16_t height, uint8_t alpha);
    /* for dialogue re-wrapping (2026-11-14; pc_msg_rewrap.cpp, src/game/gs_msg.c says how): a running message's
     * task record by its key (GSmsgFindCheck; NULL when none: 0x68 bytes, text pointers in 32-bit slots at +0x28
     * start, +0x2C page start, +0x30 cursor), and its top-level text measured piece by piece (the count, or -1 when
     * more than cap pieces) */
    uint8_t* (*msg_task)(uint32_t key);
    int32_t (*msg_measure)(const uint8_t* task, struct PcMsgUnit* out, int32_t cap);
    /* the naming keyboard's 12-slot field (stage 3; src/game/pc_game_ui.c says why): a menu sprite drawn at a size and
     * colour (0xRRGGBBAA, relative to the draw item, as draw_sprite), the green next-slot bar's pulse (0-255, the
     * game's own formula), and whether big-endian UTF-16 text equals a species name */
    void (*draw_sprite_sized)(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t rgba, void* window, uint32_t sprite);
    uint8_t (*name_pulse)(void* window);
    int32_t (*is_species_name)(const uint16_t* text);
    /* the full names beside the save's GameCube names (2.4; src/game/pc_game_ui.c says how): the stored player's and
     * Rui's names, a PC box's name (0-2), and every Pokémon record in the save (party, boxes, daycare; the count) */
    uint16_t* (*hero_name)(void);
    uint16_t* (*partner_name)(void);
    uint16_t* (*box_name)(int32_t box);
    int32_t (*pokemon_records)(void** out, int32_t max);
    const uint16_t* (*species_name)(const void* record); /* a record's species name (the game's text), or NULL */
    /* un-nicknamed Pokémon in the chosen language (2.2 step 8, 2026-11-15; pc_names.cpp): the message key of
     * species id's name (0 for an empty id; ids 1-0x19B) and its text as GSmsgGetGSchar gives it (the disc's) */
    uint32_t (*species_key)(uint16_t id);
    const uint16_t* (*msg_string)(uint32_t key);
    /* the word pictures per language (language piece 4, 2026-11-15; pc_word_pictures.cpp): how many records the
     * sprite table has (menu_sprite above always gives the game's own US record for those, never a language's), and a
     * texture file read from a language disc made ready to draw as the game does after loading one (GStextureLoad:
     * the file's header made native, its picture offsets turned into pointers, its GX objects built; the file must
     * sit below 4 GB, 32-byte aligned). Returns the texture, or NULL */
    uint32_t (*menu_sprite_count)(void);
    void* (*texture_load)(void* file);
    /* how many records the menu item table has (menu_item above; 2026-11-15): the word pictures keep a copy of the
     * table from the first menu sprite lookup, before OrrePort's screens move items, to compare with the language
     * disc's items (0 until the game's tables are loaded) */
    uint32_t (*menu_item_count)(void);
    /* for the naming keyboard's input-method parts (the project's IME work, 2026-11-15; src/game/pc_game_ui.c says how):
     * whether game font fontId has its own glyph for a character (1 yes, 0 no, -1 the font isn't open; never the font
     * renderer's drawn stand-ins), a box with a top and a bottom colour and a 1-pixel line drawn with the game's own 2D
     * drawing (0xRRGGBBAA, relative to the current draw item, as the game's window callbacks draw), and the "O.K.?"
     * box's own slate colours (0 top, 1 bottom) */
    int32_t (*font_has)(uint16_t fontId, uint16_t code);
    void (*fill_rect)(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t top, uint32_t bottom);
    void (*draw_hline)(int16_t x, int16_t y, int16_t w, uint32_t rgba);
    uint32_t (*slate_color)(int32_t which);
    /* Main Menu on the PC screen (2026-11-15; pc_pc_screen.cpp, src/game/pc_game_ui.c says how): the game's YES /
     * NO box (menuSubOpenYesNo(0, x, y, initial): 0 YES, 1 NO, anything else B) and its message window (winMsgOpen(2,
     * key, 1, 1) / winMsgClose(1), as the title's New Game question pairs them), both only from a game task that may
     * wait; the PC screen's own tables (PC data copies, native floats: the buttons' and labels' slide rows, 4 x 6 floats
     * each {pos, pull, speed, resting x, start offset, 0}; the lit piece's places, 4 x {x, y}; the wheel's targets and
     * the icons' base angles, 4 each), the live slide rows (4 x 6 floats, copied from the button table at each open),
     * the wheel's current angle; the main menu's glove put on its default (Continue when a save exists), the save image
     * (savedataGetStatus(0, 0), its size), and the title's own Quit path (back to the title screen) */
    int32_t (*yes_no)(int32_t initial, int16_t x, int16_t y);
    void (*msg_window_open)(uint32_t key);
    void (*msg_window_close)(void);
    float* pc_slide_buttons;
    float* pc_slide_labels;
    float* pc_lit_places;
    float* pc_wheel_targets;
    float* pc_wheel_bases;
    float* pc_slide_live;
    float* pc_wheel_angle;
    float* pc_wheel_target; /* the turn's target (lbl_8047A488; the list's wrap re-centres it at once) */
    void (*top_menu_glove_default)(void);
    const uint8_t* (*save_image)(uint32_t* size);
    void (*title_restart)(void);
    /* *Added 2026-11-16 (the project's helper, the PC MENU spot check):* where Wes stands now, for test logs: the room, his
     * position (x, y, z) and facing in degrees, read the way OrrePort's PC MENU spot is (heroMoveGetHeroPos / Rot).
     * Returns 0 outside a field room (kind 1), where there is no hero model to read. Read only */
    int32_t (*hero_spot)(uint32_t* room, float* xyz, int32_t* facingDegrees);
    /* *Added 2026-11-16 (a language change switches everything at once):* forget the archive loader's ARAM copy of one
     * archive (by its id, the archive header's +0x08), so its next load reads the disc file (OrrePort's language
     * overlay) again. 1 forgotten, 0 skipped (that archive is loading right now). Game thread only */
    int32_t (*fsys_cache_forget)(uint32_t archiveId);
    /* *Added 2026-11-16 (test logs: the PC MENU spot check):* the field camera's state (field_camera.c): returns
     * enabled | point count << 8 | "blended once" << 16, and hdy = its smoothed height, distance and yaw. Read only */
    int32_t (*field_camera_state)(float hdy[3]);
    /* *Added 2026-11-16 (the project's build helper, the battle nickname fix; pc_names.cpp):* the Pokémon records (0x138-byte
     * PokemonBios, the battle's own copy of each party record) of every Pokémon on the battle field now, both sides;
     * 0 outside a battle (fightIsBattleActive). Read only. src/game/pc_game_ui.c says how */
    int32_t (*battle_field_records)(void** out, int32_t max);
    /* *Added 2026-11-20 (the PC MENU softlock fix; pc_save_windows.cpp, src/game/pc_game_ui.c says how):* a story flag
     * read and written with the game's own functions (std082_flagGetValue / _flagSet, any class), Wes's "a scene holds
     * him" people flag (0x80000008, set by heroMoveInitEvent), what is on screen for the player to answer (bit 0: a
     * message window, winMsgCheck != -1; bit 1: a window reading buttons, the window system's active id != 0), and a
     * new game task on the current room's context (GSthreadCreate, as the debug battle start; it may wait, and a room
     * change ends it with the room's other threads): 1 started, 0 not */
    uint32_t (*flag_get)(int32_t id);
    void (*flag_set)(int32_t id, uint32_t value);
    int32_t (*hero_event_locked)(void);
    int32_t (*player_prompt_up)(void);
    int32_t (*start_task)(void (*fn)(void));
    /* *Added 2026-11-20 (autosave; pc_autosave.cpp, src/game/pc_game_ui.c says how):* the save image copied into out
     * (0x1DFD0 bytes, below 4 GB) as the PC's Save would write it now (Rui's flag, the last PC's return point, the save
     * count + 1), the game's own image untouched (returns the size, 0 when none); where the player stands as PC MENU
     * records it (ids: room, room before, entry; 0 when not a resumable field spot); a battle running */
    uint32_t (*save_capture)(uint8_t* out);
    int32_t (*field_spot)(uint32_t ids[3], float xyz[3], int32_t* facingDegrees);
    int32_t (*battle_active)(void);
    /* *Added 2026-11-20 (the save-check sweep, port/src/pc_savecheck.cpp; src/game/pc_game_ui.c says how):* every
     * people entry now, one text line each ("slot index group shown state sub x y z", position empty without a model),
     * into out (cap bytes, never cut mid-line); returns the number of lines. Read only; test logs only */
    int32_t (*people_rows)(char* out, int32_t cap);
    /* *Added 2026-11-20 (the Battle Mode guard: pc_autosave.cpp, pc_field_menu.cpp; src/game/pc_game_ui.c says how and
     * why):* whether Battle Mode is running, from the game's own state, read only: 0 no; bit 0 the top menu's last
     * choice was one of its two Battle Mode entries (it stays so until the top menu room is set up again, where Battle
     * Mode ends); bit 1 the game mode variable 0x8AE (FLAG_system_gamemode) is set: a Colosseum or Mt. Battle run. And
     * where Battle Mode's record, save section 14, lies in the save image: its offset from the image's start into
     * *offset; returns its size (0 when there is no image) */
    int32_t (*battle_mode)(void);
    uint32_t (*battle_record_span)(uint32_t* offset);
    /* *Added 2026-11-20 (step 3.7's item tweaks: port/src/pc_bag_tweaks.cpp; src/game/pc_game_ui.c says how):* the bag
     * and the item data, read with the game's own functions, read only.
     * - bag_row_of: the row of a bag page (pocket) that holds an item, counted the way the bag's own actions count rows
     *   (bagItemUse's walk: only the pocket's valid entries), or -1 when the pocket holds none; *total gets how many of
     *   it the pocket holds over all its rows (NULL allowed). bag_item_at: the item on a page's row, 0 when none.
     * - the bag's three actions (the callbacks in the pockets' option lists, as 32-bit addresses: the exe's image is
     *   below 4 GB), and bag_option_label: the label key the first pocket option list with that callback uses, 0 none.
     * - item data (the item table record): its field and battle use functions (32-bit addresses, 0 none), and the
     *   addresses of the game's message-only use functions ("not now", "can't be used", "meaningless") and the Ein File
     *   reader; its kind (6 = a scent), TM number and HM number (0xFF none), price and description key.
     * - pokemon_knows_move: whether a party Pokémon's record knows the move a TM / HM teaches (its four moves).
     * - bag_message: a message in the bag's message window, waited for (winMsgOpen(2, key, 1, 0) / winMsgClose(1), as
     *   the bag's own messages); only from the game task that runs the bag (a bag option's callback). */
    int32_t (*bag_row_of)(int32_t page, uint16_t item, uint32_t* total);
    uint16_t (*bag_item_at)(int32_t page, int32_t row);
    uint32_t bag_use_addr, bag_give_addr, bag_toss_addr;
    uint32_t (*bag_option_label)(uint32_t callbackAddr);
    uint32_t (*item_field_use)(uint16_t item);
    uint32_t (*item_battle_use)(uint16_t item);
    uint32_t item_use_not_now_addr, item_use_forbidden_addr, item_use_meaningless_addr, item_use_reader_addr;
    uint8_t (*item_kind)(uint16_t item);
    uint32_t (*item_tm_number)(uint16_t item);
    uint32_t (*item_hm_number)(uint16_t item);
    uint32_t (*item_price)(uint16_t item);
    uint32_t (*item_doc)(uint16_t item);
    int32_t (*pokemon_knows_move)(void* pokemon, uint16_t item);
    void (*bag_message)(uint32_t key);
    /* *Added 2026-11-20 (step 3.7's Relic Forest shortcuts: port/src/pc_relic_forest.cpp; src/game/pc_game_ui.c says
     * how):* the room the game is in now (std092_getCurrentRoomID: inside a menu room such as the party screen's 0x38F
     * that room, not the field under it); one entry added to the pause menu's "usable now" list (std017_menuQueueAdd,
     * the script command the stone's function 11 runs: an item and the script event its use queues); and a party
     * slot's heart state as the Relic Stone's picker tests it: -1 no Pokémon there, 0 not a Shadow Pokémon, 1 a Shadow
     * Pokémon whose heart isn't open yet (gauge above 0), 2 a Shadow Pokémon whose heart is open (gauge exactly 0).
     * Read only except menu_queue_add */
    uint32_t (*current_room)(void);
    void (*menu_queue_add)(uint16_t item, uint32_t eventId);
    int32_t (*party_heart)(int32_t slot);
    /* *Added 2026-11-20 (step 3.7's right-stick walking: port/src/pc_cstick_walk.cpp):* controller 1's C-stick as the
     * game reads it (GSinputGetRightStickX/YData: smoothed when filtered is 1, else unsmoothed; both clamped by the
     * game to its radius 44, Y up positive as the game flips it), and the scene's camera mode (GSscene_GetMode: 5 the
     * debug orbit camera, which steers with the C-stick while Wes walks; 6 the debug free camera). Read only */
    void (*right_stick)(int32_t filtered, int8_t* x, int8_t* y);
    uint32_t (*scene_mode)(void);
    /* *Added 2026-11-20 (step 3.7's Smooth Party UI: port/src/pc_party_ui.cpp; src/game/pc_game_ui.c says how):* the
     * portrait key of the field party's slot (pokemonGetFaceSpriteId: 0 for an empty slot), whether a portrait is in
     * the cache and ready to draw (menuFaceIsReady == 1), and whether any portrait is loading now (fn_8010B560). Read
     * only */
    uint16_t (*party_face_key)(int32_t slot);
    int32_t (*face_ready)(uint16_t key);
    int32_t (*face_loading)(void);
    /* *Added 2026-11-21 (the Smooth Party UI's stand-ins, docs/port/tweaks-37-research.md 6d):* a portrait key's texture
     * id, which is also its poke_face.fsys entry id (_menuFaceBiosGetPtr's record; 0 for a key out of range). Read only */
    uint32_t (*face_resource_id)(uint16_t key);
    /* *Added 2026-11-21 (the pose rows: port/src/pc_pose_rows.cpp, port/ui/game/pose_rows.ini):* a room person put into a
     * pose the way the room scripts do it: its animation set (std051_characterSetAnimation(group, index, anim, 0,
     * loop)), then its place (std059_characterSetPosition, the game's own peoplePlaceAt, so the model moves too), then,
     * when hasFacing, its facing at once (peopleSetRotationDeg(group, index, 0, facing, 0)). 1 when the person was
     * found, 0 when no such person is in the room (nothing changed). Game thread */
    int32_t (*person_pose)(uint32_t group, uint32_t index, int32_t anim, int32_t loop, float x, float y, float z,
                           int32_t hasFacing, float facingDegrees);
    /* *Added 2026-11-21 (the pose rows' "look" field):* a room person's head, with the room scripts' own commands:
     * lookPlayer 0 = stop looking at anything (std067_characterStopLookAt: the head part reset, no look target; what
     * Mt. Battle's func73 does to Vander before he is knocked down), 1 = follow the player with the head
     * (std065_characterLookAtPlayer, what a placement's look-at bit does on room entry). 2 when the person was found and
     * had a look target before the call, 1 when found without one, 0 when not found (nothing changed). Game thread */
    int32_t (*person_look)(uint32_t group, uint32_t index, int32_t lookPlayer);
    /* *Added 2026-11-21 (the quicker way back from the PC screen, pc_field_menu.cpp):* run the archive loader's pending
     * jobs (LZSS decodes, group callbacks) and its frame step now, in their own order, for at most budgetUs
     * microseconds; returns the jobs run. Only from OrreHook_AfterGappUpdate (no task list walk in progress). Game
     * thread */
    int32_t (*fsys_hurry)(int32_t budgetUs);
    /* *Added 2026-11-21 (the right stick copying the left: port/src/pc_cstick_walk.cpp):* 1 while the P★DA's Pokémon
     * model page is open (the C-stick spins the model there). Read only. *Changed 2026-11-21:* non-zero (the P★DA's state
     * itself, for the log) on every page of the P★DA's Pokémon section and the fades between them (states 2, 4, 5, 6,
     * 8, 9, 0xA, 0xD), where a copied push would turn the page (the 3.7 closing sweep's P02 / P07 gap); name kept */
    int32_t (*pda_model_page)(void);
};
extern struct PcGameUi pc_game_ui; /* defined in pc_options_lang.cpp, filled by src/game/pc_game_ui.c */

#ifdef __cplusplus
}
#endif

#endif
