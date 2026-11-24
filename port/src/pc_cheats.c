/*
 * pc_cheats.c: OrrePort's quick cheats (pc_cheats.h says why they work this way; snatcherLeo, 2026-11-13, mockup 63 v2).
 *
 * Each switch sets or clears one decision hook in the game code (include/pc/orre_hooks.h); the hook pointers are
 * defined here, in the port library that is always linked into the game, and stay NULL (the console's behaviour) while
 * the cheat is off. The game code never reads the switches itself (snatcherLeo's rule: hook points, not features, in game
 * code; docs/port/hook-points.md).
 *
 * Platforms: plain C (stdio, time, SDL's nothing); the same on Windows, Linux and macOS (likely; only Windows built).
 */
#include "pc_cheats.h"
#include "pc_options.h"
#include "pc_text.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

/* the game's own integer types and the hook declarations, so these definitions are checked against them */
#include "dolphin/types.h"
#include "pc/orre_hooks.h"

typedef s32 s32_t;
typedef u32 u32_t;

#include "pc_paths.h" /* the logs folder (4.1) */
const char* pc_pad_record_path(void); /* pc_pad_record.c: this sitting's recording ("" when none) */

/* ---- the hook pointers (include/pc/orre_hooks.h): NULL = the console's behaviour ---- */
s32 (*OrreHook_MoveDamage)(int attackerOnSide0, int targetOnSide0, s32 damage, s32 targetHp);
int (*OrreHook_CatchCertain)(void);
u32 (*OrreHook_MoneyTaken)(u32 amount);
int (*OrreHook_ItemUseKeeps)(u16 item);
u32 (*OrreHook_ExpGained)(u32 exp);
/* the font renderer's hook (pc_font_render.cpp sets it; defined here with the other hook pointers, orre_hooks.h) */
int (*OrreHook_GlyphFetch)(u16 fontId, u16 code, s16 width, s16 cellHeight, s16 baseline, s16 capHeight,
                           const u8** pixels, s16* height, s16* yOffset);
/* part D's hook (pc_font_render.cpp sets it): a glyph for a character the game's font lacks */
void* (*OrreHook_GlyphMissing)(u16 fontId, u16 code, s16 cellHeight, s16 baseline, s16 capHeight, void** outBank);
void (*OrreHook_GlyphCode)(u16 code);
/* the language switch's hook (pc_game_text.cpp sets it when the language isn't English) */
void* (*OrreHook_MessageText)(u32 key, const void* original);
int OrreHook_MessageForCopy;
/* dialogue re-wrapping (pc_msg_rewrap.cpp sets it) */
void (*OrreHook_MessageBox)(u32 key, s32 width, s32 height);
void* (*OrreHook_MessageInsert)(u32 control, void* text);
/* OrrePort's 12-slot name field and the full names (pc_name_keyboard.cpp, pc_names.cpp set these) */
s32 (*OrreHook_NameEntryPiece)(s32 piece, s32 type, void* window, u8* draw);
s32 (*OrreHook_NameEntryDrawName)(void* window, u8* draw, s32 type);
void (*OrreHook_NameEntryCommit)(s32 mode, s32 index, const void* stored, void* record);
/* B on the name menu (pc_name_keyboard.cpp's naming rules per kind; 2026-11-15): who may leave it, and the caller
 * that can take a back-out (the title's New Game sets it around its call; 0 elsewhere) */
