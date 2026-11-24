/**
 * @file fight_range_8023565C_exact.c
 * @brief Exact pure-C fight-engine island.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#define fn_8011BEB4 wazaGetStatus
#define fn_80122DDC pokemonIsJoutaiNormal
#define fn_8012640C pokemonGetStatus
#define fn_801F37B0 fightFloorLoopValidFightOutPokemon
#define fn_801F453C fightFloorGetNowTenkouDataId
#define fn_801F54A4 fightFloorGetStatus
#define fn_801F8424 fightTrainerIsAllyFightTargetPtr
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_801FFEC8 fightOutPokemonCheckCanOutOkWazaBanme
#define fn_802010C8 fightOutPokemonIsJoutaiKie
#define fn_8020156C fightOutPokemonGetAllAbiCnt
#define fn_80203C5C fightOutPokemonIsJoutaiKaragenki
#define fn_80205BE8 fightPokemonGetPokemonPtr
#define fn_80207B8C fightOutPokemonGetZokuseiDataId


/* fightTrainerAiIsAbicntUnderMin: TRUE when the Pokemon's total stat stages (fightOutPokemonGetAllAbiCnt) are at
 * or below the trainer AI's minimum (AI data 0x2F, fightTrainerAiDataBiosGetAbicntMinValue). */
/* NAME-T3 fn_8023565C */
u32 fightTrainerAiIsAbicntUnderMin(u32 r3, u32 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
    extern u32 fn_8020156C();
  u16 uVar1;
  u16 uVar2;
  u32 uVar3;
  u16 uVar4;

  uVar1 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  uVar1 = fn_801FB1C0(0,uVar1,2,0);
  uVar2 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  fn_801FB1C0(0,uVar2,2,0);
  uVar3 = fn_8020156C(r4);
  uVar4 = fn_801FB1C0(0,uVar1,0x2f,0);
  return (u8)((s32)(u8)uVar4 >= (s32)(uVar3 & 0xffff));
}


/* fightTrainerAiIsAbicntOverMax: TRUE when the Pokemon's total stat stages are at or above the trainer AI's
 * maximum (AI data 0x2E, fightTrainerAiDataBiosGetAbicntMaxValue). */
/* NAME-T3 fn_80235714 */
u8 fightTrainerAiIsAbicntOverMax(u32 r3, u32 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
    extern u32 fn_8020156C();
  u16 uVar1;
  u16 uVar2;
  u32 uVar3;
  u16 uVar4;

  uVar1 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  uVar1 = fn_801FB1C0(0,uVar1,2,0);
  uVar2 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  fn_801FB1C0(0,uVar2,2,0);
  uVar3 = fn_8020156C(r4);
  uVar4 = fn_801FB1C0(0,uVar1,0x2e,0);
  return (s32)(u8)uVar4 <= (s32)(u16)uVar3;
}


u8 fightTrainerAiGetAbicntAvoid(u32 ctx, u32 param1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8  fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u16 slot1;
    u16 slot2;
    u8 val;
    u8 x;

    slot1 = (u16)fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0);
    fn_801FB1C0(0, slot1, 2, 0);
    val = (u8)fn_8012640C(PC_PTR(param1), 0, 0xec, 0);

    slot2 = (u16)fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0);
    slot2 = (u16)fn_801FB1C0(0, slot2, 2, 0);

    if ((u8)fn_801FB1C0(0, slot2, 0x24, 0) == 1) {
        x = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(param1), PC_PTR(0x19));
    } else {
        x = 0;
    }

    if (x == 1) {
        if (val > 6) {
            val = 6;
        }
    }
    return val;
}

u8 fightTrainerAiGetAbicntAverage(void* param1, void* param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    u16 slot = (u16)fn_801FB1C0(param1, 0, 0x43, 0);
    fn_801FB1C0(0, slot, 2, 0);
    return (u8)(u32)fn_8012640C(param2, 0, 0xeb, 0);
}

u8 fightTrainerAiGetAbicntNimbleness(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    u16 slot = (u16)fn_801FB1C0(PC_PTR(param1), 0, 0x43, 0);
    fn_801FB1C0(0, slot, 2, 0);
    return (u8)(u32)fn_8012640C(PC_PTR(param2), 0, 0xea, 0);
}

