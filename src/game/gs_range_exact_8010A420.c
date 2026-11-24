/**
 * @file gs_range_exact_8010A420.c
 * @brief menuModelTerm, 0x8010A420 - 0x8010A5BC.
 *
 * Function-boundary carve of gs_range_80109C88.c. Text only; state and
 * resource globals remain extern-owned by the range data objects.
 */
#include "dolphin/types.h"

/* 0x8010A420 | 0x19C */
/* Built with the unit flag -opt nopeephole (configure.py); no local pragma. */
s32 menuModelTerm(void* obj)
{
    typedef struct Obj {
        u8 state;
        u8 flag1;
        u8 pad2[0x12];
        u8 flag14;
        u8 pad15[0xF];
        u32 handle24;
        u32 taskHandle;
        u8 pad2C[8];
        PTR32_FIELD(void) texture; /* 32-bit slots: the record keeps its GameCube layout on the PC (MenuModel) */
        PTR32_FIELD(void) camera;
        PTR32_FIELD(void) lights[3];
    } Obj;
    extern s32 lbl_8047AD40;
    extern u8 lbl_8047AD44;
    extern char lbl_80271F80[];
    extern char lbl_8035B458[];
    extern void GSlogWrite(const char* fmt, ...);
    extern u8 GSthreadIsRunning(u32 task);
    extern void GSthreadClose(u32 task);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
    extern void wazaSequenceOwnerFree(u32 handle);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelFree(void* a);
#endif
    extern void GSlightFree(void* a);
    extern void GScameraFree(void* camera);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GStextureFree(void* a);
#endif
    extern int wazaSequenceSysRelease(void);

    s32 i;
    u32 temp;
    Obj* o = (Obj*)obj;

    if (o == NULL) {
        return 0;
    }
    if (lbl_8047AD40 <= 0) {
        GSlogWrite(lbl_80271F80, lbl_8035B458);
        return 0;
    }

    o->flag1 = 0;
    for (;;) {
        if (o->taskHandle == 0) {
            break;
        }
        temp = GSthreadIsRunning(o->taskHandle);
        if (temp == 0) {
            GSthreadClose(o->taskHandle);
            break;
        }
        _threadSwitch();
    }

    if (o != NULL) {
        o->flag1 = 0;
        if (o->flag14) {
            if (o->handle24 != 0) {
                wazaSequenceOwnerFree(o->handle24);
                o->handle24 = 0;
            }
        } else if (o->handle24 != 0) {
            GSmodelFree((void*)PC_PTR(o->handle24));
            o->handle24 = 0;
        }
        o->state = 0;
    }

    for (i = 0; i < 3; i++) {
        if (P32(o->lights[i]) != NULL) {
            GSlightFree(P32(o->lights[i]));
            P32_SET(o->lights[i], NULL);
        }
    }

    if (P32(o->camera) != NULL) {
        GScameraFree(P32(o->camera));
        P32_SET(o->camera, NULL);
    }

    if (P32(o->texture) != NULL) {
        GStextureFree(P32(o->texture));
        P32_SET(o->texture, NULL);
    }
    lbl_8047AD40 = lbl_8047AD40 - 1;
    if (lbl_8047AD40 == 0 && lbl_8047AD44) {
        wazaSequenceSysRelease();
        lbl_8047AD44 = 0;
    }

    return 1;
}
