/**
 * @file floor_character_exact_80117070.c
 * @brief Floor-character BIOS lookups, 0x80117070 - 0x8011711C.
 *
 * floorCharacterBiosGetPeopleInfoPtr and floorCharacterGetInfo (a floor character's
 * info by group and index, on the current floor), the last two functions of
 * the floorCharacterBios unit, carved at their function boundaries;
 * field_camera starts at 0x8011711C. Text-only: neither reads data.
 *
 * Built with the unit's flags, which include -opt nopeephole (see
 * configure.py): floorCharacterGetInfo copies both arguments and then compares the
 * first (mr r29,r3; mr r30,r4; cmplwi r29,0) where the peephole pass would
 * fold the copy and the compare into mr., and
 * floorCharacterBiosGetPeopleInfoPtr stores the link register before its
 * compare.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* room character records are big-endian on the PC */

extern void* peopleInfoBiosGetPtrFromIndex(u16 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* std092_getCurrentRoomID(void);
#endif
extern void* floorDataBiosGetPtr(void* floor);
extern u8* floorDataBiosGetGroupID(void* floorData);
extern u32 floorDataBiosGetCharInfo(void* floorData, u32 index);

void* floorCharacterBiosGetPeopleInfoPtr(u8* ptr)
{
    if (ptr != NULL) {
        return peopleInfoBiosGetPtrFromIndex(DISC_U16(ptr + 0x6));
    }
    return NULL;
}

u32 floorCharacterGetInfo(u8* group, u32 index)
{
    void* floorData;

    if (group == NULL) {
        return 0;
    }
    floorData = floorDataBiosGetPtr(PC_PTR(std092_getCurrentRoomID()));
    if (group != floorDataBiosGetGroupID(floorData)) {
        return 0;
    }
    return floorDataBiosGetCharInfo(floorData, index);
}
