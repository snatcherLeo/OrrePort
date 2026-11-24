/*
 * PC stand-in for the decomp's include/dolphin/dvd/dvd.h, used only by the SDK units the PC compiles from our own C
 * (the THP player and friends; port/tools/firstlink.py puts port/sdk_include first for them).
 *
 * Aurora implements DVDOpen / DVDRead / DVDClose and fills the caller's DVDFileInfo with its own PC layout: the
 * GameCube fields plus a trailing `auroraHandle` (0x60 bytes on the PC). The decomp header declares the GameCube struct
 * without that field (0x58 bytes on the PC), so the THP player's `THPPlayer.fileInfo` was too small: Aurora's handle
 * landed on the movie header that follows it, the header copy then overwrote the handle, and DVDClose freed garbage
 * (the first PC boot to reach the opening movie, 2026-10-16). Using Aurora's own declarations here keeps the two
 * the same.
 */
#ifndef DOLPHIN_DVD_DVD_H
#define DOLPHIN_DVD_DVD_H

#include "dolphin/types.h"
#include <dolphin/dvd.h> /* Aurora's: extern/aurora/include/dolphin/dvd.h */

/* Aurora's dvd.h makes DVDRead / DVDReadAsync macros for the ...Prio versions; our SDK C calls them as functions with
 * the GameCube's extra priority argument, so they go to the port's functions (pc_dvd.c; priority 2 either way). */
#undef DVDRead
#undef DVDReadAsync

#endif
