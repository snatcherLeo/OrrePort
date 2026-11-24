/**
 * @file fight_range_8023753C_exact.c
 * @brief Exact pure-C fight-engine island.
 */
#include "dolphin/types.h"

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
#define fn_80203DAC figthPokemonGetPokemonDataId
#define fn_80203E7C figthPokemonGetLevel
#define fn_80205BE8 fightPokemonGetPokemonPtr
#define fn_80207B8C fightOutPokemonGetZokuseiDataId
#define fn_80207BF4 fightOutPokemonGetTokuseiDataId

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerIsAllyFightTargetPtr(u32, u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetZokuseiDataId(u32, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightOutPokemonGetTokuseiDataId(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 zokuseiGetAisyouDualType(u16, u16*, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int fightOutPokemonIsPokemonJoutaiDataId();
#endif

u32 fightTrainerAiGetHpRatio(u32 r3, u32 r4)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205BE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
  u16 uVar3;
  u32 uVar1;
  u16 uVar6;
  u16 uVar7;
  u32 uVar2;
  u16 uVar4;
  u8 uVar5;

  uVar1 = fn_8012640C(PC_PTR(r4),0,0xd6,0);
  uVar6 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  uVar6 = fn_801FB1C0(0,uVar6,2,0);
  uVar7 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  fn_801FB1C0(0,uVar7,2,0);
  uVar2 = PC_PTR32_IMPLICIT(fn_80205BE8(uVar1));
  uVar3 = fn_8012640C(PC_PTR(uVar2),0,0x83,0);
  uVar7 = fn_801FB1C0(PC_PTR(r3),0,0x43,0);
  fn_801FB1C0(0,uVar7,2,0);
  uVar1 = PC_PTR32_IMPLICIT(fn_80205BE8(uVar1));
  uVar4 = fn_8012640C(PC_PTR(uVar1),0,0x87,0);
  uVar5 = fn_801FB1C0(0,uVar6,0x2c,0);
  return (int)((int)uVar3 * 100) / (int)uVar4 <= (int)uVar5;
}

u16 fn_80237664(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x87, 0);
}

u16 fn_802376EC(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80205BE8(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return (u16)fn_8012640C(fn_80205BE8(resolved), 0, 0x83, 0);
}

u32 fightAiGetLevel(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80203E7C(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return fn_80203E7C(PC_PTR32_IMPLICIT(resolved));
}

u16 fightTrainerAiGetPokemonDataId(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203DAC(void* ctx);
#endif
    void* resolved = PC_PTR(fn_8012640C(PC_PTR(param), 0, 0xD6, 0));
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    return fn_80203DAC(resolved);
}


u8 fightTrainerAiGetPartData1C(void* ctx, u32 param) {
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
    if ((u8)fn_801FB1C0(0, val, 0x23, 0) == 1) {
        return (u8)fn_801FB1C0(0, val2, 0x1C, 0);
    }
    return 0;
}


u32 fightTrainerAiGetAisyou(u32 ctx, u32 poke, u16 waza, s32 val) {
    u16 types[2];
    u16 tmp;
    u16 tk;
    struct {
        u16 value;
    } typeCount;
    u16 z;
    struct {
        u8 value;
    } i;
    u16 j;
    u16 floorVal;
    u32 result;
    u8 tkMatch;
    u32 valid;

    if (waza == 9) {
        return 0x3f;
    }

    tmp = (u16)fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0);
    fightTrainerGetStatus(0, tmp, 2, 0);
    fightFloorGetStatus(0, 0, 0x14, 0);

    tmp = (u16)fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0);
    tmp = (u16)fightTrainerGetStatus(0, tmp, 2, 0);
    if ((u8)fightTrainerGetStatus(0, tmp, 0x2b, 0) == 1) {
        tk = fightOutPokemonGetTokuseiDataId(PC_PTR(poke));
    } else {
        tk = 0;
    }
    tkMatch = (tk == 0x1a);
    if (tkMatch == 1 && waza == 4) {
        return 0x43;
    }

    typeCount.value = 0;
    i.value = typeCount.value;
    for (; i.value < 2; i.value++) {
        floorVal = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
        tmp = (u16)fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0);
        tmp = (u16)fightTrainerGetStatus(0, tmp, 2, 0);
        if ((u8)fightTrainerGetStatus(0, tmp, 0x2a, 0) == 1) {
            if ((u8)fightTrainerIsAllyFightTargetPtr(PC_PTR(ctx), PC_PTR(poke), floorVal) == 0) {
                z = fightOutPokemonGetZokuseiDataId(PC_PTR(poke), i.value);
            } else {
                z = fightOutPokemonGetZokuseiDataId(PC_PTR(poke), i.value);
            }
        } else {
            z = 9;
        }
        if (z != 9) {
            types[typeCount.value] = z;
            typeCount.value++;
        }
    }
    if (typeCount.value == 0) {
        return 0x3f;
    }

    result = zokuseiGetAisyouDualType(waza, types, typeCount.value);
    if (waza == 0 || waza == 1) {
        tmp = (u16)fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0);
        tmp = (u16)fightTrainerGetStatus(0, tmp, 2, 0);
        if ((u8)fightTrainerGetStatus(0, tmp, 0x24, 0) == 1) {
            valid = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(poke), PC_PTR(0x19));
        } else {
            valid = 0;
        }
        if ((u8)valid == 1) {
            for (j = 0; j < typeCount.value; j++) {
                u16 v = types[j];
                if (v != 9 && v == 7) {
                    result = 0x3f;
                }
            }
        }
    }

    tmp = (u16)fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0);
    fightTrainerGetStatus(0, tmp, 2, 0);
    fightFloorGetStatus(0, 0, 0x14, 0);
    tmp = (u16)fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0);
    tmp = (u16)fightTrainerGetStatus(0, tmp, 2, 0);
    if ((u8)fightTrainerGetStatus(0, tmp, 0x2b, 0) == 1) {
        tk = fightOutPokemonGetTokuseiDataId(PC_PTR(poke));
    } else {
        tk = 0;
    }
    tkMatch = (tk == 0x19);
    if (tkMatch == 1 && (u16)result != 0x41 && (s16)val > 0) {
        return 0x43;
    }
    return result;
}

