/**
 * @file main.c
 * @brief Main entry point and top-level game initialization for Pokemon Colosseum.
 *
 * Contains main(), the game initialization sequence (fn_800057B0), the main
 * game loop thread (mainGameThread), the early reset/retrace callbacks, and the
 * per-retrace game logic (mainRetraceLogic).
 *
 * Address range: 0x800055E0 - 0x80005FA8
 * Small data:    .sdata2 0x8047B6A0 - 0x8047B6B8 (the 0.0f and 359940.0f
 *                literals and the two int-to-float conversion constants)
 * Source file:   (unknown original filename, likely main.c or game.c)
 */

#include "dolphin/types.h"
#include "dolphin/vi/VI.h"

/* =========================================================================
 * Forward declarations of SDK / engine functions called by this module.
 * Names prefixed with fn_ are auto-generated and not yet fully identified.
 * =========================================================================
 */

/* CRT / libc */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif

/* Dolphin OS */
extern void* OSGetArenaLo(void);
extern void* OSGetArenaHi(void);
extern void  OSSetArenaLo(void* lo);
extern u32   OSGetResetCode(void);
extern void  OSSetErrorHandler(u32 error, void* handler);
extern void  OSResetSystem(int reset, int returnCode, BOOL forceMenu);
extern u64   OSGetTime(void);
extern void  OSLoadContext(void* context);
extern void  OSEnableScheduler(void);

/* --- Game engine subsystem init functions --- */

/* The error module (the original error.c; main_tail.c here): errorInit installs errorExceptionHandler for the OS
 * errors and protects memory ranges, called very early from main(); errorExceptionHandler saves the CPU state and
 * starts errorReportThreadMain, which formats the report; errorDecrementerHandler keeps that thread running;
 * errorPanicStop is the game's panic routine (file, line, printf message), which never returns. */
void errorInit(void);
void errorExceptionHandler(u32 error, void* context, ...);
void errorDecrementerHandler(u32 error, void* context);
s32 errorReportThreadMain(void);
void errorPanicStop(const char* file, s32 line, const char* fmt, ...);

/* --- Heap / memory init (around 0x800E3560) --- */
extern void  GSmemInit(u32 heapId, void* start, void* end); /* GSmem heap init */
extern void  fn_800E3560(u32 heapId);                          /* GSmem set default heap */

/* --- Heap allocator (around 0x8009AB60) --- */
extern void* OSInitAlloc(void* start, void* end, u32 numHeaps); /* OSInitAlloc / OSCreateHeap */

/* --- DVD / file system init --- */
extern void fn_800ACA80(void* dvdInfo, u32 priority);  /* DVDInit-related */
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern u32  ARInit(void* stack_index_addr, u32 num_entries);
#endif
extern void AIInit(void* stack);
extern void GSscratchInit(u32 arg);
extern void ARQInit(void);                          /* DVDFSInit or DVDSetAutoInvalidation */

/* --- ARAM / audio memory --- */
extern void fn_800AC440(void* arAddr);   /* ARInit / ARQInit */

/* --- Graphics init --- */
extern void GSlogInit(u32 xfbSize, u32 numXfbs);   /* GXInit or framebuffer setup */
extern void fn_800EEDF8(u32 unused);                   /* VI init */

/* --- Game-specific init called from main --- */
extern void fn_80167FA4(int argc, char** argv, u32 flag); /* GameArgsParse */

/* fn_800057B0: fn_800057B0 - the massive init function called from main.
 * Sets up all subsystems, then enters the main loop. */
void fn_800057B0(void);

/* --- Subsystem init functions called from fn_800057B0 --- */
extern void GSthread(u32 numFrames);              /* VIFrameInit or render timing */
extern void GSgfxInit__FP15_GSgfxInitParms(u32 memSize, u32 a, u32 b,   /* GSgfx init */
                         u32 c, u32 d, u32 e);
extern u32  OSGetProgressiveMode(void);                         /* OSGetResetSwitchState (checks warm boot) */
extern void fn_800D37D4(u32, u32, u32, u32, u32, u32);/* GSgfx video mode config */
extern void GStextureInit(u32 numEntries);               /* GX FIFO init */
extern void fn_80191484(u32 numSounds);                 /* Sound system init */
extern void GSmodelInit(u32 maxObjects);                /* GSmem object pool */
extern void GSpartInit(u32 param);                     /* VI callback setup */
extern void GSmaterialInit(u32 bufSize);                   /* Display list buffer init */
extern void fn_800D2AD4(u32 count);                     /* GSgfx light init */
extern void GSlightInit(u32 count);                     /* GSgfx texture init */
extern void gsParticleInit(u32 a, u32 b);                  /* GSmaterial init */
extern void fn_80132C6C(u32 a, u32 b, u32 c, u32 d);  /* GStexture cache init */
extern void GSinputInit(void);                           /* GSgfx camera/viewport init */

extern void GSinputOpen(u32 padIdx);                    /* PAD motor init per-controller */
extern void GSinputSetClamping(u32 padIdx, u32 param);        /* PAD rumble config */
extern void GSinputSetSmoothingRate(u32 padIdx, u32 param);        /* PAD deadzone config */
extern void GSinputSetSmoothingFunc(u32 padIdx, u32 param);        /* PAD stick config */

