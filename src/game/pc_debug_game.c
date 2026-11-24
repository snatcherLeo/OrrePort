/**
 * @file pc_debug_game.c
 * @brief OrrePort only: the game side of the debug toolkit (2026-10-16; docs/testing-plan.md 4.1, design note 68).
 *
 * Not part of the GameCube build (configure.py doesn't list it); the PC build compiles it with the game units
 * (port/tools/firstlink.py, PORT_GAME_SOURCES). It fills the project's PcDebugApi (port/src/pc_debug.h) with the game's
 * own functions and registers it at start-up. The port side calls these on the game thread at the frame boundary only,
 * and only when the toolkit is on (ORRE_DEBUG=1), so nothing here runs in a normal or GameCube-accurate session.
 * Hook-point rule (the project rules): the game code itself only reads the movie-skip switch below (OrreHook_SkipMovies,
 * docs/port/hook-points.md); everything else here calls existing game functions.
 */
#ifdef TARGET_PC

#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"
#include "../../port/src/pc_debug.h"
#include "../../port/src/pc_cheats.h" /* the quick cheats' game side registers here */
#include <stdlib.h>
#include <string.h>

extern void OSReport(const char*, ...);

/* Game functions (signatures as defined: floor.c, gs_range_8018FE30.c, gs_flag_get_exact_801906A0.c,
 * gs_floor_data.c, gs_math_random_exact_800E0C54.c). */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorLink(u32 room, s32 entry);
#endif
extern void _flagSet(s32 flagId, u32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std082_flagGetValue(s32 flagId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std092_getCurrentRoomID(void);
#endif
extern u32* lbl_80478C94; /* the shared RNG's state (HSD_Rand's seed pointer, docs/engine/rng.md) */
/* Items and Pokémon: signatures in pc/prototypes.h (force-included) */

#define PC_DEBUG_FLAG_COUNT 3593u /* entries in the flag definition table (docs/engine/flags.md) */

/* OrreHook_SkipMovies (decision hook, default vanilla = 0): GSmovieOpen opens no movie while this is set. */
int OrreHook_SkipMovies = 0;

/* OrreHook_FixedRngSeed (decision hook, default vanilla = unset: the game seeds from the clock at boot, fn_800E0C78).
 * Set from ORRE_RNG_SEED=<value> (decimal or 0x hex) for golden-slice comparison runs, so the PC's boot seed equals the
 * Dolphin run's; a test option, separate from GameCube-accurate mode, which keeps the clock seed (snatcherLeo, 2026-10-17). */
int OrreHook_FixedRngSeedSet = 0;
u32 OrreHook_FixedRngSeed = 0;

/* ORRE_CAMERA_LOG=<file> (observe only; golden-slice M29 / M40): the render camera (GSres group 0, id 0) every frame,
 * as CSV in the same columns as the Dolphin camera log (frame, eye, interest, fov), plus the room id. Called once per
 * frame from pc_vi.c's end of frame. */
#include "game/gs_render_util.h"
#include "game/fsys/fsys.h" /* FSYSManager: OrreHook_PortraitDeferRequest reads whether a load is running */
#include "game/input/input.h" /* InputPad: ORRE_EVENT_LOG=2's stick line */
#include <stdio.h>
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource(u32 group, u32 id);
#endif
extern f32 HSD_CObjGetFov(void* cobj);
void pc_camera_log_frame(u32 frame)
{
    static int state = -1;
    static FILE* out;
    GSRenderCamera* cam;
    if (state < 0) {
        const char* path = getenv("ORRE_CAMERA_LOG");
        state = 0;
        if (path != NULL && *path != 0) {
            out = fopen(path, "w");
            if (out != NULL) {
                /* + the retrace count (2026-10-21): Dolphin's camera probe labels its rows by retraces, which the
                 * game frame count falls behind by every retrace the game spent waiting */
                fprintf(out, "frame,eye_x,eye_y,eye_z,int_x,int_y,int_z,fov,room,retrace\n");
                state = 1;
            }
        }
    }
    if (state != 1) {
        return;
    }
    cam = (GSRenderCamera*)GSresGetResource(0, 0);
    if (cam == NULL) {
        fprintf(out, "%u,,,,,,,,%u,%u\n", (unsigned)frame, (unsigned)std092_getCurrentRoomID(),
                (unsigned)VIGetRetraceCount());
        return;
    }
    fprintf(out, "%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u\n", (unsigned)frame, cam->eye.x, cam->eye.y, cam->eye.z,
            cam->interest.x, cam->interest.y, cam->interest.z, cam->cobj != NULL ? HSD_CObjGetFov(cam->cobj) : -1.0f,
            (unsigned)std092_getCurrentRoomID(), (unsigned)VIGetRetraceCount());
    if ((frame & 63) == 0) {
        fflush(out);
    }
}

/* OrreHook_TalkTarget (decision hook, default vanilla: none): one-shot, the person the next A press on the field talks
 * to (updateChat in hero_move.c), for test routes that run every NPC's talk script. Set by pc_debug_game_talk(), which
 * the debug console's `talk GROUP INDEX` command calls (port side, B). Only the target is forced: the talk itself runs
 * the game's normal code (event mode, the talk script with the person's group and index, the end of speech). */
static int s_talk_set;
static u32 s_talk_group;
static u32 s_talk_index;

void* OrreHook_TalkTarget(void)
{
    if (!s_talk_set) {
        return NULL;
    }
    s_talk_set = 0;
    return (void*)peopleIsInGroup(s_talk_group, s_talk_index);
}

int pc_debug_game_talk(unsigned group, unsigned index)
{
    if (peopleIsInGroup(group, index) == NULL) {
        OSReport("[debug] talk: no person %u / %u in this room\n", group, index);
        return 0;
    }
    s_talk_group = group;
    s_talk_index = index;
    s_talk_set = 1;
    OSReport("[debug] talk: the next A press talks to %u / %u\n", group, index);
    return 1;
}

/* OrreHook_BattlePreloadPortraits (decision hook, vanilla default: does nothing), called at the end of the battle's
 * trainer entrance (fightActionFlowKaisiNyuujouTrainer, fight_action.c), one step before the Pokémon entrance whose
 * send-out waits for each lead's portrait (fightPokemonGetFaceSpriteId -> loadPokemonFace). "Smooth battle start" (snatcherLeo 2026-10-18: on
 * by default in the normal preset, off in GameCube-accurate mode; the switch is B's pc_option_smooth_battle_start in
 * port/src/pc_options.c) queues the same portraits here through the game's own queue, without waiting, so the send-out
 * finds them ready. The leads are chosen exactly as the entrance chooses them (per trainer the first two battle
 * Pokémon that can fight, up to the floor's Pokémon count); the key is the one loadPokemonFace computes (the battle
 * Pokémon's status 0xCC, through menuFaceRequestPokemonFace = menuFaceRequestPokemon(pokemonFaceKey(...))). Changes timing, so never in
 * comparison runs (B's switch is off in automated runs unless asked). */
/* The preload's two companions in the portrait queue (gs_range_80109C88.c; vanilla default 0 for both, i.e. off unless
 * the preload is on): a refused load request puts its slot back to "queued", and a send-out waiting on a queued portrait
 * restarts it from a normal frame. */
/* *Retired 2026-11-20 (step 3.7, the project's build helper; docs/port/tweaks-37-research.md 6b):* the "Smooth Party UI"
 * switch no longer runs the background preload below (pc_party_portraits_frame). It read the disc during field time and
 * changed which portraits were cached when the next battle started, and a battle's send-out waits for its portraits
 * while the battle draws random numbers, so it could change the rolls. The switch now drives port/src/pc_party_ui.cpp
 * (the party screen waits for, or holds, its own portraits: nothing loaded that the game wouldn't load then). So this
 * answers 0, and the queue companions below follow Smooth battle start alone */
static int pc_party_preload_on(void)
{
    return 0;
}

/* either portrait preload (smooth battle start, smooth party screen) needs the three queue companions below */
static int pc_preload_active(void)
{
    extern int pc_option_smooth_battle_start(void) __attribute__((weak));
    return (pc_option_smooth_battle_start != 0 && pc_option_smooth_battle_start() != 0) || pc_party_preload_on();
}

int OrreHook_PortraitRequestRefused(u16 key)
{
    if (!pc_preload_active()) {
        return 0;
    }
    OSReport("[options] smooth battle start: portrait 0x%04X's load request was refused (no free slot); re-queued\n",
             (unsigned)key);
    return 1;
}

int OrreHook_PortraitWaitRestart(void)
{
    return pc_preload_active();
}

/* OrreHook_BattleStatusEarly (decision hook, vanilla default 0), battle_main.c after the Pokémon entrance: with smooth
 * battle start on, the full battle display opens as soon as the last Pokémon lands, instead of at the first command
 * (snatcherLeo 2026-10-19: the HP boxes should not vanish for a moment before the battle begins) */
int OrreHook_BattleStatusEarly(void)
{
    extern int pc_option_smooth_battle_start(void) __attribute__((weak));
    /* untested (built 2026-10-19; snatcherLeo moved its test to the next day): only with ORRE_STATUS_EARLY_TEST=1 until a
     * frame capture shows the four boxes appear as the last Pokémon lands, with no blank and no flicker at the command */
    if (getenv("ORRE_STATUS_EARLY_TEST") == 0) {
        return 0;
    }
    return pc_option_smooth_battle_start != 0 && pc_option_smooth_battle_start() != 0;
}

int OrreHook_PortraitDeferRequest(u16 key)
{
    extern FSYSManager lbl_80453FEC; /* gFSYSManager */
    static u16 s_last_deferred;

    if (!pc_preload_active() || lbl_80453FEC.activeSlot == NULL) {
        return 0;
    }
    if (key != s_last_deferred) {
        s_last_deferred = key;
        OSReport("[options] smooth battle start: portrait 0x%04X waits (archive 0x%X is loading)\n", (unsigned)key,
                 (unsigned)lbl_80453FEC.activeSlot->fileHandle);
    }
    return 1;
}

void OrreHook_BattlePreloadPortraits(void* side, u32 trainerCount, u32 pokemonCount)
{
    /* fightSideGetValidFightTrainerPtr, fightTrainerGetStatus, fightPokemonCheckFightOut, pokemonGetStatus: canonical
     * prototypes in pc/prototypes.h (force-included) */
    extern int pc_option_smooth_battle_start(void) __attribute__((weak));
    extern u16 menuFaceRequestPokemonFace(void* pokemon, void* (*callback)(u32), u32 arg); /* gs_range_80109C88.c */
    u32 i;
    u32 j;
    u32 num;

    if (pc_option_smooth_battle_start == 0 || pc_option_smooth_battle_start() == 0 || side == 0) {
        return;
    }
    /* M39 (2026-10-18, fixed 2026-10-19): the third portrait's read reached the file system while the battle's own
     * archive was loading and was parked for good (load mode 3 is never restarted); OrreHook_PortraitDeferRequest now
     * keeps it queued until the disc is idle. The ORRE_PRELOAD_TEST gate is gone: the switch alone controls it. */
    for (i = 0; (u16)i < trainerCount; i++) {
        void* trainer = (void*)PC_PTR(fightSideGetValidFightTrainerPtr(PC_PTR32_IMPLICIT(side), i));
        if (trainer == 0) {
            continue;
        }
        num = 0;
        for (j = 0; (u16)j < 6; j++) {
            void* pokemon;
            if ((u16)num >= pokemonCount || (u16)num >= 2) {
                break;
            }
            pokemon = (void*)fightTrainerGetStatus(trainer, 0, 0x45, j);
            if (pokemon == 0 || (u8)fightPokemonCheckFightOut(pokemon) == 0) { /* as the Pokémon entrance checks it */
                continue;
            }
            OSReport("[options] smooth battle start: preloading portrait for trainer %u slot %u\n", (unsigned)i,
                     (unsigned)j);
            menuFaceRequestPokemonFace((void*)PC_PTR(pokemonGetStatus((u8*)pokemon, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0)), 0, 0);
            num++;
        }
    }
}

/* "Smooth party screen" (snatcherLeo 2026-10-19: its own switch, related to smooth battle start; port/src/pc_options.c
 * smooth_party_screen). Not a hook: port-side glue called once per frame from pc_vi.c's end of frame (game thread),
 * like the camera log. The party screen asks for its six portraits when it opens and they stream in one by one; the
 * portrait cache (24 slots, made once at boot by menuInit, least-recently-used) keeps them afterwards. So while the
 * field is idle (no battle, no archive loading, no portrait loading), this asks for any party portrait not yet in the
 * cache, one per frame, through the game's own queue; the party screen then finds them ready. Each portrait is asked
 * for at most 3 times in a row (then left to the party screen, the console's way). Off: nothing happens. */
void pc_party_portraits_frame(u32 frame)
{
    extern FSYSManager lbl_80453FEC;                /* gFSYSManager */
    extern u8 fightIsBattleActive(void);            /* battle_main.c */
    extern u8 fn_8010B560(void);                    /* a portrait is loading (state 1), gs_range_80109C88.c */
    extern s8 menuFaceIsReady(u16 key);             /* 1: in the cache and ready */
    extern u16 pokemonGetFaceSpriteId(void* pokemon);
    extern u16 menuFaceRequestPokemon(u16 key, void* (*callback)(u32), u32 arg);
    static u16 s_asked[6];
    static u8 s_tries[6];
    u8* save;
    u32 i;

    (void)frame;
    extern s32 lbl_8047AD48;  /* portrait slot count: 0 until menuInit has made the cache at boot */
    extern void* lbl_8047AD4C; /* the portrait slots */

    if (!pc_party_preload_on() || lbl_8047AD48 <= 0 || lbl_8047AD4C == 0) {
        return;
    }
    if (fightIsBattleActive() == 1 || lbl_80453FEC.activeSlot != NULL || fn_8010B560() != 0) {
        return;
    }
    save = (u8*)PC_PTR(savedataGetStatus(0, 2));
    if (save == 0) {
        return;
    }
    for (i = 0; i < 6; i++) {
        void* pokemon = heroBiosGetPokemonPtr(save, (u16)i);
        u16 key;
        if (pokemon == 0 || (u16)pokemonGetStatus((u8*)pokemon, 0, POKEMON_STATUS_SPECIES, 0) == 0) { /* 0x6E: species (none = empty) */
            continue;
        }
        key = pokemonGetFaceSpriteId(pokemon);
        if (key == 0) {
            continue;
        }
        if (menuFaceIsReady(key) == 1) {
            if (s_asked[i] == key) {
                s_tries[i] = 0;
            }
            continue;
        }
        if (s_asked[i] != key) {
            s_asked[i] = key;
            s_tries[i] = 0;
        }
        if (s_tries[i] >= 3) {
            continue;
        }
        s_tries[i]++;
        OSReport("[options] smooth party screen: loading party slot %u's portrait 0x%04X\n", (unsigned)i,
                 (unsigned)key);
        menuFaceRequestPokemon(key, 0, 0);
        return; /* one request per frame; the queue's load chain does the rest */
    }
}

static int dbg_warp(PcDebugU32 room, PcDebugU32 entry)
{
    /* the same call a room script makes for a door or warp (docs/engine/room-changes.md): the new room is
     * requested, the player placed at the entry point from the new room's entry list */
    floorLink(room, (s32)entry);
    return 1;
}

static int dbg_set_flag(PcDebugU32 id, PcDebugU32 value)
{
    if (id >= PC_DEBUG_FLAG_COUNT) {
        return 0;
    }
    _flagSet((s32)id, value);
    return 1;
}

static int dbg_get_flag(PcDebugU32 id, PcDebugU32* value)
{
    if (id >= PC_DEBUG_FLAG_COUNT || value == 0) {
        return 0;
    }
    *value = std082_flagGetValue((s32)id);
    return 1;
}

static int dbg_give_item(PcDebugU32 item, PcDebugU32 count)
{
    /* what the script native scriptAddItem calls to give an item (bag pocket by the item's kind) */
    s32 left;
    if (item == 0 || item > 0xFFFF || count == 0 || count > 999) {
        return 0;
    }
    /* heroItemAddItemDataId returns how many did NOT fit (0 = all added) or -1 (no pocket / invalid id): verified
     * 2026-10-16 from fn_80141308 and a run (item 13 x3 and item 4 x1 both returned 0) */
    left = heroItemAddItemDataId(0, item, count, (u32)-1);
    return left >= 0 && (u32)left < count;
}

/* ---- the quick cheats' game side (port/src/pc_cheats.c; snatcherLeo, 2026-11-13, mockup 63 v2) ----
 * Called on the game thread at the frame boundary (pc_cheats_frame), like the debug commands. */

/* "Put in the PC": tops an item up to `target` in the PC's Item Storage (235 slots, up to 999 each: bag-and-pc.md).
 * A stored item is raised to the target; a new one takes a free slot; nothing changes at the target or above.
 * Returns the count after, -1 if nothing could be added (the storage is full), -2 without a save in memory, -3 if
 * it was already at the target or above. The
 * count is read back through pcboxGetItemCapacity (999 minus the count) rather than trusting pcboxAddItem's return. */
static int cheat_pc_top_up(int item, int target)
{
    /* pcboxDelItem, not pcboxAddItem: the two names are swapped (bag-and-pc.md, "Names swapped": pcboxAddItem calls
     * the slot-removal helper and only finds items already stored; pcboxDelItem calls the add helper, which also takes
     * a free slot). *Corrected 2026-11-13:* the first version called pcboxAddItem and added nothing (verified in a
     * run: "not added"). */
    extern u16 pcboxDelItem(void* base, u16 effect_id, u16 count);
    int have, after;

    if (savedataGetStatus(0, 3) == 0) {
        return -2;
    }
    have = 0x3E7 - (int)pcboxGetItemCapacity(0, (u16)item);
    if (have >= target) {
        return -3; /* already at the target or above: nothing changes */
    }
    pcboxDelItem(0, (u16)item, (u16)(target - have));
    after = 0x3E7 - (int)pcboxGetItemCapacity(0, (u16)item);
    return after > have ? after : -1;
}

/* Infinite money, when switched on: the wallet filled to the game's own cap (heroBiosSetPokedoru keeps it at
 * 9,999,999). Returns the money after, -2 without a save in memory. */
static int cheat_money_fill(void)
{
    extern void heroAddPokedoru(u8* ptr, u32 offset);
    extern u32 heroBiosGetPokedoru(u8* ptr);

    if (savedataGetStatus(0, 2) == 0) {
        return -2;
    }
    heroAddPokedoru(0, 9999999);
    return (int)heroBiosGetPokedoru((u8*)PC_PTR(savedataGetStatus(0, 2)));
}

static int dbg_give_pokemon(PcDebugU32 species, PcDebugU32 level)
{
    /* the gift recipe (hero_pokemon.c, heroPokemonGetPikachu; docs/engine/menu-rooms.md): an attestation stamp from
     * the game data, pokemonCreate, a personality value, the stats recomputed, then heroGetPokemon puts it in the
     * party (or the PC box when the party is full). Trainer details are left as pokemonCreate sets them: a test
     * Pokémon, and the save gets the debug mark later (design note 68). Draws from the RNG (the personality). */
    u8 pokemon[0x13C]; /* the gift functions' buffer size */
    u32 attest;
    u32 rnd;
    u8* hero;

    if (species == 0 || species >= 413 || level == 0 || level > 100) { /* 413 species entries (pokemon-data.md, verified) */
        return 0;
    }
    hero = (u8*)PC_PTR(savedataGetStatus(0, 2));
    if (hero == 0) {
        return 0;
    }
    gamedataAttestCreate(&attest, 9, 3, (u8)gamedataGetStatus(0, 4), (u8)gamedataGetStatus(0, 5));
    pokemonCreate(pokemon, species, level, &attest);
    rnd = pokemonCreateRndFit(pokemon, -1, -1, 0, heroGetStatus(hero, 2, 0));
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_PERSONALITY, 0, rnd);
    pokemonResetBasisStatus(pokemon);
    return heroGetPokemon(hero, pokemon, 1) != 0;
}

/* `givefull SPECIES LEVEL SEX ABILITY M1 M2 M3 M4` (2026-10-18, snatcherLeo's personal save, docs/port/personal-save.md):
 * the starters' own recipe (heroPokemonGetEifie / GetBlacky in hero_pokemon.c) with chosen details, all through game
 * functions: pokemonCreate (level, minimum EXP for it, base friendship, random IVs), the moves with full PP and no PP
 * Ups (pokemonWazaCreate), a personality value from pokemonCreateRndFit with the asked sex (0 male, 1 female, -1 any)
 * and never shiny for the player's id, redrawn until its lowest bit equals the ability slot (the Gen 3 rule, so the
 * value and the ability agree), the ability slot (status 0xB7), the stats recomputed, the player as original trainer
 * with a Poké Ball (ball 4) met here at this level (pokemonSetCatchStatus with what heroCatchPokemon passes; location 0
 * as the developers' own debug add, dbgMenuHeroPokemonAdd), the Strategy Memo entry, then heroCatchPokemon (party, or
 * the PC box when the party is full). Draws from the RNG. */
static int dbg_give_full(u32 species, u32 level, s32 sex, u32 ability, const u32 moves[4])
{
    static u8 pokemon[0x13C]; /* static: the memo keeps a 32-bit view of the record's address */
    u32 attest;
    u32 rnd;
    u32 id;
    u8* hero;
    int i;
    int tries;

    if (species == 0 || species >= 413 || level == 0 || level > 100 || ability > 1 || sex < -1 || sex > 1) {
        return 0;
    }
    hero = (u8*)PC_PTR(savedataGetStatus(0, 2));
    if (hero == 0) {
        return 0;
    }
    id = heroGetStatus(hero, 2, 0);
    gamedataAttestCreate(&attest, 9, 3, (u8)gamedataGetStatus(0, 4), (u8)gamedataGetStatus(0, 5));
    pokemonCreate(pokemon, species, level, &attest);
    for (i = 0; i < 4; i++) {
        pokemonWazaCreate(pokemon, (u32)i, moves[i]);
    }
    tries = 0;
    do {
        rnd = pokemonCreateRndFit(pokemon, (s8)sex, -1, 0, id);
    } while ((rnd & 1u) != ability && ++tries < 64);
    if ((rnd & 1u) != ability) {
        return 0;
    }
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_PERSONALITY, 0, rnd);
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_ABILITY_SLOT, 0, ability);
    pokemonResetBasisStatus(pokemon);
    pokemonSetCatchStatus(pokemon, 0, (u8)level, 4, (u8)heroGetStatus(hero, 0xB, 0), id, heroGetStatus(hero, 1, 0));
    memoDataSet(0, PC_PTR32_IMPLICIT(pokemon));
    i = heroCatchPokemon(hero, pokemon, 0, 4, 1);
    OSReport("[debug] givefull %u Lv %u: PV 0x%08X (nature %u), ability slot %u, %s (result %d)\n", (unsigned)species,
             (unsigned)level, (unsigned)rnd, (unsigned)(rnd % 25), (unsigned)ability,
             i >= 0 ? "in the party" : i == -1 ? "in the PC box" : "no room", i);
    return i != -2;
}

