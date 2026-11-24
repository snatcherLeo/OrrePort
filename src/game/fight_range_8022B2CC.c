/**
 * @file fight_range_8022B2CC.c
 * @brief Fight target selection, 0x8022B2CC - 0x8022BB84 (fightSeqSelectTarget,
 *        _fightOutPokemonSoubiItemEffectSub), with .data 0x8039A220 - 0x8039A388.
 *
 * The .data part is fightSeqSetTsuikaKouka's switch table (written out as data, see
 * below) followed by this unit's two switch tables. _fightOutPokemonSoubiItemEffectSub's table
 * starts at 0x8039A314 and fightSeqSelectTarget's at 0x8039A2F4, both 4 mod 8, and
 * MWCC aligns each .data section to 8, so the carve has to start its .data
 * at 0x8039A220.
 *
 * fightSeqSelectTarget's Follow Me redirect is the same inline as the one in
 * fight_range_candidate_80219838.c; its locals, numbered in reverse
 * declaration order, give retail's side/floor registers.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/conditions.h"
#include "game/constants/moves.h"
#include "game/constants/types.h"
#include "game/constants/pokemon_status.h"

extern u8 lbl_80478D78[8];
extern void* lbl_8047B62C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void*, void*, u8);
#endif
extern void fightSeqSetTsuikaKouka();

/* RULE-EXCEPTION(user-approved): another function's switch table written as a
 * data initializer so the carve can own 8-aligned .data -- see
 * docs/RULE_EXCEPTIONS.md */
#if !defined(FIGHT_RANGE_802249B8_COMBINED)
void* jumptable_8039A220[53] = {
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xBC8),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xC50),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xE1C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xD04),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xD88),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xE70),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xEB4),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0xF70),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1020),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1094),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x11F0),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1234),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x126C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1450),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1494),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x14CC),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x14F0),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1514),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1584),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x15A8),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1108),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x117C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1610),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1668),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x175C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x175C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x175C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x175C),
    (void*)((u8*)fightSeqSetTsuikaKouka + 0x1738),
};
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetValidFightOutPokemonCount();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonIsZokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetFightOutPokemonPtrRandom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightSideIsJoutaiDataId();
#endif
extern u32 fightSideGetJoutaiUserFightTargetId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetFightOutPokemonPtrAryPokemonTokuseiDataIdFirst();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus();
#endif

/* The live Follow Me user (side joutai 0x4d) on the side that
 * fightTargetGetPtrAsNowFightType(3, attacker) returns, or 0. */
static inline u32 fightGetFollowMeTarget(u32 attacker)
{
    u32 target;
    u32 userId;
    u32 side;
    u32 redirected;
    u16 floor;

    side = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(3, attacker));
    floor = fightFloorGetStatus(0, 0, 0x14, 0);
    redirected = 0;
    if (fightSideIsJoutaiDataId(side, 0x4d) == 1) {
        userId = fightSideGetJoutaiUserFightTargetId(side, 0x4d);
        if ((userId & 0xffff) != 0) {
            target = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(userId, floor);
            if (target != 0 && fightOutPokemonCheckFightOut(PC_PTR(target)) == 1) {
                redirected = target;
            }
        }
    }
    return redirected;
}

u32 fightSeqSelectTarget(u32 attacker, u32 move, u32 r5, u32 selector, u32 r7, u32 r8, s8 in_r9)
{
    u32 result;
    u32 count;
    u32 abilityCount;
    u32 redirected;
    u32 targetAbility;
    u16 moveType;
    u8 category;

    result = 0;
    if (in_r9 < 0) {
        category = wazaGetStatus(0, move, 5, 0);
    } else {
        category = in_r9;
    }
    if ((u16)move == MOVE_CURSE && fightOutPokemonIsZokuseiDataId(PC_PTR(attacker), TYPE_GHOST) == 0) {
        category = 5;
    }
    moveType = wazaGetStatus(0, move, 3, 0);
    count = fightFloorGetValidFightOutPokemonCount(0, 0, attacker, 1);
    abilityCount = fightFloorCheckFightOutPokemonPtrAryPokemonTokuseiDataId(0, ABILITY_LIGHTNINGROD, 2, attacker);

    switch (category) {
    case 0:
        if ((u16)count >= 2) {
            if ((u8)r7 == 1) {
                if (selector != 0) {
                    result = ((u32 (*)(RegWord, RegWord, RegWord))PC_PTR(selector))(attacker, move, r5);
                } else {
                    result = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrRandom(0, 1, 2, attacker));
                }
            } else {
                result = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
            }
            if ((u8)r8 == 1) {
                targetAbility = fightOutPokemonGetTokuseiDataId(PC_PTR(result));
                redirected = fightGetFollowMeTarget(attacker);
                if (redirected != 0) {
                    result = redirected;
                } else if ((u16)targetAbility != ABILITY_LIGHTNINGROD && moveType == TYPE_ELECTRIC && (u16)abilityCount != 0) {
                    result = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrAryPokemonTokuseiDataIdFirst(0, ABILITY_LIGHTNINGROD,PC_PTR(1), 2, attacker));
                    pokemonSetStatus(PC_PTR(result), 0, POKEMON_STATUS_LIGHTNINGROD_FLAG, 0, 1);
                }
            }
        } else if ((u8)r7 == 1) {
            result = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrRandom(0, 1, 3, attacker));
        }
        break;
    case 1:
    case 4:
    case 6:
    case 7:
        if ((u8)r7 == 1) {
            result = fightTargetGetPtr(0xf, attacker, r5);
            if (fightOutPokemonCheckFightOut(PC_PTR(result)) == 0) {
                result = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0xe, result));
            }
        }
        break;
    case 3:
        if ((u8)r7 == 1) {
            result = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrRandom(0, 1, 2, attacker));
        }
        break;
    case 2:
    case 5:
        if ((u8)r7 == 1) {
            result = attacker;
        }
        break;
    }
    return result;
}

