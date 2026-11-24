/**
 * @file gs_model_parse_exact_800EAFE4.c
 * @brief GSmodel parse: _modelParseLoadEnvelopeMatrix, 0x800EAFE4 - 0x800EB268.
 *
 * Loads the envelope (skinned) matrices of a PObj, after HSD pobj.c's
 * SetupEnvelopeModelMtx. XD's parse.o has the same function at the same
 * size (GXXE01.map: 0x800FC624, 0x284) with the same register allocation.
 * Built like the parse TU (GC/1.3.2, strings in .rodata/.sdata2). The unit
 * owns the TU's .rodata string pool (0x80270EB8 - 0x80270EE8: "envelope",
 * "envelope->jobj", "jp->envelopemtx"); written as literals they give
 * retail's base-plus-offset addressing. The short .sdata2 strings and the
 * floats stay extern, as in gs_model_parse.c.
 *
 * HSD_JObjMtxIsDirty returns its condition as one expression. The
 * "result = FALSE; if (...) result = TRUE;" form gives the same
 * instructions, but its result local is a frontend temporary numbered
 * before the string-pool base, so the base is coloured last (r25) instead
 * of first (r31).
 */
#include "dolphin/types.h"
#include "hsd/hsd_pobj.h"

/* The real HSD_JObj (a padded GameCube-only view was here). This unit keeps its own HSD_JObjMtxIsDirty /
 * HSD_JObjSetupMatrix inlines (below), so the header's are renamed. */
#define HSD_JObjMtxIsDirty HSD_JObjMtxIsDirty_header
#define HSD_JObjSetupMatrix HSD_JObjSetupMatrix_header
#include "hsd/hsd_jobj.h"
#undef HSD_JObjMtxIsDirty
#undef HSD_JObjSetupMatrix

typedef void (*GSModelPObjDisp)(HSD_PObj* pobj, f32 vmtx[3][4], f32 pmtx[3][4],
                               f32 smtx[3][4], void* arg);

#include "hsd/hsd_dobj.h"

extern const u8 lbl_8047CC00[7];
extern const u8 lbl_8047CC08[5];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* condition);
#endif

static inline BOOL HSD_JObjMtxIsDirty(HSD_JObj* jobj)
{
    if (jobj == NULL) {
        __assert((const char*)lbl_8047CC00, 0x25d, (const char*)lbl_8047CC08);
    }
    return !(jobj->flags & 0x00800000) && (jobj->flags & 0x40);
}

extern void HSD_JObjSetupMatrixSub(HSD_JObj*);
static inline void HSD_JObjSetupMatrix(HSD_JObj* jobj)
{
    if (jobj == NULL || !HSD_JObjMtxIsDirty(jobj)) {
        return;
    }
    HSD_JObjSetupMatrixSub(jobj);
}

extern const f32 lbl_8047CC18;
extern const f32 lbl_8047CC1C;
extern void HSD_PObjClearMtxMark(u32 first, u32 second);
extern HSD_JObj* HSD_JObjGetCurrent(void);
#ifndef TARGET_PC /* PC: Aurora's PSMTXConcat (dolphin/mtx.h) takes Mtx; the calls below hand it flat f32 pointers */
extern void PSMTXConcat(f32* left, f32* right, f32* out);
#else
#undef PSMTXConcat
#define PSMTXConcat(a, b, c) C_MTXConcat((const f32(*)[4])(a), (const f32(*)[4])(b), (f32(*)[4])(c))
#endif
extern void fn_800E0628(void* dst, void* src);

void _modelParseLoadEnvelopeMatrix__FP9_HSD_PObjP5GSmtxP5GSmtxP5GSmtx(
    HSD_PObj* pobj, f32* vmtx, f32* pmtx, f32* mtx)
{
    extern f32* _HSD_mkEnvelopeModelNodeMtx(HSD_JObj* jobj, f32* matrix);
    extern void HSD_MtxScaledAdd(f32* src, f32* add, f32* out, f32 scale);
    extern const char lbl_8047CC10[8];
    extern const char lbl_8047CC20[8];
    HSD_JObj* jobj;
    HSD_SList* list;
    s32 idx;
    f32* right;
    f32 nmtx[12];

    jobj = HSD_JObjGetCurrent();
    HSD_PObjClearMtxMark(0, 2);
    right = _HSD_mkEnvelopeModelNodeMtx(jobj, nmtx);

    for (idx = 0, list = pobj->u.envelope_list; idx < 10 && list != NULL;
         idx++, list = list->next) {
        f32 m[12];
        f32 tmp[12];
        f32* mtxp;
        HSD_Envelope* envelope = list->data;

        if (envelope == NULL) {
            __assert(lbl_8047CC10, 0x65, "envelope");
        }
        if (envelope->weight >= lbl_8047CC18) {
            HSD_JObjSetupMatrix(envelope->jobj);
            if (right != NULL) {
                PSMTXConcat((f32*)envelope->jobj->mtx,
                            (f32*)envelope->jobj->envelopemtx, m);
                mtxp = m;
            } else {
                mtxp = (f32*)envelope->jobj->mtx;
            }
        } else {
            m[0] = m[1] = m[2] = m[3] = m[4] = m[5] = m[6] = m[7] =
                m[8] = m[9] = m[10] = m[11] = lbl_8047CC1C;
            while (envelope != NULL) {
                HSD_JObj* jp;

                if (envelope->jobj == NULL) {
                    __assert(lbl_8047CC10, 0x7E, "envelope->jobj");
                }
                jp = envelope->jobj;
                HSD_JObjSetupMatrix(jp);
                if ((f32*)jp->mtx == NULL) {
                    __assert(lbl_8047CC10, 0x81, lbl_8047CC20);
                }
                if ((f32*)jp->envelopemtx == NULL) {
                    __assert(lbl_8047CC10, 0x82, "jp->envelopemtx");
                }
                PSMTXConcat((f32*)jp->mtx, (f32*)jp->envelopemtx, tmp);
                HSD_MtxScaledAdd(tmp, m, m, envelope->weight);
                envelope = envelope->next;
            }
            mtxp = m;
        }
        if (right != NULL) {
            PSMTXConcat(mtxp, right, m);
        }
        PSMTXConcat(vmtx, mtxp, tmp);
        fn_800E0628(mtx + idx * 12, tmp);
    }
}
