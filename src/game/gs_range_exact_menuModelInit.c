/**
 * @file gs_range_exact_menuModelInit.c
 * @brief menuModelInit, 0x8010A5BC - 0x8010A88C.
 *
 * Function-boundary carve of gs_range_80109C88.c. Text only; shared strings,
 * constants, and counters remain extern-owned by the range data objects.
 */
#include "dolphin/types.h"

typedef struct MenuModel {
    u8 pad[0x2C];
    s32 w;
    s32 h;
    PTR32_FIELD(void) texture; /* GameCube layout on the PC too (32-bit slots): the PDA reads them at raw +0x34 / +0x38 / +0x3C.. */
    PTR32_FIELD(void) camera;
    PTR32_FIELD(void) lights[3];
} MenuModel;

/* 0x8010A5BC | 0x2D0 */
/* Built with the unit flag -opt nopeephole (configure.py); no local pragma. */
s32 menuModelInit(MenuModel* obj, s32 w, s32 h)
{
    typedef struct Vec3 {
        f32 x, y, z;
    } Vec3;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void memset(void* ptr, int value, u32 size);
#endif
    extern s32 lbl_8047AD40;
    extern u8 lbl_8047AD44;
    extern const u8 lbl_80271F38[];
    extern char lbl_8035B448[];
    extern f32 lbl_8047CE74;
    extern void GSlogWrite(const char* fmt, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GStextureCreate(u16 w, u16 h, u32 format, u32 a, u32 b);
#endif
    extern void GStextureSetFilter(void* tex, u32 min, u32 mag, u32 unk);
    extern void* GScameraCreate(void);
    extern void GScameraSetPosition(void* camera, Vec3* pos);
    extern void GScameraSetRotation(void* camera, Vec3* rot);
    extern void* GSlightCreate(void);
    extern void GSlightSetType(void* light, u32 type);
    extern void GSlightSetColor(void* light, Vec3* color);
    extern void GSlightSetPosition(void* light, Vec3* pos);
    extern void GSlightSetTarget(void* light, Vec3* target);
    extern void GSlightSetActive(void* light, u32 active);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void set__5GSvecFfff(Vec3* out, f32 x, f32 y, f32 z);
#endif

    const u8* tbl = lbl_80271F38;
    Vec3 pos;
    Vec3 rot;
    Vec3 target;
    Vec3 color1;
    Vec3 color0;
    s32 i;
    void* light;

    pos = *(Vec3*)(tbl + 0xc);
    rot = *(Vec3*)(tbl + 0x18);
    target = *(Vec3*)(tbl + 0x24);
    color1 = *(Vec3*)(tbl + 0x30);
    color0 = *(Vec3*)(tbl + 0x3c);

    if (obj == NULL) {
        return 0;
    }

    memset(obj, 0, 0x48);

    if (lbl_8047AD40 >= 4) {
        GSlogWrite((const char*)(tbl + 0x70), lbl_8035B448);
        return 0;
    }

    obj->w = w;
    obj->h = h;
    if (w < 0x100) {
        w = 0x100;
    }
    if (w > 0x280) {
        w = 0x280;
    }
    if (h < 0x100) {
        h = 0x100;
    }
    if (h > 0x1e0) {
        h = 0x1e0;
    }

    P32_SET(obj->texture, GStextureCreate((u16)w, (u16)h, 0x45, 0, 0));
    if (P32(obj->texture) == NULL) {
        GSlogWrite((const char*)(tbl + 0x8c), lbl_8035B448);
        return 0;
    }

    GStextureSetFilter(P32(obj->texture), 2, 2, 0);

    P32_SET(obj->camera, GScameraCreate());
    if (P32(obj->camera) == NULL) {
        GSlogWrite((const char*)(tbl + 0xac), lbl_8035B448);
        return 0;
    }

    GScameraSetPosition(P32(obj->camera), &pos);
    GScameraSetRotation(P32(obj->camera), &rot);

    for (i = 0; i < 3; i++) {
        light = GSlightCreate();
        if (light != NULL) {
            switch (i) {
            case 0:
                GSlightSetType(light, 0);
                GSlightSetColor(light, &color0);
                break;
            case 1:
                GSlightSetType(light, 2);
                GSlightSetColor(light, &color1);
                GSlightSetPosition(light, &pos);
                GSlightSetTarget(light, &target);
                break;
            case 2:
                set__5GSvecFfff(&pos, lbl_8047CE74, lbl_8047CE74, lbl_8047CE74);
                GSlightSetType(light, 2);
                GSlightSetColor(light, &color1);
                GSlightSetPosition(light, &pos);
                GSlightSetTarget(light, &target);
                break;
            }
            GSlightSetActive(light, 0);
            P32_SET(obj->lights[i], light);
        }
    }

    if (lbl_8047AD40 == 0) {
        lbl_8047AD44 = 0;
    }
    lbl_8047AD40++;
    return 1;
}
