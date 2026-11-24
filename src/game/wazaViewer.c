/**
 * @file wazaViewer.c
 * @brief wazaViewer: move-animation viewer thread/init/update/finalize helpers.
 *
 * Split from the former game/battle/battle_waza.c CodeCandidate bucket
 * (0x801D1470-0x801DE698); see config/GC6E01/splits.txt for the exact
 * address range of this translation unit. Shared typedefs and cross-TU
 * forward declarations live in include/game/battle/battle_waza_types.h.
 */

#include "game/battle/battle_waza_types.h"
#include "pc/disc_types.h" /* DISC_SLOT: scene_data lists are disc slots (floor-resources.md) */
#include "dolphin/pad/Pad.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_WazaViewerThread / _Draw: Studio's reference camera (ORRE_WAZA_VIEWER); all platforms */
#endif

struct GStextureHandle;
extern void GStextureFree(struct GStextureHandle* texture);
extern void GSgappTerminate(u32 taskId);
extern void GSvecCopy(void* dst, void* src);

/** Bare 3-float vector, as passed to the GSscene camera setters. */
typedef struct WazaViewerVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
} WazaViewerVec;

/**
 * One entry of the viewer's camera-preset table (rodata lbl_80279338, stride
 * 0x24). Entry 0 is the rotation preset; entries 1 and 2 are the two identical
 * "look at the model from the front" presets used by the 2-player and
 * single-player paths.
 */
typedef struct WazaViewerCamPreset {
    /* 0x00 */ WazaViewerVec direction;
    /* 0x0C */ WazaViewerVec position;
    /* 0x18 */ WazaViewerVec view;
} WazaViewerCamPreset;

/**
 * Byte 0x8 of the viewer work area: the two playback control fields the
 * viewer's per-frame callback tests. Both are read for their sign only.
 */
typedef struct WazaViewerCtrl {
    /* 0x80 */ s8 advance : 1;
    /* 0x40 */ s8 paused : 1;
    /* 0x20 */ s8 overlay : 1;
    /* 0x10 */ s8 bounds : 1;
    /* 0x0F */ s8 unused_0F : 4;
} WazaViewerCtrl;

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_5328_53D4)

/**
 * _wazaViewerFinalize - Move animation helper: screen flash.
 * Address: 0x801D5328 | Size: 0xAC
 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void _wazaViewerFinalize(u8 r, u8 g, u8 b, f32 duration) {
    extern u32 lbl_8047B3F8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801024E8(s32);
#endif
    extern void GSfilterRelease(u32);
    extern void vtrDeactivate(void);

    GSthreadTerminateGroup(0x58);
    *(u32*)(lbl_804673F8 + 0x66C) = 0;
    fn_801024E8(0);
    *(u32*)(lbl_804673F8 + 0x0C) = 0;
    GSfilterRelease(*(u32*)(lbl_804673F8 + 0x10));
    if (*(void**)(lbl_804673F8 + 0x878) != NULL) {
        GStextureFree(*(void**)(lbl_804673F8 + 0x878));
    }
    memset(lbl_804673F8, 0, 0x884);
    if (lbl_804673F8[0x87D] == 0) {
        vtrDeactivate();
    }
    GSgappTerminate(PC_PTR32_IMPLICIT((void*)PC_PTR(lbl_8047B3F8)));
    lbl_8047B3F8 = 0;
    wazaSequenceSysRelease();
}
#else
void _wazaViewerFinalize(u8 r, u8 g, u8 b, f32 duration) {
    extern u32 lbl_8047B3F8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801024E8(s32);
#endif
    extern void GSfilterRelease(u32);
    extern void vtrDeactivate(void);

    GSthreadTerminateGroup(0x58);
    *(u32*)(lbl_804673F8 + 0x66C) = 0;
    fn_801024E8(0);
    *(u32*)(lbl_804673F8 + 0x0C) = 0;
    GSfilterRelease(*(u32*)(lbl_804673F8 + 0x10));
    if (*(void**)(lbl_804673F8 + 0x878) != NULL) {
        GStextureFree(*(void**)(lbl_804673F8 + 0x878));
    }
    memset(lbl_804673F8, 0, 0x884);
    if (lbl_804673F8[0x87D] == 0) {
        vtrDeactivate();
    }
    GSgappTerminate(lbl_8047B3F8);
    lbl_8047B3F8 = 0;
    wazaSequenceSysRelease();
}
#endif

#endif

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_53D4_53D8)

/**
 * _wazaViewerUpdate - Move animation no-op.
 * Address: 0x801D53D4 | Size: 0x4
 */
void _wazaViewerUpdate(void) {
    /* No-op */
}


#endif

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_53D8_7E58) || defined(PR409_WAZA_VIEWER_EXACT_56B0_5A94) || defined(PR409_WAZA_VIEWER_EXACT_5DA0) || defined(PR409_WAZA_VIEWER_EXACT_624C) || defined(PR409_WAZA_VIEWER_EXACT_6E58) || defined(PR409_WAZA_VIEWER_EXACT_7464)

extern u8 lbl_80279338[];

/* Host-file (DVD) wrappers: open returns the file object, 0 on failure. */
extern void* GSdvdOpen(const char* path);
extern u32 GSdvdGetLength(void* file);
extern void GSdvdClose(void* file);
extern s32 GSdvdRead(void* file, void* buf, u32 size, s32 offset);
extern s32 fn_80167E54(void* file, void* buf, u32 size, s32 offset);

#if !defined(PR409_WAZA_VIEWER_EXACT_56B0_5A94)
/**
 * wazaViewerGetFilesize - size of a host file, or 0 if it cannot be opened.
 * Named by the diagnostic it prints. Inlined into every caller by -inline auto,
 * so it has no symbol of its own in the shipped image.
 */
static inline u32 wazaViewerGetFilesize(const char* messages, const char* path) {
    u32 size;
    void* file;

    file = GSdvdOpen(path);
    if (file == NULL) {
        GSlogWrite(messages + 0x6C, path);
        return 0;
    }
    size = GSdvdGetLength(file);
    GSdvdClose(file);
    return size;
}

/**
 * wazaViewerLoadFile - read a host file into buf, or write buf back out.
 * All three of this function's diagnostics name it "wazaViewerLoadFile()",
 * including the write one, so retail had a single helper with a direction
 * flag; constant propagation at each inline site keeps only one arm.
 */
static inline void wazaViewerLoadFile(const char* messages, const char* path, void* buf,
                                      u32 size, int writing) {
    void* file;

    file = GSdvdOpen(path);
    if (writing) {
        if (file != NULL) {
            if (fn_80167E54(file, buf, size, 0) < 0) {
                GSlogWrite(messages + 0xF8, path);
            }
            GSdvdClose(file);
        } else {
            GSlogWrite(messages + 0x9C, path);
        }
        return;
    }
    if (file == NULL) {
        GSlogWrite(messages + 0x9C, path);
        return;
    }
    if (GSdvdRead(file, buf, (size + 0x1F) & ~0x1F, 0) < 0) {
        GSlogWrite(messages + 0xC8, path);
        GSdvdClose(file);
        return;
    }
    GSdvdClose(file);
}

/**
 * wazaViewerPollCommand - once a second, poll the host file wazaViewer.wvd;
 * when it carries command 1, copy the three 0x200-byte name buffers out of
 * it, re-arm the viewer, and write the record back with the command cleared.
 * The same expansion appears in wazaViewerThread and wazaViewerUpdatePlayMode.
 */
static inline void wazaViewerPollCommand(const char* messages) {
    extern u16 GSmemAlloc(u32 size, u32 align);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16 block);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E24B0(u16 block);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmemFree(u16 block);
#endif

    void* readFile;
    char* path;
    void* sizeFile;
    u32 size;
    u16 block;
    void* record;
    void* writeFile;

    if (*(s32*)(lbl_804673F8 + 0x870) <= 0) {
        path = (char*)(lbl_804673F8 + 0x670);
        sizeFile = GSdvdOpen(path);
        if (sizeFile == NULL) {
            GSlogWrite(messages + 0x6C, path);
            size = 0;
        } else {
            size = GSdvdGetLength(sizeFile);
            GSdvdClose(sizeFile);
        }
        if (size != 0) {
            block = GSmemAlloc((size + 0x1F) & ~0x1F, 0x20);
            if (block != 0) {
                record = GSmemLock(block);
                readFile = GSdvdOpen(path);
                if (readFile == NULL) {
                    GSlogWrite(messages + 0x9C, path);
                } else {
                    if (GSdvdRead(readFile, record, (size + 0x1F) & ~0x1F, 0) < 0) {
                        GSlogWrite(messages + 0xC8, path);
                        GSdvdClose(readFile);
                    } else {
                        GSdvdClose(readFile);
                    }
                }
                if (((s32*)record)[0] == 1) {
                    memcpy(lbl_804673F8 + 0x1C, (u8*)record + 0x208, 0x200);
                    memcpy(lbl_804673F8 + 0x21C, (u8*)record + 0x408, 0x200);
                    memcpy(lbl_804673F8 + 0x41C, (u8*)record + 0x008, 0x200);
                    lbl_804673F8[0x87C] = ((s32*)record)[0x608 / 4];
                    ((s32*)record)[1] = 0;
                    ((s32*)record)[0] = 0;
                    wazaViewerResetAndLoad();
                    writeFile = GSdvdOpen(path);
                    if (writeFile != NULL) {
                        if (fn_80167E54(writeFile, record, size, 0) < 0) {
                            GSlogWrite(messages + 0xF8, path);
                        }
                        GSdvdClose(writeFile);
                    } else {
                        GSlogWrite(messages + 0x9C, path);
                    }
                }
                fn_800E24B0(block);
                GSmemFree(block);
            }
        }
        *(s32*)(lbl_804673F8 + 0x870) = 0x3C;
    } else {
        *(s32*)(lbl_804673F8 + 0x870) -= GSgetFrameStep();
    }
}