extern void GSgappInit(u32 numTasks, u32 numQueues);  /* GSthread init */
extern void GSresInit(u32 maxSteps);                  /* GSthread step limit */
extern void fn_800FF828(u32 a, u32 b, u32 c, u32 d);  /* GSthread pool config */
extern void fn_8010D170(void);                           /* GSthread scheduler init */
extern void cms_InitThreadPool(u32 maxPads);                   /* PAD system init */
extern void fn_800F75FC(void* padTable);                 /* PAD set mapping table */
extern void GSmsgInit(u32 numFloors, u32 numLayers);  /* GSfloor system init */
extern void GSmsgSetCtrlFunc(void* sndTable);                 /* GSfloor sound table */
extern void GSmsgFontOpen(void* relData);                  /* GSfloor register REL data */
extern void GSmsgOpen(void* relData);                  /* GSfloor register REL data (alt) */
extern void cms_RegisterScript(void* relData);                  /* GSfloor register REL data (floor) */
extern void soundSetInitParams(u32 a, u32 b, u32 c, u32 d, u32 e); /* set the five DVD error message IDs */
extern void vtrInit(void);                           /* Save/card system init */
extern void flareCreateTask(void);                           /* GBA communication init */
extern void GSmovieInit(void);                           /* Save data init */
extern void menuInit(u32 param);                      /* Effect system init */
extern void fn_80101FB8(u32 param);                      /* GS load meter draw-sync hook init */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D3074(u32 flag);                       /* GSgfx enable rendering */
#endif

extern u32 GSgappCreate(s32 state, u8 priority, void* param, /* GSthread create task */
                       void* func);
extern void GSgappUpdate(void);                           /* GSthread yield / run scheduler */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSthreadCreate(u32 affinity, u32 priority,      /* GSthread create main thread */
                         u32 stackSize, u32 usesFPU,
                         u32 autoStart, void* entry);
#endif

extern s32 GSvtrRegisterGSgapp(u32 taskId);              /* Save task registration */
extern void fadeInit(void);                           /* Script engine init */
extern void fn_80168638(u32 numSlots);                   /* Floor/scene loader init */
extern void GSeffectInit(u32 maxEffects);                 /* 3D model/effect loader */
extern u32  GSmemGetFreeTotal(void);                           /* OSGetFreeMemSize-like */
extern void GSlogWrite(const char* fmt, ...);           /* OSReport / debug printf */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 __cvt_fp2unsigned(f64 value);                       /* MSL runtime: float -> unsigned */
#endif

/* --- Game main loop (mainGameThread) and its helpers --- */
void mainGameThread(void);
void fn_8000609C(void); /* fn_8000609C - background color / render schedule init */

/* --- Per-frame callback tasks registered by fn_800057B0 --- */
void mainRetraceDraw(void);       /* mainRetraceDraw - primary retrace handler */
void fn_80005FFC(void);      /* fn_80005FFC - controller polling task */
void fn_8000604C(void);    /* fn_8000604C - controller rumble task */
void mainLogicTask(u32 a, u32 b); /* mainLogicTask - audio retrace callback */
void mainClearExceptionVectors(void);    /* mainClearExceptionVectors - reset button countdown */
void mainRetraceLogic(void);  /* mainRetraceLogic - per-retrace game logic */
void mainCheckResetButton(void); /* mainCheckResetButton - reset button handler */

/* --- Small utility functions near main --- */

/* mainSetPauseFlag: mainSetPauseFlag - sets a byte flag; returns previous value */
extern u8 mainSetPauseFlag(u8 newVal);

/* fn_800056D4: fn_800056D4 - reads flag, clears it, returns old */
extern u8 fn_800056D4(void);

/* fn_800056E4: fn_800056E4 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800056E4(u8 val);
#endif

/* gamedataSetPlaytime: gamedataSetPlaytime - if sound system present, set volume */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataSetPlaytime(f32 volume);
#endif

/* fn_80005748: fn_80005748 - if sound system present, get volume */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_80005748(void);
#endif

/* GSgetVersionMajor: GSgetVersionMajor - returns 1 */
extern u32 GSgetVersionMajor(void);

/* GSgetVersionMinor: GSgetVersionMinor - returns 6 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetVersionMinor(void);
#endif

/* --- Miscellaneous engine functions --- */
extern void movieBootStart(void); /* boot: the loader thread (file TOC), then the fade-in */
extern u32  fn_800FF81C(void* a, u32 b); /* GSthread set frame counter */
extern void* GSresAllocResource(u32 a, u32 b, u32 c, u32 d); /* GSthread alloc work area */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  savedataBiosGetNowSavedataPtr(void);    /* the current save block (was read as "SoundSystemIsReady") */
#endif
extern u32 savedataBiosGetGamedataPtr(void* ptr); /* returns ptr (0 if NULL): the game-data section at +0; was guessed
                                      "SoundSystemGetContext" (void): it takes the save object
                                      savedataBiosGetNowSavedataPtr returns, in r3 (assembly, 2026-10-15) */
extern void* gamedatasaveBiosGetPtr(void* ptr); /* ptr + 8: the game data inside the save (was guessed "SoundGetMixer") */
extern void gamedatasaveBiosSetPlaytime(void* mixer, f32 volume); /* SoundSetMasterVolume */
extern f32  gamedatasaveBiosGetPlaytime(void* mixer);  /* SoundGetMasterVolume */

extern u32  savedataBiosGetSavedataBufferPtr(void);     /* the save area (was read as "RNG get seed") */
extern void savedataBiosSetNowSavedataPtr(u32 seed); /* sets the current save block pointer; was read as RNG set seed */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataCreate(u32 a, u32 b); /* RNG init */
#endif
extern void fn_801EF5C0(void);    /* Battle system global init */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  savedataGetStatus(u32 a, u32 b); /* RNG or calendar read */
#endif
#ifdef TARGET_PC
/* PC: the real types (gs_flag_exact_801908D4.c): three flag-class buffers and their counts; declared as six u32 the
 * fifth argument (a pointer passed on the stack) had an undefined upper half (B's extern_scan, 2026-10-17) */
