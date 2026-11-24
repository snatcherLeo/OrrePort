/**
 * @file pc_bcam_log.c
 * @brief OrrePort only: ORRE_BCAM_LOG=1, an observe-only log of every battle-camera shot, for OrrePort Studio's camera
 *        check (the project's helper, 2026-11-19; asked for by Studio).
 *
 * Why: Studio ports the battle camera (studio/core/src/battle_camera.cpp) and replays each shot from the same inputs and
 * the same random state; to prove the port it must match the game's camera on every frame. The inputs a shot is built
 * from (the owner's model, size class, facing, slot and its time points, the aim joint, the effect sequence's flags, the
 * previous motion, the shared generator's state) exist only for a moment inside battleCameraStartWaza, so the game file
 * (src/game/wazaSequenceCamera.c) calls a weak pointer at five places (pc_bcam_hook: start, aim read, procedural shot
 * placed, authored camera started, the per-frame aim), and this file prints them; the per-frame camera (eye, interest,
 * field of view, frame step) is printed from the end of every game frame (pc_event_log_frame in pc_debug_game.c).
 *
 * Observe-only (not a hook, the project rules "Hook points"): everything here reads; nothing is written to game state, and the
 * shared generator is never called (its state is read through HSD_RandSeedPtr, lbl_80478C94, and the draw count is
 * worked out by stepping a local copy of the same arithmetic), so the battle rolls are the same with the log on and off
 * (checked on a the reference playthrough replay: the ORRE_ROLL_RECORD files with and without it are identical). With ORRE_BCAM_LOG unset
 * the pointer stays NULL and the game runs exactly as without this file.
 *
 * The line format is fixed (Studio parses it): docs/port/setup.md, "The battle-camera log (ORRE_BCAM_LOG)". Change it
 * only together with that row, and bump BCAM_FORMAT.
 *
 * Not part of the GameCube build (configure.py doesn't list it); port/tools/firstlink.py compiles it with the game units
 * (PORT_GAME_SOURCES). Platforms: plain C, getenv and OSReport; the same on Windows, Linux and macOS (verified on
 * Windows only).
 */
#ifdef TARGET_PC

#include "dolphin/types.h"
#include "game/gs_render_util.h" /* GSRenderCamera: the render camera's eye, interest and HSD camera object */
#include "game/gs_bound_offsets.h" /* the model box's minimum / maximum / size offsets, as the camera reads them */
#include "pc/ptr32.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BCAM_FORMAT 1 /* the "v=" of the header line; bump when a line's keys change */

extern void OSReport(const char*, ...);
extern u32 VIGetRetraceCount(void);
extern f32 HSD_CObjGetFov(void* cobj);
extern u16 battleGridGetNumPokemonsForTrainer(u32 id);
extern void* GSmodelGetBound(void* model);

/* the move camera's state (wazaSequenceCamera.c; docs/engine/battle-camera.md "Data structures") */
extern void* lbl_8047B3EC;  /* the active shot's owner */
extern void* lbl_8047B3F0;  /* its effect sequence, NULL for an idle shot */
extern u32 lbl_8047B3E8;    /* the zoom keys' frame counter */
extern f32 lbl_80478CDC;    /* the field of view the zoom keys gave this frame (cameraSetFov) */
extern u8 lbl_804673A0[];   /* WazaSequenceCameraParams: s32 motion, then 12 floats */
extern u8 lbl_804673D4[];   /* CameraFovKey[2]: f32 start, f32 end, u32 startFrame, u32 endFrame */
extern s32 gLastMotionType; /* the previous procedural motion (data/sdata_80478CA8.c) */
extern u32* lbl_80478C94;   /* HSD_RandSeedPtr: the shared generator's state (docs/engine/rng.md) */
extern u32 lbl_8047ACC8;    /* the floor context: null before the first room */

/* the replay's resyncs, read only to say whether the first draw may take another state (port/src/pc_frame_times.c,
 * src/game/pc_debug_game.c); weak so a build without them still links */
extern u32 OrreHook_RngFrameState __attribute__((weak));
extern int OrreHook_RngFrameStatePending __attribute__((weak));
extern void (*OrreHook_RngBeforeDraw)(u32* state) __attribute__((weak));
extern s32 pc_battle_serial __attribute__((weak));
extern volatile u32 pc_vi_heartbeat __attribute__((weak));

/* the weak pointer the game file calls (wazaSequenceCamera.c, top); NULL unless ORRE_BCAM_LOG is on */
void (*pc_bcam_hook)(s32 stage, void* owner, void* sequence, s32 value, const f32* a, const f32* b, const f32* c);

