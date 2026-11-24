#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"
#include "game/constants/moves.h"
#include "pc/disc_types.h" /* the move-effect script table lbl_80379BFF is big-endian disc data on the PC */

extern u8* lbl_8047B610;

static inline u32 fn_80219E10_checkMove(u16 move)
{
    extern u16 YubihuriNGTbl[];
    u16 entry;
    s32 index;

    if (move == MOVE_NONE) {
        return 0;
    }
    if (move == MOVE_SHADOW_LOCKED) {
        return 0;
    }
    if (move == MOVE_UNUSED_355) {
        return 0;
    }
    index = 0;
    while (1) {
        entry = YubihuriNGTbl[index];
        if (entry == 0xffff) {
            break;
        }
        if (move == entry) {
            break;
        }
        index++;
    }
    if (entry != 0xffff) {
        return 0;
    }
    return 1;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd9E(void)
{
    extern u32 dbgMenuFightGetYubiwohuruWazaDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
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
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightWazaSetUseWazaStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSeqSelectTarget();
#endif
    extern u32 ServerStatusFlag;
    extern u16 YubihuriNGTbl[];
    extern u32 lbl_80379BFF[];
    u16 floorId;
    u32 attacker;
    u32 status;
    u32 count;
    u32 offset;
    u32 move;
    u32 allowed;
    u32 random;
    u32 selected;
    u32 result;
    u32 i;
    u16 local_338[404];

    floorId = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    status = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);

    for (i = 0; i < 400; i++) {
        local_338[i] = 0;
    }

    count = 0;
    offset = 0;
    for (move = 0; move < 0x163; move++) {
        if (move != 0) {
            allowed = fn_80219E10_checkMove((u16)move);
            if ((u8)allowed != 0) {
                count++;
                local_338[offset] = (u16)move;
                offset++;
            }
        }
    }

    random = (u16)GSrand();
    selected = local_338[random % count];
    result = dbgMenuFightGetYubiwohuruWazaDataId();
    if ((u16)result != 0) {
        selected = result;
    }

    ServerStatusFlag &= 0xfffffbff;
    fightWazaSetUseWazaStatus(status, selected);
    {
        u32 callResult =
            fightSeqSelectTarget(attacker, selected, floorId, 0, 1, 1, PC_PTR32_IMPLICIT((void*)0xffffffff));
        result = callResult;
    }
    fightFloorSetStatus(0, 0, 0x43, 0, result);
    result = wazaGetStatus(0, selected, 9, 0);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(&lbl_80379BFF[(u16)result]));
}
#pragma optimize_for_size reset
