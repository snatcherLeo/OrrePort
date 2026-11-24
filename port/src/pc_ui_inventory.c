/*
 * pc_ui_inventory.c: the 2D inventory test switch, ORRE_UI_INVENTORY=1 (widescreen groundwork, Phase 3 step 3.6;
 * docs/port/widescreen-plan.md section 6, "The 2D inventory switch"; the format: docs/port/setup.md, "The 2D
 * inventory").
 *
 * Why: before any piece of the game's 2D can get a per-ratio row (16:9 and 16:10 for 0.1, 21:9 later through Studio's
 * UI Editor), we need the list of pieces: every window sprite, every code-drawn 2D projection, with its ids and its
 * rectangle in logical 640 x 480 pixels, per room or battle. A replay of a the reference playthrough sitting with this switch on writes that
 * list into the run's log; tools/local/ui_inventory.py folds a run's lines into one table (under extracted/: ids and
 * rectangles only, regenerable, never committed).
 *
 * How: one observation hook, OrreHook_Ui2DDraw (include/pc/orre_hooks.h), called from winSpriteDraw (a sprite about to
 * be drawn), gfxSetOrtho (a code-drawn flat projection set), spriteSetEnv (the window 2D camera set) and menuDaemon's
 * draw loop (a whole window at its origin, and whether it has a draw callback). Each piece is
 * written once per room or battle when first seen ("new"), and once when the room or battle ends ("end", with how often
 * it was drawn and the first rule set's kind). Everything here only reads: no game state is written, no random number
 * is drawn, no timing changes (proved with P01's replay rolls, setup.md). With the switch unset the hook stays NULL and
 * the game code skips the reads.
 *
 * Threads: the hook is called on the game thread (game code records its 2D there; the request buffer's playback of an
 * ortho command runs there too), as are the room callback and atexit, so no locking.
 * Platforms: plain C, the same on Windows, Linux and macOS (likely; run on Windows). The caller addresses are absolute:
 * every OrrePort exe is linked at a fixed base below 4 GB (port/CMakeLists.txt, orre_low_image), so the script names
 * them with nm on the same exe on every OS.
 * XD: the same window / sprite / GSgfx code is shared engine (likely), so the same three call sites should exist; the
 * menu ids and the code-drawn callers would differ.
 */
#include "pc_debug.h" /* the encounter id: pc_debug_api()->battle_number (reads only) */

#include <dolphin/types.h>
#include "pc/orre_hooks.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern volatile u32 pc_vi_heartbeat; /* pc_vi.c: the game frame count */

enum { MAX_PIECES = 8192 }; /* per room or battle; past it new pieces are counted, not listed (a line says so) */

typedef struct Piece {
    /* the identity (what makes two draws the same piece) */
    u32 source, window, texture, primitive, flags;
    s32 item;
    s16 cropX, cropY;
    s16 keyX, keyY, keyW, keyH; /* ortho only: its projection box is part of its identity */
    uintptr_t caller;
    /* what was seen */
    int seq;
    u32 firstFrame, lastFrame, frames, activeFrames, idleFrames;
    s16 x0, y0, x1, y1; /* the union of every rectangle seen (a sliding panel's whole path) */
    int used;
} Piece;

static int s_on;
static Piece* s_pieces;    /* MAX_PIECES, open addressing on the identity */
static int s_pieceCount, s_overflow, s_seq;
static u32 s_room = 0xFFFFFFFFu, s_battle;
static u32 s_placeFirst, s_placeFrame = 0xFFFFFFFFu; /* the place's first frame; the frame the battle was last read */
static int s_placeOpen;

static u64 mix(u64 h, u64 v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}

static u64 identity_hash(const OrreUi2DPiece* p) {
    u64 h = p->source;
    h = mix(h, p->window);
    h = mix(h, p->texture);
    h = mix(h, p->primitive);
    h = mix(h, p->flags);
    h = mix(h, (u32)p->item);
    h = mix(h, (u32)(u16)p->cropX << 16 | (u16)p->cropY);
    h = mix(h, (u64)(uintptr_t)p->caller);
    if (p->source == ORRE_UI2D_ORTHO) {
        h = mix(h, (u64)(u16)p->x << 48 | (u64)(u16)p->y << 32 | (u32)(u16)p->w << 16 | (u16)p->h);
    }
    return h;
}

