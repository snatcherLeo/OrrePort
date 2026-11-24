#include "dolphin/types.h"

#pragma section ".sdata2"
#define SDATA2 SECTION(".sdata2")

/*
 * Name-entry, worldmap and shop .sdata2 constants after gs_title_80023DA8.c's
 * pool (split from sdata2_8047B8A0.c at 0x8047B920): the list-id table at
 * B920, packed route-id words at B928/B92C, and the map animation/phase
 * constants.
 */
SDATA2 const u32 lbl_8047B920[2] = { 0, 0x00002EF7u };
#ifdef TARGET_PC
/* PC: the name entry reads both words as one u16[4] (NameEntryCharSetIds, menuNameEntry.c: `ids = lbl_8047B928`, an
 * 8-byte copy that also takes lbl_8047B92C; verified 2026-10-18), so lbl_8047B928 holds all four ids as u16 (also no
 * longer copied past its end), and lbl_8047B92C keeps its own two in PC order for word readers. */
SDATA2 const u16 lbl_8047B928[4] = { 0x040A, 0x040B, 0x040B, 0x040C };
SDATA2 const u32 lbl_8047B92C = 0x040C040Bu;
#else
SDATA2 const u32 lbl_8047B928 = 0x040A040Bu;
SDATA2 const u32 lbl_8047B92C = 0x040B040Cu;
#endif
SDATA2 const f32 lbl_8047B930 = 0.0f;
SDATA2 const f32 lbl_8047B934 = 1.0f;
SDATA2 const f32 lbl_8047B938 = 255.0f;
SDATA2 const f32 lbl_8047B93C = 0.25f;
SDATA2 const f32 lbl_8047B940 = 0.5f;
