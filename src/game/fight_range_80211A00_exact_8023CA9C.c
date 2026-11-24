/**
 * @file fight_range_80211A00_exact_8023CA9C.c
 * @brief Strict target-order suffix, 0x8023CA9C - 0x802405C0 (33 fns).
 *
 * Materialized from clean blob 754061cf; executable owner bodies are
 * byte-for-byte source extracts and compiler state is re-established per
 * original owner context.
 */
#include "dolphin/types.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
extern f32 lbl_8047E630;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int fightTrainerAiGetHpPercent(u32, u32, int, f32);
#endif

/* 0x8023CA9C size 0xC4: fn_8023CA9C */
#define fn_8011BEB4 wazaGetStatus
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_8024E52C fightTrainerAiWazaValueNull
s32 fn_8023CA9C(void* ctx, u32 param1, u32 param2, u32 param3) {
    typedef s32 (*Handler)();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern Handler fn_8011BEB4(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0(void*, u32, u32, u32);
#endif
    extern s32 fn_8024E52C();
    Handler fp;
    u16 v;
    u8 v2;

    v = (u16)fn_801FB1C0(ctx, 0, 0x43, 0);
    v = (u16)fn_801FB1C0(0, v, 2, 0);
    v2 = (u8)fn_801FB1C0(0, v, 0x32, 0);
    if (v2 != 1) {
        goto ret0;
    }
    fp = PC_PTR(fn_8011BEB4(0, param2, 0x1c, 0));
    if (fp != NULL) {
        goto call;
    }
    fp = fn_8024E52C;
    goto call;
ret0:
    return 0;
call:
    return fp(ctx, param1, param2, param3);
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8024E52C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8011BEB4
#endif

/* 0x8023CB60 size 0x140: fightTrainerAiWazaValueYumekui */
#define fn_801F1990 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_802399FC fightTrainerAiAddValue
u32 fightTrainerAiWazaValueYumekui(u32 r3, u32 r4, u32 r5, u32 r6)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F1990();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_802376EC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_802399FC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239CCC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTrainerAiGetWazaDamageValue();
#endif
  u32 uVar1;
  u32 uVar2;
  int iVar3;
  u32 uVar5;
  u8 cVar6;

  uVar1 = fn_802376EC();
  uVar2 = fightTrainerAiGetWazaDamageValue(PC_PTR(r3),r4,r5,r6,1);
  iVar3 = fn_801FB1C0(0,0x223,0x3e,0);
  uVar2 = ((int)(((int)uVar2 / 2) * 100) /
          (int)(uVar1 & 0xffff)) / iVar3;
  {
    u32 callResult = fn_802399FC(0,uVar2);
    uVar1 = callResult;
  }
  uVar5 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
  fn_80239CCC(0xec64,r3,uVar5,0,0,r5,0,0x223,uVar2);
  cVar6 = fn_801F1990(0,PC_PTR(r3),1,1,PC_PTR(0x10e),PC_PTR(r4));
  if (cVar6 == 1) {
    {
      u32 callResult = fn_80239984(uVar1,PC_PTR(r3),0x224);
      uVar1 = callResult;
    }
    uVar5 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar5,0,0,r5,0,0x224);
  }
  return uVar1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_802399FC
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1990
#endif

/* 0x8023CCA0 size 0x12C: fightTrainerAiWazaValueKawarawari */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E98 fightSideIsJoutaiDataId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_801F1990 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut
u32 fightTrainerAiWazaValueKawarawari(u32 r3, u32 r4, u32 r5, u32 r6)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F1990();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F6E98();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u32 uVar3;
  u8 cVar2;
  u32 uVar1;

  uVar3 = 0;
  uVar1 = PC_PTR32_IMPLICIT(fn_801F025C(2,r6));
  cVar2 = fn_801F6E98(uVar1,0x49);
  if ((cVar2 == 1) || (cVar2 = fn_801F6E98(uVar1,0x48), cVar2 == 1)) {
    uVar3 = fn_80239984(0,PC_PTR(r3),0x221);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x221);
  }
  cVar2 = fn_801F1990(0,PC_PTR(r3),1,1,PC_PTR(0x10e),PC_PTR(r4));
  if (cVar2 == 1) {
    uVar3 = fn_80239984(uVar3,PC_PTR(r3),0x222);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x222);
  }
  return uVar3;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1990
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E98
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

/* 0x8023CDCC size 0x94: fightTrainerAiWazaValueTeiryouDameeji */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueTeiryouDameeji(u32 arg0, u32 arg1, u32 arg2, u32 arg3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
    extern u16 fightTrainerAiGetPokemonDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u32 uVar1;
  u32 uVar3;

  uVar3 = 0;
  if (fightTrainerAiGetPokemonDataId(arg0, arg3) == 0xd5) {
    uVar3 = fn_80239984(0, PC_PTR(arg0), 0x220);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(arg1));
    fn_80239EE8(0xec64, arg0, uVar1, 0, 0, arg2, 0, 0x220);
  }
  return uVar3;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023CE60 size 0x17C: fightTrainerAiWazaValueHikarinokabe */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level 2) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 2
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueHikarinokabe(u32 r3, u32 r4, u32 r5, u32 r6)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetLastUseWaza();
#endif
    extern u16 fightTrainerAiGetBasisSpeAtk();
    extern u16 fightTrainerAiGetBasisPhyAtk();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTrainerAiGetWazaHitValue();
