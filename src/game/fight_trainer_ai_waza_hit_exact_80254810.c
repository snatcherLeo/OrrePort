/**
 * @file fight_trainer_ai_waza_hit_exact_80254810.c
 * @brief Byte-exact fightTrainerAiWazaHit.cpp range, 0x80254810 - 0x80256ED0.
 */
#include "game/fight_trainer_ai_waza_hit_shared.h"
#include "game/constants/pokemon_status.h"

s32 fightTrainerAiWazaHit191(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 one);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightCheckAccuracyRules(u32 param2, u32 param1, u32 param3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void* ctx, u32 a, u32 type);
#endif
    s32 gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0xFFFF);

    if (fightCheckAccuracyRules(param2, param1, param3) == 1) {
        return 0;
    }
    if (fn_80237F74(ctx, param1, 0x19) == 1) {
        return 0;
    }
    if (fn_80237F74(ctx, param3, 0x19) == 1) {
        return 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit190(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit189(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_802376EC(void* ctx, u32 elem);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    u16 a = fn_802376EC(ctx, param1);
    u16 b = fn_802376EC(ctx, param3);
    s32 gate;

    if (b <= a) {
        return 0;
    }
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);
    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit188(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit187(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void* ctx, u32 elem, u32 type);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void* ctx, u32 elem, u32 type);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType(u32 type, u32 elem);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightSideIsJoutaiDataId(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    extern u8 fightTrainerAiCheckSawagu(void* ctx, u32 a);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void* ctx, u32 a);
#endif
    extern u8 fightAiCheckRule0E0F(u32 type, u32 elem);
    s32 gate;

    if (fn_80237F74(ctx, param3, 0x48) == 1) {
        return 0;
    }
    if (fn_80237F74(ctx, param3, 0xf) == 1) {
        return 0;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x14)) == 1) {
        return 0;
    }
    if (fightSideIsJoutaiDataId(PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, param3)), 0x4b) == 1) {
        return 0;
    }
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);
    if (fightTrainerAiCheckSawagu(ctx, param3) == 1) {
        return 0;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x26)) == 1) {
        return 0;
    }
    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0) {
        return 0;
    }
    if (fightAiCheckRule0E0F(8, param3) != 1) {
        return 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit186(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit185(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

u32 fightTrainerAiWazaHit184(void* ctx, u32 slot)

{
    extern u16 fightAiGetUsedItem();
  u16 sVar1;

  sVar1 = fightAiGetUsedItem(ctx, slot); /* the table's arguments, handed on untouched in r3-r5 (assembly: no register written before the call) */
  return sVar1 != 0;
}

u32 fightTrainerAiWazaHit183(void) { return 1; }

s32 fightTrainerAiWazaHit182(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

#ifdef TARGET_PC
/* PC: r3 / r4 (the AI context and the Pokemon) passed through on the GameCube (verified from the assembly; port-gaps
 * 39) */
int fightTrainerAiWazaHit181(void* ctx, u32 param1, u32 param2, u32 param3)
{
  u32 uVar1;
  (void)param2;
  (void)param3;
  uVar1 = fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x25));
  return (uVar1 & 0xFF) != 1;
}
#else
int fightTrainerAiWazaHit181(void)
{
    u32 r3;
    u32 r4;
    extern u32 fightTrainerAiIsPokemonJoutai();
  u32 uVar1;
  uVar1 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3), PC_PTR(r4), PC_PTR(0x25));
  return (uVar1 & 0xFF) != 1;
}
#endif

u32 fightTrainerAiWazaHit180(u32 r3, u32 r4)
{
    extern u32 fightSeqGetTemotiPokemonNekonoteWazaDataAry();
    u8 auStack_38[0x38];

    u32 uVar1 = fightSeqGetTemotiPokemonNekonoteWazaDataAry(r3, auStack_38, 0x18, r4);
    return (-uVar1 & ~uVar1) >> 0x1f;
}

#ifdef TARGET_PC
/* PC: r3 / r4 passed through on the GameCube (port-gaps 39) */
int fightTrainerAiWazaHit179(void* ctx, u32 param1, u32 param2, u32 param3)
{
  u32 uVar1;
  (void)param2;
  (void)param3;
  uVar1 = fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x35));
  return (uVar1 & 0xFF) != 1;
}
#else
int fightTrainerAiWazaHit179(void)
{
    u32 r3;
    u32 r4;
    extern u32 fightTrainerAiIsPokemonJoutai();
  u32 uVar1;
  uVar1 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3), PC_PTR(r4), PC_PTR(0x35));
  return (uVar1 & 0xFF) != 1;
}
#endif

