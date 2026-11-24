/**
 * @file fade.c
 * @brief Screen fade state machine (retail game/pxdvs/app/fade/fade.cpp).
 *
 * Address range: 0x801C4078 - 0x801C4814 (11 functions)
 * Small data:    .sdata2 0x8047DFA8 - 0x8047DFD8 (the two fade colours,
 *                this TU's float literals and its int-to-float constants)
 *
 * The whole TU is built with -opt nopeephole (see configure.py).
 *
 * The fade state is shared between the fade daemon, which advances it once
 * per frame, and callers that poll it from other GS threads (fadeCheck spins
 * on the pending flag until the daemon clears it), so the object is declared
 * volatile: every read and write of it is a real memory access in retail.
 */

#include "dolphin/types.h"

typedef struct GStexture GStexture;

/* Per-frame fade hook installed by a hook's init function through
 * fadeSetFunctionOnly; returns 0 when the effect has finished. */
typedef u8 (*FadeHook)(u8 pending, f32 elapsed, f32 duration, f32 hookElapsed,
                       f32 hookDuration, GStexture* texture);

typedef struct FadeState {
    /* 0x00 */ u8 mode;         /* 0 idle, 1/2 fade out/in, 3/4 held */
    /* 0x01 */ u8 pending;      /* 1 while a timed fade is running */
    /* 0x02 */ u16 type;        /* bit 0: fade in, bit 1: black, bit 3: hold */
    /* 0x04 */ f32 duration;
    /* 0x08 */ f32 elapsed;
    /* 0x0C */ FadeHook hook;
    /* 0x10 */ GStexture* texture; /* back-buffer snapshot for the hook */
    /* 0x14 */ f32 hookDuration;
    /* 0x18 */ f32 hookElapsed;
} FadeState;

typedef struct FadeColor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} FadeColor;

extern volatile FadeState _fadeWork;
extern u8 lbl_8047B3A8;

