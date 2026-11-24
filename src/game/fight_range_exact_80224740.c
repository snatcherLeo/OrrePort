/**
 * @file fight_range_exact_80224740.c
 * @brief Exact island 0x80224740 - 0x802247D0 (wsCmd18), moved out of
 *        fight_range_80211A00.c.
 */
#include "dolphin/types.h"

extern u8* lbl_8047B610;
extern u8 lbl_80478D78[8];
extern u32 ServerStatusFlag;
extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void*, void*, u8);
#endif

#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd18(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
    extern u8 lbl_8047B625;
    extern u16 lbl_80279EF4[];
  u32 uVar2;
  u32 sVar1;
  u16 loadedStatus;
  u8 cVar3;

  uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(*(u8 *)(lbl_8047B610 + 1),0));
  loadedStatus = lbl_80279EF4[lbl_80478D78[3]];
  sVar1 = loadedStatus;
  if ((loadedStatus != 0) && (cVar3 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar2), PC_PTR(sVar1)), cVar3 == 1)) {
    fn_80202810(PC_PTR(uVar2),PC_PTR(sVar1));
  }
  lbl_80478D78[3] = 0;
  lbl_8047B625 = 0;
  lbl_8047B610 += 2;
  return;
}
#pragma optimize_for_size reset
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
