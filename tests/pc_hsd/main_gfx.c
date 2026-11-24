/*
 * PC test of the game's own graphics layer: GSgfx, its layers, lights and cameras on top of HSD, run the
 * way the game's start-up and main loop run them (src/game/main.c: fn_800057B0's GSgfxInit call, fn_80005D80's
 * per-frame calls; gfx.md "Flow"). Same start-up as main.c up to the point where the game hands over to GSgfx.
 *
 *   pc_gfx_test.exe [--frames N]
 *
 * Step 1: GSgfxInit + empty frames (begin, end, pace). Later steps draw through GSgfx.
 */
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include <dolphin/os.h>
#include <dolphin/gx/GXAurora.h>

#include "hsd/hsd_archive.h"
#include "hsd/hsd_jobj.h"
#include "hsd/hsd_cobj.h"
#include "hsd/hsd_lobj.h"
#include "hsd/hsd_tobj.h"
#include "game/gs_model_struct.h"
#include "game/gs_model_shadow.h"
#include "game/gs_texture.h"
#include "game/gs_gfx_layer.h"
#include "game/gs_light.h"
#include "hsd/hsd_lobj.h"

#include "pc_crash.h"
#include "pc_lowmem.h"
#include "pc_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pc_data_relocate(void);  /* build-pc/generated (pcdata.py): pointer slots in generated data */
void pc_hsd_data_init(void);  /* hsd_data_pc.c */

/* gs_vm.c's native-call argument images: B's thread library refers to them; the VM is not part of this test */
f32* lbl_8047AC3C;
u32* lbl_8047AC40;

#include <stdarg.h>
/* GSlogWrite (gs_log.cpp, not built here): the game's debug log, printed */
void GSlogWrite(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    fputs("[GSlog] ", stdout);
    vprintf(format, ap);
    va_end(ap);
}

void GSmemInit(u32 mode, void* lo, void* hi); /* gs_mem.c */
void GSscratchInit(u32 mode);                  /* gs_scratch_800EE928.c */

/* GSgfx (src/game/gs_gfx*.c; gfx.md) */
void GSgfxInit__FP15_GSgfxInitParms(u32 requestBufferSize, u32 mtxStackCount, u32 vtxDescCount, u32 dlCaptureCount,
                                    u32 videoMode, u32 efbHeight);
void fn_800D3410(void* target, u8 doUpdates); /* frame begin */
void fn_800D3190(void);                       /* frame end */
void fn_800D3FA4(u32 flags, u8 setupCamera, u8 resetQueue); /* plays the recorded requests back */
void fn_800D361C(u8 mode);                    /* frame pacing (waits for the retrace) */
void gfxSetFlags(u32 mask);
void gfxClearFlags(u32 mask);
void gfxSetOrtho(f32 a, f32 b, f32 c, f32 d);
void gfxSetBlendMode(s32 a, s32 b, s32 c);
void gfxSetUpdates(s32 a, s32 b, s32 c);
void gfxSetZMode(s32 enable, s32 func, s32 update);
void gfxSetCullMode(u32 idx);
void gfxSetPrimitive(u32 val);
void gfxSetVtxDesc(void* desc);
void gfxBegin(u16 count);
void gfxVtxPos(f32 x, f32 y, f32 z);
void fn_800D5CB8(u32 slot, u8 r, u8 g, u8 b, u8 a); /* vertex colour */
void gfxEnd(void);

/* --model PATH: a Pokemon loaded and drawn by the game's own model code (GSmodelLoad on the archive's model set;
 * the playback calls the real GSmodelDrawAll) with the game's camera code (GScamera, gs_render_util*.c). */
void* fn_801A6928(u32 size);                                              /* HSD_MemAlloc */
void pc_hsd_scan_arrays(HSD_JObj* root);                                  /* array_sizes.c */
extern const unsigned char *pc_hsd_array_lo, *pc_hsd_array_hi;           /* platform_gaps.c */
void fn_800D2AD4(u32 count);       /* camera pool */
void* fn_800D29A0(void);           /* a camera with default settings */
void fn_800D258C(void* camera);    /* make it the active camera */
void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(void); /* hands the active camera's view to HSD */
void GScameraSetPosition(void* camera, const Vec* eye);
void GScameraLookAt(void* camera, const Vec* up, const Vec* interest);
void GScameraSetPerspective(void* camera, f32 fov, f32 aspect, f32 near, f32 far);
void GSlightInit(u32 count);
void GStextureInit(u32 count);
void GSmodelInit(u32 count);
void GSpartInit(u32 count);
void GSmaterialInit(u32 count);
void* GSmodelLoad(void* modelSet, void* unused, void* boundAnim);

/* the sprite environment's globals (gs_thread_hi_range_800FE35C.c: spriteSetEnv's scale and window offset), with the
 * game's start values: scale 1.0, offset 0 */
f32 lbl_80478B10 = 1.0f;
f32 lbl_80478B14 = 1.0f;
s16 lbl_8047AC70[4];
s16 lbl_8047AC72[6];

static void* gs_model;
static void* room_camera; /* --room: the room's camera */
static int gs_anim = -1; /* --anim N: play the model's animation N through GSmodel */
static int gs_anim_all; /* --anim-all: every animation in turn, 4 frames each, texture animation of the same index */
static int blend_a = -1, blend_b = -1; /* --blend A B F: animations A and B blended by F (the field walkers' path) */
static f32 blend_f;
static int gs_texanim = -1; /* --texanim N: play its texture animation N (eye / mouth swaps) */
void GSmodelSetAnimType(void* model, u32 type); /* gs_model_anim (no canonical prototype yet) */

/* the game's main sets these pools up in this order before anything loads (main.c fn_800057B0); once */
static void init_pools(void) {
    static int done;
    if (done) {
        return;
    }
    done = 1;
    GStextureInit(0x10);
    GSmodelInit(0x40);
    GSpartInit(0x8);
    GSmaterialInit(0x400);
    fn_800D2AD4(0x20); /* 32 cameras */
    GSlightInit(0x20); /* 32 lights */
    {
        void gsParticleInit(u32 banks, u32 nodes);
        gsParticleInit(0x20, 0x80); /* 32 particle banks, 128 emitter nodes (main.c) */
    }
}

/* a model archive's first model set (scene_data -> model sets -> set 0), parsed into ARCHIVE */
static void* open_model_set(const char* path, HSD_Archive* archive) {
    typedef DISC_PTR(void) VoidSlot;
    FILE* f = fopen(path, "rb");
    long n;
    u8* data;
    VoidSlot* scene;
    VoidSlot* sets;
    VoidSlot* set;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = fn_801A6928((u32)n);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    if (HSD_ArchiveParse(archive, data, (u32)n) != 0) {
        return NULL;
    }
    pc_hsd_array_lo = archive->data;
    pc_hsd_array_hi = archive->data + archive->header.data_size;
    scene = HSD_ArchiveGetPublicAddress(archive, "scene_data");
    if (scene == NULL || (sets = DP(scene[0])) == NULL || (set = DP(sets[0])) == NULL) {
        return NULL;
    }
    return set;
}

static int load_model(const char* path) {
    static HSD_Archive archive;
    void* set = open_model_set(path, &archive);
    if (set == NULL) {
        return 0;
    }
    init_pools();
    gs_model = GSmodelLoad(set, NULL, NULL);
    if (gs_model == NULL) {
        return 0;
    }
    pc_hsd_scan_arrays(((GSmodel*)gs_model)->renderHsdJObj);
    GSmodelSetVisibility(gs_model, 1); /* as the game does after loading a model */
    if (getenv("PC_GFX_PEDESC") != NULL) {
        /* PC_GFX_PEDESC="12 hex bytes": apply an HSD_PEDesc (blend / alpha test / z) to every material, as the menu
         * model renderer does with its own (menuModelRender: GSmodelSetPEdescr(model, lbl_8035B468)) */
        static u8 pe[16];
        unsigned v[12];
        int i, n = sscanf(getenv("PC_GFX_PEDESC"), "%x %x %x %x %x %x %x %x %x %x %x %x", &v[0], &v[1], &v[2], &v[3],
                          &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11]);
        for (i = 0; i < n && i < 12; i++) {
            pe[i] = (u8)v[i];
        }
        GSmodelSetPEdescr(gs_model, pe);
        printf("[pc_gfx_test] PE descriptor applied (%d bytes)\n", n);
    }
    if (gs_anim >= 0) {
        /* through the game's model animation code; GSgfx's per-frame updates advance it (fn_800E3884) */
        GSmodelSetAnimIndex(gs_model, (u32)gs_anim);
        GSmodelSetAnimType(gs_model, 1); /* loop (likely: the game's idle animations use 1) */
        GSmodelStartAnimation(gs_model);
    }
    if (gs_texanim >= 0) {
        void GSmodelSetTexAnimType(void* model, u32 type);
        void GSmodelSetTexAnimIndex(void* model, u32 index);
        void GSmodelStartTexAnimation(void* model);
        u32 GSmodelCanTexAnimate(void* model);
        GSmodelSetTexAnimIndex(gs_model, (u32)gs_texanim);
        GSmodelSetTexAnimType(gs_model, 1);
        GSmodelStartTexAnimation(gs_model);
        printf("[pc_gfx_test] texture animation %d: can animate %u\n", gs_texanim, GSmodelCanTexAnimate(gs_model));
    }
    printf("[pc_gfx_test] GSmodel flags 0x%08x\n", ((GSmodel*)gs_model)->flags.raw);
    return 1;
}

/* --room PATH: a map room's archive set up the way floorInitMap / floorReadMapPostFunc do it (floor.c,
 * field_range_80114AE0.c; floor-resources.md), minus the resource registry: every model set through GSmodelLoad (made
 * visible here; the game shows them later), the camera through fn_800D27FC and made active, every light through
 * GSlightLoad, set active and its animation 0 started, as the game's code does. */
