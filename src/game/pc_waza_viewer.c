/**
 * @file pc_waza_viewer.c
 * @brief OrrePort only: boot straight into the developers' move viewer with one Pokémon on show, as a reference camera
 *        for OrrePort Studio's own model renderer (the project's helper, 2026-11-18; requested by Studio).
 *
 * Why: Studio draws the battle models with its own renderer and needs the game's picture of the same model, on the same
 * camera, to compare against. The game already has the right tool: Genius Sonority's move viewer (debug room 0x3E7,
 * `waza_viewer`; docs/engine/models-animation.md "The developers' move viewer"), which spawns a Pokémon's battle model
 * on the battle grid, with a fixed orbit camera aimed at it. Retail only reaches it from a debug-menu entry whose menu
 * table is gone, and its Pokémon came from that menu writing the viewer's work area, so this file stands in for both:
 *   - OrreHook_FirstRoom: the first room after start-up is 0x3E7 instead of the Pokémon logo (no title, no memory card);
 *   - OrreHook_WazaViewerThread: fills the work area's command fields once (the model, shiny, one model with the
 *     viewer's own camera), then sets the optional camera angle and holds the animation frame;
 *   - OrreHook_WazaViewerDraw: draws the viewer's own flat backdrop (wazaViewerDrawBackdrop, the game's function) and
 *     prints the camera once.
 * Debug switches only (environment variables, off by default, never in any Settings page); with ORRE_WAZA_VIEWER unset
 * no hook is registered and the game runs exactly as on the GameCube. docs/port/setup.md, "Booting into the move
 * viewer"; hooks in docs/port/hook-points.md.
 *
 * Not part of the GameCube build (configure.py doesn't list it); port/tools/firstlink.py compiles it with the game units
 * (PORT_GAME_SOURCES). Platforms: plain C and getenv, the same on Windows, Linux and macOS (verified on Windows only).
 */
#ifdef TARGET_PC

#include "game/battle/battle_waza_types.h"
#include "game/constants/pokemon_status.h"
#include "game/gs_render_util.h" /* GSRenderCamera: the render camera's HSD camera object */
#include "game/gs_model_struct.h"  /* GSmodel: the animation fields and flags printed with ORRE_WAZA_VIEWER_JOINTS */
#include "game/gs_part.h"          /* GSpart: a part's joint */
#include "hsd/hsd_jobj.h"          /* HSD_JObj: the joint fields printed with ORRE_WAZA_VIEWER_JOINTS */
#include "hsd/hsd_lobj.h"          /* HSD_LObj: the lights printed with ORRE_WAZA_VIEWER_LIGHTS */
#include "hsd/hsd_robj.h"          /* HSD_RObj: each printed joint's constraints (IK hints, targets, limits) */
#include "hsd/hsd_dobj.h"          /* HSD_DObj / HSD_MObj / HSD_TObj: ORRE_WAZA_VIEWER_TEXLOG's material and textures */
#include "hsd/hsd_state.h"         /* HSD's render state cache: ORRE_WAZA_VIEWER_TEXLOG's PE line */
#include "game/gs_material_mobj.h" /* GSmatObj / GSmaterial: GS's material class and its extensions (TEXLOG) */
#include "dolphin/mtx.h"           /* C_MTXInverse / C_MTXConcat: ORRE_WAZA_VIEWER_DRAWMTX's model-space line */
#include "sysdolphin/baselib/psstructs.h" /* HSD_Generator / HSD_PSCmdList: ORRE_WAZA_VIEWER_PTCLHOOK's generator list */
#include "game/world/field_particle.h"    /* FieldParticleNode: ORRE_WAZA_SWEEP_MODELLOG's particle entries */
#include "pc/orre_hooks.h"
#include "pc/disc_types.h" /* DISC_U32: the floor table is big-endian common_rel data on the PC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void OSReport(const char*, ...);
extern u8 lbl_804673F8[];             /* the viewer's work area (0x884 bytes) */
extern void cameraSetRotY(f32 angle); /* camera.c: the orbit camera's yaw, radians */
extern void cameraSetDistance(f32 distance);
extern void cameraSetHeight(f32 height);
extern void cameraSetTargetOfsXYZ(f32 x, f32 y, f32 z); /* camera.c: the aim offset above the target ("view") */
extern f32 cameraGetRotY(void);
extern f32 cameraGetDistance(void);
extern f32 cameraGetHeight(void);
extern void GSmodelSetTexAnimFrame(void* model, f32 frame);
extern void GSmodelSetTexAnimRate(void* model, f32 rate);
extern void GSmodelGetScale(void* model, f32* out);
/* HSD's camera object (src/hsd/cobj.c); declared here with void* so this file needs no HSD header */
extern f32 HSD_CObjGetFov(void* cobj);
extern f32 HSD_CObjGetAspect(void* cobj);
extern f32 HSD_CObjGetNear(void* cobj);
extern f32 HSD_CObjGetFar(void* cobj);

#define PC_WAZA_VIEWER_ROOM 0x3E7 /* waza_viewer (docs/engine/menu-rooms.md) */
#define PC_PI 3.14159265358979f

/* The same bit-field as wazaViewer.c's WazaViewerCtrl (work +0x08) and gs_party_access.c's PartyFlags8, which the
 * developers' debug-menu entries toggle: declared in the same order with the same compiler, so the bits land where the
 * game reads them on the PC too (GCC places the first field in the low bit, MWCC in the high bit; both sides of the
 * PC build use GCC). advance = the attachment-point dots, bounds = the flat backdrop. */
typedef struct PcWazaViewerCtrl {
    s8 advance : 1;
    s8 paused : 1;
    s8 overlay : 1;
    s8 bounds : 1;
    s8 unused : 4;
} PcWazaViewerCtrl;

static int s_species;     /* internal species id (ORRE_WAZA_VIEWER) */
static u32 s_model;       /* its model (PKX) id, the index wazaSequencePokemonCreate takes */
static int s_shiny;       /* ",shiny": the game's own shiny path (a rare_ model or the shiny colours) */
static int s_backdrop;    /* ORRE_WAZA_VIEWER_BACKDROP=1 */
static int s_dots;        /* ORRE_WAZA_VIEWER_DOTS=1 */
static int s_hold;        /* ORRE_WAZA_VIEWER_FRAME given */
static f32 s_holdFrame;
static int s_haveAngle, s_haveDistance, s_haveHeight, s_haveFov, s_haveAimY;
static f32 s_angle, s_distance, s_height, s_fov, s_aimY;
static f32 s_aimView[3];  /* ORRE_WAZA_VIEWER_AIM_Y: the aim offset kept in the camera's "view" vector every pass */
static f32 s_aimHeightFrom, s_aimHeightTo; /* the orbit height before / after the aim change (logged) */
#define PC_WAZA_VIEWER_MAX_JOINTS 32
static int s_joints[PC_WAZA_VIEWER_MAX_JOINTS]; /* ORRE_WAZA_VIEWER_JOINTS: the part (joint) indices to print */
static int s_jointCount;
static int s_hide[PC_WAZA_VIEWER_MAX_JOINTS]; /* ORRE_WAZA_VIEWER_HIDE: the part (joint) indices drawn hidden */
static int s_hideCount;
static int s_hideLogged;
static int s_lights;      /* ORRE_WAZA_VIEWER_LIGHTS=1 */
static int s_ptclFrames;  /* ORRE_WAZA_VIEWER_PTCL=<frames>: the live particle count per viewer frame, for that many */
static int s_ptclDone;
static int s_hookFrames;  /* ORRE_WAZA_VIEWER_PTCLHOOK=<frames>: emitter starts and live generators, for that many */
static int s_hookFrame;   /* the viewer frame the next summary line is for (hook lines carry the same number) */
static int s_hookCount;   /* emitter starts logged since the last summary line */
static int s_drawMtx = -1; /* ORRE_WAZA_VIEWER_DRAWMTX: the part (joint) index whose draw matrices are logged once */
static HSD_JObj* s_drawMtxJObj;  /* that joint, found when the log is armed */
static HSD_JObj* s_drawMtxRoot;  /* part 0, for naming joints with the same numbers */
static int s_drawMtxState;       /* 0 not armed yet, 1 armed (logging this draw), 2 done */
static int s_drawMtxFrames, s_drawMtxPObjs;
static int s_texLog;             /* ORRE_WAZA_VIEWER_TEXLOG=1: one frame's drawn PObjs, their materials and textures */
static int s_texLogState;        /* 0 not armed yet, 1 armed (logging this draw), 2 done */
static int s_texLogFrames, s_texLogDraws;
static int s_sweepMode;          /* ORRE_WAZA_SWEEP is driving (the viewer's own owner slots are unused then) */
static void* s_owner[2];         /* the sweep's two owners (attacker, target); defined with the sweep below */
static int s_modelLog;           /* ORRE_WAZA_SWEEP_MODELLOG=1: the effect-model / linked-particle log (sweep only) */
static int s_ptclLog;            /* ORRE_WAZA_SWEEP_PARTICLES=1: every live particle each frame a part plays (sweep only) */
static int s_sent;       /* the command was written into the work area */
static int s_spawned;     /* the model is up and the camera adjusted */
static int s_draws;       /* draw callbacks since the spawn (the camera is printed after a few) */
static int s_logged;

static u32 first_room(u32 room)
{
    OSReport("[waza_viewer] first room 0x%X -> 0x%X (waza_viewer)\n", (unsigned)room, PC_WAZA_VIEWER_ROOM);
    return PC_WAZA_VIEWER_ROOM;
}

/* The owner record in viewer slot 0, read the way the game code writes it (*(void**) at work +0x61C). Latent 64-bit
 * note (likely): the viewer stores its two owners with 4-byte steps but pointer-sized writes; on the PC the owners live
 * below 4 GB (GS heap in MEM1), so the upper half written over slot 1 is 0, the same as an empty slot 1. */
static void* viewer_owner(const u8* work)
{
    return *(void* const*)(work + 0x61C);
}

/* ---- ORRE_WAZA_VIEWER_PTCLHOOK=<frames> (2026-11-19, Studio's particle-count question) ----
 * Why: Studio ports the game's particle system, and its idle particle counts come out too low for the random-rate
 * species (Moltres, Articuno) while fixed-rate ones match, so the question is how often the game starts the model's
 * emitters and how many of them run at once. Where emitters start: a model's animation key of HSD type 0x28 (an FObj
 * key, HSD's JObjUpdateFunc in hsd/jobj.c, dptcl_callback) or a static particle joint drawn by HSD_JObjDisp
 * (hsd/displayfunc.c, sptcl_callback) both call gsParticlePtclHook (field_range_80117E58.c), which finds the model and
 * part and makes the generator with gsParticleCreate -> psCreateGeneratorID. That function calls the pointer below,
 * set only while this log runs, after the generator is made. Each viewer frame (after the sequence system's update,
 * the same place and numbering as ORRE_WAZA_VIEWER_PTCL) prints a summary: the model's animation frame, how many
 * emitter starts came since the last line, and every live generator of the model's particle bank (HSD's
 * activeGenerator list, generator.c): its id, its record index in the bank, its remaining genLife (psExecGenerator
 * counts it down once per particle step; 0 = endless) over the record's genLife, and its live particles (numChild).
 * Observe-only: everything is read, nothing written. A test log, not a hook (no OrreHook_ name). Plain C and OSReport,
 * every OS (verified on Windows only). */
void (*pc_waza_viewer_ptclhook_log)(const void* model, u32 key_bank, u32 gen_index, u32 part, const void* jobj,
                                    const void* generator);
extern HSD_Generator* activeGenerator;                /* generator.c: the live generators */
extern HSD_PSCmdList** lbl_80452AC8[PS_NUM_BANK];     /* particle.c (psCmdList): each bank's generator records */
extern s32 lbl_80452CC8[PS_NUM_BANK];                 /* particle.c (psCmdListArray): their counts */
extern void* GSmodelGetLinkedGSparticleBank(GSmodel* model); /* the model's FieldParticleBank, or NULL */

/* The first two bytes of FieldParticleBank (include/game/world/field_particle.h): in use, and the particle data bank
 * (psCreateGeneratorID's bank, gsParticleCreate's texture[1]); repeated here so this unit needs no field header */
typedef struct PcPtclBankHead {
    u8 active;
    u8 psBank;
} PcPtclBankHead;

static int ptclhook_bank(void* model)
{
    const PcPtclBankHead* b =
        model != NULL ? (const PcPtclBankHead*)GSmodelGetLinkedGSparticleBank((GSmodel*)model) : NULL;
    return b != NULL ? (int)b->psBank : -1;
}

/* the record's genLife (what psCreateGeneratorID copied into the generator when it was made) */
static unsigned ptclhook_full_life(const HSD_Generator* g)
{
    if (g->bank < PS_NUM_BANK && lbl_80452AC8[g->bank] != NULL && (s32)g->gfxIdx < lbl_80452CC8[g->bank] &&
        lbl_80452AC8[g->bank][g->gfxIdx] != NULL) {
        return lbl_80452AC8[g->bank][g->gfxIdx]->genLife;
    }
    return 0;
}

static void ptclhook_log(const void* model, u32 keyBank, u32 genIndex, u32 part, const void* jobj, const void* gen)
{
    /* which model started the emitter: the one-model viewer's model, or in the sweep (2026-11-20: the log was
     * silent there) the attacker's or the target's; the viewer's own owner slots are unused while the sweep drives */
    void* owner = s_sweepMode ? NULL : viewer_owner(lbl_804673F8);
    const void* viewerModel = owner != NULL ? PTR32_AT_T(void, (u8*)owner, 0x24) : NULL;
    const void* attackerModel = s_owner[0] != NULL ? PTR32_AT_T(void, (u8*)s_owner[0], 0x24) : NULL;
    const void* targetModel = s_owner[1] != NULL ? PTR32_AT_T(void, (u8*)s_owner[1], 0x24) : NULL;
    const char* who = model == NULL                                ? "no model"
                      : !s_sweepMode && model == viewerModel       ? "viewer model"
                      : s_sweepMode && model == attackerModel      ? "attacker model"
                      : s_sweepMode && model == targetModel        ? "target model"
                                                                   : "other model";
    const HSD_Generator* g = (const HSD_Generator*)gen;

    (void)jobj;
    s_hookCount++;
    if (g != NULL) {
        OSReport("[waza_viewer] ptclhook frame %d start: %s, anim frame %.2f, part %u, key bank %u gen %u -> generator id "
                 "%u (bank %u gen %u link %u) genLife %u, particle life %u, kind 0x%X, rate %.4f%s\n",
                 s_hookFrame, who,
                 model != NULL ? ((const GSmodel*)model)->animFrame : -1.0f, (unsigned)part, (unsigned)keyBank,
                 (unsigned)genIndex, (unsigned)g->idnum, (unsigned)g->bank, (unsigned)g->gfxIdx, (unsigned)g->linkNo,
                 (unsigned)g->genLife, (unsigned)g->life, (unsigned)g->kind, g->random,
                 g->random < 0.0f ? " (fixed rate)" : " (random rate)");
    } else {
        OSReport("[waza_viewer] ptclhook frame %d start: %s, anim frame %.2f, part %u, key bank %u gen %u -> no generator "
                 "made (bank off, no free slot, or no such record)\n",
                 s_hookFrame, who,
                 model != NULL ? ((const GSmodel*)model)->animFrame : -1.0f, (unsigned)part, (unsigned)keyBank,
                 (unsigned)genIndex);
    }
}

