#ifndef GAME_CONSTANTS_TYPES_H
#define GAME_CONSTANTS_TYPES_H

/*
 * Type numbers: Gen 3's order (verified 2026-11-09 against the move table: a well-known move of each of the 18
 * types carries exactly this number, and species types agree, e.g. Bulbasaur 12 / 3). The game's own type labels
 * on the disc are short (ELECTR, FIGHT, PSYCHC) and stored in a different order, so the constants use the full
 * names in the Ruby / Sapphire / Emerald decomp's spelling. 9 is the "???" type (Curse); the disc shows it as "?".
 * Shadow Rush carries type 0 in the move table. Added 2026-11-09 (named constants, snatcherLeo's plan of 2026-11-08).
 */

#define TYPE_NORMAL                   0
#define TYPE_FIGHTING                 1
#define TYPE_FLYING                   2
#define TYPE_POISON                   3
#define TYPE_GROUND                   4
#define TYPE_ROCK                     5
#define TYPE_BUG                      6
#define TYPE_GHOST                    7
#define TYPE_STEEL                    8
#define TYPE_MYSTERY                  9
#define TYPE_FIRE                    10
#define TYPE_WATER                   11
#define TYPE_GRASS                   12
#define TYPE_ELECTRIC                13
#define TYPE_PSYCHIC                 14
#define TYPE_ICE                     15
#define TYPE_DRAGON                  16
#define TYPE_DARK                    17

#define TYPE_COUNT                   18

#endif /* GAME_CONSTANTS_TYPES_H */