#endif
  u32 uVar6;
  u16 uVar2;
  u16 uVar3;
  u16 sVar4;
  u8 cVar5;
  u32 uVar1;

  uVar6 = 0;
  uVar2 = fightTrainerAiGetBasisSpeAtk(r3,r6);
  uVar3 = fightTrainerAiGetBasisPhyAtk(r3,r6);
  sVar4 = fightAiGetLastUseWaza(PC_PTR(r3),r6);
  cVar5 = fightTrainerAiGetWazaHitValue(PC_PTR(r3),r4,r5,r6);
  if (cVar5 == 1) {
    uVar6 = fn_80239984(0,PC_PTR(r3),0x21d);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x21d);
  }
  if (uVar3 < uVar2) {
    uVar6 = fn_80239984(uVar6,PC_PTR(r3),0x21e);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x21e);
  }
  if (sVar4 == 0x118) {
    uVar6 = fn_80239984(uVar6,PC_PTR(r3),0x21f);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x21f);
  }
  return uVar6;
}

/* 0x8023CFDC size 0x17C: fightTrainerAiWazaValueRihurekutaa */
u32 fightTrainerAiWazaValueRihurekutaa(u32 r3, u32 r4, u32 r5, u32 r6)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetLastUseWaza();
#endif
    extern u16 fightTrainerAiGetBasisSpeAtk();
    extern u16 fightTrainerAiGetBasisPhyAtk();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTrainerAiGetWazaHitValue();
#endif
  u16 uVar2;
  u16 uVar3;
  u16 sVar4;
  u8 cVar5;
  u32 uVar1;
  u32 uVar6;

  uVar6 = 0;
  uVar2 = fightTrainerAiGetBasisSpeAtk(r3,r6);
  uVar3 = fightTrainerAiGetBasisPhyAtk(r3,r6);
  sVar4 = fightAiGetLastUseWaza(PC_PTR(r3),r6);
  cVar5 = fightTrainerAiGetWazaHitValue(PC_PTR(r3),r4,r5,r6);
  if (cVar5 == 1) {
    uVar6 = fn_80239984(0,PC_PTR(r3),0x21a);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x21a);
  }
  if (uVar3 > uVar2) {
    uVar6 = fn_80239984(uVar6,PC_PTR(r3),0x21b);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x21b);
  }
  if (sVar4 == 0x118) {
    uVar6 = fn_80239984(uVar6,PC_PTR(r3),0x21c);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x21c);
  }
  return uVar6;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023D158 size 0x1CC: fightTrainerAiWazaValueOnibi */
/* RULE-EXCEPTION(user-approved): restores the unit settings after the tagged local block above — see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
#define fn_801F1C18 fightFloorGetFightTrainerFightOutPokemonPtrAry
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueOnibi(void* ctx, u32 param1, u32 param2, u32 param3)
{
    extern u16 fightTrainerAiGetBasisSpeAtk(void*, u32);
    extern u16 fightTrainerAiGetBasisPhyAtk(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetLastUseWaza(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_801F1C18(u32, void*, void*, u32, u32);
#endif
    extern u16 fightTrainerAiGetPokemonDataId(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 arr[8];
    u32 handle = 0;
    u16 a = fightTrainerAiGetBasisSpeAtk(ctx, param3);
    u16 b = fightTrainerAiGetBasisPhyAtk(ctx, param3);
    u16 c = fightAiGetLastUseWaza(ctx, param3);
    u16 count;
    u16 i;

    count = fn_801F1C18(0, PC_PTR32_IMPLICIT(ctx), arr, 0, 1);
    for (i = 0; i < count; i++) {
        u16 v = fightTrainerAiGetPokemonDataId(ctx, arr[i]);
        if (v == 0xca || v == 0x168 || v == 0x12f || v == 0xd5) {
            handle = fn_80239984(0, ctx, 0x217);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x217);
            break;
        }
    }
    if (b > a) {
        handle = fn_80239984(handle, ctx, 0x218);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x218);
    }
    if (c == 0x11f) {
        handle = fn_80239984(handle, ctx, 0x219);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x219);
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1C18
#endif

/* 0x8023D324 size 0x15C: fightTrainerAiWazaValueDoku */
#define fn_801F1C18 fightFloorGetFightTrainerFightOutPokemonPtrAry
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueDoku(void* ctx, u32 param1, u32 param2, u32 param3)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_801F1C18(u32, void*, void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetLastUseWaza(void*, u32);
#endif
    extern u16 fightTrainerAiGetPokemonDataId(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 arr[10];
    u32 handle = 0;
    u16 count;
    u16 c;
    u16 i;

    count = fn_801F1C18(0, PC_PTR32_IMPLICIT(ctx), arr, 0, 1);
    c = fightAiGetLastUseWaza(ctx, param3);
    for (i = 0; i < count; i++) {
        u16 v = fightTrainerAiGetPokemonDataId(ctx, arr[i]);
        if (v == 0xca || v == 0x168 || v == 0x12f || v == 0xd5) {
            handle = fn_80239984(0, ctx, 0x215);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x215);
            break;
        }
    }
    if (c == 0x11f) {
        handle = fn_80239984(handle, ctx, 0x216);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x216);
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1C18
#endif

/* 0x8023D480 size 0x90: fightTrainerAiWazaValueBatontatti */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueBatontatti(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsAbicntOverMax();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u8 cVar2;
  u32 uVar1;
  u32 uVar3;

  uVar3 = 0;
  cVar2 = fightTrainerAiIsAbicntOverMax(r3, r4); /* r3 / r4 untouched before the call (assembly) */
  if (cVar2 == 1) {
    uVar3 = fn_80239984(0,PC_PTR(r3),0x214);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x214);
  }
  return uVar3;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* Non-prototype declarations visible to the original late AI block. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiIsAbicntOverMax();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerAiGetFightPokemonWazaAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetFightTrainerFightOutPokemonPtrAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_80239EE8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_80239CCC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightTrainerAiAddValue();
#endif

/* 0x8023D510 size 0x23C: fightTrainerAiWazaValueHaradaiko */
u32 fightTrainerAiWazaValueHaradaiko(u32 ctx, u32 poke, u32 msgArg) {
    u32 acc = 0;
    u16 scanBuf[10];
    u32 stackArr[8];
    u16 count16;
    u32* stackPtr;
    u16 i;
    u32 count;
    u8 found;
    u32 result;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x210);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x210);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    stackPtr = stackArr;
    count16 = (u16)count;
    for (i = 0; i < count16; i++) {
        if (stackPtr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackPtr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_haradaiko;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_haradaiko:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x211);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x211);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x212);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x212);
    }

    result = acc;
    if ((u8)fightTrainerAiGetHpPercent(ctx, poke, -1, lbl_8047E630) == 1) {
        result = fn_80239984(acc, PC_PTR(ctx), 0x213);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x213);
    }

    return result;
}