static u32 s_shot;          /* shots since start-up (1-based; 0 = none yet) */
static int s_active;        /* a shot is running and gets frame lines */
static void* s_owner;       /* the running shot's owner and sequence (lbl_8047B3EC / F0 when it started) */
static void* s_seq;
static u32 s_frames;        /* frame lines printed for the running shot */
static u32 s_seed;          /* the state at the shot's start, and the state its first draw starts from */
static u32 s_firstState;
static int s_haveAim;       /* stage 4 ran since the last frame line */
static s32 s_aimPart;
static f32 s_aim[3], s_origin[3];

static char s_line[3072];
static int s_len;

static void put(const char* fmt, ...)
{
    va_list ap;
    if (s_len >= (int)sizeof(s_line) - 1) {
        return;
    }
    va_start(ap, fmt);
    s_len += vsnprintf(s_line + s_len, sizeof(s_line) - (size_t)s_len, fmt, ap);
    va_end(ap);
    if (s_len > (int)sizeof(s_line) - 1) {
        s_len = (int)sizeof(s_line) - 1;
    }
}

static u32 fbits(f32 v)
{
    u32 u;
    memcpy(&u, &v, 4);
    return u;
}

/* " key=a,b,c keyh=xxxxxxxx,xxxxxxxx,xxxxxxxx": the floats as %.6f for reading, and their IEEE-754 single bits (the
 * u32 value in hex) for exact comparison */
static void putf(const char* key, const f32* v, int n)
{
    int i;
    put(" %s=", key);
    for (i = 0; i < n; i++) {
        put(i ? ",%.6f" : "%.6f", (double)v[i]);
    }
    put(" %sh=", key);
    for (i = 0; i < n; i++) {
        put(i ? ",%08x" : "%08x", (unsigned)fbits(v[i]));
    }
}

static void flush_line(void)
{
    OSReport("[bcam] %s\n", s_line);
    s_len = 0;
    s_line[0] = '\0';
}

static u32 now_frame(void)
{
    return &pc_vi_heartbeat != NULL ? (u32)pc_vi_heartbeat : 0u;
}

/* the render camera (GSres group 0, id 0): the GS camera's own eye / interest (what the view matrix is built from) and
 * the HSD camera object's vertical field of view, as ORRE_CAMERA_LOG and the move viewer read them */
static void put_render_camera(void)
{
    GSRenderCamera* cam = (GSRenderCamera*)GSresGetResource(0, 0);
    if (cam == NULL) {
        put(" eye=none interest=none fov=none");
        return;
    }
    putf("eye", &cam->eye.x, 3);
    putf("interest", &cam->interest.x, 3);
    if (cam->cobj != NULL) {
        f32 fov = HSD_CObjGetFov(cam->cobj);
        putf("fov", &fov, 1);
    } else {
        put(" fov=none");
    }
}

/* the draws from the start state to now: the generator's own step (HSD_Rand: state * 214013 + 2531011) applied to a
 * local copy; "?" when the state wasn't reached in 65,536 steps (a replay resync replaced a state in between) */
static void put_draws(void)
{
    u32 end = *lbl_80478C94;
    u32 s = s_firstState;
    u32 n;
    for (n = 0; n < 65536u && s != end; n++) {
        s = s * 214013u + 2531011u;
    }
    put(" seed_end=%08x", (unsigned)end);
    if (s == end) {
        put(" draws=%u", (unsigned)n);
    } else {
        put(" draws=?");
    }
}

static void end_shot(const char* why)
{
    if (!s_active) {
        return;
    }
    put("end shot=%u frames=%u frame=%u why=%s", (unsigned)s_shot, (unsigned)s_frames, (unsigned)now_frame(), why);
    flush_line();
    s_active = 0;
}