/* one summary per viewer frame, then the next frame's number; disarms itself after s_hookFrames lines */
static void ptclhook_frame(void* model)
{
    extern u16 numActiveParticles;
    char list[3072];
    size_t len = 0;
    int bank = ptclhook_bank(model);
    int total = 0, mine = 0;
    unsigned children = 0;
    const HSD_Generator* g;

    list[0] = '\0';
    if (s_hookFrame == 0) {
        OSReport("[waza_viewer] ptclhook: per viewer frame (= 2 particle steps): emitter starts ('start' lines, "
                 "gsParticlePtclHook), then the model's bank's live generators: id, record (gen), remaining genLife / "
                 "the record's genLife (0 = endless), live particles (child)\n");
    }
    for (g = activeGenerator; g != NULL; g = g->next) {
        total++;
        if ((int)g->bank != bank) {
            continue;
        }
        mine++;
        children += (unsigned)g->numChild;
        if (len < sizeof list - 128) {
            int n = snprintf(list + len, sizeof list - len, " [id %u gen %u life %u/%u child %u%s]", (unsigned)g->idnum,
                             (unsigned)g->gfxIdx, (unsigned)g->genLife, ptclhook_full_life(g), (unsigned)g->numChild,
                             (g->kind & 0x800) ? " ended, kept" : "");
            if (n > 0) {
                len += (size_t)n;
            }
        }
    }
    OSReport("[waza_viewer] ptclhook frame %d anim frame %.2f: %d start(s); generators live %d (%d in the model's bank "
             "%d, with %u particles); particles alive %u (all)\n",
             s_hookFrame, model != NULL ? ((const GSmodel*)model)->animFrame : -1.0f, s_hookCount, total, mine, bank,
             children, (unsigned)numActiveParticles);
    if (mine > 0) {
        OSReport("[waza_viewer] ptclhook frame %d generators:%s\n", s_hookFrame, list);
    }
    s_hookCount = 0;
    if (++s_hookFrame >= s_hookFrames) {
        pc_waza_viewer_ptclhook_log = NULL;
        OSReport("[waza_viewer] ptclhook: done (%d frames)\n", s_hookFrames);
    }
}

static void viewer_thread(u8* work)
{
    void* owner;
    void* model;

    if (*(const s32*)work != 1) {
        return; /* mode 0 (the developer build kind 2's host-file mode) never runs on retail; nothing to drive */
    }
    if (!s_sent) {
        PcWazaViewerCtrl* ctrl = (PcWazaViewerCtrl*)(work + 0x08);
        /* the species' model (PKX) id from the game's own species table (common_rel), looked up here rather than at
         * start-up because the table is published only once common_rel is linked; the same lookup the battle
         * send-out makes (fight_out_pokemon: POKEMON_STATUS_MODEL_ID) */
        s_model = pokemonGetStatus(NULL, (u32)s_species, POKEMON_STATUS_MODEL_ID, 0);
        s_sent = 1;
        if (s_model == 0) {
            OSReport("[waza_viewer] species %d has no battle model (model id 0); nothing spawned\n", s_species);
            return;
        }
        /* the fields wazaViewerUpdateModels reads (wazaViewer.c, wazaViewerSpawnAttacker): +0x630 grid slot (nonzero =
         * a Pokémon; 1 = the first side's distance), +0x634 the model id, +0x640 shiny (its third argument),
         * +0x650 / +0x654 a move animation (none), +0x648 no second model, +0x63C one model with the viewer's camera
         * (mode 5, aimed at it; A would play motion +0x638), +0x628 the reload request that spawns it */
        *(s32*)(work + 0x630) = 1;
        *(s32*)(work + 0x634) = (s32)s_model;
        *(s32*)(work + 0x640) = s_shiny ? 1 : 0;
        *(s32*)(work + 0x650) = 0;
        *(s32*)(work + 0x654) = 0;
        *(s32*)(work + 0x644) = 0;
        *(s32*)(work + 0x648) = 0;
        *(s32*)(work + 0x638) = 0;
        *(s32*)(work + 0x63C) = 1;
        *(s32*)(work + 0x658) = 0;
        ctrl->advance = s_dots ? 1 : 0;
        ctrl->bounds = s_backdrop ? 1 : 0;
        *(s32*)(work + 0x628) = 1;
        OSReport("[waza_viewer] spawn species %d (model id %u)%s%s%s\n", s_species, (unsigned)s_model,
                 s_shiny ? ", shiny" : "", s_backdrop ? ", backdrop" : "", s_dots ? ", dots" : "");
        return;
    }
    owner = viewer_owner(work);
    if (owner == NULL) {
        if (s_spawned == 0 && *(const s32*)(work + 0x628) == 0) {
            OSReport("[waza_viewer] the model did not spawn (species %d, model id %u)\n", s_species, (unsigned)s_model);
            s_spawned = -1;
        }
        return;
    }
    model = PTR32_AT_T(void, (u8*)owner, 0x24);
    if (s_spawned != 1) {
        /* once, right after the spawn set the viewer's camera (mode 5, distance 70, height 30, yaw 0): the optional
         * overrides go in before this pass's cameraUpdate, which places the camera from them */
        if (s_haveAngle) {
            cameraSetRotY(s_angle * (PC_PI / 180.0f));
        }
        if (s_haveDistance) {
            cameraSetDistance(s_distance);
        }
        if (s_haveHeight) {
            cameraSetHeight(s_height);
        }
        if (s_haveFov) {
            cameraSetFov(s_fov);
        }
        if (s_haveAimY) {
            /* ORRE_WAZA_VIEWER_AIM_Y: why: the orbit camera (mode 5, _cameraPadRotateUpdate) aims at the target model's
             * position plus the camera state's "view" offset, which cameraInit sets to (0, 14, 0) and nothing in the
             * viewer changes; its eye is that aim point plus (0, height, distance) turned by the yaw. A small Pokémon
             * under a narrow field of view sits below that aim and leaves the frame. So the aim's height is moved to
             * the requested world height, and the orbit height is moved the other way by the same amount, so the eye
             * stays exactly where the other switches put it (the camera only tilts; yaw and distance unchanged).
             * Verified from camera.c (cameraInit, cameraUpdate case 5, _cameraPadRotateUpdate). All platforms */
            f32 pos[3] = { 0.0f, 0.0f, 0.0f };
            f32 view[3];
            if (model != NULL) {
                GSmodelGetPosition((struct GSmodel*)model, (struct GSvec*)pos);
            }
            GSscene_GetCameraViewVector((struct GSSceneVec3*)view);
            s_aimHeightFrom = cameraGetHeight();
            s_aimView[0] = view[0];
            s_aimView[1] = s_aimY - pos[1];
            s_aimView[2] = view[2];
            s_aimHeightTo = s_aimHeightFrom + (view[1] - s_aimView[1]);
            cameraSetTargetOfsXYZ(s_aimView[0], s_aimView[1], s_aimView[2]);
            cameraSetHeight(s_aimHeightTo);
        }
        /* the screen is still black from the fade-out before the first room (the logo room fades itself in; the
         * viewer room has no script that does): fade in, as the game's fades do (type 2, half a second) */
        fadeSet(2, 0.5f);
        s_spawned = 1;
    } else if (s_haveAimY) {
        /* every later pass too: a room (re)load's floor init writes its own view offset (floor.c, sFloorCameraView),
         * so the aim is kept here rather than trusted to stay; the orbit height needs no repeat (the orbit keeps it) */
        cameraSetTargetOfsXYZ(s_aimView[0], s_aimView[1], s_aimView[2]);
    }
    if (s_hold && model != NULL) {
        /* hold the model on one frame of the animation it plays (its idle) and of its texture animation, every
         * pass, after the sequence system's update, so a dump is the same frame whatever frame it is taken on */
        GSmodelSetAnimRate((struct GSmodel*)model, 0.0f);
        GSmodelSetAnimFrame((struct GSmodel*)model, s_holdFrame);
        GSmodelSetTexAnimRate(model, 0.0f);
        GSmodelSetTexAnimFrame(model, s_holdFrame);
    }
    if (s_ptclFrames > 0 && s_ptclDone < s_ptclFrames) {
        /* ORRE_WAZA_VIEWER_PTCL: why: Studio ports the game's particle system and compares how many particles a
         * species' idle keeps alive (2026-11-19: Moltres, Articuno, Vaporeon spawn at random rates, so the count's
         * average and range over time is what can be compared; an exact match would need the shared random state). One
         * line per viewer frame from the spawn on, after the sequence system's update: the game's own live counter
         * (numActiveParticles, particle.c). Observe-only; plain C, every OS */
        extern u16 numActiveParticles;
        OSReport("[waza_viewer] ptcl frame %d alive %u\n", s_ptclDone, (unsigned)numActiveParticles);
        s_ptclDone++;
    }
    if (s_hookFrames > 0 && s_hookFrame < s_hookFrames) {
        ptclhook_frame(model); /* ORRE_WAZA_VIEWER_PTCLHOOK (above viewer_thread) */
    }
    if (s_hideCount > 0 && model != NULL) {
        /* ORRE_WAZA_VIEWER_HIDE: why: Studio compares its drawing of one part of a model (Lunatone's jaw, 2026-11-19)
         * with the game's, and the other parts can cover it. The game's model draw skips a joint's polygon objects when
         * the joint has HSD's hidden flag (JOBJ_HIDDEN, 0x10: HSD's fn_80197784 under HSD_JObjDisp returns at once,
         * hsd/displayfunc.c; corrected 2026-11-19 from _modelParseJObjDispDObj, GS's walk, which only the bounds pass
         * uses), and only that joint's own, not its children's. So the listed joints get the flag, every
         * pass (cheap, and safe if anything clears it). Matrices and posing are untouched. Plain C, every OS */
        int i;
        GSmodelWalkJObjs((GSmodel*)model);
        for (i = 0; i < s_hideCount; i++) {
            GSpart* part = GSmodelGetPart((GSmodel*)model, s_hide[i]);
            if (part == NULL) {
                continue;
            }
            if (part->jobj != NULL) {
                part->jobj->flags |= JOBJ_HIDDEN;
            } else if (!s_hideLogged) {
                OSReport("[waza_viewer] ORRE_WAZA_VIEWER_HIDE: joint %d: no such joint\n", s_hide[i]);
            }
            GSpartFree(part);
        }
        if (!s_hideLogged) {
            OSReport("[waza_viewer] hiding %d joint(s) (ORRE_WAZA_VIEWER_HIDE): their polygon objects aren't drawn\n",
                     s_hideCount);
            s_hideLogged = 1;
        }
    }
}

/* The animation the model is playing, and (ORRE_WAZA_VIEWER_JOINTS) the listed joints' matrices and SRT. Why: Studio
 * poses the same model with its own code and needs the game's numbers for the same joints at the same frame to find
 * where they differ (2026-11-19, Studio's request: the jaw joints' uneven scales, or a different animation than the
 * idle). Joint numbering is the game's own part index (GSmodelGetPart): HSD_JObjWalkTree's depth-first order (a joint,
 * then its children in order, then its next sibling) from the model's root joint = 0, with the null root that model
 * loading may add (GSmodel flag 0x20000) skipped, so index i is the disc model's own i-th joint in that order.
 * Verified from gs_part_exact_800EE150.c and hsd/jobj.c. Plain C, all platforms. */
/* The part index of `target` under `jobj` (index *next), in HSD_JObjWalkTree's order: the joint, then its children
 * (not below an instance joint), then its next sibling; -1 when not found. Used to name the joint a constraint points
 * at with the same numbers as ORRE_WAZA_VIEWER_JOINTS. */
static int joint_index_of(HSD_JObj* jobj, HSD_JObj* target, int* next)
{
    for (; jobj != NULL; jobj = jobj->next) {
        int found;
        if (jobj == target) {
            return *next;
        }
        (*next)++;
        if (!(jobj->flags & JOBJ_INSTANCE)) {
            found = joint_index_of(jobj->child, target, next);
            if (found >= 0) {
                return found;
            }
        }
    }
    return -1;
}

/* Each RObj (HSD's constraint record) on a printed joint. Why (2026-11-19): HSD only uses a constraint whose
 * enable bit (0x80000000) is set (HSD_RObjGetByType and HSD_RObjGetGlobalPosition skip it otherwise; the bit comes
 * from the disc record or the RObj's animation track, RObjUpdateFunc), and resolveIKJoint1 / _2 skip the IK solve when
 * the chain's effector has no enabled target (type 0x10000000 = joint reference, subtype 1). So printing the bits says
 * whether the solve can run. Verified from hsd/robj.c and hsd/jobj.c. */
static void log_robjs(int n, HSD_JObj* jobj, HSD_JObj* root)
{
    static const char* const kRefTypes[5] = { "expression", "joint", "limit", "bytecode", "IK hint" };
    HSD_RObj* rp;
    int k = 0;

    if (jobj->robj == NULL) {
        OSReport("[waza_viewer] joint %d robj none\n", n);
        return;
    }
    for (rp = jobj->robj; rp != NULL; rp = rp->next, k++) {
        u32 type = (rp->flags & ROBJ_TYPE_MASK) >> 28;
        u32 sub = rp->flags & 0x0FFFFFFF;
        const char* enabled = (rp->flags & 0x80000000) ? "enabled" : "DISABLED";
        if ((rp->flags & ROBJ_TYPE_MASK) == REFTYPE_JOBJ) {
            int counter = 0;
            int idx = rp->u.jobj != NULL ? joint_index_of(root, rp->u.jobj, &counter) : -1;
            OSReport("[waza_viewer] joint %d robj %d: joint reference subtype %u (%s) -> joint %d\n", n, k,
                     (unsigned)sub, enabled, idx);
        } else if ((rp->flags & ROBJ_TYPE_MASK) == REFTYPE_IKHINT) {
            OSReport("[waza_viewer] joint %d robj %d: IK hint (%s) bone length %.6f rotate x %.6f\n", n, k, enabled,
                     rp->u.ik_hint.bone_length, rp->u.ik_hint.rotate_x);
        } else if ((rp->flags & ROBJ_TYPE_MASK) == REFTYPE_LIMIT) {
            OSReport("[waza_viewer] joint %d robj %d: limit subtype %u (%s) value %.6f\n", n, k, (unsigned)sub,
                     enabled, rp->u.limit);
        } else {
            OSReport("[waza_viewer] joint %d robj %d: %s subtype %u (%s)\n", n, k, type < 5 ? kRefTypes[type] : "other",
                     (unsigned)sub, enabled);
        }
    }
}

