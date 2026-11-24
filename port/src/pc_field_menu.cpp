/*
 * pc_field_menu.cpp: the field pause menu's PC MENU entry and Start closing the menu ("Access PC menu anywhere", Main
 * 2026-11-15; the project plan, "Access PC menu anywhere [0.1]"; how the menu is built: docs/engine/pause-menu.md).
 *
 * Why it exists: the game saves only through a PC (snatcherLeo, 2026-11-12: "we still allow users to access their PC menu so
 * they can access theri PC from anywhere, and they can save from anywhere as a result"). The pause menu gains PC MENU
 * right under POKéMON (snatcherLeo, 2026-11-13: "We can put PC MENU then right after POKEMON"; mockup approved 2026-11-15),
 * which opens the game's own PC menu room (POKéMON Storage, Item Storage, Save), the same screens a PC in a Pokémon
 * Center opens. And Start closes the pause menu as B does (snatcherLeo, 2026-11-15: "users being able to additionally click
 * "START" at this screen, and unpause (so it would function just like B"; "only for this specific PAUSE MENU"; the Mt.
 * Battle run's shorter pause menu too, "for consistency", but without PC MENU: "wouldn't make sense").
 *
 * How (the menu's records, decoded 2026-11-15 from main.dol's item table; pause-menu.md):
 *   - The pause menu (menu 0x41) is a chain of item records: four rail pieces (0x61 bottom at y 223, 0x62 / 0x63
 *     middles at 137 / 180, 0x64 top at 79) and four buttons 43 px apart (0x65 POKéMON y 95, 0xDB0 P★DA 138, 0xDB1
 *     ITEMS 181, 0xDB2 EXIT 224, the chain's last). Each is drawn on its own, so a fifth button is one more record.
 *   - PC MENU's button 0x7060 is a copy of P★DA's record at y 138, its text OrrePort's own message 0xFFF00030; it is
 *     linked after EXIT, so EXIT keeps choice 3 and PC MENU is choice 4 (the menu numbers its choices in chain order),
 *     while the cursor, which goes to the nearest entry in the pressed direction, reaches it right under POKéMON.
 *     P★DA, ITEMS and EXIT move down 43 px; one more rail middle (0x7061, a copy of 0x63 at y 223) goes into the rail's
 *     part of the chain (before the buttons: the rail is drawn under them) and the rail's bottom moves to 266.
 *   - Both records are served past the item table's end (OrreHook_MenuExtra, chained with the other OrrePort menus').
 *   - Every time the menu opens (OrreHook_FieldMenuOpen) the layout is applied, or the game's own records put back
 *     when PC MENU is off, from a copy taken the first time, so the table is never left half changed.
 *   - The slide-out of the chosen button (fn_8000D3AC) knows only the game's four: OrreHook_FieldMenuItem names 0x7060
 *     for position 4. Choosing it (OrreHook_FieldMenuChoice) opens the PC room in the game code (gs_range_8000D290.c),
 *     the way the P★DA is opened; afterwards the pause menu opens again, as after the game's own entries.
 *   - Start: OrreHook_MenuBackKeys answers MENU_KEY_START for menus 0x41 and 0x109 only (the game's B handling is
 *     shared by every menu).
 *
 * The label: "game.field_menu.pc" in ui/lang (the game's language, as OrrePort's other words inside the game's
 * screens), in the font the disc's own POKéMON label asks for (font 0; the Italian disc's labels use font 3), read from
 * that message's first control code.
 *
 * The switch: built in, not a Settings entry (snatcherLeo, 2026-11-15: "turned off via a switch with gamecube accurate mode
 * (inetrnal switch, don't have this in settings anywhere)"). Off in GameCube-accurate mode (the pause menu is the
 * game's own there, with B alone) and in automated runs, whose recorded presses were made on the game's own menu;
 * ORRE_FIELD_PC_MENU=1 turns it on in a test run. Its row in the accurate-mode list: the project plan, section 5.
 *
 * Random numbers: none drawn here (menu records and the game's own menu code; likely, to be confirmed by the
 * determinism test). Opening the PC only changes how many frames pass, as walking to a PC would.
 *
 * Platforms: all the same (plain C++ over the game's own menu and text code; no OS calls). Linux and macOS need nothing
 * different (likely).
 * For XD: XD's pause menu has its own records and its PC its own room; the approach (records past the table's end, a
 * copy of a button, the chain order for the choice number) carries over (likely); the ids and positions differ.
 */
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pc_lowmem.h"
#include "pc_options.h"
#include "pc_saves.h"
#include "pc_text.h"