static int same_piece(const Piece* q, const OrreUi2DPiece* p) {
    return q->source == p->source && q->window == p->window && q->texture == p->texture && q->flags == p->flags &&
           q->primitive == p->primitive && q->item == p->item && q->cropX == p->cropX && q->cropY == p->cropY &&
           q->caller == (uintptr_t)p->caller &&
           (p->source != ORRE_UI2D_ORTHO || (q->keyX == p->x && q->keyY == p->y && q->keyW == p->w && q->keyH == p->h));
}

static const char* source_name(u32 source) {
    return source == ORRE_UI2D_SPRITE   ? "sprite"
           : source == ORRE_UI2D_ORTHO  ? "ortho"
           : source == ORRE_UI2D_ENV    ? "env"
           : source == ORRE_UI2D_WINDOW ? "window"
                                        : "?";
}

/* The plan's first rule set (section 6; a guess until the list exists), applied per room or battle:
 *   full-screen  a sprite whose rectangles cover the whole 640 x 480;
 *   menu         a sprite whose window took input on most of the frames it was drawn;
 *   hud          drawn on at least half the place's frames, mostly while no window took input (60 frames at least);
 *   code-drawn   no window (a sprite drawn without one, an ortho projection, a 2D camera set outside the windows);
 *   window       any other window sprite, or a whole window (source "window": its origin, w = h = 0).
 * 3D-anchored and the code-drawn full-screen layers can't be told from these values: the script names those by their
 * caller (tools/local/ui_inventory.py). */
static const char* kind_of(const Piece* q, u32 placeFrames) {
    if (q->source == ORRE_UI2D_SPRITE && q->x0 <= 0 && q->y0 <= 0 && q->x1 >= 640 && q->y1 >= 480) {
        return "full-screen";
    }
    if ((q->source == ORRE_UI2D_SPRITE || q->source == ORRE_UI2D_WINDOW) && q->window != 0 &&
        q->activeFrames * 2 > q->frames) {
        return "menu";
    }
    if (placeFrames >= 60 && q->frames * 2 >= placeFrames && q->idleFrames * 2 >= q->frames) {
        return "hud";
    }
    if (q->source == ORRE_UI2D_ORTHO || q->source == ORRE_UI2D_ENV || q->window == 0) {
        return "code-drawn";
    }
    return "window";
}

/* the place (room or battle) ends: one "end" line per piece, then forget them */
static void place_end(void) {
    int i;
    u32 placeFrames;
    if (!s_placeOpen) {
        return;
    }
    placeFrames = pc_vi_heartbeat - s_placeFirst + 1u;
    for (i = 0; i < MAX_PIECES; i++) {
        const Piece* q = &s_pieces[i];
        if (!q->used) {
            continue;
        }
        fprintf(stderr, "[ui2d] end n=%d frames=%u active_frames=%u idle_frames=%u place_frames=%u bbox=%d,%d,%d,%d "
                        "last=%u kind=%s\n",
                q->seq, q->frames, q->activeFrames, q->idleFrames, placeFrames, q->x0, q->y0, q->x1, q->y1,
                q->lastFrame, kind_of(q, placeFrames));
    }
    fprintf(stderr, "[ui2d] place_end room=0x%X battle=%u frames=%u pieces=%d not_listed=%d\n", s_room, s_battle,
            placeFrames, s_pieceCount, s_overflow);
    memset(s_pieces, 0, (size_t)MAX_PIECES * sizeof(Piece));
    s_pieceCount = 0;
    s_overflow = 0;
    s_placeOpen = 0;
}

static void place_start(void) {
    s_placeOpen = 1;
    s_placeFirst = pc_vi_heartbeat;
    fprintf(stderr, "[ui2d] place room=0x%X battle=%u f=%u\n", s_room, s_battle, (unsigned)pc_vi_heartbeat);
}

/* the encounter id, read once a frame; a change ends the place (a battle inside a room is a place of its own) */
static void check_battle(void) {
    const u32 frame = pc_vi_heartbeat;
    const PcDebugApi* api;
    u32 battle;
    if (frame == s_placeFrame) {
        return;
    }
    s_placeFrame = frame;
    api = pc_debug_api();
    battle = api != NULL && api->battle_number != NULL ? api->battle_number() : 0;
    if (battle != s_battle) {
        place_end();
        s_battle = battle;
    }
}

