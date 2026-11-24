#include "game/fight_action.h"
#include "game/constants/pokemon_status.h"

typedef u32 (*FightOutPokemonVisitor)(void* pokemon, u16 index,
                                      void** selectedPokemon);

extern u32 tenkouDataBiosGetFightInitMsgId(u16 weatherId);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorLoopValidFightOutPokemon(
    u32 side, FightOutPokemonVisitor visitor, void* context, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightFloorGetNowTenkouDataId(void* floor, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightFloorSetStatus(u32 side, u16 index, u32 status,
                              u16 subIndex, u32 value);
#endif
extern u32 fightKoukaDoFightKoukaJoukenAndKouka(void* floor, u16 mode);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void* context, void* buffData, u8 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* pokemon, u32 group, u16 status, u32 index);
#endif

u32 _fightActionFlowTenkouInitSubGetSeqFightOutPokemonPtr__FPvUsPv(
    void* pokemon, u16 index, void** selectedPokemon);

u32 fightActionFlowTenkouInit(void* context)
{
    u8 weatherId;
    void* selectedPokemon;
    u32 messageId;

    fightKoukaDoFightKoukaJoukenAndKouka(0, 1);
    weatherId = fightFloorGetNowTenkouDataId(0, 0);
    selectedPokemon = 0;
    fightFloorLoopValidFightOutPokemon(
        0, _fightActionFlowTenkouInitSubGetSeqFightOutPokemonPtr__FPvUsPv,
        &selectedPokemon, 0);
    if (weatherId != 0) {
        fightFloorSetStatus(0, 0, 0x36, 0, PC_PTR32(selectedPokemon));
        messageId = tenkouDataBiosGetFightInitMsgId(weatherId);
        fightFloorSetStatus(0, 0, 0x50, 0, messageId);
        wsRunScript(context,
                    fightActionBiosGetBuffDataPtr((FightAction*)context), 0);
    }
    return 1;
}

u32 _fightActionFlowTenkouInitSubGetSeqFightOutPokemonPtr__FPvUsPv(
    void* pokemon, u16 index, void** selectedPokemon)
{
    if (pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SEQUENCE_PTR, 0) != 0) {
        if (selectedPokemon != 0) {
            *selectedPokemon = pokemon;
        }
        return 0;
    }
    return 1;
}

u32 fightActionFlowHeijou(void* action)
{
    return 1;
}