/* `moves PARTYSLOT M1 M2 M3 M4`: replace a party Pokémon's four moves, each with full PP and no PP Ups
 * (pokemonWazaCreate, as the starters get theirs). Party slots count from 0; a value of 6 or more is a species id (the
 * first party Pokémon of that species). */
static int dbg_set_moves(u32 slot, const u32 moves[4])
{
    u8* hero = (u8*)PC_PTR(savedataGetStatus(0, 2));
    u8* pokemon;
    int i;

    if (hero == 0) {
        return 0;
    }
    if (slot > 5) { /* a species id: the first party Pokémon of that species */
        u32 species = slot;
        for (slot = 0; slot < 6; slot++) {
            pokemon = (u8*)heroBiosGetPokemonPtr(hero, (u16)slot);
            if (pokemon != 0 && pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0) == species) {
                break;
            }
        }
        if (slot == 6) {
            return 0;
        }
    }
    pokemon = (u8*)heroBiosGetPokemonPtr(hero, (u16)slot);
    if (pokemon == 0 || pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0) == 0) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        pokemonWazaCreate(pokemon, (u32)i, moves[i]);
    }
    return 1;
}

/* `name TEXT`: the player's name (heroSetStatus 1, the game's own setter, as savedataCreate's heroCreate does), then the
 * original-trainer name (status 0x76) of every party Pokémon whose trainer id (0x75) is the player's, so the starters
 * and the given Pokémon keep matching. ASCII letters only (up to 10). */
static int dbg_set_name(const char* text)
{
    extern void heroSetStatus(u8* ptr, u32 selector, u32 value); /* hero_bios_exact_8012A450.c */
    static u16 name[11]; /* static: the setters take a 32-bit view of the address */
    u8* hero = (u8*)PC_PTR(savedataGetStatus(0, 2));
    u32 id;
    int i;

    if (hero == 0 || text == 0 || text[0] == 0 || strlen(text) > 10) {
        return 0;
    }
    /* the game's strings are UTF-16 code units (docs/engine/messages.md, "String encoding": 0x0041 = "A"), kept in the
     * GameCube's byte order in memory: the name entry stored "WES" as 00 57 00 45 00 53 in snatcherLeo's checkpoint (verified
     * 2026-10-18), so each letter goes in big-endian (GScharMakeFromSJIS gave other codes for plain ASCII bytes) */
    for (i = 0; text[i] != 0; i++) {
        name[i] = (u16)((u8)text[i] << 8);
    }
    name[i] = 0;
    heroSetStatus(hero, 1, PC_PTR32_IMPLICIT(name));
    id = heroGetStatus(hero, 2, 0);
    for (i = 0; i < 6; i++) {
        u8* pokemon = (u8*)heroBiosGetPokemonPtr(hero, (u16)i);
        if (pokemon != 0 && pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0) != 0 && pokemonGetStatus(pokemon, 0, POKEMON_STATUS_OT_ID, 0) == id) {
            pokemonSetStatus(pokemon, 0, POKEMON_STATUS_OT_NAME, 0, heroGetStatus(hero, 1, 0));
        }
    }
    return 1;
}

/* `fightmove INDEX SLOT MOVE [PP]` (2026-10-19; the PC side of the move pass, approved by snatcherLeo): during a battle,
 * sets move SLOT (0-3) of the INDEX-th battler (counted as the battle log lists them; below) to MOVE with its full PP (pokemonWazaCreate, the game's own setter), or to PP when given. It writes the
 * battle's copy of the Pokémon, which the battle reads its moves from (verified in Dolphin 2026-10-19 with
 * PLAYREC_SETMOVE, the Dolphin side); the record's Attest is an origin stamp, not a checksum, so nothing else needs
 * updating. A test aid (ORRE_DEBUG=1, the "!fightmove" pad-script step); use it only while a battle runs. */
static struct {
    u32 want, seen;
    void* work;
} s_fight_pick;
static u8 dbg_fight_pick(void* work, u16 count, void* arg)
{
    (void)count;
    (void)arg;
    if (s_fight_pick.seen++ == s_fight_pick.want) {
        s_fight_pick.work = work;
        return 0; /* found: stop the loop */
    }
    return 1;
}
static int dbg_fight_move(u32 index, u32 slot, u32 move, int pp)
{
    u8* pokemon;

    if (index >= 8 || slot >= 4 || move == 0) {
        return 0;
    }
    /* INDEX counts the valid battlers in the order the game's own loop visits them (fightFloorLoopValidFightOutPokemon
     * with flag 0: Pokémon, then trainer, then side), the order ORRE_BATTLE_LOG's "[battle] mon" lines print. Two
     * earlier versions read the out-Pokémon array (+0xA4C4, selector 0x59) and were refused on the PC while the loop
     * saw all four battlers (2026-10-19) */
    s_fight_pick.want = index;
    s_fight_pick.seen = 0;
    s_fight_pick.work = 0;
    fightFloorLoopValidFightOutPokemon(0, (void*)dbg_fight_pick, 0, 0);
    if (s_fight_pick.work == 0) {
        return 0;
    }
    pokemon = (u8*)fightOutPokemonGetPokemonPtr(s_fight_pick.work);
    if (pokemon == 0) {
        return 0;
    }
    pokemonWazaCreate(pokemon, slot, move);
    if (pp >= 0) {
        pokemon[0x7A + 4 * slot] = (u8)pp; /* PokemonBios +0x78: {u16 move, u8 PP, u8 PP-ups} x4 */
    }
    OSReport("[debug] fightmove: battler %u (species %u) move %u = %u, PP %u\n", (unsigned)index,
             (unsigned)*(u16*)pokemon, (unsigned)slot, (unsigned)*(u16*)(pokemon + 0x78 + 4 * slot),
             (unsigned)pokemon[0x7A + 4 * slot]);
    return 1;
}