static const FadeColor sFadeWhite = { 0xFF, 0xFF, 0xFF, 0xFF };
static const FadeColor sFadeBlack = { 0x00, 0x00, 0x00, 0xFF };

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
extern void fadeEffectHookFunction_Doku_Init(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStartSE(s32 arg0);
#endif
extern u8 menuOffScreenIsDoing(void);
extern GStexture* menuOffScreenGetPtr(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern GStexture* GStextureCreate(u32 arg0, u32 arg1, u32 format, u32 arg3, u32 arg4);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GStextureFree(GStexture* texture);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgfxBeginBackFBCapture(GStexture* texture, void* callback, void* userData);
#endif
extern void fn_801C6928(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(f32 left, f32 top, f32 right, f32 bottom);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(u32 arg0, u32 arg1, u32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(u32 arg0, u32 arg1, u32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(u32 arg0, u32 arg1, u32 arg2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u32 arg0);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos(f32 x, f32 y, f32 z);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(u32 arg0, u8 r, u8 g, u8 b, u8 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif

FadeHook fadeSetEX(u16 type, void (*init)(void), u8 snapshot, f32 duration, f32 hookDuration);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fadeSet(u16 type, f32 duration);
#endif
FadeHook fadeSetFunction__FPFv_vbUsf(void (*init)(void), u8 snapshot, u16 type, f32 hookDuration);
void _fadeSnapshot__Fv(void);
void* myBackFB__FP9GStextureUlPv(GStexture* texture, u32 size, void* userData);

/* 0x801C4078 | 0x24: cancel a held "doku" (poison) fade. */
void fadeEffectDokuStop(void) {
    if (_fadeWork.mode == 4) {
        _fadeWork.pending = 0;
        _fadeWork.mode = 0;
    }
}

/* 0x801C409C | 0x54: start the held "doku" (poison) fade when idle. */
void fadeEffectDokuStart(void) {
    if (_fadeWork.mode == 0) {
        fadeSetEX(9, fadeEffectHookFunction_Doku_Init, 0, 1.0f, 0.5f);
        soundStartSE(0x54);
    }
}

/* 0x801C40F0 | 0x74: return the pending flag, optionally waiting for the
 * running fade to finish first. */
#ifdef TARGET_PC
/* ORRE_FADE_LOG=1 (2026-10-20; observes only): when a thread waiting in fadeCheck starts, wakes and returns, by
 * game frame and retrace. Wes's name screen: the console's background particles run one frame longer after the fade
 * than the PC's, so the script's teardown after this wait comes a frame earlier on the PC */
static void fadeCheckLogPC(const char* what) {
    static int enabled = -1;
    extern u32 pc_vi_heartbeat;
    extern u32 VIGetRetraceCount(void);
    extern void OSReport(const char*, ...);
    if (enabled < 0) {
        extern char* getenv(const char*);
        const char* env = getenv("ORRE_FADE_LOG");
        enabled = env != NULL && *env != '\0' && *env != '0';
    }
    if (enabled) {
        /* the gapp task running now (gs_gapp.c's GSgappTask, native layout on the PC): its priority says where in the
         * frame this happens (0x0A mainLogicTask, 0xFE mainRetraceLogic with menuDaemon and fadeDaemon) */
        typedef struct { void* prev; void* next; s32 state; u8 priority; } GappTaskPC;
        extern GappTaskPC* lbl_8047AC94;
        OSReport("[fade_check] %s frame %u retrace %u pending %u task 0x%X\n", what, (unsigned)pc_vi_heartbeat,
                 (unsigned)VIGetRetraceCount(), (unsigned)_fadeWork.pending,
                 lbl_8047AC94 != NULL ? (unsigned)lbl_8047AC94->priority : 0x100u);
    }
}
#define FADE_CHECK_LOG(what) fadeCheckLogPC(what)
#else
#define FADE_CHECK_LOG(what)
#endif

s8 fadeCheck(u8 wait) {
    volatile FadeState* state;

    if (wait <= 0) {
        return _fadeWork.pending;
    }
    FADE_CHECK_LOG("wait");
    state = &_fadeWork;
    while (state->pending == 1) {
        _threadSwitch();
        FADE_CHECK_LOG("woke");
    }
    _threadSwitch();
    FADE_CHECK_LOG("return");
    return _fadeWork.pending;
}

/* 0x801C4164 | 0x64: install a hook and start a timed fade. */
FadeHook fadeSetEX(u16 type, void (*init)(void), u8 snapshot, f32 duration, f32 hookDuration) {
    FadeHook previous;

    previous = fadeSetFunction__FPFv_vbUsf(init, snapshot, type, hookDuration);
    fadeSet(type, duration);
    return previous;
}

/* 0x801C41C8 | 0x74: start a timed fade. */
void fadeSet(u16 type, f32 duration) {
#ifdef TARGET_PC
    {
        /* OrreHook_FadeDuration (docs/port/hook-points.md): the fade's length in seconds, as OrrePort wants it (the
         * PC screen opened from PC MENU fades out and back in at half length). Nothing registered: the game's own */
        extern f32 (*OrreHook_FadeDuration)(u16 type, f32 duration);
        /* OrreHook_FadeHold (docs/port/hook-points.md; 2026-11-15): on the way from the PC screen's Main Menu to
         * the title's main menu the field comes back for a moment under the black screen; its fade-in becomes "stay
         * black" (a fade-out of 0 s, as movieGsLogoExit does). Nothing registered or 0: the fade as asked */
        extern s32 (*OrreHook_FadeHold)(u16 type);
        if (OrreHook_FadeHold != NULL && OrreHook_FadeHold(type) != 0) {
            type = 3;
            duration = 0.0f;
        } else if (OrreHook_FadeDuration != NULL) {
            duration = OrreHook_FadeDuration(type, duration);
        }
    }
    { extern void OSReport(const char*, ...); OSReport("[fade] set type 0x%X duration %d/100\n", (unsigned)type, (int)(duration * 100.0f)); } /* PC test log */
    FADE_CHECK_LOG("set");
#endif
    _fadeWork.pending = 1;
    _fadeWork.type = type;
    _fadeWork.duration = duration;
    _fadeWork.elapsed = 0.0f;

    if (type & 8) {
        if (type & 1) {
            _fadeWork.mode = 4;
        } else {
            _fadeWork.mode = 3;
        }
    } else {
        if (type & 1) {
            _fadeWork.mode = 2;
        } else {
            _fadeWork.mode = 1;
        }
    }
}

/* 0x801C423C | 0xE0: reset the hook state, optionally snapshot the back
 * buffer, and run the hook's init function; returns the previous hook. */
FadeHook fadeSetFunction__FPFv_vbUsf(void (*init)(void), u8 snapshot, u16 type, f32 hookDuration) {
    FadeHook previous;

    if (init == NULL) {
        return _fadeWork.hook;
    }

    _fadeWork.pending = 1;
    _fadeWork.type = type;
    _fadeWork.hookDuration = hookDuration;
    _fadeWork.hookElapsed = 0.0f;
    _fadeWork.mode = 0;
    previous = _fadeWork.hook;
    _fadeWork.hook = NULL;

    if (snapshot == 1) {
        _fadeSnapshot__Fv();
    } else if (_fadeWork.texture != NULL) {
        if (_fadeWork.texture != menuOffScreenGetPtr()) {
            GStextureFree(_fadeWork.texture);
        }
        _fadeWork.texture = NULL;
    }

    fn_801C6928();
    init();
    return previous;
}

/* 0x801C431C | 0x10: install the per-frame hook (called by hook inits). */
void fadeSetFunctionOnly(FadeHook hook) {
    _fadeWork.hook = hook;
}

/* 0x801C432C | 0xB8: capture the back buffer into the fade texture. */
void _fadeSnapshot__Fv(void) {
    GStexture* texture;

    texture = _fadeWork.texture;
    _fadeWork.texture = NULL;
    if (!menuOffScreenIsDoing()) {
        texture = menuOffScreenGetPtr();
    }
    if (texture == NULL) {
        texture = GStextureCreate(0, 0, 0x44, 0, 0);
    }
    if (texture != NULL) {
        lbl_8047B3A8 = 0;
        GSgfxBeginBackFBCapture(texture, myBackFB__FP9GStextureUlPv, NULL);
        while (lbl_8047B3A8 == 0) {
            _threadSwitch();
        }
        _fadeWork.texture = texture;
    }
}

/* 0x801C43E4 | 0x10: back-buffer capture completion callback. */
void* myBackFB__FP9GStextureUlPv(GStexture* texture, u32 size, void* userData) {
    lbl_8047B3A8 = 1;
    return NULL;
}

#ifdef TARGET_PC
/* PC: the transition hooks (installed by fade_effect.c's init functions) are not all defined with FadeHook's parameter
 * list. Seven take the texture second, (u32 pending, texture, then the four times): fadeEffectHookFunction_trainer (trainer),
 * fadeEffectHookFunction_fadeout_in (fadeout-in), fadeEffectHookFunction_boss, fadeEffectHookFunction_ball, fadeEffectHookFunction_tate, fadeEffectHookFunction_yoko and fadeEffectHookFunction_carde. On
 * the GameCube that is the same call, since integer and pointer arguments go in r3, r4, ... and floats in f1, ...
 * whatever their position. Windows x64 passes arguments by position, so called through FadeHook those hooks read the
 * texture from a float slot and every time shifted by one: the battle-start transitions drew garbage, a black screen
 * (A, 2026-10-17). The Doku and fadein hooks match FadeHook's order (fadein reads only the first two times) and are
 * called as declared. */
typedef u8 (*FadeHookTextureSecond)(u32 pending, GStexture* texture, f32 elapsed, f32 duration, f32 hookElapsed,
                                     f32 hookDuration);

static u8 fadeCallHookPC(FadeHook hook, u8 pending, f32 elapsed, f32 duration, f32 hookElapsed, f32 hookDuration,
                         GStexture* texture) {
    extern u32 fadeEffectHookFunction_carde(u32, void*, f32, f32, f32, f32);
    const void* fn = (const void*)hook;

    if (fn == (const void*)fadeEffectHookFunction_trainer || fn == (const void*)fadeEffectHookFunction_fadeout_in || fn == (const void*)fadeEffectHookFunction_boss ||
        fn == (const void*)fadeEffectHookFunction_ball || fn == (const void*)fadeEffectHookFunction_tate || fn == (const void*)fadeEffectHookFunction_yoko ||
        fn == (const void*)fadeEffectHookFunction_carde) {
        return ((FadeHookTextureSecond)fn)(pending, texture, elapsed, duration, hookElapsed, hookDuration);
    }
    return hook(pending, elapsed, duration, hookElapsed, hookDuration, texture);
}
#endif

/* 0x801C43F4 | 0x3DC: per-frame fade update and full-screen quad draw. */
void fadeDaemon(void) {
    FadeColor white = sFadeWhite;
    FadeColor black = sFadeBlack;
    FadeColor color;
    f32 fps;
    f32 progress;
    u8 alpha;

    if (_fadeWork.pending == 1) {
        switch (_fadeWork.mode) {
        case 0:
        case 4:
            break;
        case 1:
        case 2:
        case 3:
            fps = fn_800D37CC();
            _fadeWork.elapsed += (f32)GSgetFrameStep() / fps;
            if (_fadeWork.elapsed >= _fadeWork.duration) {
                _fadeWork.elapsed = _fadeWork.duration;
                _fadeWork.pending = 0;
                if (_fadeWork.type & 1) {
                    _fadeWork.mode = 2;
                } else {
                    _fadeWork.mode = 0;
                }
            }
            break;
        }
    }

    if (_fadeWork.hook != NULL) {
        fps = fn_800D37CC();
        _fadeWork.hookElapsed += (f32)GSgetFrameStep() / fps;
        if (_fadeWork.hookElapsed >= _fadeWork.hookDuration) {
            _fadeWork.hookElapsed = _fadeWork.hookDuration;
        }
#ifdef TARGET_PC
        /* OrreHook_2DPiece (observation; frame smoothing's 2D at 120, fix 3 after the census, 2026-11-18): names the
         * Poké Ball wipe's draws (group 3, id 3: its two trails of 12 balls, moved and drawn by the hook every frame),
         * so an in-between draws each ball part way between steps (src/game/pc_debug_game.c). Only the ball style: the
         * other styles draw a captured picture back (the trainer swirl) or a fluid grid, left as they were. The end
         * tag below is sent whatever the hook returns. All platforms */
        u8 pcBallWipe = (const void*)_fadeWork.hook == (const void*)fadeEffectHookFunction_ball;
        u8 pcHookAlive;
        if (pcBallWipe) {
            extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
            pc_orre_2d_tag(0x30000, 0, (const void*)3, 0.0f);
        }
        pcHookAlive = fadeCallHookPC(_fadeWork.hook, _fadeWork.pending, _fadeWork.elapsed, _fadeWork.duration,
                                     _fadeWork.hookElapsed, _fadeWork.hookDuration, _fadeWork.texture);
        if (pcBallWipe) {
            extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
            pc_orre_2d_tag(0, 0, (const void*)0, 0.0f);
        }
        if (!pcHookAlive) {
#else
        if (!_fadeWork.hook(_fadeWork.pending, _fadeWork.elapsed, _fadeWork.duration,
                               _fadeWork.hookElapsed, _fadeWork.hookDuration,
                               _fadeWork.texture)) {
#endif
            _fadeWork.hook = NULL;
            if (_fadeWork.texture != NULL) {
                if (_fadeWork.texture != menuOffScreenGetPtr()) {
                    GStextureFree(_fadeWork.texture);
                }
                _fadeWork.texture = NULL;
            }
        }
    }

    switch (_fadeWork.mode) {
    case 0:
        break;
    case 1:
    case 2:
        if (_fadeWork.duration == 0.0f) {
            progress = 1.0f;
        } else {
            progress = _fadeWork.elapsed / _fadeWork.duration;
        }
        if (_fadeWork.type & 1) {
            alpha = 255.0f * progress;
        } else {
            alpha = 255 - (s32)(255.0f * progress);
        }
        if (_fadeWork.type & 2) {
            color = black;
        } else {
            color = white;
        }

        fn_800D9ED8(1);
        gfxSetFlags(1);
        gfxClearFlags(6);
        gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
        gfxSetBlendMode(1, 6, 7);
        gfxSetUpdates(1, 1, 0);
        gfxSetZMode(0, 1, 1);
        gfxSetCullMode(0);
        gfxSetPrimitive(7);
        gfxSetVtxDesc(0);
#ifdef TARGET_PC
        { /* OrreHook_2DPiece (observation; frame smoothing's 2D at 120, round 12): names the screen fade's full-screen
           * quad (group 3, id 2; its alpha the corners' colour), so an in-between draws the fade part way between
           * steps instead of at the newest step's level (src/game/pc_debug_game.c). All platforms */
            extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
            pc_orre_2d_tag(0x30000, 0, (const void*)2, (f32)alpha);
        }
#endif
        gfxBegin(2);
        gfxVtxPos(0.0f, 0.0f, 0.0f);
        gfxVtxColor4u8(0, color.r, color.g, color.b, alpha);
        gfxVtxPos(640.0f, 480.0f, 0.0f);
        gfxVtxColor4u8(0, color.r, color.g, color.b, alpha);
        gfxEnd();
#ifdef TARGET_PC
        {
            extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
            pc_orre_2d_tag(0, 0, (const void*)0, 0.0f);
        }
#endif
        fn_800D9ED8(0);
        break;
    case 3:
    case 4:
        break;
    }
}

/* 0x801C47D0 | 0x44: reset the fade state. */
void fadeInit(void) {
    _fadeWork.mode = 0;
    _fadeWork.pending = 0;
    _fadeWork.type = 0;
    _fadeWork.duration = 0.0f;
    _fadeWork.elapsed = 0.0f;
    _fadeWork.hook = NULL;
    _fadeWork.texture = NULL;
    _fadeWork.hookDuration = 0.0f;
    _fadeWork.hookElapsed = 0.0f;
}