u32 _fightOutPokemonSoubiItemEffectSub(void* rawCtx)
{
    extern u8 lbl_80379A3C[];
    extern u8 lbl_80379A5A[];
    extern u8 lbl_80379A78[];
    extern u8 lbl_80379A96[];
    extern u8 lbl_80379AB4[];
    extern u8 lbl_80379AD2[];
    extern u8 lbl_80379AEE[];
    extern u8 lbl_80379B0C[];
    extern u16 lbl_80279FD0[8];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetSoubiItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetSoubiItemSoubiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void figthOutPokemonGetSoubiItemBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetName();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsJoutaiNormal();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitJoutai();
#endif
    extern void fightOutPokemonResetSeqStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
    u8 result = 0;
    u32 pokemon;
    u8* next;
    u16 itemType;
    u8* msg;
    u16 i;
    u32 ctx;

    ctx = PC_PTR32(rawCtx);
    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(ctx));
    next = PC_PTR((u8*)PC_PTR((u32)fightOutPokemonGetSoubiItemDataId(ctx)));
    itemType = fightOutPokemonGetSoubiItemSoubiDataId(PC_PTR(ctx));
    figthOutPokemonGetSoubiItemBuff(PC_PTR(ctx));
    msg = 0;

    if (fightOutPokemonCheckFightOut(PC_PTR(ctx)) == 0) {
        return 1;
    }

    fightFloorSetStatus(0, 0, 0x56, 0, (u16)PC_PTR32(next));

    switch ((u16)itemType) {
    case 2: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_PARALYSIS)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_PARALYSIS));
            next = lbl_80379A3C;
        }
        msg = next;
        if (next != 0) {
            result = 1;
        }
        break;
    }
    case 4: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_POISON)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_POISON));
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BAD_POISON));
            next = lbl_80379A5A;
        }
        msg = next;
        if (next != 0) {
            result = 1;
            break;
        }

        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BAD_POISON)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BAD_POISON));
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_POISON));
            next = lbl_80379A5A;
        }
        msg = next;
        if (next != 0) {
            result = 1;
        }
        break;
    }
    case 5: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BURN)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BURN));
            next = lbl_80379A78;
        }
        msg = next;
        if (next != 0) {
            result = 1;
        }
        break;
    }
    case 6: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_FREEZE)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_FREEZE));
            next = lbl_80379A96;
        }
        msg = next;
        if (next != 0) {
            result = 1;
        }
        break;
    }
    case 3: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_SLEEP)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_SLEEP));
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(0x17));
            next = lbl_80379AB4;
        }
        msg = next;
        if (next != 0) {
            result = 1;
        }
        break;
    }
    case 8: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_CONFUSION)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_CONFUSION));
            next = lbl_80379AD2;
        }
        msg = next;
        if (next != 0) {
            result = 2;
        }
        break;
    }
    case 0x1C: {
        next = 0;
        if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0xA)) == 1) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(0xA));
            next = lbl_80379AEE;
        }
        msg = next;
        if (next != 0) {
            msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(0xA))));
            lbl_80478D78[5] = 0;
            result = 2;
        }
        break;
    }
    case 9:
        if (fightOutPokemonIsJoutaiNormal(PC_PTR(ctx)) == 0 ||
            fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_CONFUSION)) == 1) {
            lbl_80478D78[5] = 0;
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_POISON)) == 1 ||
                fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BAD_POISON)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(3))));
            }
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_SLEEP)) == 1) {
                fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(0x17));
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(8))));
            }
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_PARALYSIS)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(5))));
            }
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_BURN)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(6))));
            }
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_FREEZE)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(7))));
            }
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_CONFUSION)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(9))));
            }
            pokemonInitJoutai(PC_PTR(pokemon));
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(CONDITION_CONFUSION));
            fightOutPokemonResetSeqStatus(ctx, 0);
            result = 1;
            msg = lbl_80379AEE;
        }
        break;
    case 0x17:
        for (i = 0; i < 7; i++) {
            if (pokemonGetStatus(PC_PTR(ctx), 0, lbl_80279FD0[i], 0) < 6) {
                pokemonSetStatus(PC_PTR(ctx), 0, lbl_80279FD0[i], 0, 6);
                result = 5;
            }
        }
        if (result != 0) {
            msg = lbl_80379B0C;
        }
        break;
    }

    if (result != 0) {
        fightFloorSetStatus(0, 0, 0x4B, 0, ctx);
        fightFloorSetStatus(0, 0, 0x49, 0, ctx);
        if (result == 1 && fightOutPokemonIsUseHensinBuff(PC_PTR(ctx)) == 1) {
            fightOutPokemonSetHensinPokemonStatusId(PC_PTR(ctx), 0x7C, 0, 0);
        }
        if (msg != 0) {
            wsRunScript(lbl_8047B62C, msg, 0);
        }
    }
    return 1;
}