/* The game side's own console commands (port/src/pc_debug.c hands over any command it doesn't know): 1 = handled */
int pc_debug_game_command(int argc, char** argv, int* ok)
{
    u32 m[4];
    int i;

    if (strcmp(argv[0], "givefull") == 0) {
        if (argc < 9) {
            OSReport("[debug] givefull: give SPECIES LEVEL SEX(0 male, 1 female, -1 any) ABILITY(0/1) M1 M2 M3 M4\n");
            *ok = 0;
            return 1;
        }
        for (i = 0; i < 4; i++) {
            m[i] = (u32)strtoul(argv[5 + i], 0, 0);
        }
        *ok = dbg_give_full((u32)strtoul(argv[1], 0, 0), (u32)strtoul(argv[2], 0, 0), (s32)strtol(argv[3], 0, 0),
                            (u32)strtoul(argv[4], 0, 0), m);
        return 1;
    }
    if (strcmp(argv[0], "name") == 0) {
        *ok = argc >= 2 && dbg_set_name(argv[1]);
        return 1;
    }
    if (strcmp(argv[0], "bmcourse") == 0) {
        /* "bmcourse N [MODE]" (2026-11-15, language piece 4's stadium header check): Battle Mode's course (save section
         * 14 +0x0C, battle-mode.md: 0 Phenac .. 6 Mt. Battle), the number the VS and results screens' stadium header
         * shows, and optionally the mode (+0x00; 1 = Mt. Battle, whose header is shown alone). Battle Now always plays
         * course 0, so a test run sets it while the VS screen is up to see the other names (the same write a Dolphin
         * script makes for the comparison). Debug toolkit only (ORRE_DEBUG=1); all platforms */
        u8* bm = (u8*)PC_PTR(savedataGetStatus(0, 0xE));
        *ok = argc >= 2 && bm != NULL;
        if (*ok) {
            *(s32*)(bm + 0xC) = (s32)strtol(argv[1], 0, 0);
            if (argc >= 3) {
                *(s32*)(bm + 0x0) = (s32)strtol(argv[2], 0, 0);
            }
        }
        return 1;
    }
    if (strcmp(argv[0], "blink") == 0) {
        /* "blink SLOT:TIMER ..." (2026-10-25, P08's battle 101): a replay resync beside "seed". Sets each named
         * waza-sequence owner's blink timer (+0x20, the retraces since its last blink, which wazaSequenceOwnerUpdateBlink
         * turns into the chance to blink) to the console's value, but only while the owner is idle (+0x19 blink state
         * 0): a random seed doesn't carry these timers, and one owner out of phase changes how many numbers each frame
         * draws, and with it the AI's picks. *ok = every pair was set */
        extern u8* pc_waza_owner(s32 i, s32* count); /* wazaSequenceSys.c: entry i, or NULL */
        s32 n = 0;
        int set = 0;
        for (i = 1; i < argc; i++) {
            char* end;
            unsigned long slot = strtoul(argv[i], &end, 0);
            u8* e = *end == ':' ? pc_waza_owner((s32)slot, &n) : NULL;
            if (e != NULL && e[0x19] == 0) {
                *(u16*)(e + 0x20) = (u16)strtoul(end + 1, 0, 0);
                set++;
            }
        }
        OSReport("[debug] blink: %d of %d owners set\n", set, argc - 1);
        *ok = set == argc - 1;
        return 1;
    }
    if (strcmp(argv[0], "frameat") == 0) {
        /* "frameat K" (2026-10-28, P07's and P09's box screens): a replay resync for the recorded frame lengths
         * (ORRE_REPLAY_FRAMES). The pad writes it at each anchored window opening, K = the console's game frame there,
         * counted in the room's line: the lengths then follow the windows, not the time since the room entry. P07's box
         * visit reached a press 123 retraces later than the console after its entry, so it ran the console's 2-retrace
         * question-window frames while still in the party list, the key-repeat timer counted by 2 and a repeat was lost */
        extern int pc_frame_times_seek(unsigned long frame, unsigned long* was); /* port/src/pc_frame_times.c */
        unsigned long was = 0;
        if (argc < 2) {
            OSReport("[debug] frameat: give the frame index\n");
            *ok = 0;
            return 1;
        }
        *ok = pc_frame_times_seek(strtoul(argv[1], 0, 0), &was);
        OSReport("[debug] frameat %s: %s (was frame %lu)\n", argv[1], *ok ? "ok" : "refused", was);
        return 1;
    }
    if (strcmp(argv[0], "cursor") == 0) {
        /* "cursor ID:POS ..." (2026-10-29, P09's box and Y-menu): a replay resync for menu cursors. Each open
         * window with that id (the window list at winSysWork +0x0C, as ORRE_MENU_LOG reads it) gets the cursor
         * offset the console had (+0x95; the previous value goes to +0x97, as a move leaves it). The pad writes it just
         * before each A press in chosen menu rooms, so a key repeat landing a frame apart can't change what the press
         * picks. *ok = every named window was found open */
        extern u8 winSysWork[];
        int found = 0, changed = 0;
        for (i = 1; i < argc; i++) {
            char* end;
            unsigned long id = strtoul(argv[i], &end, 0);
            u8* w = PTR32_AT_T(u8, winSysWork, 0x0C);
            int g = 0;
            if (*end != ':') {
                continue;
            }
            while (w != NULL && g < 64) {
                if (*(const u32*)(w + 4) == (u32)id) {
                    /* "ID:POS", or "ID:BASE:POS" for a scrolling list (+0x94, the first entry shown) */
                    char* end2;
                    s8 pos = (s8)strtol(end + 1, &end2, 0);
                    if (*end2 == ':') {
                        s8 base = pos;
                        pos = (s8)strtol(end2 + 1, 0, 0);
                        if ((s8)w[0x94] != base) {
                            w[0x94] = (u8)base;
                            changed++;
                        }
                    }
                    found++;
                    if ((s8)w[0x95] != pos) {
                        w[0x97] = w[0x95];
                        w[0x95] = (u8)pos;
                        changed++;
                    }
                    break;
                }
                w = PTR32_AT_T(u8, w, 0x10);
                g++;
            }
        }
        OSReport("[debug] cursor: %d of %d windows found, %d changed\n", found, argc - 1, changed);
        *ok = found == argc - 1;
        return 1;
    }
    if (strcmp(argv[0], "moves") == 0) {
        if (argc < 6) {
            OSReport("[debug] moves: give PARTYSLOT M1 M2 M3 M4\n");
            *ok = 0;
            return 1;
        }
        for (i = 0; i < 4; i++) {
            m[i] = (u32)strtoul(argv[2 + i], 0, 0);
        }
        *ok = dbg_set_moves((u32)strtoul(argv[1], 0, 0), m);
        return 1;
    }
    if (strcmp(argv[0], "fightmove") == 0) {
        if (argc < 4) {
            OSReport("[debug] fightmove: give INDEX SLOT MOVE [PP] (during a battle)\n");
            *ok = 0;
            return 1;
        }
        *ok = dbg_fight_move((u32)strtoul(argv[1], 0, 0), (u32)strtoul(argv[2], 0, 0), (u32)strtoul(argv[3], 0, 0),
                             argc >= 5 ? (int)strtol(argv[4], 0, 0) : -1);
        return 1;
    }
    return 0;
}

static int dbg_set_rng_seed(PcDebugU32 seed)
{
    if (lbl_80478C94 == 0) {
        return 0; /* the generator isn't set up yet */
    }
    *lbl_80478C94 = seed;
    return 1;
}

/* "npc SLOT ..." (2026-10-27): one people slot's wander state from the console's npc capture (a replay resync;
 * the same fields ORRE_NPC_LOG prints). Refused for an empty slot or one without a model */
#include "game/people/people.h"
static int dbg_set_npc(PcDebugU32 slot, const float* v)
{
    extern u32 lbl_8047ACC8; /* the floor context (as in pc_npc_log_frame): no people before the first room */
    PeopleEntry* e;
    f32* p;
    f32* t;

    if (lbl_8047ACC8 == 0 || (s32)slot >= peopleGetMaxCount()) {
        return 0;
    }
    e = peopleGetEntry((s32)slot);
    if (e == NULL || e->modelHandle == NULL) {
        return 0;
    }
    t = (f32*)e->field_5C;
    {   /* through the game's own placement (peoplePlaceAt), so the model moves too: talk and sight checks read the
           model's head joint, which a direct write of the position vector left at the old spot */
        extern void std059_characterSetPosition(u32 groupId, u32 index, f32 x, f32 y, f32 z);
        std059_characterSetPosition(e->groupId, e->index, v[0], v[1], v[2]);
    }
    p = (f32*)peopleGetPositionPtr(e);
    p[0] = v[0];
    p[1] = v[1];
    p[2] = v[2];
    {   /* the previous position (+0x9C, written by peopleMoveCommit), so a standing NPC isn't seen as still elsewhere */
        f32* prev = (f32*)e->transform;
        prev[0] = v[0];
        prev[1] = v[1];
        prev[2] = v[2];
    }
    if (__builtin_isnan(v[3])) {
        return 1; /* "npc SLOT x y z": the position only */
    }
    t[0] = v[3];
    t[1] = v[4];
    t[2] = v[5];
    e->animBlendFactor = v[6];
    e->field_40 = v[7];
    {   /* the model's own rotation too: sight checks (peopleCanSee) read the facing from the model, not +0x40 */
        extern void peopleSetRotation(u32 groupId, u32 index, void* rotation);
        f32 rot[3];
        rot[0] = 0.0f;
        rot[1] = v[7];
        rot[2] = 0.0f;
        peopleSetRotation(e->groupId, e->index, rot);
        e->field_40 = v[7]; /* the console's stored heading as captured (peopleSetRotation normalises it) */
    }
    e->state = (u8)v[8];
    e->subState = (u8)v[9];
    return 1;
}

static int dbg_get_rng_state(PcDebugU32* out)
{
    if (lbl_80478C94 == 0) {
        return 0;
    }
    *out = *lbl_80478C94;
    return 1;
}

static int dbg_skip_movies(int on)
{
    OrreHook_SkipMovies = on != 0;
    return 1;
}

static PcDebugU32 dbg_current_room(void)
{
    return std092_getCurrentRoomID();
}

/* Checkpoints (2026-10-17): a copy of the game's live save block (everything a memory-card save keeps: story
 * flags, team, boxes, bag, money; docs/engine/save-data.md) plus the current room, in checkpoints/slotN.bin next to
 * the working folder. Loading copies the block back and warps to the room's default entrance. Debug only: values the
 * game keeps outside the save block (the room's live objects, timers) are not restored, so a loaded checkpoint is a
 * fast way to reach a spot, not an exact replay. */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define PC_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define PC_MKDIR(p) mkdir(p, 0755)
#endif
extern u32 now_savedata_ptr; /* the current save block (savedataBiosGetNowSavedataPtr) */
#define PC_CHECKPOINT_MAGIC 0x3150434Fu /* "OCP1" */
#define PC_CHECKPOINT_BYTES 0x1DFD0u   /* the game save block (save-data.md, "Save slot" +0x08) */

/* port/src/pc_platform.c: the checkpoints' folder, no trailing separator (System\checkpoints in the portable layout,
 * <exe folder>/checkpoints in the flat one; pc_paths.h, 4.1, 2026-11-21) */
extern const char* pc_platform_checkpoints_dir(char* out, size_t size);

static void checkpoint_path(char* out, size_t size, int slot)
{
    char dir[1024];

    snprintf(out, size, "%s/slot%d.bin", pc_platform_checkpoints_dir(dir, sizeof(dir)), slot);
}

static int dbg_checkpoint_save(int slot)
{
    u8* block = (u8*)PC_PTR(now_savedata_ptr);
    u32 header[3];
    char path[1100];
    FILE* f;

    if (block == NULL || slot < 0 || slot > 99) {
        return 0;
    }
    {
        char dir[1024];
        PC_MKDIR(pc_platform_checkpoints_dir(dir, sizeof(dir)));
    }
    checkpoint_path(path, sizeof(path), slot);
    f = fopen(path, "wb");
    if (f == NULL) {
        return 0;
    }
    header[0] = PC_CHECKPOINT_MAGIC;
    header[1] = std092_getCurrentRoomID();
    header[2] = PC_CHECKPOINT_BYTES;
    fwrite(header, sizeof(header), 1, f);
    fwrite(block, PC_CHECKPOINT_BYTES, 1, f);
    fclose(f);
    OSReport("[debug] checkpoint %d saved: room 0x%X\n", slot, (unsigned)header[1]);
    return 1;
}

static int dbg_checkpoint_load(int slot)
{
    u8* block = (u8*)PC_PTR(now_savedata_ptr);
    u32 header[3];
    char path[1100];
    FILE* f;
    int ok;

    if (block == NULL || slot < 0 || slot > 99) {
        return 0;
    }
    {
        /* only from the title (0x384, B's supported load point: it waits for the title's sound groups) or a field room
         * (class 0x20, as for `fight`): in a menu screen the open windows outlive the load and read the new memory
         * (snatcherLeo, 2026-10-17: F9 with the name list open crashed in menuNameEntrySelectDrawSelText3) */
        extern void* floorDataBiosGetPtr(u32 key);
        extern u8 floorDataBiosGetFloorKind(u8* ptr);
        u32 roomId = std092_getCurrentRoomID();
        u8* room = (u8*)floorDataBiosGetPtr(roomId);
        if (roomId != 0x384 && (room == NULL || floorDataBiosGetFloorKind(room) != 1)) {
            OSReport("[debug] checkpoint load refused in room 0x%X: load from the title or while walking in a room\n",
                     (unsigned)roomId);
            return 0;
        }
    }
    checkpoint_path(path, sizeof(path), slot);
    f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    ok = fread(header, sizeof(header), 1, f) == 1 && header[0] == PC_CHECKPOINT_MAGIC &&
         header[2] == PC_CHECKPOINT_BYTES && fread(block, PC_CHECKPOINT_BYTES, 1, f) == 1;
    fclose(f);
    if (!ok) {
        return 0;
    }
    OSReport("[debug] checkpoint %d loaded: warping to room 0x%X\n", slot, (unsigned)header[1]);
    floorLink(header[1], 0);
    return 1;
}

/* Golden-slice room log for the slice_compare.py (ORRE_SLICE_LOG=1, off by default; observes only). Called by the port's
 * room hook (pc_game_main.c) with the frame count: "[slice] entered 0x<room> frame=<n> rng=0x<hex> flags=0x<crc32>",
 * the CRC-32 (zlib's) over flag classes 0-3 in order, each word big-endian, matching dolphin_slice.py. */