/**
 * _wazaViewerInitialize - Move animation helper: camera zoom.
 * Address: 0x801D53D8 | Size: 0x8C
 */
#endif

#if !defined(PR409_WAZA_VIEWER_EXACT_56B0_5A94) && !defined(PR409_WAZA_VIEWER_EXACT_5DA0) && !defined(PR409_WAZA_VIEWER_EXACT_624C) && !defined(PR409_WAZA_VIEWER_EXACT_6E58) && !defined(PR409_WAZA_VIEWER_EXACT_7464) && !defined(PR409_WAZA_VIEWER_FN_ONLY)
#if !defined(PR409_WAZA_VIEWER_THREAD_ONLY)
void _wazaViewerInitialize(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern struct GSmodel* GSresGetResource(u32 group, u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GSthreadCreate(s32, s32, s32, s32, s32, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GSgetVersionMinor(void);
#endif

    struct GSmodel* result;
    s32 value;

    wazaSequenceSysInit(8);
    wazaViewerInit();
    result = GSresGetResource(0, 0x64);
    if (result != 0) {
        GSmodelSetVisibility(result, 0);
    }
    *(s32*)(lbl_804673F8 + 0x66C) = PC_PTR32_IMPLICIT(GSthreadCreate(0x14, 0x58, 0x2000, 1, 0, wazaViewerThread));
    value = GSgetVersionMinor();
    *(u32*)lbl_804673F8 = (u32)((u32)((2 - value) | (value - 2)) >> 31);
}
#endif

/**
 * wazaViewerThread - The viewer's own GSthread body. Once a second it polls
 * the host filesystem for wazaViewer.wvd; when the file carries command 1 it
 * copies the three 0x200-byte name buffers out of it, re-arms the viewer, and
 * writes the record back with the command cleared.
 * Address: 0x801D5464 | Size: 0x24C
 */
void wazaViewerThread(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraUpdate(void);
#endif

    const char* messages;
    void* sequence;

    messages = (const char*)lbl_80279338;

    while (TRUE) {
        wazaSequenceSysUpdate();
        switch (*(s32*)(lbl_804673F8 + 0x00)) {
        case 1:
            wazaViewerUpdateModels();
            break;
        case 0:
            sequence = *(void**)(lbl_804673F8 + 0x61C + *(s32*)(lbl_804673F8 + 0x624) * 4);
            if (sequence != NULL) {
                if ((u8)wazaSequenceSysIsPlayingWaza(sequence, 0, 0)) {
                    *(s32*)(lbl_804673F8 + 0x870) = 0;
                    break;
                }
            }
            wazaViewerPollCommand(messages);
            break;
        }
#ifdef TARGET_PC
        /* OrreHook_WazaViewerThread (docs/port/hook-points.md): why: retail's viewer (mode 1) took its Pokémon from
         * the developers' debug menu, which wrote the work area's command fields; that menu table is gone, so a PC
         * debug switch fills them here instead, and sets the camera once the model is spawned (before cameraUpdate
         * applies it this pass). Nothing registered: nothing. All platforms */
        if (OrreHook_WazaViewerThread != NULL) {
            OrreHook_WazaViewerThread(lbl_804673F8);
        }
#endif
        cameraUpdate();
        _threadSwitch();
    }
}

#endif

#if !defined(PR409_WAZA_VIEWER_THREAD_ONLY) && !defined(PR409_WAZA_VIEWER_EXACT_5DA0) && !defined(PR409_WAZA_VIEWER_EXACT_624C) && !defined(PR409_WAZA_VIEWER_EXACT_6E58) && !defined(PR409_WAZA_VIEWER_EXACT_7464) && !defined(PR409_WAZA_VIEWER_FN_ONLY)

/**
 * wazaViewerResetAndLoad - Re-arm the viewer after a sequence file has been loaded:
 * reset the camera, drop the previous sequence, and re-read the new one (and the colour file, if named).
 * Address: 0x801D56B0 | Size: 0x234
 */
/* NAME-T3 fn_801D56B0 */
void wazaViewerResetAndLoad(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraDirectionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraPositionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraViewVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetMode(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GSgetVersionMinor(void);
#endif
    extern void GSmodelDisableColorSwap(void* model);
    extern void DVDInit(void);
    extern void wazaSequenceOwnerFree(void* sequence);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmemFree(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 strlen(const char* s);
#endif

    const WazaViewerCamPreset* presets;
    WazaViewerVec direction;
    WazaViewerVec position;
    WazaViewerVec view;
    void* sequence;

    presets = (const WazaViewerCamPreset*)lbl_80279338;
    if (GSgetVersionMinor() == 2) {
        direction = presets[2].direction;
        position = presets[2].position;
        view = presets[2].view;
        GSscene_SetCameraDirectionVector(&direction);
        GSscene_SetCameraPositionVector(&position);
        GSscene_SetCameraViewVector(&view);
        GSscene_SetMode(2);
        *(s32*)(lbl_804673F8 + 0x00) = 0;
    } else {
        GSscene_SetMode(7);
        *(s32*)(lbl_804673F8 + 0x00) = 1;
    }

    memset(lbl_804673F8 + 0x08, 0, 4);
    *(s32*)(lbl_804673F8 + 0x14) = 0;
    *(s32*)(lbl_804673F8 + 0x18) = -1;
    sequence = *(void**)(lbl_804673F8 + 0x61C + *(s32*)(lbl_804673F8 + 0x624) * 4);
    if (sequence != NULL) {
        sequence = PTR32_AT_T(void, (u8*)sequence, 0x24);
        if (sequence != NULL) {
            GSmodelDisableColorSwap(sequence);
        }
    }
    *(s32*)(lbl_804673F8 + 0x65C) = 0;
    *(s32*)(lbl_804673F8 + 0x664) = 0;
    *(s32*)(lbl_804673F8 + 0x0C) = 0;
    DVDInit();
    *(s32*)(lbl_804673F8 + 0x63C) = 0;
    switch (*(s32*)(lbl_804673F8 + 0x00)) {
    case 0:
        wazaSequenceOwnerFree(*(void**)(lbl_804673F8 + 0x61C));
        if (*(u16*)(lbl_804673F8 + 0x660) != 0) {
            fn_800E24B0(*(u16*)(lbl_804673F8 + 0x660));
            GSmemFree(*(u16*)(lbl_804673F8 + 0x660));
            *(u16*)(lbl_804673F8 + 0x660) = 0;
            if (*(u16*)(lbl_804673F8 + 0x668) != 0) {
                fn_800E24B0(*(u16*)(lbl_804673F8 + 0x668));
                GSmemFree(*(u16*)(lbl_804673F8 + 0x668));
                *(u16*)(lbl_804673F8 + 0x668) = 0;
            }
        }
        memset(lbl_804673F8 + 0x61C, 0, 8);
        wazaViewerLoadSequenceFiles(0);
        break;
    case 1:
        memset(lbl_804673F8 + 0x61C, 0, 8);
        break;
    }

    if (strlen((char*)(lbl_804673F8 + 0x21C)) != 0) {
        wazaViewerApplyColorFile();
    }
    if (GSgetVersionMinor() == 2) {
        GSscene_SetMode(2);
    } else {
        GSscene_SetMode(7);
    }
}

/**
 * wazaViewerInit - Reset the viewer work area and (re)create its GS resources: the camera preset, a black screen
 * filter, the battle grid, and the GS app whose per-frame callback is wazaViewerUpdate. Called by
 * _wazaViewerInitialize.
 * Address: 0x801D58E4 | Size: 0x1B0
 */
/* NAME-T3 fn_801D58E4 */
void wazaViewerInit(void) {
    extern u32 lbl_8047B3F8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraDirectionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraPositionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraViewVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetMode(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GSgetVersionMinor(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSfilterCreate(u8* color);
#endif
    extern u8 vtrIsActive(void);
    extern void vtrActivate(void);
    extern void battleGridInit(void);
    extern u32 GSgappCreate(s32 group, s32 priority, s32 flags, void* callback);

    const WazaViewerCamPreset* presets;
    WazaViewerVec direction;
    WazaViewerVec position;
    WazaViewerVec view;
    u8 color[4];

    presets = (const WazaViewerCamPreset*)lbl_80279338;
    if (GSgetVersionMinor() == 2) {
        direction = presets[1].direction;
        position = presets[1].position;
        view = presets[1].view;
        GSscene_SetCameraDirectionVector(&direction);
        GSscene_SetCameraPositionVector(&position);
        GSscene_SetCameraViewVector(&view);
        GSscene_SetMode(2);
        *(s32*)(lbl_804673F8 + 0x00) = 0;
    } else {
        GSscene_SetMode(7);
        *(s32*)(lbl_804673F8 + 0x00) = 1;
    }

    *(s32*)(lbl_804673F8 + 0x04) = 0;
    memset(lbl_804673F8 + 0x08, 0, 4);
    *(s32*)(lbl_804673F8 + 0x14) = 0;
    *(s32*)(lbl_804673F8 + 0x18) = -1;
    color[3] = 0;
    color[2] = 0;
    color[1] = 0;
    color[0] = 0;
    *(void**)(lbl_804673F8 + 0x10) = GSfilterCreate(color);
    *(s32*)(lbl_804673F8 + 0x65C) = 0;
    *(s32*)(lbl_804673F8 + 0x664) = 0;
    *(u16*)(lbl_804673F8 + 0x660) = 0;
    *(u16*)(lbl_804673F8 + 0x668) = 0;
    memset(lbl_804673F8 + 0x61C, 0, 8);
    *(s32*)(lbl_804673F8 + 0x624) = 0;
    *(s32*)(lbl_804673F8 + 0x870) = 0;
    *(s32*)(lbl_804673F8 + 0x0C) = 0;
    *(s32*)(lbl_804673F8 + 0x880) = 0;
    *(s32*)(lbl_804673F8 + 0x878) = 0;
    *(s32*)(lbl_804673F8 + 0x874) = 1;
    lbl_804673F8[0x87D] = vtrIsActive();
    vtrActivate();
    battleGridInit();
    lbl_8047B3F8 = GSgappCreate(1, 0x7F, 0, wazaViewerUpdate);
    *(s32*)(lbl_804673F8 + 0x63C) = 0;
}

#endif

#if !defined(PR409_WAZA_VIEWER_THREAD_ONLY)
#if !defined(PR409_WAZA_VIEWER_EXACT_56B0_5A94)
#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_5A94_PREFIX)
/**
 * wazaViewerLoadSequenceFiles - load the viewer's waza sequence file (+0x1C)
 * and optional second file (+0x41C) from the host into a new effect, and
 * store it in viewer slot `slot`.
 * Address: 0x801D5A94 | Size: 0x30C
 */
void wazaViewerLoadSequenceFiles(s32 slot) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 strlen(const char* s);
#endif
    extern u16 GSmemAlloc(u32 size, u32 align);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16 block);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetRotation(void* model, WazaViewerVec* rotation);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetPosition(void* model, WazaViewerVec* position);
#endif

    void* modelBuf;
    const u8* viewerData;
    s32 modelSize;
    s32 animSize;
    void* effect;
    u16 block;
    u16 modelBlock;
    void* buf;
    void* animBuf;
    WazaViewerVec rotation;
    WazaViewerVec position;

    viewerData = lbl_80279338;
    modelSize = 0;
    animSize = 0;
    if (strlen((char*)(lbl_804673F8 + 0x1C)) != 0) {
        modelSize = wazaViewerGetFilesize((const char*)viewerData, (char*)(lbl_804673F8 + 0x1C));
        if (strlen((char*)(lbl_804673F8 + 0x41C)) != 0) {
            animSize = wazaViewerGetFilesize((const char*)viewerData, (char*)(lbl_804673F8 + 0x41C));
        }
    }

    effect = wazaSequenceSysAllocOwner();
    if (effect == NULL) {
        return;
    }
    if (modelSize == 0) {
        return;
    }
    modelBlock = GSmemAlloc((modelSize + 0x1F) & ~0x1F, 0x20);
    if (modelBlock == 0) {
        return;
    }
    *(u16*)(lbl_804673F8 + 0x660) = modelBlock;
    buf = *(void**)(lbl_804673F8 + 0x65C) = GSmemLock(modelBlock);
    wazaViewerLoadFile((const char*)viewerData, (char*)(lbl_804673F8 + 0x1C), buf, modelSize, FALSE);
    sequenceLoad(effect, *(void**)(lbl_804673F8 + 0x65C));
    *(s32*)(lbl_804673F8 + 0x04) = 1;
    wazaSequenceOwnerSetVisibility(effect, 1);
    rotation = *(WazaViewerVec*)(viewerData + 0x0C);
    position = *(WazaViewerVec*)(viewerData + 0x18);
    position.x = battleGridGetDistance(0);
    GSmodelSetRotation(PTR32_AT_T(void, (u8*)effect, 0x24), &rotation);
    GSmodelSetPosition(PTR32_AT_T(void, (u8*)effect, 0x24), &position);
    *(s8*)((u8*)effect + 0x76) = -1;
    if (animSize != 0) {
        block = GSmemAlloc((animSize + 0x1F) & ~0x1F, 0x20);
        if (block != 0) {
            *(u16*)(lbl_804673F8 + 0x668) = block;
            /* RULE-EXCEPTION(user-approved): pointer carrier for the allocator result, chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
            animBuf = GSmemLock(block);
            buf = *(void**)(lbl_804673F8 + 0x664) = animBuf;
            wazaViewerLoadFile((const char*)viewerData, (char*)(lbl_804673F8 + 0x41C), buf, animSize, FALSE);
            if ((u8)wazaSequenceOwnerAddSequence(effect, *(void**)(lbl_804673F8 + 0x664))) {
                *(s32*)(lbl_804673F8 + 0x04) = 2;
            } else {
                wazaSequenceSysStopWaza(effect, 0, 0);
            }
        }
    }
    ((void**)(lbl_804673F8 + 0x61C))[slot] = effect;
}

#endif

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_EXACT_5DA0)
/**
 * wazaViewerApplyColorFile - Read the colour file named at work +0x21C from the host (four colour-swap channel
 * indices and a modulation colour) and apply it to the current model: the viewer's preview of a recoloured
 * (shiny-style) model.
 * Address: 0x801D5DA0 | Size: 0x29C
 */
/* NAME-T3 fn_801D5DA0 */
void wazaViewerApplyColorFile(void) {
    extern u16 GSmemAlloc(u32 size, u32 align);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16 block);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E24B0(u16 block);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmemFree(u16 block);
#endif
    extern void GSmodelEnableColorSwap(void* model, s32 a, s32 b, s32 c, s32 d);
    extern void GSmodelEnableModulation(void* model, u8* color);

    const char* messages;
    char* path;
    void* sequence;
    s32 size;
    u16 block;
    s32* record;
    s32 a;
    s32 b;
    s32 c;
    s32 d;
    s32 value;
    u8 color[4];

    messages = (const char*)lbl_80279338;
    sequence = *(void**)(lbl_804673F8 + 0x61C + *(s32*)(lbl_804673F8 + 0x624) * 4);
    if (sequence == NULL) {
        return;
    }
    if (PTR32_AT_T(void, (u8*)sequence, 0x24) == NULL) {
        return;
    }

    path = (char*)(lbl_804673F8 + 0x21C);
    size = wazaViewerGetFilesize(messages, path);
    if (size == 0) {
        return;
    }

    block = GSmemAlloc((size + 0x1F) & ~0x1F, 0x20);
    if (block == 0) {
        return;
    }
    record = (s32*)GSmemLock(block);
    wazaViewerLoadFile(messages, path, record, size, FALSE);

    switch (record[0]) {
    case 0:
        a = 0;
        break;
    case 1:
        a = 1;
        break;
    case 2:
        a = 2;
        break;
    case 3:
        a = 3;
        break;
    }
    switch (record[1]) {
    case 0:
        b = 0;
        break;
    case 1:
        b = 1;
        break;
    case 2:
        b = 2;
        break;
    case 3:
        b = 3;
        break;
    }
    switch (record[2]) {
    case 0:
        c = 0;
        break;
    case 1:
        c = 1;
        break;
    case 2:
        c = 2;
        break;
    case 3:
        c = 3;
        break;
    }
    switch (record[3]) {
    case 0:
        d = 0;
        break;
    case 1:
        d = 1;
        break;
    case 2:
        d = 2;
        break;
    case 3:
        d = 3;
        break;
    }
    value = record[4];
    fn_800E24B0(block);
    GSmemFree(block);
    color[3] = value >> 24;
    color[2] = (value >> 16) & 0xFF;
    color[1] = (value >> 8) & 0xFF;
    color[0] = value & 0xFF;
    GSmodelEnableColorSwap(PTR32_AT_T(void, (u8*)sequence, 0x24), a, b, c, d);
    GSmodelEnableModulation(PTR32_AT_T(void, (u8*)sequence, 0x24), color);
}

#endif

#if !defined(PR409_WAZA_VIEWER_EXACT_5DA0)
#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_603C_ONLY)
/**
 * wazaViewerUpdate - The viewer's per-frame GS app callback (created by wazaViewerInit). Mode 1: draw the
 * attachment points, handle the pad (wazaViewerUpdateInput), print the error lines and the STEP / SLOW MOTION /
 * PAUSED / PLAYING status. Mode 0: wazaViewerUpdatePlayMode.
 * Address: 0x801D603C | Size: 0x210
 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
/* NAME-T3 fn_801D603C */
void wazaViewerUpdate(void) {
    extern u8 lbl_80279470[];  /* "SLOW MOTION" */
    extern f32 lbl_8047E2F0;
    extern const char lbl_8047E2F4;  /* "STEP" */
    extern const char lbl_8047E2FC;  /* "PAUSED" */
    extern const char lbl_8047E304;  /* "PLAYING" */
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E30C;
#endif
    extern void GSfilterSetColor(void* filter, u8* color);
    extern u8 dbgMenuIsOpen(void);
    extern s32 vtrGetMode(void);
    extern void fn_800FE6A0(f32 width, f32 height);
    extern void GSmsgPrintf(s32 x, s32 y, u32 color, const char* fmt, ...);

    void** models;
    void* sequence;
    void* node;
    s32 state;
    s32 arg1;
    s32 arg2;
    u8 color[4];

    switch (*(s32*)(lbl_804673F8 + 0x00)) {
    case 1:
        models = (void**)(lbl_804673F8 + 0x61C);
        sequence = models[*(s32*)(lbl_804673F8 + 0x624)];
        color[3] = 0;
        color[2] = 0;
        color[1] = 0;
        color[0] = 0;
        GSfilterSetColor(*(void**)(lbl_804673F8 + 0x10), color);
        if (sequence != NULL) {
            if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->advance) {
                wazaViewerDrawAttachPoints(sequence);
            }
            /* OrreHook_WazaViewerDraw (docs/port/hook-points.md): why: the viewer's flat backdrop is drawn only in the
             * developer build's mode 0; Studio's reference shots want it in retail's mode 1 too, in the same place in
             * the frame (after the dots, as mode 0 orders them). Nothing registered: nothing. All platforms */
            if (OrreHook_WazaViewerDraw != NULL) {
                OrreHook_WazaViewerDraw(sequence);
            }
            *(s32*)(lbl_804673F8 + 0x14) = *(u16*)((u8*)sequence + 0x32);
            if (!dbgMenuIsOpen() && !menuIsCheck(0x78)) {
                wazaViewerUpdateInput();
            }
        }
        wazaViewerDrawErrors();
        state = vtrGetMode();
        arg2 = arg1 = 0;
        if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->overlay) {
            sequence = models[*(s32*)(lbl_804673F8 + 0x624)];
            if (sequence != NULL) {
                node = PTR32_AT_T(void, (u8*)sequence, 0x68);
                if (node != NULL) {
                    arg1 = *(u16*)((u8*)node + 0x2C);
                    arg2 = *(u16*)((u8*)node + 0x2E);
                }
                if ((u8)wazaSequenceSysIsPlayingWaza(sequence, arg1, arg2)) {
                    fn_800FE6A0(lbl_8047E2F0, lbl_8047E2F0);
                    switch (state) {
                    case 3:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2F4);
                        break;
                    case 4:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, (const char*)lbl_80279470);
                        break;
                    case 1:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2FC);
                        break;
                    case 2:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E304);
                        break;
                    }
                    fn_800FE6A0(lbl_8047E30C, lbl_8047E30C);
                }
            }
        }
        break;
    case 0:
        wazaViewerUpdatePlayMode();
        break;
    }
}
#else
/* NAME-T3 fn_801D603C */
void wazaViewerUpdate(void) {
    extern u8 lbl_80279470[];  /* "SLOW MOTION" */
    extern f32 lbl_8047E2F0;
    extern const char lbl_8047E2F4;  /* "STEP" */
    extern const char lbl_8047E2FC;  /* "PAUSED" */
    extern const char lbl_8047E304;  /* "PLAYING" */
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E30C;
#endif
    extern void GSfilterSetColor(void* filter, u8* color);
    extern u8 dbgMenuIsOpen(void);
    extern s32 vtrGetMode(void);
    extern void fn_800FE6A0(f32 width, f32 height);
    extern void GSmsgPrintf(s32 x, s32 y, u32 color, const char* fmt, ...);

    void** models;
    void* sequence;
    void* node;
    void* current;
    s32 state;
    s32 arg1;
    s32 arg2;
    u8 color[4];

    switch (*(s32*)(lbl_804673F8 + 0x00)) {
    case 1:
        models = (void**)(lbl_804673F8 + 0x61C);
        sequence = models[*(s32*)(lbl_804673F8 + 0x624)];
        color[3] = 0;
        color[2] = 0;
        color[1] = 0;
        color[0] = 0;
        GSfilterSetColor(*(void**)(lbl_804673F8 + 0x10), color);
        if (sequence != NULL) {
            if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->advance) {
                wazaViewerDrawAttachPoints(sequence);
            }
            *(s32*)(lbl_804673F8 + 0x14) = *(u16*)((u8*)sequence + 0x32);
            if (!dbgMenuIsOpen() && !menuIsCheck(0x78)) {
                wazaViewerUpdateInput();
            }
        }
        wazaViewerDrawErrors();
        state = vtrGetMode();
        arg2 = arg1 = 0;
        if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->overlay) {
            current = models[*(s32*)(lbl_804673F8 + 0x624)];
            if (current != NULL) {
                node = *(void**)((u8*)current + 0x68);
                if (node != NULL) {
                    arg1 = *(u16*)((u8*)node + 0x2C);
                    arg2 = *(u16*)((u8*)node + 0x2E);
                }
                if ((u8)wazaSequenceSysIsPlayingWaza(current, arg1, arg2)) {
                    fn_800FE6A0(lbl_8047E2F0, lbl_8047E2F0);
                    switch (state) {
                    case 3:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2F4);
                        break;
                    case 4:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, (const char*)lbl_80279470);
                        break;
                    case 1:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2FC);
                        break;
                    case 2:
                        GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E304);
                        break;
                    }
                    fn_800FE6A0(lbl_8047E30C, lbl_8047E30C);
                }
            }
        }
        break;
    case 0:
        wazaViewerUpdatePlayMode();
        break;
    }
}
#endif