/* 0x8023D74C size 0x200: fightTrainerAiWazaValueMarukunaru */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueMarukunaru(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h (fightTrainerGetStatus) */
    extern int fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsAbicntOverMax();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetAbicntPhyDef();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetFightPokemonWazaAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239CCC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u32 uVar4;
  int uVar1;
  u8 cVar5;
  u32 uVar2;
  int iVar3;
  u16 uVar6;
  struct {
    u32 value;
  } uVar7;
  u16 local_30 [12];

  uVar4 = fightTrainerAiGetFightPokemonWazaAry(PC_PTR(r3),r4,local_30,0,1);
  uVar1 = fightTrainerAiGetAbicntPhyDef(r3,r4);
  uVar7.value = 0;
  cVar5 = fightTrainerAiIsAbicntOverMax(r3,r4);
  if (cVar5 == 0) {
    uVar7.value = fn_80239984(0,PC_PTR(r3),0x20c);
    uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar2,0,0,r5,0,0x20c);
  }
  cVar5 = fightTrainerAiIsAbicntOverMax(r3,r4);
  if (cVar5 == 1) {
    uVar7.value = fn_80239984(uVar7.value,PC_PTR(r3),0x20e);
    uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar2,0,0,r5,0,0x20e);
  }
  uVar1 = (uVar1 & 0xff) - 6;
  if (uVar1 < 0) {
    uVar1 = 0;
  }
  iVar3 = fightTrainerGetStatus(0,0x20f,0x3e,0);
  uVar1 = uVar1 * iVar3;
  uVar7.value = fightTrainerAiAddValue(uVar7.value,uVar1);
  uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
  fn_80239CCC(0xec64,r3,uVar2,0,0,r5,0,0x20f,uVar1);
  for (uVar6 = 0; uVar6 < (u16)uVar4; uVar6++) {
    if ((local_30[uVar6] == 0xcd) || (local_30[uVar6] == 0x12d)) {
      {
        u32 callResult = fn_80239984(uVar7.value,PC_PTR(r3),0x20d);
        uVar7.value = callResult;
      }
      uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
      fn_80239EE8(0xec64,r3,uVar2,0,0,r5,0,0x20d);
      break;
    }
  }
  return uVar7.value;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* Earlier helper definitions supplied these return types in the original TU. */
extern u8 fightTrainerAiGetAbicntNimbleness(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiGetAbicntSpeDef(u32, u32);
#endif
extern u8 fightTrainerAiGetAbicntSpeAtk(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiGetAbicntPhyDef(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiGetAbicntPhyAtk(u32, u32);
#endif
extern u8 fightTrainerAiGetAbicntAvoid(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTrainerGetStatus(void* ctx, u16 slot, u32 field, u32 idx);
#endif

/* 0x8023D94C size 0x24C: fightTrainerAiWazaValueMeisou */
u32 fightTrainerAiWazaValueMeisou(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntSpeDef(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x208);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x208);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan;
                    }
                }
            }
        }
    }
    found = 0;
after_scan:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x209);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x209);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x20a);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x20a);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x20b, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x20b, count2);
        return result;
    }
}