static void log_animation_and_joints(GSmodel* model)
{
    s32 count;
    int i;
    GSpart* rootPart;
    HSD_JObj* root;

    OSReport("[waza_viewer] animation: index %u type %u (of %u animations) frame %.4f end %.4f rate %.4f; texture "
             "animation index %u (of %u) frame %.4f; blending %s (indices %u / %u, factor %.4f); model flags 0x%08X\n",
             (unsigned)model->animIndex, (unsigned)model->animType, (unsigned)model->animCount, model->animFrame,
             model->animEndFrame, model->animRate, (unsigned)model->texAnimIndex, (unsigned)model->texAnimCount,
             model->texAnimFrame, (model->flags.raw & MODEL_FLAG_BLENDING) ? "yes" : "no",
             (unsigned)model->blendAnimIndexA, (unsigned)model->blendAnimIndexB, model->blendFactor,
             (unsigned)model->flags.raw);
    if (s_jointCount == 0) {
        return;
    }
    count = GSmodelWalkJObjs(model);
    rootPart = GSmodelGetPart(model, 0); /* part 0: the root the numbering starts from (an added null root skipped) */
    root = rootPart != NULL ? rootPart->jobj : NULL;
    if (rootPart != NULL) {
        GSpartFree(rootPart);
    }
    OSReport("[waza_viewer] joints: numbered by the game's part index (HSD_JObjWalkTree depth-first: joint, children, "
             "next sibling; root = 0%s); %d joints; mtx = the joint's world matrix (model placement included)\n",
             (model->flags.raw & GSMODEL_FLAG_ROOT_NULL_ADDED) ? "; the null root added at load is skipped" : "",
             (int)count);
    for (i = 0; i < s_jointCount; i++) {
        int n = s_joints[i];
        GSpart* part = GSmodelGetPart(model, n);
        HSD_JObj* jobj = part != NULL ? part->jobj : NULL;
        int r;

        if (jobj == NULL) {
            OSReport("[waza_viewer] joint %d: no such joint\n", n);
            if (part != NULL) {
                GSpartFree(part);
            }
            continue;
        }
        /* the matrix as the draw built it (the same call a draw makes when the joint is dirty) */
        HSD_JObjSetupMatrix(jobj);
        OSReport("[waza_viewer] joint %d flags 0x%08X (%s, %s)\n", n, (unsigned)jobj->flags,
                 (jobj->flags & JOBJ_USE_QUATERNION) ? "quaternion" : "euler",
                 (jobj->flags & JOBJ_CLASSICAL_SCALE) ? "classical scale" : "not classical scale");
        for (r = 0; r < 3; r++) {
            OSReport("[waza_viewer] joint %d mtx row %d: %.6f %.6f %.6f %.6f\n", n, r, jobj->mtx[r][0],
                     jobj->mtx[r][1], jobj->mtx[r][2], jobj->mtx[r][3]);
        }
        OSReport("[waza_viewer] joint %d local scale %.6f %.6f %.6f rotation %.6f %.6f %.6f %.6f translation %.6f %.6f "
                 "%.6f\n",
                 n, jobj->scale_x, jobj->scale_y, jobj->scale_z, jobj->rotate_x, jobj->rotate_y, jobj->rotate_z,
                 jobj->rotate_w, jobj->translate_x, jobj->translate_y, jobj->translate_z);
        if (jobj->scl != NULL) {
            /* HSD's accumulated scale (jobj->scl, the parent's scale carried down; made by HSD_JObjMakeMatrix) */
            OSReport("[waza_viewer] joint %d scl (accumulated) %.6f %.6f %.6f\n", n, jobj->scl[0], jobj->scl[1],
                     jobj->scl[2]);
        } else {
            OSReport("[waza_viewer] joint %d scl (accumulated) none\n", n);
        }
        if (jobj->envelopemtx != NULL) {
            /* the joint's envelope (inverse bind) matrix: a native copy of the disc joint's matrix made at load
             * (hsd/jobj.c JObjLoad); skinned vertices use mtx x envelopemtx (hsd/pobj.c). Only joints that weight
             * envelopes have one (2026-11-19, for Studio's skinning check) */
            for (r = 0; r < 3; r++) {
                OSReport("[waza_viewer] joint %d envelopemtx row %d: %.6f %.6f %.6f %.6f\n", n, r,
                         jobj->envelopemtx[r * 4 + 0], jobj->envelopemtx[r * 4 + 1], jobj->envelopemtx[r * 4 + 2],
                         jobj->envelopemtx[r * 4 + 3]);
            }
        } else {
            OSReport("[waza_viewer] joint %d envelopemtx none (null: the disc joint has no inverse bind matrix)\n", n);
        }
        log_robjs(n, jobj, root);
        GSpartFree(part);
    }
}

/* ORRE_WAZA_VIEWER_LIGHTS=1: HSD's active lights, as the latest HSD_LObjSetup (the scene's camera setup) left them,
 * and the light masks the materials use. Why: Studio reproduces the game's emboss bump mapping (HSD's emboss TEV setup
 * shifts the bump texture's coordinates toward the first diffuse light) and the specular stages, so it needs the
 * lights' own values (2026-11-19). Light N is HSD's active index (0..7; 8 is the ambient slot); "gx id" is the GX light
 * bit it was given, the bits the diffuse / specular masks are made of. Position and interest are the world positions
 * from the light's WObjs (HSD_LObjGetPosition / _GetInterest); "lvec" is HSD's view-space specular vector (only set
 * for specular lights). Verified from hsd/lobj.c (HSD_LObjSetup). Plain C, all platforms. */
static void log_lights(void)
{
    static const char* const kTypes[4] = { "ambient", "infinite", "point", "spot" };
    s32 nb = HSD_LObjGetNbActive();
    int i;

    OSReport("[waza_viewer] lights: %d active; diffuse mask 0x%02X, specular mask 0x%02X, alpha mask 0x%02X, attn-func "
             "mask 0x%02X (GX light bits)\n",
             (int)nb, (unsigned)HSD_LObjGetLightMaskDiffuse(), (unsigned)HSD_LObjGetLightMaskSpecular(),
             (unsigned)HSD_LObjGetLightMaskAlpha(), (unsigned)HSD_LObjGetLightMaskAttnFunc());
    for (i = 0; i < MAX_GXLIGHT; i++) {
        /* HSD_LObjGetActiveByIndex stops at 7; the ambient slot (8) is read through its GX id (GX_MAX_LIGHT, 0x100) */
        HSD_LObj* lobj = i < MAX_GXLIGHT - 1 ? HSD_LObjGetActiveByIndex(i) : HSD_LObjGetActiveByID(0x100);
        Vec pos = { 0.0f, 0.0f, 0.0f };
        Vec at = { 0.0f, 0.0f, 0.0f };
        u32 type;
        s32 havePos, haveAt;

        if (lobj == NULL) {
            continue;
        }
        type = lobj->flags & LOBJ_TYPE_MASK;
        havePos = HSD_LObjGetPosition(lobj, &pos);
        haveAt = HSD_LObjGetInterest(lobj, &at);
        OSReport("[waza_viewer] light %d: %s, flags 0x%04X (%s%s%s%s), priority %u, gx id 0x%X, spec id 0x%X; colour "
                 "%u %u %u %u (hardware %u %u %u %u); shininess %.4f\n",
                 i, kTypes[type], (unsigned)lobj->flags, (lobj->flags & LOBJ_DIFFUSE) ? "diffuse " : "",
                 (lobj->flags & LOBJ_SPECULAR) ? "specular " : "", (lobj->flags & LOBJ_ALPHA) ? "alpha " : "",
                 (lobj->flags & LOBJ_RAW_PARAM) ? "raw attenuation" : "",
                 (unsigned)lobj->priority, (unsigned)lobj->id, (unsigned)lobj->spec_id, lobj->color.r, lobj->color.g,
                 lobj->color.b, lobj->color.a, lobj->hw_color.r, lobj->hw_color.g, lobj->hw_color.b, lobj->hw_color.a,
                 lobj->shininess);
        OSReport("[waza_viewer] light %d position %s %.4f %.4f %.4f; interest %s %.4f %.4f %.4f; lvec %.6f %.6f %.6f\n", i,
                 havePos ? "(set)" : "(none)", pos.x, pos.y, pos.z, haveAt ? "(set)" : "(none)", at.x, at.y, at.z,
                 lobj->lvec.x, lobj->lvec.y, lobj->lvec.z);
        if (type == LOBJ_POINT || type == LOBJ_SPOT) {
            if (lobj->flags & LOBJ_RAW_PARAM) {
                OSReport("[waza_viewer] light %d attenuation a %.6f %.6f %.6f k %.6f %.6f %.6f\n", i, lobj->u.attn.a0,
                         lobj->u.attn.a1, lobj->u.attn.a2, lobj->u.attn.k0, lobj->u.attn.k1, lobj->u.attn.k2);
            } else {
                OSReport("[waza_viewer] light %d attenuation cutoff %.4f spot func %u ref brightness %.4f ref distance "
                         "%.4f dist func %u\n",
                         i, lobj->u.spot.cutoff, (unsigned)lobj->u.spot.spot_func, lobj->u.spot.ref_br,
                         lobj->u.spot.ref_dist, (unsigned)lobj->u.spot.dist_func);
            }
        }
    }
}

static void log_camera(void* owner)
{
    GSRenderCamera* cam = (GSRenderCamera*)GSresGetResource(0, 0);
    void* model = PTR32_AT_T(void, (u8*)owner, 0x24);
    f32 pos[3], rot[3], scale[3];
    int r;

    if (cam == NULL || cam->cobj == NULL) {
        OSReport("[waza_viewer] camera: no render camera\n");
        return;
    }
    /* the GS camera's own eye / interest / up (what GScameraSetPosition / GScameraLookAt set and the view matrix is
     * built from), not the HSD camera object's: the orbit camera never writes the HSD interest (found 2026-11-18: the
     * object kept (0, 0, 1) and a meaningless up) */
    OSReport("[waza_viewer] camera eye %.4f %.4f %.4f interest %.4f %.4f %.4f up %.4f %.4f %.4f\n", cam->eye.x,
             cam->eye.y, cam->eye.z, cam->interest.x, cam->interest.y, cam->interest.z, cam->upVector.x,
             cam->upVector.y, cam->upVector.z);
    for (r = 0; r < 3; r++) {
        OSReport("[waza_viewer] camera view matrix row %d: %.6f %.6f %.6f %.6f\n", r, cam->viewMtx[r][0],
                 cam->viewMtx[r][1], cam->viewMtx[r][2], cam->viewMtx[r][3]);
    }
    OSReport("[waza_viewer] camera fov %.4f (degrees, vertical) aspect %.4f near %.4f far %.4f; orbit yaw %.4f rad "
             "distance %.4f height %.4f\n",
             HSD_CObjGetFov(cam->cobj), HSD_CObjGetAspect(cam->cobj), HSD_CObjGetNear(cam->cobj),
             HSD_CObjGetFar(cam->cobj), cameraGetRotY(), cameraGetDistance(), cameraGetHeight());
    if (s_haveAimY) {
        /* the interest printed above is the one really used (the GS camera's); this line says how it was reached */
        OSReport("[waza_viewer] aim y %.4f (ORRE_WAZA_VIEWER_AIM_Y): view offset %.4f %.4f %.4f; orbit height %.4f -> "
                 "%.4f so the eye stays put\n",
                 s_aimY, s_aimView[0], s_aimView[1], s_aimView[2], s_aimHeightFrom, s_aimHeightTo);
    }
    if (model != NULL) {
        GSmodelGetPosition((struct GSmodel*)model, (struct GSvec*)pos);
        GSmodelGetRotation((struct GSmodel*)model, (struct GSvec*)rot);
        GSmodelGetScale(model, scale);
        OSReport("[waza_viewer] model position %.4f %.4f %.4f rotation %.4f %.4f %.4f (radians) scale %.4f %.4f %.4f "
                 "anim frame %.2f\n",
                 pos[0], pos[1], pos[2], rot[0], rot[1], rot[2], scale[0], scale[1], scale[2],
                 GSmodelGetAnimFrame((struct GSmodel*)model));
        log_animation_and_joints((GSmodel*)model);
    }
    if (s_lights) {
        log_lights();
    }
}

/* ORRE_WAZA_VIEWER_DRAWMTX=<part>: the matrices the real draw uses for that joint's polygon objects (PObjs), logged
 * for one frame. Why (2026-11-19, Studio's Lunatone question): Studio draws Lunatone's body (joint 1's first PObj, an
 * envelope PObj whose vertices all use matrix slot 0 = joint 7's envelope) flat and face-on, the game turned; Studio
 * needs the game's own node matrix, skeleton joint, slot matrices and envelope list to see which step differs.
 * Where it is read: the battle model is drawn by HSD's own path (GSmodelDrawModel / GSmodelDrawAll ->
 * HSD_JObjDispAll -> HSD_JObjDisp -> HSD_PObjDisp -> PObjSetupMtx -> SetupEnvelopeModelMtx in src/hsd/pobj.c; GS
 * replaces only the material class), not by GS's own envelope loader (_modelParseLoadEnvelopeMatrix, reached only
 * through GSmodelParse, which only the bounds pass uses: gs_model_bound.c). Verified from the code. So pobj.c calls
 * this function through the pointer below, set only while the log is armed: once per PObj before its matrix setup
 * (slot -1; `loaded` = the position matrix a rigid / shared PObj loads) and once per envelope slot right after
 * SetupEnvelopeModelMtx's GXLoadPosMtxImm (`right` = _HSD_mkEnvelopeModelNodeMtx's node matrix or NULL, `skin` = the
 * slot's model matrix before the camera, `loaded` = what went into the GX position matrix). Observe-only: it reads and
 * prints, nothing the draw uses is written. A test log, not a hook (no OrreHook_ name, not in hook-points.md). Plain C
 * and OSReport, every OS (verified on Windows only). */
void (*pc_waza_viewer_drawmtx_log)(unsigned pobj_flags, const void* jobj, int slot, const void* envelope,
                                   f32 (*vmtx)[4], f32 (*right)[4], f32 (*skin)[4], f32 (*loaded)[4]);
extern HSD_JObj* HSD_JObjFindSkeleton(HSD_JObj* jobj); /* hsd/displayfunc.c: walks up to the first skeleton joint */

/* HSD's envelope entry (include/hsd/hsd_pobj.h, struct HSD_Envelope), repeated here because that header's AObj
 * declarations clash with this unit's other headers; the same three fields in the same order, native pointers */
typedef struct PcDrawMtxEnvelope {
    struct PcDrawMtxEnvelope* next;
    HSD_JObj* jobj;
    f32 weight;
} PcDrawMtxEnvelope;

static void drawmtx_rows(const char* what, f32 (*m)[4])
{
    int r;
    for (r = 0; r < 3; r++) {
        OSReport("[waza_viewer] drawmtx %s row %d: %.6f %.6f %.6f %.6f\n", what, r, m[r][0], m[r][1], m[r][2], m[r][3]);
    }
}

static int drawmtx_part(HSD_JObj* jobj)
{
    int counter = 0;
    return jobj != NULL ? joint_index_of(s_drawMtxRoot, jobj, &counter) : -1;
}