void* fn_800D27FC(void* cameraDesc);  /* GScamera from a description (scene_data +4) */
void* GSlightLoad(void* lightDesc);   /* GSlight from a description (each entry of scene_data +8) */
void GSlightSetActive(void* light, u8 active);
u8 GSlightCanAnimate(void* light);
void GSlightSetAnimIndex(void* light, u32 index);
void GSlightSetAnimRate(void* light, f32 rate);
void GSlightStartAnimation(void* light);

static void* room_models[64]; /* --shadow: the room's models and its first directional light */
static int room_model_count;
static GSlight* room_sun;

static HSD_Archive room_archive; /* --room's archive (also registered as a resource by register_room_resources) */

static int load_room(const char* path) {
#define archive room_archive
    typedef DISC_PTR(void) VoidSlot;
    FILE* f = fopen(path, "rb");
    long n;
    u8* data;
    VoidSlot* scene;
    VoidSlot* list;
    void* camera;
    int i, models = 0, lights = 0;
    if (f == NULL) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = fn_801A6928((u32)n);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return 0;
    }
    fclose(f);
    if (HSD_ArchiveParse(&archive, data, (u32)n) != 0) {
        return 0;
    }
    pc_hsd_array_lo = archive.data;
    pc_hsd_array_hi = archive.data + archive.header.data_size;
    scene = HSD_ArchiveGetPublicAddress(&archive, "scene_data");
    if (scene == NULL) {
        return 0;
    }
    init_pools();
    for (list = DP(scene[0]), i = 0; list != NULL && DP(list[i]) != NULL; i++) {
        void* model = GSmodelLoad(DP(list[i]), NULL, NULL);
        if (model != NULL) {
            pc_hsd_scan_arrays(((GSmodel*)model)->renderHsdJObj);
            GSmodelSetVisibility(model, 1);
            if (room_model_count < 64) {
                room_models[room_model_count++] = model;
            }
            models++;
        }
    }
    if (getenv("PC_GFX_ROOM_ANIM") != NULL) {
        /* PC_GFX_ROOM_ANIM="set:frame,set:frame": start those room models' animation 0 at that frame, rate 0.5 (the
         * rate the game uses for S1_out's animated sets; D read set 4 at 452 and set 1 at 94 from Dolphin's RAM) */
        const char* p = getenv("PC_GFX_ROOM_ANIM");
        int set, n;
        float frame;
        while (sscanf(p, "%d:%f%n", &set, &frame, &n) == 2) {
            if (set >= 0 && set < room_model_count) {
                GSmodelSetAnimIndex(room_models[set], 0);
                GSmodelSetAnimFrame(room_models[set], frame);
                GSmodelSetAnimRate(room_models[set], 0.5f);
                GSmodelStartAnimation(room_models[set]);
                printf("[pc_gfx_test] room set %d animation from frame %.1f\n", set, frame);
            }
            p += n;
            if (*p != ',') {
                break;
            }
            p++;
        }
    }
    for (list = DP(scene[2]), i = 0; list != NULL && DP(list[i]) != NULL; i++) {
        void* light = GSlightLoad(DP(list[i]));
        if (light != NULL) {
            GSlightSetActive(light, 1);
            if (room_sun == NULL && (((GSlight*)light)->lobj->flags & 3) != 0) {
                room_sun = light; /* LObj type 1-3 (infinite, point, spot): one that can cast (GSmodelSetShadowLight) */
            }
            if (GSlightCanAnimate(light)) {
                GSlightSetAnimIndex(light, 0);
                GSlightSetAnimRate(light, 0.5f);
                GSlightStartAnimation(light);
            }
            lights++;
        }
    }
    camera = DP(scene[1]) != NULL ? fn_800D27FC(DP(scene[1])) : NULL;
    room_camera = camera;
    if (camera != NULL) {
        void GScameraStartAnimation(void* camera);
        /* the description's camera animation 0 is attached but not running; with look-at off the view would come
         * from the (zero) rotation. Run it, so the view is the room's authored camera (cameraSetGScamera's callers
         * decide this in the game; likely) */
        if (getenv("PC_GFX_ROOM_EYE") != NULL) {
            /* PC_GFX_ROOM_EYE="x y z" [PC_GFX_ROOM_AT="x y z"] [PC_GFX_ROOM_FOV=deg]: the room's own camera (and so
             * the room's own lights) placed by hand instead of its authored animation, to match a Dolphin view */
            Vec eye, at = {50.0f, 10.0f, 70.0f}, up = {0.0f, 1.0f, 0.0f};
            f32 fov, aspect, znear, zfar;
            sscanf(getenv("PC_GFX_ROOM_EYE"), "%f %f %f", &eye.x, &eye.y, &eye.z);
            if (getenv("PC_GFX_ROOM_AT") != NULL) {
                sscanf(getenv("PC_GFX_ROOM_AT"), "%f %f %f", &at.x, &at.y, &at.z);
            }
            GScameraGetPerspective(camera, &fov, &aspect, &znear, &zfar);
            if (getenv("PC_GFX_ROOM_FOV") != NULL) {
                fov = (f32)atof(getenv("PC_GFX_ROOM_FOV"));
            }
            GScameraSetPerspective(camera, fov, aspect, znear, zfar);
            GScameraSetPosition(camera, &eye);
            GScameraLookAt(camera, &up, &at);
            printf("[pc_gfx_test] room camera placed: eye %.1f %.1f %.1f at %.1f %.1f %.1f fov %.1f near %.1f far %.1f\n",
                   eye.x, eye.y, eye.z, at.x, at.y, at.z, fov, znear, zfar);
        } else {
            GScameraStartAnimation(camera);
        }
        fn_800D258C(camera);
    }
    printf("[pc_gfx_test] room: %d models, %d lights, camera %p\n", models, lights, camera);
    return models > 0 && camera != NULL;
#undef archive
}

static void setup_model_scene(void) {
    void* camera;
    Vec eye = {0.0f, 1.5f, 12.0f}, up = {0.0f, 1.0f, 0.0f}, at = {0.0f, 1.5f, 0.0f}, sun_pos = {12.0f, 12.0f, 12.0f};
    GXColor grey = {0x50, 0x50, 0x50, 0xFF}, white = {0xFF, 0xFF, 0xFF, 0xFF};

    init_pools();
    camera = fn_800D29A0();
    printf("[pc_gfx_test] camera %p\n", camera);
    if (camera == NULL) {
        return;
    }
    if (getenv("PC_GFX_EYE") != NULL) { /* PC_GFX_EYE="x y z": the camera elsewhere, still looking at the model */
        sscanf(getenv("PC_GFX_EYE"), "%f %f %f", &eye.x, &eye.y, &eye.z);
    }
    GScameraSetPosition(camera, &eye);
    GScameraLookAt(camera, &up, &at);
    GScameraSetPerspective(camera, 30.0f, 640.0f / 480.0f, 1.0f,
                           getenv("PC_GFX_FAR") ? (f32)atof(getenv("PC_GFX_FAR")) : 120.0f); /* PC_GFX_FAR: far plane */
    fn_800D258C(camera);
    {
        /* two lights through the game's GSlight code (GSlightCreate builds each from its own disc-layout
         * descriptions): a grey ambient and a white directional sun, as the HSD-only test uses */
        typedef struct { f32 r, g, b; } LightColor; /* GSlightColor (gs_light_candidate_800DCC84.c) */
        void* GSlightCreate(void);
        void GSlightSetType(void* light, s32 type);
        void GSlightSetColor(void* light, LightColor* rgb);
        void GSlightSetPosition(void* light, Vec* position);
        void GSlightSetTarget(void* light, Vec* target);
        void GSlightSetActive(void* light, u8 active);
        LightColor grey = {80.0f, 80.0f, 80.0f}, white = {255.0f, 255.0f, 255.0f};
        Vec sun_pos = {12.0f, 12.0f, 12.0f}, origin = {0.0f, 0.0f, 0.0f};
        void* ambient = GSlightCreate();
        void* sun = GSlightCreate();
        printf("[pc_gfx_test] GSlights %p %p\n", ambient, sun);
        if (ambient != NULL && sun != NULL) {
            GSlightSetType(ambient, 0); /* HSD light type 0: ambient */
            GSlightSetColor(ambient, &grey);
            GSlightSetActive(ambient, 1);
            GSlightSetType(sun, 1);     /* 1: infinite (directional) */
            GSlightSetColor(sun, &white);
            GSlightSetPosition(sun, &sun_pos);
            GSlightSetTarget(sun, &origin);
            GSlightSetActive(sun, 1);
        }
    }
}

/* --draw: one coloured triangle in screen space through GSgfx's request buffer, set up the way the debug menu's
 * GSgfx test item does it (dbgMenu.c fn_801330C8): recorded during the frame, played back at its end */
static void draw_triangle(void) {
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    gfxSetBlendMode(0, 6, 7);
    gfxSetUpdates(1, 1, 0);
    gfxSetZMode(0, 1, 1);
    gfxSetCullMode(0);
    gfxSetPrimitive(4);
    gfxSetVtxDesc(NULL);
    gfxBegin(3);
    gfxVtxPos(320.0f, 80.0f, 0.0f);
    fn_800D5CB8(0, 0xFF, 0x80, 0x40, 0xFF);
    gfxVtxPos(120.0f, 400.0f, 0.0f);
    fn_800D5CB8(0, 0x40, 0xFF, 0x00, 0xFF);
    gfxVtxPos(520.0f, 400.0f, 0.0f);
    fn_800D5CB8(0, 0x00, 0x40, 0xFF, 0xFF);
    gfxEnd();
}

/* --shadow (with --room and --model): the model casts a shadow on the room's models the way the game sets it up
 * (gbaCommunication: GSmodelSetShadowFlags(model, 2), GSmodelSetShadowLight, GSmodelSetShadowSurface with the
 * receiving models as 32-bit words; people.c passes the room's shadow-receive models). The frame start's
 * modelShadowRender then renders it. The test has no floor data, so every room model receives (up to 16). */