/* fightTrainerAiGetAbicntSpeDef: the field Pokemon's Sp. Def stage (status 0xE9). */
/* NAME-T3 fn_80235974 */
u8 fightTrainerAiGetAbicntSpeDef(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    u16 slot = (u16)fn_801FB1C0(PC_PTR(param1), 0, 0x43, 0);
    fn_801FB1C0(0, slot, 2, 0);
    return (u8)(u32)fn_8012640C(PC_PTR(param2), 0, 0xe9, 0);
}

/* fightTrainerAiGetAbicntSpeAtk: the field Pokemon's Sp. Atk stage (status 0xE8). */
/* NAME-T3 fn_802359D8 */
u8 fightTrainerAiGetAbicntSpeAtk(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    u16 slot = (u16)fn_801FB1C0(PC_PTR(param1), 0, 0x43, 0);
    fn_801FB1C0(0, slot, 2, 0);
    return (u8)(u32)fn_8012640C(PC_PTR(param2), 0, 0xe8, 0);
}

u8 fightTrainerAiGetAbicntPhyDef(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    u16 slot = (u16)fn_801FB1C0(PC_PTR(param1), 0, 0x43, 0);
    fn_801FB1C0(0, slot, 2, 0);
    return (u8)(u32)fn_8012640C(PC_PTR(param2), 0, 0xe7, 0);
}

u8 fightTrainerAiGetAbicntPhyAtk(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    u16 slot = (u16)fn_801FB1C0(PC_PTR(param1), 0, 0x43, 0);
    fn_801FB1C0(0, slot, 2, 0);
    return (u8)(u32)fn_8012640C(PC_PTR(param2), 0, 0xe6, 0);
}


u8 fightTrainerAiGetNowTenkou(void* ctx, u32 arg2, u32 arg3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F453C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F37B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTrainerAiFindTokuseiCB();
#endif
    u8 ret = fn_801F453C(PC_PTR(arg2), 0);
    if ((u8)arg3 == 1) {
        u32 msg1[5];
        msg1[0] = 0xd;
        msg1[1] = 0;
        msg1[2] = 0;
        msg1[3] = 0;
        msg1[4] = PC_PTR32(ctx);
        fn_801F37B0(PC_PTR(arg2), fightTrainerAiFindTokuseiCB, &msg1[0], 0);
        if (msg1[1] != 0) {
            return 0;
        }
        {
            u32 msg2[5];
            msg2[0] = 0x4d;
            msg2[1] = 0;
            msg2[2] = 0;
            msg2[3] = 0;
            msg2[4] = PC_PTR32(ctx);
            fn_801F37B0(PC_PTR(arg2), fightTrainerAiFindTokuseiCB, &msg2[0], 0);
            if (msg2[1] != 0) {
                return 0;
            }
        }
    }
    return ret;
}


u32 fn_80235BE4(u32 trainer, u32 floor, u32 fightOut, u32* outValue)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerIsAllyFightTargetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiFindTokuseiCB();
#endif
  u8 bVar1;
  u8 bVar2;
  u32 iVar3;
  u32 iVar4;
  u32 iVar5;
  u8 cVar10;
  u8 flag;
  u16 uVar7;
  u16 uVar8;
  u32 sVar9;
  u32 uVar6;
  u32 check17[5];
  u32 check47[5];
  u32 check2A[5];

  bVar1 = 0;
  bVar2 = 0;
  cVar10 = fightOutPokemonCheckValid(PC_PTR(fightOut));
  if (cVar10 == 0) {
    return 0;
  }
  check17[0] = 0x17;
  check17[1] = 0;
  check17[2] = 2;
  check17[3] = fightOut;
  check17[4] = trainer;
  fightFloorLoopValidFightOutPokemon(PC_PTR(floor), fightTrainerAiFindTokuseiCB, check17, 0);
  iVar5 = check17[1];
  check47[0] = 0x47;
  check47[1] = 0;
  check47[2] = 2;
  check47[3] = fightOut;
  check47[4] = trainer;
  fightFloorLoopValidFightOutPokemon(PC_PTR(floor), fightTrainerAiFindTokuseiCB, check47, 0);
  iVar4 = check47[1];
  check2A[0] = 0x2a;
  check2A[1] = 0;
  check2A[2] = 0;
  check2A[3] = fightOut;
  check2A[4] = trainer;
  fightFloorLoopValidFightOutPokemon(PC_PTR(floor), fightTrainerAiFindTokuseiCB, check2A, 0);
  iVar3 = check2A[1];
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  fightTrainerGetStatus(0,uVar7,2,0);
  uVar7 = fightFloorGetStatus(0,0,0x14,0);
  uVar8 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar8 = fightTrainerGetStatus(0,uVar8,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar8,0x2a,0);
  if (cVar10 == 1) {
    cVar10 = fightTrainerIsAllyFightTargetPtr(PC_PTR(trainer),PC_PTR(fightOut),uVar7);
    if (cVar10 == 0) {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),0);
    }
    else {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),0);
    }
  }
  else {
    sVar9 = 9;
  }
  if ((u16)sVar9 == 2) {
    goto element_two_true;
  }
  uVar7 = fightFloorGetStatus(0,0,0x14,0);
  uVar8 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar8 = fightTrainerGetStatus(0,uVar8,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar8,0x2a,0);
  if (cVar10 == 1) {
    cVar10 = fightTrainerIsAllyFightTargetPtr(PC_PTR(trainer),PC_PTR(fightOut),uVar7);
    if (cVar10 == 0) {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),1);
    }
    else {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),1);
    }
  }
  else {
    sVar9 = 9;
  }
  if ((u16)sVar9 != 2) {
    goto element_two_false;
  }
