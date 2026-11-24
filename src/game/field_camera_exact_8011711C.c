#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT: room data is big-endian on the PC */

typedef struct DISC_STRUCT FieldCameraList { /* room data: big-endian view on the PC */
    DISC_PTR(u32) entries;
    u32 count;
} FieldCameraList;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* std092_getCurrentRoomID(void);
#endif
extern void* floorDataBiosGetPtr(u32 key);
extern void* floorDataBiosGetCharInfo(void* floorData, u32 index);
extern FieldCameraList* floorDataBiosGetFieldCameraListPtr(void* floor);
extern u32 lbl_8047AD68;
extern u32 lbl_8047AD6C;
extern u8 lbl_8047AD70;
extern u8 lbl_8047AD71;
extern f32 lbl_8047AD74;
extern f32 lbl_8047AD78;
extern f32 lbl_8047AD7C;
extern f32 lbl_8047CFD0;

void* floorCharacterBiosGetByIndex(u32 index)
{
    return floorDataBiosGetCharInfo(
        floorDataBiosGetPtr(PC_PTR32(std092_getCurrentRoomID())), index);
}

void fn_80117154(void)
{
    lbl_8047AD68 = 0;
    lbl_8047AD6C = 0;
}

void fieldCameraLoadList(void* floor) /* floor.c passes the room's floor data */
{
    FieldCameraList* list;

    list = floorDataBiosGetFieldCameraListPtr(floor); /* the caller's argument, still in r3 / r4 (assembly: not written before the call) */
    lbl_8047AD68 = 0;
    lbl_8047AD6C = 0;
    lbl_8047AD70 = 0;
    lbl_8047AD71 = 1;
    lbl_8047AD74 = lbl_8047CFD0;
    lbl_8047AD78 = lbl_8047CFD0;
    lbl_8047AD7C = lbl_8047CFD0;
    if (list != 0) {
        lbl_8047AD68 = DISC_U32(DP(list->entries));
        lbl_8047AD6C = list->count;
    }
}