void GSmodelSetShadowLight(struct GSmodel* model, struct GSlight* light);
static void setup_shadow(void) {
    u32 surfaces[16];
    int i, n = room_model_count < 16 ? room_model_count : 16;
    for (i = 0; i < n; i++) {
        surfaces[i] = PC_PTR32(room_models[i]);
    }
    printf("[pc_gfx_test] shadow: %d receivers, light %p\n", n, (void*)room_sun);
    if (n == 0 || room_sun == NULL) {
        return;
    }
    if (getenv("PC_GFX_SHADOW_DARK") != NULL) {
        extern u8 lbl_8047AB94; /* shadow darkness (modelShadowInit: 0x80); the shadow's intensity is 0xFF minus it */
        lbl_8047AB94 = (u8)atoi(getenv("PC_GFX_SHADOW_DARK"));
    }
    GSmodelSetShadowFlags(gs_model, 2);
    GSmodelSetShadowLight(gs_model, room_sun);
    GSmodelSetShadowSurface(gs_model, n, surfaces);
}

/* PC_GFX_SHADOW_DUMP=<file>: the first active shadow slot's texture (the shadow pass's EFB copy, I4; 384x384 with the game's default size) read
 * back at frame 20. The shadow's leading fields as hsd/shadow.c declares them. */
typedef struct {
    void* objects;
    void* camera;
    HSD_TObj* texture;
} DbgShadow;
static AuroraReadbackStream shadow_stream;
static void shadow_debug(int frame) {
    static u8 pixels[1024 * 1024 / 2];
    static u32 size;
    static int done;
    int i;
    if (frame == 20) {
        GSbound* b = &((GSmodel*)gs_model)->bound;
        printf("[pc_gfx_test] model bound: flags %d empty %d size %g %g %g\n", b->flags, b->empty, b->size.x, b->size.y,
               b->size.z);
    }
    for (i = 0; i < 6 && frame == 20; i++) {
        DbgShadow* s = lbl_80401490[i].obj;
        void* image = s != NULL ? DP(s->texture->imagedesc->image_ptr) : NULL;
        if (lbl_80401490[i].model != NULL || lbl_80401490[i].flag) {
            printf("[pc_gfx_test] shadow slot %d: receiver %p flag %d image %p\n", i, (void*)lbl_80401490[i].model,
                   lbl_80401490[i].flag, image);
        }
        if (lbl_80401490[i].flag && image != NULL && getenv("PC_GFX_SHADOW_DUMP") != NULL && shadow_stream == 0) {
            AuroraReadbackCopySrc src = {3, image};
            size = GXGetTexBufferSize(s->texture->imagedesc->width, s->texture->imagedesc->height, GX_TF_I4, GX_FALSE, 0);
            shadow_stream = AuroraReadbackCreateStream();
            AuroraReadbackCaptureCopies(shadow_stream, &src, 1);
        }
    }
    if (!done && shadow_stream != 0) {
        AuroraReadbackDest dest = {3, pixels, size, GX_FALSE};
        if (AuroraReadbackGet(shadow_stream, &dest, 1) && dest.found) {
            FILE* f = fopen(getenv("PC_GFX_SHADOW_DUMP"), "wb");
            if (f != NULL) {
                fwrite(pixels, 1, size, f);
                fclose(f);
            }
            printf("[pc_gfx_test] shadow texture dumped\n");
            done = 1;
        }
    }
}

/* --col PATH [--at X Z]...: a room's collision file (.col) through the game's PC path, the way floorReadColPostFunc
 * loads it: a collision layer pushed (fn_8010D064), the file made current (fn_8010CFE4: offsets fixed, and on the PC
 * the whole file converted to native order once), then the floor query every walker uses (GScolsys2WalkGetHeight) at each
 * --at point. The heights can be compared with the extracted .obj (decoded by the Python tools, not the game's C). */
typedef struct {
    f32 height;
    u16 surface;
    u16 attr;
    u8 layer;
    u8 subLayer;
} ColWalkHit; /* as GScolsys2Walk_exact_8010E138.c declares it */
s32 fn_8010D064(void);
s32 fn_8010CFE4(void* fileHead);
s32 GScolsys2WalkGetHeight(GSvec* point, ColWalkHit* out);
s32 GScolsys2WalkGetLayer(GSvec* position, u8* layer, u8* subLayer);
static f32 col_at[256][2];
static int col_at_count;

static int run_collision(const char* path) {
    FILE* f = fopen(path, "rb");
    long n;
    u8* data;
    int i, k;
    if (f == NULL) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = fn_801A6928((u32)n);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return 0;
    }
    fclose(f);
    {
        /* in this order (as separate statements: the push clears the current file) */
        s32 pushed = fn_8010D064();
        s32 loaded = fn_8010CFE4(data);
        printf("[pc_gfx_test] collision: %ld bytes, push %d, load %d\n", n, (int)pushed, (int)loaded);
    }
    for (i = 0; i < col_at_count; i++) {
        ColWalkHit hits[8];
        GSvec point;
        u8 layer = 0xFF, subLayer = 0xFF;
        s32 count;
        point.x = col_at[i][0];
        point.y = 0.0f;
        point.z = col_at[i][1];
        count = GScolsys2WalkGetHeight(&point, hits);
        GScolsys2WalkGetLayer(&point, &layer, &subLayer);
        printf("[pc_gfx_test] floor at %.3f %.3f: %d hit(s):", point.x, point.z, (int)count);
        for (k = 0; k < count && k < 8; k++) {
            printf(" %.3f (surface %u attr %u layer %u/%u)", hits[k].height, hits[k].surface, hits[k].attr, hits[k].layer,
                   hits[k].subLayer);
        }
        printf("; layer %u/%u\n", layer, subLayer);
    }
    return 1;
}

/* --anim-all: switch the model to its next animation (and the texture animation of the same index, as the people
 * code does) every 4 frames, through the same setters a battle uses; prints each switch */
static void step_anim_all(int frame) {
    void GSmodelSetTexAnimIndex(void* model, u32 index);
    void GSmodelStartTexAnimation(void* model);
    GSmodel* m = gs_model;
    u32 index;
    if (m == NULL || frame % 4 != 0 || m->animCount == 0) {
        return;
    }
    index = (u32)(frame / 4) % m->animCount;
    GSmodelSetAnimIndex(gs_model, index);
    GSmodelSetAnimType(gs_model, 1);
    GSmodelStartAnimation(gs_model);
    if (index < m->texAnimCount) {
        GSmodelSetTexAnimIndex(gs_model, index);
        GSmodelStartTexAnimation(gs_model);
    }
    if (frame / 4 < (int)m->animCount) {
        printf("[pc_gfx_test] animation %u of %u (texture animations %u)\n", index, m->animCount, m->texAnimCount);
    }
}

/* --attach JOINT PATH: a second model attached to joint JOINT of the --model one, the way move sequences and NPCs do
 * it (GSmodelGetPart, GSmodelAttachToGSpart with selector 7 = follow the joint's full transform, GSpartFree) */
static void* attached_model;
static int attach_joint = -1;
static const char* attach_path;
/* --seq PKX: a Pokemon loaded the way a battle's move sequences load it (wazaViewer.c, fight code): the sequence
 * system set up for 8 sequences (fn_801DAEF8), a free sequence taken (fn_801DB154), the whole .pkx file (header, model,
 * particle bank) in a GSmem block, then sequenceLoad, which registers the parts as resources, loads the model and its
 * particle bank, and starts it. The model it made becomes the test's model. */
static void* seq_effect;
static int wzx_type = 1; /* --wzx-type: 1 attack, 2 damage (likely) */

/* The battle's current floor (floorDataBiosGetCurrentPtr): the test has no battle room, so an empty floor record (no
 * resource group, no model list); the move's room-tint effect then finds no room models. Overrides the generated stub. */
static u8 test_floor[0x40] __attribute__((aligned(32)));
void* floorDataBiosGetCurrentPtr(void) {
    return test_floor;
}

/* With --room, the room stands in for the battle's current floor: the floor record's +0x08 holds the room's resource
 * id, the floor's resource group is a test group, and the drawn room archive and its models are registered under the
 * ids the game's floor loader uses (floorReadMakeModelResID: (id & 0x7FFF0000) | 0x1000, plus the model's index in
 * the room's model list), so the moves' room effects (the tint, WZX effect type 0) find the room's real models. */
#define TEST_FLOOR_GROUP 0x7E57u
#define TEST_FLOOR_ROOM_ID 0x00010000u
u32 floorDataBiosGetGroupID(void* floor) {
    return floor == test_floor ? TEST_FLOOR_GROUP : 0;
}
u32 std093_getCurrentGroupID(void) { /* as floor.c: the current floor's group */
    return floorDataBiosGetGroupID(floorDataBiosGetCurrentPtr());
}
u32 floorReadMakeModelResID(u32 value) { /* as field_range_exact_80115250.c */
    return (value & 0x7FFF0000U) | 0x1000;
}
static void register_room_resources(void) {
    int i;
    *(u32*)(test_floor + 0x08) = __builtin_bswap32(TEST_FLOOR_ROOM_ID); /* big-endian, as the real floor records */
    GSresRegisterResource(&room_archive, TEST_FLOOR_GROUP, TEST_FLOOR_ROOM_ID, NULL);
    for (i = 0; i < room_model_count; i++) {
        GSresRegisterResource(room_models[i], TEST_FLOOR_GROUP, floorReadMakeModelResID(TEST_FLOOR_ROOM_ID) | (u32)i, NULL);
    }
    printf("[pc_gfx_test] room registered as the battle floor: %d models\n", room_model_count);
}

/* The resource registry, once (main.c sets it up with 300 entries); the camera controller, the floor loader and the
 * sequence loader all register into it. */
static void test_gsres_init(void) {
    void GSresInit(u32 count);
    static int done;
    if (!done) {
        done = 1;
        GSresInit(0x12C);
    }
}

/* A whole file in a GSmem block (32-byte aligned), as the game's loaders hand files to the sequence code. */
static u8* read_gsmem_file(const char* path) {
    u16 fn_800E2C04(u32 size, u16 alignment);
    FILE* f = fopen(path, "rb");
    long n;
    u16 block;
    u8* buf;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    block = fn_800E2C04((u32)((n + 0x1F) & ~0x1F), 0x20);
    buf = block != 0 ? fn_800E27B0(block) : NULL;
    if (buf != NULL) {
        fread(buf, 1, (size_t)n, f);
    }
    fclose(f);
    return buf;
}

