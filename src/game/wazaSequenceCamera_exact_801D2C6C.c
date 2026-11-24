/**
 * @file wazaSequenceCamera_exact_801D2C6C.c
 * @brief battleCameraWazaGetOwner / battleCameraStartOwner, 0x801D2C6C - 0x801D2D28.
 *
 * Function-boundary carve of the waza camera TU (see wazaSequenceCamera.c):
 * no jump table, no pooled constant; its data is the .sbss camera state
 * (lbl_8047B3EC/F0/F4), kept extern. GC/1.3 -O4,p like the TU, no pragmas.
 */
#include "game/battle/waza_camera_stop.h"

extern u8 lbl_8047B3F4; /* waza camera enabled */

extern void battleCameraStartWaza(void* owner, void* sequence);

/* The active waza camera sequence. */
void* battleCameraWazaGetOwner(void)
{
    return lbl_8047B3EC;
}

/* battleCameraStartOwner: the battle camera's idle shot on one Pokemon or trainer (no move), started once no
 * camera animation is running, ending the previous move camera first; battleCameraStartRandom picks the owner. */
/* NAME-T3 fn_801D2C74 */
void battleCameraStartOwner(void* owner)
{
    if (lbl_8047B3F4 != 0) {
        if (lbl_8047B3F0 == NULL) {
            if (lbl_8047B3EC != NULL) {
                wazaCameraStop();
            }
#ifdef TARGET_PC
            {
                /* PC (observe only, 2026-10-28): the idle shot's camera draws go to the replay resync's camera
                 * list, like a move's (ORRE_ROLL_SEEDS, src/game/pc_debug_game.c) */
                extern s32 pc_move_camera_depth;
                pc_move_camera_depth++;
                battleCameraStartWaza(owner, NULL);
                pc_move_camera_depth--;
            }
#else
            battleCameraStartWaza(owner, NULL);
#endif
        }
    }
}
