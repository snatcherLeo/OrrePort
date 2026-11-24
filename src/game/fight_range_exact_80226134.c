/**
 * @file fight_range_exact_80226134.c
 * @brief Exact pure-C fight-sequence island, 0x80226134 - 0x802274F0.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/battle_messages.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"


extern u8* lbl_8047B610;
extern u8 lbl_8047B614;
extern u8 lbl_80478D78[8];
extern u32 ServerStatusFlag;
extern void* lbl_8047B62C;
extern u8 lbl_80379F58[];
extern u8 lbl_80379A22[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void*, void*, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightSeqWazaTypeCalc(u32, u8, u8, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightApplyDamageVariance(u8, u8);
#endif

#define fn_801F54A4 fightFloorGetStatus
#define fn_802624CC fightMenuOpenMsg
void wsCmd14(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802624CC();
#endif
    u32* arr = DISC_PTR_AT(u32, lbl_8047B610 + 1);
    u32 val;

    if (arr != NULL) {
        val = DISC_U32(&arr[lbl_80478D78[5]]); /* the message-id table is raw big-endian data on the PC */
    } else {
        val = fn_801F54A4(0, 0, 0x50, 0);
    }
    if ((u8)fn_802624CC(val) == 1) {
        lbl_80478D78[7] = 1;
    }
    lbl_8047B610 = lbl_8047B610 + 5;
}

#define fn_801F54A4 fightFloorGetStatus
#define fn_802624CC fightMenuOpenMsg
void wsCmd13(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802624CC();
#endif
    u32* arr = DISC_PTR_AT(u32, lbl_8047B610 + 1);
    u32 val;

    if (arr != NULL) {
        val = DISC_U32(&arr[lbl_80478D78[5]]); /* the message-id table is raw big-endian data on the PC */
    } else {
        val = fn_801F54A4(0, 0, 0x50, 0);
    }
    if ((u8)fn_802624CC(val) == 1) {
        lbl_80478D78[7] = 1;
    }
    lbl_8047B610 = lbl_8047B610 + 5;
}

/* Retail callers pass the frame count in r3; battle_main.c's fightMainWaitFrame
 * body forwards it, unread, to dbgMenuFightGetMsgSpeedToFrame(u32). */
extern void fightMainWaitFrame(u32 frames);
extern void fightMenuCloseMsg(void);
#define fn_801F000C fightMainWaitFrame
#define fn_8026246C fightMenuCloseMsg
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd12(void) {
    if (lbl_80478D78[7] != 0) {
#ifdef TARGET_PC /* fightMenuCloseMsg reads no argument: the nesting only reproduces the register flow */
        fn_801F000C(DISC_U16(lbl_8047B610 + 1));
        fn_8026246C();
#else
        fn_8026246C(fn_801F000C(DISC_U16(lbl_8047B610 + 1)));
#endif
        lbl_80478D78[7] = 0;
    }
    lbl_8047B610 += 3;
}
#else
void wsCmd12(void) {
    if (lbl_80478D78[7] != 0) {
        fn_801F000C(*(u16*)(lbl_8047B610 + 1));
        fn_8026246C();
        lbl_80478D78[7] = 0;
    }
    lbl_8047B610 += 3;
}
#endif

#define fn_802624CC fightMenuOpenMsg
void wsCmd11(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802624CC();
#endif
    if (fn_802624CC(DISC_U32(lbl_8047B610 + 1)) == 1) {
        lbl_80478D78[7] = 1;
    }
    lbl_8047B610 += 3;
}

#define fn_802624CC fightMenuOpenMsg
void wsCmd10(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802624CC();
#endif
    if (fn_802624CC(DISC_U32(lbl_8047B610 + 1)) == 1) {
        lbl_80478D78[7] = 1;
    }
    lbl_8047B610 += 5;
}

