/**
 * @file gs_part_exact_800EE6B4.c
 * @brief GSpart material accessors, pool create/free/init and the JObj
 *        index callback, 0x800EE6B4 - 0x800EE928.
 *
 * The tail of the GSpart range. It is linked on its own while
 * GSpartGetTransform (gs_part.c) is still being matched.
 */
#include "dolphin/types.h"
#include "hsd/hsd_dobj.h"
#include "hsd/hsd_jobj.h"

#include "game/gs_part.h" /* GSpart (was a private copy) */
#include "game/gs_material_object.h" /* GSmaterial */

extern GSpart *_partStructList;
extern u32 _partStructCount;
extern u8 _partFound;
extern u32 _partFindCurIdx;
extern s32 _partFoundIndex;
extern void* _partToFindJObj;

extern void* GSmaterialCreate(void);

static inline HSD_DObj* partGetDObj(GSpart* part)
{
    HSD_JObj* jobj = part->jobj;

    if (union_type_dobj(jobj)) {
        return jobj->u.dobj;
    }
    return NULL;
}

/*
 * Walks the DObj list: returns the index-th material and reports the list
 * length through count. The target expands it in GSpartGetMaterial and in
 * GSpartGetMaterialCount (index -1, with the NULL test on &count intact).
 */
static inline HSD_MObj* partFindMaterial(HSD_DObj* dobj, u32 index, u32* count)
{
    u32 i = 0;

    while (dobj != NULL) {
        if (i++ == index) {
            if (count != NULL) {
                *count = index;
            }
            return dobj->mobj;
        }
        dobj = dobj->next;
    }
    if (count != NULL) {
        *count = i;
    }
    return NULL;
}

void* GSpartGetMaterial(GSpart* part, u32 index)
{
    HSD_DObj* dobj = partGetDObj(part);
    HSD_MObj* mobj;
    void* material;

    if (dobj == NULL) {
        return NULL;
    }
    mobj = partFindMaterial(dobj, index, NULL);
    if (mobj == NULL) {
        return NULL;
    }

    material = GSmaterialCreate();
    if (material != NULL) {
        ((GSmaterial*)material)->mobj = mobj;
    }
    return material;
}

u32 GSpartGetMaterialCount(GSpart* part)
{
    HSD_DObj* dobj = partGetDObj(part);
    u32 count;

    if (dobj == NULL) {
        return 0;
    }
    partFindMaterial(dobj, -1, &count);
    return count;
}

u8 fn_800EE7E0(GSpart* part)
{
    u8* jobj = (u8*)part->jobj;
    u8* material;
    u32 value;

    if (!union_type_dobj((HSD_JObj*)jobj)) {
        value = 0;
    } else {
        material = (u8*)((HSD_JObj*)jobj)->u.dobj;
        if (material == NULL) {
            value = 0;
        } else {
            value = PC_PTR32(((HSD_DObj*)material)->pobj); /* any mesh */
        }
    }
    return -value == 0;
}

void GSpartFree(GSpart *part)
{
    part->inUse = 0;
}

static inline GSpart *GSpartFindFree(void)
{
    GSpart *part;
    u32 i;

    part = _partStructList;
    for (i = 0; i < _partStructCount; i++, part++) {
        if (part->inUse == 0) {
            return part;
        }
    }
    return NULL;
}

GSpart *GSpartCreate(void)
{
    GSpart *part;

    part = GSpartFindFree();
    if (part == NULL) {
        return NULL;
    }
    part->inUse = 1;
    return part;
}

extern u16 lbl_8047ABB8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif

void GSpartInit(u32 count)
{
    u32 handle;
    u32 i;

    _partStructCount = count;
    handle = _toolentryAlloc__FUl(count * sizeof(GSpart));
    lbl_8047ABB8 = handle;
    if ((u16)handle != 0) {
        _partStructList = GSmemLock((u16)handle);
        for (i = 0; i < _partStructCount; i++) {
            ((u8*)_partStructList)[i * sizeof(GSpart)] = 0;
        }
    }
}

void _partFindIndexCB__FP9_HSD_JObjPPvi(void* jobj)
{
    if (_partFound == 1) {
        return;
    }
    if (_partToFindJObj == jobj) {
        _partFoundIndex = _partFindCurIdx;
        _partFound = 1;
    }
    _partFindCurIdx++;
}