extern void fn_801909A8(u32* buffer1, u32 count1, u32* buffer2, u32 count2, u32* buffer3, u32 count3);
#else
extern void fn_801909A8(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f); /* set up the three flag classes' buffers (corrected 2026-10-17: was "Calendar/RTC set") */
#endif
extern void GSvtrLoadTexture(void);    /* Card system tick */
extern void fn_8010C220(void);    /* Effect system tick */
extern void cameraInit(void);    /* World/map system init */
extern void fn_8013024C(void);    /* 3D model system init */
extern void peopleSystemInit(u32 a);   /* Pokemon data/model init */
extern void sodateyaHeroCallBackInit(void);    /* Day-Care step callback (corrected 2026-11-08: was "GBA link init") */
extern void memcardInit(void);    /* Menu/UI system init */
extern void fn_801128A0(void);    /* Colosseum mode init */
extern void* tableResBiosGetResPtr(u32 id);  /* Load relocatable module (REL) by ID */

extern void* soundSetListener(void* color); /* Set clear/background color */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSresRegisterResource(void* work, u32 time, u32 numFrames, u32 arg); /* Thread wait/schedule */
#endif

#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern u32  fn_800A03B4(void);    /* OSGetResetButtonState */
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern int  OSGetResetButtonState(void); /* real symtab name (signed return -> cmpwi) */
#endif
extern void VIFlush(void);               /* real symtab name for fn_800AA068 call site */
extern void soundShutdown(void);    /* Screen fade to black */
extern void PADRecalibrate(u32 mask);/* VISetBlack / video blank */
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void VISetBlack(u32 flag);/* VIFlush */
#endif
extern void fn_800AA068(void);    /* VIWaitForRetrace */
extern void OSSetIdleFunction(u32 a, u32 b, u32 c, u32 d); /* OSClearStack or thread cleanup */
extern void VIWaitForRetrace(void);    /* AIReset */

extern void GSthreadExecuteAll(void);    /* GSthread begin frame */
extern u8   fn_8000DAA8(void);    /* is the pause menu open (2026-10-16; was guessed as a locale check) */
extern void peopleMain(u32 a, u32 b); /* Retry/reconnect handler */

extern u8   fn_8008ABA0(u32 padIdx); /* PAD is connected check */
extern void GSinputRead(u32 padIdx); /* PAD disconnect handler */
extern u8 GSinputIsConnected(u32 padIdx); /* PAD connection check */
extern void fn_8008AC34(u32 padIdx); /* PAD recalibrate */

extern void fn_80101B90(u32 mask);   /* GXSetChanCtrl / GX render state */
extern void fn_80101D8C(void);       /* GX render flush */
extern void fn_80101D5C(void);       /* GX render end */
extern void GSgfxWaitRetrace(u8 mode);    /* GSgfx set draw mode */
extern void GSgfxSwapBuffers(u32 flag);   /* GSgfx swap buffers */
extern void GSgfxCaptureUpdate(void);       /* World/scene render tick */

extern void fn_801E0FB4(s32 unk, u32 a, u32 b); /* Save/card per-frame update */

extern u8   vtrIsActive(void);   /* Card system check pending */
extern s32  vtrGetMode(void);   /* Card system get state */
extern void vtrSetMode1(void);   /* Card system process state 2 */
extern void vtrSetMode2(void);   /* Card system process other */
extern void vtrSetMode3(void);   /* Card system finalize */

extern void menuDaemon(void);   /* Particle system update */
extern void GSmsgDaemon(void);   /* GSthread sync / process tasks */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  std082_flagGetValue(u32 evtId);  /* Event system check state */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32  fn_800D37CC(void);       /* GSgfx get frame counter */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  GSgetFrameStep(void);       /* GSgfx get tick counter */
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  GSinputButtonsChanged(u32 padIdx); /* PAD get buttons pressed */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fn_800F7BC4(u32 padIdx); /* PAD get buttons held */
#endif
extern void dbgMovieCaptureUpdate(void);       /* Debug menu toggle */

/* --- SDA (small data area) globals used by this module --- */

/* lbl_80478DC0 @sda21 - DVD info structure */
extern u8 lbl_80478DC0;

/* lbl_80478DC8 @sda21 - rumble enabled flag */
extern u8 lbl_80478DC8;

/* lbl_80478DC9 @sda21 - pause/init-complete flag */
extern u8 lbl_80478DC9;

/* lbl_80478DCA @sda21 - reset button latch */
extern u8 lbl_80478DCA;

/* lbl_80478DCC @sda21 - reset countdown timer (int) */
extern s32 lbl_80478DCC;

/* lbl_80478DD0 @sda21 - draw mode byte */
extern u8 lbl_80478DD0;

/* lbl_80478820 @sda21 - unknown flag (checked in VBlank task) */
extern u8 lbl_80478820;

/* lbl_80478990 @sda21 - max error handler index */
extern s32 lbl_80478990;

/* lbl_8047A260 @sda21 - error number for crash display */
extern u32 lbl_8047A260;
/* lbl_8047A264 @sda21 */
extern u32 lbl_8047A264;
/* lbl_8047A268 @sda21 */
extern u32 lbl_8047A268;

/* lbl_80478FB8 @sda21 - pointer to the floor (room) table's record count (common_rel, 2026-10-16) */
extern u32* lbl_80478FB8;
/* lbl_80478FBC @sda21 - the floor (room) table: 0x4C-byte records (common_rel) */
extern void* lbl_80478FBC;

