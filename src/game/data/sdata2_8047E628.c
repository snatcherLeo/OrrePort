#include "dolphin/types.h"

#pragma section ".sdata2"
#define SDATA2 SECTION(".sdata2")

#ifdef TARGET_PC
/* PC: read as two u16 move ids each (U16Pair, fight_range_8023B498.c, verified 2026-10-18), so they are written as u16
 * pairs: the u32 words gave the halves swapped on the little-endian PC (moves 0x5C / 0x105 and 0x4D / 0x8B, likely
 * Toxic / Will-O-Wisp and Poison Powder / Poison Gas: the trainer AI's status-move check). */
SDATA2 const u16 lbl_8047E628[2] = { 0x005C, 0x0105 };
SDATA2 const u16 lbl_8047E62C[2] = { 0x004D, 0x008B };
#else
SDATA2 const u32 lbl_8047E628 = 0x005C0105;
SDATA2 const u32 lbl_8047E62C = 0x004D008B;
#endif
SDATA2 const f32 lbl_8047E630 = 0.5f;
SDATA2 const f32 lbl_8047E634 = 0.8f;
SDATA2 const f32 lbl_8047E638 = 0.1f;
SDATA2 const f32 lbl_8047E63C = 0.3f;
SDATA2 const f32 lbl_8047E640[2] = { 0.25f, 0.0f };
SDATA2 const f32 lbl_8047E648 = 1.0f;
SDATA2 const f32 lbl_8047E64C = 0.5f;
SDATA2 const f32 lbl_8047E650[2] = { 0.25f, 0.0f };
