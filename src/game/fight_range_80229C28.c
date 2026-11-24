/**
 * @file fight_range_80229C28.c
 * @brief Hit check, 0x80229C28 - 0x8022A504 (fightIsWazaBlockedByMamoru, wsCmd40,
 *        WS_HITCHECK).
 *
 * Standalone source at GC/1.3.2 -O4,s. WS_HITCHECK's register walls were two
 * inline blocks: the defender-ability block (Soundproof, ability 0x2b,
 * against a sound move), fightSeqCheckTokuseiBlock, and the move copy after
 * the field loop, fightSeqCopyWaza. Its checkValue read takes the move slot
 * as the fourth argument, and the miss path sets the flag before the PC.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/move_effects.h"
#include "game/constants/moves.h"
#include "game/constants/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/pokemon_status.h"
extern u8* lbl_8047B610;
extern u8 lbl_80478D78[8];
#define fn_8011BEB4 wazaGetStatus
u8 fightIsWazaBlockedByMamoru(void* ctx, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u8 val = (u8)fn_8011BEB4(0, param, 0xe, 0);

    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x2b)) == 1 && val == 1) {
        return 1;
    }
    return 0;
}

void wsCmd40(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightCheckTokuseiWazaKyuushuu();
#endif
    extern u32 ServerStatusFlag;
  u32 uVar1;
  u32 initialFlag;
  u8 cVar6;
  u8 cVar7;
  /* RULE-EXCEPTION(user-approved): one-member struct locals used only for
   * register colouring -- see docs/RULE_EXCEPTIONS.md. */
  struct {
    u32 value;
  } uVar2;
  struct {
    u32 value;
  } uVar3;
  u32 uVar4;
  u32 uVar5;

  uVar1 = fightOutPokemonGetUseWazaDataId(fightTargetGetPtrAsNowFightType(0x11,0));
  initialFlag = wazaGetStatus(0,uVar1,0xe,0) & 0xff;
  uVar1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12,0));
  cVar7 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar1),PC_PTR(0x2b));
  if ((cVar7 == 1) && (initialFlag == 1)) {
    fightFloorSetStatus(0,0,0x3b,0,0x40);
    uVar1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
    uVar2.value = (int)pokemonGetStatus(PC_PTR(uVar1),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
    uVar3.value = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12,0));
    cVar6 = fightWazaIsHit(PC_PTR(uVar2.value));
    if (cVar6 == 0) {
      pokemonSetStatus((void*)PC_PTR(uVar3.value),0,POKEMON_STATUS_LAST_HIT_BY_MOVE,0,0);
      pokemonSetStatus((void*)PC_PTR(uVar3.value),0,POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE,0,9);
      lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
    }
    else {
      uVar2.value = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
      uVar4 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2,uVar2.value));
      uVar2.value = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12,0));
      uVar5 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2,uVar2.value));
      cVar6 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar2.value),PC_PTR(0x15));
      if (cVar6 == 1) {
        if (uVar4 != uVar5) {
          if ((ServerStatusFlag & 0x1000000) == 0) {
            ServerStatusFlag = ServerStatusFlag | 0x40;
          }
        }
      }
      cVar6 = fightCheckTokuseiWazaKyuushuu(uVar1,uVar3.value,0);
      if (cVar6 == 0) {
        lbl_8047B610 = lbl_8047B610 + 5;
      }
    }
    lbl_80478D78[6] = 1;
  }
  else {
    lbl_8047B610 = lbl_8047B610 + 5;
  }
  return;
}

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
extern u32 ServerStatusFlag;
extern u8 lbl_8037989E[];
/* RULE-EXCEPTION(user-approved): single-use inline helper, register-only
 * evidence -- see docs/RULE_EXCEPTIONS.md. */
static inline u8 fightSeqCheckTokuseiBlock(u32 attacker, u32 defender)
{
    u32 move;
    u16 ability;
    u8 blocked;

    fightOutPokemonGetTokuseiDataId(PC_PTR(attacker));
    move = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
    ability = fightOutPokemonGetTokuseiDataId(PC_PTR(defender));
    blocked = 0;
    switch (ability) {
    case ABILITY_SOUNDPROOF:
        if (fightOutPokemonCheckFightOut(PC_PTR(defender)) != 0 && (u8)wazaGetStatus(0, move, 0x17, 0) == 1) {
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x22)) == 1) {
                ServerStatusFlag |= 0x800;
            }
            blocked = 1;
            lbl_8047B610 = lbl_8037989E;
        }
        break;
    }
    return blocked;
}
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonGetStatus();
#endif
extern void fightWazaBiosCopy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSetStatus();
#endif
/* RULE-EXCEPTION(user-approved): single-use inline helper, register-only
 * evidence -- see docs/RULE_EXCEPTIONS.md. */