static void drawmtx_log(unsigned pobjFlags, const void* jobjv, int slot, const void* envv, f32 (*vmtx)[4],
                        f32 (*right)[4], f32 (*skin)[4], f32 (*loaded)[4])
{
    static const char* const kTypes[4] = { "skin (rigid / shared vertices)", "shape animation", "envelope",
                                           "unknown" };
    HSD_JObj* jobj = (HSD_JObj*)jobjv;
    char label[96];

    if (jobj == NULL || jobj != s_drawMtxJObj || s_drawMtxPObjs > 64) {
        return;
    }
    if (slot < 0) {
        u32 type = (pobjFlags >> 12) & 3; /* POBJ_SKIN 0, POBJ_SHAPEANIM 0x1000, POBJ_ENVELOPE 0x2000 */
        if (++s_drawMtxPObjs == 1) {
            HSD_JObj* skel = HSD_JObjFindSkeleton(jobj);
            OSReport("[waza_viewer] drawmtx joint %d flags 0x%08X (%s); HSD_JObjFindSkeleton -> joint %d flags 0x%08X; "
                     "drawn through HSD_PObjDisp / PObjSetupMtx (hsd/pobj.c)\n",
                     s_drawMtx, (unsigned)jobj->flags,
                     (jobj->flags & JOBJ_SKELETON_ROOT) ? "itself a skeleton root: no node matrix"
                                                        : "not a skeleton root",
                     drawmtx_part(skel), skel != NULL ? (unsigned)skel->flags : 0u);
            drawmtx_rows("camera view (vmtx)", vmtx);
        }
        OSReport("[waza_viewer] drawmtx pobj %d: flags 0x%04X, type %s\n", s_drawMtxPObjs - 1, pobjFlags,
                 kTypes[type]);
        if (type != 2) {
            snprintf(label, sizeof label, "pobj %d position mtx (pmtx = view x joint world)", s_drawMtxPObjs - 1);
            drawmtx_rows(label, loaded);
        }
        return;
    }
    if (slot == 0) {
        if (right != NULL) {
            snprintf(label, sizeof label, "pobj %d node (_HSD_mkEnvelopeModelNodeMtx)", s_drawMtxPObjs - 1);
            drawmtx_rows(label, right);
        } else {
            OSReport("[waza_viewer] drawmtx pobj %d node none (joint is a skeleton root: envelope joints' world "
                     "matrices used without their envelopemtx)\n",
                     s_drawMtxPObjs - 1);
        }
    }
    {
        const PcDrawMtxEnvelope* e;
        int k = 0;
        for (e = (const PcDrawMtxEnvelope*)envv; e != NULL; e = e->next, k++) {
            OSReport("[waza_viewer] drawmtx pobj %d slot %d envelope %d: joint %d weight %.6f%s\n", s_drawMtxPObjs - 1,
                     slot, k, drawmtx_part(e->jobj), e->weight,
                     (k == 0 && e->weight >= 1.0f) ? " (>= 1: this entry alone is used)" : "");
            if (e->jobj != NULL) {
                snprintf(label, sizeof label, "pobj %d slot %d envelope %d joint world", s_drawMtxPObjs - 1, slot, k);
                drawmtx_rows(label, e->jobj->mtx);
                if (e->jobj->envelopemtx != NULL) {
                    snprintf(label, sizeof label, "pobj %d slot %d envelope %d envelopemtx", s_drawMtxPObjs - 1, slot,
                             k);
                    drawmtx_rows(label, (f32(*)[4])e->jobj->envelopemtx);
                }
            }
            if (k == 0 && e->weight >= 1.0f) {
                break; /* SetupEnvelopeModelMtx reads only the first entry then */
            }
        }
    }
    snprintf(label, sizeof label, "pobj %d slot %d skin (world x envelopemtx%s)", s_drawMtxPObjs - 1, slot,
             right != NULL ? " x node" : "");
    drawmtx_rows(label, skin);
    if (s_drawMtxRoot != NULL) {
        /* the same in model space (part 0's world matrix taken off), the space Studio's values are in */
        Mtx inv, model;
        if (C_MTXInverse(s_drawMtxRoot->mtx, inv)) {
            C_MTXConcat(inv, skin, model);
            snprintf(label, sizeof label, "pobj %d slot %d skin in model space (inv(joint 0 world) x skin)",
                     s_drawMtxPObjs - 1, slot);
            drawmtx_rows(label, model);
        }
    }
    snprintf(label, sizeof label, "pobj %d slot %d loaded (view x skin -> GXLoadPosMtxImm, GX matrix %d)",
             s_drawMtxPObjs - 1, slot, slot * 3);
    drawmtx_rows(label, loaded);
}

/* Arms the log right after the camera print (the model is on its held frame by then), and disarms it one viewer draw
 * later, so exactly one frame's draws of the joint are printed */
static void drawmtx_step(void* owner)
{
    if (s_drawMtxState == 0) {
        void* model = PTR32_AT_T(void, (u8*)owner, 0x24);
        GSpart* part;
        GSpart* rootPart;

        s_drawMtxState = 2;
        if (model == NULL) {
            OSReport("[waza_viewer] ORRE_WAZA_VIEWER_DRAWMTX: no model\n");
            return;
        }
        GSmodelWalkJObjs((GSmodel*)model);
        part = GSmodelGetPart((GSmodel*)model, s_drawMtx);
        rootPart = GSmodelGetPart((GSmodel*)model, 0);
        s_drawMtxJObj = part != NULL ? part->jobj : NULL;
        s_drawMtxRoot = rootPart != NULL ? rootPart->jobj : NULL;
        if (part != NULL) {
            GSpartFree(part);
        }
        if (rootPart != NULL) {
            GSpartFree(rootPart);
        }
        if (s_drawMtxJObj == NULL) {
            OSReport("[waza_viewer] ORRE_WAZA_VIEWER_DRAWMTX: joint %d: no such joint\n", s_drawMtx);
            return;
        }
        OSReport("[waza_viewer] drawmtx: logging joint %d's draw matrices for one frame (model flags 0x%08X%s)\n",
                 s_drawMtx, (unsigned)((GSmodel*)model)->flags.raw,
                 (((GSmodel*)model)->flags.raw & 0x80) ? "; drawn from the blend tree, so this joint may not draw" : "");
        s_drawMtxState = 1;
        pc_waza_viewer_drawmtx_log = drawmtx_log;
        return;
    }
    if (s_drawMtxState == 1 && (s_drawMtxPObjs > 0 || ++s_drawMtxFrames > 30)) {
        pc_waza_viewer_drawmtx_log = NULL;
        s_drawMtxState = 2;
        if (s_drawMtxPObjs > 0) {
            OSReport("[waza_viewer] drawmtx: done (%d PObj draw(s) of joint %d logged)\n", s_drawMtxPObjs, s_drawMtx);
        } else {
            OSReport("[waza_viewer] drawmtx: joint %d drew no PObj through HSD_PObjDisp in 30 frames (hidden?)\n",
                     s_drawMtx);
        }
    }
}

/* ---- ORRE_WAZA_VIEWER_TEXLOG=1 (2026-11-19, Studio's texture / material check) ----
 * Why: Studio draws the battle models' materials with its own reader and renderer, and needs the game's own list of
 * what each drawn polygon object (PObj) is drawn with, to compare line for line: the textures (which image in the
 * model file, its format and size, palette, wrap, filters, how its coordinates are made), how each texture is combined
 * (the TObj's colour / alpha map modes, blending, its TEV block), the material's colours and render mode, and the pixel
 * engine state (blending, z, alpha compare). Studio prints the same list from its reader; the line format is documented
 * in docs/port/setup.md ("Booting into the move viewer", the _TEXLOG row) and must stay stable.
 * Where it is read: HSD draws each joint's DObjs (HSD_DObjDisp, hsd/dobj_exact_80198F7C.c): the material's setup
 * first (HSD_MObjSetup, hsd/mobj.c: material colours, HSD_TObjSetup's texture loads, the texture coordinate setup, the
 * TEV, then HSD_SetupRenderModeWithCustomPE's PE state through state.c's cache), then each PObj (HSD_PObjDisp,
 * hsd/pobj.c). GS replaces only the material class's load and TEV-building methods ("GSmaterial MObj",
 * gs_material_800DF498.c: shiny modulation, channel swaps, an environment map TObj), not the setup, so this is HSD's
 * own path for every battle model. pobj.c calls the pointer below (set only while the log is armed) for each PObj
 * after that setup and the PObj's cull mode, and this file reads, at that moment: the material HSD_DObjDisp made
 * current (mobj.c current_mobj), the TObj list HSD_TObjSetup loaded (tobj.c tobj_head; its map / coord / matrix ids are
 * assigned at each setup), and HSD's PE state cache (hsd_state.h: the values the last GX calls were made with).
 * Verified from the code. Only the viewer model's joints are printed (numbered as ORRE_WAZA_VIEWER_JOINTS); the
 * backdrop and the room are skipped.
 * The image's place in the model file: the model's PKX is found among GS's loaded resources (gs_res.c's table) as the
 * entry whose PKX header (load mode 5, 16 or 17 slots) holds a DAT that contains the model's scene_data model set
 * (GSmodel->resource); HSD archives are relocated in place (HSD_ArchiveParse), so a pointer minus the DAT's data
 * section start is the offset the file itself stores in that pointer field. A model loaded some other way (the three
 * species whose model is copied into a private buffer, docs/engine/models-animation.md "PKX file") prints offsets as
 * "none" and is matched by the hashes instead.
 * Observe-only: everything is read, nothing written. A test log, not a hook (no OrreHook_ name, not in
 * hook-points.md). Plain C and OSReport, every OS (verified on Windows only). */
void (*pc_waza_viewer_texlog_log)(const void* jobj, const void* pobj, u32 rendermode);
extern HSD_MObj* current_mobj;    /* mobj.c: the material HSD_DObjDisp set up for the PObjs being drawn */
extern HSD_TObj* tobj_head;       /* tobj.c: the TObj list HSD_TObjSetup loaded for it (toon / shadow TObjs included) */
extern HSD_MObjInfo lbl_80315490; /* gs_material: GS's material class ("GSmaterial MObj") */

/* gs_res.c's resource table entry (GSresEntry), repeated here because gs_res.c keeps it private; the same fields in
 * the same order, native pointers (the table is made at run time by GSresInit) */
typedef struct PcTexLogRes {
    u16 memHandle;
    void* data;
    u32 group;
    u32 id;
    void* release;
} PcTexLogRes;
extern PcTexLogRes* lbl_8047AC5C; /* gs_res.c: the entry table */
extern u32 lbl_8047AC60;          /* gs_res.c: its count */

/* HSD_PObj's first fields (include/hsd/hsd_pobj.h), repeated here because that header clashes with this unit's
 * others (as PcDrawMtxEnvelope above); same types in the same order */
typedef struct PcTexLogPObj {
    HSD_Class parent;
    struct PcTexLogPObj* next;
    void* verts;
    u16 flags;
} PcTexLogPObj;

static HSD_JObj* s_texRoot;      /* part 0 of the viewer model, for numbering joints as ORRE_WAZA_VIEWER_JOINTS */
static const u8* s_texData;      /* the model DAT's data section (what its pointers are relative to), or NULL */
static u32 s_texDataSize;

static u32 texlog_be32(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

/* FNV-1a over the first bytes: a content key for Studio when the offset can't be given (and a check when it can) */
static u32 texlog_hash(const void* p, u32 n)
{
    const u8* b = (const u8*)p;
    u32 h = 2166136261u;
    u32 i;
    if (b == NULL) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        h = (h ^ b[i]) * 16777619u;
    }
    return h;
}

/* "0x<offset>" from the DAT's data section start, or "none" (not in the model file) */
static void texlog_off(char* out, size_t cap, const void* p)
{
    const u8* b = (const u8*)p;
    if (b != NULL && s_texData != NULL && b >= s_texData && b < s_texData + s_texDataSize) {
        snprintf(out, cap, "0x%X", (unsigned)(b - s_texData));
    } else {
        snprintf(out, cap, "none");
    }
}

/* Finds the model's DAT among GS's loaded resources (see above); returns "pkx", "dat" or "none" */
static const char* texlog_find_base(GSmodel* model)
{
    const u8* res = (const u8*)model->resource;
    int pass;
    u32 i;

    s_texData = NULL;
    s_texDataSize = 0;
    if (res == NULL || lbl_8047AC5C == NULL) {
        return "none";
    }
    /* pass 0: a PKX (0x40-byte header, then the DAT; models-animation.md "PKX file"); pass 1: a bare DAT. The header
     * is only read once the model's own data is known to lie past it, so every byte read is inside that resource */
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < lbl_8047AC60; i++) {
            const u8* d = (const u8*)lbl_8047AC5C[i].data;
            const u8* dat = pass == 0 ? d + 0x40 : d;
            u32 fileSize, dataSize;
            if (d == NULL || res < dat + 0x20) {
                continue;
            }
            if (pass == 0) {
                u32 slots = texlog_be32(d + 0x08);
                if (texlog_be32(d + 0x10) != 5 || (slots != 16 && slots != 17) || texlog_be32(dat) != texlog_be32(d)) {
                    continue;
                }
            }
            fileSize = texlog_be32(dat);
            dataSize = texlog_be32(dat + 0x04);
            if (dataSize == 0 || 0x20u + dataSize + texlog_be32(dat + 0x08) * 4u > fileSize || res >= dat + 0x20 + dataSize) {
                continue;
            }
            s_texData = dat + 0x20;
            s_texDataSize = dataSize;
            return pass == 0 ? "pkx" : "dat";
        }
    }
    return "none";
}