#endif

/**
 * wazaViewerSpawnAttacker - respawn the attacker-side effect from the work
 * area's staged parameters, seating it at -distance for its grid slot.
 * mode is the wazaSequenceOwnerLoadWaza animation mode; passing 0 folds that call away,
 * which is how the camera-reset path spawns without an animation.
 */
static inline void wazaViewerSpawnAttacker(u8* work, s32 mode) {
    extern f32 lbl_8047E2E4;
    extern f32 lbl_8047E2EC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fadeEffectGetRandom__FUl(s32 range);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* wazaSequencePokemonCreate(u16 id, s32 random, u32 useSeed);
#endif
    extern void* wazaSequenceTrainerCreate(u16 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetPosition(void* model, f32* position);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetRotation(void* model, f32* rotation);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerLoadWaza(void* effect, u16 anim, s32 mode, u8 flag);
#endif

    WazaViewerVec position;
    WazaViewerVec rotation;
    s32 flag;
    s32 anim;
    s32 seed;
    s32 slot;
    s32 id;
    void* effect;
    f32 distance;

    slot = *(s32*)(work + 0x630);
    flag = *(s32*)(work + 0x654);
    anim = *(s32*)(work + 0x650);
    seed = *(s32*)(work + 0x640);
    id = *(s32*)(work + 0x634);
    distance = battleGridGetDistance((u8)slot);
    set__5GSvecFfff((f32*)&position, -distance, lbl_8047E2E4, lbl_8047E2E4);
    set__5GSvecFfff((f32*)&rotation, lbl_8047E2E4, lbl_8047E2EC, lbl_8047E2E4);
    *(void**)(lbl_804673F8 + 0x61C) = NULL;
    if (id == 0) {
        return;
    }
    if (slot != 0) {
        effect = wazaSequencePokemonCreate((u16)id, PC_PTR(_fadeEffectGetRandom__FUl(-1)),
                             (u32)(-seed | seed) >> 31);
    } else {
        effect = wazaSequenceTrainerCreate((u16)id);
    }
    if (effect == NULL) {
        return;
    }
    GSmodelSetPosition(PTR32_AT_T(void, (u8*)effect, 0x24), (f32*)&position);
    GSmodelSetRotation(PTR32_AT_T(void, (u8*)effect, 0x24), (f32*)&rotation);
    wazaSequenceOwnerSetVisibility(effect, 1);
    *(s8*)((u8*)effect + 0x76) = -1;
    if (mode != 0 && anim != 0) {
        wazaSequenceOwnerLoadWaza(effect, (u16)anim, mode, (u8)flag);
    }
    *(void**)(lbl_804673F8 + 0x61C) = effect;
}

/** wazaViewerSpawnTarget - the defender-side counterpart, seated at +distance. */
static inline void wazaViewerSpawnTarget(u8* work, s32 mode) {
    extern f32 lbl_8047E2E4;
    extern f32 lbl_8047E2E8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fadeEffectGetRandom__FUl(s32 range);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* wazaSequencePokemonCreate(u16 id, s32 random, u32 useSeed);
#endif
    extern void* wazaSequenceTrainerCreate(u16 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetPosition(void* model, f32* position);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetRotation(void* model, f32* rotation);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerLoadWaza(void* effect, u16 anim, s32 mode, u8 flag);
#endif

    WazaViewerVec position;
    WazaViewerVec rotation;
    s32 flag;
    s32 anim;
    s32 seed;
    s32 slot;
    s32 id;
    void* effect;
    f32 distance;

    slot = *(s32*)(work + 0x644);
    flag = *(s32*)(work + 0x654);
    anim = *(s32*)(work + 0x650);
    seed = *(s32*)(work + 0x64C);
    id = *(s32*)(work + 0x648);
    distance = battleGridGetDistance((u8)slot);
    set__5GSvecFfff((f32*)&position, distance, lbl_8047E2E4, lbl_8047E2E4);
    set__5GSvecFfff((f32*)&rotation, lbl_8047E2E4, lbl_8047E2E8, lbl_8047E2E4);
    *(void**)(lbl_804673F8 + 0x620) = NULL;
    if (id == 0) {
        return;
    }
    if (slot != 0) {
        effect = wazaSequencePokemonCreate((u16)id, PC_PTR(_fadeEffectGetRandom__FUl(-1)),
                             (u32)(-seed | seed) >> 31);
    } else {
        effect = wazaSequenceTrainerCreate((u16)id);
    }
    if (effect == NULL) {
        return;
    }
    GSmodelSetPosition(PTR32_AT_T(void, (u8*)effect, 0x24), (f32*)&position);
    GSmodelSetRotation(PTR32_AT_T(void, (u8*)effect, 0x24), (f32*)&rotation);
    wazaSequenceOwnerSetVisibility(effect, 1);
    *(s8*)((u8*)effect + 0x76) = 1;
    if (mode != 0 && anim != 0) {
        wazaSequenceOwnerLoadWaza(effect, (u16)anim, mode, (u8)flag);
    }
    *(void**)(lbl_804673F8 + 0x620) = effect;
}

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_EXACT_624C)
/**
 * wazaViewerUpdateModels - The viewer thread's mode-1 step: on a reload request (+0x628) free both models,
 * spawn the attacker and target in the chosen layout (+0x658) and aim the camera; otherwise, when playback was
 * asked for (+0x62C), start each model's move in turn once the previous one has finished.
 * Address: 0x801D624C | Size: 0x818
 */
/* NAME-T3 fn_801D624C */
void wazaViewerUpdateModels(void) {
    extern f32 lbl_8047E2E4;
    extern f32 lbl_8047E310;
    extern f32 lbl_8047E314;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetMode(s32 mode);
#endif
    extern void cameraSetTarget(s32 a, s32 b);
    extern void cameraSetDistance(f32 distance);
    extern void cameraSetHeight(f32 height);
    extern void cameraSetRotY(f32 angle);

    u8* work;
    void** models;
    void* effect;
    s32 i;
    s32 id;

    /* Retail keeps the work area in r31 through the calls, and the spawn
     * helpers read their staged parameters through it; the target-side
     * store and the index loops below address the global directly. */
    work = lbl_804673F8;
    models = (void**)(work + 0x61C);
    effect = models[*(s32*)(work + 0x624)];
    if (*(s32*)(work + 0x628) != 0) {
        for (i = 0; i < 2; i++) {
            if (*(void**)(work + 0x61C + i * 4) != NULL) {
                wazaSequenceOwnerFree(*(void**)(work + 0x61C + i * 4));
                *(void**)(work + 0x61C + i * 4) = NULL;
            }
        }
        if (*(s32*)(work + 0x63C) != 0) {
            wazaViewerSpawnAttacker(work, 0);
            *(s32*)(work + 0x628) = 0;
            *(s32*)(work + 0x62C) = 0;
            *(s32*)(work + 0x624) = 0;
            effect = models[*(s32*)(work + 0x624)];
            if (effect != NULL) {
                cameraSetTarget(*(s32*)effect, *(s32*)((u8*)effect + 0x04));
                GSscene_SetMode(5);
                cameraSetDistance(lbl_8047E310);
                cameraSetHeight(lbl_8047E314);
                cameraSetRotY(lbl_8047E2E4);
            }
            return;
        }
        switch (*(s32*)(work + 0x658)) {
        case 2:
            if (*(s32*)(work + 0x634) != 0) {
                wazaViewerSpawnAttacker(work, 4);
            }
            /* the id test is kept in a local: it colours as the helper's id */
            if ((id = *(s32*)(work + 0x648)) != 0) {
                wazaViewerSpawnTarget(work, 0);
            }
            break;
        case 1:
            if (*(s32*)(work + 0x634) != 0) {
                wazaViewerSpawnAttacker(work, 3);
            }
            if ((id = *(s32*)(work + 0x648)) != 0) {
                wazaViewerSpawnTarget(work, 0);
            }
            break;
        case 0:
        default:
            if (*(s32*)(work + 0x634) != 0) {
                wazaViewerSpawnAttacker(work, 1);
            }
            if (*(s32*)(work + 0x648) != 0) {
                wazaViewerSpawnTarget(work, 2);
            }
            break;
        }
        *(s32*)(work + 0x628) = 0;
        *(s32*)(work + 0x62C) = 0;
        *(s32*)(work + 0x624) = 0;
        for (i = 0; i < 2; i++) {
            if (*(void**)(work + 0x61C + i * 4) != NULL) {
                *(s32*)(work + 0x624) = i;
                break;
            }
        }
    } else {
        if (*(s32*)(work + 0x62C) == 0) {
            return;
        }
        if ((u8)wazaSequenceOwnerIsPlaying(effect)) {
            return;
        }
        /* the body indexes with the value the test loaded; the step reloads */
        for (*(s32*)(work + 0x624) += 1; (i = *(s32*)(work + 0x624)) < 2;
             (*(s32*)(work + 0x624))++) {
            effect = *(void**)(lbl_804673F8 + 0x61C + i * 4);
            if (effect != NULL) {
                if (PTR32_AT_T(void, (u8*)effect, 0x68) != NULL) {
                    wazaSequenceOwnerSetVisibility(effect, 1);
                    wazaSequenceSysStartWaza(effect, *(u16*)((u8*)(PTR32_AT_T(void, (u8*)effect, 0x68)) + 0x2C),
                                *(u16*)((u8*)(PTR32_AT_T(void, (u8*)effect, 0x68)) + 0x2E));
                }
                break;
            }
        }
        if (*(s32*)(work + 0x624) >= 2) {
            *(s32*)(work + 0x624) = 0;
            *(s32*)(work + 0x62C) = 0;
            for (i = 0; i < 2; i++) {
                if (*(void**)(lbl_804673F8 + 0x61C + i * 4) != NULL) {
                    *(s32*)(work + 0x624) = i;
                    break;
                }
            }
        }
    }
}

#endif

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_6A64_ONLY)
/**
 * wazaViewerUpdatePlayMode - per-frame update for the viewer's play mode: advances the
 * effect, updates the camera, draws the STEP/PAUSED/PLAYING status and polls commands.
 * Address: 0x801D6A64 | Size: 0x3F4
 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wazaViewerUpdatePlayMode(void) {
    extern f32 lbl_8047E2F0;
    extern const char lbl_8047E2F4;  /* "STEP" */
    extern const char lbl_8047E2FC;  /* "PAUSED" */
    extern const char lbl_8047E304;  /* "PLAYING" */
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E30C;
#endif
    extern void GSfilterSetColor(void* filter, u8* color);
    extern u8 dbgMenuIsOpen(void);
    extern s32 vtrGetMode(void);
    extern void fn_800FE6A0(f32 width, f32 height);
    extern void GSmsgPrintf(s32 x, s32 y, u32 color, const char* fmt, ...);

    u8 color[4];
    int arg1;
    void* node;
    const char* messages;
    u32 arg2;
    u8 paused;
    void* effect;
    s32 state;
    void** models;

    messages = (const char*)lbl_80279338;
    models = (void**)(lbl_804673F8 + 0x61C);
    effect = models[*(s32*)(lbl_804673F8 + 0x624)];
    color[3] = 0;
    color[2] = 0;
    color[1] = 0;
    color[0] = 0;
    GSfilterSetColor(*(void**)(lbl_804673F8 + 0x10), color);
    if (effect != NULL) {
        if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->advance) {
            wazaViewerDrawAttachPoints(effect);
        }
        /* retail reads the pause bit before it tests the node pointer */
        paused = ((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->paused;
        node = PTR32_AT_T(void, (u8*)effect, 0x68);
        if (node != NULL) {
            if (((u8*)node)[0x14] == 0) {
                ((u8*)node)[0x16] = paused;
            }
        }
        wazaViewerDrawBackdrop(effect);
        *(s32*)(lbl_804673F8 + 0x14) = *(u16*)((u8*)effect + 0x32);
        if (!dbgMenuIsOpen() && !menuIsCheck(0x78)) {
            wazaViewerUpdateInput();
        }
    }
    wazaViewerDrawErrors();
    state = vtrGetMode();
    arg1 = arg2 = 0;
    if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->overlay) {
        effect = models[*(s32*)(lbl_804673F8 + 0x624)];
        if (effect != NULL) {
            node = PTR32_AT_T(void, (u8*)effect, 0x68);
            if (node != NULL) {
                arg1 = *(u16*)((u8*)node + 0x2C);
                arg2 = *(u16*)((u8*)node + 0x2E);
            }
            if ((u8)wazaSequenceSysIsPlayingWaza(effect, arg1, arg2)) {
                fn_800FE6A0(lbl_8047E2F0, lbl_8047E2F0);
                switch (state) {
                case 3:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2F4);
                    break;
                case 4:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, messages + 0x138);
                    break;
                case 1:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2FC);
                    break;
                case 2:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E304);
                    break;
                }
                fn_800FE6A0(lbl_8047E30C, lbl_8047E30C);
            }
        }
    }
    if (vtrGetMode() == 1) {
        wazaViewerPollCommand(messages);
    }
}
#else
void wazaViewerUpdatePlayMode(void) {
    extern f32 lbl_8047E2F0;
    extern const char lbl_8047E2F4;  /* "STEP" */
    extern const char lbl_8047E2FC;  /* "PAUSED" */
    extern const char lbl_8047E304;  /* "PLAYING" */
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E30C;
#endif
    extern void GSfilterSetColor(void* filter, u8* color);
    extern u8 dbgMenuIsOpen(void);
    extern s32 vtrGetMode(void);
    extern void fn_800FE6A0(f32 width, f32 height);
    extern void GSmsgPrintf(s32 x, s32 y, u32 color, const char* fmt, ...);

    s32 state;
    void* node;
    const char* messages;
    u8 paused;
    void* effect;
    s32 arg1;
    s32 arg2;
    void** models;
    void* current;
    void* currentNode;
    u8 color[4];

    messages = (const char*)lbl_80279338;
    models = (void**)(lbl_804673F8 + 0x61C);
    effect = models[*(s32*)(lbl_804673F8 + 0x624)];
    color[3] = 0;
    color[2] = 0;
    color[1] = 0;
    color[0] = 0;
    GSfilterSetColor(*(void**)(lbl_804673F8 + 0x10), color);
    if (effect != NULL) {
        if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->advance) {
            wazaViewerDrawAttachPoints(effect);
        }
        /* retail reads the pause bit before it tests the node pointer */
        paused = ((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->paused;
        node = PTR32_AT_T(void, (u8*)effect, 0x68);
        if (node != NULL) {
            if (((u8*)node)[0x14] == 0) {
                ((u8*)node)[0x16] = paused;
            }
        }
        wazaViewerDrawBackdrop(effect);
        *(s32*)(lbl_804673F8 + 0x14) = *(u16*)((u8*)effect + 0x32);
        if (!dbgMenuIsOpen() && !menuIsCheck(0x78)) {
            wazaViewerUpdateInput();
        }
    }
    wazaViewerDrawErrors();
    state = vtrGetMode();
    arg2 = arg1 = 0;
    if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->overlay) {
        current = models[*(s32*)(lbl_804673F8 + 0x624)];
        if (current != NULL) {
            currentNode = *(void**)((u8*)current + 0x68);
            if (currentNode != NULL) {
                arg1 = *(u16*)((u8*)currentNode + 0x2C);
                arg2 = *(u16*)((u8*)currentNode + 0x2E);
            }
            if ((u8)wazaSequenceSysIsPlayingWaza(current, arg1, arg2)) {
                fn_800FE6A0(lbl_8047E2F0, lbl_8047E2F0);
                switch (state) {
                case 3:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2F4);
                    break;
                case 4:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, messages + 0x138);
                    break;
                case 1:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E2FC);
                    break;
                case 2:
                    GSmsgPrintf(0xC8, 0x10, 0xFF0000A0, &lbl_8047E304);
                    break;
                }
                fn_800FE6A0(lbl_8047E30C, lbl_8047E30C);
            }
        }
    }
    if (vtrGetMode() == 1) {
        wazaViewerPollCommand(messages);
    }
}
#endif