/* One Pokemon record: a free sequence record, its PKX through sequenceLoad, shown (sequenceLoad leaves it hidden). */
static void* load_sequence_record(const char* path) {
    void* fn_801DB154(void);
    void* rec = fn_801DB154();
    u8* buf = read_gsmem_file(path);
    BOOL ok;
    void* model;
    if (rec == NULL || buf == NULL) {
        printf("[pc_gfx_test] no sequence record (%p) or file (%p) for %s\n", rec, buf, path);
        return NULL;
    }
    ok = sequenceLoad(rec, buf);
    model = PTR32_AT_T(void, (u8*)rec, 0x24);
    printf("[pc_gfx_test] sequenceLoad %d: model %p, particle bank %p\n", ok, model, PTR32_AT_T(void, (u8*)rec, 0x28));
    if (!ok || model == NULL) {
        return NULL;
    }
    pc_hsd_scan_arrays(((GSmodel*)model)->renderHsdJObj);
    fn_801DA4E8(rec, 1); /* shown, as wazaViewer.c does after sequenceLoad */
    return rec;
}

static int load_sequence(const char* path) {
    void fn_801DAEF8(s32 count);
    void GSresInit(u32 count);
    void fn_80130CE0(u16 maxEffects);
    init_pools();
    test_gsres_init();
    fn_80130CE0(0x20); /* GSEffectInit, as main.c: 32 effect slots (its GSgapp tasks are run by hand below) */
    fn_801DAEF8(8);
    printf("[pc_gfx_test] sequence system set up\n");
    seq_effect = load_sequence_record(path);
    gs_model = seq_effect != NULL ? PTR32_AT_T(void, (u8*)seq_effect, 0x24) : NULL;
    return gs_model != NULL;
}

/* --wzx FILE (with --seq): a move's effect sequence attached to a Pokemon and started the way wazaViewer.c does it:
 * fn_801DDB4C (parse and attach; keys 0 / 0), then the keys set to (0, type) as the battle's fn_801DDD28 keys a move
 * (move, type: 1 attack, 2 damage; likely) and fn_801DA9E8 starts it by key; every frame fn_801DB088 runs every
 * sequence's step. */
static int attach_wzx(void* rec, const char* path, int type) {
    BOOL fn_801DDB4C(void* owner, void* resource);
    u8* buf;
    BOOL ok;
    if (rec == NULL || (buf = read_gsmem_file(path)) == NULL) {
        return 0;
    }
    ok = fn_801DDB4C(rec, buf);
    printf("[pc_gfx_test] WZX attached: %d (type %d)\n", ok, type);
    if (ok) {
        u8* waza = PTR32_AT_T(u8, (u8*)rec, 0x68); /* the record's attached-move list head */
        *(u16*)(waza + 0x2E) = (u16)type;
    }
    return ok;
}

static int start_wzx(const char* path) {
    if (!attach_wzx(seq_effect, path, wzx_type)) {
        return 0;
    }
    fn_801DA9E8(seq_effect, 0, wzx_type);
    printf("[pc_gfx_test] WZX started\n");
    if (getenv("PC_GFX_WAZA_TIMES") != NULL) {
        /* the game's time points for this move on this Pokemon (wazaSequenceSysGetWazaTime, time types 0-7) */
        s32 wazaSequenceSysGetWazaTime(void* owner, void* sequence, s32 timeType);
        void* waza = PTR32_AT_T(void, (u8*)seq_effect, 0x68);
        int t;
        for (t = 0; t < 8; t++) {
            printf("[pc_gfx_test] waza time %d: %d\n", t, (int)wazaSequenceSysGetWazaTime(seq_effect, waza, t));
        }
    }
    return 1;
}

/* --seq2 PKX [--wzx2 FILE]: a second Pokemon, the move's target. Both stand where the battle grid puts a single
 * battle's Pokemon (battle_grid.c, its constants read from the DOL): side 0 at x = +40 * factor turned -90 degrees,
 * side 1 at x = -40 * factor turned +90 degrees; the factor is the largest Pokemon's size class (1.0 normal; 0.875,
 * 1.4, 1.8, 2.75 for the other classes), --seq-dist here. The target's WZX is keyed as type 2 (damage) and started in
 * the same frame as the attacker's. */
static void* seq_target;
static f32 seq_dist = 1.0f;
static int battle_grid;     /* --battle-grid: place both through the game's battle grid (battle_grid.c) */
static const char* trainer_path[2]; /* --trainer0 / --trainer1 PKX: trainer battle models in the grid's trainer slots */
static int wzx2_delay;      /* --wzx2-delay N: the target's half starts N frames after the attacker's */
static int wzx2_pending;    /* attached, not started yet */

static void setup_target(const char* pkx, const char* wzx) {
    Vec pos, rot;
    seq_target = load_sequence_record(pkx);
    if (seq_target == NULL || seq_effect == NULL) {
        return;
    }
    if (battle_grid) {
        /* As the battle does: the grid is cleared (fn_801C3114), each side's trainer slot is added (empty sequence records
         * here: the test has no trainer models), each Pokemon under its trainer, then battleGridUpdate places every model
         * (size factor, positions, facing). */
        void fn_801C3114(void);
        void battleGridAddTrainer(u8* slot, u8 side, u8 depth);
        void battleGridAddPokemon(u8* slot, u8* pokemon);
        void battleGridUpdate(void);
        void* fn_801DB154(void);
        /* trainer battle models are PKX files like the Pokemon's (extracted/trainers); without one, an empty record */
        u8* trainer0 = trainer_path[0] != NULL ? load_sequence_record(trainer_path[0]) : fn_801DB154();
        u8* trainer1 = trainer_path[1] != NULL ? load_sequence_record(trainer_path[1]) : fn_801DB154();
        Vec p0, p1, r0, r1;
        fn_801C3114();
        battleGridAddTrainer(trainer0, 0, 0);
        battleGridAddTrainer(trainer1, 1, 0);
        battleGridAddPokemon(trainer0, seq_effect);
        battleGridAddPokemon(trainer1, seq_target);
        battleGridUpdate();
        GSmodelGetPosition(PTR32_AT_T(void, (u8*)seq_effect, 0x24), (GSvec*)&p0);
        GSmodelGetPosition(PTR32_AT_T(void, (u8*)seq_target, 0x24), (GSvec*)&p1);
        GSmodelGetRotation(PTR32_AT_T(void, (u8*)seq_effect, 0x24), (GSvec*)&r0);
        GSmodelGetRotation(PTR32_AT_T(void, (u8*)seq_target, 0x24), (GSvec*)&r1);
        printf("[pc_gfx_test] battle grid: side 0 at %g %g %g (rot y %g), side 1 at %g %g %g (rot y %g)\n", p0.x, p0.y, p0.z,
               r0.y, p1.x, p1.y, p1.z, r1.y);
        {
            /* the grid's other queries the battle and the moves use (each walks the grid's Pokemon slots) */
            f32 battleGridGetDistance(u8 side);
            void battleGridGetNormalisedScale(f32* scale);
            f32 scale[3];
            battleGridGetNormalisedScale(scale);
            printf("[pc_gfx_test] battle grid distances %g / %g, normalised scale %g\n", battleGridGetDistance(0),
                   battleGridGetDistance(1), scale[0]);
        }
    } else {
    pos.x = 40.0f * seq_dist; pos.y = 0.0f; pos.z = 0.0f;
    rot.x = 0.0f; rot.y = -1.5707964f; rot.z = 0.0f;
    GSmodelSetPosition(PTR32_AT_T(void, (u8*)seq_effect, 0x24), (GSvec*)&pos);
    GSmodelSetRotation(PTR32_AT_T(void, (u8*)seq_effect, 0x24), (GSvec*)&rot);
    pos.x = -40.0f * seq_dist;
    rot.y = 1.5707964f;
    GSmodelSetPosition(PTR32_AT_T(void, (u8*)seq_target, 0x24), (GSvec*)&pos);
    GSmodelSetRotation(PTR32_AT_T(void, (u8*)seq_target, 0x24), (GSvec*)&rot);
    }
    if (wzx != NULL && attach_wzx(seq_target, wzx, 2)) {
        if (wzx2_delay != 0) {
            wzx2_pending = 1;
        } else {
            fn_801DA9E8(seq_target, 0, 2);
            printf("[pc_gfx_test] target WZX started\n");
        }
    }
}

/* --room-game ROOM.dat: the room loaded the way the game's floor loader does it (gs_floor.c): the file in a GSmem
 * block, fn_8010147C registers it as an archive resource (the HSD_Archive in a resource block), then fn_801013A0 loads
 * each model of its list into its own resource; every model made visible. */
static int load_room_game(const char* path) {
    void fn_8010147C(u8* data, u32 size, u32 group, u32 id);
    void fn_801013A0(void* archive, u32 group, u32 modelIndex, u32 id);
    void GSresInit(u32 count);
    FILE* f = fopen(path, "rb");
    long n;
    u8* buf;
    void* archive;
    void** scene;
    u32 i, models = 0;
    if (f == NULL) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);
    init_pools();
    test_gsres_init();
    buf = read_gsmem_file(path);
    if (buf == NULL) {
        return 0;
    }
    fn_8010147C(buf, (u32)n, TEST_FLOOR_GROUP, TEST_FLOOR_ROOM_ID);
    archive = GSresGetResource(TEST_FLOOR_GROUP, TEST_FLOOR_ROOM_ID);
    if (archive == NULL) {
        return 0;
    }
    scene = HSD_ArchiveGetPublicAddress(archive, "scene_data");
    for (i = 0; scene != NULL && DP(((DISC_PTR(void)*)scene)[0]) != NULL &&
                DP(((DISC_PTR(void)*)DP(((DISC_PTR(void)*)scene)[0]))[i]) != NULL; i++) {
        void* model;
        u32 id = floorReadMakeModelResID(TEST_FLOOR_ROOM_ID) | i;
        fn_801013A0(archive, TEST_FLOOR_GROUP, i, id);
        model = GSresGetResource(TEST_FLOOR_GROUP, id);
        if (model != NULL) {
            pc_hsd_scan_arrays(((GSmodel*)model)->renderHsdJObj);
            GSmodelSetVisibility(model, 1);
            models++;
        }
    }
    printf("[pc_gfx_test] room through the game's loader: %u models\n", models);
    return models > 0;
}

