/**
 * @file gs_part_exact_800EE150.c
 * @brief GSpart lookup and rotation-callback registration,
 *        0x800EE150 - 0x800EE3BC.
 *
 * The head of the GSpart range: GSmodelGetPart and its traversal callback
 * fn_800EE20C, GSpartGetJObjIndex, fn_800EE288 and GSpartRegisterRotation.
 * It is linked on its own while GSpartGetTransform (gs_part.c, the only
 * function between this head and gs_part_exact_800EE6B4.c) is still being
 * matched.
 *
 * GSpartFindRotationSlot is a reconstructed inline: GSpartRegisterRotation
 * carries its fingerprint, the helper's "return NULL" materialised in r3
 * (li r3, 0x0) and then tested again by the caller (cmplwi r3, 0x0; beq)
 * after both failure paths.
 */
#include "dolphin/types.h"
#include "hsd/hsd_dobj.h"
#include "hsd/hsd_jobj.h"

#include "game/gs_part.h" /* GSpart (was a private copy) */
#include "game/gs_model_struct.h" /* GSmodel */

extern void *lbl_8047ABA8;
extern u32 lbl_8047ABAC;
extern u32 lbl_8047ABB0;
extern u8 _partFound;
extern u32 _partFindCurIdx;
extern s32 _partFoundIndex;
extern void* _partToFindJObj;

extern void* modelGetRenderJObj(void* model);
extern void HSD_JObjWalkTree(void* root, void (*callback)(void*), u32 flags);
extern void GSlogWrite(const char* format, ...);
extern char lbl_80270F10[];
extern char lbl_80270F44[];

void fn_800EE20C(void* jobj);
GSpart* GSpartCreate(void);
void _partFindIndexCB__FP9_HSD_JObjPPvi(void* jobj);

GSpart* GSmodelGetPart(void* model, s32 index)
{
    s32 traversalIndex = index;
    void* jobj;
    GSpart* part;

    if (((GSmodel*)model)->flags.raw & 0x20000) {
        traversalIndex++;
    }

    lbl_8047ABAC = traversalIndex;
    lbl_8047ABB0 = 0;
    lbl_8047ABA8 = NULL;
    jobj = modelGetRenderJObj(model);
    if (traversalIndex != 0) {
        HSD_JObjWalkTree(jobj, fn_800EE20C, 0);
        if (lbl_8047ABA8 == NULL) {
            return NULL;
        }
    } else {
        lbl_8047ABA8 = jobj;
    }

    part = GSpartCreate();
    if (part == NULL) {
        return NULL;
    }
    part->model = model;
    part->jobj = lbl_8047ABA8;
    part->index = index;
    return part;
}

void fn_800EE20C(void *jobj)
{
    if (lbl_8047ABB0++ == lbl_8047ABAC) {
        lbl_8047ABA8 = jobj;
    }
}

u32 GSpartGetJObjIndex(void* jobj, void* root)
{
    if (jobj == root) {
        return 0;
    }

    _partFound = 0;
    _partToFindJObj = root;
    _partFindCurIdx = 0;
    _partFoundIndex = -1;
    HSD_JObjWalkTree(jobj, _partFindIndexCB__FP9_HSD_JObjPPvi, 0);
    return _partFoundIndex;
}

void fn_800EE288(GSpart* part)
{
    GSmodel* model = part->model;
    s32 count = 4;

    while (count-- != 0) { /* the original tests the first entry four times (it never advances) */
        if ((u32)model->partAnimMix[0].partIndex == part->index) {
            model->partAnimMix[0].type = 0;
            model->partAnimMix[0].partIndex = -1;
            return;
        }
    }
}

/* The rotation-callback view of GSmodel.partAnimMix (+0xE4 on the GameCube, four entries): the same layout as
 * GSmodelPartAnimMix on both builds (two 32-bit words and a pointer), so it is reached through the GSmodel field. */
typedef struct GSpartRotationSlot {
    s32 callback;
    u32 partIndex;
    void* userData;
} GSpartRotationSlot;

static inline GSpartRotationSlot* GSpartFindRotationSlot(GSpart* part)
{
    GSmodel* model = part->model;
    GSpartRotationSlot* slot;
    u32 i;

    slot = (GSpartRotationSlot*)model->partAnimMix;
    for (i = 0; i < 4; i++, slot++) {
        if (slot->partIndex == part->index) {
            GSlogWrite(lbl_80270F10);
            return NULL;
        }
    }

    slot = (GSpartRotationSlot*)model->partAnimMix;
    for (i = 0; i < 4; i++, slot++) {
        if (slot->callback == 0) {
            return slot;
        }
    }
    GSlogWrite(lbl_80270F44);
    return NULL;
}

void GSpartRegisterRotation(GSpart* part, void* userData, void* callback)
{
    GSpartRotationSlot* slot;

    if (callback == NULL) {
        return;
    }

    slot = GSpartFindRotationSlot(part);
    if (slot != NULL) {
        slot->callback = (s32)PC_PTR32(callback);
        slot->partIndex = part->index;
        slot->userData = userData;
    }
}