/* lbl_8047B6B8 @sda21 - small string, source file name for error print */
extern char lbl_8047B6B8[8];
/* lbl_8047B6C0 @sda21 - small format string for the error name line */
extern char lbl_8047B6C0[8];

/* --- Data labels referenced by init code --- */
extern u8 lbl_8039A700[];   /* ARAM base for ARInit */
extern u8 lbl_802E1CF0[];   /* PAD mapping table */
extern u8 msgctrlcode[];   /* GSfloor sound table */
extern u8 lbl_8027A500[];   /* Floor/scene REL data (data section start) */
extern u8 lbl_802BD260[];   /* Floor/scene REL data #2 */
extern u8 lbl_802C0CB0[];   /* Floor/scene REL data #3 */
extern u8 lbl_802CF810[];   /* Floor/scene REL data #4 */
extern u8 lbl_80266438[];   /* Background clear color (rodata) */
extern u8 lbl_802663A0[];   /* Exception address map table (rodata) */
extern u8 lbl_803A0700[];   /* Crash report text buffer (BSS) */
extern u8 lbl_803A1700[];   /* Saved OS context for crash (BSS) */
extern u8 lbl_8039E700[];   /* Error display thread stack (BSS) */
extern u8 lbl_802E2888[];   /* Error name string table */

extern const char lbl_80266420[]; /* "OS avail memory: %d\n" */
extern const char lbl_80266448[]; /* "OS_ERROR_SYSTEM_RESET" (error name table base) */

/* OSProtectRange = __OSInitMemoryProtection region setup */
extern void OSProtectRange(u32 region, u32 addr, u32 size, u32 perm);

/* DBIsDebuggerPresent = __DBIsExceptionMarked or similar */
extern s32  DBIsDebuggerPresent(void);

/* OSCreateThread = OSCreateThread */
extern void OSCreateThread(void* thread, void* func, void* arg,
                         void* stackBase, u32 stackSize,
                         u32 priority, u32 detached);
/* OSResumeThread = OSResumeThread */
extern void OSResumeThread(void* thread);
/* fn_800A1E54 = OSSetThreadPriority */
extern void fn_800A1E54(void* thread, u32 prio);

/* va_list for PPC - MetroWerks CW inline */
#ifndef TARGET_PC /* MWCC's PowerPC va_list; the PC uses the host's <stdarg.h> */
typedef struct __va_list_struct {
    u8  gpr;
    u8  fpr;
    u16 padding;
    u32* overflow_arg_area;
    u32* reg_save_area;
} __va_list_struct;
typedef __va_list_struct va_list[1];
#define va_start(ap, last) ((void)last, __builtin_va_info(&ap))
#define va_end(ap)         ((void)0)
#else
#include <stdarg.h>
#endif

/* vsprintf = vsprintf */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  vsprintf(char* buf, const char* fmt, va_list ap);
#endif
/* sprintf = sprintf */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  sprintf(char* buf, const char* fmt, ...);
#endif

/* OSGetStackPointer (0x8009BD28) */
extern u32 OSGetStackPointer(void);

/* __va_arg */
extern void* __va_arg(void* ap, u32 type);

/* __OSSetExceptionHandler */
extern void __OSSetExceptionHandler(u32 exception, void* handler);

/* =========================================================================
 *  main()
 *  Address: 0x800055E0, Size: 0xE4
 *
 *  Called by __start() after CRT init. Sets up the Dolphin heap, DVD
 *  subsystem, graphics, and then calls fn_800057B0() which never returns
 *  (it enters the main loop).
 * =========================================================================
 */
int main(int argc, char** argv) {
    void* arenaLo;
    void* arenaHi;
    void* heapStart;

    /* Clear low-memory scratch area at 0x80001800, size 0x1800 */
    memset((void*)0x80001800, 0, 0x1800);

    /* Install OS error handlers before anything else */
    errorInit(); /* the OS error handlers and memory protection */

    /* Set up the main game heap from the OS arena */
    arenaLo = OSGetArenaLo();
    arenaHi = OSGetArenaHi();

    /*
     * heapStart = arenaLo + 0x00E80000
     * The game reserves ~14.5 MB for the main heap.
     * addis r30, r31, 0xe8 => heapStart = arenaLo + (0xe8 << 16)
     */
    heapStart = (void*)PC_PTR((PC_PTR32(arenaLo) + 0x00E80000));

    /* Initialize the GS memory allocator with heap region [arenaLo, heapStart) */
    GSmemInit(0, arenaLo, heapStart);

    /* Set heap 0 as the default allocation heap */
    fn_800E3560(0);

    /* Create OS heap from [heapStart, arenaHi) for system allocations,
     * then move arena low past our heaps */
    OSSetArenaLo(OSInitAlloc(heapStart, OSGetArenaHi(), 1));

    /* Initialize ARAM (audio RAM) */
    ARInit(&lbl_80478DC0, 2);

    /* Initialize DVD filesystem */
    ARQInit();

    /* Initialize AI with stack at lbl_8039A700 + 0x3FF8 */
    AIInit((void*)(lbl_8039A700 + 0x3FF8));

    /* Initialize XFB (external framebuffer): 0x10000 bytes, 1 buffer */
    GSlogInit(0x10000, 1);

    /* Initialize scratch allocator */
    GSscratchInit(0);

    /* Parse command-line arguments (from disc header / apploader) */
    fn_80167FA4(argc, argv, 1);

    /* Enter the game initialization and main loop (does not return) */
    fn_800057B0(); /* fn_800057B0 */
}