s32 (*OrreHook_NameEntryListBack)(s32 mode, s32 index, u32 caller);
u32 OrreHook_NameEntryCaller;
s32 OrreHook_NameMenuCanLeave; /* set by menuNameEntry while its list may be left with B; 0: B ignored there */
void (*OrreHook_NameMenuLeft)(void); /* NULL: the game's own fades after B leaves a name menu */
/* the Options screen's Language row (pc_options_lang.cpp sets these; 2026-11-13) */
void* (*OrreHook_MessageExtra)(u32 key, void** outGroup);
void* (*OrreHook_MenuExtra)(int kind, u32 index);
void* (*OrreHook_MenuDataExtra)(u32 index); /* the SAVE FILES list's menu record (pc_title_loadsave.cpp) */
/* the word pictures per language (pc_word_pictures.cpp sets these; language piece 4, 2026-11-15) */
void* (*OrreHook_MenuSprite)(u32 index, const void* record);
void (*OrreHook_ItemSpriteBox)(u32 itemIndex, u32 spriteIndex, s16* x, s16* y, s16* width, s16* height);
void (*OrreHook_CursorSpriteAt)(const void* item, s16* x, s16* y);
void (*OrreHook_StadiumHeader)(u32 itemIndex, s32 stadium, s16* x, s16* y, s16* width, s16* height);
void* (*OrreHook_ResourceMissing)(u32 id);
/* the archive cache's room for language files (pc_lang_files.cpp sets these; 2026-11-19) */
u32 (*OrreHook_FsysCacheRoom)(u32 size, u32 fileHandle, u32 key1, u32 key2);
void (*OrreHook_FsysCacheReserved)(u32 aram, u32 room, u32 size);
void (*OrreHook_OptionsScreen)(int event);
int (*OrreHook_OptionsCancelKeeps)(void);
int (*OrreHook_OptionsKeys)(s32* selection, u16 keys);
/* the main menu's LOAD SAVE and the Continue question's YES-first tweak (pc_title_loadsave.cpp sets these; Main
 * 2026-11-14) */
int (*OrreHook_TopMenuChoice)(s32 choice, s32* nextState);
void (*OrreHook_TopMenuOpen)(void);
u32 (*OrreHook_TopMenuHelp)(s32 choice);
void (*OrreHook_TopMenuHelpDrawn)(s32 choice, u32 key, u32 alpha, void* window); /* the accurate badge (pc_title_loadsave.cpp; 2026-11-21) */
s32 (*OrreHook_ContinueInitialAnswer)(s32 initial);
u32 (*OrreHook_ContinueQuestion)(u32 key); /* LOAD's question after a pick: pc_title_loadsave.cpp (2026-11-16) */
s32 (*OrreHook_NewGameInitialAnswer)(s32 initial, s32 hasStory); /* pc_title_loadsave.cpp (2026-11-14) */
void (*OrreHook_CardTaskStart)(s32 task_kind); /* rule R4: pc_card.c */
void (*OrreHook_CardTaskEnd)(s32 task_kind, s32 result); /* 2.4, the extras file: pc_names.cpp */
s32 (*OrreHook_CardMessageSkip)(s32 error, s32 task_kind); /* the PC screen's Main Menu re-read: pc_pc_screen.cpp;
                                                           * a save picked in LOAD: pc_title_loadsave.cpp (chained) */