element_two_true:
  flag = 1;
  goto element_two_done;
element_two_false:
  flag = 0;
element_two_done:
  if (flag == 1) {
    goto set_first_flag;
  }
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  fightTrainerGetStatus(0,uVar7,2,0);
  fightFloorGetStatus(0,0,0x14,0);
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar7 = fightTrainerGetStatus(0,uVar7,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar7,0x2b,0);
  if (cVar10 == 1) {
    uVar6 = fightOutPokemonGetTokuseiDataId(PC_PTR(fightOut));
  }
  else {
    uVar6 = 0;
  }
  uVar6 = __cntlzw(0x1a - (uVar6 & 0xffff));
  if ((uVar6 >> 5 & 0xff) != 1) {
    goto first_flag_done;
  }
set_first_flag:
  bVar1 = 1;
first_flag_done:
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  fightTrainerGetStatus(0,uVar7,2,0);
  uVar7 = fightFloorGetStatus(0,0,0x14,0);
  uVar8 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar8 = fightTrainerGetStatus(0,uVar8,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar8,0x2a,0);
  if (cVar10 == 1) {
    cVar10 = fightTrainerIsAllyFightTargetPtr(PC_PTR(trainer),PC_PTR(fightOut),uVar7);
    if (cVar10 == 0) {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),0);
    }
    else {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),0);
    }
  }
  else {
    sVar9 = 9;
  }
  if ((u16)sVar9 == 8) {
    goto element_eight_true;
  }
  uVar7 = fightFloorGetStatus(0,0,0x14,0);
  uVar8 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar8 = fightTrainerGetStatus(0,uVar8,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar8,0x2a,0);
  if (cVar10 == 1) {
    cVar10 = fightTrainerIsAllyFightTargetPtr(PC_PTR(trainer),PC_PTR(fightOut),uVar7);
    if (cVar10 == 0) {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),1);
    }
    else {
      sVar9 = fightOutPokemonGetZokuseiDataId(PC_PTR(fightOut),1);
    }
  }
  else {
    sVar9 = 9;
  }
  if ((u16)sVar9 != 8) {
    goto element_eight_false;
  }
element_eight_true:
  flag = 1;
  goto element_eight_done;
element_eight_false:
  flag = 0;
element_eight_done:
  if (flag == 1) {
    bVar2 = 1;
  }
LAB_00233098:
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar7 = fightTrainerGetStatus(0,uVar7,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar7,0x24,0);
  if (cVar10 == 1) {
    cVar10 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(fightOut),PC_PTR(0x16));
  }
  else {
    cVar10 = 0;
  }
  if (cVar10 == 1) {
    goto return_one;
  }
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar7 = fightTrainerGetStatus(0,uVar7,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar7,0x24,0);
  if (cVar10 == 1) {
    cVar10 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(fightOut),PC_PTR(0xe));
  }
  else {
    cVar10 = 0;
  }
  if (cVar10 == 1) {
    goto return_one;
  }
  uVar7 = fightTrainerGetStatus(PC_PTR(trainer),0,0x43,0);
  uVar7 = fightTrainerGetStatus(0,uVar7,2,0);
  cVar10 = fightTrainerGetStatus(0,uVar7,0x24,0);
  if (cVar10 == 1) {
    cVar10 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(fightOut),PC_PTR(0x25));
  }
  else {
    cVar10 = 0;
  }
  if (cVar10 != 1) {
    goto check_outputs;
  }
