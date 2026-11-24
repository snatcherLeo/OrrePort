/**
 * @file gs_dvd.c
 * @brief DVD-adjacent MusyX sound-runtime unit (per symbols.txt this is
 * mostly _snd-prefixed MusyX audio code, NOT a "GSDVD" disc-I/O API -
 * see 2026-07-02 note below).
 *
 * Per config/GC6E01/splits.txt this unit's real range is
 * 0x80167040 - 0x80168C64. Functions with real (symbols.txt) names -
 * e.g. _sndSetSampleDataUploadCallbackFunction, _sndCheckSndWorkALL,
 * _sndSetVolumeWork, _sndStopSE, _sndStopBGM, _sndInitStack,
 * _sndSetReverbParm, _sndInitParms, _gsdvdErrorTask,
 * _gsdvdError_MsgOpen, GSfilterCreate - confirm this is MusyX audio
 * runtime, matching the "people_field" precedent from a prior
 * campaign note (port from reference, don't brute-force). The many
 * remaining fn_XXXXXXXX gaps are genuinely un-decompiled and still
 * need real work; treat any GSDVD_* naming still present below with
 * suspicion, it has not been verified this pass.
 *
 * 2026-07-02 reconciliation: removed 7 orphan definitions whose names
 * are not present in symbols.txt and never paired in objdiff -
 * GSDVD_CheckAndClose, GSDVD_CloseHandle, GSDVD_Open, GSDVD_EmptyFunc,
 * GSDVD_ErrorStateMachine, GSDVD_ErrorCoverOpenMain, GSDVD_Init - along
 * with their unused GSDVDHandle/GSDVDEntry typedefs. Their bodies were
 * invented fiction (fake register-variable pseudo-assembly, e.g.
 * GSDVD_Init's body was actually a free-list/allocator coalescing
 * routine, unrelated to "DVD init"). This pass did not otherwise
 * change any other function or the rest of this file's speculative
 * GSDVD_* naming table below, which remains unverified.
 *
 * 2026-07-03: re-tried porting the archive bodies for _soundReleaseFinishedVoice
 * ("GSDVD_CheckAndClose", 48 bytes) and _soundSlotRelease
 * ("GSDVD_CloseHandle", 168 bytes) under their correct current
 * fn_ names -- both compile but land at 66.7% / 76.2% fuzzy match
 * (not 100%), confirming the archive content for this file is
 * unreliable register-level fiction, not real disassembly. Reverted
 * both, no net change. The only new function landed this pass is
 * fn_80167FA4 (trivial 4-byte empty function / blr-only stub),
 * confirmed 100% via objdiff.
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

#define SND_FLAG_SE 0x80
#define SND_FLAG_ACTIVE 0x20
#define SND_FLAG_PAUSED 0x10

/* ===== External SDK / engine functions ===== */
extern void  GSlogWrite(const char* fmt, ...);         /* OSReport / GSlog */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource(void* ptr, u32 param);        /* resource resolution */
#endif
extern void  soundStopRaw(u32 a, u32 b, u32 c);         /* sound subsystem */
extern void  fn_8016A644(void* ptr);                    /* resource cleanup */
extern void  fn_80169520(void* ptr);                    /* status flag update */

/* ===== String constants (rodata) ===== */
extern const char lbl_80273748[]; /* "_sndCheckSndWorkALL:Start" */
extern const char lbl_80273764[]; /* "_sndCheckSndWorkALL:End" */
extern const char lbl_80273780[]; /* "[GSDVD_ERROR_STATE_COVEROPEN_WAIT]..." */

/* ===== BSS / global state ===== */
extern GSsndEntry* snd_data;

/* Forward declarations for converted functions */
s32 fn_80167E54(void);
u32 GSdvdGetLength(u8* obj);
void fn_80167FA4(void);


