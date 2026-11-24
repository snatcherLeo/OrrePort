/* Calls whose C lists a float and an integer argument in a different order than the callee (OrrePort).
 *
 * On the GameCube integer arguments go in r3, r4, ... and float arguments in f1, f2, ... independently, so
 * `fadeSet(0.5f, 3)` and `fadeSet(3, 0.5f)` compile to the same instructions, and the decompiled C kept whichever
 * order its local declaration had. On x64 the position decides the register, so the PC must pass the callee's own
 * order. Each macro below keeps the original call form on the GameCube (same bytes) and the callee's order on the PC.
 * Found with `tools/local/port/pc_probe.py --float-args` (the project notes 11). Included at the end of
 * dolphin/types.h, like ptr32.h, so every file has it. */
#ifndef PC_ARGORDER_H
#define PC_ARGORDER_H

/* fadeSet(u16 type, f32 duration) (src/game/effect/fade.c; reads r3 and f1), called duration-first in 56 places */
#ifdef TARGET_PC
#define fadeSetDurationFirst(duration, type) fadeSet((type), (duration))
#else
#define fadeSetDurationFirst(duration, type) fadeSet((duration), (type))
#endif

/* fadeSetEX(u16 type, void (*init)(void), u8 snapshot, f32 duration, f32 hookDuration) (fade.c), called with the two
 * floats first (fight.c); the arguments include calls, so a direct reorder would change their order on the GameCube */
#ifdef TARGET_PC
#define fadeSetEXFloatsFirst(duration, hookDuration, type, init, snapshot) \
    fadeSetEX((type), (init), (snapshot), (duration), (hookDuration))
#else
#define fadeSetEXFloatsFirst(duration, hookDuration, type, init, snapshot) \
    fadeSetEX((duration), (hookDuration), (type), (init), (snapshot))
#endif

#endif