static void setup_attach(void) {
    static HSD_Archive archive;
    void GSmodelAttachToGSpart(void* model, void* part, s32 type, s32 once, u8 reset);
    void* set = open_model_set(attach_path, &archive);
    struct GSpart* part; /* GSmodelGetPart / GSpartFree: canonical prototypes */
    if (set == NULL || (attached_model = GSmodelLoad(set, NULL, NULL)) == NULL) {
        printf("[pc_gfx_test] attach: model NOT loaded\n");
        return;
    }
    pc_hsd_scan_arrays(((GSmodel*)attached_model)->renderHsdJObj);
    GSmodelSetVisibility(attached_model, 1);
    part = GSmodelGetPart(gs_model, attach_joint);
    printf("[pc_gfx_test] attach: part %p (joint %d)\n", part, attach_joint);
    if (part != NULL) {
        GSmodelAttachToGSpart(attached_model, part, 7, 0, 1);
        GSpartFree(part);
    }
}

/* layout checks for the PC views of HSD's particle generator (include/game/world/field_particle.h) */
#include "sysdolphin/baselib/psstructs.h"
#include "game/world/field_particle.h"
#include <stddef.h>
_Static_assert(offsetof(FieldParticleGenerator, position) == offsetof(HSD_Generator, pos), "generator pos");
_Static_assert(offsetof(FieldParticleGenerator, family_id) == offsetof(HSD_Generator, idnum), "generator idnum");
_Static_assert(offsetof(FieldParticleGenerator, enabled) == offsetof(HSD_Generator, x17), "generator x17");
_Static_assert(offsetof(FieldParticleGenerator, field_10) == offsetof(HSD_Generator, genLife), "generator genLife");
_Static_assert(offsetof(FieldParticleGenerator, rotation) == offsetof(HSD_Generator, rot), "generator rot");
_Static_assert(offsetof(FieldParticleGenerator, scale) == offsetof(HSD_Generator, scale), "generator scale");
_Static_assert(offsetof(FieldParticleInstanceList, entries) == offsetof(FieldParticleBank, slots), "bank slots");
_Static_assert(FIELD_PS_APPSRT_SIZE == sizeof(HSD_psAppSRT), "appsrt size");
#include "game/gs_model_offsets.h"
_Static_assert(GSMODEL_OFFSET_RENDER_JOBJ == offsetof(GSmodel, renderJObj), "GSmodel render joint offset");
_Static_assert(offsetof(HSD_psAppSRT, x72) == 0x82 && offsetof(HSD_psAppSRT, usedCount) == 0x3A, "appsrt fields (generator.c PC view)");

/* --particles PATH (with --model): a GPT1 particle bank (as a PKX carries it after the model) loaded and linked the
 * way sequenceLoad does it (gsParticleBankLoad, GSmodelLinkToGSparticleBank); the model's joint particle lists and
 * its animations' particle keys then spawn the emitters through gsParticlePtclHook. gsParticleInit runs in
 * init_pools, as in the game's start-up. */
void* gsParticleBankLoad(void* resource);
void GSmodelLinkToGSparticleBank(GSmodel* model, void* bank);
static const char* particles_path;
static int emit_all; /* --emit-all: one generator for every command list of the bank, as a move sequence would */
static void load_particles(void) {
    FILE* f = fopen(particles_path, "rb");
    long n;
    u8* data;
    void* bank;
    if (f == NULL) {
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = fn_801A6928((u32)n);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return;
    }
    fclose(f);
    {
        /* the first command id and the count, read before the load converts the bank (big-endian, description +4) */
        u32 desc = __builtin_bswap32(((u32*)data)[1]);
        u32 first = __builtin_bswap32(*(u32*)(data + desc + 4));
        u32 count = __builtin_bswap32(*(u32*)(data + desc + 8));
        bank = gsParticleBankLoad(data);
        printf("[pc_gfx_test] particle bank %p (commands %u..%u)\n", bank, first, first + count - 1);
        if (bank != NULL && gs_model != NULL) {
            GSmodelLinkToGSparticleBank(gs_model, bank);
        }
        if (bank != NULL && emit_all) {
            u8* gsParticleCreate(u8* bank, u32 selector, u32 subid);
            u32 id, made = 0;
            for (id = first; id < first + count; id++) {
                made += gsParticleCreate(bank, id, 0) != NULL;
            }
            printf("[pc_gfx_test] %u generators\n", made);
        }
    }
}

/* --rtt (with --model): the model drawn into a texture the way the TV screen does it (tvRender,
 * field_camera_exact_80117AE4.c): its own frame with the texture as the target, drawn at once instead of recorded,
 * through a camera of its own; the frame end copies the EFB into the texture (GStextureConvertFromHW). The main frame
 * then shows the texture on a quad, the way the lens flare draws its textured quads (battle_range_candidate_801ED780.c).
 * On the PC the copy stays on the GPU (Aurora keys it by the GXCopyTex destination); its bytes come back to CPU memory
 * only through Aurora's readback, which this test uses to count the drawn pixels. */
#define RTT_SIZE 128
void fn_800D4604(u32 val);    /* GSgfx record mode: 1 record, 2 draw at once */
void fn_800D377C(s32 mode);   /* HSD mode switch around a texture frame */
void fn_800D9AF0(u16* x1, u16* y1, u16* x2, u16* y2); /* viewport (read) */
void fn_800D9B24(u16* x1, u16* y1, u16* x2, u16* y2); /* scissor (read) */
void fn_800D9C24(u32 x1, u32 y1, u32 x2, u32 y2);     /* viewport (set) */
void fn_800D9D68(u16 x1, u16 y1, u16 x2, u16 y2);     /* scissor (set) */
void gfxLoadTexture(s32 slot, void* texture);
void gfxVtxTexCoord(u32 idx, f32 s, f32 t);
void gfxVtxColor(u32 idx, u8 r, u8 g, u8 b); /* 3-byte colour: lbl_80314C78 declares colour 0 RGB8 */
extern GSVtxDesc lbl_80314C78; /* position, colour 0, texture coordinate 0 (data_80314350.c) */
static int rtt;
static int envmap_on; /* PC_GFX_ENVMAP: the model stays on screen, no quad */
static int shadow;
static GStextureHandle* rtt_texture;
static void* rtt_camera;
static AuroraReadbackStream rtt_stream;

static void setup_rtt(void) {
    Vec eye = {0.0f, 1.5f, 12.0f}, up = {0.0f, 1.0f, 0.0f}, at = {0.0f, 1.5f, 0.0f};
    /* 0x44: copied as GX format 4 (RGB565), as the TV; PC_GFX_RTT_FMT=0x45 copies RGBA8 like the name-entry menu model */
    rtt_texture = GStextureCreate(RTT_SIZE, RTT_SIZE,
                                  getenv("PC_GFX_RTT_FMT") ? (u32)strtoul(getenv("PC_GFX_RTT_FMT"), NULL, 16) : 0x44, 0, 0);
    rtt_camera = fn_800D29A0();
    printf("[pc_gfx_test] rtt texture %p camera %p\n", (void*)rtt_texture, rtt_camera);
    if (rtt_texture == NULL || rtt_camera == NULL) {
        rtt = 0;
        return;
    }
    GScameraSetPosition(rtt_camera, &eye);
    GScameraLookAt(rtt_camera, &up, &at);
    GScameraSetPerspective(rtt_camera, 30.0f, 1.0f, 1.0f, 120.0f);
    GSmodelSetVisibility(gs_model, 0); /* as the TV's model: drawn only into the texture */
    rtt_stream = AuroraReadbackCreateStream();
}

static void render_rtt(void) {
    u16 vx, vy, vw, vh, sx, sy, sw, sh;
    void* camera = GScameraGetActiveCamera();
    fn_800D4604(2);
    /* PC_GFX_RTT_MODE=2: HSD's pixel format RGBA6_Z24 for the offscreen pass, as menuModelRender (name entry) sets it;
     * the field TV uses 1 (RGB8_Z24) */
    fn_800D377C(getenv("PC_GFX_RTT_MODE") ? atoi(getenv("PC_GFX_RTT_MODE")) : 1);
    fn_800D3410(rtt_texture, 0);
    fn_800D9B24(&vx, &vy, &vw, &vh);
    fn_800D9AF0(&sx, &sy, &sw, &sh);
    fn_800D258C(rtt_camera);
    fn_800D9D68(0, 0, RTT_SIZE - 1, RTT_SIZE - 1);
    fn_800D9C24(0, 0, RTT_SIZE - 1, RTT_SIZE - 1);
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
    GSmodelDrawModel(gs_model, 0x3010);
    fn_800D3190();
    fn_800D377C(1);
    fn_800D258C(camera);
    fn_800D9D68(vx, vy, vw, vh);
    fn_800D9C24(sx, sy, sw, sh);
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
    fn_800D4604(1);
}

/* reads the previous copy back (a few frames late) and asks for this frame's */
static void check_rtt(int frame) {
    static int reported;
    static u8 pixels[RTT_SIZE * RTT_SIZE * 2];
    AuroraReadbackDest dest = {1, pixels, (u32)GXGetTexBufferSize(RTT_SIZE, RTT_SIZE, GX_TF_RGB565, GX_FALSE, 0),
                               GX_FALSE};
    AuroraReadbackCopySrc src = {1, P32(rtt_texture->mipData[0])};
    if (!reported && AuroraReadbackGet(rtt_stream, &dest, 1) && dest.found) {
        int i, lit = 0;
        for (i = 0; i < RTT_SIZE * RTT_SIZE; i++) {
            if (pixels[i * 2] != 0 || pixels[i * 2 + 1] != 0) {
                lit++;
            }
        }
        printf("[pc_gfx_test] rtt readback at frame %d: %d of %d pixels not black\n", frame, lit,
               RTT_SIZE * RTT_SIZE);
        reported = 1;
        if (getenv("PC_GFX_RTT_DUMP") != NULL) {
            /* the copy's bytes as the GameCube would have them: RGB565 in 4x4 tiles, big-endian */
            FILE* f = fopen(getenv("PC_GFX_RTT_DUMP"), "wb");
            if (f != NULL) {
                fwrite(pixels, 1, dest.size, f);
                fclose(f);
            }
        }
    }
    if (!reported && !AuroraReadbackCaptureCopies(rtt_stream, &src, 1) && frame == 0) {
        printf("[pc_gfx_test] rtt readback not available\n");
    }
}