/*
 * 2026-07-02 reconciliation: removed the fictional GSDVDHandle /
 * GSDVDEntry typedefs and the fictional bodies of GSDVD_CheckAndClose,
 * GSDVD_CloseHandle, GSDVD_Open, GSDVD_EmptyFunc and
 * GSDVD_ErrorStateMachine (orphans - none of these names are present in
 * symbols.txt, none paired in objdiff, and neither typedef was even
 * referenced by anything else in this file). See the file header for
 * the honest picture of what this unit actually is.
 */

/* ==================================================================
 * fn_80168284 -- _sndCheckSndWorkALL
 *
 * Validate all sound work buffers. Logs start/end markers.
 * 388 bytes.
 *
 * From disassembly references:
 *   lbl_80273748: "_sndCheckSndWorkALL:Start"
 *   lbl_80273764: "_sndCheckSndWorkALL:End"
 * ================================================================== */
extern u32 lbl_8047B0E8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSsndGetStatus(u32 index);
#endif

void _sndCheckSndWorkALL(void)
{
    int i;

    GSlogWrite(lbl_80273748);
    for (i = 0; i < lbl_8047B0E8; i++) {
        GSsndGetStatus(i);
    }
    GSlogWrite(lbl_80273764);
}

/*
 * 2026-07-02 reconciliation: removed the fictional bodies of
 * GSDVD_ErrorCoverOpenMain and GSDVD_Init (orphans - neither name is
 * present in symbols.txt, neither paired in objdiff). Both were
 * unreferenced anywhere else in-tree. GSDVD_Init's real target address
 * (0x80168934, per its own header comment) is symbols.txt's
 * "particleSort" (size matches exactly, 0x330 bytes) - an unrelated
 * name that doesn't belong to a DVD/sound module; renaming to it was
 * not attempted since the removed body was generic linked-list/
 * allocator code with no real connection to that function's semantics.
 */

/* ==================================================================
 * fn_80167FA4 -- ported from archive (verified 100% via objdiff).
 * Archive labeled this "GSDVD_EmptyFunc" but that name is not in
 * symbols.txt (this unit is actually MusyX audio, see file header);
 * kept the fn_ name. 4-byte empty function (just blr).
 * ================================================================== */
void fn_80167FA4(void) {
    /* intentionally empty */
}

/* ===================================================================
 * AUTO-GENERATED accessor functions
 * Generated by tools/gen_accessors.py
 * 2 functions matched
 * =================================================================== */

/* Address: 0x80167E54 | Size: 0x8 | Pattern: return_constant */
s32 fn_80167E54(void) { return -1; }

/* Address: 0x80167E5C | Size: 0x8 | Pattern: simple_getter */
u32 GSdvdGetLength(u8* obj) {
#ifdef TARGET_PC
    return ((DVDFileInfo*)((u8*)obj + GSDVD_INFO_OFS))->length; /* GameCube: fileInfo +0x34 */
#else
    return *(u32*)((u8*)obj + 0x38);
#endif
}

/* 2026-07-24: decompiled directly from the current disassembly (not the
 * unreliable archive bodies noted above). */
extern BOOL OSDisableInterrupts(void);
extern BOOL OSRestoreInterrupts(BOOL level);
extern u32 _soundFindSlotByVoice(u32 handle);
extern u8 _soundSlotRelease(u32 entry, u32 flag);
extern u8 *_sndEmitter;
extern u32 _maxEmitter;
extern u8 *_sndListener;
extern u32 _maxListener;
extern u8 *_sndWork;
extern u32 _maxSndWork;
extern void winMsgOpenError(u32 message, u32 mode, u32 wait);

void _soundReleaseFinishedVoice(u32 handle)
{
    u32 entry;

    entry = _soundFindSlotByVoice(handle);
    if (entry != 0) {
        _soundSlotRelease(entry, 0);
    }
}

