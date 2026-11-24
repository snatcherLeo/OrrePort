/*
 * layout_check.c: the reader core's offsets checked against the decomp's own record layouts, at compile time.
 *
 * Why (the Studio review's decision 3, 2026-11-17): the core reads each record field by field (big-endian, GameCube
 * padding), and the decomp's headers are the reference. If a header's field moves (a layout fix in the decomp), this
 * file stops compiling until the core's offsets in species.cpp are fixed too, so Studio and the game never disagree
 * about a record. Compiled as C with TARGET_PC, so the header's DISC_STRUCT is GCC's big-endian storage order, which
 * doesn't change offsets. GCC-only (scalar_storage_order); a Clang build (macOS) would need DISC_STRUCT empty for
 * this file, which the header already does when TARGET_PC is unset (likely; not tried).
 * Nothing here runs: the file compiles to nothing.
 */
#include <stddef.h>

#include "dolphin/types.h"
#include "game/pokemon_data.h"

#define CHECK(type, field, off) _Static_assert(offsetof(type, field) == (off), #type "." #field " moved: update species.cpp")

_Static_assert(sizeof(PokemonData) == 0x11C, "PokemonData size changed: update species.cpp (kSpeciesSize)");
CHECK(PokemonData, growDataId, 0x000);
CHECK(PokemonData, get, 0x001);
CHECK(PokemonData, sexRatio, 0x002);
CHECK(PokemonData, giveExp, 0x006);
CHECK(PokemonData, initFriend, 0x008);
CHECK(PokemonData, height, 0x00A);
CHECK(PokemonData, weight, 0x00C);
CHECK(PokemonData, voice, 0x00E);
CHECK(PokemonData, numPokemon, 0x012);
CHECK(PokemonData, numZukan, 0x014);
CHECK(PokemonData, name, 0x018);
CHECK(PokemonData, typeName, 0x01C);
CHECK(PokemonData, doc, 0x020);
CHECK(PokemonData, pkxDataId, 0x02C);
CHECK(PokemonData, zokuseiDataId, 0x030);
CHECK(PokemonData, tokuseiDataId, 0x032);
CHECK(PokemonData, itemDataId, 0x070);
CHECK(PokemonData, basis, 0x084);
CHECK(PokemonData, giveEffort, 0x090);
CHECK(PokemonData, sinka, 0x09C);
CHECK(PokemonData, getWaza, 0x0BA);
CHECK(PokemonData, face, 0x10C);
_Static_assert(sizeof(PokemonDataEvolution) == 6, "PokemonDataEvolution size changed");
CHECK(PokemonDataEvolution, buff, 2);
CHECK(PokemonDataEvolution, pokemonDataId, 4);
_Static_assert(sizeof(PokemonDataWazaLearn) == 4, "PokemonDataWazaLearn size changed");
CHECK(PokemonDataWazaLearn, dataId, 2);
_Static_assert(sizeof(PokemonDataFace) == 8, "PokemonDataFace size changed");
CHECK(PokemonDataFace, statusFaceMenuSpriteId, 2);
CHECK(PokemonDataFace, pokebodyId, 4);

/* keeps the translation unit non-empty for strict compilers */
int orre_core_layout_checked = 1;
