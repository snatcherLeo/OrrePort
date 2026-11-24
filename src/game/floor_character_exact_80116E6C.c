/**
 * @file floor_character_exact_80116E6C.c
 * @brief Strict floor-character BIOS accessors, 0x80116E6C - 0x80116F68.
 *
 * The record's first byte is a bitfield (the getters below read its bits:
 * visible 7, load-init 6, talk-wall-through 4, move type 0-2). The
 * visibility setter stores the bitfield member, which MWCC inserts with
 * rlwimi itself; the unit is built with -opt nopeephole (see configure.py),
 * so a mask-and-or written by hand would not be folded into one.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* room character records are big-endian on the PC */

/* PC: a big-endian view, so GCC lays the bit-fields out from the top bit as MWCC does (visible = bit 7, what the
 * getters read). As a native struct GCC put `visible` in bit 0: scripts that showed a room character set bit 0 and the
 * loader still read 0 (Outskirt Stand's truck stayed invisible in the arrival scene, 2026-10-17). */
typedef struct DISC_STRUCT FloorCharacterFlags {
    u8 visible : 1;
    u8 loadInit : 1;
    u8 : 2;
    u8 talkWallThrough : 1;
    u8 moveType : 3;
} FloorCharacterFlags;

void floorCharacterBiosSetVisibility(FloorCharacterFlags* ptr, u8 val)
{
    if (ptr == NULL) {
        return;
    }
    ptr->visible = val;
}

void floorCharacterBiosSetPos(u8* dst, f32* src)
{
    if (dst == NULL) {
        return;
    }
    if (src == NULL) {
        return;
    }
    DISC_STORE_F32(&dst[0x18], src[0]);
    DISC_STORE_F32(&dst[0x1C], src[1]);
    DISC_STORE_F32(&dst[0x20], src[2]);
}

u32 floorCharacterBiosGetTalkSctID(void* ptr)
{
    return ptr != NULL ? DISC_U32((u8*)ptr + 0x14) : 0;
}

u32 floorCharacterBiosGetMoveSctID(void* ptr)
{
    return ptr != NULL ? DISC_U32((u8*)ptr + 0x10) : 0;
}

u32 floorCharacterBiosGetNameID(void* ptr)
{
    return ptr != NULL ? DISC_U16((u8*)ptr + 0x8) : 0;
}

u32 floorCharacterBiosGetTalkWallThrough(u8* ptr)
{
    return ptr != NULL ? (u32)((ptr[0] >> 4) & 1) : 0;
}

u32 floorCharacterBiosGetTalkEndType(u8* ptr)
{
    return ptr != NULL ? (u32)((ptr[1] >> 4) & 3) : 0;
}

u32 floorCharacterBiosGetTalkStartType(u8* ptr)
{
    return ptr != NULL ? (u32)((ptr[1] >> 6) & 3) : 0;
}

u32 floorCharacterBiosGetMoveType(u8* ptr)
{
    return ptr != NULL ? (u32)(ptr[0] & 7) : 0;
}