/* 0x8023DB98 size 0x24C: fightTrainerAiWazaValueRyuunomai */
u32 fightTrainerAiWazaValueRyuunomai(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntNimbleness(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x204);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x204);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_ryuunomai;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_ryuunomai:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x205);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x205);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x206);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x206);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x207, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x207, count2);
        return result;
    }
}

/* 0x8023DDE4 size 0x24C: fightTrainerAiWazaValueBirudoAppu */
u32 fightTrainerAiWazaValueBirudoAppu(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntPhyAtk(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x200);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x200);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_birudoappu;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_birudoappu:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x201);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x201);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x202);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x202);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x203, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x203, count2);
        return result;
    }
}

/* 0x8023E030 size 0x24C: fightTrainerAiWazaValueKosumopawaa */
u32 fightTrainerAiWazaValueKosumopawaa(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntSpeDef(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1fc);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1fc);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_kosumopawaa;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_kosumopawaa:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1fd);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1fd);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1fe);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1fe);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1ff, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1ff, count2);
        return result;
    }
}

/* 0x8023E27C size 0x24C: fightTrainerAiWazaValueKaihirituAppu */
u32 fightTrainerAiWazaValueKaihirituAppu(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntAvoid(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f8);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f8);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_kaihirituappu;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_kaihirituappu:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f9);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f9);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1fa);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1fa);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1fb, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1fb, count2);
        return result;
    }
}

/* 0x8023E4C8 size 0x24C: fightTrainerAiWazaValueDowasure */
u32 fightTrainerAiWazaValueDowasure(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntSpeDef(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f4);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f4);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_dowasure;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_dowasure:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f5);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f5);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f6);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f6);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1f7, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f7, count2);
        return result;
    }
}

/* 0x8023E714 size 0x24C: fightTrainerAiWazaValueTokukouAppu */
u32 fightTrainerAiWazaValueTokukouAppu(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntSpeAtk(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f0);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f0);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_tokukouappu;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_tokukouappu:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f1);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f1);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1f2);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f2);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1f3, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1f3, count2);
        return result;
    }
}

/* 0x8023E960 size 0x24C: fightTrainerAiWazaValueKousokuidou */
u32 fightTrainerAiWazaValueKousokuidou(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntNimbleness(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1ec);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1ec);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_kousokuidou;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_kousokuidou:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1ed);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1ed);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1ee);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1ee);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1ef, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1ef, count2);
        return result;
    }
}

/* 0x8023EBAC size 0x24C: fightTrainerAiWazaValueBougyoAppu */
u32 fightTrainerAiWazaValueBougyoAppu(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntPhyDef(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1e8);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1e8);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_bougyoappu;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_bougyoappu:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1e9);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1e9);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1ea);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1ea);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1eb, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1eb, count2);
        return result;
    }
}

/* 0x8023EDF8 size 0x24C: fightTrainerAiWazaValueKougekiAppu */
u32 fightTrainerAiWazaValueKougekiAppu(u32 ctx, u32 poke, u32 msgArg) {
    u8 v = fightTrainerAiGetAbicntPhyAtk(ctx, poke);
    u32 acc = 0;
    u16 scanBuf[16];
    u32 stackArr[8];
    u32 count;
    u16 i;
    u8 found;

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 0) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1e4);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1e4);
    }

    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, ctx, stackArr, 1, 1);
    for (i = 0; i < (u16)count; i++) {
        if (stackArr[i] != (u32)poke) {
            u16 n = (u16)fightTrainerAiGetFightPokemonWazaAry(PC_PTR(ctx), stackArr[i], scanBuf, 0, 1);
            if (n != 0) {
                u16 j;
                for (j = 0; j < n; j++) {
                    if (scanBuf[j] == 0x10a) {
                        found = 1;
                        goto after_scan_kougekiappu;
                    }
                }
            }
        }
    }
    found = 0;
after_scan_kougekiappu:

    if (found == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1e5);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1e5);
    }

    if ((u8)fightTrainerAiIsAbicntOverMax(ctx, poke) == 1) {
        u32 oldAcc = acc;
        acc = 0;
        acc += fn_80239984(oldAcc, PC_PTR(ctx), 0x1e6);
        fn_80239EE8(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1e6);
    }

    {
        u32 result = 0;
        s32 count2 = (u8)v - 6;
        u32 scale;

        if (count2 < 0) {
            count2 = 0;
        }
        scale = (u32)fightTrainerGetStatus(0, 0x1e7, 0x3e, 0);
        count2 *= scale;
        result += fightTrainerAiAddValue(acc, count2);
        fn_80239CCC(0xec64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0, msgArg, 0, 0x1e7, count2);
        return result;
    }
}