static void bcam_start(u8* owner, u8* seq)
{
    u8* model = PTR32_AT_T(u8, owner, 0x24);
    const u8* slot = (const u8*)PTR32_AT(owner, 0x2C) + *(const u16*)(owner + 0x32) * 0xD4;
    f32 v[3];
    int i;

    end_shot("replaced");
    ++s_shot;
    s_owner = owner;
    s_seq = seq;
    s_frames = 0;
    s_haveAim = 0;
    s_seed = *lbl_80478C94;
    /* replay mode's per-frame resync (ORRE_REPLAY_RNG): a posted state is taken by the next draw, so the shot's first
     * draw starts from it instead */
    s_firstState = (&OrreHook_RngFrameStatePending != NULL && OrreHook_RngFrameStatePending &&
                    &OrreHook_RngFrameState != NULL)
                       ? OrreHook_RngFrameState
                       : s_seed;

    put("start shot=%u frame=%u retrace=%u battle=%d room=%u res=0x%x:0x%x", (unsigned)s_shot, (unsigned)now_frame(),
        (unsigned)VIGetRetraceCount(), &pc_battle_serial != NULL ? (int)pc_battle_serial : -1,
        lbl_8047ACC8 != 0 ? (unsigned)std092_getCurrentRoomID() : 0u, (unsigned)*(const u32*)(owner + 0x00),
        (unsigned)*(const u32*)(owner + 0x04));
    put(" model_id=%u size_class=%d facing=%d is_pokemon=%u owner18=0x%02x", (unsigned)*(const u16*)(owner + 0x70),
        (int)*(const s32*)(owner + 0x10), (int)*(const s8*)(owner + 0x76), (unsigned)owner[0x75],
        (unsigned)owner[0x18]);
    if (model != NULL) {
        u8* bound = (u8*)GSmodelGetBound(model);
        GSmodelGetPosition((struct GSmodel*)model, (struct GSvec*)v);
        putf("pos", v, 3);
        GSmodelGetRotation((struct GSmodel*)model, (struct GSvec*)v);
        putf("rot", v, 3);
        putf("bmin", (const f32*)(bound + GS_BOUND_OFF_MIN), 3);
        putf("bmax", (const f32*)(bound + GS_BOUND_OFF_MAX), 3);
        putf("bsize", (const f32*)(bound + GS_BOUND_OFF_SIZE), 3);
    } else {
        put(" pos=none");
    }
    if (PTR32_AT(owner, 0x2C) != NULL) {
        put(" slot=%u tp_count=%u anim_count=%u slot_flags=0x%x tp=", (unsigned)*(const u16*)(owner + 0x32),
            (unsigned)*(const u32*)(slot + 0x00), (unsigned)*(const u32*)(slot + 0x04),
            (unsigned)*(const u32*)(slot + 0x08));
        for (i = 0; i < 6; i++) {
            put(i ? ",%d" : "%d", (int)*(const s32*)(slot + 0x0C + i * 4));
        }
        put(" attach=");
        for (i = 0; i < 16; i++) {
            put(i ? ",%d" : "%d", (int)*(const s32*)(slot + 0x4C + i * 4));
        }
    } else {
        put(" slot=none");
    }
    if (seq != NULL) {
        put(" seq=move flags=0x%08x aim_byte=%u seq_id=%u seq_kind=%u anim_res=0x%x:0x%x",
            (unsigned)*(const u32*)(seq + 0x08), (unsigned)seq[0x17], (unsigned)*(const u16*)(seq + 0x2C),
            (unsigned)*(const u16*)(seq + 0x2E), (unsigned)*(const u32*)(seq + 0x18),
            (unsigned)*(const u32*)(seq + 0x20));
    } else {
        put(" seq=idle");
    }
    put(" last_motion=%d seed=%08x", (int)gLastMotionType, (unsigned)s_seed);
    if (s_firstState != s_seed) {
        put(" posted=%08x", (unsigned)s_firstState);
    } else {
        put(" posted=none");
    }
    put(" rng_hook=%d", (&OrreHook_RngBeforeDraw != NULL && OrreHook_RngBeforeDraw != NULL) ? 1 : 0);
    flush_line();
}

/* stage 1: right after cameraRefreshTargetPos, so CameraPadState's position (the model's) and view (the aim joint
 * minus it) are exactly what the camera read */
static void bcam_aim(u8* owner, u8* seq, s32 paramsFlags)
{
    const u8* slot = (const u8*)PTR32_AT(owner, 0x2C) + *(const u16*)(owner + 0x32) * 0xD4;
    const u32 index = seq != NULL ? seq[0x17] : 2u; /* the camera's own choice: attach[aim byte], or attach[2] */
    f32 pos[3], view[3], aim[3];

    GSscene_GetCameraPositionVector((struct GSSceneVec3*)pos);
    GSscene_GetCameraViewVector((struct GSSceneVec3*)view);
    aim[0] = pos[0] + view[0];
    aim[1] = pos[1] + view[1];
    aim[2] = pos[2] + view[2];
    put("aim shot=%u pflags=0x%02x", (unsigned)s_shot, (unsigned)paramsFlags);
    if (owner[0x75] == 0) {
        /* a trainer: how many Pokémon it has out (battleGridGetNumPokemonsForTrainer, read-only), the input of pflags */
        put(" partners=%u", (unsigned)battleGridGetNumPokemonsForTrainer(PC_PTR32(owner)));
    } else {
        put(" partners=-");
    }
    put(" aim_index=%u aim_joint=%d", (unsigned)index,
        PTR32_AT(owner, 0x2C) != NULL ? (int)((const s32*)(slot + 0x4C))[index] : -1);
    putf("target_pos", pos, 3);
    putf("aim_view", view, 3);
    putf("aim", aim, 3);
    flush_line();
}

