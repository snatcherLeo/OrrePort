/*
 * pc_relic_forest.cpp: step 3.7's Relic Forest shortcuts (the project's build helper, 2026-11-20; decided in
 * docs/port/tweaks-37-research.md sections 2-3 and "snatcherLeo's answers (2026-11-20)" 5-6; how the Relic Stone and the Time
 * Flute work: docs/engine/shadow-system.md "Purification"; the hooks: include/pc/orre_hooks.h, "Step 3.7's Relic Forest
 * shortcuts").
 *
 * What it does (one switch for both, snatcherLeo's answer 6: "relic_forest" in pc_options.c):
 *   1. PURIFY in the party menu. In the Relic Forest (room 0x27: the forest and the stone's clearing are one room), once
 *      the stone works (flag 1067, the Small Tablet), a party Pokémon whose heart is fully open (Shadow, gauge exactly 0:
 *      the stone's own test) gets PURIFY in its menu: SUMMARY / SWITCH / ITEM / PURIFY / CANCEL. Only then (snatcherLeo's
 *      answer 5: "only once the heart is fully open"). Choosing it closes the party screen and the pause menu and runs
 *      the stone's own examine event (the room script's function 9) for that Pokémon: its own checks, then the same
 *      ceremony (room 0x385, the messages, EXP, moves, evolution, ribbon, nickname) as walking up to the stone, without
 *      asking "Which POKéMON..." again. The stone keeps working exactly as before ("we aren't getting RID of that way,
 *      just adding another method").
 *   2. The Time Flute anywhere in the Relic Forest. The game makes the flute usable only while Wes faces the stone as
 *      the pause menu opens (the stone's function 11 adds it to the pause menu's "usable now" list). Here the same entry
 *      is added whenever the pause menu opens in room 0x27, so the flute works anywhere in the forest; everything after
 *      is the game's own (only a heart not yet open, single-use, the room's function 10, the Celebi ceremony).
 *
 * How (pause-menu life through OrreHook_PauseMenuStep; the party menu's records like PC MENU's, pc_field_menu.cpp):
 *   - step 0 (the pause menu opens, in the field room): note the room; add the flute's entry in room 0x27;
 *   - step 1 (POKéMON opens the party screen): from here to step 2 the party screen is the pause menu's own (the PC's
 *     party view or a battle's never get PURIFY);
 *   - the party's per-Pokémon menu 0x6B (OrreHook_PartySubmenuOpen): when the slot's heart is open, PURIFY's record
 *     (0x7080, a copy of ITEM's) is linked between ITEM and CANCEL and the box grows by one row upward (the three upper
 *     buttons and the box's three pieces move up one step, CANCEL stays); otherwise the game's own records go back,
 *     from a copy taken the first time;
 *   - choosing it (OrreHook_PartySubmenuChoice) remembers the slot and closes the party screen;
 *   - step 2 then leaves the pause menu and queues event 0x01000009 (script 0x0100 = the room's script, function 9 =
 *     the stone's examine event: cms_ScriptCreateThread splits the word so; the flute's event is 0x0100000A, function
 *     10). The hero step runs it next frame, as a script queued by ITEMS;
 *   - the stone's ceremony start (OrreHook_RelivePickSlot) answers the remembered slot after checking the heart again,
 *     so nothing is asked twice; anything else (no pending PURIFY, the heart changed) gets the game's own picker.
 *
 * GameCube-accurate mode and recordings: off (pc_option is 0 there; automated value 0), so a console route's presses
 * keep their meaning: the extra row moves CANCEL down one, and the flute anywhere changes where Wes must stand. The test
 * switch ORRE_RELIC_FOREST=1 turns it on in a test run.
 *
 * Random numbers: none drawn here, nor by the code the hooks touch (the menus, the stone's checks, the flute's use
 * function; verified by search, research sections 2-3). What changes is where Wes stands and how many frames pass
 * before the ceremony, as for a player who walks a different path.
 *
 * Saves: nothing stored outside the game's own save; the ceremony writes what it writes at the stone.
 *
 * Platforms: plain C++ over the game's menu records and script events (no OS calls); Windows, Linux and macOS the same
 * (likely: built and run on Windows only so far).
 * For XD: XD purifies at the Relic Stone and with the Purify Chamber; its pause menu and party menu are the same engine
 * family, so the hook places would likely be the same functions at other addresses, with other room, flag, item and
 * menu ids (guess until XD is checked). docs/xd-insights.md has the entry.
 */
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pc_lowmem.h"
#include "pc_options.h"
#include "pc_text.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
const char* pc_game_text_language(void);
}
#include "pc_game_ui.h"