return_one:
  return 1;

check_outputs:
  if (iVar5 != 0) {
    if (outValue != (void *)0) {
      *outValue = iVar5;
    }
    return 2;
  }
  if ((iVar4 != 0) && (!bVar1)) {
    if (outValue != (void *)0) {
      *outValue = iVar4;
    }
    return 2;
  }
  if ((iVar3 != 0) && (bVar2 == 1)) {
    if (outValue != (void *)0) {
      *outValue = iVar3;
    }
    return 2;
  }
  return 0;
}


/* fightTrainerAiFindTokuseiCB: fightFloorLoopValidFightOutPokemon callback. state[0] = the ability wanted,
 * state[2] = which side (0 any but state[3], 1 / 2 relative to state[3]), state[4] = the AI's trainer; stops at the
 * first Pokemon whose ability (only if the AI may know abilities, 0x2B) matches and stores it in state[1]. Used for
 * Cloud Nine / Air Lock (13 / 77) by fightTrainerAiGetNowTenkou. */
/* NAME-T3 fn_80236268 */
u32 fightTrainerAiFindTokuseiCB(u32 attacker, u32 unused, u32* state)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
    u32 originalAttacker = attacker;
    u32 reference = state[3];
    u32 trainer;
    u32 relative;
    u32 attackerSide;
    s32 mode;
    u16 trainerId;
    u32 actual;
    u8 matches;

    trainer = state[4];
    if (fightOutPokemonCheckFightOut(PC_PTR(originalAttacker)) == 0) {
        return 1;
    }

    if (reference == 0) {
        relative = 0;
    } else if ((s32)state[2] == 1) {
        relative = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, reference));
    } else if ((s32)state[2] == 2) {
        relative = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(3, reference));
    } else {
        relative = 0;
    }

    attackerSide = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, originalAttacker));
    mode = state[2];
    if ((mode == 1 || mode == 2) && relative == 0) {
        return 1;
    }
    if (mode == 0) {
        if (reference != 0 && reference == originalAttacker) {
            return 1;
        }
        goto mode_done;
    }
    if (mode == 1) {
        goto compare_side;
    }
    if (mode != 2) {
        goto invalid_mode;
    }
compare_side:
    if (relative == attackerSide) {
        goto mode_done;
    }
    return 1;
invalid_mode:
    return 1;
mode_done:

    attacker = (u16)state[0];
    trainerId = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
    fightTrainerGetStatus(0, trainerId, 2, 0);
    if (attacker == 0) {
        matches = 0;
    } else {
        fightFloorGetStatus(0, 0, 0x14, 0);
        trainerId = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
        trainerId = (u16)fightTrainerGetStatus(0, trainerId, 2, 0);
        if ((u8)fightTrainerGetStatus(0, trainerId, 0x2b, 0) == 1) {
            actual = fightOutPokemonGetTokuseiDataId(PC_PTR(originalAttacker));
        } else {
            actual = 0;
        }
        if (attacker == (u16)actual) {
            matches = 1;
        } else {
            matches = 0;
        }
    }

    if (matches == 1) {
        state[1] = originalAttacker;
        return 0;
    }
    return 1;
}

u16 fightAiGetSketchWaza(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return (u16)fn_8012640C(PC_PTR(param), 0, 0xEF, 0);
}

/* fightAiGetLastSelectWaza: the field Pokemon's last selected move (status 0xF0). */
/* NAME-T3 fn_802364BC */
u16 fightAiGetLastSelectWaza(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return (u16)fn_8012640C(PC_PTR(param), 0, 0xF0, 0);
}

/* fightAiGetLastUseWaza: the field Pokemon's last used move (status 0xF1). */
/* NAME-T3 fn_80236520 */
u32 fightAiGetLastUseWaza(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return (u16)fn_8012640C(PC_PTR(param), 0, 0xF1, 0);
}


