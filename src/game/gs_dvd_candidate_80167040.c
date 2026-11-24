/**
 * @file gs_dvd_candidate_80167040.c
 * @brief MusyX sound-runtime work pools and DVD error task, 0x80167040 - 0x80167E54.
 *
 * Standalone owner for the linked .text range, with its functions in retail
 * address order. The same bodies remain in gs_dvd.c for the neighbouring
 * candidate score units that still include it. The two switch jump tables
 * (_gsdvdErrorTask, _errorTask_State_None) are this
 * unit's .data at 0x8036BF20 - 0x8036BF80.
 */
#include "dolphin/types.h"

#ifdef TARGET_PC
#include <dolphin/dvd.h> /* DVDFileInfo: GSDVDWork embeds the real one on the PC */
#undef DVDRead /* Aurora's macros; the game calls the functions port/src/pc_dvd.c provides */
#undef DVDReadAsync
#define GSDVD_INFO_OFS 8 /* offsetof(GSDVDWork, fileInfo): DVDFileInfo holds pointers, so it is 8-aligned */
#else
#define GSDVD_INFO_OFS 4 /* offsetof(GSDVDWork, fileInfo) */
#endif
#include "pc/disc_types.h" /* replaces a blank line: line-neutral */
typedef struct GSsndWork {
    u8 flags;
    u8 priority;
    u8 unk2;
    u8 unk3;
    u8 stackDepth;
    u8 volumeStack[3];
    u32 handle;
    u32 unkC;
    u32 unk10;
} GSsndWork;

typedef struct DISC_STRUCT GSsndEntry { /* disc layout: shared header */
    u8 flags;
    u8 unk1;
    u8 reverb;
    u8 waveIndex;
    u16 waveId;
    u16 unk6;
    DISC_PTR(GSsndWork) work;
} GSsndEntry;

/* The flags byte as bits (isSe = bit 7). DISC_STRUCT also makes GCC lay the bit-fields out high bit first, as MWCC
 * does (the project notes 10). GCC refuses the address of a field of a big-endian struct, so the PC form of
 * SND_FLAG_BITS casts the entry pointer (flags is at offset 0). */
typedef struct DISC_STRUCT GSsndFlagBits {
    u8 isSe : 1;
    u8 unk6 : 1;
    u8 active : 1;
    u8 paused : 1;
    u8 unk0_3 : 4;
} GSsndFlagBits;
#ifdef TARGET_PC
#define SND_FLAG_BITS(e) ((GSsndFlagBits*)(void*)(e))
#else
#define SND_FLAG_BITS(e) ((GSsndFlagBits*)&(e)->flags)
#endif

#ifdef TARGET_PC
/* _revH is MusyX's own SND_AUX_REVERBHI (sndAuxCallbackPrepareReverbHI gets it): 728 bytes on the PC, whose
 * delay-line pointers are 8 bytes, so the GameCube offsets below land inside the work area (B's handover 39) */
#include "musyx/runtime/reverb.h"
typedef SND_AUX_REVERBHI GSsndReverbState;
#else
typedef struct GSsndReverbState {
    u8 pad0[0x1C4];
    u8 tempDisableFX; /* MusyX's name (SND_AUX_REVERBHI) */
    u8 pad1[3];
    f32 coloration;
    f32 mix;
    f32 time;
    f32 damping;
    f32 preDelay;
    f32 crosstalk;
} GSsndReverbState;
#endif

typedef struct GSDVDWork {
    u8 active;
    u8 started;
    u8 drawing;
    u8 _pad03;
#ifdef TARGET_PC
    DVDFileInfo fileInfo[1]; /* Aurora's DVDFileInfo is ~0x60 bytes and DVDOpen / DVDRead write all of it (B's boot run 33);
                              * one element, so `work->fileInfo` is still the pointer the DVD calls take */
#else
    u8 fileInfo[0x3C];
#endif
    void (*callback)(s32 result, struct GSDVDWork* work);
} GSDVDWork;

