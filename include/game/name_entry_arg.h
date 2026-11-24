#ifndef GAME_NAME_ENTRY_ARG_H
#define GAME_NAME_ENTRY_ARG_H

#include "dolphin/types.h"
#include "pc/ptr32.h"

/* Name-entry session block (inputName__FPUsPUsiii builds it on its stack); the name-entry windows reach it through
 * their first argument (+0x60), and their callbacks read it at these fixed offsets. One definition for every
 * name-entry unit (2026-10-21; there were three copies). On the PC the pointer fields are 32-bit slots
 * (PTR32_FIELD; read with P32, written with P32_SET), so the block keeps its GameCube layout and the callbacks' raw
 * offsets stay right. Corrected 2026-10-21 (verified): with native 8-byte pointers every field after `name` moved, and
 * the callbacks read the wrong ones (Rui's name screen in sitting 9 crashed reading `row` at +0x24). */
typedef struct NAME_ENTRY_ARG {
    /* 0x00 */ u16 buffer[12];       /* the default name */
    /* 0x18 */ PTR32_FIELD(u16) name; /* the name being typed */
    /* 0x1C */ s32 kind;
    /* 0x20 */ s32 index;
    /* 0x24 */ PTR32_FIELD(s32) row;
    /* 0x28 */ PTR32_FIELD(s32) letter;
    /* 0x2C */ PTR32_FIELD(s32) column;
    /* 0x30 */ PTR32_FIELD(f32) fade;
    /* 0x34 */ PTR32_FIELD(s32) state;
    /* 0x38 */ PTR32_FIELD(u32) work0;
    /* 0x3C */ PTR32_FIELD(u32) work1;
    /* 0x40 */ PTR32_FIELD(u32) work2;
    /* 0x44 */ PTR32_FIELD(u32) work3;
    /* 0x48 */ PTR32_FIELD(u32) work4;
} NAME_ENTRY_ARG; /* size 0x4C */
#if defined(TARGET_PC) && !defined(__cplusplus)
_Static_assert(sizeof(NAME_ENTRY_ARG) == 0x4C, "NAME_ENTRY_ARG must keep its GameCube layout");
#endif

#endif /* GAME_NAME_ENTRY_ARG_H */
