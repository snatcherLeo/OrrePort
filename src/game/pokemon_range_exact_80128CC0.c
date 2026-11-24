/**
 * @file pokemon_range_exact_80128CC0.c
 * @brief Exact Pokemon pointer-accessor tail, 0x80128CC0 - 0x80128E38.
 */
#include "dolphin/types.h"

extern u32 now_savedata_ptr;
extern u8 pokecolo_savedata[];

/* NAME-T1 fn_80128CC0 */
void* savedataBiosGetExRibbonPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x1C45C;
}

/* NAME-T1 fn_80128CDC */
void* savedataBiosGetDarkpokemonPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x1B014;
}

void* savedataBiosGetE3E8Ptr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0xE3E8;
}

/* NAME-T3 fn_80128D14 */
void* savedataBiosGetCardEPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x9A1C;
}

/* NAME-T1 fn_80128D30 */
void* savedataBiosGetMemoDataPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x82A8;
}

/* NAME-T1 fn_80128D4C */
void* savedataBiosGetSodateyaPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x8168;
}

/* NAME-T1 fn_80128D68 */
void* savedataBiosGetMailboxPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x7D20;
}

/* NAME-T1 fn_80128D80 */
void* savedataBiosGetFlagLevel2Ptr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x1BE5C;
}

/* NAME-T1 fn_80128D9C */
void* savedataBiosGetFlagLevel1Ptr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x1BDDC;
}

/* NAME-T1 fn_80128DB8 */
void* savedataBiosGetFlagLevel0Ptr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x1BDBC;
}

/* NAME-T1 fn_80128DD4 */
void* savedataBiosGetPcboxPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0xB88;
}

/* NAME-T1 fn_80128DEC */
void* savedataBiosGetHeroPtr(void* ptr) {
    if (ptr == NULL) { return NULL; }
    return (u8*)ptr + 0x70;
}

/* NAME-T1 fn_80128E04 */
u32 savedataBiosGetGamedataPtr(void* ptr) {
    if (ptr != NULL) { return PC_PTR32(ptr); }
    return 0;
}

/* NAME-T1 fn_80128E14 */
void savedataBiosSetNowSavedataPtr(void* ptr) {
    if (ptr == NULL) { return; }
    now_savedata_ptr = PC_PTR32(ptr);
}

/* NAME-T1 fn_80128E24 */
u32 savedataBiosGetNowSavedataPtr(void) {
    return now_savedata_ptr;
}

/* NAME-T1 fn_80128E2C */
void* savedataBiosGetSavedataBufferPtr(void) {
    return (void*)pokecolo_savedata;
}