namespace {

/* the game's (room list, room script, menu tables: research sections 2-3; the records decoded 2026-11-20 from the item
 * table, docs/engine/menus.md) */
constexpr u32 kRoomRelicForest = 0x27;
constexpr int kFlagStoneWorks = 1067;             /* the Small Tablet received: the Relic Stone purifies */
constexpr u16 kItemTimeFlute = 537;               /* key item 0x219 */
constexpr u32 kEventFlute = 0x0100000A;           /* room script function 10: preReliveSetParameter(1, slot) */
constexpr u32 kEventStone = 0x01000009;           /* room script function 9: the stone's examine event */
constexpr s32 kMenuPartySub = 0x6B;               /* the party list's per-Pokémon menu (0x10F in a Mt. Battle run) */
constexpr int kBoxLeft = 0x3CC, kBoxRight = 0x3CD, kBoxMid = 0x3CE; /* the box's three pieces */
constexpr int kSummary = 0x3CF, kSwitch = 0x3D0, kItem = 0x3D1, kCancel = 0x3D2; /* its buttons, in chain order */
/* OrrePort's */
constexpr int kItemPurify = 0x7080;               /* past the table (0x7000-0x707x are the other OrrePort menus') */
constexpr u32 kKeyPurify = 0xFFF00044;            /* PURIFY's label (0xFFF00040-43: the bag's) */

void put16(u8* p, int off, u16 v) { p[off] = (u8)(v >> 8), p[off + 1] = (u8)v; }
void put32(u8* p, int off, u32 v) { p[off] = (u8)(v >> 24), p[off + 1] = (u8)(v >> 16), p[off + 2] = (u8)(v >> 8), p[off + 3] = (u8)v; }
s16 get16(const u8* p, int off) { return (s16)((p[off] << 8) | p[off + 1]); }
u32 get32(const u8* p, int off) { return (u32)p[off] << 24 | (u32)p[off + 1] << 16 | (u32)p[off + 2] << 8 | p[off + 3]; }

/* the game's records as the disc has them, copied the first time (putting them back is then exact) */
constexpr int kOwn[] = {kBoxLeft, kBoxRight, kBoxMid, kSummary, kSwitch, kItem, kCancel};
constexpr size_t kOwnCount = sizeof(kOwn) / sizeof(kOwn[0]);
u8 s_orig[kOwnCount][0x1C];
bool s_saved = false;
bool s_applied = false;
u8* s_record = nullptr;  /* PURIFY's record (below 4 GB: the game reads it as its own) */
void* (*s_prevMenuExtra)(int, u32) = nullptr;

/* the pause menu's life */
bool s_inPause = false;      /* between step 0 and step 3 */
u32 s_pauseRoom = 0;         /* the field room when the pause menu opened */
bool s_pauseParty = false;   /* the party screen open from the pause menu's POKéMON (step 1 to step 2) */
s32 s_chosenSlot = -1;       /* PURIFY chosen on this slot; the party screen is closing */
s32 s_pendingSlot = -1;      /* queued for the stone's event; taken by the ceremony's start */

u8* item(int id) { return static_cast<u8*>(pc_game_ui.menu_item(id)); }

bool on() { return pc_game_ui.ready && pc_option("relic_forest") != 0; }

bool in_forest() { return s_inPause && s_pauseRoom == kRoomRelicForest; }

bool stone_works() { return pc_game_ui.flag_get != nullptr && pc_game_ui.flag_get(kFlagStoneWorks) != 0; }

void put_back() {
    if (!s_saved || !s_applied) return;
    for (size_t i = 0; i < kOwnCount; ++i) std::memcpy(item(kOwn[i]), s_orig[i], 0x1C);
    s_applied = false;
}

/* PURIFY between ITEM and CANCEL, the box one row taller upward. The buttons are spaced 24-25 px (y 336, 360, 385,
 * 410 on the US disc); the step is CANCEL's distance from ITEM, so the layout follows whatever the disc has */
bool apply() {
    if (!s_saved) {
        for (size_t i = 0; i < kOwnCount; ++i) {
            const u8* r = item(kOwn[i]);
            if (r == nullptr) return false;
            std::memcpy(s_orig[i], r, 0x1C);
        }
        s_saved = true;
    }
    if (s_record == nullptr) {
        s_record = static_cast<u8*>(pc_lowmem_alloc(0x1C));
        if (s_record == nullptr) return false;
    }
    const u8* itemRec = s_orig[5];
    const u8* cancelRec = s_orig[6];
    const s16 step = (s16)(get16(cancelRec, 4) - get16(itemRec, 4));
    for (size_t i = 0; i < kOwnCount; ++i) std::memcpy(item(kOwn[i]), s_orig[i], 0x1C);
    /* PURIFY: ITEM's place, size, picture and callbacks, at ITEM's y (ITEM moves up below), its own label, then CANCEL */
    std::memcpy(s_record, itemRec, 0x1C);
    put32(s_record, 0x10, kKeyPurify);
    s_record[0] = (u8)((s_record[0] | 0x80) & ~0x40); /* shown, selectable, not the chain's end */
    put16(s_record, 0x18, (u16)kCancel);
    put16(item(kItem), 0x18, (u16)kItemPurify);
    /* SUMMARY, SWITCH, ITEM one step up; the box's pieces one step up and one step taller (height at +0x08) */
    for (int id : {kSummary, kSwitch, kItem}) put16(item(id), 4, (u16)(get16(item(id), 4) - step));
    for (int id : {kBoxLeft, kBoxRight, kBoxMid}) {
        u8* r = item(id);
        put16(r, 4, (u16)(get16(r, 4) - step));
        put16(r, 8, (u16)(get16(r, 8) + step));
    }
    /* the label in the game's language, in the font of the disc's own ITEM label (made each time: the language can
     * change) */
    const char* lang = pc_game_text_language();
    pc_game_text_set_own(kKeyPurify, pc_text_lang(lang, "game.party_menu.purify"), get32(itemRec, 0x10));
    if (!s_applied) {
        const s32 rect = pc_game_ui.msg_rect != nullptr ? pc_game_ui.msg_rect(kKeyPurify) : 0;
        std::printf("[relic_forest] PURIFY added to the party menu (%s, \"%s\", %d px of the button's %d; step %d)\n", lang,
                    pc_text_lang(lang, "game.party_menu.purify"), (int)(rect >> 16), (int)get16(itemRec, 6), (int)step);
    }
    s_applied = true;
    return true;
}

/* OrreHook_MenuExtra: PURIFY's record; anything else to the hook registered before this one */
void* menu_extra(int kind, u32 index) {
    if (kind == 0 && s_record != nullptr && index == (u32)kItemPurify) return s_record;
    return s_prevMenuExtra != nullptr ? s_prevMenuExtra(kind, index) : nullptr;
}

/* OrreHook_PauseMenuStep */
s32 pause_menu_step(s32 step, u32* eventId, u32* value) {
    switch (step) {
    case 0:
        s_inPause = true;
        s_pauseParty = false;
        s_chosenSlot = -1;
        s_pendingSlot = -1; /* a PURIFY whose stone event never reached the ceremony (it can't, but never carried on) */
        s_pauseRoom = pc_game_ui.ready && pc_game_ui.current_room != nullptr ? pc_game_ui.current_room() : 0;
        if (s_pauseRoom == kRoomRelicForest && on()) {
            /* the flute anywhere here: the same entry the stone's function 11 adds when Wes faces it (a duplicate when he
             * does is harmless: the lookup takes the first) */
            pc_game_ui.menu_queue_add(kItemTimeFlute, kEventFlute);
            std::printf("[relic_forest] the pause menu opened in the Relic Forest: the Time Flute is usable here\n");
        }
        return 0;
    case 1:
        s_pauseParty = true;
        s_chosenSlot = -1;
        return 0;
    case 2:
        s_pauseParty = false;
        put_back();
        if (s_chosenSlot >= 0 && eventId != nullptr && value != nullptr) {
            s_pendingSlot = s_chosenSlot;
            s_chosenSlot = -1;
            *eventId = kEventStone;
            *value = (u32)s_pendingSlot;
            std::printf("[relic_forest] PURIFY: the pause menu closes and the Relic Stone's event runs for slot %d\n",
                        (int)s_pendingSlot);
            return 1;
        }
        return 0;
    default:
        s_inPause = false;
        s_pauseParty = false;
        put_back();
        return 0;
    }
}

/* OrreHook_PartySubmenuOpen: PURIFY on a ready heart, the game's own menu otherwise */
void party_submenu_open(s32 menuId, s32 slot) {
    const bool want = menuId == kMenuPartySub && s_pauseParty && in_forest() && on() && stone_works() &&
                      pc_game_ui.party_heart(slot) == 2;
    if (!want) {
        put_back();
        return;
    }
    if (!apply()) {
        std::printf("[relic_forest] PURIFY couldn't be added: the party menu is the game's own\n");
        put_back();
    }
}

/* OrreHook_PartySubmenuChoice */
s32 party_submenu_choice(s32 itemId, s32 slot) {
    if (!s_applied || itemId != kItemPurify) return 0;
    if (pc_game_ui.party_heart(slot) != 2) return 0; /* can't change while the menu is open; checked for safety */
    s_chosenSlot = slot;
    std::printf("[relic_forest] PURIFY chosen for party slot %d: the party screen closes\n", (int)slot);
    return 1;
}

/* OrreHook_RelivePickSlot: the slot PURIFY chose, once, if its heart is still open (the picker's own test) */
s32 relive_pick_slot() {
    const s32 slot = s_pendingSlot;
    s_pendingSlot = -1;
    if (slot < 0) return -1;
    if (pc_game_ui.party_heart(slot) != 2) {
        std::printf("[relic_forest] PURIFY's slot %d is no longer ready: the stone asks as usual\n", (int)slot);
        return -1;
    }
    std::printf("[relic_forest] the Relic Stone's ceremony starts for slot %d (chosen with PURIFY)\n", (int)slot);
    return slot;
}

}  // namespace

/* called once at start-up (pc_platform.c), after the other menus' item hooks (it chains them); each hook asks the
 * switch when called, so a Settings change takes effect at once */
extern "C" void pc_relic_forest_start(void) {
    s_prevMenuExtra = OrreHook_MenuExtra;
    OrreHook_MenuExtra = menu_extra;
    OrreHook_PauseMenuStep = pause_menu_step;
    OrreHook_PartySubmenuOpen = party_submenu_open;
    OrreHook_PartySubmenuChoice = party_submenu_choice;
    OrreHook_RelivePickSlot = relive_pick_slot;
}
