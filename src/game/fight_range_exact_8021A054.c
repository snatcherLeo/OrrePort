#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd9D(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
    extern u8 fightOutPokemonUseHensinBuff();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetPokemonJoutai08();
#endif
    extern u32 fightOutPokemonGetPokemonJoutai08();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWritePokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCreate();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
    extern u16 YubihuriNGTbl[];
    u32 attacker;
    u32 pokemon;
    u32 move;
    u16 entry;
    u32 index;
    u8 invalid;
    s8 slot;
    u8 pp;
    u32 flags;

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    pokemon = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    move = pokemonGetStatus(
               fightTargetGetPtrAsNowFightType(0x12, 0), 0, POKEMON_STATUS_LAST_SELECTED_MOVE, 0) &
           0xffff;
    wazaSetStatus(PC_PTR(pokemon), 0, 0x27, 0, 0xffff);

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x10)) == 1) {
        goto advance_alt;
    }

    if (move == 0) {
        invalid = 1;
    } else if (move == 0x165) {
        invalid = 1;
    } else if (move == 0x163) {
        invalid = 1;
    } else {
        index = 0;
        while ((entry = YubihuriNGTbl[index]) != 0xfffe) {
            if (move == entry) {
                break;
            }
            index++;
        }
        invalid = entry != 0xfffe;
    }

    if (invalid != 0 || move == 0 || move == 0xffff || move == 0x165 ||
        move == 0x163) {
advance_alt:
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }

    if (pokemonSearchWazaDataId(fightOutPokemonGetPokemonPtr(PC_PTR(attacker)), move) >=
        0) {
        goto advance_alt;
    }

    slot = (s8)wazaGetStatus(PC_PTR(pokemon), 0, 0x26, 0);
    if (slot < 0) {
        goto advance_alt;
    }

    if (fightOutPokemonIsUseHensinBuff(PC_PTR(attacker)) == 0 &&
        fightOutPokemonUseHensinBuff(attacker) == 0) {
        goto advance_alt;
    }

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x31)) == 0) {
        fightOutPokemonWritePokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x31), 0);
    }

    pokemon = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(attacker));
    pokemonWazaCreate(PC_PTR(pokemon), slot, move);
    pp = (u8)wazaGetStatus(0, move, 2, 0);
    if (pp > 5) {
        pp = 5;
    }
    pokemonSetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE_PP, slot, pp);
    msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, move, 1, 0))));
    flags = fightOutPokemonGetPokemonJoutai08(attacker, 0x31);
    flags |= 1 << slot;
    fightOutPokemonSetPokemonJoutai08(PC_PTR(attacker), PC_PTR(0x31), PC_PTR(flags));
    lbl_8047B610 += 5;
}
#pragma optimize_for_size reset
