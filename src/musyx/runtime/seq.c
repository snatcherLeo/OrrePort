/**
 * @file seq.c
 * @brief MusyX runtime sequencer prefix, 0x8014635C - 0x80146E88.
 *
 * Split out of the misnamed people_field.c unit (2026-07-02). Reference:
 * AxioDL/musyx `musyx/runtime/seq.c`, cross-verified byte-exact against the
 * matched copies in Mario Party 4, Metroid Prime and Mario Strikers
 * (GC/1.3.2). Boundary evidence: seqGetPrivateId (0x8014635C) is the first
 * seq.c function after the item-use function itemUse2Pokemon; seqInit
 * (0x8014A094 + 0x1A8) is reference seq.c's last function and ends exactly
 * at synthSetBpm (0x8014A23C), reference synth.c's first.
 */

#include "dolphin/types.h"
#include "game/people/people.h"

/* ===== External SDK / engine functions ===== */
extern void  GSlogWrite(const char* fmt, ...);
#ifndef TARGET_PC /* PC: <string.h> (pc/prototypes.h) declares them with size_t */
extern void* memset(void* dst, int val, u32 size);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif
#endif
extern void  DCFlushRange(void* ptr, u32 size);
extern u32   OSDisableInterrupts(void);
extern void  OSRestoreInterrupts(u32 level);

/* renamed symbols referenced by asm incs (symbolmap port) */
extern void ARQPostRequest();
extern void InitStreamBuffers();
extern void aramQueueCallback();
extern void aramUploadData();
extern u32 inpGetMidiCtrl(u32 ctrl, u32 bank, u32 channel);
#ifndef TARGET_PC /* GameCube register view (vol travels in f1); never called here. PC: the real prototype is
                   hw_volconv.c's (u8, SAL_VOLINFO*, f32 vol, u32, u32, f32, f32, u32, u32): argument order matters on x64 */
extern void salCalcVolume(u32 volumeArg, f32* out, u32 pan, u32 surroundPan,
                          f32 a, f32 b, f32 c, u32 narrowPan,
                          u32 studioMode);
#endif
extern void salCallback();
extern u8 jumptable_80369CB0[];
extern u8 jumptable_80369CD4[];
extern u8 jumptable_80369CF8[];
extern u8 lbl_80273448[];
extern u8 lbl_8036944C[];
extern u8 lbl_8036BF00[];
extern u8 lbl_80434C50[];
extern f32 lbl_8047D4D8;
extern f32 lbl_8047D4DC;
extern f32 lbl_8047D4E0;
extern f64 lbl_8047D4E8;
extern f32 lbl_8047D4F0;
extern f32 lbl_8047D4F4;
extern f32 lbl_8047D4F8;
extern f32 lbl_8047D4FC;
extern f32 lbl_8047D500;
extern f32 lbl_8047D504;
extern f64 lbl_8047D508;
extern f32 lbl_8047D510;
extern f32 lbl_8047D514;
extern f64 lbl_8047D518;
extern f32 lbl_8047D520;
extern f64 lbl_8047D528;
extern f32 lbl_8047D530;
extern f32 lbl_8047D534;
extern u32 lbl_8047B070;
extern u32 lbl_8047B078;
extern u32 lbl_8047B07C;

/* GSmem allocator */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif

/* External functions referenced from asm wrappers */
extern u32 sndAuxCallbackUpdateSettingsReverbHI(u8* ptr);