static void texlog_rgba(char* out, size_t cap, GXColor c)
{
    snprintf(out, cap, "%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
}

static void texlog_tobj(const char* id, int n, HSD_TObj* tobj)
{
    static const char* const kCoord[7] = { "uv", "reflection", "highlight", "shadow", "toon", "gradation", "backlight" };
    const HSD_ImageDesc* img = tobj->imagedesc;
    const HSD_TexLODDesc* lod = tobj->lod;
    const HSD_Tlut* tlut = NULL;
    const void* pixels = img != NULL ? DP(img->image_ptr) : NULL;
    unsigned fmt = img != NULL ? (unsigned)img->format : 0xFFFFFFFFu;
    int isCI = img != NULL && (fmt == GX_TF_C4 || fmt == GX_TF_C8 || fmt == GX_TF_C14X2);
    int minFilt = lod != NULL ? (int)lod->minFilt : GX_LIN_MIP_LIN; /* HSD_TObjSetup's default_lod */
    int minUsed = minFilt;
    u32 coordType = tobj_coord(tobj);
    char coordName[16], imgOff[24], descOff[24], tlutOff[24], k[12], t0[12], t1[12];

    if (coordType < 7) {
        snprintf(coordName, sizeof coordName, "%s", kCoord[coordType]);
    } else {
        snprintf(coordName, sizeof coordName, "other%u", (unsigned)coordType);
    }
    /* the minification filter HSD_TObjSetup really passes to GXInitTexObjLOD */
    if (isCI && minUsed == GX_LIN_MIP_LIN) {
        minUsed = GX_LIN_MIP_NEAR;
    }
    if (img != NULL && !img->mipmap) {
        minUsed &= 0x01;
    }
    texlog_off(imgOff, sizeof imgOff, pixels);
    texlog_off(descOff, sizeof descOff, img);
    OSReport("[waza_viewer] texlog tobj %s tobj=%d map=%d coord=%d texmtx=%d src=%d coordtype=%s img=%s img_hash=%08X "
             "imgdesc=%s fmt=%d w=%u h=%u mip=%u minlod=%.6f maxlod=%.6f wrap_s=%d wrap_t=%d repeat_s=%u repeat_t=%u "
             "mag=%d min=%d min_used=%d lod=%s lodbias=%.6f bias_clamp=%u edge_lod=%u aniso=%u\n",
             id, n, (int)tobj->id, (int)tobj->coord, (int)tobj->mtxid, (int)tobj->src, coordName, imgOff,
             (unsigned)texlog_hash(pixels, 32), descOff, img != NULL ? (int)fmt : -1, img != NULL ? img->width : 0u,
             img != NULL ? img->height : 0u, img != NULL ? (unsigned)img->mipmap : 0u,
             img != NULL ? img->minLOD : 0.0f, img != NULL ? img->maxLOD : 0.0f, (int)tobj->wrap_s,
             (int)tobj->wrap_t, (unsigned)tobj->repeat_s, (unsigned)tobj->repeat_t, (int)tobj->magFilt, minFilt,
             minUsed, lod != NULL ? "desc" : "default", lod != NULL ? lod->LODBias : 0.0f,
             lod != NULL ? (unsigned)lod->bias_clamp : 0u, lod != NULL ? (unsigned)lod->edgeLODEnable : 0u,
             lod != NULL ? (unsigned)lod->max_anisotropy : 0u);
    if (isCI) {
        /* the palette HSD_TObjSetup loads: the texture animation's table entry when one is selected, else the TObj's */
        tlut = tobj->tlut_no != (u8)-1 && tobj->tluttbl != NULL ? tobj->tluttbl[tobj->tlut_no] : tobj->tlut;
    }
    if (tlut != NULL) {
        u32 bytes = (u32)tlut->n_entries * 2u;
        texlog_off(tlutOff, sizeof tlutOff, tlut->lut);
        OSReport("[waza_viewer] texlog tlut %s tobj=%d tlut=%s tlut_hash=%08X tlut_fmt=%u tlut_n=%u tlut_name=%u "
                 "tlut_no=%d\n",
                 id, n, tlutOff, (unsigned)texlog_hash(tlut->lut, bytes < 32u ? bytes : 32u), (unsigned)tlut->fmt,
                 (unsigned)tlut->n_entries, (unsigned)tlut->tlut_name,
                 tobj->tlut_no != (u8)-1 ? (int)tobj->tlut_no : -1);
    } else {
        OSReport("[waza_viewer] texlog tlut %s tobj=%d tlut=none\n", id, n);
    }
    if (tobj->tev != NULL) {
        const HSD_TObjTev* tev = tobj->tev;
        texlog_rgba(k, sizeof k, tev->konst);
        texlog_rgba(t0, sizeof t0, tev->tev0);
        texlog_rgba(t1, sizeof t1, tev->tev1);
        OSReport("[waza_viewer] texlog tobjtev %s tobj=%d flags=0x%08X colormap=%u alphamap=%u lightmap=0x%03X bump=%u "
                 "blending=%.6f tev=1 color_op=%u alpha_op=%u color_bias=%u alpha_bias=%u color_scale=%u "
                 "alpha_scale=%u color_clamp=%u alpha_clamp=%u color_abcd=%u,%u,%u,%u alpha_abcd=%u,%u,%u,%u konst=%s "
                 "tev0=%s tev1=%s active=0x%08X\n",
                 id, n, (unsigned)tobj->flags, (unsigned)((tobj->flags >> 16) & 0xF),
                 (unsigned)((tobj->flags >> 20) & 0xF), (unsigned)(tobj->flags & TEX_LIGHTMAP_MASK),
                 tobj_bump(tobj) ? 1u : 0u, tobj->blending, tev->color_op, tev->alpha_op, tev->color_bias,
                 tev->alpha_bias, tev->color_scale, tev->alpha_scale, tev->color_clamp, tev->alpha_clamp, tev->color_a,
                 tev->color_b, tev->color_c, tev->color_d, tev->alpha_a, tev->alpha_b, tev->alpha_c, tev->alpha_d, k,
                 t0, t1, (unsigned)tev->active);
    } else {
        OSReport("[waza_viewer] texlog tobjtev %s tobj=%d flags=0x%08X colormap=%u alphamap=%u lightmap=0x%03X bump=%u "
                 "blending=%.6f tev=0\n",
                 id, n, (unsigned)tobj->flags, (unsigned)((tobj->flags >> 16) & 0xF),
                 (unsigned)((tobj->flags >> 20) & 0xF), (unsigned)(tobj->flags & TEX_LIGHTMAP_MASK),
                 tobj_bump(tobj) ? 1u : 0u, tobj->blending);
    }
    OSReport("[waza_viewer] texlog tobjmtx %s tobj=%d scale=%.6f,%.6f,%.6f rotate=%.6f,%.6f,%.6f "
             "translate=%.6f,%.6f,%.6f anim_id=%u\n",
             id, n, tobj->scale.x, tobj->scale.y, tobj->scale.z, tobj->rotate.x, tobj->rotate.y, tobj->rotate.z,
             tobj->translate.x, tobj->translate.y, tobj->translate.z, (unsigned)tobj->anim_id);
}

static void texlog_log(const void* jobjv, const void* pobjv, u32 rendermode)
{
    static const char* const kTypes[4] = { "skin", "shapeanim", "envelope", "unknown" };
    HSD_JObj* jobj = (HSD_JObj*)jobjv;
    const PcTexLogPObj* pobj = (const PcTexLogPObj*)pobjv;
    HSD_DObj* dobj = NULL;
    HSD_DObj* dp;
    HSD_MObj* mobj;
    HSD_TObj* tobj;
    int counter = 0, part, d = 0, p = -1, setup, n;
    char id[64], amb[12], dif[12], spc[12];

    if (jobj == NULL || pobj == NULL || s_texRoot == NULL || s_texLogDraws >= 512) {
        return;
    }
    part = joint_index_of(s_texRoot, jobj, &counter);
    if (part < 0) {
        return; /* not the viewer model (the backdrop, the room) */
    }
    for (dp = union_type_dobj(jobj) ? jobj->u.dobj : NULL; dp != NULL && dobj == NULL; dp = dp->next, d++) {
        const PcTexLogPObj* pp;
        int k = 0;
        for (pp = (const PcTexLogPObj*)dp->pobj; pp != NULL; pp = pp->next, k++) {
            if (pp == pobj) {
                dobj = dp;
                p = k;
                break;
            }
        }
    }
    if (dobj == NULL) {
        d = -1;
    } else {
        d--; /* the loop stepped once past the DObj it found */
    }
    mobj = dobj != NULL ? dobj->mobj : current_mobj;
    /* 1 when HSD_DObjDisp set this DObj's material up for this draw (a shadow-pass draw skips the setup) */
    setup = dobj != NULL && mobj == current_mobj && !(rendermode & RENDER_SHADOW);
    snprintf(id, sizeof id, "draw=%d part=%d dobj=%d pobj=%d", s_texLogDraws, part, d, p);
    s_texLogDraws++;

    OSReport("[waza_viewer] texlog pobj %s flags=0x%04X type=%s cull=%d rm=0x%08X setup=%d\n", id,
             (unsigned)pobj->flags, kTypes[(pobj->flags >> 12) & 3], state_cull_mode, (unsigned)rendermode, setup);
    if (mobj == NULL) {
        OSReport("[waza_viewer] texlog mat %s none\n", id);
        return;
    }
    if (mobj->mat != NULL) {
        texlog_rgba(amb, sizeof amb, mobj->mat->ambient);
        texlog_rgba(dif, sizeof dif, mobj->mat->diffuse);
        texlog_rgba(spc, sizeof spc, mobj->mat->specular);
        OSReport("[waza_viewer] texlog mat %s class=%s mrm=0x%08X ambient=%s diffuse=%s specular=%s alpha=%.6f "
                 "shininess=%.6f\n",
                 id, mobj->parent.class_info == (HSD_ClassInfo*)&lbl_80315490 ? "gs" : "hsd", (unsigned)mobj->rendermode, amb, dif, spc, mobj->mat->alpha, mobj->mat->shininess);
    } else {
        OSReport("[waza_viewer] texlog mat %s class=%s mrm=0x%08X material=none\n", id,
                 mobj->parent.class_info == (HSD_ClassInfo*)&lbl_80315490 ? "gs" : "hsd", (unsigned)mobj->rendermode);
    }
    if (mobj->parent.class_info == (HSD_ClassInfo*)&lbl_80315490 && ((GSmatObj*)mobj)->material != NULL) {
        const GSmaterial* gm = ((GSmatObj*)mobj)->material;
        OSReport("[waza_viewer] texlog gsmat %s ext=0x%04X modulate=%02X%02X%02X%02X channels=%u,%u,%u,%u env=%d "
                 "env_mode=%d env_type=%d env_blend=%.6f\n",
                 id, (unsigned)gm->extensions, gm->modulate[0], gm->modulate[1], gm->modulate[2], gm->modulate[3],
                 (unsigned)gm->channels[0], (unsigned)gm->channels[1], (unsigned)gm->channels[2],
                 (unsigned)gm->channels[3], gm->envTObj != NULL ? 1 : 0, (int)gm->envMode, (int)gm->envType,
                 gm->envBlend);
    } else {
        OSReport("[waza_viewer] texlog gsmat %s none\n", id);
    }
    if (mobj->pe != NULL) {
        const HSD_PEDesc* pe = mobj->pe;
        OSReport("[waza_viewer] texlog matpe %s flags=0x%02X ref0=%u ref1=%u dst_alpha=%u type=%u src=%u dst=%u logic=%u "
                 "z_comp=%u alpha_comp0=%u alpha_op=%u alpha_comp1=%u\n",
                 id, pe->flags, pe->ref0, pe->ref1, pe->dst_alpha, pe->type, pe->src_factor, pe->dst_factor,
                 pe->logic_op, pe->z_comp, pe->alpha_comp0, pe->alpha_op, pe->alpha_comp1);
    } else {
        OSReport("[waza_viewer] texlog matpe %s none\n", id);
    }
    /* HSD's state cache (state.c): the values of the last GXSetBlendMode / GXSetZMode / GXSetAlphaCompare /
     * GXSetZCompLoc / GXSetColorUpdate / GXSetAlphaUpdate / GXSetDstAlpha / GXSetDither calls, which
     * HSD_SetupPEMode just made for this material (from its PE description, or from the render mode without one) */
    OSReport("[waza_viewer] texlog pe %s blend=%d src=%d dst=%d logic=%d z_enable=%u z_func=%d z_update=%u "
             "z_before_tex=%u alpha_comp0=%d alpha_ref0=%u alpha_op=%d alpha_comp1=%d alpha_ref1=%u color_update=%u "
             "alpha_update=%u dst_alpha_enable=%u dst_alpha=%u dither=%u\n",
             id, state_blend_type, state_src_factor, state_dst_factor, state_logic_op, (unsigned)state_z_enable,
             state_z_func, (unsigned)state_z_update, (unsigned)state_before_tex, state_alpha_comp0,
             (unsigned)state_alpha_ref0, state_alpha_op, state_alpha_comp1, (unsigned)state_alpha_ref1,
             (unsigned)state_color_update, (unsigned)state_alpha_update, (unsigned)state_enable_dst_alpha,
             (unsigned)state_dst_alpha, (unsigned)state_dither);
    /* the TObjs: the list HSD_TObjSetup loaded for this draw, or the material's own when its setup was skipped */
    for (tobj = setup ? tobj_head : mobj->tobj, n = 0; tobj != NULL && n < 16; tobj = tobj->next, n++) {
        texlog_tobj(id, n, tobj);
    }
    OSReport("[waza_viewer] texlog end %s tobjs=%d\n", id, n);
}

/* Arms the log on the first viewer draw after the camera print (the model is on its held frame by then) and disarms
 * it one viewer draw later, so exactly one frame's draws are printed (the same timing as ORRE_WAZA_VIEWER_DRAWMTX) */
static void texlog_step(void* owner)
{
    if (s_texLogState == 0) {
        void* model = PTR32_AT_T(void, (u8*)owner, 0x24);
        GSpart* rootPart;
        const char* base;

        s_texLogState = 2;
        if (model == NULL) {
            OSReport("[waza_viewer] texlog: no model\n");
            return;
        }
        GSmodelWalkJObjs((GSmodel*)model);
        rootPart = GSmodelGetPart((GSmodel*)model, 0);
        s_texRoot = rootPart != NULL ? rootPart->jobj : NULL;
        if (rootPart != NULL) {
            GSpartFree(rootPart);
        }
        if (s_texRoot == NULL) {
            OSReport("[waza_viewer] texlog: the model has no joints\n");
            return;
        }
        base = texlog_find_base((GSmodel*)model);
        {
            char resOff[24];
            texlog_off(resOff, sizeof resOff, ((GSmodel*)model)->resource);
            OSReport("[waza_viewer] texlog begin base=%s data_size=0x%X model_set=%s model_flags=0x%08X\n", base,
                     (unsigned)s_texDataSize, resOff, (unsigned)((GSmodel*)model)->flags.raw);
        }
        s_texLogState = 1;
        pc_waza_viewer_texlog_log = texlog_log;
        return;
    }
    if (s_texLogState == 1 && (s_texLogDraws > 0 || ++s_texLogFrames > 30)) {
        pc_waza_viewer_texlog_log = NULL;
        s_texLogState = 2;
        if (s_texLogDraws > 0) {
            OSReport("[waza_viewer] texlog done draws=%d\n", s_texLogDraws);
        } else {
            OSReport("[waza_viewer] texlog done draws=0 (no PObj of the model drawn through HSD_PObjDisp in 30 frames)\n");
        }
    }
}

/* Whether the room's map archive is there: wazaViewerDrawBackdrop looks its scene_data up without a NULL check (it
 * would fault on the GameCube too), so the backdrop waits until the map is loaded */
static int room_map_loaded(void)
{
    void* floorData = floorDataBiosGetCurrentPtr();
    if (floorData == NULL) {
        return 0;
    }
    return GSresGetResource(std093_getCurrentGroupID(), DISC_U32((u8*)floorData + 0x08)) != NULL;
}

static void viewer_draw(void* current)
{
    if (s_backdrop) {
        static int waited;
        /* the game's own backdrop: hides the room's models and draws the grey quad behind the model, facing the camera
         * (needs the bounds bit set above) */
        if (room_map_loaded()) {
            wazaViewerDrawBackdrop(current);
        } else if (!waited) {
            OSReport("[waza_viewer] backdrop: the room's map archive isn't loaded yet; waiting\n");
            waited = 1;
        }
    }
    if (!s_logged && s_spawned == 1 && ++s_draws >= 3) {
        /* a few frames in, so the orbit camera has been placed from the final settings */
        log_camera(current);
        s_logged = 1;
    } else if (s_logged) {
        if (s_drawMtx >= 0) {
            drawmtx_step(current); /* ORRE_WAZA_VIEWER_DRAWMTX: armed on the first draw after the camera print */
        }
        if (s_texLog) {
            texlog_step(current); /* ORRE_WAZA_VIEWER_TEXLOG: the same timing */
        }
    }
}

static int env_float(const char* name, f32* out)
{
    const char* v = getenv(name);
    char* end;
    double d;
    if (v == NULL || *v == '\0') {
        return 0;
    }
    d = strtod(v, &end);
    if (end == v) {
        OSReport("[waza_viewer] %s=%s is not a number; ignored\n", name, v);
        return 0;
    }
    *out = (f32)d;
    return 1;
}

static int env_on(const char* name)
{
    const char* v = getenv(name);
    return v != NULL && *v != '\0' && *v != '0';
}

/* ---- The every-move sweep (2026-11-18; the project's build helper, for frame smoothing's sweep; docs/port/setup.md "The
 * move sweep") ----
 * ORRE_WAZA_SWEEP=main[:<first>-<last>] | status[:<first>-<last>]: boots into the move viewer like ORRE_WAZA_VIEWER and
 * plays every move's sequences, one after another, on a fixed pair: Espeon (attacker) and Umbreon (target), snatcherLeo's own
 * the reference playthrough pair, both with all 17 animation slots. Why: frame smoothing needs every move effect checked at 120 pictures a
 * second (snatcherLeo: every move verified, not assumed), and the viewer is the game's own tool for playing one move on two
 * models with nothing else on screen. The viewer runs in mode 1 on retail, which takes its orders from its work area
 * (wazaViewerUpdateModels): the two models' grid slots and model ids, the move (+0x650), the layout (+0x658: 0 = the
 * attacker plays the move's part 1 and the target part 2, 1 = the attacker part 3, 2 = the attacker a type-4 sequence:
 * status, weather, ball and title effects), a reload request (+0x628) and the play request (+0x62C, which the viewer
 * clears when both models' sequences are done). For each item the sweep writes those fields, waits for the spawn,
 * plays, waits for the end (or 30 s), logs "[waza_sweep] ..." with the game frames and the move's name, and goes on.
 *   main: for each move id 1 .. the table's count: layout 0 when part 1 or 2 exists, then layout 1 when part 3 exists
 *   status: each type-4 sequence 1 .. its count (layout 2)
 *   variant: each record of the per-turn variant table (Rollout / Ice Ball turns 2-5, ...), the move's parts with that
 *     variant (only the parts the variant gives a file of its own), layouts 0 / 1 as in main
 *   pokemon[:<first>-<last> records]: each per-Pokémon version (lbl_80373750: Pikachu's Thunderbolt, ...), with that
 *     model on its side (the attacker, or the target for a damage part), batched by model
 * A debug switch only (off by
 * default, never a setting); plain C and getenv, the same on every OS. */
/* wazaSequenceGetWazaResource: battle_waza_types.h (the game's own lookup, owner NULL: no per-Pokémon version) */
extern u32 lbl_80478CC0[]; /* the main move-sequence table's count (sequence.c) */
extern u32 lbl_80478CE0[]; /* the type-4 table's count */
extern u8 lbl_803727C8[];  /* the type-4 table: 12-byte records {archive, entry, name message} (read natively, as
                            * sequence.c reads it) */
extern u8 lbl_80373750[];  /* the per-Pokémon versions: 16-byte records, u16 model id at +2, u16 move at +4 */
extern volatile u32 pc_vi_heartbeat; /* the game frame (pc_vi.c) */
const char* pc_event_log_move_name(u32 move);
void pc_msg_text_utf8(u32 key, char* out, size_t cap);

#define PC_SWEEP_ATTACKER 196 /* Espeon (internal species id) */
#define PC_SWEEP_TARGET 197   /* Umbreon */

typedef struct PcSweepItem {
    u16 move;   /* the move id, or the type-4 sequence's index (layout 2) */
    u8 layout;
    u8 variant; /* the variant pass: the per-turn version (Rollout / Ice Ball turns 2-5...); 0 otherwise */
    u16 model;  /* the per-Pokémon pass: the model (PKX id) that has its own version; 0 otherwise */
    u8 side;    /* 0: that model attacks (parts 1 / 3), 1: it is the target (part 2) */
    u16 record; /* the per-Pokémon table's record (lbl_80373750), for the report */
} PcSweepItem;

extern u8 lbl_80373210[];  /* the per-turn variants: 32-byte records {u32 move, u32 variant, 3 x {archive, entry}} */
extern u32 lbl_80478CE8[]; /* their count */

static PcSweepItem s_items[1024];
static int s_itemCount, s_item = -1;
static int s_sweepState; /* 0 next item, 1 spawning, 2 playing, 3 pause after it */
static u32 s_sweepModelA, s_sweepModelB, s_stateFrame, s_playFrame;

static void sweep_name(const PcSweepItem* it, char* out, size_t cap)
{
    if (it->layout == 2) {
        /* the record's third word, the name message (untranslated Japanese developer labels in the US banks):
         * read as sequence.c reads the record's first two words, and big-endian when that gives nothing (the first
         * run printed no names: the PC table converts the words the game reads, likely not this one) */
        const u32 key = ((const u32*)lbl_803727C8)[it->move * 3 + 2];
        pc_msg_text_utf8(key, out, cap);
        if (out[0] == '\0') {
            pc_msg_text_utf8(DISC_U32((const u8*)lbl_803727C8 + it->move * 12 + 8), out, cap);
        }
        if (out[0] == '\0') {
            snprintf(out, cap, "(no name)");
        }
    } else if (it->move == 355) {
        /* record 355 is Shadow Rush's animation (wzx_darkrush_attack / _damage), played for move 356; move 355 itself
         * is a placeholder with no name (verified: the extractor's links and the battle log's "move 356 SHADOW RUSH") */
        snprintf(out, cap, "SHADOW RUSH (move 356's animation)");
    } else {
        snprintf(out, cap, "%s", pc_event_log_move_name(it->move));
    }
}

static int sweep_has(u16 move, u16 type)
{
    u32 g = 0, r = 0;
    wazaSequenceGetWazaResource(NULL, move, type, 0, &g, &r);
    return g != 0 && r != 0;
}

/* the variant pass: whether the variant record gives its own file for this part (the game's lookup with the
 * variant; owner NULL, so no per-Pokémon version) */
static int sweep_has_variant(u16 move, u16 type, u8 variant)
{
    u32 g = 0, r = 0, g0 = 0, r0 = 0;
    wazaSequenceGetWazaResource(NULL, move, type, variant, &g, &r);
    wazaSequenceGetWazaResource(NULL, move, type, 0, &g0, &r0);
    return g != 0 && r != 0 && (g != g0 || r != r0);
}

/* the items, built on the first viewer pass (the tables are published with common_rel and the main executable) */
static void sweep_build(const char* spec)
{
    int pass = strncmp(spec, "status", 6) == 0    ? 2
               : strncmp(spec, "variant", 7) == 0 ? 3
               : strncmp(spec, "pokemon", 7) == 0 ? 4
                                                  : 0;
    int first = 1, last = 0x7FFF;
    const char* colon = strchr(spec, ':');
    int m;
    if (colon != NULL) {
        sscanf(colon + 1, "%d-%d", &first, &last);
    }
    if (pass == 4) {
        /* the per-Pokémon versions (lbl_80373750, 16-byte records read natively as sequence.c reads them: u8 variant,
         * u16 model id at +2, u16 move at +4, u32 archive at +8, u32 entry at +12). The record's index says the part
         * (wazaSequenceGetWazaResource's ranges): 1-0x11B part 1 (that model attacks), 0x11E-0x129 part 2 (it is the
         * target), 0x12C-0x15D part 3, 0x162-0x16D a per-turn variant (parts 1 / 3 with that variant). Batched by model
         * (sorted below), so each model loads once in a row. first-last select records. */
        const int count = 0x16F0 / 0x10;
        int i, j;
        for (i = 0; i < count && s_itemCount < 1023; i++) {
            const u8* e = lbl_80373750 + i * 0x10;
            const u16 model = *(const u16*)(e + 2);
            const u16 move = *(const u16*)(e + 4);
            PcSweepItem it;
            if (i < first || i > last || model == 0 || move == 0 || *(const u32*)(e + 8) == 0 ||
                *(const u32*)(e + 12) == 0) {
                continue;
            }
            memset(&it, 0, sizeof(it));
            it.move = move;
            it.model = model;
            it.record = (u16)i;
            if (i >= 0x11E && i < 0x12A) {
                it.layout = 0;
                it.side = 1;
            } else if (i >= 0x12C && i < 0x15E) {
                it.layout = 1;
            } else if (i >= 0x162 && i < 0x16E) {
                /* a per-turn variant of the species' own part 1, or of its own part 3 when the table has a part-3
                 * record for the same model and move (Swallow's four variant-1 records, 358-361: part 1 had no
                 * sequence, so the first pass skipped them; 2026-11-18) */
                s32 k;
                it.layout = 0;
                for (k = 0x12C; k < 0x15E; k++) {
                    const u8* p3 = lbl_80373750 + k * 0x10;
                    if (*(const u16*)(p3 + 2) == model && *(const u16*)(p3 + 4) == move) {
                        it.layout = 1;
                        break;
                    }
                }
                it.variant = e[0];
            } else if (i >= 1 && i < 0x11C) {
                it.layout = 0;
            } else {
                continue; /* the type-4 slot (0x160) and the gaps between the ranges: not looked up by the game */
            }
            s_items[s_itemCount++] = it;
        }
        /* by model (insertion sort: a few hundred items), keeping the table's order within a model */
        for (i = 1; i < s_itemCount; i++) {
            const PcSweepItem key = s_items[i];
            for (j = i - 1; j >= 0 && s_items[j].model > key.model; j--) {
                s_items[j + 1] = s_items[j];
            }
            s_items[j + 1] = key;
        }
    } else if (pass == 3) {
        /* each record of the variant table (read natively, as sequence.c reads it): the move's parts with that
         * variant, layout 0 when part 1 or 2 has its own file, layout 1 when part 3 has */
        const int count = (int)lbl_80478CE8[0];
        for (m = 0; m < count && s_itemCount < 1023; m++) {
            const u32* rec = (const u32*)(lbl_80373210 + m * 0x20);
            const u16 move = (u16)rec[0];
            const u8 variant = (u8)rec[1];
            if (move < first || move > last || variant == 0) {
                continue;
            }
            if (sweep_has_variant(move, 1, variant) || sweep_has_variant(move, 2, variant)) {
                s_items[s_itemCount].move = move;
                s_items[s_itemCount].variant = variant;
                s_items[s_itemCount++].layout = 0;
            }
            if (sweep_has_variant(move, 3, variant)) {
                s_items[s_itemCount].move = move;
                s_items[s_itemCount].variant = variant;
                s_items[s_itemCount++].layout = 1;
            }
        }
    } else if (pass == 2) {
        const int count = (int)lbl_80478CE0[0];
        for (m = first < 1 ? 1 : first; m < count && m <= last && s_itemCount < 1024; m++) {
            if (sweep_has((u16)m, 4)) {
                s_items[s_itemCount].move = (u16)m;
                s_items[s_itemCount++].layout = 2;
            }
        }
    } else {
        const int count = (int)lbl_80478CC0[0];
        for (m = first < 1 ? 1 : first; m < count && m <= last && s_itemCount < 1023; m++) {
            if (sweep_has((u16)m, 1) || sweep_has((u16)m, 2)) {
                s_items[s_itemCount].move = (u16)m;
                s_items[s_itemCount++].layout = 0;
            }
            if (sweep_has((u16)m, 3)) {
                s_items[s_itemCount].move = (u16)m;
                s_items[s_itemCount++].layout = 1;
            }
        }
    }
    OSReport("[waza_sweep] %s: %d items (moves %d-%d; the main table holds %u, the type-4 table %u, the variant table "
             "%u)\n",
             pass == 4 ? "pokemon" : pass == 3 ? "variant" : pass == 2 ? "status" : "main", s_itemCount, first, last,
             (unsigned)lbl_80478CC0[0], (unsigned)lbl_80478CE0[0], (unsigned)lbl_80478CE8[0]);
}

/* The sweep spawns and plays the two models itself, the way wazaViewerSpawnAttacker / Target and
 * wazaViewerUpdateModels do (the same calls in the same order), but keeps the two owners in its own pointers. Why
 * (verified, the first sweep run crashed at game frame 262 reading 0xffffffffffffffff): the viewer keeps its two
 * owners in 4-byte slots at work +0x61C / +0x620 but writes and reads them as pointers, which on a 64-bit PC overlap;
 * with a target in the second slot the first slot's pointer reads back with the target's low half in its top half.
 * Retail (32-bit) is fine; the PC only ever used the one-model mode (slot 2 empty) until this sweep. Fixing the viewer's
 * own code for two models would touch many matched functions in wazaViewer.c; this keeps the fix in the PC-only file.
 * Linux and macOS: the same (any 64-bit build). */
extern f32 battleGridGetDistance(u8 side);
extern const f32 lbl_8047E2E4; /* 0.0 */
extern const f32 lbl_8047E2E8; /* -pi / 2: the target faces the attacker */
extern const f32 lbl_8047E2EC; /* +pi / 2 */
static void* s_owner[2];

static void* sweep_spawn(u32 model, int side, u16 move, int part, u8 variant)
{
    f32 pos[3], rot[3];
    void* owner;
    const f32 distance = battleGridGetDistance(1);
    pos[0] = side == 0 ? -distance : distance;
    pos[1] = lbl_8047E2E4;
    pos[2] = lbl_8047E2E4;
    rot[0] = lbl_8047E2E4;
    rot[1] = side == 0 ? lbl_8047E2EC : lbl_8047E2E8;
    rot[2] = lbl_8047E2E4;
    owner = wazaSequencePokemonCreate((u16)model, PC_PTR(_fadeEffectGetRandom__FUl((u32)-1)), 0);
    if (owner == NULL) {
        return NULL;
    }
    GSmodelSetPosition(PTR32_AT_T(struct GSmodel, (u8*)owner, 0x24), (struct GSvec*)pos);
    GSmodelSetRotation(PTR32_AT_T(struct GSmodel, (u8*)owner, 0x24), (struct GSvec*)rot);
    wazaSequenceOwnerSetVisibility(owner, 1);
    *(s8*)((u8*)owner + 0x76) = side == 0 ? -1 : 1;
    if (part != 0 && move != 0) {
        wazaSequenceOwnerLoadWaza(owner, move, (u16)part, variant);
    }
    return owner;
}

static void sweep_free(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (s_owner[i] != NULL) {
            wazaSequenceOwnerFree(s_owner[i]);
            s_owner[i] = NULL;
        }
    }
}