#endif

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_EXACT_6E58)
/**
 * wazaViewerDrawBackdrop - The viewer's plain-background option: while it is on, hide the room's models (the
 * floor's scene_data list) and draw a flat coloured quad facing the camera behind the model, sized from its bound.
 * Address: 0x801D6E58 | Size: 0x3D8
 */
/* NAME-T3 fn_801D6E58 */
void wazaViewerDrawBackdrop(void* effect) {
    extern u8 lbl_8027947C[];  /* "scene_data" */
    extern s32 lbl_8047B400;
    extern u16 lbl_8047E2E0;
    extern u8 lbl_8047E2E2;
    extern f32 lbl_8047E2F0;
    extern u8 lbl_80314958[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorDataBiosGetCurrentPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resId);
#endif
    extern void* HSD_ArchiveGetPublicAddress(void* archive, const char* symbol);
    extern u32 floorReadMakeModelResID(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GScameraGetActiveCamera(void);
#endif
    extern void* GSmodelGetBound(void* model);
    extern void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(void);
    extern void GScameraGetPosition(void* camera, WazaViewerVec* position);
    extern void GScameraGetLookAt(void* camera, WazaViewerVec* up, WazaViewerVec* at);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecSub(WazaViewerVec* dst, const WazaViewerVec* a, const WazaViewerVec* b);
#endif
    extern void ObjInfoInit(void* bound, WazaViewerVec* extent);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 vecMag(const WazaViewerVec* v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecNormalize(WazaViewerVec* dst, const WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void vecScale(WazaViewerVec* dst, const WazaViewerVec* src, f32 scale);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecAdd(WazaViewerVec* dst, const WazaViewerVec* a, const WazaViewerVec* b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DFFCC(WazaViewerVec* dst, const WazaViewerVec* a, const WazaViewerVec* b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetBlendMode(u32 a, u32 b, u32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetUpdates(u32 a, u32 b, u32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetZMode(u32 a, u32 b, u32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800D9ED8(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetCullMode(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos(f32 x, f32 y, f32 z);
#endif
    extern void gfxVtxColor(u32 a, u32 r, u32 g, u32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif

    void* floorData;
    u32 group;
    void* sceneData;
    void* camera;
    void* bound;
    s32 i;
    s32 index;
    u8 green;
    u8 blue;
    f32 extentLength;
    u8 color[4];
    WazaViewerVec eye;
    WazaViewerVec axisX;
    WazaViewerVec forward;
    WazaViewerVec at;
    WazaViewerVec centre;
    WazaViewerVec axisY;
    WazaViewerVec extent;

/* The room record's map resource id (+0x08, floorDataBiosGetMapResID's field). PC: the floor table is common_rel data,
 * kept big-endian on the PC (port-gaps.md 6.3), so it is read with DISC_U32 like floor_data.c does; the plain read gave
 * a byte-swapped id, no archive, and a crash in HSD_ArchiveGetPublicAddress (found 2026-11-18 with ORRE_WAZA_VIEWER).
 * Port correctness fix (byte order), all platforms; the GameCube keeps the original read. */
#ifdef TARGET_PC
#define WAZA_VIEWER_MAP_RES_ID(floorData) DISC_U32((u8*)(floorData) + 0x08)
#else
#define WAZA_VIEWER_MAP_RES_ID(floorData) (*(u32*)((u8*)(floorData) + 0x08))
#endif
    floorData = floorDataBiosGetCurrentPtr();
    group = std093_getCurrentGroupID();
    index = 0;
    if (lbl_8047B400 != ((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->bounds) {
        sceneData = HSD_ArchiveGetPublicAddress(
            GSresGetResource(std093_getCurrentGroupID(), WAZA_VIEWER_MAP_RES_ID(floorData)),
            (const char*)lbl_8027947C);
        if (sceneData == NULL) {
            return;
        }
        if (*(u32*)sceneData != 0) {
            i = floorReadMakeModelResID(WAZA_VIEWER_MAP_RES_ID(floorData));
            for (; ((u32*)DISC_SLOT(sceneData, 0))[index] != 0; index++) { /* model list: disc slots, tested for zero */
                void* model = GSresGetResource(group, i | index);
                if (model != NULL) {
                    GSmodelSetVisibility(model, (u8)lbl_8047B400);
                }
            }
        }
        lbl_8047B400 = ((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->bounds;
    }
    if (lbl_8047B400 == 0) {
        return;
    }
    camera = GScameraGetActiveCamera();
    *(u16*)&color[0] = lbl_8047E2E0;
    color[2] = lbl_8047E2E2;
    if (camera == NULL) {
        return;
    }
    if (PTR32_AT_T(void, (u8*)effect, 0x24) == NULL) {
        return;
    }
    bound = GSmodelGetBound(PTR32_AT_T(void, (u8*)effect, 0x24));
    if (bound == NULL) {
        return;
    }
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
    GScameraGetPosition(camera, &eye);
    GScameraGetLookAt(camera, &forward, &at);
    GSvecSub(&axisX, &at, &eye);
    ObjInfoInit(bound, &extent);
    extentLength = vecMag(&axisX);
    {
        f32 length = vecMag(&extent);
        extentLength = length + extentLength;
    }
    GSvecNormalize(&axisX, &axisX);
    vecScale(&centre, &axisX, extentLength);
    GSvecAdd(&centre, &centre, &at);
    GSvecNormalize(&forward, &forward);
    fn_800DFFCC(&axisY, &axisX, &forward);
    GSvecNormalize(&axisY, &axisY);
    fn_800DFFCC(&forward, &axisY, &axisX);
    vecScale(&axisY, &axisY, lbl_8047E2F0 * extentLength);
    vecScale(&forward, &forward, extentLength);
    gfxSetBlendMode(0, 1, 0);
    gfxSetUpdates(1, 0, 1);
    gfxSetZMode(1, 2, 1);
    fn_800D9ED8(0);
    gfxSetCullMode(0);
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetVtxDesc(PC_PTR32(lbl_80314958));
    gfxSetPrimitive(6);
    gfxBegin(4);
    gfxVtxPos(forward.x + (centre.x + axisY.x), forward.y + (centre.y + axisY.y),
                forward.z + (centre.z + axisY.z));
    green = color[1];
    blue = color[2];
    gfxVtxColor(0, color[0], green, blue);
    gfxVtxPos(forward.x + (centre.x - axisY.x), forward.y + (centre.y - axisY.y),
                forward.z + (centre.z - axisY.z));
    gfxVtxColor(0, color[0], green, blue);
    gfxVtxPos((centre.x - axisY.x) - forward.x, (centre.y - axisY.y) - forward.y,
                (centre.z - axisY.z) - forward.z);
    gfxVtxColor(0, color[0], green, blue);
    gfxVtxPos((centre.x + axisY.x) - forward.x, (centre.y + axisY.y) - forward.y,
                (centre.z + axisY.z) - forward.z);
    gfxVtxColor(0, color[0], green, blue);
    gfxEnd();
}

/**
 * wazaViewerDrawErrors - Print one message line for each error bit set in work +0x0C (the bits
 * wazaViewerSetErrorFlag sets, e.g. a missing model part).
 * Address: 0x801D7230 | Size: 0x21C
 */
/* NAME-T3 fn_801D7230 */
void wazaViewerDrawErrors(void) {
    extern f32 lbl_8047E2F0;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E30C;
#endif
    extern void fn_800FE6A0(f32 width, f32 height);
    extern void GSmsgPrintf(s32 x, s32 y, u32 color, const char* fmt, ...);

    const char* messages;
    s32 flags;
    s32 line;

    messages = (const char*)lbl_80279338;
    line = 0;
    flags = *(s32*)(lbl_804673F8 + 0x0C);
    if (flags != 0) {
        fn_800FE6A0(lbl_8047E2F0, lbl_8047E2F0);
        if (flags & 0x001) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x150);
            line++;
        }
        if (flags & 0x002) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x150);
            line++;
        }
        if (flags & 0x004) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x16C);
            line++;
        }
        if (flags & 0x008) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x184);
            line++;
        }
        if (flags & 0x010) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x19C);
            line++;
        }
        if (flags & 0x020) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x1B0);
            line++;
        }
        if (flags & 0x040) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x1C8);
            line++;
        }
        if (flags & 0x080) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x1DC);
            line++;
        }
        if (flags & 0x100) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x1F4);
            line++;
        }
        if (flags & 0x200) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x208);
            line++;
        }
        if (flags & 0x400) {
            GSmsgPrintf(0xA, 0x10 + line * 0xD, -1, messages + 0x228);
            line++;
        }
        fn_800FE6A0(lbl_8047E30C, lbl_8047E30C);
    }
}

