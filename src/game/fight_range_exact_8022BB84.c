/**
 * @file fight_range_exact_8022BB84.c
 * @brief Held-item battle effect dispatcher at 0x8022BB84.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSrand(void);
#endif
extern u8 lbl_80379B61[];
extern u8 lbl_80478D78[8];
extern u8* lbl_8047B610;
extern void* lbl_8047B62C;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetSoubiItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetSoubiItemSoubiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 figthOutPokemonGetSoubiItemBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetUseWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightWazaIsHit();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightOutPokemonCheckFightOut();
#endif
extern u8 fightOutPokemonIsHpMantan();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript();
#endif
extern void fightSeqSetTsuikaKouka(u32, u32);

u32 _wsSoubiItemAfterDamage(u32 attacker, u32 target)
{
    u32 move;
    u32 itemData;
    u32 itemType;
    s32 itemBuff;
    u8 hit;
    u8 moveFlag;
    s16 value11C;
    s16 value11E;
    u32 moveData;
    s32 damage;
    s32 heal;
    u32 result;
    u8* saved;

    move = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    itemData = fightOutPokemonGetSoubiItemDataId(PC_PTR(attacker));
    itemType = fightOutPokemonGetSoubiItemSoubiDataId(PC_PTR(attacker));
    itemBuff = figthOutPokemonGetSoubiItemBuff(PC_PTR(attacker));
    fightOutPokemonGetSoubiItemDataId(PC_PTR(target));
    fightOutPokemonGetSoubiItemSoubiDataId(PC_PTR(target));
    figthOutPokemonGetSoubiItemBuff(PC_PTR(target));

    result = 0;
    if (wazaGetStatus(PC_PTR(move), 0, 0x2D, 0) == 0) {
        return 0;
    }

    moveData = fightOutPokemonGetUseWazaDataId(PC_PTR(attacker));
    hit = fightWazaIsHit(PC_PTR(move));
    moveFlag = wazaGetStatus(0, moveData, 0x12, 0);
    value11C = pokemonGetStatus(PC_PTR(target), 0, 0x11C, 0);
    value11E = pokemonGetStatus(PC_PTR(target), 0, 0x11E, 0);
    damage = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0);

    switch ((u16)itemType) {
    case 0x1E:
        if (hit == 1 && (value11C != 0 || value11E != 0) && moveFlag == 1 &&
            ((s32)(GSrand() & 0xFFFF) % 100) < itemBuff &&
            fightOutPokemonCheckFightOut(PC_PTR(target)) == 1) {
            lbl_80478D78[3] = 8;
            saved = lbl_8047B610;
            fightSeqSetTsuikaKouka(0, 0);
            lbl_8047B610 = saved;
        }
        break;
    case 0x3E:
        if (hit == 1 && damage != 0 && damage != 0xFFFF && attacker != target &&
            fightOutPokemonIsHpMantan(attacker) == 0 &&
            fightOutPokemonCheckFightOut(PC_PTR(attacker)) == 1) {
            fightFloorSetStatus(0, 0, 0x56, 0, (u16)itemData);
            fightFloorSetStatus(0, 0, 0x49, 0, attacker);
            fightFloorSetStatus(0, 0, 0x4B, 0, attacker);
            heal = -(damage / itemBuff);
            if (heal == 0) {
                heal = -1;
            }
            wazaSetStatus(PC_PTR(move), 0, 0x2D, 0, heal);
            pokemonSetStatus(PC_PTR(target), 0, POKEMON_STATUS_SHELL_BELL_DAMAGE, 0, 0);
            wsRunScript(lbl_8047B62C, lbl_80379B61, 0);
            result = 1;
        }
        break;
    }
    return result;
}
