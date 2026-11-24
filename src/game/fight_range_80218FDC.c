/**
 * @file fight_range_80218FDC.c
 * @brief Move-copy script command (copies the target's last
 *        move into the user's empty slot), 0x8021908C - 0x80219270.
 *
 * Standalone source at GC/1.3. The user's 0xD5 and 0xD6 slots are read into
 * their own locals before the two fightPokemonGetPokemonPtr calls.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */
#include "game/constants/pokemon_status.h"
extern u8* lbl_8047B610;
void wsCmdA8(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
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
    extern void pokemonWazaCreate();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    u32 status;
    u32 firstSlot;
    u32 secondSlot;
    u32 firstPokemon;
    u32 target;
    u32 secondPokemon;
    u32 invalid;
    u16 move;
    u32 attacker;
    s8 slot;

    attacker = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    status = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x12, 0));
    move = (u16)pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SKETCH_MOVE, 0);
    wazaSetStatus(PC_PTR(status), 0, 0x27, 0, 0xffff);

    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(attacker), PC_PTR(0x10)) == 1) {
        goto failed;
    }

    if (move == 0x164 || (u16)(move - 0xa5) <= 1 || move == 0xffff ||
        move == 0 || move == 0x165) {
        invalid = 1;
    } else {
        invalid = 0;
    }
    if ((u8)invalid == 1) {
failed:
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        goto done;
    }

    firstSlot = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    secondSlot = pokemonGetStatus(PC_PTR(attacker), 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0);
    firstPokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(firstSlot));
    secondPokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(secondSlot));
    if (pokemonSearchWazaDataId(PC_PTR(firstPokemon), move) >= 0) {
        goto failed;
    }

    slot = (s8)wazaGetStatus(PC_PTR(status), 0, 0x26, 0);
    if (slot < 0) {
        goto failed;
    }

    pokemonWazaCreate(PC_PTR(firstPokemon), slot, move);
    if (fightOutPokemonIsUseHensinBuff(PC_PTR(attacker)) == 1) {
        pokemonWazaCreate(PC_PTR(secondPokemon), slot, move);
    }
    msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, move, 1, 0))));
    lbl_8047B610 += 5;
done:
    return;
}
