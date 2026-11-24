/*
 * pc_cstick_walk.cpp: step 3.7's right-stick walking (the project's build helper, 2026-11-20; decided in
 * docs/port/tweaks-37-research.md section 4 and "snatcherLeo's answers (2026-11-20)" 7; how walking reads the stick:
 * docs/engine/player-movement.md; the hook: include/pc/orre_hooks.h, "Step 3.7's right-stick walking").
 *
 * What it does ("cstick_walk" in pc_options.c): in the field, the right stick (the GameCube's C-stick) moves Wes too,
 * as the left stick does (Pokémon XD lets both sticks walk). A full push runs like the main stick (snatcherLeo's answer 7).
 * The main stick wins when both are pushed; the d-pad is tried after both, as before.
 *
 * *Changed 2026-11-21 (snatcherLeo, after checking XD: the right stick copies the left one everywhere in story mode, except in
 * battles and on the P★DA's model page; outside story mode, the game's own behaviour; *story battles included since
 * the same evening*, snatcherLeo: "in a battle, (in story mode) right stick should work just like left stick"):* the C-stick is now copied into
 * the main stick where the game latches the pad (OrreHook_PadSticks, pad_sticks below), so it moves menu cursors too;
 * OrreHook_WalkStick only keeps the test log. The two paragraphs below describe the 2026-11-20 walking-only design and
 * why it was made that way (snatcherLeo's earlier rule, "where the game uses the stick itself it keeps that use", still holds:
 * the copy is off exactly where the game uses the C-stick itself). The scale and walk-button paragraph still applies.
 *
 * Why in the walking code and not in the controller layer: the game's other C-stick readers (the battle menu's
 * direct pick, the P★DA's model page, the debug cameras) keep their own use only if the walking code alone sees the
 * C-stick as a walking stick (snatcherLeo: "where the game uses the stick itself it keeps that use"). Merging the sticks in the
 * pad state would also move menu cursors and hide which stick a recording used. The two readers that turn the stick
 * into steps both ask OrreHook_WalkStick: heroReadStick (Wes's step) and fn_80186B5C (the step that script command
 * std170 tests against a line on Mt. Battle and elsewhere), so the C-stick can't walk Wes past a scripted line.
 *
 * The C-stick's smaller range: the game clamps the C-stick to a circle of radius 44 and the main stick to 56, and Wes's
 * speed is the tilt / 28 (walk up to 1.0, run from 1.0 to 2.0). A full C-stick push read as it is would give 1.57, a
 * fast walk that never runs; so the values are scaled by 56 / 44 (a full push = 56 = a full run). The walk button
 * (OrrePort's own, which the controller layer applies to the main stick only) scales the C-stick here by the same
 * percentage (Settings › Controls), so holding it with the right stick walks.
 *
 * Not in the debug orbit camera (scene mode 5) or the debug free camera (mode 6): the orbit camera steers with the
 * C-stick while Wes walks (camera.c), and the free camera stops the hero step. Not in battles, menus or the P★DA: the
 * walking code doesn't run there, so the hook is never asked.
 *
 * GameCube-accurate mode and recordings: off (pc_option is 0 there; automated value 0): it changes which inputs move
 * Wes, and a console has no such thing. ORRE_CSTICK_WALK=1 turns it on in a test run. Recordings store the shaped pad
 * before the game reads it (pc_pad_record.c), so this never changes a recording; a normal-mode recording that walked
 * with the C-stick needs the switch on to replay, and the walk button's state with the C-stick is read live (not in the
 * recording), so such a stretch replays exactly only without the walk button (likely; untested).
 *
 * Random numbers: none drawn here or in the stick readers (verified by search, research section 4). Wes's path changes
 * what the field draws, as any walking does.
 *
 * Platforms: the stick values come from the game's own input layer (PAD through Aurora / SDL), so Windows, Linux and
 * macOS behave the same (likely: built and run on Windows only). What differs per OS is only how a controller's right
 * stick reaches SDL (SDL's gamepad mapping: the same "right stick" on all three; on macOS a controller may need SDL's
 * mapping database or the system's Game Controller framework to report it, guess) and, for the keyboard, which keys are
 * bound to the C-stick (Settings › Controls; the same bindings file everywhere).
 * For XD: XD's own story mode already lets the right stick do what the left does (snatcherLeo checked it, 2026-11-21); this is
 * Colosseum catching up, nothing to port (likely).
 */
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>

