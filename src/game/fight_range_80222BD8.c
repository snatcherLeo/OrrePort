/**
 * @file fight_range_80222BD8.c
 * @brief Exact pure-C fight-sequence helper, 0x80222BD8 - 0x80222C44.
 *
 * Prefix before the exact 0x80222C44 - 0x802230BC island.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;
extern u8 lbl_80478D78[8];
extern u32 ServerStatusFlag;

/*
 * wsCmd25 (0x80222BD8)
 * Sequence opcode: touch the current attacker's field 0xd9, reset the move
 * loop, clear the two sequence flag bytes and flag bits 0x40 / 0x4000, and
 * advance the sequence PC by one byte.
 */
void wsCmd25(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    extern void fightWazaInitLoop();
    u32 target;

    target = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));

    fightWazaInitLoop(pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));
    lbl_80478D78[3] = 0;
    ServerStatusFlag &= ~0x40;
    lbl_80478D78[6] = 0;
    ServerStatusFlag &= ~0x4000;
    lbl_8047B610++;
}