/* start owner i's loaded sequence (as wazaViewerUpdateModels does); 0 when it has none */
static int sweep_start(int i)
{
    u8* seq;
    if (s_owner[i] == NULL) {
        return 0;
    }
    seq = PTR32_AT_T(u8, (u8*)s_owner[i], 0x68);
    if (seq == NULL) {
        return 0;
    }
    wazaSequenceOwnerSetVisibility(s_owner[i], 1);
    wazaSequenceSysStartWaza(s_owner[i], *(u16*)(seq + 0x2C), *(u16*)(seq + 0x2E));
    return 1;
}

/* ---- ORRE_WAZA_SWEEP_MODELLOG=1 (2026-11-20, Studio's "effect model with no joint tree" question) ----
 * Why: some move effect archives hold a model set with no joint tree (the set's joint pointer is 0) but with an
 * animation (Trick and Skill Swap parts 1-3, Splash, Tail Whip, Encore, Hidden Power, several third parts). The model
 * entry still makes a GSmodel from it (_modelLoad, HSD_JObjLoadJoint(NULL)), and particle entries linked to it (entry
 * flag 1, _wazaSequenceParticleEntryStart / _wazaSequenceModelEntryStart) take that model as their source. Studio's
 * port of those moves leaves the sparkles in place and Splash's hop out, so it needs to see what the game's GSmodel
 * actually holds over the move and whether the linked generators move. Each sweep frame while a part plays, this prints
 * for both owners: the owner's Pokémon model placement, then every model entry (kind 2) of the playing sequence (its
 * GSmodel: pointer, renderJObj, position / rotation / scale as GSmodelGetPosition / GetRotation / GetScale return them,
 * the animation index / frame / count, flags, the transform-override link) and every particle entry (kind 3: its field
 * particle node's placement fields and its HSD generator's pos / rot / scale / linked joint / app SRT translate, live
 * particle count). Line format in docs/port/move-viewer.md. Observe-only: everything is read, nothing written, no game
 * function called (the getters are plain field copies, read directly here), no random draws. Sweep only, off unless
 * set. Plain C and OSReport, every OS (verified on Windows only). */