#define fn_8012640C pokemonGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_801F4C14 fightFloorSetStatus
#define fn_802037DC fightOutPokemonGetNicknamePtr
#define fn_80209618 fightWazaIsMix
#define fn_8020990C fightWazaIsJoutaiDataId
#define fn_80209960 fightWazaInitJoutaiDataId
#define fn_80262334 fightMenuWazaKoukaMsg
void wsCmd0F(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802037DC();
#endif
    extern u8 fn_80209618();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8020990C();
#endif
    extern void fn_80209960();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
    extern u8 fn_80262334();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetSoubiItemDataId();
#endif
    extern void* lbl_8047B62C;
    extern int lbl_80279D18[];
    extern u8 lbl_80379BE7[];
    extern u8 lbl_803791C7[];
    extern u8 lbl_803791BE[];
  u32 uVar1;
  u32 uVar2;
  u32 iVar5;
  u32 uVar3;
  u8 cVar4;

  uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar1 = (int)fn_8012640C(PC_PTR(uVar2),0,0xd9,0);
  uVar2 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  fn_802037DC(PC_PTR(uVar2));
  uVar3 = fightOutPokemonGetSoubiItemDataId(PC_PTR(uVar2));
  lbl_80478D78[7] = 0;
  iVar5 = 0;
  cVar4 = fn_8020990C(PC_PTR(uVar1),0x40);
  if (cVar4 == 1) {
    cVar4 = fn_8020990C(PC_PTR(uVar1),0x43);
    if (cVar4 == 0) goto _body;
  }
  cVar4 = fn_8020990C(PC_PTR(uVar1),0x40);
  if (cVar4 != 1) goto _else;
  if (lbl_80478D78[6] < 3) goto _else;
_body:
  iVar5 = lbl_80279D18[lbl_80478D78[6]];
  goto _join;
_else:
  {
    cVar4 = fn_80209618(uVar1);
    if (cVar4 == 0) {
      cVar4 = fn_8020990C(PC_PTR(uVar1),0x41);
      if (cVar4 == 1) {
        iVar5 = BATTLE_MSG_SUPER_EFFECTIVE;
      }
      else {
        cVar4 = fn_8020990C(PC_PTR(uVar1),0x42);
        if (cVar4 == 1) {
          iVar5 = BATTLE_MSG_NOT_VERY_EFFECTIVE;
        }
        else {
          cVar4 = fn_8020990C(PC_PTR(uVar1),0x43);
          if (cVar4 == 1) {
            iVar5 = BATTLE_MSG_NO_EFFECT;
          }
          else {
            cVar4 = fn_8020990C(PC_PTR(uVar1),0x44);
            if (cVar4 == 1) {
              iVar5 = 0x7632;
            }
            else {
              cVar4 = fn_8020990C(PC_PTR(uVar1),0x45);
              if (cVar4 == 1) {
                iVar5 = BATTLE_MSG_BUT_IT_FAILED;
              }
              else {
                cVar4 = fn_8020990C(PC_PTR(uVar1),0x46);
                if (cVar4 == 1) {
                  iVar5 = 0x75c9;
                }
                else {
                  cVar4 = fn_8020990C(PC_PTR(uVar1),0x47);
                  if (cVar4 == 1) {
                    fn_80209960(uVar1,0x46);
                    fn_80209960(uVar1,0x47);
                    fn_801F4C14(0,0,0x56,0,uVar3 & 0xffff);
                    fn_801F4C14(0,0,0x49,0,uVar2);
                    wsRunScript(lbl_8047B62C,PC_PTR32(lbl_80379BE7),0);
                    return;
                  }
                }
              }
            }
          }
        }
      }
    }
    else {
      cVar4 = fn_8020990C(PC_PTR(uVar1),0x43);
      if (cVar4 == 1) {
        iVar5 = BATTLE_MSG_NO_EFFECT;
      }
      else {
        cVar4 = fn_8020990C(PC_PTR(uVar1),0x44);
        if (cVar4 == 1) {
          fn_80209960(uVar1,0x44);
          fn_80209960(uVar1,0x41);
          fn_80209960(uVar1,0x42);
          wsRunScript(lbl_8047B62C,PC_PTR32(lbl_803791C7),0);
          return;
        }
        cVar4 = fn_8020990C(PC_PTR(uVar1),0x46);
        if (cVar4 == 1) {
          fn_80209960(uVar1,0x46);
          fn_80209960(uVar1,0x47);
          wsRunScript(lbl_8047B62C,PC_PTR32(lbl_803791BE),0);
          return;
        }
        cVar4 = fn_8020990C(PC_PTR(uVar1),0x47);
        if (cVar4 == 1) {
          fn_80209960(uVar1,0x46);
          fn_80209960(uVar1,0x47);
          fn_801F4C14(0,0,0x56,0,uVar3 & 0xffff);
          fn_801F4C14(0,0,0x49,0,uVar2);
          wsRunScript(lbl_8047B62C,PC_PTR32(lbl_80379BE7),0);
          return;
        }
        cVar4 = fn_8020990C(PC_PTR(uVar1),0x45);
        if (cVar4 == 1) {
          iVar5 = BATTLE_MSG_BUT_IT_FAILED;
        }
      }
    }
  }
_join:
  if (iVar5 != 0) {
    fn_801F4C14(0,0,0x53,0,iVar5);
    cVar4 = fn_80262334(iVar5,uVar2,uVar3);
    if (cVar4 == 1) {
  lbl_80478D78[7] = 1;
    }
  }
  lbl_8047B610 = lbl_8047B610 + 1;
  return;
}

