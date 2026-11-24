#ifndef GAME_WORLD_FLOOR_ARCHIVE_H
#define GAME_WORLD_FLOOR_ARCHIVE_H

/* The map / camera / object readers allocate the file with room for its HSD_Archive record in front
 * (floorRead*PreFunc), and the post functions parse the archive into that room. The record is 0x44 bytes on the
 * GameCube (0x60 reserved) and 0x70 on the PC (8-byte pointers), so the PC reserves 0x80 (the file stays 32-byte
 * aligned). */
#ifdef TARGET_PC
#define FLOOR_ARCHIVE_HEADER 0x80
#else
#define FLOOR_ARCHIVE_HEADER 0x60
#endif

/* The floor readers address their strings (the archive's public name "scene_data" and their error messages) as
 * offsets from lbl_80272200 (0x80272200, 0x37 bytes), as the compiler did: on the GameCube the .rodata strings sit in
 * one block. On the PC every symbol is its own array, so the offsets are mapped back to the symbol that really holds
 * them (offsets used: 0xAC, 0x1CC, 0x200, 0x298, 0x2D0). */
#ifdef TARGET_PC
extern const char lbl_802722AC[]; /* 0x802722AC "scene_data" */
extern const char lbl_80272394[]; /* 0x80272394, 0x94 bytes of messages */
extern const char lbl_80272460[]; /* 0x80272460, 0x88 bytes of messages */
static inline const char* floorReadString(const void* base, unsigned offset)
{
    unsigned address = 0x80272200u + offset;

    if (address >= 0x80272460u && address < 0x802724E8u) {
        return lbl_80272460 + (address - 0x80272460u);
    }
    if (address >= 0x80272394u && address < 0x80272428u) {
        return lbl_80272394 + (address - 0x80272394u);
    }
    if (address >= 0x802722ACu && address < 0x802722B7u) {
        return lbl_802722AC + (address - 0x802722ACu);
    }
    return (const char*)base + offset; /* inside lbl_80272200 itself */
}
#define FLOOR_READ_STRING(base, offset) floorReadString((base), (offset))
#else
#define FLOOR_READ_STRING(base, offset) ((base) + (offset))
#endif

#endif /* GAME_WORLD_FLOOR_ARCHIVE_H */