/* =========================================================================
 *  mainSetPauseFlag / mainSetPauseFlag
 *  Address: 0x800056C4, Size: 0x10
 *
 *  Atomically sets the pause flag and returns the previous value.
 * =========================================================================
 */
u8 mainSetPauseFlag(u8 newVal) {
    u8 oldVal = lbl_80478DC9;
    lbl_80478DC9 = newVal;
    return oldVal;
}

/* =========================================================================
 *  fn_800056D4 / fn_800056D4
 *  Address: 0x800056D4, Size: 0x10
 *
 *  Reads the pause flag, clears it to 0, returns the old value.
 * =========================================================================
 */
u8 fn_800056D4(void) {
    u8 val = lbl_80478DC9;
    lbl_80478DC9 = 0;
    return val;
}

/* =========================================================================
 *  fn_800056E4 / fn_800056E4
 *  Address: 0x800056E4, Size: 0x8
 *
 *  Stores a flag controlling whether controller rumble is active.
 * =========================================================================
 */
void fn_800056E4(u8 val) {
    lbl_80478DC8 = val;
}

/* =========================================================================
 *  gamedataSetPlaytime / gamedataSetPlaytime
 *  Address: 0x800056EC, Size: 0x5C
 *
 *  If the sound system is initialized, sets the master volume on the
 *  sound mixer. The volume parameter arrives in f1.
 * =========================================================================
 */
void gamedataSetPlaytime(f32 volume) {
    void* mixer;
    u32 save; /* each call feeds the next in r3 (assembly) */

    /* The current save's game data (was read as "check if sound system is ready") */
    if ((save = savedataBiosGetNowSavedataPtr()) != 0 && (save = savedataBiosGetGamedataPtr((void*)PC_PTR(save))) != 0) {
        mixer = gamedatasaveBiosGetPtr((void*)PC_PTR(save));
    } else {
        mixer = NULL;
    }

    if (mixer != NULL) {
        gamedatasaveBiosSetPlaytime(mixer, volume);
    }
}

/* =========================================================================
 *  fn_80005748 / fn_80005748
 *  Address: 0x80005748, Size: 0x58
 *
 *  Returns the play time (gamedatasaveBiosGetPlaytime, converted to an unsigned integer), or 0 when the save data
 *  isn't available. *Corrected 2026-10-15:* this was described as the master volume and typed f32; the assembly ends
 *  in __cvt_fp2unsigned and returns r3, and the PDA uses the result as playTime.
 * =========================================================================
 */
u32 fn_80005748(void) {
    void* mixer;
    u32 save; /* each call feeds the next in r3 (assembly) */
    f32 volume; /* the play time as a float (name kept from the first reading) */

    if ((save = savedataBiosGetNowSavedataPtr()) != 0 && (save = savedataBiosGetGamedataPtr((void*)PC_PTR(save))) != 0) {
        mixer = gamedatasaveBiosGetPtr((void*)PC_PTR(save));
    } else {
        mixer = NULL;
    }

    if (mixer != NULL) {
        volume = gamedatasaveBiosGetPlaytime(mixer);
    } else {
        volume = 0.0f;
    }

    return __cvt_fp2unsigned(volume);
}

/* =========================================================================
 *  GSgetVersionMajor / GSgetVersionMajor
 *  Address: 0x800057A0, Size: 0x8
 *
 *  Returns the major version number: 1
 * =========================================================================
 */
u32 GSgetVersionMajor(void) {
    return 1;
}

/* =========================================================================
 *  GSgetVersionMinor / GSgetVersionMinor
 *  Address: 0x800057A8, Size: 0x8
 *
 *  Returns the minor version number: 6
 *  (Pokemon Colosseum version 1.6)
 * =========================================================================
 */
u32 GSgetVersionMinor(void) {
    return 6;
}

/* =========================================================================
 *  fn_800057B0 / fn_800057B0
 *  Address: 0x800057B0, Size: 0x2FC
 *
 *  Master initialization function. Sets up every game subsystem, then
 *  creates the main game thread and enters an infinite yield loop.
 *
 *  This function never returns.
 * =========================================================================
 */