#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
void wsCmd0E(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    fn_8012640C(fn_801F025C(0x11, 0), 0, 0xD9, 0);
    lbl_8047B610++;
}

#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_8012640C pokemonGetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_802096E8 fightWazaIsHit
#define fn_801F4C14 fightFloorSetStatus
#define fn_802624CC fightMenuOpenMsg
void wsCmd0D(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802096E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_802624CC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8012640C();
#endif
    void* ctx = (void*)fn_801F025C(0x11, 0);
    void* resolved = PC_PTR(fn_8012640C(ctx, 0, 0xD9, 0));

    if ((u8)fn_8011BEB4(resolved, 0, 0x2B, 0) == 2) {
        if ((u8)fn_802096E8(resolved) == 1) {
            fn_801F4C14(0, 0, 0x52, 0, BATTLE_MSG_CRITICAL_HIT);
            if ((u8)fn_802624CC(BATTLE_MSG_CRITICAL_HIT) == 1) {
                lbl_80478D78[7] = 1;
            }
        }
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}

#define fn_801F54A4 fightFloorGetStatus
#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_802656AC fightMenuGetFightOutPokemonPtrToStatusMenuId
#define fn_80102620 menuIsCheck
#define fn_80011C78 menuFightStatusWaitAnimeHP
#define fn_802038A4 fightOutPokemonIsHinsi
#define fn_80207C24 fightOutPokemonSetWazaEffectDownFlag
#define fn_801FEF74 fightOutPokemonGetJoutaiMigawariHp
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
extern u8 wazaSequenceSysIsAllEnd(s32 timeType);
extern void _threadSwitch(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s16 fightOutPokemonWritePokemonJoutaiDataId(void* ctx, void* typeObj, void* inputParam);
#endif
extern u16 fightOutPokemonSetPokemonJoutai08(void* context, void* item, void* target);
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd0C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_802656AC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80102620();
#endif
    extern u32 fn_80011C78();
    extern u8 fn_802038A4();
    extern void fn_80207C24();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_801FEF74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 val;
    u32 ctx;
    u16 count;

    count = (u16)fn_801F54A4(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));
    val = fn_802656AC(ctx, count, 1);

    if ((u8)fn_80102620(val) == 1) {
        fn_80011C78(val, 1);
    }
    if ((u8)fn_802038A4(ctx) == 1) {
        fn_80207C24(ctx, 1);
    }
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x14)) == 1 && fn_801FEF74(PC_PTR(ctx)) <= 0) {
        do {
            if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                break;
            }
            _threadSwitch();
        } while (1);
        fn_80202810(PC_PTR(ctx), PC_PTR(0x14));
        wsRunScript(lbl_8047B62C, (void*)&lbl_80379A22, 0);
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x14), 0);
        fightOutPokemonSetPokemonJoutai08(PC_PTR(ctx), PC_PTR(0x14), 0);
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#else
void wsCmd0C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_802656AC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80102620();
#endif
    extern u32 fn_80011C78();
    extern u8 fn_802038A4();
    extern void fn_80207C24();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_801FEF74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F54A4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 val;
    u32 ctx;
    u16 count;

    count = (u16)fn_801F54A4(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fn_801F025C(*(u8*)(lbl_8047B610 + 1), 0));
    val = fn_802656AC(ctx, count, 1);

    if ((u8)fn_80102620(val) == 1) {
        fn_80011C78(val, 1);
    }
    if ((u8)fn_802038A4(ctx) == 1) {
        fn_80207C24(ctx, 1);
    }
    if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x14)) == 1 && fn_801FEF74(PC_PTR(ctx)) <= 0) {
        do {
            if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                break;
            }
            _threadSwitch();
        } while (1);
        fn_80202810(PC_PTR(ctx), PC_PTR(0x14));
        wsRunScript(lbl_8047B62C, (void*)&lbl_80379A22, 0);
        fightOutPokemonWritePokemonJoutaiDataId((void*)ctx, (void*)0x14, 0);
        fightOutPokemonSetPokemonJoutai08((void*)ctx, (void*)0x14, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level 4; opt_lifetimes off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
#pragma opt_lifetimes off
void wsCmd0B(void)
{
    extern void menuFightStatusStartAnimHP();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuIsCheck(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int zokuseiDataBiosGetCategory();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMainWaitFrame();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    extern void fightOutPokemonAddFightOutPokemonEnemyDamage();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonIsUseHensinBuff();
#endif
    extern void fightOutPokemonSetJoutaiMigawariHp();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetJoutaiMigawariHp();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightWazaIsHit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuCloseMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuOpenMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80265598();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightMenuGetFightOutPokemonPtrToStatusMenuId();
#endif
    extern u32 ServerStatusFlag;
    u32 initialEffectDamage;
    u32 attacker;
    u32 moveId;
    u32 ally;
    u16 category;
    u32 move;
    s32 damage;
    s32 hp;
    u32 target;
    s32 finalHp;
    u32 effectDamage;
    u16 floorId;
    s16 displayHp;
    u32 pokemon;
    s32 maxHp;
    s32 substituteHp;
    u32 relativeRaw;
    s16 previousDamage;
    u8 mode;

    floorId = fightFloorGetStatus(0, 0, 0x14, 0);
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    moveId = fightOutPokemonGetUseWazaDataId((void*)PC_PTR(attacker));
    move = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    damage = wazaGetStatus(PC_PTR(move), 0, 0x2d, 0);
    initialEffectDamage = 0;
    initialEffectDamage += wazaGetStatus(PC_PTR(move), 0, 0x2e, 0);
    category = wazaGetStatus(PC_PTR(move), 0, 0x30, 0);
    ally = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));

    if ((u8)fightWazaIsHit(PC_PTR(move)) == 1) {
        target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(lbl_8047B610[1], 0));
        pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(target));
        hp = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_HP, 0);
        maxHp = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MAX_HP, 0);
        substituteHp = fightOutPokemonGetJoutaiMigawariHp(PC_PTR(target));

        if ((substituteHp != 0) && ((ServerStatusFlag & 0x100) == 0)) {
            if (substituteHp >= damage) {
                pokemon = damage;
                substituteHp -= damage;
            } else {
                pokemon = substituteHp;
                substituteHp = 0;
            }
            if (pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0) == 0) {
                pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0, pokemon);
            }
            wazaSetStatus(PC_PTR(move), 0, 0x2e, 0, pokemon);
            fightOutPokemonSetJoutaiMigawariHp(target, substituteHp);
            fightMenuOpenMsg(0x75b1);
            fightMainWaitFrame(0x40);
            fightMenuCloseMsg();
        } else {
            ServerStatusFlag &= 0xfffffeff;
            if (damage < 0) {
                effectDamage = initialEffectDamage;
                finalHp = hp - damage;
                if (finalHp > maxHp) {
                    finalHp = maxHp;
                }
            } else {
                if (hp > damage) {
                    effectDamage = damage;
                    finalHp = hp - damage;
                } else {
                    effectDamage = hp;
                    finalHp = 0;
                }

                if ((ServerStatusFlag & 0x20) != 0) {
                    ServerStatusFlag &= 0xffffffdf;
                } else {
                    previousDamage = (s16)pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_BIDE_DAMAGE, 0);
                    previousDamage += damage;
                    pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_BIDE_DAMAGE, 0, previousDamage);
                    if (lbl_8047B610[1] == 0x12) {
                        relativeRaw = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(attacker, floorId);
                        pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_BIDE_TARGET, 0,
                                         relativeRaw & 0xffff);
                    } else {
                        relativeRaw = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(ally, floorId);
                        pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_BIDE_TARGET, 0,
                                         relativeRaw & 0xffff);
                    }
                    fightOutPokemonAddFightOutPokemonEnemyDamage(target, attacker,
                                                                  effectDamage & 0xffff);
                }

                if ((pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0) == 0) &&
                    ((ServerStatusFlag & 0x100000) == 0)) {
                    pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0, effectDamage);
                }

                mode = zokuseiDataBiosGetCategory(category);
                switch (mode) {
                case 1:
                    if (((ServerStatusFlag & 0x100000) == 0) &&
                        ((moveId & 0xffff) != 0xdc)) {
                        pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_PHYSICAL_DAMAGE_TAKEN, 0, effectDamage);
                        pokemonSetStatus((void*)PC_PTR(target), 0, 0x11c, 0, effectDamage);
                        if (lbl_8047B610[1] == 0x12) {
                            relativeRaw = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(attacker, floorId);
                            pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_PHYSICAL_DAMAGE_FROM, 0,
                                             damage = relativeRaw & 0xffff);
                            pokemonSetStatus((void*)PC_PTR(target), 0, 0x11d, 0, damage);
                        } else {
                            relativeRaw = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(ally, floorId);
                            pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_PHYSICAL_DAMAGE_FROM, 0,
                                             damage = relativeRaw & 0xffff);
                            pokemonSetStatus((void*)PC_PTR(target), 0, 0x11d, 0, damage);
                        }
                    }
                    break;
                case 2:
                    if ((ServerStatusFlag & 0x100000) == 0) {
                        pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_SPECIAL_DAMAGE_TAKEN, 0, effectDamage);
                        pokemonSetStatus((void*)PC_PTR(target), 0, 0x11e, 0, effectDamage);
                        if (lbl_8047B610[1] == 0x12) {
                            relativeRaw = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(attacker, floorId);
                            pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_SPECIAL_DAMAGE_FROM, 0,
                                             damage = relativeRaw & 0xffff);
                            pokemonSetStatus((void*)PC_PTR(target), 0, 0x11f, 0, damage);
                        } else {
                            relativeRaw = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(ally, floorId);
                            pokemonSetStatus((void*)PC_PTR(target), 0, POKEMON_STATUS_SPECIAL_DAMAGE_FROM, 0,
                                             damage = relativeRaw & 0xffff);
                            pokemonSetStatus((void*)PC_PTR(target), 0, 0x11f, 0, damage);
                        }
                    }
                    break;
                }
            }

            ServerStatusFlag &= 0xffefffff;
            wazaSetStatus(PC_PTR(move), 0, 0x2e, 0, effectDamage);
            pokemonSetStatus((void*)PC_PTR(pokemon), 0, POKEMON_STATUS_HP, 0, finalHp);
            if ((u8)fightOutPokemonIsUseHensinBuff(PC_PTR(target)) == 1) {
                fightOutPokemonSetHensinPokemonStatusId(PC_PTR(target), 0x83, 0, 0);
            }
            maxHp = fightMenuGetFightOutPokemonPtrToStatusMenuId(target, floorId, 1);
            displayHp = finalHp;
            if ((u8)menuIsCheck(maxHp) == 0) {
                fn_80265598(target, floorId, 1);
            }
            menuFightStatusStartAnimHP(maxHp, displayHp);
        }
    } else {
        pokemon = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(lbl_8047B610[1], 0));
        if (pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0) == 0) {
            pokemonSetStatus((void*)PC_PTR(pokemon), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0, 0xffff);
        }
    }
    lbl_8047B610 += 2;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragmas kept for this function; the unit builds with unit-level flags — see docs/RULE_EXCEPTIONS.md */
