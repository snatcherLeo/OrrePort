/*
 * musyx_pc_data.c: MusyX's zero-initialised state, defined for the PC build with its real types (snatcherLeo, 2026-10-15:
 * "MusyX data on the PC in port/ with real types"; docs/port/musyx-evaluation.md, "Decisions").
 *
 * On the GameCube these globals are defined in the game's split data units (src/game/data/bss_*.c, sbss_*.c) as
 * blocks of their GameCube size. Several hold pointers (sample and macro tables, sequencer lists), so they are larger
 * on a 64-bit PC; here each gets its real type and therefore its PC size. Compiled with port/src/musyx_pc_alias.h
 * force-included, like our MusyX units, so the hardware-layer names come out as AxioDL's (salFrame, vs, ...).
 * The initialised tables (attenuation, DLS volume, sine, two constants) are generated into build-pc/ from the game's
 * data units by port/tools/musyx_tables.py, in native byte order.
 *
 * Array lengths: the GameCube size divided by the GameCube element size (from symbols.txt and the extern
 * declarations in src/musyx); the MusyX name in each comment comes from our source comments.
 */
#include "dolphin/types.h"
#include "musyx/runtime/s_data_types.h"
#include "musyx/runtime/seq_types.h"
#include "musyx/runtime/stream_types.h"
#include "musyx/runtime/synthdata_types.h"
#include "musyx/runtime/vsamples_types.h"

/* ---- sequencer (seq.c) ---- */
NOTE lbl_804271D0[256];          /* seqNote (0x1400 / 0x14) */
SEQ_INSTANCE lbl_804285D0[8];    /* seqInstance */
u16 lbl_80434910[8][16];         /* seqMIDIPriority */
u32 lbl_8047AEF8;                /* seq_next_id */
u8 lbl_8047AEFC;                 /* curFadeOutState */
u32 lbl_8047AF00;                /* curSeqId */
NOTE* lbl_8047AF04;              /* noteFree */
SEQ_INSTANCE* lbl_8047AF08;      /* cseq */
SEQ_INSTANCE* lbl_8047AF0C;      /* seqFreeRoot */
SEQ_INSTANCE* lbl_8047AF10;      /* seqPausedRoot */
SEQ_INSTANCE* lbl_8047AF14;      /* seqActiveRoot */

/* ---- streams (stream.c) ---- */
STREAM_INFO lbl_80435FF8[64];    /* streamInfo */
u32 lbl_8047AF60;                /* nextPublicID */
u8 lbl_8047AF64;                 /* streamCallDelay */
u8 lbl_8047AF65;                 /* streamCallCnt */

/* ---- data manager (synthdata.c) ---- */
DataTabT lbl_804378F8[256];      /* key map table (0x800 / 8) */
LayerTabT lbl_804380F8[256];     /* layer table (0xC00 / 12) */
DataTabT lbl_80438CF8[2048];     /* curve / data table (0x4000 / 8) */
FxGroupT lbl_8043D2F8[128];      /* FX group table (0x400 / 8) */
MacMainEntryT lbl_8043D6F8[512]; /* dataMacMainTab */
MacSubEntryT lbl_8043DEF8[2048]; /* dataMacSubTabmem */
LayerTabT lbl_80445F18;          /* search key for the layer table (0xC on the GameCube; one LayerTabT on the PC) */
u8 lbl_80445F24[0xC];            /* search key for an FX table entry (disc format, 10 bytes, no pointers) */

/* ---- group stack (s_data.c) ---- */
GSTACK lbl_80447860[128];        /* gs */
s16 lbl_8047AFE8;                /* sp (group stack depth) */

/* ---- state our runtime shares with AxioDL's PC code (layouts checked: musyx_layout_check) ----
 * The hardware-layer globals (salFrame, salAuxFrame, salNumVoices, salMaxStudioNum, salHooks, the message callback,
 * dspHRTFOn) are defined by AxioDL's hardware.c, which the PC build uses instead of ours (snatcherLeo's rule: audio hardware
 * -> AxioDL's version). */
VS lbl_80446F10;                 /* vs */
u32 lbl_80478BF0 = 1;            /* last_rnd: sndRand's seed; 1 on the disc and in AxioDL (verified) */