/* the texture on a quad in the middle of the screen */
static void draw_rtt_quad(void) {
    f32 x0 = 320.0f - 128.0f, y0 = 240.0f - 128.0f, x1 = 320.0f + 128.0f, y1 = 240.0f + 128.0f;
    if (getenv("PC_GFX_RTT_RECT") != NULL && atoi(getenv("PC_GFX_RTT_RECT")) == 2) {
        /* PC_GFX_RTT_RECT=2: exactly menuNameEntryBackDrawHumanModel's calls, no blend / z / update setup of our own:
         * the quad inherits whatever state the model drawing left */
        extern u8 lbl_80314F98[];
        extern void spriteSetEnv(void);
        fn_800FE6D0(0, 0); /* winSpriteDraw's setup before a sprite's draw callback (window offset, sprite env) */
        spriteSetEnv();
        gfxClearFlags(4);
        gfxSetFlags(3);
        gfxSetVtxDesc(lbl_80314F98);
        gfxLoadTexture(0, rtt_texture);
        gfxSetPrimitive(7);
        gfxBegin(2);
        fn_800D61E4((s32)x0, (s32)y0);
        fn_800D5CB8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxTexCoord(0, 0.0f, 0.0f);
        fn_800D61E4((s32)x1, (s32)y1);
        fn_800D5CB8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxTexCoord(0, 1.0f, 1.0f);
        gfxEnd();
        return;
    }
    gfxSetFlags(3); /* 2: textured (as effect_visual.c's fn_8013E258) */
    gfxClearFlags(4);
    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    /* PC_GFX_RTT_BLEND=1: blend by the texture's alpha, as the name-entry window draws its model picture (window
     * sprites set gfxSetBlendMode(1, 6, 7)) */
    gfxSetBlendMode(getenv("PC_GFX_RTT_BLEND") ? 1 : 0, 6, 7);
    gfxSetUpdates(1, 1, 0);
    gfxSetZMode(0, 1, 1);
    gfxSetCullMode(0);
    if (getenv("PC_GFX_RTT_RECT") != NULL) {
        /* PC_GFX_RTT_RECT=1: the name-entry window's way (menuNameEntryBackDrawHumanModel): primitive 7, two corners,
         * position through fn_800D61E4 and colour through fn_800D5CB8, the menus' vertex description lbl_80314F98 */
        extern u8 lbl_80314F98[];
        gfxSetPrimitive(7);
        gfxSetVtxDesc(lbl_80314F98);
        gfxLoadTexture(0, rtt_texture);
        gfxBegin(2);
        fn_800D61E4((s32)x0, (s32)y0);
        fn_800D5CB8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxTexCoord(0, 0.0f, 0.0f);
        fn_800D61E4((s32)x1, (s32)y1);
        fn_800D5CB8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxTexCoord(0, 1.0f, 1.0f);
        gfxEnd();
        return;
    }
    gfxSetPrimitive(4);
    gfxSetVtxDesc(&lbl_80314C78);
    gfxLoadTexture(0, rtt_texture);
    gfxBegin(4);
    gfxVtxPos(x0, y0, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 0.0f, 0.0f);
    gfxVtxPos(x1, y0, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 1.0f, 0.0f);
    gfxVtxPos(x0, y1, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 0.0f, 1.0f);
    gfxVtxPos(x1, y1, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 1.0f, 1.0f);
    gfxEnd();
}

/* PC_GFX_SCREEN_DUMP=<file> (PC_GFX_SCREEN_FRAME=N, default 20): the finished frame N copied out of the EFB
 * (RGB565, 4x4 tiles, as GXCopyTex would write it) and read back through Aurora, so a run can be checked without a
 * display (window captures fail when the display is off). */
static AuroraReadbackStream screen_stream;
static u8 screen_key[16]; /* the copy's destination: Aurora keys the copy by this address */
static int screen_frame = -1;
static void screen_copy(int frame) {
    AuroraReadbackCopySrc src = {2, screen_key};
    if (frame != screen_frame) {
        return;
    }
    GXSetTexCopySrc(0, 0, 640, 480);
    GXSetTexCopyDst(640, 480, GX_TF_RGB565, GX_FALSE);
    GXCopyTex(screen_key, GX_FALSE);
    if (screen_stream == 0) {
        screen_stream = AuroraReadbackCreateStream();
    }
    AuroraReadbackCaptureCopies(screen_stream, &src, 1);
}
static void screen_collect(void) {
    static u8 pixels[640 * 480 * 2];
    static int done;
    AuroraReadbackDest dest = {2, pixels, sizeof(pixels), GX_FALSE};
    if (done || screen_stream == 0 || !AuroraReadbackGet(screen_stream, &dest, 1) || !dest.found) {
        return;
    }
    {
        FILE* f = fopen(getenv("PC_GFX_SCREEN_DUMP"), "wb");
        if (f != NULL) {
            fwrite(pixels, 1, sizeof(pixels), f);
            fclose(f);
        }
    }
    printf("[pc_gfx_test] screen of frame %d dumped\n", screen_frame);
    done = 1;
}

/* --dl: the same triangle captured once into a GSgfx display list (GSgfxDLBegin / End: GSgfx writes the GX command
 * bytes itself) and drawn from it every frame (GSgfxDLDraw), as the collision debug view draws its static objects. */
u32 GSgfxDLBegin(GSVtxDesc* desc, u32 size);
void* GSgfxDLEnd(void);
void GSgfxDLDraw(void* capture);
extern GSVtxDesc lbl_80314638; /* position (f32) and colour 0 (RGBA8) */
static void* dl_capture;
static int dl_mode;

static void capture_triangle(void) {
    if (!GSgfxDLBegin(&lbl_80314638, 0x400)) {
        printf("[pc_gfx_test] display list: begin failed\n");
        return;
    }
    gfxSetPrimitive(4);
    gfxBegin(3);
    gfxVtxPos(320.0f, 80.0f, 0.0f);
    fn_800D5CB8(0, 0xFF, 0x80, 0x40, 0xFF);
    gfxVtxPos(120.0f, 400.0f, 0.0f);
    fn_800D5CB8(0, 0x40, 0xFF, 0x00, 0xFF);
    gfxVtxPos(520.0f, 400.0f, 0.0f);
    fn_800D5CB8(0, 0x00, 0x40, 0xFF, 0xFF);
    gfxEnd();
    dl_capture = GSgfxDLEnd();
    printf("[pc_gfx_test] display list: %s\n", dl_capture != NULL ? "captured" : "NOT captured");
}

static void draw_dl_triangle(void) {
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    gfxSetBlendMode(0, 6, 7);
    gfxSetUpdates(1, 1, 0);
    gfxSetZMode(0, 1, 1);
    gfxSetCullMode(0);
    GSgfxDLDraw(dl_capture);
}

/* --backfb (with --model): the back-framebuffer capture the fades use (GSgfxBeginBackFBCapture: at the frame's end
 * GSgfxBackFBDoFrame copies the EFB into the texture and calls the callback, which ends it by returning 0, as fade.c's
 * myBackFB does). From then on the model is hidden and the captured frame is drawn as a half-size quad. */
u32 GSgfxBeginBackFBCapture(void* texture, void* callback, void* userData);
static int backfb;
static int backfb_done;
static GStextureHandle* backfb_texture;
static u32 backfb_callback(void* texture, u32 frame, void* userData) {
    (void)texture;
    (void)userData;
    printf("[pc_gfx_test] back framebuffer captured (callback frame %u)\n", frame);
    if (getenv("PC_GFX_BACKFB_STACK") != NULL) {
        extern unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long skip, unsigned long count, void** frames,
                                                                 unsigned long* hash);
        void* frames[6];
        unsigned short k, n = RtlCaptureStackBackTrace(0, 6, frames, NULL);
        for (k = 0; k < n; k++) {
            printf("[stack] %p\n", frames[k]);
        }
    }
    backfb_done = 1;
    return 0;
}
static void draw_backfb_quad(void) {
    f32 x0 = 160.0f, y0 = 120.0f, x1 = 480.0f, y1 = 360.0f;
    gfxSetFlags(3); /* 2: textured (as effect_visual.c's fn_8013E258) */
    gfxClearFlags(4);
    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    gfxSetBlendMode(0, 6, 7);
    gfxSetUpdates(1, 1, 0);
    gfxSetZMode(0, 1, 1);
    gfxSetCullMode(0);
    gfxSetPrimitive(4);
    gfxSetVtxDesc(&lbl_80314C78);
    gfxLoadTexture(0, backfb_texture);
    gfxBegin(4);
    gfxVtxPos(x0, y0, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 0.0f, 0.0f);
    gfxVtxPos(x1, y0, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 1.0f, 0.0f);
    gfxVtxPos(x0, y1, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 0.0f, 1.0f);
    gfxVtxPos(x1, y1, 0.0f);
    gfxVtxColor(0, 0xFF, 0xFF, 0xFF);
    gfxVtxTexCoord(0, 1.0f, 1.0f);
    gfxEnd();
}

static void log_callback(AuroraLogLevel level, const char* module, const char* message, unsigned int len) {
    (void)len;
    if (level >= LOG_WARNING) {
        fprintf(stderr, "[%s] %s\n", module, message);
    }
    if (level >= LOG_FATAL) {
        fflush(stderr);
        *(volatile int*)0 = 0; /* let the crash handler print the call stack */
    }
}

