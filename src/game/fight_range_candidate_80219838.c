/**
 * @file fight_range_candidate_80219838.c
 * @brief Fight sequence commands, 0x80219838 - 0x80219D98 (wsCmdA3,
 *        wsCmdA2, wsCmdA1, wsCmdA0).
 *
 * wsCmdA2 and wsCmdA1 are one body with different pokemon status
 * ids, so they share an inline. As an inline its locals are numbered in
 * reverse declaration order, which gives retail's register order (floor,
 * power, target id, move, attacker) and lets the move result coalesce
 * without an extra copy. The Follow Me redirect is the same inline as in
 * fightSeqSelectTarget (fight_range_8022B2CC.c).
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus(u32, u16, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int fightTargetGetRelativeHostSideFightTargetIdToTragetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightSideIsJoutaiDataId();
#endif
extern u32 fightSideGetJoutaiUserFightTargetId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetPokemonPtr();
#endif
extern u8 fightOutPokemonIsAlly();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 figthOutPokemonGetLevel();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonSetPokemonJoutai0C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_802025B8();
#endif

void wsCmdA3(void)
{
    u32 target;
    u32 pokemon;
    u16 lastMove;
    u16 move;
    s8 slot;
    u8 pp;

    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(target));
    lastMove = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0);
    slot = pokemonSearchWazaDataId(PC_PTR(pokemon), lastMove);
    if (slot < 0) {
        pp = 0;
    } else {
        pp = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slot);
    }
    if (fn_802025B8(PC_PTR(target), PC_PTR(0x29)) == 2 && slot >= 0 && pp != 0) {
        move = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE, slot);

        msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, move, 1, 0)))); /* r3 = the move's name message id (assembly) */
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(target), PC_PTR(0x29), 0);
        fightOutPokemonSetPokemonJoutai0C(PC_PTR(target), PC_PTR(0x29), move);
        lbl_8047B610 += 5;
        return;
    }
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
}

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

/*
 * Checks the target stored in the attacker's status (targetStatus) and, if
 * it is still in battle and not an ally, sets the move's power from
 * powerStatus and aims the move there, unless a Follow Me user redirects it.
 */
static inline void fightSeqAimStoredTarget(u32 targetStatus, u32 powerStatus)
{
    int target;
    u32 followMe;
    u32 attacker;
    u32 move;
    u16 targetId;
    s16 power;
    u16 floor;

    floor = fightFloorGetStatus(0, 0, 0x14, 0);
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    targetId = pokemonGetStatus(PC_PTR(attacker), 0, targetStatus, 0);
    power = pokemonGetStatus(PC_PTR(attacker), 0, powerStatus, 0);
    if (targetId == 0 || power == 0) {
        goto fail;
    }
    target = fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(targetId, floor);
    if (target == 0) {
        goto fail;
    }
    if (fightOutPokemonIsAlly(attacker, target) != 0) {
        goto fail;
    }
    if (fightOutPokemonCheckFightOut(PC_PTR(target)) == 1) {
        wazaSetStatus(PC_PTR(move), 0, 0x2d, 0, power << 1);
        followMe = fightGetFollowMeTarget(attacker);
        if (followMe != 0) {
            target = followMe;
        }
        fightFloorSetStatus(0, 0, 0x43, 0, target);
        lbl_8047B610 += 5;
        return;
    }
fail:
    pokemonSetStatus(PC_PTR(attacker), 0, 0x118, 0, 1);
    lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
}

void wsCmdA2(void)
{
    fightSeqAimStoredTarget(0x105, 0x104);
}

void wsCmdA1(void)
{
    fightSeqAimStoredTarget(0x103, 0x102);
}

/* Sets the move's power to level * (50 + 10 * (rand % 11)) / 100. */
void wsCmdA0(void)
{
    u32 attacker;
    u32 move;
    u32 level;
    u16 rng;

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    move = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    level = figthOutPokemonGetLevel(PC_PTR(attacker));
    rng = GSrand();
    wazaSetStatus(PC_PTR(move), 0, 0x2d, 0, (s32)(level * ((rng % 11) * 10 + 50)) / 100);
    lbl_8047B610++;
}
