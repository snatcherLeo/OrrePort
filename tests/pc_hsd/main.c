/*
 * PC rendering test for HSD (toward Milestone 1).
 *
 * Step 1: start the platform the way orreport does (GameCube memory at 0x80000000, Aurora, OSInit), initialise HSD
 * through the game's own HSD_InitComponent, read one model archive into game memory, relocate it
 * (HSD_ArchiveParse, the disc marks in include/pc/disc_types.h), build the runtime objects with HSD_JObjLoadJoint
 * and report what was built. Step 2 (next) draws one frame through Aurora with a fixed camera.
 *
 *   pc_hsd_test.exe <model archive .dat> [--frames N]
 *
 * The archive comes from extracted/ for now (a development shortcut, never committed); B's disc access replaces it.
 */
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>

#include "hsd/hsd_archive.h"
#include "hsd/hsd_jobj.h"
#include "hsd/hsd_dobj.h"
#include "hsd/hsd_mobj.h"
#include "hsd/hsd_pobj.h"
#include "hsd/hsd_tobj.h"
#include "hsd/hsd_cobj.h"
#include "hsd/hsd_lobj.h"

#include "pc_crash.h"
#include "pc_lowmem.h"
#include "pc_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void HSD_InitComponent(void);
void* fn_801A6928(u32 size); /* HSD_MemAlloc */
void pc_hsd_data_init(void);
void pc_hsd_scan_arrays(HSD_JObj* root); /* array_sizes.c */
int pc_hsd_array_count(void);
void fn_801A13CC(HSD_JObj* jobj, MtxPtr vmtx, u32 flags, u32 rendermode); /* HSD_JObjDispAll */
void fn_8019C708(s32 pass);                                             /* HSD_StartRender */
void fn_80195A48(void); /* HSD_CObjEndCurrent */
void HSD_LObjAddCurrent(HSD_LObj* lobj);
extern unsigned int pc_hsd_dl_calls, pc_hsd_dl_bytes; /* platform_gaps.c */
extern int pc_hsd_trace, pc_hsd_notex, pc_hsd_nolight, pc_hsd_cullswap, pc_hsd_nofog; /* gx_trace.c */
extern const unsigned char *pc_hsd_array_lo, *pc_hsd_array_hi; /* platform_gaps.c: the archive's data block */  /* hsd_data_pc.c: typed HSD data */
void pc_data_relocate(void);  /* build-pc/generated (pcdata.py): pointer slots in generated data */
BOOL fn_8019C3C4(int param, ...); /* HSD_SetInitParameter: 1 = XFB count, 4 = render mode */

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int len) {
    static const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    FILE* out = level >= LOG_ERROR ? stderr : stdout;
    (void)len;
    fprintf(out, "[%s: %s; %s]\n", (unsigned)level < 5 ? names[level] : "?", module, message);
    if (level == LOG_FATAL) {
        fflush(out);
        abort();
    }
}

/* counts of the runtime objects HSD built */
static int n_jobj, n_dobj, n_mobj, n_tobj, n_pobj;

static void count_jobj(HSD_JObj* jobj) {
    for (; jobj != NULL; jobj = jobj->next) {
        n_jobj++;
        if (!(jobj->flags & (JOBJ_SPLINE | JOBJ_PTCL))) {
            HSD_DObj* dobj;
            for (dobj = jobj->u.dobj; dobj != NULL; dobj = dobj->next) {
                HSD_PObj* pobj;
                HSD_TObj* tobj;
                n_dobj++;
                if (dobj->mobj != NULL) {
                    n_mobj++;
                    for (tobj = dobj->mobj->tobj; tobj != NULL; tobj = tobj->next) {
                        n_tobj++;
                    }
                }
                for (pobj = dobj->pobj; pobj != NULL; pobj = pobj->next) {
                    n_pobj++;
                }
            }
        }
        if (!(jobj->flags & JOBJ_INSTANCE)) {
            count_jobj(jobj->child);
        }
    }
}

static u8* read_file(const char* path, u32* size) {
    FILE* f = fopen(path, "rb");
    long n;
    u8* buf;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = fn_801A6928((u32)n); /* HSD_MemAlloc: HSD's heap in game memory (below 4 GB, as the archive's pointers
                                  require); HSD_InitComponent gives HSD the whole arena, as on the GameCube */
    if (buf != NULL && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        buf = NULL;
    }
    fclose(f);
    *size = (u32)n;
    return buf;
}

/* Step 2: draw the model every frame through HSD, with a fixed camera and two lights, in the game's order
 * (HSD_StartRender, camera current, lights, the three transparency passes, camera end). */
