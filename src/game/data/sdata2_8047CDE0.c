#include "dolphin/types.h"

#pragma section ".sdata2"
#define SDATA2 SECTION(".sdata2")

typedef struct Sdata2PaddedU16 {
    u16 value;
    u16 pad;
} Sdata2PaddedU16;

/* 0x8047CDC0-0x8047CDE0 is the menu TU pool (gs_model_sdata2_8047CDC0.c and
 * menu_exact_80103614.c). */
SDATA2 const Sdata2PaddedU16 lbl_8047CDE0 = { 0xFFFF, 0 };
SDATA2 const Sdata2PaddedU16 lbl_8047CDE4 = { 0xFFFF, 0 };
#ifdef TARGET_PC
/* The window system reads this word as four byte masks {1, 2, 4, 8} (PortMasks, window_exact_801054B8.c):
 * stored little-endian, the PC must hold 0x08040201 to have the GameCube's bytes in memory. Read as 0x01020408 the
 * masks came out {8, 4, 2, 1}, so the title's window (port mask 1) never got a key (2026-10-16). */
SDATA2 const u32 lbl_8047CDE8 = 0x08040201;
#else
SDATA2 const u32 lbl_8047CDE8 = 0x01020408;
#endif
SDATA2 const f32 lbl_8047CDEC = 1.0f;
SDATA2 const u32 lbl_8047CDF0 = 0x213A44F2;
SDATA2 const u32 lbl_8047CDF4 = 0x11272BF2;
SDATA2 const f32 lbl_8047CDF8 = 1.0f;
SDATA2 const f32 lbl_8047CDFC = 56.0f;
SDATA2 const f32 lbl_8047CE00 = 0.017453292f;
SDATA2 const f64 lbl_8047CE08 = 6.2831854820251465;
/* 0x8047CE10-0x8047CE20 holds the int-to-float bias doubles MWCC emits for
 * fn_80105634's conversions; game/window_candidate_80105634.c owns them.
 * 0x8047CE20-0x8047CE48 is the literal pool of win_sprite.c (winSeq/winSprite
 * TU); 0x8047CE48-0x8047CE70 is menu_offscreen.c's. */
