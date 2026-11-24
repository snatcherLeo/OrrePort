/**
 * @file gs_movie_801E189C.c
 * @brief GSmovie: THP movie start (loader thread) and THP-player init hook.
 *
 * Address range: 0x801E189C - 0x801E1B54 (.text) plus its string pool at
 * 0x80279A68 - 0x80279AE8 (.rodata; the three "ERROR(GSmovie): ..." log
 * messages). fn_801E1924 addresses the pool through one base register
 * (base + 0x00 / 0x30 / 0x5C), which only a TU-local literal pool produces,
 * so this unit owns that .rodata range.
 *
 * Built with GC/1.3.2 -O4,s: fn_801E1924's stmw r29 register save and
 * pooled base register need -O4,s, and fn_801E189C's argument scheduling
 * needs 1.3.2+ (GC/1.3 cannot reproduce it at either optimisation level).
 */
#include "dolphin/types.h"

typedef struct OSThread OSThread;

typedef struct GsMovieStartArgs {
    const char* path;
    u32 onMemory;
} GsMovieStartArgs;

typedef struct GsVtrDimensions {
    u16 pad_00;
    u16 pad_02;
    u16 width;
    u16 height;
    u8 pad_08[0x1F0];
} GsVtrDimensions;

typedef struct GsVtrOrigin {
    u32 x;
    u32 y;
    u32 pad_08[2];
} GsVtrOrigin;

extern OSThread lbl_80467D08;            /* loader thread */
extern u8 lbl_80468020[0x1000];         /* loader thread stack */
extern GsVtrDimensions lbl_80466BC0;
extern u32 lbl_80469020[];
extern GsVtrOrigin lbl_80469030;
extern u8 lbl_8047B440;
extern u8 lbl_8047B441;
extern s32 lbl_8047B444;                /* loader thread finished */
extern GsMovieStartArgs lbl_8047B448;   /* loader thread argument */
extern void* lbl_8047B450;
extern u32 lbl_8047B454;
extern u32 lbl_8047B458;
extern GsVtrDimensions* lbl_8047B45C;

extern BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack,
                           u32 stackSize, s32 priority, u16 attr);
extern s32 OSResumeThread(OSThread* thread);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
extern void GSscratchSetInvalid(void);
extern void GSscratchSetValid(void);
extern BOOL THPPlayerOpen(const char* path, BOOL onMemory); /* THPPlayerOpen */
extern void GSlogWrite(const char* format, ...);
extern void THPPlayerGetVideoInfo(GsVtrOrigin* origin);
extern void THPPlayerGetAudioInfo(u32* state);
extern u32 THPPlayerCalcNeedMemory(void);
extern u16 fn_800E2B00(u32 size, u32 alignment);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
extern BOOL THPPlayerSetBuffer(u8* buffer);
extern u32 OSGetTick(void);
extern BOOL THPPlayerPrepare(u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSmemGetHandleByPtr(void* pointer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif
extern void movieGetStreamIds(u32* first, u32* second);
extern void sndStreamMixParameterEx(s32 id, u32, u32, u32, u32, u32);
extern void THPPlayerPlay(void);
extern BOOL THPPlayerInit(void);

/* Loader thread entry: opens the THP file and prepares playback. */
void* fn_801E1924(void* param);

void GSmovieOpen(const char* path, u8 onMemory)
{
#ifdef TARGET_PC
    {
        /* OrreHook_SkipMovies (decision hook, default 0 = vanilla; docs/port/hook-points.md): set by the debug toolkit
         * (skipmovies) or ORRE_SKIP_MOVIES, both outside the game code (src/game/pc_debug_game.c). While set, no movie
         * opens, so the callers' "still playing?" loops end at once. */
        extern int OrreHook_SkipMovies;
        extern void OSReport(const char*, ...);
        if (OrreHook_SkipMovies) {
            OSReport("[movie] skipped %s (OrreHook_SkipMovies)\n", path);
            return;
        }
    }
#endif
    lbl_8047B444 = 0;
    lbl_8047B448.onMemory = onMemory;
    lbl_8047B448.path = path;
    OSCreateThread(&lbl_80467D08, fn_801E1924, &lbl_8047B448, lbl_80468020 + 0xFFC, 0x1000,
                   0x10, 1);
    OSResumeThread(&lbl_80467D08);
    while (lbl_8047B444 == 0) {
        _threadSwitch();
    }
}

void* fn_801E1924(void* param)
{
    GsMovieStartArgs* args = param;
    const char* path;
    u8 onMemory;
    u16 handle;
    u32 remainder;
    u32 first;
    u32 second;

    path = args->path;
    onMemory = args->onMemory;
    GSscratchSetInvalid();
    if (!THPPlayerOpen(path, onMemory)) {
        GSlogWrite("ERROR(GSmovie): Fail to open the thp file = %s\n", path);
        GSscratchSetValid();
        lbl_8047B444 = 1;
        return NULL;
    }

    THPPlayerGetVideoInfo(&lbl_80469030);
    THPPlayerGetAudioInfo(lbl_80469020);
    lbl_8047B45C = &lbl_80466BC0;
    lbl_8047B458 = (lbl_8047B45C->width - lbl_80469030.x) >> 1;
    lbl_8047B454 = (lbl_8047B45C->height - lbl_80469030.y) >> 1;

    handle = fn_800E2B00(THPPlayerCalcNeedMemory(), 0x20);
    if (handle != 0) {
        lbl_8047B450 = GSmemLock(handle);
    } else {
        lbl_8047B450 = NULL;
    }
    if (lbl_8047B450 == NULL) {
        GSlogWrite("ERROR(GSmovie): Can't allocate the memory\n");
        GSscratchSetValid();
        lbl_8047B444 = 1;
        return NULL;
    }

    THPPlayerSetBuffer(lbl_8047B450);
#ifdef TARGET_PC
    /* a movie with no audio tracks has a count of 0: PowerPC's divwu gives an undefined result without trapping, x86
     * traps (B's run 49); the PC takes track 0 (the GameCube's value there is undefined) */
    if (lbl_80469020[3] != 1 && lbl_80469020[3] != 0) {
#else
    if (lbl_80469020[3] != 1) {
#endif
        remainder = OSGetTick() % lbl_80469020[3];
    } else {
        remainder = 0;
    }
    if (!THPPlayerPrepare(0, 0, remainder)) {
        GSlogWrite("ERROR(GSmovie): Fail to prepare\n");
        handle = GSmemGetHandleByPtr(lbl_8047B450);
        if (handle != 0) {
            fn_800E24B0(handle);
            GSmemFree(handle);
        }
        GSscratchSetValid();
        lbl_8047B444 = 1;
        return NULL;
    }

    movieGetStreamIds(&first, &second);
    if (first != (u32)-1) {
        sndStreamMixParameterEx(first, 0, 0, 0, 0x7F, 0);
    }
    if (second != (u32)-1) {
        sndStreamMixParameterEx(second, 0, 0x7F, 0, 0x7F, 0);
    }
    THPPlayerPlay();
    lbl_8047B441 = 1;
    lbl_8047B444 = 1;
    return NULL;
}

void GSmovieInit(void)
{
    THPPlayerInit();
    lbl_8047B440 = 1;
}