/* 0x8023F044 size 0x100: fightTrainerAiWazaValuetorikku */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValuetorikku(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetSoubiItem(void* ctx, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
    u32 handle = 0;
    u16 v = fightTrainerAiGetSoubiItem(ctx, param1);

    if (v == 0x1d || v == 0x18) {
        handle = fn_80239984(0, ctx, 0x1e2);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1e2);
    }
    if (fn_80237F74(ctx, param3, 0x3c) == 1) {
        handle = fn_80239984(handle, ctx, 0x1e3);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1e3);
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023F144 size 0x134: fightTrainerAiWazaValueSukirusuwappu */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueSukirusuwappu(u32 arg0, u32 arg1, u32 arg2, u32 arg3)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u8 cVar2;
  u32 uVar1;
  u32 uVar3;

  uVar3 = 0;
  cVar2 = fn_80237F74(PC_PTR(arg0),arg1,0x36);
  if ((cVar2 == 1) || (cVar2 = fn_80237F74(PC_PTR(arg0),arg1,0x10), cVar2 == 1)) {
    uVar3 = fn_80239984(0,PC_PTR(arg0),0x1e0);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(arg1));
    fn_80239EE8(0xec64,arg0,uVar1,0,0,arg2,0,0x1e0);
  }
  cVar2 = fn_80237F74(PC_PTR(arg0),arg3,0x36);
  if ((cVar2 == 1) || (cVar2 = fn_80237F74(PC_PTR(arg0),arg3,0x10), cVar2 == 1)) {
    uVar3 = fn_80239984(uVar3,PC_PTR(arg0),0x1e1);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(arg1));
    fn_80239EE8(0xec64,arg0,uVar1,0,0,arg2,0,0x1e1);
  }
  return uVar3;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

extern u32* waza_data_number;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut();
#endif
extern s32 fightTrainerAiWazaValueJisin();
extern s32 fightTrainerAiWazaValueJibaku();
extern s32 fightTrainerAiWazaValueNull();
extern u8 fightTrainerAiIsWazaTypeId(int, int, int);
extern u32 fightAiGetLastUseWaza(void* ctx, u32 param);
extern u8 fn_80237F74(void* ctx, u32 param2, u32 param3);
extern u8 fightTrainerAiGetZokusei(u32 ctx, void* param2, u16 target);

/* 0x8023F278 size 0x648: fightTrainerAiWazaValueMamoru */
u32 fightTrainerAiWazaValueMamoru(u32 ctx, u32 poke, u32 msgArg,
                                  u32 otherPoke)
{
    typedef s32 (*WazaValueFunc)();
    u32 party[8];
    u16 moveBuf[10];
    u32 moveCount;
    u32 partyCount;
    u32 finalMoveCount;
    u32 score;
    u16 i;
    u16 j;
    u32 waza;
    WazaValueFunc valueFunc;
    u8 allyJisin;
    u8 allyJibaku;
    u8 queuedJisin;
    u8 queuedJibaku;

    score = 0;
    allyJisin = 0;
    allyJibaku = 0;
    queuedJisin = 0;
    queuedJibaku = 0;

    partyCount = fightFloorGetFightTrainerFightOutPokemonPtrAry(
        0, ctx, party, 1, 1);
    waza = fightAiGetLastUseWaza((void*)PC_PTR(ctx), poke);
    fightAiGetLastUseWaza((void*)PC_PTR(ctx), otherPoke);

    for (i = 0; i < (u16)partyCount; i++) {
        if (party[i] == poke) {
            continue;
        }
        moveCount = (u16)fightTrainerAiGetFightPokemonWazaAry((void*)PC_PTR(ctx), party[i], moveBuf, 0, 1);
        if (moveCount == 0) {
            continue;
        }
        for (j = 0; j < moveCount; j++) {
            valueFunc = (WazaValueFunc)PC_PTR(wazaGetStatus(0, moveBuf[j], 0x1C, 0));
            if (valueFunc == 0) {
                valueFunc = fightTrainerAiWazaValueNull;
            }
            if (valueFunc == fightTrainerAiWazaValueJisin) {
                allyJisin = 1;
            }
            if (valueFunc == fightTrainerAiWazaValueJibaku) {
                allyJibaku = 1;
            }
        }
    }

    for (moveCount = 0; (moveCount & 0xFFFF) < *waza_data_number; moveCount++) {
        valueFunc = (WazaValueFunc)PC_PTR(wazaGetStatus(0, moveCount, 0x1C, 0));
        if (valueFunc == 0) {
            valueFunc = fightTrainerAiWazaValueNull;
        }
        if (valueFunc == fightTrainerAiWazaValueJisin ||
            valueFunc == fightTrainerAiWazaValueJibaku) {
            if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(
                    0, PC_PTR(ctx), 1, 1, PC_PTR(moveCount), 0) == 1) {
                if (valueFunc == fightTrainerAiWazaValueJisin) {
                    queuedJisin = 1;
                }
                if (valueFunc == fightTrainerAiWazaValueJibaku) {
                    queuedJibaku = 1;
                }
            }
        }
    }

    if (allyJisin == 1 || allyJibaku == 1) {
        score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1D7);
        fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0,
                    msgArg, 0, 0x1D7);
    }
    if (queuedJisin == 1 || queuedJibaku == 1) {
        score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1D8);
        fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0,
                    msgArg, 0, 0x1D8);
    }

    if ((u8)fn_80237F74((void*)PC_PTR(ctx), poke, 3) == 1 &&
        fightTrainerAiIsWazaTypeId(ctx, waza, 4) == 1) {
        score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1D9);
        fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0,
                    msgArg, 0, 0x1D9);
    }

    if (((u16)waza == 0xB6 || (u16)waza == 0xC5) &&
        pokemonGetStatus(PC_PTR(poke), 0, POKEMON_STATUS_PROTECT_COUNT, 0) != 0) {
        score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1DA);
        fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0, 0,
                    msgArg, 0, 0x1DA);
    }

    if ((u8)fightTrainerAiGetZokusei(ctx, (void*)PC_PTR(poke), 7) == 1) {
        if (allyJibaku == 1) {
            score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1DB);
            fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0,
                        0, msgArg, 0, 0x1DB);
        }
        if (queuedJibaku == 1) {
            score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1DC);
            fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0,
                        0, msgArg, 0, 0x1DC);
        }
    }

    if ((u8)fightTrainerAiGetZokusei(ctx, (void*)PC_PTR(poke), 2) == 1 ||
        (u8)fn_80237F74((void*)PC_PTR(ctx), poke, 0x1A) == 1 ||
        (u8)fn_80237F74((void*)PC_PTR(ctx), poke, 0x19) == 1) {
        if (allyJisin == 1) {
            score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1DD);
            fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0,
                        0, msgArg, 0, 0x1DD);
        }
        if (queuedJisin == 1) {
            score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1DE);
            fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))), 0,
                        0, msgArg, 0, 0x1DE);
        }
    }

    for (i = 0; i < (u16)partyCount; i++) {
        if (party[i] == poke) {
            continue;
        }
        finalMoveCount = fightTrainerAiGetFightPokemonWazaAry((void*)PC_PTR(ctx), party[i], moveBuf, 0, 1);
        if ((u16)finalMoveCount == 0) {
            continue;
        }
        for (j = 0; j < (u16)finalMoveCount; j++) {
            if (moveBuf[j] == 0x10A) {
                score = fn_80239984(score, (void*)PC_PTR(ctx), 0x1DF);
                fn_80239EE8(0xEC64, ctx, PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(poke))),
                            0, 0, msgArg, 0, 0x1DF);
                break;
            }
        }
        if ((u16)j < (u16)finalMoveCount) {
            break;
        }
    }
    return score;
}