static int AMB = 0x50, trace = 0, nosun = 0, use_scene = 0, n_scene_lights = 0;
#define MAX_ROOTS 64
static HSD_JObj* roots[MAX_ROOTS];
static int n_roots;
static HSD_CObj* scene_cobj; /* --scene: the archive's own camera */
static int only_root = -1;    /* --root N: draw only model set N */
void fn_801A2B5C(HSD_JObj* jobj, HSD_AnimJoint* aj, HSD_MatAnimJoint* mj, HSD_ShapeAnimJoint* sj); /* HSD_JObjAddAnimAll (jobj.c) */
static int anim_index = -1;   /* --anim N: play joint animation N of each model set (and its material animation) */
static int anim_loop = 120;   /* --animlen F: restart the animation every F frames */
static void render(int frames, f32 dist, f32 height) {
    HSD_CObj* cobj = scene_cobj != NULL ? scene_cobj : HSD_CObjAlloc();
    HSD_LObj* ambient = HSD_LObjAlloc();
    HSD_LObj* sun = HSD_LObjAlloc();
    Vec eye = {0.0f, height, dist}, at = {0.0f, height, 0.0f}, sun_pos = {dist, dist, dist};
    GXColor grey = {AMB, AMB, AMB, 0xFF}, white = {0xFF, 0xFF, 0xFF, 0xFF};
    int frame = 0, exiting = 0;

    if (scene_cobj == NULL) {
        HSD_CObjSetViewportfx4(cobj, 0.0f, 640.0f, 0.0f, 480.0f);
        HSD_CObjSetScissorx4(cobj, 0, 640, 0, 480);
        HSD_CObjSetEyePosition(cobj, &eye);
        HSD_CObjSetInterest(cobj, &at);
        HSD_CObjSetPerspective(cobj, 30.0f, 640.0f / 480.0f);
        HSD_CObjSetNear(cobj, 1.0f);
        HSD_CObjSetFar(cobj, dist * 10.0f);
    }

    if (n_scene_lights == 0) {
        HSD_LObjSetFlags(ambient, LOBJ_AMBIENT | LOBJ_DIFFUSE);
        HSD_LObjSetColor(ambient, grey);
        HSD_LObjSetFlags(sun, LOBJ_INFINITE | LOBJ_DIFFUSE);
        HSD_LObjSetColor(sun, white);
        HSD_LObjSetPosition(sun, &sun_pos);
        HSD_LObjAddCurrent(ambient);
        if (!nosun) {
            HSD_LObjAddCurrent(sun);
        }
    }

    while (!exiting && frame < frames) {
        const AuroraEvent* event = aurora_update();
        while (event != NULL && event->type != AURORA_NONE) {
            if (event->type == AURORA_EXIT) {
                exiting = 1;
            }
            ++event;
        }
        if (exiting || !aurora_begin_frame()) {
            continue;
        }
        GXSetCopyClear((GXColor){0x30, 0x40, 0x60, 0xFF}, GX_MAX_Z24);
        if (frame == 0) {
            pc_hsd_trace = trace; /* trace the second frame (the first one also sets up state once) */
        }
        if (anim_index >= 0) {
            int r;
            for (r = 0; r < n_roots; r++) {
                if (frame % anim_loop == 0) {
                    HSD_JObjReqAnimAll(roots[r], 0.0f);
                }
                HSD_JObjAnimAll(roots[r]); /* advances one frame: keyframe streams through fobj.c */
            }
            if (getenv("PC_HSD_ANIMDBG") && (frame == 1 || frame == 20 || frame == 40)) {
                HSD_JObj* j = roots[0]->child;
                int k;
                for (k = 0; k < 3 && j != NULL; k++, j = j->child) {
                    printf("[anim] frame %d joint %d: rot %.3f %.3f %.3f pos %.3f %.3f %.3f aobj %p\n", frame, k,
                           j->rotate_x, j->rotate_y, j->rotate_z, j->translate_x, j->translate_y, j->translate_z,
                           (void*)j->aobj);
                }
            }
        }
        fn_8019C708(0); /* HSD_RP_SCREEN */
        if (HSD_CObjSetCurrent(cobj)) {
            int r;
            HSD_LObjSetup(cobj);
            for (r = 0; r < n_roots; r++) {
                if (only_root >= 0 && r != only_root) continue;
                fn_801A13CC(roots[r], NULL, HSD_TRSP_OPA, 0);
            }
            for (r = 0; r < n_roots; r++) {
                if (only_root >= 0 && r != only_root) continue;
                fn_801A13CC(roots[r], NULL, HSD_TRSP_TEXEDGE, 0);
            }
            for (r = 0; r < n_roots; r++) {
                if (only_root >= 0 && r != only_root) continue;
                fn_801A13CC(roots[r], NULL, HSD_TRSP_XLU, 0);
            }
            fn_80195A48(); /* HSD_CObjEndCurrent */
        } else if (frame == 0) {
            printf("[pc_hsd_test] HSD_CObjSetCurrent refused the camera\n");
        }
        aurora_end_frame();
        if (frame == 0) {
            printf("[pc_hsd_test] first frame drawn: %u display lists, %u bytes\n", pc_hsd_dl_calls, pc_hsd_dl_bytes);
            fflush(stdout);
        }
        frame++;
    }
    printf("[pc_hsd_test] %d frames drawn\n", frame);
}

