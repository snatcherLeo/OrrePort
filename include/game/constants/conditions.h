#ifndef GAME_CONSTANTS_CONDITIONS_H
#define GAME_CONSTANTS_CONDITIONS_H

/*
 * Condition ids (joutai): the status conditions and battle effects kept on a Pokemon, a side or the field. All three
 * share one id space (pokemonIsJoutaiDataId, fightOutPokemonIsPokemonJoutaiDataId, fightSideIsJoutaiDataId and
 * fightFloorIsJoutaiDataId all take these ids); each id has a 20-byte definition (lbl_8035BBA8, 86 entries).
 *
 * Only ids the code pins down are named here (verified 2026-10-16 .. 2026-11-09; evidence in
 * docs/engine/battle-turn-flow.md, "Condition ids"):
 *   3-9      their definitions' name messages ("poison", "burn", ...), the per-turn damage and checks, and the GBA
 *            converter, which maps exactly these to the GBA game's status bits;
 *   0x0C     the move it forces (Bide);
 *   0x1D     the one-hit-KO and accuracy checks read the stored target (Lock-On / Mind Reader);
 *   0x1F-21  WS_KIERUTAME_AFTAR clears 0x1F after Fly / Bounce, 0x20 after Dig, 0x21 after Dive;
 *   0x24     doubles the next Electric move (Charge);
 *   0x2C     survives at 1 HP (Endure);
 *   0x36     set by the Choice Band's held-item effect at the end of a move (the locked move);
 *   0x48, 0x49, 0x4C  their "wore off" messages name Reflect, Light Screen and Mist.
 * Ids that are only likely (infatuation, flinch, Taunt, Hyper Mode, ...) stay numbers until checked; weather
 * (0x4E-0x55) is named during the Mt. Battle run (snatcherLeo, 2026-11-08).
 * Added 2026-11-09 (named constants, snatcherLeo's plan of 2026-11-08: "status conditions").
 */

#define CONDITION_POISON              3
#define CONDITION_BAD_POISON          4   /* Toxic; its counter stacks to 15 */
#define CONDITION_PARALYSIS           5
#define CONDITION_BURN                6
#define CONDITION_FREEZE              7
#define CONDITION_SLEEP               8
#define CONDITION_CONFUSION           9
#define CONDITION_BIDE                0x0C
#define CONDITION_LOCK_ON             0x1D  /* Lock-On / Mind Reader; stores the locked target */
#define CONDITION_IN_THE_AIR          0x1F  /* Fly, Bounce */
#define CONDITION_UNDERGROUND         0x20  /* Dig */
#define CONDITION_UNDERWATER          0x21  /* Dive */
#define CONDITION_CHARGE              0x24
#define CONDITION_ENDURE              0x2C
#define CONDITION_CHOICE_LOCK         0x36  /* Choice Band: the locked move */
#define CONDITION_REFLECT             0x48  /* side */
#define CONDITION_LIGHT_SCREEN        0x49  /* side */
#define CONDITION_MIST                0x4C  /* side */

#endif /* GAME_CONSTANTS_CONDITIONS_H */