u32 fightAiCollectWazaToSide(u32 ctx, u32 fightOut, u16 wantedMove, s16* outSlot, u8 checkUsable)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckCanOutOkWazaBanme();
#endif
    u16 moveIds[10];
    s16 slots[10];
    struct {
        u16 value;
    } count;
    u16 slot;
    struct {
        u16 value;
    } i;

    fightFloorGetStatus(0, 0, 0x14, 0);
    fightTrainerGetStatus(0, fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0), 2, 0);

    count.value = 0;
    for (i.value = count.value; i.value < 10; i.value++) {
        if (moveIds != 0) {
            moveIds[i.value] = count.value;
        }
        if (slots != 0) {
            slots[i.value] = -1;
        }
    }

    for (slot = 0; slot < 4; slot++) {
        struct {
            u32 value;
        } pokemon;
        u8 slot8;
        slot8 = (u8)slot;
        pokemon.value = pokemonGetStatus(PC_PTR(fightOut), 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0);
        fightFloorGetStatus(0, 0, 0x14, 0);
        fightTrainerGetStatus(0, fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0), 2, 0);
        fightPokemonGetPokemonPtr(PC_PTR(pokemon.value));
        pokemon.value = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(pokemon.value));
        pokemon.value = (u16)pokemonGetStatus(PC_PTR(pokemon.value), 0, POKEMON_STATUS_MOVE, slot8);
        if (pokemon.value == 0) {
            continue;
        }
        if (pokemon.value == 0x165) {
            if (pokemon.value == 0x163) {
                continue;
            }
        }
        if (checkUsable == 1) {
            fightFloorGetStatus(0, 0, 0x14, 0);
            fightTrainerGetStatus(0, fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0), 2, 0);
            if (fightOutPokemonCheckCanOutOkWazaBanme(PC_PTR(fightOut), slot8, 0, 0) != 0) {
                continue;
            }
        }
        if (moveIds != 0) {
            moveIds[count.value] = pokemon.value;
        }
        if (slots != 0) {
            slots[count.value] = slot;
        }
        count.value++;
    }

    for (i.value = 0; i.value < count.value; i.value++) {
        if (wantedMove == moveIds[i.value]) {
            if (outSlot != 0) {
                *outSlot = slots[i.value];
            }
            return 1;
        }
    }
    return 0;
}

u32 fightTrainerAiGetFightPokemonWazaAry(void* ctx, u32 fightOut, void* moveIds, u32 slots,
                u32 checkUsable)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FFEC8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205BE8();
#endif
    u16 trainer;
    u32 pokemon;
    u8 slot8;
    u16 slot;
    u32 count;

  fn_801F54A4(0,0,0x14,0);
  trainer = fn_801FB1C0(ctx,0,0x43,0);
  fn_801FB1C0(0,trainer,2,0);
  count = 0;
  for (slot = count; slot < 10; slot++) {
    if (moveIds != 0) {
      ((u16*)moveIds)[slot] = count;
    }
    if (slots != 0) {
      ((s16*)PC_PTR(slots))[slot] = -1;
    }
  }

  for (slot = 0; slot < 4; slot++) {
    slot8 = (u8)slot;
    pokemon = (0, fn_8012640C(PC_PTR(fightOut),0,0xd6,0));
    fn_801F54A4(0,0,0x14,0);
    trainer = fn_801FB1C0(ctx,0,0x43,0);
    fn_801FB1C0(0,trainer,2,0);
    fn_80205BE8(PC_PTR(pokemon));
    pokemon = PC_PTR32_IMPLICIT(fn_80205BE8(pokemon));
    pokemon = (u16)fn_8012640C(PC_PTR(pokemon),0,0x7f,slot8);
    if (pokemon == 0) {
      continue;
    }
    if (pokemon == 0x165) {
      if (pokemon == 0x163) {
        continue;
      }
    }
    if ((u8)checkUsable == 1) {
      fn_801F54A4(0,0,0x14,0);
      trainer = fn_801FB1C0(ctx,0,0x43,0);
      fn_801FB1C0(0,trainer,2,0);
#ifdef TARGET_PC
      {
        /* ORRE_BATTLE_LOG (observe only, 2026-10-25, P05's battle 56): each move the AI's list leaves out, and
         * why (fn_801FFEC8's result: the move can't be chosen this turn) */
        static int enabled = -1;
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        u8 pcBlocked = fn_801FFEC8(PC_PTR(fightOut),slot8,0,0);
        if (enabled < 0) {
          extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
          enabled = pc_battle_log_on();
        }
        if (enabled) {
          OSReport("[battle] ai move: mon %08X slot %u move %u blocked %u\n", (unsigned)fightOut, (unsigned)slot8,
                   (unsigned)pokemon, (unsigned)pcBlocked);
        }
        if (pcBlocked != 0) {
          continue;
        }
      }
#else
      if (fn_801FFEC8(PC_PTR(fightOut),slot8,0,0) != 0) {
        continue;
      }
#endif
    }
    if (moveIds != 0) {
      ((u16*)moveIds)[(u16)count] = pokemon;
    }
    if (slots != 0) {
      ((s16*)PC_PTR(slots))[(u16)count] = slot;
    }
    count++;
  }
  return count;
}

