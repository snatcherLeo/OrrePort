/**
 * @file msgctrl_candidate_80131BF8.c
 * @brief msgctrl.c carve, 0x80131BF8 - 0x80131F9C: the six side-name
 *        control codes, _msgctrlSideName and msgctrlClientnowork.
 *
 * Built with the TU's -O4,p and the peephole pass off (one unit-wide flag in
 * configure.py, as for the other msgctrl.c carves).
 */
#include "dolphin/types.h"

extern u32 _CLIENTNOWORK;
extern u32 _SIDE_ATTACK_NAMEHA;
extern u32 _SIDE_ATTACK_NAMEWO;
extern u32 _SIDE_ATTACK_NAMENO;
extern u32 _SIDE_DEFENCE_NAMEHA;
extern u32 _SIDE_DEFENCE_NAMEWO;
extern u32 _SIDE_DEFENCE_NAMENO;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(u32 id, u32 value);
#endif
extern u8 fightTargetIsHostSide(u32 pokemon, u16 status);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTargetGetPtrAsNowFightType(u32 target, u32 pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_801F18DC(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32 arg, u32 pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus(u32 a, u32 b, u32 id, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightSideGetValidFightTrainerPtr(void* side, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetNamePtr(void* trainer);
#endif
extern u8 fightTrainerCheckDoFight(void* trainer);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetNicknamePtr(u32 pokemon);
#endif

void* _msgctrlSideName__FP15FightOutPokemonUc(u32 pokemon, u8 kind);

u32 msgctrlSideDefenceNameno(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMENO, 2));
}

u32 msgctrlSideDefenceNamewo(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMEWO, 1));
}

u32 msgctrlSideDefenceNameha(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMEHA, 0));
}

u32 msgctrlSideAttackNameno(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMENO, 2));
}

u32 msgctrlSideAttackNamewo(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMEWO, 1));
}

u32 msgctrlSideAttackNameha(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMEHA, 0));
}

/*
 * Put the side's trainer name(s) into message values 0x4D/0x57, then fetch
 * the side-name string for this message kind (0, 1, other).
 */
void* _msgctrlSideName__FP15FightOutPokemonUc(u32 pokemon, u8 kind)
{
    u16 status;
    void* side;
    void* ownTrainer;
    void* trainer;
    u16 count;
    u16 i;

    status = fightFloorGetStatus(0, 0, 0x14, 0);
    side = fightTargetGetPtrAsNowFightType(2, pokemon);
    ownTrainer = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, PC_PTR(pokemon));
    count = 0;
    for (i = 0; i < 2; i++) {
        trainer = PC_PTR(fightSideGetValidFightTrainerPtr(PC_PTR32_IMPLICIT(side), i));
        if (trainer != NULL && fightTrainerCheckDoFight(trainer) != 0) {
            if (count == 0) {
                msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
            } else if (count == 1) {
                msgctrlSetValue(0x57, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
            }
            count++;
        }
    }

    if (fn_801F18DC(0) == 1 && ownTrainer != NULL) {
        if (count <= 1) {
            msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(ownTrainer)));
            if (kind == 0) {
                return GSmsgGetGSchar(0x7722);
            } else if (kind == 1) {
                return GSmsgGetGSchar(0x7725);
            } else {
                return GSmsgGetGSchar(0x7727);
            }
        }
        if (kind == 0) {
            return GSmsgGetGSchar(0x7724);
        } else if (kind == 1) {
            return GSmsgGetGSchar(0x7726);
        } else {
            return GSmsgGetGSchar(0x7728);
        }
    }

    if (fightTargetIsHostSide(pokemon, status) == 1) {
        if (kind == 0) {
            return GSmsgGetGSchar(0x768A);
        } else if (kind == 1) {
            return GSmsgGetGSchar(0x768C);
        } else {
            return GSmsgGetGSchar(0x7688);
        }
    }
    if (kind == 0) {
        return GSmsgGetGSchar(0x7689);
    } else if (kind == 1) {
        return GSmsgGetGSchar(0x768B);
    } else {
        return GSmsgGetGSchar(0x7687);
    }
}

/* The trainer-or-nickname body msgctrl_candidate_80131FF4.c's *Mons getters
 * share, for the client Pokemon. */
static inline void* msgctrlOutPokemonName(u32 pokemon)
{
    void* trainer = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, PC_PTR(pokemon));

    if (fn_801F18DC(0) == 1 && trainer != NULL) {
        msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(fightTrainerGetNamePtr(trainer)));
        msgctrlSetValue(0x57, PC_PTR32_IMPLICIT(fightOutPokemonGetNicknamePtr(pokemon)));
        return GSmsgGetGSchar(0x7721);
    }
    return PC_PTR((void*)PC_PTR(fightOutPokemonGetNicknamePtr(pokemon)));
}

void* msgctrlClientnowork(void)
{
    return msgctrlOutPokemonName(_CLIENTNOWORK);
}
