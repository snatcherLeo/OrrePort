/**
 * @file gs_range_801653CC.c
 * @brief gs-engine sound suffix, 0x80166E88 - 0x80167040.
 *
 * fn_80166E88 initialises the MusyX wrapper. Its allocator hooks are a local
 * aggregate initializer, so MWCC emits the { alloc, free } pair as this
 * unit's own .sdata2 constant (0x8047D558 - 0x8047D560) and copies it to
 * the stack before handing it to sndSetHooks (sndSetHooks).
 */
#include "game/gs_range_801653CC_shared.h"

/* MusyX SND_HOOKS: allocator callbacks passed to sndSetHooks. */
typedef struct GSsndHooks {
    void* (*malloc)(u32 size);
    void (*free)(void* ptr);
} GSsndHooks;

extern u32 _sndEmitter;
extern u32 _maxEmitter;
extern u32 _sndListener;
extern u32 _maxListener;
extern u32 _sndStack;
extern u32 _maxStack;
extern u32 _sndWork;
extern u32 _maxSndWork;
extern void* _soundMemAlloc(u32 size);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _soundMemFree(void* ptr);
#endif
extern void _sndInitSndWork(void);
extern void _sndInitListener(void);
extern void _sndInitEmitter(void);
extern void _sndInitStack(void);
extern void sndSetHooks(GSsndHooks* hooks);
extern s32 fn_8015FE88(u32, u32, u32, u32, u32, u32);
extern void sndAuxCallbackPrepareReverbHI(void* work);
extern void sndAuxCallbackReverbHI(void);
#ifdef TARGET_PC
/* PC: the definition's types (snd_synthapi.c): auxB and userB (args 6-7) are pointers passed on the stack; declared
 * u32, a 0 left their upper halves undefined, so the mixer could call a garbage aux-B callback (B, 2026-10-17) */
extern void sndSetAuxProcessingCallbacks(u8, void* auxA, void* userA, u8, u32, void* auxB, void* userB, u8, u32);
#else
extern void sndSetAuxProcessingCallbacks(u32, void*, void*, u32, u32, u32,
                                         u32, u32, u32);
#endif
extern u32 GSsndGetOutputMode(void);
extern void soundSetOutputMode(u32 mode);
extern void _soundReleaseFinishedVoice(void);
extern void sndSetReceiveMessageCallback(void* callback);

u32 soundInit(u32 stackCount, u32 workSize, u32 extraStackCount,
                u32 emitterCount, u32 emitter3dCount)
{
    GSsndHooks hooks = { _soundMemAlloc, _soundMemFree };

    lbl_8047B0E8 = *snd_data_number;
    _revH_now_parm_id = -1;
    _maxSndWork = stackCount + extraStackCount;

    _sndWork = PC_PTR32(_soundMemAlloc(_maxSndWork * 0x14));
    if (_sndWork == 0) {
        return 0;
    }
    _sndInitSndWork();

    _maxStack = workSize;
    _sndStack = PC_PTR32(_soundMemAlloc(workSize));
    if (_sndStack == 0) {
        return 0;
    }
    _sndInitStack();

    _maxListener = emitterCount;
    _sndListener = PC_PTR32(_soundMemAlloc(emitterCount * 0xD0));
    if (_sndListener == 0) {
        return 0;
    }
    _sndInitListener();

    _maxEmitter = emitter3dCount;
    _sndEmitter = PC_PTR32(_soundMemAlloc(emitter3dCount * 0x78));
    if (_sndEmitter == 0) {
        return 0;
    }
    _sndInitEmitter();

    sndSetHooks(&hooks);
    if (fn_8015FE88(0x40, 0x30, 0x10, 1, 0, 0x9FC000) != 0) {
        return 0;
    }

    _sndSetReverbParm(0);
    sndAuxCallbackPrepareReverbHI(_revH);
    sndSetAuxProcessingCallbacks(0, sndAuxCallbackReverbHI, _revH,
                                 0xFF, 0, 0, 0, 0xFF, 0);
    soundMasterVolume(0x7F, 0, 1, 0);
    soundMasterVolume(0x64, 0, 0, 1);
    soundSetOutputMode(GSsndGetOutputMode());
    sndSetReceiveMessageCallback(_soundReleaseFinishedVoice);
    return 1;
}