static void modellog_vec(char* out, size_t cap, const f32* v)
{
    snprintf(out, cap, "%.3f,%.3f,%.3f", v[0], v[1], v[2]);
}

static void modellog_step(u32 now)
{
    int i;
    for (i = 0; i < 2; i++) {
        u8* owner = (u8*)s_owner[i];
        const GSmodel* om;
        u8* seq;
        u8* node;
        int guard = 0;
        char p[48], r[48], s[48];
        if (owner == NULL) {
            continue;
        }
        om = PTR32_AT_T(GSmodel, owner, 0x24);
        seq = PTR32_AT_T(u8, owner, 0x6C); /* the sequence being played (currentSequence), else the loaded one */
        if (seq == NULL) {
            seq = PTR32_AT_T(u8, owner, 0x68);
        }
        if (om != NULL) {
            modellog_vec(p, sizeof p, &om->position.x);
            modellog_vec(r, sizeof r, &om->rotation.x);
            OSReport("[waza_modellog] f=%u owner %d (%s) pokemon model %p pos %s rot %s anim %u frame %.2f override %d\n",
                     (unsigned)now, i, i == 0 ? "attacker" : "target", (const void*)om, p, r, (unsigned)om->animIndex,
                     om->animFrame, (int)om->transformOverride);
        }
        if (seq == NULL) {
            continue;
        }
        for (node = PTR32_AT_T(u8, seq, 0x24); node != NULL && guard < 128; node = PTR32_AT_T(u8, node, 0xA8), guard++) {
            const WazaSequenceNode* n = (const WazaSequenceNode*)node;
            if (n->kind == 2) {
                const GSmodel* m = PTR32_AT_T(GSmodel, node, 0xA4);
                if (m == NULL) {
                    OSReport("[waza_modellog] f=%u owner %d entry key %d model: none (start %d time %d state %u)\n",
                             (unsigned)now, i, (int)n->linkKey, (int)n->startTime, (int)n->currentTime,
                             (unsigned)n->runtimeState);
                    continue;
                }
                modellog_vec(p, sizeof p, &m->position.x);
                modellog_vec(r, sizeof r, &m->rotation.x);
                modellog_vec(s, sizeof s, &m->scale.x);
                OSReport("[waza_modellog] f=%u owner %d entry key %d model %p renderJObj %p alt %p pos %s rot %s scale %s "
                         "anim %d/%u frame %.2f end %.2f rate %.2f flags 0x%08X override %d->%p part %d visible %d | "
                         "entry start %d time %d state %u flags 0x%X pos type %d anim entry %d restore %d part %d "
                         "attached %d\n",
                         (unsigned)now, i, (int)n->linkKey, (const void*)m, (const void*)m->renderJObj,
                         (const void*)m->renderJObjAlt, p, r, s, (int)m->animIndex, (unsigned)m->animCount,
                         m->animFrame, m->animEndFrame, m->animRate, (unsigned)m->flags.raw, (int)m->transformOverride,
                         (const void*)m->attachmentModel, (int)m->attachmentPartIndex,
                         (m->flags.raw & GSMODEL_FLAG_VISIBLE) != 0,
                         (int)n->startTime, (int)n->currentTime, (unsigned)n->runtimeState, (unsigned)n->flags,
                         (int)n->positionType, *(const s32*)(node + 0x88), *(const s32*)(node + 0x94),
                         *(const s32*)(node + 0x98), (int)n->attached);
            } else if (n->kind == 3) {
                const FieldParticleNode* pn = PTR32_AT_T(FieldParticleNode, node, 0x8C);
                const HSD_Generator* g = pn != NULL ? (const HSD_Generator*)pn->generator : NULL;
                char gp[48], gr[48], gs[48], lp[48], tp[48], ap[48];
                if (pn == NULL) {
                    OSReport("[waza_modellog] f=%u owner %d entry key %d particle: none (linked key %d flags 0x%X start "
                             "%d time %d state %u)\n",
                             (unsigned)now, i, (int)n->linkKey, (int)n->linkedEntryKey, (unsigned)n->flags,
                             (int)n->startTime, (int)n->currentTime, (unsigned)n->runtimeState);
                    continue;
                }
                modellog_vec(lp, sizeof lp, pn->local_position);
                modellog_vec(tp, sizeof tp, pn->transformed_position);
                if (g != NULL) {
                    modellog_vec(gp, sizeof gp, &g->pos.x);
                    modellog_vec(gr, sizeof gr, &g->rot.x);
                    modellog_vec(gs, sizeof gs, &g->scale.x);
                    if (g->appsrt != NULL) {
                        modellog_vec(ap, sizeof ap, &g->appsrt->translate.x);
                    } else {
                        snprintf(ap, sizeof ap, "none");
                    }
                }
                OSReport("[waza_modellog] f=%u owner %d entry key %d particle node %p active %u state %d linked key %d "
                         "flags 0x%X mode 0x%X emitter %d local %s transformed %s link model 0x%08X part %u | generator "
                         "%p%s%s%s%s%s%s%s%s%s id %u link %u jobj %p child %u | entry start %d time %d state %u pos type %d\n",
                         (unsigned)now, i, (int)n->linkKey, (const void*)pn, (unsigned)pn->active,
                         (int)pn->transform_state, (int)n->linkedEntryKey, (unsigned)n->flags,
                         (unsigned)n->animationMode, (int)n->field_80, lp, tp, (unsigned)pn->field_48,
                         (unsigned)pn->field_4C, (const void*)g, g ? " pos " : "", g ? gp : "", g ? " rot " : "",
                         g ? gr : "", g ? " scale " : "", g ? gs : "", g ? " appsrt " : "", g ? ap : "", "",
                         g ? (unsigned)g->idnum : 0u, g ? (unsigned)g->linkNo : 0u, g ? (const void*)g->jobj : NULL,
                         g ? (unsigned)g->numChild : 0u, (int)n->startTime, (int)n->currentTime,
                         (unsigned)n->runtimeState, (int)n->positionType);
            }
        }
    }
}

/* ---- ORRE_WAZA_SWEEP_PARTICLES=1 (2026-11-21, Studio's Grudge 288 and Hidden Power 237 questions) ----
 * Why: Studio's port of two move effects follows the game's particle code line for line, yet draws Grudge's rising
 * wisps higher and narrower and Hidden Power's streaks pink where the game's are white. ORRE_WAZA_SWEEP_MODELLOG shows
 * the generators, not the particles, so this prints every live particle (HSD's activeParticle lists, pslist.c, all 16
 * links) once per sweep frame while a part plays, with what the draw uses:
 *   - where its quad is centred: pos as stored; with an app SRT, what psDispSubAppSRT does with it (mmtx * pos, or for
 *     an app SRT with x72 set: (mmtx - translate) * pos, turned by the camera's z/x rotation, plus translate); mmtx is
 *     the matrix the last draw built (psDispSubAppSRT rebuilds it once per draw frame unless it is "still"), so a moving
 *     app SRT reads one frame behind (likely harmless: the moves asked about have none, checked from this log);
 *   - its generator (pointer, record, position, same transform) so heights above the emitter can be taken;
 *   - its colours: prim / env as getColorPrimEnv blends them (psdisp_color.h, copied: the same fixed-point formula), and
 *     the TEV registers C0 / C1 that setupTevReg would load for it (C0 = prim when PrimEnv is on or neither lighting
 *     nor trail is; C1 = env with PrimEnv, 0 without; "kept" when setupTevReg leaves the register as the previous
 *     particle set it), plus psSetupTev's case (kind & 0x80100480);
 *   - its picture: texture group, pose, the group's format and size, and a byte sum of that pose's image (to tell
 *     pictures apart without dumping disc data);
 *   - its record in the bank (the record whose command list starts closest at or below the particle's), life, size;
 *   - the raw fields behind the blend and the command stream: trail (the tail vertices' alpha factor in
 *     psDispSubMakePolygon), cmdPtr / cmdWait (how far its command list has run), and the colour animations' start,
 *     target, remain / count, so a port can line its own interpreter up step by step (the log runs once per battle
 *     frame = 2 particle steps).
 * Observe-only: fields read directly, nothing written, no game function called, no random draws. Sweep only, off
 * unless set. Plain C and OSReport, every OS (verified on Windows only). Line format in docs/port/move-viewer.md. */
extern HSD_Particle* activeParticle[PS_NUM_LINK]; /* pslist.c: the live particles, one list per link */
extern HSD_PSTexGroup** lbl_804529C8[PS_NUM_BANK]; /* particle.c (psTexGroupArray): each bank's texture groups */
extern Mtx lbl_80452DE8; /* psdisp.c: the camera's rotation about z / x (psDispSubAppSRT's x72 case), last draw */
#define PC_PTCL_PRIMENV (1u << 7)       /* psdisp.c's PrimEnv */
#define PC_PTCL_TRAIL (1u << 20)        /* Trail */
#define PC_PTCL_LIGHTING (1u << 31)     /* DispLighting */

static u8 ptcllog_lerp(u8 target, u8 from, int scale)
{
    return (u8)(((target << 16) + (from - target) * scale) >> 16);
}

/* getColorPrimEnv (psdisp_color.h): the colour part-way from the start colour to the target by remain / count */
static void ptcllog_colours(const HSD_Particle* pp, GXColor* prim, GXColor* env)
{
    if (pp->primColCount) {
        int scale = 65536 * pp->primColRemain / pp->primColCount;
        prim->r = ptcllog_lerp(pp->primColTarget.r, pp->primCol.r, scale);
        prim->g = ptcllog_lerp(pp->primColTarget.g, pp->primCol.g, scale);
        prim->b = ptcllog_lerp(pp->primColTarget.b, pp->primCol.b, scale);
        prim->a = ptcllog_lerp(pp->primColTarget.a, pp->primCol.a, scale);
    } else {
        *prim = pp->primCol;
    }
    if (pp->envColCount) {
        int scale = 65536 * pp->envColRemain / pp->envColCount;
        env->r = ptcllog_lerp(pp->envColTarget.r, pp->envCol.r, scale);
        env->g = ptcllog_lerp(pp->envColTarget.g, pp->envCol.g, scale);
        env->b = ptcllog_lerp(pp->envColTarget.b, pp->envCol.b, scale);
        env->a = ptcllog_lerp(pp->envColTarget.a, pp->envCol.a, scale);
    } else {
        *env = pp->envCol;
    }
}

static void ptcllog_mul(const f32 (*m)[4], const f32* v, f32* out)
{
    out[0] = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2] + m[0][3];
    out[1] = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2] + m[1][3];
    out[2] = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2] + m[2][3];
}

/* where psDispSubAppSRT puts a point (the particle's pos), or the point itself without an app SRT */
static void ptcllog_place(const HSD_psAppSRT* a, const f32* v, f32* out)
{
    if (a == NULL) {
        out[0] = v[0];
        out[1] = v[1];
        out[2] = v[2];
    } else if (a->x72 != 0) {
        Mtx m;
        f32 t[3];
        memcpy(m, a->mmtx, sizeof m);
        m[0][3] -= a->translate.x;
        m[1][3] -= a->translate.y;
        m[2][3] -= a->translate.z;
        ptcllog_mul((const f32(*)[4])m, v, t);
        ptcllog_mul((const f32(*)[4])lbl_80452DE8, t, out);
        out[0] += a->translate.x;
        out[1] += a->translate.y;
        out[2] += a->translate.z;
    } else {
        ptcllog_mul((const f32(*)[4])a->mmtx, v, out);
    }
}

static int ptcllog_record(const HSD_Particle* pp)
{
    int r, best = -1;
    const u8* bestStart = NULL;
    if (pp->bank >= PS_NUM_BANK || lbl_80452AC8[pp->bank] == NULL || pp->cmdList == NULL) {
        return -1;
    }
    for (r = 0; r < lbl_80452CC8[pp->bank]; r++) {
        const HSD_PSCmdList* c = lbl_80452AC8[pp->bank][r];
        if (c != NULL && c->cmdList <= pp->cmdList && (bestStart == NULL || c->cmdList > bestStart)) {
            bestStart = c->cmdList;
            best = r;
        }
    }
    return best;
}

