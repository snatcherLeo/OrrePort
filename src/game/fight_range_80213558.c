/**
 * @file fight_range_80213558.c
 * @brief Fight turn-end joutai countdown carve, 0x80213558 - 0x802136A4
 *        (_fightSeqTurnEndSubFightOutPokemon), linked.
 *
 * Standalone copy of the shared candidate body, built at -O4,s
 * (configure.py) in place of the shared file's optimize_for_size pragma.
 */
#include "dolphin/types.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"

u32 _fightSeqTurnEndSubFightOutPokemon(u32 r3, u32 r4, char* r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonGetJoutaiMigawariHp();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80201FDC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNowKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
    extern void fightOutPokemonInitOneTurn();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonSetStatus();
#endif
  u16 status;
  s32 current;
  s32 count;
  s32 next;
  int iVar1;

  if (*r5 == 1) {
    fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(0x2b));
    fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(CONDITION_ENDURE));
  }
  else {
    fightOutPokemonInitOneTurn(r3);
    fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(0x32));
    fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(0x37));
    fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(0x33));
    status = (u16)pokemonGetStatus(PC_PTR(r3),0,POKEMON_STATUS_TURNS_ON_FIELD,0);
    if (status != 0) {
      pokemonSetStatus(PC_PTR(r3),0,POKEMON_STATUS_TURNS_ON_FIELD,0,(u16)(status - 1));
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x12)) == 1) {
      current = fightOutPokemonGetNowKaisuuJoutaiDataId(PC_PTR(r3),PC_PTR(0x12));
      count = fightOutPokemonGetKaisuuJoutaiDataId(PC_PTR(r3),PC_PTR(0x12));
      current = (s8)current;
      count = (s8)count;
      next = (s8)(current + 1);
      if (next >= count) {
        fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(0x12));
      }
      else {
        fn_80201FDC(PC_PTR(r3),PC_PTR(0x12),next);
      }
    }
  }
  if ((fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(r3),PC_PTR(0x14)) == 1) &&
      (iVar1 = fightOutPokemonGetJoutaiMigawariHp(PC_PTR(r3)), iVar1 <= 0)) {
    fightOutPokemonWriteJoutaiDataId(PC_PTR(r3),PC_PTR(0x14));
  }
  return 1;
}
