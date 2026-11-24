/*
 * Vertex-array sizes for Aurora's GXSetArray (which uploads `size` bytes of each array to the GPU; the game never
 * knows sizes, the GameCube hardware just reads what the indices ask for). At load, walk every mesh (PObj) of the
 * loaded joint trees, parse its display list (big-endian GX commands, as on the disc), find the highest index used
 * for each indexed attribute, and record array -> (max index + 1) * stride. platform_gaps.c's GXSetArray forwarder
 * looks the array up here. This is the rule a real port needs (tests/pc_hsd/README.md, item 8).
 */
#include "hsd/hsd_jobj.h"
#include "hsd/hsd_dobj.h"
#include "hsd/hsd_pobj.h"

#include <stdio.h>

#define MAX_ARRAYS 8192
typedef struct { const u8* data; u32 size; } ArraySize;
static ArraySize sArrays[MAX_ARRAYS];
static int sCount;

static void note(const u8* data, u32 size) {
    int i;
    for (i = 0; i < sCount; i++) {
        if (sArrays[i].data == data) {
            if (size > sArrays[i].size) {
                sArrays[i].size = size;
            }
            return;
        }
    }
    if (sCount < MAX_ARRAYS) {
        sArrays[sCount].data = data;
        sArrays[sCount].size = size;
        sCount++;
    }
}

u32 pc_hsd_array_size(const void* data) {
    int i;
    for (i = 0; i < sCount; i++) {
        if (sArrays[i].data == data) {
            return sArrays[i].size;
        }
    }
    return 0;
}

/* bytes one vertex's attribute takes in the display list when sent directly */
static u32 direct_size(const HSD_VtxDescList* d) {
    static const u8 comp_bytes[] = {1, 1, 2, 2, 4};          /* U8, S8, U16, S16, F32 */
    static const u8 color_bytes[] = {2, 3, 4, 2, 3, 4};      /* RGB565, RGB8, RGBX8, RGBA4, RGBA6, RGBA8 */
    u32 n;
    if (d->attr <= 8) {
        return 1; /* position / normal and texture matrix indices */
    }
    if (d->attr == 11 || d->attr == 12) {
        return d->comp_type < 6 ? color_bytes[d->comp_type] : 4;
    }
    if (d->attr == 9) {
        n = d->comp_cnt == 0 ? 2 : 3; /* POS_XY / POS_XYZ */
    } else if (d->attr == 10 || d->attr == 25) {
        n = d->comp_cnt == 0 ? 3 : 9; /* NRM_XYZ / NRM_NBT */
    } else {
        n = d->comp_cnt == 0 ? 1 : 2; /* TEX_S / TEX_ST */
    }
    return n * (d->comp_type < 5 ? comp_bytes[d->comp_type] : 4);
}

static void scan_pobj(HSD_PObj* pobj) {
    const HSD_VtxDescList* desc = pobj->verts;
    const u8* p = pobj->display;
    const u8* end = p + (pobj->n_display << 5);
    u32 max_index[32] = {0};
    int used[32] = {0};
    const HSD_VtxDescList* d;
    int n_attr = 0, a;

    if (desc == NULL || p == NULL) {
        return;
    }
    for (d = desc; d->attr != 0xFF && n_attr < 32; d++) {
        n_attr++;
    }
    while (p < end) {
        u8 cmd = *p++;
        if ((cmd & 0xF8) >= 0x80 && (cmd & 0xF8) <= 0xB8) {
            u32 count = ((u32)p[0] << 8) | p[1];
            u32 v;
            p += 2;
            for (v = 0; v < count && p < end; v++) {
                for (a = 0, d = desc; a < n_attr; a++, d++) {
                    u32 index;
                    switch (d->attr_type) {
                    case 1: /* GX_DIRECT */
                        p += direct_size(d);
                        continue;
                    case 2: /* GX_INDEX8 */
                        index = *p++;
                        break;
                    case 3: /* GX_INDEX16 */
                        index = ((u32)p[0] << 8) | p[1];
                        p += 2;
                        break;
                    default:
                        continue;
                    }
                    used[a] = 1;
                    if (index > max_index[a]) {
                        max_index[a] = index;
                    }
                }
            }
        } else if (cmd == 0x00) {
            continue; /* no-op padding */
        } else {
            break; /* anything else ends what HSD's lists contain */
        }
    }
    for (a = 0, d = desc; a < n_attr; a++, d++) {
        if (used[a] && DP(d->vertex) != NULL) {
            /* the whole last element: up to `stride` bytes (NBT normals use 3 vectors per index) */
            note((const u8*)DP(d->vertex), (max_index[a] + 1) * d->stride);
        }
    }
}

static void scan_jobj(HSD_JObj* jobj) {
    for (; jobj != NULL; jobj = jobj->next) {
        if (!(jobj->flags & (JOBJ_SPLINE | JOBJ_PTCL))) {
            HSD_DObj* dobj;
            for (dobj = jobj->u.dobj; dobj != NULL; dobj = dobj->next) {
                HSD_PObj* pobj;
                for (pobj = dobj->pobj; pobj != NULL; pobj = pobj->next) {
                    scan_pobj(pobj);
                }
            }
        }
        if (!(jobj->flags & JOBJ_INSTANCE)) {
            scan_jobj(jobj->child);
        }
    }
}

void pc_hsd_scan_arrays(HSD_JObj* root) {
    scan_jobj(root);
}

int pc_hsd_array_count(void) {
    return sCount;
}