#endif

#if !defined(PR409_WAZA_VIEWER_EXACT_624C) && !defined(PR409_WAZA_VIEWER_EXACT_6E58) && !defined(PR409_WAZA_VIEWER_EXACT_7464) && !defined(PR409_WAZA_VIEWER_FN_ONLY)
/**
 * wazaViewerSetErrorFlag - Move animation OR bits into lbl_804673F8+0xC.
 * Address: 0x801D744C | Size: 0x18
 */
extern u8 lbl_804673F8[];
void wazaViewerSetErrorFlag(u32 bits) {
    *(u32*)(lbl_804673F8 + 0x0C) |= bits;
}

#endif

#if !defined(PR409_WAZA_VIEWER_EXACT_624C) && !defined(PR409_WAZA_VIEWER_EXACT_6E58)
/* =========================================================================
 * CORE WAZA SEQUENCE FUNCTIONS (0x801D7464 - 0x801D9E34)
 *
 * The main waza sequence API: load, update, start/stop entries.
 * These are the functions called from the battle state machine.
 * ========================================================================= */

/**
 * wazaViewerUpdateInput - The viewer's pad handling (skipped while the debug menu is open): the buttons start
 * or replay the move and play motions, the sticks move the camera.
 * Corrected 2026-11-08: this was proposed as wazaSequenceLoad ("load a complete waza sequence"); the code reads
 * the pad (GSinputButtonsChanged, both sticks) and moves the camera.
 * Address: 0x801D7464 | Size: 0x730
 */
