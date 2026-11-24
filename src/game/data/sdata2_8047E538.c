#include "dolphin/types.h"

#pragma section ".sdata2"
#define SDATA2 SECTION(".sdata2")

/*
 * .sdata2 constants continuing directly after sdata2_8047E490 and ending
 * before the private pool paired with fightTrainerAiGetHpPercent. Contains several small
 * u32 id/index tables (values in the 0x7500-0x76B5 range, likely message or
 * script ids) and a few byte/word constants. Values are reproduced verbatim
 * from the shipped binary; the exact producing structs are not yet identified.
 */
SDATA2 const u32 lbl_8047E538[6] = {
    0x00007650, 0x00007656, 0x00007596, 0x00007597, 0x00007651, 0x00007657,
};
SDATA2 const u32 lbl_8047E550[2] = { 0x0000757F, 0x00007580 };
SDATA2 const u32 lbl_8047E558[20] = {
    0x000075A1, 0x000075A2, 0x000075A4, 0x000075A5, 0x0000759D, 0x0000759F,
    0x00007659, 0x00007577, 0x00007591, 0x00007647, 0x00007593, 0x00007647,
    0x000075AE, 0x00007647, 0x000075AF, 0x0000765D, 0x00007552, 0x00007553,
    0x00007562, 0x00007563,
};
SDATA2 const u32 lbl_8047E5A8[12] = {
    0x00007558, 0x00007559, 0x0000755C, 0x0000755D, 0x00007560, 0x00007561,
    0x0000753E, 0x000075AD, 0x00007557, 0x0000761C, 0x000075F1, 0x000075F2,
};
SDATA2 const u32 lbl_8047E5D8[8] = {
    0x000075D1, 0x000075D3, 0x000076B5, 0x000076B6,
    0x0000760C, 0x0000761A, 0x0000754D, 0x0000754E,
};
#ifdef TARGET_PC
/* PC: these are read in smaller pieces than they are written here (verified 2026-10-18), so on the little-endian PC
 * the values are given with their halves / bytes in the order the readers need:
 * - lbl_8047E5F8 is read as u16[4] (wsCmd77, Protect / Detect / Endure): the success chances FFFF 7FFF 3FFF 1FFF
 *   by repeat count. The plain words gave 7FFF for the first use, so every first Protect failed half the time
 *   (snatcherLeo's playtest, 2026-10-18: Raichu's Protect "failed").
 * - lbl_8047E600 / lbl_8047E604 are copied as one u16 / u32 into byte lists ({1, 2, 5}, {1, 2, 3, 4, 5}), which came
 *   out reversed. */
SDATA2 const u32 lbl_8047E5F8[2] = { 0x7FFFFFFF, 0x1FFF3FFF };
SDATA2 const u16 lbl_8047E600 = 0x0201;
SDATA2 const u8 lbl_8047E602 = 0x05;
SDATA2 const u32 lbl_8047E604 = 0x04030201;
#else
SDATA2 const u32 lbl_8047E5F8[2] = { 0xFFFF7FFF, 0x3FFF1FFF };
SDATA2 const u16 lbl_8047E600 = 0x0102;
SDATA2 const u8 lbl_8047E602 = 0x05;
SDATA2 const u32 lbl_8047E604 = 0x01020304;
#endif
SDATA2 const u8 lbl_8047E608 = 0x05;
SDATA2 const f64 lbl_8047E610 = 4.503599627370496e+15;