static void put_params_and_keys(void)
{
    const u8* k = lbl_804673D4;
    int i;
    putf("params", (const f32*)(lbl_804673A0 + 4), 12);
    for (i = 0; i < 2; i++, k += 0x10) {
        char key[8];
        snprintf(key, sizeof(key), "fk%d", i);
        putf(key, (const f32*)k, 2);
        put(" %sf=%u,%u", key, (unsigned)*(const u32*)(k + 8), (unsigned)*(const u32*)(k + 12));
    }
    putf("fov_key", &lbl_80478CDC, 1);
    put(" fovt=%u", (unsigned)lbl_8047B3E8);
}

static void bcam_hook(s32 stage, void* owner, void* sequence, s32 value, const f32* a, const f32* b, const f32* c)
{
    switch (stage) {
    case 0:
        bcam_start((u8*)owner, (u8*)sequence);
        break;
    case 1:
        bcam_aim((u8*)owner, (u8*)sequence, value);
        break;
    case 2: /* a procedural shot placed: the motion picked, the state after, the parameters, keys and camera */
        put("picked shot=%u kind=proc motion=%d reverse=%d shift=%d", (unsigned)s_shot, (int)*(const s32*)lbl_804673A0,
            (int)(value & 1), (int)((value >> 1) & 1));
        put_draws();
        put_params_and_keys();
        put_render_camera();
        flush_line();
        s_active = 1;
        break;
    case 3: /* an authored camera started (cameraPlayOffsetAnime): no draws; its offset */
        put("picked shot=%u kind=anim anim_shift=%d", (unsigned)s_shot, (int)value);
        put_draws();
        putf("ofs_pos", a, 3);
        putf("ofs_rot", b, 3);
        putf("ofs_scale", c, 3);
        put_render_camera();
        flush_line();
        s_active = 1;
        break;
    case 4: /* battleCameraUpdateWaza's aim this frame (printed with the frame line) */
        if (s_active && owner == s_owner && sequence == s_seq) {
            s_haveAim = 1;
            s_aimPart = value;
            memcpy(s_aim, a, sizeof(s_aim));
            memcpy(s_origin, b, sizeof(s_origin));
        }
        break;
    }
}

/* once per game frame, at its end (pc_event_log_frame, from port/src/pc_vi.c): the running shot's camera */
void pc_bcam_log_frame(u32 frame)
{
    static int enabled = -1;
    if (enabled < 0) {
        const char* env = getenv("ORRE_BCAM_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
        if (enabled) {
            pc_bcam_hook = bcam_hook;
            OSReport("[bcam] log v=%d (ORRE_BCAM_LOG; docs/port/setup.md, \"The battle-camera log\")\n", BCAM_FORMAT);
        }
    }
    if (!enabled || !s_active) {
        return;
    }
    if (lbl_8047B3EC != s_owner || lbl_8047B3F0 != s_seq) {
        end_shot("ended");
        return;
    }
    ++s_frames;
    put("frame shot=%u n=%u frame=%u step=%u motion=%d", (unsigned)s_shot, (unsigned)s_frames, (unsigned)frame,
        (unsigned)GSgetFrameStep(), (int)*(const s32*)lbl_804673A0);
    put_render_camera();
    putf("fov_key", &lbl_80478CDC, 1);
    put(" fovt=%u", (unsigned)lbl_8047B3E8);
    if (s_haveAim) {
        put(" part=%d", (int)s_aimPart);
        putf("aim", s_aim, 3);
        putf("origin", s_origin, 3);
    } else {
        put(" part=- aim=- origin=-");
    }
    {
        f32 view[3];
        GSscene_GetCameraViewVector((struct GSSceneVec3*)view);
        putf("view", view, 3);
    }
    flush_line();
    s_haveAim = 0;
}

#endif /* TARGET_PC */