typedef struct DISC_STRUCT GSsndOpenState { /* common_rel's sample-archive table (lbl_80478FB4), big-endian */
    u8 _pad00;
    u8 active;
    u16 soundId;
    u8 _pad04[0x14];
} GSsndOpenState;

typedef struct GSsndStartParams {
    u32 unk00;
    u32 unk04;
    u32 unk08;
    u16 unk0C;
    u16 value;
    u8 volume;
    u8 pad11;
    u8 unk12;
    u8 pad13;
#ifdef TARGET_PC
    /* this is MusyX's SND_PLAYPARA (flags, trackMute[2], speed, volume {time, target}, numSeqVolDef, seqVolDef,
     * numFaded, faded), passed to sndSeqPlayEx: on the PC its two pointers are native, so seqVolDef / faded sit at
     * 0x18 / 0x28 where MusyX's PC struct reads them (B's handover 43; numFaded is read only through these fields) */
    void* unk14; /* seqVolDef */
    u8 unk18;    /* numFaded */
    u8 pad19[3];
    void* unk1C; /* faded */
#else
    u32 unk14;
    u8 unk18;
    u8 pad19[3];
    u32 unk1C;
#endif
} GSsndStartParams;

extern void GSlogWrite(const char* fmt, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource(void* ptr, u32 param);
#endif
extern void soundStopRaw(u32 a, u32 b, u32 c);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSsndGetStatus(u32 index);
#endif
extern u32 soundAttachWork(u32 id, u32 arg1, u32 arg2);
extern void soundApplyReverb(u32 reverb);
extern BOOL OSDisableInterrupts(void);
extern BOOL OSRestoreInterrupts(BOOL level);
extern u32 GSmemAlloc(u32 size, u32 align);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmemGetHandleByPtr(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 DVDGetCommandBlockStatus(const void* block);
#endif
extern s32 DVDGetDriveStatus(void);
#ifndef TARGET_PC /* PC: Aurora's prototype (dolphin/dvd.h) */
extern BOOL DVDSeekAsyncPrio(void* fileInfo, s32 offset,
                             void (*callback)(s32 result, void* fileInfo),
                             s32 priority);
#endif
extern u32 ARGetDMAStatus(void);
extern u32 ARQGetChunkSize(void);
extern u32 sndFXCtrl(u32 voice, u8 control, u8 value);
extern u32 sndFXKeyOff(u32 voice);
extern u32 sndFXCheck(u32 voice);
extern u32 sndFXStartEx(u16 id, u8 volume, u8 pan, u8 studio);
extern void sndSeqVolume(u8 group, u16 volume, u32 sequence, u8 fade);
extern u32 sndSeqPlayEx(u8 group, u16 id, void* resource, void* params,
                       u32 flags);
extern void sndSetSampleDataUploadCallback(u8* callback, u32 chunkSize);
extern u8 sndPushGroup(void* resource, u16 soundId, void* sampleData,
                      void* resource2, void* resource3);
extern void winMsgOpenError(u32 message, u32 mode, u32 wait);
extern void winMsgCloseError(u32 wait);
extern void fn_800056D4(void);

extern void _AsyncCallback(s32 result, void* fileInfo);
extern void GSdvdClose(u8* file);
extern GSDVDWork* GSdvdOpen(const char* path);
extern s32 GSdvdRead(GSDVDWork* work, void* addr, s32 length, s32 offset);

extern const char lbl_80273748[]; /* "_sndCheckSndWorkALL:Start" */
extern const char lbl_80273764[]; /* "_sndCheckSndWorkALL:End" */
extern const char lbl_80273780[]; /* "[GSDVD_ERROR_STATE_COVEROPEN_WAIT]..." */

extern GSsndEntry* snd_data;
extern GSsndOpenState* lbl_80478FB4;
extern f32* lbl_80478FA4;
extern GSsndReverbState _revH;
extern u32 lbl_80478C10;
extern u32 lbl_80478C14;
extern u32 lbl_80478C18;
extern u32 lbl_80478C1C;
extern u32 lbl_80478C20;
extern u32 lbl_8047B0B8;
extern GSDVDWork* lbl_8047B0BC;
extern void* lbl_8047B0C0;
extern u8* _sndEmitter;
extern u32 _maxEmitter;
extern u8* _sndListener;
extern u32 _maxListener;
extern u8* _sndStack;
extern u32 _maxStack;
extern u8* _sndWork;
extern u32 _maxSndWork;
extern u32 _revH_now_parm_id;
extern u32 lbl_8047B0E8;
extern s32 lbl_8047B0F0;

u32 _soundFindSlotByVoice(u32 handle);
u8 _soundSlotRelease(u32 entry, u32 flag);
void _sndInitParms(GSsndEntry* entry, GSsndWork* work);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void _soundMemFree(void* ptr);
#endif
void* _soundMemAlloc(u32 size);
void* _sndSetSampleDataUploadCallbackFunction(u32 offset, u32 length);
s32 GSdvdGetDriveStatus(void);

void _soundReleaseFinishedVoice(u32 handle)
{
    u32 entry;

    entry = _soundFindSlotByVoice(handle);
    if (entry != 0) {
        _soundSlotRelease(entry, 0);
    }
}

u8 _soundSlotRelease(u32 index, u32 stop)
{
    extern void _sndFreeSndWork(u8* flag);
    GSsndEntry* entry = &snd_data[index];
    GSsndWork* work;

    if (SND_FLAG_BITS(entry)->active != 1) {
        return 0;
    }
    work = DP(entry->work);
    if (work == NULL) {
        return 0;
    }
    if ((u8)stop == 1) {
        soundStopRaw(index, 0, 0);
    }
    _sndInitParms(entry, work);
    _sndFreeSndWork((u8*)work);
    DP_SET(entry->work, NULL);
    SND_FLAG_BITS(entry)->active = 0;
    return 1;
}

u8 soundPushWave(u32 slot, u32 stream, const char* path, u32 offset,
               void* resourceOwner, u32 resourceId3, u32 resourceId1,
               u32 resourceId2)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _soundMemFree();
#endif
    GSsndOpenState* state = &lbl_80478FB4[slot];
    void* resource1;
    void* sampleData;
    void* resource2;
    void* resource3;

    if (state->active == 1) {
        return 0;
    }
    resource1 = GSresGetResource(PC_PTR32_IMPLICIT(resourceOwner), resourceId1);
    if (resource1 == NULL) {
        return 0;
    }
    resource2 = GSresGetResource(PC_PTR32_IMPLICIT(resourceOwner), resourceId2);
    if (resource2 == NULL) {
        return 0;
    }
    resource3 = GSresGetResource(PC_PTR32_IMPLICIT(resourceOwner), resourceId3);
    if (resource3 == NULL) {
        return 0;
    }
    if (stream == 1) {
        sampleData = NULL;
        sndSetSampleDataUploadCallback((u8*)_sndSetSampleDataUploadCallbackFunction,
                    ARQGetChunkSize());
        lbl_8047B0BC = GSdvdOpen(path);
        if (lbl_8047B0BC == NULL) {
            return 0;
        }
        lbl_8047B0B8 = offset;
        lbl_8047B0C0 = _soundMemAlloc(ARQGetChunkSize());
        if (lbl_8047B0C0 == NULL) {
            GSdvdClose((u8*)lbl_8047B0BC);
            return 0;
        }
    } else {
        sampleData = (void*)path;
        sndSetSampleDataUploadCallback(NULL, 0);
    }
    if (!sndPushGroup(resource1, state->soundId, sampleData, resource2,
                     resource3)) {
        if (stream == 1) {
            GSdvdClose((u8*)lbl_8047B0BC);
        }
        return 0;
    }
    if (stream == 1) {
        _soundMemFree(lbl_8047B0C0);
        GSdvdClose((u8*)lbl_8047B0BC);
    }
    state->active = 1;
    return 1;
}

void* _sndSetSampleDataUploadCallbackFunction(u32 offset, u32 length)
{
    s32 result;

    while (ARGetDMAStatus() != 0) {
    }
    for (;;) {
        result = GSdvdRead(lbl_8047B0BC, lbl_8047B0C0, length,
                             offset + lbl_8047B0B8);
        switch (result) {
        case -3:
        case -1:
            continue;
        }
        break;
    }
    return lbl_8047B0C0;
}

void _sndCheckSndWorkALL(void)
{
    int i;

    GSlogWrite(lbl_80273748);
    for (i = 0; i < lbl_8047B0E8; i++) {
        GSsndGetStatus(i);
    }
    GSlogWrite(lbl_80273764);
}

u32 _soundSlotSetVolume(GSsndEntry* entry, u32 volume, u32 limit)
{
    GSsndWork* work = DP(entry->work);

    if (work == NULL) {
        return 0;
    }

    work->priority = limit & 0x7F;
    if (work->handle != -1) {
        if (SND_FLAG_BITS(entry)->isSe == 1) {
            sndFXCtrl(work->handle, 7, work->priority);
        } else {
            sndSeqVolume(work->priority, (u16)volume, work->handle, 0);
        }
        return 1;
    }
    return 0;
}

u8 _sndSetVolumeWork(u32 id, u32 volume)
{
    GSsndEntry* entry = &snd_data[id];
    GSsndWork* work;

    if (SND_FLAG_BITS(entry)->active != 1) {
        if (SND_FLAG_BITS(entry)->isSe == 1) {
            soundAttachWork(id, 0, 0);
        } else {
            return 0;
        }
    }

    work = DP(entry->work);
    if (work != NULL) {
        work->priority = volume & 0x7F;
    }
    return 1;
}

u32 _sndStopSE(GSsndEntry* entry, u32 arg1, u32 arg2)
{
    GSsndWork* work = DP(entry->work);
    u32 result;

    if (work == NULL) {
        return 0;
    }
    if (work->handle == -1U) {
        return 0;
    }
    if (sndFXCheck(work->handle) != -1) {
        result = sndFXKeyOff(work->handle);
    } else {
        result = 0;
    }
    work->handle = -1;
    return result;
}

u32 _sndStopBGM(GSsndEntry* entry, u32 fade, u32 arg2)
{
    GSsndWork* work = DP(entry->work);

    if (work == NULL) {
        return 0;
    }
    if (SND_FLAG_BITS(entry)->active != 1) {
        return 0;
    }
    if (work->handle == -1U) {
        return 0;
    }
    sndSeqVolume(0, (u16)fade, work->handle, 1);
    work->handle = -1;
    return 1;
}

u32 soundStartFX(GSsndEntry* entry, u32 id)
{
    GSsndWork* work;

    if (SND_FLAG_BITS(entry)->active != 1) {
        soundAttachWork(id, 0, 0);
    }
    work = DP(entry->work);
    if (work != NULL) {
        work->handle =
            sndFXStartEx(entry->waveId, work->priority, work->unk2, 0);
        sndFXCtrl(work->handle, 7, work->priority);
        return 1;
    }
    return 0;
}

u32 _soundSlotStartStream(GSsndEntry* entry, u16 value)
{
    GSsndStartParams params;
    void* resource;
    GSsndWork* work = DP(entry->work);

    if (work == NULL) {
        return 0;
    }
    if (SND_FLAG_BITS(entry)->active != 1) {
        return 0;
    }
    if (work->handle != -1U) {
        return 0;
    }
    resource = (void*)GSresGetResource(PC_PTR32_IMPLICIT((void*)PC_PTR(work->unkC)), work->unk10);
    if (resource == NULL) {
        return 0;
    }
    soundApplyReverb(entry->reverb);
    params.unk00 = 4;
    params.unk04 = 0;
    params.unk08 = 0;
    params.unk0C = 0x100;
    params.value = value;
    params.volume = work->priority;
    params.unk12 = 0;
    params.unk14 = 0;
    params.unk18 = 0;
    params.unk1C = 0;
    work->handle =
        sndSeqPlayEx(entry->unk1, entry->waveId, resource, &params, 0);
    return 1;
}

u32 _soundFindSlotByVoice(u32 handle)
{
    int i;

    for (i = 0; i < lbl_8047B0E8; i++) {
        GSsndWork* work = DP(snd_data[i].work);

        if (work != NULL && work->handle == handle) {
            return i;
        }
    }
    return 0;
}

u32 soundFindOwner(u32 unkC, u32 unk10)
{
    int i;

    for (i = 0; i < lbl_8047B0E8; i++) {
        GSsndWork* work = DP(snd_data[i].work);

        if (work != NULL && work->unkC == unkC && work->unk10 == unk10) {
            return i;
        }
    }
    return -1;
}

/* NAME-T1 fn_801677BC */
void _sndFreeEmitter(u8* flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

/* NAME-T1 fn_801677F4 */
void _sndFreeListener(u8* flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

/* NAME-T1 fn_8016782C */
void _sndFreeSndWork(u8* flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

/* NAME-T1 fn_80167864 */
u8* _sndGetNewEmitter(void)
{
    BOOL enabled;
    u8* entry;
    u32 i;
    u32 offset;
    u8* result;

    enabled = OSDisableInterrupts();
    result = 0;
    for (i = 0; i < _maxEmitter; i++) {
        entry = &_sndEmitter[i * 0x78];
        if (*entry != 1) {
            offset = i * 0x78;
            _sndEmitter[offset] = 1;
            result = &_sndEmitter[offset];
            break;
        }
    }
    OSRestoreInterrupts(enabled);
    return result;
}

/* NAME-T1 fn_801678E4 */
u8* _sndGetNewListener(void)
{
    BOOL enabled;
    u8* entry;
    u8* base;
    u32 i;
    u32 offset;
    u8* result;

    enabled = OSDisableInterrupts();
    entry = base = _sndListener;
    result = 0;
    i = 0;
    while (i < _maxListener) {
        if (*entry != 1) {
            offset = i * 0xD0;
            base[offset] = 1;
            result = &_sndListener[offset];
            break;
        }
        entry += 0xD0;
        i++;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

/* NAME-T1 fn_80167964 */
u8* _sndGetNewSndWork(void)
{
    BOOL enabled;
    u8* entry;
    u8* base;
    u32 i;
    u32 offset;
    u8* result;

    enabled = OSDisableInterrupts();
    entry = base = _sndWork;
    result = 0;
    i = 0;
    while (i < _maxSndWork) {
        if (*entry != 1) {
            offset = i * 0x14;
            base[offset] = 1;
            result = &_sndWork[offset];
            break;
        }
        entry += 0x14;
        i++;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

/* NAME-T1 fn_801679E4 */
void _sndInitEmitter(void)
{
    u32 i;

    for (i = 0; i < _maxEmitter; i++) {
        _sndEmitter[i * 0x78] = 0;
    }
}

/* NAME-T1 fn_80167A14 */
void _sndInitListener(void)
{
    u32 i;

    for (i = 0; i < _maxListener; i++) {
        _sndListener[i * 0xD0] = 0;
    }
}

void _sndInitStack(void)
{
    u32 i;

    for (i = 0; i < _maxStack; i++) {
        _sndStack[i] = 0;
    }
}

/* NAME-T1 fn_80167A6C */
void _sndInitSndWork(void)
{
    u32 i;

    for (i = 0; i < _maxSndWork; i++) {
        _sndWork[i * 0x14] = 0;
    }
}

void _sndSetReverbParm(u32 index)
{
    f32* source = &lbl_80478FA4[index * 6];

    _revH.tempDisableFX = 0;
    /* the presets are common_rel data: big-endian on the PC */
    _revH.coloration = DISC_EL_F32(source, 0);
    _revH.mix = DISC_EL_F32(source, 1);
    _revH.time = DISC_EL_F32(source, 2);
    _revH.damping = DISC_EL_F32(source, 3);
    _revH.preDelay = DISC_EL_F32(source, 4);
    _revH.crosstalk = DISC_EL_F32(source, 5);
    _revH_now_parm_id = index;
}

void _sndInitParms(GSsndEntry* entry, GSsndWork* work)
{
    BOOL enabled = OSDisableInterrupts();
    GSsndFlagBits* flags = SND_FLAG_BITS(entry);

    flags->active = 1;
    flags->paused = 0;
    DP_SET(entry->work, work);
    work->handle = -1;
    work->stackDepth = 0;
    work->priority = 0x7F;
    work->unk2 = 0x40;
    work->unk3 = 0x40;
    OSRestoreInterrupts(enabled);
}

void _soundMemFree(void* ptr) /* frees a buffer by its address (passes ptr through: r3 untouched) */
{
    u32 handle = GSmemGetHandleByPtr(ptr);

    if ((u16)handle != 0) {
        fn_800E24B0(handle);
        GSmemFree(handle);
    }
}

void* _soundMemAlloc(u32 size)
{
    u32 handle = GSmemAlloc(size, 0x20);

    if ((u16)handle != 0) {
        return GSmemLock(handle);
    }
    return 0;
}

void _gsdvdErrorTask(void)
{
    extern void _errorTask_State_None(s32 state);
    extern void _gsdvdError_MsgOpen(u32 message);
    s32 driveState = GSdvdGetDriveStatus();

    switch (lbl_8047B0F0) {
    case 0:
        _errorTask_State_None(driveState);
        break;
    case 1:
        _gsdvdError_MsgOpen(lbl_80478C10);
        lbl_8047B0F0 = 2;
        break;
    case 2:
        GSlogWrite(lbl_80273780, driveState);
        if (driveState != 5 && driveState != 2) {
            winMsgCloseError(1);
            lbl_8047B0F0 = 0;
        }
        break;
    case 3:
        _gsdvdError_MsgOpen(lbl_80478C14);
        lbl_8047B0F0 = 4;
        break;
    case 4:
        if (driveState != 4) {
            winMsgCloseError(1);
            lbl_8047B0F0 = 0;
        }
        break;
    case 5:
        _gsdvdError_MsgOpen(lbl_80478C18);
        lbl_8047B0F0 = 6;
        break;
    case 6:
        if (driveState != 6) {
            winMsgCloseError(1);
            lbl_8047B0F0 = 0;
        }
        break;
    case 7:
        _gsdvdError_MsgOpen(lbl_80478C1C);
        lbl_8047B0F0 = 8;
        break;
    case 8:
        if (driveState != 11) {
            winMsgCloseError(1);
            lbl_8047B0F0 = 0;
        }
        break;
    case 9:
        fn_800056D4();
        _gsdvdError_MsgOpen(lbl_80478C20);
        lbl_8047B0F0 = 10;
        break;
    case 10:
        /* Fatal-error message is up; nothing further to do. */
        break;
    }
}

void _gsdvdError_MsgOpen(u32 message)
{
    if (message != 1) {
        winMsgOpenError(message, 1, 1);
    }
}

void _errorTask_State_None(s32 state)
{
    switch (state) {
    case -1:
        lbl_8047B0F0 = 9;
        break;
    case 5:
        lbl_8047B0F0 = 1;
        break;
    case 4:
        lbl_8047B0F0 = 3;
        break;
    case 6:
        lbl_8047B0F0 = 5;
        break;
    case 11:
        lbl_8047B0F0 = 7;
        break;
    }
}

void soundSetInitParams(u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
    lbl_80478C10 = arg0;
    lbl_80478C14 = arg1;
    lbl_80478C18 = arg2;
    lbl_80478C1C = arg3;
    lbl_80478C20 = arg4;
}

u8 GSdvdSeekAsync(GSDVDWork* work, s32 offset,
               void (*callback)(s32 result, GSDVDWork* work))
{
    work->callback = callback;
    return DVDSeekAsyncPrio(work->fileInfo, offset, _AsyncCallback, 2);
}

s32 fn_80167E10(u8* handle)
{
    return DVDGetCommandBlockStatus(handle + GSDVD_INFO_OFS);
}

s32 GSdvdGetDriveStatus(void)
{
    return DVDGetDriveStatus();
}