static void ptcllog_step(u32 now)
{
    int link, count = 0;
    for (link = 0; link < PS_NUM_LINK; link++) {
        const HSD_Particle* pp;
        int guard = 0;
        for (pp = activeParticle[link]; pp != NULL && guard < 4096; pp = pp->next, guard++) {
            GXColor prim, env;
            f32 c[3], gc[3] = { 0.0f, 0.0f, 0.0f };
            char tev0[24], tev1[24], tex[96], gen[96], app[96];
            const HSD_Generator* g = pp->gen;
            const HSD_PSTexGroup* tg = NULL;
            const u32 kind = pp->kind;
            ptcllog_colours(pp, &prim, &env);
            ptcllog_place(pp->appsrt, &pp->pos.x, c);
            /* setupTevReg's two registers for this particle */
            if ((kind & PC_PTCL_PRIMENV) || (!(kind & PC_PTCL_LIGHTING) && !(kind & PC_PTCL_TRAIL))) {
                snprintf(tev0, sizeof tev0, "%u,%u,%u,%u", prim.r, prim.g, prim.b, prim.a);
                if (kind & PC_PTCL_PRIMENV) {
                    snprintf(tev1, sizeof tev1, "%u,%u,%u,%u", env.r, env.g, env.b, env.a);
                } else {
                    snprintf(tev1, sizeof tev1, "0,0,0,0");
                }
            } else {
                snprintf(tev0, sizeof tev0, "kept");
                snprintf(tev1, sizeof tev1, "kept");
            }
            if (pp->bank < PS_NUM_BANK && lbl_804529C8[pp->bank] != NULL) {
                tg = lbl_804529C8[pp->bank][pp->texGroup];
            }
            if (tg != NULL) {
                const u8* img = pp->poseNum < tg->num ? tg->texTable[pp->poseNum] : NULL;
                u32 nbytes = tg->fmt == 1 || tg->fmt == 2 ? tg->width * tg->height /* I8 / IA4 */
                             : tg->fmt == 0 ? tg->width * tg->height / 2           /* I4 */
                             : tg->fmt == 3 ? tg->width * tg->height * 2 : 0;      /* IA8 */
                u32 b, sum = 0;
                for (b = 0; img != NULL && b < nbytes; b++) {
                    sum += img[b];
                }
                snprintf(tex, sizeof tex, "%u/%u fmt %u %ux%u sum %u", (unsigned)pp->texGroup, (unsigned)pp->poseNum,
                         (unsigned)tg->fmt, (unsigned)tg->width, (unsigned)tg->height, (unsigned)sum);
            } else {
                snprintf(tex, sizeof tex, "%u/%u (no group)", (unsigned)pp->texGroup, (unsigned)pp->poseNum);
            }
            if (g != NULL) {
                ptcllog_place(g->appsrt, &g->pos.x, gc);
                snprintf(gen, sizeof gen, "%p rec %u id %u at %.3f,%.3f,%.3f", (const void*)g, (unsigned)g->gfxIdx,
                         (unsigned)g->idnum, gc[0], gc[1], gc[2]);
            } else {
                snprintf(gen, sizeof gen, "none");
            }
            if (pp->appsrt != NULL) {
                snprintf(app, sizeof app, "tr %.3f,%.3f,%.3f status %u x72 %u", pp->appsrt->translate.x,
                         pp->appsrt->translate.y, pp->appsrt->translate.z, (unsigned)pp->appsrt->status,
                         (unsigned)pp->appsrt->x72);
            } else {
                snprintf(app, sizeof app, "none");
            }
            OSReport("[waza_ptcl] f=%u link %d p %p id %u bank %u rec %d kind 0x%08X tev case 0x%08X life %u size %.3f "
                     "pos %.3f,%.3f,%.3f centre %.3f,%.3f,%.3f vel %.3f,%.3f,%.3f | gen %s | appsrt %s | prim %u,%u,%u,%u "
                     "env %u,%u,%u,%u C0 %s C1 %s | tex %s | trail %.3f cmd %u wait %u blend prim %u,%u,%u,%u->%u,%u,%u,%u "
                     "%u/%u env %u,%u,%u,%u->%u,%u,%u,%u %u/%u\n",
                     (unsigned)now, link, (const void*)pp, (unsigned)pp->idnum, (unsigned)pp->bank, ptcllog_record(pp),
                     (unsigned)kind, (unsigned)(kind & 0x80100480u), (unsigned)pp->life, pp->size, pp->pos.x, pp->pos.y,
                     pp->pos.z, c[0], c[1], c[2], pp->vel.x, pp->vel.y, pp->vel.z, gen, app, prim.r, prim.g, prim.b,
                     prim.a, env.r, env.g, env.b, env.a, tev0, tev1, tex, pp->trail, (unsigned)pp->cmdPtr,
                     (unsigned)pp->cmdWait, pp->primCol.r, pp->primCol.g, pp->primCol.b, pp->primCol.a,
                     pp->primColTarget.r, pp->primColTarget.g, pp->primColTarget.b, pp->primColTarget.a,
                     (unsigned)pp->primColRemain, (unsigned)pp->primColCount, pp->envCol.r, pp->envCol.g, pp->envCol.b,
                     pp->envCol.a, pp->envColTarget.r, pp->envColTarget.g, pp->envColTarget.b, pp->envColTarget.a,
                     (unsigned)pp->envColRemain, (unsigned)pp->envColCount);
            count++;
        }
    }
    OSReport("[waza_ptcl] f=%u total %d\n", (unsigned)now, count);
}

static void sweep_thread(u8* work)
{
    const u32 now = pc_vi_heartbeat;
    if (*(const s32*)work != 1) {
        return;
    }
    if (s_modelLog && s_sweepState >= 2 && s_item >= 0 && s_item < s_itemCount) {
        modellog_step(now); /* ORRE_WAZA_SWEEP_MODELLOG (above): every frame a part plays */
    }
    if (s_ptclLog && s_sweepState >= 2 && s_item >= 0 && s_item < s_itemCount) {
        ptcllog_step(now); /* ORRE_WAZA_SWEEP_PARTICLES (above): every frame a part plays */
    }
    if (s_hookFrames > 0 && s_hookFrame < s_hookFrames && s_sweepState >= 2 && s_owner[0] != NULL) {
        /* ORRE_WAZA_VIEWER_PTCLHOOK in the sweep (2026-11-20): the per-frame summary for the attacker's bank while a
         * part plays (its 'start' lines come from gsParticlePtclHook whenever any model starts an emitter) */
        ptclhook_frame(PTR32_AT_T(void, (u8*)s_owner[0], 0x24));
    }
    if (s_item < 0) {
        int i, hits = 0;
        sweep_build(getenv("ORRE_WAZA_SWEEP"));
        s_sweepModelA = pokemonGetStatus(NULL, PC_SWEEP_ATTACKER, POKEMON_STATUS_MODEL_ID, 0);
        s_sweepModelB = pokemonGetStatus(NULL, PC_SWEEP_TARGET, POKEMON_STATUS_MODEL_ID, 0);
        /* the project asked: confirm the pair has no per-Pokémon versions (lbl_80373750, read as sequence.c reads it) */
        for (i = 0; i < 0x16F0 / 0x10; i++) {
            const u8* e = lbl_80373750 + i * 0x10;
            const u16 model = *(const u16*)(e + 2);
            if (model != 0 && (model == s_sweepModelA || model == s_sweepModelB)) {
                OSReport("[waza_sweep] per-Pokemon version for model %u: record %d, move %u (variant %u)\n",
                         (unsigned)model, i, (unsigned)*(const u16*)(e + 4), (unsigned)e[0]);
                ++hits;
            }
        }
        OSReport("[waza_sweep] pair: Espeon model %u -> Umbreon model %u; per-Pokemon versions for them: %d\n",
                 (unsigned)s_sweepModelA, (unsigned)s_sweepModelB, hits);
        s_item = 0;
        s_sweepState = 0;
        s_stateFrame = now;
    }
    switch (s_sweepState) {
    case 0: { /* next item: free the last pair, spawn the new one with its parts loaded */
        const PcSweepItem* it;
        if (s_item >= s_itemCount) {
            if (s_item == s_itemCount) {
                sweep_free();
                OSReport("[waza_sweep] done at game frame %u\n", (unsigned)now);
                ++s_item;
            }
            return;
        }
        if (now - s_stateFrame < 20) {
            return; /* a short gap after the item before, so its end frames stay clean */
        }
        it = &s_items[s_item];
        sweep_free();
        /* the parts as the viewer's layouts load them (wazaViewerUpdateModels): 0 = the attacker part 1, the target
         * part 2; 1 = the attacker part 3; 2 = the attacker a type-4 sequence; the target stands by in 1 and 2 */
        /* the per-Pokémon pass puts the model that has its own version on its side (the attacker for parts 1 / 3,
         * the target for part 2), so the game's lookup (owner +0x70, the model id) finds that version */
        s_owner[0] = sweep_spawn(it->model != 0 && it->side == 0 ? it->model : s_sweepModelA, 0, it->move,
                                 it->layout == 0 ? 1 : it->layout == 1 ? 3 : 4, it->variant);
        s_owner[1] = sweep_spawn(it->model != 0 && it->side == 1 ? it->model : s_sweepModelB, 1, it->move,
                                 it->layout == 0 ? 2 : 0, it->variant);
        if (s_item == 0) {
            fadeSet(2, 0.5f); /* the viewer room has no fade-in of its own (as the one-model mode above) */
        }
        s_sweepState = 1;
        s_stateFrame = now;
        return;
    }
    case 1: { /* a few frames for the models to settle, then the attacker's part */
        char name[64];
        if (now - s_stateFrame < 10) {
            return;
        }
        if (!sweep_start(0)) {
            sweep_name(&s_items[s_item], name, sizeof(name));
            OSReport("[waza_sweep] item %d layout %d move %u %s: no sequence on the attacker; skipped\n", s_item,
                     s_items[s_item].layout, (unsigned)s_items[s_item].move, name);
            ++s_item;
            s_sweepState = 0;
            s_stateFrame = now;
            return;
        }
        s_sweepState = 2;
        s_playFrame = now;
        s_stateFrame = now;
        return;
    }
    case 2: /* the attacker's part playing (at most 30 s), then the target's */
    case 3: {
        char name[128]; /* the move's name, a variant and a per-Pokémon record's model */
        const int timeout = now - s_stateFrame > 1800;
        const int playing = s_owner[s_sweepState - 2] != NULL &&
                            (u8)wazaSequenceOwnerIsPlaying(s_owner[s_sweepState - 2]) != 0;
        if (playing && !timeout) {
            return;
        }
        if (s_sweepState == 2 && !timeout && sweep_start(1)) {
            s_sweepState = 3;
            s_stateFrame = now;
            return;
        }
        sweep_name(&s_items[s_item], name, sizeof(name));
        if (s_items[s_item].variant != 0) {
            const size_t len = strlen(name);
            snprintf(name + len, sizeof(name) - len, " variant %u", (unsigned)s_items[s_item].variant);
        }
        if (s_items[s_item].model != 0) {
            const size_t len = strlen(name);
            snprintf(name + len, sizeof(name) - len, " [model %u %s, record %u]", (unsigned)s_items[s_item].model,
                     s_items[s_item].side ? "target" : "attacker", (unsigned)s_items[s_item].record);
        }
        OSReport("[waza_sweep] item %d layout %d move %u %s: game frames %u-%u%s\n", s_item, s_items[s_item].layout,
                 (unsigned)s_items[s_item].move, name, (unsigned)s_playFrame, (unsigned)now,
                 timeout ? " (stopped after 30 s)" : "");
        ++s_item;
        s_sweepState = 0;
        s_stateFrame = now;
        return;
    }
    default:
        return;
    }
}

/* A joint list switch (ORRE_WAZA_VIEWER_JOINTS, ORRE_WAZA_VIEWER_HIDE): i,j,k or ranges i-k, mixed (0,4-6); part
 * indices, numbered as log_animation_and_joints says; at most PC_WAZA_VIEWER_MAX_JOINTS. Returns how many were read.
 * Why one function: both switches take the same list, so they parse it the same way. Plain C, every OS */
static int joint_list(const char* p, const char* name, int* out)
{
    int count = 0;
    while (p != NULL && *p != '\0' && count < PC_WAZA_VIEWER_MAX_JOINTS) {
        char* next;
        long n = strtol(p, &next, 10);
        long last;
        if (next == p) {
            OSReport("[waza_viewer] %s: \"%s\" is not a list of joint numbers; rest ignored\n", name, p);
            break;
        }
        last = n;
        p = next;
        if (*p == '-') {
            last = strtol(p + 1, &next, 10);
            if (next == p + 1 || last < n) {
                OSReport("[waza_viewer] %s: bad range at \"%s\"; rest ignored\n", name, p);
                break;
            }
            p = next;
        }
        for (; n <= last && count < PC_WAZA_VIEWER_MAX_JOINTS; n++) {
            out[count++] = (int)n;
        }
        while (*p == ',' || *p == ' ') {
            p++;
        }
    }
    return count;
}

__attribute__((constructor)) static void pc_waza_viewer_register(void)
{
    const char* v = getenv("ORRE_WAZA_VIEWER");
    const char* sweep = getenv("ORRE_WAZA_SWEEP");
    char* end;
    if (sweep != NULL && *sweep != '\0') {
        /* the move sweep (above): its own driver; the one-model options don't apply, except the observe-only logs
         * that also work here: ORRE_WAZA_SWEEP_MODELLOG and ORRE_WAZA_VIEWER_PTCLHOOK (2026-11-20), and
         * ORRE_WAZA_SWEEP_PARTICLES (2026-11-21) */
        f32 frames = 0.0f;
        OrreHook_FirstRoom = first_room;
        OrreHook_WazaViewerThread = sweep_thread;
        s_sweepMode = 1;
        s_modelLog = env_on("ORRE_WAZA_SWEEP_MODELLOG");
        s_ptclLog = env_on("ORRE_WAZA_SWEEP_PARTICLES");
        if (env_float("ORRE_WAZA_VIEWER_PTCLHOOK", &frames) && frames > 0.0f) {
            s_hookFrames = (int)frames;
            pc_waza_viewer_ptclhook_log = ptclhook_log; /* armed from the start, as in the one-model mode */
        }
        return;
    }
    if (v == NULL || *v == '\0') {
        return; /* off: no hook registered, vanilla */
    }
    s_species = (int)strtol(v, &end, 0);
    if (end == v || s_species <= 0) {
        OSReport("[waza_viewer] ORRE_WAZA_VIEWER=%s: expected an internal species id, e.g. 110 or 110,shiny\n", v);
        return;
    }
    s_shiny = strstr(end, "shiny") != NULL;
    s_backdrop = env_on("ORRE_WAZA_VIEWER_BACKDROP");
    s_dots = env_on("ORRE_WAZA_VIEWER_DOTS");
    s_hold = env_float("ORRE_WAZA_VIEWER_FRAME", &s_holdFrame);
    s_haveAngle = env_float("ORRE_WAZA_VIEWER_ANGLE", &s_angle);
    s_haveDistance = env_float("ORRE_WAZA_VIEWER_DISTANCE", &s_distance);
    s_haveHeight = env_float("ORRE_WAZA_VIEWER_HEIGHT", &s_height);
    s_haveFov = env_float("ORRE_WAZA_VIEWER_FOV", &s_fov);
    s_haveAimY = env_float("ORRE_WAZA_VIEWER_AIM_Y", &s_aimY);
    s_lights = env_on("ORRE_WAZA_VIEWER_LIGHTS");
    s_texLog = env_on("ORRE_WAZA_VIEWER_TEXLOG");
    {
        f32 frames = 0.0f;
        if (env_float("ORRE_WAZA_VIEWER_PTCL", &frames) && frames > 0.0f) {
            s_ptclFrames = (int)frames;
        }
        frames = 0.0f;
        if (env_float("ORRE_WAZA_VIEWER_PTCLHOOK", &frames) && frames > 0.0f) {
            s_hookFrames = (int)frames;
            pc_waza_viewer_ptclhook_log = ptclhook_log; /* armed from the start, so the spawn's own keys are seen */
        }
    }
    s_jointCount = joint_list(getenv("ORRE_WAZA_VIEWER_JOINTS"), "ORRE_WAZA_VIEWER_JOINTS", s_joints);
    s_hideCount = joint_list(getenv("ORRE_WAZA_VIEWER_HIDE"), "ORRE_WAZA_VIEWER_HIDE", s_hide);
    {
        int one[PC_WAZA_VIEWER_MAX_JOINTS];
        if (joint_list(getenv("ORRE_WAZA_VIEWER_DRAWMTX"), "ORRE_WAZA_VIEWER_DRAWMTX", one) > 0) {
            s_drawMtx = one[0]; /* one joint; the list parser is reused so the number reads the same way */
        }
    }
    OrreHook_FirstRoom = first_room;
    OrreHook_WazaViewerThread = viewer_thread;
    OrreHook_WazaViewerDraw = viewer_draw;
}

#endif /* TARGET_PC */