u32 fightTrainerAiGetFightPokemonWazaAry2(u32 trainer, u32 fightPokemon, u16* outMoves,
                s16* outSlots, u32 requirePp)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    u16 species;
    u16 index;
    u32 move;
    u32 pokemon;
    u32 count;

    fightFloorGetStatus(0, 0, 0x14, 0);
    species = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
    fightTrainerGetStatus(0, species, 2, 0);

    count = 0;
    for (index = 0; index < 10; index++) {
        if (outMoves != 0) {
            outMoves[index] = 0;
        }
        if (outSlots != 0) {
            outSlots[index] = -1;
        }
    }

    for (index = 0; index < 4; index++) {
        fightFloorGetStatus(0, 0, 0x14, 0);
        species = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
        fightTrainerGetStatus(0, species, 2, 0);
        fightPokemonGetPokemonPtr(PC_PTR(fightPokemon));
        pokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(fightPokemon));
        move = (u16)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE, (u8)index);

        if (move != 0 && (move != 0x165 || move != 0x163)) {
            if ((u8)requirePp == 1) {
                fightFloorGetStatus(0, 0, 0x14, 0);
                species = (u16)fightTrainerGetStatus(PC_PTR(trainer), 0, 0x43, 0);
                fightTrainerGetStatus(0, species, 2, 0);
                fightPokemonGetPokemonPtr(PC_PTR(fightPokemon));
                pokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(fightPokemon));
                if ((u8)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, (u8)index) == 0) {
                    continue;
                }
            }
            if (outMoves != 0) {
                outMoves[(u16)count] = move;
            }
            if (outSlots != 0) {
                outSlots[(u16)count] = index;
            }
            count++;
        }
    }
    return count;
}

u16 fightAiGetUsedItem(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return (u16)fn_8012640C(PC_PTR(param), 0, 0xFA, 0);
}


u32 fightTrainerAiIsPokemonJoutai(void* ctx, void* param2, void* param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u16 val = (u16)fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    if ((u8)fn_801FB1C0(0, val, 0x24, 0) == 1) {
        return fightOutPokemonIsPokemonJoutaiDataId(param2, param3);
    }
    return 0;
}


/* fightTrainerAiGetPartDataId: if the AI's part flag (0x33) is set, the trainer Pokemon's part data id (0x1D);
 * otherwise 1. */
/* NAME-T3 fn_80236C80 */
u8 fightTrainerAiGetPartDataId(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val, val2;
    fn_801F54A4(0, 0, 0x14, 0);
    val = (u16)fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val2 = (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0xC9, 0);
    if ((u8)fn_801FB1C0(0, val, 0x33, 0) == 1) {
        return (u8)fn_801FB1C0(0, val2, 0x1D, 0);
    }
    return 1;
}