#include "pc_options.h"
#include "pc_pad_shape.h"

extern "C" {
#include "dolphin/types.h"
#include "pc/orre_hooks.h"
}
#include "pc_game_ui.h"

namespace {

constexpr int kMainRadius = 56; /* the game's clamp circles (input.c, inputClampSticks): main stick 56, C-stick 44 */
constexpr int kCRadius = 44;
constexpr u32 kModeOrbitCamera = 5, kModeFreeCamera = 6; /* GSscene_GetMode: the debug cameras */

/* a C-stick value as the main stick's: x 56 / 44 (the two clamp circles), x the walk button's percentage, rounded. Read
 * before the game's clamps (the pad's own range, up to about 127): the game then clamps the copy to the main stick's
 * circle itself, so only a full-range overflow is cut here */
s8 scale_raw(int v, int pct) {
    int s = (v * kMainRadius * pct + (v >= 0 ? kCRadius * 50 : -kCRadius * 50)) / (kCRadius * 100); /* rounded */
    if (s > 127) s = 127;
    if (s < -127) s = -127;
    return (s8)s;
}

/* *Added 2026-11-21 (snatcherLeo, after checking Pokémon XD: "We ensure right stick copies left one exactly, except, in battle
 * mode (and battle now) it will not. So, anything outside story mode, it will go back to default functionality ... for
 * PDA menu, we could turn this off. Everything else, we copy over"):* why the copy is made where the game latches the
 * pad (OrreHook_PadSticks in GSinputRead) and not in each reader: every reader of the main stick (Wes's step, the
 * scripted-line step, every menu's cursor through GSinputGetLeftStick*) then sees it exactly as a main-stick push, with
 * the game's own smoothing and clamping applied after. The C-stick's own values stay as read, so the places that use it
 * themselves still get it where the copy is off. The copy is off (the GameCube's behaviour) in:
 *   - Battle Mode (the top menu's Battle Mode choice or a 0x8AE run: pc_game_ui.battle_mode, the autosave's guard), so
 *     Battle Mode, Battle Now and their menus keep the C-stick's own use;
 *   - (*changed 2026-11-21, snatcherLeo: "in-battle (in story mode) it does not, when I want it too":* story battles are no
 *     longer an exception: the copy moves the battle menus' cursor as the main stick does, and the C-stick itself is
 *     centred there so the game's direct pick doesn't act on the same push; Battle Mode's battles stay excluded by the
 *     line above);
 *   - the P★DA's Pokémon model page (pc_game_ui.pda_model_page): the C-stick spins the model there; *since 2026-11-21*
 *     every page of the P★DA's Pokémon section and the fades between them (the 3.7 closing sweep: a push on the third
 *     page, where the GameCube's C-stick does nothing, was copied and the P★DA read it as a page key, turning back to
 *     the model page; src/game/pc_game_ui.c, pda_model_page, lists the states). The top menu's tabs still copy;
 *   - the debug orbit and free cameras (scene modes 5 and 6), which steer with the C-stick.
 * The main stick wins when both are pushed (the copy is made only while it is centred, after the pad layer's dead zone).
 * The same 56 / 44 scale as before (a full C-stick push = a full main-stick push, so a full push runs), and the walk
 * button (Settings › Controls, applied by the controller layer to the main stick only) scales the copy the same way. */
bool s_wasCopying = false; /* for the log: one line when the copy starts and stops */

void pad_sticks(s32 id, s8* stickX, s8* stickY, s8* substickXp, s8* substickYp) {
    if (id != 1 || !pc_game_ui.ready) return;             /* the player's controller only (pad ids are channel + 1) */
    const s8 substickX = *substickXp, substickY = *substickYp;
    if (*stickX != 0 || *stickY != 0) return;              /* the main stick is pushed: it wins */
    if (substickX == 0 && substickY == 0) {
        if (s_wasCopying) {
            s_wasCopying = false;
            std::printf("[cstick] the right stick let go\n");
        }
        return;
    }
    if (pc_option("cstick_walk") == 0) return;
    const char* off = nullptr;
    if (pc_game_ui.battle_mode != nullptr && pc_game_ui.battle_mode() != 0) off = "Battle Mode";
    else if (const int pda = pc_game_ui.pda_model_page != nullptr ? pc_game_ui.pda_model_page() : 0; pda != 0) {
        off = "the P*DA's Pokemon pages"; /* (2026-11-21: the whole section, not only the model page) */
        static int s_lastPdaState = 0; /* the P★DA's state in the log, once per change: which page held the copy back */
        if (pda != s_lastPdaState) std::printf("[cstick] (P*DA state %d)\n", pda);
        s_lastPdaState = pda;
    } else {
        const u32 mode = pc_game_ui.scene_mode != nullptr ? pc_game_ui.scene_mode() : 0;
        if (mode == kModeOrbitCamera || mode == kModeFreeCamera) off = "a debug camera";
    }
    static const char* s_lastOff = nullptr;
    if (off != nullptr) {
        if (off != s_lastOff) std::printf("[cstick] the right stick keeps its own use: %s\n", off);
        s_lastOff = off;
        s_wasCopying = false;
        return;
    }
    s_lastOff = nullptr;
    const int pct = pc_stick_walk_held() ? pc_stick_walk_pct() : 100;
    *stickX = scale_raw(substickX, pct);
    *stickY = scale_raw(substickY, pct);
    /* *Added 2026-11-21 (snatcherLeo: "in a battle, (in story mode) right stick should work just like left stick"):* in a story
     * battle the C-stick is centred once copied, so the battle menus' own C-stick reader (the direct pick, menuFight.c
     * GSinputGetRightStick*) sees nothing and the push moves the cursor as the main stick does, not both at once.
     * Outside battles it is left as read (the copy alone was checked there, 2026-11-21, and the readers that use it
     * themselves are the excluded places above) */
    const bool inBattle = pc_game_ui.battle_active != nullptr && pc_game_ui.battle_active() != 0;
    if (inBattle) *substickXp = 0, *substickYp = 0;
    if (!s_wasCopying) {
        s_wasCopying = true;
        std::printf("[cstick] the right stick works as the left one (room 0x%X%s; %d,%d -> %d,%d)\n",
                    pc_game_ui.current_room != nullptr ? (unsigned)pc_game_ui.current_room() : 0u,
                    inBattle ? ", a story battle: its own direct pick off" : "", substickX, substickY, *stickX, *stickY);
    }
}

/* ORRE_CSTICK_LOG=1 (test log, observes only): Wes's position every 30 walking reads, whichever stick moves him, so a
 * test can compare the C-stick's speed with the main stick's (distance per frame) */
void test_log(s8 x, s8 y) {
    static int s_on = -1, s_n = 0;
    if (s_on < 0) {
        const char* v = SDL_getenv("ORRE_CSTICK_LOG");
        s_on = v != nullptr && v[0] == '1';
    }
    if (!s_on || pc_game_ui.hero_spot == nullptr || ++s_n % 30 != 0) return;
    uint32_t room = 0;
    float p[3] = {0, 0, 0};
    int32_t facing = 0;
    if (pc_game_ui.hero_spot(&room, p, &facing))
        std::printf("[cstick] read %d: stick %d,%d; Wes at (%.2f, %.2f, %.2f) in room 0x%X\n", s_n, x, y, p[0], p[1], p[2],
                    (unsigned)room);
}

/* OrreHook_WalkStick: only the test log now. *Changed 2026-11-21:* the C-stick no longer walks Wes through this hook;
 * the copy into the main stick (pad_sticks above) walks him like everything else, and stays off in Battle Mode, where
 * this hook used to let the C-stick walk Mt. Battle's course too */
void walk_stick(s8* x, s8* y, s8* rawX, s8* rawY) {
    (void)rawX, (void)rawY;
    test_log(*x, *y);
}

}  // namespace

/* called once at start-up (pc_platform.c); the hooks ask the switch when called, so a Settings change applies at once */
extern "C" void pc_cstick_walk_start(void) {
    OrreHook_WalkStick = walk_stick;
    OrreHook_PadSticks = pad_sticks;
}