#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_EXACT_7464)
/* NAME-T3 fn_801D7464 */
void wazaViewerUpdateInput(void) {
    extern u8 lbl_804673F8[];
    extern u8 lbl_803725A0[];
    extern u8 lbl_8047B3FC;
    extern u8 lbl_8047B3FD;
    extern u8 lbl_8047B3FE;
    extern u8 lbl_8047B3FF;
    extern f32 lbl_8047E2E4;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E30C;
#endif
    extern const f32 lbl_8047E318;
    extern const f32 lbl_8047E31C;
    extern const f32 lbl_8047E320;
    extern const f32 lbl_8047E328;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraDirectionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraPositionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraViewVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraDirectionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraPositionVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraViewVector(WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 GSscene_GetMode(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecSub(WazaViewerVec* dst, const WazaViewerVec* a,
                            const WazaViewerVec* b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 vecMag(const WazaViewerVec* v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecNormalize(WazaViewerVec* dst, const WazaViewerVec* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DFFCC(WazaViewerVec* dst, const WazaViewerVec* a,
                            const WazaViewerVec* b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void vecScale(WazaViewerVec* dst, const WazaViewerVec* src,
                            f32 scale);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E0718(void* out, const void* axis, f32 angle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecTransformQuat(void* out, const void* quat, const void* vec);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecAdd(WazaViewerVec* dst, const WazaViewerVec* a,
                         const WazaViewerVec* b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void set__5GSvecFfff(f32* vec, f32 x, f32 y, f32 z);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSinputButtonsChanged(s32 pad);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800F7BC4(s32 pad);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 GSinputGetLeftStickXData(s32 pad, s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 GSinputGetLeftStickYData(s32 pad, s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 GSinputGetRightStickXData(s32 pad, s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 GSinputGetRightStickYData(s32 pad, s32 mode);
#endif
    extern u32 fn_800F7B5C(s32 pad);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GScameraGetActiveCamera(void);
#endif
    extern void GScameraSetPosition(void* cam, WazaViewerVec* pos);
    extern void GScameraLookAt(void* cam, void* up, WazaViewerVec* at);
    extern void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(void);
    extern u8 wazaSequencePokemonMotionStart(void* owner, BOOL enabled);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 wazaSequenceSysIsPlayingWaza(void* obj, s32 search_key1, s32 search_key2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerSetVisibility(void* effect, u32 visible);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysStartWaza(void* sequence, s32 moveID, s32 variant);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerPlayMotion(void* effect, s32 weatherType, s32 enabled);
#endif
    WazaEffect* effect;
    void* node;
    void* camera;
    WazaViewerVec direction;
    WazaViewerVec position;
    WazaViewerVec view;
    WazaViewerVec target;
    WazaViewerVec forward;
    WazaViewerVec axisX;
    WazaViewerVec step;
    WazaViewerVec axis;
    f32 rotation[4];
    WazaViewerVec eye;
    WazaViewerVec dirNorm;
    WazaViewerVec zoom;
    WazaViewerVec lookAt;
    f32 angleStep;
    s32 currentIdx;
    s32 motionA;
    s32 motionB;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;

    motionA = 0;
    motionB = 0;
    currentIdx = *(s32*)(lbl_804673F8 + 0x624);
    effect = ((WazaEffect**)(lbl_804673F8 + 0x61C))[currentIdx];
    if (effect == NULL) {
        return;
    }

    node = PTR32_AT_T(void, (u8*)effect, 0x68);
    if (node != NULL) {
        motionA = *(u16*)((u8*)node + 0x2C);
        motionB = *(u16*)((u8*)node + 0x2E);
    }

    if (*(s32*)lbl_804673F8 == 0) {
        if (GSscene_GetMode() == 2) {
            forward = *(const WazaViewerVec*)lbl_80279338;
            angleStep = lbl_8047E318 * (f32)GSgetFrameStep();
            GSscene_GetCameraDirectionVector(&direction);
            GSscene_GetCameraPositionVector(&position);
            GSscene_GetCameraViewVector(&view);
            GSvecAdd(&target, &position, &view);
            GSvecSub(&target, &target, &direction);
            GSvecNormalize(&target, &target);
            fn_800DFFCC(&axisX, &forward, &target);
            GSvecNormalize(&axisX, &axisX);
            fn_800DFFCC(&forward, &target, &axisX);
            GSvecNormalize(&forward, &forward);

            stickX = GSinputGetLeftStickXData(1, 1);
            stickY = GSinputGetLeftStickYData(1, 1);
            substickX = GSinputGetRightStickXData(1, 1);
            substickY = GSinputGetRightStickYData(1, 1);

            if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_UP) != 0) {
                lbl_8047B3FC = 1;
            }
            if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_DOWN) != 0) {
                lbl_8047B3FD = 1;
            }
            if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_LEFT) != 0) {
                lbl_8047B3FE = 1;
            }
            if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_RIGHT) != 0) {
                lbl_8047B3FF = 1;
            }

            if (lbl_8047B3FC != 0) {
                vecScale(&step, &forward, angleStep);
                GSvecAdd(&view, &view, &forward);
                if ((fn_800F7B5C(1) & GSinputButtonsChanged(1) & PAD_BUTTON_UP) != 0) {
                    lbl_8047B3FC = 0;
                }
            }
            if (lbl_8047B3FD != 0) {
                vecScale(&step, &forward, angleStep);
                GSvecSub(&view, &view, &step);
                if ((fn_800F7B5C(1) & GSinputButtonsChanged(1) & PAD_BUTTON_DOWN) != 0) {
                    lbl_8047B3FD = 0;
                }
            }
            if (lbl_8047B3FE != 0) {
                vecScale(&step, &axisX, angleStep);
                GSvecAdd(&view, &view, &step);
                if ((fn_800F7B5C(1) & GSinputButtonsChanged(1) & PAD_BUTTON_LEFT) != 0) {
                    lbl_8047B3FE = 0;
                }
            }
            if (lbl_8047B3FF != 0) {
                vecScale(&step, &axisX, angleStep);
                GSvecSub(&view, &view, &step);
                if ((fn_800F7B5C(1) & GSinputButtonsChanged(1) & PAD_BUTTON_RIGHT) != 0) {
                    lbl_8047B3FF = 0;
                }
            }

            if (stickX != 0 || stickY != 0 || substickX != 0 || substickY != 0) {
                GSvecAdd(&eye, &position, &view);
                GSvecSub(&direction, &direction, &eye);
                if (stickX != 0) {
                    set__5GSvecFfff((f32*)&axis, lbl_8047E2E4, lbl_8047E30C, lbl_8047E2E4);
                    fn_800E0718(rotation, &axis, lbl_8047E31C * (stickX > 0 ? angleStep : -angleStep));
                    GSvecTransformQuat(&direction, rotation, &direction);
                }
                if (stickY != 0) {
                    set__5GSvecFfff((f32*)&axis, lbl_8047E2E4, lbl_8047E30C, lbl_8047E2E4);
                    GSvecNormalize(&dirNorm, &direction);
                    fn_800DFFCC(&axis, &axis, &dirNorm);
                    if (stickY > 0 || vecMag(&axis) > lbl_8047E320) {
                        GSvecNormalize(&axis, &axis);
                        fn_800E0718(rotation, &axis, lbl_8047E31C * (stickY > 0 ? angleStep : -angleStep));
                        GSvecTransformQuat(&direction, rotation, &direction);
                    }
                }
                if (substickY != 0) {
                    GSvecCopy(&zoom, &direction);
                    GSvecNormalize(&zoom, &zoom);
                    vecScale(&zoom, &zoom, substickY > 0 ? angleStep : -angleStep);
                    GSvecAdd(&direction, &direction, &zoom);
                }
                GSvecAdd(&direction, &direction, &eye);
            }

            GSscene_SetCameraDirectionVector(&direction);
            GSscene_SetCameraPositionVector(&position);
            GSscene_SetCameraViewVector(&view);
            camera = GScameraGetActiveCamera();
            if (camera != NULL) {
                GSvecAdd(&lookAt, &position, &view);
                GScameraSetPosition(camera, &direction);
                GScameraLookAt(camera, lbl_803725A0, &lookAt);
                _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
            }
        }

        if (*(s32*)(lbl_804673F8 + 0x4) == 1 && PTR32_AT_T(void, (u8*)effect, 0x68) == NULL) {
            if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_TRIGGER_L) != 0) {
                (*(u16*)((u8*)effect + 0x32))--;
            }
            if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_TRIGGER_R) != 0) {
                (*(u16*)((u8*)effect + 0x32))++;
                if (*(u16*)((u8*)effect + 0x32) > *(u16*)((u8*)effect + 0x14) - 1) {
                    *(u16*)((u8*)effect + 0x32) = 0;
                }
            }
        }

        if (((WazaViewerCtrl*)(lbl_804673F8 + 0x08))->advance &&
            (fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_Y) != 0) {
            (*(s32*)(lbl_804673F8 + 0x18))++;
            if (*(s32*)(lbl_804673F8 + 0x18) >= 0x10) {
                *(s32*)(lbl_804673F8 + 0x18) = -1;
            }
        }

        if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_A) != 0 &&
            *(s32*)(lbl_804673F8 + 0x4) == 1 && PTR32_AT_T(void, (u8*)effect, 0x68) == NULL) {
            wazaSequencePokemonMotionStart(effect, 0);
        }
    }

    if ((fn_800F7BC4(1) & GSinputButtonsChanged(1) & PAD_BUTTON_A) != 0) {
        if (*(s32*)lbl_804673F8 == 1 && *(s32*)(lbl_804673F8 + 0x63C) != 0) {
            wazaSequenceOwnerPlayMotion(effect, *(s32*)(lbl_804673F8 + 0x638), 0);
        } else if ((u8)wazaSequenceSysIsPlayingWaza(effect, motionA, motionB) == 0) {
            wazaSequenceOwnerSetVisibility(effect, 1);
            wazaSequenceSysStartWaza(effect, motionA, motionB);
            *(s32*)(lbl_804673F8 + 0x62C) = 1;
        }
    }
}