void (*OrreHook_MenuNameCopy)(void* copy, const void* record); /* 2.4, full names in menu copies: pc_names.cpp */
void (*OrreHook_MessageFit)(u32 key, u8* task);  /* 2.4, long names in fixed spots: pc_names.cpp */
s32 (*OrreHook_MessageFitRect)(u32 key, s32 rect);
u32 OrreHook_PrintPlace; /* the place of a generic print (the summary's OT name), for the fit; 0 elsewhere */
/* the field pause menu's PC MENU and Start-to-close (pc_field_menu.cpp sets these; 2026-11-15) */
u32 (*OrreHook_MenuBackKeys)(u32 menuId);
s32 (*OrreHook_MenuWrap)(u32 menuId);
void (*OrreHook_PcMenuExit)(void);
s32 (*OrreHook_PcMenuCloseTogether)(void);
s32 (*OrreHook_WindowCloseNow)(u32 windowId);
f32 (*OrreHook_FadeDuration)(u16 type, f32 duration);
void (*OrreHook_FieldMenuOpen)(s32 menuId);
s32 (*OrreHook_FieldMenuItem)(s32 selection);
s32 (*OrreHook_FieldMenuChoice)(s32 choice);
void (*OrreHook_FieldMenuSpot)(const OrreFieldSpot* spot);
s32 (*OrreHook_ContinueSpot)(OrreFieldSpot* spot);
/* Main Menu on the PC screen (pc_pc_screen.cpp sets these; 2026-11-15) */
void (*OrreHook_PcMenuOpen)(void);
s32 (*OrreHook_PcMenuChoice)(s32 choice);
s32 (*OrreHook_PcMenuCursor)(s8* row, u16 keys);
void (*OrreHook_PcMenuRowPlace)(s32 row, f32* x, f32* y);
void (*OrreHook_PcMenuWheel)(s32 choice, f32* target, f32* stepScale, f32* angleScale);
s32 (*OrreHook_FadeHold)(u16 type);
s32 (*OrreHook_RoomLeave)(u32 room, s32 depth, s32 settled, s32 pauseOpen);
void (*OrreHook_RoomEntryDone)(u32 room, s32 depth); /* the pose rows (pc_pose_rows.cpp; 2026-11-21) */
s32 (*OrreHook_FieldMenuReopen)(void);
s32 (*OrreHook_FieldMenuUnduckMusic)(void); /* the music held ducked on the way to the main menu (2026-11-16) */
/* PC MENU saves in a scene's open window, the softlock fix (pc_save_windows.cpp sets these; 2026-11-20) */
void (*OrreHook_PcSaveStep)(s32 step);
void (*OrreHook_HeroEventScript)(s32 running);
/* frame smoothing's camera blend (pc_smooth.c sets it while smoothing is on; 2026-11-16) */
void (*OrreHook_CameraSet)(const void* camera, s32 renderPass, const f32* view, const f32* interest);
/* frame smoothing's model blend, step (d) (pc_smooth.c sets it while smoothing is on; 2026-11-17) */
void (*OrreHook_ModelDraw)(const void* jobj, const void* pobj);
/* frame smoothing's particle blend, item (a) (pc_smooth.c sets it while smoothing is on; 2026-11-17) */
void (*OrreHook_ParticleDraw)(const void* particle, u32 guard, u32 life);
/* frame smoothing's 2D at 120, round 11 (pc_smooth.c sets it while smoothing and ORRE_SMOOTH_2D are on; 2026-11-17) */
void (*OrreHook_2DPiece)(u32 kind, u32 variant, u32 id, u32 valueBits);
/* widescreen groundwork's 2D inventory (pc_ui_inventory.c sets it with ORRE_UI_INVENTORY=1; 2026-11-19) */
void (*OrreHook_Ui2DDraw)(const OrreUi2DPiece* piece);
/* the move viewer as Studio's reference camera (src/game/pc_waza_viewer.c sets these with ORRE_WAZA_VIEWER; 2026-11-18) */
u32 (*OrreHook_FirstRoom)(u32 room);
void (*OrreHook_WazaViewerThread)(u8* work);
void (*OrreHook_WazaViewerDraw)(void* current);
/* the naming keyboard, version B (pc_name_keyboard.cpp sets these in the normal mode; 2026-11-14) */
s32 (*OrreHook_NameEntryInput)(void* window);
s32 (*OrreHook_NameEntryDrawGrid)(void* window);
s32 (*OrreHook_NameEntryCursorBox)(void* window, s32* x, s32* y, f32* width, f32* height);
s32 (*OrreHook_NameEntryDrawLabel)(s32 which, void* window, u8* draw);
s32 (*OrreHook_NameEntryPreview)(void* window, u16* letter);
/* step 3.7's item tweaks (pc_bag_tweaks.cpp sets these; 2026-11-20): the party list after a use or a give, the TM
 * design, infinite TMs, the remembered scent cursors and the bag's USE / READ / CHECK / TOSS rule */
s32 (*OrreHook_ItemUseAgain)(s32 kind, u16 item, s32 page, s32 slot, s32 result, u32* row);
s32 (*OrreHook_PartyListHold)(s32 step, s32 slot); /* the party list held open between uses (snatcherLeo's hand check, 2026-11-20) */
s32 (*OrreHook_PartyCursorStart)(s32 mode, u16 item);
s32 (*OrreHook_TmSkipAsk)(u16 item);
u32 (*OrreHook_TmUsedCount)(u16 item, u32 count);
u32 (*OrreHook_TmLabel)(u32 key, void* pokemon, u16 item);
s32 (*OrreHook_ScentListOpen)(u16* ids, s32 count);
void (*OrreHook_ScentUsed)(u16 item, s32 slot);
s32 (*OrreHook_BagOptions)(void* out, s32 count, s32 max, s32 page, s32 mode, u16 item);
/* step 3.7's Relic Forest shortcuts (pc_relic_forest.cpp sets these; 2026-11-20): PURIFY in the party menu, the Time
 * Flute anywhere in the forest */