static u32 slice_crc32(u32 crc, const u8* p, u32 n)
{
    u32 k;

    while (n--) {
        crc ^= *p++;
        for (k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

void pc_slice_log_room(unsigned room, unsigned frame)
{
    typedef struct { u32 wordCount; u32 buffer; } SliceFlagState; /* FlagStateEntry (gs_flag_exact_801908D4.c) */
    extern void* lbl_80478EEC; /* the four flag classes (lbl_80478C94, the RNG state pointer, is declared above) */
    static int enabled = -1;
    SliceFlagState* states;
    u32 crc = 0xFFFFFFFFu;
    u32 c, w;

    {
        /* ORRE_RELPTR_LOG=1 (2026-10-26; observes only): main-program pointers into big-endian module data,
         * listed at each room entry for the byte-order sweep (pc_rel_scan_main_pointers, generated by pcreldata.py) */
        static int relptr = -1;
        if (relptr < 0) {
            const char* env = getenv("ORRE_RELPTR_LOG");
            relptr = env != 0 ? atoi(env) : 0;
        }
        if (relptr) {
            extern void pc_rel_scan_main_pointers(unsigned room);
            pc_rel_scan_main_pointers(room);
        }
        if (relptr >= 2) { /* =2: also every word of the exe's data and bss (copies of those pointers) */
            extern void pc_rel_scan_globals(unsigned room);
            pc_rel_scan_globals(room);
        }
        if (relptr >= 3) {
            /* =3: every bss word pointing into the exe's data section, as "[bssptr] pc,value" (each pair once). Matched
             * offline against the unsure tables kept big-endian (build-pc/generated/unsure_data.txt + nm): runtime
             * copies of pointers to them, which a search of the code that names a table can't see */
            extern char __data_start__[], __data_end__[], __bss_start__[], __bss_end__[];
            static u32 seen_at[32768], seen_val[32768];
            static unsigned nseen;
            const u8* p = (const u8*)(((uintptr_t)__bss_start__ + 3) & ~(uintptr_t)3);
            const u32 lo = (u32)(uintptr_t)__data_start__, hi = (u32)(uintptr_t)__data_end__;
            for (; p + 4 <= (const u8*)__bss_end__ && nseen < 32768; p += 4) {
                u32 v, at = (u32)(uintptr_t)p;
                unsigned k;
                if ((p >= (const u8*)seen_at && p < (const u8*)(seen_at + 32768)) ||
                    (p >= (const u8*)seen_val && p < (const u8*)(seen_val + 32768))) {
                    continue; /* this scan's own bookkeeping */
                }
                memcpy(&v, p, 4);
                if (v < lo || v >= hi) {
                    continue;
                }
                for (k = 0; k < nseen && !(seen_at[k] == at && seen_val[k] == v); k++) {
                }
                if (k < nseen) {
                    continue;
                }
                seen_at[nseen] = at;
                seen_val[nseen++] = v;
                OSReport("[bssptr] %08X,%08X\n", (unsigned)at, (unsigned)v);
            }
        }
    }
    if (enabled < 0) {
        enabled = getenv("ORRE_SLICE_LOG") != 0;
    }
    if (!enabled) {
        return;
    }
    states = (SliceFlagState*)lbl_80478EEC;
    for (c = 0; states != NULL && c < 4; c++) {
        const u32* words = (const u32*)PC_PTR(states[c].buffer);
        for (w = 0; words != NULL && w < states[c].wordCount; w++) {
            u8 be[4];
            be[0] = (u8)(words[w] >> 24);
            be[1] = (u8)(words[w] >> 16);
            be[2] = (u8)(words[w] >> 8);
            be[3] = (u8)words[w];
            crc = slice_crc32(crc, be, 4);
        }
    }
    OSReport("[slice] entered 0x%X frame=%u rng=0x%08X flags=0x%08X\n", room, frame,
             lbl_80478C94 ? (unsigned)*lbl_80478C94 : 0u, (unsigned)~crc);
}

/* Called once per frame from pc_vi.c (before the pad script's frame): when the player (people group 0, index 100, as
 * the Dolphin recorder's player_pos reads it) appears after being absent, tells the pad script, whose "@0x1<room>+N"
 * steps count from that moment. Only while a pad script runs; observes only. */
void pc_hero_watch_frame(u32 frame)
{
    extern void pc_pad_script_hero_appeared(unsigned room, unsigned frame); /* port/src/pc_pad_script.c */
    extern u8 fightIsBattleActive(void);
    static int enabled = -1;
    static int present;
    struct PeopleEntry* hero;
    int now;

    if (enabled < 0) {
        enabled = getenv("ORRE_PAD_SCRIPT") != 0;
    }
    if (!enabled) {
        return;
    }
    hero = fightIsBattleActive() == 1 ? NULL : peopleIsInGroup(0, 100);
    now = hero != NULL && peopleGetPositionPtr(hero) != NULL;
    if (now && !present) {
        pc_pad_script_hero_appeared((unsigned)std092_getCurrentRoomID(), (unsigned)frame);
    }
    present = now;
    {
        /* the pad script's "@0x3<room>" anchors (2026-10-26): a fade-in finishing (fade.c FadeState, _fadeWork:
         * +1 pending goes 1 -> 0 while +2 type is 2, the same transition dolphin_playrec.py's fade probe records);
         * observes only */
        extern void pc_pad_script_fadein_done(unsigned room, unsigned frame); /* port/src/pc_pad_script.c */
        extern volatile u8 _fadeWork[];
        static int last_pending = -1;
        u16 type;
        int pending = _fadeWork[1];
        memcpy(&type, (const void*)(_fadeWork + 2), 2);
        if (last_pending == 1 && pending == 0 && type == 2) {
            unsigned room = (unsigned)std092_getCurrentRoomID();
            if (room != 0) { /* the boot fade, before any room: no "@0x3" step waits for it */
                pc_pad_script_fadein_done(room, (unsigned)frame);
            }
        }
        last_pending = pending;
    }
}

/* ORRE_NPC_LOG=<room hex or *>[,<from>-<to>] (2026-10-20; observes only): every people entry, every frame the room is
 * current (or only frames from..to, as the pad script counts them), as "[npc] " + the CSV row of dolphin_playrec.py's
 * npc probe (PLAYREC_NPC group -1), so one script compares both sides. room_frame counts from the first frame the room
 * is current. Columns: frame, room_frame, slot, active, index, state, sub, move (moveSpeed), walk target x/y/z (+0x5C),
 * home x / z (+0x74 / +0x7C), radius (+0x80), pause (+0x84), yaw (+0x40), position x/y/z (peopleGetPositionPtr; empty
 * without a model), group. P02's Drig walk: did a wandering NPC block Wes? */
#include "game/people/people.h"
static void pc_npc_log_frame(u32 frame)
{
    static int enabled = -1;
    static u32 room, from, to;
    static s32 roomStart = -1;
    s32 i, n;

    if (enabled < 0) {
        const char* env = getenv("ORRE_NPC_LOG");
        enabled = 0;
        if (env != 0 && *env != '\0') {
            char* end;
            /* "*" for any room (2026-10-21: the playrec probe's room is the last one entered, and P02's second
             * Phenac talk came while the game's current room was another id, so the log stayed empty) */
            room = *env == '*' ? 0xFFFFFFFFu : (u32)strtoul(env, &end, 16);
            if (*env == '*') {
                end = (char*)env + 1;
            }
            from = 0;
            to = 0xFFFFFFFFu;
            if (*end == ',') {
                from = (u32)strtoul(end + 1, &end, 10);
                to = *end == '-' ? (u32)strtoul(end + 1, 0, 10) : 0xFFFFFFFFu;
            }
            enabled = 1;
            OSReport("[npc] frame,room_frame,slot,active,index,state,sub,move,tx,ty,tz,hx,hz,radius,pause,yaw,x,y,z,"
                     "group\n");
        }
    }
    {
        /* the floor context is null until the first room exists (2026-10-21: std092_getCurrentRoomID read it at
         * frame 1, inside GSgfxInit, and crashed; only with ORRE_NPC_LOG set) */
        extern u32 lbl_8047ACC8; /* GSFloorContext* (gs_floor.c); as u32 like gs_floor_data.c: non-zero once set */
        if (!enabled || lbl_8047ACC8 == 0 || (room != 0xFFFFFFFFu && std092_getCurrentRoomID() != room)) {
            return;
        }
    }
    if (roomStart < 0) {
        roomStart = (s32)frame;
    }
    if (frame < from || frame > to) {
        return;
    }
    n = peopleGetMaxCount();
    for (i = 0; i < n && i < 512; i++) {
        PeopleEntry* e = peopleGetEntry(i);
        const f32* p;
        const f32* t;
        if (e == NULL) {
            continue;
        }
        p = e->modelHandle != NULL ? (const f32*)peopleGetPositionPtr(e) : NULL;
        t = (const f32*)e->field_5C;
        if (p != NULL) {
            OSReport("[npc] %u,%d,%d,%u,%u,%u,%u,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%u\n", (unsigned)frame,
                     (int)frame - roomStart, (int)i, (unsigned)e->active, (unsigned)e->index, (unsigned)e->state,
                     (unsigned)e->subState, (double)e->moveSpeed, (double)t[0], (double)t[1], (double)t[2],
                     (double)e->collisionX, (double)e->collisionZ, (double)e->field_80, (double)e->animBlendFactor,
                     (double)e->field_40, (double)p[0], (double)p[1], (double)p[2], (unsigned)e->groupId);
        } else {
            OSReport("[npc] %u,%d,%d,%u,%u,%u,%u,%g,%g,%g,%g,%g,%g,%g,%g,%g,,,,%u\n", (unsigned)frame,
                     (int)frame - roomStart, (int)i, (unsigned)e->active, (unsigned)e->index, (unsigned)e->state,
                     (unsigned)e->subState, (double)e->moveSpeed, (double)t[0], (double)t[1], (double)t[2],
                     (double)e->collisionX, (double)e->collisionZ, (double)e->field_80, (double)e->animBlendFactor,
                     (double)e->field_40, (unsigned)e->groupId);
        }
    }
}

/* ORRE_MSG_LOG=1 (2026-10-21; observes only): the field text box page by page, as "[msg] " + the CSV row of
 * dolphin_playrec.py's fieldmsg probe: frame, the field message window's (menu 0x40) phase, its +0x98 "finished" and
 * +0x99 bytes, the message key (its work area's first word) and that GSmsg entry's 0x68 bytes as 26 words (the entry
 * table _pMw: u16 count, +0x20 the entries, key at +0x1C; GSmsgIsCheck). One row whenever any of it changes.
 * The words are printed as the PC holds them, so a byte field shows in another place of its word than in Dolphin's
 * big-endian row: compare when a word changes, not its digits. Sitting 9's Phenac arrival: the PC closed the first
 * conversation 27 frames late. */
#include "pc/ptr32.h"
struct WindowWork;
static void pc_msg_log_frame(u32 frame)
{
    extern void* windowSearchID();
    extern void* windowGetFreeWork(struct WindowWork* window);
    extern void* _pMw;
    static int enabled = -1;
    static u32 msgWindow = 0x40;
    static char last[400];
    char row[400];
    u8* window;
    int len;

    if (enabled < 0) {
        const char* env = getenv("ORRE_MSG_LOG");
        enabled = env != 0 ? atoi(env) : 0;
        /* ORRE_MSG_WINDOW=<id> (2026-10-29): another message window than the field box 0x40, such as the menu
         * message 0x10 (P09: the item-use result after Miror B. that the PC's A doesn't close); the fieldmsg probe's
         * PLAYREC_MSGWINDOW is the same */
        env = getenv("ORRE_MSG_WINDOW");
        if (env != 0 && *env != '\0') {
            msgWindow = (u32)strtoul(env, 0, 0);
        }
        if (enabled) {
            OSReport("[msg] frame,phase,finished,b99,key,entry (window 0x%x)\n", (unsigned)msgWindow);
        }
    }
    if (!enabled) {
        return;
    }
    window = (u8*)windowSearchID(msgWindow);
    if (window == NULL) {
        strcpy(row, ",,,,");
    } else {
        u32 key = *(u32*)windowGetFreeWork((struct WindowWork*)window);
        u8* head = (u8*)_pMw;
        len = snprintf(row, sizeof(row), "%d,%u,%u,0x%x,", (int)(s8)window[1], (unsigned)window[0x98],
                       (unsigned)window[0x99], (unsigned)key);
        if (head != NULL) {
            u8* base = PTR32_AT_T(u8, head, 0x20);
            u32 i, n = *(u16*)head;
            for (i = 0; base != NULL && i < n && i < 64; i++) {
                u8* e = base + i * 0x68;
                if (e[0] != 0 && *(u32*)(e + 0x1C) == key) {
                    u32 o;
                    for (o = 0; o < 0x68 && len < (int)sizeof(row) - 9; o += 4) {
                        len += snprintf(row + len, sizeof(row) - (size_t)len, "%08x", (unsigned)*(u32*)(e + o));
                    }
                    break;
                }
            }
        }
        if (enabled == 2 && len < (int)sizeof(row) - 64) {
            /* ORRE_MSG_LOG=2: also the window's flags byte (bit 0x04 lets the text advance: win_msg.c winMsgDraw), the
             * frame step and the window's address (for ORRE_WATCH), after the entry */
            extern u32 GSgetFrameStep(void);
            snprintf(row + len, sizeof(row) - (size_t)len, " flags=0x%02x step=%u at=%p", (unsigned)window[0],
                     (unsigned)GSgetFrameStep(), (void*)window);
        }
    }
    if (strcmp(row, last) != 0) {
        OSReport("[msg] %u,%s\n", (unsigned)frame, row);
        strcpy(last, row);
    }
}

/* ORRE_CAMPARAMS_LOG=1 (2026-10-21; observes only): the move camera's computed parameters (lbl_804673A0,
 * wazaSequenceCamera.c WazaSequenceCameraParams: motion, then 12 floats), "[camp] " + the CSV row of
 * dolphin_playrec.py's camparams probe, when any changes. snatcherLeo: the battle cameras differ (random shots) */
static void pc_camparams_log_frame(u32 frame)
{
    extern u8 lbl_804673A0[];
    static int enabled = -1;
    static u32 last[13];
    u32 now[13];
    const f32* f = (const f32*)lbl_804673A0;
    s32 k;

    if (enabled < 0) {
        enabled = getenv("ORRE_CAMPARAMS_LOG") != NULL;
    }
    if (!enabled) {
        return;
    }
    memcpy(now, lbl_804673A0, sizeof(now));
    if (memcmp(now, last, sizeof(now)) == 0) {
        return;
    }
    memcpy(last, now, sizeof(now));
    {
        extern u32 lbl_8047ACC8; /* the floor context: null before the first room (see pc_npc_log_frame) */
        char row[400];
        int len = snprintf(row, sizeof(row), "%u,%u,%d", (unsigned)frame,
                           lbl_8047ACC8 != 0 ? (unsigned)std092_getCurrentRoomID() : 0u,
                           (int)*(const s32*)lbl_804673A0);
        for (k = 1; k < 13 && len < (int)sizeof(row) - 16; k++) {
            len += snprintf(row + len, sizeof(row) - (size_t)len, ",%.4f", (double)f[k]);
        }
        OSReport("[camp] %s\n", row);
    }
}

/* ORRE_FADE_LOG=1 (2026-10-20; observes only): the screen fade's state (fade.c FadeState: mode, pending, type,
 * duration, elapsed), a "[fade_state] frame,retrace,mode,pending,type,duration,elapsed" row whenever it changes; the
 * same columns as dolphin_playrec.py's fade probe (floats as %.9g). Wes's name screen leaves a frame early on the PC */
static void pc_fade_log_frame(u32 frame)
{
    extern volatile u8 _fadeWork[]; /* FadeState; its first 0x0C bytes have the GameCube layout on the PC too */
    static int enabled = -1;
    static char last[128];
    char row[128];
    f32 duration, elapsed;
    u16 type;
    if (enabled < 0) {
        const char* env = getenv("ORRE_FADE_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled) {
        return;
    }
    memcpy(&type, (const void*)(_fadeWork + 2), 2);
    memcpy(&duration, (const void*)(_fadeWork + 4), 4);
    memcpy(&elapsed, (const void*)(_fadeWork + 8), 4);
    snprintf(row, sizeof(row), "%u,%u,%u,%.9g,%.9g", (unsigned)_fadeWork[0], (unsigned)_fadeWork[1],
             (unsigned)type, (double)duration, (double)elapsed);
    if (strcmp(row, last) == 0) {
        return;
    }
    strcpy(last, row);
    OSReport("[fade_state] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), row);
}

/* ORRE_SOUND_LOG=1 (2026-10-20; observes only): the two sound slots soundPlay keeps (lbl_8047B0A8 = the last
 * sound effect it started, lbl_8047B0AC = the current music), a "[sound_slots] frame,retrace,effect,music" row whenever
 * either changes; the same pair as dolphin_playrec.py's "sound" event, compared by sound_compare.py */
static void pc_sound_log_frame(u32 frame)
{
    extern u32 lbl_8047B0A8;
    extern u32 lbl_8047B0AC;
    static int enabled = -1;
    static int have;
    static u32 lastA, lastB;
    if (enabled < 0) {
        const char* env = getenv("ORRE_SOUND_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled || (have && lbl_8047B0A8 == lastA && lbl_8047B0AC == lastB)) {
        return;
    }
    have = 1;
    lastA = lbl_8047B0A8;
    lastB = lbl_8047B0AC;
    OSReport("[sound_slots] %u,%u,%u,%u\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), (unsigned)lastA,
             (unsigned)lastB);
}

/* ORRE_WIN_LOG=1 (2026-10-20; observes only): every open menu window as "id:phase" (the window list from
 * winSysWork +0x0C, next at +0x10, phase at +0x01, id at +0x04: the same offsets on the PC, 32-bit slots), a
 * "[win] frame,retrace,list" row whenever the list changes; the same row as dolphin_playrec.py's win probe. Wes's name
 * screen: the name list's close animation ends a frame sooner on the PC */
static void pc_win_log_frame(u32 frame)
{
    extern u8 winSysWork[];
    static int enabled = -1;
    static char last[512];
    char row[512];
    int len = 0, n = 0;
    u8* node;
    if (enabled < 0) {
        const char* env = getenv("ORRE_WIN_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled) {
        return;
    }
    row[0] = '\0';
    for (node = PTR32_AT_T(u8, winSysWork, 0x0C); node != NULL && n < 32 && len < (int)sizeof(row) - 24;
         node = PTR32_AT_T(u8, node, 0x10), n++) {
        len += snprintf(row + len, sizeof(row) - (size_t)len, "%s%x:%d", len ? " " : "", (unsigned)*(u32*)(node + 4),
                        (int)(s8)node[1]);
    }
    if (strcmp(row, last) == 0) {
        return;
    }
    strcpy(last, row);
    OSReport("[win] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), row);
}

/* ORRE_FLOOR_LOG=1 (2026-10-25; observes only): the room manager's state (lbl_8047ACD8), the current room
 * context's resource step (isActive, fn_80100B24's phases 1-6) and the fade's pending flag, a "[floor]
 * frame,retrace,state,step,fade" row whenever one changes; the same row as dolphin_playrec.py's floor probe. P01's
 * Folly battle: the room change after the battle starts a frame later on the PC */
static void pc_floor_log_frame(u32 frame)
{
    extern u32 lbl_8047ACD8;
    extern long pc_floor_step(void); /* gs_floor.c: the current context's isActive, or -1 */
    extern s8 fadeCheck(u8 wait);    /* fadeCheck(0): the fade's pending flag, no wait */
    static int enabled = -1;
    static long last_state = -1, last_step = -1, last_fade = -1;
    long step, fade;
    if (enabled < 0) {
        const char* env = getenv("ORRE_FLOOR_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled) {
        return;
    }
    step = pc_floor_step();
    fade = (long)fadeCheck(0);
    if ((long)lbl_8047ACD8 == last_state && step == last_step && fade == last_fade) {
        return;
    }
    last_state = (long)lbl_8047ACD8;
    last_step = step;
    last_fade = fade;
    OSReport("[floor] %u,%u,%ld,%ld,%ld\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), last_state, step, fade);
}

/* ORRE_VID_LOG=1 (2026-10-25; observes only): MusyX's live voice ids at the end of every frame, a
 * "[vids] frame,retrace,added,removed" row when the set changes (hex, space-separated); the same row as
 * dolphin_playrec.py's vids probe. sndFXCheck(vid) says "playing" while the vid is listed */
static void pc_vid_log_frame(u32 frame)
{
    extern int pc_musyx_live_vids(u32* out, int max); /* src/musyx/runtime/synthvoice.c */
    static int enabled = -1;
    static u32 old[512];
    static int nold;
    u32 now[512];
    char added[2048], removed[2048];
    int n, i, j, la = 0, lr = 0;
    if (enabled < 0) {
        const char* env = getenv("ORRE_VID_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled) {
        return;
    }
    n = pc_musyx_live_vids(now, 512);
    added[0] = removed[0] = '\0';
    for (i = 0; i < n; i++) {
        for (j = 0; j < nold && old[j] != now[i]; j++) {
        }
        if (j == nold && la < (int)sizeof(added) - 12) {
            la += snprintf(added + la, sizeof(added) - (size_t)la, "%s%x", la ? " " : "", (unsigned)now[i]);
        }
    }
    for (j = 0; j < nold; j++) {
        for (i = 0; i < n && now[i] != old[j]; i++) {
        }
        if (i == n && lr < (int)sizeof(removed) - 12) {
            lr += snprintf(removed + lr, sizeof(removed) - (size_t)lr, "%s%x", lr ? " " : "", (unsigned)old[j]);
        }
    }
    if (la || lr) {
        OSReport("[vids] %u,%u,%s,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), added, removed);
    }
    for (i = 0; i < n; i++) {
        old[i] = now[i];
    }
    nold = n;
}

/* ORRE_OWNERS_LOG=<from>-<to> (2026-10-25; observes only): the battle's waza-sequence owners every frame in that
 * range, "[owners] frame,retrace,slot,index,motion_busy,flags,blink_state,blink_timer,blink_texanim,anim_active,
 * playing,seq_time,seq_14,seq_15", the columns of dolphin_playrec.py's owners probe (without its room_frame and model). P01's Folly battle: a trainer's blink
 * ends a frame apart from the console's (wazaSequenceOwnerUpdateBlink draws once per idle owner per frame) */
/* ORRE_ROLL_SEEDS=@file:<path> (2026-10-27; replays only): the battle-roll resync per draw. The file holds, per
 * battle ("battle N": the Nth battle thread since power-on), the console's random state just before each draw its
 * battle thread made (`roll_resync_spec.py --draws`). Every draw the PC's battle thread makes takes the next state
 * first, so each accuracy, damage, critical or AI roll gets the console's number however the idle blinks (main-thread
 * draws that drift by a frame) have run since. A frame-scheduled seed can't do that once the battle's frames have
 * drifted by one (P08 battle 101's Supersonic roll, 2 frames late on the PC). A second list per battle ("idle N")
 * does the same for the idle camera's draws (battleCameraStartRandom, marked by fn_801EF488 on the PC): its 50/50
 * roll decides whether an idle shot starts, and a shot changes the motion the next move camera may pick, so the
 * battle thread's draw count (P08: the PC's idle shot at its first menu, the console's roll said skip). A third
 * list ("camera N", 2026-10-28) holds the move camera's draws on the battle thread: some procedural shots take
 * another number of draws on the PC (P07's battle 171), and kept apart they can't shift the battle rolls' list;
 * without a camera list the camera draws use the battle list as before. Through the hook OrreHook_RngBeforeDraw
 * (src/hsd/random.c; docs/port/hook-points.md): unset, the game's own sequence */
void (*OrreHook_RngBeforeDraw)(u32* state);
s32 pc_idle_camera_depth; /* battle_main.c's fn_801EF488: inside battleCameraStartRandom */
s32 pc_move_camera_depth; /* wazaSequence_exact_801DBB10.c: inside a move's battleCameraStartWaza */
s32 pc_anim_particle_depth; /* field_range_80117E58.c: inside gsParticlePtclHook's gsParticleCreate (a model
                             * animation's particle, on whichever thread animates the model; port-gaps 49, P24) */
s32 pc_battle_serial;     /* battle_main.c: battles started since power-on (the battle thread's creation) */
extern u32 _activeThread; /* the GS thread running now (gs_thread_exact_800F0424.c) */
extern u32 lbl_8047B5D0; /* the battle thread (battle_main.c), 0 outside a battle */
static const char* const s_rs_kind[3] = {"draw", "idle", "camera"};
static u32** s_rs_states[3]; /* [0] the battle thread's rolls, [1] the idle camera, [2] the move camera; per battle */
static s32* s_rs_count[3];
static s32 s_rs_battles;     /* entries in the arrays */
static s32 s_rs_battle, s_rs_next[3]; /* the battle running now (1-based, 0 before the first) and its next states */
static u32 s_rs_seen;        /* the battle thread handle at the last look */

static void pc_rollseed_track(void)
{
    /* a new battle: the serial the battle thread's creation bumps (the handle stays set between battles,
     * 2026-10-28: P07's second round kept drawing from the first round's list) */
    u32 thr = lbl_8047B5D0;
    if (pc_battle_serial != s_rs_battle) {
        s_rs_battle = pc_battle_serial;
        s_rs_next[0] = s_rs_next[1] = s_rs_next[2] = 0;
        if (s_rs_battle < s_rs_battles &&
            s_rs_count[0][s_rs_battle] + s_rs_count[1][s_rs_battle] + s_rs_count[2][s_rs_battle] > 0) {
            OSReport("[rollseed] battle %d starts, %d + %d idle + %d camera states\n", (int)s_rs_battle,
                     (int)s_rs_count[0][s_rs_battle], (int)s_rs_count[1][s_rs_battle],
                     (int)s_rs_count[2][s_rs_battle]);
        }
    }
    s_rs_seen = thr;
}

static void pc_rollseed_before_draw(u32* state)
{
    s32 k;
    pc_rollseed_track();
    if (s_rs_seen == 0 || s_rs_battle >= s_rs_battles || pc_anim_particle_depth > 0) {
        return; /* (an animation's particle: in no list, as roll_resync_spec.py leaves it out; 2026-11-02) */
    }
    if (pc_move_camera_depth > 0 && s_rs_count[2][s_rs_battle] > 0 &&
        (_activeThread == s_rs_seen || pc_idle_camera_depth > 0)) {
        k = 2; /* a move's camera, or an idle shot's (2026-10-28: an idle shot's count also differs, P07 battle 171) */
    } else if (_activeThread == s_rs_seen) {
        k = 0;
    } else if (pc_idle_camera_depth > 0) {
        k = 1;
    } else {
        return;
    }
    if (s_rs_next[k] < s_rs_count[k][s_rs_battle]) {
        u32 want = s_rs_states[k][s_rs_battle][s_rs_next[k]++];
        if (*state != want) {
            OSReport("[rollseed] battle %d %s %d: %08X -> %08X\n", (int)s_rs_battle, s_rs_kind[k],
                     (int)s_rs_next[k], (unsigned)*state, (unsigned)want);
        }
        *state = want;
    } else if (s_rs_next[k]++ == s_rs_count[k][s_rs_battle] && s_rs_count[k][s_rs_battle] > 0) {
        OSReport("[rollseed] battle %d %s draws past the console's %d\n", (int)s_rs_battle, s_rs_kind[k],
                 (int)s_rs_count[k][s_rs_battle]);
    }
}

/* ORRE_ROLL_RECORD=<file> (2026-11-02; observes only; the other direction of ORRE_ROLL_SEEDS, for the reference playthrough's
 * Dolphin referee): during a live sitting, the state before every battle-thread draw, sorted into the same three lists
 * (battle rolls, idle camera, move camera; an animation's particles in none), one line per draw:
 * "<battle|idle|camera> <battle N> <index in that list> <state hex> <game frame> <state after>", plus every draw made
 * while a trainer or Pokémon is created or a trainer's team is sorted, any thread, as "create <battle N, or 0 outside
 * battles> ...". A Dolphin replay of the sitting's
 * converted movie can then give the console the PC's numbers in the same places, so a battle is compared by its
 * logic (same rolls, same damage and messages?) rather than by the PC's per-frame draw timing. Not with
 * ORRE_ROLL_SEEDS (that run's rolls are the console's already). */
static FILE* s_rr_file;
static s32 s_rr_next[3], s_rr_battle = -1; /* each list's next index, for the battle s_rr_battle */
static s32 s_rr_create;                    /* the "create" list's next index (within its battle tag) */
s32 pc_create_depth; /* inside pokemonCreate / pokemonCreateRndFit, a trainer's creation or its team's sort (2026-11-02) */
s32 pc_fight_initializing; /* battle_main.c: inside _fightInitialize, before its battle thread exists */
s32 pc_field_depth;        /* people.c: inside a wandering person's direction or pause draw (2026-11-03) */
static s32 s_rr_field;     /* the "field" list's next index (one list for the whole sitting) */

static void pc_rollrec_before_draw(u32* state)
{
    extern volatile u32 pc_vi_heartbeat __attribute__((weak));
    static const char* const kinds[3] = {"battle", "idle", "camera"}; /* the seed files' list names */
    s32 k;
    if (pc_field_depth > 0) {
        /* a wandering person's draw (peopleMoveWander: the direction, then the pause, 2026-11-03, for the field
         * injection): "field 0 <index> ...", one list for the whole sitting, in the PC's order */
        fprintf(s_rr_file, "field 0 %d %08X %u %08X\n", (int)s_rr_field++, (unsigned)*state,
                &pc_vi_heartbeat != NULL ? (unsigned)pc_vi_heartbeat : 0u, (unsigned)(*state * 0x343FDu + 0x269EC3u));
        fflush(s_rr_file);
        return;
    }
    if (pc_create_depth > 0) {
        /* a trainer or Pokémon being created (an opponent's team as its battle starts, a gift, a Shadow Pokémon), any
         * thread, in order: "create <battle> <index> ...", the battle being pc_battle_serial while a battle thread
         * exists (lbl_8047B5D0 non-zero), else 0 (2026-11-02: Dolphin watches the generator inside battles only, so
         * it takes ("create", N) for battle N and skips the 0s; the index counts within that tag) */
        static s32 tag = -1;
        /* during _fightInitialize the team is made before the battle thread exists: the battle about to start */
        s32 now = pc_fight_initializing ? pc_battle_serial + 1 : lbl_8047B5D0 != 0 ? pc_battle_serial : 0;
        if (now != tag) {
            tag = now;
            s_rr_create = 0;
        }
        fprintf(s_rr_file, "create %d %d %08X %u %08X\n", (int)now, (int)s_rr_create++, (unsigned)*state,
                &pc_vi_heartbeat != NULL ? (unsigned)pc_vi_heartbeat : 0u, (unsigned)(*state * 0x343FDu + 0x269EC3u));
        fflush(s_rr_file);
        return;
    }
    pc_rollseed_track();
    if (s_rs_battle != s_rr_battle) {
        s_rr_battle = s_rs_battle;
        s_rr_next[0] = s_rr_next[1] = s_rr_next[2] = 0;
    }
    if (s_rs_seen == 0 || pc_anim_particle_depth > 0) {
        return;
    }
    if (pc_move_camera_depth > 0 && (_activeThread == s_rs_seen || pc_idle_camera_depth > 0)) {
        k = 2;
    } else if (_activeThread == s_rs_seen) {
        k = 0;
    } else if (pc_idle_camera_depth > 0) {
        k = 1;
    } else {
        return;
    }
    fprintf(s_rr_file, "%s %d %d %08X %u %08X\n", kinds[k], (int)s_rs_battle, (int)s_rr_next[k]++, (unsigned)*state,
            &pc_vi_heartbeat != NULL ? (unsigned)pc_vi_heartbeat : 0u, (unsigned)(*state * 0x343FDu + 0x269EC3u));
    fflush(s_rr_file); /* whole up to a crash (a few hundred battle-thread draws a battle; blinks are on another thread) */
}

static void pc_rollseed_frame(void)
{
    static int loaded;
    if (!loaded) {
        const char* env = getenv("ORRE_ROLL_SEEDS");
        const char* rec = getenv("ORRE_ROLL_RECORD");
        FILE* f;
        char buf[256];
        s32 cur = -1, kind = 0, total = 0;
        loaded = 1;
        if (rec != NULL && env == NULL) {
            s_rr_file = fopen(strncmp(rec, "@file:", 6) == 0 ? rec + 6 : rec, "w");
            if (s_rr_file != NULL) {
                fprintf(s_rr_file, "# ORRE_ROLL_RECORD: kind battle index state_before frame state_after "
                                   "(pc_debug_game.c)\n");
                OrreHook_RngBeforeDraw = pc_rollrec_before_draw;
                OSReport("[rollrec] ORRE_ROLL_RECORD: the battle draws' states to %s\n", rec);
            } else {
                OSReport("[rollrec] ORRE_ROLL_RECORD: cannot write %s\n", rec);
            }
            return;
        }
        if (rec != NULL) {
            OSReport("[rollrec] ORRE_ROLL_RECORD ignored: ORRE_ROLL_SEEDS gives this run the console's rolls\n");
        }
        if (env == NULL || strncmp(env, "@file:", 6) != 0 || (f = fopen(env + 6, "r")) == NULL) {
            return;
        }
        while (fgets(buf, sizeof(buf), f) != NULL) {
            char* end;
            unsigned long v;
            if (strncmp(buf, "battle", 6) == 0 || strncmp(buf, "idle", 4) == 0 || strncmp(buf, "camera", 6) == 0) {
                kind = buf[0] == 'i' ? 1 : buf[0] == 'c' ? 2 : 0;
                cur = (s32)strtoul(buf + (kind == 1 ? 4 : 6), NULL, 10);
                if (cur >= s_rs_battles) {
                    s32 n = cur + 1;
                    s32 j;
                    for (j = 0; j < 3; j++) {
                        s_rs_states[j] = (u32**)realloc(s_rs_states[j], n * sizeof(*s_rs_states[j]));
                        s_rs_count[j] = (s32*)realloc(s_rs_count[j], n * sizeof(*s_rs_count[j]));
                    }
                    for (; s_rs_battles < n; s_rs_battles++) {
                        for (j = 0; j < 3; j++) {
                            s_rs_states[j][s_rs_battles] = NULL;
                            s_rs_count[j][s_rs_battles] = 0;
                        }
                    }
                }
                continue;
            }
            v = strtoul(buf, &end, 16);
            if (cur < 0 || end == buf || buf[0] == '#') {
                continue;
            }
            s_rs_states[kind][cur] =
                (u32*)realloc(s_rs_states[kind][cur], (s_rs_count[kind][cur] + 1) * sizeof(u32));
            s_rs_states[kind][cur][s_rs_count[kind][cur]++] = (u32)v;
            total++;
        }
        fclose(f);
        OrreHook_RngBeforeDraw = pc_rollseed_before_draw;
        OSReport("[rollseed] ORRE_ROLL_SEEDS: %d states for %d battle slots\n", (int)total, (int)s_rs_battles);
    }
    if (OrreHook_RngBeforeDraw != NULL) {
        pc_rollseed_track();
    }
}

/* ORRE_RNG_COUNT=1: how many random numbers the game draws, per room, in windows of 60 game frames (2026-11-13;
 * snatcherLeo: "Yes, measure"). Why: step 2.2 adds OrrePort's Language row to the title's Options screen and keeps it in
 * GameCube-accurate mode, on the reading that the title and top menu draw numbers by themselves (the title's particle
 * sparkles) while the row draws none; this measures it. Counts every draw (any thread) through OrreHook_RngBeforeDraw,
 * chained after whatever replay hook is set, so replays are untouched; changes nothing in the game. Lines:
 * "[rngcount] room 0x384 frames 1200-1259: 180 draws (60 frames)", plus one at each room change. Combine with
 * ORRE_MENU_LOG to see which menu (the Options screen) was open. Platform-neutral (debug builds of the port). */
static void (*s_rc_prev)(u32* state);
static u32 s_rc_draws;
/* Frame smoothing's tripwire (port/src/pc_smooth.c; docs/port/frame-smoothing-plan.md section 5; 2026-11-16) rides
 * on this chain: while smoothing is on, the chain is installed even without ORRE_RNG_COUNT, and every draw asks whether
 * an in-between picture is being made right now (on Aurora's render worker, or in the test mode while one is open);
 * if so the program stops, because in-between pictures must never run game code. Observe-only otherwise. Weak: the
 * port's side is not in every program that links this file. Platform-neutral. */
extern int pc_smooth_on(void) __attribute__((weak));
extern void pc_smooth_check_rng_draw(void) __attribute__((weak));

static void pc_rngcount_before_draw(u32* state)
{
    s_rc_draws++;
    if (pc_smooth_check_rng_draw != NULL) {
        pc_smooth_check_rng_draw();
    }
    if (s_rc_prev != NULL) {
        s_rc_prev(state);
    }
}

static void pc_rngcount_frame(u32 frame)
{
    static int enabled = -1;
    static u32 room_last = 0xFFFFFFFFu, from, win_draws, win_frames;
    u32 room, d;
    int tripwire;
    if (enabled < 0) {
        enabled = getenv("ORRE_RNG_COUNT") != NULL;
        if (enabled) {
            OSReport("[rngcount] ORRE_RNG_COUNT: the game's random draws per room, in 60-frame windows\n");
        }
    }
    tripwire = pc_smooth_on != NULL && pc_smooth_on(); /* frame smoothing's tripwire (above) */
    if (!enabled && !tripwire) {
        return;
    }
    if (OrreHook_RngBeforeDraw != pc_rngcount_before_draw) { /* chain after a replay hook set since */
        s_rc_prev = OrreHook_RngBeforeDraw;
        OrreHook_RngBeforeDraw = pc_rngcount_before_draw;
    }
    if (!enabled) {
        s_rc_draws = 0; /* only the tripwire: no [rngcount] lines */
        return;
    }
    {
        /* the floor context is null before the first room (the room lookup would read through it: the first run
         * crashed at frame 1); until then the draws count under room 0xFFFFFFFF, the boot */
        extern u32 lbl_8047ACC8;
        room = lbl_8047ACC8 != 0 ? std092_getCurrentRoomID() : 0xFFFFFFFFu;
    }
    d = s_rc_draws;
    s_rc_draws = 0;
    if (room != room_last || win_frames >= 60) {
        if (win_frames > 0) {
            OSReport("[rngcount] room 0x%X frames %u-%u: %u draws (%u frames)\n", (unsigned)room_last, (unsigned)from,
                     (unsigned)(frame - 1), (unsigned)win_draws, (unsigned)win_frames);
        }
        room_last = room;
        from = frame;
        win_draws = 0;
        win_frames = 0;
    }
    win_draws += d;
    win_frames++;
}

static void pc_owners_log_frame(u32 frame)
{
    extern u8* pc_waza_owner(s32 i, s32* count); /* wazaSequenceSys.c: entry i, or NULL */
    extern u8* pc_waza_owner_seq(u8* owner);      /* wazaSequenceSys.c: its running sequence (+0x6C), or NULL */
    extern u8* pc_waza_seq_timing(u8* seq);       /* wazaSequenceSys.c: its GetWazaTime timing record, or NULL */
    static int enabled = -1;
    static u32 from, to;
    s32 i, n = 0;
    if (enabled < 0) {
        const char* env = getenv("ORRE_OWNERS_LOG");
        enabled = 0;
        if (env != NULL && *env != '\0') {
            char* end;
            from = (u32)strtoul(env, &end, 10);
            to = *end == '-' ? (u32)strtoul(end + 1, NULL, 10) : 0xFFFFFFFFu;
            enabled = 1;
        }
    }
    if (!enabled || frame < from || frame > to) {
        return;
    }
    (void)pc_waza_owner(0, &n);
    for (i = 0; i < n && i < 256; i++) {
        u8* e = pc_waza_owner(i, &n);
        u8* seq;
        s16 tex;
        if (e == NULL) {
            continue;
        }
        tex = *(s16*)(e + 0x1C);
        seq = pc_waza_owner_seq(e);
        /* + the running sequence (2026-10-25): playing, its time counter (+0x00, compared with
         * wazaSequenceSysGetWazaTime by wazaSequenceSysIsAllEnd), its +0x14 / +0x15 bytes, and GetWazaTime's inputs:
         * kind (+0x0C), field_10, animationMode (+0x2E) and the timing record's s32s at +0x0C / +0x10 / +0x14 / +0x18 */
        {
            u8* tm = pc_waza_seq_timing(seq);
            OSReport("[owners] %u,%u,%d,%u,%u,%u,%u,%u,%d,%u,%d,%d,%u,%d,%d,%d,%u,%d,%d,%d,%d\n", (unsigned)frame,
                     (unsigned)VIGetRetraceCount(), (int)i, (unsigned)*(u16*)(e + 0x32), (unsigned)e[0x16],
                     (unsigned)e[0x18], (unsigned)e[0x19], (unsigned)*(u16*)(e + 0x20), (int)tex, (unsigned)e[0x75],
                     seq != NULL, seq != NULL ? (int)*(s32*)seq : 0, seq != NULL ? (unsigned)seq[0x14] : 0u,
                     seq != NULL ? (int)*(s8*)(seq + 0x15) : 0, seq != NULL ? (int)*(s32*)(seq + 0x0C) : 0,
                     seq != NULL ? (int)*(s32*)(seq + 0x10) : 0, seq != NULL ? (unsigned)*(u16*)(seq + 0x2E) : 0u,
                     tm != NULL ? (int)*(s32*)(tm + 0x0C) : 0, tm != NULL ? (int)*(s32*)(tm + 0x10) : 0,
                     tm != NULL ? (int)*(s32*)(tm + 0x14) : 0, tm != NULL ? (int)*(s32*)(tm + 0x18) : 0);
        }
    }
}

/* ORRE_ANIM_LOG=1 or =<from>-<to> (2026-11-22; observes only). Why: Studio's move preview and snatcherLeo disagreed on
 * whether a move's target first plays the last frames of its hit animation before the hit lands (the lead-in that
 * wazaSequencePokemonMotionStart sets: the hit starts frameCount - duration/2 into the animation). snatcherLeo asked for a
 * battle sweep that "doesn't affect the actual PC Port, it is just a copy/extension of that"; this is its first,
 * cheapest part: a text log over any battle (a the reference playthrough replay, a live game) of what each battle Pokémon's model plays.
 * Lines, at the frame boundary, for every active waza-sequence owner (the pool wazaSequenceSysIsAllEnd walks):
 *   [anim] frame,owner,index,animA,animB,animFrame,animEnd,playing,move,part,shift,seqTime
 *     owner: the pool slot; index: the owner's table index (+0x32); animA / animB: GSmodelGetAnimIndex (animB -1
 *     unless blending); animFrame: GSmodelGetAnimFrame; animEnd: GSmodelGetFrameCount's animation end; playing: a
 *     sequence is running (+0x6C); move: its move id (+0x2C); part: its kind (+0x0C: which half of the move, the
 *     timing record's row); shift: its field_10 (the value Studio reads as the lead-in shift); seqTime: its time
 *     counter (+0x00)
 *   [anim] start frame,owner,move,part,shift,animA,animFrame,animEnd: once, the first frame an owner's running sequence
 *     is a different (sequence, move, part) than the frame before, with the animation as it stands after the start
 * Owners are identified by slot; which slot is which Pokémon comes from the move's halves (the user plays its part
 * first, the target its part after) and from the [btext] / [battle] lines of the same log. Reads only (the getters
 * are pure reads of the model), so a replay's random rolls are identical with it on or off (checked on P17c,
 * 2026-11-22). Every OS the same (no platform calls). */
static void pc_anim_log_frame(u32 frame)
{
    extern u8* pc_waza_owner(s32 i, s32* count); /* wazaSequenceSys.c: entry i, or NULL */
    extern u8* pc_waza_owner_seq(u8* owner);      /* wazaSequenceSys.c: its running sequence (+0x6C), or NULL */
    static int enabled = -1;
    static u32 from, to;
    static u8* last_seq[64];
    static s32 last_move[64], last_part[64];
    s32 i, n = 0;
    if (enabled < 0) {
        const char* env = getenv("ORRE_ANIM_LOG");
        enabled = 0;
        if (env != NULL && *env != '\0' && strcmp(env, "0") != 0) {
            char* end;
            enabled = 1;
            from = 0;
            to = 0xFFFFFFFFu;
            if (strchr(env, '-') != NULL) { /* a range; "1" alone means every frame */
                from = (u32)strtoul(env, &end, 10);
                to = *end == '-' ? (u32)strtoul(end + 1, NULL, 10) : 0xFFFFFFFFu;
            }
            OSReport("[anim] logging %u-%u: frame,owner,index,animA,animB,animFrame,animEnd,playing,move,part,shift,"
                     "seqTime (pc_debug_game.c)\n", (unsigned)from, (unsigned)to);
        }
    }
    if (!enabled || frame < from || frame > to) {
        return;
    }
    (void)pc_waza_owner(0, &n);
    for (i = 0; i < n && i < 64; i++) {
        u8* e = pc_waza_owner(i, &n);
        u8* seq;
        struct GSmodel* model;
        u32 animA = 0xFFFFFFFFu, animB = 0xFFFFFFFFu;
        f32 animFrame = 0.0f, animEnd = 0.0f, texEnd = 0.0f;
        s32 move = -1, part = -1, shift = 0, seqTime = 0;
        if (e == NULL) {
            last_seq[i] = NULL;
            continue;
        }
        model = PTR32_AT_T(struct GSmodel, e, 0x24);
        if (model != NULL) {
            GSmodelGetAnimIndex(model, &animA, &animB);
            animFrame = GSmodelGetAnimFrame(model);
            GSmodelGetFrameCount(model, &animEnd, &texEnd);
        }
        seq = pc_waza_owner_seq(e);
        if (seq != NULL) {
            move = (s32)*(u16*)(seq + 0x2C);
            part = *(s32*)(seq + 0x0C);
            shift = *(s32*)(seq + 0x10);
            seqTime = *(s32*)seq;
            if (seq != last_seq[i] || move != last_move[i] || part != last_part[i]) {
                OSReport("[anim] start %u,%d,%d,%d,%d,%d,%.2f,%.2f\n", (unsigned)frame, (int)i, (int)move, (int)part,
                         (int)shift, (int)animA, (double)animFrame, (double)animEnd);
            }
        }
        last_seq[i] = seq;
        last_move[i] = move;
        last_part[i] = part;
        OSReport("[anim] %u,%d,%u,%d,%d,%.2f,%.2f,%d,%d,%d,%d,%d\n", (unsigned)frame, (int)i,
                 (unsigned)*(u16*)(e + 0x32), (int)animA, (int)animB, (double)animFrame, (double)animEnd,
                 seq != NULL, (int)move, (int)part, (int)shift, (int)seqTime);
    }
}

/* ORRE_SFX_LOG=1 (2026-10-20; observes only): every sound start and stop, battle cries and move sounds included
 * (they go straight to soundStart, past soundPlay's two slots). The sound table's entries (snd_data, disc layout)
 * hold a work pointer while the sound plays; the work's handle is the MusyX handle of its latest start. A
 * "[sfx] frame,retrace,started,stopped" row whenever the set changes (ids space-separated); the same row as
 * dolphin_playrec.py's sfx probe */
#include "game/gs_range_801653CC_shared.h"
static void pc_sfx_log_frame(u32 frame)
{
    static int enabled = -1;
    static s32 handles[4096];
    static u8 live[4096];
    char started[1024], stopped[1024];
    int ls = 0, lp = 0;
    u32 i, n;
    if (enabled < 0) {
        const char* env = getenv("ORRE_SFX_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled || snd_data_number == NULL || snd_data == NULL) {
        return;
    }
    n = *snd_data_number;
    if (n == 0 || n > 4096) {
        return;
    }
    started[0] = stopped[0] = '\0';
    for (i = 0; i < n; i++) {
        GSsndWork* w = DP(snd_data[i].work);
        if (w != NULL) {
            if (!live[i] || handles[i] != w->handle) {
                if (ls < (int)sizeof(started) - 8) {
                    ls += snprintf(started + ls, sizeof(started) - (size_t)ls, "%s%u", ls ? " " : "", (unsigned)i);
                }
                handles[i] = w->handle;
            }
            live[i] = 1;
        } else if (live[i]) {
            if (lp < (int)sizeof(stopped) - 8) {
                lp += snprintf(stopped + lp, sizeof(stopped) - (size_t)lp, "%s%u", lp ? " " : "", (unsigned)i);
            }
            live[i] = 0;
        }
    }
    if (ls || lp) {
        OSReport("[sfx] %u,%u,%s,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), started, stopped);
    }
}

/* ORRE_FSYS_LOG=1 (2026-10-21; observes only): the archive cache, every FSYS slot in use as "id:status:refs:name",
 * a "[fsys] frame,retrace,room,slots" row whenever the set changes; the same row as dolphin_playrec.py's fsys probe.
 * P01's Willie battle: the console's send-out found wzx_monsterball_out loaded, the PC loaded it */
#include "game/fsys/fsys.h"
static void pc_fsys_log_frame(u32 frame)
{
    extern FSYSManager lbl_80453FEC;
    extern FSYSSlot* lbl_8047B1B4;
    extern u32 lbl_8047ACC8;
    static int enabled = -1;
    static char last[4096];
    char row[4096];
    int len = 0;
    u32 i;

    if (enabled < 0) {
        const char* env = getenv("ORRE_FSYS_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled || lbl_8047B1B4 == NULL || lbl_80453FEC.maxSlots == 0 || lbl_80453FEC.maxSlots > 256) {
        return;
    }
    row[0] = '\0';
    for (i = 0; i < lbl_80453FEC.maxSlots && len < (int)sizeof(row) - 160; i++) {
        const FSYSSlot* s = &lbl_8047B1B4[i];
        const char* name;
        const char* slash;
        if (s->status == 0) {
            continue;
        }
        name = s->filename;
        slash = strrchr(name, '/');
        len += snprintf(row + len, sizeof(row) - (size_t)len, "%s%x:%d:%d:%.60s", len ? " " : "", (unsigned)s->fileHandle,
                        (int)s->status, (int)s->refCount, slash != NULL ? slash + 1 : name);
    }
    if (strcmp(row, last) == 0) {
        return;
    }
    strcpy(last, row);
    OSReport("[fsys] %u,%u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(),
             lbl_8047ACC8 != 0 ? (unsigned)std092_getCurrentRoomID() : 0u, row);
}

/* ORRE_FSYS_LOG=2 (2026-10-25; observes only): also the slots' states at the moment of each disc read's issue
 * and hand-over (port/src/pc_dvd.c calls this), as "[fsysnow] retrace why: handle:status ...", so the order of the
 * loader's steps and its reads inside one frame shows (P05's portrait load: state 101 before its table-of-contents
 * read) */
void pc_fsys_log_now(const char* why)
{
    extern FSYSManager lbl_80453FEC;
    extern FSYSSlot* lbl_8047B1B4;
    static int level = -1;
    char row[2048];
    int len = 0;
    u32 i;
    if (level < 0) {
        const char* env = getenv("ORRE_FSYS_LOG");
        level = env != NULL ? atoi(env) : 0;
    }
    if (level < 2 || lbl_8047B1B4 == NULL || lbl_80453FEC.maxSlots == 0 || lbl_80453FEC.maxSlots > 256) {
        return;
    }
    row[0] = '\0';
    for (i = 0; i < lbl_80453FEC.maxSlots && len < (int)sizeof(row) - 32; i++) {
        const FSYSSlot* s = &lbl_8047B1B4[i];
        if (s->status == 0 || s->status == 1000) {
            continue;
        }
        len += snprintf(row + len, sizeof(row) - (size_t)len, "%s%x:%d", len ? " " : "", (unsigned)s->fileHandle,
                        (int)s->status);
    }
    OSReport("[fsysnow] %u %s: %s\n", (unsigned)VIGetRetraceCount(), why, row);
}

/* ORRE_CACHE_LOG=1 (2026-10-25; observes only): the fsys ARAM cache's nodes (lbl_80454038.cache, 0x400 of them;
 * gs_range_8017FA5C_shared.h GsRangeCacheNode), a "[cache] frame,retrace,node:handle:key1:key2:size:aram ..." row of
 * the active ones whenever the set changes; dolphin_playrec.py's cache probe prints the same. P05's first street
 * battle: the portrait archive's table of contents was cached on the PC and not on the console */
/* ORRE_MENU_LOG=1 (2026-10-25; observes only): the open menu windows, as dolphin_playrec.py's menu probe reads
 * them (winSysWork +0x0C the window list; each window, GameCube layout on the PC too: +0x01 phase, +0x04 id, +0x10
 * next, +0x19 cursorCount, +0x1A cursorMode, +0x1B cursorFlags, +0x94 / +0x95 cursor base / offset), a
 * "[menu] frame,retrace,id:phase:count:mode:flags:base:offset ..." row when anything changes. P05's PC box: the
 * console deposited where the PC, with the same presses, withdrew */
static void pc_menu_log_frame(u32 frame)
{
    extern u8 winSysWork[];
    static int enabled = -1;
    static char last[2048];
    char row[2048];
    int len = 0, guard = 0;
    u8* node;
    if (enabled < 0) {
        const char* env = getenv("ORRE_MENU_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled) {
        return;
    }
    row[0] = '\0';
    node = PTR32_AT_T(u8, winSysWork, 0x0C);
    {
        /* and the battle message box's message id (window 0x50, +0x9C), a "[bmsg] frame,retrace,id" line when it
         * changes: the Dolphin probe's battle_msg events (dolphin_playrec.py) read the same field */
        static u32 lastMsg;
        u8* w = node;
        u32 mid = 0;
        int g = 0;
        while (w != NULL && g < 64) {
            if (*(const u32*)(w + 4) == 0x50) {
                mid = *(const u32*)(w + 0x9C);
                break;
            }
            w = PTR32_AT_T(u8, w, 0x10);
            g++;
        }
        if (mid != 0 && mid != lastMsg) {
            OSReport("[bmsg] %u,%u,%u\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), (unsigned)mid);
        }
        lastMsg = mid;
    }
    while (node != NULL && guard < 64 && len < (int)sizeof(row) - 64) {
        len += snprintf(row + len, sizeof(row) - (size_t)len, "%s%u:%d:%d:%u:%u:%d:%d", len ? " " : "",
                        (unsigned)*(const u32*)(node + 4), (int)(s8)node[1], (int)(s8)node[0x19], (unsigned)node[0x1A],
                        (unsigned)node[0x1B], (int)(s8)node[0x94], (int)(s8)node[0x95]);
        node = PTR32_AT_T(u8, node, 0x10);
        guard++;
    }
    if (strcmp(row, last) == 0) {
        return;
    }
    strcpy(last, row);
    OSReport("[menu] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), row);
}

/* ORRE_PARTY_LOG=1 (2026-10-25; observes only): the party's species in slot order, a "[party] frame,retrace,
 * s0 s1 s2 s3 s4 s5" row whenever it changes (0 = empty); dolphin_playrec.py's party probe prints the same. P05: the
 * console reordered the party in the PC box before the first street battle, the PC with the same presses didn't */
static void pc_party_log_frame(u32 frame)
{
    /* heroGetStatus, pokemonGetStatus, savedataBiosGetNowSavedataPtr: canonical prototypes in pc/prototypes.h (force-included) */
    static int enabled = -1;
    static char last[96];
    char row[96];
    int len = 0;
    u16 slot;
    if (enabled < 0) {
        const char* env = getenv("ORRE_PARTY_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0' ? atoi(env) : 0;
        if (env != NULL && *env != '\0' && *env != '0' && enabled == 0) {
            enabled = 1;
        }
        if (env == NULL || *env == '\0') {
            /* on with the event log (2026-11-10, snatcherLeo: capture what helps the docs): the party's order, the Shadow
             * Pokémon's heart gauges and the TM pocket, each when it changes (observe only) */
            extern int pc_event_log_on(void); /* port/src/pc_event_log.c */
            enabled = pc_event_log_on() ? 2 : 0;
        }
    }
    if (!enabled) {
        return;
    }
    if (savedataBiosGetNowSavedataPtr() == 0) {
        return;
    }
    if (enabled >= 2) {
        /* ORRE_PARTY_LOG=2 (2026-10-29): also the TM pocket in slot order, "[tms] frame,retrace,item:count ..."
         * whenever it changes (the save work block +0x92C: the hero record +0x70, its pockets after the party, 0x40
         * {u16 item, u16 count} entries; native on the PC); dolphin_playrec.py's tms probe prints the same. P09: the TM
         * list's row 0 held another TM on the PC after Miror B. */
        static char lastTms[400];
        char tms[400];
        int tn = 0;
        u8* pocket = (u8*)PC_PTR(savedataBiosGetNowSavedataPtr()) + 0x92C;
        u16 i;
        tms[0] = '\0';
        for (i = 0; i < 0x40 && tn < (int)sizeof(tms) - 16; i++) {
            u16 item = *(u16*)(pocket + 4 * i);
            if (item != 0) {
                tn += snprintf(tms + tn, sizeof(tms) - (size_t)tn, "%s%u:%u", tn ? " " : "", (unsigned)item,
                               (unsigned)*(u16*)(pocket + 4 * i + 2));
            }
        }
        if (strcmp(tms, lastTms) != 0) {
            strcpy(lastTms, tms);
            OSReport("[tms] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), tms);
        }
    }
    row[0] = '\0';
    for (slot = 0; slot < 6; slot++) {
        u8* p = (u8*)PC_PTR(heroGetStatus(NULL, 3, slot));
        unsigned sp = p != NULL ? (unsigned)(u16)pokemonGetStatus(p, 0, POKEMON_STATUS_SPECIES, 0) : 0u;
        len += snprintf(row + len, sizeof(row) - (size_t)len, "%s%u", slot ? " " : "", sp);
    }
    {
        /* and the party's heart gauges (selector 0xC5, the current gauge; 0 = ready to purify) for Shadow Pokémon (0xC2
         * set), a "[dp] frame,retrace,species:gauge ..." row whenever one changes (2026-10-29: P11's Relic Stone
         * purified on the console and not on the PC; likely a gauge one step-drop above 0) */
        static char lastDp[160];
        char dp[160];
        int n = 0;
        dp[0] = '\0';
        for (slot = 0; slot < 6; slot++) {
            u8* p = (u8*)PC_PTR(heroGetStatus(NULL, 3, slot));
            if (p != NULL && (u8)pokemonGetStatus(p, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0) != 0) {
                n += snprintf(dp + n, sizeof(dp) - (size_t)n, "%s%u:%d", n ? " " : "",
                              (unsigned)(u16)pokemonGetStatus(p, 0, POKEMON_STATUS_SPECIES, 0), (int)pokemonGetStatus(p, 0, POKEMON_STATUS_HEART_GAUGE, 0));
            }
        }
        if (strcmp(dp, lastDp) != 0) {
            strcpy(lastDp, dp);
            OSReport("[dp] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), dp);
        }
    }
    if (strcmp(row, last) == 0) {
        return;
    }
    strcpy(last, row);
    OSReport("[party] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), row);
}

/* a local view of gs_range_8017FA5C_shared.h's GsRangeCacheNode / GsRangeCache (that header's GSmemAlloc prototype
 * clashes with this file's: firstlink then stubbed the whole file, 2026-10-25) */
typedef struct PcCacheNode {
    void* data;
    struct PcCacheNode* prev;
    struct PcCacheNode* next;
    s32 size;
    u32 fileHandle;
    u32 key1;
    u32 key2;
    s32 active;
} PcCacheNode;
typedef struct PcCache {
    PcCacheNode* nodes;
    PcCacheNode* last;
    u32 capacity;
    s32 count;
} PcCache;
static void pc_cache_log_frame(u32 frame)
{
    extern PcCache lbl_80454038;
    static int enabled = -1;
    static char last[40000];
    static char row[40000];
    int len = 0;
    s32 i;
    if (enabled < 0) {
        const char* env = getenv("ORRE_CACHE_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (!enabled || lbl_80454038.nodes == NULL) {
        return;
    }
    row[0] = '\0';
    for (i = 0; i < 0x400 && len < (int)sizeof(row) - 64; i++) {
        const PcCacheNode* n = &lbl_80454038.nodes[i];
        if (!n->active) {
            continue;
        }
        len += snprintf(row + len, sizeof(row) - (size_t)len, "%s%d:%x:%x:%x:%x:%x", len ? " " : "", (int)i,
                        (unsigned)n->fileHandle, (unsigned)n->key1, (unsigned)n->key2, (unsigned)n->size,
                        (unsigned)(uintptr_t)n->data);
    }
    if (strcmp(row, last) == 0) {
        return;
    }
    strcpy(last, row);
    OSReport("[cache] %u,%u,%s\n", (unsigned)frame, (unsigned)VIGetRetraceCount(), row);
}

/* ORRE_EVENT_LOG=1 (2026-10-19, snatcherLeo: the PC logs what the Dolphin recorder logs, so replay_report.py can compare
 * them; observes only): called once per frame from pc_vi.c's end of frame. Prints, like dolphin_playrec.py's
 * watch_game / watch_flags:
 *   "[ev] frame=<n> msgwin type=<t> error=<e>"   when the open message window's type changes (lbl_80478B30, s8, -1 =
 *                                                 closed; error = _winMsgOpenErrWin), not for the first value seen;
 *   "[ev] frame=<n> flag cls=<c> word=<k> bit=<b> now=<v>"   for every story-flag bit that changed since the last
 *                                                 frame (classes 0-3 of lbl_80478EEC, words as the game holds them);
 *   "[ev] frame=<n> pos <x> <y> <z>"              the player's position every 10 frames (field only). */
void pc_event_log_frame(u32 frame)
{
    typedef struct { u32 wordCount; u32 buffer; } EvFlagState; /* FlagStateEntry (gs_flag_exact_801908D4.c) */
    extern void* lbl_80478EEC;
    extern s8 lbl_80478B30; /* the open message window's type, -1 when closed (win_msg_exact_80105A3C.c) */
    extern u8 _winMsgOpenErrWin; /* the message window's error byte, as dolphin_playrec.py reads it */
    static int enabled = -1;
    static int haveMsg;
    static s8 lastMsg;
    static u32* prev[4];
    static u32 prevCount[4];
    EvFlagState* states;
    u32 c, k, b;

    pc_npc_log_frame(frame); /* ORRE_NPC_LOG: independent of ORRE_EVENT_LOG, same once-per-frame call */
    pc_msg_log_frame(frame); /* ORRE_MSG_LOG: the same */
    {
        extern void pc_event_log_dialog_frame(void); /* port/src/pc_event_log.c: the dialogue lines (event log) */
        pc_event_log_dialog_frame();
    }
    pc_camparams_log_frame(frame); /* ORRE_CAMPARAMS_LOG: the same */
    {
        extern void pc_bcam_log_frame(u32 frame); /* pc_bcam_log.c: ORRE_BCAM_LOG, the running battle-camera shot */
        pc_bcam_log_frame(frame);
    }
    pc_fsys_log_frame(frame);      /* ORRE_FSYS_LOG: the same */
    pc_cache_log_frame(frame);     /* ORRE_CACHE_LOG: the ARAM cache's nodes */
    pc_party_log_frame(frame);     /* ORRE_PARTY_LOG: the party's order */
    pc_menu_log_frame(frame);      /* ORRE_MENU_LOG: the open menu windows */
    pc_fade_log_frame(frame);      /* ORRE_FADE_LOG: the same */
    pc_sound_log_frame(frame);     /* ORRE_SOUND_LOG: the same */
    pc_sfx_log_frame(frame);       /* ORRE_SFX_LOG: the same */
    pc_win_log_frame(frame);       /* ORRE_WIN_LOG: the same */
    pc_floor_log_frame(frame);     /* ORRE_FLOOR_LOG: the same */
    pc_owners_log_frame(frame);    /* ORRE_OWNERS_LOG: the battle's waza-sequence owners (blink) */
    pc_anim_log_frame(frame);      /* ORRE_ANIM_LOG: each battle Pokémon's animation and running move part */
    pc_rollseed_frame();           /* ORRE_ROLL_SEEDS: the per-draw battle-roll resync; ORRE_ROLL_RECORD: the other way */
    pc_rngcount_frame(frame);      /* ORRE_RNG_COUNT: random draws per room (after the replay hooks, which it chains) */
    pc_vid_log_frame(frame);       /* ORRE_VID_LOG: MusyX's live voice ids */
    if (enabled < 0) {
        const char* env = getenv("ORRE_EVENT_LOG");
        enabled = env != 0 ? (atoi(env) > 1 ? 2 : 1) : 0;
    }
    if (!enabled) {
        return;
    }
    /* the player's position every 10 frames ("[ev] frame=<n> pos <x> <y> <z>"), like dolphin_playrec.py's tick (which
     * samples every 60), or every frame with ORRE_EVENT_LOG=2 (the per-frame pos probe): the hero is people group 0,
     * index 100 (the recorder's player_pos); none in battle or menus */
    if (enabled == 2 || frame % 10 == 0) {
        extern u8 fightIsBattleActive(void);
        struct PeopleEntry* hero = fightIsBattleActive() == 1 ? NULL : peopleIsInGroup(0, 100);
        const f32* p = hero != NULL ? (const f32*)peopleGetPositionPtr(hero) : NULL;
        if (p != NULL) {
            OSReport("[ev] frame=%u pos %.5f %.5f %.5f\n", (unsigned)frame, (double)p[0], (double)p[1], (double)p[2]);
        }
    }
    /* with ORRE_EVENT_LOG=2, also the stick the game uses each frame (pad 1: the sampled value, this frame's value, the
     * filtered position and the final quantised value; input.h InputPad), for comparing with Dolphin's input probe */
    if (enabled == 2) {
        extern InputPad lbl_80401C10[INPUT_PAD_COUNT];
        int k;
        for (k = 0; k < INPUT_PAD_COUNT; k++) {
            const InputPad* ip = &lbl_80401C10[k];
            if (ip->id == 1) {
                /* step: the game's frame step (retraces this game frame counts as; timers and movement scale by
                 * it), to compare with Dolphin's frame_retraces (the pos probe) */
                extern u32 GSgetFrameStep(void);
                OSReport("[ev] frame=%u stick latched %d %d current %d %d filtered %.4f %.4f out %d %d smooth %u/%d mode %u "
                         "step %u\n",
                         (unsigned)frame, (int)ip->latched.stickX, (int)ip->latched.stickY, (int)ip->current.stickX,
                         (int)ip->current.stickY, (double)ip->stickX, (double)ip->stickY, (int)ip->outStickX,
                         (int)ip->outStickY, (unsigned)ip->smoothFrames, (int)ip->smoothMode, (unsigned)ip->outputMode,
                         (unsigned)GSgetFrameStep());
            }
        }
    }
    if (haveMsg && lbl_80478B30 != lastMsg) {
        OSReport("[ev] frame=%u msgwin type=%d error=%u\n", (unsigned)frame, (int)lbl_80478B30,
                 (unsigned)_winMsgOpenErrWin);
    }
    lastMsg = lbl_80478B30;
    haveMsg = 1;

    states = (EvFlagState*)lbl_80478EEC;
    for (c = 0; states != NULL && c < 4; c++) {
        const u32* words = (const u32*)PC_PTR(states[c].buffer);
        u32 n = words != NULL ? states[c].wordCount : 0;
        if (n > 0x1000) {
            continue;
        }
        if (prev[c] != NULL && prevCount[c] == n) {
            for (k = 0; k < n; k++) {
                u32 x = prev[c][k] ^ words[k];
                for (b = 0; x != 0 && b < 32; b++) {
                    if ((x >> b) & 1u) {
                        OSReport("[ev] frame=%u flag cls=%u word=%u bit=%u now=%u\n", (unsigned)frame, (unsigned)c,
                                 (unsigned)k, (unsigned)b, (unsigned)((words[k] >> b) & 1u));
                    }
                }
            }
        }
        if (prevCount[c] != n) {
            free(prev[c]);
            prev[c] = n != 0 ? (u32*)malloc(n * sizeof(u32)) : NULL;
            prevCount[c] = prev[c] != NULL ? n : 0;
        }
        if (prev[c] != NULL) {
            memcpy(prev[c], words, n * sizeof(u32));
        }
    }
}

/* `fight ID`: start a battle from the field the way the developers' own debug battle test does (fn_8000804C in
 * gs_task.c: GSthreadCreate(1, floorGetCurrentContext(), 0x4000, 1, 1, fightStartEncount) with the id as its argument;
 * battle-engine.md "Battle start", verified). fightStartEncount blocks and yields, so it runs on its own GS thread.
 * Refused while a battle runs and outside field rooms: the room record's class must be 0x20 (floorDataBiosGetFloorKind
 * 1); pushed-room classes 0xA0 (menus, special) and 0xC0 (title, top menus), battle fields 0x40 and the move viewer
 * 0x80 are refused (snatcherLeo, 2026-10-17: use the class values, not a room-id threshold). */
static int dbg_fight_start(PcDebugU32 id)
{
    extern u8 fightIsBattleActive(void);
    extern u32 fightStartEncount(u32 id);
    extern void* floorDataBiosGetPtr(u32 key);
    extern u8 floorDataBiosGetFloorKind(u8* ptr);
    extern void GSthreadSetArgs(void* threadPtr, s32 count, ...);
    u8* room;
    void* task;

    if (id == 0 || fightIsBattleActive() != 0) {
        return 0;
    }
    room = (u8*)floorDataBiosGetPtr(std092_getCurrentRoomID());
    if (room == NULL || floorDataBiosGetFloorKind(room) != 1) {
        return 0;
    }
    task = GSthreadCreate(1, floorGetCurrentContext(), 0x4000, 1, 1, (void*)fightStartEncount);
    if (task == NULL) {
        return 0;
    }
    GSthreadSetArgs(task, 1, (u32)id);
    return 1;
}

/* The F1 header's room name: not in the room record (only archive ids), so NULL for now; the port side can name rooms
 * from the docs/testing/spots.json */
static const char* dbg_room_name(PcDebugU32 room)
{
    (void)room;
    return 0;
}

/* The current encounter id, 0 outside a battle (battle_main.c: fightIsBattleActive, fightGetFloorKind = the encounter id) */
static PcDebugU32 dbg_battle_number(void)
{
    extern u8 fightIsBattleActive(void);
    extern u16 fightGetFloorKind(void);
    return fightIsBattleActive() != 0 ? (PcDebugU32)fightGetFloorKind() : 0;
}

static const PcDebugApi s_api = {
    dbg_warp,
    dbg_set_flag,
    dbg_get_flag,
    dbg_give_item,
    dbg_give_pokemon,
    0, /* set_team */
    0, /* battle_result */
    dbg_set_rng_seed,
    dbg_skip_movies,
    dbg_checkpoint_save,
    dbg_checkpoint_load,
    dbg_current_room,
    dbg_fight_start,
    dbg_room_name,
    dbg_battle_number,
    dbg_get_rng_state,
    dbg_set_npc,
};

__attribute__((constructor)) static void pc_debug_game_register(void)
{
    pc_debug_register(&s_api);
    /* the quick cheats' game side, always (the Cheats menu works without the debug tools) */
    pc_cheats_pc_top_up = cheat_pc_top_up;
    pc_cheats_money_fill = cheat_money_fill;
    /* the old test switch keeps working: it now sets the hook instead of being read inside GSmovieOpen */
    if (getenv("ORRE_SKIP_MOVIES") != 0) {
        OrreHook_SkipMovies = 1;
    }
    {
        const char* seed = getenv("ORRE_RNG_SEED");
        if (seed != 0 && seed[0] != 0) {
            u32 value = 0;
            u32 base = 10;
            const char* p = seed;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                base = 16;
                p += 2;
            }
            for (; *p != 0; p++) {
                u32 digit;
                if (*p >= '0' && *p <= '9') {
                    digit = (u32)(*p - '0');
                } else if (base == 16 && *p >= 'a' && *p <= 'f') {
                    digit = (u32)(*p - 'a' + 10);
                } else if (base == 16 && *p >= 'A' && *p <= 'F') {
                    digit = (u32)(*p - 'A' + 10);
                } else {
                    break;
                }
                value = value * base + digit;
            }
            OrreHook_FixedRngSeed = value;
            OrreHook_FixedRngSeedSet = 1;
        }
    }
}

/* effect_visual.c's LAYER_JOBJ_OFF_* (the aura's joint fields, read through a byte pointer) must equal the PC's
 * HSD_JObj layout; that file can't include hsd_jobj.h, so the values are checked here (2026-10-25) */
#include <stddef.h>
#include "hsd/hsd_jobj.h"
_Static_assert(offsetof(HSD_JObj, flags) == 0x28, "effect_visual.c LAYER_JOBJ_OFF_FLAGS");
_Static_assert(offsetof(HSD_JObj, scale_x) == 0x48, "effect_visual.c LAYER_JOBJ_OFF_SCALE");
_Static_assert(offsetof(HSD_JObj, translate_x) == 0x54, "effect_visual.c LAYER_JOBJ_OFF_TRANSLATE");

/* Frame smoothing round 4 (the project's build helper, 2026-11-17; docs/port/frame-smoothing-plan.md section 16): what
 * port/src/pc_smooth.c's model marker needs to know about the piece OrreHook_ModelDraw reports, read here because
 * only the game units are compiled with the PC's HSD struct layout (pc/prototypes.h; port code can't safely include
 * hsd_pobj.h on its own). Observation only: two pointer walks, no game state written, no random draws.
 *   *root:  the model the piece belongs to, the top JObj of its tree (the parent chain's end), so the blend can treat
 *           every piece of one model alike (a model snaps whole, never piece by piece: a piece shown at its newest
 *           pose next to its neighbours half a step behind tears the model at the seam);
 *   return: the mask of position-matrix slots the piece's display list uses (bit n = GX_PNMTX0 + 3n), from how
 *           PObjSetupMtx loads them (src/hsd/pobj.c): an envelope piece one slot per envelope (at most ten), a piece
 *           with a second joint (shared vertices) slots 0 and 1, every other piece (rigid, shape animation) slot 0.
 *           The other slots keep whatever an earlier piece loaded; the blend leaves them alone.
 * Platform-neutral (plain C over the game's own structs). XD: the same HSD structs (likely). */
#include "hsd/hsd_pobj.h"
#include <stdint.h>
u32 pc_smooth_model_info(const void* jobjp, const void* pobjp, u64* root) {
    const HSD_JObj* jobj = (const HSD_JObj*)jobjp;
    const HSD_PObj* pobj = (const HSD_PObj*)pobjp;
    u32 depth = 0;
    *root = 0;
    if (jobj != NULL) {
        while (jobj->parent != NULL && depth++ < 256) { /* 256: a guard against a broken chain, never reached */
            jobj = jobj->parent;
        }
        *root = (u64)(uintptr_t)jobj;
    }
    if (pobj == NULL) {
        return 0;
    }
    switch (pobj_type(pobj)) {
    case POBJ_ENVELOPE: {
        const HSD_SList* list = pobj->u.envelope_list;
        u32 mask = 0, i;
        for (i = 0; i < 10 && list != NULL; i++, list = list->next) {
            mask |= 1u << i;
        }
        return mask;
    }
    case POBJ_SKIN:
        return pobj->u.jobj != NULL ? 3u : 1u;
    default:
        return 1u;
    }
}

/* Frame smoothing round 11, 2D at 120 (the project's build helper, 2026-11-17; docs/port/frame-smoothing-plan.md section 24):
 * names a 2D piece for OrreHook_2DPiece. The game's 2D is drawn through GSgfx's request buffer: while the game runs it
 * records the drawing commands (recordMode 1) and the frame plays them back later, once per layer
 * (gs_gfx_range_800D3FA4.c), so a plain call here would come long before the piece's draws. The tag goes into the
 * buffer as a PC-only command (0x7E: kind, variant, id, value) right before the piece's own commands, with a kind-0
 * tag after them; played back, it reaches the hook in order with the draws. While playing back or setting up
 * (recordMode 2: commands run at once) the hook is called directly. With the hook unset (smoothing off, or
 * ORRE_SMOOTH_2D=0) nothing is recorded: the buffer is the vanilla one. Observation only: no game state, no random
 * draws. All platforms (plain C). XD: the same GSgfx request buffer (likely), so the same tag would work there. */
/* GSgfx's state (include/game/gs_gfx_layer.h, GSgfxState): only its first field is read here, recordMode (s32 at
 * 0x000). The header itself is not included: it pulls Dolphin OS headers whose declarations clash with this unit's
 * (a first try did, and firstlink stubbed the whole unit: the replay crashed at boot, round 11) */
extern void* gsgfxState;
extern void (*OrreHook_2DPiece)(u32 kind, u32 variant, u32 id, u32 valueBits);
void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value) {
    extern void gfxRecordCommand(u32 command, u32 format, ...);
    u32 bits;
    if (OrreHook_2DPiece == NULL || gsgfxState == NULL) {
        return;
    }
    memcpy(&bits, &value, sizeof(bits));
    if (*(const s32*)gsgfxState == 1) { /* recordMode: 1 record into the request buffer */
        gfxRecordCommand(0x7E, 4, (u32)kind, variant, (u32)(uintptr_t)id, bits);
    } else {
        OrreHook_2DPiece((u32)kind, variant, (u32)(uintptr_t)id, bits);
    }
}

#endif /* TARGET_PC */
