/*
 * pc_names.h: the full names beside the save's GameCube names (2.4; pc_names.cpp says how and why).
 * Platforms: plain C interface; the same everywhere.
 */
#ifndef PC_NAMES_H
#define PC_NAMES_H

#include "pc_grapheme.h" /* PC_NAME_MAX_UTF8 */

#ifdef __cplusplus
extern "C" {
#endif

/* one long name of a slot, as its extras file keeps it (UTF-8) */
typedef struct PcLongName {
    char id[48];      /* "hero", "partner", "box1"-"box3", or "nick.<personality value and trainer ID, hex>" */
    char species[64]; /* a nickname's species ("RAIKOU"); "" for the others */
    /* as typed, up to 12 visible letters. *Changed 2026-11-16 (visible letters, pc_grapheme.h):* 64 bytes held at most
     * 16 four-byte characters, so twelve emoji (or four families) were cut, mid-character, in the hub and the GameCube
     * copy's note; now sized for the longest name the keyboard can make (132 characters of up to 4 bytes) */
    char full[PC_NAME_MAX_UTF8];
    char gc[64];      /* the GameCube name the save holds */
} PcLongName;

/* a slot's long names, read from its extras file (the game needn't run); the count */
int pc_names_read_slot(int slot, PcLongName* out, int max);

#ifdef __cplusplus
}
#endif

#endif