#pragma opt_lifetimes reset
#pragma optimization_level 2
void wsCmd0A(void) {
    extern u32 ServerStatusFlag;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceSysIsAllEnd();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuAllFightOutPokemonCloseStatusMenu();
#endif
    struct {
        u32 index;
        u32 attacker;
        u32 move;
    } vars;

    vars.attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    vars.move = fightOutPokemonGetUseWazaDataId(PC_PTR(vars.attacker));
    pokemonGetStatus(PC_PTR(vars.attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    fightTargetGetPtrAsNowFightType(0x12, 0);
    if ((ServerStatusFlag & 0x80) == 0 || (u16)vars.move == 0x90 ||
        (u16)vars.move == 0xa4) {
        vars.index = lbl_8047B610[1];
        do {
            if (wazaSequenceSysIsAllEnd(vars.index) == 1) {
                break;
            }
            _threadSwitch();
        } while (1);
    }
    if ((u8)vars.index == 2 || (u8)vars.index == 6) {
        fightMenuAllFightOutPokemonCloseStatusMenu(0);
    }
    lbl_8047B610 += 2;
    return;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragmas kept for this function; the unit builds with unit-level flags — see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level reset
void wsCmd09(void)

{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysStartWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceOwnerLoadWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    extern void fightWazaDoEffectFunc();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuAllFightOutPokemonCloseStatusMenu();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80265598();
#endif
    extern u32 ServerStatusFlag;
    extern void* lbl_8047B62C;
    extern u8 lbl_80378964[];
    u32 move;
    u32 attacker;
    u32 status;
    u32 target;
    u32 effectAttacker;
    u16 floorMove;

    fightFloorGetStatus(0, 0, 0x14, 0);
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
    status = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));

#ifdef TARGET_PC
    { /* PC, observe only (ORRE_WS_TRACE): which branch wsCmd09 takes (sitting 42's crash after it, 2026-10-27) */
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        if (getenv("ORRE_WS_TRACE") != NULL) {
            OSReport("[ws] cmd09: move %u hit %d flags 0x%X\n", (unsigned)(u16)move,
                     (int)fightWazaIsHit(PC_PTR(status)), (unsigned)ServerStatusFlag);
        }
    }
#endif
    if (fightWazaIsHit(PC_PTR(status)) == 1) {
        if ((ServerStatusFlag & 0x80) != 0 && (u16)move != 0x90 &&
            (u16)move != 0xa4) {
            wsRunScript(lbl_8047B62C, lbl_80378964, 0);
        } else {
            fightWazaDoEffectFunc(move, attacker, target,
                                  lbl_80379F58[0x160a1]);
        }
        lbl_80379F58[0x16002]++;
        lbl_8047B610++;
        lbl_80379F58[0x160a1]++;
    } else {
        fightMenuAllFightOutPokemonCloseStatusMenu(0);
        floorMove = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
        effectAttacker = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
        if (effectAttacker != 0 && wazaSequenceOwnerLoadWaza(PC_PTR(effectAttacker), 0x57, 4, 0) != 0) {
            wazaSequenceSysStartWaza(PC_PTR(effectAttacker), 0x57, 4);
            fn_80265598(target, floorMove, 1);
        }
        lbl_8047B610++;
    }
}

void WS_DAMAGE_LOSS_ONLY(void) { fightApplyDamageVariance(0, 1); lbl_8047B610 = lbl_8047B610 + 1; }

#ifdef TARGET_PC
static void orre_move_damage_hook(void); /* below fightApplyDamageVariance */
/* PC: the final damage passes OrreHook_MoveDamage (instant win); nothing registered = the console's damage */
void WS_DAMAGE_LOSS(void) { fightApplyDamageVariance(1, 1); orre_move_damage_hook(); lbl_8047B610 = lbl_8047B610 + 1; }
#else
void WS_DAMAGE_LOSS(void) { fightApplyDamageVariance(1, 1); lbl_8047B610 = lbl_8047B610 + 1; }
#endif

#define fn_801F025C fightTargetGetPtrAsNowFightType
#define fn_80205184 fightOutPokemonGetUseWazaDataId
#define fn_8012640C pokemonGetStatus
#define fn_8011BBD8 wazaSetStatus
#define fn_8011BEB4 wazaGetStatus
#define fn_802040E8 fightOutPokemonGetSoubiItemDataId
#define fn_80203FE4 fightOutPokemonGetSoubiItemSoubiDataId
#define fn_80203EDC figthOutPokemonGetSoubiItemBuff
#define fn_80205B8C fightOutPokemonGetPokemonPtr
#define fn_801254B4 pokemonSetStatus
#define fn_8020981C fightWazaCheckWriteJoutaiDataId
#define fn_802097C8 fightWazaWriteJoutaiDataId
#define fn_801F4C14 fightFloorSetStatus
void fightApplyDamageVariance(u8 r3, u8 r4)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8011BBD8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_8011BEB4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F025C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801F4C14();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fn_80203EDC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80203FE4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_802040E8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205B8C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_802097C8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8020981C();
#endif
  u32 uVar4;
  u32 uVar2;
  u32 uVar7;
  u32 uVar1;
  int iVar3;
  u16 uVar9;
  u16 sVar10;
  int iVar5;
  u32 uVar6;
  u32 uVar0;
  u16 uVar8;
  u8 cVar11;

  uVar0 = PC_PTR32_IMPLICIT(fn_801F025C(0x11,0));
  uVar2 = fn_80205184((void*)PC_PTR(uVar0));
  uVar1 = (int)fn_8012640C(PC_PTR(uVar0),0,0xd9,0);
  iVar3 = fn_8011BEB4(PC_PTR(uVar1),0,0x2d,0);
  uVar4 = PC_PTR32_IMPLICIT(fn_801F025C(0x12,0));
  uVar9 = fn_802040E8(PC_PTR(uVar4));
  sVar10 = fn_80203FE4(PC_PTR(uVar4));
  iVar5 = fn_80203EDC(PC_PTR(uVar4));
  uVar6 = PC_PTR32_IMPLICIT(fn_80205B8C(uVar4));
  uVar7 = (int)fn_8012640C(PC_PTR(uVar6),0,0x83,0) & 0xffff;
  if (r4 == 1) {
    uVar8 = GSrand();
    uVar8 = (100 - uVar8 % 16) & 0xffff;
    if (iVar3 != 0) {
      iVar3 *= uVar8;
      iVar3 /= 100;
      if (iVar3 == 0) {
        iVar3 = 1;
      }
      fn_8011BBD8(PC_PTR(uVar1),0,0x2d,0,iVar3);
    }
  }
  if ((sVar10 == 0x27) && (uVar8 = GSrand(), (int)(uVar8 % 100) < iVar5)) {
    fn_801254B4((void*)PC_PTR(uVar4),0,0x11a,0,1);
  }
  cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(0x14));
  if (cVar11 == 0) {
    sVar10 = fn_8011BEB4(0,uVar2,9,0);
    if ((((sVar10 != 0x65) || (r3 != 1)) &&
        (cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(CONDITION_ENDURE)), cVar11 == 0)) &&
       (iVar5 = (int)fn_8012640C(PC_PTR(uVar4),0,0x11a,0), iVar5 == 0)) {
      return;
    }
    uVar2 = PC_PTR32_IMPLICIT(fn_80205B8C(uVar4));
    iVar5 = (int)fn_8012640C(PC_PTR(uVar2),0,0x83,0);
    if (iVar5 <= iVar3) {
      fn_8011BBD8(PC_PTR(uVar1),0,0x2d,0,(uVar7 & 0xffff) - 1);
      cVar11 = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(uVar4),PC_PTR(CONDITION_ENDURE));
      if (cVar11 != 0) {
        cVar11 = fn_8020981C(PC_PTR(uVar1),0x46);
        if (cVar11 == 2) {
          fn_802097C8(uVar1,0x46,0);
        }
        return;
      }
      iVar3 = (int)fn_8012640C(PC_PTR(uVar4),0,0x11a,0);
      if (iVar3 != 0) {
        cVar11 = fn_8020981C(PC_PTR(uVar1),0x47);
        if (cVar11 == 2) {
          fn_802097C8(uVar1,0x47,0);
        }
        fn_801F4C14(0,0,0x56,0,uVar9);
      }
    }
    return;
  }
  return;
}

