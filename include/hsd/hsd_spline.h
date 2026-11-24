#ifndef HSD_SPLINE_H
#define HSD_SPLINE_H

#include "dolphin/types.h"
#include "dolphin/mtx.h"
#include "hsd/hsd_forward.h"

/* The spline arrays' element type: big-endian on the PC (the spline is disc data used in place), f32 on the GameCube. */
#ifdef TARGET_PC
typedef struct DISC_STRUCT HSD_DiscF32 { f32 v; } HSD_DiscF32;
#define HSD_DISC_F32_AT(a, i) ((a)[i].v)
#else
typedef f32 HSD_DiscF32;
#define HSD_DISC_F32_AT(a, i) ((a)[i])
#endif
typedef HSD_DiscF32 HSD_DiscF32Row5[5];

/* HAL sysdolphin spline.h; a joint's path (JOBJ_SPLINE), read from the archive in place */
struct DISC_STRUCT HSD_Spline {
    /* 0x00 */ u8 type;
    /* 0x02 */ s16 numcv;
    /* 0x04 */ f32 tension;
    /* 0x08 */ DISC_PTR(HSD_DiscVec3) cv;
    /* 0x0C */ f32 totalLength;
    /* 0x10 */ DISC_PTR(HSD_DiscF32) segLength;     /* numcv entries */
    /* 0x14 */ DISC_PTR(HSD_DiscF32Row5) segPoly;   /* per segment: arc-length polynomial coefficients */
};

f32 splGetHelmite(f32 fterm, f32 time, f32 p0, f32 p1, f32 d0,
                f32 d1);                                   /* splGetHelmite */
void splGetSplinePoint(Vec3* p, HSD_Spline* spline, f32 u);    /* splGetSplinePoint */
f32 splArcLengthGetParameter(HSD_Spline* spline, f32 u); /* splArcLengthGetParameter */
void splArcLengthPoint(Vec3* p, HSD_Spline* spline, f32 u);

#ifndef TARGET_PC
typedef char HSD_Spline_check_disc_size[sizeof(struct HSD_Spline) == 0x18 ? 1 : -1];
#endif

#endif /* HSD_SPLINE_H */