u32 fightAiCheckAllyFlag2A(u32 r3, u32 r4, int r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F8424();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207B8C();
#endif
  u32 uVar1;
  u32 uVar2;
  u8 cVar5;
  u16 sVar4;
  u32 uVar7;
  u32 uVar6;

  uVar7 = 0;
  for (uVar6 = 0; (uVar6 & 0xff) < 2; uVar6 = uVar6 + 1) {
    uVar2 = fn_801F54A4(0,0,0x14,0);
    cVar5 = fn_801FB1C0(0,fn_801FB1C0(0,fn_801FB1C0(PC_PTR(r3),0,0x43,0),2,0),0x2a,0);
    if (cVar5 == 1) {
      cVar5 = fn_801F8424(PC_PTR(r3),PC_PTR(r4),uVar2);
      if (cVar5 == 0) {
        sVar4 = fn_80207B8C(PC_PTR(r4),uVar6);
      }
      else {
        sVar4 = fn_80207B8C(PC_PTR(r4),uVar6);
      }
    }
    else {
      sVar4 = 9;
    }
    if (sVar4 != 9) {
      uVar1 = uVar7 & 0xffff;
      uVar7 = uVar7 + 1;
      *(short *)PC_PTR((r5 + uVar1 * 2)) = sVar4;
    }
  }
  return uVar7;
}


u8 fightTrainerAiGetZokusei(u32 ctx, void* param2, u16 target) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerIsAllyFightTargetPtr(u32 fieldA, void* floor, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80207B8C(void* context, u8 field);
#endif
    u16 val;
    u16 temp;
    fn_801FB1C0(0, fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    val = (u16)fn_801F54A4(0, 0, 0x14, 0);
    if ((u8)fn_801FB1C0(0, fn_801FB1C0(0, fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0) & 0xFFFF, 0x2, 0) & 0xFFFF, 0x2A, 0) == 1) {
        if (!fightTrainerIsAllyFightTargetPtr(PC_PTR(ctx), param2, val)) {
            temp = fn_80207B8C(param2, 0);
        } else {
            temp = fn_80207B8C(param2, 0);
        }
    } else {
        temp = 9;
    }
    if (target == temp) {
        goto found;
    }
    val = (u16)fn_801F54A4(0, 0, 0x14, 0);
    if ((u8)fn_801FB1C0(0, fn_801FB1C0(0, fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0) & 0xFFFF, 0x2, 0) & 0xFFFF, 0x2A, 0) == 1) {
        if (!fightTrainerIsAllyFightTargetPtr(PC_PTR(ctx), param2, val)) {
            temp = fn_80207B8C(param2, 1);
        } else {
            temp = fn_80207B8C(param2, 1);
        }
    } else {
        temp = 9;
    }
    if (target != temp) {
        goto not_found;
    }
found:
    return 1;
not_found:
    return 0;
}


u8 fn_80237F74(void* ctx, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80207BF4(void* param);
#endif
    u16 val;
    u32 temp;
    fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    if ((u16)param3 == 0) {
        return 0;
    }
    fn_801F54A4(0, 0, 0x14, 0);
    val = (u16)fn_801FB1C0(0, fn_801FB1C0(ctx, 0, 0x43, 0) & 0xFFFF, 0x2, 0);
    if ((u8)fn_801FB1C0(0, val, 0x2B, 0) == 1) {
        temp = fn_80207BF4((void*)PC_PTR(param2));
    } else {
        temp = 0;
    }
    if ((u16)param3 == (u16)temp) {
        return 1;
    }
    return 0;
}