/* 0x8023F8C0 size 0x15C: fightTrainerAiWazaValueTakuwaeru */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueTakuwaeru(void* ctx, u32 param1, u32 param2)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 fightOutPokemonGetPokemonJoutaiStack(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
    s16 val;
    u32 handle = 0;

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x2d)) == 1) {
        val = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(param1), PC_PTR(0x2d));
    } else {
        val = 0;
    }
    if (val == 0) {
        handle = fn_80239984(0, ctx, 0x1d4);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1d4);
    } else if (val == 1) {
        handle = fn_80239984(0, ctx, 0x1d5);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1d5);
    } else if (val == 2) {
        handle = fn_80239984(0, ctx, 0x1d6);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1d6);
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023FA1C size 0x140: fightTrainerAiWazaValueJyuuden */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueJyuuden(void* ctx, u32 param1, u32 param2)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetFightPokemonWazaAry(void*, u32, void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetWazaTenkouValue(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u16 buf[10];
    u32 handle = 0;
    u16 count;
    u16 i;

    count = fightTrainerAiGetFightPokemonWazaAry(ctx, param1, buf, 0, 1);
    for (i = 0; i < count; i++) {
        if (buf[i] == 0x10c) {
            continue;
        }
        if (fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), buf[i], param1) == 0xd) {
            handle = fn_80239984(0, ctx, 0x1d2);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x1d2);
            break;
        }
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x24)) == 1) {
        handle = fn_80239984(handle, ctx, 0x1d3);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x1d3);
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023FB5C size 0x1E8: fightTrainerAiWazaValueNemuriKoudou */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueNemuriKoudou(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonGetNowKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonGetKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u8 cVar3;
  u32 uVar1;
  u32 uVar2;
  s8 cVar4;
  s8 cVar6;
  int iVar7;
  int iVar8;
  u32 uVar5;

  uVar5 = 0;
  cVar3 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3),PC_PTR(r4),PC_PTR(8));
  if (cVar3 == 1) {
    cVar3 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3),PC_PTR(r4),PC_PTR(8));
    if (cVar3 == 0) {
      cVar6 = -1;
    }
    else {
      uVar2 = fn_80237F74(PC_PTR(r3),r4,0x30);
      iVar7 = ((u32)__cntlzw(1 - (uVar2 & 0xff)) >> 5) + 1;
      iVar8 = fightOutPokemonGetNowKaisuuJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_SLEEP));
      iVar7 = iVar8 + iVar7;
      if ((s8)iVar7 >= (cVar4 = fightOutPokemonGetKaisuuJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_SLEEP)))) {
        cVar6 = 1;
      }
      else {
        cVar6 = 0;
      }
    }
    if (cVar6 == 0) {
      uVar5 = fn_80239984(0,PC_PTR(r3),0x1d0);
      uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
      fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x1d0);
    }
    cVar3 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3),PC_PTR(r4),PC_PTR(8));
    if (cVar3 == 0) {
      cVar6 = -1;
    }
    else {
      uVar2 = fn_80237F74(PC_PTR(r3),r4,0x30);
      iVar7 = ((u32)__cntlzw(1 - (uVar2 & 0xff)) >> 5) + 1;
      iVar8 = fightOutPokemonGetNowKaisuuJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_SLEEP));
      iVar7 = iVar8 + iVar7;
      if ((s8)iVar7 >= (cVar4 = fightOutPokemonGetKaisuuJoutaiDataId(PC_PTR(r4),PC_PTR(CONDITION_SLEEP)))) {
        cVar6 = 1;
      }
      else {
        cVar6 = 0;
      }
    }
    if (cVar6 == 1) {
      uVar5 = fn_80239984(uVar5,PC_PTR(r3),0x1d1);
      uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
      fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x1d1);
    }
  }
  return uVar5;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023FD44 size 0x98: fightTrainerAiWazaValueMigawari */
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueMigawari(u32 r3, u32 r4, u32 r5)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetHpPercent();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
    extern const f32 lbl_8047E634;

  u8 cVar2;
  u32 uVar1;
  u32 uVar3;

  uVar3 = 0;
