/**
 * @file msgctrl_r49_8013182C_o2.c
 * @brief msgctrl.c carve, 0x8013182C - 0x80131A34: msgctrlTime.
 *
 * Built with the TU's -O4,p and the peephole pass off, like the other
 * msgctrl.c carves.
 */
#include "dolphin/types.h"

extern u32 lbl_8047AE84;
#ifdef TARGET_PC
extern u32 lbl_8047AEA8; /* PC: kept 32-bit, as its other declarations (effect_util_types.h) and its disc slot */
#else
extern u16* lbl_8047AEA8;
#endif
extern u16 lbl_80427030[];
extern u16 lbl_80427050[];

extern u16* _msgctrlMakeDigit__FPUslUll(void* table, u32 stride, u32 value, u32 type);

/* Play time (in seconds) as "[H]HH:MM": each digit is drawn through
 * _msgctrlMakeDigit and its glyph copied into the output string, which is
 * returned like the other message-control callbacks' strings. Returning the
 * buffer keeps its address live in r3 past the terminator store, which is
 * what gives retail's final-block register choice. */
u16* msgctrlTime(void)
{
    u32 minutes;
    u32 hours;
    s32 i;
    u16* glyph;

    hours = lbl_8047AE84 / 3600;
    minutes = (lbl_8047AE84 % 3600) / 60;
    i = 0;

    if (hours >= 100) {
        glyph = _msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, hours / 100, 0);
        lbl_8047AEA8 = PC_PTR32_IMPLICIT(glyph);
        lbl_80427050[i++] = *glyph;
        hours = hours % 100;
    }

    glyph = _msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, hours / 10, 0);
    lbl_8047AEA8 = PC_PTR32_IMPLICIT(glyph);
    lbl_80427050[i++] = *glyph;

    glyph = _msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, hours % 10, 0);
    lbl_8047AEA8 = PC_PTR32_IMPLICIT(glyph);
    lbl_80427050[i++] = *glyph;

#ifdef TARGET_PC
    /* PC: the buffer is message text, which the text engine reads big-endian (gs_msg.c); the digits copied in above
     * are already in that order (_msgctrlMakeDigit), a native ':' drew as a white box (golden-slice A16) */
    lbl_80427050[i++] = (u16)__builtin_bswap16(':');
#else
    lbl_80427050[i++] = ':';
#endif

    glyph = _msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, minutes / 10, 0);
    lbl_8047AEA8 = PC_PTR32_IMPLICIT(glyph);
    lbl_80427050[i++] = *glyph;

    glyph = _msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, minutes % 10, 0);
    lbl_8047AEA8 = PC_PTR32_IMPLICIT(glyph);
    lbl_80427050[i++] = *glyph;
    lbl_80427050[i] = 0;
    return lbl_80427050;
}