u32 fightTrainerAiWazaHit178(void* ctx, u32 slot, u32 param, u32 extra) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void*, u32, u32, u32, u32);
#endif
    s32 gate;

    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(slot), param, PC_PTR(extra), 0xFFFF);
    if (fn_80237F74(ctx, extra, 0x19) == 1) {
        return 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit177(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetSoubiItemOf(void* ctx, u32 elem);
#endif
    extern u8 fightAiCanStealItem(u32 elem);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80142984(u16 elem);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void* ctx, u32 a, u32 type);
#endif
    u16 A = fightAiGetSoubiItemOf(ctx, param1);
    u16 B = fightAiGetSoubiItemOf(ctx, param3);

    if (fightAiCanStealItem(param1) == 0) {
        return 0;
    }
    if (A == 0 && B == 0) {
        goto ret0;
    }
    if (A == 0xaf) {
        goto ret0;
    }
    if (B == 0xaf) {
        goto ret0;
    }
    if (A != 0 && fn_80142984(A) == 0) {
        goto ret0;
    }
    if (B != 0 && fn_80142984(B) == 0) {
    ret0:
        return 0;
    }
    if (fn_80237F74(ctx, param3, 0x3c) == 1) {
        return 0;
    }
    return 1;
}

u32 fightTrainerAiWazaHit176(void* ctx, u32 slot, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
    u32 target;

    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0xe, slot));
    if (target == 0) {
        return 0;
    }
    if ((fightFloorGetStatus(0, 0, 0x19, 0) & 0xFFFF) < 2) {
        goto fail;
    }
    if (fightOutPokemonCheckFightOut(PC_PTR(target)) != 1) {
        goto fail;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(slot), PC_PTR(0x32)) != 0) {
        goto fail;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(target), PC_PTR(0x32)) == 0) {
        goto success;
    }
fail:
    return 0;
success:
    return 1;
}

u32 fightTrainerAiWazaHit175(void* ctx, u32 slot, u32 param, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fightTrainerAiWazaHitCheck(void*, u32, u32, u32, u32);
#endif
    u32 result;

    result = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(slot), param, PC_PTR(param3), 0);
    if ((fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x30)) & 0xff) == 1) {
        return 0;
    }
    if ((s32)result == 0) {
        return 0;
    }
    if ((s32)result == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit174(void) { return 1; }

u32 fightTrainerAiWazaHit173(void* ctx, u32 slot, u32 param, u32 extra) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus(u32, u32, u32, u32);
#endif
    extern u32 tikeiDataBiosGetWazaId(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaHitValue(void*, u32, u32, u32);
#endif
    u32 paramType;
    u32 other;
    u32 otherType;

    other = tikeiDataBiosGetWazaId(fightFloorGetStatus(0, 0, 0xf, 0) & 0xFFFF);
    paramType = wazaGetStatus(0, param, 9, 0) & 0xFFFF;
    param = other;
    otherType = wazaGetStatus(0, param, 9, 0) & 0xFFFF;
    if (otherType != paramType) {
        return fightTrainerAiGetWazaHitValue(ctx, slot, param, extra);
    }
    return 1;
}

u32 fightTrainerAiWazaHit172(void) { return 1; }

s32 fightTrainerAiWazaHit171(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit170(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit169(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit168(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetAbicntPhyAtk(void* ctx, u32 elem);
#endif
    extern u8 fightTrainerAiGetAbicntSpeAtk(void* ctx, u32 elem);
    extern u8 fightTrainerAiCheckGuard(void* ctx, u32 param3, u32 param2);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiCheckAbiCnt(void*, u32, u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void* ctx, u32 param3, u32 flag);
#endif
    u8 q1 = fightTrainerAiGetAbicntPhyAtk(PC_PTR32_IMPLICIT(ctx), param3);
    u8 q2 = fightTrainerAiGetAbicntSpeAtk(ctx, param3);

    if (fightTrainerAiCheckGuard(ctx, param3, param2) == 1) {
        return 0;
    }
    if (q1 == 0 && q2 == 0) {
        return 0;
    }
    if (fightTrainerAiCheckAbiCnt(ctx, param1, param3, param2, 0xa0, 1, 1) == 0 &&
        fightTrainerAiCheckAbiCnt(ctx, param1, param3, param2, 0xa0, 4, 1) == 0) {
        return 0;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x14)) == 1) {
        return 0;
    }
    return 1;
}

u32 fightTrainerAiWazaHit167(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetZokusei();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightSideIsJoutaiDataId();
#endif
    s32 handle;
    u8 flag;

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x14)) == 1) return 0;
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x6)) == 1) return 0;
    if (fightTrainerAiGetZokusei(PC_PTR32_IMPLICIT(ctx), PC_PTR(param3), 0xA) == 1) return 0;
    if (fn_80237F74(ctx, param3, 0x29) == 1) return 0;

    if (fn_80237F74(ctx, param3, 0x11) == 1) goto flag1;
    if (fn_80237F74(ctx, param3, 0x14) == 1) goto flag1;
    if (fn_80237F74(ctx, param3, 0x7) == 1) goto flag1;
    if (fn_80237F74(ctx, param3, 0xF) == 1) goto flag1;
    if (fn_80237F74(ctx, param3, 0x48) == 1) goto flag1;
    if (fn_80237F74(ctx, param3, 0x29) == 1) {
        flag = 0;
        goto check;
    }
    if (fn_80237F74(ctx, param3, 0x28) != 1) {
        fn_80237F74(ctx, param3, 0xC);
    }
