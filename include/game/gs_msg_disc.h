#ifndef GAME_GS_MSG_DISC_H
#define GAME_GS_MSG_DISC_H

#include "dolphin/types.h"
#include "pc/disc_types.h"

/*
 * Message banks and font files as they are loaded from the disc (docs/engine/messages.md). On the PC they stay
 * big-endian (DISC_STRUCT, the project notes 6.3). The game links banks and fonts into lists in place:
 * next / previous are 32-bit slots inside the loaded file (DISC_PTR, read with DP, written with DP_SET).
 * Shared by gs_msg.c and gs_msg_exact_800FDF1C.c (2026-10-15; B's first boot crashed in GSmsgFontOpen reading
 * the font header natively).
 */

/* Retail uses eight-byte entries after the 0x10-byte group header. */
struct DISC_STRUCT MessageEntry {
    u32 key;
    u32 offset;
};
struct DISC_STRUCT MessageGroup {
    u16 id;
    u8 reserved_02;
    u8 fontId;
    u16 count;
    u8 reserved_06[2];
    DISC_PTR(struct MessageGroup) next;     /* 0x08 */
    DISC_PTR(struct MessageGroup) previous; /* 0x0C */
    struct MessageEntry entries[1];         /* 0x10 */
};

/* Font files are chains of eight-byte headers, each followed by a glyph bank. */
struct DISC_STRUCT GlyphEntry {
    u16 code;   /* 0x00 */
    u8 width;   /* 0x02 */
    u8 height;  /* 0x03 */
    u32 offset; /* 0x04 */
};
struct DISC_STRUCT FontBank {
    u16 count;                          /* 0x00: glyphs, sorted by code */
    u8 reserved_02[2];                  /* 0x02 */
    u32 dataOffset;                     /* 0x04 */
    DISC_PTR(struct FontBank) next;     /* 0x08 */
    DISC_PTR(struct FontBank) previous; /* 0x0C */
    struct GlyphEntry glyphs[1];        /* 0x10 */
};
struct DISC_STRUCT FontFileHeader {
    u16 id;
    u8 width;
    u8 height;
    u32 nextOffset;
};

#endif