/* Model system */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmodelGetPart(void* model, u32 param);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSpartFree(void* model, u32 param);
#endif
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_800E24B0(void* model, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmemFree(void* model, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  set__5GSvecFfff(void* dst, void* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSvecCopy(void* dst, void* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSvecAdd(void* model, void* param);
#endif
extern void  fn_800E0BA0(void* param);
extern void  GSrandf(void* param);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  vecScale(void* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   __cvt_fp2unsigned(f64 val);
#endif

/* Floor/field system */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource(u16 group, u16 model, u16 param);
#endif

/* GX rendering */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmodelSetVisibility(void* param);
#endif

/* People data layer (people_data.c) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* itemDataBiosGetPtr(u16 index);   /* peopleFieldGetByIndex */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_80142CF4(u32 a, u32 b, u32 c, u32 d);  /* peopleFieldAlloc */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_801429E8(void* entry);  /* peopleFieldGetEntry */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_80142984(u32 id);       /* peopleFieldGetByID */
#endif

/* Script system */
extern void sndConvertMs(u32* ptr);  /* peopleFieldUtilDispatch - same-TU asm wrapper */

/* ===================================================================
 * All functions in this module are listed above in the MODULE MAP.
 * The asm files in build/GC6E01/asm/ remain the authoritative
 * implementation until each function is individually decompiled.
 * =================================================================== */

/* ===================================================================
 * MusyX runtime: seq.c (sequencer instance / playback control).
 * Reference: AxioDL/musyx `musyx/runtime/seq.c`, cross-verified against
 * byte-exact matched copies in Mario Party 4, Metroid Prime and Mario
 * Strikers (GC/1.3.2). SEQ_INSTANCE / SND_CROSSFADE layouts below are
 * copied field-for-field from Metroid Prime's musyx/seq.h and confirmed
 * byte-identical against this TU's own disassembly (defVGroup @0xEB0,
 * syncCrossInfo @0xEB4, trackVolGroup @0x324, noteUsed/noteKeyOff
 * @0xE64/0xE6C all match exactly). One proven version delta: PRG_STATE
 * here is the pre-2.0.1 4-byte variant (no `program` field) even though
 * this build otherwise sits close to XD 2.0.3 -- proven by the
 * prgState[16] gap arithmetic (0xEB0-0xE70 = 0x40 = 16*4). Struct
 * internals not touched by the functions implemented so far (track[],
 * pattern[], prgState[], event[], section[]) are kept as opaque padding
 * until a later pass needs them.
 * =================================================================== */

#include "musyx/runtime/seq_types.h" /* NOTE ... SEQ_INSTANCE */

extern SEQ_INSTANCE lbl_804285D0[8]; /* seqInstance */
extern NOTE* lbl_8047AF04;           /* noteFree */
extern SEQ_INSTANCE* lbl_8047AF08;   /* cseq (current sequence being processed) */
extern SEQ_INSTANCE* lbl_8047AF0C;   /* seqFreeRoot */
extern SEQ_INSTANCE* lbl_8047AF10;   /* seqPausedRoot */
extern SEQ_INSTANCE* lbl_8047AF14;   /* seqActiveRoot */
extern u32 lbl_8047AF00;             /* curSeqId */

extern void synthSetBpm(u32 bpm, u8 seqId, u8 secIndex);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 fmod(f64 a, f64 b); /* fmod (wraps __ieee754_fmod) */
#endif
extern f64 floor(f64 x);
extern SEQ_EVENT* HandleEvent(SEQ_EVENT* event, u8 secIndex, u32* loopFlag); /* HandleEvent */
extern u32 synthSendKeyOff(u32 id);
extern u32 synthIsFadeOutActive(u8 volGroup);
extern u32 sndFXCheck(u32 id); /* sndFXCheck */
extern u8 lbl_8047AEFC;         /* curFadeOutState */
extern u32 lbl_8047AEF8;        /* seq_next_id */
extern NOTE lbl_804271D0[256];  /* seqNote */
extern u16 lbl_80434910[8][16]; /* seqMIDIPriority */

extern void voiceKillSound(u32 id);
extern void synthVolume(u8 volume, u16 time, u8 volGroup, u8 mode, u32 pubId);

#define SND_SEQ_ERROR_ID 0xFFFFFFFFU
#define SND_SEQ_CROSSFADE_ID 0x80000000U
#define SND_CROSSFADE_SYNC 4
#define SND_CROSSFADE_PAUSENEW 8
#define SND_CROSSFADE_TRACKMUTE 16
#define SND_CROSSFADE_SPEED 32
#define SND_CROSSFADE_MUTE 64
#define SND_CROSSFADE_MUTENEW 128
#define SND_SEQVOL_CONTINUE 0
#define SND_SEQVOL_STOP 1
#define SND_SEQVOL_PAUSE 2
#define SND_SEQVOL_MUTE 3
#define SND_SEQVOL_MODEMASK 0xF

typedef struct {
    u8 program;
    u8 volume;
    u8 panning;
    u8 reverb;
    u8 chorus;
} MIDI_CHANNEL_SETUP;

typedef struct {
    u16 songId;
    u16 reserved;
    MIDI_CHANNEL_SETUP channel[16];
} MIDISETUP;

typedef struct {
    u8 track;
    u8 volGroup;
} SND_SEQVOLDEF;

typedef struct {
    u32 flags;
    u32 trackMute[2];
    u16 speed;
    struct {
        u16 time;
        u8 target;
    } volume;
    u8 numSeqVolDef;
    SND_SEQVOLDEF* seqVolDef;
    u8 numFaded;
    u8* faded;
} SND_PLAYPARA;

#define SND_PLAYPARA_TRACKMUTE 1
#define SND_PLAYPARA_SPEED 2
#define SND_PLAYPARA_VOLUME 4
#define SND_PLAYPARA_SEQVOLDEF 8
#define SND_PLAYPARA_PAUSE 0x10

extern u8 lbl_80435464[64]; /* synthTrackVolume */
extern void synthSetMusicVolumeType(u8 volumeGroup, u8 type);
extern void inpSetMidiCtrl(u8 ctrl, u8 midi, u8 midiSet, u8 value);
extern void inpResetMidiCtrl(u8 midi, u8 midiSet, u32 coldReset);
extern void inpResetChannelDefaults(u8 midi, u8 midiSet);
extern void InitTrackEvents(void);

void StartPause(SEQ_INSTANCE* si);

static inline void DoPrgChange(SEQ_INSTANCE* seq, u8 program, u8 midi)
{
    lbl_80434910[lbl_8047AF00][midi] = 0xFFFF;
    if (midi != 9) {
        program = seq->normTrans[program];
        if (program == 0xFF) {
            return;
        }
        seq->prgState[midi].macId = seq->normtab[program].macro;
        seq->prgState[midi].priority = seq->normtab[program].prio;
        seq->prgState[midi].maxVoices = seq->normtab[program].maxVoices;
        return;
    }
    program = seq->drumTrans[program];
    if (program == 0xFF) {
        return;
    }
    seq->prgState[midi].macId = seq->drumtab[program].macro;
    seq->prgState[midi].priority = seq->drumtab[program].prio;
    seq->prgState[midi].maxVoices = seq->drumtab[program].maxVoices;
}

static inline void BuildTransTab(u8* table, PAGE* page)
{
    u8 i;

    for (i = 0; i < 128; i++) {
        table[i] = 0xFF;
    }
    for (i = 0; page->index != 0xFF; i++, page++) {
        table[page->index] = i;
    }
}

static inline u32 GetPublicId(u32 seqId)
{
    u32 publicId;
    SEQ_INSTANCE* seq;

    do {
        publicId = lbl_8047AEF8++;
        lbl_8047AEF8 &= ~SND_SEQ_CROSSFADE_ID;
        for (seq = lbl_8047AF14; seq != NULL; seq = seq->next) {
            if (seq->publicId == publicId) {
                publicId = SND_SEQ_ERROR_ID;
                break;
            }
        }
        for (seq = lbl_8047AF10; seq != NULL; seq = seq->next) {
            if (seq->publicId == publicId) {
                publicId = SND_SEQ_ERROR_ID;
                break;
            }
        }
    } while (publicId == SND_SEQ_ERROR_ID);

    lbl_804285D0[seqId].publicId = publicId;
    return publicId;
}

#ifndef SEQ_801463C4_ONLY
u32 seqGetPrivateId(u32 seqId) {
    SEQ_INSTANCE* si;
    for (si = lbl_8047AF14; si != NULL; si = si->next) {
        if (si->publicId == (seqId & ~SND_SEQ_CROSSFADE_ID)) {
            return si->index | (seqId & SND_SEQ_CROSSFADE_ID);
        }
    }
    for (si = lbl_8047AF10; si != NULL; si = si->next) {
        if (si->publicId == (seqId & ~SND_SEQ_CROSSFADE_ID)) {
            return si->index | (seqId & SND_SEQ_CROSSFADE_ID);
        }
    }
    return SND_SEQ_ERROR_ID;
}
#endif

#ifdef TARGET_PC
/* PORT: song files are big-endian; AxioDL's PC song cache (hw_pc_song.c) returns a native-byte-order copy, as in
 * AxioDL's own seqStartPlay (docs/port/musyx-evaluation.md, "Songs") */
extern void* salPCAcquireSong(const void* input);
#endif
u32 seqStartPlay(PAGE* normal, PAGE* drum, MIDISETUP* midiSetup, u32* song,
                SND_PLAYPARA* para, u8 studio, u16 groupId)
{
    ARR* arr;
    u32* trackTable;
    s32 i;
    SEQ_INSTANCE* seq;
    SEQ_INSTANCE* oldSeq;
    u32 seqId;
    u32 bpm;

    if ((seq = lbl_8047AF0C) == NULL) {
        return SND_SEQ_ERROR_ID;
    }
#ifdef TARGET_PC
    song = (u32*)salPCAcquireSong(song);
    if (song == NULL) {
        return SND_SEQ_ERROR_ID;
    }
#endif
    if ((lbl_8047AF0C = seq->next) != NULL) {
        lbl_8047AF0C->prev = NULL;
    }
    if ((seq->next = lbl_8047AF14) != NULL) {
        lbl_8047AF14->prev = seq;
    }
    seq->prev = NULL;
    lbl_8047AF14 = seq;
    seq->state = 1;
    for (i = 0; i < 16; i++) {
        seq->section[i].globalEventRoot = NULL;
    }

    seqId = seq->index;
    seq->syncActive = 0;
    seq->normtab = normal;
    seq->drumtab = drum;
    seq->arrbase = (ARR*)song;
    seq->groupID = groupId;
    BuildTransTab(seq->normTrans, seq->normtab);
    BuildTransTab(seq->drumTrans, seq->drumtab);
    seq->defVGroup = seqId + 23;
    for (i = 0; i < 64; i++) {
        seq->trackVolGroup[i] = seq->defVGroup;
    }
    seq->defStudio = studio;

    if (para == NULL) {
        seq->trackMute[0] = -1;
        seq->trackMute[1] = -1;
        for (i = 0; i < 16; i++) {
            seq->section[i].speed = 256;
        }
        synthVolume(127, 0, seq->defVGroup, 0, SND_SEQ_ERROR_ID);
    } else {
        if ((para->flags & SND_PLAYPARA_TRACKMUTE) != 0) {
            seq->trackMute[0] = para->trackMute[0];
            seq->trackMute[1] = para->trackMute[1];
        } else {
            seq->trackMute[0] = -1;
            seq->trackMute[1] = -1;
        }
        if ((para->flags & SND_PLAYPARA_SPEED) != 0) {
            for (i = 0; i < 16; i++) {
                seq->section[i].speed = para->speed;
            }
        } else {
            for (i = 0; i < 16; i++) {
                seq->section[i].speed = 256;
            }
        }
        if ((para->flags & SND_PLAYPARA_SEQVOLDEF) != 0) {
            for (i = 0; i < para->numSeqVolDef; i++) {
                seq->trackVolGroup[para->seqVolDef[i].track] =
                    para->seqVolDef[i].volGroup;
                synthSetMusicVolumeType(para->seqVolDef[i].volGroup, 0);
            }
        }
        if ((para->flags & SND_PLAYPARA_VOLUME) != 0) {
            synthVolume(para->volume.target, para->volume.time,
                        seq->defVGroup, 0, SND_SEQ_ERROR_ID);
            for (i = 0; i < para->numFaded; i++) {
                synthVolume(para->volume.target, para->volume.time,
                            para->faded[i], 0, SND_SEQ_ERROR_ID);
            }
        }
    }

    arr = (ARR*)song;
    if ((arr->info & 0x80000000) != 0) {
        seq->trackSectionTab = ARR_GET(arr, arr->tsTab);
    } else {
        seq->trackSectionTab = NULL;
    }
    bpm = arr->info & 0x0FFFFFFF;
    if ((arr->info & 0x40000000) == 0) {
        bpm <<= 10;
    }
    for (i = 0; i < 16; i++) {
        seq->section[i].bpm = bpm;
        synthSetBpm(bpm >> 10, seqId, i);
        if (arr->mTrack != 0) {
            seq->section[i].mTrack.base = ARR_GET(arr, arr->mTrack);
            seq->section[i].mTrack.addr = seq->section[i].mTrack.base;
        } else {
            seq->section[i].mTrack.base = NULL;
        }
        seq->section[i].loopDisable = 0;
        seq->section[i].loopCnt = 0;
    }

    trackTable = ARR_GET(arr, arr->tTab);
    for (i = 0; i < 64; i++) {
        lbl_80435464[i] = 127;
        seq->pattern[i].addr = NULL;
        if (trackTable[i] != 0) {
            seq->track[i].addr = seq->track[i].base =
                ARR_GET(arr, trackTable[i]);
        } else {
            seq->track[i].addr = seq->track[i].base = NULL;
        }
    }
    seq->noteUsed[0] = NULL;
    seq->noteUsed[1] = NULL;
    seq->noteKeyOff = NULL;
    for (i = 0; i < 16; i++) {
        inpResetMidiCtrl(i, seqId, 1);
    }
    for (i = 0; i < 16; i++) {
        seq->prgState[i].macId = 0xFFFF;
    }
    for (i = 0; i < 16; i++) {
        inpResetChannelDefaults(i, seqId);
    }
    if (midiSetup != NULL) {
        for (i = 0; i < 16; i++) {
            DoPrgChange(seq, midiSetup->channel[i].program, i);
            inpSetMidiCtrl(7, i, seqId, midiSetup->channel[i].volume);
            inpSetMidiCtrl(10, i, seqId, midiSetup->channel[i].panning);
            inpSetMidiCtrl(91, i, seqId, midiSetup->channel[i].reverb);
            inpSetMidiCtrl(93, i, seqId, midiSetup->channel[i].chorus);
        }
    }
    for (i = 0; i < 16; i++) {
        lbl_80434910[seqId][i] = 0xFFFF;
    }
    for (i = 0; i < 16; i++) {
        seq->section[i].time[0].high = 0;
        seq->section[i].time[0].low = 0;
        seq->section[i].time[1].high = 0;
        seq->section[i].time[1].low = 0;
        seq->section[i].timeIndex = 0;
    }
    seq->keyOffCheck = 0;
    if (para != NULL && (para->flags & SND_PLAYPARA_PAUSE) != 0) {
        StartPause(seq);
    }
    oldSeq = lbl_8047AF08;
    lbl_8047AF08 = seq;
    InitTrackEvents();
    lbl_8047AF08 = oldSeq;
    seqId = GetPublicId(seqId);
    return seqId;
}

#ifndef SEQ_801463C4_ONLY
static void KillNotes(SEQ_INSTANCE* seq) {
    NOTE* n;
    u32 i;

    for (i = 0; i < 2; i++) {
        for (n = seq->noteUsed[i]; n != NULL; n = n->next) {
            voiceKillSound(n->id);
        }
    }

    for (n = seq->noteKeyOff; n != NULL; n = n->next) {
        voiceKillSound(n->id);
    }
}

static void ResetNotes(SEQ_INSTANCE* seq) {
    NOTE* n;
    u32 i;

    for (i = 0; i < 2; i++) {
        n = seq->noteUsed[i];
        if (n != NULL) {
            for (; n->next != NULL; n = n->next) {
            }

            if (lbl_8047AF04 != NULL) {
                n->next = lbl_8047AF04;
                lbl_8047AF04->prev = n;
            }

            lbl_8047AF04 = seq->noteUsed[i];
            seq->noteUsed[i] = NULL;
        }
    }

    n = seq->noteKeyOff;
    if (n != NULL) {
        for (; n->next != NULL; n = n->next) {
        }

        if (lbl_8047AF04 != NULL) {
            n->next = lbl_8047AF04;
            lbl_8047AF04->prev = n;
        }

        lbl_8047AF04 = seq->noteKeyOff;
        seq->noteKeyOff = NULL;
    }
}
#endif