flag1:
    flag = 1;
check:
    if (flag == 0) return 0;

    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0) return 0;

    handle = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);
    if (fightSideIsJoutaiDataId(PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, param3)), 0x4B) == 1) return 0;
    if (handle == 0) return 0;
    if (handle == -1) return 1;
    return 1;
}

s32 fightTrainerAiWazaHit166(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType(u32 type, u32 elem);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightSideIsJoutaiDataId(u32 a, u32 b);
#endif
    extern u8 fightTrainerAiGetAbicntNimbleness(void* ctx, u32 elem);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void* ctx, u32 elem, u32 type);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void* ctx, u32 elem, u32 type);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiCheckAbiCnt(void* ctx, u32 param1, u32 param3, u32 param2, u32 a, u32 b, u32 c);
#endif
    u32 side = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, param3));
    u8 abiVal = fightTrainerAiGetAbicntNimbleness(ctx, param3);
    s32 gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);
    u8 flag;

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x9)) == 1 && abiVal >= 0xc) {
        if (fightTrainerAiCheckAbiCnt(ctx, param1, param3, param2, 0x10, 4, 1) == 0) {
            return 0;
        }
    }
    if (fn_80237F74(ctx, param3, 0x14) == 1) {
        return 0;
    }
    if (fn_80237F74(ctx, param3, 0x11) == 1) goto allowedLabel;
    if (fn_80237F74(ctx, param3, 0x14) == 1) {
        flag = 0;
        goto flagCheckDone;
    }
    if (fn_80237F74(ctx, param3, 0x7) == 1) goto allowedLabel;
    if (fn_80237F74(ctx, param3, 0xf) == 1) goto allowedLabel;
    if (fn_80237F74(ctx, param3, 0x48) == 1) goto allowedLabel;
    if (fn_80237F74(ctx, param3, 0x29) == 1) goto allowedLabel;
    if (fn_80237F74(ctx, param3, 0x28) == 1) goto allowedLabel;
    fn_80237F74(ctx, param3, 0xc);
allowedLabel:
    flag = 1;
flagCheckDone:
    if (flag == 0) {
        return 0;
    }
    if (fightSideIsJoutaiDataId(side, 0x4b) == 1) {
        return 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit165(void* ctx, u32 slot, u32 param, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fightTrainerAiWazaHitCheck(void*, u32, u32, u32, u32);
#endif
    u32 result;

    result = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(slot), param, PC_PTR(param3), 0);
    if ((fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1b)) & 0xff) == 1) {
        return 0;
    }
    if ((s32)result == 0) {
        return 0;
    }
    if ((s32)result == -1) {
        return 1;
    }
    return 1;
}

#ifdef TARGET_PC
/* PC: r3 (the AI context) passed through on the GameCube (verified from the assembly; port-gaps 39): fails while
 * the weather is already 4 */
int fightTrainerAiWazaHit164(void* ctx, u32 param1, u32 param2, u32 param3)
{
  u32 uVar1;
  (void)param1;
  (void)param2;
  (void)param3;
  uVar1 = fightTrainerAiGetNowTenkou(ctx,0,0);
  return (uVar1 & 0xff) != 4;
}
#else
int fightTrainerAiWazaHit164(void)

{
    u32 r3;

    extern u32 fightTrainerAiGetNowTenkou();
  u32 uVar1;

  uVar1 = fightTrainerAiGetNowTenkou(PC_PTR(r3),0,0);
  return (uVar1 & 0xff) != 4;
}
#endif

