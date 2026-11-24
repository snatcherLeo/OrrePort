/**
 * @file gs_texture_exact_800EFD14.c
 * @brief fn_800EFD14 / GStextureLoad, 0x800EFD14 - 0x800EFFC0.
 *
 * Function-boundary carve of the GStexture candidate
 * (gs_texture_candidate_800EF5FC.c): no jump table (GStextureGetGXformat's
 * switch expands to a compare tree), no pooled constant, no data. The GX
 * setup is the repeated-expansion helper shared with GStextureCreate
 * (game/gs_texture_init_gx.h). GC/1.3 -O4,p like the TU, no pragmas.
 */
#include "game/gs_texture_init_gx.h"

/* Claim tex for the memory handle unless it is already in use. */
void fn_800EFD14(GStextureHandle* tex, u16 handle)
{
    if (tex == NULL || tex->inUse != 0) {
        return;
    }
    tex->inUse = 1;
    tex->memHandle = handle;
}

#ifdef TARGET_PC
/* The GX objects of every GStexture (GS_TEX_OBJ in gs_texture.h): Aurora's are larger than the space a texture
 * record reserves for them. The record's gxTexObj bytes hold a marker and a slot number; a slot whose owner is no
 * longer this record (the memory was reused) is taken again. Slots are handed out round robin, so the oldest is
 * reused first once all are taken (likely far more than the game keeps alive: GStextures are menu / font / effect
 * images, not the HSD model textures). */
#define PC_GS_TEXTURE_SLOTS 4096
#define PC_GS_TEXTURE_MARK 0x4F52544Fu /* "ORTO" */
static struct PcGsTextureObjs pc_gs_texture_pool[PC_GS_TEXTURE_SLOTS];
static GStextureHandle* pc_gs_texture_owner[PC_GS_TEXTURE_SLOTS];
static u32 pc_gs_texture_next;

struct PcGsTextureObjs* pc_gs_texture_objs(GStextureHandle* tex)
{
    u32* ref = (u32*)tex->gxTexObj;
    u32 slot;

    if (ref[0] == PC_GS_TEXTURE_MARK && ref[1] < PC_GS_TEXTURE_SLOTS && pc_gs_texture_owner[ref[1]] == tex) {
        return &pc_gs_texture_pool[ref[1]];
    }
    slot = pc_gs_texture_next++ % PC_GS_TEXTURE_SLOTS;
    pc_gs_texture_owner[slot] = tex;
    ref[0] = PC_GS_TEXTURE_MARK;
    ref[1] = slot;
    return &pc_gs_texture_pool[slot];
}

/* A texture file starts with this record in big-endian order: convert its fields once, before relocating. */
static void pc_gs_texture_header_to_native(GStextureHandle* tex)
{
    u32* words;
    s32 i;

    tex->width = __builtin_bswap16(tex->width);
    tex->height = __builtin_bswap16(tex->height);
    words = (u32*)((u8*)tex + 0x08); /* format, tlutFormat, wrapS, wrapT, minFilter, magFilter, mipFilter */
    for (i = 0; i < 7; i++) {
        words[i] = __builtin_bswap32(words[i]);
    }
    tex->memHandle = __builtin_bswap16(tex->memHandle);
    for (i = 0; i < 8; i++) {
        tex->mipData[i].raw = __builtin_bswap32(tex->mipData[i].raw); /* offsets from the record */
    }
    tex->tlutData.raw = __builtin_bswap32(tex->tlutData.raw);
    tex->totalSize = __builtin_bswap32(tex->totalSize);
    tex->refCount = __builtin_bswap16(tex->refCount);
    tex->unk52 = __builtin_bswap16(tex->unk52);
    ((u32*)tex->gxTexObj)[0] = 0; /* no pool slot yet */
}
#endif

/* Relocate a texture image loaded from a file and build its GX objects. */
GStextureHandle* GStextureLoad(GStextureHandle* tex)
{
    s32 i;

#ifdef TARGET_PC
    /* A copy of a texture that was already loaded (the face-portrait loader _menuFaceLoad copies a loaded texture, turns
     * mip 0 back into an offset and loads the copy) has a native header and still carries the PC marker from its
     * first load; converting it again garbled the offset (2026-10-17, Willie's battle: 0x1001A5620). Disc
     * texture files never contain the marker. */
    if (((u32*)tex->gxTexObj)[0] != PC_GS_TEXTURE_MARK) {
        pc_gs_texture_header_to_native(tex);
    }
    for (i = 0; i < (s32)tex->mipLevels; i++) {
        P32_SET(tex->mipData[i], (u8*)tex + tex->mipData[i].raw);
    }
    for (i = tex->mipLevels; i < 8; i++) {
        P32_SET(tex->mipData[i], NULL);
    }
    if (tex->tlutData.raw != 0) {
        P32_SET(tex->tlutData, (u8*)tex + tex->tlutData.raw);
    }
#else
    for (i = 0; i < (s32)tex->mipLevels; i++) {
        tex->mipData[i] = (u8*)tex + PC_PTR32(tex->mipData[i]);
    }
    for (i = tex->mipLevels; i < 8; i++) {
        tex->mipData[i] = NULL;
    }
    if (tex->tlutData != NULL) {
        tex->tlutData = (u8*)tex + PC_PTR32(tex->tlutData);
    }
#endif

    textureInitGXObjects(tex);
    tex->memHandle = 0;
    return tex;
}