#endif

#if !defined(PR409_WAZA_VIEWER_SPLIT) || defined(PR409_WAZA_VIEWER_7B94_ONLY)
/**
 * wazaViewerDrawAttachPoints - Draw a green point at each of the model's 16 attachment parts (or only the
 * selected one, work +0x18), sized by the size class and camera distance; a missing part sets error bit 2.
 * Corrected 2026-11-08: this was proposed as wazaSequenceUpdate (a different function, 0x801DB988).
 * Address: 0x801D7B94 | Size: 0x2C4
 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
/* NAME-T3 fn_801D7B94 */
void wazaViewerDrawAttachPoints(void* effect) {
    extern u8 lbl_804673F8[];
    extern u8 lbl_80314638[];
    extern const f32 lbl_8047E318;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E30C;
#endif
    extern const f32 lbl_8047E330;
    extern const f32 lbl_8047E334;
    extern const f32 lbl_8047E338;
    extern const f32 lbl_8047E33C;
    extern const f32 lbl_8047E340;
    extern const f32 lbl_8047E344;
    extern void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GScameraGetActiveCamera(void);
#endif
    extern void GScameraGetPosition(void*, WazaViewerVec*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GScameraGetPerspective(
        void*, f32*, f32*, f32*, f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetBlendMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetUpdates(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetZMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetCullMode(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 vecMag(WazaViewerVec*);
#endif
    extern void GXSetPointSize(s32, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSpartGetTransform(
        GSpart*, WazaViewerVec*, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void clear__5GSvecFv(WazaViewerVec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos(f32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxColor4u8(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif
    u8* data;
    u8* entry;
    struct GSmodel* model;
    GSpart* part;
    void* camera;
    WazaViewerVec transform;
    WazaViewerVec cameraPosition;
    f32 aspect;
    f32 fov;
    f32 scale;
    s32 partIndex;
    s32 i;

    data = effect;
    model = PTR32_AT_T(struct GSmodel, data, 0x24);
    entry = ((u8*)PTR32_AT(data, 0x2C)) + *(u16*)(data + 0x32) * 0xD4;
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(
        ((void*)PTR32_AT(data, 0x2C)));
    camera = GScameraGetActiveCamera();
    GScameraGetPosition(camera, &cameraPosition);
    GScameraGetPerspective(camera, &fov, &aspect, &aspect, &aspect);

    switch (*(s32*)(data + 0x10)) {
    case -2:
        aspect = lbl_8047E318;
        break;
    case -1:
        aspect = lbl_8047E330;
        break;
    case 0:
        aspect = lbl_8047E30C;
        break;
    case 1:
        aspect = lbl_8047E334;
        break;
    case 2:
        aspect = lbl_8047E338;
        break;
    case 3:
        aspect = lbl_8047E33C;
        break;
    }

    gfxSetBlendMode(1, 6, 1);
    gfxSetUpdates(1, 1, 1);
    gfxSetZMode(0, 7, 0);
    gfxSetCullMode(0);
    scale = lbl_8047E340 * aspect /
            (fov * vecMag(&cameraPosition));
    if (scale > lbl_8047E30C) {
        scale = lbl_8047E30C;
    }
    GXSetPointSize((s32)(lbl_8047E344 * scale), 5);
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetVtxDesc(lbl_80314638);
    gfxSetPrimitive(0);

    partIndex = *(s32*)(lbl_804673F8 + 0x18);
    if (partIndex < 0) {
        gfxBegin(0x10);
        for (i = 0; i < 0x10; i++) {
            part = GSmodelGetPart(
                model, *(s32*)(entry + 0x4C + i * 4));
            if (part != NULL) {
                GSpartGetTransform(part, &transform, NULL, NULL);
                GSpartFree(part);
            } else {
                clear__5GSvecFv(&transform);
                *(u32*)(lbl_804673F8 + 0x0C) |= 2;
            }
            gfxVtxPos(transform.x, transform.y, transform.z);
            gfxVtxColor4u8(0, 0xFF, 0, 0, 0xDC);
        }
        gfxEnd();
    } else {
        gfxBegin(1);
        part = GSmodelGetPart(
            model, *(s32*)(entry + 0x4C + partIndex * 4));
        if (part != NULL) {
            GSpartGetTransform(part, &transform, NULL, NULL);
            GSpartFree(part);
        } else {
            clear__5GSvecFv(&transform);
            *(u32*)(lbl_804673F8 + 0x0C) |= 2;
        }
        gfxVtxPos(transform.x, transform.y, transform.z);
        gfxVtxColor4u8(0, 0xFF, 0, 0, 0xDC);
        gfxEnd();
    }
}
#else
/* NAME-T3 fn_801D7B94 */
void wazaViewerDrawAttachPoints(void* effect) {
    extern u8 lbl_804673F8[];
    extern u8 lbl_80314638[];
    extern void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GScameraGetActiveCamera(void);
#endif
    extern void GScameraGetPosition(void*, WazaViewerVec*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GScameraGetPerspective(
        void*, f32*, f32*, f32*, f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetBlendMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetUpdates(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetZMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetCullMode(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 vecMag(WazaViewerVec*);
#endif
    extern void GXSetPointSize(s32, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSpartGetTransform(
        GSpart*, WazaViewerVec*, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void clear__5GSvecFv(WazaViewerVec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos(f32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxColor4u8(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif
    typedef struct WazaSequenceEntry {
        u8 unk00[0x4C];
        s32 parts[0x10];
        u8 unk8C[0xD4 - 0x8C];
    } WazaSequenceEntry;
    u8* data;
    WazaSequenceEntry* entry;
    struct GSmodel* model;
    void* camera;
    GSpart* part;
    s32 i;
    WazaViewerVec cameraPosition;
    WazaViewerVec transform;
    f32 fov;
    f32 aspect;
    f32 scale;

    data = effect;
    model = *(struct GSmodel**)(data + 0x24);
    entry = *(WazaSequenceEntry**)(data + 0x2C) + *(u16*)(data + 0x32);
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(
        ((void*)PTR32_AT(data, 0x2C)));
    camera = GScameraGetActiveCamera();
    GScameraGetPosition(camera, &cameraPosition);
    GScameraGetPerspective(camera, &fov, &aspect, &aspect, &aspect);

    switch (*(s32*)(data + 0x10)) {
    case -2:
        aspect = 0.5f;
        break;
    case -1:
        aspect = 0.75f;
        break;
    case 0:
        aspect = 1.0f;
        break;
    case 1:
        aspect = 1.3333f;
        break;
    case 2:
        aspect = 2.0f;
        break;
    case 3:
        aspect = 4.0f;
        break;
    }

    gfxSetBlendMode(1, 6, 1);
    gfxSetUpdates(1, 1, 1);
    gfxSetZMode(0, 7, 0);
    gfxSetCullMode(0);
    scale = 256.0f * aspect /
            (fov * vecMag(&cameraPosition));
    if (scale > 1.0f) {
        scale = 1.0f;
    }
    GXSetPointSize((s32)(255.0f * scale), 5);
    gfxSetFlags(1);
    gfxClearFlags(6);
    gfxSetVtxDesc(lbl_80314638);
    gfxSetPrimitive(0);

    if (*(s32*)(lbl_804673F8 + 0x18) < 0) {
        gfxBegin(0x10);
        for (i = 0; i < 0x10; i++) {
            part = GSmodelGetPart(
                model, entry->parts[i]);
            if (part != NULL) {
                GSpartGetTransform(part, &transform, NULL, NULL);
                GSpartFree(part);
            } else {
                clear__5GSvecFv(&transform);
                *(u32*)(lbl_804673F8 + 0x0C) |= 2;
            }
            gfxVtxPos(transform.x, transform.y, transform.z);
            gfxVtxColor4u8(0, 0xFF, 0, 0, 0xDC);
        }
        gfxEnd();
    } else {
        gfxBegin(1);
        part = GSmodelGetPart(
            model, entry->parts[*(s32*)(lbl_804673F8 + 0x18)]);
        if (part != NULL) {
            GSpartGetTransform(part, &transform, NULL, NULL);
            GSpartFree(part);
        } else {
            clear__5GSvecFv(&transform);
            *(u32*)(lbl_804673F8 + 0x0C) |= 2;
        }
        gfxVtxPos(transform.x, transform.y, transform.z);
        gfxVtxColor4u8(0, 0xFF, 0, 0, 0xDC);
        gfxEnd();
    }
}
#endif

#endif

#endif

#endif

#endif

#endif

#endif
