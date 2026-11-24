#ifndef GAME_GS_MSG_TASK_H
#define GAME_GS_MSG_TASK_H

#include "dolphin/types.h"

/*
 * One message task: a block of text being laid out and drawn (GSmsg, src/game/gs_msg.c). The message system
 * allocates taskCount of these (GSmsgInit) and keeps them at MessageSystem.tasks; the print helpers use a few
 * static ones. 0x68 bytes on GameCube.
 *
 * Offsets and types come from every raw access in gs_msg.c (2026-10-15 scan: msgTaskDraw, GSmsgInit, the print
 * and layout helpers). Names are guesses where marked; unknown fields keep their offset in the name. Several
 * fields are pointers (text cursor, text stack, 0x28 / 0x2C / 0x54), so the size and layout differ on a
 * 64-bit host: code must use these fields and sizeof(MessageTask), never 0x68 or raw offsets.
 */
typedef struct MessageTask {
    /* 0x00 */ u8 unk00[3];
    /* 0x03 */ s8 unk03;
    /* 0x04 */ f32 startX;          /* guess: layout origin, copied to cursorX / cursorY at draw start */
    /* 0x08 */ f32 startY;
    /* 0x0C */ f32 cursorX;         /* pen position while laying out glyphs */
    /* 0x10 */ f32 cursorY;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ s16 unk18;
    /* 0x1A */ s16 unk1A;
    /* 0x1C */ u32 unk1C;
    /* 0x20 */ u16 unk20;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 unk23;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ PTR32_FIELD(u8) unk28; /* pointer fields are 32-bit slots on the PC too (pc/ptr32.h): code steps tasks by 0x68 */
    /* 0x2C */ PTR32_FIELD(u8) unk2C;
    /* 0x30 */ PTR32_FIELD(u16) textCursor;     /* current position in the UTF-16 text */
    /* 0x34 */ PTR32_FIELD(u16) textStack[3];   /* guess: saved cursors for nested strings (inserted names / values) */
    /* 0x40 */ s8 textDepth;        /* guess: entries used in textStack */
    /* 0x41 */ u8 unk41;
    /* 0x42 */ u8 unk42;
    /* 0x43 */ u8 unk43;
    /* 0x44 */ u8 unk44;
    /* 0x45 */ u8 unk45;            /* set from draw flags & 0x30 in msgTaskDraw */
    /* 0x46 */ u8 unk46;
    /* 0x47 */ u8 unk47;
    /* 0x48 */ u8 unk48[3];
    /* 0x4B */ u8 mode;             /* guess: layout state (msgTaskDraw tests 2) */
    /* 0x4C */ f32 unk4C;
    /* 0x50 */ f32 unk50;
    /* 0x54 */ PTR32_FIELD(u8) unk54;
    /* 0x58 */ u8 color[4];         /* guess: text colour RGBA */
    /* 0x5C */ f32 unk5C;
    /* 0x60 */ f32 scaleX;          /* 1.0 after GSmsgInit */
    /* 0x64 */ f32 scaleY;
} MessageTask; /* 0x68 on GameCube */

#endif /* GAME_GS_MSG_TASK_H */
