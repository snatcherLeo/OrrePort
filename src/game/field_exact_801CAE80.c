#include "dolphin/types.h"

extern void etctoolSetPokemonNakigoe(u16 species);

void scriptSetPokemonNakigoe(u16 species) /* script native: the script's first argument arrives in r3 (likely) */
{
    etctoolSetPokemonNakigoe(species);
}
