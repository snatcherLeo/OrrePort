#ifndef GAME_CONSTANTS_GENDER_H
#define GAME_CONSTANTS_GENDER_H

/*
 * Gender values, as pokemonGetSex returns them (verified 2026-11-09 from its code: 2 for no Pokemon or a genderless
 * species, 0 / 1 for the always-male / always-female ratios, otherwise 0 when the personality value's low byte is at
 * least the species' gender ratio, else 1). Note the GBA games use 0 / 0xFE / 0xFF instead.
 * Added 2026-11-09 (named constants, snatcherLeo's plan of 2026-11-08).
 */

#define GENDER_MALE                   0
#define GENDER_FEMALE                 1
#define GENDER_GENDERLESS             2

#endif /* GAME_CONSTANTS_GENDER_H */