u32 fightTrainerAiWazaHit162(void* ctx, u32 slot, u32 param) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E648;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(slot), PC_PTR(0x2d)) == 0) {
        return 0;
    }
    return fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), slot, 0, lbl_8047E648) != 1;
}

s32 fightTrainerAiWazaHit161(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void* ctx, u32 param1, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x2d)) == 0) {
        gate = 0;
    }
    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit160(void* ctx, u32 slot, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void* ctx, u32 slot, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonJoutaiStack(u32 slot, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiDataBiosGetMaxCount(u32 param);
#endif
    u32 result;

    result = slot;
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(slot), PC_PTR(0x2d)) == 0) {
        result = 0;
    } else {
        result = fightOutPokemonGetPokemonJoutaiStack(PC_PTR(result), PC_PTR(0x2d));
    }
    if ((s16)result >= (s32)joutaiDataBiosGetMaxCount(0x2d)) {
        return 0;
    }
    return 1;
}

s32 fightTrainerAiWazaHit159(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit158(u32 r3_in, u32 r4)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
  u32 uVar1;
  u8 result;
  uVar1 = pokemonGetStatus(PC_PTR(r4), 0, POKEMON_STATUS_TURNS_ON_FIELD, 0) & 0xFFFF;
  result = uVar1 != 0;
  return result;
}

u32 fightTrainerAiWazaHit157(void* ctx, u32 slot, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E648;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif

    return fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), slot, 0, lbl_8047E648) != 1;
}

u32 fightTrainerAiWazaHit156(void* ctx, u32 slot, u32 param, u32 arg3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiCheckAbiCnt();
#endif
    u32 result = fightTrainerAiCheckAbiCnt(ctx, slot, arg3, param, 0x10, 0x2, 0x41) & 0xFF;
    return (result != 0) ? 1 : 0;
}

s32 fightTrainerAiWazaHit155(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit154(u32 r3, u32 r4, u32 r5, u32 r6)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int _fightTrainerAiWazaHitCheck();
#endif
  int iVar1;
  iVar1 = _fightTrainerAiWazaHitCheck(PC_PTR(r3), PC_PTR(r4), r5, PC_PTR(r6), 0);
  if (iVar1 == 0) {
    return 0;
  }
  if (iVar1 == -1) {
    return 1;
  }
  return 1;
}

u32 fightTrainerAiWazaHit153(void) { return 0; }

s32 fightTrainerAiWazaHit152(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit151(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit150(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit149(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

int fightTrainerAiWazaHit148(u32 r3, u32 r4, u32 r5, u32 r6)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
  extern u32 fightTrainerAiIsPokemonJoutai();
#endif
  u32 uVar1;

  uVar1 = fightTrainerAiIsPokemonJoutai(PC_PTR(r3),PC_PTR(r6),PC_PTR(0x34));
  return (uVar1 & 0xff) != 1;
}

s32 fightTrainerAiWazaHit147(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit146(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit145(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit144(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit143(void) { return 1; }

u32 fightTrainerAiWazaHit142(void* ctx, u32 slot, u32 param) {
    extern f32 lbl_8047E64C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetAbicntPhyAtk(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif

    if ((fightTrainerAiGetAbicntPhyAtk(PC_PTR32(ctx), slot) & 0xFF) >= 0xC) { /* r3 / r4 untouched (assembly) */
        goto ret0;
    }
    if ((fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), slot, -1, lbl_8047E64C) & 0xFF) == 0) {
        goto ret1;
    }
ret0:
    return 0;
ret1:
    return 1;
}

s32 fightTrainerAiWazaHit140(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit139(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit138(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

#ifdef TARGET_PC
/* PC: r3 passed through on the GameCube (port-gaps 39): fails while the weather is already 1 */
int fightTrainerAiWazaHit137(void* ctx, u32 param1, u32 param2, u32 param3)
{
  u32 uVar1;
  (void)param1;
  (void)param2;
  (void)param3;
  uVar1 = fightTrainerAiGetNowTenkou(ctx,0,0);
  return (uVar1 & 0xff) != 1;
}
#else
int fightTrainerAiWazaHit137(void)

{
    u32 r3;

    extern u32 fightTrainerAiGetNowTenkou();
  u32 uVar1;

  uVar1 = fightTrainerAiGetNowTenkou(PC_PTR(r3),0,0);
  return (uVar1 & 0xff) != 1;
}
#endif

#ifdef TARGET_PC
/* PC: r3 passed through on the GameCube (port-gaps 39): fails while the weather is already 2 */
int fightTrainerAiWazaHit136(void* ctx, u32 param1, u32 param2, u32 param3)
{
  u32 uVar1;
  (void)param1;
  (void)param2;
  (void)param3;
  uVar1 = fightTrainerAiGetNowTenkou(ctx,0,0);
  return (uVar1 & 0xff) != 2;
}
#else
int fightTrainerAiWazaHit136(void)

{
    u32 r3;

    extern u32 fightTrainerAiGetNowTenkou();
  u32 uVar1;

  uVar1 = fightTrainerAiGetNowTenkou(PC_PTR(r3),0,0);
  return (uVar1 & 0xff) != 2;
}
#endif

s32 fightTrainerAiWazaHit135(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32 v);
#endif
    extern void pokemonGetMezamerupower(u32, void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, s16 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    u16 hi;
    u16 lo;
    s32 gate;

    pokemonGetMezamerupower(PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), &hi, &lo);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);
    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, lo, (s16)hi) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

u32 fightTrainerAiWazaHit134(void* ctx, u32 slot, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E648;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif

    return fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), slot, 0, lbl_8047E648) != 1;
}

u32 fightTrainerAiWazaHit133(void* ctx, u32 slot, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E648;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif

    return fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), slot, 0, lbl_8047E648) != 1;
}

u32 fightTrainerAiWazaHit132(void* ctx, u32 slot, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E648;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif

    return fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), slot, 0, lbl_8047E648) != 1;
}

s32 fightTrainerAiWazaHit130(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit129(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

u32 fightTrainerAiWazaHit128(void* ctx, u32 a, u32 b, u32 c) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerAiGetWazaTenkouValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightAiGetWazaIryoku();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerAiGetAisyou();
#endif
    s32 res1;

    res1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), b, a);
    return !((u16)fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), c, res1, fightAiGetWazaIryoku(ctx, b)) == 0x43);
}

