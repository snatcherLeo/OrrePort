/**
 * @file fight_range_80238060.c
 * @brief fightTrainerAiGetWazaPpRatio, 0x80238060 - 0x802381C4: whether a move slot's PP is
 *        at or below the trainer's threshold percentage (trainer status
 *        0x37) of its max PP.
 *
 * The body is built from the fight AI accessors (compare fn_802381C4, which
 * is the PP getter written out). As inlines, aiGetTrainer's result is
 * numbered before the PP value and the slot's u8 mask is taken in place,
 * which is retail's allocation.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightPokemonGetPokemonPtr(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonWazaGetMaxPP();
#endif

static inline u16 aiGetTrainer(u32 ctx)
{
    return fightTrainerGetStatus(0, fightTrainerGetStatus(PC_PTR(ctx), 0, 0x43, 0) & 0xFFFF, 2, 0);
}

static inline void* aiGetFightPokemon(u32 ctx, void* resolved)
{
    fightFloorGetStatus(0, 0, 0x14, 0);
    aiGetTrainer(ctx);
    fightPokemonGetPokemonPtr(resolved);
    return fightPokemonGetPokemonPtr(resolved);
}

static inline u8 aiGetPP(u32 ctx, void* resolved, u32 slot)
{
    return (u8)pokemonGetStatus(aiGetFightPokemon(ctx, resolved), 0, POKEMON_STATUS_MOVE_PP, slot & 0xFF);
}

static inline u8 aiGetMaxPP(u32 ctx, void* resolved, u32 slot)
{
    return pokemonWazaGetMaxPP(aiGetFightPokemon(ctx, resolved), slot & 0xFF);
}

u32 fightTrainerAiGetWazaPpRatio(u32 ctx, u32 pokemon, u32 slot)
{
    u16 trainer;
    void* resolved;
    s32 pp;
    s32 maxPP;

    resolved = PC_PTR(pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
    trainer = aiGetTrainer(ctx);
    pp = aiGetPP(ctx, resolved, slot);
    maxPP = aiGetMaxPP(ctx, resolved, slot);
    return pp * 100 / maxPP <= (u8)fightTrainerGetStatus(0, trainer, 0x37, 0);
}