#ifdef TARGET_PC
#include "pc/orre_hooks.h"
/* OrreHook_MoveDamage (docs/port/hook-points.md): the damage fightApplyDamageVariance just stored, read and written
 * where it does (the acting Pokémon's move work, status 0x2d; the target's HP, status 0x83), so the health bar and
 * the HP change that follow use the hook's number. Reads only; no random draws, so with nothing registered the
 * battle is unchanged. Side 0 is the player's side in story battles (likely: the host side, battle-targets.md). */
static void orre_move_damage_hook(void)
{
  u32 attacker, target, work, pokemon;
  void *side0, *attackerSide, *targetSide;
  s32 damage, hp, next;

  if (OrreHook_MoveDamage == NULL) {
    return;
  }
  attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
  target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
  if (attacker == 0 || target == 0) {
    return;
  }
  work = (u32)pokemonGetStatus(PC_PTR(attacker), 0, 0xd9, 0);
  damage = (s32)wazaGetStatus(PC_PTR(work), 0, 0x2d, 0);
  pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(PC_PTR(target)));
  hp = (s32)(pokemonGetStatus(PC_PTR(pokemon), 0, 0x83, 0) & 0xffff);
  side0 = fightTargetGetPtrAsNowFightType(4, 0);
  attackerSide = fightTargetGetPtrAsNowFightType(2, attacker);
  targetSide = fightTargetGetPtrAsNowFightType(2, target);
  next = OrreHook_MoveDamage(attackerSide == side0, targetSide == side0, damage, hp);
  if (next != damage) {
    wazaSetStatus(PC_PTR(work), 0, 0x2d, 0, (u32)next);
  }
}
#endif

void wsCmd4A(void) { fightSeqWazaTypeCalc(0, 1, 1, 0); }

void WS_TYPE_CHECK(void) { fightSeqWazaTypeCalc(1, 1, 1, 0); }