void fn_800057B0(void) {
    u8 isWarmBoot;
    s32 i;

    /* Create the 20-slot cooperative GS thread (fiber) pool */
    GSthread(0x14);

    /* Initialize GSgfx (see GSgfxInit):
     *   0x6DDD0 = request-buffer size, 0x10 = matrix-stack depth,
     *   0x8 = render-layer pool (8 records of 0x190), 0x20 = pool of 0x18-byte records,
     *   0x1 = TV standard, 0x1E0 = EFB height override */
    GSgfxInit__FP15_GSgfxInitParms(0x6DDD0, 0x10, 0x8, 0x20, 0x1, 0x1E0);

    isWarmBoot = 0;

    /* Check if this is a warm boot (reset from game) */
    if (OSGetProgressiveMode() == 1) {
        /* If reset code's top bit indicates "return to game" */
        BOOL isReset = ((OSGetResetCode() + 0x80000000) == 0) ? TRUE : FALSE;
        if (isReset == TRUE) {
            isWarmBoot = 1;
        }
    }

    /* On warm boot, reconfigure video mode */
    if (isWarmBoot) {
        fn_800D37D4(1, 2, 0, 2, 1, 0);
    }

    /* Initialize GX command FIFO with 16 entries */
    GStextureInit(0x10);

    /* Initialize sound system with 8 channels */
    fn_80191484(0x8);

    /* Initialize GSmem object pool for 64 objects */
    GSmodelInit(0x40);

    /* Initialize VI retrace callback system with 8 callbacks */
    GSpartInit(0x8);

    /* Initialize display list buffer: 1024 bytes */
    GSmaterialInit(0x400);

    /* Initialize lighting system with 32 lights */
    fn_800D2AD4(0x20);

    /* Initialize texture system with 32 textures */
    GSlightInit(0x20);

    /* Initialize material system: 32 materials, 128 max textures */
    gsParticleInit(0x20, 0x80);

    /* Initialize texture cache: 16 slots, 64 entries, 1 bank, 8192 byte cache */
    fn_80132C6C(0x10, 0x40, 0x1, 0x2000);

    /* Initialize camera/viewport defaults */
    GSinputInit();

    /* Configure all 4 controller pads (indices 1-4) */
    for (i = 1; i <= 4; i++) {
        GSinputOpen(i);        /* Init motor for pad i */
        GSinputSetClamping(i, 2);    /* Set rumble mode */
        GSinputSetSmoothingRate(i, 7);    /* Set analog deadzone */
        GSinputSetSmoothingFunc(i, 1);    /* Set stick mode */
    }

    /* Initialize GSthread system: 16 tasks, 4 queues */
    GSgappInit(0x10, 0x4);

    /* Set thread step limit to 300 */
    GSresInit(0x12C);

    /* Initialize four floor contexts plus 48 floor resources and launch the floor worker */
    fn_800FF828(0x4, 0x10, 0x10, 0x10);

    /* Initialize collision-draw state */
    fn_8010D170();

    /* Allocate 16 script VM contexts (0x16C bytes each) */
    cms_InitThreadPool(0x10);

    /* Set the script VM native-call dispatch table (12-byte entries) */
    fn_800F75FC(lbl_802E1CF0);

    /* Initialize floor/scene system: 2 floors, 5 layers */
    GSmsgInit(0x2, 0x5);

    /* Register floor sound table */
    GSmsgSetCtrlFunc(msgctrlcode);

    /* Register floor REL data tables */
    GSmsgFontOpen(lbl_8027A500); /* Main scene data */
    GSmsgFontOpen(lbl_802BD260); /* Scene data #2 */
    GSmsgFontOpen(lbl_802C0CB0); /* Scene data #3 */
    GSmsgOpen(lbl_802CF810); /* Scene data #4 (alt registration) */

    /* Set the DVD error message IDs: cover open, no disk, wrong disk, retry, fatal */
    soundSetInitParams(0x3BE8, 0x3BEA, 0x3BED, 0x3BEF, 0x3BF2);

    /* Initialize GSvtr texture-display state and create its display task */
    vtrInit();

    /* Initialize battle texture/resource state and create its render callback */
    flareCreateTask();

    /* Initialize the THP movie runtime */
    GSmovieInit();

    /* Initialize 3D effect system: 24 max effects */
    menuInit(0x18);

    /* Install the GS load meter's GX draw-sync callback; GPU perf counters off */
    fn_80101FB8(0x4);

    /* Enable rendering pipeline */
    fn_800D3074(1);

    /* ---------------------------------------------------------------
     * Register per-frame callback tasks.
     * GSgappCreate creates a task: (active, taskId, param, func)
     * ---------------------------------------------------------------
     */

    /* Task 0xFF: Primary VBlank handler (mainRetraceDraw) */
    GSgappCreate(1, 0xFF, 0, (void*)mainRetraceDraw);

    /* Task 0x00: Pad rumble update (fn_8000604C) */
    GSgappCreate(1, 0x00, 0, (void*)fn_8000604C);

    /* Task 0x01: Pad read/polling (fn_80005FFC) */
    GSgappCreate(1, 0x01, 0, (void*)fn_80005FFC);

    /* Task 0x0A: Audio retrace callback (mainLogicTask) */
    GSvtrRegisterGSgapp(
        GSgappCreate(1, 0x0A, 0, (void*)mainLogicTask));

    /* Task 0xFD: Reset button countdown (mainClearExceptionVectors) */
    GSgappCreate(1, 0xFD, 0, (void*)mainClearExceptionVectors);

    /* Task 0xFE: Main retrace logic (mainRetraceLogic) */
    GSgappCreate(1, 0xFE, 0, (void*)mainRetraceLogic);

    /* Task 0xFF: Reset handler (mainCheckResetButton) - overrides earlier 0xFF? */
    GSgappCreate(1, 0xFF, 0, (void*)mainCheckResetButton);

    /* Mark initialization as complete */
    lbl_80478DC9 = 1;

    /* Initialize script engine */
    fadeInit();

    /* Initialize floor/scene loader with 8 slots */
    fn_80168638(0x8);

    /* Initialize model/effect loader: 32 max */
    GSeffectInit(0x20);

    /* Print available memory to debug console */
    {
        u32 freeMem = (u32)GSmemGetFreeTotal();
        GSlogWrite(lbl_80266420, freeMem); /* "OS avail memory: %d\n" */
    }

    /* Create and start the main game loop thread:
     *   affinity=0, priority=1000, stackSize=0x4000,
     *   usesFPU=1, autoStart=1, entry=mainGameThread */
    GSthreadCreate(0, 0x3E8, 0x4000, 1, 1, (void*)mainGameThread);

    /* Infinite yield loop - scheduler takes over from here */
    for (;;) {
        GSgappUpdate(); /* GSthread yield */
#ifdef TARGET_PC
        {
            /* OrreHook_AfterGappUpdate (docs/port/hook-points.md; 2026-11-21, the quicker way back from the PC screen):
             * the frame's update tasks have all run and no task list walk is in progress. OrrePort may run more of
             * the archive loader's work here while the PC screen's field room is being restored behind the black
             * screen (pc_field_menu.cpp). Nothing registered: nothing, as on the GameCube. All platforms */
            extern void (*OrreHook_AfterGappUpdate)(void);
            if (OrreHook_AfterGappUpdate != NULL) {
                OrreHook_AfterGappUpdate();
            }
        }
#endif
    }
}

