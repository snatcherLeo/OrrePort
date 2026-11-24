/**
 * @file people_item_tail_exact_80144064.c
 * @brief Strict item-data tail, 0x80144064 - 0x801441A8.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the item table is disc data, kept big-endian on the PC */

typedef struct DISC_STRUCT PeopleFieldEntry { /* the item table (lbl_80363CE8, main.dol data, kept big-endian on the PC): big-endian view */
    f32 field_00;
    u8 flags_04;
    s8 field_05;
    s8 field_06;
    s8 field_07;
    u32 field_08;
    u8 field_0C;
    u8 field_0D;
    u8 field_0E;
    u8 field_0F;
    f32 posX;
    f32 posY;
    f32 posZ;
    f32 rotAngle;
    f32 scale;
    u32 modelRef;
} PeopleFieldEntry;

typedef struct DISC_STRUCT ItemDataBios { /* the item table (lbl_80363CE8, main.dol data, kept big-endian on the PC): big-endian view */
    u8 kind;
    u8 important;
    u8 useful;
    u8 field_03;
    u8 itemEffectParam;
    u8 padding_05;
    u16 price;
    u16 coupon;
    u16 itemSoubiDataId;
    u16 fightUseKoukaDataId;
    u16 padding_0E;
    u32 name;
    u32 doc;
    u32 buff;
    u32 fieldUseFunc;
    u32 battleUseFunc;
    s8 useFriend[3];
    u8 padding_27;
} ItemDataBios;

extern u32 lbl_80478BD8;
extern u32 lbl_80478BB0;
extern u16 lbl_803681E8[];
extern PeopleFieldEntry lbl_80363CE8[];

extern void* fightFloorGetNowPtr(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 itemUse2Pokemon(void*, void*, void*, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif

s32 itemDataBiosCheckImportable(u8* p)
{
    ItemDataBios* item = (ItemDataBios*)p;
    if (item == NULL) {
        return 0;
    }
    return item->name != 0;
}

u32 itemDataBiosGetName(u8* p)
{
    ItemDataBios* item = (ItemDataBios*)p;
    if (item == NULL) {
        return 0;
    }
    return item->name;
}

u8* itemDataBiosGetPtr(u16 index)
{
    u16 slot;

    if (index >= lbl_80478BD8) {
        return NULL;
    }
    slot = lbl_803681E8[index];
    if (slot >= lbl_80478BB0) {
        return NULL;
    }
    return (u8*)&lbl_80363CE8[slot];
}

s32 itemUse2PokemonSimulation(
    void* arg0, void* arg1, void* arg2, void* arg3, void* arg4)
{
    void* savedFightFloor;
    void* fightFloor;
    u16 memHandle;
    s32 result;

    fightFloor = fightFloorGetNowPtr();
    if (fightFloor == NULL) {
        return 0;
    }

    memHandle = _toolentryAlloc__FUl(0xA4E8);
    savedFightFloor = GSmemLock(memHandle);
    memcpy(savedFightFloor, fightFloor, 0xA4E8);
#ifdef TARGET_PC
    /* the reference playthrough P10's crash (port-gaps 68): the only caller, the trainer AI's item check
     * (fight_trainer_ai_irekae_parent.c), passes (work, 0, status, item id, 0) through a prototype
     * with u32 integers, while this definition reads void* parameters. itemUse2Pokemon takes the last
     * two as a u16 item id and a u8 move slot. On x64 the fifth argument goes on the stack with only
     * its low 32 bits written, so its upper half was leftover junk (0x00007FF9_00000000) and the
     * pointer check stopped the game. Take the low bits, as the GameCube's registers do; the
     * second argument (the caller's 0) likewise. */
    result = itemUse2Pokemon(arg0, (void*)(uintptr_t)(u32)(uintptr_t)arg1, arg2,
                         (u16)(uintptr_t)arg3, (u8)(uintptr_t)arg4);
#else
    result = itemUse2Pokemon(arg0, arg1, arg2, PC_PTR32_IMPLICIT(arg3), PC_PTR32_IMPLICIT(arg4));
#endif
    memcpy(fightFloor, savedFightFloor, 0xA4E8);
    fn_800E24B0(memHandle);
    GSmemFree(memHandle);
    return result;
}