extern "C" {
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/orre_hooks.h"
void pc_game_text_set_own(u32 key, const char* utf8, u32 fontRef);
int pc_platform_automated_run(void);
const char* pc_game_text_language(void);
u32 VIGetRetraceCount(void); /* the screen's refresh count: when the fade to black is complete */
}
#include "pc_game_ui.h"

namespace {

void put16(u8* p, int off, u16 v) { p[off] = (u8)(v >> 8), p[off + 1] = (u8)v; }
void put32(u8* p, int off, u32 v) { p[off] = (u8)(v >> 24), p[off + 1] = (u8)(v >> 16), p[off + 2] = (u8)(v >> 8), p[off + 3] = (u8)v; }

/* the game's records (main.dol's item table; pause-menu.md) */
constexpr s32 kMenuPause = 0x41, kMenuPauseMtBattle = 0x109;
constexpr int kRailBottom = 0x61, kRailMid2 = 0x63, kPda = 0xDB0, kItems = 0xDB1, kExit = 0xDB2;
constexpr int kStep = 43;            /* one button and rail middle, in screen pixels (640 x 480) */
constexpr u32 kKeyPokemon = 0x136;   /* the disc's POKéMON label: its font is PC MENU's */
/* OrrePort's */
constexpr int kItemButton = 0x7060, kItemRail = 0x7061; /* past the table (0x7000-0x704x are the other OrrePort menus') */
constexpr u32 kKeyLabel = 0xFFF00030;
constexpr s32 kChoicePc = 4;

/* the game's records as the disc has them, copied the first time (so putting them back is exact) */
constexpr int kOwn[] = {kRailBottom, kRailMid2, kPda, kItems, kExit};
u8 s_orig[sizeof(kOwn) / sizeof(kOwn[0])][0x1C];
bool s_saved = false;
bool s_applied = false; /* the layout with PC MENU is in the table */
int s_shortFades = 0;   /* fades left to shorten when the PC screen opened from PC MENU is left (below) */
bool s_closeNow = false; /* that PC screen's windows skip their closing slide (behind the black screen; below) */
u32 s_exitAt = 0;        /* the retrace the PC screen was left at */
u32 s_blackAfter = 10;   /* retraces until the fade to black is complete */
/* the PC exit's fades, in seconds (the game's 0.5). 0.25 at first (2026-11-15); *changed 2026-11-16 to 0.15 (snatcherLeo's hand
 * check: "just a hair faster": about 20 % faster):* measured in a PC run, B on the PC screen to the pause menu
 * back took 83 frames with 0.25 (fade out 15, the leave job waiting for the black screen 2 more, the field room starting
 * again 47, fade in 15). The middle part is the field's own restart, left alone (pc-menu-anywhere.md, "Left as it is"),
 * so the fades carry the change: both 0.15 s (9 frames each), and the slide cut follows the shorter fade-out */
constexpr f32 kPcExitFade = 0.15f;
u8* s_items = nullptr;  /* OrrePort's two records (below 4 GB: the game reads them as its own) */
void* (*s_prevMenuExtra)(int, u32) = nullptr;

/* built in, off in GameCube-accurate mode and in automated runs (ORRE_FIELD_PC_MENU=1 turns it on in a test run) */
bool enabled() {
    if (!pc_game_ui.ready || pc_option_gamecube_accurate()) return false;
    if (!pc_platform_automated_run()) return true;
    const char* force = SDL_getenv("ORRE_FIELD_PC_MENU");
    return force != nullptr && force[0] == '1';
}

u8* item(int id) { return static_cast<u8*>(pc_game_ui.menu_item(id)); }
s16 y_of(const u8* r) { return (s16)((r[4] << 8) | r[5]); }

/* the font the disc's POKéMON label asks for (its first control code, FFFF 07 0n), so PC MENU looks like its neighbours
 * in every language (the Italian disc's labels use font 3); font 0 when it can't tell */
u32 label_font() {
    const u8* t = static_cast<const u8*>(pc_game_ui.msg_gschar != nullptr ? pc_game_ui.msg_gschar(kKeyPokemon) : nullptr);
    if (t != nullptr && t[0] == 0xFF && t[1] == 0xFF && t[2] == 0x07 && t[3] < 8) return 0xF0000000u | t[3];
    return 0xF0000000u;
}

void put_back() {
    if (!s_saved || !s_applied) return;
    for (size_t i = 0; i < sizeof(kOwn) / sizeof(kOwn[0]); ++i) std::memcpy(item(kOwn[i]), s_orig[i], 0x1C);
    s_applied = false;
    std::printf("[field_menu] the pause menu is the game's own again\n");
}

bool apply() {
    if (!s_saved) {
        for (size_t i = 0; i < sizeof(kOwn) / sizeof(kOwn[0]); ++i) {
            const u8* r = item(kOwn[i]);
            if (r == nullptr) return false;
            std::memcpy(s_orig[i], r, 0x1C);
        }
        s_saved = true;
    }
    if (s_items == nullptr) {
        s_items = static_cast<u8*>(pc_lowmem_alloc(0x1C * 2));
        if (s_items == nullptr) return false;
    }
    /* from the disc's records every time (idempotent) */
    const u8* pda = s_orig[2];
    const u8* mid2 = s_orig[1];
    u8* button = s_items;
    u8* rail = s_items + 0x1C;
    std::memcpy(button, pda, 0x1C); /* P★DA's place, size, picture and draw callback */
    put32(button, 0x10, kKeyLabel);
    button[0] = (u8)(button[0] | 0xC0); /* shown, selectable, and the chain's new end */
    put16(button, 0x18, 0);
    std::memcpy(rail, mid2, 0x1C);
    put16(rail, 4, (u16)(y_of(mid2) + kStep));
    put16(rail, 0x18, (u16)(mid2[0x18] << 8 | mid2[0x19])); /* 0x63's next (the rail's top) */
    for (size_t i = 0; i < sizeof(kOwn) / sizeof(kOwn[0]); ++i) std::memcpy(item(kOwn[i]), s_orig[i], 0x1C);
    /* the rail: 0x63 -> OrrePort's middle -> the top; the bottom one step lower */
    put16(item(kRailMid2), 0x18, (u16)kItemRail);
    put16(item(kRailBottom), 4, (u16)(y_of(s_orig[0]) + kStep));
    /* P★DA, ITEMS, EXIT one step down; EXIT -> PC MENU */
    for (int id : {kPda, kItems, kExit}) put16(item(id), 4, (u16)(y_of(item(id)) + kStep));
    u8* exit = item(kExit);
    exit[0] = (u8)(exit[0] & ~0x40);
    put16(exit, 0x18, (u16)kItemButton);
    /* the label, in the game's language and its labels' font (made each time: the language can change) */
    const char* lang = pc_game_text_language();
    pc_game_text_set_own(kKeyLabel, pc_text_lang(lang, "game.field_menu.pc"), label_font());
    if (!s_applied) {
        const s32 rect = pc_game_ui.msg_rect != nullptr ? pc_game_ui.msg_rect(kKeyLabel) : 0;
        std::printf("[field_menu] PC MENU added to the pause menu (%s, \"%s\", font %u, %d px of the button's %d)\n", lang,
                    pc_text_lang(lang, "game.field_menu.pc"), label_font() & 0xF, (int)(rect >> 16),
                    (int)((pda[6] << 8) | pda[7]));
    }
    s_applied = true;
    return true;
}

/* OrreHook_MenuExtra: OrrePort's two records; anything else to the hook registered before this one */
void* menu_extra(int kind, u32 index) {
    if (kind == 0 && s_items != nullptr && (index == (u32)kItemButton || index == (u32)kItemRail))
        return s_items + (size_t)(index - kItemButton) * 0x1C;
    return s_prevMenuExtra != nullptr ? s_prevMenuExtra(kind, index) : nullptr;
}

/* OrreHook_FieldMenuOpen: the layout with PC MENU on the story's pause menu; the game's own otherwise.
 * *Battle Mode (added 2026-11-20, the Battle Mode guard):* never PC MENU there. Its Mt. Battle course is walked in the
 * story's own field rooms (0x4C on), and a PC MENU save there would write Battle Mode's image as a story save; a
 * GameCube has no save in Battle Mode's pause menu either. The game already shows its own short pause menu (0x109:
 * POKéMON and quit) during a run (variable 0x8AE, gs_range_8000D290.c), which never gets PC MENU (snatcherLeo, 2026-11-15:
 * "wouldn't make sense"); the check here makes it direct for any Battle Mode stretch, from the game's own state
 * (pc_game_ui.battle_mode: the top menu's Battle Mode choice or 0x8AE; src/game/pc_game_ui.c). All platforms */
bool in_battle_mode() { return pc_game_ui.battle_mode != nullptr && pc_game_ui.battle_mode() != 0; }

void field_menu_open(s32 menuId) {
    s_shortFades = 0; /* never carried past the pause menu opening again */
    s_closeNow = false;
    if (enabled() && in_battle_mode()) {
        std::printf("[field_menu] Battle Mode (signal %d): the pause menu is the game's own (menu 0x%X), no PC MENU\n",
                    (int)pc_game_ui.battle_mode(), (unsigned)menuId);
        put_back();
        return;
    }
    if (menuId == kMenuPause && enabled()) {
        if (!apply()) std::printf("[field_menu] PC MENU couldn't be added: the pause menu is the game's own\n");
    } else {
        put_back();
    }
}

s32 field_menu_item(s32 selection) { return s_applied && selection == kChoicePc ? kItemButton : 0; }

s32 field_menu_choice(s32 choice) {
    if (!s_applied || choice != kChoicePc) return 0;
    std::printf("[field_menu] PC MENU: the PC menu opens\n");
    return 1;
}

/* Start closes the pause menu (both versions) as B does */
u32 menu_back_keys(u32 menuId) {
    return (menuId == (u32)kMenuPause || menuId == (u32)kMenuPauseMtBattle) && enabled() ? (u32)MENU_KEY_START : 0u;
}

/* the pause menu (both versions) wraps round: Up on POKéMON goes to EXIT, Down on EXIT to POKéMON, held or tapped
 * (snatcherLeo, 2026-11-15: "this makes navigation far better"; "this slight modification would ONLY be for the pause menu").
 * The game's own wrapping (its cursor mode 2 menus) does the moving */
s32 menu_wrap(u32 menuId) { return (menuId == (u32)kMenuPause || menuId == (u32)kMenuPauseMtBattle) && enabled() ? 1 : 0; }

/* ---- leaving the PC screen sooner (snatcherLeo, 2026-11-15: "could we slightly [shorten] the time it takes to get out of PC
 * MENU and back to the game? ... We would treat this delicately") ----
 * Measured in a PC run (60 frames a second): from B on the PC screen to the field again took 148 frames: the PC room's
 * fade to black (30), the room manager taking the PC screen down behind the black (37), the field room's archive read
 * again (42: a menu room frees it), and the field's fade back in (30).
 * How the cause was found (for the docs and snatcherLeo's videos; docs/port/pc-menu-anywhere.md): halving both fades brought
 * the field back on the same frame, so the 37 frames were not the fade. A test log in the room manager's leaving step
 * (gs_floor.c, ORRE_FLOOR_LOG) timed it: the PC room's leave job fn_800388C4 took 66 frames. It closes the PC screen's
 * eleven windows one after another; timing each window showed one, 0x1A (the right-hand panel), takes 60 frames: its
 * closing slide, mostly behind the black screen. Every other window closes in 0-2 frames.
 * The fix and its knobs (every PC screen: PC MENU's and a PC walked up to, snatcherLeo 2026-11-15; off in GameCube-accurate
 * mode and recordings):
 *   - once the screen is fully black, the rest of the closing slide is cut (OrreHook_WindowCloseNow), about 45 frames
 *     sooner; while the screen is still visible everything looks as the game's own (the first try cut it at once and
 *     the field flashed up bright). ORRE_PC_EXIT_SLIDE=1 keeps the whole slide;
 *   - ORRE_PC_EXIT_TOGETHER=1 closes the windows all at once (OrreHook_PcMenuCloseTogether; off: it showed the same
 *     flash);
 *   - the fade to black: ORRE_PC_EXIT_FADE_OUT=<seconds> (the game's 0.5; default here kPcExitFade, 0.15);
 *   - the field's fade back in: ORRE_PC_EXIT_FADE_IN=<seconds> (the game's 0.5; default here kPcExitFade, 0.15).
 * They are test switches for snatcherLeo to try values with (run_game.ps1 -KeepTests), not settings; the values he picks
 * become the defaults. Armed when that PC screen is left (OrreHook_PcMenuExit), disarmed after the fade back in (or when
 * the pause menu opens again). Fades and window closes draw no random numbers (likely), but they change how many frames
 * pass, so this is an enhancement row in the accurate-mode list (s_shortFades is declared at the top) */
bool s_exitTogether = false;  /* its leave job closes the windows together */

/* ---- the field room back sooner (2026-11-21, the project's build helper; snatcherLeo, after trying shorter fades: "the fade isn't
 * the problem. We can have the fade, but actually getting back to the main pause menu should be faster ... see if our
 * value we are modifying is actually what we need to modify") ----
 * Measured (PC runs, fast loading on, Mt. Battle's valley; docs/port/pc-menu-anywhere.md "Leaving the PC screen
 * sooner"): from B on the PC screen to the field fully back took 110 frames: the fade to black 9, the PC room's leave
 * jobs 3 more, then 80 frames with the room manager waiting for the field's archive (floorLoadData), then the fade in 9.
 * The archive itself came back from the loader's ARAM cache in one frame; the 78 frames were its compressed entries
 * going through the loader's job chain, one job per frame (an LZSS decode, then the entry's group callback, two frames
 * per entry, 39 entries). So the fades were not the thing to change: the chain was.
 * The fix: while the PC screen is being left (armed by pc_menu_exit, ended when the field's fade in starts), after each
 * frame's update pass (OrreHook_AfterGappUpdate) the loader's pending jobs and its own frame step run at once
 * (pc_game_ui.fsys_hurry, src/game/pc_game_ui.c), for at most kHurryBudgetUs a frame, in the chain's own order. Same
 * jobs, same order, same data: only the frames between them go. With the fast loading setting only (it is the same
 * idea: loads finish sooner), and with the rest of the PC exit's tweaks: off in GameCube-accurate mode and recordings.
 * ORRE_PC_EXIT_HURRY=0 turns it off in a test run (for comparing). Random numbers: none drawn by the loader's jobs
 * (likely; checked with ORRE_RNG_COUNT). Platforms: all the same */
constexpr int kHurryBudgetUs = 6000; /* 6 ms of a 16.7 ms frame, behind the black screen */
constexpr int kHurryMaxFrames = 600;  /* the arm's own stall check: ten seconds, then it ends whatever happens */
bool s_hurry = false;
int s_hurryFrames = 0, s_hurryJobs = 0;
void (*s_prevAfterGapp)(void) = nullptr;

void hurry_end(const char* why) {
    if (!s_hurry) return;
    s_hurry = false;
    std::printf("[field_menu] leaving the PC: %d loader job(s) run sooner over %d frame(s) (%s)\n", s_hurryJobs,
                s_hurryFrames, why);
}

void after_gapp_update() {
    if (s_prevAfterGapp != nullptr) s_prevAfterGapp();
    if (!s_hurry || pc_game_ui.fsys_hurry == nullptr) return;
    if (++s_hurryFrames > kHurryMaxFrames) {
        hurry_end("stopped: the field's fade in never came");
        return;
    }
    s_hurryJobs += pc_game_ui.fsys_hurry(kHurryBudgetUs);
}

f32 knob(const char* env, f32 fallback) {
    const char* v = SDL_getenv(env);
    if (v == nullptr || v[0] == '\0') return fallback;
    const f32 s = (f32)SDL_atof(v);
    return s > 0.0f && s <= 2.0f ? s : fallback;
}

/* *Changed 2026-11-15 (snatcherLeo: "it also applies to, if we walk up to a PC, and accessed the menu that way ... whatever
 * values we set for it"):* every PC screen, not only PC MENU's (both leave through this same code) */
void pc_menu_exit() {
    const bool on = enabled();
    s_shortFades = on ? 2 : 0;
    /* *Changed 2026-11-15 (pictures):* closing every window at once, or cutting the slide at once, made the PC screen
     * vanish while the screen was still bright (the field flashed up for a few frames before the fade caught up). So by
     * default the windows close in the game's order and the slide plays while the screen fades to black; only once it
     * is fully black is the rest of the slide cut (window_close_now). ORRE_PC_EXIT_TOGETHER=1 tries the all-at-once
     * closing again; ORRE_PC_EXIT_SLIDE=1 keeps the whole slide (the game's timing) */
    const char* together = SDL_getenv("ORRE_PC_EXIT_TOGETHER");
    s_exitTogether = on && together != nullptr && together[0] == '1';
    const char* slide = SDL_getenv("ORRE_PC_EXIT_SLIDE");
    s_closeNow = on && !(slide != nullptr && slide[0] == '1');
    s_exitAt = VIGetRetraceCount();
    s_blackAfter = (u32)(knob("ORRE_PC_EXIT_FADE_OUT", kPcExitFade) * 60.0f + 0.5f) + 1;
    /* the field room back sooner (above): with the fast loading setting, unless a test turns it off */
    const char* hurry = SDL_getenv("ORRE_PC_EXIT_HURRY");
    s_hurry = on && pc_option("fast_loading") != 0 && !(hurry != nullptr && hurry[0] == '0');
    s_hurryFrames = 0, s_hurryJobs = 0;
    if (on)
        std::printf("[field_menu] leaving the PC: windows %s, %s, fade out %.2f s, fade in %.2f s\n",
                    s_exitTogether ? "closed together" : "one after another",
                    s_closeNow ? "no closing slide" : "with the closing slide", knob("ORRE_PC_EXIT_FADE_OUT", kPcExitFade),
                    knob("ORRE_PC_EXIT_FADE_IN", kPcExitFade));
}

/* the PC screen's windows (0x18-0x23: the ones its leave job closes) skip the rest of their closing sequence once the
 * screen is fully black (the fade-out's length after the PC screen was left, plus a frame), while it is left from PC
 * MENU. Measured: window 0x1A (the right-hand panel) slides away for 60 frames; every other one closes in 0-2 */
s32 window_close_now(u32 windowId) {
    if (!s_closeNow || windowId < 0x18 || windowId > 0x23) return 0;
    return VIGetRetraceCount() - s_exitAt >= s_blackAfter ? 1 : 0;
}

s32 pc_menu_close_together() {
    const bool t = s_exitTogether;
    s_exitTogether = false;
    return t ? 1 : 0;
}

/* *Changed 2026-11-15 (measured):* first both fades were halved alone, but the field came back on the same frame (the
 * windows' one-after-another closing held it); with the windows closed together, the fade to black matters again */
/* the name-menu back-out's own timing (pc_fast_fades_backout, below): set while its fades run */
int s_backoutOuts = 0; /* fade-outs seen so far in the back-out (0: not in one) */
bool s_backout = false;

f32 fade_duration(u16 type, f32 duration) {
    if (s_backout && (type == 3 || type == 2)) {
        /* *2026-11-15 (snatcherLeo: "let's increase the speed a little", measured):* the back-out fades out twice (the name
         * screen, then the room change, while the screen is already black) and in once. The second fade-out is made
         * instant (nothing left to darken) and the others shorter than the PC exit's. Knobs for tuning by feel:
         * ORRE_NAME_BACK_FADE_OUT / _IN (seconds) */
        /* 0.15 s at first; *a hair faster, 0.10 s (snatcherLeo's hand check, 2026-11-15: "we could make just a hair faster
         * (just a hair) ... nothing crazy, just a tad")* */
        if (type == 3) return ++s_backoutOuts == 1 ? knob("ORRE_NAME_BACK_FADE_OUT", 0.10f) : 0.01f;
        s_backout = false;
        s_backoutOuts = 0;
        return knob("ORRE_NAME_BACK_FADE_IN", 0.10f);
    }
    if (s_shortFades <= 0 || (type != 3 && type != 2)) return duration;
    if (type == 3) {
        --s_shortFades;
        return knob("ORRE_PC_EXIT_FADE_OUT", kPcExitFade);
    }
    s_shortFades = 0; /* the fade back in is the last one */
    s_closeNow = false;
    hurry_end("the field's fade in starts");
    return knob("ORRE_PC_EXIT_FADE_IN", kPcExitFade);
}

/* ---- the exact spot (snatcherLeo, 2026-11-15: "A save from that PC MENU you lcick should load yo in teh exact spot you
 * saved tho") ----
 * While the PC screen opened from PC MENU is up, the game code has told us where the player stands
 * (OrreHook_FieldMenuSpot). If a save lands in that time (card task 4, result 4), the spot is written to the slot's
 * slot.ini with the new .gci's SHA-1 (pc_saves_spot_set); any other save clears it. On Continue (and the main menu's
 * LOAD, which ends in Continue) OrreHook_ContinueSpot gives it back only when the slot's .gci still has that SHA-1, so
 * a save made since at a real PC, in Dolphin, or put back by Restore / Undo goes on from its own return point. The .gci
 * itself is the game's own save, with the last Pokémon Center as its return point: a GameCube, Dolphin and
 * GameCube-accurate mode go on from there, and whiteouts always do (they never ask this hook). */
bool s_pcOpen = false;  /* the PC screen opened from PC MENU is up */
OrreFieldSpot s_spot{}; /* where the player stood (room 0: not a spot to resume) */
void (*s_prevCardTaskEnd)(s32, s32) = nullptr;

void field_menu_spot(const OrreFieldSpot* spot) {
    s_pcOpen = spot != nullptr;
    if (spot != nullptr) {
        s_spot = *spot;
        if (spot->room == 0) std::printf("[field_menu] the PC opened here: not a spot to resume (not a field room, or a story area with its own return point)\n");
        else std::printf("[field_menu] the PC opened in room 0x%X at (%.1f, %.1f, %.1f), facing %d\n", (unsigned)spot->room,
                         spot->x, spot->y, spot->z, (int)spot->facingDegrees);
    }
}

void card_task_end(s32 kind, s32 result) {
    if (s_prevCardTaskEnd != nullptr) s_prevCardTaskEnd(kind, result); /* the names' extras file first */
    /* *2026-11-15:* every write kind (a new game's first save is kind 9; 10 / 11 write too; memory-card.md), as the
     * names' extras file (pc_names.cpp), not only kind 4 */
    const bool wrote = result == 4 && (kind == 4 || kind == 9 || kind == 10 || kind == 11);
    if (!wrote || !pc_saves_ready() || !enabled()) return;
    const int slot = pc_saves_current();
    char sha[41] = {0};
    if (s_pcOpen && s_spot.room != 0 && pc_saves_save_digest(slot, sha)) {
        char line[200];
        std::snprintf(line, sizeof(line), "%s %X %X %u %.9g %.9g %.9g %d", sha, (unsigned)s_spot.room, (unsigned)s_spot.prevRoom,
                      (unsigned)s_spot.entry, s_spot.x, s_spot.y, s_spot.z, (int)s_spot.facingDegrees);
        pc_saves_spot_set(slot, line);
        std::printf("[field_menu] saved from PC MENU: Continue goes on from this spot (slot %d)\n", slot);
    } else {
        pc_saves_spot_set(slot, ""); /* a save made elsewhere: the game's own return point */
    }
}

s32 continue_spot(OrreFieldSpot* out) {
    if (!enabled() || !pc_saves_ready()) return 0;
    const int slot = pc_saves_current();
    char line[200], sha[41] = {0}, want[41] = {0};
    if (!pc_saves_spot_get(slot, line, sizeof(line))) return 0;
    OrreFieldSpot s{};
    unsigned room = 0, prev = 0, entry = 0;
    int facing = 0;
    if (std::sscanf(line, "%40s %X %X %u %f %f %f %d", want, &room, &prev, &entry, &s.x, &s.y, &s.z, &facing) != 8 || room == 0) {
        std::printf("[field_menu] slot %d's spot line can't be read: Continue goes on from the save's own return point\n", slot);
        return 0;
    }
    if (!pc_saves_save_digest(slot, sha) || std::strcmp(sha, want) != 0) {
        std::printf("[field_menu] slot %d's spot belongs to an older save: Continue goes on from the save's own return point\n", slot);
        pc_saves_spot_set(slot, "");
        return 0;
    }
    s.room = room, s.prevRoom = prev, s.entry = entry, s.facingDegrees = facing;
    *out = s;
    std::printf("[field_menu] Continue goes on from the PC MENU save's spot: room 0x%X at (%.1f, %.1f, %.1f), facing %d\n",
                room, s.x, s.y, s.z, facing);
    return 1;
}

} // namespace