s32 (*OrreHook_PauseMenuStep)(s32 step, u32* eventId, u32* value);
void (*OrreHook_PartySubmenuOpen)(s32 menuId, s32 slot);
s32 (*OrreHook_PartySubmenuChoice)(s32 itemId, s32 slot);
s32 (*OrreHook_RelivePickSlot)(void);
/* step 3.7's right-stick walking (pc_cstick_walk.cpp sets it; 2026-11-20) */
void (*OrreHook_WalkStick)(s8* x, s8* y, s8* rawX, s8* rawY);
/* the right stick copies the left one in story mode (pc_cstick_walk.cpp sets it; 2026-11-21) */
void (*OrreHook_PadSticks)(s32 id, s8* stickX, s8* stickY, s8* substickX, s8* substickY);
/* the quicker way back from the PC screen: the loader's jobs run sooner after each update pass (pc_field_menu.cpp sets
 * it; 2026-11-21) */
void (*OrreHook_AfterGappUpdate)(void);
/* step 3.7's Smooth Party UI (pc_party_ui.cpp sets these; 2026-11-20) */
s32 (*OrreHook_PartyPortraitWait)(s32 fromBattle, s32 waited);
s32 (*OrreHook_FaceDrawHold)(u16 key);
void* (*OrreHook_FaceStandIn)(u16 key, u32 resourceId); /* 2026-11-21: the stand-in portraits (no pause) */

/* set by the game side (src/game/pc_debug_game.c) at start-up */
int (*pc_cheats_pc_top_up)(int item, int target);
int (*pc_cheats_money_fill)(void);

static int s_on[PC_CHEAT_SWITCH_COUNT];
static int s_exp = 1;

static const char* const kNameKeys[PC_CHEAT_SWITCH_COUNT] = {
    "cheats.instant_win", "cheats.instant_catch", "cheats.infinite_money", "cheats.items_never_run_out"};

/* "Put in the PC": the items of the menu, in its order (ids from the game's item table, Gen 3 numbering: verified
 * against the disc's item names, 2026-11-13) */
static const struct {
    int item;
    const char* key;
} kPut[] = {{1, "cheats.item.master_ball"},
            {68, "cheats.item.rare_candy"},
            {25, "cheats.item.max_revive"},
            {37, "cheats.item.max_elixir"},
            {19, "cheats.item.full_restore"},
            {23, "cheats.item.full_heal"}}; /* added 2026-11-13 (snatcherLeo: "We can also add "Full Heal" to this menu") */
enum { PUT_COUNT = (int)(sizeof(kPut) / sizeof(kPut[0])), PUT_TARGET = 99 };

/* queued work for the game thread (pc_cheats_frame): a "Put in the PC" per item, and the money fill */
static volatile int s_put_pending[PUT_COUNT];
/* each item's mark in the open list: 0 none, 1 done (at 99 or more now), 2 failed (storage full, or no game); cleared
 * when the list closes (pc_cheat_put_clear_marks) */
static volatile int s_put_mark[PUT_COUNT];
static volatile int s_put_changed;
static volatile int s_money_fill_pending;
/* the last "Put in the PC" result, for the overlay's notice (the menu is closed by then) */
static char s_notice[512];
static volatile int s_notice_ready;

/* ---- the hooks' bodies ---- */

/* Instant win: a damaging move from the player's side into the other side knocks the target out */
static s32_t hook_move_damage(int attackerOnSide0, int targetOnSide0, s32_t damage, s32_t targetHp) {
    if (attackerOnSide0 && !targetOnSide0 && damage > 0 && targetHp > damage) {
        return targetHp;
    }
    return damage;
}
static int hook_catch_certain(void) { return 1; }
static u32_t hook_money_taken(u32_t amount) {
    (void)amount;
    return 0;
}
static int hook_item_use_keeps(u16 item) {
    (void)item;
    return 1;
}
static u32_t hook_exp_gained(u32_t exp) {
    const unsigned long long n = (unsigned long long)exp * (unsigned)s_exp;
    return n > 0x7FFFFFFFull ? 0x7FFFFFFFu : (u32_t)n; /* the game adds it to a u32 total: kept well inside it */
}

/* ---- the user log: logs/Cheats log.txt (never pruned; pc_log.c deletes only orreport_*.log) ---- */
static void note_in_recording(const char* line); /* below: every logged cheat action is noted beside the recording */

static void user_log(const char* line) {
    char path[1100];
    note_in_recording(line);
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    FILE* f;

    printf("[cheats] %s\n", line); /* the run log too, so a bug report carries it */
    pc_paths_get(PC_PLACE_LOGS, "Cheats log.txt", path, sizeof(path)); /* data\system\logs\ (flat layout: logs\), 4.1 */
    f = fopen(path, "a");
    if (f == NULL) {
        return;
    }
    if (t != NULL) {
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %s\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
                t->tm_min, t->tm_sec, line);
    } else {
        fprintf(f, "%s\n", line);
    }
    fclose(f);
}

