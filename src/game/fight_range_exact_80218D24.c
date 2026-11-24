#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/move_effects.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;

void wsCmdA9(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaCheckValid();
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
    extern u8 fightOutPokemonCheckCanOutOkWazaBanme();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSeqSelectTarget();
#endif
    extern u16 lbl_8047B60C;
    extern u32 ServerStatusFlag;
    u8 bVar1;
    u16 uVar6;
    u32 uVar2;
    u32 bVar9;
    s32 sVar10;
    u32 uVar4;
    u32 uVar3;
    u8 cVar8;
    u32 uVar5;
    u16 sVar7;
    u32 initIndex;
    u32 floorTarget;
    short local_28[4];

    uVar6 = fightFloorGetStatus(0, 0, 0x14, 0);
    uVar2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    uVar3 = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(uVar2));
    {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
        extern u32 pokemonGetStatus();
#endif
        uVar4 = pokemonGetStatus(PC_PTR(uVar2), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    }
    for (initIndex = 0; ((initIndex) & 0xff) < 4; initIndex = initIndex + 1) {
        local_28[(u8)initIndex] = -1;
    }
    bVar9 = 0;
    for (sVar10 = 0; (s16)sVar10 < 4; sVar10 = sVar10 + 1) {
        cVar8 = pokemonWazaCheckValid(PC_PTR(uVar3), (u16)sVar10);
        if (cVar8 != 0) {
            cVar8 = fightOutPokemonCheckCanOutOkWazaBanme(PC_PTR(uVar2), (u16)sVar10, 0, 0);
            if ((cVar8 == 0) || (cVar8 == 6)) {
                {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
                    extern u32 pokemonGetStatus();
#endif
                    sVar7 = (u16)pokemonGetStatus(PC_PTR(uVar3), 0, POKEMON_STATUS_MOVE, (u16)sVar10);
                }
                if ((sVar7 != 0) && ((sVar7 != 0x165 && (sVar7 != 0x163)))) {
                    if ((sVar7 == 0) ||
                        ((((sVar7 == 0x165 || (sVar7 == 0xd6)) ||
                           (sVar7 == 0x112)) ||
                          ((sVar7 == 0x77 || (sVar7 == 0x76)))))) {
                        bVar1 = 1;
                    } else {
                        bVar1 = 0;
                    }
                    if (((!bVar1) && (sVar7 != 0x108)) && (sVar7 != 0xfd)) {
                        sVar7 = (u16)wazaGetStatus(0, sVar7, 9, 0);
                        if (((((sVar7 == MOVE_EFFECT_SKULL_BASH) || (sVar7 == MOVE_EFFECT_RAZOR_WIND)) ||
                               (sVar7 == MOVE_EFFECT_SKY_ATTACK)) ||
                              ((sVar7 == MOVE_EFFECT_SOLARBEAM || (sVar7 == MOVE_EFFECT_SEMI_INVULNERABLE)))) ||
                             (sVar7 == MOVE_EFFECT_BIDE)) {
                            bVar1 = 1;
                        } else {
                            bVar1 = 0;
                        }
                        if (bVar1 == 0) {
                            local_28[(u8)bVar9] = (s16)sVar10;
                            bVar9 = bVar9 + 1;
                        }
                    }
                }
            }
        }
    }
    if ((u8)bVar9 != 0) {
        goto choose_move;
    }
failed:
    lbl_8047B610 = lbl_8047B610 + 5;
    goto done;

choose_move:
    uVar5 = GSrand();
    sVar10 = *(s16*)PC_PTR(((int)PC_PTR32(local_28) +
                     (((uVar5 & 0xffff) -
                       ((s32)(uVar5 & 0xffff) / (s32)(u8)bVar9) * (u8)bVar9) *
                              2 &
                      0x1fe)));
    if ((s16)sVar10 < 0) {
        goto failed;
    }
    {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
        extern u32 pokemonGetStatus(u32, u32, u32, u16);
#endif
        sVar7 = (u16)pokemonGetStatus(PC_PTR(uVar3), 0, POKEMON_STATUS_MOVE, (u16)sVar10);
    }
    if (sVar7 == 0 || sVar7 == 0x165 || sVar7 == 0x163) {
        goto failed;
    }
    lbl_8047B60C = sVar7;
    wazaSetStatus(PC_PTR(uVar4), 0, 0x26, 0, (int)sVar10);
    ServerStatusFlag = ServerStatusFlag & 0xfffffbff;
    floorTarget =
        fightSeqSelectTarget(uVar2, lbl_8047B60C, uVar6, 0, 1, 1, PC_PTR32_IMPLICIT((void*)0xffffffff));
    fightFloorSetStatus(0, 0, 0x43, 0, floorTarget);
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
done:
    return;
}