int fightTrainerAiIsFasterBasis(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
  struct {
    u32 value;
  } uVar1;
  u16 uVar4;
  u16 uVar2;
  u32 uVar3;

  uVar1.value = (int)pokemonGetStatus(PC_PTR(r4),0,POKEMON_STATUS_BATTLE_POKEMON_PTR,0);
  fightFloorGetStatus(0,0,0x14,0);
  uVar4 = fightTrainerGetStatus(PC_PTR(r3),0,0x43,0);
  fightTrainerGetStatus(0,uVar4,2,0);
  uVar1.value = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(uVar1.value));
  uVar4 = (int)pokemonGetStatus(PC_PTR(uVar1.value),0,POKEMON_STATUS_SPECIES,0);
  uVar2 = (int)pokemonGetStatus(0,uVar4,POKEMON_STATUS_BASE_SPEED,0);
  uVar1.value = (int)pokemonGetStatus(PC_PTR(r5),0,POKEMON_STATUS_BATTLE_POKEMON_PTR,0);
  fightFloorGetStatus(0,0,0x14,0);
  uVar4 = fightTrainerGetStatus(PC_PTR(r3),0,0x43,0);
  fightTrainerGetStatus(0,uVar4,2,0);
  uVar1.value = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(uVar1.value));
  uVar4 = (int)pokemonGetStatus(PC_PTR(uVar1.value),0,POKEMON_STATUS_SPECIES,0);
  uVar3 = (int)pokemonGetStatus(0,uVar4,POKEMON_STATUS_BASE_SPEED,0);
  return (uVar2 & 0xffff) - (uVar3 & 0xffff);
}


/* fightTrainerAiGetBasis*: the field Pokemon's species base stat (species data 4-8: Attack, Defense, Sp. Atk,
 * Sp. Def, Speed). This one: base Speed (8). */
/* NAME-T3 fn_80236E9C */
u16 fightTrainerAiGetBasisNimbleness(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val;
    fn_801F54A4(0, 0, 0x14, 0);
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val = (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x6E, 0);
    return (u16)fn_8012640C(0, val, 0x8, 0);
}


/* Base Sp. Def (species data 7). */
/* NAME-T3 fn_80236F4C */
u16 fightTrainerAiGetBasisSpeDef(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val;
    fn_801F54A4(0, 0, 0x14, 0);
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val = (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x6E, 0);
    return (u16)fn_8012640C(0, val, 0x7, 0);
}


/* Base Sp. Atk (species data 6). */
/* NAME-T3 fn_80236FFC */
u16 fightTrainerAiGetBasisSpeAtk(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val;
    fn_801F54A4(0, 0, 0x14, 0);
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val = (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x6E, 0);
    return (u16)fn_8012640C(0, val, 0x6, 0);
}


/* Base Defense (species data 5). */
/* NAME-T3 fn_802370AC */
u16 fightTrainerAiGetBasisPhyDef(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val;
    fn_801F54A4(0, 0, 0x14, 0);
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val = (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x6E, 0);
    return (u16)fn_8012640C(0, val, 0x5, 0);
}


/* Base Attack (species data 4). */
/* NAME-T3 fn_8023715C */
u16 fightTrainerAiGetBasisPhyAtk(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val;
    fn_801F54A4(0, 0, 0x14, 0);
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val = (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x6E, 0);
    return (u16)fn_8012640C(0, val, 0x4, 0);
}

u32 fightAiIsKaragenki(void* ctx, void* param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80203C5C(void* ctx);
#endif
    u16 val = (u16)fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    if ((u8)fn_801FB1C0(0, val, 0x24, 0) == 1) {
        return fn_80203C5C(param);
    }
    return 0;
}


/* fightTrainerAiIsJoutaiKie: if the AI may know conditions (0x24), fightOutPokemonIsJoutaiKie for the field
 * Pokemon; otherwise FALSE (like fightTrainerAiIsJoutaiNormal). */
/* NAME-T3 fn_80237288 */
u32 fightTrainerAiIsJoutaiKie(u32 r3, u32 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern short fn_802010C8();
#endif
  u16 uVar1;
  u8 cVar3;
  u16 sVar2;
  u32 bVar4;

  uVar1 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  uVar1 = fn_801FB1C0(0,uVar1,2,0);
  cVar3 = fn_801FB1C0(0,uVar1,0x24,0);
  if (cVar3 == 1) {
    sVar2 = fn_802010C8(PC_PTR(r4));
    bVar4 = sVar2 != 0;
  }
  else {
    bVar4 = 0;
  }
  return bVar4;
}

u32 fightTrainerAiIsJoutaiNormal(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80122DDC(u8* ptr);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    u16 val = (u16)fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    if ((u8)fn_801FB1C0(0, val, 0x24, 0) == 1) {
        return fn_80122DDC(fn_80205BE8(resolved));
    }
    return 1;
}