/* *Added 2026-11-20:* a cheat used during a recorded sitting is noted beside the recording
 * (<recording>.cheats.txt, one line per switch or action), because the recording alone holds only the button presses:
 * a sitting where a cheat changed the game can't be replayed on a console or checked against Dolphin, and whoever
 * looks at it later must be able to tell. Plain C file calls; every OS the same */
static void note_in_recording(const char* line) {
    const char* rec = pc_pad_record_path();
    char path[1400];
    FILE* f;
    if (rec == NULL || rec[0] == '\0') {
        return;
    }
    snprintf(path, sizeof(path), "%s.cheats.txt", rec);
    f = fopen(path, "a");
    if (f == NULL) {
        return;
    }
    fprintf(f, "%s\n", line);
    fclose(f);
}

static void log_switch(const char* name, int on) {
    char line[512];
    pc_text_format(line, sizeof(line), "cheats.log.switch", "what", name, "state",
                   pc_text(on ? "cheats.log.on" : "cheats.log.off"), (const char*)NULL);
    user_log(line);
}

/* ---- the switches ---- */

/* *Changed 2026-11-20 (snatcherLeo: "I think, you can have a legitimate save with these cheats, the cheats just act as
 * modifiers that can be toggled off whenever and help you advance through the game easily, we don't need to have them
 * off"):* the cheats are allowed in GameCube-accurate mode too (they were off there since 2026-11-13). Every result
 * stays inside the game's own limits (money at most the wallet's maximum, items at 99, normal levels), so a save made
 * with them is still a normal GameCube save (likely). Accurate mode then means "as on a GameCube while no cheat is on":
 * all start off, each switch is logged (Cheats log.txt) and noted beside a recording (note_in_recording). The function
 * stays so the menu can still ask. */
int pc_cheats_allowed(void) { return 1; }

int pc_cheat_on(int which) { return which >= 0 && which < PC_CHEAT_SWITCH_COUNT && s_on[which]; }

const char* pc_cheat_name_key(int which) {
    return which >= 0 && which < PC_CHEAT_SWITCH_COUNT ? kNameKeys[which] : "";
}

void pc_cheat_set(int which, int on) {
    if (which < 0 || which >= PC_CHEAT_SWITCH_COUNT) {
        return;
    }
    on = on && pc_cheats_allowed();
    if (s_on[which] == on) {
        return;
    }
    s_on[which] = on;
    switch (which) {
    case PC_CHEAT_INSTANT_WIN: OrreHook_MoveDamage = on ? hook_move_damage : NULL; break;
    case PC_CHEAT_INSTANT_CATCH: OrreHook_CatchCertain = on ? hook_catch_certain : NULL; break;
    case PC_CHEAT_INFINITE_MONEY:
        OrreHook_MoneyTaken = on ? hook_money_taken : NULL;
        if (on) {
            s_money_fill_pending = 1; /* the wallet filled on the game thread (pc_cheats_frame) */
        }
        break;
    case PC_CHEAT_ITEMS_NEVER_RUN_OUT: OrreHook_ItemUseKeeps = on ? hook_item_use_keeps : NULL; break;
    }
    log_switch(pc_text(kNameKeys[which]), on);
}

int pc_cheat_exp_multiplier(void) { return s_exp; }

void pc_cheat_set_exp_multiplier(int times) {
    char what[256], value[64];
    if (times != 2 && times != 5 && times != 10) {
        times = 1;
    }
    if (!pc_cheats_allowed()) {
        times = 1;
    }
    if (times == s_exp) {
        return;
    }
    s_exp = times;
    OrreHook_ExpGained = times > 1 ? hook_exp_gained : NULL;
    /* "EXP: ×5" / "EXP: Normal" */
    if (times > 1) {
        char n[8];
        snprintf(n, sizeof(n), "%d", times);
        pc_text_format(value, sizeof(value), "cheats.exp.times", "n", n, (const char*)NULL);
    } else {
        snprintf(value, sizeof(value), "%s", pc_text("cheats.exp.normal"));
    }
    snprintf(what, sizeof(what), "%s", pc_text("cheats.exp"));
    {
        char line[512];
        pc_text_format(line, sizeof(line), "cheats.log.switch", "what", what, "state", value, (const char*)NULL);
        user_log(line);
    }
}

