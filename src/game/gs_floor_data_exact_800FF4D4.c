/**
 * @file gs_floor_data_exact_800FF4D4.c
 * @brief Exact floor resource registration and state accessors.
 */
#include "dolphin/types.h"
#include "game/gs_floor.h"
#include "pc/disc_types.h" /* DISC_U32 */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void memcpy(void* dst, const void* src, u32 n);
#endif

extern u32 lbl_8047ACC4;
extern u32 lbl_8047ACC8;
extern u32 lbl_8047ACE0;
extern GSFloorResHandler lbl_80404918[];

void fn_800FF4D4(void* data, u8 typeId)
{
    GSFloorResHandler* handler;

    if (lbl_8047ACE0 < 0x18) {
        handler = &lbl_80404918[lbl_8047ACE0];
        handler->typeId = typeId;
#ifdef TARGET_PC
        /* PC: three native function pointers (24 bytes), not the GameCube's 12; every caller passes void*[3] */
        memcpy(&handler->loadFunc, data, 3 * sizeof(void*));
#else
        memcpy(&handler->loadFunc, data, 0xC);
#endif
        lbl_8047ACE0++;
    }
}

u32 fn_800FF52C(void)
{
    return lbl_8047ACC4 != 0;
}

u32 fn_800FF540(void)
{
    return lbl_8047ACC4;
}

u8 fn_800FF548(void)
{
#ifdef TARGET_PC /* GSFloorContext has native pointers on the PC: read the field, not the GameCube offset */
    return ((GSFloorContext*)PC_PTR(lbl_8047ACC8))->doFadeOut;
#else
    return *(u8*)((u8*)PC_PTR(lbl_8047ACC8) + 0xB);
#endif
}

u8 fn_800FF554(void)
{
#ifdef TARGET_PC /* GSFloorContext has native pointers on the PC: read the field, not the GameCube offset */
    return ((GSFloorContext*)PC_PTR(lbl_8047ACC8))->doFadeIn;
#else
    return *(u8*)((u8*)PC_PTR(lbl_8047ACC8) + 0xA);
#endif
}

u32 floorGetCurrentContext(void)
{
#ifdef TARGET_PC /* GSFloorContext has native pointers on the PC: read the field, not the GameCube offset */
    return ((GSFloorContext*)PC_PTR(lbl_8047ACC8))->floorId;
#else
    return *(u32*)((u8*)PC_PTR(lbl_8047ACC8) + 0x4);
#endif
}

u32 std092_getCurrentRoomID(void)
{
    void* floorData;

    floorData = ((GSFloorContext*)PC_PTR(lbl_8047ACC8))->floorDataEntry;
    if (floorData != NULL) {
        return DISC_U32((u8*)floorData + 0xC); /* the floor record's room id: common_rel data, big-endian on the PC */
    }
    return 0;
}
