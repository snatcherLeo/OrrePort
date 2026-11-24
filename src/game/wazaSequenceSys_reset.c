#include "game/battle/battle_waza_types.h"

typedef struct WazaPoolHeader {
    PTR32_FIELD(void) entries; /* 32-bit slots: lbl_80467CC0 keeps the GameCube layout (0x14 bytes) on the PC */
    u16 count;
    u8 active;
    u8 pad07;
    PTR32_FIELD(void) resourceA;
    PTR32_FIELD(void) resourceB;
    u16 handle;
} WazaPoolHeader;

u8 lbl_80467C80[0x40];
u8 lbl_80467CC0[0x14];
u8 lbl_80467CD4[0x24];

/* RULE-EXCEPTION(title-path): unreferenced BSS-order helper, dead-stripped
 * at link time; the same pattern is recorded for GSmsg in RULE_EXCEPTIONS.md. */
static void wazaResetBssLayout(void) {
    lbl_80467C80[0] = 0;
    lbl_80467CC0[0] = 0;
    lbl_80467CD4[0] = 0;
}

/* wazaSequenceSysInitWithoutParticles: wazaSequenceSysInit without the shared effect archive (0x6F7); see
 * wazaSequenceSys_tail.c. */
/* NAME-T3 fn_801DADC0 */
void wazaSequenceSysInitWithoutParticles(void* context) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorDataBiosGetCurrentPtr(void);
#endif
    extern u32 floorDataBiosGetGroupID(void*);
    extern u32 floorDataBiosGetShadowReciveNum(void*);
    extern void* floorDataBiosGetShadowReciveID(void*, u32);
    extern void* floorDataBiosGetShadowLightID(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32, u32);
#endif
    extern void wazaSequenceSysClearWork(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 _toolentryAlloc__FUl(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16);
#endif
    u32 groupId;
    void* floor;
    void* resource;
    s32 receiverCount;
    u16 handle;
    s32 count = (s32)PC_PTR32(context);
    s32 i;

    memset(lbl_80467CC0, 0, sizeof(WazaPoolHeader));
    memset(lbl_80467CD4, 0, 0x20);
    floor = floorDataBiosGetCurrentPtr();
    groupId = floorDataBiosGetGroupID(floor);
    receiverCount = floorDataBiosGetShadowReciveNum(floor);
    _modelShadowCount = 0;
    for (i = 0; i < receiverCount; i++) {
        resource = GSresGetResource(groupId, PC_PTR32(floorDataBiosGetShadowReciveID(floor, i)));
        if (resource != NULL) {
            PTR32_AT_SET_T(void, lbl_80467C80, _modelShadowCount++ * 4, resource); /* 32-bit slots, as GSmodelSetShadowSurface copies them */
        }
    }
    resource = floorDataBiosGetShadowLightID(floor);
    if (resource != NULL) {
        _modelShadowLight = (s32)PC_PTR32(GSresGetResource(groupId, PC_PTR32(resource)));
    }
    if (count != 0) {
        wazaSequenceSysClearWork();
        receiverCount = count * 0x8C;
        handle = _toolentryAlloc__FUl(receiverCount);
        if (handle != 0) {
            ((WazaPoolHeader*)lbl_80467CC0)->handle = handle;
            ((WazaPoolHeader*)lbl_80467CC0)->count = count;
            P32_SET(((WazaPoolHeader*)lbl_80467CC0)->entries, GSmemLock(handle));
            memset(P32(((WazaPoolHeader*)lbl_80467CC0)->entries), 0, receiverCount);
            battleCameraWazaInit();
            P32_SET(((WazaPoolHeader*)lbl_80467CC0)->resourceA, NULL);
            P32_SET(((WazaPoolHeader*)lbl_80467CC0)->resourceB, NULL);
            ((WazaPoolHeader*)lbl_80467CC0)->active = 0;
        }
    }
}

void wazaSequenceSysInit(s32 count) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorDataBiosGetCurrentPtr(void);
#endif
    extern u32 floorDataBiosGetGroupID(void*);
    extern u32 floorDataBiosGetShadowReciveNum(void*);
    extern void* floorDataBiosGetShadowReciveID(void*, u32);
    extern void* floorDataBiosGetShadowLightID(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32, u32);
#endif
    extern void wazaSequenceSysClearWork(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 _toolentryAlloc__FUl(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16);
#endif
    u32 groupId;
    void* floor;
    void* resource;
    s32 receiverCount;
    u16 handle;
    s32 i;

    memset(lbl_80467CC0, 0, sizeof(WazaPoolHeader));
    memset(lbl_80467CD4, 0, 0x20);
    floor = floorDataBiosGetCurrentPtr();
    groupId = floorDataBiosGetGroupID(floor);
    receiverCount = floorDataBiosGetShadowReciveNum(floor);
    _modelShadowCount = 0;
    for (i = 0; i < receiverCount; i++) {
        resource = GSresGetResource(groupId, PC_PTR32(floorDataBiosGetShadowReciveID(floor, i)));
        if (resource != NULL) {
            PTR32_AT_SET_T(void, lbl_80467C80, _modelShadowCount++ * 4, resource); /* 32-bit slots, as GSmodelSetShadowSurface copies them */
        }
    }
    resource = floorDataBiosGetShadowLightID(floor);
    if (resource != NULL) {
        _modelShadowLight = (s32)PC_PTR32(GSresGetResource(groupId, PC_PTR32(resource)));
    }
    if (count != 0) {
        wazaSequenceSysClearWork();
        receiverCount = count * 0x8C;
        handle = _toolentryAlloc__FUl(receiverCount);
        if (handle != 0) {
            ((WazaPoolHeader*)lbl_80467CC0)->handle = handle;
            ((WazaPoolHeader*)lbl_80467CC0)->count = count;
            P32_SET(((WazaPoolHeader*)lbl_80467CC0)->entries, GSmemLock(handle));
            memset(P32(((WazaPoolHeader*)lbl_80467CC0)->entries), 0, receiverCount);
            battleCameraWazaInit();
            wazaSequenceLoadGroupAndWait(0x6F7, 0);
            P32_SET(((WazaPoolHeader*)lbl_80467CC0)->resourceA, GSresGetResource(0x6F7, 0x11EF2400));
            P32_SET(((WazaPoolHeader*)lbl_80467CC0)->resourceB, GSresGetResource(0x6F7, 0x11EE2400));
            ((WazaPoolHeader*)lbl_80467CC0)->active = 0;
        }
    }
}