/* Short fades for another quick exit (2026-11-15): the next `fadeOuts` fades to black and the fade back in take
 * the same short times as leaving a PC screen (the knobs above). Used by B on Wes's name menu, whose way back to the
 * main menu fades out twice (the name screen, then the room change) and in once: snatcherLeo: "We can cut back the amount of
 * time it takes to get back to main menu" (it took about 2.8 s, measured in a test run). Only called where the caller
 * is itself off in GameCube-accurate mode. Platforms: all. */
extern "C" void pc_fast_fades(int fadeOuts) { s_shortFades = fadeOuts; }

/* B on Wes's name menu: its own quicker timing (fade_duration above: first fade-out short, the second instant, the fade
 * in short), measured on its way back (2026-11-15) */
extern "C" void pc_fast_fades_backout(void) {
    s_backout = true;
    s_backoutOuts = 0;
}

/* Where the player stood when PC MENU opened the PC screen that is up now (2026-11-20, the softlock fix,
 * pc_save_windows.cpp): 1 with *out filled while that screen is up and the spot can be resumed (a field room), 0
 * otherwise (no PC MENU screen up, a PC walked up to, or not a field room). The unsafe-window table matches on this
 * room, not on the PC menu room the game is in while saving. Platforms: all. */
extern "C" int pc_field_menu_pc_spot(OrreFieldSpot* out) {
    if (!s_pcOpen || s_spot.room == 0) return 0;
    *out = s_spot;
    return 1;
}

/* called once at start-up (pc_platform.c), last: it chains the item hook (the other menus') and the card-task hook (the
 * names' extras file) */
extern "C" void pc_field_menu_start(void) {
    s_prevMenuExtra = OrreHook_MenuExtra;
    OrreHook_MenuExtra = menu_extra;
    s_prevCardTaskEnd = OrreHook_CardTaskEnd;
    OrreHook_CardTaskEnd = card_task_end;
    OrreHook_FieldMenuSpot = field_menu_spot;
    OrreHook_ContinueSpot = continue_spot;
    OrreHook_FieldMenuOpen = field_menu_open;
    OrreHook_FieldMenuItem = field_menu_item;
    OrreHook_FieldMenuChoice = field_menu_choice;
    OrreHook_MenuBackKeys = menu_back_keys;
    OrreHook_MenuWrap = menu_wrap;
    OrreHook_PcMenuExit = pc_menu_exit;
    OrreHook_PcMenuCloseTogether = pc_menu_close_together;
    OrreHook_WindowCloseNow = window_close_now;
    OrreHook_FadeDuration = fade_duration;
    s_prevAfterGapp = OrreHook_AfterGappUpdate;
    OrreHook_AfterGappUpdate = after_gapp_update;
}