static inline void fightSeqCopyWaza(u32 attacker, u32 other)
{
    u32 mine;
    u32 theirs;
    u16 last;

    mine = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    theirs = pokemonGetStatus(PC_PTR(other), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    if (theirs != 0 && mine != 0) {
        last = wazaGetStatus(PC_PTR(theirs), 0, 0x28, 0);
        fightWazaBiosCopy(theirs, mine);
        wazaSetStatus(PC_PTR(theirs), 0, 0x27, 0, last);
    }
}
void WS_HITCHECK(void)
{
    struct HitCheckLoopState {
      u32 attacker;
      u32 result;
    };
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonInitJoutaiKeep();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightWazaWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaCheckWriteJoutaiDataId();
#endif
    extern void fightWazaBiosCopy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern u32 _fightCheckYokodoriSub();
    extern u32 _wsHitCheckSub();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 _wsHitCheckJoutaiSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern u8 lbl_80375FBF[];
    extern u8 lbl_8037989E[];
    extern u8 lbl_80379021[];
    extern u8 lbl_8037917B[];
    extern u8 lbl_803796F3[];
    extern u8 lbl_8047B614;
    extern u32 ServerStatusFlag;
    extern void* lbl_8047B62C;
  u32 bVar1;
  u32 iVar2;
  u32 iVar4;
  u32 uVar3;
  u16 floorState;
  u8 cVar12;
  u32 iVar6;
  u32 uVar7;
  u8 moveFlag;
  u8 cVar13;
  s8 cVar14;
  u32 uVar8;
  s8 bVar15;
  u32 iVar9;
  u16 attackerAbility;
  u16 moveEffect;
  u8 checkValue;
  u8 specialResult;
  s32 hitResult;
  u8 moveSlot;
  s32 status114;

  struct HitCheckLoopState loopState;

  iVar2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11,0));
  uVar3 = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(iVar2));
  iVar4 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12,0));
  floorState = std103_battleGetResult();
  if (floorState != 0) {
    lbl_8047B614 = 2;
  }
  else {
    cVar12 = fightOutPokemonCheckFightOut(PC_PTR(iVar2));
    if ((cVar12 == 0) && ((ServerStatusFlag & 0x200) == 0)) {
      ServerStatusFlag = ServerStatusFlag | 0x80000;
      lbl_8047B610 = lbl_80375FBF;
    }
    else {
      cVar12 = _wsHitCheckJoutaiSub();
      if (cVar12 == 0) {
        if (fightSeqCheckTokuseiBlock(iVar2, iVar4) == 0) {
          iVar6 = pokemonGetStatus(PC_PTR(iVar2),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
          uVar7 = fightOutPokemonGetUseWazaDataId((void*)PC_PTR(iVar2));
          moveFlag = wazaGetStatus(0,uVar7,0xe,0);
          cVar13 = wazaGetStatus(0,uVar7,0xf,0);
          cVar14 = wazaGetStatus(PC_PTR(iVar6),0,0x26,0);
          if (((cVar14 < 0) || ((uVar7 & 0xffff) == 0xa5)) || ((uVar7 & 0xffff) == 0x164)) {
            checkValue = 0;
          }
          else {
            checkValue = pokemonGetStatus(PC_PTR(uVar3),0,POKEMON_STATUS_MOVE_PP,cVar14);
          }
          if (checkValue != 0 || (uVar7 & 0xffff) == 0xa5 ||
              (uVar7 & 0xffff) == 0x164) {
            goto normal_hit;
          }
          if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar2),PC_PTR(0x22)) != 0 || (ServerStatusFlag & 0x800200) != 0) {
            goto normal_hit;
          }
          {
            specialResult = fightWazaCheckWriteJoutaiDataId(PC_PTR(iVar6),0x40);
            if (specialResult == 2) {
              fightWazaWriteJoutaiDataId(iVar6,0x40,0);
            }
            ServerStatusFlag = ServerStatusFlag | 0x80000;
            lbl_8047B610 = lbl_80379021;
            return;
          }
        normal_hit:
          {
            uVar8 = ServerStatusFlag;
            ServerStatusFlag = uVar8 & 0xff7fffff;
            if ((uVar8 & 0x2000000) == 0) {
              if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar2),PC_PTR(0x22)) == 0) {
                hitResult = (u8)_wsHitCheckSub(iVar2);
                if (hitResult != 0) {
                  if (hitResult == 2) {
                    ServerStatusFlag = ServerStatusFlag | 0x2000000;
                    return;
                  }
                  specialResult = fightWazaCheckWriteJoutaiDataId(PC_PTR(iVar6),0x40);
                  if (specialResult != 2) {
                    return;
                  }
                  fightWazaWriteJoutaiDataId(iVar6,0x40,0);
                  return;
                }
              }
            }
            ServerStatusFlag = ServerStatusFlag | 0x2000000;
            specialResult = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar4),PC_PTR(0x37));
            if ((specialResult == 1) && (cVar13 == 1)) {
              if (((iVar2 != 0) && (iVar4 != 0)) &&
                  (attackerAbility = fightOutPokemonGetTokuseiDataId(PC_PTR(iVar2)),
                   attackerAbility == ABILITY_PRESSURE))
              {
                uVar3 = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(iVar4));
                bVar15 = pokemonSearchWazaDataId(PC_PTR(uVar3),0x115);
                if (bVar15 >= 0) {
                  s32 slot = bVar15;
                  checkValue = pokemonGetStatus(PC_PTR(uVar3),0,POKEMON_STATUS_MOVE_PP,slot);
                  if (checkValue != 0) {
                    checkValue = checkValue - 1;
                  }
                  pokemonSetStatus((void*)PC_PTR(uVar3),0,POKEMON_STATUS_MOVE_PP,slot,checkValue);
                  moveSlot = (u8)bVar15;
                  if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar4),PC_PTR(0x10)) != 0) {
                    goto after_hensin_update;
                  }
                  if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar4),PC_PTR(0x31)) != 1) {
                    goto after_hensin_update;
                  }
                  uVar8 = fightOutPokemonGetPokemonJoutai08(iVar4,0x31);
                  if ((uVar8 & 1 << moveSlot) != 0) {
                    goto after_hensin_update;
                  }
                  if (fightOutPokemonIsUseHensinBuff(PC_PTR(iVar4)) != 1) {
                    goto after_hensin_update;
                  }
                  fightOutPokemonSetHensinPokemonStatusId(PC_PTR(iVar4),0x80,moveSlot,0);
                after_hensin_update:;
                }
              }
              fightOutPokemonWriteJoutaiDataId(PC_PTR(iVar4),PC_PTR(0x37));
              iVar9 = pokemonGetStatus(PC_PTR(iVar4),0,POKEMON_STATUS_BATTLE_MOVE_PTR,0);
              wsRunScript(lbl_8047B62C,lbl_8037917B,0);
              if ((iVar6 != 0) && (iVar9 != 0)) {
                fightWazaBiosCopy(iVar9,iVar6);
                wazaSetStatus(PC_PTR(iVar9),0,0x27,0,uVar7 & 0xffff);
              }
            }
            else {
              loopState.attacker = iVar2;
              loopState.result = 0;
              fightFloorLoopValidFightOutPokemon(0,_fightCheckYokodoriSub,&loopState,1);
              if (loopState.result != 0) {
                fightSeqCopyWaza(iVar2, loopState.result);
              }
              status114 = pokemonGetStatus(PC_PTR(iVar4),0,POKEMON_STATUS_LIGHTNINGROD_FLAG,0);
              if (status114 == 1) {
                pokemonSetStatus((void*)PC_PTR(iVar4),0,POKEMON_STATUS_LIGHTNINGROD_FLAG,0,0);
                wsRunScript(lbl_8047B62C,lbl_803796F3,0);
              }
              specialResult = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar4),PC_PTR(0x2b));
              if ((specialResult == 1) && (moveFlag == 1)) {
                if (((uVar7 & 0xffff) == MOVE_CURSE) &&
                    (specialResult = fightOutPokemonIsZokuseiDataId(PC_PTR(iVar2),TYPE_GHOST),
                     specialResult == 0))
                {
                  lbl_8047B610 += 1;
                  return;
                }
                moveEffect = wazaGetStatus(0,uVar7,9,0);
                if (((((moveEffect == MOVE_EFFECT_SKULL_BASH) || (moveEffect == MOVE_EFFECT_RAZOR_WIND)) ||
                       (moveEffect == MOVE_EFFECT_SKY_ATTACK)) ||
                      ((moveEffect == MOVE_EFFECT_SOLARBEAM || (moveEffect == MOVE_EFFECT_SEMI_INVULNERABLE)))) ||
                    (moveEffect == MOVE_EFFECT_BIDE)) {
                  bVar1 = 1;
                }
                else {
                  bVar1 = 0;
                }
                if (((u8)bVar1 == 0) ||
                    (specialResult = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(iVar2),PC_PTR(0x22)), specialResult == 1)) {
                  fightOutPokemonInitJoutaiKeep(PC_PTR(iVar2));
                  fightFloorSetStatus(0,0,0x3b,0,0x40);
                  pokemonSetStatus((void*)PC_PTR(iVar4),0,POKEMON_STATUS_LAST_HIT_BY_MOVE,0,0);
                  pokemonSetStatus((void*)PC_PTR(iVar4),0,POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE,0,9);
                  lbl_80478D78[6] = 1;
                  lbl_8047B610 += 1;
                  return;
                }
              }
              lbl_8047B610 += 1;
            }
          }
        }
      }
    }
  }
  return;
}
