/**
 * @file gs_pokemon_summary_r57b_8001501C_prefix.c
 * @brief gs_pokemon_summary carve, 0x8001501C - 0x800150E4: fn_8001501C
 *        (draw the summary label) and fn_80015050 (call the current page's
 *        draw handler with its cursor position).
 *
 * Text only, GC/1.3 -O4,p with the peephole pass off for the whole unit (the
 * source wrapped both in `#pragma peephole off`). The page table
 * lbl_80266918 is the TU's six-entry .rodata table, read by several units.
 */
#include "dolphin/types.h"

typedef s32 (*SummaryDrawHandler)(u8* context, u8* param, u16* cursor);

/* One summary page (0x4C bytes; see SummaryPageEntry in gs_pokemon_summary.c). */
typedef struct SummaryPageEntry {
    u8 displayColor[3];
    u8 unk_03;
    s32 dataSource;
    u8 unk_08[4];
    s32 colorMatchId;
    s32 gaugeMatchId;
    u8 unk_14[4];
#ifdef TARGET_PC
    u32 drawHandler; /* PC: a 4-byte cell (the table keeps the GameCube layout, pcdata_types.txt) */
#else
    SummaryDrawHandler drawHandler;
#endif
    u32 messageId;
    u8 unk_20[0x2C];
} SummaryPageEntry;
#ifdef TARGET_PC
_Static_assert(sizeof(SummaryPageEntry) == 0x4C, "the page table keeps the GameCube layout on the PC");
#endif

extern SummaryPageEntry lbl_80266918[6];
extern u32 lbl_8047A2DC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor(s32 x, s32 y, s32 flags, u32 color);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 cursorBiosGetPos(u32 id);
#endif

s32 fn_8001501C(void)
{
    GSmsgPrintColor(0, 0, -1, lbl_8047A2DC);
    return 0;
}

s32 fn_80015050(u8* context, u8* param)
{
    SummaryPageEntry* entry = lbl_80266918;
    SummaryDrawHandler handler;
    u16 cursor;

    entry += (s8)context[0x95];
#ifdef TARGET_PC
    handler = (SummaryDrawHandler)PC_PTR(entry->drawHandler);
#else
    handler = entry->drawHandler;
#endif

    if (handler != NULL) {
        cursor = cursorBiosGetPos((u16)entry->messageId) >> 16;
        return handler(context, param, &cursor);
    }
    return 0;
}