#ifdef TARGET_PC /* fightTrainerAiGetHpPercent(ctx, fightOut, direction, f32 ratio): the ratio goes last on x64 (port-gaps.md 11) */
  cVar2 = fightTrainerAiGetHpPercent(r3, r4, 1, lbl_8047E634);
#else
  cVar2 = fightTrainerAiGetHpPercent((double)lbl_8047E634,r3,r4,1);
#endif
  if (cVar2 == 1) {
    uVar3 = fn_80239984(0,PC_PTR(r3),0x1cf);
    uVar1 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar1,0,0,r5,0,0x1cf);
  }
  return uVar3;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif

/* 0x8023FDDC size 0x2FC: fightTrainerAiWazaValueSawagu */
/* RULE-EXCEPTION(user-approved): local optimisation pragmas for fightTrainerAiWazaValueSawagu only (speed, no lifetime opt, no inlining); the unit builds at -O4,s — see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
#pragma opt_lifetimes off
#define fn_801F1C18 fightFloorGetFightTrainerFightOutPokemonPtrAry
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueSawagu(u32 r3, u32 r4, u32 r5, u32 r6)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F1C18();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonGetNowKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonGetKaisuuJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetFightPokemonWazaAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239EE8();
#endif
  u32 uVar3;
  u32 uVar4;
  s8 cVar5;
  u8 cVar11;
  u32 uVar1;
  s8 cVar6;
  u32 uVar2;
  int iVar8;
  int iVar7;
  u32 uVar10;
  u32 uVar9;
  u32 uVar8;
  u32 local_54 [8];
  u16 local_68 [10];

  uVar9 = 0;
  uVar3 = fn_801F1C18(0,r3,local_54,1,1);
  for (uVar10 = 0; (u16)uVar10 < (u16)uVar3; uVar10 = uVar10 + 1) {
    iVar7 = local_54[(u16)uVar10];
    if (r4 != iVar7) {
      cVar11 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3),PC_PTR(iVar7),PC_PTR(8));
      if (cVar11 == 0) {
        cVar5 = -1;
      }
      else {
        uVar1 = fn_80237F74(PC_PTR(r3),iVar7,0x30);
        uVar8 = ((u32)__cntlzw(1 - (uVar1 & 0xff)) >> 5) + 1;
        iVar8 = fightOutPokemonGetNowKaisuuJoutaiDataId(PC_PTR(iVar7),PC_PTR(CONDITION_SLEEP));
        uVar8 = iVar8 + uVar8;
        if ((s8)uVar8 >= (cVar6 = fightOutPokemonGetKaisuuJoutaiDataId(PC_PTR(iVar7),PC_PTR(CONDITION_SLEEP)))) {
          cVar5 = 1;
        }
        else {
          cVar5 = 0;
        }
      }
      if (cVar5 == 0) {
        uVar9 = fn_80239984(0,PC_PTR(r3),0x1cc);
        uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
        fn_80239EE8(0xec64,r3,uVar2,0,0,r5,0,0x1cc);
        break;
      }
    }
  }
  for (uVar10 = 0; (u16)uVar10 < (u16)uVar3; uVar10 = uVar10 + 1) {
    if (r4 != local_54[(u16)uVar10]) {
      uVar4 = fightTrainerAiGetFightPokemonWazaAry(PC_PTR(r3),local_54[(u16)uVar10],local_68,0,1);
      if ((u16)uVar4 != 0) {
        for (uVar8 = 0; (u16)uVar8 < (u16)uVar4; uVar8 = uVar8 + 1) {
          if (local_68[(u16)uVar8] == 0x9c) {
            uVar9 = fn_80239984(uVar9,PC_PTR(r3),0x1cd);
            uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
            fn_80239EE8(0xec64,r3,uVar2,0,0,r5,0,0x1cd);
            break;
          }
        }
        if ((u16)uVar8 < (u16)uVar4) break;
      }
    }
  }
  cVar11 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3),PC_PTR(r6),PC_PTR(8));
  if (cVar11 == 0) {
    cVar5 = -1;
  }
  else {
    uVar1 = fn_80237F74(PC_PTR(r3),r6,0x30);
    uVar8 = ((u32)__cntlzw(1 - (uVar1 & 0xff)) >> 5) + 1;
    iVar8 = fightOutPokemonGetNowKaisuuJoutaiDataId(PC_PTR(r6),PC_PTR(CONDITION_SLEEP));
    uVar4 = iVar8;
    uVar4 += uVar8;
    if ((s8)uVar4 >= (cVar6 = fightOutPokemonGetKaisuuJoutaiDataId(PC_PTR(r6),PC_PTR(CONDITION_SLEEP)))) {
      cVar5 = 1;
    }
    else {
      cVar5 = 0;
    }
  }
  if (cVar5 == 0) {
    uVar9 = fn_80239984(uVar9,PC_PTR(r3),0x1ce);
    uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(r4));
    fn_80239EE8(0xec64,r3,uVar2,0,0,r5,0,0x1ce);
  }
  return uVar9;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1C18
#endif

/* 0x802400D8 size 0x6C: fightTrainerAiWazaValueOomugaesi */
/* RULE-EXCEPTION(user-approved): restores the unit settings after the tagged local block above — see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
#define fn_8025CB3C fightTrainerAiCheckOumu
u32 fightTrainerAiWazaValueOomugaesi(void* ctx, u32 slot, u16 species, u32 extra) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8023CA9C();
#endif
    extern u32 fn_8025CB3C();
    u32 currentSpecies;
    u32 result;
    currentSpecies = fn_8025CB3C(ctx);
    if ((u16)currentSpecies == species) {
        goto ret_zero;
    }
    if ((u16)currentSpecies == 0) {
        goto ret_zero;
    }
    result = fn_8023CA9C(ctx, slot, currentSpecies, extra);
    goto done;
ret_zero:
    result = 0;
done:
    return result;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8025CB3C
#endif

/* 0x80240144 size 0xAC: fightTrainerAiWazaValueRandamuSentaku */
#define fn_801FB1C0 fightTrainerGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
s32 fightTrainerAiWazaValueRandamuSentaku(void* ctx, u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_801FB1C0(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
    s32 handle;
    s32 scale;
    s32 mod;
    u16 rng;

    scale = fn_801FB1C0(0, 0x1cb, 0x3e, 0);
    rng = GSrand();
    mod = rng % (scale + 1);
    handle = fightTrainerAiAddValue(0, mod);
    fn_80239CCC(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1cb, mod);
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif

/* 0x802401F0 size 0x264: fightTrainerAiWazaValueHurahuradansu */
#define fn_801F1C18 fightFloorGetFightTrainerFightOutPokemonPtrAry
#define fn_801F54A4 fightFloorGetStatus
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueHurahuradansu(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F1C18(u32, void*, u32*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_801F54A4(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetSoubiItem(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 arr[10];
    u32 handle = 0;
    u32 count;
    u16 i;

    count = fn_801F1C18(0, PC_PTR32_IMPLICIT(ctx), arr, 1, 1);
    if (fn_801F54A4(0, 0, 0x18, 0) >= 2) {
        handle = fn_80239984(0, ctx, 0x1c7);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x1c7);
    }
    for (i = 0; i < (u16)count; i++) {
        if (param1 != arr[i]) {
            u16 v = fightTrainerAiGetSoubiItem(ctx, arr[i]);
            if (v == 8 || v == 9) {
                handle = fn_80239984(handle, ctx, 0x1c8);
                fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x1c8);
                break;
            }
        }
    }
    for (i = 0; i < (u16)count; i++) {
        if (arr[i] != param1 && fn_80237F74(ctx, arr[i], 0x14) == 1) {
            handle = fn_80239984(handle, ctx, 0x1c9);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x1c9);
            break;
        }
    }
    for (i = 0; i < (u16)count; i++) {
        if (arr[i] != param1) {
            handle = fn_80239984(handle, ctx, 0x1ca);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(param1)), 0, 0, param2, 0, 0x1ca);
            break;
        }
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F54A4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F1C18
#endif

/* 0x80240454 size 0x16C: fightTrainerAiWazaValueMakibisi */
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F6E98 fightSideIsJoutaiDataId
#define fn_801F6D9C fightSideGetCountAsJoutaiDataId
#define fn_80205B8C fightOutPokemonGetPokemonPtr
u32 fightTrainerAiWazaValueMakibisi(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F6E98(void*, u32);
#endif
    extern s16 fn_801F6D9C(void*, u32);
    u32 handle = 0;
    void* obj = fn_801F025C(2, param3);
    s16 val;

    if (fn_801F6E98(PC_PTR32_IMPLICIT(obj), 0x4a) == 1) {
        val = fn_801F6D9C(obj, 0x4a);
    } else {
        val = 0;
    }
    if (val == 0) {
        handle = fn_80239984(0, ctx, 0x1c4);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1c4);
    } else if (val == 1) {
        handle = fn_80239984(0, ctx, 0x1c5);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1c5);
    } else if (val == 2) {
        handle = fn_80239984(0, ctx, 0x1c6);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fn_80205B8C(PC_PTR(param1))), 0, 0, param2, 0, 0x1c6);
    }
    return handle;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205B8C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6D9C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F6E98
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801F025C
#endif