int pc_cheats_on_count(void) {
    int n = s_exp > 1 ? 1 : 0;
    for (int i = 0; i < PC_CHEAT_SWITCH_COUNT; i++) {
        n += s_on[i] != 0;
    }
    return n;
}

void pc_cheats_list(char* out, unsigned size) {
    size_t used = 0;
    if (size == 0) {
        return;
    }
    out[0] = '\0';
    for (int i = 0; i < PC_CHEAT_SWITCH_COUNT; i++) {
        if (s_on[i]) {
            used += (size_t)snprintf(out + used, used < size ? size - used : 0, "%s%s", used ? ", " : "",
                                     pc_text(kNameKeys[i]));
        }
    }
    if (s_exp > 1 && used < size) {
        char n[8], value[64];
        snprintf(n, sizeof(n), "%d", s_exp);
        pc_text_format(value, sizeof(value), "cheats.exp.times", "n", n, (const char*)NULL);
        snprintf(out + used, size - used, "%s%s %s", used ? ", " : "", pc_text("cheats.exp"), value);
    }
}

/* ---- Put in the PC ---- */

int pc_cheat_put_count(void) { return PUT_COUNT; }

const char* pc_cheat_put_key(int index) { return index >= 0 && index < PUT_COUNT ? kPut[index].key : ""; }

void pc_cheat_put_in_pc(int index) {
    if (index >= 0 && index < PUT_COUNT && pc_cheats_allowed()) {
        s_put_pending[index] = 1;
    }
}

/* the game thread, at the frame boundary (pc_vi.c), where the debug commands also run, so the save is in a
 * consistent state while it changes */
void pc_cheats_frame(void) {
    if (s_money_fill_pending) {
        s_money_fill_pending = 0;
        if (pc_cheats_money_fill != NULL) {
            const int money = pc_cheats_money_fill();
            printf("[cheats] money filled: %d\n", money); /* -2: no game loaded yet (filled at the next switch) */
        }
    }
    for (int i = 0; i < PUT_COUNT; i++) {
        char line[512], count[16];
        const char* name;
        int after;
        if (!s_put_pending[i]) {
            continue;
        }
        s_put_pending[i] = 0;
        name = pc_text(kPut[i].key);
        if (pc_cheats_pc_top_up == NULL) {
            continue;
        }
        after = pc_cheats_pc_top_up(kPut[i].item, PUT_TARGET);
        if (after == -3) {
            pc_text_format(line, sizeof(line), "cheats.log.put_none", "what", name, (const char*)NULL);
        } else if (after == -2) {
            pc_text_format(line, sizeof(line), "cheats.log.put_no_game", "what", name, (const char*)NULL);
        } else if (after == -1) {
            pc_text_format(line, sizeof(line), "cheats.log.put_full", "what", name, (const char*)NULL);
        } else {
            snprintf(count, sizeof(count), "%d", after);
            pc_text_format(line, sizeof(line), "cheats.log.put", "what", name, "count", count, (const char*)NULL);
        }
        user_log(line);
        /* the list stays open (snatcherLeo, 2026-11-13: "it simply puts a checkmark next to each one, and the menu doesn't
         * close, in case I wanna put a check mark next to others"): a check mark when it worked; only a failure gets
         * the notice over the game, since the mark can't say why */
        s_put_mark[i] = after >= 0 || after == -3 ? 1 : 2;
        s_put_changed = 1;
        if (s_put_mark[i] == 2) {
            snprintf(s_notice, sizeof(s_notice), "%s", line); /* the overlay shows it (pc_cheats_take_notice) */
            s_notice_ready = 1;
        }
    }
}

int pc_cheat_put_mark(int index) { return index >= 0 && index < PUT_COUNT ? s_put_mark[index] : 0; }

void pc_cheat_put_clear_marks(void) {
    for (int i = 0; i < PUT_COUNT; i++) {
        s_put_mark[i] = 0;
    }
}

int pc_cheat_put_take_changed(void) {
    const int changed = s_put_changed;
    s_put_changed = 0;
    return changed;
}

int pc_cheats_take_notice(char* out, unsigned size) {
    if (!s_notice_ready || size == 0) {
        return 0;
    }
    s_notice_ready = 0;
    snprintf(out, size, "%s", s_notice);
    return 1;
}