int main(int argc, char* argv[]) {
    static HSD_Archive archive;
    const char* path = NULL;
    u32 size = 0;
    u8* data;
    DISC_PTR(void)* scene;
    HSD_JObj* root;
    int i, frames = 0;
    f32 dist = 100.0f, height = 20.0f;

    setvbuf(stdout, NULL, _IONBF, 0); /* progress reaches a redirected log even if the run is stopped */
    pc_crash_install();
    if (!pc_mem_reserve(PC_MEM1_RETAIL_SIZE) || !pc_mem_map_locked_cache() || !pc_lowmem_init()) {
        fprintf(stderr, "[pc_hsd_test] cannot map GameCube memory below 4 GB\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dist") == 0 && i + 1 < argc) {
            dist = (f32)atof(argv[++i]);
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            only_root = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--anim") == 0 && i + 1 < argc) {
            anim_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--animlen") == 0 && i + 1 < argc) {
            anim_loop = atoi(argv[++i]);
            if (anim_loop <= 0) anim_loop = 120;
        } else if (strcmp(argv[i], "--scene") == 0) {
            use_scene = 1;
        } else if (strcmp(argv[i], "--nofog") == 0) {
            pc_hsd_nofog = 1;
        } else if (strcmp(argv[i], "--cullswap") == 0) {
            pc_hsd_cullswap = 1;
        } else if (strcmp(argv[i], "--nolight") == 0) {
            pc_hsd_nolight = 1;
        } else if (strcmp(argv[i], "--nosun") == 0) {
            nosun = 1;
        } else if (strcmp(argv[i], "--c0only") == 0) {
            pc_hsd_notex = 3;
        } else if (strcmp(argv[i], "--rasonly") == 0) {
            pc_hsd_notex = 4;
        } else if (strcmp(argv[i], "--white") == 0) {
            pc_hsd_notex = 2;
        } else if (strcmp(argv[i], "--notex") == 0) {
            pc_hsd_notex = 1;
        } else if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ambient") == 0 && i + 1 < argc) {
            AMB = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = (f32)atof(argv[++i]);
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }
    if (path == NULL) {
        fprintf(stderr, "usage: pc_hsd_test <model archive .dat>\n");
        return 2;
    }

    const AuroraConfig config = {
        .appName = "pc_hsd_test",
        .logCallback = &log_callback,
        .mem1Size = PC_MEM1_RETAIL_SIZE,
        .mem1Base = pc_mem_from_gc(PC_MEM1_BASE),
    };
    aurora_initialize(argc, argv, &config);
    OSInit();
    if (getenv("PC_HSD_STEPS")) printf("[pc_hsd_test] step: %s\n", "OSInit();");
    pc_data_relocate();
    if (getenv("PC_HSD_STEPS")) printf("[pc_hsd_test] step: %s\n", "pc_data_relocate();");
    pc_hsd_data_init();
    if (getenv("PC_HSD_STEPS")) printf("[pc_hsd_test] step: %s\n", "pc_hsd_data_init();");
    /* the game's main does this before anything allocates (main.c: OSInitAlloc on the arena, one heap slot; HSD's
     * heap is then created from what is left). Without it every OS heap allocation fails. */
    OSSetArenaLo(OSInitAlloc(OSGetArenaLo(), OSGetArenaHi(), 1));

    /* the game's order (GSgfx init, gs_gfx_candidate_800D37D4.c): XFB count, render mode, then HSD */
    fn_8019C3C4(1, 2);
    if (getenv("PC_HSD_STEPS")) printf("[pc_hsd_test] step: %s\n", "fn_8019C3C4(1, 2);");
    fn_8019C3C4(4, &GXNtsc480IntDf);
    if (getenv("PC_HSD_STEPS")) printf("[pc_hsd_test] step: %s\n", "fn_8019C3C4(4, &GXNtsc480IntDf);");
    HSD_InitComponent();
    printf("[pc_hsd_test] HSD_InitComponent done\n");

    data = read_file(path, &size);
    if (data == NULL) {
        fprintf(stderr, "[pc_hsd_test] cannot read %s\n", path);
        return 1;
    }
    if (HSD_ArchiveParse(&archive, data, size) != 0) {
        fprintf(stderr, "[pc_hsd_test] HSD_ArchiveParse failed\n");
        return 1;
    }
    pc_hsd_array_lo = archive.data;
    pc_hsd_array_hi = archive.data + archive.header.data_size;
    scene = HSD_ArchiveGetPublicAddress(&archive, "scene_data");
    if (scene == NULL) {
        fprintf(stderr, "[pc_hsd_test] no scene_data\n");
        return 1;
    }
    {
        /* scene_data +0: model sets (null-terminated); a model set +0: the root joint (hsd.md). A Pokemon model has
         * one set, a map several. */
        DISC_PTR(void)* sets = DP(scene[0]);
        for (n_roots = 0; n_roots < MAX_ROOTS && sets != NULL && DP(sets[n_roots]) != NULL; n_roots++) {
            DISC_PTR(HSD_Joint)* set = DP(sets[n_roots]);
            roots[n_roots] = HSD_JObjLoadJoint(DP(set[0]));
            if (anim_index >= 0) {
                /* a model set: +0 root joint, +4 joint animations, +8 material animations (null-terminated lists) */
                typedef DISC_PTR(void) VoidSlot;
                typedef DISC_PTR(HSD_AnimJoint) AnimSlot;
                typedef DISC_PTR(HSD_MatAnimJoint) MatAnimSlot;
                VoidSlot* vset = (VoidSlot*)set;
                AnimSlot* anims = DP(vset[1]);
                MatAnimSlot* matanims = DP(vset[2]);
                int n_anims = 0, n_mat = 0;
                while (anims != NULL && DP(anims[n_anims]) != NULL) n_anims++;
                while (matanims != NULL && DP(matanims[n_mat]) != NULL) n_mat++;
                printf("[pc_hsd_test] model set %d: %d joint animations, %d material animations\n", n_roots, n_anims,
                       n_mat);
                if (anim_index < n_anims) {
                    fn_801A2B5C(roots[n_roots], DP(anims[anim_index]), /* HSD_JObjAddAnimAll */
                                       anim_index < n_mat ? DP(matanims[anim_index]) : NULL, NULL);
                    HSD_JObjReqAnimAll(roots[n_roots], 0.0f);
                }
            }
            count_jobj(roots[n_roots]);
            pc_hsd_scan_arrays(roots[n_roots]);
        }
        root = roots[0];
        if (use_scene) {
            /* scene_data +4: one camera set (+0 the camera description); +8: the light sets (+0 each one's light
             * description), as the map loader registers them (floorReadMapPostFunc, hsd.md) */
            DISC_PTR(void)* camset = DP(scene[1]);
            DISC_PTR(void)* lights = DP(scene[2]);
            if (camset != NULL && DP(camset[0]) != NULL) {
                scene_cobj = HSD_CObjLoadDesc((HSD_CObjDesc*)DP(camset[0]));
            }
            for (i = 0; lights != NULL && DP(lights[i]) != NULL; i++) {
                DISC_PTR(HSD_LightDesc)* lset = DP(lights[i]);
                HSD_LObjAddCurrentAll(HSD_LObjLoadDesc(DP(lset[0])));
                n_scene_lights++;
            }
            printf("[pc_hsd_test] scene camera %s, %d scene lights\n", scene_cobj != NULL ? "loaded" : "missing",
                   n_scene_lights);
        }
    }
    printf("[pc_hsd_test] %d model set(s), %d vertex arrays sized from the display lists\n", n_roots,
           pc_hsd_array_count());
    (void)root;
    printf("[pc_hsd_test] %s: %u bytes, %u relocations; runtime objects: joints %d, meshes %d, materials %d, "
           "textures %d, shapes %d\n",
           path, (unsigned)size, (unsigned)archive.header.nb_reloc, n_jobj, n_dobj, n_mobj, n_tobj, n_pobj);

    if (frames > 0) {
        render(frames, dist, height);
    }
    aurora_shutdown();
    return 0;
}
