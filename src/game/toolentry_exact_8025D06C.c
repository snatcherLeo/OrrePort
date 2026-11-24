/** Exact tool-entry coupon and species-scale helpers, 0x8025D06C - 0x8025D164. */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the tool-entry species list is common_rel data */

typedef struct DISC_STRUCT ToolentrySpeciesList { /* common_rel species list: big-endian view on the PC */
    u16 speciesId[1];
} ToolentrySpeciesList;

u32 couponAddFromResult(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 savedataBattleGetCouponRun(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataBattleGetSavedHero(u8* savedata);
#endif
    extern u8* fn_8006B5A8(void);
    extern void heroAddPokecoupon(u8* savedata, s32 amount);
    s32 amount;

    savedataBattleGetSavedHero(fn_8006B5A8());
    amount = savedataBattleGetCouponRun();
    heroAddPokecoupon(0, amount);
    return 0;
}

f32 heroCountPartyInList(void* ctx)
{
    extern ToolentrySpeciesList* lbl_80478EAC;
    extern f32 lbl_8047E658;
    extern f32 lbl_8047E65C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonBiosGetPokemonDataId(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataGetStatus(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroBiosGetPokemonPtr(void*, u32);
#endif
    u32 i;
    u32 count;
    void* member;
    void* party;
    ToolentrySpeciesList* speciesList;
    u32 offset;
    u16 species;
    u16 entry;
    f32 scale;
    f32 factor;

    count = 0;
    if ((party = ctx) == 0) {
        party = PC_PTR(savedataGetStatus(0, 2));
    }
    for (i = 0; (s32)i < 6; i++) {
        member = heroBiosGetPokemonPtr(party, i & 0xFFFF);
        if (pokemonCheckValid(member) != 0) {
            species = pokemonBiosGetPokemonDataId(member);
            speciesList = lbl_80478EAC;
            offset = 0;
            while (1) {
                entry = speciesList->speciesId[offset];
                if (entry == 0) {
                    break;
                }
                if (species == entry) {
                    count++;
                }
                offset++;
            }
        }
    }
    scale = lbl_8047E658;
    factor = lbl_8047E65C;
    while ((s32)count > 0) {
        scale *= factor;
        count--;
    }
    return scale;
}
