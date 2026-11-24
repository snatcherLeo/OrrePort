/*
 * musyx_pc_alias.h: joins our matched MusyX runtime (src/musyx) with AxioDL MusyX's PC hardware layer
 * (extern/musyx-pc: hw_dolphin.c, hw_dspctrl.c, hw_aramdma.c, hw_pc*.c), without renaming anything in the repo
 * (snatcherLeo, 2026-10-15: PC-only alias header now, real names later through proposed-names.md).
 *
 * Force-included (-include) only when port/CMakeLists.txt compiles our src/musyx units for the PC. Our code still
 * calls some hardware functions and uses some hardware-layer data by their address names; each line below makes
 * that name mean AxioDL's function or variable, so our calls reach AxioDL's PC hardware layer and our definitions
 * carry the names AxioDL's code expects. The GameCube build never sees this file.
 *
 * Every entry needs evidence (docs/port/musyx-evaluation.md, "Audio path, step 2"). Confidence: "comment" = our own
 * source names it in a comment; "signature" = same parameters and position in the file as AxioDL's function.
 */
#ifndef MUSYX_PC_ALIAS_H
#define MUSYX_PC_ALIAS_H

/* ---- hardware functions our runtime calls ---- */
/* aramSyncTransferQueue: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* aramInit: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* aramExit: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* aramStoreData: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* aramRemoveData: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* aramAllocateStreamBuffer: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* salStartAi: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* salInitDsp: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* salExitDsp: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwExitIrq: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwIRQEnterCritical: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwIRQLeaveCritical: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* salDeactivateStudio: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* salAddStudioInput: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* salRemoveStudioInput: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */

/* hwOff: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwActivateStudio: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSaveSample: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSetSaveSampleCallback: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwRemoveSample: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSyncSampleMem: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwFrameDone: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* sndSetHooks: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */

/* ---- hardware.c functions our other units call by address name. The PC build uses AxioDL's hardware.c (snatcherLeo's
 * rule: audio hardware -> AxioDL); each line: our hardware.c's body is the same as AxioDL's function, in the same
 * file order ("body") ---- */
/* hwSetTimeOffset: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwGetTimeOffset: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwIsActive: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSetMesgCallback: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSetPriority: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
#define fn_80162858 hwSetVirtualSampleLoopBuffer /* body: vSampleInfo.loopBufferAddr / Length */
/* hwGetVirtualSampleState: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
#define fn_8016288C hwGetSampleType              /* body: smp_info.compType */
#define fn_801628A0 hwGetSampleID                /* body: smp_id */
/* hwSetStreamLoopPS: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSetSRCType: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwSetPolyPhaseFilter: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwDeactivateStudio: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwAddInput: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwRemoveInput: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwGetPos: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwInitStream: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwExitStream: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwGetStreamPlayBuffer: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwTransAddr: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwInitSampleMem: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwExitSampleMem: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwDisableHRTF: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* hwVoiceInStartup: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */

/* ---- runtime functions AxioDL's hardware.c (snd_handle_irq) calls by name: same call sequence in both handlers ---- */
/* streamCorrectLoops: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* seqHandle: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* s3dHandle: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* streamHandle: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */

/* ---- tables AxioDL's hardware.c reads by name ---- */
#define lbl_8036944C dspScale2IndexTab /* comment; u8[1024] on both sides */

/* ---- public API our runtime still has under address names (the test and the port call them by name) ---- */
#define fn_8015FE88 sndInit      /* comment (snd_init.c: "sndInit"); the game's sound init calls it with sndInit's arguments */
/* sndPushGroup: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
/* sndSeqPlayEx: the game now calls it by this name (upstream rename 2026-10-18), the same as its PC target */
#define fn_8015FFD4 sndIsInstalled /* comment (snd_init.c: "sndIsInstalled"); returns sndActive */

/* ---- hardware-layer data ---- */
#define lbl_8047B05E salAuxFrame     /* comment */
#define lbl_8047B05F salFrame        /* comment */
#define lbl_8047B05C salMaxStudioNum /* comment */
#define lbl_80447E60 dspStudio       /* comment */
#define lbl_80434C50 synthInfo       /* comment ("synthInfo block at lbl_80434C50") */
/* dspVoice, sndActive, synthVoice: named in symbols.txt since 2026-11-09 (data batch 19, XD's map), the same as their PC targets */
#define lbl_8047B054 salHooks        /* sndSetHooks stores into it */
#define lbl_804285D0 seqInstance     /* comment (SEQ_INSTANCE[8]) */
#define lbl_8047AF58 synthRealTime   /* comment (u64) */
#define lbl_80446F10 vs              /* comment (#define vs (*(MusyxVirtualSamples*)lbl_80446F10)) */
#define lbl_8047B05D salNumVoices    /* likely: our salInitDsp (hw_dspctrl_exact_8015A484.c) stores numVoices here and sizes the voice tables with it, as AxioDL's does */
#define lbl_80478BF0 last_rnd        /* verified: sndRand multiplies it by 0xA8351D63 (= AxioDL's 2822053219); disc value 1 = AxioDL's */

/* ---- still to map (AxioDL's backend needs them from the runtime; candidates are guesses, not wired in) ----
 * (none left for the trial link) */

#endif /* MUSYX_PC_ALIAS_H */