/* =========================================================================
 *  mainGameThread / mainGameThread
 *  Address: 0x80005AAC, Size: 0x190
 *
 *  The main game loop thread entry point. Runs once per frame:
 *  1. Updates scene state
 *  2. Processes input, RNG seeding from clock
 *  3. Loads REL modules
 *  4. Updates all game subsystems
 *  5. Clears scratch memory
 * =========================================================================
 */
void mainGameThread(void) {
    void* mixer;
    u32 save; /* each call feeds the next in r3 (assembly) */

    /* Tick the scene/floor system */
    movieBootStart();

    /* Install the floor (room) table and its record count (fn_800FF81C; gs_floor.c) */
    {
        u32* floorCount = lbl_80478FB8;
        void* floorTable = lbl_80478FBC;
        fn_800FF81C(floorTable, *floorCount);
    }

    /* Allocate a 2-byte work area (for some per-frame state) */
    {
        void* work = GSresAllocResource(2, 0, 2, 0);
        *(u8*)(work) = 0;
        *((u8*)(work) + 1) = 0;
    }

    /* Clear draw mode flag */
    lbl_80478DD0 = 0;

    /* If there is a current save, reset its play-time float to 0.0 (was read as "reset master volume") */
    if ((save = savedataBiosGetNowSavedataPtr()) != 0 && (save = savedataBiosGetGamedataPtr((void*)PC_PTR(save))) != 0) {
        mixer = gamedatasaveBiosGetPtr((void*)PC_PTR(save));
    } else {
        mixer = NULL;
    }
    if (mixer != NULL) {
        gamedatasaveBiosSetPlaytime(mixer, 0.0f);
    }

    /* Reset rumble flag */
    lbl_80478DC8 = 0;

    /* Load REL modules and register their scene/floor data.
     * tableResBiosGetResPtr loads a relocatable module by ID and returns a pointer
     * to its data, which is then registered with the floor system. */
    GSmsgFontOpen(tableResBiosGetResPtr(1));  /* REL 1 -> register scene data */
    GSmsgOpen(tableResBiosGetResPtr(4));  /* REL 4 -> register scene data (alt) */
    cms_RegisterScript(tableResBiosGetResPtr(7));  /* REL 7 -> register floor data */

    /* Seed the RNG using the low bits of the OS tick counter.
     * OSGetTime returns u64 in r3:r4; we use r4 (low 32 bits).
     * rlwinm r27, r4, 0, 21, 26 => r4 & 0x000007E0 */
    savedataBiosSetNowSavedataPtr(savedataBiosGetSavedataBufferPtr() + ((u32)OSGetTime() & 0x7E0)); /* the save block's random offset, varied by the clock (likely anti-cheat; was read as an RNG seed) */
    savedataCreate(0, 0); /* Reset RNG sequence */

    /* Initialize the battle system */
    fn_801EF5C0();

    /* Point the three flag classes at their save-block buffers (save status 4 / 5 / 6) with their counts (7 / 8 / 9).
     * Corrected 2026-10-17: this was described as setting the calendar from the RTC. */
#ifdef TARGET_PC
    fn_801909A8((u32*)PC_PTR(savedataGetStatus(0, 4)), savedataGetStatus(0, 7), (u32*)PC_PTR(savedataGetStatus(0, 5)),
                savedataGetStatus(0, 8), (u32*)PC_PTR(savedataGetStatus(0, 6)), savedataGetStatus(0, 9));
#else
    fn_801909A8(savedataGetStatus(0, 4), savedataGetStatus(0, 7), savedataGetStatus(0, 5),
                savedataGetStatus(0, 8), savedataGetStatus(0, 6), savedataGetStatus(0, 9));
#endif

    /* Tick save/card system */
    GSvtrLoadTexture();

    /* Tick effect system (no-op stub at this point) */
    fn_8010C220();

    /* Initialize background color / render schedule */
    fn_8000609C();

    /* Initialize world/map system */
    cameraInit();

    /* Initialize 3D model system */
    fn_8013024C();

    /* Initialize Pokemon model/data system (48 slots) */
    peopleSystemInit(0x30);

    /* Register the Day-Care's per-step callback (corrected 2026-11-08: not the GBA link) */
    sodateyaHeroCallBackInit();

    /* Initialize menu/UI system */
    memcardInit();

    /* Initialize Colosseum battle mode */
    fn_801128A0();

    /* Clear low-memory scratch area 0x80001803, size 0x17FD
     * (preserves bytes 0x1800-0x1802 which contain status flags) */
    memset((void*)0x80001803, 0, 0x17FD);
}

/* =========================================================================
 *  mainCheckResetButton / mainCheckResetButton
 *  Address: 0x80005C3C, Size: 0xA8
 *
 *  Checks the reset button state. On first press, latches.
 *  On release (after latch), if init is complete, performs a full
 *  hardware shutdown and calls OSResetSystem.
 * =========================================================================
 */
