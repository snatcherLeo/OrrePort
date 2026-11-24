#ifndef GAME_GS_BOUND_OFFSETS_H
#define GAME_GS_BOUND_OFFSETS_H

/* Byte offsets of a model bounding box's minimum, maximum and size vectors (GSbound, game/gs_model_struct.h), for
 * code that reads the box through GSmodelGetBound as raw bytes. GSbound holds three pointers before the box, 8 bytes
 * each on the PC, so the box sits 0x10 bytes further in there; gs_model_struct.h checks these against offsetof.
 * Corrected 2026-10-21 (verified): the move camera read the GameCube offsets on the PC, so every random battle
 * shot took its height and size from the wrong bytes (P01's Willie battle: height 4.36 vs Dolphin's 20.82); snatcherLeo saw
 * "that weird side profile". */
#ifdef TARGET_PC
#define GS_BOUND_OFF_MIN 0x20
#define GS_BOUND_OFF_MAX 0x2C
#define GS_BOUND_OFF_SIZE 0x38
#else
#define GS_BOUND_OFF_MIN 0x10
#define GS_BOUND_OFF_MAX 0x1C
#define GS_BOUND_OFF_SIZE 0x28
#endif

#endif /* GAME_GS_BOUND_OFFSETS_H */
