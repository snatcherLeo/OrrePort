#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

extern u8* lbl_8047B610;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTargetGetPtrAsNowFightType(u32 slotType, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus(void* object, u32 id, u32 selector, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 figthOutPokemonGetLevel(void* context);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSetStatus(
    void* ptr, u16 dataId, u16 status, u32 index, u32 value);
#endif

void wsCmd9F(void)
{
    void* context = fightTargetGetPtrAsNowFightType(0x11, 0);
    void* move = (void*)PC_PTR(pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0));

    wazaSetStatus(move, 0, 0x2d, 0, figthOutPokemonGetLevel(context));
    lbl_8047B610++;
}