void mainCheckResetButton(void) {
    if (lbl_80478DCA == 0) {
        /* Button not yet latched - check if pressed */
        if (OSGetResetButtonState() == 1) {
            lbl_80478DCA = 1; /* latch the reset press */
        }
    } else {
        /* Button was latched - wait for release */
        if (OSGetResetButtonState() == 0) {
            /* Released. Only reset if init is complete. */
            if (lbl_80478DC9 == 1) {
                soundShutdown();           /* Fade screen to black */
                PADRecalibrate(0xF0000000); /* likely PADRecalibrate(all four channels) (port/src/sdk_pc_alias.h) */
                VISetBlack(1);          /* black screen */
                VIFlush();               /* apply it (real symtab: VIFlush, was fn_800AA068) */
                VISetPreRetraceCallback(NULL);        /* Clear pre-retrace callback */
                VISetPostRetraceCallback(NULL);        /* Clear post-retrace callback */
                OSSetIdleFunction(0, 0, 0, 0); /* Clean up threads */
                VIWaitForRetrace();           /* wait one frame so the black screen shows */
                OSResetSystem(0, 0, 1);  /* Reset to system menu */
            }
        }
    }
}

/* =========================================================================
 *  mainClearExceptionVectors / mainClearExceptionVectors
 *  Address: 0x80005CE4, Size: 0x9C
 *
 *  On first call (lbl_80478DCC == 0), clears OS exception vectors by
 *  using the table at lbl_802663A0, then clears low-memory scratch.
 *  Sets a countdown timer of 100 frames, decremented each call.
 *  This provides a delay before the game is fully ready.
 * =========================================================================
 */
void mainClearExceptionVectors(void) {
    u32 i;

    if (lbl_80478DCC == 0) {
        /* First-time init: clear exception vector memory regions */
        for (i = 0; i < 0x20; i += 2) {
            u32 start = ((u32*)lbl_802663A0)[i] + 0x15;
            u32 end   = ((u32*)lbl_802663A0)[i + 1] + 0x16;
            memset((void*)PC_PTR((start + 0x80000000)), 0, end - start);
        }

        /* Clear scratch memory at 0x80001801 */
        memset((void*)0x80001801, 0, 0x17FF);

        /* Start the 100-frame countdown */
        lbl_80478DCC = 100;
    }

    /* Decrement the countdown timer each frame */
    lbl_80478DCC--;
}

/* =========================================================================
 *  mainRetraceDraw / mainRetraceDraw
 *  Address: 0x80005D80, Size: 0x80
 *
 *  Primary vertical blank handler. Runs each frame:
 *  1. Sets up GX render state
 *  2. Applies the draw mode
 *  3. Flushes GX commands
 *  4. Triggers the world/scene render
 *  5. Checks a flag and calls the save/card update accordingly
 *  6. Finalizes the GX render state
 * =========================================================================
 */
void mainRetraceDraw(void) {
    /* Set render state mask (0xFF00 = enable all channels) */
    fn_80101B90(0xFF00);

    /* Apply the draw mode from the per-frame flag */
    GSgfxWaitRetrace(lbl_80478DD0);

    /* Flush GX render commands */
    fn_80101D8C();

    /* Clear swap flag */
    GSgfxSwapBuffers(0);

    /* Tick world/scene renderer */
    GSgfxCaptureUpdate();

    /* Update save/card system based on lbl_80478820 flag */
    if (lbl_80478820 == 0) {
        fn_801E0FB4(0x10, 1, 1);
    } else {
        fn_801E0FB4(-1, 1, 1);
    }

    /* Finalize GX render state (0xFFFF00 = full state mask) */
    fn_80101B90(0xFFFF00);

    /* End GX render pass */
    fn_80101D5C();
}

/* =========================================================================
 *  mainRetraceLogic / mainRetraceLogic
 *  Address: 0x80005E00, Size: 0x1A8
 *
 *  Per-retrace game logic handler. Checks controller buttons for
 *  debug menu activation, handles save/card state transitions,
 *  and advances the save file's play time by one retrace, capped at
 *  359940 seconds (99:59:00).
 *
 *  The cap is a literal: retail loads it from this TU's own .sdata2 pool
 *  (0x8047B6A4, between the 0.0f literal and the conversion constants), and
 *  an extern reference instead changes the FPR allocation of the clamp.
 * =========================================================================
 */
void mainRetraceLogic(void) {
    void* mixer;
    u32 save; /* each call feeds the next in r3 (assembly) */
    f32 vol;
    f32 v;
    u32 pressed;
    u32 held;

    fn_80101B90(0xFF);

    pressed = GSinputButtonsChanged(1);
    held = fn_800F7BC4(1);
    if (held & pressed & 0x10) {
        dbgMovieCaptureUpdate();
    }

    if (vtrIsActive()) {
        pressed = GSinputButtonsChanged(1);
        held = fn_800F7BC4(1);
        if (held & pressed & 0x40) {
            if (vtrGetMode() == 2) {
                vtrSetMode1();
            } else {
                vtrSetMode2();
            }
        }
        pressed = GSinputButtonsChanged(1);
        held = fn_800F7BC4(1);
        if (held & pressed & 0x20) {
            vtrSetMode3();
        }
    }

    menuDaemon();
    GSmsgDaemon();

    if ((save = savedataBiosGetNowSavedataPtr()) != 0 && (save = savedataBiosGetGamedataPtr((void*)PC_PTR(save))) != 0) {
        mixer = gamedatasaveBiosGetPtr((void*)PC_PTR(save));
    } else {
        mixer = NULL;
    }

    if (mixer != NULL && lbl_80478DC8 == 1 && std082_flagGetValue(0x8AE) == 0) {
        vol = gamedatasaveBiosGetPlaytime(mixer);
        v = vol + (f32)GSgetFrameStep() / (f32)fn_800D37CC();
        if (v >= 359940.0f) {
            v = 359940.0f;
        }
        gamedatasaveBiosSetPlaytime(mixer, v);
    }
}