/* NAME-T1 fn_801677BC */ /* XD's map, GSsnd.o: the sound pools (emitter 0x78, listener 0xD0, sound work 0x14) */
void _sndFreeEmitter(u8 *flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

/* particleSort (0x80168934) is linked from pslist.c, the particle link-list
 * unit it belongs to (it sorts pslist's per-link lists). */

/* NAME-T1 fn_801677F4 */
void _sndFreeListener(u8 *flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

/* NAME-T1 fn_8016782C */
void _sndFreeSndWork(u8 *flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

/* NAME-T1 fn_80167864 */ /* was _soundPool0Alloc */
u8 *_sndGetNewEmitter(void)
{
    BOOL enabled;
    u8 *entry;
    u32 i;
    u32 offset;
    u8 *result;

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
u8 *_sndGetNewListener(void)
{
    BOOL enabled;
    u8 *entry;
    u8 *base;
    u32 i;
    u32 offset;
    u8 *result;

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

/* NAME-T1 fn_80167964 */ /* was _soundPool2Alloc */
u8 *_sndGetNewSndWork(void)
{
    BOOL enabled;
    u8 *entry;
    u8 *base;
    u32 i;
    u32 offset;
    u8 *result;

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

void _gsdvdError_MsgOpen(u32 message)
{
    if (message != 1) {
        winMsgOpenError(message, 1, 1);
    }
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
extern s32 DVDConvertPathToEntrynum(const char* path);
extern s32 DVDGetCommandBlockStatus(const void* block);
extern s32 DVDGetDriveStatus(void);
extern void DVDClose(void* fileInfo);
extern s32 DVDRead(void* fileInfo, void* addr, s32 length, s32 offset,
                   s32 priority);
extern BOOL DVDReadAsync(void* fileInfo, void* addr, s32 length, s32 offset,
                         void (*callback)(s32 result, void* fileInfo),
                         s32 priority);
void _freeDvdWork(u8* flag);

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
#ifdef TARGET_PC
#include <stddef.h>
_Static_assert(offsetof(GSDVDWork, fileInfo) == GSDVD_INFO_OFS, "GSDVD_INFO_OFS");
#endif

typedef struct DISC_STRUCT GSsndOpenState { /* common_rel's sample-archive table (lbl_80478FB4), big-endian */
    u8 _pad00;
    u8 active;
    u16 soundId;
    u8 _pad04[0x14];
} GSsndOpenState;

typedef struct GSFilter {
    u8 index;
    u8 active;
    u8 drawing;
    u8 _pad03;
} GSFilter;

typedef struct GSFilterState {
    GSFilter* filters;
    u8* colors;
    u16 viewport[8];
    u8 capacity;
    u8 count;
    u8 drawingCount;
    u8 _pad1B;
    u16 filterHandle;
    u16 colorHandle;
    u32 renderState;
} GSFilterState;

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

extern GSDVDWork* _dvdWork;
extern u32 _maxDvdWork;
extern GSFilterState lbl_804526E0;
extern void GXDrawDone(void);
extern void GXInvalidateVtxCache(void);
extern u32 sndFXCtrl(u32 voice, u8 control, u8 value);
extern u32 sndFXKeyOff(u32 voice);
extern void sndSeqVolume(u8 group, u16 volume, u32 sequence, u8 fade);
extern u32 sndFXCheck(u32 voice);
extern u32 soundAttachWork(u32 id, u32 arg1, u32 arg2);
extern BOOL DVDSeekAsyncPrio(void* fileInfo, s32 offset,
                            void (*callback)(s32 result, void* fileInfo),
                            s32 priority);

extern u8* _sndStack;
extern u32 _maxStack;
extern u8* _sndWork;
extern u32 _maxSndWork;
extern u32 _revH_now_parm_id;
extern u32 lbl_8047B0E8;
extern f32* lbl_80478FA4;
extern GSsndReverbState _revH;
extern u32 lbl_80478C10;
extern u32 lbl_80478C14;
extern u32 lbl_80478C18;
extern u32 lbl_80478C1C;
extern u32 lbl_80478C20;
extern const u8 lbl_8047D578[];
extern const u8 lbl_8047D594[];
extern const u8* lbl_80478C24;
extern const u8* lbl_80478C28;
extern u32 lbl_8047B0B8;
extern GSDVDWork* lbl_8047B0BC;
extern void* lbl_8047B0C0;
extern GSsndOpenState* lbl_80478FB4;
extern const char lbl_8047D584[];
extern const char lbl_8047D58C[];
extern u32 ARGetDMAStatus(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* condition);
#endif
extern BOOL DVDOpen(const char* path, void* fileInfo);
extern u32 sndFXStartEx(u16 id, u8 volume, u8 pan, u8 studio);
extern void soundApplyReverb(u32 reverb);
extern u32 sndSeqPlayEx(u8 group, u16 id, void* resource, void* params,
                      u32 flags);
extern void DCFlushRange(void* address, u32 size);
extern void DVDInit(void);
extern BOOL DVDSetAutoFatalMessaging(BOOL enable);
extern s32 GSgetVersionMajor(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSgetVersionMinor(void);
#endif
extern u8* DVDGetCurrentDiskID(void);
extern u32 GSgappCreate(s32 state, u8 priority, u32 parameter,
                       void (*callback)(void));
extern void winMsgCloseError(u32 wait);
extern void fn_800056D4(void);
extern u32 ARQGetChunkSize(void);
extern void sndSetSampleDataUploadCallback(u8* callback, u32 chunkSize);
extern u8 sndPushGroup(void* resource, u16 soundId, void* sampleData,
                     void* resource2, void* resource3);
extern const f32 lbl_8047D5A0;
extern const f32 lbl_8047D5A4;
extern const f32 lbl_8047D5A8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(u32 enable);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(f32 red, f32 green, f32 blue, f32 alpha);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetAlphaCompare(u32, u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u32);
#endif
extern void gfxVtxPos1x8(u32);
extern void gfxVtxColor1x8(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
extern void* gfxVtxDescAlloc(void);
extern void gfxVtxDescSetAttr(void*, u32, u32, u32, u32, u8, void*, u8);
extern void gfxVtxDescFree(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void*, int, u32);
#endif
extern GSFilter* lbl_8047B100;

void _sndInitParms(GSsndEntry* entry, GSsndWork* work);
s32 GSdvdRead(GSDVDWork* work, void* addr, s32 length, s32 offset);
GSDVDWork* _getNewDvdWork(void);
void _initDvdWork(void);
void* fn_8016824C(u32 size);
void _gsdvdErrorTask(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void _soundMemFree(void* ptr);
#endif
void* _soundMemAlloc(u32 size);
void GSdvdClose(u8* file);
GSDVDWork* GSdvdOpen(const char* path);
void* _sndSetSampleDataUploadCallbackFunction(u32 offset, u32 length);

GSDVDWork* _info2work(void* fileInfo);

void _AsyncCallback(s32 result, void* fileInfo)
{
    GSDVDWork* work;

    work = _info2work(fileInfo);
    if (work != NULL && work->callback != NULL) {
        work->callback(result, work);
    }
}

GSDVDWork* _info2work(void* fileInfo)
{
    GSDVDWork* cursor;
    GSDVDWork* base;
    u32 i;

    cursor = base = _dvdWork;
    for (i = 0; i < _maxDvdWork; cursor++, i++) {
        if (cursor->active != 0 && cursor->fileInfo == fileInfo) {
            return &base[i];
        }
    }
    return NULL;
}

void _soundMemFree(void* ptr) /* frees a buffer by its address (passes ptr through: r3 untouched) */
{
    u32 handle = GSmemGetHandleByPtr(ptr);

    if ((u16)handle != 0) {
        fn_800E24B0(handle);
        GSmemFree(handle);
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
    resource1 = GSresGetResource(resourceOwner, resourceId1);
    if (resource1 == NULL) {
        return 0;
    }
    resource2 = GSresGetResource(resourceOwner, resourceId2);
    if (resource2 == NULL) {
        return 0;
    }
    resource3 = GSresGetResource(resourceOwner, resourceId3);
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
    resource = (void*)GSresGetResource((void*)work->unkC, work->unk10);
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

void* _soundMemAlloc(u32 size)
{
    u32 handle = GSmemAlloc(size, 0x20);

    if ((u16)handle != 0) {
        return GSmemLock(handle);
    }
    return 0;
}

s32 fn_80167E10(u8* handle)
{
    return DVDGetCommandBlockStatus(handle + GSDVD_INFO_OFS);
}

s32 GSdvdGetDriveStatus(void)
{
    return DVDGetDriveStatus();
}

void GSdvdClose(u8* file)
{
    _freeDvdWork(file);
    DVDClose(file + GSDVD_INFO_OFS);
}

u32 fn_80167EF8(const char* path)
{
    return DVDConvertPathToEntrynum(path) != -1;
}

GSDVDWork* GSdvdOpen(const char* path)
{
    GSDVDWork* work = _getNewDvdWork();

    if (work == NULL) {
        return NULL;
    }
    if (!DVDOpen(path, work->fileInfo)) {
        __assert("GSdvd.c", 0x26A, "FALSE");
        _freeDvdWork((u8*)work);
        return NULL;
    }
    return work;
}

u8 GSdvdReadAsync(GSDVDWork* work, void* addr, s32 length, s32 offset,
                void (*callback)(s32 result, GSDVDWork* work))
{
    work->callback = callback;
    return DVDReadAsync(work->fileInfo, addr, length, offset, _AsyncCallback,
                        2);
}

s32 GSdvdRead(GSDVDWork* work, void* addr, s32 length, s32 offset)
{
    return DVDRead(work->fileInfo, addr, length, offset, 2);
}

/* NAME-T1 fn_80168164 */ /* XD's map, GSdvd.o */
void _freeDvdWork(u8* flag)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    *flag = 0;
    OSRestoreInterrupts(enabled);
}

u32 GSdvdInit(u32 workCount)
{
    u8* destination;

    _maxDvdWork = workCount;
    _dvdWork = fn_8016824C(workCount * sizeof(GSDVDWork));
    if (_dvdWork == NULL) {
        return 0;
    }
    _initDvdWork();
    DVDInit();
    if (GSgetVersionMinor() != 4) {
        switch (GSgetVersionMajor()) {
        case 0:
            lbl_80478C24 = (const u8*)"GC6J";
            break;
        case 1:
            lbl_80478C24 = (const u8*)"GC6E";
            break;
        case 2:
            lbl_80478C24 = (const u8*)"GC6E";
            break;
        }
        destination = DVDGetCurrentDiskID();
        destination[0] = lbl_80478C24[0];
        destination[1] = lbl_80478C24[1];
        destination[2] = lbl_80478C24[2];
        destination[3] = lbl_80478C24[3];
        destination[4] = lbl_80478C28[0];
        destination[5] = lbl_80478C28[1];
        destination[6] = 0;
        destination[7] = 0;
    }
    DVDSetAutoFatalMessaging(TRUE);
    GSgappCreate(1, 0x13, 0, _gsdvdErrorTask);
    return 1;
}

/* NAME-T1 fn_8016819C */ /* XD's map, GSdvd.o: takes the first free request record */
GSDVDWork* _getNewDvdWork(void)
{
    GSDVDWork* base;
    GSDVDWork* work;
    GSDVDWork* result;
    BOOL enabled;
    u32 i;

    enabled = OSDisableInterrupts();
    result = NULL;
    work = base = _dvdWork;
    for (i = 0; i < _maxDvdWork; work++, i++) {
        if (work->active != 1) {
            base[i].active = 1;
            result = &_dvdWork[i];
            break;
        }
    }
    OSRestoreInterrupts(enabled);
    return result;
}

/* NAME-T1 fn_8016821C */ /* XD's map, GSdvd.o: marks every request record free */
void _initDvdWork(void)
{
    u32 i;

    i = 0;
    while (i < _maxDvdWork) {
        _dvdWork[i].active = 0;
        i++;
    }
}

void fn_80168284(void)
{
    u32 index;
    u8 capacity;
    GSFilter* filter;

    if (lbl_804526E0.count != 0) {
        if (lbl_804526E0.drawingCount != 0) {
            capacity = lbl_804526E0.capacity;
            filter = lbl_804526E0.filters;
            gfxSetFlags(1);
            gfxClearFlags(6);
            gfxSetOrtho(lbl_8047D5A0, lbl_8047D5A0, lbl_8047D5A4,
                        lbl_8047D5A8);
            gfxSetBlendMode(1, 6, 7);
            gfxSetUpdates(1, 1, 0);
            gfxSetAlphaCompare(0, 7, 0, 0, 7, 0);
            gfxSetZMode(1, 1, 1);
            fn_800D9ED8(1);
            gfxSetCullMode(0);
            gfxSetVtxDesc(lbl_804526E0.renderState);
            gfxSetPrimitive(6);
            gfxBegin((u16)(lbl_804526E0.drawingCount * 4));
            for (index = 0; (u8)index < capacity; index++, filter++) {
                if (filter->drawing != 0) {
                    gfxVtxPos1x8(0);
                    gfxVtxColor1x8(0, index);
                    gfxVtxPos1x8(1);
                    gfxVtxColor1x8(0, index);
                    gfxVtxPos1x8(2);
                    gfxVtxColor1x8(0, index);
                    gfxVtxPos1x8(3);
                    gfxVtxColor1x8(0, index);
                }
            }
            gfxEnd();
            fn_800D9ED8(0);
        }
    }
}

void* fn_8016824C(u32 size)
{
    u32 handle = GSmemAlloc(size, 0x20);

    if ((u16)handle != 0) {
        return GSmemLock(handle);
    }
    return 0;
}

void GSfilterRelease(GSDVDWork* work)
{
    if (work->started != 0) {
        work->started = 0;
        if (work->drawing != 0) {
            GXDrawDone();
            GXInvalidateVtxCache();
            work->drawing = 0;
            lbl_804526E0.drawingCount--;
        }
        lbl_804526E0.count--;
    }
}

void GSfilterSetColor(GSFilter* filter, const u8* color)
{
    u8* destination;

    if (filter->active == 0) {
        return;
    }
    destination = &lbl_804526E0.colors[filter->index * 4];
    destination[0] = color[0];
    destination[1] = color[1];
    destination[2] = color[2];
    destination[3] = color[3];
    DCFlushRange(destination, 4);
    if (color[3] != 0) {
        if (filter->drawing == 0) {
            filter->drawing = 1;
            lbl_804526E0.drawingCount++;
        }
    } else if (filter->drawing != 0) {
        GXDrawDone();
        GXInvalidateVtxCache();
        filter->drawing = 0;
        lbl_804526E0.drawingCount--;
    }
}

/*
 * fn_80168638 carries GSfilterCreate's body inline. The linked owner of
 * fn_80168638 is the standalone carve gs_dvd_r47_80168638_o4s.c; this copy
 * is kept in step with it. `inline` here means this file emits no
 * out-of-line GSfilterCreate (gs_dvd_r47_prefix.c owns that one).
 */
inline GSFilter* GSfilterCreate(const u8* color)
{
    GSFilter* filter;
    u8 capacity = lbl_804526E0.capacity;
    u8 index;

    if (lbl_804526E0.count < capacity) {
        filter = lbl_804526E0.filters;
        for (index = 0; index < capacity; index++, filter++) {
            if (filter->active == 0) {
                u8* destination = &lbl_804526E0.colors[index * 4];
                destination[0] = color[0];
                destination[1] = color[1];
                destination[2] = color[2];
                destination[3] = color[3];
                if (color[3] != 0 && filter->drawing == 0) {
                    filter->drawing = 1;
                    lbl_804526E0.drawingCount++;
                }
                filter->active = 1;
                lbl_804526E0.count++;
                return filter;
            }
        }
    }
    return NULL;
}

/*
 * Tears the filter manager back down. Repeated expansion: retail carries
 * this exact sequence on both allocation-failure paths.
 */
static inline void GSfilterRelease(void)
{
    u16 handle;

    handle = lbl_804526E0.filterHandle;
    if (handle != 0) {
        fn_800E24B0(handle);
        GSmemFree(handle);
    }
    handle = lbl_804526E0.colorHandle;
    if (handle != 0) {
        fn_800E24B0(handle);
        GSmemFree(handle);
    }
    if (lbl_804526E0.renderState != 0) {
        gfxVtxDescFree((void*)lbl_804526E0.renderState);
    }
    memset(&lbl_804526E0, 0, sizeof(GSFilterState));
}

/*
 * Creates the filter render state and binds the viewport and colour
 * arrays to it. Inline fingerprint: on the NULL path retail loads the
 * result register with a fresh `li r28, 0` even though the preceding
 * `mr. r28, r3` has just proven it zero; that is this helper's own
 * `return NULL` materialised into the inlined return temp. Written
 * inline in the caller, the store takes the tested value directly and
 * no such load exists.
 */
static inline void* GSfilterRenderStateCreate(void)
{
    void* renderState = gfxVtxDescAlloc();

    if (renderState != NULL) {
        gfxVtxDescSetAttr(renderState, 1, 1, 0, 2, 0, lbl_804526E0.viewport, 4);
        gfxVtxDescSetAttr(renderState, 4, 1, 6, 10, 0, lbl_804526E0.colors, 4);
        return renderState;
    }
    return NULL;
}

void fn_80168638(u8 capacity)
{
    u32 size;
    u16 handle;
    GSFilter* filter;
    u32 index;
    u8 clearColor[4];

    memset(&lbl_804526E0, 0, sizeof(GSFilterState));
    size = capacity * sizeof(GSFilter);

    handle = _toolentryAlloc__FUl(size);
    if (handle == 0) {
        return;
    }
    lbl_804526E0.filterHandle = handle;
    lbl_804526E0.filters = GSmemLock(lbl_804526E0.filterHandle);
    memset(lbl_804526E0.filters, 0, size);

    handle = _toolentryAlloc__FUl(size);
    if (handle == 0) {
        GSfilterRelease();
        return;
    }
    lbl_804526E0.colorHandle = handle;
    lbl_804526E0.colors = GSmemLock(lbl_804526E0.colorHandle);
    memset(lbl_804526E0.colors, 0, size);

    /* Retail allocates a third block and only checks it for failure. */
    handle = _toolentryAlloc__FUl(size);
    if (handle == 0) {
        GSfilterRelease();
        return;
    }

    lbl_804526E0.viewport[0] = 0;
    lbl_804526E0.viewport[1] = 0;
    lbl_804526E0.viewport[2] = 0;
    lbl_804526E0.viewport[3] = 480;
    lbl_804526E0.viewport[4] = 640;
    lbl_804526E0.viewport[5] = 480;
    lbl_804526E0.viewport[6] = 640;
    lbl_804526E0.viewport[7] = 0;
    lbl_804526E0.capacity = capacity;

    lbl_804526E0.renderState = (u32)GSfilterRenderStateCreate();

    filter = lbl_804526E0.filters;
    for (index = 0; (u8)index < capacity; index++, filter++) {
        filter->index = index;
    }

    clearColor[0] = 0;
    clearColor[1] = 0;
    clearColor[2] = 0;
    clearColor[3] = 0;
    lbl_8047B100 = GSfilterCreate(clearColor);
    GSgappCreate(1, 0xFC, 0, fn_80168284);
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

extern s32 lbl_8047B0F0;

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