/* OrreHook_Ui2DDraw: game thread; reads only */
static void observe(const OrreUi2DPiece* p) {
    const u32 frame = pc_vi_heartbeat;
    const u64 h = identity_hash(p);
    unsigned i = (unsigned)(h ^ (h >> 32)) & (MAX_PIECES - 1u);
    unsigned probes = 0;
    Piece* q;
    const int active = (p->source == ORRE_UI2D_SPRITE || p->source == ORRE_UI2D_WINDOW) && p->window != 0 &&
                       p->window == p->activeWindow;
    s16 x1 = (s16)(p->x + p->w), y1 = (s16)(p->y + p->h);
    check_battle();
    if (!s_placeOpen) {
        place_start();
    }
    while (s_pieces[i].used && !same_piece(&s_pieces[i], p)) {
        i = (i + 1u) & (MAX_PIECES - 1u);
        if (++probes >= MAX_PIECES) {
            ++s_overflow;
            return;
        }
    }
    q = &s_pieces[i];
    if (!q->used) {
        if (s_pieceCount * 4 >= MAX_PIECES * 3) { /* keep the table at most 3/4 full */
            ++s_overflow;
            return;
        }
        q->used = 1;
        q->source = p->source, q->window = p->window, q->texture = p->texture, q->primitive = p->primitive;
        q->flags = p->flags;
        q->item = p->item, q->cropX = p->cropX, q->cropY = p->cropY, q->caller = (uintptr_t)p->caller;
        q->keyX = p->x, q->keyY = p->y, q->keyW = p->w, q->keyH = p->h;
        q->seq = ++s_seq;
        q->firstFrame = frame;
        q->lastFrame = frame - 1u; /* so this frame counts below */
        q->x0 = p->x, q->y0 = p->y, q->x1 = x1, q->y1 = y1;
        ++s_pieceCount;
        fprintf(stderr, "[ui2d] new n=%d f=%u room=0x%X battle=%u src=%s win=0x%X active=%d item=%d tex=0x%08X "
                        "prim=0x%X crop=%d,%d rect=%d,%d,%d,%d rec=%d flags=%u caller=0x%llX\n",
                q->seq, (unsigned)frame, s_room, s_battle, source_name(p->source), p->window, active, (int)p->item,
                p->texture, p->primitive, p->cropX, p->cropY, p->x, p->y, p->w, p->h, (int)p->recordMode,
                (unsigned)p->flags, (unsigned long long)(uintptr_t)p->caller);
    }
    if (p->x < q->x0) q->x0 = p->x;
    if (p->y < q->y0) q->y0 = p->y;
    if (x1 > q->x1) q->x1 = x1;
    if (y1 > q->y1) q->y1 = y1;
    if (q->lastFrame != frame) { /* once a frame (a piece drawn twice in one frame counts once) */
        q->lastFrame = frame;
        ++q->frames;
        if (active) ++q->activeFrames;
        if (p->activeWindow == 0) ++q->idleFrames;
    }
}

/* port/src/pc_game_main.c's room callback: a new room is a new place */
void pc_ui_inventory_room_entered(unsigned room) {
    if (!s_on) {
        return;
    }
    place_end();
    s_room = room;
}

static void at_exit(void) {
    place_end();
    fprintf(stderr, "[ui2d] off at exit, frame %u, %d pieces listed in all\n", (unsigned)pc_vi_heartbeat, s_seq);
}

/* port/src/pc_game_main.c, at start-up: ORRE_UI_INVENTORY=1 sets the hook (a test switch: off by default, never a
 * setting). With anything else the hook stays NULL and the game code does no reads at all. */
void pc_ui_inventory_init(void) {
    const char* v = getenv("ORRE_UI_INVENTORY");
    if (v == NULL || strcmp(v, "1") != 0) {
        return;
    }
    s_pieces = (Piece*)calloc(MAX_PIECES, sizeof(Piece));
    if (s_pieces == NULL) {
        fprintf(stderr, "[ui2d] ORRE_UI_INVENTORY=1: no memory for the table; off\n");
        return;
    }
    s_on = 1;
    OrreHook_Ui2DDraw = observe;
    atexit(at_exit);
    fprintf(stderr, "[ui2d] on (ORRE_UI_INVENTORY=1): format 1. One \"new\" line per 2D piece per room or battle, one "
                    "\"end\" line per piece when it ends (docs/port/setup.md, \"The 2D inventory\"); observation only\n");
}