#ifdef TARGET_PC
/* PC: r3 (the AI context) passed through on the GameCube (verified from the assembly; port-gaps 39): works when the
 * trainer has someone to switch in */
u32 fightTrainerAiWazaHit127(void* ctx, u32 param1, u32 param2, u32 param3)
{
  int iVar1;
  u16 local_8[4];
  (void)param1;
  (void)param2;
  (void)param3;

  local_8[0] = 0;
  iVar1 = PC_PTR32_IMPLICIT(fightTrainerGetIrekaeFightPokemon(ctx, local_8));
  return (-iVar1 != 0) ? 1 : 0;
}
#else
u32 fightTrainerAiWazaHit127(void)
{
    extern int fightTrainerGetIrekaeFightPokemon();
  u32 r3;
  int iVar1;
  u16 local_8[4];

  local_8[0] = 0;
  iVar1 = PC_PTR32_IMPLICIT(fightTrainerGetIrekaeFightPokemon(r3, local_8));
  return (-iVar1 != 0) ? 1 : 0;
}
#endif

s32 fightTrainerAiWazaHit126(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit125(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

u32 fightTrainerAiWazaHit124(void* ctx, u32 slot, u32 param) /* AI table signature (like fightTrainerAiWazaHit112): slot reaches the call below in r4 */

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
    extern u32 fightSideCheckWriteJoutaiDataId();
  u32 uVar1;
  u32 uVar2;

  uVar1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, slot));
  uVar2 = fightSideCheckWriteJoutaiDataId(uVar1,0x4b);
  uVar2 = __cntlzw(2 - (uVar2 & 0xff));
  return uVar2 >> 5;
}

s32 fightTrainerAiWazaHit123(void* ctx, u32 param1, u32 param2, u32 param3) {
    s32 gate = wazaHitGateCheck(ctx, param1, param2, param3);
    s32 result;
    if (gate == 0) {
        result = 0;
    } else if (gate == -1) {
        result = 1;
    } else {
        result = 1;
    }
    return result;
}

s32 fightTrainerAiWazaHit122(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}

s32 fightTrainerAiWazaHit121(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void* ctx, u32 param2, u32 param1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void* ctx, u32 param2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void* ctx, u32 param3, u32 v1, u32 v3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 _fightTrainerAiWazaHitCheck(void* ctx, u32 param1, u32 param2, u32 param3, u32 zero);
#endif
    s32 gate;
    u32 v1 = fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), param2, param1);
    gate = _fightTrainerAiWazaHitCheck(ctx, PC_PTR(param1), param2, PC_PTR(param3), 0);

    if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param3, v1, fightAiGetWazaIryoku(ctx, param2)) == 0x43) {
        gate = 0;
    }
    if (gate == 0) {
        return 0;
    }
    if (gate == -1) {
        return 1;
    }
    return 1;
}
