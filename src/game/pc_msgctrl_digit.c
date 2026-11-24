/**
 * @file pc_msgctrl_digit.c
 * @brief OrrePort only: _msgctrlMakeDigit (0x80132834), the routine that writes a number into a text buffer, written
 *        as C from the original instructions (2026-10-17). Not matched code: the GameCube build keeps its
 *        assembly (msgctrl_candidate_80132834); the PC build had only a generated stub, so numbers in text (money,
 *        levels, counts) came out blank. Built by port/tools/firstlink.py (PORT_GAME_SOURCES), not configure.py.
 *
 * What it does (read from the code, verified against every branch of the original):
 *   - writes a 0 terminator at buffer[length - 1], then the digits backwards from there;
 *   - mode 0 and 5: signed decimal (a negative value gets a leading '-'); mode 1: unsigned decimal; mode 2: decimal
 *     padded with zeros to at least 10 digits; mode 3: hexadecimal padded to 8 digits; mode 4: decimal with a ','
 *     every three digits;
 *   - the digit characters come from a 16-entry table of u16 character codes: lbl_80363610 for mode 5 (likely the
 *     full-width digits), lbl_803635F0 otherwise; at least one digit is always written ("0" for zero);
 *   - returns a pointer to the first character of the number.
 *
 * Byte order (A, 2026-10-18): the buffer is message text, which the text engine reads big-endian (gs_msg.c,
 * DISC_U16 on every character), so every character goes in big-endian (TEXT_CHAR). The two digit tables are native
 * u16 in the PC's data (pc_data_check: 30 00 31 00 ...). The first version read them as big-endian and stored that
 * natively: two swaps, so the digits came out right, but ',' and '-' were stored natively and drew as white boxes
 * (the money "10,000" on the trainer card, golden-slice A16).
 */
#ifdef TARGET_PC

#include "dolphin/types.h"

#define TEXT_CHAR(c) ((u16)__builtin_bswap16((u16)(c))) /* a character stored in message-text (big-endian) order */

extern u16 lbl_803635F0[]; /* 16 u16 digit characters (native on the PC) */
extern u16 lbl_80363610[]; /* the alternate digit set (mode 5) */

u16* _msgctrlMakeDigit__FPUslUll(u16* buffer, s32 length, u32 value, s32 mode)
{
    s32 index = length - 1;
    u32 base = 10;
    s32 minDigits = 1;
    s32 count = 0;
    BOOL negative = FALSE;
    const u16* digits;

    buffer[index] = 0;
    switch (mode) {
    case 0:
    case 5:
        if ((s32)value < 0) {
            value = (u32)(-(s32)value);
            negative = TRUE;
        }
        break;
    case 2:
        minDigits = 10;
        break;
    case 3:
        base = 16;
        minDigits = 8;
        break;
    default: /* 1, 4 and anything else: unsigned, no padding */
        break;
    }
    digits = (mode == 5) ? lbl_80363610 : lbl_803635F0;

    while (value != 0) {
        if (mode == 4 && count != 0 && count % 3 == 0) {
            buffer[--index] = TEXT_CHAR(',');
        }
        buffer[--index] = TEXT_CHAR(digits[value % base]);
        value /= base;
        count++;
    }
    while (count < minDigits) {
        buffer[--index] = TEXT_CHAR(digits[0]);
        count++;
    }
    if (negative) {
        buffer[--index] = TEXT_CHAR('-');
    }
    return buffer + index;
}

#endif /* TARGET_PC */