int main(int argc, char* argv[]) {
    int i, frames = 60, frame, draw = 0;
    const char* model_path = NULL;
    const char* room_path = NULL;
    const char* col_path = NULL;
    const char* seq_path = NULL;
    const char* wzx_path = NULL;
    const char* seq2_path = NULL;
    const char* room_game_path = NULL;
    int battle_cam = 0;
    const char* wzx2_path = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    pc_crash_install();
    if (!pc_mem_reserve(PC_MEM1_RETAIL_SIZE) || !pc_mem_map_locked_cache() || !pc_lowmem_init()) {
        fprintf(stderr, "[pc_gfx_test] cannot map GameCube memory below 4 GB\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--draw") == 0) {
            draw = 1;
        } else if (strcmp(argv[i], "--dl") == 0) {
            dl_mode = 1;
        } else if (strcmp(argv[i], "--col") == 0 && i + 1 < argc) {
            col_path = argv[++i];
        } else if (strcmp(argv[i], "--at") == 0 && i + 2 < argc && col_at_count < 256) {
            col_at[col_at_count][0] = (f32)atof(argv[++i]);
            col_at[col_at_count][1] = (f32)atof(argv[++i]);
            col_at_count++;
        } else if (strcmp(argv[i], "--blend") == 0 && i + 3 < argc) {
            blend_a = atoi(argv[++i]);
            blend_b = atoi(argv[++i]);
            blend_f = (f32)atof(argv[++i]);
        } else if (strcmp(argv[i], "--attach") == 0 && i + 2 < argc) {
            attach_joint = atoi(argv[++i]);
            attach_path = argv[++i];
        } else if (strcmp(argv[i], "--backfb") == 0) {
            backfb = 1;
        } else if (strcmp(argv[i], "--emit-all") == 0) {
            emit_all = 1;
        } else if (strcmp(argv[i], "--particles") == 0 && i + 1 < argc) {
            particles_path = argv[++i];
        } else if (strcmp(argv[i], "--anim-all") == 0) {
            gs_anim_all = 1;
        } else if (strcmp(argv[i], "--rtt") == 0) {
            rtt = 1;
        } else if (strcmp(argv[i], "--shadow") == 0) {
            shadow = 1;
        } else if (strcmp(argv[i], "--texanim") == 0 && i + 1 < argc) {
            gs_texanim = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--anim") == 0 && i + 1 < argc) {
            gs_anim = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--room") == 0 && i + 1 < argc) {
            room_path = argv[++i];
        } else if (strcmp(argv[i], "--battle-cam") == 0) {
            battle_cam = 1;
        } else if (strcmp(argv[i], "--room-game") == 0 && i + 1 < argc) {
            room_game_path = argv[++i];
        } else if (strcmp(argv[i], "--seq2") == 0 && i + 1 < argc) {
            seq2_path = argv[++i];
        } else if (strcmp(argv[i], "--wzx2") == 0 && i + 1 < argc) {
            wzx2_path = argv[++i];
        } else if (strcmp(argv[i], "--wzx2-after") == 0) {
            wzx2_delay = -1; /* start the target's half when the attacker's ends (fightWazaDoEffect's wait, as the
                              * battle's waiting move type functions do, e.g. fn_80210B08) */
        } else if (strcmp(argv[i], "--wzx2-delay") == 0 && i + 1 < argc) {
            wzx2_delay = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--trainer0") == 0 && i + 1 < argc) {
            trainer_path[0] = argv[++i];
        } else if (strcmp(argv[i], "--trainer1") == 0 && i + 1 < argc) {
            trainer_path[1] = argv[++i];
        } else if (strcmp(argv[i], "--battle-grid") == 0) {
            battle_grid = 1;
        } else if (strcmp(argv[i], "--seq-dist") == 0 && i + 1 < argc) {
            seq_dist = (f32)atof(argv[++i]);
        } else if (strcmp(argv[i], "--wzx-type") == 0 && i + 1 < argc) {
            wzx_type = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--wzx") == 0 && i + 1 < argc) {
            wzx_path = argv[++i];
        } else if (strcmp(argv[i], "--seq") == 0 && i + 1 < argc) {
            seq_path = argv[++i];
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_path = argv[++i];
        }
    }

    const AuroraConfig config = {
        .appName = "pc_gfx_test",
        .logCallback = &log_callback,
        .mem1Size = PC_MEM1_RETAIL_SIZE,
        .mem1Base = pc_mem_from_gc(PC_MEM1_BASE),
    };
    aurora_initialize(argc, argv, &config);
    OSInit();
    pc_data_relocate();
    pc_hsd_data_init();
    {
        /* the game's main (main.c): GSmem gets the first 0xE80000 bytes of the arena, the OS heap the rest, then the
         * scratch slots */
        void* arenaLo = OSGetArenaLo();
        void* heapStart = (u8*)arenaLo + 0x00E80000;
        GSmemInit(0, arenaLo, heapStart);
        OSSetArenaLo(OSInitAlloc(heapStart, OSGetArenaHi(), 1));
        GSscratchInit(0);
    }
    printf("[pc_gfx_test] start-up done\n");

    /* the game's call (main.c fn_800057B0): request buffer 0x6DDD0 bytes, matrix stack 16, 8 vertex descriptors,
     * 32 display-list captures, video mode 1 (NTSC), EFB height 480. It runs HSD_InitComponent itself. */
    {
        extern int pc_hsd_trace; /* gx_trace.c: PC_GFX_TRACE=1 prints the GX set-up calls */
        pc_hsd_trace = getenv("PC_GFX_TRACE") ? 100000 : 0;
    }
    if (getenv("PC_GFX_SCREEN_DUMP") != NULL) {
        screen_frame = getenv("PC_GFX_SCREEN_FRAME") ? atoi(getenv("PC_GFX_SCREEN_FRAME")) : 20;
    }
    GSgfxInit__FP15_GSgfxInitParms(0x6DDD0, 0x10, 0x8, 0x20, 0x1, 0x1E0);
    printf("[pc_gfx_test] GSgfxInit done\n");
    init_pools(); /* as the game's start-up: the frame start's shadow pass needs GSmodelInit's shadow slots */
    if (room_path != NULL) {
        printf("[pc_gfx_test] room %s\n", load_room(room_path) ? "set up" : "NOT set up");
    }
    if (room_game_path != NULL) {
        printf("[pc_gfx_test] room (game loader) %s\n", load_room_game(room_game_path) ? "loaded" : "NOT loaded");
    }
    if (model_path != NULL) {
        printf("[pc_gfx_test] model %s\n", load_model(model_path) ? "loaded" : "NOT loaded");
        if (room_camera == NULL) {
            setup_model_scene();
        } else if (gs_model != NULL) {
            /* with a room: stand the model where the room's camera looks (its look-at point when built) */
            void GScameraGetLookAt(void* camera, Vec* up, Vec* interest);
            Vec up, at;
            GScameraGetLookAt(room_camera, &up, &at);
            GSmodelSetPosition(gs_model, (GSvec*)&at);
            printf("[pc_gfx_test] model placed at %g %g %g\n", at.x, at.y, at.z);
        }
    }
    if (seq_path != NULL) {
        /* With --seq, the game's camera controller (camera.c) is started as main.c does, before the Pokemon load (the
         * sequence and move code call into it). With --battle-cam it also drives the view: cameraUpdate runs every
         * frame below, so a move's camera work (battleCameraStartWaza -> cameraMove*) frames it. The test has no floor
         * table: a zero count stands in for it (lbl_80478FB8 points at the count). */
        extern void* lbl_80478FB8;
        static u32 no_floors;
        void cameraInit(void);
        if (lbl_80478FB8 == NULL) {
            lbl_80478FB8 = &no_floors;
        }
        init_pools();
        test_gsres_init();
        cameraInit();
        printf("[pc_gfx_test] game camera controller started\n");
        if (room_camera != NULL && getenv("PC_GFX_ROOM_CAM_HANDOFF") != NULL) {
            /* PC_GFX_ROOM_CAM_HANDOFF=1: the room's authored camera handed to the controller (cameraSetGScamera: its
             * perspective, eye and interest), as the room loader does */
            void cameraSetGScamera(void* camera);
            cameraSetGScamera(room_camera);
            printf("[pc_gfx_test] room camera handed to the controller\n");
        }
    }
    if (seq_path != NULL) {
        printf("[pc_gfx_test] sequence %s\n", load_sequence(seq_path) ? "loaded" : "NOT loaded");
        if (gs_model != NULL && (room_camera == NULL || getenv("PC_GFX_EYE") != NULL)) {
            setup_model_scene(); /* with a room too when PC_GFX_EYE asks for a camera of our own */
        }
        if (room_path != NULL) {
            register_room_resources();
        }
        if (seq2_path != NULL) {
            setup_target(seq2_path, wzx2_path);
        }
        if (wzx_path != NULL) {
            start_wzx(wzx_path);
        }
    }
    if (particles_path != NULL && (gs_model != NULL || emit_all)) {
        init_pools();
        load_particles();
    }
    if (gs_model != NULL && getenv("PC_GFX_COLORSWAP") != NULL) {
        /* PC_GFX_COLORSWAP="r g b a": each output channel taken from input channel N (GSmodelEnableColorSwap, the
         * material extension that sets a TEV swap table; sequence.c / wazaViewer.c use it) */
        void GSmodelEnableColorSwap(GSmodel* model, void* c0, void* c1, void* c2, void* c3);
        unsigned c[4] = {0, 1, 2, 3};
        sscanf(getenv("PC_GFX_COLORSWAP"), "%u %u %u %u", &c[0], &c[1], &c[2], &c[3]);
        GSmodelEnableColorSwap(gs_model, (void*)(uintptr_t)c[0], (void*)(uintptr_t)c[1], (void*)(uintptr_t)c[2],
                               (void*)(uintptr_t)c[3]);
        printf("[pc_gfx_test] colour swap %u %u %u %u\n", c[0], c[1], c[2], c[3]);
    }
    if (gs_model != NULL && getenv("PC_GFX_MODULATE") != NULL) {
        /* PC_GFX_MODULATE="r g b a": every material tinted through the game's material extension
         * (GSmodelEnableModulation, as the battle intro and effects do) */
        void GSmodelEnableModulation(GSmodel* model, const GScolor* modulation);
        unsigned r = 255, g = 255, b = 255, a = 255;
        GScolor c;
        sscanf(getenv("PC_GFX_MODULATE"), "%u %u %u %u", &r, &g, &b, &a);
        c.r = (u8)r;
        c.g = (u8)g;
        c.b = (u8)b;
        c.a = (u8)a;
        GSmodelEnableModulation(gs_model, &c);
        printf("[pc_gfx_test] modulation %u %u %u %u\n", r, g, b, a);
    }
    if (gs_model != NULL && attach_path != NULL) {
        setup_attach();
    }
    if (gs_model != NULL && blend_a >= 0) {
        /* as hero_move.c / people.c: the three blend joint trees once, then the pair and the factor */
        void GSmodelEnableAnimBlend(GSmodel* model);
        GSmodel* m = gs_model;
        GSmodelEnableAnimBlend(gs_model);
        GSmodelSetAnimBlend(gs_model, (u32)blend_a, (u32)blend_b);
        GSmodelSetBlendFactor(gs_model, blend_f);
        GSmodelSetAnimType(gs_model, 1);
        GSmodelStartAnimation(gs_model);
        printf("[pc_gfx_test] blend %d / %d at %g: blending %u, alt tree %p\n", blend_a, blend_b, blend_f,
               (m->flags.raw >> 7) & 1, (void*)m->renderHsdJObjAlt);
    }
    if (gs_model != NULL && getenv("PC_GFX_BOUND") != NULL) {
        /* the model's bounding box as the game builds it (GSmodelRecalculateBound: animation frame 0, every mesh's
         * vertices through the bound walk) */
        void GSmodelRecalculateBound(GSmodel* model);
        GSbound* b = &((GSmodel*)gs_model)->bound;
        GSmodelRecalculateBound(gs_model);
        printf("[pc_gfx_test] bound: empty %d size %g %g %g\n", b->empty, b->size.x, b->size.y, b->size.z);
    }
    if (dl_mode) {
        capture_triangle();
    }
    if (col_path != NULL) {
        printf("[pc_gfx_test] collision %s\n", run_collision(col_path) ? "done" : "NOT loaded");
    }
    if (shadow && gs_model != NULL && room_camera != NULL) {
        setup_shadow();
    }
    if (rtt && gs_model != NULL) {
        setup_rtt();
        if (rtt && getenv("PC_GFX_ENVMAP") != NULL) {
            /* PC_GFX_ENVMAP="mode type blend": a solid-colour texture as the model's environment map
             * (GSmodelEnableEnvMap, the third material extension; effect_visual.c's envMapEffectStart) */
            unsigned mode = 0, type = 0;
            float blend = 0.5f;
            sscanf(getenv("PC_GFX_ENVMAP"), "%u %u %f", &mode, &type, &blend);
            /* a texture of its own, solid green (RGB565 0x07E0, big-endian bytes as GX reads them): the RTT texture
             * would feed back into itself (it starts black and is rendered with the env map) */
            GStextureHandle* env = GStextureCreate(32, 32, 0x44, 0, 0);
            u8* texels = P32(env->mipData[0]);
            u32 k;
            for (k = 0; k + 1 < 32 * 32 * 2; k += 2) {
                texels[k] = 0x07;
                texels[k + 1] = 0xE0;
            }
            GSmodelEnableEnvMap(gs_model, (void*)(uintptr_t)mode, (void*)(uintptr_t)type, env, blend);
            GSmodelSetVisibility(gs_model, 1);
            envmap_on = 1;
            printf("[pc_gfx_test] env map mode %u type %u blend %g\n", mode, type, blend);
        }
    } else {
        rtt = 0;
    }

    for (frame = 0; frame < frames; frame++) {
        const AuroraEvent* event = aurora_update();
        int exiting = 0;
        while (event != NULL && event->type != AURORA_NONE) {
            if (event->type == AURORA_EXIT) {
                exiting = 1;
            }
            ++event;
        }
        if (exiting) {
            break;
        }
        if (!aurora_begin_frame()) {
            continue;
        }
        if (screen_frame >= 0 || shadow_stream != 0 || rtt) {
            /* frame dumps must see every draw: wait for new pipelines instead of skipping draws while they compile */
            AuroraSetDrawsBlocking(true);
        }
        if (model_path != NULL || room_path != NULL) {
            /* as the game does before a frame that draws models (menu_model.c, field_camera): the active camera's view
             * matrix goes into its HSD camera, which then uses it as is */
            _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
        }
        if (gs_anim_all) {
            step_anim_all(frame);
        }
        if (seq_effect != NULL) {
            void fn_801DB088(void);
            if (wzx2_pending && wzx2_delay < 0 && frame > 0) {
                if (!fn_801DA94C(seq_effect, 0, wzx_type)) { /* the attacker's half has ended */
                    wzx2_delay = frame;
                }
            }
            if (wzx2_pending && frame == wzx2_delay) {
                fn_801DA9E8(seq_target, 0, 2);
                wzx2_pending = 0;
                printf("[pc_gfx_test] target WZX started at frame %d\n", frame);
            }
            fn_801DB088(); /* every sequence's step (the battle's per-frame call) */
            if (battle_cam && battle_grid && getenv("PC_GFX_IDLE_CAM") != NULL) {
                /* PC_GFX_IDLE_CAM=1: the battle's idle camera (battleCameraStartRandom: every ~200 frames a random grid
                 * member gets a camera move), as the battle runs it while waiting for commands */
                void battleCameraStartRandom(void);
                battleCameraStartRandom();
            }
            if (battle_cam) {
                void cameraUpdate(void);
                cameraUpdate(); /* the game's camera controller step (its thread calls it once a frame) */
            }
            {
                /* the effect system's two GSgapp tasks (gs_effect.c; no GSgapp scheduler in the test): every active
                 * effect's update, then its render */
                void fn_80130F68(void);
                void fn_80130F04(void);
                fn_80130F68();
                fn_80130F04();
            }
        }
        if (rtt) {
            render_rtt();
        }
        if (frame == 0) printf("[pc_gfx_test] frame begin\n");
        fn_800D3410(NULL, 1); /* begin: per-frame updates, HSD frame start, erase */
        if (draw) {
            draw_triangle();
        }
        if (dl_capture != NULL) {
            draw_dl_triangle();
        }
        if (rtt && !envmap_on) {
            draw_rtt_quad();
        }
        if (backfb && gs_model != NULL) {
            if (frame == 5) {
                backfb_texture = GStextureCreate(640, 480, 0x44, 0, 0);
                printf("[pc_gfx_test] back framebuffer capture: %u\n",
                       GSgfxBeginBackFBCapture(backfb_texture, (void*)backfb_callback, NULL));
            } else if (backfb_done) {
                GSmodelSetVisibility(gs_model, 0);
                draw_backfb_quad();
            }
        }
        /* the game's frame (gs_exact_801E0FB4.c): begin, play the recorded requests back, end; the main loop passes
         * (0x10, 1, 1) */
        fn_800D3FA4(getenv("PC_GFX_OPA_ONLY") ? 0x10 : 0xFFFFFFFF, 1, 1); /* main.c passes 0x10 or -1 (all passes) */
        screen_copy(frame); /* after the playback, before the frame end */
        if (frame == 0) printf("[pc_gfx_test] frame end\n");
        fn_800D3190();        /* end: GX flush / draw done, HSD frame end, end-of-frame updates */
        if (getenv("PC_GFX_SWAP_AT_END") != NULL) {
            void fn_800D30F0(u32 flag);
            fn_800D30F0(0); /* the main loop's "swap" step (main.c fn_80005D80), here before Aurora ends the frame */
        }
        if (frame == 0) printf("[pc_gfx_test] aurora_end_frame\n");
        aurora_end_frame();
        if (rtt) {
            check_rtt(frame);
        }
        if (screen_frame >= 0) {
            screen_collect();
        }
        if (shadow) {
            shadow_debug(frame);
        }
        if (getenv("PC_GFX_MODEL_COUNT") != NULL && frame % 20 == 0) {
            /* PC_GFX_MODEL_COUNT=1: how many of GSmodel's pool slots are in use (flag bit 0), every 20 frames */
            extern GSmodel* lbl_8047AB74;
            extern u32 lbl_8047AB78;
            u32 k, used = 0;
            for (k = 0; k < lbl_8047AB78; k++) {
                used += lbl_8047AB74[k].flags.raw & 1;
            }
            printf("[pc_gfx_test] frame %d: %u of %u model slots in use\n", frame, used, lbl_8047AB78);
        }
        if (particles_path != NULL && frame % 20 == 19) {
            extern u16 lbl_8047B11A; /* live particles (particle.c) */
            printf("[pc_gfx_test] frame %d: %u live particles\n", frame, lbl_8047B11A);
            if (getenv("PC_GFX_PS_DEBUG") != NULL) {
                extern HSD_Particle* _psListGetFirst(s32 linkNo);
                s32 link;
                for (link = 0; link < PS_NUM_LINK; link++) {
                    HSD_Particle* pp = _psListGetFirst(link);
                    if (pp != NULL) {
                        printf("[ps] link %d: life %u wait %u ptr %u pos %g %g %g vel %g %g %g size %g kind %08x tex %u/%u\n",
                               (int)link, pp->life, pp->cmdWait, pp->cmdPtr, pp->pos.x, pp->pos.y, pp->pos.z, pp->vel.x,
                               pp->vel.y, pp->vel.z, pp->size, pp->kind, pp->texGroup, pp->poseNum);
                    }
                }
            }
        }
        fn_800D361C(1);       /* pace: wait for the retrace */
        if (frame == 0) {
            printf("[pc_gfx_test] first frame done\n");
        }
    }
    printf("[pc_gfx_test] %d frames done\n", frame);
    aurora_shutdown();
    return 0;
}
